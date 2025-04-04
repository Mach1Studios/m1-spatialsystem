#include "ServiceManager.h"

namespace Mach1 {

ServiceManager::ServiceManager(int serverPort) : serverPort(serverPort) {
#if defined(__APPLE__)
    uid = getuid();
#endif
}

ServiceManager::~ServiceManager() {
    killOrientationManager();
}

Result ServiceManager::startOrientationManager() {
    // Check if port is available
    juce::DatagramSocket socket(false);
    socket.setEnablePortReuse(false);
    
    if (!socket.bindToPort(serverPort)) {
        DBG("[ServiceManager] Port " + juce::String(serverPort) + " is in use, assuming service is running");
        return Result::ok(); // Service is already running
    }
    socket.shutdown();

    DBG("[ServiceManager] Starting orientation manager service");

    // Get support directory
    auto m1SupportDirectory = juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory);

#if JUCE_MAC
    if (juce::SystemStats::getOperatingSystemType() <= juce::SystemStats::MacOSX_10_9) {
        // Old MacOS (10.7-10.9)
        int result = system(("launchctl start " + getServiceName()).toStdString().c_str());
        if (result != 0) {
            return Result::fail("Failed to start service with launchctl start");
        }
    } else if (juce::SystemStats::getOperatingSystemType() <= juce::SystemStats::MacOS_14) {
        // 10.10 - 13 MacOS
        int result = system(("/bin/launchctl kickstart -p " + getServiceTarget()).toStdString().c_str());
        if (result != 0) {
            return Result::fail("Failed to start service with launchctl kickstart");
        }
    } else {
        // Modern MacOS
        // TODO: Add this, kickstart is removed 14.4+
        return Result::fail("Service start not implemented for this macOS version");
    }
#elif JUCE_WINDOWS
    int result = system("sc start M1-OrientationManager");
    return handleServiceOperation(ServiceOperation::Start, result);
#else
    // Linux
    auto orientationManagerExe = m1SupportDirectory.getChildFile("Mach1")
                                                  .getChildFile("m1-orientationmanager");
    juce::StringArray arguments;
    arguments.add(orientationManagerExe.getFullPathName().quoted());
    
    if (!orientationManagerProcess.start(arguments)) {
        return Result::fail("Failed to start m1-orientationmanager process");
    }
#endif

    timeWhenWeLastStartedAManager = juce::Time::currentTimeMillis();
    return Result::ok();
}

Result ServiceManager::killOrientationManager() {
    DBG("[ServiceManager] Stopping orientation manager service");

#if JUCE_MAC
    if (juce::SystemStats::getOperatingSystemType() <= juce::SystemStats::MacOSX_10_9) {
        int result = system(("launchctl stop " + getServiceName()).toStdString().c_str());
        if (result != 0) {
            return Result::fail("Failed to stop service with launchctl stop");
        }
    } else {
        int result = system(("launchctl kill 9 " + getServiceTarget()).toStdString().c_str());
        if (result != 0) {
            return Result::fail("Failed to kill service with launchctl kill");
        }
    }
#elif JUCE_WINDOWS
    int result = system("sc stop M1-OrientationManager");
    return handleServiceOperation(ServiceOperation::Stop, result);
#else
    int result = system("pkill m1-orientationmanager");
    if (result != 0 && result != 1) { // pkill returns 1 if no processes were killed
        return Result::fail("Failed to kill m1-orientationmanager process");
    }
#endif

    return Result::ok();
}

Result ServiceManager::restartOrientationManagerIfNeeded() {
    auto currentTime = juce::Time::currentTimeMillis();
    if (clientRequestsServer && (currentTime - timeWhenWeLastStartedAManager) > SERVICE_RESTART_DELAY_MS) {
        DBG("[ServiceManager] Restarting orientation manager due to client request");
        
        // Kill existing service
        auto killResult = killOrientationManager();
        if (!killResult.wasOk()) {
            DBG("[ServiceManager] Warning: " + killResult.getErrorMessage());
            // Continue anyway, as the service might not be running
        }
        
        juce::Thread::sleep(2000); // wait to terminate
        
        // Start new service process
        auto startResult = startOrientationManager();
        if (!startResult.wasOk()) {
            DBG("[ServiceManager] Error: " + startResult.getErrorMessage());
            return startResult; // Return the error
        }
        
        juce::Thread::sleep(6000);
        clientRequestsServer = false;
        timeWhenWeLastStartedAManager = juce::Time::currentTimeMillis();
        DBG("[ServiceManager] Orientation manager restarted successfully");
        return Result::ok();
    }
    
    return Result::ok(); // No restart needed
}

bool ServiceManager::isOrientationManagerRunning() const {
    // Check if port is in use
    juce::DatagramSocket socket(false);
    socket.setEnablePortReuse(false);
    
    bool isRunning = !socket.bindToPort(serverPort);
    socket.shutdown();
    
    return isRunning;
}

// Platform-specific implementations...
juce::String ServiceManager::getServiceName() const {
    return "com.mach1.spatial.orientationmanager";
}

juce::String ServiceManager::getServicePath() const {
#if JUCE_MAC
    return "/Library/LaunchAgents/com.mach1.spatial.orientationmanager.plist";
#else
    return {};
#endif
}

juce::String ServiceManager::getServiceTarget() const {
#if JUCE_MAC
    return "gui/" + juce::String(uid) + "/" + getServiceName();
#else
    return "gui/" + getServiceName();
#endif
}

juce::Result ServiceManager::handleServiceOperation(ServiceOperation operation, int result) {
    switch (result) {
        case 0:
            return juce::Result::ok();
        case 1060:
            return juce::Result::fail("Service not found");
        case 1053:
            return juce::Result::fail("Failed to start/stop service");
        case 5:
            return juce::Result::fail("Need to run as admin");
        default:
            return juce::Result::fail("Unknown error");
    }
}

} // namespace Mach1
