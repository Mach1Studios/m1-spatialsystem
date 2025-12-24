// Network/OSCHandler.cpp
#include "OSCHandler.h"

namespace Mach1 {

OSCHandler::OSCHandler(ClientManager* clientManager, PluginManager* pluginManager, ServiceManager* serviceManager, PannerTrackingManager* pannerTrackingManager)
    : clientManager(clientManager)
    , pluginManager(pluginManager)
    , serviceManager(serviceManager)
    , pannerTrackingManager(pannerTrackingManager)
{
    setupMessageHandlers();
    startTimer(20);
}

OSCHandler::~OSCHandler() {
    stopTimer();
    stopListening();
}

bool OSCHandler::startListening(int port) {
    stopListening();
    
    if (receiver.connect(port)) {
        receiver.addListener(this);
        return true;
    }
    return false;
}

void OSCHandler::stopListening() {
    receiver.disconnect();
    receiver.removeListener(this);
}

void OSCHandler::setupMessageHandlers() {
    messageHandlers = {
        // OrientationManager signals
        {"/m1-clientRequestsServer", [this](const auto& m) { handleClientRequestsServer(m); }}, // used for m1_orientation_client comms
        {"/m1-clientExists", [this](const auto& m) { handleOMClientPulse(m); }}, // used for an OM_client of any client to signal

        // Client signals
        {"/m1-addClient", [this](const auto& m) { handleAddClient(m); }}, // used for clients only
        {"/m1-removeClient", [this](const auto& m) { handleRemoveClient(m); }}, // used for clients only
        {"/m1-status", [this](const auto& m) { handleClientPulse(m); }}, // used to signal a pulse from any client

        // Plugin signals
        {"/m1-register-plugin", [this](const auto& m) { handleRegisterPlugin(m); }}, // used for plugins only
        {"/m1-status-plugin", [this](const auto& m) { handlePluginPulse(m); }},

        // General signals
        {"/setPlayerYPR", [this](const auto& m) { handleSetPlayerYPR(m); }},
        {"/setMonitoringMode", [this](const auto& m) { handleSetMonitoringMode(m); }},
        {"/setMasterYPR", [this](const auto& m) { handleSetMasterYPR(m); }},
        {"/panner-settings", [this](const auto& m) { handlePannerSettings(m); }},
        {"/setChannelConfigReq", [this](const auto& m) { handleSetChannelConfigRequest(m); }},
        {"/setMonitorActiveReq", [this](const auto& m) { handleSetMonitorActiveRequest(m); }},
        {"/setPlayerFrameRate", [this](const auto& m) { handleSetPlayerFrameRate(m); }},
        {"/setPlayerPosition", [this](const auto& m) { handleSetPlayerPosition(m); }},
        {"/setPlayerIsPlaying", [this](const auto& m) { handleSetPlayerIsPlaying(m); }}
    };
}

void OSCHandler::oscMessageReceived(const juce::OSCMessage& message) {
    auto address = message.getAddressPattern().toString();
    
    // Handle other messages
    if (auto it = messageHandlers.find(address); it != messageHandlers.end()) {
        it->second(message);
    }
}

void OSCHandler::handleAddClient(const juce::OSCMessage& message) {
    if (message.size() >= 2) {
        M1OrientationClientConnection client;
        client.port = message[0].getInt32();
        client.type = message[1].getString() == "monitor" ? ClientType::Monitor : 
                     message[1].getString() == "player" ? ClientType::Player : 
                     ClientType::Unknown;
        client.time = juce::Time::currentTimeMillis();
        
        // Add client and get result
        auto result = clientManager->addClient(client);
        
        // Send connection confirmation
        juce::OSCSender sender;
        if (sender.connect("127.0.0.1", client.port)) {
            juce::OSCMessage response("/connectedToServer");
            response.addInt32(clientManager->getClientCount() - 1); // Send ID for multiple clients
            
            if (!sender.send(response)) {
                DBG("[OSCHandler] Failed to send connection confirmation to port: " + 
                    std::to_string(client.port));
            } else {
                DBG("[OSCHandler] Sent connection confirmation to port: " + 
                    std::to_string(client.port));
            }
        }
    }
}

void OSCHandler::handleSetMasterYPR(const juce::OSCMessage& message) {
    if (message.size() >= 3) {
        
        masterYaw = message[0].getFloat32();
        masterPitch = message[1].getFloat32();
        masterRoll = message[2].getFloat32();
        
        if (prevMasterYaw != masterYaw || prevMasterPitch != masterPitch || prevMasterRoll != masterRoll || prevMasterMode != masterMode)
        {
            pluginManager->sendMonitorSettings(masterMode, masterYaw, masterPitch, masterRoll);
            
            prevMasterYaw = masterYaw;
            prevMasterPitch = masterPitch;
            prevMasterRoll = masterRoll;
            prevMasterMode = masterMode;
            
            DBG("[Monitor] YPR updated: Y=" + std::to_string(masterYaw) +
                " P=" + std::to_string(masterPitch) +
                " R=" + std::to_string(masterRoll));
        }
    } else {
        DBG("[OSCHandler] Invalid YPR message size: " + std::to_string(message.size()));
    }
}

void OSCHandler::handleRemoveClient(const juce::OSCMessage& message) {
    if (message.size() >= 1) {
        int port = message[0].getInt32();
        clientManager->removeClient(port);
        
        // Send updated client list to all remaining clients
        juce::OSCMessage updateMsg("/connectedClientsUpdate");
        updateMsg.addInt32(clientManager->getClientCount());
        clientManager->sendToAllClients(updateMsg);
        
        DBG("[OSCHandler] Removed client on port: " + std::to_string(port) + 
            ", remaining clients: " + std::to_string(clientManager->getClientCount()));
    }
}

void OSCHandler::handleClientPulse(const juce::OSCMessage& message) {
    if (message.size() >= 1) {
        int port = message[0].getInt32();
        bool clientExists = clientManager->updateClientTime(port);
        
        juce::OSCSender sender;
        if (sender.connect("127.0.0.1", port)) {
            if (clientExists) {
                // Client exists, send normal response
                juce::OSCMessage response("/m1-response");
                if (!sender.send(response)) {
                    DBG("[OSCHandler] Failed to send status response to port: " + 
                        std::to_string(port));
                }
            } else {
                // Client not found, request re-registration
                juce::OSCMessage reconnectReq("/m1-reconnect-req");
                if (!sender.send(reconnectReq)) {
                    DBG("[OSCHandler] Failed to send reconnect request to port: " + 
                        std::to_string(port));
                } else {
                    DBG("[OSCHandler] Requesting re-registration from client on port: " + 
                        std::to_string(port));
                }
            }
        }
    }
}

void OSCHandler::handleSetPlayerYPR(const juce::OSCMessage& message) {
    if (message.size() >= 3) {
        juce::OSCMessage forwardMsg("/YPR-Offset");
        forwardMsg.addFloat32(message[0].getFloat32()); // yaw
        forwardMsg.addFloat32(message[1].getFloat32()); // pitch
        //forwardMsg.addFloat32(message[2].getFloat32()); // roll
        
        clientManager->sendToClientsOfType(forwardMsg, ClientType::Monitor);
    }
}

void OSCHandler::handleSetMonitoringMode(const juce::OSCMessage& message) {
    if (message.size() >= 1) {
        masterMode = message[0].getInt32();
        
        if (masterMode != prevMasterMode) {
            pluginManager->sendMonitorSettings(masterMode, masterYaw, masterPitch, masterRoll);
            prevMasterMode = masterMode;
        }
    }
}

void OSCHandler::handleRegisterPlugin(const juce::OSCMessage& message) {
    if (message.size() >= 1) {
        M1RegisteredPlugin plugin;
        plugin.port = message[0].getInt32();
        plugin.time = juce::Time::currentTimeMillis();
        pluginManager->registerPlugin(plugin);
        pluginManager->sendMonitorSettings(masterMode, masterYaw, masterPitch, masterRoll);
        
        // Also register with the panner tracking manager for unified tracking
        if (pannerTrackingManager) {
            pannerTrackingManager->registerOSCPanner(plugin);
            DBG("[OSCHandler] Registered panner plugin on port " + juce::String(plugin.port) + " with tracking manager");
        }
    }
}

void OSCHandler::handlePannerSettings(const juce::OSCMessage& message) {
    if (message.size() >= 2) {
        int port = message[0].getInt32();
        int state = message[1].getInt32();
        
        if (state == -1) {
            // Handle disconnect
            pluginManager->removePlugin(port);
            
            // Forward disconnect message to players
            juce::OSCMessage forwardMsg("/panner-settings");
            forwardMsg.addInt32(port);
            forwardMsg.addInt32(-1);  // Disconnect state
            
            clientManager->sendToClientsOfType(forwardMsg, ClientType::Player);
            DBG("[OSCHandler] Relayed panner disconnect for port: " + std::to_string(port));
            return;
        } else {
            // Update plugin time
            pluginManager->updatePluginTime(port);
        }
        
        // Handle regular panner settings (state is 0, 1, or 2)
        if (message.size() >= 10) {
            // Update plugin settings first
            pluginManager->updatePluginSettings(port, message);
            
            // Forward to players with all original parameters
            juce::OSCMessage forwardMsg("/panner-settings");
            
            // Add all parameters from original message
            for (int i = 0; i < message.size(); ++i) {
                if (message[i].isInt32()) {
                    forwardMsg.addInt32(message[i].getInt32());
                } 
                else if (message[i].isFloat32()) {
                    forwardMsg.addFloat32(message[i].getFloat32());
                } 
                else if (message[i].isString()) {
                    forwardMsg.addString(message[i].getString());
                } 
                else if (message[i].isColour()) {
                    forwardMsg.addColour(message[i].getColour());
                }
            }
            
            clientManager->sendToClientsOfType(forwardMsg, ClientType::Player);
            
            // Debug output
            if (message.size() >= 9) {
                DBG("[OSCHandler] Panner settings - Port: " + std::to_string(port) + 
                    ", State: " + std::to_string(state) + 
                    ", Input Mode: " + std::to_string(message[4].getInt32()) + 
                    ", Azimuth: " + std::to_string(message[5].getFloat32()) + 
                    ", Elevation: " + std::to_string(message[6].getFloat32()) + 
                    ", Diverge: " + std::to_string(message[7].getFloat32()) + 
                    ", Gain: " + std::to_string(message[8].getFloat32()));
            }
        } else {
            DBG("[OSCHandler] Invalid panner settings message size: " + 
                std::to_string(message.size()));
        }
    } else {
        DBG("[OSCHandler] Invalid panner settings message: insufficient parameters");
    }
}

void OSCHandler::handleClientRequestsServer(const juce::OSCMessage& message) {
    serviceManager->setClientRequestsServer(true);
}

void OSCHandler::handleOMClientPulse(const juce::OSCMessage& message) {
    timeWhenHelperLastSeenAClient = juce::Time::currentTimeMillis();
}

void OSCHandler::handlePluginPulse(const juce::OSCMessage& message) {
    if (message.size() >= 1) {
        int port = message[0].getInt32();
        
        // Check if plugin exists and is active
        if (pluginManager->hasActivePlugin(port)) // exists
        {
            pluginManager->updatePluginTime(port);
        }
    }
}

void OSCHandler::handleSetChannelConfigRequest(const juce::OSCMessage& message) {
    if (message.size() >= 1) {
        int channelCountForConfig = message[0].getInt32();
        if (lastSystemChannelCount != channelCountForConfig) {
            DBG("[Client] Config request for: " + std::to_string(channelCountForConfig) + " channels");
            
            // Forward the configuration change to all clients
            juce::OSCMessage forwardMsg("/m1-channel-config");
            forwardMsg.addInt32(channelCountForConfig);
            pluginManager->sendToAllPlugins(forwardMsg);
            lastSystemChannelCount = channelCountForConfig;
        }
    }
}

void OSCHandler::handleSetMonitorActiveRequest(const juce::OSCMessage& message) {
    if (message.size() >= 1) {
        int monitorPort = message[0].getInt32();
        DBG("[Monitor] Port: " + std::to_string(monitorPort) + " is requesting first index");
        
        // Request the ClientManager to rotate the specified monitor to active position
        if (clientManager->rotateMonitorToActive(monitorPort)) {
            clientManager->activateClients();
        } else {
            // Handle the case where the rotation fails
            DBG("[Monitor] Failed to rotate monitor to active position");
        }
    }
}

void OSCHandler::handleSetPlayerFrameRate(const juce::OSCMessage& message) {
    if (message.size() >= 1) {
        float frameRate = message[0].getFloat32();
        
        juce::OSCMessage forwardMsg("/playerFrameRate");
        forwardMsg.addFloat32(frameRate);
        
        clientManager->sendToClientsOfType(forwardMsg, ClientType::Player);
    }
}

void OSCHandler::handleSetPlayerPosition(const juce::OSCMessage& message) {
    if (message.size() >= 2) {
        playerLastUpdate = message[0].getInt32();
        float playerPositionInSeconds = message[1].getFloat32();
        
        juce::OSCMessage forwardMsg("/playerPosition");
        forwardMsg.addInt32(playerLastUpdate);
        forwardMsg.addFloat32(playerPositionInSeconds);
        
        clientManager->sendToClientsOfType(forwardMsg, ClientType::Player);
    }
}

void OSCHandler::handleSetPlayerIsPlaying(const juce::OSCMessage& message) {
    if (message.size() >= 2) {
        playerLastUpdate = message[0].getInt32();
        bool playerIsPlaying = message[1].getInt32() != 0;
        
        juce::OSCMessage forwardMsg("/playerIsPlaying");
        forwardMsg.addInt32(playerLastUpdate);
        forwardMsg.addInt32(playerIsPlaying ? 1 : 0);
        
        clientManager->sendToClientsOfType(forwardMsg, ClientType::Player);
    }
}

void OSCHandler::timerCallback() {
    // Send pings to maintain connections
    juce::OSCMessage pingMsg("/m1-ping");
    clientManager->sendToAllClients(pingMsg);
    pluginManager->sendToAllPlugins(pingMsg);
    
    // Cleanup inactive clients and plugins
    clientManager->cleanupInactiveClients();
    pluginManager->cleanupInactivePlugins();
}

} // namespace Mach1
