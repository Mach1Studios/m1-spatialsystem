/*
    M1SpatialOutComponent.h
    -----------------------
    UI component that displays output meters and monitor controls for the selected Mach1 Spatial format.
    
    Features:
    - Output level meters for the combined multichannel signal of all M1-Panner instances via the ExternalMixerProcessor
    - Monitor yaw/pitch/roll controls for the multichannel input -> stereo output via Mach1Decode obj in ExternalMixerProcessor
    - Format selection display for Mach1Spatial configuration
    - Real-time level and loudness meters (future feature)
    
    Display Elements:
    - Output level meters (number of channels based on selected Mach1Spatial config)
    - Monitor orientation controls (yaw, pitch, roll)
    - Format indicator (shows current Mach1 spatial format)
    - Master output level and loudness meters for post-Mach1Decode processing
*/

#pragma once

#include <JuceHeader.h>
#include "../../Managers/PannerTrackingManager.h"
#include <vector>
#include <memory>

namespace Mach1 {

/**
 * Individual output meter for a spatial channel
 */
class OutputMeter : public juce::Component
{
public:
    explicit OutputMeter(int channelIndex);
    ~OutputMeter() override;
    
    void setLevel(float level);
    void paint(juce::Graphics& g) override;
    void resized() override;
    
private:
    int channelIndex;
    float currentLevel = 0.0f;
    float peakLevel = 0.0f;
    
    // Styling
    juce::Colour backgroundColour{0xFF2A2A2A};
    juce::Colour levelColour{0xFF4CAF50};
    juce::Colour peakColour{0xFFFFFF00};
    juce::Colour clipColour{0xFFFF0000};
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OutputMeter)
};

/**
 * Monitor orientation control component
 */
class MonitorOrientationControl : public juce::Component
{
public:
    explicit MonitorOrientationControl();
    ~MonitorOrientationControl() override;
    
    // Orientation control
    void setYawPitchRoll(float yaw, float pitch, float roll);
    void getYawPitchRoll(float& yaw, float& pitch, float& roll) const;
    
    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    
    // Callbacks
    std::function<void(float, float, float)> onOrientationChanged;
    
private:
    // Orientation values
    float currentYaw = 0.0f;
    float currentPitch = 0.0f;
    float currentRoll = 0.0f;
    
    // UI elements
    std::unique_ptr<juce::Label> yawLabel;
    std::unique_ptr<juce::Label> pitchLabel;
    std::unique_ptr<juce::Label> rollLabel;
    
    // Interaction
    enum DragMode { NoDrag, YawDrag, PitchDrag, RollDrag };
    DragMode currentDragMode = NoDrag;
    juce::Point<int> dragStartPosition;
    float dragStartValue = 0.0f;
    
    // Drawing
    void drawOrientationControl(juce::Graphics& g, juce::Rectangle<int> bounds, float value, const juce::String& label);
    DragMode getDragModeForPosition(juce::Point<int> position);
    
    // Layout
    juce::Rectangle<int> yawBounds;
    juce::Rectangle<int> pitchBounds;
    juce::Rectangle<int> rollBounds;
    
    // Styling
    juce::Colour backgroundColour{0xFF2A2A2A};
    juce::Colour controlColour{0xFF666666};
    juce::Colour activeControlColour{0xFF0078D4};
    juce::Colour textColour{0xFFE0E0E0};
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MonitorOrientationControl)
};

/**
 * Main M1 Spatial Out component
 */
class M1SpatialOutComponent : public juce::Component
{
public:
    explicit M1SpatialOutComponent();
    ~M1SpatialOutComponent() override;
    
    // Data updates
    void updateOutputLevels(const std::vector<float>& levels);
    void setMonitorOrientation(float yaw, float pitch, float roll);
    void setSpatialFormat(const juce::String& format);
    
    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    // Callbacks
    std::function<void(float, float, float)> onMonitorOrientationChanged;
    std::function<void(const juce::String&)> onSpatialFormatChanged;

private:
    // Data
    std::vector<float> outputLevels;
    juce::String currentSpatialFormat = "Mach1 Spatial";
    
    // UI components
    std::vector<std::unique_ptr<OutputMeter>> outputMeters;
    std::unique_ptr<MonitorOrientationControl> orientationControl;
    std::unique_ptr<juce::Label> titleLabel;
    std::unique_ptr<juce::Label> formatLabel;
    
    // Layout
    void updateOutputMeters();
    int getOutputMeterCount() const;
    
    // Layout bounds
    juce::Rectangle<int> metersBounds;
    juce::Rectangle<int> orientationBounds;
    juce::Rectangle<int> titleBounds;
    juce::Rectangle<int> formatBounds;
    
    // Configuration
    static constexpr int OUTPUT_METER_WIDTH = 20;
    static constexpr int OUTPUT_METER_HEIGHT = 200;
    static constexpr int METER_SPACING = 4;
    static constexpr int ORIENTATION_CONTROL_HEIGHT = 100;
    static constexpr int DEFAULT_OUTPUT_CHANNELS = 8;  // Default M1 Spatial format
    
    // Styling
    juce::Colour backgroundColour{0xFF1A1A1A};
    juce::Colour separatorColour{0xFF404040};
    juce::Colour textColour{0xFFE0E0E0};
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(M1SpatialOutComponent)
};

} // namespace Mach1 
