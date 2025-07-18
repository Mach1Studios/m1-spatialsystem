/*
    SoundfieldDisplayComponent.cpp
    ------------------------------
    Implementation of the 3D soundfield visualization component
*/

#include "SoundfieldDisplayComponent.h"

namespace Mach1 {

//==============================================================================
SoundfieldDisplayComponent::SoundfieldDisplayComponent()
{
    // Create title label
    titleLabel = std::make_unique<juce::Label>("title", "Soundfield Display");
    titleLabel->setFont(juce::Font(16.0f, juce::Font::bold));
    titleLabel->setColour(juce::Label::textColourId, textColour);
    titleLabel->setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel.get());
    
    // Create location label
    locationLabel = std::make_unique<juce::Label>("location", "Location: 0.0, 0.0, 0.0");
    locationLabel->setFont(juce::Font(12.0f));
    locationLabel->setColour(juce::Label::textColourId, textColour);
    locationLabel->setJustificationType(juce::Justification::centred);
    addAndMakeVisible(locationLabel.get());
    
    // Create rotation label
    rotationLabel = std::make_unique<juce::Label>("rotation", "Rotation: Y:0, P:0, R:0");
    rotationLabel->setFont(juce::Font(12.0f));
    rotationLabel->setColour(juce::Label::textColourId, textColour);
    rotationLabel->setJustificationType(juce::Justification::centred);
    addAndMakeVisible(rotationLabel.get());
}

SoundfieldDisplayComponent::~SoundfieldDisplayComponent()
{
}

//==============================================================================
void SoundfieldDisplayComponent::updatePannerData(const std::vector<PannerInfo>& panners)
{
    pannerData = panners;
    updatePannerPositions();
    repaint();
}

void SoundfieldDisplayComponent::setSelectedPanner(int index)
{
    selectedPannerIndex = index;
    repaint();
}

//==============================================================================
void SoundfieldDisplayComponent::setRotation(float yaw, float pitch, float roll)
{
    currentYaw = yaw;
    currentPitch = pitch;
    currentRoll = roll;
    
    // Update rotation label
    rotationLabel->setText(juce::String::formatted("Rotation: Y:%.1f° P:%.1f° R:%.1f°", 
                                                   yaw, pitch, roll), 
                          juce::dontSendNotification);
    
    repaint();
}

void SoundfieldDisplayComponent::getRotation(float& yaw, float& pitch, float& roll) const
{
    yaw = currentYaw;
    pitch = currentPitch;
    roll = currentRoll;
}

void SoundfieldDisplayComponent::setZoom(float zoom)
{
    currentZoom = juce::jlimit(MIN_ZOOM, MAX_ZOOM, zoom);
    repaint();
}

//==============================================================================
void SoundfieldDisplayComponent::paint(juce::Graphics& g)
{
    g.fillAll(backgroundColour);
    
    // Draw sphere
    drawSphere(g);
    
    // Draw coordinate grid
    drawCoordinateGrid(g);
    
    // Draw panner positions
    drawPannerPositions(g);
    
    // Draw rotation controls
    drawRotationControls(g);
    
    // Draw location readout
    drawLocationReadout(g);
}

void SoundfieldDisplayComponent::resized()
{
    auto bounds = getLocalBounds();
    
    // Title at top
    titleLabel->setBounds(bounds.removeFromTop(25));
    
    // Info labels at bottom
    auto infoArea = bounds.removeFromBottom(40);
    locationLabel->setBounds(infoArea.removeFromTop(20));
    rotationLabel->setBounds(infoArea);
    
    // Sphere area
    sphereBounds = bounds.reduced(20);
    
    // Make sphere area square
    int size = juce::jmin(sphereBounds.getWidth(), sphereBounds.getHeight());
    sphereBounds = juce::Rectangle<int>(
        sphereBounds.getCentreX() - size/2,
        sphereBounds.getCentreY() - size/2,
        size, size
    );
    
    controlsBounds = bounds.removeFromRight(100);
    infoBounds = bounds.removeFromBottom(60);
}

//==============================================================================
void SoundfieldDisplayComponent::mouseDown(const juce::MouseEvent& event)
{
    if (sphereBounds.contains(event.getPosition()))
    {
        // Check if clicking on a panner
        int pannerIndex = findPannerAtPosition(event.getPosition());
        if (pannerIndex >= 0)
        {
            selectedPannerIndex = pannerIndex;
            
            if (onPannerSelected)
                onPannerSelected(pannerIndex);
        }
        else
        {
            // Start rotation drag
            isDragging = true;
            lastMousePosition = event.getPosition();
        }
    }
}

void SoundfieldDisplayComponent::mouseDrag(const juce::MouseEvent& event)
{
    if (isDragging && sphereBounds.contains(event.getPosition()))
    {
        auto delta = event.getPosition() - lastMousePosition;
        handleRotationDrag(delta);
        lastMousePosition = event.getPosition();
    }
}

void SoundfieldDisplayComponent::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    // Zoom with mouse wheel
    float zoomChange = wheel.deltaY * ZOOM_SENSITIVITY;
    setZoom(currentZoom + zoomChange);
}

//==============================================================================
void SoundfieldDisplayComponent::drawSphere(juce::Graphics& g)
{
    auto centre = sphereBounds.getCentre();
    float radius = SPHERE_RADIUS * currentZoom;
    
    // Draw sphere outline
    g.setColour(sphereColour);
    g.drawEllipse(centre.x - radius, centre.y - radius, radius * 2, radius * 2, 2);
    
    // Draw sphere fill with transparency
    g.setColour(sphereColour.withAlpha(0.1f));
    g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2, radius * 2);
}

void SoundfieldDisplayComponent::drawCoordinateGrid(juce::Graphics& g)
{
    auto centre = sphereBounds.getCentre();
    float radius = SPHERE_RADIUS * currentZoom;
    
    g.setColour(gridColour);
    
    // Draw latitude lines
    for (int lat = -60; lat <= 60; lat += 30)
    {
        float y = centre.y - radius * sin(lat * M_PI / 180.0f);
        float w = radius * cos(lat * M_PI / 180.0f) * 2;
        
        g.drawEllipse(centre.x - w/2, y - w/2, w, w, 1);
    }
    
    // Draw longitude lines
    for (int lon = 0; lon < 360; lon += 45)
    {
        float angle = (lon + currentYaw) * M_PI / 180.0f;
        float x1 = centre.x + radius * cos(angle);
        float y1 = centre.y + radius * sin(angle);
        float x2 = centre.x - radius * cos(angle);
        float y2 = centre.y - radius * sin(angle);
        
        g.drawLine(x1, y1, x2, y2, 1);
    }
    
    // Draw axis lines
    g.setColour(gridColour.brighter(0.3f));
    
    // X-axis (front-back)
    g.drawLine(centre.x - radius, centre.y, centre.x + radius, centre.y, 2);
    
    // Y-axis (left-right)
    g.drawLine(centre.x, centre.y - radius, centre.x, centre.y + radius, 2);
}

void SoundfieldDisplayComponent::drawPannerPositions(juce::Graphics& g)
{
    auto centre = sphereBounds.getCentre();
    
    for (int i = 0; i < pannerPositions.size(); ++i)
    {
        const auto& pos = pannerPositions[i];
        auto screenPos = project3DTo2D(pos);
        
        // Only draw if within sphere bounds
        if (sphereBounds.contains(screenPos.toInt()))
        {
            // Choose color based on selection
            juce::Colour pannerColour = (i == selectedPannerIndex) ? selectedPannerColour : this->pannerColour;
            
            // Draw panner dot
            g.setColour(pannerColour);
            g.fillEllipse(screenPos.x - 4, screenPos.y - 4, 8, 8);
            
            // Draw panner border
            g.setColour(pannerColour.darker(0.3f));
            g.drawEllipse(screenPos.x - 4, screenPos.y - 4, 8, 8, 1);
            
            // Draw panner label
            if (i < pannerData.size())
            {
                g.setColour(textColour);
                g.setFont(juce::Font(10.0f));
                g.drawText(pannerData[i].name, 
                          screenPos.x - 20, screenPos.y + 6, 40, 12,
                          juce::Justification::centred, true);
            }
        }
    }
}

void SoundfieldDisplayComponent::drawRotationControls(juce::Graphics& g)
{
    // This could be enhanced with visual rotation controls
    // For now, just show text instructions
    
    g.setColour(textColour.withAlpha(0.7f));
    g.setFont(juce::Font(10.0f));
    g.drawText("Click and drag to rotate", 
               sphereBounds.getRight() - 100, sphereBounds.getY() + 20, 
               100, 20, juce::Justification::centred);
    
    g.drawText("Mouse wheel to zoom", 
               sphereBounds.getRight() - 100, sphereBounds.getY() + 40, 
               100, 20, juce::Justification::centred);
}

void SoundfieldDisplayComponent::drawLocationReadout(juce::Graphics& g)
{
    // Location readout is handled by the location label
    // This could be enhanced with additional visual feedback
}

//==============================================================================
juce::Point<float> SoundfieldDisplayComponent::project3DTo2D(const SoundfieldPosition& pos) const
{
    auto centre = sphereBounds.getCentre();
    float radius = SPHERE_RADIUS * currentZoom;
    
    // Apply rotation transformations
    float x = pos.x;
    float y = pos.y;
    float z = pos.z;
    
    // Simple 2D projection (orthographic)
    float screenX = centre.x + x * radius;
    float screenY = centre.y - y * radius;  // Flip Y for screen coordinates
    
    return {screenX, screenY};
}

SoundfieldPosition SoundfieldDisplayComponent::screenToSoundfield(juce::Point<int> screenPos) const
{
    auto centre = sphereBounds.getCentre();
    float radius = SPHERE_RADIUS * currentZoom;
    
    float x = (screenPos.x - centre.x) / radius;
    float y = -(screenPos.y - centre.y) / radius;  // Flip Y for screen coordinates
    
    // Convert to spherical coordinates
    float distance = sqrt(x * x + y * y);
    float azimuth = atan2(y, x) * 180.0f / M_PI;
    float elevation = 0.0f;  // Simplified for 2D projection
    
    return SoundfieldPosition(azimuth, elevation, distance);
}

void SoundfieldDisplayComponent::updatePannerPositions()
{
    pannerPositions.clear();
    
    for (const auto& panner : pannerData)
    {
        SoundfieldPosition pos(panner.azimuth, panner.elevation, 1.0f);
        pannerPositions.push_back(pos);
    }
}

//==============================================================================
int SoundfieldDisplayComponent::findPannerAtPosition(juce::Point<int> screenPos)
{
    for (int i = 0; i < pannerPositions.size(); ++i)
    {
        auto pannerScreenPos = project3DTo2D(pannerPositions[i]);
        float distance = pannerScreenPos.getDistanceFrom(screenPos.toFloat());
        
        if (distance < 10.0f)  // 10 pixel tolerance
        {
            return i;
        }
    }
    
    return -1;
}

void SoundfieldDisplayComponent::handleRotationDrag(juce::Point<int> delta)
{
    // Update rotation based on mouse drag
    currentYaw += delta.x * ROTATION_SENSITIVITY;
    currentPitch += delta.y * ROTATION_SENSITIVITY;
    
    // Wrap yaw to 0-360 range
    while (currentYaw < 0) currentYaw += 360;
    while (currentYaw >= 360) currentYaw -= 360;
    
    // Clamp pitch to -90 to 90 range
    currentPitch = juce::jlimit(-90.0f, 90.0f, currentPitch);
    
    // Update rotation label
    rotationLabel->setText(juce::String::formatted("Rotation: Y:%.1f° P:%.1f° R:%.1f°", 
                                                   currentYaw, currentPitch, currentRoll), 
                          juce::dontSendNotification);
    
    if (onRotationChanged)
        onRotationChanged(currentYaw, currentPitch, currentRoll);
    
    repaint();
}

} // namespace Mach1 
