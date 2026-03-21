#include "MonitorPanel.h"

#include <cmath>

namespace Mach1 {
namespace {

constexpr double kReferenceDragSpeed = 250.0;
constexpr double kFineTuneDragMultiplier = 50.0;
constexpr int kCompactPanelMinWidth = 380;
constexpr int kCompactPanelMinHeight = 130;
constexpr int kPopupItemHeight = 20;

juce::String formatAngle(double value)
{
    const double displayValue = std::abs(value) < 0.05 ? 0.0 : value;
    return juce::String(displayValue, 1)
        + juce::String::charToString(static_cast<juce::juce_wchar>(0x00B0));
}

double getEffectiveDragSpeed(const juce::ModifierKeys& modifiers)
{
    auto speed = kReferenceDragSpeed;
    if (modifiers.isShiftDown() || modifiers.isCtrlDown() || modifiers.isCommandDown())
        speed *= kFineTuneDragMultiplier;

    return speed;
}

double wrapValue(double value, double minimum, double maximum)
{
    const auto range = maximum - minimum;
    if (range <= 0.0)
        return value;

    while (value > maximum)
        value -= range;

    while (value < minimum)
        value += range;

    return value;
}

bool shouldUseCompactMonitorView(const juce::Component& component)
{
    return component.getWidth() < kCompactPanelMinWidth || component.getHeight() < kCompactPanelMinHeight;
}

juce::String formatChannelCountLabel(int channelCount)
{
    switch (channelCount)
    {
        case 4:  return "M1Spatial-4";
        case 8:  return "M1Spatial-8";
        case 14: return "M1Spatial-14";
        default: return juce::String(channelCount) + " channels";
    }
}

class MonitorPopupLookAndFeel : public juce::LookAndFeel_V4
{
public:
    juce::Font getPopupMenuFont() override
    {
        return { 10.0f };
    }

    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override
    {
        g.fillAll(HelperUIColours::background);
        g.setColour(HelperUIColours::active);
        g.drawRect(0, 0, width, height, 1);
    }

    void drawPopupMenuItem(juce::Graphics& g,
                           const juce::Rectangle<int>& area,
                           bool isSeparator,
                           bool isActive,
                           bool isHighlighted,
                           bool isTicked,
                           bool hasSubMenu,
                           const juce::String& text,
                           const juce::String& shortcutKeyText,
                           const juce::Drawable* icon,
                           const juce::Colour* const textColourToUse) override
    {
        juce::ignoreUnused(icon, shortcutKeyText, textColourToUse);

        if (isSeparator)
        {
            g.setColour(HelperUIColours::active.withAlpha(0.55f));
            g.drawHorizontalLine(area.getCentreY(), (float) area.getX() + 6.0f, (float) area.getRight() - 6.0f);
            return;
        }

        const auto itemBounds = area.reduced(2, 1);
        const bool isSelected = isHighlighted || isTicked;

        if (isSelected)
        {
            g.setColour(HelperUIColours::active);
            g.fillRect(itemBounds);
        }

        g.setColour((isActive ? (isSelected ? HelperUIColours::background : HelperUIColours::text)
                              : HelperUIColours::inactive)
                        .withAlpha(isActive ? 1.0f : 0.4f));
        g.setFont(getPopupMenuFont());
        g.drawText(text, itemBounds.reduced(8, 0), juce::Justification::centredLeft, true);

        if (hasSubMenu)
        {
            juce::Path triangle;
            const auto centreX = (float) itemBounds.getRight() - 8.0f;
            const auto centreY = (float) itemBounds.getCentreY();
            triangle.startNewSubPath(centreX - 3.0f, centreY - 4.0f);
            triangle.lineTo(centreX + 2.0f, centreY);
            triangle.lineTo(centreX - 3.0f, centreY + 4.0f);
            triangle.closeSubPath();
            g.fillPath(triangle);
        }
    }

    void getIdealPopupMenuItemSize(const juce::String& text,
                                   bool isSeparator,
                                   int standardMenuItemHeight,
                                   int& idealWidth,
                                   int& idealHeight) override
    {
        juce::LookAndFeel_V4::getIdealPopupMenuItemSize(text, isSeparator, standardMenuItemHeight, idealWidth, idealHeight);
        idealHeight = isSeparator ? 8 : juce::jmax(standardMenuItemHeight, kPopupItemHeight);
    }
};

MonitorPopupLookAndFeel& getMonitorPopupLookAndFeel()
{
    static MonitorPopupLookAndFeel lookAndFeel;
    return lookAndFeel;
}

class MonitorMenuButton : public juce::Button
{
public:
    MonitorMenuButton(const juce::String& componentName,
                      const juce::String& buttonText,
                      juce::Colour textColour,
                      juce::Colour highlightColour)
        : juce::Button(componentName)
        , label(buttonText)
        , textColour(textColour)
        , highlightColour(highlightColour)
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }

    void paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override
    {
        const auto alpha = isEnabled() ? 1.0f : 0.4f;
        auto colour = textColour.withAlpha(alpha);
        if (isButtonDown)
            colour = highlightColour.withAlpha(alpha);
        else if (isMouseOverButton)
            colour = colour.brighter(0.2f);

        const auto bounds = getLocalBounds().toFloat();
        g.setColour(colour);
        g.setFont(juce::Font(10.0f));

        auto textBounds = bounds.reduced(0.0f, 2.0f);
        if (showsArrow)
            textBounds = textBounds.withTrimmedRight(12.0f);

        g.drawText(label, textBounds.toNearestInt(), juce::Justification::centredLeft, true);

        if (showsArrow)
        {
            juce::Path triangle;
            const float centreX = bounds.getRight() - 6.0f;
            const float centreY = bounds.getCentreY();
            triangle.startNewSubPath(centreX - 4.0f, centreY - 2.0f);
            triangle.lineTo(centreX + 4.0f, centreY - 2.0f);
            triangle.lineTo(centreX, centreY + 3.0f);
            triangle.closeSubPath();
            g.fillPath(triangle);
        }
    }

private:
    juce::String label;
    juce::Colour textColour;
    juce::Colour highlightColour;
    bool showsArrow = true;
};

class ReferenceYawSlider : public juce::Slider
{
public:
    ReferenceYawSlider(juce::Colour dialFaceColour,
                       juce::Colour guideColour,
                       juce::Colour accentColour,
                       juce::Colour textColour,
                       juce::Colour disabledColour)
        : dialFaceColour(dialFaceColour)
        , guideColour(guideColour)
        , accentColour(accentColour)
        , textColour(textColour)
        , disabledColour(disabledColour)
    {
        setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        setScrollWheelEnabled(false);
        setDoubleClickReturnValue(true, 0.0);
        setMouseDragSensitivity(250);
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }

    void paint(juce::Graphics& g) override
    {
        const float alpha = isEnabled() ? 1.0f : 0.35f;
        const auto bounds = getLocalBounds().toFloat().reduced(4.0f);
        const float diameter = juce::jmin(bounds.getWidth(), bounds.getHeight());
        auto dialBounds = juce::Rectangle<float>(diameter, diameter).withCentre(bounds.getCentre());
        const auto centre = dialBounds.getCentre();
        const float radius = dialBounds.getWidth() * 0.5f;
        const auto activeColour = isEnabled() ? accentColour : disabledColour;
        const float outerInset = juce::jmax(2.5f, radius * (5.0f / 135.0f));
        const float minorTickInset = juce::jmax(6.0f, radius * (12.0f / 135.0f));
        const float majorTickInset = juce::jmax(11.0f, radius * (24.0f / 135.0f));
        const float pointerOuterInset = juce::jmax(4.0f, radius * (7.0f / 135.0f));
        const float arcThickness = juce::jmax(1.75f, radius * (2.5f / 135.0f));
        const float pointerThickness = juce::jmax(2.0f, radius * (3.0f / 135.0f));

        g.setColour(dialFaceColour.withAlpha(alpha));
        g.fillEllipse(dialBounds);

        g.setColour(guideColour.withAlpha(alpha));
        for (int i = 0; i < 8 * 4; ++i)
        {
            const float angle = (juce::MathConstants<float>::twoPi / (8.0f * 4.0f)) * (float) i;
            const float startL = radius - minorTickInset;
            const float endL = radius - outerInset;

            const juce::Point<float> start(centre.x + std::cos(angle) * startL,
                                           centre.y + std::sin(angle) * startL);
            const juce::Point<float> end(centre.x + std::cos(angle) * endL,
                                         centre.y + std::sin(angle) * endL);
            g.drawLine({ start, end }, 1.0f);
        }

        for (int i = 0; i < 8; ++i)
        {
            const float angle = (juce::MathConstants<float>::twoPi / 8.0f) * (float) i;
            const float startL = radius - majorTickInset;
            const float endL = radius - outerInset;

            const juce::Point<float> start(centre.x + std::cos(angle) * startL,
                                           centre.y + std::sin(angle) * startL);
            const juce::Point<float> end(centre.x + std::cos(angle) * endL,
                                         centre.y + std::sin(angle) * endL);
            g.drawLine({ start, end }, 2.0f);
        }

        float inputValueNormalised = ((float) getValue() - (float) getMinimum())
            / ((float) getMaximum() - (float) getMinimum());
        inputValueNormalised -= 0.5f;

        const float angle = juce::MathConstants<float>::twoPi * (inputValueNormalised - 0.25f);
        const float angleSize = juce::MathConstants<float>::halfPi;

        g.setColour(activeColour.withAlpha(alpha));
        constexpr int numSteps = 60;
        for (int i = 0; i < numSteps; ++i)
        {
            const float phase0 = (float) i / (float) numSteps;
            const float phase1 = (float) (i + 1) / (float) numSteps;

            const float angle0 = phase0 * angleSize + angle - angleSize * 0.5f;
            const float angle1 = phase1 * angleSize + angle - angleSize * 0.5f;

            const juce::Point<float> lineStart(centre.x + std::cos(angle0 - 0.01f) * (radius - 1.0f),
                                               centre.y + std::sin(angle0 - 0.01f) * (radius - 1.0f));
            const juce::Point<float> lineEnd(centre.x + std::cos(angle1 + 0.01f) * (radius - 1.0f),
                                             centre.y + std::sin(angle1 + 0.01f) * (radius - 1.0f));
            g.drawLine({ lineStart, lineEnd }, arcThickness);
        }

        const float centralLineInnerRadius = radius * (25.0f / 135.0f);
        const juce::Point<float> centralLineStart(centre.x + std::cos(angle) * centralLineInnerRadius,
                                                  centre.y + std::sin(angle) * centralLineInnerRadius);
        const juce::Point<float> centralLineEnd(centre.x + std::cos(angle) * (radius - pointerOuterInset),
                                                centre.y + std::sin(angle) * (radius - pointerOuterInset));
        g.drawLine({ centralLineStart, centralLineEnd }, pointerThickness);

        g.setColour(textColour.withAlpha(alpha));
        g.setFont(juce::Font(juce::jlimit(9.0f, 11.0f, radius * 0.12f)));
        g.drawText("YAW",
                   juce::Rectangle<float>(centre.x - radius * 0.42f,
                                          dialBounds.getY() + dialBounds.getHeight() * 0.25f - radius * 0.08f,
                                          radius * 0.84f,
                                          juce::jmax(14.0f, radius * 0.18f)).toNearestInt(),
                   juce::Justification::centred,
                   true);

        g.setFont(juce::Font(juce::jlimit(11.0f, 14.0f, radius * 0.16f)));
        g.drawText(formatAngle(getValue()),
                   juce::Rectangle<float>(centre.x - radius * 0.62f,
                                          centre.y - radius * 0.09f,
                                          radius * 1.24f,
                                          juce::jmax(18.0f, radius * 0.22f)).toNearestInt(),
                   juce::Justification::centred,
                   true);
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        if (!isEnabled())
            return;

        isDragging = true;
        previousDragPosition = event.position;
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (!isEnabled() || !isDragging)
            return;

        const auto deltaY = (double) previousDragPosition.y - (double) event.position.y;
        previousDragPosition = event.position;

        auto nextValue = getValue() + (deltaY / getEffectiveDragSpeed(event.mods)) * (getMaximum() - getMinimum());
        nextValue = wrapValue(nextValue, getMinimum(), getMaximum());
        setValue(nextValue, juce::sendNotificationSync);
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        isDragging = false;
    }

    void mouseDoubleClick(const juce::MouseEvent&) override
    {
        if (isEnabled())
            setValue(0.0, juce::sendNotificationSync);
    }

private:
    juce::Colour dialFaceColour;
    juce::Colour guideColour;
    juce::Colour accentColour;
    juce::Colour textColour;
    juce::Colour disabledColour;
    bool isDragging = false;
    juce::Point<float> previousDragPosition;
};

class ReferenceAxisSlider : public juce::Slider
{
public:
    enum class AxisOrientation
    {
        Vertical,
        Horizontal
    };

    ReferenceAxisSlider(AxisOrientation orientation,
                        juce::String label,
                        juce::Colour guideColour,
                        juce::Colour accentColour,
                        juce::Colour textColour,
                        juce::Colour disabledColour)
        : orientation(orientation)
        , label(std::move(label))
        , guideColour(guideColour)
        , accentColour(accentColour)
        , textColour(textColour)
        , disabledColour(disabledColour)
    {
        setSliderStyle(orientation == AxisOrientation::Vertical
                           ? juce::Slider::LinearVertical
                           : juce::Slider::LinearHorizontal);
        setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        setScrollWheelEnabled(false);
        setDoubleClickReturnValue(true, 0.0);
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }

    void paint(juce::Graphics& g) override
    {
        const float alpha = isEnabled() ? 1.0f : 0.35f;
        const auto bounds = getLocalBounds().toFloat();
        const auto activeColour = isEnabled() ? accentColour : disabledColour;
        const float ellipseRadius = juce::jlimit(3.5f, 5.0f, juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.05f);
        constexpr float labelWidth = 36.0f;
        constexpr float labelHeight = 24.0f;

        g.setFont(juce::Font(10.0f));

        if (orientation == AxisOrientation::Vertical)
        {
            const float reticlePositionNorm = juce::jlimit(0.0f,
                                                           1.0f,
                                                           (float) ((getValue() - getMaximum()) / (getMinimum() - getMaximum())));
            const float trackX = bounds.getCentreX();

            g.setColour(guideColour.withAlpha(alpha));
            g.drawLine(trackX, ellipseRadius * 0.5f, trackX, bounds.getHeight() - ellipseRadius, 1.0f);

            float posY = reticlePositionNorm * (bounds.getHeight() - 2.0f * ellipseRadius)
                + ellipseRadius - g.getCurrentFont().getHeight() * 0.5f;
            posY = juce::jlimit(0.0f, bounds.getHeight() - g.getCurrentFont().getHeight(), posY);

            g.setColour(textColour.withAlpha(alpha));
            g.drawText(label,
                       juce::Rectangle<float>(trackX - 50.0f, posY - 7.0f, labelWidth, labelHeight).toNearestInt(),
                       juce::Justification::centred,
                       true);
            g.drawText(formatAngle(getValue()),
                       juce::Rectangle<float>(trackX + 12.0f, posY - 7.0f, labelWidth, labelHeight).toNearestInt(),
                       juce::Justification::centred,
                       true);

            const float thumbY = reticlePositionNorm * (bounds.getHeight() - 2.0f * ellipseRadius) + ellipseRadius;
            g.setColour(activeColour.withAlpha(alpha));
            g.fillEllipse(trackX - ellipseRadius, thumbY - ellipseRadius, ellipseRadius * 2.0f, ellipseRadius * 2.0f);
        }
        else
        {
            const float reticlePositionNorm = juce::jlimit(0.0f,
                                                           1.0f,
                                                           (float) ((getValue() - getMinimum()) / (getMaximum() - getMinimum())));
            const float trackY = bounds.getCentreY();

            g.setColour(guideColour.withAlpha(alpha));
            g.drawLine(ellipseRadius * 0.5f, trackY, bounds.getWidth() - ellipseRadius, trackY, 1.0f);

            float posX = reticlePositionNorm * (bounds.getWidth() - 2.0f * ellipseRadius)
                + ellipseRadius - labelWidth * 0.5f;
            posX = juce::jlimit(0.0f, bounds.getWidth() - labelWidth, posX);

            g.setColour(textColour.withAlpha(alpha));
            g.drawText(label,
                       juce::Rectangle<float>(posX + 2.0f, trackY - 29.0f, labelWidth, labelHeight).toNearestInt(),
                       juce::Justification::centred,
                       true);
            g.drawText(formatAngle(getValue()),
                       juce::Rectangle<float>(posX + 2.0f, trackY + 2.0f, labelWidth, labelHeight).toNearestInt(),
                       juce::Justification::centred,
                       true);

            const float thumbX = reticlePositionNorm * (bounds.getWidth() - 2.0f * ellipseRadius) + ellipseRadius;
            g.setColour(activeColour.withAlpha(alpha));
            g.fillEllipse(thumbX - ellipseRadius, trackY - ellipseRadius, ellipseRadius * 2.0f, ellipseRadius * 2.0f);
        }
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        if (!isEnabled())
            return;

        isDragging = true;
        previousDragPosition = event.position;
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (!isEnabled() || !isDragging)
            return;

        const auto deltaPixels = orientation == AxisOrientation::Vertical
                                     ? ((double) previousDragPosition.y - (double) event.position.y)
                                     : ((double) event.position.x - (double) previousDragPosition.x);
        previousDragPosition = event.position;

        auto nextValue = getValue() + (deltaPixels / getEffectiveDragSpeed(event.mods)) * (getMaximum() - getMinimum());
        nextValue = juce::jlimit(getMinimum(), getMaximum(), nextValue);
        setValue(nextValue, juce::sendNotificationSync);
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        isDragging = false;
    }

    void mouseDoubleClick(const juce::MouseEvent&) override
    {
        if (isEnabled())
            setValue(0.0, juce::sendNotificationSync);
    }

private:
    AxisOrientation orientation;
    juce::String label;
    juce::Colour guideColour;
    juce::Colour accentColour;
    juce::Colour textColour;
    juce::Colour disabledColour;
    bool isDragging = false;
    juce::Point<float> previousDragPosition;
};

} // namespace

MonitorPanel::MonitorPanel()
{
    setOpaque(false);

    decodeModeButton = std::make_unique<MonitorMenuButton>("decodeModeButton", "DECODE MODE", dimTextColour, guideColour);
    decodeModeButton->onClick = [this]() { showSettingsMenu(); };
    addAndMakeVisible(decodeModeButton.get());

    monitorButton = std::make_unique<MonitorMenuButton>("monitorButton", "MONITOR", dimTextColour, guideColour);
    monitorButton->onClick = [this]() { showMonitorMenu(); };
    addAndMakeVisible(monitorButton.get());

    yawSlider = std::make_unique<ReferenceYawSlider>(dialFaceColour,
                                                     guideColour,
                                                     accentColour,
                                                     textColour,
                                                     disabledColour);
    yawSlider->setRange(-180.0, 180.0, 0.1);
    yawSlider->onValueChange = [this]() {
        if (!suppressCallbacks && onOrientationChanged)
            onOrientationChanged((float) yawSlider->getValue(),
                                 (float) pitchSlider->getValue(),
                                 (float) rollSlider->getValue());
    };
    addAndMakeVisible(yawSlider.get());

    pitchSlider = std::make_unique<ReferenceAxisSlider>(ReferenceAxisSlider::AxisOrientation::Vertical,
                                                        "PITCH",
                                                        trackColour,
                                                        accentColour,
                                                        textColour,
                                                        disabledColour);
    pitchSlider->setRange(-90.0, 90.0, 0.1);
    pitchSlider->onValueChange = [this]() {
        if (!suppressCallbacks && onOrientationChanged)
            onOrientationChanged((float) yawSlider->getValue(),
                                 (float) pitchSlider->getValue(),
                                 (float) rollSlider->getValue());
    };
    addAndMakeVisible(pitchSlider.get());

    rollSlider = std::make_unique<ReferenceAxisSlider>(ReferenceAxisSlider::AxisOrientation::Horizontal,
                                                       "ROLL",
                                                       trackColour,
                                                       accentColour,
                                                       textColour,
                                                       disabledColour);
    rollSlider->setRange(-90.0, 90.0, 0.1);
    rollSlider->onValueChange = [this]() {
        if (!suppressCallbacks && onOrientationChanged)
            onOrientationChanged((float) yawSlider->getValue(),
                                 (float) pitchSlider->getValue(),
                                 (float) rollSlider->getValue());
    };
    addAndMakeVisible(rollSlider.get());

    setControlsEnabled(false);
}

MonitorPanel::~MonitorPanel() = default;

void MonitorPanel::paint(juce::Graphics& g)
{
    if (embeddedMonitor != nullptr)
        return;

    // Left border divider (match Panner3DViewPanel style)
    g.setColour(dividerColour);
    g.drawVerticalLine(0, 0.0f, static_cast<float>(getHeight()));

    auto footer = getLocalBounds().removeFromBottom(BOTTOM_BAR_HEIGHT).reduced(10, 0);

    if (currentState.monitors.empty())
    {
        g.setColour(textColour.withAlpha(0.5f));
        g.setFont(juce::Font(10.0f));
        g.drawText("No active monitor instances connected",
                   footer,
                   juce::Justification::centred,
                   true);
        return;
    }

    if (shouldUseCompactMonitorView(*this))
    {
        g.setColour(textColour.withAlpha(0.5f));
        g.setFont(juce::Font(10.0f));
        const auto activeMonitorPort = getActiveMonitorPort();
        juce::String line = "Expand to edit monitor  |  ";
        if (activeMonitorPort != 0)
            line += "Mon " + juce::String(activeMonitorPort) + "  ";
        line += formatChannelCountLabel(currentState.channelCount);
        g.drawText(line, footer, juce::Justification::centred, true);
    }
}

void MonitorPanel::resized()
{
    auto bounds = getLocalBounds().reduced(14, 12);

    if (embeddedMonitor != nullptr)
    {
        embeddedMonitor->setBounds(bounds);
        return;
    }

    bounds.removeFromBottom(BOTTOM_BAR_HEIGHT);
    bounds.removeFromBottom(4);

    const bool useCompactView = shouldUseCompactMonitorView(*this);
    decodeModeButton->setVisible(!useCompactView);
    monitorButton->setVisible(!useCompactView);
    yawSlider->setVisible(!useCompactView);
    pitchSlider->setVisible(!useCompactView);
    rollSlider->setVisible(!useCompactView);

    if (useCompactView)
        return;

    const int controlSectionWidth = juce::jlimit(CONTROL_SECTION_MIN_WIDTH,
                                                 CONTROL_SECTION_MAX_WIDTH,
                                                 juce::roundToInt(bounds.getWidth() * 0.46f));
    auto controlSection = bounds.removeFromRight(controlSectionWidth);
    auto menuColumn = controlSection.removeFromRight(MENU_COLUMN_WIDTH);
    auto sliderColumn = controlSection.reduced(2, 0);
    auto yawArea = bounds.reduced(4, 0);

    const int yawSize = juce::jmin(MAX_YAW_DIAMETER,
                                   juce::jmin(yawArea.getWidth(), yawArea.getHeight()));
    yawSlider->setBounds(juce::Rectangle<int>(yawSize, yawSize).withCentre(yawArea.getCentre()));

    auto pitchArea = sliderColumn.removeFromTop(juce::roundToInt(sliderColumn.getHeight() * 0.62f));
    pitchSlider->setBounds(pitchArea.reduced(10, 8));

    sliderColumn.removeFromTop(6);
    rollSlider->setBounds(sliderColumn.reduced(10, 6));

    const int menuBtnX = menuColumn.getX() + (menuColumn.getWidth() - MENU_BUTTON_WIDTH) / 2;
    const int menuBtnTop = menuColumn.getY();
    constexpr int menuBtnGap = 6;
    decodeModeButton->setBounds(menuBtnX, menuBtnTop, MENU_BUTTON_WIDTH, MENU_BUTTON_HEIGHT);
    monitorButton->setBounds(menuBtnX, menuBtnTop + MENU_BUTTON_HEIGHT + menuBtnGap,
                             MENU_BUTTON_WIDTH, MENU_BUTTON_HEIGHT);
}

void MonitorPanel::setMonitorComponent(juce::Component* monitorComponent)
{
    if (embeddedMonitor != nullptr)
        removeChildComponent(embeddedMonitor);

    embeddedMonitor = monitorComponent;

    if (embeddedMonitor != nullptr)
        addAndMakeVisible(embeddedMonitor);

    const bool usingEmbeddedMonitor = embeddedMonitor != nullptr;
    decodeModeButton->setVisible(!usingEmbeddedMonitor);
    monitorButton->setVisible(!usingEmbeddedMonitor);
    yawSlider->setVisible(!usingEmbeddedMonitor);
    pitchSlider->setVisible(!usingEmbeddedMonitor);
    rollSlider->setVisible(!usingEmbeddedMonitor);

    resized();
    repaint();
}

void MonitorPanel::updateState(const MonitorPanelState& state)
{
    currentState = state;
    suppressCallbacks = true;

    yawSlider->setValue(currentState.yaw, juce::dontSendNotification);
    pitchSlider->setValue(currentState.pitch, juce::dontSendNotification);
    rollSlider->setValue(currentState.roll, juce::dontSendNotification);

    suppressCallbacks = false;

    decodeModeButton->setTooltip("Output format: " + formatChannelCountLabel(currentState.channelCount));

    const int activeMonitorPort = getActiveMonitorPort();
    monitorButton->setTooltip(activeMonitorPort != 0
                                  ? "Active monitor: " + juce::String(activeMonitorPort)
                                  : "No active monitor selected");

    setControlsEnabled(!currentState.monitors.empty());
    repaint();
}

int MonitorPanel::getActiveMonitorPort() const
{
    for (const auto& monitor : currentState.monitors)
    {
        if (monitor.active)
            return monitor.port;
    }

    return currentState.monitors.empty() ? 0 : currentState.monitors.front().port;
}

void MonitorPanel::showMonitorMenu()
{
    if (currentState.monitors.empty())
        return;

    juce::PopupMenu menu;
    menu.setLookAndFeel(&getMonitorPopupLookAndFeel());
    const int activeMonitorPort = getActiveMonitorPort();

    for (const auto& monitor : currentState.monitors)
    {
        const juce::String label = "Monitor " + juce::String(monitor.port)
            + (monitor.port == activeMonitorPort ? " (active)" : "");
        menu.addItem(monitor.port, label, true, monitor.port == activeMonitorPort);
    }

    menu.showMenuAsync(juce::PopupMenu::Options()
                           .withTargetComponent(monitorButton.get())
                           .withStandardItemHeight(kPopupItemHeight),
                       [safeThis = juce::Component::SafePointer<MonitorPanel>(this)](int result) {
                           if (safeThis != nullptr && result != 0 && safeThis->onActiveMonitorSelected)
                               safeThis->onActiveMonitorSelected(result);
                       });
}

void MonitorPanel::showSettingsMenu()
{
    juce::PopupMenu menu;
    menu.setLookAndFeel(&getMonitorPopupLookAndFeel());
    menu.addItem(4, "M1Spatial-4", true, currentState.channelCount == 4);
    menu.addItem(8, "M1Spatial-8", true, currentState.channelCount == 8);
    menu.addItem(14, "M1Spatial-14", true, currentState.channelCount == 14);

    menu.showMenuAsync(juce::PopupMenu::Options()
                           .withTargetComponent(decodeModeButton.get())
                           .withStandardItemHeight(kPopupItemHeight),
                       [safeThis = juce::Component::SafePointer<MonitorPanel>(this)](int result) {
                           if (safeThis != nullptr && result != 0 && safeThis->onOutputChannelCountChanged)
                               safeThis->onOutputChannelCountChanged(result);
                       });
}

void MonitorPanel::setControlsEnabled(bool enabled)
{
    decodeModeButton->setEnabled(enabled);
    monitorButton->setEnabled(enabled);
    yawSlider->setEnabled(enabled);
    pitchSlider->setEnabled(enabled);
    rollSlider->setEnabled(enabled);
}

} // namespace Mach1

