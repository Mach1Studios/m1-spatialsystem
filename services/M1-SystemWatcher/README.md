# M1-SystemWatcher
A background executable that checks if the server is still active and operating and otherwise attempts to relaunch it properly. This should be launched by any and all `m1_orientation_client` or client to the server.

## Setup

### Build via CMake
- `mkdir cmake-build && cd cmake-build`
- `cmake ..` Create project files by adding the appropriate `-G Xcode` or `-G "Visual Studio 16 2019"` to the end of this line
- `cmake --build .`

### Build via .jucer
- Open the `M1-OrientationManager-Watcher.jucer` and compile as needed