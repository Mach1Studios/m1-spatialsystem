/*
    PannerTrackingManager.cpp
    -------------------------
    Implementation of the main panner tracking orchestrator.
    
    Clear Logic Flow:
    1. Try M1MemoryShare first (high performance)
    2. Fall back to OSC if M1MemoryShare unavailable
    3. Provide unified interface regardless of method
    4. Handle transitions between methods gracefully
*/

#include "PannerTrackingManager.h"
#include "M1MemoryShareTracker.h"
#include "OSCPannerTracker.h"
#include "../Common/TypesForDataExchange.h"

namespace Mach1 {

PannerTrackingManager::PannerTrackingManager(std::shared_ptr<EventSystem> events)
    : eventSystem(std::move(events))
    , lastScanTime(0)
{
    // Initialize tracking components
    memoryShareTracker = std::make_unique<M1MemoryShareTracker>(CONSUMER_ID);
    // Note: OSC tracker will be initialized when pluginManager is available
    
    DBG("[PannerTrackingManager] Created with consumer ID: " + std::to_string(CONSUMER_ID));
}

PannerTrackingManager::~PannerTrackingManager() {
    stop();
}

void PannerTrackingManager::initializeOSCTracker(PluginManager* pluginManager) {
    if (!oscTracker && pluginManager) {
        oscTracker = std::make_unique<OSCPannerTracker>(pluginManager);
        DBG("[PannerTrackingManager] OSC tracker initialized with plugin manager");
    }
}

void PannerTrackingManager::start() {
    if (initialized) {
        return;
    }
    
    DBG("[PannerTrackingManager] Starting panner tracking...");
    
    // Try to start M1MemoryShare tracking first
    if (memoryShareTracker) {
        memoryShareTracker->start();
        DBG("[PannerTrackingManager] M1MemoryShare tracker started");
    }
    
    // OSC tracker will be started when pluginManager is available
    
    initialized = true;
    lastScanTime = juce::Time::currentTimeMillis();
    
    DBG("[PannerTrackingManager] Started successfully");
}

void PannerTrackingManager::stop() {
    if (!initialized) {
        return;
    }
    
    DBG("[PannerTrackingManager] Stopping panner tracking...");
    
    // Stop both trackers
    if (memoryShareTracker) {
        memoryShareTracker->stop();
    }
    
    if (oscTracker) {
        oscTracker->stop();
    }
    
    // Clear state
    {
        const juce::ScopedLock lock(pannersMutex);
        activePanners.clear();
    }
    
    usingMemoryShare = false;
    usingOSC = false;
    initialized = false;
    
    DBG("[PannerTrackingManager] Stopped successfully");
}

void PannerTrackingManager::update() {
    if (!initialized) {
        return;
    }
    
    auto currentTime = juce::Time::currentTimeMillis();
    
    // Scan for panners at regular intervals
    if (currentTime - lastScanTime > SCAN_INTERVAL_MS) {
        scanForPanners();
        lastScanTime = currentTime;
    }
    
    // Update tracking method if needed
    updateTrackingMethod();
    
    // Clean up inactive panners
    cleanupInactivePanners();
}

// =============================================================================
// CORE LOGIC - Easy to Follow
// =============================================================================

void PannerTrackingManager::scanForPanners() {
    DBG("[PannerTrackingManager] Scanning for panners...");
    
    // Step 1: Try M1MemoryShare tracking first
    tryMemoryShareTracking();
    
    // Step 2: Fall back to OSC if M1MemoryShare didn't find any panners
    if (!usingMemoryShare || !hasPanners()) {
        tryOSCTracking();
    }
    
    // Step 3: Merge results from both tracking methods
    mergeTrackingResults();
    
    DBG("[PannerTrackingManager] Scan complete - MemoryShare: " + 
        std::to_string(usingMemoryShare) + ", OSC: " + std::to_string(usingOSC));
}

void PannerTrackingManager::tryMemoryShareTracking() {
    if (!memoryShareTracker) {
        return;
    }
    
    // Update M1MemoryShare tracker
    memoryShareTracker->update();
    
    // Check if we found any panners
    bool foundPanners = memoryShareTracker->hasPanners();
    bool wasUsingMemoryShare = usingMemoryShare;
    
    usingMemoryShare = foundPanners && memoryShareTracker->isAvailable();
    
    // Notify if tracking method changed
    if (wasUsingMemoryShare != usingMemoryShare) {
        DBG("[PannerTrackingManager] M1MemoryShare tracking " <<
            (usingMemoryShare ? "ENABLED" : "DISABLED"));
        publishTrackingMethodChanged(usingMemoryShare, usingOSC);
    }
    
    if (usingMemoryShare) {
        DBG("[PannerTrackingManager] M1MemoryShare found " + 
            std::to_string(memoryShareTracker->getStats().activePanners) + " panners");
    }
}

void PannerTrackingManager::tryOSCTracking() {
    if (!oscTracker) {
        return;  // OSC tracker not available yet
    }
    
    // Update OSC tracker
    oscTracker->update();
    
    // Check if we found any panners
    bool foundPanners = oscTracker->hasPanners();
    bool wasUsingOSC = usingOSC;
    
    usingOSC = foundPanners && oscTracker->isAvailable();
    
    // Notify if tracking method changed
    if (wasUsingOSC != usingOSC) {
        DBG("[PannerTrackingManager] OSC tracking " <<
            (usingOSC ? "ENABLED" : "DISABLED"));
        publishTrackingMethodChanged(usingMemoryShare, usingOSC);
    }
    
    if (usingOSC) {
        DBG("[PannerTrackingManager] OSC found " + 
            std::to_string(oscTracker->getStats().activePanners) + " panners");
    }
}

void PannerTrackingManager::updateTrackingMethod() {
    // Priority: M1MemoryShare > OSC
    // If M1MemoryShare becomes available, prefer it over OSC
    if (memoryShareTracker && memoryShareTracker->isAvailable() && 
        memoryShareTracker->hasPanners() && !usingMemoryShare) {
        
        DBG("[PannerTrackingManager] Switching to M1MemoryShare tracking");
        usingMemoryShare = true;
        publishTrackingMethodChanged(usingMemoryShare, usingOSC);
    }
}

void PannerTrackingManager::mergeTrackingResults() {
    const juce::ScopedLock lock(pannersMutex);
    
    // Clear previous results
    activePanners.clear();
    
    // Add M1MemoryShare panners (higher priority)
    if (usingMemoryShare && memoryShareTracker) {
        const auto& memoryPanners = memoryShareTracker->getActivePanners();
        for (const auto& memoryPanner : memoryPanners) {
            activePanners.push_back(convertFromMemoryShare(memoryPanner));
        }
    }
    
    // Add OSC panners (fallback)
    if (usingOSC && oscTracker) {
        const auto& oscPanners = oscTracker->getActivePanners();
        for (const auto& oscPanner : oscPanners) {
            // Only add if not already covered by M1MemoryShare
            bool alreadyExists = false;
            auto oscPannerInfo = convertFromOSC(oscPanner);
            
            for (const auto& existing : activePanners) {
                // Primary: Match by process ID (most reliable)
                // Secondary: Match by port (for cases where process ID isn't available)
                // Tertiary: Match by name (as additional verification)
                if ((existing.processId != 0 && oscPannerInfo.processId != 0 && 
                     existing.processId == oscPannerInfo.processId) ||
                    (existing.port != 0 && oscPannerInfo.port != 0 && 
                     existing.port == oscPannerInfo.port) ||
                    (!existing.name.empty() && !oscPannerInfo.name.empty() && 
                     existing.name == oscPannerInfo.name)) {
                    
                    alreadyExists = true;
                    DBG("[PannerTrackingManager] Skipping OSC panner (already tracked via MemoryShare): " <<
                        "PID=" + std::to_string(oscPannerInfo.processId) <<
                        ", Port=" + std::to_string(oscPannerInfo.port) <<
                        ", Name=" + oscPannerInfo.name);
                    break;
                }
            }
            
            if (!alreadyExists) {
                activePanners.push_back(oscPannerInfo);
                DBG("[PannerTrackingManager] Added OSC panner: " <<
                    "PID=" + std::to_string(oscPannerInfo.processId) <<
                    ", Port=" + std::to_string(oscPannerInfo.port) <<
                    ", Name=" + oscPannerInfo.name);
            }
        }
    }
    
    DBG("[PannerTrackingManager] Merged results: " + 
        std::to_string(activePanners.size()) + " total panners");
}

// =============================================================================
// PANNER ACCESS
// =============================================================================

std::vector<PannerInfo> PannerTrackingManager::getActivePanners() const {
    const juce::ScopedLock lock(pannersMutex);
    return activePanners;
}

PannerInfo* PannerTrackingManager::findPanner(int port, uint32_t processId) {
    const juce::ScopedLock lock(pannersMutex);
    
    for (auto& panner : activePanners) {
        if (panner.port == port && (processId == 0 || panner.processId == processId)) {
            return &panner;
        }
    }
    
    return nullptr;
}

bool PannerTrackingManager::hasPanners() const {
    const juce::ScopedLock lock(pannersMutex);
    return !activePanners.empty();
}

juce::String PannerTrackingManager::getTrackingStatus() const {
    if (usingMemoryShare && usingOSC) {
        return "M1MemoryShare + OSC";
    } else if (usingMemoryShare) {
        return "M1MemoryShare";
    } else if (usingOSC) {
        return "OSC";
    } else {
        return "None";
    }
}

// =============================================================================
// COMMUNICATION
// =============================================================================

void PannerTrackingManager::sendToAllPanners(const juce::OSCMessage& message) {
    // Send via OSC (M1MemoryShare doesn't support sending messages)
    if (usingOSC && oscTracker) {
        oscTracker->sendToAllPanners(message);
    }
}

void PannerTrackingManager::sendToPanner(const PannerInfo& panner, const juce::OSCMessage& message) {
    // Send via OSC (M1MemoryShare doesn't support sending messages)
    if (usingOSC && oscTracker) {
        oscTracker->sendToPanner(panner.port, message);
    }
}

// =============================================================================
// OSC FALLBACK (Backward Compatibility)
// =============================================================================

juce::Result PannerTrackingManager::registerOSCPanner(const M1RegisteredPlugin& plugin) {
    if (oscTracker) {
        bool success = oscTracker->registerPanner(plugin);
        return success ? juce::Result::ok() : juce::Result::fail("Failed to register OSC panner");
    }
    return juce::Result::fail("OSC tracker not available");
}

void PannerTrackingManager::removeOSCPanner(int port) {
    if (oscTracker) {
        oscTracker->removePanner(port);
    }
}

// =============================================================================
// CONSUMER MANAGEMENT
// =============================================================================

bool PannerTrackingManager::registerAsConsumer(uint32_t consumerId) {
    if (memoryShareTracker) {
        return memoryShareTracker->registerAsConsumer(consumerId);
    }
    return false;
}

bool PannerTrackingManager::unregisterAsConsumer(uint32_t consumerId) {
    if (memoryShareTracker) {
        return memoryShareTracker->unregisterAsConsumer(consumerId);
    }
    return false;
}

// =============================================================================
// CONVERSION UTILITIES
// =============================================================================

PannerInfo PannerTrackingManager::convertFromMemoryShare(const MemorySharePannerInfo& info) {
    PannerInfo panner;
    
    // Identity
    panner.port = 0;  // M1MemoryShare doesn't use ports
    panner.name = info.name;
    panner.processId = info.processId;
    
    // State
    panner.isActive = info.isActive;
    panner.isMemoryShareBased = true;
    panner.lastUpdateTime = info.lastUpdateTime;
    
    // Audio info
    panner.sampleRate = info.sampleRate;
    panner.channels = info.channels;
    panner.samplesPerBlock = info.samplesPerBlock;
    
    // Panner parameters
    panner.azimuth = info.getAzimuth();
    panner.elevation = info.getElevation();
    panner.diverge = info.getDiverge();
    panner.gain = info.getGain();
    panner.stereoOrbitAzimuth = info.getStereoOrbitAzimuth();
    panner.stereoSpread = info.getStereoSpread();
    panner.stereoInputBalance = info.getStereoInputBalance();
    
    // DAW integration
    panner.dawTimestamp = info.dawTimestamp;
    panner.playheadPositionInSeconds = info.playheadPositionInSeconds;
    panner.isPlaying = info.isPlaying;
    
    // Buffer tracking
    panner.currentBufferId = info.currentBufferId;
    panner.queuedBufferCount = info.queuedBufferCount;
    panner.consumerCount = info.consumerCount;
    
    // Modes
    panner.inputMode = info.getInputMode();
    panner.autoOrbit = info.getAutoOrbit();
    panner.state = info.getState();
    
    return panner;
}

PannerInfo PannerTrackingManager::convertFromOSC(const M1RegisteredPlugin& plugin) {
    PannerInfo panner;
    
    // Identity
    panner.port = plugin.port;
    panner.name = plugin.name.empty() ? ("OSC Panner " + std::to_string(plugin.port)) : plugin.name;
    panner.processId = 0;  // OSC doesn't provide process ID
    
    // State
    panner.isActive = true;
    panner.isMemoryShareBased = false;
    panner.lastUpdateTime = plugin.time;
    
    // Audio info (defaults for OSC)
    panner.sampleRate = 44100;
    panner.channels = 1;
    panner.samplesPerBlock = 512;
    
    // Panner parameters - use real OSC data or reasonable defaults
    panner.azimuth = plugin.azimuth;
    panner.elevation = plugin.elevation;
    panner.diverge = plugin.diverge;
    panner.gain = plugin.gain;
    panner.stereoOrbitAzimuth = plugin.stOrbitAzimuth;
    panner.stereoSpread = plugin.stSpread;
    
    // Visual
    panner.color = plugin.color;
    panner.state = plugin.state;
    
    // Modes
    panner.inputMode = plugin.inputMode;
    panner.pannerMode = plugin.pannerMode;
    panner.autoOrbit = plugin.autoOrbit;
    
    return panner;
}

// =============================================================================
// STATISTICS
// =============================================================================

PannerTrackingManager::TrackingStats PannerTrackingManager::getTrackingStats() const {
    TrackingStats stats;
    
    if (memoryShareTracker) {
        auto memStats = memoryShareTracker->getStats();
        stats.memorySharePanners = memStats.activePanners;
        stats.memoryShareAvailable = memStats.scannerActive;
    }
    
    if (oscTracker) {
        auto oscStats = oscTracker->getStats();
        stats.oscPanners = oscStats.activePanners;
        stats.oscAvailable = oscStats.pluginManagerAvailable;
    }
    
    stats.totalPanners = static_cast<uint32_t>(activePanners.size());
    stats.lastScanTime = lastScanTime;
    
    return stats;
}

// =============================================================================
// CLEANUP
// =============================================================================

void PannerTrackingManager::cleanupInactivePanners() {
    const juce::ScopedLock lock(pannersMutex);
    
    auto currentTime = juce::Time::currentTimeMillis();
    
    activePanners.erase(
        std::remove_if(activePanners.begin(), activePanners.end(), 
            [currentTime](const PannerInfo& panner) {
                return (currentTime - panner.lastUpdateTime) > PANNER_TIMEOUT_MS;
            }),
        activePanners.end()
    );
}

// =============================================================================
// EVENT PUBLISHING
// =============================================================================

void PannerTrackingManager::publishPannerAdded(const PannerInfo& panner) {
    if (eventSystem) {
        eventSystem->publish("PannerAdded", static_cast<int>(panner.processId));
    }
}

void PannerTrackingManager::publishPannerUpdated(const PannerInfo& panner) {
    if (eventSystem) {
        eventSystem->publish("PannerUpdated", static_cast<int>(panner.processId));
    }
}

void PannerTrackingManager::publishPannerRemoved(const PannerInfo& panner) {
    if (eventSystem) {
        eventSystem->publish("PannerRemoved", static_cast<int>(panner.processId));
    }
}

void PannerTrackingManager::publishTrackingMethodChanged(bool memoryShare, bool osc) {
    if (eventSystem) {
        juce::var method;
        if (memoryShare && osc) {
            method = "MemoryShare+OSC";
        } else if (memoryShare) {
            method = "MemoryShare";
        } else if (osc) {
            method = "OSC";
        } else {
            method = "None";
        }
        
        eventSystem->publish("TrackingMethodChanged", method);
    }
}

} // namespace Mach1 
