#include "M1MemoryShareTracker.h"
#include "../Common/Common.h"
#include "../Common/TypesForDataExchange.h"
#include "../Common/SharedPathUtils.h"

// Platform-specific includes for process checking
#ifdef JUCE_MAC
    #include <signal.h>
    #include <sys/types.h>
#elif JUCE_WINDOWS
    #include <windows.h>
#endif

namespace Mach1 {

// MemorySharePannerInfo method implementations
float MemorySharePannerInfo::getAzimuth() const {
    return parameters.getFloat(M1SystemHelperParameterIDs::AZIMUTH, 0.0f);
}

float MemorySharePannerInfo::getElevation() const {
    return parameters.getFloat(M1SystemHelperParameterIDs::ELEVATION, 0.0f);
}

float MemorySharePannerInfo::getDiverge() const {
    return parameters.getFloat(M1SystemHelperParameterIDs::DIVERGE, 0.0f);
}

float MemorySharePannerInfo::getGain() const {
    return parameters.getFloat(M1SystemHelperParameterIDs::GAIN, 1.0f);
}

float MemorySharePannerInfo::getStereoOrbitAzimuth() const {
    return parameters.getFloat(M1SystemHelperParameterIDs::STEREO_ORBIT_AZIMUTH, 0.0f);
}

float MemorySharePannerInfo::getStereoSpread() const {
    return parameters.getFloat(M1SystemHelperParameterIDs::STEREO_SPREAD, 0.0f);
}

float MemorySharePannerInfo::getStereoInputBalance() const {
    return parameters.getFloat(M1SystemHelperParameterIDs::STEREO_INPUT_BALANCE, 0.0f);
}

bool MemorySharePannerInfo::getAutoOrbit() const {
    return parameters.getBool(M1SystemHelperParameterIDs::AUTO_ORBIT, false);
}

int MemorySharePannerInfo::getInputMode() const {
    return parameters.getInt(M1SystemHelperParameterIDs::INPUT_MODE, 0);
}

int MemorySharePannerInfo::getOutputMode() const {
    return parameters.getInt(M1SystemHelperParameterIDs::OUTPUT_MODE, 0);
}

int MemorySharePannerInfo::getState() const {
    return parameters.getInt(M1SystemHelperParameterIDs::STATE, 0);
}

int MemorySharePannerInfo::getPort() const {
    return parameters.getInt(M1SystemHelperParameterIDs::PORT, 0);
}

std::string MemorySharePannerInfo::getDisplayName() const {
    return parameters.getString(M1SystemHelperParameterIDs::DISPLAY_NAME, name);
}

int MemorySharePannerInfo::getColorR() const {
    return parameters.getInt(M1SystemHelperParameterIDs::COLOR_R, 0);
}

int MemorySharePannerInfo::getColorG() const {
    return parameters.getInt(M1SystemHelperParameterIDs::COLOR_G, 0);
}

int MemorySharePannerInfo::getColorB() const {
    return parameters.getInt(M1SystemHelperParameterIDs::COLOR_B, 0);
}

int MemorySharePannerInfo::getColorA() const {
    return parameters.getInt(M1SystemHelperParameterIDs::COLOR_A, 0);
}

// M1MemoryShareTracker implementation stubs
M1MemoryShareTracker::M1MemoryShareTracker(uint32_t consumerId) : consumerId(consumerId) {
}

M1MemoryShareTracker::~M1MemoryShareTracker() {
}

void M1MemoryShareTracker::start() {
    if (isRunning) return;
    
    isRunning = true;
    initialized = true;
    lastScanTime = 0; // Force immediate scan
    
    DBG("[M1MemoryShareTracker] Started memory share tracking with consumer ID: " + juce::String(consumerId));
}

void M1MemoryShareTracker::stop() {
    if (!isRunning) return;
    
    isRunning = false;
    
    // Disconnect from all panners and force cleanup of memory-mapped files
    for (auto& panner : activePanners) {
        disconnectFromPanner(panner);
        // Force cleanup of the memory share object
        if (panner.memoryShare) {
            panner.memoryShare.reset();
        }
    }
    
    activePanners.clear();
    DBG("[M1MemoryShareTracker] Stopped memory share tracking");
}

void M1MemoryShareTracker::update() {
    if (!isRunning || !initialized) return;
    
    auto currentTime = juce::Time::currentTimeMillis();
    
    // Scan for new memory segments periodically
    if (currentTime - lastScanTime > SCAN_INTERVAL_MS) {
        scanForMemorySegments();
        lastScanTime = currentTime;
    }
    
    // Update existing panners more frequently
    updateExistingPanners();
    
    // Cleanup inactive panners
    cleanupInactivePanners();
}

const std::vector<MemorySharePannerInfo>& M1MemoryShareTracker::getActivePanners() const {
    return activePanners;
}

MemorySharePannerInfo* M1MemoryShareTracker::findPanner(uint32_t processId, uintptr_t memoryAddress) {
    for (auto& panner : activePanners) {
        if (panner.processId == processId && (memoryAddress == 0 || panner.memoryAddress == memoryAddress)) {
            return &panner;
        }
    }
    return nullptr;
}

bool M1MemoryShareTracker::hasPanners() const {
    return !activePanners.empty();
}

bool M1MemoryShareTracker::isAvailable() const {
    return true; // Memory share is always available
}

bool M1MemoryShareTracker::connectToPanner(MemorySharePannerInfo& panner) {
    if (panner.isConnected || panner.memoryFilePath.empty()) {
        return panner.isConnected;
    }
    
    try {
        DBG("[M1MemoryShareTracker] Connecting to panner at: " + juce::String(panner.memoryFilePath));
        
        // Create M1MemoryShare instance with explicit file path
        panner.memoryShare = std::make_unique<M1MemoryShare>(
            panner.memorySegmentName, 
            1024 * 1024, // 1MB default size
            8,           // maxQueueSize
            true,        // persistent
            false,       // createMode = false (open existing)
            panner.memoryFilePath  // Explicit file path (std::string)
        );
        
        if (panner.memoryShare->isValid()) {
            // Register as consumer
            if (panner.memoryShare->registerConsumer(consumerId)) {
                panner.isConnected = true;
                panner.lastUpdateTime = juce::Time::currentTimeMillis();
                DBG("[M1MemoryShareTracker] Successfully connected to panner: " + juce::String(panner.name));
                return true;
            }
        }
        
        DBG("[M1MemoryShareTracker] Failed to validate or register with panner");
    } catch (const std::exception& e) {
        // Connection failed
        DBG("[M1MemoryShareTracker] Exception connecting to panner: " + juce::String(e.what()));
        panner.memoryShare.reset();
    }
    
    return false;
}

void M1MemoryShareTracker::disconnectFromPanner(MemorySharePannerInfo& panner) {
    if (panner.isConnected && panner.memoryShare) {
        // Unregister as consumer
        panner.memoryShare->unregisterConsumer(consumerId);
        panner.memoryShare.reset();
        panner.isConnected = false;
    }
}

bool M1MemoryShareTracker::updatePannerData(MemorySharePannerInfo& panner) {
    if (!panner.isConnected || !panner.memoryShare) {
        return false;
    }
    
    try {
        // Read latest audio buffer data
        if (readAudioBufferData(panner)) {
            panner.lastUpdateTime = juce::Time::currentTimeMillis();
            panner.isActive = true;
            return true;
        }
    } catch (const std::exception& e) {
        // Connection may have been lost
        panner.isConnected = false;
    }
    
    return false;
}

void M1MemoryShareTracker::extractParametersFromBuffer(MemorySharePannerInfo& panner) {
    // Extract display name from parameters (if available)
    std::string displayName = panner.parameters.getString(M1SystemHelperParameterIDs::DISPLAY_NAME, "");
    if (!displayName.empty()) {
        panner.name = displayName;
    }
    
    // Note: Other parameters (azimuth, elevation, etc.) are accessed via the getter methods
    // which read directly from panner.parameters
}

bool M1MemoryShareTracker::readAudioBufferData(MemorySharePannerInfo& panner) {
    if (!panner.memoryShare || !panner.memoryShare->isValid()) {
        return false;
    }
    
    // Try to read latest audio buffer with parameters
    ParameterMap parameters;
    juce::AudioBuffer<float> audioBuffer;
    uint64_t dawTimestamp = 0;
    double playheadPosition = 0.0;
    bool isPlaying = false;
    uint64_t bufferId = 0;
    uint32_t updateSource = 0;
    
    if (panner.memoryShare->readAudioBufferWithGenericParameters(
            audioBuffer, parameters, dawTimestamp, playheadPosition, isPlaying, bufferId, updateSource)) {
        
        // Update panner info with latest data
        panner.parameters = parameters;
        panner.dawTimestamp = dawTimestamp;
        panner.playheadPositionInSeconds = playheadPosition;
        panner.isPlaying = isPlaying;
        panner.currentBufferId = bufferId;
        
        // Update audio format info from the buffer
        if (audioBuffer.getNumChannels() > 0) {
            panner.channels = static_cast<uint32_t>(audioBuffer.getNumChannels());
        }
        if (audioBuffer.getNumSamples() > 0) {
            panner.samplesPerBlock = static_cast<uint32_t>(audioBuffer.getNumSamples());
        }
        
        // Extract display name and other parameters
        extractParametersFromBuffer(panner);
        return true;
    }
    
    return false;
}

bool M1MemoryShareTracker::registerAsConsumer(uint32_t consumerId) {
    // TODO: Register this instance as a consumer with the given ID
    this->consumerId = consumerId;
    return true;
}

bool M1MemoryShareTracker::unregisterAsConsumer(uint32_t consumerId) {
    // TODO: Unregister as consumer
    if (this->consumerId == consumerId) {
        this->consumerId = 0;
        return true;
    }
    return false;
}

M1MemoryShareTracker::MemoryShareStats M1MemoryShareTracker::getStats() const {
    MemoryShareStats stats;
    stats.totalPanners = static_cast<uint32_t>(activePanners.size());
    stats.activePanners = 0;
    stats.connectedPanners = 0;
    
    // Count active and connected panners
    for (const auto& panner : activePanners) {
        if (panner.isActive) {
            stats.activePanners++;
        }
        if (panner.isConnected) {
            stats.connectedPanners++;
        }
    }
    
    stats.lastScanTime = lastScanTime;
    
    // Add memory segment names
    for (const auto& panner : activePanners) {
        stats.memorySegmentNames.push_back(panner.memorySegmentName);
    }
    
    return stats;
}

// Core scanning implementation
void M1MemoryShareTracker::scanForMemorySegments()
{
    DBG("=== [M1MemoryShareTracker] Starting memory segment scan ===");
    
    // Get search directories using SharedPathUtils (App Group + cross-platform fallbacks)
    auto sharedDirectories = SharedPathUtils::getAllSharedDirectories();
    std::vector<juce::File> searchDirs;
    
    // Convert string paths to JUCE File objects
    for (const auto& dirPath : sharedDirectories)
    {
        searchDirs.push_back(juce::File(dirPath));
    }
    
    DBG("[M1MemoryShareTracker] Will search in " + juce::String(searchDirs.size()) + " directories:");
    
    int totalActivePanners = 0;
    bool foundActiveFiles = false;
    
    for (const auto& dir : searchDirs)
    {
        DBG("[M1MemoryShareTracker] Checking directory: " + dir.getFullPathName());
        
        if (!dir.exists())
        {
            DBG("[M1MemoryShareTracker] Directory exists: false");
            DBG("[M1MemoryShareTracker] Directory does not exist, skipping");
            continue;
        }
        
        DBG("[M1MemoryShareTracker] Directory exists: true");
        
        // Look for M1SpatialSystem_*.mem files
        auto memoryFiles = dir.findChildFiles(juce::File::findFiles, false, "M1SpatialSystem_*.mem");
        
        DBG("[M1MemoryShareTracker] Found " + juce::String(memoryFiles.size()) + " .mem files in " + dir.getFullPathName());
        
        for (const auto& file : memoryFiles)
        {
            std::string filename = file.getFileNameWithoutExtension().toStdString();
            
            DBG("[M1MemoryShareTracker] Processing panner file: " + file.getFileName() + " from " + dir.getFullPathName());
            
            // Skip if filename doesn't contain "M1Panner"
            if (filename.find("M1Panner") == std::string::npos)
            {
                DBG("[M1MemoryShareTracker] File does not contain M1Panner identifier");
                continue;
            }
            
            DBG("[M1MemoryShareTracker] File contains M1Panner identifier");
            
            // Parse segment name and details
            std::string name;
            uint32_t processId = 0;
            uintptr_t memoryAddress = 0;
            uint64_t timestamp = 0;
            
            if (parsePannerSegmentName(filename, name, processId, memoryAddress, timestamp))
            {
                DBG("[M1MemoryShareTracker] Parsed panner: " + juce::String(name) + " (PID: " + juce::String(processId) + ")");
                
                // PRIMARY CHECK: Is the process still running?
                // This is more reliable than file modification time
                bool processAlive = isProcessRunning(processId);
                
                if (!processAlive) {
                    DBG("[M1MemoryShareTracker] Process " + juce::String(processId) + " is not running, skipping");
                    continue;
                }
                
                // SECONDARY CHECK: File modification time (only as a sanity check for very old files)
                auto fileModTime = file.getLastModificationTime().toMilliseconds();
                auto currentTime = juce::Time::currentTimeMillis();
                auto fileAge = currentTime - fileModTime;
                
                // Only reject if file is VERY old (1 hour) AND process check passed
                // This catches orphaned files from crashed processes
                const int64_t MAX_FILE_AGE_MS = 3600000; // 1 hour
                bool isValid = (fileAge < MAX_FILE_AGE_MS);
                
                if (!isValid) {
                    DBG("[M1MemoryShareTracker] File too old (" + juce::String(fileAge / 60000) + " minutes), skipping");
                    continue;
                }
                
                DBG("[M1MemoryShareTracker] Process alive and file valid, processing panner");
                
                // Check if we already have this panner
                auto existing = findPanner(processId, memoryAddress);
                if (!existing)
                {
                    // Create new panner info
                    MemorySharePannerInfo newPanner;
                    newPanner.name = name;
                    newPanner.processId = processId;
                    newPanner.memoryAddress = memoryAddress;
                    newPanner.creationTimestamp = timestamp;
                    newPanner.memorySegmentName = filename;
                    newPanner.memoryFilePath = file.getFullPathName().toStdString();  // Store full path!
                    newPanner.isActive = true;
                    
                    DBG("[M1MemoryShareTracker] Creating panner with path: " + file.getFullPathName());
                    
                    // Try to connect
                    if (connectToPanner(newPanner))
                    {
                        activePanners.emplace_back(std::move(newPanner));
                        totalActivePanners++;
                        foundActiveFiles = true;
                        DBG("[M1MemoryShareTracker] Connected to new panner: " + name + " (PID: " + std::to_string(processId) + ")");
                    }
                }
                else
                {
                    // Update existing panner's last seen time
                    existing->lastUpdateTime = currentTime;
                    totalActivePanners++;
                    foundActiveFiles = true;
                    
                    // Also try to read latest data
                    if (existing->isConnected) {
                        updatePannerData(*existing);
                    }
                }
            }
        }
        
        // If we found active files in this directory (App Group should be first), 
        // and it's the first directory, we can skip the rest for better performance
        if (foundActiveFiles && &dir == &searchDirs[0]) {
            DBG("[M1MemoryShareTracker] Found active panners in priority directory, skipping remaining directories");
            break;
        }
    }
    
    // Cleanup old/stale files after scanning
    cleanupStaleMemoryFiles();
    
    DBG("[M1MemoryShareTracker] Scan complete. Active panners: " + juce::String(totalActivePanners));
    DBG("=== [M1MemoryShareTracker] End memory segment scan ===");
}

bool M1MemoryShareTracker::parsePannerSegmentName(const std::string& filename,
                                                  std::string& name, 
                                                  uint32_t& processId, 
                                                  uintptr_t& memoryAddress, 
                                                  uint64_t& timestamp) {
    // Expected format: "M1SpatialSystem_M1Panner_PID{processId}_PTR{memoryAddress}_T{timestamp}"
    // Extract the part after "M1SpatialSystem_"
    
    size_t prefixPos = filename.find("M1SpatialSystem_");
    if (prefixPos == std::string::npos) return false;
    
    std::string pannerPart = filename.substr(prefixPos + 16); // 16 = length of "M1SpatialSystem_"
    // Note: We'll set a human-readable name after parsing the PID
    
    // Parse PID
    size_t pidPos = pannerPart.find("_PID");
    if (pidPos == std::string::npos) return false;
    
    size_t pidStart = pidPos + 4; // 4 = length of "_PID"
    size_t pidEnd = pannerPart.find("_", pidStart);
    if (pidEnd == std::string::npos) return false;
    
    try {
        processId = std::stoul(pannerPart.substr(pidStart, pidEnd - pidStart));
    } catch (...) {
        return false;
    }
    
    // Parse PTR
    size_t ptrPos = pannerPart.find("_PTR", pidEnd);
    if (ptrPos == std::string::npos) return false;
    
    size_t ptrStart = ptrPos + 4; // 4 = length of "_PTR"
    size_t ptrEnd = pannerPart.find("_", ptrStart);
    if (ptrEnd == std::string::npos) return false;
    
    try {
        std::string ptrValue = pannerPart.substr(ptrStart, ptrEnd - ptrStart);
        // Try parsing as hex first, then decimal if that fails
        if (ptrValue.length() > 2 && ptrValue.substr(0, 2) == "0x") {
            memoryAddress = std::stoull(ptrValue, nullptr, 16);
        } else {
            // Try hex first (common for memory addresses), then decimal as fallback
            try {
                memoryAddress = std::stoull(ptrValue, nullptr, 16);
            } catch (...) {
                memoryAddress = std::stoull(ptrValue, nullptr, 10);
            }
        }
    } catch (...) {
        return false;
    }
    
    // Parse timestamp
    size_t tPos = pannerPart.find("_T", ptrEnd);
    if (tPos == std::string::npos) return false;
    
    size_t tStart = tPos + 2; // 2 = length of "_T"
    
    try {
        timestamp = std::stoull(pannerPart.substr(tStart));
    } catch (...) {
        return false;
    }
    
    // Set human-readable name: "M1-Panner (PID 12345)"
    name = "M1-Panner (PID " + std::to_string(processId) + ")";
    
    return true;
}

void M1MemoryShareTracker::updateExistingPanners() {
    for (auto& panner : activePanners) {
        if (panner.isConnected) {
            updatePannerData(panner);
        }
    }
}

void M1MemoryShareTracker::cleanupInactivePanners() {
    auto currentTime = juce::Time::currentTimeMillis();
    
    auto it = activePanners.begin();
    while (it != activePanners.end()) {
        bool shouldRemove = false;
        std::string reason;
        
        // Check if not connected
        if (!it->isConnected) {
            shouldRemove = true;
            reason = "not connected";
        }
        // Check if timed out AND process is no longer running
        else if (currentTime - it->lastUpdateTime > PANNER_TIMEOUT_MS) {
            // Only remove if the process is actually dead
            if (!isProcessRunning(it->processId)) {
                shouldRemove = true;
                reason = "process " + std::to_string(it->processId) + " no longer running";
            } else {
                // Process still running - just mark as stale but keep tracking
                // The panner might just not be playing audio
                it->isActive = false;
            }
        }
        
        if (shouldRemove) {
            DBG("[M1MemoryShareTracker] Removing panner: " + juce::String(it->name) + " (" + reason + ")");
            if (it->isConnected) {
                disconnectFromPanner(*it);
            }
            it = activePanners.erase(it);
        } else {
            ++it;
        }
    }
}

void M1MemoryShareTracker::cleanupStaleMemoryFiles() {
    DBG("[M1MemoryShareTracker] Starting cleanup of stale memory files");
    
    auto sharedDirectories = SharedPathUtils::getAllSharedDirectories();
    
    for (const auto& dirPath : sharedDirectories) {
        juce::File dir(dirPath);
        if (!dir.exists()) continue;
        
        auto memoryFiles = dir.findChildFiles(juce::File::findFiles, false, "M1SpatialSystem_*.mem");
        
        for (const auto& file : memoryFiles) {
            bool shouldDelete = false;
            std::string reason;
            
                        // Check: File age (only delete very old files or old files from dead processes)
            auto fileAge = juce::Time::currentTimeMillis() - file.getLastModificationTime().toMilliseconds();
            
            if (fileAge > 7200000) { // 2 hours = definitely stale
                shouldDelete = true;
                reason = "older than 2 hours (" + std::to_string(fileAge / 60000) + " minutes)";
            }
            else if (fileAge > 600000) { // 10 minutes = check if process is still running
                std::string filename = file.getFileNameWithoutExtension().toStdString();
                std::string name;
                uint32_t processId;
                uintptr_t memoryAddress;
                uint64_t timestamp;
                
                if (parsePannerSegmentName(filename, name, processId, memoryAddress, timestamp)) {
                    // Only delete if file is older than 10 minutes AND process is dead
                    if (!isProcessRunning(processId)) {
                        shouldDelete = true;
                        reason = "older than 10 minutes and process " + std::to_string(processId) + " no longer running";
                    }
                }
            }
            // Files newer than 10 minutes are never cleaned up (allows for plugin reload cycles)
            
            if (shouldDelete) {
                DBG("[M1MemoryShareTracker] Deleting stale file: " + file.getFullPathName() + " (reason: " + reason + ")");
                if (!file.deleteFile()) {
                    DBG("[M1MemoryShareTracker] Failed to delete stale file: " + file.getFullPathName());
                }
            }
        }
    }
    
    DBG("[M1MemoryShareTracker] Cleanup of stale memory files complete");
}

bool M1MemoryShareTracker::isProcessRunning(uint32_t processId) {
#ifdef JUCE_MAC
    // On macOS, use kill(pid, 0) to check if process exists
    return (kill(static_cast<pid_t>(processId), 0) == 0);
#elif JUCE_WINDOWS
    // On Windows, try to open the process handle
    HANDLE processHandle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, processId);
    if (processHandle != NULL) {
        CloseHandle(processHandle);
        return true;
    }
    return false;
#else
    // On other platforms, assume process is running (conservative approach)
    return true;
#endif
}

} // namespace Mach1 
