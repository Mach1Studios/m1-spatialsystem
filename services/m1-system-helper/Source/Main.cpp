#include <JuceHeader.h>
#include "M1SystemHelperService.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>      // for fcntl
#include <sys/select.h> // for select
#include <thread>
#include <future>
#include <chrono>
#include <atomic>

// Socket activation handler for on-demand service startup
class SocketActivationHandler {
private:
    static std::atomic<bool> s_shouldStop;
    static std::thread s_socketThread;
    static int s_sockfd;

public:
    static void setupSocketActivation() {
        s_shouldStop = false;
        
        s_socketThread = std::thread([]() {
            s_sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
            if (s_sockfd < 0) return;
            
            struct sockaddr_un addr;
            memset(&addr, 0, sizeof(addr));
            addr.sun_family = AF_UNIX;
            strcpy(addr.sun_path, "/tmp/com.mach1.spatial.helper.socket");
            
            // Remove existing socket file
            unlink(addr.sun_path);
            
            if (bind(s_sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                close(s_sockfd);
                return;
            }
            
            if (listen(s_sockfd, 5) < 0) {
                close(s_sockfd);
                return;
            }
            
            // Set socket to non-blocking for clean shutdown
            fcntl(s_sockfd, F_SETFL, O_NONBLOCK);
            
            DBG("[M1SystemHelper] Socket activation listener started");
            
            while (!s_shouldStop) {
                fd_set readfds;
                FD_ZERO(&readfds);
                FD_SET(s_sockfd, &readfds);
                
                struct timeval timeout;
                timeout.tv_sec = 1;  // Check for shutdown every second
                timeout.tv_usec = 0;
                
                int result = select(s_sockfd + 1, &readfds, nullptr, nullptr, &timeout);
                
                if (result > 0 && FD_ISSET(s_sockfd, &readfds)) {
                    int clientfd = accept(s_sockfd, nullptr, nullptr);
                    if (clientfd >= 0) {
                        DBG("[M1SystemHelper] Client connected via socket activation");
                        
                        // Send acknowledgment
                        const char* response = "PONG\n";
                        send(clientfd, response, strlen(response), 0);
                        close(clientfd);
                    }
                }
                // Continue loop to check s_shouldStop
            }
            
            close(s_sockfd);
            unlink(addr.sun_path);
            DBG("[M1SystemHelper] Socket activation listener stopped");
        });
    }
    
    static void stopSocketActivation() {
        s_shouldStop = true;
        if (s_socketThread.joinable()) {
            s_socketThread.join();
        }
    }
};

// Static member definitions
std::atomic<bool> SocketActivationHandler::s_shouldStop{false};
std::thread SocketActivationHandler::s_socketThread;
int SocketActivationHandler::s_sockfd = -1;

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
        // Setup socket activation for on-demand startup
        SocketActivationHandler::setupSocketActivation();
        
        // Initialize the service directly on the main thread (no separate thread)
        Mach1::M1SystemHelperService::getInstance().initialise();
        
        DBG("[M1SystemHelper] Service initialized on main thread");
        
        // Run the JUCE message loop - this is the ONLY message loop
        juce::MessageManager::getInstance()->runDispatchLoop();
    }

    void shutdown() override {
        DBG("[M1SystemHelper] Application shutdown starting...");
        
        // Stop the socket activation thread
        DBG("[M1SystemHelper] Stopping socket activation thread...");
        SocketActivationHandler::stopSocketActivation();
        
        DBG("[M1SystemHelper] Application shutdown complete");
    }
    
    void systemRequestedQuit() override { 
        DBG("[M1SystemHelper] System requested quit");
        // Shutdown the service first to clean up properly
        Mach1::M1SystemHelperService::getInstance().shutdown();
        // Let JUCE handle its own shutdown - it will call our shutdown() automatically
        quit();
        // Add a fallback with proper JUCE cleanup using a separate thread
        std::thread([]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            DBG("[M1SystemHelper] Fallback exit - cleaning up JUCE resources");
            
            // Force cleanup of any remaining JUCE resources before exit
            try {
                // Stop any remaining timers
                auto& service = Mach1::M1SystemHelperService::getInstance();
                service.shutdown();
                service.stopTimer();
                service = nullptr;
                
                // Ensure MessageManager cleanup
                if (auto* mm = juce::MessageManager::getInstanceWithoutCreating()) {
                    mm->stopDispatchLoop();
                }
                
                DBG("[M1SystemHelper] JUCE cleanup complete, forcing exit");
            } catch (...) {
                // Ignore any exceptions during cleanup
            }
            
            std::exit(0);
        }).detach();
    }
    
    void anotherInstanceStarted(const juce::String&) override { 
        DBG("[M1SystemHelper] Another instance started, quitting");
        quit(); 
    }

private:
    // No service thread needed - everything runs on main JUCE message thread
};

START_JUCE_APPLICATION(M1SystemHelperApplication)

#endif