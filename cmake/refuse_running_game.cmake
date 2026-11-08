# Stops a build from replacing the game while a copy of it runs. Godot reads
# each scene from its .pck by name when it needs it, so a running game handed a
# rewritten .pck reads the wrong bytes and can no longer open its screens (the
# town's Explore button then does nothing). Quitting the game first is enough.
#
#   EXECUTABLE  the exported game's executable, as the running game names it
if(NOT EXECUTABLE)
    message(FATAL_ERROR "refuse_running_game.cmake needs EXECUTABLE")
endif()
find_program(pgrep_program pgrep)
if(NOT pgrep_program)
    return()
endif()
# pgrep -f matches a pattern against each process's whole command line, so
# the path's own regular-expression characters are escaped.
string(REGEX REPLACE "([][.^$*+?(){}|\\\\])" "\\\\\\1" pattern "${EXECUTABLE}")
execute_process(COMMAND "${pgrep_program}" -f "^${pattern}( |$)"
    RESULT_VARIABLE not_running OUTPUT_VARIABLE running ERROR_QUIET)
if(not_running EQUAL 0)
    string(STRIP "${running}" running)
    message(FATAL_ERROR "The game is running from ${EXECUTABLE} (process ${running}). "
        "Quit it and build again: replacing it while it runs breaks the running copy.")
endif()
