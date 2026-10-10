#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/check_box.hpp>
#include "godot_images.h"
#include "godot_nodes.h"
#include "localization.h"
#include "game_resources.h"
#include "character_creation_view.h"
#include "character_pool_dialog.h"
#include "town_sheet_dialog.h"
#include "character_sheet_text.h"
#include "equipment_choice_dialog.h"
#include "combat_view.h"
#include "rolf_tour_view.h"
#include "save_slots.h"
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
// The exploration screen's gold, which marks the leader in both lists.
const Color leader_gold(215 / 255.f, 180 / 255.f, 121 / 255.f);

String gs(std::string_view text)
{
    return String::utf8(text.data(), text.size());
}

Character preview_guard()
{
    auto rules = srd5::character_rules();
    rules::CharacterDraft draft;
    draft.race = "human";
    draft.gender = "male";
    draft.character_class = "fighter";
    draft.alignment = "lawful_good";
    draft.background = "soldier";
    draft.name = "Preview guard";
    draft.rolled = true;
    for (auto &roll : draft.rolls)
        roll = {{6, 5, 4, 1}, 3};
    return Character(*rules, std::move(draft), {});
}

presentation::NodeOwner<> combat_scene(const std::shared_ptr<CampaignParty> &party,
                                       const por::CharacterArt &art,
                                       const por::CombatBodyCatalog &catalog,
                                       std::optional<CampaignEncounter> encounter = {})
{
    std::vector<CombatArt> images;
    for (const auto &participant : party->participants())
    {
        const auto resolved =
            por::resolve_combat_appearance(party->member(participant.id), catalog);
        images.push_back({participant.id, resolved.icon(art, por::IconPose::ready),
                           resolved.icon(art, por::IconPose::action),
                          resolved.selection.matched ? std::string{} : resolved.selection.label});
    }
    auto owned = presentation::instantiate_scene("res://scenes/combat_demo.tscn");
    auto *combat = Object::cast_to<CombatView>(owned.get());
    if (!combat)
        throw std::runtime_error("Invalid combat scene");
    combat->set_name("CampaignCombat");
    combat->campaign_party(party, std::move(images));
    if (encounter)
        combat->campaign_encounter(std::move(*encounter));
    combat->prepare_combat();
    return owned;
}
} // namespace

void CharacterCreationView::setup_party()
{
    body_catalog_ = por::CombatBodyCatalog::load(
                        presentation::path_from_godot(game_combat_body_file()),
                        presentation::path_from_godot(game_combat_weapon_file()));
    const auto pack = presentation::path_from_godot(game_rules_file());
    campaign_ = std::make_shared<CampaignParty>(srd5::load(pack));
    auto panel = presentation::instantiate_scene("res://scenes/party_panel.tscn");
    i18n::prepare_ui(*panel);
    presentation::attach_child(*this, std::move(panel));
    required_node<Control>(*this, "PartyPanel").hide();
    connect_party_button("Party", PartyAction::open);
    connect_party_button("AddParty", PartyAction::add_created);
    connect_party_button("ReturnParty", PartyAction::return_to_party);
    connect_party_button("PartyPanel/Create", PartyAction::create);
    connect_party_button("PartyPanel/Remove", PartyAction::remove);
    connect_party_button("PartyPanel/Rejoin", PartyAction::rejoin);
    connect_party_button("PartyPanel/Recruit", PartyAction::recruit);
    connect_party_button("PartyPanel/Equip", PartyAction::equip);
    connect_party_button("PartyPanel/Unequip", PartyAction::unequip);
    connect_party_button("PartyPanel/Explore", PartyAction::explore);
    connect_party_button("PartyPanel/Combat", PartyAction::combat);
    connect_party_button("PartyPanel/Close", PartyAction::close);
    required_node<ItemList>(*this, "PartyPanel/Roster")
    .connect("item_selected", presentation::guarded(this, &CharacterCreationView::party_selected));
    required_node<Button>(*this, "PartyPanel/MakeLeader")
    .connect("pressed", presentation::guarded(this, &CharacterCreationView::make_roster_leader));
    required_node<TownSheetDialog>(*this, "TownSheet").connect_host([this]() -> CampaignParty &
    {
        return *campaign_;
    },
    [this]
    {
        show_leader_change();
    },
    [this](const std::exception & failure)
    {
        report_failure(failure);
    });
    required_node<Button>(*this, "PartyPanel/Pool")
    .connect("pressed", presentation::guarded(this, &CharacterCreationView::show_pool));
    required_node<CharacterPoolDialog>(*this, "PoolModal")
    .connect_host(creator_->rules(), *art_, *portraits_, [this]() -> CampaignParty &
    {
        return *campaign_;
    },
    [this]
    {
        roster_index_ = campaign_->state().roster.size() - 1;
        refresh_party();
    },
    [this](const std::exception & failure)
    {
        report_failure(failure);
    });
    required_node<Button>(*this, "PartyPanel/Modifiers")
    .connect("pressed", presentation::guarded(this, &CharacterCreationView::show_modifiers));
    required_node<Button>(*this, "PartyPanel/SavingThrows")
    .connect("pressed", presentation::guarded(this, &CharacterCreationView::show_saving_throws));
    required_node<RichTextLabel>(*this, "PartyPanel/Sheet").set_use_bbcode(true);
    setup_saves();
    setup_defeat();
    setup_advancement();
    party_check_ = OS::get_singleton()->get_cmdline_user_args().has("--party-check");
    party_layout();
    equipment_art_check_ =
        OS::get_singleton()->get_cmdline_user_args().has("--equipment-art-check");
    expedition_check_ = OS::get_singleton()->get_cmdline_user_args().has("--expedition-check");
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
    const auto w = get_size().x, h = get_size().y;
    presentation::size_scene_control(required_node<Control>(*this, "PartyPanel"), get_size());
    const auto place = [&](const char *name, Rect2 rect)
    {
        auto *node = &required_node<Control>(*this, name);
        presentation::place_scene_control(*node, rect);
    };
    place("Party", Rect2(24, h - 158, 166, 36));
    place("AddParty", Rect2(218 + page_rect_.size.x - 190, h - 60, 190, 38));
    place("ReturnParty", Rect2(w - 218, 20, 190, 36));
    place("PartyPanel/Title", Rect2(24, 22, w - 48, 40));
    place("PartyPanel/Roster", Rect2(24, 90, 300, h - 350));
    place("PartyPanel/MakeLeader", Rect2(24, h - 252, 300, 36));
    place("PartyPanel/Sheet", Rect2(350, 90, w - 650, h - 380));
    place("PartyPanel/Portrait", Rect2(w - 284, 90, 264, 264));
    place("PartyPanel/ReadySprite", Rect2(w - 284, 364, 120, 120));
    place("PartyPanel/ActionSprite", Rect2(w - 140, 364, 120, 120));
    place("PartyPanel/ReadyLabel", Rect2(w - 284, 488, 120, 24));
    place("PartyPanel/ActionLabel", Rect2(w - 140, 488, 120, 24));
    place("PartyPanel/Inventory", Rect2(350, h - 280, w - 374, 96));
    // A row of buttons across the window: each as wide as its label needs
    // (longer in some languages) plus an equal share of the room left.
    const auto place_row = [&](std::initializer_list<const char *> names, double y)
    {
        double needed = 0;
        for (const auto *name : names)
            needed += required_node<Control>(*this, gs("PartyPanel/") + name)
                      .get_combined_minimum_size()
                      .x;
        const double gap = 4, spare = std::max(0.0, w - 48 - needed - gap * (names.size() - 1));
        double x = 24;
        for (const auto *name : names)
        {
            auto *button = &required_node<Control>(*this, gs("PartyPanel/") + name);
            const double width = button->get_combined_minimum_size().x + spare / names.size();
            presentation::place_scene_control(*button, Rect2(x, y, width, 36));
            x += width + gap;
        }
    };
    place_row({"Create", "Remove", "Rejoin", "Recruit", "Equip"}, h - 125);
    place_row({"Unequip", "Explore", "Combat", "Close", "Modifiers", "SavingThrows"}, h - 81);
    place("PartyPanel/Save", Rect2(w - 520, 24, 140, 36));
    place("PartyPanel/Load", Rect2(w - 370, 24, 140, 36));
    place("PartyPanel/Pool", Rect2(w - 220, 24, 196, 36));
    required_node<CharacterPoolDialog>(*this, "PoolModal").fit(get_size());
    required_node<TownSheetDialog>(*this, "TownSheet").fit(get_size());
    place("PartyPanel/Status", Rect2(24, h - 39, w - 48, 32));
    for (const auto *name :
            {"CampaignTown", "CampaignCombat"
            })
        if (auto *child = Object::cast_to<Control>(get_node_or_null(name)))
            child->set_size(get_size());
}

void CharacterCreationView::party_selected(std::int64_t index)
{
    if (index < 0 || static_cast<std::size_t>(index) >= campaign_->state().roster.size())
        return;
    roster_index_ = index;
    const auto id = campaign_->state().roster[index].id;
    for (unsigned slot = 0; slot < 8; ++slot)
        if (campaign_->state().slots[slot] == id)
            campaign_->select(PartySlot{slot});
    refresh_party();
}

void CharacterCreationView::refresh_party()
{
    auto *list = &required_node<ItemList>(*this, "PartyPanel/Roster");
    list->clear();
    const auto &state = campaign_->state();
    for (const auto &m : state.roster)
    {
        auto slot = std::find(state.slots.begin(), state.slots.end(), m.id);
        const bool leader = slot != state.slots.end() && m.id == campaign_->leader();
        list->add_item((leader ? String::utf8("★ ") : String()) + gs(m.character.sheet().name) +
                       (slot == state.slots.end() ? i18n::text(" (Reserve)") : String()));
        if (leader)
            list->set_item_custom_fg_color(list->get_item_count() - 1, leader_gold);
    }
    auto *items = &required_node<ItemList>(*this, "PartyPanel/Inventory");
    items->clear();
    std::string sheet = i18n::utf8(
                            "Create a character, finish its sheet, then Add to party.\n\nSix PC positions and two NPC positions. Removed members remain in the roster.");
    if (!state.roster.empty())
    {
        roster_index_ = std::min(roster_index_, state.roster.size() - 1);
        list->select(static_cast<std::int32_t>(roster_index_));
        const auto &m = state.roster[roster_index_];
        sheet = presentation::sheet_text(*campaign_, m).utf8().get_data();
        const auto profile = campaign_->profile(m.id);
        for (const auto &item : m.character.inventory().items())
        {
            const auto found = std::find(m.equipped.begin(), m.equipped.end(), item.id);
            String prefix;
            if (found != m.equipped.end())
                prefix =
                    i18n::text(profile.equipment_positions.at(found - m.equipped.begin()).source) +
                    " / ";
            items->add_item(prefix + i18n::text(item.name) + " x" +
                            gs(std::to_string(item.quantity)));
        }
        required_node<TextureRect>(*this, "PartyPanel/Portrait")
        .set_texture(portraits_->texture(m.character.appearance(), m.character.creation_data()));
        const auto resolved = por::resolve_combat_appearance(m, *body_catalog_);
        for (unsigned pose = 0; pose < 2; ++pose)
        {
            const auto icon = resolved.icon(*art_, pose != 0 ? por::IconPose::action : por::IconPose::ready);
            required_node<TextureRect>(*this,
                                       pose ? "PartyPanel/ActionSprite" : "PartyPanel/ReadySprite")
            .set_texture(presentation::image_texture(icon));
        }
    }
    if (state.roster.empty())
        for (const char *name :
                {"PartyPanel/Portrait", "PartyPanel/ReadySprite", "PartyPanel/ActionSprite"
                })
            required_node<TextureRect>(*this, name).set_texture({});
    required_node<RichTextLabel>(*this, "PartyPanel/Sheet").set_text(gs(sheet));
    required_node<Label>(*this, "PartyPanel/Status")
    .set_text(
        error_.is_empty()
        ? i18n::text("New PCs receive 250 gp / Save game stores this campaign on disk.")
        : error_);
    for (const char *name :
            {"Remove", "Rejoin", "Equip", "Unequip", "Explore", "Combat", "Modifiers", "SavingThrows"
            })
        required_node<Button>(*this, gs(std::string("PartyPanel/") + name))
        .set_disabled(state.roster.empty());
    // Only an active member who is not already the leader can be made leader.
    const auto chosen = state.roster.empty() ? MemberId{} : state.roster[roster_index_].id;
    required_node<Button>(*this, "PartyPanel/MakeLeader")
    .set_disabled(!chosen || chosen == campaign_->leader() ||
                   std::find(state.slots.begin(), state.slots.end(), chosen) == state.slots.end());
}

void CharacterCreationView::make_roster_leader()
{
    const auto &roster = campaign_->state().roster;
    if (roster_index_ >= roster.size())
        return;
    campaign_->make_leader(roster[roster_index_].id);
    show_leader_change();
}

// The roster and the exploration party list both mark the leader.
void CharacterCreationView::show_leader_change()
{
    refresh_party();
    if (auto *town = Object::cast_to<RolfTourView>(get_node_or_null("CampaignTown")))
        town->refresh();
}

// Original-data integration check, also runnable in the packaged executable.
// Equipment actions use the real controls and inspect uploaded texture pixels.
void CharacterCreationView::equipment_art_check()
{
    const auto require = [](bool ok, const char *message)
    {
        if (!ok)
            throw std::runtime_error(message);
    };
    const auto press = [&](const char *path)
    {
        required_node<Button>(*this, path).emit_signal("pressed");
        if (!error_.is_empty())
            throw std::runtime_error(error_.utf8().get_data());
    };
    const auto gear = [&](unsigned index, bool equip)
    {
        required_node<ItemList>(*this, "PartyPanel/Inventory").select(index);
        press(equip ? "PartyPanel/Equip" : "PartyPanel/Unequip");
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
        return presentation::rgba_image(art_->equipped_icon(
                   a, body, action ? por::IconPose::action : por::IconPose::ready))->get_data();
    };
    const auto verify_preview = [&](unsigned member, unsigned body)
    {
        for (bool action :
                {
                    false, true
                })
        {
            const auto texture =
                required_node<TextureRect>(
                    *this, action ? "PartyPanel/ActionSprite" : "PartyPanel/ReadySprite")
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
            for (bool action : {false, true})
        {
            const auto texture =
            combat->sprite_texture(campaign_->state().roster.at(member).id,
                                   action ? por::IconPose::action : por::IconPose::ready);
            require(texture.is_valid() &&
                    texture->get_image()->get_data() == expected(member, 34, action),
                    "Combat textures differ from the equipment shown in party previews");
        }
    };
    if (check_stage_ == 0)
    {
        for (unsigned member = 0; member < 2; ++member)
        {
            const auto guard = preview_guard();
            auto draft = guard.creation_data();
            draft.name = member ? "Equipment check NPC" : "Equipment check PC";
            auto a = guard.appearance();
            a.combat_body = 24;
            a.tall = member == 0;
            a.portrait = portraits_->recommended(draft);
            Character character(*srd5::character_rules(), draft, a);
            const auto id = member
                            ? campaign_->recruit("check:equipment-guard", std::move(character))
                            : campaign_->add_pc(std::move(character));
            for (const unsigned type :
                    {
                        36u, 8u, 59u, 55u
                    })
            {
                por::Equipment item;
                item.stored.type = type;
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
        const std::array<unsigned, 8> bodies
        {
            0, body_catalog_->choose(sword, 24).body, 24, 32, 34, 33, 0, 34};
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
            verify_preview(member,
                           bodies[step + 1]); // Refresh is synchronous with each control action.
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
        } save{presentation::path_from_godot(
                   ProjectSettings::get_singleton()
                   ->globalize_path("user://checks/equipment-art-" +
                                    String::num_int64(OS::get_singleton()->get_process_id()) + ".ogs"))};

        std::filesystem::create_directories(save.path.parent_path());
        save_campaign(save.path);
        gear(2, false);
        verify_preview(0, 33);
        load_campaign(save.path);
        verify_preview(0, 34);
        party_selected(1);
        verify_preview(1, 34);
        press("PartyPanel/Combat");
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
        presentation::attach_child(
            *this, combat_scene(campaign_, *art_, *body_catalog_, std::move(encounter)));
        verify_combat();
        for (bool action :
                {
                    false, true
                })
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
        auto character = preview_guard();
        auto appearance = character.appearance();
        appearance.combat_body = 24;
        character.appearance(appearance);
        for (const auto &option : body_catalog_->options())
            if (option.original_type)
            {
                por::Equipment item;
                item.stored.type = option.original_type;
                item.stored.stack_size = 1;
                character.add_item({.definition_id = equipment_conversion(item),
                                           .name = option.label,
                                           .quantity = 1,
                                           .original_type = option.original_type});
            }
        character.add_item({.definition_id = "shield",
                                   .name = "Shield",
                                   .quantity = 1,
                                   .original_type = 59});
        const auto id = campaign_->add_pc(std::move(character));
        party_selected(campaign_->state().roster.size() - 1);
        // Exercise the shipped party's buttons, not the separate review UI.
        const auto inventory = campaign_->member(id).character.inventory().items();
        for (unsigned index = 0; index + 1 < inventory.size(); ++index)
        {
            gear(index, true);
            require(campaign_->member(id).equipped ==
                    std::vector<std::uint64_t> {inventory[index].id},
                    "Game Equip did not replace the weapon");
            const auto resolved =
                por::resolve_combat_appearance(campaign_->member(id), *body_catalog_);
            require(resolved.appearance == appearance, "Game equipment changed character anatomy");
            for (bool action :
                    {
                        false, true
                    })
                require(required_node<TextureRect>(*this, action ? "PartyPanel/ActionSprite"
                                              : "PartyPanel/ReadySprite")
                        .get_texture()
                        ->get_image()
                        ->get_data() ==
                        presentation::rgba_image(resolved.icon(*art_, action ? por::IconPose::action : por::IconPose::ready))->get_data(),
                                                       "Game preview differs from shared compositor");
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
        throw std::runtime_error("Equipment cannot change during combat");
    auto choices = campaign_->equipment_choices(member, item);
    if (choices.empty())
        return false;
    // Made the first time an item offers a choice of hands, then kept.
    auto *dialog = Object::cast_to<EquipmentChoiceDialog>(get_node_or_null("EquipmentChoice"));
    if (!dialog)
    {
        auto owned = EquipmentChoiceDialog::create();
        owned->connect_host([this]() -> CampaignParty &
        {
            return *campaign_;
        },
        [this]
        {
            required_node<Button>(*this, "PartyPanel/Equip").grab_focus();
        },
        [this]
        {
            error_ = String();
            refresh_party();
        },
        [this](const std::exception & failure)
        {
            report_failure(failure);
        });
        dialog = presentation::attach_child(*this, std::move(owned));
    }
    dialog->open(member, item, std::move(choices));
    return true;
}

void CharacterCreationView::connect_party_button(const char *path, PartyAction action)
{
    // A bound signal argument travels as a Variant, which carries the action
    // as its number; party_button_pressed turns it back.
    required_node<Button>(*this, path)
    .connect("pressed", presentation::guarded(this, &CharacterCreationView::party_button_pressed)
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
            return;
        error_ = "";
        String equipment_notice;
        const auto id =
            campaign_->state().roster.empty() ? 0 : campaign_->state().roster.at(roster_index_).id;
        if (action == PartyAction::add_created)
        {
            if (!completed_ || added_to_party_)
                return;
            const auto added = campaign_->add_pc(*completed_);
            campaign_->set_wealth(added, {0, 0, 0, 250, 0, 0, 0});
            added_to_party_ = true;
            roster_index_ = campaign_->state().roster.size() - 1;
            refresh();
        }
        if (action == PartyAction::create || action == PartyAction::close)
        {
            party_open_ = false;
            required_node<Control>(*this, "PartyPanel").hide();
            if (action == PartyAction::create)
                restart();
            return;
        }
        if (action == PartyAction::remove)
            campaign_->remove(id);
        if (action == PartyAction::rejoin)
            campaign_->rejoin(id);
        if (action == PartyAction::recruit)
        {
            const auto recruited = campaign_->recruit("preview:guard", preview_guard());
            const auto &roster = campaign_->state().roster;
            roster_index_ = std::find_if(roster.begin(), roster.end(),
                                         [&](const auto & m)
            {
                return m.id == recruited;
            }) -
            roster.begin();
        }
        if (action == PartyAction::equip || action == PartyAction::unequip)
        {
            const auto selection =
                required_node<ItemList>(*this, "PartyPanel/Inventory").get_selected_items();
            if (selection.is_empty())
                throw std::runtime_error("Select an inventory item first");
            // Copies: equipping can replace the member's inventory, and with it
            // anything still pointing into it (Effective C++ Item 28).
            const auto items = campaign_->member(id).character.inventory().items();
            if (selection[0] < 0 || static_cast<std::size_t>(selection[0]) >= items.size())
                throw std::runtime_error("Select an existing item");
            const auto selected = items[selection[0]].id;
            const std::string definition = items[selection[0]].definition_id;
            if (action == PartyAction::equip)
            {
                if (open_equipment_choice(id, selected))
                    return;
                campaign_->equip(id, selected);
                equipment_notice =
                    i18n::text("Equipped.") + " " +
                    i18n::text(srd5::equipment_note(campaign_->member(id).character.sheet(),
                                                    definition));
            }
            else
                campaign_->unequip(id, selected);
        }
        if (action == PartyAction::explore || action == PartyAction::combat)
        {
            if (!campaign_->selected())
                throw std::runtime_error("Add a party member first");
            if (action == PartyAction::explore)
            {
                auto *town = Object::cast_to<RolfTourView>(get_node_or_null("CampaignTown"));
                if (!town)
                {
                    auto owned = presentation::instantiate_scene("res://scenes/rolf_tour.tscn");
                    town = Object::cast_to<RolfTourView>(owned.get());
                    if (!town)
                        throw std::runtime_error("Invalid exploration scene");
                    town->set_name("CampaignTown");
                    town->campaign_party(campaign_);
                    if (OS::get_singleton()->get_cmdline_user_args().has("--save-check-write"))
                        town->enable_save_check([this](const auto & name)
                    {
                        save_checkpoint_check(name);
                    });
                    town->connect("save_requested",
                                  presentation::guarded(this, &CharacterCreationView::open_saves));
                    auto *sheet = &required_node<TownSheetDialog>(*this, "TownSheet");
                    town->connect("party_member_selected",
                                  presentation::guarded(sheet, &TownSheetDialog::show_member));
                    town->connect("level_up_requested",
                                  presentation::guarded(this, &CharacterCreationView::open_advancement));
                    presentation::attach_child(*this, std::move(owned));
                }
                town->show();
                town->set_process(true);
                town->set_process_input(true);
                town->resume_party();
            }
            else
            {
                presentation::attach_child(*this, combat_scene(campaign_, *art_, *body_catalog_));
            }
            required_node<Control>(*this, "PartyPanel").hide();
            auto *back = &required_node<Button>(*this, "ReturnParty");
            move_child(back, get_child_count() - 1);
            back->show();
            party_layout();
            return;
        }
        if (action == PartyAction::return_to_party)
        {
            if (auto *combat = Object::cast_to<CombatView>(get_node_or_null("CampaignCombat")))
            {
                if (!combat->can_leave())
                    throw std::runtime_error("Finish the fight before returning to the party");
                presentation::detach_child(*this, *combat).reset();
            }
            if (auto *town = Object::cast_to<RolfTourView>(get_node_or_null("CampaignTown")))
            {
                if (town->is_visible() && !town->can_leave())
                    throw std::runtime_error("Finish the dialogue or leave the shop first");
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
            required_node<Label>(*this, "PartyPanel/Status").set_text(equipment_notice);
    }
    catch (const std::exception &e)
    {
        error_ = i18n::text(e.what());
        required_node<Label>(*this, "Status").set_text(error_);
        required_node<Label>(*this, "PartyPanel/Status").set_text(error_);
        required_node<Button>(*this, "ReturnParty").set_tooltip_text(error_);
    }
}

void CharacterCreationView::party_check()
{
    const auto press = [&](const char *node)
    {
        required_node<Button>(*this, node).emit_signal("pressed");
        if (!error_.is_empty())
            throw std::runtime_error(error_.utf8().get_data());
    };
    auto *pool = &required_node<CharacterPoolDialog>(*this, "PoolModal");
    if (pool_check_stage_ < 3)
    {
        if (pool_check_stage_ == 0)
        {
            show_pool();
            if (pool->characters().size() != 48)
                throw std::runtime_error("Pool must contain four characters per class");
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
                    throw std::runtime_error("Invalid pool ability range");
                if (!s.training.complete)
                    throw std::runtime_error("Preset training must be complete");
                art_->validate(c.appearance());
                pool->select(i);
            }
            if (names.size() != 48 || classes.size() != 12 ||
                    std::any_of(classes.begin(), classes.end(),
                                [](const auto & c)
        {
            return c.second != 4;
        }))
            throw std::runtime_error("Pool class counts or names invalid");
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
                    image->save_png(ProjectSettings::get_singleton()->globalize_path(
                                        "user://checks/character-pool.png"));
            }
            const auto state = campaign_->checkpoint();
            pool->add();
            pool->add();
            if (campaign_->state().roster.size() != state.roster.size() + 1)
                throw std::runtime_error("Pool add or duplicate guard failed");
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
            creator_->assign_roll(i, static_cast<rules::Ability>(i));
        for (unsigned attempt = 0;
                !rules::class_eligible(creator_->rules(), creator_->draft(), "fighter");
                ++attempt)
        {
            if (attempt == 100)
                throw std::runtime_error("Could not roll qualified party-check fixture");
            creator_->roll();
            for (unsigned i = 0; i < 6; ++i)
                creator_->assign_roll(i, static_cast<rules::Ability>(i));
        }
        while (creator_->step() != CreationStep::sheet)
        {
            const auto before = creator_->step();
            if (before == CreationStep::training)
            {
                required_node<CheckBox>(*this, "Training/Rows/Group1/athletics").set_pressed(true);
                required_node<CheckBox>(*this, "Training/Rows/Group1/history").set_pressed(true);
                for (const char *mastery :
                        {"longsword", "handaxe", "javelin"
                        })
                    required_node<CheckBox>(*this, String("Training/Rows/Group2/") + mastery)
                    .set_pressed(true);
                required_node<OptionButton>(*this, "Training/Rows/Group0/Choice").select(2);
                required_node<OptionButton>(*this, "Training/Rows/Group0/Choice")
                .emit_signal("item_selected", 2);
            }
            next();
            if (creator_->step() == before)
                throw std::runtime_error("Party-check creation did not advance");
        }
        press("PortraitNext");
        const auto chosen = completed_->appearance();
        press("AddParty");
        if (campaign_->state().slots[0] == 0)
            throw std::runtime_error("Add party callback failed");
        if (!required_node<Button>(*this, "PortraitNext").is_disabled() ||
                !required_node<Button>(*this, "PortraitSelect").is_disabled())
            throw std::runtime_error("Added portrait controls remained enabled");
        portrait_part(1);
        if (completed_->appearance() != chosen ||
                campaign_->member(campaign_->state().slots[0]).character.appearance() != chosen)
            throw std::runtime_error("Party portrait changed after adding");
        press("PartyPanel/Recruit");
        if (!campaign_->state().slots[6])
            throw std::runtime_error("Recruit callback failed");
        press("PartyPanel/Remove");
        press("PartyPanel/Rejoin");
        party_selected(0);
        capture("party-roster.png");
        ++party_check_stage_;
        break;
    }
    case 1:
        press("PartyPanel/Explore");
        ++party_check_stage_;
        break;
    case 2:
        if (!required_node<RolfTourView>(*this, "CampaignTown").party_route_checked())
            return;
        press("ReturnParty");
        party_selected(0);
        if (campaign_->member(campaign_->selected()).character.inventory().items().size() != 1)
            throw std::runtime_error("Town purchase missing from party inventory");
        required_node<ItemList>(*this, "PartyPanel/Inventory").select(0);
        press("PartyPanel/Equip");
        ++party_check_stage_;
        break;
    case 3:
        if (!required_node<RichTextLabel>(*this, "PartyPanel/Sheet")
                .get_text()
                .contains("Saving throw"))
            throw std::runtime_error("Party selection did not display the character sheet");
        press("PartyPanel/Modifiers");
        if (!required_node<RichTextLabel>(*this, "ModifiersModal/Text")
                .get_text()
                .contains("Shield: +2 AC"))
            throw std::runtime_error("Party modifiers omitted equipped shield");
        if (!required_node<RichTextLabel>(*this, "ModifiersModal/Text")
                .get_text()
                .contains(OS::get_singleton()->get_cmdline_user_args().has("--adrenaline-check")
                          ? "Adrenaline Rush"
                          : "Resistance to Poison damage"))
            throw std::runtime_error("Created Dwarf is missing its sourced damage resistance");
        press("ModifiersModal/Close");
        press("PartyPanel/SavingThrows");
        if (!required_node<Label>(*this, "SavingThrowsModal/Title")
                .get_text()
                .contains(
                    gs(campaign_->state().roster.at(roster_index_).character.sheet().name)) ||
                !required_node<RichTextLabel>(*this, "SavingThrowsModal/Text")
                .get_text()
                .contains("saving throw proficiency"))
            throw std::runtime_error("Party saving throws did not use selected character");
        press("SavingThrowsModal/Close");
        capture("party-equipped.png");
        press("PartyPanel/Combat");
        ++party_check_stage_;
        break;
    case 4:
    {
        auto *fight = &required_node<CombatView>(*this, "CampaignCombat");
        if (!fight->can_leave())
            return;
        press("ReturnParty");
        capture("party-after-combat.png");
        if (campaign_->in_combat() ||
                campaign_->member(campaign_->state().slots[0]).vitals.resources.empty())
            throw std::runtime_error("Combat state was not returned");
        const auto id = campaign_->state().slots[0];
        if (campaign_->member(id).character.sheet().level != 1 || !campaign_->can_advance(id))
            throw std::runtime_error("XP must wait for explicit level-up confirmation");
        refresh_advancement_arrows();
        const auto path = "PartyPanel/Roster/Advance" + std::to_string(id);
        press(path.c_str());
        if (!required_node<Window>(*this, "LevelUp").is_visible())
            throw std::runtime_error("Level-up arrow did not open choices");
        press("LevelUp/Cancel");
        if (campaign_->member(id).character.sheet().level != 1)
            throw std::runtime_error("Cancel applied advancement");
        press(path.c_str());
        press("LevelUp/Confirm");
        const auto slots = campaign_->state().slots;
        for (const auto other : slots)
            if (other && other != id && campaign_->can_advance(other))
            {
                open_advancement(other);
                press("LevelUp/Confirm");
            }
        const auto sheet = required_node<RichTextLabel>(*this, "PartyPanel/Sheet").get_text();
        if (!sheet.contains("Level 2") || !sheet.contains("XP 300"))
            throw std::runtime_error("Victory advancement is missing from character sheet");
        if (OS::get_singleton()->get_cmdline_user_args().has("--save-check-write"))
            save_checkpoint_check("advancement");
        ++party_check_stage_;
        break;
    }
    case 5:
        press("PartyPanel/Explore");
        required_node<RolfTourView>(*this, "CampaignTown").start_recovery_check();
        ++party_check_stage_;
        break;
    case 6:
        if (!required_node<RolfTourView>(*this, "CampaignTown").recovery_checked())
            return;
        press("ReturnParty");
        ++party_check_stage_;
        break;
    case 7:
        capture("party-recovered.png");
        press("PartyPanel/Combat");
        ++party_check_stage_;
        break;
    case 8:
        if (!required_node<CombatView>(*this, "CampaignCombat").can_leave())
            return;
        press("ReturnParty");
        if (campaign_->state().roster.at(0).experience != 300)
            throw std::runtime_error("Reopening party combat duplicated XP");
        if (OS::get_singleton()->get_cmdline_user_args().has("--save-check-write"))
            save_checkpoint_check("final");
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
            owned = combat_scene(campaign_, *art_, *body_catalog_, town->pending_encounter());
        }
        catch (const std::exception &e)
        {
            if (!town->reject_combat(e.what()))
                throw;
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
        if (fight->expedition() && !campaign_defeated_)
            if (const auto outcome = fight->completed_outcome())
            {
                if (!town || !town->resolve_combat(*outcome))
                    throw std::runtime_error("Exploration rejected the combat result");
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
        if (fight)
        {
            allowed = fight->can_leave();
            if (fight->defeated() && !campaign_defeated_)
                show_defeat();
        }
    }
    if (auto *town = Object::cast_to<RolfTourView>(get_node_or_null("CampaignTown")))
        if (town->is_visible())
            allowed = town->can_leave();
    if (campaign_defeated_)
        allowed = false;
    auto *button = &required_node<Button>(*this, "ReturnParty");
    button->set_disabled(!allowed);
    button->set_tooltip_text(i18n::text(
                                 allowed ? N_("Inspect your party and equipment.")
                                 : N_("Finish combat, dialogue or shopping before returning to the party.")));
}

void CharacterCreationView::expedition_check()
{
    if (++expedition_frames_ > 20000)
        throw std::runtime_error("Expedition check timed out");
    if (!expedition_started_)
    {
        for (unsigned i = 0; i < 6; ++i)
        {
            auto character = preview_guard();
            const auto id = campaign_->add_pc(std::move(character));
            campaign_->set_wealth(id, {0, 0, 0, 500, 0, 0, 0});
            for (unsigned type :
                    {
                        36, 55, 59
                    })
            {
                por::Equipment gear;
                gear.stored.type = type;
                gear.stored.stack_size = 1;
                gear.stored.value = 1;
                campaign_->purchase(id, gear);
                campaign_->equip(id, campaign_->member(id).character.inventory().items().back().id);
            }
        }
        campaign_->award_experience(2700, "fixture:experienced-party");
        const auto slots = campaign_->state().slots;
        for (const auto id : slots)
            if (id)
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
        expedition_started_ = true;
        party_action(PartyAction::explore);
        return;
    }
    if (campaign_defeated_)
        throw std::runtime_error("Expedition fixture was defeated");
    if (get_node_or_null("CampaignCombat"))
        return;
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
            throw std::runtime_error("Slums district save round trip differs");
        town->restore_campaign(campaign_, std::move(*loaded.town));
        expedition_saved_ = true;
        return;
    }
    if (!town->check_expedition_step())
        return;
    const auto *session = town->saved_session();
    if (!session || session->script_variable(por::ecl_slums_orc_victory) != 255)
        throw std::runtime_error("Four-orc victory flag missing");
    const auto &rewards = campaign_->state().claimed_rewards;
    if (std::count(rewards.begin(), rewards.end(), "por:ECL2:20:search1:orcs:v1") != 1)
        throw std::runtime_error("Four-orc XP reward missing or repeated");
    UtilityFunctions::print(
        "Godot expedition passed: gate, roaming encounter, original arena, four-orc victory, automatic exploration return and town gate.");
    expedition_check_ = false;
    get_tree()->quit(0);
}

void CharacterCreationView::setup_defeat()
{
    auto window = presentation::make_node<Window>();
    window->set_name("Defeat");
    window->set_title(i18n::text(N_("Defeat")));
    presentation::set_dialog_window_size(*window, Vector2i(520, 240));
    window->set_flag(Window::FLAG_RESIZE_DISABLED, true);
    window->set_transient(true);
    window->set_exclusive(true);
    window->hide();
    presentation::attach_child(*this, std::move(window));
    auto *dialog = &required_node<Window>(*this, "Defeat");
    auto title = presentation::make_node<Label>();
    title->set_name("Title");
    title->set_text(i18n::text(N_("Your party has been defeated.")));
    const auto title_rect = presentation::dialog_layout_rect(*dialog, "Title", Rect2(24, 30, 472, 44));
    title->set_position(title_rect.position);
    title->set_size(title_rect.size);
    title->set_theme_type_variation("PartyPreviewTitle");
    presentation::attach_child(*dialog, std::move(title));
    auto body = presentation::make_node<Label>();
    body->set_name("Body");
    body->set_text(i18n::text(N_("Load a saved game to continue.")));
    const auto body_rect = presentation::dialog_layout_rect(*dialog, "Body", Rect2(24, 90, 472, 36));
    body->set_position(body_rect.position);
    body->set_size(body_rect.size);
    presentation::attach_child(*dialog, std::move(body));
    for (bool reload :
            {
                true, false
            })
    {
        auto button = presentation::make_node<Button>();
        button->set_name(reload ? "Reload" : "Exit");
        button->set_text(i18n::text(reload ? N_("Reload a Saved Game") : N_("Exit to OS")));
        const auto button_rect = presentation::dialog_layout_rect(
                                     *dialog, reload ? "Reload" : "Exit",
                                     Rect2(reload ? 24 : 308, 170, reload ? 268 : 188, 44));
        button->set_position(button_rect.position);
        button->set_size(button_rect.size);
        button->connect("pressed",
                        reload ? presentation::guarded(this, &CharacterCreationView::reload_after_defeat)
                        : presentation::guarded(this, &CharacterCreationView::exit_after_defeat));
        presentation::attach_child(*dialog, std::move(button));
    }
    dialog->connect("close_requested", presentation::guarded(this, &CharacterCreationView::show_defeat));
    required_node<SaveSlots>(*this, "SaveSlots")
    .connect("visibility_changed",
              presentation::guarded(this, &CharacterCreationView::save_dialog_visibility_changed));
    defeat_check_ = OS::get_singleton()->get_cmdline_user_args().has("--defeat-check");
}

void CharacterCreationView::show_defeat()
{
    campaign_defeated_ = true;
    required_node<Button>(*this, "ReturnParty").hide();
    if (auto *fight = Object::cast_to<CombatView>(get_node_or_null("CampaignCombat")))
        fight->set_process_input(false);
    auto *dialog = &required_node<Window>(*this, "Defeat");
    if (!dialog->is_visible())
        dialog->popup_centered();
    required_node<Button>(*dialog, "Reload").grab_focus();
}

void CharacterCreationView::reload_after_defeat()
{
    if (!campaign_defeated_)
        return;
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
        show_defeat();
}

void CharacterCreationView::exit_after_defeat()
{
    if (campaign_defeated_)
        get_tree()->quit(0);
}

void CharacterCreationView::defeat_check()
{
    auto *dialog = &required_node<Window>(*this, "Defeat");
    auto *saves = &required_node<SaveSlots>(*this, "SaveSlots");
    if (++defeat_check_frames_ > 4000)
        throw std::runtime_error("Defeat check timed out");
    if (defeat_check_stage_ == 0)
    {
        const auto id = campaign_->add_pc(preview_guard());
        campaign_->set_wealth(id, {0, 0, 0, 250, 0, 0, 0});
        open_saves(true);
        required_node<LineEdit>(*saves, "Name").set_text(i18n::text(N_("Defeat test")));
        required_node<Button>(*saves, "Action").emit_signal("pressed");
        if (saves->is_visible())
            required_node<Button>(*saves, "Action").emit_signal("pressed");
        if (saves->is_visible())
            throw std::runtime_error("Defeat fixture save failed");
        party_action(PartyAction::combat);
        ++defeat_check_stage_;
        return;
    }
    if (defeat_check_stage_ == 1)
    {
        if (!campaign_defeated_)
            return;
        auto *fight = &required_node<CombatView>(*this, "CampaignCombat");
        if (!dialog->is_visible() || fight->can_leave() || campaign_->in_combat() ||
                campaign_->state().roster.at(0).vitals.hit_points)
            throw std::runtime_error("Defeat did not lock gameplay with persisted zero HP");
        party_action(PartyAction::return_to_party);
        if (!get_node_or_null("CampaignCombat") || !dialog->is_visible())
            throw std::runtime_error("Return to party bypassed defeat");
        required_node<Button>(*dialog, "Reload").emit_signal("pressed");
        if (!saves->is_visible() || dialog->is_visible())
            throw std::runtime_error("Defeat reload did not open save slots");
        required_node<Button>(*saves, "Cancel").emit_signal("pressed");
        ++defeat_check_stage_;
        return;
    }
    if (defeat_check_stage_ == 2)
    {
        if (!dialog->is_visible())
            throw std::runtime_error("Cancel bypassed defeat");
        auto *fight = &required_node<CombatView>(*this, "CampaignCombat");
        const auto broken = presentation::path_from_godot(
                                ProjectSettings::get_singleton()
                                ->globalize_path("user://checks/save-check/defeat-corrupt.ogs"));
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
            throw std::runtime_error("Rejected defeat load changed campaign");
        ++defeat_check_stage_;
        defeat_check_frames_ = 0;
        return;
    }
    if (defeat_check_stage_ == 3)
    {
        if (defeat_check_frames_ < 4)
            return;
        if (capture_)
        {
            const auto image = dialog->get_texture()->get_image();
            if (image.is_null() || image->save_png(ProjectSettings::get_singleton()->globalize_path(
                    "user://checks/party-defeat.png")) != OK)
                throw std::runtime_error("Cannot capture defeat screen");
        }
        required_node<Button>(*dialog, "Reload").emit_signal("pressed");
        auto *slots = &required_node<ItemList>(*saves, "Slots");
        int selected = -1;
        for (int i = 0; i < slots->get_item_count(); ++i)
            if (slots->get_item_text(i) == "Defeat test")
                selected = i;
        if (selected < 0)
            throw std::runtime_error("Defeat save slot missing");
        slots->select(selected);
        slots->emit_signal("item_selected", selected);
        required_node<Button>(*saves, "Action").emit_signal("pressed");
        if (!campaign_defeated_ || !saves->is_visible())
            throw std::runtime_error("Defeat load skipped confirmation");
        required_node<Button>(*saves, "Action").emit_signal("pressed");
        if (campaign_defeated_ || dialog->is_visible() || saves->is_visible() ||
                get_node_or_null("CampaignCombat") ||
                campaign_->state().roster.at(0).vitals.hit_points == 0)
            throw std::runtime_error("Confirmed reload did not replace defeated campaign");
        party_action(PartyAction::combat);
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
