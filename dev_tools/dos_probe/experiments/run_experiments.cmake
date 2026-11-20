# Runs the probe's experiments against the installed original game, found as
# the game finds it (GAME_FINDER); skipped (exit 77) without its files.
include("${CMAKE_CURRENT_LIST_DIR}/../../../tests/find_games.cmake")
if(NOT DEFINED game_folder_POOLRAD)
    message("Skipped: Pool of Radiance's files were not found")
    cmake_language(EXIT 77)
endif()
file(REMOVE_RECURSE "${WORK}")
execute_process(COMMAND "${PYTHON}" -B "${CMAKE_CURRENT_LIST_DIR}/vm_semantics.py" "${PROBE}"
    "${game_folder_POOLRAD}" "${WORK}" RESULT_VARIABLE result)
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "The original's ECL results differ from those recorded")
endif()
execute_process(COMMAND "${PYTHON}" -B "${CMAKE_CURRENT_LIST_DIR}/battlefield.py" "${PROBE}"
    "${MAPS}" "${game_folder_POOLRAD}" "${WORK}/battlefield" RESULT_VARIABLE result)
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "The original's fights start where OpenGoldBox's do not")
endif()
execute_process(COMMAND "${PYTHON}" -B "${CMAKE_CURRENT_LIST_DIR}/wilderness.py" "${PROBE}"
    "${MAPS}" "${game_folder_POOLRAD}" "${WORK}/wilderness" RESULT_VARIABLE result)
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "The original's wilderness fights start where OpenGoldBox's do not")
endif()
execute_process(COMMAND "${PYTHON}" -B "${CMAKE_CURRENT_LIST_DIR}/wilderness_field.py" "${PROBE}"
    "${MAPS}" "${game_folder_POOLRAD}" "${WORK}/wilderness_field" RESULT_VARIABLE result)
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "The original's wilderness fields differ from OpenGoldBox's")
endif()
file(REMOVE_RECURSE "${WORK}")
