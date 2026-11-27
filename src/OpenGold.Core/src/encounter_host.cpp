// The tour's encounters (encounter_host.h): the monsters a script stages, the
// original's encounter menu, the fight handed to combat and its end.
#include "opengold/encounter_host.h"
#include "opengold/tour_host.h"
#include "opengold/phlan_resources.h"
#include "opengold/vga_host.h"
#include "area_loading.h"
#include "host_services.h"
#include "dark_queen_campaign.h"
#include "opengold/original_rules.h"
#include "opengold/diagnostic_log.h"
#include "opengold/creature_art.h"
#include "opengold/encounter_budget.h"
#include "opengold/encounter_formation.h"
#include "opengold/game_traits.h"
#include <algorithm>
#include <limits>
#include <set>

namespace opengold::por
{
using namespace host_services;

namespace
{
// The script's surprise bits (0x6DCB). SURPRISE writes only 0 to 3, but a
// script may store any number there.
opengold::Surprise script_surprise(std::uint16_t bits)
{
    if (bits > static_cast<std::uint16_t>(opengold::Surprise::both))
    {
        throw EclError("Unsupported encounter surprise " + std::to_string(bits));
    }
    return static_cast<opengold::Surprise>(bits);
}
} // namespace

const EncounterOverride *EncounterHost::encounter_override(const TourHost &tour,
                                                           const EclInstruction &instruction)
{
    if (!tour.resources())
    {
        return nullptr;
    }
    const auto bank = tour.town_resources().script_banks.find(tour.current_script());
    if (bank == tour.town_resources().script_banks.end())
    {
        return nullptr;
    }
    const auto found = tour.town_resources().encounter_overrides.find(
        {bank->second, tour.current_script(), instruction.address});
    return found == tour.town_resources().encounter_overrides.end() ? nullptr : &found->second;
}

const ExportedMonster *EncounterHost::exported_creature(const TourHost &tour,
                                                        const PhlanResources &area, unsigned record)
{
    return tour.resources() && tour.town_resources().monsters
               ? tour.town_resources().monsters->creature(area.bank, record)
               : nullptr;
}

// The original makes creature record 24 hostile as it joins, whatever the
// area (eclvm.pas EclAddNpc): Valhingen Graveyard's mystic, who turns on the
// party and fights for the vampire.
constexpr unsigned joins_against_party = 24;

bool EncounterHost::turncoat(const TourHost &tour, MemberId id) const
{
    const auto &source = tour.campaign()->member(id).npc_source;
    return turncoats_.contains(id) ||
           (source.starts_with("por:MON") &&
            source.ends_with("CHA:" + std::to_string(joins_against_party)));
}

const std::vector<AnimationFrame> *EncounterHost::monster_picture(const TourHost &tour) const
{
    if (!showing_monster_picture_)
    {
        return nullptr;
    }
    return tour.pic_frames(*monster_picture_id_);
}

bool EncounterHost::start_encounter(TourHost &tour)
{
    if (tour.screen().phase != TourPhase::combat || !showing_monster_picture_)
    {
        return false;
    }
    showing_monster_picture_ = false;
    ++tour.screen().revision;
    return true;
}

bool EncounterHost::reject_combat(TourHost &tour, std::string diagnostic)
{
    if (tour.screen().phase != TourPhase::combat || !encounter_ || !combat_request_ ||
        !tour.campaign() || tour.campaign()->in_combat())
    {
        return false;
    }
    tour.fail(std::move(diagnostic));
    return true;
}

// The arena's copy has no creature record of its own.
constexpr unsigned mirror_record = std::numeric_limits<unsigned>::max();

void EncounterHost::stage_mirror(TourHost &tour)
{
    // The fight builds the copy from the duelist; here it gets its place
    // among the enemies and the original's combat icon 11.
    auto art = creature_art(tour.area_resources().combat_archive, 11, nullptr, false, tour.game());
    if (!art)
    {
        throw EclError("Invalid original combat icon");
    }
    art->entity = opengold::mirror_id;
    staged_enemies_.push_back({.id = opengold::mirror_id, .name = "Rolf"});
    staged_art_.push_back(std::move(*art));
    staged_records_.push_back(mirror_record);
}

unsigned EncounterHost::record_experience(const TourHost &tour, unsigned record) const
{
    if (!tour.plays_original_records())
    {
        return tour.area_resources().conversions.at(record).award_xp;
    }
    // The original's award: the record's own and so much for each of its Hit
    // Points (postcbt.pas, DetermineLootExp).
    const auto &stored = tour.area_resources().encounter_creatures.at(record).stored;
    return stored.base_xp_award + unsigned{stored.xp_per_hit_point} * stored.rolled_hit_points;
}

unsigned EncounterHost::treasure_experience(const TourHost &tour, const PendingLoot &loot) const
{
    if (!tour.plays_original_records())
    {
        return 0;
    }
    // The original's worth of the monsters' coins, gems and jewelry, and 400
    // for each plus of a magic item (postcbt.pas, DetermineLootExp).
    const auto &w = loot.wealth;
    unsigned experience = w[0] / 200 + w[1] / 20 + w[2] / 2 + w[3] +
                          w[4] * traits(tour.game()).gem_experience + w[5] * 250 + w[6] * 2200;
    for (const auto &item : loot.items)
    {
        if (item.stored.magic_bonus > 0)
        {
            experience += static_cast<unsigned>(item.stored.magic_bonus) * 400;
        }
    }
    return experience;
}

unsigned EncounterHost::experience_share(const TourHost &tour, unsigned total) const
{
    if (!tour.plays_original_records())
    {
        return total;
    }
    // The original divides a fight's experience among the party's members
    // (postcbt.pas, DetermineLootExp); the rules then share each member's
    // among its classes.
    const auto members =
        std::ranges::count_if(tour.campaign()->state().slots,
                              [&tour](MemberId id)
                              {
                                  if (!id)
                                  {
                                      return false;
                                  }
                                  if (tour.game() != GoldBoxGame::dark_queen_of_krynn)
                                  {
                                      return true;
                                  }
                                  const auto &member = tour.campaign()->member(id);
                                  return tour.campaign()->rule_module().receives_experience(
                                      member.character.sheet(), member.vitals);
                              });
    return members ? total / static_cast<unsigned>(members) : total;
}

EncounterHost::TurncoatLoot EncounterHost::part_with_turncoats(TourHost &tour,
                                                               const rules::Snapshot &result)
{
    TurncoatLoot loot;
    for (const auto id : encounter_->turncoats)
    {
        const auto unit = std::ranges::find(result.combatants, id, &rules::CombatantView::id);
        // Under the original rules a creature down may be below 0.
        const bool beaten = unit != result.combatants.end() && unit->hit_points <= 0;
        const auto &member = tour.campaign()->member(id);
        if (beaten && result.outcome == rules::Outcome::victory)
        {
            ++loot.defeated;
            for (const auto &item : member.character.inventory().items())
            {
                if (const auto source = member.item_sources.find(item.id);
                    source != member.item_sources.end())
                {
                    loot.items.push_back(source->second);
                }
            }
            // A companion's original record pays its experience, as the
            // monster it fought as.
            const auto &source = member.npc_source;
            const auto prefix = "por:MON" + std::to_string(tour.area_resources().bank) + "CHA:";
            if (source.starts_with(prefix))
            {
                const auto record = static_cast<unsigned>(std::stoul(source.substr(prefix.size())));
                if (tour.plays_original_records() ||
                    tour.area_resources().conversions.contains(record))
                {
                    loot.experience += record_experience(tour, record);
                }
            }
        }
        else if (beaten)
        {
            ++loot.defeated;
        }
        // The original takes every hostile creature off its list after a fight.
        if (beaten)
        {
            tour.campaign()->lose(id);
        }
        else
        {
            tour.campaign()->remove(id);
        }
        turncoats_.erase(id);
    }
    return loot;
}

namespace
{
// The DOS 1.1 finale reuses Xoham's monster-count cell as the index of its
// recovery loop, without initializing it. Recognize that exact loop before
// supplying its missing initial value with the final combat's reply.
bool darkness_recovery_uses_monster_count(const EclProgram &program)
{
    try
    {
        const auto load = program.instruction(0x95BF);
        const auto increment = program.instruction(0x95D0);
        return ecl_opcode(load.opcode, GoldBoxGame::pools_of_darkness).name == "LOAD CHARACTER" &&
               load.operands.size() == 1 && load.operands[0].tag == 1 &&
               load.operands[0].value == 0xC4 &&
               ecl_opcode(increment.opcode, GoldBoxGame::pools_of_darkness).name == "ADD" &&
               increment.operands.size() == 3 && increment.operands[0].tag == 0 &&
               increment.operands[0].value == 1 && increment.operands[1].tag == 1 &&
               increment.operands[1].value == 0xC4 && increment.operands[2].tag == 1 &&
               increment.operands[2].value == 0xC4;
    }
    catch (const EclError &)
    {
        return false;
    }
}
} // namespace

bool EncounterHost::resolve_combat(TourHost &tour, const rules::Snapshot &result)
{
    if (is_buck_rogers(tour.game()))
    {
        return resolve_buck_rogers_combat(tour, result);
    }
    if (diagnostic_log_enabled())
    {
        log_diagnostic(LogPriority::info, "fight",
                       "ends in area " + std::to_string(tour.screen().area_id) + ", outcome " +
                           std::to_string(static_cast<int>(result.outcome)));
    }
    if (tour.screen().phase != TourPhase::combat || !encounter_ || !combat_request_ ||
        !tour.campaign() || tour.campaign()->in_combat() ||
        result.outcome == rules::Outcome::ongoing || result.identity != tour.campaign()->identity())
    {
        return false;
    }
    unsigned defeated = 0;
    // Monsters that ran off the field, or that the original placement had no
    // room for (PLACE-1), are worth neither experience nor treasure.
    std::vector<unsigned> unpaid_records;
    std::set<rules::EntityId> enemies, party_ids;
    // Companions who turned on the party when one of its members attacked
    // its own side are its turncoats now, and leave it after the fight.
    if (result.allies_attacked)
    {
        for (const auto &unit : result.combatants)
        {
            const auto &slots = tour.campaign()->state().slots;
            if (unit.side == rules::Side::opposition &&
                std::ranges::find(slots, unit.id) != slots.end() &&
                std::ranges::find(encounter_->turncoats, unit.id) == encounter_->turncoats.end())
            {
                encounter_->turncoats.push_back(unit.id);
            }
        }
    }
    // In a duel only the champion fights; the rest of the party stands by.
    const auto champion = encounter_->champion;
    const auto &turned = encounter_->turncoats;
    std::set<rules::EntityId> expected_party, turncoats_seen;
    for (auto id : tour.campaign()->state().slots)
    {
        if (id && (!champion || id == champion) && std::ranges::find(turned, id) == turned.end())
        {
            expected_party.insert(id);
        }
    }
    for (const auto &unit : result.combatants)
    {
        // A member who fought for the monsters is beaten, or got away with them.
        if (unit.side == rules::Side::opposition &&
            std::ranges::find(turned, unit.id) != turned.end())
        {
            if (!turncoats_seen.insert(unit.id).second ||
                (result.outcome == rules::Outcome::victory && unit.hit_points > 0 && !unit.fled &&
                 !result.round_limit_reached))
            {
                return false;
            }
            continue;
        }
        // A creature that fought beside the party until it was attacked, or
        // until a spell charmed it onto the monsters' side (Treasures'
        // dwarves in Freezefire's lair).
        if (unit.side == rules::Side::opposition && opengold::is_ally(unit.id))
        {
            continue;
        }
        // A member charmed onto the monsters' side is still the party's.
        const bool charmed_member =
            unit.side == rules::Side::opposition && expected_party.contains(unit.id);
        if (unit.side == rules::Side::opposition && !charmed_member)
        {
            if (unit.id < 1000 || unit.id >= 1000 + staged_records_.size() ||
                !enemies.insert(unit.id).second)
            {
                return false;
            }
            // A monster that surrendered is beaten, and pays as the dead do
            // (postcbt.pas, DetermineLootExp); under the original rules one
            // down may be below 0 Hit Points, or unconscious with Hit Points
            // healed into it, which leave it out of the fight (fxengine.pas,
            // RestoreHitPoints). The rules' death state remains authoritative
            // when a lasting effect leaves positive Hit Points on a dead foe.
            if (unit.dead || unit.hit_points <= 0 || unit.surrendered ||
                unit.status == "unconscious")
            {
                ++defeated;
            }
            // One that fled pays nothing (postcbt.pas, DetermineLootExp), and
            // nor does one still standing when the fight ended for want of
            // attacks: it leaves the field.
            else if (unit.fled || result.round_limit_reached)
            {
                unpaid_records.push_back(staged_records_.at(unit.id - 1000));
            }
        }
        else if (unit.side == rules::Side::party && opengold::is_ally(unit.id))
        {
            continue;
        }
        // A monster still charmed onto the party's side as the fight ends
        // leaves as one that ran (the original's CombatDoneCleanup), worth
        // neither experience nor treasure.
        else if (unit.side == rules::Side::party && unit.id >= 1000 &&
                 unit.id < 1000 + staged_records_.size() && !expected_party.contains(unit.id))
        {
            if (!enemies.insert(unit.id).second)
            {
                return false;
            }
            unpaid_records.push_back(staged_records_.at(unit.id - 1000));
        }
        else if ((unit.side != rules::Side::party && !charmed_member) ||
                 !expected_party.contains(unit.id) || !party_ids.insert(unit.id).second ||
                 tour.campaign()->member(unit.id).vitals != unit.persistent)
        {
            return false;
        }
    }
    // Members who were already dead never joined the fight.
    for (auto id : expected_party)
    {
        if (!party_ids.contains(id) && !tour.campaign()->member(id).vitals.dead)
        {
            return false;
        }
    }
    if (enemies.empty() || (result.outcome == rules::Outcome::victory &&
                            defeated + unpaid_records.size() != enemies.size()))
    {
        return false;
    }
    for (std::size_t n = 0; n < staged_records_.size(); ++n)
    {
        if (!enemies.contains(static_cast<rules::EntityId>(1000 + n)))
        {
            unpaid_records.push_back(staged_records_[n]);
        }
    }
    // A lost duel is not the party's defeat: the champion is left with 1 Hit
    // Point and the script hears 128 (coab ovr006).
    const bool duel_lost = champion && result.outcome == rules::Outcome::defeat;
    // A fight won as the last member died leaves no one to go on: the party
    // is lost, as the original's game ends with its party dead.
    const bool none_left =
        std::ranges::none_of(tour.campaign()->state().slots,
                             [&tour](opengold::MemberId id)
                             {
                                 return id && !tour.campaign()->member(id).vitals.dead;
                             });
    const bool native_recovery = tour.game() == GoldBoxGame::dark_queen_of_krynn &&
                                 tour.script_machine().variable(EclAddress{0x1D}) != 0 &&
                                 !none_left;
    if ((result.outcome == rules::Outcome::defeat && !duel_lost && !native_recovery) ||
        (!champion && none_left))
    {
        tour.screen().phase = TourPhase::defeated;
        ++tour.screen().revision;
        return true;
    }
    const bool mirror = std::exchange(mirror_duel_, false);
    const bool first =
        tour.current_area() == 20 && encounter_records_ == std::vector<unsigned>{13, 4, 4, 4};
    const auto reward = first ? std::string("por:ECL2:20:search1:orcs:v1")
                              : "por:ECL" + std::to_string(tour.area_resources().bank) + ":" +
                                    std::to_string(tour.current_script()) +
                                    ":roaming:" + std::to_string(tour.new_ticket());
    const bool fled = result.outcome == rules::Outcome::fled;
    const bool native_lost = native_recovery && result.outcome == rules::Outcome::defeat;
    const bool pays_experience = tour.game() != GoldBoxGame::dark_queen_of_krynn ||
                                 tour.script_machine().variable(EclAddress{0x1F}) == 0;
    // Members are lost and rewards claimed before the original script hears the
    // result; any failure from here rolls the whole event back rather than
    // leaving those half applied (Effective C++ Item 29).
    try
    {
        // Those who fought for the monsters leave the party first, so they
        // share in nothing it wins.
        const auto turncoat_loot = part_with_turncoats(tour, result);
        defeated += turncoat_loot.defeated;
        if (fled)
        {
            // The party fled (the original's rule): members left on the field are
            // lost for good, and only the monsters it killed are worth experience.
            // The treasure stays with the monsters.
            tour.screen().dialogue = "Your party flees the battle.";
            // Original members left on the field are lost regardless of a
            // charm changing their side (Native postcmbt.c, hero_drop). Lent
            // allies and charmed encounter monsters are not original members.
            for (const auto id : expected_party)
            {
                const auto unit =
                    std::ranges::find(result.combatants, id, &rules::CombatantView::id);
                if (unit == result.combatants.end() || !unit->fled)
                {
                    const auto name = tour.campaign()->member(id).character.sheet().name;
                    tour.campaign()->lose(id);
                    tour.screen().dialogue += "\n" + name + " is left behind and lost.";
                }
            }
            // The script's chosen member is one still in the party.
            if (!tour.campaign()->state().slots.at(tour.selected_character().index))
            {
                tour.select_character(tour.campaign()->state().selected_slot);
            }
            unsigned experience = 0;
            for (const auto &unit : result.combatants)
            {
                if (unit.side == rules::Side::opposition && enemies.contains(unit.id) &&
                    unit.hit_points <= 0 && staged_records_.at(unit.id - 1000) != mirror_record)
                {
                    experience += record_experience(tour, staged_records_.at(unit.id - 1000));
                }
            }
            if (experience && pays_experience)
            {
                tour.campaign()->award_experience(experience_share(tour, experience), reward);
            }
        }
        else if (duel_lost || native_lost)
        {
            // No experience, and the opponent keeps its gear.
            if (duel_lost)
            {
                tour.campaign()->recover_duelist(champion);
            }
            tour.screen().dialogue = "You have lost the fight.";
        }
        else if (mirror)
        {
            // Beating its own copy earns the duelist 100 experience a level
            // and nothing else (postcbt.pas DetermineLootExp).
            if (pays_experience)
            {
                tour.campaign()->award_experience(
                    100 * static_cast<unsigned>(
                              tour.campaign()->member(champion).character.sheet().level),
                    reward, champion);
            }
        }
        else
        {
            // Experience and loot stay the original encounter's, however many fought,
            // less a record for each monster that got away or was left out.
            auto paying = encounter_records_;
            for (const auto record : unpaid_records)
            {
                if (const auto it = std::find(paying.begin(), paying.end(), record);
                    it != paying.end())
                {
                    paying.erase(it);
                }
            }
            // What the monsters still on the field used of their gear is gone
            // from the loot (DIFF-1).
            std::vector<SpentGear> spent;
            for (const auto &unit : result.combatants)
            {
                if (unit.side == rules::Side::opposition && !unit.fled &&
                    !unit.item_uses_left.empty())
                {
                    SpentGear gear{staged_records_.at(unit.id - 1000), {}};
                    for (const auto &[item, left] : unit.item_uses_left)
                    {
                        gear.uses_left.emplace_back(item - 1, left);
                    }
                    spent.push_back(std::move(gear));
                }
            }
            // Pool divides and applies prime-score bonuses once to the whole
            // post-combat award, including treasure supplied by the script.
            const bool combine_script_experience =
                tour.game() == GoldBoxGame::pool_of_radiance && tour.plays_original_records();
            const bool has_monster_loot = !paying.empty();
            unsigned experience = combine_script_experience && tour.treasure_service().staged()
                                      ? treasure_experience(tour, *tour.treasure_service().staged())
                                      : 0;
            // Monsters that all got away leave no monster loot to pay.
            if (!paying.empty())
            {
                for (auto record : paying)
                {
                    experience += record_experience(tour, record);
                }
                // Krynn's overland retains a dungeon map while its creatures
                // come from the root archive. Keep that source with unclaimed
                // loot as well, including after an ordinary save and reload.
                const auto loot_area = tour.in_curse_wilderness() ? 0 : tour.current_area();
                auto loot = tour.encounter_loot(
                    loot_area, std::move(paying), reward + ":loot",
                    tour.script_machine().variable(tour.game_cell(EclAddress{0x6DE3})) != 1,
                    std::move(spent));
                experience += treasure_experience(tour, loot);
                tour.treasure_service().add_loot(std::move(loot));
                tour.treasure_service().pile_loot(tour);
            }
            if (pays_experience && (has_monster_loot || experience))
            {
                tour.campaign()->award_experience(
                    champion ? experience : experience_share(tour, experience), reward, champion);
            }
            // Treasure the script added to the fight is won with it.
            if (tour.treasure_service().staged())
            {
                tour.award_staged_treasure(!combine_script_experience);
            }
            // A beaten turncoat's gear is the party's, and its record's
            // experience too.
            if (pays_experience && turncoat_loot.experience)
            {
                tour.campaign()->award_experience(turncoat_loot.experience, reward + ":turncoats");
            }
            for (const auto &item : turncoat_loot.items)
            {
                tour.treasure_service().add_to_pile(item);
            }
        }
        if (native_recovery && !fled)
        {
            for (const auto id : tour.campaign()->state().slots)
            {
                if (id)
                {
                    tour.campaign()->recover_duelist(id);
                }
            }
        }
        // The original's combat results (coab ovr006): 0 won, 128 (0x80) a
        // duel lost, 129 (0x81) the party fled; scripts test "above 128" for a
        // flight.
        std::vector<EclMemoryWrite> results;
        for (auto write : std::array<EclMemoryWrite, 8>{
                 {{tour.game_cell(ecl_combat_result), static_cast<std::uint16_t>(fled        ? 129
                                                                                 : duel_lost ? 128
                                                                                             : 0)},
                  {tour.game_cell(ecl_monsters_defeated), static_cast<std::uint16_t>(defeated)},
                  {tour.game_cell(ecl_encounter_surprise), 0},
                  {tour.game_cell(EclAddress{0x6DE3}), 0},
                  {tour.game_cell(EclAddress{0x6E70}), 0},
                  {tour.game_cell(EclAddress{0x6E71}), 0},
                  {tour.game_cell(EclAddress{0x6E72}), 0},
                  // Whether a member attacked its own side (Attack Ally),
                  // which the nomads hold against the party (ECL7:17 0x9ABA).
                  {tour.game_cell(ecl_allies_attacked),
                   static_cast<std::uint16_t>(result.allies_attacked)}}})
        {
            results.push_back(write);
        }
        const auto conditions =
            vga_combat_conditions(tour.game(), !fled && !duel_lost && !native_lost);
        if (tour.game() == GoldBoxGame::dark_queen_of_krynn)
        {
            // Native PostCombat clears its transient battle modifiers and
            // award/recovery flags, retaining anti-magic and underwater state.
            for (const int cell : {0x01, 0x2F, 0x1C, 0x1D, 0x1F})
            {
                results.push_back({EclAddress{static_cast<std::uint16_t>(cell)}, 0});
            }
        }
        // The party leaves a maze fight facing the side it ran off, which
        // the script reads to find the next part of the maze (postcbt.pas).
        if (result.escape_side)
        {
            results.push_back({tour.game_cell(ecl_party_facing),
                               static_cast<std::uint16_t>(*result.escape_side)});
        }
        // Pools of Darkness's fight's end clears its to-hit cells
        // (add_script_to_hit) and its party's movement modifier.
        if (tour.game() == GoldBoxGame::pools_of_darkness)
        {
            results.push_back({darkness_monster_hit_mod, 0});
            results.push_back({darkness_party_hit_mod, 0});
            results.push_back({darkness_party_move_mod, 0});
            const auto &request = tour.script_machine().pending_request();
            if (result.outcome == rules::Outcome::victory && tour.current_script() == 75 &&
                request && request->id == combat_request_ && request->instruction &&
                request->instruction->address == 0x94CB && tour.resources())
            {
                const auto program = tour.town_resources().programs.find(75);
                if (program != tour.town_resources().programs.end() &&
                    darkness_recovery_uses_monster_count(*program->second))
                {
                    results.push_back({EclAddress{0xC4}, 0});
                }
            }
        }
        // A Treasures crystal battle's crystal joins those the party holds
        // (cell 3Ch: 1 red, 2 green, 4 blue), and cell 3Eh tells the script
        // which it was (effhndlr.pas, EffRedCrystal).
        if (result.crystal)
        {
            constexpr EclAddress crystals_held{0x3C}, last_crystal{0x3E};
            results.push_back({crystals_held, static_cast<std::uint16_t>(
                                                  tour.script_machine().variable(crystals_held) |
                                                  1U << *result.crystal)});
            results.push_back({last_crystal, static_cast<std::uint16_t>(*result.crystal)});
        }
        // Treasures' romance: the loves as the fight moved them (cells 106h
        // and 107h; effhndlr.pas, EffInLove).
        if (result.romance)
        {
            results.push_back(
                {EclAddress{0x106}, static_cast<std::uint16_t>(result.romance->member_love)});
            results.push_back(
                {EclAddress{0x107}, static_cast<std::uint16_t>(result.romance->npc_love)});
        }
        // The items the monsters leave wait for the party to take them; the
        // script hears the result once the party leaves the treasure.
        if (!tour.treasure_service().treasure_pile().empty())
        {
            tour.treasure_service().open_treasure(tour, combat_request_, std::move(results),
                                                  conditions);
        }
        else
        {
            auto reply = tour.character_reply(tour.selected_character());
            reply.writes.insert(reply.writes.end(), results.begin(), results.end());
            reply.conditions = conditions;
            if (!tour.script_machine().resume_host(combat_request_, reply))
            {
                throw EclError("Combat result rejected by original script");
            }
        }
    }
    catch (const std::exception &error)
    {
        tour.fail(error.what());
        return true;
    }
    combat_request_ = 0;
    encounter_.reset();
    monster_picture_id_.reset();
    staged_enemies_.clear();
    staged_art_.clear();
    staged_records_.clear();
    staged_undead_loads_ = 0;
    staged_allies_.clear();
    staged_ally_art_.clear();
    // A later script fault must never restore pre-combat HP or erase a victory.
    tour.keep_event();
    if (!tour.treasure_service().pending_loot().empty())
    {
        tour.screen().dialogue += "\nLoot is retained until your party has room in its purses.";
    }
    if (tour.screen().phase == TourPhase::treasure)
    {
        return true;
    }
    tour.screen().phase = TourPhase::running;
    ++tour.screen().revision;
    tour.advance(0);
    return true;
}

EncounterTurn encounter_turn(std::size_t choice, unsigned response, unsigned distance,
                             bool party_escapes, bool monsters_outrun)
{
    constexpr std::size_t fight = 0, wait = 1, flee = 2, advance = 3, parley = 4;
    // Adjacent monsters answer with `adjacent`; others come a step closer.
    const auto close_in = [&](std::optional<unsigned> adjacent)
    {
        return distance == 0 ? EncounterTurn{adjacent, false} : EncounterTurn{std::nullopt, true};
    };
    const auto fight_or_flight = [&]
    {
        return EncounterTurn{choice == flee ? 2U : 1U, false};
    };
    switch (response)
    {
    case 0:
        // Only this answer tests the party's speed.
        return {choice == flee && party_escapes ? 2U : 1U, false};
    case 1:
        if (choice == wait)
        {
            return {};
        }
        if (choice == advance || choice == parley)
        {
            return close_in(choice == parley ? std::optional<unsigned>{3U} : std::nullopt);
        }
        return fight_or_flight();
    case 2:
        // The monsters leave, unless the party attacks monsters it can outrun.
        return {choice == fight && !monsters_outrun ? 1U : 0U, false};
    case 3:
        if (choice == wait || choice == advance)
        {
            return close_in(std::nullopt);
        }
        if (choice == parley)
        {
            return close_in(3U);
        }
        return fight_or_flight();
    case 4:
        if (choice == fight || choice == flee)
        {
            return fight_or_flight();
        }
        return close_in(3U);
    default:
        throw EclError("Invalid encounter response");
    }
}

void EncounterHost::show_encounter_menu(TourHost &tour)
{
    // The first operand names the distance sprite, the third the close-up
    // picture (coab CMD_EncounterMenu); Sokal Keep's Ferran Martinez has a
    // picture of his own (19) and the Spectre's sprite (17).
    // A sprite with fewer distance frames (the western wilderness's centaur
    // has two) shows its farthest one beyond them.
    const auto &args = encounter_menu_->arguments;
    for (unsigned frame = 0; frame < 3; ++frame)
    {
        auto image = decode_ega_sprite(tour.area_resources().sprite_archive,
                                       static_cast<std::uint8_t>(args[0].number()),
                                       static_cast<std::uint8_t>(frame));
        if (!image && frame > 0)
        {
            tour.encounter_sprites()[frame] = tour.encounter_sprites()[frame - 1];
            continue;
        }
        if (!image)
        {
            throw EclError("Unsupported roaming encounter sprite");
        }
        tour.encounter_sprites()[frame] = std::move(image.image);
    }
    tour.screen().sprite_id = args[0].number();
    tour.screen().sprite_frame = encounter_distance_;
    monster_picture_id_ = args[2].number();
    tour.screen().dialogue = join_script_text({}, args[9 + encounter_distance_].text());
    if (tour.screen().dialogue.empty())
    {
        tour.screen().dialogue = join_script_text({}, args[9].text());
    }
    tour.announce_camp_attack();
    if (!meeting_pose_)
    {
        meeting_pose_ = tour.screen().pose;
    }
    // As in the original, Parley replaces Advance once the monsters are
    // adjacent, and on the wilderness map, where no one advances.
    tour.screen().choices = {
        "Fight", "Wait", "Flee",
        encounter_distance_ && !tour.overland() && !tour.gateway_overland() ? "Advance" : "Parley"};
    tour.screen().phase = TourPhase::awaiting_continue;
    tour.screen().continue_ticket = tour.new_ticket();
    ++tour.screen().revision;
}

std::vector<unsigned> EncounterHost::living_member_levels(const TourHost &tour) const
{
    std::vector<unsigned> levels;
    for (const auto id : tour.campaign()->state().slots)
    {
        if (id && !tour.campaign()->member(id).vitals.dead)
        {
            levels.push_back(tour.campaign()->member(id).character.sheet().level);
        }
    }
    return levels;
}

// Shrinks the staged original encounter to the party's SRD XP budget at the
// encounter challenge and to one creature per living character, the size the
// party can face without being swarmed. Each LOAD MONSTER group keeps its first.
// Under the original rules the whole encounter fights, as in the original.
void EncounterHost::fit_staged_encounter(TourHost &tour)
{
    staged_band_.clear();
    std::vector<unsigned> counts(staged_records_.size(),
                                 static_cast<unsigned>(staged_records_.size()));
    if (!tour.plays_original_records())
    {
        const auto groups = staged_groups(staged_records_, tour.area_resources().conversions);
        counts = fit_encounter_to_difficulty(groups, living_member_levels(tour),
                                             tour.encounter_challenge(), tour.played_difficulty());
    }
    for (std::size_t n = 0; n < staged_records_.size() && !tour.plays_original_records(); ++n)
    {
        const auto record = staged_records_[n];
        const auto &creature = tour.area_resources().encounter_creatures.at(record);
        const auto *exported = exported_creature(tour, tour.area_resources(), record);
        auto member = staged_enemies_[n];
        member.name = exported ? exported->name : creature.stored.name;
        member.inventory = fighting_gear(creature, exported);
        staged_band_.push_back(
            {std::move(member), record, tour.area_resources().conversions.at(record).fit_xp});
    }
    std::vector<rules::Participant> enemies;
    std::vector<opengold::CombatArt> art;
    std::vector<unsigned> records;
    for (std::size_t n = 0, group = 0, kept = 0; n < staged_records_.size(); ++n)
    {
        if (n && staged_records_[n] != staged_records_[n - 1])
        {
            ++group;
            kept = 0;
        }
        if (kept++ >= counts[group])
        {
            continue;
        }
        const auto id = static_cast<rules::EntityId>(1000 + enemies.size());
        const auto &creature = tour.area_resources().encounter_creatures.at(staged_records_[n]);
        enemies.push_back(std::move(staged_enemies_[n]));
        enemies.back().id = id;
        const auto *exported = exported_creature(tour, tour.area_resources(), staged_records_[n]);
        enemies.back().name = (exported ? exported->name : creature.stored.name) + " " +
                              std::to_string(enemies.size());
        art.push_back(std::move(staged_art_[n]));
        art.back().entity = id;
        records.push_back(staged_records_[n]);
    }
    staged_enemies_ = std::move(enemies);
    staged_art_ = std::move(art);
    staged_records_ = std::move(records);
}

bool EncounterHost::choose_encounter(TourHost &tour, std::size_t choice)
{
    if (!encounter_menu_ || choice > 3)
    {
        return false;
    }
    // The fourth choice is Advance at a distance and Parley when adjacent or
    // on the wilderness map.
    if (choice == 3 && (encounter_distance_ == 0 || tour.overland() || tour.gateway_overland()))
    {
        choice = 4;
    }
    const auto &request = *encounter_menu_;
    const auto &args = request.arguments;
    const auto response = args[4 + choice].number();
    int slowest = 1000, fastest = 0;
    for (auto id : tour.campaign()->state().slots)
    {
        // The party carries its dead with it, as in the original, so they never slow it.
        if (id && !tour.campaign()->member(id).vitals.dead)
        {
            // Conversion boundary: original movement 12 corresponds to a modern
            // ordinary 30-foot creature. Unconscious members cannot escape on foot.
            const int movement = tour.campaign()->member(id).vitals.hit_points <= 0
                                     ? 0
                                     : tour.campaign()->profile(id).movement_feet * 2 / 5;
            slowest = std::min(slowest, movement);
            fastest = std::max(fastest, movement);
        }
    }
    // On Easy a flight always succeeds (DIFF-2).
    const auto turn = encounter_turn(choice, response, encounter_distance_,
                                     slowest >= static_cast<int>(args[12].number()) ||
                                         spares_the_party(tour.played_difficulty()),
                                     static_cast<int>(args[13].number()) > fastest);
    if (!turn.result)
    {
        // The meeting goes on: the monsters came a step closer, or both sides
        // waited.
        encounter_distance_ -= turn.closer ? 1 : 0;
        show_encounter_menu(tour);
        return true;
    }
    const auto result = turn.result;
    EclHostReply reply;
    reply.writes = {{args[3].address(), static_cast<std::uint16_t>(*result)}};
    if (!tour.script_machine().resume_host(request.id, reply))
    {
        return false;
    }
    // The script ends the meeting quietly (0); the event's end says so.
    encounter_outcome_ = *result == 0 ? "The monsters go on their way." : "";
    encounter_menu_.reset();
    tour.screen().choices.clear();
    tour.screen().continue_ticket = 0;
    tour.screen().phase = TourPhase::running;
    ++tour.screen().revision;
    return true;
}

void EncounterHost::setup_monster(TourHost &tour, const EclRequest &request)
{
    // Operand n as a number.
    const auto arg = [&](unsigned n)
    {
        return request.arguments.at(n).number();
    };
    // Encounter distance images use the already verified SPRIT3 format.
    std::array<Image, 3> decoded;
    const bool native = tour.game() == GoldBoxGame::dark_queen_of_krynn;
    // Matrix's Venus ambush requests three squares even though its
    // distance sprites and the ground encounter use at most two. Keep
    // the existing corridor limit below, as for the other Buck setups.
    const bool buck_distance_three = is_buck_rogers(tour.game()) && arg(1) == 3;
    if (arg(1) > 2 && !native && !buck_distance_three)
    {
        throw EclError("Unsupported encounter distance");
    }
    // Native limit 10h explicitly forces the farthest distance even
    // when the current corridor ends sooner (CMD_SetupMonster).
    encounter_distance_ =
        native && arg(1) == 0x10 ? 2U : std::min<unsigned>(arg(1), tour.room_ahead());
    // Pools of Darkness's and Treasures' SETUP MONSTER names the sides
    // the monsters come from too (eclvm.pas, EclSetupMonster).
    if (request.arguments.size() > 3)
    {
        monster_sides_ = arg(3) & 0xFFU;
    }
    // Champions' DisplayMonster loads distance sprites only in the 3D
    // view. Wilderness scripts deliberately name sprites absent from SPRIT.
    if (krynn_game(tour.game()) && tour.vga_area_map())
    {
        tour.encounter_sprites() = {};
        tour.screen().sprite_frame = -1;
        tour.screen().sprite_id = arg(0);
        monster_picture_id_.reset();
        return;
    }
    // Gateway's monsters have no distance sprites (255; it has no SPRIT
    // files): only the close-up shows, face to face (its DisplayMonster).
    constexpr unsigned no_sprite = 255;
    if (arg(0) == no_sprite)
    {
        tour.encounter_sprites() = {};
        tour.screen().sprite_frame = -1;
        tour.screen().sprite_id = arg(0);
        tour.shown_animation().reset();
        tour.shown_picture().reset();
        if (arg(2) == no_sprite)
        {
            monster_picture_id_.reset();
        }
        else
        {
            monster_picture_id_ = arg(2);
        }
        ++tour.screen().picture_revision;
        return;
    }
    // The 256-color engine's sprites are 256-color animations.
    const auto sprite = [&tour](unsigned id, unsigned distance)
    {
        if (const auto shared = tour.town_resources().shared_sprites.find(id);
            shared != tour.town_resources().shared_sprites.end() &&
            distance < shared->second.size())
        {
            return ImageDecodeResult{FormatResult::ok, shared->second[distance]};
        }
        return vga_art(tour.game()) ? VgaHost::vga_sprite(tour, id, distance)
                                    : decode_ega_sprite(tour.area_resources().sprite_archive,
                                                        static_cast<std::uint8_t>(id),
                                                        static_cast<std::uint8_t>(distance));
    };
    // The Pitt's buccaneer names sprite 19, which SPRIT1 lacks: met face
    // to face, the original shows the head (0x6DE1) and body portrait.
    const auto head_id = tour.script_machine().variable(tour.game_cell(EclAddress{0x6DE1}));
    if (!sprite(arg(0), 0) && encounter_distance_ == 0 && head_id != 255)
    {
        tour.screen().sprite_frame = -1;
        tour.screen().sprite_id = arg(0);
        monster_picture_id_.reset();
        tour.shown_animation().reset();
        tour.shown_picture() = close_up_portrait(tour.area_resources(), head_id, arg(2));
        ++tour.screen().picture_revision;
        return;
    }
    // A Buck Rogers sprite its disk's SPRIT file lacks loads nothing, and
    // nothing shows (portrait.pas LoadAnimation of an empty record).
    if (is_buck_rogers(tour.game()) && !sprite(arg(0), 0))
    {
        tour.encounter_sprites() = {};
        tour.screen().sprite_frame = -1;
        tour.screen().sprite_id = arg(0);
        monster_picture_id_ = arg(2);
        tour.shown_picture().reset();
        ++tour.screen().picture_revision;
        return;
    }
    for (unsigned n = 0; n < 3; ++n)
    {
        auto image = sprite(arg(0), n);
        // Beyond a sprite's last distance frame its farthest one shows.
        if (!image && n > 0)
        {
            decoded[n] = decoded[n - 1];
            continue;
        }
        if (!image)
        {
            throw EclError("Unsupported encounter sprite " + std::to_string(arg(0)));
        }
        decoded[n] = std::move(image.image);
    }
    tour.encounter_sprites() = std::move(decoded);
    tour.screen().sprite_frame = static_cast<int>(encounter_distance_);
    tour.screen().sprite_id = arg(0);
    monster_picture_id_ = arg(2);
    tour.shown_picture().reset();
    ++tour.screen().picture_revision;
}

void EncounterHost::load_monster(TourHost &tour, const EclRequest &request)
{
    // Operand n as a number.
    const auto arg = [&](unsigned n)
    {
        return request.arguments.at(n).number();
    };
    constexpr auto monster_limit = fight_monster_limit;
    // A roaming group's size follows the party's strength, so a party with
    // most of its members down can be met by no one at all: the script
    // loads nothing, and its COMBAT then has nothing to fight.
    if (!arg(1))
    {
        return;
    }
    // An override (data/encounters/overrides.json) changes what this
    // instruction loads; the count is still sized to the party.
    const auto *change = encounter_override(tour, *request.instruction);
    const unsigned record = change && change->record ? *change->record : arg(0);
    const unsigned loaded = change && change->count ? *change->count : arg(1);
    const unsigned icon = change && change->icon ? *change->icon : arg(2);
    const auto count = static_cast<unsigned>(std::min<std::size_t>(
        loaded, monster_limit - std::min(monster_limit, staged_enemies_.size())));
    if (!tour.plays_original_records() && !tour.area_resources().conversions.contains(record))
    {
        throw EclError("Encounter needs an explicit supported creature conversion: record " +
                       std::to_string(record) + ", count " + std::to_string(count) + ", icon " +
                       std::to_string(icon));
    }
    const auto &area = tour.area_resources();
    const bool ally = fights_for_party(area, record);
    std::optional<std::array<VgaColor, 256>> native_palette;
    if (area.dark_queen)
    {
        native_palette = ua::combat_palette(area.dark_queen->files, tour.dark_queen_combat_set(),
                                            tour.script_machine().variable(EclAddress{0x22}) != 0);
    }
    // Treasures' LOAD MONSTER holds its creatures back while cell 44h is
    // set: they join the fight as that round ends, the ninth at the
    // latest (its EclLoadMonster and JoinDelayedCombatants).
    constexpr EclAddress treasures_arrival_round{0x44};
    constexpr unsigned latest_arrival = 9;
    const unsigned arrival =
        tour.game() == GoldBoxGame::treasures_of_the_savage_frontier
            ? std::min(
                  static_cast<unsigned>(tour.script_machine().variable(treasures_arrival_round)),
                  latest_arrival)
            : 0;
    const auto *exported = exported_creature(tour, area, record);
    const auto name = creature_name(area, record, exported);
    // Pools of Darkness gives each member a try at turning undead for
    // each LOAD MONSTER that brings undead (its EclLoadMonster,
    // turnUndeadLimit).
    if (const auto creature = area.encounter_creatures.find(record);
        count > 0 && creature != area.encounter_creatures.end() &&
        creature->second.stored.undead_level != 0)
    {
        ++staged_undead_loads_;
    }
    for (unsigned i = 0; ally && i < count; ++i)
    {
        const auto id =
            static_cast<rules::EntityId>(opengold::first_ally_id + staged_allies_.size());
        auto staged = stage_area_creature(
            area, record, icon, exported, id, name, rules::Side::party, tour.played_difficulty(),
            tour.plays_original_records(), tour.game(), native_palette);
        staged.fighter.arrival_round = arrival;
        staged_allies_.push_back(std::move(staged.fighter));
        staged_ally_art_.push_back(std::move(staged.art));
    }
    for (unsigned i = 0; !ally && i < count; ++i)
    {
        const auto id = static_cast<rules::EntityId>(1000 + staged_enemies_.size());
        auto staged = stage_area_creature(
            area, record, icon, exported, id,
            name + " " + std::to_string(staged_enemies_.size() + 1), rules::Side::opposition,
            tour.played_difficulty(), tour.plays_original_records(), tour.game(), native_palette);
        staged.fighter.arrival_round = arrival;
        staged_enemies_.push_back(std::move(staged.fighter));
        staged_art_.push_back(std::move(staged.art));
        staged_records_.push_back(record);
    }
}

void EncounterHost::open_menu(TourHost &tour, const EclRequest &request)
{
    // Operand n as a number.
    const auto arg = [&](unsigned n)
    {
        return request.arguments.at(n).number();
    };
    if (!tour.current_area() || arg(1) > 2)
    {
        throw EclError("Unsupported encounter menu context");
    }
    encounter_menu_ = request;
    encounter_distance_ = std::min<unsigned>(arg(1), tour.room_ahead());
    show_encounter_menu(tour);
}

bool EncounterHost::fight(TourHost &tour, const EclRequest &request)
{
    if (mirror_duel_ && tour.campaign())
    {
        stage_mirror(tour);
    }
    // New Phlan's own fights too: the city watch's, Tempus's priests'.
    if (!staged_enemies_.empty())
    {
        tour.read_character();
        encounter_records_ = staged_records_;
        // The arena's copy is the duelist's match, whatever the party.
        if (!mirror_duel_)
        {
            fit_staged_encounter(tour);
        }
        const auto p = tour.screen().pose;
        // On the wilderness map the field grows from the terrain under the
        // party, drawn with the wilderness set, north up.
        const auto roll = [&](int count, int sides)
        {
            int total = 0;
            for (int die = 0; die < count; ++die)
            {
                total += static_cast<int>(tour.script_machine().host_random(
                             request.id, static_cast<unsigned>(sides))) +
                         1;
            }
            return total;
        };
        // The original faces a wilderness fight the way the party stepped;
        // a diagonal step faces the direction to its left (PLACE-2).
        const auto wilderness_facing = static_cast<MapDirection>(
            tour.script_machine().variable(tour.game_cell(ecl_overland_direction)) % 8 / 2);
        // Curse fights out of doors (its 3D-area cell clear) on open ground
        // grown from its current place's terrain (cell 0x4E42), as the
        // original's SetupWildernessFloor does.
        const bool curse_outdoors =
            tour.town_resources().curse_wilderness &&
            tour.script_machine().variable(tour.game_cell(ecl_in_3d_area)) == 0;
        const auto curse_place_flags = [&]() -> unsigned
        {
            constexpr EclAddress curse_current_place{0x4E42};
            const auto &flags = tour.town_resources().curse_wilderness->place_flags;
            const auto place = tour.script_machine().variable(curse_current_place);
            return place < flags.size() ? flags[place] : 0U;
        };
        // Gateway fights on its overland on grass grown from the ground's
        // class (cell 4CA1h, which nothing sets), as its
        // BuildWildernessArena does.
        const bool gateway_outdoors =
            tour.town_resources().gateway_wilderness &&
            (tour.gateway_overland() || (krynn_game(tour.game()) && tour.vga_area_map()));
        const auto gateway_ground_class = [&]() -> unsigned
        {
            constexpr EclAddress gateway_ground{0x4CA1};
            const auto &classes = tour.town_resources().gateway_wilderness->terrain_classes;
            // Champions' BuildWildernessArena explicitly selects terrain 15.
            if (krynn_game(tour.game()))
            {
                return classes.at(15);
            }
            const std::size_t ground = tour.script_machine().variable(gateway_ground) & 0xFFu;
            return ground < classes.size() ? classes[ground] : 0U;
        };
        std::vector<Image> native_terrain;
        auto field = [&]
        {
            if (tour.town_resources().dark_queen)
            {
                const bool dungeon = tour.script_machine().variable(EclAddress{0x22}) != 0;
                auto native =
                    dungeon
                        ? dark_queen_dungeon_field(*tour.town_resources().dark_queen,
                                                   tour.loaded_map(), p,
                                                   tour.dark_queen_combat_set())
                        : ua::wilderness_field(
                              tour.town_resources().dark_queen->files, tour.dark_queen_combat_set(),
                              [&](unsigned sides)
                              {
                                  return tour.script_machine().host_random(request.id, sides) + 1;
                              },
                              dark_queen_wilderness_style(
                                  tour.script_machine().variable(EclAddress{0x1A2}),
                                  tour.script_machine().variable(EclAddress{0x33}) != 0));
                native_terrain = std::move(native.terrain_art);
                return std::move(native.field);
            }
            return tour.overland()
                       ? wilderness_battlefield(
                             *tour.town_resources().overland,
                             tour.town_resources().overland->at(
                                 tour.town_resources()
                                         .overland->window_start[tour.travel_mode() - 2] +
                                     tour.script_machine().variable(tour.game_cell(ecl_overland_x)),
                                 tour.script_machine().variable(tour.game_cell(ecl_overland_y))),
                             roll)
                   : curse_outdoors   ? wilderness_battlefield(
                                            tour.town_resources().curse_wilderness->combat_tiles,
                                            curse_place_flags(), roll)
                   : gateway_outdoors ? gateway_wilderness_battlefield(
                                            tour.town_resources().gateway_wilderness->combat_tiles,
                                            gateway_ground_class(), roll)
                                      : dungeon_battlefield(tour.loaded_map(), p.x, p.y);
        }();
        encounter_ =
            opengold::CampaignEncounter{std::move(field),
                                        std::move(staged_enemies_),
                                        std::move(staged_art_),
                                        tour.overland() || curse_outdoors || gateway_outdoors
                                            ? tour.town_resources().wilderness_art
                                            : tour.area_resources().terrain_art,
                                        tour.overland() ? wilderness_facing : p.facing,
                                        script_surprise(tour.script_machine().variable(
                                            tour.game_cell(ecl_encounter_surprise)))};
        if (tour.town_resources().dark_queen)
        {
            encounter_->terrain_art = std::move(native_terrain);
        }
        encounter_->band = std::move(staged_band_);
        staged_band_.clear();
        encounter_->allies = std::move(staged_allies_);
        staged_allies_.clear();
        for (auto &art : staged_ally_art_)
        {
            encounter_->art.push_back(std::move(art));
        }
        staged_ally_art_.clear();
        encounter_->party_resting = tour.party_resting();
        const auto hour = clock_minute_of_day(tour.campaign()->state().time_minutes) / 60;
        encounter_->daylight = hour >= 6 && hour < 18;
        encounter_->outdoors = tour.overland() || curse_outdoors || gateway_outdoors;
        if (tour.town_resources().dark_queen)
        {
            encounter_->outdoors = tour.script_machine().variable(EclAddress{0x22}) == 0;
        }
        // Gateway's combat mode 1 (cell 4D01h), which its maze through
        // Ascore sets (ECL6:27 0x843A).
        constexpr EclAddress gateway_combat_mode{0x4D01};
        encounter_->escape_ends_fight =
            tour.game() == GoldBoxGame::gateway_to_the_savage_frontier &&
            tour.script_machine().variable(gateway_combat_mode) == 1;
        // Treasures' crystal battles, which their scripts mark by cell
        // 3Dh (gameState's crystalsActive).
        constexpr EclAddress treasures_crystals_active{0x3D};
        encounter_->crystals = tour.game() == GoldBoxGame::treasures_of_the_savage_frontier &&
                               tour.script_machine().variable(treasures_crystals_active) != 0;
        // PARTY_MOVE_MODIFIER, which the fight's end clears (postcbt.pas).
        encounter_->party_move_modifier = static_cast<int>(
            tour.script_machine().variable(tour.game_cell(EclAddress{0x6E72})) & 0xFF);
        // Treasures' cell 53h, which its scripts set where its yetis
        // surprise the party (effhndlr.pas, EffFrightenAura).
        encounter_->frightening_surprise =
            tour.game() == GoldBoxGame::treasures_of_the_savage_frontier &&
            tour.script_machine().variable(EclAddress{0x53}) != 0;
        // Treasures' romance, once a script has paired a member and an
        // NPC (romance.pas, TagRelationshipPair, as each fight starts).
        encounter_->romance = tour.treasures_romance();
        // Pools of Darkness's turning: a try for each LOAD MONSTER of
        // undead, and the penalty its scripts set in cell Fh
        // (turnUndeadPenalty), as CombatTurnUndead reads them.
        if (tour.game() == GoldBoxGame::pools_of_darkness)
        {
            constexpr EclAddress darkness_turning_penalty{0x0F};
            encounter_->turning_tries = static_cast<int>(staged_undead_loads_);
            encounter_->turning_penalty =
                static_cast<int>(tour.script_machine().variable(darkness_turning_penalty) & 0xFF);
            // Its cell 2 (antiMagic), which the finale's script sets for
            // Gothmenes' last stand: no one casts a spell or uses a magic
            // item (combatai.pas, SelectSpellToCast; combat.pas, the
            // Cast menu; viewchar.pas, the Use menu).
            encounter_->no_magic = tour.magic_barred();
            // Its own cell for the party's movement modifier, which
            // Moander's heart sets (ECL1:53 0x9C8C).
            encounter_->party_move_modifier =
                static_cast<int>(tour.script_machine().variable(darkness_party_move_mod) & 0xFF);
        }
        if (tour.game() == GoldBoxGame::dark_queen_of_krynn)
        {
            // Native LOAD MONSTER counts undead loads, not copies, for
            // each member's turning attempts. Cell Fh reduces its column.
            encounter_->turning_tries = static_cast<int>(staged_undead_loads_);
            encounter_->turning_penalty =
                static_cast<int>(tour.script_machine().variable(EclAddress{0x0F}) & 0xFF);
            encounter_->underwater = tour.script_machine().variable(EclAddress{0x003C}) != 0;
            encounter_->no_magic = tour.magic_barred();
            encounter_->party_move_modifier =
                static_cast<int>(tour.script_machine().variable(EclAddress{0x001C}) & 0xFF);
        }
        if (std::exchange(duel_, false))
        {
            encounter_->mirror = mirror_duel_;
            encounter_->champion =
                tour.campaign()->state().slots.at(tour.selected_character().index);
            if (!encounter_->champion)
            {
                throw EclError("A duel needs a chosen member");
            }
        }
        else
        {
            std::size_t living = 0;
            for (const auto id : tour.campaign()->state().slots)
            {
                if (id && !tour.campaign()->member(id).vitals.dead)
                {
                    ++living;
                    if (turncoat(tour, id))
                    {
                        encounter_->turncoats.push_back(id);
                    }
                }
            }
            // With every member gone over to the monsters, no one is left
            // to fight them: the party is lost.
            if (living && encounter_->turncoats.size() == living)
            {
                tour.screen().phase = TourPhase::defeated;
                ++tour.screen().revision;
                return true;
            }
        }
        if (!tour.overland())
        {
            encounter_distance_ = std::min(encounter_distance_, tour.room_ahead());
        }
        // On the wilderness map the original stands the monsters 2 squares
        // away, whatever SETUP MONSTER asked (PLACE-2); so does Gateway's
        // overland (its MaxMonsterDist).
        encounter_->distance =
            tour.overland() || tour.gateway_overland() ? 2u : encounter_distance_;
        // The monsters are dealt to the sides SETUP MONSTER named, in turn
        // (combatst.pas, PlaceAllCreatures).
        encounter_->enemy_quarters =
            por::dealt_quarters(monster_sides_, encounter_->enemies.size());
        // The script sets the encounter's morale before COMBAT.
        const auto morale_cell = tour.game() == GoldBoxGame::pools_of_darkness
                                     ? darkness_break_point
                                     : tour.game_cell(ecl_encounter_morale);
        encounter_->morale = std::min<unsigned>(tour.script_machine().variable(morale_cell), 100);
        if (tour.plays_original_records())
        {
            apply_original_difficulty(*encounter_, tour.played_difficulty(), tour.game());
        }
        else
        {
            apply_difficulty(*encounter_, tour.played_difficulty(),
                             average_party_level(living_member_levels(tour)));
        }
        add_script_to_hit(tour);
        tour.announce_camp_attack();
        combat_request_ = request.id;
        // The original shows the approached monster's close-up until a key press.
        showing_monster_picture_ =
            monster_picture_id_ && tour.area_resources().animations.contains(*monster_picture_id_);
        tour.screen().phase = TourPhase::combat;
        ++tour.screen().revision;
        return true;
    }
    return false;
}

void EncounterHost::ward_off()
{
    staged_enemies_.clear();
    staged_art_.clear();
    staged_records_.clear();
    staged_undead_loads_ = 0;
}

void EncounterHost::forget_staged()
{
    staged_enemies_.clear();
    staged_art_.clear();
    staged_records_.clear();
    staged_undead_loads_ = 0;
    staged_allies_.clear();
    staged_ally_art_.clear();
}

void EncounterHost::approach() noexcept
{
    if (encounter_distance_ > 0)
    {
        --encounter_distance_;
    }
}

void EncounterHost::duel(bool mirror) noexcept
{
    duel_ = true;
    if (mirror)
    {
        mirror_duel_ = true;
    }
}

void EncounterHost::turn(MemberId id, bool hostile)
{
    if (hostile)
    {
        turncoats_.insert(id);
    }
    else
    {
        turncoats_.erase(id);
    }
}

void EncounterHost::begin_event() noexcept
{
    encounter_outcome_.clear();
    meeting_pose_.reset();
    robbed_ = false;
}

void EncounterHost::end_meeting(TourSnapshot &snapshot)
{
    // A party the script moved away from a meeting (Flee, or Run after a
    // parley) got away, unless it was robbed and sent off.
    if (meeting_pose_ && encounter_outcome_.empty() && !robbed_ &&
        (meeting_pose_->x != snapshot.pose.x || meeting_pose_->y != snapshot.pose.y))
    {
        encounter_outcome_ = "You get away.";
    }
    if (!encounter_outcome_.empty())
    {
        snapshot.dialogue += (snapshot.dialogue.empty() ? "" : "\n") + encounter_outcome_;
    }
    encounter_outcome_.clear();
    meeting_pose_.reset();
    robbed_ = false;
}

void EncounterHost::reset()
{
    staged_enemies_.clear();
    staged_art_.clear();
    staged_records_.clear();
    staged_undead_loads_ = 0;
    encounter_menu_.reset();
    encounter_.reset();
    monster_picture_id_.reset();
    showing_monster_picture_ = false;
    combat_request_ = 0;
    duel_ = mirror_duel_ = false;
    turncoats_.clear();
}

void EncounterHost::forget_for_load()
{
    combat_request_ = 0;
    staged_enemies_.clear();
    staged_art_.clear();
    staged_undead_loads_ = 0;
    staged_allies_.clear();
    staged_ally_art_.clear();
    encounter_.reset();
    duel_ = mirror_duel_ = false;
    turncoats_.clear();
    monster_picture_id_.reset();
    showing_monster_picture_ = false;
    encounter_outcome_.clear();
    meeting_pose_.reset();
    robbed_ = false;
}
} // namespace opengold::por
