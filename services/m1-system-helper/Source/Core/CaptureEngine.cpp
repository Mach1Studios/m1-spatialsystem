/*
    CaptureEngine.cpp
    -----------------
    Implementation of the background capture engine.
*/

#include "CaptureEngine.h"
#include <cstring>
#include <random>
#include <map>

namespace Mach1 {

//==============================================================================
CaptureEngine::CaptureEngine(PannerTrackingManager& pannerManager)
    : juce::Thread("CaptureEngine")
    , m_pannerManager(pannerManager)
{
}

CaptureEngine::~CaptureEngine()
{
    stopCapture();
}

//==============================================================================
bool CaptureEngine::startCapture(const juce::String& sessionId, const juce::File& captureRoot)
{
    if (m_capturing.load())
    {
        DBG("[CaptureEngine] Already capturing");
        return false;
    }
    
    // Validate inputs
    if (sessionId.isEmpty())
    {
        DBG("[CaptureEngine] Session ID cannot be empty");
        return false;
    }
    
    // Create capture root directory
    if (!captureRoot.exists())
    {
        if (!captureRoot.createDirectory())
        {
            DBG("[CaptureEngine] Failed to create capture root: " + captureRoot.getFullPathName());
            return false;
        }
    }
    
    // Create session directory
    juce::File sessionDir = captureRoot.getChildFile(sessionId);
    if (!sessionDir.exists())
    {
        if (!sessionDir.createDirectory())
        {
            DBG("[CaptureEngine] Failed to create session directory: " + sessionDir.getFullPathName());
            return false;
        }
    }
    
    m_sessionId = sessionId;
    m_captureRoot = captureRoot;
    m_startTime = juce::Time::getCurrentTime();
    m_capturing.store(true);
    
    // Reset statistics
    m_totalChunksWritten.store(0);
    m_totalBytesWritten.store(0);
    m_totalDropoutsDetected.store(0);
    
    // Reset coverage model
    m_coverageModel.reset();
    
    // Start background thread
    startThread(juce::Thread::Priority::normal);
    
    DBG("[CaptureEngine] Started capture - Session: " + sessionId + ", Root: " + captureRoot.getFullPathName());
    
    return true;
}

void CaptureEngine::stopCapture()
{
    if (!m_capturing.load())
        return;
    
    m_capturing.store(false);
    
    // Stop thread
    stopThread(2000);  // 2 second timeout
    
    // Close all panner states
    closeAllPannerStates();
    
    DBG("[CaptureEngine] Stopped capture");
    
    // Notify listeners
    sendChangeMessage();
}

CaptureEngine::CaptureStats CaptureEngine::getStats() const
{
    CaptureStats stats;
    stats.sessionId = m_sessionId;
    stats.startTime = m_startTime;
    stats.lastUpdateTime = juce::Time::getCurrentTime();
    
    {
        const juce::ScopedLock lock(m_stateMutex);
        stats.activePanners = static_cast<uint32_t>(m_pannerStates.size());
    }
    
    stats.totalChunksWritten = m_totalChunksWritten.load();
    stats.totalBytesWritten = m_totalBytesWritten.load();
    stats.totalDropoutsDetected = m_totalDropoutsDetected.load();
    
    // Calculate captured duration from coverage model
    auto globalStats = m_coverageModel.getGlobalStats();
    uint32_t sampleRate = m_coverageModel.getSampleRate();
    if (sampleRate > 0)
    {
        stats.capturedDurationSeconds = static_cast<double>(globalStats.totalCapturedSamples) / sampleRate;
    }
    
    return stats;
}

void CaptureEngine::resetCoverage()
{
    m_coverageModel.reset();
    sendChangeMessage();
}

//==============================================================================
void CaptureEngine::run()
{
    DBG("[CaptureEngine] Background thread started");
    
    while (!threadShouldExit() && m_capturing.load())
    {
        processCapture();
        
        // Sleep briefly to avoid busy-waiting
        // Adjust this based on expected block rate (e.g., 10ms for ~100 blocks/sec)
        Thread::sleep(5);
    }
    
    DBG("[CaptureEngine] Background thread exiting");
}

void CaptureEngine::processCapture()
{
    // Handle debug mode
    if (m_debugFakeBlocks)
    {
        generateDebugFakeBlocks();
        return;
    }
    
    // Get active panners from tracking manager
    auto panners = m_pannerManager.getActivePanners();
    
    // Periodic debug logging (every 5 seconds)
    static juce::int64 lastDebugTime = 0;
    auto now = juce::Time::currentTimeMillis();
    if (now - lastDebugTime > 5000)
    {
        lastDebugTime = now;
        DBG("[CaptureEngine] processCapture: " + juce::String(panners.size()) + " panners found");
        for (const auto& p : panners)
        {
            juce::String statusStr = "Unknown";
            switch (p.connectionStatus) {
                case PannerConnectionStatus::Active: statusStr = "Active"; break;
                case PannerConnectionStatus::Stale: statusStr = "Stale"; break;
                case PannerConnectionStatus::Disconnected: statusStr = "Disconnected"; break;
            }
            DBG("  - " + juce::String(p.name) + " [PID:" + juce::String(p.processId) + 
                "] status=" + statusStr + 
                " isPlaying=" + juce::String(p.isPlaying ? "yes" : "no") +
                " playhead=" + juce::String(p.playheadPositionInSeconds, 2) + "s" +
                " isMemShare=" + juce::String(p.isMemoryShareBased ? "yes" : "no"));
        }
    }
    
    for (const auto& panner : panners)
    {
        if (!panner.isMemoryShareBased)
            continue;  // Only capture from memory share panners
        
        processPannerData(panner);
    }
}

void CaptureEngine::processPannerData(const PannerInfo& panner)
{
    // Create panner ID
    PannerId pannerId = createPannerId(panner);
    
    // Get the memory share tracker to read audio data
    auto* tracker = m_pannerManager.getMemoryShareTracker();
    if (!tracker)
    {
        DBG("[CaptureEngine] No memory share tracker available");
        return;
    }
    
    // Find the panner in the tracker
    auto* memPanner = tracker->findPanner(panner.processId);
    if (!memPanner || !memPanner->memoryShare)
    {
        // Reduced logging - only log occasionally
        static std::map<uint32_t, juce::int64> lastLogTime;
        auto now = juce::Time::currentTimeMillis();
        if (now - lastLogTime[panner.processId] > 10000) // Every 10s per panner
        {
            lastLogTime[panner.processId] = now;
            DBG("[CaptureEngine] Panner not found in tracker: " + juce::String(panner.name) + 
                " PID=" + juce::String(panner.processId));
        }
        return;
    }
    
    // Read audio buffer with parameters
    juce::AudioBuffer<float> audioBuffer;
    ParameterMap parameters;
    uint64_t dawTimestamp = 0;
    double playheadPosition = 0.0;
    bool isPlaying = false;
    uint64_t bufferId = 0;
    uint32_t updateSource = 0;
    
    if (!memPanner->memoryShare->readAudioBufferWithGenericParameters(
            audioBuffer, parameters, dawTimestamp, playheadPosition, isPlaying, bufferId, updateSource))
    {
        return;  // No new data available
    }
    
    // Get or create panner state
    PannerCaptureState& state = getOrCreatePannerState(pannerId);
    
    // Skip if we've already processed this buffer
    if (bufferId == state.lastBufferId)
        return;
    
    // Debug logging for new buffer
    static std::map<std::string, juce::int64> lastBufferLogTime;
    auto now = juce::Time::currentTimeMillis();
    std::string key = pannerId.toString();
    if (now - lastBufferLogTime[key] > 2000) // Every 2s per panner
    {
        lastBufferLogTime[key] = now;
        DBG("[CaptureEngine] New buffer from " + juce::String(panner.name) + 
            ": bufferId=" + juce::String((juce::int64)bufferId) +
            " playhead=" + juce::String(playheadPosition, 3) + "s" +
            " isPlaying=" + juce::String(isPlaying ? "yes" : "no") +
            " channels=" + juce::String(audioBuffer.getNumChannels()) +
            " samples=" + juce::String(audioBuffer.getNumSamples()));
    }
    
    // Calculate start sample position
    uint32_t sampleRate = memPanner->sampleRate > 0 ? memPanner->sampleRate : 44100;
    int64_t startSample = static_cast<int64_t>(playheadPosition * sampleRate);
    int32_t numSamples = audioBuffer.getNumSamples();
    int16_t numChannels = static_cast<int16_t>(audioBuffer.getNumChannels());
    
    // Detect dropout (sequence gap)
    uint32_t sequenceNumber = memPanner->sequenceNumber;
    if (state.lastBufferId > 0 && sequenceNumber > state.lastSequenceNumber + 1)
    {
        uint32_t missed = sequenceNumber - state.lastSequenceNumber - 1;
        m_totalDropoutsDetected.fetch_add(missed);
        
        // Record dropout in coverage model
        int64_t dropoutStart = state.lastEndSample;
        int64_t dropoutEnd = startSample;
        if (dropoutEnd > dropoutStart)
        {
            m_coverageModel.addDropout(pannerId, dropoutStart, dropoutEnd, missed, true);
        }
    }
    
    // Skip if no audio data
    if (numChannels <= 0 || numSamples <= 0)
    {
        return;
    }
    
    // Create chunk header
    ChunkHeader header;
    header.startSample = startSample;
    header.numSamples = numSamples;
    header.numChannels = numChannels;
    header.sampleRate = sampleRate;
    header.bufferId = bufferId;
    header.sequenceNumber = sequenceNumber;
    header.dawTimestampMs = dawTimestamp;
    header.wallClockMs = static_cast<uint64_t>(juce::Time::currentTimeMillis());
    header.audioDataSize = static_cast<uint32_t>(numChannels * numSamples * sizeof(float));
    
    // Create state snapshot from panner info
    StateSnapshot snapshot = createStateSnapshot(panner);
    snapshot.captureTimestampMs = header.wallClockMs;
    snapshot.stateSeq = sequenceNumber;
    
    // Interleave audio data for storage
    std::vector<float> interleavedAudio(static_cast<size_t>(numChannels * numSamples));
    for (int sample = 0; sample < numSamples; ++sample)
    {
        for (int channel = 0; channel < numChannels; ++channel)
        {
            interleavedAudio[static_cast<size_t>(sample * numChannels + channel)] = audioBuffer.getSample(channel, sample);
        }
    }
    
    // Write chunk to disk
    writeChunk(state, header, snapshot, interleavedAudio.data());
    
    // Update coverage model
    m_coverageModel.addPannerInterval(pannerId, startSample, numSamples,
                                      sampleRate, numChannels, sequenceNumber, bufferId);
    
    // Update state tracking
    state.lastSequenceNumber = sequenceNumber;
    state.lastBufferId = bufferId;
    state.lastEndSample = startSample + numSamples;
    
    // Acknowledge the buffer
    memPanner->memoryShare->acknowledgeBuffer(bufferId, 9001);  // Consumer ID
}

void CaptureEngine::writeChunk(PannerCaptureState& state, const ChunkHeader& header,
                               const StateSnapshot& snapshot, const float* audioData)
{
    if (!state.isOpen())
        return;
    
    auto* stream = state.outputStream.get();
    
    // Write header
    stream->write(&header, ChunkHeader::SIZE);
    
    // Write state snapshot
    stream->write(&snapshot, StateSnapshot::SIZE);
    
    // Write audio data (only if there's data to write)
    if (audioData != nullptr && header.audioDataSize > 0)
    {
        stream->write(audioData, header.audioDataSize);
    }
    
    // Flush periodically (every 100 chunks)
    state.chunksWritten++;
    if (state.chunksWritten % 100 == 0)
    {
        stream->flush();
    }
    
    state.bytesWritten += ChunkHeader::SIZE + StateSnapshot::SIZE + header.audioDataSize;
    m_totalChunksWritten.fetch_add(1);
    m_totalBytesWritten.fetch_add(ChunkHeader::SIZE + StateSnapshot::SIZE + header.audioDataSize);
    
    // Notify listeners less frequently to avoid GUI stalls (every ~500ms worth of chunks)
    // At 48kHz with 512 sample blocks, that's about 47 blocks per 500ms
    if (state.chunksWritten % 100 == 0)
    {
        // Use async notification to avoid blocking capture thread
        juce::MessageManager::callAsync([this]() {
            sendChangeMessage();
        });
    }
}

//==============================================================================
PannerCaptureState& CaptureEngine::getOrCreatePannerState(const PannerId& pannerId)
{
    const juce::ScopedLock lock(m_stateMutex);
    
    std::string key = pannerId.toString();
    auto it = m_pannerStates.find(key);
    
    if (it != m_pannerStates.end())
        return it->second;
    
    // Create new state
    PannerCaptureState& state = m_pannerStates[key];
    state.pannerId = pannerId;
    
    // Create panner capture directory
    juce::File pannerDir = getPannerCaptureDir(pannerId);
    if (!pannerDir.exists())
    {
        pannerDir.createDirectory();
    }
    
    // Open chunk file for writing
    state.chunkFile = pannerDir.getChildFile("chunks.bin");
    state.outputStream = std::make_unique<juce::FileOutputStream>(state.chunkFile);
    
    if (!state.outputStream->openedOk())
    {
        DBG("[CaptureEngine] Failed to open chunk file: " + state.chunkFile.getFullPathName());
        state.outputStream.reset();
    }
    else
    {
        DBG("[CaptureEngine] Created chunk file: " + state.chunkFile.getFullPathName());
    }
    
    return state;
}

void CaptureEngine::closePannerState(PannerCaptureState& state)
{
    if (state.outputStream)
    {
        state.outputStream->flush();
        state.outputStream.reset();
    }
}

void CaptureEngine::closeAllPannerStates()
{
    const juce::ScopedLock lock(m_stateMutex);
    
    for (auto& pair : m_pannerStates)
    {
        closePannerState(pair.second);
    }
    
    m_pannerStates.clear();
}

//==============================================================================
PannerId CaptureEngine::createPannerId(const PannerInfo& panner) const
{
    return PannerId(
        m_sessionId.toStdString(),
        panner.name,
        panner.processId
    );
}

StateSnapshot CaptureEngine::createStateSnapshot(const PannerInfo& panner) const
{
    StateSnapshot snapshot;
    snapshot.azimuthDeg = panner.azimuth;
    snapshot.elevationDeg = panner.elevation;
    snapshot.diverge = panner.diverge;
    snapshot.gainDb = panner.gain;
    snapshot.stereoOrbitAzimuth = panner.stereoOrbitAzimuth;
    snapshot.stereoSpread = panner.stereoSpread;
    snapshot.stereoInputBalance = panner.stereoInputBalance;
    snapshot.autoOrbit = panner.autoOrbit;
    snapshot.autoOrbit = panner.autoOrbit;
    snapshot.inputMode = panner.inputMode;
    snapshot.outputMode = panner.outputMode;
    snapshot.pannerMode = panner.pannerMode;
    return snapshot;
}

juce::File CaptureEngine::getPannerCaptureDir(const PannerId& pannerId) const
{
    return m_captureRoot
        .getChildFile(m_sessionId)
        .getChildFile(juce::String(pannerId.instanceUuid) + "_" + juce::String(pannerId.processId));
}

//==============================================================================
void CaptureEngine::generateDebugFakeBlocks()
{
    // Generate fake blocks for testing UI without real panners
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<float> azimuthDist(-180.0f, 180.0f);
    static std::uniform_real_distribution<float> elevationDist(-90.0f, 90.0f);
    static std::uniform_int_distribution<int> dropoutDist(0, 100);
    
    const int numFakePanners = 3;
    const int sampleRate = 48000;
    const int blockSize = 512;
    
    // Generate blocks at ~10 blocks/sec per panner
    static uint64_t lastGenerateTime = 0;
    uint64_t now = static_cast<uint64_t>(juce::Time::currentTimeMillis());
    
    if (now - lastGenerateTime < 100)  // 100ms interval
        return;
    
    lastGenerateTime = now;
    
    for (int i = 0; i < numFakePanners; ++i)
    {
        PannerId pannerId(m_sessionId.toStdString(), "FakePanner_" + std::to_string(i), 10000 + i);
        
        // Simulate occasional dropout (2% chance)
        bool isDropout = (dropoutDist(gen) < 2);
        if (isDropout)
        {
            // Skip a block to simulate dropout
            m_debugSequenceNumber++;
            m_debugSamplePosition += blockSize;
            m_totalDropoutsDetected.fetch_add(1);
            m_coverageModel.addDropout(pannerId, m_debugSamplePosition - blockSize, m_debugSamplePosition, 1, true);
        }
        
        // Add coverage interval
        m_coverageModel.addPannerInterval(
            pannerId,
            m_debugSamplePosition,
            blockSize,
            sampleRate,
            2,  // stereo
            m_debugSequenceNumber,
            m_debugSequenceNumber + 1000
        );
    }
    
    // Advance position
    m_debugSamplePosition += blockSize;
    m_debugSequenceNumber++;
    
    // Notify listeners
    sendChangeMessage();
}

} // namespace Mach1

