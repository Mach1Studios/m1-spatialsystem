/*
    MixEngine.h
    -----------
    The live render clock for external-renderer mode (P2). The helper owns no
    audio device, so nothing used to pace the encode/sum math - the mixer was
    API-only and the monitor had no way to hear the mix. This thread is that
    missing clock.

    Data flow, one iteration:

      panner segments --readNextBlockForConsumer--> per-feed FIFO (sample ring)
      while every fresh feed holds >= MIX_BLOCK_SIZE samples:
          per feed: FIFO -> Mach1Encode (from the feed's latest parameters)
          sum encoded outputs -> spatial bus block (4/8/14 ch)
          publish block to the MixBus shared-memory segment
      (all feeds silent/stalled -> wall-clock-paced silence keeps the bus alive)

    Pacing is data-driven, not wall-clock-driven: a mix block is produced when
    every live feed has delivered one block's worth of samples, so the DAW's
    audio clock transparently paces the bus and there is no drift. A feed that
    stops delivering (track idle, DAW closed, transport peculiarities) is
    marked stalled after STALL_TIMEOUT_MS and contributes silence without
    blocking the bus for everyone else.

    The M1-Monitor (on a mono/stereo-only DAW bus) opens the MixBus segment,
    reads blocks with a small jitter buffer, and runs its own Mach1Decode with
    its own head-tracking orientation - decode latency stays at DAW-block
    scale instead of round-tripping YPR through IPC.
*/

#pragma once

#include <JuceHeader.h>
#include "../Common/M1MemoryShare.h"
#include "../Common/TypesForDataExchange.h"
#include <Mach1Encode.h>

#include <array>
#include <atomic>
#include <map>
#include <memory>

namespace Mach1 {

class PannerTrackingManager;

class MixEngine : public juce::Thread
{
public:
    static constexpr const char* MIX_BUS_SEGMENT_NAME = "M1SpatialSystem_MixBus";
    static constexpr int MIX_BLOCK_SIZE = 512;
    static constexpr int MAX_BUS_CHANNELS = 14;

    /** @param segmentName override lets tests run against an isolated bus
        segment instead of the production well-known name. */
    explicit MixEngine(PannerTrackingManager& pannerManager,
                       const juce::String& segmentName = MIX_BUS_SEGMENT_NAME);
    ~MixEngine() override;

    void startEngine();
    void stopEngine();

    /** Spatial bus format: 4, 8 or 14 channels (Mach1 Spatial). Thread-safe. */
    void setOutputFormat(int channelCount);
    int getOutputFormat() const { return m_busChannels.load(); }

    struct Status
    {
        bool busActive = false;        // MixBus segment exists and is being written
        int busChannels = 0;
        uint32_t sampleRate = 0;
        int liveFeeds = 0;             // feeds passing the render-pacing freshness gate
        int streamingFeeds = 0;        // feeds with audio in the last 2s (heartbeat count)
        int totalFeeds = 0;
        uint64_t blocksPublished = 0;
    };
    Status getStatus() const;

    /** Peak level (absolute sample value, decayed) of one bus channel. */
    float getMasterPeak(int channel) const;

    /** Peak level of one panner feed's input audio (post feed gain). */
    float getFeedPeak(uint32_t processId, uintptr_t memoryAddress) const;

protected:
    void run() override;

private:
    //==========================================================================
    struct Feed
    {
        uint32_t processId = 0;
        uintptr_t memoryAddress = 0;

        // Circular sample buffer; positions are absolute sample counters
        juce::AudioBuffer<float> fifo;
        uint64_t writePos = 0;
        uint64_t readPos = 0;
        int channels = 2;

        // Latest parameter snapshot from the panner's blocks
        ParameterMap params;
        uint32_t sampleRate = 0;
        bool isPlaying = false;
        double playheadSeconds = 0.0;

        juce::int64 lastBlockWallMs = 0;
        bool everReceived = false;

        // Per-feed encoder state
        std::unique_ptr<Mach1Encode<float>> encode;
        int lastInputMode = -1;
        int lastOutputMode = -1;
        int lastPannerMode = -1;
        std::vector<std::vector<float>> encodeIn;   // [inChans][MIX_BLOCK_SIZE]
        std::vector<std::vector<float>> encodeOut;  // [outChans][MIX_BLOCK_SIZE]

        std::atomic<float> peak { 0.0f };
    };

    static constexpr int FIFO_CAPACITY = 32768;      // per-feed ring, samples
    static constexpr int MAX_QUEUED_SAMPLES = 8192;  // latency cap (drop-oldest)
    static constexpr int STALL_TIMEOUT_MS = 300;
    static constexpr int MAX_BLOCKS_PER_TICK = 8;
    static constexpr uint32_t MIX_CONSUMER_ID = 0x4D495845; // "MIXE"

    PannerTrackingManager& m_pannerManager;
    juce::String m_segmentName;

    mutable juce::CriticalSection m_feedsMutex;
    std::map<uint64_t, std::unique_ptr<Feed>> m_feeds; // keyed by (pid << 48) ^ address

    std::unique_ptr<M1MemoryShare> m_mixBus;
    std::atomic<int> m_busChannels { 8 };
    int m_configuredBusChannels = 0;      // geometry the segment was initialised with
    uint32_t m_configuredSampleRate = 0;
    juce::AudioBuffer<float> m_busBlock;  // mix accumulator [busChannels][MIX_BLOCK_SIZE]

    std::atomic<bool> m_busActive { false };
    std::atomic<uint64_t> m_blocksPublished { 0 };
    std::atomic<uint32_t> m_currentSampleRate { 0 };
    std::atomic<int> m_liveFeedCount { 0 };
    std::array<std::atomic<float>, MAX_BUS_CHANNELS> m_masterPeaks {};

    juce::int64 m_lastSilenceEmitMs = 0;

    //==========================================================================
    void syncFeedsWithTracker();
    void drainFeeds();
    void pushAudioToFeed(Feed& feed, const M1MemoryShare::SharedBlock& block);
    int renderReadyBlocks();
    void renderOneBlock(uint32_t sampleRate);
    void configureFeedEncoder(Feed& feed);
    bool ensureMixBus(uint32_t sampleRate, int channels);
    void publishBusBlock(uint32_t sampleRate, bool anyPlaying, double playheadSeconds);
    void updateMasterPeaks();

    static uint64_t feedKey(uint32_t processId, uintptr_t memoryAddress)
    {
        return (static_cast<uint64_t>(processId) << 48) ^ static_cast<uint64_t>(memoryAddress);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixEngine)
};

} // namespace Mach1
