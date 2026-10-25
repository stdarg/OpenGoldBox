add_library(opengold_combat_sprite_catalog SHARED EXCLUDE_FROM_ALL sprite_catalog.cpp register_types.cpp)
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
