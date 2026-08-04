// Regression tests for the M1MemoryShare slot-ring transport (layout v2).
//
// Guards against the transport failure modes found in the field:
//  1. The old layout had exactly one buffer slot: every write overwrote the
//     previous block, so any reader slower than the writer silently lost
//     audio (capture dropouts during playback and offline bounces).
//  2. Readers had no cursors: the tracker's parameter poll and the capture
//     engine consumed the same "latest" slot and deduped by bufferId, which
//     also hid drops entirely (no accounting).
//  3. Offline bounces render faster than realtime; without backpressure the
//     writer lapped the helper and exports had gaps.
//
// The v2 ring gives every registered consumer its own cursor, counts overruns
// explicitly, and lets non-realtime writers block until consumers catch up.
//
// Both repos compile a sibling copy of M1MemoryShare; these tests exercise
// the helper copy, whose ring logic is textually identical to the panner's.

#include <JuceHeader.h>
#include "Common/M1MemoryShare.h"
#include "Common/TypesForDataExchange.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

static int ringFailures = 0;

#define RCHECK(cond)                                                                  \
    do {                                                                              \
        if (!(cond)) {                                                                \
            ++ringFailures;                                                           \
            std::cout << "FAILED: " #cond " (" << __FILE__ << ":" << __LINE__ << ")"  \
                      << std::endl;                                                   \
        }                                                                             \
    } while (0)

namespace {

constexpr uint32_t kSampleRate = 48000;
constexpr int kBlockSamples = 512;
constexpr uint32_t kConsumerId = 9001;

juce::File makeTempSegmentPath(const juce::String& testName)
{
    auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                   .getChildFile("m1-memoryshare-ring-tests");
    dir.createDirectory();
    return dir.getChildFile(testName + "-"
                            + juce::String(juce::Random::getSystemRandom().nextInt(1 << 30))
                            + ".mem");
}

// One writer mapping (simulating the panner) + one reader mapping of the same
// file (simulating the helper), like the real cross-process setup.
struct Segment
{
    juce::File path;
    std::unique_ptr<M1MemoryShare> writer;
    std::unique_ptr<M1MemoryShare> reader;

    explicit Segment(const juce::String& testName)
        : path(makeTempSegmentPath(testName))
    {
        writer = std::make_unique<M1MemoryShare>("ring-test", 1024 * 1024,
                                                 /*persistent*/ false, /*createMode*/ true,
                                                 path.getFullPathName().toStdString());
        writer->initializeForAudio(kSampleRate, 2, kBlockSamples);

        reader = std::make_unique<M1MemoryShare>("ring-test", 1024 * 1024,
                                                 /*persistent*/ true, /*createMode*/ false,
                                                 path.getFullPathName().toStdString());
    }

    ~Segment()
    {
        // Unmap the reader before the (non-persistent) writer deletes the file.
        reader.reset();
        writer.reset();
    }
};

// Writes one block whose audio and azimuth encode the given marker value.
// Playhead advances one whole second per block so the sample position
// round-trips exactly through the double conversion.
uint64_t writeMarkedBlock(M1MemoryShare& writer, double marker, bool blockWhenBehind = false,
                          int numSamples = kBlockSamples)
{
    juce::AudioBuffer<float> audio(2, numSamples);
    for (int ch = 0; ch < audio.getNumChannels(); ++ch)
        for (int s = 0; s < numSamples; ++s)
            audio.setSample(ch, s, static_cast<float>(marker) + static_cast<float>(ch));

    ParameterMap params;
    params.addFloat(M1SystemHelperParameterIDs::AZIMUTH, static_cast<float>(marker));
    params.addBool(M1SystemHelperParameterIDs::AUTO_ORBIT, true);
    params.addInt(M1SystemHelperParameterIDs::PORT, 9999);
    params.addString(M1SystemHelperParameterIDs::DISPLAY_NAME, "Ring Test Panner");

    return writer.writeAudioBufferWithGenericParameters(audio, params,
                                                        /*dawTimestamp*/ 12345,
                                                        /*playheadSeconds*/ marker,
                                                        /*isPlaying*/ true,
                                                        blockWhenBehind,
                                                        /*updateSource*/ 1,
                                                        kSampleRate);
}

void testSequentialReadDeliversEveryBlock()
{
    Segment seg("sequential");
    RCHECK(seg.writer->isValid());
    RCHECK(seg.writer->isRingConfigured());
    RCHECK(seg.reader->isValid());
    RCHECK(seg.reader->isRingConfigured());
    RCHECK(seg.reader->getSlotCount() == seg.writer->getSlotCount());
    RCHECK(seg.reader->registerConsumer(kConsumerId));

    constexpr int kBlocks = 20;
    for (int i = 0; i < kBlocks; ++i)
        RCHECK(writeMarkedBlock(*seg.writer, static_cast<double>(i)) == static_cast<uint64_t>(i) + 1);

    M1MemoryShare::SharedBlock block;
    for (int i = 0; i < kBlocks; ++i)
    {
        RCHECK(seg.reader->readNextBlockForConsumer(kConsumerId, block));
        RCHECK(block.bufferId == static_cast<uint64_t>(i) + 1);
        RCHECK(block.sequenceNumber == static_cast<uint32_t>(i));
        RCHECK(block.droppedBlocksBefore == 0);
        RCHECK(block.isPlaying);
        RCHECK(block.dawTimestamp == 12345);
        RCHECK(block.sampleRate == kSampleRate);
        // marker seconds * 48000 (exact for whole seconds)
        RCHECK(block.startSamplePosition == static_cast<int64_t>(i) * kSampleRate);

        // Audio round-trips through the interleave/deinterleave
        RCHECK(block.audio.getNumChannels() == 2);
        RCHECK(block.audio.getNumSamples() == kBlockSamples);
        RCHECK(block.audio.getSample(0, 0) == static_cast<float>(i));
        RCHECK(block.audio.getSample(1, kBlockSamples - 1) == static_cast<float>(i) + 1.0f);

        // Parameter payload round-trips
        RCHECK(block.parameters.getFloat(M1SystemHelperParameterIDs::AZIMUTH, -1.0f) == static_cast<float>(i));
        RCHECK(block.parameters.getBool(M1SystemHelperParameterIDs::AUTO_ORBIT, false));
        RCHECK(block.parameters.getInt(M1SystemHelperParameterIDs::PORT, 0) == 9999);
        RCHECK(block.parameters.getString(M1SystemHelperParameterIDs::DISPLAY_NAME, "") == "Ring Test Panner");
    }

    // Fully drained
    RCHECK(!seg.reader->readNextBlockForConsumer(kConsumerId, block));
}

void testLatestReadDoesNotConsume()
{
    Segment seg("latest");
    RCHECK(seg.reader->registerConsumer(kConsumerId));

    for (int i = 0; i < 5; ++i)
        writeMarkedBlock(*seg.writer, static_cast<double>(i));

    // Latest-read (tracker's parameter poll) always sees the newest block...
    M1MemoryShare::SharedBlock latest;
    RCHECK(seg.reader->readLatestBlock(latest));
    RCHECK(latest.bufferId == 5);
    RCHECK(seg.reader->readLatestBlock(latest));
    RCHECK(latest.bufferId == 5);

    // ...while the sequential consumer still receives every block from the start.
    M1MemoryShare::SharedBlock block;
    for (int i = 0; i < 5; ++i)
    {
        RCHECK(seg.reader->readNextBlockForConsumer(kConsumerId, block));
        RCHECK(block.bufferId == static_cast<uint64_t>(i) + 1);
        RCHECK(block.droppedBlocksBefore == 0);
    }
}

void testOverrunSkipsAndCountsDroppedBlocks()
{
    Segment seg("overrun");
    RCHECK(seg.reader->registerConsumer(kConsumerId));

    const uint32_t slotCount = seg.writer->getSlotCount();
    RCHECK(slotCount >= M1MemoryShare::MIN_SLOT_COUNT);

    // Realtime writer laps the consumer: write slotCount + 10 blocks unread.
    const uint64_t total = slotCount + 10;
    for (uint64_t i = 0; i < total; ++i)
        RCHECK(writeMarkedBlock(*seg.writer, static_cast<double>(i)) != 0);

    // The first read must skip to the oldest intact block and report exactly
    // how many blocks were lost - never deliver torn/overwritten data.
    M1MemoryShare::SharedBlock block;
    RCHECK(seg.reader->readNextBlockForConsumer(kConsumerId, block));
    const uint64_t expectedFirst = total - slotCount + 1; // oldest safe block index
    RCHECK(block.blockIndex == expectedFirst);
    RCHECK(block.droppedBlocksBefore == expectedFirst);
    RCHECK(block.audio.getSample(0, 0) == static_cast<float>(expectedFirst));

    // The remaining blocks arrive in order without further drops.
    uint64_t readCount = 1;
    while (seg.reader->readNextBlockForConsumer(kConsumerId, block))
    {
        RCHECK(block.droppedBlocksBefore == 0);
        RCHECK(block.blockIndex == expectedFirst + readCount);
        ++readCount;
    }
    RCHECK(readCount == slotCount - 1);
    RCHECK(readCount + expectedFirst == total);
}

void testTwoConsumersHaveIndependentCursors()
{
    Segment seg("two-consumers");
    RCHECK(seg.reader->registerConsumer(9001));
    RCHECK(seg.reader->registerConsumer(9002));

    for (int i = 0; i < 5; ++i)
        writeMarkedBlock(*seg.writer, static_cast<double>(i));

    // First consumer drains everything...
    M1MemoryShare::SharedBlock block;
    for (int i = 0; i < 5; ++i)
    {
        RCHECK(seg.reader->readNextBlockForConsumer(9001, block));
        RCHECK(block.bufferId == static_cast<uint64_t>(i) + 1);
    }
    RCHECK(!seg.reader->readNextBlockForConsumer(9001, block));

    // ...and the second consumer still sees every block independently.
    for (int i = 0; i < 5; ++i)
    {
        RCHECK(seg.reader->readNextBlockForConsumer(9002, block));
        RCHECK(block.bufferId == static_cast<uint64_t>(i) + 1);
    }
    RCHECK(!seg.reader->readNextBlockForConsumer(9002, block));
}

void testNonRealtimeBackpressureLosesNothing()
{
    Segment seg("backpressure");
    RCHECK(seg.reader->registerConsumer(kConsumerId));

    const uint32_t slotCount = seg.writer->getSlotCount();
    const uint64_t total = static_cast<uint64_t>(slotCount) * 3; // 3x ring capacity

    // Offline-bounce writer: much faster than the consumer, but with
    // blockWhenConsumersBehind=true it must wait instead of overwriting.
    std::atomic<uint64_t> written { 0 };
    std::thread writerThread([&]() {
        for (uint64_t i = 0; i < total; ++i)
        {
            if (writeMarkedBlock(*seg.writer, static_cast<double>(i), /*blockWhenBehind*/ true) != 0)
                ++written;
        }
    });

    // Deliberately slow consumer.
    M1MemoryShare::SharedBlock block;
    uint64_t readCount = 0;
    uint64_t totalDropped = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    while (readCount < total && std::chrono::steady_clock::now() < deadline)
    {
        if (seg.reader->readNextBlockForConsumer(kConsumerId, block))
        {
            RCHECK(block.blockIndex == readCount); // strict order, no gaps
            totalDropped += block.droppedBlocksBefore;
            ++readCount;
            if ((readCount % 8) == 0)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        }
    }

    writerThread.join();

    RCHECK(written.load() == total);
    RCHECK(readCount == total);
    RCHECK(totalDropped == 0);
}

void testBackpressureTimeoutWhenConsumerStalls()
{
    Segment seg("backpressure-timeout");
    RCHECK(seg.reader->registerConsumer(kConsumerId));

    const uint32_t slotCount = seg.writer->getSlotCount();
    seg.writer->setBackpressureTimeoutMs(50);

    // Fill up to just below the point where a blocking write would wait.
    for (uint64_t i = 0; i + 2 <= slotCount; ++i)
        RCHECK(writeMarkedBlock(*seg.writer, static_cast<double>(i), true) != 0);

    // The consumer never reads: the next blocking writes must time out and
    // proceed (a dead helper must not hang an offline bounce forever).
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 3; ++i)
        RCHECK(writeMarkedBlock(*seg.writer, 1000.0 + i, true) != 0);
    const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::steady_clock::now() - start).count();

    RCHECK(elapsedMs >= 100); // 3 writes x ~50ms timeout (allow scheduling slack)
    RCHECK(elapsedMs < 5000);
}

void testControlMessageRing()
{
    Segment seg("control");

    // Helper -> panner: the reader-side mapping writes control messages, the
    // writer-side (panner) mapping consumes them in order.
    RCHECK(seg.reader->writeControlMessage(0x1111, ParameterType::FLOAT, 45.0f));
    RCHECK(seg.reader->writeControlMessage(0x2222, ParameterType::INT, 0.0f, 7));
    RCHECK(seg.reader->writeControlMessage(0x3333, ParameterType::FLOAT, -12.5f));

    M1MemoryShare::ControlMessage msg;
    RCHECK(seg.writer->readControlMessage(msg));
    RCHECK(msg.parameterID == 0x1111);
    RCHECK(msg.floatValue == 45.0f);

    RCHECK(seg.writer->readControlMessage(msg));
    RCHECK(msg.parameterID == 0x2222);
    RCHECK(msg.intValue == 7);

    RCHECK(seg.writer->readControlMessage(msg));
    RCHECK(msg.parameterID == 0x3333);
    RCHECK(msg.floatValue == -12.5f);

    RCHECK(!seg.writer->readControlMessage(msg)); // drained

    // A full ring must reject further writes instead of overwriting unread
    // messages (index-based, no wraparound corruption).
    for (uint32_t i = 0; i < M1MemoryShare::MAX_CONTROL_MESSAGES; ++i)
        RCHECK(seg.reader->writeControlMessage(i, ParameterType::FLOAT, static_cast<float>(i)));
    RCHECK(!seg.reader->writeControlMessage(999, ParameterType::FLOAT, 999.0f));

    // Draining one frees exactly one slot.
    RCHECK(seg.writer->readControlMessage(msg));
    RCHECK(msg.parameterID == 0);
    RCHECK(seg.reader->writeControlMessage(999, ParameterType::FLOAT, 999.0f));
}

void testGarbageSegmentIsRejected()
{
    // A file full of junk (or a legacy v1 layout without magic/version) must
    // be rejected on open instead of being misparsed.
    juce::File path = makeTempSegmentPath("garbage");
    {
        juce::FileOutputStream out(path);
        RCHECK(out.openedOk());
        std::vector<uint8_t> junk(1024 * 1024, 0xAB);
        out.write(junk.data(), junk.size());
    }

    M1MemoryShare reader("garbage-test", 1024 * 1024, true, /*createMode*/ false,
                         path.getFullPathName().toStdString());
    RCHECK(!reader.isValid());
    RCHECK(!reader.isRingConfigured());

    M1MemoryShare::SharedBlock block;
    RCHECK(!reader.readLatestBlock(block));

    path.deleteFile();
}

void testGeometryReinitResetsRing()
{
    Segment seg("reinit");
    RCHECK(seg.reader->registerConsumer(kConsumerId));

    for (int i = 0; i < 5; ++i)
        writeMarkedBlock(*seg.writer, static_cast<double>(i));

    const uint32_t generationBefore = seg.writer->getRingGeneration();

    // Same geometry: no-op, cursors intact.
    RCHECK(seg.writer->initializeForAudio(kSampleRate, 2, kBlockSamples));
    RCHECK(seg.writer->getRingGeneration() == generationBefore);
    RCHECK(seg.writer->getWriteCursor() == 5);

    // Host reconfigures to a larger block size: ring resets.
    RCHECK(seg.writer->initializeForAudio(kSampleRate, 2, 4096));
    RCHECK(seg.writer->getRingGeneration() == generationBefore + 1);
    RCHECK(seg.writer->getWriteCursor() == 0);

    // The consumer must recover cleanly and read the new stream from block 0.
    M1MemoryShare::SharedBlock block;
    RCHECK(!seg.reader->readNextBlockForConsumer(kConsumerId, block));

    RCHECK(writeMarkedBlock(*seg.writer, 100.0, false, 4096) == 1);
    RCHECK(writeMarkedBlock(*seg.writer, 101.0, false, 4096) == 2);

    RCHECK(seg.reader->readNextBlockForConsumer(kConsumerId, block));
    RCHECK(block.bufferId == 1);
    RCHECK(block.audio.getNumSamples() == 4096);
    RCHECK(block.audio.getSample(0, 0) == 100.0f);

    RCHECK(seg.reader->readNextBlockForConsumer(kConsumerId, block));
    RCHECK(block.bufferId == 2);
}

void testOversizedBlockIsRefusedNotCorrupted()
{
    Segment seg("oversized");
    RCHECK(seg.reader->registerConsumer(kConsumerId));

    // Ring sized for 2048-sample slots; a 100000-sample block cannot fit and
    // must be refused outright (returns 0) without touching the ring.
    RCHECK(writeMarkedBlock(*seg.writer, 0.0, false, /*numSamples*/ 100000) == 0);
    RCHECK(seg.writer->getWriteCursor() == 0);

    M1MemoryShare::SharedBlock block;
    RCHECK(!seg.reader->readNextBlockForConsumer(kConsumerId, block));
}

} // namespace

int runMemoryShareRingTests()
{
    testSequentialReadDeliversEveryBlock();
    testLatestReadDoesNotConsume();
    testOverrunSkipsAndCountsDroppedBlocks();
    testTwoConsumersHaveIndependentCursors();
    testNonRealtimeBackpressureLosesNothing();
    testBackpressureTimeoutWhenConsumerStalls();
    testControlMessageRing();
    testGarbageSegmentIsRejected();
    testGeometryReinitResetsRing();
    testOversizedBlockIsRefusedNotCorrupted();

    return ringFailures;
}
