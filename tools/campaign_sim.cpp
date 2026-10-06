// Plays the Slums as a campaign arc many times to estimate how often a party
// succeeds. A level-one party from the curated character pool, in its class
// kits, fights the original roaming encounters and the four-orc search in a
// fixed order, rests between fights and gains levels up to four. Both sides
// use the automated demo policy. Each seed is one run; results go to CSV.
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
    default:
        return {63, "slums-bugbear", 200};
    }
}

// Experience as the campaign awards it: the original encounter's, per member.
unsigned original_xp(unsigned record)
{
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

struct Fight
{
    const char *label;
    unsigned record; // 13: the four-orc search event
    bool guild;
};

// The arc: the Slums' kobolds, goblins and orcs on the streets and in the Old
// Rope Guild, with the four-orc search second.
constexpr std::array arc{
    Fight{"street kobolds", 0, false}, Fight{"four orcs", 13, false},
    Fight{"street goblins", 2, false}, Fight{"street orcs", 4, false},
    Fight{"guild kobolds", 0, true},   Fight{"street goblins", 2, false},
    Fight{"guild goblins", 2, true},   Fight{"street orcs", 4, false},
    Fight{"guild orcs", 4, true},      Fight{"street goblins", 2, false},
    Fight{"guild orcs", 4, true},      Fight{"guild orcs", 4, true}};

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

RunResult play(const std::vector<Character> &members, const por::GeoMap &slums,
               std::uint64_t seed, Usage &usage)
{
    auto party = std::make_shared<CampaignParty>(module());
    for (const auto &member : members)
        outfit_pool_member(*party, party->add_pc(member));
    RunResult result;
    unsigned since_long_rest = 0;
    for (std::size_t n = 0; n < arc.size(); ++n)
    {
        const auto &step = arc[n];
        std::vector<Group> groups =
            step.record == 13 ? std::vector<Group> {{creature(13), 1}, {creature(4), 3}}
            : roaming(step.record, party->strength(), step.guild);
        unsigned experience = 0;
        for (const auto &group : groups)
            experience += group.count * original_xp(group.kind.record);
        // The session fits roaming groups to the XP budget and to one creature
        // per living character; the four-orc search keeps its four.
        if (step.record != 13)
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
        fight.field = step.guild ? por::dungeon_battlefield(slums, 6, 14)
                      : por::dungeon_battlefield(slums, 14, 7);
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
        const auto maps = por::MapCatalog::load(game);
        const auto slums = maps.find({"GEO2.DAX", 20});
        if (!slums)
            throw std::runtime_error("Missing GEO2:20");
        const auto characters = srd5::character_rules();
        const auto pool = character_pool(*characters, por::CharacterArt::load(game));
        std::ofstream detail(out / "runs.csv"), summary(out / "summary.csv");
        std::ofstream usage_csv(out / "usage.csv");
        detail << "party,run,outcome,fights_won,lost_at,deaths,dead_classes,short_rests,"
               "long_rests,levels\n";
        summary << "party,runs,success_pct,flawless_pct,avg_fights_won,avg_deaths,avg_level\n";
        usage_csv << "party,class,command,count\n";
        std::printf("%-14s %5s %8s %9s %10s %7s %9s\n", "party", "runs", "success%", "flawless%",
                    "fights_won", "deaths", "avg_level");
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
            unsigned complete = 0, flawless = 0, fights = 0, deaths = 0, levels = 0, people = 0;
            Usage usage;
            for (unsigned run = 1; run <= runs; ++run)
            {
                const auto r = play(members, slums->get(), run, usage);
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
            summary << plan.name << ',' << runs << ',' << success << ','
                    << 100.0 * flawless / runs << ',' << double(fights) / runs << ','
                    << double(deaths) / runs << ',' << double(levels) / people << '\n';
            std::printf("%-14s %5u %8.0f %9.0f %10.1f %7.2f %9.2f\n", plan.name.c_str(), runs,
                        success, 100.0 * flawless / runs, double(fights) / runs,
                        double(deaths) / runs, double(levels) / people);
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
