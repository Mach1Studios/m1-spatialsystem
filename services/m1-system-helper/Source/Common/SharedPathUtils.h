#pragma once

#include <string>
#include <vector>

namespace Mach1 {

/**
 * Cross-platform utility for getting shared paths between M1-Panner and M1-System-Helper
 * Handles App Groups on macOS, alternative paths on other platforms
 */
class SharedPathUtils {
public:
    /**
     * Get the primary shared directory for .mem files
     * Returns App Group container on macOS (sandboxed), fallback paths otherwise
     */
    static std::string getSharedMemoryDirectory();
    
    /**
     * Get all possible shared directories to search (in priority order)
     * Used by M1MemoryShareTracker to find .mem files
     */
    static std::vector<std::string> getAllSharedDirectories();
    
    /**
     * Get App Group container path (macOS only)
     * Returns empty string on other platforms or if App Group not available
     */
    static std::string getAppGroupContainer(const std::string& groupIdentifier);
    
private:
    // Platform-specific implementations
    static std::string getAppGroupContainerMacOS(const std::string& groupIdentifier);
    static std::vector<std::string> getFallbackDirectories();
};

} // namespace Mach1 