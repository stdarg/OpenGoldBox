#ifndef OPENGOLDBOX_APPLICATION_SETTINGS_H
#define OPENGOLDBOX_APPLICATION_SETTINGS_H
#include "game_rules.h"
#include "opengold/difficulty.h"
#include "opengold/ua_design.h"
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <filesystem>
#include <optional>

#include "opengold/game_finder.h"
#include "opengold/gold_box_game.h"

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
// [game] difficulty: easy, normal, hard, nightmare or infernal (DIFF-1), or
// novice, which only the Buck Rogers games offer and the others play as easy.
opengold::Difficulty difficulty();
// The setting as the Buck Rogers games read it, Novice included.
opengold::DifficultySetting difficulty_setting();
// [monsters]: each monster ability's lowest difficulty, or "never"; the
// abilities off at the set difficulty (MONSTER-2).
std::vector<std::string> monster_abilities_off();
// One monster ability's option: a difficulty name or "never", its default
// when unset or unreadable.
godot::String monster_ability_option(const std::string &ability);
// The Options screen's writes; each keeps the other settings.
bool save_difficulty(opengold::Difficulty difficulty);
bool save_difficulty_setting(opengold::DifficultySetting setting);
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
// original art. OPENGOLD_NEW_ART_DIR overrides it.
godot::String new_art_directory();
// Why `directory` is not the new art's folder, or empty when it is.
godot::String new_art_problem(const godot::String &directory);
bool save_new_art_directory(const godot::String &directory);
bool save_original_wall_art(bool original);
bool save_original_combat_art(bool original);
bool save_original_portraits(bool original);
// [original_shops] by game folder: "off" lets the game's modified shops sell
// (Pool of Radiance's New Phlan adds the Rogue's Shop and OpenGoldBox's gear);
// otherwise its shops are as they were in the original, at its prices.
bool original_shops(opengold::GoldBoxGame game);
bool save_original_shops(opengold::GoldBoxGame game, bool original);
// [gateway_money] TREASURE: "on" lets a character brought from Gateway into
// Treasures keep its own platinum, gems and jewelry, as the original does;
// otherwise it starts with Treasures' 200 platinum.
bool gateway_money_kept();
bool save_gateway_money_kept(bool kept);
// [darkness_gazes] DARKNESS: "on" plays Pools of Darkness's paralyzing and
// charming gazes (its effects 114 and 124), which the original registers but
// never runs; otherwise they do nothing, as in the original.
bool darkness_gazes();
bool save_darkness_gazes(bool played);
// The Buck Rogers games' ship-combat options, by game folder
// (opengold::original::ShipCombatOptions). [command_every_round]: "on"
// offers COMMAND once a round even while the pilot is well, as the original
// does; otherwise only while the pilot is down, as the Rule Book says.
// [empty_helm_dodges]: "on" lets a ship with no one at the helm dodge a ram
// by its own pilot value, as the original does; otherwise it dodges with 0.
// [boarding_fuel]: "off" takes no fuel from a boarded ship, as the original;
// otherwise the captured ship's fuel is transferred, as the Rule Book says.
bool command_every_round(opengold::GoldBoxGame game);
bool save_command_every_round(opengold::GoldBoxGame game, bool on);
bool empty_helm_dodges(opengold::GoldBoxGame game);
bool save_empty_helm_dodges(opengold::GoldBoxGame game, bool on);
bool boarding_fuel(opengold::GoldBoxGame game);
bool save_boarding_fuel(opengold::GoldBoxGame game, bool on);
// [ship_picture_shrinks], by game folder: "off" draws the enemy ship at one
// size, as the original does; otherwise its picture grows smaller with the
// range (an OpenGoldBox touch, the user's choice of 2026-11-20).
bool ship_picture_shrinks(opengold::GoldBoxGame game);
bool save_ship_picture_shrinks(opengold::GoldBoxGame game, bool on);
// [design_difficulty] UA: how the difficulty plays in an Unlimited Adventures
// design, "design" (the kit's own levels), "table" (OpenGoldBox's table) or
// "both", the default.
opengold::ua::DesignDifficulty design_difficulty();
bool save_design_difficulty(opengold::ua::DesignDifficulty plays);
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
// [sound] effects and music: each a percentage, 0-100, of the games' full
// loudness, the same for every game. Effects are the games' samples (a fight,
// thunder, a title's sound, footsteps); music is a title's music.
enum class SoundKind
{
    effects,
    music
};
[[nodiscard]] int volume_percent(SoundKind kind);
bool save_volume_percent(SoundKind kind, int percent);
// The volume percentage as a share of full loudness, 0-1.
[[nodiscard]] double volume_share(SoundKind kind);
// The volume a SoundPlayer of `kind` plays at: its percentage of the loudness
// the games' sounds have always played at.
[[nodiscard]] double sound_volume(SoundKind kind);
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

// The game chosen on the startup menu: Pool of Radiance until another is chosen.
[[nodiscard]] opengold::GoldBoxGame chosen_game();
void choose_game(opengold::GoldBoxGame game);
// The rules the new game's question chose (docs/ORIGINAL-RULES.md): what a new
// game plays by until it is asked.
[[nodiscard]] GameRules chosen_rules();
void choose_rules(GameRules rules);

// How the game screen begins: a new party made on the creation screens, the
// game's default party (data/default_parties), or a save loaded.
struct GameStart
{
    enum class Kind
    {
        new_party,
        default_party,
        saved_game
    };
    Kind kind{Kind::new_party};
    std::filesystem::path save; // The save to load, for saved_game.
};

[[nodiscard]] GameStart chosen_game_start();
void choose_game_start(GameStart start);
// A game's folder and where it came from: the one the player chose (Pool of
// Radiance's under [game]; another's under [games] by its game_folder name)
// while it still holds the game's files, else the one OPENGOLD_GAME_DIR,
// OPENGOLD_GOLDBOX_DIR or the search upward from the game's own folder finds
// by its files (opengold/game_finder.h, docs/TECH.md); nothing when neither.
// Startup asks the player to confirm a found folder before saving it.
[[nodiscard]] std::optional<opengold::GameFolder> locate_game_folder(opengold::GoldBoxGame game);
// locate_game_folder's folder, or empty; Pool of Radiance's is game_path().
[[nodiscard]] godot::String game_folder(opengold::GoldBoxGame game);
bool save_game_folder(opengold::GoldBoxGame game, const godot::String &directory);
// Pool of Radiance's folder is checked file by file (validate_game_path);
// another game's must hold that game's files.
[[nodiscard]] Validation validate_game_folder(const godot::String &directory,
                                              opengold::GoldBoxGame game);
// The design an Unlimited Adventures game plays: a folder the construction
// kit made (NAME.DSN), kept under [games] as UA_DESIGN (docs/games/ua.md).
[[nodiscard]] godot::String design_folder();
bool save_design_folder(const godot::String &directory);
[[nodiscard]] Validation validate_design_folder(const godot::String &directory);
} // namespace settings
#endif
