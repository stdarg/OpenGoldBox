# What the Windows and macOS packages share: the third-party notices and the
# check that a package holds only what a player should get.

# Writes THIRD-PARTY-NOTICES.txt to OUTPUT, ending with EXTRA_NOTICES. Uses
# GODOT, NOTICES_SCRIPT and GODOT_CPP_LICENSE from the calling script.
function(write_third_party_notices output extra_notices)
    set(engine_file "${output}.engine")
    execute_process(
        COMMAND "${GODOT}" --headless --script "${NOTICES_SCRIPT}" -- "${engine_file}"
        RESULT_VARIABLE result OUTPUT_QUIET ERROR_VARIABLE errors)
    if(NOT result EQUAL 0 OR NOT EXISTS "${engine_file}")
        message(FATAL_ERROR "Cannot write the Godot Engine notices (${result}): ${errors}")
    endif()
    file(READ "${engine_file}" engine_notices)
    file(REMOVE "${engine_file}")
    file(READ "${GODOT_CPP_LICENSE}" godot_cpp_license)
    file(WRITE "${output}"
        "OpenGoldBox includes the following third-party software.\n\n"
        "Pool of Radiance is not included. OpenGoldBox reads the game files from a copy\n"
        "the player owns and installs separately.\n\n"
        "================================================================================\n"
        "${engine_notices}\n"
        "================================================================================\n"
        "godot-cpp (C++ bindings for GDExtension)\n\n"
        "${godot_cpp_license}\n"
        "${extra_notices}")
endfunction()

# A player supplies the original game; a package must hold none of its files.
# Stops on any file the MD5_MANIFEST names, any .DAX, a settings.cfg or a .TMP.
function(check_package_files package_folder)
    file(READ "${MD5_MANIFEST}" manifest)
    string(JSON original_count LENGTH "${manifest}" files)
    math(EXPR last "${original_count} - 1")
    set(original_names "")
    foreach(index RANGE ${last})
        string(JSON name MEMBER "${manifest}" files ${index})
        string(TOUPPER "${name}" name)
        list(APPEND original_names "${name}")
    endforeach()
    file(GLOB_RECURSE packaged RELATIVE "${package_folder}" "${package_folder}/*")
    foreach(path IN LISTS packaged)
        get_filename_component(name "${path}" NAME)
        string(TOUPPER "${name}" upper)
        if(upper IN_LIST original_names OR upper MATCHES "\\.DAX$")
            message(FATAL_ERROR "The package contains an original game file: ${path}")
        endif()
        if(upper STREQUAL "SETTINGS.CFG" OR upper MATCHES "\\.TMP$")
            message(FATAL_ERROR "The package contains a development leftover: ${path}")
        endif()
    endforeach()
endfunction()
