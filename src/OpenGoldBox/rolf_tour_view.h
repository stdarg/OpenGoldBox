#ifndef OPENGOLD_ROLF_TOUR_VIEW_H
#define OPENGOLD_ROLF_TOUR_VIEW_H

#include "opengold/rolf_tour.h"
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <map>
#include <optional>
#include <set>
#include <functional>

class RolfTourView : public godot::Control
{
    GDCLASS(RolfTourView, godot::Control)
  public:
    void _ready() override;
    void _process(double delta) override;
    void _draw() override;
    void _input(const godot::Ref<godot::InputEvent> &event) override;

    void campaign_party(std::shared_ptr<opengold::CampaignParty> party)
    {
        campaign_ = std::move(party);
        embedded_party_ = true;
    }

    [[nodiscard]] bool can_leave() const
    {
        return !session_ || session_->can_leave();
    }

    void resume_party()
    {
        shown_revision_ = 0;
        refresh();
    }

    [[nodiscard]] const opengold::por::RolfTourSession *saved_session() const
    {
        return session_ ? &*session_ : nullptr;
    }

    [[nodiscard]] std::optional<opengold::CampaignEncounter> pending_encounter() const
    {
        return session_ ? session_->pending_encounter() : std::nullopt;
    }

    bool resolve_combat(const opengold::rules::Snapshot &result)
    {
        if (!session_ || !session_->resolve_combat(result))
            return false;
        refresh();
        return true;
    }

    bool reject_combat(std::string diagnostic)
    {
        if (!session_ || !session_->reject_combat(std::move(diagnostic)))
            return false;
        refresh();
        return true;
    }

    void restore_campaign(std::shared_ptr<opengold::CampaignParty> party,
                          opengold::por::RolfTourSession session);
    void request_save(bool saving);
    std::function<void(const std::string &)> save_check;

    [[nodiscard]] bool party_route_checked() const
    {
        return shop_check_stage_ == 4;
    }

    void start_recovery_check();

    [[nodiscard]] bool recovery_checked() const
    {
        return recovery_stage_ == 8;
    }

    bool check_expedition_step();

    // Redraws from the session and party, e.g. after the leader changes.
    void refresh();

  protected:
    static void _bind_methods();
    void _notification(int what);

  private:
    void advance_frame(double delta);
    void respond_to_input(const godot::Ref<godot::InputEvent> &event);
    std::optional<opengold::por::RolfTourSession> session_;
    std::shared_ptr<opengold::CampaignParty> campaign_;
    std::array<godot::Ref<godot::ImageTexture>, 3> sprites_;
    godot::Ref<godot::ImageTexture> wall_view_;
    // Complete NPC portraits by file, loaded when first shown.
    std::map<std::string, godot::Ref<godot::Texture2D>, std::less<>> npc_portraits_;
    // Monster close-up looping before combat; the pointer identifies the frames shown.
    const std::vector<opengold::AnimationFrame> *shown_monster_picture_{};
    std::vector<godot::Ref<godot::ImageTexture>> monster_frames_;
    double monster_picture_seconds_{};
    unsigned monster_picture_check_frames_{};
    std::optional<opengold::por::PartyPose> rendered_pose_;
    godot::Ref<godot::AudioStreamWAV> footstep_;
    godot::Rect2 scene_rect_, map_rect_, dialogue_rect_;
    godot::String error_;
    std::uint64_t shown_revision_{}, checked_ticket_{};
    unsigned played_footsteps_{}, check_frames_{}, check_prompts_{};
    unsigned rendered_sprite_id_{999};
    std::uint64_t displayed_ticket_{};
    std::uint64_t rendered_picture_revision_{};
    bool no_fog_{}, ready_{}, checking_{}, capture_{}, capture_pending_{};
    bool town_check_{}, embedded_party_{};
    unsigned shop_check_stage_{};
    unsigned recovery_stage_{};
    bool save_cancel_checked_{}, save_cancel_pending_{};
    bool slums_labels_checked_{};
    std::uint64_t recovery_capture_ticket_{};
    std::optional<opengold::PartyState> recovery_before_;
    std::set<std::pair<unsigned, unsigned>> check_refused_edges_;
    std::optional<std::pair<unsigned, unsigned>> check_pending_edge_;
    void layout();
    void check_district_labels(const opengold::por::TourSnapshot &s);
    void select_buyer(unsigned slot);
    [[nodiscard]] std::vector<unsigned> occupied_slots() const;
    void buyer_key(godot::Key keycode);
    void sync_monster_picture();
    [[nodiscard]] std::size_t current_monster_frame() const;
    void dismiss_monster_picture();
    void restart();
    void next();
    void left();
    void right();
    void forward();
    void look();
    void camp();
    void setup_rest();
    void check_rest_controls();
    void check_mastery_rest_controls();
    unsigned rest_check_stage_{};
    void refresh_rest();
    void rest_selected(std::int64_t index);
    void rest_start();
    void rest_heal();
    void rest_recover();
    void rest_use();
    bool refresh_rest_use(const std::vector<opengold::MemberRestInfo> &infos, bool spending);
    void refresh_rest_spells();
    void rest_spell_toggled(bool selected, godot::String group, godot::String option);
    void rest_spell_replaced(std::int64_t index);
    void rest_spell_apply();
    void rest_spell_keep();
    void rest_spell_input(const godot::Ref<godot::InputEvent> &event);
    opengold::MemberId rest_spell_member_{};
    opengold::rules::SpellChoices rest_spell_choice_;
    void refresh_rest_training();
    void rest_training_toggled(bool selected, godot::String option);
    void rest_training_apply();
    void rest_training_keep();
    void rest_training_input(const godot::Ref<godot::InputEvent> &event);
    opengold::MemberId rest_training_member_{};
    opengold::RestTicket rest_training_ticket_{};
    std::vector<std::string> rest_training_choice_;
    void rest_finish();
    void rest_save();
    void rest_input(const godot::Ref<godot::InputEvent> &event);
    opengold::MemberId rest_member_{};
    godot::String rest_result_;
    bool rest_save_open_{};
    void inventory();
    void refresh_inventory();
    void inventory_selected(std::int64_t index);
    void equip_item(bool equip);
    void party_selected(std::int64_t index);
    void level_up_requested(int slot);
    void close_sheet();
    void leave_shop();
    void movement(opengold::por::ExplorationCommand command);
    godot::Ref<godot::Texture2D> npc_portrait(std::string_view file);
    void draw_scene();
    void draw_map();
    void check_run();
    void check_town();
    void check_recovery();
    void check_walk_to(unsigned x, unsigned y);
    void capture_frame(const godot::String &name);
};
#endif
