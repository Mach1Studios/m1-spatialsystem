@echo off

rem # MACH1 SPATIAL SYSTEM
rem # Previous version cleanup

rem # move the old videoplayer
if exist "%ProgramFiles(x86)%\Mach1 Spatial System\M1-VideoPlayer.exe" (
	MKDIR "%ProgramFiles%\Mach1"
	MKDIR "%ProgramFiles%\Mach1\legacy"
	MOVE /Y "%ProgramFiles(x86)%\Mach1 Spatial System\M1-VideoPlayer.exe" "%ProgramFiles%\Mach1\legacy\M1-VideoPlayer.exe"
)