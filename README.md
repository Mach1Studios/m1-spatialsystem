# m1-spatialsystem
DAW focused plugins and apps relating to mixing Mach1 Spatial multichannel mixes

## Setup
- `make pull` to pull all the nested submodules (check that you have pulled them all)
- Update the `./Makefile.variables` with the needed variables, specifically the VST SDK and AAX SDK paths
- `make dev` will setup xcode local dev environment for all products

### MacOS
- `make setup-codeisgning` will store your apple credentials for notarization steps
- `make configure` will clear and configure for "Release"
- `make package` will build/codesign/notarize and package an installer for "Release"

#### Debugging and running locally
To properly debug the services [m1-system-helper](services/m1-system-helper) and [m1-orientationmanager](m1-orientationmanager) it is recommended to clear all the built and installed components to remove the services from the control of `launchd` so you can compile and debug via an IDE.

- run `make clear-installs`
- run `make dev`
- open each compoennt/service in xcode

### Windows
- `make configure` will clear and configure for "Release"
- `make package` will build/codesign/notarize and package an installer for "Release"

#### Codesigning
- Use SafeNet Authentication Client app to check and update current windows certificates for codesigning (via the hardware usb key)
- Use `certmgr.exe` and open the Personal certificates to grab the `Thumbprint` from the relevant Digicert Codesigning Certificate to be used in the codesign step via `WIN_SIGNTOOL_ID`

## Known Issues
- [MacOS] m1-orientationmanager and external BLE device handling issues in macOS versions 12.0, 12.1 and 12.2

## TODO
- Add "Sync Panner instances to Monitor" button this updates all panners to monitor's current outputMode
- Add Camera and fusion orientation to OrientationManager
- ~Add Panner position drawing to M1-Player~
- Add ~windows and~ linux service handling
- ~Add id system to clients to detect more than one M1-Monitor instance~
- ~Add better angle reset and offset adding management for clients~
- ~Fix PostInstall script for m1-orientationmanager on macos to support installing without requiring restarts~

## Communication Map
The following describes what is communicated between all apps and plugins via OSC and UDP, the ports are described and set by the [settings.json](m1-orientationmanager/Resources/settings.json) file.

- OrientationManager -> Monitor [sends 3rd party orientation]
- OrientationManager -> Player [sends 3rd party orientation]
- Monitor -> Panners [sends calculated orientation for GUI]
- Panners -> Player [sends panner settings for drawing in Player]
- Player -> Monitor [sends mouse offset orientation to monitor]
- Monitor -> OrientationManager [transport]
- OrientationManager -> Player [transport]
