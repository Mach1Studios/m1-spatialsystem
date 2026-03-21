#pragma once

#include <JuceHeader.h>
#include "../Common/Common.h"
#include "../Managers/PannerTrackingManager.h"
#include <memory>
#include <vector>
#include <unordered_map>

#include <Mach1Encode.h>
#include <Mach1Decode.h>

namespace Mach1 {

class PannerTrackingManager;

struct PerPannerEncoder {
    std::unique_ptr<Mach1Encode<float>> m1Encode;

    // Working buffers in the format expected by Mach1 API
    std::vector<std::vector<float>> inputBuf;   // [inputChannels][samples]
    std::vector<std::vector<float>> encodedBuf; // [outputChannels][samples]

    int lastInputMode = -1;
    int lastOutputMode = -1;
    int lastPannerMode = -1;
    int allocatedSamples = 0;
    int allocatedInputChans = 0;
    int allocatedOutputChans = 0;
};

struct MixerTrackInfo {
    int pluginPort = 0;
    juce::String trackName;
    juce::Colour trackColor = juce::Colours::grey;
    bool active = false;
    bool muted = false;
    float gain = 1.0f;

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

    std::vector<float> inputLevels;
    juce::int64 lastUpdateTime = 0;
};

class ExternalMixerProcessor {
public:
    ExternalMixerProcessor();
    ~ExternalMixerProcessor();
    
    void initialize(double sampleRate, int maxBlockSize);
    void setPannerTrackingManager(PannerTrackingManager* manager);
    
    // Audio processing
    void processAudioBlock(float* const* outputChannels, int numChannels, int numSamples);
    void processMemorySharePanners(int numSamples);
    
    // Track management (legacy OSC-based, kept for compatibility)
    void addTrack(int pluginPort, const juce::String& trackName);
    void removeTrack(int pluginPort);
    void updateTrackSettings(int pluginPort, float azimuth, float elevation, float diverge, float gain);
    
    void setTrackGain(int pluginPort, float gain);
    void setTrackMute(int pluginPort, bool muted);
    
    // Master output
    void setOutputFormat(int formatMode);
    void setMasterYPR(float yaw, float pitch, float roll);
    int getOutputChannelCount() const;
    
    // Metering
    std::vector<float> getOutputLevels() const;
    std::vector<float> getTrackInputLevels(int pluginPort) const;
    
    std::vector<MixerTrackInfo> getTrackInfo() const;
    bool hasActiveTracks() const;
    
    // Recording
    void startRecording(const juce::File& outputFile);
    void stopRecording();
    bool isRecording() const { return recording; }
    
private:
    void updateTrackLevels(int numSamples);
    void processTrack(int pluginPort, MixerTrackInfo& track, float* const* mixChannels, int numSamples);
    void applyMasterDecoding(float* const* channels, int numChannels, int numSamples);
    
    PerPannerEncoder& getOrCreateEncoder(uint32_t processId);
    void configureEncoder(PerPannerEncoder& enc, const MemorySharePannerInfo& panner, int numSamples);
    void cleanupStaleEncoders(const std::vector<MemorySharePannerInfo>& activePanners);
    
    double sampleRate = 44100.0;
    int blockSize = 512;
    int spatialChannelCount = 8; // M1Spatial_8 default

    std::unique_ptr<Mach1Decode<float>> m1Decode;
    
    // Legacy OSC track map
    std::unordered_map<int, MixerTrackInfo> trackMap;
    juce::CriticalSection tracksMutex;
    
    // Per-panner M1Encode instances keyed by PID
    std::unordered_map<uint32_t, PerPannerEncoder> pannerEncoders;
    
    // Processing buffers (spatialChannelCount channels x blockSize samples)
    std::vector<std::vector<float>> spatialMixBuffer;
    
    // Decode working buffers
    std::vector<std::vector<float>> decodeSrcBuffer;  // [spatialChannelCount][samples]
    std::vector<std::vector<float>> decodeOutBuffer;   // [2][samples] stereo output
    
    // Output format and head-tracking
    Mach1EncodeOutputMode currentEncodeOutputMode = M1Spatial_8;
    Mach1DecodeMode currentDecodeMode = M1DecodeSpatial_8;
    float masterYaw = 0.0f, masterPitch = 0.0f, masterRoll = 0.0f;
    
    // Metering
    std::vector<float> currentOutputLevels;
    
    bool recording = false;
    juce::File recordingFile;
    
    PannerTrackingManager* pannerTrackingManager = nullptr;
    juce::AudioBuffer<float> streamingReadBuffer;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ExternalMixerProcessor)
};

} // namespace Mach1 
