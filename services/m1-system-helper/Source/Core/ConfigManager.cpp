#include "ConfigManager.h"

namespace Mach1 {

Result ConfigManager::loadConfig(const juce::File& configFile) {
    if (!configFile.exists()) {
        return Result::fail("Config file does not exist");
    }

    try {
        auto json = juce::JSON::parse(configFile);
        
        if (auto* obj = json.getDynamicObject()) {
            serverPort = obj->getProperty("serverPort");
            helperPort = obj->getProperty("helperPort");
            
            if (serverPort == 0 || helperPort == 0) {
                return Result::fail("Invalid port configuration");
            }
            
            return Result::ok();
        }
    }
    catch (const std::exception& e) {
        return Result::fail(e.what());
    }
    
    return Result::fail("Failed to parse config file");
}

} // namespace Mach1
