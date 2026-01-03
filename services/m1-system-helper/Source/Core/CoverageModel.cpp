/*
    CoverageModel.cpp
    -----------------
    Implementation of the coverage interval tracking system.
*/

#include "CoverageModel.h"
#include <algorithm>
#include <numeric>

namespace Mach1 {

//==============================================================================
// CapturedIntervalSet Implementation
//==============================================================================

void CapturedIntervalSet::addInterval(int64_t start, int64_t end)
{
    if (end <= start) return;  // Invalid interval
    
    m_intervals.push_back(SampleInterval(start, end));
    mergeOverlapping();
}

int64_t CapturedIntervalSet::getTotalCapturedSamples() const
{
    int64_t total = 0;
    for (const auto& interval : m_intervals)
    {
        total += interval.length();
    }
    return total;
}

bool CapturedIntervalSet::isCovered(int64_t sample) const
{
    for (const auto& interval : m_intervals)
    {
        if (interval.contains(sample))
            return true;
    }
    return false;
}

SampleInterval CapturedIntervalSet::getBoundingInterval() const
{
    if (m_intervals.empty())
        return SampleInterval(0, 0);
    
    int64_t minStart = m_intervals.front().start;
    int64_t maxEnd = m_intervals.back().end;
    
    for (const auto& interval : m_intervals)
    {
        minStart = std::min(minStart, interval.start);
        maxEnd = std::max(maxEnd, interval.end);
    }
    
    return SampleInterval(minStart, maxEnd);
}

std::vector<SampleInterval> CapturedIntervalSet::getGaps() const
{
    std::vector<SampleInterval> gaps;
    
    if (m_intervals.size() < 2)
        return gaps;
    
    for (size_t i = 0; i < m_intervals.size() - 1; ++i)
    {
        int64_t gapStart = m_intervals[i].end;
        int64_t gapEnd = m_intervals[i + 1].start;
        
        if (gapEnd > gapStart)
        {
            gaps.push_back(SampleInterval(gapStart, gapEnd));
        }
    }
    
    return gaps;
}

void CapturedIntervalSet::clear()
{
    m_intervals.clear();
}

void CapturedIntervalSet::mergeOverlapping()
{
    if (m_intervals.size() < 2)
        return;
    
    // Sort by start position
    std::sort(m_intervals.begin(), m_intervals.end());
    
    std::vector<SampleInterval> merged;
    merged.push_back(m_intervals[0]);
    
    for (size_t i = 1; i < m_intervals.size(); ++i)
    {
        SampleInterval& last = merged.back();
        const SampleInterval& current = m_intervals[i];
        
        if (last.canMerge(current))
        {
            last = last.merge(current);
        }
        else
        {
            merged.push_back(current);
        }
    }
    
    m_intervals = std::move(merged);
}

//==============================================================================
// PannerCoverage Implementation
//==============================================================================

float PannerCoverage::getCoveragePercent() const
{
    auto bounds = capturedIntervals.getBoundingInterval();
    if (bounds.isEmpty())
        return 0.0f;
    
    return 100.0f * capturedIntervals.getTotalCapturedSamples() / bounds.length();
}

double PannerCoverage::getCapturedDurationSeconds() const
{
    if (sampleRate == 0)
        return 0.0;
    
    return static_cast<double>(capturedIntervals.getTotalCapturedSamples()) / sampleRate;
}

double PannerCoverage::getDropoutDurationSeconds() const
{
    if (sampleRate == 0 || dropouts.empty())
        return 0.0;
    
    int64_t totalDropoutSamples = 0;
    for (const auto& dropout : dropouts)
    {
        totalDropoutSamples += dropout.length();
    }
    
    return static_cast<double>(totalDropoutSamples) / sampleRate;
}

//==============================================================================
// CoverageModel Implementation
//==============================================================================

CoverageModel::CoverageModel()
{
}

CoverageModel::~CoverageModel()
{
}

void CoverageModel::addPannerInterval(const PannerId& pannerId, int64_t startSample, int64_t numSamples,
                                      uint32_t sampleRate, uint32_t channels, uint32_t sequenceNumber, uint64_t bufferId)
{
    if (!pannerId.isValid() || numSamples <= 0)
        return;
    
    int64_t endSample = startSample + numSamples;
    
    {
        const juce::ScopedLock lock(m_mutex);
        
        std::string key = pannerId.toString();
        
        // Get or create panner coverage
        auto& coverage = m_pannerCoverages[key];
        if (!coverage.pannerId.isValid())
        {
            coverage.pannerId = pannerId;
        }
        
        // Detect dropout (gap in sequence or sample position)
        if (coverage.lastBufferId > 0 && coverage.lastEndSample > 0)
        {
            // Check for sequence gap
            if (sequenceNumber > coverage.lastSequenceNumber + 1)
            {
                uint32_t missed = sequenceNumber - coverage.lastSequenceNumber - 1;
                // Estimate dropout region based on expected vs actual position
                int64_t expectedStart = coverage.lastEndSample;
                if (startSample > expectedStart)
                {
                    coverage.dropouts.push_back(DropoutInterval(
                        expectedStart, startSample,
                        juce::Time::currentTimeMillis(),
                        missed, true
                    ));
                    coverage.totalDropoutsDetected += missed;
                }
            }
            // Check for sample position gap (without sequence gap - could be DAW seeking)
            else if (startSample > coverage.lastEndSample + numSamples)
            {
                // Significant gap - record as potential dropout
                // (Could also be DAW timeline jump, but we track it anyway)
            }
        }
        
        // Add the interval
        coverage.capturedIntervals.addInterval(startSample, endSample);
        
        // Update tracking
        coverage.sampleRate = sampleRate;
        coverage.channels = channels;
        coverage.lastSequenceNumber = sequenceNumber;
        coverage.lastBufferId = bufferId;
        coverage.lastEndSample = endSample;
        coverage.totalBlocksReceived++;
        
        // Update global sample rate
        m_globalSampleRate.store(sampleRate);
    }
    
    // Update global range (outside lock for atomics)
    updateGlobalRange(startSample, endSample);
    
    // Update latest position
    int64_t currentLatest = m_latestSamplePosition.load();
    while (endSample > currentLatest && !m_latestSamplePosition.compare_exchange_weak(currentLatest, endSample))
    {
        // CAS loop
    }
}

void CoverageModel::addDropout(const PannerId& pannerId, int64_t startSample, int64_t endSample,
                               uint32_t missedBufferCount, bool boundsKnown)
{
    if (!pannerId.isValid())
        return;
    
    const juce::ScopedLock lock(m_mutex);
    
    std::string key = pannerId.toString();
    auto it = m_pannerCoverages.find(key);
    
    if (it != m_pannerCoverages.end())
    {
        it->second.dropouts.push_back(DropoutInterval(
            startSample, endSample,
            juce::Time::currentTimeMillis(),
            missedBufferCount, boundsKnown
        ));
        it->second.totalDropoutsDetected++;
    }
}

void CoverageModel::removePanner(const PannerId& pannerId)
{
    const juce::ScopedLock lock(m_mutex);
    m_pannerCoverages.erase(pannerId.toString());
}

const PannerCoverage* CoverageModel::getPannerCoverage(const PannerId& pannerId) const
{
    const juce::ScopedLock lock(m_mutex);
    
    auto it = m_pannerCoverages.find(pannerId.toString());
    if (it != m_pannerCoverages.end())
    {
        return &it->second;
    }
    return nullptr;
}

std::vector<PannerId> CoverageModel::getPannerIds() const
{
    const juce::ScopedLock lock(m_mutex);
    
    std::vector<PannerId> ids;
    for (const auto& pair : m_pannerCoverages)
    {
        ids.push_back(pair.second.pannerId);
    }
    return ids;
}

SampleInterval CoverageModel::getGlobalRange() const
{
    if (m_rangeLocked)
    {
        return SampleInterval(m_lockedStartSample, m_lockedEndSample);
    }
    
    int64_t start = m_globalStartSample.load();
    int64_t end = m_globalEndSample.load();
    
    if (start == INT64_MAX || end == INT64_MIN || start >= end)
    {
        return SampleInterval(0, 0);
    }
    
    return SampleInterval(start, end);
}

void CoverageModel::setGlobalRange(int64_t start, int64_t end)
{
    if (m_rangeLocked)
    {
        m_lockedStartSample = start;
        m_lockedEndSample = end;
    }
}

CapturedIntervalSet CoverageModel::getAnyCoverage() const
{
    const juce::ScopedLock lock(m_mutex);
    
    CapturedIntervalSet combined;
    
    for (const auto& pair : m_pannerCoverages)
    {
        for (const auto& interval : pair.second.capturedIntervals.getIntervals())
        {
            combined.addInterval(interval);
        }
    }
    
    return combined;
}

CapturedIntervalSet CoverageModel::getAllCoverage() const
{
    const juce::ScopedLock lock(m_mutex);
    
    if (m_pannerCoverages.empty())
        return CapturedIntervalSet();
    
    // Start with the first panner's intervals
    auto it = m_pannerCoverages.begin();
    std::vector<SampleInterval> result = it->second.capturedIntervals.getIntervals();
    ++it;
    
    // Intersect with each subsequent panner
    while (it != m_pannerCoverages.end())
    {
        const auto& otherIntervals = it->second.capturedIntervals.getIntervals();
        std::vector<SampleInterval> intersection;
        
        for (const auto& r : result)
        {
            for (const auto& o : otherIntervals)
            {
                // Calculate intersection
                int64_t iStart = std::max(r.start, o.start);
                int64_t iEnd = std::min(r.end, o.end);
                
                if (iStart < iEnd)
                {
                    intersection.push_back(SampleInterval(iStart, iEnd));
                }
            }
        }
        
        result = std::move(intersection);
        ++it;
    }
    
    CapturedIntervalSet allCoverage;
    for (const auto& interval : result)
    {
        allCoverage.addInterval(interval);
    }
    
    return allCoverage;
}

std::vector<SampleInterval> CoverageModel::getAnyDropouts() const
{
    const juce::ScopedLock lock(m_mutex);
    
    CapturedIntervalSet dropoutSet;
    
    for (const auto& pair : m_pannerCoverages)
    {
        for (const auto& dropout : pair.second.dropouts)
        {
            dropoutSet.addInterval(dropout.startSample, dropout.endSample);
        }
    }
    
    return dropoutSet.getIntervals();
}

std::vector<SampleInterval> CoverageModel::getAllDropouts() const
{
    const juce::ScopedLock lock(m_mutex);
    
    if (m_pannerCoverages.empty())
        return {};
    
    // Find regions where ALL panners have gaps
    auto globalRange = getGlobalRange();
    if (globalRange.isEmpty())
        return {};
    
    // For each position in the global range, check if ALL panners are missing
    // This is expensive for large ranges, so we'll use interval arithmetic
    
    // Get the union of all coverage (any panner)
    auto anyCoverage = getAnyCoverage();
    
    // Get gaps in the any-coverage - these are total dropouts
    return anyCoverage.getGaps();
}

CoverageModel::GlobalStats CoverageModel::getGlobalStats() const
{
    GlobalStats stats;
    
    auto range = getGlobalRange();
    stats.globalStartSample = range.start;
    stats.globalEndSample = range.end;
    stats.totalRangeSamples = range.length();
    
    {
        const juce::ScopedLock lock(m_mutex);
        
        stats.pannerCount = static_cast<uint32_t>(m_pannerCoverages.size());
        
        for (const auto& pair : m_pannerCoverages)
        {
            stats.totalBlocksReceived += pair.second.totalBlocksReceived;
            stats.totalDropoutsDetected += pair.second.totalDropoutsDetected;
        }
    }
    
    // Calculate coverage stats
    auto anyCoverage = getAnyCoverage();
    stats.totalCapturedSamples = anyCoverage.getTotalCapturedSamples();
    
    auto allCoverage = getAllCoverage();
    stats.fullCoverageSamples = allCoverage.getTotalCapturedSamples();
    
    stats.partialDropoutSamples = stats.totalCapturedSamples - stats.fullCoverageSamples;
    stats.totalDropoutSamples = stats.totalRangeSamples - stats.totalCapturedSamples;
    
    return stats;
}

void CoverageModel::reset()
{
    const juce::ScopedLock lock(m_mutex);
    
    m_pannerCoverages.clear();
    m_globalStartSample.store(INT64_MAX);
    m_globalEndSample.store(INT64_MIN);
    m_latestSamplePosition.store(0);
    m_rangeLocked = false;
    m_lockedStartSample = 0;
    m_lockedEndSample = 0;
}

void CoverageModel::updateGlobalRange(int64_t startSample, int64_t endSample)
{
    if (m_rangeLocked)
        return;
    
    // Update global start (atomic CAS)
    int64_t currentStart = m_globalStartSample.load();
    while (startSample < currentStart && !m_globalStartSample.compare_exchange_weak(currentStart, startSample))
    {
        // CAS loop
    }
    
    // Update global end (atomic CAS)
    int64_t currentEnd = m_globalEndSample.load();
    while (endSample > currentEnd && !m_globalEndSample.compare_exchange_weak(currentEnd, endSample))
    {
        // CAS loop
    }
}

} // namespace Mach1

