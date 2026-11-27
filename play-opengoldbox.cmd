@echo off
rem Build the game without its tests, then start it. Made for a desktop
rem shortcut: the window stays open only when something goes wrong.
setlocal
set "VSDEV_CMD=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\Tools\VsDevCmd.bat"
set "CMAKE_EXE=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if not exist "%VSDEV_CMD%" (
    echo Visual Studio Build Tools were not found. See docs\INSTALL.md.
    goto failed
)
if not exist "%CMAKE_EXE%" (
    echo CMake was not found. See docs\INSTALL.md.
    goto failed
)
call "%VSDEV_CMD%" -arch=x64 >nul 2>nul
if errorlevel 1 goto failed
if not exist "%~dp0build\game\CMakeCache.txt" (
    if exist "%~dp0build\_deps\godot-cpp-src\CMakeLists.txt" (
        "%CMAKE_EXE%" -S "%~dp0." -B "%~dp0build/game" -G Ninja -Werror=dev -Werror=deprecated -DCMAKE_BUILD_TYPE=RelWithDebInfo -DOPENGOLD_BUILD_GAME=ON "-DFETCHCONTENT_SOURCE_DIR_GODOT_CPP=%~dp0build/_deps/godot-cpp-src"
    ) else (
        "%CMAKE_EXE%" -S "%~dp0." -B "%~dp0build/game" -G Ninja -Werror=dev -Werror=deprecated -DCMAKE_BUILD_TYPE=RelWithDebInfo -DOPENGOLD_BUILD_GAME=ON
    )
    if errorlevel 1 goto failed
)
echo Building OpenGoldBox...
"%CMAKE_EXE%" --build "%~dp0build/game" --parallel %NUMBER_OF_PROCESSORS% --target OpenGoldBox
if errorlevel 1 goto failed
start "" "%~dp0win-package\opengoldbox.exe"
exit /b 0

:failed
echo.
echo The game could not be built. The messages above say why.
pause
exit /b 1
