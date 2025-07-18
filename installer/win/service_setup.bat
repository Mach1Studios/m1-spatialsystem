rem # MACH1 SPATIAL SYSTEM
rem # Services Setup

rem # Setup the m1-system-helper service
if exist "%ProgramData%\Mach1\" (
	sc query M1-System-Helper-Service >nul && sc delete M1-System-Helper-Service
	sc query M1-System-Helper >nul && sc delete M1-System-Helper

	echo Installing M1-System-Helper Windows Service...

	REM Create the service with manual start type (on-demand)
	sc create "M1-System-Helper" ^
		binPath= "C:\Program Files\Mach1\m1-system-helper.exe" ^
		DisplayName= "Mach1 Spatial System Helper" ^
		Description= "Background service for Mach1 Spatial System plugin & app tracking" ^
		start= demand ^
		type= own

	if %errorlevel% == 0 (
		echo Service installed successfully!
		echo Service will start on-demand when Mach1 applications request it.
		
		REM Set service to auto-recover on failure
		sc failure "M1-System-Helper" reset= 86400 actions= restart/30000/restart/60000/restart/120000
		
		echo Configuration complete.
	) else (
		echo Failed to install service. Make sure you're running as Administrator.
	)
)

rem # Setup the m1-orientationmanager service
if exist "%ProgramData%\Mach1\" (
	sc query M1-OrientationManager >nul && sc delete M1-OrientationManager
	sc create M1-OrientationManager binPath= "%ProgramData%\Mach1\m1-orientationmanager.exe" DisplayName= "M1-OrientationManager"
	sc description M1-OrientationManager "Watches for known 3rd party IMU or headtracking sesnors and manages them."
)
