// Replays the original Slums roaming encounters through the campaign combat path
// to compare creature conversions and party-strength scaling. The encounter mix
// follows ECL2.DAX record 20: base count from party strength (x2/3 on streets,
// x1 in the Old Rope Guild), three or four leaders from strength 8 (14 for orcs)
// and a bugbear above 18. Both sides use the automated demo policy.
#include "opengold/campaign_party.h"
#include "opengold/character_creator.h"
#include "opengold/combat_demo.h"
#include "opengold/dungeon_battlefield.h"
#include "opengold/map_catalog.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>

using namespace opengold;
using por::GeoMap;
using por::MapCatalog;

namespace
{
// The content pack to test; an alternative pack compares candidate conversions.
std::filesystem::path content_pack =
    std::filesystem::path(OPENGOLD_SOURCE_DIR) / "data/rules/srd-5.2.1/combat.rules";

std::unique_ptr<rules::RulesModule> module()
{
    return srd5::load(content_pack);
}

// The expedition test's fighter: best rolls to Strength, Constitution, Dexterity.
Character fighter(std::string name, std::uint64_t seed)
{
    CharacterCreator creator(srd5::character_rules(), seed);
    creator.select(rules::CreationField::race, "human");
    creator.select(rules::CreationField::character_class, "fighter");
    creator.roll();
    creator.name(std::move(name));
    std::array<unsigned, 6> best{0, 1, 2, 3, 4, 5};
    const auto &rolls = creator.draft().rolls;
    std::stable_sort(best.begin(), best.end(), [&](unsigned a, unsigned b)
    {
        return rolls[a].total() > rolls[b].total();
    });
    constexpr std::array<unsigned, 6> priority{0, 2, 1, 4, 5, 3};
    for (unsigned i = 0; i < 6; ++i)
        creator.assign_roll(best[i], priority[i]);
    return Character(creator.rules(), creator.draft(), creator.appearance());
}

// Six created fighters in long sword, chain mail and shield, as bought in New Phlan.
std::shared_ptr<CampaignParty> party_of(unsigned members, unsigned level)
{
    auto party = std::make_shared<CampaignParty>(module());
    const std::array<const char *, 6> names{"Arden", "Bryn", "Cora", "Darin", "Elin", "Fenn"};
    for (unsigned n = 0; n < members; ++n)
    {
        auto person = fighter(names[n], n + 1);
        auto &items = person.inventory();
        const auto sword = items.add("longsword", "Long sword", 1, 36);
        const auto mail = items.add("chain_mail", "Chain mail", 1, 55);
        const auto shield = items.add("shield", "Shield", 1, 59);
        const auto id = party->add_pc(std::move(person));
        for (const auto item : {sword, mail, shield})
            party->equip(id, item);
    }
    if (level == 2)
    {
        party->award_experience(300, "balance:level-two");
        for (const auto id : party->state().slots)
            if (id)
                party->advance(id, party->default_advancement(id));
    }
    return party;
}

struct Group
{
    std::string definition;
    unsigned count{};
};

const char *definition(unsigned record)
{
    switch (record)
    {
    case 0:
        return "slums-kobold";
    case 1:
        return "slums-kobold-leader";
    case 2:
        return "slums-goblin";
    case 3:
        return "slums-goblin-leader";
    case 4:
        return "slums-orc";
    case 5:
        return "slums-orc-leader";
    default:
        return "slums-bugbear";
    }
}

// ECL2:20 0x9b68-0x9b81 (streets) or 0xadde (Rope Guild), then 0xb157-0xb20b.
std::vector<Group> encounter(unsigned record, unsigned strength, bool rope_guild)
{
    const unsigned scaled = rope_guild ? strength : strength / 3 * 2;
    std::vector<Group> groups;
    const unsigned leader_threshold = record == 4 ? 14 : 8;
    if (scaled >= leader_threshold)
        groups.push_back({definition(record + 1), record < 1 ? 3u : 4u});
    if (scaled > 18)
        groups.push_back({definition(63), 1});
    groups.push_back({definition(record), scaled});
    return groups;
}

struct Tally
{
    unsigned fights{}, wins{}, deaths{}, down{};
    double hp_left{};
};

// A map cell, in GEO coordinates.
struct Place
{
    unsigned x{}, y{};
};

Tally play(const PartyState &start, const std::vector<Group> &groups, const GeoMap &map,
           Place cell, unsigned seeds)
{
    Tally tally;
    for (unsigned seed = 1; seed <= seeds; ++seed)
    {
        auto party = std::make_shared<CampaignParty>(module());
        party->restore(start);
        CampaignEncounter fight;
        fight.field = por::dungeon_battlefield(map, cell.x, cell.y);
        rules::EntityId next = 1000;
        for (const auto &group : groups)
            for (unsigned n = 0; n < group.count; ++n, ++next)
                fight.enemies.push_back(
                {next, group.definition, group.definition + " " + std::to_string(next), 1, {}});
        CombatDemo combat(module());
        combat.campaign_party(party);
        combat.encounter(fight, seed);
        for (unsigned n = 0;
                n < 20000 && combat.combat().snapshot().outcome == rules::Outcome::ongoing; ++n)
            if (!combat.submit(choose_demo_command(combat.combat())))
                throw std::runtime_error("Combat refused the demo command");
        const auto result = combat.combat().snapshot();
        ++tally.fights;
        tally.wins += result.outcome == rules::Outcome::victory;
        int hp = 0, max_hp = 0;
        for (const auto &unit : result.combatants)
            if (unit.side == 0)
            {
                tally.deaths += unit.dead;
                tally.down += !unit.dead && unit.hit_points == 0;
                hp += unit.hit_points;
                max_hp += unit.max_hit_points;
            }
        tally.hp_left += double(hp) / max_hp;
    }
    return tally;
}
} // namespace

int main(int argc, char **argv)
{
    try
    {
        if (argc < 2)
        {
            std::cerr << "Usage: opengold_encounter_balance GAME_DIR [SEEDS [CONTENT_PACK]]\n";
            return 2;
        }
        const unsigned seeds = argc > 2 ? unsigned(std::stoul(argv[2])) : 40;
        if (argc > 3)
            content_pack = argv[3];
        const auto maps = MapCatalog::load(argv[1]);
        const auto slums = maps.find({"GEO2.DAX", 20});
        if (!slums)
            throw std::runtime_error("Missing GEO2:20");
        std::printf("level members context  monsters factor strength  enemies                              "
                    "win%%  deaths down  hp_left%%\n");
        for (const unsigned level : {1u, 2u})
            for (const unsigned members : {6u})
            {
                const auto party = party_of(members, level);
                const auto start = party->checkpoint();
                const unsigned strength = party->strength();
                for (const bool rope_guild : {false, true})
                    for (const unsigned record : {0u, 2u, 4u})
                        for (const double factor : {1.0, 0.75, 0.67, 0.5})
                        {
                            const auto scaled = unsigned(std::lround(strength * factor));
                            const auto groups = encounter(record, scaled, rope_guild);
                            std::string mix;
                            for (const auto &g : groups)
                                mix += std::to_string(g.count) + " " + g.definition.substr(6) + " ";
                            // Where the route met each kind of fight.
                            const Place cell = rope_guild ? Place{6, 14} : Place{14, 7};
                            const auto t = play(start, groups, slums->get(), cell, seeds);
                            std::printf("%5u %7u %-8s %-8s %6.2f %8u  %-36s %4.0f %7.2f %4.2f %8.0f\n",
                                        level, members, rope_guild ? "guild" : "street",
                                        definition(record) + 6, factor, scaled, mix.c_str(),
                                        100.0 * t.wins / t.fights, double(t.deaths) / t.fights,
                                        double(t.down) / t.fights, 100.0 * t.hp_left / t.fights);
                            std::fflush(stdout);
                        }
            }
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
