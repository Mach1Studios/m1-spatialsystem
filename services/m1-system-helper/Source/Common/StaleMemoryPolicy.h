#pragma once

#include <cstdint>

namespace Mach1 {
namespace StaleMemoryPolicy {

constexpr std::int64_t kDeadProcessGraceMs = 10 * 60 * 1000;
constexpr std::int64_t kUnknownFileMaxAgeMs = 2 * 60 * 60 * 1000;

/**
 * Decides whether a shared-memory backing file can be removed safely.
 *
 * Parsed panner files are process-owned. A live DAW process always wins over
 * file age: deleting its segment would orphan existing mappings and break
 * streaming. Files from dead processes get a ten-minute reload grace period.
 * Unknown Mach1 files have no owner identity, so only very old ones are swept.
 * The helper-owned MixBus is never touched by the panner-file collector.
 */
constexpr bool shouldDelete(bool isMixBus,
                            bool isParsedPannerFile,
                            bool ownerProcessAlive,
                            std::int64_t fileAgeMs)
{
    if (isMixBus)
        return false;

    if (isParsedPannerFile)
        return !ownerProcessAlive && fileAgeMs > kDeadProcessGraceMs;

    return fileAgeMs > kUnknownFileMaxAgeMs;
}

} // namespace StaleMemoryPolicy
} // namespace Mach1
