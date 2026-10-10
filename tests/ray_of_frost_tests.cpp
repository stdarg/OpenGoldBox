#include "opengold/campaign_save.h"
#include "opengold/srd5.h"
#include "status_effects.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace opengold;
using namespace opengold::rules;
namespace fx = opengold::srd5::detail;

namespace
{
const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR);

void check(bool v, const char *m)
{
    if (!v)
        throw std::runtime_error(m);
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
    check(caught, "Malformed state must reject");
}

std::string read(const std::filesystem::path &p)
{
    std::ifstream f(p);
    check(bool(f), "Read fixture");
    return {std::istreambuf_iterator<char>(f), {}};
}

auto module()
{
    return srd5::load(root / "data/rules/srd-5.2.1/combat.rules");
}

auto custom(std::string affinity = {}, int speed = 30)
{
    return srd5::parse_content(read(root / "data/rules/srd-5.2.1/combat.rules") +
                               "\ncreature target 1 1000 0 " + std::to_string(speed) +
                               " 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n" + affinity);
}

Character hero(unsigned level = 1, bool frost = true)
{
    CharacterDraft d;
    d.race = "orc";
    d.gender = "female";
    d.character_class = "wizard";
    d.background = "sage";
    d.alignment = "neutral_good";
    d.name = "Frost Wizard";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    d.cantrips =
        frost ? std::vector<std::string> {"ray_of_frost"} :
        std::vector<std::string> {"fire_bolt"};
    Character h(*srd5::character_rules(), d, {});
    VitalState state;
    for (unsigned n = 1; n < level; ++n)
        check(h.advance(*module(), state), "Ordinary advancement");
    return h;
}

Command command(const CombatSession &c, std::string_view verb, EntityId target = 0)
{
    for (const auto &v : c.legal_commands())
        if (v.verb == verb && (!target || v.target == target))
            return v;
    throw std::runtime_error("Missing command: " + std::string(verb));
}

void act(CombatSession &c, std::string_view verb, EntityId target = 0)
{
    check(c.submit(command(c, verb, target)), "Submit legal action");
}

bool has(const CombatSession &c, std::string_view verb, EntityId target = 0)
{
    for (const auto &v : c.legal_commands())
        if (v.verb == verb && (!target || v.target == target))
            return true;
    return false;
}

CombatantView unit(const CombatSession &c, EntityId id = 1)
{
    for (const auto &a : c.snapshot().combatants)
        if (a.id == id)
            return a;
    throw std::runtime_error("Missing actor");
}

fx::EffectState effects(const VitalState &state)
{
    auto at = state.resources.find("FX");
    if (at == state.resources.npos)
        return {};
    std::istringstream in(state.resources.substr(at));
    return fx::read_effects(in);
}

std::uint64_t rng(const CombatSession &c)
{
    std::istringstream in(c.save());
    std::string row;
    for (int n = 0; n < 3; ++n)
        std::getline(in, row);
    std::uint64_t result;
    in >> result;
    return result;
}

auto battle(const RulesModule &rules, const Character &h, unsigned seed = 13, Cell target = {3, 1},
std::vector<std::string> gear = {})
{
    auto c = rules.create({{20, 8, std::vector<Terrain>(160)},
        {   {
                1,
                "campaign-character",
                "Caster",
                0,
                {1, 1},
                rules.character_profile(h.sheet(), gear).data
            },
            {2, "target", "Target", 1, target}
        }},
    seed);
    while (c->snapshot().actor != 1)
        act(*c, "end");
    return c;
}

void access()
{
    auto h = hero();
    auto rules = module();
    auto access = rules->spell_access(h.sheet());
    check(access.cantrips.size() == 1 && access.cantrips[0].id == "ray_of_frost" &&
          access.cantrips[0].source_id == "class:wizard:spellcasting" &&
          access.cantrips[0].acquired_level == 1,
          "Ordinary starting choice has real source and acquisition level");
    auto profile = rules->character_profile(h.sheet(), {}).data;
    check(profile.starts_with("PC42 1 ") && profile.find(" ray_of_frost ") != profile.npos,
          "Profile lists the chosen spell by id");
    profile.replace(0, 4, "PC13");
    rejects(
        [&]
    {
        (void)rules->create({{8, 8, std::vector<Terrain>(64)},
            {   {1, "campaign-character", "Forged", 0, {1, 1}, profile},
                {2, "vanguard", "Enemy", 1, {3, 1}}
            }},
        13);
    });
    auto c = battle(*custom(), hero(1, false));
    check(!has(*c, "ray_of_frost"), "Unknown spell is not automatically granted");
}

void damage()
{
    for (unsigned level = 1; level <= 4; ++level)
        for (unsigned seed :
                {
                    0u, 13u, 40u
                })
            for (const std::string defense :
                    {"", "resistance", "vulnerability", "immunity"
                    })
            {
                auto rules =
                    custom(defense.empty() ? "" : "affinity target test " + defense + " cold\n");
                auto c = battle(*rules, hero(level), seed);
                auto copy = rules->restore(c->save());
                auto before = unit(*c);
                auto random = rng(*c);
                auto ticket = command(*c, "ray_of_frost", 2);
                check(c->submit(ticket) && copy->submit(ticket) && c->save() == copy->save(),
                      "Checkpoint preserves exact hit and slow continuation");
                // Independent fixed examples: seed 0 rolls natural 20, d8 5+4;
                // seed 13 rolls natural 17, d8 4; seed 40 rolls natural 1.
                // From level three a Wizard is an Evoker: Potent Cantrip deals half
                // damage on a miss, covered by opengold_wizard_spell_tests.
                const bool potent = seed == 40 && level >= 3;
                const int raw = seed == 0 ? 9 : seed == 13 ? 4 : 0;
                const int expected = defense == "immunity"        ? 0
                                     : defense == "resistance"    ? raw / 2
                                     : defense == "vulnerability" ? raw * 2
                                     : raw;
                check((potent || unit(*c, 2).hit_points == 1000 - expected),
                      "Independent Cold damage and critical/defense values");
                check(potent || rng(*c) == random + 0x9e3779b97f4a7c15ULL * (seed == 0    ? 3u
                        : seed == 13 ? 2u
                        : 1u),
                      "Miss/normal/critical consume only their required dice");
                auto target = unit(*c, 2);
                auto status = effects(target.persistent);
                check(fx::speed_penalty(status) == (seed == 40 ? 0 : 10),
                      "Slow depends on hit even when Cold damage is immune");
                check(target.movement_feet == (seed == 40 ? 30 : 20),
                      "Speed reduction affects movement allowance");
                if (seed != 40)
                    check(status.active.size() == 1 && status.active[0].source_actor == 1 &&
                          status.active[0].remaining_ms == 6000 &&
                          status.active[0].save_in_ms == 0,
                          "Slow records caster and exact next-turn duration without saves");
                auto after = unit(*c);
                check(!after.action && after.bonus_action && after.reaction &&
                      after.movement_feet == before.movement_feet &&
                      after.persistent == before.persistent,
                      "Cast consumes only Magic action");
                auto saved = c->save();
                check(!c->submit(ticket) && c->save() == saved,
                      "Stale command preserves every saved byte");
            }
}

void timing()
{
    auto rules = custom();
    auto c = battle(*rules, hero());
    act(*c, "ray_of_frost", 2);
    act(*c, "end");
    check(c->snapshot().actor == 2 && unit(*c, 2).movement_feet == 20,
          "Target starts its turn slowed");
    act(*c, "dash");
    check(unit(*c, 2).movement_feet == 40, "Dash uses reduced Speed");
    auto copy = rules->restore(c->save());
    act(*c, "end");
    act(*copy, "end");
    check(c->save() == copy->save() && c->snapshot().actor == 1 &&
          effects(unit(*c, 2).persistent).active.empty(),
          "Slow expires at caster turn start with exact restored continuation");
    check(unit(*c, 2).movement_feet == 60,
          "Expiry restores both Speed-derived allowances, not spent movement");
    c = battle(*rules, hero(), 13, {15, 1});
    Command move;
    for (const auto &v : c->legal_commands())
        if (v.verb == "move" && v.destination == Cell{6, 1})
            move = v;
    check(c->submit(move) && unit(*c).movement_feet == 5, "Spend 25 feet before casting");
    act(*c, "ray_of_frost", 1);
    check(unit(*c).movement_feet == 0, "Slow cannot expose negative movement");
    copy = rules->restore(c->save());
    act(*c, "adrenaline_rush");
    act(*copy, "adrenaline_rush");
    check(c->save() == copy->save() && unit(*c).movement_feet == 15,
          "Slow preserves all 25 spent feet when adding Bonus Action Dash");
    auto slow = custom({}, 5);
    c = battle(*slow, hero());
    act(*c, "ray_of_frost", 2);
    act(*c, "end");
    act(*c, "dash");
    check(unit(*c, 2).movement_feet == 0, "Speed and Dash allowances floor at zero");
}

void multiple_casters()
{
    auto rules = custom();
    const auto profile = rules->character_profile(hero(3).sheet(), {}).data;
    auto c = rules->create({{20, 8, std::vector<Terrain>(160)},
        {   {1, "campaign-character", "First", 0, {1, 1}, profile},
            {2, "target", "Target", 1, {8, 1}},
            {3, "campaign-character", "Second", 0, {3, 1}, profile}
        }},
    2);
    while (c->snapshot().actor != 1)
        act(*c, "end");
    act(*c, "ray_of_frost", 2);
    act(*c, "end");
    while (c->snapshot().actor != 3)
        act(*c, "end");
    act(*c, "ray_of_frost", 2);
    auto state = effects(unit(*c, 2).persistent);
    check(state.active.size() == 2 && unit(*c, 2).movement_feet == 20,
          "Two real casters retain separate applications but one Speed penalty");
    auto copy = rules->restore(c->save());
    do
    {
        act(*c, "end");
        act(*copy, "end");
    }
    while (c->snapshot().actor != 1);
    state = effects(unit(*c, 2).persistent);
    check(c->save() == copy->save() && state.active.size() == 1 &&
          state.active[0].source_actor == 3,
          "First caster turn expires only its application after reload");
    do
    {
        act(*c, "end");
    }
    while (c->snapshot().actor != 3);
    check(effects(unit(*c, 2).persistent).active.empty(),
          "Second caster turn ends the remaining slow");
}

void effect_lifecycle()
{
    fx::EffectState state;
    fx::apply_ray_of_frost(state, 5, 1, "First", 2000);
    fx::apply_ray_of_frost(state, 5, 2, "Second", 6000);
    fx::apply_ray_of_frost(state, 6, 1, "Other encounter", 4000);
    check(fx::speed_penalty(state) == 10, "Same spell from multiple sources does not stack");
    std::ostringstream out;
    fx::write_effects(out, state);
    std::istringstream in(out.str());
    check(fx::read_effects(in) == state, "Effect round trip retains every independent source");
    fx::EffectSubject subject{10, state, {}};
    std::uint64_t random = 17;
    fx::elapse_effects(std::span(&subject, 1), 2000, random);
    check(state.active.size() == 2 && fx::speed_penalty(state) == 10 && random == 17,
          "Earliest caster expiry does not remove another slow or roll a save");
    fx::elapse_effects(std::span(&subject, 1), 4000, random);
    check(state.active.empty() && random == 17, "Last application expires without consuming RNG");
    for (const char *bad :
            {"FX1 2 1 1 2 5 1 \"Caster\" 0 6000 0", "FX8 2 1 1 2 5 1 \"Caster\" 1 6000 0 0",
             "FX8 2 1 1 2 5 1 \"Caster\" 0 6001 0 0", "FX8 2 1 1 2 5 1 \"Caster\" 0 6000 1 0"
            })
        rejects(
            [&]
    {
        std::istringstream input(bad);
        (void)fx::read_effects(input);
    });
}

void legality()
{
    auto rules = custom();
    for (int feet :
            {
                60, 65
            })
    {
        auto c = battle(*rules, hero(), 13, {1 + feet / 5, 1});
        check(has(*c, "ray_of_frost", 2) == (feet == 60), "Range includes 60 feet and excludes 65");
        if (feet == 65)
        {
            auto before = c->save();
            check(!c->submit({c->snapshot().revision, 1, 2, "ray_of_frost"}) && before == c->save(),
                  "Invalid target preserves action, RNG and resources");
        }
    }
    auto c = battle(*rules, hero(), 13, {3, 1}, {"quarterstaff", "shield"});
    check(has(*c, "ray_of_frost"), "Full hands do not block casting (CLASS-11)");
    auto board = Battlefield{20, 8, std::vector<Terrain>(160)};
    board.terrain[22] = Terrain::obstacle;
    c = rules->create({board,
        {   {
                1,
                "campaign-character",
                "Caster",
                0,
                {1, 1},
                rules->character_profile(hero().sheet(), {}).data
            },
            {2, "target", "Target", 1, {3, 1}}
        }},
    13);
    check(!has(*c, "ray_of_frost", 2), "Opaque terrain blocks casting");
    c = battle(*rules, hero());
    auto before = c->save();
    for (EntityId target :
            {
                0u, 999u
            })
        check(!c->submit({c->snapshot().revision, 1, target, "ray_of_frost"}) &&
              c->save() == before,
              "Only real creatures may be targeted");
}

void campaign()
{
    auto rules = module();
    CampaignParty party(module());
    auto id = party.add_pc(hero(3));
    auto actors = party.participants();
    actors[0].cell = {1, 1};
    actors.push_back({99, "vanguard", "Enemy", 1, {13, 1}});
    auto c = rules->create({{20, 8, std::vector<Terrain>(160)}, actors}, 13);
    while (c->snapshot().actor != id)
        act(*c, "end");
    act(*c, "ray_of_frost", id);
    party.begin_combat();
    party.apply_combat(c->snapshot());
    party.end_combat();
    check(fx::speed_penalty(effects(party.member(id).vitals)) == 10,
          "Campaign receives actual sourced combat effect");
    auto saved = encode_campaign(party, nullptr, "frost");
    CampaignParty restored(module());
    restored.restore(
        decode_campaign(saved, *srd5::character_rules(), *rules, "frost", nullptr).party);
    check(encode_campaign(restored, nullptr, "frost") == saved,
          "Campaign reload retains exact unresolved duration and grants");
    auto participants = restored.participants();
    RandomState random{123};
    rules->elapse(participants, std::chrono::milliseconds(5999), random);
    check(fx::speed_penalty(effects(*participants[0].state)) == 10 && random.value == 123,
          "Campaign timeline preserves a still-active slow without save rolls");
    rules->elapse(participants, std::chrono::milliseconds(1), random);
    check(effects(*participants[0].state).active.empty() && random.value == 123,
          "Campaign effect ends at exact deadline");
    auto rest = restored.rest(RestKind::short_rest);
    check(bool(rest) && effects(restored.member(id).vitals).active.empty(),
          "Completed camp rest expires the slow");
    restored.finish_short_rest(restored.state().short_rest->ticket);
    auto camp = encode_campaign(restored, nullptr, "frost");
    CampaignParty again(module());
    again.restore(decode_campaign(camp, *srd5::character_rules(), *rules, "frost", nullptr).party);
    check(encode_campaign(again, nullptr, "frost") == camp, "Post-camp save/reload is canonical");
}

void fixtures()
{
    auto path = std::filesystem::path(OPENGOLD_BINARY_DIR) / "frost-fixtures";
    std::filesystem::create_directories(path);
    auto rules = module();
    auto h = hero(3);
    auto profile =
        rules->character_profile(h.sheet(), std::vector<std::string> {"quarterstaff"}).data;
    auto c = rules->create({{20, 8, std::vector<Terrain>(160)},
        {   {1, "campaign-character", "Frost Wizard", 0, {1, 1}, profile},
            {2, "vanguard", "Ally", 0, {3, 1}},
            {99, "vanguard", "Enemy", 1, {8, 1}}
        }},
    2);
    while (c->snapshot().actor != 1)
        act(*c, "end");
    std::ofstream(path / "known.save", std::ios::binary) << c->save();
    act(*c, "ray_of_frost", 2);
    std::ofstream(path / "slow.save", std::ios::binary) << c->save();
}
} // namespace

int main()
{
    try
    {
        access();
        damage();
        timing();
        multiple_casters();
        effect_lifecycle();
        legality();
        campaign();
        fixtures();
        std::cout << "Ray of Frost tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
