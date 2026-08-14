#pragma once

#include <JuceHeader.h>
#include "../../Common/Common.h"
#include "../../Managers/ProjectPairingManager.h"
#include "FlatButtonLookAndFeel.h"

#include <algorithm>

namespace Mach1 {

class ProjectSessionOverlay : public juce::Component,
                              private juce::TableListBoxModel
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
        sessionName.setColour(juce::TextEditor::backgroundColourId,
                              HelperUIColours::background.darker(0.35f));
        sessionName.setColour(juce::TextEditor::textColourId, HelperUIColours::text);
        sessionName.setColour(juce::TextEditor::outlineColourId, HelperUIColours::gridEmphasis);
        sessionName.setColour(juce::TextEditor::focusedOutlineColourId, HelperUIColours::active);
        sessionName.setColour(juce::TextEditor::highlightColourId,
                              HelperUIColours::active.withAlpha(0.28f));
        sessionName.setColour(juce::CaretComponent::caretColourId, HelperUIColours::active);
        sessionName.setFont(juce::Font(13.0f));
        sessionName.setIndents(8, 7);
        sessionName.onTextChange = [this]() {
            nameButton.setEnabled(sessionName.getText().trim().isNotEmpty());
        };
        sessionName.onReturnKey = [this]() { nameCurrentProject(); };
        addAndMakeVisible(sessionName);

        styleButton(nameButton, "NAME & START", HelperUIColours::text);
        nameButton.onClick = [this]() { nameCurrentProject(); };
        nameButton.setEnabled(false);

        configureRecentSessionsTable();

        styleButton(laterButton, "NOT NOW", HelperUIColours::text);
        laterButton.onClick = [this]() {
            setVisible(false);
            if (onDismiss)
                onDismiss();
        };
    }

    ~ProjectSessionOverlay() override
    {
        recentSessionsTable.setModel(nullptr);
        nameButton.setLookAndFeel(nullptr);
        laterButton.setLookAndFeel(nullptr);
    }

    void open(std::vector<HostOption> hostOptions,
              std::vector<ProjectBinding> knownBindings)
    {
        hosts = std::move(hostOptions);
        bindings = std::move(knownBindings);
        selectedHostIndex = hosts.empty() ? -1 : 0;
        sortBindings(sortColumnId, sortForwards);

        sessionName.clear();
        emptySessionsLabel.setVisible(bindings.empty());
        recentSessionsTable.updateContent();
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
        g.setColour(HelperUIColours::text);
        g.setFont(juce::Font(17.0f, juce::Font::bold));
        g.drawText("SESSION ASSIGNMENT", title, juce::Justification::centredLeft);

        content.removeFromTop(5);
        g.setColour(HelperUIColours::textApp);
        g.setFont(juce::Font(12.5f));
        g.drawFittedText(
            "Please either assign an existing session to this host process or create a new one to continue.",
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
                + "  |  " + juce::String(host.streamingPanners) + " streaming Panner"
                + (host.streamingPanners == 1 ? "" : "s");
            if (host.ambiguous)
                label += "  |  MULTIPLE SAVED PROJECTS";
            else if (host.currentBinding.displayName.isNotEmpty())
                label += "  |  " + host.currentBinding.displayName;

            g.setColour(host.ambiguous ? HelperUIColours::textApp : HelperUIColours::text);
            g.setFont(juce::Font(12.0f, selected ? juce::Font::bold : juce::Font::plain));
            g.drawText(label, row.reduced(10, 0), juce::Justification::centredLeft);
        }

        content.setY(hostHitAreas.empty() ? content.getY()
                                         : hostHitAreas.back().getBottom() + 12);
        paintSectionLabel(g, content.removeFromTop(20), "NEW SESSION NAME");
        content.removeFromTop(kEditorHeight + 12);

        paintSectionLabel(g, content.removeFromTop(20), "SELECT EXISTING SESSION");
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
        nameButton.setBounds(buttons.removeFromRight(125));

        const int tableTop = hostBottom + 98;
        const int tableBottom = buttons.getY() - 14;
        recentSessionsTable.setBounds(panel.getX(), tableTop, panel.getWidth(),
                                      juce::jmax(80, tableBottom - tableTop));
        emptySessionsLabel.setBounds(
            recentSessionsTable.getBounds().reduced(10).withTrimmedTop(kTableHeaderHeight));
    }

    void mouseUp(const juce::MouseEvent& event) override
    {
        for (size_t index = 0; index < hostHitAreas.size(); ++index)
        {
            if (hostHitAreas[index].contains(event.getPosition()))
            {
                selectedHostIndex = static_cast<int>(index);
                resized();
                recentSessionsTable.updateContent();
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
    static constexpr int kTableHeaderHeight = 26;
    static constexpr int kSessionColumn = 1;
    static constexpr int kLastUsedColumn = 2;
    static constexpr int kPannersColumn = 3;
    static constexpr int kUseColumn = 4;

    void configureRecentSessionsTable()
    {
        recentSessionsTable.setModel(this);
        recentSessionsTable.setHeaderHeight(kTableHeaderHeight);
        recentSessionsTable.setRowHeight(32);
        recentSessionsTable.setMultipleSelectionEnabled(false);
        recentSessionsTable.setOutlineThickness(1);
        recentSessionsTable.setColour(
            juce::ListBox::backgroundColourId,
            HelperUIColours::background.darker(0.35f));
        recentSessionsTable.setColour(juce::ListBox::outlineColourId,
                                      HelperUIColours::gridEmphasis);

        auto& header = recentSessionsTable.getHeader();
        const int sortable = juce::TableHeaderComponent::visible
            | juce::TableHeaderComponent::resizable
            | juce::TableHeaderComponent::sortable;
        header.addColumn("SESSION", kSessionColumn, 260, 140, 420, sortable);
        header.addColumn("LAST USED", kLastUsedColumn, 155, 115, 220, sortable);
        header.addColumn("PANNERS", kPannersColumn, 85, 70, 110, sortable);
        header.addColumn("", kUseColumn, 82, 72, 100,
                         juce::TableHeaderComponent::visible);
        header.setStretchToFitActive(true);
        header.setPopupMenuActive(false);
        header.setColour(juce::TableHeaderComponent::backgroundColourId,
                         HelperUIColours::background.darker(0.18f));
        header.setColour(juce::TableHeaderComponent::textColourId,
                         HelperUIColours::textApp);
        header.setColour(juce::TableHeaderComponent::outlineColourId,
                         HelperUIColours::gridEmphasis);
        header.setColour(juce::TableHeaderComponent::highlightColourId,
                         HelperUIColours::background.brighter(0.08f));
        header.setSortColumnId(kLastUsedColumn, false);

        recentSessionsTable.getVerticalScrollBar().setColour(
            juce::ScrollBar::thumbColourId, HelperUIColours::gridEmphasis);
        addAndMakeVisible(recentSessionsTable);

        emptySessionsLabel.setText("No named sessions yet.",
                                   juce::dontSendNotification);
        emptySessionsLabel.setJustificationType(juce::Justification::centred);
        emptySessionsLabel.setColour(juce::Label::textColourId,
                                     HelperUIColours::textApp);
        emptySessionsLabel.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(emptySessionsLabel);
    }

    int getNumRows() override
    {
        return static_cast<int>(bindings.size());
    }

    void paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height,
                            bool rowIsSelected) override
    {
        const auto base = HelperUIColours::background.darker(
            rowNumber % 2 == 0 ? 0.35f : 0.24f);
        g.fillAll(rowIsSelected ? HelperUIColours::background.brighter(0.08f) : base);
        g.setColour(HelperUIColours::separator);
        g.fillRect(0, height - 1, width, 1);
    }

    void paintCell(juce::Graphics& g, int rowNumber, int columnId,
                   int width, int height, bool rowIsSelected) override
    {
        if (rowNumber < 0 || rowNumber >= static_cast<int>(bindings.size()))
            return;

        const auto& binding = bindings[static_cast<size_t>(rowNumber)];
        juce::String value;
        auto justification = juce::Justification::centredLeft;
        if (columnId == kSessionColumn)
            value = binding.displayName;
        else if (columnId == kLastUsedColumn)
            value = formatLastUsed(binding.lastUsedMs);
        else if (columnId == kPannersColumn)
        {
            value = binding.pannerCount >= 0 ? juce::String(binding.pannerCount) : "N/A";
            justification = juce::Justification::centred;
        }

        if (value.isNotEmpty())
        {
            g.setColour(HelperUIColours::text);
            g.setFont(juce::Font(11.5f, rowIsSelected
                ? juce::Font::bold : juce::Font::plain));
            g.drawText(value, 8, 0, width - 16, height, justification, true);
        }

        g.setColour(HelperUIColours::separator);
        g.fillRect(width - 1, 0, 1, height);
    }

    juce::Component* refreshComponentForCell(
        int rowNumber, int columnId, bool,
        juce::Component* existingComponentToUpdate) override
    {
        if (columnId != kUseColumn)
        {
            delete existingComponentToUpdate;
            return nullptr;
        }

        auto* button = dynamic_cast<juce::TextButton*>(existingComponentToUpdate);
        if (button == nullptr)
        {
            delete existingComponentToUpdate;
            button = new juce::TextButton();
            button->setLookAndFeel(&flatLookAndFeel);
        }

        button->setButtonText("USE");
        button->setColour(juce::TextButton::buttonColourId,
                          HelperUIColours::background.brighter(0.10f));
        button->setColour(juce::TextButton::textColourOffId,
                          HelperUIColours::text);
        button->setEnabled(selectedHostIndex >= 0);
        button->onClick = [this, rowNumber]() { useBinding(rowNumber); };
        return button;
    }

    void sortOrderChanged(int newSortColumnId, bool isForwards) override
    {
        sortColumnId = newSortColumnId;
        sortForwards = isForwards;
        sortBindings(sortColumnId, sortForwards);
        recentSessionsTable.updateContent();
        recentSessionsTable.repaint();
    }

    void sortBindings(int columnId, bool forwards)
    {
        std::stable_sort(bindings.begin(), bindings.end(),
                         [columnId, forwards](const auto& first, const auto& second) {
            int result = 0;
            if (columnId == kLastUsedColumn)
                result = first.lastUsedMs < second.lastUsedMs ? -1
                    : (first.lastUsedMs > second.lastUsedMs ? 1 : 0);
            else if (columnId == kPannersColumn)
                result = first.pannerCount < second.pannerCount ? -1
                    : (first.pannerCount > second.pannerCount ? 1 : 0);
            else
                result = first.displayName.compareIgnoreCase(second.displayName);

            if (result == 0)
                result = first.displayName.compareIgnoreCase(second.displayName);
            return forwards ? result < 0 : result > 0;
        });
    }

    static juce::String formatLastUsed(juce::int64 lastUsedMs)
    {
        if (lastUsedMs <= 0)
            return "Unknown";
        return juce::Time(lastUsedMs).formatted("%b %d, %H:%M");
    }

    void useBinding(int rowNumber)
    {
        if (selectedHostIndex < 0
            || rowNumber < 0
            || rowNumber >= static_cast<int>(bindings.size())
            || onSelectProject == nullptr)
            return;

        onSelectProject(hosts[static_cast<size_t>(selectedHostIndex)].processId,
                        bindings[static_cast<size_t>(rowNumber)].bindingId);
    }

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
                         HelperUIColours::background.brighter(0.10f));
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
        g.setColour(selected ? HelperUIColours::background.brighter(0.08f)
                             : HelperUIColours::background.darker(0.35f));
        g.fillRoundedRectangle(row.toFloat(), 3.0f);
        g.setColour(selected ? HelperUIColours::active : HelperUIColours::gridEmphasis);
        g.drawRoundedRectangle(row.toFloat().reduced(0.5f), 3.0f, 1.0f);
    }

    void rebuildHitAreas()
    {
        hostHitAreas.clear();
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
    juce::TextButton laterButton;
    juce::TableListBox recentSessionsTable;
    juce::Label emptySessionsLabel;
    std::vector<HostOption> hosts;
    std::vector<ProjectBinding> bindings;
    std::vector<juce::Rectangle<int>> hostHitAreas;
    int selectedHostIndex = -1;
    int sortColumnId = kLastUsedColumn;
    bool sortForwards = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProjectSessionOverlay)
};

} // namespace Mach1
