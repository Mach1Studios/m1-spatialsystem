/*
    MonitorPlaceholderPanel.cpp
    ---------------------------
    Implementation of the monitor placeholder panel.
*/

#include "MonitorPanel.h"

namespace Mach1 {

//==============================================================================
MonitorPanel::MonitorPanel()
{
    // Title label (hidden, we draw it in paint)
    titleLabel = std::make_unique<juce::Label>("title", "");
    titleLabel->setVisible(false);
    addChildComponent(titleLabel.get());
    
    // Status label
    statusLabel = std::make_unique<juce::Label>("status", 
        "Monitoring UI placeholder");
    statusLabel->setFont(juce::Font(10.0f));
    statusLabel->setColour(juce::Label::textColourId, dimTextColour);
    statusLabel->setJustificationType(juce::Justification::centred);
    addAndMakeVisible(statusLabel.get());
}

MonitorPanel::~MonitorPanel()
{
}

//==============================================================================
void MonitorPanel::paint(juce::Graphics& g)
{
    // Fill background
    g.fillAll(backgroundColour);
    
    // Draw toolbar area
    auto bounds = getLocalBounds();
    auto toolbarBounds = bounds.removeFromTop(TOOLBAR_HEIGHT);
    
    g.setColour(toolbarColour);
    g.fillRect(toolbarBounds);
    
    // Draw section title
    g.setColour(juce::Colour(0xFF808080));
    g.setFont(juce::Font(11.0f, juce::Font::bold));
    g.drawText("M1 SPATIAL OUT", toolbarBounds.withLeft(10), juce::Justification::centredLeft);
    
    g.setColour(borderColour);
    g.drawHorizontalLine(TOOLBAR_HEIGHT - 1, 0, (float)getWidth());
    
    // Draw border
    g.setColour(borderColour);
    g.drawRect(getLocalBounds(), 1);
    
    // Draw placeholder content if no embedded monitor
    if (embeddedMonitor == nullptr)
    {
        auto contentBounds = bounds.reduced(20);
        
        // Draw compass-like circle (matching reference)
        auto circleSize = juce::jmin(contentBounds.getWidth(), contentBounds.getHeight()) * 0.5f;
        auto circleBounds = contentBounds.toFloat()
            .withSizeKeepingCentre(circleSize, circleSize)
            .translated(0, -20);
        
        g.setColour(borderColour);
        g.drawEllipse(circleBounds, 1.0f);
        
        // Draw crosshairs
        auto centerX = circleBounds.getCentreX();
        auto centerY = circleBounds.getCentreY();
        auto radius = circleSize * 0.5f;
        
        g.setColour(juce::Colour(0xFF333333));
        g.drawLine(centerX - radius, centerY, centerX + radius, centerY, 1.0f);
        g.drawLine(centerX, centerY - radius, centerX, centerY + radius, 1.0f);
        
        // Draw center marker
        g.setColour(accentColour);
        g.fillEllipse(centerX - 4, centerY - 4, 8, 8);
        
        // Draw cardinal direction labels
        g.setColour(dimTextColour);
        g.setFont(juce::Font(9.0f));
        g.drawText("F", centerX - 10, centerY - radius - 15, 20, 15, juce::Justification::centred);
        g.drawText("B", centerX - 10, centerY + radius + 2, 20, 15, juce::Justification::centred);
        g.drawText("L", centerX - radius - 18, centerY - 7, 15, 15, juce::Justification::centred);
        g.drawText("R", centerX + radius + 5, centerY - 7, 15, 15, juce::Justification::centred);
    }
}

void MonitorPanel::resized()
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop(TOOLBAR_HEIGHT);  // Skip toolbar area
    
    if (embeddedMonitor != nullptr)
    {
        // If we have an embedded monitor, give it space below toolbar
        embeddedMonitor->setBounds(bounds);
        statusLabel->setVisible(false);
    }
    else
    {
        // Show placeholder content
        statusLabel->setVisible(true);
        statusLabel->setBounds(bounds.removeFromBottom(30).reduced(10, 5));
    }
}

//==============================================================================
void MonitorPanel::buttonClicked(juce::Button* button)
{
}

//==============================================================================
void MonitorPanel::setMonitorComponent(juce::Component* monitorComponent)
{
    // Remove old embedded component if any
    if (embeddedMonitor != nullptr)
    {
        removeChildComponent(embeddedMonitor);
    }
    
    embeddedMonitor = monitorComponent;
    
    if (embeddedMonitor != nullptr)
    {
        addAndMakeVisible(embeddedMonitor);
    }
    
    resized();
    repaint();
}

} // namespace Mach1

