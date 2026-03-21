/*
    CoverageModel.h
    ---------------
    Tracks captured sample intervals per panner and globally.
    Supports interval merging, dropout detection, and coverage statistics.
    
    Design:
    - CapturedIntervalSet: stores union of [start, end) sample intervals with auto-merge
    - DropoutInterval: represents a known dropout (ring buffer overrun)
    - PannerCoverage: per-panner coverage data
    - GlobalCoverage: aggregated view across all panners
*/

#pragma once

#include <JuceHeader.h>
#include <vector>
#include <map>
#include <set>
#include <string>
#include <mutex>
#include <atomic>
#include <algorithm>

namespace Mach1 {

//==============================================================================
/**
 * Represents a sample interval [start, end)
 */
struct SampleInterval
{
    int64_t start = 0;  // Inclusive
    int64_t end = 0;    // Exclusive
    
    SampleInterval() = default;
    SampleInterval(int64_t s, int64_t e) : start(s), end(e) {}
    
    int64_t length() const { return end - start; }
    bool isEmpty() const { return end <= start; }
    bool contains(int64_t sample) const { return sample >= start && sample < end; }
    bool overlaps(const SampleInterval& other) const {
        return start < other.end && other.start < end;
    }
    bool adjacentTo(const SampleInterval& other) const {
        return end == other.start || other.end == start;
    }
    bool canMerge(const SampleInterval& other) const {
        return overlaps(other) || adjacentTo(other);
    }
    
    SampleInterval merge(const SampleInterval& other) const {
        return SampleInterval(std::min(start, other.start), std::max(end, other.end));
    }
    
    bool operator<(const SampleInterval& other) const {
        return start < other.start || (start == other.start && end < other.end);
    }
    
    bool operator==(const SampleInterval& other) const {
        return start == other.start && end == other.end;
    }
};

//==============================================================================
/**
 * A set of sample intervals with automatic merging of overlapping/adjacent intervals.
 * Maintains a sorted, non-overlapping list of intervals.
 */
class CapturedIntervalSet
{
public:
    CapturedIntervalSet() = default;
    
    /**
     * Add an interval [start, end) to the set, merging with existing intervals as needed.
     */
    void addInterval(int64_t start, int64_t end);
    void addInterval(const SampleInterval& interval) { addInterval(interval.start, interval.end); }
    
    /**
     * Get all intervals (sorted, non-overlapping)
     */
    const std::vector<SampleInterval>& getIntervals() const { return m_intervals; }
    
    /**
     * Get total captured samples (sum of all interval lengths)
     */
    int64_t getTotalCapturedSamples() const;
    
    /**
     * Check if a sample position is covered
     */
    bool isCovered(int64_t sample) const;
    
    /**
     * Get the range covered by all intervals
     */
    SampleInterval getBoundingInterval() const;
    
    /**
     * Get gaps (uncovered intervals) within the bounding range
     */
    std::vector<SampleInterval> getGaps() const;
    
    /**
     * Clear all intervals
     */
    void clear();
    
    /**
     * Get number of discrete intervals
     */
    size_t getIntervalCount() const { return m_intervals.size(); }
    
private:
    std::vector<SampleInterval> m_intervals;  // Sorted, non-overlapping
    
    void mergeOverlapping();
};

//==============================================================================
/**
 * Represents a detected dropout (ring buffer overrun)
 */
struct DropoutInterval
{
    int64_t startSample = 0;      // Start of dropout (estimated)
    int64_t endSample = 0;        // End of dropout (estimated)
    uint64_t detectedAtMs = 0;    // Wall clock time when detected
    uint32_t missedBufferCount = 0; // Number of buffers missed (if known)
    bool boundsKnown = false;     // True if start/end are accurate
    
    DropoutInterval() = default;
    DropoutInterval(int64_t start, int64_t end, uint64_t detectedAt, uint32_t missed = 0, bool known = true)
        : startSample(start), endSample(end), detectedAtMs(detectedAt), missedBufferCount(missed), boundsKnown(known) {}
    
    int64_t length() const { return endSample - startSample; }
};

//==============================================================================
/**
 * Unique identifier for a panner instance
 */
struct PannerId
{
    std::string sessionId;       // Session/project identifier
    std::string instanceUuid;    // Unique instance ID (from memory segment name)
    uint32_t processId = 0;      // Process ID
    
    PannerId() = default;
    PannerId(const std::string& session, const std::string& uuid, uint32_t pid = 0)
        : sessionId(session), instanceUuid(uuid), processId(pid) {}
    
    std::string toString() const {
        return sessionId + "_" + instanceUuid + "_" + std::to_string(processId);
    }
    
    bool operator<(const PannerId& other) const {
        return toString() < other.toString();
    }
    
    bool operator==(const PannerId& other) const {
        return sessionId == other.sessionId && instanceUuid == other.instanceUuid && processId == other.processId;
    }
    
    bool isValid() const { return !instanceUuid.empty(); }
};

//==============================================================================
/**
 * Per-panner coverage tracking
 */
struct PannerCoverage
{
    PannerId pannerId;
    CapturedIntervalSet capturedIntervals;
    std::vector<DropoutInterval> dropouts;
    
    // Audio format info
    uint32_t sampleRate = 44100;
    uint32_t channels = 1;
    
    // State tracking
    uint32_t lastSequenceNumber = 0;
    uint64_t lastBufferId = 0;
    int64_t lastEndSample = 0;
    
    // Statistics
    uint32_t totalBlocksReceived = 0;
    uint32_t totalDropoutsDetected = 0;
    
    // Get coverage percentage within the bounding interval
    float getCoveragePercent() const;
    
    // Get total captured duration in seconds
    double getCapturedDurationSeconds() const;
    
    // Get total dropout duration in seconds
    double getDropoutDurationSeconds() const;
};

//==============================================================================
/**
 * Global coverage model - aggregates data from all panners
 */
class CoverageModel
{
public:
    CoverageModel();
    ~CoverageModel();
    
    //==========================================================================
    // Panner management
    
    /**
     * Add or update coverage for a panner
     */
    void addPannerInterval(const PannerId& pannerId, int64_t startSample, int64_t numSamples,
                          uint32_t sampleRate, uint32_t channels, uint32_t sequenceNumber, uint64_t bufferId);
    
    /**
     * Record a dropout for a panner
     */
    void addDropout(const PannerId& pannerId, int64_t startSample, int64_t endSample,
                   uint32_t missedBufferCount = 0, bool boundsKnown = true);
    
    /**
     * Remove a panner (when it disconnects)
     */
    void removePanner(const PannerId& pannerId);
    
    /**
     * Get coverage data for a specific panner
     */
    const PannerCoverage* getPannerCoverage(const PannerId& pannerId) const;
    
    /**
     * Get all panner coverages
     */
    std::vector<PannerId> getPannerIds() const;
    
    //==========================================================================
    // Global range tracking
    
    /**
     * Get the global sample range (earliest to latest across all panners)
     */
    SampleInterval getGlobalRange() const;
    
    /**
     * Lock/unlock the global range (prevents automatic expansion)
     */
    void setRangeLocked(bool locked) { m_rangeLocked = locked; }
    bool isRangeLocked() const { return m_rangeLocked; }
    
    /**
     * Manually set the global range (only works when locked)
     */
    void setGlobalRange(int64_t start, int64_t end);
    
    //==========================================================================
    // Global coverage calculation
    
    /**
     * Get intervals where at least one panner has coverage
     */
    CapturedIntervalSet getAnyCoverage() const;
    
    /**
     * Get intervals where ALL panners have coverage
     */
    CapturedIntervalSet getAllCoverage() const;
    
    /**
     * Get intervals where at least one panner has a dropout
     */
    std::vector<SampleInterval> getAnyDropouts() const;
    
    /**
     * Get intervals where ALL panners have dropouts (total loss)
     */
    std::vector<SampleInterval> getAllDropouts() const;
    
    //==========================================================================
    // Statistics
    
    struct GlobalStats
    {
        int64_t globalStartSample = 0;
        int64_t globalEndSample = 0;
        int64_t totalRangeSamples = 0;
        int64_t totalCapturedSamples = 0;  // Any panner coverage
        int64_t fullCoverageSamples = 0;   // All panners coverage
        int64_t partialDropoutSamples = 0; // Some panners missing
        int64_t totalDropoutSamples = 0;   // All panners missing
        uint32_t pannerCount = 0;
        uint32_t totalBlocksReceived = 0;
        uint32_t totalDropoutsDetected = 0;
        
        float anyCoveragePercent() const {
            return totalRangeSamples > 0 ? (100.0f * totalCapturedSamples / totalRangeSamples) : 0.0f;
        }
        float fullCoveragePercent() const {
            return totalRangeSamples > 0 ? (100.0f * fullCoverageSamples / totalRangeSamples) : 0.0f;
        }
    };
    
    GlobalStats getGlobalStats() const;
    
    /**
     * Get latest sample position (playhead)
     */
    int64_t getLatestSamplePosition() const { return m_latestSamplePosition.load(); }
    
    /**
     * Get sample rate (uses first panner's rate)
     */
    uint32_t getSampleRate() const { return m_globalSampleRate.load(); }
    
    //==========================================================================
    // Reset
    
    /**
     * Clear all coverage data
     */
    void reset();
    
private:
    mutable juce::CriticalSection m_mutex;
    std::map<std::string, PannerCoverage> m_pannerCoverages;  // key = PannerId::toString()
    
    std::atomic<int64_t> m_globalStartSample{INT64_MAX};
    std::atomic<int64_t> m_globalEndSample{INT64_MIN};
    std::atomic<int64_t> m_latestSamplePosition{0};
    std::atomic<uint32_t> m_globalSampleRate{44100};
    
    bool m_rangeLocked = false;
    int64_t m_lockedStartSample = 0;
    int64_t m_lockedEndSample = 0;
    
    void updateGlobalRange(int64_t startSample, int64_t endSample);
};

} // namespace Mach1

