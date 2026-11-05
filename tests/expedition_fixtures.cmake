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
set(ENV{OPENGOLD_TOWN_SERVICE_FIXTURES} "${FIXTURES}/town")
set(ENV{OPENGOLD_SLUMS_EVENT_FIXTURES} "${FIXTURES}/slums-events")
set(ENV{OPENGOLD_SLUMS_EVENT_SAMPLE} 8)
# Every other map's events: the whole route runs to save each map's arrival,
# then a sample of the event squares is written.
set(ENV{OPENGOLD_AREA_EVENT_FIXTURES} "${FIXTURES}/area-events")
set(ENV{OPENGOLD_AREA_EVENT_SAMPLE} 240)
execute_process(COMMAND "${EXPEDITION}" RESULT_VARIABLE result)
foreach(fixture shop.ogs hall.ogs west-gate.ogs ohlo-door.ogs slums/hobgoblins.ogs slums/orcs.ogs slums/trolls.ogs
        town/shop-9-13.ogs town/temple-11-3-sick.ogs town/inn-4-12-wounded.ogs
        slums-events/slums-3-13-10-3-potion.ogs)
    if(NOT result STREQUAL "0" OR NOT EXISTS "${FIXTURES}/${fixture}")
        message(FATAL_ERROR "The expedition route did not write ${fixture} (${result})")
    endif()
endforeach()
file(GLOB area_event_saves "${FIXTURES}/area-events/area*.ogs")
if(NOT area_event_saves)
    message(FATAL_ERROR "The expedition route wrote no saves of other maps' events")
endif()
