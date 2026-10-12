@echo off
setlocal
rem These views are the game's own; run them in the game project.
set "GAME=%~dp0..\src\OpenGoldBox\godot"
if not exist "%GAME%\bin\opengoldbox_game.dll" (
    echo Build the game first: build-opengoldbox.cmd
    exit /b 1
)
godot --headless --editor --path "%GAME%" --import --quit
if not "%errorlevel%"=="0" exit /b %errorlevel%
godot --path "%GAME%" --resolution 1280x900 res://scenes/combat_demo.tscn %*
exit /b %errorlevel%
