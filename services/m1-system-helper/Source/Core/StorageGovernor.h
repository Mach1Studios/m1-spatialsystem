/*
    StorageGovernor.h
    -----------------
    Policy layer for the on-disk capture store (P1.5 of the external renderer
    plan). Answers "how much has this session captured?" and "how much is
    sitting on disk across all sessions?", classifies each session directory
    (active / recent / stale / orphaned / pinned), and turns cleanup into a
    two-step dry-run + execute flow so nothing is deleted blind.

    Invariants enforced by planCleanup():
      - the currently-capturing session is never selected
      - pinned sessions (a "pinned" marker file in the session dir) are never
        selected
      - dry-run byte totals are computed from the same scan that executeCleanup
        later deletes, so the preview matches what is freed

    Layout it manages: <captureRoot>/<sessionId>/{<stream dirs>, manifest.json,
    exports/}. The manifest is written by CaptureEngine while capturing; a
    session directory without one is treated as orphaned (attributable to no
    known writer) and therefore reclaimable.
*/

#pragma once

#include <JuceHeader.h>
#include <vector>

namespace Mach1 {

class StorageGovernor
{
public:
    enum class SessionState
    {
        Active,     // the session currently being captured into
        Recent,     // has a manifest, written within the keep window
        Stale,      // has a manifest, older than the keep window
        Orphaned    // no manifest (unknown writer, e.g. crashed helper)
    };

    struct Policy
    {
        int keepDays = 30;
        juce::int64 maxTotalBytes = 25LL * 1024 * 1024 * 1024; // 25 GB soft cap
    };

    struct SessionUsage
    {
        juce::File directory;
        juce::String sessionId;          // directory name
        juce::String displayName;        // project name from manifest, if available
        juce::int64 sizeBytes = 0;       // recursive size (chunks + exports)
        juce::int64 lastWrittenMs = 0;   // manifest lastWrittenMs, else dir mtime
        int streamCount = 0;
        SessionState state = SessionState::Orphaned;
        bool pinned = false;
        bool hasManifest = false;
    };

    struct Report
    {
        std::vector<SessionUsage> sessions; // newest first
        juce::int64 totalBytes = 0;
        juce::int64 reclaimableBytes = 0;   // stale + orphaned, minus pinned/active
        bool overCap = false;               // totalBytes exceeds policy cap
    };

    struct CleanupPlan
    {
        std::vector<SessionUsage> selected;
        juce::int64 bytesToFree = 0;
    };

    struct CleanupResult
    {
        int sessionsRemoved = 0;
        juce::int64 bytesFreed = 0;
        std::vector<juce::String> failures; // session ids that could not be removed
    };

    //==========================================================================
    /** Durable capture root (Application Support, not Caches/tmp - the OS is
        free to purge caches, which silently destroyed capture data). */
    static juce::File getDefaultCaptureRoot();

    /** One-time move of sessions from the legacy temp/caches root into the
        durable root. Safe to call every launch; does nothing once migrated. */
    static void migrateLegacyCaptures();

    //==========================================================================
    /** Scans every session directory under the root. activeSessionId marks the
        session currently being captured into (never reclaimable). */
    static Report scan(const juce::File& captureRoot,
                       const juce::String& activeSessionId,
                       const Policy& policy);

    /** Dry run: selects every stale + orphaned session that is neither active
        nor pinned. The returned plan is what executeCleanup() will delete. */
    static CleanupPlan planCleanup(const Report& report);

    /** Deletes the planned sessions. Tries the system trash first (recoverable)
        and falls back to recursive deletion; useTrash=false skips the trash
        (used by tests and headless runs). */
    static CleanupResult executeCleanup(const CleanupPlan& plan, bool useTrash = true);

    //==========================================================================
    static bool isPinned(const juce::File& sessionDir);
    static void setPinned(const juce::File& sessionDir, bool shouldPin);

    /** Recursive size of a directory in bytes. */
    static juce::int64 directorySizeBytes(const juce::File& directory);
};

} // namespace Mach1
