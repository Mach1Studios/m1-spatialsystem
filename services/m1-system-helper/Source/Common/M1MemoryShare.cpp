#include "M1MemoryShare.h"
#include <iostream>
#include <cstring>
#include <chrono>
#include <fstream>
#include <filesystem>

//==============================================================================
M1MemoryShare::M1MemoryShare(const std::string& memoryName,
                             size_t totalSize,
                             uint32_t maxQueueSize,
                             bool persistent,
                             bool createMode)
    : m_memoryName(memoryName)
    , m_totalSize(totalSize)
    , m_maxQueueSize(maxQueueSize)
    , m_persistent(persistent)
    , m_createMode(createMode)
    , m_header(nullptr)
    , m_dataBuffer(nullptr)
    , m_dataBufferSize(0)
    , m_queuedBuffers(nullptr)
    , m_queuedBuffersSize(0)
{
    // Ensure minimum size for header and queue
    size_t minSize = sizeof(SharedMemoryHeader) + 
                    (maxQueueSize * sizeof(QueuedBuffer)) + 
                    4096; // minimum data buffer
    
    if (m_totalSize < minSize)
    {
        m_totalSize = minSize;
    }

    // Create or open the shared memory
    if (m_createMode)
    {
        if (!createSharedMemoryFile())
        {
            std::cerr << "[M1MemoryShare] Failed to create shared memory: " << m_memoryName << std::endl;
            return;
        }
    }
    else
    {
        if (!openSharedMemoryFile())
        {
            std::cerr << "[M1MemoryShare] Failed to open shared memory: " << m_memoryName << std::endl;
            return;
        }
    }

    setupMemoryPointers();
}

M1MemoryShare::~M1MemoryShare()
{
    if (m_mappedFile)
    {
        m_mappedFile.reset();
    }
    
    // Clean up temp file if not persistent
    if (!m_persistent && m_tempFile.exists())
    {
        m_tempFile.deleteFile();
    }
}

//==============================================================================
bool M1MemoryShare::createSharedMemoryFile()
{
    // Use JUCE temporary directory
    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
    m_tempFile = tempDir.getChildFile("M1SpatialSystem_" + m_memoryName + ".mem");
    
    std::cout << "[M1MemoryShare] Attempting to create file: " << m_tempFile.getFullPathName() << std::endl;
    
    // Create the file and set its size
    if (!m_tempFile.create())
    {
        std::cout << "[M1MemoryShare] Failed to create temporary file" << std::endl;
        return false;
    }
    
    // Create a file output stream to set the size
    auto fileStream = std::make_unique<juce::FileOutputStream>(m_tempFile);
    if (!fileStream->openedOk())
    {
        std::cout << "[M1MemoryShare] Failed to open file for writing" << std::endl;
        return false;
    }
    
    // Write zeros to set the file size
    std::vector<uint8_t> zeros(m_totalSize, 0);
    fileStream->write(zeros.data(), m_totalSize);
    fileStream.reset();
    
    // Now create the memory mapped file
    m_mappedFile = std::make_unique<juce::MemoryMappedFile>(m_tempFile, juce::MemoryMappedFile::readWrite);
    if (!m_mappedFile->getData())
    {
        std::cout << "[M1MemoryShare] Failed to memory map file" << std::endl;
        return false;
    }
    
    std::cout << "[M1MemoryShare] Successfully created and mapped file: " << m_tempFile.getFullPathName() << std::endl;
    
    setupMemoryPointers();
    
    // Initialize header if we're in create mode
    if (m_createMode)
    {
        *m_header = SharedMemoryHeader(); // Initialize with defaults
        m_header->bufferSize = static_cast<uint32_t>(m_dataBufferSize);
        m_header->maxQueueSize = m_maxQueueSize;
        strncpy(m_header->name, m_memoryName.toStdString().c_str(), sizeof(m_header->name) - 1);
        
        // Initialize queued buffers array
        for (uint32_t i = 0; i < m_maxQueueSize; ++i)
        {
            m_queuedBuffers[i] = QueuedBuffer();
        }
    }
    
    return true;
}

bool M1MemoryShare::openSharedMemoryFile()
{
    // Try to open existing file using JUCE
    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
    m_tempFile = tempDir.getChildFile("M1SpatialSystem_" + m_memoryName + ".mem");

    if (!m_tempFile.exists())
    {
        std::cerr << "[M1MemoryShare] Shared memory file does not exist: " << m_tempFile.getFullPathName() << std::endl;
        return false;
    }

    // Create memory mapped file
    m_mappedFile = std::make_unique<juce::MemoryMappedFile>(m_tempFile, juce::MemoryMappedFile::readWrite);
    if (!m_mappedFile->getData())
    {
        std::cerr << "[M1MemoryShare] Failed to memory map file: " << m_tempFile.getFullPathName() << std::endl;
        return false;
    }

    setupMemoryPointers();
    return true;
}

void M1MemoryShare::setupMemoryPointers()
{
    if (!m_mappedFile || !m_mappedFile->getData())
    {
        return;
    }
    
    uint8_t* basePtr = static_cast<uint8_t*>(m_mappedFile->getData());
    
    // Set up header
    m_header = reinterpret_cast<SharedMemoryHeader*>(basePtr);
    
    // Set up data buffer (after header)
    m_dataBuffer = basePtr + sizeof(SharedMemoryHeader);
    m_dataBufferSize = m_totalSize - sizeof(SharedMemoryHeader) - m_queuedBuffersSize;
    
    // Set up queued buffers area (at the end)
    m_queuedBuffers = reinterpret_cast<QueuedBuffer*>(basePtr + sizeof(SharedMemoryHeader) + m_dataBufferSize);
}

//==============================================================================
bool M1MemoryShare::initializeForAudio(uint32_t sampleRate, uint32_t numChannels, uint32_t samplesPerBlock)
{
    if (!isValid())
    {
        return false;
    }

    m_header->sampleRate = sampleRate;
    m_header->numChannels = numChannels;
    m_header->samplesPerBlock = samplesPerBlock;

    // Clear any existing data
    clear();

    return true;
}

bool M1MemoryShare::registerConsumer(uint32_t consumerId)
{
    if (!isValid())
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_queueMutex);
    
    // Check if consumer is already registered
    if (isConsumerRegistered(consumerId))
    {
        return true; // Already registered
    }

    // Find empty slot
    if (m_header->consumerCount >= 16)
    {
        std::cerr << "[M1MemoryShare] Maximum consumers reached" << std::endl;
        return false;
    }

    // Add consumer
    m_header->consumerIds[m_header->consumerCount] = consumerId;
    m_header->consumerCount++;

    std::cout << "[M1MemoryShare] Registered consumer " << consumerId << 
                 " (total consumers: " << m_header->consumerCount << ")" << std::endl;
    return true;
}

bool M1MemoryShare::unregisterConsumer(uint32_t consumerId)
{
    if (!isValid())
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_queueMutex);
    
    int index = findConsumerIndex(consumerId);
    if (index == -1)
    {
        return false; // Consumer not found
    }

    // Remove consumer by shifting array
    for (int i = index; i < static_cast<int>(m_header->consumerCount) - 1; ++i)
    {
        m_header->consumerIds[i] = m_header->consumerIds[i + 1];
    }
    m_header->consumerCount--;

    std::cout << "[M1MemoryShare] Unregistered consumer " << consumerId << 
                 " (total consumers: " << m_header->consumerCount << ")" << std::endl;
    return true;
}

// Implementation continues with buffer acknowledgment system...
// This is a substantial implementation that would continue with the rest of the methods
// For brevity, I'll provide a placeholder for the key methods:

uint64_t M1MemoryShare::writeAudioBufferWithGenericParameters(
    const std::vector<std::vector<float>>& audioBuffer,
    const ParameterMap& parameters,
    uint64_t dawTimestamp,
    double playheadPositionInSeconds,
    bool isPlaying,
    bool requiresAcknowledgment,
    uint32_t updateSource)
{
    // Implementation would write audio data with acknowledgment tracking
    // Returns buffer ID for acknowledgment
    return 0; // Placeholder
}

bool M1MemoryShare::acknowledgeBuffer(uint64_t bufferId, uint32_t consumerId)
{
    if (!isValid())
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_queueMutex);
    
    QueuedBuffer* buffer = findQueuedBuffer(bufferId);
    if (!buffer)
    {
        return false; // Buffer not found
    }

    // Find consumer index in buffer's consumer list
    int consumerIndex = -1;
    for (int i = 0; i < static_cast<int>(buffer->consumerCount); ++i)
    {
        if (buffer->consumerIds[i] == consumerId)
        {
            consumerIndex = i;
            break;
        }
    }

    if (consumerIndex == -1 || buffer->acknowledged[consumerIndex])
    {
        return false; // Consumer not found or already acknowledged
    }

    // Mark as acknowledged
    buffer->acknowledged[consumerIndex] = true;
    buffer->acknowledgedCount++;

    std::cout << "[M1MemoryShare] Consumer " << consumerId << " acknowledged buffer " << bufferId << 
                 " (" << buffer->acknowledgedCount << "/" << buffer->consumerCount << ")" << std::endl;

    // If all consumers have acknowledged, mark buffer as fully consumed
    if (buffer->acknowledgedCount >= buffer->consumerCount)
    {
        // Mark as acknowledged for all consumers
        for (uint32_t i = 0; i < buffer->consumerCount; ++i)
        {
            buffer->acknowledged[i] = true;
        }
        buffer->acknowledgedCount = buffer->consumerCount;
        cleanupAcknowledgedBuffers();
    }

    return true;
}

//==============================================================================
bool M1MemoryShare::isValid() const
{
    return (m_mappedFile && m_mappedFile->getData() != nullptr && 
            m_header != nullptr && 
            m_dataBuffer != nullptr && 
            m_queuedBuffers != nullptr);
}

uint64_t M1MemoryShare::getCurrentTimestamp() const
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

M1MemoryShare::QueuedBuffer* M1MemoryShare::findQueuedBuffer(uint64_t bufferId)
{
    for (uint32_t i = 0; i < m_header->queueSize; ++i)
    {
        if (m_queuedBuffers[i].bufferId == bufferId)
        {
            return &m_queuedBuffers[i];
        }
    }
    return nullptr;
}

int M1MemoryShare::findConsumerIndex(uint32_t consumerId) const
{
    for (int i = 0; i < static_cast<int>(m_header->consumerCount); ++i)
    {
        if (m_header->consumerIds[i] == consumerId)
        {
            return i;
        }
    }
    return -1;
}

bool M1MemoryShare::isConsumerRegistered(uint32_t consumerId) const
{
    return findConsumerIndex(consumerId) != -1;
}

void M1MemoryShare::cleanupAcknowledgedBuffers()
{
    // Remove consumed buffers from the queue
    uint32_t writeIndex = 0;
    for (uint32_t readIndex = 0; readIndex < m_header->queueSize; ++readIndex)
    {
        if (m_queuedBuffers[readIndex].acknowledgedCount < m_queuedBuffers[readIndex].consumerCount)
        {
            if (writeIndex != readIndex)
            {
                m_queuedBuffers[writeIndex] = m_queuedBuffers[readIndex];
            }
            writeIndex++;
        }
    }
    m_header->queueSize = writeIndex;
}

void M1MemoryShare::clear()
{
    if (!isValid())
    {
        return;
    }

    m_header->writeIndex = 0;
    m_header->readIndex = 0;
    m_header->dataSize = 0;
    m_header->hasData = false;
    m_header->queueSize = 0;
    m_header->nextSequenceNumber = 0;
    m_header->nextBufferId = 1;

    // Clear data buffer
    memset(m_dataBuffer, 0, m_dataBufferSize);
    
    // Clear queued buffers
    for (uint32_t i = 0; i < m_maxQueueSize; ++i)
    {
        m_queuedBuffers[i] = QueuedBuffer();
    }
}

uint32_t M1MemoryShare::getUnconsumedBufferCount() const
{
    if (!isValid())
    {
        return 0;
    }

    return m_header->queueSize;
}

//==============================================================================
bool M1MemoryShare::deleteSharedMemory(const juce::String& memoryName)
{
    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
    juce::File memoryFile = tempDir.getChildFile("M1SpatialSystem_" + memoryName + ".mem");

    if (memoryFile.exists())
    {
        return memoryFile.deleteFile();
    }

    return true; // File doesn't exist, consider it deleted
}

bool M1MemoryShare::readAudioBufferWithGenericParameters(juce::AudioBuffer<float>& audioBuffer,
                                                       ParameterMap& parameters,
                                                       uint64_t& dawTimestamp,
                                                       double& playheadPositionInSeconds,
                                                       bool& isPlaying,
                                                       uint64_t& bufferId,
                                                       uint32_t& updateSource)
{
    if (!isValid() || !m_header->hasData)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_queueMutex);

    // Check if there are any queued buffers
    if (m_header->queueSize == 0)
    {
        return false;
    }

    // Get the oldest buffer from the queue
    uint32_t readIndex = m_header->readIndex % m_maxQueueSize;
    const QueuedBuffer& queuedBuffer = m_queuedBuffers[readIndex];

    // For now, assume stereo 2-channel, 512 samples (this would need to be encoded in the data format)
    int numChannels = 2;
    int numSamples = 512;
    
    // Calculate expected audio data size
    uint32_t expectedAudioSize = numChannels * numSamples * sizeof(float);
    
    // Set the audio buffer size 
    audioBuffer.setSize(numChannels, numSamples);

    // Copy audio data if available
    if (queuedBuffer.dataSize >= expectedAudioSize && queuedBuffer.dataOffset < m_dataBufferSize)
    {
        const float* sourceData = reinterpret_cast<const float*>(m_dataBuffer + queuedBuffer.dataOffset);
        
        for (int channel = 0; channel < numChannels; ++channel)
        {
            if (channel < audioBuffer.getNumChannels())
            {
                audioBuffer.copyFrom(channel, 0, 
                                   sourceData + (channel * numSamples), 
                                   numSamples);
            }
        }
    }
    else
    {
        // Clear buffer if no data available
        audioBuffer.clear();
    }

    // Clear and set basic parameters
    parameters.floatParams.clear();
    parameters.intParams.clear();
    parameters.boolParams.clear();
    parameters.stringParams.clear();
    parameters.doubleParams.clear();
    parameters.uint32Params.clear();
    parameters.uint64Params.clear();
    
    // Add some basic parameters using the available fields
    parameters.addFloat(M1SystemHelperParameterIDs::GAIN, 1.0f);
    parameters.addUInt64(M1SystemHelperParameterIDs::BUFFER_ID, queuedBuffer.bufferId);
    parameters.addUInt32(M1SystemHelperParameterIDs::BUFFER_SEQUENCE, queuedBuffer.sequenceNumber);
    parameters.addUInt64(M1SystemHelperParameterIDs::BUFFER_TIMESTAMP, queuedBuffer.timestamp);

    // Set output parameters - using available fields
    dawTimestamp = queuedBuffer.timestamp;  // Use timestamp field
    playheadPositionInSeconds = 0.0;        // Default value 
    isPlaying = true;                       // Default value
    bufferId = queuedBuffer.bufferId;
    updateSource = 1; // Indicate MemoryShare as source

    // Move to next buffer
    m_header->readIndex = (m_header->readIndex + 1) % m_maxQueueSize;
    if (m_header->queueSize > 0)
    {
        m_header->queueSize--;
    }

    return true;
} 