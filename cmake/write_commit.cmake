# Writes the commit being built to OUTPUT, for the game's log (game_log.cpp). A
# build outside a git checkout, such as from a source archive, says "unknown". The
# file is rewritten only when the commit changes, so it costs no rebuild otherwise.
execute_process(COMMAND git rev-parse --short=12 HEAD WORKING_DIRECTORY "${SOURCE}"
    OUTPUT_VARIABLE commit OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET RESULT_VARIABLE failed)
if(failed OR commit STREQUAL "")
    set(commit "unknown")
else()
    execute_process(COMMAND git status --porcelain --untracked-files=no WORKING_DIRECTORY "${SOURCE}"
        OUTPUT_VARIABLE changes OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(NOT changes STREQUAL "")
        string(APPEND commit " with local changes")
    endif()
endif()
file(CONFIGURE OUTPUT "${OUTPUT}" CONTENT "#define OPENGOLDBOX_COMMIT \"${commit}\"\n" @ONLY)
