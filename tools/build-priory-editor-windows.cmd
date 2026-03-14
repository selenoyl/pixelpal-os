@echo off
setlocal

pushd "%~dp0.." >nul || (
  echo Failed to resolve project root.
  exit /b 1
)
set "ROOT=%CD%"

if not exist "C:\msys64\mingw64\bin\cmake.exe" (
  echo Missing C:\msys64\mingw64\bin\cmake.exe
  echo Install the MinGW-w64 toolchain in MSYS2 first.
  popd >nul
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
    popd >nul
    exit /b 1
  )
)

set "PATH=C:\msys64\mingw64\bin;C:\Windows\System32;C:\Windows;C:\Windows\System32\Wbem"
set "TMP=%ROOT%\build\tmp"
set "TEMP=%TMP%"
if not exist "%TMP%" mkdir "%TMP%"

set "EDITOR_DIR=%ROOT%\build\windows-debug\tools\priory-editor"
set "EDITOR_OBJ=%EDITOR_DIR%\main.cpp.obj"
set "EDITOR_EXE=%ROOT%\build\windows-debug\tools\priory-editor\priory-editor.exe"

if not exist "%EDITOR_DIR%" mkdir "%EDITOR_DIR%"

"C:\msys64\mingw64\bin\g++.exe" -g -std=c++20 -DSDL_MAIN_HANDLED -I"%ROOT%\sdk\include" -isystem C:/msys64/mingw64/include/SDL2 -c "%ROOT%\tools\priory-editor\main.cpp" -o "%EDITOR_OBJ%" || (
  popd >nul
  exit /b 1
)
"C:\msys64\mingw64\bin\g++.exe" -g "%EDITOR_OBJ%" C:/msys64/mingw64/lib/libSDL2.dll.a -lkernel32 -luser32 -lgdi32 -lwinspool -lshell32 -lole32 -loleaut32 -luuid -lcomdlg32 -ladvapi32 -o "%EDITOR_EXE%" || (
    popd >nul
    exit /b 1
  )

copy /Y "C:\msys64\mingw64\bin\SDL2.dll" "%ROOT%\build\windows-debug\tools\priory-editor\" >nul
copy /Y "C:\msys64\mingw64\bin\libgcc_s_seh-1.dll" "%ROOT%\build\windows-debug\tools\priory-editor\" >nul
copy /Y "C:\msys64\mingw64\bin\libstdc++-6.dll" "%ROOT%\build\windows-debug\tools\priory-editor\" >nul
copy /Y "C:\msys64\mingw64\bin\libwinpthread-1.dll" "%ROOT%\build\windows-debug\tools\priory-editor\" >nul

echo Priory Editor build complete.
popd >nul
