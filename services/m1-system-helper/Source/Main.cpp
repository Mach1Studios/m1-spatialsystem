#include <JuceHeader.h>
#include "M1SystemHelperService.h"

#if !defined(GUI_APP) && defined(JUCE_WINDOWS)
// Windows service implementation
#include <Windows.h>

SERVICE_STATUS g_ServiceStatus = { 0 };
SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
HANDLE g_ServiceStopEvent = INVALID_HANDLE_VALUE;

VOID WINAPI ServiceControlHandler(DWORD dwControl) {
    switch (dwControl) {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
        g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
        juce::MessageManager::getInstance()->stopDispatchLoop();
        SetEvent(g_ServiceStopEvent);
        return;
    default:
        break;
    }
}

VOID WINAPI ServiceMain(DWORD argc, LPTSTR* argv) {
    g_StatusHandle = RegisterServiceCtrlHandler("M1-System-Helper", ServiceControlHandler);
    if (!g_StatusHandle) return;

    g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

    g_ServiceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (g_ServiceStopEvent == NULL) return;

    std::thread([&] { juce::MessageManager::getInstance()->runDispatchLoop(); }).detach();
    std::thread([]() { Mach1::M1SystemHelperService::getInstance().start(); }).join();

    CloseHandle(g_ServiceStopEvent);
    g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
}

int main(int argc, char* argv[]) {
    SERVICE_TABLE_ENTRY ServiceTable[] = {
        { "M1-System-Helper", (LPSERVICE_MAIN_FUNCTION)ServiceMain },
        { NULL, NULL }
    };
    StartServiceCtrlDispatcher(ServiceTable);
    return 0;
}

#else

class M1SystemHelperApplication : public juce::JUCEApplication {
public:
    M1SystemHelperApplication() = default;
    
    const juce::String getApplicationName() override { return ProjectInfo::projectName; }
    const juce::String getApplicationVersion() override { return ProjectInfo::versionString; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise(const juce::String& commandLine) override {
        std::thread([]() { Mach1::M1SystemHelperService::getInstance().start(); }).detach();
        juce::MessageManager::getInstance()->runDispatchLoop();
    }

    void shutdown() override {}
    void systemRequestedQuit() override { quit(); }
    void anotherInstanceStarted(const juce::String&) override { quit(); }
};

START_JUCE_APPLICATION(M1SystemHelperApplication)

#endif