#include "M1SystemHelperService.h"

namespace Mach1 {

M1SystemHelperService::M1SystemHelperService() {
    eventSystem = std::make_shared<EventSystem>();
    configManager = std::make_unique<ConfigManager>();
    
    // Try to load config file
    auto configFile = juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory)
                        .getChildFile("Mach1")
                        .getChildFile("config.json");
    
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
    processManager = std::make_unique<ProcessManager>(configManager->getServerPort());
    oscHandler = std::make_unique<OSCHandler>(clientManager.get(), 
                                            pluginManager.get(), 
                                            processManager.get());
    
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

void M1SystemHelperService::initialise() {
    startTimer(1000); // Check status every second
    processManager->startOrientationManager();
}

void M1SystemHelperService::timerCallback() {
    auto currentTime = juce::Time::currentTimeMillis();
    
    // Check for inactive clients
    if ((currentTime - timeWhenHelperLastSeenAClient) > 20000) {
        processManager->killOrientationManager();
        timeWhenHelperLastSeenAClient = currentTime;
    }
    
    // Handle client server requests
    if (clientRequestsServer) {
        processManager->restartOrientationManagerIfNeeded();
        clientRequestsServer = false;
    }
    
    // Cleanup and maintenance
    clientManager->cleanupInactiveClients();
    pluginManager->sendPingToAll();
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
    processManager->killOrientationManager();
}

M1SystemHelperService::~M1SystemHelperService() {
    shutdown();
}

} // namespace Mach1
