#pragma once

#include <JuceHeader.h>
#include <cstdint>
#include <map>
#include <set>
#include <vector>

namespace Mach1 {

struct ProjectBinding
{
    juce::String bindingId;
    juce::String displayName;
    juce::String sessionId;
    juce::int64 lastUsedMs = 0;

    bool isValid() const { return bindingId.isNotEmpty(); }
    bool isNamed() const { return isValid() && displayName.isNotEmpty() && sessionId.isNotEmpty(); }
};

/**
 * Resolves plugin-persisted project IDs into one helper capture session.
 *
 * Host process IDs are deliberately only live leases: they are useful for
 * joining new/legacy instances while a DAW is open, but are never persisted.
 * The stable identity is bindingId, which is saved in plugin state and in this
 * registry.
 */
class ProjectPairingManager
{
public:
    ProjectPairingManager();
    explicit ProjectPairingManager(juce::File registryFile);
    ~ProjectPairingManager();

    /** Registers an instance's saved claim and returns the canonical live
        binding for its host process. An empty result means no claim exists or
        two different named projects are open in the same host process. */
    ProjectBinding registerHostClaim(uint32_t hostProcessId,
                                     const juce::String& claimedBindingId,
                                     const juce::String& claimedDisplayName);

    /** Names a new/unnamed live project, preserving its existing binding ID. */
    ProjectBinding nameHostProject(uint32_t hostProcessId,
                                   const juce::String& displayName);

    /** Explicit user choice of a previously named project. */
    ProjectBinding selectProject(uint32_t hostProcessId,
                                 const juce::String& bindingId);

    ProjectBinding getHostBinding(uint32_t hostProcessId) const;
    std::vector<ProjectBinding> getKnownBindings() const;
    bool isHostAmbiguous(uint32_t hostProcessId) const;

    /** Writes a dirty registry. Call from a non-audio/message-service thread. */
    void flushIfNeeded();

    static juce::File getDefaultRegistryFile();
    static juce::String makeSessionId(const juce::String& displayName,
                                      const juce::String& bindingId);

private:
    void load();
    ProjectBinding bindingForIdLocked(const juce::String& bindingId) const;
    ProjectBinding& ensureBindingLocked(const juce::String& bindingId,
                                        const juce::String& displayName);

    juce::File registryFile;
    mutable juce::CriticalSection mutex;
    std::map<juce::String, ProjectBinding> bindingsById;
    std::map<uint32_t, juce::String> bindingIdByHostProcess;
    std::set<uint32_t> ambiguousHostProcesses;
    bool dirty = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProjectPairingManager)
};

} // namespace Mach1
