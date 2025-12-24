/*
    SoundfieldDisplayComponent.h
    ----------------------------
    UI component that displays a 3D rotatable view of all M1-Panner instances in the soundfield.
    
    Features:
    - 3D sphere visualization with panner positions
    - Rotatable view (yaw, pitch, roll) for spatial orientation
    - Real-time position updates from panner parameters
    - Visual indicators for selected panner
    - Location and rotation controls
    
    Display Elements:
    - 3D sphere with coordinate grid
    - Panner position dots with labels
    - Rotation controls (yaw, pitch, roll)
    - Location readout
    - Zoom controls
*/

#pragma once

#include <JuceHeader.h>
#include "../../Managers/PannerTrackingManager.h"
#include <vector>
#include <memory>

namespace Mach1 {

/**
 * 3D point structure for soundfield positioning
 */
struct SoundfieldPosition {
    float x, y, z;
    float azimuth, elevation, distance;
    
    SoundfieldPosition(float az = 0.0f, float el = 0.0f, float dist = 1.0f)
        : azimuth(az), elevation(el), distance(dist) {
        updateCartesian();
    }
    
    void updateCartesian() {
        // Convert spherical to cartesian coordinates
        float azRad = azimuth * M_PI / 180.0f;
        float elRad = elevation * M_PI / 180.0f;
        
        x = distance * cos(elRad) * cos(azRad);
        y = distance * cos(elRad) * sin(azRad);
        z = distance * sin(elRad);
    }
};

/**
 * 3D soundfield visualization component
 */
class SoundfieldDisplayComponent : public juce::Component
{
public:
    explicit SoundfieldDisplayComponent();
    ~SoundfieldDisplayComponent() override;
    
    // Data updates
    void updatePannerData(const std::vector<PannerInfo>& panners);
    void setSelectedPanner(int index);
    
    // View control
    void setRotation(float yaw, float pitch, float roll);
    void getRotation(float& yaw, float& pitch, float& roll) const;
    void setZoom(float zoom);
    float getZoom() const { return currentZoom; }
    
    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
    
    // Callbacks
    std::function<void(int)> onPannerSelected;
    std::function<void(float, float, float)> onRotationChanged;

private:
    // Data
    std::vector<PannerInfo> pannerData;
    std::vector<SoundfieldPosition> pannerPositions;
    int selectedPannerIndex = -1;
    
    // View state
    float currentYaw = 0.0f;
    float currentPitch = 0.0f;
    float currentRoll = 0.0f;
    float currentZoom = 1.0f;
    bool isDragging = false;
    juce::Point<int> lastMousePosition;
    
    // UI elements
    std::unique_ptr<juce::Label> titleLabel;
    std::unique_ptr<juce::Label> locationLabel;
    std::unique_ptr<juce::Label> rotationLabel;
    
    // Drawing
    void drawSphere(juce::Graphics& g);
    void drawCoordinateGrid(juce::Graphics& g);
    void drawPannerPositions(juce::Graphics& g);
    void drawRotationControls(juce::Graphics& g);
    void drawLocationReadout(juce::Graphics& g);
    
    // 3D calculations
    juce::Point<float> project3DTo2D(const SoundfieldPosition& pos) const;
    SoundfieldPosition screenToSoundfield(juce::Point<int> screenPos) const;
    void updatePannerPositions();
    
    // Interaction
    int findPannerAtPosition(juce::Point<int> screenPos);
    void handleRotationDrag(juce::Point<int> delta);
    
    // Layout
    juce::Rectangle<int> sphereBounds;
    juce::Rectangle<int> controlsBounds;
    juce::Rectangle<int> infoBounds;
    
    // Styling
    juce::Colour backgroundColour{0xFF1A1A1A};
    juce::Colour sphereColour{0xFF404040};
    juce::Colour gridColour{0xFF606060};
    juce::Colour pannerColour{0xFF4CAF50};
    juce::Colour selectedPannerColour{0xFF0078D4};
    juce::Colour textColour{0xFFE0E0E0};
    
    // Configuration
    static constexpr float SPHERE_RADIUS = 100.0f;
    static constexpr float MIN_ZOOM = 0.5f;
    static constexpr float MAX_ZOOM = 3.0f;
    static constexpr float ZOOM_SENSITIVITY = 0.1f;
    static constexpr float ROTATION_SENSITIVITY = 0.5f;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundfieldDisplayComponent)
};

} // namespace Mach1 