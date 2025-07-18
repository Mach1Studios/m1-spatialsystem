/*
    SessionUI.h
    -----------
    System tray icon using timer-based menu approach that works with native OS menus
*/

#pragma once

#include <JuceHeader.h>
#include "../Managers/PannerTrackingManager.h"
#include "Components/InputTracklistComponent.h"
#include "Components/TimelineComponent.h"

namespace Mach1 {

//==============================================================================
/**
 * Custom DocumentWindow that properly handles close button
 */
class SessionDocumentWindow : public juce::DocumentWindow
{
public:
    SessionDocumentWindow(const juce::String& name, juce::Colour backgroundColour, int requiredButtons)
        : DocumentWindow(name, backgroundColour, requiredButtons)
    {
    }
    
    void closeButtonPressed() override
    {
        // Simply hide the window when close button is pressed
        setVisible(false);
    }
    
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SessionDocumentWindow)
};

//==============================================================================
/**
 * System tray icon using timer-based approach for reliable native OS menus
 */
class SessionUI : public juce::SystemTrayIconComponent,
                  private juce::Timer
{
public:
    SessionUI(PannerTrackingManager& manager);
    ~SessionUI() override;

    // SystemTrayIconComponent overrides
    void mouseDown(const juce::MouseEvent& event) override;

private:
    // Timer callback for delayed menu display
    void timerCallback() override;
    
    // UI management
    void createMenu();
    void showSessionWindow();
    void updateStatus();
    
    // Icon management
    void updateTrayIcon();
    juce::Image createTrayIcon(bool isActive);
    
    // Components
    PannerTrackingManager& pannerManager;
    std::unique_ptr<SessionDocumentWindow> sessionWindow;
    std::unique_ptr<juce::PopupMenu> trayMenu;
    InputTracklistComponent* tracklistComponent = nullptr;  // Non-owning pointer for updates
    TimelineComponent* timelineComponent = nullptr;  // Non-owning pointer for updates
    
    // Update timer for data refresh
    std::unique_ptr<juce::Timer> updateTimer;
    
    // Status tracking
    int lastPannerCount;
    bool lastMemoryShareStatus;
    bool lastOSCStatus;
    bool isMenuTimer = false;  // Flag to distinguish menu vs data timers
    
    // MenuBarModel for macOS compatibility
    class MyMenuBarModel : public juce::MenuBarModel
    {
    public:
        MyMenuBarModel();
        ~MyMenuBarModel() override;
        
        juce::StringArray getMenuBarNames() override { return {""}; }
        juce::PopupMenu getMenuForIndex(int, const juce::String&) override { return juce::PopupMenu(); }
        void menuItemSelected(int, int) override {}
        
    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MyMenuBarModel)
    };
    
    MyMenuBarModel menuBarModel;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SessionUI)
};

} // namespace Mach1 
