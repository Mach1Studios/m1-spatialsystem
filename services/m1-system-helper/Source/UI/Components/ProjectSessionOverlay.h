#pragma once

#include <JuceHeader.h>
#include "../../Common/Common.h"
#include "../../Managers/ProjectPairingManager.h"
#include "FlatButtonLookAndFeel.h"

namespace Mach1 {

class ProjectSessionOverlay : public juce::Component
{
public:
    struct HostOption
    {
        uint32_t processId = 0;
        int streamingPanners = 0;
        ProjectBinding currentBinding;
        bool ambiguous = false;
    };

    std::function<void(uint32_t, const juce::String&)> onNameProject;
    std::function<void(uint32_t, const juce::String&)> onSelectProject;
    std::function<void()> onDismiss;

    ProjectSessionOverlay()
    {
        setInterceptsMouseClicks(true, true);

        sessionName.setMultiLine(false);
        sessionName.setReturnKeyStartsNewLine(false);
        sessionName.setTextToShowWhenEmpty("e.g. Feature Film Mix", HelperUIColours::textApp);
        sessionName.setColour(juce::TextEditor::backgroundColourId, HelperUIColours::backgroundAlt);
        sessionName.setColour(juce::TextEditor::textColourId, HelperUIColours::text);
        sessionName.setColour(juce::TextEditor::outlineColourId, HelperUIColours::gridEmphasis);
        sessionName.setColour(juce::TextEditor::focusedOutlineColourId, HelperUIColours::active);
        sessionName.setColour(juce::TextEditor::highlightColourId,
                              HelperUIColours::active.withAlpha(0.28f));
        sessionName.setFont(juce::Font(13.0f));
        sessionName.onTextChange = [this]() {
            nameButton.setEnabled(sessionName.getText().trim().isNotEmpty());
        };
        sessionName.onReturnKey = [this]() { nameCurrentProject(); };
        addAndMakeVisible(sessionName);

        styleButton(nameButton, "NAME & START", HelperUIColours::text);
        nameButton.onClick = [this]() { nameCurrentProject(); };
        nameButton.setEnabled(false);

        styleButton(useButton, "USE SELECTED", HelperUIColours::text);
        useButton.onClick = [this]() {
            if (selectedHostIndex >= 0 && selectedBindingIndex >= 0 && onSelectProject)
                onSelectProject(hosts[static_cast<size_t>(selectedHostIndex)].processId,
                                bindings[static_cast<size_t>(selectedBindingIndex)].bindingId);
        };

        styleButton(laterButton, "NOT NOW", HelperUIColours::text);
        laterButton.onClick = [this]() {
            setVisible(false);
            if (onDismiss)
                onDismiss();
        };
    }

    ~ProjectSessionOverlay() override
    {
        nameButton.setLookAndFeel(nullptr);
        useButton.setLookAndFeel(nullptr);
        laterButton.setLookAndFeel(nullptr);
    }

    void open(std::vector<HostOption> hostOptions,
              std::vector<ProjectBinding> knownBindings)
    {
        hosts = std::move(hostOptions);
        bindings = std::move(knownBindings);
        selectedHostIndex = hosts.empty() ? -1 : 0;
        selectedBindingIndex = -1;

        if (selectedHostIndex >= 0)
        {
            const auto currentId = hosts.front().currentBinding.bindingId;
            for (size_t index = 0; index < bindings.size(); ++index)
                if (bindings[index].bindingId == currentId)
                    selectedBindingIndex = static_cast<int>(index);
        }

        sessionName.clear();
        rebuildHitAreas();
        setVisible(true);
        toFront(true);
        resized();
        repaint();
        sessionName.grabKeyboardFocus();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black.withAlpha(0.72f));

        const auto panel = getPanelBounds();
        g.setColour(HelperUIColours::background);
        g.fillRoundedRectangle(panel.toFloat(), 7.0f);
        g.setColour(HelperUIColours::gridEmphasis);
        g.drawRoundedRectangle(panel.toFloat().reduced(0.5f), 7.0f, 1.0f);

        auto content = panel.reduced(kPadding);
        auto title = content.removeFromTop(32);
        g.setColour(HelperUIColours::textApp);
        g.fillRect(title.removeFromLeft(3));
        title.removeFromLeft(10);
        g.setColour(HelperUIColours::text);
        g.setFont(juce::Font(17.0f, juce::Font::bold));
        g.drawText("NAME THIS STREAMING SESSION", title, juce::Justification::centredLeft);

        content.removeFromTop(5);
        g.setColour(HelperUIColours::textApp);
        g.setFont(juce::Font(12.5f));
        g.drawFittedText(
            "The selected name and project ID are shared with Monitor and every Panner "
            "in this host process, then recalled from the DAW project state.",
            content.removeFromTop(42), juce::Justification::topLeft, 2);

        content.removeFromTop(8);
        paintSectionLabel(g, content.removeFromTop(20), "STREAMING HOST");
        for (size_t index = 0; index < hostHitAreas.size(); ++index)
        {
            const auto row = hostHitAreas[index];
            const bool selected = static_cast<int>(index) == selectedHostIndex;
            paintRow(g, row, selected);

            const auto& host = hosts[index];
            auto label = "Host process " + juce::String(static_cast<int>(host.processId))
                + "  •  " + juce::String(host.streamingPanners) + " streaming Panner"
                + (host.streamingPanners == 1 ? "" : "s");
            if (host.ambiguous)
                label += "  •  MULTIPLE SAVED PROJECTS";
            else if (host.currentBinding.displayName.isNotEmpty())
                label += "  •  " + host.currentBinding.displayName;

            g.setColour(host.ambiguous ? HelperUIColours::textApp : HelperUIColours::text);
            g.setFont(juce::Font(12.0f, selected ? juce::Font::bold : juce::Font::plain));
            g.drawText(label, row.reduced(10, 0), juce::Justification::centredLeft);
        }

        content.setY(hostHitAreas.empty() ? content.getY()
                                         : hostHitAreas.back().getBottom() + 12);
        paintSectionLabel(g, content.removeFromTop(20), "NEW SESSION NAME");
        content.removeFromTop(kEditorHeight + 12);

        paintSectionLabel(g, content.removeFromTop(20), "OR SELECT A RECENT SESSION");
        if (bindingHitAreas.empty())
        {
            g.setColour(HelperUIColours::textApp);
            g.setFont(juce::Font(12.0f));
            g.drawText("No named sessions yet.", content.removeFromTop(kRowHeight),
                       juce::Justification::centredLeft);
        }
        else
        {
            for (size_t index = 0; index < bindingHitAreas.size(); ++index)
            {
                const auto row = bindingHitAreas[index];
                const bool selected = static_cast<int>(index) == selectedBindingIndex;
                paintRow(g, row, selected);
                g.setColour(HelperUIColours::text);
                g.setFont(juce::Font(12.0f, selected ? juce::Font::bold : juce::Font::plain));
                g.drawText(bindings[index].displayName, row.reduced(10, 0),
                           juce::Justification::centredLeft);
            }
        }
    }

    void resized() override
    {
        rebuildHitAreas();
        auto panel = getPanelBounds().reduced(kPadding);

        const int hostBottom = hostHitAreas.empty()
            ? panel.getY() + 107
            : hostHitAreas.back().getBottom();
        sessionName.setBounds(panel.getX(), hostBottom + 32,
                              panel.getWidth(), kEditorHeight);

        auto buttons = panel.removeFromBottom(kButtonHeight);
        laterButton.setBounds(buttons.removeFromRight(90));
        buttons.removeFromRight(8);
        useButton.setBounds(buttons.removeFromRight(120));
        buttons.removeFromRight(8);
        nameButton.setBounds(buttons.removeFromRight(125));
        useButton.setEnabled(selectedBindingIndex >= 0);
    }

    void mouseUp(const juce::MouseEvent& event) override
    {
        for (size_t index = 0; index < hostHitAreas.size(); ++index)
        {
            if (hostHitAreas[index].contains(event.getPosition()))
            {
                selectedHostIndex = static_cast<int>(index);
                selectedBindingIndex = -1;
                const auto currentId = hosts[index].currentBinding.bindingId;
                for (size_t bindingIndex = 0; bindingIndex < bindings.size(); ++bindingIndex)
                    if (bindings[bindingIndex].bindingId == currentId)
                        selectedBindingIndex = static_cast<int>(bindingIndex);
                resized();
                repaint();
                return;
            }
        }

        for (size_t index = 0; index < bindingHitAreas.size(); ++index)
        {
            if (bindingHitAreas[index].contains(event.getPosition()))
            {
                selectedBindingIndex = static_cast<int>(index);
                resized();
                repaint();
                return;
            }
        }
    }

private:
    static constexpr int kPadding = 24;
    static constexpr int kRowHeight = 34;
    static constexpr int kEditorHeight = 34;
    static constexpr int kButtonHeight = 30;
    static constexpr int kMaxVisibleHosts = 4;
    static constexpr int kMaxVisibleBindings = 5;

    juce::Rectangle<int> getPanelBounds() const
    {
        return getLocalBounds().withSizeKeepingCentre(
            juce::jmin(680, getWidth() - 32),
            juce::jmin(620, getHeight() - 32));
    }

    void styleButton(juce::TextButton& button, const juce::String& text,
                     juce::Colour textColour)
    {
        button.setButtonText(text);
        button.setLookAndFeel(&flatLookAndFeel);
        button.setColour(juce::TextButton::buttonColourId,
                         HelperUIColours::backgroundAlt);
        button.setColour(juce::TextButton::textColourOffId, textColour);
        addAndMakeVisible(button);
    }

    void paintSectionLabel(juce::Graphics& g, juce::Rectangle<int> area,
                           const juce::String& text)
    {
        g.setColour(HelperUIColours::textApp);
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        g.drawText(text, area, juce::Justification::centredLeft);
    }

    void paintRow(juce::Graphics& g, juce::Rectangle<int> row, bool selected)
    {
        g.setColour(selected ? HelperUIColours::backgroundAltStrong.withAlpha(0.62f)
                             : HelperUIColours::backgroundAlt);
        g.fillRoundedRectangle(row.toFloat(), 3.0f);
        g.setColour(selected ? HelperUIColours::active : HelperUIColours::gridEmphasis);
        g.drawRoundedRectangle(row.toFloat().reduced(0.5f), 3.0f, 1.0f);
    }

    void rebuildHitAreas()
    {
        hostHitAreas.clear();
        bindingHitAreas.clear();
        if (getWidth() <= 0 || getHeight() <= 0)
            return;

        auto content = getPanelBounds().reduced(kPadding);
        content.removeFromTop(32 + 5 + 42 + 8 + 20);
        const int visibleHosts = juce::jmin(static_cast<int>(hosts.size()), kMaxVisibleHosts);
        for (int index = 0; index < visibleHosts; ++index)
        {
            hostHitAreas.push_back(content.removeFromTop(kRowHeight));
            content.removeFromTop(4);
        }

        content.removeFromTop(8 + 20 + kEditorHeight + 12 + 20);
        const int visibleBindings = juce::jmin(static_cast<int>(bindings.size()),
                                              kMaxVisibleBindings);
        for (int index = 0; index < visibleBindings; ++index)
        {
            bindingHitAreas.push_back(content.removeFromTop(kRowHeight));
            content.removeFromTop(4);
        }
    }

    void nameCurrentProject()
    {
        const auto name = sessionName.getText().trim();
        if (name.isEmpty() || selectedHostIndex < 0 || onNameProject == nullptr)
            return;
        onNameProject(hosts[static_cast<size_t>(selectedHostIndex)].processId, name);
    }

    FlatButtonLookAndFeel flatLookAndFeel;
    juce::TextEditor sessionName;
    juce::TextButton nameButton;
    juce::TextButton useButton;
    juce::TextButton laterButton;
    std::vector<HostOption> hosts;
    std::vector<ProjectBinding> bindings;
    std::vector<juce::Rectangle<int>> hostHitAreas;
    std::vector<juce::Rectangle<int>> bindingHitAreas;
    int selectedHostIndex = -1;
    int selectedBindingIndex = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProjectSessionOverlay)
};

} // namespace Mach1
