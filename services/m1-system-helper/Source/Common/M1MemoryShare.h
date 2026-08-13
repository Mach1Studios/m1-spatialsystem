#pragma once

#include <atomic>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "Common.h"
#include "TypesForDataExchange.h"

// Platform-specific includes
#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
#endif

/**
 * M1MemoryShare - shared-memory audio block transport between M1-Panner
 * plugin instances (writer) and m1-system-helper (readers).
 *
 * Layout v2 ("slot ring"):
 *
 *   [SharedMemoryHeader]                     versioned header with ring cursors
 *   [slot 0][slot 1]...[slot N-1]            fixed-size block slots
 *   [control ring]                           MAX_CONTROL_MESSAGES ControlMessage
 *
 * Each slot contains one serialized audio block:
 *   GenericAudioBufferHeader + GenericParameter entries + interleaved audio.
 *
 * Concurrency model:
 *  - Single writer process (the panner). Writes go to slot
 *    (writeCursor % slotCount) and are published by a release-store that
 *    increments writeCursor. In-process writer threads are serialized by
 *    m_writerMutex (try-lock on the realtime path).
 *  - Readers either poll the latest published block (non-consuming) or
 *    register a consumer and read sequentially with a per-consumer cursor.
 *    Torn reads (writer lapping the reader mid-copy) are detected by
 *    re-checking writeCursor after the copy and validating the block's
 *    bufferId against the expected block index.
 *  - For non-realtime (offline bounce) writes the writer can block until all
 *    registered consumers have caught up, so no blocks are lost.
 *
 * IMPORTANT: this file has a sibling copy in m1-panner/Source/M1MemoryShare.h.
 * The SharedMemoryHeader / ControlMessage structs and the ring logic must stay
 * layout-identical between the two copies.
 */
class M1MemoryShare
{
public:
    static constexpr uint32_t MEMORY_LAYOUT_MAGIC = 0x4D315348;  // "M1SH"
    static constexpr uint32_t MEMORY_LAYOUT_VERSION = 2;
    static constexpr uint32_t MAX_CONSUMERS = 8;
    static constexpr uint32_t MAX_CONTROL_MESSAGES = 16;

    // Per-slot reservation for serialized parameters (worst case today ~400B).
    static constexpr uint32_t MAX_PARAMETER_BYTES = 1024;
    // Minimum audio headroom per slot in samples-per-channel, so common host
    // block sizes (<= 2048) never require re-initializing the ring geometry.
    static constexpr uint32_t MIN_SLOT_SAMPLES = 2048;
    static constexpr uint32_t MIN_SLOT_COUNT = 4;
    static constexpr uint32_t MAX_SLOT_COUNT = 256;

    /**
     * Header at offset 0 of every shared memory segment.
     *
     * Atomics are used for cross-process publication; they are lock-free and
     * address-free for 32/64-bit integers on all supported platforms.
     * MUST remain byte-identical to the sibling copy in m1-panner.
     */
    struct SharedMemoryHeader
    {
        // Identity / geometry (written by the creating process only)
        uint32_t magic = 0;              // MEMORY_LAYOUT_MAGIC
        uint32_t layoutVersion = 0;      // MEMORY_LAYOUT_VERSION
        uint32_t headerSize = 0;         // sizeof(SharedMemoryHeader)
        uint32_t totalSize = 0;          // total mapped size in bytes
        uint32_t sampleRate = 0;
        uint32_t numChannels = 0;
        uint32_t samplesPerBlock = 0;
        uint32_t ringGeneration = 0;     // incremented whenever the ring is (re)configured
        uint32_t slotCount = 0;          // 0 until initializeForAudio() configures the ring
        uint32_t slotSize = 0;           // bytes per slot
        uint32_t slotsOffset = 0;        // byte offset of slot 0 from mapping base
        uint32_t controlRingOffset = 0;  // byte offset of the control ring from mapping base
        char name[64] = {};

        // Block ring cursor: number of published blocks (single writer)
        std::atomic<uint64_t> writeCursor { 0 };

        // Registered sequential consumers
        std::atomic<uint32_t> consumerCount { 0 };
        uint32_t consumerIds[MAX_CONSUMERS] = {};
        std::atomic<uint64_t> consumerCursors[MAX_CONSUMERS] = {};

        // Control ring indices (helper -> panner)
        std::atomic<uint32_t> controlReadIndex { 0 };
        std::atomic<uint32_t> controlWriteIndex { 0 };
    };

    static_assert(std::atomic<uint64_t>::is_always_lock_free,
                  "shared-memory cursors require lock-free 64-bit atomics");
    static_assert(std::atomic<uint32_t>::is_always_lock_free,
                  "shared-memory indices require lock-free 32-bit atomics");

    /**
     * Control message for bidirectional communication (helper -> panner).
     * Stored in a small ring after the block slots.
     */
    struct ControlMessage
    {
        uint32_t parameterID;
        ParameterType parameterType;
        float floatValue;
        int32_t intValue;

        ControlMessage() : parameterID(0), parameterType(ParameterType::FLOAT), floatValue(0.0f), intValue(0) {}
    };

    /**
     * A fully deserialized audio block read from the ring.
     */
    struct SharedBlock
    {
        juce::AudioBuffer<float> audio;
        ParameterMap parameters;
        uint64_t dawTimestamp = 0;
        double playheadPositionInSeconds = 0.0;
        bool isPlaying = false;
        uint64_t bufferId = 0;            // blockIndex + 1 (0 = invalid)
        uint64_t blockIndex = 0;          // position in the ring's monotonic sequence
        uint32_t sequenceNumber = 0;      // low 32 bits of blockIndex, kept for chunk files
        uint32_t updateSource = 0;
        int64_t startSamplePosition = 0;  // DAW timeline position of the first sample
        uint32_t sampleRate = 0;
        uint64_t droppedBlocksBefore = 0; // blocks lost to ring overrun before this one
    };

    /**
     * @param memoryName Unique name for the shared memory segment (OS-wide)
     * @param totalSize Total size of the shared memory in bytes
     * @param persistent If false, the backing file is deleted in the destructor
     * @param createMode If true, creates a new segment; if false, opens an existing one
     * @param explicitFilePath Optional full path to the memory file (bypasses directory search)
     */
    M1MemoryShare(const std::string& memoryName,
                  size_t totalSize,
                  bool persistent = true,
                  bool createMode = true,
                  const std::string& explicitFilePath = "");

    ~M1MemoryShare();

    /**
     * Configure (or reconfigure) the ring geometry for audio streaming.
     * Only meaningful on the creating side. If the geometry changes, the ring
     * is reset (cursors zeroed, generation bumped). Safe to call repeatedly
     * with the same values (no-op).
     */
    bool initializeForAudio(uint32_t sampleRate, uint32_t numChannels, uint32_t samplesPerBlock);

    //==========================================================================
    // Writer API (panner side)

    /**
     * Serialize one audio block (+ parameter snapshot) into the next ring slot
     * and publish it.
     *
     * @param blockWhenConsumersBehind When true (non-realtime/offline bounce),
     *        blocks until all registered consumers have caught up before
     *        overwriting unread slots, so no blocks are dropped. When false
     *        (realtime), never blocks; a slow consumer simply loses old blocks.
     * @return bufferId (blockIndex + 1) on success, 0 on failure/skip
     */
    uint64_t writeAudioBufferWithGenericParameters(const juce::AudioBuffer<float>& audioBuffer,
                                                   const ParameterMap& parameters,
                                                   uint64_t dawTimestamp,
                                                   double playheadPositionInSeconds,
                                                   bool isPlaying,
                                                   bool blockWhenConsumersBehind = false,
                                                   uint32_t updateSource = 1,
                                                   uint32_t sampleRate = 44100,
                                                   int64_t playheadPositionSamples = -1);

    /** Maximum time a blocking write waits for consumers (default 2000 ms). */
    void setBackpressureTimeoutMs(uint32_t timeoutMs) { m_backpressureTimeoutMs = timeoutMs; }

    //==========================================================================
    // Reader API (helper side)

    /**
     * Register a sequential consumer. Its cursor starts at the current write
     * position (it only sees blocks published after registration).
     */
    bool registerConsumer(uint32_t consumerId);
    bool unregisterConsumer(uint32_t consumerId);
    bool isConsumerRegistered(uint32_t consumerId) const;

    /**
     * Read the next unread block for a registered consumer and advance its
     * cursor. Detects ring overruns (reported via out.droppedBlocksBefore).
     * @return true if a block was read, false if no new data is available
     */
    bool readNextBlockForConsumer(uint32_t consumerId, SharedBlock& out);

    /**
     * Read the most recently published block without consuming anything.
     * Used for parameter/status polling; consumer cursors are unaffected.
     */
    bool readLatestBlock(SharedBlock& out);

    //==========================================================================
    // Control messages (helper -> panner)

    bool writeControlMessage(uint32_t parameterID, ParameterType type, float floatValue, int32_t intValue = 0);
    bool readControlMessage(ControlMessage& outMessage);

    //==========================================================================
    // Introspection

    bool isValid() const;
    bool isRingConfigured() const;
    uint64_t getWriteCursor() const;
    uint32_t getSlotCount() const;
    uint32_t getRingGeneration() const;
    bool getConsumerCursor(uint32_t consumerId, uint64_t& outCursor) const;

    /** Number of registered sequential consumers (0 = nobody drains this ring). */
    uint32_t getConsumerCount() const;

    /** Async (message-thread) file modification time bump so directory scans
        can tell live segments from stale ones without touching the RT path. */
    void scheduleAsyncFileModTimeUpdate();

    /** Delete a shared memory segment file by name. */
    static bool deleteSharedMemory(const juce::String& memoryName);

private:
    std::string m_memoryName;
    size_t m_totalSize;
    bool m_persistent;
    bool m_createMode;
    std::string m_explicitFilePath;

    std::unique_ptr<juce::MemoryMappedFile> m_mappedFile;
    juce::File m_tempFile;

    SharedMemoryHeader* m_header = nullptr;
    size_t m_mappedSize = 0;

    // Serializes in-process writers (audio thread vs. parameter-only timer).
    std::mutex m_writerMutex;
    // Serializes in-process readers (tracker poll vs. capture thread) and
    // guards consumer registration.
    mutable std::mutex m_readerMutex;

    uint32_t m_backpressureTimeoutMs = 2000;
    std::atomic<uint32_t> m_modTimeWriteCounter { 0 };

    bool createSharedMemoryFile();
    bool openSharedMemoryFile();
    bool setupMemoryPointers();

    uint8_t* basePtr() const;
    uint8_t* slotPointer(uint64_t blockIndex) const;
    ControlMessage* controlRing() const;

    int findConsumerIndex(uint32_t consumerId) const;
    uint64_t minimumConsumerCursor() const;
    void waitForConsumersToCatchUp(uint64_t blockIndex);

    size_t computeSerializedParameterBytes(const ParameterMap& parameters) const;
    size_t serializeBlock(uint8_t* dst,
                          size_t capacity,
                          const juce::AudioBuffer<float>& audioBuffer,
                          const ParameterMap& parameters,
                          uint64_t dawTimestamp,
                          double playheadPositionInSeconds,
                          bool isPlaying,
                          uint32_t updateSource,
                          uint32_t sampleRate,
                          uint64_t blockIndex,
                          int64_t playheadPositionSamples);
    bool parseBlock(const uint8_t* data, size_t size, SharedBlock& out) const;
    bool copySlotToScratch(uint64_t blockIndex, std::vector<uint8_t>& scratch) const;

    // Prevent copying
    M1MemoryShare(const M1MemoryShare&) = delete;
    M1MemoryShare& operator=(const M1MemoryShare&) = delete;
};
