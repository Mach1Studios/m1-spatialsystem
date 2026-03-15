#pragma once

#include <JuceHeader.h>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>

namespace Mach1 {

// Enums
enum class ClientType {
    Monitor,
    Player,
    Unknown
};

enum class ServiceOperation {
    Start,
    Stop,
    Restart
};

// Constants
constexpr int DEFAULT_SERVER_PORT = 6345;
constexpr int DEFAULT_HELPER_PORT = 6346;
constexpr juce::int64 CLIENT_TIMEOUT_MS = 10000;
constexpr juce::int64 SERVICE_RESTART_DELAY_MS = 10000;

namespace PannerConfigColours {
inline const juce::Colour m1ActionYellow = juce::Colour::fromRGB(255, 198, 30);
inline const juce::Colour enabledParam = juce::Colour::fromRGB(190, 190, 190);
inline const juce::Colour disabledParam = juce::Colour::fromRGB(63, 63, 63);
inline const juce::Colour backgroundGrey = juce::Colour::fromRGB(40, 40, 40);
inline const juce::Colour gridLines1 = juce::Colour::fromRGBA(68, 68, 68, 51);
inline const juce::Colour gridLines2 = juce::Colour::fromRGB(68, 68, 68);
inline const juce::Colour gridLines3 = juce::Colour::fromRGBA(102, 102, 102, 178);
inline const juce::Colour gridLines4 = juce::Colour::fromRGB(133, 133, 133);
inline const juce::Colour overlayYawRef = juce::Colour::fromRGBA(93, 93, 93, 51);
inline const juce::Colour labelText = juce::Colour::fromRGB(163, 163, 163);
inline const juce::Colour refLabelText = juce::Colour::fromRGB(93, 93, 93);
inline const juce::Colour highlightText = juce::Colour::fromRGB(0, 0, 0);
inline const juce::Colour meterRed = juce::Colour::fromRGB(178, 24, 23);
inline const juce::Colour meterRedDim = juce::Colour::fromRGB(107, 14, 14);
inline const juce::Colour meterYellow = juce::Colour::fromRGB(220, 174, 37);
inline const juce::Colour meterYellowDim = juce::Colour::fromRGB(132, 104, 22);
inline const juce::Colour meterGreen = juce::Colour::fromRGB(67, 174, 56);
inline const juce::Colour meterGreenDim = juce::Colour::fromRGB(40, 104, 33);
} // namespace PannerConfigColours

namespace HelperUIColours {
inline const juce::Colour background = PannerConfigColours::backgroundGrey;
inline const juce::Colour backgroundAlt = PannerConfigColours::gridLines2;
inline const juce::Colour backgroundAltStrong = PannerConfigColours::gridLines3;
inline const juce::Colour overlayBackground = PannerConfigColours::gridLines1;
inline const juce::Colour toolbar = PannerConfigColours::backgroundGrey;
inline const juce::Colour border = PannerConfigColours::gridLines3;
inline const juce::Colour separator = PannerConfigColours::gridLines2;
inline const juce::Colour gridMinor = PannerConfigColours::gridLines1;
inline const juce::Colour gridMajor = PannerConfigColours::gridLines2;
inline const juce::Colour gridEmphasis = PannerConfigColours::gridLines3;
inline const juce::Colour text = PannerConfigColours::labelText;
inline const juce::Colour textDim = PannerConfigColours::refLabelText;
inline const juce::Colour textApp = PannerConfigColours::gridLines4;
inline const juce::Colour accent = PannerConfigColours::m1ActionYellow;
inline const juce::Colour accentText = PannerConfigColours::highlightText;
inline const juce::Colour active = PannerConfigColours::enabledParam;
inline const juce::Colour inactive = PannerConfigColours::disabledParam;
inline const juce::Colour osc = PannerConfigColours::meterYellow;
inline const juce::Colour warning = PannerConfigColours::meterYellow;
inline const juce::Colour error = PannerConfigColours::meterRed;
inline const juce::Colour errorDim = PannerConfigColours::meterRedDim;
inline const juce::Colour success = PannerConfigColours::meterGreen;
inline const juce::Colour successDim = PannerConfigColours::meterGreenDim;
inline const juce::Colour warningDim = PannerConfigColours::meterYellowDim;
inline const juce::Colour overlayYawReference = PannerConfigColours::overlayYawRef;
} // namespace HelperUIColours

// Structures
struct M1RegisteredPlugin {
    // At a minimum we should expect port and if applicable name
    // messages to ensure we do not delete this instance of a registered plugin
    int port = 0;
    int state = 0;
    std::string name;
    juce::OSCColour color;
    int inputMode = 0;
    int outputMode = 0;
    float azimuth = 0.0f, elevation = 0.0f, diverge = 0.0f; // values expected unnormalized
    float gain = 1.0f; // values expected unnormalized
    float stOrbitAzimuth = 0.0f, stSpread = 0.0f; // values expected unnormalized
    int pannerMode = 0;
    bool autoOrbit = false;
    bool isPannerPlugin = false;
    juce::int64 time = 0;
    
    std::shared_ptr<juce::OSCSender> messageSender;
    
    bool operator==(const M1RegisteredPlugin& other) const {
        return port == other.port;
    }
};

struct M1OrientationClientConnection {
    int port = 0;
    ClientType type = ClientType::Unknown;
    bool active = false;
    juce::int64 time = 0;
    
    bool operator==(const M1OrientationClientConnection& other) const {
        return port == other.port;
    }
};

// Utility functions
inline bool isValidPort(int port) {
    return port > 0 && port < 65536;
}

inline juce::String clientTypeToString(ClientType type) {
    switch (type) {
        case ClientType::Monitor: return "monitor";
        case ClientType::Player: return "player";
        default: return "unknown";
    }
}

inline ClientType stringToClientType(const juce::String& str) {
    if (str.equalsIgnoreCase("monitor")) return ClientType::Monitor;
    if (str.equalsIgnoreCase("player")) return ClientType::Player;
    return ClientType::Unknown;
}

// Type aliases
using Result = juce::Result;

} // namespace Mach1
