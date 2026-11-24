# The series' next game's party screen brings in the winning party the game
# before banked at its win (winner_bank_import_tests.gd, docs/SAVES.md, The
# winning party). Skipped unless Pool of Radiance's, EARLIER's and SEQUEL's
# files are found (find_games.cmake).
#
# BUTTON and TITLE are the party screen's button and the dialog's title for
# EARLIER; NPC set says its winning party has a permanent NPC to bring in.
#
# FIXTURE_WRITER (opengold_winner_bank_tests) writes EARLIER's winning party
# and an ordinary EARLIER save; the check runs SEQUEL with its own
# settings.cfg and user data under WORK.
include("${CMAKE_CURRENT_LIST_DIR}/find_games.cmake")
if(NOT DEFINED game_folder_POOLRAD OR NOT DEFINED "game_folder_${EARLIER}"
   OR NOT DEFINED "game_folder_${SEQUEL}")
    message("Skipped: ${EARLIER}'s or ${SEQUEL}'s files are not here")
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
     "[game]\npath=\"${game_folder}\"\n\n${found_games_settings}\n[interface]\nlanguage=\"en\"\n")

set(fixtures "${WORK}/fixtures")
set(ENV{OPENGOLD_WINNER_BANK_FIXTURES} "${fixtures}")
execute_process(COMMAND "${CMAKE_COMMAND}" -E env "HOME=${found_games_home}" "${FIXTURE_WRITER}"
    RESULT_VARIABLE result)
unset(ENV{OPENGOLD_WINNER_BANK_FIXTURES})
set(bank "${fixtures}/${EARLIER}-winning-party.ogs")
set(save "${fixtures}/${EARLIER}-save.ogs")
if(NOT result STREQUAL "0" OR NOT EXISTS "${bank}" OR NOT EXISTS "${save}")
    message(FATAL_ERROR "The ${EARLIER} saves were not written (${result})")
endif()

set(arguments --game=${SEQUEL} --rules=original --earlier=${EARLIER} --bank=${bank} --save=${save}
    "--button=${BUTTON}" "--title=${TITLE}")
if(NPC)
    list(APPEND arguments --npc)
endif()
execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DGODOT=${GODOT}" "-DPROJECT=${PROJECT}"
    "-DSCRIPT=${CMAKE_CURRENT_LIST_DIR}/winner_bank_import_tests.gd" "-DEXPECTED=${EXPECTED}"
    "-DARGS=${arguments}"
    -DTEST_TIMEOUT=200
    -P "${CMAKE_CURRENT_LIST_DIR}/run_godot_test.cmake"
    RESULT_VARIABLE result)
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "Godot check failed: winner_bank_import_tests")
endif()
