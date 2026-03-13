// Network/OSCHandler.cpp
#include "OSCHandler.h"
#include "../Core/ExternalMixerProcessor.h"

namespace Mach1 {

OSCHandler::OSCHandler(ClientManager* clientManager, PluginManager* pluginManager, ServiceManager* serviceManager, PannerTrackingManager* pannerTrackingManager, ExternalMixerProcessor* externalMixer)
    : clientManager(clientManager)
    , pluginManager(pluginManager)
    , serviceManager(serviceManager)
    , pannerTrackingManager(pannerTrackingManager)
    , externalMixer(externalMixer)
{
    setupMessageHandlers();
    // Keepalive and stale-client cleanup do not need to run on a 20ms message-thread loop.
    startTimer(KEEPALIVE_INTERVAL_MS);
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

int OSCHandler::getActiveMonitorPort() const
{
    if (!clientManager)
        return 0;

    const auto monitors = clientManager->getClientsByType(ClientType::Monitor);
    if (monitors.empty())
        return 0;

    for (const auto& monitor : monitors)
    {
        if (monitor.active)
            return monitor.port;
    }

    return monitors.front().port;
}

int OSCHandler::resolveMonitorPortFromMessage(const juce::OSCMessage& message, int payloadItemsWithoutPort) const
{
    if (message.size() >= payloadItemsWithoutPort + 1 && message[0].isInt32())
        return message[0].getInt32();

    return getActiveMonitorPort();
}

OSCHandler::MonitorStateCache& OSCHandler::getOrCreateMonitorStateLocked(int port)
{
    return monitorStatesByPort[port];
}

OSCHandler::MonitorStateCache OSCHandler::getMonitorStateLocked(int port) const
{
    const auto it = monitorStatesByPort.find(port);
    if (it != monitorStatesByPort.end())
        return it->second;

    return {};
}

void OSCHandler::broadcastMonitorSettings(const MonitorStateCache& state)
{
    if (externalMixer)
        externalMixer->setMasterYPR(state.yaw, state.pitch, state.roll);

    if (pluginManager)
        pluginManager->sendMonitorSettings(state.mode, state.yaw, state.pitch, state.roll);
}

void OSCHandler::broadcastMonitorChannelConfig(int channelCount)
{
    if (channelCount != 4 && channelCount != 8 && channelCount != 14)
        return;

    juce::OSCMessage forwardMsg("/m1-channel-config");
    forwardMsg.addInt32(channelCount);

    if (pluginManager)
        pluginManager->sendToAllPlugins(forwardMsg);

    if (externalMixer)
        externalMixer->setOutputFormat(channelCount);
}

bool OSCHandler::sendMessageToMonitorClient(int port, const juce::OSCMessage& message) const
{
    if (port <= 0 || clientManager == nullptr || !clientManager->hasActiveClientOfType(port, "monitor"))
        return false;

    juce::OSCSender sender;
    if (!sender.connect("127.0.0.1", port))
    {
        DBG("[OSCHandler] Failed to connect to monitor client on port: " + std::to_string(port));
        return false;
    }

    if (!sender.send(message))
    {
        DBG("[OSCHandler] Failed to send monitor control message to port: " + std::to_string(port));
        return false;
    }

    return true;
}

void OSCHandler::pruneInactiveMonitorStates()
{
    std::vector<M1OrientationClientConnection> monitors;
    if (clientManager)
        monitors = clientManager->getClientsByType(ClientType::Monitor);

    const juce::ScopedLock lock(stateMutex);
    for (auto it = monitorStatesByPort.begin(); it != monitorStatesByPort.end();)
    {
        const bool monitorStillConnected = std::any_of(
            monitors.begin(),
            monitors.end(),
            [port = it->first](const auto& monitor) { return monitor.port == port; });

        if (!monitorStillConnected)
            it = monitorStatesByPort.erase(it);
        else
            ++it;
    }
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

        if (client.type == ClientType::Monitor)
        {
            const juce::ScopedLock lock(stateMutex);
            auto& state = getOrCreateMonitorStateLocked(client.port);
            state.lastUpdateTime = client.time;
        }
        
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
    const int valueOffset = (message.size() >= 4 && message[0].isInt32()) ? 1 : 0;
    const int monitorPort = resolveMonitorPortFromMessage(message, 3);

    if (message.size() >= valueOffset + 3) {
        const float yaw = message[valueOffset + 0].getFloat32();
        const float pitch = message[valueOffset + 1].getFloat32();
        const float roll = message[valueOffset + 2].getFloat32();

        MonitorStateCache state;
        {
            const juce::ScopedLock lock(stateMutex);
            if (monitorPort > 0) {
                auto& cachedState = getOrCreateMonitorStateLocked(monitorPort);
                cachedState.yaw = yaw;
                cachedState.pitch = pitch;
                cachedState.roll = roll;
                cachedState.lastUpdateTime = juce::Time::currentTimeMillis();
                state = cachedState;
            } else {
                state.yaw = yaw;
                state.pitch = pitch;
                state.roll = roll;
                state.lastUpdateTime = juce::Time::currentTimeMillis();
            }
        }

        if (monitorPort == 0 || monitorPort == getActiveMonitorPort())
        {
            broadcastMonitorSettings(state);
            DBG("[Monitor] YPR updated from port " + std::to_string(monitorPort) +
                ": Y=" + std::to_string(yaw) +
                " P=" + std::to_string(pitch) +
                " R=" + std::to_string(roll));
        }
    } else {
        DBG("[OSCHandler] Invalid YPR message size: " + std::to_string(message.size()));
    }
}

void OSCHandler::handleRemoveClient(const juce::OSCMessage& message) {
    if (message.size() >= 1) {
        int port = message[0].getInt32();
        clientManager->removeClient(port);

        {
            const juce::ScopedLock lock(stateMutex);
            monitorStatesByPort.erase(port);
        }
        
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
    const int valueOffset = (message.size() >= 2 && message[0].isInt32()) ? 1 : 0;
    const int monitorPort = resolveMonitorPortFromMessage(message, 1);

    if (message.size() >= valueOffset + 1) {
        const int mode = message[valueOffset].getInt32();

        MonitorStateCache state;
        {
            const juce::ScopedLock lock(stateMutex);
            if (monitorPort > 0) {
                auto& cachedState = getOrCreateMonitorStateLocked(monitorPort);
                cachedState.mode = mode;
                cachedState.lastUpdateTime = juce::Time::currentTimeMillis();
                state = cachedState;
            } else {
                state.mode = mode;
                state.lastUpdateTime = juce::Time::currentTimeMillis();
            }
        }

        if (monitorPort == 0 || monitorPort == getActiveMonitorPort())
            broadcastMonitorSettings(state);
    }
}

void OSCHandler::handleRegisterPlugin(const juce::OSCMessage& message) {
    if (message.size() >= 1) {
        M1RegisteredPlugin plugin;
        plugin.port = message[0].getInt32();
        plugin.time = juce::Time::currentTimeMillis();
        pluginManager->registerPlugin(plugin);
        
        // Also register with the panner tracking manager for unified tracking
        if (pannerTrackingManager) {
            pannerTrackingManager->registerOSCPanner(plugin);
            DBG("[OSCHandler] Registered panner plugin on port " + juce::String(plugin.port) + " with tracking manager");
        }

        const auto activeMonitorSnapshot = getActiveMonitorSnapshot();
        pluginManager->sendMonitorSettings(activeMonitorSnapshot.masterMode,
                                           activeMonitorSnapshot.masterYaw,
                                           activeMonitorSnapshot.masterPitch,
                                           activeMonitorSnapshot.masterRoll);
        broadcastMonitorChannelConfig(activeMonitorSnapshot.systemChannelCount);
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
    if (serviceManager != nullptr)
        serviceManager->noteOrientationManagerClientPulse();
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
    const int valueOffset = (message.size() >= 2 && message[0].isInt32()) ? 1 : 0;
    const int monitorPort = resolveMonitorPortFromMessage(message, 1);

    if (message.size() >= valueOffset + 1) {
        const int channelCountForConfig = message[valueOffset].getInt32();
        if (channelCountForConfig != 4 && channelCountForConfig != 8 && channelCountForConfig != 14)
            return;

        if (monitorPort > 0) {
            const juce::ScopedLock lock(stateMutex);
            auto& cachedState = getOrCreateMonitorStateLocked(monitorPort);
            cachedState.channelCount = channelCountForConfig;
            cachedState.lastUpdateTime = juce::Time::currentTimeMillis();
        }

        if (monitorPort == 0 || monitorPort == getActiveMonitorPort()) {
            DBG("[Client] Config request from port " + std::to_string(monitorPort) +
                " for: " + std::to_string(channelCountForConfig) + " channels");
            broadcastMonitorChannelConfig(channelCountForConfig);
        }
    }
}

ActiveMonitorSnapshot OSCHandler::getActiveMonitorSnapshot() const
{
    ActiveMonitorSnapshot snapshot;
    if (clientManager) {
        snapshot.monitors = clientManager->getClientsByType(ClientType::Monitor);
    }

    for (const auto& monitor : snapshot.monitors)
    {
        if (monitor.active) {
            snapshot.activeMonitorPort = monitor.port;
            break;
        }
    }

    if (snapshot.activeMonitorPort == 0 && !snapshot.monitors.empty())
        snapshot.activeMonitorPort = snapshot.monitors.front().port;

    const juce::ScopedLock lock(stateMutex);
    if (snapshot.activeMonitorPort != 0)
    {
        const auto state = getMonitorStateLocked(snapshot.activeMonitorPort);
        snapshot.masterYaw = state.yaw;
        snapshot.masterPitch = state.pitch;
        snapshot.masterRoll = state.roll;
        snapshot.masterMode = state.mode;
        snapshot.systemChannelCount = state.channelCount;
    }

    return snapshot;
}

void OSCHandler::applyMonitorOrientationFromUi(float yaw, float pitch, float roll)
{
    const int activeMonitorPort = getActiveMonitorPort();
    if (activeMonitorPort == 0)
        return;

    MonitorStateCache state;
    {
        const juce::ScopedLock lock(stateMutex);
        auto& cachedState = getOrCreateMonitorStateLocked(activeMonitorPort);
        cachedState.yaw = yaw;
        cachedState.pitch = pitch;
        cachedState.roll = roll;
        cachedState.lastUpdateTime = juce::Time::currentTimeMillis();
        state = cachedState;
    }

    juce::OSCMessage monitorMsg("/m1-set-monitor-ypr");
    monitorMsg.addFloat32(yaw);
    monitorMsg.addFloat32(pitch);
    monitorMsg.addFloat32(roll);
    sendMessageToMonitorClient(activeMonitorPort, monitorMsg);

    broadcastMonitorSettings(state);
}

void OSCHandler::applyMonitorModeFromUi(int mode)
{
    const int activeMonitorPort = getActiveMonitorPort();
    if (activeMonitorPort == 0)
        return;

    MonitorStateCache state;
    {
        const juce::ScopedLock lock(stateMutex);
        auto& cachedState = getOrCreateMonitorStateLocked(activeMonitorPort);
        cachedState.mode = mode;
        cachedState.lastUpdateTime = juce::Time::currentTimeMillis();
        state = cachedState;
    }

    broadcastMonitorSettings(state);
}

void OSCHandler::applyChannelConfigFromUi(int channelCount)
{
    if (channelCount != 4 && channelCount != 8 && channelCount != 14) {
        return;
    }

    const int activeMonitorPort = getActiveMonitorPort();
    if (activeMonitorPort == 0)
        return;

    {
        const juce::ScopedLock lock(stateMutex);
        auto& cachedState = getOrCreateMonitorStateLocked(activeMonitorPort);
        cachedState.channelCount = channelCount;
        cachedState.lastUpdateTime = juce::Time::currentTimeMillis();
    }

    juce::OSCMessage monitorMsg("/m1-channel-config");
    monitorMsg.addInt32(channelCount);
    sendMessageToMonitorClient(activeMonitorPort, monitorMsg);

    broadcastMonitorChannelConfig(channelCount);
}

void OSCHandler::handleSetMonitorActiveRequest(const juce::OSCMessage& message) {
    if (message.size() >= 1) {
        int monitorPort = message[0].getInt32();
        DBG("[Monitor] Port: " + std::to_string(monitorPort) + " is requesting first index");
        
        // Request the ClientManager to rotate the specified monitor to active position
        if (clientManager->rotateMonitorToActive(monitorPort)) {
            MonitorStateCache state;
            bool hasCachedState = false;

            {
                const juce::ScopedLock lock(stateMutex);
                if (const auto it = monitorStatesByPort.find(monitorPort); it != monitorStatesByPort.end()) {
                    state = it->second;
                    hasCachedState = true;
                }
            }

            if (hasCachedState) {
                broadcastMonitorChannelConfig(state.channelCount);
                broadcastMonitorSettings(state);
            }
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
    pruneInactiveMonitorStates();
}

} // namespace Mach1
