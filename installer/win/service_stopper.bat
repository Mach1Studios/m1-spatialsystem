rem # MACH1 SPATIAL SYSTEM
rem # Services Stopper

rem # Stop the m1-system-helper service
sc stop "M1-System-Helper-Service"

rem # Stop the m1-orientationmanager service
sc stop "M1-OrientationManager"