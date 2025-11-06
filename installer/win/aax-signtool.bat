@echo off

:: KDSP Signtool wrapper for Eden SDK / AAX wraptool
::
:: wraptool invokes signtool but makes a lot of assumptions about how we're going to sign
:: which are incompatible with Azure Trusted Signing.
::
:: This tool removes all its arguments and replaces it with the correct or necessary ones.
:: Please adjust accordingly if necessary.
::
:: Run Eden SDK's wraptool as follows:
::
:: wraptool.exe sign --signtool aax-signtool.bat --signid 1 --verbose --installedbinaries --account ... --password ... --wcguid ... --in ...
::
:: signid 1 is bogus, but wraptool needs this nonsense in order to start up..
::
:: The following environment variables are necessary:
::
:: SIGNTOOL_PATH
:: ACS_DLIB (points to Dlib.dll file)
:: ACS_JSON (points to the metadata.json file)
:: AZURE_TENANT_ID (Microsoft Azure tenant ID)
:: AZURE_CLIENT_ID (Microsoft Azure codesigning app client ID)
:: AZURE_SECRET_ID (Microsoft Azure codesigning app secret value)
::

:: Get script root dir, so we can find aax-signtool.py
set root=%~dp0
set root=%root:~0,-1%

:: wraptool seems to mangle signtool's args and doesn't properly quote-escape the final binary path,
:: and batch is not easy with string handling, so we use python to fix things up..
set args=%*
echo %args%>args.tmp
echo Patched signtool: Input arguments: %args%
python "%root%\aax-signtool.py"
set /p args=<args.tmp
echo Patched signtool: Filtered arguments: %args%
set file="%args%"

echo Patched signtool: File to sign: %file%

:: Strip quotes and trim leading/trailing spaces from environment variables
:: Makefile variables may have quotes that need to be removed
:: Clean variables directly in main scope (no setlocal/endlocal)
setlocal enabledelayedexpansion
:: Remove quotes and clean
set "SIGNTOOL_PATH=!SIGNTOOL_PATH:"=!"
set "SIGNTOOL_PATH=!SIGNTOOL_PATH:'=!"
set "ACS_DLIB=!ACS_DLIB:"=!"
set "ACS_DLIB=!ACS_DLIB:'=!"
set "ACS_JSON=!ACS_JSON:"=!"
set "ACS_JSON=!ACS_JSON:'=!"
set "AZURE_TENANT_ID=!AZURE_TENANT_ID:"=!"
set "AZURE_TENANT_ID=!AZURE_TENANT_ID:'=!"
set "AZURE_CLIENT_ID=!AZURE_CLIENT_ID:"=!"
set "AZURE_CLIENT_ID=!AZURE_CLIENT_ID:'=!"
:: Note: Makefile sets AZURE_SECRET_ID, but Azure DLL expects AZURE_CLIENT_SECRET
set "AZURE_CLIENT_SECRET=!AZURE_SECRET_ID:"=!"
set "AZURE_CLIENT_SECRET=!AZURE_CLIENT_SECRET:'=!"
:: Actually trim trailing/leading spaces using PowerShell (batch set doesn't trim)
:: Write trimmed values to temp files, then read back
powershell -NoProfile -Command "('!SIGNTOOL_PATH!').Trim()" >%TEMP%\st_path.tmp
powershell -NoProfile -Command "('!ACS_DLIB!').Trim()" >%TEMP%\st_dlib.tmp
powershell -NoProfile -Command "('!ACS_JSON!').Trim()" >%TEMP%\st_json.tmp
powershell -NoProfile -Command "('!AZURE_TENANT_ID!').Trim()" >%TEMP%\st_tenant.tmp
powershell -NoProfile -Command "('!AZURE_CLIENT_ID!').Trim()" >%TEMP%\st_client.tmp
powershell -NoProfile -Command "('!AZURE_CLIENT_SECRET!').Trim()" >%TEMP%\st_secret.tmp
:: Read back trimmed values (for /f automatically trims leading spaces, quotes handle trailing)
for /f "usebackq tokens=* delims=" %%a in ("%TEMP%\st_path.tmp") do set "SIGNTOOL_PATH=%%a"
for /f "usebackq tokens=* delims=" %%a in ("%TEMP%\st_dlib.tmp") do set "ACS_DLIB=%%a"
for /f "usebackq tokens=* delims=" %%a in ("%TEMP%\st_json.tmp") do set "ACS_JSON=%%a"
for /f "usebackq tokens=* delims=" %%a in ("%TEMP%\st_tenant.tmp") do set "AZURE_TENANT_ID=%%a"
for /f "usebackq tokens=* delims=" %%a in ("%TEMP%\st_client.tmp") do set "AZURE_CLIENT_ID=%%a"
for /f "usebackq tokens=* delims=" %%a in ("%TEMP%\st_secret.tmp") do set "AZURE_CLIENT_SECRET=%%a"
del %TEMP%\st_*.tmp >nul 2>&1
:: Variables are now cleaned and will persist (setlocal keeps them for rest of script)

if not defined SIGNTOOL_PATH (
	echo Patched signtool: ERROR: SIGNTOOL_PATH not defined
	exit /b 1000
)
if not exist "%SIGNTOOL_PATH%" (
	echo Patched signtool: ERROR: Could not find signtool.exe at "%SIGNTOOL_PATH%"
	exit /b 1000
)

if not defined ACS_DLIB (
	echo Patched signtool: ERROR: ACS_DLIB not defined
	exit /b 1000
)
if not exist "%ACS_DLIB%" (
	echo Patched signtool: ERROR: Could not find Azure DLL at "%ACS_DLIB%"
	exit /b 1000
)

if not defined ACS_JSON (
	echo Patched signtool: ERROR: ACS_JSON not defined
	exit /b 1000
)
if not exist "%ACS_JSON%" (
	echo Patched signtool: ERROR: Could not find metadata JSON at "%ACS_JSON%"
	exit /b 1000
)

echo Patched signtool: Executing: "!SIGNTOOL_PATH!" sign /v /debug /fd SHA256 /tr "http://timestamp.acs.microsoft.com" /td SHA256 /dlib "!ACS_DLIB!" /dmdf "!ACS_JSON!" %file%
echo Patched signtool: Azure Tenant ID: [!AZURE_TENANT_ID!]
echo Patched signtool: Azure Tenant ID length: !AZURE_TENANT_ID:~0,36!
echo Patched signtool: Azure Client ID: [!AZURE_CLIENT_ID!]
echo Patched signtool: Azure Client Secret: [REDACTED]
"!SIGNTOOL_PATH!" sign /v /debug /fd SHA256 /tr "http://timestamp.acs.microsoft.com" /td SHA256 /dlib "!ACS_DLIB!" /dmdf "!ACS_JSON!" %file%
@if %errorlevel% neq 0 exit /b %errorlevel%

echo Patched signtool: Success

