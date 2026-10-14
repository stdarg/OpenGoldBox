add_library(opengold_tile_art_viewer SHARED EXCLUDE_FROM_ALL
    wall_art_catalog.cpp map_art_profiles.cpp register_types.cpp)
target_link_libraries(opengold_tile_art_viewer PRIVATE opengold_core godot-cpp)
target_compile_features(opengold_tile_art_viewer PRIVATE cxx_std_20)
set_target_properties(opengold_tile_art_viewer PROPERTIES
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/dev_tools/tile_art_viewer/godot/bin"
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/dev_tools/tile_art_viewer/godot/bin"
)
add_custom_command(TARGET opengold_tile_art_viewer POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/tile_art_viewer.gdextension"
        "${PROJECT_SOURCE_DIR}/dev_tools/tile_art_viewer/godot/bin/tile_art_viewer.gdextension")
opengold_enable_warnings("${CMAKE_CURRENT_SOURCE_DIR}")
