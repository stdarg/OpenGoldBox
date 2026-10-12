#include "combat_sprite_demo.h"
#include "equipment_sprite_demo.h"
#include "sound_board_view.h"
#include <godot_cpp/godot.hpp>

namespace
{
void initialize(godot::ModuleInitializationLevel level)
{
    if (level == godot::MODULE_INITIALIZATION_LEVEL_SCENE)
    {
        godot::ClassDB::register_class<CombatSpriteDemo>();
        godot::ClassDB::register_class<EquipmentSpriteDemo>();
        godot::ClassDB::register_class<SoundBoardView>();
    }
}

void terminate(godot::ModuleInitializationLevel)
{
}
} // namespace

extern "C"
{
    GDExtensionBool GDE_EXPORT opengold_library_init(
    GDExtensionInterfaceGetProcAddress get_proc_address,
    const GDExtensionClassLibraryPtr library, GDExtensionInitialization *initialization)
    {
        // Pointers above are borrowed C ABI arguments owned by Godot.
        godot::GDExtensionBinding::InitObject init(get_proc_address, library, initialization);
        init.register_initializer(initialize);
        init.register_terminator(terminate);
        init.set_minimum_library_initialization_level(godot::MODULE_INITIALIZATION_LEVEL_SCENE);
        return init.init();
    }
}
