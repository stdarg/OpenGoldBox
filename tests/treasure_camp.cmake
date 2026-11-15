# Treasures' camp dialog: its Fix and Crown (treasure_camp_tests.gd). Skipped
# unless OPENGOLD_GAME_DIR holds Pool of Radiance and OPENGOLD_GOLDBOX_DIR
# holds the TREASURE folder.
#
# FIXTURE_WRITER (opengold_treasure_tests) writes the save of a party in the
# fortress, a member wounded and the crown held; the check runs with its own
# settings.cfg and user data under WORK.
if("$ENV{OPENGOLD_GAME_DIR}" STREQUAL "" OR NOT EXISTS "$ENV{OPENGOLD_GOLDBOX_DIR}/TREASURE")
    message("Skipped: OPENGOLD_GAME_DIR or OPENGOLD_GOLDBOX_DIR's TREASURE folder is not set")
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
file(WRITE "${settings}" "[game]\npath=\"${game_folder}\"\n\n[interface]\nlanguage=\"en\"\n")

set(fixture "${WORK}/treasure-camp.ogs")
set(ENV{OPENGOLD_TREASURE_CAMP_FIXTURE} "${fixture}")
execute_process(COMMAND "${FIXTURE_WRITER}" RESULT_VARIABLE result)
unset(ENV{OPENGOLD_TREASURE_CAMP_FIXTURE})
if(NOT result STREQUAL "0" OR NOT EXISTS "${fixture}")
    message(FATAL_ERROR "The Treasures camp save was not written (${result})")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DGODOT=${GODOT}" "-DPROJECT=${PROJECT}"
    "-DSCRIPT=${CMAKE_CURRENT_LIST_DIR}/treasure_camp_tests.gd" "-DEXPECTED=${EXPECTED}"
    "-DARGS=--camp-save=${fixture}" -DTEST_TIMEOUT=200
    -P "${CMAKE_CURRENT_LIST_DIR}/run_godot_test.cmake"
    RESULT_VARIABLE result)
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "Godot check failed: treasure_camp_tests")
endif()
