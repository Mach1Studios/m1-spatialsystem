#include "M1MemoryShare.h"
#include "SharedPathUtils.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <limits>
#include <new>
#include <thread>

//==============================================================================
M1MemoryShare::M1MemoryShare(const std::string& memoryName,
                             size_t totalSize,
                             bool persistent,
                             bool createMode,
                             const std::string& explicitFilePath)
    : m_memoryName(memoryName)
    , m_totalSize(totalSize)
    , m_persistent(persistent)
    , m_createMode(createMode)
    , m_explicitFilePath(explicitFilePath)
{
    // Enough for the header, the control ring, and a useful number of slots.
    const size_t minSize = ((sizeof(SharedMemoryHeader) + 63u) & ~size_t(63))
                         + MAX_CONTROL_MESSAGES * sizeof(ControlMessage)
                         + 64 * 1024;
    if (m_totalSize < minSize)
        m_totalSize = minSize;

    const bool fileReady = m_createMode ? createSharedMemoryFile() : openSharedMemoryFile();
    if (!fileReady)
    {
        std::cerr << "[M1MemoryShare] Failed to " << (m_createMode ? "create" : "open")
                  << " shared memory: " << m_memoryName << std::endl;
        return;
    }

    if (!setupMemoryPointers())
    {
        std::cerr << "[M1MemoryShare] Invalid or incompatible shared memory layout: "
                  << m_memoryName << std::endl;
        m_header = nullptr;
        m_mappedFile.reset();
    }
}

M1MemoryShare::~M1MemoryShare()
{
    m_mappedFile.reset();

    if (!m_persistent && m_tempFile.exists())
        m_tempFile.deleteFile();
}

//==============================================================================
bool M1MemoryShare::createSharedMemoryFile()
{
    if (!m_explicitFilePath.empty())
    {
        m_tempFile = juce::File(m_explicitFilePath);
    }
    else
    {
        // Use SharedPathUtils to get the shared memory directory (App Group or fallback)
        std::string sharedDir = Mach1::SharedPathUtils::getSharedMemoryDirectory();
        if (sharedDir.empty())
            sharedDir = juce::File::getSpecialLocation(juce::File::tempDirectory).getFullPathName().toStdString();

        juce::File memDir(sharedDir);
        if (!memDir.exists())
            memDir.createDirectory();

        // Memory name already includes the full filename prefix (e.g. M1SpatialSystem_M1Panner_...)
        m_tempFile = memDir.getChildFile(m_memoryName + ".mem");
    }

    if (!m_tempFile.create())
    {
        std::cerr << "[M1MemoryShare] Failed to create file: " << m_tempFile.getFullPathName() << std::endl;
        return false;
    }

    {
        juce::FileOutputStream fileStream(m_tempFile);
        if (!fileStream.openedOk())
        {
            std::cerr << "[M1MemoryShare] Failed to open file for writing" << std::endl;
            return false;
        }

        std::vector<uint8_t> zeros(8192, 0);
        size_t bytesWritten = 0;
        while (bytesWritten < m_totalSize)
        {
            const size_t chunk = std::min(zeros.size(), m_totalSize - bytesWritten);
            fileStream.write(zeros.data(), chunk);
            bytesWritten += chunk;
        }
        fileStream.flush();
    }

    m_mappedFile = std::make_unique<juce::MemoryMappedFile>(m_tempFile, juce::MemoryMappedFile::readWrite);
    if (!m_mappedFile->getData())
    {
        std::cerr << "[M1MemoryShare] Failed to memory map file" << std::endl;
        return false;
    }

    return true;
}

bool M1MemoryShare::openSharedMemoryFile()
{
    // If explicit file path provided, use it directly
    if (!m_explicitFilePath.empty())
    {
        m_tempFile = juce::File(m_explicitFilePath);
        if (!m_tempFile.exists())
        {
            std::cerr << "[M1MemoryShare] Explicit file path does not exist: " << m_explicitFilePath << std::endl;
            return false;
        }

        m_mappedFile = std::make_unique<juce::MemoryMappedFile>(m_tempFile, juce::MemoryMappedFile::readWrite);
        if (!m_mappedFile->getData())
        {
            std::cerr << "[M1MemoryShare] Failed to memory map explicit file: " << m_tempFile.getFullPathName() << std::endl;
            return false;
        }
        return true;
    }

    // Otherwise search all possible directories for the file
    auto directories = Mach1::SharedPathUtils::getAllSharedDirectories();
    directories.push_back(juce::File::getSpecialLocation(juce::File::tempDirectory).getFullPathName().toStdString());

    for (const auto& dirPath : directories)
    {
        juce::File candidate = juce::File(dirPath).getChildFile(m_memoryName + ".mem");
        if (!candidate.exists())
            continue;

        m_tempFile = candidate;
        m_mappedFile = std::make_unique<juce::MemoryMappedFile>(m_tempFile, juce::MemoryMappedFile::readWrite);
        if (m_mappedFile->getData())
            return true;

        std::cerr << "[M1MemoryShare] Failed to memory map file: " << m_tempFile.getFullPathName() << std::endl;
    }

    std::cerr << "[M1MemoryShare] Shared memory file not found in any directory: " << m_memoryName << ".mem" << std::endl;
    return false;
}

bool M1MemoryShare::setupMemoryPointers()
{
    if (!m_mappedFile || !m_mappedFile->getData())
        return false;

    m_mappedSize = m_mappedFile->getSize();
    if (m_mappedSize < sizeof(SharedMemoryHeader))
        return false;

    m_header = reinterpret_cast<SharedMemoryHeader*>(m_mappedFile->getData());

    if (m_createMode)
    {
        new (m_header) SharedMemoryHeader();
        m_header->magic = MEMORY_LAYOUT_MAGIC;
        m_header->layoutVersion = MEMORY_LAYOUT_VERSION;
        m_header->headerSize = static_cast<uint32_t>(sizeof(SharedMemoryHeader));
        m_header->totalSize = static_cast<uint32_t>(m_mappedSize);
        std::strncpy(m_header->name, m_memoryName.c_str(), sizeof(m_header->name) - 1);
        return true;
    }

    // Opening an existing segment: reject unknown/legacy layouts.
    if (m_header->magic != MEMORY_LAYOUT_MAGIC
        || m_header->layoutVersion != MEMORY_LAYOUT_VERSION
        || m_header->headerSize != sizeof(SharedMemoryHeader)
        || m_header->totalSize > m_mappedSize)
    {
        return false;
    }

    return true;
}

//==============================================================================
bool M1MemoryShare::initializeForAudio(uint32_t sampleRate, uint32_t numChannels, uint32_t samplesPerBlock)
{
    if (!isValid())
        return false;

    m_header->sampleRate = sampleRate;
    m_header->numChannels = numChannels;
    m_header->samplesPerBlock = samplesPerBlock;

    if (!m_createMode)
        return true; // only the creating side owns the ring geometry

    const uint32_t channels = juce::jlimit(1u, 32u, numChannels == 0 ? 2u : numChannels);
    const uint32_t slotSamples = std::max(MIN_SLOT_SAMPLES, samplesPerBlock);

    uint32_t slotSize = static_cast<uint32_t>(sizeof(GenericAudioBufferHeader))
                      + MAX_PARAMETER_BYTES
                      + channels * slotSamples * static_cast<uint32_t>(sizeof(float));
    slotSize = (slotSize + 511u) & ~511u;

    const uint32_t slotsOffset = (static_cast<uint32_t>(sizeof(SharedMemoryHeader)) + 63u) & ~63u;
    const uint32_t controlRingBytes = MAX_CONTROL_MESSAGES * static_cast<uint32_t>(sizeof(ControlMessage));
    const uint32_t controlRingOffset = static_cast<uint32_t>((m_mappedSize - controlRingBytes) & ~size_t(7));

    if (controlRingOffset <= slotsOffset)
        return false;

    uint32_t slotCount = (controlRingOffset - slotsOffset) / slotSize;
    if (slotCount < MIN_SLOT_COUNT)
    {
        std::cerr << "[M1MemoryShare] Segment too small for ring: slotSize=" << slotSize
                  << " available=" << (controlRingOffset - slotsOffset) << std::endl;
        return false;
    }
    slotCount = std::min(slotCount, MAX_SLOT_COUNT);

    if (m_header->slotCount == slotCount
        && m_header->slotSize == slotSize
        && m_header->slotsOffset == slotsOffset
        && m_header->controlRingOffset == controlRingOffset)
    {
        return true; // geometry unchanged; keep cursors intact
    }

    // (Re)configure the ring: reset cursors so readers restart cleanly.
    m_header->slotsOffset = slotsOffset;
    m_header->controlRingOffset = controlRingOffset;
    m_header->slotSize = slotSize;
    m_header->slotCount = slotCount;
    m_header->writeCursor.store(0, std::memory_order_release);
    for (uint32_t i = 0; i < MAX_CONSUMERS; ++i)
        m_header->consumerCursors[i].store(0, std::memory_order_release);
    m_header->ringGeneration++;

    return true;
}

//==============================================================================
// Writer

uint64_t M1MemoryShare::writeAudioBufferWithGenericParameters(const juce::AudioBuffer<float>& audioBuffer,
                                                              const ParameterMap& parameters,
                                                              uint64_t dawTimestamp,
                                                              double playheadPositionInSeconds,
                                                              bool isPlaying,
                                                              bool blockWhenConsumersBehind,
                                                              uint32_t updateSource,
                                                              uint32_t sampleRate)
{
    if (!isRingConfigured())
        return 0;

    std::unique_lock<std::mutex> lock(m_writerMutex, std::defer_lock);
    if (blockWhenConsumersBehind)
        lock.lock();
    else if (!lock.try_lock())
        return 0; // another in-process writer is active; the realtime path must not block

    const uint64_t blockIndex = m_header->writeCursor.load(std::memory_order_relaxed);

    if (blockWhenConsumersBehind)
        waitForConsumersToCatchUp(blockIndex);

    uint8_t* slot = slotPointer(blockIndex);
    const size_t written = serializeBlock(slot, m_header->slotSize,
                                          audioBuffer, parameters,
                                          dawTimestamp, playheadPositionInSeconds, isPlaying,
                                          updateSource, sampleRate, blockIndex);
    if (written == 0)
        return 0;

    m_header->writeCursor.store(blockIndex + 1, std::memory_order_release);

    // Bump the backing file's mtime occasionally so directory scans can tell
    // live segments from stale ones (done async, never on the audio thread).
    if (++m_modTimeWriteCounter % 50 == 0)
        scheduleAsyncFileModTimeUpdate();

    return blockIndex + 1;
}

void M1MemoryShare::waitForConsumersToCatchUp(uint64_t blockIndex)
{
    const uint32_t slotCount = m_header->slotCount;

    if (m_header->consumerCount.load(std::memory_order_acquire) == 0)
        return;

    // After publishing blockIndex the oldest still-readable block becomes
    // (blockIndex + 2 - slotCount); anything older is either overwritten by
    // this write or unreadable under the torn-read window. Wait until every
    // registered consumer has consumed past that point.
    if (blockIndex + 2 <= slotCount)
        return;
    const uint64_t requiredCursor = blockIndex + 2 - slotCount;

    const auto deadline = std::chrono::steady_clock::now()
                        + std::chrono::milliseconds(m_backpressureTimeoutMs);
    while (minimumConsumerCursor() < requiredCursor)
    {
        if (std::chrono::steady_clock::now() >= deadline)
        {
            // Consumer stalled or died: proceed (and drop) rather than hang the render.
            std::cerr << "[M1MemoryShare] Backpressure timeout waiting for consumers ("
                      << m_memoryName << ")" << std::endl;
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

uint64_t M1MemoryShare::minimumConsumerCursor() const
{
    const uint32_t count = std::min(m_header->consumerCount.load(std::memory_order_acquire), MAX_CONSUMERS);
    uint64_t minCursor = std::numeric_limits<uint64_t>::max();
    for (uint32_t i = 0; i < count; ++i)
        minCursor = std::min(minCursor, m_header->consumerCursors[i].load(std::memory_order_acquire));
    return minCursor;
}

//==============================================================================
// Consumers

bool M1MemoryShare::registerConsumer(uint32_t consumerId)
{
    if (!isValid())
        return false;

    std::lock_guard<std::mutex> lock(m_readerMutex);

    if (findConsumerIndex(consumerId) >= 0)
        return true;

    const uint32_t count = m_header->consumerCount.load(std::memory_order_relaxed);
    if (count >= MAX_CONSUMERS)
    {
        std::cerr << "[M1MemoryShare] Maximum consumers reached" << std::endl;
        return false;
    }

    // Publish id + cursor before the count so the writer never scans
    // an uninitialized entry.
    m_header->consumerIds[count] = consumerId;
    m_header->consumerCursors[count].store(m_header->writeCursor.load(std::memory_order_acquire),
                                           std::memory_order_release);
    m_header->consumerCount.store(count + 1, std::memory_order_release);

    return true;
}

bool M1MemoryShare::unregisterConsumer(uint32_t consumerId)
{
    if (!isValid())
        return false;

    std::lock_guard<std::mutex> lock(m_readerMutex);

    const int index = findConsumerIndex(consumerId);
    if (index < 0)
        return false;

    const uint32_t count = m_header->consumerCount.load(std::memory_order_relaxed);
    for (uint32_t i = static_cast<uint32_t>(index); i + 1 < count; ++i)
    {
        m_header->consumerIds[i] = m_header->consumerIds[i + 1];
        m_header->consumerCursors[i].store(m_header->consumerCursors[i + 1].load(std::memory_order_relaxed),
                                           std::memory_order_relaxed);
    }
    m_header->consumerCount.store(count - 1, std::memory_order_release);

    return true;
}

bool M1MemoryShare::isConsumerRegistered(uint32_t consumerId) const
{
    if (!isValid())
        return false;
    return findConsumerIndex(consumerId) >= 0;
}

int M1MemoryShare::findConsumerIndex(uint32_t consumerId) const
{
    const uint32_t count = std::min(m_header->consumerCount.load(std::memory_order_acquire), MAX_CONSUMERS);
    for (uint32_t i = 0; i < count; ++i)
    {
        if (m_header->consumerIds[i] == consumerId)
            return static_cast<int>(i);
    }
    return -1;
}

//==============================================================================
// Readers

bool M1MemoryShare::readNextBlockForConsumer(uint32_t consumerId, SharedBlock& out)
{
    if (!isRingConfigured())
        return false;

    std::lock_guard<std::mutex> lock(m_readerMutex);

    const int idx = findConsumerIndex(consumerId);
    if (idx < 0)
        return false;

    const uint32_t slotCount = m_header->slotCount;
    std::vector<uint8_t> scratch;
    uint64_t dropped = 0;

    for (int attempt = 0; attempt < 16; ++attempt)
    {
        const uint64_t wc = m_header->writeCursor.load(std::memory_order_acquire);
        uint64_t rc = m_header->consumerCursors[idx].load(std::memory_order_relaxed);

        if (rc > wc)
        {
            // The ring was reset (geometry re-init); restart from the head.
            rc = wc;
            m_header->consumerCursors[idx].store(rc, std::memory_order_release);
        }

        if (rc >= wc)
            return false; // nothing new

        // The writer may currently be filling slot (wc % slotCount), which
        // aliases block (wc - slotCount): the safe window is (wc - slotCount, wc).
        if (wc >= slotCount && rc <= wc - slotCount)
        {
            const uint64_t skipTo = wc - slotCount + 1;
            dropped += skipTo - rc;
            rc = skipTo;
        }

        if (!copySlotToScratch(rc, scratch))
            return false;

        // Torn-read check: if the writer lapped this block during the copy, retry.
        const uint64_t wcAfter = m_header->writeCursor.load(std::memory_order_acquire);
        if (wcAfter >= rc + slotCount)
        {
            m_header->consumerCursors[idx].store(rc, std::memory_order_release);
            continue;
        }

        if (!parseBlock(scratch.data(), scratch.size(), out) || out.bufferId != rc + 1)
        {
            // Corrupt or stale slot; count it as dropped and move on.
            ++dropped;
            m_header->consumerCursors[idx].store(rc + 1, std::memory_order_release);
            continue;
        }

        out.blockIndex = rc;
        out.droppedBlocksBefore = dropped;
        m_header->consumerCursors[idx].store(rc + 1, std::memory_order_release);
        return true;
    }

    return false;
}

bool M1MemoryShare::readLatestBlock(SharedBlock& out)
{
    if (!isRingConfigured())
        return false;

    std::lock_guard<std::mutex> lock(m_readerMutex);

    const uint32_t slotCount = m_header->slotCount;
    std::vector<uint8_t> scratch;

    for (int attempt = 0; attempt < 16; ++attempt)
    {
        const uint64_t wc = m_header->writeCursor.load(std::memory_order_acquire);
        if (wc == 0)
            return false;

        const uint64_t idx = wc - 1;
        if (!copySlotToScratch(idx, scratch))
            return false;

        const uint64_t wcAfter = m_header->writeCursor.load(std::memory_order_acquire);
        if (wcAfter >= idx + slotCount)
            continue; // lapped mid-copy; retry with the newer head

        if (!parseBlock(scratch.data(), scratch.size(), out) || out.bufferId != idx + 1)
            continue;

        out.blockIndex = idx;
        out.droppedBlocksBefore = 0;
        return true;
    }

    return false;
}

//==============================================================================
// Serialization

size_t M1MemoryShare::computeSerializedParameterBytes(const ParameterMap& parameters) const
{
    size_t bytes = 0;
    bytes += parameters.floatParams.size() * (sizeof(GenericParameter) + sizeof(float));
    bytes += parameters.intParams.size() * (sizeof(GenericParameter) + sizeof(int32_t));
    bytes += parameters.boolParams.size() * (sizeof(GenericParameter) + sizeof(bool));
    bytes += parameters.doubleParams.size() * (sizeof(GenericParameter) + sizeof(double));
    bytes += parameters.uint32Params.size() * (sizeof(GenericParameter) + sizeof(uint32_t));
    bytes += parameters.uint64Params.size() * (sizeof(GenericParameter) + sizeof(uint64_t));
    for (const auto& pair : parameters.stringParams)
        bytes += sizeof(GenericParameter) + pair.second.length() + 1;
    return bytes;
}

size_t M1MemoryShare::serializeBlock(uint8_t* dst,
                                     size_t capacity,
                                     const juce::AudioBuffer<float>& audioBuffer,
                                     const ParameterMap& parameters,
                                     uint64_t dawTimestamp,
                                     double playheadPositionInSeconds,
                                     bool isPlaying,
                                     uint32_t updateSource,
                                     uint32_t sampleRate,
                                     uint64_t blockIndex)
{
    const uint32_t channels = static_cast<uint32_t>(std::max(0, audioBuffer.getNumChannels()));
    const uint32_t samples = static_cast<uint32_t>(std::max(0, audioBuffer.getNumSamples()));

    size_t headerBytes = sizeof(GenericAudioBufferHeader) + computeSerializedParameterBytes(parameters);
    headerBytes = (headerBytes + 3u) & ~size_t(3); // keep the audio region float-aligned
    const size_t audioBytes = static_cast<size_t>(channels) * samples * sizeof(float);

    if (headerBytes + audioBytes > capacity)
        return 0;

    GenericAudioBufferHeader header;
    header.version = 1;
    header.channels = channels;
    header.samples = samples;
    header.dawTimestamp = dawTimestamp;
    header.playheadPositionInSeconds = playheadPositionInSeconds;
    header.isPlaying = isPlaying ? 1 : 0;
    header.parameterCount = static_cast<uint32_t>(parameters.floatParams.size()
                                                  + parameters.intParams.size()
                                                  + parameters.boolParams.size()
                                                  + parameters.stringParams.size()
                                                  + parameters.doubleParams.size()
                                                  + parameters.uint32Params.size()
                                                  + parameters.uint64Params.size());
    header.headerSize = static_cast<uint32_t>(headerBytes);
    header.updateSource = updateSource;
    header.isUpdatingFromExternal = 0;
    header.bufferId = blockIndex + 1;
    header.sequenceNumber = static_cast<uint32_t>(blockIndex);
    header.bufferTimestamp = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    header.requiresAcknowledgment = 0;
    header.consumerCount = m_header->consumerCount.load(std::memory_order_relaxed);
    header.acknowledgedCount = 0;
    header.sampleRate = sampleRate;
    header.startSamplePosition = static_cast<int64_t>(playheadPositionInSeconds * static_cast<double>(sampleRate));

    std::memcpy(dst, &header, sizeof(header));

    uint8_t* p = dst + sizeof(GenericAudioBufferHeader);
    auto writeParam = [&p](uint32_t id, ParameterType type, const void* payload, uint32_t payloadBytes)
    {
        GenericParameter descriptor(id, type, payloadBytes);
        std::memcpy(p, &descriptor, sizeof(descriptor));
        p += sizeof(descriptor);
        std::memcpy(p, payload, payloadBytes);
        p += payloadBytes;
    };

    for (const auto& pair : parameters.floatParams)
        writeParam(pair.first, ParameterType::FLOAT, &pair.second, sizeof(float));
    for (const auto& pair : parameters.intParams)
        writeParam(pair.first, ParameterType::INT, &pair.second, sizeof(int32_t));
    for (const auto& pair : parameters.boolParams)
    {
        const uint8_t value = pair.second ? 1 : 0;
        writeParam(pair.first, ParameterType::BOOL, &value, sizeof(uint8_t));
    }
    for (const auto& pair : parameters.stringParams)
        writeParam(pair.first, ParameterType::STRING, pair.second.c_str(),
                   static_cast<uint32_t>(pair.second.length() + 1));
    for (const auto& pair : parameters.doubleParams)
        writeParam(pair.first, ParameterType::DOUBLE, &pair.second, sizeof(double));
    for (const auto& pair : parameters.uint32Params)
        writeParam(pair.first, ParameterType::UINT32, &pair.second, sizeof(uint32_t));
    for (const auto& pair : parameters.uint64Params)
        writeParam(pair.first, ParameterType::UINT64, &pair.second, sizeof(uint64_t));

    // Zero the alignment padding between the parameters and the audio region.
    const size_t writtenHeader = static_cast<size_t>(p - dst);
    if (writtenHeader < headerBytes)
        std::memset(p, 0, headerBytes - writtenHeader);

    // Interleaved audio
    if (audioBytes > 0)
    {
        float* audioOut = reinterpret_cast<float*>(dst + headerBytes);
        const float* const* channelData = audioBuffer.getArrayOfReadPointers();
        for (uint32_t s = 0; s < samples; ++s)
            for (uint32_t ch = 0; ch < channels; ++ch)
                audioOut[s * channels + ch] = channelData[ch][s];
    }

    return headerBytes + audioBytes;
}

bool M1MemoryShare::parseBlock(const uint8_t* data, size_t size, SharedBlock& out) const
{
    if (size < sizeof(GenericAudioBufferHeader))
        return false;

    GenericAudioBufferHeader header;
    std::memcpy(&header, data, sizeof(header));

    if (header.version != 1
        || header.channels > 32
        || header.samples > 65536
        || header.parameterCount > 256
        || header.headerSize < sizeof(GenericAudioBufferHeader)
        || header.headerSize > size
        || (header.headerSize & 3) != 0)
    {
        return false;
    }

    const size_t audioBytes = static_cast<size_t>(header.channels) * header.samples * sizeof(float);
    if (header.headerSize + audioBytes > size)
        return false;

    out.parameters.clear();
    out.dawTimestamp = header.dawTimestamp;
    out.playheadPositionInSeconds = header.playheadPositionInSeconds;
    out.isPlaying = header.isPlaying != 0;
    out.bufferId = header.bufferId;
    out.sequenceNumber = header.sequenceNumber;
    out.updateSource = header.updateSource;
    out.startSamplePosition = header.startSamplePosition;
    out.sampleRate = header.sampleRate;

    const uint8_t* p = data + sizeof(GenericAudioBufferHeader);
    const uint8_t* paramEnd = data + header.headerSize;

    for (uint32_t i = 0; i < header.parameterCount; ++i)
    {
        if (p + sizeof(GenericParameter) > paramEnd)
            return false;

        GenericParameter descriptor;
        std::memcpy(&descriptor, p, sizeof(descriptor));
        p += sizeof(descriptor);

        if (descriptor.dataSize > static_cast<size_t>(paramEnd - p))
            return false;

        switch (descriptor.parameterType)
        {
            case ParameterType::FLOAT:
            {
                float value;
                if (descriptor.dataSize < sizeof(value)) return false;
                std::memcpy(&value, p, sizeof(value));
                out.parameters.addFloat(descriptor.parameterID, value);
                break;
            }
            case ParameterType::INT:
            {
                int32_t value;
                if (descriptor.dataSize < sizeof(value)) return false;
                std::memcpy(&value, p, sizeof(value));
                out.parameters.addInt(descriptor.parameterID, value);
                break;
            }
            case ParameterType::BOOL:
            {
                if (descriptor.dataSize < 1) return false;
                out.parameters.addBool(descriptor.parameterID, p[0] != 0);
                break;
            }
            case ParameterType::STRING:
            {
                const char* chars = reinterpret_cast<const char*>(p);
                const size_t maxLen = strnlen(chars, descriptor.dataSize);
                out.parameters.addString(descriptor.parameterID, std::string(chars, maxLen));
                break;
            }
            case ParameterType::DOUBLE:
            {
                double value;
                if (descriptor.dataSize < sizeof(value)) return false;
                std::memcpy(&value, p, sizeof(value));
                out.parameters.addDouble(descriptor.parameterID, value);
                break;
            }
            case ParameterType::UINT32:
            {
                uint32_t value;
                if (descriptor.dataSize < sizeof(value)) return false;
                std::memcpy(&value, p, sizeof(value));
                out.parameters.addUInt32(descriptor.parameterID, value);
                break;
            }
            case ParameterType::UINT64:
            {
                uint64_t value;
                if (descriptor.dataSize < sizeof(value)) return false;
                std::memcpy(&value, p, sizeof(value));
                out.parameters.addUInt64(descriptor.parameterID, value);
                break;
            }
            default:
                return false;
        }

        p += descriptor.dataSize;
    }

    // Deinterleave audio
    const int numChannels = static_cast<int>(header.channels);
    const int numSamples = static_cast<int>(header.samples);
    out.audio.setSize(numChannels, numSamples, false, true, true);
    if (numChannels > 0 && numSamples > 0)
    {
        const float* audioData = reinterpret_cast<const float*>(data + header.headerSize);
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* dstChannel = out.audio.getWritePointer(ch);
            for (int s = 0; s < numSamples; ++s)
                dstChannel[s] = audioData[static_cast<size_t>(s) * numChannels + ch];
        }
    }

    return true;
}

bool M1MemoryShare::copySlotToScratch(uint64_t blockIndex, std::vector<uint8_t>& scratch) const
{
    const uint32_t slotSize = m_header->slotSize;
    scratch.resize(slotSize);
    std::memcpy(scratch.data(), slotPointer(blockIndex), slotSize);
    return true;
}

//==============================================================================
// Control messages

bool M1MemoryShare::writeControlMessage(uint32_t parameterID, ParameterType type, float floatValue, int32_t intValue)
{
    if (!isRingConfigured())
        return false;

    const uint32_t writeIndex = m_header->controlWriteIndex.load(std::memory_order_relaxed);
    const uint32_t readIndex = m_header->controlReadIndex.load(std::memory_order_acquire);
    if (writeIndex - readIndex >= MAX_CONTROL_MESSAGES)
        return false; // ring full; drop

    ControlMessage& message = controlRing()[writeIndex % MAX_CONTROL_MESSAGES];
    message.parameterID = parameterID;
    message.parameterType = type;
    message.floatValue = floatValue;
    message.intValue = intValue;

    m_header->controlWriteIndex.store(writeIndex + 1, std::memory_order_release);
    return true;
}

bool M1MemoryShare::readControlMessage(ControlMessage& outMessage)
{
    if (!isRingConfigured())
        return false;

    const uint32_t readIndex = m_header->controlReadIndex.load(std::memory_order_relaxed);
    const uint32_t writeIndex = m_header->controlWriteIndex.load(std::memory_order_acquire);
    if (readIndex >= writeIndex)
        return false; // no pending messages

    outMessage = controlRing()[readIndex % MAX_CONTROL_MESSAGES];
    m_header->controlReadIndex.store(readIndex + 1, std::memory_order_release);
    return true;
}

//==============================================================================
// Introspection

bool M1MemoryShare::isValid() const
{
    return m_mappedFile != nullptr
        && m_mappedFile->getData() != nullptr
        && m_header != nullptr;
}

bool M1MemoryShare::isRingConfigured() const
{
    if (!isValid())
        return false;

    const uint32_t slotCount = m_header->slotCount;
    const uint32_t slotSize = m_header->slotSize;
    const uint32_t slotsOffset = m_header->slotsOffset;

    if (slotCount == 0 || slotSize == 0 || slotCount > MAX_SLOT_COUNT)
        return false;
    if (slotsOffset < sizeof(SharedMemoryHeader))
        return false;
    if (slotsOffset + static_cast<uint64_t>(slotCount) * slotSize > m_mappedSize)
        return false;
    if (m_header->controlRingOffset + MAX_CONTROL_MESSAGES * sizeof(ControlMessage) > m_mappedSize)
        return false;

    return true;
}

uint64_t M1MemoryShare::getWriteCursor() const
{
    return isValid() ? m_header->writeCursor.load(std::memory_order_acquire) : 0;
}

uint32_t M1MemoryShare::getSlotCount() const
{
    return isValid() ? m_header->slotCount : 0;
}

uint32_t M1MemoryShare::getRingGeneration() const
{
    return isValid() ? m_header->ringGeneration : 0;
}

bool M1MemoryShare::getConsumerCursor(uint32_t consumerId, uint64_t& outCursor) const
{
    if (!isValid())
        return false;
    const int idx = findConsumerIndex(consumerId);
    if (idx < 0)
        return false;
    outCursor = m_header->consumerCursors[idx].load(std::memory_order_acquire);
    return true;
}

uint32_t M1MemoryShare::getConsumerCount() const
{
    if (!isValid())
        return 0;
    return m_header->consumerCount.load(std::memory_order_acquire);
}

uint8_t* M1MemoryShare::basePtr() const
{
    return static_cast<uint8_t*>(m_mappedFile->getData());
}

uint8_t* M1MemoryShare::slotPointer(uint64_t blockIndex) const
{
    return basePtr() + m_header->slotsOffset
         + (blockIndex % m_header->slotCount) * static_cast<uint64_t>(m_header->slotSize);
}

M1MemoryShare::ControlMessage* M1MemoryShare::controlRing() const
{
    return reinterpret_cast<ControlMessage*>(basePtr() + m_header->controlRingOffset);
}

void M1MemoryShare::scheduleAsyncFileModTimeUpdate()
{
    if (!m_tempFile.exists())
        return;

    // Copy the file handle so the callback never touches a destroyed instance.
    juce::File file = m_tempFile;
    juce::MessageManager::callAsync([file]() {
        if (file.exists())
            file.setLastModificationTime(juce::Time::getCurrentTime());
    });
}

//==============================================================================
bool M1MemoryShare::deleteSharedMemory(const juce::String& memoryName)
{
    // Search all possible directories for the file
    auto directories = Mach1::SharedPathUtils::getAllSharedDirectories();
    directories.push_back(juce::File::getSpecialLocation(juce::File::tempDirectory).getFullPathName().toStdString());

    bool deleted = false;
    for (const auto& dirPath : directories)
    {
        juce::File memoryFile = juce::File(dirPath).getChildFile(memoryName + ".mem");
        if (memoryFile.exists())
            deleted = memoryFile.deleteFile() || deleted;
    }

    return deleted || true; // consider it deleted if not found
}
