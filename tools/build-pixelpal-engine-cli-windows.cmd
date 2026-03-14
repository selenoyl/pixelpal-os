@echo off
setlocal

pushd "%~dp0.." >nul || (
  echo Failed to resolve project root.
  exit /b 1
)
set "ROOT=%CD%"
popd >nul

if not exist "C:\msys64\mingw64\bin\g++.exe" (
  echo Missing compiler: C:\msys64\mingw64\bin\g++.exe
  echo Install the MinGW-w64 toolchain in MSYS2 first.
  exit /b 1
)

for %%F in (
  "C:\msys64\mingw64\bin\libgcc_s_seh-1.dll"
  "C:\msys64\mingw64\bin\libmpc-3.dll"
  "C:\msys64\mingw64\bin\libgmp-10.dll"
  "C:\msys64\mingw64\bin\libisl-23.dll"
  "C:\msys64\mingw64\bin\libstdc++-6.dll"
  "C:\msys64\mingw64\bin\libwinpthread-1.dll"
) do (
  if not exist %%~F (
    echo Missing MinGW runtime dependency: %%~F
    echo Repair the MSYS2 MinGW-w64 toolchain before building.
    exit /b 1
  )
)

set "PATH=C:\msys64\mingw64\bin;C:\Windows\System32;C:\Windows;C:\Windows\System32\Wbem"
set "TMP=%ROOT%\build\tmp"
set "TEMP=%TMP%"
set "TMPDIR=%TMP%"

if not exist "%TMP%" mkdir "%TMP%"
if not exist "%ROOT%\build\windows-debug\tools\pixelpal-engine-cli" mkdir "%ROOT%\build\windows-debug\tools\pixelpal-engine-cli"

"C:\msys64\mingw64\bin\g++.exe" -g -std=c++20 ^
  -I"%ROOT%\sdk\include" ^
  "%ROOT%\sdk\src\rpg_engine.cpp" ^
  "%ROOT%\tools\pixelpal-engine-cli\main.cpp" ^
  -o "%ROOT%\build\windows-debug\tools\pixelpal-engine-cli\pixelpal-engine-cli.exe" || exit /b 1

copy /Y "C:\msys64\mingw64\bin\libgcc_s_seh-1.dll" "%ROOT%\build\windows-debug\tools\pixelpal-engine-cli\" >nul
copy /Y "C:\msys64\mingw64\bin\libstdc++-6.dll" "%ROOT%\build\windows-debug\tools\pixelpal-engine-cli\" >nul
copy /Y "C:\msys64\mingw64\bin\libwinpthread-1.dll" "%ROOT%\build\windows-debug\tools\pixelpal-engine-cli\" >nul

echo PixelPal engine CLI build complete.
exit /b 0
