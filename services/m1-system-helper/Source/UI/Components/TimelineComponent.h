/*
    TimelineComponent.h
    -------------------
    UI component that displays buffer acknowledgment timeline for M1MemoryShare tracking.
    
    Features:
    - Shows white lines for acknowledged audio buffers
    - Shows red lines for missing buffers (gaps in sequence)
    - Timeline from left (earliest) to right (latest)
    - Real-time updates as buffers are consumed
    - Visual feedback for buffer tracking performance
    
    Timeline Elements:
    - White vertical lines: Successfully acknowledged buffers
    - Red vertical lines: Missing buffers in sequence
    - Timeline scrubber: Shows current playhead position
    - Buffer ID labels: Shows buffer identifiers
    - Timestamp labels: Shows buffer timestamps
*/

#pragma once

#include <JuceHeader.h>
#include "../../Managers/PannerTrackingManager.h"
#include <vector>
#include <map>
#include <memory>

namespace Mach1 {

/**
 * Structure to represent a buffer event in the timeline
 */
struct BufferEvent {
    uint64_t bufferId;
    uint64_t timestamp;
    uint32_t sequenceNumber;
    bool isAcknowledged;
    bool isMissing;
    std::string pannerName;
    
    // Default constructor
    BufferEvent() : bufferId(0), timestamp(0), sequenceNumber(0), isAcknowledged(false), isMissing(false), pannerName("") {}
    
    BufferEvent(uint64_t id, uint64_t ts, uint32_t seq, bool ack, bool miss, const std::string& name)
        : bufferId(id), timestamp(ts), sequenceNumber(seq), isAcknowledged(ack), isMissing(miss), pannerName(name) {}
};

/**
 * Timeline component that visualizes buffer acknowledgment status
 */
class TimelineComponent : public juce::Component,
                         public juce::Timer
{
public:
    explicit TimelineComponent();
    ~TimelineComponent() override;
    
    // Data updates
    void updateBufferEvents(const std::vector<PannerInfo>& panners);
    void addBufferEvent(const BufferEvent& event);
    void clearTimeline();
    
    // Timeline control (fixed bounds based on DAW timestamps)
    void setTimeWindow(uint64_t startTime, uint64_t endTime);
    uint64_t getEarliestTimestamp() const { return earliestDawTimestamp; }
    uint64_t getLatestTimestamp() const { return latestDawTimestamp; }
    
    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    
    // Timer for auto-updates
    void timerCallback() override;
    
    // Callbacks
    std::function<void(uint64_t)> onBufferSelected;
    std::function<void(uint64_t, uint64_t)> onTimeRangeChanged;

private:
    // Data
    std::vector<BufferEvent> bufferEvents;
    std::map<uint64_t, BufferEvent> bufferEventMap;
    uint64_t timeWindowStart = 0;
    uint64_t timeWindowEnd = 0;
    uint64_t currentPlayheadTime = 0;
    uint64_t earliestDawTimestamp = UINT64_MAX;
    uint64_t latestDawTimestamp = 0;
    
    // UI state
    uint64_t selectedBufferId = 0;
    bool isDragging = false;
    juce::Point<int> lastMousePosition;
    
    // Drawing
    void drawTimeline(juce::Graphics& g);
    void drawBufferEvents(juce::Graphics& g);
    void drawPlayheadCursor(juce::Graphics& g);
    void drawTimeLabels(juce::Graphics& g);
    void drawBufferTooltip(juce::Graphics& g, const BufferEvent& event, juce::Point<int> position);
    
    // Calculations
    int timeToPixel(uint64_t timestamp) const;
    uint64_t pixelToTime(int pixel) const;
    BufferEvent* getBufferEventAtPixel(int pixel);
    void updateTimeWindow();
    void detectMissingBuffers();
    
    // Layout
    juce::Rectangle<int> timelineBounds;
    juce::Rectangle<int> bufferAreaBounds;
    juce::Rectangle<int> timeLabelBounds;
    
    // Styling
    juce::Colour backgroundColour{0xFF1A1A1A};
    juce::Colour timelineColour{0xFF404040};
    juce::Colour acknowledgedBufferColour{0xFFFFFFFF};  // White for acknowledged
    juce::Colour missingBufferColour{0xFFFF0000};       // Red for missing
    juce::Colour playheadColour{0xFF00FF00};            // Green for playhead
    juce::Colour selectedBufferColour{0xFF0078D4};      // Blue for selection
    juce::Colour textColour{0xFFE0E0E0};
    
    // Configuration
    static constexpr int BUFFER_LINE_WIDTH = 2;
    static constexpr int PLAYHEAD_LINE_WIDTH = 3;
    static constexpr int TIME_LABEL_HEIGHT = 20;
    static constexpr int BUFFER_AREA_HEIGHT = 100;
    static constexpr uint64_t DEFAULT_TIME_WINDOW = 10000; // 10 seconds in milliseconds
    static constexpr int UPDATE_INTERVAL_MS = 100;
    
    // Tooltip
    std::unique_ptr<juce::Label> tooltipLabel;
    BufferEvent* hoveredEvent = nullptr;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TimelineComponent)
};

} // namespace Mach1 