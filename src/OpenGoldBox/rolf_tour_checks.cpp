// The town view's acceptance hooks: test drivers that walk the live campaign
// through the town, its shops and recovery with the same button signals and
// keys the player uses.
#include "rolf_tour_view.h"
#include "rolf_tour_view_internal.h"
#include "game_lifecycle.h"
#include "godot_nodes.h"
#include "localization.h"
#include "vital_fixtures.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/item_list.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <algorithm>
#include <array>
#include <queue>
#include <stdexcept>

using namespace godot;
using namespace opengold::por;
using opengold::Coin;
using opengold::gold_value;
using presentation::required_node;

// The town check's Cast menu and Identify (#262): the member, made a Bard with
// both spells, casts Detect Magic from the Cast menu, sees a hidden Long Sword
// +2 marked, and identifies it from the inventory. One step a frame, so each
// capture shows the step before it.
void RolfTourView::check_identification_spells()
{
    auto &items = required_node<ItemList>(*this, "InventoryPanel/%Items");
    const auto sword_row = [&]
    {
        const auto carried = campaign_->member(campaign_->selected()).character.inventory().items();
        return static_cast<int>(std::find_if(carried.begin(), carried.end(),
                                             [&](const auto &item)
                                             {
                                                 return item.id == identify_check_sword_;
                                             }) -
                                carried.begin());
    };
    if (shop_check_stage_ == 10)
    {
        identify_check_retained_ = campaign_->checkpoint();
        auto fixture = *identify_check_retained_;
        auto &caster = fixture.roster.at(0);
        auto draft = caster.character.creation_data();
        draft.character_class = "bard";
        draft.background = "sage";
        draft.training.clear();
        draft.training["class:bard"] = {"performance", "persuasion", "insight"};
        draft.cantrips = std::vector<std::string>{"vicious_mockery", "starry_wisp"};
        draft.spells = opengold::rules::SpellChoices{
            {},
            std::vector<std::string>{"identify", "detect_magic", "healing_word",
                                     "dissonant_whispers"},
            {},
            {}};
        auto inventory = caster.character.inventory();
        caster.character = opengold::Character(*campaign_->character_rules(), draft,
                                               caster.character.appearance());
        opengold::por::Equipment sword;
        sword.stored.type = 36;
        sword.stored.magic_bonus = 2;
        sword.stored.name_components = {0, 163, 36};
        sword.stored.hidden_name_words = 6;
        sword.stored.stack_size = 1;
        sword.bonuses.weapon_to_hit = 2;
        identify_check_sword_ =
            inventory.add({.definition_id = opengold::equipment_conversion(sword),
                           .name = sword.label(),
                           .quantity = 1,
                           .original_type = 36});
        opengold::por::Equipment book;
        book.stored.type = 70;
        book.stored.magic_bonus = 12;
        book.stored.effect_codes = {1, 90, 0};
        book.stored.stack_size = 1;
        const auto manual = inventory.add({.definition_id = opengold::equipment_conversion(book),
                                           .name = "Manual of Bodily Health",
                                           .quantity = 1,
                                           .original_type = 70});
        opengold::por::Equipment ring;
        ring.stored.type = 93;
        ring.stored.magic_bonus = 2;
        ring.stored.stack_size = 1;
        ring.decoded_name = "Ring Of Protection +2";
        const auto protection =
            inventory.add({.definition_id = opengold::equipment_conversion(ring),
                           .name = ring.label(),
                           .quantity = 1,
                           .original_type = 93});
        opengold::por::Equipment parchment;
        parchment.stored.type = 61;
        parchment.stored.magic_bonus = 1;
        parchment.stored.effect_codes = {14, 47, 0};
        parchment.stored.stack_size = 1;
        parchment.decoded_name = "Magic User Scroll With 2 Spells";
        const auto scroll =
            inventory.add({.definition_id = opengold::equipment_conversion(parchment),
                           .name = parchment.label(),
                           .quantity = 1,
                           .original_type = 61});
        caster.character.replace_inventory(std::move(inventory));
        caster.item_sources.emplace(identify_check_sword_, sword);
        caster.item_sources.emplace(manual, book);
        caster.item_sources.emplace(protection, ring);
        caster.item_sources.emplace(scroll, parchment);
        caster.equipped.clear();
        caster.vitals = {caster.character.sheet().hit_points, false, {}};
        campaign_->restore(std::move(fixture));
        refresh();
        refresh_inventory();
        if (!required_node<Button>(*this, "InventoryPanel/%CastIdentify").is_visible())
        {
            throw std::runtime_error("A Bard must be offered Identify in the inventory");
        }
        required_node<Button>(*this, "InventoryPanel/%Close").emit_signal("pressed");
        required_node<Button>(*this, "%Cast").emit_signal("pressed");
        auto &action = required_node<OptionButton>(*this, "CastPanel/%Action");
        for (int index = 0; index < action.get_item_count(); ++index)
        {
            if (action.get_item_metadata(index) == Variant(String("detect_magic")))
            {
                action.select(index);
                cast_action_selected(index);
            }
        }
        if (action.get_selected_metadata() != Variant(String("detect_magic")) ||
            required_node<OptionButton>(*this, "CastPanel/%Target").is_visible())
        {
            throw std::runtime_error("The Cast menu must offer Detect Magic for the party");
        }
        shop_check_stage_ = 11;
        return;
    }
    if (shop_check_stage_ == 11)
    {
        capture_frame("phlan-cast");
        required_node<Button>(*this, "CastPanel/%Use").emit_signal("pressed");
        if (!campaign_->detecting_magic() ||
            !required_node<Label>(*this, "CastPanel/%Status").get_text().contains("ten minutes"))
        {
            throw std::runtime_error("Detect Magic from the Cast menu must mark magic items");
        }
        shop_check_stage_ = 12;
        return;
    }
    if (shop_check_stage_ == 12)
    {
        capture_frame("phlan-cast-done");
        required_node<Button>(*this, "CastPanel/%Close").emit_signal("pressed");
        required_node<Button>(*this, "%Inventory").emit_signal("pressed");
        const auto row = sword_row();
        if (!items.get_item_text(row).begins_with("* ") || items.get_item_text(row).contains("+2"))
        {
            throw std::runtime_error("Detect Magic must mark the unidentified sword");
        }
        items.select(row);
        shop_check_stage_ = 13;
        return;
    }
    if (shop_check_stage_ == 13)
    {
        capture_frame("phlan-detect");
        required_node<Button>(*this, "InventoryPanel/%CastIdentify").emit_signal("pressed");
        if (!items.get_item_text(sword_row()).contains("+2") ||
            !required_node<Label>(*this, "InventoryPanel/%Status").get_text().contains("Identify"))
        {
            throw std::runtime_error("Identify from the inventory must reveal the sword");
        }
        shop_check_stage_ = 14;
        return;
    }
    capture_frame("phlan-identify");
    // The inventory's Use reads the Manual of Bodily Health (MAGIC-3).
    const auto carried = campaign_->member(campaign_->selected()).character.inventory().items();
    const auto book = std::find_if(carried.begin(), carried.end(),
                                   [](const auto &item)
                                   {
                                       return item.definition_id == "manual_of_bodily_health";
                                   });
    const int constitution = campaign_->member(campaign_->selected())
                                 .character.sheet()
                                 .scores[opengold::rules::Ability::constitution];
    items.select(static_cast<int>(book - carried.begin()));
    inventory_selected(book - carried.begin());
    auto &use = required_node<Button>(*this, "InventoryPanel/%UseItem");
    if (!use.is_visible())
    {
        throw std::runtime_error("The inventory must offer Use for the Manual");
    }
    use.emit_signal("pressed");
    if (campaign_->member(campaign_->selected())
                .character.sheet()
                .scores[opengold::rules::Ability::constitution] != constitution + 2 ||
        !required_node<Label>(*this, "InventoryPanel/%Status").get_text().contains("Constitution"))
    {
        throw std::runtime_error("Using the Manual must raise Constitution by 2");
    }
    // A Ring of Protection is worn from the inventory (MAGIC-4).
    const auto worn = campaign_->member(campaign_->selected()).character.inventory().items();
    const auto ring = std::find_if(worn.begin(), worn.end(),
                                   [](const auto &item)
                                   {
                                       return item.definition_id == "ring_of_protection+2";
                                   });
    const int armor_class = campaign_->profile(campaign_->selected()).armor_class;
    items.select(static_cast<int>(ring - worn.begin()));
    inventory_selected(ring - worn.begin());
    required_node<Button>(*this, "InventoryPanel/%Equip").emit_signal("pressed");
    if (campaign_->profile(campaign_->selected()).armor_class != armor_class + 2 ||
        campaign_->profile(campaign_->selected()).item_save_bonus != 2 ||
        !required_node<Label>(*this, "InventoryPanel/%Status")
             .get_text()
             .contains("Rings of Protection"))
    {
        throw std::runtime_error("Wearing a Ring of Protection +2 must add 2 to AC and saves");
    }
    // A Bard reads Friends from an arcane scroll through Use and the Cast menu
    // (MAGIC-6); Fireball stays on it for a fight.
    const auto carried_now = campaign_->member(campaign_->selected()).character.inventory().items();
    const auto parchment = std::find_if(carried_now.begin(), carried_now.end(),
                                        [](const auto &item)
                                        {
                                            return item.definition_id.starts_with("scroll:");
                                        });
    const auto parchment_id = parchment->id;
    items.select(static_cast<int>(parchment - carried_now.begin()));
    inventory_selected(parchment - carried_now.begin());
    if (!required_node<Button>(*this, "InventoryPanel/%UseItem").is_visible())
    {
        throw std::runtime_error("The inventory must offer Use for a scroll the Bard can read");
    }
    required_node<Button>(*this, "InventoryPanel/%UseItem").emit_signal("pressed");
    auto &spell_choice = required_node<OptionButton>(*this, "CastPanel/%Action");
    if (!required_node<Control>(*this, "CastPanel").is_visible() ||
        spell_choice.get_item_count() != 1 ||
        spell_choice.get_selected_metadata() != Variant(String("0:friends")))
    {
        throw std::runtime_error("Use on a scroll must open the Cast menu with its spell");
    }
    required_node<Button>(*this, "CastPanel/%Use").emit_signal("pressed");
    const auto read =
        campaign_->member(campaign_->selected()).character.inventory().find(parchment_id);
    if (!read || read->get().definition_id != "scroll:arcane:fireball" ||
        !required_node<Label>(*this, "CastPanel/%Status").get_text().contains("reads"))
    {
        throw std::runtime_error("Reading Friends must leave Fireball on the scroll");
    }
    required_node<Button>(*this, "CastPanel/%Close").emit_signal("pressed");
    required_node<Button>(*this, "%Inventory").emit_signal("pressed");
    campaign_->restore(std::move(*identify_check_retained_));
    identify_check_retained_.reset();
    refresh();
    refresh_inventory();
    required_node<Button>(*this, "InventoryPanel/%Close").emit_signal("pressed");
    required_node<Button>(*this, "%LeaveShop").emit_signal("pressed");
    shop_check_stage_ = 3;
}

void RolfTourView::check_run()
{
    try
    {
        const auto press_key = [&](Key code, bool echo = false)
        {
            Ref<InputEventKey> key;
            key.instantiate();
            key->set_keycode(code);
            key->set_pressed(true);
            key->set_echo(echo);
            get_viewport()->push_input(key, true);
            key->set_pressed(false);
            key->set_echo(false);
            get_viewport()->push_input(key, true);
        };
        if (!session_ || session_->snapshot().phase == TourPhase::faulted)
        {
            throw std::runtime_error(session_ ? session_->snapshot().diagnostic
                                              : error_.utf8().get_data());
        }
        if (++check_frames_ > 2000)
        {
            throw std::runtime_error("Tour integration check timed out; recovery stage " +
                                     std::to_string(recovery_stage_) + ", position " +
                                     std::to_string(session_->snapshot().pose.x) + "," +
                                     std::to_string(session_->snapshot().pose.y) + ", " +
                                     session_->snapshot().dialogue);
        }
        const auto &s = session_->snapshot();
        if (town_check_ && s.tour_finished)
        {
            check_town();
            return;
        }
        if (s.phase == TourPhase::awaiting_continue)
        {
            if (s.continue_ticket != checked_ticket_)
            {
                checked_ticket_ = s.continue_ticket;
                ++check_prompts_;
                capture_pending_ = true;
                return;
            }
            if (capture_pending_)
            {
                capture_frame("rolf-tour-" + String::num_uint64(check_prompts_));
                capture_pending_ = false;
                if (required_node<Button>(*this, "%Continue").is_disabled() ||
                    !required_node<Button>(*this, "%Forward").is_disabled())
                {
                    throw std::runtime_error("Incorrect input lock at tour prompt");
                }
                UtilityFunctions::print("Tour pause ", check_prompts_, " at ", s.pose.x, ",",
                                        s.pose.y, " facing ",
                                        static_cast<int64_t>(index(s.pose.facing)));
                required_node<Button>(*this, "%Restart").grab_focus();
                const auto pose = s.pose;
                press_key(Key::KEY_RIGHT);
                if (session_->snapshot().pose != pose)
                {
                    throw std::runtime_error("Keyboard bypassed tour movement lock");
                }
                press_key(Key::KEY_ENTER, true);
                if (session_->snapshot().phase != TourPhase::awaiting_continue)
                {
                    throw std::runtime_error("Held Enter skipped a prompt");
                }
                press_key(Key::KEY_ENTER);
                if (session_->snapshot().phase != TourPhase::running)
                {
                    throw std::runtime_error(
                        "Enter did not continue with a different button focused");
                }
            }
        }
        else if (s.phase == TourPhase::completed)
        {
            if (check_prompts_ != 8)
            {
                throw std::runtime_error("Expected all eight original dialogue pauses");
            }
            required_node<Button>(*this, "%Restart").grab_focus();
            const auto facing = s.pose.facing;
            press_key(Key::KEY_RIGHT);
            if (session_->snapshot().pose.facing != turned_right(facing))
            {
                throw std::runtime_error("Exploration turn did not update native state");
            }
            const auto before = session_->snapshot().pose;
            press_key(Key::KEY_DOWN);
            if (session_->snapshot().pose != PartyPose{before.x, before.y, reversed(before.facing)})
            {
                throw std::runtime_error("Down arrow must turn without moving");
            }
            UtilityFunctions::print("Godot C++ tour integration passed: ", check_prompts_,
                                    " pauses.");
            get_tree()->quit(0);
            checking_ = false;
        }
    }
    catch (const std::exception &error)
    {
        UtilityFunctions::push_error(String::utf8(error.what()));
        get_tree()->quit(1);
        checking_ = false;
    }
}

void RolfTourView::check_town()
{
    if (recovery_stage_)
    {
        check_recovery();
        return;
    }
    const auto &s = session_->snapshot();
    const auto inventory_size = [&]
    {
        return campaign_
                   ? campaign_->member(campaign_->selected()).character.inventory().items().size()
                   : dax_session().party().inventory.size();
    };
    const auto gold = [&]
    {
        return campaign_ ? campaign_->purse_gold() : gold_value(dax_session().party().wealth);
    };
    if (check_prompts_ != 8)
    {
        throw std::runtime_error("Town started before the full tour");
    }
    if (shop_check_stage_ == 3 && s.phase == TourPhase::completed)
    {
        if (inventory_size() != 1 || dax_session().script_variable(EclAddress{0x6BC1}) != gold())
        {
            throw std::runtime_error("Shop results did not persist after leaving");
        }
        open_member_sheet(0);
        auto *sheet =
            &required_node<Window>(*this, embedded_party_ ? "../TownSheet" : "MemberSheet");
        if (!sheet->is_visible())
        {
            throw std::runtime_error("Town party click did not open a character sheet");
        }
        sheet->emit_signal("close_requested");
        if (sheet->is_visible())
        {
            throw std::runtime_error("Town character sheet did not close");
        }
        // M opens the Cast menu while exploring; a Fighter has nothing to cast,
        // but an Orc has Adrenaline Rush (RUSH-1).
        Ref<InputEventKey> magic;
        magic.instantiate();
        magic->set_keycode(Key::KEY_M);
        magic->set_pressed(true);
        respond_to_input(magic);
        const NodePath offers_path{"CastPanel/%Action"};
        const auto &offers = required_node<OptionButton>(*this, offers_path);
        const bool orc = settings::flag("--adrenaline-check");
        if (!required_node<Control>(*this, "CastPanel").is_visible() ||
            (orc ? offers.get_item_count() != 1 ||
                       offers.get_item_metadata(0) != Variant("adrenaline_rush")
                 : !required_node<Label>(*this, "CastPanel/%Status")
                        .get_text()
                        .contains("Nothing to cast")))
        {
            throw std::runtime_error("M must open the Cast menu");
        }
        required_node<Button>(*this, "CastPanel/%Close").emit_signal("pressed");
        UtilityFunctions::print(
            "Godot town integration passed: tour, doors, shop purchase, inventory and return to exploration.");
        if (!embedded_party_)
        {
            get_tree()->quit(0);
        }
        shop_check_stage_ = 4;
        checking_ = false;
        return;
    }
    if (s.phase == TourPhase::shopping)
    {
        if (shop_check_stage_ == 0)
        {
            shop_check_stage_ = 1;
            return;
        }
        if (shop_check_stage_ >= 10)
        {
            check_identification_spells();
            return;
        }
        if (shop_check_stage_ == 1)
        {
            capture_frame("phlan-shop");
            const auto gold_before = gold();
            // The check buys the Shield, wherever the alphabetical list puts it.
            const auto shield =
                std::ranges::find_if(shop_rows_,
                                     [&](std::size_t n)
                                     {
                                         return dax_session().shop_stock()[n].label() == "Shield";
                                     });
            if (required_node<ItemList>(*this, "%Choices").get_item_count() != 55 ||
                required_node<Button>(*this, "%Continue").is_disabled() ||
                shield == shop_rows_.end())
            {
                throw std::runtime_error("Arms shop list is not available in Godot");
            }
            const auto price = dax_session().shop_stock()[*shield].stored.value;
            required_node<ItemList>(*this, "%Choices")
                .select(static_cast<std::int32_t>(shield - shop_rows_.begin()));
            required_node<Button>(*this, "%Continue").emit_signal("pressed");
            if (gold() != gold_before - price || inventory_size() != 1)
            {
                throw std::runtime_error(
                    "Godot purchase did not debit the purse and add inventory");
            }
            required_node<Button>(*this, "%Inventory").emit_signal("pressed");
            shop_check_stage_ = 2;
            return;
        }
        required_node<ItemList>(*this, "InventoryPanel/%Items").select(0);
        const auto id = campaign_->selected();
        // Auto-equip (EQUIP-1), on unless the settings turn it off, put the
        // shield on as it was bought.
        if (settings::auto_equip())
        {
            const auto worn = campaign_->profile(id).armor_class;
            required_node<Button>(*this, "InventoryPanel/%Unequip").emit_signal("pressed");
            if (campaign_->profile(id).armor_class != worn - 2)
            {
                throw std::runtime_error("Auto-equip did not put the bought shield on");
            }
        }
        const auto ac = campaign_->profile(id).armor_class;
        required_node<Button>(*this, "InventoryPanel/%Equip").emit_signal("pressed");
        if (campaign_->profile(id).armor_class != ac + 2)
        {
            throw std::runtime_error("Inventory Equip did not apply shield AC");
        }
        required_node<Button>(*this, "InventoryPanel/%Unequip").emit_signal("pressed");
        if (campaign_->profile(id).armor_class != ac)
        {
            throw std::runtime_error("Inventory Unequip did not remove shield AC");
        }
        required_node<Button>(*this, "InventoryPanel/%Equip").emit_signal("pressed");
        // The shop's ID takes any item for 200 gp; a plain shield hides nothing.
        auto purse = campaign_->state().wealth;
        coins(purse, Coin::gold) = 250;
        campaign_->set_wealth(purse);
        auto &identify = required_node<Button>(*this, "InventoryPanel/%Identify");
        if (!identify.is_visible() || identify.is_disabled())
        {
            throw std::runtime_error("The shop must offer ID in the inventory");
        }
        identify.emit_signal("pressed");
        if (gold() != 50 || !required_node<Label>(*this, "InventoryPanel/%Status")
                                 .get_text()
                                 .contains("nothing new"))
        {
            throw std::runtime_error("ID must charge 200 gp and report what it learned");
        }
        auto retained = campaign_->checkpoint();
        auto changed = retained;
        auto &member = changed.roster.at(0);
        auto inventory = member.character.inventory();
        auto draft = member.character.creation_data();
        draft.character_class = "wizard";
        draft.training.erase("class:fighter");
        draft.training.erase(
            "class:fighter:fighting_style"); // This untrained-shield fixture changes class.
        draft.training.erase("class:fighter:weapon_mastery");
        member.character = opengold::Character(*campaign_->character_rules(), draft,
                                               member.character.appearance());
        member.character.replace_inventory(std::move(inventory));
        member.equipped.clear();
        member.vitals = {member.character.sheet().hit_points, false, {}};
        campaign_->restore(std::move(changed));
        refresh_inventory();
        required_node<ItemList>(*this, "InventoryPanel/%Items").select(0);
        required_node<Button>(*this, "InventoryPanel/%Equip").emit_signal("pressed");
        if (!required_node<Label>(*this, "InventoryPanel/%Status")
                 .get_text()
                 .contains("Untrained shield: no AC bonus"))
        {
            throw std::runtime_error("Equip must display the untrained penalty");
        }
        auto staff_fixture = retained;
        auto &wielder = staff_fixture.roster.at(0);
        const auto staff = wielder.character.add_item(
            {.definition_id = "quarterstaff", .name = "Quarterstaff", .quantity = 1});
        wielder.equipped.push_back(staff);
        campaign_->restore(std::move(staff_fixture));
        refresh_inventory();
        if (campaign_->profile(id).weapon_hands != 1)
        {
            throw std::runtime_error("A shield must keep the quarterstaff one-handed");
        }
        required_node<ItemList>(*this, "InventoryPanel/%Items").select(0);
        required_node<Button>(*this, "InventoryPanel/%Unequip").emit_signal("pressed");
        if (campaign_->profile(id).weapon_hands != 2)
        {
            throw std::runtime_error("Removing the shield must free the second hand");
        }
        campaign_->restore(std::move(retained));
        refresh_inventory();
        capture_frame("phlan-inventory");
        shop_check_stage_ = 10;
        check_identification_spells();
        return;
    }
    if (s.phase == TourPhase::awaiting_continue)
    {
        std::size_t selection = s.choices.size() - 1;
        for (const auto *safe : {"NO", "LEAVE", "RUN", "GO", "NONE", "EXIT"})
        {
            for (std::size_t n = 0; n < s.choices.size(); ++n)
            {
                if (s.choices[n] == safe)
                {
                    selection = n;
                }
            }
        }
        if (s.pose.x == 13 && s.pose.y == 8 && s.dialogue.find("SHOP") != std::string::npos)
        {
            selection = 0;
        }
        if (campaign_ && s.choosing_member)
        {
            selection = 0;
        }
        required_node<ItemList>(*this, "%Choices").select(static_cast<std::int32_t>(selection));
        required_node<Button>(*this, "%Continue").emit_signal("pressed");
        return;
    }
    if (s.phase == TourPhase::awaiting_input)
    {
        required_node<LineEdit>(*this, "%Answer").set_text("0");
        next();
        return;
    }
    if (s.phase != TourPhase::completed)
    {
        return;
    }
    check_walk_to(13, 8);
}

void RolfTourView::check_walk_to(unsigned tx, unsigned ty)
{
    const auto &s = session_->snapshot();
    // Exercise actual movement callbacks and original script entry dispatch.
    const unsigned position = s.pose.y * 16 + s.pose.x;
    if (check_pending_edge_)
    {
        if (position != check_pending_edge_->second)
        {
            check_refused_edges_.insert(*check_pending_edge_);
        }
        check_pending_edge_.reset();
    }
    static constexpr std::array dx{0, 1, 0, -1}, dy{-1, 0, 1, 0};
    std::array<int, 256> previous;
    previous.fill(-1);
    std::queue<int> cells;
    const int origin = s.pose.y * 16 + s.pose.x;
    previous[origin] = origin;
    cells.push(origin);
    while (!cells.empty())
    {
        const int cell = cells.front();
        cells.pop();
        for (int d = 0; d < 4; ++d)
        {
            const int x = cell % 16 + dx[d], y = cell / 16 + dy[d];
            if (x < 0 || y < 0 || x >= 16 || y >= 16)
            {
                continue;
            }
            const auto &a = dax_session().map().at(cell % 16, cell / 16);
            const auto &b = dax_session().map().at(x, y);
            const int r = (d + 2) % 4;
            if (a.doors[d] > 1 || b.doors[r] > 1 || (a.walls[d] && !a.doors[d]) ||
                (b.walls[r] && !b.doors[r]))
            {
                continue;
            }
            const int next = y * 16 + x;
            if (previous[next] >= 0 || check_refused_edges_.contains({cell, next}))
            {
                continue;
            }
            previous[next] = cell;
            cells.push(next);
        }
    }
    int next = ty * 16 + tx;
    if (next == origin)
    {
        look();
        return;
    }
    if (previous[next] < 0)
    {
        throw std::runtime_error("No route to acceptance target " + std::to_string(tx) + "," +
                                 std::to_string(ty) + " from " + std::to_string(s.pose.x) + "," +
                                 std::to_string(s.pose.y) + "; blocked edges " +
                                 std::to_string(check_refused_edges_.size()));
    }
    while (previous[next] != origin)
    {
        next = previous[next];
    }
    const auto facing = next % 16 > static_cast<int>(s.pose.x)   ? MapDirection::east
                        : next % 16 < static_cast<int>(s.pose.x) ? MapDirection::west
                        : next / 16 > static_cast<int>(s.pose.y) ? MapDirection::south
                                                                 : MapDirection::north;
    if (s.pose.facing == facing)
    {
        check_pending_edge_ = {{origin, next}};
    }
    required_node<Button>(*this, s.pose.facing == facing ? "%Forward" : "%Right")
        .emit_signal("pressed");
}

void RolfTourView::check_district_labels(const TourSnapshot &s)
{
    // Labels follow the session at the next refresh, not immediately.
    if (shown_revision_ != s.revision)
    {
        return;
    }
    const String district = place_name(application_settings(*this).chosen_game(), session_.get());
    const String location = required_node<Label>(*this, "%Location").get_text();
    const String map_title = required_node<Label>(*this, "%MapTitle").get_text();
    const String speaker = required_node<Label>(*this, "%Speaker").get_text();
    const bool speaker_shows_district =
        s.tour_finished && s.phase != TourPhase::shopping && s.phase != TourPhase::treasure;
    const String title = required_node<Label>(*this, "%Title").get_text();
    if (!location.begins_with(district + String(" / ")) ||
        map_title != i18n::format("{district}    N ↑", {{"district", district}}) ||
        (speaker_shows_district && speaker != district) ||
        (s.tour_finished &&
         title != i18n::format("OPENGOLDBOX  /  {district}", {{"district", district}})))
    {
        throw std::runtime_error(
            "District labels do not name area " + std::to_string(s.area_id) + ": " +
            (title + String(" | ") + location + String(" | ") + map_title + String(" | ") + speaker)
                .utf8()
                .get_data());
    }
    if (s.area_id == 20)
    {
        slums_labels_checked_ = true;
    }
}

bool RolfTourView::check_expedition_step()
{
    if (!session_)
    {
        throw std::runtime_error("Expedition session missing");
    }
    const auto &s = session_->snapshot();
    if (!dax_session().script_diagnostics().empty())
    {
        throw std::runtime_error(dax_session().script_diagnostics().back());
    }
    if (s.phase == TourPhase::faulted)
    {
        throw std::runtime_error(s.diagnostic);
    }
    check_district_labels(s);
    if (fight_waits_on_picture_)
    {
        // This check steps the view itself, so it hurries the close-up as a key does.
        if (++monster_picture_check_frames_ < 120)
        {
            return false;
        }
        UtilityFunctions::print("Monster close-up shown with ",
                                static_cast<int64_t>(monster_frames_.size()),
                                " frames before combat");
        capture_frame("monster-close-up");
        dismiss_monster_picture();
        return false;
    }
    if (s.phase == TourPhase::treasure)
    {
        // The check takes the first item, then leaves the rest behind.
        if (!dax_session().treasure_pile().empty() && treasure_notice_.is_empty())
        {
            required_node<ItemList>(*this, "%Choices").select(0);
            required_node<Button>(*this, "%Continue").emit_signal("pressed");
            return false;
        }
        required_node<Button>(*this, "%LeaveShop").emit_signal("pressed");
        return false;
    }
    if (s.phase == TourPhase::awaiting_continue)
    {
        const auto choice = s.choices.size() == 5 && s.choices[0] == "Fight" ? 1 : 0;
        required_node<ItemList>(*this, "%Choices").select(choice);
        required_node<Button>(*this, "%Continue").emit_signal("pressed");
        return false;
    }
    if (s.phase != TourPhase::completed)
    {
        return false;
    }
    if (s.area_id == 0)
    {
        if (dax_session().script_variable(ecl_slums_orc_victory) == 255)
        {
            if (!slums_labels_checked_)
            {
                throw std::runtime_error("Slums district labels were never checked");
            }
            return true;
        }
        if (s.pose.x != 0 || s.pose.y != 4)
        {
            throw std::runtime_error("Unexpected tour destination");
        }
        if (s.pose.facing != MapDirection::west)
        {
            right();
        }
        else
        {
            forward();
        }
        return false;
    }
    if (dax_session().script_variable(ecl_slums_orc_victory) == 255)
    {
        if (s.pose.x != 15 || s.pose.y != 4)
        {
            check_walk_to(15, 4);
        }
        else if (s.pose.facing != MapDirection::east)
        {
            right();
        }
        else
        {
            forward();
        }
    }
    else
    {
        check_walk_to(12, 1);
    }
    return false;
}

void RolfTourView::start_recovery_check()
{
    // Acceptance-only fixtures exercise mortality through ordinary camp/travel.
    const auto reserve = campaign_->add_pc(campaign_->state().roster.at(0).character);
    campaign_->remove(reserve);
    auto state = campaign_->checkpoint();
    auto &member = state.roster.at(0);
    state.random_state.value = 17;
    const auto stable = presentation::srd_vitals(
        state.roster.at(1).character, {.stable = true, .hit_dice = 1, .stable_recovery_ms = 2000});
    const auto dying = presentation::srd_vitals(
        state.roster.back().character,
        {.successes = 2, .failures = 1, .hit_dice = 1, .death_save_ms = 6000});
    state.roster.at(1).vitals = {0, false, stable};
    state.roster.back().vitals = {0, false, dying};
    // Gold alone pays the temple and, with change made, the inn's one platinum.
    if (member.vitals.dead)
    {
        throw std::runtime_error("Recovery check requires a living victory survivor");
    }
    member.vitals.hit_points = 1;
    state.wealth = {0, 0, 0, 200, 0, 0, 0};
    campaign_->restore(state);
    recovery_before_ = std::move(state);
    recovery_stage_ = 1;
    check_frames_ = 0;
    checking_ = town_check_ = true;
}

void RolfTourView::check_recovery()
{
    const auto &s = session_->snapshot();
    const auto id = campaign_->state().roster.at(0).id;
    if (!dax_session().script_diagnostics().empty())
    {
        throw std::runtime_error("Recovery route fault: " +
                                 dax_session().script_diagnostics().back());
    }
    const auto choose = [&](std::size_t choice)
    {
        required_node<ItemList>(*this, "%Choices").select(static_cast<std::int32_t>(choice));
        required_node<Button>(*this, "%Continue").emit_signal("pressed");
    };
    if (s.phase == TourPhase::awaiting_continue)
    {
        // The temple's Heal list (TEMPLE-1): Cure Light Wounds for the first
        // member, its price confirmed, then Leave.
        const auto choice_at = [&](std::string_view label)
        {
            const auto found = std::find_if(s.choices.begin(), s.choices.end(),
                                            [&](const auto &c)
                                            {
                                                return c.starts_with(label);
                                            });
            return found == s.choices.end() ? std::optional<std::size_t>{}
                                            : std::optional<std::size_t>{static_cast<std::size_t>(
                                                  found - s.choices.begin())};
        };
        const auto light = choice_at("Cure Light Wounds (");
        if (recovery_stage_ == 3 && light)
        {
            if (save_check_ && !save_cancel_checked_)
            {
                recovery_before_ = campaign_->checkpoint();
                save_cancel_checked_ = save_cancel_pending_ = true;
                choose(s.choices.size() - 1);
                return;
            }
            if (recovery_capture_ticket_ != s.continue_ticket)
            {
                recovery_capture_ticket_ = s.continue_ticket;
                return;
            }
            if (s.dialogue.starts_with("The clerics cast "))
            {
                choose(s.choices.size() - 1);
                recovery_stage_ = 4;
                return;
            }
            recovery_before_ = campaign_->checkpoint();
            capture_frame("party-temple");
            choose(*light);
            return;
        }
        if (recovery_stage_ == 3 && s.dialogue == "Choose who receives Cure Light Wounds.")
        {
            choose(*choice_at(campaign_->member(id).character.sheet().name));
            return;
        }
        if (const auto pay = choice_at("Pay"); recovery_stage_ == 3 && pay)
        {
            choose(*pay);
            return;
        }
        std::size_t selection = s.choices.size() - 1;
        for (const auto *safe : {"NO", "LEAVE", "RUN", "GO", "NONE", "EXIT", "Cancel"})
        {
            for (std::size_t n = 0; n < s.choices.size(); ++n)
            {
                if (s.choices[n] == safe)
                {
                    selection = n;
                }
            }
        }
        const auto event = dax_session().map().at(s.pose.x, s.pose.y).event_number();
        // Temple cells can be transit cells: enter, then cancel the native service.
        // Answering NO makes the original script move the party back to its prior cell.
        if (((recovery_stage_ == 3 || recovery_stage_ == 5) && event == 6) ||
            (recovery_stage_ == 5 && event == 9))
        {
            for (std::size_t n = 0; n < s.choices.size(); ++n)
            {
                if (s.choices[n] == "YES")
                {
                    selection = n;
                }
            }
            if (s.choosing_member)
            {
                selection = 0;
            }
        }
        choose(selection);
        return;
    }
    if (s.phase == TourPhase::shopping)
    {
        required_node<Button>(*this, "%LeaveShop").emit_signal("pressed");
        return;
    }
    if (s.phase == TourPhase::awaiting_input)
    {
        required_node<LineEdit>(*this, "%Answer").set_text("0");
        next();
        return;
    }
    if (s.phase != TourPhase::completed)
    {
        return;
    }
    const auto &member = campaign_->member(id);
    if (save_cancel_pending_)
    {
        if (campaign_->state().wealth != recovery_before_->wealth ||
            member.vitals != recovery_before_->roster.at(0).vitals)
        {
            throw std::runtime_error("Cancelled temple service changed party");
        }
        save_check_("cancelled-service");
        save_cancel_pending_ = false;
    }
    if (recovery_stage_ == 1)
    {
        required_node<Button>(*this, "%Camp").emit_signal("pressed");
        required_node<Button>(required_node<Window>(*this, "RestDialog"), "Start")
            .emit_signal("pressed");
        recovery_stage_ = 2;
        return;
    }
    if (recovery_stage_ == 2)
    {
        if (campaign_->state().time_minutes != recovery_before_->time_minutes + 5 ||
            member.vitals != recovery_before_->roster.at(0).vitals)
        {
            throw std::runtime_error(
                "Original city-watch interruption must consume five minutes without recovery");
        }
        if (campaign_->state().roster.at(1).vitals.hit_points != 1 ||
            campaign_->state().roster.back().vitals.hit_points != 1 ||
            campaign_->state().random_state.value != 11400714819323198502ULL ||
            campaign_->state().roster.back().vitals.resources !=
                presentation::srd_vitals(campaign_->state().roster.back().character,
                                         {.hit_dice = 1}))
        {
            throw std::runtime_error(
                "Camp time must advance companion Stable recovery and the reserve death save exactly once");
        }
        if (save_check_)
        {
            save_check_("interrupted-rest");
        }
        recovery_stage_ = 3;
    }
    if (recovery_stage_ == 4)
    {
        const auto gold_before = coins(recovery_before_->wealth, Coin::gold);
        if (coins(campaign_->state().wealth, Coin::gold) != gold_before - 100 ||
            member.vitals.hit_points <= 1 || dax_session().script_variable(ecl_temple_service) != 0)
        {
            throw std::runtime_error("Original temple must charge 100 gp, heal and resume ECL");
        }
        if (save_check_)
        {
            save_check_("temple-payment");
        }
        recovery_before_ = campaign_->checkpoint();
        recovery_stage_ = 5;
    }
    // Walking to the inn now consumes game time. Detect completion using the
    // persisted rest timestamp; the route's travel time is additional to 8 hours.
    if (recovery_stage_ == 5 && member.last_rest_minutes &&
        member.last_rest_minutes != recovery_before_->roster.at(0).last_rest_minutes)
    {
        if (*member.last_rest_minutes < recovery_before_->time_minutes + 480 ||
            member.vitals.hit_points != member.character.sheet().hit_points ||
            coins(campaign_->state().wealth, Coin::gold) !=
                coins(recovery_before_->wealth, Coin::gold) - 10)
        {
            throw std::runtime_error("Original inn payment and full recovery must persist");
        }
        const auto &payments = s.payments;
        const auto shown = required_node<RichTextLabel>(*this, "%Dialogue").get_text();
        if (payments.size() != 1 || payments[0].coins.paid != opengold::Coins{0, 0, 0, 10, 0} ||
            !shown.contains(payment_notice(payments[0])))
        {
            throw std::runtime_error("The inn's change-making payment must be shown");
        }
        // The Fighter's inn rest offers an optional mastery replacement; keep the
        // current set so the rested party is saved at an editable boundary.
        if (!campaign_->state().training_rest)
        {
            throw std::runtime_error("The inn rest must offer the Fighter's mastery choice");
        }
        required_node<Button>(*this, "RestTraining/Cancel").emit_signal("pressed");
        if (campaign_->state().training_rest)
        {
            throw std::runtime_error("Keeping the mastery set must finish the rest choices");
        }
        if (save_check_)
        {
            save_check_("inn-rest");
        }
        if (save_check_)
        {
            bool rejected = false;
            try
            {
                campaign_->temple_service(id, opengold::rules::TempleService::cure_light_wounds,
                                          100);
            }
            catch (const std::exception &)
            {
                rejected = true;
            }
            if (!rejected)
            {
                throw std::runtime_error("Full-health temple service should reject");
            }
            save_check_("rejected-service");
        }
        recovery_before_ = campaign_->checkpoint();
        recovery_stage_ = 6;
    }
    if (recovery_stage_ == 6)
    {
        required_node<Button>(*this, "%Camp").emit_signal("pressed");
        recovery_stage_ = 7;
        return;
    }
    if (recovery_stage_ == 7)
    {
        if (campaign_->state().time_minutes != recovery_before_->time_minutes ||
            !required_node<Button>(required_node<Window>(*this, "RestDialog"), "Start")
                 .is_disabled())
        {
            throw std::runtime_error("Immediate repeated long rest must be denied");
        }
        if (settings::flag("--adrenaline-check"))
        {
            check_camp_adrenaline_rush(id);
        }
        required_node<Button>(required_node<Window>(*this, "RestDialog"), "Finish")
            .emit_signal("pressed");
        if (save_check_)
        {
            save_check_("denied-rest");
        }
        if (save_check_)
        {
            // A wounded member gives the reloaded rest something to heal; the
            // route then continues from the unwounded party.
            auto rested = campaign_->checkpoint();
            auto wounded = rested;
            wounded.roster.at(0).vitals.hit_points = 1;
            campaign_->restore(std::move(wounded));
            (void)campaign_->rest(opengold::RestKind::short_rest);
            save_check_("short-rest-spending");
            campaign_->finish_short_rest(campaign_->state().short_rest->ticket);
            campaign_->restore(std::move(rested));
        }
        capture_frame("party-rest");
        recovery_stage_ = 8;
        checking_ = false;
        UtilityFunctions::print(
            "Godot recovery check passed: original interruption, temple payment, inn rest, repeated-rest denial and companion/reserve mortality.");
        return;
    }
    const auto target_event = recovery_stage_ == 3 ? 6u : 9u;
    for (unsigned y = 0; y < 16; ++y)
    {
        for (unsigned x = 0; x < 16; ++x)
        {
            if (dax_session().map().at(x, y).event_number() == target_event)
            {
                check_walk_to(x, y);
                return;
            }
        }
    }
    throw std::runtime_error("Original recovery location is missing");
}
