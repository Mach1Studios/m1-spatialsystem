#pragma once

#include "../Common/Common.h"
#include "../Core/EventSystem.h"

namespace Mach1 {

class PluginManager {
public:
    explicit PluginManager(std::shared_ptr<EventSystem> events);
    
    juce::Result registerPlugin(const M1RegisteredPlugin& plugin);
    void removePlugin(int port);
    void updatePluginSettings(int port, const juce::OSCMessage& message);
    void sendPingToAll();
    void sendMonitorSettings(int mode, float yaw, float pitch, float roll);
    void sendToAllPlugins(const juce::OSCMessage& message);
    void sendToPannerPlugins(const juce::OSCMessage& message);
    bool hasActivePlugins() const;
    void cleanupInactivePlugins();
    
    const std::vector<M1RegisteredPlugin>& getPlugins() const;
    bool hasActivePlugin(int port) const;

private:
    void setupPluginConnection(M1RegisteredPlugin& plugin);
    juce::Result validatePlugin(const M1RegisteredPlugin& plugin);
    
    std::vector<M1RegisteredPlugin> plugins;
    std::shared_ptr<EventSystem> eventSystem;
    juce::CriticalSection mutex;
    
    juce::int64 lastPingTime = 0;
};

} // namespace Mach1