/*
    PannerTrackingManager.h
    -----------------------
    Main orchestrator for tracking M1-Panner plugin instances.
    
    Logic Flow:
    1. Try M1MemoryShare tracking first (direct shared memory access)
    2. Fall back to OSC/UDP tracking if M1MemoryShare unavailable
    3. Provide unified interface regardless of tracking method
    
    This provides the best of both worlds:
    - High performance with M1MemoryShare when available
    - Backward compatibility with OSC when needed
*/

#pragma once

#include "../Common/Common.h"
#include "../Core/EventSystem.h"
#include "M1MemoryShareTracker.h"
#include "OSCPannerTracker.h"
#include <memory>
#include <vector>

namespace Mach1 {

/**
 * Connection status for panner instances
 */
enum class PannerConnectionStatus {
    Active,       // Recently updated, actively streaming
    Stale,        // Timed out but process still running (not playing audio)
    Disconnected  // Process no longer running or connection lost
};

/**
 * Unified panner information structure
 * Normalizes data from both M1MemoryShare and OSC sources
 */
struct PannerInfo {
    // Identity
    int port = 0;
    std::string name;
    uint32_t processId = 0;
    
    // State
    bool isActive = false;
    bool isMemoryShareBased = false;  // true = M1MemoryShare, false = OSC
    juce::int64 lastUpdateTime = 0;
    PannerConnectionStatus connectionStatus = PannerConnectionStatus::Active;
    
    // Audio info
    uint32_t sampleRate = 44100;
    uint32_t channels = 1;
    uint32_t samplesPerBlock = 512;
    
    // Panner parameters (normalized values)
    float azimuth = 0.0f;
    float elevation = 0.0f;
    float diverge = 50.0f;
    float gain = 0.0f;  // in dB
    float stereoOrbitAzimuth = 0.0f;
    float stereoSpread = 50.0f;
    float stereoInputBalance = 0.0f;
    bool autoOrbit = true;
    
    // DAW integration
    uint64_t dawTimestamp = 0;
    double playheadPositionInSeconds = 0.0;
    bool isPlaying = false;
    
    // Buffer tracking (M1MemoryShare only)
    uint64_t currentBufferId = 0;
    uint32_t queuedBufferCount = 0;
    uint32_t consumerCount = 0;
    
    // Visual
    juce::OSCColour color;
    int state = 0;  // -1=delete, 0=off, 1=on, 2=focused
    
    // Modes
    int inputMode = 0;
    int outputMode = 0;
    int pannerMode = 0;
    
    bool operator==(const PannerInfo& other) const {
        return port == other.port && processId == other.processId;
    }
};

/**
 * Main panner tracking manager
 * Provides unified interface for both M1MemoryShare and OSC tracking
 */
class PannerTrackingManager {
public:
    explicit PannerTrackingManager(std::shared_ptr<EventSystem> events);
    ~PannerTrackingManager();
    
    // Setup
    void initializeOSCTracker(PluginManager* pluginManager);
    
    // Main interface
    void start();
    void stop();
    void update();  // Call regularly to scan for panners
    
    // Panner discovery and access
    std::vector<PannerInfo> getActivePanners() const;
    PannerInfo* findPanner(int port, uint32_t processId = 0);
    bool hasPanners() const;
    
    // Tracking method info
    bool isUsingMemoryShare() const { return usingMemoryShare; }
    bool isUsingOSC() const { return usingOSC; }
    juce::String getTrackingStatus() const;
    
    // Direct access to trackers (for audio processing)
    M1MemoryShareTracker* getMemoryShareTracker() { return memoryShareTracker.get(); }
    
    // Send commands to panners
    void sendToAllPanners(const juce::OSCMessage& message);
    void sendToPanner(const PannerInfo& panner, const juce::OSCMessage& message);
    
    // Bidirectional communication via MemoryShare
    bool sendParameterUpdate(const PannerInfo& panner, const std::string& parameterName, float value);
    bool sendParameterUpdate(const PannerInfo& panner, const std::string& parameterName, int value);
    bool sendParameterUpdate(const PannerInfo& panner, const std::string& parameterName, bool value);
    
    // OSC fallback registration (for backward compatibility)
    juce::Result registerOSCPanner(const M1RegisteredPlugin& plugin);
    void removeOSCPanner(int port);
    
    // Consumer management (M1MemoryShare only)
    bool registerAsConsumer(uint32_t consumerId);
    bool unregisterAsConsumer(uint32_t consumerId);
    
    // Statistics
    struct TrackingStats {
        uint32_t memorySharePanners = 0;
        uint32_t oscPanners = 0;
        uint32_t totalPanners = 0;
        juce::int64 lastScanTime = 0;
        bool memoryShareAvailable = false;
        bool oscAvailable = false;
    };
    
    TrackingStats getTrackingStats() const;

private:
    // Core logic - easy to follow
    void scanForPanners();
    void tryMemoryShareTracking();
    void tryOSCTracking();
    void updateTrackingMethod();
    void mergeTrackingResults();
    
    // Event publishing
    void publishPannerAdded(const PannerInfo& panner);
    void publishPannerUpdated(const PannerInfo& panner);
    void publishPannerRemoved(const PannerInfo& panner);
    void publishTrackingMethodChanged(bool memoryShare, bool osc);
    
    // Utility
    PannerInfo convertFromMemoryShare(const MemorySharePannerInfo& info);
    PannerInfo convertFromOSC(const M1RegisteredPlugin& plugin);
    void cleanupInactivePanners();
    
    // Dependencies
    std::shared_ptr<EventSystem> eventSystem;
    
    // Tracking components
    std::unique_ptr<M1MemoryShareTracker> memoryShareTracker;
    std::unique_ptr<OSCPannerTracker> oscTracker;
    
    // State
    std::vector<PannerInfo> activePanners;
    mutable juce::CriticalSection pannersMutex;
    
    // Tracking method flags
    bool usingMemoryShare = false;
    bool usingOSC = false;
    bool initialized = false;
    
    // Configuration
    static constexpr int SCAN_INTERVAL_MS = 1000;  // Scan every 1 second
    static constexpr int PANNER_TIMEOUT_MS = 30000; // Consider inactive after 30 seconds
    static constexpr uint32_t CONSUMER_ID = 9001;   // Our consumer ID for M1MemoryShare
    
    // Process checking helper
    bool isProcessRunning(uint32_t processId) const;
    
    juce::int64 lastScanTime = 0;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PannerTrackingManager)
};

} // namespace Mach1 
