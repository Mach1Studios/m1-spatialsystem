/*
    SessionUI.h
    -----------
    System tray icon with resizable multi-pane status window.
    
    Layout:
    - Root: Vertical split (Main content / Timeline placeholder)
    - Main content: Horizontal split (PannerList / Right panel)
    - Right panel: Vertical split (3D View / Monitor placeholder)
    
    Uses StretchableLayoutManager for resizable split panes.
*/

#pragma once

#include <JuceHeader.h>
#include "../Managers/PannerTrackingManager.h"
#include "Components/InputPanelContainer.h"
#include "Components/TimelineComponent.h"
#include "Components/Panner3DViewPanel.h"
#include "Components/MonitorPanel.h"

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
 * Main content component with resizable split panes
 */
class SessionMainComponent : public juce::Component,
                             private juce::Timer
{
public:
    SessionMainComponent(PannerTrackingManager& manager);
    ~SessionMainComponent() override;
    
    void resized() override;
    void paint(juce::Graphics& g) override;
    
    // Timer callback for polling panner updates
    void timerCallback() override;
    
    // Update data from manager
    void updateFromManager();
    
    // Access to components
    InputPanelContainer* getInputPanel() { return inputPanelContainer.get(); }
    Panner3DViewPanel* get3DView() { return view3DComponent.get(); }
    TimelineComponent* getTimeline() { return timelineComponent.get(); }

private:
    void setupLayout();
    
    // Reference to panner manager
    PannerTrackingManager& pannerManager;
    
    // UI Components
    std::unique_ptr<InputPanelContainer> inputPanelContainer;
    std::unique_ptr<Panner3DViewPanel> view3DComponent;
    std::unique_ptr<MonitorPanel> monitorComponent;
    std::unique_ptr<TimelineComponent> timelineComponent;
    
    // Layout managers
    juce::StretchableLayoutManager verticalLayout;   // Main vs Timeline
    juce::StretchableLayoutManager horizontalLayout; // Left vs Right
    juce::StretchableLayoutManager rightPanelLayout; // 3D vs Monitor
    
    // Resizer bars
    std::unique_ptr<juce::StretchableLayoutResizerBar> mainTimelineResizer;
    std::unique_ptr<juce::StretchableLayoutResizerBar> leftRightResizer;
    std::unique_ptr<juce::StretchableLayoutResizerBar> view3DMonitorResizer;
    
    // Styling
    juce::Colour backgroundColour{0xFF1A1A1A};
    juce::Colour resizerColour{0xFF404040};
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SessionMainComponent)
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
    void copyDiagnosticsToClipboard();
    juce::String generateDiagnosticsText();
    
    // Icon management
    void updateTrayIcon();
    juce::Image createTrayIcon(bool isActive);
    
    // Components
    PannerTrackingManager& pannerManager;
    std::unique_ptr<SessionDocumentWindow> sessionWindow;
    std::unique_ptr<juce::PopupMenu> trayMenu;
    std::unique_ptr<SessionMainComponent> mainComponent;
    
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
