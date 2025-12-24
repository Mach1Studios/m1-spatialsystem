/*
    InputMixerComponent.cpp
    -----------------------
    Implementation of the mixer interface component
*/

#include "InputMixerComponent.h"

namespace Mach1 {

//==============================================================================
// PannerChannelStrip Implementation
//==============================================================================

PannerChannelStrip::PannerChannelStrip(int channelIndex)
    : channelIndex(channelIndex)
{
    // Create channel label
    channelLabel = std::make_unique<juce::Label>("channel", juce::String(channelIndex + 1));
    channelLabel->setFont(juce::Font(12.0f, juce::Font::bold));
    channelLabel->setColour(juce::Label::textColourId, textColour);
    channelLabel->setJustificationType(juce::Justification::centred);
    addAndMakeVisible(channelLabel.get());
    
    // Create name label
    nameLabel = std::make_unique<juce::Label>("name", "---");
    nameLabel->setFont(juce::Font(10.0f));
    nameLabel->setColour(juce::Label::textColourId, textColour);
    nameLabel->setJustificationType(juce::Justification::centred);
    addAndMakeVisible(nameLabel.get());
}

PannerChannelStrip::~PannerChannelStrip()
{
}

//==============================================================================
void PannerChannelStrip::updatePannerData(const PannerInfo& panner)
{
    currentPanner = panner;
    
    // Update gain value
    gainValue = panner.gain;
    
    // Update name label
    nameLabel->setText(panner.name.substr(0, 8), juce::dontSendNotification);
    
    repaint();
}

void PannerChannelStrip::setLevelMeter(float level)
{
    meterLevel = level;
    repaint();
}

//==============================================================================
void PannerChannelStrip::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    
    // Background
    g.setColour(isSelected ? selectedColour.withAlpha(0.3f) : backgroundColour);
    g.fillRect(bounds);
    
    // Border
    g.setColour(isSelected ? selectedColour : backgroundColour.brighter(0.2f));
    g.drawRect(bounds, 1);
    
    // Calculate layout
    auto labelArea = bounds.removeFromBottom(30);
    auto gainArea = bounds.removeFromBottom(40);
    auto meterArea = bounds;
    
    // Draw level meter
    drawLevelMeter(g, meterArea);
    
    // Draw gain knob
    drawGainKnob(g, gainArea);
}

void PannerChannelStrip::resized()
{
    auto bounds = getLocalBounds();
    
    // Channel label at top
    channelLabel->setBounds(bounds.removeFromTop(15));
    
    // Name label at bottom
    nameLabel->setBounds(bounds.removeFromBottom(15));
    
    // Calculate areas for meter and gain knob
    auto gainArea = bounds.removeFromBottom(40);
    meterBounds = bounds.reduced(10);
    gainKnobBounds = gainArea.reduced(10);
}

//==============================================================================
void PannerChannelStrip::drawLevelMeter(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Meter background
    g.setColour(juce::Colours::black);
    g.fillRect(bounds);
    
    // Meter border
    g.setColour(juce::Colours::grey);
    g.drawRect(bounds, 1);
    
    // Level bar
    if (meterLevel > 0.0f)
    {
        int levelHeight = static_cast<int>(bounds.getHeight() * meterLevel);
        auto levelRect = bounds.removeFromBottom(levelHeight);
        
        // Color based on level
        juce::Colour levelColour = meterColour;
        if (meterLevel > 0.8f)
            levelColour = juce::Colours::red;
        else if (meterLevel > 0.6f)
            levelColour = juce::Colours::yellow;
        
        g.setColour(levelColour);
        g.fillRect(levelRect);
    }
}

void PannerChannelStrip::drawGainKnob(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    auto centre = bounds.getCentre();
    float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f - 2.0f;
    
    // Knob background
    g.setColour(knobColour);
    g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2, radius * 2);
    
    // Knob border
    g.setColour(knobColour.brighter(0.3f));
    g.drawEllipse(centre.x - radius, centre.y - radius, radius * 2, radius * 2, 1);
    
    // Gain indicator line
    float gainNormalized = (gainValue + 40.0f) / 80.0f;  // -40dB to +40dB range
    float angle = (gainNormalized * 300.0f - 150.0f) * M_PI / 180.0f;
    
    g.setColour(textColour);
    g.drawLine(centre.x, centre.y,
               centre.x + radius * 0.8f * cos(angle),
               centre.y + radius * 0.8f * sin(angle), 2);
}

//==============================================================================
void PannerChannelStrip::mouseDown(const juce::MouseEvent& event)
{
    if (gainKnobBounds.contains(event.getPosition()))
    {
        isDragging = true;
        lastMousePosition = event.getPosition();
    }
    
    if (onChannelSelected)
        onChannelSelected(channelIndex);
}

void PannerChannelStrip::mouseDrag(const juce::MouseEvent& event)
{
    if (isDragging)
    {
        auto delta = event.getPosition() - lastMousePosition;
        float gainChange = -delta.y * 0.1f;  // Negative for natural drag direction
        
        gainValue = juce::jlimit(-40.0f, 40.0f, gainValue + gainChange);
        
        if (onGainChanged)
            onGainChanged(channelIndex, gainValue);
        
        lastMousePosition = event.getPosition();
        repaint();
    }
}

void PannerChannelStrip::mouseUp(const juce::MouseEvent& event)
{
    isDragging = false;
}

//==============================================================================
// InputMixerComponent Implementation
//==============================================================================

InputMixerComponent::InputMixerComponent()
{
    // Create title label
    titleLabel = std::make_unique<juce::Label>("title", "Input Mixer");
    titleLabel->setFont(juce::Font(16.0f, juce::Font::bold));
    titleLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    titleLabel->setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel.get());
}

InputMixerComponent::~InputMixerComponent()
{
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
    for (int i = 0; i < channelStrips.size(); ++i)
    {
        if (auto* strip = channelStrips[i].get())
        {
            strip->isSelected = (i == index);
            strip->repaint();
        }
    }
}

void InputMixerComponent::updateLevelMeters(const std::vector<float>& levels)
{
    levelData = levels;
    
    // Update level meters
    for (int i = 0; i < channelStrips.size() && i < levels.size(); ++i)
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
    
    // Draw separators between channel strips
    g.setColour(separatorColour);
    for (int i = 1; i < channelStrips.size(); ++i)
    {
        int x = i * (CHANNEL_STRIP_WIDTH + CHANNEL_SPACING) - CHANNEL_SPACING / 2;
        g.drawLine(x, 30, x, getHeight() - 10, 1);
    }
}

void InputMixerComponent::resized()
{
    auto bounds = getLocalBounds();
    
    // Title at top
    titleLabel->setBounds(bounds.removeFromTop(25));
    
    // Channel strips
    bounds = bounds.reduced(5);
    
    for (int i = 0; i < channelStrips.size(); ++i)
    {
        if (auto* strip = channelStrips[i].get())
        {
            int x = i * (CHANNEL_STRIP_WIDTH + CHANNEL_SPACING);
            strip->setBounds(x, 0, CHANNEL_STRIP_WIDTH, bounds.getHeight());
        }
    }
}

//==============================================================================
void InputMixerComponent::updateChannelStrips()
{
    // Remove existing strips
    for (auto& strip : channelStrips)
    {
        removeChildComponent(strip.get());
    }
    channelStrips.clear();
    
    // Create new strips for each panner
    for (int i = 0; i < pannerData.size(); ++i)
    {
        auto strip = std::make_unique<PannerChannelStrip>(i);
        strip->updatePannerData(pannerData[i]);
        
        // Set up callbacks
        strip->onGainChanged = [this](int index, float gain) {
            if (onGainChanged)
                onGainChanged(index, gain);
        };
        
        strip->onChannelSelected = [this](int index) {
            if (onChannelSelected)
                onChannelSelected(index);
        };
        
        addAndMakeVisible(strip.get());
        channelStrips.push_back(std::move(strip));
    }
    
    resized();
}

int InputMixerComponent::getChannelStripWidth() const
{
    return CHANNEL_STRIP_WIDTH;
}

} // namespace Mach1 