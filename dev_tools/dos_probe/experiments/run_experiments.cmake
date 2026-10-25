# Runs the probe's experiments against the installed original game; skipped
# (exit 77) without OPENGOLD_GAME_DIR.
if("$ENV{OPENGOLD_GAME_DIR}" STREQUAL "")
    message("Skipped: OPENGOLD_GAME_DIR is not set")
    cmake_language(EXIT 77)
endif()
file(REMOVE_RECURSE "${WORK}")
execute_process(COMMAND "${PYTHON}" -B "${CMAKE_CURRENT_LIST_DIR}/vm_semantics.py" "${PROBE}"
    "$ENV{OPENGOLD_GAME_DIR}" "${WORK}" RESULT_VARIABLE result)
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "The original's ECL results differ from those recorded")
endif()
file(REMOVE_RECURSE "${WORK}")
