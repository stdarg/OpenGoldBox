add_library(opengold_combat_sprite_catalog SHARED sprite_catalog.cpp register_types.cpp)
target_link_libraries(opengold_combat_sprite_catalog PRIVATE opengold_core godot-cpp)
target_compile_features(opengold_combat_sprite_catalog PRIVATE cxx_std_20)
set_target_properties(opengold_combat_sprite_catalog PROPERTIES
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/dev_tools/combat_sprite_catalog/godot/bin"
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/dev_tools/combat_sprite_catalog/godot/bin")
add_custom_command(TARGET opengold_combat_sprite_catalog POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/combat_sprite_catalog.gdextension"
        "${PROJECT_SOURCE_DIR}/dev_tools/combat_sprite_catalog/godot/bin/combat_sprite_catalog.gdextension")
opengold_enable_warnings("${CMAKE_CURRENT_SOURCE_DIR}")

# The catalog's own checks, so a change to its weapon sets or records cannot pass
# the suite while they fail.
if(OPENGOLD_BUILD_TESTS AND OPENGOLD_DEMO_GODOT)
    include("${PROJECT_SOURCE_DIR}/cmake/godot_out_of_dock.cmake")
    opengold_check_godot("${OPENGOLD_DEMO_GODOT}" OPENGOLD_DEMO_CHECK_GODOT)
    set(catalog_project "${PROJECT_SOURCE_DIR}/dev_tools/combat_sprite_catalog/godot")
    add_test(NAME opengold_catalog_prepare COMMAND "${CMAKE_COMMAND}"
        "-DGODOT=${OPENGOLD_DEMO_CHECK_GODOT}" "-DPROJECT=${catalog_project}"
        -P "${PROJECT_SOURCE_DIR}/tests/prepare_catalog_project.cmake")
    set_tests_properties(opengold_catalog_prepare PROPERTIES
        FIXTURES_SETUP catalog_project TIMEOUT 300 LABELS demo RESOURCE_LOCK catalog_project)
    foreach(check IN ITEMS metadata catalog)
        add_test(NAME opengold_catalog_${check}_check COMMAND "${CMAKE_COMMAND}"
            "-DGODOT=${OPENGOLD_DEMO_CHECK_GODOT}" "-DPROJECT=${catalog_project}" "-DCHECK=--${check}-check"
            "-DEXPECTED=checks passed" "-DUSER_HOME=${CMAKE_BINARY_DIR}/godot-home/catalog_${check}"
            -P "${PROJECT_SOURCE_DIR}/tests/run_catalog_check.cmake")
        set_tests_properties(opengold_catalog_${check}_check PROPERTIES
            FIXTURES_REQUIRED catalog_project TIMEOUT 150 LABELS demo RESOURCE_LOCK catalog_project)
    endforeach()
endif()
