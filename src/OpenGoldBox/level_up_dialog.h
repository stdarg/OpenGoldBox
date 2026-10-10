#ifndef OPENGOLDBOX_LEVEL_UP_DIALOG_H
#define OPENGOLDBOX_LEVEL_UP_DIALOG_H
#include "godot_nodes.h"
#include "opengold/campaign_party.h"
#include <godot_cpp/classes/window.hpp>
#include <exception>
#include <functional>

// The level-up window: its pages, the choices made on them and the preview of
// the result. Its state is its own; the campaign view supplies the campaign
// and hears when a level-up is confirmed, so neither depends on the other's
// internals (Effective C++ Item 31).
class LevelUpDialog : public godot::Window
{
    GDCLASS(LevelUpDialog, godot::Window)
  public:
    using CampaignAccess = std::function<opengold::CampaignParty &()>;
    using FailureReport = std::function<void(const std::exception &)>;

    // The hidden window, named LevelUp, with all of its controls.
    [[nodiscard]] static presentation::NodeOwner<LevelUpDialog> create();
    // The host's campaign, read at each use because loading a save replaces
    // it; what follows a confirmed level-up; and where a failed handler is
    // shown. All three are required.
    void connect_host(CampaignAccess campaign, std::function<void()> advanced,
                      FailureReport report);
    // Opens on the first page for a member the host has checked can advance.
    void open(opengold::MemberId member);

    [[nodiscard]] const opengold::rules::AdvancementOptions &options() const
    {
        return advancement_options_;
    }

    // Shows a failed handler's error; called by presentation::run_guarded.
    void report_failure(const std::exception &failure);

  protected:
    static void _bind_methods()
    {
    }

  private:
    // The level-up window is a sequence of pages, not a single optional extra:
    // Skilled and a Wizard's spell choices can both follow the first page, so a
    // flag cannot say which one is showing.
    enum class AdvancementPage
    {
        choices,
        skilled,
        spells
    };

    CampaignAccess campaign_;
    std::function<void()> advanced_;
    FailureReport report_;
    AdvancementPage advancement_page_{};
    opengold::MemberId advancing_{};
    opengold::rules::AdvancementOptions advancement_options_;
    opengold::rules::AdvancementChoice advancement_choice_;
    bool advancement_refreshing_{};

    void add_controls();
    [[nodiscard]] opengold::CampaignParty &campaign();
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
    void advancement_changed(std::int64_t unused = 0);
    void drop_unoffered_learning();
    void advancement_spell_changed(bool checked, int index);
    void close();
    void confirm();
};
#endif
