#ifndef OPENGOLD_ROLF_TOUR_VIEW_H
#define OPENGOLD_ROLF_TOUR_VIEW_H

#include "opengold/campaign_save.h"
#include "opengold/exploration_session.h"
#include "opengold/rolf_tour.h"
#include "portrait_catalog.h"
#include "opengold/sound_player.h"
#include "opengold/title_screens.h"
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <functional>
#include <exception>

class RolfTourView : public godot::Control
{
    GDCLASS(RolfTourView, godot::Control)
  public:
    // A failed handler is already in the log, and this view has no status
    // line to show it; called by presentation::run_guarded.
    void report_failure(const std::exception &) noexcept
    {
    }

    void _ready() override;
    void _process(double delta) override;
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

    // The DAX campaign a save keeps; null for a design.
    [[nodiscard]] const opengold::por::RolfTourSession *saved_session() const
    {
        return dax();
    }

    // A design's play as a save keeps it (docs/games/ua.md, Saving and loading
    // a game); nullopt for a DAX campaign.
    [[nodiscard]] std::optional<opengold::DesignSave> saved_design() const;

    [[nodiscard]] const std::optional<opengold::CampaignEncounter> &pending_encounter() const
    {
        static const std::optional<opengold::CampaignEncounter> none;
        return session_ ? session_->pending_encounter() : none;
    }

    bool resolve_combat(const opengold::rules::Snapshot &result)
    {
        if (!session_ || !session_->resolve_combat(result))
        {
            return false;
        }
        refresh();
        return true;
    }

    bool reject_combat(std::string diagnostic)
    {
        if (!session_ || !session_->reject_combat(std::move(diagnostic)))
        {
            return false;
        }
        refresh();
        return true;
    }

    void restore_campaign(std::shared_ptr<opengold::CampaignParty> party,
                          opengold::por::RolfTourSession session);
    // The design a save left (ua::UaSession::resume), played on by the
    // loaded party.
    void restore_design(std::shared_ptr<opengold::CampaignParty> party,
                        std::unique_ptr<opengold::ExplorationSession> design);
    void request_save(bool saving);
    // A short note where "The way is blocked" shows, for three seconds.
    void show_note(const godot::String &note);
    // Reads the difficulty, encounter challenge and auto-equip settings
    // again, as the Options screen changed them.
    void apply_settings();
    // The Sound effects volume, to the thunder and the footsteps, playing or not.
    void apply_sound_volume();

    // The party check's save-and-reload hook; only that check sets one.
    void enable_save_check(std::function<void(const std::string &)> check)
    {
        save_check_ = std::move(check);
    }

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
    // Reads the party's portraits again, from another new art folder.
    void forget_portraits();
    // The 3D view art setting changed: the walls follow it at once (#274).
    void show_original_walls(bool original);

    [[nodiscard]] godot::Dictionary tour_visuals();

  protected:
    static void _bind_methods();
    void _notification(int what);

  private:
    void respond_to_notification(int what);
    void prepare_view();
    // Private, so only enable_save_check sets it (Effective C++ Item 22).
    std::function<void(const std::string &)> save_check_;
    void advance_frame(double delta);
    void respond_to_input(const godot::Ref<godot::InputEvent> &event);
    // The adventure played: a DAX game's campaign or an Unlimited Adventures
    // design (docs/games/ua.md, The seam).
    std::unique_ptr<opengold::ExplorationSession> session_;

    // The DAX campaign played; null without one or while a design plays,
    // whose screen leaves out what only a DAX campaign has.
    [[nodiscard]] opengold::por::RolfTourSession *dax() const noexcept
    {
        return session_ ? session_->dax() : nullptr;
    }

    // A DAX campaign's session, where the view needs one (its checks and
    // what only it shows); throws without one.
    [[nodiscard]] opengold::por::RolfTourSession &dax_session() const;
    // Auto-equip as the Options screen sets it (EQUIP-1): nothing for off.
    [[nodiscard]] static std::optional<opengold::UnidentifiedMagic> auto_equip_setting();
    // A design plays the difficulty as the Options screen sets it.
    void apply_design_difficulty();
    std::shared_ptr<opengold::CampaignParty> campaign_;
    std::array<godot::Ref<godot::ImageTexture>, 3> sprites_;
    godot::Ref<godot::ImageTexture> wall_view_;
    // Complete NPC portraits by file, loaded when first shown.
    std::map<std::string, godot::Ref<godot::Texture2D>, std::less<>> npc_portraits_;
    // The party list's portraits, as the party roster shows them; loaded when
    // the list first shows a member.
    std::optional<PortraitCatalog> portraits_;
    // The member the party list last scrolled to, so it scrolls only when the
    // selection changes.
    std::optional<unsigned> shown_selected_slot_;
    // Monster close-up looping before combat; the pointer identifies the frames shown.
    // Only compared with the session's current close-up, never read: the
    // frames it points to go when the session is reset or replaced.
    const std::vector<opengold::AnimationFrame> *shown_monster_picture_{};
    // The close-up is playing, and the fight opens when it has played.
    bool fight_waits_on_picture_{};
    std::vector<godot::Ref<godot::ImageTexture>> monster_frames_;
    std::vector<std::uint32_t> monster_delays_;
    double monster_picture_seconds_{};
    unsigned monster_picture_check_frames_{};
    std::optional<opengold::por::PartyPose> rendered_pose_;
    bool original_walls_{};
    godot::Ref<godot::Image> wall_image_; // what wall_view_ shows
    // The wilderness square and direction last drawn: x, y, direction.
    std::array<unsigned, 3> rendered_overland_{};
    godot::Ref<godot::AudioStreamWAV> footstep_;
    // The footstep's loudness with Sound effects at the top: the scene's.
    float footstep_full_volume_{};
    godot::String error_;
    std::uint64_t shown_revision_{}, checked_ticket_{};
    unsigned played_footsteps_{}, played_thunder_{}, played_script_sounds_{}, check_frames_{},
        check_prompts_{};
    // Whether the last snapshot shown had just won the game (its film, if
    // any, plays once a win).
    bool won_game_shown_{};
    // The original's own closing film while it plays (Pools of Darkness'
    // FINAL): its screens, the one shown, that screen's time left, and its song.
    std::optional<opengold::TitleScreens> closing_film_;
    std::size_t closing_screen_{};
    double closing_seconds_left_{};
    std::unique_ptr<opengold::por::SoundPlayer> closing_song_;
    // The original's sound effects from the player's game, when they have them.
    std::unique_ptr<opengold::por::SoundPlayer> original_sound_;
    // Pools of Darkness's and Treasures' own sound effects (ZOUNDS.DAX, sound
    // n its record n - 1): the footstep, sound 10, which their steps and their
    // scripts' PLAY SOUND 10 play, and their scripts' other effects
    // (TourSnapshot::script_sound); null in the other games, whose footstep is
    // footstep_.
    std::unique_ptr<opengold::por::SoundPlayer> game_sounds_;
    // The later games' music their scripts start (TourSnapshot::music), the
    // piece it holds, and the starts already played.
    std::unique_ptr<opengold::por::SoundPlayer> music_;
    unsigned music_piece_{}, played_music_starts_{};
    // Starts the scripts' music, stops it once a sound effect has.
    void follow_music(const opengold::por::TourSnapshot &snapshot);
    void play_music(unsigned piece);
    unsigned rendered_sprite_id_{999};
    std::uint64_t displayed_ticket_{};
    // The shop's filter, ShopFilter's index (All by default), and the stock
    // index of each item the shop lists under it.
    std::int64_t shop_filter_{3};
    std::vector<std::size_t> shop_rows_;
    std::uint64_t rendered_picture_revision_{};
    bool no_fog_{}, ready_{}, checking_{}, capture_{}, capture_pending_{};
    // The rest checks' command-line flags, read once.
    bool rest_check_{}, mastery_rest_check_{};
    bool town_check_{}, embedded_party_{};
    unsigned shop_check_stage_{};
    // At a treasure: Leave was pressed once with items still in the pile; the
    // next press leaves them behind.
    bool treasure_leave_warned_{};
    // What the last take at a treasure gave, shown under the pile.
    godot::String treasure_notice_;

    // In the inventory: Sell or Drop was pressed once for this item; the next
    // press of the same button gives it up (SELL-1).
    struct GiveUpWarning
    {
        std::uint64_t item{};
        bool sell{};
    };

    std::optional<GiveUpWarning> give_up_warned_;
    std::optional<opengold::PartyState> identify_check_retained_;
    std::uint64_t identify_check_sword_{};
    // The scroll whose spells the Cast menu offers (MAGIC-6); none while it
    // offers the member's own spells and features.
    std::optional<std::uint64_t> scroll_item_;

    // What the Cast menu offers: an action's id and whether it needs a target.
    struct CastOption
    {
        std::string id;
        bool whole_party{};
    };

    std::vector<CastOption> cast_options_;
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
    void forget_monster_picture();
    void dismiss_monster_picture();
    // The game's ending film, played over the whole window when the party
    // wins and the film is among the game's files; any key or click ends it.
    void play_ending_film();
    [[nodiscard]] bool play_closing_film();
    void show_closing_screen();
    void advance_closing_film(double delta);
    void end_ending_film();
    [[nodiscard]] bool ending_film_playing() const;
    void restart();
    void next();
    void choices_input(const godot::Ref<godot::InputEvent> &event);
    void fit_choice_columns();
    void shop_filter_selected(std::int64_t index);
    [[nodiscard]] bool passes_shop_filter(const opengold::por::Equipment &item) const;
    void left();
    void right();
    void forward();
    [[nodiscard]] bool on_overland() const;
    // The Buck Rogers games' solar map (rolf_tour_solar_map.cpp): the map in
    // the 3D view's place; the body's picture, the rocket, the fuel and the
    // clock in the side column; the map's menu and an eight-way pad.
    [[nodiscard]] bool on_solar_map() const;
    void connect_solar_map();
    // The game's Fly at once option ([direct_flight]); off plays the
    // original's Move mode.
    [[nodiscard]] bool flies_at_once() const;
    void fly_rocket(opengold::por::ExplorationCommand command);
    // A pad button's step, its ExplorationCommand as a number.
    void fly_pad(std::int64_t command);
    void toggle_rocket_move();
    void view_rocket();
    void change_settings();
    // A key the solar map answers; false for one it leaves to the screen.
    [[nodiscard]] bool solar_map_key(godot::Key key);
    // The pointer over the map names the body under it; a click flies a
    // step toward where it points.
    void solar_map_pointer(const godot::Ref<godot::InputEvent> &event);
    [[nodiscard]] std::optional<opengold::original::OrbitCell>
    solar_cell_at(godot::Vector2 point) const;
    // The side column, labels and buttons while the solar map shows, and
    // the normal screen's back once it does not.
    void show_solar_map(const opengold::por::TourSnapshot &s);
    // What TourVisuals draws for the solar map, added to `data`.
    void solar_visuals(godot::Dictionary &data);
    // Move mode is on (the original's, while Fly at once is off); the ship's
    // systems are shown; the body the pointer is over; the cells flown since
    // the solar map opened; and the map's state last drawn.
    bool rocket_move_mode_{}, rocket_shown_{};
    std::optional<std::size_t> hovered_body_;
    std::vector<opengold::original::OrbitCell> solar_course_;
    std::optional<opengold::original::SolarMapState> rendered_solar_;
    void look();
    // The original's Search mode: each step searches, and takes longer.
    void toggle_search();
    void camp();
    // The camp dialog, once the camp's opening is done (camp()).
    void show_camp_dialog();
    void setup_rest();
    void check_rest_controls();
    void check_mastery_rest_controls();
    // The open Camp dialog's Cast / Use row gives the Orc its Adrenaline Rush.
    void check_camp_adrenaline_rush(opengold::MemberId orc);
    unsigned rest_check_stage_{};
    void refresh_rest();
    // The rest time the dialog shows, under rules that rest any length.
    [[nodiscard]] std::chrono::minutes rest_length() const;
    void rest_selected(std::int64_t index);
    void rest_start();
    // Treasures' camp menu also offers Fix and Crown (camp.pas, CampMainMenu).
    void rest_fix();
    void rest_crown();
    void rest_heal();
    void rest_recover();
    void rest_use();
    std::optional<godot::String> use_outside_combat(opengold::MemberId user,
                                                    opengold::MemberId patient,
                                                    const godot::String &id,
                                                    const godot::String &action_text,
                                                    const godot::String &target_text);
    void cast_menu();
    void refresh_cast();
    void cast_action_selected(std::int64_t index);
    void cast_use();
    void read_or_copy_scroll(const std::string &id, opengold::MemberId patient, const godot::String &label);
    bool refresh_rest_use(const std::vector<opengold::MemberRestInfo> &infos, bool spending);
    // Teleport's cheapest offer becomes one entry for each place the party has
    // been (TELEPORT-1), its id followed by "@" and the map; its other slots'
    // offers go.
    void expand_teleport(std::vector<opengold::rules::CampAction> &actions,
                         std::vector<godot::String> &labels) const;
    void refresh_rest_spells();
    void rest_spell_toggled(bool selected, godot::String group, godot::String option);
    void rest_spell_replaced(std::int64_t index);
    void rest_spell_apply();
    void rest_spell_keep();
    void rest_spell_input(const godot::Ref<godot::InputEvent> &event);
    opengold::MemberId rest_spell_member_{};
    opengold::rules::SpellChoices rest_spell_choice_;
    void refresh_rest_training();
    [[nodiscard]] std::vector<opengold::rules::TrainingReplacementOptions>
    rest_training_choices(opengold::MemberId id) const;
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
    // show_note's note, kept over the movement hint while its seconds last.
    godot::String note_;
    double note_seconds_{};
    // The event whose camp offer the camp dialog last answered, and the one
    // that last opened Treasures' camp menu.
    unsigned camp_offer_run_{}, camp_menu_run_{};
    // The Menu button asks the host for the in-game menu.
    void request_menu();
    void inventory();
    void refresh_inventory();
    void inventory_selected(std::int64_t index);
    void equip_item(bool equip);
    void identify_item();
    // The shop's Sell (sell) or Drop: the first press says what happens, the
    // second gives the selected item up.
    void give_up_item(bool sell);
    // The prices a sale is judged by: the shop the party is in and the town's (SELL-1).
    [[nodiscard]] opengold::SaleMarket sale_market() const;
    void cast_identify();
    void use_inventory_item();
    void check_identification_spells();
    // A click on a party row chooses the member services go to.
    void party_selected(std::int64_t index);
    // A double click also opens the member's sheet.
    void open_member_sheet(std::int64_t index);
    // A shop or a treasure is showing, which lays the screen out for its list.
    [[nodiscard]] bool shop_layout() const;
    void show_party_rows();
    void level_up_requested(int slot);
    void close_sheet();
    void leave_shop();
    // At a treasure: the selected member takes the pile's `item` (-1: none
    // highlighted).
    void take_treasure(std::int64_t item);
    void movement(opengold::por::ExplorationCommand command);
    godot::Ref<godot::Texture2D> npc_portrait(std::string_view file);
    void check_run();
    void check_town();
    void check_recovery();
    void check_walk_to(unsigned x, unsigned y);
    void capture_frame(const godot::String &name);
};
#endif
