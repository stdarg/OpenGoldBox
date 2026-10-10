#include "godot_images.h"
#include "godot_nodes.h"
#include "application_settings.h"
#include "localization.h"
#include "game_resources.h"
#include "rolf_tour_view.h"
#include "opengold/campaign_party.h"
#include "opengold/npc_portraits.h"
#include "vital_fixtures.h"
#include "opengold/srd5.h"
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
// Numeric constructors are safe before Godot initializes the extension interface.
const Color background(18 / 255.f, 26 / 255.f, 32 / 255.f),
      panel(28 / 255.f, 39 / 255.f, 46 / 255.f), line(65 / 255.f, 80 / 255.f, 88 / 255.f),
      gold(215 / 255.f, 180 / 255.f, 121 / 255.f), party_color(121 / 255.f, 214 / 255.f, 212 / 255.f);
const std::array<Vector2, 4> direction{Vector2(0, -1), Vector2(1, 0), Vector2(0, 1),
    Vector2(-1, 0)};
const std::array<const char *, 4> direction_name{"North", "East", "South", "West"};

// The areas the session can enter: the town (0), the Slums (20) and Kuto's
// Well's plaza (29) and catacombs (32).
[[nodiscard]] const char *district_name(unsigned area_id)
{
    if (area_id == 20)
        return N_("Slums");
    if (area_id == 29 || area_id == 32)
        return N_("Kuto's Well");
    return N_("New Phlan");
}

} // namespace

void RolfTourView::_bind_methods()
{
    ADD_SIGNAL(MethodInfo("save_requested", PropertyInfo(Variant::BOOL, "saving")));
    ADD_SIGNAL(MethodInfo("party_member_selected", PropertyInfo(Variant::INT, "slot")));
    ADD_SIGNAL(MethodInfo("level_up_requested", PropertyInfo(Variant::INT, "id")));
}

void RolfTourView::_notification(int what)
{
    if (what == NOTIFICATION_RESIZED && ready_)
    {
        layout();
        queue_redraw();
    }
}

void RolfTourView::_ready()
{
    i18n::prepare_ui(*this);
    // Resizing the window can notify this view before dynamic controls exist.
    ready_ = false;
    set_texture_filter(CanvasItem::TEXTURE_FILTER_NEAREST);
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
        auto *member = &required_node<Button>(
                           *this, String("PartyList/Rows/Member") + String::num_uint64(slot));
        member->connect("pressed", presentation::guarded(this, &RolfTourView::party_selected).bind(slot));
        auto arrow = presentation::make_node<Button>();
        arrow->set_name("Advance");
        arrow->set_text(String::utf8("↑"));
        arrow->set_size(Vector2(30, 26));
        arrow->set_tooltip_text(i18n::text(N_("Level up")));
        arrow->connect("pressed", presentation::guarded(this, &RolfTourView::level_up_requested).bind(slot));
        presentation::attach_child(*member, std::move(arrow));
    }
    required_node<ItemList>(*this, "InventoryPanel/Items")
    .connect("item_selected", presentation::guarded(this, &RolfTourView::inventory_selected));
    required_node<Button>(*this, "InventoryPanel/Equip")
    .connect("pressed", presentation::guarded(this, &RolfTourView::equip_item).bind(true));
    required_node<Button>(*this, "InventoryPanel/Unequip")
    .connect("pressed", presentation::guarded(this, &RolfTourView::equip_item).bind(false));
    required_node<Button>(*this, "MemberSheet/Close")
    .connect("pressed", presentation::guarded(this, &RolfTourView::close_sheet));
    required_node<Window>(*this, "MemberSheet")
    .connect("close_requested", presentation::guarded(this, &RolfTourView::close_sheet));
    required_node<Button>(*this, "LeaveShop")
    .connect("pressed", presentation::guarded(this, &RolfTourView::leave_shop));
    get_window()->set_min_size(Vector2i(960, 720));
    for (bool saving :
            {
                true, false
            })
    {
        auto button = presentation::make_node<Button>();
        button->set_name(saving ? "SaveGame" : "LoadGame");
        button->set_text(i18n::text(saving ? N_("Save game") : N_("Load game")));
        button->connect("pressed", presentation::guarded(this, &RolfTourView::request_save).bind(saving));
        button->set_visible(embedded_party_);
        presentation::attach_child(*this, std::move(button));
    }
    ready_ = true;
    layout();
    if (Engine::get_singleton()->is_editor_hint())
        return;
    const auto args = OS::get_singleton()->get_cmdline_user_args();
    no_fog_ = settings::flag("--no-fog");
    town_check_ = args.has("--town-check");
    checking_ = args.has("--tour-check") || town_check_;
    capture_ = args.has("--capture");
    if (campaign_ && args.has("--party-check"))
        town_check_ = checking_ = true;
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
        required_node<Button>(*this, "Restart").hide();
}

void RolfTourView::layout()
{
    const auto width = get_size().x, height = get_size().y;
    const double margin = 24, gutter = 24,
                 sidebar = std::min(std::clamp(width * .30, 270.0, 430.0), height - 380.0);
    const double main_width = width - margin * 2 - gutter - sidebar;
    const bool shopping = session_ && session_->snapshot().phase == TourPhase::shopping;
    const double view_height = std::min(main_width * .625, height - (shopping ? 490.0 : 410.0));
    scene_rect_ = Rect2(margin, 102, view_height / 1.2, view_height);
    map_rect_ = Rect2(margin + main_width + gutter, 102, sidebar, sidebar);
    dialogue_rect_ = Rect2(margin, scene_rect_.get_end().y + 18, main_width,
                           height - scene_rect_.get_end().y - 76);
    const auto place = [&](const char *name, Rect2 rect)
    {
        auto *node = &required_node<Control>(*this, name);
        node->set_position(rect.position);
        node->set_size(rect.size);
    };
    place("SaveGame", Rect2(width - 520, 20, 140, 36));
    place("LoadGame", Rect2(width - 370, 20, 140, 36));
    place("Title", Rect2(margin, 20, main_width, 34));
    place("PartyList", Rect2(scene_rect_.get_end().x + 16, 102,
                             main_width - scene_rect_.size.x - 16, view_height));
    place("Location", Rect2(margin, 68, main_width, 26));
    place("MapTitle", Rect2(map_rect_.position.x, 68, sidebar, 26));
    place("Coordinates", Rect2(map_rect_.position.x, map_rect_.get_end().y + 12, sidebar, 28));
    place("Legend", Rect2(map_rect_.position.x, map_rect_.get_end().y + 48, sidebar, 50));
    place("Speaker",
          Rect2(dialogue_rect_.position + Vector2(18, 12), Vector2(main_width - 36, 26)));
    place("Dialogue", Rect2(dialogue_rect_.position + Vector2(18, 46),
                            Vector2(main_width - 36, dialogue_rect_.size.y - 112)));
    place("Continue", Rect2(dialogue_rect_.get_end() - Vector2(182, 54), Vector2(164, 40)));
    place("Progress", Rect2(dialogue_rect_.position + Vector2(18, dialogue_rect_.size.y - 48),
                            Vector2(main_width - 220, 30)));
    place("Movement", Rect2(map_rect_.position.x, height - 178, sidebar, 26));
    const double button_width = (sidebar - 12) / 3;
    place("Left", Rect2(map_rect_.position.x, height - 140, button_width, 40));
    place("Forward",
          Rect2(map_rect_.position.x + button_width + 6, height - 140, button_width, 40));
    place("Right",
          Rect2(map_rect_.position.x + (button_width + 6) * 2, height - 140, button_width, 40));
    place("Restart", Rect2(map_rect_.position.x, height - 88, sidebar, 34));
    place("Footer", Rect2(margin, height - 36, width - 2 * margin, 26));
    place("Party", Rect2(map_rect_.position.x, map_rect_.get_end().y + 46, sidebar, 50));
    required_node<Label>(*this, "Legend").hide();
    const double utility_width = (sidebar - 12) / 3;
    place("Look", Rect2(map_rect_.position.x, height - 226, utility_width, 34));
    place("Camp", Rect2(map_rect_.position.x + utility_width + 6, height - 226, utility_width, 34));
    place("Inventory",
          Rect2(map_rect_.position.x + 2 * (utility_width + 6), height - 226, utility_width, 34));
    place("Choices", Rect2(dialogue_rect_.position + Vector2(18, 84),
                           Vector2(main_width - 36, dialogue_rect_.size.y - 148)));
    place("Answer", Rect2(dialogue_rect_.position + Vector2(18, dialogue_rect_.size.y - 100),
                          Vector2(main_width - 36, 36)));
    place("LeaveShop", Rect2(dialogue_rect_.get_end() - Vector2(332, 54), Vector2(140, 40)));
    place("InventoryPanel", Rect2(margin + 40, 90, width - 2 * margin - 80, height - 160));
    auto *inventory_panel = &required_node<Control>(*this, "InventoryPanel");
    required_node<Control>(*this, "InventoryPanel/Items").set_position(Vector2(20, 70));
    required_node<Control>(*this, "InventoryPanel/Items")
    .set_size(inventory_panel->get_size() - Vector2(40, 254));
    required_node<Control>(*this, "InventoryPanel/Close")
    .set_position(Vector2(20, inventory_panel->get_size().y - 54));
    required_node<Control>(*this, "InventoryPanel/Close").set_size(Vector2(180, 36));
    const auto iw = inventory_panel->get_size().x, ih = inventory_panel->get_size().y;
    required_node<Control>(*this, "InventoryPanel/Header").set_position(Vector2(20, 18));
    required_node<Control>(*this, "InventoryPanel/Header").set_size(Vector2(iw - 40, 44));
    required_node<Control>(*this, "InventoryPanel/Status").set_position(Vector2(20, ih - 132));
    required_node<Control>(*this, "InventoryPanel/Status").set_size(Vector2(iw - 40, 68));
    required_node<Control>(*this, "InventoryPanel/Equip").set_position(Vector2(220, ih - 54));
    required_node<Control>(*this, "InventoryPanel/Equip").set_size(Vector2(150, 36));
    required_node<Control>(*this, "InventoryPanel/Unequip").set_position(Vector2(390, ih - 54));
    required_node<Control>(*this, "InventoryPanel/Unequip").set_size(Vector2(150, 36));
    required_node<Window>(*this, "MemberSheet").set_size(Vector2i(width - 120, height - 120));
    place("MemberSheet/Text", Rect2(24, 24, width - 168, height - 220));
    place("MemberSheet/Close", Rect2(width - 290, height - 180, 130, 36));
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
                roll = {{6, 5, 4, 1}, 3};
            const auto id =
                campaign_->add_pc(opengold::Character(*opengold::srd5::character_rules(), d, {}));
            campaign_->set_wealth(id, {0, 0, 0, 9999, 0, 0, 0});
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
                RolfTourSession::load(presentation::path_from_godot(directory)));
            session_->encounter_challenge(settings::encounter_challenge());
            if (campaign_)
                session_->campaign_party(campaign_);
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
        return;
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
            session_->buy(s.continue_ticket, selected[0]);
    }
    else if (s.phase == TourPhase::awaiting_input)
    {
        session_->input(s.continue_ticket,
                        required_node<LineEdit>(*this, "Answer").get_text().utf8().get_data());
    }
    else
        session_->choose(s.continue_ticket, selected.is_empty() ? 0 : selected[0]);
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
        return;
    refresh_inventory();
    panel->show();
}

void RolfTourView::refresh_inventory()
{
    auto *items = &required_node<ItemList>(*this, "InventoryPanel/Items");
    items->clear();
    if (!campaign_ || !campaign_->selected())
        return;
    const auto &m = campaign_->member(campaign_->selected());
    required_node<Label>(*this, "InventoryPanel/Header")
    .set_text(i18n::format("{name} / {class} / {gold} gp",
    {
        {"name", String::utf8(m.character.sheet().name.c_str())},
        {"class", i18n::text(m.character.sheet().character_class)},
        {"gold", coins(m.wealth, Coin::gold)}
    }));
    for (const auto &item : m.character.inventory().items())
        items->add_item(
            (std::find(m.equipped.begin(), m.equipped.end(), item.id) != m.equipped.end()
             ? i18n::text("Equipped / ")
             : String()) +
            i18n::format("{item} x{quantity}",
    {{"item", i18n::text(item.name)}, {"quantity", item.quantity}}));
    if (items->get_item_count())
        items->select(0);
    required_node<Button>(*this, "InventoryPanel/Equip").set_disabled(!items->get_item_count());
    required_node<Button>(*this, "InventoryPanel/Unequip").set_disabled(!items->get_item_count());
    required_node<Label>(*this, "InventoryPanel/Status")
    .set_text(i18n::text(N_("Inventory is empty. Visit a shop to buy equipment.")));
    if (items->get_item_count())
        inventory_selected(0);
}

void RolfTourView::inventory_selected(std::int64_t index)
{
    if (!campaign_ || !campaign_->selected())
        return;
    const auto &m = campaign_->member(campaign_->selected());
    const auto items = m.character.inventory().items();
    if (index < 0 || static_cast<std::size_t>(index) >= items.size())
        return;
    try
    {
        required_node<Label>(*this, "InventoryPanel/Status")
        .set_text(i18n::text(
                       opengold::srd5::equipment_note(m.character.sheet(), items[index].definition_id)));
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
            throw std::runtime_error("Select an item first.");
        const auto id = campaign_->selected();
        // Copies: equipping can replace the member's inventory, and with it
        // anything still pointing into it (Effective C++ Item 28).
        std::uint64_t item{};
        std::string definition;
        {
            const auto items = campaign_->member(id).character.inventory().items();
            if (selection[0] < 0 || static_cast<std::size_t>(selection[0]) >= items.size())
                throw std::runtime_error("Select an existing item.");
            item = items[selection[0]].id;
            definition = items[selection[0]].definition_id;
        }
        if (equip)
            campaign_->equip(id, item);
        else
            campaign_->unequip(id, item);
        refresh_inventory();
        required_node<ItemList>(*this, "InventoryPanel/Items").select(selection[0]);
        refresh();
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

void RolfTourView::party_selected(std::int64_t index)
{
    if (index < 0 || index >= 8 || !session_ || !campaign_ || !campaign_->state().slots[index])
        return;
    const auto slot = static_cast<unsigned>(index);
    if (session_->snapshot().phase == TourPhase::shopping)
    {
        // In a shop, clicking a member chooses the buyer instead of opening a sheet.
        select_buyer(slot);
        return;
    }
    if (session_->can_select_member())
        campaign_->select(opengold::PartySlot{slot});
    if (embedded_party_)
        emit_signal("party_member_selected", slot);
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
        const std::array<const char *, 6> names{"Strength",     "Dexterity", "Constitution",
                                                "Intelligence", "Wisdom",    "Charisma"};
        for (unsigned i = 0; i < 6; ++i)
        {
            const auto ability = opengold::rules::all_abilities[i];
            text += std::string(names[i]) + ": " + std::to_string(s.scores[ability]) + " / Save " +
                    std::to_string(s.saving_throws[ability]) + "\n";
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
        return;
    campaign_->select(opengold::PartySlot{slot});
    refresh();
}

std::vector<unsigned> RolfTourView::occupied_slots() const
{
    std::vector<unsigned> slots;
    for (unsigned slot = 0; slot < 8; ++slot)
        if (campaign_->state().slots[slot])
            slots.push_back(slot);
    return slots;
}

void RolfTourView::buyer_key(Key keycode)
{
    const auto slots = occupied_slots();
    if (slots.empty())
        return;
    if (keycode == Key::KEY_TAB)
    {
        // Cycle through the member rows in display order, wrapping at the end.
        const auto current =
            std::find(slots.begin(), slots.end(), campaign_->state().selected_slot.index);
        const auto next = current == slots.end() || current + 1 == slots.end()
                          ? slots.begin()
                          : current + 1;
        select_buyer(*next);
        return;
    }
    // Number keys pick the nth visible member row.
    const auto row = static_cast<std::size_t>(keycode) - static_cast<std::size_t>(Key::KEY_1);
    if (row < slots.size())
        select_buyer(slots[row]);
}

void RolfTourView::close_sheet()
{
    required_node<Window>(*this, "MemberSheet").hide();
}

void RolfTourView::level_up_requested(int slot)
{
    if (embedded_party_ && campaign_ && session_ && session_->can_leave())
        emit_signal("level_up_requested", campaign_->state().slots.at(slot));
}

void RolfTourView::movement(ExplorationCommand command)
{
    if (!session_)
        return;
    if (session_->explore(command))
        refresh();
    else if (session_->snapshot().phase == TourPhase::completed)
        required_node<Label>(*this, "Movement").set_text(i18n::text(N_("The way is blocked")));
}

void RolfTourView::_input(const Ref<InputEvent> &event)
{
    presentation::run_guarded(*this, [&]
    {
        respond_to_input(event);
    });
}

void RolfTourView::respond_to_input(const Ref<InputEvent> &event)
{
    if (required_node<Window>(*this, "RestDialog").is_visible())
        return;
    const Ref<InputEventKey> key = event;
    if (key.is_null() || !key->is_pressed() || key->is_echo())
        return;
    if (shown_monster_picture_)
    {
        // As in the original, any key leaves the monster close-up for combat.
        dismiss_monster_picture();
        get_viewport()->set_input_as_handled();
        return;
    }
    if (required_node<Control>(*this, "InventoryPanel").is_visible())
        return;
    if (required_node<LineEdit>(*this, "Answer").has_focus() &&
            key->get_keycode() != Key::KEY_ENTER)
        return;
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
        return;
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
    default:
        return;
    }
    get_viewport()->set_input_as_handled();
}

void RolfTourView::_process(double delta)
{
    presentation::run_guarded(*this, [&]
    {
        advance_frame(delta);
    });
}

void RolfTourView::advance_frame(double delta)
{
    if (Engine::get_singleton()->is_editor_hint())
        return;
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
                required_node<AudioStreamPlayer>(*this, "Footstep").play();
        }
        if (session_->snapshot().revision != shown_revision_)
            refresh();
        if (shown_monster_picture_)
        {
            monster_picture_seconds_ += delta;
            queue_redraw();
        }
    }
    if (OS::get_singleton()->get_cmdline_user_args().has("--mastery-rest-check"))
    {
        check_mastery_rest_controls();
        return;
    }
    if (OS::get_singleton()->get_cmdline_user_args().has("--rest-check"))
    {
        check_rest_controls();
        return;
    }
    if (checking_)
        check_run();
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
        {
            N_("Rest is not allowed here."),
            N_("Short rest complete: one hour passed; eligible members can spend Hit Dice."),
            N_("Long rest complete: eight hours passed; eligible members recovered HP and supported resources."),
            N_("Rest denied: no active member is eligible."),
            N_("The rest was interrupted. Rest again to recover."),
            N_("Your camp is attacked!"),
            N_("The monsters go on their way."),
            N_("You get away."),
            N_("Your party flees the battle."),
            N_("Locked.")
        })
        result = result.replace(String::utf8(source), i18n::text(source));
    // The parley's spokesman and the members lost in a flight are named.
    const String speaks = " speaks for the party.", lost = " is left behind and lost.";
    auto lines = result.split("\n");
    for (int n = 0; n < lines.size(); ++n)
        if (lines[n].ends_with(speaks))
            lines[n] = i18n::format(N_("{name} speaks for the party."),
            {{"name", lines[n].substr(0, lines[n].length() - speaks.length())}});
        else if (lines[n].ends_with(lost))
            lines[n] = i18n::format(N_("{name} is left behind and lost."),
            {{"name", lines[n].substr(0, lines[n].length() - lost.length())}});
    return String("\n").join(lines);
}

// Choices the campaign host adds use the engine catalog; the rest are original.
String choice_label(const std::string &resource, const std::string &choice)
{
    for (const auto *host :
            {
                N_("Bash"), N_("Pick"), N_("Knock"), N_("Exit")
            })
        if (choice == host)
            return i18n::text(host);
    return i18n::campaign(resource + "/choices", choice);
}

String coin_list(const opengold::Coins &coins)
{
    static constexpr std::array<const char *, 5> amounts{N_("{count} cp"), N_("{count} sp"),
        N_("{count} ep"), N_("{count} gp"), N_("{count} pp")};
    String result;
    for (std::size_t coin = coins.size(); coin-- > 0;)
    {
        if (!coins[coin])
            continue;
        if (!result.is_empty())
            result += ", ";
        result += i18n::format(amounts[coin], {{"count", coins[coin]}});
    }
    return result;
}

String payment_notice(const CoinPayment &payment)
{
    const auto &coins = payment.coins;
    const auto name = String::utf8(payment.payer.c_str());
    if (coins.change == opengold::Coins{})
        return i18n::format("{name} pays {paid} for {owed}.",
    {{"name", name}, {"paid", coin_list(coins.paid)}, {"owed", coin_list(coins.owed)}});
    return i18n::format("{name} pays {paid} for {owed} and receives {change} in change.",
    {
        {"name", name}, {"paid", coin_list(coins.paid)}, {"owed", coin_list(coins.owed)},
        {"change", coin_list(coins.change)}
    });
}

String door_check_notice(const DoorCheck &check)
{
    const auto name = String::utf8(check.member.c_str());
    if (check.method == opengold::DoorMethod::knock)
        return i18n::format("{name} casts Knock, and the lock opens.", {{"name", name}});
    if (check.method == opengold::DoorMethod::pick)
        return i18n::format(
                   "{name} tries to pick the lock: Dexterity (Sleight of Hand) {total} (d20 {die}) against DC {dc}.",
    {{"name", name}, {"total", check.total}, {"die", check.die}, {"dc", check.difficulty}});
    return i18n::format(
               "{name} tries to force the door: Strength (Athletics) {total} (d20 {die}) against DC {dc}.",
    {{"name", name}, {"total", check.total}, {"die", check.die}, {"dc", check.difficulty}});
}

// Script dialogue followed by the coins the party's purses exchanged for it
// and any checks made against a locked door.
String event_dialogue(const std::string &resource, const TourSnapshot &s)
{
    auto result = rest_notice(resource, s.dialogue);
    for (const auto &payment : s.payments)
        result += "\n" + payment_notice(payment);
    for (const auto &check : s.door_checks)
        result += "\n" + door_check_notice(check);
    if (!s.door_checks.empty())
        result += "\n" + i18n::text(s.door_opened ? N_("The door opens.")
                                    : N_("The door stays locked."));
    return result;
}
} // namespace

void RolfTourView::sync_monster_picture()
{
    const auto *picture = session_ ? session_->monster_picture() : nullptr;
    if (picture == shown_monster_picture_)
        return;
    forget_monster_picture();
    shown_monster_picture_ = picture;
    if (!picture)
        return;
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
        refresh();
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
                wall_view_ = ImageTexture::create_from_image(image);
            else
                wall_view_->set_image(image);
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
    const TourSnapshot s = loaded ? session_->snapshot() : TourSnapshot{};
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
    {{"district", district}, {"direction", i18n::text(direction_name[index(s.pose.facing)])}}));
    required_node<Label>(*this, "MapTitle")
    .set_text(i18n::format("{district}    N ↑", {{"district", district}}));
    // Rolf's welcome names the opening tour; afterwards the title names the area.
    required_node<Label>(*this, "Title").set_text(
        s.tour_finished ? i18n::format("OPENGOLDBOX  /  {district}", {{"district", district}})
        : i18n::text("OPENGOLDBOX  /  Rolf's welcome"));
    required_node<Label>(*this, "Coordinates")
    .set_text(i18n::format("Party ({x}, {y})   {direction}",
    {
        {"x", s.pose.x},
        {"y", s.pose.y},
        {"direction", i18n::text(direction_name[index(s.pose.facing)])}
    }));
    required_node<Label>(*this, "Speaker").set_text(i18n::text(faulted    ? N_("Unable to continue")
            : shopping ? N_("Shop / select an item")
            : s.tour_finished
            ? district_name(s.area_id)
            : N_("Rolf  /  Council guide")));
    if (shopping && campaign_ && campaign_->selected())
    {
        const auto &buyer = campaign_->member(campaign_->selected());
        required_node<Label>(*this, "Speaker").set_text(
            i18n::format("Shop / buying for {name}: {gold} gp, {count}/16 items",
        {
            {"name", String::utf8(buyer.character.sheet().name.c_str())},
            {"gold", coins(buyer.wealth, Coin::gold)},
            {"count", static_cast<int64_t>(buyer.character.inventory().items().size())}
        }));
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
    if (waiting && s.choices.size() == 1 && s.choices[0] == "Cancel")
        required_node<Button>(*this, "Continue").set_text(i18n::text(N_("Leave temple [Enter]")));
    required_node<Button>(*this, "Continue").set_visible(!completed);
    required_node<Label>(*this, "Progress")
    .set_text(faulted           ? i18n::text("Stopped")
               : shopping && campaign_ ? i18n::text(N_("Tab or number keys: choose the buyer"))
               : s.tour_finished || waiting ? String()
    : i18n::text("Following the guide"));
    required_node<Label>(*this, "Movement")
    .set_text(i18n::text(completed ? N_("Explore  /  arrow keys") : N_("Movement paused")));
    for (const char *name :
            {"Left", "Forward", "Right", "Look", "Camp"
            })
        required_node<Button>(*this, name).set_disabled(!completed);
    required_node<Button>(*this, "LeaveShop").set_visible(shopping);
    required_node<LineEdit>(*this, "Answer").set_visible(answer);
    required_node<LineEdit>(*this, "Answer").set_max_length(s.number_input ? 6 : 40);
    auto *choices = &required_node<ItemList>(*this, "Choices");
    choices->set_visible(shopping || multiple);
    if (displayed_ticket_ != s.continue_ticket)
    {
        choices->clear();
        if (shopping)
            for (const auto &item : session_->shop_stock())
            {
                // Stock without an equipment conversion stays for sale, disclosed.
                const bool usable = !opengold::equipment_conversion(item).starts_with("por:unsupported:");
                choices->add_item(i18n::format(
                usable ? N_("{item} / {price} gp") : N_("{item} / {price} gp / cannot be equipped"),
                {   {"item", i18n::text(item.label())},
                    {"price", item.stored.value}
                }));
            }
        else
            for (const auto &c : s.choices)
                choices->add_item(s.dialogue == "Choose a party member."
                                  ? String::utf8(c.c_str())
                                  : choice_label(resource, c));
        if (choices->get_item_count())
            choices->select(0);
        if (shopping || multiple)
            choices->grab_focus();
        if (answer)
        {
            required_node<LineEdit>(*this, "Answer").clear();
            required_node<LineEdit>(*this, "Answer").grab_focus();
        }
        displayed_ticket_ = s.continue_ticket;
    }
    required_node<RichTextLabel>(*this, "Dialogue")
    .set_size(
        Vector2(dialogue_rect_.size.x - 36,
                (shopping || multiple) ? 36 : dialogue_rect_.size.y - (answer ? 160 : 112)));
    if (multiple)
    {
        const double text_height = std::min((dialogue_rect_.size.y - 122) * .55,
                                            std::max<double>(28.0, dialogue_rect_.size.y - 192));
        required_node<RichTextLabel>(*this, "Dialogue")
        .set_size(Vector2(dialogue_rect_.size.x - 36, text_height));
        choices->set_position(dialogue_rect_.position + Vector2(18, 56 + text_height));
        choices->set_size(
            Vector2(dialogue_rect_.size.x - 36, dialogue_rect_.size.y - 120 - text_height));
    }
    else
    {
        choices->set_position(dialogue_rect_.position + Vector2(18, 84));
        choices->set_size(Vector2(dialogue_rect_.size.x - 36, dialogue_rect_.size.y - 148));
    }
    if (shopping)
        required_node<RichTextLabel>(*this, "Dialogue")
        .set_text(s.diagnostic.empty() ? i18n::text("Prices are per listed item or bundle.")
                   : i18n::text(s.diagnostic));
    if (answer && !s.diagnostic.empty())
        required_node<RichTextLabel>(*this, "Dialogue")
        .add_text("\n" + String::utf8(s.diagnostic.c_str()));
    if (waiting && !s.diagnostic.empty())
        required_node<RichTextLabel>(*this, "Dialogue")
        .set_text(i18n::text(s.diagnostic) + "\n" +
                   event_dialogue(resource + "/dialogue", s));
    if (loaded)
    {
        const auto &p = session_->party();
        required_node<Label>(*this, "Party").set_text(i18n::format(
                                               "Fighter / Level 1 / HP {current}/{maximum}\n{gold} gp / {items}",
        {
            {"current", p.hit_points},
            {"maximum", p.max_hit_points},
            {"gold", coins(p.wealth, Coin::gold)},
            {"items", i18n::plural("{count} item", "{count} items", static_cast<int>(p.inventory.size()))}
        }));
    }
    if (loaded && campaign_ && campaign_->selected())
    {
        const auto &m = campaign_->member(campaign_->selected());
        required_node<Label>(*this, "Party").set_text(
            i18n::format("{name} / HP {current}/{maximum}\n{gold} gp / {items}",
        {
            {"name", String::utf8(m.character.sheet().name.c_str())},
            {"current", m.vitals.hit_points},
            {"maximum", campaign_->hit_point_maximum(campaign_->selected())},
            {"gold", coins(m.wealth, Coin::gold)},
            {
                "items", i18n::plural("{count} item", "{count} items",
                                      static_cast<int>(m.character.inventory().items().size()))
            }
        }));
    }
    for (unsigned slot = 0; slot < 8; ++slot)
    {
        auto *button = &required_node<Button>(
                           *this, String("PartyList/Rows/Member") + String::num_uint64(slot));
        const auto id = campaign_ ? campaign_->state().slots[slot] : 0;
        button->set_visible(id != 0);
        if (!id)
            continue;
        // While shopping, the buyer's row is drawn in gold; otherwise the leader's.
        const bool leader = id == campaign_->leader();
        for (const char *color : {"font_color", "font_hover_color", "font_focus_color"})
            if (shopping ? campaign_->state().selected_slot.index == slot : leader)
                button->add_theme_color_override(color, gold);
            else
                button->remove_theme_color_override(color);
        const auto &m = campaign_->member(id);
        const auto &cs = m.character.sheet();
        const auto name = (leader ? String::utf8("★ ") : String()) + String::utf8(cs.name.c_str());
        const auto text = i18n::format("{name}\n{class} / AC {ac} / HP {current}/{maximum}",
        {
            {"name", name},
            {"class", i18n::text(cs.character_class)},
            {"ac", campaign_->profile(id).armor_class},
            {"current", m.vitals.hit_points},
            {"maximum", cs.hit_points}
        });
        button->set_text(text);
        button->set_tooltip_text(text);
        auto *arrow = &required_node<Button>(*button, "Advance");
        const auto width = Vector2(button->get_theme_font("font")->call(
                                       "get_string_size", name, 0, -1,
                                       button->get_theme_font_size("font_size")))
                           .x;
        arrow->set_position(Vector2(std::min(width + 16, button->get_size().x - 36), 2));
        arrow->set_visible(embedded_party_ && campaign_->can_advance(id) && session_->can_leave());
    }
    queue_redraw();
    // Marked shown only once everything above succeeded, so a failed refresh
    // is tried again on the next frame (Effective C++ Item 29).
    shown_revision_ = s.revision;
}

void RolfTourView::_draw()
{
    draw_rect(Rect2(Vector2(), get_size()), background);
    draw_line(Vector2(24, 57), Vector2(get_size().x - 24, 57), line);
    draw_rect(dialogue_rect_, panel);
    draw_rect(dialogue_rect_, line, false);
    draw_line(dialogue_rect_.position, dialogue_rect_.position + Vector2(dialogue_rect_.size.x, 0),
              gold, 2);
    draw_rect(map_rect_, Color(0, 0, 0));
    draw_scene();
    draw_map();
    draw_rect(scene_rect_, line, false);
    draw_rect(map_rect_, line, false);
}

Ref<Texture2D> RolfTourView::npc_portrait(std::string_view file)
{
    auto found = npc_portraits_.find(file);
    if (found == npc_portraits_.end())
    {
        const auto path = "res://bin/portraits/" + std::string(file);
        found = npc_portraits_
                .emplace(file, ResourceLoader::get_singleton()->load(String::utf8(path.c_str())))
                .first;
    }
    return found->second;
}

void RolfTourView::draw_scene()
{
    draw_rect(scene_rect_, panel);
    if (!session_ || session_->snapshot().visited.none() || wall_view_.is_null())
        return;
    // Fit the complete original 88x88 view. DOS EGA pixels were displayed 6/5
    // as tall as wide; letterboxing preserves art and door framing on resize.
    const double scale = std::min(scene_rect_.size.x / 88.0, scene_rect_.size.y / 105.6);
    const Vector2 pixel_scale(scale, scale * 1.2), size(88 * pixel_scale.x, 88 * pixel_scale.y);
    const Rect2 view(scene_rect_.position + (scene_rect_.size - size) * .5, size);
    draw_texture_rect(wall_view_, view, false);
    if (shown_monster_picture_ && !monster_frames_.empty())
    {
        // The 88x88 close-up covers the whole 3D view, as in the original.
        draw_texture_rect(monster_frames_[current_monster_frame()], view, false);
        return;
    }
    const auto &state = session_->snapshot();
    // A speaking NPC's portrait replaces the small encounter sprite. It is cut to
    // the view's 5:6 on-screen shape (see docs/PORTRAITS.md), so it fills the view.
    const auto portrait_file = opengold::speaking_npc_portrait(state);
    const auto portrait = portrait_file.empty() ? Ref<Texture2D>() : npc_portrait(portrait_file);
    if (portrait.is_valid())
        draw_texture_rect(portrait, view, false);
    else if (state.sprite_frame >= 0 && sprites_[state.sprite_frame].is_valid())
    {
        const auto &source = session_->sprites()[state.sprite_frame];
        const Vector2 sprite_size(source.width * pixel_scale.x, source.height * pixel_scale.y);
        draw_texture_rect(sprites_[state.sprite_frame],
                          Rect2(view.position + Vector2((view.size.x - sprite_size.x) * .5,
                                  view.size.y - sprite_size.y),
                                sprite_size),
                          false);
    }
}

void RolfTourView::draw_map()
{
    if (!session_)
        return;
    const auto &s = session_->snapshot();
    if (s.visited.none())
        return; // The opening script has not placed the party yet.
    const double cell = map_rect_.size.x / 16;
    for (unsigned y = 0; y < 16; ++y)
        for (unsigned x = 0; x < 16; ++x)
        {
            const auto origin = map_rect_.position + Vector2(x * cell, y * cell);
            // The override affects drawing only. It must never discover cells or
            // alter the history later written to a campaign save.
            if (!no_fog_ && !s.seen.test(y * 16 + x))
                continue;
            draw_rect(Rect2(origin, Vector2(cell, cell)),
                      s.visited.test(y * 16 + x) ? Color("304747") : Color("253038"));
            draw_rect(Rect2(origin, Vector2(cell, cell)), Color("1b252b"), false);
            const auto &c = session_->map().at(x, y);
            const double inset = 1.3;
            const std::array<Vector2, 4> corners{origin + Vector2(inset, inset),
                                                 origin + Vector2(cell - inset, inset),
                                                 origin + Vector2(cell - inset, cell - inset),
                                                 origin + Vector2(inset, cell - inset)};
            for (unsigned d = 0; d < 4; ++d)
            {
                if (c.walls[d])
                    draw_line(corners[d], corners[(d + 1) % 4], Color("9ca8a7"), 1.5);
                if (c.doors[d])
                    draw_line(corners[d].lerp(corners[(d + 1) % 4], .27),
                              corners[d].lerp(corners[(d + 1) % 4], .73), gold, 3);
            }
        }
    const auto center =
        map_rect_.position + Vector2((s.pose.x + .5) * cell, (s.pose.y + .5) * cell);
    const auto forward = direction[index(s.pose.facing)], right = Vector2(-forward.y, forward.x);
    PackedVector2Array arrow;
    arrow.push_back(center + forward * cell * .43);
    arrow.push_back(center - forward * cell * .3 + right * cell * .32);
    arrow.push_back(center - forward * cell * .16);
    arrow.push_back(center - forward * cell * .3 - right * cell * .32);
    draw_circle(center, cell * .45, background);
    draw_colored_polygon(arrow, party_color);
}

void RolfTourView::capture_frame(const String &name)
{
    if (!capture_)
        return;
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
            for (unsigned x = 0; x < 16; ++x)
            {
                if (x == state.pose.x && y == state.pose.y)
                    continue; // Party arrow covers its cell.
                const auto center = get_global_transform_with_canvas().xform(
                                        map_rect_.position + Vector2((x + .5) * cell, (y + .5) * cell));
                const auto expected = !no_fog_ && !state.seen.test(y * 16 + x) ? Color(0, 0, 0)
                                      : state.visited.test(y * 16 + x)         ? Color("304747")
                                      : Color("253038");
                if (!image->get_pixel(center.x, center.y).is_equal_approx(expected))
                    throw std::runtime_error("Overhead fog pixel mismatch at " + std::to_string(x) +
                                             "," + std::to_string(y));
            }
    }
    if (image.is_null() || image->save_png(path) != OK)
        throw std::runtime_error("Failed to capture tour scene");
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
            throw std::runtime_error(session_ ? session_->snapshot().diagnostic
                                     : error_.utf8().get_data());
        if (++check_frames_ > 2000)
            throw std::runtime_error("Tour integration check timed out; recovery stage " +
                                     std::to_string(recovery_stage_) + ", position " +
                                     std::to_string(session_->snapshot().pose.x) + "," +
                                     std::to_string(session_->snapshot().pose.y) + ", " +
                                     session_->snapshot().dialogue);
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
                    throw std::runtime_error("Incorrect input lock at tour prompt");
                UtilityFunctions::print("Tour pause ", check_prompts_, " at ", s.pose.x, ",",
                                        s.pose.y, " facing ", static_cast<int64_t>(index(s.pose.facing)));
                required_node<Button>(*this, "Restart").grab_focus();
                const auto pose = s.pose;
                press_key(Key::KEY_RIGHT);
                if (session_->snapshot().pose != pose)
                    throw std::runtime_error("Keyboard bypassed tour movement lock");
                press_key(Key::KEY_ENTER, true);
                if (session_->snapshot().phase != TourPhase::awaiting_continue)
                    throw std::runtime_error("Held Enter skipped a prompt");
                press_key(Key::KEY_ENTER);
                if (session_->snapshot().phase != TourPhase::running)
                    throw std::runtime_error(
                        "Enter did not continue with a different button focused");
            }
        }
        else if (s.phase == TourPhase::completed)
        {
            if (check_prompts_ != 8)
                throw std::runtime_error("Expected all eight original dialogue pauses");
            required_node<Button>(*this, "Restart").grab_focus();
            const auto facing = s.pose.facing;
            press_key(Key::KEY_RIGHT);
            if (session_->snapshot().pose.facing != turned_right(facing))
                throw std::runtime_error("Exploration turn did not update native state");
            const auto before = session_->snapshot().pose;
            press_key(Key::KEY_DOWN);
            if (session_->snapshot().pose != PartyPose{before.x, before.y, reversed(before.facing)})
                throw std::runtime_error("Down arrow must turn without moving");
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
        return campaign_
               ? coins(campaign_->member(campaign_->selected()).wealth, Coin::gold)
        : coins(session_->party().wealth, Coin::gold);
    };
    if (check_prompts_ != 8)
        throw std::runtime_error("Town started before the full tour");
    if (shop_check_stage_ == 3 && s.phase == TourPhase::completed)
    {
        if (inventory_size() != 1 || session_->script_variable(EclAddress{0x6BC1}) != gold())
            throw std::runtime_error("Shop results did not persist after leaving");
        party_selected(0);
        auto *sheet =
            &required_node<Window>(*this, embedded_party_ ? "../TownSheet" : "MemberSheet");
        if (!sheet->is_visible())
            throw std::runtime_error("Town party click did not open a character sheet");
        sheet->emit_signal("close_requested");
        if (sheet->is_visible())
            throw std::runtime_error("Town character sheet did not close");
        UtilityFunctions::print(
            "Godot town integration passed: tour, doors, shop purchase, inventory and return to exploration.");
        if (!embedded_party_)
            get_tree()->quit(0);
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
        if (shop_check_stage_ == 1)
        {
            capture_frame("phlan-shop");
            const auto gold_before = gold();
            const auto price = session_->shop_stock()[0].stored.value;
            if (required_node<ItemList>(*this, "Choices").get_item_count() != 57 ||
                    required_node<Button>(*this, "Continue").is_disabled())
                throw std::runtime_error("Arms shop list is not available in Godot");
            required_node<Button>(*this, "Continue").emit_signal("pressed");
            if (gold() != gold_before - price || inventory_size() != 1)
                throw std::runtime_error(
                    "Godot purchase did not debit the purse and add inventory");
            required_node<Button>(*this, "Inventory").emit_signal("pressed");
            shop_check_stage_ = 2;
            return;
        }
        required_node<ItemList>(*this, "InventoryPanel/Items").select(0);
        const auto id = campaign_->selected();
        const auto ac = campaign_->profile(id).armor_class;
        required_node<Button>(*this, "InventoryPanel/Equip").emit_signal("pressed");
        if (campaign_->profile(id).armor_class != ac + 2)
            throw std::runtime_error("Inventory Equip did not apply shield AC");
        required_node<Button>(*this, "InventoryPanel/Unequip").emit_signal("pressed");
        if (campaign_->profile(id).armor_class != ac)
            throw std::runtime_error("Inventory Unequip did not remove shield AC");
        required_node<Button>(*this, "InventoryPanel/Equip").emit_signal("pressed");
        const auto retained = campaign_->checkpoint();
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
        campaign_->restore(changed);
        refresh_inventory();
        required_node<ItemList>(*this, "InventoryPanel/Items").select(0);
        required_node<Button>(*this, "InventoryPanel/Equip").emit_signal("pressed");
        if (!required_node<Label>(*this, "InventoryPanel/Status")
                .get_text()
                .contains("Untrained shield: no AC bonus"))
            throw std::runtime_error("Equip must display the untrained penalty");
        auto staff_fixture = retained;
        auto &wielder = staff_fixture.roster.at(0);
        const auto staff = wielder.character.add_item({.definition_id = "quarterstaff",
                                                              .name = "Quarterstaff",
                                                              .quantity = 1});
        wielder.equipped.push_back(staff);
        campaign_->restore(staff_fixture);
        refresh_inventory();
        if (campaign_->profile(id).weapon_hands != 1)
            throw std::runtime_error("A shield must keep the quarterstaff one-handed");
        required_node<ItemList>(*this, "InventoryPanel/Items").select(0);
        required_node<Button>(*this, "InventoryPanel/Unequip").emit_signal("pressed");
        if (campaign_->profile(id).weapon_hands != 2)
            throw std::runtime_error("Removing the shield must free the second hand");
        campaign_->restore(retained);
        refresh_inventory();
        capture_frame("phlan-inventory");
        required_node<Button>(*this, "InventoryPanel/Close").emit_signal("pressed");
        required_node<Button>(*this, "LeaveShop").emit_signal("pressed");
        shop_check_stage_ = 3;
        return;
    }
    if (s.phase == TourPhase::awaiting_continue)
    {
        std::size_t selection = s.choices.size() - 1;
        for (const auto *safe :
                {"NO", "LEAVE", "RUN", "GO", "NONE", "EXIT"
                })
            for (std::size_t n = 0; n < s.choices.size(); ++n)
                if (s.choices[n] == safe)
                    selection = n;
        if (s.pose.x == 13 && s.pose.y == 8 && s.dialogue.find("SHOP") != std::string::npos)
            selection = 0;
        if (campaign_ && s.dialogue == "Choose a party member.")
            selection = 0;
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
        return;
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
            check_refused_edges_.insert(*check_pending_edge_);
        check_pending_edge_.reset();
    }
    constexpr int dx[] {0, 1, 0, -1}, dy[] {-1, 0, 1, 0};
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
                continue;
            const auto &a = session_->map().at(cell % 16, cell / 16);
            const auto &b = session_->map().at(x, y);
            const int r = (d + 2) % 4;
            if (a.doors[d] > 1 || b.doors[r] > 1 || (a.walls[d] && !a.doors[d]) ||
                    (b.walls[r] && !b.doors[r]))
                continue;
            const int next = y * 16 + x;
            if (previous[next] >= 0 || check_refused_edges_.contains({cell, next}))
                continue;
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
        throw std::runtime_error("No route to acceptance target " + std::to_string(tx) + "," +
                                 std::to_string(ty) + " from " + std::to_string(s.pose.x) + "," +
                                 std::to_string(s.pose.y) + "; blocked edges " +
                                 std::to_string(check_refused_edges_.size()));
    while (previous[next] != origin)
        next = previous[next];
    const auto facing = next % 16 > int(s.pose.x)   ? MapDirection::east
                        : next % 16 < int(s.pose.x) ? MapDirection::west
                        : next / 16 > int(s.pose.y) ? MapDirection::south
                        : MapDirection::north;
    if (s.pose.facing == facing)
        check_pending_edge_ = {{origin, next}};
    required_node<Button>(*this, s.pose.facing == facing ? "Forward" : "Right")
    .emit_signal("pressed");
}

void RolfTourView::check_district_labels(const TourSnapshot &s)
{
    // Labels follow the session at the next refresh, not immediately.
    if (shown_revision_ != s.revision)
        return;
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
        throw std::runtime_error("District labels do not name area " + std::to_string(s.area_id) +
                                 ": " + (title + String(" | ") + location + String(" | ") + map_title + String(" | ") + speaker).utf8().get_data());
    if (s.area_id == 20)
        slums_labels_checked_ = true;
}

bool RolfTourView::check_expedition_step()
{
    if (!session_)
        throw std::runtime_error("Expedition session missing");
    const auto &s = session_->snapshot();
    if (!session_->script_diagnostics().empty())
        throw std::runtime_error(session_->script_diagnostics().back());
    if (s.phase == TourPhase::faulted)
        throw std::runtime_error(s.diagnostic);
    check_district_labels(s);
    if (shown_monster_picture_)
    {
        // Let the close-up animate for about two seconds before pressing a key.
        if (++monster_picture_check_frames_ < 120)
            return false;
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
        return false;
    if (s.area_id == 0)
    {
        if (session_->script_variable(ecl_slums_orc_victory) == 255)
        {
            if (!slums_labels_checked_)
                throw std::runtime_error("Slums district labels were never checked");
            return true;
        }
        if (s.pose.x != 0 || s.pose.y != 4)
            throw std::runtime_error("Unexpected tour destination");
        if (s.pose.facing != MapDirection::west)
            right();
        else
            forward();
        return false;
    }
    if (session_->script_variable(ecl_slums_orc_victory) == 255)
    {
        if (s.pose.x != 15 || s.pose.y != 4)
            check_walk_to(15, 4);
        else if (s.pose.facing != MapDirection::east)
            right();
        else
            forward();
    }
    else
        check_walk_to(12, 1);
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
                            state.roster.at(1).character,
    {.stable = true, .hit_dice = 1, .stable_recovery_ms = 2000});
    const auto dying = presentation::srd_vitals(
                           state.roster.back().character,
    {.successes = 2, .failures = 1, .hit_dice = 1, .death_save_ms = 6000});
    state.roster.at(1).vitals = {0, false, stable};
    state.roster.back().vitals = {0, false, dying};
    // Gold alone pays the temple and, with change made, the inn's one platinum.
    if (member.vitals.dead)
        throw std::runtime_error("Recovery check requires a living victory survivor");
    member.vitals.hit_points = 1;
    member.wealth = {0, 0, 0, 200, 0, 0, 0};
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
        throw std::runtime_error("Recovery route fault: " + session_->script_diagnostics().back());
    const auto choose = [&](std::size_t choice)
    {
        required_node<ItemList>(*this, "Choices").select(static_cast<std::int32_t>(choice));
        required_node<Button>(*this, "Continue").emit_signal("pressed");
    };
    if (s.phase == TourPhase::awaiting_continue)
    {
        if (recovery_stage_ == 3 && !s.choices.empty() && s.choices[0].starts_with("Cure Wounds:"))
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
            recovery_before_ = campaign_->checkpoint();
            capture_frame("party-temple");
            choose(0);
            recovery_stage_ = 4;
            return;
        }
        std::size_t selection = s.choices.size() - 1;
        for (const auto *safe :
                {"NO", "LEAVE", "RUN", "GO", "NONE", "EXIT", "Cancel"
                })
            for (std::size_t n = 0; n < s.choices.size(); ++n)
                if (s.choices[n] == safe)
                    selection = n;
        const auto event = session_->map().at(s.pose.x, s.pose.y).event_number();
        // Temple cells can be transit cells: enter, then cancel the native service.
        // Answering NO makes the original script move the party back to its prior cell.
        if (((recovery_stage_ == 3 || recovery_stage_ == 5) && event == 6) ||
                (recovery_stage_ == 5 && event == 9))
        {
            for (std::size_t n = 0; n < s.choices.size(); ++n)
                if (s.choices[n] == "YES")
                    selection = n;
            if (s.dialogue == "Choose a party member.")
                selection = 0;
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
        return;
    const auto &member = campaign_->member(id);
    if (save_cancel_pending_)
    {
        if (member.wealth != recovery_before_->roster.at(0).wealth ||
                member.vitals != recovery_before_->roster.at(0).vitals)
            throw std::runtime_error("Cancelled temple service changed party");
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
            throw std::runtime_error(
                "Original city-watch interruption must consume five minutes without recovery");
        if (campaign_->state().roster.at(1).vitals.hit_points != 1 ||
                campaign_->state().roster.back().vitals.hit_points != 1 ||
                campaign_->state().random_state.value != 11400714819323198502ULL ||
                campaign_->state().roster.back().vitals.resources !=
                presentation::srd_vitals(campaign_->state().roster.back().character,
    {.hit_dice = 1}))
            throw std::runtime_error(
                "Camp time must advance companion Stable recovery and the reserve death save exactly once");
        if (save_check_)
            save_check_("interrupted-rest");
        recovery_stage_ = 3;
    }
    if (recovery_stage_ == 4)
    {
        const auto gold_before = coins(recovery_before_->roster.at(0).wealth, Coin::gold);
        if (coins(member.wealth, Coin::gold) != gold_before - 100 ||
                member.vitals.hit_points <= 1 || session_->script_variable(ecl_temple_service) != 0)
            throw std::runtime_error("Original temple must charge 100 gp, heal and resume ECL");
        if (save_check_)
            save_check_("temple-payment");
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
                coins(member.wealth, Coin::gold) !=
                coins(recovery_before_->roster.at(0).wealth, Coin::gold) - 10)
            throw std::runtime_error("Original inn payment and full recovery must persist");
        const auto &payments = s.payments;
        const auto shown = required_node<RichTextLabel>(*this, "Dialogue").get_text();
        if (payments.size() != 1 || payments[0].coins.paid != opengold::Coins{0, 0, 0, 10, 0} ||
                !shown.contains(payment_notice(payments[0])))
            throw std::runtime_error("The inn's change-making payment must be shown");
        // The Fighter's inn rest offers an optional mastery replacement; keep the
        // current set so the rested party is saved at an editable boundary.
        if (!campaign_->state().training_rest)
            throw std::runtime_error("The inn rest must offer the Fighter's mastery choice");
        required_node<Button>(*this, "RestTraining/Cancel").emit_signal("pressed");
        if (campaign_->state().training_rest)
            throw std::runtime_error("Keeping the mastery set must finish the rest choices");
        if (save_check_)
            save_check_("inn-rest");
        if (save_check_)
        {
            bool rejected = false;
            try
            {
                campaign_->temple_heal(id);
            }
            catch (const std::exception &)
            {
                rejected = true;
            }
            if (!rejected)
                throw std::runtime_error("Full-health temple service should reject");
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
            throw std::runtime_error("Immediate repeated long rest must be denied");
        required_node<Button>(required_node<Window>(*this, "RestDialog"), "Finish")
        .emit_signal("pressed");
        if (save_check_)
            save_check_("denied-rest");
        if (save_check_)
        {
            // A wounded member gives the reloaded rest something to heal; the
            // route then continues from the unwounded party.
            const auto rested = campaign_->checkpoint();
            auto wounded = rested;
            wounded.roster.at(0).vitals.hit_points = 1;
            campaign_->restore(std::move(wounded));
            (void)campaign_->rest(opengold::RestKind::short_rest);
            save_check_("short-rest-spending");
            campaign_->finish_short_rest(campaign_->state().short_rest->ticket);
            campaign_->restore(rested);
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
        for (unsigned x = 0; x < 16; ++x)
            if (session_->map().at(x, y).event_number() == target_event)
            {
                check_walk_to(x, y);
                return;
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
