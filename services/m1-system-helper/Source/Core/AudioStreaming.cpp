#include "AudioStreaming.h"

namespace Mach1 {

AudioStreamManager::AudioStreamManager() {
    // Constructor implementation
}

AudioStreamManager::~AudioStreamManager() {
    juce::ScopedLock lock(streamsMutex);
    streams.clear();
}

juce::Result AudioStreamManager::registerPluginStream(int pluginPort, const juce::String& pluginName, 
                                                     int numChannels, int sampleRate, int bufferSize) {
    juce::ScopedLock lock(streamsMutex);
    
    // Check if already registered
    if (streams.find(pluginPort) != streams.end()) {
        return juce::Result::fail("Plugin stream already registered for port " + juce::String(pluginPort));
    }
    
    // Calculate buffer size
    size_t totalSize = sizeof(AudioStreamHeader) + (numChannels * bufferSize * sizeof(float));
    
    // Create new stream info
    auto streamInfo = std::make_unique<StreamInfo>();
    streamInfo->sharedMemoryName = generateSharedMemoryName(pluginPort);
    
    if (!createSharedMemory(pluginPort, totalSize)) {
        return juce::Result::fail("Failed to create shared memory for port " + juce::String(pluginPort));
    }
    
    streams[pluginPort] = std::move(streamInfo);
    
    return juce::Result::ok();
}

void AudioStreamManager::unregisterPluginStream(int pluginPort) {
    juce::ScopedLock lock(streamsMutex);
    streams.erase(pluginPort);
}

bool AudioStreamManager::writeAudioData(int pluginPort, const float* const* channelData, int numSamples) {
    juce::ScopedLock lock(streamsMutex);
    
    auto it = streams.find(pluginPort);
    if (it == streams.end() || !it->second || !it->second->buffer) {
        return false;
    }
    
    auto& streamInfo = it->second;
    juce::ScopedLock streamLock(streamInfo->mutex);
    
    AudioStreamBuffer* buffer = streamInfo->buffer;
    float* audioData = buffer->getAudioData();
    
    // Copy interleaved audio data
    int numChannels = buffer->header.numChannels;
    for (int sample = 0; sample < numSamples; ++sample) {
        for (int ch = 0; ch < numChannels; ++ch) {
            audioData[sample * numChannels + ch] = channelData[ch][sample];
        }
    }
    
    // Update header
    buffer->header.numSamples = numSamples;
    buffer->header.dataReady = true;
    buffer->header.timestamp = juce::Time::getCurrentTime().toMilliseconds();
    streamInfo->lastUpdateTime = buffer->header.timestamp;
    
    return true;
}

bool AudioStreamManager::readAudioData(int pluginPort, float* const* channelData, int numSamples) {
    juce::ScopedLock lock(streamsMutex);
    
    auto it = streams.find(pluginPort);
    if (it == streams.end() || !it->second || !it->second->buffer) {
        return false;
    }
    
    auto& streamInfo = it->second;
    juce::ScopedLock streamLock(streamInfo->mutex);
    
    AudioStreamBuffer* buffer = streamInfo->buffer;
    if (!buffer->header.dataReady) {
        return false;
    }
    
    const float* audioData = buffer->getAudioData();
    int numChannels = buffer->header.numChannels;
    int samplesToRead = juce::jmin(numSamples, buffer->header.numSamples);
    
    // Copy deinterleaved audio data
    for (int sample = 0; sample < samplesToRead; ++sample) {
        for (int ch = 0; ch < numChannels; ++ch) {
            channelData[ch][sample] = audioData[sample * numChannels + ch];
        }
    }
    
    // Mark as read
    buffer->header.dataReady = false;
    
    return true;
}

std::vector<int> AudioStreamManager::getActiveStreams() const {
    juce::ScopedLock lock(streamsMutex);
    
    std::vector<int> activeStreams;
    juce::int64 currentTime = juce::Time::getCurrentTime().toMilliseconds();
    
    for (const auto& [port, streamInfo] : streams) {
        if (streamInfo && (currentTime - streamInfo->lastUpdateTime) < 5000) { // 5 second timeout
            activeStreams.push_back(port);
        }
    }
    
    return activeStreams;
}

AudioStreamHeader AudioStreamManager::getStreamInfo(int pluginPort) const {
    juce::ScopedLock lock(streamsMutex);
    
    auto it = streams.find(pluginPort);
    if (it != streams.end() && it->second && it->second->buffer) {
        return it->second->buffer->header;
    }
    
    return AudioStreamHeader{};
}

bool AudioStreamManager::isStreamActive(int pluginPort) const {
    juce::ScopedLock lock(streamsMutex);
    
    auto it = streams.find(pluginPort);
    if (it == streams.end() || !it->second) {
        return false;
    }
    
    juce::int64 currentTime = juce::Time::getCurrentTime().toMilliseconds();
    return (currentTime - it->second->lastUpdateTime) < 5000; // 5 second timeout
}

void AudioStreamManager::updatePluginSettings(int pluginPort, const AudioStreamHeader& settings) {
    juce::ScopedLock lock(streamsMutex);
    
    auto it = streams.find(pluginPort);
    if (it != streams.end() && it->second && it->second->buffer) {
        auto& streamInfo = it->second;
        juce::ScopedLock streamLock(streamInfo->mutex);
        
        // Update M1Encode settings
        streamInfo->buffer->header.azimuth = settings.azimuth;
        streamInfo->buffer->header.elevation = settings.elevation;
        streamInfo->buffer->header.diverge = settings.diverge;
        streamInfo->buffer->header.gain = settings.gain;
        streamInfo->buffer->header.inputMode = settings.inputMode;
        streamInfo->buffer->header.outputMode = settings.outputMode;
        streamInfo->buffer->header.isPlaying = settings.isPlaying;
        streamInfo->lastUpdateTime = juce::Time::getCurrentTime().toMilliseconds();
    }
}

juce::String AudioStreamManager::generateSharedMemoryName(int pluginPort) {
    return "M1PannerStream_" + juce::String(pluginPort);
}

bool AudioStreamManager::createSharedMemory(int pluginPort, size_t size) {
    auto it = streams.find(pluginPort);
    if (it == streams.end()) {
        return false;
    }
    
    auto& streamInfo = it->second;
    
    // Create memory mapped file
    juce::File tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                         .getChildFile(streamInfo->sharedMemoryName);
    
    streamInfo->mappedFile = std::make_unique<juce::MemoryMappedFile>(tempFile, 
                                                                     juce::MemoryMappedFile::readWrite, 
                                                                     size);
    
    if (!streamInfo->mappedFile->getData()) {
        streamInfo->mappedFile.reset();
        return false;
    }
    
    // Initialize buffer
    streamInfo->buffer = reinterpret_cast<AudioStreamBuffer*>(streamInfo->mappedFile->getData());
    
    // Initialize header
    streamInfo->buffer->header = AudioStreamHeader{};
    streamInfo->buffer->header.pluginPort = pluginPort;
    
    return true;
}

void AudioStreamManager::cleanupInactiveStreams() {
    juce::ScopedLock lock(streamsMutex);
    
    juce::int64 currentTime = juce::Time::getCurrentTime().toMilliseconds();
    
    auto it = streams.begin();
    while (it != streams.end()) {
        if (!it->second || (currentTime - it->second->lastUpdateTime) > 30000) { // 30 second timeout
            it = streams.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace Mach1 