# The game's export refuses to replace a copy of the game that is running
# (cmake/refuse_running_game.cmake), and builds on once it has quit. The game
# stands in as STAND_IN, run from a folder whose name holds characters a
# pattern would read as its own: spaces, brackets and dots.
#
#   STAND_IN  a program that sleeps until it is killed
#   GUARD     cmake/refuse_running_game.cmake
#   WORK      scratch folder
set(executable "${WORK}/Game (test) v1.0.app/Contents/MacOS/OpenGoldBox")
get_filename_component(folder "${executable}" DIRECTORY)
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${folder}")
file(COPY_FILE "${STAND_IN}" "${executable}")

set(failure "")

# Runs the guard for `path`, noting in `failure` when it does not refuse as
# expected; the stand-in is always stopped before the test reports.
function(guard path expect_refusal what)
    if(NOT failure STREQUAL "")
        return()
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DEXECUTABLE=${path}" -P "${GUARD}"
        RESULT_VARIABLE result ERROR_VARIABLE errors)
    if(expect_refusal AND (result EQUAL 0 OR NOT errors MATCHES "Quit it"))
        set(failure "${what}: the export went ahead (${result})\n${errors}" PARENT_SCOPE)
    elseif(NOT expect_refusal AND NOT result EQUAL 0)
        set(failure "${what}: the export was refused\n${errors}" PARENT_SCOPE)
    endif()
endfunction()

guard("${executable}" FALSE "No game running")
execute_process(COMMAND /bin/sh -c "\"$0\" >/dev/null 2>&1 & echo $!" "${executable}"
    OUTPUT_VARIABLE pid OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND /bin/sleep 1)
guard("${executable}" TRUE "The game running")
# Another game whose path begins the same is not this one.
guard("${folder}/OpenGoldBo" FALSE "A game at a shorter path")
execute_process(COMMAND /bin/kill "${pid}")
execute_process(COMMAND /bin/sleep 1)
guard("${executable}" FALSE "The game quit")
if(NOT failure STREQUAL "")
    message(FATAL_ERROR "${failure}")
endif()
message("Running game guard passed")
