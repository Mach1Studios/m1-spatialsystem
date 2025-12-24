#pragma once

#include <memory>
#include <atomic>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <chrono>
#include <cstring>
#include <string>
#include <vector>
#include "Common.h"
#include "TypesForDataExchange.h"

// Platform-specific includes
#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
    #include <sys/mman.h>
    #include <fcntl.h>
#endif

/**
 * M1MemoryShare provides IPC (Inter-Process Communication) memory sharing functionality
 * using JUCE's MemoryMappedFile for sharing audio data and control data between processes.
 *
 * Enhanced with buffer acknowledgment system for handling multiple unconsumed buffers.
 */
class M1MemoryShare
{
public:
    /**
     * Header structure that prefixes all shared memory segments
     * Contains metadata about the shared data and buffer queue
     */
    struct SharedMemoryHeader
    {
        volatile uint32_t writeIndex;           // Current write position
        volatile uint32_t readIndex;            // Current read position
        volatile uint32_t dataSize;             // Size of valid data
        volatile bool hasData;                  // Flag indicating if data is available
        uint32_t bufferSize;                    // Total buffer size (set once)
        uint32_t sampleRate;                    // Audio sample rate
        uint32_t numChannels;                   // Number of audio channels
        uint32_t samplesPerBlock;               // Samples per processing block
        char name[64];                          // Name identifier for debugging
        
        // Buffer queue management
        volatile uint32_t queueSize;            // Number of buffers in queue
        volatile uint32_t maxQueueSize;         // Maximum queue size
        volatile uint32_t nextSequenceNumber;   // Next sequence number to assign
        volatile uint64_t nextBufferId;         // Next buffer ID to assign
        
        // Consumer management
        volatile uint32_t consumerCount;        // Number of registered consumers
        volatile uint32_t consumerIds[16];      // Consumer IDs (max 16 consumers)
        
        // Bidirectional communication - Control messages from consumers back to producer
        volatile uint32_t controlMessageCount;  // Number of pending control messages
        volatile uint32_t controlReadIndex;     // Read index for control messages
        volatile uint32_t controlWriteIndex;    // Write index for control messages
        
        SharedMemoryHeader() : writeIndex(0), readIndex(0), dataSize(0), hasData(false),
                              bufferSize(0), sampleRate(0), numChannels(0), samplesPerBlock(0),
                              queueSize(0), maxQueueSize(8), nextSequenceNumber(0), nextBufferId(1),
                              consumerCount(0), controlMessageCount(0), controlReadIndex(0), controlWriteIndex(0)
        {
            std::memset(name, 0, sizeof(name));
            std::memset(const_cast<uint32_t*>(consumerIds), 0, sizeof(consumerIds));
        }
    };
    
    /**
     * Queued buffer entry with acknowledgment tracking
     */
    struct QueuedBuffer
    {
        uint64_t bufferId;
        uint32_t sequenceNumber;
        uint64_t timestamp;
        uint32_t dataSize;
        uint32_t dataOffset;        // Offset from start of data buffer
        bool requiresAcknowledgment;
        uint32_t consumerCount;
        uint32_t acknowledgedCount;
        uint32_t consumerIds[16];   // IDs of consumers that need to acknowledge
        bool acknowledged[16];      // Acknowledgment status for each consumer
        
        QueuedBuffer() : bufferId(0), sequenceNumber(0), timestamp(0), dataSize(0), dataOffset(0),
                        requiresAcknowledgment(false), consumerCount(0), acknowledgedCount(0)
        {
            std::memset(consumerIds, 0, sizeof(consumerIds));
            std::memset(acknowledged, false, sizeof(acknowledged));
        }
    };

    /**
     * Constructor for creating/opening a shared memory segment
     * @param memoryName Unique name for the shared memory segment (OS-wide)
     * @param totalSize Total size of the shared memory in bytes (must be >= 4KB)
     * @param maxQueueSize Maximum number of buffers to queue (default: 8)
     * @param persistent If true, memory persists until manually deleted; if false, cleaned up when process exits
     * @param createMode If true, creates new segment; if false, opens existing segment
     */
    M1MemoryShare(const std::string& memoryName,
                  size_t totalSize,
                  uint32_t maxQueueSize = 8,
                  bool persistent = true,
                  bool createMode = true);

    ~M1MemoryShare();

    /**
     * Initialize the shared memory for audio data
     * @param sampleRate Audio sample rate
     * @param numChannels Number of audio channels
     * @param samplesPerBlock Samples per processing block
     * @return true if successful
     */
    bool initializeForAudio(uint32_t sampleRate, uint32_t numChannels, uint32_t samplesPerBlock);

    /**
     * Register a consumer for buffer acknowledgment
     * @param consumerId Unique ID for the consumer
     * @return true if registration successful
     */
    bool registerConsumer(uint32_t consumerId);

    /**
     * Unregister a consumer
     * @param consumerId Consumer ID to unregister
     * @return true if unregistration successful
     */
    bool unregisterConsumer(uint32_t consumerId);

    /**
     * Write audio buffer to shared memory with generic parameter system and acknowledgment
     * @param audioBuffer Audio buffer containing the audio data (vector of channels)
     * @param parameters Generic parameter map containing all settings
     * @param dawTimestamp DAW/host timestamp
     * @param playheadPositionInSeconds DAW playhead position
     * @param isPlaying Whether DAW is currently playing
     * @param requiresAcknowledgment Whether this buffer requires acknowledgment
     * @param updateSource Source of the update (HOST, UI, MEMORYSHARE)
     * @return buffer ID if write was successful, 0 otherwise
     */
    uint64_t writeAudioBufferWithGenericParameters(const std::vector<std::vector<float>>& audioBuffer,
                                                  const ParameterMap& parameters,
                                                  uint64_t dawTimestamp,
                                                  double playheadPositionInSeconds,
                                                  bool isPlaying,
                                                  bool requiresAcknowledgment = false,
                                                  uint32_t updateSource = 1);

    /**
     * Read the oldest unacknowledged audio buffer from shared memory
     * @param audioBuffer JUCE AudioBuffer to store the read data
     * @param parameters Output parameter map to store all parameters
     * @param dawTimestamp Output DAW timestamp
     * @param playheadPositionInSeconds Output DAW playhead position
     * @param isPlaying Output playing state
     * @param bufferId Output buffer ID
     * @param updateSource Output update source
     * @return true if read was successful and data was available
     */
    bool readAudioBufferWithGenericParameters(juce::AudioBuffer<float>& audioBuffer,
                                            ParameterMap& parameters,
                                            uint64_t& dawTimestamp,
                                            double& playheadPositionInSeconds,
                                            bool& isPlaying,
                                            uint64_t& bufferId,
                                            uint32_t& updateSource);

    /**
     * Read a specific buffer by ID
     * @param bufferId Buffer ID to read
     * @param audioBuffer JUCE AudioBuffer to store the read data
     * @param parameters Output parameter map to store all parameters
     * @param dawTimestamp Output DAW timestamp
     * @param playheadPositionInSeconds Output DAW playhead position
     * @param isPlaying Output playing state
     * @param updateSource Output update source
     * @return true if read was successful and buffer was found
     */
    bool readBufferById(uint64_t bufferId,
                       juce::AudioBuffer<float>& audioBuffer,
                       ParameterMap& parameters,
                       uint64_t& dawTimestamp,
                       double& playheadPositionInSeconds,
                       bool& isPlaying,
                       uint32_t& updateSource);

    /**
     * Acknowledge consumption of a buffer
     * @param bufferId Buffer ID to acknowledge
     * @param consumerId Consumer ID that is acknowledging
     * @return true if acknowledgment was successful
     */
    bool acknowledgeBuffer(uint64_t bufferId, uint32_t consumerId);

    /**
     * Get list of available buffer IDs
     * @return Vector of buffer IDs that are available for reading
     */
    std::vector<uint64_t> getAvailableBufferIds() const;

    /**
     * Get the number of unconsumed buffers in the queue
     * @return Number of buffers waiting to be consumed
     */
    uint32_t getUnconsumedBufferCount() const;

    /**
     * Read only the generic parameters from shared memory (without audio data)
     * @param parameters Output parameter map to store all parameters
     * @param dawTimestamp Output DAW timestamp
     * @param playheadPositionInSeconds Output DAW playhead position
     * @param isPlaying Output playing state
     * @param updateSource Output update source
     * @return true if read was successful and header data was available
     */
    bool readGenericParameters(ParameterMap& parameters,
                             uint64_t& dawTimestamp,
                             double& playheadPositionInSeconds,
                             bool& isPlaying,
                             uint32_t& updateSource);

    /**
     * Read audio buffer from shared memory (legacy method)
     * @param audioBuffer JUCE AudioBuffer to store the read data
     * @return true if read was successful and data was available
     */
    bool readAudioBuffer(juce::AudioBuffer<float>& audioBuffer);

    /**
     * Write string data to shared memory
     * @param data String to write
     * @return true if write was successful
     */
    bool writeString(const juce::String& data);

    /**
     * Read string data from shared memory
     * @return String data if available, empty string otherwise
     */
    juce::String readString();

    /**
     * Write raw binary data to shared memory
     * @param data Pointer to data
     * @param size Size of data in bytes
     * @return true if write was successful
     */
    bool writeData(const void* data, size_t size);

    /**
     * Read raw binary data from shared memory
     * @param buffer Buffer to store data
     * @param maxSize Maximum size to read
     * @return Number of bytes actually read
     */
    size_t readData(void* buffer, size_t maxSize);

    /**
     * Check if the shared memory is valid and accessible
     * @return true if memory is accessible
     */
    bool isValid() const;

    /**
     * Get the current data size in the shared memory
     * @return Size of available data in bytes
     */
    size_t getDataSize() const;

    /**
     * Clear all data in the shared memory
     */
    void clear();

    /**
     * Get memory usage statistics
     */
    struct MemoryStats
    {
        size_t totalSize;
        size_t availableSize;
        size_t usedSize;
        uint32_t writeCount;
        uint32_t readCount;
        uint32_t queuedBufferCount;
        uint32_t acknowledgedBufferCount;
        uint32_t consumerCount;
    };

    MemoryStats getStats() const;

    /**
     * Static method to delete a shared memory segment by name
     * @param memoryName Name of the memory segment to delete
     * @return true if successfully deleted
     */
    static bool deleteSharedMemory(const juce::String& memoryName);

private:
    juce::String m_memoryName;
    size_t m_totalSize;
    uint32_t m_maxQueueSize;
    bool m_persistent;
    bool m_createMode;

    std::unique_ptr<juce::MemoryMappedFile> m_mappedFile;
    juce::File m_tempFile;

    SharedMemoryHeader* m_header;
    uint8_t* m_dataBuffer;
    size_t m_dataBufferSize;

    // Queue management
    QueuedBuffer* m_queuedBuffers;  // Array of queued buffers
    size_t m_queuedBuffersSize;     // Size of queued buffers area
    
    mutable std::atomic<uint32_t> m_writeCount{0};
    mutable std::atomic<uint32_t> m_readCount{0};
    mutable std::mutex m_queueMutex;

    bool createSharedMemoryFile();
    bool openSharedMemoryFile();
    void setupMemoryPointers();
    
    // Buffer management
    uint64_t getNextBufferId();
    uint32_t getNextSequenceNumber();
    uint64_t getCurrentTimestamp() const;
    
    // Queue management
    bool addToQueue(uint64_t bufferId, uint32_t sequenceNumber, uint64_t timestamp,
                   uint32_t dataSize, uint32_t dataOffset, bool requiresAcknowledgment);
    bool removeFromQueue(uint64_t bufferId);
    QueuedBuffer* findQueuedBuffer(uint64_t bufferId);
    void cleanupAcknowledgedBuffers();
    
    // Consumer management
    int findConsumerIndex(uint32_t consumerId) const;
    bool isConsumerRegistered(uint32_t consumerId) const;

    // Prevent copying
    M1MemoryShare(const M1MemoryShare&) = delete;
    M1MemoryShare& operator=(const M1MemoryShare&) = delete;
}; 
