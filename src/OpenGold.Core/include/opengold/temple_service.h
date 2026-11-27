#ifndef OPENGOLD_TEMPLE_SERVICE_H
#define OPENGOLD_TEMPLE_SERVICE_H

#include "opengold/campaign_party.h"
#include "opengold/message.h"
#include "opengold/temple_catalog.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace opengold::por
{
class TownServiceHost;

// A temple's COMBAT request with the healing flag (host_temples.cpp): the
// script waits on the visit's request while the party buys services from the
// temple's Heal list (TEMPLE-1). The list names the services; choosing one
// lists the members it would help, and choosing one of those asks to confirm
// the price.
class TempleService
{
  public:
    // The temples data/shops describes (TEMPLE-1); a temple it does not name
    // offers the original's services and prices.
    void temples(std::vector<opengold::TempleSite> sites)
    {
        temples_ = std::move(sites);
    }

    [[nodiscard]] const std::vector<opengold::TempleSite> &temples() const noexcept
    {
        return temples_;
    }

    // The temple the party stands at opens; the script's `request` is
    // answered when the party leaves.
    void open_temple(TownServiceHost &host, std::uint64_t request);
    // The party chose the menu's `choice`.
    bool choose_at_temple(TownServiceHost &host, std::size_t choice);

    [[nodiscard]] bool visiting() const noexcept
    {
        return temple_visit_.has_value();
    }

    // Forgets the visit, as a new run of the tour or a loaded save does.
    void close() noexcept
    {
        temple_visit_.reset();
    }

  private:
    struct TempleChoice
    {
        enum class Kind
        {
            service,
            member,
            beyond_raising, // dead too long or turned to dust, on Hard and above
            confirm,
            back,
            leave
        };
        Kind kind{};
        opengold::MemberId member{};
        opengold::TempleOffering offering;
    };

    struct TempleVisit
    {
        std::uint64_t request{};
        std::vector<opengold::TempleOffering> offerings;
        std::optional<opengold::TempleOffering> chosen; // the service whose members are listed
        opengold::MemberId member{};                    // whose purchase awaits confirmation
        std::vector<TempleChoice> choices;
    };

    std::vector<opengold::TempleSite> temples_;
    std::optional<TempleVisit> temple_visit_;
    void show_temple(TownServiceHost &host, std::vector<opengold::rules::Message> notice);
};
} // namespace opengold::por
#endif
