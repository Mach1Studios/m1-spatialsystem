#pragma once

#include "../Common/Common.h"

namespace Mach1 {

class ConfigManager {
public:
    ConfigManager() : serverPort(DEFAULT_SERVER_PORT), helperPort(DEFAULT_HELPER_PORT) {}
    
    juce::Result loadConfig(const juce::File& configFile);
    
    int getServerPort() const { return serverPort; }
    int getHelperPort() const { return helperPort; }
    
private:
    int serverPort;
    int helperPort;
};

} // namespace Mach1
