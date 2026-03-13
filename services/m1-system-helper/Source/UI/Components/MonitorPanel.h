#pragma once

#include <JuceHeader.h>
#include <functional>
#include <vector>

namespace Mach1 {

struct MonitorPanelClientState {
    int port = 0;
    bool active = false;
};

struct MonitorPanelState {
    std::vector<MonitorPanelClientState> monitors;
    float yaw = 0.0f;
    float pitch = 0.0f;
    float roll = 0.0f;
    int channelCount = 8;
};

class MonitorPanel : public juce::Component {
public:
    MonitorPanel();
    ~MonitorPanel() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;

    void updateState(const MonitorPanelState& state);
    void setMonitorComponent(juce::Component* monitorComponent);

    std::function<void(int)> onActiveMonitorSelected;
    std::function<void(float, float, float)> onOrientationChanged;
    std::function<void(int)> onOutputChannelCountChanged;

private:
    int getActiveMonitorPort() const;
    void showMonitorMenu();
    void showSettingsMenu();
    void setControlsEnabled(bool enabled);

    MonitorPanelState currentState;
    bool suppressCallbacks = false;

    std::unique_ptr<juce::Button> settingsButton;
    std::unique_ptr<juce::Button> monitorButton;
    std::unique_ptr<juce::Slider> yawSlider;
    std::unique_ptr<juce::Slider> pitchSlider;
    std::unique_ptr<juce::Slider> rollSlider;

    juce::Component* embeddedMonitor = nullptr;

    juce::Colour dialFaceColour{juce::Colour::fromRGBA(68, 68, 68, 51)};
    juce::Colour dividerColour{juce::Colour::fromRGBA(68, 68, 68, 178)};
    juce::Colour guideColour{0xFFBEBEBE};
    juce::Colour textColour{0xFF5D5D5D};
    juce::Colour dimTextColour{0xFF5D5D5D};
    juce::Colour accentColour{0xFFFFC61E};
    juce::Colour trackColour{0xFF858585};
    juce::Colour disabledColour{0xFF3F3F3F};

    static constexpr int BOTTOM_BAR_HEIGHT = 42;
    static constexpr int MENU_BUTTON_WIDTH = 118;
    static constexpr int MENU_BUTTON_HEIGHT = 24;
    static constexpr int ORIENTATION_SECTION_MIN_HEIGHT = 220;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MonitorPanel)
};

} // namespace Mach1

