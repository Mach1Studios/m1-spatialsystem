/*
    ProcessManager.h
    -----------------
    Manages the orientation manager process.
    Starts, stops, and restarts the orientation manager as needed.

    Designed to manage background services that are required for the system to function.
*/

#pragma once

#include "../Common/Common.h"

namespace Mach1 {

class ServiceManager {
public:
    explicit ServiceManager(int serverPort);
    ~ServiceManager();

    Result startOrientationManager();
    Result killOrientationManager();
    Result restartOrientationManagerIfNeeded();
    bool isOrientationManagerRunning() const;
    
    void setClientRequestsServer(bool value) { clientRequestsServer = value; }
    bool getClientRequestsServer() { return clientRequestsServer; }
    
private:
    void killProcessByName(const std::string& name);
    juce::Result handleServiceOperation(ServiceOperation operation, int result);
    
    int serverPort;
    juce::ChildProcess orientationManagerProcess;
    juce::int64 timeWhenWeLastStartedAManager = -10000;
    bool clientRequestsServer = false;
    
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
