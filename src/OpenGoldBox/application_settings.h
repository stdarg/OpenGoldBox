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
// members keep theirs either way. portraits: "original" shows the player
// characters' original head-and-body portraits in place of the new ones.
bool original_wall_art();
bool original_combat_art();
bool original_portraits();
// [art] directory: the folder of the optional new art download, or empty
// while there is none or it is not the new art: then the game shows only the
// original art. OPENGOLD_NEW_ART_DIR overrides it, as OPENGOLD_GAME_DIR does
// the game's folder.
godot::String new_art_directory();
// Why `directory` is not the new art's folder, or empty when it is.
godot::String new_art_problem(const godot::String &directory);
bool save_new_art_directory(const godot::String &directory);
bool save_original_wall_art(bool original);
bool save_original_combat_art(bool original);
bool save_original_portraits(bool original);
// [gear] auto_equip: "off" leaves gained gear in the pack; otherwise a better
// weapon, armor, shield or worn item goes on (EQUIP-1). unidentified_magic: "equip" lets
// auto-equip put on an unidentified magic item; otherwise it stays packed.
bool auto_equip();
bool skip_unidentified_magic();
bool save_auto_equip(bool on);
bool save_skip_unidentified_magic(bool skip);
// [combat] keep_quick: "on" keeps members on Quick for the next fight;
// otherwise a fight's end returns every member to the player.
bool keep_quick();
bool save_keep_quick(bool keep);
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
