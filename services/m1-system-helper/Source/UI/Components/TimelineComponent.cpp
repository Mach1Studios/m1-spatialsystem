/*
    TimelineComponent.cpp
    ---------------------
    Implementation of the buffer acknowledgment timeline component
*/

#include "TimelineComponent.h"

namespace Mach1 {

//==============================================================================
TimelineComponent::TimelineComponent()
{
    // Create tooltip label
    tooltipLabel = std::make_unique<juce::Label>("tooltip", "");
    tooltipLabel->setFont(juce::Font(10.0f));
    tooltipLabel->setColour(juce::Label::textColourId, textColour);
    tooltipLabel->setColour(juce::Label::backgroundColourId, juce::Colours::black.withAlpha(0.8f));
    tooltipLabel->setJustificationType(juce::Justification::centred);
    tooltipLabel->setVisible(false);
    addAndMakeVisible(tooltipLabel.get());
    
    // Set default time window
    auto now = juce::Time::getCurrentTime().toMilliseconds();
    timeWindowStart = now - DEFAULT_TIME_WINDOW;
    timeWindowEnd = now;
    
    // Start timer for updates
    startTimer(UPDATE_INTERVAL_MS);
}

TimelineComponent::~TimelineComponent()
{
    stopTimer();
}

//==============================================================================
void TimelineComponent::updateBufferEvents(const std::vector<PannerInfo>& panners)
{
    // Extract buffer events from panner data and update DAW timestamp bounds
    for (const auto& panner : panners)
    {
        if (panner.isMemoryShareBased && panner.dawTimestamp > 0)
        {
            // Update earliest and latest DAW timestamps
            if (panner.dawTimestamp < earliestDawTimestamp)
                earliestDawTimestamp = panner.dawTimestamp;
            if (panner.dawTimestamp > latestDawTimestamp)
                latestDawTimestamp = panner.dawTimestamp;
            
            // Create buffer event for this panner
            BufferEvent event(
                panner.currentBufferId,
                panner.dawTimestamp,
                0, // sequenceNumber - not available in PannerInfo, using 0
                true,  // Acknowledged (we received it)
                false, // Not missing
                panner.name
            );
            
            addBufferEvent(event);
        }
    }
    
    // Update time window to match the DAW timestamp bounds
    if (earliestDawTimestamp != UINT64_MAX && latestDawTimestamp > 0)
    {
        // Add some padding (5% on each side) for better visualization
        uint64_t range = latestDawTimestamp - earliestDawTimestamp;
        uint64_t padding = range > 0 ? range * 0.05 : 1000; // 5% padding or 1 second minimum
        
        timeWindowStart = earliestDawTimestamp - padding;
        timeWindowEnd = latestDawTimestamp + padding;
    }
    
    // Detect missing buffers
    detectMissingBuffers();
    
    repaint();
}

void TimelineComponent::addBufferEvent(const BufferEvent& event)
{
    // Add to main vector
    bufferEvents.push_back(event);
    
    // Add to map for quick lookup
    bufferEventMap[event.bufferId] = event;
    
    // Keep only recent events (last 1000 events)
    if (bufferEvents.size() > 1000)
    {
        bufferEvents.erase(bufferEvents.begin());
    }
    
    // Sort by timestamp
    std::sort(bufferEvents.begin(), bufferEvents.end(), 
              [](const BufferEvent& a, const BufferEvent& b) {
                  return a.timestamp < b.timestamp;
              });
}

void TimelineComponent::clearTimeline()
{
    bufferEvents.clear();
    bufferEventMap.clear();
    repaint();
}

//==============================================================================
void TimelineComponent::setTimeWindow(uint64_t startTime, uint64_t endTime)
{
    timeWindowStart = startTime;
    timeWindowEnd = endTime;
    repaint();
}

//==============================================================================
void TimelineComponent::paint(juce::Graphics& g)
{
    g.fillAll(backgroundColour);
    
    // Draw timeline
    drawTimeline(g);
    
    // Draw buffer events
    drawBufferEvents(g);
    
    // Draw playhead cursor
    drawPlayheadCursor(g);
    
    // Draw time labels
    drawTimeLabels(g);
}

void TimelineComponent::resized()
{
    auto bounds = getLocalBounds();
    
    // Time labels at bottom
    timeLabelBounds = bounds.removeFromBottom(TIME_LABEL_HEIGHT);
    
    // Buffer area in middle
    bufferAreaBounds = bounds.removeFromTop(BUFFER_AREA_HEIGHT);
    
    // Timeline area is remaining
    timelineBounds = bounds;
}

//==============================================================================
void TimelineComponent::mouseDown(const juce::MouseEvent& event)
{
    if (bufferAreaBounds.contains(event.getPosition()))
    {
        // Check if clicking on a buffer event
        if (auto* bufferEvent = getBufferEventAtPixel(event.x))
        {
            selectedBufferId = bufferEvent->bufferId;
            
            if (onBufferSelected)
                onBufferSelected(selectedBufferId);
        }
    }
    
    isDragging = true;
    lastMousePosition = event.getPosition();
}

void TimelineComponent::mouseDrag(const juce::MouseEvent& event)
{
    if (isDragging)
    {
        auto delta = event.getPosition() - lastMousePosition;
        
        // Pan timeline based on horizontal drag
        uint64_t timeDelta = static_cast<uint64_t>(-delta.x * (timeWindowEnd - timeWindowStart) / getWidth());
        
        timeWindowStart += timeDelta;
        timeWindowEnd += timeDelta;
        

        
        lastMousePosition = event.getPosition();
        repaint();
    }
}



//==============================================================================
void TimelineComponent::timerCallback()
{
    // Timer only used for updating current playhead position
    // Timeline bounds are now fixed to DAW timestamp range
    repaint();
}

//==============================================================================
void TimelineComponent::drawTimeline(juce::Graphics& g)
{
    // Draw timeline background
    g.setColour(timelineColour);
    g.fillRect(timelineBounds);
    
    // Draw timeline border
    g.setColour(timelineColour.brighter(0.3f));
    g.drawRect(timelineBounds, 1);
    
    // Draw grid lines
    g.setColour(timelineColour.brighter(0.1f));
    
    // Vertical grid lines (time marks)
    uint64_t timeRange = timeWindowEnd - timeWindowStart;
    uint64_t gridInterval = timeRange / 10;
    
    for (int i = 0; i <= 10; ++i)
    {
        uint64_t gridTime = timeWindowStart + (i * gridInterval);
        int x = timeToPixel(gridTime);
        
        g.drawLine(x, timelineBounds.getY(), x, timelineBounds.getBottom(), 1);
    }
}

void TimelineComponent::drawBufferEvents(juce::Graphics& g)
{
    for (const auto& event : bufferEvents)
    {
        // Only draw events within time window
        if (event.timestamp >= timeWindowStart && event.timestamp <= timeWindowEnd)
        {
            int x = timeToPixel(event.timestamp);
            
            // Choose color based on event type
            juce::Colour eventColour;
            if (event.isMissing)
                eventColour = missingBufferColour;
            else if (event.isAcknowledged)
                eventColour = acknowledgedBufferColour;
            else
                eventColour = juce::Colours::grey;
            
            // Highlight selected buffer
            if (event.bufferId == selectedBufferId)
                eventColour = selectedBufferColour;
            
            g.setColour(eventColour);
            g.drawLine(x, bufferAreaBounds.getY(), x, bufferAreaBounds.getBottom(), BUFFER_LINE_WIDTH);
        }
    }
}

void TimelineComponent::drawPlayheadCursor(juce::Graphics& g)
{
    // Draw playhead at current time
    currentPlayheadTime = juce::Time::getCurrentTime().toMilliseconds();
    
    if (currentPlayheadTime >= timeWindowStart && currentPlayheadTime <= timeWindowEnd)
    {
        int x = timeToPixel(currentPlayheadTime);
        
        g.setColour(playheadColour);
        g.drawLine(x, bufferAreaBounds.getY(), x, bufferAreaBounds.getBottom(), PLAYHEAD_LINE_WIDTH);
    }
}

void TimelineComponent::drawTimeLabels(juce::Graphics& g)
{
    g.setColour(textColour);
    g.setFont(juce::Font(10.0f));
    
    // Draw time labels
    uint64_t timeRange = timeWindowEnd - timeWindowStart;
    uint64_t labelInterval = timeRange / 5;
    
    for (int i = 0; i <= 5; ++i)
    {
        uint64_t labelTime = timeWindowStart + (i * labelInterval);
        int x = timeToPixel(labelTime);
        
        // Format time as HH:MM:SS
        juce::Time time(labelTime);
        juce::String timeStr = time.toString(false, true, true, true);
        
        g.drawText(timeStr, x - 30, timeLabelBounds.getY(), 60, timeLabelBounds.getHeight(),
                   juce::Justification::centred, true);
    }
}

//==============================================================================
int TimelineComponent::timeToPixel(uint64_t timestamp) const
{
    if (timeWindowEnd == timeWindowStart)
        return 0;
    
    float ratio = static_cast<float>(timestamp - timeWindowStart) / (timeWindowEnd - timeWindowStart);
    return static_cast<int>(ratio * getWidth());
}

uint64_t TimelineComponent::pixelToTime(int pixel) const
{
    float ratio = static_cast<float>(pixel) / getWidth();
    return timeWindowStart + static_cast<uint64_t>(ratio * (timeWindowEnd - timeWindowStart));
}

BufferEvent* TimelineComponent::getBufferEventAtPixel(int pixel)
{
    uint64_t clickTime = pixelToTime(pixel);
    
    // Find closest buffer event within a tolerance
    const uint64_t tolerance = 100; // 100ms tolerance
    
    for (auto& event : bufferEvents)
    {
        if (abs(static_cast<int64_t>(event.timestamp - clickTime)) < tolerance)
        {
            return &event;
        }
    }
    
    return nullptr;
}

void TimelineComponent::updateTimeWindow()
{
    auto now = juce::Time::getCurrentTime().toMilliseconds();
    uint64_t windowSize = timeWindowEnd - timeWindowStart;
    
    timeWindowEnd = now;
    timeWindowStart = now - windowSize;
}

void TimelineComponent::detectMissingBuffers()
{
    // Group events by panner
    std::map<std::string, std::vector<BufferEvent*>> pannerEvents;
    
    for (auto& event : bufferEvents)
    {
        pannerEvents[event.pannerName].push_back(&event);
    }
    
    // Check for missing sequence numbers in each panner
    for (auto& [pannerName, events] : pannerEvents)
    {
        if (events.size() < 2)
            continue;
        
        // Sort by sequence number
        std::sort(events.begin(), events.end(), 
                  [](const BufferEvent* a, const BufferEvent* b) {
                      return a->sequenceNumber < b->sequenceNumber;
                  });
        
        // Find gaps in sequence
        for (int i = 1; i < events.size(); ++i)
        {
            uint32_t expectedSeq = events[i-1]->sequenceNumber + 1;
            uint32_t actualSeq = events[i]->sequenceNumber;
            
            // Create missing buffer events for gaps
            while (expectedSeq < actualSeq)
            {
                uint64_t estimatedTime = events[i-1]->timestamp + 
                    ((events[i]->timestamp - events[i-1]->timestamp) * (expectedSeq - events[i-1]->sequenceNumber)) / 
                    (actualSeq - events[i-1]->sequenceNumber);
                
                BufferEvent missingEvent(
                    0, // No buffer ID for missing events
                    estimatedTime,
                    expectedSeq,
                    false, // Not acknowledged
                    true,  // Missing
                    pannerName
                );
                
                bufferEvents.push_back(missingEvent);
                expectedSeq++;
            }
        }
    }
}

} // namespace Mach1 