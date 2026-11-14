#ifndef OPENGOLD_UA_SESSION_H
#define OPENGOLD_UA_SESSION_H

#include "opengold/campaign_encounter.h"
#include "opengold/campaign_party.h"
#include "opengold/original_combat.h"
#include "opengold/rolf_tour.h"
#include "opengold/ua_design.h"
#include "opengold/ua_view.h"

#include <array>
#include <bitset>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Playing an adventure made with Unlimited Adventures' construction kit
// (docs/games/ua.md, Playing a design). A design is not a DAX campaign: its
// areas hold the kit's own event records, not scripts, so it has a session of
// its own beside the DAX games' RolfTourSession. It speaks the same language
// to the view: the same TourSnapshot (where the party stands, what is said,
// the choices), the same exploration commands and answers, a fight as a
// CampaignEncounter for the original rules' combat, and the 3D view drawn by
// the same renderer from the design's own wall sets.
namespace opengold::ua
{

// One of an area's 100 event records, 20 bytes; byte 0 is its kind.
using EventRecord = std::array<std::uint8_t, 20>;

// The kinds of event the kit offers (the event editor's forms, SCRIPT.GLB).
enum class EventKind : std::uint8_t
{
    none = 0,
    combat = 1,
    text = 2,
    give_treasure = 3,
    damage = 4,
    stairs = 5,
    training_hall = 6,
    tavern = 7,
    shop = 8,
    temple = 9,
    question_buttons = 10,
    transfer_module = 11,
    guided_tour = 12,
    add_npc = 13,
    npc_says = 14,
    encounter = 15,
    utilities = 16,
    sounds = 17,
    who_tries = 18,
    who_pays = 19,
    password = 20,
    question_list = 21,
    small_town = 22,
    chain = 23,
    vault = 24,
    combat_treasure = 25,
    gain_experience = 26,
    pass_time = 27,
    camp = 29,
    remove_npc = 32,
    pick_one_combat = 33,
    teleporter = 34,
    quest_stage = 35,
    yes_no = 36,
    tavern_tales = 37,
    special_item = 38,
};

// An area of the design as the session plays it.
struct DesignArea
{
    unsigned number{};
    Area area;

    [[nodiscard]] bool overland() const noexcept
    {
        // Areas 1 to 4 are the overland maps (docs/games/ua.md).
        return number <= 4;
    }
};

// What a design's monsters fight as under the original rules: the bestiary's
// key for MONST record `number`.
[[nodiscard]] std::string monster_key(unsigned number);

// The original rules' content of a design: its monsters (MONST, by
// monster_key) and the kit's item types (ITEMS.DAT). Throws when ITEMS.DAT
// cannot be read.
[[nodiscard]] original::Content load_design_content(const DesignFiles &files);

// The numbers a MONST record fights with under the original rules.
[[nodiscard]] original::Combatant monster_combatant(const Monster &monster);

class UaSession
{
  public:
    // The design's start: the game record's area and entry point. Throws
    // when the design has no game record or its start area.
    [[nodiscard]] static UaSession start(DesignFiles files);

    // The party plays the design; its members' fights run through `party`'s
    // rules. The start square's event runs once the party is there.
    void campaign_party(std::shared_ptr<CampaignParty> party);

    [[nodiscard]] const por::TourSnapshot &snapshot() const noexcept
    {
        return snapshot_;
    }

    [[nodiscard]] const DesignArea &area() const noexcept
    {
        return area_;
    }

    [[nodiscard]] const DesignFiles &files() const noexcept
    {
        return files_;
    }

    // A step, a turn or a look; false while an event waits or the way is shut.
    bool explore(por::ExplorationCommand command);
    // Answers to what an event waits for, with the snapshot's ticket.
    bool continue_dialogue(std::uint64_t ticket);
    bool choose(std::uint64_t ticket, std::size_t choice);
    bool input(std::uint64_t ticket, std::string_view value);

    // The fight an event started, until resolve_combat takes its result.
    [[nodiscard]] const std::optional<CampaignEncounter> &pending_encounter() const noexcept
    {
        return encounter_;
    }

    bool resolve_combat(const rules::Snapshot &outcome);

    // The 3D view where the party stands, from the area's wall sets.
    [[nodiscard]] Image observe_view();

    // Events the session cannot run yet, by kind, in the order met.
    [[nodiscard]] const std::vector<std::string> &diagnostics() const noexcept
    {
        return diagnostics_;
    }

    // A quest flag (the kit's 1-based flags, 0 to 255 values).
    [[nodiscard]] std::uint8_t quest_flag(unsigned number) const;

    // Puts the party on a square of an area, facing `facing`, without running
    // its event: for tools and tests that walk a design, as the kit's own
    // test mode does. Throws for a square outside the area.
    void place(unsigned area, unsigned x, unsigned y, por::MapDirection facing);
    // Runs the event of the square the party stands on, as Look does.
    void look();

    // Whether a step from the party's square in `direction` is open.
    [[nodiscard]] bool can_step(por::MapDirection direction) const;

  private:
    UaSession(DesignFiles files, GameSettings game);

    // A step of the event loop waits for this answer.
    enum class Wait : std::uint8_t
    {
        nothing,
        words,
        choice,
        typing,
        combat,
    };

    struct Pending
    {
        Wait wait{Wait::nothing};
        // Called with the choice made (0 for Return) or the typed text.
        std::function<void(std::size_t, std::string_view)> resume;
    };

    void load_area(unsigned number);
    [[nodiscard]] EventRecord record(unsigned number) const;
    [[nodiscard]] unsigned square_event(unsigned x, unsigned y) const;
    // Text pool slot `slot` (1-based; 0 is none), its first two carets the
    // selected member's name unless `names` is false.
    [[nodiscard]] std::string text(unsigned slot, bool names = true) const;

    // The event loop (the kit's events_run): event `n` and its chain, until
    // they end or one waits for the player.
    void run_events(unsigned n);
    void pump();
    [[nodiscard]] bool event_passes(unsigned n, const EventRecord &event);
    void run_kind(unsigned n, const EventRecord &event);
    // The kind chose event `next` to run next (0 none: its chain then).
    void finish_kind(unsigned next);
    void after_kind();
    void end_events();
    bool answer(std::uint64_t ticket, Wait wait, std::size_t choice, std::string_view typed);

    [[nodiscard]] bool done(unsigned n) const;
    void set_done(unsigned n, bool value);
    void push_event(unsigned n);
    [[nodiscard]] unsigned pop_event();

    // What an event shows, then what waits for the player.
    void say(std::string words);
    void wait_for_return(std::function<void()> then);
    void ask(std::vector<std::string> choices, std::function<void(std::size_t)> then);
    void ask_yes_no(std::function<void(bool)> then);
    void show_event_text(const EventRecord &event, std::function<void()> then);
    void unsupported(EventKind kind);

    // The event kinds.
    void text_event(const EventRecord &event);
    void show_texts(const EventRecord &event, unsigned from);
    void treasure_event(const EventRecord &event, bool now);
    void hand_over_treasure();
    void damage_event(const EventRecord &event);
    void damage_round(const EventRecord &event);
    void damage_member(const EventRecord &event, MemberId member);
    void transfer_event(const EventRecord &event, EventKind kind);
    void transfer(const EventRecord &event, EventKind kind);
    void menu_event(unsigned n, const EventRecord &event);
    void choose_branch(unsigned n, const EventRecord &event, unsigned way);
    void utilities_event(const EventRecord &event);
    void experience_event(const EventRecord &event);
    void quest_event(const EventRecord &event);
    void yes_no_event(unsigned n, const EventRecord &event);
    void flag_event(const EventRecord &event);
    void combat_event(const EventRecord &event, bool pick_one);
    void begin_combat(const EventRecord &event, bool pick_one);

    void move_party(unsigned x, unsigned y, por::MapDirection facing);
    void step_back();
    void advance_minutes(unsigned minutes);
    [[nodiscard]] unsigned hour() const;
    // Members who can still act, and every member.
    [[nodiscard]] std::vector<MemberId> active_members() const;
    [[nodiscard]] std::vector<MemberId> party_members() const;
    [[nodiscard]] bool party_has_class_group(unsigned group) const;
    [[nodiscard]] bool party_has_race(unsigned race) const;
    // 1 to `sides`, and 0 to 99, from the session's dice.
    [[nodiscard]] unsigned roll(unsigned sides);
    [[nodiscard]] unsigned roll_percent();
    void refresh_snapshot();

    DesignFiles files_;
    GameSettings game_;
    DesignArea area_;
    ViewArt view_art_;
    std::shared_ptr<CampaignParty> party_;
    por::TourSnapshot snapshot_;
    std::uint64_t next_ticket_{1};
    std::uint32_t dice_{0x5EED};
    std::vector<std::string> diagnostics_;

    // The saved state the events read and write: quest flags and the done
    // bits of every area's events (area * 100 + event - 1).
    std::array<std::uint8_t, 256> quest_flags_{};
    std::vector<bool> done_bits_ = std::vector<bool>(64 * 100, false);
    // Where a step back takes the party: the square it stepped from.
    std::optional<por::PartyPose> step_back_pose_;
    bool looking_{};
    // The start square's event has run.
    bool started_{};
    // The clock while no party plays (the party keeps it otherwise).
    std::uint64_t clock_minutes_{};

    // The event loop's state between steps.
    std::uint8_t loop_step_{};
    unsigned current_{}, chained_event_{};
    bool running_{}, chain_due_{}, chain_allowed_{true}, party_moved_{}, moved_last_pass_{};
    bool rerun_square_{}, step_back_due_{}, again_{};
    // No member can act any more, or an event ended the adventure.
    bool party_lost_{};
    std::vector<unsigned> event_stack_;
    Pending pending_;
    std::optional<CampaignEncounter> encounter_;
    // The monsters of the pending fight, for its experience.
    unsigned encounter_experience_{};
    // The fight's loss ends the adventure (an event may say it does not).
    bool party_can_lose_{true};
    // What a treasure event put in the pile: money (the Purse's places) and
    // ITEM.DAT numbers.
    std::array<unsigned, 7> treasure_wealth_{};
    std::vector<unsigned> treasure_items_;
    std::vector<std::string> tavern_tales_;
};

} // namespace opengold::ua

#endif
