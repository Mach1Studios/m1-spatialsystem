#pragma once

#include <string>
#include <set>
#include <mutex>
#include <atomic>
#include <memory>

#ifdef _WIN32
    #include <windows.h>
#endif

namespace Mach1 {

/**
 * @brief Cross-platform utility class for managing the system helper service on-demand
 * 
 * Supports:
 * - macOS: launchd with socket activation
 * - Windows: Windows Service Control Manager with named pipes
 * - Linux: systemd with socket activation
 * 
 * Usage is identical across all platforms:
 * ```cpp
 * auto& manager = M1SystemHelperManager::getInstance();
 * manager.requestHelperService("M1-Panner");  // Start if needed
 * // ... do work ...
 * manager.releaseHelperService("M1-Panner");  // Stop if no other apps need it
 * ```
 */
class M1SystemHelperManager
{
public:
    static M1SystemHelperManager& getInstance();
    
    /**
     * @brief Request the helper service to start (if not already running)
     * @param appName Name of the requesting application (e.g., "M1-Panner")
     * @return true if service is now running, false on error
     */
    bool requestHelperService(const std::string& appName);
    
    /**
     * @brief Release the helper service (may stop if no other apps need it)
     * @param appName Name of the releasing application
     */
    void releaseHelperService(const std::string& appName);
    
    /**
     * @brief Check if the helper service is currently running
     * @return true if running, false otherwise
     */
    bool isHelperServiceRunning() const;
    
    /**
     * @brief Force start the helper service
     * @return true on success, false on error
     */
    bool startHelperService();
    
    /**
     * @brief Force stop the helper service
     * @return true on success, false on error
     */
    bool stopHelperService();
    
    /**
     * @brief Get the number of applications currently using the service
     * @return Number of active users
     */
    int getActiveUserCount() const { return static_cast<int>(m_activeUsers.size()); }

private:
    M1SystemHelperManager() = default;
    ~M1SystemHelperManager() = default;
    
    // Disable copy/move
    M1SystemHelperManager(const M1SystemHelperManager&) = delete;
    M1SystemHelperManager& operator=(const M1SystemHelperManager&) = delete;
    
    // Platform-specific implementations
    #ifdef __APPLE__
        bool executeLaunchctlCommand(const std::string& command) const;
        bool isServiceLoaded() const;
        bool triggerSocketActivation() const;
        static const char* const SERVICE_LABEL;
        static const char* const SOCKET_PATH;
        static const char* const PLIST_PATH;
    #elif defined(_WIN32)
        bool executeServiceCommand(const std::string& command) const;
        bool isServiceInstalled() const;
        bool triggerNamedPipeActivation() const;
        static const char* const SERVICE_NAME;
        static const char* const PIPE_NAME;
    #else  // Linux
        bool executeSystemctlCommand(const std::string& command) const;
        bool isServiceEnabled() const;
        bool triggerSocketActivation() const;
        static const char* const SERVICE_NAME;
        static const char* const SOCKET_PATH;
    #endif
    
    mutable std::mutex m_mutex;
    std::set<std::string> m_activeUsers;  // Apps currently using the service
    std::atomic<bool> m_serviceRequested{false};
};

} // namespace Mach1 