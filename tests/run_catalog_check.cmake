# Runs one of the Combat Sprite Catalog's own checks, which its scene starts from
# a command-line flag. They read the original game's files. A failed assert leaves
# Godot running, so the timeout ends the check and the output decides it.
if("$ENV{OPENGOLD_GAME_DIR}" STREQUAL "")
    message("Skipped: OPENGOLD_GAME_DIR is not set")
    return()
endif()
file(REMOVE_RECURSE "${USER_HOME}")
file(MAKE_DIRECTORY "${USER_HOME}")
foreach(variable HOME APPDATA XDG_DATA_HOME)
    set(ENV{${variable}} "${USER_HOME}")
endforeach()
execute_process(
    COMMAND "${GODOT}" --headless --path "${PROJECT}" -- "${CHECK}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 120)
message("${output}${errors}")
if(NOT result STREQUAL "0" OR NOT output MATCHES "${EXPECTED}" OR
        "${output}${errors}" MATCHES "SCRIPT ERROR|Assertion failed")
    message(FATAL_ERROR "Catalog check failed (${result}): ${CHECK}")
endif()
