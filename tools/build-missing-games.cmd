@echo off
setlocal

set ROOT=%~dp0..
set BUILD=%ROOT%\build\windows-debug
set MINGW=C:\msys64\mingw64\bin
set PATH=%MINGW%;%PATH%

if not exist "%BUILD%" (
  echo Build directory not found: %BUILD%
  exit /b 1
)

cd /d "%BUILD%"

if not exist "sample-games\kilobytes\CMakeFiles\kilobytes.dir" mkdir "sample-games\kilobytes\CMakeFiles\kilobytes.dir"
if not exist "sample-games\pixelpal-checkers\CMakeFiles\pixelpal-checkers.dir" mkdir "sample-games\pixelpal-checkers\CMakeFiles\pixelpal-checkers.dir"
if not exist "sample-games\pixelpal-chess\CMakeFiles\pixelpal-chess.dir" mkdir "sample-games\pixelpal-chess\CMakeFiles\pixelpal-chess.dir"
if not exist "sample-games\kilobytes" mkdir "sample-games\kilobytes"
if not exist "sample-games\pixelpal-checkers" mkdir "sample-games\pixelpal-checkers"
if not exist "sample-games\pixelpal-chess" mkdir "sample-games\pixelpal-chess"

echo [1/9] Compiling sdk\pixelpal.c
"%MINGW%\cc.exe" -I"%ROOT%\sdk\include" -isystem C:/msys64/mingw64/include/SDL2 -g -std=c11 -MD -MT sdk/CMakeFiles/pixelpal.dir/src/pixelpal.c.obj -MF sdk\CMakeFiles\pixelpal.dir\src\pixelpal.c.obj.d -o sdk/CMakeFiles/pixelpal.dir/src/pixelpal.c.obj -c "%ROOT%\sdk\src\pixelpal.c"
if errorlevel 1 exit /b %errorlevel%

echo [2/9] Archiving sdk\libpixelpal.a
"%MINGW%\cmake.exe" -E rm -f sdk\libpixelpal.a
"%MINGW%\ar.exe" qc sdk\libpixelpal.a sdk/CMakeFiles/pixelpal.dir/src/pixelpal.c.obj
if errorlevel 1 exit /b %errorlevel%
"%MINGW%\ranlib.exe" sdk\libpixelpal.a
if errorlevel 1 exit /b %errorlevel%

echo [3/9] Compiling kilobytes
"%MINGW%\cc.exe" -DSDL_MAIN_HANDLED -I"%ROOT%\sdk\include" -isystem C:/msys64/mingw64/include/SDL2 -g -std=c11 -MD -MT sample-games/kilobytes/CMakeFiles/kilobytes.dir/main.c.obj -MF sample-games\kilobytes\CMakeFiles\kilobytes.dir\main.c.obj.d -o sample-games/kilobytes/CMakeFiles/kilobytes.dir/main.c.obj -c "%ROOT%\sample-games\kilobytes\main.c"
if errorlevel 1 exit /b %errorlevel%

echo [4/9] Linking kilobytes
"%MINGW%\cc.exe" -g sample-games/kilobytes/CMakeFiles/kilobytes.dir/main.c.obj -o sample-games\kilobytes\kilobytes.exe -Wl,--out-implib,sample-games\kilobytes\libkilobytes.dll.a -Wl,--major-image-version,0,--minor-image-version,0 sdk/libpixelpal.a C:/msys64/mingw64/lib/libSDL2.dll.a C:/msys64/mingw64/lib/libSDL2.dll.a -lkernel32 -luser32 -lgdi32 -lwinspool -lshell32 -lole32 -loleaut32 -luuid -lcomdlg32 -ladvapi32
if errorlevel 1 exit /b %errorlevel%

echo [5/9] Staging kilobytes
"%MINGW%\cmake.exe" -E copy_if_different "%MINGW%\SDL2.dll" "%BUILD%\sample-games\kilobytes"
"%MINGW%\cmake.exe" -E copy_if_different "%MINGW%\libgcc_s_seh-1.dll" "%BUILD%\sample-games\kilobytes"
"%MINGW%\cmake.exe" -E copy_if_different "%MINGW%\libstdc++-6.dll" "%BUILD%\sample-games\kilobytes"
"%MINGW%\cmake.exe" -E copy_if_different "%MINGW%\libwinpthread-1.dll" "%BUILD%\sample-games\kilobytes"
"%MINGW%\cmake.exe" -E make_directory "%BUILD%\games\kilobytes\bin"
"%MINGW%\cmake.exe" -E copy_if_different "%ROOT%\sample-games\kilobytes\manifest.toml" "%BUILD%\games\kilobytes"
"%MINGW%\cmake.exe" -E copy_if_different "%ROOT%\sample-games\kilobytes\icon.ppm" "%BUILD%\games\kilobytes"
"%MINGW%\cmake.exe" -E copy_if_different "%ROOT%\sample-games\kilobytes\splash.ppm" "%BUILD%\games\kilobytes"
"%MINGW%\cmake.exe" -E copy_if_different "%BUILD%\sample-games\kilobytes\kilobytes.exe" "%BUILD%\games\kilobytes\bin"
"%MINGW%\cmake.exe" -E copy_if_different "%BUILD%\sample-games\kilobytes\SDL2.dll" "%BUILD%\games\kilobytes\bin"
"%MINGW%\cmake.exe" -E copy_if_different "%BUILD%\sample-games\kilobytes\libgcc_s_seh-1.dll" "%BUILD%\games\kilobytes\bin"
"%MINGW%\cmake.exe" -E copy_if_different "%BUILD%\sample-games\kilobytes\libstdc++-6.dll" "%BUILD%\games\kilobytes\bin"
"%MINGW%\cmake.exe" -E copy_if_different "%BUILD%\sample-games\kilobytes\libwinpthread-1.dll" "%BUILD%\games\kilobytes\bin"
if errorlevel 1 exit /b %errorlevel%

echo [6/9] Compiling pixelpal-checkers
"%MINGW%\c++.exe" -DSDL_MAIN_HANDLED -I"%ROOT%\sdk\include" -isystem C:/msys64/mingw64/include/SDL2 -g -std=c++20 -MD -MT sample-games/pixelpal-checkers/CMakeFiles/pixelpal-checkers.dir/main.cpp.obj -MF sample-games\pixelpal-checkers\CMakeFiles\pixelpal-checkers.dir\main.cpp.obj.d -o sample-games/pixelpal-checkers/CMakeFiles/pixelpal-checkers.dir/main.cpp.obj -c "%ROOT%\sample-games\pixelpal-checkers\main.cpp"
if errorlevel 1 exit /b %errorlevel%

echo [7/9] Linking pixelpal-checkers
"%MINGW%\c++.exe" -g sample-games/pixelpal-checkers/CMakeFiles/pixelpal-checkers.dir/main.cpp.obj -o sample-games\pixelpal-checkers\pixelpal-checkers.exe -Wl,--out-implib,sample-games\pixelpal-checkers\libpixelpal-checkers.dll.a -Wl,--major-image-version,0,--minor-image-version,0 sdk/libpixelpal.a C:/msys64/mingw64/lib/libSDL2.dll.a C:/msys64/mingw64/lib/libSDL2.dll.a -lkernel32 -luser32 -lgdi32 -lwinspool -lshell32 -lole32 -loleaut32 -luuid -lcomdlg32 -ladvapi32
if errorlevel 1 exit /b %errorlevel%

echo [8/9] Staging pixelpal-checkers
"%MINGW%\cmake.exe" -E copy_if_different "%MINGW%\SDL2.dll" "%BUILD%\sample-games\pixelpal-checkers"
"%MINGW%\cmake.exe" -E copy_if_different "%MINGW%\libgcc_s_seh-1.dll" "%BUILD%\sample-games\pixelpal-checkers"
"%MINGW%\cmake.exe" -E copy_if_different "%MINGW%\libstdc++-6.dll" "%BUILD%\sample-games\pixelpal-checkers"
"%MINGW%\cmake.exe" -E copy_if_different "%MINGW%\libwinpthread-1.dll" "%BUILD%\sample-games\pixelpal-checkers"
"%MINGW%\cmake.exe" -E make_directory "%BUILD%\games\pixelpal-checkers\bin"
"%MINGW%\cmake.exe" -E copy_if_different "%ROOT%\sample-games\pixelpal-checkers\manifest.toml" "%BUILD%\games\pixelpal-checkers"
"%MINGW%\cmake.exe" -E copy_if_different "%ROOT%\sample-games\pixelpal-checkers\icon.ppm" "%BUILD%\games\pixelpal-checkers"
"%MINGW%\cmake.exe" -E copy_if_different "%ROOT%\sample-games\pixelpal-checkers\splash.ppm" "%BUILD%\games\pixelpal-checkers"
"%MINGW%\cmake.exe" -E copy_if_different "%BUILD%\sample-games\pixelpal-checkers\pixelpal-checkers.exe" "%BUILD%\games\pixelpal-checkers\bin"
"%MINGW%\cmake.exe" -E copy_if_different "%BUILD%\sample-games\pixelpal-checkers\SDL2.dll" "%BUILD%\games\pixelpal-checkers\bin"
"%MINGW%\cmake.exe" -E copy_if_different "%BUILD%\sample-games\pixelpal-checkers\libgcc_s_seh-1.dll" "%BUILD%\games\pixelpal-checkers\bin"
"%MINGW%\cmake.exe" -E copy_if_different "%BUILD%\sample-games\pixelpal-checkers\libstdc++-6.dll" "%BUILD%\games\pixelpal-checkers\bin"
"%MINGW%\cmake.exe" -E copy_if_different "%BUILD%\sample-games\pixelpal-checkers\libwinpthread-1.dll" "%BUILD%\games\pixelpal-checkers\bin"
if errorlevel 1 exit /b %errorlevel%

echo [9/9] Compiling pixelpal-chess
"%MINGW%\c++.exe" -DSDL_MAIN_HANDLED -I"%ROOT%\sdk\include" -isystem C:/msys64/mingw64/include/SDL2 -g -std=c++20 -MD -MT sample-games/pixelpal-chess/CMakeFiles/pixelpal-chess.dir/main.cpp.obj -MF sample-games\pixelpal-chess\CMakeFiles\pixelpal-chess.dir\main.cpp.obj.d -o sample-games/pixelpal-chess/CMakeFiles/pixelpal-chess.dir/main.cpp.obj -c "%ROOT%\sample-games\pixelpal-chess\main.cpp"
if errorlevel 1 exit /b %errorlevel%

echo [10/10] Linking and staging pixelpal-chess
"%MINGW%\c++.exe" -g sample-games/pixelpal-chess/CMakeFiles/pixelpal-chess.dir/main.cpp.obj -o sample-games\pixelpal-chess\pixelpal-chess.exe -Wl,--out-implib,sample-games\pixelpal-chess\libpixelpal-chess.dll.a -Wl,--major-image-version,0,--minor-image-version,0 sdk/libpixelpal.a C:/msys64/mingw64/lib/libSDL2.dll.a C:/msys64/mingw64/lib/libSDL2.dll.a -lkernel32 -luser32 -lgdi32 -lwinspool -lshell32 -lole32 -loleaut32 -luuid -lcomdlg32 -ladvapi32
if errorlevel 1 exit /b %errorlevel%
"%MINGW%\cmake.exe" -E copy_if_different "%MINGW%\SDL2.dll" "%BUILD%\sample-games\pixelpal-chess"
"%MINGW%\cmake.exe" -E copy_if_different "%MINGW%\libgcc_s_seh-1.dll" "%BUILD%\sample-games\pixelpal-chess"
"%MINGW%\cmake.exe" -E copy_if_different "%MINGW%\libstdc++-6.dll" "%BUILD%\sample-games\pixelpal-chess"
"%MINGW%\cmake.exe" -E copy_if_different "%MINGW%\libwinpthread-1.dll" "%BUILD%\sample-games\pixelpal-chess"
"%MINGW%\cmake.exe" -E make_directory "%BUILD%\games\pixelpal-chess\bin"
"%MINGW%\cmake.exe" -E copy_if_different "%ROOT%\sample-games\pixelpal-chess\manifest.toml" "%BUILD%\games\pixelpal-chess"
"%MINGW%\cmake.exe" -E copy_if_different "%ROOT%\sample-games\pixelpal-chess\icon.ppm" "%BUILD%\games\pixelpal-chess"
"%MINGW%\cmake.exe" -E copy_if_different "%ROOT%\sample-games\pixelpal-chess\splash.ppm" "%BUILD%\games\pixelpal-chess"
"%MINGW%\cmake.exe" -E copy_if_different "%BUILD%\sample-games\pixelpal-chess\pixelpal-chess.exe" "%BUILD%\games\pixelpal-chess\bin"
"%MINGW%\cmake.exe" -E copy_if_different "%BUILD%\sample-games\pixelpal-chess\SDL2.dll" "%BUILD%\games\pixelpal-chess\bin"
"%MINGW%\cmake.exe" -E copy_if_different "%BUILD%\sample-games\pixelpal-chess\libgcc_s_seh-1.dll" "%BUILD%\games\pixelpal-chess\bin"
"%MINGW%\cmake.exe" -E copy_if_different "%BUILD%\sample-games\pixelpal-chess\libstdc++-6.dll" "%BUILD%\games\pixelpal-chess\bin"
"%MINGW%\cmake.exe" -E copy_if_different "%BUILD%\sample-games\pixelpal-chess\libwinpthread-1.dll" "%BUILD%\games\pixelpal-chess\bin"
if errorlevel 1 exit /b %errorlevel%

echo Done.
exit /b 0
