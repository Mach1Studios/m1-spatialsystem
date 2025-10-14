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

## AAX Plugin Validation

AAX plugins require special validation to ensure they work properly in Pro Tools. 

### Setup
Set `AAX_VALIDATOR_PATH` in `Makefile.variables` to point to your PACE AAX Validator `dsh` tool:
```makefile
AAX_VALIDATOR_PATH=/path/to/aax-validator-dsh-.../CommandLineTools/dsh
```

### Available Targets
- `make test-aax-monitor` - Validate M1-Monitor AAX plugin (dev build)
- `make test-aax-panner` - Validate M1-Panner AAX plugin (dev build)
- `make test-aax-plugins` - Validate both AAX plugins (dev builds)
- `make test-aax-release` - Validate release AAX plugins
- `make verify-aax-signing` - Verify code signing on AAX plugins
- `make diagnose-aax` - Generate diagnostic report for AAX issues

### User Diagnostic Tools

If users report AAX plugins not loading in Pro Tools, diagnostic scripts are available:
- **macOS**: `./installer/osx/diagnose_aax_issues.sh`
- **Windows**: `installer\win\diagnose_aax_issues.bat`

These scripts check for common issues (missing iLok, quarantine flags, code signing problems) and provide recommended fixes.

## Known Issues
- [MacOS] m1-orientationmanager and external BLE device handling issues in macOS versions 12.0, 12.1 and 12.2

## TODO
[Github Project Board](https://github.com/orgs/Mach1Studios/projects/4)

## Communication Map
The following describes what is communicated between all apps and plugins via OSC and UDP, the ports are described and set by the [settings.json](m1-orientationmanager/Resources/settings.json) file.

- OrientationManager -> Monitor [sends 3rd party orientation]
- OrientationManager -> Player [sends 3rd party orientation]
- Monitor -> m1-system-helper -> Panners [sends calculated orientation for GUI]
- Panners -> m1-system-helper -> Player [sends panner settings for drawing in Player]
- Player -> m1-system-helper -> Monitor [sends mouse offset orientation to monitor]
- Monitor -> m1-system-helper [transport]
- m1-system-helper -> Player [transport]
