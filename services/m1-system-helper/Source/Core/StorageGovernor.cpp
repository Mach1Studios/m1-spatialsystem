#include "StorageGovernor.h"

namespace Mach1 {

namespace {
    constexpr const char* kManifestFileName = "manifest.json";
    constexpr const char* kPinnedFileName = "pinned";
    constexpr const char* kLegacyRootName = "M1SpatialSystem_Captures";
}

//==============================================================================
juce::File StorageGovernor::getDefaultCaptureRoot()
{
    auto base = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
#if JUCE_MAC
    // userApplicationDataDirectory is ~/Library on macOS
    base = base.getChildFile("Application Support");
#endif
    return base.getChildFile("Mach1").getChildFile("m1-system-helper").getChildFile("Captures");
}

void StorageGovernor::migrateLegacyCaptures()
{
    const juce::File legacyRoot = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile(kLegacyRootName);
    if (!legacyRoot.isDirectory())
        return;

    const juce::File newRoot = getDefaultCaptureRoot();
    newRoot.createDirectory();

    for (const auto& entry : juce::RangedDirectoryIterator(legacyRoot, false, "*", juce::File::findDirectories))
    {
        const juce::File target = newRoot.getChildFile(entry.getFile().getFileName());
        if (target.exists())
            continue; // never overwrite - leave the legacy copy for manual review

        if (entry.getFile().moveFileTo(target))
            DBG("[StorageGovernor] Migrated legacy session: " + entry.getFile().getFileName());
    }

    // Remove the legacy root only once it is empty
    if (legacyRoot.getNumberOfChildFiles(juce::File::findFilesAndDirectories) == 0)
        legacyRoot.deleteFile();
}

//==============================================================================
juce::int64 StorageGovernor::directorySizeBytes(const juce::File& directory)
{
    juce::int64 total = 0;
    for (const auto& entry : juce::RangedDirectoryIterator(directory, true, "*", juce::File::findFiles))
        total += entry.getFileSize();
    return total;
}

bool StorageGovernor::isPinned(const juce::File& sessionDir)
{
    return sessionDir.getChildFile(kPinnedFileName).existsAsFile();
}

void StorageGovernor::setPinned(const juce::File& sessionDir, bool shouldPin)
{
    const juce::File marker = sessionDir.getChildFile(kPinnedFileName);
    if (shouldPin)
        marker.replaceWithText("pinned by user\n");
    else
        marker.deleteFile();
}

//==============================================================================
StorageGovernor::Report StorageGovernor::scan(const juce::File& captureRoot,
                                              const juce::String& activeSessionId,
                                              const Policy& policy)
{
    Report report;
    if (!captureRoot.isDirectory())
        return report;

    const juce::int64 nowMs = juce::Time::currentTimeMillis();
    const juce::int64 keepWindowMs = static_cast<juce::int64>(policy.keepDays) * 24 * 60 * 60 * 1000;

    for (const auto& entry : juce::RangedDirectoryIterator(captureRoot, false, "*", juce::File::findDirectories))
    {
        const juce::File dir = entry.getFile();

        SessionUsage usage;
        usage.directory = dir;
        usage.sessionId = dir.getFileName();
        usage.sizeBytes = directorySizeBytes(dir);
        usage.pinned = isPinned(dir);

        const juce::File manifestFile = dir.getChildFile(kManifestFileName);
        usage.hasManifest = manifestFile.existsAsFile();

        if (usage.hasManifest)
        {
            const auto manifest = juce::JSON::parse(manifestFile.loadFileAsString());
            usage.lastWrittenMs = static_cast<juce::int64>(manifest.getProperty("lastWrittenMs", 0.0));
            if (const auto* streams = manifest.getProperty("streams", juce::var()).getArray())
                usage.streamCount = streams->size();
        }
        if (usage.lastWrittenMs == 0)
            usage.lastWrittenMs = dir.getLastModificationTime().toMilliseconds();

        if (usage.sessionId == activeSessionId && activeSessionId.isNotEmpty())
            usage.state = SessionState::Active;
        else if (!usage.hasManifest)
            usage.state = SessionState::Orphaned;
        else if (nowMs - usage.lastWrittenMs > keepWindowMs)
            usage.state = SessionState::Stale;
        else
            usage.state = SessionState::Recent;

        report.totalBytes += usage.sizeBytes;
        if ((usage.state == SessionState::Stale || usage.state == SessionState::Orphaned) && !usage.pinned)
            report.reclaimableBytes += usage.sizeBytes;

        report.sessions.push_back(std::move(usage));
    }

    std::sort(report.sessions.begin(), report.sessions.end(),
              [](const SessionUsage& a, const SessionUsage& b)
              { return a.lastWrittenMs > b.lastWrittenMs; });

    report.overCap = report.totalBytes > policy.maxTotalBytes;
    return report;
}

//==============================================================================
StorageGovernor::CleanupPlan StorageGovernor::planCleanup(const Report& report)
{
    CleanupPlan plan;
    for (const auto& session : report.sessions)
    {
        if (session.pinned || session.state == SessionState::Active)
            continue;
        if (session.state != SessionState::Stale && session.state != SessionState::Orphaned)
            continue;

        plan.selected.push_back(session);
        plan.bytesToFree += session.sizeBytes;
    }
    return plan;
}

StorageGovernor::CleanupResult StorageGovernor::executeCleanup(const CleanupPlan& plan, bool useTrash)
{
    CleanupResult result;
    for (const auto& session : plan.selected)
    {
        // Re-verify the invariants against the live filesystem: the plan may
        // be stale by the time the user confirms (e.g. they pinned a session
        // between preview and confirm).
        if (isPinned(session.directory))
        {
            result.failures.push_back(session.sessionId + " (pinned since preview)");
            continue;
        }
        if (!session.directory.exists())
            continue; // already gone - nothing to free

        const juce::int64 size = directorySizeBytes(session.directory);

        bool removed = false;
        if (useTrash)
            removed = session.directory.moveToTrash();
        if (!removed)
            removed = session.directory.deleteRecursively();

        if (removed)
        {
            ++result.sessionsRemoved;
            result.bytesFreed += size;
        }
        else
        {
            result.failures.push_back(session.sessionId);
        }
    }
    return result;
}

} // namespace Mach1
