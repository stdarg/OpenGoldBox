#include "combat_test_bridge.h"
#include "rolf_tour_view.h"
#include "combat_view.h"
#include "character_creation_view.h"
#include "character_pool_dialog.h"
#include "town_sheet_dialog.h"
#include "equipment_choice_dialog.h"
#include "level_up_dialog.h"
#include "level_up_lab_view.h"
#include "save_slots.h"
#include "game_menu_view.h"
#include "startup_view.h"
#include "game_lifecycle.h"
#include "screenshot_service.h"
#include <godot_cpp/godot.hpp>

namespace
{
void initialize(godot::ModuleInitializationLevel level)
{
    if (level != godot::MODULE_INITIALIZATION_LEVEL_SCENE)
    {
        return;
    }
    godot::ClassDB::register_class<RolfTourView>();
    godot::ClassDB::register_class<CombatView>();
    godot::ClassDB::register_class<CharacterCreationView>();
    godot::ClassDB::register_class<LevelUpDialog>();
    godot::ClassDB::register_class<LevelUpLabView>();
    godot::ClassDB::register_class<EquipmentChoiceDialog>();
    godot::ClassDB::register_class<CharacterPoolDialog>();
    godot::ClassDB::register_class<TownSheetDialog>();
    godot::ClassDB::register_class<SaveSlots>();
    godot::ClassDB::register_class<GameMenuView>();
    godot::ClassDB::register_class<StartupView>();
    godot::ClassDB::register_class<GameLifecycle>();
    godot::ClassDB::register_class<ScreenshotService>();
    godot::ClassDB::register_class<CombatTestBridge>();
}
void terminate(godot::ModuleInitializationLevel) {}
}

extern "C" GDExtensionBool GDE_EXPORT combat_tester_library_init(
    GDExtensionInterfaceGetProcAddress get_proc_address,
    GDExtensionClassLibraryPtr library, GDExtensionInitialization *initialization)
{
    godot::GDExtensionBinding::InitObject init(get_proc_address, library, initialization);
    init.register_initializer(initialize);
    init.register_terminator(terminate);
    init.set_minimum_library_initialization_level(godot::MODULE_INITIALIZATION_LEVEL_SCENE);
    return init.init();
}
