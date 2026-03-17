#pragma once

#include "Common/Common.h"
#include "Common/ServiceLocator.h"
#include "Core/ConfigManager.h"
#include "Core/ExternalMixerProcessor.h"
#include "Managers/ClientManager.h"
#include "Managers/PluginManager.h"
#include "Managers/ServiceManager.h"
#include "Managers/PannerTrackingManager.h"
#include "Network/OSCHandler.h"
#include "UI/SessionUI.h"
#include <memory>

namespace Mach1 {

class M1SystemHelperService : public juce::Timer {
public:
    static M1SystemHelperService& getInstance();
    
    void start();
    void initialise();
    void shutdown();
    void revealSessionWindow();
    
    // Debug mode control
    void setDebugFakeBlocks(bool enabled) { debugFakeBlocks = enabled; }
    bool isDebugFakeBlocks() const { return debugFakeBlocks; }
    
    // Accessors for managers
    ClientManager& getClientManager() { return *clientManager; }
    PluginManager& getPluginManager() { return *pluginManager; }
    ServiceManager& getServiceManager() { return *serviceManager; }
    OSCHandler& getOSCHandler() { return *oscHandler; }
    
    // Panner tracking
    PannerTrackingManager& getPannerTrackingManager() { return *pannerTrackingManager; }
    
    // External mixer
    ExternalMixerProcessor& getExternalMixer() { return *externalMixer; }
    
private:
    M1SystemHelperService();
    ~M1SystemHelperService() override;
    
    void timerCallback() override;
    void ensureSessionUICreated();
    
private:
    std::shared_ptr<EventSystem> eventSystem;
    std::unique_ptr<ClientManager> clientManager;
    std::unique_ptr<PluginManager> pluginManager;
    std::unique_ptr<ServiceManager> serviceManager;
    std::unique_ptr<ConfigManager> configManager;
    std::unique_ptr<OSCHandler> oscHandler;
    
    // Panner tracking component
    std::unique_ptr<PannerTrackingManager> pannerTrackingManager;
    
    // External mixer
    std::unique_ptr<ExternalMixerProcessor> externalMixer;
    
    // UI component
    std::unique_ptr<SessionUI> sessionUI;
    bool showSessionUI = true;  // Default to showing UI for debugging
    bool debugFakeBlocks = false;  // Debug mode for fake capture blocks

    static constexpr int TRACKING_UPDATE_INTERVAL_MS = 100;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(M1SystemHelperService)
};

} // namespace Mach1
