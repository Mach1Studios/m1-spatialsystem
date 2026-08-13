#pragma once

#include "../Common/Common.h"
#include "../Common/MonitorBroadcastThrottle.h"
#include "../Managers/ClientManager.h"
#include "../Managers/PluginManager.h"
#include "../Managers/ServiceManager.h"
#include "../Managers/PannerTrackingManager.h"

namespace Mach1 {

class ExternalMixerProcessor;
class MixEngine;

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
    OSCHandler(ClientManager* clientManager, PluginManager* pluginManager, ServiceManager* serviceManager, PannerTrackingManager* pannerTrackingManager, ExternalMixerProcessor* externalMixer, MixEngine* mixEngine = nullptr);
    ~OSCHandler() override;

    bool startListening(int port);
    void stopListening();
    ActiveMonitorSnapshot getActiveMonitorSnapshot() const;
    void applyMonitorOrientationFromUi(float yaw, float pitch, float roll);
    void applyMonitorModeFromUi(int mode);
    void applyChannelConfigFromUi(int channelCount);

    // Tells every monitor client how many panner instances are currently
    // streaming audio into the helper (memory-share based). Called once per
    // keepalive tick so monitors can treat it as a heartbeat with a timeout;
    // public so integration tests can trigger it without the timer.
    void broadcastStreamingStatusToMonitors();

    // P4: user-facing external renderer toggle. When disabled, the live mix
    // engine is stopped (which also removes the MixBus segment) and every
    // registered panner is told via "/m1-external-renderer-enabled" to fall
    // back to native processing and stop streaming audio blocks.
    bool isExternalRendererEnabled() const { return externalRendererEnabled.load(); }
    void setExternalRendererEnabled(bool enabled, bool notifyChange = true);
    // Persistence hook (set by the service); invoked on user-driven changes.
    std::function<void(bool)> onExternalRendererChanged;

    // Invoked (from the OSC thread) when a plugin asks the helper to reveal
    // its status window ("/m1-show-helper-ui"); the service hops to the
    // message thread internally.
    std::function<void()> onShowUIRequested;

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
    // `force` bypasses the broadcast rate limiter; use it for discrete state
    // changes (mode / active-monitor switches) that must reach plugins now.
    void broadcastMonitorSettings(const MonitorStateCache& state, bool force = false);
    void broadcastMonitorChannelConfig(int channelCount);
    // Targeted monitor-settings + channel-config reply (registration and
    // editor-reopen refresh).
    void sendCurrentMonitorStateToPlugin(int port);
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
    MixEngine* mixEngine;
    
    juce::OSCReceiver receiver;
    using MessageHandler = std::function<void(const juce::OSCMessage&)>;
    std::unordered_map<juce::String, MessageHandler> messageHandlers;
    
    void broadcastExternalRendererState();

    // Cached state
    mutable juce::CriticalSection stateMutex;
    std::unordered_map<int, MonitorStateCache> monitorStatesByPort;
    MonitorBroadcastThrottle monitorBroadcastThrottle; // guarded by stateMutex
    int playerLastUpdate = 0;

    std::atomic<bool> externalRendererEnabled { true };

    static constexpr int KEEPALIVE_INTERVAL_MS = 1000;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OSCHandler)
};

} // namespace Mach1
