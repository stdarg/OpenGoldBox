cmake_minimum_required(VERSION 3.25)
project(OpenGoldCombatTester LANGUAGES CXX)

set(OPENGOLD_BUILD_GAME ON CACHE BOOL "" FORCE)
set(OPENGOLD_BUILD_GODOT OFF CACHE BOOL "" FORCE)
set(OPENGOLD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(repo_root "${CMAKE_CURRENT_LIST_DIR}/../..")
add_subdirectory("${repo_root}" "${CMAKE_BINARY_DIR}/opengold")

# Compile the game's exact source files into this standalone extension. The
# registration entry point is replaced only to add the tester's setup bridge.
get_target_property(game_sources opengoldbox_game SOURCES)
set(shared_game_sources)
foreach(source IN LISTS game_sources)
    if(NOT source MATCHES "(^|/)register_types\\.cpp$")
        list(APPEND shared_game_sources "${repo_root}/src/OpenGoldBox/${source}")
    endif()
endforeach()

add_library(opengold_combat_tester SHARED
    ${shared_game_sources}
    native/register_types.cpp
    native/combat_test_bridge.cpp)
target_include_directories(opengold_combat_tester PRIVATE "${repo_root}/src/OpenGoldBox")
target_link_libraries(opengold_combat_tester PRIVATE opengold_core opengold_rules_srd5 godot-cpp)
target_compile_features(opengold_combat_tester PRIVATE cxx_std_20)
set_target_properties(opengold_combat_tester PROPERTIES PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/project/bin"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/project/bin")
