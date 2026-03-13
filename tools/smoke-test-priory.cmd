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

call "%~dp0build-priory-windows.cmd" || exit /b 1
"%ROOT%\build\windows-debug\sample-games\priory\priory.exe" --smoke-test
