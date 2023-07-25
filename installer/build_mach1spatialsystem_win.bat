@echo off

setlocal
setlocal EnableDelayedExpansion
CALL %UserProfile%\m1-globallocal.bat

SET WIN_SIGN_TOOL=%WIN_SIGN_TOOL%
SET WIN_SIGN_TOOL_PASS=%WIN_SIGN_TOOL_PASS%

REM use VS2017 for oF
set PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Community\MSBuild\15.0\Bin;%PATH%

REM Installer (FULL)
echo "Package (FULL) AAX/VST/VST3/AU Installer?"
set yn=Y
set /P yn="[Y]/N? "
if /I !yn! EQU Y (
            echo "### PACKAGING FULL AAX/VST/VST3/AU INSTALLER ###"
            cd %M1WORKFLOW_PATH%\installer\win
            "%programfiles(x86)%\Inno Setup 5\ISCC.exe" /Qp installer.iss
            echo "### FINISHED FULL AAX/VST/VST3/AU INSTALLER ###"
)

cd %M1WORKFLOW_PATH%\installer\win\Output
echo "Deploy Installer?"
set yn=Y
set /P yn="[Y]/N? "
if /I !yn! EQU Y (
            echo "### DEPLOYING TO S3 ###"
            @echo off
            set /p version="Version: "
            echo "### Version: !version! ###"
            echo "link: s3://mach1-releases/!version!/Mach1 Spatial System Installer.exe"
            pause
            aws s3 cp "Mach1 Spatial System Installer.exe" "s3://mach1-releases/!version!/Mach1 Spatial System Installer.exe" --profile mach1 --content-disposition "Mach1 Spatial System Installer.exe"
            echo "REMINDER: Update the Avid Store Submission per version update!"
)
endlocal
pause