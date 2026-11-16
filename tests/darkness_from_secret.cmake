# Pools of Darkness's characters from Secret of the Silver Blades on the party
# screen (darkness_from_secret_tests.gd). Skipped unless OPENGOLD_GAME_DIR
# holds Pool of Radiance and OPENGOLD_GOLDBOX_DIR holds the DARKNESS and
# SECRET folders.
#
# FIXTURE_WRITER (opengold_darkness_tests) writes the Secret save the check
# brings a character from; the check runs with its own settings.cfg and user
# data under WORK.
if("$ENV{OPENGOLD_GAME_DIR}" STREQUAL "" OR NOT EXISTS "$ENV{OPENGOLD_GOLDBOX_DIR}/DARKNESS"
   OR NOT EXISTS "$ENV{OPENGOLD_GOLDBOX_DIR}/SECRET")
    message("Skipped: OPENGOLD_GAME_DIR or OPENGOLD_GOLDBOX_DIR's DARKNESS or SECRET folder is not set")
    return()
endif()
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/home")
foreach(variable HOME APPDATA XDG_DATA_HOME)
    set(ENV{${variable}} "${WORK}/home")
endforeach()
set(settings "${WORK}/home/settings.cfg")
set(ENV{OPENGOLD_SETTINGS_FILE} "${settings}")
set(ENV{OPENGOLD_LANG} en)
file(TO_CMAKE_PATH "$ENV{OPENGOLD_GAME_DIR}" game_folder)
file(WRITE "${settings}"
     "[game]\npath=\"${game_folder}\"\n\n[interface]\nlanguage=\"en\"\n")

set(fixture "${WORK}/secret-veterans.ogs")
set(ENV{OPENGOLD_SECRET_SAVE_FIXTURE} "${fixture}")
execute_process(COMMAND "${FIXTURE_WRITER}" RESULT_VARIABLE result)
unset(ENV{OPENGOLD_SECRET_SAVE_FIXTURE})
if(NOT result STREQUAL "0" OR NOT EXISTS "${fixture}")
    message(FATAL_ERROR "The Secret save was not written (${result})")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DGODOT=${GODOT}" "-DPROJECT=${PROJECT}"
    "-DSCRIPT=${CMAKE_CURRENT_LIST_DIR}/darkness_from_secret_tests.gd" "-DEXPECTED=${EXPECTED}"
    "-DARGS=--secret-save=${fixture}" -DTEST_TIMEOUT=200
    -P "${CMAKE_CURRENT_LIST_DIR}/run_godot_test.cmake"
    RESULT_VARIABLE result)
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "Godot check failed: darkness_from_secret_tests")
endif()
