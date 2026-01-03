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
    // Title label
    titleLabel = std::make_unique<juce::Label>("title", "Monitor Interface");
    titleLabel->setFont(juce::Font(14.0f, juce::Font::bold));
    titleLabel->setColour(juce::Label::textColourId, textColour);
    titleLabel->setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel.get());
    
    // Status label
    statusLabel = std::make_unique<juce::Label>("status", 
        "Reserved space for m1-monitor GUI");
    statusLabel->setFont(juce::Font(12.0f));
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
    
    // Draw border
    g.setColour(borderColour);
    g.drawRect(getLocalBounds(), 1);
    
    // Draw placeholder icon/graphic
    if (embeddedMonitor == nullptr)
    {
        auto bounds = getLocalBounds().reduced(20);
        auto iconBounds = bounds.withSizeKeepingCentre(80, 80).translated(0, -20);
        
        // Draw speaker/monitor icon placeholder
        g.setColour(borderColour);
        g.drawRoundedRectangle(iconBounds.toFloat(), 8.0f, 2.0f);
        
        // Draw simple speaker cone
        auto centerX = iconBounds.getCentreX();
        auto centerY = iconBounds.getCentreY();
        
        g.setColour(dimTextColour.withAlpha(0.3f));
        
        // Draw concentric arcs representing sound waves
        for (int i = 0; i < 3; ++i)
        {
            float radius = 15.0f + i * 12.0f;
            float startAngle = -juce::MathConstants<float>::pi * 0.3f;
            float endAngle = juce::MathConstants<float>::pi * 0.3f;
            
            juce::Path arc;
            arc.addCentredArc(centerX, centerY, radius, radius, 0, startAngle, endAngle, true);
            g.strokePath(arc, juce::PathStrokeType(2.0f));
        }
        
        // Draw speaker symbol
        g.fillRect(centerX - 20, centerY - 10, 15, 20);
        
        juce::Path cone;
        cone.addTriangle(centerX - 5, centerY - 15, 
                         centerX - 5, centerY + 15, 
                         centerX + 15, centerY);
        g.fillPath(cone);
    }
}

void MonitorPanel::resized()
{
    auto bounds = getLocalBounds();
    
    if (embeddedMonitor != nullptr)
    {
        // If we have an embedded monitor, give it full space
        embeddedMonitor->setBounds(bounds);
        titleLabel->setVisible(false);
        statusLabel->setVisible(false);
    }
    else
    {
        // Show placeholder content
        titleLabel->setVisible(true);
        statusLabel->setVisible(true);
        
        titleLabel->setBounds(bounds.removeFromTop(30));

        // Status label gets remaining space in center
        statusLabel->setBounds(bounds.reduced(10).translated(0, 40));
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

