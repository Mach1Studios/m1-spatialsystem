#include "OSCPannerTracker.h"
#include "PluginManager.h"

namespace Mach1 {

OSCPannerTracker::OSCPannerTracker(PluginManager* pluginManager) 
    : pluginManager(pluginManager) {
}

OSCPannerTracker::~OSCPannerTracker() {
    stop();
}

void OSCPannerTracker::start() {
    // TODO: Start OSC discovery and communication
}

void OSCPannerTracker::stop() {
    // TODO: Stop OSC communication and cleanup
}

void OSCPannerTracker::update() {
    // TODO: Update panner data via OSC communication
    if (pluginManager) {
        // Update from plugin manager
        registeredPanners = pluginManager->getPlugins();
    }
}

std::vector<M1RegisteredPlugin> OSCPannerTracker::getActivePanners() const {
    return registeredPanners;
}

M1RegisteredPlugin* OSCPannerTracker::findPanner(int port) {
    for (auto& panner : registeredPanners) {
        if (panner.port == port) {
            return &panner;
        }
    }
    return nullptr;
}

bool OSCPannerTracker::hasPanners() const {
    return !registeredPanners.empty();
}

bool OSCPannerTracker::isAvailable() const {
    return true; // OSC is always available
}

bool OSCPannerTracker::registerPanner(const M1RegisteredPlugin& plugin) {
    // Add to our list if not already present
    auto existing = findPanner(plugin.port);
    if (!existing) {
        registeredPanners.push_back(plugin);
    }
    return true;
}

void OSCPannerTracker::removePanner(int port) {
    registeredPanners.erase(
        std::remove_if(registeredPanners.begin(), registeredPanners.end(),
                      [port](const M1RegisteredPlugin& p) { return p.port == port; }),
        registeredPanners.end());
}

void OSCPannerTracker::sendToPanner(int port, const juce::OSCMessage& message) {
    // TODO: Send OSC message to specific panner by port
    // Find the panner and send the message to its address
    auto* panner = findPanner(port);
    if (panner) {
        // Implementation would send OSC message to panner->address:panner->port
        // For now, just log that we would send it
        DBG("Would send OSC message to panner at port " + juce::String(port));
    }
}

void OSCPannerTracker::sendToAllPanners(const juce::OSCMessage& message) {
    // TODO: Send OSC message to all registered panners
    for (const auto& panner : registeredPanners) {
        sendToPanner(panner.port, message);
    }
}

OSCPannerTracker::OSCStats OSCPannerTracker::getStats() const {
    OSCStats stats;
    stats.totalPanners = static_cast<uint32_t>(registeredPanners.size());
    stats.activePanners = static_cast<uint32_t>(registeredPanners.size()); // All registered panners are considered active for OSC
    stats.lastUpdateTime = lastUpdateTime;
    stats.pluginManagerAvailable = (pluginManager != nullptr);
    return stats;
}

} // namespace Mach1 