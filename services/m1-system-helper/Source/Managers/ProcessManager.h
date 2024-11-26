#pragma once

#include "../Common/Common.h"

namespace Mach1 {

class ProcessManager {
public:
    explicit ProcessManager(int serverPort);
    ~ProcessManager();

    void startOrientationManager();
    void killOrientationManager();
    void restartOrientationManagerIfNeeded();
    bool isOrientationManagerRunning() const;
    
    void setClientRequestsServer(bool value) { clientRequestsServer = value; }

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProcessManager)
};

} // namespace Mach1
