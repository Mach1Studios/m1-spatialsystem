/*
    FlatButtonLookAndFeel.h
    -----------------------
    Custom LookAndFeel for flat buttons matching the Soundfield Display style.
*/

#pragma once

#include <JuceHeader.h>

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
        setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF1F1F1F));
        setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFF939393));
        setColour(juce::TextButton::textColourOffId, juce::Colour(0xFFCCCCCC));
        setColour(juce::TextButton::textColourOnId, juce::Colour(0xFF0D0D0D));
        
        // Toggle button colors
        setColour(juce::ToggleButton::textColourId, juce::Colour(0xFFCCCCCC));
        setColour(juce::ToggleButton::tickColourId, juce::Colour(0xFFCCCCCC));
        setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour(0xFF666666));
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
    
    void drawTickBox(juce::Graphics& g,
                     juce::Component& component,
                     float x, float y, float w, float h,
                     bool ticked,
                     bool isEnabled,
                     bool shouldDrawButtonAsHighlighted,
                     bool shouldDrawButtonAsDown) override
    {
        auto bounds = juce::Rectangle<float>(x, y, w, h);
        
        // Draw checkbox background
        g.setColour(juce::Colour(0xFF1F1F1F));
        g.fillRoundedRectangle(bounds, 2.0f);
        
        // Draw border
        g.setColour(juce::Colour(0xFF3A3A3A));
        g.drawRoundedRectangle(bounds, 2.0f, 1.0f);
        
        // Draw tick if checked
        if (ticked)
        {
            auto tickColour = isEnabled ? juce::Colour(0xFFCCCCCC) : juce::Colour(0xFF666666);
            g.setColour(tickColour);
            
            // Draw checkmark
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

