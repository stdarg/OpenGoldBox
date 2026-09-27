#include "godot_nodes.h"
#include "application_settings.h"
#include "localization.h"
#include "game_resources.h"
#include "character_creation_view.h"
#include "rolf_tour_view.h"
#include "combat_view.h"
#include "save_slots.h"
#include "opengold/campaign_save.h"
#include "opengold/srd5.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/item_list.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/viewport_texture.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
using namespace godot;
using namespace opengold;

namespace
{
std::filesystem::path game_directory()
{
    const auto dir = settings::game_path();
    return std::filesystem::u8path(dir.utf8().get_data());
}

auto rules_module()
{
    return srd5::load(std::filesystem::u8path(game_rules_file().utf8().get_data()));
}
} // namespace

void CharacterCreationView::setup_saves()
{
    save_read_check_ = OS::get_singleton()->get_cmdline_user_args().has("--save-check-read");
    auto dialog = presentation::make_node<SaveSlots>();
    dialog->set_name("SaveSlots");
    dialog->save = [this](const auto &p)
    {
        save_campaign(p);
    };
    dialog->load = [this](const auto &p)
    {
        load_campaign(p);
    };
    presentation::attach_child(*this, std::move(dialog));
    for (bool saving : {true, false})
    {
        auto button = presentation::make_node<Button>();
        button->set_name(saving ? "Save" : "Load");
        button->set_text(i18n::text(saving ? N_("Save game") : N_("Load game")));
        button->connect("pressed",
                        callable_mp(this, &CharacterCreationView::open_saves).bind(saving));
        presentation::attach_child(*get_node<Control>("PartyPanel"), std::move(button));
    }
}

void CharacterCreationView::open_saves(bool saving)
{
    if (campaign_->in_combat())
        return;
    if (campaign_defeated_ && saving)
        return;
    if (auto *town = Object::cast_to<RolfTourView>(get_node_or_null("CampaignTown"));
        town && !town->can_leave() && !campaign_defeated_)
        return;
    get_node<SaveSlots>("SaveSlots")->open(saving);
}

void CharacterCreationView::save_campaign(const std::filesystem::path &path)
{
    if (campaign_defeated_)
        throw std::runtime_error("Load a saved game after defeat");
    const auto *town = Object::cast_to<RolfTourView>(get_node_or_null("CampaignTown"));
    const auto bytes = encode_campaign(*campaign_, town ? town->saved_session() : nullptr,
                                       campaign_asset_identity(game_directory()));
    write_campaign_file(path, bytes);
    error_ = i18n::text("Campaign saved.");
    refresh_party();
}

void CharacterCreationView::load_campaign(const std::filesystem::path &path)
{
    if (campaign_->in_combat())
        throw std::runtime_error("Finish combat before loading");
    auto *town = Object::cast_to<RolfTourView>(get_node_or_null("CampaignTown"));
    if (town && !town->can_leave() && !campaign_defeated_)
        throw std::runtime_error("Finish the current event before loading");
    const auto directory = game_directory();
    auto prototype = por::RolfTourSession::load(directory);
    auto module = rules_module();
    auto saved = decode_campaign(read_campaign_file(path), *srd5::character_rules(), *module,
                                 campaign_asset_identity(directory), &prototype);
    auto replacement = std::make_shared<CampaignParty>(std::move(module));
    replacement->restore(std::move(saved.party));
    for (const auto &m : replacement->state().roster)
    {
        art_->validate(m.character.appearance());
        (void)replacement->profile(m.id);
    }
    presentation::NodeOwner<Node> owned;
    if (saved.town && !town)
    {
        owned = presentation::instantiate_scene("res://scenes/rolf_tour.tscn");
        town = Object::cast_to<RolfTourView>(owned.get());
        if (!town)
            throw std::runtime_error("Invalid town scene");
        town->set_name("CampaignTown");
        town->hide();
        town->campaign_party(replacement);
        town->connect("party_member_selected",
                      callable_mp(this, &CharacterCreationView::town_member_selected));
        town->connect("level_up_requested",
                      callable_mp(this, &CharacterCreationView::open_advancement));
        town->connect("save_requested", callable_mp(this, &CharacterCreationView::open_saves));
        presentation::attach_child(*this, std::move(owned));
    }
    // All decoding, resource loading and character validation completed above.
    campaign_ = std::move(replacement);
    campaign_defeated_ = false;
    get_node<Window>("Defeat")->hide();
    if (auto *fight = Object::cast_to<CombatView>(get_node_or_null("CampaignCombat")))
    {
        remove_child(fight);
        presentation::NodeOwner<Node> removed(fight);
    }
    if (saved.town)
    {
        town->restore_campaign(campaign_, std::move(*saved.town));
        town->hide();
        town->set_process(false);
        town->set_process_input(false);
    }
    else if (town)
    {
        remove_child(town);
        presentation::NodeOwner<Node> removed(town);
    }
    pool_added_.clear();
    for (unsigned i = 0; i < 48; ++i)
        for (const auto &m : campaign_->state().roster)
            if (m.creation_source == "pool:v1:" + std::to_string(i))
                pool_added_.push_back(i);
    completed_.reset();
    added_to_party_ = false;
    roster_index_ = 0;
    party_open_ = true;
    get_node<Button>("ReturnParty")->hide();
    get_node<Control>("PartyPanel")->show();
    error_ = i18n::text("Campaign loaded.");
    refresh_party();
    party_layout();
}

void RolfTourView::restore_campaign(std::shared_ptr<CampaignParty> party,
                                    por::RolfTourSession session)
{
    rest_result_ = String();
    rest_member_ = 0;
    rest_save_open_ = true;
    get_node<Window>("RestDialog")->hide();
    session.attach_restored_party(party);
    campaign_ = std::move(party);
    session_ = std::move(session);
    shown_revision_ = 0;
    rendered_pose_.reset();
    rendered_sprite_id_ = 999;
    rendered_picture_revision_ = 0;
    played_footsteps_ = session_->snapshot().footsteps;
    refresh();
    rest_save_open_ = false;
}

void RolfTourView::request_save(bool saving)
{
    if (embedded_party_ && session_ && session_->can_leave())
        emit_signal("save_requested", saving);
}

void CharacterCreationView::save_checkpoint_check(const std::string &name)
{
    auto directory = std::filesystem::u8path(ProjectSettings::get_singleton()
                                                 ->globalize_path("user://checks/save-check")
                                                 .utf8()
                                                 .get_data());
    save_campaign(directory / (name + ".ogs"));
    error_ = "";
    if (name == "final")
    {
        open_saves(true);
        auto *dialog = get_node<SaveSlots>("SaveSlots");
        dialog->get_node<LineEdit>("Name")->set_text(i18n::text(N_("Restart test")));
        dialog->get_node<Button>("Action")->emit_signal("pressed");
        if (dialog->is_visible())
            dialog->get_node<Button>("Action")->emit_signal("pressed");
        if (dialog->is_visible())
            throw std::runtime_error("Save slot UI did not complete its write");
        error_ = "";
    }
    UtilityFunctions::print("Saved restart case: ", String::utf8(name.c_str()));
}

void CharacterCreationView::load_checkpoint_check()
{
    auto directory = std::filesystem::u8path(ProjectSettings::get_singleton()
                                                 ->globalize_path("user://checks/save-check")
                                                 .utf8()
                                                 .get_data());
    const auto assets = campaign_asset_identity(game_directory());
    for (const char *name :
         {"advancement", "interrupted-rest", "cancelled-service", "temple-payment", "inn-rest",
          "rejected-service", "denied-rest", "short-rest-spending", "final"})
    {
        auto path = directory / (std::string(name) + ".ogs");
        load_campaign(path);
        auto *town = get_node<RolfTourView>("CampaignTown");
        const auto before = encode_campaign(*campaign_, town->saved_session(), assets);
        if (before != read_campaign_file(path))
            throw std::runtime_error(std::string("State changed across process restart: ") + name);
        if (std::string_view(name) != "short-rest-spending")
            campaign_->award_experience(300, "preview:bandit:v1");
        if (encode_campaign(*campaign_, town->saved_session(), assets) != before)
            throw std::runtime_error("Reward duplicated after restart");
        if (std::string_view(name) == "inn-rest" || std::string_view(name) == "denied-rest")
            if (campaign_->rest())
                throw std::runtime_error("Rest timer reset after restart");
        auto corrupt = before;
        corrupt.back() ^= 1;
        auto broken = directory / "corrupt.ogs";
        write_campaign_file(broken, corrupt);
        bool rejected = false;
        try
        {
            load_campaign(broken);
        }
        catch (const std::exception &)
        {
            rejected = true;
        }
        if (!rejected || encode_campaign(*campaign_, town->saved_session(), assets) != before)
            throw std::runtime_error("Rejected load changed live campaign");
        if (std::string_view(name) == "short-rest-spending")
        {
            town->show();
            town->resume_party();
            auto *rest = town->get_node<Window>("RestDialog");
            if (!rest->is_visible() || !campaign_->state().short_rest)
                throw std::runtime_error("Reload did not reopen pending rest controls");
            rest->get_node<Button>("Save")->emit_signal("pressed");
            if (!get_node<SaveSlots>("SaveSlots")->is_visible() || rest->is_visible())
                throw std::runtime_error("Rest save did not use existing save dialog");
            get_node<SaveSlots>("SaveSlots")->get_node<Button>("Cancel")->emit_signal("pressed");
            town->get_node<Button>("Camp")->emit_signal("pressed");
            rest->get_node<Button>("Spend")->emit_signal("pressed");
            if (encode_campaign(*campaign_, town->saved_session(), assets) == before)
                throw std::runtime_error("Reloaded rest could not spend its next die");
            rest->get_node<Button>("Finish")->emit_signal("pressed");
            town->hide();
        }
        UtilityFunctions::print("Restored restart case: ", name);
    }
    open_saves(false);
    auto *dialog = get_node<SaveSlots>("SaveSlots");
    auto *slots = dialog->get_node<ItemList>("Slots");
    int index = -1;
    for (int i = 0; i < slots->get_item_count(); ++i)
        if (slots->get_item_text(i) == "Restart test")
            index = i;
    if (index < 0)
        throw std::runtime_error("Named UI save missing after restart");
    slots->select(index);
    slots->emit_signal("item_selected", index);
    auto original = campaign_;
    dialog->get_node<Button>("Action")->emit_signal("pressed");
    if (campaign_ != original || !dialog->is_visible())
        throw std::runtime_error("Load failed to wait for confirmation");
    dialog->get_node<Button>("Action")->emit_signal("pressed");
    if (dialog->is_visible() || campaign_ == original)
        throw std::runtime_error("Confirmed UI load failed");
    UtilityFunctions::print(
        "Godot campaign restart check passed: nine complete states, reward claims, rest timers, rejected-load rollback and named-slot controls.");
    if (capture_)
    {
        open_saves(false);
        slots->select(index);
        slots->emit_signal("item_selected", index);
        dialog->get_node<Button>("Action")->emit_signal("pressed");
        save_capture_frames_ = 1;
    }
    else
        get_tree()->quit(0);
}

void CharacterCreationView::capture_save_ui()
{
    if (++save_capture_frames_ == 4)
    {
        auto *dialog = get_node<SaveSlots>("SaveSlots");
        const auto image = dialog->get_texture()->get_image();
        if (image.is_null() || image->save_png(ProjectSettings::get_singleton()->globalize_path(
                                   "user://checks/campaign-load-dialog.png")) != OK)
            throw std::runtime_error("Cannot capture save dialog");
        dialog->hide();
    }
    if (save_capture_frames_ == 8)
    {
        capture("campaign-party.png");
        party_action(7);
    }
    if (save_capture_frames_ == 12)
    {
        capture("campaign-town.png");
        save_capture_frames_ = 0;
        get_tree()->quit(0);
    }
}
