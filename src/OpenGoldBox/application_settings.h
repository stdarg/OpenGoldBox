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
