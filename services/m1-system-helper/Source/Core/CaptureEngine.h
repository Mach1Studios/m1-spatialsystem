/*
    CaptureEngine.h
    ---------------
    Background thread capture engine that ingests audio blocks from panners,
    associates state snapshots, and writes to disk in an append-only chunk format.
    
    Features:
    - Runs on a background thread (no blocking of message thread)
    - Reads from M1MemoryShare per-panner connections
    - Writes append-only binary chunk files per panner
    - Maintains coverage model for UI visualization
    - Detects dropouts via sequence number gaps or ring buffer overruns
    
    Storage Format (per panner):
    - Folder: <capture_root>/<session_id>/<panner_uuid>/
    - File: chunks.bin (append-only)
    - Each chunk: ChunkHeader + StateSnapshot + audio data
*/

#pragma once

#include <JuceHeader.h>
#include "CoverageModel.h"
#include "../Managers/PannerTrackingManager.h"
#include "../Common/TypesForDataExchange.h"
#include <atomic>
#include <map>
#include <memory>
#include <functional>

namespace Mach1 {

//==============================================================================
/**
 * State snapshot captured alongside audio data
 */
struct StateSnapshot
{
    // Spatial parameters
    float azimuthDeg = 0.0f;
    float elevationDeg = 0.0f;
    float diverge = 0.0f;
    float gainDb = 0.0f;
    
    // Stereo parameters
    float stereoOrbitAzimuth = 0.0f;
    float stereoSpread = 0.0f;
    float stereoInputBalance = 0.0f;
    bool autoOrbit = false;
    
    // Mode settings
    int32_t inputMode = 0;
    int32_t outputMode = 0;
    int32_t pannerMode = 0;
    
    // Sequence tracking
    uint32_t stateSeq = 0;
    uint64_t captureTimestampMs = 0;
    
    // Padding for alignment and future expansion
    uint8_t reserved[24] = {0};
    
    static constexpr size_t SIZE = 80;  // Fixed size for binary storage
};
static_assert(sizeof(StateSnapshot) == StateSnapshot::SIZE, "StateSnapshot size mismatch");

//==============================================================================
/**
 * Header for each captured chunk in the binary file
 */
struct ChunkHeader
{
    // Magic number for validation
    uint32_t magic = 0x4D314348;  // "M1CH"
    uint32_t version = 1;
    
    // Audio metadata
    int64_t startSample = 0;
    int32_t numSamples = 0;
    int16_t numChannels = 0;
    int16_t reserved1 = 0;
    uint32_t sampleRate = 44100;
    
    // Tracking
    uint64_t bufferId = 0;
    uint32_t sequenceNumber = 0;
    uint64_t dawTimestampMs = 0;
    uint64_t wallClockMs = 0;
    
    // Data sizes
    uint32_t stateSize = StateSnapshot::SIZE;
    uint32_t audioDataSize = 0;  // numChannels * numSamples * sizeof(float)
    
    // Padding for alignment
    uint8_t reserved2[8] = {0};
    
    static constexpr size_t SIZE = 80;  // Fixed size
};
static_assert(sizeof(ChunkHeader) == ChunkHeader::SIZE, "ChunkHeader size mismatch");

//==============================================================================
/**
 * Per-panner capture state
 */
struct PannerCaptureState
{
    PannerId pannerId;
    juce::File chunkFile;
    std::unique_ptr<juce::FileOutputStream> outputStream;
    
    // Ring buffer tracking for dropout detection
    uint32_t lastSequenceNumber = 0;
    uint64_t lastBufferId = 0;
    int64_t lastEndSample = 0;
    
    // Statistics
    uint32_t chunksWritten = 0;
    uint64_t bytesWritten = 0;
    
    bool isOpen() const { return outputStream != nullptr && outputStream->openedOk(); }
};

//==============================================================================
/**
 * Background capture engine
 */
class CaptureEngine : public juce::Thread,
                      public juce::ChangeBroadcaster
{
public:
    CaptureEngine(PannerTrackingManager& pannerManager);
    ~CaptureEngine() override;
    
    //==========================================================================
    // Control
    
    /**
     * Start capturing
     * @param sessionId Unique session identifier (e.g., project name + timestamp)
     * @param captureRoot Root directory for capture storage
     */
    bool startCapture(const juce::String& sessionId, const juce::File& captureRoot);
    
    /**
     * Stop capturing
     */
    void stopCapture();
    
    /**
     * Check if capturing is active
     */
    bool isCapturing() const { return m_capturing.load(); }
    
    /**
     * Get the current session ID
     */
    juce::String getSessionId() const { return m_sessionId; }
    
    /**
     * Get the capture root directory
     */
    juce::File getCaptureRoot() const { return m_captureRoot; }
    
    //==========================================================================
    // Coverage model access
    
    /**
     * Get the coverage model (thread-safe)
     */
    CoverageModel& getCoverageModel() { return m_coverageModel; }
    const CoverageModel& getCoverageModel() const { return m_coverageModel; }
    
    //==========================================================================
    // Statistics
    
    struct CaptureStats
    {
        juce::String sessionId;
        uint32_t activePanners = 0;
        uint32_t totalChunksWritten = 0;
        uint64_t totalBytesWritten = 0;
        uint32_t totalDropoutsDetected = 0;
        double capturedDurationSeconds = 0.0;
        juce::Time startTime;
        juce::Time lastUpdateTime;
    };
    
    CaptureStats getStats() const;
    
    //==========================================================================
    // Debug mode
    
    /**
     * Enable/disable debug fake block generation
     * (for testing without real panner plugins)
     */
    void setDebugFakeBlocks(bool enabled) { m_debugFakeBlocks = enabled; }
    bool isDebugFakeBlocksEnabled() const { return m_debugFakeBlocks; }
    
    //==========================================================================
    // Reset
    
    /**
     * Reset coverage model (keeps capture running)
     */
    void resetCoverage();
    
protected:
    // Thread override
    void run() override;
    
private:
    PannerTrackingManager& m_pannerManager;
    CoverageModel m_coverageModel;
    
    // Capture state
    std::atomic<bool> m_capturing{false};
    juce::String m_sessionId;
    juce::File m_captureRoot;
    juce::Time m_startTime;
    
    // Per-panner capture state
    juce::CriticalSection m_stateMutex;
    std::map<std::string, PannerCaptureState> m_pannerStates;
    
    // Statistics
    std::atomic<uint32_t> m_totalChunksWritten{0};
    std::atomic<uint64_t> m_totalBytesWritten{0};
    std::atomic<uint32_t> m_totalDropoutsDetected{0};
    
    // Debug mode
    bool m_debugFakeBlocks = false;
    int64_t m_debugSamplePosition = 0;
    uint32_t m_debugSequenceNumber = 0;
    
    // Processing
    void processCapture();
    void processPannerData(const PannerInfo& panner);
    void writeChunk(PannerCaptureState& state, const ChunkHeader& header,
                   const StateSnapshot& snapshot, const float* audioData);
    
    // Panner state management
    PannerCaptureState& getOrCreatePannerState(const PannerId& pannerId);
    void closePannerState(PannerCaptureState& state);
    void closeAllPannerStates();
    
    // Helpers
    PannerId createPannerId(const PannerInfo& panner) const;
    StateSnapshot createStateSnapshot(const PannerInfo& panner) const;
    juce::File getPannerCaptureDir(const PannerId& pannerId) const;
    
    // Debug fake data generation
    void generateDebugFakeBlocks();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CaptureEngine)
};

} // namespace Mach1

