add_library(opengold_tile_art_viewer SHARED EXCLUDE_FROM_ALL
    wall_art_catalog.cpp map_art_profiles.cpp register_types.cpp)
target_link_libraries(opengold_tile_art_viewer PRIVATE opengold_core godot-cpp)
target_compile_features(opengold_tile_art_viewer PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(opengold_tile_art_viewer PRIVATE /WX)
else()
    target_compile_options(opengold_tile_art_viewer PRIVATE -Werror)
endif()
set_target_properties(opengold_tile_art_viewer PROPERTIES
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/dev_tools/tile_art_viewer/godot/bin"
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/dev_tools/tile_art_viewer/godot/bin"
)
add_custom_command(TARGET opengold_tile_art_viewer POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/tile_art_viewer.gdextension"
        "${PROJECT_SOURCE_DIR}/dev_tools/tile_art_viewer/godot/bin/tile_art_viewer.gdextension")
add_custom_target(opengold_replacement_art_creator
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "${PROJECT_SOURCE_DIR}/dev_tools/replacement_art_creator/godot/bin"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:opengold_tile_art_viewer>"
        "${PROJECT_SOURCE_DIR}/dev_tools/replacement_art_creator/godot/bin/$<TARGET_FILE_NAME:opengold_tile_art_viewer>"
    DEPENDS opengold_tile_art_viewer)
opengold_enable_warnings("${CMAKE_CURRENT_SOURCE_DIR}")
