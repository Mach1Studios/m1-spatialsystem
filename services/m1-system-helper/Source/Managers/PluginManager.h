/*
    PluginManager.h
    -----------------
    Manages plugins registered with the system helper.
    Sends and receives OSC messages to plugins.
    Tracks plugin state and pings them to keep them alive.

    Designed for less critical plugins to support supplemental UI features on those plugins.
*/

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
    // When `onlyToOpenEditors` is true, plugins that reported a closed editor
    // are skipped (the values only feed their UI overlay). Discrete changes
    // (monitor mode, active-monitor switches) must pass false so every
    // instance stays in sync for audio-relevant state.
    void sendMonitorSettings(int mode, float yaw, float pitch, float roll, bool onlyToOpenEditors = false);
    // Records whether the plugin's editor is open. Returns true when this is
    // a closed -> open transition (caller should push fresh monitor state).
    bool setEditorOpen(int port, bool open);
    // Sends a message to the single plugin registered on `port`.
    // Returns false if the plugin is unknown or the send fails.
    bool sendToPlugin(int port, const juce::OSCMessage& message);
    void sendToAllPlugins(const juce::OSCMessage& message);
    void sendToPannerPlugins(const juce::OSCMessage& message);
    bool hasActivePlugins() const;
    void cleanupInactivePlugins();
    
    const std::vector<M1RegisteredPlugin>& getPlugins() const;
    std::vector<M1RegisteredPlugin> getPluginsSnapshot() const;
    bool hasActivePlugin(int port) const;
    void updatePluginTime(int port);
    size_t getPluginCount() const { return plugins.size(); }

private:
    void setupPluginConnection(M1RegisteredPlugin& plugin);
    juce::Result validatePlugin(const M1RegisteredPlugin& plugin);
    
    std::vector<M1RegisteredPlugin> plugins;
    std::shared_ptr<EventSystem> eventSystem;
    juce::CriticalSection mutex;
    
    juce::int64 lastPingTime = 0;
};

} // namespace Mach1
