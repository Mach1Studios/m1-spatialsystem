rem # MACH1 SPATIAL SYSTEM
rem # Services Setup

rem # m1-system-helper now runs as an on-demand tray app instead of a Windows service.
rem # Remove any legacy service registrations so the plugins can launch the GUI app directly.
sc stop "M1-System-Helper-Service" >nul 2>&1
sc stop "M1-System-Helper" >nul 2>&1
sc delete "M1-System-Helper-Service" >nul 2>&1
sc delete "M1-System-Helper" >nul 2>&1

if exist "%ProgramFiles%\Mach1\m1-system-helper.exe" (
	echo Configured M1-System-Helper for on-demand tray launch.
) else (
	echo WARNING: m1-system-helper.exe was not found in %%ProgramFiles%%\Mach1
)

rem # Setup the m1-orientationmanager service
if exist "%ProgramData%\Mach1\" (
	sc query M1-OrientationManager >nul && sc delete M1-OrientationManager
	sc create M1-OrientationManager binPath= "%ProgramData%\Mach1\m1-orientationmanager.exe" DisplayName= "M1-OrientationManager"
	sc description M1-OrientationManager "Watches for known 3rd party IMU or headtracking sesnors and manages them."
)
