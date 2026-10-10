# Official bindings for the documented GDExtension boundary. Native-only builds
# do not need this dependency or its Python code-generation prerequisite.
include(FetchContent)
if(NOT Python3_EXECUTABLE AND EXISTS "${PROJECT_SOURCE_DIR}/build/_deps/python/python.exe")
    set(Python3_EXECUTABLE "${PROJECT_SOURCE_DIR}/build/_deps/python/python.exe")
endif()
find_package(Python3 3.8 COMPONENTS Interpreter REQUIRED)
set(GODOTCPP_BUILD_PROFILE "${CMAKE_CURRENT_SOURCE_DIR}/build_profile.json" CACHE PATH "" FORCE)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${GODOTCPP_BUILD_PROFILE}")
set(GODOTCPP_DISABLE_EXCEPTIONS OFF CACHE BOOL "" FORCE)
set(GODOTCPP_USE_HOT_RELOAD OFF CACHE BOOL "" FORCE)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
FetchContent_Declare(godot_cpp
    GIT_REPOSITORY https://github.com/godotengine/godot-cpp.git
    GIT_TAG e83fd0904c13356ed1d4c3d09f8bb9132bdc6b77
)
FetchContent_MakeAvailable(godot_cpp)
# godot-cpp's headers are third-party; keep their warnings out of the project's.
get_target_property(godot_cpp_includes godot-cpp INTERFACE_INCLUDE_DIRECTORIES)
set_target_properties(godot-cpp PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${godot_cpp_includes}")
# Keep the approved source PNGs authoritative, and ship build copies inside
# the Godot project. Asset changes refresh even when the DLL needs no relink.
set(portrait_outputs)
foreach(species gnome orc goliath tiefling dragonborn)
    foreach(gender male female)
        set(portrait_source "${PROJECT_SOURCE_DIR}/data/art/portraits/${species}-${gender}.png")
        set(portrait_output "${PROJECT_SOURCE_DIR}/demos/godot/bin/portraits/${species}-${gender}.png")
        add_custom_command(OUTPUT "${portrait_output}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${PROJECT_SOURCE_DIR}/demos/godot/bin/portraits"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${portrait_source}" "${portrait_output}"
            DEPENDS "${portrait_source}" VERBATIM)
        list(APPEND portrait_outputs "${portrait_output}")
    endforeach()
endforeach()
file(GLOB complete_portraits CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/art/portraits/*.png" "${PROJECT_SOURCE_DIR}/art/portraits/portraits.json")
foreach(source IN LISTS complete_portraits)
    get_filename_component(name "${source}" NAME)
    set(output "${PROJECT_SOURCE_DIR}/demos/godot/bin/portraits/${name}")
    add_custom_command(OUTPUT "${output}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${PROJECT_SOURCE_DIR}/demos/godot/bin/portraits"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${source}" "${output}"
        DEPENDS "${source}" VERBATIM)
    list(APPEND portrait_outputs "${output}")
endforeach()
add_custom_target(opengold_portrait_assets DEPENDS ${portrait_outputs})
add_library(opengold_godot SHARED rolf_tour_view.cpp combat_view.cpp combat_sprite_demo.cpp equipment_sprite_demo.cpp character_creation_view.cpp
    portrait_picker.cpp party_preview.cpp equipment_choice.cpp level_up_view.cpp campaign_save_view.cpp save_slots.cpp character_pool_view.cpp character_sheet_view.cpp sound_board_view.cpp godot_sound_output.cpp register_types.cpp)
add_dependencies(opengold_godot opengold_portrait_assets)
target_link_libraries(opengold_godot PRIVATE opengold_core opengold_rules_srd5 godot-cpp)
target_compile_features(opengold_godot PRIVATE cxx_std_20)
target_compile_definitions(opengold_godot PRIVATE $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>)
set_target_properties(opengold_godot PROPERTIES
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/demos/godot/bin"
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/demos/godot/bin"
)
# Register the extension only after its library exists, so native-only users
# can still open the existing Godot research tools without a missing-DLL error.
add_custom_command(TARGET opengold_godot POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/opengold.gdextension"
        "${PROJECT_SOURCE_DIR}/demos/godot/bin/opengold.gdextension"
)

if(OPENGOLD_BUILD_TESTS)
    # Search PATH first, then the usual per-platform install locations. Listing
    # only macOS bundles here meant the Godot checks silently did not register on
    # Windows, which reads as "all checks passed" -- see docs/TECH.md 14.1.
    find_program(OPENGOLD_DEMO_GODOT
        NAMES godot godot4 Godot Godot_v4 godot_console
        PATHS
            /Applications/Godot.app/Contents/MacOS
            /Applications/Godot_mono.app/Contents/MacOS
            "$ENV{LOCALAPPDATA}/Programs/Godot"
            "$ENV{ProgramFiles}/Godot"
            /usr/local/bin /usr/bin /opt/godot)
    if(NOT OPENGOLD_DEMO_GODOT)
        message(STATUS "Godot not found: the Godot runtime checks will not be registered. "
                       "Set OPENGOLD_DEMO_GODOT to its path to enable them.")
    endif()
    if(OPENGOLD_DEMO_GODOT)
        set(demo_project "${PROJECT_SOURCE_DIR}/demos/godot")
        add_test(NAME opengold_demo_prepare COMMAND "${OPENGOLD_DEMO_GODOT}"
            --headless --editor --path "${demo_project}" --import --quit)
        set_tests_properties(opengold_demo_prepare PROPERTIES FIXTURES_SETUP demo_project TIMEOUT 120 LABELS demo)
        add_test(NAME opengold_godot_body_review COMMAND "${CMAKE_COMMAND}"
            "-DGODOT=${OPENGOLD_DEMO_GODOT}" "-DPROJECT=${demo_project}"
            "-DSCRIPT=${PROJECT_SOURCE_DIR}/tests/combat_body_review_tests.gd"
            "-DEXPECTED=Combat body reviewer checks passed" -P "${PROJECT_SOURCE_DIR}/tests/run_godot_test.cmake")
        # The demo checks share one Godot project; ctest -j must run them one at a time.
        set_tests_properties(opengold_godot_body_review PROPERTIES FIXTURES_REQUIRED demo_project TIMEOUT 40 LABELS demo
            RESOURCE_LOCK demo_project)
        add_test(NAME opengold_godot_sprite_demo COMMAND "${CMAKE_COMMAND}"
            "-DGODOT=${OPENGOLD_DEMO_GODOT}" "-DPROJECT=${demo_project}"
            "-DSCRIPT=${PROJECT_SOURCE_DIR}/tests/combat_sprite_demo_tests.gd"
            "-DEXPECTED=Combat sprite demo checks passed" -P "${PROJECT_SOURCE_DIR}/tests/run_godot_test.cmake")
        set_tests_properties(opengold_godot_sprite_demo PROPERTIES FIXTURES_REQUIRED demo_project TIMEOUT 40 LABELS demo
            RESOURCE_LOCK demo_project ENVIRONMENT "OPENGOLD_GAME_DIR=;OPENGOLD_SCREENSHOT_DIR=${CMAKE_BINARY_DIR}/sprite-demo-screenshots")
        add_test(NAME opengold_godot_equipment_demo COMMAND "${CMAKE_COMMAND}"
            "-DGODOT=${OPENGOLD_DEMO_GODOT}" "-DPROJECT=${demo_project}"
            "-DSCRIPT=${PROJECT_SOURCE_DIR}/tests/equipment_sprite_demo_tests.gd"
            "-DEXPECTED=Equipment sprite demo checks passed" -P "${PROJECT_SOURCE_DIR}/tests/run_godot_test.cmake")
        set_tests_properties(opengold_godot_equipment_demo PROPERTIES FIXTURES_REQUIRED demo_project TIMEOUT 40 LABELS demo
            RESOURCE_LOCK demo_project)
        # Synthetic checks, plus the installed ECL scripts and maps when
        # OPENGOLD_GAME_DIR names the original files. The map inspector reads
        # maps through build/opengold_maps, which ./build.sh builds.
        add_test(NAME opengold_godot_ecl_art COMMAND "${CMAKE_COMMAND}"
            "-DGODOT=${OPENGOLD_DEMO_GODOT}" "-DPROJECT=${demo_project}"
            "-DSCRIPT=${PROJECT_SOURCE_DIR}/tests/ecl_art_tests.gd"
            "-DEXPECTED=PASS: ECL decoder, script evidence, and review integration"
            "-DUSER_HOME=${CMAKE_BINARY_DIR}/godot-home/ecl_art"
            -P "${PROJECT_SOURCE_DIR}/tests/run_godot_test.cmake")
        set_tests_properties(opengold_godot_ecl_art PROPERTIES FIXTURES_REQUIRED demo_project
            TIMEOUT 120 LABELS demo RESOURCE_LOCK demo_project)
        add_test(NAME opengold_godot_map_inspector COMMAND "${CMAKE_COMMAND}"
            "-DGODOT=${OPENGOLD_DEMO_GODOT}" "-DPROJECT=${demo_project}"
            "-DSCRIPT=${PROJECT_SOURCE_DIR}/tests/map_inspector_tests.gd"
            "-DEXPECTED=Map inspector tests passed"
            "-DUSER_HOME=${CMAKE_BINARY_DIR}/godot-home/map_inspector"
            -P "${PROJECT_SOURCE_DIR}/tests/run_godot_test.cmake")
        set_tests_properties(opengold_godot_map_inspector PROPERTIES FIXTURES_REQUIRED demo_project
            TIMEOUT 120 LABELS demo RESOURCE_LOCK demo_project)
    endif()
endif()

# Build the demo at the project's warning level, as the game does.
opengold_enable_warnings("${CMAKE_CURRENT_SOURCE_DIR}")
