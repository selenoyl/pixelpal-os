@echo off
setlocal

pushd "%~dp0.." >nul || (
  echo Failed to resolve project root.
  exit /b 1
)
set "ROOT=%CD%"
popd >nul

set "PATH=C:\msys64\mingw64\bin;%ROOT%\build\windows-debug\games\priory\bin;%PATH%"
set "TMP=%ROOT%\build\tmp"
set "TEMP=%TMP%"
set "TMPDIR=%TMP%"

if not exist "%ROOT%\build\windows-debug\games\priory\bin\priory.exe" (
  echo Priory is not staged yet.
  echo Run tools\build-priory-windows.cmd first.
  exit /b 1
)

"%ROOT%\build\windows-debug\games\priory\bin\priory.exe"
