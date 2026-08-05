/*
    StorageGovernorTests.cpp
    ------------------------
    Policy tests for the capture-store governor against synthetic session
    directories. The invariants matter more than the numbers:

      - the active session and pinned sessions are NEVER selected for cleanup
      - the dry-run byte total matches what execution actually frees
      - sessions without a manifest are orphaned (reclaimable), sessions with
        a fresh manifest are kept, sessions past the keep window are stale
      - execution only removes what the plan listed
*/

#include <JuceHeader.h>
#include "Core/StorageGovernor.h"

#include <iostream>

namespace {

int storageFailures = 0;

#define SCHECK(cond, message)                                                   \
    do {                                                                        \
        if (!(cond)) {                                                          \
            ++storageFailures;                                                  \
            std::cout << "FAILED: " << message << " (" << #cond << ") at "      \
                      << __FILE__ << ":" << __LINE__ << std::endl;              \
        }                                                                       \
    } while (false)

using Mach1::StorageGovernor;

// Creates <root>/<name> with a chunk file of exactly `bytes` bytes and,
// unless lastWrittenMs is 0, a manifest carrying that timestamp.
juce::File makeSession(const juce::File& root, const juce::String& name,
                       int bytes, juce::int64 lastWrittenMs)
{
    const juce::File dir = root.getChildFile(name);
    const juce::File streamDir = dir.getChildFile("stream_1");
    streamDir.createDirectory();

    juce::MemoryBlock payload(static_cast<size_t>(bytes));
    payload.fillWith(0x42);
    streamDir.getChildFile("chunks.bin").replaceWithData(payload.getData(), payload.getSize());

    if (lastWrittenMs != 0)
    {
        auto* manifest = new juce::DynamicObject();
        manifest->setProperty("sessionId", name);
        manifest->setProperty("lastWrittenMs", lastWrittenMs);
        juce::Array<juce::var> streams;
        auto* stream = new juce::DynamicObject();
        stream->setProperty("dir", "stream_1");
        stream->setProperty("bytes", bytes);
        streams.add(juce::var(stream));
        manifest->setProperty("streams", streams);
        dir.getChildFile("manifest.json").replaceWithText(juce::JSON::toString(juce::var(manifest)));
    }

    return dir;
}

void testStorageGovernorPolicies()
{
    const juce::File root = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("M1StorageGovernorTest_" + juce::String(juce::Time::currentTimeMillis()));
    root.createDirectory();

    const juce::int64 nowMs = juce::Time::currentTimeMillis();
    const juce::int64 fortyDaysAgo = nowMs - 40LL * 24 * 60 * 60 * 1000;

    // Manifest sizes are per-session directory content:
    // manifest.json adds a few bytes on top of the chunk payload.
    makeSession(root, "Session_Active", 1000, fortyDaysAgo);      // active despite stale timestamp
    makeSession(root, "Session_Recent", 2000, nowMs - 1000);      // fresh
    makeSession(root, "Session_Stale", 3000, fortyDaysAgo);       // past keep window
    const juce::File pinnedDir =
        makeSession(root, "Session_PinnedStale", 4000, fortyDaysAgo);
    StorageGovernor::setPinned(pinnedDir, true);
    makeSession(root, "Session_Orphaned", 5000, 0);               // no manifest

    StorageGovernor::Policy policy;
    policy.keepDays = 30;

    const auto report = StorageGovernor::scan(root, "Session_Active", policy);
    SCHECK(report.sessions.size() == 5, "all session directories scanned");

    const auto find = [&report](const juce::String& id) -> const StorageGovernor::SessionUsage*
    {
        for (const auto& session : report.sessions)
            if (session.sessionId == id)
                return &session;
        return nullptr;
    };

    // Classification
    SCHECK(find("Session_Active") && find("Session_Active")->state == StorageGovernor::SessionState::Active,
           "capturing session classified Active even with an old manifest");
    SCHECK(find("Session_Recent") && find("Session_Recent")->state == StorageGovernor::SessionState::Recent,
           "fresh manifest classified Recent");
    SCHECK(find("Session_Stale") && find("Session_Stale")->state == StorageGovernor::SessionState::Stale,
           "old manifest classified Stale");
    SCHECK(find("Session_Orphaned") && find("Session_Orphaned")->state == StorageGovernor::SessionState::Orphaned,
           "missing manifest classified Orphaned");
    SCHECK(find("Session_PinnedStale") && find("Session_PinnedStale")->pinned,
           "pin marker detected");

    // Usage accounting
    juce::int64 expectedTotal = 0;
    for (const auto& session : report.sessions)
        expectedTotal += StorageGovernor::directorySizeBytes(session.directory);
    SCHECK(report.totalBytes == expectedTotal, "total bytes equals sum of session sizes");
    SCHECK(find("Session_Orphaned")->sizeBytes >= 5000, "orphaned session size measured without manifest");

    // Reclaimable = stale + orphaned, minus pinned
    const juce::int64 expectedReclaimable =
        find("Session_Stale")->sizeBytes + find("Session_Orphaned")->sizeBytes;
    SCHECK(report.reclaimableBytes == expectedReclaimable,
           "reclaimable excludes active, recent and pinned sessions");

    // Plan invariants
    const auto plan = StorageGovernor::planCleanup(report);
    SCHECK(plan.selected.size() == 2, "plan selects exactly stale + orphaned");
    for (const auto& selected : plan.selected)
    {
        SCHECK(selected.sessionId != "Session_Active", "active session never selected");
        SCHECK(selected.sessionId != "Session_PinnedStale", "pinned session never selected");
        SCHECK(selected.sessionId != "Session_Recent", "recent session never selected");
    }
    SCHECK(plan.bytesToFree == expectedReclaimable, "dry-run total matches reclaimable bytes");

    // Execution frees exactly what the dry run promised, and nothing else
    const auto result = StorageGovernor::executeCleanup(plan, /*useTrash*/ false);
    SCHECK(result.failures.empty(), "cleanup completed without failures");
    SCHECK(result.sessionsRemoved == 2, "both planned sessions removed");
    SCHECK(result.bytesFreed == plan.bytesToFree, "freed bytes match the dry-run preview");
    SCHECK(root.getChildFile("Session_Active").isDirectory(), "active session untouched");
    SCHECK(root.getChildFile("Session_Recent").isDirectory(), "recent session untouched");
    SCHECK(root.getChildFile("Session_PinnedStale").isDirectory(), "pinned session untouched");
    SCHECK(!root.getChildFile("Session_Stale").exists(), "stale session removed");
    SCHECK(!root.getChildFile("Session_Orphaned").exists(), "orphaned session removed");

    // A session pinned AFTER the plan was made must survive execution
    const auto report2 = StorageGovernor::scan(root, "", policy);
    auto plan2 = StorageGovernor::planCleanup(report2);
    bool planIncludesActive = false;
    for (const auto& selected : plan2.selected)
        planIncludesActive |= (selected.sessionId == "Session_Active");
    SCHECK(planIncludesActive, "previously-active session becomes reclaimable once capture ends");

    StorageGovernor::setPinned(root.getChildFile("Session_Active"), true);
    const auto result2 = StorageGovernor::executeCleanup(plan2, /*useTrash*/ false);
    SCHECK(root.getChildFile("Session_Active").isDirectory(),
           "session pinned between preview and confirm survives execution");
    SCHECK(!result2.failures.empty(), "late pin reported as a skipped entry");

    root.deleteRecursively();
}

} // namespace

// Called from HelperServiceTests.cpp's main(); returns failed check count.
int runStorageGovernorTests()
{
    testStorageGovernorPolicies();

    if (storageFailures == 0)
        std::cout << "All StorageGovernor tests passed" << std::endl;

    return storageFailures;
}
