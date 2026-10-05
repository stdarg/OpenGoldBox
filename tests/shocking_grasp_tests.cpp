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

Character hero(unsigned level = 1, std::string spell = "shocking_grasp")
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
    d.cantrips = std::vector<std::string> {spell};
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

auto battle(const RulesModule &rules, const Character &h, unsigned seed = 13, Cell target = {2, 1},
std::vector<std::string> gear = {})
{
    auto c = rules.create({{20, 8, std::vector<std::uint8_t>(160)},
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
    auto rules = module();
    auto h = hero();
    const auto access = rules->spell_access(h.sheet());
    check(access.cantrips.size() == 1 && access.cantrips[0].id == "shocking_grasp" &&
          access.cantrips[0].source_id == "class:wizard:spellcasting",
          "Real Wizard source");
    auto profile = rules->character_profile(h.sheet(), {}).data;
    check(profile.starts_with("PC42 1 0 2 shocking_grasp magic_missile "),
          "The recipe lists the selected cantrip");
    profile.replace(0, 4, "PC32");
    rejects(
        [&]
    {
        (void)rules->create({{8, 8, std::vector<std::uint8_t>(64)},
            {   {1, "campaign-character", "Forged", 0, {1, 1}, profile},
                {2, "vanguard", "Enemy", 1, {2, 1}}
            }},
        13);
    });
    auto c = battle(*custom(), hero(1, "fire_bolt"));
    check(!has(*c, "shocking_grasp"), "Unknown spell unavailable");
    auto wrong = h.sheet();
    for (auto &g : wrong.grants)
        if (g.id == "spell:shocking_grasp")
            g.source_id = "species:elf";
    rejects(
        [&]
    {
        (void)rules->character_profile(wrong, {});
    });
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
                auto rules = custom(
                                 defense.empty() ? "" : "affinity target test " + defense + " lightning\n");
                auto c = battle(*rules, hero(level), seed);
                auto copy = rules->restore(c->save());
                auto before = unit(*c);
                const auto random = rng(*c);
                const auto ticket = command(*c, "shocking_grasp", 2);
                check(c->submit(ticket) && copy->submit(ticket) && c->save() == copy->save(),
                      "Selected cast preserves deterministic attack/effect continuation");
                // Independent SplitMix64: seed 0 natural 20, d8 5+4; seed 13 natural 17, d8 4; seed
                // 40 natural 1.
                bool logged = false;
                for (const auto &message : c->snapshot().log_messages)
                    if (message.source.starts_with("{actor} -> {target}: d20"))
                        for (const auto &arg : message.arguments)
                            if (arg.name == "bonus")
                            {
                                logged = true;
                                check(
                                    arg.value == std::to_string(level == 4 ? 6 : 5),
                                    "Wizard Intelligence plus proficiency, not Strength/Charisma");
                            }
                check(logged, "Attack bonus recorded");
                // From level three a Wizard is an Evoker: Potent Cantrip deals half
                // damage on a miss, covered by opengold_wizard_spell_tests.
                const bool potent = seed == 40 && level >= 3;
                const int raw = seed == 0    ? 9
                                : seed == 13 ? 4
                                : 0,
                                expected = defense == "immunity"        ? 0
                                           : defense == "resistance"    ? raw / 2
                                           : defense == "vulnerability" ? raw * 2
                                           : raw;
                check((potent || unit(*c, 2).hit_points == 1000 - expected),
                      "Melee spell hit/miss/critical and Lightning defenses");
                check(
                    potent || rng(*c) == random + 0x9e3779b97f4a7c15ULL * (seed == 0    ? 3u
                            : seed == 13 ? 2u
                            : 1u),
                    "Melee spell has no adjacent-hostile Disadvantage or extra save/damage modifier");
                const auto status = effects(unit(*c, 2).persistent);
                check(fx::opportunity_blocked(status) == (seed != 40),
                      "A hit suppresses opportunities even with Lightning immunity");
                check(unit(*c, 2).reaction && unit(*c, 2).movement_feet == 30,
                      "Target Reaction and movement preserved");
                if (seed != 40)
                    check(status.active.size() == 1 && status.active[0].source_actor == 1 &&
                          status.active[0].remaining_ms == 3000 &&
                          status.active[0].save_in_ms == 0,
                          "Effect lasts until target turn, not caster turn");
                const auto after = unit(*c);
                check(!after.action && after.bonus_action && after.reaction &&
                      after.movement_feet == before.movement_feet &&
                      after.persistent == before.persistent,
                      "Only Magic action spent");
                auto saved = c->save();
                check(!c->submit(ticket) && c->save() == saved, "Stale action is atomic");
            }
}

void timing()
{
    auto rules = custom();
    auto c = battle(*rules, hero());
    act(*c, "shocking_grasp", 2);
    auto copy = rules->restore(c->save());
    act(*c, "end");
    act(*copy, "end");
    check(c->save() == copy->save() && c->snapshot().actor == 2 &&
          !fx::opportunity_blocked(effects(unit(*c, 2).persistent)),
          "Expires at next target turn with identical continuation");
    c = battle(*rules, hero());
    act(*c, "shocking_grasp", 1);
    check(fx::opportunity_blocked(effects(unit(*c).persistent)), "Self hit supported");
    act(*c, "end");
    check(fx::opportunity_blocked(effects(unit(*c).persistent)),
          "Self suppression remains through enemy turn");
    act(*c, "end");
    check(!fx::opportunity_blocked(effects(unit(*c).persistent)),
          "Self suppression ends at own next turn");
}

void movement()
{
    auto rules = custom();
    auto profile = rules->character_profile(hero().sheet(), {}).data;
    // A second adjacent enemy remains able to interrupt; the shocked enemy is omitted.
    auto c = rules->create({{10, 8, std::vector<std::uint8_t>(80)},
        {   {1, "campaign-character", "Wizard", 0, {1, 1}, profile},
            {2, "target", "Shocked", 1, {2, 1}},
            {3, "target", "Other", 1, {1, 2}}
        }},
    13);
    while (c->snapshot().actor != 1)
        act(*c, "end");
    act(*c, "shocking_grasp", 2);
    check(fx::opportunity_blocked(effects(unit(*c, 2).persistent)),
          "Actual hit applies suppression");
    auto find_move = [&](const CombatSession & s)
    {
        for (const auto &v : s.legal_commands())
            if (v.verb == "move" && v.destination == Cell{0, 0})
                return v;
        throw std::runtime_error("Missing movement");
    };
    check(c->submit(find_move(*c)), "Move after cast");
    check(c->snapshot().reaction_pending && c->snapshot().actor == 3,
          "Only unaffected enemy offers Opportunity Attack");
    check(unit(*c, 2).reaction, "Suppressed enemy does not spend its Reaction");
    auto copy = rules->restore(c->save());
    act(*c, "decline");
    act(*copy, "decline");
    check(c->save() == copy->save() && !c->snapshot().reaction_pending,
          "Interrupted movement continues exactly and skips shocked reactor");
    check(fx::opportunity_blocked(effects(unit(*c, 2).persistent)),
          "Movement interruption does not advance the effect clock");
    while (c->snapshot().actor != 2)
        act(*c, "end");
    check(!fx::opportunity_blocked(effects(unit(*c, 2).persistent)) && unit(*c, 2).reaction,
          "Target turn restores ordinary opportunity eligibility");
}

void reaction_and_armor()
{
    auto rules = custom();
    bool covered = false;
    for (unsigned seed = 0; seed < 100 && !covered; ++seed)
    {
        auto c = battle(*rules, hero(4), seed);
        auto move = [&](Cell to)
        {
            for (const auto &v : c->legal_commands())
                if (v.verb == "move" && v.destination == to)
                    return v;
            throw std::runtime_error("Missing test move");
        };
        check(c->submit(move({0, 1})), "Move out of reach");
        check(c->snapshot().reaction_pending, "Ordinary target can react before shock");
        act(*c, "opportunity");
        check(!unit(*c, 2).reaction, "Actual Opportunity Attack spends Reaction");
        check(c->submit(move({1, 1})), "Return within touch range");
        act(*c, "shocking_grasp", 2);
        check(!unit(*c, 2).reaction, "A hit or miss never refunds a spent Reaction");
        if (!fx::opportunity_blocked(effects(unit(*c, 2).persistent)))
            continue;
        covered = true;
        auto copy = rules->restore(c->save());
        act(*c, "end");
        act(*copy, "end");
        check(c->save() == copy->save() && unit(*c, 2).reaction &&
              !fx::opportunity_blocked(effects(unit(*c, 2).persistent)),
              "Only target turn refreshes the previously spent Reaction");
    }
    check(covered, "Hit with spent Reaction exercised");
    auto d = hero().creation_data();
    d.character_class = "fighter";
    d.cantrips = std::vector<std::string> {};
    Character fighter(*srd5::character_rules(), d, {});
    VitalState state;
    for (unsigned n = 1; n < 4; ++n)
        check(fighter.advance(*rules, state), "Ordinary armored Fighter advancement");
    auto c = rules->create(
    {
        {8, 8, std::vector<std::uint8_t>(64)},
        {   {
                1,
                "campaign-character",
                "Wizard",
                0,
                {1, 1},
                rules->character_profile(hero().sheet(), {}).data
            },
            {
                2,
                "campaign-character",
                "Plate Fighter",
                1,
                {2, 1},
                rules->character_profile(fighter.sheet(), std::vector<std::string>{"plate"}).data
            }
        }},
    89);
    while (c->snapshot().actor != 1)
        act(*c, "end");
    const auto random = rng(*c);
    const int hp = unit(*c, 2).hit_points;
    act(*c, "shocking_grasp", 2);
    check(rng(*c) == random + 2 * 0x9e3779b97f4a7c15ULL && unit(*c, 2).hit_points == hp - 8,
          "Metal armor grants no Advantage; no ability modifier to damage");
}

void lifecycle()
{
    fx::EffectState state;
    fx::apply_shocking_grasp(state, 5, 1, "First", 2000);
    fx::apply_shocking_grasp(state, 5, 2, "Second", 6000);
    fx::apply_ray_of_frost(state, 6, 1, "Cold", 4000);
    std::ostringstream out;
    fx::write_effects(out, state);
    check(out.str().starts_with("FX8 "), "Effects are written in the only effect format");
    std::istringstream input(out.str());
    check(fx::read_effects(input) == state, "Mixed sourced effects round trip");
    fx::EffectSubject subject{10, state, {}};
    std::uint64_t random = 17;
    fx::elapse_effects(std::span(&subject, 1), 2000, random);
    check(state.active.size() == 2 && fx::opportunity_blocked(state) && random == 17,
          "Independent suppression sources do not consume saves");
    fx::elapse_effects(std::span(&subject, 1), 4000, random);
    check(state.active.empty() && random == 17, "Last source expires without RNG");
    std::istringstream valid("FX8 2 1 1 3 5 1 \"Caster\" 0 6000 0 0");
    check(fx::opportunity_blocked(fx::read_effects(valid)),
          "A well-formed Shocking Grasp record loads");
    for (const char *bad :
            {"FX3 2 1 1 3 5 1 \"Caster\" 0 6000 0", "FX8 2 1 1 3 5 1 \"Caster\" 1 6000 0 0",
             "FX8 2 1 1 3 5 1 \"Caster\" 0 6001 0 0", "FX8 2 1 1 3 5 1 \"Caster\" 0 6000 1 0",
             "FX8 1 0"
            })
        rejects(
            [&]
    {
        std::istringstream in(bad);
        (void)fx::read_effects(in);
    });
}

void legality()
{
    auto rules = custom();
    for (int feet :
            {
                5, 10
            })
    {
        auto c = battle(*rules, hero(), 13, {1 + feet / 5, 1}, {"whip"});
        check(has(*c, "shocking_grasp", 2) == (feet == 5), "Touch ignores weapon reach");
        if (feet == 10)
        {
            auto before = c->save();
            check(!c->submit({c->snapshot().revision, 1, 2, "shocking_grasp"}) &&
                  c->save() == before,
                  "Range rejection atomic");
        }
    }
    for (auto gear : std::vector<std::vector<std::string>>
{
    {"quarterstaff", "shield"}, {"wand", "shield"}, {"plate"}
})
    {
        auto c = battle(*rules, hero(), 13, {2, 1}, gear);
        auto before = c->save();
        check(!has(*c, "shocking_grasp") &&
              !c->submit({c->snapshot().revision, 1, 2, "shocking_grasp"}) &&
              c->save() == before,
              "Hands/untrained armor reject without mutation");
    }
    auto c = battle(*rules, hero());
    auto before = c->save();
    for (EntityId id :
            {
                0u, 999u
            })
        check(!c->submit({c->snapshot().revision, 1, id, "shocking_grasp"}) && c->save() == before,
              "Unknown target atomic");
}

void persistence_guards()
{
    auto rules = custom();
    auto c = battle(*rules, hero());
    act(*c, "shocking_grasp", 2);
    const auto current = c->save();
    auto old = current;
    old.replace(old.find(module()->identity().version), 6, "0.6.37");
    rejects(
        [&]
    {
        (void)rules->restore(old);
    });
    auto malformed = current;
    const auto fx = malformed.find("FX8 ");
    check(fx != malformed.npos, "Actual live effect encoded as FX8");
    malformed.replace(fx, 3, "FX6");
    rejects(
        [&]
    {
        (void)rules->restore(malformed);
    });
    check(c->save() == current, "Rejected restore preserves current session");
    auto board = Battlefield{8, 8, std::vector<std::uint8_t>(64)};
    auto profile = rules->character_profile(hero().sheet(), {}).data;
    auto dead = rules->create({board,
        {   {1, "campaign-character", "Wizard", 0, {1, 1}, profile},
            {2, "target", "Dead", 1, {2, 1}, {}, VitalState{0, true, {}}},
            {3, "target", "Enemy", 1, {6, 1}}
        }},
    13);
    while (dead->snapshot().actor != 1)
        act(*dead, "end");
    auto before = dead->save();
    check(!has(*dead, "shocking_grasp", 2) &&
          !dead->submit({dead->snapshot().revision, 1, 2, "shocking_grasp"}) &&
          dead->save() == before,
          "Dead creature cannot be targeted");
}

void campaign()
{
    auto rules = module();
    for (unsigned level = 1; level <= 4; ++level)
        for (bool npc :
                {
                    false, true
                })
        {
            CampaignParty party(module());
            const auto id =
                npc ? party.recruit("fixture:shocking", hero(level)) : party.add_pc(hero(level));
            auto actors = party.participants();
            actors[0].cell = {1, 1};
            actors.push_back({99, "vanguard", "Enemy", 1, {6, 1}});
            auto c = rules->create({{8, 8, std::vector<std::uint8_t>(64)}, actors}, 13);
            while (c->snapshot().actor != id)
                act(*c, "end");
            act(*c, "shocking_grasp", id);
            party.begin_combat();
            party.apply_combat(c->snapshot());
            party.end_combat();
            check(fx::opportunity_blocked(effects(party.member(id).vitals)),
                  "Actual self hit reaches campaign");
            auto saved = encode_campaign(party, nullptr, "shocking");
            CampaignParty restored(module());
            restored.restore(
                decode_campaign(saved, *srd5::character_rules(), *rules, "shocking", nullptr)
                .party);
            check(encode_campaign(restored, nullptr, "shocking") == saved,
                  "Campaign exact round trip");
            auto participants = restored.participants();
            std::uint64_t random = 123;
            rules->elapse(participants, 5999, random);
            check(fx::opportunity_blocked(effects(*participants[0].state)) && random == 123,
                  "Suppression persists until deadline outside combat");
            rules->elapse(participants, 1, random);
            check(!fx::opportunity_blocked(effects(*participants[0].state)) && random == 123,
                  "Expires at exact campaign deadline");
            for (auto kind :
                    {
                        RestKind::short_rest, RestKind::long_rest
                    })
            {
                check(bool(restored.rest(kind)), "Rest accepted");
                if (restored.state().short_rest)
                    restored.finish_short_rest(restored.state().short_rest->ticket);
                check(!fx::opportunity_blocked(effects(restored.member(id).vitals)),
                      "Rest expires suppression");
                const auto camp = encode_campaign(restored, nullptr, "shocking");
                CampaignParty again(module());
                again.restore(
                    decode_campaign(camp, *srd5::character_rules(), *rules, "shocking", nullptr)
                    .party);
                check(encode_campaign(again, nullptr, "shocking") == camp,
                      "Rest-save reload retains grants and state");
            }
        }
}

void fixtures()
{
    auto path = std::filesystem::path(OPENGOLD_BINARY_DIR) / "shocking-fixtures";
    std::filesystem::create_directories(path);
    auto rules = module();
    auto h = hero(3);
    auto profile = rules->character_profile(h.sheet(), {}).data;
    auto c = rules->create({{12, 9, std::vector<std::uint8_t>(108)},
        {   {1, "campaign-character", "Wizard", 0, {1, 1}, profile},
            {2, "vanguard", "Ally", 0, {2, 1}},
            {99, "vanguard", "Enemy", 1, {5, 1}}
        }},
    2);
    while (c->snapshot().actor != 1)
        act(*c, "end");
    std::ofstream(path / "known.save", std::ios::binary) << c->save();
    act(*c, "shocking_grasp", 2);
    std::ofstream(path / "suppressed.save", std::ios::binary) << c->save();
}
} // namespace

int main()
{
    try
    {
        access();
        damage();
        timing();
        movement();
        reaction_and_armor();
        lifecycle();
        legality();
        persistence_guards();
        campaign();
        fixtures();
        std::cout << "Shocking Grasp tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
