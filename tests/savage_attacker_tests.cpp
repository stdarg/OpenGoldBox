#include "campaign_fixture.h"
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

SavageAttackChoice offer(const CombatSession &c)
{
    const auto s = c.snapshot();
    check(s.savage_attack_choice.has_value(), "Expected damage choice");
    return *s.savage_attack_choice;
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
    return rules.create({{8, 8, std::vector<std::uint8_t>(64)},
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
          "Pending choice, RNG and costs round trip exactly");
}

void choices()
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
        const auto attack = command(*c, "melee");
        check(c->submit(attack), "Ordinary weapon attack hits");
        const auto first = offer(*c);
        check(first.first_damage == 9 && !first.second_damage && first.dice_count == 2 &&
              first.dice_sides == 6 && first.modifier == 3 && !first.critical &&
              first.weapon == "Greatsword",
              "Seed 13 rolls 2+4 weapon dice, plus 3 once");
        check(unit(*c, 99).hit_points == 1000 && !unit(*c).action &&
              unit(*c).reaction == before.reaction &&
              unit(*c).bonus_action == before.bonus_action &&
              unit(*c).movement_feet == before.movement_feet &&
              unit(*c).persistent == before.persistent,
              "First hit spends Action only and postpones all damage");
        check(c->legal_commands().size() == 2 && c->movement_reach(1).empty(),
              "Only the first-stage decision is legal");
        roundtrip(*rules, *c);
        const auto first_bytes = c->save();
        const auto random = rng(*c);
        check(!c->submit(attack) && c->save() == first_bytes,
              "Stale hit cannot reroll or spend twice");
        auto bad = command(*c, "savage_use");
        bad.target = 1;
        check(!c->submit(bad) && c->save() == first_bytes,
              "Wrong-target decision rejects atomically");
        auto skip = rules->restore(first_bytes);
        act(*skip, "savage_skip");
        check(!skip->snapshot().savage_attack_choice && unit(*skip, 99).hit_points == 991 &&
              rng(*skip) == random,
              "Declining applies first damage without consuming extra dice");
        const auto use = command(*c, "savage_use");
        check(c->submit(use), "Player elects to spend feat");
        const auto second = offer(*c);
        check(second.first_damage == 9 && second.second_damage == 10 &&
              unit(*c, 99).hit_points == 1000 && rng(*c) == random + 2 * 0x9e3779b97f4a7c15ULL,
              "Only the second set of weapon dice is rolled; damage still waits");
        roundtrip(*rules, *c);
        const auto second_bytes = c->save();
        check(!c->submit(use) && c->save() == second_bytes, "Repeated use cannot reroll again");
        auto lower = rules->restore(second_bytes);
        act(*lower, "savage_first");
        check(unit(*lower, 99).hit_points == 991 && !lower->snapshot().savage_attack_choice,
              "Player can retain a lower first result");
        act(*c, "savage_second");
        check(unit(*c, 99).hit_points == 990 && !c->snapshot().savage_attack_choice,
              "Player can use the second result");
        check(rng(*lower) == rng(*c) && c->snapshot().elapsed_milliseconds == 0,
              "Picking either result consumes no additional RNG or time");
        auto ranged = battle(*rules, h, "longbow", 0, true);
        act(*ranged, "ranged");
        check(offer(*ranged).critical && offer(*ranged).dice_count == 2 &&
              offer(*ranged).first_damage == 12,
              "Critical weapon dice double, flat modifier does not");
        act(*ranged, "savage_use");
        check(offer(*ranged).second_damage == 8, "Critical reroll uses 3+2 plus 3 once");
        act(*ranged, "savage_second");
        check(unit(*ranged, 99).hit_points == 992, "Lower second critical roll is a real choice");
    }
    check(count == 12, "All twelve classes receive the Soldier feat's real decisions");
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
            check(!close->snapshot().savage_attack_choice,
                  "Unarmed Strike has no weapon dice to reroll");
        }
        else
            check(!c->snapshot().savage_attack_choice && unit(*c, 99).hit_points == 999,
                  "Fixed Blowgun damage has no damage dice");
    }
    auto c = battle(*rules, h, "greatsword", 40);
    const auto before = rng(*c);
    act(*c, "melee");
    check(!c->snapshot().savage_attack_choice && unit(*c, 99).hit_points == 1000 &&
          rng(*c) == before + 0x9e3779b97f4a7c15ULL,
          "Miss uses only its attack roll and never offers feat");
    c = battle(*rules, hero("wizard"), "greatsword", 13, true);
    act(*c, "fire_bolt");
    check(!c->snapshot().savage_attack_choice,
          "Spell damage is excluded even while holding a weapon");
    c = battle(*rules, hero("fighter", "sage"));
    act(*c, "melee");
    check(!c->snapshot().savage_attack_choice, "No entitlement means no choice");
    c = battle(*rules, h, "longsword");
    act(*c, "grip_two");
    act(*c, "melee");
    check(offer(*c).dice_sides == 10 && offer(*c).first_damage == 11,
          "Versatile uses the chosen grip's weapon dice");
    act(*c, "savage_use");
    act(*c, "savage_first");
    act(*c, "end");
    auto move = command(*c, "move");
    move.destination = {3, 1};
    check(c->submit(move) && c->snapshot().reaction_pending,
          "Enemy movement triggers next-turn reaction");
    const auto movement = c->snapshot();
    act(*c, "opportunity");
    check(c->snapshot().savage_attack_choice.has_value() && !unit(*c).reaction &&
          !unit(*c).action &&
          c->snapshot().elapsed_milliseconds == movement.elapsed_milliseconds,
          "Feat refreshes each turn, including enemy turns, without refreshing Action");
    roundtrip(*rules, *c);
    const auto cell = unit(*c, 99).cell;
    check(cell == Cell{2, 1}, "Movement waits before leaving reach");
    act(*c, "savage_use");
    roundtrip(*rules, *c);
    act(*c, "savage_first");
    check(!c->snapshot().reaction_pending && unit(*c, 99).cell == Cell{3, 1} && !unit(*c).reaction,
          "Resolving damage resumes movement once, Reaction remains spent");
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
            {8, 8, std::vector<std::uint8_t>(64)},
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
        check(c->snapshot().savage_attack_choice.has_value(), "First reactor makes feat decision");
        roundtrip(*rules, *c);
        act(*c, "savage_skip");
        if (lethal)
        {
            check(c->snapshot().outcome == Outcome::victory && !c->snapshot().reaction_pending &&
                  unit(*c, 99).cell == Cell{2, 1},
                  "Lethal chosen damage cancels movement and later reactions");
            roundtrip(*rules, *c);
        }
        else
        {
            check(c->snapshot().reaction_pending && c->snapshot().actor == 2,
                  "Next reactor is offered only after first damage resolves");
            act(*c, "opportunity");
            roundtrip(*rules, *c);
            act(*c, "savage_skip");
            check(!c->snapshot().reaction_pending && unit(*c, 99).cell == Cell{3, 2},
                  "All decisions finish before the movement step");
        }
    }
}

void defenses()
{
    auto rules = module(true, "affinity target ward resistance slashing\n");
    const auto h = hero();
    const auto profile =
        rules->character_profile(h.sheet(), std::array<std::string, 1> {"greatsword"}).data;
    auto c =
    rules->create({{8, 8, std::vector<std::uint8_t>(64)},
        {   {1, "campaign-character", "Hero", 0, {1, 1}, profile},
            {
                99,
                "target",
                "Target",
                1,
                {2, 1},
                "",
                VitalState{1000, false, "SRD7 0 0 0 0 0 0 0 0 0 3 \"ward\" 0 FX1 1 0"}
            }
        }},
    13);
    act(*c, "melee");
    check(offer(*c).first_damage == 9 && unit(*c, 99).temporary_hp.amount == 3,
          "Choice shows pre-defense damage without consuming the buffer");
    act(*c, "savage_use");
    roundtrip(*rules, *c);
    act(*c, "savage_first");
    check(unit(*c, 99).hit_points == 999 && unit(*c, 99).temporary_hp.amount == 0,
          "Chosen 9 rounds to 4 after resistance, then absorbs 3 Temporary HP");
}

void invalid()
{
    auto rules = module();
    auto c = battle(*rules, hero());
    act(*c, "melee");
    const auto saved = c->save();
    const auto line = saved.rfind('\n', saved.size() - 2) + 1;
    for (const auto &fields :
            {"0 99 0 17 0 9 -1", "1 1 0 17 0 9 -1", "1 99 0 1 0 9 -1",
             "1 99 0 17 1 9 -1", "1 99 0 17 0 2 -1", "1 99 0 17 0 99 -1",
             "1 99 0 17 0 9 -2", "1 99 0 17 0 9 10", "1 99 1 17 0 9 -1"
            })
    {
        const auto bad = saved.substr(0, line) + fields + '\n';
        rejects(
            [&]
        {
            (void)rules->restore(bad);
        });
        check(c->save() == saved, "Malformed pending state leaves live combat intact");
    }
    auto choice = command(*c, "savage_use");
    choice.verb = "savage_second";
    check(!c->submit(choice) && c->save() == saved, "Cannot choose nonexistent second roll");
    act(*c, "savage_use");
    const auto used = c->save();
    auto bad = used;
    bad.replace(bad.rfind(' ') + 1, std::string::npos, "999\n");
    rejects(
        [&]
    {
        (void)rules->restore(bad);
    });
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
        check(c->snapshot().savage_attack_choice.has_value(),
              "Normal selected feat affects next encounter");
        auto repeated = party.default_advancement(id);
        check(!party.can_advance(id), "Existing level band cannot invent another entitlement");
    }
}

std::string upgraded(std::string bytes)
{
    const auto at = bytes.find("0.6.19");
    check(at != bytes.npos, "Actual prior writer identity");
    bytes.replace(at, 6, module(false)->identity().version);
    bytes.replace(9, 2, "13");
    return bytes + "0\n";
}

void legacy()
{
    auto rules = module(false);
    const auto base = root / "tests/fixtures";
    const auto old = read(base / "campaign-v10-savage.ogs");
    CampaignParty p(module(false));
    p.restore(
        decode_campaign(old, *srd5::character_rules(), *rules, "savage-fixture", nullptr).party);
    auto expected = old.substr(old.find('\n', old.find('\n') + 1) + 1);
    expected.replace(expected.find("0.6.19"), 6, rules->identity().version);
    const auto bytes = encode_campaign(p, nullptr, "savage-fixture");
    check(
        bytes.substr(bytes.find('\n', bytes.find('\n') + 1) + 1) ==
        test::with_background_training_grants(test::with_action_surge_grants(
                    test::with_legacy_cantrip_choices(expected), {false, true})),
        "Only the justified Action Surge and background grants are added; existing equipment, wounds, pools and clocks unchanged");
    for (const auto suffix :
            {"", "-reaction"
            })
    {
        auto c = rules->restore(read(base / (std::string("combat-v12-savage") + suffix + ".save")));
        check(c->save() ==
              upgraded(read(base / (std::string("combat-v12-savage") + suffix + ".save"))),
              "Old checkpoint adds only empty decision and identities");
        act(*c, *suffix ? "opportunity" : "melee");
        act(*c, "savage_use");
        act(*c,
            offer(*c).first_damage >= *offer(*c).second_damage ? "savage_first" : "savage_second");
        auto prior = rules->restore(
                         read(base / (std::string("combat-v12-savage") + suffix + "-continued.save")));
        check(rng(*c) == rng(*prior) &&
              c->snapshot().elapsed_milliseconds == prior->snapshot().elapsed_milliseconds &&
              c->snapshot().outcome == prior->snapshot().outcome,
              "Explicit higher-result choice matches old automatic RNG/time/outcome");
        for (auto id :
                {
                    1u, 99u
                })
        {
            const auto a = unit(*c, id), b = unit(*prior, id);
            check(a.hit_points == b.hit_points && a.persistent == b.persistent &&
                  a.cell == b.cell && a.action == b.action &&
                  a.bonus_action == b.bonus_action && a.reaction == b.reaction &&
                  a.movement_feet == b.movement_feet,
                  "Old writer's damage, resources and movement continue exactly");
        }
    }
}

void fixtures(const std::filesystem::path &path)
{
    std::filesystem::create_directories(path);
    auto rules = module(false);
    const auto h = hero();
    const auto profile =
        rules->character_profile(h.sheet(), std::array<std::string, 1> {"greatsword"}).data;
    auto c = rules->create({{8, 8, std::vector<std::uint8_t>(64)},
        {   {1, "campaign-character", "Hero", 0, {1, 1}, profile},
            {99, "vanguard", "Target", 1, {2, 1}}
        }},
    13);
    act(*c, "melee");
    std::ofstream(path / "savage-first.save") << c->save();
    act(*c, "savage_use");
    std::ofstream(path / "savage-second.save") << c->save();
}
} // namespace

int main(int argc, char **argv)
{
    try
    {
        choices();
        exceptions_and_turns();
        lethal_and_queues();
        defenses();
        invalid();
        grants_and_campaign();
        legacy();
        if (argc == 2)
            fixtures(argv[1]);
        std::cout << "Savage Attacker tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
