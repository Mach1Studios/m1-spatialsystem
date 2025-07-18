/*
    SessionUI.cpp
    -------------
    System tray icon using timer-based approach for reliable native OS menus
*/

#include "SessionUI.h"

namespace Mach1 {

//==============================================================================
// MenuBarModel Implementation (for macOS compatibility)
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
// SessionUI Implementation
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
        
        // Update components if window is open
        if (sessionWindow && sessionWindow->isVisible())
        {
            auto activePanners = pannerManager.getActivePanners();
            
            if (tracklistComponent)
            {
                tracklistComponent->updatePannerData(activePanners);
            }
            
            if (timelineComponent)
            {
                timelineComponent->updateBufferEvents(activePanners);
            }
        }
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
    trayMenu->addItem("Show Session UI", [this]() { 
        DBG("[SessionUI] Menu callback triggered!");
        showSessionWindow(); 
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
        sessionWindow = std::make_unique<SessionDocumentWindow>(
            "Mach1 Spatial System - Input Tracklist",
            juce::Colours::darkgrey,
            juce::DocumentWindow::allButtons
        );
        
        // Create main component container
        auto mainComponent = std::make_unique<juce::Component>();
        mainComponent->setSize(700, 600);  // Increased height for timeline
        
        // Create INPUT Tracklist component
        auto inputTracklist = std::make_unique<InputTracklistComponent>();
        
        // Set up selection callback
        inputTracklist->onSelectionChanged = [this](int index) {
            DBG("[SessionUI] Panner " + juce::String(index + 1) + " selected");
        };
        
        // Create Timeline component
        auto timeline = std::make_unique<TimelineComponent>();
        
        // Add components to main component
        mainComponent->addAndMakeVisible(inputTracklist.get());
        mainComponent->addAndMakeVisible(timeline.get());
        
        // Layout - tracklist on top, timeline on bottom
        inputTracklist->setBounds(10, 10, 680, 280);  // Top half
        timeline->setBounds(10, 300, 680, 290);  // Bottom half
        
        // Store references for updates
        tracklistComponent = inputTracklist.get();
        timelineComponent = timeline.get();
        
        // Release pointers (component takes ownership)
        inputTracklist.release();
        timeline.release();
        
        sessionWindow->setContentOwned(mainComponent.release(), true);
        sessionWindow->centreWithSize(700, 600);
        sessionWindow->setResizable(true, true);
        sessionWindow->setUsingNativeTitleBar(true);
    }
    
    // Update tracklist with current panner data
    if (tracklistComponent)
    {
        auto activePanners = pannerManager.getActivePanners();
        tracklistComponent->updatePannerData(activePanners);
    }
    
    sessionWindow->setVisible(true);
    sessionWindow->toFront(true);
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