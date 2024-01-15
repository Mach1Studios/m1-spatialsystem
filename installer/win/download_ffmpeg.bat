@echo off

rem # MACH1 SPATIAL SYSTEM
rem # Windows ffmpeg dep downloader

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

echo downloading ffmpeg 1>&2
curl -L "https://github.com/GyanD/codexffmpeg/releases/download/6.1.1/ffmpeg-6.1.1-full_build-shared.zip" -o "%ProgramW6432%\Mach1\ffmpeg.zip"
powershell Expand-Archive '%ProgramW6432%\Mach1\ffmpeg.zip' -DestinationPath '%ProgramW6432%\Mach1'
del "%ProgramW6432%\Mach1\ffmpeg.zip"
ren "%ProgramW6432%\Mach1\ffmpeg-6.1.1-full_build-shared" "ffmpeg"

echo copying ffmpeg dlls 1>&2
copy /Y "%ProgramW6432%\Mach1\ffmpeg\bin\avformat*.dll" "%ProgramW6432%\Mach1"
copy /Y "%ProgramW6432%\Mach1\ffmpeg\bin\avutil*.dll" "%ProgramW6432%\Mach1"
copy /Y "%ProgramW6432%\Mach1\ffmpeg\bin\avcodec*.dll" "%ProgramW6432%\Mach1"
copy /Y "%ProgramW6432%\Mach1\ffmpeg\bin\swscale*.dll" "%ProgramW6432%\Mach1"

echo removing downloaded ffmpeg dir 1>&2
rmdir /s /q "%ProgramW6432%\Mach1\ffmpeg"