#include "ship_combat_view.h"
#include "application_settings.h"
#include "godot_images.h"
#include "godot_nodes.h"
#include "godot_path.h"
#include "guarded_handlers.h"
#include "localization.h"
#include "opengold/buck_rogers_characters.h"
#include "opengold/buck_rogers_rules.h"
#include "opengold/campaign_party.h"
#include "opengold/campaign_save.h"
#include "opengold/creature_art.h"
#include "opengold/dice.h"
#include "opengold/original.h"
#include "opengold/original_combat.h"
#include "opengold/original_content.h"
#include "opengold/rolf_tour.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/grid_container.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/margin_container.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <utility>

using namespace godot;
using presentation::required_node;
namespace original = opengold::original;
namespace por = opengold::por;
using original::ShipAction;
using original::ShipEventKind;
using original::ShipOrder;
using original::ShipSide;
using original::ShipSystem;

namespace
{
constexpr std::size_t system_count = opengold::buck_rogers::Ship::systems;

// The systems by the record's order, then the weapons, which a shot may aim
// at too.
constexpr std::array<const char *, 7> system_names{
    N_("Hull"), N_("Sensors"), N_("Controls"), N_("Life support"),
    N_("Fuel"), N_("Engines"), N_("Weapons")};

// The weapon kinds 1 to 8 (gdconsts.pas shipWeapons), as the screen names them.
constexpr std::array<const char *, 8> weapon_names{
    N_("Beam laser"),     N_("Pumped laser"), N_("Gyrocannon"),       N_("Missiles"),
    N_("Heavy missiles"), N_("K-cannon"),     N_("Light accel. gun"), N_("Heavy accel. gun")};

// buck_rogers_characters.cpp's names, by the record's bytes.
constexpr std::array<const char *, 9> race_names{
    N_("Monster"), N_("Terran"),        N_("Martian"),  N_("Venusian"), N_("Mercurian"),
    N_("Tinker"),  N_("Desert Runner"), N_("Lunarian"), N_("Lowlander")};
constexpr std::array<const char *, 6> career_names{N_("Rocket Jock"), N_("Medic"), N_("Warrior"),
                                                   N_("Engineer"),    N_("Rogue"), N_("Scout")};
constexpr std::array<const char *, 2> gender_names{N_("Male"), N_("Female")};

// The ship types (gdconsts.pas shipTypeNames), by the record's type byte.
constexpr std::array<const char *, 11> ship_type_names{
    N_("Fighter"),         N_("Asterover"),      N_("Scout cruiser"),   N_("Light freighter"),
    N_("Heavy freighter"), N_("Medium cruiser"), N_("Light transport"), N_("Heavy transport"),
    N_("Heavy cruiser"),   N_("Space liner"),    N_("Battler")};

// A member's status, by original::BuckRogersStatus.
constexpr std::array<const char *, 15> status_names{N_("No status"),
                                                    N_("Okay"),
                                                    N_("Gone"),
                                                    N_("Dead"),
                                                    N_("Dying"),
                                                    N_("Unconscious"),
                                                    N_("Fleeing"),
                                                    N_("Poisoned"),
                                                    N_("Comatose"),
                                                    N_("Not here"),
                                                    N_("Dodging"),
                                                    N_("Sprinting"),
                                                    N_("Failed in zero gravity"),
                                                    N_("Used a jetpack"),
                                                    N_("Using a jetpack")};

// The orders, by their buttons, in the original's order (shipcbt.pas
// BuildPlayerMenu): the pilot's Fire Target Quit View / Close Withdraw Ram
// Board, the crew's Fire Target Load Jury rig Quit View / Command Aid Boost
// engines Sensors, and the key each answers to.
struct OrderButton
{
    ShipAction action;
    const char *path;
    Key key;
    bool pilot, crew;
};

// View is no order of the rules; it shows a ship or a member.
constexpr auto view_action = static_cast<ShipAction>(255);
constexpr std::array<OrderButton, 14> order_buttons{{
    {ShipAction::fire, "First/Fire", KEY_F, true, true},
    {ShipAction::target, "First/Target", KEY_T, true, true},
    {ShipAction::load, "First/Load", KEY_L, false, true},
    {ShipAction::jury_rig, "First/JuryRig", KEY_J, false, true},
    {ShipAction::quit, "First/Quit", KEY_Q, true, true},
    {view_action, "First/View", KEY_V, true, true},
    {ShipAction::close, "Second/Close", KEY_C, true, false},
    {ShipAction::withdraw, "Second/Withdraw", KEY_W, true, false},
    {ShipAction::ram, "Second/Ram", KEY_R, true, false},
    {ShipAction::board, "Second/Board", KEY_B, true, false},
    {ShipAction::command, "Second/Command", KEY_C, false, true},
    {ShipAction::aid, "Second/Aid", KEY_A, false, true},
    {ShipAction::boost_engines, "Second/Boost", KEY_B, false, true},
    {ShipAction::sensors, "Second/Sensors", KEY_S, false, true},
}};

// The picture's share of the scene at range 8, when it shrinks with the
// range; at range 1 or less it fills the scene.
constexpr double farthest_share = 0.4;
constexpr int scanner_range = 8;

[[nodiscard]] const char *named(std::span<const char *const> names, std::size_t index)
{
    return index < names.size() ? names[index] : "?";
}

// The record keeps the career from 1, Rocketjock (buck_rogers_rules.h
// career_from_record).
[[nodiscard]] String career_name(const opengold::buck_rogers::Creature &character)
{
    return character.career >= 1
               ? i18n::text(named(career_names, static_cast<std::size_t>(character.career - 1)))
               : String("?");
}

[[nodiscard]] String weapon_name(int kind)
{
    return kind >= 1 && kind <= 8 ? i18n::text(weapon_names[static_cast<std::size_t>(kind - 1)])
                                  : String("?");
}

[[nodiscard]] String system_name(ShipSystem system)
{
    return i18n::text(named(system_names, static_cast<std::size_t>(system)));
}

[[nodiscard]] int kind_of(const opengold::buck_rogers::Ship::WeaponMount &mount) noexcept
{
    return mount.kind & 0x7F;
}

[[nodiscard]] Color palette(const Control &control, const char *name)
{
    return control.get_theme_color(name, "OpenGoldPalette");
}

// A detached label, owned by the caller until it is attached.
[[nodiscard]] presentation::NodeOwner<Label> new_label(const String &text, const char *variation)
{
    auto label = presentation::make_node<Label>();
    label->set_text(text);
    label->set_theme_type_variation(variation);
    return label;
}

void clear_children(Node &node)
{
    // Freed at once: no child is emitting a signal while its row is rebuilt
    // (the answers' buttons are connected deferred).
    while (node.get_child_count() > 0)
    {
        presentation::detach_child(node, *node.get_child(0)).reset();
    }
}

[[nodiscard]] std::vector<std::uint8_t> file_bytes(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        throw std::runtime_error("Cannot read " + path.string());
    }
    return {std::istreambuf_iterator<char>(in), {}};
}

[[nodiscard]] NodePath path(const std::string &text)
{
    return NodePath(String::utf8(text.c_str()));
}

[[nodiscard]] Ref<Texture2D> texture_of(const opengold::Image &image)
{
    if (image.width == 0 || image.height == 0)
    {
        return {};
    }
    return presentation::image_texture(image);
}
} // namespace

void ShipCombatView::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("stage_check", "folder", "save_path", "enemy_type", "pilot_skill",
                                  "to_hit", "seed", "picture_shrinks"),
                         &ShipCombatView::stage_check);
    ClassDB::bind_method(D_METHOD("check_outcome"), &ShipCombatView::check_outcome);
    ClassDB::bind_method(D_METHOD("check_boarded"), &ShipCombatView::check_boarded);
    ClassDB::bind_method(D_METHOD("picture_share"), &ShipCombatView::picture_share);
    ClassDB::bind_method(D_METHOD("done"), &ShipCombatView::done);
}

void ShipCombatView::report_failure(const std::exception &failure) noexcept
{
    try
    {
        log_line(String::utf8(failure.what()), palette(*this, "score_negative"));
    }
    catch (const std::exception &)
    {
        // Already in the game's log.
    }
}

void ShipCombatView::_ready()
{
    presentation::run_guarded(*this,
                              [this]
                              {
                                  prepare();
                              });
}

void ShipCombatView::prepare()
{
    if (prepared_)
    {
        return;
    }
    prepared_ = true;
    i18n::prepare_ui(*this);
    auto &orders = required_node<Control>(*this, "Margin/Columns/Main/Orders");
    for (const auto &order : order_buttons)
    {
        required_node<Button>(orders, order.path)
            .connect("pressed", presentation::guarded(this, &ShipCombatView::order_pressed)
                                    .bind(static_cast<std::int64_t>(order.action)));
    }
    required_node<Control>(*this, "Margin/Columns/Main/Scene")
        .connect("resized", presentation::guarded(this, &ShipCombatView::place_picture));
    // Six rows of each ship's systems: icon, name, bar and value.
    for (const char *ship : {"Enemy", "Party"})
    {
        auto &grid = required_node<GridContainer>(
            *this, path(std::string("Margin/Columns/Main/Ships/") + ship + "/Margin/Rows/Systems"));
        for (std::size_t system = 0; system < system_count; ++system)
        {
            presentation::attach_child(grid,
                                       presentation::instantiate_control<TextureRect>(
                                           "res://scenes/control_templates/ship_system_icon.tscn"));
            auto name = presentation::instantiate_control<Label>(
                "res://scenes/control_templates/ship_system_name.tscn");
            name->set_text(system_name(static_cast<ShipSystem>(system)));
            presentation::attach_child(grid, std::move(name));
            presentation::attach_child(grid,
                                       presentation::instantiate_control<HBoxContainer>(
                                           "res://scenes/control_templates/ship_system_bar.tscn"));
            presentation::attach_child(
                grid, presentation::instantiate_control<Label>(
                          "res://scenes/control_templates/ship_system_value.tscn"));
        }
    }
}

void ShipCombatView::play(por::RolfTourSession &session)
{
    prepare();
    const auto game = session.game();
    const original::ShipCombatOptions options{
        .command_every_round = settings::command_every_round(game),
        .empty_helm_dodges = settings::empty_helm_dodges(game),
        .no_boarding_fuel = !settings::boarding_fuel(game)};
    Fight fight{.game = game,
                .enemy_type =
                    session.snapshot().ship_combat ? session.snapshot().ship_combat->enemy_type : 1,
                .combat = &por::ship_fight(session, options),
                .files = &por::ship_fight_files(session)};
    for (const auto &art : session.buck_rogers_team_art())
    {
        fight.icons.push_back(texture_of(art.image));
    }
    auto *played = &session;
    fight.finish = [played]() -> const por::ShipFightAftermath *
    {
        if (!played->finish_ship_combat(played->snapshot().continue_ticket))
        {
            throw std::runtime_error("The script did not take the ship fight's end");
        }
        return por::ship_fight_aftermath(*played);
    };
    begin(std::move(fight), settings::ship_picture_shrinks(game));
}

bool ShipCombatView::stage_check(const String &folder, const String &save_path,
                                 std::int64_t enemy_type, std::int64_t pilot_skill,
                                 std::int64_t to_hit, std::int64_t seed, bool picture_shrinks)
{
    bool staged = false;
    presentation::run_guarded(
        *this,
        [&]
        {
            prepare();
            const auto directory = presentation::path_from_godot(folder);
            const auto bytes =
                opengold::read_campaign_file(presentation::path_from_godot(save_path));
            const auto game = opengold::saved_campaign_rules(bytes).game;
            std::shared_ptr<const opengold::rules::CharacterRules> creation =
                original::character_rules(original::default_aging, game);
            const auto content = opengold::load_original_content(directory, game);
            const auto module = original::rules_module(content);
            const auto saved = opengold::decode_campaign(
                bytes, *creation, *module, opengold::campaign_asset_identity(directory),
                []() -> por::RolfTourSession
                {
                    throw std::runtime_error("The check's save holds the team alone");
                });
            opengold::CampaignParty party(original::rules_module(content), creation);
            party.restore(saved.party);

            check_ = std::make_unique<CheckFight>();
            const auto ships = opengold::decode_dax_archive(file_bytes(directory / "SHIPS.DAX"));
            check_->files = por::read_ship_fight_files(ships);
            const auto party_record =
                std::ranges::find(ships.records, 50, &opengold::DaxRecord::id);
            auto party_ship = party_record == ships.records.end()
                                  ? std::nullopt
                                  : opengold::buck_rogers::decode_ship(party_record->bytes);
            if (!party_ship)
            {
                throw std::runtime_error("SHIPS.DAX lacks the team's ship");
            }
            const auto icons = file_bytes(directory / "CHARS.DAX");
            Fight fight{.game = game,
                        .enemy_type = static_cast<unsigned>(enemy_type),
                        .files = &check_->files};
            std::vector<original::TeamMember> crew;
            for (const auto id : party.state().slots)
            {
                if (!id)
                {
                    continue;
                }
                const auto record = original::buck_rogers_record_of(party.game_record(id));
                if (!record)
                {
                    throw std::runtime_error("The check's team is not Buck Rogers'");
                }
                crew.push_back({record->creature, record->items, false, false});
                const auto art =
                    por::creature_art(icons, record->creature.icon, nullptr, false, game);
                fight.icons.push_back(art ? texture_of(art->image) : Ref<Texture2D>{});
            }
            const por::ShipCombatCall call{static_cast<unsigned>(enemy_type), 1,
                                           static_cast<unsigned>(pilot_skill),
                                           static_cast<unsigned>(to_hit)};
            auto start = por::ship_combat_start(game, std::move(*party_ship), check_->files, call,
                                                por::enemy_ship_name(call.enemy_type, 0),
                                                std::move(crew), check_->options);
            check_->dice = std::make_shared<opengold::RandomState>(
                opengold::RandomState{static_cast<std::uint64_t>(seed)});
            auto dice = check_->dice;
            check_->combat.emplace(std::move(start),
                                   [dice](int sides)
                                   {
                                       return opengold::roll_die(*dice, sides);
                                   });
            fight.combat = &*check_->combat;
            auto *check = check_.get();
            fight.finish = [check, game]() -> const por::ShipFightAftermath *
            {
                check->aftermath = por::follow_ship_fight(game, check->combat->end(),
                                                          check->options, *check->dice);
                return &*check->aftermath;
            };
            begin(std::move(fight), picture_shrinks);
            staged = true;
        });
    return staged;
}

std::int64_t ShipCombatView::check_outcome() const
{
    if (!fight_ || !fight_->combat->over())
    {
        return -1;
    }
    return fight_->combat->end().outcome;
}

bool ShipCombatView::check_boarded() const
{
    return fight_ && fight_->combat->over() && fight_->combat->end().boarded;
}

double ShipCombatView::picture_share() const
{
    const auto *scene = &required_node<Control>(*this, "Margin/Columns/Main/Scene");
    const auto *frame = &required_node<MarginContainer>(*scene, "ShipFrame");
    const double width = scene->get_size().x;
    if (width <= 0)
    {
        return 0;
    }
    const double margins =
        frame->get_theme_constant("margin_left") + frame->get_theme_constant("margin_right");
    return (width - margins) / width;
}

void ShipCombatView::begin(Fight fight, bool picture_shrinks)
{
    fight_ = std::move(fight);
    picture_shrinks_ = picture_shrinks;
    aftermath_ = nullptr;
    choice_ = Choice::none;
    sensed_ = enemy_retreating_ = finished_ = done_ = false;
    required_node<RichTextLabel>(*this, "Margin/Columns/Main/Log").clear();
    // The enemy ship of type n is picture n - 1 (saveload.pas LoadShipData).
    const auto picture = std::clamp<std::size_t>(fight_->enemy_type, 1, por::enemy_ship_types) - 1;
    required_node<TextureRect>(*this, "Margin/Columns/Main/Scene/ShipFrame/ShipPicture")
        .set_texture(texture_of(fight_->files->enemy_pictures[picture]));
    for (const char *ship : {"Enemy", "Party"})
    {
        const bool enemy_side = std::string_view(ship) == "Enemy";
        auto &grid = required_node<GridContainer>(
            *this, path(std::string("Margin/Columns/Main/Ships/") + ship + "/Margin/Rows/Systems"));
        const auto &icons =
            enemy_side ? fight_->files->enemy_system_icons : fight_->files->party_system_icons;
        for (std::size_t system = 0; system < system_count; ++system)
        {
            auto *icon = Object::cast_to<TextureRect>(grid.get_child(static_cast<int>(system * 4)));
            icon->set_texture(texture_of(icons[system]));
        }
    }
    take_events();
    refresh();
}

void ShipCombatView::_input(const Ref<InputEvent> &event)
{
    presentation::run_guarded(
        *this,
        [&]
        {
            const Ref<InputEventKey> key = event;
            if (!fight_ || key.is_null() || !key->is_pressed() || key->is_echo() ||
                !is_visible_in_tree())
            {
                return;
            }
            const auto code = key->get_keycode();
            if (code == KEY_ENTER || code == KEY_KP_ENTER || code == KEY_ESCAPE)
            {
                auto &answers = required_node<Control>(*this, "Margin/Columns/Main/Choice/Answers");
                const int pick = code == KEY_ESCAPE ? answers.get_child_count() - 1 : 0;
                if (pick >= 0 && choice_ == Choice::none && pick < answers.get_child_count())
                {
                    if (auto *button = Object::cast_to<Button>(answers.get_child(pick)))
                    {
                        get_viewport()->set_input_as_handled();
                        button->emit_signal("pressed");
                    }
                }
                return;
            }
            auto &orders = required_node<Control>(*this, "Margin/Columns/Main/Orders");
            for (const auto &order : order_buttons)
            {
                auto &button = required_node<Button>(orders, order.path);
                if (order.key == code && button.is_visible() && !button.is_disabled())
                {
                    get_viewport()->set_input_as_handled();
                    button.emit_signal("pressed");
                    return;
                }
            }
        });
}

bool ShipCombatView::acting_pilot() const
{
    const auto &combat = *fight_->combat;
    return combat.acting_member() && combat.pilot() == combat.acting_member();
}

bool ShipCombatView::enemy_figures_shown() const
{
    const auto &party = fight_->combat->ship(ShipSide::party);
    return sensed_ && party.system_status[static_cast<std::size_t>(ShipSystem::sensors)] > 0;
}

String ShipCombatView::member_name(std::size_t member) const
{
    const auto crew = fight_->combat->crew();
    return member < crew.size() ? String::utf8(crew[member].character.name.c_str()) : String("?");
}

String ShipCombatView::side_name(ShipSide side) const
{
    return String::utf8(fight_->combat->ship(side).name.c_str());
}

void ShipCombatView::refresh()
{
    if (!fight_)
    {
        return;
    }
    show_scene();
    show_range_track();
    show_ship(true);
    show_ship(false);
    show_crew();
    show_orders();
    show_choice();
    show_status();
}

void ShipCombatView::show_scene()
{
    const auto &combat = *fight_->combat;
    const auto &enemy = combat.ship(ShipSide::enemy);
    const std::string lines = "Margin/Columns/Main/Scene/SceneText/Lines/";
    required_node<Label>(*this, path(lines + "Maneuver"))
        .set_text(i18n::text(enemy_retreating_ ? N_("Retreating") : N_("Closing")));
    required_node<Label>(*this, path(lines + "Range"))
        .set_text(i18n::format(N_("Range: {range}"), {{"range", std::max(combat.range(), 0)}}));
    required_node<Label>(*this, path(lines + "ShipName"))
        .set_text(String::utf8(enemy.name.c_str()));
    const auto type = enemy.type >= 1 ? static_cast<std::size_t>(enemy.type - 1) : 99;
    required_node<Label>(*this, path(lines + "ShipKind"))
        .set_text(i18n::text(named(ship_type_names, type)));
    place_picture();
}

// The picture fills the scene at range 1 or less and grows smaller to
// farthest_share at range 8, when the option says it shrinks; else it
// fills the scene at every range, as the original draws it.
void ShipCombatView::place_picture()
{
    if (!fight_)
    {
        return;
    }
    const auto *scene = &required_node<Control>(*this, "Margin/Columns/Main/Scene");
    auto &frame = required_node<MarginContainer>(*scene, "ShipFrame");
    double share = 1;
    if (picture_shrinks_)
    {
        const int range = std::clamp(fight_->combat->range(), 1, scanner_range);
        share = 1 - (1 - farthest_share) * (range - 1) / (scanner_range - 1);
    }
    const auto size = scene->get_size();
    const int across = static_cast<int>(size.x * (1 - share) / 2);
    const int down = static_cast<int>(size.y * (1 - share) / 2);
    frame.add_theme_constant_override("margin_left", across);
    frame.add_theme_constant_override("margin_right", across);
    frame.add_theme_constant_override("margin_top", down);
    frame.add_theme_constant_override("margin_bottom", down);
}

// The range, 0 to 8, and over each cell the team's weapons that reach no
// farther (an OpenGoldBox addition).
void ShipCombatView::show_range_track()
{
    const auto &combat = *fight_->combat;
    auto &cells = required_node<HBoxContainer>(*this, "Margin/Columns/Main/RangeTrack/Cells");
    clear_children(cells);
    std::array<std::vector<int>, scanner_range + 1> reach;
    for (const auto &mount : combat.ship(ShipSide::party).weapons)
    {
        const int kind = kind_of(mount);
        if (kind == 0 || mount.maximum == 0)
        {
            continue;
        }
        const auto farthest = static_cast<std::size_t>(std::clamp(
            original::ship_weapon_facts(fight_->game, kind).maximum_range, 0, scanner_range));
        if (std::ranges::find(reach[farthest], kind) == reach[farthest].end())
        {
            reach[farthest].push_back(kind);
        }
    }
    const int range = std::clamp(combat.range(), 0, scanner_range);
    for (int cell = 0; cell <= scanner_range; ++cell)
    {
        auto column = presentation::make_node<VBoxContainer>();
        column->set_name(String("Cell") + String::num_int64(cell));
        column->set_h_size_flags(Control::SIZE_EXPAND_FILL);
        column->add_theme_constant_override("separation", 0);
        String names;
        for (const int kind : reach[static_cast<std::size_t>(cell)])
        {
            names += (names.is_empty() ? "" : ", ") + weapon_name(kind);
        }
        auto weapons = new_label(names, "LabelText11");
        weapons->set_name("Reach");
        weapons->add_theme_color_override("font_color", palette(*this, "party"));
        // TextServer::AUTOWRAP_WORD_SMART, which the trimmed bindings lack.
        weapons->set("autowrap_mode", 3);
        weapons->set_vertical_alignment(VERTICAL_ALIGNMENT_BOTTOM);
        weapons->set_v_size_flags(Control::SIZE_EXPAND_FILL);
        presentation::attach_child(*column, std::move(weapons));
        auto number = new_label(cell == range ? String::utf8("◆ ") + String::num_int64(cell)
                                              : String::num_int64(cell),
                                cell == range ? "LabelGoldText16" : "LabelMutedText14");
        number->set_name("Number");
        presentation::attach_child(*column, std::move(number));
        presentation::attach_child(cells, std::move(column));
    }
}

void ShipCombatView::show_ship(bool enemy_side)
{
    const auto &combat = *fight_->combat;
    const auto side = enemy_side ? ShipSide::enemy : ShipSide::party;
    const auto &ship = combat.ship(side);
    const std::string rows = std::string("Margin/Columns/Main/Ships/") +
                             (enemy_side ? "Enemy" : "Party") + "/Margin/Rows/";
    required_node<Label>(*this, path(rows + "Header/Name"))
        .set_text(enemy_side ? i18n::format(N_("Enemy: {ship}"), {{"ship", side_name(side)}})
                             : i18n::format(N_("Your ship: {ship}"), {{"ship", side_name(side)}}));
    required_node<Label>(*this, path(rows + "Header/Facts"))
        .set_text(i18n::format(
            N_("{tons} tons · speed {speed}"),
            {{"tons", ship.tonnage}, {"speed", original::ship_speed(ship, combat.boosts(side))}}));
    // The enemy's meters light green, the team's light cyan, as the
    // original draws them (charhlp.pas DrawSystemMeter).
    const auto normal = palette(*this, enemy_side ? "character_palette_10" : "party");
    const auto damaged = palette(*this, "score_positive");
    const auto destroyed = palette(*this, "score_negative");
    const auto track = palette(*this, "combat_hp_track");
    // With the team's sensors destroyed, the enemy's systems are not shown
    // (charhlp.pas DrawSystemMeter).
    const bool hidden =
        enemy_side &&
        combat.ship(ShipSide::party).system_status[static_cast<std::size_t>(ShipSystem::sensors)] ==
            0;
    auto &grid = required_node<GridContainer>(*this, path(rows + "Systems"));
    grid.set_visible(!hidden);
    if (enemy_side)
    {
        required_node<Label>(*this, path(rows + "SensorsOut")).set_visible(hidden);
    }
    for (std::size_t system = 0; system < system_count; ++system)
    {
        const int value = std::max(ship.system_status[system], 0);
        const int most = std::max(ship.system_maximum[system], 1);
        const auto color = value == 0 ? destroyed : value < most ? damaged : normal;
        auto *bar =
            Object::cast_to<HBoxContainer>(grid.get_child(static_cast<int>(system * 4 + 2)));
        auto *filled = Object::cast_to<ColorRect>(bar->get_node_or_null("Filled"));
        auto *empty = Object::cast_to<ColorRect>(bar->get_node_or_null("Empty"));
        filled->set_color(color);
        filled->set_visible(value > 0);
        filled->set_stretch_ratio(static_cast<float>(value));
        empty->set_color(track);
        empty->set_visible(value < most);
        empty->set_stretch_ratio(static_cast<float>(std::max(most - value, 0)));
        auto *shown = Object::cast_to<Label>(grid.get_child(static_cast<int>(system * 4 + 3)));
        shown->set_text(String::num_int64(value) + " / " +
                        String::num_int64(ship.system_maximum[system]));
        if (value < most)
        {
            shown->add_theme_color_override("font_color", color);
        }
        else
        {
            shown->remove_theme_color_override("font_color");
        }
        auto *name = Object::cast_to<Label>(grid.get_child(static_cast<int>(system * 4 + 1)));
        if (value == 0)
        {
            name->add_theme_color_override("font_color", destroyed);
        }
        else
        {
            name->remove_theme_color_override("font_color");
        }
    }

    // The weapons: who fires each mount, its working weapons, rounds,
    // reloads and reach; the enemy's rounds and reloads only once the
    // sensors have read it (charhlp.pas VALUE_SENSED).
    auto &weapons = required_node<GridContainer>(*this, path(rows + "Weapons"));
    clear_children(weapons);
    for (const char *heading :
         {N_("Weapons"), N_("By"), N_("Up"), N_("Rounds"), N_("Reloads"), N_("Range")})
    {
        presentation::attach_child(weapons, new_label(i18n::text(heading), "LabelMutedText14"));
    }
    const bool figures = !enemy_side || enemy_figures_shown();
    for (const auto &mount : ship.weapons)
    {
        const int kind = kind_of(mount);
        if (kind == 0 || mount.maximum == 0)
        {
            continue;
        }
        const auto facts = original::ship_weapon_facts(fight_->game, kind);
        const bool lasers = facts.reload_rounds == 0;
        const auto unknown = String("?");
        const std::array<String, 6> cells{
            weapon_name(kind),
            i18n::text((mount.kind & original::ship_crew_mount) != 0 ? N_("Crew") : N_("Pilot")),
            String::num_int64(mount.status) + "/" + String::num_int64(mount.maximum),
            lasers    ? String::utf8("—")
            : figures ? String::num_int64(mount.ammunition)
                      : unknown,
            lasers    ? String::utf8("—")
            : figures ? String::num_int64(mount.reloads)
                      : unknown,
            String::num_int64(facts.maximum_range)};
        const auto color = mount.status == 0              ? destroyed
                           : mount.status < mount.maximum ? damaged
                                                          : Color();
        for (const auto &text : cells)
        {
            auto cell = new_label(text, "LabelText14");
            if (mount.status < mount.maximum)
            {
                cell->add_theme_color_override("font_color", color);
            }
            else if (text == unknown)
            {
                cell->add_theme_color_override("font_color", destroyed);
            }
            presentation::attach_child(weapons, std::move(cell));
        }
    }
}

void ShipCombatView::show_crew()
{
    const auto &combat = *fight_->combat;
    auto &rows = required_node<VBoxContainer>(*this, "Margin/Columns/Crew/Scroll/Rows");
    clear_children(rows);
    const auto crew = combat.crew();
    for (std::size_t member = 0; member < crew.size(); ++member)
    {
        const auto &character = crew[member].character;
        const bool acting = combat.acting_member() == member;
        auto row = presentation::make_node<PanelContainer>();
        row->set_name(String("Member") + String::num_int64(static_cast<std::int64_t>(member)));
        Ref<StyleBoxFlat> style;
        style.instantiate();
        style->set_bg_color(palette(*this, acting ? "combat_row_selected" : "combat_row"));
        style->set_content_margin_all(4);
        if (acting)
        {
            style->set_border_width_all(1);
            style->set_border_color(palette(*this, "gold"));
        }
        row->add_theme_stylebox_override("panel", style);
        auto line = presentation::make_node<HBoxContainer>();
        line->add_theme_constant_override("separation", 8);
        auto icon = presentation::instantiate_control<TextureRect>(
            "res://scenes/control_templates/ship_crew_icon.tscn");
        if (member < fight_->icons.size())
        {
            icon->set_texture(fight_->icons[member]);
        }
        presentation::attach_child(*line, std::move(icon));
        auto text = presentation::make_node<VBoxContainer>();
        text->add_theme_constant_override("separation", 0);
        text->set_h_size_flags(Control::SIZE_EXPAND_FILL);
        String name = String::utf8(character.name.c_str());
        if (combat.pilot() == member)
        {
            name += String::utf8("  ★ ") + i18n::text(N_("Pilot"));
        }
        auto name_label = new_label(name, acting ? "LabelGoldText16" : "LabelText16");
        name_label->set_name("Name");
        name_label->set_clip_text(true);
        presentation::attach_child(*text, std::move(name_label));
        auto kind =
            new_label(career_name(character) + String::utf8(" \u00B7 ") +
                          i18n::text(named(race_names, character.race)) + String::utf8(" \u00B7 ") +
                          i18n::text(named(gender_names, character.gender)),
                      "LabelText13");
        kind->add_theme_color_override("font_color", palette(*this, "score_neutral"));
        kind->set_clip_text(true);
        presentation::attach_child(*text, std::move(kind));
        const auto status = original::status_of(character);
        String health = i18n::format(
            N_("HP {now} / {most}"),
            {{"now", static_cast<int>(static_cast<std::int16_t>(character.hit_points))},
             {"most", static_cast<int>(character.hit_point_maximum)}});
        if (status != original::BuckRogersStatus::okay)
        {
            health += "   " + i18n::text(named(status_names, static_cast<std::size_t>(status)));
        }
        auto health_label = new_label(health, "LabelText14");
        health_label->set_name("Health");
        health_label->add_theme_color_override(
            "font_color",
            palette(*this,
                    status == original::BuckRogersStatus::okay ? "party" : "score_negative"));
        presentation::attach_child(*text, std::move(health_label));
        presentation::attach_child(*line, std::move(text));
        presentation::attach_child(*row, std::move(line));
        presentation::attach_child(rows, std::move(row));
    }
}

void ShipCombatView::show_orders()
{
    const auto &combat = *fight_->combat;
    const auto legal = combat.legal_orders();
    const bool acting = combat.acting_member().has_value() && !finished_;
    const bool pilot = acting_pilot();
    auto &orders = required_node<Control>(*this, "Margin/Columns/Main/Orders");
    orders.set_visible(!finished_);
    for (const auto &order : order_buttons)
    {
        auto &button = required_node<Button>(orders, order.path);
        button.set_visible(pilot ? order.pilot : order.crew);
        const bool offered = order.action == view_action
                                 ? acting
                                 : std::ranges::any_of(legal,
                                                       [&order](const ShipOrder &given)
                                                       {
                                                           return given.action == order.action;
                                                       });
        button.set_disabled(!acting || !offered || choice_ != Choice::none);
    }
    String prompt;
    if (finished_)
    {
        prompt = i18n::text(N_("The ship fight is over."));
    }
    else if (combat.awaits_dodge())
    {
        prompt = i18n::format(N_("The {ship} tries to ram you!"),
                              {{"ship", side_name(ShipSide::enemy)}});
    }
    else if (combat.awaits_round())
    {
        prompt = i18n::text(N_("No one aboard can act this round."));
    }
    else if (const auto member = combat.acting_member())
    {
        prompt = pilot ? i18n::format(N_("{name} is your pilot: what does {name} do?"),
                                      {{"name", member_name(*member)}})
                       : i18n::format(N_("{name}: what does {name} do?"),
                                      {{"name", member_name(*member)}});
    }
    required_node<Label>(*this, "Margin/Columns/Main/Prompt").set_text(prompt);
    required_node<Label>(*this, "Margin/Columns/Main/Help")
        .set_text(
            pilot
                ? i18n::text(N_(
                      "F: fire | T: target | Q: quit | V: view | C: close | W: withdraw | R: ram | B: board"))
                : i18n::text(N_(
                      "F: fire | T: target | L: load | J: jury rig | Q: quit | V: view | C: command | A: aid | B: boost | S: sensors")));
}

// The question below the orders: a choice an order opens, the dodge, the
// round no one can act in, or the end.
void ShipCombatView::show_choice()
{
    const auto &combat = *fight_->combat;
    auto &question = required_node<Label>(*this, "Margin/Columns/Main/Choice/Question");
    auto &answers = required_node<HBoxContainer>(*this, "Margin/Columns/Main/Choice/Answers");
    clear_children(answers);
    offered_.clear();
    std::vector<String> labels;
    String asked;
    const auto legal = combat.legal_orders();
    const auto of_action = [&](ShipAction action)
    {
        for (const auto &order : legal)
        {
            if (order.action == action)
            {
                offered_.push_back(order);
            }
        }
    };
    if (finished_)
    {
        if (!done_)
        {
            labels.push_back(i18n::text(N_("Continue")));
        }
    }
    else if (combat.awaits_dodge())
    {
        asked = i18n::text(N_("Try to dodge the enemy ship?"));
        labels = {i18n::text(N_("Yes")), i18n::text(N_("No"))};
    }
    else if (combat.awaits_round())
    {
        labels.push_back(i18n::text(N_("Next round")));
    }
    else
    {
        switch (choice_)
        {
        case Choice::none:
            break;
        case Choice::target:
            asked = i18n::text(N_("Target which system?"));
            of_action(ShipAction::target);
            for (const auto &order : offered_)
            {
                labels.push_back(system_name(order.system));
            }
            break;
        case Choice::load:
            asked = i18n::text(N_("Load which weapon?"));
            of_action(ShipAction::load);
            for (const auto &order : offered_)
            {
                labels.push_back(weapon_name(kind_of(
                    combat.ship(ShipSide::party).weapons[static_cast<std::size_t>(order.mount)])));
            }
            break;
        case Choice::jury_rig:
            asked = i18n::text(N_("Jury-rig what?"));
            of_action(ShipAction::jury_rig);
            for (const auto &order : offered_)
            {
                labels.push_back(
                    order.mount >= 0
                        ? weapon_name(kind_of(combat.ship(ShipSide::party)
                                                  .weapons[static_cast<std::size_t>(order.mount)]))
                        : system_name(order.system));
            }
            break;
        case Choice::ram:
            asked = i18n::format(N_("Ram the {ship}?"), {{"ship", side_name(ShipSide::enemy)}});
            offered_.push_back({.action = ShipAction::ram});
            labels = {i18n::text(N_("Yes")), i18n::text(N_("No"))};
            break;
        case Choice::view:
            asked = i18n::text(N_("View:"));
            labels = {i18n::text(N_("Ship")), i18n::text(N_("Character"))};
            break;
        }
        if (choice_ != Choice::none && choice_ != Choice::ram)
        {
            labels.push_back(i18n::text(N_("Back")));
        }
    }
    question.set_text(asked);
    for (std::size_t index = 0; index < labels.size(); ++index)
    {
        auto button = presentation::instantiate_control<Button>(
            "res://scenes/control_templates/ship_answer.tscn");
        button->set_name(String("Answer") + String::num_int64(static_cast<std::int64_t>(index)));
        button->set_text(labels[index]);
        // Deferred: answering builds the answers again, this button with them.
        button->connect("pressed",
                        presentation::guarded(this, &ShipCombatView::choice_pressed)
                            .bind(static_cast<std::int64_t>(index)),
                        Object::CONNECT_DEFERRED);
        presentation::attach_child(answers, std::move(button));
    }
}

void ShipCombatView::show_status()
{
    const auto &combat = *fight_->combat;
    const auto &party = combat.ship(ShipSide::party);
    const auto &enemy = combat.ship(ShipSide::enemy);
    required_node<Label>(*this, "Margin/Columns/Main/Status")
        .set_text(i18n::format(
            N_("Range {range} · Your speed {speed}, boosts {boosts} of 2 · Enemy speed {enemy}"),
            {{"range", std::max(combat.range(), 0)},
             {"speed", original::ship_speed(party, combat.boosts(ShipSide::party))},
             {"boosts", combat.boosts(ShipSide::party)},
             {"enemy", original::ship_speed(enemy, combat.boosts(ShipSide::enemy))}}));
}

void ShipCombatView::order_pressed(std::int64_t action_number)
{
    if (!fight_ || finished_)
    {
        return;
    }
    const auto action = static_cast<ShipAction>(action_number);
    switch (action)
    {
    case ShipAction::target:
        choice_ = Choice::target;
        break;
    case ShipAction::load:
        choice_ = Choice::load;
        break;
    case ShipAction::jury_rig:
        choice_ = Choice::jury_rig;
        break;
    case ShipAction::ram:
        choice_ = Choice::ram;
        break;
    default:
        if (action == view_action)
        {
            choice_ = Choice::view;
            break;
        }
        submit({.action = action});
        return;
    }
    refresh();
}

void ShipCombatView::choice_pressed(std::int64_t index)
{
    if (!fight_)
    {
        return;
    }
    auto &combat = *fight_->combat;
    const auto chosen = static_cast<std::size_t>(index);
    if (finished_)
    {
        done_ = true;
        refresh();
        return;
    }
    if (combat.awaits_dodge())
    {
        combat.answer_dodge(chosen == 0);
        take_events();
        refresh();
        return;
    }
    if (combat.awaits_round())
    {
        combat.play_round();
        take_events();
        refresh();
        return;
    }
    const auto choice = std::exchange(choice_, Choice::none);
    if (choice == Choice::view)
    {
        const auto &ship = combat.ship(ShipSide::party);
        if (chosen == 0)
        {
            log_line(
                i18n::format(
                    N_("{ship}: {tons} tons, Armor Class {armor}, crew {crew}, speed {speed}, reaction {reaction}."),
                    {{"ship", side_name(ShipSide::party)},
                     {"tons", ship.tonnage},
                     {"armor", ship.armor_class},
                     {"crew", ship.crew},
                     {"speed", original::ship_speed(ship, combat.boosts(ShipSide::party))},
                     {"reaction", ship.reaction}}),
                palette(*this, "party"));
        }
        else if (chosen == 1 && combat.acting_member())
        {
            const auto &character = combat.crew()[*combat.acting_member()].character;
            log_line(
                i18n::format(
                    N_("{name}: level {level} {career}, HP {now} of {most}; Pilot Rocket {pilot}, Sensor Operation {sensors}, Repair Nuclear Engine {engine}."),
                    {{"name", member_name(*combat.acting_member())},
                     {"level", character.level},
                     {"career", career_name(character)},
                     {"now", static_cast<int>(static_cast<std::int16_t>(character.hit_points))},
                     {"most", static_cast<int>(character.hit_point_maximum)},
                     {"pilot", original::skill_value(character, original::Skill::pilot_rocket)},
                     {"sensors",
                      original::skill_value(character, original::Skill::sensor_operation)},
                     {"engine",
                      original::skill_value(character, original::Skill::repair_nuclear_engine)}}),
                palette(*this, "party"));
        }
        refresh();
        return;
    }
    if (chosen >= offered_.size())
    {
        // Back, or No to the ram.
        refresh();
        return;
    }
    submit(offered_[chosen]);
}

void ShipCombatView::submit(const ShipOrder &order)
{
    fight_->combat->submit(order);
    take_events();
    refresh();
}

void ShipCombatView::take_events()
{
    for (const auto &event : fight_->combat->take_events())
    {
        if (event.kind == ShipEventKind::sensors && event.success)
        {
            sensed_ = true;
        }
        if (event.side == ShipSide::enemy &&
            (event.kind == ShipEventKind::close || event.kind == ShipEventKind::withdraw))
        {
            enemy_retreating_ = event.kind == ShipEventKind::withdraw;
        }
        Color color = palette(*this, "combat_name");
        const auto text = event_text(event, color);
        if (!text.is_empty())
        {
            log_line(text, color);
        }
    }
    if (fight_->combat->over() && !finished_)
    {
        finish_fight();
    }
}

void ShipCombatView::finish_fight()
{
    finished_ = true;
    choice_ = Choice::none;
    const auto end = fight_->combat->end();
    aftermath_ = fight_->finish();
    if (end.boarded)
    {
        log_line(
            i18n::format(N_("The team boards the {ship}."), {{"ship", side_name(ShipSide::enemy)}}),
            palette(*this, "gold"));
    }
    if (!aftermath_)
    {
        return;
    }
    if (aftermath_->team_destroyed)
    {
        log_line(i18n::text(N_("Your team has been destroyed.")), palette(*this, "score_negative"));
        return;
    }
    if (const auto &treatment = aftermath_->treatment)
    {
        const auto crew = aftermath_->end.crew;
        const auto name = [&crew](std::size_t member)
        {
            return member < crew.size() ? String::utf8(crew[member].character.name.c_str())
                                        : String("?");
        };
        for (const auto &line : treatment->lines)
        {
            using Kind = original::TreatmentLine::Kind;
            switch (line.kind)
            {
            case Kind::wounds_healed:
                log_line(i18n::format(N_("{helper} heals {patient} for {points}."),
                                      {{"helper", name(line.helper)},
                                       {"patient", name(line.patient)},
                                       {"points", line.hit_points}}),
                         palette(*this, "party"));
                break;
            case Kind::poison_cured:
            case Kind::antidote_used:
                log_line(
                    i18n::format(N_("{helper} cures {patient}'s poison."),
                                 {{"helper", name(line.helper)}, {"patient", name(line.patient)}}),
                    palette(*this, "party"));
                break;
            case Kind::coma:
                log_line(i18n::format(N_("{patient} falls into a coma."),
                                      {{"patient", name(line.patient)}}),
                         palette(*this, "score_negative"));
                break;
            }
        }
    }
    if (const auto &repairs = aftermath_->repairs)
    {
        const auto crew = aftermath_->end.crew;
        if (repairs->experience_share > 0)
        {
            log_line(i18n::format(N_("Each member gains {points} experience points."),
                                  {{"points", repairs->experience_share}}),
                     palette(*this, "gold"));
        }
        for (const auto &repair : repairs->repairs)
        {
            const auto who = repair.member < crew.size()
                                 ? String::utf8(crew[repair.member].character.name.c_str())
                                 : String("?");
            if (repair.system == ShipSystem::weapons)
            {
                const auto &mount =
                    aftermath_->end.party_ship.weapons[static_cast<std::size_t>(repair.mount)];
                log_line(i18n::format(N_("{member} repairs a {weapon}."),
                                      {{"member", who}, {"weapon", weapon_name(kind_of(mount))}}),
                         palette(*this, "party"));
                continue;
            }
            log_line(i18n::format(N_("{member} repairs the {system}: {points} points."),
                                  {{"member", who},
                                   {"system", system_name(repair.system)},
                                   {"points", repair.amount}}),
                     palette(*this, "party"));
        }
        if (repairs->fuel_taken > 0)
        {
            log_line(
                i18n::format(N_("The tanks take {fuel} fuel from the {ship}."),
                             {{"fuel", repairs->fuel_taken}, {"ship", side_name(ShipSide::enemy)}}),
                palette(*this, "party"));
        }
    }
}

void ShipCombatView::log_line(const String &text, const Color &color)
{
    auto &log = required_node<RichTextLabel>(*this, "Margin/Columns/Main/Log");
    if (!log.get_parsed_text().is_empty())
    {
        log.newline();
    }
    log.push_color(color);
    log.add_text(text);
    log.pop();
}

String ShipCombatView::event_text(const original::ShipEvent &event, Color &color) const
{
    const auto damage = palette(*this, "score_positive");
    const auto harm = palette(*this, "score_negative");
    const auto who = [&]
    {
        return event.member ? member_name(*event.member) : side_name(event.side);
    };
    switch (event.kind)
    {
    case ShipEventKind::pilot_contest:
        return i18n::text(
            event.success ? N_("The pilots size each other up: the enemy moves first each round.")
                          : N_("The pilots size each other up: the team moves first each round."));
    case ShipEventKind::shot:
        if (!event.success)
        {
            return i18n::format(N_("{who} fires the {weapon}: a miss."),
                                {{"who", who()}, {"weapon", weapon_name(event.weapon_kind)}});
        }
        return i18n::format(
            N_("{who} fires the {weapon}: a hit for {damage}."),
            {{"who", who()}, {"weapon", weapon_name(event.weapon_kind)}, {"damage", event.amount}});
    case ShipEventKind::system_damaged:
        color = damage;
        return i18n::format(N_("{ship}: {system} takes {damage}."),
                            {{"ship", side_name(event.side)},
                             {"system", system_name(event.system)},
                             {"damage", event.amount}});
    case ShipEventKind::system_destroyed:
        color = harm;
        return i18n::format(
            N_("{ship}: {system} destroyed!"),
            {{"ship", side_name(event.side)}, {"system", system_name(event.system)}});
    case ShipEventKind::weapon_destroyed:
        color = harm;
        return i18n::format(
            N_("{ship}: {weapon} hit, one destroyed!"),
            {{"ship", side_name(event.side)}, {"weapon", weapon_name(event.weapon_kind)}});
    case ShipEventKind::ship_crew_lost:
        color = damage;
        return i18n::format(N_("{ship} loses a crew member."), {{"ship", side_name(event.side)}});
    case ShipEventKind::member_hurt:
        color = harm;
        return i18n::format(N_("{who} takes {damage} damage."),
                            {{"who", who()}, {"damage", event.amount}});
    case ShipEventKind::member_leaps_clear:
        return i18n::format(N_("{who} leaps clear of an explosion."), {{"who", who()}});
    case ShipEventKind::member_killed:
        color = harm;
        return i18n::format(N_("{who} dies as the ship explodes!"), {{"who", who()}});
    case ShipEventKind::member_comatose:
        color = harm;
        return i18n::format(N_("{who} falls into a coma."), {{"who", who()}});
    case ShipEventKind::boost:
        return event.success
                   ? i18n::format(N_("{who} boosts the engines."), {{"who", who()}})
                   : i18n::format(N_("{who} fails to boost the engines."), {{"who", who()}});
    case ShipEventKind::jury_rig:
    {
        const auto what =
            event.system == ShipSystem::weapons && event.mount >= 0
                ? weapon_name(kind_of(fight_->combat->ship(event.side)
                                          .weapons[static_cast<std::size_t>(event.mount)]))
                : system_name(event.system);
        return event.success ? i18n::format(N_("{who} jury-rigs the {what}."),
                                            {{"who", who()}, {"what", what}})
                             : i18n::format(N_("{who} fails to jury-rig the {what}."),
                                            {{"who", who()}, {"what", what}});
    }
    case ShipEventKind::reload:
        return i18n::format(N_("{who} reloads the {weapon}."),
                            {{"who", who()}, {"weapon", weapon_name(event.weapon_kind)}});
    case ShipEventKind::close:
        return i18n::format(N_("{ship} closes by {distance}."),
                            {{"ship", side_name(event.side)}, {"distance", event.amount}});
    case ShipEventKind::withdraw:
        return i18n::format(N_("{ship} withdraws by {distance}."),
                            {{"ship", side_name(event.side)}, {"distance", event.amount}});
    case ShipEventKind::ram_attempt:
        color = damage;
        return i18n::format(N_("{ship} tries to ram!"), {{"ship", side_name(event.side)}});
    case ShipEventKind::ram_dodged:
        return i18n::format(N_("{ship} dodges the ram."), {{"ship", side_name(event.side)}});
    case ShipEventKind::rammed:
        color = harm;
        return i18n::format(N_("{ship} rams home!"), {{"ship", side_name(event.side)}});
    case ShipEventKind::board:
        color = palette(*this, "gold");
        return i18n::text(N_("You close with the crippled ship and attempt to board her."));
    case ShipEventKind::command:
        return i18n::format(N_("{who} takes the helm."), {{"who", who()}});
    case ShipEventKind::bandage:
        return i18n::format(N_("{who} is bandaged."), {{"who", who()}});
    case ShipEventKind::treat_stun:
        return event.success ? i18n::format(N_("{who} is no longer stunned."), {{"who", who()}})
                             : i18n::format(N_("{who} is still stunned."), {{"who", who()}});
    case ShipEventKind::sensors:
        return event.success
                   ? i18n::format(N_("{who} reads the enemy on the sensors."), {{"who", who()}})
                   : i18n::format(N_("{who} fails at sensor operation."), {{"who", who()}});
    case ShipEventKind::lost_from_scanners:
        return i18n::format(N_("The {ship} is lost from the scanners."),
                            {{"ship", side_name(ShipSide::enemy)}});
    case ShipEventKind::breaks_off:
        return i18n::format(N_("The {ship} breaks off."), {{"ship", side_name(ShipSide::enemy)}});
    }
    return {};
}
