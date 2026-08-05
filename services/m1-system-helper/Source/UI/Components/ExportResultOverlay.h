/*
    ExportResultOverlay.h
    ---------------------
    In-app notification panel for export results, styled to match the helper
    UI (dark theme, flat buttons) instead of the stock JUCE AlertWindow.

    Shown as a child overlay covering the SessionMainComponent: dimmed scrim,
    centered panel with a title, per-stream result lines, and REVEAL / CLOSE
    actions. Clicking the scrim dismisses it.
*/

#pragma once

#include <JuceHeader.h>
#include "../../Common/Common.h"
#include "FlatButtonLookAndFeel.h"

namespace Mach1 {

class ExportResultOverlay : public juce::Component
{
public:
    struct Line
    {
        juce::String text;
        juce::Colour colour { HelperUIColours::text };
    };

    ExportResultOverlay()
    {
        setInterceptsMouseClicks(true, true);

        closeButton.setLookAndFeel(&flatLookAndFeel);
        closeButton.setColour(juce::TextButton::buttonColourId, HelperUIColours::backgroundAltStrong);
        closeButton.setColour(juce::TextButton::textColourOffId, HelperUIColours::text);
        closeButton.onClick = [this]() { dismiss(); };
        addAndMakeVisible(closeButton);

        revealButton.setLookAndFeel(&flatLookAndFeel);
        revealButton.setColour(juce::TextButton::buttonColourId, HelperUIColours::backgroundAltStrong);
        revealButton.setColour(juce::TextButton::textColourOffId, HelperUIColours::accentText);
        revealButton.onClick = [this]() {
            if (revealFile.exists())
                revealFile.revealToUser();
        };
        addChildComponent(revealButton);
    }

    ~ExportResultOverlay() override
    {
        closeButton.setLookAndFeel(nullptr);
        revealButton.setLookAndFeel(nullptr);
    }

    /** Populate and show. Pass a non-existent/empty file to hide REVEAL. */
    void show(const juce::String& titleIn, const std::vector<Line>& linesIn,
              const juce::File& revealFileIn, bool isErrorIn)
    {
        title = titleIn;
        lines = linesIn;
        revealFile = revealFileIn;
        isError = isErrorIn;

        revealButton.setVisible(revealFile.exists());
        setVisible(true);
        toFront(true);
        resized();
        repaint();
    }

    void dismiss()
    {
        setVisible(false);
    }

    void paint(juce::Graphics& g) override
    {
        // Dim everything behind the panel
        g.fillAll(juce::Colours::black.withAlpha(0.55f));

        const auto panel = getPanelBounds().toFloat();
        g.setColour(HelperUIColours::backgroundAlt);
        g.fillRoundedRectangle(panel, 6.0f);
        g.setColour(isError ? HelperUIColours::error : HelperUIColours::border);
        g.drawRoundedRectangle(panel.reduced(0.5f), 6.0f, 1.0f);

        auto content = getPanelBounds().reduced(kPadding);

        // Title with a status accent bar
        auto titleArea = content.removeFromTop(kTitleHeight);
        g.setColour(isError ? HelperUIColours::error : HelperUIColours::success);
        g.fillRect(titleArea.removeFromLeft(3));
        titleArea.removeFromLeft(9);
        g.setColour(HelperUIColours::text);
        g.setFont(juce::Font(15.0f, juce::Font::bold));
        g.drawText(title, titleArea, juce::Justification::centredLeft);

        content.removeFromTop(8);

        g.setFont(juce::Font(12.5f));
        for (const auto& line : lines)
        {
            auto rowArea = content.removeFromTop(kLineHeight);
            g.setColour(line.colour);
            g.drawText(line.text, rowArea, juce::Justification::centredLeft);
        }
    }

    void resized() override
    {
        auto panel = getPanelBounds().reduced(kPadding);
        auto buttonRow = panel.removeFromBottom(kButtonHeight);

        closeButton.setBounds(buttonRow.removeFromRight(90));
        buttonRow.removeFromRight(8);
        if (revealButton.isVisible())
            revealButton.setBounds(buttonRow.removeFromRight(120));
    }

    void mouseUp(const juce::MouseEvent& event) override
    {
        // Click outside the panel dismisses
        if (!getPanelBounds().contains(event.getPosition()))
            dismiss();
    }

private:
    juce::Rectangle<int> getPanelBounds() const
    {
        const int width = juce::jmin(560, getWidth() - 40);
        const int height = kPadding * 2 + kTitleHeight + 8
                         + static_cast<int>(lines.size()) * kLineHeight
                         + 12 + kButtonHeight;
        return { (getWidth() - width) / 2, (getHeight() - height) / 2, width, height };
    }

    static constexpr int kPadding = 18;
    static constexpr int kTitleHeight = 22;
    static constexpr int kLineHeight = 19;
    static constexpr int kButtonHeight = 26;

    juce::String title;
    std::vector<Line> lines;
    juce::File revealFile;
    bool isError = false;

    FlatButtonLookAndFeel flatLookAndFeel;
    juce::TextButton revealButton { "REVEAL FILE" };
    juce::TextButton closeButton { "CLOSE" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ExportResultOverlay)
};

} // namespace Mach1
