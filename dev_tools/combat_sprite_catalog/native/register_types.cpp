#include "sprite_catalog.h"
#include <godot_cpp/godot.hpp>

namespace
{
void initialize(godot::ModuleInitializationLevel level)
{
    if (level == godot::MODULE_INITIALIZATION_LEVEL_SCENE)
        godot::ClassDB::register_class<SpriteCatalog>();
}

void terminate(godot::ModuleInitializationLevel)
{
}
} // namespace

extern "C" GDExtensionBool GDE_EXPORT combat_sprite_catalog_init(
    GDExtensionInterfaceGetProcAddress get_proc_address, const GDExtensionClassLibraryPtr library,
    GDExtensionInitialization *initialization)
{
    godot::GDExtensionBinding::InitObject init(get_proc_address, library, initialization);
    init.register_initializer(initialize);
    init.register_terminator(terminate);
    init.set_minimum_library_initialization_level(godot::MODULE_INITIALIZATION_LEVEL_SCENE);
    return init.init();
}
