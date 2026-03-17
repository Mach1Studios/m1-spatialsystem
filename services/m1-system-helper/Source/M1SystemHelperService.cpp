#include "M1SystemHelperService.h"

namespace Mach1 {

M1SystemHelperService::M1SystemHelperService() {
    eventSystem = std::make_shared<EventSystem>();
    configManager = std::make_unique<ConfigManager>();
    
    juce::File configFile;
    if ((juce::SystemStats::getOperatingSystemType() & juce::SystemStats::MacOSX) != 0) {
        configFile = juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory)
                            .getChildFile("Application Support")
                            .getChildFile("Mach1")
                            .getChildFile("settings.json");
    } else {
        configFile = juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory)
                            .getChildFile("Mach1")
                            .getChildFile("settings.json");
    }
    
    if (configFile.exists()) {
        auto result = configManager->loadConfig(configFile);
        if (!result.wasOk()) {
            DBG("Failed to load config: " + result.getErrorMessage());
        }
    } else {
        DBG("No config file found, using default ports");
    }
    
    // Initialize managers with configured ports
    clientManager = std::make_unique<ClientManager>(eventSystem);
    pluginManager = std::make_unique<PluginManager>(eventSystem);
    serviceManager = std::make_unique<ServiceManager>(configManager->getServerPort());
    
    // Initialize new panner tracking manager
    pannerTrackingManager = std::make_unique<PannerTrackingManager>(eventSystem);
    
    // Initialize OSC tracker with plugin manager
    pannerTrackingManager->initializeOSCTracker(pluginManager.get());

    // Initialize external mixer
    externalMixer = std::make_unique<ExternalMixerProcessor>();
    externalMixer->initialize(44100.0, 512); // Default sample rate and block size
    externalMixer->setPannerTrackingManager(pannerTrackingManager.get());
    
    oscHandler = std::make_unique<OSCHandler>(clientManager.get(), 
                                            pluginManager.get(), 
                                            serviceManager.get(),
                                            pannerTrackingManager.get(),
                                            externalMixer.get());
    
    // Start listening on helper port
    if (!oscHandler->startListening(configManager->getHelperPort())) {
        DBG("Failed to start listening on helper port " + 
            juce::String(configManager->getHelperPort()));
    }
    
    DBG("Helper listening to port: " + juce::String(configManager->getHelperPort()));
    
    // Register service for dependency injection
    Mach1::ServiceLocator::getInstance().registerService(eventSystem);
}

M1SystemHelperService& M1SystemHelperService::getInstance() {
    static M1SystemHelperService instance;
    return instance;
}

void M1SystemHelperService::ensureSessionUICreated()
{
    if (!showSessionUI || sessionUI || !pannerTrackingManager || !clientManager || !oscHandler)
        return;

    sessionUI = std::make_unique<SessionUI>(*pannerTrackingManager, *clientManager, *oscHandler, debugFakeBlocks);
    sessionUI->setVisible(true);
    DBG("[M1SystemHelperService] Created system tray icon on main thread");

    if (debugFakeBlocks)
        DBG("[M1SystemHelperService] Debug fake blocks enabled");
}

void M1SystemHelperService::initialise() {
    // Start panner tracking manager
    if (pannerTrackingManager) {
        pannerTrackingManager->start();
        DBG("[M1SystemHelperService] Started panner tracking manager");
    }
    
    // Schedule system tray icon creation on the main thread if enabled
    if (showSessionUI && pannerTrackingManager) {
        juce::MessageManager::callAsync([this]() {
            ensureSessionUICreated();
        });
    }
    
    startTimer(TRACKING_UPDATE_INTERVAL_MS); // Keep helper state responsive
}

void M1SystemHelperService::revealSessionWindow()
{
    if (!showSessionUI)
        return;

    juce::MessageManager::callAsync([this]()
    {
        ensureSessionUICreated();

        if (sessionUI)
            sessionUI->showStatusWindow();
    });
}

void M1SystemHelperService::timerCallback() {
    auto currentTime = juce::Time::currentTimeMillis();
    
    // Update panner tracking manager
    if (pannerTrackingManager) {
        pannerTrackingManager->update();
    }
    
    // Check for inactive clients
    const auto lastOrientationPulseTime = serviceManager->getLastOrientationManagerClientPulseTime();
    if (lastOrientationPulseTime > 0 && (currentTime - lastOrientationPulseTime) > CLIENT_TIMEOUT_MS) {
        if (serviceManager->isOrientationManagerRunning()) {
            auto result = serviceManager->killOrientationManager();
            if (!result.wasOk()) {
                DBG("[M1SystemHelperService] Failed to kill orientation manager: " + result.getErrorMessage());
            }
        }
    }
    
    // Handle client server requests
    if (serviceManager->getClientRequestsServer()) {
        auto result = serviceManager->handleClientRequestToStartOrientationManager();
        if (!result.wasOk()) {
            DBG("[M1SystemHelperService] Failed to start orientation manager: " + result.getErrorMessage());
        }
    }
}

void M1SystemHelperService::start() {
    // Legacy method - now just calls initialise() for compatibility
    // The actual service runs via JUCE timers on the main message thread
    initialise();
}

void M1SystemHelperService::shutdown() {
    DBG("[M1SystemHelperService] Service shutdown starting...");
    
    stopTimer();
    
    // Clean up SessionUI on the message thread to avoid GUI threading assertions
    if (sessionUI) {
        if (juce::MessageManager::getInstance()->isThisTheMessageThread()) {
            sessionUI->setVisible(false);
            sessionUI.reset();
        } else {
            SessionUI* raw = sessionUI.release();
            juce::MessageManager::callAsync([raw]() {
                if (raw) { raw->setVisible(false); delete raw; }
            });
            juce::Thread::sleep(100);
        }
    }
    
    if (oscHandler)
        oscHandler->stopTimer();
    
    if (pannerTrackingManager)
        pannerTrackingManager->stop();
    
    if (serviceManager)
        serviceManager->killOrientationManager();
    
    DBG("[M1SystemHelperService] Service shutdown complete");
}

M1SystemHelperService::~M1SystemHelperService() {
    shutdown();
}

} // namespace Mach1
