@echo off
setlocal

set "OG_DIR=%~dp0..\external\priory\Priory\src\Priory.Game\bin\Debug\net10.0"

where dotnet >nul 2>nul || (
  echo Missing dotnet on PATH.
  exit /b 1
)

pushd "%OG_DIR%" >nul || (
  echo Original Priory text build not found at:
  echo   %OG_DIR%
  exit /b 1
)

if not exist "Priory.Game.dll" (
  echo Missing Priory.Game.dll in:
  echo   %CD%
  popd >nul
  exit /b 1
)

dotnet "Priory.Game.dll"
set "RC=%ERRORLEVEL%"
popd >nul
exit /b %RC%
