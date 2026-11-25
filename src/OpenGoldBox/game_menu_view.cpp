#include "game_menu_view.h"
#include <utility>
#include "application_settings.h"
#include "godot_nodes.h"
#include "godot_keys.h"
#include "guarded_handlers.h"
#include "localization.h"
#include "monster_ability_options.h"
#include "opengold/difficulty.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/h_slider.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/file_dialog.hpp>
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

// Each volume's slider, by settings::SoundKind.
constexpr std::array<const char *, 2> volume_sliders{"Options/Interface/Volume/SoundEffects",
                                                     "Options/Interface/Volume/Music"};
constexpr std::array<settings::SoundKind, 2> volume_kinds{settings::SoundKind::effects,
                                                          settings::SoundKind::music};

// The Options rows of the party's area spell limits, in
// GameMenuView::PartySpellLimit's order.
constexpr std::array<const char *, 5> party_spell_rows{"DeathLimit", "HurtShare", "HurtChance",
                                                       "FightEndChance", "PartyWeight"};
} // namespace

void GameMenuView::_bind_methods()
{
    ADD_SIGNAL(MethodInfo("save_requested", PropertyInfo(Variant::BOOL, "saving")));
    ADD_SIGNAL(MethodInfo("quick_save_requested"));
    ADD_SIGNAL(MethodInfo("quick_load_requested"));
    ADD_SIGNAL(MethodInfo("main_menu_requested"));
    ADD_SIGNAL(MethodInfo("closed"));
    // The 3D view art or the Combat art setting changed (#274).
    ADD_SIGNAL(MethodInfo("art_changed"));
    // Another new art folder was chosen and kept.
    ADD_SIGNAL(MethodInfo("new_art_changed"));
    // The sound effects or music volume changed.
    ADD_SIGNAL(MethodInfo("volume_changed"));
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
        required_node<Button>(*this, saving ? "Menu/QuickSave" : "Menu/QuickLoad")
            .connect("pressed",
                     presentation::guarded(this, &GameMenuView::menu_quick).bind(saving));
    }
    required_node<Button>(*this, "Menu/Options")
        .connect("pressed", presentation::guarded(this, &GameMenuView::open_options));
    required_node<Button>(*this, "Menu/MainMenu")
        .connect("pressed", presentation::guarded(this, &GameMenuView::exit_to_main_menu));
    required_node<Button>(*this, "Menu/Desktop")
        .connect("pressed", presentation::guarded(this, &GameMenuView::exit_to_desktop));
    required_node<Button>(*this, "Options/Close")
        .connect("pressed", presentation::guarded(this, &GameMenuView::close_options));
    required_node<Button>(*this, "Options/Interface/NewArtChoose")
        .connect("pressed", presentation::guarded(this, &GameMenuView::choose_new_art_folder));
    required_node<FileDialog>(*this, "Options/Interface/NewArtDialog")
        .connect("dir_selected", presentation::guarded(this, &GameMenuView::new_art_folder_chosen));
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
    if (!is_visible() || !presentation::escape_pressed(event))
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
    // A quick save or load is allowed where the save or load screen is.
    required_node<Button>(*this, "Menu/QuickSave").set_disabled(!allowed.save);
    required_node<Button>(*this, "Menu/QuickLoad").set_disabled(!allowed.load);
    required_node<Button>(*this, "Menu/MainMenu").set_disabled(!allowed.main_menu);
    may_change_art_folder_ = allowed.load;
    srd_rules_ = allowed.srd_rules;
    required_node<Control>(*this, "Options").hide();
    required_node<Control>(*this, "Menu").show();
    show();
    required_node<Button>(*this, "Menu/Resume").grab_focus();
}

void GameMenuView::show_options()
{
    open_options();
}

void GameMenuView::open_options_alone()
{
    options_alone_ = true;
    may_change_art_folder_ = true;
    // Before a game, the settings are there for the SRD games to come.
    srd_rules_ = true;
    required_node<Control>(*this, "Menu").hide();
    show();
    open_options();
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

void GameMenuView::menu_quick(bool saving)
{
    close();
    emit_signal(saving ? "quick_save_requested" : "quick_load_requested");
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
    auto &difficulty =
        required_node<OptionButton>(*this, "Options/Gameplay/Scroll/Page/Choices/Difficulty");
    difficulty.clear();
    if (opengold::offers_novice(settings::chosen_game()))
    {
        for (const auto *name :
             {N_("Novice"), N_("Cadet"), N_("Trooper"), N_("Veteran"), N_("Ace"), N_("Admiral")})
        {
            difficulty.add_item(i18n::text(name));
        }
        difficulty.select(static_cast<int>(settings::difficulty_setting()));
    }
    else
    {
        for (std::size_t n = 0; n + 1 < i18n::ability_from_names.size(); ++n)
        {
            difficulty.add_item(i18n::text(i18n::ability_from_names[n]));
        }
        difficulty.select(static_cast<int>(settings::difficulty()));
    }
    // SpinBoxes, which the game's Godot class list leaves out: called by name.
    required_node<Control>(*this, "Options/Gameplay/Scroll/Page/Choices/Challenge")
        .call("set_value_no_signal", settings::encounter_challenge());
    required_node<Control>(*this, "Options/Interface/Zoom")
        .call("set_value_no_signal", settings::combat_zoom_percent());
    for (std::int64_t kind : {0, 1})
    {
        show_volume(kind);
    }
    required_node<OptionButton>(*this, "Options/Interface/WallArt")
        .select(settings::original_wall_art() ? 1 : 0);
    required_node<OptionButton>(*this, "Options/Interface/CombatArt")
        .select(settings::original_combat_art() ? 1 : 0);
    required_node<OptionButton>(*this, "Options/Interface/PortraitArt")
        .select(settings::original_portraits() ? 1 : 0);
    show_new_art_folder();
    required_node<OptionButton>(*this, "Options/Gameplay/Scroll/Page/Choices/AutoEquip")
        .select(settings::auto_equip() ? 0 : 1);
    required_node<OptionButton>(*this, "Options/Gameplay/Scroll/Page/Choices/UnidentifiedMagic")
        .select(settings::skip_unidentified_magic() ? 0 : 1);
    show_unidentified_magic(settings::auto_equip());
    required_node<OptionButton>(*this, "Options/Gameplay/Scroll/Page/Choices/KeepQuick")
        .select(settings::keep_quick() ? 0 : 1);
    required_node<OptionButton>(*this, "Options/Gameplay/Scroll/Page/Choices/OriginalShops")
        .select(settings::original_shops(opengold::GoldBoxGame::pool_of_radiance) ? 0 : 1);
    required_node<OptionButton>(*this, "Options/Gameplay/Scroll/Page/Choices/TreasuresMoney")
        .select(settings::gateway_money_kept() ? 0 : 1);
    required_node<OptionButton>(*this, "Options/Gameplay/Scroll/Page/Choices/DarknessGazes")
        .select(settings::darkness_gazes() ? 0 : 1);
    required_node<OptionButton>(*this, "Options/Gameplay/Scroll/Page/Choices/DesignDifficulty")
        .select(static_cast<int>(settings::design_difficulty()));
    show_party_spell_limits();
    show_game_options();
    const auto language = settings::language();
    const auto *const found = std::find_if(language_codes.begin(), language_codes.end(),
                                           [&](const char *code)
                                           {
                                               return language == code;
                                           });
    required_node<OptionButton>(*this, "Options/Interface/Language")
        .select(found == language_codes.end() ? 0
                                              : static_cast<int>(found - language_codes.begin()));
    auto &rows = required_node<Control>(*this, "Options/Gameplay/Scroll/Page/Abilities/Rows");
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
    show_srd_balance();
    show_section(0);
    required_node<Control>(*this, "Menu").hide();
    required_node<Control>(*this, "Options").show();
    required_node<Button>(*this, "Options/GameplayTab").grab_focus();
}

void GameMenuView::close_options()
{
    if (std::exchange(options_alone_, false))
    {
        required_node<Control>(*this, "Options").hide();
        close();
        return;
    }
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

namespace
{
// The game's own on/off options: each row's dropdown (On, then Off), the
// setting it reads and saves for the game being played, and whether only a
// Buck Rogers game shows it. The ship-combat and solar-map options are the
// Buck Rogers games'; whether fights under the original rules move as the
// original's is every game's.
struct GameSwitch
{
    const char *node;
    bool (*read)(opengold::GoldBoxGame);
    bool (*save)(opengold::GoldBoxGame, bool);
    bool buck_rogers_only;
};

constexpr std::array game_switches{
    GameSwitch{"CommandEveryRound", settings::command_every_round,
               settings::save_command_every_round, true},
    GameSwitch{"EmptyHelmDodges", settings::empty_helm_dodges, settings::save_empty_helm_dodges,
               true},
    GameSwitch{"BoardingFuel", settings::boarding_fuel, settings::save_boarding_fuel, true},
    GameSwitch{"DirectFlight", settings::direct_flight, settings::save_direct_flight, true},
    GameSwitch{"CourseFlown", settings::course_flown, settings::save_course_flown, true},
    GameSwitch{"ShipPictureShrinks", settings::ship_picture_shrinks,
               settings::save_ship_picture_shrinks, true},
    GameSwitch{"OriginalMovement", settings::original_movement, settings::save_original_movement,
               false}};
} // namespace

void GameMenuView::build_options()
{
    auto &difficulty =
        required_node<OptionButton>(*this, "Options/Gameplay/Scroll/Page/Choices/Difficulty");
    difficulty.connect("item_selected",
                       presentation::guarded(this, &GameMenuView::difficulty_selected));
    required_node<Control>(*this, "Options/Gameplay/Scroll/Page/Choices/Challenge")
        .connect("value_changed", presentation::guarded(this, &GameMenuView::challenge_changed));
    // Auto-equip (EQUIP-1), Keep Quick and Original Shops: each On, then Off.
    for (const auto &[path, selected] :
         {std::pair{"Options/Gameplay/Scroll/Page/Choices/AutoEquip",
                    &GameMenuView::auto_equip_selected},
          std::pair{"Options/Gameplay/Scroll/Page/Choices/UnidentifiedMagic",
                    &GameMenuView::unidentified_magic_selected},
          std::pair{"Options/Gameplay/Scroll/Page/Choices/KeepQuick",
                    &GameMenuView::keep_quick_selected},
          std::pair{"Options/Gameplay/Scroll/Page/Choices/OriginalShops",
                    &GameMenuView::original_shops_selected},
          std::pair{"Options/Gameplay/Scroll/Page/Choices/TreasuresMoney",
                    &GameMenuView::treasures_money_selected},
          std::pair{"Options/Gameplay/Scroll/Page/Choices/DarknessGazes",
                    &GameMenuView::darkness_gazes_selected}})
    {
        auto &choice = required_node<OptionButton>(*this, path);
        choice.add_item(i18n::text(N_("On")));
        choice.add_item(i18n::text(N_("Off")));
        choice.connect("item_selected", presentation::guarded(this, selected));
    }
    auto &gameplay = required_node<Control>(*this, "Options/Gameplay/Scroll/Page/Choices");
    for (std::size_t option = 0; option < game_switches.size(); ++option)
    {
        auto &choice = required_node<OptionButton>(gameplay, game_switches[option].node);
        choice.add_item(i18n::text(N_("On")));
        choice.add_item(i18n::text(N_("Off")));
        choice.connect("item_selected",
                       presentation::guarded(this, &GameMenuView::game_switch_selected)
                           .bind(static_cast<std::int64_t>(option)));
    }
    // How the difficulty plays in an Unlimited Adventures design, in
    // ua::DesignDifficulty's order.
    auto &design_difficulty =
        required_node<OptionButton>(*this, "Options/Gameplay/Scroll/Page/Choices/DesignDifficulty");
    for (const char *choice : {N_("The design's"), N_("OpenGoldBox's"), N_("Both")})
    {
        design_difficulty.add_item(i18n::text(choice));
    }
    design_difficulty.connect(
        "item_selected", presentation::guarded(this, &GameMenuView::design_difficulty_selected));
    for (std::size_t row = 0; row < party_spell_rows.size(); ++row)
    {
        required_node<Control>(*this, NodePath(String("Options/Gameplay/Scroll/Page/PartyLimits/") +
                                               party_spell_rows.at(row)))
            .connect("value_changed",
                     presentation::guarded(this, &GameMenuView::party_spell_limit_changed)
                         .bind(static_cast<std::int64_t>(row)));
    }
    auto &rows = required_node<Control>(*this, "Options/Gameplay/Scroll/Page/Abilities/Rows");
    for (std::size_t row = 0; row < i18n::monster_ability_options.size(); ++row)
    {
        const auto &option = i18n::monster_ability_options[row];
        auto line = presentation::make_node<HBoxContainer>();
        line->set_custom_minimum_size({0, 46});
        auto name = presentation::make_node<Label>();
        name->set_text(i18n::text(option.label));
        name->set_custom_minimum_size({300, 0});
        name->set("autowrap_mode", 3);
        auto monsters = presentation::make_node<Label>();
        monsters->set_text(i18n::text(option.monsters));
        monsters->set("autowrap_mode", 3);
        monsters->set_theme_type_variation("LabelMuted");
        monsters->set_h_size_flags(Control::SIZE_EXPAND_FILL);
        auto from = presentation::make_node<OptionButton>();
        from->set_name("From");
        from->set_custom_minimum_size({180, 40});
        for (const auto *choice : i18n::ability_from_names)
        {
            from->add_item(i18n::text(choice));
        }
        from->connect("item_selected", presentation::guarded(this, &GameMenuView::ability_selected)
                                           .bind(static_cast<std::int64_t>(row)));
        presentation::attach_child(*line, std::move(name));
        presentation::attach_child(*line, std::move(monsters));
        presentation::attach_child(*line, std::move(from));
        presentation::attach_child(rows, std::move(line));
    }
    auto &language = required_node<OptionButton>(*this, "Options/Interface/Language");
    // Each language under its own name, as the first-run language choice shows them.
    language.add_item("English");
    language.add_item(String::utf8("Español"));
    language.connect("item_selected",
                     presentation::guarded(this, &GameMenuView::language_selected));
    required_node<Control>(*this, "Options/Interface/Zoom")
        .connect("value_changed", presentation::guarded(this, &GameMenuView::zoom_changed));
    for (std::int64_t kind : {0, 1})
    {
        required_node<HSlider>(*this, volume_sliders.at(static_cast<std::size_t>(kind)))
            .connect("value_changed",
                     presentation::guarded(this, &GameMenuView::volume_changed).bind(kind));
    }
    // The new art or the original game's (#274).
    for (const auto &[path, selected] :
         {std::pair{"Options/Interface/WallArt", &GameMenuView::wall_art_selected},
          std::pair{"Options/Interface/CombatArt", &GameMenuView::combat_art_selected},
          std::pair{"Options/Interface/PortraitArt", &GameMenuView::portrait_art_selected}})
    {
        auto &art = required_node<OptionButton>(*this, path);
        art.add_item(i18n::text(N_("New")));
        art.add_item(i18n::text(N_("Original")));
        art.connect("item_selected", presentation::guarded(this, selected));
    }
}

void GameMenuView::difficulty_selected(std::int64_t index)
{
    if (opengold::offers_novice(settings::chosen_game()))
    {
        settings::save_difficulty_setting(static_cast<opengold::DifficultySetting>(index));
    }
    else
    {
        settings::save_difficulty(static_cast<opengold::Difficulty>(index));
    }
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

// The encounter challenge and the monster abilities balance the SRD rules
// only: under the original rules every difficulty plays the original's fights
// as they were (docs/ORIGINAL-RULES.md), but in an Unlimited Adventures
// design, where Design difficulty says how it plays.
void GameMenuView::show_srd_balance()
{
    auto &gameplay = required_node<Control>(*this, "Options/Gameplay/Scroll/Page");
    // A Buck Rogers game has no rules but the original's, and shows Original
    // movement where the SRD balance's rows would stand.
    const bool buck_rogers = opengold::is_buck_rogers(settings::chosen_game());
    const bool srd_balance = srd_rules_ && !buck_rogers;
    for (const char *name : {"Choices/ChallengeLabel", "Choices/Challenge", "Choices/ChallengeNote",
                             "AbilitiesHeader", "AbilityColumns", "Abilities"})
    {
        required_node<Control>(gameplay, name).set_visible(srd_balance);
    }
    required_node<Control>(*this, "Options/Gameplay/Defaults").set_visible(srd_balance);
    for (const char *name : {"OriginalMovementLabel", "OriginalMovement", "OriginalMovementNote"})
    {
        required_node<Control>(gameplay, NodePath(String("Choices/") + name))
            .set_visible(!srd_balance);
    }
    // The party's area spells weigh their risks under the original rules,
    // whose spells a Buck Rogers game has none of.
    const bool party_spells = !srd_balance && !buck_rogers;
    required_node<Control>(gameplay, "PartyLimits").set_visible(party_spells);
    required_node<Control>(gameplay, "PartySpellsHeader").set_visible(party_spells);
    required_node<Control>(gameplay, "PartySpellsNote").set_visible(party_spells);
    for (const char *row : party_spell_rows)
    {
        for (const char *part : {"Label", ""})
        {
            required_node<Control>(gameplay, NodePath(String("PartyLimits/") + row + part))
                .set_visible(party_spells);
        }
    }
    required_node<Label>(gameplay, "Choices/DifficultyNote")
        .set_text(
            srd_balance
                ? i18n::text(N_(
                      "From Hard: monsters use their potions, wands and scrolls, and temples cast no Resurrection. Each monster ability below has its own lowest difficulty."))
            : opengold::is_buck_rogers(settings::chosen_game())
                ? i18n::text(N_(
                      "Trooper plays as the original game did. The other levels change the party's to-hit and the monsters' Hit Points, to-hit and Armor Class."))
            : settings::chosen_game() == opengold::GoldBoxGame::unlimited_adventures
                ? i18n::text(N_(
                      "In this design, the difficulty plays as Design difficulty below sets it."))
                : i18n::text(N_(
                      "Under the original rules, Normal plays as the original game did; the other levels change only the monsters' to-hit, Hit Points and Armor Class.")));
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

void GameMenuView::volume_changed(double value, std::int64_t kind)
{
    settings::save_volume_percent(volume_kinds.at(static_cast<std::size_t>(kind)),
                                  static_cast<int>(value));
    show_volume(kind);
    emit_signal("volume_changed");
}

// The slider and, beside it, its percentage.
void GameMenuView::show_volume(std::int64_t kind)
{
    const auto index = static_cast<std::size_t>(kind);
    const auto percent = settings::volume_percent(volume_kinds.at(index));
    auto &slider = required_node<HSlider>(*this, volume_sliders.at(index));
    slider.set_value_no_signal(percent);
    required_node<Label>(*this, NodePath(String(volume_sliders.at(index)) + "Percent"))
        .set_text(String::num_int64(percent) + "%");
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

void GameMenuView::portrait_art_selected(std::int64_t index)
{
    settings::save_original_portraits(index == 1);
    emit_signal("art_changed");
}

void GameMenuView::show_new_art_folder(const String &problem)
{
    const auto folder = settings::new_art_directory();
    required_node<Label>(*this, "Options/Interface/NewArtFolder")
        .set_text(folder.is_empty() ? i18n::text(N_("None")) : folder);
    required_node<Button>(*this, "Options/Interface/NewArtChoose")
        .set_disabled(!may_change_art_folder_);
    required_node<Label>(*this, "Options/Interface/NewArtNote")
        .set_text(
            !problem.is_empty() ? i18n::text(problem.utf8().get_data())
            : !may_change_art_folder_
                ? i18n::text(N_("Finish the fight or event to choose another folder."))
            : folder.is_empty()
                ? i18n::text(N_(
                      "The original game's art shows until you choose the folder of the OpenGoldBox new art download."))
                : i18n::text(
                      N_("The art settings above choose the new art or the original game's.")));
}

void GameMenuView::choose_new_art_folder()
{
    if (!may_change_art_folder_)
    {
        return;
    }
    auto &dialog = required_node<FileDialog>(*this, "Options/Interface/NewArtDialog");
    const auto folder = settings::new_art_directory();
    if (!folder.is_empty())
    {
        dialog.set_current_dir(folder);
    }
    dialog.popup_centered_ratio(.7f);
}

void GameMenuView::new_art_folder_chosen(const String &folder)
{
    const auto problem = settings::new_art_problem(folder);
    if (!problem.is_empty())
    {
        show_new_art_folder(problem);
        return;
    }
    if (!settings::save_new_art_directory(folder))
    {
        show_new_art_folder(N_("Cannot save the settings."));
        return;
    }
    show_new_art_folder();
    emit_signal("new_art_changed");
}

void GameMenuView::auto_equip_selected(std::int64_t index)
{
    settings::save_auto_equip(index == 0);
    show_unidentified_magic(index == 0);
}

void GameMenuView::unidentified_magic_selected(std::int64_t index)
{
    settings::save_skip_unidentified_magic(index == 0);
}

void GameMenuView::keep_quick_selected(std::int64_t index)
{
    settings::save_keep_quick(index == 0);
}

// Only Pool of Radiance has modified shops so far.
void GameMenuView::original_shops_selected(std::int64_t index)
{
    settings::save_original_shops(opengold::GoldBoxGame::pool_of_radiance, index == 0);
}

void GameMenuView::treasures_money_selected(std::int64_t index)
{
    settings::save_gateway_money_kept(index == 0);
}

void GameMenuView::darkness_gazes_selected(std::int64_t index)
{
    settings::save_darkness_gazes(index == 0);
}

void GameMenuView::design_difficulty_selected(std::int64_t index)
{
    settings::save_design_difficulty(static_cast<opengold::ua::DesignDifficulty>(index));
}

void GameMenuView::game_switch_selected(std::int64_t index, std::int64_t option)
{
    game_switches.at(static_cast<std::size_t>(option)).save(settings::chosen_game(), index == 0);
}

void GameMenuView::show_game_options()
{
    const auto game = settings::chosen_game();
    const bool buck_rogers = opengold::is_buck_rogers(game);
    auto &gameplay = required_node<Control>(*this, "Options/Gameplay/Scroll/Page/Choices");
    // Unlimited Adventures' Design difficulty steps aside too, for the solar
    // map's rows.
    for (const char *name :
         {"OriginalShopsLabel", "OriginalShops", "OriginalShopsNote", "TreasuresMoneyLabel",
          "TreasuresMoney", "TreasuresMoneyNote", "DarknessGazesLabel", "DarknessGazes",
          "DarknessGazesNote", "DesignDifficultyLabel", "DesignDifficulty", "DesignDifficultyNote"})
    {
        required_node<Control>(gameplay, name).set_visible(!buck_rogers);
    }
    for (const auto &row : game_switches)
    {
        if (row.buck_rogers_only)
        {
            for (const char *suffix : {"Label", "", "Note"})
            {
                required_node<Control>(gameplay, String(row.node) + suffix)
                    .set_visible(buck_rogers);
            }
        }
        if (buck_rogers || !row.buck_rogers_only)
        {
            required_node<OptionButton>(gameplay, row.node).select(row.read(game) ? 0 : 1);
        }
    }
}

// Each limit as a percentage, the party's weight as it is.
void GameMenuView::show_party_spell_limits()
{
    const auto limits = settings::party_spell_limits();
    const std::array<double, 5> shown{limits.death_limit * 100, limits.hurt_share * 100,
                                      limits.hurt_chance * 100, limits.fight_end_chance * 100,
                                      limits.party_weight};
    for (std::size_t row = 0; row < party_spell_rows.size(); ++row)
    {
        // SpinBoxes, which the game's Godot class list leaves out: called by name.
        required_node<Control>(*this, NodePath(String("Options/Gameplay/Scroll/Page/PartyLimits/") +
                                               party_spell_rows.at(row)))
            .call("set_value_no_signal", shown.at(row));
    }
}

void GameMenuView::party_spell_limit_changed(double value, std::int64_t row)
{
    auto limits = settings::party_spell_limits();
    const double share = value / 100;
    switch (static_cast<PartySpellLimit>(row))
    {
    case PartySpellLimit::death_limit:
        limits.death_limit = share;
        break;
    case PartySpellLimit::hurt_share:
        limits.hurt_share = share;
        break;
    case PartySpellLimit::hurt_chance:
        limits.hurt_chance = share;
        break;
    case PartySpellLimit::fight_end_chance:
        limits.fight_end_chance = share;
        break;
    case PartySpellLimit::party_weight:
        limits.party_weight = value;
        break;
    }
    settings::save_party_spell_limits(limits);
}

void GameMenuView::show_unidentified_magic(bool shown)
{
    required_node<Control>(*this, "Options/Gameplay/Scroll/Page/Choices/UnidentifiedMagicNote")
        .set_visible(shown);
    required_node<Control>(*this, "Options/Gameplay/Scroll/Page/Choices/UnidentifiedMagicLabel")
        .set_visible(shown);
    required_node<Control>(*this, "Options/Gameplay/Scroll/Page/Choices/UnidentifiedMagic")
        .set_visible(shown);
}
