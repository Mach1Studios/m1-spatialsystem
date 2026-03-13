@echo off
REM Legacy cleanup script for M1-System-Helper
REM Run as Administrator

echo Removing legacy M1-System-Helper Windows services...

sc stop "M1-System-Helper-Service" >nul 2>&1
sc stop "M1-System-Helper" >nul 2>&1
sc delete "M1-System-Helper-Service" >nul 2>&1
sc delete "M1-System-Helper" >nul 2>&1

if exist "C:\Program Files\Mach1\m1-system-helper.exe" (
    echo Helper tray app is ready for on-demand launch.
) else (
    echo WARNING: C:\Program Files\Mach1\m1-system-helper.exe was not found.
)
