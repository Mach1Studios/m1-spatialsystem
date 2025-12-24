/*
    InputMixerComponent.h
    ---------------------
    UI component that displays a mixer interface for all M1-Panner instances.
    
    Features:
    - Shows each panner as a channel strip with meters and gain control
    - Real-time level meters for each panner
    - Gain knobs for adjustment
    - Mini panner reticle grid (future feature)
    - Visual feedback for M1MemoryShare vs OSC tracking
    
    Channel Strip Elements:
    - Panner name
    - Panner Input Meter(s) [one per channel]
    - Panner Output Gain Slider [controls Panner's Gain knob]
    - Panner Mini Reticle indicator (future feature) [controls Panner's azimuth/diverge]
    - Panner Mute/Solo buttons (for locally muting or soloing via the ExternalMixerProcessor)
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
    void setLevelMeter(float level);
    void setGainValue(float gain);
    
    // Make public so main component can access it
    bool isSelected = false;
    
    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    // Interaction
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    
    // Callbacks
    std::function<void(int, float)> onGainChanged;
    std::function<void(int)> onChannelSelected;
    
private:
    // Data
    int channelIndex;
    float gainValue = 0.5f;
    float meterLevel = 0.0f;
    PannerInfo currentPanner;
    
    // UI elements
    std::unique_ptr<juce::Label> channelLabel;
    std::unique_ptr<juce::Label> nameLabel;
    
    // Meter drawing
    void drawLevelMeter(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawGainKnob(juce::Graphics& g, juce::Rectangle<int> bounds);
    
    // Layout
    juce::Rectangle<int> meterBounds;
    juce::Rectangle<int> gainKnobBounds;
    juce::Rectangle<int> labelBounds;
    
    // Styling
    juce::Colour backgroundColour{0xFF2A2A2A};
    juce::Colour meterColour{0xFF4CAF50};
    juce::Colour knobColour{0xFF666666};
    juce::Colour textColour{0xFFE0E0E0};
    juce::Colour selectedColour{0xFF0078D4};
    
    // Interaction state
    bool isDragging = false;
    juce::Point<int> lastMousePosition;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PannerChannelStrip)
};

/**
 * Main mixer component that contains all channel strips
 */
class InputMixerComponent : public juce::Component
{
public:
    explicit InputMixerComponent();
    ~InputMixerComponent() override;
    
    // Data updates
    void updatePannerData(const std::vector<PannerInfo>& panners);
    void setSelectedPanner(int index);
    void updateLevelMeters(const std::vector<float>& levels);
    
    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    // Callbacks
    std::function<void(int, float)> onGainChanged;
    std::function<void(int)> onChannelSelected;

private:
    // Data
    std::vector<PannerInfo> pannerData;
    std::vector<float> levelData;
    int selectedPannerIndex = -1;
    
    // UI components
    std::vector<std::unique_ptr<PannerChannelStrip>> channelStrips;
    std::unique_ptr<juce::Label> titleLabel;
    
    // Layout
    void updateChannelStrips();
    int getChannelStripWidth() const;
    static constexpr int CHANNEL_STRIP_WIDTH = 80;
    static constexpr int CHANNEL_STRIP_HEIGHT = 200;
    static constexpr int CHANNEL_SPACING = 4;
    
    // Styling
    juce::Colour backgroundColour{0xFF1A1A1A};
    juce::Colour separatorColour{0xFF404040};
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InputMixerComponent)
};

} // namespace Mach1 
