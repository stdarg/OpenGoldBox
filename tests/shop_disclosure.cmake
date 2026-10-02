# The New Phlan shop list with original files: opengold_expedition_tests writes a
# party outside the jeweler, and a fresh Godot game loads it through Load game,
# opens the shop and reads its stock. Godot's user data goes to WORK.
if("$ENV{OPENGOLD_GAME_DIR}" STREQUAL "")
    message("Skipped: OPENGOLD_GAME_DIR is not set")
    return()
endif()
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/home")
set(fixture "${WORK}/shop.ogs")

set(ENV{OPENGOLD_SHOP_FIXTURE} "${fixture}")
execute_process(COMMAND "${EXPEDITION}" RESULT_VARIABLE result)
unset(ENV{OPENGOLD_SHOP_FIXTURE})
if(NOT result STREQUAL "0" OR NOT EXISTS "${fixture}")
    message(FATAL_ERROR "The expedition route did not write the shop fixture (${result})")
endif()

foreach(variable HOME APPDATA XDG_DATA_HOME)
    set(ENV{${variable}} "${WORK}/home")
endforeach()
set(ENV{OPENGOLD_LANG} en)
execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DGODOT=${GODOT}" "-DPROJECT=${PROJECT}"
    "-DSCRIPT=${CMAKE_CURRENT_LIST_DIR}/shop_disclosure_tests.gd"
    "-DEXPECTED=Shop disclosure checks passed" -DTEST_TIMEOUT=120
    "-DARGS=--shop-fixture=${fixture}"
    -P "${CMAKE_CURRENT_LIST_DIR}/run_godot_test.cmake"
    RESULT_VARIABLE result)
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "The game's shop list does not disclose unusable stock")
endif()
file(REMOVE_RECURSE "${WORK}")
