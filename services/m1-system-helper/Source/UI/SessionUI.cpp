/*
    SessionUI.cpp
    -------------
    Implementation of the system tray icon and resizable status window
*/

#include "SessionUI.h"

namespace Mach1 {

//==============================================================================
// SessionMainComponent
//==============================================================================

SessionMainComponent::SessionMainComponent(PannerTrackingManager& manager)
    : pannerManager(manager)
{
    // Create components
    inputPanelContainer = std::make_unique<InputPanelContainer>();
    view3DComponent = std::make_unique<Panner3DViewPanel>();
    monitorComponent = std::make_unique<MonitorPanel>();
    timelineComponent = std::make_unique<TimelineComponent>();
    
    // Set up callbacks
    inputPanelContainer->onSelectionChanged = [this](int index) {
        view3DComponent->setSelectedPannerIndex(index);
    };
    
    inputPanelContainer->setPannerTrackingManager(&pannerManager);
    
    view3DComponent->onPannerSelected = [this](int index) {
        inputPanelContainer->setSelectedPanner(index);
    };
    
    // Add as visible children
    addAndMakeVisible(inputPanelContainer.get());
    addAndMakeVisible(view3DComponent.get());
    addAndMakeVisible(monitorComponent.get());
    addAndMakeVisible(timelineComponent.get());
    
    // Set up layout
    setupLayout();
    
    // Initial data update
    updateFromManager();
    
    // Start update timer for polling panner data
    startTimerHz(10); // Update at 10Hz
}

SessionMainComponent::~SessionMainComponent()
{
    stopTimer();
}

void SessionMainComponent::setupLayout()
{
    // Main vertical layout: [Main content] [Resizer] [Timeline]
    verticalLayout.setItemLayout(0, 100, -1.0, -0.75);  // Main content: 75%
    verticalLayout.setItemLayout(1, 5, 5, 5);           // Resizer
    verticalLayout.setItemLayout(2, 80, -0.4, -0.25);   // Timeline: 25%
    
    // Horizontal layout: [Input Panel] [Resizer] [Right Panel]
    horizontalLayout.setItemLayout(0, 200, -1.0, -0.35);  // Input panel: 35%
    horizontalLayout.setItemLayout(1, 5, 5, 5);           // Resizer
    horizontalLayout.setItemLayout(2, 200, -1.0, -0.65);  // Right panel: 65%
    
    // Right panel layout: [3D View] [Resizer] [Monitor]
    rightPanelLayout.setItemLayout(0, 100, -1.0, -0.65);  // 3D View: 65%
    rightPanelLayout.setItemLayout(1, 5, 5, 5);           // Resizer
    rightPanelLayout.setItemLayout(2, 80, -0.5, -0.35);   // Monitor: 35%
    
    // Create resizer bars
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
    juce::Component* verticalComps[] = { nullptr, mainTimelineResizer.get(), timelineComponent.get() };
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
}

void SessionMainComponent::paint(juce::Graphics& g)
{
    g.fillAll(backgroundColour);
}

void SessionMainComponent::timerCallback()
{
    // Poll panner data regularly
    updateFromManager();
}

void SessionMainComponent::updateFromManager()
{
    auto panners = pannerManager.getActivePanners();
    
    inputPanelContainer->updatePannerData(panners);
    view3DComponent->updatePannerData(panners);
    timelineComponent->updateBufferEvents(panners);
}

//==============================================================================
// SessionUI::MyMenuBarModel
//==============================================================================

SessionUI::MyMenuBarModel::MyMenuBarModel()
{
    // Set up dummy menu for macOS compatibility
    juce::PopupMenu dummyMenu;
    dummyMenu.addItem("Dummy", [](){});
    juce::MenuBarModel::setMacMainMenu(this, &dummyMenu);
}

SessionUI::MyMenuBarModel::~MyMenuBarModel()
{
    juce::MenuBarModel::setMacMainMenu(nullptr);
}

//==============================================================================
// SessionUI
//==============================================================================

SessionUI::SessionUI(PannerTrackingManager& manager)
    : pannerManager(manager),
      lastPannerCount(-1),
      lastMemoryShareStatus(false),
      lastOSCStatus(false),
      isMenuTimer(false)
{
    DBG("[SessionUI] Constructor started - using timer-based system tray");
    
    // Create tray icon
    updateTrayIcon();
    
    // Start async updates for data refresh (every 1 second)
    startTimer(1000);
    
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

void SessionUI::mouseDown(const juce::MouseEvent& event)
{
    DBG("[SessionUI] mouseDown event triggered!");
    
    // Bring app to foreground (crucial for macOS)
    juce::Process::makeForegroundProcess();
    
    // Switch to menu timer mode (50ms delay)
    stopTimer();
    isMenuTimer = true;
    startTimer(50);
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
            
            // Use MessageManager::callAsync like the working example
            juce::MessageManager::callAsync([this]()
            {
                DBG("[SessionUI] Async callback - calling showDropdownMenu");
                showDropdownMenu(*trayMenu);
                DBG("[SessionUI] showDropdownMenu call completed");
                
                // Restart regular data timer after menu interaction
                startTimer(1000);
            });
        }
        else
        {
            DBG("[SessionUI] ERROR: trayMenu is null!");
            // Restart regular timer even if menu failed
            startTimer(1000);
        }
    }
    else
    {
        // Handle regular status updates (every 1 second)
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
        // Create the main component
        mainComponent = std::make_unique<SessionMainComponent>(pannerManager);
        
        // Create the window
        sessionWindow = std::make_unique<SessionDocumentWindow>(
            "Mach1 Spatial System Helper",
            juce::Colour(0xFF1A1A1A),
            juce::DocumentWindow::allButtons);
        
        sessionWindow->setContentNonOwned(mainComponent.get(), false);
        sessionWindow->setResizable(true, false);
        sessionWindow->centreWithSize(1100, 700);
        sessionWindow->setUsingNativeTitleBar(true);
    }
    
    sessionWindow->setVisible(true);
    sessionWindow->toFront(true);
    
    // Update data
    if (mainComponent)
    {
        mainComponent->updateFromManager();
    }
}

void SessionUI::updateStatus()
{
    // Get current panner count using the correct method
    lastPannerCount = static_cast<int>(pannerManager.getActivePanners().size());
    
    // Get memory share status using the correct method
    lastMemoryShareStatus = pannerManager.isUsingMemoryShare();
    
    // Get OSC status using the correct method
    lastOSCStatus = pannerManager.isUsingOSC();
    
    // Update tray icon based on status
    updateTrayIcon();
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
    text << juce::newLine;
    
    int index = 1;
    for (const auto& panner : panners)
    {
        text << "--- Panner " << index << " ---" << juce::newLine;
        text << "  Name: " << panner.name << juce::newLine;
        text << "  Port: " << juce::String(panner.port) << juce::newLine;
        text << "  Process ID: " << juce::String(static_cast<int>(panner.processId)) << juce::newLine;
        text << "  Channels: " << juce::String(static_cast<int>(panner.channels)) << juce::newLine;
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
}

juce::Image SessionUI::createTrayIcon(bool isActive)
{
    juce::Image icon(juce::Image::ARGB, 32, 32, true);
    juce::Graphics g(icon);
    
    g.fillAll(juce::Colours::transparentBlack);
    g.setColour(isActive ? juce::Colours::green : juce::Colours::grey);
    g.setFont(24.0f);
    
    // Draw "M1" for Mach1
    juce::String text = "M1";
    g.drawText(text, 0, 0, 32, 32, juce::Justification::centred, true);
    
    return icon;
}

} // namespace Mach1
