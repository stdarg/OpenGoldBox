#ifndef OPENGOLD_EXPLORATION_SESSION_H
#define OPENGOLD_EXPLORATION_SESSION_H

#include "opengold/rolf_tour.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace opengold
{

// What the game's exploration view asks of the adventure it plays
// (docs/games/ua.md, The seam): a DAX game's campaign
// (DaxExplorationSession) or a design made with Unlimited Adventures' kit
// (ua::UaSession). Both speak through the same snapshot, commands, answers
// and fights. What only a DAX campaign has (shops, treasure piles, its
// script's variables, its overhead map and sprites, temples, teleports,
// close-ups and camping) is reached through dax(), which is null for a
// design, so the view leaves those out.
class ExplorationSession
{
  public:
    virtual ~ExplorationSession() = default;

    [[nodiscard]] virtual const por::TourSnapshot &snapshot() const noexcept = 0;
    // The party plays the adventure; its fights run under the party's rules.
    virtual void campaign_party(std::shared_ptr<CampaignParty> party) = 0;
    // The adventure from its start again; campaign_party then names the party.
    virtual void restart() = 0;
    // Time passing on the screen: a script's pauses and pictures.
    virtual void advance(double seconds) = 0;
    // A step, a turn or a look; false while an event waits or the way is shut.
    virtual bool explore(por::ExplorationCommand command) = 0;
    // An answer to what waits, with the snapshot's ticket: the choice made
    // (0 goes on from words), or the words typed.
    virtual bool choose(std::uint64_t ticket, std::size_t choice) = 0;
    virtual bool input(std::uint64_t ticket, std::string_view value) = 0;
    // The original's Search mode: each step searches, and takes longer.
    virtual void set_searching(bool searching) = 0;
    // Nothing waits for the player: the party may camp, save or leave.
    [[nodiscard]] virtual bool can_leave() const = 0;
    // The player may change the selected member now.
    [[nodiscard]] virtual bool can_select_member() const = 0;
    // The member will fight for the monsters in the next fight.
    [[nodiscard]] virtual bool turns_on_party(MemberId id) const = 0;

    // The fight an event started, until resolve_combat takes its result or
    // reject_combat refuses it (the fight could not be set up).
    [[nodiscard]] virtual const std::optional<CampaignEncounter> &pending_encounter() const = 0;
    virtual bool resolve_combat(const rules::Snapshot &outcome) = 0;
    virtual bool reject_combat(std::string diagnostic) = 0;

    // What the view's window shows: a picture an event shows, or the 3D view
    // where the party stands. Nullopt where there is nothing to draw yet (a
    // design's overland areas).
    [[nodiscard]] virtual std::optional<Image> view_picture() = 0;
    // The adventure's own name for the party's area (a design's area name);
    // empty when the view names it, as it does a DAX game's areas.
    [[nodiscard]] virtual std::string area_name() const = 0;

    // The DAX campaign played, or null for a design.
    [[nodiscard]] virtual por::RolfTourSession *dax() noexcept = 0;
    [[nodiscard]] virtual const por::RolfTourSession *dax() const noexcept = 0;

  protected:
    // Only a session's own class copies or moves it (C++ Core Guidelines C.67).
    ExplorationSession() = default;
    ExplorationSession(const ExplorationSession &) = default;
    ExplorationSession(ExplorationSession &&) = default;
    ExplorationSession &operator=(const ExplorationSession &) = default;
    ExplorationSession &operator=(ExplorationSession &&) = default;
};

// A DAX game's campaign as the exploration view plays it.
class DaxExplorationSession final : public ExplorationSession
{
  public:
    explicit DaxExplorationSession(por::RolfTourSession session) : session_(std::move(session))
    {
    }

    [[nodiscard]] const por::TourSnapshot &snapshot() const noexcept override
    {
        return session_.snapshot();
    }

    void campaign_party(std::shared_ptr<CampaignParty> party) override
    {
        session_.campaign_party(std::move(party));
    }

    void restart() override
    {
        session_.restart();
    }

    void advance(double seconds) override
    {
        session_.advance(seconds);
    }

    bool explore(por::ExplorationCommand command) override
    {
        return session_.explore(command);
    }

    bool choose(std::uint64_t ticket, std::size_t choice) override
    {
        return session_.choose(ticket, choice);
    }

    bool input(std::uint64_t ticket, std::string_view value) override
    {
        return session_.input(ticket, value);
    }

    void set_searching(bool searching) override
    {
        session_.set_searching(searching);
    }

    [[nodiscard]] bool can_leave() const override
    {
        return session_.can_leave();
    }

    [[nodiscard]] bool can_select_member() const override
    {
        return session_.can_select_member();
    }

    [[nodiscard]] bool turns_on_party(MemberId id) const override
    {
        return session_.turns_on_party(id);
    }

    [[nodiscard]] const std::optional<CampaignEncounter> &pending_encounter() const override
    {
        return session_.pending_encounter();
    }

    bool resolve_combat(const rules::Snapshot &outcome) override
    {
        return session_.resolve_combat(outcome);
    }

    bool reject_combat(std::string diagnostic) override
    {
        return session_.reject_combat(std::move(diagnostic));
    }

    [[nodiscard]] std::optional<Image> view_picture() override
    {
        return session_.picture() ? *session_.picture() : session_.observe_view();
    }

    [[nodiscard]] std::string area_name() const override
    {
        return {};
    }

    [[nodiscard]] por::RolfTourSession *dax() noexcept override
    {
        return &session_;
    }

    [[nodiscard]] const por::RolfTourSession *dax() const noexcept override
    {
        return &session_;
    }

  private:
    por::RolfTourSession session_;
};

} // namespace opengold

#endif
