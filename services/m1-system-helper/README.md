# m1-system-helper
A background executable that checks if the server is still active and operating and otherwise attempts to relaunch it properly. This should be launched by any and all `m1_orientation_client` or client to the server.

It is also the external multichannel renderer for M1-Panner/M1-Monitor instances hosted in mono/stereo-only DAWs (Ableton Live, or narrow tracks in any DAW). See [External renderer](#external-renderer) below.

## External renderer

When an M1-Panner instance is hosted on a mono- or stereo-only bus (1-in/2-out or
2-in/2-out) it cannot render multichannel audio itself. Instead it enters *streaming
mode*: every processed block is written into a per-instance shared-memory segment
(`M1SpatialSystem_M1Panner_PID<pid>_<address>_<timestamp>.mem`) along with the full
panner parameter state, and the helper takes over the spatial math.

Inside the helper:

- **PannerTrackingManager / M1MemoryShareTracker** discover panner segments, dedupe
  them against OSC registrations, and expose one strip per instance in the status UI.
- **CaptureEngine** drains audio blocks to per-session chunk files on disk (capture
  root managed by **StorageGovernor**, which also implements the stale/orphan cleanup
  policies surfaced in the Storage overlay).
- **ExportEngine** assembles captured chunks into a coverage-gated multichannel WAV
  plus a JSON report (offline render).
- **MixEngine** is the live render clock: it re-encodes every streaming panner with
  its current parameters, sums the result into the configured spatial format, and
  publishes fixed-size blocks into the shared `M1SpatialSystem_MixBus.mem` segment.
  M1-Monitor instances on stereo-only buses read that segment (advertised via the
  `/m1-external-mixer-state` OSC heartbeat) and decode it with head-tracked
  orientation even though their host bus never carried the multichannel mix.
- **Two-way control**: the helper writes parameter edits into each panner's control
  ring (shared memory); the panner applies them through its host parameters and
  echoes the applied revision back, which clears the helper's pending-edit overlay.

### User-facing toggle

Audio streaming can be turned off at runtime from the helper's tray menu
("Enable Audio Streaming (External Renderer)") or verified in the status window
title. When disabled:

- the MixEngine stops and the MixBus segment is removed (monitors fall back to
  their host bus),
- every registered panner is told via `/m1-external-renderer-enabled` to stay in
  native processing (the panner UI shows "STREAMING DISABLED IN M1-SYSTEM-HELPER"),
- the choice is persisted per-user in `~/Library/Application Support/Mach1/helper-settings.json`
  (`%APPDATA%\Mach1\helper-settings.json` on Windows) as `externalRendererEnabled`.

The compile-time gate for the plugin side is the `ENABLE_EXTERNAL_RENDERER` CMake
option in `m1-panner` (defaults ON; passed explicitly by the Makefile and CI).
`make test-external-renderer` builds the panner policy tests with that option both
ON and OFF, tests the monitor's host-vs-MixBus selection, and runs the helper's
shared-memory/live-mix integration suite. Release CI must pass this gate before
building any platform artifacts.

### Host-mode behavior

- **Mono/stereo panner + narrow monitor bus:** the panner streams source audio,
  the helper encodes and publishes the MixBus, and the monitor decodes it.
- **4/8/14-channel host buses:** panner and monitor process the host's native
  multichannel audio. The panner keeps a parameter-only heartbeat so it remains
  visible and controllable in the helper; its status reads
  `Multichannel (not streaming)` to make clear that helper capture/export is not
  receiving that track's audio.
- A live format change (4/8/14) rebuilds each helper feed encoder before the next
  block, then reconfigures the MixBus ring. Regression tests exercise real
  8-to-14 and 14-to-4 payloads, not only status values.

### Stale shared-memory policy

Panner segments are deleted by their owning plugin instance on clean shutdown. The
helper additionally sweeps the shared directory, but never age-deletes a segment
whose DAW process is still alive. A dead process's panner files receive a ten-minute
plugin/session-reload grace period; unrecognized Mach1 memory files are removed only
after two hours. The helper-owned MixBus segment is always excluded from this sweep
(it is recreated on startup and removed on clean shutdown). Captured session data
on disk is governed separately by the StorageGovernor policies (never touches
sessions with live writers or pinned projects).

On macOS, CMake validates that the panner, monitor, and helper entitlement files all
contain the same `group.com.mach1.spatial.shared` application group. At runtime the
helper also creates and removes a write probe in the resolved shared-memory
directory; failures appear in diagnostics and the tray menu.

## Setup

### Build via CMake
- `mkdir cmake-build && cd cmake-build`
- `cmake ..` Create project files by adding the appropriate `-G Xcode` or `-G "Visual Studio 16 2019"` to the end of this line
- `cmake --build .`

## Install
Currently this helper service executable is expected in a common data directory of each local machine, and where applicable to be managed by a service agent or LaunchAgent.

### OSX
- `cmake -Bbuild -G "Xcode" -DCMAKE_INSTALL_PREFIX="/Library/Application Support/Mach1"`
- `cmake --build --configuration Release --install`

### WIN
- `cmake -Bbuild -G "Visual Studio 16 2019" -DCMAKE_INSTALL_PREFIX="%APP_DATA%\Mach1"`
- `cmake --build --configuration Release --install`