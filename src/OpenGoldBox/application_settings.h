#ifndef OPENGOLDBOX_APPLICATION_SETTINGS_H
#define OPENGOLDBOX_APPLICATION_SETTINGS_H
#include "opengold/difficulty.h"
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

namespace settings
{
bool flag(const char *name);
godot::String path();
godot::String saved_game_path();
godot::String saved_language();
godot::String game_path();
godot::String language();
int combat_zoom_percent();
// [combat] encounter_challenge: a percentage of the SRD's Moderate XP budget, 0-200.
unsigned encounter_challenge();
// [game] difficulty: easy, normal, hard, nightmare or infernal (DIFF-1).
opengold::Difficulty difficulty();
// [monsters]: each monster ability's lowest difficulty, or "never"; the
// abilities off at the set difficulty (MONSTER-2).
std::vector<std::string> monster_abilities_off();
// One monster ability's option: a difficulty name or "never", its default
// when unset or unreadable.
godot::String monster_ability_option(const std::string &ability);
// The Options screen's writes; each keeps the other settings.
bool save_difficulty(opengold::Difficulty difficulty);
bool save_encounter_challenge(unsigned challenge);
bool save_combat_zoom(int percent);
bool save_monster_ability(const std::string &ability, const godot::String &from);
// [art] walls and combat: "original" shows the original game's art in the 3D
// view and for the creatures in a fight in place of the new art (#274). Party
// members keep theirs either way.
bool original_wall_art();
bool original_combat_art();
bool save_original_wall_art(bool original);
bool save_original_combat_art(bool original);
// Every monster ability back to its default.
bool restore_monster_abilities();
bool valid_language(const godot::String &locale);
bool save_game_path(const godot::String &directory);
bool save_language(const godot::String &locale);

struct Validation
{
    bool usable{};
    godot::PackedStringArray missing;
    godot::PackedStringArray different;
    godot::String error;
};

Validation validate_game_path(const godot::String &directory);
} // namespace settings
#endif
