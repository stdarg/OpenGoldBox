#include "godot_images.h"
#include "godot_nodes.h"
#include "application_settings.h"
#include "character_text.h"
#include "localization.h"
#include "game_resources.h"
#include "shop_data_files.h"
#include "game_monsters.h"
#include "game_wall_art.h"
#include "rolf_tour_view.h"
#include "inventory_label.h"
#include "opengold/campaign_party.h"
#include "opengold/npc_portraits.h"
#include "vital_fixtures.h"
#include "opengold/srd5.h"
#include "opengold/scroll_key.h"
#include "godot_path.h"
#include "guarded_handlers.h"
#include "monster_picture_timing.h"
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/item_list.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/viewport_texture.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <vector>
#include <queue>

using namespace godot;
using namespace opengold::por;
using opengold::Coin;
using presentation::required_node;

namespace
{
const std::array<Vector2, 4> direction{Vector2(0, -1), Vector2(1, 0), Vector2(0, 1),
                                       Vector2(-1, 0)};
constexpr std::array<const char *, 4> direction_name{N_("North"), N_("East"), N_("South"),
                                                     N_("West")};

// The areas the session can enter: the town (0), the Slums (20), Kuto's
// Well's plaza (29) and catacombs (32), and Podol Plaza (18).
[[nodiscard]] const char *district_name(unsigned area_id)
{
    if (area_id == 20)
    {
        return N_("Slums");
    }
    if (area_id == 18)
    {
        return N_("Podol Plaza");
    }
    if (area_id == 29 || area_id == 32)
    {
        return N_("Kuto's Well");
    }
    return N_("New Phlan");
}

} // namespace

void RolfTourView::_bind_methods()
{
    ADD_SIGNAL(MethodInfo("save_requested", PropertyInfo(Variant::BOOL, "saving")));
    ADD_SIGNAL(MethodInfo("party_member_selected", PropertyInfo(Variant::INT, "slot")));
    ADD_SIGNAL(MethodInfo("level_up_requested", PropertyInfo(Variant::INT, "id")));
    // Reincarnate from camp: the new body's pages belong to the creation view.
    ADD_SIGNAL(MethodInfo("reincarnation_requested", PropertyInfo(Variant::INT, "caster"),
                          PropertyInfo(Variant::INT, "target"),
                          PropertyInfo(Variant::STRING, "action")));
    ClassDB::bind_method(D_METHOD("tour_visuals"), &RolfTourView::tour_visuals);
}

void RolfTourView::_notification(int what)
{
    presentation::run_guarded(*this,
                              [&]
                              {
                                  respond_to_notification(what);
                              });
}

void RolfTourView::respond_to_notification(int what)
{
    if (what == NOTIFICATION_RESIZED && ready_)
    {
        layout();
        required_node<Control>(*this, "TourVisuals").queue_redraw();
    }
}

void RolfTourView::_ready()
{
    presentation::run_guarded(*this,
                              [&]
                              {
                                  prepare_view();
                              });
}

void RolfTourView::prepare_view()
{
    i18n::prepare_ui(*this);
    // Resizing the window can notify this view before dynamic controls exist.
    ready_ = false;
    // Child nodes are scene-owned; these lookups are temporary non-owning views.
    required_node<Button>(*this, "Continue")
        .connect("pressed", presentation::guarded(this, &RolfTourView::next));
    required_node<Button>(*this, "Restart")
        .connect("pressed", presentation::guarded(this, &RolfTourView::restart));
    required_node<Button>(*this, "Left")
        .connect("pressed", presentation::guarded(this, &RolfTourView::left));
    required_node<Button>(*this, "Right")
        .connect("pressed", presentation::guarded(this, &RolfTourView::right));
    required_node<Button>(*this, "Forward")
        .connect("pressed", presentation::guarded(this, &RolfTourView::forward));
    required_node<Button>(*this, "Look")
        .connect("pressed", presentation::guarded(this, &RolfTourView::look));
    required_node<Button>(*this, "Camp")
        .connect("pressed", presentation::guarded(this, &RolfTourView::camp));
    required_node<Button>(*this, "Inventory")
        .connect("pressed", presentation::guarded(this, &RolfTourView::inventory));
    required_node<Button>(*this, "InventoryPanel/Close")
        .connect("pressed", presentation::guarded(this, &RolfTourView::inventory));
    for (unsigned slot = 0; slot < 8; ++slot)
    {
        auto *member = &required_node<Button>(*this, String("PartyList/Rows/Member") +
                                                         String::num_uint64(slot));
        member->connect("pressed",
                        presentation::guarded(this, &RolfTourView::party_selected).bind(slot));
        auto arrow = presentation::instantiate_control<Button>(
            "res://scenes/control_templates/party_advance.tscn");
        arrow->set_tooltip_text(i18n::text(N_("Level up")));
        arrow->connect("pressed",
                       presentation::guarded(this, &RolfTourView::level_up_requested).bind(slot));
        presentation::attach_child(*member, std::move(arrow));
    }
    required_node<ItemList>(*this, "InventoryPanel/Items")
        .connect("item_selected", presentation::guarded(this, &RolfTourView::inventory_selected));
    required_node<Button>(*this, "InventoryPanel/Equip")
        .connect("pressed", presentation::guarded(this, &RolfTourView::equip_item).bind(true));
    required_node<Button>(*this, "InventoryPanel/Unequip")
        .connect("pressed", presentation::guarded(this, &RolfTourView::equip_item).bind(false));
    required_node<Button>(*this, "InventoryPanel/Identify")
        .connect("pressed", presentation::guarded(this, &RolfTourView::identify_item));
    required_node<Button>(*this, "InventoryPanel/CastIdentify")
        .connect("pressed", presentation::guarded(this, &RolfTourView::cast_identify));
    required_node<Button>(*this, "InventoryPanel/UseItem")
        .connect("pressed", presentation::guarded(this, &RolfTourView::use_inventory_item));
    required_node<Button>(*this, "Cast")
        .connect("pressed", presentation::guarded(this, &RolfTourView::cast_menu));
    required_node<Button>(*this, "CastPanel/Close")
        .connect("pressed", presentation::guarded(this, &RolfTourView::cast_menu));
    required_node<OptionButton>(*this, "CastPanel/Action")
        .connect("item_selected", presentation::guarded(this, &RolfTourView::cast_action_selected));
    required_node<Button>(*this, "CastPanel/Use")
        .connect("pressed", presentation::guarded(this, &RolfTourView::cast_use));
    required_node<Button>(*this, "MemberSheet/Close")
        .connect("pressed", presentation::guarded(this, &RolfTourView::close_sheet));
    required_node<Window>(*this, "MemberSheet")
        .connect("close_requested", presentation::guarded(this, &RolfTourView::close_sheet));
    required_node<Button>(*this, "LeaveShop")
        .connect("pressed", presentation::guarded(this, &RolfTourView::leave_shop));
    for (bool saving : {true, false})
    {
        auto &button = required_node<Button>(*this, saving ? "SaveGame" : "LoadGame");
        button.set_text(i18n::text(saving ? N_("Save game") : N_("Load game")));
        button.connect("pressed",
                       presentation::guarded(this, &RolfTourView::request_save).bind(saving));
        button.set_visible(embedded_party_);
    }
    ready_ = true;
    layout();
    if (Engine::get_singleton()->is_editor_hint())
    {
        return;
    }
    const auto args = OS::get_singleton()->get_cmdline_user_args();
    no_fog_ = settings::flag("--no-fog");
    town_check_ = args.has("--town-check");
    checking_ = args.has("--tour-check") || town_check_;
    rest_check_ = args.has("--rest-check");
    mastery_rest_check_ = args.has("--mastery-rest-check");
    capture_ = args.has("--capture");
    if (campaign_ && args.has("--party-check"))
    {
        town_check_ = checking_ = true;
    }
    // An original OpenGoldBox footstep cue, not extracted SSI sound data.
    footstep_.instantiate();
    footstep_->set_format(AudioStreamWAV::FORMAT_16_BITS);
    footstep_->set_mix_rate(22050);
    PackedByteArray samples;
    samples.resize(2205 * 2);
    std::uint32_t noise = 7;
    for (int i = 0; i < 2205; ++i)
    {
        noise = noise * 1664525U + 1013904223U;
        const double envelope = std::exp(-i / 280.0);
        const auto sample =
            static_cast<std::int16_t>((static_cast<int>(noise >> 16) - 32768) * 0.13 * envelope);
        samples.set(i * 2, static_cast<std::uint16_t>(sample) & 255);
        samples.set(i * 2 + 1, static_cast<std::uint16_t>(sample) >> 8);
    }
    footstep_->set_data(samples);
    required_node<AudioStreamPlayer>(*this, "Footstep").set_stream(footstep_);
    setup_rest();
    restart();
    if (embedded_party_)
    {
        required_node<Button>(*this, "Restart").hide();
    }
}

void RolfTourView::layout()
{
    auto &shopping_layout = required_node<Node>(*this, "ShoppingLayout");
    const bool shopping = session_ && session_->snapshot().phase == TourPhase::shopping;
    shopping_layout.call("apply_layout", shopping);
    const auto bounds = [&](const char *name)
    {
        auto &control = required_node<Control>(*this, name);
        return Rect2(control.get_position(), control.get_size());
    };
    scene_rect_ = bounds("SceneBounds");
    map_rect_ = bounds("MapBounds");
    dialogue_rect_ = bounds("DialogueBounds");
    required_node<Label>(*this, "Legend").hide();
    required_node<Node>(*this, "MemberSheet/WindowLayout").call("fit", Vector2i(get_size()));
}

void RolfTourView::restart()
{
    forget_monster_picture();
    try
    {
        error_ = "";
        if (!campaign_ || !embedded_party_)
        {
            const auto pack = game_rules_file();
            campaign_ = std::make_shared<opengold::CampaignParty>(
                opengold::srd5::load(presentation::path_from_godot(pack)));
            opengold::rules::CharacterDraft d;
            d.race = "human";
            d.gender = "female";
            d.character_class = "fighter";
            d.alignment = "neutral_good";
            d.background = "soldier";
            d.name = "Adventurer";
            d.rolled = true;
            for (auto &roll : d.rolls)
            {
                roll = {{6, 5, 4, 1}, 3};
            }
            (void)campaign_->add_pc(opengold::Character(*opengold::srd5::character_rules(), d, {}));
            campaign_->add_coins(Coin::gold, 9999);
        }
        if (session_)
        {
            session_->restart();
            session_->campaign_party(campaign_);
        }
        else
        {
            const auto directory = settings::game_path();
            session_.emplace(
                RolfTourSession::load(presentation::path_from_godot(directory), game_wall_art(),
                                      game_monsters()));
            session_->encounter_challenge(settings::encounter_challenge());
            session_->difficulty(settings::difficulty());
            use_shop_data(*session_);
            if (campaign_)
            {
                session_->campaign_party(campaign_);
            }
            for (unsigned i = 0; i < sprites_.size(); ++i)
            {
                const auto &source = session_->sprites()[i];
                sprites_[i] = presentation::image_texture(source);
            }
        }
        rendered_pose_.reset();
        rendered_sprite_id_ = 999;
        displayed_ticket_ = 0;
        played_footsteps_ = 0;
        shown_revision_ = 0;
        session_->advance(0);
    }
    catch (const std::exception &error)
    {
        session_.reset();
        forget_monster_picture();
        error_ = String::utf8(error.what());
    }
    refresh();
}

void RolfTourView::next()
{
    if (!session_)
    {
        return;
    }
    // Continue, like any key, leaves the monster close-up for combat.
    if (shown_monster_picture_)
    {
        dismiss_monster_picture();
        return;
    }
    const auto &s = session_->snapshot();
    const auto selected = required_node<ItemList>(*this, "Choices").get_selected_items();
    if (s.phase == TourPhase::shopping)
    {
        if (!selected.is_empty())
        {
            session_->buy(s.continue_ticket, selected[0]);
        }
    }
    else if (s.phase == TourPhase::awaiting_input)
    {
        session_->input(s.continue_ticket,
                        required_node<LineEdit>(*this, "Answer").get_text().utf8().get_data());
    }
    else
    {
        session_->choose(s.continue_ticket, selected.is_empty() ? 0 : selected[0]);
    }
    refresh();
}

void RolfTourView::left()
{
    movement(ExplorationCommand::turn_left);
}

void RolfTourView::right()
{
    movement(ExplorationCommand::turn_right);
}

void RolfTourView::forward()
{
    movement(ExplorationCommand::forward);
}

void RolfTourView::look()
{
    movement(ExplorationCommand::look);
}

void RolfTourView::leave_shop()
{
    if (session_)
    {
        session_->leave_shop(session_->snapshot().continue_ticket);
        refresh();
    }
}

void RolfTourView::inventory()
{
    auto *panel = &required_node<Control>(*this, "InventoryPanel");
    if (panel->is_visible())
    {
        panel->hide();
        return;
    }
    if (!session_)
    {
        return;
    }
    refresh_inventory();
    panel->show();
}

void RolfTourView::refresh_inventory()
{
    auto *items = &required_node<ItemList>(*this, "InventoryPanel/Items");
    items->clear();
    if (!campaign_ || !campaign_->selected())
    {
        return;
    }
    const auto &m = campaign_->member(campaign_->selected());
    required_node<Label>(*this, "InventoryPanel/Header")
        .set_text(i18n::format("{name} / {class} / party {gold} gp",
                               {{"name", String::utf8(m.character.sheet().name.c_str())},
                                {"class", i18n::text(m.character.sheet().character_class)},
                                {"gold", coins(campaign_->state().wealth, Coin::gold)}}));
    for (const auto &item : m.character.inventory().items())
    {
        // Detect Magic marks magic items, as the original's item lists did.
        const bool sensed =
            campaign_->detecting_magic() && campaign_->magical(campaign_->selected(), item.id);
        items->add_item(
            (sensed ? String("* ") : String()) +
            (std::find(m.equipped.begin(), m.equipped.end(), item.id) != m.equipped.end()
                 ? i18n::text("Equipped / ")
                 : String()) +
            inventory_label(m, item));
    }
    if (items->get_item_count())
    {
        items->select(0);
    }
    required_node<Button>(*this, "InventoryPanel/Equip").set_disabled(!items->get_item_count());
    required_node<Button>(*this, "InventoryPanel/Unequip").set_disabled(!items->get_item_count());
    // As in the original, only a shop identifies items, and it takes any item.
    auto &identify = required_node<Button>(*this, "InventoryPanel/Identify");
    identify.set_visible(session_ && session_->snapshot().phase == TourPhase::shopping);
    identify.set_disabled(!items->get_item_count());
    // Any member who can cast Identify may cast it on the selected item.
    auto &spell = required_node<Button>(*this, "InventoryPanel/CastIdentify");
    spell.set_visible(campaign_->identifier().has_value());
    spell.set_disabled(!items->get_item_count());
    required_node<Button>(*this, "InventoryPanel/UseItem").hide();
    required_node<Label>(*this, "InventoryPanel/Status")
        .set_text(i18n::text(N_("Inventory is empty. Visit a shop to buy equipment.")));
    if (items->get_item_count())
    {
        inventory_selected(0);
    }
}

void RolfTourView::inventory_selected(std::int64_t index)
{
    if (!campaign_ || !campaign_->selected())
    {
        return;
    }
    const auto &m = campaign_->member(campaign_->selected());
    const auto items = m.character.inventory().items();
    if (index < 0 || static_cast<std::size_t>(index) >= items.size())
    {
        return;
    }
    // The Manual and some potions are used outside combat (MAGIC-3), and a
    // scroll whose spell the member can read or copy now (MAGIC-6).
    const auto &item = items[index];
    const bool scroll = opengold::rules::parse_scroll_key(item.definition_id).has_value();
    required_node<Button>(*this, "InventoryPanel/UseItem")
        .set_visible(opengold::srd5::usable_outside_combat(item.definition_id) ||
                     (scroll && (!campaign_->scroll_actions(campaign_->selected(), item.id).empty() ||
                                 !campaign_->copyable_spells(campaign_->selected(), item.id).empty())));
    try
    {
        required_node<Label>(*this, "InventoryPanel/Status")
            .set_text(i18n::text(
                opengold::srd5::equipment_note(m.character.sheet(),
                                               opengold::rules_definition(m, items[index]))));
    }
    catch (const std::exception &e)
    {
        required_node<Label>(*this, "InventoryPanel/Status").set_text(i18n::text(e.what()));
    }
}

void RolfTourView::equip_item(bool equip)
{
    try
    {
        const auto selection =
            required_node<ItemList>(*this, "InventoryPanel/Items").get_selected_items();
        if (selection.is_empty())
        {
            throw std::runtime_error("Select an item first.");
        }
        const auto id = campaign_->selected();
        // Copies: equipping can replace the member's inventory, and with it
        // anything still pointing into it (Effective C++ Item 28).
        std::uint64_t item{};
        std::string definition;
        {
            const auto items = campaign_->member(id).character.inventory().items();
            if (selection[0] < 0 || static_cast<std::size_t>(selection[0]) >= items.size())
            {
                throw std::runtime_error("Select an existing item.");
            }
            item = items[selection[0]].id;
            definition = opengold::rules_definition(campaign_->member(id), items[selection[0]]);
        }
        std::optional<opengold::rules::HazardAttackResult> burn;
        if (equip)
        {
            burn = campaign_->equip(id, item);
        }
        else
        {
            campaign_->unequip(id, item);
        }
        refresh_inventory();
        required_node<ItemList>(*this, "InventoryPanel/Items").select(selection[0]);
        refresh();
        // An alignment-bound sword refuses a wielder of another alignment (#267).
        if (burn)
        {
            required_node<Label>(*this, "InventoryPanel/Status")
                .set_text(i18n::format(
                    N_("The sword will not stay in {name}'s hand and burns for {damage} Force damage."),
                    {{"name",
                      String::utf8(campaign_->member(id).character.sheet().name.c_str())},
                     {"damage", burn->damage}}));
            return;
        }
        required_node<Label>(*this, "InventoryPanel/Status")
            .set_text(equip ? i18n::text("Equipped.") + " " +
                                  i18n::text(opengold::srd5::equipment_note(
                                      campaign_->member(id).character.sheet(), definition))
                            : i18n::text("Item unequipped."));
    }
    catch (const std::exception &e)
    {
        required_node<Label>(*this, "InventoryPanel/Status").set_text(i18n::text(e.what()));
    }
}

void RolfTourView::identify_item()
{
    try
    {
        const auto selection =
            required_node<ItemList>(*this, "InventoryPanel/Items").get_selected_items();
        if (selection.is_empty())
        {
            throw std::runtime_error("Select an item first.");
        }
        const auto id = campaign_->selected();
        const auto items = campaign_->member(id).character.inventory().items();
        if (selection[0] < 0 || static_cast<std::size_t>(selection[0]) >= items.size())
        {
            throw std::runtime_error("Select an existing item.");
        }
        const auto item = items[selection[0]].id;
        const bool revealed = campaign_->identify_at_shop(id, item);
        refresh_inventory();
        required_node<ItemList>(*this, "InventoryPanel/Items").select(selection[0]);
        refresh();
        const auto name = campaign_->member(id).character.inventory().find(item)->get().name;
        required_node<Label>(*this, "InventoryPanel/Status")
            .set_text(i18n::format(revealed ? N_("The shop identifies it: {item}.")
                                            : N_("The shop learns nothing new about {item}."),
                                   {{"item", i18n::text(name)}}));
    }
    catch (const std::exception &e)
    {
        required_node<Label>(*this, "InventoryPanel/Status").set_text(i18n::text(e.what()));
    }
}

// The town check's Cast menu and Identify (#262): the member, made a Bard with
// both spells, casts Detect Magic from the Cast menu, sees a hidden Long Sword
// +2 marked, and identifies it from the inventory. One step a frame, so each
// capture shows the step before it.
void RolfTourView::check_identification_spells()
{
    auto &items = required_node<ItemList>(*this, "InventoryPanel/Items");
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
        caster.character = opengold::Character(*opengold::srd5::character_rules(), draft,
                                               caster.character.appearance());
        opengold::por::Equipment sword;
        sword.stored.type = 36;
        sword.stored.magic_bonus = 2;
        sword.stored.name_components = {0, 163, 36};
        sword.stored.hidden_name_words = 6;
        sword.stored.stack_size = 1;
        sword.bonuses.weapon_to_hit = 2;
        identify_check_sword_ = inventory.add({.definition_id = opengold::equipment_conversion(sword),
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
        const auto protection = inventory.add({.definition_id = opengold::equipment_conversion(ring),
                                               .name = ring.label(),
                                               .quantity = 1,
                                               .original_type = 93});
        opengold::por::Equipment parchment;
        parchment.stored.type = 61;
        parchment.stored.magic_bonus = 1;
        parchment.stored.effect_codes = {14, 47, 0};
        parchment.stored.stack_size = 1;
        parchment.decoded_name = "Magic User Scroll With 2 Spells";
        const auto scroll = inventory.add({.definition_id = opengold::equipment_conversion(parchment),
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
        if (!required_node<Button>(*this, "InventoryPanel/CastIdentify").is_visible())
        {
            throw std::runtime_error("A Bard must be offered Identify in the inventory");
        }
        required_node<Button>(*this, "InventoryPanel/Close").emit_signal("pressed");
        required_node<Button>(*this, "Cast").emit_signal("pressed");
        auto &action = required_node<OptionButton>(*this, "CastPanel/Action");
        for (int index = 0; index < action.get_item_count(); ++index)
        {
            if (action.get_item_metadata(index) == Variant(String("detect_magic")))
            {
                action.select(index);
                cast_action_selected(index);
            }
        }
        if (action.get_selected_metadata() != Variant(String("detect_magic")) ||
            required_node<OptionButton>(*this, "CastPanel/Target").is_visible())
        {
            throw std::runtime_error("The Cast menu must offer Detect Magic for the party");
        }
        shop_check_stage_ = 11;
        return;
    }
    if (shop_check_stage_ == 11)
    {
        capture_frame("phlan-cast");
        required_node<Button>(*this, "CastPanel/Use").emit_signal("pressed");
        if (!campaign_->detecting_magic() ||
            !required_node<Label>(*this, "CastPanel/Status").get_text().contains("ten minutes"))
        {
            throw std::runtime_error("Detect Magic from the Cast menu must mark magic items");
        }
        shop_check_stage_ = 12;
        return;
    }
    if (shop_check_stage_ == 12)
    {
        capture_frame("phlan-cast-done");
        required_node<Button>(*this, "CastPanel/Close").emit_signal("pressed");
        required_node<Button>(*this, "Inventory").emit_signal("pressed");
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
        required_node<Button>(*this, "InventoryPanel/CastIdentify").emit_signal("pressed");
        if (!items.get_item_text(sword_row()).contains("+2") ||
            !required_node<Label>(*this, "InventoryPanel/Status").get_text().contains("Identify"))
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
    const int constitution =
        campaign_->member(campaign_->selected()).character.sheet().scores[opengold::rules::Ability::constitution];
    items.select(static_cast<int>(book - carried.begin()));
    inventory_selected(book - carried.begin());
    auto &use = required_node<Button>(*this, "InventoryPanel/UseItem");
    if (!use.is_visible())
    {
        throw std::runtime_error("The inventory must offer Use for the Manual");
    }
    use.emit_signal("pressed");
    if (campaign_->member(campaign_->selected()).character.sheet().scores[opengold::rules::Ability::constitution] !=
            constitution + 2 ||
        !required_node<Label>(*this, "InventoryPanel/Status").get_text().contains("Constitution"))
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
    required_node<Button>(*this, "InventoryPanel/Equip").emit_signal("pressed");
    if (campaign_->profile(campaign_->selected()).armor_class != armor_class + 2 ||
        campaign_->profile(campaign_->selected()).item_save_bonus != 2 ||
        !required_node<Label>(*this, "InventoryPanel/Status").get_text().contains("Rings of Protection"))
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
    if (!required_node<Button>(*this, "InventoryPanel/UseItem").is_visible())
    {
        throw std::runtime_error("The inventory must offer Use for a scroll the Bard can read");
    }
    required_node<Button>(*this, "InventoryPanel/UseItem").emit_signal("pressed");
    auto &spell_choice = required_node<OptionButton>(*this, "CastPanel/Action");
    if (!required_node<Control>(*this, "CastPanel").is_visible() ||
        spell_choice.get_item_count() != 1 ||
        spell_choice.get_selected_metadata() != Variant(String("0:friends")))
    {
        throw std::runtime_error("Use on a scroll must open the Cast menu with its spell");
    }
    required_node<Button>(*this, "CastPanel/Use").emit_signal("pressed");
    const auto read = campaign_->member(campaign_->selected()).character.inventory().find(parchment_id);
    if (!read || read->get().definition_id != "scroll:arcane:fireball" ||
        !required_node<Label>(*this, "CastPanel/Status").get_text().contains("reads"))
    {
        throw std::runtime_error("Reading Friends must leave Fireball on the scroll");
    }
    required_node<Button>(*this, "CastPanel/Close").emit_signal("pressed");
    required_node<Button>(*this, "Inventory").emit_signal("pressed");
    campaign_->restore(std::move(*identify_check_retained_));
    identify_check_retained_.reset();
    refresh();
    refresh_inventory();
    required_node<Button>(*this, "InventoryPanel/Close").emit_signal("pressed");
    required_node<Button>(*this, "LeaveShop").emit_signal("pressed");
    shop_check_stage_ = 3;
}

void RolfTourView::use_inventory_item()
{
    try
    {
        const auto selection =
            required_node<ItemList>(*this, "InventoryPanel/Items").get_selected_items();
        if (selection.is_empty())
        {
            throw std::runtime_error("Select an item first.");
        }
        const auto id = campaign_->selected();
        const auto items = campaign_->member(id).character.inventory().items();
        if (selection[0] < 0 || static_cast<std::size_t>(selection[0]) >= items.size())
        {
            throw std::runtime_error("Select an existing item.");
        }
        const auto item = items[selection[0]];
        const auto name = String::utf8(campaign_->member(id).character.sheet().name.c_str());
        // A scroll's spells are read, or copied, from the Cast menu (MAGIC-6).
        if (opengold::rules::parse_scroll_key(item.definition_id))
        {
            scroll_item_ = item.id;
            required_node<Control>(*this, "InventoryPanel").hide();
            required_node<Label>(*this, "CastPanel/Status").set_text(String());
            refresh_cast();
            required_node<Control>(*this, "CastPanel").show();
            return;
        }
        String result;
        if (item.definition_id == "manual_of_bodily_health")
        {
            campaign_->read_manual(id, item.id);
            result = i18n::format(
                N_("{name} reads the Manual of Bodily Health: Constitution rises by 2."),
                {{"name", name}});
        }
        else
        {
            campaign_->drink_potion(id, item.id);
            result = i18n::format(N_("{name} drinks the {item}."),
                                  {{"name", name}, {"item", i18n::text(item.name)}});
        }
        refresh_inventory();
        refresh();
        required_node<Label>(*this, "InventoryPanel/Status").set_text(result);
    }
    catch (const std::exception &e)
    {
        required_node<Label>(*this, "InventoryPanel/Status").set_text(i18n::text(e.what()));
    }
}

// Reads the scroll's spell `id` on `patient`, or copies one ("copy:N") into
// the member's spellbook (MAGIC-6).
void RolfTourView::read_or_copy_scroll(const std::string &id, opengold::MemberId patient,
                                       const String &label)
{
    const auto caster = campaign_->selected();
    const auto name = String::utf8(campaign_->member(caster).character.sheet().name.c_str());
    String result;
    if (id.starts_with("copy:"))
    {
        campaign_->copy_scroll_spell(caster, *scroll_item_, static_cast<unsigned>(id[5] - '0'));
        result = i18n::format(N_("{name} copies the spell into the spellbook."), {{"name", name}});
    }
    else
    {
        result = campaign_->read_scroll(caster, *scroll_item_, patient, id)
                     ? i18n::format(N_("{name} reads {spell} from the scroll."),
                                    {{"name", name}, {"spell", label}})
                     : i18n::format(N_("{name} fails to read {spell}, and it is lost."),
                                    {{"name", name}, {"spell", label}});
    }
    refresh_cast();
    refresh();
    required_node<Label>(*this, "CastPanel/Status").set_text(result);
}

void RolfTourView::cast_identify()
{
    try
    {
        const auto selection =
            required_node<ItemList>(*this, "InventoryPanel/Items").get_selected_items();
        if (selection.is_empty())
        {
            throw std::runtime_error("Select an item first.");
        }
        const auto holder = campaign_->selected();
        const auto items = campaign_->member(holder).character.inventory().items();
        if (selection[0] < 0 || static_cast<std::size_t>(selection[0]) >= items.size())
        {
            throw std::runtime_error("Select an existing item.");
        }
        const auto item = items[selection[0]].id;
        const auto caster = campaign_->identifier();
        if (!caster)
        {
            throw std::runtime_error("That spell or feature cannot be used now");
        }
        const bool revealed = campaign_->cast_identify(*caster, holder, item);
        refresh_inventory();
        required_node<ItemList>(*this, "InventoryPanel/Items").select(selection[0]);
        refresh();
        const auto name = campaign_->member(holder).character.inventory().find(item)->get().name;
        const auto caster_name =
            String::utf8(campaign_->member(*caster).character.sheet().name.c_str());
        required_node<Label>(*this, "InventoryPanel/Status")
            .set_text(i18n::format(revealed
                                       ? N_("{name} casts Identify: {item}.")
                                       : N_("{name} casts Identify and learns nothing new about {item}."),
                                   {{"name", caster_name}, {"item", i18n::text(name)}}));
    }
    catch (const std::exception &e)
    {
        required_node<Label>(*this, "InventoryPanel/Status").set_text(i18n::text(e.what()));
    }
}

// The Cast menu: the selected member's spells and features outside combat,
// the same ones the Camp dialog's Cast / Use offers.
void RolfTourView::cast_menu()
{
    auto *panel = &required_node<Control>(*this, "CastPanel");
    if (panel->is_visible())
    {
        panel->hide();
        return;
    }
    if (!session_ || !campaign_ || !campaign_->selected() || campaign_->in_combat())
    {
        return;
    }
    scroll_item_.reset();
    required_node<Label>(*this, "CastPanel/Status").set_text(String());
    refresh_cast();
    panel->show();
}

void RolfTourView::refresh_cast()
{
    auto *action = &required_node<OptionButton>(*this, "CastPanel/Action");
    auto *target = &required_node<OptionButton>(*this, "CastPanel/Target");
    action->clear();
    target->clear();
    if (!campaign_ || !campaign_->selected())
    {
        return;
    }
    const auto caster = campaign_->selected();
    const auto &member = campaign_->member(caster);
    // A scroll read or copied from: the member's own, while it still holds a
    // spell (MAGIC-6).
    if (scroll_item_ && !member.character.inventory().find(*scroll_item_))
    {
        scroll_item_.reset();
    }
    const auto name = String::utf8(member.character.sheet().name.c_str());
    required_node<Label>(*this, "CastPanel/Header")
        .set_text(scroll_item_ ? i18n::format(N_("{name}: read a scroll"), {{"name", name}})
                               : i18n::format(N_("{name}: cast a spell or use a feature"),
                                              {{"name", name}}));
    cast_options_.clear();
    const auto add = [&](const String &label, const std::string &id, bool whole_party)
    {
        action->add_item(label);
        action->set_item_metadata(action->get_item_count() - 1, String::utf8(id.c_str()));
        cast_options_.push_back({id, whole_party});
    };
    if (scroll_item_)
    {
        for (const auto &offered : campaign_->scroll_actions(caster, *scroll_item_))
        {
            add(i18n::render(offered.label), offered.id, offered.whole_party);
        }
        // Copying takes a bottle of Magic Ink of the spell's level or higher
        // and two hours a spell level.
        for (const auto &copy : campaign_->copyable_spells(caster, *scroll_item_))
        {
            add(i18n::format(
                    N_("Copy {spell} into the spellbook (Magic Ink of level {level} or higher, {hours} hours)"),
                    {{"spell", i18n::text(opengold::srd5::spell_label(copy.spell))},
                     {"level", static_cast<int>(copy.level)},
                     {"hours", static_cast<int>(2 * copy.level)}}),
                "copy:" + std::to_string(copy.entry), true);
        }
    }
    else
    {
        for (const auto &offered : campaign_->camp_actions(caster))
        {
            add(i18n::render(offered.label), offered.id, offered.whole_party);
        }
    }
    for (const auto id : campaign_->state().slots)
    {
        if (id)
        {
            const int index = target->get_item_count();
            target->add_item(String::utf8(campaign_->member(id).character.sheet().name.c_str()));
            target->set_item_metadata(index, static_cast<int64_t>(id));
        }
    }
    const bool offered = action->get_item_count() > 0;
    required_node<Button>(*this, "CastPanel/Use").set_disabled(!offered);
    if (!offered)
    {
        required_node<Label>(*this, "CastPanel/Status")
            .set_text(i18n::text(N_("Nothing to cast or use outside combat.")));
    }
    cast_action_selected(action->get_selected());
}

void RolfTourView::cast_action_selected(std::int64_t index)
{
    // A whole-party action, and copying a spell, choose no member.
    const bool party = index >= 0 && static_cast<std::size_t>(index) < cast_options_.size() &&
                       cast_options_[static_cast<std::size_t>(index)].whole_party;
    required_node<OptionButton>(*this, "CastPanel/Target").set_visible(!party);
}

void RolfTourView::cast_use()
{
    auto *status = &required_node<Label>(*this, "CastPanel/Status");
    try
    {
        auto *action = &required_node<OptionButton>(*this, "CastPanel/Action");
        auto *target = &required_node<OptionButton>(*this, "CastPanel/Target");
        if (!campaign_ || campaign_->in_combat() || action->get_selected() < 0 ||
            target->get_selected() < 0)
        {
            return;
        }
        const String id = action->get_selected_metadata();
        const auto patient =
            static_cast<unsigned>(static_cast<int64_t>(target->get_selected_metadata()));
        if (scroll_item_)
        {
            read_or_copy_scroll(std::string(id.utf8().get_data()), patient,
                                action->get_item_text(action->get_selected()));
            return;
        }
        const auto result = use_outside_combat(
            campaign_->selected(), patient, id, action->get_item_text(action->get_selected()),
            target->is_visible() ? target->get_item_text(target->get_selected()) : String());
        if (!result)
        {
            return;
        }
        refresh_cast();
        refresh();
        status->set_text(*result);
    }
    catch (const std::exception &e)
    {
        status->set_text(i18n::text(e.what()));
    }
}

void RolfTourView::party_selected(std::int64_t index)
{
    if (index < 0 || index >= 8 || !session_ || !campaign_ || !campaign_->state().slots[index])
    {
        return;
    }
    const auto slot = static_cast<unsigned>(index);
    if (session_->snapshot().phase == TourPhase::shopping)
    {
        // In a shop, clicking a member chooses the buyer instead of opening a sheet.
        select_buyer(slot);
        return;
    }
    if (session_->can_select_member())
    {
        campaign_->select(opengold::PartySlot{slot});
    }
    if (required_node<Control>(*this, "CastPanel").is_visible())
    {
        // The Cast menu follows the chosen member, and their own spells: a
        // scroll belongs to the member who opened it.
        scroll_item_.reset();
        refresh_cast();
        refresh();
        return;
    }
    if (embedded_party_)
    {
        emit_signal("party_member_selected", slot);
    }
    else
    {
        const auto &m = campaign_->member(campaign_->state().slots[slot]);
        const auto &s = m.character.sheet();
        std::string text = s.name + "\nLevel " + std::to_string(s.level) + " " + s.race + " " +
                           s.gender + " " + s.character_class + "\n" + s.alignment + " / " +
                           s.background + "\nXP " + std::to_string(m.experience) + "\nAC " +
                           std::to_string(campaign_->profile(m.id).armor_class) + " / HP " +
                           std::to_string(m.vitals.hit_points) + "/" +
                           std::to_string(s.hit_points) + "\n" + m.vitals.description + "\n\n";
        for (unsigned i = 0; i < 6; ++i)
        {
            const auto ability = opengold::rules::all_abilities[i];
            text += std::string(i18n::ability_names[i]) + ": " + std::to_string(s.scores[ability]) +
                    " / Save " + std::to_string(s.saving_throws[ability]) + "\n";
        }
        required_node<RichTextLabel>(*this, "MemberSheet/Text")
            .set_text(String::utf8(text.c_str()));
        required_node<Window>(*this, "MemberSheet").popup_centered();
    }
    refresh();
}

void RolfTourView::select_buyer(unsigned slot)
{
    if (!session_ || !campaign_ || !session_->can_select_member() || slot >= 8 ||
        !campaign_->state().slots[slot])
    {
        return;
    }
    campaign_->select(opengold::PartySlot{slot});
    refresh();
}

std::vector<unsigned> RolfTourView::occupied_slots() const
{
    std::vector<unsigned> slots;
    for (unsigned slot = 0; slot < 8; ++slot)
    {
        if (campaign_->state().slots[slot])
        {
            slots.push_back(slot);
        }
    }
    return slots;
}

void RolfTourView::buyer_key(Key keycode)
{
    const auto slots = occupied_slots();
    if (slots.empty())
    {
        return;
    }
    if (keycode == Key::KEY_TAB)
    {
        // Cycle through the member rows in display order, wrapping at the end.
        const auto current =
            std::find(slots.begin(), slots.end(), campaign_->state().selected_slot.index);
        const auto next =
            current == slots.end() || current + 1 == slots.end() ? slots.begin() : current + 1;
        select_buyer(*next);
        return;
    }
    // Number keys pick the nth visible member row.
    const auto row = static_cast<std::size_t>(keycode) - static_cast<std::size_t>(Key::KEY_1);
    if (row < slots.size())
    {
        select_buyer(slots[row]);
    }
}

void RolfTourView::close_sheet()
{
    required_node<Window>(*this, "MemberSheet").hide();
}

void RolfTourView::level_up_requested(int slot)
{
    if (embedded_party_ && campaign_ && session_ && session_->can_leave())
    {
        emit_signal("level_up_requested", campaign_->state().slots.at(slot));
    }
}

void RolfTourView::movement(ExplorationCommand command)
{
    if (!session_)
    {
        return;
    }
    if (session_->explore(command))
    {
        refresh();
    }
    else if (session_->snapshot().phase == TourPhase::completed)
    {
        required_node<Label>(*this, "Movement").set_text(i18n::text(N_("The way is blocked")));
    }
}

void RolfTourView::_input(const Ref<InputEvent> &event)
{
    presentation::run_guarded(*this,
                              [&]
                              {
                                  respond_to_input(event);
                              });
}

void RolfTourView::respond_to_input(const Ref<InputEvent> &event)
{
    if (required_node<Window>(*this, "RestDialog").is_visible())
    {
        return;
    }
    const Ref<InputEventKey> key = event;
    if (key.is_null() || !key->is_pressed() || key->is_echo())
    {
        return;
    }
    if (shown_monster_picture_)
    {
        // As in the original, any key leaves the monster close-up for combat.
        dismiss_monster_picture();
        get_viewport()->set_input_as_handled();
        return;
    }
    if (required_node<Control>(*this, "InventoryPanel").is_visible() ||
        required_node<Control>(*this, "CastPanel").is_visible())
    {
        return;
    }
    if (required_node<LineEdit>(*this, "Answer").has_focus() &&
        key->get_keycode() != Key::KEY_ENTER)
    {
        return;
    }
    if (session_ && campaign_ && session_->snapshot().phase == TourPhase::shopping &&
        (key->get_keycode() == Key::KEY_TAB ||
         (key->get_keycode() >= Key::KEY_1 && key->get_keycode() <= Key::KEY_8)))
    {
        buyer_key(key->get_keycode());
        get_viewport()->set_input_as_handled();
        return;
    }
    if (session_ &&
        (session_->snapshot().choices.size() > 1 ||
         session_->snapshot().phase == TourPhase::shopping) &&
        key->get_keycode() != Key::KEY_ENTER)
    {
        return;
    }
    switch (key->get_keycode())
    {
    case Key::KEY_ENTER:
        next();
        break;
    case Key::KEY_LEFT:
        left();
        break;
    case Key::KEY_RIGHT:
        right();
        break;
    case Key::KEY_UP:
        forward();
        break;
    case Key::KEY_DOWN:
        movement(ExplorationCommand::turn_around);
        break;
    case Key::KEY_L:
        look();
        break;
    case Key::KEY_C:
        camp();
        break;
    case Key::KEY_M:
        // M for the original's Magic menu; unavailable while the Cast button is.
        if (!required_node<Button>(*this, "Cast").is_disabled())
        {
            cast_menu();
        }
        break;
    default:
        return;
    }
    get_viewport()->set_input_as_handled();
}

void RolfTourView::_process(double delta)
{
    presentation::run_guarded(*this,
                              [&]
                              {
                                  advance_frame(delta);
                              });
}

void RolfTourView::advance_frame(double delta)
{
    if (Engine::get_singleton()->is_editor_hint())
    {
        return;
    }
    if (rest_save_open_)
    {
        auto *save = Object::cast_to<Window>(get_parent()->get_node_or_null("SaveSlots"));
        if (!save || !save->is_visible())
        {
            rest_save_open_ = false;
            refresh_rest();
        }
    }
    if (session_)
    {
        session_->advance(checking_ ? 0.3 : delta);
        if (session_->snapshot().footsteps != played_footsteps_)
        {
            played_footsteps_ = session_->snapshot().footsteps;
            if (!checking_)
            {
                required_node<AudioStreamPlayer>(*this, "Footstep").play();
            }
        }
        if (session_->snapshot().revision != shown_revision_)
        {
            refresh();
        }
        if (shown_monster_picture_)
        {
            monster_picture_seconds_ += delta;
            required_node<Control>(*this, "TourVisuals").queue_redraw();
        }
    }
    if (mastery_rest_check_)
    {
        check_mastery_rest_controls();
        return;
    }
    if (rest_check_)
    {
        check_rest_controls();
        return;
    }
    if (checking_)
    {
        check_run();
    }
}

namespace
{
String rest_notice(const std::string &resource, const std::string &text)
{
    auto result = i18n::campaign(resource, text);
    // Original dialogue stays in its original-content domain; these appended
    // messages are generated by our campaign host and use the engine catalog.
    for (
        const auto *source :
        {N_("Rest is not allowed here."),
         N_("Short rest complete: one hour passed; eligible members can spend Hit Dice."),
         N_("Long rest complete: eight hours passed; eligible members recovered HP and supported resources."),
         N_("Rest denied: no active member is eligible."),
         N_("The rest was interrupted. Rest again to recover."), N_("Your camp is attacked!"),
         N_("The monsters go on their way."), N_("You get away."),
         N_("Your party flees the battle."), N_("You have lost the fight."), N_("Locked."),
         N_("Wizard locked."), N_("The way to Stojanow Gate is not in the game yet."),
         N_("The way to Cadorna Textile House is not in the game yet."),
         N_("The way to the wilderness is not in the game yet."),
         N_("The way to another area is not in the game yet.")})
    {
        result = result.replace(String::utf8(source), i18n::text(source));
    }
    // The parley's spokesman and the members lost in a flight are named.
    const String speaks = " speaks for the party.", lost = " is left behind and lost.";
    auto lines = result.split("\n");
    for (int n = 0; n < lines.size(); ++n)
    {
        if (lines[n].ends_with(speaks))
        {
            lines[n] =
                i18n::format(N_("{name} speaks for the party."),
                             {{"name", lines[n].substr(0, lines[n].length() - speaks.length())}});
        }
        else if (lines[n].ends_with(lost))
        {
            lines[n] =
                i18n::format(N_("{name} is left behind and lost."),
                             {{"name", lines[n].substr(0, lines[n].length() - lost.length())}});
        }
    }
    return String("\n").join(lines);
}

// Choices the campaign host adds use the engine catalog; the rest are original.
String choice_label(const std::string &resource, const std::string &choice)
{
    for (const auto *host : {N_("Bash"), N_("Pick"), N_("Knock"), N_("Exit")})
    {
        if (choice == host)
        {
            return i18n::text(host);
        }
    }
    return i18n::campaign(resource + "/choices", choice);
}

String coin_list(const opengold::Coins &coins)
{
    static constexpr std::array<const char *, 5> amounts{
        N_("{count} cp"), N_("{count} sp"), N_("{count} ep"), N_("{count} gp"), N_("{count} pp")};
    String result;
    for (std::size_t coin = coins.size(); coin-- > 0;)
    {
        if (!coins[coin])
        {
            continue;
        }
        if (!result.is_empty())
        {
            result += ", ";
        }
        result += i18n::format(amounts[coin], {{"count", coins[coin]}});
    }
    return result;
}

String payment_notice(const CoinPayment &payment)
{
    const auto &coins = payment.coins;
    const auto name = String::utf8(payment.payer.c_str());
    if (coins.change == opengold::Coins{})
    {
        return i18n::format(
            "{name} pays {paid} for {owed}.",
            {{"name", name}, {"paid", coin_list(coins.paid)}, {"owed", coin_list(coins.owed)}});
    }
    return i18n::format("{name} pays {paid} for {owed} and receives {change} in change.",
                        {{"name", name},
                         {"paid", coin_list(coins.paid)},
                         {"owed", coin_list(coins.owed)},
                         {"change", coin_list(coins.change)}});
}

String door_check_notice(const DoorCheck &check)
{
    const auto name = String::utf8(check.member.c_str());
    if (check.method == opengold::DoorMethod::knock)
    {
        return i18n::format("{name} casts Knock, and the lock opens.", {{"name", name}});
    }
    if (check.method == opengold::DoorMethod::pick && check.thieves_tools)
    {
        return i18n::format(
            "{name} tries to pick the lock with Thieves' Tools, which double the chance: Dexterity (Sleight of Hand) {total} (d20 {die}) against DC {dc}.",
            {{"name", name}, {"total", check.total}, {"die", check.die}, {"dc", check.difficulty}});
    }
    if (check.method == opengold::DoorMethod::pick)
    {
        return i18n::format(
            "{name} tries to pick the lock: Dexterity (Sleight of Hand) {total} (d20 {die}) against DC {dc}.",
            {{"name", name}, {"total", check.total}, {"die", check.die}, {"dc", check.difficulty}});
    }
    return i18n::format(
        "{name} tries to force the door: Strength (Athletics) {total} (d20 {die}) against DC {dc}.",
        {{"name", name}, {"total", check.total}, {"die", check.die}, {"dc", check.difficulty}});
}

// Script dialogue followed by the coins the party's purses exchanged for it
// and any checks made against a locked door.
String event_dialogue(const std::string &resource, const TourSnapshot &s)
{
    // Our campaign host's own dialogue, such as a training hall's, is
    // translated from its messages; the script's text is original.
    auto result =
        s.host_dialogue.empty() ? rest_notice(resource, s.dialogue) : i18n::render(s.host_dialogue);
    for (const auto &payment : s.payments)
    {
        result += "\n" + payment_notice(payment);
    }
    for (const auto &check : s.door_checks)
    {
        result += "\n" + door_check_notice(check);
    }
    if (!s.door_checks.empty())
    {
        result +=
            "\n" + i18n::text(s.door_opened ? N_("The door opens.") : N_("The door stays locked."));
    }
    return result;
}
} // namespace

void RolfTourView::sync_monster_picture()
{
    const auto *picture = session_ ? session_->monster_picture() : nullptr;
    if (picture == shown_monster_picture_)
    {
        return;
    }
    forget_monster_picture();
    shown_monster_picture_ = picture;
    if (!picture)
    {
        return;
    }
    for (const auto &frame : *picture)
    {
        monster_frames_.push_back(presentation::image_texture(frame.image));
        monster_delays_.push_back(frame.delay);
    }
}

void RolfTourView::forget_monster_picture()
{
    shown_monster_picture_ = nullptr;
    monster_frames_.clear();
    monster_delays_.clear();
    monster_picture_seconds_ = 0;
    monster_picture_check_frames_ = 0;
}

std::size_t RolfTourView::current_monster_frame() const
{
    return presentation::monster_frame_at(monster_delays_, monster_picture_seconds_);
}

void RolfTourView::dismiss_monster_picture()
{
    if (session_ && session_->start_encounter())
    {
        refresh();
    }
}

void RolfTourView::refresh()
{
    layout();
    sync_monster_picture();
    if (session_ && rendered_sprite_id_ != session_->snapshot().sprite_id)
    {
        for (unsigned n = 0; n < 3; ++n)
        {
            const auto &source = session_->sprites()[n];
            sprites_[n] = presentation::image_texture(source);
        }
        rendered_sprite_id_ = session_->snapshot().sprite_id;
    }
    if (session_ && session_->snapshot().visited.any() &&
        (!rendered_pose_ || *rendered_pose_ != session_->snapshot().pose ||
         rendered_picture_revision_ != session_->snapshot().picture_revision))
    {
        try
        {
            const auto pose = session_->snapshot().pose;
            const auto source =
                session_->picture() ? *session_->picture() : session_->observe_view();
            const auto image = presentation::rgba_image(source);
            if (wall_view_.is_null())
            {
                wall_view_ = ImageTexture::create_from_image(image);
            }
            else
            {
                wall_view_->set_image(image);
            }
            rendered_pose_ = pose;
            rendered_picture_revision_ = session_->snapshot().picture_revision;
        }
        catch (const std::exception &error)
        {
            session_.reset();
            forget_monster_picture();
            error_ = String::utf8(error.what());
        }
    }
    const bool loaded = session_.has_value();
    static const TourSnapshot none;
    const TourSnapshot &s = loaded ? session_->snapshot() : none;
    refresh_rest();
    const bool waiting = loaded && s.phase == TourPhase::awaiting_continue;
    const bool completed = loaded && s.phase == TourPhase::completed;
    required_node<Button>(*this, "SaveGame").set_disabled(!completed);
    required_node<Button>(*this, "LoadGame").set_disabled(!completed);
    const bool faulted = !loaded || s.phase == TourPhase::faulted;
    const bool shopping = loaded && s.phase == TourPhase::shopping;
    const bool answer = loaded && s.phase == TourPhase::awaiting_input;
    const bool multiple = waiting && s.choices.size() > 1;
    const String district = i18n::text(district_name(s.area_id));
    required_node<Label>(*this, "Location")
        .set_text(i18n::format("{district} / {direction} view",
                               {{"district", district},
                                {"direction", i18n::text(direction_name[index(s.pose.facing)])}}));
    required_node<Label>(*this, "MapTitle")
        .set_text(i18n::format("{district}    N ↑", {{"district", district}}));
    // Rolf's welcome names the opening tour; afterwards the title names the area.
    required_node<Label>(*this, "Title")
        .set_text(s.tour_finished
                      ? i18n::format("OPENGOLDBOX  /  {district}", {{"district", district}})
                      : i18n::text("OPENGOLDBOX  /  Rolf's welcome"));
    required_node<Label>(*this, "Coordinates")
        .set_text(i18n::format("Party ({x}, {y})   {direction}",
                               {{"x", s.pose.x},
                                {"y", s.pose.y},
                                {"direction", i18n::text(direction_name[index(s.pose.facing)])}}));
    required_node<Label>(*this, "Speaker")
        .set_text(i18n::text(faulted           ? N_("Unable to continue")
                             : shopping        ? N_("Shop / select an item")
                             : s.tour_finished ? district_name(s.area_id)
                                               : N_("Rolf  /  Council guide")));
    if (shopping && campaign_ && campaign_->selected())
    {
        const auto &buyer = campaign_->member(campaign_->selected());
        required_node<Label>(*this, "Speaker")
            .set_text(i18n::format(
                "Shop / buying for {name}: party {gold} gp, {count}/16 items",
                {{"name", String::utf8(buyer.character.sheet().name.c_str())},
                 {"gold", coins(campaign_->state().wealth, Coin::gold)},
                 {"count", static_cast<int64_t>(buyer.character.inventory().items().size())}}));
    }
    const auto resource =
        "por/area/" + std::to_string(s.area_id) + "/script/" + std::to_string(s.script_id);
    required_node<RichTextLabel>(*this, "Dialogue")
        .set_text(
            faulted
                ? (loaded ? i18n::text(s.diagnostic) : error_) + "\n" +
                      i18n::text(
                          "Restart with --reset-game-path to choose your Pool of Radiance data folder.")
            : shown_monster_picture_ ? i18n::text("Press any key to fight.")
            : s.dialogue.empty() ? (s.tour_finished ? String() : i18n::text("Following Rolf..."))
                                 : event_dialogue(resource + "/dialogue", s));
    required_node<Button>(*this, "Continue")
        .set_disabled(!waiting && !shopping && !answer && !shown_monster_picture_);
    required_node<Button>(*this, "Continue")
        .set_text(i18n::text(shopping   ? N_("Buy [Enter]")
                             : multiple ? N_("Choose [Enter]")
                             : answer   ? N_("Submit [Enter]")
                                        : N_("Continue [Enter]")));
    required_node<Button>(*this, "Continue").set_visible(!completed);
    required_node<Label>(*this, "Progress")
        .set_text(faulted                 ? i18n::text("Stopped")
                  : shopping && campaign_ ? i18n::text(N_("Tab or number keys: choose the buyer"))
                  : s.tour_finished || waiting ? String()
                                               : i18n::text("Following the guide"));
    required_node<Label>(*this, "Movement")
        .set_text(i18n::text(completed ? N_("Explore  /  arrow keys") : N_("Movement paused")));
    for (const char *name : {"Left", "Forward", "Right", "Look", "Camp", "Cast"})
    {
        required_node<Button>(*this, name).set_disabled(!completed);
    }
    required_node<Button>(*this, "LeaveShop").set_visible(shopping);
    required_node<LineEdit>(*this, "Answer").set_visible(answer);
    required_node<LineEdit>(*this, "Answer").set_max_length(s.number_input ? 6 : 40);
    auto *choices = &required_node<ItemList>(*this, "Choices");
    choices->set_visible(shopping || multiple);
    if (displayed_ticket_ != s.continue_ticket)
    {
        choices->clear();
        if (shopping)
        {
            for (const auto &item : session_->shop_stock())
            {
                // Stock without an equipment conversion stays for sale, disclosed.
                const bool usable =
                    !opengold::equipment_conversion(item).starts_with("por:unsupported:");
                choices->add_item(i18n::format(
                    usable ? N_("{item} / {price} gp")
                           : N_("{item} / {price} gp / cannot be equipped"),
                    {{"item", i18n::text(item.label())}, {"price", item.stored.value}}));
            }
        }
        else if (!s.host_choices.empty())
        {
            for (const auto &c : s.host_choices)
            {
                choices->add_item(i18n::render(c));
            }
        }
        else
        {
            for (const auto &c : s.choices)
            {
                choices->add_item(s.dialogue == "Choose a party member."
                                      ? String::utf8(c.c_str())
                                      : choice_label(resource, c));
            }
        }
        if (choices->get_item_count())
        {
            choices->select(0);
        }
        if (shopping || multiple)
        {
            choices->grab_focus();
        }
        if (answer)
        {
            required_node<LineEdit>(*this, "Answer").clear();
            required_node<LineEdit>(*this, "Answer").grab_focus();
        }
        displayed_ticket_ = s.continue_ticket;
    }
    auto &dialogue_layout = required_node<Node>(*this, "DialogueLayout");
    dialogue_layout.call("play", shopping   ? "shopping"
                                 : multiple ? "multiple"
                                 : answer   ? "answer"
                                            : "normal");
    dialogue_layout.call("advance", 0);
    if (shopping)
    {
        required_node<RichTextLabel>(*this, "Dialogue")
            .set_text(s.diagnostic.empty() ? i18n::text("Prices are per listed item or bundle.")
                                           : i18n::text(s.diagnostic));
    }
    if (answer && !s.diagnostic.empty())
    {
        required_node<RichTextLabel>(*this, "Dialogue")
            .add_text("\n" + String::utf8(s.diagnostic.c_str()));
    }
    if (waiting && !s.diagnostic.empty())
    {
        required_node<RichTextLabel>(*this, "Dialogue")
            .set_text(i18n::text(s.diagnostic) + "\n" + event_dialogue(resource + "/dialogue", s));
    }
    if (loaded)
    {
        const auto &p = session_->party();
        required_node<Label>(*this, "Party")
            .set_text(
                i18n::format("Fighter / Level 1 / HP {current}/{maximum}\n{gold} gp / {items}",
                             {{"current", p.hit_points},
                              {"maximum", p.max_hit_points},
                              {"gold", coins(p.wealth, Coin::gold)},
                              {"items", i18n::plural("{count} item", "{count} items",
                                                     static_cast<int>(p.inventory.size()))}}));
    }
    if (loaded && campaign_ && campaign_->selected())
    {
        const auto &m = campaign_->member(campaign_->selected());
        required_node<Label>(*this, "Party")
            .set_text(i18n::format(
                "{name} / HP {current}/{maximum}\nparty {gold} gp / {items}",
                {{"name", String::utf8(m.character.sheet().name.c_str())},
                 {"current", m.vitals.hit_points},
                 {"maximum", campaign_->hit_point_maximum(campaign_->selected())},
                 {"gold", coins(campaign_->state().wealth, Coin::gold)},
                 {"items",
                  i18n::plural("{count} item", "{count} items",
                               static_cast<int>(m.character.inventory().items().size()))}}));
    }
    for (unsigned slot = 0; slot < 8; ++slot)
    {
        auto *button = &required_node<Button>(*this, String("PartyList/Rows/Member") +
                                                         String::num_uint64(slot));
        const auto id = campaign_ ? campaign_->state().slots[slot] : 0;
        button->set_visible(id != 0);
        if (!id)
        {
            continue;
        }
        // While shopping, the buyer's row is drawn in gold; otherwise the leader's.
        const bool leader = id == campaign_->leader();
        button->call("set_highlight",
                     shopping ? campaign_->state().selected_slot.index == slot : leader);
        const auto &m = campaign_->member(id);
        const auto &cs = m.character.sheet();
        const auto name = (leader ? String::utf8("★ ") : String()) + String::utf8(cs.name.c_str());
        const auto text = i18n::format("{name}\n{class} / AC {ac} / HP {current}/{maximum}",
                                       {{"name", name},
                                        {"class", i18n::text(cs.character_class)},
                                        {"ac", campaign_->profile(id).armor_class},
                                        {"current", m.vitals.hit_points},
                                        {"maximum", cs.hit_points}});
        button->set_text(text);
        button->set_tooltip_text(text);
        auto *arrow = &required_node<Button>(*button, "Advance");
        arrow->call("place_in_member_row", name);
        arrow->set_visible(embedded_party_ && campaign_->can_advance(id) && session_->can_leave());
    }
    required_node<Control>(*this, "TourVisuals").queue_redraw();
    // Marked shown only once everything above succeeded, so a failed refresh
    // is tried again on the next frame (Effective C++ Item 29).
    shown_revision_ = s.revision;
}

Ref<Texture2D> RolfTourView::npc_portrait(std::string_view file)
{
    auto found = npc_portraits_.find(file);
    if (found == npc_portraits_.end())
    {
        const auto path = "res://bin/portraits/" + std::string(file);
        found =
            npc_portraits_
                .emplace(file, ResourceLoader::get_singleton()->load(String::utf8(path.c_str())))
                .first;
    }
    return found->second;
}

Dictionary RolfTourView::tour_visuals()
{
    Dictionary data;
    if (!session_ || wall_view_.is_null() || session_->snapshot().visited.none())
    {
        return data;
    }
    const auto &state = session_->snapshot();
    data["wall"] = wall_view_;
    if (shown_monster_picture_ && !monster_frames_.empty())
    {
        data["picture"] = monster_frames_[current_monster_frame()];
    }
    else
    {
        const auto portrait_file =
            opengold::speaking_npc_portrait(state, session_->shops(), session_->temples());
        const auto portrait =
            portrait_file.empty() ? Ref<Texture2D>() : npc_portrait(portrait_file);
        if (portrait.is_valid())
        {
            data["picture"] = portrait;
        }
        else if (state.sprite_frame >= 0 && sprites_[state.sprite_frame].is_valid())
        {
            const auto &source = session_->sprites()[state.sprite_frame];
            data["sprite"] = sprites_[state.sprite_frame];
            data["sprite_size"] = Vector2(source.width, source.height);
        }
    }
    Array cells;
    cells.resize(256);
    for (unsigned y = 0; y < 16; ++y)
    {
        for (unsigned x = 0; x < 16; ++x)
        {
            const unsigned index = y * 16 + x;
            if (!no_fog_ && !state.seen.test(index))
            {
                cells[index] = -1;
                continue;
            }
            const auto &cell = session_->map().at(x, y);
            int flags = state.visited.test(index) ? 1 : 0;
            for (unsigned direction = 0; direction < 4; ++direction)
            {
                if (cell.walls[direction])
                {
                    flags |= 1 << (1 + direction);
                }
                if (cell.doors[direction])
                {
                    flags |= 1 << (5 + direction);
                }
            }
            cells[index] = flags;
        }
    }
    data["cells"] = cells;
    data["pose"] = Vector2i(state.pose.x, state.pose.y);
    data["facing"] = static_cast<int>(index(state.pose.facing));
    return data;
}

void RolfTourView::capture_frame(const String &name)
{
    if (!capture_)
    {
        return;
    }
    const auto directory = ProjectSettings::get_singleton()->globalize_path("user://checks");
    std::filesystem::create_directories(presentation::path_from_godot(directory));
    const auto path = directory.path_join(name + String(".png"));
    const auto image = get_viewport()->get_texture()->get_image();
    if (checking_ && !town_check_ && session_ && image.is_valid())
    {
        // The tour check waits a frame at each dialogue pause before capture.
        // Check actual framebuffer pixels, including hidden cells, rather than
        // merely checking the flag that the draw code was supposed to honor.
        const auto &state = session_->snapshot();
        const double cell = map_rect_.size.x / 16;
        for (unsigned y = 0; y < 16; ++y)
        {
            for (unsigned x = 0; x < 16; ++x)
            {
                if (x == state.pose.x && y == state.pose.y)
                {
                    continue; // Party arrow covers its cell.
                }
                const auto center = get_global_transform_with_canvas().xform(
                    map_rect_.position + Vector2((x + .5) * cell, (y + .5) * cell));
                const auto expected =
                    get_theme_color(!no_fog_ && !state.seen.test(y * 16 + x) ? "map_black"
                                    : state.visited.test(y * 16 + x)         ? "map_visited"
                                                                             : "map_seen",
                                    "OpenGoldPalette");
                if (!image->get_pixel(center.x, center.y).is_equal_approx(expected))
                {
                    throw std::runtime_error("Overhead fog pixel mismatch at " + std::to_string(x) +
                                             "," + std::to_string(y));
                }
            }
        }
    }
    if (image.is_null() || image->save_png(path) != OK)
    {
        throw std::runtime_error("Failed to capture tour scene");
    }
    UtilityFunctions::print("Screenshot: ", path);
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
                if (required_node<Button>(*this, "Continue").is_disabled() ||
                    !required_node<Button>(*this, "Forward").is_disabled())
                {
                    throw std::runtime_error("Incorrect input lock at tour prompt");
                }
                UtilityFunctions::print("Tour pause ", check_prompts_, " at ", s.pose.x, ",",
                                        s.pose.y, " facing ",
                                        static_cast<int64_t>(index(s.pose.facing)));
                required_node<Button>(*this, "Restart").grab_focus();
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
            required_node<Button>(*this, "Restart").grab_focus();
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
                   : session_->party().inventory.size();
    };
    const auto gold = [&]
    {
        return campaign_ ? coins(campaign_->state().wealth, Coin::gold)
                         : coins(session_->party().wealth, Coin::gold);
    };
    if (check_prompts_ != 8)
    {
        throw std::runtime_error("Town started before the full tour");
    }
    if (shop_check_stage_ == 3 && s.phase == TourPhase::completed)
    {
        if (inventory_size() != 1 || session_->script_variable(EclAddress{0x6BC1}) != gold())
        {
            throw std::runtime_error("Shop results did not persist after leaving");
        }
        party_selected(0);
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
        // M opens the Cast menu while exploring; a Fighter has nothing to cast.
        Ref<InputEventKey> magic;
        magic.instantiate();
        magic->set_keycode(Key::KEY_M);
        magic->set_pressed(true);
        respond_to_input(magic);
        if (!required_node<Control>(*this, "CastPanel").is_visible() ||
            !required_node<Label>(*this, "CastPanel/Status").get_text().contains("Nothing to cast"))
        {
            throw std::runtime_error("M must open the Cast menu");
        }
        required_node<Button>(*this, "CastPanel/Close").emit_signal("pressed");
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
            const auto price = session_->shop_stock()[0].stored.value;
            if (required_node<ItemList>(*this, "Choices").get_item_count() != 55 ||
                required_node<Button>(*this, "Continue").is_disabled())
            {
                throw std::runtime_error("Arms shop list is not available in Godot");
            }
            required_node<Button>(*this, "Continue").emit_signal("pressed");
            if (gold() != gold_before - price || inventory_size() != 1)
            {
                throw std::runtime_error(
                    "Godot purchase did not debit the purse and add inventory");
            }
            required_node<Button>(*this, "Inventory").emit_signal("pressed");
            shop_check_stage_ = 2;
            return;
        }
        required_node<ItemList>(*this, "InventoryPanel/Items").select(0);
        const auto id = campaign_->selected();
        const auto ac = campaign_->profile(id).armor_class;
        required_node<Button>(*this, "InventoryPanel/Equip").emit_signal("pressed");
        if (campaign_->profile(id).armor_class != ac + 2)
        {
            throw std::runtime_error("Inventory Equip did not apply shield AC");
        }
        required_node<Button>(*this, "InventoryPanel/Unequip").emit_signal("pressed");
        if (campaign_->profile(id).armor_class != ac)
        {
            throw std::runtime_error("Inventory Unequip did not remove shield AC");
        }
        required_node<Button>(*this, "InventoryPanel/Equip").emit_signal("pressed");
        // The shop's ID takes any item for 200 gp; a plain shield hides nothing.
        auto purse = campaign_->state().wealth;
        coins(purse, Coin::gold) = 250;
        campaign_->set_wealth(purse);
        auto &identify = required_node<Button>(*this, "InventoryPanel/Identify");
        if (!identify.is_visible() || identify.is_disabled())
        {
            throw std::runtime_error("The shop must offer ID in the inventory");
        }
        identify.emit_signal("pressed");
        if (gold() != 50 || !required_node<Label>(*this, "InventoryPanel/Status")
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
        member.character = opengold::Character(*opengold::srd5::character_rules(), draft,
                                               member.character.appearance());
        member.character.replace_inventory(std::move(inventory));
        member.equipped.clear();
        member.vitals = {member.character.sheet().hit_points, false, {}};
        campaign_->restore(std::move(changed));
        refresh_inventory();
        required_node<ItemList>(*this, "InventoryPanel/Items").select(0);
        required_node<Button>(*this, "InventoryPanel/Equip").emit_signal("pressed");
        if (!required_node<Label>(*this, "InventoryPanel/Status")
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
        required_node<ItemList>(*this, "InventoryPanel/Items").select(0);
        required_node<Button>(*this, "InventoryPanel/Unequip").emit_signal("pressed");
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
        if (campaign_ && s.dialogue == "Choose a party member.")
        {
            selection = 0;
        }
        required_node<ItemList>(*this, "Choices").select(static_cast<std::int32_t>(selection));
        required_node<Button>(*this, "Continue").emit_signal("pressed");
        return;
    }
    if (s.phase == TourPhase::awaiting_input)
    {
        required_node<LineEdit>(*this, "Answer").set_text("0");
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
    constexpr int dx[]{0, 1, 0, -1}, dy[]{-1, 0, 1, 0};
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
            const auto &a = session_->map().at(cell % 16, cell / 16);
            const auto &b = session_->map().at(x, y);
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
    required_node<Button>(*this, s.pose.facing == facing ? "Forward" : "Right")
        .emit_signal("pressed");
}

void RolfTourView::check_district_labels(const TourSnapshot &s)
{
    // Labels follow the session at the next refresh, not immediately.
    if (shown_revision_ != s.revision)
    {
        return;
    }
    const String district = i18n::text(district_name(s.area_id));
    const String location = required_node<Label>(*this, "Location").get_text();
    const String map_title = required_node<Label>(*this, "MapTitle").get_text();
    const String speaker = required_node<Label>(*this, "Speaker").get_text();
    const bool speaker_shows_district = s.tour_finished && s.phase != TourPhase::shopping;
    const String title = required_node<Label>(*this, "Title").get_text();
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
    if (!session_->script_diagnostics().empty())
    {
        throw std::runtime_error(session_->script_diagnostics().back());
    }
    if (s.phase == TourPhase::faulted)
    {
        throw std::runtime_error(s.diagnostic);
    }
    check_district_labels(s);
    if (shown_monster_picture_)
    {
        // Let the close-up animate for about two seconds before pressing a key.
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
    if (s.phase == TourPhase::awaiting_continue)
    {
        const auto choice = s.choices.size() == 5 && s.choices[0] == "Fight" ? 1 : 0;
        required_node<ItemList>(*this, "Choices").select(choice);
        required_node<Button>(*this, "Continue").emit_signal("pressed");
        return false;
    }
    if (s.phase != TourPhase::completed)
    {
        return false;
    }
    if (s.area_id == 0)
    {
        if (session_->script_variable(ecl_slums_orc_victory) == 255)
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
    if (session_->script_variable(ecl_slums_orc_victory) == 255)
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
    if (!session_->script_diagnostics().empty())
    {
        throw std::runtime_error("Recovery route fault: " + session_->script_diagnostics().back());
    }
    const auto choose = [&](std::size_t choice)
    {
        required_node<ItemList>(*this, "Choices").select(static_cast<std::int32_t>(choice));
        required_node<Button>(*this, "Continue").emit_signal("pressed");
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
        const auto event = session_->map().at(s.pose.x, s.pose.y).event_number();
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
            if (s.dialogue == "Choose a party member.")
            {
                selection = 0;
            }
        }
        choose(selection);
        return;
    }
    if (s.phase == TourPhase::shopping)
    {
        required_node<Button>(*this, "LeaveShop").emit_signal("pressed");
        return;
    }
    if (s.phase == TourPhase::awaiting_input)
    {
        required_node<LineEdit>(*this, "Answer").set_text("0");
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
        required_node<Button>(*this, "Camp").emit_signal("pressed");
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
            member.vitals.hit_points <= 1 || session_->script_variable(ecl_temple_service) != 0)
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
        const auto shown = required_node<RichTextLabel>(*this, "Dialogue").get_text();
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
        required_node<Button>(*this, "Camp").emit_signal("pressed");
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
            if (session_->map().at(x, y).event_number() == target_event)
            {
                check_walk_to(x, y);
                return;
            }
        }
    }
    throw std::runtime_error("Original recovery location is missing");
}

namespace
{
godot::String rest_rules_path()
{
    return game_rules_file();
}

godot::String rest_text(std::string_view value)
{
    return i18n::text(value);
}
} // namespace

#include "rest_dialog_impl.h"
