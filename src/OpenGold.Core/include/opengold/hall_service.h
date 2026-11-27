#ifndef OPENGOLD_HALL_SERVICE_H
#define OPENGOLD_HALL_SERVICE_H

#include "opengold/campaign_party.h"
#include "opengold/message.h"
#include "opengold/training_hall.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace opengold::por
{
class TownServiceHost;

// A training hall's PROGRAM 0 (host_halls.cpp): the script waits on the
// visit's request while the party buys sessions of experience and skill
// lessons from the list, or under rules that train at halls (the original's)
// trains its members as the original's hall does.
class HallService
{
  public:
    // The hall the script names opens; the script's `request` is answered
    // when the party leaves.
    void open_hall(TownServiceHost &host, std::uint64_t request);
    // The party chose the menu's `choice`.
    bool choose_at_hall(TownServiceHost &host, std::size_t choice);

    [[nodiscard]] bool visiting() const noexcept
    {
        return hall_visit_.has_value();
    }

    // Forgets the visit, as a new run of the tour or a loaded save does.
    void close() noexcept
    {
        hall_visit_.reset();
    }

  private:
    struct HallChoice
    {
        enum class Kind
        {
            session,
            lessons,
            lesson,
            back,
            leave,
            // Under rules that train at halls (the original's): a member to
            // train, training it, and a magic-user's new spell.
            train,
            confirm,
            learn
        };
        Kind kind{};
        opengold::MemberId member{};
        std::string grant; // a lesson's skill or Expertise grant, or a spell
    };

    struct HallVisit
    {
        std::uint64_t request{};
        opengold::TrainingHall hall{};
        opengold::MemberId member{}; // whose lessons are listed; 0 lists the services
        std::vector<HallChoice> choices;
        // The classes the original's hall trains (its script's class mask).
        unsigned class_mask{};
    };

    std::optional<HallVisit> hall_visit_;
    void show_hall(TownServiceHost &host, std::vector<opengold::rules::Message> notice);
    // The hall as the original runs it (chargen.pas, TrainingMenu): its
    // members, what one will become, and a magic-user's new spell.
    void show_original_hall(TownServiceHost &host, std::vector<opengold::rules::Message> notice);
    [[nodiscard]] bool choose_at_original_hall(TownServiceHost &host, const HallChoice &picked);
};
} // namespace opengold::por
#endif
