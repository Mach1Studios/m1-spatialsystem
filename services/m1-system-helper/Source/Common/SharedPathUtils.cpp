#include "SharedPathUtils.h"
#include <algorithm>
#include <cstdlib>

// Note: Foundation framework integration is temporarily disabled
// Will be properly implemented in Objective-C++ files

#ifndef MACH1_SHARED_APP_GROUP_ID
#define MACH1_SHARED_APP_GROUP_ID "group.com.mach1.spatial.shared"
#endif

namespace Mach1 {

#ifdef __APPLE__
// Forward declaration for Objective-C++ implementation in SharedPathUtils.mm
extern std::string getAppGroupContainerImpl(const std::string& groupIdentifier);
#endif

namespace {
constexpr const char* kSharedAppGroupId = MACH1_SHARED_APP_GROUP_ID;
}

std::string SharedPathUtils::getSharedMemoryDirectory() {
    auto directories = getAllSharedDirectories();
    return directories.empty() ? "" : directories[0];
}

std::vector<std::string> SharedPathUtils::getAllSharedDirectories() {
    static const std::vector<std::string> cachedDirectories = [] {
        std::vector<std::string> directories;

        const auto appendUnique = [&directories](std::string path) {
            if (path.empty())
                return;

            if (std::find(directories.begin(), directories.end(), path) == directories.end())
                directories.push_back(std::move(path));
        };

        // Priority 1: App Group container (macOS sandboxed)
        std::string appGroupPath = getAppGroupContainer(kSharedAppGroupId);
        if (!appGroupPath.empty())
            appendUnique(appGroupPath + "/Library/Caches/M1-Panner");

        // Priority 2: Real system cache directory (where M1-Panner actually writes)
        if (const char* homeDir = std::getenv("HOME"))
            appendUnique(std::string(homeDir) + "/Library/Caches/M1-Panner");

        // Priority 3+: Platform-specific fallbacks
        for (auto& fallback : getFallbackDirectories())
            appendUnique(std::move(fallback));

        return directories;
    }();

    return cachedDirectories;
}

std::string SharedPathUtils::getAppGroupContainer(const std::string& groupIdentifier) {
#ifdef __APPLE__
    return getAppGroupContainerMacOS(groupIdentifier);
#else
    // App Groups only exist on Apple platforms
    return "";
#endif
}

#ifdef __APPLE__
std::string SharedPathUtils::getAppGroupContainerMacOS(const std::string& groupIdentifier) {
    // Call the implementation from the .mm file within the same namespace
    return getAppGroupContainerImpl(groupIdentifier);
}
#else
std::string SharedPathUtils::getAppGroupContainerMacOS(const std::string& groupIdentifier) {
    // Not on macOS
    return "";
}
#endif

std::vector<std::string> SharedPathUtils::getFallbackDirectories() {
    std::vector<std::string> directories;
    
#ifdef __APPLE__
    // macOS fallback paths
    const char* homeDir = std::getenv("HOME");
    if (homeDir) {
        directories.push_back(std::string(homeDir) + "/Library/Caches/M1-Panner");
        directories.push_back(std::string(homeDir) + "/Library/Containers/com.mach1.spatial.helper/Data/Library/Caches/M1-Panner");
        directories.push_back(std::string(homeDir) + "/Library/Caches/m1-system-helper/M1-Panner");
    }
    directories.push_back("/tmp/M1-Panner");
    
#elif defined(_WIN32)
    // Windows fallback paths
    const char* appData = std::getenv("LOCALAPPDATA");
    if (appData) {
        directories.push_back(std::string(appData) + "\\M1-Panner");
    }
    const char* temp = std::getenv("TEMP");
    if (temp) {
        directories.push_back(std::string(temp) + "\\M1-Panner");
    }
    
#else
    // Linux/other Unix fallback paths
    const char* homeDir = std::getenv("HOME");
    if (homeDir) {
        directories.push_back(std::string(homeDir) + "/.cache/M1-Panner");
        directories.push_back(std::string(homeDir) + "/.local/share/M1-Panner");
    }
    directories.push_back("/tmp/M1-Panner");
#endif
    
    return directories;
}

} // namespace Mach1 