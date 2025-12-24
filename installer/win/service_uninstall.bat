sc stop "M1-System-Helper"
sc stop "M1-OrientationManager"

sc delete M1-System-Helper-Service
sc delete M1-System-Helper
sc delete M1-OrientationManager

pause

