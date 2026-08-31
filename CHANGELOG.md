# Changelog

All notable changes to the **Mach1 Spatial System** (M1-Panner, M1-Monitor, M1-Player, M1-OrientationManager, m1-system-helper, M1-Transcoder, and the installer).

SDK library notes belong in `m1-sdk`. This file is the product-suite changelog. Versions match the root `VERSION` file and git tags `v2.1` onward (there is no `v2.0` tag).

Agents append bullets under `## [Unreleased]` in the **same commit** as the code. When a version is cut, move that section to `## [x.y] - YYYY-MM-DD` in the version-bump commit.

## [Unreleased]

### Added

- M1-Transcoder multi-mono exports, including Batch Convert jobs, can start channel filenames at 0 or 1 and optionally use destination-named folders.

### Changed

- M1-Transcoder opens Batch Convert from the View menu.

### Fixed

### Removed

## [3.0] - 2026-08-14

### Added

- External renderer: streaming panners on mono/stereo DAW buses send audio to m1-system-helper; MixEngine publishes a shared MixBus for stereo monitors (Ableton Live and other narrow-bus hosts).
- Project pairing: plugins persist `projectBindingId` / `pluginInstanceId`; the helper names sessions, resumes capture directories across host-process restarts, and refuses to merge two named projects in one DAW.
- StorageGovernor and capture/export UI for helper session data, including stale/orphan cleanup that never deletes live writers or pinned projects.
- Ring-buffer shared-memory layout (M1MemoryShare v2) for panner→helper audio and two-way parameter control.

### Changed

- Native 4/8/14-channel buses stay in-plugin; those panners keep a parameter-only heartbeat (`Multichannel (not streaming)`).
- Helper tray toggle to enable/disable audio streaming; persisted per user as `externalRendererEnabled`.

### Fixed

- Session project-management flow for naming and selecting helper capture sessions.
- Stale shared-memory / dead-process audio tracking so leftover `.mem` files do not collide with a reopened project.

## [2.7] - 2026-07-29

### Changed

- Lower monitor→helper OSC message rate.
- Stereo downfold path in the panner/helper stack.

### Fixed

- Helper messaging hardening (less redundant broadcast / safer IPC).
- Windows build automation and PowerShell signing for the installer.

## [2.6] - 2026-07-08

### Fixed

- macOS helper socket startup when hosted in Pro Tools.

## [2.5] - 2026-06-23

### Changed

- Panner overlay window-grab behavior (Pro Tools video window targeting).
- Versioning and codesigning flow for the suite.

## [2.4] - 2026-06-13

### Fixed

- macOS build and packaging issues.

## [2.3] - 2026-03-23

### Added

- m1-system-helper as an on-demand tray app with a summary UI, monitor/soundfield display, and 3D panner viewer.
- Shared-memory transport (M1MemoryShare) between panner instances and the helper, including a capture-engine stub for the external mixer.
- Two-way OSC so the active monitor instance can be selected and driven from the helper.

### Fixed

- Windows Session UI.
- Panner-instance tracking, mem-share header reading, and helper GUI layout.
- Windows/macOS CI compile strictness and installer issues.

## [2.2] - 2026-03-12

### Added

- Nuendo session template under `installer/resources/templates/Nuendo`.

### Fixed

- Panner GUI memory leaks.
- Windows signing, macOS notarization timeouts, and multi-installer flow from CI.
- Release workflow now runs only on version tags.

## [2.1] - 2025-12-18

First tagged 2.x Spatial System super-repo (plugins + services + installer).

### Added

- Unified Makefile super-repo: nested product submodules, `make dev` / `configure` / `build` / `package`.
- m1-system-helper (system watcher) in-tree: launchd on macOS, Windows service, ports from `settings.json`, shutdown when no clients remain.
- macOS Packages and Windows Inno Setup installers, notarization, and codesigning (apps, VST3, AAX, transcoder).
- User documentation site and DAW templates (including Mach1 Spatial 14-channel width).
- M1-Player libVLC playback backend.
- Azure Trusted Signing path for Windows.

### Changed

- Bundle IDs renamed to `com.mach1.spatial.*`.
- `.jucer` files removed as the build source of truth (CMake only).
- Panner XY controls removed in favor of azimuth / elevation / diverge.

### Fixed

- Panner and monitor processing, I/O, and Pro Tools-targeted bugs.
- Monitor keyboard (arrow keys) and orientation-manager connection to the monitor.
- macOS service install scripts, helper naming, and m1-player / transcoder codesigning.

[Unreleased]: https://github.com/Mach1Studios/m1-spatialsystem/compare/v3.0...HEAD
[3.0]: https://github.com/Mach1Studios/m1-spatialsystem/releases/tag/v3.0
[2.7]: https://github.com/Mach1Studios/m1-spatialsystem/releases/tag/v2.7
[2.6]: https://github.com/Mach1Studios/m1-spatialsystem/releases/tag/v2.6
[2.5]: https://github.com/Mach1Studios/m1-spatialsystem/releases/tag/v2.5
[2.4]: https://github.com/Mach1Studios/m1-spatialsystem/releases/tag/v2.4
[2.3]: https://github.com/Mach1Studios/m1-spatialsystem/releases/tag/v2.3
[2.2]: https://github.com/Mach1Studios/m1-spatialsystem/releases/tag/v2.2
[2.1]: https://github.com/Mach1Studios/m1-spatialsystem/releases/tag/v2.1
