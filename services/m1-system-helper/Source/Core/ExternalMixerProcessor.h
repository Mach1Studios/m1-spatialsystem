#pragma once

#include <JuceHeader.h>
#include "../Common/Common.h"
#include <memory>
#include <vector>
#include <unordered_map>

#include <Mach1Encode.h>
#include <Mach1Decode.h>

namespace Mach1 {

struct MixerTrackInfo {
    int pluginPort = 0;
    juce::String trackName;
    juce::Colour trackColor = juce::Colours::grey;
    bool active = false;
    bool muted = false;
    float gain = 1.0f;
    
    // M1Encode settings from plugin
    float azimuth = 0.0f;
    float elevation = 0.0f;
    float diverge = 0.0f;
    bool st_auto_orbit = true;
    float st_spread = 0.0f;
    float st_azimuth = 0.0f;
    float st_balance = 0.0f;
    int panner_mode = 0;
    int inputMode = 0;
    int outputMode = 0;
    
    // Meter values
    std::vector<float> inputLevels;
    juce::int64 lastUpdateTime = 0;
    
    // Each track has its own M1Encode instance
    std::unique_ptr<Mach1Encode<float>> m1Encode;
};

class ExternalMixerProcessor {
public:
    ExternalMixerProcessor();
    ~ExternalMixerProcessor();
    
    // Initialize the processor
    void initialize(double sampleRate, int maxBlockSize);
    
    // Audio processing
    void processAudioBlock(float* const* outputChannels, int numChannels, int numSamples);
    
    // Track management  
    void addTrack(int pluginPort, const juce::String& trackName);
    void removeTrack(int pluginPort);
    void updateTrackSettings(int pluginPort, float azimuth, float elevation, float diverge, float gain);
    
    // Mixer control
    void setTrackGain(int pluginPort, float gain);
    void setTrackMute(int pluginPort, bool muted);
    
    // Master output control
    void setOutputFormat(int formatMode);
    void setMasterYPR(float yaw, float pitch, float roll);
    int getOutputChannelCount() const;
    
    // Metering
    std::vector<float> getOutputLevels() const;
    std::vector<float> getTrackInputLevels(int pluginPort) const;
    
    // Track info
    std::vector<MixerTrackInfo> getTrackInfo() const;
    bool hasActiveTracks() const;
    
    // Recording/Export
    void startRecording(const juce::File& outputFile);
    void stopRecording();
    bool isRecording() const { return recording; }
    
private:
    void updateTrackLevels();
    void processTrack(int pluginPort, MixerTrackInfo& track, float* const* mixChannels, int numSamples);
    void applyMasterDecoding(float* const* channels, int numChannels, int numSamples);
    
    // Audio processing members
    double sampleRate = 44100.0;
    int blockSize = 512;
    int maxChannels = 14;  // Maximum channels to support
    
    // Mach1 API objects - one decode for master output, multiple encode per track
    std::unique_ptr<Mach1Decode<float>> m1Decode;         // Single decoder for master output
    
    // Track management - each track contains its own M1Encode
    std::unordered_map<int, MixerTrackInfo> trackMap;   // Using map for faster lookup by pluginPort
    juce::CriticalSection tracksMutex;
    
    // Processing buffers (raw float arrays)
    std::vector<std::vector<float>> spatialMixBuffer;      // Multichannel spatial mix
    std::vector<std::vector<float>> trackProcessBuffer;    // Per-track processing
    std::vector<std::vector<float>> tempBuffer;            // Temporary calculations
    
    // Output format and decoding
    int currentOutputFormat = 0; // M1Spatial_14 by default
    float masterYaw = 0.0f, masterPitch = 0.0f, masterRoll = 0.0f;
    
    // Metering (simple smoothing)
    std::vector<float> outputLevelSmoothers;
    std::vector<float> currentOutputLevels;
    
    // Recording
    bool recording = false;
    juce::File recordingFile;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ExternalMixerProcessor)
};

} // namespace Mach1 
