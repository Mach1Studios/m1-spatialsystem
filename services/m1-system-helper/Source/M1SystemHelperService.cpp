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
    
    oscHandler = std::make_unique<OSCHandler>(clientManager.get(), 
                                            pluginManager.get(), 
                                            serviceManager.get(),
                                            pannerTrackingManager.get());
    
    // Start listening on helper port
    if (!oscHandler->startListening(configManager->getHelperPort())) {
        DBG("Failed to start listening on helper port " + 
            juce::String(configManager->getHelperPort()));
    }
    
    DBG("Helper listening to port: " + juce::String(configManager->getHelperPort()));
    
    // Initialize streaming components
    audioStreamManager = std::make_unique<AudioStreamManager>();
    externalMixer = std::make_unique<ExternalMixerProcessor>();
    externalMixer->initialize(44100.0, 512); // Default sample rate and block size
    
    // Register service for dependency injection
    Mach1::ServiceLocator::getInstance().registerService(eventSystem);
    
//    // Add this event subscription
//    eventSystem->subscribe("ClientSeen", [this](const juce::var& time) {
//        timeWhenHelperLastSeenAClient = time;
//    });
}

M1SystemHelperService& M1SystemHelperService::getInstance() {
    static M1SystemHelperService instance;
    return instance;
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
            if (pannerTrackingManager && showSessionUI) {
                sessionUI = std::make_unique<SessionUI>(*pannerTrackingManager);
                sessionUI->setVisible(true);  // Shows system tray icon
                DBG("[M1SystemHelperService] Created system tray icon on main thread");
            }
        });
    }
    
    startTimer(1000); // Check status every second
}

void M1SystemHelperService::timerCallback() {
    auto currentTime = juce::Time::currentTimeMillis();
    
    // Update panner tracking manager
    if (pannerTrackingManager) {
        pannerTrackingManager->update();
    }
    
    // Check for inactive clients
    if ((currentTime - timeWhenHelperLastSeenAClient) > CLIENT_TIMEOUT_MS) {
        if (serviceManager->isOrientationManagerRunning()) {
            auto result = serviceManager->killOrientationManager();
            if (!result.wasOk()) {
                DBG("[M1SystemHelperService] Failed to kill orientation manager: " + result.getErrorMessage());
            }
            timeWhenHelperLastSeenAClient = currentTime;
        }
    }
    
    // Handle client server requests
    if (serviceManager->getClientRequestsServer()) {
        // Try to start the orientation manager
        auto result = serviceManager->startOrientationManager();
        if (!result.wasOk()) {
            DBG("[M1SystemHelperService] Failed to start orientation manager: " + result.getErrorMessage());
            
            // If starting fails, try restarting
            result = serviceManager->restartOrientationManagerIfNeeded();
            if (!result.wasOk()) {
                DBG("[M1SystemHelperService] Failed to restart orientation manager: " + result.getErrorMessage());
            }
        }
        serviceManager->setClientRequestsServer(false);
    }
}

void M1SystemHelperService::start() {
    // Legacy method - now just calls initialise() for compatibility
    // The actual service runs via JUCE timers on the main message thread
    initialise();
}

void M1SystemHelperService::shutdown() {
    DBG("[M1SystemHelperService] Service shutdown starting...");
    
    // CRITICAL: Clean up SessionUI FIRST if not already done
    // This prevents MenuBarModel singleton conflicts during JUCE shutdown
    // MUST happen on the message thread to avoid GUI threading assertions
            if (sessionUI) {
        DBG("[M1SystemHelperService] Cleaning up SessionUI during shutdown");
        
        if (juce::MessageManager::getInstance()->isThisTheMessageThread()) {
            // We're on the message thread - clean up immediately
                sessionUI->setVisible(false);
                sessionUI.reset();
            DBG("[M1SystemHelperService] SessionUI cleaned up synchronously");
        } else {
            // We're on a background thread - must use message thread for GUI cleanup
            DBG("[M1SystemHelperService] Transferring SessionUI cleanup to message thread");
            
            // Release ownership and transfer to message thread
            // This prevents the unique_ptr destructor from running on the wrong thread
            SessionUI* sessionUIPtr = sessionUI.release();
            
            // Schedule cleanup on message thread without capture issues
            juce::MessageManager::callAsync([sessionUIPtr]() {
                if (sessionUIPtr) {
                    sessionUIPtr->setVisible(false);
                    delete sessionUIPtr;  // Clean deletion on message thread
                    DBG("[M1SystemHelperService] SessionUI cleaned up on message thread");
            }
        });
            
            // Brief pause to ensure the async cleanup is queued
            juce::Thread::sleep(100);
        }
    }
    
    // Stop all timers immediately
    stopTimer();
    
    // Stop OSC handler timer to prevent periodic events
    if (oscHandler) {
        oscHandler->stopTimer();
        DBG("[M1SystemHelperService] Stopped OSC handler timer");
    }
    
    // Stop panner tracking manager (non-GUI operation, safe to call from any thread)
    if (pannerTrackingManager) {
        pannerTrackingManager->stop();
        DBG("[M1SystemHelperService] Stopped panner tracking manager");
    }
    
    // Kill orientation manager (non-GUI operation, safe to call from any thread)
    if (serviceManager) {
    serviceManager->killOrientationManager();
        DBG("[M1SystemHelperService] Killed orientation manager");
    }
    
    DBG("[M1SystemHelperService] Service shutdown complete");
}

M1SystemHelperService::~M1SystemHelperService() {
    shutdown();
}

} // namespace Mach1
