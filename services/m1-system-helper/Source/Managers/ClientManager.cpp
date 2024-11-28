#include "ClientManager.h"

namespace Mach1 {

ClientManager::ClientManager(std::shared_ptr<EventSystem> events)
    : eventSystem(std::move(events)) {}

juce::Result ClientManager::addClient(const M1OrientationClientConnection& client) {
    const juce::ScopedLock lock(mutex);
    
    auto it = std::find_if(clients.begin(), clients.end(),
        [&client](const auto& existing) { return existing.port == client.port; });
        
    if (it != clients.end()) {
        it->time = juce::Time::currentTimeMillis();
        return juce::Result::ok();
    }

    clients.push_back(client);
    
    // Update type-specific collections
    if (client.type == ClientType::Monitor) {
        monitors.push_back(client);
        DBG("[ClientManager] Added monitor client on port: " + std::to_string(client.port));
    } else if (client.type == ClientType::Player) {
        players.push_back(client);
        DBG("[ClientManager] Added player client on port: " + std::to_string(client.port));
    }
    
    eventSystem->publish("ClientAdded", client.port);
    DBG("[ClientManager] Client added: " + clientTypeToString(client.type) + 
        ", port: " + std::to_string(client.port) + 
        ", isActive? " + (client.active ? "true" : "false"));
    
    // Always call activateClients() after adding a new client
    activateClients();
    
    return juce::Result::ok();
}

void ClientManager::cleanupInactiveClients() {
    const juce::ScopedLock lock(mutex);
    auto currentTime = juce::Time::currentTimeMillis();
    
    auto isInactive = [currentTime](const auto& client) {
        DBG("[ClientManager] Client removed on port: " + std::to_string(client.port));
        return (currentTime - client.time) > CLIENT_TIMEOUT_MS;
    };
    
    // Remove from type-specific collections
    monitors.erase(std::remove_if(monitors.begin(), monitors.end(), isInactive), monitors.end());
    players.erase(std::remove_if(players.begin(), players.end(), isInactive), players.end());
    
    // Remove from main collection
    for (auto it = clients.begin(); it != clients.end();) {
        if (isInactive(*it)) {
            eventSystem->publish("ClientRemoved", it->port);
            it = clients.erase(it);
        } else {
            ++it;
        }
    }
}

void ClientManager::activateClients() {
    const juce::ScopedLock lock(mutex);
    
    // Activate first monitor and deactivate others
    for (size_t i = 0; i < monitors.size(); ++i) {
        juce::OSCSender sender;
        if (sender.connect("127.0.0.1", monitors[i].port)) {
            juce::OSCMessage msg("/m1-activate-client");
            msg.addInt32(i == 0 ? 1 : 0);  // First monitor is active
            sender.send(msg);
            
            // Update active state in our records
            monitors[i].active = (i == 0);
            
            // Also update the corresponding client in the main clients vector
            auto it = std::find_if(clients.begin(), clients.end(),
                [port = monitors[i].port](const auto& client) {
                    return client.port == port;
                });
            if (it != clients.end()) {
                it->active = (i == 0);
            }
            
            DBG("[ClientManager] " + std::string(i == 0 ? "Activated" : "Deactivated") + 
                " monitor on port: " + std::to_string(monitors[i].port));
        }
    }

    // Activate first player and deactivate others
    // Also send monitor count to players
    for (size_t i = 0; i < players.size(); ++i) {
        juce::OSCSender sender;
        if (sender.connect("127.0.0.1", players[i].port)) {
            juce::OSCMessage msg("/m1-activate-client");
            msg.addInt32(i == 0 ? 1 : 0);  // First player is active
            
            // Add monitor count for players
            if (!monitors.empty()) {
                msg.addInt32(static_cast<int>(monitors.size()));
            }
            
            sender.send(msg);
            
            // Update active state in our records
            players[i].active = (i == 0);
            
            DBG("[ClientManager] " + 
                std::to_string(players[i].active) +
                " player on port: " + std::to_string(players[i].port) +
                " (monitor count: " + std::to_string(monitors.size()) + ")");
        }
    }
    
    // Notify event system of activation changes
    eventSystem->publish("ClientsActivationChanged", 0);
}

void ClientManager::removeClient(int port) {
    const juce::ScopedLock lock(mutex);
    
    // First, find if this was an active monitor before removing it
    auto monitorIt = std::find_if(monitors.begin(), monitors.end(),
        [port](const auto& client) { return client.port == port; });
    bool wasActiveMonitor = (monitorIt != monitors.end() && monitorIt->active);
    size_t removedIndex = std::distance(monitors.begin(), monitorIt);

    auto removeFromVector = [port](auto& vec) {
        vec.erase(
            std::remove_if(vec.begin(), vec.end(),
                [port](const auto& client) { return client.port == port; }),
            vec.end()
        );
    };
    
    // Remove from type-specific collections
    removeFromVector(monitors);
    removeFromVector(players);
    
    // Remove from main collection and notify
    auto it = std::find_if(clients.begin(), clients.end(),
        [port](const auto& client) { return client.port == port; });
        
    if (it != clients.end()) {
        eventSystem->publish("ClientRemoved", port);
        clients.erase(it);
    }

    // If we removed an active monitor and there are still monitors left,
    // activate the previous monitor in the list (or the last one if we removed the first)
    if (wasActiveMonitor && !monitors.empty()) {
        size_t newActiveIndex = (removedIndex > 0) ? removedIndex - 1 : monitors.size() - 1;
        if (newActiveIndex < monitors.size()) {
            rotateMonitorToActive(monitors[newActiveIndex].port);
        }
    }
}

bool ClientManager::updateClientTime(int port) {
    const juce::ScopedLock lock(mutex);
    bool found = false;
    
    // Update in main clients vector
    for (auto& client : clients) {
        if (client.port == port) {
            client.time = juce::Time::currentTimeMillis();
            found = true;
        }
    }
    
    // Update in type-specific vectors
    for (auto& monitor : monitors) {
        if (monitor.port == port) {
            monitor.time = juce::Time::currentTimeMillis();
        }
    }
    
    for (auto& player : players) {
        if (player.port == port) {
            player.time = juce::Time::currentTimeMillis();
        }
    }
    
    return found;
}

bool ClientManager::sendToAllClients(const juce::OSCMessage& msg) {
    const juce::ScopedLock lock(mutex);
    bool success = true;
    
    for (const auto& client : clients) {
        juce::OSCSender sender;
        if (sender.connect("127.0.0.1", client.port)) {
            if (!sender.send(msg)) {
                DBG("Failed to send message to client on port " + juce::String(client.port));
                success = false;
            }
        } else {
            DBG("Failed to connect to client on port " + juce::String(client.port));
            success = false;
        }
    }
    
    return success;
}

bool ClientManager::sendToClientsOfType(const juce::OSCMessage& msg, ClientType type) {
    const juce::ScopedLock lock(mutex);
    bool success = true;
    
    const auto& targetClients = (type == ClientType::Monitor) ? monitors : 
                               (type == ClientType::Player) ? players : 
                               std::vector<M1OrientationClientConnection>();
    
    for (const auto& client : targetClients) {
        juce::OSCSender sender;
        if (sender.connect("127.0.0.1", client.port)) {
            if (!sender.send(msg)) {
                DBG("Failed to send message to " + clientTypeToString(type) + 
                    " client on port " + juce::String(client.port));
                success = false;
            }
        } else {
            DBG("Failed to connect to " + clientTypeToString(type) + 
                " client on port " + juce::String(client.port));
            success = false;
        }
    }
    
    return success;
}

std::vector<M1OrientationClientConnection> ClientManager::getClientsByType(ClientType type) const {
    const juce::ScopedLock lock(mutex);
    switch (type) {
        case ClientType::Monitor: return monitors;
        case ClientType::Player: return players;
        default: return {};
    }
}

const std::vector<M1OrientationClientConnection>& ClientManager::getAllClients() const {
    return clients;
}

bool ClientManager::hasActiveClientOfType(int port, const juce::String& type) const {
    const juce::ScopedLock lock(mutex);
    
    const auto& targetClients = (type == "monitor") ? monitors :
                               (type == "player") ? players :
                               std::vector<M1OrientationClientConnection>();
                               
    auto it = std::find_if(targetClients.begin(), targetClients.end(),
        [port](const auto& client) {
            return client.port == port &&
                   (juce::Time::currentTimeMillis() - client.time) < CLIENT_TIMEOUT_MS;
        });
        
    return it != targetClients.end();
}

bool ClientManager::rotateMonitorToActive(int port) {
    const juce::ScopedLock lock(mutex);
    
    // Find the monitor with the specified port
    auto pivot = std::find_if(monitors.begin(), monitors.end(),
        [port](const auto& client) {
            return client.port == port && client.type == ClientType::Monitor;
        });
        
    if (pivot != monitors.end()) {
        // Rotate the found monitor to the first position
        std::rotate(monitors.begin(), pivot, pivot + 1);
        
        // Also update the main clients vector to maintain consistency
        auto mainPivot = std::find_if(clients.begin(), clients.end(),
            [port](const auto& client) {
                return client.port == port && client.type == ClientType::Monitor;
            });
            
        if (mainPivot != clients.end()) {
            std::rotate(
                std::find_if(clients.begin(), clients.end(), 
                    [](const auto& client) { return client.type == ClientType::Monitor; }),
                mainPivot,
                mainPivot + 1
            );
        }
        
        // Make sure to call activateClients to update the active states
        activateClients();
        
        DBG("[Monitor] Successfully rotated monitor port: " + std::to_string(port) + " to active position");
        return true;
    }
    
    DBG("[Monitor] Failed to find monitor with port: " + std::to_string(port));
    return false;
}

size_t ClientManager::getClientCount() const {
    const juce::ScopedLock lock(mutex);
    return clients.size();
}

} // namespace Mach1
