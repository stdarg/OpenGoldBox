#include "godot_text.h"
#include "class_text.h"
#include "inventory_label.h"
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/check_box.hpp>
#include "application_settings.h"
#include "godot_images.h"
#include "godot_nodes.h"
#include "localization.h"
#include "game_resources.h"
#include "game_combat_looks.h"
#include "character_creation_view.h"
#include "character_pool_dialog.h"
#include "town_sheet_dialog.h"
#include "character_sheet_text.h"
#include "equipment_choice_dialog.h"
#include "combat_view.h"
#include "campaign_combat.h"
#include "rolf_tour_view.h"
#include "save_slots.h"
#include "vital_fixtures.h"
#include "opengold/campaign_save.h"
#include "opengold/combat_body_catalog.h"
#include "opengold/srd5.h"
#include "godot_path.h"
#include "guarded_handlers.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/classes/viewport_texture.hpp>
#include <godot_cpp/classes/item_list.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <algorithm>
#include <stdexcept>
#include <set>
#include <map>

using namespace godot;
using namespace opengold;
using presentation::required_node;

namespace
{
using presentation::gs;

Character check_fighter()
{
    auto rules = srd5::character_rules();
    rules::CharacterDraft draft;
    draft.race = "human";
    draft.gender = "male";
    draft.character_class = "fighter";
    draft.alignment = "lawful_good";
    draft.background = "soldier";
    draft.name = "Check fighter";
    draft.rolled = true;
    for (auto &roll : draft.rolls)
    {
        roll = {{6, 5, 4, 1}, 3};
    }
    return Character(*rules, std::move(draft), {});
}

} // namespace

void CharacterCreationView::setup_party()
{
    body_catalog_ = game_combat_looks();
    const auto pack = presentation::path_from_godot(game_rules_file());
    campaign_ = std::make_shared<CampaignParty>(srd5::load(pack));
    auto panel = presentation::instantiate_scene("res://scenes/party_panel.tscn");
    i18n::prepare_ui(*panel);
    presentation::attach_child(*this, std::move(panel));
    required_node<Control>(*this, "PartyPanel").hide();
    connect_party_button("Party", PartyAction::open);
    connect_party_button("AddParty", PartyAction::add_created);
    connect_party_button("ReturnParty", PartyAction::return_to_party);
    connect_party_button("PartyPanel/ActionRow1/Create", PartyAction::create);
    connect_party_button("PartyPanel/ActionRow1/Remove", PartyAction::remove);
    connect_party_button("PartyPanel/ActionRow1/Rejoin", PartyAction::rejoin);
    connect_party_button("PartyPanel/ActionRow1/Equip", PartyAction::equip);
    connect_party_button("PartyPanel/ActionRow2/Unequip", PartyAction::unequip);
    connect_party_button("PartyPanel/ActionRow2/Explore", PartyAction::explore);
    connect_party_button("PartyPanel/ActionRow2/Close", PartyAction::close);
    auto *roster = &required_node<ItemList>(*this, "PartyPanel/Roster");
    roster->connect("item_selected",
                    presentation::guarded(this, &CharacterCreationView::party_selected));
    roster->set_drag_forwarding(
        presentation::guarded(this, &CharacterCreationView::drag_member),
        presentation::guarded(this, &CharacterCreationView::can_drop_member),
        presentation::guarded(this, &CharacterCreationView::drop_member));
    roster->set_tooltip_text(i18n::text(N_(
        "Drag a character up or down to change the party's order. A fight puts the first half of the party in the front rank.")));
    required_node<Button>(*this, "PartyPanel/MakeLeader")
        .connect("pressed",
                 presentation::guarded(this, &CharacterCreationView::make_roster_leader));
    required_node<TownSheetDialog>(*this, "TownSheet")
        .connect_host(
            [this]() -> CampaignParty &
            {
                return *campaign_;
            },
            [this]
            {
                show_leader_change();
            },
            [this](const std::exception &failure)
            {
                report_failure(failure);
            });
    required_node<Button>(*this, "PartyPanel/Pool")
        .connect("pressed", presentation::guarded(this, &CharacterCreationView::show_pool));
    required_node<CharacterPoolDialog>(*this, "PoolModal")
        .connect_host(
            srd5::character_rules(), *art_, *portraits_,
            [this]() -> CampaignParty &
            {
                return *campaign_;
            },
            [this]
            {
                roster_index_ = campaign_->state().roster.size() - 1;
                refresh_party();
            },
            [this](const std::exception &failure)
            {
                report_failure(failure);
            });
    required_node<Button>(*this, "PartyPanel/ActionRow2/Modifiers")
        .connect("pressed", presentation::guarded(this, &CharacterCreationView::show_modifiers));
    required_node<Button>(*this, "PartyPanel/ActionRow2/SavingThrows")
        .connect("pressed",
                 presentation::guarded(this, &CharacterCreationView::show_saving_throws));
    setup_saves();
    setup_defeat();
    setup_advancement();
    party_check_ = OS::get_singleton()->get_cmdline_user_args().has("--party-check");
    party_layout();
    equipment_art_check_ =
        OS::get_singleton()->get_cmdline_user_args().has("--equipment-art-check");
    expedition_check_ = OS::get_singleton()->get_cmdline_user_args().has("--expedition-check");
    reincarnate_check_ = OS::get_singleton()->get_cmdline_user_args().has("--reincarnate-check");
}

// Reincarnate from camp (LEVELS-5): a Druid's spell gives a dead companion a
// new body through the creation pages, captured page by page.
void CharacterCreationView::reincarnate_check()
{
    if (++reincarnate_frames_ > 4000)
    {
        throw std::runtime_error("Reincarnation check timed out");
    }
    // Room for each page to draw before it is captured.
    if (reincarnate_frames_ % 30)
    {
        return;
    }
    const auto check = [](bool ok, const char *message)
    {
        if (!ok)
        {
            throw std::runtime_error(message);
        }
    };
    const auto press = [&](const char *path)
    {
        auto *button = &required_node<Button>(*this, path);
        check(!button->is_disabled(), path);
        button->emit_signal("pressed");
        check(error_.is_empty(), error_.utf8().get_data());
    };
    const auto &slots = campaign_->state().slots;
    const auto reincarnate_save = []
    {
        const auto path = presentation::path_from_godot(
            ProjectSettings::get_singleton()->globalize_path("user://checks/reincarnate.ogs"));
        std::filesystem::create_directories(path.parent_path());
        return path;
    };
    switch (reincarnate_stage_++)
    {
    case 0:
    {
        rules::CharacterDraft druid;
        druid.race = "human";
        druid.gender = "female";
        druid.character_class = "druid";
        druid.background = "sage";
        druid.alignment = "neutral_good";
        druid.name = "Druid";
        druid.rolled = true;
        for (auto &roll : druid.rolls)
        {
            roll = {{6, 5, 4, 1}, 3};
        }
        druid.training = {{"class:druid", {"nature", "medicine"}},
                          {"class:druid:primal_order", {"warden"}}};
        const auto caster = campaign_->add_pc(Character(*srd5::character_rules(), druid, {}));
        campaign_->award_experience(48000, "fixture:reincarnate-druid");
        while (campaign_->can_advance(caster))
        {
            auto choice = campaign_->default_advancement(caster);
            if (campaign_->member(caster).character.sheet().level == 8 &&
                std::find(choice.spells.begin(), choice.spells.end(), "reincarnate") ==
                    choice.spells.end())
            {
                choice.spells.back() = "reincarnate";
            }
            campaign_->advance(caster, choice);
        }
        auto guard = check_fighter();
        guard.rename("Fallen companion");
        const auto fallen = campaign_->add_pc(std::move(guard));
        campaign_->add_coins(Coin::gold, 5000);
        auto state = campaign_->checkpoint();
        for (auto &m : state.roster)
        {
            if (m.id == fallen)
            {
                m.vitals = {
                    0, true,
                    presentation::srd_vitals(
                        m.character,
                        {.failures = 3,
                         .hit_dice = 1,
                         .effects = "FX9 2 1 1 113 1 1 \"Fallen companion\" 0 864000000 0 0 0"}),
                    "Dead"};
            }
        }
        campaign_->restore(std::move(state));
        party_action(PartyAction::explore);
        break;
    }
    case 1:
        // A character being made waits through the spell.
        creator_->name("Waiting character");
        open_reincarnation(slots[0], slots[1], "reincarnate");
        check(creator_->reincarnating() && creator_->step() == CreationStep::race &&
                  !required_node<RolfTourView>(*this, "CampaignTown").is_visible(),
              "Reincarnate opens the race page");
        check(!required_node<LineEdit>(*this, "Name").is_editable(), "The name stays");
        break;
    case 2:
        capture("reincarnate-race.png");
        press("Next");
        break;
    case 3:
        if (creator_->step() == CreationStep::spell_choices)
        {
            capture("reincarnate-spells.png");
            for (const auto &group : creator_->spell_choice_options().learning)
            {
                for (unsigned n = 0; n < group.count; ++n)
                {
                    creator_->spell_choice(
                        {.group = group.id, .option = group.options[n].id, .selected = true});
                }
            }
            press("Next");
        }
        check(creator_->step() == CreationStep::combat_icon, "The combat icon follows");
        refresh();
        break;
    case 4:
        capture("reincarnate-icon.png");
        press("Next");
        check(creator_->step() == CreationStep::portrait, "The portrait follows");
        break;
    case 5:
        capture("reincarnate-portrait.png");
        press("Next");
        check(creator_->step() == CreationStep::sheet &&
                  required_node<Button>(*this, "AddParty").is_visible(),
              "The sheet offers Reincarnate");
        break;
    case 6:
    {
        capture("reincarnate-sheet.png");
        const auto species = campaign_->reincarnation_species();
        press("AddParty");
        const auto &body = campaign_->member(slots[1]);
        check(!body.vitals.dead && body.character.creation_data().race == species &&
                  !reincarnation_ &&
                  required_node<RolfTourView>(*this, "CampaignTown").is_visible(),
              "The companion returns in its new body, back in town");
        check(creator_->draft().name == "Waiting character" &&
                  required_node<LineEdit>(*this, "Name").get_text() == "Waiting character",
              "The character being made before the spell is back");
        break;
    }
    case 7:
        capture("reincarnate-town.png");
        // The party alone: the check's town is not idle enough to save.
        write_campaign_file(reincarnate_save(),
                            encode_campaign(*campaign_, nullptr,
                                            campaign_asset_identity(presentation::path_from_godot(
                                                settings::game_path()))));
        // A Human body whose Versatile feat is Magic Initiate, for its spell page.
        begin_reincarnation({slots[0], slots[1], "reincarnate"}, "human");
        creator_->training_choice({.group = "species:human:versatile",
                                   .option = "magic_initiate_cleric",
                                   .selected = true});
        refresh();
        break;
    case 8:
        capture("reincarnate-human.png");
        press("Next");
        check(creator_->step() == CreationStep::spell_choices, "Magic Initiate's spells come next");
        break;
    case 9:
    {
        capture("reincarnate-human-spells.png");
        // Loading a campaign with the pages open ends them before the old
        // campaign, whose rules the creator borrows, goes (#239).
        // The check's town is mid-tour; after a defeat a game loads anywhere.
        campaign_defeated_ = true;
        load_campaign(reincarnate_save());
        check(!reincarnation_ && !creator_->reincarnating(),
              "Loading ends the reincarnation pages");
        break;
    }
    default:
    {
        press("Restart");
        check(!reincarnation_ && !creator_->reincarnating(), "Cancel leaves the pages");
        // The pool, first opened after both, is built from rules of its own (#236).
        auto *pool = &required_node<CharacterPoolDialog>(*this, "PoolModal");
        show_pool();
        check(pool->characters().size() == 48, "The pool opens after a reincarnation");
        const auto members = campaign_->state().roster.size();
        pool->select(0);
        pool->add();
        check(campaign_->state().roster.size() == members + 1, "A pool character joins");
        pool->close();
        UtilityFunctions::print(
            "Godot reincarnation passed: race page, spells, combat icon, portrait, sheet, new body.");
        reincarnate_check_ = false;
        get_tree()->quit(0);
    }
    }
}

void CharacterCreationView::show_pool()
{
    try
    {
        required_node<CharacterPoolDialog>(*this, "PoolModal").open();
    }
    catch (const std::exception &e)
    {
        required_node<Label>(*this, "PartyPanel/Status").set_text(i18n::text(e.what()));
    }
}

void CharacterCreationView::party_layout()
{
    required_node<CharacterPoolDialog>(*this, "PoolModal").fit(get_size());
    required_node<TownSheetDialog>(*this, "TownSheet").fit(get_size());
}

void CharacterCreationView::party_selected(std::int64_t index)
{
    const auto rows = roster_rows();
    if (index < 0 || static_cast<std::size_t>(index) >= rows.size())
    {
        return;
    }
    roster_index_ = rows[index];
    const auto id = campaign_->state().roster[roster_index_].id;
    for (unsigned slot = 0; slot < 8; ++slot)
    {
        if (campaign_->state().slots[slot] == id)
        {
            campaign_->select(PartySlot{slot});
        }
    }
    refresh_party();
}

std::vector<std::size_t> CharacterCreationView::roster_rows() const
{
    const auto &state = campaign_->state();
    std::vector<std::size_t> rows;
    for (const auto id : state.slots)
    {
        for (std::size_t i = 0; id && i < state.roster.size(); ++i)
        {
            if (state.roster[i].id == id)
            {
                rows.push_back(i);
            }
        }
    }
    for (std::size_t i = 0; i < state.roster.size(); ++i)
    {
        if (std::find(state.slots.begin(), state.slots.end(), state.roster[i].id) ==
            state.slots.end())
        {
            rows.push_back(i);
        }
    }
    return rows;
}

// Only a player character in the party moves; an NPC keeps its position.
static bool in_party_order(const CampaignParty &campaign, const PartyMember &member)
{
    const auto &slots = campaign.state().slots;
    return member.npc_source.empty() &&
           std::find(slots.begin(), slots.end(), member.id) != slots.end();
}

std::optional<std::size_t> CharacterCreationView::drop_place(Vector2 position) const
{
    // A named path, not a temporary: GCC 16 takes the node reference for one
    // into the temporary NodePath.
    const NodePath roster_path{"PartyPanel/Roster"};
    const auto &list = required_node<ItemList>(*this, roster_path);
    const auto row = list.get_item_at_position(position);
    const auto rows = roster_rows();
    if (row < 0 || static_cast<std::size_t>(row) >= rows.size() ||
        !in_party_order(*campaign_, campaign_->state().roster[rows[row]]))
    {
        return std::nullopt;
    }
    std::size_t place = 0;
    for (int earlier = 0; earlier < row; ++earlier)
    {
        place += in_party_order(*campaign_, campaign_->state().roster[rows[earlier]]) ? 1 : 0;
    }
    return place;
}

Variant CharacterCreationView::drag_member(Vector2 position)
{
    auto *list = &required_node<ItemList>(*this, "PartyPanel/Roster");
    // The press that starts a drag selects its row, which rebuilds the list.
    list->force_update_list_size();
    const auto row = list->get_item_at_position(position, true);
    const auto rows = roster_rows();
    if (!party_open_ || campaign_->in_combat() || row < 0 ||
        static_cast<std::size_t>(row) >= rows.size())
    {
        return {};
    }
    const auto &member = campaign_->state().roster[rows[row]];
    const auto place = drop_place(position);
    if (!place || !in_party_order(*campaign_, member))
    {
        return {};
    }
    auto preview = presentation::make_node<TextureRect>();
    preview->set_texture(list->get_item_icon(row));
    preview->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
    preview->set_size(list->get_fixed_icon_size());
    preview->set_modulate(Color(1, 1, 1, 0.8f));
    list->set_drag_preview(preview.get());
    // Godot adopts the preview only while a drag can show it; one it refused
    // stays with NodeOwner and is freed (Effective C++ Item 13).
    if (preview->get_parent())
    {
        (void)preview.release();
    }
    Dictionary data;
    data["opengold_party_member"] = static_cast<std::int64_t>(member.id);
    data["opengold_party_place"] = static_cast<std::int64_t>(*place);
    return data;
}

// A dragged roster member's payload: who moves and the place it leaves.
static bool is_party_member_drag(const Variant &data)
{
    if (data.get_type() != Variant::DICTIONARY)
    {
        return false;
    }
    const Dictionary payload = data;
    return payload.has("opengold_party_member") && payload.has("opengold_party_place");
}

bool CharacterCreationView::can_drop_member(Vector2 position, const Variant &data)
{
    auto *list = &required_node<ItemList>(*this, "PartyPanel/Roster");
    const auto place = drop_place(position);
    if (!party_open_ || !place || !is_party_member_drag(data))
    {
        list->call("show_drop", -1, false);
        return false;
    }
    const Dictionary payload = data;
    const std::int64_t from = payload["opengold_party_place"];
    const auto row = list->get_item_at_position(position);
    // The line shows where the character will stand: above the row it
    // pushes down, below the row it pushes up.
    list->call("show_drop", std::cmp_equal(*place, from) ? -1 : row,
               std::cmp_greater(*place, from));
    return true;
}

void CharacterCreationView::drop_member(Vector2 position, const Variant &data)
{
    required_node<ItemList>(*this, "PartyPanel/Roster").call("show_drop", -1, false);
    const auto place = drop_place(position);
    if (!party_open_ || !place || !is_party_member_drag(data))
    {
        return;
    }
    const Dictionary payload = data;
    const auto id =
        static_cast<MemberId>(static_cast<std::int64_t>(payload["opengold_party_member"]));
    try
    {
        campaign_->move_member(id, *place);
        error_ = "";
    }
    catch (const std::exception &e)
    {
        error_ = i18n::text(e.what());
    }
    for (std::size_t i = 0; i < campaign_->state().roster.size(); ++i)
    {
        if (campaign_->state().roster[i].id == id)
        {
            roster_index_ = i;
        }
    }
    // The first member leads when no leader was chosen, so the lead may move.
    show_leader_change();
    refresh_advancement_arrows();
}

void CharacterCreationView::refresh_party()
{
    auto *list = &required_node<ItemList>(*this, "PartyPanel/Roster");
    list->clear();
    const auto &state = campaign_->state();
    const auto rows = roster_rows();
    Array members;
    for (const auto index : rows)
    {
        const auto &m = state.roster[index];
        auto slot = std::find(state.slots.begin(), state.slots.end(), m.id);
        const bool leader = slot != state.slots.end() && m.id == campaign_->leader();
        const auto &sheet = m.character.sheet();
        // The roster has room for the six starting members' photos and identity.
        const auto title = (leader ? String::utf8("★ ") : String()) + gs(sheet.name) +
                           (slot == state.slots.end() ? i18n::text(" (Reserve)") : String());
        list->add_item(title,
                       portraits_->texture(m.character.appearance(), m.character.creation_data()));
        members.push_back(PackedStringArray({title, i18n::class_summary(sheet),
                                             i18n::text(sheet.race), i18n::text(sheet.gender)}));
        if (leader)
        {
            list->call("mark_leader", list->get_item_count() - 1);
        }
    }
    list->call("show_members", members);
    auto *items = &required_node<ItemList>(*this, "PartyPanel/Inventory");
    items->clear();
    std::string sheet = i18n::utf8(
        "Create a character, finish its sheet, then Add to party.\n\nA party holds six characters, and up to two others may join it on the way. Removed members remain in the roster.");
    if (!state.roster.empty())
    {
        roster_index_ = std::min(roster_index_, state.roster.size() - 1);
        list->select(static_cast<std::int32_t>(std::find(rows.begin(), rows.end(), roster_index_) -
                                               rows.begin()));
        const auto &m = state.roster[roster_index_];
        sheet = presentation::sheet_text(*campaign_, m).utf8().get_data();
        const auto profile = campaign_->profile(m.id);
        for (const auto &item : m.character.inventory().items())
        {
            const auto found = std::find(m.equipped.begin(), m.equipped.end(), item.id);
            String prefix;
            if (found != m.equipped.end())
            {
                prefix =
                    i18n::text(profile.equipment_positions.at(found - m.equipped.begin()).source) +
                    " / ";
            }
            items->add_item(prefix + inventory_label(m, item));
        }
        required_node<TextureRect>(*this, "PartyPanel/Portrait")
            .set_texture(
                portraits_->texture(m.character.appearance(), m.character.creation_data()));
        const auto icons = por::party_combat_icons(m, *art_, looks_or_none(body_catalog_));
        required_node<TextureRect>(*this, "PartyPanel/ReadySprite")
            .set_texture(presentation::image_texture(icons.ready));
        required_node<TextureRect>(*this, "PartyPanel/ActionSprite")
            .set_texture(presentation::image_texture(icons.action));
    }
    if (state.roster.empty())
    {
        for (const char *name :
             {"PartyPanel/Portrait", "PartyPanel/ReadySprite", "PartyPanel/ActionSprite"})
        {
            required_node<TextureRect>(*this, name).set_texture({});
        }
    }
    // The poses' captions name nothing until there is someone to show.
    for (const char *name : {"PartyPanel/ReadyLabel", "PartyPanel/ActionLabel"})
    {
        required_node<Label>(*this, name).set_visible(!state.roster.empty());
    }
    required_node<RichTextLabel>(*this, "PartyPanel/Sheet").call("render", gs(sheet));
    required_node<Label>(*this, "PartyPanel/Status")
        .set_text(
            error_.is_empty()
                ? i18n::text(
                      "Each new character brings 3d6 × 10 gold pieces. Save game keeps this party to play on later.")
                : error_);
    for (const char *name : {"Remove", "Rejoin", "Equip"})
    {
        required_node<Button>(*this, gs(std::string("PartyPanel/ActionRow1/") + name))
            .set_disabled(state.roster.empty());
    }
    for (const char *name : {"Unequip", "Explore", "Modifiers", "SavingThrows"})
    {
        required_node<Button>(*this, gs(std::string("PartyPanel/ActionRow2/") + name))
            .set_disabled(state.roster.empty());
    }
    // Only an active member who is not already the leader can be made leader.
    const auto chosen = state.roster.empty() ? MemberId{} : state.roster[roster_index_].id;
    required_node<Button>(*this, "PartyPanel/MakeLeader")
        .set_disabled(!chosen || chosen == campaign_->leader() ||
                      std::find(state.slots.begin(), state.slots.end(), chosen) ==
                          state.slots.end());
}

void CharacterCreationView::make_roster_leader()
{
    const auto &roster = campaign_->state().roster;
    if (roster_index_ >= roster.size())
    {
        return;
    }
    campaign_->make_leader(roster[roster_index_].id);
    show_leader_change();
}

// The roster and the exploration party list both mark the leader.
void CharacterCreationView::show_leader_change()
{
    refresh_party();
    if (auto *town = Object::cast_to<RolfTourView>(get_node_or_null("CampaignTown")))
    {
        town->refresh();
    }
}

// Original-data integration check, also runnable in the packaged executable.
// Equipment actions use the real controls and inspect uploaded texture pixels.
void CharacterCreationView::equipment_art_check()
{
    const auto require = [](bool ok, const char *message)
    {
        if (!ok)
        {
            throw std::runtime_error(message);
        }
    };
    const auto press = [&](const char *path)
    {
        required_node<Button>(*this, path).emit_signal("pressed");
        if (!error_.is_empty())
        {
            throw std::runtime_error(error_.utf8().get_data());
        }
    };
    const auto gear = [&](unsigned index, bool equip)
    {
        required_node<ItemList>(*this, "PartyPanel/Inventory").select(index);
        press(equip ? "PartyPanel/ActionRow1/Equip" : "PartyPanel/ActionRow2/Unequip");
        if (auto *choice = Object::cast_to<Window>(get_node_or_null("EquipmentChoice"));
            choice && choice->is_visible())
        {
            auto *hand = &required_node<OptionButton>(*choice, "Hand");
            hand->select(0);
            hand->emit_signal("item_selected", 0);
            press("EquipmentChoice/Equip");
        }
    };
    const auto expected = [&](unsigned member, unsigned body, bool action)
    {
        const auto a = campaign_->state().roster.at(member).character.appearance();
        return presentation::rgba_image(
                   art_->equipped_icon(a, body,
                                       action ? por::IconPose::action : por::IconPose::ready))
            ->get_data();
    };
    const auto verify_preview = [&](unsigned member, unsigned body)
    {
        for (bool action : {false, true})
        {
            const auto texture =
                required_node<TextureRect>(*this, action ? "PartyPanel/ActionSprite"
                                                         : "PartyPanel/ReadySprite")
                    .get_texture();
            require(texture.is_valid() &&
                        texture->get_image()->get_data() == expected(member, body, action),
                    "Party preview differs from equipped body in ready/action pose");
        }
        require(campaign_->state().roster.at(member).character.appearance().combat_body == 24,
                "Equipment display overwrote the saved base body");
    };
    const auto verify_combat = [&]
    {
        auto *combat = &required_node<CombatView>(*this, "CampaignCombat");
        for (unsigned member = 0; member < 2; ++member)
        {
            for (bool action : {false, true})
            {
                const auto texture =
                    combat->sprite_texture(campaign_->state().roster.at(member).id,
                                           action ? por::IconPose::action : por::IconPose::ready);
                require(texture.is_valid() &&
                            texture->get_image()->get_data() == expected(member, 34, action),
                        "Combat textures differ from the equipment shown in party previews");
            }
        }
    };
    if (check_stage_ == 0)
    {
        for (unsigned member = 0; member < 2; ++member)
        {
            const auto guard = check_fighter();
            auto draft = guard.creation_data();
            draft.name = member ? "Equipment check NPC" : "Equipment check PC";
            auto a = guard.appearance();
            a.combat_body = 24;
            a.icon_size = member == 0 ? por::IconSize::regular : por::IconSize::small;
            a.portrait = portraits_->recommended(draft);
            Character character(*srd5::character_rules(), draft, a);
            const auto id = member
                                ? campaign_->recruit("check:equipment-guard", std::move(character))
                                : campaign_->add_pc(std::move(character));
            for (const unsigned type : {36u, 8u, 59u, 55u})
            {
                por::Equipment item;
                item.stored.type = static_cast<uint8_t>(type);
                item.stored.stack_size = 1;
                campaign_->purchase(id, item);
            }
        }
        press("Party");
        party_selected(0);
        verify_preview(0, 0);
    }
    else if (check_stage_ <= 16)
    {
        const unsigned member = (check_stage_ - 1) / 8, step = (check_stage_ - 1) % 8;
        const std::array<por::CombatEquipment, 1> sword{{{36, "Long Sword", "longsword"}}};
        const std::array<unsigned, 8> bodies{
            0, body_catalog_.value().choose(sword, 24).body, 24, 32, 34, 33, 0, 34};
        verify_preview(member, bodies[step]);
        if (step < 5)
        {
            const auto name = "equipment-art-" + std::string(member ? "npc" : "pc") + "-" +
                              std::to_string(step) + ".png";
            capture(name.c_str());
        }
        switch (step)
        {
        case 0:
            gear(0, true);
            break;
        case 1:
        {
            const auto id = campaign_->state().roster.at(member).id;
            require(campaign_->profile(id).weapon_hands == 2,
                    "A longsword with an empty other hand is wielded two-handed");
            gear(2, true);
            require(campaign_->profile(id).weapon_hands == 1,
                    "Equipping a shield makes the longsword one-handed");
            break;
        }
        case 2:
            gear(0, false);
            break;
        case 3:
            gear(1, true);
            break;
        case 4:
            gear(2, false);
            break;
        case 5:
            gear(1, false);
            break;
        case 6:
            gear(3, true);
            verify_preview(member, 0);
            gear(3, false);
            gear(1, true);
            gear(2, true);
            break;
        case 7:
            if (member == 0)
            {
                party_selected(1);
                verify_preview(1, 0);
            }
            break;
        }
        if (step < 7)
        {
            verify_preview(member,
                           bodies[step + 1]); // Refresh is synchronous with each control action.
        }
    }
    else if (check_stage_ == 17)
    {
        party_selected(0);
        verify_preview(0, 34);

        // Removes the check's save file however the check ends; one owner
        // only, so copying is forbidden (Effective C++ Items 6 and 14).
        struct CheckSave
        {
            std::filesystem::path path;

            explicit CheckSave(std::filesystem::path file) : path(std::move(file))
            {
            }

            CheckSave(const CheckSave &) = delete;
            CheckSave &operator=(const CheckSave &) = delete;

            ~CheckSave()
            {
                std::error_code ignored;
                std::filesystem::remove(path, ignored);
            }
        } save{presentation::path_from_godot(ProjectSettings::get_singleton()->globalize_path(
            "user://checks/equipment-art-" +
            String::num_int64(OS::get_singleton()->get_process_id()) + ".ogs"))};

        std::filesystem::create_directories(save.path.parent_path());
        save_campaign(save.path);
        gear(2, false);
        verify_preview(0, 33);
        load_campaign(save.path);
        verify_preview(0, 34);
        party_selected(1);
        verify_preview(1, 34);
        start_check_fight();
        verify_combat();
    }
    else if (check_stage_ == 18)
    {
        verify_combat();
        capture("equipment-art-combat.png");
        auto *combat = &required_node<CombatView>(*this, "CampaignCombat");
        // Freeing the fight releases the fixture's combat lock.
        presentation::detach_child(*this, *combat).reset();
        CampaignEncounter encounter;
        encounter.field.geometry = {12, 9, std::vector<rules::Terrain>(108)};
        encounter.enemies.push_back(
            {1000, "bandit", "Artwork fixture", rules::Side::opposition, {9, 4}});
        // A distinguishable authored enemy texture must survive party-only resolution.
        opengold::Image enemy;
        enemy.width = enemy.height = 24;
        enemy.rgba.assign(24 * 24 * 4, 255);
        encounter.art.push_back({1000, enemy, enemy});
        presentation::attach_child(*this, campaign_combat_scene(campaign_, *art_,
                                                                looks_or_none(body_catalog_),
                                                                std::move(encounter)));
        verify_combat();
        for (bool action : {false, true})
        {
            const auto texture =
                required_node<CombatView>(*this, "CampaignCombat")
                    .sprite_texture(1000, action ? por::IconPose::action : por::IconPose::ready);
            require(texture.is_valid() && texture->get_image()->get_data() ==
                                              presentation::rgba_image(enemy)->get_data(),
                    "Party equipment changed an encounter creature's texture");
        }
    }
    else if (check_stage_ == 19)
    {
        auto *combat = &required_node<CombatView>(*this, "CampaignCombat");
        presentation::detach_child(*this, *combat).reset();
        auto character = check_fighter();
        auto appearance = character.appearance();
        appearance.combat_body = 24;
        character.appearance(appearance);
        for (const auto &option : body_catalog_.value().options())
        {
            if (option.original_type)
            {
                por::Equipment item;
                item.stored.type = static_cast<uint8_t>(option.original_type);
                item.stored.stack_size = 1;
                character.add_item({.definition_id = equipment_conversion(item),
                                    .name = option.label,
                                    .quantity = 1,
                                    .original_type = option.original_type});
            }
        }
        character.add_item(
            {.definition_id = "shield", .name = "Shield", .quantity = 1, .original_type = 59});
        const auto id = campaign_->add_pc(std::move(character));
        // The roster shows the party in its order, so the new member's row is
        // its place there, not its place in the roster.
        const auto rows = roster_rows();
        party_selected(std::find(rows.begin(), rows.end(), campaign_->state().roster.size() - 1) -
                       rows.begin());
        // Exercise the shipped party's buttons, not the separate review UI.
        const auto inventory = campaign_->member(id).character.inventory().items();
        for (unsigned index = 0; index + 1 < inventory.size(); ++index)
        {
            gear(index, true);
            require(campaign_->member(id).equipped ==
                        std::vector<std::uint64_t>{inventory[index].id},
                    "Game Equip did not replace the weapon");
            const auto resolved =
                por::resolve_combat_appearance(campaign_->member(id), body_catalog_.value());
            require(resolved.appearance == appearance, "Game equipment changed character anatomy");
            for (bool action : {false, true})
            {
                require(required_node<TextureRect>(*this, action ? "PartyPanel/ActionSprite"
                                                                 : "PartyPanel/ReadySprite")
                                .get_texture()
                                ->get_image()
                                ->get_data() ==
                            presentation::rgba_image(
                                resolved.icon(*art_, action ? por::IconPose::action
                                                            : por::IconPose::ready))
                                ->get_data(),
                        "Game preview differs from shared compositor");
            }
        }
        require(inventory.size() == 48, "Game integration fixture missed reviewer weapons");
    }
    else
    {
        UtilityFunctions::print(
            "Equipment artwork checks passed: all 47 weapons through game controls, PC, NPC, both poses, save/load, training, campaign and unchanged creature art");
        equipment_art_check_ = false;
        get_tree()->quit(0);
    }
    ++check_stage_;
}

bool CharacterCreationView::open_equipment_choice(MemberId member, std::uint64_t item)
{
    if (campaign_->in_combat())
    {
        throw std::runtime_error("Equipment cannot change during combat");
    }
    auto choices = campaign_->equipment_choices(member, item);
    if (choices.empty())
    {
        return false;
    }
    // Made the first time an item offers a choice of hands, then kept.
    auto *dialog = Object::cast_to<EquipmentChoiceDialog>(get_node_or_null("EquipmentChoice"));
    if (!dialog)
    {
        auto owned = EquipmentChoiceDialog::create();
        owned->connect_host(
            [this]() -> CampaignParty &
            {
                return *campaign_;
            },
            [this]
            {
                required_node<Button>(*this, "PartyPanel/ActionRow1/Equip").grab_focus();
            },
            [this]
            {
                error_ = String();
                refresh_party();
            },
            [this](const std::exception &failure)
            {
                report_failure(failure);
            });
        dialog = presentation::attach_child(*this, std::move(owned));
    }
    dialog->open(member, item, std::move(choices));
    return true;
}

void CharacterCreationView::open_reincarnation(std::int64_t caster, std::int64_t target,
                                               String action)
{
    if (!campaign_)
    {
        return;
    }
    begin_reincarnation(
        {static_cast<MemberId>(caster), static_cast<MemberId>(target), action.utf8().get_data()},
        campaign_->reincarnation_species());
}

void CharacterCreationView::begin_reincarnation(Reincarnation spell, const std::string &species)
{
    auto *town = Object::cast_to<RolfTourView>(get_node_or_null("CampaignTown"));
    if (!town)
    {
        return;
    }
    waiting_creation_.emplace(CreationInProgress{std::move(creator_), std::move(completed_),
                                                 added_to_party_, portrait_chosen_});
    creator_ =
        std::make_unique<CharacterCreator>(srd5::character_rules(), campaign_->rule_module(),
                                           campaign_->member(spell.target).character, species);
    reincarnation_ = std::move(spell);
    completed_.reset();
    added_to_party_ = false;
    // A face of the new species, which the portrait page may change.
    portrait_chosen_ = false;
    recommend_portrait();
    recommend_icon_size();
    error_ = "";
    town->hide();
    town->set_process(false);
    town->set_process_input(false);
    required_node<Button>(*this, "ReturnParty").hide();
    required_node<Control>(*this, "PartyPanel").hide();
    refresh();
}

void CharacterCreationView::finish_reincarnation()
{
    if (!reincarnation_ || !completed_)
    {
        return;
    }
    campaign_->reincarnate(*srd5::character_rules(), reincarnation_->caster, reincarnation_->target,
                           reincarnation_->action, creator_->reincarnation_choices(),
                           creator_->appearance());
    leave_reincarnation();
}

void CharacterCreationView::leave_reincarnation()
{
    end_reincarnation();
    refresh_party();
    if (auto *town = Object::cast_to<RolfTourView>(get_node_or_null("CampaignTown")))
    {
        town->show();
        town->set_process(true);
        town->set_process_input(true);
        town->resume_party();
    }
    auto *back = &required_node<Button>(*this, "ReturnParty");
    move_child(back, get_child_count() - 1);
    back->show();
}

void CharacterCreationView::end_reincarnation()
{
    if (!reincarnation_)
    {
        return;
    }
    reincarnation_.reset();
    auto waiting = std::move(*waiting_creation_);
    waiting_creation_.reset();
    creator_ = std::move(waiting.creator);
    completed_ = std::move(waiting.completed);
    added_to_party_ = waiting.added_to_party;
    portrait_chosen_ = waiting.portrait_chosen;
    auto *name = &required_node<LineEdit>(*this, "Name");
    name->set_editable(true);
    name->set_text(String::utf8(creator_->draft().name.c_str()));
    required_node<OptionButton>(*this, "Gender").set_disabled(false);
    required_node<Button>(*this, "Restart").set_text(i18n::text(N_("Start over")));
    required_node<Button>(*this, "AddParty").set_text(i18n::text(N_("Add to party")));
    recommend_portrait();
    refresh();
}

void CharacterCreationView::connect_party_button(const char *path, PartyAction action)
{
    // A bound signal argument travels as a Variant, which carries the action
    // as its number; party_button_pressed turns it back.
    required_node<Button>(*this, path)
        .connect("pressed",
                 presentation::guarded(this, &CharacterCreationView::party_button_pressed)
                     .bind(static_cast<int>(action)));
}

void CharacterCreationView::party_button_pressed(int action)
{
    party_action(static_cast<PartyAction>(action));
}

void CharacterCreationView::party_action(PartyAction action)
{
    try
    {
        if (campaign_defeated_)
        {
            return;
        }
        error_ = "";
        String equipment_notice;
        const auto id =
            campaign_->state().roster.empty() ? 0 : campaign_->state().roster.at(roster_index_).id;
        if (action == PartyAction::add_created && reincarnation_)
        {
            finish_reincarnation();
            return;
        }
        if (action == PartyAction::add_created)
        {
            if (!completed_ || added_to_party_)
            {
                return;
            }
            (void)campaign_->add_pc(*completed_);
            campaign_->add_coins(Coin::gold, creator_->starting_gold());
            added_to_party_ = true;
            roster_index_ = campaign_->state().roster.size() - 1;
            refresh();
        }
        if (action == PartyAction::create || action == PartyAction::close)
        {
            party_open_ = false;
            required_node<Control>(*this, "PartyPanel").hide();
            if (action == PartyAction::create)
            {
                restart();
            }
            return;
        }
        if (action == PartyAction::remove)
        {
            campaign_->remove(id);
        }
        if (action == PartyAction::rejoin)
        {
            campaign_->rejoin(id);
        }
        if (action == PartyAction::equip || action == PartyAction::unequip)
        {
            const auto selection =
                required_node<ItemList>(*this, "PartyPanel/Inventory").get_selected_items();
            if (selection.is_empty())
            {
                throw std::runtime_error("Select an inventory item first");
            }
            // Copies: equipping can replace the member's inventory, and with it
            // anything still pointing into it (Effective C++ Item 28).
            const auto items = campaign_->member(id).character.inventory().items();
            if (selection[0] < 0 || static_cast<std::size_t>(selection[0]) >= items.size())
            {
                throw std::runtime_error("Select an existing item");
            }
            const auto selected = items[selection[0]].id;
            const std::string definition = rules_definition(campaign_->member(id), items[selection[0]]);
            if (action == PartyAction::equip)
            {
                if (open_equipment_choice(id, selected))
                {
                    return;
                }
                campaign_->equip(id, selected);
                equipment_notice = i18n::text("Equipped.") + " " +
                                   i18n::text(srd5::equipment_note(
                                       campaign_->member(id).character.sheet(), definition));
            }
            else
            {
                campaign_->unequip(id, selected);
            }
        }
        if (action == PartyAction::explore)
        {
            if (!campaign_->selected())
            {
                throw std::runtime_error("Add a party member first");
            }
            auto *town = Object::cast_to<RolfTourView>(get_node_or_null("CampaignTown"));
            if (!town)
            {
                auto owned = presentation::instantiate_scene("res://scenes/rolf_tour.tscn");
                town = Object::cast_to<RolfTourView>(owned.get());
                if (!town)
                {
                    throw std::runtime_error("Invalid exploration scene");
                }
                town->set_name("CampaignTown");
                town->campaign_party(campaign_);
                if (OS::get_singleton()->get_cmdline_user_args().has("--save-check-write"))
                {
                    town->enable_save_check(
                        [this](const auto &name)
                        {
                            save_checkpoint_check(name);
                        });
                }
                town->connect("save_requested",
                              presentation::guarded(this, &CharacterCreationView::open_saves));
                town->connect(
                    "menu_requested",
                    presentation::guarded(this, &CharacterCreationView::open_game_menu));
                auto *sheet = &required_node<TownSheetDialog>(*this, "TownSheet");
                town->connect("party_member_selected",
                              presentation::guarded(sheet, &TownSheetDialog::show_member));
                town->connect(
                    "level_up_requested",
                    presentation::guarded(this, &CharacterCreationView::open_advancement));
                town->connect(
                    "reincarnation_requested",
                    presentation::guarded(this, &CharacterCreationView::open_reincarnation));
                presentation::attach_child(*this, std::move(owned));
            }
            town->show();
            town->set_process(true);
            town->set_process_input(true);
            town->resume_party();
            leave_roster();
            return;
        }
        if (action == PartyAction::return_to_party)
        {
            if (auto *combat = Object::cast_to<CombatView>(get_node_or_null("CampaignCombat")))
            {
                if (!combat->can_leave())
                {
                    throw std::runtime_error("Finish the fight before returning to the party");
                }
                presentation::detach_child(*this, *combat).reset();
                check_fight_ = false;
            }
            if (auto *town = Object::cast_to<RolfTourView>(get_node_or_null("CampaignTown")))
            {
                if (town->is_visible() && !town->can_leave())
                {
                    throw std::runtime_error("Finish the dialogue or leave the shop first");
                }
                town->hide();
                town->set_process(false);
                town->set_process_input(false);
            }
            required_node<Button>(*this, "ReturnParty").hide();
        }
        party_open_ = true;
        auto *panel = &required_node<Control>(*this, "PartyPanel");
        move_child(panel, get_child_count() - 1);
        panel->show();
        refresh_party();
        if (!equipment_notice.is_empty())
        {
            required_node<Label>(*this, "PartyPanel/Status").set_text(equipment_notice);
        }
    }
    catch (const std::exception &e)
    {
        error_ = i18n::text(e.what());
        required_node<Label>(*this, "Status").set_text(error_);
        required_node<Label>(*this, "PartyPanel/Status").set_text(error_);
        required_node<Button>(*this, "ReturnParty").set_tooltip_text(error_);
    }
}

void CharacterCreationView::leave_roster()
{
    required_node<Control>(*this, "PartyPanel").hide();
    auto *back = &required_node<Button>(*this, "ReturnParty");
    move_child(back, get_child_count() - 1);
    back->show();
    party_layout();
}

void CharacterCreationView::start_check_fight()
{
    const rules::Cell bandit_cell{9, 4};
    CampaignEncounter encounter;
    encounter.field.geometry = {12, 9, std::vector<rules::Terrain>(108)};
    encounter.enemies.push_back({1000, "bandit", "Bandit", rules::Side::opposition, bandit_cell});
    for (const auto &member : campaign_->participants())
    {
        encounter.positions.push_back(member.cell);
    }
    encounter.positions.push_back(bandit_cell);
    presentation::attach_child(*this,
                               campaign_combat_scene(campaign_, *art_, looks_or_none(body_catalog_),
                                                     std::move(encounter)));
    check_fight_ = true;
    leave_roster();
}

void CharacterCreationView::party_check()
{
    const auto press = [&](const char *node)
    {
        required_node<Button>(*this, node).emit_signal("pressed");
        if (!error_.is_empty())
        {
            throw std::runtime_error(error_.utf8().get_data());
        }
    };
    auto *pool = &required_node<CharacterPoolDialog>(*this, "PoolModal");
    if (pool_check_stage_ < 3)
    {
        if (pool_check_stage_ == 0)
        {
            show_pool();
            if (pool->characters().size() != 48)
            {
                throw std::runtime_error("Pool must contain four characters per class");
            }
        }
        else if (pool_check_stage_ == 1)
        {
            std::map<std::string, unsigned> classes;
            std::set<std::string> names;
            for (unsigned i = 0; i < pool->characters().size(); ++i)
            {
                const auto &c = pool->characters()[i];
                const auto &s = c.sheet();
                ++classes[s.character_class];
                names.insert(s.name);
                if (s.level != 1 || *std::min_element(s.scores.begin(), s.scores.end()) < 13 ||
                    *std::max_element(s.scores.begin(), s.scores.end()) > 20)
                {
                    throw std::runtime_error("Invalid pool ability range");
                }
                if (!s.training.complete)
                {
                    throw std::runtime_error("Preset training must be complete");
                }
                art_->validate(c.appearance());
                pool->select(i);
            }
            if (names.size() != 48 || classes.size() != 12 ||
                std::any_of(classes.begin(), classes.end(),
                            [](const auto &c)
                            {
                                return c.second != 4;
                            }))
            {
                throw std::runtime_error("Pool class counts or names invalid");
            }
            pool->select(19);
            required_node<ItemList>(*this, "PoolModal/List").select(19);
        }
        else
        {
            if (capture_)
            {
                const auto image =
                    required_node<Window>(*this, "PoolModal").get_texture()->get_image();
                if (image.is_valid())
                {
                    image->save_png(ProjectSettings::get_singleton()->globalize_path(
                        "user://checks/character-pool.png"));
                }
            }
            const auto state = campaign_->checkpoint();
            pool->add();
            pool->add();
            if (campaign_->state().roster.size() != state.roster.size() + 1)
            {
                throw std::runtime_error("Pool add or duplicate guard failed");
            }
            campaign_->restore(state);
            pool->mark_added(*campaign_);
            roster_index_ = 0;
            pool->close();
        }
        ++pool_check_stage_;
        return;
    }
    switch (party_check_stage_)
    {
    case 0:
    {
        creator_->select(rules::CreationField::race,
                         OS::get_singleton()->get_cmdline_user_args().has("--adrenaline-check")
                             ? "orc"
                             : "dwarf");
        creator_->select(rules::CreationField::character_class, "fighter");
        recommend_portrait();
        creator_->roll();
        creator_->name(OS::get_singleton()->get_cmdline_user_args().has("--adrenaline-check")
                           ? "Party check Orc fighter"
                           : "Party check Dwarf fighter");
        for (unsigned i = 0; i < 6; ++i)
        {
            creator_->assign_roll(i, static_cast<rules::Ability>(i));
        }
        for (unsigned attempt = 0;
             !rules::class_eligible(creator_->rules(), creator_->draft(), "fighter"); ++attempt)
        {
            if (attempt == 100)
            {
                throw std::runtime_error("Could not roll qualified party-check fixture");
            }
            creator_->roll();
            for (unsigned i = 0; i < 6; ++i)
            {
                creator_->assign_roll(i, static_cast<rules::Ability>(i));
            }
        }
        while (creator_->step() != CreationStep::portrait)
        {
            const auto before = creator_->step();
            if (before == CreationStep::training)
            {
                required_node<CheckBox>(*this, "Training/Rows/Group1/athletics").set_pressed(true);
                required_node<CheckBox>(*this, "Training/Rows/Group1/history").set_pressed(true);
                for (const char *mastery : {"longsword", "handaxe", "javelin"})
                {
                    required_node<CheckBox>(*this, String("Training/Rows/Group2/") + mastery)
                        .set_pressed(true);
                }
                required_node<CheckBox>(*this, "Training/Rows/Group0/archery").set_pressed(true);
            }
            // The default Acolyte background brings Magic Initiate (Cleric): its
            // ability, cantrips and spell, each the first one offered.
            if (before == CreationStep::spell_choices)
            {
                for (const auto &group :
                     creator_->rules().spell_choice_options(creator_->draft()).learning)
                {
                    const auto section = String("SpellChoices/Rows/BookChoices/") +
                                         String::utf8(group.id.c_str()).replace(":", "_") + "/";
                    for (unsigned n = 0; n < group.count && n < group.options.size(); ++n)
                    {
                        required_node<CheckBox>(*this,
                                                section + String::utf8(group.options[n].id.c_str()))
                            .set_pressed(true);
                    }
                }
            }
            next();
            if (creator_->step() == before)
            {
                throw std::runtime_error("Party-check creation did not advance");
            }
        }
        portrait_part(1);
        next();
        if (creator_->step() != CreationStep::sheet)
        {
            throw std::runtime_error("Portrait must advance to the character sheet");
        }
        const auto chosen = completed_->appearance();
        press("AddParty");
        if (campaign_->state().slots[0] == 0)
        {
            throw std::runtime_error("Add party callback failed");
        }
        const auto gold = coins(campaign_->state().wealth, Coin::gold);
        if (gold != creator_->starting_gold() || gold < 30 || gold > 180)
        {
            throw std::runtime_error("A new PC must bring its rolled 3d6 x 10 gp");
        }
        if (bool(required_node<Control>(*this, "PortraitScroll/PortraitSheet").get("enabled")))
        {
            throw std::runtime_error("Added portrait controls remained enabled");
        }
        portrait_part(1);
        if (completed_->appearance() != chosen ||
            campaign_->member(campaign_->state().slots[0]).character.appearance() != chosen)
        {
            throw std::runtime_error("Party portrait changed after adding");
        }
        (void)campaign_->recruit("check:guard", check_fighter());
        if (!campaign_->state().slots[6])
        {
            throw std::runtime_error("The recruit must take an NPC position");
        }
        roster_index_ = campaign_->state().roster.size() - 1;
        refresh_party();
        press("PartyPanel/ActionRow1/Remove");
        press("PartyPanel/ActionRow1/Rejoin");
        party_selected(0);
        capture("party-roster.png");
        ++party_check_stage_;
        break;
    }
    case 1:
        press("PartyPanel/ActionRow2/Explore");
        ++party_check_stage_;
        break;
    case 2:
        if (!required_node<RolfTourView>(*this, "CampaignTown").party_route_checked())
        {
            return;
        }
        press("ReturnParty");
        party_selected(0);
        if (campaign_->member(campaign_->selected()).character.inventory().items().size() != 1)
        {
            throw std::runtime_error("Town purchase missing from party inventory");
        }
        required_node<ItemList>(*this, "PartyPanel/Inventory").select(0);
        press("PartyPanel/ActionRow1/Equip");
        ++party_check_stage_;
        break;
    case 3:
        if (!required_node<RichTextLabel>(*this, "PartyPanel/Sheet")
                 .get_text()
                 .contains("Saving throw"))
        {
            throw std::runtime_error("Party selection did not display the character sheet");
        }
        press("PartyPanel/ActionRow2/Modifiers");
        if (!required_node<RichTextLabel>(*this, "ModifiersModal/Text")
                 .get_text()
                 .contains("Shield: +2 AC"))
        {
            throw std::runtime_error("Party modifiers omitted equipped shield");
        }
        if (!required_node<RichTextLabel>(*this, "ModifiersModal/Text")
                 .get_text()
                 .contains(OS::get_singleton()->get_cmdline_user_args().has("--adrenaline-check")
                               ? "Adrenaline Rush"
                               : "Resistance to Poison damage"))
        {
            throw std::runtime_error("Created Dwarf is missing its sourced damage resistance");
        }
        press("ModifiersModal/Close");
        press("PartyPanel/ActionRow2/SavingThrows");
        if (!required_node<Label>(*this, "SavingThrowsModal/Title")
                 .get_text()
                 .contains(
                     gs(campaign_->state().roster.at(roster_index_).character.sheet().name)) ||
            !required_node<RichTextLabel>(*this, "SavingThrowsModal/Text")
                 .get_text()
                 .contains("saving throw proficiency"))
        {
            throw std::runtime_error("Party saving throws did not use selected character");
        }
        press("SavingThrowsModal/Close");
        capture("party-equipped.png");
        start_check_fight();
        ++party_check_stage_;
        break;
    case 4:
    {
        auto *fight = &required_node<CombatView>(*this, "CampaignCombat");
        if (!fight->can_leave())
        {
            return;
        }
        if (fight->completed_outcome()->outcome != rules::Outcome::victory)
        {
            throw std::runtime_error("The party must beat one Bandit");
        }
        press("ReturnParty");
        campaign_->award_experience(300, "check:bandit:v1");
        capture("party-after-combat.png");
        if (campaign_->in_combat() ||
            campaign_->member(campaign_->state().slots[0]).vitals.resources.empty())
        {
            throw std::runtime_error("Combat state was not returned");
        }
        const auto id = campaign_->state().slots[0];
        if (campaign_->member(id).character.sheet().level != 1 || !campaign_->can_advance(id))
        {
            throw std::runtime_error("XP must wait for explicit level-up confirmation");
        }
        refresh_advancement_arrows();
        const auto path = "PartyPanel/Roster/Advance" + std::to_string(id);
        press(path.c_str());
        if (!required_node<Window>(*this, "LevelUp").is_visible())
        {
            throw std::runtime_error("Level-up arrow did not open choices");
        }
        press("LevelUp/Cancel");
        if (campaign_->member(id).character.sheet().level != 1)
        {
            throw std::runtime_error("Cancel applied advancement");
        }
        press(path.c_str());
        press("LevelUp/Confirm");
        const auto slots = campaign_->state().slots;
        for (const auto other : slots)
        {
            if (other && other != id && campaign_->can_advance(other))
            {
                open_advancement(other);
                press("LevelUp/Confirm");
            }
        }
        const auto sheet = required_node<RichTextLabel>(*this, "PartyPanel/Sheet").get_text();
        if (!sheet.contains("Level 2") || !sheet.contains("XP 300"))
        {
            throw std::runtime_error("Victory advancement is missing from character sheet");
        }
        if (OS::get_singleton()->get_cmdline_user_args().has("--save-check-write"))
        {
            save_checkpoint_check("advancement");
        }
        ++party_check_stage_;
        break;
    }
    case 5:
        press("PartyPanel/ActionRow2/Explore");
        required_node<RolfTourView>(*this, "CampaignTown").start_recovery_check();
        ++party_check_stage_;
        break;
    case 6:
        if (!required_node<RolfTourView>(*this, "CampaignTown").recovery_checked())
        {
            return;
        }
        press("ReturnParty");
        ++party_check_stage_;
        break;
    case 7:
        capture("party-recovered.png");
        start_check_fight();
        ++party_check_stage_;
        break;
    case 8:
        if (!required_node<CombatView>(*this, "CampaignCombat").can_leave())
        {
            return;
        }
        press("ReturnParty");
        campaign_->award_experience(300, "check:bandit:v1");
        if (campaign_->state().roster.at(0).experience != 300)
        {
            throw std::runtime_error("A reward must be paid only once");
        }
        if (OS::get_singleton()->get_cmdline_user_args().has("--save-check-write"))
        {
            save_checkpoint_check("final");
        }
        UtilityFunctions::print(
            "Godot party check passed: creation, shops, combat, level-two sheet, recovery services, subsequent combat and exactly-once XP");
        party_check_ = false;
        get_tree()->quit(0);
        break;
    }
}

void CharacterCreationView::update_party_navigation()
{
    bool allowed = true;
    auto *town = Object::cast_to<RolfTourView>(get_node_or_null("CampaignTown"));
    auto *fight = Object::cast_to<CombatView>(get_node_or_null("CampaignCombat"));
    if (!fight && town && town->is_visible() && town->pending_encounter() &&
        !campaign_->state().short_rest)
    {
        presentation::NodeOwner<> owned;
        try
        {
            owned = campaign_combat_scene(campaign_, *art_, looks_or_none(body_catalog_),
                                          *town->pending_encounter());
        }
        catch (const std::exception &e)
        {
            if (!town->reject_combat(e.what()))
            {
                throw;
            }
            return;
        }
        fight = Object::cast_to<CombatView>(owned.get());
        presentation::attach_child(*this, std::move(owned));
        town->hide();
        town->set_process(false);
        town->set_process_input(false);
        party_layout();
    }
    if (fight)
    {
        if (fight->expedition() && !campaign_defeated_ && !check_fight_)
        {
            if (const auto outcome = fight->completed_outcome())
            {
                if (!town || !town->resolve_combat(*outcome))
                {
                    throw std::runtime_error("Exploration rejected the combat result");
                }
                // After a victory or a flight, exploration resumes.
                if (outcome->outcome == rules::Outcome::victory ||
                    outcome->outcome == rules::Outcome::fled)
                {
                    const auto released = presentation::detach_child(*this, *fight);
                    fight = nullptr;
                    town->show();
                    town->set_process(true);
                    town->set_process_input(true);
                    town->resume_party();
                }
            }
        }
        if (fight)
        {
            allowed = fight->can_leave();
            if (fight->defeated() && !campaign_defeated_)
            {
                show_defeat();
            }
        }
    }
    if (auto *shown_town = Object::cast_to<RolfTourView>(get_node_or_null("CampaignTown")))
    {
        if (shown_town->is_visible())
        {
            allowed = shown_town->can_leave();
        }
    }
    if (campaign_defeated_)
    {
        allowed = false;
    }
    auto *button = &required_node<Button>(*this, "ReturnParty");
    button->set_disabled(!allowed);
    button->set_tooltip_text(i18n::text(
        allowed ? N_("Inspect your party and equipment.")
                : N_("Finish combat, dialogue or shopping before returning to the party.")));
}

void CharacterCreationView::expedition_check()
{
    if (++expedition_frames_ > 20000)
    {
        throw std::runtime_error("Expedition check timed out");
    }
    if (!expedition_started_)
    {
        for (unsigned i = 0; i < 6; ++i)
        {
            auto character = check_fighter();
            const auto id = campaign_->add_pc(std::move(character));
            campaign_->add_coins(Coin::gold, 500);
            for (unsigned type : {36, 55, 59})
            {
                por::Equipment gear;
                gear.stored.type = static_cast<uint8_t>(type);
                gear.stored.stack_size = 1;
                gear.stored.value = 1;
                campaign_->purchase(id, gear);
                campaign_->equip(id, campaign_->member(id).character.inventory().items().back().id);
            }
        }
        campaign_->award_experience(2700, "fixture:experienced-party");
        const auto slots = campaign_->state().slots;
        for (const auto id : slots)
        {
            if (id)
            {
                for (unsigned level = 2; level <= 4; ++level)
                {
                    auto choice = campaign_->default_advancement(id);
                    if (level == 4)
                    {
                        choice.feat = "defense";
                        choice.abilities = {};
                    }
                    campaign_->advance(id, choice);
                }
            }
        }
        expedition_started_ = true;
        party_action(PartyAction::explore);
        return;
    }
    if (campaign_defeated_)
    {
        throw std::runtime_error("Expedition fixture was defeated");
    }
    if (get_node_or_null("CampaignCombat"))
    {
        return;
    }
    auto *town = &required_node<RolfTourView>(*this, "CampaignTown");
    if (const auto *state = town->saved_session();
        state && state->can_leave() && state->snapshot().area_id == 20 && !expedition_saved_)
    {
        const auto pack = presentation::path_from_godot(game_rules_file());
        const auto saved = encode_campaign(*campaign_, state, "expedition-fixture");
        auto loaded = decode_campaign(saved, *srd5::character_rules(), *srd5::load(pack),
                                      "expedition-fixture", state);
        loaded.town->attach_restored_party(campaign_);
        if (encode_campaign(*campaign_, &*loaded.town, "expedition-fixture") != saved)
        {
            throw std::runtime_error("Slums district save round trip differs");
        }
        town->restore_campaign(campaign_, std::move(*loaded.town));
        expedition_saved_ = true;
        return;
    }
    if (!town->check_expedition_step())
    {
        return;
    }
    const auto *session = town->saved_session();
    if (!session || session->script_variable(por::ecl_slums_orc_victory) != 255)
    {
        throw std::runtime_error("Four-orc victory flag missing");
    }
    const auto &rewards = campaign_->state().claimed_rewards;
    if (std::count(rewards.begin(), rewards.end(), "por:ECL2:20:search1:orcs:v1") != 1)
    {
        throw std::runtime_error("Four-orc XP reward missing or repeated");
    }
    UtilityFunctions::print(
        "Godot expedition passed: gate, roaming encounter, original arena, four-orc victory, automatic exploration return and town gate.");
    expedition_check_ = false;
    get_tree()->quit(0);
}

void CharacterCreationView::setup_defeat()
{
    auto window = presentation::instantiate_scene("res://scenes/dialog_layouts/Defeat.tscn");
    presentation::attach_child(*this, std::move(window));
    auto *dialog = &required_node<Window>(*this, "Defeat");
    dialog->set_title(i18n::text(N_("Defeat")));
    required_node<Label>(*dialog, "Title")
        .set_text(i18n::text(N_("Your party has been defeated.")));
    required_node<Label>(*dialog, "Body")
        .set_text(i18n::text(N_("Load a saved game to continue.")));
    for (bool reload : {true, false})
    {
        auto *button = &required_node<Button>(*dialog, reload ? "Reload" : "Exit");
        button->set_text(i18n::text(reload ? N_("Reload a Saved Game") : N_("Exit to OS")));
        button->connect(
            "pressed",
            reload ? presentation::guarded(this, &CharacterCreationView::reload_after_defeat)
                   : presentation::guarded(this, &CharacterCreationView::exit_after_defeat));
    }
    dialog->connect("close_requested",
                    presentation::guarded(this, &CharacterCreationView::show_defeat));
    required_node<SaveSlots>(*this, "SaveSlots")
        .connect(
            "visibility_changed",
            presentation::guarded(this, &CharacterCreationView::save_dialog_visibility_changed));
    defeat_check_ = OS::get_singleton()->get_cmdline_user_args().has("--defeat-check");
}

void CharacterCreationView::show_defeat()
{
    campaign_defeated_ = true;
    required_node<Button>(*this, "ReturnParty").hide();
    if (auto *fight = Object::cast_to<CombatView>(get_node_or_null("CampaignCombat")))
    {
        fight->set_process_input(false);
    }
    auto *dialog = &required_node<Window>(*this, "Defeat");
    if (!dialog->is_visible())
    {
        dialog->popup_centered();
    }
    required_node<Button>(*dialog, "Reload").grab_focus();
}

void CharacterCreationView::reload_after_defeat()
{
    if (!campaign_defeated_)
    {
        return;
    }
    required_node<Window>(*this, "Defeat").hide();
    open_saves(false);
}

void CharacterCreationView::save_dialog_visibility_changed()
{
    // Window releases its exclusive-child slot after emitting visibility_changed.
    presentation::guarded(this, &CharacterCreationView::restore_defeat_dialog).call_deferred();
}

void CharacterCreationView::restore_defeat_dialog()
{
    if (campaign_defeated_ && !required_node<SaveSlots>(*this, "SaveSlots").is_visible())
    {
        show_defeat();
    }
}

void CharacterCreationView::exit_after_defeat()
{
    if (campaign_defeated_)
    {
        get_tree()->quit(0);
    }
}

void CharacterCreationView::defeat_check()
{
    auto *dialog = &required_node<Window>(*this, "Defeat");
    auto *saves = &required_node<SaveSlots>(*this, "SaveSlots");
    if (++defeat_check_frames_ > 4000)
    {
        throw std::runtime_error("Defeat check timed out");
    }
    if (defeat_check_stage_ == 0)
    {
        (void)campaign_->add_pc(check_fighter());
        campaign_->add_coins(Coin::gold, 250);
        open_saves(true);
        required_node<LineEdit>(*saves, "Name").set_text(i18n::text(N_("Defeat test")));
        required_node<Button>(*saves, "Action").emit_signal("pressed");
        if (saves->is_visible())
        {
            required_node<Button>(*saves, "Action").emit_signal("pressed");
        }
        if (saves->is_visible())
        {
            throw std::runtime_error("Defeat fixture save failed");
        }
        start_check_fight();
        ++defeat_check_stage_;
        return;
    }
    if (defeat_check_stage_ == 1)
    {
        if (!campaign_defeated_)
        {
            return;
        }
        auto *fight = &required_node<CombatView>(*this, "CampaignCombat");
        if (!dialog->is_visible() || fight->can_leave() || campaign_->in_combat() ||
            campaign_->state().roster.at(0).vitals.hit_points)
        {
            throw std::runtime_error("Defeat did not lock gameplay with persisted zero HP");
        }
        party_action(PartyAction::return_to_party);
        if (!get_node_or_null("CampaignCombat") || !dialog->is_visible())
        {
            throw std::runtime_error("Return to party bypassed defeat");
        }
        required_node<Button>(*dialog, "Reload").emit_signal("pressed");
        if (!saves->is_visible() || dialog->is_visible())
        {
            throw std::runtime_error("Defeat reload did not open save slots");
        }
        required_node<Button>(*saves, "Cancel").emit_signal("pressed");
        ++defeat_check_stage_;
        return;
    }
    if (defeat_check_stage_ == 2)
    {
        if (!dialog->is_visible())
        {
            throw std::runtime_error("Cancel bypassed defeat");
        }
        auto *fight = &required_node<CombatView>(*this, "CampaignCombat");
        const auto broken =
            presentation::path_from_godot(ProjectSettings::get_singleton()->globalize_path(
                "user://checks/save-check/defeat-corrupt.ogs"));
        write_campaign_file(broken, "corrupt");
        const auto original = campaign_;
        bool rejected = false;
        try
        {
            load_campaign(broken);
        }
        catch (const std::exception &)
        {
            rejected = true;
        }
        if (!rejected || campaign_ != original || !fight->defeated() || !dialog->is_visible())
        {
            throw std::runtime_error("Rejected defeat load changed campaign");
        }
        ++defeat_check_stage_;
        defeat_check_frames_ = 0;
        return;
    }
    if (defeat_check_stage_ == 3)
    {
        if (defeat_check_frames_ < 4)
        {
            return;
        }
        if (capture_)
        {
            const auto image = dialog->get_texture()->get_image();
            if (image.is_null() || image->save_png(ProjectSettings::get_singleton()->globalize_path(
                                       "user://checks/party-defeat.png")) != OK)
            {
                throw std::runtime_error("Cannot capture defeat screen");
            }
        }
        required_node<Button>(*dialog, "Reload").emit_signal("pressed");
        auto *slots = &required_node<ItemList>(*saves, "Slots");
        int selected = -1;
        for (int i = 0; i < slots->get_item_count(); ++i)
        {
            if (slots->get_item_text(i) == "Defeat test")
            {
                selected = i;
            }
        }
        if (selected < 0)
        {
            throw std::runtime_error("Defeat save slot missing");
        }
        slots->select(selected);
        slots->emit_signal("item_selected", selected);
        required_node<Button>(*saves, "Action").emit_signal("pressed");
        if (!campaign_defeated_ || !saves->is_visible())
        {
            throw std::runtime_error("Defeat load skipped confirmation");
        }
        required_node<Button>(*saves, "Action").emit_signal("pressed");
        if (campaign_defeated_ || dialog->is_visible() || saves->is_visible() ||
            get_node_or_null("CampaignCombat") ||
            campaign_->state().roster.at(0).vitals.hit_points == 0)
        {
            throw std::runtime_error("Confirmed reload did not replace defeated campaign");
        }
        start_check_fight();
        ++defeat_check_stage_;
        return;
    }
    if (defeat_check_stage_ == 4 && campaign_defeated_)
    {
        UtilityFunctions::print(
            "Godot defeat check passed: real combat loss, blocked return, cancel, corrupt load rollback, confirmed save reload, subsequent defeat and Exit to OS callback.");
        required_node<Button>(*dialog, "Exit").emit_signal("pressed");
        defeat_check_ = false;
    }
}
