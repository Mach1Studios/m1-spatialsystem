@echo off

rem # MACH1 SPATIAL SYSTEM
rem # Previous version cleanup

rem # move the old M1-Transcoder dir
if exist "%APPDATA%\Mach1 Spatial System\" (
	MKDIR "%APPDATA%\Mach1"
	RD /s /q "%APPDATA%\Mach1 Spatial System\" && echo "Removed old AppData Mach1 Spatial System directory."
)