/*
    CaptureTimelinePanel.cpp
    ------------------------
    Implementation of the capture timeline visualization panel.
*/

#include "CaptureTimelinePanel.h"

namespace Mach1 {

//==============================================================================
CaptureTimelinePanel::CaptureTimelinePanel()
{
    // Create buttons with flat styling (matching Soundfield Display)
    m_resetButton = std::make_unique<juce::TextButton>("RESET");
    m_resetButton->setLookAndFeel(&m_flatButtonLookAndFeel);
    m_resetButton->setColour(juce::TextButton::buttonColourId, m_buttonColour);
    m_resetButton->setColour(juce::TextButton::textColourOffId, m_textColour);
    m_resetButton->onClick = [this]() {
        if (m_engine)
        {
            m_engine->resetCoverage();
            m_viewStartSample = 0;
            m_viewEndSample = 0;
            updateCache();
            repaint();
        }
        if (onResetClicked)
            onResetClicked();
    };
    addAndMakeVisible(m_resetButton.get());
    
    m_lockRangeButton = std::make_unique<juce::TextButton>("LOCK");
    m_lockRangeButton->setLookAndFeel(&m_flatButtonLookAndFeel);
    m_lockRangeButton->setColour(juce::TextButton::buttonColourId, m_buttonColour);
    m_lockRangeButton->setColour(juce::TextButton::textColourOffId, m_textColour);
    m_lockRangeButton->setColour(juce::TextButton::buttonOnColourId, m_buttonActiveColour);
    m_lockRangeButton->setColour(juce::TextButton::textColourOnId, juce::Colour(0xFF0D0D0D));
    m_lockRangeButton->setClickingTogglesState(true);
    m_lockRangeButton->onClick = [this]() {
        m_rangeLocked = m_lockRangeButton->getToggleState();
        if (m_engine)
        {
            m_engine->getCoverageModel().setRangeLocked(m_rangeLocked);
            if (m_rangeLocked)
            {
                auto range = m_engine->getCoverageModel().getGlobalRange();
                m_engine->getCoverageModel().setGlobalRange(range.start, range.end);
            }
        }
    };
    addAndMakeVisible(m_lockRangeButton.get());
    
    m_exportButton = std::make_unique<juce::TextButton>("EXPORT");
    m_exportButton->setLookAndFeel(&m_flatButtonLookAndFeel);
    m_exportButton->setColour(juce::TextButton::buttonColourId, m_buttonColour);
    m_exportButton->setColour(juce::TextButton::textColourOffId, m_textColour);
    m_exportButton->onClick = [this]() {
        // TODO: Implement export dialog
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
            "Export", "Export functionality is not yet implemented.\nCaptured chunks are stored in the capture directory.");
        if (onExportClicked)
            onExportClicked();
    };
    addAndMakeVisible(m_exportButton.get());
    
    m_fillGapsToggle = std::make_unique<juce::ToggleButton>("FILL GAPS");
    m_fillGapsToggle->setLookAndFeel(&m_flatButtonLookAndFeel);
    m_fillGapsToggle->setColour(juce::ToggleButton::textColourId, m_textColour);
    m_fillGapsToggle->setColour(juce::ToggleButton::tickColourId, m_textColour);
    m_fillGapsToggle->setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour(0xFF666666));
    m_fillGapsToggle->onClick = [this]() {
        m_fillGapsOnly = m_fillGapsToggle->getToggleState();
        // TODO: Implement fill-gaps-only capture mode
    };
    addAndMakeVisible(m_fillGapsToggle.get());
    
    m_autoZoomToggle = std::make_unique<juce::ToggleButton>("AUTO ZOOM");
    m_autoZoomToggle->setLookAndFeel(&m_flatButtonLookAndFeel);
    m_autoZoomToggle->setColour(juce::ToggleButton::textColourId, m_textColour);
    m_autoZoomToggle->setColour(juce::ToggleButton::tickColourId, m_textColour);
    m_autoZoomToggle->setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour(0xFF666666));
    m_autoZoomToggle->setToggleState(true, juce::dontSendNotification);
    m_autoZoomToggle->onClick = [this]() {
        m_autoZoom = m_autoZoomToggle->getToggleState();
        if (m_autoZoom)
        {
            fitToRange();
        }
    };
    addAndMakeVisible(m_autoZoomToggle.get());
    
    // Start timer for updates - reduced frequency for performance
    startTimerHz(15);  // 15 fps update rate (was 30)
}

CaptureTimelinePanel::~CaptureTimelinePanel()
{
    stopTimer();
    
    // Clear look and feel before buttons are destroyed
    m_resetButton->setLookAndFeel(nullptr);
    m_lockRangeButton->setLookAndFeel(nullptr);
    m_exportButton->setLookAndFeel(nullptr);
    m_fillGapsToggle->setLookAndFeel(nullptr);
    m_autoZoomToggle->setLookAndFeel(nullptr);
    
    if (m_engine)
    {
        m_engine->removeChangeListener(this);
    }
}

void CaptureTimelinePanel::setCaptureEngine(CaptureEngine* engine)
{
    if (m_engine)
    {
        m_engine->removeChangeListener(this);
    }
    
    m_engine = engine;
    
    if (m_engine)
    {
        m_engine->addChangeListener(this);
        updateCache();
    }
    
    repaint();
}

//==============================================================================
void CaptureTimelinePanel::paint(juce::Graphics& g)
{
    g.fillAll(m_backgroundColour);
    
    drawHeader(g);
    drawTimeline(g);
    drawRuler(g);
    drawStats(g);
    
    // Draw border around entire panel
    g.setColour(m_borderColour);
    g.drawRect(getLocalBounds(), 1);
}

void CaptureTimelinePanel::resized()
{
    auto bounds = getLocalBounds();
    
    // Header at top
    m_headerBounds = bounds.removeFromTop(HEADER_HEIGHT);
    
    // Controls below header
    m_controlsBounds = bounds.removeFromTop(CONTROLS_HEIGHT);
    
    // Stats at bottom
    m_statsBounds = bounds.removeFromBottom(STATS_HEIGHT);
    
    // Ruler above stats
    m_rulerBounds = bounds.removeFromBottom(RULER_HEIGHT);
    
    // Timeline in the middle
    m_timelineBounds = bounds;
    
    // Layout control buttons - compact style
    auto controlsArea = m_controlsBounds.reduced(6, 5);
    int buttonHeight = 18;
    int buttonY = (m_controlsBounds.getHeight() - buttonHeight) / 2;
    
    m_resetButton->setBounds(controlsArea.removeFromLeft(50).withHeight(buttonHeight).withY(buttonY));
    controlsArea.removeFromLeft(3);
    m_lockRangeButton->setBounds(controlsArea.removeFromLeft(45).withHeight(buttonHeight).withY(buttonY));
    controlsArea.removeFromLeft(3);
    m_exportButton->setBounds(controlsArea.removeFromLeft(50).withHeight(buttonHeight).withY(buttonY));
    controlsArea.removeFromLeft(10);
    m_fillGapsToggle->setBounds(controlsArea.removeFromLeft(80).withHeight(buttonHeight).withY(buttonY));
    controlsArea.removeFromLeft(6);
    m_autoZoomToggle->setBounds(controlsArea.removeFromLeft(70).withHeight(buttonHeight).withY(buttonY));
}

//==============================================================================
void CaptureTimelinePanel::mouseDown(const juce::MouseEvent& event)
{
    if (m_timelineBounds.contains(event.getPosition()) || m_rulerBounds.contains(event.getPosition()))
    {
        m_isDragging = true;
        m_lastMousePos = event.getPosition();
        m_dragStartViewStart = m_viewStartSample;
    }
}

void CaptureTimelinePanel::mouseDrag(const juce::MouseEvent& event)
{
    if (m_isDragging)
    {
        int deltaX = event.getPosition().x - m_lastMousePos.x;
        panView(-deltaX);
        m_lastMousePos = event.getPosition();
    }
}

void CaptureTimelinePanel::mouseUp(const juce::MouseEvent& event)
{
    m_isDragging = false;
}

void CaptureTimelinePanel::mouseDoubleClick(const juce::MouseEvent& event)
{
    if (m_timelineBounds.contains(event.getPosition()))
    {
        fitToRange();
    }
}

void CaptureTimelinePanel::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (m_timelineBounds.contains(event.getPosition()) || m_rulerBounds.contains(event.getPosition()))
    {
        float zoomFactor = 1.0f - wheel.deltaY * 0.1f;
        zoomAtPoint(zoomFactor, event.x - m_timelineBounds.getX());
        repaint();
    }
}

//==============================================================================
void CaptureTimelinePanel::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == m_engine)
    {
        // Mark that cache needs update - timer will handle it to batch updates
        m_cacheUpdatePending.store(true);
    }
}

void CaptureTimelinePanel::timerCallback()
{
    if (m_engine && m_engine->isCapturing())
    {
        // Only update if there's pending data or periodically (every 4th frame = ~4Hz background)
        static int frameCounter = 0;
        bool shouldUpdate = m_cacheUpdatePending.exchange(false) || (++frameCounter % 4 == 0);
        
        if (shouldUpdate)
        {
            updateCache();
        }
        
        // Auto-zoom to fit all captured data
        if (m_autoZoom && !m_isDragging)
        {
            fitToRange();
        }
        // Auto-follow playhead if not zooming
        else if (m_autoFollow && !m_rangeLocked && !m_isDragging)
        {
            updateViewFromCoverage();
        }
        
        if (shouldUpdate)
        {
            repaint();
        }
    }
}

//==============================================================================
void CaptureTimelinePanel::drawHeader(juce::Graphics& g)
{
    // Toolbar background
    g.setColour(m_toolbarColour);
    g.fillRect(m_headerBounds);
    
    // Section title - dimmer style like reference
    g.setColour(m_headerTextColour);
    g.setFont(juce::Font(11.0f, juce::Font::bold));
    
    juce::String title = "CAPTURE TIMELINE";
    g.drawText(title, m_headerBounds.withLeft(10), juce::Justification::centredLeft);
    
    // Session ID on the right (if capturing)
    if (m_engine && m_engine->isCapturing())
    {
        g.setFont(juce::Font(10.0f));
        g.setColour(m_textColour.withAlpha(0.7f));
        g.drawText(m_engine->getSessionId(), m_headerBounds.reduced(10, 0).withRight(m_headerBounds.getRight() - 30), 
                   juce::Justification::centredRight);
        
        // Draw capture indicator (red dot when capturing - like a record indicator)
        g.setColour(juce::Colour(0xFFFF4444));
        g.fillEllipse(m_headerBounds.getRight() - 20.0f, m_headerBounds.getCentreY() - 4.0f, 8.0f, 8.0f);
    }
    
    // Bottom border
    g.setColour(m_borderColour);
    g.drawHorizontalLine(m_headerBounds.getBottom() - 1, 0, (float)getWidth());
}

void CaptureTimelinePanel::drawTimeline(juce::Graphics& g)
{
    // Background
    g.setColour(m_timelineBackgroundColour);
    g.fillRect(m_timelineBounds);
    
    // Border
    g.setColour(m_rulerColour);
    g.drawRect(m_timelineBounds);
    
    // Draw coverage and dropouts
    drawCoverageIntervals(g);
    drawDropouts(g);
    drawPlayhead(g);
    
    // Draw placeholder if no data
    if (m_cacheMutex.tryEnter())
    {
        if (m_cachedData.coverageIntervals.empty())
        {
            g.setColour(m_textColour.withAlpha(0.5f));
            g.setFont(juce::Font(14.0f));
            g.drawText("No captured data", m_timelineBounds, juce::Justification::centred);
        }
        m_cacheMutex.exit();
    }
}

void CaptureTimelinePanel::drawCoverageIntervals(juce::Graphics& g)
{
    // Use try-lock to avoid blocking paint on slow cache updates
    if (m_cacheMutex.tryEnter())
    {
        g.setColour(m_coverageColour);
        
        for (const auto& interval : m_cachedData.coverageIntervals)
        {
            int x1 = sampleToPixel(interval.start);
            int x2 = sampleToPixel(interval.end);
            
            if (x2 > m_timelineBounds.getX() && x1 < m_timelineBounds.getRight())
            {
                x1 = std::max(x1, m_timelineBounds.getX());
                x2 = std::min(x2, m_timelineBounds.getRight());
                
                g.fillRect(x1, m_timelineBounds.getY() + 4, x2 - x1, m_timelineBounds.getHeight() - 8);
            }
        }
        
        m_cacheMutex.exit();
    }
}

void CaptureTimelinePanel::drawDropouts(juce::Graphics& g)
{
    // Use try-lock to avoid blocking paint
    if (!m_cacheMutex.tryEnter())
        return;
    
    // Draw partial dropouts (some panners missing)
    g.setColour(m_partialDropoutColour.withAlpha(0.6f));
    for (const auto& dropout : m_cachedData.anyDropouts)
    {
        int x1 = sampleToPixel(dropout.start);
        int x2 = sampleToPixel(dropout.end);
        
        if (x2 > m_timelineBounds.getX() && x1 < m_timelineBounds.getRight())
        {
            x1 = std::max(x1, m_timelineBounds.getX());
            x2 = std::min(x2, m_timelineBounds.getRight());
            
            // Draw hash pattern for partial dropout
            for (int x = x1; x < x2; x += 4)
            {
                g.drawLine((float)x, (float)m_timelineBounds.getY() + 4,
                          (float)x, (float)m_timelineBounds.getBottom() - 4, 1.0f);
            }
        }
    }
    
    // Draw total dropouts (all panners missing)
    g.setColour(m_totalDropoutColour);
    for (const auto& dropout : m_cachedData.allDropouts)
    {
        int x1 = sampleToPixel(dropout.start);
        int x2 = sampleToPixel(dropout.end);
        
        if (x2 > m_timelineBounds.getX() && x1 < m_timelineBounds.getRight())
        {
            x1 = std::max(x1, m_timelineBounds.getX());
            x2 = std::min(x2, m_timelineBounds.getRight());
            
            g.fillRect(x1, m_timelineBounds.getY() + 4, x2 - x1, m_timelineBounds.getHeight() - 8);
        }
    }
    
    m_cacheMutex.exit();
}

void CaptureTimelinePanel::drawPlayhead(juce::Graphics& g)
{
    // Use try-lock to avoid blocking paint
    if (!m_cacheMutex.tryEnter())
        return;
    
    int64_t latestSample = m_cachedData.latestSample;
    m_cacheMutex.exit();
    
    if (latestSample <= 0)
        return;
    
    int x = sampleToPixel(latestSample);
    
    if (x >= m_timelineBounds.getX() && x <= m_timelineBounds.getRight())
    {
        g.setColour(m_playheadColour);
        g.drawLine((float)x, (float)m_timelineBounds.getY(),
                  (float)x, (float)m_timelineBounds.getBottom(), 2.0f);
        
        // Playhead triangle at top
        juce::Path triangle;
        triangle.addTriangle((float)x - 5, (float)m_timelineBounds.getY(),
                            (float)x + 5, (float)m_timelineBounds.getY(),
                            (float)x, (float)m_timelineBounds.getY() + 8);
        g.fillPath(triangle);
    }
}

void CaptureTimelinePanel::drawRuler(juce::Graphics& g)
{
    g.setColour(m_timelineBackgroundColour.darker(0.2f));
    g.fillRect(m_rulerBounds);
    
    g.setColour(m_rulerColour);
    g.drawRect(m_rulerBounds);
    
    // Calculate tick interval based on view range
    int64_t viewRange = m_viewEndSample - m_viewStartSample;
    if (viewRange <= 0)
        viewRange = 1;
    
    // Get sample rate without blocking
    uint32_t sampleRate = 44100;
    if (m_cacheMutex.tryEnter())
    {
        sampleRate = m_cachedData.sampleRate > 0 ? m_cachedData.sampleRate : 44100;
        m_cacheMutex.exit();
    }
    
    // Find a nice tick interval (in samples)
    double viewSeconds = static_cast<double>(viewRange) / sampleRate;
    double tickIntervalSeconds = 1.0;  // Default 1 second ticks
    
    if (viewSeconds > 300) tickIntervalSeconds = 60.0;      // 5+ min: 1 min ticks
    else if (viewSeconds > 60) tickIntervalSeconds = 10.0;  // 1+ min: 10 sec ticks
    else if (viewSeconds > 10) tickIntervalSeconds = 1.0;   // 10+ sec: 1 sec ticks
    else if (viewSeconds > 1) tickIntervalSeconds = 0.1;    // 1+ sec: 100ms ticks
    else tickIntervalSeconds = 0.01;                         // <1 sec: 10ms ticks
    
    int64_t tickIntervalSamples = static_cast<int64_t>(tickIntervalSeconds * sampleRate);
    if (tickIntervalSamples <= 0)
        tickIntervalSamples = sampleRate;
    
    // Draw ticks
    int64_t firstTick = (m_viewStartSample / tickIntervalSamples) * tickIntervalSamples;
    
    g.setColour(m_textColour);
    g.setFont(juce::Font(10.0f));
    
    for (int64_t tick = firstTick; tick <= m_viewEndSample; tick += tickIntervalSamples)
    {
        int x = sampleToPixel(tick);
        
        if (x >= m_rulerBounds.getX() && x <= m_rulerBounds.getRight())
        {
            // Draw tick mark
            g.setColour(m_rulerColour);
            g.drawLine((float)x, (float)m_rulerBounds.getY(),
                      (float)x, (float)m_rulerBounds.getY() + 6, 1.0f);
            
            // Draw label
            g.setColour(m_textColour);
            double seconds = static_cast<double>(tick) / sampleRate;
            g.drawText(formatTime(seconds), x - 25, m_rulerBounds.getY() + 6, 50, 16,
                      juce::Justification::centred);
        }
    }
}

void CaptureTimelinePanel::drawStats(juce::Graphics& g)
{
    // Use try-lock to avoid blocking paint
    if (!m_cacheMutex.tryEnter())
        return;
    
    auto stats = m_cachedData.stats;
    uint32_t sampleRate = m_cachedData.sampleRate > 0 ? m_cachedData.sampleRate : 44100;
    m_cacheMutex.exit();
    
    g.setColour(m_textColour);
    g.setFont(juce::Font(11.0f));
    
    double capturedSeconds = static_cast<double>(stats.totalCapturedSamples) / sampleRate;
    double totalSeconds = static_cast<double>(stats.totalRangeSamples) / sampleRate;
    double dropoutSeconds = static_cast<double>(stats.totalDropoutSamples) / sampleRate;
    
    juce::String statsText;
    statsText << "Coverage: " << juce::String(stats.anyCoveragePercent(), 1) << "% | ";
    statsText << "Captured: " << formatTime(capturedSeconds) << " | ";
    statsText << "Missing: " << formatTime(dropoutSeconds) << " | ";
    statsText << "Dropouts: " << static_cast<int>(stats.totalDropoutsDetected) << " | ";
    statsText << "Panners: " << static_cast<int>(stats.pannerCount) << " | ";
    statsText << "Blocks: " << static_cast<int>(stats.totalBlocksReceived);
    
    g.drawText(statsText, m_statsBounds.reduced(8, 0), juce::Justification::centredLeft);
}

//==============================================================================
int CaptureTimelinePanel::sampleToPixel(int64_t sample) const
{
    if (m_viewEndSample == m_viewStartSample)
        return m_timelineBounds.getX();
    
    double ratio = static_cast<double>(sample - m_viewStartSample) / (m_viewEndSample - m_viewStartSample);
    return m_timelineBounds.getX() + static_cast<int>(ratio * m_timelineBounds.getWidth());
}

int64_t CaptureTimelinePanel::pixelToSample(int pixel) const
{
    double ratio = static_cast<double>(pixel - m_timelineBounds.getX()) / m_timelineBounds.getWidth();
    return m_viewStartSample + static_cast<int64_t>(ratio * (m_viewEndSample - m_viewStartSample));
}

//==============================================================================
void CaptureTimelinePanel::fitToRange()
{
    if (!m_engine)
        return;
    
    auto range = m_engine->getCoverageModel().getGlobalRange();
    
    if (range.isEmpty())
    {
        m_viewStartSample = 0;
        m_viewEndSample = m_cachedData.sampleRate * 10;  // 10 seconds default
    }
    else
    {
        // Add 5% padding
        int64_t padding = std::max(range.length() / 20, static_cast<int64_t>(m_cachedData.sampleRate));
        m_viewStartSample = range.start - padding;
        m_viewEndSample = range.end + padding;
    }
    
    repaint();
}

void CaptureTimelinePanel::zoomAtPoint(float factor, int pixelX)
{
    // Calculate the sample position at the mouse point
    int64_t sampleAtMouse = pixelToSample(pixelX + m_timelineBounds.getX());
    
    // Calculate new range
    int64_t currentRange = m_viewEndSample - m_viewStartSample;
    int64_t newRange = static_cast<int64_t>(currentRange * factor);
    
    // Clamp minimum range (0.1 seconds at current sample rate)
    int64_t minRange = m_cachedData.sampleRate / 10;
    newRange = std::max(newRange, minRange);
    
    // Maintain the mouse position in the view
    double mouseRatio = static_cast<double>(pixelX) / m_timelineBounds.getWidth();
    m_viewStartSample = sampleAtMouse - static_cast<int64_t>(mouseRatio * newRange);
    m_viewEndSample = m_viewStartSample + newRange;
}

void CaptureTimelinePanel::panView(int deltaPixels)
{
    int64_t viewRange = m_viewEndSample - m_viewStartSample;
    int64_t sampleDelta = static_cast<int64_t>(deltaPixels * viewRange / m_timelineBounds.getWidth());
    
    m_viewStartSample += sampleDelta;
    m_viewEndSample += sampleDelta;
    
    repaint();
}

void CaptureTimelinePanel::updateViewFromCoverage()
{
    if (!m_engine || m_rangeLocked)
        return;
    
    auto range = m_engine->getCoverageModel().getGlobalRange();
    
    if (range.isEmpty())
        return;
    
    // If view is empty, fit to range
    if (m_viewEndSample <= m_viewStartSample)
    {
        fitToRange();
        return;
    }
    
    // If playhead is beyond view, scroll to follow
    int64_t latestSample = m_engine->getCoverageModel().getLatestSamplePosition();
    if (latestSample > m_viewEndSample - (m_viewEndSample - m_viewStartSample) / 10)
    {
        int64_t viewRange = m_viewEndSample - m_viewStartSample;
        m_viewEndSample = latestSample + viewRange / 10;
        m_viewStartSample = m_viewEndSample - viewRange;
    }
}

void CaptureTimelinePanel::updateCache()
{
    if (!m_engine)
        return;
    
    auto& coverageModel = m_engine->getCoverageModel();
    
    // Get lightweight stats first (fast, minimal locking)
    auto stats = coverageModel.getGlobalStats();
    auto latestSample = coverageModel.getLatestSamplePosition();
    auto sampleRate = coverageModel.getSampleRate();
    bool capturing = m_engine->isCapturing();
    
    // Get coverage intervals (this may take longer)
    auto anyCoverage = coverageModel.getAnyCoverage();
    
    // Now update the cached data with a short lock
    {
        const juce::ScopedLock lock(m_cacheMutex);
        
        m_cachedData.stats = stats;
        m_cachedData.coverageIntervals = anyCoverage.getIntervals();
        m_cachedData.anyDropouts = coverageModel.getAnyDropouts();
        m_cachedData.allDropouts = coverageModel.getAllDropouts();
        m_cachedData.latestSample = latestSample;
        m_cachedData.sampleRate = sampleRate;
        m_cachedData.capturing = capturing;
    }
}

//==============================================================================
juce::String CaptureTimelinePanel::formatTime(double seconds) const
{
    if (seconds < 0)
        seconds = 0;
    
    int minutes = static_cast<int>(seconds) / 60;
    int secs = static_cast<int>(seconds) % 60;
    int millis = static_cast<int>((seconds - static_cast<int>(seconds)) * 1000);
    
    if (minutes > 0)
    {
        return juce::String::formatted("%d:%02d.%03d", minutes, secs, millis);
    }
    else
    {
        return juce::String::formatted("%d.%03d", secs, millis);
    }
}

juce::String CaptureTimelinePanel::formatSamplePosition(int64_t sample, uint32_t sampleRate) const
{
    if (sampleRate == 0)
        sampleRate = 44100;
    
    double seconds = static_cast<double>(sample) / sampleRate;
    return formatTime(seconds) + " (" + juce::String(sample) + ")";
}

} // namespace Mach1

