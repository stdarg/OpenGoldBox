# Runs the distributable Windows package the way a player's machine would:
# installed under a folder whose name has spaces and non-ASCII letters, with
# the install folder and a copy of the original game files both read-only, a
# fresh Windows profile, no development tools on PATH and no OPENGOLD_*
# overrides. It creates characters, plays, saves, then restarts the game and
# restores the saves. Skipped when OPENGOLD_GAME_DIR (the original files to
# copy) is not set.
#
#   PACKAGE  the staged package folder (build/.../package/OpenGoldBox)
#   WORK     scratch folder for the install, game files and profile
if("$ENV{OPENGOLD_GAME_DIR}" STREQUAL "")
    message("Skipped: OPENGOLD_GAME_DIR is not set")
    return()
endif()
set(original "$ENV{OPENGOLD_GAME_DIR}")
set(install "${WORK}/Program Files Prüfung 冒険/OpenGoldBox")
set(game_files "${WORK}/Pool of Radiance Daten ü 冒険/POOLRAD")
set(profile "${WORK}/profile")
set(user_data "${profile}/Godot/app_userdata/OpenGold")
set(denied "$ENV{USERDOMAIN}\\$ENV{USERNAME}")

# Directory-only deny ACEs: no new file or folder appears and nothing is
# deleted. Files keep their own ACLs, which matters because the install's files
# are hard links that share theirs with the staged package.
function(set_read_only folder enabled)
    if(enabled)
        set(change /deny "${denied}:(CI)(WD,AD,DC)")
    else()
        set(change /remove:d "${denied}")
    endif()
    execute_process(COMMAND icacls "${folder}" ${change} /T /C /Q
        RESULT_VARIABLE result OUTPUT_QUIET ERROR_QUIET)
    if(enabled AND NOT result EQUAL 0)
        message(FATAL_ERROR "Cannot make ${folder} read-only (${result})")
    endif()
endfunction()

if(EXISTS "${WORK}")
    set_read_only("${WORK}" OFF)
endif()
file(REMOVE_RECURSE "${WORK}")

file(GLOB_RECURSE package_files RELATIVE "${PACKAGE}" "${PACKAGE}/*")
foreach(path IN LISTS package_files)
    get_filename_component(folder "${install}/${path}" DIRECTORY)
    file(MAKE_DIRECTORY "${folder}")
    file(CREATE_LINK "${PACKAGE}/${path}" "${install}/${path}" COPY_ON_ERROR)
endforeach()
file(COPY "${original}/" DESTINATION "${game_files}")
file(MAKE_DIRECTORY "${user_data}")
set_read_only("${install}" ON)
set_read_only("${game_files}" ON)

set(failure "")
execute_process(COMMAND cmd /c "echo probe> probe.txt" WORKING_DIRECTORY "${install}"
    RESULT_VARIABLE writable OUTPUT_QUIET ERROR_QUIET)
if(writable EQUAL 0)
    set(failure "The install folder is still writable; the check would prove nothing")
endif()

foreach(variable APPDATA LOCALAPPDATA USERPROFILE HOME)
    set(ENV{${variable}} "${profile}")
endforeach()
unset(ENV{OPENGOLD_GAME_DIR})
unset(ENV{OPENGOLD_LANG})
set(ENV{PATH} "$ENV{SystemRoot}\\System32;$ENV{SystemRoot}")

# Runs the packaged game with ARGN, expecting EXPECTED in its output.
function(run_game expected)
    if(NOT failure STREQUAL "")
        return()
    endif()
    execute_process(COMMAND "${install}/opengoldbox.console.exe" --headless ${ARGN}
        WORKING_DIRECTORY "${profile}"
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
run_game("Godot party check passed"
    -- --party-check --save-check-write)
run_game("Godot campaign restart check passed"
    -- --save-check-read)

if(failure STREQUAL "")
    file(GLOB saves "${user_data}/checks/save-check/*.ogs")
    if(NOT saves)
        set(failure "No campaign save was written to the player's folder: ${user_data}")
    endif()
    file(GLOB_RECURSE installed RELATIVE "${install}" "${install}/*")
    if(NOT installed STREQUAL package_files)
        set(failure "The game wrote into its install folder: ${install}")
    endif()
endif()

set_read_only("${install}" OFF)
set_read_only("${game_files}" OFF)
if(NOT failure STREQUAL "")
    message(FATAL_ERROR "${failure}")
endif()
message("Windows package smoke check passed: read-only install and game files, non-ASCII paths, fresh profile, saves restored after restart")
