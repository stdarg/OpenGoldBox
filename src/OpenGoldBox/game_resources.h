#ifndef OPENGOLDBOX_GAME_RESOURCES_H
#define OPENGOLDBOX_GAME_RESOURCES_H

#include "application_settings.h"
#include "game_rules.h"
#include "godot_path.h"
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>

// The native rules loader reads ordinary files, not Godot's packed resources.
inline godot::String game_rules_file()
{
    constexpr auto relative = "data/rules/srd-5.2.1/combat.rules";
    auto *os = godot::OS::get_singleton(); // borrowed engine singleton
    if (os->has_feature("editor"))
    {
        return godot::ProjectSettings::get_singleton()->globalize_path(godot::String("res://") +
                                                                       relative);
    }
    const auto executable_dir = os->get_executable_path().get_base_dir();
    if (os->has_feature("macos"))
    {
        return executable_dir.get_base_dir().path_join("Resources").path_join(relative);
    }
    return executable_dir.path_join(relative);
}

inline godot::String game_combat_body_file()
{
    constexpr auto relative = "data/art/combat-body-looks.tsv";
    auto *os = godot::OS::get_singleton();
    if (os->has_feature("editor"))
    {
        return godot::ProjectSettings::get_singleton()->globalize_path(godot::String("res://") +
                                                                       relative);
    }
    const auto executable_dir = os->get_executable_path().get_base_dir();
    if (os->has_feature("macos"))
    {
        return executable_dir.get_base_dir().path_join("Resources").path_join(relative);
    }
    return executable_dir.path_join(relative);
}

inline godot::String game_combat_weapon_file()
{
    constexpr auto relative = "data/art/combat-weapon-options.tsv";
    auto *os = godot::OS::get_singleton();
    if (os->has_feature("editor"))
    {
        return godot::ProjectSettings::get_singleton()->globalize_path(godot::String("res://") +
                                                                       relative);
    }
    const auto executable_dir = os->get_executable_path().get_base_dir();
    if (os->has_feature("macos"))
    {
        return executable_dir.get_base_dir().path_join("Resources").path_join(relative);
    }
    return executable_dir.path_join(relative);
}

// A file or folder in the new art's folder, such as "walls"; empty without the
// new art, when the game shows the original art.
inline godot::String new_art_path(const char *relative)
{
    const auto folder = settings::new_art_directory();
    return folder.is_empty() ? godot::String() : folder.path_join(relative);
}

// What each set of rules plays with: the SRD's content pack and the chosen
// game's folder.
inline RulesFiles game_rules_files()
{
    const auto game = settings::chosen_game();
    return {presentation::path_from_godot(game_rules_file()),
            presentation::path_from_godot(settings::game_folder(game)), game};
}

// A file or folder the game ships beside its executable, such as data/shops.
inline godot::String game_data_path(const char *relative)
{
    auto *os = godot::OS::get_singleton();
    if (os->has_feature("editor"))
    {
        return godot::ProjectSettings::get_singleton()->globalize_path(godot::String("res://") +
                                                                       relative);
    }
    const auto executable_dir = os->get_executable_path().get_base_dir();
    if (os->has_feature("macos"))
    {
        return executable_dir.get_base_dir().path_join("Resources").path_join(relative);
    }
    return executable_dir.path_join(relative);
}

#endif
