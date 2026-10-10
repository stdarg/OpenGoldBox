#include "opengold/campaign_save.h"
#include "opengold/combat_demo.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace opengold;
using namespace opengold::rules;

namespace
{
void check(bool ok, const char *why)
{
    if (!ok)
        throw std::runtime_error(why);
}

template <class F> void rejects(F f)
{
    bool caught = false;
    try
    {
        f();
    }
    catch (const std::exception &)
    {
        caught = true;
    }
    check(caught, "Malformed Savage Attacker state must reject");
}

const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR);

std::string read(const std::filesystem::path &p)
{
    std::ifstream in(p);
    check(bool(in), "Fixture exists");
    return {std::istreambuf_iterator<char>(in), {}};
}

auto module(bool target = true, std::string affinity = {})
{
    auto text = read(root / "data/rules/srd-5.2.1/combat.rules");
    if (target)
        text += "\ncreature target 1 1000 0 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n";
    return srd5::parse_content(text + affinity);
}

Character hero(std::string klass = "fighter", std::string background = "soldier")
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = klass;
    d.background = background;
    d.alignment = "neutral_good";
    d.name = "Savage tester";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    return Character(*srd5::character_rules(), d, {});
}

Command command(const CombatSession &c, std::string_view verb)
{
    for (const auto &a : c.legal_commands())
        if (a.verb == verb)
            return a;
    throw std::runtime_error("Missing command: " + std::string(verb));
}

void act(CombatSession &c, std::string_view verb)
{
    check(c.submit(command(c, verb)), "Command accepted");
}

CombatantView unit(const CombatSession &c, EntityId id = 1)
{
    for (const auto &a : c.snapshot().combatants)
        if (a.id == id)
            return a;
    throw std::runtime_error("Missing actor");
}

std::uint64_t rng(const CombatSession &c)
{
    std::istringstream in(c.save());
    std::string row;
    for (int n = 0; n < 3; ++n)
        std::getline(in, row);
    std::uint64_t r{};
    in >> r;
    return r;
}

auto battle(const RulesModule &rules, const Character &h, std::string gear = "greatsword",
            unsigned seed = 13, bool ranged = false)
{
    std::vector<std::string> equipment;
    if (!gear.empty())
        equipment.push_back(gear);
    return rules.create({{8, 8, std::vector<Terrain>(64)},
        {   {
                1,
                "campaign-character",
                "Hero",
                0,
                {1, 1},
                rules.character_profile(h.sheet(), equipment).data
            },
            {99, "target", "Target", 1, ranged ? Cell{4, 1} : Cell{2, 1}}
        }},
    seed);
}

void roundtrip(const RulesModule &r, const CombatSession &c)
{
    check(r.restore(c.save())->save() == c.save(),
          "Checkpoint, RNG and costs round trip exactly");
}

bool logged(const CombatSession &c, std::string_view text)
{
    const auto log = c.snapshot().log();
    return std::any_of(log.begin(), log.end(), [&](const auto & line)
    {
        return line.find(text) != std::string::npos;
    });
}

bool rerolled(const CombatSession &c)
{
    return logged(c, "rerolls weapon damage (Savage Attacker)");
}

void automatic_higher_roll()
{
    auto rules = module();
    unsigned count = 0;
    for (const auto &klass : srd5::character_rules()->choices(CreationField::character_class))
    {
        ++count;
        const auto h = hero(klass.id);
        check(h.sheet().modifiers[0] == 3 && h.sheet().modifiers[1] == 3,
              "Independent Soldier ability oracle");
        auto c = battle(*rules, h);
        const auto before = unit(*c);
        const auto random = rng(*c);
        act(*c, "melee");
        check(logged(*c, "Hero rerolls weapon damage (Savage Attacker): 9 and 10, keeps 10."),
              "Seed 13 rolls 2+4 then 3+4 weapon dice plus 3 once; the log shows both");
        check(unit(*c, 99).hit_points == 990 && !unit(*c).action &&
              unit(*c).reaction == before.reaction &&
              unit(*c).bonus_action == before.bonus_action &&
              unit(*c).movement_feet == before.movement_feet,
              "The higher roll applies at once and the hit spends only the Action");
        check(rng(*c) == random + 5 * 0x9e3779b97f4a7c15ULL &&
              c->snapshot().elapsed_milliseconds == 0,
              "Attack roll, both weapon dice sets and nothing else consume RNG");
        const auto &message = c->snapshot().log_messages();
        check(std::any_of(message.begin(), message.end(), [](const auto & m)
        {
            return m.source ==
                   "{name} rerolls weapon damage (Savage Attacker): {first} and {second}, keeps {kept}.";
        }), "The reroll is a localizable log message");
        roundtrip(*rules, *c);
        auto ranged = battle(*rules, h, "longbow", 0, true);
        act(*ranged, "ranged");
        check(logged(*ranged, "Hero rerolls weapon damage (Savage Attacker): 12 and 8, keeps 12.") &&
              unit(*ranged, 99).hit_points == 988,
              "Critical dice double in both sets, modifier once; the lower reroll is ignored");
    }
    check(count == 12, "All twelve classes receive the Soldier feat automatically");
}

void exceptions_and_turns()
{
    auto rules = module();
    const auto h = hero();
    for (const auto &gear :
            {"", "blowgun"
            })
    {
        auto c = battle(*rules, h, gear, 13, true);
        act(*c, *gear ? "ranged" : "end");
        if (!*gear)
        {
            act(*c, "end");
            auto close = battle(*rules, h, gear);
            act(*close, "melee");
            check(!rerolled(*close), "Unarmed Strike has no weapon dice to reroll");
        }
        else
            check(!rerolled(*c) && unit(*c, 99).hit_points == 999,
                  "Fixed Blowgun damage has no damage dice");
    }
    auto c = battle(*rules, h, "greatsword", 40);
    const auto before = rng(*c);
    act(*c, "melee");
    check(!rerolled(*c) && unit(*c, 99).hit_points == 1000 &&
          rng(*c) == before + 0x9e3779b97f4a7c15ULL,
          "Miss uses only its attack roll and never uses the feat");
    c = battle(*rules, hero("wizard"), "greatsword", 13, true);
    act(*c, "fire_bolt");
    check(!rerolled(*c), "Spell damage is excluded even while holding a weapon");
    c = battle(*rules, hero("fighter", "sage"));
    act(*c, "melee");
    check(!rerolled(*c), "No entitlement means no reroll");
    c = battle(*rules, h, "longsword");
    act(*c, "melee");
    check(logged(*c, "(Savage Attacker): 11 and "),
          "A Versatile weapon with an empty other hand rerolls its two-handed die");
    const auto first_turn = c->snapshot().log().size();
    act(*c, "end");
    auto move = command(*c, "move");
    move.destination = {3, 1};
    check(c->submit(move) && c->snapshot().reaction_pending,
          "Enemy movement triggers next-turn reaction");
    act(*c, "opportunity");
    const auto log = c->snapshot().log();
    check(std::any_of(log.begin() + first_turn, log.end(), [](const auto & line)
    {
        return line.find("(Savage Attacker)") != std::string::npos;
    }) && !unit(*c).reaction && !unit(*c).action,
    "Feat refreshes each turn, including enemy turns, without refreshing Action");
    roundtrip(*rules, *c);
    check(!c->snapshot().reaction_pending && unit(*c, 99).cell == Cell{3, 1} && !unit(*c).reaction,
          "Damage resolves at once and movement resumes, Reaction remains spent");
}

void lethal_and_queues()
{
    auto rules = module();
    const auto h = hero();
    const auto profile =
        rules->character_profile(h.sheet(), std::array<std::string, 1> {"greatsword"}).data;
    for (bool lethal :
            {
                false, true
            })
    {
        auto c = rules->create(
        {
            {8, 8, std::vector<Terrain>(64)},
            {   {1, "campaign-character", "First", 0, {1, 1}, profile},
                {2, "campaign-character", "Second", 0, {2, 0}, profile},
                {99, "target", "Target", 1, {2, 1}, "", VitalState{lethal ? 1 : 1000, false, {}}}
            }},
        13);
        while (c->snapshot().actor != 99)
            act(*c, "end");
        bool moved = false;
        for (const auto &a : c->legal_commands())
            if (a.verb == "move" && a.destination == Cell{3, 2})
            {
                moved = c->submit(a);
                break;
            }
        check(moved && c->snapshot().reaction_pending, "Multiple reactors are queued");
        act(*c, "opportunity");
        check(rerolled(*c), "First reactor rerolls automatically");
        roundtrip(*rules, *c);
        if (lethal)
        {
            check(c->snapshot().outcome == Outcome::victory && !c->snapshot().reaction_pending &&
                  unit(*c, 99).cell == Cell{2, 1},
                  "Lethal damage cancels movement and later reactions");
            continue;
        }
        check(c->snapshot().reaction_pending && c->snapshot().actor == 2,
              "Next reactor is offered once the first hit's damage resolves");
        act(*c, "opportunity");
        roundtrip(*rules, *c);
        check(!c->snapshot().reaction_pending && unit(*c, 99).cell == Cell{3, 2},
              "All reactions finish before the movement step");
    }
}

void defenses()
{
    auto rules = module(true, "affinity target ward resistance slashing\n");
    const auto h = hero();
    const auto profile =
        rules->character_profile(h.sheet(), std::array<std::string, 1> {"greatsword"}).data;
    auto c =
    rules->create({{8, 8, std::vector<Terrain>(64)},
        {   {1, "campaign-character", "Hero", 0, {1, 1}, profile},
            {
                99,
                "target",
                "Target",
                1,
                {2, 1},
                "",
                VitalState{1000, false, "SRD11 0 0 0 0 0 0 0 0 0 3 \"ward\" 0 0 0 0 0 0 FX8 1 0 0"}
            }
        }},
    13);
    act(*c, "melee");
    check(logged(*c, "9 and 10, keeps 10.") && unit(*c, 99).hit_points == 998 &&
          unit(*c, 99).temporary_hp.amount == 0,
          "Kept 10 halves to 5 after resistance, then 3 Temporary HP absorb part of it");
}

void grants_and_campaign()
{
    auto rules = module();
    for (const auto &klass :
            {"fighter", "cleric", "wizard"
            })
    {
        CampaignParty party(module());
        const auto id = party.add_pc(hero(klass, "sage"));
        party.award_experience(2700, "savage-xp");
        for (int n = 2; n <= 3; ++n)
            party.advance(id, party.default_advancement(id));
        auto choice = party.default_advancement(id);
        choice.feat = "savage_attacker";
        choice.abilities = {};
        const auto before = encode_campaign(party, nullptr, "savage");
        const auto preview = party.preview_advancement(id, choice);
        check(encode_campaign(party, nullptr, "savage") == before, "Preview is isolated");
        party.advance(id, choice);
        const auto &sheet = party.member(id).character.sheet();
        const FeatureGrant grant{"feat:savage_attacker",
                                 std::string("class:") + klass + ":ability_score_improvement",
                                 4,
                                 {}};
        check(std::find(sheet.grants.begin(), sheet.grants.end(), grant) != sheet.grants.end() &&
              sheet.grants == preview.character.sheet().grants,
              "Selected feat retains real entitlement and acquisition level");
        auto broken = sheet;
        broken.grants.push_back(grant);
        rejects(
            [&]
        {
            (void)rules->character_profile(broken, {});
        });
        broken = sheet;
        broken.grants.back().source_id = "background:soldier";
        rejects(
            [&]
        {
            (void)rules->character_profile(broken, {});
        });
        const auto bytes = encode_campaign(party, nullptr, "savage");
        CampaignParty copy(module());
        copy.restore(
            decode_campaign(bytes, *srd5::character_rules(), *rules, "savage", nullptr).party);
        check(encode_campaign(copy, nullptr, "savage") == bytes,
              "Campaign grants and all state persist");
        auto c = battle(*rules, copy.member(id).character);
        act(*c, "melee");
        check(rerolled(*c), "Normal selected feat affects next encounter");
        auto repeated = party.default_advancement(id);
        check(!party.can_advance(id), "Existing level band cannot invent another entitlement");
    }
}
} // namespace

int main()
{
    try
    {
        automatic_higher_roll();
        exceptions_and_turns();
        lethal_and_queues();
        defenses();
        grants_and_campaign();
        std::cout << "Savage Attacker tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
