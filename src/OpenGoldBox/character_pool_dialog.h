#ifndef OPENGOLDBOX_CHARACTER_POOL_DIALOG_H
#define OPENGOLDBOX_CHARACTER_POOL_DIALOG_H
#include "portrait_catalog.h"
#include "opengold/campaign_party.h"
#include <godot_cpp/classes/window.hpp>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <vector>

// The character pool: 48 ready-made level-one characters to preview and add
// to the party. The window's controls come from the creation scene; its state
// is its own, and the campaign view hears when a member is added, so neither
// depends on the other's internals (Effective C++ Item 31).
class CharacterPoolDialog : public godot::Window
{
    GDCLASS(CharacterPoolDialog, godot::Window)
  public:
    using CampaignAccess = std::function<opengold::CampaignParty &()>;
    using FailureReport = std::function<void(const std::exception &)>;

    void _ready() override;
    // The art and portraits the pool is shown with, which outlive this window;
    // the host's campaign, read at each use because loading a save replaces it
    // (the pool is made under its rules); what follows an added member; and
    // where a failed handler is shown.
    void connect_host(const opengold::por::CharacterArt &art, PortraitCatalog &portraits,
                      CampaignAccess campaign, std::function<void()> member_added,
                      FailureReport report);
    // Builds the pool under the campaign's rules on first use, or once they
    // change, and shows it; throws if it cannot be built.
    void open();
    // Sizes the window and its controls to the campaign view.
    void fit(godot::Vector2 view_size);
    // Marks the pool characters the campaign already holds, after it has been
    // loaded or restored.
    void mark_added(const opengold::CampaignParty &campaign);

    [[nodiscard]] const std::vector<opengold::Character> &characters() const
    {
        return pool_;
    }

    void select(std::int64_t index);
    void add();
    void close();
    // Shows a failed handler's error; called by presentation::run_guarded.
    void report_failure(const std::exception &failure);

  protected:
    static void _bind_methods()
    {
    }

  private:
    void connect_controls();
    // The character rules the pool was made under.
    std::shared_ptr<const opengold::rules::CharacterRules> pool_rules_;
    // Borrowed from the campaign view, which owns them for longer than this
    // window exists.
    const opengold::por::CharacterArt *art_{};
    PortraitCatalog *portraits_{};
    CampaignAccess campaign_;
    std::function<void()> member_added_;
    FailureReport report_;
    std::vector<opengold::Character> pool_;
    std::vector<unsigned> pool_added_;
    unsigned pool_index_{};
};
#endif
