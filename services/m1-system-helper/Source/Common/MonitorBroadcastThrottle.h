#pragma once

#include <cmath>
#include <cstdint>

namespace Mach1 {

/**
 * Rate-limiter/deduplicator for the monitor-settings fan-out to panner plugins.
 *
 * A monitor with a streaming head-tracking device (BLE/OSC/serial) sends
 * "/setMasterYPR" on every yaw/pitch/roll parameter change - up to hundreds of
 * messages per second. Each of those used to be re-broadcast immediately to
 * every registered plugin, so N panner instances received rate*N messages per
 * second, all dispatched onto the host's message thread. In large sessions
 * this starved the message thread enough that newly opened plugin editors
 * never finished initializing their UI (permanent grey editor windows while
 * the head-tracker streamed).
 *
 * shouldBroadcast() decides whether an update is forwarded now; suppressed
 * updates are remembered so takePendingFlush() can deliver the latest values
 * once the interval has passed (call it from a timer).
 *
 * Not thread-safe; callers must provide their own locking.
 */
struct MonitorBroadcastThrottle {
    struct Values {
        int mode = 0;
        float yaw = 0.0f;
        float pitch = 0.0f;
        float roll = 0.0f;
    };

    std::int64_t minIntervalMs = 50; // max ~20 broadcasts per second
    float valueEpsilon = 0.01f;      // below this delta (degrees) a value is "unchanged"

    /**
     * @param force  bypass the rate limit and dedupe (use for discrete state
     *               changes such as monitor mode or active-monitor switches)
     * @return true when the caller should broadcast `values` now
     */
    bool shouldBroadcast(const Values& values, std::int64_t nowMs, bool force = false)
    {
        if (force) {
            accept(values, nowMs);
            return true;
        }

        const bool changed = !hasLastSent
            || values.mode != lastSent.mode
            || std::fabs(values.yaw - lastSent.yaw) > valueEpsilon
            || std::fabs(values.pitch - lastSent.pitch) > valueEpsilon
            || std::fabs(values.roll - lastSent.roll) > valueEpsilon;

        if (!changed)
            return false;

        if (hasLastSent && (nowMs - lastSentMs) < minIntervalMs) {
            pending = values;
            hasPending = true;
            return false;
        }

        accept(values, nowMs);
        return true;
    }

    /**
     * Delivers the newest suppressed update once the interval has passed.
     * @return true when `out` was filled and should be broadcast
     */
    bool takePendingFlush(std::int64_t nowMs, Values& out)
    {
        if (!hasPending || (nowMs - lastSentMs) < minIntervalMs)
            return false;
        out = pending;
        accept(pending, nowMs);
        return true;
    }

private:
    void accept(const Values& values, std::int64_t nowMs)
    {
        lastSent = values;
        lastSentMs = nowMs;
        hasLastSent = true;
        hasPending = false;
    }

    Values lastSent;
    Values pending;
    std::int64_t lastSentMs = 0;
    bool hasLastSent = false;
    bool hasPending = false;
};

} // namespace Mach1
