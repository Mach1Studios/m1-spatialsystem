/*
    Panner3DViewPanel.h
    -------------------
    3D view component displaying panner instances as reticles in a wireframe cube.
    
    Coordinate System (Mach1 Standard):
    - X: left/right (+X is right, -X is left)
    - Y: up/down (+Y is up, -Y is down)
    - Z: forward/back (+Z is forward, -Z is backward)
    
    Azimuth/Elevation (Mach1 AED):
    - Azimuth: left -> right (negative = left, positive = right), 0 = front
    - Elevation: down -> up (negative = down, positive = up), 0 = horizon
    
    Features:
    - Wireframe cube/box visualization with X/Y/Z axis labels
    - Reticle/point for each panner derived from azimuth/elevation
    - Front-facing default camera with top-down view toggle
    - Left-drag orbit (yaw/pitch around center)
    - Mouse wheel zoom
    - Optional shift-drag pan
    - Selection highlighting (synced with panner table)
    
    Uses pure JUCE Graphics with simple 3D math + orthographic projection.
*/

#pragma once

#include <JuceHeader.h>
#include "../../Managers/PannerTrackingManager.h"
#include <vector>
#include <functional>

namespace Mach1 {

/**
 * Camera view presets
 */
enum class CameraPreset {
    Front,      // Default front view (looking at -Z from +Z)
    TopDown,    // Looking down from above (bird's eye view)
    Side,       // Side view (looking at -X from +X)
    Back,       // Rear view (looking at +Z from -Z)
    Custom      // User-controlled orbit position
};

/**
 * Simple 3D camera struct for view transformation
 * Uses Mach1 coordinate system:
 * - Yaw: rotation around Y axis (up)
 * - Pitch: rotation around X axis (right)
 */
struct Camera3D {
    float yaw = 0.0f;       // Rotation around Y axis (degrees), 0 = front view
    float pitch = -15.0f;   // Rotation around X axis (degrees), negative = looking down (first-person)
    float distance = 3.0f;  // Distance from center
    float fov = 60.0f;      // Field of view (not used in ortho)
    
    // Orthographic projection scale
    float zoom = 1.0f;
    
    // Pan offset
    float panX = 0.0f;
    float panY = 0.0f;
    
    CameraPreset preset = CameraPreset::Front;
    
    void reset() {
        setPreset(CameraPreset::Front);
    }
    
    void setPreset(CameraPreset newPreset) {
        preset = newPreset;
        panX = 0.0f;
        panY = 0.0f;
        
        switch (preset) {
            case CameraPreset::Front:
                // First-person view: looking forward (+Z), slight downward tilt
                // Negative pitch = tilt down = things in front appear higher on screen
                yaw = 0.0f;
                pitch = -15.0f;
                break;
            case CameraPreset::TopDown:
                // Bird's eye view: looking down from above
                // Negative pitch (-90) = look straight down, front (+Z) at top of screen
                yaw = 0.0f;
                pitch = -90.0f;
                break;
            case CameraPreset::Side:
                // Looking from the right side
                yaw = 90.0f;
                pitch = -15.0f;
                break;
            case CameraPreset::Back:
                // Looking backward (toward -Z)
                yaw = 180.0f;
                pitch = -15.0f;
                break;
            case CameraPreset::Custom:
                // Keep current values
                break;
        }
    }
};

/**
 * 3D vector for simple math operations
 * Uses Mach1 coordinate system:
 * - X: left/right (+X is right)
 * - Y: up/down (+Y is up)
 * - Z: forward/back (+Z is forward)
 */
struct Vec3 {
    float x, y, z;
    
    Vec3(float x_ = 0.0f, float y_ = 0.0f, float z_ = 0.0f) : x(x_), y(y_), z(z_) {}
    
    Vec3 operator+(const Vec3& v) const { return {x + v.x, y + v.y, z + v.z}; }
    Vec3 operator-(const Vec3& v) const { return {x - v.x, y - v.y, z - v.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    
    float length() const { return std::sqrt(x*x + y*y + z*z); }
    Vec3 normalized() const {
        float len = length();
        return len > 0.0001f ? Vec3{x/len, y/len, z/len} : Vec3{0, 0, 0};
    }
    
    /**
     * Convert from Mach1 AED (Azimuth, Elevation, Diverge/Distance) to XYZ
     * 
     * Mach1 AED Convention:
     * - Azimuth: 0 = front (+Z), positive = right (+X), negative = left (-X)
     *   Range: -180 to +180 degrees
     * - Elevation: 0 = horizon, positive = up (+Y), negative = down (-Y)
     *   Range: -90 to +90 degrees
     * - Diverge/Distance: radius from center
     */
    static Vec3 fromAED(float azimuthDeg, float elevationDeg, float distance = 1.0f) {
        float azRad = azimuthDeg * juce::MathConstants<float>::pi / 180.0f;
        float elRad = elevationDeg * juce::MathConstants<float>::pi / 180.0f;
        
        // Mach1 coordinate conversion:
        // X = distance * cos(elevation) * sin(azimuth)  [right/left]
        // Y = distance * sin(elevation)                  [up/down]
        // Z = distance * cos(elevation) * cos(azimuth)  [forward/back]
        return {
            distance * std::cos(elRad) * std::sin(azRad),   // X: positive azimuth -> right
            distance * std::sin(elRad),                      // Y: positive elevation -> up
            distance * std::cos(elRad) * std::cos(azRad)    // Z: azimuth=0 -> front
        };
    }
    
    // Alias for backward compatibility
    static Vec3 fromSpherical(float azimuthDeg, float elevationDeg, float radius = 1.0f) {
        return fromAED(azimuthDeg, elevationDeg, radius);
    }
};

/**
 * Panner reticle data for rendering
 */
struct PannerReticle {
    Vec3 position;
    juce::String label;
    juce::Colour colour;
    bool isSelected = false;
    int pannerIndex = -1;
    float azimuth = 0.0f;
    float elevation = 0.0f;
};

/**
 * 3D wireframe cube view with panner reticles
 */
class Panner3DViewPanel : public juce::Component,
                          public juce::Timer
{
public:
    Panner3DViewPanel();
    ~Panner3DViewPanel() override;
    
    // Data updates
    void updatePannerData(const std::vector<PannerInfo>& panners);
    void setSelectedPannerIndex(int index);
    int getSelectedPannerIndex() const { return selectedPannerIndex; }
    
    // Camera control
    void resetCamera();
    void setCameraPreset(CameraPreset preset);
    CameraPreset getCameraPreset() const { return camera.preset; }
    const Camera3D& getCamera() const { return camera; }
    
    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    
    // Timer for animation (fake panners)
    void timerCallback() override;
    
    // Callbacks
    std::function<void(int)> onPannerSelected;

private:
    // 3D math helpers
    juce::Point<float> project(const Vec3& worldPos) const;
    Vec3 rotateByCamera(const Vec3& pos) const;
    float getDepth(const Vec3& worldPos) const;
    
    // Drawing
    void drawToolbar(juce::Graphics& g);
    void drawWireframeCube(juce::Graphics& g);
    void drawFloorGrid(juce::Graphics& g);
    void drawAxes(juce::Graphics& g);
    void drawAxisLabels(juce::Graphics& g);
    void drawDirectionLabels(juce::Graphics& g);
    void drawPannerReticles(juce::Graphics& g);
    void drawReticle(juce::Graphics& g, const PannerReticle& reticle);
    void drawCameraInfo(juce::Graphics& g);
    void drawListenerPosition(juce::Graphics& g);
    
    // Line drawing with depth-based alpha
    void drawLine3D(juce::Graphics& g, const Vec3& from, const Vec3& to, 
                    juce::Colour colour, float thickness = 1.0f);
    
    // Hit testing
    int findReticleAtPoint(juce::Point<int> screenPos);
    bool isTopDownButtonHit(juce::Point<int> screenPos);
    bool isFrontViewButtonHit(juce::Point<int> screenPos);
    bool isResetButtonHit(juce::Point<int> screenPos);
    
    // UI elements
    juce::Rectangle<int> topDownButtonBounds;
    juce::Rectangle<int> frontViewButtonBounds;
    juce::Rectangle<int> resetButtonBounds;
    
    // Data
    std::vector<PannerReticle> reticles;
    int selectedPannerIndex = -1;
    
    // Camera state
    Camera3D camera;
    
    // Interaction state
    bool isDragging = false;
    bool isPanning = false;
    juce::Point<int> lastMousePos;
    
    // View bounds
    juce::Rectangle<float> viewBounds;
    float viewScale = 100.0f;
    
    // Toolbar height
    static constexpr int TOOLBAR_HEIGHT = 28;
    
    // Styling - matching reference design
    juce::Colour backgroundColour{0xFF0D0D0D};
    juce::Colour cubeColour{0xFF333333};
    juce::Colour gridColour{0xFF1A1A1A};
    juce::Colour floorGridColour{0xFF222222};
    
    // Mach1 axis colors (muted to match reference)
    juce::Colour axisXColour{0xFF6B6B6B};  // X
    juce::Colour axisYColour{0xFF6B6B6B};  // Y (matches accent)
    juce::Colour axisZColour{0xFF6B6B6B};  // Z
    
    juce::Colour reticleColour{0xFF939393};
    juce::Colour selectedReticleColour{0xFFFFAA00};  // m1-yellow
    juce::Colour textColour{0xFFCCCCCC};
    juce::Colour labelBackgroundColour{0xDD0D0D0D};
    juce::Colour buttonColour{0xFF1F1F1F};
    juce::Colour buttonHoverColour{0xFF2A2A2A};
    juce::Colour buttonActiveColour{0xFF939393};
    juce::Colour toolbarColour{0xFF141414};
    juce::Colour borderColour{0xFF2A2A2A};
    
    // Configuration
    static constexpr float CUBE_SIZE = 1.0f;
    static constexpr float RETICLE_RADIUS = 6.0f;
    static constexpr float ORBIT_SENSITIVITY = 0.5f;
    static constexpr float PAN_SENSITIVITY = 0.01f;
    static constexpr float ZOOM_SENSITIVITY = 0.1f;
    static constexpr float MIN_ZOOM = 0.3f;
    static constexpr float MAX_ZOOM = 3.0f;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Panner3DViewPanel)
};

} // namespace Mach1
