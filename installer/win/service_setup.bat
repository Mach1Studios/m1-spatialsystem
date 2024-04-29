rem # MACH1 SPATIAL SYSTEM
rem # Services Setup

rem # Setup the m1-system-helper service
if exist "%ProgramData%\Mach1\" (
	sc query M1-System-Helper-Service >nul && sc delete M1-System-Helper-Service
	sc create M1-System-Helper-Service binPath= "%ProgramData%\Mach1\m1-system-helper.exe" DisplayName= "M1-System-Helper" start= auto
	sc description M1-System-Helper-Service "Watches and tracks all Mach1 Spatial System plugins/apps."
	sc start M1-System-Helper-Service
)

rem # Setup the m1-orientationmanager service
if exist "%ProgramData%\Mach1\" (
	sc query M1-OrientationManager >nul && sc delete M1-OrientationManager
	sc create M1-OrientationManager binPath= "%ProgramData%\Mach1\m1-orientationmanager.exe" DisplayName= "M1-OrientationManager"
	sc description M1-OrientationManager "Watches for known 3rd party IMU or headtracking sesnors and manages them."
)
