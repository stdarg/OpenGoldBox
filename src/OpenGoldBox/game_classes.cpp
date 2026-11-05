#include "game_classes.h"
#include "game_log.h"
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
#include <godot_cpp/core/class_db.hpp>

void register_game_classes()
{
    godot::ClassDB::register_class<RolfTourView>();
    godot::ClassDB::register_class<CombatView>();
    godot::ClassDB::register_class<CharacterCreationView>();
    godot::ClassDB::register_class<LevelUpDialog>();
    godot::ClassDB::register_class<LevelUpLabView>();
    godot::ClassDB::register_class<EquipmentChoiceDialog>();
    godot::ClassDB::register_class<CharacterPoolDialog>();
    godot::ClassDB::register_class<TownSheetDialog>();
    godot::ClassDB::register_class<SaveSlots>();
    godot::ClassDB::register_class<EngineErrors>();
    godot::ClassDB::register_class<GameMenuView>();
    godot::ClassDB::register_class<StartupView>();
    godot::ClassDB::register_class<GameLifecycle>();
    godot::ClassDB::register_class<ScreenshotService>();
}
