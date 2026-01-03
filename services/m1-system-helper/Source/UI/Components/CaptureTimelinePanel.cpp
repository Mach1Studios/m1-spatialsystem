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
    // Create buttons
    m_resetButton = std::make_unique<juce::TextButton>("Reset");
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
    
    m_lockRangeButton = std::make_unique<juce::TextButton>("Lock Range");
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
    
    m_exportButton = std::make_unique<juce::TextButton>("Export...");
    m_exportButton->onClick = [this]() {
        // TODO: Implement export dialog
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
            "Export", "Export functionality is not yet implemented.\nCaptured chunks are stored in the capture directory.");
        if (onExportClicked)
            onExportClicked();
    };
    addAndMakeVisible(m_exportButton.get());
    
    m_fillGapsToggle = std::make_unique<juce::ToggleButton>("Fill Gaps Only");
    m_fillGapsToggle->onClick = [this]() {
        m_fillGapsOnly = m_fillGapsToggle->getToggleState();
        // TODO: Implement fill-gaps-only capture mode
    };
    addAndMakeVisible(m_fillGapsToggle.get());
    
    m_statusLabel = std::make_unique<juce::Label>("status", "Not capturing");
    m_statusLabel->setColour(juce::Label::textColourId, m_textColour);
    m_statusLabel->setFont(juce::Font(12.0f));
    addAndMakeVisible(m_statusLabel.get());
    
    // Start timer for updates
    startTimerHz(30);  // 30 fps update rate
}

CaptureTimelinePanel::~CaptureTimelinePanel()
{
    stopTimer();
    
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
    
    // Layout control buttons
    auto controlsArea = m_controlsBounds.reduced(4, 4);
    m_resetButton->setBounds(controlsArea.removeFromLeft(60));
    controlsArea.removeFromLeft(4);
    m_lockRangeButton->setBounds(controlsArea.removeFromLeft(80));
    controlsArea.removeFromLeft(4);
    m_exportButton->setBounds(controlsArea.removeFromLeft(70));
    controlsArea.removeFromLeft(16);
    m_fillGapsToggle->setBounds(controlsArea.removeFromLeft(100));
    
    // Status label on the right
    m_statusLabel->setBounds(controlsArea.removeFromRight(200));
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
        // Update cache on message thread
        juce::MessageManager::callAsync([this]() {
            updateCache();
            repaint();
        });
    }
}

void CaptureTimelinePanel::timerCallback()
{
    if (m_engine && m_engine->isCapturing())
    {
        updateCache();
        
        // Auto-follow playhead if not locked
        if (m_autoFollow && !m_rangeLocked && !m_isDragging)
        {
            updateViewFromCoverage();
        }
        
        repaint();
    }
}

//==============================================================================
void CaptureTimelinePanel::drawHeader(juce::Graphics& g)
{
    g.setColour(m_timelineBackgroundColour);
    g.fillRect(m_headerBounds);
    
    g.setColour(m_headerTextColour);
    g.setFont(juce::Font(16.0f, juce::Font::bold));
    
    juce::String title = "Capture Timeline";
    if (m_engine && m_engine->isCapturing())
    {
        title += " - Session: " + m_engine->getSessionId();
    }
    
    g.drawText(title, m_headerBounds.reduced(8, 0), juce::Justification::centredLeft);
    
    // Draw capture indicator
    if (m_engine && m_engine->isCapturing())
    {
        g.setColour(juce::Colours::red);
        g.fillEllipse(m_headerBounds.getRight() - 24.0f, m_headerBounds.getCentreY() - 6.0f, 12.0f, 12.0f);
    }
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
    const juce::ScopedLock lock(m_cacheMutex);
    if (m_cachedData.anyCoverage.getIntervals().empty())
    {
        g.setColour(m_textColour.withAlpha(0.5f));
        g.setFont(juce::Font(14.0f));
        g.drawText("No captured data", m_timelineBounds, juce::Justification::centred);
    }
}

void CaptureTimelinePanel::drawCoverageIntervals(juce::Graphics& g)
{
    const juce::ScopedLock lock(m_cacheMutex);
    
    g.setColour(m_coverageColour);
    
    for (const auto& interval : m_cachedData.anyCoverage.getIntervals())
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
}

void CaptureTimelinePanel::drawDropouts(juce::Graphics& g)
{
    const juce::ScopedLock lock(m_cacheMutex);
    
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
}

void CaptureTimelinePanel::drawPlayhead(juce::Graphics& g)
{
    const juce::ScopedLock lock(m_cacheMutex);
    
    if (m_cachedData.latestSample <= 0)
        return;
    
    int x = sampleToPixel(m_cachedData.latestSample);
    
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
    
    const juce::ScopedLock lock(m_cacheMutex);
    uint32_t sampleRate = m_cachedData.sampleRate > 0 ? m_cachedData.sampleRate : 44100;
    
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
    const juce::ScopedLock lock(m_cacheMutex);
    
    g.setColour(m_textColour);
    g.setFont(juce::Font(11.0f));
    
    auto& stats = m_cachedData.stats;
    uint32_t sampleRate = m_cachedData.sampleRate > 0 ? m_cachedData.sampleRate : 44100;
    
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
    
    const juce::ScopedLock lock(m_cacheMutex);
    
    auto& coverageModel = m_engine->getCoverageModel();
    
    m_cachedData.stats = coverageModel.getGlobalStats();
    m_cachedData.anyCoverage = coverageModel.getAnyCoverage();
    m_cachedData.anyDropouts = coverageModel.getAnyDropouts();
    m_cachedData.allDropouts = coverageModel.getAllDropouts();
    m_cachedData.latestSample = coverageModel.getLatestSamplePosition();
    m_cachedData.sampleRate = coverageModel.getSampleRate();
    m_cachedData.capturing = m_engine->isCapturing();
    
    // Update status label
    if (m_cachedData.capturing)
    {
        m_statusLabel->setText("Capturing...", juce::dontSendNotification);
        m_statusLabel->setColour(juce::Label::textColourId, juce::Colours::lightgreen);
    }
    else
    {
        m_statusLabel->setText("Not capturing", juce::dontSendNotification);
        m_statusLabel->setColour(juce::Label::textColourId, m_textColour);
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

