/*
    InputPanelContainer.h
    ---------------------
    Container component that allows switching between InputTracklistComponent 
    and InputMixerComponent views.
    
    Features:
    - Toolbar with view toggle buttons (List/Mixer)
    - Manages both child views
    - Forwards selection and data updates to both views
*/

#pragma once

#include <JuceHeader.h>
#include "InputTracklistComponent.h"
#include "InputMixerComponent.h"
#include "FlatButtonLookAndFeel.h"
#include "../../Managers/PannerTrackingManager.h"

namespace Mach1 {

/**
 * View modes for the input panel
 */
enum class InputPanelViewMode {
    List,   // Table list view
    Mixer   // Channel strip mixer view
};

/**
 * Container that holds both InputTracklistComponent and InputMixerComponent
 * with a toolbar to switch between views
 */
class InputPanelContainer : public juce::Component
{
public:
    InputPanelContainer();
    ~InputPanelContainer() override;
    
    // Data updates - forwards to both views
    void updatePannerData(const std::vector<PannerInfo>& panners);
    void setSelectedPanner(int index);
    int getSelectedPanner() const;
    
    // Level meter updates (for mixer view)
    void updateLevelMeters(const std::vector<float>& levels);
    
    // Set tracking manager for bi-directional editing
    void setPannerTrackingManager(PannerTrackingManager* manager);
    
    // View mode
    void setViewMode(InputPanelViewMode mode);
    InputPanelViewMode getViewMode() const { return currentViewMode; }
    
    // Callbacks
    std::function<void(int)> onSelectionChanged;
    std::function<void(int pannerIndex, const juce::String& paramName, float value)> onParameterChanged;
    std::function<void(int channelIndex, float gain)> onGainChanged;
    
    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    // View components
    std::unique_ptr<InputTracklistComponent> tracklistView;
    std::unique_ptr<InputMixerComponent> mixerView;
    
    // Current view mode
    InputPanelViewMode currentViewMode = InputPanelViewMode::List;
    
    // Toolbar buttons
    std::unique_ptr<juce::TextButton> listViewButton;
    std::unique_ptr<juce::TextButton> mixerViewButton;
    
    // Layout
    static constexpr int TOOLBAR_HEIGHT = 28;
    
    // Styling - matching reference design
    juce::Colour backgroundColour{0xFF0D0D0D};
    juce::Colour toolbarColour{0xFF141414};
    juce::Colour buttonColour{0xFF1F1F1F};
    juce::Colour buttonActiveColour{0xFF939393};  // Gray for active state
    juce::Colour textColour{0xFFCCCCCC};
    juce::Colour headerTextColour{0xFF808080};    // Dimmer header text
    juce::Colour separatorColour{0xFF2A2A2A};
    juce::Colour borderColour{0xFF2A2A2A};
    
    // Update button states
    void updateButtonStates();
    
    // Custom look and feel for flat buttons
    FlatButtonLookAndFeel flatButtonLookAndFeel;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InputPanelContainer)
};

} // namespace Mach1

