# Stages the distributable Linux package from the exported game in EXPORT,
# writes its notices and checks it, then archives it. Like the Windows export,
# EXPORT can hold development leftovers, so the package takes only the files a
# player needs.
#
#   EXPORT            linux-package, the exported game
#   STAGE             folder to create; the package is STAGE/OpenGoldBox
#   ARCHIVE           .tar.gz to write
#   SYMBOLS           .tar.gz of the unstripped game library to write; not for players
#   STRIP             strip, to take the debug information out of the packaged library
#   GODOT             Godot executable that matches the export templates
#   NOTICES_SCRIPT    write_engine_notices.gd
#   PROJECT_LICENSE   OpenGoldBox's LICENSE
#   GODOT_CPP_LICENSE godot-cpp's LICENSE.md
#   MD5_MANIFEST      por-pc13-md5.json, naming every original game file

include("${CMAKE_CURRENT_LIST_DIR}/package_common.cmake")

set(package "${STAGE}/OpenGoldBox")
file(REMOVE_RECURSE "${STAGE}")
file(MAKE_DIRECTORY "${package}")

foreach(name opengoldbox.x86_64 opengoldbox.pck opengoldbox_game.so)
    if(NOT EXISTS "${EXPORT}/${name}")
        message(FATAL_ERROR "Linux package is missing ${name}: ${EXPORT}")
    endif()
    file(COPY "${EXPORT}/${name}" DESTINATION "${package}")
endforeach()
file(COPY "${EXPORT}/data" DESTINATION "${package}")
# The debug information would more than double the library's size, so players
# get it stripped.
execute_process(COMMAND "${STRIP}" --strip-debug "${package}/opengoldbox_game.so" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Cannot strip the game library: ${package}/opengoldbox_game.so")
endif()

file(COPY_FILE "${PROJECT_LICENSE}" "${package}/LICENSE.txt")
write_third_party_notices("${package}/THIRD-PARTY-NOTICES.txt" "")

check_package_files("${package}")
foreach(required data/rules/srd-5.2.1/combat.rules data/art/walls/WALLDEF3_DAX_0_1/near_front.png
        data/monsters/mon2cha_dax_4/monster.json data/art/combat-body-looks.tsv)
    if(NOT EXISTS "${package}/${required}")
        message(FATAL_ERROR "Linux package is missing ${required}")
    endif()
endforeach()

file(REMOVE "${ARCHIVE}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar czf "${ARCHIVE}" OpenGoldBox
    WORKING_DIRECTORY "${STAGE}" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Cannot write the Linux package: ${ARCHIVE}")
endif()

# Kept with a release: it reads crash reports down to source lines.
file(REMOVE "${SYMBOLS}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar czf "${SYMBOLS}" opengoldbox_game.so
    WORKING_DIRECTORY "${EXPORT}" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Cannot write the debug symbols: ${SYMBOLS}")
endif()
message(STATUS "Linux package: ${package}; ${ARCHIVE}; symbols: ${SYMBOLS}")
