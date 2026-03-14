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
if not exist "%ROOT%\build\windows-debug\sample-games\priory" mkdir "%ROOT%\build\windows-debug\sample-games\priory"
if not exist "%ROOT%\build\windows-debug\games\priory\bin" mkdir "%ROOT%\build\windows-debug\games\priory\bin"
if not exist "%ROOT%\build\windows-debug\sdk\libpixelpal.a" (
  echo Missing %ROOT%\build\windows-debug\sdk\libpixelpal.a
  echo Run the PixelPal Windows build once before building Priory:
  echo   C:\msys64\mingw64\bin\cmake.exe --preset windows-debug
  echo   C:\msys64\mingw64\bin\cmake.exe --build --preset windows-debug --target pixelpal
  exit /b 1
)

"C:\msys64\mingw64\bin\g++.exe" -g -std=c++20 -DSDL_MAIN_HANDLED -I"%ROOT%\sdk\include" -isystem C:/msys64/mingw64/include/SDL2 -c "%ROOT%\sample-games\priory\main.cpp" -o "%ROOT%\build\windows-debug\sample-games\priory\main.cpp.obj" || exit /b 1
"C:\msys64\mingw64\bin\g++.exe" -g "%ROOT%\build\windows-debug\sample-games\priory\main.cpp.obj" "%ROOT%\build\windows-debug\sdk\libpixelpal.a" C:/msys64/mingw64/lib/libSDL2.dll.a -lkernel32 -luser32 -lgdi32 -lwinspool -lshell32 -lole32 -loleaut32 -luuid -lcomdlg32 -ladvapi32 -o "%ROOT%\build\windows-debug\sample-games\priory\priory.exe" || exit /b 1

copy /Y "%ROOT%\sample-games\priory\manifest.toml" "%ROOT%\build\windows-debug\games\priory\" >nul
copy /Y "%ROOT%\sample-games\priory\icon.ppm" "%ROOT%\build\windows-debug\games\priory\" >nul
copy /Y "%ROOT%\sample-games\priory\splash.ppm" "%ROOT%\build\windows-debug\games\priory\" >nul
copy /Y "%ROOT%\build\windows-debug\sample-games\priory\priory.exe" "%ROOT%\build\windows-debug\games\priory\bin\" >nul
copy /Y "C:\msys64\mingw64\bin\SDL2.dll" "%ROOT%\build\windows-debug\sample-games\priory\" >nul
copy /Y "C:\msys64\mingw64\bin\libgcc_s_seh-1.dll" "%ROOT%\build\windows-debug\sample-games\priory\" >nul
copy /Y "C:\msys64\mingw64\bin\libstdc++-6.dll" "%ROOT%\build\windows-debug\sample-games\priory\" >nul
copy /Y "C:\msys64\mingw64\bin\libwinpthread-1.dll" "%ROOT%\build\windows-debug\sample-games\priory\" >nul
copy /Y "C:\msys64\mingw64\bin\SDL2.dll" "%ROOT%\build\windows-debug\games\priory\bin\" >nul
copy /Y "C:\msys64\mingw64\bin\libgcc_s_seh-1.dll" "%ROOT%\build\windows-debug\games\priory\bin\" >nul
copy /Y "C:\msys64\mingw64\bin\libstdc++-6.dll" "%ROOT%\build\windows-debug\games\priory\bin\" >nul
copy /Y "C:\msys64\mingw64\bin\libwinpthread-1.dll" "%ROOT%\build\windows-debug\games\priory\bin\" >nul

echo Priory build complete.
