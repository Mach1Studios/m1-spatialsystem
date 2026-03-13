#include <JuceHeader.h>
#include "M1SystemHelperService.h"
#include <thread>
#include <future>
#include <chrono>
#include <atomic>

#ifndef JUCE_WINDOWS
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>      // for fcntl
#include <sys/select.h> // for select
#endif

// Forward declaration for fake panner simulator
namespace Mach1 {
    class FakePannerSimulator;
}

// Socket activation handler for on-demand service startup
class SocketActivationHandler {
private:
    static std::atomic<bool> s_shouldStop;
    static std::thread s_socketThread;
#ifndef JUCE_WINDOWS
    static int s_sockfd;
#endif

public:
    static void setupSocketActivation() {
#ifndef JUCE_WINDOWS
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
#endif
    }
    
    static void stopSocketActivation() {
#ifndef JUCE_WINDOWS
        s_shouldStop = true;
        if (s_socketThread.joinable()) {
            s_socketThread.join();
        }
#endif
    }
};

// Static member definitions
std::atomic<bool> SocketActivationHandler::s_shouldStop{false};
std::thread SocketActivationHandler::s_socketThread;
#ifndef JUCE_WINDOWS
int SocketActivationHandler::s_sockfd = -1;
#endif

//==============================================================================
/**
 * Fake panner simulator for testing the UI without a DAW
 * Enabled via --debug-fake-panners N command line flag
 */
namespace Mach1 {

class FakePannerSimulator : public juce::Timer
{
public:
    FakePannerSimulator(PannerTrackingManager& manager, int numPanners)
        : pannerManager(manager), pannerCount(numPanners)
    {
        // Initialize fake panners
        for (int i = 0; i < pannerCount; ++i)
        {
            FakePanner fake;
            fake.name = "Fake Panner " + std::to_string(i + 1);
            fake.azimuth = static_cast<float>(i * 360 / pannerCount);
            fake.elevation = static_cast<float>((i % 3 - 1) * 30);
            fake.diverge = 50.0f;
            fake.gain = 0.0f;
            fake.channels = (i % 2 == 0) ? 1 : 2;
            fake.phase = static_cast<float>(i) * 0.5f;
            fakePanners.push_back(fake);
        }
        
        DBG("[FakePannerSimulator] Created " + juce::String(pannerCount) + " fake panners");
        
        // Start animation timer (60fps for smooth animation)
        startTimer(16);
    }
    
    ~FakePannerSimulator() override
    {
        stopTimer();
    }
    
    void timerCallback() override
    {
        // Animate azimuth for each fake panner
        float time = static_cast<float>(juce::Time::getMillisecondCounter()) / 1000.0f;
        
        for (size_t i = 0; i < fakePanners.size(); ++i)
        {
            auto& fake = fakePanners[i];
            
            // Oscillate azimuth between -180 and 180
            float baseAzimuth = static_cast<float>(i * 360 / pannerCount);
            float oscillation = 45.0f * std::sin(time * 0.5f + fake.phase);
            fake.azimuth = baseAzimuth + oscillation;
            
            // Wrap to -180 to 180 range
            while (fake.azimuth > 180.0f) fake.azimuth -= 360.0f;
            while (fake.azimuth < -180.0f) fake.azimuth += 360.0f;
            
            // Oscillate elevation slightly
            fake.elevation = static_cast<float>((static_cast<int>(i) % 3 - 1) * 30) + 
                            10.0f * std::sin(time * 0.3f + fake.phase * 1.5f);
        }
        
        // Inject fake panners into the tracking system
        injectFakePanners();
    }
    
private:
    struct FakePanner
    {
        std::string name;
        float azimuth = 0.0f;
        float elevation = 0.0f;
        float diverge = 50.0f;
        float gain = 0.0f;
        int channels = 1;
        float phase = 0.0f;
    };
    
    void injectFakePanners()
    {
        std::vector<PannerInfo> fakeInfos;
        
        for (size_t i = 0; i < fakePanners.size(); ++i)
        {
            const auto& fake = fakePanners[i];
            
            PannerInfo info;
            info.name = fake.name;
            info.port = 10000 + static_cast<int>(i);
            info.processId = 99990 + static_cast<uint32_t>(i);
            info.azimuth = fake.azimuth;
            info.elevation = fake.elevation;
            info.diverge = fake.diverge;
            info.gain = fake.gain;
            info.channels = static_cast<uint32_t>(fake.channels);
            info.isActive = true;
            info.isMemoryShareBased = true;
            info.lastUpdateTime = juce::Time::currentTimeMillis();
            info.isPlaying = (i % 3 != 0);
            info.sampleRate = 48000;
            info.samplesPerBlock = 512;
            
            fakeInfos.push_back(info);
        }
        
        pannerManager.injectFakePanners(fakeInfos);
    }
    
    PannerTrackingManager& pannerManager;
    int pannerCount;
    std::vector<FakePanner> fakePanners;
};

} // namespace Mach1

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

class M1SystemHelperApplication : public juce::JUCEApplication, private juce::Timer {
public:
    M1SystemHelperApplication() = default;
    
    const juce::String getApplicationName() override { return ProjectInfo::projectName; }
    const juce::String getApplicationVersion() override { return ProjectInfo::versionString; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise(const juce::String& commandLine) override {
        DBG("[M1SystemHelper] Initializing with command line: " + commandLine);
        
        parseFakePannersFlag(commandLine);
        parseDebugFakeBlocksFlag(commandLine);
        parseKeepAliveFlag(commandLine);
        
        SocketActivationHandler::setupSocketActivation();
        
        if (debugFakeBlocks)
            Mach1::M1SystemHelperService::getInstance().setDebugFakeBlocks(true);
        
        Mach1::M1SystemHelperService::getInstance().initialise();
        
        if (debugFakePannerCount > 0)
        {
            DBG("[M1SystemHelper] Creating " + juce::String(debugFakePannerCount) + " fake panners for testing");
            fakePannerSimulator = std::make_unique<Mach1::FakePannerSimulator>(
                Mach1::M1SystemHelperService::getInstance().getPannerTrackingManager(),
                debugFakePannerCount
            );
        }
        
        if (!keepAlive)
            startTimer(AUTO_QUIT_CHECK_INTERVAL_MS);
        
        DBG("[M1SystemHelper] Service initialized (keepAlive=" + juce::String(keepAlive ? "true" : "false") + ")");
    }

    void shutdown() override {
        DBG("[M1SystemHelper] Application shutdown starting...");
        stopTimer();
        fakePannerSimulator.reset();
        Mach1::M1SystemHelperService::getInstance().shutdown();
        SocketActivationHandler::stopSocketActivation();
        DBG("[M1SystemHelper] Application shutdown complete");
    }
    
    void systemRequestedQuit() override { 
        DBG("[M1SystemHelper] System requested quit");
        quit();
    }
    
    void timerCallback() override {
        // Auto-quit logic: if no panners connected for AUTO_QUIT_TIMEOUT_MS, shut down
        auto& service = Mach1::M1SystemHelperService::getInstance();
        bool hasPanners = service.getPannerTrackingManager().hasPanners();
        
        if (hasPanners) {
            lastPannerSeenTime = juce::Time::currentTimeMillis();
        } else if (lastPannerSeenTime > 0) {
            auto elapsed = juce::Time::currentTimeMillis() - lastPannerSeenTime;
            if (elapsed > AUTO_QUIT_TIMEOUT_MS) {
                DBG("[M1SystemHelper] No panners connected for " + juce::String(AUTO_QUIT_TIMEOUT_MS / 1000) + "s, auto-quitting");
                systemRequestedQuit();
                return;
            }
        } else {
            // First call, initialize the timer
            lastPannerSeenTime = juce::Time::currentTimeMillis();
        }
    }
    
    void anotherInstanceStarted(const juce::String&) override { 
        DBG("[M1SystemHelper] Another instance started, quitting");
        quit(); 
    }

private:
    void parseFakePannersFlag(const juce::String& commandLine)
    {
        juce::StringArray args;
        args.addTokens(commandLine, " ", "\"");
        for (int i = 0; i < args.size(); ++i)
        {
            if (args[i] == "--debug-fake-panners" && i + 1 < args.size())
            {
                debugFakePannerCount = args[i + 1].getIntValue();
                break;
            }
        }
    }
    
    void parseDebugFakeBlocksFlag(const juce::String& commandLine)
    {
        juce::StringArray args;
        args.addTokens(commandLine, " ", "\"");
        for (int i = 0; i < args.size(); ++i)
        {
            if (args[i] == "--debug-fake-blocks")
            {
                debugFakeBlocks = true;
                break;
            }
        }
    }
    
    void parseKeepAliveFlag(const juce::String& commandLine)
    {
        juce::StringArray args;
        args.addTokens(commandLine, " ", "\"");
        for (int i = 0; i < args.size(); ++i)
        {
            if (args[i] == "--keep-alive")
            {
                keepAlive = true;
                DBG("[M1SystemHelper] Keep-alive mode enabled (will not auto-quit)");
                break;
            }
        }
        
        // Also check settings file for persistent preference
        if (!keepAlive)
        {
            juce::File settingsFile;
        #if JUCE_MAC
            settingsFile = juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory)
                                .getChildFile("Application Support")
                                .getChildFile("Mach1")
                                .getChildFile("settings.json");
        #else
            settingsFile = juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory)
                                .getChildFile("Mach1")
                                .getChildFile("settings.json");
        #endif
            if (settingsFile.existsAsFile())
            {
                auto json = juce::JSON::parse(settingsFile.loadFileAsString());
                if (auto* obj = json.getDynamicObject())
                {
                    if (obj->hasProperty("keepHelperAlive"))
                        keepAlive = static_cast<bool>(obj->getProperty("keepHelperAlive"));
                }
            }
        }
    }
    
    static constexpr int AUTO_QUIT_CHECK_INTERVAL_MS = 10000; // Check every 10s
    static constexpr juce::int64 AUTO_QUIT_TIMEOUT_MS = 300000; // 5 minutes with no panners
    
    int debugFakePannerCount = 0;
    bool debugFakeBlocks = false;
    bool keepAlive = false;
    juce::int64 lastPannerSeenTime = 0;
    std::unique_ptr<Mach1::FakePannerSimulator> fakePannerSimulator;
};

START_JUCE_APPLICATION(M1SystemHelperApplication)

#endif
