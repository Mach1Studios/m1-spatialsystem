#if defined(__APPLE__)
#include <unistd.h>
#endif
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
    timeWhenWeLastAttemptedToStartAManager = juce::Time::currentTimeMillis();

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
    // Prefer the installed LaunchAgent on modern macOS, with a legacy start fallback.
    const auto plistPath = getServicePath();
    const auto bootstrapCommand = "/bin/launchctl bootstrap gui/" + juce::String(uid) + " " + plistPath.quoted();
    const auto kickstartCommand = "/bin/launchctl kickstart -kp " + getServiceTarget();
    const auto legacyStartCommand = "/bin/launchctl start " + getServiceName();
    const auto command = bootstrapCommand + " >/dev/null 2>&1 || "
                       + kickstartCommand + " >/dev/null 2>&1 || "
                       + legacyStartCommand + " >/dev/null 2>&1";
    int result = system(command.toStdString().c_str());
    if (result != 0) {
        return Result::fail("Failed to start service with launchctl");
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
    const auto bootoutCommand = "/bin/launchctl bootout gui/" + juce::String(uid) + " " + getServicePath().quoted();
    const auto killCommand = "/bin/launchctl kill 9 " + getServiceTarget();
    const auto legacyStopCommand = "/bin/launchctl stop " + getServiceName();
    const auto command = bootoutCommand + " >/dev/null 2>&1 || "
                       + killCommand + " >/dev/null 2>&1 || "
                       + legacyStopCommand + " >/dev/null 2>&1";
    int result = system(command.toStdString().c_str());
    if (result != 0) {
        return Result::fail("Failed to stop service with launchctl");
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
    if (clientRequestsServer.load() && (currentTime - timeWhenWeLastAttemptedToStartAManager) > SERVICE_RESTART_DELAY_MS) {
        DBG("[ServiceManager] Restarting orientation manager due to client request");
        
        // Kill existing service
        auto killResult = killOrientationManager();
        if (!killResult.wasOk()) {
            DBG("[ServiceManager] Warning: " + killResult.getErrorMessage());
            // Continue anyway, as the service might not be running
        }
        // Start new service process
        auto startResult = startOrientationManager();
        if (!startResult.wasOk()) {
            DBG("[ServiceManager] Error: " + startResult.getErrorMessage());
            return startResult; // Return the error
        }

        clientRequestsServer.store(false);
        timeWhenWeLastStartedAManager = juce::Time::currentTimeMillis();
        DBG("[ServiceManager] Orientation manager restarted successfully");
        return Result::ok();
    }
    
    return Result::ok(); // No restart needed
}

Result ServiceManager::handleClientRequestToStartOrientationManager() {
    if (!clientRequestsServer.exchange(false))
        return Result::ok();

    auto currentTime = juce::Time::currentTimeMillis();
    if ((currentTime - timeWhenWeLastAttemptedToStartAManager) <= SERVICE_RESTART_DELAY_MS) {
        return Result::ok();
    }

    return startOrientationManager();
}

bool ServiceManager::isOrientationManagerRunning() const {
    // Check if port is in use
    juce::DatagramSocket socket(false);
    socket.setEnablePortReuse(false);
    
    bool isRunning = !socket.bindToPort(serverPort);
    socket.shutdown();
    
    return isRunning;
}

void ServiceManager::noteOrientationManagerClientPulse() {
    lastOrientationManagerClientPulseTime.store(juce::Time::currentTimeMillis());
}

juce::int64 ServiceManager::getLastOrientationManagerClientPulseTime() const {
    return lastOrientationManagerClientPulseTime.load();
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
