/*
    InputMixerComponent.cpp
    -----------------------
    Implementation of the mixer interface component with channel strips
*/

#include "InputMixerComponent.h"

namespace Mach1 {

//==============================================================================
// PannerChannelStrip Implementation
//==============================================================================

PannerChannelStrip::PannerChannelStrip(int channelIndex)
    : channelIndex(channelIndex)
{
}

PannerChannelStrip::~PannerChannelStrip()
{
}

//==============================================================================
void PannerChannelStrip::updatePannerData(const PannerInfo& panner)
{
    currentPanner = panner;
    gainValue = panner.gain;
    isStereo = (panner.channels >= 2);
    repaint();
}

void PannerChannelStrip::setLevelMeter(float levelL, float levelR)
{
    meterLevelL = juce::jlimit(0.0f, 1.0f, levelL);
    meterLevelR = (levelR >= 0.0f) ? juce::jlimit(0.0f, 1.0f, levelR) : meterLevelL;
    repaint();
}

//==============================================================================
void PannerChannelStrip::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    
    // Background
    g.setColour(isSelected ? selectedColour.withAlpha(0.15f) : backgroundColour);
    g.fillRect(bounds);
    
    // Selection border
    if (isSelected)
    {
        g.setColour(selectedColour);
        g.drawRect(bounds, 2.0f);
    }
    
    // Channel number at top
    g.setColour(isSelected ? selectedColour : textColour);
    g.setFont(juce::Font(11.0f, juce::Font::bold));
    auto topArea = bounds.removeFromTop(18.0f);
    g.drawText(juce::String(channelIndex + 1), topArea, juce::Justification::centred);
    
    // Tracking indicator (small dot next to channel number)
    juce::Colour indicatorColour = currentPanner.isMemoryShareBased ? memShareColour : oscColour;
    g.setColour(indicatorColour);
    g.fillEllipse(topArea.getRight() - 10.0f, topArea.getCentreY() - 3.0f, 6.0f, 6.0f);
    
    // Name label
    auto nameArea = bounds.removeFromTop(14.0f);
    g.setColour(dimTextColour);
    g.setFont(juce::Font(9.0f));
    juce::String displayName = currentPanner.name.empty() ? "---" : juce::String(currentPanner.name).substring(0, 8);
    g.drawText(displayName, nameArea, juce::Justification::centred, true);
    
    // Bottom area for gain readout
    auto gainReadout = bounds.removeFromBottom(16.0f);
    g.setColour(textColour);
    g.setFont(juce::Font(10.0f));
    juce::String gainText = juce::String(gainValue, 1) + " dB";
    g.drawText(gainText, gainReadout, juce::Justification::centred);
    
    // Small panner indicator at bottom
    auto pannerArea = bounds.removeFromBottom(30.0f).reduced(8.0f, 4.0f);
    drawPannerIndicator(g, pannerArea);
    
    // Remaining area for meters and fader
    bounds = bounds.reduced(4.0f, 4.0f);
    
    // Calculate meter and fader layout
    float meterWidth = isStereo ? 8.0f : 12.0f;
    float faderWidth = 16.0f;
    float spacing = 4.0f;
    float totalMeterWidth = isStereo ? (meterWidth * 2 + 2.0f) : meterWidth;
    float totalWidth = totalMeterWidth + spacing + faderWidth;
    float startX = bounds.getCentreX() - totalWidth / 2.0f;
    
    // Draw meters
    if (isStereo)
    {
        auto meterL = juce::Rectangle<float>(startX, bounds.getY(), meterWidth, bounds.getHeight());
        auto meterR = juce::Rectangle<float>(startX + meterWidth + 2.0f, bounds.getY(), meterWidth, bounds.getHeight());
        drawLevelMeter(g, meterL, meterLevelL);
        drawLevelMeter(g, meterR, meterLevelR);
    }
    else
    {
        auto meterMono = juce::Rectangle<float>(startX, bounds.getY(), meterWidth, bounds.getHeight());
        drawLevelMeter(g, meterMono, meterLevelL);
    }
    
    // Draw fader
    faderBounds = juce::Rectangle<float>(startX + totalMeterWidth + spacing, bounds.getY(), faderWidth, bounds.getHeight());
    drawGainFader(g, faderBounds);
}

void PannerChannelStrip::resized()
{
    // Layout is calculated dynamically in paint()
}

//==============================================================================
void PannerChannelStrip::drawLevelMeter(juce::Graphics& g, juce::Rectangle<float> bounds, float level)
{
    // Meter background
    g.setColour(meterBackgroundColour);
    g.fillRect(bounds);
    
    // Meter border
    g.setColour(backgroundColour.brighter(0.3f));
    g.drawRect(bounds, 1.0f);
    
    if (level > 0.0f)
    {
        // Calculate level height
        float levelHeight = bounds.getHeight() * level;
        auto levelRect = bounds.removeFromBottom(levelHeight);
        
        // Gradient based on level
        if (level > 0.9f)
        {
            g.setColour(meterRedColour);
        }
        else if (level > 0.7f)
        {
            // Gradient from yellow to red
            auto yellowRect = levelRect.removeFromBottom(levelRect.getHeight() * 0.7f / level);
            g.setColour(meterYellowColour);
            g.fillRect(yellowRect);
            
            auto redRect = levelRect;
            g.setColour(meterRedColour.interpolatedWith(meterYellowColour, 0.5f));
            g.fillRect(redRect);
            return;
        }
        else if (level > 0.5f)
        {
            g.setColour(meterYellowColour.interpolatedWith(meterGreenColour, (0.7f - level) / 0.2f));
        }
        else
        {
            g.setColour(meterGreenColour);
        }
        
        g.fillRect(levelRect);
    }
    
    // Draw scale marks
    g.setColour(backgroundColour.brighter(0.2f));
    for (float db : {-6.0f, -12.0f, -24.0f, -36.0f})
    {
        float normalized = (db + 60.0f) / 60.0f;  // Assuming -60dB to 0dB range
        float y = bounds.getBottom() - bounds.getHeight() * normalized;
        g.drawHorizontalLine(static_cast<int>(y), bounds.getX(), bounds.getRight());
    }
}

void PannerChannelStrip::drawGainFader(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    // Fader track
    auto trackBounds = bounds.reduced(bounds.getWidth() * 0.3f, 0);
    g.setColour(faderTrackColour);
    g.fillRoundedRectangle(trackBounds, 2.0f);
    
    // Calculate thumb position (-60dB to +12dB range)
    float normalizedGain = (gainValue + 60.0f) / 72.0f;
    normalizedGain = juce::jlimit(0.0f, 1.0f, normalizedGain);
    float thumbY = bounds.getBottom() - (bounds.getHeight() - 20.0f) * normalizedGain - 10.0f;
    
    // Unity gain marker (0dB)
    float unityY = bounds.getBottom() - (bounds.getHeight() - 20.0f) * (60.0f / 72.0f) - 10.0f;
    g.setColour(textColour.withAlpha(0.5f));
    g.drawHorizontalLine(static_cast<int>(unityY), bounds.getX() - 2.0f, bounds.getRight() + 2.0f);
    
    // Fader thumb
    auto thumbBounds = juce::Rectangle<float>(bounds.getX(), thumbY - 8.0f, bounds.getWidth(), 16.0f);
    g.setColour(faderThumbColour);
    g.fillRoundedRectangle(thumbBounds, 3.0f);
    
    // Thumb groove
    g.setColour(faderTrackColour);
    g.drawHorizontalLine(static_cast<int>(thumbY), thumbBounds.getX() + 3.0f, thumbBounds.getRight() - 3.0f);
}

void PannerChannelStrip::drawPannerIndicator(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    // Background circle
    g.setColour(meterBackgroundColour);
    float diameter = juce::jmin(bounds.getWidth(), bounds.getHeight());
    auto circleBounds = bounds.withSizeKeepingCentre(diameter, diameter);
    g.fillEllipse(circleBounds);
    
    // Border
    g.setColour(faderTrackColour);
    g.drawEllipse(circleBounds, 1.0f);
    
    // Cross lines
    g.setColour(faderTrackColour.withAlpha(0.5f));
    g.drawLine(circleBounds.getCentreX(), circleBounds.getY(),
               circleBounds.getCentreX(), circleBounds.getBottom(), 0.5f);
    g.drawLine(circleBounds.getX(), circleBounds.getCentreY(),
               circleBounds.getRight(), circleBounds.getCentreY(), 0.5f);
    
    // Panner position dot
    // Convert azimuth/elevation to 2D position (top-down view)
    float azRad = currentPanner.azimuth * juce::MathConstants<float>::pi / 180.0f;
    float radius = diameter / 2.0f - 4.0f;
    
    // In top-down view: X = sin(az), Y = -cos(az) (front is up)
    float dotX = circleBounds.getCentreX() + radius * std::sin(azRad);
    float dotY = circleBounds.getCentreY() - radius * std::cos(azRad);
    
    g.setColour(selectedColour);
    g.fillEllipse(dotX - 3.0f, dotY - 3.0f, 6.0f, 6.0f);
}

//==============================================================================
void PannerChannelStrip::mouseDown(const juce::MouseEvent& event)
{
    // Check if clicking on fader
    if (faderBounds.contains(event.position))
    {
        isDraggingFader = true;
        dragStartGain = gainValue;
        dragStartY = event.y;
    }
    
    // Always notify selection
    if (onChannelSelected)
        onChannelSelected(channelIndex);
}

void PannerChannelStrip::mouseDrag(const juce::MouseEvent& event)
{
    if (isDraggingFader)
    {
        // Calculate gain change based on Y delta
        float deltaY = static_cast<float>(dragStartY - event.y);
        float gainRange = 72.0f;  // -60 to +12 dB
        float gainChange = (deltaY / faderBounds.getHeight()) * gainRange;
        
        float newGain = juce::jlimit(-60.0f, 12.0f, dragStartGain + gainChange);
        
        if (std::abs(newGain - gainValue) > 0.1f)
        {
            gainValue = newGain;
            
            if (onGainChanged)
                onGainChanged(channelIndex, gainValue);
            
            repaint();
        }
    }
}

void PannerChannelStrip::mouseUp(const juce::MouseEvent& event)
{
    isDraggingFader = false;
}

void PannerChannelStrip::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    // Fine-tune gain with mouse wheel
    float gainChange = wheel.deltaY * 3.0f;  // 3dB per wheel notch
    float newGain = juce::jlimit(-60.0f, 12.0f, gainValue + gainChange);
    
    if (std::abs(newGain - gainValue) > 0.1f)
    {
        gainValue = newGain;
        
        if (onGainChanged)
            onGainChanged(channelIndex, gainValue);
        
        repaint();
    }
}

//==============================================================================
// InputMixerComponent Implementation
//==============================================================================

InputMixerComponent::InputMixerComponent()
{
    // Create viewport for horizontal scrolling
    stripContainer = std::make_unique<juce::Component>();
    
    viewport = std::make_unique<juce::Viewport>();
    viewport->setViewedComponent(stripContainer.get(), false);
    viewport->setScrollBarsShown(false, true);  // Only horizontal scrollbar
    viewport->getHorizontalScrollBar().setColour(juce::ScrollBar::thumbColourId, juce::Colour(0xFF606060));
    viewport->getHorizontalScrollBar().setColour(juce::ScrollBar::trackColourId, juce::Colour(0xFF303030));
    addAndMakeVisible(viewport.get());
}

InputMixerComponent::~InputMixerComponent()
{
    // Clear channel strips before viewport is destroyed
    channelStrips.clear();
}

//==============================================================================
void InputMixerComponent::updatePannerData(const std::vector<PannerInfo>& panners)
{
    pannerData = panners;
    updateChannelStrips();
}

void InputMixerComponent::setSelectedPanner(int index)
{
    selectedPannerIndex = index;
    
    // Update selection state on all channel strips
    for (size_t i = 0; i < channelStrips.size(); ++i)
    {
        if (auto* strip = channelStrips[i].get())
        {
            strip->isSelected = (static_cast<int>(i) == index);
            strip->repaint();
        }
    }
    
    // Scroll to show selected channel
    if (index >= 0 && index < static_cast<int>(channelStrips.size()))
    {
        auto* strip = channelStrips[static_cast<size_t>(index)].get();
        if (strip)
        {
            viewport->setViewPosition(strip->getX() - 20, 0);
        }
    }
}

void InputMixerComponent::updateLevelMeters(const std::vector<float>& levels)
{
    // Update level meters (assumes mono levels, could extend to stereo)
    for (size_t i = 0; i < channelStrips.size() && i < levels.size(); ++i)
    {
        if (auto* strip = channelStrips[i].get())
        {
            strip->setLevelMeter(levels[i]);
        }
    }
}

//==============================================================================
void InputMixerComponent::paint(juce::Graphics& g)
{
    g.fillAll(backgroundColour);
    
    // Draw message if no panners
    if (pannerData.empty())
    {
        g.setColour(juce::Colour(0xFF606060));
        g.setFont(juce::Font(12.0f));
        g.drawText("No panners connected", getLocalBounds(), juce::Justification::centred);
    }
}

void InputMixerComponent::resized()
{
    auto bounds = getLocalBounds();
    
    // Viewport takes full area
    viewport->setBounds(bounds);
    
    // Update strip container size and layout
    updateStripContainer();
}

void InputMixerComponent::scrollBarMoved(juce::ScrollBar* scrollBarThatHasMoved, double newRangeStart)
{
    // Handle scroll if needed
}

//==============================================================================
void InputMixerComponent::updateChannelStrips()
{
    // Remove existing strips
    for (auto& strip : channelStrips)
    {
        stripContainer->removeChildComponent(strip.get());
    }
    channelStrips.clear();
    
    // Create new strips for each panner
    for (size_t i = 0; i < pannerData.size(); ++i)
    {
        auto strip = std::make_unique<PannerChannelStrip>(static_cast<int>(i));
        strip->updatePannerData(pannerData[i]);
        strip->isSelected = (static_cast<int>(i) == selectedPannerIndex);
        
        // Set up callbacks
        strip->onGainChanged = [this](int index, float gain) {
            if (onGainChanged)
                onGainChanged(index, gain);
        };
        
        strip->onChannelSelected = [this](int index) {
            setSelectedPanner(index);
            if (onChannelSelected)
                onChannelSelected(index);
        };
        
        stripContainer->addAndMakeVisible(strip.get());
        channelStrips.push_back(std::move(strip));
    }
    
    updateStripContainer();
}

void InputMixerComponent::updateStripContainer()
{
    if (channelStrips.empty())
    {
        stripContainer->setSize(viewport->getWidth(), viewport->getHeight());
        return;
    }
    
    // Calculate container size
    int totalWidth = static_cast<int>(channelStrips.size()) * (CHANNEL_STRIP_WIDTH + CHANNEL_SPACING);
    int containerHeight = viewport->getHeight() - (viewport->isHorizontalScrollBarShown() ? 10 : 0);
    
    stripContainer->setSize(juce::jmax(totalWidth, viewport->getWidth()), containerHeight);
    
    // Layout channel strips
    int x = CHANNEL_SPACING;
    for (auto& strip : channelStrips)
    {
        strip->setBounds(x, 0, CHANNEL_STRIP_WIDTH, containerHeight);
        x += CHANNEL_STRIP_WIDTH + CHANNEL_SPACING;
    }
}

} // namespace Mach1
