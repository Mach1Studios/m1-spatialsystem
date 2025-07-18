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

// Structures
struct M1RegisteredPlugin {
    // At a minimum we should expect port and if applicable name
    // messages to ensure we do not delete this instance of a registered plugin
    int port = 0;
    int state = 0;
    std::string name;
    juce::OSCColour color;
    int inputMode = 0;
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
