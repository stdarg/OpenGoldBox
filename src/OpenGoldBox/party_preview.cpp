#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/check_box.hpp>
#include "godot_images.h"
#include "godot_nodes.h"
#include "localization.h"
#include "grip_control.h"
#include "game_resources.h"
#include "character_creation_view.h"
#include "combat_view.h"
#include "rolf_tour_view.h"
#include "save_slots.h"
#include "opengold/campaign_save.h"
#include "opengold/combat_body_catalog.h"
#include "opengold/srd5.h"
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

namespace
{
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
        images.push_back({participant.id, resolved.icon(art, false), resolved.icon(art, true),
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
        std::filesystem::u8path(game_combat_body_file().utf8().get_data()),
        std::filesystem::u8path(game_combat_weapon_file().utf8().get_data()));
    const auto pack = std::filesystem::u8path(game_rules_file().utf8().get_data());
    campaign_ = std::make_shared<CampaignParty>(srd5::load(pack));
    auto panel = presentation::instantiate_scene("res://scenes/party_panel.tscn");
    i18n::prepare_ui(*panel);
    presentation::attach_child(*this, std::move(panel));
    get_node<Control>("PartyPanel")->hide();
    get_node<OptionButton>("PartyPanel/Grip")
        ->connect("item_selected", callable_mp(this, &CharacterCreationView::party_grip_selected));
    get_node<Button>("Party")->connect(
        "pressed", callable_mp(this, &CharacterCreationView::party_action).bind(0));
    get_node<Button>("AddParty")
        ->connect("pressed", callable_mp(this, &CharacterCreationView::party_action).bind(1));
    get_node<Button>("ReturnParty")
        ->connect("pressed", callable_mp(this, &CharacterCreationView::party_action).bind(9));
    const std::array<const char *, 9> buttons{"Create",  "Remove",  "Rejoin", "Recruit", "Equip",
                                              "Unequip", "Explore", "Combat", "Close"};
    const std::array<int, 9> actions{2, 3, 4, 5, 6, 10, 7, 8, 11};
    for (unsigned i = 0; i < buttons.size(); ++i)
        get_node<Button>(gs(std::string("PartyPanel/") + buttons[i]))
            ->connect("pressed",
                      callable_mp(this, &CharacterCreationView::party_action).bind(actions[i]));
    get_node<ItemList>("PartyPanel/Roster")
        ->connect("item_selected", callable_mp(this, &CharacterCreationView::party_selected));
    get_node<Button>("PartyPanel/Pool")
        ->connect("pressed", callable_mp(this, &CharacterCreationView::show_pool));
    get_node<ItemList>("PoolModal/List")
        ->connect("item_selected", callable_mp(this, &CharacterCreationView::pool_selected));
    get_node<Button>("PoolModal/Add")
        ->connect("pressed", callable_mp(this, &CharacterCreationView::pool_add));
    get_node<Button>("PoolModal/Close")
        ->connect("pressed", callable_mp(this, &CharacterCreationView::close_pool));
    get_node<Window>("PoolModal")
        ->connect("close_requested", callable_mp(this, &CharacterCreationView::close_pool));
    get_node<Button>("TownSheet/Close")
        ->connect("pressed", callable_mp(this, &CharacterCreationView::close_town_sheet));
    get_node<Window>("TownSheet")
        ->connect("close_requested", callable_mp(this, &CharacterCreationView::close_town_sheet));
    get_node<Button>("PartyPanel/Modifiers")
        ->connect("pressed", callable_mp(this, &CharacterCreationView::show_modifiers));
    get_node<Button>("PartyPanel/SavingThrows")
        ->connect("pressed", callable_mp(this, &CharacterCreationView::show_saving_throws));
    get_node<RichTextLabel>("PartyPanel/Sheet")->set_use_bbcode(true);
    setup_saves();
    setup_defeat();
    setup_advancement();
    setup_training_review();
    setup_spellbook();
    party_check_ = OS::get_singleton()->get_cmdline_user_args().has("--party-check");
    party_layout();
    equipment_art_check_ =
        OS::get_singleton()->get_cmdline_user_args().has("--equipment-art-check");
    expedition_check_ = OS::get_singleton()->get_cmdline_user_args().has("--expedition-check");
}

void CharacterCreationView::party_layout()
{
    const auto w = get_size().x, h = get_size().y;
    get_node<Control>("PartyPanel")->set_size(get_size());
    const auto place = [&](const char *name, Rect2 rect)
    {
        auto *node = get_node<Control>(name);
        node->set_position(rect.position);
        node->set_size(rect.size);
    };
    place("Party", Rect2(24, h - 158, 166, 36));
    place("AddParty", Rect2(218 + page_rect_.size.x - 190, h - 60, 190, 38));
    place("ReturnParty", Rect2(w - 218, 20, 190, 36));
    place("PartyPanel/Title", Rect2(24, 22, w - 48, 40));
    place("PartyPanel/Roster", Rect2(24, 90, 300, h - 300));
    place("PartyPanel/Sheet", Rect2(350, 90, w - 650, h - 380));
    place("PartyPanel/Portrait", Rect2(w - 284, 90, 264, 264));
    place("PartyPanel/ReadySprite", Rect2(w - 284, 364, 120, 120));
    place("PartyPanel/ActionSprite", Rect2(w - 140, 364, 120, 120));
    place("PartyPanel/ReadyLabel", Rect2(w - 284, 488, 120, 24));
    place("PartyPanel/ActionLabel", Rect2(w - 140, 488, 120, 24));
    place("PartyPanel/Inventory", Rect2(350, h - 280, w - 374, 96));
    place("PartyPanel/GripLabel", Rect2(350, h - 176, 64, 36));
    place("PartyPanel/Grip", Rect2(420, h - 176, 180, 36));
    place("PartyPanel/ReviewTraining", Rect2(610, h - 176, w - 844, 36));
    place("PartyPanel/Spellbook", Rect2(w - 224, h - 176, 200, 36));
    const std::array<const char *, 9> buttons{"Create",  "Remove",  "Rejoin", "Recruit", "Equip",
                                              "Unequip", "Explore", "Combat", "Close"};
    const double bw = (w - 64) / 5;
    for (unsigned i = 0; i < buttons.size(); ++i)
        place((std::string("PartyPanel/") + buttons[i]).c_str(),
              Rect2(24 + (i % 5) * (bw + 4), h - 125 + (i / 5) * 44, bw, 36));
    place("PartyPanel/Save", Rect2(w - 520, 24, 140, 36));
    place("PartyPanel/Load", Rect2(w - 370, 24, 140, 36));
    place("PartyPanel/Pool", Rect2(w - 220, 24, 196, 36));
    if (auto *review = Object::cast_to<Window>(get_node_or_null("TrainingReview"));
        review && review->is_visible())
        review->popup_centered();
    pool_layout();
    place("PartyPanel/Status", Rect2(24, h - 39, w - 48, 32));
    place("PartyPanel/Modifiers", Rect2(24 + 4 * (bw + 4), h - 81, bw * 0.42f, 36));
    place("PartyPanel/SavingThrows",
          Rect2(28 + 4 * (bw + 4) + bw * 0.42f, h - 81, bw * 0.58f - 4, 36));
    for (const auto *name : {"CampaignTown", "CampaignCombat"})
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
            campaign_->select(slot);
    refresh_party();
}

void CharacterCreationView::party_grip_selected(std::int64_t index)
{
    const auto selection = get_node<ItemList>("PartyPanel/Inventory")->get_selected_items();
    try
    {
        auto *grip = get_node<OptionButton>("PartyPanel/Grip");
        if (index < 0 || index >= grip->get_item_count())
            return;
        campaign_->set_grip(campaign_->state().roster.at(roster_index_).id,
                            grip->get_item_id(index));
        error_ = String();
    }
    catch (const std::exception &e)
    {
        error_ = i18n::text(e.what());
    }
    refresh_party();
    if (!selection.is_empty())
        get_node<ItemList>("PartyPanel/Inventory")->select(selection[0]);
}

void CharacterCreationView::refresh_party()
{
    auto *list = get_node<ItemList>("PartyPanel/Roster");
    list->clear();
    const auto &state = campaign_->state();
    for (const auto &m : state.roster)
    {
        auto slot = std::find(state.slots.begin(), state.slots.end(), m.id);
        list->add_item(gs(m.character.sheet().name) +
                       (slot == state.slots.end() ? i18n::text(" (Reserve)") : String()));
    }
    auto *items = get_node<ItemList>("PartyPanel/Inventory");
    items->clear();
    std::string sheet = i18n::utf8(
        "Create a character, finish its sheet, then Add to party.\n\nSix PC positions and two NPC positions. Removed members remain in the roster.");
    presentation::refresh_grip(*get_node<OptionButton>("PartyPanel/Grip"), {}, {});
    if (!state.roster.empty())
    {
        roster_index_ = std::min(roster_index_, state.roster.size() - 1);
        list->select(roster_index_);
        const auto &m = state.roster[roster_index_];
        sheet = sheet_text(m.character, &m).utf8().get_data();
        const auto profile = campaign_->profile(m.id);
        presentation::refresh_grip(*get_node<OptionButton>("PartyPanel/Grip"), profile.equipment,
                                   profile.grips);
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
        get_node<TextureRect>("PartyPanel/Portrait")
            ->set_texture(portrait_texture(m.character.appearance(), m.character.creation_data()));
        const auto resolved = por::resolve_combat_appearance(m, *body_catalog_);
        for (unsigned pose = 0; pose < 2; ++pose)
        {
            const auto icon = resolved.icon(*art_, pose != 0);
            get_node<TextureRect>(pose ? "PartyPanel/ActionSprite" : "PartyPanel/ReadySprite")
                ->set_texture(presentation::image_texture(icon));
        }
    }
    if (state.roster.empty())
        for (const char *name :
             {"PartyPanel/Portrait", "PartyPanel/ReadySprite", "PartyPanel/ActionSprite"})
            get_node<TextureRect>(name)->set_texture({});
    auto *spellbook = get_node<Button>("PartyPanel/Spellbook");
    spellbook->set_visible(!state.roster.empty() &&
                           campaign_->rule_module()
                                   .spell_access(state.roster[roster_index_].character.sheet())
                                   .spellbook_choices > 0);
    spellbook->set_disabled(campaign_->in_combat() || state.rest_activity.has_value() ||
                            state.short_rest.has_value() || state.spell_rest.has_value() ||
                            state.training_rest.has_value());
    auto *review = get_node<Button>("PartyPanel/ReviewTraining");
    review->set_visible(!state.roster.empty() &&
                        !state.roster[roster_index_].character.sheet().training.complete);
    review->set_disabled(campaign_->in_combat());
    get_node<RichTextLabel>("PartyPanel/Sheet")->set_text(gs(sheet));
    get_node<Label>("PartyPanel/Status")
        ->set_text(
            error_.is_empty()
                ? i18n::text("New PCs receive 250 gp / Save game stores this campaign on disk.")
                : error_);
    for (const char *name :
         {"Remove", "Rejoin", "Equip", "Unequip", "Explore", "Combat", "Modifiers", "SavingThrows"})
        get_node<Button>(gs(std::string("PartyPanel/") + name))->set_disabled(state.roster.empty());
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
        get_node<Button>(path)->emit_signal("pressed");
        if (!error_.is_empty())
            throw std::runtime_error(error_.utf8().get_data());
    };
    const auto gear = [&](unsigned index, bool equip)
    {
        get_node<ItemList>("PartyPanel/Inventory")->select(index);
        press(equip ? "PartyPanel/Equip" : "PartyPanel/Unequip");
        if (auto *choice = Object::cast_to<Window>(get_node_or_null("EquipmentChoice"));
            choice && choice->is_visible())
        {
            auto *hand = choice->get_node<OptionButton>("Hand");
            hand->select(0);
            hand->emit_signal("item_selected", 0);
            press("EquipmentChoice/Equip");
        }
    };
    const auto expected = [&](unsigned member, unsigned body, bool action)
    {
        const auto a = campaign_->state().roster.at(member).character.appearance();
        return presentation::rgba_image(art_->equipped_icon(a, body, action))->get_data();
    };
    const auto verify_preview = [&](unsigned member, unsigned body)
    {
        for (bool action : {false, true})
        {
            const auto texture =
                get_node<TextureRect>(action ? "PartyPanel/ActionSprite" : "PartyPanel/ReadySprite")
                    ->get_texture();
            require(texture.is_valid() &&
                        texture->get_image()->get_data() == expected(member, body, action),
                    "Party preview differs from equipped body in ready/action pose");
        }
        require(campaign_->state().roster.at(member).character.appearance().combat_body == 24,
                "Equipment display overwrote the saved base body");
    };
    const auto verify_combat = [&]
    {
        auto *combat = get_node<CombatView>("CampaignCombat");
        for (unsigned member = 0; member < 2; ++member)
            for (bool action : {false, true})
            {
                const auto texture =
                    combat->sprite_texture(campaign_->state().roster.at(member).id, action);
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
            a.portrait = recommended_portrait(draft);
            Character character(*srd5::character_rules(), draft, a);
            const auto id = member
                                ? campaign_->recruit("check:equipment-guard", std::move(character))
                                : campaign_->add_pc(std::move(character));
            for (const unsigned type : {36u, 8u, 59u, 55u})
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
        const std::array<unsigned, 8> bodies{
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
            auto *grip = get_node<OptionButton>("PartyPanel/Grip");
            const auto id = campaign_->state().roster.at(member).id;
            require(grip->get_item_count() == 2 && grip->get_selected_id() == 1,
                    "Party inventory offers both longsword grips");
            grip->emit_signal("item_selected", 1);
            require(campaign_->profile(id).equipment.weapon_hands == 2 &&
                        grip->get_selected_id() == 2,
                    "Party dropdown applies two hands");
            grip->emit_signal("item_selected", 0);
            require(campaign_->profile(id).equipment.weapon_hands == 1,
                    "Party dropdown applies one hand");
            gear(2, true);
            require(grip->is_item_disabled(1),
                    "Equipped shield disables two hands in party inventory");
            const auto profile = campaign_->profile(id).data;
            grip->emit_signal("item_selected", 1);
            require(campaign_->profile(id).data == profile && grip->get_selected_id() == 1,
                    "Rejected party grip restores displayed selection");
            error_ = String();
            refresh_party();
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

        struct CheckSave
        {
            std::filesystem::path path;

            ~CheckSave()
            {
                std::error_code ignored;
                std::filesystem::remove(path, ignored);
            }
        } save{std::filesystem::u8path(
            ProjectSettings::get_singleton()
                ->globalize_path("user://checks/equipment-art-" +
                                 String::num_int64(OS::get_singleton()->get_process_id()) + ".ogs")
                .utf8()
                .get_data())};

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
        auto *combat = get_node<CombatView>("CampaignCombat");
        remove_child(combat);
        {
            presentation::NodeOwner<Node> removed(combat);
        } // Release the fixture's combat lock.
        CampaignEncounter encounter;
        encounter.field.geometry = {12, 9, std::vector<std::uint8_t>(108)};
        encounter.enemies.push_back({1000, "bandit", "Artwork fixture", 1, {9, 4}});
        // A distinguishable authored enemy texture must survive party-only resolution.
        opengold::Image enemy;
        enemy.width = enemy.height = 24;
        enemy.rgba.assign(24 * 24 * 4, 255);
        encounter.art.push_back({1000, enemy, enemy});
        presentation::attach_child(
            *this, combat_scene(campaign_, *art_, *body_catalog_, std::move(encounter)));
        verify_combat();
        for (bool action : {false, true})
        {
            const auto texture =
                get_node<CombatView>("CampaignCombat")->sprite_texture(1000, action);
            require(texture.is_valid() && texture->get_image()->get_data() ==
                                              presentation::rgba_image(enemy)->get_data(),
                    "Party equipment changed an encounter creature's texture");
        }
    }
    else if (check_stage_ == 19)
    {
        auto *combat = get_node<CombatView>("CampaignCombat");
        remove_child(combat);
        {
            presentation::NodeOwner<Node> removed(combat);
        }
        auto character = preview_guard();
        auto appearance = character.appearance();
        appearance.combat_body = 24;
        character.appearance(appearance);
        for (const auto &option : body_catalog_->options)
            if (option.original_type)
            {
                por::Equipment item;
                item.stored.type = option.original_type;
                item.stored.stack_size = 1;
                character.inventory().add(equipment_conversion(item), option.label, 1,
                                          option.original_type);
            }
        character.inventory().add("shield", "Shield", 1, 59);
        const auto id = campaign_->add_pc(std::move(character));
        party_selected(campaign_->state().roster.size() - 1);
        // Exercise the shipped party's buttons, not the separate review UI.
        const auto inventory = campaign_->member(id).character.inventory().items();
        for (unsigned index = 0; index + 1 < inventory.size(); ++index)
        {
            gear(index, true);
            require(campaign_->member(id).equipped ==
                        std::vector<std::uint64_t>{inventory[index].id},
                    "Game Equip did not replace the weapon");
            const auto resolved =
                por::resolve_combat_appearance(campaign_->member(id), *body_catalog_);
            require(resolved.appearance == appearance, "Game equipment changed character anatomy");
            for (bool action : {false, true})
                require(get_node<TextureRect>(action ? "PartyPanel/ActionSprite"
                                                     : "PartyPanel/ReadySprite")
                                ->get_texture()
                                ->get_image()
                                ->get_data() ==
                            presentation::rgba_image(resolved.icon(*art_, action))->get_data(),
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

void CharacterCreationView::party_action(int action)
{
    try
    {
        if (campaign_defeated_)
            return;
        error_ = "";
        String equipment_notice;
        const auto id =
            campaign_->state().roster.empty() ? 0 : campaign_->state().roster.at(roster_index_).id;
        if (action == 1)
        {
            if (!completed_ || added_to_party_)
                return;
            const auto added = campaign_->add_pc(*completed_);
            campaign_->set_wealth(added, {0, 0, 0, 250, 0, 0, 0});
            added_to_party_ = true;
            roster_index_ = campaign_->state().roster.size() - 1;
            refresh();
        }
        if (action == 2 || action == 11)
        {
            party_open_ = false;
            get_node<Control>("PartyPanel")->hide();
            if (action == 2)
                restart();
            return;
        }
        if (action == 3)
            campaign_->remove(id);
        if (action == 4)
            campaign_->rejoin(id);
        if (action == 5)
        {
            const auto recruited = campaign_->recruit("preview:guard", preview_guard());
            const auto &roster = campaign_->state().roster;
            roster_index_ = std::find_if(roster.begin(), roster.end(),
                                         [&](const auto &m)
                                         {
                                             return m.id == recruited;
                                         }) -
                            roster.begin();
        }
        if (action == 6 || action == 10)
        {
            const auto selection = get_node<ItemList>("PartyPanel/Inventory")->get_selected_items();
            if (selection.is_empty())
                throw std::runtime_error("Select an inventory item first");
            const auto items = campaign_->member(id).character.inventory().items();
            const auto selected = items[selection[0]].id;
            if (action == 6)
            {
                if (open_equipment_choice(id, selected))
                    return;
                campaign_->equip(id, selected);
                equipment_notice =
                    i18n::text("Equipped.") + " " +
                    i18n::text(srd5::equipment_note(campaign_->member(id).character.sheet(),
                                                    items[selection[0]].definition_id));
            }
            else
                campaign_->unequip(id, selected);
        }
        if (action == 7 || action == 8)
        {
            if (!campaign_->selected())
                throw std::runtime_error("Add a party member first");
            if (action == 7)
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
                        town->save_check = [this](const auto &name)
                        {
                            save_checkpoint_check(name);
                        };
                    town->connect("save_requested",
                                  callable_mp(this, &CharacterCreationView::open_saves));
                    town->connect("party_member_selected",
                                  callable_mp(this, &CharacterCreationView::town_member_selected));
                    town->connect("level_up_requested",
                                  callable_mp(this, &CharacterCreationView::open_advancement));
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
            get_node<Control>("PartyPanel")->hide();
            auto *back = get_node<Button>("ReturnParty");
            move_child(back, get_child_count() - 1);
            back->show();
            party_layout();
            return;
        }
        if (action == 9)
        {
            if (auto *combat = Object::cast_to<CombatView>(get_node_or_null("CampaignCombat")))
            {
                if (!combat->can_leave())
                    throw std::runtime_error("Finish the fight before returning to the party");
                remove_child(combat);
                presentation::NodeOwner<Node> released(combat);
            }
            if (auto *town = Object::cast_to<RolfTourView>(get_node_or_null("CampaignTown")))
            {
                if (town->is_visible() && !town->can_leave())
                    throw std::runtime_error("Finish the dialogue or leave the shop first");
                town->hide();
                town->set_process(false);
                town->set_process_input(false);
            }
            get_node<Button>("ReturnParty")->hide();
        }
        party_open_ = true;
        auto *panel = get_node<Control>("PartyPanel");
        move_child(panel, get_child_count() - 1);
        panel->show();
        refresh_party();
        if (!equipment_notice.is_empty())
            get_node<Label>("PartyPanel/Status")->set_text(equipment_notice);
    }
    catch (const std::exception &e)
    {
        error_ = i18n::text(e.what());
        get_node<Label>("Status")->set_text(error_);
        get_node<Label>("PartyPanel/Status")->set_text(error_);
        get_node<Button>("ReturnParty")->set_tooltip_text(error_);
    }
}

void CharacterCreationView::party_check()
{
    const auto press = [&](const char *node)
    {
        get_node<Button>(node)->emit_signal("pressed");
        if (!error_.is_empty())
            throw std::runtime_error(error_.utf8().get_data());
    };
    if (pool_check_stage_ < 3)
    {
        if (pool_check_stage_ == 0)
        {
            show_pool();
            if (pool_.size() != 48)
                throw std::runtime_error("Pool must contain four characters per class");
        }
        else if (pool_check_stage_ == 1)
        {
            std::map<std::string, unsigned> classes;
            std::set<std::string> names;
            for (unsigned i = 0; i < pool_.size(); ++i)
            {
                const auto &c = pool_[i];
                const auto &s = c.sheet();
                ++classes[s.character_class];
                names.insert(s.name);
                if (s.level != 1 || *std::min_element(s.scores.begin(), s.scores.end()) < 13 ||
                    *std::max_element(s.scores.begin(), s.scores.end()) > 20)
                    throw std::runtime_error("Invalid pool ability range");
                if (!s.training.complete)
                    throw std::runtime_error("Preset training must be complete");
                art_->validate(c.appearance());
                pool_selected(i);
            }
            if (names.size() != 48 || classes.size() != 12 ||
                std::any_of(classes.begin(), classes.end(),
                            [](const auto &c)
                            {
                                return c.second != 4;
                            }))
                throw std::runtime_error("Pool class counts or names invalid");
            pool_selected(19);
            get_node<ItemList>("PoolModal/List")->select(19);
        }
        else
        {
            if (capture_)
            {
                const auto image = get_node<Window>("PoolModal")->get_texture()->get_image();
                if (image.is_valid())
                    image->save_png(ProjectSettings::get_singleton()->globalize_path(
                        "user://checks/character-pool.png"));
            }
            const auto state = campaign_->checkpoint();
            pool_add();
            pool_add();
            if (campaign_->state().roster.size() != state.roster.size() + 1)
                throw std::runtime_error("Pool add or duplicate guard failed");
            campaign_->restore(state);
            pool_added_.clear();
            roster_index_ = 0;
            close_pool();
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
            creator_->assign_roll(i, i);
        for (unsigned attempt = 0; !creator_->rules().class_eligible(creator_->draft(), "fighter");
             ++attempt)
        {
            if (attempt == 100)
                throw std::runtime_error("Could not roll qualified party-check fixture");
            creator_->roll();
            for (unsigned i = 0; i < 6; ++i)
                creator_->assign_roll(i, i);
        }
        while (creator_->step() != CreationStep::sheet)
        {
            const auto before = creator_->step();
            if (before == CreationStep::training)
            {
                get_node<CheckBox>("Training/Rows/Group0/elvish")->set_pressed(true);
                get_node<CheckBox>("Training/Rows/Group0/dwarvish")->set_pressed(true);
                get_node<CheckBox>("Training/Rows/Group2/athletics")->set_pressed(true);
                get_node<CheckBox>("Training/Rows/Group2/history")->set_pressed(true);
                get_node<OptionButton>("Training/Rows/Group1/Choice")->select(2);
                get_node<OptionButton>("Training/Rows/Group1/Choice")
                    ->emit_signal("item_selected", 2);
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
        if (!get_node<Button>("PortraitNext")->is_disabled() ||
            !get_node<Button>("PortraitSelect")->is_disabled())
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
        if (!get_node<RolfTourView>("CampaignTown")->party_route_checked())
            return;
        press("ReturnParty");
        party_selected(0);
        if (campaign_->member(campaign_->selected()).character.inventory().items().size() != 1)
            throw std::runtime_error("Town purchase missing from party inventory");
        get_node<ItemList>("PartyPanel/Inventory")->select(0);
        press("PartyPanel/Equip");
        ++party_check_stage_;
        break;
    case 3:
        if (!get_node<RichTextLabel>("PartyPanel/Sheet")->get_text().contains("Saving throw"))
            throw std::runtime_error("Party selection did not display the character sheet");
        press("PartyPanel/Modifiers");
        if (!get_node<RichTextLabel>("ModifiersModal/Text")->get_text().contains("Shield: +2 AC"))
            throw std::runtime_error("Party modifiers omitted equipped shield");
        if (!get_node<RichTextLabel>("ModifiersModal/Text")
                 ->get_text()
                 .contains(OS::get_singleton()->get_cmdline_user_args().has("--adrenaline-check")
                               ? "Adrenaline Rush"
                               : "Resistance to Poison damage"))
            throw std::runtime_error("Created Dwarf is missing its sourced damage resistance");
        press("ModifiersModal/Close");
        press("PartyPanel/SavingThrows");
        if (!get_node<Label>("SavingThrowsModal/Title")
                 ->get_text()
                 .contains(
                     gs(campaign_->state().roster.at(roster_index_).character.sheet().name)) ||
            !get_node<RichTextLabel>("SavingThrowsModal/Text")
                 ->get_text()
                 .contains("saving throw proficiency"))
            throw std::runtime_error("Party saving throws did not use selected character");
        press("SavingThrowsModal/Close");
        capture("party-equipped.png");
        press("PartyPanel/Combat");
        ++party_check_stage_;
        break;
    case 4:
    {
        auto *fight = get_node<CombatView>("CampaignCombat");
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
        if (!get_node<Window>("LevelUp")->is_visible())
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
        if (!get_node<RichTextLabel>("PartyPanel/Sheet")->get_text().contains("Level 2") ||
            !get_node<RichTextLabel>("PartyPanel/Sheet")->get_text().contains("XP 300"))
            throw std::runtime_error("Victory advancement is missing from character sheet");
        if (OS::get_singleton()->get_cmdline_user_args().has("--save-check-write"))
            save_checkpoint_check("advancement");
        ++party_check_stage_;
        break;
    }
    case 5:
        press("PartyPanel/Explore");
        get_node<RolfTourView>("CampaignTown")->start_recovery_check();
        ++party_check_stage_;
        break;
    case 6:
        if (!get_node<RolfTourView>("CampaignTown")->recovery_checked())
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
        if (!get_node<CombatView>("CampaignCombat")->can_leave())
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
                if (outcome->outcome == rules::Outcome::victory)
                {
                    remove_child(fight);
                    presentation::NodeOwner<Node> released(fight);
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
    auto *button = get_node<Button>("ReturnParty");
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
            for (unsigned type : {36, 55, 59})
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
        party_action(7);
        return;
    }
    if (campaign_defeated_)
        throw std::runtime_error("Expedition fixture was defeated");
    if (get_node_or_null("CampaignCombat"))
        return;
    auto *town = get_node<RolfTourView>("CampaignTown");
    if (const auto *state = town->saved_session();
        state && state->can_leave() && state->snapshot().area_id == 20 && !expedition_saved_)
    {
        const auto pack = std::filesystem::u8path(game_rules_file().utf8().get_data());
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
    if (!session || session->script_variable(0x4ACA) != 255)
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
    window->set_size(Vector2i(520, 240));
    window->set_min_size(Vector2i(520, 240));
    window->set_flag(Window::FLAG_RESIZE_DISABLED, true);
    window->set_transient(true);
    window->set_exclusive(true);
    window->hide();
    presentation::attach_child(*this, std::move(window));
    auto *dialog = get_node<Window>("Defeat");
    auto title = presentation::make_node<Label>();
    title->set_name("Title");
    title->set_text(i18n::text(N_("Your party has been defeated.")));
    title->set_position(Vector2(24, 30));
    title->set_size(Vector2(472, 44));
    title->add_theme_font_size_override("font_size", 24);
    presentation::attach_child(*dialog, std::move(title));
    auto body = presentation::make_node<Label>();
    body->set_text(i18n::text(N_("Load a saved game to continue.")));
    body->set_position(Vector2(24, 90));
    body->set_size(Vector2(472, 36));
    presentation::attach_child(*dialog, std::move(body));
    for (bool reload : {true, false})
    {
        auto button = presentation::make_node<Button>();
        button->set_name(reload ? "Reload" : "Exit");
        button->set_text(i18n::text(reload ? N_("Reload a Saved Game") : N_("Exit to OS")));
        button->set_position(Vector2(reload ? 24 : 308, 170));
        button->set_size(Vector2(reload ? 268 : 188, 44));
        button->connect("pressed",
                        reload ? callable_mp(this, &CharacterCreationView::reload_after_defeat)
                               : callable_mp(this, &CharacterCreationView::exit_after_defeat));
        presentation::attach_child(*dialog, std::move(button));
    }
    dialog->connect("close_requested", callable_mp(this, &CharacterCreationView::show_defeat));
    get_node<SaveSlots>("SaveSlots")
        ->connect("visibility_changed",
                  callable_mp(this, &CharacterCreationView::save_dialog_visibility_changed));
    defeat_check_ = OS::get_singleton()->get_cmdline_user_args().has("--defeat-check");
}

void CharacterCreationView::show_defeat()
{
    campaign_defeated_ = true;
    get_node<Button>("ReturnParty")->hide();
    if (auto *fight = Object::cast_to<CombatView>(get_node_or_null("CampaignCombat")))
        fight->set_process_input(false);
    auto *dialog = get_node<Window>("Defeat");
    if (!dialog->is_visible())
        dialog->popup_centered();
    dialog->get_node<Button>("Reload")->grab_focus();
}

void CharacterCreationView::reload_after_defeat()
{
    if (!campaign_defeated_)
        return;
    get_node<Window>("Defeat")->hide();
    open_saves(false);
}

void CharacterCreationView::save_dialog_visibility_changed()
{
    // Window releases its exclusive-child slot after emitting visibility_changed.
    callable_mp(this, &CharacterCreationView::restore_defeat_dialog).call_deferred();
}

void CharacterCreationView::restore_defeat_dialog()
{
    if (campaign_defeated_ && !get_node<SaveSlots>("SaveSlots")->is_visible())
        show_defeat();
}

void CharacterCreationView::exit_after_defeat()
{
    if (campaign_defeated_)
        get_tree()->quit(0);
}

void CharacterCreationView::defeat_check()
{
    auto *dialog = get_node<Window>("Defeat");
    auto *saves = get_node<SaveSlots>("SaveSlots");
    if (++defeat_check_frames_ > 4000)
        throw std::runtime_error("Defeat check timed out");
    if (defeat_check_stage_ == 0)
    {
        const auto id = campaign_->add_pc(preview_guard());
        campaign_->set_wealth(id, {0, 0, 0, 250, 0, 0, 0});
        open_saves(true);
        saves->get_node<LineEdit>("Name")->set_text(i18n::text(N_("Defeat test")));
        saves->get_node<Button>("Action")->emit_signal("pressed");
        if (saves->is_visible())
            saves->get_node<Button>("Action")->emit_signal("pressed");
        if (saves->is_visible())
            throw std::runtime_error("Defeat fixture save failed");
        party_action(8);
        ++defeat_check_stage_;
        return;
    }
    if (defeat_check_stage_ == 1)
    {
        if (!campaign_defeated_)
            return;
        auto *fight = get_node<CombatView>("CampaignCombat");
        if (!dialog->is_visible() || fight->can_leave() || campaign_->in_combat() ||
            campaign_->state().roster.at(0).vitals.hit_points)
            throw std::runtime_error("Defeat did not lock gameplay with persisted zero HP");
        party_action(9);
        if (!get_node_or_null("CampaignCombat") || !dialog->is_visible())
            throw std::runtime_error("Return to party bypassed defeat");
        dialog->get_node<Button>("Reload")->emit_signal("pressed");
        if (!saves->is_visible() || dialog->is_visible())
            throw std::runtime_error("Defeat reload did not open save slots");
        saves->get_node<Button>("Cancel")->emit_signal("pressed");
        ++defeat_check_stage_;
        return;
    }
    if (defeat_check_stage_ == 2)
    {
        if (!dialog->is_visible())
            throw std::runtime_error("Cancel bypassed defeat");
        auto *fight = get_node<CombatView>("CampaignCombat");
        const auto broken = std::filesystem::u8path(
            ProjectSettings::get_singleton()
                ->globalize_path("user://checks/save-check/defeat-corrupt.ogs")
                .utf8()
                .get_data());
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
        dialog->get_node<Button>("Reload")->emit_signal("pressed");
        auto *slots = saves->get_node<ItemList>("Slots");
        int selected = -1;
        for (int i = 0; i < slots->get_item_count(); ++i)
            if (slots->get_item_text(i) == "Defeat test")
                selected = i;
        if (selected < 0)
            throw std::runtime_error("Defeat save slot missing");
        slots->select(selected);
        slots->emit_signal("item_selected", selected);
        saves->get_node<Button>("Action")->emit_signal("pressed");
        if (!campaign_defeated_ || !saves->is_visible())
            throw std::runtime_error("Defeat load skipped confirmation");
        saves->get_node<Button>("Action")->emit_signal("pressed");
        if (campaign_defeated_ || dialog->is_visible() || saves->is_visible() ||
            get_node_or_null("CampaignCombat") ||
            campaign_->state().roster.at(0).vitals.hit_points == 0)
            throw std::runtime_error("Confirmed reload did not replace defeated campaign");
        party_action(8);
        ++defeat_check_stage_;
        return;
    }
    if (defeat_check_stage_ == 4 && campaign_defeated_)
    {
        UtilityFunctions::print(
            "Godot defeat check passed: real combat loss, blocked return, cancel, corrupt load rollback, confirmed save reload, subsequent defeat and Exit to OS callback.");
        dialog->get_node<Button>("Exit")->emit_signal("pressed");
        defeat_check_ = false;
    }
}
