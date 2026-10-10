#include "rolf_tour_view.h"
#include "opengold/exploration_view.h"
#include "opengold/srd5.h"
#include "../../../src/OpenGoldBox/godot_path.h"
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
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/viewport_texture.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <vector>
#include <queue>

using namespace godot;
using namespace opengold::por;

namespace
{
struct DeleteNode
{
    void operator()(Node *n) const
    {
        memdelete(n);
    }
};

// Numeric constructors are safe before Godot initializes the extension interface.
const Color background(18 / 255.f, 26 / 255.f, 32 / 255.f),
      panel(28 / 255.f, 39 / 255.f, 46 / 255.f), line(65 / 255.f, 80 / 255.f, 88 / 255.f),
      gold(215 / 255.f, 180 / 255.f, 121 / 255.f), party_color(121 / 255.f, 214 / 255.f, 212 / 255.f);
const std::array<Vector2, 4> direction{Vector2(0, -1), Vector2(1, 0), Vector2(0, 1),
    Vector2(-1, 0)};
const std::array<const char *, 4> direction_name{"North", "East", "South", "West"};

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
    ready_ = false;
    set_texture_filter(CanvasItem::TEXTURE_FILTER_NEAREST);
    // Child nodes are scene-owned; these lookups are temporary non-owning views.
    get_node<Button>("Continue")->connect("pressed", callable_mp(this, &RolfTourView::next));
    get_node<Button>("Restart")->connect("pressed", callable_mp(this, &RolfTourView::restart));
    get_node<Button>("Left")->connect("pressed", callable_mp(this, &RolfTourView::left));
    get_node<Button>("Right")->connect("pressed", callable_mp(this, &RolfTourView::right));
    get_node<Button>("Forward")->connect("pressed", callable_mp(this, &RolfTourView::forward));
    get_node<Button>("MapMode")->connect("pressed", callable_mp(this, &RolfTourView::map_mode));
    get_node<Button>("Look")->connect("pressed", callable_mp(this, &RolfTourView::look));
    get_node<Button>("Camp")->connect("pressed", callable_mp(this, &RolfTourView::camp));
    get_node<Button>("Inventory")->connect("pressed", callable_mp(this, &RolfTourView::inventory));
    get_node<Button>("InventoryPanel/Close")
    ->connect("pressed", callable_mp(this, &RolfTourView::inventory));
    for (unsigned slot = 0; slot < 8; ++slot)
    {
        auto *member = get_node<Button>(String("PartyList/Rows/Member") + String::num_uint64(slot));
        member->connect("pressed", callable_mp(this, &RolfTourView::party_selected).bind(slot));
        std::unique_ptr<Button, DeleteNode> arrow(memnew(Button));
        arrow->set_name("Advance");
        arrow->set_text(String::utf8("↑"));
        arrow->set_size(Vector2(30, 26));
        arrow->set_tooltip_text("Level up");
        arrow->connect("pressed", callable_mp(this, &RolfTourView::level_up_requested).bind(slot));
        member->add_child(arrow.get());
        arrow.release();
    }
    get_node<ItemList>("InventoryPanel/Items")
    ->connect("item_selected", callable_mp(this, &RolfTourView::inventory_selected));
    get_node<Button>("InventoryPanel/Equip")
    ->connect("pressed", callable_mp(this, &RolfTourView::equip_item).bind(true));
    get_node<Button>("InventoryPanel/Unequip")
    ->connect("pressed", callable_mp(this, &RolfTourView::equip_item).bind(false));
    get_node<Button>("MemberSheet/Close")
    ->connect("pressed", callable_mp(this, &RolfTourView::close_sheet));
    get_node<Window>("MemberSheet")
    ->connect("close_requested", callable_mp(this, &RolfTourView::close_sheet));
    get_node<Button>("LeaveShop")->connect("pressed", callable_mp(this, &RolfTourView::leave_shop));
    get_window()->set_min_size(Vector2i(960, 720));
    for (bool saving :
            {
                true, false
            })
    {
        std::unique_ptr<Button, DeleteNode> button(memnew(Button));
        button->set_name(saving ? "SaveGame" : "LoadGame");
        button->set_text(saving ? "Save game" : "Load game");
        add_child(button.get());
        button->connect("pressed", callable_mp(this, &RolfTourView::request_save).bind(saving));
        button->set_visible(embedded_party_);
        button.release();
    }
    layout();
    if (Engine::get_singleton()->is_editor_hint())
        return;
    const auto args = OS::get_singleton()->get_cmdline_user_args();
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
    get_node<AudioStreamPlayer>("Footstep")->set_stream(footstep_);
    ready_ = true;
    setup_rest();
    restart();
    if (embedded_party_)
        get_node<Button>("Restart")->hide();
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
        auto *node = get_node<Control>(name);
        node->set_position(rect.position);
        node->set_size(rect.size);
    };
    place("SaveGame", Rect2(width - 520, 20, 140, 36));
    place("LoadGame", Rect2(width - 370, 20, 140, 36));
    place("Title", Rect2(margin, 20, main_width, 34));
    place("PartyList", Rect2(scene_rect_.get_end().x + 16, 102,
                             main_width - scene_rect_.size.x - 16, view_height));
    place("Location", Rect2(margin, 68, main_width, 26));
    place("MapTitle", Rect2(map_rect_.position.x, 68, sidebar - 130, 26));
    place("MapMode", Rect2(width - margin - 122, 64, 122, 30));
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
    get_node<Label>("Legend")->hide();
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
    auto *inventory_panel = get_node<Control>("InventoryPanel");
    get_node<Control>("InventoryPanel/Items")->set_position(Vector2(20, 70));
    get_node<Control>("InventoryPanel/Items")
    ->set_size(inventory_panel->get_size() - Vector2(40, 210));
    get_node<Control>("InventoryPanel/Close")
    ->set_position(Vector2(20, inventory_panel->get_size().y - 54));
    get_node<Control>("InventoryPanel/Close")->set_size(Vector2(180, 36));
    const auto iw = inventory_panel->get_size().x, ih = inventory_panel->get_size().y;
    get_node<Control>("InventoryPanel/Header")->set_position(Vector2(20, 18));
    get_node<Control>("InventoryPanel/Header")->set_size(Vector2(iw - 40, 44));
    get_node<Control>("InventoryPanel/Status")->set_position(Vector2(20, ih - 132));
    get_node<Control>("InventoryPanel/Status")->set_size(Vector2(iw - 40, 68));
    get_node<Control>("InventoryPanel/Equip")->set_position(Vector2(220, ih - 54));
    get_node<Control>("InventoryPanel/Equip")->set_size(Vector2(150, 36));
    get_node<Control>("InventoryPanel/Unequip")->set_position(Vector2(390, ih - 54));
    get_node<Control>("InventoryPanel/Unequip")->set_size(Vector2(150, 36));
    get_node<Window>("MemberSheet")->set_size(Vector2i(width - 120, height - 120));
    place("MemberSheet/Text", Rect2(24, 24, width - 168, height - 220));
    place("MemberSheet/Close", Rect2(width - 290, height - 180, 130, 36));
}

void RolfTourView::restart()
{
    try
    {
        error_ = "";
        if (!campaign_ || !embedded_party_)
        {
            const auto pack = ProjectSettings::get_singleton()->globalize_path(
                                  "res://../../data/rules/srd-5.2.1/combat.rules");
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
            auto directory = OS::get_singleton()->get_environment("OPENGOLD_GAME_DIR");
            if (directory.is_empty())
                directory =
                    ProjectSettings::get_singleton()->get_setting("opengold/game_directory", "");
            session_.emplace(
                RolfTourSession::load(presentation::path_from_godot(directory)));
            if (campaign_)
                session_->campaign_party(campaign_);
            for (unsigned i = 0; i < sprites_.size(); ++i)
            {
                const auto &source = session_->sprites()[i];
                PackedByteArray pixels;
                pixels.resize(source.rgba.size());
                std::copy(source.rgba.begin(), source.rgba.end(), pixels.ptrw());
                const auto image = godot::Image::create_from_data(
                                       source.width, source.height, false, godot::Image::FORMAT_RGBA8, pixels);
                sprites_[i] = ImageTexture::create_from_image(image);
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
        error_ = String::utf8(error.what());
    }
    refresh();
}

void RolfTourView::next()
{
    if (!session_)
        return;
    const auto &s = session_->snapshot();
    const auto selected = get_node<ItemList>("Choices")->get_selected_items();
    if (s.phase == TourPhase::shopping)
    {
        if (!selected.is_empty())
            session_->buy(s.continue_ticket, selected[0]);
    }
    else if (s.phase == TourPhase::awaiting_input)
    {
        session_->input(s.continue_ticket,
                        get_node<LineEdit>("Answer")->get_text().utf8().get_data());
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
    auto *panel = get_node<Control>("InventoryPanel");
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
    auto *items = get_node<ItemList>("InventoryPanel/Items");
    items->clear();
    if (!campaign_ || !campaign_->selected())
        return;
    const auto &m = campaign_->member(campaign_->selected());
    get_node<Label>("InventoryPanel/Header")
    ->set_text(
        String::utf8((m.character.sheet().name + " / " + m.character.sheet().character_class +
                      " / " + std::to_string(m.wealth[3]) + " gp")
                     .c_str()));
    for (const auto &item : m.character.inventory().items())
        items->add_item(String::utf8(
                            ((std::find(m.equipped.begin(), m.equipped.end(), item.id) != m.equipped.end()
                              ? "Equipped / "
                              : "") +
                             item.name + " x" + std::to_string(item.quantity))
                            .c_str()));
    if (items->get_item_count())
        items->select(0);
    get_node<Button>("InventoryPanel/Equip")->set_disabled(!items->get_item_count());
    get_node<Button>("InventoryPanel/Unequip")->set_disabled(!items->get_item_count());
    get_node<Label>("InventoryPanel/Status")
    ->set_text("Inventory is empty. Visit a shop to buy equipment.");
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
        get_node<Label>("InventoryPanel/Status")
        ->set_text(String::utf8(
                       opengold::srd5::equipment_note(m.character.sheet(), items[index].definition_id)
                       .c_str()));
    }
    catch (const std::exception &e)
    {
        get_node<Label>("InventoryPanel/Status")->set_text(String::utf8(e.what()));
    }
}

void RolfTourView::equip_item(bool equip)
{
    try
    {
        const auto selection = get_node<ItemList>("InventoryPanel/Items")->get_selected_items();
        if (selection.is_empty())
            throw std::runtime_error("Select an item first.");
        const auto id = campaign_->selected();
        const auto &m = campaign_->member(id);
        const auto items = m.character.inventory().items();
        if (selection[0] < 0 || static_cast<std::size_t>(selection[0]) >= items.size())
            throw std::runtime_error("Select an existing item.");
        const auto item = items[selection[0]].id;
        if (equip)
            campaign_->equip(id, item);
        else
            campaign_->unequip(id, item);
        refresh_inventory();
        get_node<ItemList>("InventoryPanel/Items")->select(selection[0]);
        refresh();
        get_node<Label>("InventoryPanel/Status")
        ->set_text(equip ? String::utf8(("Equipped. " + opengold::srd5::equipment_note(
                                             m.character.sheet(),
                                             items[selection[0]].definition_id))
                                        .c_str())
                   : String("Item unequipped."));
    }
    catch (const std::exception &e)
    {
        get_node<Label>("InventoryPanel/Status")->set_text(String::utf8(e.what()));
    }
}

void RolfTourView::party_selected(std::int64_t index)
{
    if (index < 0 || index >= 8 || !session_ || !campaign_ || !campaign_->state().slots[index])
        return;
    const auto slot = static_cast<unsigned>(index);
    if (session_->can_leave())
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
            text += std::string(names[i]) + ": " + std::to_string(s.scores[ability]) +
                    " / Save " + std::to_string(s.saving_throws[ability]) + "\n";
        }
        get_node<RichTextLabel>("MemberSheet/Text")->set_text(String::utf8(text.c_str()));
        get_node<Window>("MemberSheet")->popup_centered();
    }
    refresh();
}

void RolfTourView::close_sheet()
{
    get_node<Window>("MemberSheet")->hide();
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
        get_node<Label>("Movement")->set_text("The way is blocked");
}

void RolfTourView::map_mode()
{
    full_map_ = !full_map_;
    refresh();
}

void RolfTourView::_input(const Ref<InputEvent> &event)
{
    if (get_node<Window>("RestDialog")->is_visible())
        return;
    const Ref<InputEventKey> key = event;
    if (key.is_null() || !key->is_pressed() || key->is_echo())
        return;
    if (get_node<Control>("InventoryPanel")->is_visible())
        return;
    if (get_node<LineEdit>("Answer")->has_focus() && key->get_keycode() != Key::KEY_ENTER)
        return;
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
                get_node<AudioStreamPlayer>("Footstep")->play();
        }
        if (session_->snapshot().revision != shown_revision_)
            refresh();
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

void RolfTourView::refresh()
{
    layout();
    // The reference demo does not draw monster close-ups; start combat at once.
    if (session_ && session_->monster_picture())
        session_->start_encounter();
    if (session_ && rendered_sprite_id_ != session_->snapshot().sprite_id)
    {
        for (unsigned n = 0; n < 3; ++n)
        {
            const auto &source = session_->sprites()[n];
            PackedByteArray pixels;
            pixels.resize(source.rgba.size());
            std::copy(source.rgba.begin(), source.rgba.end(), pixels.ptrw());
            sprites_[n] = ImageTexture::create_from_image(godot::Image::create_from_data(
                    source.width, source.height, false, godot::Image::FORMAT_RGBA8, pixels));
        }
        rendered_sprite_id_ = session_->snapshot().sprite_id;
    }
    if (session_ && (!rendered_pose_ || *rendered_pose_ != session_->snapshot().pose ||
                     rendered_picture_revision_ != session_->snapshot().picture_revision))
    {
        try
        {
            const auto pose = session_->snapshot().pose;
            const auto source =
                session_->picture()
                ? *session_->picture()
                : compose_exploration_view(session_->map(), session_->wall_art(), pose.x,
                                           pose.y, pose.facing);
            PackedByteArray pixels;
            pixels.resize(source.rgba.size());
            std::copy(source.rgba.begin(), source.rgba.end(), pixels.ptrw());
            const auto image = godot::Image::create_from_data(source.width, source.height, false,
                godot::Image::FORMAT_RGBA8, pixels);
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
            error_ = String::utf8(error.what());
        }
    }
    const bool loaded = session_.has_value();
    const TourSnapshot s = loaded ? session_->snapshot() : TourSnapshot{};
    shown_revision_ = s.revision;
    refresh_rest();
    const bool waiting = loaded && s.phase == TourPhase::awaiting_continue;
    const bool completed = loaded && s.phase == TourPhase::completed;
    get_node<Button>("SaveGame")->set_disabled(!completed);
    get_node<Button>("LoadGame")->set_disabled(!completed);
    const bool faulted = !loaded || s.phase == TourPhase::faulted;
    const bool shopping = loaded && s.phase == TourPhase::shopping;
    const bool answer = loaded && s.phase == TourPhase::awaiting_input;
    const bool multiple = waiting && s.choices.size() > 1;
    get_node<Label>("Location")
    ->set_text("New Phlan  /  " + String(direction_name[index(s.pose.facing)]) + " view");
    get_node<Label>("Coordinates")
    ->set_text("Party  (" + String::num_uint64(s.pose.x) + ", " + String::num_uint64(s.pose.y) +
               ")   " + String(direction_name[index(s.pose.facing)]));
    get_node<Label>("Speaker")->set_text(faulted           ? "Unable to continue"
                                         : shopping        ? "Shop / select an item"
                                         : s.tour_finished ? "New Phlan"
                                         : "Rolf  /  Council guide");
    get_node<RichTextLabel>("Dialogue")
    ->set_text(
        faulted
        ? (loaded ? String::utf8(s.diagnostic.c_str()) : error_) +
        "\nSet OPENGOLD_GAME_DIR to your Pool of Radiance data folder, then restart."
        : s.dialogue.empty() ? "Following Rolf..."
        : String::utf8(s.dialogue.c_str()));
    get_node<Button>("Continue")->set_disabled(!waiting && !shopping && !answer);
    get_node<Button>("Continue")
    ->set_text(shopping   ? "Buy [Enter]"
               : multiple ? "Choose [Enter]"
               : answer   ? "Submit [Enter]"
               : "Continue [Enter]");
    if (waiting && s.choices.size() == 1 && s.choices[0] == "Cancel")
        get_node<Button>("Continue")->set_text("Leave temple [Enter]");
    get_node<Button>("Continue")->set_visible(!completed);
    get_node<Label>("Progress")
    ->set_text(faulted           ? "Stopped"
               : s.tour_finished ? ""
               : waiting         ? "Pause " + String::num_uint64(s.prompts)
               : "Following the guide");
    get_node<Label>("Movement")->set_text(completed ? "Explore  /  arrow keys" : "Movement paused");
    for (const char *name :
            {"Left", "Forward", "Right", "Look", "Camp"
            })
        get_node<Button>(name)->set_disabled(!completed);
    get_node<Button>("LeaveShop")->set_visible(shopping);
    get_node<LineEdit>("Answer")->set_visible(answer);
    get_node<LineEdit>("Answer")->set_max_length(s.number_input ? 6 : 40);
    auto *choices = get_node<ItemList>("Choices");
    choices->set_visible(shopping || multiple);
    if (displayed_ticket_ != s.continue_ticket)
    {
        choices->clear();
        if (shopping)
            for (const auto &item : session_->shop_stock())
                choices->add_item(String::utf8(
                                      (item.label() + " — " + std::to_string(item.stored.value) + " gp").c_str()));
        else
            for (const auto &c : s.choices)
                choices->add_item(String::utf8(c.c_str()));
        if (choices->get_item_count())
            choices->select(0);
        if (shopping || multiple)
            choices->grab_focus();
        if (answer)
        {
            get_node<LineEdit>("Answer")->clear();
            get_node<LineEdit>("Answer")->grab_focus();
        }
        displayed_ticket_ = s.continue_ticket;
    }
    get_node<RichTextLabel>("Dialogue")
    ->set_size(
        Vector2(dialogue_rect_.size.x - 36,
                (shopping || multiple) ? 36 : dialogue_rect_.size.y - (answer ? 160 : 112)));
    if (multiple)
    {
        const double text_height = std::min((dialogue_rect_.size.y - 122) * .55,
                                            std::max<double>(28.0, dialogue_rect_.size.y - 192));
        get_node<RichTextLabel>("Dialogue")
        ->set_size(Vector2(dialogue_rect_.size.x - 36, text_height));
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
        get_node<RichTextLabel>("Dialogue")
        ->set_text(s.diagnostic.empty() ? "Prices are per listed item or bundle."
                   : String::utf8(s.diagnostic.c_str()));
    if (answer && !s.diagnostic.empty())
        get_node<RichTextLabel>("Dialogue")->add_text("\n" + String::utf8(s.diagnostic.c_str()));
    if (waiting && !s.diagnostic.empty())
        get_node<RichTextLabel>("Dialogue")
        ->set_text(String::utf8((s.diagnostic + "\n" + s.dialogue).c_str()));
    if (loaded)
    {
        const auto &p = session_->party();
        get_node<Label>("Party")->set_text(
            "Fighter  /  Level 1  /  HP " + String::num_uint64(p.hit_points) + "/" +
            String::num_uint64(p.max_hit_points) + "\n" + String::num_uint64(p.wealth[3]) +
            " gp  /  " + String::num_uint64(p.inventory.size()) + " items");
    }
    if (loaded && campaign_ && campaign_->selected())
    {
        const auto &m = campaign_->member(campaign_->selected());
        get_node<Label>("Party")->set_text(String::utf8(
                                               (m.character.sheet().name + " / HP " + std::to_string(m.vitals.hit_points) + "/" +
                                                std::to_string(m.character.sheet().hit_points) + "\n" + std::to_string(m.wealth[3]) +
                                                " gp / " + std::to_string(m.character.inventory().items().size()) + " items")
                                               .c_str()));
    }
    for (unsigned slot = 0; slot < 8; ++slot)
    {
        auto *button = get_node<Button>(String("PartyList/Rows/Member") + String::num_uint64(slot));
        const auto id = campaign_ ? campaign_->state().slots[slot] : 0;
        button->set_visible(id != 0);
        if (!id)
            continue;
        const auto &m = campaign_->member(id);
        const auto &cs = m.character.sheet();
        const auto text = cs.name + "\n" + cs.character_class + " / AC " +
                          std::to_string(campaign_->profile(id).armor_class) + " / HP " +
                          std::to_string(m.vitals.hit_points) + "/" + std::to_string(cs.hit_points);
        button->set_text(String::utf8(text.c_str()));
        button->set_tooltip_text(String::utf8(text.c_str()));
        auto *arrow = button->get_node<Button>("Advance");
        const auto width = Vector2(button->get_theme_font("font")->call(
                                       "get_string_size", String::utf8(cs.name.c_str()), 0, -1,
                                       button->get_theme_font_size("font_size")))
                           .x;
        arrow->set_position(Vector2(std::min(width + 16, button->get_size().x - 36), 2));
        arrow->set_visible(embedded_party_ && campaign_->can_advance(id) && session_->can_leave());
    }
    get_node<Button>("MapMode")->set_text(full_map_ ? "Map: full" : "Map: visited");
    queue_redraw();
}

void RolfTourView::_draw()
{
    draw_rect(Rect2(Vector2(), get_size()), background);
    draw_line(Vector2(24, 57), Vector2(get_size().x - 24, 57), line);
    draw_rect(dialogue_rect_, panel);
    draw_rect(dialogue_rect_, line, false);
    draw_line(dialogue_rect_.position, dialogue_rect_.position + Vector2(dialogue_rect_.size.x, 0),
              gold, 2);
    draw_rect(map_rect_, Color("162128"));
    draw_scene();
    draw_map();
    draw_rect(scene_rect_, line, false);
    draw_rect(map_rect_, line, false);
}

void RolfTourView::draw_scene()
{
    draw_rect(scene_rect_, panel);
    if (!session_ || wall_view_.is_null())
        return;
    // Fit the complete original 88x88 view. DOS EGA pixels were displayed 6/5
    // as tall as wide; letterboxing preserves art and door framing on resize.
    const double scale = std::min(scene_rect_.size.x / 88.0, scene_rect_.size.y / 105.6);
    const Vector2 pixel_scale(scale, scale * 1.2), size(88 * pixel_scale.x, 88 * pixel_scale.y);
    const Rect2 view(scene_rect_.position + (scene_rect_.size - size) * .5, size);
    draw_texture_rect(wall_view_, view, false);
    const auto &state = session_->snapshot();
    if (state.sprite_frame >= 0 && sprites_[state.sprite_frame].is_valid())
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
    const double cell = map_rect_.size.x / 16;
    for (unsigned y = 0; y < 16; ++y)
        for (unsigned x = 0; x < 16; ++x)
        {
            const auto origin = map_rect_.position + Vector2(x * cell, y * cell);
            if (!full_map_ && !s.visited.test(y * 16 + x))
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
    const auto directory =
        ProjectSettings::get_singleton()->globalize_path("res://../../user-data");
    std::filesystem::create_directories(presentation::path_from_godot(directory));
    const auto path = directory.path_join(name + String(".png"));
    const auto image = get_viewport()->get_texture()->get_image();
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
                if (get_node<Button>("Continue")->is_disabled() ||
                        !get_node<Button>("Forward")->is_disabled())
                    throw std::runtime_error("Incorrect input lock at tour prompt");
                UtilityFunctions::print("Tour pause ", check_prompts_, " at ", s.pose.x, ",",
                                        s.pose.y, " facing ",
                                        static_cast<int64_t>(index(s.pose.facing)));
                get_node<Button>("MapMode")->grab_focus();
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
            get_node<Button>("Restart")->grab_focus();
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
        return campaign_ ? campaign_->member(campaign_->selected()).wealth[3]
        : session_->party().wealth[3];
    };
    if (check_prompts_ != 8)
        throw std::runtime_error("Town started before the full tour");
    if (shop_check_stage_ == 3 && s.phase == TourPhase::completed)
    {
        if (inventory_size() != 1 || session_->script_variable(EclAddress{0x6BC1}) != gold())
            throw std::runtime_error("Shop results did not persist after leaving");
        party_selected(0);
        auto *sheet = get_node<Window>(embedded_party_ ? "../TownSheet" : "MemberSheet");
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
            if (get_node<ItemList>("Choices")->get_item_count() != 57 ||
                    get_node<Button>("Continue")->is_disabled())
                throw std::runtime_error("Arms shop list is not available in Godot");
            get_node<Button>("Continue")->emit_signal("pressed");
            if (gold() != gold_before - price || inventory_size() != 1)
                throw std::runtime_error(
                    "Godot purchase did not debit the purse and add inventory");
            get_node<Button>("Inventory")->emit_signal("pressed");
            shop_check_stage_ = 2;
            return;
        }
        get_node<ItemList>("InventoryPanel/Items")->select(0);
        const auto id = campaign_->selected();
        const auto ac = campaign_->profile(id).armor_class;
        get_node<Button>("InventoryPanel/Equip")->emit_signal("pressed");
        if (campaign_->profile(id).armor_class != ac + 2)
            throw std::runtime_error("Inventory Equip did not apply shield AC");
        get_node<Button>("InventoryPanel/Unequip")->emit_signal("pressed");
        if (campaign_->profile(id).armor_class != ac)
            throw std::runtime_error("Inventory Unequip did not remove shield AC");
        get_node<Button>("InventoryPanel/Equip")->emit_signal("pressed");
        const auto retained = campaign_->checkpoint();
        auto changed = retained;
        auto &member = changed.roster.at(0);
        auto inventory = member.character.inventory();
        auto draft = member.character.creation_data();
        draft.character_class = "wizard";
        draft.training.erase("class:fighter");
        draft.training.erase(
            "class:fighter:fighting_style"); // This untrained-shield fixture changes class.
        member.character = opengold::Character(*opengold::srd5::character_rules(), draft,
                                               member.character.appearance());
        member.character.replace_inventory(std::move(inventory));
        member.equipped.clear();
        member.vitals = {member.character.sheet().hit_points, false, {}};
        campaign_->restore(changed);
        refresh_inventory();
        get_node<ItemList>("InventoryPanel/Items")->select(0);
        get_node<Button>("InventoryPanel/Equip")->emit_signal("pressed");
        if (!get_node<Label>("InventoryPanel/Status")
                ->get_text()
                .contains("Untrained shield: no AC bonus"))
            throw std::runtime_error("Equip must display the untrained penalty");
        campaign_->restore(retained);
        refresh_inventory();
        capture_frame("phlan-inventory");
        get_node<Button>("InventoryPanel/Close")->emit_signal("pressed");
        get_node<Button>("LeaveShop")->emit_signal("pressed");
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
        get_node<ItemList>("Choices")->select(static_cast<std::int32_t>(selection));
        get_node<Button>("Continue")->emit_signal("pressed");
        return;
    }
    if (s.phase == TourPhase::awaiting_input)
    {
        get_node<LineEdit>("Answer")->set_text("0");
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
    const auto facing = next % 16 > static_cast<int>(s.pose.x)   ? MapDirection::east
                        : next % 16 < static_cast<int>(s.pose.x) ? MapDirection::west
                        : next / 16 > static_cast<int>(s.pose.y) ? MapDirection::south
                        : MapDirection::north;
    if (s.pose.facing == facing)
        check_pending_edge_ = {{origin, next}};
    get_node<Button>(s.pose.facing == facing ? "Forward" : "Right")->emit_signal("pressed");
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
    if (s.phase == TourPhase::awaiting_continue)
    {
        const auto choice = s.choices.size() == 5 && s.choices[0] == "Fight" ? 1 : 0;
        get_node<ItemList>("Choices")->select(choice);
        get_node<Button>("Continue")->emit_signal("pressed");
        return false;
    }
    if (s.phase != TourPhase::completed)
        return false;
    if (s.area_id == 0)
    {
        if (session_->script_variable(ecl_slums_orc_victory) == 255)
            return true;
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
    // Deterministic wounded fixture and one platinum for the original inn payment.
    auto state = campaign_->checkpoint();
    auto &member = state.roster.at(0);
    if (member.vitals.dead)
        throw std::runtime_error("Recovery check requires a living victory survivor");
    member.vitals.hit_points = 1;
    member.wealth[3] = 200;
    member.wealth[4] = 1;
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
        get_node<ItemList>("Choices")->select(static_cast<std::int32_t>(choice));
        get_node<Button>("Continue")->emit_signal("pressed");
    };
    if (s.phase == TourPhase::awaiting_continue)
    {
        if (recovery_stage_ == 3 && !s.choices.empty() && s.choices[0].starts_with("Cure Wounds:"))
        {
            if (save_check && !save_cancel_checked_)
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
        get_node<Button>("LeaveShop")->emit_signal("pressed");
        return;
    }
    if (s.phase == TourPhase::awaiting_input)
    {
        get_node<LineEdit>("Answer")->set_text("0");
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
        save_check("cancelled-service");
        save_cancel_pending_ = false;
    }
    if (recovery_stage_ == 1)
    {
        get_node<Button>("Camp")->emit_signal("pressed");
        get_node<Window>("RestDialog")->get_node<Button>("Start")->emit_signal("pressed");
        recovery_stage_ = 2;
        return;
    }
    if (recovery_stage_ == 2)
    {
        if (campaign_->state().time_minutes != recovery_before_->time_minutes + 5 ||
                member.vitals != recovery_before_->roster.at(0).vitals)
            throw std::runtime_error(
                "Original city-watch interruption must consume five minutes without recovery");
        if (save_check)
            save_check("interrupted-rest");
        recovery_stage_ = 3;
    }
    if (recovery_stage_ == 4)
    {
        if (member.wealth[3] != recovery_before_->roster.at(0).wealth[3] - 100 ||
                member.vitals.hit_points <= 1 || session_->script_variable(ecl_temple_service) != 0)
            throw std::runtime_error("Original temple must charge 100 gp, heal and resume ECL");
        if (save_check)
            save_check("temple-payment");
        recovery_before_ = campaign_->checkpoint();
        recovery_stage_ = 5;
    }
    // Include the travel time to the inn, as the game integration check does.
    if (recovery_stage_ == 5 && member.last_rest_minutes &&
            member.last_rest_minutes != recovery_before_->roster.at(0).last_rest_minutes)
    {
        if (*member.last_rest_minutes < recovery_before_->time_minutes + 480 ||
                member.vitals.hit_points != member.character.sheet().hit_points ||
                member.wealth[4] != 0)
            throw std::runtime_error("Original inn payment and full recovery must persist");
        if (save_check)
            save_check("inn-rest");
        if (save_check)
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
            save_check("rejected-service");
        }
        recovery_before_ = campaign_->checkpoint();
        recovery_stage_ = 6;
    }
    if (recovery_stage_ == 6)
    {
        get_node<Button>("Camp")->emit_signal("pressed");
        recovery_stage_ = 7;
        return;
    }
    if (recovery_stage_ == 7)
    {
        if (campaign_->state().time_minutes != recovery_before_->time_minutes ||
                !get_node<Window>("RestDialog")->get_node<Button>("Start")->is_disabled())
            throw std::runtime_error("Immediate repeated long rest must be denied");
        get_node<Window>("RestDialog")->get_node<Button>("Finish")->emit_signal("pressed");
        if (save_check)
            save_check("denied-rest");
        if (save_check)
        {
            (void)campaign_->rest(opengold::RestKind::short_rest);
            save_check("short-rest-spending");
            campaign_->finish_short_rest(campaign_->state().short_rest->ticket);
        }
        capture_frame("party-rest");
        recovery_stage_ = 8;
        checking_ = false;
        UtilityFunctions::print(
            "Godot recovery check passed: original interruption, temple payment, inn rest and repeated-rest denial.");
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
    return ProjectSettings::get_singleton()->globalize_path(
               "res://../../data/rules/srd-5.2.1/combat.rules");
}

godot::String rest_text(std::string_view value)
{
    return godot::String::utf8(value.data(), value.size());
}
} // namespace

#include "../../../src/OpenGoldBox/rest_dialog_impl.h"
