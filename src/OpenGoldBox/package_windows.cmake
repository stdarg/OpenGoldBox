# Stages the distributable Windows package from the exported game in EXPORT,
# writes its notices and checks it, then zips it. EXPORT holds development
# leftovers (Godot's replaced-file .TMP copies, a developer's settings), so the
# package takes only the files a player needs.
#
#   EXPORT            win-package, the exported game
#   STAGE             folder to create; the package is STAGE/OpenGoldBox
#   ZIP               zip file to write
#   SYMBOLS           zip of the game DLL's debug symbols to write; not for players
#   GAME_PDB          the game DLL's PDB
#   GODOT             Godot executable that matches the export templates
#   NOTICES_SCRIPT    write_engine_notices.gd
#   PROJECT_LICENSE   OpenGoldBox's LICENSE
#   GODOT_CPP_LICENSE godot-cpp's LICENSE.md
#   MD5_MANIFEST      por-pc13-md5.json, naming every original game file

include("${CMAKE_CURRENT_LIST_DIR}/package_common.cmake")

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
write_third_party_notices("${package}/THIRD-PARTY-NOTICES.txt"
    "================================================================================\n"
    "Microsoft Visual C++ runtime\n\n"
    "The vcruntime, msvcp and concrt DLLs are Microsoft Visual C++ Redistributable\n"
    "files, distributed with this application under the Microsoft Visual Studio\n"
    "license terms for redistributable code.\n")

check_package_files("${package}")
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

# Kept with a release: it reads crash reports down to source lines.
get_filename_component(pdb_folder "${GAME_PDB}" DIRECTORY)
get_filename_component(pdb_name "${GAME_PDB}" NAME)
file(REMOVE "${SYMBOLS}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar cf "${SYMBOLS}" --format=zip "${pdb_name}"
    WORKING_DIRECTORY "${pdb_folder}" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Cannot write the debug symbols: ${SYMBOLS}")
endif()
message(STATUS "Windows package: ${package}; ${ZIP}; symbols: ${SYMBOLS}")
