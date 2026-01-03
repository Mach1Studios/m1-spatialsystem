/*
    CaptureTimelinePanel.h
    ----------------------
    UI component that visualizes the capture timeline with coverage, dropouts,
    and playhead position.
    
    Features:
    - Overall coverage lane (captured intervals as filled rects)
    - Different indication for partial vs total dropouts
    - Time ruler with sample-to-seconds conversion
    - Playhead marker (latest received sample position)
    - Stats display (% captured, duration, dropout count)
    - Controls: Reset, Lock Range, Export (stub), Fill Gaps toggle
    - Mouse wheel zoom, click-drag pan, double-click fit
*/

#pragma once

#include <JuceHeader.h>
#include "../../Core/CaptureEngine.h"
#include "../../Core/CoverageModel.h"
#include "FlatButtonLookAndFeel.h"
#include <functional>

namespace Mach1 {

//==============================================================================
/**
 * Capture Timeline Panel Component
 */
class CaptureTimelinePanel : public juce::Component,
                             public juce::ChangeListener,
                             public juce::Timer
{
public:
    CaptureTimelinePanel();
    ~CaptureTimelinePanel() override;
    
    /**
     * Set the capture engine to visualize
     */
    void setCaptureEngine(CaptureEngine* engine);
    
    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
    
    // ChangeListener override
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    
    // Timer override
    void timerCallback() override;
    
    // Callbacks
    std::function<void()> onResetClicked;
    std::function<void()> onExportClicked;
    
private:
    CaptureEngine* m_engine = nullptr;
    
    // View state
    int64_t m_viewStartSample = 0;
    int64_t m_viewEndSample = 0;
    bool m_autoFollow = true;
    bool m_rangeLocked = false;
    bool m_fillGapsOnly = false;
    bool m_autoZoom = true;  // Auto-fit view to captured range
    
    // Interaction state
    bool m_isDragging = false;
    juce::Point<int> m_lastMousePos;
    int64_t m_dragStartViewStart = 0;
    
    // Cached data for painting (thread-safe snapshot)
    struct CachedData
    {
        CoverageModel::GlobalStats stats;
        std::vector<SampleInterval> coverageIntervals;  // Copy for thread safety
        std::vector<SampleInterval> anyDropouts;
        std::vector<SampleInterval> allDropouts;
        int64_t latestSample = 0;
        uint32_t sampleRate = 44100;
        bool capturing = false;
        bool needsRepaint = false;
    };
    CachedData m_cachedData;
    std::atomic<bool> m_cacheUpdatePending{false};
    juce::CriticalSection m_cacheMutex;
    
    // Layout
    static constexpr int HEADER_HEIGHT = 40;
    static constexpr int CONTROLS_HEIGHT = 32;
    static constexpr int TIMELINE_HEIGHT = 60;
    static constexpr int RULER_HEIGHT = 24;
    static constexpr int STATS_HEIGHT = 20;
    
    juce::Rectangle<int> m_headerBounds;
    juce::Rectangle<int> m_controlsBounds;
    juce::Rectangle<int> m_timelineBounds;
    juce::Rectangle<int> m_rulerBounds;
    juce::Rectangle<int> m_statsBounds;
    
    // UI Components
    std::unique_ptr<juce::TextButton> m_resetButton;
    std::unique_ptr<juce::TextButton> m_lockRangeButton;
    std::unique_ptr<juce::TextButton> m_exportButton;
    std::unique_ptr<juce::ToggleButton> m_fillGapsToggle;
    std::unique_ptr<juce::ToggleButton> m_autoZoomToggle;
    
    // Colors - matching reference design
    juce::Colour m_backgroundColour{0xFF0D0D0D};
    juce::Colour m_toolbarColour{0xFF141414};
    juce::Colour m_timelineBackgroundColour{0xFF111111};
    juce::Colour m_coverageColour{0xFF939393};         // Gray for captured intervals
    juce::Colour m_partialDropoutColour{0xFFFFAA00};   // Amber for partial dropout
    juce::Colour m_totalDropoutColour{0xFFFF4444};     // Red for total dropout
    juce::Colour m_playheadColour{0xFFFFFFFF};         // White for playhead
    juce::Colour m_rulerColour{0xFF333333};
    juce::Colour m_textColour{0xFFCCCCCC};
    juce::Colour m_headerTextColour{0xFF808080};       // Dimmer header text
    juce::Colour m_borderColour{0xFF2A2A2A};
    juce::Colour m_buttonColour{0xFF1F1F1F};
    juce::Colour m_buttonActiveColour{0xFF939393};     // Gray for active state
    
    // Drawing methods
    void drawHeader(juce::Graphics& g);
    void drawTimeline(juce::Graphics& g);
    void drawCoverageIntervals(juce::Graphics& g);
    void drawDropouts(juce::Graphics& g);
    void drawPlayhead(juce::Graphics& g);
    void drawRuler(juce::Graphics& g);
    void drawStats(juce::Graphics& g);
    
    // Coordinate conversion
    int sampleToPixel(int64_t sample) const;
    int64_t pixelToSample(int pixel) const;
    
    // View management
    void fitToRange();
    void zoomAtPoint(float factor, int pixelX);
    void panView(int deltaPixels);
    void updateViewFromCoverage();
    void updateCache();
    
    // Helpers
    juce::String formatTime(double seconds) const;
    juce::String formatSamplePosition(int64_t sample, uint32_t sampleRate) const;
    
    // Custom look and feel for flat buttons
    FlatButtonLookAndFeel m_flatButtonLookAndFeel;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CaptureTimelinePanel)
};

} // namespace Mach1

