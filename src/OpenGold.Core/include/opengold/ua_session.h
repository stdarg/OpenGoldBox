#ifndef OPENGOLD_UA_SESSION_H
#define OPENGOLD_UA_SESSION_H

#include "opengold/campaign_encounter.h"
#include "opengold/campaign_party.h"
#include "opengold/exploration_session.h"
#include "opengold/original.h"
#include "opengold/original_combat.h"
#include "opengold/rolf_tour.h"
#include "opengold/ua_combat.h"
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
// to the view (ExplorationSession): the same TourSnapshot (where the party stands, what is said,
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

// The design's items (ITEM.DAT, numbered from 1) and their types (ITEMS.DAT),
// the design's own files or the kit's. Throws when either cannot be read.
struct DesignItems
{
    std::vector<ItemRecord> items;
    std::vector<por::ItemTemplate> types;
};

[[nodiscard]] DesignItems load_design_items(const DesignFiles &files);

// Item `number` as a member carries it; nullopt for no item (0, past the
// list, or a record the kit leaves unused). An identified item shows every
// name word.
[[nodiscard]] std::optional<por::Equipment> design_item(const DesignItems &items, unsigned number,
                                                        bool identified);

// The items a MONST record carries (its tail) as the kit loads them
// (saveload.c, mon_load): unidentified, with their charges, each readied in
// turn where the creature may ready it. A scroll (FFh) is made where it is
// handed over, not here.
[[nodiscard]] std::vector<por::Equipment> monster_items(const Monster &monster,
                                                        const DesignItems &items);

// The numbers a MONST record fights with under the original rules: its
// record and readied items as the original rules recalculate a creature
// (original::combatant_from_record), its size its combat icon's.
[[nodiscard]] original::Combatant
monster_combatant(const Monster &monster, const DesignItems &items, rules::Footprint icon);

// The kit's price of `price` at one of its rates, 0 to 19 (temple.c,
// rate_price): what shops, temples and training halls ask, in platinum.
[[nodiscard]] unsigned rated_price(unsigned price, unsigned rate);

// What a new character of the design starts with by its game record:
// experience and platinum go to creation (original::design_character_rules).
[[nodiscard]] original::StartingStock starting_stock(const GameSettings &game);

// The items a new character starts with (create.c's table): the game
// record's set (1 to 8; 0 none) for its class group, fighters, paladins and
// rangers first, then clerics, thieves and magic-users.
[[nodiscard]] std::vector<unsigned> starting_items(unsigned set,
                                                   const rules::CharacterSheet &sheet);

// A new member made for the design gets the rest of its start: the game
// record's gems, jewelry and equipment, readied where it can be.
void outfit_new_character(CampaignParty &party, MemberId id, const DesignFiles &files);

class UaSession final : public ExplorationSession
{
  public:
    // The design's start: the game record's area and entry point. Throws
    // when the design has no game record or its start area.
    [[nodiscard]] static UaSession start(DesignFiles files);

    // The party plays the design; its members' fights run through `party`'s
    // rules. The start square's event runs once the party is there.
    void campaign_party(std::shared_ptr<CampaignParty> party) override;

    [[nodiscard]] const por::TourSnapshot &snapshot() const noexcept override
    {
        return snapshot_;
    }

    // The design from its start again, its flags cleared; campaign_party
    // then runs the start square's event.
    void restart() override;

    // Nothing in a design moves by itself between the player's answers.
    void advance(double) override
    {
    }

    void set_searching(bool searching) override
    {
        snapshot_.searching = searching;
    }

    [[nodiscard]] bool can_leave() const override
    {
        return snapshot_.phase == por::TourPhase::completed;
    }

    [[nodiscard]] bool can_select_member() const override
    {
        return can_leave();
    }

    // No member turns on the party in a design yet.
    [[nodiscard]] bool turns_on_party(MemberId) const override
    {
        return false;
    }

    // The 3D view in a dungeon; nothing on the overland, which is not drawn
    // yet (docs/games/ua.md, What remains).
    [[nodiscard]] std::optional<Image> view_picture() override;
    // The area's name as the designer wrote it.
    [[nodiscard]] std::string area_name() const override;

    [[nodiscard]] por::RolfTourSession *dax() noexcept override
    {
        return nullptr;
    }

    [[nodiscard]] const por::RolfTourSession *dax() const noexcept override
    {
        return nullptr;
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
    bool explore(por::ExplorationCommand command) override;
    // Answers to what an event waits for, with the snapshot's ticket.
    bool continue_dialogue(std::uint64_t ticket);
    bool choose(std::uint64_t ticket, std::size_t choice) override;
    bool input(std::uint64_t ticket, std::string_view value) override;

    // The fight an event started, until resolve_combat takes its result.
    [[nodiscard]] const std::optional<CampaignEncounter> &
    pending_encounter() const noexcept override
    {
        return encounter_;
    }

    bool resolve_combat(const rules::Snapshot &outcome) override;
    // The fight could not be set up: the adventure stops there, saying why.
    bool reject_combat(std::string diagnostic) override;

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
    // The kit's own words and choices, which the view translates.
    void say_kit(rules::Message message);
    void ask_kit(std::vector<rules::Message> choices, std::function<void(std::size_t)> then);
    void type_answer(bool number, std::function<void(std::string_view)> then);
    void pick_member(std::function<void(MemberId)> then);
    [[nodiscard]] std::string member_name(MemberId id) const;
    [[nodiscard]] bool member_active(MemberId id) const;
    // The party's purse pays a price in platinum; false when it cannot.
    bool pay_platinum(unsigned amount);
    [[nodiscard]] std::uint32_t platinum_in_gold(unsigned amount) const;
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
    [[nodiscard]] std::optional<por::Equipment> random_scroll();
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

    // The services (ua_services.cpp): each ends by calling its ServiceEnd
    // with the event it chose, or the small town's menu takes over again.
    using ServiceEnd = std::function<void(unsigned)>;
    [[nodiscard]] ServiceEnd service_end(const EventRecord &event);
    void training_event(const EventRecord &event, bool in_town, ServiceEnd end);
    void hall_menu(const EventRecord &event, ServiceEnd end);
    void train_member(const EventRecord &event, ServiceEnd end, unsigned fee, MemberId id);
    void tavern_event(const EventRecord &event, bool in_town, ServiceEnd end);
    void tavern_menu(const EventRecord &event, bool in_town, ServiceEnd end);
    void drink(const EventRecord &event, bool in_town, ServiceEnd end);
    void shop_event(const EventRecord &event, bool in_town, ServiceEnd end);
    void shop_menu(const EventRecord &event, ServiceEnd end);
    void shop_buy(const EventRecord &event, ServiceEnd end);
    void shop_sell(const EventRecord &event, ServiceEnd end, bool identify);
    void temple_event(const EventRecord &event, bool in_town, ServiceEnd end);
    void temple_menu(const EventRecord &event, bool in_town, ServiceEnd end);
    void temple_heal(const EventRecord &event, bool in_town, ServiceEnd end);
    void cure(rules::TempleService service, unsigned price, MemberId id);
    void donate(const EventRecord &event, bool in_town, ServiceEnd end);
    void vault_event(const EventRecord &event, bool in_town, ServiceEnd end);
    void vault_menu(ServiceEnd end);
    void inn_event(const EventRecord &event, bool in_town, ServiceEnd end);
    void town_event(const EventRecord &event);
    void tour_event(const EventRecord &event);
    void tour_step(const EventRecord &event, unsigned step);
    [[nodiscard]] std::optional<Character> npc_character(unsigned number) const;
    [[nodiscard]] static std::string_view hall_class_id(std::size_t slot);
    [[nodiscard]] MemberId npc_member(unsigned number) const;
    void add_npc_event(const EventRecord &event);
    void remove_npc_event(const EventRecord &event);
    void encounter_event(const EventRecord &event);
    void encounter_turn(const EventRecord &event);
    void encounter_action(const EventRecord &event, unsigned action);
    void outcome(const EventRecord &event, bool passed);
    void who_tries_event(const EventRecord &event);
    void try_once(const EventRecord &event);
    [[nodiscard]] bool member_passes(const EventRecord &event, MemberId id);
    void who_pays_event(const EventRecord &event);
    void password_event(const EventRecord &event);
    void ask_password(const EventRecord &event);
    void begin_combat(const EventRecord &event, bool pick_one);
    [[nodiscard]] FightField fight_field(const EventRecord &event);
    [[nodiscard]] unsigned fight_distance(const EventRecord &event) const;

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
    DesignItems items_;
    CombatIcons icons_;
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
    bool treasure_identified_{};
    // A tavern's drinks this visit and the next tale it tells in order.
    unsigned drunkenness_{}, tavern_tale_next_{};
    // The open shop's ITEM.DAT numbers.
    std::vector<unsigned> shop_stock_;
    // Platinum given to temples toward a tithe's goal, kept for the game.
    std::uint32_t donated_{};
    // The vault's platinum, gems and jewelry, kept for the game.
    std::array<std::uint32_t, 3> vault_{};
    // A guided tour's two texts, once each at their step.
    std::array<bool, 2> tour_text_shown_{};
    // An encounter's range, and the surprise the fight it leads to takes.
    unsigned encounter_range_{};
    std::optional<Surprise> encounter_surprise_;
    // Who tries and a password: the tries left.
    unsigned tries_left_{};
    std::vector<std::string> tavern_tales_;
};

} // namespace opengold::ua

#endif
