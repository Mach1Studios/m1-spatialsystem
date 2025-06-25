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
    oscHandler = std::make_unique<OSCHandler>(clientManager.get(), 
                                            pluginManager.get(), 
                                            serviceManager.get());
    
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
    startTimer(1000); // Check status every second
}

void M1SystemHelperService::timerCallback() {
    auto currentTime = juce::Time::currentTimeMillis();
    
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
    initialise();
    
    while (!juce::MessageManager::getInstance()->hasStopMessageBeenSent()) {
        juce::Thread::sleep(100);
    }
    
    shutdown();
}

void M1SystemHelperService::shutdown() {
    stopTimer();
    serviceManager->killOrientationManager();
}

M1SystemHelperService::~M1SystemHelperService() {
    shutdown();
}

} // namespace Mach1
