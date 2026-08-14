/*
    PannerTrackingManager.cpp
    -------------------------
    Implementation of the main panner tracking orchestrator.
    
    Clear Logic Flow:
    1. Try M1MemoryShare first (high performance)
    2. Fall back to OSC if M1MemoryShare unavailable
    3. Provide unified interface regardless of method
    4. Handle transitions between methods gracefully
*/

#include "PannerTrackingManager.h"
#include "M1MemoryShareTracker.h"
#include "OSCPannerTracker.h"
#include "../Common/TypesForDataExchange.h"
#include <Mach1Encode.h>

// Platform-specific includes for process checking
#ifdef JUCE_MAC
    #include <signal.h>
    #include <sys/types.h>
#elif JUCE_WINDOWS
    #include <windows.h>
#endif

namespace Mach1 {

namespace
{
int resolveMemorySharePannerMode(const MemorySharePannerInfo& info)
{
    const bool isotropic = info.parameters.getBool(M1SystemHelperParameterIDs::ISOTROPIC_MODE, true);
    const bool equalpower = info.parameters.getBool(M1SystemHelperParameterIDs::EQUALPOWER_MODE, false);

    if (equalpower)
        return static_cast<int>(Mach1EncodePannerMode::IsotropicEqualPower);

    if (isotropic)
        return static_cast<int>(Mach1EncodePannerMode::IsotropicLinear);

    return static_cast<int>(Mach1EncodePannerMode::PeriphonicLinear);
}
}

PannerTrackingManager::PannerTrackingManager(std::shared_ptr<EventSystem> events)
    : eventSystem(std::move(events))
    , lastScanTime(0)
{
    // Initialize tracking components
    memoryShareTracker = std::make_unique<M1MemoryShareTracker>(CONSUMER_ID);
    // Note: OSC tracker will be initialized when pluginManager is available
    
    DBG("[PannerTrackingManager] Created with consumer ID: " + std::to_string(CONSUMER_ID));
}

PannerTrackingManager::~PannerTrackingManager() {
    stop();
}

void PannerTrackingManager::initializeOSCTracker(PluginManager* pluginManager) {
    if (!oscTracker && pluginManager) {
        oscTracker = std::make_unique<OSCPannerTracker>(pluginManager);
        DBG("[PannerTrackingManager] OSC tracker initialized with plugin manager");
    }
}

void PannerTrackingManager::start() {
    if (initialized) {
        return;
    }
    
    DBG("[PannerTrackingManager] Starting panner tracking...");
    
    // Try to start M1MemoryShare tracking first
    if (memoryShareTracker) {
        memoryShareTracker->start();
        DBG("[PannerTrackingManager] M1MemoryShare tracker started");
    }
    
    // OSC tracker will be started when pluginManager is available
    
    initialized = true;
    lastScanTime = juce::Time::currentTimeMillis();
    
    DBG("[PannerTrackingManager] Started successfully");
}

void PannerTrackingManager::stop() {
    if (!initialized) {
        return;
    }
    
    DBG("[PannerTrackingManager] Stopping panner tracking...");
    
    // Stop both trackers
    if (memoryShareTracker) {
        memoryShareTracker->stop();
    }
    
    if (oscTracker) {
        oscTracker->stop();
    }
    
    // Clear state
    {
        const juce::ScopedLock lock(pannersMutex);
        activePanners.clear();
    }
    
    usingMemoryShare = false;
    usingOSC = false;
    initialized = false;
    
    DBG("[PannerTrackingManager] Stopped successfully");
}

void PannerTrackingManager::update() {
    if (!initialized) {
        return;
    }
    
    auto currentTime = juce::Time::currentTimeMillis();
    
    // Scan for panners at regular intervals
    if (currentTime - lastScanTime >= SCAN_INTERVAL_MS) {
        scanForPanners();
        lastScanTime = currentTime;
    }
    
    // Update tracking method if needed
    updateTrackingMethod();
    
    // Clean up inactive panners
    cleanupInactivePanners();
}

// =============================================================================
// CORE LOGIC - Easy to Follow
// =============================================================================

void PannerTrackingManager::scanForPanners() {
    tryMemoryShareTracking();
    tryOSCTracking();
    mergeTrackingResults();
}

void PannerTrackingManager::tryMemoryShareTracking() {
    if (!memoryShareTracker) {
        return;
    }
    
    // Update M1MemoryShare tracker
    memoryShareTracker->update();
    
    // Check if we found any panners
    bool foundPanners = memoryShareTracker->hasPanners();
    bool wasUsingMemoryShare = usingMemoryShare;
    
    usingMemoryShare = foundPanners && memoryShareTracker->isAvailable();
    
    // Notify if tracking method changed
    if (wasUsingMemoryShare != usingMemoryShare) {
        DBG("[PannerTrackingManager] M1MemoryShare tracking " <<
            (usingMemoryShare ? "ENABLED" : "DISABLED"));
        publishTrackingMethodChanged(usingMemoryShare, usingOSC);
    }
    
}

void PannerTrackingManager::tryOSCTracking() {
    if (!oscTracker) {
        return;  // OSC tracker not available yet
    }
    
    // Update OSC tracker
    oscTracker->update();
    
    // Check if we found any panners
    bool foundPanners = oscTracker->hasPanners();
    bool wasUsingOSC = usingOSC;
    
    usingOSC = foundPanners && oscTracker->isAvailable();
    
    // Notify if tracking method changed
    if (wasUsingOSC != usingOSC) {
        DBG("[PannerTrackingManager] OSC tracking " <<
            (usingOSC ? "ENABLED" : "DISABLED"));
        publishTrackingMethodChanged(usingMemoryShare, usingOSC);
    }
    
}

void PannerTrackingManager::updateTrackingMethod() {
    // Priority: M1MemoryShare > OSC
    // If M1MemoryShare becomes available, prefer it over OSC
    if (memoryShareTracker && memoryShareTracker->isAvailable() && 
        memoryShareTracker->hasPanners() && !usingMemoryShare) {
        
        DBG("[PannerTrackingManager] Switching to M1MemoryShare tracking");
        usingMemoryShare = true;
        publishTrackingMethodChanged(usingMemoryShare, usingOSC);
    }
}

void PannerTrackingManager::mergeTrackingResults() {
    const juce::ScopedLock lock(pannersMutex);
    
    std::vector<PannerInfo> foundPanners;
    
    // Collect from MemoryShare (primary, high-fidelity data)
    if (memoryShareTracker) {
        const auto& memoryPanners = memoryShareTracker->getActivePanners();
        for (const auto& memoryPanner : memoryPanners) {
            foundPanners.push_back(convertFromMemoryShare(memoryPanner));
        }
    }
    
    // Collect from OSC (always available, used when MemoryShare is disabled)
    if (oscTracker) {
        const auto& oscPanners = oscTracker->getActivePanners();
        for (const auto& oscPlugin : oscPanners) {
            PannerInfo oscPanner = convertFromOSC(oscPlugin);
            
            // Skip if a MemoryShare panner already covers this port
            bool alreadyCovered = false;
            for (auto& existing : foundPanners) {
                if (existing.port == oscPanner.port && existing.isMemoryShareBased) {
                    existing.pluginInstanceId = oscPanner.pluginInstanceId;
                    existing.projectBindingId = oscPanner.projectBindingId;
                    existing.projectDisplayName = oscPanner.projectDisplayName;
                    alreadyCovered = true;
                    break;
                }
            }
            if (!alreadyCovered) {
                foundPanners.push_back(oscPanner);
            }
        }
    }
    
    auto currentTime = juce::Time::currentTimeMillis();
    
    // Update existing panners with new data
    for (auto& existingPanner : activePanners) {
        for (const auto& foundPanner : foundPanners) {
            bool matches = false;
            if (existingPanner.isMemoryShareBased && foundPanner.isMemoryShareBased) {
                // processId alone is NOT unique: DAWs host every plugin
                // instance in one process. The memory address disambiguates.
                matches = (existingPanner.processId == foundPanner.processId && foundPanner.processId != 0
                           && existingPanner.memoryAddress == foundPanner.memoryAddress);
            } else {
                matches = (existingPanner.port == foundPanner.port && foundPanner.port != 0);
            }
            
            if (matches) {
                // Promote an OSC-tracked entry to memory-share identity as
                // soon as the instance's shared-memory stream is discovered
                // (the port joins both views of the same plugin instance).
                // Without this the merged entry would keep isMemoryShareBased
                // = false and the capture engine would skip its audio.
                if (foundPanner.isMemoryShareBased && !existingPanner.isMemoryShareBased) {
                    existingPanner.isMemoryShareBased = true;
                    existingPanner.processId = foundPanner.processId;
                    existingPanner.memoryAddress = foundPanner.memoryAddress;
                }
                // Audio format only flows through memory share (OSC entries
                // carry hardcoded defaults that must not stomp real values)
                if (foundPanner.isMemoryShareBased) {
                    existingPanner.channels = foundPanner.channels;
                    existingPanner.sampleRate = foundPanner.sampleRate;
                    existingPanner.msSinceLastBlock = foundPanner.msSinceLastBlock;
                    existingPanner.externalStreamingActive = foundPanner.externalStreamingActive;
                    // The port arrives via the block parameter payload and can
                    // lag discovery by a few blocks; adopt it as soon as it is
                    // known so this entry can be joined with its OSC twin.
                    if (foundPanner.port != 0)
                        existingPanner.port = foundPanner.port;
                }
                existingPanner.name = foundPanner.name;
                existingPanner.azimuth = foundPanner.azimuth;
                existingPanner.elevation = foundPanner.elevation;
                existingPanner.diverge = foundPanner.diverge;
                existingPanner.gain = foundPanner.gain;
                existingPanner.stereoOrbitAzimuth = foundPanner.stereoOrbitAzimuth;
                existingPanner.stereoSpread = foundPanner.stereoSpread;
                existingPanner.stereoInputBalance = foundPanner.stereoInputBalance;
                existingPanner.dawTimestamp = foundPanner.dawTimestamp;
                existingPanner.playheadPositionInSeconds = foundPanner.playheadPositionInSeconds;
                existingPanner.isPlaying = foundPanner.isPlaying;
                existingPanner.currentBufferId = foundPanner.currentBufferId;
                existingPanner.inputMode = foundPanner.inputMode;
                existingPanner.outputMode = foundPanner.outputMode;
                existingPanner.pannerMode = foundPanner.pannerMode;
                existingPanner.autoOrbit = foundPanner.autoOrbit;
                existingPanner.controlRevision = foundPanner.controlRevision;
                existingPanner.state = foundPanner.state;
                existingPanner.color = foundPanner.color;
                if (!foundPanner.pluginInstanceId.empty())
                    existingPanner.pluginInstanceId = foundPanner.pluginInstanceId;
                if (!foundPanner.projectBindingId.empty())
                    existingPanner.projectBindingId = foundPanner.projectBindingId;
                if (!foundPanner.projectDisplayName.empty())
                    existingPanner.projectDisplayName = foundPanner.projectDisplayName;
                existingPanner.lastUpdateTime = currentTime;
                existingPanner.isActive = true;
                existingPanner.connectionStatus = PannerConnectionStatus::Active;
                break;
            }
        }
    }
    
    // Add new panners
    for (const auto& foundPanner : foundPanners) {
        bool isNew = true;
        for (const auto& existing : activePanners) {
            if (existing.isMemoryShareBased && foundPanner.isMemoryShareBased) {
                if (existing.processId == foundPanner.processId && foundPanner.processId != 0
                    && existing.memoryAddress == foundPanner.memoryAddress) {
                    isNew = false;
                    break;
                }
            } else {
                if (existing.port == foundPanner.port && foundPanner.port != 0) {
                    isNew = false;
                    break;
                }
            }
        }
        
        if (isNew) {
            PannerInfo newPanner = foundPanner;
            newPanner.lastUpdateTime = currentTime;
            activePanners.push_back(newPanner);
            publishPannerAdded(newPanner);
            DBG("[PannerTrackingManager] Added new panner: " + newPanner.name + 
                " (PID: " + std::to_string(newPanner.processId) + 
                ", port: " + std::to_string(newPanner.port) + 
                ", source: " + std::string(newPanner.isMemoryShareBased ? "MemoryShare" : "OSC") + ")");
        }
    }

    // Collapse duplicate entries that refer to the same plugin instance.
    // These arise from discovery races: a memory-share segment can be found
    // while its parameter payload still carries port 0, creating one entry,
    // while the instance's OSC registration creates another; once the port
    // arrives and the OSC entry is promoted, both rows share one identity.
    for (auto it = activePanners.begin(); it != activePanners.end();) {
        bool duplicate = false;
        for (auto keep = activePanners.begin(); keep != it; ++keep) {
            const bool sameMemoryIdentity =
                it->isMemoryShareBased && keep->isMemoryShareBased
                && it->memoryAddress != 0
                && it->processId == keep->processId
                && it->memoryAddress == keep->memoryAddress;
            // A port match only joins an OSC row with a memory-share row (two
            // views of one instance). Two memory-share rows with different
            // memory addresses are distinct instances even if their parameter
            // payloads claim the same port (stale/incorrect port data must
            // never merge real audio streams).
            const bool oscJoinedWithMemoryShare =
                it->port != 0 && it->port == keep->port
                && (it->isMemoryShareBased != keep->isMemoryShareBased);

            if (sameMemoryIdentity || oscJoinedWithMemoryShare) {
                // Keep the richer row: prefer one that already knows its port
                if (keep->port == 0 && it->port != 0)
                    *keep = *it;
                duplicate = true;
                break;
            }
        }

        if (duplicate) {
            DBG("[PannerTrackingManager] Collapsed duplicate panner entry: " + it->name +
                " (port: " + std::to_string(it->port) +
                ", addr: " + std::to_string(static_cast<unsigned long long>(it->memoryAddress)) + ")");
            it = activePanners.erase(it);
        } else {
            ++it;
        }
    }

    // Overlay in-flight helper edits (sendParameterUpdate) so tracking data -
    // and every UI reading it - keeps showing the edited value until the
    // panner acknowledges it, instead of snapping back for a few cycles.
    for (auto& panner : activePanners) {
        if (panner.isMemoryShareBased) {
            applyPendingEditOverlay(panner, currentTime);
        }
    }
}

void PannerTrackingManager::applyPendingEditOverlay(PannerInfo& panner, juce::int64 currentTimeMs)
{
    // Caller holds pannersMutex
    auto it = pendingEdits.find(instanceKey(panner.processId, panner.memoryAddress));
    if (it == pendingEdits.end())
        return;

    auto& pending = it->second;
    const bool acknowledged = panner.controlRevision >= pending.revision;
    const bool expired = (currentTimeMs - pending.sentAtMs) > PENDING_EDIT_TIMEOUT_MS;
    if (acknowledged || expired)
    {
        if (expired && !acknowledged)
            DBG("[PannerTrackingManager] Pending edit expired unacknowledged for " + panner.name);
        pendingEdits.erase(it);
        return;
    }

    for (const auto& [paramID, value] : pending.values)
    {
        switch (paramID)
        {
            case M1SystemHelperParameterIDs::AZIMUTH:              panner.azimuth = value; break;
            case M1SystemHelperParameterIDs::ELEVATION:            panner.elevation = value; break;
            case M1SystemHelperParameterIDs::DIVERGE:              panner.diverge = value; break;
            case M1SystemHelperParameterIDs::GAIN:                 panner.gain = value; break;
            case M1SystemHelperParameterIDs::STEREO_ORBIT_AZIMUTH: panner.stereoOrbitAzimuth = value; break;
            case M1SystemHelperParameterIDs::STEREO_SPREAD:        panner.stereoSpread = value; break;
            case M1SystemHelperParameterIDs::STEREO_INPUT_BALANCE: panner.stereoInputBalance = value; break;
            case M1SystemHelperParameterIDs::AUTO_ORBIT:           panner.autoOrbit = value >= 0.5f; break;
            default: break;
        }
    }
}

// =============================================================================
// PANNER ACCESS
// =============================================================================

std::vector<PannerInfo> PannerTrackingManager::getActivePanners() const {
    const juce::ScopedLock lock(pannersMutex);
    return activePanners;
}

PannerInfo* PannerTrackingManager::findPanner(int port, uint32_t processId) {
    const juce::ScopedLock lock(pannersMutex);
    
    for (auto& panner : activePanners) {
        if (panner.port == port && (processId == 0 || panner.processId == processId)) {
            return &panner;
        }
    }
    
    return nullptr;
}

bool PannerTrackingManager::hasPanners() const {
    const juce::ScopedLock lock(pannersMutex);
    return !activePanners.empty();
}

juce::String PannerTrackingManager::getTrackingStatus() const {
    if (usingMemoryShare && usingOSC) {
        return "M1MemoryShare + OSC";
    } else if (usingMemoryShare) {
        return "M1MemoryShare";
    } else if (usingOSC) {
        return "OSC";
    } else {
        return "None";
    }
}

// =============================================================================
// COMMUNICATION
// =============================================================================

void PannerTrackingManager::sendToAllPanners(const juce::OSCMessage& message) {
    // Send via OSC (M1MemoryShare doesn't support sending messages)
    if (usingOSC && oscTracker) {
        oscTracker->sendToAllPanners(message);
    }
}

void PannerTrackingManager::sendToPanner(const PannerInfo& panner, const juce::OSCMessage& message) {
    // Send via OSC (M1MemoryShare doesn't support sending messages)
    if (usingOSC && oscTracker) {
        oscTracker->sendToPanner(panner.port, message);
    }
}

// =============================================================================
// BIDIRECTIONAL PARAMETER UPDATES
// =============================================================================

uint32_t PannerTrackingManager::parameterIdForName(const std::string& parameterName) {
    // The well-known IDs are hand-picked constants, NOT string hashes, so
    // hashString() must never be used for these names - the panner compares
    // incoming control messages against the constants and would silently
    // ignore hashed IDs.
    if (parameterName == "azimuth")            return M1SystemHelperParameterIDs::AZIMUTH;
    if (parameterName == "elevation")          return M1SystemHelperParameterIDs::ELEVATION;
    if (parameterName == "diverge")            return M1SystemHelperParameterIDs::DIVERGE;
    if (parameterName == "gain")               return M1SystemHelperParameterIDs::GAIN;
    if (parameterName == "stereoOrbitAzimuth") return M1SystemHelperParameterIDs::STEREO_ORBIT_AZIMUTH;
    if (parameterName == "stereoSpread")       return M1SystemHelperParameterIDs::STEREO_SPREAD;
    if (parameterName == "stereoInputBalance") return M1SystemHelperParameterIDs::STEREO_INPUT_BALANCE;
    if (parameterName == "autoOrbit")          return M1SystemHelperParameterIDs::AUTO_ORBIT;
    if (parameterName == "isotropicMode")      return M1SystemHelperParameterIDs::ISOTROPIC_MODE;
    if (parameterName == "equalpowerMode")     return M1SystemHelperParameterIDs::EQUALPOWER_MODE;
    if (parameterName == "gainCompensationMode") return M1SystemHelperParameterIDs::GAIN_COMPENSATION_MODE;
    return 0; // unknown parameter - refuse to send rather than send garbage
}

bool PannerTrackingManager::sendParameterUpdate(const PannerInfo& panner, const std::string& parameterName, float value) {
    if (!panner.isMemoryShareBased || !memoryShareTracker)
        return false;

    const uint32_t paramID = parameterIdForName(parameterName);
    if (paramID == 0) {
        DBG("[PannerTrackingManager] Refusing to send unknown parameter: " + parameterName);
        return false;
    }

    // Find the panner's M1MemoryShare instance via the tracker
    auto* pannerInfo = memoryShareTracker->findPanner(panner.processId, panner.memoryAddress);
    auto share = pannerInfo != nullptr ? pannerInfo->getShare() : nullptr;
    if (!share || !share->isValid())
        return false;

    // The revision rides in the control message's intValue; the panner echoes
    // the highest applied revision back via CONTROL_REVISION in its block
    // parameters, which clears the pending-edit overlay below.
    int32_t revision = 0;
    {
        const juce::ScopedLock lock(pannersMutex);
        revision = ++controlRevisionCounter;
        auto& pending = pendingEdits[instanceKey(panner.processId, panner.memoryAddress)];
        pending.values[paramID] = value;
        pending.revision = revision;
        pending.sentAtMs = juce::Time::currentTimeMillis();
    }

    return share->writeControlMessage(paramID, ParameterType::FLOAT, value, revision);
}

bool PannerTrackingManager::sendParameterUpdate(const PannerInfo& panner, const std::string& parameterName, int value) {
    return sendParameterUpdate(panner, parameterName, static_cast<float>(value));
}

bool PannerTrackingManager::sendParameterUpdate(const PannerInfo& panner, const std::string& parameterName, bool value) {
    return sendParameterUpdate(panner, parameterName, value ? 1.0f : 0.0f);
}

// =============================================================================
// OSC FALLBACK (Backward Compatibility)
// =============================================================================

juce::Result PannerTrackingManager::registerOSCPanner(const M1RegisteredPlugin& plugin) {
    if (oscTracker) {
        bool success = oscTracker->registerPanner(plugin);
        return success ? juce::Result::ok() : juce::Result::fail("Failed to register OSC panner");
    }
    return juce::Result::fail("OSC tracker not available");
}

void PannerTrackingManager::removeOSCPanner(int port) {
    if (oscTracker) {
        oscTracker->removePanner(port);
    }
}

// =============================================================================
// CONSUMER MANAGEMENT
// =============================================================================

bool PannerTrackingManager::registerAsConsumer(uint32_t consumerId) {
    if (memoryShareTracker) {
        return memoryShareTracker->registerAsConsumer(consumerId);
    }
    return false;
}

bool PannerTrackingManager::unregisterAsConsumer(uint32_t consumerId) {
    if (memoryShareTracker) {
        return memoryShareTracker->unregisterAsConsumer(consumerId);
    }
    return false;
}

// =============================================================================
// CONVERSION UTILITIES
// =============================================================================

PannerInfo PannerTrackingManager::convertFromMemoryShare(const MemorySharePannerInfo& info) {
    PannerInfo panner;
    
    // Identity - use display name from parameters if available
    panner.port = info.getPort();
    panner.name = info.getDisplayName();  // Uses DISPLAY_NAME parameter, falls back to info.name
    panner.processId = info.processId;
    panner.memoryAddress = info.memoryAddress;
    
    // State
    panner.isActive = info.isActive;
    panner.isMemoryShareBased = true;
    panner.lastUpdateTime = info.lastUpdateTime;
    
    // Connection status - infer from isStale flag
    if (info.isStale) {
        panner.connectionStatus = PannerConnectionStatus::Stale;
    } else if (info.isActive) {
        panner.connectionStatus = PannerConnectionStatus::Active;
    } else {
        panner.connectionStatus = PannerConnectionStatus::Disconnected;
    }
    
    // Audio info
    panner.sampleRate = info.sampleRate;
    panner.channels = info.channels;
    panner.samplesPerBlock = info.samplesPerBlock;
    
    // Panner parameters
    panner.azimuth = info.getAzimuth();
    panner.elevation = info.getElevation();
    panner.diverge = info.getDiverge();
    panner.gain = info.getGain();
    panner.stereoOrbitAzimuth = info.getStereoOrbitAzimuth();
    panner.stereoSpread = info.getStereoSpread();
    panner.stereoInputBalance = info.getStereoInputBalance();
    panner.controlRevision = info.parameters.getInt(M1SystemHelperParameterIDs::CONTROL_REVISION, 0);

    // Block-flow freshness + the plugin's own streaming-state report, so the
    // UI can tell "streaming" / "alive but native multichannel" / "silent"
    panner.msSinceLastBlock = info.lastDataTime > 0
        ? juce::jmax<juce::int64>(0, juce::Time::currentTimeMillis() - info.lastDataTime)
        : -1;
    panner.externalStreamingActive = info.parameters.getBool(M1SystemHelperParameterIDs::EXTERNAL_ACTIVE, true);
    
    // DAW integration
    panner.dawTimestamp = info.dawTimestamp;
    panner.playheadPositionInSeconds = info.playheadPositionInSeconds;
    panner.isPlaying = info.isPlaying;
    
    // Buffer tracking
    panner.currentBufferId = info.currentBufferId;
    panner.queuedBufferCount = info.queuedBufferCount;
    panner.consumerCount = info.consumerCount;
    
    // Visual
    panner.color = juce::OSCColour::fromInt32(
        (static_cast<uint32_t>(info.getColorA()) << 24) |
        (static_cast<uint32_t>(info.getColorR()) << 16) |
        (static_cast<uint32_t>(info.getColorG()) << 8) |
        static_cast<uint32_t>(info.getColorB()));
    
    // Modes
    panner.inputMode = info.getInputMode();
    panner.outputMode = info.getOutputMode();
    panner.pannerMode = resolveMemorySharePannerMode(info);
    panner.autoOrbit = info.getAutoOrbit();
    panner.state = info.getState();
    
    return panner;
}

PannerInfo PannerTrackingManager::convertFromOSC(const M1RegisteredPlugin& plugin) {
    PannerInfo panner;
    
    // Identity
    panner.port = plugin.port;
    panner.name = plugin.name.empty() ? ("OSC Panner " + std::to_string(plugin.port)) : plugin.name;
    panner.processId = plugin.hostProcessId;
    panner.pluginInstanceId = plugin.pluginInstanceId.toStdString();
    panner.projectBindingId = plugin.projectBindingId.toStdString();
    panner.projectDisplayName = plugin.projectDisplayName.toStdString();
    
    // State
    panner.isActive = true;
    panner.isMemoryShareBased = false;
    panner.lastUpdateTime = plugin.time;
    
    // Audio info (defaults for OSC)
    panner.sampleRate = 44100;
    panner.channels = 1;
    panner.samplesPerBlock = 512;
    
    // Panner parameters - use real OSC data or reasonable defaults
    panner.azimuth = plugin.azimuth;
    panner.elevation = plugin.elevation;
    panner.diverge = plugin.diverge;
    panner.gain = plugin.gain;
    panner.stereoOrbitAzimuth = plugin.stOrbitAzimuth;
    panner.stereoSpread = plugin.stSpread;
    
    // Visual
    panner.color = plugin.color;
    panner.state = plugin.state;
    
    // Modes
    panner.inputMode = plugin.inputMode;
    panner.outputMode = plugin.outputMode;
    panner.pannerMode = plugin.pannerMode;
    panner.autoOrbit = plugin.autoOrbit;
    
    return panner;
}

// =============================================================================
// STATISTICS
// =============================================================================

PannerTrackingManager::TrackingStats PannerTrackingManager::getTrackingStats() const {
    TrackingStats stats;
    
    if (memoryShareTracker) {
        auto memStats = memoryShareTracker->getStats();
        stats.memorySharePanners = memStats.activePanners;
        stats.memoryShareAvailable = memStats.scannerActive;
    }
    
    if (oscTracker) {
        auto oscStats = oscTracker->getStats();
        stats.oscPanners = oscStats.activePanners;
        stats.oscAvailable = oscStats.pluginManagerAvailable;
    }
    
    stats.totalPanners = static_cast<uint32_t>(activePanners.size());
    stats.lastScanTime = lastScanTime;
    
    return stats;
}

// =============================================================================
// TESTING
// =============================================================================

void PannerTrackingManager::injectFakePanners(const std::vector<PannerInfo>& panners) {
    const juce::ScopedLock lock(pannersMutex);
    auto currentTime = juce::Time::currentTimeMillis();
    
    for (const auto& fake : panners) {
        bool found = false;
        for (auto& existing : activePanners) {
            if (existing.processId == fake.processId) {
                existing.azimuth = fake.azimuth;
                existing.elevation = fake.elevation;
                existing.diverge = fake.diverge;
                existing.gain = fake.gain;
                existing.isActive = fake.isActive;
                existing.isPlaying = fake.isPlaying;
                existing.lastUpdateTime = currentTime;
                existing.connectionStatus = PannerConnectionStatus::Active;
                found = true;
                break;
            }
        }
        if (!found) {
            PannerInfo newPanner = fake;
            newPanner.lastUpdateTime = currentTime;
            activePanners.push_back(newPanner);
            publishPannerAdded(newPanner);
        }
    }
}

// =============================================================================
// CLEANUP
// =============================================================================

void PannerTrackingManager::cleanupInactivePanners() {
    const juce::ScopedLock lock(pannersMutex);
    
    auto currentTime = juce::Time::currentTimeMillis();
    
    auto it = activePanners.begin();
    while (it != activePanners.end()) {
        bool shouldRemove = false;
        auto timeSinceUpdate = currentTime - it->lastUpdateTime;
        
        // Only consider removal if timed out
        if (timeSinceUpdate > PANNER_TIMEOUT_MS) {
            // For memory-share based panners, check if process is still running
            if (it->isMemoryShareBased && it->processId != 0) {
                if (!isProcessRunning(it->processId)) {
                    shouldRemove = true;
                    it->connectionStatus = PannerConnectionStatus::Disconnected;
                    DBG("[PannerTrackingManager] Removing panner (process dead): " + it->name);
                } else {
                    // Process is running but no updates - mark as stale (not playing audio)
                    it->connectionStatus = PannerConnectionStatus::Stale;
                    it->isActive = false;
                }
            } else {
                // For OSC panners, use timeout-based removal
                shouldRemove = true;
                it->connectionStatus = PannerConnectionStatus::Disconnected;
                DBG("[PannerTrackingManager] Removing OSC panner (timed out): " + it->name);
            }
        } else {
            // Recently updated - mark as active
            it->connectionStatus = PannerConnectionStatus::Active;
        }
        
        if (shouldRemove) {
            publishPannerRemoved(*it);
            it = activePanners.erase(it);
        } else {
            ++it;
        }
    }
}

bool PannerTrackingManager::isProcessRunning(uint32_t processId) const {
#ifdef JUCE_MAC
    return (kill(static_cast<pid_t>(processId), 0) == 0);
#elif JUCE_WINDOWS
    HANDLE processHandle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, processId);
    if (processHandle != NULL) {
        CloseHandle(processHandle);
        return true;
    }
    return false;
#else
    return true; // Assume running on other platforms
#endif
}

// =============================================================================
// EVENT PUBLISHING
// =============================================================================

void PannerTrackingManager::publishPannerAdded(const PannerInfo& panner) {
    if (eventSystem) {
        eventSystem->publish("PannerAdded", static_cast<int>(panner.processId));
    }
}

void PannerTrackingManager::publishPannerUpdated(const PannerInfo& panner) {
    if (eventSystem) {
        eventSystem->publish("PannerUpdated", static_cast<int>(panner.processId));
    }
}

void PannerTrackingManager::publishPannerRemoved(const PannerInfo& panner) {
    if (eventSystem) {
        eventSystem->publish("PannerRemoved", static_cast<int>(panner.processId));
    }
}

void PannerTrackingManager::publishTrackingMethodChanged(bool memoryShare, bool osc) {
    if (eventSystem) {
        juce::var method;
        if (memoryShare && osc) {
            method = "MemoryShare+OSC";
        } else if (memoryShare) {
            method = "MemoryShare";
        } else if (osc) {
            method = "OSC";
        } else {
            method = "None";
        }
        
        eventSystem->publish("TrackingMethodChanged", method);
    }
}

} // namespace Mach1 
