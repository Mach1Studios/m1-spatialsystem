#include "ExternalMixerProcessor.h"

namespace Mach1 {

ExternalMixerProcessor::ExternalMixerProcessor() 
    : sampleRate(44100.0)
    , blockSize(512)
    , maxChannels(16)
    , currentOutputFormat(0)
    , masterYaw(0.0f)
    , masterPitch(0.0f) 
    , masterRoll(0.0f)
    , recording(false) {
    // Constructor implementation
}

ExternalMixerProcessor::~ExternalMixerProcessor() {
    // Destructor implementation - unique_ptrs will clean up automatically
    if (recording) {
        stopRecording();
    }
}

void ExternalMixerProcessor::initialize(double sampleRate, int maxBlockSize) {
    this->sampleRate = sampleRate;
    this->blockSize = maxBlockSize;
    
    // Initialize processing buffers
    spatialMixBuffer.resize(maxChannels);
    trackProcessBuffer.resize(maxChannels);
    tempBuffer.resize(maxChannels);
    
    for (int ch = 0; ch < maxChannels; ++ch) {
        spatialMixBuffer[ch].resize(maxBlockSize, 0.0f);
        trackProcessBuffer[ch].resize(maxBlockSize, 0.0f);
        tempBuffer[ch].resize(maxBlockSize, 0.0f);
    }
    
    // Initialize metering
    outputLevelSmoothers.resize(maxChannels, 0.0f);
    currentOutputLevels.resize(maxChannels, 0.0f);
    
    // Initialize Mach1 objects
    m1Decode = std::make_unique<Mach1Decode<float>>();
}

void ExternalMixerProcessor::processAudioBlock(float* const* outputChannels, int numChannels, int numSamples) {
    // Clear output channels
    for (int ch = 0; ch < numChannels; ++ch) {
        if (outputChannels[ch]) {
            for (int i = 0; i < numSamples; ++i) {
                outputChannels[ch][i] = 0.0f;
            }
        }
    }
    
    // Process each active track
    juce::ScopedLock lock(tracksMutex);
    for (auto& [pluginPort, track] : trackMap) {
        if (track.active && !track.muted) {
            processTrack(pluginPort, track, outputChannels, numSamples);
        }
    }
    
    // Apply master decoding
    applyMasterDecoding(outputChannels, numChannels, numSamples);
    
    // Update metering
    updateTrackLevels();
}

void ExternalMixerProcessor::addTrack(int pluginPort, const juce::String& trackName) {
    juce::ScopedLock lock(tracksMutex);
    
    // Create new track info
    MixerTrackInfo track;
    track.pluginPort = pluginPort;
    track.trackName = trackName;
    track.active = true;
    
    // Initialize M1Encode for this track
    track.m1Encode = std::make_unique<Mach1Encode<float>>();
    
    trackMap[pluginPort] = std::move(track);
}

void ExternalMixerProcessor::removeTrack(int pluginPort) {
    juce::ScopedLock lock(tracksMutex);
    trackMap.erase(pluginPort);
}

void ExternalMixerProcessor::updateTrackSettings(int pluginPort, float azimuth, float elevation, float diverge, float gain) {
    juce::ScopedLock lock(tracksMutex);
    
    auto it = trackMap.find(pluginPort);
    if (it != trackMap.end()) {
        auto& track = it->second;
        track.azimuth = azimuth;
        track.elevation = elevation;
        track.diverge = diverge;
        track.gain = gain;
        
        // Update M1Encode settings
        if (track.m1Encode) {
            track.m1Encode->setAzimuth(azimuth);
            track.m1Encode->setElevation(elevation);
            track.m1Encode->setDiverge(diverge);
        }
    }
}

void ExternalMixerProcessor::setTrackGain(int pluginPort, float gain) {
    juce::ScopedLock lock(tracksMutex);
    
    auto it = trackMap.find(pluginPort);
    if (it != trackMap.end()) {
        it->second.gain = gain;
    }
}

void ExternalMixerProcessor::setTrackMute(int pluginPort, bool muted) {
    juce::ScopedLock lock(tracksMutex);
    
    auto it = trackMap.find(pluginPort);
    if (it != trackMap.end()) {
        it->second.muted = muted;
    }
}

void ExternalMixerProcessor::setOutputFormat(int formatMode) {
    currentOutputFormat = formatMode;
    
    // Update M1Decode format
    if (m1Decode) {
        // m1Decode->setFormat(formatMode); // Would need to check the actual API
    }
}

void ExternalMixerProcessor::setMasterYPR(float yaw, float pitch, float roll) {
    masterYaw = yaw;
    masterPitch = pitch;
    masterRoll = roll;
    
    // Update M1Decode orientation
    if (m1Decode) {
        // m1Decode->setYaw(yaw);     // Would need to check the actual API
        // m1Decode->setPitch(pitch); // Would need to check the actual API  
        // m1Decode->setRoll(roll);   // Would need to check the actual API
    }
}

int ExternalMixerProcessor::getOutputChannelCount() const {
    return maxChannels; // Or based on current output format
}

std::vector<float> ExternalMixerProcessor::getOutputLevels() const {
    return currentOutputLevels;
}

std::vector<float> ExternalMixerProcessor::getTrackInputLevels(int pluginPort) const {
    juce::ScopedLock lock(tracksMutex);
    
    auto it = trackMap.find(pluginPort);
    if (it != trackMap.end()) {
        return it->second.inputLevels;
    }
    return std::vector<float>();
}

std::vector<MixerTrackInfo> ExternalMixerProcessor::getTrackInfo() const {
    juce::ScopedLock lock(tracksMutex);
    
    std::vector<MixerTrackInfo> trackList;
    for (const auto& [pluginPort, track] : trackMap) {
        MixerTrackInfo trackCopy;
        trackCopy.pluginPort = track.pluginPort;
        trackCopy.trackName = track.trackName;
        trackCopy.active = track.active;
        // ... copy other fields but skip m1Encode
        trackList.push_back(std::move(trackCopy));
    }
    return trackList;
}

bool ExternalMixerProcessor::hasActiveTracks() const {
    juce::ScopedLock lock(tracksMutex);
    
    for (const auto& [pluginPort, track] : trackMap) {
        if (track.active) {
            return true;
        }
    }
    return false;
}

void ExternalMixerProcessor::startRecording(const juce::File& outputFile) {
    recordingFile = outputFile;
    recording = true;
}

void ExternalMixerProcessor::stopRecording() {
    recording = false;
}

void ExternalMixerProcessor::updateTrackLevels() {
    // Update metering levels - basic implementation
    for (int ch = 0; ch < currentOutputLevels.size(); ++ch) {
        // Simple smoothing
        outputLevelSmoothers[ch] *= 0.95f; // Decay
        currentOutputLevels[ch] = outputLevelSmoothers[ch];
    }
}

void ExternalMixerProcessor::processTrack(int pluginPort, MixerTrackInfo& track, float* const* mixChannels, int numSamples) {
    // This would:
    // 1. Get audio data from AudioStreamManager for this pluginPort
    // 2. Apply M1Encode with track's spatial settings
    // 3. Mix the encoded result into the spatial mix buffer
    // 4. Update input level metering
    
    if (track.m1Encode) {
        // Example of how this would work:
        // auto gains = track.m1Encode->getGains();
        // Apply encoding and mix into spatialMixBuffer
    }
}

void ExternalMixerProcessor::applyMasterDecoding(float* const* channels, int numChannels, int numSamples) {
    // This would:
    // 1. Take the spatial mix buffer 
    // 2. Apply M1Decode to convert to desired output format
    // 3. Write result to output channels
    
    if (m1Decode) {
        // Example of how this would work:
        // m1Decode->decode(spatialMixBuffer, channels, numSamples);
    }
}

} // namespace Mach1 
