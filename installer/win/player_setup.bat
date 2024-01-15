@echo off

rem # MACH1 SPATIAL SYSTEM
rem # Previous version cleanup

rem # move the old videoplayer
if exist "%ProgramFiles(x86)%\Mach1 Spatial System\M1-VideoPlayer.exe" (
	MKDIR "%ProgramFiles%\Mach1"
	MKDIR "%ProgramFiles%\Mach1\legacy"
	MOVE /Y "%ProgramFiles(x86)%\Mach1 Spatial System\M1-VideoPlayer.exe" "%ProgramFiles%\Mach1\legacy\M1-VideoPlayer.exe"
) else (
)

rem # download and install ffmpeg libs

set ff_ver=6.1.1
rem if "%1" == "" (
rem     echo getting latest ffmpeg version 1>&2
rem     for /f "tokens=*" %%v in ('curl -L https://www.gyan.dev/ffmpeg/builds/release-version') do (
rem         set ff_ver=%%v
rem     )
rem ) else (
rem     set ff_ver=%1
rem )

echo downloading ffmpeg %ff_ver% 1>&2
curl -L "https://github.com/GyanD/codexffmpeg/releases/download/6.1.1/ffmpeg-6.1.1-full_build-shared.zip" -o "%ProgramFiles%\Mach1\ffmpeg.zip"
powershell Expand-Archive "%ProgramFiles%\Mach1\ffmpeg.zip" -DestinationPath "%ProgramFiles%\Mach1"
del "%ProgramFiles%\Mach1\ffmpeg.zip"
ren "%ProgramFiles%\Mach1\ffmpeg-6.1.1-full_build-shared" "%ProgramFiles%\Mach1\ffmpeg"

echo copying ffmpeg dlls 1>&2
copy %ProgramFiles%\Mach1\ffmpeg\bin\avcodec*.dll %ProgramFiles%\Mach1
copy %ProgramFiles%\Mach1\ffmpeg\bin\avformat*.dll %ProgramFiles%\Mach1
copy %ProgramFiles%\Mach1\ffmpeg\bin\avutil*.dll %ProgramFiles%\Mach1
copy %ProgramFiles%\Mach1\ffmpeg\bin\swresample*.dll %ProgramFiles%\Mach1
rmdir /s /q %ProgramFiles%\Mach1\ffmpeg