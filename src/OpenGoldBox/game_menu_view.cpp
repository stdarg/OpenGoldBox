#include "game_menu_view.h"
#include "application_settings.h"
#include "godot_nodes.h"
#include "guarded_handlers.h"
#include "localization.h"
#include "monster_ability_options.h"
#include "opengold/difficulty.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <algorithm>
#include <array>

using namespace godot;
using presentation::required_node;

namespace
{
// The Options screen's languages, as settings.cfg names them.
constexpr std::array<const char *, 2> language_codes{"en", "es"};
} // namespace

void GameMenuView::_bind_methods()
{
    ADD_SIGNAL(MethodInfo("save_requested", PropertyInfo(Variant::BOOL, "saving")));
    ADD_SIGNAL(MethodInfo("main_menu_requested"));
    ADD_SIGNAL(MethodInfo("closed"));
    // The 3D view art or the Combat art setting changed (#274).
    ADD_SIGNAL(MethodInfo("art_changed"));
}

void GameMenuView::_ready()
{
    i18n::prepare_ui(*this);
    required_node<Button>(*this, "Menu/Resume")
        .connect("pressed", presentation::guarded(this, &GameMenuView::close));
    for (bool saving : {true, false})
    {
        required_node<Button>(*this, saving ? "Menu/Save" : "Menu/Load")
            .connect("pressed", presentation::guarded(this, &GameMenuView::menu_save).bind(saving));
    }
    required_node<Button>(*this, "Menu/Options")
        .connect("pressed", presentation::guarded(this, &GameMenuView::open_options));
    required_node<Button>(*this, "Menu/MainMenu")
        .connect("pressed", presentation::guarded(this, &GameMenuView::exit_to_main_menu));
    required_node<Button>(*this, "Menu/Desktop")
        .connect("pressed", presentation::guarded(this, &GameMenuView::exit_to_desktop));
    required_node<Button>(*this, "Options/Close")
        .connect("pressed", presentation::guarded(this, &GameMenuView::close_options));
    required_node<Button>(*this, "Options/Gameplay/Defaults")
        .connect("pressed", presentation::guarded(this, &GameMenuView::restore_ability_defaults));
    for (std::int64_t section : {0, 1})
    {
        required_node<Button>(*this, section ? "Options/InterfaceTab" : "Options/GameplayTab")
            .connect("pressed",
                     presentation::guarded(this, &GameMenuView::show_section).bind(section));
    }
    build_options();
}

void GameMenuView::_input(const Ref<InputEvent> &event)
{
    presentation::run_guarded(*this,
                              [&]
                              {
                                  respond_to_input(event);
                              });
}

// Escape over Options goes back to the menu; over the menu it closes it.
void GameMenuView::respond_to_input(const Ref<InputEvent> &event)
{
    const Ref<InputEventKey> key = event;
    if (!is_visible() || key.is_null() || !key->is_pressed() || key->is_echo() ||
        key->get_keycode() != Key::KEY_ESCAPE)
    {
        return;
    }
    get_viewport()->set_input_as_handled();
    if (required_node<Control>(*this, "Options").is_visible())
    {
        close_options();
    }
    else
    {
        close();
    }
}

void GameMenuView::open(Allowed allowed)
{
    required_node<Button>(*this, "Menu/Save").set_disabled(!allowed.save);
    required_node<Button>(*this, "Menu/Load").set_disabled(!allowed.load);
    required_node<Button>(*this, "Menu/MainMenu").set_disabled(!allowed.main_menu);
    required_node<Control>(*this, "Options").hide();
    required_node<Control>(*this, "Menu").show();
    show();
    required_node<Button>(*this, "Menu/Resume").grab_focus();
}

void GameMenuView::close()
{
    if (!is_visible())
    {
        return;
    }
    hide();
    emit_signal("closed");
}

bool GameMenuView::is_open() const
{
    return is_visible();
}

void GameMenuView::report_failure(const std::exception &failure)
{
    UtilityFunctions::push_error(String::utf8(failure.what()));
}

void GameMenuView::menu_save(bool saving)
{
    close();
    emit_signal("save_requested", saving);
}

void GameMenuView::exit_to_main_menu()
{
    close();
    emit_signal("main_menu_requested");
}

void GameMenuView::exit_to_desktop()
{
    get_tree()->quit();
}

void GameMenuView::open_options()
{
    required_node<OptionButton>(*this, "Options/Gameplay/Difficulty")
        .select(static_cast<int>(settings::difficulty()));
    // SpinBoxes, which the game's Godot class list leaves out: called by name.
    required_node<Control>(*this, "Options/Gameplay/Challenge")
        .call("set_value_no_signal", settings::encounter_challenge());
    required_node<Control>(*this, "Options/Interface/Zoom")
        .call("set_value_no_signal", settings::combat_zoom_percent());
    required_node<OptionButton>(*this, "Options/Interface/WallArt")
        .select(settings::original_wall_art() ? 1 : 0);
    required_node<OptionButton>(*this, "Options/Interface/CombatArt")
        .select(settings::original_combat_art() ? 1 : 0);
    const auto language = settings::language();
    const auto found = std::find_if(language_codes.begin(), language_codes.end(),
                                    [&](const char *code)
                                    {
                                        return language == code;
                                    });
    required_node<OptionButton>(*this, "Options/Interface/Language")
        .select(found == language_codes.end() ? 0
                                              : static_cast<int>(found - language_codes.begin()));
    auto &rows = required_node<Control>(*this, "Options/Gameplay/Abilities/Rows");
    for (std::size_t row = 0; row < i18n::monster_ability_options.size(); ++row)
    {
        const auto value =
            settings::monster_ability_option(i18n::monster_ability_options[row].ability);
        auto &from = *Object::cast_to<OptionButton>(
            rows.get_child(static_cast<int>(row))->get_node_or_null("From"));
        from.select(value == "never"
                        ? static_cast<int>(i18n::ability_from_names.size()) - 1
                        : static_cast<int>(*opengold::parse_difficulty(value.utf8().get_data())));
    }
    show_section(0);
    required_node<Control>(*this, "Menu").hide();
    required_node<Control>(*this, "Options").show();
    required_node<Button>(*this, "Options/GameplayTab").grab_focus();
}

void GameMenuView::close_options()
{
    required_node<Control>(*this, "Options").hide();
    required_node<Control>(*this, "Menu").show();
    required_node<Button>(*this, "Menu/Options").grab_focus();
}

void GameMenuView::show_section(std::int64_t section)
{
    required_node<Control>(*this, "Options/Gameplay").set_visible(section == 0);
    required_node<Control>(*this, "Options/Interface").set_visible(section == 1);
    required_node<Button>(*this, "Options/GameplayTab").set_pressed_no_signal(section == 0);
    required_node<Button>(*this, "Options/InterfaceTab").set_pressed_no_signal(section == 1);
}

void GameMenuView::build_options()
{
    auto &difficulty = required_node<OptionButton>(*this, "Options/Gameplay/Difficulty");
    for (std::size_t n = 0; n + 1 < i18n::ability_from_names.size(); ++n)
    {
        difficulty.add_item(i18n::text(i18n::ability_from_names[n]));
    }
    difficulty.connect("item_selected",
                       presentation::guarded(this, &GameMenuView::difficulty_selected));
    required_node<Control>(*this, "Options/Gameplay/Challenge")
        .connect("value_changed", presentation::guarded(this, &GameMenuView::challenge_changed));
    auto &rows = required_node<Control>(*this, "Options/Gameplay/Abilities/Rows");
    for (std::size_t row = 0; row < i18n::monster_ability_options.size(); ++row)
    {
        const auto &option = i18n::monster_ability_options[row];
        auto *line = memnew(HBoxContainer);
        line->set_custom_minimum_size({0, 46});
        auto *name = memnew(Label);
        name->set_text(i18n::text(option.label));
        name->set_custom_minimum_size({440, 0});
        auto *monsters = memnew(Label);
        monsters->set_text(i18n::text(option.monsters));
        monsters->set_theme_type_variation("LabelMuted");
        monsters->set_h_size_flags(Control::SIZE_EXPAND_FILL);
        auto *from = memnew(OptionButton);
        from->set_name("From");
        from->set_custom_minimum_size({240, 40});
        for (const auto *choice : i18n::ability_from_names)
        {
            from->add_item(i18n::text(choice));
        }
        from->connect("item_selected", presentation::guarded(this, &GameMenuView::ability_selected)
                                           .bind(static_cast<std::int64_t>(row)));
        line->add_child(name);
        line->add_child(monsters);
        line->add_child(from);
        rows.add_child(line);
    }
    auto &language = required_node<OptionButton>(*this, "Options/Interface/Language");
    // Each language under its own name, as the first-run language choice shows them.
    language.add_item("English");
    language.add_item(String::utf8("Español"));
    language.connect("item_selected",
                     presentation::guarded(this, &GameMenuView::language_selected));
    required_node<Control>(*this, "Options/Interface/Zoom")
        .connect("value_changed", presentation::guarded(this, &GameMenuView::zoom_changed));
    // The new art or the original game's (#274).
    for (const auto &[path, selected] :
         {std::pair{"Options/Interface/WallArt", &GameMenuView::wall_art_selected},
          std::pair{"Options/Interface/CombatArt", &GameMenuView::combat_art_selected}})
    {
        auto &art = required_node<OptionButton>(*this, path);
        art.add_item(i18n::text(N_("New")));
        art.add_item(i18n::text(N_("Original")));
        art.connect("item_selected", presentation::guarded(this, selected));
    }
}

void GameMenuView::difficulty_selected(std::int64_t index)
{
    settings::save_difficulty(static_cast<opengold::Difficulty>(index));
}

void GameMenuView::challenge_changed(double value)
{
    settings::save_encounter_challenge(static_cast<unsigned>(std::max(0.0, value)));
}

void GameMenuView::ability_selected(std::int64_t index, std::int64_t row)
{
    const auto never = static_cast<std::int64_t>(i18n::ability_from_names.size()) - 1;
    const String from =
        index == never ? String("never")
                       : String(opengold::difficulty_names[static_cast<std::size_t>(index)].data());
    settings::save_monster_ability(
        i18n::monster_ability_options[static_cast<std::size_t>(row)].ability, from);
}

void GameMenuView::restore_ability_defaults()
{
    settings::restore_monster_abilities();
    open_options();
}

void GameMenuView::language_selected(std::int64_t index)
{
    settings::save_language(language_codes.at(static_cast<std::size_t>(index)));
}

void GameMenuView::zoom_changed(double value)
{
    settings::save_combat_zoom(static_cast<int>(value));
}

void GameMenuView::wall_art_selected(std::int64_t index)
{
    settings::save_original_wall_art(index == 1);
    emit_signal("art_changed");
}

void GameMenuView::combat_art_selected(std::int64_t index)
{
    settings::save_original_combat_art(index == 1);
    emit_signal("art_changed");
}
