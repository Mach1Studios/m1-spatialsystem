rem # MACH1 SPATIAL SYSTEM
rem # Services Setup

rem # Setup the m1-system-helper service
if exist "%ProgramData%\Mach1\" (
	sc create M1-System-Helper-Service binPath= "%ProgramData%\Mach1\m1-system-helper.exe" DisplayName= "M1-System-Helper" start= auto
)

rem # Setup the m1-orientationmanager service
if exist "%ProgramData%\Mach1\" (
	sc create M1-OrientationManager binPath= "%ProgramData%\Mach1\m1-orientationmanager.exe" DisplayName= "M1-OrientationManager"
)