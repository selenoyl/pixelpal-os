@echo off
setlocal

set ROOT=%~dp0..
pushd "%ROOT%" >nul

if not exist "build\windows-debug\tools\pixelpal-engine-cli\pixelpal-engine-cli.exe" (
  call "tools\build-pixelpal-engine-cli-windows.cmd"
  if errorlevel 1 (
    popd >nul
    exit /b 1
  )
)

set "PATH=C:\msys64\mingw64\bin;%ROOT%\build\windows-debug\tools\pixelpal-engine-cli;%PATH%"
"build\windows-debug\tools\pixelpal-engine-cli\pixelpal-engine-cli.exe" --smoke-test
set RESULT=%ERRORLEVEL%

popd >nul
exit /b %RESULT%
