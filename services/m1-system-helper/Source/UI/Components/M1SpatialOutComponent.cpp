#include "M1SpatialOutComponent.h"

namespace Mach1 {

//==============================================================================
// OutputMeter Implementation
//==============================================================================

OutputMeter::OutputMeter(int channelIndex) : channelIndex(channelIndex) {
    setSize(20, 200); // Default meter size
}

OutputMeter::~OutputMeter() {
}

void OutputMeter::setLevel(float level) {
    currentLevel = juce::jlimit(0.0f, 1.0f, level);
    peakLevel = juce::jmax(peakLevel * 0.99f, currentLevel); // Slow decay for peak
    repaint();
}

void OutputMeter::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();
    
    // Background
    g.setColour(backgroundColour);
    g.fillRect(bounds);
    
    // Level meter
    if (currentLevel > 0.0f) {
        auto levelHeight = bounds.getHeight() * currentLevel;
        auto levelBounds = juce::Rectangle<float>(bounds.getX(), bounds.getBottom() - levelHeight, bounds.getWidth(), levelHeight);
        
        // Choose color based on level
        if (currentLevel > 0.9f) {
            g.setColour(clipColour);
        } else if (currentLevel > 0.7f) {
            g.setColour(peakColour);
        } else {
            g.setColour(levelColour);
        }
        
        g.fillRect(levelBounds);
    }
    
    // Peak indicator
    if (peakLevel > 0.0f) {
        auto peakY = bounds.getBottom() - (bounds.getHeight() * peakLevel);
        g.setColour(peakColour);
        g.drawHorizontalLine(static_cast<int>(peakY), bounds.getX(), bounds.getRight());
    }
    
    // Border
    g.setColour(juce::Colours::black);
    g.drawRect(bounds, 1.0f);
}

void OutputMeter::resized() {
    // Nothing special needed for resize
}

//==============================================================================
// M1SpatialOutComponent Implementation  
//==============================================================================

M1SpatialOutComponent::M1SpatialOutComponent() {
    // Basic constructor - just set a default size
    setSize(400, 300);
}

M1SpatialOutComponent::~M1SpatialOutComponent() {
    // Basic destructor
}

void M1SpatialOutComponent::paint(juce::Graphics& g) {
    // Fill the background
    g.fillAll(backgroundColour);
    
    // Draw title
    g.setColour(textColour);
    g.setFont(16.0f);
    g.drawText("M1 Spatial Output", titleBounds, juce::Justification::centred);
    
    // Draw format label
    g.setFont(12.0f);
    g.drawText(currentSpatialFormat, formatBounds, juce::Justification::centred);
    
    // Draw separator lines
    g.setColour(separatorColour);
    g.drawHorizontalLine(titleBounds.getBottom() + 5, 0, getWidth());
    g.drawHorizontalLine(metersBounds.getBottom() + 5, 0, getWidth());
}

void M1SpatialOutComponent::resized() {
    auto bounds = getLocalBounds();
    const int margin = 10;
    
    bounds.reduce(margin, margin);
    
    // Title area
    titleBounds = bounds.removeFromTop(30);
    bounds.removeFromTop(5);  // spacing
    
    // Format label
    formatBounds = bounds.removeFromTop(20);
    bounds.removeFromTop(10);  // spacing
    
    // Output meters area
    const int meterAreaHeight = OUTPUT_METER_HEIGHT;
    metersBounds = bounds.removeFromTop(meterAreaHeight);
    bounds.removeFromTop(10);  // spacing
    
    // Orientation control area
    orientationBounds = bounds.removeFromTop(ORIENTATION_CONTROL_HEIGHT);
    
    // Update meter positions if they exist
    updateOutputMeters();
}

void M1SpatialOutComponent::updateOutputLevels(const std::vector<float>& levels) {
    outputLevels = levels;
    
    // Update output meters if they exist
    for (size_t i = 0; i < outputMeters.size() && i < levels.size(); ++i) {
        if (outputMeters[i]) {
            outputMeters[i]->setLevel(levels[i]);
        }
    }
}

void M1SpatialOutComponent::setMonitorOrientation(float yaw, float pitch, float roll) {
    if (orientationControl) {
        
        // TODO: fix
        //orientationControl->setOrientation(yaw, pitch, roll);
    }
}

void M1SpatialOutComponent::setSpatialFormat(const juce::String& format) {
    currentSpatialFormat = format;
    repaint();
}

void M1SpatialOutComponent::updateOutputMeters() {
    if (metersBounds.isEmpty()) return;
    
    const int meterCount = getOutputMeterCount();
    const int totalMeterWidth = meterCount * OUTPUT_METER_WIDTH;
    const int totalSpacing = (meterCount - 1) * METER_SPACING;
    const int totalWidth = totalMeterWidth + totalSpacing;
    
    int startX = metersBounds.getX() + (metersBounds.getWidth() - totalWidth) / 2;
    
    for (int i = 0; i < meterCount; ++i) {
        if (i < outputMeters.size() && outputMeters[i]) {
            const int x = startX + i * (OUTPUT_METER_WIDTH + METER_SPACING);
            outputMeters[i]->setBounds(x, metersBounds.getY(), OUTPUT_METER_WIDTH, metersBounds.getHeight());
        }
    }
}

int M1SpatialOutComponent::getOutputMeterCount() const {
    // Return the number of output channels based on current format
    // For now, default to 8 channels for Mach1 Spatial
    return DEFAULT_OUTPUT_CHANNELS;
}

} // namespace Mach1 
