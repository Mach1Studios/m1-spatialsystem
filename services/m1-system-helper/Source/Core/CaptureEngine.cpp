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

    // Manifest identifies this directory as an attributable capture session;
    // the StorageGovernor treats directories without one as orphaned.
    writeSessionManifest();
    
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

    // Final manifest with the closing byte counts
    writeSessionManifest();
    
    DBG("[CaptureEngine] Stopped capture");
    
    // Notify listeners
    sendChangeMessage();
}

void CaptureEngine::writeSessionManifest()
{
    if (m_sessionId.isEmpty() || m_captureRoot == juce::File())
        return;

    auto* root = new juce::DynamicObject();
    root->setProperty("sessionId", m_sessionId);
    root->setProperty("createdMs", m_startTime.toMilliseconds());
    root->setProperty("lastWrittenMs", juce::Time::currentTimeMillis());
    root->setProperty("totalBytes", static_cast<juce::int64>(m_totalBytesWritten.load()));
    root->setProperty("totalChunks", static_cast<juce::int64>(m_totalChunksWritten.load()));

    juce::Array<juce::var> streams;
    {
        const juce::ScopedLock lock(m_stateMutex);
        for (const auto& [key, state] : m_pannerStates)
        {
            auto* stream = new juce::DynamicObject();
            stream->setProperty("dir", juce::String(state.pannerId.instanceUuid) + "_"
                                       + juce::String(state.pannerId.processId));
            stream->setProperty("name", juce::String(state.lastDisplayName));
            stream->setProperty("chunks", static_cast<juce::int64>(state.chunksWritten));
            stream->setProperty("bytes", static_cast<juce::int64>(state.bytesWritten));
            streams.add(juce::var(stream));
        }
    }
    root->setProperty("streams", streams);

    const juce::File manifestFile = m_captureRoot.getChildFile(m_sessionId).getChildFile("manifest.json");
    // Write-then-rename so a reader never sees a half-written manifest
    const juce::File tempFile = manifestFile.getSiblingFile("manifest.json.tmp");
    if (tempFile.replaceWithText(juce::JSON::toString(juce::var(root))))
        tempFile.moveFileTo(manifestFile);
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
    
    juce::int64 lastManifestMs = juce::Time::currentTimeMillis();

    while (!threadShouldExit() && m_capturing.load())
    {
        processCapture();

        // Refresh the session manifest a few times a minute so the storage
        // panel sees near-live byte counts and a fresh lastWrittenMs even if
        // the helper is killed without a clean stop.
        const auto nowMs = juce::Time::currentTimeMillis();
        if (nowMs - lastManifestMs > 10000)
        {
            lastManifestMs = nowMs;
            writeSessionManifest();
        }
        
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
    
    // Find the panner in the tracker. The memory address disambiguates
    // multiple plugin instances hosted in the same DAW process.
    auto* memPanner = tracker->findPanner(panner.processId, panner.memoryAddress);
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
    
    // Drain the ring sequentially through this consumer's cursor. Every
    // published block is seen exactly once; ring overruns are reported via
    // droppedBlocksBefore. Bounded per pass so one busy panner cannot starve
    // the others.
    const uint32_t consumerId = tracker->getConsumerId();
    M1MemoryShare::SharedBlock block;
    int blocksThisPass = 0;
    
    while (blocksThisPass < MAX_BLOCKS_PER_PASS
           && memPanner->memoryShare->readNextBlockForConsumer(consumerId, block))
    {
        ++blocksThisPass;
        ingestBlock(panner, pannerId, block);
    }
}

void CaptureEngine::ingestBlock(const PannerInfo& panner, const PannerId& pannerId,
                                const M1MemoryShare::SharedBlock& block)
{
    PannerCaptureState& state = getOrCreatePannerState(pannerId);

    // Keep the human-readable name next to the capture data (the directory
    // name is a stable instance id, not the track name). Updated whenever
    // the DAW reports a new track name; name changes are rare so the tiny
    // file write is negligible.
    if (!panner.name.empty() && panner.name != state.lastDisplayName)
    {
        state.lastDisplayName = panner.name;
        getPannerCaptureDir(pannerId).getChildFile("name.txt")
            .replaceWithText(juce::String(panner.name));
    }
    
    const uint32_t sampleRate = block.sampleRate > 0 ? block.sampleRate : 44100;
    const int64_t startSample = block.startSamplePosition;
    const int32_t numSamples = block.audio.getNumSamples();
    const int16_t numChannels = static_cast<int16_t>(block.audio.getNumChannels());
    
    // Record blocks lost to ring overrun (capture fell behind the writer)
    if (block.droppedBlocksBefore > 0)
    {
        const uint32_t missed = static_cast<uint32_t>(block.droppedBlocksBefore);
        m_totalDropoutsDetected.fetch_add(missed);
        
        const int64_t dropoutStart = state.lastEndSample;
        const int64_t dropoutEnd = startSample;
        if (dropoutEnd > dropoutStart)
        {
            m_coverageModel.addDropout(pannerId, dropoutStart, dropoutEnd, missed, true);
        }
    }
    
    // Occasional debug logging
    static std::map<std::string, juce::int64> lastBufferLogTime;
    auto now = juce::Time::currentTimeMillis();
    std::string key = pannerId.toString();
    if (now - lastBufferLogTime[key] > 2000) // Every 2s per panner
    {
        lastBufferLogTime[key] = now;
        DBG("[CaptureEngine] Block from " + juce::String(panner.name) + 
            ": bufferId=" + juce::String((juce::int64)block.bufferId) +
            " startSample=" + juce::String(startSample) +
            " isPlaying=" + juce::String(block.isPlaying ? "yes" : "no") +
            " channels=" + juce::String((int)numChannels) +
            " samples=" + juce::String(numSamples) +
            " dropped=" + juce::String((juce::int64)block.droppedBlocksBefore));
    }
    
    // Parameter-only blocks carry no audio; just track the sequence
    if (numChannels <= 0 || numSamples <= 0)
    {
        state.lastSequenceNumber = block.sequenceNumber;
        state.lastBufferId = block.bufferId;
        return;
    }
    
    // Create chunk header
    ChunkHeader header;
    header.startSample = startSample;
    header.numSamples = numSamples;
    header.numChannels = numChannels;
    header.sampleRate = sampleRate;
    header.bufferId = block.bufferId;
    header.sequenceNumber = block.sequenceNumber;
    header.dawTimestampMs = block.dawTimestamp;
    header.wallClockMs = static_cast<uint64_t>(now);
    header.audioDataSize = static_cast<uint32_t>(numChannels * numSamples * sizeof(float));
    
    // State snapshot straight from the block's own parameter payload, so the
    // captured automation state is sample-aligned with the captured audio.
    StateSnapshot snapshot = createStateSnapshot(panner, block.parameters);
    snapshot.captureTimestampMs = header.wallClockMs;
    snapshot.stateSeq = block.sequenceNumber;
    
    // Interleave audio data for storage
    std::vector<float> interleavedAudio(static_cast<size_t>(numChannels) * numSamples);
    for (int sample = 0; sample < numSamples; ++sample)
    {
        for (int channel = 0; channel < numChannels; ++channel)
        {
            interleavedAudio[static_cast<size_t>(sample) * numChannels + channel] = block.audio.getSample(channel, sample);
        }
    }
    
    // Write chunk to disk
    writeChunk(state, header, snapshot, interleavedAudio.data());
    
    // Update coverage model
    m_coverageModel.addPannerInterval(pannerId, startSample, numSamples,
                                      sampleRate, numChannels, block.sequenceNumber, block.bufferId);
    
    // Update state tracking
    state.lastSequenceNumber = block.sequenceNumber;
    state.lastBufferId = block.bufferId;
    state.lastEndSample = startSample + numSamples;
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
    // The instance uuid must be unique per plugin instance AND stable for
    // the instance's lifetime. Display names are neither: the default
    // "M1-Panner (PID x)" flips to the DAW track name as soon as the host
    // reports it, which used to split one instance's capture into two
    // streams mid-session. Identity therefore comes from the memory address
    // (unique per instance within a process); the human-readable name is
    // stored separately (name.txt in the capture directory).
    std::string instanceUuid;
    if (panner.memoryAddress != 0)
        instanceUuid = "PTR" + juce::String::toHexString(static_cast<juce::int64>(panner.memoryAddress)).toStdString();
    else if (panner.port != 0)
        instanceUuid = "PORT" + std::to_string(panner.port);
    else
        instanceUuid = panner.name;

    return PannerId(
        m_sessionId.toStdString(),
        instanceUuid,
        panner.processId
    );
}

StateSnapshot CaptureEngine::createStateSnapshot(const PannerInfo& panner,
                                                 const ParameterMap& blockParameters) const
{
    // Prefer the parameter payload carried inside the block itself (it was
    // serialized in the same processBlock() as the audio); fall back to the
    // tracker's last-known values for anything missing.
    StateSnapshot snapshot;
    snapshot.azimuthDeg = blockParameters.getFloat(M1SystemHelperParameterIDs::AZIMUTH, panner.azimuth);
    snapshot.elevationDeg = blockParameters.getFloat(M1SystemHelperParameterIDs::ELEVATION, panner.elevation);
    snapshot.diverge = blockParameters.getFloat(M1SystemHelperParameterIDs::DIVERGE, panner.diverge);
    snapshot.gainDb = blockParameters.getFloat(M1SystemHelperParameterIDs::GAIN, panner.gain);
    snapshot.stereoOrbitAzimuth = blockParameters.getFloat(M1SystemHelperParameterIDs::STEREO_ORBIT_AZIMUTH, panner.stereoOrbitAzimuth);
    snapshot.stereoSpread = blockParameters.getFloat(M1SystemHelperParameterIDs::STEREO_SPREAD, panner.stereoSpread);
    snapshot.stereoInputBalance = blockParameters.getFloat(M1SystemHelperParameterIDs::STEREO_INPUT_BALANCE, panner.stereoInputBalance);
    snapshot.autoOrbit = blockParameters.getBool(M1SystemHelperParameterIDs::AUTO_ORBIT, panner.autoOrbit);
    snapshot.inputMode = blockParameters.getInt(M1SystemHelperParameterIDs::INPUT_MODE, panner.inputMode);
    snapshot.outputMode = blockParameters.getInt(M1SystemHelperParameterIDs::OUTPUT_MODE, panner.outputMode);
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

