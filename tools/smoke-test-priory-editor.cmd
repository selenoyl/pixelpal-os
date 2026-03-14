@echo off
setlocal

pushd "%~dp0.." >nul || (
  echo Failed to resolve project root.
  exit /b 1
)
set "ROOT=%CD%"

if not exist "%ROOT%\build\windows-debug\tools\priory-editor\priory-editor.exe" (
  call "%ROOT%\tools\build-priory-editor-windows.cmd" || (
    popd >nul
    exit /b 1
  )
)

set "PATH=C:\msys64\mingw64\bin;%ROOT%\build\windows-debug\tools\priory-editor;%PATH%"
"%ROOT%\build\windows-debug\tools\priory-editor\priory-editor.exe" --smoke-test
set "RESULT=%ERRORLEVEL%"
popd >nul
exit /b %RESULT%
