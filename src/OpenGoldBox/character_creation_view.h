#ifndef OPENGOLD_CHARACTER_CREATION_VIEW_H
#define OPENGOLD_CHARACTER_CREATION_VIEW_H
#include "opengold/character_creator.h"
#include "opengold/campaign_party.h"
#include "opengold/combat_body_catalog.h"
#include "original_portrait_parts.h"
#include "portrait_catalog.h"
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <exception>
#include <optional>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// The creation wizard, party panel, save/load and drag-and-drop stay in this
// class on purpose: they all read and write its creator, campaign and party
// state. The parts with state of their own are separate classes (Effective
// C++ Item 31): PortraitCatalog, LevelUpDialog, EquipmentChoiceDialog,
// CharacterPoolDialog, TownSheetDialog and presentation::sheet_text.
class CharacterCreationView : public godot::Control
{
    GDCLASS(CharacterCreationView, godot::Control)
  public:
    void _ready() override;
    void _draw() override;
    void _process(double delta) override;
    // Escape that nothing else used opens the in-game menu.
    void _unhandled_input(const godot::Ref<godot::InputEvent> &event) override;
    // Shows a failed handler's error; called by presentation::run_guarded.
    void report_failure(const std::exception &failure);

  protected:
    static void _bind_methods();
    void _notification(int what);

  private:
    void respond_to_notification(int what);
    void draw_view();
    std::unique_ptr<opengold::CharacterCreator> creator_;
    std::optional<opengold::Character> completed_;
    std::optional<opengold::por::CharacterArt> art_;
    // The combat bodies that look alike without weapons and shields, grouped.
    std::vector<std::vector<unsigned>> body_groups_;
    std::optional<opengold::por::CombatBodyCatalog> body_catalog_;
    std::optional<opengold::por::CharacterAppearance> rendered_;
    std::array<godot::Ref<godot::ImageTexture>, 3> images_;
    // A random portrait of the race chosen on the first page, and that race.
    godot::Ref<godot::ImageTexture> race_portrait_;
    std::string race_portrait_of_, race_portrait_gender_, race_portrait_file_;
    godot::String error_;
    int selected_score_{-1}, color_bank_{}, color_part_{};
    unsigned check_stage_{}, check_frames_{}, check_head_{}, check_default_{};
    unsigned drag_check_stage_{}, modal_check_stage_{};
    bool ready_{}, refreshing_{}, checking_{}, portrait_check_{}, capture_{}, fatal_{},
        portrait_chosen_{}, original_chosen_{}, on_portrait_page_{};
    // The race and gender the original portrait was last chosen at random for.
    std::string original_portrait_of_;
    void layout();
    void refresh();
    // Whether the game's rules roll a character whole, scores in place (the
    // original's), not assigned from six rolls.
    [[nodiscard]] bool rolls_in_place() const;
    void show_rolled_character(const opengold::rules::CharacterSheet *sheet);
    void refresh_art();
    void recommend_portrait();
    // The original portrait's head or body, `direction` places on.
    void original_head(std::int64_t direction);
    void original_body(std::int64_t direction);
    void step_original_part(presentation::OriginalPart part, std::int64_t direction);
    // Short for Gnomes, Large for Goliaths, Regular for everyone else.
    void recommend_icon_size();
    // Shows a random portrait of the draft's race; `again` picks a new one
    // even when the race is unchanged.
    void show_race_portrait(bool again);
    std::optional<PortraitCatalog> portraits_;
    std::vector<std::size_t> filtered_portraits_;
    void load_portraits();
    void fill_portrait_filters();
    void refresh_portraits();
    void match_portrait_filters();
    void portrait_filter_selected(std::int64_t index);

    // Runs a handler's change and refreshes, or shows its error. The
    // handlers' lambdas run at once, so the callable is not type-erased
    // (Effective Modern C++ Item 5).
    template <class Action> void perform(Action &&action)
    {
        if (!creator_ || fatal_)
        {
            return;
        }
        try
        {
            error_ = "";
            std::forward<Action>(action)();
            refresh();
        }
        catch (const std::exception &e)
        {
            show_error(e);
            redraw_after_refusal();
        }
    }

    // A refused change leaves the clicked control showing a choice the
    // character does not have; the page is drawn again from the character,
    // with the refusal's message.
    void redraw_after_refusal();

    void show_error(const std::exception &failure);
    void next();
    void back();
    void restart();
    void choice_selected(std::int64_t index);
    void gender_selected(std::int64_t index);
    void background_selected(std::int64_t index);
    void target_toggled(bool selected, int index);
    void bonus_selected(std::int64_t index);
    void creation_spell_toggled(bool selected, godot::String group, godot::String option);
    void cantrip_toggled(bool selected, godot::String option);
    void training_toggled(bool selected, godot::String group, godot::String option);
    void roll();
    void score_selected(int index);
    godot::Variant drag_roll(godot::Vector2 position, int index);
    bool can_drop_roll(godot::Vector2 position, const godot::Variant &data, int index);
    void drop_roll(godot::Vector2 position, const godot::Variant &data, int index);
    void show_modifiers();
    void close_modifiers();
    void show_saving_throws();
    void close_saving_throws();
    void update_saving_throws(godot::String value);
    void name_changed(godot::String value);
    void portrait_part(int direction);
    void portrait_selected(std::int64_t index);
    void combat_part(int part, int direction);
    void size_selected(std::int64_t index);
    // Which of body_groups_ holds the body.
    [[nodiscard]] std::size_t combat_body_group(unsigned body) const;
    void color_selected(int bank, int part);
    void palette_selected(int index);
    void check_run();
    void capture(const char *name);
    std::shared_ptr<opengold::CampaignParty> campaign_;
    std::size_t roster_index_{};
    bool party_open_{}, added_to_party_{}, party_check_{};
    bool equipment_art_check_{};
    void equipment_art_check();
    unsigned party_check_stage_{};
    unsigned pool_check_stage_{};
    void show_pool();
    // Curse's Add Character from Pool of Radiance, offered when the party
    // plays Curse under the original rules.
    void show_pool_of_radiance();
    [[nodiscard]] bool bringing_across() const;
    // The leader speaks for the party and deals with shopkeepers.
    void make_roster_leader();
    void show_leader_change();
    // Offers a choice of hands when the item has one; false when it has none.
    bool open_equipment_choice(opengold::MemberId member, std::uint64_t item);
    void setup_party();
    // How the startup menu began the game (settings::chosen_game_start), read
    // once so a later return here makes a new party; for the checks,
    // --default-party chooses the default party and --load-save=PATH a save.
    void begin_chosen_start();
    // Play with Default Party: the game's saved game A, then Begin
    // Adventuring; a party that cannot be read leaves the creation screen
    // with the reason.
    void play_with_default_party();
    // Continue or Load Game: the save, then exploring where it was made.
    void load_saved_start();
    std::filesystem::path saved_start_;

    // Reincarnate from camp (LEVELS-5): the creation pages for a dead
    // member's new body, then back to the town.
    struct Reincarnation
    {
        opengold::MemberId caster{}, target{};
        std::string action;
    };

    std::optional<Reincarnation> reincarnation_;

    // The character being made when the spell took over the pages, back
    // when it ends.
    struct CreationInProgress
    {
        std::unique_ptr<opengold::CharacterCreator> creator;
        std::optional<opengold::Character> completed;
        bool added_to_party{}, portrait_chosen{};
    };

    std::optional<CreationInProgress> waiting_creation_;
    void open_reincarnation(std::int64_t caster, std::int64_t target, godot::String action);
    void begin_reincarnation(Reincarnation spell, const std::string &species);
    void reincarnate_check();
    bool reincarnate_check_{};
    unsigned reincarnate_stage_{}, reincarnate_frames_{};
    void finish_reincarnation();
    void leave_reincarnation();
    // Drops the reincarnation pages' creator, which borrows the campaign's
    // rules module, before that campaign is replaced (#239).
    void end_reincarnation();
    void setup_advancement();
    void refresh_advancement_arrows();
    void open_advancement(std::int64_t id);
    void advancement_check();
    bool advancement_check_{}, advancement_review_{};
    unsigned advancement_stage_{}, advancement_frames_{};
    void setup_saves();
    void setup_defeat();
    void show_defeat();
    void reload_after_defeat();
    void exit_after_defeat();
    void save_dialog_visibility_changed();
    void restore_defeat_dialog();
    void defeat_check();
    void expedition_check();
    void add_original_expedition_party();
    bool expedition_check_{}, expedition_started_{}, expedition_saved_{};
    unsigned expedition_frames_{};
    bool campaign_defeated_{}, defeat_check_{};
    unsigned defeat_check_stage_{}, defeat_check_frames_{};
    void open_saves(bool saving);
    void save_campaign(const std::filesystem::path &path);
    void load_campaign(const std::filesystem::path &path);
    // Loads a campaign from a save's bytes: a file's, or the game in progress
    // saved in memory.
    void load_campaign_bytes(const std::string &bytes);
    void reload_in_memory();
    void save_checkpoint_check(const std::string &name);
    void load_checkpoint_check();
    bool save_read_check_{};
    unsigned save_capture_frames_{};
    void capture_save_ui();
    // The party panel's commands, named so a call says which one it means
    // rather than passing a bare number (Effective C++ Item 18).
    enum class PartyAction
    {
        open,
        add_created,
        create,
        remove,
        rejoin,
        equip,
        unequip,
        explore,
        return_to_party,
        close,
    };
    void connect_party_button(const char *path, PartyAction action);
    void party_button_pressed(int action);
    void party_action(PartyAction action);
    void party_selected(std::int64_t index);
    // The roster's rows, as indices into the campaign's roster: the party in
    // its order, which a fight stands it in, then the members out of it.
    [[nodiscard]] std::vector<std::size_t> roster_rows() const;
    // Dragging a player character in the roster moves it in the party's order.
    godot::Variant drag_member(godot::Vector2 position);
    bool can_drop_member(godot::Vector2 position, const godot::Variant &data);
    void drop_member(godot::Vector2 position, const godot::Variant &data);
    // The place among the party's player characters a member dropped at
    // `position` takes, or none when the row there is not one of them.
    [[nodiscard]] std::optional<std::size_t> drop_place(godot::Vector2 position) const;
    void refresh_party();
    void party_layout();
    void party_check();
    // The checks' fight: one Bandit on an open field, begun as exploration begins a
    // campaign fight, with no exploration to report to when it ends.
    void start_check_fight();
    bool check_fight_{};
    // The roster gives way to exploration or a fight, with the way back on top.
    void leave_roster();
    // The in-game menu (game_menu_host.cpp): opened with what it allows, the
    // exploration screen and the fight paused beneath it until it closes.
    void attach_game_menu();
    void open_game_menu();
    void game_menu_closed();
    // The town's walls and a fight's creatures take the new art setting.
    void art_changed();
    void new_art_changed();
    // The exploration screen's and a fight's sounds take the new volume.
    void volume_changed();
    // Reads the portraits again, from another new art folder.
    void reload_portraits();

    struct Paused
    {
        const char *node;
        bool process, input;
    };

    std::vector<Paused> paused_;
    void update_party_navigation();
};
#endif
