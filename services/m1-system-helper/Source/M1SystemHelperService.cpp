#include "M1SystemHelperService.h"
#include "Common/SharedPathUtils.h"

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

    // Live mix render clock: paces the encode/sum math and publishes the
    // MixBus segment that M1-Monitor reads in mono/stereo-only DAWs.
    mixEngine = std::make_unique<MixEngine>(*pannerTrackingManager);

    oscHandler = std::make_unique<OSCHandler>(clientManager.get(), 
                                            pluginManager.get(), 
                                            serviceManager.get(),
                                            pannerTrackingManager.get(),
                                            externalMixer.get(),
                                            mixEngine.get());
    
    // Start listening on helper port
    if (!oscHandler->startListening(configManager->getHelperPort())) {
        DBG("Failed to start listening on helper port " + 
            juce::String(configManager->getHelperPort()));
    }
    
    DBG("Helper listening to port: " + juce::String(configManager->getHelperPort()));

    // P4: user-facing external renderer toggle. Load the persisted choice and
    // wire persistence for future changes (tray menu / status window).
    oscHandler->onExternalRendererChanged = [this](bool enabled) {
        saveExternalRendererSetting(enabled);
    };
    oscHandler->setExternalRendererEnabled(loadExternalRendererSetting(), /*notifyChange*/ false);

    // Plugins can ask us to reveal the status window over OSC (safe from the
    // OSC thread: revealSessionWindow dispatches to the message thread).
    oscHandler->onShowUIRequested = [this]() {
        revealSessionWindow();
    };

    // P4: entitlement/permission sanity check - if we can't write the shared
    // memory directory, no panner audio can ever reach us.
    checkSharedMemoryDirWritable();

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

    sessionUI = std::make_unique<SessionUI>(*pannerTrackingManager, *clientManager, *oscHandler, debugFakeBlocks, mixEngine.get());
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

    if (mixEngine && oscHandler->isExternalRendererEnabled()) {
        mixEngine->startEngine();
        DBG("[M1SystemHelperService] Started mix engine");
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
        // Clear the pulse first: otherwise the stale timestamp keeps this
        // branch firing on every timer tick, spamming kill commands until a
        // new client pulse arrives.
        serviceManager->clearOrientationManagerClientPulse();
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

    if (mixEngine)
        mixEngine->stopEngine();

    if (pannerTrackingManager)
        pannerTrackingManager->stop();
    
    if (serviceManager)
        serviceManager->killOrientationManager();
    
    DBG("[M1SystemHelperService] Service shutdown complete");
}

M1SystemHelperService::~M1SystemHelperService() {
    shutdown();
}

//==============================================================================
// P4: external renderer toggle persistence + shared-dir sanity check

juce::File M1SystemHelperService::getUserSettingsFile() {
    // Per-user (writable) - the system-wide settings.json under
    // commonApplicationDataDirectory is installed root-owned.
    juce::File base = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    if ((juce::SystemStats::getOperatingSystemType() & juce::SystemStats::MacOSX) != 0)
        base = base.getChildFile("Application Support");
    return base.getChildFile("Mach1").getChildFile("helper-settings.json");
}

bool M1SystemHelperService::loadExternalRendererSetting() const {
    const juce::File file = getUserSettingsFile();
    if (!file.existsAsFile())
        return true; // default: feature enabled

    const auto json = juce::JSON::parse(file);
    if (auto* obj = json.getDynamicObject())
        return static_cast<bool>(obj->getProperty("externalRendererEnabled"));

    return true;
}

void M1SystemHelperService::saveExternalRendererSetting(bool enabled) const {
    const juce::File file = getUserSettingsFile();
    file.getParentDirectory().createDirectory();

    // Preserve any other keys already in the file
    juce::var json = file.existsAsFile() ? juce::JSON::parse(file) : juce::var();
    auto* obj = json.getDynamicObject();
    if (obj == nullptr) {
        json = juce::var(new juce::DynamicObject());
        obj = json.getDynamicObject();
    }
    obj->setProperty("externalRendererEnabled", enabled);

    if (!file.replaceWithText(juce::JSON::toString(json)))
        DBG("[M1SystemHelperService] Failed to persist helper-settings.json");
}

void M1SystemHelperService::checkSharedMemoryDirWritable() {
    const juce::File dir { juce::String(SharedPathUtils::getSharedMemoryDirectory()) };
    sharedMemoryDirPath = dir.getFullPathName();
    dir.createDirectory();

    juce::File probe = dir.getChildFile(".m1-helper-write-probe");
    sharedMemoryDirWritable = probe.replaceWithText("ok");
    probe.deleteFile();

    if (!sharedMemoryDirWritable)
        DBG("[M1SystemHelperService] WARNING: shared memory directory is not writable: "
            + sharedMemoryDirPath + " - streaming/capture cannot work");
}

} // namespace Mach1
