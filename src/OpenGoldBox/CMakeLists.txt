if(NOT WIN32 AND NOT APPLE)
    message(FATAL_ERROR "The OpenGoldBox executable target supports Windows and macOS only.")
endif()
include(FetchContent)
if(NOT Python3_EXECUTABLE AND EXISTS "${PROJECT_SOURCE_DIR}/build/_deps/python/python.exe")
    set(Python3_EXECUTABLE "${PROJECT_SOURCE_DIR}/build/_deps/python/python.exe")
endif()
find_package(Python3 3.8 COMPONENTS Interpreter REQUIRED)
set(GODOTCPP_BUILD_PROFILE "${CMAKE_CURRENT_SOURCE_DIR}/build_profile.json" CACHE PATH "" FORCE)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${GODOTCPP_BUILD_PROFILE}")
set(GODOTCPP_DISABLE_EXCEPTIONS OFF CACHE BOOL "" FORCE)
set(GODOTCPP_USE_HOT_RELOAD OFF CACHE BOOL "" FORCE)
FetchContent_Declare(godot_cpp
    GIT_REPOSITORY https://github.com/godotengine/godot-cpp.git
    GIT_TAG e83fd0904c13356ed1d4c3d09f8bb9132bdc6b77)
FetchContent_MakeAvailable(godot_cpp)
# godot-cpp's headers are third-party; keep their warnings out of the project's.
get_target_property(godot_cpp_includes godot-cpp INTERFACE_INCLUDE_DIRECTORIES)
set_target_properties(godot-cpp PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${godot_cpp_includes}")
find_program(OPENGOLDBOX_GODOT NAMES godot_console godot Godot
    HINTS "/Applications/Godot.app/Contents/MacOS" "/Applications/Godot_mono.app/Contents/MacOS" REQUIRED)
set(game_project "${CMAKE_CURRENT_SOURCE_DIR}/godot")
if(WIN32)
    set(OPENGOLDBOX_DEBUG_TEMPLATE "${PROJECT_SOURCE_DIR}/build/export-templates/windows_debug_x86_64.exe"
        CACHE FILEPATH "Godot Windows x64 debug export template matching the installed editor")
    if(NOT EXISTS "${OPENGOLDBOX_DEBUG_TEMPLATE}")
        message(STATUS "Executable export needs OPENGOLDBOX_DEBUG_TEMPLATE. The game DLL can be built without templates; see src/OpenGoldBox/README.md.")
    endif()
    # Godot builds opengoldbox.console.exe, the CMD launcher, from this sibling template.
    string(REGEX REPLACE "\\.exe$" "_console.exe" console_template "${OPENGOLDBOX_DEBUG_TEMPLATE}")
    if(NOT EXISTS "${console_template}")
        message(STATUS "The CMD launcher opengoldbox.console.exe needs ${console_template}; see src/OpenGoldBox/README.md.")
    endif()
    file(TO_CMAKE_PATH "${OPENGOLDBOX_DEBUG_TEMPLATE}" OPENGOLDBOX_DEBUG_TEMPLATE)
    set(game_output "${PROJECT_SOURCE_DIR}/win-package")
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
    include(InstallRequiredSystemLibraries)
    if(NOT CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS)
        message(FATAL_ERROR "Visual C++ runtime DLLs were not found for the Windows package.")
    endif()
    set(export_preset "Windows Desktop")
    set(export_path "${game_output}/opengoldbox.exe")
    set(export_config "export_presets.cfg.in")
else()
    set(game_output "${PROJECT_SOURCE_DIR}/mac-package")
    set(export_preset "macOS")
    set(export_path "${game_output}/OpenGoldBox.app")
    set(export_config "export_presets.macos.cfg.in")
    set(pre_export_commands COMMAND "${CMAKE_COMMAND}" -E rm -rf "${export_path}")
endif()
configure_file("${export_config}" "${game_project}/export_presets.cfg" @ONLY)

add_library(opengoldbox_game SHARED rolf_tour_view.cpp combat_view.cpp character_creation_view.cpp
    portrait_picker.cpp portrait_catalog.cpp party_preview.cpp equipment_choice_dialog.cpp level_up_view.cpp level_up_dialog.cpp campaign_save_view.cpp save_slots.cpp
    character_pool_dialog.cpp town_sheet_dialog.cpp character_sheet_view.cpp character_sheet_text.cpp godot_sound_output.cpp register_types.cpp startup_view.cpp game_lifecycle.cpp screenshot_service.cpp localization.cpp application_settings.cpp)
target_link_libraries(opengoldbox_game PRIVATE opengold_core opengold_rules_srd5 godot-cpp)
target_compile_features(opengoldbox_game PRIVATE cxx_std_20)
target_compile_definitions(opengoldbox_game PRIVATE $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>)
set_target_properties(opengoldbox_game PROPERTIES PREFIX "" RUNTIME_OUTPUT_DIRECTORY "${game_project}/bin" LIBRARY_OUTPUT_DIRECTORY "${game_project}/bin")

file(GLOB portrait_sources CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/art/portraits/*.png"
    "${PROJECT_SOURCE_DIR}/data/art/portraits/*.png" "${PROJECT_SOURCE_DIR}/art/portraits/portraits.json")
set(portrait_outputs)
foreach(source IN LISTS portrait_sources)
    get_filename_component(name "${source}" NAME)
    set(output "${game_project}/bin/portraits/${name}")
    add_custom_command(OUTPUT "${output}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${game_project}/bin/portraits"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${source}" "${output}"
        DEPENDS "${source}" VERBATIM)
    list(APPEND portrait_outputs "${output}")
endforeach()
file(GLOB npc_portrait_sources CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/art/portraits/NPCs/*.png")
foreach(source IN LISTS npc_portrait_sources)
    get_filename_component(name "${source}" NAME)
    set(output "${game_project}/bin/portraits/NPCs/${name}")
    add_custom_command(OUTPUT "${output}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${game_project}/bin/portraits/NPCs"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${source}" "${output}"
        DEPENDS "${source}" VERBATIM)
    list(APPEND portrait_outputs "${output}")
endforeach()
set(splash_outputs)
foreach(name OpenGoldBoxSplashBackground OpenGoldBoxEngineLettering OpenGoldBoxGameLettering
    OpenGoldBoxEngineLettering.es OpenGoldBoxGameLettering.es)
    set(source "${PROJECT_SOURCE_DIR}/art/${name}.png")
    set(output "${game_project}/bin/splashes/${name}.png")
    add_custom_command(OUTPUT "${output}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${game_project}/bin/splashes"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${source}" "${output}"
        DEPENDS "${source}" VERBATIM)
    list(APPEND splash_outputs "${output}")
endforeach()
set(combat_body_source "${PROJECT_SOURCE_DIR}/data/art/combat-body-looks.tsv")
set(combat_body_output "${game_project}/data/art/combat-body-looks.tsv")
set(combat_weapon_source "${PROJECT_SOURCE_DIR}/data/art/combat-weapon-options.tsv")
set(combat_weapon_output "${game_project}/data/art/combat-weapon-options.tsv")
add_custom_command(OUTPUT "${combat_body_output}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${game_project}/data/art"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${combat_body_source}" "${combat_body_output}"
    DEPENDS "${combat_body_source}" VERBATIM)
add_custom_command(OUTPUT "${combat_weapon_output}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${game_project}/data/art"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${combat_weapon_source}" "${combat_weapon_output}"
    DEPENDS "${combat_weapon_source}" VERBATIM)
add_custom_target(opengoldbox_assets DEPENDS ${portrait_outputs} ${splash_outputs} "${combat_body_output}" "${combat_weapon_output}")

if(OPENGOLD_BUILD_TESTS)
    add_executable(opengold_combat_sprite_layout_tests "${PROJECT_SOURCE_DIR}/tests/combat_sprite_layout_tests.cpp")
    target_link_libraries(opengold_combat_sprite_layout_tests PRIVATE opengold_rules godot-cpp)
    add_test(NAME opengold_combat_sprite_layout_tests COMMAND opengold_combat_sprite_layout_tests)
    add_executable(opengold_guarded_handler_tests "${PROJECT_SOURCE_DIR}/tests/guarded_handler_tests.cpp")
    target_compile_definitions(opengold_guarded_handler_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_guarded_handler_tests COMMAND opengold_guarded_handler_tests)
    add_executable(opengold_cached_combat_tests "${PROJECT_SOURCE_DIR}/tests/cached_combat_tests.cpp")
    target_link_libraries(opengold_cached_combat_tests PRIVATE opengold_rules_srd5 opengold_core)
    target_compile_definitions(opengold_cached_combat_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_cached_combat_tests COMMAND opengold_cached_combat_tests)
    add_executable(opengold_monster_picture_tests "${PROJECT_SOURCE_DIR}/tests/monster_picture_tests.cpp")
    target_compile_definitions(opengold_monster_picture_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_monster_picture_tests COMMAND opengold_monster_picture_tests)
    add_executable(opengold_godot_path_tests "${PROJECT_SOURCE_DIR}/tests/godot_path_tests.cpp")
    target_link_libraries(opengold_godot_path_tests PRIVATE godot-cpp)
    target_compile_definitions(opengold_godot_path_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_godot_path_tests COMMAND opengold_godot_path_tests)
    add_test(NAME opengold_screenshot_tool_tests
        COMMAND "${Python3_EXECUTABLE}" -B "${PROJECT_SOURCE_DIR}/tests/screenshot_tool_tests.py")
    # The catalogs must list every message the game shows, with current references.
    add_test(NAME opengold_localization_check
        COMMAND "${Python3_EXECUTABLE}" -B "${PROJECT_SOURCE_DIR}/tools/localization.py" --check)
    # These checks use synthetic combat and first-run setup, without original
    # game files, export templates, or a packaged application.
    add_custom_target(opengoldbox_test_project
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/opengold.gdextension" "${game_project}/bin/opengold.gdextension"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory "${PROJECT_SOURCE_DIR}/data/rules/srd-5.2.1" "${game_project}/data/rules/srd-5.2.1"
        COMMAND "${OPENGOLDBOX_GODOT}" --headless --editor --path "${game_project}" --import --quit
        DEPENDS opengoldbox_game opengoldbox_assets VERBATIM)
    add_test(NAME opengold_godot_prepare COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}"
        --target opengoldbox_test_project --config $<CONFIG>)
    set_tests_properties(opengold_godot_prepare PROPERTIES FIXTURES_SETUP godot_project TIMEOUT 600 LABELS godot
        RESOURCE_LOCK godot_project)
    function(opengold_godot_test name script expected)
        add_test(NAME "opengold_godot_${name}" COMMAND "${CMAKE_COMMAND}"
            "-DGODOT=${OPENGOLDBOX_GODOT}" "-DPROJECT=${game_project}"
            "-DSCRIPT=${PROJECT_SOURCE_DIR}/tests/${script}.gd" "-DEXPECTED=${expected}" "-DARGS=${ARGN}"
            -P "${PROJECT_SOURCE_DIR}/tests/run_godot_test.cmake")
        set_tests_properties("opengold_godot_${name}" PROPERTIES FIXTURES_REQUIRED godot_project
            TIMEOUT 40 LABELS godot RESOURCE_LOCK godot_project
            ENVIRONMENT "OPENGOLD_GAME_DIR=;OPENGOLD_LANG=en;OPENGOLD_SCREENSHOT_DIR=${CMAKE_BINARY_DIR}/screenshots-test/${name}")
    endfunction()
    opengold_godot_test(rest rest_view_tests "Godot rest controls passed" "--rest-check")
    opengold_godot_test(combat_view combat_view_tests "Game combat view checks passed")
    opengold_godot_test(adrenaline adrenaline_view_tests "Adrenaline view checks passed" "--adrenaline-fixtures=${PROJECT_BINARY_DIR}/adrenaline-fixtures")
    set_tests_properties(opengold_godot_adrenaline PROPERTIES FIXTURES_REQUIRED "godot_project;adrenaline_fixtures")
    opengold_godot_test(components spell_component_view_tests "Spell component view checks passed" "--component-fixtures=${PROJECT_BINARY_DIR}/component-fixtures")
    set_tests_properties(opengold_godot_components PROPERTIES FIXTURES_REQUIRED "godot_project;component_fixtures")
    opengold_godot_test(posture posture_view_tests "Posture controls passed" "--posture-fixtures=${PROJECT_BINARY_DIR}/posture-fixtures")
    set_tests_properties(opengold_godot_posture PROPERTIES FIXTURES_REQUIRED "godot_project;posture_fixtures")
    opengold_godot_test(rogue_attack rogue_attack_view_tests "Rogue attack UI checks passed" "--rogue-fixtures=${PROJECT_BINARY_DIR}/rogue-fixtures")
    set_tests_properties(opengold_godot_rogue_attack PROPERTIES FIXTURES_REQUIRED "godot_project;cunning_fixtures")
    opengold_godot_test(cunning cunning_action_view_tests "Cunning Action controls passed" "--cunning-fixtures=${PROJECT_BINARY_DIR}/cunning-fixtures")
    set_tests_properties(opengold_godot_cunning PROPERTIES FIXTURES_REQUIRED "godot_project;cunning_fixtures")
    opengold_godot_test(surge action_surge_view_tests "Action Surge existing keyboard checks passed" "--surge-fixtures=${PROJECT_BINARY_DIR}/surge-fixtures")
    set_tests_properties(opengold_godot_surge PROPERTIES FIXTURES_REQUIRED "godot_project;surge_fixtures")
    opengold_godot_test(sorcerer sorcerer_view_tests "Sorcerer cantrip view checks passed" "--sorcerer-fixtures=${PROJECT_BINARY_DIR}/sorcerer-fixtures")
    set_tests_properties(opengold_godot_sorcerer PROPERTIES FIXTURES_REQUIRED "godot_project;sorcerer_fixtures")
    opengold_godot_test(eldritch eldritch_view_tests "Eldritch Blast view checks passed" "--eldritch-fixtures=${PROJECT_BINARY_DIR}/eldritch-fixtures")
    set_tests_properties(opengold_godot_eldritch PROPERTIES FIXTURES_REQUIRED "godot_project;eldritch_fixtures")
    opengold_godot_test(chill chill_view_tests "Chill Touch view checks passed" "--chill-fixtures=${PROJECT_BINARY_DIR}/chill-fixtures")
    set_tests_properties(opengold_godot_chill PROPERTIES FIXTURES_REQUIRED "godot_project;chill_fixtures")
    opengold_godot_test(medicine medicine_view_tests "Medicine and Tactical Mind controls passed" "--medicine-fixtures=${PROJECT_BINARY_DIR}/medicine-fixtures")
    set_tests_properties(opengold_godot_medicine PROPERTIES FIXTURES_REQUIRED "godot_project;medicine_fixtures")
    opengold_godot_test(thrown thrown_view_tests "Thrown weapon controls passed" "--thrown-fixtures=${PROJECT_BINARY_DIR}/thrown-fixtures")
    set_tests_properties(opengold_godot_thrown PROPERTIES FIXTURES_REQUIRED "godot_project;thrown_fixtures")
    opengold_godot_test(items items_view_tests "Items controls passed" "--playtest-fixtures=${PROJECT_BINARY_DIR}/playtest-fixtures")
    set_tests_properties(opengold_godot_items PROPERTIES FIXTURES_REQUIRED "godot_project;playtest_fixtures")
    opengold_godot_test(flee flee_view_tests "Flee controls passed" "--playtest-fixtures=${PROJECT_BINARY_DIR}/playtest-fixtures")
    set_tests_properties(opengold_godot_flee PROPERTIES FIXTURES_REQUIRED "godot_project;playtest_fixtures")
    opengold_godot_test(quick quick_view_tests "Quick combat controls passed" "--playtest-fixtures=${PROJECT_BINARY_DIR}/playtest-fixtures")
    set_tests_properties(opengold_godot_quick PROPERTIES FIXTURES_REQUIRED "godot_project;playtest_fixtures")
    opengold_godot_test(ui_audit ui_audit "UI audit passed" "--playtest-fixtures=${PROJECT_BINARY_DIR}/playtest-fixtures")
    set_tests_properties(opengold_godot_ui_audit PROPERTIES FIXTURES_REQUIRED "godot_project;playtest_fixtures")
    opengold_godot_test(champion champion_view_tests "Champion controls passed" "--champion-fixtures=${PROJECT_BINARY_DIR}/champion-fixtures")
    set_tests_properties(opengold_godot_champion PROPERTIES FIXTURES_REQUIRED "godot_project;champion_fixtures")
    opengold_godot_test(shocking shocking_view_tests "Shocking Grasp view checks passed" "--shocking-fixtures=${PROJECT_BINARY_DIR}/shocking-fixtures")
    set_tests_properties(opengold_godot_shocking PROPERTIES FIXTURES_REQUIRED "godot_project;shocking_fixtures")
    opengold_godot_test(frost frost_view_tests "Ray of Frost view checks passed" "--frost-fixtures=${PROJECT_BINARY_DIR}/frost-fixtures")
    set_tests_properties(opengold_godot_frost PROPERTIES FIXTURES_REQUIRED "godot_project;frost_fixtures")
    opengold_godot_test(sacred sacred_view_tests "Sacred Flame view checks passed" "--sacred-fixtures=${PROJECT_BINARY_DIR}/sacred-fixtures")
    set_tests_properties(opengold_godot_sacred PROPERTIES FIXTURES_REQUIRED "godot_project;sacred_fixtures")
    opengold_godot_test(inflict inflict_view_tests "Inflict Wounds view checks passed" "--inflict-fixtures=${PROJECT_BINARY_DIR}/inflict-fixtures")
    set_tests_properties(opengold_godot_inflict PROPERTIES FIXTURES_REQUIRED "godot_project;inflict_fixtures")
    opengold_godot_test(lay_on_hands lay_on_hands_view_tests "Lay On Hands view checks passed" "--lay-fixtures=${PROJECT_BINARY_DIR}/lay-fixtures")
    set_tests_properties(opengold_godot_lay_on_hands PROPERTIES FIXTURES_REQUIRED "godot_project;lay_fixtures")
    opengold_godot_test(smite smite_view_tests "Smite view checks passed" "--smite-fixtures=${PROJECT_BINARY_DIR}/smite-fixtures")
    set_tests_properties(opengold_godot_smite PROPERTIES FIXTURES_REQUIRED "godot_project;smite_fixtures")
    opengold_godot_test(bless bless_view_tests "Bless view checks passed" "--bless-fixtures=${PROJECT_BINARY_DIR}/bless-fixtures")
    set_tests_properties(opengold_godot_bless PROPERTIES FIXTURES_REQUIRED "godot_project;bless_fixtures")
    opengold_godot_test(command command_view_tests "Command view checks passed" "--command-fixtures=${PROJECT_BINARY_DIR}/bless-fixtures")
    set_tests_properties(opengold_godot_command PROPERTIES FIXTURES_REQUIRED "godot_project;bless_fixtures")
    opengold_godot_test(sacred_weapon sacred_weapon_view_tests "Sacred Weapon view checks passed" "--sacred-fixtures=${PROJECT_BINARY_DIR}/bless-fixtures")
    set_tests_properties(opengold_godot_sacred_weapon PROPERTIES FIXTURES_REQUIRED "godot_project;bless_fixtures")
    opengold_godot_test(hunters_mark hunters_mark_view_tests "Hunter's Mark view checks passed" "--ranger-fixtures=${PROJECT_BINARY_DIR}/ranger-fixtures")
    set_tests_properties(opengold_godot_hunters_mark PROPERTIES FIXTURES_REQUIRED "godot_project;ranger_fixtures")
    opengold_godot_test(ensnaring ensnaring_view_tests "Ensnaring Strike view checks passed" "--ensnare-fixtures=${PROJECT_BINARY_DIR}/ranger-fixtures")
    set_tests_properties(opengold_godot_ensnaring PROPERTIES FIXTURES_REQUIRED "godot_project;ranger_fixtures")
    opengold_godot_test(entangle entangle_view_tests "Entangle view checks passed" "--entangle-fixtures=${PROJECT_BINARY_DIR}/ranger-fixtures")
    set_tests_properties(opengold_godot_entangle PROPERTIES FIXTURES_REQUIRED "godot_project;ranger_fixtures")
    opengold_godot_test(divine_spark divine_spark_view_tests "Divine Spark view checks passed" "--spark-fixtures=${PROJECT_BINARY_DIR}/cleric-fixtures")
    set_tests_properties(opengold_godot_divine_spark PROPERTIES FIXTURES_REQUIRED "godot_project;cleric_fixtures")
    opengold_godot_test(poison poison_view_tests "Poison Spray view checks passed" "--poison-fixtures=${PROJECT_BINARY_DIR}/poison-fixtures")
    set_tests_properties(opengold_godot_poison PROPERTIES FIXTURES_REQUIRED "godot_project;poison_fixtures")
    opengold_godot_test(opportunity opportunity_view_tests "Opportunity view checks passed")
    opengold_godot_test(conditions condition_view_tests "Condition view checks passed" --conditions)
    opengold_godot_test(node_ownership node_ownership_tests "Native node ownership checks passed")
    opengold_godot_test(screenshots screenshot_tests "Screenshot checks passed")
    opengold_godot_test(shutdown_key shutdown_tests "Shutdown check passed" --reset-game-path --shutdown-key)
    opengold_godot_test(shutdown_close shutdown_tests "Shutdown check passed" --reset-game-path --shutdown-close)
    opengold_godot_test(shutdown_dialog_paused shutdown_tests "Shutdown check passed"
        --reset-game-path --shutdown-dialog --shutdown-paused)
    # Needs original files; skipped when OPENGOLD_GAME_DIR is not set.
    add_test(NAME opengold_godot_ohlo_save_route COMMAND "${CMAKE_COMMAND}"
        "-DGODOT=${OPENGOLDBOX_GODOT}" "-DPROJECT=${game_project}"
        "-DEXPEDITION=$<TARGET_FILE:opengold_expedition_tests>"
        "-DWORK=${CMAKE_BINARY_DIR}/ohlo-save-route"
        -P "${PROJECT_SOURCE_DIR}/tests/ohlo_save_route.cmake")
    set_tests_properties(opengold_godot_ohlo_save_route PROPERTIES FIXTURES_REQUIRED godot_project
        TIMEOUT 240 LABELS godot RESOURCE_LOCK godot_project SKIP_REGULAR_EXPRESSION "Skipped: ")
    add_test(NAME opengold_godot_shop_disclosure COMMAND "${CMAKE_COMMAND}"
        "-DGODOT=${OPENGOLDBOX_GODOT}" "-DPROJECT=${game_project}"
        "-DEXPEDITION=$<TARGET_FILE:opengold_expedition_tests>"
        "-DWORK=${CMAKE_BINARY_DIR}/shop-disclosure"
        -P "${PROJECT_SOURCE_DIR}/tests/shop_disclosure.cmake")
    set_tests_properties(opengold_godot_shop_disclosure PROPERTIES FIXTURES_REQUIRED godot_project
        TIMEOUT 240 LABELS godot RESOURCE_LOCK godot_project SKIP_REGULAR_EXPRESSION "Skipped: ")
    add_test(NAME opengold_godot_slums_encounter COMMAND "${CMAKE_COMMAND}"
        "-DGODOT=${OPENGOLDBOX_GODOT}" "-DPROJECT=${game_project}"
        "-DEXPEDITION=$<TARGET_FILE:opengold_expedition_tests>"
        "-DWORK=${CMAKE_BINARY_DIR}/slums-encounter"
        -P "${PROJECT_SOURCE_DIR}/tests/slums_encounter.cmake")
    set_tests_properties(opengold_godot_slums_encounter PROPERTIES FIXTURES_REQUIRED godot_project
        TIMEOUT 300 LABELS godot RESOURCE_LOCK godot_project SKIP_REGULAR_EXPRESSION "Skipped: ")
    add_test(NAME opengold_godot_slums_ui_audit COMMAND "${CMAKE_COMMAND}"
        "-DGODOT=${OPENGOLDBOX_GODOT}" "-DPROJECT=${game_project}"
        "-DEXPEDITION=$<TARGET_FILE:opengold_expedition_tests>"
        "-DPLAYTEST_FIXTURES=${PROJECT_BINARY_DIR}/playtest-fixtures"
        "-DWORK=${CMAKE_BINARY_DIR}/slums-ui-audit"
        -P "${PROJECT_SOURCE_DIR}/tests/ui_audit.cmake")
    set_tests_properties(opengold_godot_slums_ui_audit PROPERTIES
        FIXTURES_REQUIRED "godot_project;playtest_fixtures"
        TIMEOUT 300 LABELS godot RESOURCE_LOCK godot_project SKIP_REGULAR_EXPRESSION "Skipped: ")
    # Character-creation screens with original files; skipped without OPENGOLD_GAME_DIR.
    # Each keeps its Godot user data under WORK (see original_view_check.cmake).
    function(opengold_creation_check name expected)
        add_test(NAME "opengold_godot_creation_${name}" COMMAND "${CMAKE_COMMAND}"
            "-DGODOT=${OPENGOLDBOX_GODOT}" "-DPROJECT=${game_project}" "-DEXPECTED=${expected}"
            "-DWORK=${CMAKE_BINARY_DIR}/creation-checks/${name}" ${ARGN}
            -P "${PROJECT_SOURCE_DIR}/tests/original_view_check.cmake")
        set_tests_properties("opengold_godot_creation_${name}" PROPERTIES
            FIXTURES_REQUIRED godot_project TIMEOUT 320 LABELS godot RESOURCE_LOCK godot_project
            SKIP_REGULAR_EXPRESSION "Skipped: ")
    endfunction()
    opengold_creation_check(character "Godot C++ character check passed" -DCHECK_FLAG=--character-check)
    # The party check saves a campaign that the restart check then loads.
    opengold_creation_check(party "Godot party check passed"
        -DCHECK_FLAG=--party-check "-DARGS=--save-check-write"
        "-DWORK=${CMAKE_BINARY_DIR}/creation-checks/saves")
    set_tests_properties(opengold_godot_creation_party PROPERTIES FIXTURES_SETUP creation_save)
    opengold_creation_check(save_read "Godot campaign restart check passed"
        -DCHECK_FLAG=--save-check-read -DKEEP_WORK=ON
        "-DWORK=${CMAKE_BINARY_DIR}/creation-checks/saves")
    set_tests_properties(opengold_godot_creation_save_read PROPERTIES
        FIXTURES_REQUIRED "godot_project;creation_save")
    opengold_creation_check(advancement "Godot advancement passed" -DCHECK_FLAG=--advancement-check)
    opengold_creation_check(equipment_art "Equipment artwork checks passed"
        -DCHECK_FLAG=--equipment-art-check)
    opengold_creation_check(defeat "Godot defeat check passed" -DCHECK_FLAG=--defeat-check)
    opengold_creation_check(expedition "Godot expedition passed" -DCHECK_FLAG=--expedition-check)
    opengold_creation_check(training "Training UI checks passed" -DSCRIPT=training_view_tests)
    opengold_creation_check(portrait_fallback "Portrait fallback checks passed"
        -DSCRIPT=portrait_fallback_tests)
    opengold_creation_check(handoff "Combat handoff checks passed" -DSCRIPT=combat_handoff_tests)
    # Reads rendered pixels, so it needs a real window rather than --headless.
    opengold_creation_check(goliath "Goliath game rendering checks passed"
        -DSCRIPT=goliath_render_tests -DGRAPHICAL=ON)
    opengold_creation_check(hands "Equipment hand view checks passed"
        -DSCRIPT=equipment_hand_view_tests "-DARGS=--hands-fixture=${PROJECT_BINARY_DIR}/hands-ui.ogs")
    set_tests_properties(opengold_godot_creation_hands PROPERTIES
        FIXTURES_REQUIRED "godot_project;hands_fixtures")
    foreach(klass cleric fighter paladin ranger rogue wizard)
        opengold_creation_check(asi_${klass} "ASI controls passed" -DSCRIPT=asi_view_tests
            "-DARGS=--style-class=${klass}\;--style-fixture=${PROJECT_BINARY_DIR}/asi-${klass}-ui.ogs")
        set_tests_properties(opengold_godot_creation_asi_${klass} PROPERTIES
            FIXTURES_REQUIRED "godot_project;asi_fixtures")
    endforeach()
    foreach(klass fighter wizard)
        opengold_creation_check(skilled_${klass} "Skilled controls passed" -DSCRIPT=skilled_view_tests
            "-DARGS=--style-class=${klass}\;--style-fixture=${PROJECT_BINARY_DIR}/asi-${klass}-ui.ogs")
        set_tests_properties(opengold_godot_creation_skilled_${klass} PROPERTIES
            FIXTURES_REQUIRED "godot_project;asi_fixtures")
    endforeach()
endif()

# Godot exports the executable; the C++ game code is its GDExtension.
# Rules remain loose files because the engine-independent loader uses filesystem I/O.
if(WIN32)
    set(package_commands
        COMMAND "${CMAKE_COMMAND}" -E copy_directory "${PROJECT_SOURCE_DIR}/data/rules/srd-5.2.1" "${game_output}/data/rules/srd-5.2.1"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${game_output}/data/art"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${combat_body_source}" "${game_output}/data/art/combat-body-looks.tsv"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${combat_weapon_source}" "${game_output}/data/art/combat-weapon-options.tsv"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} "${game_output}")
else()
    set(package_commands
        COMMAND "${CMAKE_COMMAND}" -E copy_directory "${PROJECT_SOURCE_DIR}/data/rules/srd-5.2.1" "${export_path}/Contents/Resources/data/rules/srd-5.2.1"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${export_path}/Contents/Resources/data/art"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${combat_body_source}" "${export_path}/Contents/Resources/data/art/combat-body-looks.tsv"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${combat_weapon_source}" "${export_path}/Contents/Resources/data/art/combat-weapon-options.tsv"
        COMMAND codesign --force --deep --sign - "${export_path}"
        COMMAND "${CMAKE_COMMAND}" "-DAPP_BUNDLE=${export_path}" -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_macos_package.cmake")
endif()
add_custom_target(OpenGoldBox ALL
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/opengold.gdextension" "${game_project}/bin/opengold.gdextension"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${PROJECT_SOURCE_DIR}/data/rules/srd-5.2.1" "${game_project}/data/rules/srd-5.2.1"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${game_output}"
    COMMAND "${OPENGOLDBOX_GODOT}" --headless --editor --path "${game_project}" --import --quit
    ${pre_export_commands}
    COMMAND "${OPENGOLDBOX_GODOT}" --headless --path "${game_project}" --export-debug "${export_preset}" "${export_path}"
    ${package_commands}
    DEPENDS opengoldbox_game opengoldbox_assets
    VERBATIM)

opengold_enable_warnings("${CMAKE_CURRENT_SOURCE_DIR}")
