/*
    MonitorPlaceholderPanel.h
    -------------------------
    Placeholder component for the m1-monitor GUI integration.
    
    This panel reserves space for the monitor interface that will be
    embedded later. For now it shows placeholder content and styling
    that matches the app theme.
    
    Design allows for easy replacement with actual monitor component.
*/

#pragma once

#include <JuceHeader.h>

namespace Mach1 {

/**
 * Placeholder panel for future m1-monitor GUI embedding
 */
class MonitorPanel : public juce::Component,
                                 public juce::Button::Listener
{
public:
    MonitorPanel();
    ~MonitorPanel() override;
    
    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    // Button listener
    void buttonClicked(juce::Button* button) override;
    
    // Future: Method to embed actual monitor component
    void setMonitorComponent(juce::Component* monitorComponent);

private:
    // UI elements
    std::unique_ptr<juce::Label> titleLabel;
    std::unique_ptr<juce::Label> statusLabel;
    
    // Future: Embedded monitor component
    juce::Component* embeddedMonitor = nullptr;
    
    // Styling - matching reference design
    juce::Colour backgroundColour{0xFF0D0D0D};
    juce::Colour toolbarColour{0xFF141414};
    juce::Colour borderColour{0xFF2A2A2A};
    juce::Colour textColour{0xFFCCCCCC};
    juce::Colour dimTextColour{0xFF666666};
    juce::Colour accentColour{0xFF939393};            // Gray accent
    
    // Toolbar height
    static constexpr int TOOLBAR_HEIGHT = 28;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MonitorPanel)
};

} // namespace Mach1

