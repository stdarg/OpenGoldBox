@echo off
setlocal
if "%~1"=="--save-restart" set "APPDATA=%~dp0..\user-data\save-check-profile"
if "%~1"=="--defeat-check" set "APPDATA=%~dp0..\user-data\defeat-check-profile"
if "%~1"=="--expedition-check" set "APPDATA=%~dp0..\user-data\expedition-check-profile"
if "%~1"=="--advancement-check" set "APPDATA=%~dp0..\user-data\advancement-check-profile"
if "%~1"=="--level-up-review" set "APPDATA=%~dp0..\user-data\level-up-review-profile"
rem These views are the game's own; run them in the game project.
set "GAME=%~dp0..\src\OpenGoldBox\godot"
if not exist "%GAME%\bin\opengoldbox_game.dll" (
    echo Build the game first: build-opengoldbox.cmd
    exit /b 1
)
godot --headless --editor --path "%GAME%" --import --quit
if not "%errorlevel%"=="0" exit /b %errorlevel%
if "%~1"=="--save-restart" goto save_restart
if "%~1"=="--defeat-check" goto defeat_check
if "%~1"=="--expedition-check" goto expedition_check
if "%~1"=="--advancement-check" goto advancement_check
if "%~1"=="--level-up-review" goto level_up_review
godot --path "%GAME%" --resolution 1280x900 res://scenes/character_creation.tscn %*
exit /b %errorlevel%

:defeat_check
rem Exercise loss and recovery using isolated saves, then capture the defeat UI.
godot --path "%GAME%" --resolution 1280x900 res://scenes/character_creation.tscn -- --defeat-check --capture
exit /b %errorlevel%

:expedition_check
godot --path "%GAME%" --resolution 1600x1000 res://scenes/character_creation.tscn -- --expedition-check --capture
exit /b %errorlevel%

:save_restart
rem Isolate acceptance saves from the player's normal per-user saves.
set "APPDATA=%~dp0..\user-data\save-check-profile"
godot --headless --path "%GAME%" res://scenes/character_creation.tscn -- --party-check --save-check-write
if not "%errorlevel%"=="0" exit /b %errorlevel%
rem The writer has exited; this is a new process with new native/Godot owners.
godot --headless --path "%GAME%" res://scenes/character_creation.tscn -- --save-check-read
exit /b %errorlevel%

:advancement_check
godot --path "%GAME%" --resolution 1280x900 res://scenes/character_creation.tscn -- --advancement-check --capture
exit /b %errorlevel%

:level_up_review
godot --path "%GAME%" --resolution 1280x900 res://scenes/character_creation.tscn -- --level-up-review
exit /b %errorlevel%
