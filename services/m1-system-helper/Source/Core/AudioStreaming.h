#pragma once

#include <JuceHeader.h>
#include "../Common/Common.h"
#include <memory>
#include <unordered_map>

namespace Mach1 {

// Shared memory structure for audio streaming
struct AudioStreamHeader {
    static constexpr int MAGIC_NUMBER = 0x4D314155; // "M1AU"
    static constexpr int VERSION = 1;
    
    int magic = MAGIC_NUMBER;
    int version = VERSION;
    int sampleRate = 44100;
    int bufferSize = 512;
    int numChannels = 2;
    int numSamples = 0;
    bool isPlaying = false;
    bool dataReady = false;
    juce::int64 timestamp = 0;
    
    // Plugin identification
    int pluginPort = 0;
    char pluginName[64] = {0};
    
    // M1Encode settings for processing
    float azimuth = 0.0f;
    float elevation = 0.0f;
    float diverge = 0.0f;
    float gain = 1.0f;
    int inputMode = 0;
    int outputMode = 0;
};

// Audio buffer in shared memory (header + audio data)
struct AudioStreamBuffer {
    AudioStreamHeader header;
    // Audio data follows immediately after header
    // float audioData[numChannels * bufferSize];
    
    float* getAudioData() {
        return reinterpret_cast<float*>(this + 1);
    }
    
    const float* getAudioData() const {
        return reinterpret_cast<const float*>(this + 1);
    }
    
    size_t getTotalSize() const {
        return sizeof(AudioStreamHeader) + 
               (header.numChannels * header.bufferSize * sizeof(float));
    }
};

class AudioStreamManager {
public:
    AudioStreamManager();
    ~AudioStreamManager();
    
    // Plugin registration for streaming
    juce::Result registerPluginStream(int pluginPort, const juce::String& pluginName, 
                                     int numChannels, int sampleRate, int bufferSize);
    void unregisterPluginStream(int pluginPort);
    
    // Audio streaming interface
    bool writeAudioData(int pluginPort, const float* const* channelData, int numSamples);
    bool readAudioData(int pluginPort, float* const* channelData, int numSamples);
    
    // Stream management
    std::vector<int> getActiveStreams() const;
    AudioStreamHeader getStreamInfo(int pluginPort) const;
    bool isStreamActive(int pluginPort) const;
    
    // M1Encode coefficient updates
    void updatePluginSettings(int pluginPort, const AudioStreamHeader& settings);
    
private:
    struct StreamInfo {
        std::unique_ptr<juce::MemoryMappedFile> mappedFile;
        AudioStreamBuffer* buffer = nullptr;
        juce::String sharedMemoryName;
        juce::CriticalSection mutex;
        juce::int64 lastUpdateTime = 0;
    };
    
    std::unordered_map<int, std::unique_ptr<StreamInfo>> streams;
    juce::CriticalSection streamsMutex;
    
    juce::String generateSharedMemoryName(int pluginPort);
    bool createSharedMemory(int pluginPort, size_t size);
    void cleanupInactiveStreams();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioStreamManager)
};

} // namespace Mach1 