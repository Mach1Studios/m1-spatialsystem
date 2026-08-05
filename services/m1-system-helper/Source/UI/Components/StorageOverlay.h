/*
    StorageOverlay.h
    ----------------
    Storage panel for the capture store (P1.5): shows how much audio the
    current session has captured, how much exists on disk across all sessions,
    and offers safe cleanup of stale/orphaned session data.

    Safety model:
      - cleanup is two-step: a dry-run preview lists the exact sessions and
        total bytes, then a separate CONFIRM click performs the deletion
      - the active session and pinned sessions are never selectable (enforced
        by StorageGovernor, re-verified at execution time)
      - deletion goes to the system trash when possible

    Same visual language as ExportResultOverlay: dimmed scrim, centered dark
    panel, flat buttons. Scanning runs on a background thread since session
    directories can hold many gigabytes.
*/

#pragma once

#include <JuceHeader.h>
#include "../../Common/Common.h"
#include "../../Core/StorageGovernor.h"
#include "FlatButtonLookAndFeel.h"

namespace Mach1 {

class StorageOverlay : public juce::Component
{
public:
    /** Returns the session id currently being captured into (empty if none). */
    std::function<juce::String()> getActiveSessionId;

    StorageOverlay()
    {
        setInterceptsMouseClicks(true, true);

        const auto styleButton = [this](juce::TextButton& button, juce::Colour textColour)
        {
            button.setLookAndFeel(&flatLookAndFeel);
            button.setColour(juce::TextButton::buttonColourId, HelperUIColours::backgroundAltStrong);
            button.setColour(juce::TextButton::textColourOffId, textColour);
            addChildComponent(button);
        };

        styleButton(closeButton, HelperUIColours::text);
        closeButton.setVisible(true);
        closeButton.onClick = [this]() { dismiss(); };

        styleButton(revealButton, HelperUIColours::accentText);
        revealButton.setVisible(true);
        revealButton.onClick = [this]() {
            const auto root = StorageGovernor::getDefaultCaptureRoot();
            if (root.isDirectory())
                root.revealToUser();
        };

        styleButton(cleanupButton, HelperUIColours::accentText);
        cleanupButton.onClick = [this]() { enterPreview(); };

        styleButton(confirmButton, HelperUIColours::error);
        confirmButton.onClick = [this]() { executePlannedCleanup(); };

        styleButton(cancelButton, HelperUIColours::text);
        cancelButton.onClick = [this]() { leavePreview(); };
    }

    ~StorageOverlay() override
    {
        closeButton.setLookAndFeel(nullptr);
        revealButton.setLookAndFeel(nullptr);
        cleanupButton.setLookAndFeel(nullptr);
        confirmButton.setLookAndFeel(nullptr);
        cancelButton.setLookAndFeel(nullptr);
    }

    void open()
    {
        previewing = false;
        setVisible(true);
        toFront(true);
        rescan();
    }

    void dismiss() { setVisible(false); }

    //==========================================================================
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black.withAlpha(0.55f));

        const auto panel = getPanelBounds().toFloat();
        g.setColour(HelperUIColours::backgroundAlt);
        g.fillRoundedRectangle(panel, 6.0f);
        g.setColour(HelperUIColours::border);
        g.drawRoundedRectangle(panel.reduced(0.5f), 6.0f, 1.0f);

        auto content = getPanelBounds().reduced(kPadding);

        // Title
        auto titleArea = content.removeFromTop(kTitleHeight);
        g.setColour(previewing ? HelperUIColours::warning : HelperUIColours::accent);
        g.fillRect(titleArea.removeFromLeft(3));
        titleArea.removeFromLeft(9);
        g.setColour(HelperUIColours::text);
        g.setFont(juce::Font(15.0f, juce::Font::bold));
        g.drawText(previewing ? "CLEAN UP PREVIEW" : "CAPTURE STORAGE",
                   titleArea, juce::Justification::centredLeft);
        content.removeFromTop(8);

        pinHitRects.clear();

        if (scanning)
        {
            g.setColour(HelperUIColours::textDim);
            g.setFont(juce::Font(12.5f));
            g.drawText("Scanning capture storage...", content.removeFromTop(kLineHeight),
                       juce::Justification::centredLeft);
            return;
        }

        if (previewing)
            paintPreview(g, content);
        else
            paintOverview(g, content);
    }

    void resized() override
    {
        auto panel = getPanelBounds().reduced(kPadding);
        auto buttonRow = panel.removeFromBottom(kButtonHeight);

        closeButton.setBounds(buttonRow.removeFromRight(90));
        buttonRow.removeFromRight(8);

        if (previewing)
        {
            confirmButton.setBounds(buttonRow.removeFromRight(170));
            buttonRow.removeFromRight(8);
            cancelButton.setBounds(buttonRow.removeFromRight(90));
        }
        else
        {
            cleanupButton.setBounds(buttonRow.removeFromRight(230));
            buttonRow.removeFromRight(8);
            revealButton.setBounds(buttonRow.removeFromRight(130));
        }

        confirmButton.setVisible(previewing);
        cancelButton.setVisible(previewing);
        cleanupButton.setVisible(!previewing);
        revealButton.setVisible(!previewing);
    }

    void mouseUp(const juce::MouseEvent& event) override
    {
        // Pin toggles (overview rows only)
        for (const auto& [rect, index] : pinHitRects)
        {
            if (rect.contains(event.getPosition()) && index < static_cast<int>(report.sessions.size()))
            {
                const auto& session = report.sessions[static_cast<size_t>(index)];
                StorageGovernor::setPinned(session.directory, !session.pinned);
                rescan();
                return;
            }
        }

        if (!getPanelBounds().contains(event.getPosition()))
            dismiss();
    }

private:
    //==========================================================================
    void rescan()
    {
        scanning = true;
        repaint();

        const juce::String activeId = getActiveSessionId ? getActiveSessionId() : juce::String();
        auto safeThis = juce::Component::SafePointer<StorageOverlay>(this);

        juce::Thread::launch([safeThis, activeId]() {
            const auto freshReport = StorageGovernor::scan(
                StorageGovernor::getDefaultCaptureRoot(), activeId, StorageGovernor::Policy{});

            juce::MessageManager::callAsync([safeThis, freshReport]() {
                if (safeThis == nullptr)
                    return;
                safeThis->report = freshReport;
                safeThis->plan = StorageGovernor::planCleanup(freshReport);
                safeThis->scanning = false;
                safeThis->cleanupButton.setEnabled(!safeThis->plan.selected.empty());
                safeThis->cleanupButton.setButtonText(safeThis->plan.selected.empty()
                    ? "NOTHING TO CLEAN UP"
                    : "CLEAN UP STALE + ORPHANED (" + formatBytes(safeThis->plan.bytesToFree) + ")");
                safeThis->resized();
                safeThis->repaint();
            });
        });
    }

    void enterPreview()
    {
        if (plan.selected.empty())
            return;
        previewing = true;
        resized();
        repaint();
    }

    void leavePreview()
    {
        previewing = false;
        resized();
        repaint();
    }

    void executePlannedCleanup()
    {
        // Deletion is quick (trash move); rescan afterwards shows the result
        lastResult = StorageGovernor::executeCleanup(plan);
        hasLastResult = true;
        previewing = false;
        rescan();
    }

    //==========================================================================
    void paintOverview(juce::Graphics& g, juce::Rectangle<int>& content)
    {
        g.setFont(juce::Font(12.5f));

        // Summary: this session / all sessions / reclaimable
        const StorageGovernor::SessionUsage* active = nullptr;
        for (const auto& session : report.sessions)
            if (session.state == StorageGovernor::SessionState::Active)
                active = &session;

        g.setColour(HelperUIColours::text);
        g.drawText("This session:  " + (active
                       ? formatBytes(active->sizeBytes) + "   (" + juce::String(active->streamCount) + " stream(s))"
                       : juce::String("no active capture")),
                   content.removeFromTop(kLineHeight), juce::Justification::centredLeft);

        g.drawText("All sessions:  " + formatBytes(report.totalBytes)
                       + "   (" + juce::String(static_cast<int>(report.sessions.size())) + " session(s))",
                   content.removeFromTop(kLineHeight), juce::Justification::centredLeft);

        g.setColour(report.reclaimableBytes > 0 ? HelperUIColours::warning : HelperUIColours::success);
        g.drawText("Reclaimable (stale + orphaned):  " + formatBytes(report.reclaimableBytes),
                   content.removeFromTop(kLineHeight), juce::Justification::centredLeft);

        if (hasLastResult)
        {
            g.setColour(lastResult.failures.empty() ? HelperUIColours::success : HelperUIColours::error);
            juce::String line = "Last cleanup freed " + formatBytes(lastResult.bytesFreed)
                + " (" + juce::String(lastResult.sessionsRemoved) + " session(s))";
            if (!lastResult.failures.empty())
                line += ", " + juce::String(static_cast<int>(lastResult.failures.size())) + " failed";
            g.drawText(line, content.removeFromTop(kLineHeight), juce::Justification::centredLeft);
        }

        content.removeFromTop(10);

        // Column header
        g.setColour(HelperUIColours::textDim);
        {
            auto header = content.removeFromTop(kLineHeight);
            g.drawText("SESSION", header.removeFromLeft(kNameColWidth), juce::Justification::centredLeft);
            g.drawText("LAST WRITTEN", header.removeFromLeft(kAgeColWidth), juce::Justification::centredLeft);
            g.drawText("SIZE", header.removeFromLeft(kSizeColWidth), juce::Justification::centredLeft);
            g.drawText("STATE", header.removeFromLeft(kStateColWidth), juce::Justification::centredLeft);
            g.drawText("PIN", header.removeFromLeft(kPinColWidth), juce::Justification::centredLeft);
        }

        int rowIndex = 0;
        for (const auto& session : report.sessions)
        {
            if (rowIndex >= kMaxRows)
            {
                g.setColour(HelperUIColours::textDim);
                g.drawText("... " + juce::String(static_cast<int>(report.sessions.size()) - kMaxRows)
                               + " more session(s), REVEAL FOLDER to browse",
                           content.removeFromTop(kLineHeight), juce::Justification::centredLeft);
                break;
            }

            auto row = content.removeFromTop(kLineHeight);

            g.setColour(session.state == StorageGovernor::SessionState::Active
                            ? HelperUIColours::accentText : HelperUIColours::text);
            g.drawText(session.sessionId, row.removeFromLeft(kNameColWidth), juce::Justification::centredLeft);

            g.setColour(HelperUIColours::textDim);
            g.drawText(formatAge(session.lastWrittenMs), row.removeFromLeft(kAgeColWidth), juce::Justification::centredLeft);
            g.setColour(HelperUIColours::text);
            g.drawText(formatBytes(session.sizeBytes), row.removeFromLeft(kSizeColWidth), juce::Justification::centredLeft);

            g.setColour(stateColour(session.state));
            g.drawText(stateName(session.state), row.removeFromLeft(kStateColWidth), juce::Justification::centredLeft);

            // Pin toggle: pinned sessions are exempt from cleanup
            auto pinRect = row.removeFromLeft(kPinColWidth);
            pinHitRects.push_back({ pinRect, rowIndex });
            g.setColour(session.pinned ? HelperUIColours::accent : HelperUIColours::textDim);
            g.drawText(session.pinned ? "[x]" : "[ ]", pinRect, juce::Justification::centredLeft);

            ++rowIndex;
        }
    }

    void paintPreview(juce::Graphics& g, juce::Rectangle<int>& content)
    {
        g.setFont(juce::Font(12.5f));

        g.setColour(HelperUIColours::text);
        g.drawText("The following " + juce::String(static_cast<int>(plan.selected.size()))
                       + " session(s) will be moved to the trash, freeing "
                       + formatBytes(plan.bytesToFree) + ":",
                   content.removeFromTop(kLineHeight), juce::Justification::centredLeft);
        content.removeFromTop(6);

        int rowIndex = 0;
        for (const auto& session : plan.selected)
        {
            if (rowIndex >= kMaxRows)
            {
                g.setColour(HelperUIColours::textDim);
                g.drawText("... and " + juce::String(static_cast<int>(plan.selected.size()) - kMaxRows) + " more",
                           content.removeFromTop(kLineHeight), juce::Justification::centredLeft);
                break;
            }

            auto row = content.removeFromTop(kLineHeight);
            g.setColour(HelperUIColours::text);
            g.drawText(session.sessionId, row.removeFromLeft(kNameColWidth), juce::Justification::centredLeft);
            g.setColour(HelperUIColours::textDim);
            g.drawText(formatAge(session.lastWrittenMs), row.removeFromLeft(kAgeColWidth), juce::Justification::centredLeft);
            g.setColour(HelperUIColours::text);
            g.drawText(formatBytes(session.sizeBytes), row.removeFromLeft(kSizeColWidth), juce::Justification::centredLeft);
            g.setColour(stateColour(session.state));
            g.drawText(stateName(session.state), row.removeFromLeft(kStateColWidth), juce::Justification::centredLeft);
            ++rowIndex;
        }

        content.removeFromTop(6);
        g.setColour(HelperUIColours::textDim);
        g.drawText("Active and pinned sessions are never selected.",
                   content.removeFromTop(kLineHeight), juce::Justification::centredLeft);
    }

    //==========================================================================
    juce::Rectangle<int> getPanelBounds() const
    {
        const int visibleRows = juce::jmin(kMaxRows + 1,
            static_cast<int>(previewing ? plan.selected.size() + 2 : report.sessions.size() + 1));
        const int summaryLines = previewing ? 2 : (4 + (hasLastResult ? 1 : 0));
        const int height = kPadding * 2 + kTitleHeight + 8
                         + (summaryLines + visibleRows + 1) * kLineHeight
                         + 12 + kButtonHeight;
        const int width = juce::jmin(640, getWidth() - 40);
        return { (getWidth() - width) / 2,
                 (getHeight() - juce::jmin(height, getHeight() - 40)) / 2,
                 width, juce::jmin(height, getHeight() - 40) };
    }

    static juce::String formatBytes(juce::int64 bytes)
    {
        return juce::File::descriptionOfSizeInBytes(bytes);
    }

    static juce::String formatAge(juce::int64 whenMs)
    {
        const juce::int64 ageMs = juce::Time::currentTimeMillis() - whenMs;
        if (ageMs < 60 * 1000)                 return "just now";
        if (ageMs < 60 * 60 * 1000)            return juce::String(ageMs / (60 * 1000)) + " min ago";
        if (ageMs < 24 * 60 * 60 * 1000)       return juce::String(ageMs / (60 * 60 * 1000)) + " h ago";
        return juce::String(ageMs / (24 * 60 * 60 * 1000)) + " d ago";
    }

    static juce::String stateName(StorageGovernor::SessionState state)
    {
        switch (state)
        {
            case StorageGovernor::SessionState::Active:   return "Active";
            case StorageGovernor::SessionState::Recent:   return "Recent";
            case StorageGovernor::SessionState::Stale:    return "Stale";
            case StorageGovernor::SessionState::Orphaned: return "Orphaned";
        }
        return {};
    }

    static juce::Colour stateColour(StorageGovernor::SessionState state)
    {
        switch (state)
        {
            case StorageGovernor::SessionState::Active:   return HelperUIColours::success;
            case StorageGovernor::SessionState::Recent:   return HelperUIColours::text;
            case StorageGovernor::SessionState::Stale:    return HelperUIColours::warning;
            case StorageGovernor::SessionState::Orphaned: return HelperUIColours::error;
        }
        return HelperUIColours::text;
    }

    //==========================================================================
    static constexpr int kPadding = 18;
    static constexpr int kTitleHeight = 22;
    static constexpr int kLineHeight = 19;
    static constexpr int kButtonHeight = 26;
    static constexpr int kMaxRows = 12;
    static constexpr int kNameColWidth = 230;
    static constexpr int kAgeColWidth = 110;
    static constexpr int kSizeColWidth = 90;
    static constexpr int kStateColWidth = 80;
    static constexpr int kPinColWidth = 40;

    StorageGovernor::Report report;
    StorageGovernor::CleanupPlan plan;
    StorageGovernor::CleanupResult lastResult;
    bool hasLastResult = false;
    bool scanning = false;
    bool previewing = false;

    std::vector<std::pair<juce::Rectangle<int>, int>> pinHitRects;

    FlatButtonLookAndFeel flatLookAndFeel;
    juce::TextButton closeButton { "CLOSE" };
    juce::TextButton revealButton { "REVEAL FOLDER" };
    juce::TextButton cleanupButton { "CLEAN UP STALE + ORPHANED" };
    juce::TextButton confirmButton { "CONFIRM (MOVE TO TRASH)" };
    juce::TextButton cancelButton { "CANCEL" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StorageOverlay)
};

} // namespace Mach1
