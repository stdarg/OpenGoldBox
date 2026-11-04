cmake_minimum_required(VERSION 3.25)
project(OpenGoldCreatureStatsEditor LANGUAGES CXX)

# The game's own build, for its libraries, godot-cpp and the target that copies
# data/ into the game project (opengoldbox_game_data).
set(OPENGOLD_BUILD_GAME ON CACHE BOOL "" FORCE)
set(OPENGOLD_BUILD_GODOT OFF CACHE BOOL "" FORCE)
set(OPENGOLD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(repo_root "${CMAKE_CURRENT_LIST_DIR}/../..")
add_subdirectory("${repo_root}" "${CMAKE_BINARY_DIR}/opengold")

add_library(opengold_creature_stats_editor SHARED
    native/register_types.cpp
    native/creature_editor.cpp)
target_include_directories(opengold_creature_stats_editor PRIVATE
    "${repo_root}/tools" "${repo_root}/src/OpenGoldBox")
target_link_libraries(opengold_creature_stats_editor PRIVATE opengold_creature_data godot-cpp)
target_compile_features(opengold_creature_stats_editor PRIVATE cxx_std_20)
set_target_properties(opengold_creature_stats_editor PROPERTIES PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/project/bin"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/project/bin")
