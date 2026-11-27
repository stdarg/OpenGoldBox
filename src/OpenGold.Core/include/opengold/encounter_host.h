#ifndef OPENGOLD_ENCOUNTER_HOST_H
#define OPENGOLD_ENCOUNTER_HOST_H

// The tour's encounters (encounter_host.cpp; the Buck Rogers games' fights in
// buck_rogers_host.cpp): the monsters a script stages, the original's
// encounter menu, the fight handed to combat and its end. It reaches the tour
// through TourHost (tour_host.h), lent for each call.

#include "opengold/campaign_encounter.h"
#include "opengold/campaign_party.h"
#include "opengold/ecl_machine.h"
#include "opengold/encounter_data.h"
#include "opengold/formats.h"
#include "opengold/monster_bundle.h"
#include "opengold/tour_snapshot.h"
#include "opengold/treasure_service.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace opengold::por
{
struct PhlanResources;
class TourHost;

// One answer at the original's encounter menu (eclvm.pas EclEncounter):
// `result` is what the script receives (0 the monsters leave, 1 a fight, 2
// the party gets away, 3 a parley), or nothing while the meeting goes on; then
// `closer` says whether the monsters came a step closer or both sides waited.
struct EncounterTurn
{
    std::optional<unsigned> result;
    bool closer{};
};

// The original's table for the script's `response` (0 to 4) to the party's
// `choice` (0 Fight, 1 Wait, 2 Flee, 3 Advance, 4 Parley) with the monsters
// `distance` steps away. `party_escapes` when its slowest member keeps up with
// the meeting's flight threshold; `monsters_outrun` when the monsters are
// faster than the party's fastest member.
[[nodiscard]] EncounterTurn encounter_turn(std::size_t choice, unsigned response, unsigned distance,
                                           bool party_escapes, bool monsters_outrun);

class EncounterHost
{
  public:
    // The fight waiting for combat, while the monster close-up is not up
    // (RolfTourSession::pending_encounter).
    [[nodiscard]] const std::optional<CampaignEncounter> &pending_encounter() const
    {
        static const std::optional<opengold::CampaignEncounter> not_ready;
        // Combat waits until the player dismisses the monster close-up.
        return showing_monster_picture_ ? not_ready : encounter_;
    }

    // The monster close-up that loops before combat, or null when none is showing.
    [[nodiscard]] const std::vector<AnimationFrame> *monster_picture(const TourHost &tour) const;

    [[nodiscard]] bool showing_monster_picture() const noexcept
    {
        return showing_monster_picture_;
    }

    // SETUP MONSTER's PIC record.
    [[nodiscard]] std::optional<unsigned> monster_picture_id() const noexcept
    {
        return monster_picture_id_;
    }

    // The player pressed a key on the close-up; returns false when none is showing.
    bool start_encounter(TourHost &tour);
    bool resolve_combat(TourHost &tour, const rules::Snapshot &result);
    // Reject a pending handoff before combat starts, using the event rollback path.
    bool reject_combat(TourHost &tour, std::string diagnostic);

    // The script's encounter commands: SETUP MONSTER, LOAD MONSTER and the
    // original's encounter menu (its 29h), whose choices choose_encounter
    // answers.
    void setup_monster(TourHost &tour, const EclRequest &request);
    void load_monster(TourHost &tour, const EclRequest &request);
    void open_menu(TourHost &tour, const EclRequest &request);

    [[nodiscard]] bool menu_open() const noexcept
    {
        return encounter_menu_.has_value();
    }

    bool choose_encounter(TourHost &tour, std::size_t choice);
    // COMBAT with monsters staged (or the arena's copy): the fight is handed
    // to combat. False when no one is staged.
    [[nodiscard]] bool fight(TourHost &tour, const EclRequest &request);
    // The Buck Rogers games' LOAD MONSTER: copies of a MON0CHA creature with
    // its items, drawn with a CPIC icon (eclvm.pas EclLoadMonster).
    void stage_buck_rogers_creatures(TourHost &tour, const EclRequest &request);
    // Their COMBAT and SOLO COMBAT: the fight on the arena the area's kind
    // names, or with no one hostile, the fight's end at once (eclvm.pas
    // EclCombat).
    void buck_rogers_combat(TourHost &tour, const EclRequest &request, bool solo);

    [[nodiscard]] const std::vector<rules::Participant> &staged_enemies() const noexcept
    {
        return staged_enemies_;
    }

    // A Magic Circle keeps the staged enemies away: none of them fights.
    void ward_off();
    // The script's 1Ch clears what LOAD MONSTER staged.
    void forget_staged();
    // The monsters come a step closer (the tour's 0Dh).
    void approach() noexcept;
    // CALL 0x8001 before COMBAT: the member WHO chose fights alone; with
    // `mirror` (CALL 0x8000), the training hall's arena duel, against a copy
    // of itself.
    void duel(bool mirror) noexcept;

    // Whether the member fights for the monsters: one a script made hostile,
    // or a companion the original makes hostile as it joins.
    [[nodiscard]] bool turncoat(const TourHost &tour, MemberId id) const;
    // A script made the member hostile (the record's byte 0x10C set to
    // 0x81, as a vote to join Tyranthraxus does), or turned it back.
    void turn(MemberId id, bool hostile);

    // The experience a creature record of the area awards: its conversion's,
    // or under rules that play the original's records, the record's own.
    [[nodiscard]] unsigned record_experience(const TourHost &tour, unsigned record) const;
    // What the treasure a fight won is worth in experience: under rules that
    // play the original's records, its coins, gems, jewelry and magic; else
    // nothing.
    [[nodiscard]] unsigned treasure_experience(const TourHost &tour, const PendingLoot &loot) const;
    // Each member's share of a fight's experience: under rules that play the
    // original's records, the total divided among the party, as the original
    // does; otherwise the total, each member earning it.
    [[nodiscard]] unsigned experience_share(const TourHost &tour, unsigned total) const;
    // The export of the original creature `record` of `area`'s bank, if any.
    [[nodiscard]] static const ExportedMonster *
    exported_creature(const TourHost &tour, const PhlanResources &area, unsigned record);

    // An event begins: no meeting yet.
    void begin_event() noexcept;
    // The event ends: how a pre-combat meeting ended without a fight is said.
    void end_meeting(TourSnapshot &snapshot);

    // The party was robbed in the meeting; it did not get away.
    void party_robbed() noexcept
    {
        robbed_ = true;
    }

    // A new run of the tour, or an event rolled back: no staged monsters, no
    // menu, no fight.
    void reset();
    // A save is read: what no saved game keeps.
    void forget_for_load();

    // Pools of Darkness's to-hit cells, which its scripts set before a fight
    // and its end clears: cell 1 (monsterHitMod) is added to the monsters'
    // attack rolls, cell 2Fh (partyHitMod) to the party's side's
    // (fxengine.pas, AttackRoll; postcbt.pas).
    static constexpr EclAddress darkness_monster_hit_mod{0x01}, darkness_party_hit_mod{0x2F};
    // Pools of Darkness's cell 1Bh (breakLossPct): the encounter's morale,
    // which its monsters' side breaks below (combatai.pas, CheckMorale); and
    // cell 1Ch (partyMoveMod), added to the party's movement and cleared by
    // the fight's end (charhlp.pas, RecalcMovement; postcbt.pas).
    static constexpr EclAddress darkness_break_point{0x1B}, darkness_party_move_mod{0x1C};

  private:
    // What the members who fought for the monsters leave once a fight ends:
    // each leaves the party, and a beaten one's gear and its record's
    // experience go to the victors.
    struct TurncoatLoot
    {
        unsigned defeated{}, experience{};
        std::vector<Equipment> items;
    };

    TurncoatLoot part_with_turncoats(TourHost &tour, const rules::Snapshot &result);
    // Stages that copy as the fight's one enemy.
    void stage_mirror(TourHost &tour);
    void show_encounter_menu(TourHost &tour);
    [[nodiscard]] std::vector<unsigned> living_member_levels(const TourHost &tour) const;
    void fit_staged_encounter(TourHost &tour);
    // The override for this LOAD MONSTER of the current script, or null.
    [[nodiscard]] static const EncounterOverride *
    encounter_override(const TourHost &tour, const EclInstruction &instruction);
    void add_script_to_hit(TourHost &tour);
    // The fight's end (postcbt.pas PostCombat): the results, the wounded
    // and their treatment, experience, then the booty; the script goes on
    // once the booty is left.
    [[nodiscard]] bool resolve_buck_rogers_combat(TourHost &tour, const rules::Snapshot &result);
    void finish_buck_rogers_fight(TourHost &tour, std::uint64_t request,
                                  std::uint16_t combat_result, unsigned defeated);
    // After the fight on a boarded ship's decks (post-combat mode 2): the
    // repairs, the captured ship's fuel and the ship fight's experience.
    void repair_after_boarding(TourHost &tour, std::uint64_t request);

    std::vector<rules::Participant> staged_enemies_;
    std::vector<opengold::BandMember> staged_band_;
    std::vector<opengold::CombatArt> staged_art_;
    // Monsters a script fields on the party's side, and their art.
    std::vector<rules::Participant> staged_allies_;
    std::vector<opengold::CombatArt> staged_ally_art_;
    std::vector<unsigned> staged_records_;
    // Pools of Darkness: the LOAD MONSTERs since the last fight that brought
    // undead, each a try at turning them (rules::Encounter::turning_tries).
    unsigned staged_undead_loads_{};
    std::optional<EclRequest> encounter_menu_;
    unsigned encounter_distance_{};
    // The sides of the party SETUP MONSTER's monsters come from (Pools of
    // Darkness's and Treasures' fourth operand: bit 0 north, 1 east, 2 south,
    // 3 west), which the original keeps until the next SETUP MONSTER.
    unsigned monster_sides_{};
    std::optional<opengold::CampaignEncounter> encounter_;
    std::optional<unsigned> monster_picture_id_; // SETUP MONSTER's PIC record.
    // CALL 0x8001 before COMBAT: the member WHO chose fights alone.
    bool duel_{};
    // CALL 0x8000 before COMBAT: the training hall's arena duel, the member
    // WHO chose against a copy of itself.
    bool mirror_duel_{};
    // Members a script made hostile (the record's byte 0x10C set to 0x81, as
    // a vote to join Tyranthraxus does): in the event's next COMBAT they fight
    // for the monsters, and afterwards they leave the party.
    std::set<MemberId> turncoats_;
    bool showing_monster_picture_{};
    std::uint64_t combat_request_{};
    // The original encounter before fitting: its experience and loot are awarded.
    std::vector<unsigned> encounter_records_;
    // How a pre-combat meeting ended without a fight, said when its event ends.
    std::string encounter_outcome_;
    // Where the party met monsters this event, and whether it was robbed.
    std::optional<PartyPose> meeting_pose_;
    bool robbed_{};
};
} // namespace opengold::por
#endif
