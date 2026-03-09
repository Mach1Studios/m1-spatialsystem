#include "ExternalMixerProcessor.h"
#include "../Managers/M1MemoryShareTracker.h"
#include "../Common/TypesForDataExchange.h"

namespace Mach1 {

ExternalMixerProcessor::ExternalMixerProcessor() {}

ExternalMixerProcessor::~ExternalMixerProcessor() {
    if (recording) {
        stopRecording();
    }
}

void ExternalMixerProcessor::initialize(double sr, int maxBlockSize) {
    sampleRate = sr;
    blockSize = maxBlockSize;
    
    // Determine channel count from current output format
    switch (currentEncodeOutputMode) {
        case M1Spatial_4:  spatialChannelCount = 4;  currentDecodeMode = M1DecodeSpatial_4;  break;
        case M1Spatial_8:  spatialChannelCount = 8;  currentDecodeMode = M1DecodeSpatial_8;  break;
        case M1Spatial_14: spatialChannelCount = 14; currentDecodeMode = M1DecodeSpatial_14; break;
    }
    
    spatialMixBuffer.resize(spatialChannelCount);
    for (int ch = 0; ch < spatialChannelCount; ++ch)
        spatialMixBuffer[ch].resize(maxBlockSize, 0.0f);
    
    decodeSrcBuffer.resize(spatialChannelCount);
    for (int ch = 0; ch < spatialChannelCount; ++ch)
        decodeSrcBuffer[ch].resize(maxBlockSize, 0.0f);
    
    decodeOutBuffer.resize(spatialChannelCount);
    for (int ch = 0; ch < spatialChannelCount; ++ch)
        decodeOutBuffer[ch].resize(maxBlockSize, 0.0f);
    
    streamingReadBuffer.setSize(spatialChannelCount, maxBlockSize);
    currentOutputLevels.resize(2, 0.0f); // stereo output
    
    m1Decode = std::make_unique<Mach1Decode<float>>();
    m1Decode->setDecodeMode(currentDecodeMode);
    m1Decode->setPlatformType(Mach1PlatformDefault);
}

void ExternalMixerProcessor::setPannerTrackingManager(PannerTrackingManager* manager) {
    pannerTrackingManager = manager;
}

void ExternalMixerProcessor::processAudioBlock(float* const* outputChannels, int numChannels, int numSamples) {
    for (int ch = 0; ch < numChannels; ++ch)
        if (outputChannels[ch])
            juce::FloatVectorOperations::clear(outputChannels[ch], numSamples);
    
    for (int ch = 0; ch < spatialChannelCount; ++ch)
        std::fill(spatialMixBuffer[ch].begin(), spatialMixBuffer[ch].begin() + numSamples, 0.0f);
    
    processMemorySharePanners(numSamples);
    applyMasterDecoding(outputChannels, numChannels, numSamples);
    updateTrackLevels(numSamples);
}

// ---------------------------------------------------------------------------
// Per-panner M1Encode management
// ---------------------------------------------------------------------------

PerPannerEncoder& ExternalMixerProcessor::getOrCreateEncoder(uint32_t processId) {
    auto it = pannerEncoders.find(processId);
    if (it == pannerEncoders.end()) {
        PerPannerEncoder enc;
        enc.m1Encode = std::make_unique<Mach1Encode<float>>();
        auto result = pannerEncoders.emplace(processId, std::move(enc));
        return result.first->second;
    }
    return it->second;
}

void ExternalMixerProcessor::configureEncoder(PerPannerEncoder& enc, const MemorySharePannerInfo& panner, int numSamples) {
    auto& e = *enc.m1Encode;
    
    int inputMode  = panner.getInputMode();
    int outputMode = panner.getOutputMode();
    int pannerMode = 0; // default IsotropicLinear

    bool autoOrbit = panner.getAutoOrbit();
    if (panner.parameters.boolParams.count(M1SystemHelperParameterIDs::ISOTROPIC_MODE)) {
        bool isotropic = panner.parameters.boolParams.at(M1SystemHelperParameterIDs::ISOTROPIC_MODE);
        bool equalpower = false;
        if (panner.parameters.boolParams.count(M1SystemHelperParameterIDs::EQUALPOWER_MODE))
            equalpower = panner.parameters.boolParams.at(M1SystemHelperParameterIDs::EQUALPOWER_MODE);
        
        if (equalpower)       pannerMode = IsotropicEqualPower;
        else if (isotropic)   pannerMode = IsotropicLinear;
        else                  pannerMode = PeriphonicLinear;
    }
    
    // Only reconfigure modes when they change to avoid unnecessary recalculation
    if (inputMode != enc.lastInputMode) {
        e.setInputMode(static_cast<Mach1EncodeInputMode>(inputMode));
        enc.lastInputMode = inputMode;
    }
    if (outputMode != enc.lastOutputMode) {
        e.setOutputMode(static_cast<Mach1EncodeOutputMode>(outputMode));
        enc.lastOutputMode = outputMode;
    }
    if (pannerMode != enc.lastPannerMode) {
        e.setPannerMode(static_cast<Mach1EncodePannerMode>(pannerMode));
        enc.lastPannerMode = pannerMode;
    }
    
    e.setAzimuth(panner.getAzimuth());
    e.setElevation(panner.getElevation());
    e.setDiverge(panner.getDiverge());
    e.setStereoSpread(panner.getStereoSpread());
    e.setAutoOrbit(autoOrbit);
    e.setOrbitRotation(panner.getStereoOrbitAzimuth());

    if (panner.parameters.boolParams.count(M1SystemHelperParameterIDs::GAIN_COMPENSATION_MODE))
        e.setGainCompensationActive(panner.parameters.boolParams.at(M1SystemHelperParameterIDs::GAIN_COMPENSATION_MODE));
    
    e.generatePointResults();
    
    int inChans  = e.getInputChannelsCount();
    int outChans = e.getOutputChannelsCount();
    
    if (enc.allocatedSamples < numSamples || enc.allocatedInputChans != inChans || enc.allocatedOutputChans != outChans) {
        enc.inputBuf.resize(inChans);
        for (int ch = 0; ch < inChans; ++ch)
            enc.inputBuf[ch].resize(numSamples, 0.0f);
        
        enc.encodedBuf.resize(outChans);
        for (int ch = 0; ch < outChans; ++ch)
            enc.encodedBuf[ch].resize(numSamples, 0.0f);
        
        enc.allocatedSamples = numSamples;
        enc.allocatedInputChans = inChans;
        enc.allocatedOutputChans = outChans;
    }
}

void ExternalMixerProcessor::cleanupStaleEncoders(const std::vector<MemorySharePannerInfo>& activePanners) {
    std::vector<uint32_t> toRemove;
    for (auto& [pid, enc] : pannerEncoders) {
        bool found = false;
        for (auto& p : activePanners)
            if (p.processId == pid && p.isConnected) { found = true; break; }
        if (!found)
            toRemove.push_back(pid);
    }
    for (auto pid : toRemove)
        pannerEncoders.erase(pid);
}

// ---------------------------------------------------------------------------
// Memory-share panner processing: read raw audio, M1Encode, mix into spatial
// ---------------------------------------------------------------------------

void ExternalMixerProcessor::processMemorySharePanners(int numSamples) {
    if (!pannerTrackingManager) return;
    
    auto* memShareTracker = pannerTrackingManager->getMemoryShareTracker();
    if (!memShareTracker) return;
    
    const auto& panners = memShareTracker->getActivePanners();
    if (panners.empty()) return;
    
    if (streamingReadBuffer.getNumChannels() < spatialChannelCount ||
        streamingReadBuffer.getNumSamples() < numSamples)
        streamingReadBuffer.setSize(spatialChannelCount, numSamples, false, true, false);
    
    for (const auto& pannerInfo : panners) {
        if (!pannerInfo.isConnected || !pannerInfo.memoryShare || !pannerInfo.memoryShare->isValid())
            continue;
        
        streamingReadBuffer.clear();
        
        ParameterMap currentParams;
        uint64_t dawTimestamp = 0;
        double playheadPosition = 0.0;
        bool isPlaying = false;
        uint64_t bufferId = 0;
        uint32_t updateSource = 0;

        bool readSuccess = pannerInfo.memoryShare->readAudioBufferWithGenericParameters(
            streamingReadBuffer, currentParams, dawTimestamp, playheadPosition, isPlaying, bufferId, updateSource
        );
        
        if (!readSuccess) continue;
        
        // Get the actual number of channels in the read buffer (from the panner's write)
        int readChannels = static_cast<int>(pannerInfo.channels);
        if (readChannels <= 0) readChannels = 1;
        readChannels = juce::jmin(readChannels, streamingReadBuffer.getNumChannels());
        
        // Apply per-track gain from the panner parameters
        float pannerGain = pannerInfo.getGain();
        // Gain is in dB in some codepaths; if it's in linear [0,1], use directly.
        // The panner stores gain as a linear float [0..1] based on the PluginProcessor code.
        pannerGain = juce::jlimit(0.0f, 2.0f, pannerGain);
        
        // Get or create an M1Encode for this panner
        auto& enc = getOrCreateEncoder(pannerInfo.processId);
        configureEncoder(enc, pannerInfo, numSamples);
        
        int inChans  = enc.m1Encode->getInputChannelsCount();
        int outChans = enc.m1Encode->getOutputChannelsCount();
        
        // Copy raw audio into the encoder's input buffer
        for (int ch = 0; ch < inChans; ++ch) {
            if (ch < readChannels) {
                const float* src = streamingReadBuffer.getReadPointer(ch);
                for (int i = 0; i < numSamples; ++i)
                    enc.inputBuf[ch][i] = src[i] * pannerGain;
            } else {
                std::fill(enc.inputBuf[ch].begin(), enc.inputBuf[ch].begin() + numSamples, 0.0f);
            }
        }
        
        // Clear encoded buffer before accumulation
        for (int ch = 0; ch < outChans; ++ch)
            std::fill(enc.encodedBuf[ch].begin(), enc.encodedBuf[ch].begin() + numSamples, 0.0f);
        
        // M1Encode: raw input → spatial multichannel
        enc.m1Encode->encodeBuffer(enc.inputBuf, enc.encodedBuf, numSamples);
        
        // Mix encoded result into the spatial mix buffer
        int chansToMix = juce::jmin(outChans, spatialChannelCount);
        for (int ch = 0; ch < chansToMix; ++ch) {
            for (int i = 0; i < numSamples; ++i)
                spatialMixBuffer[ch][i] += enc.encodedBuf[ch][i];
        }
    }
    
    cleanupStaleEncoders(panners);
}

// ---------------------------------------------------------------------------
// Master decoding: spatial mix → stereo output with head tracking
// ---------------------------------------------------------------------------

void ExternalMixerProcessor::applyMasterDecoding(float* const* channels, int numChannels, int numSamples) {
    if (!m1Decode || numChannels < 2) return;
    
    // Set head-tracking orientation
    Mach1Point3D rotation;
    rotation.x = masterYaw;
    rotation.y = masterPitch;
    rotation.z = masterRoll;
    m1Decode->setRotationDegrees(rotation);
    
    // Copy spatialMixBuffer into the decode source format
    for (int ch = 0; ch < spatialChannelCount; ++ch) {
        for (int i = 0; i < numSamples; ++i)
            decodeSrcBuffer[ch][i] = spatialMixBuffer[ch][i];
    }
    
    // M1Decode: spatial multichannel → stereo
    m1Decode->decodeBuffer(decodeSrcBuffer, decodeOutBuffer, numSamples);
    
    // Copy stereo output to the actual output channels
    for (int ch = 0; ch < juce::jmin(numChannels, 2); ++ch) {
        if (channels[ch]) {
            for (int i = 0; i < numSamples; ++i)
                channels[ch][i] = decodeOutBuffer[ch][i];
        }
    }
}

// ---------------------------------------------------------------------------
// Legacy OSC track management (kept for backward compatibility)
// ---------------------------------------------------------------------------

void ExternalMixerProcessor::addTrack(int pluginPort, const juce::String& trackName) {
    juce::ScopedLock lock(tracksMutex);
    MixerTrackInfo track;
    track.pluginPort = pluginPort;
    track.trackName = trackName;
    track.active = true;
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
    }
}

void ExternalMixerProcessor::setTrackGain(int pluginPort, float gain) {
    juce::ScopedLock lock(tracksMutex);
    auto it = trackMap.find(pluginPort);
    if (it != trackMap.end())
        it->second.gain = gain;
}

void ExternalMixerProcessor::setTrackMute(int pluginPort, bool muted) {
    juce::ScopedLock lock(tracksMutex);
    auto it = trackMap.find(pluginPort);
    if (it != trackMap.end())
        it->second.muted = muted;
}

void ExternalMixerProcessor::setOutputFormat(int formatMode) {
    currentEncodeOutputMode = static_cast<Mach1EncodeOutputMode>(formatMode);
    
    switch (currentEncodeOutputMode) {
        case M1Spatial_4:  spatialChannelCount = 4;  currentDecodeMode = M1DecodeSpatial_4;  break;
        case M1Spatial_8:  spatialChannelCount = 8;  currentDecodeMode = M1DecodeSpatial_8;  break;
        case M1Spatial_14: spatialChannelCount = 14; currentDecodeMode = M1DecodeSpatial_14; break;
    }
    
    if (m1Decode)
        m1Decode->setDecodeMode(currentDecodeMode);
    
    // Force re-initialization of all encoders on next process call
    pannerEncoders.clear();
    
    initialize(sampleRate, blockSize);
}

void ExternalMixerProcessor::setMasterYPR(float yaw, float pitch, float roll) {
    masterYaw = yaw;
    masterPitch = pitch;
    masterRoll = roll;
}

int ExternalMixerProcessor::getOutputChannelCount() const {
    return 2; // Always produces stereo after M1Decode
}

std::vector<float> ExternalMixerProcessor::getOutputLevels() const {
    return currentOutputLevels;
}

std::vector<float> ExternalMixerProcessor::getTrackInputLevels(int pluginPort) const {
    juce::ScopedLock lock(tracksMutex);
    auto it = trackMap.find(pluginPort);
    if (it != trackMap.end())
        return it->second.inputLevels;
    return {};
}

std::vector<MixerTrackInfo> ExternalMixerProcessor::getTrackInfo() const {
    juce::ScopedLock lock(tracksMutex);
    std::vector<MixerTrackInfo> result;
    for (const auto& [port, track] : trackMap) {
        MixerTrackInfo copy;
        copy.pluginPort = track.pluginPort;
        copy.trackName = track.trackName;
        copy.active = track.active;
        copy.muted = track.muted;
        copy.gain = track.gain;
        copy.azimuth = track.azimuth;
        copy.elevation = track.elevation;
        copy.diverge = track.diverge;
        result.push_back(std::move(copy));
    }
    return result;
}

bool ExternalMixerProcessor::hasActiveTracks() const {
    if (pannerTrackingManager && pannerTrackingManager->hasPanners())
        return true;
    juce::ScopedLock lock(tracksMutex);
    for (const auto& [port, track] : trackMap)
        if (track.active) return true;
    return false;
}

void ExternalMixerProcessor::startRecording(const juce::File& outputFile) {
    recordingFile = outputFile;
    recording = true;
}

void ExternalMixerProcessor::stopRecording() {
    recording = false;
}

void ExternalMixerProcessor::updateTrackLevels(int numSamples) {
    if (currentOutputLevels.size() < 2) currentOutputLevels.resize(2, 0.0f);
    
    for (int ch = 0; ch < 2; ++ch) {
        float peak = 0.0f;
        if (ch < (int)decodeOutBuffer.size()) {
            for (int i = 0; i < numSamples && i < (int)decodeOutBuffer[ch].size(); ++i)
                peak = juce::jmax(peak, std::abs(decodeOutBuffer[ch][i]));
        }
        // Simple smoothing
        currentOutputLevels[ch] = currentOutputLevels[ch] * 0.85f + peak * 0.15f;
    }
}

void ExternalMixerProcessor::processTrack(int /*pluginPort*/, MixerTrackInfo& /*track*/, float* const* /*mixChannels*/, int /*numSamples*/) {
    // Legacy OSC track processing - not used when all panners go through memory share
}

} // namespace Mach1 
