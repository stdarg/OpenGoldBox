#ifndef OPENGOLD_ROLF_TOUR_H
#define OPENGOLD_ROLF_TOUR_H

#include "opengold/ecl_machine.h"
#include "opengold/map_catalog.h"
#include "opengold/formats.h"
#include "opengold/wall_art.h"
#include "opengold/creature_catalog.h"
#include "opengold/campaign_party.h"
#include "opengold/combat_demo.h"
#include "opengold/encounter_budget.h"
#include <bitset>

namespace opengold
{
struct SaveCodec;
}

namespace opengold::por
{
struct PartyPose
{
    unsigned x{}, y{}, facing{}; // GEO coordinates, 0=N, 1=E, 2=S, 3=W.
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
    std::array<std::uint16_t, 7> wealth{0, 0, 0, 9999, 0, 0, 0};
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
    bool number_input{};
    std::uint64_t picture_revision{};
    std::bitset<256> visited;
    std::bitset<256> seen; // Persistent map knowledge: visited or visible in a shown 3D view.
    std::vector<CoinPayment> payments; // This event's payments; cleared when the next begins.
    // This step's checks against a locked door and whether it opened.
    std::vector<DoorCheck> door_checks;
    bool door_opened{};
};
enum class ExplorationCommand
{
    turn_left,
    turn_right,
    turn_around,
    forward,
    look,
    camp
};

// Original tour followed by the New Phlan movement/search/script scheduler.
// Bound VM position cells are authoritative; snapshots are read-only views.
class RolfTourSession
{
  public:
    [[nodiscard]] static RolfTourSession load(const std::filesystem::path &directory);
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

    [[nodiscard]] std::uint16_t script_variable(std::uint16_t address) const
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
    friend struct opengold::SaveCodec;
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
    TownParty party_, saved_party_;
    std::shared_ptr<opengold::CampaignParty> campaign_;
    std::optional<opengold::PartyState> saved_campaign_;
    std::uint64_t who_request_{}, temple_request_{};
    // A DAMAGE volley waits here until its results are acknowledged.
    std::uint64_t damage_request_{};
    std::vector<opengold::MemberId> temple_targets_;
    std::vector<unsigned> who_slots_;
    // How a pre-combat meeting ended without a fight, said when its event ends.
    std::string encounter_outcome_;
    // Whether this event's script asked which member acts (WHO).
    bool member_chosen_{};
    std::vector<Equipment> treasure_;
    std::optional<Image> picture_;
    std::optional<EclMachine> checkpoint_;
    std::vector<std::string> diagnostics_;
    std::optional<ExplorationCommand> pending_movement_;
    unsigned current_script_{}, saved_script_{}, selected_character_{};
    unsigned current_area_{}, saved_area_{};
    std::map<unsigned, std::bitset<256>> visited_areas_;
    std::map<unsigned, std::bitset<256>> saved_visited_areas_;
    std::map<unsigned, std::bitset<256>> seen_areas_, saved_seen_areas_;
    std::optional<TourSnapshot> saved_snapshot_;
    std::vector<rules::Participant> staged_enemies_;
    std::vector<opengold::CombatArt> staged_art_;
    std::vector<unsigned> staged_records_;
    std::optional<EclRequest> encounter_menu_;
    unsigned encounter_distance_{};
    std::optional<opengold::CampaignEncounter> encounter_;
    std::optional<unsigned> monster_picture_id_; // SETUP MONSTER's PIC record.
    bool showing_monster_picture_{};
    std::uint64_t combat_request_{};
    unsigned encounter_challenge_{opengold::default_encounter_challenge};
    // The original encounter before fitting: its experience and loot are awarded.
    std::vector<unsigned> encounter_records_;
    std::vector<PendingLoot> pending_loot_, saved_pending_loot_;
    // Money from a non-shop TREASURE, awarded by the COMBAT that follows it.
    std::optional<PendingLoot> staged_treasure_;
    // Doors forced open stay open until the district map is reloaded.
    GeoMap saved_map_;
    bool door_menu_{};
    // As in the original, one Pick per lock until the party moves again.
    bool pick_tried_{};
    std::vector<opengold::DoorMethod> door_choices_;
    void claim_loot();
    void stage_treasure(const EclRequest &request);
    void award_staged_treasure();
    [[nodiscard]] std::string treasure_identity(std::uint16_t address) const;
    [[nodiscard]] bool locked_door_ahead() const;
    void show_locked_door();
    void try_locked_door(opengold::DoorMethod method);
    void search_destination();
    // The coins and items of an encounter's original creature records.
    [[nodiscard]] PendingLoot encounter_loot(unsigned area, std::vector<unsigned> records,
            std::string reward,
                                         bool items) const;
    void show_encounter_menu();
    void fit_staged_encounter();
    void announce_spokesman();
    bool choose_encounter(std::size_t choice);
    [[nodiscard]] const PhlanResources &area_resources() const;
    void change_area(unsigned id);
    unsigned saved_selected_character_{};
    unsigned
    event_stage_{}; // 0 tour, 1 before step, 2 search, 3 area entry, 4 pre-camp, 5 interrupted.
    RestKind camp_kind_{RestKind::long_rest};
    // Five-minute rest steps since the last interruption check. Like the
    // original engine's counter, it carries over between rests; it is saved.
    unsigned rest_checks_{}, saved_rest_checks_{};
    [[nodiscard]] std::optional<unsigned> rest_interruption(unsigned interval, unsigned chance);
    void announce_camp_attack();
    bool transition_{}, message_only_{};
    std::uint64_t shop_request_{};
    void configure_town();
    void synchronize_clock();
    [[nodiscard]] EclHostReply clock_reply() const;
    void begin_event(unsigned slot);
    void finish_event();
    bool handle_town_host(const EclRequest &request);
    // Scripts change coins relative to the view they were given, so the script
    // must receive a fresh character_reply after every read before it continues.
    void read_character();
    [[nodiscard]] EclHostReply character_reply(unsigned index) const;
    void bind_pose(PartyPose pose);
    bool move_party(ExplorationCommand command);
    void notice(std::string message);
    // Each arrow attacks a random conscious active member; returns a report line for each.
    [[nodiscard]] std::string shoot_arrows(unsigned arrows, const rules::HazardAttack &arrow);
    void publish_pose();
    void handle_host(const EclRequest &request);
    void fail(std::string diagnostic);
};
} // namespace opengold::por
#endif
