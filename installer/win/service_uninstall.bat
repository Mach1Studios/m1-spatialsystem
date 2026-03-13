taskkill /IM "m1-system-helper.exe" /F >nul 2>&1
sc stop "M1-System-Helper-Service" >nul 2>&1
sc stop "M1-System-Helper" >nul 2>&1
sc stop "M1-OrientationManager"

sc delete M1-System-Helper-Service
sc delete M1-System-Helper
sc delete M1-OrientationManager
