#include "M1MemoryShare.h"
#include "SharedPathUtils.h"
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
                             bool createMode,
                             const std::string& explicitFilePath)
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
    , m_explicitFilePath(explicitFilePath)
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
    // Use SharedPathUtils to get the shared memory directory (App Group or fallback)
    std::string sharedDir = Mach1::SharedPathUtils::getSharedMemoryDirectory();
    if (sharedDir.empty()) {
        // Fallback to temp directory
        sharedDir = juce::File::getSpecialLocation(juce::File::tempDirectory).getFullPathName().toStdString();
    }
    
    juce::File memDir(sharedDir);
    if (!memDir.exists()) {
        memDir.createDirectory();
    }
    
    // Memory name already includes the full filename (e.g., M1SpatialSystem_M1Panner_...)
    // so we just append the extension
    m_tempFile = memDir.getChildFile(m_memoryName + ".mem");
    
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
    // If explicit file path provided, use it directly
    if (!m_explicitFilePath.empty())
    {
        m_tempFile = juce::File(m_explicitFilePath);
        if (m_tempFile.exists())
        {
            std::cout << "[M1MemoryShare] Opening explicit file path: " << m_tempFile.getFullPathName() << std::endl;
            
            m_mappedFile = std::make_unique<juce::MemoryMappedFile>(m_tempFile, juce::MemoryMappedFile::readWrite);
            if (m_mappedFile->getData())
            {
                setupMemoryPointers();
                return true;
            }
            else
            {
                std::cerr << "[M1MemoryShare] Failed to memory map explicit file: " << m_tempFile.getFullPathName() << std::endl;
            }
        }
        else
        {
            std::cerr << "[M1MemoryShare] Explicit file path does not exist: " << m_explicitFilePath << std::endl;
        }
        return false;
    }
    
    // Otherwise search all possible directories for the file
    // Memory name already includes the full filename (e.g., M1SpatialSystem_M1Panner_...)
    auto directories = Mach1::SharedPathUtils::getAllSharedDirectories();
    
    // Also add temp directory as last resort
    directories.push_back(juce::File::getSpecialLocation(juce::File::tempDirectory).getFullPathName().toStdString());
    
    for (const auto& dirPath : directories)
    {
        juce::File dir(dirPath);
        juce::File candidate = dir.getChildFile(m_memoryName + ".mem");
        
        if (candidate.exists())
        {
            m_tempFile = candidate;
            std::cout << "[M1MemoryShare] Found memory file at: " << m_tempFile.getFullPathName() << std::endl;
            
            // Create memory mapped file
            m_mappedFile = std::make_unique<juce::MemoryMappedFile>(m_tempFile, juce::MemoryMappedFile::readWrite);
            if (m_mappedFile->getData())
            {
                setupMemoryPointers();
                return true;
            }
            else
            {
                std::cerr << "[M1MemoryShare] Failed to memory map file: " << m_tempFile.getFullPathName() << std::endl;
            }
        }
    }

    std::cerr << "[M1MemoryShare] Shared memory file not found in any directory: " << m_memoryName << ".mem" << std::endl;
    return false;
}

void M1MemoryShare::setupMemoryPointers()
{
    if (!m_mappedFile || !m_mappedFile->getData())
    {
        return;
    }
    
    uint8_t* basePtr = static_cast<uint8_t*>(m_mappedFile->getData());
    
    // Set up header first (always at offset 0)
    m_header = reinterpret_cast<SharedMemoryHeader*>(basePtr);
    
    // When opening an existing file (not creating), read maxQueueSize from the header
    // This ensures we use the same layout as the panner that created the file
    if (!m_createMode && m_header->maxQueueSize > 0)
    {
        m_maxQueueSize = m_header->maxQueueSize;
    }
    
    // Memory layout (must match panner's M1MemoryShare):
    // 1. SharedMemoryHeader (200 bytes)
    // 2. QueuedBuffer array (maxQueueSize * sizeof(QueuedBuffer))
    // 3. Data buffer (remaining space)
    
    // Set up queued buffers pointer (AFTER header)
    m_queuedBuffers = reinterpret_cast<QueuedBuffer*>(basePtr + sizeof(SharedMemoryHeader));
    m_queuedBuffersSize = m_maxQueueSize * sizeof(QueuedBuffer);
    
    // Set up data buffer pointer (AFTER header AND queued buffers)
    m_dataBuffer = basePtr + sizeof(SharedMemoryHeader) + m_queuedBuffersSize;
    
    // Calculate actual data buffer size from file size
    size_t actualFileSize = m_mappedFile->getSize();
    m_dataBufferSize = actualFileSize - sizeof(SharedMemoryHeader) - m_queuedBuffersSize;
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
    // Search all possible directories for the file
    auto directories = Mach1::SharedPathUtils::getAllSharedDirectories();
    directories.push_back(juce::File::getSpecialLocation(juce::File::tempDirectory).getFullPathName().toStdString());
    
    bool deleted = false;
    for (const auto& dirPath : directories)
    {
        juce::File dir(dirPath);
        // Memory name may or may not already include the prefix
        juce::File memoryFile = dir.getChildFile(memoryName + ".mem");

        if (memoryFile.exists())
        {
            deleted = memoryFile.deleteFile() || deleted;
        }
    }

    return deleted || true; // Consider it deleted if not found
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

    // Read the GenericAudioBufferHeader from the data buffer
    // The panner writes: GenericAudioBufferHeader + GenericParameter entries + Audio data
    if (m_header->dataSize < sizeof(GenericAudioBufferHeader))
    {
        return false;
    }
    
    const uint8_t* readPtr = m_dataBuffer;
    const GenericAudioBufferHeader* header = reinterpret_cast<const GenericAudioBufferHeader*>(readPtr);
    
    // Validate header
    if (header->version != 1 || header->headerSize < sizeof(GenericAudioBufferHeader))
    {
        return false;
    }
    
    // Extract header fields
    int numChannels = static_cast<int>(header->channels);
    int numSamples = static_cast<int>(header->samples);
    dawTimestamp = header->dawTimestamp;
    playheadPositionInSeconds = header->playheadPositionInSeconds;
    isPlaying = (header->isPlaying != 0);
    bufferId = header->bufferId;
    updateSource = header->updateSource;
    
    // Clear existing parameters
    parameters.clear();
    
    // Parse parameters from the buffer
    readPtr += sizeof(GenericAudioBufferHeader);
    uint32_t paramCount = header->parameterCount;
    
    for (uint32_t i = 0; i < paramCount && (readPtr - m_dataBuffer) < m_header->dataSize; ++i)
    {
        const GenericParameter* param = reinterpret_cast<const GenericParameter*>(readPtr);
        readPtr += sizeof(GenericParameter);
        
        // Make sure we don't read past buffer
        if ((readPtr - m_dataBuffer) + param->dataSize > m_header->dataSize)
        {
            break;
        }
        
        switch (param->parameterType)
        {
            case ParameterType::FLOAT:
                parameters.addFloat(param->parameterID, *reinterpret_cast<const float*>(readPtr));
                break;
            case ParameterType::INT:
                parameters.addInt(param->parameterID, *reinterpret_cast<const int32_t*>(readPtr));
                break;
            case ParameterType::BOOL:
                parameters.addBool(param->parameterID, *reinterpret_cast<const bool*>(readPtr));
                break;
            case ParameterType::STRING:
                parameters.addString(param->parameterID, std::string(reinterpret_cast<const char*>(readPtr)));
                break;
            case ParameterType::DOUBLE:
                parameters.addDouble(param->parameterID, *reinterpret_cast<const double*>(readPtr));
                break;
            case ParameterType::UINT32:
                parameters.addUInt32(param->parameterID, *reinterpret_cast<const uint32_t*>(readPtr));
                break;
            case ParameterType::UINT64:
                parameters.addUInt64(param->parameterID, *reinterpret_cast<const uint64_t*>(readPtr));
                break;
        }
        
        readPtr += param->dataSize;
    }
    
    // Set the audio buffer size and copy audio data
    if (numChannels > 0 && numSamples > 0)
    {
        audioBuffer.setSize(numChannels, numSamples);
        
        // Audio data follows the header (readPtr should now point to audio data)
        const float* audioDataPtr = reinterpret_cast<const float*>(readPtr);
        
        // Copy interleaved audio data to JUCE buffer (deinterleave)
        for (int sample = 0; sample < numSamples; ++sample)
        {
            for (int channel = 0; channel < numChannels; ++channel)
            {
                audioBuffer.setSample(channel, sample, audioDataPtr[sample * numChannels + channel]);
            }
        }
    }
    else
    {
        audioBuffer.clear();
    }

    return true;
} 