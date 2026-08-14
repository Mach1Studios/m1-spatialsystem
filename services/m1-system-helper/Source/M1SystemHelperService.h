#pragma once

#include "Common/Common.h"
#include "Common/ServiceLocator.h"
#include "Core/ConfigManager.h"
#include "Core/ExternalMixerProcessor.h"
#include "Core/MixEngine.h"
#include "Managers/ClientManager.h"
#include "Managers/PluginManager.h"
#include "Managers/ProjectPairingManager.h"
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
    ProjectPairingManager& getProjectPairingManager() { return *projectPairingManager; }
    
    // Panner tracking
    PannerTrackingManager& getPannerTrackingManager() { return *pannerTrackingManager; }
    
    // External mixer
    ExternalMixerProcessor& getExternalMixer() { return *externalMixer; }

    // Live mix render clock (P2)
    MixEngine& getMixEngine() { return *mixEngine; }

    // P4: true when the shared-memory directory accepted a probe write at
    // startup. When false, streaming cannot work (entitlement/permission
    // problem) and the UI should say so instead of showing empty coverage.
    bool isSharedMemoryDirWritable() const { return sharedMemoryDirWritable; }
    juce::String getSharedMemoryDirPath() const { return sharedMemoryDirPath; }

private:
    M1SystemHelperService();
    ~M1SystemHelperService() override;
    
    void timerCallback() override;
    void ensureSessionUICreated();

    // P4: user-facing external renderer toggle persistence (per-user file,
    // the system-wide settings.json is root-owned and read-only for us)
    static juce::File getUserSettingsFile();
    bool loadExternalRendererSetting() const;
    void saveExternalRendererSetting(bool enabled) const;
    void checkSharedMemoryDirWritable();
    
private:
    std::shared_ptr<EventSystem> eventSystem;
    std::unique_ptr<ClientManager> clientManager;
    std::unique_ptr<PluginManager> pluginManager;
    std::unique_ptr<ProjectPairingManager> projectPairingManager;
    std::unique_ptr<ServiceManager> serviceManager;
    std::unique_ptr<ConfigManager> configManager;
    std::unique_ptr<OSCHandler> oscHandler;
    
    // Panner tracking component
    std::unique_ptr<PannerTrackingManager> pannerTrackingManager;
    
    // External mixer
    std::unique_ptr<ExternalMixerProcessor> externalMixer;

    // Live mix render clock (P2)
    std::unique_ptr<MixEngine> mixEngine;
    
    // UI component
    std::unique_ptr<SessionUI> sessionUI;
    bool showSessionUI = true;  // Default to showing UI for debugging
    bool debugFakeBlocks = false;  // Debug mode for fake capture blocks

    bool sharedMemoryDirWritable = true;
    juce::String sharedMemoryDirPath;

    static constexpr int TRACKING_UPDATE_INTERVAL_MS = 100;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(M1SystemHelperService)
};

} // namespace Mach1
