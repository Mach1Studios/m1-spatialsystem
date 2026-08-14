/*
    SessionUI.cpp
    -------------
    Implementation of the system tray icon and resizable status window
*/

#include "SessionUI.h"
#include "BinaryData.h"
#include "../Common/ProjectSessionPolicy.h"
#include "../Core/ExportEngine.h"
#include "../M1SystemHelperService.h"
#include <Mach1Encode.h>
#include <algorithm>
#include <map>

namespace Mach1 {

namespace {
constexpr int kTrayStatusUpdateIntervalMs = 500;
constexpr int kTrayMenuDelayMs = 50;
}

//==============================================================================
// SessionMainComponent
//==============================================================================

SessionMainComponent::SessionMainComponent(PannerTrackingManager& manager,
                                           ClientManager& clientManagerRef,
                                           OSCHandler& oscHandlerRef,
                                           ProjectPairingManager& projectPairingManagerRef,
                                           bool debugFakeBlocks,
                                           MixEngine* mixEngineRef)
    : pannerManager(manager)
    , clientManager(clientManagerRef)
    , oscHandler(oscHandlerRef)
    , projectPairingManager(projectPairingManagerRef)
    , mixEngine(mixEngineRef)
    , m_debugFakeBlocks(debugFakeBlocks)
{
    // Create components
    inputPanelContainer = std::make_unique<InputPanelContainer>();
    view3DComponent = std::make_unique<Panner3DViewPanel>();
    monitorComponent = std::make_unique<MonitorPanel>();
    captureTimelinePanel = std::make_unique<CaptureTimelinePanel>();
    
    // Set up callbacks
    inputPanelContainer->onSelectionChanged = [this](int index) {
        view3DComponent->setSelectedPannerIndex(index);
    };
    
    inputPanelContainer->setPannerTrackingManager(&pannerManager);
    
    view3DComponent->onPannerSelected = [this](int index) {
        inputPanelContainer->setSelectedPanner(index);
    };

    monitorComponent->onActiveMonitorSelected = [this](int port) {
        clientManager.rotateMonitorToActive(port);
        updateFromManager();
    };

    monitorComponent->onOrientationChanged = [this](float yaw, float pitch, float roll) {
        oscHandler.applyMonitorOrientationFromUi(yaw, pitch, roll);
    };

    monitorComponent->onOutputChannelCountChanged = [this](int channelCount) {
        oscHandler.applyChannelConfigFromUi(channelCount);
    };
    
    // Set up capture timeline callbacks
    captureTimelinePanel->onResetClicked = [this]() {
        DBG("[SessionMainComponent] Timeline reset clicked");
    };
    
    captureTimelinePanel->onExportClicked = [this]() {
        runExport();
    };

    captureTimelinePanel->onStorageClicked = [this]() {
        if (storageOverlay)
            storageOverlay->open();
    };
    
    // Add as visible children
    addAndMakeVisible(inputPanelContainer.get());
    addAndMakeVisible(view3DComponent.get());
    addAndMakeVisible(monitorComponent.get());
    addAndMakeVisible(captureTimelinePanel.get());

    // Export result notification overlay (hidden until an export finishes)
    exportResultOverlay = std::make_unique<ExportResultOverlay>();
    addChildComponent(exportResultOverlay.get());

    // Storage panel overlay (opened from the timeline's STORAGE button)
    storageOverlay = std::make_unique<StorageOverlay>();
    storageOverlay->getActiveSessionId = [this]() {
        return (captureEngine && captureEngine->isCapturing())
            ? captureEngine->getSessionId() : juce::String();
    };
    addChildComponent(storageOverlay.get());

    projectSessionOverlay = std::make_unique<ProjectSessionOverlay>();
    projectSessionOverlay->onNameProject = [this](uint32_t hostProcessId,
                                                  const juce::String& displayName) {
        const auto binding = oscHandler.nameProjectForHost(hostProcessId, displayName);
        if (binding.isNamed())
        {
            projectSessionOverlay->setVisible(false);
            startCaptureForBinding(hostProcessId, binding);
        }
    };
    projectSessionOverlay->onSelectProject = [this](uint32_t hostProcessId,
                                                    const juce::String& bindingId) {
        const auto binding = oscHandler.selectProjectForHost(hostProcessId, bindingId);
        if (binding.isNamed())
        {
            projectSessionOverlay->setVisible(false);
            startCaptureForBinding(hostProcessId, binding);
        }
    };
    projectSessionOverlay->onDismiss = [this]() {
        promptDismissedForThisWindowOpen = true;
    };
    addChildComponent(projectSessionOverlay.get());
    
    // Set up layout
    setupLayout();
    
    // Set up capture engine
    setupCaptureEngine(debugFakeBlocks);
    
    // Initial data update
    updateFromManager();
    
    // Keep the open helper window close to backend tracking latency.
    startTimerHz(20); // Update at 20Hz
}

SessionMainComponent::~SessionMainComponent()
{
    stopTimer();
    
    // Stop capture engine
    if (captureEngine)
    {
        captureEngine->stopCapture();
    }
}

void SessionMainComponent::setupCaptureEngine(bool debugFakeBlocks)
{
    captureEngine = std::make_unique<CaptureEngine>(pannerManager);
    
    // Enable debug mode if requested
    if (debugFakeBlocks)
    {
        captureEngine->setDebugFakeBlocks(true);
        DBG("[SessionMainComponent] Debug fake blocks mode enabled");
    }
    
    // Connect to capture timeline panel
    captureTimelinePanel->setCaptureEngine(captureEngine.get());
    
    // Capture starts only after a streaming host has a stable project binding.
    // This prevents a new timestamp-named orphan every time the UI is opened.
}

bool SessionMainComponent::startCapture(const juce::String& sessionId)
{
    if (!captureEngine)
        return false;
    
    // Generate session ID if not provided
    juce::String actualSessionId = sessionId;
    if (actualSessionId.isEmpty())
    {
        actualSessionId = "Session_" + juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S");
    }
    
    // Durable capture root (Application Support): the old temp/caches root
    // could be purged by the OS behind the user's back. Any sessions still in
    // the legacy location are moved over once.
    StorageGovernor::migrateLegacyCaptures();
    const juce::File captureRoot = StorageGovernor::getDefaultCaptureRoot();
    
    return captureEngine->startCapture(actualSessionId, captureRoot);
}

void SessionMainComponent::startCaptureForBinding(uint32_t hostProcessId,
                                                  const ProjectBinding& binding)
{
    if (!captureEngine || !binding.isNamed() || hostProcessId == 0)
        return;

    if (captureEngine->isCapturing())
        captureEngine->stopCapture();

    StorageGovernor::migrateLegacyCaptures();
    if (captureEngine->startCapture(binding.sessionId,
                                    StorageGovernor::getDefaultCaptureRoot(),
                                    hostProcessId,
                                    binding.bindingId,
                                    binding.displayName))
    {
        selectedHostProcessId = hostProcessId;
        activeProjectBindingId = binding.bindingId;
        promptDismissedForThisWindowOpen = false;
    }
}

void SessionMainComponent::stopCapture()
{
    if (captureEngine)
    {
        captureEngine->stopCapture();
    }
}

bool SessionMainComponent::isCapturing() const
{
    return captureEngine && captureEngine->isCapturing();
}

void SessionMainComponent::runExport()
{
    if (!captureEngine)
        return;

    bool expected = false;
    if (!exportInProgress.compare_exchange_strong(expected, true))
    {
        DBG("[SessionMainComponent] Export already in progress");
        return;
    }

    const juce::File sessionDir = captureEngine->getCaptureRoot()
                                      .getChildFile(captureEngine->getSessionId());

    // Export in the format the monitor/system is currently set to (4/8/14)
    const int channelCount = oscHandler.getActiveMonitorSnapshot().systemChannelCount;
    ExportEngine::Request request;
    request.sessionDir = sessionDir;
    switch (channelCount)
    {
        case 4:  request.outputMode = M1Spatial_4;  break;
        case 14: request.outputMode = M1Spatial_14; break;
        default: request.outputMode = M1Spatial_8;  break;
    }
    const int busChannels = (channelCount == 4 || channelCount == 14) ? channelCount : 8;
    request.outputFile = sessionDir.getChildFile("exports")
        .getChildFile("M1Spatial-" + juce::String(busChannels) + "_"
                      + juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S") + ".wav");

    auto safeThis = juce::Component::SafePointer<SessionMainComponent>(this);
    juce::Thread::launch([safeThis, request]() {
        const ExportEngine::Result result = ExportEngine::exportSession(request);

        juce::MessageManager::callAsync([safeThis, result]() {
            if (safeThis == nullptr)
                return;

            safeThis->exportInProgress.store(false);

            auto* overlay = safeThis->exportResultOverlay.get();
            if (overlay == nullptr)
                return;

            if (!result.success)
            {
                overlay->show("Export failed",
                              { { result.errorMessage, HelperUIColours::text } },
                              juce::File(), /*isError*/ true);
                return;
            }

            const double seconds = result.sampleRate > 0
                ? static_cast<double>(result.endSample - result.startSample) / result.sampleRate
                : 0.0;

            std::vector<ExportResultOverlay::Line> lines;
            lines.push_back({ juce::String(result.channels) + "-channel Mach1 Spatial mix, "
                                  + juce::String(seconds, 2) + " s @ "
                                  + juce::String(static_cast<int>(result.sampleRate)) + " Hz",
                              HelperUIColours::text });

            // The export rate comes from the captured plugin blocks, so an
            // unexpected value here means the DAW itself ran the plugins at
            // that rate (e.g. a Bluetooth headset forcing the device to 16/24
            // kHz). Mixed rates mean the rate changed mid-session and the
            // timeline positions are unreliable - tell the user to re-capture.
            if (result.capturedSampleRates.size() > 1)
            {
                juce::String rateList;
                for (const auto rate : result.capturedSampleRates)
                    rateList += (rateList.isEmpty() ? "" : ", ") + juce::String(static_cast<int>(rate));
                lines.push_back({ "Warning: mixed sample rates captured (" + rateList
                                      + " Hz). The DAW rate changed mid-session; "
                                        "clear the session data and re-capture.",
                                  HelperUIColours::warning });
            }

            const bool fullCoverage = result.allStreamsCoveragePercent > 99.9f;
            lines.push_back({ "Full coverage (all inputs): "
                                  + juce::String(result.allStreamsCoveragePercent, 1) + "%",
                              fullCoverage ? HelperUIColours::success : HelperUIColours::warning });

            for (const auto& stream : result.streams)
            {
                const juce::String streamName = stream.displayName.empty()
                    ? juce::String(stream.name)
                    : juce::String(stream.displayName);

                juce::String line = streamName + ": "
                    + juce::String(stream.coveragePercent, 1) + "% received";
                if (!stream.missing.empty())
                    line += " (" + juce::String(static_cast<int>(stream.missing.size())) + " gap(s), see report)";

                lines.push_back({ line, stream.missing.empty() ? HelperUIColours::success
                                                               : HelperUIColours::warning });
            }

            lines.push_back({ result.outputFile.getFullPathName(), HelperUIColours::textDim });

            overlay->show("Export complete", lines, result.outputFile, /*isError*/ false);
        });
    });
}

void SessionMainComponent::setupLayout()
{
    // Main vertical layout: [Main content] [Resizer] [Timeline]
    // Reference shows timeline at bottom, but smaller proportion
    verticalLayout.setItemLayout(0, 100, -1.0, -0.80);  // Main content: 80%
    verticalLayout.setItemLayout(1, 3, 3, 3);           // Thinner resizer
    verticalLayout.setItemLayout(2, 60, -0.35, -0.20);  // Timeline: 20%
    
    // Horizontal layout: [Input Panel] [Resizer] [Right Panel]
    // Reference shows roughly equal split with input tracklist on left
    horizontalLayout.setItemLayout(0, 200, -1.0, -0.50);  // Input panel: 50%
    horizontalLayout.setItemLayout(1, 3, 3, 3);           // Thinner resizer
    horizontalLayout.setItemLayout(2, 200, -1.0, -0.50);  // Right panel: 50%
    
    // Right panel layout: [3D View] [Resizer] [Monitor]
    // Reference shows 3D view larger than monitor
    rightPanelLayout.setItemLayout(0, 100, -1.0, -0.66);  // 3D View: 66%
    rightPanelLayout.setItemLayout(1, 3, 3, 3);           // Thinner resizer
    rightPanelLayout.setItemLayout(2, 80, -0.45, -0.34);  // Monitor: 34%
    
    // Create resizer bars - thinner/more subtle
    mainTimelineResizer = std::make_unique<juce::StretchableLayoutResizerBar>(
        &verticalLayout, 1, false);
    mainTimelineResizer->setColour(juce::ResizableWindow::backgroundColourId, resizerColour);
    addAndMakeVisible(mainTimelineResizer.get());
    
    leftRightResizer = std::make_unique<juce::StretchableLayoutResizerBar>(
        &horizontalLayout, 1, true);
    leftRightResizer->setColour(juce::ResizableWindow::backgroundColourId, resizerColour);
    addAndMakeVisible(leftRightResizer.get());
    
    view3DMonitorResizer = std::make_unique<juce::StretchableLayoutResizerBar>(
        &rightPanelLayout, 1, false);
    view3DMonitorResizer->setColour(juce::ResizableWindow::backgroundColourId, resizerColour);
    addAndMakeVisible(view3DMonitorResizer.get());
}

void SessionMainComponent::resized()
{
    auto bounds = getLocalBounds();
    
    // Vertical layout for main content vs timeline
    juce::Component* verticalComps[] = { nullptr, mainTimelineResizer.get(), captureTimelinePanel.get() };
    verticalLayout.layOutComponents(verticalComps, 3, 
                                    bounds.getX(), bounds.getY(), 
                                    bounds.getWidth(), bounds.getHeight(),
                                    true, true);
    
    // Get main content area (from first item of vertical layout)
    auto mainBounds = bounds.withHeight(verticalLayout.getItemCurrentPosition(1));
    
    // Horizontal layout for input panel vs right panel
    juce::Component* horizontalComps[] = { inputPanelContainer.get(), leftRightResizer.get(), nullptr };
    horizontalLayout.layOutComponents(horizontalComps, 3,
                                      mainBounds.getX(), mainBounds.getY(),
                                      mainBounds.getWidth(), mainBounds.getHeight(),
                                      false, true);
    
    // Get right panel area
    int rightPanelX = horizontalLayout.getItemCurrentPosition(2);
    auto rightBounds = mainBounds.withLeft(rightPanelX);
    
    // Right panel layout for 3D view vs monitor
    juce::Component* rightComps[] = { view3DComponent.get(), view3DMonitorResizer.get(), monitorComponent.get() };
    rightPanelLayout.layOutComponents(rightComps, 3,
                                      rightBounds.getX(), rightBounds.getY(),
                                      rightBounds.getWidth(), rightBounds.getHeight(),
                                      true, true);

    // Notification overlays cover everything
    if (exportResultOverlay)
        exportResultOverlay->setBounds(getLocalBounds());
    if (storageOverlay)
        storageOverlay->setBounds(getLocalBounds());
    if (projectSessionOverlay)
        projectSessionOverlay->setBounds(getLocalBounds());
}

void SessionMainComponent::paint(juce::Graphics& g)
{
    g.fillAll(backgroundColour);
}

void SessionMainComponent::timerCallback()
{
    // Poll panner data regularly
    updateFromManager();
    maybeResolveCaptureSession();
}

std::vector<ProjectSessionOverlay::HostOption> SessionMainComponent::getStreamingHosts() const
{
    std::map<uint32_t, int> countsByProcess;
    for (const auto& panner : pannerManager.getActivePanners())
    {
        const bool isStreaming = m_debugFakeBlocks
            || (panner.isMemoryShareBased
                && panner.externalStreamingActive
                && panner.connectionStatus != PannerConnectionStatus::Disconnected);
        if (isStreaming && panner.processId != 0)
            ++countsByProcess[panner.processId];
    }

    std::vector<ProjectSessionOverlay::HostOption> hosts;
    for (const auto& [processId, count] : countsByProcess)
    {
        ProjectSessionOverlay::HostOption option;
        option.processId = processId;
        option.streamingPanners = count;
        option.currentBinding = projectPairingManager.getHostBinding(processId);
        option.ambiguous = projectPairingManager.isHostAmbiguous(processId);
        hosts.push_back(std::move(option));
    }
    return hosts;
}

void SessionMainComponent::maybeResolveCaptureSession()
{
    if (!sessionWindowHasBeenShown || !captureEngine)
        return;

    const auto hosts = getStreamingHosts();
    if (captureEngine->isCapturing())
    {
        const auto selected = std::find_if(hosts.begin(), hosts.end(), [this](const auto& host) {
            return host.processId == selectedHostProcessId;
        });
        if (selected != hosts.end())
            return;

        // The same saved project can reopen under a new DAW PID. Rebind the
        // capture filter to that process and continue in the same directory.
        const auto reopened = std::find_if(hosts.begin(), hosts.end(), [this](const auto& host) {
            return !host.ambiguous
                && host.currentBinding.bindingId == activeProjectBindingId
                && host.currentBinding.isNamed();
        });
        if (reopened != hosts.end())
        {
            startCaptureForBinding(reopened->processId, reopened->currentBinding);
            return;
        }

        if (hosts.empty())
            return; // host may be restarting; keep the session available to resume

        captureEngine->stopCapture();
        selectedHostProcessId = 0;
        activeProjectBindingId.clear();
    }

    const auto action = ProjectSessionPolicy::chooseAction(
        hosts.size(),
        hosts.size() == 1 && hosts.front().currentBinding.isNamed(),
        hosts.size() == 1 && hosts.front().ambiguous,
        captureEngine->isCapturing());

    if (action == ProjectSessionPolicy::Action::AutoStart)
    {
        startCaptureForBinding(hosts.front().processId, hosts.front().currentBinding);
        return;
    }

    if (action == ProjectSessionPolicy::Action::Prompt
        && !promptDismissedForThisWindowOpen
        && projectSessionOverlay
        && !projectSessionOverlay->isVisible())
    {
        projectSessionOverlay->open(hosts, projectPairingManager.getKnownBindings());
    }
}

void SessionMainComponent::sessionWindowShown()
{
    sessionWindowHasBeenShown = true;
    promptDismissedForThisWindowOpen = false;
    maybeResolveCaptureSession();
}

void SessionMainComponent::showProjectSessionChooser()
{
    const auto hosts = getStreamingHosts();
    if (hosts.empty() || !projectSessionOverlay)
        return;

    promptDismissedForThisWindowOpen = false;
    projectSessionOverlay->open(hosts, projectPairingManager.getKnownBindings());
}

void SessionMainComponent::updateFromManager()
{
    auto panners = pannerManager.getActivePanners();

    inputPanelContainer->updatePannerData(panners);
    view3DComponent->updatePannerData(panners);

    // Live input meters: per-feed peaks published by the MixEngine render
    // thread, in the same order as the strip list.
    if (mixEngine != nullptr && !panners.empty())
    {
        std::vector<float> levels(panners.size(), 0.0f);
        for (size_t i = 0; i < panners.size(); ++i)
            levels[i] = mixEngine->getFeedPeak(panners[i].processId, panners[i].memoryAddress);
        inputPanelContainer->updateLevelMeters(levels);
    }

    const auto activeMonitorSnapshot = oscHandler.getActiveMonitorSnapshot();
    MonitorPanelState monitorPanelState;
    monitorPanelState.yaw = activeMonitorSnapshot.masterYaw;
    monitorPanelState.pitch = activeMonitorSnapshot.masterPitch;
    monitorPanelState.roll = activeMonitorSnapshot.masterRoll;
    monitorPanelState.channelCount = activeMonitorSnapshot.systemChannelCount;
    if (mixEngine != nullptr)
        monitorPanelState.streamingPanners = mixEngine->getStatus().streamingFeeds;
    for (const auto& monitor : activeMonitorSnapshot.monitors)
    {
        monitorPanelState.monitors.push_back({ monitor.port, monitor.active });
    }
    monitorComponent->updateState(monitorPanelState);

    // Note: CaptureTimelinePanel updates itself via CaptureEngine's ChangeBroadcaster
}

//==============================================================================
// SessionUI::MyMenuBarModel
//==============================================================================

SessionUI::MyMenuBarModel::MyMenuBarModel()
{
#if JUCE_MAC
    // Register the model without passing a temporary PopupMenu pointer.
    // AppKit may consult the extra Apple menu items long after this constructor
    // returns, so handing it a stack object can leave dangling state.
    juce::MenuBarModel::setMacMainMenu(this);
#endif
}

SessionUI::MyMenuBarModel::~MyMenuBarModel()
{
#if JUCE_MAC
    juce::MenuBarModel::setMacMainMenu(nullptr);
#endif
}

//==============================================================================
// SessionUI
//==============================================================================

SessionUI::SessionUI(PannerTrackingManager& manager, ClientManager& clientManagerRef,
                     OSCHandler& oscHandlerRef, ProjectPairingManager& projectPairingManagerRef,
                     bool debugFakeBlocks, MixEngine* mixEngineRef)
    : pannerManager(manager),
      clientManager(clientManagerRef),
      oscHandler(oscHandlerRef),
      projectPairingManager(projectPairingManagerRef),
      mixEngine(mixEngineRef),
      lastPannerCount(-1),
      lastMemoryShareStatus(false),
      lastOSCStatus(false),
      isMenuTimer(false),
      m_debugFakeBlocks(debugFakeBlocks)
{
    DBG("[SessionUI] Constructor started - using timer-based system tray");

    loadTrayIcon();
    updateStatus();
    setIconTooltip("Mach1 Spatial System Helper");
    
    // Keep tray status reasonably fresh without rebuilding native tray assets every 100ms.
    startTimer(kTrayStatusUpdateIntervalMs);
    
    DBG("[SessionUI] System tray icon created successfully");
}

SessionUI::~SessionUI()
{
    DBG("[SessionUI] Destructor called");
    stopTimer();
    mainComponent = nullptr;
    sessionWindow = nullptr;
    trayMenu = nullptr;
}

void SessionUI::showStatusWindow()
{
    juce::Process::makeForegroundProcess();
    showSessionWindow();
}

void SessionUI::mouseDown(const juce::MouseEvent& event)
{
    DBG("[SessionUI] mouseDown event triggered!");
    
    // Bring app to foreground (crucial for macOS)
    juce::Process::makeForegroundProcess();
    
    // Switch to menu timer mode (50ms delay)
    stopTimer();
    isMenuTimer = true;
    startTimer(kTrayMenuDelayMs);
}

void SessionUI::timerCallback()
{
    if (isMenuTimer)
    {
        // Handle menu display (50ms timer)
        DBG("[SessionUI] timerCallback - showing menu");
        stopTimer();
        isMenuTimer = false;
        
        // Create fresh menu each time
        createMenu();
        
        if (trayMenu)
        {
            DBG("[SessionUI] Using MessageManager::callAsync approach");
            auto safeThis = juce::Component::SafePointer<SessionUI>(this);
            
            // Use MessageManager::callAsync like the working example
            juce::MessageManager::callAsync([safeThis]()
            {
                if (safeThis == nullptr)
                    return;

#if JUCE_MAC
                DBG("[SessionUI] Async callback - calling showDropdownMenu");
                safeThis->showDropdownMenu(*safeThis->trayMenu);
                DBG("[SessionUI] showDropdownMenu call completed");
                
                // Restart regular data timer after menu interaction
                safeThis->startTimer(kTrayStatusUpdateIntervalMs);
#else
                DBG("[SessionUI] Async callback - calling PopupMenu::showMenuAsync");
                safeThis->trayMenu->showMenuAsync(juce::PopupMenu::Options(),
                                    [safeThis](int)
                                    {
                                        if (safeThis != nullptr)
                                        {
                                            safeThis->trayMenu.reset();
                                            safeThis->startTimer(kTrayStatusUpdateIntervalMs);
                                        }
                                    });
#endif
            });
        }
        else
        {
            DBG("[SessionUI] ERROR: trayMenu is null!");
            // Restart regular timer even if menu failed
            startTimer(kTrayStatusUpdateIntervalMs);
        }
    }
    else
    {
        // Handle regular status updates
        updateStatus();
    }
}

void SessionUI::createMenu()
{
    DBG("[SessionUI] Creating menu");
    
    trayMenu = std::make_unique<juce::PopupMenu>();
    
    // Get current status
    updateStatus();
    
    // Status information
    juce::String statusText = "Panners: " + juce::String(lastPannerCount);
    statusText += ", Memory: " + juce::String(lastMemoryShareStatus ? "Active" : "Inactive");
    statusText += ", OSC: " + juce::String(lastOSCStatus ? "Active" : "Inactive");
    
    // Add menu items
    trayMenu->addItem("Open Status Window", [this]() { 
        DBG("[SessionUI] Open Status Window callback triggered!");
        showSessionWindow(); 
    });
    trayMenu->addItem("Choose Streaming Session...", [this]() {
        showSessionWindow();
        if (mainComponent)
            mainComponent->showProjectSessionChooser();
    });
    trayMenu->addSeparator();

    // P4: user-facing external renderer toggle. Ticked = panners on
    // mono/stereo-only buses stream audio here for mixing/capture/export.
    const bool streamingEnabled = oscHandler.isExternalRendererEnabled();
    trayMenu->addItem("Enable Audio Streaming (External Renderer)", true, streamingEnabled,
                      [this, streamingEnabled]() {
                          oscHandler.setExternalRendererEnabled(!streamingEnabled);
                      });
    if (!M1SystemHelperService::getInstance().isSharedMemoryDirWritable())
    {
        trayMenu->addItem("WARNING: shared memory dir not writable", false, false, []() {});
    }
    trayMenu->addSeparator();
    trayMenu->addItem("Copy Diagnostics", [this]() {
        DBG("[SessionUI] Copy Diagnostics callback triggered!");
        copyDiagnosticsToClipboard();
    });
    trayMenu->addSeparator();
    trayMenu->addItem(statusText, false, false, [](){}); // Non-clickable status
    trayMenu->addSeparator();
    trayMenu->addItem("Quit", [this]() { 
        DBG("[SessionUI] Quit callback triggered!");
        
        // CRITICAL: Clean up SessionUI components BEFORE triggering quit
        // This prevents MenuBarModel singleton conflicts during JUCE shutdown
        
        // Stop timers first
        stopTimer();
        
        // Clean up main component
        if (mainComponent) {
            mainComponent.reset();
        }
        
        // Clean up session window if open
        if (sessionWindow) {
            sessionWindow->setVisible(false);
            sessionWindow.reset();
        }
        
        // Clean up tray menu
        if (trayMenu) {
            trayMenu.reset();
        }
        
        // Hide the tray icon
        setVisible(false);
        
        DBG("[SessionUI] SessionUI cleaned up, now triggering quit");
        
        // Use async call to ensure cleanup completes before quit
        juce::MessageManager::callAsync([]() {
            juce::JUCEApplication::getInstance()->systemRequestedQuit(); 
        });
    });
    
    DBG("[SessionUI] Menu created with " + juce::String(trayMenu->getNumItems()) + " items");
}

void SessionUI::showSessionWindow()
{
    DBG("[SessionUI] showSessionWindow called");
    
    if (!sessionWindow)
    {
        // Create the main component with debug flag
        mainComponent = std::make_unique<SessionMainComponent>(
            pannerManager, clientManager, oscHandler, projectPairingManager,
            m_debugFakeBlocks, mixEngine);
        
        // Create the window with darker background matching reference
        sessionWindow = std::make_unique<SessionDocumentWindow>(
            "Mach1 Spatial System",
            HelperUIColours::background,
            juce::DocumentWindow::allButtons);
        
        sessionWindow->setContentNonOwned(mainComponent.get(), false);
        sessionWindow->setResizable(true, false);
        sessionWindow->centreWithSize(1200, 750);  // Slightly larger default
        sessionWindow->setUsingNativeTitleBar(true);
    }
    
    sessionWindow->setVisible(true);
    sessionWindow->toFront(true);
    
    // Update data
    if (mainComponent)
    {
        mainComponent->updateFromManager();
        mainComponent->sessionWindowShown();
    }
}

void SessionUI::updateStatus()
{
    const int newPannerCount = static_cast<int>(pannerManager.getActivePanners().size());
    const bool newMemoryShareStatus = pannerManager.isUsingMemoryShare();
    const bool newOSCStatus = pannerManager.isUsingOSC();

    const bool statusChanged = newPannerCount != lastPannerCount
        || newMemoryShareStatus != lastMemoryShareStatus
        || newOSCStatus != lastOSCStatus;

    lastPannerCount = newPannerCount;
    lastMemoryShareStatus = newMemoryShareStatus;
    lastOSCStatus = newOSCStatus;

    if (statusChanged)
        updateTrayIcon();

    // Reflect the renderer toggle in the status window title so its state is
    // obvious even without opening the tray menu.
    if (sessionWindow != nullptr)
    {
        const juce::String title = oscHandler.isExternalRendererEnabled()
            ? "Mach1 Spatial System"
            : "Mach1 Spatial System - AUDIO STREAMING DISABLED";
        if (sessionWindow->getName() != title)
            sessionWindow->setName(title);
    }
}

void SessionUI::copyDiagnosticsToClipboard()
{
    juce::String diagnostics = generateDiagnosticsText();
    juce::SystemClipboard::copyTextToClipboard(diagnostics);
    DBG("[SessionUI] Diagnostics copied to clipboard");
}

juce::String SessionUI::generateDiagnosticsText()
{
    juce::String text;
    
    text << "=== Mach1 Spatial System Helper Diagnostics ===" << juce::newLine;
    text << "Time: " << juce::Time::getCurrentTime().toString(true, true, true, true) << juce::newLine;
    text << juce::newLine;
    
    auto panners = pannerManager.getActivePanners();
    
    text << "Connected Panners: " << juce::String(static_cast<int>(panners.size())) << juce::newLine;
    text << "MemoryShare Active: " << (lastMemoryShareStatus ? "Yes" : "No") << juce::newLine;
    text << "OSC Active: " << (lastOSCStatus ? "Yes" : "No") << juce::newLine;
    text << "External Renderer Enabled: " << (oscHandler.isExternalRendererEnabled() ? "Yes" : "No") << juce::newLine;
    auto& service = M1SystemHelperService::getInstance();
    text << "Shared Memory Dir: " << service.getSharedMemoryDirPath()
         << (service.isSharedMemoryDirWritable() ? " (writable)" : " (NOT WRITABLE)") << juce::newLine;
    text << juce::newLine;
    
    int index = 1;
    for (const auto& panner : panners)
    {
        text << "--- Panner " << index << " ---" << juce::newLine;
        text << "  Name: " << panner.name << juce::newLine;
        text << "  Port: " << juce::String(panner.port) << juce::newLine;
        text << "  Process ID: " << juce::String(static_cast<int>(panner.processId)) << juce::newLine;
        text << "  Channels: " << juce::String(static_cast<int>(panner.channels)) << juce::newLine;
        juce::String inputModeName;
        switch (panner.inputMode)
        {
            case 0: inputModeName = "Mono"; break;
            case 1: inputModeName = "Stereo"; break;
            default: inputModeName = "Mode " + juce::String(panner.inputMode); break;
        }
        text << "  Input Mode: " << inputModeName << juce::newLine;
        text << "  Azimuth: " << juce::String(panner.azimuth, 1) << juce::newLine;
        text << "  Elevation: " << juce::String(panner.elevation, 1) << juce::newLine;
        text << "  Diverge: " << juce::String(panner.diverge, 1) << juce::newLine;
        text << "  Gain: " << juce::String(panner.gain, 1) << " dB" << juce::newLine;
        text << "  Tracking: " << (panner.isMemoryShareBased ? "MemoryShare" : "OSC") << juce::newLine;
        text << "  Active: " << (panner.isActive ? "Yes" : "No") << juce::newLine;
        text << juce::newLine;
        index++;
    }
    
    return text;
}

void SessionUI::updateTrayIcon()
{
    bool isActive = (lastPannerCount > 0) || lastMemoryShareStatus || lastOSCStatus;
    
    auto icon = createTrayIcon(isActive);
    setIconImage(icon, icon);
    setIconTooltip("Mach1 Spatial System Helper");
}

juce::Image SessionUI::createTrayIcon(bool isActive)
{
    loadTrayIcon();

    if (trayIconSource.isValid())
    {
        constexpr int outputSize = 64;
        constexpr int glyphSize = 80;

        juce::Image icon(juce::Image::ARGB, outputSize, outputSize, true);
        auto scaled = trayIconSource.rescaled(glyphSize, glyphSize, juce::Graphics::highResamplingQuality);
        const int offsetX = (outputSize - glyphSize) / 2;
        const int offsetY = (outputSize - glyphSize) / 2;
        const float targetAlpha = isActive ? 1.0f : 0.55f;

        for (int y = 0; y < scaled.getHeight(); ++y)
        {
            for (int x = 0; x < scaled.getWidth(); ++x)
            {
                const auto pixel = scaled.getPixelAt(x, y);
                if (pixel.getFloatAlpha() <= 0.0f || pixel.getPerceivedBrightness() < 0.2f)
                    continue;

                icon.setPixelAt(offsetX + x, offsetY + y,
                                juce::Colours::white.withAlpha(pixel.getFloatAlpha() * targetAlpha));
            }
        }

        return icon;
    }

    juce::Image fallback(juce::Image::ARGB, 32, 32, true);
    juce::Graphics g(fallback);

    g.fillAll(juce::Colours::transparentBlack);
    g.setColour(isActive ? juce::Colours::white : juce::Colours::white.withAlpha(0.55f));
    g.setFont(24.0f);
    g.drawText("M1", 0, 0, 32, 32, juce::Justification::centred, true);

    return fallback;
}

void SessionUI::loadTrayIcon()
{
    if (trayIconSource.isValid())
        return;

    trayIconSource = juce::ImageFileFormat::loadFrom(BinaryData::icon_png, BinaryData::icon_pngSize);
}

} // namespace Mach1
