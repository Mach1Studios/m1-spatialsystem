/*
    Panner3DViewPanel.cpp
    ---------------------
    Implementation of the 3D panner visualization with wireframe cube and reticles.
    
    Mach1 Coordinate System:
    - X: left/right (+X is right, -X is left)
    - Y: front/back (+Y is front, -Y is back)
    - Z: up/down (+Z is top, -Z is bottom)
    
    Camera: yaw rotates around Z (up), pitch rotates around X (right).
    Projection: X -> screen horizontal, Z -> screen vertical, Y -> depth.
*/

#include "Panner3DViewPanel.h"

namespace Mach1 {

//==============================================================================
Panner3DViewPanel::Panner3DViewPanel()
{
    camera.reset();
    setWantsKeyboardFocus(true);
}

Panner3DViewPanel::~Panner3DViewPanel()
{
    stopTimer();
}

//==============================================================================
void Panner3DViewPanel::updatePannerData(const std::vector<PannerInfo>& panners)
{
    reticles.clear();
    
    int index = 0;
    for (const auto& panner : panners)
    {
        PannerReticle reticle;
        
        // Store raw azimuth/elevation for display
        reticle.azimuth = panner.azimuth;
        reticle.elevation = panner.elevation;
        
        // Convert azimuth/elevation to 3D position using Mach1 AED convention
        // Place reticles on a unit sphere (radius 0.85 to fit within cube)
        reticle.position = Vec3::fromAED(panner.azimuth, panner.elevation, 0.85f);
        
        // Use panner name or index as label
        reticle.label = panner.name.empty() 
            ? juce::String(index + 1)
            : juce::String(panner.name).substring(0, 10);
        
        // Color based on state - gray for active, orange for OSC
        if (panner.isMemoryShareBased) {
            reticle.colour = reticleColour;  // Gray
        } else {
            reticle.colour = juce::Colour(0xFFFF9800);  // Orange for OSC
        }
        
        reticle.isSelected = (index == selectedPannerIndex);
        reticle.pannerIndex = index;
        
        reticles.push_back(reticle);
        index++;
    }
    
    repaint();
}

void Panner3DViewPanel::setSelectedPannerIndex(int index)
{
    if (selectedPannerIndex != index)
    {
        selectedPannerIndex = index;
        
        // Update selection state in reticles
        for (auto& reticle : reticles)
        {
            reticle.isSelected = (reticle.pannerIndex == index);
        }
        
        repaint();
    }
}

void Panner3DViewPanel::resetCamera()
{
    camera.reset();
    repaint();
}

void Panner3DViewPanel::setCameraPreset(CameraPreset preset)
{
    camera.setPreset(preset);
    repaint();
}

//==============================================================================
void Panner3DViewPanel::paint(juce::Graphics& g)
{
    // Fill background
    g.fillAll(backgroundColour);
    
    // Calculate view bounds (below toolbar)
    auto bounds = getLocalBounds().toFloat();
    auto toolbarBounds = bounds.removeFromTop(TOOLBAR_HEIGHT);
    viewBounds = bounds.reduced(30);
    viewScale = juce::jmin(viewBounds.getWidth(), viewBounds.getHeight()) * 0.35f * camera.zoom;
    
    // Draw toolbar with view buttons
    drawToolbar(g);
    
    // Draw 3D content
    drawFloorGrid(g);
    drawWireframeCube(g);
    drawListenerPosition(g);
    drawDirectionLabels(g);
    drawPannerReticles(g);
    drawCornerGizmo(g);
    drawCameraInfo(g);
    
    // Draw border matching reference design
    g.setColour(borderColour);
    g.drawRect(getLocalBounds(), 1);
    
    // Draw instructions at bottom
    g.setColour(textColour.withAlpha(0.5f));
    g.setFont(juce::Font(10.0f));
    g.drawText("Drag: orbit | Wheel: zoom | Shift+drag: pan", 
               getLocalBounds().removeFromBottom(16), juce::Justification::centred);
}

void Panner3DViewPanel::drawToolbar(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    auto toolbarArea = bounds.removeFromTop(TOOLBAR_HEIGHT);
    
    // Toolbar background
    g.setColour(toolbarColour);
    g.fillRect(toolbarArea);
    g.setColour(borderColour);
    g.drawHorizontalLine(TOOLBAR_HEIGHT - 1, 0, (float)getWidth());
    
    // Section title "SOUNDFIELD DISPLAY" like the reference
    g.setColour(juce::Colour(0xFF808080));
    g.setFont(juce::Font(11.0f, juce::Font::bold));
    g.drawText("SOUNDFIELD DISPLAY", toolbarArea.withLeft(10), juce::Justification::centredLeft);
    
    // Buttons on the right - smaller, more compact
    int buttonWidth = 40;
    int buttonHeight = 18;
    int buttonY = (TOOLBAR_HEIGHT - buttonHeight) / 2;
    int buttonX = getWidth() - 10;
    
    // Reset button (icon-style)
    buttonX -= buttonWidth + 3;
    resetButtonBounds = juce::Rectangle<int>(buttonX, buttonY, buttonWidth, buttonHeight);
    g.setColour(buttonColour);
    g.fillRoundedRectangle(resetButtonBounds.toFloat(), 2.0f);
    g.setColour(textColour);
    g.setFont(juce::Font(9.0f));
    g.drawText("RESET", resetButtonBounds, juce::Justification::centred);
    
    // Front View button
    buttonX -= buttonWidth + 3;
    frontViewButtonBounds = juce::Rectangle<int>(buttonX, buttonY, buttonWidth, buttonHeight);
    bool isFront = (camera.preset == CameraPreset::Front);
    g.setColour(isFront ? buttonActiveColour : buttonColour);
    g.fillRoundedRectangle(frontViewButtonBounds.toFloat(), 2.0f);
    g.setColour(isFront ? juce::Colour(0xFF0D0D0D) : textColour);
    g.drawText("3D", frontViewButtonBounds, juce::Justification::centred);
    
    // Top-Down button
    buttonX -= buttonWidth + 3;
    topDownButtonBounds = juce::Rectangle<int>(buttonX, buttonY, buttonWidth, buttonHeight);
    bool isTopDown = (camera.preset == CameraPreset::TopDown);
    g.setColour(isTopDown ? buttonActiveColour : buttonColour);
    g.fillRoundedRectangle(topDownButtonBounds.toFloat(), 2.0f);
    g.setColour(isTopDown ? juce::Colour(0xFF0D0D0D) : textColour);
    g.drawText("TOP", topDownButtonBounds, juce::Justification::centred);
}

void Panner3DViewPanel::resized()
{
    // Nothing specific needed, paint() recalculates bounds
}

//==============================================================================
void Panner3DViewPanel::mouseDown(const juce::MouseEvent& e)
{
    lastMousePos = e.getPosition();
    
    // Check toolbar buttons
    if (isTopDownButtonHit(e.getPosition()))
    {
        setCameraPreset(CameraPreset::TopDown);
        return;
    }
    
    if (isFrontViewButtonHit(e.getPosition()))
    {
        setCameraPreset(CameraPreset::Front);
        return;
    }
    
    if (isResetButtonHit(e.getPosition()))
    {
        resetCamera();
        return;
    }
    
    // Check if clicking on a reticle
    int hitIndex = findReticleAtPoint(e.getPosition());
    if (hitIndex >= 0)
    {
        setSelectedPannerIndex(hitIndex);
        
        if (onPannerSelected)
            onPannerSelected(hitIndex);
        
        return;
    }
    
    if (e.mods.isShiftDown())
    {
        isPanning = true;
        isDragging = false;
    }
    else
    {
        isDragging = true;
        isPanning = false;
    }
}

void Panner3DViewPanel::mouseDrag(const juce::MouseEvent& e)
{
    auto delta = e.getPosition() - lastMousePos;
    lastMousePos = e.getPosition();
    
    if (isDragging)
    {
        // Orbit camera - switch to custom preset
        camera.preset = CameraPreset::Custom;
        // Standard orbit: drag right = rotate view right (yaw increases)
        camera.yaw += delta.x * ORBIT_SENSITIVITY;
        camera.pitch += delta.y * ORBIT_SENSITIVITY;
        
        // Clamp pitch to avoid gimbal lock
        camera.pitch = juce::jlimit(-89.0f, 89.0f, camera.pitch);
        
        // Wrap yaw
        if (camera.yaw > 180.0f) camera.yaw -= 360.0f;
        if (camera.yaw < -180.0f) camera.yaw += 360.0f;
        
        repaint();
    }
    else if (isPanning)
    {
        // Pan camera
        camera.panX += delta.x * PAN_SENSITIVITY / camera.zoom;
        camera.panY -= delta.y * PAN_SENSITIVITY / camera.zoom;
        
        repaint();
    }
}

void Panner3DViewPanel::mouseUp(const juce::MouseEvent&)
{
    isDragging = false;
    isPanning = false;
}

void Panner3DViewPanel::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    // Zoom
    float zoomChange = wheel.deltaY * ZOOM_SENSITIVITY;
    camera.zoom = juce::jlimit(MIN_ZOOM, MAX_ZOOM, camera.zoom + zoomChange);
    repaint();
}

void Panner3DViewPanel::timerCallback()
{
    // Used for animation when fake panners are enabled
    repaint();
}

//==============================================================================
bool Panner3DViewPanel::isTopDownButtonHit(juce::Point<int> screenPos)
{
    return topDownButtonBounds.contains(screenPos);
}

bool Panner3DViewPanel::isFrontViewButtonHit(juce::Point<int> screenPos)
{
    return frontViewButtonBounds.contains(screenPos);
}

bool Panner3DViewPanel::isResetButtonHit(juce::Point<int> screenPos)
{
    return resetButtonBounds.contains(screenPos);
}

//==============================================================================
Vec3 Panner3DViewPanel::rotateByCamera(const Vec3& pos) const
{
    // Apply yaw (rotation around Z axis - Mach1 up)
    float yawRad = camera.yaw * juce::MathConstants<float>::pi / 180.0f;
    float cosYaw = std::cos(yawRad);
    float sinYaw = std::sin(yawRad);
    
    Vec3 rotatedYaw = {
        pos.x * cosYaw - pos.y * sinYaw,
        pos.x * sinYaw + pos.y * cosYaw,
        pos.z
    };
    
    // Apply pitch (rotation around X axis - Mach1 right)
    // Negate so negative pitch = looking down = front things move up on screen
    float pitchRad = -camera.pitch * juce::MathConstants<float>::pi / 180.0f;
    float cosPitch = std::cos(pitchRad);
    float sinPitch = std::sin(pitchRad);
    
    Vec3 rotatedPitch = {
        rotatedYaw.x,
        rotatedYaw.y * cosPitch - rotatedYaw.z * sinPitch,
        rotatedYaw.y * sinPitch + rotatedYaw.z * cosPitch
    };
    
    return rotatedPitch;
}

juce::Point<float> Panner3DViewPanel::project(const Vec3& worldPos) const
{
    Vec3 rotated = rotateByCamera(worldPos);
    
    rotated.x += camera.panX;
    rotated.z += camera.panY;
    
    float centerX = viewBounds.getCentreX();
    float centerY = viewBounds.getCentreY();
    
    float screenX = centerX + rotated.x * viewScale;  // X -> horizontal
    float screenY = centerY - rotated.z * viewScale;   // Z (Mach1 up) -> vertical (flipped for screen)
    
    return {screenX, screenY};
}

float Panner3DViewPanel::getDepth(const Vec3& worldPos) const
{
    Vec3 rotated = rotateByCamera(worldPos);
    return rotated.y; // Y (Mach1 front/back) is depth after rotation
}

//==============================================================================
void Panner3DViewPanel::drawWireframeCube(juce::Graphics& g)
{
    const float s = CUBE_SIZE;
    
    // Mach1 coordinate system: X: left(-)/right(+), Y: back(-)/front(+), Z: bottom(-)/top(+)
    Vec3 vertices[8] = {
        // Bottom face (Z = -s)
        {-s, -s, -s},  // 0: left, back, bottom
        {+s, -s, -s},  // 1: right, back, bottom
        {+s, +s, -s},  // 2: right, front, bottom
        {-s, +s, -s},  // 3: left, front, bottom
        // Top face (Z = +s)
        {-s, -s, +s},  // 4: left, back, top
        {+s, -s, +s},  // 5: right, back, top
        {+s, +s, +s},  // 6: right, front, top
        {-s, +s, +s}   // 7: left, front, top
    };
    
    // Define edges (pairs of vertex indices)
    int edges[12][2] = {
        // Bottom face
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        // Top face
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        // Vertical edges
        {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };
    
    // Draw edges
    for (int i = 0; i < 12; ++i)
    {
        drawLine3D(g, vertices[edges[i][0]], vertices[edges[i][1]], cubeColour, 1.0f);
    }
}

void Panner3DViewPanel::drawFloorGrid(juce::Graphics& g)
{
    const float s = CUBE_SIZE;
    const int gridDivisions = 8;
    const float gridStep = (s * 2) / gridDivisions;
    
    // Draw grid on Z=0 plane (horizon/ear level in Mach1)
    for (int i = 0; i <= gridDivisions; ++i)
    {
        float offset = -s + i * gridStep;
        
        // Lines parallel to X axis (varying Y)
        drawLine3D(g, Vec3{-s, offset, 0}, Vec3{+s, offset, 0}, floorGridColour, 0.5f);
        
        // Lines parallel to Y axis (varying X)
        drawLine3D(g, Vec3{offset, -s, 0}, Vec3{offset, +s, 0}, floorGridColour, 0.5f);
    }
}

void Panner3DViewPanel::drawListenerPosition(juce::Graphics& g)
{
    // Draw a small indicator at the origin representing the listener
    Vec3 origin{0, 0, 0};
    auto centerPos = project(origin);
    
    // Draw listener icon (small filled circle)
    float listenerRadius = 4.0f * camera.zoom;
    g.setColour(juce::Colour(0xFFFFFFFF).withAlpha(0.6f));
    g.fillEllipse(centerPos.x - listenerRadius, centerPos.y - listenerRadius,
                  listenerRadius * 2, listenerRadius * 2);
    
    // Draw direction indicator (small triangle pointing forward +Y)
    if (camera.preset != CameraPreset::TopDown)
    {
        auto frontPos = project(Vec3{0, 0.15f, 0});
        g.setColour(textColour.withAlpha(0.5f));
        
        juce::Path arrow;
        arrow.startNewSubPath(frontPos.x, frontPos.y - 3);
        arrow.lineTo(frontPos.x + 4, frontPos.y + 3);
        arrow.lineTo(frontPos.x - 4, frontPos.y + 3);
        arrow.closeSubPath();
        g.fillPath(arrow);
    }
}

void Panner3DViewPanel::drawCornerGizmo(juce::Graphics& g)
{
    const float axisLen = 22.0f;
    const float margin = 8.0f;
    const float cx = margin + axisLen + 4;
    const float cy = TOOLBAR_HEIGHT + margin + axisLen + 4;
    const float bgRadius = axisLen + 6;
    
    g.setColour(juce::Colour(0x25000000));
    g.fillEllipse(cx - bgRadius, cy - bgRadius, bgRadius * 2, bgRadius * 2);
    
    Vec3 xDir = rotateByCamera(Vec3{1, 0, 0});
    Vec3 yDir = rotateByCamera(Vec3{0, 1, 0});
    Vec3 zDir = rotateByCamera(Vec3{0, 0, 1});
    
    auto projectGizmo = [&](const Vec3& dir) -> juce::Point<float> {
        return { cx + dir.x * axisLen, cy - dir.z * axisLen };
    };
    
    juce::Point<float> center(cx, cy);
    
    // All axes use the same gray tone, differentiated only by label
    juce::Colour gizmoColour = textColour;
    
    struct AxisDraw { float depth; juce::Point<float> end; juce::String label; };
    AxisDraw axes[3] = {
        { xDir.y, projectGizmo(xDir), "X" },
        { yDir.y, projectGizmo(yDir), "Y" },
        { zDir.y, projectGizmo(zDir), "Z" }
    };
    
    std::sort(std::begin(axes), std::end(axes),
              [](const AxisDraw& a, const AxisDraw& b) { return a.depth < b.depth; });
    
    g.setFont(juce::Font(8.0f, juce::Font::bold));
    
    for (const auto& axis : axes)
    {
        float alpha = juce::jmap(axis.depth, -1.0f, 1.0f, 0.2f, 0.85f);
        g.setColour(gizmoColour.withAlpha(alpha));
        g.drawLine(center.x, center.y, axis.end.x, axis.end.y, 1.0f);
        
        g.fillEllipse(axis.end.x - 2.0f, axis.end.y - 2.0f, 4.0f, 4.0f);
        
        float dx = axis.end.x - center.x;
        float dy = axis.end.y - center.y;
        float len = std::sqrt(dx * dx + dy * dy);
        if (len > 0.01f) {
            float labelX = axis.end.x + (dx / len) * 7.0f;
            float labelY = axis.end.y + (dy / len) * 7.0f;
            g.drawText(axis.label,
                       juce::Rectangle<float>(labelX - 6, labelY - 6, 12, 12),
                       juce::Justification::centred);
        }
    }
}

void Panner3DViewPanel::drawDirectionLabels(juce::Graphics& g)
{
    const float labelDist = CUBE_SIZE * 1.1f;
    
    g.setFont(juce::Font(9.0f));
    g.setColour(textColour.withAlpha(0.7f));
    
    // Front (+Y in Mach1)
    auto frontPos = project(Vec3{0, labelDist, 0});
    g.setColour(textColour.withAlpha(0.7f));
    g.drawText("FRONT", juce::Rectangle<float>(frontPos.x - 25, frontPos.y - 6, 50, 12), 
               juce::Justification::centred);
    
    // Back (-Y in Mach1)
    auto backPos = project(Vec3{0, -labelDist, 0});
    g.setColour(textColour.withAlpha(0.4f));
    g.drawText("BACK", juce::Rectangle<float>(backPos.x - 25, backPos.y - 6, 50, 12), 
               juce::Justification::centred);
    
    g.setColour(textColour.withAlpha(0.6f));
    
    if (camera.preset != CameraPreset::TopDown)
    {
        auto rightPos = project(Vec3{labelDist, 0, 0});
        g.drawText("R", juce::Rectangle<float>(rightPos.x - 10, rightPos.y - 6, 20, 12), 
                   juce::Justification::centred);
        
        auto leftPos = project(Vec3{-labelDist, 0, 0});
        g.drawText("L", juce::Rectangle<float>(leftPos.x - 10, leftPos.y - 6, 20, 12), 
                   juce::Justification::centred);
    }
    else
    {
        auto rightPos = project(Vec3{labelDist, 0, 0});
        g.drawText("RIGHT", juce::Rectangle<float>(rightPos.x - 25, rightPos.y - 6, 50, 12), 
                   juce::Justification::centred);
        
        auto leftPos = project(Vec3{-labelDist, 0, 0});
        g.drawText("LEFT", juce::Rectangle<float>(leftPos.x - 25, leftPos.y - 6, 50, 12), 
                   juce::Justification::centred);
    }
}

void Panner3DViewPanel::drawPannerReticles(juce::Graphics& g)
{
    // Sort reticles by depth (back to front) for proper occlusion
    std::vector<std::pair<float, size_t>> sortedIndices;
    for (size_t i = 0; i < reticles.size(); ++i)
    {
        float depth = getDepth(reticles[i].position);
        sortedIndices.push_back({depth, i});
    }
    
    std::sort(sortedIndices.begin(), sortedIndices.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    
    // Draw reticles back to front
    for (const auto& [depth, idx] : sortedIndices)
    {
        drawReticle(g, reticles[idx]);
    }
}

void Panner3DViewPanel::drawReticle(juce::Graphics& g, const PannerReticle& reticle)
{
    auto screenPos = project(reticle.position);
    float depth = getDepth(reticle.position);
    
    // Closer (higher depth Y) = larger
    float depthFactor = juce::jmap(depth, -2.0f, 2.0f, 0.7f, 1.3f);
    float radius = RETICLE_RADIUS * depthFactor * camera.zoom;
    
    float alpha = juce::jmap(depth, -2.0f, 2.0f, 0.5f, 1.0f);
    juce::Colour colour = reticle.isSelected ? selectedReticleColour : reticle.colour;
    colour = colour.withAlpha(alpha);
    
    // Draw connection line to horizon grid (Z=0 plane)
    Vec3 floorPos = {reticle.position.x, reticle.position.y, 0.0f};
    auto floorScreenPos = project(floorPos);
    g.setColour(colour.withAlpha(alpha * 0.3f));
    g.drawLine(screenPos.x, screenPos.y, floorScreenPos.x, floorScreenPos.y, 1.0f);
    
    // Draw outer ring
    g.setColour(colour);
    g.drawEllipse(screenPos.x - radius, screenPos.y - radius, radius * 2, radius * 2, 2.0f);
    
    // Draw inner dot
    float innerRadius = radius * 0.4f;
    g.fillEllipse(screenPos.x - innerRadius, screenPos.y - innerRadius, 
                  innerRadius * 2, innerRadius * 2);
    
    // Draw crosshairs and extra ring if selected
    if (reticle.isSelected)
    {
        float crossSize = radius * 1.5f;
        g.drawLine(screenPos.x - crossSize, screenPos.y, screenPos.x + crossSize, screenPos.y, 1.5f);
        g.drawLine(screenPos.x, screenPos.y - crossSize, screenPos.x, screenPos.y + crossSize, 1.5f);
        
        // Extra outer ring
        g.drawEllipse(screenPos.x - radius * 1.4f, screenPos.y - radius * 1.4f, 
                      radius * 2.8f, radius * 2.8f, 1.0f);
    }
    
    // Draw label with azimuth/elevation info
    g.setFont(juce::Font(9.0f * depthFactor * camera.zoom));
    g.setColour(labelBackgroundColour);
    
    juce::String labelText = reticle.label;
    if (reticle.isSelected)
    {
        labelText += juce::String::formatted(" (%.0f, %.0f)", reticle.azimuth, reticle.elevation);
    }
    
    float labelWidth = labelText.length() * 5.5f + 6;
    float labelHeight = 12.0f * depthFactor;
    juce::Rectangle<float> labelBounds(screenPos.x - labelWidth/2, 
                                        screenPos.y + radius + 3, 
                                        labelWidth, labelHeight);
    g.fillRoundedRectangle(labelBounds, 2.0f);
    
    g.setColour(colour);
    g.drawText(labelText, labelBounds, juce::Justification::centred);
}

void Panner3DViewPanel::drawCameraInfo(juce::Graphics& g)
{
    g.setColour(textColour.withAlpha(0.6f));
    g.setFont(juce::Font(9.0f));
    
    juce::String viewName;
    switch (camera.preset)
    {
        case CameraPreset::Front:   viewName = "Front View"; break;
        case CameraPreset::TopDown: viewName = "Top-Down View"; break;
        case CameraPreset::Side:    viewName = "Side View"; break;
        case CameraPreset::Back:    viewName = "Back View"; break;
        case CameraPreset::Custom:  viewName = "Custom"; break;
    }
    
    juce::String info = viewName + juce::String::formatted(" | Yaw: %.0f  Pitch: %.0f  Zoom: %.1fx", 
                                                           camera.yaw, camera.pitch, camera.zoom);
    
    auto infoBounds = getLocalBounds().removeFromTop(TOOLBAR_HEIGHT + 20).removeFromBottom(18);
    g.drawText(info, infoBounds.removeFromRight(250).reduced(5), juce::Justification::topRight);
}

void Panner3DViewPanel::drawLine3D(juce::Graphics& g, const Vec3& from, const Vec3& to, 
                                    juce::Colour colour, float thickness)
{
    auto p1 = project(from);
    auto p2 = project(to);
    
    float avgDepth = (getDepth(from) + getDepth(to)) / 2.0f;
    // Higher depth (Y) = closer to viewer = brighter
    float alpha = juce::jmap(avgDepth, -2.0f, 2.0f, 0.35f, 1.0f);
    
    g.setColour(colour.withAlpha(alpha));
    g.drawLine(p1.x, p1.y, p2.x, p2.y, thickness);
}

int Panner3DViewPanel::findReticleAtPoint(juce::Point<int> screenPos)
{
    // Skip toolbar area
    if (screenPos.y < TOOLBAR_HEIGHT)
        return -1;
    
    // Search in reverse depth order (front reticles first)
    std::vector<std::pair<float, int>> sortedByDepth;
    for (int i = 0; i < static_cast<int>(reticles.size()); ++i)
    {
        float depth = getDepth(reticles[i].position);
        sortedByDepth.push_back({depth, i});
    }
    
    // Sort front-to-back (higher depth = closer to camera after rotation)
    std::sort(sortedByDepth.begin(), sortedByDepth.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });
    
    for (const auto& [depth, i] : sortedByDepth)
    {
        auto projected = project(reticles[i].position);
        float dist = std::sqrt(std::pow(projected.x - screenPos.x, 2) + 
                               std::pow(projected.y - screenPos.y, 2));
        
        if (dist < RETICLE_RADIUS * 2.5f * camera.zoom)
        {
            return i;
        }
    }
    return -1;
}

} // namespace Mach1
