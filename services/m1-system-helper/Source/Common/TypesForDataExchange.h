#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <cstdint>
#include <map>
#include <vector>
#include <chrono>

/**
 * Parameter ID constants for M1 System Helper (using string hashes for type safety)
 */
struct M1SystemHelperParameterIDs
{
    static const uint32_t AZIMUTH = 0x1A2B3C4D;           // "azimuth"
    static const uint32_t ELEVATION = 0x2B3C4D5E;         // "elevation"
    static const uint32_t DIVERGE = 0x3C4D5E6F;           // "diverge"
    static const uint32_t GAIN = 0x4D5E6F70;              // "gain"
    static const uint32_t STEREO_ORBIT_AZIMUTH = 0x5E6F7081; // "stereoOrbitAzimuth"
    static const uint32_t STEREO_SPREAD = 0x6F708192;     // "stereoSpread"
    static const uint32_t STEREO_INPUT_BALANCE = 0x708192A3; // "stereoInputBalance"
    static const uint32_t AUTO_ORBIT = 0x8192A3B4;        // "autoOrbit"
    static const uint32_t ISOTROPIC_MODE = 0x92A3B4C5;    // "isotropicMode"
    static const uint32_t EQUALPOWER_MODE = 0xA3B4C5D6;   // "equalpowerMode"
    static const uint32_t GAIN_COMPENSATION_MODE = 0xB4C5D6E7; // "gainCompensationMode"
    static const uint32_t LOCK_OUTPUT_LAYOUT = 0xC5D6E7F8; // "lockOutputLayout"
    static const uint32_t INPUT_MODE = 0xD6E7F809;        // "inputMode"
    static const uint32_t OUTPUT_MODE = 0xE7F8091A;       // "outputMode"
    static const uint32_t PORT = 0xF8091A2B;              // "port"
    static const uint32_t STATE = 0x091A2B3C;             // "state"
    static const uint32_t COLOR_R = 0x1A2B3C4E;           // "colorR"
    static const uint32_t COLOR_G = 0x2B3C4E5F;           // "colorG"
    static const uint32_t COLOR_B = 0x3C4E5F60;           // "colorB"
    static const uint32_t COLOR_A = 0x4E5F6071;           // "colorA"
    static const uint32_t DISPLAY_NAME = 0x5F607182;      // "displayName"
    
    // System Helper specific parameters
    static const uint32_t BUFFER_ID = 0x60718293;         // "bufferId"
    static const uint32_t BUFFER_SEQUENCE = 0x71829304;   // "bufferSequence"
    static const uint32_t BUFFER_TIMESTAMP = 0x82930415;  // "bufferTimestamp"

    // Helper function to generate hash from string (for dynamic parameter IDs)
    static uint32_t hashString(const char* str)
    {
        uint32_t hash = 0;
        while (*str)
        {
            hash = hash * 31 + *str++;
        }
        return hash;
    }
};

// Update source priority for circular update prevention
enum class ParameterUpdateSource
{
    HOST = 0,        // Highest priority - DAW automation, parameter changes from host
    UI = 1,          // Medium priority - User interaction with plugin UI
    MEMORYSHARE = 2  // Lowest priority - External memory share updates
};

// =============================================================================
// GENERIC PARAMETER SYSTEM FOR MEMORY SHARING
// =============================================================================

/**
 * Generic parameter types for flexible parameter system
 */
enum class ParameterType : uint32_t
{
    FLOAT = 0,
    INT = 1,
    BOOL = 2,
    STRING = 3,
    DOUBLE = 4,
    UINT32 = 5,
    UINT64 = 6
};

/**
 * Generic parameter structure - variable length
 */
struct GenericParameter
{
    uint32_t parameterID;       // Hash/ID of parameter name
    ParameterType parameterType; // Type of parameter data
    uint32_t dataSize;          // Size of parameter data in bytes
    // Parameter data follows immediately after this struct

    GenericParameter() : parameterID(0), parameterType(ParameterType::FLOAT), dataSize(0) {}
    GenericParameter(uint32_t id, ParameterType type, uint32_t size)
        : parameterID(id), parameterType(type), dataSize(size) {}
};

/**
 * Buffer acknowledgment system for tracking consumed buffers
 */
struct BufferAcknowledgment
{
    uint64_t bufferId;          // Unique buffer identifier
    uint32_t sequenceNumber;    // Sequence number for ordering
    uint64_t timestamp;         // Timestamp when buffer was created
    bool isConsumed;            // Whether buffer has been consumed
    uint32_t consumerCount;     // Number of consumers that need to acknowledge
    uint32_t acknowledgedCount; // Number of consumers that have acknowledged
    std::vector<uint32_t> consumerIds; // List of consumer IDs

    BufferAcknowledgment() : bufferId(0), sequenceNumber(0), timestamp(0), 
                           isConsumed(false), consumerCount(0), acknowledgedCount(0) {}
};

/**
 * Enhanced generic audio buffer header with acknowledgment system
 */
struct GenericAudioBufferHeader
{
    uint32_t version;           // Version of header format (for future compatibility)
    uint32_t channels;          // Number of audio channels
    uint32_t samples;           // Number of samples per channel
    uint64_t dawTimestamp;      // DAW/host timestamp in milliseconds
    double playheadPositionInSeconds; // DAW playhead position
    uint32_t isPlaying;         // Is DAW playing (1) or stopped (0)
    uint32_t parameterCount;    // Number of parameters following
    uint32_t headerSize;        // Total size of header including all parameters
    uint32_t updateSource;      // Source of update (0=HOST, 1=UI, 2=MEMORYSHARE)
    uint32_t isUpdatingFromExternal; // Flag to prevent circular updates
    
    // Buffer acknowledgment fields
    uint64_t bufferId;          // Unique buffer identifier
    uint32_t sequenceNumber;    // Sequence number for ordering
    uint64_t bufferTimestamp;   // Buffer creation timestamp
    uint32_t requiresAcknowledgment; // Whether this buffer needs acknowledgment
    uint32_t consumerCount;     // Number of expected consumers
    uint32_t acknowledgedCount; // Number of consumers that have acknowledged
    
    uint32_t reserved[2];       // Reserved for future expansion (reduced from 4 to 2)

    // Parameters follow immediately after this struct
    // Each parameter is: GenericParameter + parameter data

    GenericAudioBufferHeader() : version(1), channels(0), samples(0), dawTimestamp(0),
                                playheadPositionInSeconds(0.0), isPlaying(0), parameterCount(0),
                                headerSize(sizeof(GenericAudioBufferHeader)), updateSource(1),
                                isUpdatingFromExternal(0), bufferId(0), sequenceNumber(0),
                                bufferTimestamp(0), requiresAcknowledgment(0), consumerCount(0),
                                acknowledgedCount(0)
    {
        std::memset(reserved, 0, sizeof(reserved));
    }
};

/**
 * Generic parameter map for flexible parameter passing
 */
struct ParameterMap
{
    std::map<uint32_t, float> floatParams;
    std::map<uint32_t, int32_t> intParams;
    std::map<uint32_t, bool> boolParams;
    std::map<uint32_t, std::string> stringParams;
    std::map<uint32_t, double> doubleParams;
    std::map<uint32_t, uint32_t> uint32Params;
    std::map<uint32_t, uint64_t> uint64Params;

    // Convenience methods for adding parameters
    void addFloat(uint32_t id, float value) { floatParams[id] = value; }
    void addInt(uint32_t id, int32_t value) { intParams[id] = value; }
    void addBool(uint32_t id, bool value) { boolParams[id] = value; }
    void addString(uint32_t id, const std::string& value) { stringParams[id] = value; }
    void addDouble(uint32_t id, double value) { doubleParams[id] = value; }
    void addUInt32(uint32_t id, uint32_t value) { uint32Params[id] = value; }
    void addUInt64(uint32_t id, uint64_t value) { uint64Params[id] = value; }

    // Convenience methods for getting parameters
    float getFloat(uint32_t id, float defaultValue = 0.0f) const
    {
        auto it = floatParams.find(id);
        return it != floatParams.end() ? it->second : defaultValue;
    }

    int32_t getInt(uint32_t id, int32_t defaultValue = 0) const
    {
        auto it = intParams.find(id);
        return it != intParams.end() ? it->second : defaultValue;
    }

    bool getBool(uint32_t id, bool defaultValue = false) const
    {
        auto it = boolParams.find(id);
        return it != boolParams.end() ? it->second : defaultValue;
    }

    std::string getString(uint32_t id, const std::string& defaultValue = "") const
    {
        auto it = stringParams.find(id);
        return it != stringParams.end() ? it->second : defaultValue;
    }

    double getDouble(uint32_t id, double defaultValue = 0.0) const
    {
        auto it = doubleParams.find(id);
        return it != doubleParams.end() ? it->second : defaultValue;
    }

    uint32_t getUInt32(uint32_t id, uint32_t defaultValue = 0) const
    {
        auto it = uint32Params.find(id);
        return it != uint32Params.end() ? it->second : defaultValue;
    }

    uint64_t getUInt64(uint32_t id, uint64_t defaultValue = 0) const
    {
        auto it = uint64Params.find(id);
        return it != uint64Params.end() ? it->second : defaultValue;
    }

    void clear()
    {
        floatParams.clear();
        intParams.clear();
        boolParams.clear();
        stringParams.clear();
        doubleParams.clear();
        uint32Params.clear();
        uint64Params.clear();
    }
};

struct HostTimelineData
{
    // Currently implementing via JUCE 6, however JUCE 7 will change require a change to this struct design
    bool isPlaying;
    double playheadPositionInSeconds;

    // TODO: Implement the following after upgrading project to JUCE 7
    // double hostBPM; // Used to calculate loop points in seconds
    // double loopStartPositionInSeconds; // for more detailed indication on timeline indicator
    // double loopEndPositionInSeconds; // for more detailed indication on timeline indicator
    // double editOriginPositionInSeconds;
}; 