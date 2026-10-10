# One character-creation screen check with original files; skipped when
# OPENGOLD_GAME_DIR is not set. Godot's user data goes to WORK, so checks do
# not share saves with each other or with the player's own game.
#
# CHECK_FLAG runs a check built into the character-creation scene; otherwise
# SCRIPT names a tests/*.gd check. KEEP_WORK reuses the user data another check
# left in WORK, for a check that reads back what that one saved.
if("$ENV{OPENGOLD_GAME_DIR}" STREQUAL "")
    message("Skipped: OPENGOLD_GAME_DIR is not set")
    return()
endif()
if(NOT KEEP_WORK)
    file(REMOVE_RECURSE "${WORK}")
endif()
file(MAKE_DIRECTORY "${WORK}/home")
foreach(variable HOME APPDATA XDG_DATA_HOME)
    set(ENV{${variable}} "${WORK}/home")
endforeach()
set(ENV{OPENGOLD_LANG} en)

if(DEFINED CHECK_FLAG)
    execute_process(
        COMMAND "${GODOT}" --headless --path "${PROJECT}" res://scenes/character_creation.tscn
        -- ${CHECK_FLAG} ${ARGS}
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 300)
    message("${output}${errors}")
    # EXPECTED is literal text ("C++" would not compile as a regular expression).
    string(FIND "${output}" "${EXPECTED}" expected_at)
    if(NOT result STREQUAL "0" OR expected_at EQUAL -1 OR
            "${output}${errors}" MATCHES "SCRIPT ERROR")
        message(FATAL_ERROR "Godot check failed (${result}): ${CHECK_FLAG}")
    endif()
else()
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DGODOT=${GODOT}" "-DPROJECT=${PROJECT}"
        "-DSCRIPT=${CMAKE_CURRENT_LIST_DIR}/${SCRIPT}.gd" "-DEXPECTED=${EXPECTED}"
        "-DARGS=${ARGS}" "-DGRAPHICAL=${GRAPHICAL}" -DTEST_TIMEOUT=300
        -P "${CMAKE_CURRENT_LIST_DIR}/run_godot_test.cmake"
        RESULT_VARIABLE result)
    if(NOT result STREQUAL "0")
        message(FATAL_ERROR "Godot check failed: ${SCRIPT}")
    endif()
endif()
