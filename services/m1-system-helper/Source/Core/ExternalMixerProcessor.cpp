#include "ExternalMixerProcessor.h"
#include "../Managers/M1MemoryShareTracker.h"
#include "../Common/TypesForDataExchange.h"

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
}

ExternalMixerProcessor::~ExternalMixerProcessor() {
    // unique_ptrs will clean up automatically
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
    
    // Initialize streaming read buffer
    streamingReadBuffer.setSize(maxChannels, maxBlockSize);
    
    // Initialize metering
    outputLevelSmoothers.resize(maxChannels, 0.0f);
    currentOutputLevels.resize(maxChannels, 0.0f);
    
    // Initialize Mach1 objects
    m1Decode = std::make_unique<Mach1Decode<float>>();
}

void ExternalMixerProcessor::setPannerTrackingManager(PannerTrackingManager* manager) {
    pannerTrackingManager = manager;
    DBG("[ExternalMixerProcessor] Panner tracking manager set");
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
    
    // Clear spatial mix buffer for this block
    for (int ch = 0; ch < maxChannels; ++ch) {
        std::fill(spatialMixBuffer[ch].begin(), spatialMixBuffer[ch].begin() + numSamples, 0.0f);
    }
    
    // Process memory-share panners and mix into spatialMixBuffer
    processMemorySharePanners(numSamples);
    
    // Process each active OSC track
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

void ExternalMixerProcessor::processMemorySharePanners(int numSamples) {
    if (!pannerTrackingManager) {
        return;
    }
    
    auto* memShareTracker = pannerTrackingManager->getMemoryShareTracker();
    if (!memShareTracker) {
        return;
    }
    
    // Get all active memory-share panners directly from the tracker
    const auto& panners = memShareTracker->getActivePanners();
    
    if (panners.empty()) {
        return;
    }
    
    // Ensure streaming read buffer is large enough
    if (streamingReadBuffer.getNumChannels() < maxChannels || 
        streamingReadBuffer.getNumSamples() < numSamples) {
        streamingReadBuffer.setSize(maxChannels, numSamples, false, true, false);
    }
    
    // Process each connected panner
    for (const auto& pannerInfo : panners) {
        if (!pannerInfo.isConnected || !pannerInfo.memoryShare || !pannerInfo.memoryShare->isValid()) {
            continue;
        }
        
        // Clear read buffer for this panner
        streamingReadBuffer.clear();
        
        // Try to read audio and parameters from this panner's M1MemoryShare
        ParameterMap currentParams;
        uint64_t dawTimestamp = 0;
        double playheadPosition = 0.0;
        bool isPlaying = false;
        uint64_t bufferId = 0;
        uint32_t updateSource = 0;

        bool readSuccess = pannerInfo.memoryShare->readAudioBufferWithGenericParameters(
            streamingReadBuffer, currentParams, dawTimestamp, playheadPosition, isPlaying, bufferId, updateSource
        );
        
        if (!readSuccess) {
            continue;
        }
        
        // Mix this panner's audio into the spatial mix buffer
        // The audio should already be Mach1-encoded by the panner
        const int channelsToMix = juce::jmin(maxChannels, streamingReadBuffer.getNumChannels());
        const int samplesToMix = juce::jmin(numSamples, streamingReadBuffer.getNumSamples());
        
        for (int ch = 0; ch < channelsToMix; ++ch) {
            const float* srcData = streamingReadBuffer.getReadPointer(ch);
            for (int i = 0; i < samplesToMix; ++i) {
                spatialMixBuffer[ch][i] += srcData[i];
            }
        }
        
        // Log parameter updates for debugging (remove for release builds)
        float azimuth = currentParams.getFloat(M1SystemHelperParameterIDs::AZIMUTH, 0.0f);
        float elevation = currentParams.getFloat(M1SystemHelperParameterIDs::ELEVATION, 0.0f);
        DBG("[ExternalMixerProcessor] Processed panner: " + juce::String(pannerInfo.name) + 
            " (PID: " + juce::String(pannerInfo.processId) + 
            ", azimuth: " + juce::String(azimuth) +
            ", elevation: " + juce::String(elevation) + ")");
    }
}

} // namespace Mach1 
