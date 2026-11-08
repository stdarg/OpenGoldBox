# A startup or setup check with original files; skipped when OPENGOLD_GAME_DIR
# is not set. Its settings.cfg is WORK's own (OPENGOLD_SETTINGS_FILE), so the
# developer's file in the project folder is never touched.
#
# Each check starts from settings naming the original folder and English. It
# runs SCRIPT with ARGS, expecting EXPECTED; RESTART_ARGS then runs SCRIPT again
# in a fresh process that reads what the first saved, expecting
# RESTART_EXPECTED. Godot's user data goes to WORK.
if("$ENV{OPENGOLD_GAME_DIR}" STREQUAL "")
    message("Skipped: OPENGOLD_GAME_DIR is not set")
    return()
endif()
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/home")
foreach(variable HOME APPDATA XDG_DATA_HOME)
    set(ENV{${variable}} "${WORK}/home")
endforeach()
set(settings "${WORK}/home/settings.cfg")
set(ENV{OPENGOLD_SETTINGS_FILE} "${settings}")
# A language override would take the place of the saved language these checks test.
unset(ENV{OPENGOLD_LANG})
file(TO_CMAKE_PATH "$ENV{OPENGOLD_GAME_DIR}" game_folder)
file(WRITE "${settings}" "[game]\npath=\"${game_folder}\"\n\n[interface]\nlanguage=\"en\"\n")

function(run_check arguments expected)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DGODOT=${GODOT}" "-DPROJECT=${PROJECT}"
        "-DSCRIPT=${SCRIPT}" "-DEXPECTED=${expected}" "-DARGS=${arguments}"
        -DTEST_TIMEOUT=150 -P "${CMAKE_CURRENT_LIST_DIR}/run_godot_test.cmake"
        RESULT_VARIABLE result)
    set(result "${result}" PARENT_SCOPE)
endfunction()

run_check("${ARGS}" "${EXPECTED}")
if(result STREQUAL "0" AND DEFINED RESTART_ARGS)
    run_check("${RESTART_ARGS}" "${RESTART_EXPECTED}")
endif()

if(NOT result STREQUAL "0")
    message(FATAL_ERROR "Godot check failed: ${SCRIPT}")
endif()
