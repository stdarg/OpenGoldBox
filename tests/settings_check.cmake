# A startup or setup check with original files; skipped when OPENGOLD_GAME_DIR
# is not set. Editor builds of Godot keep settings.cfg in the project folder
# (docs/CONFIGURATION.md), so the developer's own file waits in KEPT while the
# check runs and goes back afterwards, also when the check fails. A run that
# was stopped leaves it in KEPT, and the next run puts it back.
#
# Each check starts from settings naming the original folder and English. It
# runs SCRIPT with ARGS, expecting EXPECTED; RESTART_ARGS then runs SCRIPT again
# in a fresh process that reads what the first saved, expecting
# RESTART_EXPECTED. Godot's user data goes to WORK.
if("$ENV{OPENGOLD_GAME_DIR}" STREQUAL "")
    message("Skipped: OPENGOLD_GAME_DIR is not set")
    return()
endif()
set(settings "${PROJECT}/settings.cfg")
if(NOT EXISTS "${KEPT}")
    file(MAKE_DIRECTORY "${KEPT}")
    if(EXISTS "${settings}")
        file(COPY_FILE "${settings}" "${KEPT}/settings.cfg")
    endif()
endif()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/home")
foreach(variable HOME APPDATA XDG_DATA_HOME)
    set(ENV{${variable}} "${WORK}/home")
endforeach()
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

if(EXISTS "${KEPT}/settings.cfg")
    file(COPY_FILE "${KEPT}/settings.cfg" "${settings}")
else()
    file(REMOVE "${settings}")
endif()
file(REMOVE_RECURSE "${KEPT}")
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "Godot check failed: ${SCRIPT}")
endif()
