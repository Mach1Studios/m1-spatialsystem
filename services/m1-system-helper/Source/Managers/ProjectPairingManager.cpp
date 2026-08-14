#include "ProjectPairingManager.h"

#include <algorithm>

namespace Mach1 {

namespace {
constexpr int kRegistrySchemaVersion = 1;

juce::String normaliseBindingId(const juce::String& value)
{
    return value.trim().toLowerCase();
}

juce::String normaliseDisplayName(const juce::String& value)
{
    return value.trim().substring(0, 96);
}
} // namespace

ProjectPairingManager::ProjectPairingManager()
    : ProjectPairingManager(getDefaultRegistryFile())
{
}

ProjectPairingManager::ProjectPairingManager(juce::File file)
    : registryFile(std::move(file))
{
    load();
}

ProjectPairingManager::~ProjectPairingManager()
{
    flushIfNeeded();
}

juce::File ProjectPairingManager::getDefaultRegistryFile()
{
    auto base = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
#if JUCE_MAC
    base = base.getChildFile("Application Support");
#endif
    return base.getChildFile("Mach1")
        .getChildFile("m1-system-helper")
        .getChildFile("project-bindings.json");
}

juce::String ProjectPairingManager::makeSessionId(const juce::String& displayName,
                                                  const juce::String& bindingId)
{
    auto slug = normaliseDisplayName(displayName)
        .replaceCharacters("\\/:*?\"<>|", "_________")
        .retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 _-.")
        .replaceCharacter(' ', '_');

    while (slug.contains("__"))
        slug = slug.replace("__", "_");

    slug = slug.trimCharactersAtStart("._-").trimCharactersAtEnd("._-").substring(0, 64);
    if (slug.isEmpty())
        slug = "Session";

    auto suffix = normaliseBindingId(bindingId).removeCharacters("-{}").substring(0, 8);
    if (suffix.isEmpty())
        suffix = juce::Uuid().toString().removeCharacters("-").substring(0, 8);

    return slug + "_" + suffix;
}

ProjectBinding ProjectPairingManager::bindingForIdLocked(const juce::String& bindingId) const
{
    const auto it = bindingsById.find(normaliseBindingId(bindingId));
    return it != bindingsById.end() ? it->second : ProjectBinding{};
}

ProjectBinding& ProjectPairingManager::ensureBindingLocked(const juce::String& bindingId,
                                                           const juce::String& displayName)
{
    const auto id = normaliseBindingId(bindingId);
    auto [it, inserted] = bindingsById.try_emplace(id);
    auto& binding = it->second;

    if (inserted)
        binding.bindingId = id;

    const auto name = normaliseDisplayName(displayName);
    if (binding.displayName.isEmpty() && name.isNotEmpty())
    {
        binding.displayName = name;
        binding.sessionId = makeSessionId(name, id);
        dirty = true;
    }

    binding.lastUsedMs = juce::Time::currentTimeMillis();
    if (binding.isNamed())
        dirty = true;
    return binding;
}

ProjectBinding ProjectPairingManager::registerHostClaim(uint32_t hostProcessId,
                                                        const juce::String& claimedBindingId,
                                                        const juce::String& claimedDisplayName)
{
    if (hostProcessId == 0)
        return {};

    const juce::ScopedLock lock(mutex);
    const auto claimId = normaliseBindingId(claimedBindingId);

    auto leaseIt = bindingIdByHostProcess.find(hostProcessId);
    if (leaseIt == bindingIdByHostProcess.end())
    {
        if (claimId.isEmpty())
            return {};

        auto& claimed = ensureBindingLocked(claimId, claimedDisplayName);
        bindingIdByHostProcess[hostProcessId] = claimId;
        return claimed;
    }

    auto current = bindingForIdLocked(leaseIt->second);
    if (claimId.isEmpty() || claimId == current.bindingId)
    {
        if (claimId.isNotEmpty())
            current = ensureBindingLocked(claimId, claimedDisplayName);
        return current;
    }

    auto& claimed = ensureBindingLocked(claimId, claimedDisplayName);

    // A saved, named project is stronger evidence than a fresh generated ID.
    if (!current.isNamed() && claimed.isNamed())
    {
        leaseIt->second = claimed.bindingId;
        ambiguousHostProcesses.erase(hostProcessId);
        return claimed;
    }

    // Two named saved projects in one DAW process can be REAPER project tabs.
    // Never silently merge them; require an explicit helper-UI selection.
    if (current.isNamed() && claimed.isNamed())
    {
        ambiguousHostProcesses.insert(hostProcessId);
        return {};
    }

    // Multiple unnamed IDs are expected on the first load of legacy sessions.
    // The first instance becomes the temporary anchor until the user names it.
    return current;
}

ProjectBinding ProjectPairingManager::nameHostProject(uint32_t hostProcessId,
                                                      const juce::String& displayName)
{
    const auto name = normaliseDisplayName(displayName);
    if (hostProcessId == 0 || name.isEmpty())
        return {};

    const juce::ScopedLock lock(mutex);
    auto& id = bindingIdByHostProcess[hostProcessId];
    if (id.isEmpty())
        id = juce::Uuid().toString().toLowerCase();

    auto& binding = ensureBindingLocked(id, {});
    if (binding.displayName != name || binding.sessionId.isEmpty())
    {
        binding.displayName = name;
        if (binding.sessionId.isEmpty())
            binding.sessionId = makeSessionId(name, binding.bindingId);
        binding.lastUsedMs = juce::Time::currentTimeMillis();
        dirty = true;
    }

    ambiguousHostProcesses.erase(hostProcessId);
    return binding;
}

ProjectBinding ProjectPairingManager::selectProject(uint32_t hostProcessId,
                                                    const juce::String& bindingId)
{
    if (hostProcessId == 0)
        return {};

    const juce::ScopedLock lock(mutex);
    const auto selected = bindingForIdLocked(bindingId);
    if (!selected.isNamed())
        return {};

    bindingIdByHostProcess[hostProcessId] = selected.bindingId;
    ambiguousHostProcesses.erase(hostProcessId);
    bindingsById[selected.bindingId].lastUsedMs = juce::Time::currentTimeMillis();
    dirty = true;
    return bindingsById[selected.bindingId];
}

ProjectBinding ProjectPairingManager::getHostBinding(uint32_t hostProcessId) const
{
    const juce::ScopedLock lock(mutex);
    if (ambiguousHostProcesses.count(hostProcessId) != 0)
        return {};

    const auto it = bindingIdByHostProcess.find(hostProcessId);
    return it != bindingIdByHostProcess.end() ? bindingForIdLocked(it->second)
                                               : ProjectBinding{};
}

std::vector<ProjectBinding> ProjectPairingManager::getKnownBindings() const
{
    const juce::ScopedLock lock(mutex);
    std::vector<ProjectBinding> result;
    for (const auto& [id, binding] : bindingsById)
    {
        juce::ignoreUnused(id);
        if (binding.isNamed())
            result.push_back(binding);
    }

    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        return a.lastUsedMs > b.lastUsedMs;
    });
    return result;
}

bool ProjectPairingManager::isHostAmbiguous(uint32_t hostProcessId) const
{
    const juce::ScopedLock lock(mutex);
    return ambiguousHostProcesses.count(hostProcessId) != 0;
}

void ProjectPairingManager::updateHostPannerCount(uint32_t hostProcessId, int pannerCount)
{
    if (hostProcessId == 0)
        return;

    const juce::ScopedLock lock(mutex);
    const auto host = bindingIdByHostProcess.find(hostProcessId);
    if (host == bindingIdByHostProcess.end())
        return;

    const auto binding = bindingsById.find(host->second);
    if (binding == bindingsById.end())
        return;

    const int count = juce::jmax(0, pannerCount);
    if (binding->second.pannerCount != count)
    {
        binding->second.pannerCount = count;
        dirty = binding->second.isNamed() || dirty;
    }
}

void ProjectPairingManager::load()
{
    if (!registryFile.existsAsFile())
        return;

    const auto json = juce::JSON::parse(registryFile);
    const auto* root = json.getDynamicObject();
    if (root == nullptr)
        return;

    const auto* entries = root->getProperty("bindings").getArray();
    if (entries == nullptr)
        return;

    const juce::ScopedLock lock(mutex);
    for (const auto& value : *entries)
    {
        const auto* object = value.getDynamicObject();
        if (object == nullptr)
            continue;

        ProjectBinding binding;
        binding.bindingId = normaliseBindingId(object->getProperty("bindingId").toString());
        binding.displayName = normaliseDisplayName(object->getProperty("displayName").toString());
        binding.sessionId = object->getProperty("sessionId").toString();
        binding.lastUsedMs = static_cast<juce::int64>(
            static_cast<juce::int64>(object->getProperty("lastUsedMs")));
        binding.pannerCount = object->hasProperty("pannerCount")
            ? juce::jmax(0, static_cast<int>(object->getProperty("pannerCount")))
            : -1;

        if (binding.bindingId.isEmpty() || binding.displayName.isEmpty())
            continue;
        if (binding.sessionId.isEmpty())
            binding.sessionId = makeSessionId(binding.displayName, binding.bindingId);

        bindingsById[binding.bindingId] = std::move(binding);
    }
}

void ProjectPairingManager::flushIfNeeded()
{
    juce::var json;
    {
        const juce::ScopedLock lock(mutex);
        if (!dirty)
            return;

        auto* root = new juce::DynamicObject();
        root->setProperty("schemaVersion", kRegistrySchemaVersion);

        juce::Array<juce::var> entries;
        for (const auto& [id, binding] : bindingsById)
        {
            juce::ignoreUnused(id);
            if (!binding.isNamed())
                continue;

            auto* object = new juce::DynamicObject();
            object->setProperty("bindingId", binding.bindingId);
            object->setProperty("displayName", binding.displayName);
            object->setProperty("sessionId", binding.sessionId);
            object->setProperty("lastUsedMs", binding.lastUsedMs);
            object->setProperty("pannerCount", binding.pannerCount);
            entries.add(juce::var(object));
        }
        root->setProperty("bindings", entries);
        json = juce::var(root);
        dirty = false;
    }

    registryFile.getParentDirectory().createDirectory();
    const auto temporary = registryFile.getSiblingFile(registryFile.getFileName() + ".tmp");
    if (!temporary.replaceWithText(juce::JSON::toString(json))
        || (!temporary.moveFileTo(registryFile) && !temporary.replaceFileIn(registryFile)))
    {
        const juce::ScopedLock lock(mutex);
        dirty = true;
        DBG("[ProjectPairingManager] Failed to write " + registryFile.getFullPathName());
    }
}

} // namespace Mach1
