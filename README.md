# m1-spatialsystem
DAW focused plugins and apps relating to mixing Mach1 Spatial multichannel mixes

## Setup

### MacOS
- update the `./Makefile.variables` with the needed variables, specifically the VST SDK and AAX SDK paths
- `make dev` will setup xcode local dev environment for all products

### Windows


## Known Issues
- [MacOS] m1-orientationmanager and external BLE device handling issues in macOS versions 12.0, 12.1 and 12.2

## TODO
- Add "Sync Panner instances to Monitor" button this updates all panners to monitor's current outputMode
- Add Camera and fusion orientation to OrientationManager
- Add Panner position drawing to M1-Player
- Add windows and linux service handling
- Add id system to clients to detect more than one M1-Monitor instance
- Add better angle reset and offset adding management for clients