cmake_minimum_required(VERSION 3.24)

project(OpenGoldBox VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
    # Keep core, tests and the Godot boundary on the same CRT/iterator ABI.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

option(OPENGOLD_BUILD_TESTS "Build the native test suite" ${BUILD_TESTING})
option(OPENGOLD_BUILD_GODOT "Build the C++ Godot tour and combat scenes" OFF)
option(OPENGOLD_BUILD_GAME "Build the OpenGoldBox Windows game" OFF)
option(OPENGOLD_BUILD_FUZZERS "Build isolated Clang libFuzzer targets with ASan/UBSan" OFF)

if(OPENGOLD_BUILD_FUZZERS)
    if(OPENGOLD_BUILD_GAME OR OPENGOLD_BUILD_GODOT)
        message(FATAL_ERROR "Use a separate native-only build directory for fuzzing")
    endif()
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "Fuzzing requires Clang with libFuzzer, AddressSanitizer, and UBSan")
    endif()
    include(CheckCXXSourceCompiles)
    include(CMakePushCheckState)
    cmake_push_check_state(RESET)
    set(CMAKE_REQUIRED_FLAGS "-fsanitize=fuzzer,address,undefined")
    check_cxx_source_compiles("#include <cstddef>\n#include <cstdint>\nextern \"C\" int LLVMFuzzerTestOneInput(const std::uint8_t*, std::size_t) { return 0; }"
        OPENGOLD_HAS_FUZZER_RUNTIME)
    cmake_pop_check_state()
    if(NOT OPENGOLD_HAS_FUZZER_RUNTIME)
        message(FATAL_ERROR "This Clang installation lacks the required fuzzing/sanitizer runtimes")
    endif()
    # Instrument the libraries too; instrumenting only the harness would miss
    # coverage feedback and many memory/undefined-behavior failures in parsers.
    add_compile_options(-fsanitize=fuzzer-no-link,address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

add_library(opengold_rules STATIC src/OpenGold.Rules/src/rules.cpp src/OpenGold.Rules/src/character_rules.cpp)
target_include_directories(opengold_rules PUBLIC src/OpenGold.Rules/include)
add_library(opengold_rules_srd5 STATIC
    src/OpenGold.Rules.Srd5/src/srd5.cpp
    src/OpenGold.Rules.Srd5/src/character_rules.cpp
    src/OpenGold.Rules.Srd5/src/feature_grants.cpp
    src/OpenGold.Rules.Srd5/src/training.cpp
    src/OpenGold.Rules.Srd5/src/spell_access.cpp
    src/OpenGold.Rules.Srd5/src/combat_grid.cpp
    src/OpenGold.Rules.Srd5/src/status_effects.cpp
    src/OpenGold.Rules.Srd5/src/life_cycle.cpp
    src/OpenGold.Rules.Srd5/src/damage.cpp
    src/OpenGold.Rules.Srd5/src/rest_activity.cpp
    src/OpenGold.Rules.Srd5/src/recovery_timeline.cpp
)
target_include_directories(opengold_rules_srd5 PUBLIC src/OpenGold.Rules.Srd5/include)
target_link_libraries(opengold_rules_srd5 PUBLIC opengold_rules)

add_library(opengold_formats STATIC
    src/OpenGold.Formats/src/formats.cpp
    src/OpenGold.Formats/src/pool_radiance.cpp
    src/OpenGold.Formats/src/geo_map.cpp
    src/OpenGold.Formats/src/ecl.cpp
    src/OpenGold.Formats/src/wall_art.cpp
    src/OpenGold.Formats/src/por_sound.cpp
)

target_include_directories(opengold_formats
    PUBLIC
        src/OpenGold.Formats/include
)

add_library(opengold_core STATIC
    src/OpenGold.Core/src/core.cpp
    src/OpenGold.Core/src/character_art.cpp
    src/OpenGold.Core/src/combat_sprite_layers.cpp
    src/OpenGold.Core/src/combat_body_catalog.cpp
    src/OpenGold.Core/src/character_creator.cpp
    src/OpenGold.Core/src/character_pool.cpp
    src/OpenGold.Core/src/character.cpp
    src/OpenGold.Core/src/campaign_party.cpp
    src/OpenGold.Core/src/campaign_rest.cpp
    src/OpenGold.Core/src/campaign_save.cpp
    src/OpenGold.Core/src/save_file.cpp
    src/OpenGold.Core/src/inventory.cpp
    src/OpenGold.Core/src/creature_catalog.cpp
    src/OpenGold.Core/src/creature_factory.cpp
    src/OpenGold.Core/src/map_catalog.cpp
    src/OpenGold.Core/src/ecl_machine.cpp
    src/OpenGold.Core/src/por_effects.cpp
    src/OpenGold.Core/src/rolf_tour.cpp
    src/OpenGold.Core/src/phlan_session.cpp
    src/OpenGold.Core/src/combat_demo.cpp
    src/OpenGold.Core/src/dungeon_battlefield.cpp
    src/OpenGold.Core/src/exploration_view.cpp
    src/OpenGold.Core/src/speaker_audio.cpp
    src/OpenGold.Core/src/sound_bank.cpp
    src/OpenGold.Core/src/sound_player.cpp
)

target_include_directories(opengold_core
    PUBLIC
        src/OpenGold.Core/include
)

target_link_libraries(opengold_core
    PUBLIC
        opengold_formats
        opengold_rules
)

if(OPENGOLD_BUILD_TESTS OR OPENGOLD_BUILD_FUZZERS)
    add_library(opengold_fuzz_support STATIC tests/fuzz/fuzz_cases.cpp)
    target_link_libraries(opengold_fuzz_support PUBLIC opengold_core opengold_rules_srd5)
    add_executable(opengold_fuzz_smoke_tests tests/fuzz_smoke_tests.cpp)
    target_link_libraries(opengold_fuzz_smoke_tests PRIVATE opengold_fuzz_support)
endif()

if(OPENGOLD_BUILD_FUZZERS)
    foreach(kind formats checkpoint)
        add_executable(opengold_${kind}_fuzzer tests/fuzz/${kind}_fuzzer.cpp)
        target_link_libraries(opengold_${kind}_fuzzer PRIVATE opengold_fuzz_support)
        target_link_options(opengold_${kind}_fuzzer PRIVATE -fsanitize=fuzzer)
    endforeach()
endif()

if(OPENGOLD_BUILD_TESTS)
    enable_testing()
    add_test(NAME opengold_fuzz_smoke_tests COMMAND opengold_fuzz_smoke_tests)
    set_tests_properties(opengold_fuzz_smoke_tests PROPERTIES TIMEOUT 60 LABELS "fuzz;native")
    add_executable(opengold_concentration_tests tests/concentration_tests.cpp)
    target_include_directories(opengold_concentration_tests PRIVATE src/OpenGold.Rules.Srd5/src)
    target_link_libraries(opengold_concentration_tests PRIVATE opengold_rules_srd5)
    add_test(NAME opengold_concentration_tests COMMAND opengold_concentration_tests)

    add_executable(opengold_status_effect_tests tests/status_effect_tests.cpp)
    target_include_directories(opengold_status_effect_tests PRIVATE src/OpenGold.Rules.Srd5/src)
    target_link_libraries(opengold_status_effect_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_status_effect_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_status_effect_tests COMMAND opengold_status_effect_tests)
    add_executable(opengold_combat_grid_tests tests/combat_grid_tests.cpp)
    target_include_directories(opengold_combat_grid_tests PRIVATE src/OpenGold.Rules.Srd5/src)
    target_link_libraries(opengold_combat_grid_tests PRIVATE opengold_rules_srd5)
    add_test(NAME opengold_combat_grid_tests COMMAND opengold_combat_grid_tests)
    add_executable(opengold_archery_tests tests/archery_tests.cpp)
    target_link_libraries(opengold_archery_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_archery_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_archery_tests COMMAND opengold_archery_tests)
    add_executable(opengold_advancement_tests tests/advancement_tests.cpp)
    target_link_libraries(opengold_advancement_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_advancement_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_advancement_tests COMMAND opengold_advancement_tests)
    add_executable(opengold_feature_grant_tests tests/feature_grant_tests.cpp)
    target_link_libraries(opengold_feature_grant_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_feature_grant_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_feature_grant_tests COMMAND opengold_feature_grant_tests)
    add_executable(opengold_action_surge_tests tests/action_surge_tests.cpp)
    target_include_directories(opengold_action_surge_tests PRIVATE src/OpenGold.Rules.Srd5/src)
    target_link_libraries(opengold_action_surge_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_action_surge_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}" OPENGOLD_BINARY_DIR="${PROJECT_BINARY_DIR}")
    add_test(NAME opengold_action_surge_tests COMMAND opengold_action_surge_tests)
    set_tests_properties(opengold_action_surge_tests PROPERTIES FIXTURES_SETUP surge_fixtures)
    add_executable(opengold_ray_of_frost_tests tests/ray_of_frost_tests.cpp)
    target_include_directories(opengold_ray_of_frost_tests PRIVATE src/OpenGold.Rules.Srd5/src)
    target_link_libraries(opengold_ray_of_frost_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_ray_of_frost_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}" OPENGOLD_BINARY_DIR="${PROJECT_BINARY_DIR}")
    add_test(NAME opengold_ray_of_frost_tests COMMAND opengold_ray_of_frost_tests)
    set_tests_properties(opengold_ray_of_frost_tests PROPERTIES FIXTURES_SETUP frost_fixtures TIMEOUT 30)
    add_executable(opengold_sacred_flame_tests tests/sacred_flame_tests.cpp)
    target_include_directories(opengold_sacred_flame_tests PRIVATE src/OpenGold.Rules.Srd5/src)
    target_link_libraries(opengold_sacred_flame_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_sacred_flame_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}" OPENGOLD_BINARY_DIR="${PROJECT_BINARY_DIR}")
    add_test(NAME opengold_sacred_flame_tests COMMAND opengold_sacred_flame_tests)
    set_tests_properties(opengold_sacred_flame_tests PROPERTIES FIXTURES_SETUP sacred_fixtures)
    add_executable(opengold_poison_spray_tests tests/poison_spray_tests.cpp)
    target_include_directories(opengold_poison_spray_tests PRIVATE src/OpenGold.Rules.Srd5/src)
    target_link_libraries(opengold_poison_spray_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_poison_spray_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}" OPENGOLD_BINARY_DIR="${PROJECT_BINARY_DIR}")
    add_test(NAME opengold_poison_spray_tests COMMAND opengold_poison_spray_tests)
    set_tests_properties(opengold_poison_spray_tests PROPERTIES FIXTURES_SETUP poison_fixtures)
    add_executable(opengold_sorcerer_cantrip_tests tests/sorcerer_cantrip_tests.cpp)
    target_include_directories(opengold_sorcerer_cantrip_tests PRIVATE src/OpenGold.Rules.Srd5/src)
    target_link_libraries(opengold_sorcerer_cantrip_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_sorcerer_cantrip_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}" OPENGOLD_BINARY_DIR="${PROJECT_BINARY_DIR}")
    add_test(NAME opengold_sorcerer_cantrip_tests COMMAND opengold_sorcerer_cantrip_tests)
    set_tests_properties(opengold_sorcerer_cantrip_tests PROPERTIES FIXTURES_SETUP sorcerer_fixtures)
    add_executable(opengold_eldritch_blast_tests tests/eldritch_blast_tests.cpp)
    target_include_directories(opengold_eldritch_blast_tests PRIVATE src/OpenGold.Rules.Srd5/src)
    target_link_libraries(opengold_eldritch_blast_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_eldritch_blast_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}" OPENGOLD_BINARY_DIR="${PROJECT_BINARY_DIR}")
    add_test(NAME opengold_eldritch_blast_tests COMMAND opengold_eldritch_blast_tests)
    set_tests_properties(opengold_eldritch_blast_tests PROPERTIES FIXTURES_SETUP eldritch_fixtures)
    add_executable(opengold_chill_touch_tests tests/chill_touch_tests.cpp)
    target_include_directories(opengold_chill_touch_tests PRIVATE src/OpenGold.Rules.Srd5/src)
    target_link_libraries(opengold_chill_touch_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_chill_touch_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}" OPENGOLD_BINARY_DIR="${PROJECT_BINARY_DIR}")
    add_test(NAME opengold_chill_touch_tests COMMAND opengold_chill_touch_tests)
    set_tests_properties(opengold_chill_touch_tests PROPERTIES FIXTURES_SETUP chill_fixtures)
    add_executable(opengold_medicine_tests tests/medicine_tests.cpp)
    target_include_directories(opengold_medicine_tests PRIVATE src/OpenGold.Rules.Srd5/src)
    target_link_libraries(opengold_medicine_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_medicine_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}" OPENGOLD_BINARY_DIR="${PROJECT_BINARY_DIR}")
    add_test(NAME opengold_medicine_tests COMMAND opengold_medicine_tests)
    set_tests_properties(opengold_medicine_tests PROPERTIES FIXTURES_SETUP medicine_fixtures)
    add_executable(opengold_champion_tests tests/champion_tests.cpp)
    target_include_directories(opengold_champion_tests PRIVATE src/OpenGold.Rules.Srd5/src)
    target_link_libraries(opengold_champion_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_champion_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}" OPENGOLD_BINARY_DIR="${PROJECT_BINARY_DIR}")
    add_test(NAME opengold_champion_tests COMMAND opengold_champion_tests)
    set_tests_properties(opengold_champion_tests PROPERTIES FIXTURES_SETUP champion_fixtures)
    add_executable(opengold_thrown_weapon_tests tests/thrown_weapon_tests.cpp)
    target_link_libraries(opengold_thrown_weapon_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_thrown_weapon_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}" OPENGOLD_BINARY_DIR="${PROJECT_BINARY_DIR}")
    add_test(NAME opengold_thrown_weapon_tests COMMAND opengold_thrown_weapon_tests)
    set_tests_properties(opengold_thrown_weapon_tests PROPERTIES FIXTURES_SETUP thrown_fixtures)
    add_executable(opengold_ammunition_tests tests/ammunition_tests.cpp)
    target_link_libraries(opengold_ammunition_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_ammunition_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_ammunition_tests COMMAND opengold_ammunition_tests)
    add_executable(opengold_arcane_recovery_tests tests/arcane_recovery_tests.cpp)
    target_link_libraries(opengold_arcane_recovery_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_arcane_recovery_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_arcane_recovery_tests COMMAND opengold_arcane_recovery_tests)
    add_executable(opengold_shocking_grasp_tests tests/shocking_grasp_tests.cpp)
    target_include_directories(opengold_shocking_grasp_tests PRIVATE src/OpenGold.Rules.Srd5/src)
    target_link_libraries(opengold_shocking_grasp_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_shocking_grasp_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}" OPENGOLD_BINARY_DIR="${PROJECT_BINARY_DIR}")
    add_test(NAME opengold_shocking_grasp_tests COMMAND opengold_shocking_grasp_tests)
    set_tests_properties(opengold_shocking_grasp_tests PROPERTIES FIXTURES_SETUP shocking_fixtures)
    add_executable(opengold_spell_table_tests tests/spell_table_tests.cpp)
    target_include_directories(opengold_spell_table_tests PRIVATE src/OpenGold.Rules.Srd5/src)
    target_link_libraries(opengold_spell_table_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_spell_table_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}" OPENGOLD_BINARY_DIR="${PROJECT_BINARY_DIR}")
    add_test(NAME opengold_spell_table_tests COMMAND opengold_spell_table_tests)
    add_executable(opengold_spell_component_tests tests/spell_component_tests.cpp)
    target_include_directories(opengold_spell_component_tests PRIVATE src/OpenGold.Rules.Srd5/src)
    target_link_libraries(opengold_spell_component_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_spell_component_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}" OPENGOLD_BINARY_DIR="${PROJECT_BINARY_DIR}")
    add_test(NAME opengold_spell_component_tests COMMAND opengold_spell_component_tests)
    set_tests_properties(opengold_spell_component_tests PROPERTIES FIXTURES_SETUP component_fixtures)
    add_executable(opengold_training_tests tests/training_tests.cpp)
    target_link_libraries(opengold_training_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_training_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}" OPENGOLD_BINARY_DIR="${PROJECT_BINARY_DIR}")
    add_test(NAME opengold_training_tests COMMAND opengold_training_tests)
    set_tests_properties(opengold_training_tests PROPERTIES FIXTURES_SETUP cunning_fixtures)
    add_executable(opengold_rest_resource_tests tests/rest_resource_tests.cpp)
    target_link_libraries(opengold_rest_resource_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_rest_resource_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_rest_resource_tests COMMAND opengold_rest_resource_tests)
    add_executable(opengold_adrenaline_tests tests/adrenaline_tests.cpp)
    target_link_libraries(opengold_adrenaline_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_adrenaline_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}" OPENGOLD_BINARY_DIR="${PROJECT_BINARY_DIR}")
    add_test(NAME opengold_adrenaline_tests COMMAND opengold_adrenaline_tests)
    set_tests_properties(opengold_adrenaline_tests PROPERTIES FIXTURES_SETUP adrenaline_fixtures)
    add_executable(opengold_temporary_hp_tests tests/temporary_hp_tests.cpp)
    target_include_directories(opengold_temporary_hp_tests PRIVATE src/OpenGold.Rules.Srd5/src)
    target_link_libraries(opengold_temporary_hp_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_temporary_hp_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_temporary_hp_tests COMMAND opengold_temporary_hp_tests)
    add_executable(opengold_damage_tests tests/damage_tests.cpp)
    target_include_directories(opengold_damage_tests PRIVATE src/OpenGold.Rules.Srd5/src)
    target_link_libraries(opengold_damage_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_damage_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_damage_tests COMMAND opengold_damage_tests)
    add_executable(opengold_recovery_clock_tests tests/recovery_clock_tests.cpp)
    target_include_directories(opengold_recovery_clock_tests PRIVATE src/OpenGold.Rules.Srd5/src)
    target_link_libraries(opengold_recovery_clock_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_recovery_clock_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_recovery_clock_tests COMMAND opengold_recovery_clock_tests)
    add_executable(opengold_campaign_recovery_tests tests/campaign_recovery_tests.cpp)
    target_link_libraries(opengold_campaign_recovery_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_campaign_recovery_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_campaign_recovery_tests COMMAND opengold_campaign_recovery_tests)
    add_executable(opengold_campaign_rest_tests tests/campaign_rest_tests.cpp)
    target_link_libraries(opengold_campaign_rest_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_campaign_rest_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_campaign_rest_tests COMMAND opengold_campaign_rest_tests)
    add_executable(opengold_battlefield_tests tests/battlefield_tests.cpp)
    target_link_libraries(opengold_battlefield_tests PRIVATE opengold_core)
    add_test(NAME opengold_battlefield_tests COMMAND opengold_battlefield_tests)
    add_executable(opengold_core_tests
        tests/core_tests.cpp
    )
    target_link_libraries(opengold_core_tests PRIVATE opengold_core)
    add_test(NAME opengold_core_tests COMMAND opengold_core_tests)
    add_executable(opengold_content_tests tests/content_tests.cpp)
    target_link_libraries(opengold_content_tests PRIVATE opengold_rules_srd5)
    target_compile_definitions(opengold_content_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_content_tests COMMAND opengold_content_tests)
    add_executable(opengold_creature_tests tests/creature_tests.cpp)
    target_link_libraries(opengold_creature_tests PRIVATE opengold_core)
    add_test(NAME opengold_creature_tests COMMAND opengold_creature_tests)
    add_executable(opengold_ecl_tests tests/ecl_tests.cpp)
    target_link_libraries(opengold_ecl_tests PRIVATE opengold_core)
    add_test(NAME opengold_ecl_tests COMMAND opengold_ecl_tests)
    add_executable(opengold_tour_tests tests/tour_tests.cpp)
    target_link_libraries(opengold_tour_tests PRIVATE opengold_core)
    add_test(NAME opengold_tour_tests COMMAND opengold_tour_tests)
    add_executable(opengold_savage_attacker_tests tests/savage_attacker_tests.cpp)
    target_link_libraries(opengold_savage_attacker_tests PRIVATE opengold_rules_srd5 opengold_core)
    target_compile_definitions(opengold_savage_attacker_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_savage_attacker_tests COMMAND opengold_savage_attacker_tests "${PROJECT_BINARY_DIR}/savage-fixtures")
    set_tests_properties(opengold_savage_attacker_tests PROPERTIES FIXTURES_SETUP savage_fixtures)

    add_executable(opengold_spell_access_tests tests/spell_access_tests.cpp)
    target_link_libraries(opengold_spell_access_tests PRIVATE opengold_rules_srd5 opengold_core)
    target_compile_definitions(opengold_spell_access_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}" OPENGOLD_BINARY_DIR="${PROJECT_BINARY_DIR}")
    add_test(NAME opengold_spell_access_tests COMMAND opengold_spell_access_tests)

    add_executable(opengold_armor_catalog_tests tests/armor_catalog_tests.cpp)
    target_link_libraries(opengold_armor_catalog_tests PRIVATE opengold_rules_srd5 opengold_core)
    target_include_directories(opengold_armor_catalog_tests PRIVATE src/OpenGold.Rules.Srd5/src)
    target_compile_definitions(opengold_armor_catalog_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_armor_catalog_tests COMMAND opengold_armor_catalog_tests)

    add_executable(opengold_weapon_catalog_tests tests/weapon_catalog_tests.cpp)
    target_link_libraries(opengold_weapon_catalog_tests PRIVATE opengold_rules_srd5 opengold_core)
    target_include_directories(opengold_weapon_catalog_tests PRIVATE src/OpenGold.Rules.Srd5/src)
    target_compile_definitions(opengold_weapon_catalog_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_weapon_catalog_tests COMMAND opengold_weapon_catalog_tests)

    add_executable(opengold_heavy_weapon_tests tests/heavy_weapon_tests.cpp)
    target_link_libraries(opengold_heavy_weapon_tests PRIVATE opengold_rules_srd5 opengold_core)
    target_compile_definitions(opengold_heavy_weapon_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_heavy_weapon_tests COMMAND opengold_heavy_weapon_tests)

    add_executable(opengold_versatile_tests tests/versatile_tests.cpp)
    target_link_libraries(opengold_versatile_tests PRIVATE opengold_rules_srd5 opengold_core)
    target_compile_definitions(opengold_versatile_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_versatile_tests COMMAND opengold_versatile_tests)
    add_executable(opengold_rules_tests tests/rules_tests.cpp)
    target_link_libraries(opengold_rules_tests PRIVATE opengold_rules_srd5 opengold_core)
    target_compile_definitions(opengold_rules_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_rules_tests COMMAND opengold_rules_tests)
    add_executable(opengold_character_tests tests/character_tests.cpp)
    target_link_libraries(opengold_character_tests PRIVATE opengold_core opengold_rules_srd5)
    add_test(NAME opengold_character_tests COMMAND opengold_character_tests)
    add_executable(opengold_art_tests tests/art_tests.cpp)
    target_link_libraries(opengold_art_tests PRIVATE opengold_core)
    add_test(NAME opengold_art_tests COMMAND opengold_art_tests)
    add_executable(opengold_party_tests tests/party_tests.cpp)
    target_link_libraries(opengold_party_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_party_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_party_tests COMMAND opengold_party_tests)
    add_executable(opengold_save_tests tests/save_tests.cpp)
    target_link_libraries(opengold_save_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_save_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_save_tests COMMAND opengold_save_tests)
    add_executable(opengold_sound_tests tests/sound_tests.cpp)
    target_link_libraries(opengold_sound_tests PRIVATE opengold_core)
    add_test(NAME opengold_sound_tests COMMAND opengold_sound_tests)
    add_executable(opengold_sound_player_tests tests/sound_player_tests.cpp)
    target_link_libraries(opengold_sound_player_tests PRIVATE opengold_core)
    add_test(NAME opengold_sound_player_tests COMMAND opengold_sound_player_tests)
endif()

add_executable(opengold_creatures tools/creatures.cpp)
target_link_libraries(opengold_creatures PRIVATE opengold_core)
add_executable(opengold_maps tools/maps.cpp)
target_link_libraries(opengold_maps PRIVATE opengold_core)
add_executable(opengold_scripts tools/scripts.cpp)
target_link_libraries(opengold_scripts PRIVATE opengold_core)

if(OPENGOLD_BUILD_GODOT)
    set_target_properties(opengold_core opengold_formats opengold_rules opengold_rules_srd5 PROPERTIES POSITION_INDEPENDENT_CODE ON)
    add_subdirectory(demos/src/OpenGold.Godot)
endif()

if(OPENGOLD_BUILD_GAME)
    add_subdirectory(src/OpenGoldBox)
endif()

if(OPENGOLD_BUILD_TESTS)
    add_executable(opengold_natural_sleep_tests tests/natural_sleep_tests.cpp)
    target_link_libraries(opengold_natural_sleep_tests PRIVATE opengold_core opengold_rules_srd5)
    target_include_directories(opengold_natural_sleep_tests PRIVATE src/OpenGold.Rules.Srd5/src)
    target_compile_definitions(opengold_natural_sleep_tests PRIVATE OPENGOLD_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}" OPENGOLD_BINARY_DIR="${CMAKE_BINARY_DIR}")
    add_test(NAME opengold_natural_sleep_tests COMMAND opengold_natural_sleep_tests)
    set_tests_properties(opengold_natural_sleep_tests PROPERTIES FIXTURES_SETUP sleep_fixtures)
endif()
