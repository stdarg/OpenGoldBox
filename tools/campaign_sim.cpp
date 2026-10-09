// Plays the Slums and then Kuto's Well as a campaign arc many times to estimate
// how often a party succeeds. A level-one party from the curated character
// pool, in its class kits, fights the original encounters in a fixed order,
// takes the catacombs' arrow volleys, rests between fights and gains levels up
// to four. Both sides use the automated demo policy. Each seed is one run;
// results go to CSV.
#include "opengold/authored_items.h"
#include "opengold/campaign_party.h"
#include "opengold/character_art.h"
#include "opengold/character_pool.h"
#include "opengold/combat_demo.h"
#include "opengold/dungeon_battlefield.h"
#include "opengold/encounter_budget.h"
#include "opengold/map_catalog.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>

using namespace opengold;

namespace
{
std::unique_ptr<rules::RulesModule> module()
{
    return srd5::load(std::filesystem::path(OPENGOLD_SOURCE_DIR) /
                      "data/rules/srd-5.2.1/combat.rules");
}

struct PartyPlan
{
    std::string name;
    std::vector<std::string> classes;
};

// Standard parties: a classic Pool of Radiance mix, all martial, all casters,
// and six of each class.
std::vector<PartyPlan> party_plans(const rules::CharacterRules &rules)
{
    std::vector<PartyPlan> plans{
        {"classic", {"fighter", "fighter", "cleric", "rogue", "wizard", "paladin"}},
        {"martial", {"fighter", "barbarian", "paladin", "ranger", "monk", "rogue"}},
        {"casters", {"wizard", "sorcerer", "warlock", "cleric", "druid", "bard"}}};
    for (const auto &klass : rules.choices(rules::CreationField::character_class))
        plans.push_back({"six-" + klass.id, std::vector<std::string>(6, klass.id)});
    return plans;
}

// The original creature records of a fight and the profiles they fight as.
struct Creature
{
    unsigned record;
    const char *definition;
    unsigned fit_xp; // the stat block's XP, which sizes the encounter
    // The record's Intelligence: a broken monster that cannot outrun the party
    // surrenders above 5 and fights on otherwise. None of these records has a
    // morale of its own (byte 0x84 is 0xFF), so the encounter's decides.
    unsigned intelligence;
};

Creature creature(unsigned record)
{
    switch (record)
    {
    case 0:
        return {0, "slums-kobold", 25, 6};
    case 1:
        return {1, "slums-kobold-leader", 25, 10};
    case 2:
        return {2, "slums-goblin", 50, 10};
    case 3:
        return {3, "slums-goblin-leader", 50, 10};
    case 4:
        return {4, "slums-orc", 100, 6};
    case 5:
        return {5, "slums-orc-leader-archer", 100, 10};
    case 13:
        return {13, "slums-orc-leader", 100, 6};
    case 6:
        return {6, "slums-hobgoblin", 100, 10};
    case 7:
        return {7, "slums-hobgoblin", 100, 10};
    case 8:
        return {8, "ogre", 450, 6};
    case 31:
        return {31, "troll", 1800, 10};
    case 32:
        return {32, "norris-the-gray", 450, 10};
    case 57:
        return {57, "lizardfolk", 100, 3};
    case 59:
        return {59, "giant-lizard", 50, 3};
    case 73:
        return {73, "gnoll-warrior", 100, 10};
    default:
        return {63, "slums-bugbear", 200, 10};
    }
}

// Experience as the campaign awards it (rolf_tour.cpp's area table), per member:
// the original Slums creatures' values, otherwise the stat block's XP, a
// hobgoblin leader double.
unsigned original_xp(unsigned record)
{
    if (record == 7)
        return 200;
    if (record == 6 || record == 8 || record == 31 || (record >= 32 && record != 63))
        return creature(record).fit_xp;
    return record == 63                                 ? 200
           : record == 0                                ? 25
           : record == 1 || record == 2 || record == 11 ? 50
           : record == 3 || record == 12                ? 100
           : record == 4 || record == 13                ? 75
           : 150;
}

struct Group
{
    Creature kind;
    unsigned count{};
};

// A fight's creatures and the encounter morale its script sets before COMBAT
// (0x6DC6; 100 never breaks).
struct Fight
{
    std::vector<Group> groups;
    unsigned morale;
};

// ECL2:20's roaming mix (as tools/encounter_balance.cpp documents it): base
// count from party strength, leaders from strength 8 (14 for orcs), a bugbear
// above 18. The script's morale depends on how the meeting opened, from 70
// (the party surprised) to 30 (the monsters surprised); the simulator takes
// the middle one, 50, when the groups bump into each other. Leaders add 5 and
// a bugbear 15.
Fight roaming(unsigned record, unsigned strength, bool guild)
{
    const unsigned scaled = guild ? strength : strength / 3 * 2;
    Fight fight{{}, 50};
    if (scaled >= (record == 4 ? 14u : 8u))
    {
        fight.groups.push_back({creature(record + 1), record < 1 ? 3u : 4u});
        fight.morale = 55;
    }
    if (scaled > 18)
    {
        fight.groups.push_back({creature(63), 1});
        fight.morale = 65;
    }
    fight.groups.push_back({creature(record), std::max(1u, scaled)});
    return fight;
}

// ECL8:29's roaming mix (its table at 0xAFA2): gnolls at a third of the
// party's strength, kobolds at half with three kobold leaders, lizardmen at a
// quarter; at least one. Morale comes from the table at 0xAFB1 (gnolls 75,
// kobolds 50, lizardmen 90); the script adjusts it by how the meeting opened,
// from +9 to -20, and the simulator takes the unadjusted face-to-face one.
Fight kutos_roaming(unsigned record, unsigned strength)
{
    const unsigned divisor = record == 73 ? 3 : record == 0 ? 2 : 4;
    Fight fight{{}, record == 73 ? 75u : record == 0 ? 50u : 90u};
    if (record == 0)
        fight.groups.push_back({creature(1), 3});
    fight.groups.push_back({creature(record), std::max(1u, strength / divisor)});
    return fight;
}

enum class ArcMap
{
    slums,
    kutos_plaza,
    kutos_catacombs
};

// Where a fight happens: its map and the square the battlefield is cut around.
struct Place
{
    ArcMap map;
    unsigned x, y;
};

// A Slums street, the Old Rope Guild and three set-encounter rooms; north of
// Kuto's Well and Norris's hall.
constexpr Place slums_street{ArcMap::slums, 14, 7}, slums_guild{ArcMap::slums, 6, 14},
          hobgoblin_room{ArcMap::slums, 0, 2}, leaders_room{ArcMap::slums, 1, 5},
          troll_room{ArcMap::slums, 0, 14}, kutos_plaza{ArcMap::kutos_plaza, 7, 4},
          kutos_catacombs{ArcMap::kutos_catacombs, 10, 3};

struct Step
{
    const char *label;
    Place place;
    Fight (*fight)(unsigned strength);
    unsigned arrows{}; // An arrow volley instead of a fight.
};

// The Slums' trolls and ogres, the script's morale 75.
Fight troll_fight()
{
    return {{{creature(8), 2}, {creature(31), 4}}, 75};
}

// The arc: the Slums' kobolds, goblins and orcs on the streets and in the Old
// Rope Guild, with the four-orc search second and its set encounters (the
// hobgoblins arguing over gold, the monster leaders and, last, the trolls and
// ogres); then Kuto's Well's plaza, the catacombs' arrow volleys and Norris the
// Gray's band. Each fight has its script's morale.
const std::vector<Step> arc{
    {"street kobolds", slums_street, [](unsigned s) { return roaming(0, s, false); }},
    {"four orcs", slums_street,
     [](unsigned) { return Fight{{{creature(13), 1}, {creature(4), 3}}, 99}; }},
    {"street goblins", slums_street, [](unsigned s) { return roaming(2, s, false); }},
    {"street orcs", slums_street, [](unsigned s) { return roaming(4, s, false); }},
    {"guild kobolds", slums_guild, [](unsigned s) { return roaming(0, s, true); }},
    {"street goblins", slums_street, [](unsigned s) { return roaming(2, s, false); }},
    {"guild goblins", slums_guild, [](unsigned s) { return roaming(2, s, true); }},
    {"street orcs", slums_street, [](unsigned s) { return roaming(4, s, false); }},
    {"guild orcs", slums_guild, [](unsigned s) { return roaming(4, s, true); }},
    {"street goblins", slums_street, [](unsigned s) { return roaming(2, s, false); }},
    {"guild orcs", slums_guild, [](unsigned s) { return roaming(4, s, true); }},
    {"guild orcs", slums_guild, [](unsigned s) { return roaming(4, s, true); }},
    {"arguing hobgoblins", hobgoblin_room,
     [](unsigned) { return Fight{{{creature(6), 5}}, 75}; }},
    {"monster leaders", leaders_room,
     [](unsigned) {
         return Fight{{{creature(8), 1}, {creature(73), 2}, {creature(7), 2}}, 80};
     }},
    {"trolls and ogres", troll_room,
     [](unsigned) { return troll_fight(); }},
    {"plaza gnolls", kutos_plaza, [](unsigned s) { return kutos_roaming(73, s); }},
    {"sickly kobolds", kutos_plaza,
     [](unsigned) { return Fight{{{creature(0), 2}, {creature(1), 4}}, 25}; }},
    {"plaza kobolds", kutos_plaza, [](unsigned s) { return kutos_roaming(0, s); }},
    {"well kobolds", kutos_plaza,
     [](unsigned) { return Fight{{{creature(0), 6}, {creature(1), 3}}, 67}; }},
    {"lizardman patrol", kutos_plaza,
     [](unsigned) { return Fight{{{creature(57), 1}, {creature(59), 4}}, 75}; }},
    {"plaza lizardmen", kutos_plaza, [](unsigned s) { return kutos_roaming(57, s); }},
    {"catacomb volley", kutos_catacombs, nullptr, 4},
    {"dim archer", kutos_catacombs, nullptr, 1},
    {"Norris's band", kutos_catacombs,
     [](unsigned) {
         return Fight{{{creature(32), 1}, {creature(57), 5}, {creature(1), 9}}, 70};
     }}};

// The first Kuto's Well step; runs that reach it have cleared the Slums.
const std::size_t kutos_well_start = 15;

// Spells the demo policy uses well, prepared on gaining a level when offered.
const std::map<std::string, std::vector<std::string>> preferred_spells{
    {"Cleric", {"spiritual_weapon", "healing_word", "cure_wounds", "guiding_bolt", "hold_person"}},
    {"Druid", {"moonbeam", "healing_word", "cure_wounds"}},
    {"Bard", {"healing_word", "dissonant_whispers", "cure_wounds", "hold_person"}},
    {"Sorcerer", {"magic_missile", "scorching_ray", "shield", "hold_person"}},
    {"Warlock", {"hex", "hold_person"}},
    {"Paladin", {"cure_wounds"}},
    {"Ranger", {"cure_wounds"}}};

// The default advancement, with the class's preferred spells swapped in for
// the last unlocked defaults where the rules accept them.
rules::AdvancementChoice advancement(const CampaignParty &party, MemberId id)
{
    auto choice = party.default_advancement(id);
    const auto found = preferred_spells.find(party.member(id).character.sheet().character_class);
    if (found == preferred_spells.end())
        return choice;
    for (const auto &spell : found->second)
    {
        if (std::find(choice.spells.begin(), choice.spells.end(), spell) != choice.spells.end())
            continue;
        for (auto slot = choice.spells.rbegin(); slot != choice.spells.rend(); ++slot)
        {
            const bool preferred = std::find(found->second.begin(), found->second.end(), *slot) !=
                                   found->second.end();
            if (preferred)
                continue;
            auto trial = choice;
            trial.spells[std::size_t(choice.spells.rend() - slot - 1)] = spell;
            try
            {
                (void)party.preview_advancement(id, trial);
                choice = std::move(trial);
            }
            catch (const std::exception &)
            {
                // Not on the list, not yet castable or locked; keep the default.
            }
            break;
        }
    }
    return choice;
}

struct RunResult
{
    std::string outcome; // "complete", "defeated" or "stalled"
    unsigned fights_won{}, deaths{}, short_rests{}, long_rests{};
    // Monsters whose morale broke: those that ran off the field and those that
    // gave up.
    unsigned escaped{}, surrendered{};
    std::string lost_at;
    bool cleared_slums{};
    std::vector<unsigned> levels;
    std::vector<std::string> dead; // the classes of members who died
};

// How often each class's members chose each command, to see which features
// the policy actually uses.
using Usage = std::map<std::pair<std::string, std::string>, unsigned>;

// "flurry_of_blows", "wild_shape_wolf" and "moonbeam" stay as they are; a
// spell's level-two form and Sorcerous Burst's type fold into the base verb.
std::string usage_verb(std::string verb)
{
    if (verb.ends_with("_2"))
        verb.resize(verb.size() - 2);
    if (verb.starts_with("sorcerous_burst_"))
        verb = "sorcerous_burst";
    return verb;
}

std::vector<MemberId> living(const CampaignParty &party)
{
    std::vector<MemberId> result;
    for (const auto id : party.state().slots)
        if (id && !party.member(id).vitals.dead)
            result.push_back(id);
    return result;
}

bool wounded(const CampaignParty &party, double fraction)
{
    for (const auto id : living(party))
        if (party.member(id).vitals.hit_points < fraction * party.hit_point_maximum(id))
            return true;
    return false;
}

// True when the party has fewer than `fraction` of its spell slots left. A party
// with no spell slots never is.
bool out_of_spells(const CampaignParty &party, double fraction)
{
    unsigned remaining = 0, capacity = 0;
    for (const auto &info : party.rest_info(rules::RestKind::long_rest))
        for (const auto &resource : info.recovery.resources)
            if (resource.id.starts_with("spell_slot:"))
            {
                remaining += resource.remaining;
                capacity += resource.capacity;
            }
    return remaining < fraction * capacity;
}

// Keeps every completed-rest choice as it is.
void settle_rest_choices(CampaignParty &party)
{
    // Copies: each choice removes its member from the window.
    if (const auto window = party.state().spell_rest)
        for (const auto id : window->members)
            party.keep_rest_spells(id);
    if (const auto window = party.state().training_rest)
        for (const auto id : window->members)
            party.keep_rest_training(window->ticket, id);
}

void short_rest(CampaignParty &party, RunResult &result)
{
    const auto rest = party.rest(rules::RestKind::short_rest);
    if (!rest)
        return;
    ++result.short_rests;
    if (rest->spending)
    {
        // Each spending refreshes the rest's ticket.
        for (const auto id : rest->members)
            if (party.member(id).vitals.hit_points < party.hit_point_maximum(id) &&
                    party.recovery_info(id).hit_dice > 0)
                (void)party.heal_with_hit_dice(party.state().short_rest->ticket, id);
        party.finish_short_rest(party.state().short_rest->ticket);
    }
    settle_rest_choices(party);
}

void long_rest(CampaignParty &party, RunResult &result)
{
    // A Long Rest needs a day since the last; wait it out.
    std::uint64_t wait = 0;
    for (const auto &info : party.rest_info(rules::RestKind::long_rest))
        if (info.denial == RestDenial::cooldown)
            wait = std::max(wait, info.wait_milliseconds);
    if (wait)
        party.advance_time_milliseconds(wait);
    if (party.rest(rules::RestKind::long_rest))
        ++result.long_rests;
    settle_rest_choices(party);
}

// The maps the arc's fights happen on.
struct ArcMaps
{
    const por::GeoMap &slums, &kutos_plaza, &kutos_catacombs;
};

por::DungeonBattlefield battlefield(const ArcMaps &maps, Place place)
{
    const auto &map = place.map == ArcMap::slums         ? maps.slums
                      : place.map == ArcMap::kutos_plaza ? maps.kutos_plaza
                      : maps.kutos_catacombs;
    return por::dungeon_battlefield(map, place.x, place.y);
}

// Each arrow attacks a random conscious member as the campaign's DAMAGE volleys
// do; returns false when nobody is left standing.
bool take_arrows(CampaignParty &party, unsigned arrows)
{
    for (unsigned n = 0; n < arrows; ++n)
        if (!party.hazard_attack({4, 1, 6, 0, "piercing"}))
            return false;
    for (const auto id : living(party))
        if (party.member(id).vitals.hit_points > 0)
            return true;
    return false;
}

// Each class's pool characters in turn, so a party of six of one class has its
// four variants.
std::vector<Character> plan_members(const PartyPlan &plan, const std::vector<Character> &pool)
{
    std::vector<Character> members;
    std::map<std::string, unsigned> used;
    for (const auto &klass : plan.classes)
    {
        std::vector<const Character *> of_class;
        for (const auto &c : pool)
            if (c.creation_data().character_class == klass)
                of_class.push_back(&c);
        members.push_back(*of_class.at(used[klass]++ % of_class.size()));
    }
    return members;
}

// Troll arena: the Slums' trolls-and-ogres fight (2 ogres and 4 trolls, fitted
// as the campaign fits it) on an open area of a given width, for a level-four
// party carrying extra flasks.
struct Loadout
{
    const char *name;
    std::uint8_t oil, fire; // flasks per member, beyond the starting kit
};

constexpr std::array loadouts{Loadout{"kit", 0, 0}, Loadout{"oil", 2, 0},
                              Loadout{"fire", 0, 2}, Loadout{"oil+fire", 2, 2}};
constexpr std::array<unsigned, 5> arena_widths{1, 2, 4, 8, 16};

// Gives a member flasks of Oil and Alchemist's Fire. The simulator does not
// track gold, so they cost nothing here.
void carry_flasks(CampaignParty &party, MemberId id, std::uint8_t oil, std::uint8_t fire)
{
    for (const auto &[type, count] : {std::pair{original_item::flask_of_oil, oil},
                                      std::pair{authored_item::alchemists_fire, fire}
                                     })
        if (count)
        {
            por::Equipment flask;
            flask.stored.type = type;
            flask.stored.stack_size = count;
            party.purchase(id, flask);
        }
}

std::shared_ptr<CampaignParty> level_four_party(const std::vector<Character> &members,
        const Loadout &loadout)
{
    auto party = std::make_shared<CampaignParty>(module());
    for (const auto &member : members)
    {
        const auto id = party->add_pc(member);
        outfit_pool_member(*party, id);
        carry_flasks(*party, id, loadout.oil, loadout.fire);
    }
    party->award_experience(party->rule_module().experience_for_level(4), "arena");
    for (const auto id : living(*party))
        while (party->can_advance(id))
            party->advance(id, advancement(*party, id));
    return party;
}

// Adds the fight's creatures, numbered from 1000 in group order, and its morale.
void add_enemies(CampaignEncounter &encounter, const Fight &fight)
{
    rules::EntityId next = 1000;
    for (const auto &group : fight.groups)
        for (unsigned c = 0; c < group.count; ++c, ++next)
        {
            encounter.enemies.push_back({next, group.kind.definition,
                                         std::string(group.kind.definition) + " " +
                                         std::to_string(next),
                                         1, {}});
            encounter.enemies.back().intelligence = group.kind.intelligence;
        }
    encounter.morale = fight.morale;
}

// The original record of the creature `add_enemies` numbered `id`.
unsigned enemy_record(const Fight &fight, rules::EntityId id)
{
    auto index = id - 1000;
    for (const auto &group : fight.groups)
    {
        if (index < group.count)
            return group.kind.record;
        index -= group.count;
    }
    throw std::out_of_range("No such enemy");
}

// An open area `width` squares wide between walls: the party at the west end,
// the monsters 14 squares east (ogres in front of trolls in a one-square
// corridor, side by side when there is room).
CampaignEncounter arena_encounter(unsigned width, const Fight &fight, std::size_t members)
{
    const int length = 30, height = int(width) + 2;
    CampaignEncounter arena;
    arena.field.geometry = {length, height, std::vector<std::uint8_t>(std::size_t(length * height))};
    for (int x = 0; x < length; ++x)
        arena.field.geometry.terrain[std::size_t(x)] =
            arena.field.geometry.terrain[std::size_t((height - 1) * length + x)] = 1;
    arena.field.tiles.resize(std::size_t(length * height), 7);
    const auto place = [&](std::size_t n, int front, int step)
    {
        return rules::Cell{front + step * int(n / width), 1 + int(n % width)};
    };
    for (std::size_t n = 0; n < members; ++n)
        arena.positions.push_back(place(n, 6, -1));
    add_enemies(arena, fight);
    for (std::size_t n = 0; n < arena.enemies.size(); ++n)
        arena.positions.push_back(place(n, 20, 1));
    return arena;
}

// Plays one arena fight; true on victory. Width 0 is the trolls' own room in the
// Slums (`slums`), with the campaign's usual placement.
bool arena_fight(const std::vector<Character> &members, const Loadout &loadout, unsigned width,
                 const por::GeoMap &slums, std::uint64_t seed)
{
    auto party = level_four_party(members, loadout);
    auto fight = troll_fight();
    std::vector<unsigned> levels;
    std::vector<EncounterGroup> sizes;
    for (const auto id : living(*party))
        levels.push_back(party->member(id).character.sheet().level);
    for (const auto &group : fight.groups)
        sizes.push_back({group.kind.fit_xp, group.count});
    const auto counts = fit_encounter_to_budget(
                            sizes, encounter_xp_budget(levels, default_encounter_challenge),
                            unsigned(levels.size()));
    for (std::size_t g = 0; g < fight.groups.size(); ++g)
        fight.groups[g].count = counts[g];
    CombatDemo combat(module());
    combat.campaign_party(party);
    if (width)
        combat.encounter(arena_encounter(width, fight, members.size()), seed);
    else
    {
        CampaignEncounter room;
        room.field = battlefield({slums, slums, slums}, troll_room);
        add_enemies(room, fight);
        combat.encounter(std::move(room), seed);
    }
    for (unsigned commands = 0;
            commands < 20000 && combat.combat().snapshot().outcome == rules::Outcome::ongoing;
            ++commands)
        if (!combat.submit(choose_demo_command(combat.combat())))
            throw std::runtime_error("Combat refused the demo command");
    return combat.combat().snapshot().outcome == rules::Outcome::victory;
}

// Runs every party, loadout and width; writes arena.csv and prints a table.
void troll_arena(const std::vector<PartyPlan> &plans, const std::vector<Character> &pool,
                 const por::GeoMap &slums, unsigned runs, const std::filesystem::path &out,
                 const std::string &only)
{
    std::ofstream csv(out / "arena.csv");
    csv << "party,loadout,width,runs,wins\n";
    std::printf("%-14s %-9s", "party", "loadout");
    for (const auto width : arena_widths)
        std::printf(" %5u", width);
    std::printf("  room   (win%% by area width in squares; room: the Slums' own)\n");
    for (const auto &plan : plans)
    {
        if (!only.empty() && plan.name != only)
            continue;
        const auto members = plan_members(plan, pool);
        for (const auto &loadout : loadouts)
        {
            std::printf("%-14s %-9s", plan.name.c_str(), loadout.name);
            for (const auto width : std::array{arena_widths[0], arena_widths[1], arena_widths[2],
                                               arena_widths[3], arena_widths[4], 0u})
            {
                unsigned wins = 0;
                for (unsigned run = 1; run <= runs; ++run)
                    wins += arena_fight(members, loadout, width, slums, run);
                csv << plan.name << ',' << loadout.name << ',' << width << ',' << runs << ','
                    << wins << '\n';
                std::printf(" %5.0f", 100.0 * wins / runs);
                std::fflush(stdout);
            }
            std::printf("\n");
        }
    }
}

RunResult play(const std::vector<Character> &members, const ArcMaps &maps, std::uint64_t seed,
               Usage &usage)
{
    auto party = std::make_shared<CampaignParty>(module());
    for (const auto &member : members)
    {
        const auto id = party->add_pc(member);
        outfit_pool_member(*party, id);
        // As if bought at New Phlan's general store before setting out.
        carry_flasks(*party, id, 2, 2);
    }
    RunResult result;
    unsigned since_long_rest = 0;
    for (std::size_t n = 0; n < arc.size(); ++n)
    {
        const auto &step = arc[n];
        if (step.arrows)
        {
            if (!take_arrows(*party, step.arrows))
            {
                result.outcome = "defeated";
                result.lost_at = step.label;
                break;
            }
            continue;
        }
        auto fight = step.fight(party->strength());
        // Experience is the original encounter's, however many fight.
        std::vector<unsigned> paying;
        for (const auto &group : fight.groups)
            paying.insert(paying.end(), group.count, group.kind.record);
        // The session fits every encounter, the four-orc search too, to the XP
        // budget and to one creature per living character.
        {
            std::vector<unsigned> levels;
            std::vector<EncounterGroup> sizes;
            for (const auto id : living(*party))
                levels.push_back(party->member(id).character.sheet().level);
            for (const auto &group : fight.groups)
                sizes.push_back({group.kind.fit_xp, group.count});
            const auto counts =
                fit_encounter_to_budget(sizes, encounter_xp_budget(levels, default_encounter_challenge),
                                        unsigned(levels.size()));
            for (std::size_t g = 0; g < fight.groups.size(); ++g)
                fight.groups[g].count = counts[g];
        }
        CampaignEncounter encounter;
        encounter.field = battlefield(maps, step.place);
        add_enemies(encounter, fight);
        rules::Outcome outcome{};
        {
            CombatDemo combat(module());
            combat.campaign_party(party);
            combat.encounter(encounter, seed * 100 + n);
            unsigned commands = 0;
            for (; commands < 20000 &&
                    combat.combat().snapshot().outcome == rules::Outcome::ongoing;
                    ++commands)
            {
                const auto command = choose_demo_command(combat.combat());
                if (command.actor < 1000 && command.verb != "move" && command.verb != "end" &&
                        command.verb != "area_move")
                    for (const auto id : party->state().slots)
                        if (id && party->member(id).id == command.actor)
                            ++usage[{party->member(id).character.creation_data().character_class,
                                     usage_verb(command.verb)}];
                if (!combat.submit(command))
                    throw std::runtime_error("Combat refused the demo command");
            }
            const auto result_snapshot = combat.combat().snapshot();
            outcome = result_snapshot.outcome;
            // As in the session, a monster that ran off the field takes its
            // record's experience with it.
            for (const auto &unit : result_snapshot.combatants)
                if (unit.side == 1 && unit.fled && unit.hit_points > 0)
                {
                    const auto record = enemy_record(fight, unit.id);
                    if (const auto it = std::find(paying.begin(), paying.end(), record);
                            it != paying.end())
                        paying.erase(it);
                    ++result.escaped;
                }
                else if (unit.side == 1 && unit.surrendered)
                    ++result.surrendered;
        }
        if (outcome != rules::Outcome::victory)
        {
            result.outcome = outcome == rules::Outcome::ongoing ? "stalled" : "defeated";
            result.lost_at = step.label;
            break;
        }
        ++result.fights_won;
        result.cleared_slums |= n + 1 == kutos_well_start;
        unsigned experience = 0;
        for (const auto record : paying)
            experience += original_xp(record);
        party->award_experience(experience, "sim:" + std::to_string(n));
        // Dying members make their death saves; the Stable regain 1 HP in hours.
        party->advance_time(10);
        bool down = false;
        for (const auto id : living(*party))
            down |= party->member(id).vitals.hit_points == 0;
        if (down)
            party->advance_time(240);
        for (const auto id : living(*party))
            while (party->can_advance(id))
                party->advance(id, advancement(*party, id));
        if (wounded(*party, 0.5))
            short_rest(*party, result);
        // A party low on spells rests too, as a player would before going on.
        if (++since_long_rest >= 3 || wounded(*party, 0.5) || out_of_spells(*party, 0.5))
        {
            long_rest(*party, result);
            since_long_rest = 0;
        }
    }
    if (result.outcome.empty())
        result.outcome = "complete";
    for (const auto id : party->state().slots)
        if (id)
        {
            result.deaths += party->member(id).vitals.dead;
            if (party->member(id).vitals.dead)
                result.dead.push_back(party->member(id).character.creation_data().character_class);
            result.levels.push_back(party->member(id).character.sheet().level);
        }
    return result;
}
} // namespace

int main(int argc, char **argv)
{
    try
    {
        if (argc < 2)
        {
            std::cerr << "Usage: opengold_campaign_sim [--troll-arena] GAME_DIR [RUNS [OUT_DIR "
                         "[PARTY]]]\n";
            return 2;
        }
        const bool arena = std::string_view(argv[1]) == "--troll-arena";
        if (arena && argc < 3)
        {
            std::cerr << "--troll-arena needs GAME_DIR\n";
            return 2;
        }
        if (arena)
        {
            ++argv;
            --argc;
        }
        const std::filesystem::path game = argv[1];
        const unsigned runs = argc > 2 ? unsigned(std::stoul(argv[2])) : 20;
        const std::filesystem::path out = argc > 3 ? argv[3] : "user-data/campaign-sim";
        const std::string only = argc > 4 ? argv[4] : "";
        std::filesystem::create_directories(out);
        const auto catalog = por::MapCatalog::load(game);
        const auto slums = catalog.find({"GEO2.DAX", 20});
        const auto plaza = catalog.find({"GEO8.DAX", 29});
        const auto catacombs = catalog.find({"GEO8.DAX", 32});
        if (!slums || !plaza || !catacombs)
            throw std::runtime_error("Missing GEO2:20, GEO8:29 or GEO8:32");
        const ArcMaps maps{slums->get(), plaza->get(), catacombs->get()};
        const auto characters = srd5::character_rules();
        const auto pool = character_pool(*characters, por::CharacterArt::load(game));
        if (arena)
        {
            troll_arena(party_plans(*characters), pool, maps.slums, runs, out, only);
            return 0;
        }
        std::ofstream detail(out / "runs.csv"), summary(out / "summary.csv");
        std::ofstream usage_csv(out / "usage.csv");
        detail << "party,run,outcome,fights_won,lost_at,deaths,dead_classes,short_rests,"
               "long_rests,levels,escaped,surrendered\n";
        summary << "party,runs,slums_pct,success_pct,flawless_pct,avg_fights_won,avg_deaths,"
                   "avg_level,avg_escaped,avg_surrendered\n";
        usage_csv << "party,class,command,count\n";
        std::printf("%-14s %5s %7s %8s %9s %10s %7s %9s %7s %11s\n", "party", "runs", "slums%",
                    "success%", "flawless%", "fights_won", "deaths", "avg_level", "escaped",
                    "surrendered");
        for (const auto &plan : party_plans(*characters))
        {
            if (!only.empty() && plan.name != only)
                continue;
            const auto members = plan_members(plan, pool);
            unsigned cleared = 0, complete = 0, flawless = 0, fights = 0, deaths = 0, levels = 0,
                     people = 0, escaped = 0, surrendered = 0;
            Usage usage;
            for (unsigned run = 1; run <= runs; ++run)
            {
                const auto r = play(members, maps, run, usage);
                cleared += r.cleared_slums;
                complete += r.outcome == "complete";
                flawless += r.outcome == "complete" && !r.deaths;
                fights += r.fights_won;
                deaths += r.deaths;
                escaped += r.escaped;
                surrendered += r.surrendered;
                std::string level_list;
                for (const auto level : r.levels)
                {
                    levels += level;
                    ++people;
                    level_list += (level_list.empty() ? "" : " ") + std::to_string(level);
                }
                std::string dead_list;
                for (const auto &klass : r.dead)
                    dead_list += (dead_list.empty() ? "" : " ") + klass;
                detail << plan.name << ',' << run << ',' << r.outcome << ',' << r.fights_won << ','
                       << r.lost_at << ',' << r.deaths << ',' << dead_list << ','
                       << r.short_rests << ',' << r.long_rests << ',' << level_list << ','
                       << r.escaped << ',' << r.surrendered << '\n';
            }
            for (const auto &[key, count] : usage)
                usage_csv << plan.name << ',' << key.first << ',' << key.second << ',' << count << '\n';
            const double success = 100.0 * complete / runs;
            const double slums_success = 100.0 * cleared / runs;
            summary << plan.name << ',' << runs << ',' << slums_success << ',' << success << ','
                    << 100.0 * flawless / runs << ',' << double(fights) / runs << ','
                    << double(deaths) / runs << ',' << double(levels) / people << ','
                    << double(escaped) / runs << ',' << double(surrendered) / runs << '\n';
            std::printf("%-14s %5u %7.0f %8.0f %9.0f %10.1f %7.2f %9.2f %7.1f %11.1f\n",
                        plan.name.c_str(), runs, slums_success, success, 100.0 * flawless / runs,
                        double(fights) / runs, double(deaths) / runs, double(levels) / people,
                        double(escaped) / runs, double(surrendered) / runs);
            std::fflush(stdout);
        }
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
