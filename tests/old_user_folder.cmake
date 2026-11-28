# Windows: the game moves %APPDATA%\Godot\app_userdata\OpenGold into
# %APPDATA%\OpenGold at startup, keeping any file the new folder already has,
# and removes the old folder once it is empty. APPDATA is WORK's own.
file(REMOVE_RECURSE "${WORK}")
set(home "${WORK}/home")
set(old "${home}/Godot/app_userdata/OpenGold")
set(new "${home}/OpenGold")
foreach(variable HOME APPDATA XDG_DATA_HOME)
    set(ENV{${variable}} "${home}")
endforeach()
unset(ENV{OPENGOLD_SETTINGS_FILE})

file(WRITE "${old}/settings.cfg" "[interface]\nlanguage=\"es\"\n")
file(WRITE "${old}/saves/moved.ogs" "old save")
file(WRITE "${old}/both.txt" "old")
file(WRITE "${new}/both.txt" "new")
file(WRITE "${new}/saves/kept.ogs" "new save")

function(run_game)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DGODOT=${GODOT}" "-DPROJECT=${PROJECT}"
        "-DSCRIPT=${SCRIPT}" "-DEXPECTED=Old user folder checks passed"
        -P "${CMAKE_CURRENT_LIST_DIR}/run_godot_test.cmake"
        RESULT_VARIABLE result)
    if(NOT result STREQUAL "0")
        message(FATAL_ERROR "The game did not start")
    endif()
endfunction()

function(require_text path expected)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Missing ${path}")
    endif()
    file(READ "${path}" text)
    if(NOT text MATCHES "${expected}")
        message(FATAL_ERROR "${path} holds \"${text}\", not \"${expected}\"")
    endif()
endfunction()

run_game()
require_text("${new}/settings.cfg" "language=\"es\"")
require_text("${new}/saves/moved.ogs" "old save")
require_text("${new}/saves/kept.ogs" "new save")
require_text("${new}/both.txt" "new")
# The file both folders had stays in the old one, so nothing is lost.
require_text("${old}/both.txt" "old")
if(EXISTS "${old}/settings.cfg" OR EXISTS "${old}/saves")
    message(FATAL_ERROR "The old folder still holds what was moved")
endif()

file(REMOVE "${old}/both.txt")
run_game()
if(EXISTS "${home}/Godot")
    message(FATAL_ERROR "The empty old folder was not removed")
endif()
message("Old user folder moved")
