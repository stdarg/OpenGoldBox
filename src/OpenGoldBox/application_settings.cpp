#include "application_settings.h"
#include "localization.h"
#include "opengold/diagnostic_log.h"
#include "opengold/difficulty.h"
#include "opengold/encounter_budget.h"
#include "opengold/game_census.h"
#include "godot_path.h"
#include <godot_cpp/classes/config_file.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <algorithm>
#include <functional>
#include <map>
#include <optional>
#include <utility>

using namespace godot;

namespace
{
std::optional<String> confirmed_path, confirmed_language;
opengold::GoldBoxGame current_game = opengold::GoldBoxGame::pool_of_radiance;
GameRules current_rules = new_game_rules;
settings::GameStart current_start;
std::map<opengold::GoldBoxGame, String> confirmed_folders;

String read(const char *section, const char *key)
{
    Ref<ConfigFile> config;
    config.instantiate();
    if (config->load(settings::path()) != OK)
    {
        return {};
    }
    const Variant value = config->get_value(section, key, String());
    return value.get_type() == Variant::STRING ? String(value).strip_edges() : String();
}

// Applies `change` to the settings and writes them, the defaults of keys they
// lack included.
bool save_config(const std::function<void(ConfigFile &)> &change)
{
    Ref<ConfigFile> config;
    config.instantiate();
    const auto loaded = config->load(settings::path());
    // Do not overwrite unreadable or malformed configuration and unrelated keys.
    if (loaded != OK && loaded != ERR_FILE_NOT_FOUND)
    {
        return false;
    }
    change(**config);
    if (!config->has_section_key("combat", "combat_zoom"))
    {
        config->set_value("combat", "combat_zoom", 100);
    }
    for (const char *key : {"walls", "combat", "portraits"})
    {
        if (!config->has_section_key("art", key))
        {
            config->set_value("art", key, "new");
        }
    }
    if (!config->has_section_key("combat", "encounter_challenge"))
    {
        config->set_value("combat", "encounter_challenge",
                          static_cast<int>(opengold::default_encounter_challenge));
    }
    for (const auto &[ability, from] : opengold::ability_defaults)
    {
        if (!config->has_section_key("monsters", String(std::string(ability).c_str())))
        {
            config->set_value("monsters", String(std::string(ability).c_str()),
                              String(std::string(from).c_str()));
        }
    }
    if (!config->has_section_key("game", "difficulty"))
    {
        config->set_value(
            "game", "difficulty",
            String(
                opengold::difficulty_names[static_cast<std::size_t>(opengold::default_difficulty)]
                    .data()));
    }
    const String temporary = settings::path() + ".tmp";
    if (config->save(temporary) != OK)
    {
        return false;
    }
    if (DirAccess::rename_absolute(temporary, settings::path()) == OK)
    {
        return true;
    }
    DirAccess::remove_absolute(temporary);
    return false;
}

bool save(const char *section, const char *key, const Variant &value)
{
    opengold::log_diagnostic(opengold::LogPriority::notice, "settings",
                             std::string(section) + "/" + key + " = " +
                                 String(value).utf8().get_data());
    return save_config(
        [&](ConfigFile &config)
        {
            config.set_value(section, key, value);
        });
}
} // namespace

namespace settings
{
bool flag(const char *name)
{
    auto *os = OS::get_singleton();
    return os->get_cmdline_args().has(name) || os->get_cmdline_user_args().has(name);
}

String path()
{
    auto *os = OS::get_singleton();
    // Each Godot check names its own file, so checks can run at the same time
    // without sharing the project's settings.
    const auto override = os->get_environment("OPENGOLD_SETTINGS_FILE").strip_edges();
    if (!override.is_empty())
    {
        return override;
    }

    // Editor runs share Godot's executable, so keep development settings in the project.
    if (os->has_feature("editor"))
    {
        return ProjectSettings::get_singleton()->globalize_path("res://settings.cfg");
    }
    // Beside saves, in the player's own folder: the game's folder may be read-only.
    return ProjectSettings::get_singleton()->globalize_path("user://settings.cfg");
}

String saved_game_path()
{
    return read("game", "path");
}

String saved_language()
{
    return read("interface", "language");
}

int combat_zoom_percent()
{
    Ref<ConfigFile> config;
    config.instantiate();
    if (config->load(path()) == OK)
    {
        const Variant value = config->get_value("combat", "combat_zoom", 100);
        if (value.get_type() == Variant::INT)
        {
            return std::clamp(static_cast<int>(value), 10, 1000);
        }
    }
    const Variant fallback =
        ProjectSettings::get_singleton()->get_setting("opengold/combat_zoom", 100);
    return fallback.get_type() == Variant::INT ? std::clamp(static_cast<int>(fallback), 10, 1000)
                                               : 100;
}

unsigned encounter_challenge()
{
    Ref<ConfigFile> config;
    config.instantiate();
    if (config->load(path()) == OK)
    {
        const Variant fallback = static_cast<int>(opengold::default_encounter_challenge);
        const Variant value = config->get_value("combat", "encounter_challenge", fallback);
        if (value.get_type() == Variant::INT)
        {
            return static_cast<unsigned>(std::clamp(
                static_cast<int>(value), 0, static_cast<int>(opengold::max_encounter_challenge)));
        }
    }
    return opengold::default_encounter_challenge;
}

opengold::Difficulty difficulty()
{
    const auto name = read("game", "difficulty").to_lower();
    return opengold::parse_difficulty(name.utf8().get_data())
        .value_or(opengold::default_difficulty);
}

std::vector<std::string> monster_abilities_off()
{
    std::vector<std::pair<std::string, std::string>> options;
    Ref<ConfigFile> config;
    config.instantiate();
    if (config->load(path()) == OK && config->has_section("monsters"))
    {
        for (const auto &key : config->get_section_keys("monsters"))
        {
            const Variant value = config->get_value("monsters", key, String());
            options.emplace_back(key.utf8().get_data(),
                                 String(value).strip_edges().to_lower().utf8().get_data());
        }
    }
    return opengold::abilities_off(difficulty(), options);
}

String monster_ability_option(const std::string &ability)
{
    const auto key = String(ability.c_str());
    const auto fallback =
        std::find_if(opengold::ability_defaults.begin(), opengold::ability_defaults.end(),
                     [&](const auto &option)
                     {
                         return option.first == ability;
                     });
    const String standard = fallback == opengold::ability_defaults.end()
                                ? String(opengold::difficulty_names.front().data())
                                : String(std::string(fallback->second).c_str());
    const auto value = read("monsters", std::string(ability).c_str()).to_lower();
    return value == "never" || opengold::parse_difficulty(value.utf8().get_data()) ? value
                                                                                   : standard;
}

bool save_difficulty(opengold::Difficulty difficulty)
{
    return save("game", "difficulty",
                String(opengold::difficulty_names[static_cast<std::size_t>(difficulty)].data()));
}

bool original_wall_art()
{
    return read("art", "walls").to_lower() == "original";
}

bool original_combat_art()
{
    return read("art", "combat").to_lower() == "original";
}

String new_art_problem(const String &directory)
{
    if (!directory.is_absolute_path() || !DirAccess::dir_exists_absolute(directory))
    {
        return N_("Choose an existing folder holding the OpenGoldBox new art.");
    }
    Ref<JSON> json;
    json.instantiate();
    const auto manifest = directory.path_join("new-art-pack.json");
    if (!FileAccess::file_exists(manifest) ||
        json->parse(FileAccess::get_file_as_string(manifest)) != OK ||
        json->get_data().get_type() != Variant::DICTIONARY ||
        Dictionary(json->get_data()).get("format", "") != Variant("opengold-new-art-pack-v1"))
    {
        return N_("This folder is not the OpenGoldBox new art: it has no new-art-pack.json.");
    }
    return {};
}

String new_art_directory()
{
    const auto override =
        OS::get_singleton()->get_environment("OPENGOLD_NEW_ART_DIR").strip_edges();
    const auto directory = override.is_empty() ? read("art", "directory") : override;
    // Asked for on every portrait drawn, so each folder is checked once.
    static String checked;
    static bool usable{};
    if (directory != checked)
    {
        checked = directory;
        usable = !directory.is_empty() && new_art_problem(directory).is_empty();
    }
    return usable ? directory : String();
}

bool save_new_art_directory(const String &directory)
{
    return save("art", "directory", directory);
}

bool original_portraits()
{
    return read("art", "portraits").to_lower() == "original";
}

bool save_original_portraits(bool original)
{
    return save("art", "portraits", original ? "original" : "new");
}

bool original_shops(opengold::GoldBoxGame game)
{
    const std::string folder(opengold::game_folder(game));
    return read("original_shops", folder.c_str()).to_lower() != "off";
}

bool save_original_shops(opengold::GoldBoxGame game, bool original)
{
    const std::string folder(opengold::game_folder(game));
    return save("original_shops", folder.c_str(), original ? "on" : "off");
}

bool save_original_wall_art(bool original)
{
    return save("art", "walls", original ? "original" : "new");
}

bool save_original_combat_art(bool original)
{
    return save("art", "combat", original ? "original" : "new");
}

bool auto_equip()
{
    return read("gear", "auto_equip").to_lower() != "off";
}

bool skip_unidentified_magic()
{
    return read("gear", "unidentified_magic").to_lower() != "equip";
}

bool save_auto_equip(bool on)
{
    return save("gear", "auto_equip", on ? "on" : "off");
}

bool save_skip_unidentified_magic(bool skip)
{
    return save("gear", "unidentified_magic", skip ? "skip" : "equip");
}

bool keep_quick()
{
    return read("combat", "keep_quick").to_lower() == "on";
}

bool save_keep_quick(bool keep)
{
    return save("combat", "keep_quick", keep ? "on" : "off");
}

bool save_combat_zoom(int percent)
{
    return save("combat", "combat_zoom", std::clamp(percent, 10, 1000));
}

bool save_encounter_challenge(unsigned challenge)
{
    return save("combat", "encounter_challenge",
                static_cast<int>(std::min(challenge, opengold::max_encounter_challenge)));
}

bool save_monster_ability(const std::string &ability, const String &from)
{
    return save("monsters", ability.c_str(), from);
}

bool restore_monster_abilities()
{
    opengold::log_diagnostic(opengold::LogPriority::notice, "settings",
                             "monster abilities back to their defaults");
    return save_config(
        [](ConfigFile &config)
        {
            if (config.has_section("monsters"))
            {
                config.erase_section("monsters");
            }
        });
}

bool valid_language(const String &locale)
{
    return locale == "en" || locale == "es";
}

String game_path()
{
    if (confirmed_path)
    {
        return *confirmed_path;
    }
    const auto override = OS::get_singleton()->get_environment("OPENGOLD_GAME_DIR").strip_edges();
    return !flag("--reset-game-path") && !override.is_empty() ? override : saved_game_path();
}

String language()
{
    if (confirmed_language)
    {
        return *confirmed_language;
    }
    const auto override =
        OS::get_singleton()->get_environment("OPENGOLD_LANG").strip_edges().to_lower();
    if (!flag("--reset-lang") && valid_language(override))
    {
        return override;
    }
    const auto saved = saved_language();
    if (valid_language(saved))
    {
        return saved;
    }
    return OS::get_singleton()->get_locale_language().to_lower() == "es" ? "es" : "en";
}

bool save_game_path(const String &directory)
{
    if (!save("game", "path", directory))
    {
        return false;
    }
    confirmed_path = directory;
    return true;
}

bool save_language(const String &locale)
{
    if (!valid_language(locale) || !save("interface", "language", locale))
    {
        return false;
    }
    confirmed_language = locale;
    return true;
}

Validation validate_game_path(const String &directory)
{
    Validation result;
    if (!directory.is_absolute_path() || !DirAccess::dir_exists_absolute(directory))
    {
        result.error = "Choose an existing folder containing the Pool of Radiance game files.";
        return result;
    }
    Ref<JSON> json;
    json.instantiate();
    if (json->parse(FileAccess::get_file_as_string("res://config/por-pc13-md5.json")) != OK ||
        json->get_data().get_type() != Variant::DICTIONARY)
    {
        result.error = "Cannot read the bundled game-file checksum manifest.";
        return result;
    }
    const Dictionary manifest = json->get_data();
    const Dictionary hashes = manifest.get("files", Dictionary());
    if (hashes.is_empty())
    {
        result.error = "Cannot read the bundled game-file checksum manifest.";
        return result;
    }
    const auto names = hashes.keys();
    for (int i = 0; i < names.size(); ++i)
    {
        const String name = names[i];
        const auto file = directory.path_join(name);
        if (!FileAccess::file_exists(file))
        {
            result.missing.append(name);
            continue;
        }
        const auto actual = FileAccess::get_md5(file);
        if (actual.is_empty())
        {
            result.missing.append(name);
            continue;
        }
        if (actual != String(hashes[name]))
        {
            result.different.append(name);
        }
    }
    // Missing/unreadable source files cannot support the existing loaders. A different
    // revision can be tried after an explicit compatibility warning.
    result.usable = result.missing.is_empty();
    return result;
}

opengold::GoldBoxGame chosen_game()
{
    return current_game;
}

void choose_game(opengold::GoldBoxGame game)
{
    current_game = game;
}

GameRules chosen_rules()
{
    return current_rules;
}

void choose_rules(GameRules rules)
{
    current_rules = rules;
}

GameStart chosen_game_start()
{
    return current_start;
}

void choose_game_start(GameStart start)
{
    current_start = std::move(start);
}

String game_folder(opengold::GoldBoxGame game)
{
    if (game == opengold::GoldBoxGame::pool_of_radiance)
    {
        return game_path();
    }
    if (const auto found = confirmed_folders.find(game); found != confirmed_folders.end())
    {
        return found->second;
    }
    const std::string folder(opengold::game_folder(game));
    const auto saved = read("games", folder.c_str());
    if (!saved.is_empty())
    {
        return saved;
    }
    const auto all_games =
        OS::get_singleton()->get_environment("OPENGOLD_GOLDBOX_DIR").strip_edges();
    return all_games.is_empty() ? String() : all_games.path_join(folder.c_str());
}

bool save_game_folder(opengold::GoldBoxGame game, const String &directory)
{
    if (game == opengold::GoldBoxGame::pool_of_radiance)
    {
        return save_game_path(directory);
    }
    const std::string folder(opengold::game_folder(game));
    if (!save("games", folder.c_str(), directory))
    {
        return false;
    }
    confirmed_folders[game] = directory;
    return true;
}

Validation validate_game_folder(const String &directory, opengold::GoldBoxGame game)
{
    if (game == opengold::GoldBoxGame::pool_of_radiance)
    {
        return validate_game_path(directory);
    }
    Validation result;
    if (directory.is_absolute_path() && DirAccess::dir_exists_absolute(directory) &&
        opengold::identify_game(presentation::path_from_godot(directory)) == game)
    {
        result.usable = true;
        return result;
    }
    result.error = N_("Choose the folder that holds this game's files.");
    return result;
}
} // namespace settings
