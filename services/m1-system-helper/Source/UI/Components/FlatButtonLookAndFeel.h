/*
    FlatButtonLookAndFeel.h
    -----------------------
    Custom LookAndFeel for flat buttons matching the Soundfield Display style.
*/

#pragma once

#include <JuceHeader.h>
#include "../../Common/Common.h"

namespace Mach1 {

/**
 * Custom LookAndFeel that draws flat buttons without outlines
 * Matches the style used in Panner3DViewPanel toolbar
 */
class FlatButtonLookAndFeel : public juce::LookAndFeel_V4
{
public:
    FlatButtonLookAndFeel()
    {
        // Set default colors
        setColour(juce::TextButton::buttonColourId, HelperUIColours::gridMajor);
        setColour(juce::TextButton::buttonOnColourId, HelperUIColours::active);
        setColour(juce::TextButton::textColourOffId, HelperUIColours::text);
        setColour(juce::TextButton::textColourOnId, HelperUIColours::accentText);
        
        // Toggle button colors
        setColour(juce::ToggleButton::textColourId, HelperUIColours::text);
        setColour(juce::ToggleButton::tickColourId, HelperUIColours::text);
        setColour(juce::ToggleButton::tickDisabledColourId, HelperUIColours::inactive);
    }
    
    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat();
        
        // Determine the fill color
        juce::Colour fillColour = backgroundColour;
        
        if (shouldDrawButtonAsDown)
        {
            fillColour = fillColour.brighter(0.1f);
        }
        else if (shouldDrawButtonAsHighlighted)
        {
            fillColour = fillColour.brighter(0.05f);
        }
        
        // Draw flat rectangle with subtle rounded corners (2px radius like Soundfield Display)
        g.setColour(fillColour);
        g.fillRoundedRectangle(bounds, 2.0f);
    }
    
    void drawButtonText(juce::Graphics& g,
                        juce::TextButton& button,
                        bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override
    {
        auto font = juce::Font(9.0f);
        g.setFont(font);
        
        auto textColour = button.findColour(button.getToggleState() 
            ? juce::TextButton::textColourOnId 
            : juce::TextButton::textColourOffId);
        
        g.setColour(textColour);
        
        auto bounds = button.getLocalBounds();
        g.drawText(button.getButtonText(), bounds, juce::Justification::centred, false);
    }
    
    void drawToggleButton(juce::Graphics& g,
                          juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat();
        float tickSize = 14.0f;
        
        drawTickBox(g, button,
                    bounds.getX(), bounds.getCentreY() - tickSize * 0.5f,
                    tickSize, tickSize,
                    button.getToggleState(),
                    button.isEnabled(),
                    shouldDrawButtonAsHighlighted,
                    shouldDrawButtonAsDown);
        
        g.setColour(button.findColour(juce::ToggleButton::textColourId));
        g.setFont(juce::Font(10.0f));
        
        auto textBounds = bounds.withLeft(bounds.getX() + tickSize + 4.0f);
        g.drawText(button.getButtonText(), textBounds, juce::Justification::centredLeft, false);
    }
    
    void drawTickBox(juce::Graphics& g,
                     juce::Component& component,
                     float x, float y, float w, float h,
                     bool ticked,
                     bool isEnabled,
                     bool shouldDrawButtonAsHighlighted,
                     bool shouldDrawButtonAsDown) override
    {
        auto bounds = juce::Rectangle<float>(x, y, w, h);
        
        g.setColour(HelperUIColours::gridMajor);
        g.fillRoundedRectangle(bounds, 2.0f);
        
        g.setColour(HelperUIColours::border);
        g.drawRoundedRectangle(bounds, 2.0f, 1.0f);
        
        if (ticked)
        {
            auto tickColour = isEnabled ? HelperUIColours::text : HelperUIColours::inactive;
            g.setColour(tickColour);
            
            auto tickBounds = bounds.reduced(3.0f);
            juce::Path tick;
            tick.startNewSubPath(tickBounds.getX(), tickBounds.getCentreY());
            tick.lineTo(tickBounds.getCentreX() - 1, tickBounds.getBottom() - 2);
            tick.lineTo(tickBounds.getRight(), tickBounds.getY() + 2);
            g.strokePath(tick, juce::PathStrokeType(1.5f));
        }
    }
    
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FlatButtonLookAndFeel)
};

} // namespace Mach1

