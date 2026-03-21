/*
    OSCPannerTracker.h
    ------------------
    Wraps existing OSC/UDP functionality for panner tracking fallback.
    
    Logic Flow:
    1. Wrap existing PluginManager OSC functionality
    2. Provide same interface as M1MemoryShareTracker
    3. Handle OSC message registration and communication
    4. Maintain backward compatibility
    
    This provides fallback when M1MemoryShare is not available.
*/

#pragma once

#include "../Common/Common.h"
#include <vector>
#include <memory>

namespace Mach1 {

// Forward declarations
class PluginManager;

/**
 * OSC-based panner tracker that wraps existing PluginManager functionality
 * Provides fallback when M1MemoryShare is not available
 */
class OSCPannerTracker {
public:
    explicit OSCPannerTracker(PluginManager* pluginManager);
    ~OSCPannerTracker();
    
    // Main interface (matches M1MemoryShareTracker for consistency)
    void start();
    void stop();
    void update();
    
    // Panner discovery and access
    std::vector<M1RegisteredPlugin> getActivePanners() const;
    M1RegisteredPlugin* findPanner(int port);
    bool hasPanners() const;
    bool isAvailable() const;
    
    // OSC registration (for backward compatibility)
    bool registerPanner(const M1RegisteredPlugin& plugin);
    void removePanner(int port);
    
    // OSC communication
    void sendToPanner(int port, const juce::OSCMessage& message);
    void sendToAllPanners(const juce::OSCMessage& message);
    
    // Statistics
    struct OSCStats {
        uint32_t totalPanners = 0;
        uint32_t activePanners = 0;
        juce::int64 lastUpdateTime = 0;
        bool pluginManagerAvailable = false;
    };
    
    OSCStats getStats() const;

private:
    // Wrapped functionality
    PluginManager* pluginManager;
    
    // State
    bool isRunning = false;
    bool initialized = false;
    std::vector<M1RegisteredPlugin> registeredPanners;
    
    // Timing
    juce::int64 lastUpdateTime = 0;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OSCPannerTracker)
};

} // namespace Mach1 