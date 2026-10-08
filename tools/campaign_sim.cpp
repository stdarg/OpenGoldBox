// Plays the Slums and then Kuto's Well as a campaign arc many times to estimate
// how often a party succeeds. A level-one party from the curated character
// pool, in its class kits, fights the original encounters in a fixed order,
// takes the catacombs' arrow volleys, rests between fights and gains levels up
// to four. Both sides use the automated demo policy. Each seed is one run;
// results go to CSV.
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
};

Creature creature(unsigned record)
{
    switch (record)
    {
    case 0:
        return {0, "slums-kobold", 25};
    case 1:
        return {1, "slums-kobold-leader", 25};
    case 2:
        return {2, "slums-goblin", 50};
    case 3:
        return {3, "slums-goblin-leader", 50};
    case 4:
        return {4, "slums-orc", 100};
    case 5:
        return {5, "slums-orc-leader-archer", 100};
    case 13:
        return {13, "slums-orc-leader", 100};
    case 32:
        return {32, "norris-the-gray", 450};
    case 57:
        return {57, "lizardfolk", 100};
    case 59:
        return {59, "giant-lizard", 50};
    case 73:
        return {73, "gnoll-warrior", 100};
    default:
        return {63, "slums-bugbear", 200};
    }
}

// Experience as the campaign awards it: the original encounter's, per member.
// Kuto's Well records (32 and up) award their stat block's XP.
unsigned original_xp(unsigned record)
{
    if (record >= 32 && record != 63)
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

// ECL2:20's roaming mix (as tools/encounter_balance.cpp documents it): base
// count from party strength, leaders from strength 8 (14 for orcs), a bugbear
// above 18.
std::vector<Group> roaming(unsigned record, unsigned strength, bool guild)
{
    const unsigned scaled = guild ? strength : strength / 3 * 2;
    std::vector<Group> groups;
    if (scaled >= (record == 4 ? 14u : 8u))
        groups.push_back({creature(record + 1), record < 1 ? 3u : 4u});
    if (scaled > 18)
        groups.push_back({creature(63), 1});
    groups.push_back({creature(record), std::max(1u, scaled)});
    return groups;
}

// ECL8:29's roaming mix (its table at 0xAFA2): gnolls at a third of the
// party's strength, kobolds at half with three kobold leaders, lizardmen at a
// quarter; at least one.
std::vector<Group> kutos_roaming(unsigned record, unsigned strength)
{
    const unsigned divisor = record == 73 ? 3 : record == 0 ? 2 : 4;
    std::vector<Group> groups;
    if (record == 0)
        groups.push_back({creature(1), 3});
    groups.push_back({creature(record), std::max(1u, strength / divisor)});
    return groups;
}

// Where a fight happens: its map and a square the battlefield is cut around.
enum class Place
{
    slums_street,
    slums_guild,
    kutos_plaza,
    kutos_catacombs
};

struct Step
{
    const char *label;
    Place place;
    std::vector<Group> (*groups)(unsigned strength);
    unsigned arrows{}; // An arrow volley instead of a fight.
};

// The arc: the Slums' kobolds, goblins and orcs on the streets and in the Old
// Rope Guild, with the four-orc search second; then Kuto's Well's plaza, the
// catacombs' arrow volleys and Norris the Gray's band.
const std::vector<Step> arc{
    {"street kobolds", Place::slums_street, [](unsigned s) { return roaming(0, s, false); }},
    {"four orcs", Place::slums_street,
     [](unsigned) { return std::vector<Group>{{creature(13), 1}, {creature(4), 3}}; }},
    {"street goblins", Place::slums_street, [](unsigned s) { return roaming(2, s, false); }},
    {"street orcs", Place::slums_street, [](unsigned s) { return roaming(4, s, false); }},
    {"guild kobolds", Place::slums_guild, [](unsigned s) { return roaming(0, s, true); }},
    {"street goblins", Place::slums_street, [](unsigned s) { return roaming(2, s, false); }},
    {"guild goblins", Place::slums_guild, [](unsigned s) { return roaming(2, s, true); }},
    {"street orcs", Place::slums_street, [](unsigned s) { return roaming(4, s, false); }},
    {"guild orcs", Place::slums_guild, [](unsigned s) { return roaming(4, s, true); }},
    {"street goblins", Place::slums_street, [](unsigned s) { return roaming(2, s, false); }},
    {"guild orcs", Place::slums_guild, [](unsigned s) { return roaming(4, s, true); }},
    {"guild orcs", Place::slums_guild, [](unsigned s) { return roaming(4, s, true); }},
    {"plaza gnolls", Place::kutos_plaza, [](unsigned s) { return kutos_roaming(73, s); }},
    {"sickly kobolds", Place::kutos_plaza,
     [](unsigned) { return std::vector<Group>{{creature(0), 2}, {creature(1), 4}}; }},
    {"plaza kobolds", Place::kutos_plaza, [](unsigned s) { return kutos_roaming(0, s); }},
    {"well kobolds", Place::kutos_plaza,
     [](unsigned) { return std::vector<Group>{{creature(0), 6}, {creature(1), 3}}; }},
    {"lizardman patrol", Place::kutos_plaza,
     [](unsigned) { return std::vector<Group>{{creature(57), 1}, {creature(59), 4}}; }},
    {"plaza lizardmen", Place::kutos_plaza, [](unsigned s) { return kutos_roaming(57, s); }},
    {"catacomb volley", Place::kutos_catacombs, nullptr, 4},
    {"dim archer", Place::kutos_catacombs, nullptr, 1},
    {"Norris's band", Place::kutos_catacombs,
     [](unsigned) {
         return std::vector<Group>{{creature(32), 1}, {creature(57), 5}, {creature(1), 9}};
     }}};

// The first Kuto's Well step; runs that reach it have cleared the Slums.
const std::size_t kutos_well_start = 12;

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

// Open squares the battlefields are cut around: a Slums street and the Old Rope
// Guild, north of Kuto's Well and Norris's hall.
por::DungeonBattlefield battlefield(const ArcMaps &maps, Place place)
{
    switch (place)
    {
    case Place::slums_street:
        return por::dungeon_battlefield(maps.slums, 14, 7);
    case Place::slums_guild:
        return por::dungeon_battlefield(maps.slums, 6, 14);
    case Place::kutos_plaza:
        return por::dungeon_battlefield(maps.kutos_plaza, 7, 4);
    default:
        return por::dungeon_battlefield(maps.kutos_catacombs, 10, 3);
    }
}

// Each arrow attacks a random conscious member as the campaign's DAMAGE volleys
// do; returns false when nobody is left standing.
bool take_arrows(CampaignParty &party, unsigned arrows)
{
    for (unsigned n = 0; n < arrows; ++n)
        if (!party.hazard_attack({3, 1, 6, 0, "piercing"}))
            return false;
    for (const auto id : living(party))
        if (party.member(id).vitals.hit_points > 0)
            return true;
    return false;
}

RunResult play(const std::vector<Character> &members, const ArcMaps &maps, std::uint64_t seed,
               Usage &usage)
{
    auto party = std::make_shared<CampaignParty>(module());
    for (const auto &member : members)
        outfit_pool_member(*party, party->add_pc(member));
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
        std::vector<Group> groups = step.groups(party->strength());
        unsigned experience = 0;
        for (const auto &group : groups)
            experience += group.count * original_xp(group.kind.record);
        // The session fits every encounter, the four-orc search too, to the XP
        // budget and to one creature per living character.
        {
            std::vector<unsigned> levels;
            std::vector<EncounterGroup> sizes;
            for (const auto id : living(*party))
                levels.push_back(party->member(id).character.sheet().level);
            for (const auto &group : groups)
                sizes.push_back({group.kind.fit_xp, group.count});
            const auto counts =
                fit_encounter_to_budget(sizes, encounter_xp_budget(levels, default_encounter_challenge),
                                        unsigned(levels.size()));
            for (std::size_t g = 0; g < groups.size(); ++g)
                groups[g].count = counts[g];
        }
        CampaignEncounter fight;
        fight.field = battlefield(maps, step.place);
        rules::EntityId next = 1000;
        for (const auto &group : groups)
            for (unsigned c = 0; c < group.count; ++c, ++next)
                fight.enemies.push_back({next, group.kind.definition,
                                         std::string(group.kind.definition) + " " +
                                         std::to_string(next),
                                         1, {}});
        rules::Outcome outcome{};
        {
            CombatDemo combat(module());
            combat.campaign_party(party);
            combat.encounter(fight, seed * 100 + n);
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
            outcome = combat.combat().snapshot().outcome;
        }
        if (outcome != rules::Outcome::victory)
        {
            result.outcome = outcome == rules::Outcome::ongoing ? "stalled" : "defeated";
            result.lost_at = step.label;
            break;
        }
        ++result.fights_won;
        result.cleared_slums |= n + 1 == kutos_well_start;
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
        if (++since_long_rest >= 3 || wounded(*party, 0.5))
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
            std::cerr << "Usage: opengold_campaign_sim GAME_DIR [RUNS [OUT_DIR [PARTY]]]\n";
            return 2;
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
        std::ofstream detail(out / "runs.csv"), summary(out / "summary.csv");
        std::ofstream usage_csv(out / "usage.csv");
        detail << "party,run,outcome,fights_won,lost_at,deaths,dead_classes,short_rests,"
               "long_rests,levels\n";
        summary << "party,runs,slums_pct,success_pct,flawless_pct,avg_fights_won,avg_deaths,"
                   "avg_level\n";
        usage_csv << "party,class,command,count\n";
        std::printf("%-14s %5s %7s %8s %9s %10s %7s %9s\n", "party", "runs", "slums%", "success%",
                    "flawless%", "fights_won", "deaths", "avg_level");
        for (const auto &plan : party_plans(*characters))
        {
            if (!only.empty() && plan.name != only)
                continue;
            // Each class's pool characters in turn, so a party of six of one
            // class has its four variants.
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
            unsigned cleared = 0, complete = 0, flawless = 0, fights = 0, deaths = 0, levels = 0,
                     people = 0;
            Usage usage;
            for (unsigned run = 1; run <= runs; ++run)
            {
                const auto r = play(members, maps, run, usage);
                cleared += r.cleared_slums;
                complete += r.outcome == "complete";
                flawless += r.outcome == "complete" && !r.deaths;
                fights += r.fights_won;
                deaths += r.deaths;
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
                       << r.short_rests << ',' << r.long_rests << ',' << level_list << '\n';
            }
            for (const auto &[key, count] : usage)
                usage_csv << plan.name << ',' << key.first << ',' << key.second << ',' << count << '\n';
            const double success = 100.0 * complete / runs;
            const double slums_success = 100.0 * cleared / runs;
            summary << plan.name << ',' << runs << ',' << slums_success << ',' << success << ','
                    << 100.0 * flawless / runs << ',' << double(fights) / runs << ','
                    << double(deaths) / runs << ',' << double(levels) / people << '\n';
            std::printf("%-14s %5u %7.0f %8.0f %9.0f %10.1f %7.2f %9.2f\n", plan.name.c_str(),
                        runs, slums_success, success, 100.0 * flawless / runs,
                        double(fights) / runs, double(deaths) / runs, double(levels) / people);
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
