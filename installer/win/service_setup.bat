rem # MACH1 SPATIAL SYSTEM
rem # Services Setup

rem # Setup the m1-system-helper service
if exist "%ProgramFiles%\Mach1\" (
	sc create M1-System-Helper-Service binPath= "%ProgramFiles%\Mach1\services\m1-system-helper.exe" DisplayName= "M1-System-Helper" start= auto
) else (
	sc create M1-System-Helper-Service binPath= "%ProgramFiles(x86)%\Mach1\services\m1-system-helper.exe" DisplayName= "M1-System-Helper" start= auto
)

rem # Setup the m1-orientationmanager service
if exist "%ProgramFiles%\Mach1\" (
	sc create M1-OrientationManager binPath= "%ProgramFiles%\Mach1\services\m1-orientationmanager.exe" DisplayName= "M1-OrientationManager"
) else (
	sc create M1-OrientationManager binPath= "%ProgramFiles(x86)%\Mach1\services\m1-orientationmanager.exe" DisplayName= "M1-OrientationManager"
)