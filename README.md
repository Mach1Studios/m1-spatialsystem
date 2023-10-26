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
- Refactor panner/monitor/player communication to a new class within Monitor instead of within OrientationManager
- Fix PostInstall script for m1-orientationmanager on macos to support installing without requiring restarts

## Communication Map
The following describes what is communicated between all apps and plugins via OSC and UDP, the ports are described and set by the [settings.json](m1-orientationmanager/Resources/settings.json) file.

- OrientationManager -> Monitor [sends 3rd party orientation]
- Monitor -> OrientationManager [sends calculated orientation for Panner[s]/Player]
- OrientationManager -> Player [sends 3rd party orientation]
- OrientationManager -> Player [sends panners settings]
- Panner[s] -> OrientationManager [sends panner settings]
- Player -> Monitor [sends orientation]
- Monitor -> OrientationManager [transport]
- OrientationManager -> Player [transport]
