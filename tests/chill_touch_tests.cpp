#include "opengold/campaign_save.h"
#include "opengold/srd5.h"
#include "life_cycle.h"
#include "recovery_timeline.h"
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

auto custom(std::string affinity = {})
{
    return srd5::parse_content(read(root / "data/rules/srd-5.2.1/combat.rules") +
                               "\ncreature target 1 1000 0 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n" +
                               affinity);
}

Character hero(std::string klass = "wizard", unsigned level = 1)
{
    CharacterDraft d;
    d.race = "orc";
    d.gender = "female";
    d.character_class = klass;
    d.background = "sage";
    d.alignment = "neutral_good";
    d.name = "Chill caster";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    d.cantrips = std::vector<std::string> {"chill_touch"};
    if (klass == "fighter" || klass == "cleric")
        d.cantrips = std::vector<std::string> {};
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

bool blocked(const CombatSession &c, EntityId id = 2)
{
    return fx::healing_blocked(effects(unit(c, id).persistent));
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
    for (const auto *klass :
            {"wizard", "sorcerer", "warlock"
            })
    {
        auto h = hero(klass);
        auto access = rules->spell_access(h.sheet());
        check(access.cantrips.size() == 1 && access.cantrips[0].id == "chill_touch",
              "Real learned cantrip");
        check(access.cantrips[0].source_id ==
              (std::string(klass) == "warlock"
               ? "class:warlock:pact_magic"
               : "class:" + std::string(klass) + ":spellcasting"),
              "Class provenance");
        auto c = battle(*custom(), h);
        act(*c, "chill_touch", 2);
        bool checked = false;
        for (const auto &message : c->snapshot().log_messages)
            for (const auto &arg : message.arguments)
                if (arg.name == "bonus")
                {
                    const auto ability = std::string(klass) == "wizard" ? 3 : 5;
                    check(arg.value == std::to_string(2 + (h.sheet().scores[ability] - 10) / 2),
                          "Class Intelligence/Charisma casting bonus");
                    checked = true;
                }
        check(checked, "Real attack bonus observed");
        auto profile = rules->character_profile(h.sheet(), {}).data;
        check(profile.starts_with("PC42 ") && profile.find(" chill_touch ") != profile.npos,
              "Profile lists the learned cantrip");
        profile.replace(0, 4, "PC28");
        rejects(
            [&]
        {
            (void)rules->create({{8, 8, std::vector<std::uint8_t>(64)},
                {   {1, "campaign-character", "Forged", 0, {1, 1}, profile},
                    {2, "vanguard", "Enemy", 1, {2, 1}}
                }},
            13);
        });
        auto draft = h.creation_data();
        draft.cantrips = std::vector<std::string> {"chill_touch", "chill_touch"};
        rejects(
            [&]
        {
            Character invalid(*srd5::character_rules(), draft, {});
        });
    }
    auto draft = hero("warlock").creation_data();
    draft.cantrips = std::vector<std::string> {"eldritch_blast", "poison_spray", "chill_touch"};
    rejects(
        [&]
    {
        Character invalid(*srd5::character_rules(), draft, {});
    });
    draft = hero("sorcerer").creation_data();
    draft.cantrips = std::vector<std::string> {"fire_bolt", "poison_spray", "ray_of_frost",
        "shocking_grasp", "chill_touch"
                                              };
    rejects(
        [&]
    {
        Character invalid(*srd5::character_rules(), draft, {});
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
                                 defense.empty() ? "" : "affinity target test " + defense + " necrotic\n");
                auto c = battle(*rules, hero("wizard", level), seed);
                auto copy = rules->restore(c->save());
                const auto random = rng(*c);
                const auto ticket = command(*c, "chill_touch", 2);
                const auto before = unit(*c);
                check(c->submit(ticket) && copy->submit(ticket) && c->save() == copy->save(),
                      "Deterministic cast and restore");
                // From level three a Wizard is an Evoker: Potent Cantrip deals half
                // damage on a miss, covered by opengold_wizard_spell_tests.
                const bool potent = seed == 40 && level >= 3;
                const int raw = seed == 0 ? 13 : seed == 13 ? 8 : 0;
                const int expected = defense == "immunity"        ? 0
                                     : defense == "resistance"    ? raw / 2
                                     : defense == "vulnerability" ? raw * 2
                                     : raw;
                check((potent || unit(*c, 2).hit_points == 1000 - expected),
                      "1d10 melee Necrotic hit/crit/miss through level four");
                check(potent || rng(*c) == random + 0x9e3779b97f4a7c15ULL * (seed == 0    ? 3u
                        : seed == 13 ? 2u
                        : 1u),
                      "No ranged melee disadvantage, save or flat damage modifier");
                check(blocked(*c) == (seed != 40), "Hit prevents healing even through immunity");
                if (seed != 40)
                {
                    const auto fx = effects(unit(*c, 2).persistent);
                    check(fx.active.size() == 1 && fx.active[0].source_actor == 1 &&
                          fx.active[0].remaining_ms == 9000,
                          "End of next caster turn, with source");
                }
                const auto after = unit(*c);
                check(!after.action && after.bonus_action && after.reaction &&
                      after.movement_feet == before.movement_feet &&
                      after.persistent == before.persistent,
                      "Only ordinary Magic action spent");
                const auto saved = c->save();
                check(!c->submit(ticket) && c->save() == saved, "Repeated command is atomic");
            }
}

void timing()
{
    auto rules = custom();
    auto c = battle(*rules, hero());
    act(*c, "chill_touch", 2);
    auto copy = rules->restore(c->save());
    for (int n = 0; n < 3; ++n)
    {
        check(blocked(*c), "Healing blocked throughout target and caster next turn");
        act(*c, "end");
        act(*copy, "end");
        check(c->save() == copy->save(), "Exact effect continuation");
    }
    check(!blocked(*c), "Expires at end of next caster turn");
    c = battle(*rules, hero("wizard", 4));
    act(*c, "chill_touch", 1);
    check(blocked(*c, 1), "Self targeting");
    act(*c, "end");
    act(*c, "end");
    check(blocked(*c, 1), "Self effect lasts through next own turn");
    act(*c, "end");
    check(!blocked(*c, 1), "Self expiry");
}

void skipped_caster()
{
    for (bool lethal :
            {
                false, true
            })
    {
        bool covered = false;
        const auto rules = srd5::parse_content(
                               read(root / "data/rules/srd-5.2.1/combat.rules") +
                               "\ncreature reaper 1 1000 -10 30 30 " +
                               (lethal ? std::string("10 20 30") : std::string("1 4 0")) + " 0 0 0 0 0 0 0 0 0 1 0\n");
        for (unsigned seed = 0; seed < 50 && !covered; ++seed)
        {
            auto h = hero();
            auto c = rules->create({{8, 8, std::vector<std::uint8_t>(64)},
                {   {
                        1,
                        "campaign-character",
                        "Caster",
                        0,
                        {1, 1},
                        rules->character_profile(h.sheet(), {}).data,
                        VitalState{1, false, {}}
                    },
                    {2, "reaper", "Enemy", 1, {2, 1}},
                    {3, "vanguard", "Companion", 0, {6, 1}}
                }},
            seed);
            while (c->snapshot().actor != 1)
                act(*c, "end");
            act(*c, "chill_touch", 2);
            if (!blocked(*c))
                continue;
            const auto deadline = c->snapshot().elapsed_milliseconds +
                                  effects(unit(*c, 2).persistent).active[0].remaining_ms;
            while (c->snapshot().actor != 2)
                act(*c, "end");
            act(*c, "melee", 1);
            if (unit(*c).hit_points > 0)
                continue;
            check(unit(*c).dead == lethal, "Actual enemy hit exercises dead/unconscious caster");
            auto copy = rules->restore(c->save());
            for (unsigned turns = 0; c->snapshot().elapsed_milliseconds < deadline && turns < 6;
                    ++turns)
            {
                act(*c, "end");
                act(*copy, "end");
                check(c->save() == copy->save(),
                      "Skipped caster slot preserves exact continuation");
                check(blocked(*c) == (c->snapshot().elapsed_milliseconds < deadline),
                      "Caster incapacity cannot prolong or shorten healing prevention");
            }
            check(!blocked(*c), "Block expires despite skipped caster slot");
            covered = true;
        }
        check(covered, "Caster incapacitation path exercised");
    }
}

void campaign_handoff()
{
    auto rules = module();
    for (bool npc :
            {
                false, true
            })
    {
        CampaignParty party(module());
        const auto h = hero("wizard", 4);
        const auto id = npc ? party.recruit("fixture:chill", h) : party.add_pc(h);
        auto actors = party.participants();
        actors[0].cell = {1, 1};
        actors.push_back({99, "vanguard", "Enemy", 1, {6, 1}});
        auto c = rules->create({{8, 8, std::vector<std::uint8_t>(64)}, actors}, 13);
        while (c->snapshot().actor != id)
            act(*c, "end");
        act(*c, "chill_touch", id);
        check(blocked(*c, id), "Actual self hit applies effect");
        party.begin_combat();
        party.apply_combat(c->snapshot());
        party.end_combat();
        check(fx::healing_blocked(effects(party.member(id).vitals)),
              "Real cast persists through campaign handoff");
        const auto saved = encode_campaign(party, nullptr, "chill-handoff");
        CampaignParty copy(module());
        copy.restore(
            decode_campaign(saved, *srd5::character_rules(), *rules, "chill-handoff", nullptr)
            .party);
        check(encode_campaign(copy, nullptr, "chill-handoff") == saved,
              "Actual PC/NPC cast round trips");
        check(bool(copy.rest(RestKind::short_rest)),
              "Ordinary camping elapses effects before recovery");
        check(!fx::healing_blocked(effects(copy.member(id).vitals)),
              "Short Rest clears expired block");
        if (copy.state().short_rest)
            copy.finish_short_rest(copy.state().short_rest->ticket);
        check(bool(copy.rest(RestKind::long_rest)) &&
              copy.member(id).vitals.hit_points == h.sheet().hit_points,
              "Long Rest heals normally after effect elapsed");
    }
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
        check(has(*c, "chill_touch", 2) == (feet == 5), "Touch ignores weapon reach");
        if (feet == 10)
        {
            auto saved = c->save();
            check(!c->submit({c->snapshot().revision, 1, 2, "chill_touch"}) && c->save() == saved,
                  "Out of range is atomic");
        }
    }
    // Full hands block nothing (CLASS-11); untrained armor does.
    for (auto gear : std::vector<std::vector<std::string>>
{
    {"quarterstaff", "shield"}, {"wand", "shield"}
})
    check(has(*battle(*rules, hero(), 13, {2, 1}, gear), "chill_touch"),
          "Full hands do not block casting");
    {
        auto c = battle(*rules, hero(), 13, {2, 1}, {"plate"});
        auto saved = c->save();
        check(!has(*c, "chill_touch") &&
              !c->submit({c->snapshot().revision, 1, 2, "chill_touch"}) && c->save() == saved,
              "Untrained armor prevents casting");
    }
}

void lifecycle()
{
    fx::EffectState state;
    fx::apply_chill_touch(state, 5, 1, "First", 2000);
    fx::apply_chill_touch(state, 5, 2, "Second", 9000);
    fx::apply_ray_of_frost(state, 6, 1, "Cold", 4000);
    state.prone = true;
    std::ostringstream out;
    fx::write_effects(out, state);
    check(out.str().starts_with("FX8 "), "Effect state writes the current tag");
    std::istringstream input(out.str());
    check(fx::read_effects(input) == state, "Mixed source and posture codec");
    fx::EffectSubject subject{10, state, {}};
    std::uint64_t random = 17;
    fx::elapse_effects(std::span(&subject, 1), 2000, random);
    check(state.active.size() == 2 && fx::healing_blocked(state) && random == 17,
          "Overlapping sources cannot heal early or consume RNG");
    check(!fx::healing_blocked(state, 7000), "Healing allowed at exact expiry boundary");
    fx::elapse_effects(std::span(&subject, 1), 7000, random);
    check(state.active.empty() && state.prone && random == 17,
          "Last source expires without changing posture or RNG");
    for (const char *bad :
            {"FX4 2 1 1 4 5 1 \"Caster\" 0 6000 0 0 1", "FX8 2 1 1 4 5 1 \"Caster\" 1 6000 0 0",
             "FX8 2 1 1 4 5 1 \"Caster\" 0 12001 0 0", "FX8 2 1 1 4 5 1 \"Caster\" 0 6000 1 0",
             "FX8 2 1 1 4 5 1 \"Caster\" 0 6000 0 2"
            })
        rejects(
            [&]
    {
        std::istringstream in(bad);
        (void)fx::read_effects(in);
    });
}

VitalState blocked_state(const RulesModule &rules, const Character &h, int hp)
{
    auto c = battle(rules, h);
    auto result = unit(*c).persistent;
    result.hit_points = hp;
    auto at = result.resources.find("FX");
    check(at != result.resources.npos, "Orc fixture carries effect record");
    result.resources.replace(at, result.resources.size() - at,
                             "FX8 2 1 1 4 5 99 \"Enemy\" 0 9000 0 0");
    return result;
}

void recovery()
{
    auto rules = custom();
    auto h = hero("fighter", 2);
    auto state = blocked_state(*rules, h, 1);
    const auto hp = state.hit_points;
    std::uint64_t random = 13;
    rules->temple_heal(state, h.sheet(), random);
    check(state.hit_points == hp && random != 13, "Temple cast consumes rolls but cannot heal");
    rules->set_hit_points(state, h.sheet(), h.sheet().hit_points);
    check(state.hit_points == hp, "Positive script HP assignment is blocked");
    const auto spent = rules->spend_hit_die(state, h.sheet(), random);
    check(spent.healing == 0 && spent.remaining == 1 && state.hit_points == hp,
          "Hit Die spent without healing");
    rules->grant_temporary_hit_points(state, h.sheet(), {5, "test:temp"},
                                      TemporaryHpChoice::use_new);
    check(rules->recovery_info(h.sheet(), state).temporary_hp.amount == 5,
          "Temporary HP unaffected");
    rules->recover(state, h.sheet());
    check(state.hit_points == hp && rules->recovery_info(h.sheet(), state).hit_dice == 2,
          "Direct recovery obeys block while restoring other pools");
    fx::LifeState zero;
    zero.hp = 0;
    zero.recovery.death_save_in_ms = 6000;
    bool exercised = false;
    for (std::uint64_t seed = 0; seed < 200; ++seed)
    {
        auto attempt = zero;
        auto rng = seed;
        if (fx::death_save(attempt, rng, false) == 20)
        {
            check(attempt.hp == 0 && attempt.successes == 1 && !attempt.stable,
                  "Natural 20 succeeds but cannot regain HP or reset death counters");
            exercised = true;
            break;
        }
    }
    check(exercised, "Natural 20 branch exercised");
    auto target = rules->character_profile(h.sheet(), {}).data;
    auto c = rules->create(
    {
        {8, 8, std::vector<std::uint8_t>(64)},
        {   {1, "campaign-character", "Fighter", 0, {1, 1}, target, blocked_state(*rules, h, 1)},
            {2, "target", "Enemy", 1, {2, 1}}
        }},
    13);
    while (c->snapshot().actor != 1)
        act(*c, "end");
    act(*c, "second_wind");
    check(unit(*c).hit_points == 1 && !unit(*c).bonus_action, "Second Wind spent without healing");
}

void healing_spells()
{
    auto rules = custom();
    auto cleric = hero("cleric").sheet();
    cleric.prepared_spells = {"cure_wounds", "healing_word"};
    auto fighter = hero("fighter", 2);
    for (const char *spell :
            {"cure_wounds", "healing_word"
            })
    {
        auto c = rules->create({{8, 8, std::vector<std::uint8_t>(64)},
            {   {
                    1,
                    "campaign-character",
                    "Cleric",
                    0,
                    {1, 1},
                    rules->character_profile(cleric, {}).data
                },
                {
                    2,
                    "campaign-character",
                    "Fighter",
                    0,
                    {2, 1},
                    rules->character_profile(fighter.sheet(), {}).data,
                    blocked_state(*rules, fighter, 1)
                },
                {99, "target", "Enemy", 1, {5, 1}}
            }},
        13);
        while (c->snapshot().actor != 1)
            act(*c, "end");
        check(blocked(*c), "Block remains before actual healing spell");
        const auto random = rng(*c);
        auto copy = rules->restore(c->save());
        act(*c, spell, 2);
        act(*copy, spell, 2);
        check(unit(*c, 2).hit_points == 1 && c->save() == copy->save() && rng(*c) != random,
              "Actual healing spell spends its rolls/slot without HP recovery");
        check(std::string(spell) == "cure_wounds" ? !unit(*c).action : !unit(*c).bonus_action,
              "Healing spell spends correct action");
    }
}

void combat_death_save()
{
    const auto rules = custom();
    const auto h = hero("fighter", 2);
    auto vitality = blocked_state(*rules, h, 1);
    rules->set_hit_points(vitality, h.sheet(), 0);
    bool covered = false;
    for (unsigned seed = 0; seed < 100 && !covered; ++seed)
    {
        auto c = rules->create({{8, 8, std::vector<std::uint8_t>(64)},
            {   {
                    1,
                    "campaign-character",
                    "Fighter",
                    0,
                    {1, 1},
                    rules->character_profile(h.sheet(), {}).data,
                    vitality
                },
                {2, "target", "Enemy", 1, {2, 1}},
                {3, "vanguard", "Companion", 0, {6, 1}}
            }},
        seed);
        for (unsigned turns = 0; turns < 3 && !covered; ++turns)
        {
            for (const auto &message : c->snapshot().log_messages)
                if (message.source == "{name} death save: {roll}")
                {
                    bool fighter = false, natural20 = false;
                    for (const auto &arg : message.arguments)
                    {
                        fighter |= arg.name == "name" && arg.value == "Fighter";
                        natural20 |= arg.name == "roll" && arg.value == "20";
                    }
                    if (fighter && natural20 && blocked(*c, 1))
                    {
                        check(
                            unit(*c).hit_points == 0 && !unit(*c).action &&
                            c->snapshot().actor != 1,
                            "Blocked natural 20 does not wake the combatant or grant its turn's actions");
                        auto restored = rules->restore(c->save());
                        check(restored->save() == c->save(),
                              "Blocked combat death save retains exact checkpoint");
                        act(*c, "end");
                        act(*restored, "end");
                        check(c->save() == restored->save(),
                              "Blocked combat death-save continuation is deterministic");
                        covered = true;
                        break;
                    }
                }
            if (!covered)
                act(*c, "end");
        }
    }
    check(covered, "Actual blocked combat natural 20 exercised");
}

void death_saves_outside_combat()
{
    const auto rules = module();
    const auto fixture_rules = custom();
    const auto h = hero("fighter", 2);
    auto vitality = blocked_state(*fixture_rules, h, 1);
    rules->set_hit_points(vitality, h.sheet(), 0); // Real damage path: dying, not Stable.
    CampaignParty whole(module());
    const auto id = whole.add_pc(h);
    auto state = whole.checkpoint();
    state.roster[0].vitals = vitality;
    state.random_state = 17;
    state.next_combat_scope = 6;
    whole.restore(state);
    CampaignParty pieces(module());
    pieces.restore(state);
    whole.advance_time_milliseconds(6000);
    pieces.advance_time_milliseconds(1);
    const auto settled = encode_campaign(pieces, nullptr, "chill-death-saves");
    CampaignParty loaded(module());
    loaded.restore(
        decode_campaign(settled, *srd5::character_rules(), *rules, "chill-death-saves", nullptr)
        .party);
    loaded.advance_time_milliseconds(1999);
    loaded.advance_time_milliseconds(4000);
    check(encode_campaign(loaded, nullptr, "chill-death-saves") ==
          encode_campaign(whole, nullptr, "chill-death-saves"),
          "Death saves resolved in the first moment continue identically after save/load");
    const auto &settled_vitals = whole.member(id).vitals;
    // Independent SplitMix64 calculation: seed 17's first d20 is 20, so more
    // than one draw proves the blocked natural 20 counted only as a success.
    check(settled_vitals.hit_points == 0 &&
          (settled_vitals.dead ||
           settled_vitals.description.find("Stable") != std::string::npos) &&
          whole.state().random_state != 17 + 0x9e3779b97f4a7c15ULL,
          "While Chill Touch prevents healing, a natural 20 is a success, not 1 HP");
}

void persistence()
{
    auto rules = custom();
    for (const auto *klass :
            {"wizard", "sorcerer", "warlock"
            })
    {
        CampaignParty party(module());
        auto h = hero(klass);
        const auto id = party.add_pc(h);
        auto stage = party.checkpoint();
        stage.roster[0].vitals = blocked_state(*rules, h, 1);
        party.restore(stage);
        auto bytes = encode_campaign(party, nullptr, "chill");
        CampaignParty copy(module());
        copy.restore(
            decode_campaign(bytes, *srd5::character_rules(), *module(), "chill", nullptr).party);
        check(encode_campaign(copy, nullptr, "chill") == bytes,
              "Campaign grant and effect round trip");
        auto actors = copy.participants();
        std::uint64_t random = 17;
        rules->elapse(actors, 8999, random);
        check(fx::healing_blocked(effects(*actors[0].state)), "Campaign expiry not early");
        rules->elapse(actors, 1, random);
        check(!fx::healing_blocked(effects(*actors[0].state)) && random == 17,
              "Exact outside combat expiry, no RNG");
        (void)id;
    }
}

void earned_lifecycle()
{
    fx::LifeState life{0, 0, 0, true, false, {0, 1000}};
    check(!fx::advance_recovery_clock(life, 1000, false) && life.recovery.stable_recovery_due,
          "Blocked deadline retains earned recovery");
    auto random = std::uint64_t{17};
    const auto pending = life;
    fx::stabilize(life, random);
    fx::start_stable_recovery(life, random);
    check(life == pending && random == 17,
          "Earned recovery cannot reroll on repeated stabilization");
    for (bool buffer :
            {
                false, true
            })
    {
        auto damaged = pending;
        if (buffer)
            damaged.temporary_hp = {5, "test:buffer"};
        fx::damage_life(damaged, 1, 20);
        check(!damaged.stable && !damaged.recovery.stable_recovery_due && damaged.failures == 1,
              "Damage cancels earned recovery even through Temporary HP");
        check(!fx::advance_recovery_clock(damaged, 9000) && damaged.hp == 0,
              "Cancelled recovery cannot heal later");
    }
    fx::damage_life(life, 0, 20);
    check(life == pending, "Zero resolved damage preserves earned recovery");
    check(fx::advance_recovery_clock(life, 1) && life.hp == 1 &&
          life.recovery == fx::RecoveryClock{},
          "Earned recovery grants exactly one HP once allowed");
    for (const auto &invalid : std::vector<fx::LifeState> {{1, 0, 0, true, false, {0, 0, true}},
    {0, 0, 0, true, true, {0, 0, true}},
    {0, 0, 0, false, false, {0, 0, true}},
    {0, 0, 0, true, false, {0, 1, true}}
})
    rejects(
        [&]
    {
        fx::validate_recovery(invalid);
    });
}

void stable_timeline()
{
    for (auto mode :
            {
                fx::RecoveryMode::campaign, fx::RecoveryMode::combat
            })
        for (unsigned deadline :
                {
                    999u, 1000u, 1001u, 3000u, 4000u
                })
        {
            fx::LifeState life{0, 0, 0, true, false, {0, deadline}}, other{10};
            fx::EffectState effect, other_effect;
            fx::apply_chill_touch(effect, 1, 99, "First", 1000);
            fx::apply_chill_touch(effect, 1, 98, "Second", 3000);
            fx::apply_blindness(effect, 1, 98, "Blind", 30, 1000);
            fx::apply_blindness(other_effect, 1, 99, "Blind", 30, 1000);
            auto pieces = life, other_pieces = other;
            auto piece_effect = effect, other_piece_effect = other_effect;
            std::vector<fx::RecoverySubject> whole{{{2, other_effect, {}}, other},
                {{1, effect, {}}, life}};
            std::vector<fx::RecoverySubject> chunked{{{2, other_piece_effect, {}}, other_pieces},
                {{1, piece_effect, {}}, pieces}};
            std::uint64_t random = 17, piece_random = 17;
            fx::elapse_recovery(whole, 2999, random, mode);
            for (unsigned step :
                    {
                        1u, 998u, 1u, 999u, 1000u
                    })
                fx::elapse_recovery(chunked, step, piece_random, mode);
            check(life == pieces && effect == piece_effect && other_effect == other_piece_effect &&
                  random == piece_random,
                  "Shared effect/deadline ordering and RNG are partition invariant");
            check(life.hp == 0 && life.recovery.stable_recovery_due == (deadline < 3000),
                  "No recovery before last blocking source expires");
            fx::elapse_recovery(whole, 1, random, mode);
            check(life.hp == (deadline <= 3000 ? 1 : 0),
                  "Earned recovery at exact final expiry; future deadline is preserved");
            fx::elapse_recovery(whole, 1000, random, mode);
            check(life.hp == 1, "Recovery eventually occurs at the later boundary");
        }
}

VitalState stable_blocked(unsigned deadline = 1000)
{
    return {0, false,
            "SRD11 2 0 0 0 0 1 2 0 " + std::to_string(deadline) +
            " 0 \"\" 2 1 0 0 0 0 FX8 2 1 1 4 5 99 \"Enemy\" 0 9000 0 0"};
}

void stable_continuation()
{
    auto rules = module();
    auto h = hero("fighter", 2);
    CampaignParty party(module());
    const auto id = party.add_pc(h);
    auto stage = party.checkpoint();
    stage.roster[0].vitals = stable_blocked();
    stage.random_state = 17;
    stage.next_combat_scope = 6;
    party.restore(stage);
    party.advance_time_milliseconds(1000);
    const auto pending = party.member(id).vitals;
    check(pending.hit_points == 0 && pending.resources.find("14400001") != pending.resources.npos &&
          party.state().random_state == 17,
          "Earned recovery stored without additional d4 draw");
    auto bytes = encode_campaign(party, nullptr, "earned-recovery");
    CampaignParty loaded(module());
    loaded.restore(
        decode_campaign(bytes, *srd5::character_rules(), *rules, "earned-recovery", nullptr).party);
    check(encode_campaign(loaded, nullptr, "earned-recovery") == bytes,
          "Pending earned recovery round trips exactly");
    auto no_block = pending;
    no_block.resources.replace(no_block.resources.find("FX8"), std::string::npos, "FX8 1 0 0");
    rejects(
        [&]
    {
        rules->validate_character_state(h.sheet(), no_block);
    });
    loaded.advance_time_milliseconds(7999);
    check(loaded.member(id).vitals.hit_points == 0 && loaded.state().random_state == 17,
          "Deferred recovery waits without reroll");
    loaded.advance_time_milliseconds(1);
    party.advance_time_milliseconds(8000);
    check(loaded.member(id).vitals.hit_points == 1 && loaded.state().random_state == 17 &&
          encode_campaign(loaded, nullptr, "earned-recovery") ==
          encode_campaign(party, nullptr, "earned-recovery"),
          "Saved earned recovery resumes at exact expiry");
    auto c = rules->create({{8, 8, std::vector<std::uint8_t>(64)},
        {   {
                1,
                "campaign-character",
                "Patient",
                0,
                {1, 1},
                rules->character_profile(h.sheet(), {}).data,
                pending
            },
            {2, "vanguard", "Companion", 0, {3, 1}},
            {99, "vanguard", "Enemy", 1, {6, 1}}
        }},
    13);
    check(unit(*c).hit_points == 0, "Combat retains due recovery while blocked");
    auto copy = rules->restore(c->save());
    const auto random = rng(*c);
    for (unsigned turns = 0; turns < 8 && unit(*c).hit_points == 0; ++turns)
    {
        act(*c, "end");
        act(*copy, "end");
        check(c->save() == copy->save(), "Pending combat recovery continues exactly after reload");
    }
    check(unit(*c).hit_points == 1 && rng(*c) == random,
          "Combat expiry restores earned HP without RNG");
}

void stable_actual_cast()
{
    auto rules = custom("affinity target test immunity necrotic\n");
    auto h = hero();
    auto c = rules->create({{8, 8, std::vector<std::uint8_t>(64)},
        {   {
                1,
                "campaign-character",
                "Caster",
                0,
                {1, 1},
                rules->character_profile(h.sheet(), {}).data
            },
            {
                2,
                "target",
                "Patient",
                0,
                {2, 1},
                {},
                VitalState{0, false, "SRD11 0 0 0 0 0 1 0 0 5000 0 \"\" 0 0 0 0 0 0 FX8 1 0 0"}
            },
            {99, "vanguard", "Enemy", 1, {6, 1}}
        }},
    13);
    while (c->snapshot().actor != 1)
        act(*c, "end");
    check(unit(*c, 2).hit_points == 0, "Stable before actual cast");
    act(*c, "chill_touch", 2);
    check(blocked(*c, 2) && unit(*c, 2).hit_points == 0,
          "Actual immune target takes zero damage and keeps Stable with prevention");
    const auto expiry =
        c->snapshot().elapsed_milliseconds + effects(unit(*c, 2).persistent).active[0].remaining_ms;
    bool saw_due = false;
    auto random = rng(*c);
    while (c->snapshot().elapsed_milliseconds < expiry)
    {
        if (unit(*c, 2).persistent.resources.find("14400001") != std::string::npos)
        {
            saw_due = true;
            auto copy = rules->restore(c->save());
            check(copy->save() == c->save(), "Actual cast creates persistent earned recovery");
        }
        act(*c, "end");
        check(unit(*c, 2).hit_points == (c->snapshot().elapsed_milliseconds < expiry ? 0 : 1),
              "Actual Stable target heals only at expiry");
    }
    check(saw_due && rng(*c) == random,
          "Actual cast delays the existing deadline without another recovery roll");
}

void fixtures()
{
    auto path = std::filesystem::path(OPENGOLD_BINARY_DIR) / "chill-fixtures";
    std::filesystem::create_directories(path);
    auto rules = module();
    for (const auto *klass :
            {"wizard", "sorcerer", "warlock"
            })
    {
        auto h = hero(klass);
        auto profile = rules->character_profile(h.sheet(), {}).data;
        auto c = rules->create({{12, 9, std::vector<std::uint8_t>(108)},
            {   {1, "campaign-character", "Caster", 0, {1, 1}, profile},
                {2, "vanguard", "Ally", 0, {2, 1}},
                {99, "vanguard", "Enemy", 1, {5, 1}}
            }},
        2);
        while (c->snapshot().actor != 1)
            act(*c, "end");
        std::ofstream(path / (std::string(klass) + ".save"), std::ios::binary) << c->save();
        act(*c, "chill_touch", 2);
        std::ofstream(path / (std::string(klass) + "-blocked.save"), std::ios::binary) << c->save();
    }
}
} // namespace

int main()
{
    try
    {
        access();
        damage();
        timing();
        skipped_caster();
        campaign_handoff();
        legality();
        lifecycle();
        recovery();
        healing_spells();
        combat_death_save();
        death_saves_outside_combat();
        persistence();
        earned_lifecycle();
        stable_timeline();
        stable_continuation();
        stable_actual_cast();
        fixtures();
        std::cout << "Chill Touch tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
