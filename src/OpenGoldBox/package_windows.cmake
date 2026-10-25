# Stages the distributable Windows package from the exported game in EXPORT,
# writes its notices and checks it, then zips it. EXPORT holds development
# leftovers (Godot's replaced-file .TMP copies, a developer's settings), so the
# package takes only the files a player needs.
#
#   EXPORT            win-package, the exported game
#   STAGE             folder to create; the package is STAGE/OpenGoldBox
#   ZIP               zip file to write
#   GODOT             Godot executable that matches the export templates
#   NOTICES_SCRIPT    write_engine_notices.gd
#   PROJECT_LICENSE   OpenGoldBox's LICENSE
#   GODOT_CPP_LICENSE godot-cpp's LICENSE.md
#   MD5_MANIFEST      por-pc13-md5.json, naming every original game file

set(package "${STAGE}/OpenGoldBox")
file(REMOVE_RECURSE "${STAGE}")
file(MAKE_DIRECTORY "${package}")

# The DLLs are the game's GDExtension and the Microsoft C++ runtime it needs.
file(GLOB libraries RELATIVE "${EXPORT}" "${EXPORT}/*.dll")
set(game_files opengoldbox.exe opengoldbox.console.exe opengoldbox.pck ${libraries})
foreach(name opengoldbox_game.dll vcruntime140.dll msvcp140.dll)
    if(NOT name IN_LIST libraries)
        message(FATAL_ERROR "Windows package is missing ${name}: ${EXPORT}")
    endif()
endforeach()
foreach(name IN LISTS game_files)
    if(NOT EXISTS "${EXPORT}/${name}")
        message(FATAL_ERROR "Windows package is missing ${name}: ${EXPORT}")
    endif()
    file(COPY "${EXPORT}/${name}" DESTINATION "${package}")
endforeach()
file(COPY "${EXPORT}/data" DESTINATION "${package}")

file(COPY_FILE "${PROJECT_LICENSE}" "${package}/LICENSE.txt")
execute_process(
    COMMAND "${GODOT}" --headless --script "${NOTICES_SCRIPT}" -- "${STAGE}/engine-notices.txt"
    RESULT_VARIABLE result OUTPUT_QUIET ERROR_VARIABLE errors)
if(NOT result EQUAL 0 OR NOT EXISTS "${STAGE}/engine-notices.txt")
    message(FATAL_ERROR "Cannot write the Godot Engine notices (${result}): ${errors}")
endif()
file(READ "${STAGE}/engine-notices.txt" engine_notices)
file(REMOVE "${STAGE}/engine-notices.txt")
file(READ "${GODOT_CPP_LICENSE}" godot_cpp_license)
file(WRITE "${package}/THIRD-PARTY-NOTICES.txt"
    "OpenGoldBox includes the following third-party software.\n\n"
    "Pool of Radiance is not included. OpenGoldBox reads the game files from a copy\n"
    "the player owns and installs separately.\n\n"
    "================================================================================\n"
    "${engine_notices}\n"
    "================================================================================\n"
    "godot-cpp (C++ bindings for GDExtension)\n\n"
    "${godot_cpp_license}\n"
    "================================================================================\n"
    "Microsoft Visual C++ runtime\n\n"
    "The vcruntime, msvcp and concrt DLLs are Microsoft Visual C++ Redistributable\n"
    "files, distributed with this application under the Microsoft Visual Studio\n"
    "license terms for redistributable code.\n")

# A player supplies the original game; the package must hold none of its files.
file(READ "${MD5_MANIFEST}" manifest)
string(JSON original_count LENGTH "${manifest}" files)
math(EXPR last "${original_count} - 1")
set(original_names "")
foreach(index RANGE ${last})
    string(JSON name MEMBER "${manifest}" files ${index})
    string(TOUPPER "${name}" name)
    list(APPEND original_names "${name}")
endforeach()
file(GLOB_RECURSE packaged RELATIVE "${package}" "${package}/*")
foreach(path IN LISTS packaged)
    get_filename_component(name "${path}" NAME)
    string(TOUPPER "${name}" upper)
    if(upper IN_LIST original_names OR upper MATCHES "\\.DAX$")
        message(FATAL_ERROR "Windows package contains an original game file: ${path}")
    endif()
    if(upper STREQUAL "SETTINGS.CFG" OR upper MATCHES "\\.TMP$")
        message(FATAL_ERROR "Windows package contains a development leftover: ${path}")
    endif()
endforeach()
foreach(required data/rules/srd-5.2.1/combat.rules data/art/walls/WALLDEF3_DAX_0_1/near_front.png
        data/monsters/mon2cha_dax_4/monster.json data/art/combat-body-looks.tsv)
    if(NOT EXISTS "${package}/${required}")
        message(FATAL_ERROR "Windows package is missing ${required}")
    endif()
endforeach()

file(REMOVE "${ZIP}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar cf "${ZIP}" --format=zip OpenGoldBox
    WORKING_DIRECTORY "${STAGE}" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Cannot write the Windows package zip: ${ZIP}")
endif()
list(LENGTH packaged packaged_count)
message(STATUS "Windows package: ${packaged_count} files in ${package}; ${ZIP}")
