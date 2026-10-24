#ifndef OPENGOLD_ROLF_TOUR_H
#define OPENGOLD_ROLF_TOUR_H

#include "opengold/ecl_machine.h"
#include "opengold/map_catalog.h"
#include "opengold/overland.h"
#include "opengold/formats.h"
#include "opengold/wall_art.h"
#include "opengold/wall_art_replacements.h"
#include "opengold/creature_catalog.h"
#include "opengold/campaign_party.h"
#include "opengold/campaign_encounter.h"
#include "opengold/difficulty.h"
#include "opengold/monster_bundle.h"
#include "opengold/encounter_budget.h"
#include "opengold/shop_data.h"
#include "opengold/temple_catalog.h"
#include <bitset>

namespace opengold
{
template <bool Reading> struct SaveCodec;

namespace test
{
struct TourAccess;
}
}

namespace opengold::por
{
struct PartyPose
{
    unsigned x{}, y{}; // GEO coordinates.
    MapDirection facing{MapDirection::north};
    auto operator<=>(const PartyPose &) const = default;
};
enum class TourPhase
{
    running,
    awaiting_continue,
    awaiting_input,
    shopping,
    completed,
    faulted,
    combat,
    defeated
};

struct TownParty
{
    std::string name{"Fighter"};
    unsigned level{1}, hit_points{12}, max_hit_points{12};
    Purse wealth{0, 0, 0, 9999, 0, 0, 0};
    std::vector<Equipment> inventory;
};

// Uncollected rewards. Encounter loot comes from defeated creature records;
// script treasure (TREASURE) has no records and carries only its money.
struct PendingLoot
{
    std::array<unsigned, 7> wealth{};
    std::vector<Equipment> items;
    std::string reward_id;
    std::vector<unsigned> records;
    bool include_items{true};
    unsigned area{}; // the district whose original creature records these are
};

// What a monster used of its gear in a fight (DIFF-1): its creature record and,
// by each item's place in the record's gear, the uses it has left.
struct SpentGear
{
    unsigned record{};
    std::vector<std::pair<std::size_t, unsigned>> uses_left;
};

// What an original creature record fights as: its combat creature, the XP of
// that stat block (which sizes encounters) and the experience it awards.
struct EncounterConversion
{
    std::string definition;
    unsigned fit_xp{}, award_xp{};
};

struct PhlanResources
{
    std::map<unsigned, std::shared_ptr<const EclProgram>> programs;
    std::map<unsigned, std::vector<Equipment>> treasure;
    // ITEMS templates, indexed by original item type, for generated treasure.
    std::vector<ItemTemplate> item_templates;
    std::vector<std::uint8_t> sprite_archive;
    std::map<unsigned, Image> heads, bodies, pictures;
    // PIC records: monster close-ups shown before combat, looping until a key press.
    std::map<unsigned, std::vector<AnimationFrame>> animations;
    // Explicit converted NPC profiles scoped to this resource bank. No guessed ID conversion.
    std::map<unsigned, opengold::Character> npc_profiles;
    // Immutable district profiles. Root owns districts; children never own root.
    std::map<unsigned, std::shared_ptr<const PhlanResources>> districts;
    std::optional<GeoMap> map;
    WallArtSet wall_art;
    std::map<unsigned, Creature> encounter_creatures;
    std::map<unsigned, EncounterConversion> conversions;
    // A district's script, archive bank and LOAD PIECES wall banks; zero for the town.
    unsigned script{}, bank{};
    std::array<unsigned, 3> pieces{};
    std::vector<std::uint8_t> combat_archive;
    std::vector<Image> terrain_art;
    // On the town only: the wilderness from START.EXE, SQRPACI's 256 overland
    // tiles, the party's overland icon and the wilderness battlefield art
    // (WILDCOM, then RANDCOM). Null without START.EXE.
    std::shared_ptr<const OverlandWorld> overland;
    std::vector<Image> overland_tiles;
    Image party_icon;
    std::vector<Image> wilderness_art;
    // The Combat Sprite Catalog's export (#274), on the town only: creatures'
    // names, gear and new art. Null without one.
    std::shared_ptr<const MonsterBundle> monsters;
};

// A script took coins the payer lacked and the purse made change.
struct CoinPayment
{
    std::string payer;
    opengold::CoinExchange coins;
};

// One check made against a locked door, for the player's log.
struct DoorCheck
{
    std::string member;
    opengold::DoorMethod method{};
    int die{}, total{}, difficulty{};
    bool thieves_tools{};
};

struct TourSnapshot
{
    PartyPose pose;
    TourPhase phase{TourPhase::running};
    std::string dialogue, diagnostic;
    std::uint64_t revision{}, continue_ticket{};
    unsigned prompts{}, redraws{}, footsteps{};
    int sprite_frame{-1}; // -1 hidden; native near/medium/far frames are 0/1/2.
    bool tour_finished{};
    unsigned script_id{}, event_runs{}, sprite_id{12};
    unsigned area_id{};
    std::vector<std::string> choices;
    // Dialogue and choices our campaign host writes (a training hall's
    // services), for the view to translate; empty while the script speaks.
    std::vector<opengold::rules::Message> host_dialogue, host_choices;
    bool number_input{};
    std::uint64_t picture_revision{};
    std::bitset<256> visited;
    std::bitset<256> seen; // Persistent map knowledge: visited or visible in a shown 3D view.
    std::vector<CoinPayment> payments; // This event's payments; cleared when the next begins.
    // This step's checks against a locked door and whether it opened.
    std::vector<DoorCheck> door_checks;
    bool door_opened{};
    // On the wilderness map: the party's square in its window's coordinates
    // and the direction it last moved, 0 north and on clockwise to 7.
    bool overland{};
    unsigned overland_x{}, overland_y{}, overland_direction{};
};
enum class ExplorationCommand
{
    turn_left,
    turn_right,
    turn_around,
    forward,
    look,
    camp,
    // On the wilderness map, a step in one of eight directions.
    north,
    northeast,
    east,
    southeast,
    south,
    southwest,
    west,
    northwest
};

// Original tour followed by the New Phlan movement/search/script scheduler.
// Bound VM position cells are authoritative; snapshots are read-only views.
class RolfTourSession
{
  public:
    // A session drives a shared party, so it is not copied by accident: a
    // copy would be a second session acting on the same party (Effective C++
    // Item 6). detached_copy() is an explicit copy without the party.
    RolfTourSession(RolfTourSession &&) = default;
    RolfTourSession &operator=(RolfTourSession &&) = default;
    ~RolfTourSession() = default;
    [[nodiscard]] RolfTourSession detached_copy() const;

    // New wall art replaces the original appearances it covers; exported
    // monsters bring their names, gear and art to the fights they are in.
    [[nodiscard]] static RolfTourSession
    load(const std::filesystem::path &directory, const WallArtReplacements &new_wall_art = {},
         std::shared_ptr<const MonsterBundle> monsters = {});
    // Also accepts wholly synthetic resources for asset-free host tests.
    RolfTourSession(GeoMap map, std::shared_ptr<const EclProgram> program,
                    std::array<opengold::Image, 3> sprites, std::uint32_t entry,
                    WallArtSet wall_art = {}, std::shared_ptr<const PhlanResources> town = {});
    void restart();
    void campaign_party(std::shared_ptr<opengold::CampaignParty> party);
    // Attach an already validated replacement without restarting its restored VM.
    void attach_restored_party(std::shared_ptr<opengold::CampaignParty> party);

    [[nodiscard]] bool can_leave() const
    {
        return snapshot_.phase == TourPhase::completed;
    }

    // Word of Recall from camp (LEVELS-3): cast on the square where the party
    // used a temple of New Phlan, it prepares the party's sanctuary there;
    // elsewhere it takes the whole party back to that sanctuary. Throws when
    // it can do neither, before spending anything.
    void recall(opengold::MemberId caster);

    [[nodiscard]] bool has_sanctuary() const
    {
        return sanctuary_.has_value();
    }

    // Outside events, and while shopping to choose the buyer. The shop script
    // receives whoever is selected when the shop closes.
    [[nodiscard]] bool can_select_member() const
    {
        return can_leave() || snapshot_.phase == TourPhase::shopping;
    }

    void advance(double seconds);
    bool continue_dialogue(std::uint64_t ticket);
    bool choose(std::uint64_t ticket, std::size_t choice);
    bool input(std::uint64_t ticket, std::string_view value);
    bool buy(std::uint64_t ticket, std::size_t item);
    bool leave_shop(std::uint64_t ticket);
    bool explore(ExplorationCommand command);
    // Both kinds run the original pre-camp and interruption services.
    bool camp(RestKind kind);
    // A percentage of the SRD's Moderate XP budget, 0-200. Original encounters
    // above the party's budget, or with more creatures than characters, shrink.
    void encounter_challenge(unsigned challenge);

    // On Hard and above, monsters bring their potions, wands and scrolls into
    // a fight and use them (DIFF-1), and temples cast no Resurrection
    // (TEMPLE-1).
    void difficulty(opengold::Difficulty difficulty) noexcept
    {
        difficulty_ = difficulty;
    }

    // The character creation rules the original's NPCs who join the party are
    // built with (Skullcrusher, CADORNA-1). Without them, ADD NPC accepts only
    // the profiles in the area's resources.
    void companion_rules(std::shared_ptr<const rules::CharacterRules> creation)
    {
        companion_rules_ = std::move(creation);
    }

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

    // The shops data/shops describes: each sells its record's items at its
    // prices. A shop it does not name sells the original's list. Throws when an
    // item's source list is not in the loaded game.
    void shops(std::vector<opengold::ShopSite> sites);

    [[nodiscard]] const std::vector<opengold::ShopSite> &shops() const noexcept
    {
        return shops_;
    }

    [[nodiscard]] const TourSnapshot &snapshot() const noexcept
    {
        return snapshot_;
    }

    [[nodiscard]] const GeoMap &map() const noexcept
    {
        return map_;
    }

    [[nodiscard]] const auto &sprites() const noexcept
    {
        return sprites_;
    }

    [[nodiscard]] const WallArtSet &wall_art() const noexcept
    {
        return wall_art_;
    }

    // Compose the current 3D view and remember its visible cells. Call only
    // when presenting exploration, not when an encounter picture replaces it.
    [[nodiscard]] Image observe_view();

    [[nodiscard]] const TownParty &party() const noexcept
    {
        return party_;
    }

    [[nodiscard]] const std::vector<Equipment> &shop_stock() const noexcept
    {
        return treasure_;
    }

    [[nodiscard]] const std::vector<std::string> &script_diagnostics() const noexcept
    {
        return diagnostics_;
    }

    [[nodiscard]] const std::optional<Image> &picture() const noexcept
    {
        return picture_;
    }

    [[nodiscard]] std::uint16_t script_variable(EclAddress address) const
    {
        return machine_.variable(address);
    }

    [[nodiscard]] const std::optional<opengold::CampaignEncounter> &pending_encounter() const
    {
        static const std::optional<opengold::CampaignEncounter> not_ready;
        // Combat waits until the player dismisses the monster close-up.
        return showing_monster_picture_ ? not_ready : encounter_;
    }

    // The monster close-up that loops before combat, or null when none is showing.
    [[nodiscard]] const std::vector<AnimationFrame> *monster_picture() const;
    // The player pressed a key on the close-up; returns false when none is showing.
    bool start_encounter();

    bool resolve_combat(const rules::Snapshot &outcome);
    // Reject a pending handoff before combat starts, using the event rollback path.
    bool reject_combat(std::string diagnostic);

  private:
    RolfTourSession(const RolfTourSession &) = default;
    RolfTourSession &operator=(const RolfTourSession &) = default;
    template <bool Reading> friend struct opengold::SaveCodec;
    // Tests set a flag the route cannot reach through the game (tests/tour_access.h).
    friend struct opengold::test::TourAccess;
    // Clears everything one run of the tour builds up; restart() also reloads
    // the town's map and a fresh script machine.
    void reset_run();
    GeoMap map_;
    std::shared_ptr<const EclProgram> program_;
    std::array<opengold::Image, 3> sprites_;
    WallArtSet wall_art_;
    EclMachine machine_;
    std::uint32_t entry_;
    TourSnapshot snapshot_;
    std::uint64_t next_ticket_{}, menu_request_{}, delayed_request_{};
    double remaining_delay_{};
    std::shared_ptr<const PhlanResources> town_;
    TownParty party_;
    std::shared_ptr<opengold::CampaignParty> campaign_;
    std::uint64_t who_request_{};
    // A DAMAGE volley waits here until its results are acknowledged.
    std::uint64_t damage_request_{};

    // A training hall's PROGRAM 0: the script waits on `request` while the
    // party buys sessions of experience and skill lessons from the list.
    struct HallChoice
    {
        enum class Kind
        {
            session,
            lessons,
            lesson,
            back,
            leave
        };
        Kind kind{};
        opengold::MemberId member{};
        std::string grant; // a lesson's skill or Expertise grant
    };

    struct HallVisit
    {
        std::uint64_t request{};
        opengold::TrainingHall hall{};
        opengold::MemberId member{}; // whose lessons are listed; 0 lists the services
        std::vector<HallChoice> choices;
    };

    std::optional<HallVisit> hall_visit_;
    void open_hall(std::uint64_t request);
    void show_hall(std::vector<opengold::rules::Message> notice);
    bool choose_at_hall(std::size_t choice);

    // A temple's COMBAT request with the healing flag: the script waits on
    // `request` while the party buys services from the temple's Heal list
    // (TEMPLE-1). The list names the services; choosing one lists the members
    // it would help, and choosing one of those asks to confirm the price.
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
    std::shared_ptr<const rules::CharacterRules> companion_rules_;
    std::vector<opengold::ShopSite> shops_;
    std::vector<std::vector<Equipment>> shop_stocks_; // each shop's items, as shops_
    [[nodiscard]] bool standing_at(const std::string &archive, unsigned record,
                                   const std::vector<std::pair<unsigned, unsigned>> &markers) const;
    std::optional<TempleVisit> temple_visit_;
    void open_temple(std::uint64_t request);
    void show_temple(std::vector<opengold::rules::Message> notice);
    bool choose_at_temple(std::size_t choice);
    std::vector<PartySlot> who_slots_;
    // How a pre-combat meeting ended without a fight, said when its event ends.
    std::string encounter_outcome_;
    // Whether this event's script asked which member acts (WHO).
    bool member_chosen_{};
    // Where the party met monsters this event, and whether it was robbed.
    std::optional<PartyPose> meeting_pose_;
    bool robbed_{};
    std::vector<Equipment> treasure_;
    std::optional<Image> picture_;

    // Everything an event can change, saved when it begins so a failed event
    // rolls back as a whole rather than member by hand-paired member
    // (Effective C++ Item 29).
    struct EventCheckpoint
    {
        EclMachine machine;
        TownParty party;
        std::optional<opengold::PartyState> campaign;
        unsigned script{}, area{};
        std::map<unsigned, std::bitset<256>> visited_areas, seen_areas;
        unsigned rest_checks{};
        unsigned travel_mode{};
        GeoMap map;
        std::vector<PendingLoot> pending_loot;
        TourSnapshot snapshot;
        PartySlot selected_character{};
    };

    std::optional<EventCheckpoint> event_checkpoint_;
    void roll_back_event();
    std::vector<std::string> diagnostics_;
    std::optional<ExplorationCommand> pending_movement_;
    unsigned current_script_{};
    PartySlot selected_character_{};
    unsigned current_area_{};
    // The original's travel mode: 1 in a 3D area, 2-4 on the west, central or
    // east window of the wilderness map, where the party's square is the
    // script's 0x49C3/0x49C4 and it steps in eight directions.
    unsigned travel_mode_{1};
    [[nodiscard]] bool overland() const noexcept
    {
        return travel_mode_ > 1;
    }
    void update_travel_mode();
    // Steps the party one wilderness square in its direction, unless the
    // terrain stops it; false when it stays.
    bool overland_step();
    std::map<unsigned, std::bitset<256>> visited_areas_;
    std::map<unsigned, std::bitset<256>> seen_areas_;
    std::vector<rules::Participant> staged_enemies_;
    std::vector<opengold::CombatArt> staged_art_;
    std::vector<unsigned> staged_records_;
    // A Magic Circle kept the camp's attackers away: the rest completes.
    bool rest_warded_{};
    std::optional<EclRequest> encounter_menu_;
    unsigned encounter_distance_{};
    std::optional<opengold::CampaignEncounter> encounter_;
    std::optional<unsigned> monster_picture_id_; // SETUP MONSTER's PIC record.
    // CALL 0x8001 before COMBAT: the member WHO chose fights alone.
    bool duel_{};
    // Whether Podol Plaza's script rolled a roaming group since the party
    // entered the area; a cancelled auction needs one (PODOL-1).
    bool plaza_group_rolled_{};
    void roll_plaza_group(const EclRequest &request, EclHostReply &reply);
    bool showing_monster_picture_{};
    std::uint64_t combat_request_{};
    unsigned encounter_challenge_{opengold::default_encounter_challenge};
    opengold::Difficulty difficulty_{opengold::default_difficulty};
    // The original encounter before fitting: its experience and loot are awarded.
    std::vector<unsigned> encounter_records_;
    std::vector<PendingLoot> pending_loot_;
    // Money from a non-shop TREASURE, awarded by the COMBAT that follows it.
    std::optional<PendingLoot> staged_treasure_;
    // Doors forced open stay open until the district map is reloaded; the
    // event checkpoint keeps the map as the event found it.
    bool door_menu_{};
    // As in the original, one Pick per lock until the party moves again.
    bool pick_tried_{};
    std::vector<opengold::DoorMethod> door_choices_;
    void claim_loot();
    void stage_treasure(const EclRequest &request);
    void award_staged_treasure();
    [[nodiscard]] std::string treasure_identity(std::uint16_t address) const;
    [[nodiscard]] bool locked_door_ahead() const;
    // Whether the lock ahead is a wizard lock (door code 3).
    [[nodiscard]] bool wizard_locked_ahead() const;
    void show_locked_door();
    void try_locked_door(opengold::DoorMethod method);
    void search_destination();
    // The coins and items of an encounter's original creature records, less
    // the gear the monsters used up.
    [[nodiscard]] PendingLoot encounter_loot(unsigned area, std::vector<unsigned> records,
                                             std::string reward, bool items,
                                             std::vector<SpentGear> spent = {}) const;
    void show_encounter_menu();
    void fit_staged_encounter();
    void announce_spokesman();
    bool choose_encounter(std::size_t choice);
    [[nodiscard]] const PhlanResources &area_resources() const;
    // The export of the original creature `record` of `area`'s bank, if any.
    [[nodiscard]] const ExportedMonster *exported_creature(const PhlanResources &area,
                                                           unsigned record) const;
    void change_area(unsigned id);

    // Which script of an exploration event is running, so finish_event knows
    // what follows it.
    enum class EventStage : std::uint8_t
    {
        none, // No event: the tour itself.
        step, // Before the party steps forward.
        search,
        area_entry,
        pre_camp,
        camp_interrupted
    };

    EventStage event_stage_{EventStage::none};
    RestKind camp_kind_{RestKind::long_rest};
    // Five-minute rest steps since the last interruption check. Like the
    // original engine's counter, it carries over between rests; it is saved.
    unsigned rest_checks_{};
    [[nodiscard]] std::optional<unsigned> rest_interruption(unsigned interval, unsigned chance);
    void announce_camp_attack();
    [[nodiscard]] bool camp_warded() const;
    bool transition_{}, message_only_{};

    // Word of Recall: the square where the party last used a temple of New
    // Phlan this session, and the sanctuary it prepared (saved).
    struct Sanctuary
    {
        unsigned script{};
        PartyPose pose;
        auto operator<=>(const Sanctuary &) const = default;
    };

    std::optional<Sanctuary> temple_square_, sanctuary_;
    std::uint64_t shop_request_{};
    void configure_town();
    void synchronize_clock();
    [[nodiscard]] EclHostReply clock_reply() const;

    // What the player did to start an event. Each value is the original script
    // entry slot that runs it.
    enum class EventTrigger : std::uint8_t
    {
        step = 0,
        look = 1,
        camp = 2
    };

    // `entry` starts another script slot than the trigger's, such as a town's entry.
    void begin_event(EventTrigger trigger, std::optional<std::size_t> entry = std::nullopt);
    void finish_event();
    bool handle_town_host(const EclRequest &request);
    // Scripts change coins relative to the view they were given, so the script
    // must receive a fresh character_reply after every read before it continues.
    void read_character();
    [[nodiscard]] EclHostReply character_reply(PartySlot slot) const;
    void bind_pose(PartyPose pose);
    bool move_party(ExplorationCommand command);
    void notice(std::string message);
    // Each arrow attacks a random conscious active member; returns a report line for each.
    [[nodiscard]] std::string shoot_arrows(unsigned arrows, const rules::HazardAttack &arrow);
    // The chosen member falls for dice d sides + bonus Bludgeoning; returns its report line.
    [[nodiscard]] std::string hurt(const std::optional<HazardHit> &hit) const;
    void publish_pose();
    void handle_host(const EclRequest &request);
    void fail(std::string diagnostic);
    // Set when a step leads to an area the game does not have yet: fail()
    // then rolls the step back with this notice instead of a diagnostic.
    std::string closed_way_;
    // The rune lines a PROTECTION scroll showed, kept above cleared text
    // until the next pause.
    std::string rune_scroll_;
};
} // namespace opengold::por
#endif
