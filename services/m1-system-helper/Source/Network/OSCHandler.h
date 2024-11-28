#pragma once

#include "../Common/Common.h"
#include "../Managers/ClientManager.h"
#include "../Managers/PluginManager.h"
#include "../Managers/ServiceManager.h"

namespace Mach1 {

class OSCHandler : public juce::OSCReceiver::Listener<juce::OSCReceiver::RealtimeCallback>,
                  public juce::Timer  // Add Timer
{
public:
    OSCHandler(ClientManager* clientManager, PluginManager* pluginManager, ServiceManager* processManager);
    ~OSCHandler() override;

    bool startListening(int port);
    void stopListening();

private:
    void timerCallback() override;  // Add this
    void oscMessageReceived(const juce::OSCMessage& message) override;
    void setupMessageHandlers();
    
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
    ServiceManager* processManager;
    
    juce::OSCReceiver receiver;
    using MessageHandler = std::function<void(const juce::OSCMessage&)>;
    std::unordered_map<juce::String, MessageHandler> messageHandlers;
    
    // Cached state
    float masterYaw = 0.0f, masterPitch = 0.0f, masterRoll = 0.0f;
    int masterMode = 0;
    float prevMasterYaw = 0.0f, prevMasterPitch = 0.0f, prevMasterRoll = 0.0f;
    int prevMasterMode = 0;
    int lastSystemChannelCount = 0;
    int playerLastUpdate = 0;

    juce::int64 pingTime = 0;
    juce::int64 timeWhenHelperLastSeenAClient = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OSCHandler)
};

} // namespace Mach1
