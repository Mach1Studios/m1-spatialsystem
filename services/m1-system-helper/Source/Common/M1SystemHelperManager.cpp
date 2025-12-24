#include "M1SystemHelperManager.h"
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <thread>
#include <chrono>

namespace Mach1 {

// Static constants
const char* const M1SystemHelperManager::SERVICE_LABEL = "com.mach1.spatial.helper";
const char* const M1SystemHelperManager::SOCKET_PATH = "/tmp/com.mach1.spatial.helper.socket";
const char* const M1SystemHelperManager::PLIST_PATH = "/Library/LaunchDaemons/com.mach1.spatial.helper.plist";

M1SystemHelperManager& M1SystemHelperManager::getInstance()
{
    static M1SystemHelperManager instance;
    return instance;
}

bool M1SystemHelperManager::requestHelperService(const std::string& appName)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Add this app to the active users list
    m_activeUsers.insert(appName);
    
    // If service is already running, we're done
    if (isHelperServiceRunning()) {
        std::cout << "[M1SystemHelperManager] Service already running for " << appName << std::endl;
        return true;
    }
    
    std::cout << "[M1SystemHelperManager] Starting helper service for " << appName << std::endl;
    
    // Load the service if not loaded
    if (!isServiceLoaded()) {
        if (!executeLaunchctlCommand("load " + std::string(PLIST_PATH))) {
            std::cerr << "[M1SystemHelperManager] Failed to load service" << std::endl;
            return false;
        }
        // Give it a moment to load
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    // Start the service
    if (!startHelperService()) {
        std::cerr << "[M1SystemHelperManager] Failed to start service" << std::endl;
        return false;
    }
    
    // Trigger socket activation to ensure the service starts
    triggerSocketActivation();
    
    m_serviceRequested = true;
    return true;
}

void M1SystemHelperManager::releaseHelperService(const std::string& appName)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Remove this app from the active users list
    m_activeUsers.erase(appName);
    
    std::cout << "[M1SystemHelperManager] " << appName << " released helper service. "
              << "Active users: " << m_activeUsers.size() << std::endl;
    
    // If no more apps are using the service, we can stop it
    // Note: The service is configured to auto-exit after 30 seconds of inactivity
    // so we don't need to explicitly stop it unless we want immediate cleanup
    if (m_activeUsers.empty()) {
        std::cout << "[M1SystemHelperManager] No more active users. Service will auto-exit." << std::endl;
        m_serviceRequested = false;
        
        // Optionally stop the service immediately
        // stopHelperService();
    }
}

bool M1SystemHelperManager::isHelperServiceRunning() const
{
    // Check if the process is running by trying to connect to the socket
    return triggerSocketActivation();
}

bool M1SystemHelperManager::startHelperService()
{
    return executeLaunchctlCommand("start " + std::string(SERVICE_LABEL));
}

bool M1SystemHelperManager::stopHelperService()
{
    return executeLaunchctlCommand("stop " + std::string(SERVICE_LABEL));
}

bool M1SystemHelperManager::executeLaunchctlCommand(const std::string& command) const
{
    std::string fullCommand = "launchctl " + command + " 2>/dev/null";
    int result = std::system(fullCommand.c_str());
    
    if (result == 0) {
        std::cout << "[M1SystemHelperManager] launchctl " << command << " - SUCCESS" << std::endl;
        return true;
    } else {
        std::cerr << "[M1SystemHelperManager] launchctl " << command << " - FAILED (exit code: " 
                  << result << ")" << std::endl;
        return false;
    }
}

bool M1SystemHelperManager::isServiceLoaded() const
{
    // Use launchctl list to check if service is loaded
    std::string command = "launchctl list | grep " + std::string(SERVICE_LABEL) + " >/dev/null 2>&1";
    return std::system(command.c_str()) == 0;
}

bool M1SystemHelperManager::triggerSocketActivation() const
{
    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        return false;
    }
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    
    bool success = false;
    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
        // Successfully connected - service is running
        success = true;
        
        // Send a simple ping message to trigger activation
        const char* ping = "PING\n";
        send(sockfd, ping, strlen(ping), 0);
        
        // Read response (optional)
        char buffer[256];
        recv(sockfd, buffer, sizeof(buffer) - 1, 0);
    }
    
    close(sockfd);
    return success;
}

} // namespace Mach1 