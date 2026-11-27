#ifndef OPENGOLD_BUCK_ROGERS_MENU_SERVICE_H
#define OPENGOLD_BUCK_ROGERS_MENU_SERVICE_H

#include "opengold/buck_rogers_records.h"
#include "opengold/buck_rogers_rules.h"
#include "opengold/campaign_party.h"
#include "opengold/message.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace opengold::por
{
struct PhlanResources;
class TownServiceHost;

// A Buck Rogers training begun in the team menu (chargen.pas TrainingMenu):
// the trainee and what is left to choose; the team menu shows again once the
// choices are kept.
struct BuckRogersTraining
{
    opengold::MemberId member{};
    original::BuckRogersTrainingChoices choices;
};

// The Buck Rogers menus the session offers in the original's place
// (buck_rogers_host.cpp): the booty after a fight, a shop, the team menu with
// its training, and the rest menu.
class BuckRogersMenuService
{
  public:
    enum class Kind : std::uint8_t
    {
        booty,
        shop,
        shop_buy,
        shop_sell,
        shop_ammunition,
        team,
        rest
    };

    // The menu of `kind` opens, selling `stock` in a shop; the script's
    // `request` is answered when the party leaves it.
    void open_menu(TownServiceHost &host, Kind kind, std::uint64_t request,
                   std::vector<buck_rogers::Item> stock = {});

    [[nodiscard]] bool choosing() const noexcept
    {
        return buck_rogers_menu_.has_value();
    }

    // A shop or the booty, at which the party list chooses who buys or takes.
    [[nodiscard]] bool selects_member() const noexcept
    {
        return buck_rogers_menu_ &&
               (buck_rogers_menu_->kind == Kind::shop || buck_rogers_menu_->kind == Kind::booty);
    }

    bool choose_in_buck_rogers_menu(TownServiceHost &host, std::size_t choice);

    // The fight's or the script's booty (gdvars.pas availableItems) and the
    // team's pool of credits (poolCredits), kept while a script runs.
    void add_credits(std::uint32_t credits) noexcept
    {
        buck_rogers_pool_ += credits;
    }

    // The item comes first in the booty, as the original inserts it.
    void add_booty(buck_rogers::Item item)
    {
        buck_rogers_booty_.insert(buck_rogers_booty_.begin(), std::move(item));
    }

    [[nodiscard]] bool booty_waiting() const noexcept
    {
        return !buck_rogers_booty_.empty() || buck_rogers_pool_ != 0;
    }

    void clear_booty() noexcept
    {
        buck_rogers_booty_.clear();
        buck_rogers_pool_ = 0;
    }

    // The training; see RolfTourSession::buck_rogers_training and the
    // training choices beside it.
    [[nodiscard]] const BuckRogersTraining *buck_rogers_training() const noexcept
    {
        return buck_rogers_training_ ? &*buck_rogers_training_ : nullptr;
    }

    [[nodiscard]] original::SkillRefusal raise_training_skill(TownServiceHost &host,
                                                              original::Skill skill);
    bool lower_training_skill(TownServiceHost &host, original::Skill skill);
    [[nodiscard]] bool specialize_in_training(TownServiceHost &host, std::uint8_t item_type);
    bool unspecialize_in_training(TownServiceHost &host, std::uint8_t item_type);
    void finish_buck_rogers_training(TownServiceHost &host);

    // Days of rest for the team (gametime.pas RestAndHeal), the clock moved on.
    static void rest_buck_rogers_team(CampaignParty &campaign, int days);

  private:
    struct BuckRogersMenu
    {
        using Kind = BuckRogersMenuService::Kind;
        enum class Action : std::uint8_t
        {
            take_item,
            take_credits,
            share_credits,
            pool_credits,
            buy,
            sell,
            ammunition_clip,
            ammunition_fill,
            open_buy,
            open_sell,
            open_ammunition,
            back,
            train,
            rest_a_day,
            leave
        };

        struct Choice
        {
            Action action{};
            std::size_t index{};
        };

        Kind kind{};
        std::uint64_t request{};
        std::vector<Choice> choices;
        std::vector<buck_rogers::Item> stock;
    };

    void show_buck_rogers_menu(TownServiceHost &host, BuckRogersMenu::Kind kind,
                               std::vector<rules::Message> notice = {});
    void close_buck_rogers_menu(TownServiceHost &host);
    [[nodiscard]] static std::string buck_rogers_item_name(const PhlanResources &town,
                                                           const buck_rogers::Item &item);
    std::optional<BuckRogersMenu> buck_rogers_menu_;

    // It is not saved: saving waits for the script, which waits for the menu.
    std::optional<BuckRogersTraining> buck_rogers_training_;
    [[nodiscard]] original::BuckRogersTrainingChoices &training_in_progress();
    std::vector<buck_rogers::Item> buck_rogers_booty_;
    std::uint32_t buck_rogers_pool_{};
};
} // namespace opengold::por
#endif
