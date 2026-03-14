@echo off
setlocal

pushd "%~dp0.." >nul || (
  echo Failed to resolve project root.
  exit /b 1
)
set "ROOT=%CD%"

set "PATH=C:\msys64\mingw64\bin;%ROOT%\build\windows-debug\tools\priory-editor;%PATH%"

if not exist "%ROOT%\build\windows-debug\tools\priory-editor\priory-editor.exe" (
  echo Priory Editor is not built yet.
  echo Run tools\build-priory-editor-windows.cmd first.
  popd >nul
  exit /b 1
)

"%ROOT%\build\windows-debug\tools\priory-editor\priory-editor.exe" %*
popd >nul
