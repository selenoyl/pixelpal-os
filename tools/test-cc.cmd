@echo off
setlocal
set ROOT=%~dp0..
set MINGW=C:\msys64\mingw64\bin
set PATH=%MINGW%;%PATH%
cd /d "%ROOT%\build\windows-debug"
echo ROOT=%ROOT%
echo MINGW=%MINGW%
if not exist "sdk\CMakeFiles\pixelpal.dir\src" mkdir "sdk\CMakeFiles\pixelpal.dir\src"
"%MINGW%\cc.exe" --version
echo CC_VERSION_EXIT=%ERRORLEVEL%
"%MINGW%\cc.exe" -v -I"%ROOT%\sdk\include" -isystem C:/msys64/mingw64/include/SDL2 -g -std=c11 -c "%ROOT%\sdk\src\pixelpal.c" -o "sdk\CMakeFiles\pixelpal.dir\src\pixelpal-test.obj"
echo COMPILE_EXIT=%ERRORLEVEL%
exit /b %ERRORLEVEL%
