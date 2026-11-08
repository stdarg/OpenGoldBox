cmake_minimum_required(VERSION 3.25)
project(OpenGoldPartyTester LANGUAGES CXX)

if(MSVC)
    # The game's libraries and godot-cpp use the DLL runtime, which the
    # top-level CMakeLists.txt sets only for its own directory.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

set(OPENGOLD_BUILD_GAME ON CACHE BOOL "" FORCE)
set(OPENGOLD_BUILD_GODOT OFF CACHE BOOL "" FORCE)
set(OPENGOLD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(repo_root "${CMAKE_CURRENT_LIST_DIR}/../..")
add_subdirectory("${repo_root}" "${CMAKE_BINARY_DIR}/opengold")

# Compile the game's exact source files into this standalone extension. The
# registration entry point is replaced only to add the tester's bridge.
get_target_property(game_sources opengoldbox_game SOURCES)
set(shared_game_sources)
foreach(source IN LISTS game_sources)
    if(NOT source MATCHES "(^|/)register_types\\.cpp$")
        list(APPEND shared_game_sources "${repo_root}/src/OpenGoldBox/${source}")
    endif()
endforeach()

add_library(opengold_party_tester SHARED
    ${shared_game_sources}
    native/register_types.cpp
    native/party_test_bridge.cpp
    ../shared/seeded_battlefield.cpp)
target_include_directories(opengold_party_tester PRIVATE "${repo_root}/src/OpenGoldBox"
    "${CMAKE_CURRENT_LIST_DIR}/../shared")
# The game's own include folders, its generated commit header's among them.
get_target_property(game_includes opengoldbox_game INCLUDE_DIRECTORIES)
target_include_directories(opengold_party_tester PRIVATE ${game_includes})
add_dependencies(opengold_party_tester opengoldbox_commit)
target_link_libraries(opengold_party_tester PRIVATE opengold_core opengold_rules_srd5 godot-cpp)
target_compile_features(opengold_party_tester PRIVATE cxx_std_20)
get_target_property(game_definitions opengoldbox_game COMPILE_DEFINITIONS)
target_compile_definitions(opengold_party_tester PRIVATE ${game_definitions})
set_target_properties(opengold_party_tester PROPERTIES PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/project/bin"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/project/bin")
