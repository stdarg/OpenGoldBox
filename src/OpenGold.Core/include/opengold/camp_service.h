#ifndef OPENGOLD_CAMP_SERVICE_H
#define OPENGOLD_CAMP_SERVICE_H

#include "opengold/campaign_party.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <utility>

namespace opengold::por
{
class TownServiceHost;

// A camp's rest (host_camp.cpp): how long the camp running rests, the rolls
// that interrupt it, and the wait for its casters' spells. The camp's events
// (its scripts, Treasures' camp menu and romance) are the tour's.
class CampService
{
  public:
    // Dice of [0, count): the engine's between scripts, the request's
    // (host_random) while a script waits on the host.
    using Dice = std::function<unsigned(unsigned)>;

    // The camp waits while its casters choose their spells, under rules that
    // rest any length (gametime.pas, CampRest): the rest, interrupted already
    // or not, goes on for their preparation, and a rest encounter comes once
    // they have chosen.
    struct CampMemorizing
    {
        // An interruption ended the rest's own time.
        bool interrupted{};
        // The campaign's clock as the rest's own time ended.
        std::uint64_t rest_end_minutes{};
        RestResult rest;
    };

    // A camp of `kind` begins, of `length` when one was asked for.
    void start(RestKind kind, std::optional<std::chrono::minutes> length) noexcept
    {
        camp_kind_ = kind;
        camp_length_ = length;
    }

    // The campaign's rest for the camp running: camp_kind_'s, of camp_length_
    // when one was asked for.
    [[nodiscard]] std::optional<RestResult> camp_rest(TownServiceHost &host);
    // The minutes the camp running rests before an interruption, or nothing
    // when none occurs, rolled on the engine's dice every `interval` steps
    // at `chance` in 100.
    [[nodiscard]] std::optional<unsigned> rest_interruption(TownServiceHost &host,
                                                            unsigned interval, unsigned chance);
    // Over a rest of `minutes`, rolled with `dice`.
    [[nodiscard]] std::optional<unsigned> rest_interruption(unsigned interval, unsigned chance,
                                                            unsigned minutes, const Dice &dice);
    // Where the rest going on for its casters' spells would be interrupted,
    // rolled ahead on the engine's dice and taken back (the rolls come again
    // as the rest goes on), tells the party's rest.
    void foresee_rest_interruption(TownServiceHost &host, unsigned interval, unsigned chance);

    [[nodiscard]] bool memorizing() const noexcept
    {
        return camp_memorizing_.has_value();
    }

    void wait_for_spells(CampMemorizing waiting) noexcept
    {
        camp_memorizing_ = std::move(waiting);
    }

    // The wait for the casters' spells is over; what it waited with.
    [[nodiscard]] CampMemorizing end_memorizing()
    {
        return *std::exchange(camp_memorizing_, std::nullopt);
    }

    // Five-minute rest steps since the last interruption check. Like the
    // original engine's counter, it carries over between rests; it is saved.
    [[nodiscard]] unsigned rest_checks() const noexcept
    {
        return rest_checks_;
    }

    // As a new run of the tour: no step counted, no wait, a long rest next.
    void restart() noexcept
    {
        rest_checks_ = 0;
        camp_memorizing_.reset();
        camp_kind_ = RestKind::long_rest;
    }

    // As a rolled-back event began: its count of steps, and no wait.
    void roll_back(unsigned rest_checks) noexcept
    {
        rest_checks_ = rest_checks;
        camp_memorizing_.reset();
    }

    // Reads `camp`'s count of rest steps from a campaign save, or writes a
    // const one (campaign_save.cpp).
    template <class Codec, class Camp> static void save(Codec &codec, Camp &camp);

  private:
    // How long the camp running rests before its preparation lengthens it.
    [[nodiscard]] unsigned camp_minutes(const CampaignParty &campaign) const;
    RestKind camp_kind_{RestKind::long_rest};
    // The length camp(std::chrono::minutes) asked for; none for a rest of
    // camp_kind_'s own length.
    std::optional<std::chrono::minutes> camp_length_;
    unsigned rest_checks_{};
    std::optional<CampMemorizing> camp_memorizing_;
};
} // namespace opengold::por
#endif
