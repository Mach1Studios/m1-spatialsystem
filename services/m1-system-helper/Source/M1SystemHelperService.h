#pragma once

#include "Common/Common.h"
#include "Common/ServiceLocator.h"
#include "Core/ConfigManager.h"
#include "Core/AudioStreaming.h"
#include "Core/ExternalMixerProcessor.h"
#include "Managers/ClientManager.h"
#include "Managers/PluginManager.h"
#include "Managers/ServiceManager.h"
#include "Network/OSCHandler.h"
#include <memory>

namespace Mach1 {

class M1SystemHelperService : public juce::Timer {
public:
    static M1SystemHelperService& getInstance();
    
    void start();
    void shutdown();
    
    // Accessors for managers
    ClientManager& getClientManager() { return *clientManager; }
    PluginManager& getPluginManager() { return *pluginManager; }
    ServiceManager& getServiceManager() { return *serviceManager; }
    
    // New streaming functionality
    AudioStreamManager& getAudioStreamManager() { return *audioStreamManager; }
    ExternalMixerProcessor& getExternalMixer() { return *externalMixer; }
    
    void updateClientSeenTime(juce::int64 time) {
        timeWhenHelperLastSeenAClient = time;
    }
    
private:
    M1SystemHelperService();
    ~M1SystemHelperService() override;
    
    void initialise();
    void timerCallback() override;
    
    std::shared_ptr<EventSystem> eventSystem;
    std::unique_ptr<ClientManager> clientManager;
    std::unique_ptr<PluginManager> pluginManager;
    std::unique_ptr<ServiceManager> serviceManager;
    std::unique_ptr<ConfigManager> configManager;
    std::unique_ptr<OSCHandler> oscHandler;
    
    // New streaming components
    std::unique_ptr<AudioStreamManager> audioStreamManager;
    std::unique_ptr<ExternalMixerProcessor> externalMixer;
    
    juce::int64 timeWhenHelperLastSeenAClient = 0;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(M1SystemHelperService)
};

} // namespace Mach1
