@echo off
setlocal
set "VSDEV_CMD=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\Tools\VsDevCmd.bat"
set "CMAKE_EXE=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if not exist "%VSDEV_CMD%" (
    echo Visual Studio Build Tools were not found. See docs\INSTALL.md.
    exit /b 1
)
if not exist "%CMAKE_EXE%" (
    echo CMake was not found. See docs\INSTALL.md.
    exit /b 1
)
call "%VSDEV_CMD%" -arch=x64
if not "%errorlevel%"=="0" exit /b %errorlevel%
if exist "%~dp0build\_deps\godot-cpp-src\CMakeLists.txt" (
    "%CMAKE_EXE%" -S "%~dp0." -B "%~dp0build/game" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DOPENGOLD_BUILD_GAME=ON "-DFETCHCONTENT_SOURCE_DIR_GODOT_CPP=%~dp0build/_deps/godot-cpp-src" %*
) else (
    "%CMAKE_EXE%" -S "%~dp0." -B "%~dp0build/game" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DOPENGOLD_BUILD_GAME=ON %*
)
if not "%errorlevel%"=="0" exit /b %errorlevel%
"%CMAKE_EXE%" --build "%~dp0build/game" --parallel 6
if not "%errorlevel%"=="0" exit /b %errorlevel%
"%CMAKE_EXE%" -E chdir "%~dp0build/game" ctest --output-on-failure -j %NUMBER_OF_PROCESSORS%
if not "%errorlevel%"=="0" exit /b %errorlevel%
echo Game package ready: "%~dp0win-package\opengoldbox.exe"
exit /b 0
