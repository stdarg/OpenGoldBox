#include "application_settings.h"
#include "opengold/encounter_budget.h"
#include <godot_cpp/classes/config_file.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <algorithm>
#include <optional>

using namespace godot;

namespace
{
std::optional<String> confirmed_path, confirmed_language;

String read(const char *section, const char *key)
{
    Ref<ConfigFile> config;
    config.instantiate();
    if (config->load(settings::path()) != OK)
        return {};
    const Variant value = config->get_value(section, key, String());
    return value.get_type() == Variant::STRING ? String(value).strip_edges() : String();
}

bool save(const char *section, const char *key, const String &value)
{
    Ref<ConfigFile> config;
    config.instantiate();
    const auto loaded = config->load(settings::path());
    // Do not overwrite unreadable or malformed configuration and unrelated keys.
    if (loaded != OK && loaded != ERR_FILE_NOT_FOUND)
        return false;
    config->set_value(section, key, value);
    if (!config->has_section_key("combat", "combat_zoom"))
        config->set_value("combat", "combat_zoom", 100);
    if (!config->has_section_key("combat", "encounter_challenge"))
        config->set_value("combat", "encounter_challenge",
                          static_cast<int>(opengold::default_encounter_challenge));
    const String temporary = settings::path() + ".tmp";
    if (config->save(temporary) != OK)
        return false;
    if (DirAccess::rename_absolute(temporary, settings::path()) == OK)
        return true;
    DirAccess::remove_absolute(temporary);
    return false;
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
    // Editor runs share Godot's executable, so keep development settings in the project.
    if (os->has_feature("editor"))
        return ProjectSettings::get_singleton()->globalize_path("res://settings.cfg");
    if (os->has_feature("macos"))
        return ProjectSettings::get_singleton()->globalize_path("user://settings.cfg");
    return os->get_executable_path().get_base_dir().path_join("settings.cfg");
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
            return std::clamp(static_cast<int>(value), 10, 1000);
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
            return static_cast<unsigned>(std::clamp(
                                             static_cast<int>(value), 0,
                                             static_cast<int>(opengold::max_encounter_challenge)));
    }
    return opengold::default_encounter_challenge;
}

bool valid_language(const String &locale)
{
    return locale == "en" || locale == "es";
}

String game_path()
{
    if (confirmed_path)
        return *confirmed_path;
    const auto override = OS::get_singleton()->get_environment("OPENGOLD_GAME_DIR").strip_edges();
    return !flag("--reset-game-path") && !override.is_empty() ? override : saved_game_path();
}

String language()
{
    if (confirmed_language)
        return *confirmed_language;
    const auto override =
        OS::get_singleton()->get_environment("OPENGOLD_LANG").strip_edges().to_lower();
    if (!flag("--reset-lang") && valid_language(override))
        return override;
    const auto saved = saved_language();
    if (valid_language(saved))
        return saved;
    return OS::get_singleton()->get_locale_language().to_lower() == "es" ? "es" : "en";
}

bool save_game_path(const String &directory)
{
    if (!save("game", "path", directory))
        return false;
    confirmed_path = directory;
    return true;
}

bool save_language(const String &locale)
{
    if (!valid_language(locale) || !save("interface", "language", locale))
        return false;
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
            result.different.append(name);
    }
    // Missing/unreadable source files cannot support the existing loaders. A different
    // revision can be tried after an explicit compatibility warning.
    result.usable = result.missing.is_empty();
    return result;
}
} // namespace settings
