#ifndef OPENGOLD_COMBAT_VIEW_H
#define OPENGOLD_COMBAT_VIEW_H
#include "opengold/combat_demo.h"
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <map>
class CombatView : public godot::Control
{
    GDCLASS(CombatView, godot::Control)
  public:
    void _ready() override;
    void _process(double delta) override;
    void _draw() override;
    void _input(const godot::Ref<godot::InputEvent> &event) override;
    void campaign_party(std::shared_ptr<opengold::CampaignParty> party,
                        std::vector<opengold::CombatArt> art)
    {
        campaign_ = std::move(party);
        campaign_art_ = std::move(art);
    }
    [[nodiscard]] bool defeated() const
    {
        return campaign_ && demo_ && demo_->has_combat() &&
               demo_->combat().snapshot().outcome == opengold::rules::Outcome::defeat;
    }
    [[nodiscard]] bool can_leave() const
    {
        return !defeated() &&
               (!demo_ || !demo_->has_combat() ||
                demo_->combat().snapshot().outcome != opengold::rules::Outcome::ongoing);
    }
    void campaign_encounter(opengold::CampaignEncounter encounter)
    {
        encounter_ = std::move(encounter);
    }
    [[nodiscard]] bool expedition() const
    {
        return encounter_.has_value();
    }
    [[nodiscard]] std::optional<opengold::rules::Snapshot> completed_outcome() const
    {
        if (!demo_ || !demo_->has_combat())
            return {};
        auto s = demo_->combat().snapshot();
        return s.outcome == opengold::rules::Outcome::ongoing ? std::nullopt : std::optional{s};
    }

  protected:
    static void _bind_methods();
    void _notification(int what);

  private:
    std::unique_ptr<opengold::CombatDemo> demo_;
    std::shared_ptr<opengold::CampaignParty> campaign_;
    std::vector<opengold::CombatArt> campaign_art_;
    std::optional<opengold::CampaignEncounter> encounter_;
    std::vector<godot::Ref<godot::ImageTexture>> terrain_art_;
    std::map<opengold::rules::EntityId, godot::Ref<godot::ImageTexture>> art_;
    godot::Rect2 board_rect_;
    std::string mode_{"move"}, error_;
    opengold::rules::EntityId aid_target_{};
    double ai_delay_{};
    bool ready_{}, checking_{}, capture_{}, captured_{}, check_slums_{}, checked_input_{};
    bool party_check_{}, defeat_check_{}, expedition_check_{};
    unsigned check_steps_{}, completion_frames_{};
    void layout();
    void refresh();
    void sync_art();
    void act(const opengold::rules::Command &command);
    void select_mode(godot::String verb);
    void immediate(godot::String verb);
    void spell_slot();
    void bonus_selected(std::int64_t index);
    void use_bonus_action();
    void ground_selected(std::int64_t index);
    void pick_up();
    unsigned ground_item_{}, thrown_item_{};
    void thrown_selected(std::int64_t index);
    void begin_throw();
    std::vector<std::pair<unsigned, opengold::rules::EntityId>> item_holders_;
    unsigned spell_slot_{1};
    void training();
    void slums();
    void replay();
    void next();
    void revisit();
    void save_game();
    void load_game();
    std::filesystem::path local_path(const char *path) const;
};
#endif
