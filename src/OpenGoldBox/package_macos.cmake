# Adds the notices to the exported app and checks it holds what a player needs
# and nothing else. The export recreates the bundle each time, so unlike the
# Windows package it collects no leftovers and needs no staging folder.
#
#   APP_BUNDLE        mac-package/OpenGoldBox.app, the exported game
#   GODOT             Godot executable that matches the export templates
#   NOTICES_SCRIPT    write_engine_notices.gd
#   PROJECT_LICENSE   OpenGoldBox's LICENSE
#   GODOT_CPP_LICENSE godot-cpp's LICENSE.md
#   MD5_MANIFEST      por-pc13-md5.json, naming every original game file

include("${CMAKE_CURRENT_LIST_DIR}/package_common.cmake")

set(resources "${APP_BUNDLE}/Contents/Resources")
file(COPY_FILE "${PROJECT_LICENSE}" "${resources}/LICENSE.txt")
write_third_party_notices("${resources}/THIRD-PARTY-NOTICES.txt" "")

check_package_files("${APP_BUNDLE}")
foreach(required
        Contents/MacOS/OpenGoldBox
        Contents/Frameworks/opengoldbox_game.dylib
        Contents/Resources/OpenGoldBox.pck
        Contents/Resources/data/rules/srd-5.2.1/combat.rules
        Contents/Resources/data/art/walls/WALLDEF3_DAX_0_1/near_front.png
        Contents/Resources/data/monsters/mon2cha_dax_4/monster.json
        Contents/Resources/data/art/combat-body-looks.tsv)
    if(NOT EXISTS "${APP_BUNDLE}/${required}")
        message(FATAL_ERROR "macOS package is missing ${required}: ${APP_BUNDLE}")
    endif()
endforeach()
