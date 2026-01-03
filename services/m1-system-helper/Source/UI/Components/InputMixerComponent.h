/*
    InputMixerComponent.h
    ---------------------
    UI component that displays a mixer interface for all M1-Panner instances.
    
    Features:
    - Shows each panner as a channel strip with meters and gain control
    - Real-time level meters for each panner
    - Gain faders for adjustment
    - Mini panner position indicator
    - Visual feedback for M1MemoryShare vs OSC tracking
    - Horizontal scrolling when many panners exist
    
    Channel Strip Elements:
    - Channel number
    - Panner name (truncated)
    - Level meter (stereo or mono based on channels)
    - Gain fader
    - dB readout
    - Mute/Solo buttons (placeholder for future)
*/

#pragma once

#include <JuceHeader.h>
#include "../../Managers/PannerTrackingManager.h"
#include <vector>
#include <memory>

namespace Mach1 {

/**
 * Individual channel strip component for a single panner
 */
class PannerChannelStrip : public juce::Component
{
public:
    explicit PannerChannelStrip(int channelIndex);
    ~PannerChannelStrip() override;
    
    // Data updates
    void updatePannerData(const PannerInfo& panner);
    void setLevelMeter(float levelL, float levelR = -1.0f);  // -1 means mono
    
    // Selection state
    bool isSelected = false;
    
    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    // Interaction
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
    
    // Callbacks
    std::function<void(int, float)> onGainChanged;
    std::function<void(int)> onChannelSelected;
    
private:
    // Data
    int channelIndex;
    float gainValue = 0.0f;  // in dB
    float meterLevelL = 0.0f;
    float meterLevelR = 0.0f;
    bool isStereo = false;
    PannerInfo currentPanner;
    
    // Drawing helpers
    void drawLevelMeter(juce::Graphics& g, juce::Rectangle<float> bounds, float level);
    void drawGainFader(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawPannerIndicator(juce::Graphics& g, juce::Rectangle<float> bounds);
    
    // Layout bounds (calculated in resized)
    juce::Rectangle<float> meterBoundsL;
    juce::Rectangle<float> meterBoundsR;
    juce::Rectangle<float> faderBounds;
    juce::Rectangle<float> pannerIndicatorBounds;
    juce::Rectangle<float> labelBounds;
    juce::Rectangle<float> gainReadoutBounds;
    
    // Styling
    juce::Colour backgroundColour{0xFF2A2A2A};
    juce::Colour meterBackgroundColour{0xFF1A1A1A};
    juce::Colour meterGreenColour{0xFF4CAF50};
    juce::Colour meterYellowColour{0xFFFFC107};
    juce::Colour meterRedColour{0xFFF44336};
    juce::Colour faderTrackColour{0xFF404040};
    juce::Colour faderThumbColour{0xFFE0E0E0};
    juce::Colour textColour{0xFFE0E0E0};
    juce::Colour dimTextColour{0xFF909090};
    juce::Colour selectedColour{0xFFF5B942};
    juce::Colour memShareColour{0xFF4CAF50};
    juce::Colour oscColour{0xFFFF9800};
    
    // Interaction state
    bool isDraggingFader = false;
    float dragStartGain = 0.0f;
    int dragStartY = 0;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PannerChannelStrip)
};

/**
 * Main mixer component that contains all channel strips with horizontal scrolling
 */
class InputMixerComponent : public juce::Component,
                            public juce::ScrollBar::Listener
{
public:
    InputMixerComponent();
    ~InputMixerComponent() override;
    
    // Data updates
    void updatePannerData(const std::vector<PannerInfo>& panners);
    void setSelectedPanner(int index);
    void updateLevelMeters(const std::vector<float>& levels);
    
    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    // ScrollBar::Listener
    void scrollBarMoved(juce::ScrollBar* scrollBarThatHasMoved, double newRangeStart) override;
    
    // Callbacks
    std::function<void(int, float)> onGainChanged;
    std::function<void(int)> onChannelSelected;

private:
    // Data
    std::vector<PannerInfo> pannerData;
    int selectedPannerIndex = -1;
    
    // UI components
    std::vector<std::unique_ptr<PannerChannelStrip>> channelStrips;
    std::unique_ptr<juce::Viewport> viewport;
    std::unique_ptr<juce::Component> stripContainer;
    
    // Layout
    void updateChannelStrips();
    void updateStripContainer();
    
    static constexpr int CHANNEL_STRIP_WIDTH = 70;
    static constexpr int CHANNEL_STRIP_MIN_HEIGHT = 180;
    static constexpr int CHANNEL_SPACING = 2;
    static constexpr int HEADER_HEIGHT = 0;  // No header, container has toolbar
    
    // Styling
    juce::Colour backgroundColour{0xFF1A1A1A};
    juce::Colour separatorColour{0xFF404040};
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InputMixerComponent)
};

} // namespace Mach1
