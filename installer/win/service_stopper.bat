rem # MACH1 SPATIAL SYSTEM
rem # Services Stopper

rem # Stop any running helper instances and remove legacy helper services
taskkill /IM "m1-system-helper.exe" /F >nul 2>&1
sc stop "M1-System-Helper-Service" >nul 2>&1
sc stop "M1-System-Helper" >nul 2>&1

rem # Stop the m1-orientationmanager service
sc stop "M1-OrientationManager"