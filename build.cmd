@echo off
setlocal

rem Build this checkout even when called from another directory or worktree.
cd /d "%~dp0"

rem "build.cmd debug" runs the slower Debug preset; the default is optimized.
set "PRESET=%~1"
if "%PRESET%"=="" set "PRESET=default"

set "VSDEV_CMD=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\Tools\VsDevCmd.bat"
set "CMAKE_EXE=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

if not exist "%VSDEV_CMD%" goto missing_vs
if not exist "%CMAKE_EXE%" goto missing_cmake

call "%VSDEV_CMD%" -arch=x64
if errorlevel 1 exit /b %errorlevel%

"%CMAKE_EXE%" --preset %PRESET%
if errorlevel 1 exit /b %errorlevel%

"%CMAKE_EXE%" --build --preset %PRESET%
if errorlevel 1 exit /b %errorlevel%

ctest --preset %PRESET% -j %NUMBER_OF_PROCESSORS%
exit /b %errorlevel%

:missing_vs
echo Visual Studio Build Tools environment was not found:
echo %VSDEV_CMD%
exit /b 1

:missing_cmake
echo CMake was not found:
echo %CMAKE_EXE%
exit /b 1
