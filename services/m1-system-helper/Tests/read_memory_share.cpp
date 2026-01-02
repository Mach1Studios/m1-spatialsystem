/**
 * M1MemoryShare Reader Tool
 * 
 * This tool reads and parses .mem files created by M1-Panner plugins.
 * It matches the exact struct layouts from:
 *   - m1-panner/Source/M1MemoryShare.h
 *   - m1-panner/Source/TypesForDataExchange.h
 *   - services/m1-system-helper/Source/Common/M1MemoryShare.h
 *   - services/m1-system-helper/Source/Common/TypesForDataExchange.h
 * 
 * Build: clang++ -std=c++17 -o read_memory_share read_memory_share.cpp
 * Usage: ./read_memory_share <memory_file.mem>
 */

#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <ctime>

// ============================================================================
// STRUCT DEFINITIONS - Must match M1MemoryShare.h exactly
// ============================================================================

/**
 * SharedMemoryHeader - matches M1MemoryShare.h
 * This struct is at the beginning of every .mem file
 */
struct SharedMemoryHeader {
    volatile uint32_t writeIndex;           // 0: Current write position
    volatile uint32_t readIndex;            // 4: Current read position
    volatile uint32_t dataSize;             // 8: Size of valid data
    volatile bool hasData;                  // 12: Flag indicating if data is available (1 byte + 3 padding)
    uint32_t bufferSize;                    // 16: Total buffer size (set once)
    uint32_t sampleRate;                    // 20: Audio sample rate
    uint32_t numChannels;                   // 24: Number of audio channels
    uint32_t samplesPerBlock;               // 28: Samples per processing block
    char name[64];                          // 32-95: Name identifier for debugging
    
    // Buffer queue management
    volatile uint32_t queueSize;            // 96: Number of buffers in queue
    volatile uint32_t maxQueueSize;         // 100: Maximum queue size
    volatile uint32_t nextSequenceNumber;   // 104: Next sequence number to assign
    volatile uint64_t nextBufferId;         // 112: Next buffer ID to assign (aligned to 8)
    
    // Consumer management
    volatile uint32_t consumerCount;        // 120: Number of registered consumers
    volatile uint32_t consumerIds[16];      // 124-187: Consumer IDs (max 16 consumers)
    
    // Bidirectional communication - Control messages
    volatile uint32_t controlMessageCount;  // 188: Number of pending control messages
    volatile uint32_t controlReadIndex;     // 192: Read index for control messages
    volatile uint32_t controlWriteIndex;    // 196: Write index for control messages
    // Total size: 200 bytes
};

/**
 * GenericAudioBufferHeader - matches TypesForDataExchange.h
 * This struct follows SharedMemoryHeader + QueuedBuffers in the data section
 */
struct GenericAudioBufferHeader {
    uint32_t version;                       // 0: Version of header format
    uint32_t channels;                      // 4: Number of audio channels
    uint32_t samples;                       // 8: Number of samples per channel
    // 4 bytes padding for uint64_t alignment
    uint64_t dawTimestamp;                  // 16: DAW/host timestamp in milliseconds
    double playheadPositionInSeconds;       // 24: DAW playhead position
    uint32_t isPlaying;                     // 32: Is DAW playing (1) or stopped (0)
    uint32_t parameterCount;                // 36: Number of parameters following
    uint32_t headerSize;                    // 40: Total size of header including all parameters
    uint32_t updateSource;                  // 44: Source of update (0=HOST, 1=UI, 2=MEMORYSHARE)
    uint32_t isUpdatingFromExternal;        // 48: Flag to prevent circular updates
    // 4 bytes padding for uint64_t alignment
    uint64_t bufferId;                      // 56: Unique buffer identifier
    uint32_t sequenceNumber;                // 64: Sequence number for ordering
    // 4 bytes padding for uint64_t alignment
    uint64_t bufferTimestamp;               // 72: Buffer creation timestamp
    uint32_t requiresAcknowledgment;        // 80: Whether this buffer requires acknowledgment
    uint32_t consumerCount;                 // 84: Number of consumers that need to acknowledge
    uint32_t acknowledgedCount;             // 88: Number of consumers that have acknowledged
    uint32_t reserved[2];                   // 92-99: Reserved for future expansion
    // Total size: 104 bytes (with padding)
};

/**
 * GenericParameter - matches TypesForDataExchange.h
 */
struct GenericParameter {
    uint32_t parameterID;       // Hash/ID of parameter name
    uint32_t parameterType;     // Type: 0=FLOAT, 1=INT, 2=BOOL, 3=STRING, 4=DOUBLE, 5=UINT32, 6=UINT64
    uint32_t dataSize;          // Size of parameter data in bytes
    // Parameter data follows immediately after this struct
};

/**
 * QueuedBuffer - matches M1MemoryShare.h
 */
struct QueuedBuffer {
    uint64_t bufferId;
    uint32_t sequenceNumber;
    uint64_t timestamp;
    uint32_t dataSize;
    uint32_t dataOffset;
    bool requiresAcknowledgment;
    uint32_t consumerCount;
    uint32_t acknowledgedCount;
    uint32_t consumerIds[16];
    bool acknowledged[16];
};

// ============================================================================
// PARAMETER ID DEFINITIONS - matches TypesForDataExchange.h
// ============================================================================

struct ParameterInfo {
    uint32_t id;
    const char* name;
};

const ParameterInfo KNOWN_PARAMETERS[] = {
    {0x1A2B3C4D, "AZIMUTH"},
    {0x2B3C4D5E, "ELEVATION"},
    {0x3C4D5E6F, "DIVERGE"},
    {0x4D5E6F70, "GAIN"},
    {0x5E6F7081, "STEREO_ORBIT_AZIMUTH"},
    {0x6F708192, "STEREO_SPREAD"},
    {0x708192A3, "STEREO_INPUT_BALANCE"},
    {0x8192A3B4, "AUTO_ORBIT"},
    {0x92A3B4C5, "ISOTROPIC_MODE"},
    {0xA3B4C5D6, "EQUALPOWER_MODE"},
    {0xB4C5D6E7, "GAIN_COMPENSATION_MODE"},
    {0xC5D6E7F8, "LOCK_OUTPUT_LAYOUT"},
    {0xD6E7F809, "INPUT_MODE"},
    {0xE7F8091A, "OUTPUT_MODE"},
    {0xF8091A2B, "PORT"},
    {0x091A2B3C, "STATE"},
    {0x1A2B3C4E, "COLOR_R"},
    {0x2B3C4E5F, "COLOR_G"},
    {0x3C4E5F60, "COLOR_B"},
    {0x4E5F6071, "COLOR_A"},
    {0x5F607182, "DISPLAY_NAME"},
};

const char* getParameterName(uint32_t id) {
    for (const auto& param : KNOWN_PARAMETERS) {
        if (param.id == id) return param.name;
    }
    return "UNKNOWN";
}

const char* getParameterTypeName(uint32_t type) {
    switch (type) {
        case 0: return "FLOAT";
        case 1: return "INT";
        case 2: return "BOOL";
        case 3: return "STRING";
        case 4: return "DOUBLE";
        case 5: return "UINT32";
        case 6: return "UINT64";
        default: return "UNKNOWN";
    }
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

void printHex(const uint8_t* data, size_t len, size_t offset = 0) {
    for (size_t i = 0; i < len; i += 16) {
        std::cout << std::hex << std::setfill('0') << std::setw(8) << (offset + i) << "  ";
        for (size_t j = 0; j < 16 && (i + j) < len; ++j) {
            std::cout << std::setw(2) << (int)data[i + j] << " ";
            if (j == 7) std::cout << " ";
        }
        std::cout << " |";
        for (size_t j = 0; j < 16 && (i + j) < len; ++j) {
            char c = data[i + j];
            std::cout << (isprint(c) ? c : '.');
        }
        std::cout << "|" << std::endl;
    }
    std::cout << std::dec;
}

std::string formatTimestamp(uint64_t ms) {
    time_t seconds = ms / 1000;
    struct tm* tm_info = localtime(&seconds);
    char buffer[64];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
    return std::string(buffer) + "." + std::to_string(ms % 1000);
}

// ============================================================================
// MAIN ANALYSIS FUNCTION
// ============================================================================

int analyzeMemoryFile(const char* filepath) {
    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "M1MemoryShare File Analysis\n";
    std::cout << "================================================================\n";
    std::cout << "File: " << filepath << "\n\n";
    
    // Open file
    int fd = open(filepath, O_RDONLY);
    if (fd == -1) {
        std::cerr << "ERROR: Failed to open file: " << strerror(errno) << std::endl;
        return 1;
    }
    
    // Get file size
    struct stat st;
    if (fstat(fd, &st) == -1) {
        std::cerr << "ERROR: Failed to stat file: " << strerror(errno) << std::endl;
        close(fd);
        return 1;
    }
    
    std::cout << "File size: " << st.st_size << " bytes\n";
    std::cout << "Expected SharedMemoryHeader size: " << sizeof(SharedMemoryHeader) << " bytes\n";
    std::cout << "Expected GenericAudioBufferHeader size: " << sizeof(GenericAudioBufferHeader) << " bytes\n";
    std::cout << "\n";
    
    // Memory map the file
    void* mapped = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (mapped == MAP_FAILED) {
        std::cerr << "ERROR: Failed to mmap file: " << strerror(errno) << std::endl;
        close(fd);
        return 1;
    }
    
    const uint8_t* rawData = static_cast<const uint8_t*>(mapped);
    
    // ========================================================================
    // Parse SharedMemoryHeader
    // ========================================================================
    std::cout << "--- SharedMemoryHeader (offset 0) ---\n";
    
    if (st.st_size < sizeof(SharedMemoryHeader)) {
        std::cerr << "ERROR: File too small for SharedMemoryHeader\n";
        munmap(mapped, st.st_size);
        close(fd);
        return 1;
    }
    
    const SharedMemoryHeader* header = static_cast<const SharedMemoryHeader*>(mapped);
    
    std::cout << "  writeIndex:         " << header->writeIndex << "\n";
    std::cout << "  readIndex:          " << header->readIndex << "\n";
    std::cout << "  dataSize:           " << header->dataSize << " bytes\n";
    std::cout << "  hasData:            " << (header->hasData ? "YES" : "NO") << "\n";
    std::cout << "  bufferSize:         " << header->bufferSize << " bytes\n";
    std::cout << "  sampleRate:         " << header->sampleRate << " Hz\n";
    std::cout << "  numChannels:        " << header->numChannels << "\n";
    std::cout << "  samplesPerBlock:    " << header->samplesPerBlock << "\n";
    std::cout << "  name:               \"" << header->name << "\"\n";
    std::cout << "  queueSize:          " << header->queueSize << "\n";
    std::cout << "  maxQueueSize:       " << header->maxQueueSize << "\n";
    std::cout << "  nextSequenceNumber: " << header->nextSequenceNumber << "\n";
    std::cout << "  nextBufferId:       " << header->nextBufferId << "\n";
    std::cout << "  consumerCount:      " << header->consumerCount << "\n";
    
    if (header->consumerCount > 0 && header->consumerCount <= 16) {
        std::cout << "  consumerIds:        [";
        for (uint32_t i = 0; i < header->consumerCount; ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << header->consumerIds[i];
        }
        std::cout << "]\n";
    }
    
    std::cout << "  controlMsgCount:    " << header->controlMessageCount << "\n";
    std::cout << "  controlReadIdx:     " << header->controlReadIndex << "\n";
    std::cout << "  controlWriteIdx:    " << header->controlWriteIndex << "\n";
    std::cout << "\n";
    
    // Show raw hex of header for debugging
    std::cout << "--- SharedMemoryHeader Raw Hex (first 64 bytes) ---\n";
    printHex(rawData, 64, 0);
    std::cout << "\n";
    
    // ========================================================================
    // Parse Data Section
    // ========================================================================
    if (!header->hasData || header->dataSize == 0) {
        std::cout << "No data available in shared memory.\n";
        munmap(mapped, st.st_size);
        close(fd);
        return 0;
    }
    
    // Data buffer is AFTER SharedMemoryHeader AND QueuedBuffer array
    // Layout: SharedMemoryHeader | QueuedBuffer[maxQueueSize] | DataBuffer
    size_t queuedBuffersSize = header->maxQueueSize * sizeof(QueuedBuffer);
    size_t dataOffset = sizeof(SharedMemoryHeader) + queuedBuffersSize;
    const uint8_t* dataSection = rawData + dataOffset;
    
    std::cout << "QueuedBuffer array size: " << queuedBuffersSize << " bytes (" 
              << header->maxQueueSize << " x " << sizeof(QueuedBuffer) << ")\n";
    std::cout << "Data section starts at offset: " << dataOffset << "\n\n";
    
    std::cout << "--- Data Section (offset " << dataOffset << ") ---\n";
    std::cout << "Data section raw hex (first 128 bytes):\n";
    printHex(dataSection, std::min((size_t)128, (size_t)header->dataSize), dataOffset);
    std::cout << "\n";
    
    // ========================================================================
    // Parse GenericAudioBufferHeader
    // ========================================================================
    if (header->dataSize < sizeof(GenericAudioBufferHeader)) {
        std::cerr << "WARNING: Data size (" << header->dataSize << ") too small for GenericAudioBufferHeader (" 
                  << sizeof(GenericAudioBufferHeader) << ")\n";
        munmap(mapped, st.st_size);
        close(fd);
        return 0;
    }
    
    const GenericAudioBufferHeader* audioHeader = reinterpret_cast<const GenericAudioBufferHeader*>(dataSection);
    
    std::cout << "--- GenericAudioBufferHeader ---\n";
    std::cout << "  version:            " << audioHeader->version << " (expected: 1)\n";
    std::cout << "  channels:           " << audioHeader->channels << "\n";
    std::cout << "  samples:            " << audioHeader->samples << "\n";
    std::cout << "  dawTimestamp:       " << audioHeader->dawTimestamp << " ms";
    if (audioHeader->dawTimestamp > 0) {
        std::cout << " (" << formatTimestamp(audioHeader->dawTimestamp) << ")";
    }
    std::cout << "\n";
    std::cout << "  playheadPosition:   " << std::fixed << std::setprecision(3) 
              << audioHeader->playheadPositionInSeconds << " seconds\n";
    std::cout << "  isPlaying:          " << (audioHeader->isPlaying ? "YES" : "NO") << "\n";
    std::cout << "  parameterCount:     " << audioHeader->parameterCount << "\n";
    std::cout << "  headerSize:         " << audioHeader->headerSize << " bytes\n";
    std::cout << "  updateSource:       " << audioHeader->updateSource 
              << " (0=HOST, 1=UI, 2=MEMORYSHARE)\n";
    std::cout << "  isUpdatingExternal: " << (audioHeader->isUpdatingFromExternal ? "YES" : "NO") << "\n";
    std::cout << "  bufferId:           " << audioHeader->bufferId << "\n";
    std::cout << "  sequenceNumber:     " << audioHeader->sequenceNumber << "\n";
    std::cout << "  bufferTimestamp:    " << audioHeader->bufferTimestamp << " ms\n";
    std::cout << "  requiresAck:        " << (audioHeader->requiresAcknowledgment ? "YES" : "NO") << "\n";
    std::cout << "  consumerCount:      " << audioHeader->consumerCount << "\n";
    std::cout << "  acknowledgedCount:  " << audioHeader->acknowledgedCount << "\n";
    std::cout << "\n";
    
    // Validate header
    if (audioHeader->version != 1) {
        std::cout << "WARNING: Unexpected version! Expected 1, got " << audioHeader->version << "\n";
        std::cout << "This may indicate a struct layout mismatch.\n\n";
    }
    
    // ========================================================================
    // Parse Parameters
    // ========================================================================
    if (audioHeader->parameterCount > 0 && audioHeader->parameterCount < 100) {
        std::cout << "--- Parameters (" << audioHeader->parameterCount << " total) ---\n";
        
        const uint8_t* paramPtr = dataSection + sizeof(GenericAudioBufferHeader);
        size_t remainingSize = header->dataSize - sizeof(GenericAudioBufferHeader);
        
        for (uint32_t i = 0; i < audioHeader->parameterCount && remainingSize >= sizeof(GenericParameter); ++i) {
            const GenericParameter* param = reinterpret_cast<const GenericParameter*>(paramPtr);
            paramPtr += sizeof(GenericParameter);
            remainingSize -= sizeof(GenericParameter);
            
            if (param->dataSize > remainingSize) {
                std::cout << "  [" << i << "] ERROR: Parameter data size exceeds remaining buffer\n";
                break;
            }
            
            std::cout << "  [" << i << "] " << std::setw(25) << std::left << getParameterName(param->parameterID)
                      << " (0x" << std::hex << std::setw(8) << std::setfill('0') << param->parameterID << std::dec << std::setfill(' ') << ") "
                      << std::setw(8) << getParameterTypeName(param->parameterType) << " = ";
            
            switch (param->parameterType) {
                case 0: // FLOAT
                    std::cout << std::fixed << std::setprecision(4) << *reinterpret_cast<const float*>(paramPtr);
                    break;
                case 1: // INT
                    std::cout << *reinterpret_cast<const int32_t*>(paramPtr);
                    break;
                case 2: // BOOL
                    std::cout << (*reinterpret_cast<const uint8_t*>(paramPtr) ? "true" : "false");
                    break;
                case 3: // STRING
                    std::cout << "\"" << std::string(reinterpret_cast<const char*>(paramPtr), param->dataSize) << "\"";
                    break;
                case 4: // DOUBLE
                    std::cout << std::fixed << std::setprecision(6) << *reinterpret_cast<const double*>(paramPtr);
                    break;
                case 5: // UINT32
                    std::cout << *reinterpret_cast<const uint32_t*>(paramPtr);
                    break;
                case 6: // UINT64
                    std::cout << *reinterpret_cast<const uint64_t*>(paramPtr);
                    break;
                default:
                    std::cout << "(unknown type)";
            }
            std::cout << "\n";
            
            paramPtr += param->dataSize;
            remainingSize -= param->dataSize;
        }
        std::cout << "\n";
    }
    
    // ========================================================================
    // Audio Data Info
    // ========================================================================
    if (audioHeader->channels > 0 && audioHeader->samples > 0) {
        size_t audioDataOffset = dataOffset + audioHeader->headerSize;
        size_t audioDataSize = audioHeader->channels * audioHeader->samples * sizeof(float);
        
        std::cout << "--- Audio Data ---\n";
        std::cout << "  Offset:       " << audioDataOffset << " bytes\n";
        std::cout << "  Expected size: " << audioDataSize << " bytes\n";
        std::cout << "  Channels:     " << audioHeader->channels << "\n";
        std::cout << "  Samples:      " << audioHeader->samples << "\n";
        
        if (audioDataOffset + audioDataSize <= (size_t)st.st_size) {
            const float* audioData = reinterpret_cast<const float*>(rawData + audioDataOffset);
            
            // Show first few samples
            std::cout << "  First 10 samples (channel 0): ";
            for (int i = 0; i < 10 && i < (int)audioHeader->samples; ++i) {
                std::cout << std::fixed << std::setprecision(4) << audioData[i * audioHeader->channels] << " ";
            }
            std::cout << "\n";
            
            // Calculate RMS level
            float rms = 0.0f;
            for (uint32_t i = 0; i < audioHeader->samples * audioHeader->channels; ++i) {
                rms += audioData[i] * audioData[i];
            }
            rms = sqrt(rms / (audioHeader->samples * audioHeader->channels));
            std::cout << "  RMS Level:    " << std::fixed << std::setprecision(6) << rms << "\n";
        } else {
            std::cout << "  ERROR: Audio data extends beyond file size\n";
        }
    }
    
    std::cout << "\n================================================================\n";
    
    munmap(mapped, st.st_size);
    close(fd);
    return 0;
}

// ============================================================================
// FIND MEMORY FILES
// ============================================================================

std::vector<std::string> findMemoryFiles() {
    std::vector<std::string> files;
    
    const char* searchDirs[] = {
        "/Users/m1dev/Library/Group Containers/group.com.mach1.spatial.shared/Library/Caches/M1-Panner",
        "/Users/m1dev/Library/Caches/M1-Panner",
        "/tmp/M1-Panner",
        "/tmp"
    };
    
    for (const char* dir : searchDirs) {
        DIR* d = opendir(dir);
        if (!d) continue;
        
        struct dirent* entry;
        while ((entry = readdir(d)) != nullptr) {
            std::string name = entry->d_name;
            if (name.find(".mem") != std::string::npos && name.find("M1SpatialSystem") != std::string::npos) {
                files.push_back(std::string(dir) + "/" + name);
            }
        }
        closedir(d);
    }
    
    return files;
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char* argv[]) {
    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "M1MemoryShare Reader Tool\n";
    std::cout << "================================================================\n";
    std::cout << "Struct sizes (must match panner and helper):\n";
    std::cout << "  SharedMemoryHeader:      " << sizeof(SharedMemoryHeader) << " bytes\n";
    std::cout << "  GenericAudioBufferHeader:" << sizeof(GenericAudioBufferHeader) << " bytes\n";
    std::cout << "  GenericParameter:        " << sizeof(GenericParameter) << " bytes\n";
    std::cout << "  QueuedBuffer:            " << sizeof(QueuedBuffer) << " bytes\n";
    std::cout << "\n";
    
    if (argc > 1) {
        // Analyze specified file
        return analyzeMemoryFile(argv[1]);
    }
    
    // Auto-find memory files
    std::cout << "Searching for .mem files...\n";
    auto files = findMemoryFiles();
    
    if (files.empty()) {
        std::cout << "\nNo .mem files found in standard locations.\n";
        std::cout << "Searched in:\n";
        std::cout << "  - ~/Library/Group Containers/group.com.mach1.spatial.shared/Library/Caches/M1-Panner\n";
        std::cout << "  - ~/Library/Caches/M1-Panner\n";
        std::cout << "  - /tmp/M1-Panner\n";
        std::cout << "  - /tmp\n";
        std::cout << "\nMake sure M1-Panner is loaded on a stereo track in your DAW.\n";
        return 1;
    }
    
    std::cout << "Found " << files.size() << " memory file(s):\n";
    for (size_t i = 0; i < files.size(); ++i) {
        std::cout << "  [" << i << "] " << files[i] << "\n";
    }
    
    // Analyze all files
    for (const auto& file : files) {
        analyzeMemoryFile(file.c_str());
    }
    
    return 0;
}

