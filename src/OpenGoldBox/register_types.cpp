#include "rolf_tour_view.h"
#include "combat_view.h"
#include "character_creation_view.h"
#include "equipment_choice_dialog.h"
#include "level_up_dialog.h"
#include "save_slots.h"
#include "startup_view.h"
#include "game_lifecycle.h"
#include "screenshot_service.h"
#include <godot_cpp/godot.hpp>

namespace
{
void initialize(godot::ModuleInitializationLevel level)
{
    if (level == godot::MODULE_INITIALIZATION_LEVEL_SCENE)
    {
        godot::ClassDB::register_class<RolfTourView>();
        godot::ClassDB::register_class<CombatView>();
        godot::ClassDB::register_class<CharacterCreationView>();
        godot::ClassDB::register_class<LevelUpDialog>();
        godot::ClassDB::register_class<EquipmentChoiceDialog>();
        godot::ClassDB::register_class<SaveSlots>();
        godot::ClassDB::register_class<StartupView>();
        godot::ClassDB::register_class<GameLifecycle>();
        godot::ClassDB::register_class<ScreenshotService>();
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
