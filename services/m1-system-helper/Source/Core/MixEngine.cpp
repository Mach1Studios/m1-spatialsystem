#include "MixEngine.h"
#include "../Managers/PannerTrackingManager.h"
#include "../Managers/M1MemoryShareTracker.h"
#include "../Common/SharedPathUtils.h"

namespace Mach1 {

//==============================================================================
MixEngine::MixEngine(PannerTrackingManager& pannerManager, const juce::String& segmentName)
    : juce::Thread("MixEngine")
    , m_pannerManager(pannerManager)
    , m_segmentName(segmentName)
{
}

MixEngine::~MixEngine()
{
    stopEngine();
}

void MixEngine::startEngine()
{
    if (!isThreadRunning())
        startThread(juce::Thread::Priority::high);
}

void MixEngine::stopEngine()
{
    stopThread(2000);

    const juce::ScopedLock lock(m_feedsMutex);
    m_feeds.clear();
    m_mixBus.reset();
    m_busActive.store(false);
}

void MixEngine::setOutputFormat(int channelCount)
{
    if (channelCount == 4 || channelCount == 8 || channelCount == 14)
        m_busChannels.store(channelCount);
}

MixEngine::Status MixEngine::getStatus() const
{
    Status status;
    status.busActive = m_busActive.load();
    status.busChannels = m_busChannels.load();
    status.sampleRate = m_currentSampleRate.load();
    status.liveFeeds = m_liveFeedCount.load();
    status.blocksPublished = m_blocksPublished.load();
    {
        const juce::ScopedLock lock(m_feedsMutex);
        status.totalFeeds = static_cast<int>(m_feeds.size());

        // Heartbeat count with a wider window than the render-pacing gate
        // (STALL_TIMEOUT_MS) so momentary scheduling hiccups don't make the
        // monitors' "streaming panners" report flap to zero and back.
        const auto nowMs = juce::Time::currentTimeMillis();
        for (const auto& [key, feed] : m_feeds)
        {
            if (feed->everReceived && (nowMs - feed->lastBlockWallMs) < 2000)
                ++status.streamingFeeds;
        }
    }
    return status;
}

float MixEngine::getMasterPeak(int channel) const
{
    if (channel < 0 || channel >= MAX_BUS_CHANNELS)
        return 0.0f;
    return m_masterPeaks[static_cast<size_t>(channel)].load();
}

float MixEngine::getFeedPeak(uint32_t processId, uintptr_t memoryAddress) const
{
    const juce::ScopedLock lock(m_feedsMutex);
    auto it = m_feeds.find(feedKey(processId, memoryAddress));
    return it != m_feeds.end() ? it->second->peak.load() : 0.0f;
}

//==============================================================================
void MixEngine::run()
{
    DBG("[MixEngine] Render thread started");

    juce::int64 lastFeedSyncMs = 0;

    while (!threadShouldExit())
    {
        const auto nowMs = juce::Time::currentTimeMillis();

        // Discovery is comparatively expensive (tracker lock + vector walk);
        // 250 ms matches the tracker's own scan cadence.
        if (nowMs - lastFeedSyncMs >= 250)
        {
            lastFeedSyncMs = nowMs;
            syncFeedsWithTracker();
        }

        drainFeeds();

        const int rendered = renderReadyBlocks();

        // Nothing mixable: keep the bus alive with wall-clock-paced silence so
        // the monitor's connection health stays green while tracks are idle.
        if (rendered == 0 && m_mixBus != nullptr && m_busActive.load())
        {
            const uint32_t sampleRate = m_currentSampleRate.load() != 0 ? m_currentSampleRate.load() : 48000;
            const double blockMs = 1000.0 * MIX_BLOCK_SIZE / sampleRate;
            if (static_cast<double>(nowMs - m_lastSilenceEmitMs) >= blockMs)
            {
                bool anyFresh = false;
                {
                    const juce::ScopedLock lock(m_feedsMutex);
                    for (const auto& [key, feed] : m_feeds)
                        anyFresh |= (nowMs - feed->lastBlockWallMs) < STALL_TIMEOUT_MS;
                }
                // Only pad with silence when feeds are stalled; if feeds are
                // live we are merely between blocks and must not inject gaps.
                if (!anyFresh)
                {
                    m_lastSilenceEmitMs = nowMs;
                    m_busBlock.clear();
                    publishBusBlock(sampleRate, false, 0.0);
                }
            }
        }

        wait(2);
    }

    DBG("[MixEngine] Render thread exiting");
}

//==============================================================================
void MixEngine::syncFeedsWithTracker()
{
    auto* tracker = m_pannerManager.getMemoryShareTracker();
    if (tracker == nullptr)
        return;

    const juce::ScopedLock lock(m_feedsMutex);

    // Add feeds for newly connected panners
    for (const auto& panner : tracker->getActivePanners())
    {
        auto share = panner.getShare();
        if (!panner.isConnected || share == nullptr || !share->isValid())
            continue;

        // Dedicated consumer: the capture engine sequentially drains the same
        // ring with its own cursor; the two must never share one. Checked for
        // existing feeds too: a remapped segment (plugin recreated its .mem
        // after a bus-width change) comes back without our registration.
        if (!share->isConsumerRegistered(MIX_CONSUMER_ID))
            share->registerConsumer(MIX_CONSUMER_ID);

        const uint64_t key = feedKey(panner.processId, panner.memoryAddress);
        if (m_feeds.count(key) != 0)
            continue;

        auto feed = std::make_unique<Feed>();
        feed->processId = panner.processId;
        feed->memoryAddress = panner.memoryAddress;
        feed->fifo.setSize(2, FIFO_CAPACITY);
        feed->fifo.clear();
        feed->encode = std::make_unique<Mach1Encode<float>>();
        m_feeds.emplace(key, std::move(feed));

        DBG("[MixEngine] Feed added for PID " + juce::String(static_cast<int>(panner.processId))
            + " addr " + juce::String::toHexString(static_cast<juce::int64>(panner.memoryAddress)));
    }

    // Drop feeds whose panner disappeared from tracking
    for (auto it = m_feeds.begin(); it != m_feeds.end();)
    {
        auto* live = tracker->findPanner(it->second->processId, it->second->memoryAddress);
        if (live == nullptr || !live->isConnected)
        {
            DBG("[MixEngine] Feed removed for PID " + juce::String(static_cast<int>(it->second->processId)));
            it = m_feeds.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void MixEngine::drainFeeds()
{
    auto* tracker = m_pannerManager.getMemoryShareTracker();
    if (tracker == nullptr)
        return;

    const juce::ScopedLock lock(m_feedsMutex);

    for (auto& [key, feed] : m_feeds)
    {
        auto* panner = tracker->findPanner(feed->processId, feed->memoryAddress);
        if (panner == nullptr)
            continue;
        auto share = panner->getShare();
        if (share == nullptr || !share->isValid())
            continue;

        M1MemoryShare::SharedBlock block;
        int drained = 0;
        // Bounded drain so one chatty feed cannot monopolise the tick
        while (drained < 64 && share->readNextBlockForConsumer(MIX_CONSUMER_ID, block))
        {
            ++drained;

            // Every block refreshes the parameter snapshot; only blocks with
            // samples feed the FIFO (parameter-only keepalives carry none).
            feed->params = block.parameters;
            feed->isPlaying = block.isPlaying;
            feed->playheadSeconds = block.playheadPositionInSeconds;
            if (block.sampleRate != 0)
                feed->sampleRate = block.sampleRate;

            if (block.audio.getNumSamples() > 0 && block.audio.getNumChannels() > 0)
            {
                pushAudioToFeed(*feed, block);
                feed->lastBlockWallMs = juce::Time::currentTimeMillis();
                feed->everReceived = true;
            }
        }
    }
}

void MixEngine::pushAudioToFeed(Feed& feed, const M1MemoryShare::SharedBlock& block)
{
    const int numSamples = block.audio.getNumSamples();
    const int numChannels = juce::jmin(block.audio.getNumChannels(), feed.fifo.getNumChannels());
    feed.channels = numChannels;

    // Latency cap: drop the oldest queued audio rather than letting a fast
    // producer (or a paused consumer) grow the monitoring delay unbounded.
    const uint64_t queuedAfter = (feed.writePos + static_cast<uint64_t>(numSamples)) - feed.readPos;
    if (queuedAfter > static_cast<uint64_t>(MAX_QUEUED_SAMPLES))
        feed.readPos = feed.writePos + static_cast<uint64_t>(numSamples) - static_cast<uint64_t>(MAX_QUEUED_SAMPLES);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* src = block.audio.getReadPointer(ch);
        for (int i = 0; i < numSamples; ++i)
        {
            const uint64_t pos = (feed.writePos + static_cast<uint64_t>(i)) % FIFO_CAPACITY;
            feed.fifo.setSample(ch, static_cast<int>(pos), src[i]);
        }
    }
    feed.writePos += static_cast<uint64_t>(numSamples);
}

//==============================================================================
int MixEngine::renderReadyBlocks()
{
    const juce::ScopedLock lock(m_feedsMutex);

    if (m_feeds.empty())
    {
        m_liveFeedCount.store(0);
        return 0;
    }

    const auto nowMs = juce::Time::currentTimeMillis();

    int rendered = 0;
    for (int pass = 0; pass < MAX_BLOCKS_PER_TICK; ++pass)
    {
        // A block is rendered when every live (non-stalled) feed can supply a
        // full mix block - the DAW's own clock paces the bus, no drift.
        int liveFeeds = 0;
        bool allReady = true;
        uint32_t sampleRate = 0;

        for (const auto& [key, feed] : m_feeds)
        {
            const bool fresh = feed->everReceived
                && (nowMs - feed->lastBlockWallMs) < STALL_TIMEOUT_MS;
            if (!fresh)
                continue;

            ++liveFeeds;
            if (feed->sampleRate != 0 && sampleRate == 0)
                sampleRate = feed->sampleRate;
            if (feed->writePos - feed->readPos < static_cast<uint64_t>(MIX_BLOCK_SIZE))
                allReady = false;
        }

        m_liveFeedCount.store(liveFeeds);

        if (liveFeeds == 0 || !allReady)
            break;

        renderOneBlock(sampleRate != 0 ? sampleRate : 48000);
        ++rendered;
    }

    return rendered;
}

void MixEngine::renderOneBlock(uint32_t sampleRate)
{
    const int busChannels = m_busChannels.load();
    if (!ensureMixBus(sampleRate, busChannels))
        return;

    m_busBlock.clear();
    const auto nowMs = juce::Time::currentTimeMillis();

    bool anyPlaying = false;
    double playheadSeconds = 0.0;

    for (auto& [key, feed] : m_feeds)
    {
        const bool fresh = feed->everReceived
            && (nowMs - feed->lastBlockWallMs) < STALL_TIMEOUT_MS;
        const bool hasSamples = (feed->writePos - feed->readPos) >= static_cast<uint64_t>(MIX_BLOCK_SIZE);
        if (!fresh || !hasSamples)
        {
            feed->peak.store(feed->peak.load() * 0.8f);
            continue;
        }

        configureFeedEncoder(*feed);

        const int inChans = feed->encode->getInputChannelsCount();
        const int outChans = feed->encode->getOutputChannelsCount();

        // Pull one block from the FIFO into the encoder input. Feed gain is
        // applied inside Mach1Encode (setOutputGain, dB) - see configure.
        float blockPeak = 0.0f;
        for (int ch = 0; ch < inChans; ++ch)
        {
            const int srcChannel = juce::jmin(ch, feed->channels - 1);
            for (int i = 0; i < MIX_BLOCK_SIZE; ++i)
            {
                const uint64_t pos = (feed->readPos + static_cast<uint64_t>(i)) % FIFO_CAPACITY;
                const float sample = feed->fifo.getSample(srcChannel, static_cast<int>(pos));
                feed->encodeIn[static_cast<size_t>(ch)][static_cast<size_t>(i)] = sample;
                blockPeak = juce::jmax(blockPeak, std::abs(sample));
            }
        }
        feed->readPos += static_cast<uint64_t>(MIX_BLOCK_SIZE);
        feed->peak.store(juce::jmax(blockPeak, feed->peak.load() * 0.8f));

        for (int ch = 0; ch < outChans; ++ch)
            std::fill(feed->encodeOut[static_cast<size_t>(ch)].begin(),
                      feed->encodeOut[static_cast<size_t>(ch)].end(), 0.0f);

        feed->encode->encodeBuffer(feed->encodeIn, feed->encodeOut, MIX_BLOCK_SIZE);

        const int mixChans = juce::jmin(outChans, m_busBlock.getNumChannels());
        for (int ch = 0; ch < mixChans; ++ch)
        {
            float* dst = m_busBlock.getWritePointer(ch);
            const auto& src = feed->encodeOut[static_cast<size_t>(ch)];
            for (int i = 0; i < MIX_BLOCK_SIZE; ++i)
                dst[i] += src[static_cast<size_t>(i)];
        }

        anyPlaying |= feed->isPlaying;
        playheadSeconds = juce::jmax(playheadSeconds, feed->playheadSeconds);
    }

    updateMasterPeaks();
    publishBusBlock(sampleRate, anyPlaying, playheadSeconds);
}

void MixEngine::configureFeedEncoder(Feed& feed)
{
    const auto& p = feed.params;

    const int inputMode = p.getInt(M1SystemHelperParameterIDs::INPUT_MODE, 0);
    int outputMode;
    switch (m_busChannels.load())
    {
        case 4:  outputMode = static_cast<int>(M1Spatial_4);  break;
        case 14: outputMode = static_cast<int>(M1Spatial_14); break;
        default: outputMode = static_cast<int>(M1Spatial_8);  break;
    }

    int pannerMode = 0;
    const bool isotropic = p.getBool(M1SystemHelperParameterIDs::ISOTROPIC_MODE, true);
    const bool equalPower = p.getBool(M1SystemHelperParameterIDs::EQUALPOWER_MODE, false);
    if (equalPower)      pannerMode = IsotropicEqualPower;
    else if (isotropic)  pannerMode = IsotropicLinear;
    else                 pannerMode = PeriphonicLinear;

    // Mode changes resize the SDK's gain matrix, but encodeBuffer's internal
    // crossfade history (last_gains) only re-seeds when the POINT count
    // changes; an output switch with an unchanged point count (e.g. the
    // helper's 8 -> 14 channel-config change) reads past the old inner
    // vectors and crashes. A fresh encoder starts with empty history and
    // re-seeds safely, at the cost of one un-smoothed block on a mode switch.
    if ((feed.lastInputMode != -1 && inputMode != feed.lastInputMode)
        || (feed.lastOutputMode != -1 && outputMode != feed.lastOutputMode))
    {
        feed.encode = std::make_unique<Mach1Encode<float>>();
        feed.lastInputMode = -1;
        feed.lastOutputMode = -1;
        feed.lastPannerMode = -1;
    }

    auto& e = *feed.encode;

    if (inputMode != feed.lastInputMode)
    {
        e.setInputMode(static_cast<Mach1EncodeInputMode>(inputMode));
        feed.lastInputMode = inputMode;
    }
    if (outputMode != feed.lastOutputMode)
    {
        e.setOutputMode(static_cast<Mach1EncodeOutputMode>(outputMode));
        feed.lastOutputMode = outputMode;
    }
    if (pannerMode != feed.lastPannerMode)
    {
        e.setPannerMode(static_cast<Mach1EncodePannerMode>(pannerMode));
        feed.lastPannerMode = pannerMode;
    }

    e.setAzimuthDegrees(p.getFloat(M1SystemHelperParameterIDs::AZIMUTH, 0.0f));
    e.setElevationDegrees(p.getFloat(M1SystemHelperParameterIDs::ELEVATION, 0.0f));
    e.setDiverge(p.getFloat(M1SystemHelperParameterIDs::DIVERGE, 50.0f) / 100.0f);
    e.setStereoSpread(juce::jlimit(0.0f, 1.0f, p.getFloat(M1SystemHelperParameterIDs::STEREO_SPREAD, 50.0f) / 100.0f));
    e.setAutoOrbit(p.getBool(M1SystemHelperParameterIDs::AUTO_ORBIT, true));
    e.setOrbitRotationDegrees(p.getFloat(M1SystemHelperParameterIDs::STEREO_ORBIT_AZIMUTH, 0.0f));
    e.setOutputGain(p.getFloat(M1SystemHelperParameterIDs::GAIN, 0.0f), true); // dB
    e.setGainCompensationActive(p.getBool(M1SystemHelperParameterIDs::GAIN_COMPENSATION_MODE, false));

    e.generatePointResults();

    const size_t inChans = static_cast<size_t>(e.getInputChannelsCount());
    const size_t outChans = static_cast<size_t>(e.getOutputChannelsCount());
    if (feed.encodeIn.size() != inChans || (inChans > 0 && feed.encodeIn[0].size() != MIX_BLOCK_SIZE))
    {
        feed.encodeIn.assign(inChans, std::vector<float>(MIX_BLOCK_SIZE, 0.0f));
    }
    if (feed.encodeOut.size() != outChans || (outChans > 0 && feed.encodeOut[0].size() != MIX_BLOCK_SIZE))
    {
        feed.encodeOut.assign(outChans, std::vector<float>(MIX_BLOCK_SIZE, 0.0f));
    }
}

//==============================================================================
bool MixEngine::ensureMixBus(uint32_t sampleRate, int channels)
{
    if (m_mixBus != nullptr && m_configuredBusChannels == channels
        && m_configuredSampleRate == sampleRate)
        return true;

    if (m_mixBus == nullptr)
    {
        const juce::File dir = juce::File(juce::String(SharedPathUtils::getSharedMemoryDirectory()));
        dir.createDirectory();
        const juce::File file = dir.getChildFile(m_segmentName + ".mem");

        // Generous fixed size: 14ch x 2048-sample slots fit comfortably
        m_mixBus = std::make_unique<M1MemoryShare>(m_segmentName.toStdString(),
                                                   16 * 1024 * 1024,
                                                   /*persistent*/ false,
                                                   /*createMode*/ true,
                                                   file.getFullPathName().toStdString());
        if (!m_mixBus->isValid())
        {
            DBG("[MixEngine] Failed to create MixBus segment");
            m_mixBus.reset();
            return false;
        }
    }

    if (!m_mixBus->initializeForAudio(sampleRate, static_cast<uint32_t>(channels), MIX_BLOCK_SIZE))
    {
        DBG("[MixEngine] Failed to configure MixBus ring");
        return false;
    }

    m_configuredBusChannels = channels;
    m_configuredSampleRate = sampleRate;
    m_currentSampleRate.store(sampleRate);
    m_busBlock.setSize(channels, MIX_BLOCK_SIZE);
    m_busBlock.clear();

    DBG("[MixEngine] MixBus configured: " + juce::String(channels) + " ch @ "
        + juce::String(static_cast<int>(sampleRate)) + " Hz");
    return true;
}

void MixEngine::publishBusBlock(uint32_t sampleRate, bool anyPlaying, double playheadSeconds)
{
    if (m_mixBus == nullptr)
        return;

    ParameterMap params;
    params.addInt(M1SystemHelperParameterIDs::OUTPUT_MODE, m_busChannels.load());
    params.addString(M1SystemHelperParameterIDs::DISPLAY_NAME, "MixBus");

    const uint64_t id = m_mixBus->writeAudioBufferWithGenericParameters(
        m_busBlock, params,
        static_cast<uint64_t>(juce::Time::currentTimeMillis()),
        playheadSeconds, anyPlaying,
        /*blockWhenConsumersBehind*/ false,
        /*updateSource*/ 2, sampleRate);

    if (id != 0)
    {
        m_blocksPublished.fetch_add(1);
        m_busActive.store(true);
    }
}

void MixEngine::updateMasterPeaks()
{
    const int channels = juce::jmin(m_busBlock.getNumChannels(), MAX_BUS_CHANNELS);
    for (int ch = 0; ch < channels; ++ch)
    {
        const float peak = m_busBlock.getMagnitude(ch, 0, m_busBlock.getNumSamples());
        auto& slot = m_masterPeaks[static_cast<size_t>(ch)];
        slot.store(juce::jmax(peak, slot.load() * 0.8f));
    }
}

} // namespace Mach1
