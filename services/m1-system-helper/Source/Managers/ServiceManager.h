/*
    ProcessManager.h
    -----------------
    Manages the orientation manager process.
    Starts, stops, and restarts the orientation manager as needed.

    Designed to manage background services that are required for the system to function.
*/

#pragma once

#include "../Common/Common.h"
#include <atomic>

namespace Mach1 {

class ServiceManager {
public:
    explicit ServiceManager(int serverPort);
    ~ServiceManager();

    Result startOrientationManager();
    Result killOrientationManager();
    Result restartOrientationManagerIfNeeded();
    Result handleClientRequestToStartOrientationManager();
    bool isOrientationManagerRunning() const;
    void noteOrientationManagerClientPulse();
    juce::int64 getLastOrientationManagerClientPulseTime() const;
    
    void setClientRequestsServer(bool value) { clientRequestsServer.store(value); }
    bool getClientRequestsServer() const { return clientRequestsServer.load(); }
    
private:
    void killProcessByName(const std::string& name);
    juce::Result handleServiceOperation(ServiceOperation operation, int result);
    
    int serverPort;
    juce::ChildProcess orientationManagerProcess;
    juce::int64 timeWhenWeLastStartedAManager = -10000;
    juce::int64 timeWhenWeLastAttemptedToStartAManager = -10000;
    std::atomic<bool> clientRequestsServer { false };
    std::atomic<juce::int64> lastOrientationManagerClientPulseTime { 0 };
    
    // Platform-specific paths
    juce::String getServiceName() const;
    juce::String getServicePath() const;
    juce::String getServiceTarget() const;
    
#if defined(__APPLE__)
    uid_t uid;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ServiceManager)
};

} // namespace Mach1
