# Runs the macOS app the way a player's machine would: installed under a folder
# whose name has spaces and non-ASCII letters, with the app and a copy of the
# original game files both read-only, a fresh home folder, only the system's
# tools on PATH and no OPENGOLD_* overrides. It creates characters, plays, saves,
# restarts and restores the saves, then crashes on purpose and checks the next
# launch leaves a report. Skipped when OPENGOLD_GAME_DIR (the original files to
# copy) is not set.
#
#   APP_BUNDLE  mac-package/OpenGoldBox.app, the packaged game
#   WORK        scratch folder for the install, game files and home folder
if("$ENV{OPENGOLD_GAME_DIR}" STREQUAL "")
    message("Skipped: OPENGOLD_GAME_DIR is not set")
    return()
endif()
set(original "$ENV{OPENGOLD_GAME_DIR}")
set(install "${WORK}/Programme Prüfung 冒険/OpenGoldBox.app")
set(game_files "${WORK}/Pool of Radiance Daten ü 冒険/POOLRAD")
set(home "${WORK}/home")
set(user_data "${home}/Library/Application Support/Godot/app_userdata/OpenGold")

function(run_or_stop)
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE result ERROR_VARIABLE errors OUTPUT_QUIET)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Failed (${result}): ${ARGN}\n${errors}")
    endif()
endfunction()

if(EXISTS "${WORK}")
    run_or_stop(chmod -R u+w "${WORK}")
endif()
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${user_data}")
get_filename_component(game_files_parent "${game_files}" DIRECTORY)
file(MAKE_DIRECTORY "${game_files_parent}")
run_or_stop(ditto "${APP_BUNDLE}" "${install}")
run_or_stop(ditto "${original}" "${game_files}")
run_or_stop(codesign --verify --deep --strict "${install}")
file(GLOB_RECURSE installed_before RELATIVE "${install}" "${install}/*")
run_or_stop(chmod -R a-w "${install}")
run_or_stop(chmod -R a-w "${game_files}")

set(ENV{HOME} "${home}")
unset(ENV{OPENGOLD_GAME_DIR})
unset(ENV{OPENGOLD_LANG})
set(ENV{PATH} "/usr/bin:/bin")

set(failure "")

# Runs the game with ARGN, expecting EXPECTED in its output.
function(run_game expected)
    if(NOT failure STREQUAL "")
        return()
    endif()
    execute_process(COMMAND "${install}/Contents/MacOS/OpenGoldBox" --headless ${ARGN}
        WORKING_DIRECTORY "${home}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 300)
    message("${output}${errors}")
    string(FIND "${output}" "${expected}" expected_at)
    if(NOT result EQUAL 0 OR expected_at EQUAL -1 OR
            "${output}${errors}" MATCHES "SCRIPT ERROR|Can't open dynamic library|GDExtension")
        set(failure "Packaged game failed (${result}): ${ARGN}" PARENT_SCOPE)
    endif()
endfunction()

# First launch: no settings yet, so the game waits in setup until it is told to quit.
run_game("" --quit-after 120)
if(failure STREQUAL "" AND EXISTS "${user_data}/settings.cfg")
    set(failure "First launch wrote settings before setup was confirmed")
endif()

# What setup saves once the player has chosen the language and the game folder.
file(WRITE "${user_data}/settings.cfg"
    "[game]\npath=\"${game_files}\"\n\n[interface]\nlanguage=\"en\"\n")
run_game("Godot party check passed" -- --party-check --save-check-write)
run_game("Godot campaign restart check passed" -- --save-check-read)

if(failure STREQUAL "")
    file(GLOB saves "${user_data}/checks/save-check/*.ogs")
    if(NOT saves)
        set(failure "No campaign save was written to the player's folder: ${user_data}")
    endif()
    file(GLOB reports "${user_data}/crash-reports/*")
    if(reports)
        set(failure "Clean exits left a crash report: ${reports}")
    endif()
endif()

# Two copies at once, which execute_process starts together: neither reports the other.
if(failure STREQUAL "")
    set(game "${install}/Contents/MacOS/OpenGoldBox")
    execute_process(COMMAND "${game}" --headless --quit-after 300 COMMAND "${game}" --headless --quit-after 300
        WORKING_DIRECTORY "${home}" RESULT_VARIABLE result OUTPUT_QUIET ERROR_QUIET TIMEOUT 120)
    file(GLOB reports "${user_data}/crash-reports/*")
    if(reports)
        set(failure "A running copy of the game was reported as crashed: ${reports}")
    endif()
endif()

# A crash: the log has the failure and a backtrace, and the next launch reports it once.
if(failure STREQUAL "")
    execute_process(COMMAND "${install}/Contents/MacOS/OpenGoldBox" --headless -- --crash-check
        WORKING_DIRECTORY "${home}" RESULT_VARIABLE result OUTPUT_QUIET ERROR_QUIET TIMEOUT 120)
    if(result EQUAL 0)
        set(failure "The crash check exited normally")
    endif()
endif()
run_game("did not shut down normally" --quit-after 30)
run_game("" --quit-after 30)
if(failure STREQUAL "")
    file(GLOB reports "${user_data}/crash-reports/*.txt")
    list(LENGTH reports report_count)
    if(NOT report_count EQUAL 1)
        set(failure "Expected one crash report, found ${report_count}")
    else()
        file(READ "${reports}" report)
        foreach(expected "Game version: " "Deliberate failure for the crash report check"
                "handle_crash" "END OF C++ BACKTRACE")
            string(FIND "${report}" "${expected}" expected_at)
            if(expected_at EQUAL -1)
                set(failure "The crash report lacks '${expected}':\n${report}")
            endif()
        endforeach()
    endif()
    file(GLOB marks "${user_data}/sessions/*")
    if(marks)
        set(failure "A clean exit left its session mark: ${marks}")
    endif()
endif()

if(failure STREQUAL "")
    file(GLOB_RECURSE installed_after RELATIVE "${install}" "${install}/*")
    if(NOT installed_after STREQUAL installed_before)
        set(failure "The game wrote into its app bundle: ${install}")
    endif()
endif()

run_or_stop(chmod -R u+w "${install}")
run_or_stop(chmod -R u+w "${game_files}")
if(NOT failure STREQUAL "")
    message(FATAL_ERROR "${failure}")
endif()
message("macOS package smoke check passed: read-only app and game files, non-ASCII paths, fresh home folder, saves restored after restart, crash reported once")
