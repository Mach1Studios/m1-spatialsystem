#pragma once

#include "../Common/Common.h"
#include "../Managers/ClientManager.h"
#include "../Managers/PluginManager.h"
#include "../Managers/ServiceManager.h"
#include "../Managers/PannerTrackingManager.h"

namespace Mach1 {

class ExternalMixerProcessor;

struct ActiveMonitorSnapshot {
    std::vector<M1OrientationClientConnection> monitors;
    int activeMonitorPort = 0;
    float masterYaw = 0.0f;
    float masterPitch = 0.0f;
    float masterRoll = 0.0f;
    int masterMode = 0;
    int systemChannelCount = 8;
};

class OSCHandler : public juce::OSCReceiver::Listener<juce::OSCReceiver::RealtimeCallback>,
                  public juce::Timer  // Add Timer
{
public:
    OSCHandler(ClientManager* clientManager, PluginManager* pluginManager, ServiceManager* serviceManager, PannerTrackingManager* pannerTrackingManager, ExternalMixerProcessor* externalMixer);
    ~OSCHandler() override;

    bool startListening(int port);
    void stopListening();
    ActiveMonitorSnapshot getActiveMonitorSnapshot() const;
    void applyMonitorOrientationFromUi(float yaw, float pitch, float roll);
    void applyMonitorModeFromUi(int mode);
    void applyChannelConfigFromUi(int channelCount);

private:
    struct MonitorStateCache {
        float yaw = 0.0f;
        float pitch = 0.0f;
        float roll = 0.0f;
        int mode = 0;
        int channelCount = 8;
        juce::int64 lastUpdateTime = 0;
    };

    void timerCallback() override;  // Add this
    void oscMessageReceived(const juce::OSCMessage& message) override;
    void setupMessageHandlers();
    int getActiveMonitorPort() const;
    int resolveMonitorPortFromMessage(const juce::OSCMessage& message, int payloadItemsWithoutPort) const;
    MonitorStateCache& getOrCreateMonitorStateLocked(int port);
    MonitorStateCache getMonitorStateLocked(int port) const;
    void broadcastMonitorSettings(const MonitorStateCache& state);
    void broadcastMonitorChannelConfig(int channelCount);
    bool sendMessageToMonitorClient(int port, const juce::OSCMessage& message) const;
    void pruneInactiveMonitorStates();
    
    // Message handlers
    void handleAddClient(const juce::OSCMessage& message);
    void handleRemoveClient(const juce::OSCMessage& message);
    void handleClientPulse(const juce::OSCMessage& message);
    void handleSetPlayerYPR(const juce::OSCMessage& message);
    void handleSetChannelConfig(const juce::OSCMessage& message);
    void handleSetMonitorActive(const juce::OSCMessage& message);
    void handleSetMonitoringMode(const juce::OSCMessage& message);
    void handleSetMasterYPR(const juce::OSCMessage& message);
    void handleRegisterPlugin(const juce::OSCMessage& message);
    void handlePannerSettings(const juce::OSCMessage& message);
    void handleClientRequestsServer(const juce::OSCMessage& message);
    void handleOMClientPulse(const juce::OSCMessage& message);
    void handlePluginPulse(const juce::OSCMessage& message);
    void handleSetChannelConfigRequest(const juce::OSCMessage& message);
    void handleSetMonitorActiveRequest(const juce::OSCMessage& message);
    void handleSetPlayerFrameRate(const juce::OSCMessage& message);
    void handleSetPlayerPosition(const juce::OSCMessage& message);
    void handleSetPlayerIsPlaying(const juce::OSCMessage& message);

    ClientManager* clientManager;
    PluginManager* pluginManager;
    ServiceManager* serviceManager;
    PannerTrackingManager* pannerTrackingManager;
    ExternalMixerProcessor* externalMixer;
    
    juce::OSCReceiver receiver;
    using MessageHandler = std::function<void(const juce::OSCMessage&)>;
    std::unordered_map<juce::String, MessageHandler> messageHandlers;
    
    // Cached state
    mutable juce::CriticalSection stateMutex;
    std::unordered_map<int, MonitorStateCache> monitorStatesByPort;
    int playerLastUpdate = 0;

    static constexpr int KEEPALIVE_INTERVAL_MS = 1000;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OSCHandler)
};

} // namespace Mach1
