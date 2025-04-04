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

void ServiceManager::startOrientationManager() {
    // Check if port is available
    juce::DatagramSocket socket(false);
    socket.setEnablePortReuse(false);
    
    if (!socket.bindToPort(serverPort)) {
        DBG("Port " + juce::String(serverPort) + " is in use, assuming service is running");
        return;
    }
    socket.shutdown();

    // Get support directory
    auto m1SupportDirectory = juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory);

#if JUCE_MAC
    if (juce::SystemStats::getOperatingSystemType() <= juce::SystemStats::MacOSX_10_9) {
        // Old MacOS (10.7-10.9)
        system(("launchctl start " + getServiceName()).toStdString().c_str());
    } else if (juce::SystemStats::getOperatingSystemType() <= juce::SystemStats::MacOS_14) {
        // 10.10 - 13 MacOS
        system(("/bin/launchctl kickstart -p " + getServiceTarget()).toStdString().c_str());
    } else {
        // Modern MacOS
        // TODO: Add this, kickstart is removed 14.4+
    }
#elif JUCE_WINDOWS
    int result = system("sc start M1-OrientationManager");
    handleServiceOperation(ServiceOperation::Start, result);
#else
    // Linux
    auto orientationManagerExe = m1SupportDirectory.getChildFile("Mach1")
                                                  .getChildFile("m1-orientationmanager");
    juce::StringArray arguments;
    arguments.add(orientationManagerExe.getFullPathName().quoted());
    
    if (!orientationManagerProcess.start(arguments)) {
        DBG("Failed to start m1-orientationmanager");
        juce::JUCEApplicationBase::quit();
    }
#endif

    timeWhenWeLastStartedAManager = juce::Time::currentTimeMillis();
}

void ServiceManager::killOrientationManager() {
#if JUCE_MAC
    if (juce::SystemStats::getOperatingSystemType() <= juce::SystemStats::MacOSX_10_9) {
        system(("launchctl stop " + getServiceName()).toStdString().c_str());
    } else {
        system(("launchctl kill 9 " + getServiceTarget()).toStdString().c_str());
    }
#elif JUCE_WINDOWS
    int result = system("sc stop M1-OrientationManager");
    handleServiceOperation(ServiceOperation::Stop, result);
#else
    system("pkill m1-orientationmanager");
#endif
}

void ServiceManager::restartOrientationManagerIfNeeded() {
    auto currentTime = juce::Time::currentTimeMillis();
    if (clientRequestsServer && (currentTime - timeWhenWeLastStartedAManager) > SERVICE_RESTART_DELAY_MS) {
        DBG("[ServiceManager] Restarting orientation manager due to client request");
        killOrientationManager(); // kill existing
        juce::Thread::sleep(2000); // wait to terminate
        startOrientationManager(); // start new service process
        juce::Thread::sleep(6000);
        clientRequestsServer = false;
        currentTime = juce::Time::currentTimeMillis();
        timeWhenWeLastStartedAManager = currentTime;
        DBG("[ServiceManager] Orientation manager restarted successfully");
    }
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
