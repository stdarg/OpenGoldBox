# The saves the Godot checks with original files load: one run of
# opengold_expedition_tests writes them all into FIXTURES, so each check reads
# them instead of playing the route again while it holds the Godot project.
if("$ENV{OPENGOLD_GAME_DIR}" STREQUAL "")
    message("Skipped: OPENGOLD_GAME_DIR is not set")
    return()
endif()
file(REMOVE_RECURSE "${FIXTURES}")
file(MAKE_DIRECTORY "${FIXTURES}")

set(ENV{OPENGOLD_SHOP_FIXTURE} "${FIXTURES}/shop.ogs")
set(ENV{OPENGOLD_HALL_FIXTURE} "${FIXTURES}/hall.ogs")
set(ENV{OPENGOLD_GATE_FIXTURE} "${FIXTURES}/west-gate.ogs")
set(ENV{OPENGOLD_OHLO_FIXTURE} "${FIXTURES}/ohlo-door.ogs")
set(ENV{OPENGOLD_SLUMS_FIXTURES} "${FIXTURES}/slums")
execute_process(COMMAND "${EXPEDITION}" RESULT_VARIABLE result)
foreach(fixture shop.ogs hall.ogs west-gate.ogs ohlo-door.ogs slums/hobgoblins.ogs slums/orcs.ogs slums/trolls.ogs)
    if(NOT result STREQUAL "0" OR NOT EXISTS "${FIXTURES}/${fixture}")
        message(FATAL_ERROR "The expedition route did not write ${fixture} (${result})")
    endif()
endforeach()
