@echo off
setlocal

set ROOT=%~dp0..
set BUILD=%ROOT%\build\windows-debug
set MINGW=C:\msys64\mingw64\bin
set PATH=%MINGW%;%PATH%

cd /d "%BUILD%"

if not exist "launcher\CMakeFiles\pixelpal-launcher.dir\src" mkdir "launcher\CMakeFiles\pixelpal-launcher.dir\src"
if not exist "sample-games\glitch-grid\CMakeFiles\glitchgrid.dir" mkdir "sample-games\glitch-grid\CMakeFiles\glitchgrid.dir"
if not exist "launcher" mkdir "launcher"
if not exist "sample-games\glitch-grid" mkdir "sample-games\glitch-grid"

echo [1/8] Compiling launcher catalog.cpp
"%MINGW%\c++.exe" -DSDL_MAIN_HANDLED -I"%ROOT%\launcher\include" -I"%BUILD%\launcher\generated" -isystem C:/msys64/mingw64/include/SDL2 -g -std=c++20 -MD -MT launcher/CMakeFiles/pixelpal-launcher.dir/src/catalog.cpp.obj -MF launcher\CMakeFiles\pixelpal-launcher.dir\src\catalog.cpp.obj.d -o launcher/CMakeFiles/pixelpal-launcher.dir/src/catalog.cpp.obj -c "%ROOT%\launcher\src\catalog.cpp"
if errorlevel 1 exit /b %errorlevel%

echo [2/8] Compiling launcher main.cpp
"%MINGW%\c++.exe" -DSDL_MAIN_HANDLED -I"%ROOT%\launcher\include" -I"%BUILD%\launcher\generated" -isystem C:/msys64/mingw64/include/SDL2 -g -std=c++20 -MD -MT launcher/CMakeFiles/pixelpal-launcher.dir/src/main.cpp.obj -MF launcher\CMakeFiles\pixelpal-launcher.dir\src\main.cpp.obj.d -o launcher/CMakeFiles/pixelpal-launcher.dir/src/main.cpp.obj -c "%ROOT%\launcher\src\main.cpp"
if errorlevel 1 exit /b %errorlevel%

echo [3/8] Compiling launcher menu_audio.cpp
"%MINGW%\c++.exe" -DSDL_MAIN_HANDLED -I"%ROOT%\launcher\include" -I"%BUILD%\launcher\generated" -isystem C:/msys64/mingw64/include/SDL2 -g -std=c++20 -MD -MT launcher/CMakeFiles/pixelpal-launcher.dir/src/menu_audio.cpp.obj -MF launcher\CMakeFiles\pixelpal-launcher.dir\src\menu_audio.cpp.obj.d -o launcher/CMakeFiles/pixelpal-launcher.dir/src/menu_audio.cpp.obj -c "%ROOT%\launcher\src\menu_audio.cpp"
if errorlevel 1 exit /b %errorlevel%

echo [4/8] Compiling launcher status_snapshot.cpp
"%MINGW%\c++.exe" -DSDL_MAIN_HANDLED -I"%ROOT%\launcher\include" -I"%BUILD%\launcher\generated" -isystem C:/msys64/mingw64/include/SDL2 -g -std=c++20 -MD -MT launcher/CMakeFiles/pixelpal-launcher.dir/src/status_snapshot.cpp.obj -MF launcher\CMakeFiles\pixelpal-launcher.dir\src\status_snapshot.cpp.obj.d -o launcher/CMakeFiles/pixelpal-launcher.dir/src/status_snapshot.cpp.obj -c "%ROOT%\launcher\src\status_snapshot.cpp"
if errorlevel 1 exit /b %errorlevel%

echo [5/8] Linking launcher
"%MINGW%\c++.exe" -g launcher/CMakeFiles/pixelpal-launcher.dir/src/catalog.cpp.obj launcher/CMakeFiles/pixelpal-launcher.dir/src/main.cpp.obj launcher/CMakeFiles/pixelpal-launcher.dir/src/menu_audio.cpp.obj launcher/CMakeFiles/pixelpal-launcher.dir/src/status_snapshot.cpp.obj -o launcher\pixelpal-launcher.exe -Wl,--out-implib,launcher\libpixelpal-launcher.dll.a -Wl,--major-image-version,0,--minor-image-version,0 C:/msys64/mingw64/lib/libSDL2.dll.a -lkernel32 -luser32 -lgdi32 -lwinspool -lshell32 -lole32 -loleaut32 -luuid -lcomdlg32 -ladvapi32
if errorlevel 1 exit /b %errorlevel%
"%MINGW%\cmake.exe" -E copy_if_different "%MINGW%\SDL2.dll" "%BUILD%\launcher"
"%MINGW%\cmake.exe" -E copy_if_different "%MINGW%\libgcc_s_seh-1.dll" "%BUILD%\launcher"
"%MINGW%\cmake.exe" -E copy_if_different "%MINGW%\libstdc++-6.dll" "%BUILD%\launcher"
"%MINGW%\cmake.exe" -E copy_if_different "%MINGW%\libwinpthread-1.dll" "%BUILD%\launcher"
"%MINGW%\cmake.exe" -E copy_directory "%ROOT%\themes\default" "%BUILD%\themes\default"
if errorlevel 1 exit /b %errorlevel%

echo [6/8] Compiling glitchgrid
"%MINGW%\cc.exe" -DSDL_MAIN_HANDLED -I"%ROOT%\sdk\include" -isystem C:/msys64/mingw64/include/SDL2 -g -std=c11 -MD -MT sample-games/glitch-grid/CMakeFiles/glitchgrid.dir/main.c.obj -MF sample-games\glitch-grid\CMakeFiles\glitchgrid.dir\main.c.obj.d -o sample-games/glitch-grid/CMakeFiles/glitchgrid.dir/main.c.obj -c "%ROOT%\sample-games\glitch-grid\main.c"
if errorlevel 1 exit /b %errorlevel%

echo [7/8] Rebuilding sdk static library
"%MINGW%\cc.exe" -I"%ROOT%\sdk\include" -isystem C:/msys64/mingw64/include/SDL2 -g -std=c11 -MD -MT sdk/CMakeFiles/pixelpal.dir/src/pixelpal.c.obj -MF sdk\CMakeFiles\pixelpal.dir\src\pixelpal.c.obj.d -o sdk/CMakeFiles/pixelpal.dir/src/pixelpal.c.obj -c "%ROOT%\sdk\src\pixelpal.c"
if errorlevel 1 exit /b %errorlevel%
"%MINGW%\cmake.exe" -E rm -f sdk\libpixelpal.a
"%MINGW%\ar.exe" qc sdk\libpixelpal.a sdk/CMakeFiles/pixelpal.dir/src/pixelpal.c.obj
"%MINGW%\ranlib.exe" sdk\libpixelpal.a
if errorlevel 1 exit /b %errorlevel%

echo [8/8] Linking and staging glitchgrid
"%MINGW%\cc.exe" -g sample-games/glitch-grid/CMakeFiles/glitchgrid.dir/main.c.obj -o sample-games\glitch-grid\glitchgrid.exe -Wl,--out-implib,sample-games\glitch-grid\libglitchgrid.dll.a -Wl,--major-image-version,0,--minor-image-version,0 sdk/libpixelpal.a C:/msys64/mingw64/lib/libSDL2.dll.a C:/msys64/mingw64/lib/libSDL2.dll.a -lkernel32 -luser32 -lgdi32 -lwinspool -lshell32 -lole32 -loleaut32 -luuid -lcomdlg32 -ladvapi32
if errorlevel 1 exit /b %errorlevel%
"%MINGW%\cmake.exe" -E copy_if_different "%MINGW%\SDL2.dll" "%BUILD%\sample-games\glitch-grid"
"%MINGW%\cmake.exe" -E copy_if_different "%MINGW%\libgcc_s_seh-1.dll" "%BUILD%\sample-games\glitch-grid"
"%MINGW%\cmake.exe" -E copy_if_different "%MINGW%\libstdc++-6.dll" "%BUILD%\sample-games\glitch-grid"
"%MINGW%\cmake.exe" -E copy_if_different "%MINGW%\libwinpthread-1.dll" "%BUILD%\sample-games\glitch-grid"
"%MINGW%\cmake.exe" -E make_directory "%BUILD%\games\glitch-grid\bin"
"%MINGW%\cmake.exe" -E copy_if_different "%ROOT%\sample-games\glitch-grid\manifest.toml" "%BUILD%\games\glitch-grid"
"%MINGW%\cmake.exe" -E copy_if_different "%ROOT%\sample-games\glitch-grid\icon.ppm" "%BUILD%\games\glitch-grid"
"%MINGW%\cmake.exe" -E copy_if_different "%ROOT%\sample-games\glitch-grid\splash.ppm" "%BUILD%\games\glitch-grid"
"%MINGW%\cmake.exe" -E copy_if_different "%BUILD%\sample-games\glitch-grid\glitchgrid.exe" "%BUILD%\games\glitch-grid\bin"
"%MINGW%\cmake.exe" -E copy_if_different "%BUILD%\sample-games\glitch-grid\SDL2.dll" "%BUILD%\games\glitch-grid\bin"
"%MINGW%\cmake.exe" -E copy_if_different "%BUILD%\sample-games\glitch-grid\libgcc_s_seh-1.dll" "%BUILD%\games\glitch-grid\bin"
"%MINGW%\cmake.exe" -E copy_if_different "%BUILD%\sample-games\glitch-grid\libstdc++-6.dll" "%BUILD%\games\glitch-grid\bin"
"%MINGW%\cmake.exe" -E copy_if_different "%BUILD%\sample-games\glitch-grid\libwinpthread-1.dll" "%BUILD%\games\glitch-grid\bin"
if errorlevel 1 exit /b %errorlevel%

echo Done.
exit /b 0
