@echo off
REM Windows Service Installation Script for M1-System-Helper
REM Run as Administrator

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

pause 