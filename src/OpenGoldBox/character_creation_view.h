#ifndef OPENGOLD_CHARACTER_CREATION_VIEW_H
#define OPENGOLD_CHARACTER_CREATION_VIEW_H
#include "opengold/character_creator.h"
#include "opengold/campaign_party.h"
#include "opengold/combat_body_catalog.h"
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <exception>
#include <optional>
#include <functional>

class CharacterCreationView : public godot::Control
{
    GDCLASS(CharacterCreationView, godot::Control)
  public:
    void _ready() override;
    void _draw() override;
    void _process(double delta) override;
    // Shows a failed handler's error; called by presentation::run_guarded.
    void report_failure(const std::exception &failure);

  protected:
    static void _bind_methods();
    void _notification(int what);

  private:
    void draw_view();
    std::unique_ptr<opengold::CharacterCreator> creator_;
    std::optional<opengold::Character> completed_;
    std::optional<opengold::por::CharacterArt> art_;
    std::optional<opengold::por::CombatBodyCatalog> body_catalog_;
    std::optional<opengold::por::CharacterAppearance> rendered_;
    std::array<godot::Ref<godot::ImageTexture>, 3> images_;
    godot::Rect2 page_rect_, preview_rect_, portrait_rect_, ready_rect_, action_rect_;
    godot::String error_;
    int selected_score_{-1}, color_bank_{}, color_part_{};
    unsigned check_stage_{}, check_frames_{}, check_head_{}, check_default_{};
    unsigned drag_check_stage_{}, modal_check_stage_{};
    bool ready_{}, refreshing_{}, checking_{}, capture_{}, fatal_{}, portrait_chosen_{};
    void layout();
    void refresh();
    void refresh_art();
    void recommend_portrait();

    struct Portrait
    {
        std::string filename, gender, klass, race;
    };

    std::vector<Portrait> portraits_;
    std::map<std::string, godot::Ref<godot::ImageTexture>> portrait_textures_;
    std::vector<std::size_t> filtered_portraits_;
    void load_portraits();
    void refresh_portraits();
    void portrait_filter_selected(std::int64_t index);
    std::string recommended_portrait(const opengold::rules::CharacterDraft &draft) const;
    godot::Ref<godot::ImageTexture>
    portrait_texture(const opengold::por::CharacterAppearance &appearance,
                     const opengold::rules::CharacterDraft &draft);
    void perform(const std::function<void()> &action);
    void next();
    void back();
    void restart();
    void choice_selected(std::int64_t index);
    void gender_selected(std::int64_t index);
    void background_selected(std::int64_t index);
    void target_toggled(bool selected, int index);
    void bonus_selected(std::int64_t index);
    void creation_spell_toggled(bool selected, godot::String group, godot::String option);
    // The level-up window is a sequence of pages, not a single optional extra:
    // Skilled and a Wizard's spell choices can both follow the first page, so a
    // flag cannot say which one is showing.
    enum class AdvancementPage
    {
        choices,
        skilled,
        spells
    };
    AdvancementPage advancement_page_{};
    void advancement_pages();
    [[nodiscard]] const opengold::rules::TrainingChoiceGroup *advancement_skilled_group() const;
    // The single training dropdown never shows Skilled, which owns a whole page.
    [[nodiscard]] const opengold::rules::TrainingChoiceGroup *advancement_dropdown_group() const;
    [[nodiscard]] AdvancementPage advancement_next_page(AdvancementPage from) const;
    [[nodiscard]] AdvancementPage advancement_previous_page(AdvancementPage from) const;
    void refresh_advancement_skilled();
    void advancement_skilled_toggled(bool selected, godot::String option);
    void advancement_back();
    void advancement_learning_toggled(bool selected, godot::String group, godot::String option);
    void cantrip_toggled(bool selected, godot::String option);
    void training_toggled(bool selected, godot::String group, godot::String option);
    void training_selected(std::int64_t index, godot::String group);
    void roll();
    void score_selected(int index);
    godot::Variant drag_roll(godot::Vector2 position, int index);
    bool can_drop_roll(godot::Vector2 position, const godot::Variant &data, int index);
    void drop_roll(godot::Vector2 position, const godot::Variant &data, int index);
    godot::String sheet_text(const opengold::Character &character,
                             const opengold::PartyMember *member = nullptr) const;
    void show_modifiers();
    void close_modifiers();
    void show_saving_throws();
    void close_saving_throws();
    void update_saving_throws(godot::String value);
    void name_changed(godot::String value);
    void portrait_part(int direction);
    void portrait_selected(std::int64_t index);
    void combat_part(int part, int direction);
    void toggle_size();
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
    std::vector<opengold::Character> pool_;
    std::vector<unsigned> pool_added_;
    unsigned pool_index_{}, pool_check_stage_{};
    void show_pool();
    void pool_selected(std::int64_t index);
    void pool_add();
    void close_pool();
    void pool_layout();
    void town_member_selected(std::int64_t slot);
    void close_town_sheet();
    // The leader speaks for the party and deals with shopkeepers.
    void make_roster_leader();
    void make_town_sheet_leader();
    void show_leader_change();
    opengold::MemberId town_sheet_member_{};
    bool open_equipment_choice(opengold::MemberId member, std::uint64_t item);
    void equipment_choice_selected(std::int64_t index);
    void apply_equipment_choice();
    void close_equipment_choice();
    void equipment_choice_input(const godot::Ref<godot::InputEvent> &event);
    opengold::MemberId equipment_member_{};
    std::uint64_t equipment_item_{};
    std::vector<opengold::rules::EquipmentChoice> equipment_choices_;
    void setup_party();
    void setup_advancement();
    void refresh_advancement_arrows();
    void open_advancement(std::int64_t id);
    void advancement_changed(std::int64_t unused = 0);
    void drop_unoffered_learning();
    void advancement_spell_changed(bool checked, int index);
    void close_advancement();
    void confirm_advancement();
    opengold::MemberId advancing_{};
    opengold::rules::AdvancementOptions advancement_options_;
    opengold::rules::AdvancementChoice advancement_choice_;
    bool advancement_refreshing_{};
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
    bool expedition_check_{}, expedition_started_{}, expedition_saved_{};
    unsigned expedition_frames_{};
    bool campaign_defeated_{}, defeat_check_{};
    unsigned defeat_check_stage_{}, defeat_check_frames_{};
    void open_saves(bool saving);
    void save_campaign(const std::filesystem::path &path);
    void load_campaign(const std::filesystem::path &path);
    void save_checkpoint_check(const std::string &name);
    void load_checkpoint_check();
    bool save_read_check_{};
    unsigned save_capture_frames_{};
    void capture_save_ui();
    void party_action(int action);
    void party_selected(std::int64_t index);
    void refresh_party();
    void party_layout();
    void party_check();
    void update_party_navigation();
};
#endif
