/*
    M1MemoryShareTracker.h
    ----------------------
    Tracks M1-Panner plugin instances using M1MemoryShare (direct shared memory access).
    
    Logic Flow:
    1. Scan filesystem for M1MemoryShare files created by M1-Panner plugins
    2. Connect to each found memory segment as a consumer
    3. Extract panner parameters and audio data from shared memory
    4. Provide real-time updates with buffer acknowledgment
    
    This provides high-performance, low-latency tracking compared to OSC.
*/

#pragma once

#include "../Common/Common.h"
#include "../Common/M1MemoryShare.h"
#include "../Common/TypesForDataExchange.h"
#include <vector>
#include <memory>
#include <string>

namespace Mach1 {

/**
 * Information about a panner discovered via M1MemoryShare
 */
struct MemorySharePannerInfo {
    // Identity (extracted from memory segment name)
    std::string name;
    uint32_t processId = 0;
    uintptr_t memoryAddress = 0;
    uint64_t creationTimestamp = 0;
    
    // Connection
    std::string memorySegmentName;
    std::unique_ptr<M1MemoryShare> memoryShare;
    bool isConnected = false;
    
    // Audio format
    uint32_t sampleRate = 44100;
    uint32_t channels = 1;
    uint32_t samplesPerBlock = 512;
    
    // Latest audio data
    uint64_t currentBufferId = 0;
    uint32_t sequenceNumber = 0;
    uint64_t dawTimestamp = 0;
    double playheadPositionInSeconds = 0.0;
    bool isPlaying = false;
    
    // Panner parameters from memory
    ParameterMap parameters;
    
    // Buffer tracking
    uint32_t queuedBufferCount = 0;
    uint32_t consumerCount = 0;
    
    // Status
    bool isActive = false;
    juce::int64 lastUpdateTime = 0;
    
    // Default constructor
    MemorySharePannerInfo() = default;
    
    // Move constructor
    MemorySharePannerInfo(MemorySharePannerInfo&& other) noexcept = default;
    
    // Move assignment operator
    MemorySharePannerInfo& operator=(MemorySharePannerInfo&& other) noexcept = default;
    
    // Delete copy constructor and copy assignment operator
    MemorySharePannerInfo(const MemorySharePannerInfo&) = delete;
    MemorySharePannerInfo& operator=(const MemorySharePannerInfo&) = delete;
    
    // Extract commonly used parameters
    float getAzimuth() const;
    float getElevation() const;
    float getDiverge() const;
    float getGain() const;
    float getStereoOrbitAzimuth() const;
    float getStereoSpread() const;
    float getStereoInputBalance() const;
    bool getAutoOrbit() const;
    int getInputMode() const;
    int getOutputMode() const;
    int getState() const;
    
    bool operator==(const MemorySharePannerInfo& other) const {
        return processId == other.processId && memoryAddress == other.memoryAddress;
    }
};

/**
 * M1MemoryShare-based panner tracker
 * Discovers and tracks M1-Panner instances through shared memory segments
 */
class M1MemoryShareTracker {
public:
    explicit M1MemoryShareTracker(uint32_t consumerId = 9001);
    ~M1MemoryShareTracker();
    
    // Main interface
    void start();
    void stop();
    void update();  // Call regularly to scan for new panners and update existing ones
    
    // Panner discovery and access
    const std::vector<MemorySharePannerInfo>& getActivePanners() const;
    MemorySharePannerInfo* findPanner(uint32_t processId, uintptr_t memoryAddress = 0);
    bool hasPanners() const;
    bool isAvailable() const;
    
    // Consumer management
    bool registerAsConsumer(uint32_t consumerId);
    bool unregisterAsConsumer(uint32_t consumerId);
    
    // Statistics
    struct MemoryShareStats {
        uint32_t totalPanners = 0;
        uint32_t activePanners = 0;
        uint32_t connectedPanners = 0;
        juce::int64 lastScanTime = 0;
        std::vector<std::string> memorySegmentNames;
        bool scannerActive = false;
    };
    
    MemoryShareStats getStats() const;

private:
    // Core scanning logic
    void scanForMemorySegments();
    void updateExistingPanners();
    void cleanupInactivePanners();
    void cleanupStaleMemoryFiles();
    bool isProcessRunning(uint32_t processId);
    
    // Memory segment management
    bool connectToPanner(MemorySharePannerInfo& panner);
    void disconnectFromPanner(MemorySharePannerInfo& panner);
    bool updatePannerData(MemorySharePannerInfo& panner);
    
    // Memory segment discovery
    std::vector<std::string> findPannerMemorySegments();
    bool parsePannerSegmentName(const std::string& filename, 
                               std::string& name, 
                               uint32_t& processId, 
                               uintptr_t& memoryAddress, 
                               uint64_t& timestamp);
    
    // Parameter extraction
    void extractParametersFromBuffer(MemorySharePannerInfo& panner);
    bool readAudioBufferData(MemorySharePannerInfo& panner);
    
    // File system scanning
    std::vector<juce::File> getPannerMemoryDirectories();
    bool isValidPannerMemoryFile(const juce::File& filePath);
    
    // State
    std::vector<MemorySharePannerInfo> activePanners;
    mutable juce::CriticalSection pannersMutex;
    
    // Configuration
    uint32_t consumerId;
    bool isRunning = false;
    bool initialized = false;
    
    // Timing
    juce::int64 lastScanTime = 0;
    static constexpr int SCAN_INTERVAL_MS = 1000;    // Scan every 1 second
    static constexpr int UPDATE_INTERVAL_MS = 100;   // Update existing panners every 100ms
    static constexpr int PANNER_TIMEOUT_MS = 5000;   // Consider inactive after 5 seconds
    
    // Search paths for M1MemoryShare files
    static const std::vector<std::string> MEMORY_SEARCH_PATHS;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(M1MemoryShareTracker)
};

} // namespace Mach1 