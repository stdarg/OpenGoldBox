cmake_minimum_required(VERSION 3.24)

# Lets CMAKE_MSVC_DEBUG_INFORMATION_FORMAT choose /Z7 below; must precede project().
if(POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
endif()

project(OpenGoldBox VERSION 0.1.0 LANGUAGES CXX)

# A compiler cache lets a fresh worktree reuse objects another tree compiled.
# base_dir makes paths relative so the cache key does not contain the checkout
# location, and hash_dir=false keeps the build directory out of the key.
find_program(OPENGOLD_CCACHE ccache)
if(OPENGOLD_CCACHE)
    execute_process(COMMAND "${OPENGOLD_CCACHE}" --version OUTPUT_VARIABLE ccache_version_text)
    string(REGEX MATCH "[0-9]+\\.[0-9]+" ccache_version "${ccache_version_text}")
    if(ccache_version VERSION_LESS 4.8)
        # Older ccache, such as AlmaLinux 8's 3.7 in the Linux release container,
        # takes its settings only from the environment.
        set(CMAKE_CXX_COMPILER_LAUNCHER "${CMAKE_COMMAND};-E;env;CCACHE_BASEDIR=${PROJECT_SOURCE_DIR}"
            "CCACHE_NOHASHDIR=1;${OPENGOLD_CCACHE}")
    else()
        set(CMAKE_CXX_COMPILER_LAUNCHER
            "${OPENGOLD_CCACHE};base_dir=${PROJECT_SOURCE_DIR};hash_dir=false")
    endif()
    # ccache cannot cache MSVC's shared PDB (/Zi); embedded debug info (/Z7) is cacheable.
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>")
endif()

include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# compile_commands.json for clang-tidy (docs/CODING_STYLE.md). Ninja and Makefile
# generators write it; Visual Studio ignores the setting.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(MSVC)
    # Keep core, tests and the Godot boundary on the same CRT/iterator ABI.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

option(OPENGOLD_BUILD_TESTS "Build the native test suite" ${BUILD_TESTING})
option(OPENGOLD_BUILD_GODOT "Build the C++ Godot tour and combat scenes" OFF)
option(OPENGOLD_BUILD_GAME "Build the OpenGoldBox Windows game" OFF)
option(OPENGOLD_BUILD_FUZZERS "Build isolated Clang libFuzzer targets with ASan/UBSan" OFF)
option(OPENGOLD_BUILD_DOS_PROBE "Build the research-only DOS probe (dev_tools/dos_probe)" OFF)

# Effective C++ Item 53: the project's own targets build at a high warning
# level and fail on any warning, while fetched code such as godot-cpp keeps its
# own flags. Designated initializers that leave fields at their defaults are
# idiomatic here, so that one warning stays off.
if(MSVC)
    # getenv and sscanf are standard C++; MSVC's advice to use its _s variants
    # is not portable.
    set(OPENGOLD_WARNING_FLAGS /W4 /permissive- /D_CRT_SECURE_NO_WARNINGS)
else()
    set(OPENGOLD_WARNING_FLAGS -Wall -Wextra -Wno-missing-field-initializers)
    # Clang can also report the 64-to-32-bit narrowing MSVC's /W4 warns about.
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        list(APPEND OPENGOLD_WARNING_FLAGS -Wshorten-64-to-32)
    endif()
    # GCC 14, which builds the Linux package for players (tools/linux_release_build.sh),
    # reports std::optional members read behind a has_value() check as maybe
    # uninitialized. GCC 15 and later check the same code without these false alarms.
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 15)
        list(APPEND OPENGOLD_WARNING_FLAGS -Wno-maybe-uninitialized)
    endif()
endif()
function(opengold_enable_warnings directory)
    get_property(targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
    foreach(target IN LISTS targets)
        get_target_property(type ${target} TYPE)
        if(type MATCHES "^(EXECUTABLE|STATIC_LIBRARY|SHARED_LIBRARY|MODULE_LIBRARY|OBJECT_LIBRARY)$")
            target_compile_options(${target} PRIVATE ${OPENGOLD_WARNING_FLAGS})
            set_target_properties(${target} PROPERTIES COMPILE_WARNING_AS_ERROR ON)
        endif()
    endforeach()
endfunction()

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

add_library(opengold_rules STATIC src/OpenGold.Rules/src/rules.cpp src/OpenGold.Rules/src/character_rules.cpp
    src/OpenGold.Rules/src/movement_grid.cpp)
target_include_directories(opengold_rules PUBLIC src/OpenGold.Rules/include)
add_library(opengold_rules_srd5 STATIC
    src/OpenGold.Rules.Srd5/src/srd5.cpp
    src/OpenGold.Rules.Srd5/src/character_rules.cpp
    src/OpenGold.Rules.Srd5/src/character_class.cpp
    src/OpenGold.Rules.Srd5/src/weapon_mastery.cpp
    src/OpenGold.Rules.Srd5/src/feature_grants.cpp
    src/OpenGold.Rules.Srd5/src/training.cpp
    src/OpenGold.Rules.Srd5/src/spell_access.cpp
    src/OpenGold.Rules.Srd5/src/sheet_details.cpp
    src/OpenGold.Rules.Srd5/src/status_effects.cpp
    src/OpenGold.Rules.Srd5/src/life_cycle.cpp
    src/OpenGold.Rules.Srd5/src/damage.cpp
    src/OpenGold.Rules.Srd5/src/recovery_timeline.cpp
)
target_include_directories(opengold_rules_srd5 PUBLIC src/OpenGold.Rules.Srd5/include)
target_link_libraries(opengold_rules_srd5 PUBLIC opengold_rules)
add_library(opengold_rules_original STATIC
    src/OpenGold.Rules.Original/src/original_rules.cpp
    src/OpenGold.Rules.Original/src/character_rules.cpp
    src/OpenGold.Rules.Original/src/combatant.cpp
    src/OpenGold.Rules.Original/src/spells.cpp
    src/OpenGold.Rules.Original/src/combat_session.cpp
    src/OpenGold.Rules.Original/src/rules_module.cpp
)
target_include_directories(opengold_rules_original PUBLIC src/OpenGold.Rules.Original/include)
# The original's monster records come decoded from the Formats library.
target_link_libraries(opengold_rules_original PUBLIC opengold_rules opengold_formats)

add_library(opengold_formats STATIC
    src/OpenGold.Formats/src/formats.cpp
    src/OpenGold.Formats/src/gateway_formats.cpp
    src/OpenGold.Formats/src/pool_radiance.cpp
    src/OpenGold.Formats/src/geo_map.cpp
    src/OpenGold.Formats/src/ecl.cpp
    src/OpenGold.Formats/src/wall_art.cpp
    src/OpenGold.Formats/src/por_sound.cpp
    src/OpenGold.Formats/src/curse_records.cpp
    src/OpenGold.Formats/src/secret_records.cpp
    src/OpenGold.Formats/src/treasures_texts.cpp
    src/OpenGold.Formats/src/vga_picture.cpp
    src/OpenGold.Formats/src/late_records.cpp
    src/OpenGold.Formats/src/vace_film.cpp
    src/OpenGold.Formats/src/music_bank.cpp
    src/OpenGold.Formats/src/gold_box_game.cpp
    src/OpenGold.Formats/src/ua_formats.cpp
    src/OpenGold.Formats/src/ua_media.cpp
)

target_include_directories(opengold_formats
    PUBLIC
        src/OpenGold.Formats/include
)

add_library(opengold_core STATIC
    src/OpenGold.Core/src/character_art.cpp
    src/OpenGold.Core/src/combat_sprite_layers.cpp
    src/OpenGold.Core/src/combat_body_catalog.cpp
    src/OpenGold.Core/src/character_creator.cpp
    src/OpenGold.Core/src/character_pool.cpp
    src/OpenGold.Core/src/companions.cpp
    src/OpenGold.Core/src/default_party.cpp
    src/OpenGold.Core/src/overland.cpp
    src/OpenGold.Core/src/character.cpp
    src/OpenGold.Core/src/campaign_party.cpp
    src/OpenGold.Core/src/ecl_party_host.cpp
    src/OpenGold.Core/src/npc_portraits.cpp
    src/OpenGold.Core/src/shop_data.cpp
    src/OpenGold.Core/src/shop_sale.cpp
    src/OpenGold.Core/src/encounter_data.cpp
    src/OpenGold.Core/src/json_document.cpp
    src/OpenGold.Core/src/creature_art.cpp src/OpenGold.Core/src/monster_bundle.cpp
    src/OpenGold.Core/src/coin_purse.cpp
    src/OpenGold.Core/src/diagnostic_log.cpp
    src/OpenGold.Core/src/training_hall.cpp
    src/OpenGold.Core/src/campaign_rest.cpp
    src/OpenGold.Core/src/campaign_save.cpp
    src/OpenGold.Core/src/save_file.cpp
    src/OpenGold.Core/src/inventory.cpp
    src/OpenGold.Core/src/creature_catalog.cpp
    src/OpenGold.Core/src/creature_factory.cpp
    src/OpenGold.Core/src/map_catalog.cpp
    src/OpenGold.Core/src/game_census.cpp
    src/OpenGold.Core/src/ua_census.cpp
    src/OpenGold.Core/src/ua_design.cpp
    src/OpenGold.Core/src/ua_view.cpp
    src/OpenGold.Core/src/ua_session.cpp
    src/OpenGold.Core/src/ua_events.cpp
    src/OpenGold.Core/src/ecl_address.cpp
    src/OpenGold.Core/src/area_loading.cpp
    src/OpenGold.Core/src/campaign_session.cpp
    src/OpenGold.Core/src/original_content.cpp
    src/OpenGold.Core/src/ecl_machine.cpp
    src/OpenGold.Core/src/por_effects.cpp
    src/OpenGold.Core/src/rolf_tour.cpp
    src/OpenGold.Core/src/phlan_session.cpp
    src/OpenGold.Core/src/random_treasure.cpp
    src/OpenGold.Core/src/magic_item_catalog.cpp
    src/OpenGold.Core/src/encounter_budget.cpp
    src/OpenGold.Core/src/campaign_encounter.cpp
    src/OpenGold.Core/src/combat_demo.cpp
    src/OpenGold.Core/src/dungeon_battlefield.cpp
    src/OpenGold.Core/src/encounter_formation.cpp
    src/OpenGold.Core/src/exploration_view.cpp
    src/OpenGold.Core/src/wall_art_replacements.cpp
    src/OpenGold.Core/src/speaker_audio.cpp
    src/OpenGold.Core/src/sound_bank.cpp
    src/OpenGold.Core/src/sound_player.cpp
    src/OpenGold.Core/src/fm_synth.cpp
    src/OpenGold.Core/src/music_render.cpp
    src/OpenGold.Core/src/title_screens.cpp
)

target_include_directories(opengold_core
    PUBLIC
        src/OpenGold.Core/include
)

# The diagnostic log's std::call_once and std::mutex need libpthread on glibc
# before 2.34, where the Linux package for players is built; without it,
# call_once throws "Unknown error -1" at run time instead of failing to link.
find_package(Threads REQUIRED)
target_link_libraries(opengold_core
    PUBLIC
        opengold_formats
        opengold_rules
        opengold_rules_original
        Threads::Threads
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
    add_executable(opengold_original_rules_tests tests/original_rules_tests.cpp)
    target_link_libraries(opengold_original_rules_tests PRIVATE opengold_rules_original)
    add_test(NAME opengold_original_rules_tests COMMAND opengold_original_rules_tests)
    add_executable(opengold_original_creation_tests tests/original_creation_tests.cpp)
    target_link_libraries(opengold_original_creation_tests PRIVATE opengold_rules_original
        opengold_core)
    add_test(NAME opengold_original_creation_tests COMMAND opengold_original_creation_tests)
    add_executable(opengold_original_combat_tests tests/original_combat_tests.cpp)
    target_link_libraries(opengold_original_combat_tests PRIVATE opengold_rules_original opengold_core)
    add_test(NAME opengold_original_combat_tests COMMAND opengold_original_combat_tests)

    add_executable(opengold_gold_box_games_tests tests/gold_box_games_tests.cpp)
    target_link_libraries(opengold_gold_box_games_tests PRIVATE opengold_core opengold_rules_srd5
        opengold_rules_original)
    target_compile_definitions(opengold_gold_box_games_tests PRIVATE
        OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_gold_box_games_tests COMMAND opengold_gold_box_games_tests)
    # Play with Default Party: each game's default parties, under both rule sets.
    add_executable(opengold_default_party_tests tests/default_party_tests.cpp)
    target_link_libraries(opengold_default_party_tests PRIVATE opengold_core opengold_rules_srd5
        opengold_rules_original)
    target_compile_definitions(opengold_default_party_tests PRIVATE
        OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_default_party_tests COMMAND opengold_default_party_tests)
    # Gateway to the Savage Frontier's campaign, played on the real files.
    add_executable(opengold_gateway_tests tests/gateway_tests.cpp)
    target_link_libraries(opengold_gateway_tests PRIVATE opengold_core opengold_rules_srd5
        opengold_rules_original)
    add_test(NAME opengold_gateway_tests COMMAND opengold_gateway_tests)
    # Every map of Curse walked with an original-rules party: each event runs.
    add_test(NAME opengold_curse_walk COMMAND opengold_gold_box_games_tests --curse-walk)
    # Every map of Secret of the Silver Blades walked the same way.
    add_test(NAME opengold_secret_walk COMMAND opengold_gold_box_games_tests --secret-walk)
    # And every map of Gateway to the Savage Frontier, its overland too.
    add_test(NAME opengold_gateway_walk COMMAND opengold_gold_box_games_tests --gateway-walk)
    # Curse's story, beat by beat, with a party made for it (plan step 7).
    add_test(NAME opengold_curse_route COMMAND opengold_gold_box_games_tests --curse-route)
    # Secret of the Silver Blades' story the same way (plan phase 4, step 4).
    add_test(NAME opengold_secret_route COMMAND opengold_gold_box_games_tests --secret-route)
    # Each game's title pictures, as its original shows them when it starts.
    add_executable(opengold_title_screens_tests tests/title_screens_tests.cpp)
    target_link_libraries(opengold_title_screens_tests PRIVATE opengold_core)
    add_test(NAME opengold_title_screens_tests COMMAND opengold_title_screens_tests)
    set_tests_properties(opengold_title_screens_tests PROPERTIES SKIP_REGULAR_EXPRESSION "Skipped: ")
    # Pools of Darkness' and Treasures' 256-color pictures, read from the games' files.
    add_executable(opengold_vga_graphics_tests tests/vga_graphics_tests.cpp)
    target_link_libraries(opengold_vga_graphics_tests PRIVATE opengold_formats)
    add_test(NAME opengold_vga_graphics_tests COMMAND opengold_vga_graphics_tests)
    set_tests_properties(opengold_vga_graphics_tests PROPERTIES SKIP_REGULAR_EXPRESSION "Skipped: ")
    # Their scripts, creatures and items.
    add_executable(opengold_late_records_tests tests/late_records_tests.cpp)
    target_link_libraries(opengold_late_records_tests PRIVATE opengold_formats)
    add_test(NAME opengold_late_records_tests COMMAND opengold_late_records_tests)
    # Pools of Darkness' opening and ending films, read from the game's files.
    add_executable(opengold_vace_film_tests tests/vace_film_tests.cpp)
    target_link_libraries(opengold_vace_film_tests PRIVATE opengold_formats)
    add_test(NAME opengold_vace_film_tests COMMAND opengold_vace_film_tests)
    set_tests_properties(opengold_vace_film_tests PROPERTIES SKIP_REGULAR_EXPRESSION "Skipped: ")
    add_test(NAME opengold_game_census COMMAND "${CMAKE_COMMAND}"
        "-DCENSUS=$<TARGET_FILE:opengold_census>"
        -P "${PROJECT_SOURCE_DIR}/tests/game_census.cmake")
    set_tests_properties(opengold_game_census PROPERTIES TIMEOUT 300
        SKIP_REGULAR_EXPRESSION "Skipped: ")
    # What Gateway to the Savage Frontier's files changed (docs/games/gateway.md).
    add_executable(opengold_gateway_census_tests tests/gateway_census_tests.cpp)
    target_link_libraries(opengold_gateway_census_tests PRIVATE opengold_core)
    add_test(NAME opengold_gateway_census_tests COMMAND opengold_gateway_census_tests)
    # The later games' music and digitized sounds (docs/games/later-sound.md).
    add_executable(opengold_late_sound_tests tests/late_sound_tests.cpp)
    target_link_libraries(opengold_late_sound_tests PRIVATE opengold_core)
    add_test(NAME opengold_late_sound_tests COMMAND opengold_late_sound_tests)
    # Unlimited Adventures' libraries, glyphs, records, sounds and music (docs/games/ua.md).
    add_executable(opengold_ua_census_tests tests/ua_census_tests.cpp)
    target_link_libraries(opengold_ua_census_tests PRIVATE opengold_core)
    add_test(NAME opengold_ua_census_tests COMMAND opengold_ua_census_tests)
    # Playing a design made with Unlimited Adventures' kit (docs/games/ua.md).
    add_executable(opengold_ua_session_tests tests/ua_session_tests.cpp)
    target_link_libraries(opengold_ua_session_tests PRIVATE opengold_core)
    add_test(NAME opengold_ua_session_tests COMMAND opengold_ua_session_tests)
    # Every square of every area of the test design walked, each event and fight played.
    add_test(NAME opengold_ua_walk COMMAND opengold_ua_session_tests --ua-walk)
    set_tests_properties(opengold_ua_session_tests opengold_ua_walk PROPERTIES
        SKIP_REGULAR_EXPRESSION "Skipped: ")

    add_executable(opengold_concentration_tests tests/concentration_tests.cpp)
    target_include_directories(opengold_concentration_tests PRIVATE src/OpenGold.Rules.Srd5/src)
    target_link_libraries(opengold_concentration_tests PRIVATE opengold_rules_srd5)
    add_test(NAME opengold_concentration_tests COMMAND opengold_concentration_tests)

    add_executable(opengold_status_effect_tests tests/status_effect_tests.cpp)
    target_include_directories(opengold_status_effect_tests PRIVATE src/OpenGold.Rules.Srd5/src)
    target_link_libraries(opengold_status_effect_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_status_effect_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_status_effect_tests COMMAND opengold_status_effect_tests)
    # It also writes the Weapon Mastery, Nick, Graze and optional mastery screen fixtures.
    set(mastery_fixture_environment
        "OPENGOLD_MASTERY_FIXTURES=${PROJECT_BINARY_DIR}/mastery-fixtures"
        "OPENGOLD_NICK_FIXTURES=${PROJECT_BINARY_DIR}/nick-fixtures"
        "OPENGOLD_GRAZE_FIXTURES=${PROJECT_BINARY_DIR}/graze-fixtures"
        "OPENGOLD_OPTIONAL_MASTERY_FIXTURES=${PROJECT_BINARY_DIR}/optional-mastery-fixtures")
    set_tests_properties(opengold_status_effect_tests PROPERTIES FIXTURES_SETUP mastery_fixtures
        ENVIRONMENT "${mastery_fixture_environment}")
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
    target_compile_definitions(opengold_advancement_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}" OPENGOLD_BINARY_DIR="${PROJECT_BINARY_DIR}")
    add_test(NAME opengold_advancement_tests COMMAND opengold_advancement_tests)
    # With original files it also writes the ASI and Skilled screen fixtures.
    set_tests_properties(opengold_advancement_tests PROPERTIES FIXTURES_SETUP asi_fixtures)
    add_executable(opengold_feature_grant_tests tests/feature_grant_tests.cpp)
    target_link_libraries(opengold_feature_grant_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_feature_grant_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_feature_grant_tests COMMAND opengold_feature_grant_tests)
    # With original files it also writes the pending mastery rest screen fixture.
    set_tests_properties(opengold_feature_grant_tests PROPERTIES FIXTURES_SETUP mastery_rest_fixture
        ENVIRONMENT "OPENGOLD_MASTERY_REST_FIXTURE=${PROJECT_BINARY_DIR}/mastery-rest-pending.ogs")
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
    add_executable(opengold_paladin_spell_tests tests/paladin_spell_tests.cpp)
    target_link_libraries(opengold_paladin_spell_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_paladin_spell_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}" OPENGOLD_BINARY_DIR="${PROJECT_BINARY_DIR}")
    add_test(NAME opengold_paladin_spell_tests COMMAND opengold_paladin_spell_tests)
    set_tests_properties(opengold_paladin_spell_tests PROPERTIES FIXTURES_SETUP bless_fixtures)
    add_executable(opengold_smite_tests tests/smite_tests.cpp)
    target_link_libraries(opengold_smite_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_smite_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}" OPENGOLD_BINARY_DIR="${PROJECT_BINARY_DIR}")
    add_test(NAME opengold_smite_tests COMMAND opengold_smite_tests)
    set_tests_properties(opengold_smite_tests PROPERTIES FIXTURES_SETUP smite_fixtures)
    add_executable(opengold_lay_on_hands_tests tests/lay_on_hands_tests.cpp)
    target_include_directories(opengold_lay_on_hands_tests PRIVATE src/OpenGold.Rules.Srd5/src)
    target_link_libraries(opengold_lay_on_hands_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_lay_on_hands_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}" OPENGOLD_BINARY_DIR="${PROJECT_BINARY_DIR}")
    add_test(NAME opengold_lay_on_hands_tests COMMAND opengold_lay_on_hands_tests)
    set_tests_properties(opengold_lay_on_hands_tests PROPERTIES FIXTURES_SETUP lay_fixtures)
    add_executable(opengold_bard_tests tests/bard_tests.cpp)
    target_include_directories(opengold_bard_tests PRIVATE src/OpenGold.Rules.Srd5/src)
    target_link_libraries(opengold_bard_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_bard_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_bard_tests COMMAND opengold_bard_tests)
    add_executable(opengold_playtest_fixtures tests/playtest_fixtures.cpp)
    target_link_libraries(opengold_playtest_fixtures PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_playtest_fixtures PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}" OPENGOLD_BINARY_DIR="${PROJECT_BINARY_DIR}")
    add_test(NAME opengold_playtest_fixtures COMMAND opengold_playtest_fixtures)
    set_tests_properties(opengold_playtest_fixtures PROPERTIES FIXTURES_SETUP playtest_fixtures)
    add_executable(opengold_fire_and_acid_tests tests/fire_and_acid_tests.cpp)
    target_include_directories(opengold_fire_and_acid_tests PRIVATE src/OpenGold.Rules.Srd5/src)
    target_link_libraries(opengold_fire_and_acid_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_fire_and_acid_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_fire_and_acid_tests COMMAND opengold_fire_and_acid_tests)
    add_executable(opengold_slums_creatures_tests tests/slums_creatures_tests.cpp)
    target_link_libraries(opengold_slums_creatures_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_slums_creatures_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_slums_creatures_tests COMMAND opengold_slums_creatures_tests)
    add_executable(opengold_kutos_well_tests tests/kutos_well_tests.cpp)
    target_link_libraries(opengold_kutos_well_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_kutos_well_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_kutos_well_tests COMMAND opengold_kutos_well_tests)
    add_executable(opengold_footprint_tests tests/footprint_tests.cpp)
    target_link_libraries(opengold_footprint_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_footprint_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_footprint_tests COMMAND opengold_footprint_tests)
    add_executable(opengold_druid_tests tests/druid_tests.cpp)
    target_include_directories(opengold_druid_tests PRIVATE src/OpenGold.Rules.Srd5/src)
    target_link_libraries(opengold_druid_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_druid_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_druid_tests COMMAND opengold_druid_tests)
    add_executable(opengold_warlock_tests tests/warlock_tests.cpp)
    target_link_libraries(opengold_warlock_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_warlock_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_warlock_tests COMMAND opengold_warlock_tests)
    add_executable(opengold_sorcerer_tests tests/sorcerer_tests.cpp)
    target_include_directories(opengold_sorcerer_tests PRIVATE src/OpenGold.Rules.Srd5/src)
    target_link_libraries(opengold_sorcerer_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_sorcerer_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_sorcerer_tests COMMAND opengold_sorcerer_tests)
    add_executable(opengold_monk_tests tests/monk_tests.cpp)
    target_link_libraries(opengold_monk_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_monk_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_monk_tests COMMAND opengold_monk_tests)
    add_executable(opengold_barbarian_tests tests/barbarian_tests.cpp)
    target_include_directories(opengold_barbarian_tests PRIVATE src/OpenGold.Rules.Srd5/src)
    target_link_libraries(opengold_barbarian_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_barbarian_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_barbarian_tests COMMAND opengold_barbarian_tests)
    add_executable(opengold_wizard_spell_tests tests/wizard_spell_tests.cpp)
    target_link_libraries(opengold_wizard_spell_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_wizard_spell_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}" OPENGOLD_BINARY_DIR="${PROJECT_BINARY_DIR}")
    add_test(NAME opengold_wizard_spell_tests COMMAND opengold_wizard_spell_tests)
    set_tests_properties(opengold_wizard_spell_tests PROPERTIES FIXTURES_SETUP wizard_fixtures)
    add_executable(opengold_cleric_channel_tests tests/cleric_channel_tests.cpp)
    target_link_libraries(opengold_cleric_channel_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_cleric_channel_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}" OPENGOLD_BINARY_DIR="${PROJECT_BINARY_DIR}")
    add_test(NAME opengold_cleric_channel_tests COMMAND opengold_cleric_channel_tests)
    set_tests_properties(opengold_cleric_channel_tests PROPERTIES FIXTURES_SETUP cleric_fixtures)
    add_executable(opengold_ranger_spell_tests tests/ranger_spell_tests.cpp)
    target_link_libraries(opengold_ranger_spell_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_ranger_spell_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}" OPENGOLD_BINARY_DIR="${PROJECT_BINARY_DIR}")
    add_test(NAME opengold_ranger_spell_tests COMMAND opengold_ranger_spell_tests)
    set_tests_properties(opengold_ranger_spell_tests PROPERTIES FIXTURES_SETUP ranger_fixtures)
    add_executable(opengold_camp_action_tests tests/camp_action_tests.cpp)
    target_include_directories(opengold_camp_action_tests PRIVATE src/OpenGold.Rules.Srd5/src)
    target_link_libraries(opengold_camp_action_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_camp_action_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_camp_action_tests COMMAND opengold_camp_action_tests)
    add_executable(opengold_inflict_wounds_tests tests/inflict_wounds_tests.cpp)
    target_link_libraries(opengold_inflict_wounds_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_inflict_wounds_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}" OPENGOLD_BINARY_DIR="${PROJECT_BINARY_DIR}")
    add_test(NAME opengold_inflict_wounds_tests COMMAND opengold_inflict_wounds_tests)
    set_tests_properties(opengold_inflict_wounds_tests PROPERTIES FIXTURES_SETUP inflict_fixtures)
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
    add_executable(opengold_monk_levels_tests tests/monk_levels_tests.cpp)
    target_link_libraries(opengold_monk_levels_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_monk_levels_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_monk_levels_tests COMMAND opengold_monk_levels_tests)
    add_executable(opengold_paladin_levels_tests tests/paladin_levels_tests.cpp)
    target_link_libraries(opengold_paladin_levels_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_paladin_levels_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_paladin_levels_tests COMMAND opengold_paladin_levels_tests)
    add_executable(opengold_cleric_levels_tests tests/cleric_levels_tests.cpp)
    target_include_directories(opengold_cleric_levels_tests PRIVATE src/OpenGold.Rules.Srd5/src)
    target_link_libraries(opengold_cleric_levels_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_cleric_levels_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_cleric_levels_tests COMMAND opengold_cleric_levels_tests)
    add_executable(opengold_sorcerer_levels_tests tests/sorcerer_levels_tests.cpp)
    target_link_libraries(opengold_sorcerer_levels_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_sorcerer_levels_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_sorcerer_levels_tests COMMAND opengold_sorcerer_levels_tests)
    add_executable(opengold_warlock_levels_tests tests/warlock_levels_tests.cpp)
    target_link_libraries(opengold_warlock_levels_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_warlock_levels_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_warlock_levels_tests COMMAND opengold_warlock_levels_tests)
    add_executable(opengold_bard_levels_tests tests/bard_levels_tests.cpp)
    target_link_libraries(opengold_bard_levels_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_bard_levels_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}" OPENGOLD_BINARY_DIR="${PROJECT_BINARY_DIR}")
    add_test(NAME opengold_bard_levels_tests COMMAND opengold_bard_levels_tests)
    # It writes Compulsion's combat-window fixture, and with original files the
    # Magical Discoveries swap screen's.
    set_tests_properties(opengold_bard_levels_tests PROPERTIES
        FIXTURES_SETUP "discovery_ui_fixtures;compulsion_fixtures")
    add_executable(opengold_druid_levels_tests tests/druid_levels_tests.cpp)
    target_link_libraries(opengold_druid_levels_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_druid_levels_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_druid_levels_tests COMMAND opengold_druid_levels_tests)
    add_executable(opengold_wizard_levels_tests tests/wizard_levels_tests.cpp)
    target_link_libraries(opengold_wizard_levels_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_wizard_levels_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_wizard_levels_tests COMMAND opengold_wizard_levels_tests)
    add_executable(opengold_wall_art_replacements_tests tests/wall_art_replacements_tests.cpp)
    target_link_libraries(opengold_wall_art_replacements_tests PRIVATE opengold_core)
    target_compile_definitions(opengold_wall_art_replacements_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_wall_art_replacements_tests COMMAND opengold_wall_art_replacements_tests)
    add_executable(opengold_monster_bundle_tests tests/monster_bundle_tests.cpp)
    target_link_libraries(opengold_monster_bundle_tests PRIVATE opengold_core)
    target_compile_definitions(opengold_monster_bundle_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_monster_bundle_tests COMMAND opengold_monster_bundle_tests)
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME opengold_session_board_tests
                 COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/session_board_tests.py)
        add_test(NAME opengold_release_tool_tests
                 COMMAND ${Python3_EXECUTABLE} -B ${PROJECT_SOURCE_DIR}/tests/release_tool_tests.py)
    endif()
    add_executable(opengold_creature_edits_tests tests/creature_edits_tests.cpp)
    target_link_libraries(opengold_creature_edits_tests PRIVATE opengold_creature_edits)
    target_compile_definitions(opengold_creature_edits_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_creature_edits_tests COMMAND opengold_creature_edits_tests)
    add_executable(opengold_creature_data_tests tests/creature_data_tests.cpp)
    target_link_libraries(opengold_creature_data_tests PRIVATE opengold_creature_data)
    target_compile_definitions(opengold_creature_data_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_creature_data_tests COMMAND opengold_creature_data_tests)
    set_tests_properties(opengold_creature_data_tests PROPERTIES SKIP_REGULAR_EXPRESSION "Skipped: ")
    add_executable(opengold_companion_data_tests tests/companion_data_tests.cpp)
    target_link_libraries(opengold_companion_data_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_companion_data_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_companion_data_tests COMMAND opengold_companion_data_tests)
    add_executable(opengold_encounter_data_tests tests/encounter_data_tests.cpp)
    target_link_libraries(opengold_encounter_data_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_encounter_data_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_encounter_data_tests COMMAND opengold_encounter_data_tests)
    add_executable(opengold_shop_data_tests tests/shop_data_tests.cpp)
    target_link_libraries(opengold_shop_data_tests PRIVATE opengold_core)
    target_compile_definitions(opengold_shop_data_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_shop_data_tests COMMAND opengold_shop_data_tests)
    add_executable(opengold_temple_tests tests/temple_tests.cpp)
    target_include_directories(opengold_temple_tests PRIVATE src/OpenGold.Rules.Srd5/src)
    target_link_libraries(opengold_temple_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_temple_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_temple_tests COMMAND opengold_temple_tests)
    add_executable(opengold_word_of_recall_tests tests/word_of_recall_tests.cpp)
    target_link_libraries(opengold_word_of_recall_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_word_of_recall_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_word_of_recall_tests COMMAND opengold_word_of_recall_tests)
    add_executable(opengold_rogue_shop_tests tests/rogue_shop_tests.cpp)
    target_link_libraries(opengold_rogue_shop_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_rogue_shop_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_rogue_shop_tests COMMAND opengold_rogue_shop_tests)
    add_executable(opengold_ranger_levels_tests tests/ranger_levels_tests.cpp)
    target_link_libraries(opengold_ranger_levels_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_ranger_levels_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_ranger_levels_tests COMMAND opengold_ranger_levels_tests)
    add_executable(opengold_rogue_levels_tests tests/rogue_levels_tests.cpp)
    target_link_libraries(opengold_rogue_levels_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_rogue_levels_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_rogue_levels_tests COMMAND opengold_rogue_levels_tests)
    add_executable(opengold_barbarian_levels_tests tests/barbarian_levels_tests.cpp)
    target_link_libraries(opengold_barbarian_levels_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_barbarian_levels_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_barbarian_levels_tests COMMAND opengold_barbarian_levels_tests)
    add_executable(opengold_fighter_levels_tests tests/fighter_levels_tests.cpp)
    target_link_libraries(opengold_fighter_levels_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_fighter_levels_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_fighter_levels_tests COMMAND opengold_fighter_levels_tests)
    add_executable(opengold_champion_tests tests/champion_tests.cpp)
    target_include_directories(opengold_champion_tests PRIVATE src/OpenGold.Rules.Srd5/src)
    target_link_libraries(opengold_champion_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_champion_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}" OPENGOLD_BINARY_DIR="${PROJECT_BINARY_DIR}")
    add_test(NAME opengold_champion_tests COMMAND opengold_champion_tests)
    set_tests_properties(opengold_champion_tests PROPERTIES FIXTURES_SETUP champion_fixtures)
    add_executable(opengold_flee_tests tests/flee_tests.cpp)
    target_link_libraries(opengold_flee_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_flee_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_flee_tests COMMAND opengold_flee_tests)
    add_executable(opengold_combat_rollback_tests tests/combat_rollback_tests.cpp
        tests/failing_allocation.cpp)
    target_link_libraries(opengold_combat_rollback_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_combat_rollback_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_combat_rollback_tests COMMAND opengold_combat_rollback_tests)
    add_executable(opengold_quick_tests tests/quick_tests.cpp)
    target_link_libraries(opengold_quick_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_quick_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_quick_tests COMMAND opengold_quick_tests)
    add_executable(opengold_ai_policy_tests tests/ai_policy_tests.cpp)
    target_link_libraries(opengold_ai_policy_tests PRIVATE opengold_core)
    add_test(NAME opengold_ai_policy_tests COMMAND opengold_ai_policy_tests)
    add_executable(opengold_thrown_weapon_tests tests/thrown_weapon_tests.cpp)
    target_link_libraries(opengold_thrown_weapon_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_thrown_weapon_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}" OPENGOLD_BINARY_DIR="${PROJECT_BINARY_DIR}")
    add_test(NAME opengold_thrown_weapon_tests COMMAND opengold_thrown_weapon_tests)
    set_tests_properties(opengold_thrown_weapon_tests PROPERTIES FIXTURES_SETUP thrown_fixtures)
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
    add_executable(opengold_spell_effects_tests tests/spell_effects_tests.cpp)
    target_include_directories(opengold_spell_effects_tests PRIVATE src/OpenGold.Rules.Srd5/src)
    target_link_libraries(opengold_spell_effects_tests PRIVATE opengold_core opengold_rules_srd5)
    add_test(NAME opengold_spell_effects_tests COMMAND opengold_spell_effects_tests)
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
    # With original files it also writes the Fighting Style, Rogue and Scholar
    # level-up screen fixtures; it always writes the Alert and Light ones.
    set_tests_properties(opengold_training_tests PROPERTIES
        FIXTURES_SETUP "cunning_fixtures;advancement_ui_fixtures;alert_fixtures;light_fixtures")
    add_test(NAME opengold_hands_fixture COMMAND opengold_training_tests --hands-ui)
    set_tests_properties(opengold_hands_fixture PROPERTIES FIXTURES_SETUP hands_fixtures)
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
    add_executable(opengold_encounter_formation_tests tests/encounter_formation_tests.cpp)
    target_link_libraries(opengold_encounter_formation_tests PRIVATE opengold_core)
    add_test(NAME opengold_encounter_formation_tests COMMAND opengold_encounter_formation_tests)
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
    add_executable(opengold_wilderness_field_tests tests/wilderness_field_tests.cpp)
    target_link_libraries(opengold_wilderness_field_tests PRIVATE opengold_core)
    add_test(NAME opengold_wilderness_field_tests COMMAND opengold_wilderness_field_tests)
    add_executable(opengold_tour_tests tests/tour_tests.cpp)
    target_link_libraries(opengold_tour_tests PRIVATE opengold_core)
    target_compile_definitions(opengold_tour_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}" OPENGOLD_BINARY_DIR="${PROJECT_BINARY_DIR}")
    add_test(NAME opengold_tour_tests COMMAND opengold_tour_tests)
    add_executable(opengold_savage_attacker_tests tests/savage_attacker_tests.cpp)
    target_link_libraries(opengold_savage_attacker_tests PRIVATE opengold_rules_srd5 opengold_core)
    target_compile_definitions(opengold_savage_attacker_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_savage_attacker_tests COMMAND opengold_savage_attacker_tests)

    add_executable(opengold_spell_access_tests tests/spell_access_tests.cpp)
    target_link_libraries(opengold_spell_access_tests PRIVATE opengold_rules_srd5 opengold_core)
    target_compile_definitions(opengold_spell_access_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}" OPENGOLD_BINARY_DIR="${PROJECT_BINARY_DIR}")
    add_test(NAME opengold_spell_access_tests COMMAND opengold_spell_access_tests)
    # With original files it also writes the Wizard, Cleric and Blessed Warrior
    # level-up screen fixtures.
    set_tests_properties(opengold_spell_access_tests PROPERTIES
        FIXTURES_SETUP spell_choice_fixtures)

    add_executable(opengold_magic_initiate_tests tests/magic_initiate_tests.cpp)
    target_link_libraries(opengold_magic_initiate_tests PRIVATE opengold_rules_srd5 opengold_core)
    target_compile_definitions(opengold_magic_initiate_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_magic_initiate_tests COMMAND opengold_magic_initiate_tests)
    add_executable(opengold_training_hall_tests tests/training_hall_tests.cpp)
    target_link_libraries(opengold_training_hall_tests PRIVATE opengold_rules_srd5 opengold_core)
    target_compile_definitions(opengold_training_hall_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_training_hall_tests COMMAND opengold_training_hall_tests)
    add_executable(opengold_attained_levels_tests tests/attained_levels_tests.cpp)
    target_link_libraries(opengold_attained_levels_tests PRIVATE opengold_rules_srd5 opengold_core)
    target_compile_definitions(opengold_attained_levels_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_attained_levels_tests COMMAND opengold_attained_levels_tests)
    add_executable(opengold_healing_spell_tests tests/healing_spell_tests.cpp)
    target_link_libraries(opengold_healing_spell_tests PRIVATE opengold_rules_srd5 opengold_core)
    target_compile_definitions(opengold_healing_spell_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_healing_spell_tests COMMAND opengold_healing_spell_tests)
    add_executable(opengold_level_swap_tests tests/level_swap_tests.cpp)
    target_link_libraries(opengold_level_swap_tests PRIVATE opengold_rules_srd5 opengold_core)
    target_compile_definitions(opengold_level_swap_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_level_swap_tests COMMAND opengold_level_swap_tests)
    add_executable(opengold_hide_tests tests/hide_tests.cpp)
    target_include_directories(opengold_hide_tests PRIVATE src/OpenGold.Rules.Srd5/src)
    target_link_libraries(opengold_hide_tests PRIVATE opengold_rules_srd5 opengold_core)
    target_compile_definitions(opengold_hide_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_hide_tests COMMAND opengold_hide_tests)
    add_executable(opengold_species_tests tests/species_tests.cpp)
    target_include_directories(opengold_species_tests PRIVATE src/OpenGold.Rules.Srd5/src)
    target_link_libraries(opengold_species_tests PRIVATE opengold_rules_srd5 opengold_core)
    target_compile_definitions(opengold_species_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_species_tests COMMAND opengold_species_tests)

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

    add_executable(opengold_enchanted_item_tests tests/enchanted_item_tests.cpp)
    target_link_libraries(opengold_enchanted_item_tests PRIVATE opengold_rules_srd5 opengold_core)
    target_compile_definitions(opengold_enchanted_item_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_enchanted_item_tests COMMAND opengold_enchanted_item_tests)
    add_executable(opengold_auto_equip_tests tests/auto_equip_tests.cpp)
    target_link_libraries(opengold_auto_equip_tests PRIVATE opengold_rules_srd5 opengold_core)
    target_compile_definitions(opengold_auto_equip_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_auto_equip_tests COMMAND opengold_auto_equip_tests)
    add_executable(opengold_sell_drop_tests tests/sell_drop_tests.cpp)
    target_link_libraries(opengold_sell_drop_tests PRIVATE opengold_rules_srd5 opengold_core)
    target_compile_definitions(opengold_sell_drop_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_sell_drop_tests COMMAND opengold_sell_drop_tests)
    add_executable(opengold_shop_sale_tests tests/shop_sale_tests.cpp)
    target_link_libraries(opengold_shop_sale_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_shop_sale_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_shop_sale_tests COMMAND opengold_shop_sale_tests)
    add_executable(opengold_multiclass_tests tests/multiclass_tests.cpp)
    target_link_libraries(opengold_multiclass_tests PRIVATE opengold_rules_srd5 opengold_core)
    target_compile_definitions(opengold_multiclass_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_multiclass_tests COMMAND opengold_multiclass_tests)
    add_executable(opengold_charged_item_tests tests/charged_item_tests.cpp)
    target_link_libraries(opengold_charged_item_tests PRIVATE opengold_rules_srd5 opengold_core)
    target_compile_definitions(opengold_charged_item_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_charged_item_tests COMMAND opengold_charged_item_tests)
    add_executable(opengold_worn_item_tests tests/worn_item_tests.cpp)
    target_link_libraries(opengold_worn_item_tests PRIVATE opengold_rules_srd5 opengold_core)
    target_compile_definitions(opengold_worn_item_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_worn_item_tests COMMAND opengold_worn_item_tests)
    add_executable(opengold_scroll_tests tests/scroll_tests.cpp)
    target_link_libraries(opengold_scroll_tests PRIVATE opengold_rules_srd5 opengold_core)
    target_compile_definitions(opengold_scroll_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_scroll_tests COMMAND opengold_scroll_tests)

    add_executable(opengold_heavy_weapon_tests tests/heavy_weapon_tests.cpp)
    target_link_libraries(opengold_heavy_weapon_tests PRIVATE opengold_rules_srd5 opengold_core)
    target_compile_definitions(opengold_heavy_weapon_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_heavy_weapon_tests COMMAND opengold_heavy_weapon_tests)

    add_executable(opengold_versatile_tests tests/versatile_tests.cpp)
    target_link_libraries(opengold_versatile_tests PRIVATE opengold_rules_srd5 opengold_core)
    target_compile_definitions(opengold_versatile_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}" OPENGOLD_BINARY_DIR="${PROJECT_BINARY_DIR}")
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
    add_executable(opengold_diagnostic_log_tests tests/diagnostic_log_tests.cpp)
    target_link_libraries(opengold_diagnostic_log_tests PRIVATE opengold_core)
    add_test(NAME opengold_diagnostic_log_tests COMMAND opengold_diagnostic_log_tests)
    add_executable(opengold_party_tests tests/party_tests.cpp tests/failing_allocation.cpp)
    target_link_libraries(opengold_party_tests PRIVATE opengold_core opengold_rules_srd5)
    target_compile_definitions(opengold_party_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_party_tests COMMAND opengold_party_tests)
    add_executable(opengold_expedition_tests tests/expedition_tests.cpp
        src/OpenGoldBox/game_rules.cpp)
    target_include_directories(opengold_expedition_tests PRIVATE src/OpenGold.Rules.Srd5/src
        src/OpenGoldBox)
    target_link_libraries(opengold_expedition_tests PRIVATE opengold_ai_scenario opengold_rules_srd5
        opengold_rules_original)
    target_compile_definitions(opengold_expedition_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_expedition_tests COMMAND opengold_expedition_tests)
    add_test(NAME opengold_encounter_override_tests
        COMMAND opengold_expedition_tests --encounter-overrides)
    # A party made under the original rules fights the Slums' orcs.
    add_test(NAME opengold_original_campaign_tests COMMAND opengold_expedition_tests --original-rules)
    set_tests_properties(opengold_encounter_override_tests PROPERTIES SKIP_REGULAR_EXPRESSION "Skipped: ")
    add_executable(opengold_save_tests tests/save_tests.cpp)
    target_link_libraries(opengold_save_tests PRIVATE opengold_core opengold_rules_srd5
        opengold_rules_original)
    target_compile_definitions(opengold_save_tests PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME opengold_save_tests COMMAND opengold_save_tests)
    add_executable(opengold_encounter_budget_tests tests/encounter_budget_tests.cpp)
    target_link_libraries(opengold_encounter_budget_tests PRIVATE opengold_core)
    add_test(NAME opengold_encounter_budget_tests COMMAND opengold_encounter_budget_tests)
    add_executable(opengold_sound_tests tests/sound_tests.cpp)
    target_link_libraries(opengold_sound_tests PRIVATE opengold_core)
    add_test(NAME opengold_sound_tests COMMAND opengold_sound_tests)
    add_executable(opengold_magic_item_tests tests/magic_item_tests.cpp)
    target_link_libraries(opengold_magic_item_tests PRIVATE opengold_core)
    add_test(NAME opengold_magic_item_tests COMMAND opengold_magic_item_tests)
    add_executable(opengold_sound_player_tests tests/sound_player_tests.cpp)
    target_link_libraries(opengold_sound_player_tests PRIVATE opengold_core)
    add_test(NAME opengold_sound_player_tests COMMAND opengold_sound_player_tests)
    add_executable(opengold_music_render_tests tests/music_render_tests.cpp)
    target_link_libraries(opengold_music_render_tests PRIVATE opengold_core)
    add_test(NAME opengold_music_render_tests COMMAND opengold_music_render_tests)
endif()

add_executable(opengold_creatures tools/creatures.cpp)
target_link_libraries(opengold_creatures PRIVATE opengold_core)
target_include_directories(opengold_creatures PRIVATE src/OpenGold.Rules.Srd5/src)
add_executable(opengold_maps tools/maps.cpp)
target_link_libraries(opengold_maps PRIVATE opengold_core)
# The Creature Stats Editor's importer (dev_tools/docs/creature-stats-editor-interface.md).
add_library(opengold_creature_edits STATIC tools/creature_edits.cpp)
target_link_libraries(opengold_creature_edits PUBLIC opengold_core opengold_rules_srd5)
add_library(opengold_creature_data STATIC tools/creature_data.cpp)
target_link_libraries(opengold_creature_data PUBLIC opengold_creature_edits)
add_executable(opengold_import_edits tools/import_edits.cpp)
target_link_libraries(opengold_import_edits PRIVATE opengold_creature_edits)
target_compile_definitions(opengold_import_edits PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
add_executable(opengold_census tools/census.cpp)
target_link_libraries(opengold_census PRIVATE opengold_core)
add_executable(opengold_scripts tools/scripts.cpp)
target_link_libraries(opengold_scripts PRIVATE opengold_core)
add_executable(opengold_magic_items tools/magic_items.cpp)
target_link_libraries(opengold_magic_items PRIVATE opengold_core)
add_executable(opengold_campaign_sim tools/campaign_sim.cpp)
target_link_libraries(opengold_campaign_sim PRIVATE opengold_core opengold_rules_srd5)
target_compile_definitions(opengold_campaign_sim PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
add_executable(opengold_encounter_balance tools/encounter_balance.cpp)
target_link_libraries(opengold_encounter_balance PRIVATE opengold_core opengold_rules_srd5)
target_compile_definitions(opengold_encounter_balance PRIVATE OPENGOLD_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
# The balance tools stage fights as the game does; a short run keeps them working.
if(OPENGOLD_BUILD_TESTS)
    add_test(NAME opengold_balance_tools_smoke COMMAND "${CMAKE_COMMAND}"
        "-DSIM=$<TARGET_FILE:opengold_campaign_sim>" "-DBALANCE=$<TARGET_FILE:opengold_encounter_balance>"
        "-DWORK=${CMAKE_BINARY_DIR}/balance-tools-smoke"
        -P "${PROJECT_SOURCE_DIR}/tests/balance_tools_smoke.cmake")
    # A debug build takes about six minutes; an optimised one under one.
    set_tests_properties(opengold_balance_tools_smoke PROPERTIES TIMEOUT 900
        SKIP_REGULAR_EXPRESSION "Skipped: ")
endif()
# The game's export refuses to replace a copy of the game that is running,
# which would leave that copy unable to open its screens.
if(OPENGOLD_BUILD_TESTS AND NOT WIN32)
    add_executable(opengold_stand_in_game tests/stand_in_game.cpp)
    add_test(NAME opengold_running_game_guard COMMAND "${CMAKE_COMMAND}"
        "-DSTAND_IN=$<TARGET_FILE:opengold_stand_in_game>"
        "-DGUARD=${PROJECT_SOURCE_DIR}/cmake/refuse_running_game.cmake"
        "-DWORK=${CMAKE_BINARY_DIR}/running-game-guard"
        -P "${PROJECT_SOURCE_DIR}/tests/running_game_guard.cmake")
    set_tests_properties(opengold_running_game_guard PROPERTIES
        PASS_REGULAR_EXPRESSION "Running game guard passed")
endif()

if(OPENGOLD_BUILD_GODOT)
    set_target_properties(opengold_core opengold_formats opengold_rules opengold_rules_srd5 PROPERTIES POSITION_INDEPENDENT_CODE ON)
    add_subdirectory(demos/src/OpenGold.Godot)
    add_subdirectory(dev_tools/tile_art_viewer/native)
    add_subdirectory(dev_tools/combat_sprite_catalog/native)
endif()

if(OPENGOLD_BUILD_GAME)
    # The game library is a shared object; on Linux what it links must be PIC.
    set_target_properties(opengold_core opengold_formats opengold_rules opengold_rules_srd5 PROPERTIES POSITION_INDEPENDENT_CODE ON)
    add_subdirectory(src/OpenGoldBox)
endif()

if(OPENGOLD_BUILD_DOS_PROBE)
    add_subdirectory(dev_tools/dos_probe)
endif()

add_subdirectory(dev_tools/ai_lab)

if(OPENGOLD_BUILD_TESTS)
    add_executable(opengold_posture_tests tests/posture_tests.cpp)
    target_link_libraries(opengold_posture_tests PRIVATE opengold_core opengold_rules_srd5)
    target_include_directories(opengold_posture_tests PRIVATE src/OpenGold.Rules.Srd5/src)
    target_compile_definitions(opengold_posture_tests PRIVATE OPENGOLD_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}" OPENGOLD_BINARY_DIR="${CMAKE_BINARY_DIR}")
    add_test(NAME opengold_posture_tests COMMAND opengold_posture_tests)
    set_tests_properties(opengold_posture_tests PROPERTIES FIXTURES_SETUP posture_fixtures)
endif()

opengold_enable_warnings("${CMAKE_CURRENT_SOURCE_DIR}")
