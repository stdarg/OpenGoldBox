#ifndef OPENGOLD_COMBAT_VIEW_H
#define OPENGOLD_COMBAT_VIEW_H
#include "cached_combat.h"
#include "opengold/combat_demo.h"
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <exception>
#include <map>
#include <set>
#include "opengold/sound_player.h"

class CombatView : public godot::Control
{
    GDCLASS(CombatView, godot::Control)
  public:
    void _ready() override;
    void _process(double delta) override;
    void _draw() override;
    void _input(const godot::Ref<godot::InputEvent> &event) override;
    // Shows a failed handler's error; called by presentation::run_guarded.
    void report_failure(const std::exception &failure);

    [[nodiscard]] std::int64_t selected_character_id() const
    {
        return static_cast<std::int64_t>(selected_);
    }

    [[nodiscard]] godot::Vector2i selected_character_cell() const;
    // Pixels per battlefield square at the current zoom, for play-test clicks.
    [[nodiscard]] double cell_pixels() const;
    // Squares of the enemies still standing, for play-test scripts.
    [[nodiscard]] godot::Array enemy_cells() const;
    [[nodiscard]] bool sprite_facing_left(std::int64_t id) const;
    [[nodiscard]] godot::Ref<godot::Texture2D> sprite_texture(opengold::rules::EntityId id,
            opengold::por::IconPose pose) const;

    [[nodiscard]] bool attack_pose_active(std::int64_t id) const
    {
        return action_seconds_.contains(static_cast<opengold::rules::EntityId>(id));
    }

    // Prepare while detached so the caller can keep its current screen on failure.
    void prepare_combat();

    void campaign_party(std::shared_ptr<opengold::CampaignParty> party,
                        std::vector<opengold::CombatArt> art)
    {
        campaign_ = std::move(party);
        campaign_art_ = std::move(art);
    }

    [[nodiscard]] bool defeated() const
    {
        return campaign_ && demo_ && demo_->has_combat() &&
               demo_.snapshot().outcome == opengold::rules::Outcome::defeat;
    }

    [[nodiscard]] bool can_leave() const
    {
        return !defeated() &&
               (!demo_ || !demo_->has_combat() ||
                demo_.snapshot().outcome != opengold::rules::Outcome::ongoing);
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
        const auto &s = demo_.snapshot();
        return s.outcome == opengold::rules::Outcome::ongoing ? std::nullopt : std::optional{s};
    }

  protected:
    static void _bind_methods();
    void _notification(int what);

  private:
    void respond_to_input(const godot::Ref<godot::InputEvent> &event);
    void draw_view();
    presentation::CachedCombat demo_;
    std::shared_ptr<opengold::CampaignParty> campaign_;
    std::vector<opengold::CombatArt> campaign_art_;
    std::map<opengold::rules::EntityId, std::string> missing_art_;
    std::optional<opengold::CampaignEncounter> encounter_;
    std::vector<godot::Ref<godot::ImageTexture>> terrain_art_;

    struct SpriteArt
    {
        godot::Ref<godot::ImageTexture> texture;
        godot::Ref<godot::ImageTexture> action;
        godot::Ref<godot::ImageTexture> left_texture;
        godot::Ref<godot::ImageTexture> left_action;
        godot::Ref<godot::ImageTexture> unconscious;
        godot::Rect2 visible;
        godot::Rect2 left_visible;
        godot::Rect2 unconscious_visible;
        bool goliath{};
    };

    // Everything sync_art() installs, built off to the side first.
    struct ArtSet
    {
        std::map<opengold::rules::EntityId, std::string> missing;
        std::map<opengold::rules::EntityId, SpriteArt> sprites;
        std::map<std::string, SpriteArt> forms;
        std::map<opengold::rules::EntityId, godot::Ref<godot::Texture2D>> portraits;
        std::vector<godot::Ref<godot::ImageTexture>> terrain;
        godot::Ref<godot::ImageTexture> skull;
        std::vector<opengold::CombatArt> campaign;
    };
    [[nodiscard]] ArtSet build_art() const;

    std::map<opengold::rules::EntityId, SpriteArt> art_;
    // Wild Shape's Beast forms, by form key, from the original combat icons.
    std::map<std::string, SpriteArt> form_art_;
    [[nodiscard]] const SpriteArt *combatant_art(const opengold::rules::CombatantView &a) const;
    godot::Ref<godot::ImageTexture> skull_art_;
    std::map<opengold::rules::EntityId, bool> known_dead_;
    std::map<opengold::rules::EntityId, double> skull_seconds_;
    std::map<opengold::rules::EntityId, double> action_seconds_;
    std::unique_ptr<opengold::por::SoundPlayer> attack_sound_;
    std::unique_ptr<opengold::por::SoundPlayer> effect_sound_;
    std::unique_ptr<opengold::por::SoundPlayer> death_sound_;
    std::map<opengold::rules::EntityId, godot::Ref<godot::Texture2D>> portraits_;
    opengold::rules::EntityId selected_{};
    opengold::rules::EntityId last_actor_{};
    godot::Rect2 board_rect_;
    // Where the log header and the log go, below the controls.
    godot::Rect2 log_area_;
    // The rows of controls the battlefield was last sized to leave room for.
    double laid_out_controls_height_{};
    double base_tile_{};
    double combat_zoom_{1.0};
    std::string mode_{"move"}, error_;
    opengold::rules::EntityId aid_target_{};
    double ai_delay_{};
    // The Flee button hands every party member to the flee policy until the
    // fight ends.
    bool flee_mode_{};
    void flee();
    // Quick combat, as in the original: the computer plays party members on
    // Quick until the player takes control. A campaign party keeps who is on
    // Quick and Quick magic between fights; the standalone demo keeps them here.
    std::set<opengold::rules::EntityId> quick_;
    bool quick_magic_{};
    [[nodiscard]] bool is_quick(opengold::rules::EntityId id) const;
    [[nodiscard]] bool any_quick() const;
    [[nodiscard]] bool quick_magic() const;
    void quick();
    void take_control();
    void toggle_quick_magic();
    [[nodiscard]] bool quick_turn(const opengold::rules::Snapshot &state) const;
    bool quick_key(godot::Key key);
    bool ready_{}, checking_{}, capture_{}, captured_{}, check_slums_{}, checked_input_{};
    bool party_check_{}, defeat_check_{}, expedition_check_{};
    unsigned check_steps_{}, completion_frames_{};
    unsigned zoom_center_frames_{};
    void draw_battlefield();
    void update_hover(const godot::Vector2 &pointer);
    void center_on(opengold::rules::Cell cell);
    std::optional<std::pair<opengold::rules::EntityId, opengold::rules::Cell>> followed_;
    bool panning_{}, check_target_centered_{};
    void layout();
    void layout_status();
    void layout_log();
    [[nodiscard]] double controls_height(bool show_controls) const;
    void layout_reaction_controls(bool reaction);
    void refresh();
    void sync_art(bool preserve_effects = false);
    void act(const opengold::rules::Command &command);
    void select_mode(godot::String verb);
    void immediate(godot::String verb);
    // Area spells (CLASS-5): whether `verb` starts aiming one, and moving the preview.
    [[nodiscard]] bool aims_area(std::string_view verb) const;
    void aim_area_at(opengold::rules::Cell cell);
    void select_party(opengold::rules::EntityId id);
    void select_acting_character();
    void move_selected(opengold::rules::Cell direction);
    void use_cunning_action();
    void cunning_selected(std::int64_t);
    void cantrip_selected(std::int64_t index);
    void cast_cantrip();
    std::string cantrip_;
    void spell_slot();
    unsigned thrown_item_{}, light_item_{};
    void weapon_selected(std::int64_t index);
    bool matches_item(const opengold::rules::Command &command) const;
    void thrown_selected(std::int64_t index);
    void begin_throw();
    // The Items row: gear actions (Torch attack, thrown flasks, Shoot, Take off
    // shield) offered this turn, by verb, and the chosen one.
    std::vector<std::string> item_verbs_;
    std::string item_verb_;
    void item_selected(std::int64_t index);
    void use_item();
    unsigned effect_target_index_{};
    void initiative_input(const godot::Ref<godot::InputEvent> &event);
    void optional_effect_input(const godot::Ref<godot::InputEvent> &event);
    void begin_nick();
    void nick_selected(std::int64_t index);
    void confirm_nick();
    void cancel_nick();
    void nick_input(const godot::Ref<godot::InputEvent> &event);
    void adjust_zoom(int percentage_points);
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
