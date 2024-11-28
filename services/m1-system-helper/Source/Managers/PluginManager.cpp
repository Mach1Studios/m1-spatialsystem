#include "PluginManager.h"

namespace Mach1 {

PluginManager::PluginManager(std::shared_ptr<EventSystem> events)
    : eventSystem(std::move(events))
    , lastPingTime(0)
{
}

juce::Result PluginManager::registerPlugin(const M1RegisteredPlugin& plugin) {
    const juce::ScopedLock lock(mutex);
    
    DBG("[PluginManager] Registering plugin on port: " + std::to_string(plugin.port));
    
    // Check if plugin already exists
    auto it = std::find_if(plugins.begin(), plugins.end(),
        [&plugin](const auto& existing) { return existing.port == plugin.port; });
        
    if (it != plugins.end()) {
        // Update existing plugin
        *it = plugin;
        setupPluginConnection(*it);
        updatePluginTime(plugin.port);
        eventSystem->publish("PluginUpdated", plugin.port);
        DBG("[PluginManager] Updated existing plugin");
    } else {
        // Add new plugin
        auto newPlugin = plugin;
        setupPluginConnection(newPlugin);
        plugins.push_back(newPlugin);
        
        eventSystem->publish("PluginAdded", plugin.port);
        DBG("[PluginManager] New plugin added on port: " + std::to_string(plugin.port));
    }
    
    return juce::Result::ok();
}

void PluginManager::removePlugin(int port) {
    const juce::ScopedLock lock(mutex);
    
    auto it = std::find_if(plugins.begin(), plugins.end(),
        [port](const auto& plugin) { return plugin.port == port; });
        
    if (it != plugins.end()) {
        eventSystem->publish("PluginRemoved", port);
        plugins.erase(it);
    }
}

void PluginManager::updatePluginSettings(int port, const juce::OSCMessage& message) {
    const juce::ScopedLock lock(mutex);
    
    auto it = std::find_if(plugins.begin(), plugins.end(),
        [port](const auto& plugin) { return plugin.port == port; });
        
    if (it != plugins.end() && message.size() >= 10) {
        // Basic properties
        it->state = message[1].getInt32();
        if (message[2].isString()) {
            it->name = message[2].getString().toStdString();
        }
        if (message[3].isColour()) {
            it->color = message[3].getColour();
        }
        it->inputMode = message[4].getInt32();
        it->azimuth = message[5].getFloat32();
        it->elevation = message[6].getFloat32();
        it->diverge = message[7].getFloat32();
        it->gain = message[8].getFloat32();
        it->pannerMode = message[9].getInt32();
        
        // Optional stereo properties
        if (message.size() >= 13) {
            it->autoOrbit = message[10].getInt32();
            it->stOrbitAzimuth = message[11].getFloat32();
            it->stSpread = message[12].getFloat32();
        }
        
        it->isPannerPlugin = true;  // Mark as panner plugin
        eventSystem->publish("PluginSettingsUpdated", port);
    }
}

void PluginManager::sendMonitorSettings(int mode, float yaw, float pitch, float roll) {
    const juce::ScopedLock lock(mutex);
    
    DBG("[PluginManager] Sending monitor settings to " + std::to_string(plugins.size()) + " plugins");
    
    juce::OSCMessage msg("/monitor-settings");
    msg.addInt32(mode);
    msg.addFloat32(yaw);
    msg.addFloat32(pitch);
    msg.addFloat32(roll);
    
    for (auto& plugin : plugins) {
        if (plugin.messageSender) {
            if (!plugin.messageSender->send(msg)) {
                DBG("[PluginManager] Failed to send monitor settings to plugin on port: " + 
                    std::to_string(plugin.port));
            } else {
                DBG("[PluginManager] Sent monitor settings to plugin on port: " + 
                    std::to_string(plugin.port) + 
                    " (Mode=" + std::to_string(mode) + 
                    ", Y=" + std::to_string(yaw) + 
                    ", P=" + std::to_string(pitch) + 
                    ", R=" + std::to_string(roll) + ")");
            }
        } else {
            DBG("[PluginManager] Plugin on port " + std::to_string(plugin.port) + 
                " has no message sender!");
        }
    }
}

void PluginManager::sendToAllPlugins(const juce::OSCMessage& message) {
    const juce::ScopedLock lock(mutex);
    
    for (auto& plugin : plugins) {
        if (plugin.messageSender) {
            if (!plugin.messageSender->send(message)) {
                DBG("Failed to send message to plugin on port " + juce::String(plugin.port));
            }
        }
    }
}

void PluginManager::sendToPannerPlugins(const juce::OSCMessage& message) {
    const juce::ScopedLock lock(mutex);
    
    for (auto& plugin : plugins) {
        if (plugin.isPannerPlugin && plugin.messageSender) {
            if (!plugin.messageSender->send(message)) {
                DBG("Failed to send message to panner plugin on port " + juce::String(plugin.port));
            }
        }
    }
}

const std::vector<M1RegisteredPlugin>& PluginManager::getPlugins() const {
    return plugins;
}

bool PluginManager::hasActivePlugins() const {
    const juce::ScopedLock lock(mutex);
    return !plugins.empty();
}

void PluginManager::setupPluginConnection(M1RegisteredPlugin& plugin) {
    if (!plugin.messageSender) {
        plugin.messageSender = std::make_unique<juce::OSCSender>();
    }
    
    if (!plugin.messageSender->connect("127.0.0.1", plugin.port)) {
        DBG("Failed to connect to plugin on port " + juce::String(plugin.port));
    }
}

juce::Result PluginManager::validatePlugin(const M1RegisteredPlugin& plugin) {
    if (!isValidPort(plugin.port)) {
        return juce::Result::fail("Invalid plugin port");
    }
    
    if (plugin.name.empty()) {
        return juce::Result::fail("Plugin name cannot be empty");
    }
    
    return juce::Result::ok();
}

void PluginManager::cleanupInactivePlugins() {
    const juce::ScopedLock lock(mutex);
    auto currentTime = juce::Time::currentTimeMillis();
    
    plugins.erase(
        std::remove_if(plugins.begin(), plugins.end(),
            [currentTime](const auto& plugin) {
                // Remove plugins that haven't responded to pings
                DBG("[PluginManager] Removing instance at port: " + std::to_string(plugin.port));
                return (currentTime - plugin.time) > CLIENT_TIMEOUT_MS;
            }),
        plugins.end()
    );
}

bool PluginManager::hasActivePlugin(int port) const {
    const juce::ScopedLock lock(mutex);
    
    auto it = std::find_if(plugins.begin(), plugins.end(),
        [port](const auto& plugin) {
            return plugin.port == port &&
                   (juce::Time::currentTimeMillis() - plugin.time) < CLIENT_TIMEOUT_MS;
        });
        
    return it != plugins.end();
}

void PluginManager::updatePluginTime(int port) {
    const juce::ScopedLock lock(mutex);
    
    for (auto& plugin : plugins) {
        if (plugin.port == port) {
            plugin.time = juce::Time::currentTimeMillis();
            break;
        }
    }
}

} // namespace Mach1
