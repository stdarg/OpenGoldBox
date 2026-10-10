#include "opengold/campaign_save.h"
#include "opengold/combat_demo.h"
#include "opengold/srd5.h"
#include "combat_fixture.h"
#include "status_effects.h"
#include "weapons.h"
#include <fstream>
#include <cstdlib>
#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>

using namespace opengold;
using namespace opengold::rules;
namespace fx = opengold::srd5::detail;

namespace
{
void check(bool value, const char *message)
{
    if (!value)
        throw std::runtime_error(message);
}

template <class F> void rejects(F f)
{
    bool failed = false;
    try
    {
        f();
    }
    catch (const std::exception &)
    {
        failed = true;
    }
    check(failed, "Invalid effect input accepted");
}

// An effect's scope and source, and a save's DC and first-save delay, are
// integers of the same kind side by side, so they travel in named structs:
// passed loose, in either order, they must not compile (Effective C++ Item 18).
template <class... Arguments>
concept AppliesBlindness = requires(fx::EffectState &effects, Arguments... arguments)
{
    fx::apply_blindness(effects, arguments...);
};
static_assert(AppliesBlindness<fx::EffectSource, std::string, fx::RepeatingSave>);
static_assert(!AppliesBlindness<std::uint64_t, EntityId, std::string, int, unsigned>);
static_assert(!AppliesBlindness<EntityId, std::uint64_t, std::string, unsigned, int>);

// Command's option is a CommandOption, so its stored number cannot be passed
// where the duration goes, or the other way round.
template <class Option, class Duration>
concept AppliesCommand = requires(fx::EffectState &effects, Option option, Duration duration)
{
    fx::apply_command(effects, fx::EffectSource{}, "Caster", option, duration);
};
static_assert(AppliesCommand<fx::CommandOption, unsigned>);
static_assert(!AppliesCommand<int, unsigned>);
static_assert(!AppliesCommand<unsigned, fx::CommandOption>);

auto module()
{
    return srd5::load(std::filesystem::path(OPENGOLD_SOURCE_DIR) /
                      "data/rules/srd-5.2.1/combat.rules");
}

Command command(const CombatSession &s, std::string_view verb, EntityId target = 0)
{
    for (const auto &c : s.legal_commands())
        if (c.verb == verb && (!target || c.target == target))
            return c;
    throw std::runtime_error("Missing command: " + std::string(verb));
}

bool offers(const CombatSession &s, std::string_view verb)
{
    const auto cs = s.legal_commands();
    return std::any_of(cs.begin(), cs.end(),
                       [&](const auto & c)
    {
        return c.verb == verb;
    });
}

CombatantView unit(const CombatSession &s, EntityId id)
{
    for (const auto &a : s.snapshot().combatants)
        if (a.id == id)
            return a;
    throw std::runtime_error("Missing actor");
}

fx::EffectState effects(const VitalState &state)
{
    const auto where = state.resources.find("FX8");
    if (where == std::string::npos)
        return {};
    std::istringstream input(state.resources.substr(where));
    return fx::read_effects(input);
}

// Replaces the effect tail of a complete vital record taken from a live session.
VitalState with_effects(VitalState state, const fx::EffectState &effects)
{
    const auto at = state.resources.find("FX8");
    check(at != std::string::npos, "Fixture has complete resource state");
    std::ostringstream out;
    fx::write_effects(out, effects);
    state.resources.replace(at, std::string::npos, out.str());
    return state;
}

fx::EffectState blind(int dc = 38, unsigned remaining = 60000)
{
    fx::EffectState result;
    fx::apply_blindness(result, {.scope = 77, .actor = 99}, "Source caster",
                        {.dc = dc, .first_save_ms = 6000});
    result.active.front().remaining_ms = remaining;
    return result;
}

Encounter encounter()
{
    return {{12, 9, std::vector<Terrain>(108)},
        {   {1, "blindness-adept", "Caster", Side::party, {2, 4}},
            {2, "bandit", "Target", Side::opposition, {3, 4}}
        },
        123};
}

Character character(std::string klass, std::string name)
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = klass;
    d.alignment = "neutral_good";
    d.background = "sage";
    d.name = std::move(name);
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    return Character(*srd5::character_rules(), d, {});
}

void saving_throws()
{
    std::array<bool, 20> seen{};
    for (std::uint64_t seed = 0; seed < 500; ++seed)
    {
        auto rng = seed;
        const auto result = fx::saving_throw(fx::Ability::constitution, 3, 14, {}, rng);
        seen[result.natural - 1] = true;
        check(result.success == (result.natural + 3 >= 14), "Save total meets DC exactly");
        for (auto ability :
                {
                    fx::Ability::strength, fx::Ability::dexterity, fx::Ability::constitution,
                    fx::Ability::intelligence, fx::Ability::wisdom, fx::Ability::charisma
                })
        {
            rng = seed;
            const auto same = fx::saving_throw(ability, 3, 14, {}, rng);
            check(same.success == result.success, "All abilities share the resolver");
        }
        rng = seed;
        check(fx::saving_throw(fx::Ability::wisdom, 100, 21, {}, rng).success,
              "Natural 1 can pass an ordinary save");
        rng = seed;
        check(!fx::saving_throw(fx::Ability::wisdom, -100, 1, {}, rng).success,
              "Natural 20 can fail an ordinary save");
        auto normal = seed, cancelled = seed, advantage = seed, disadvantage = seed;
        const auto roll = fx::d20({}, normal);
        check(fx::d20({true, true}, cancelled) == roll && cancelled == normal,
              "Cancellation consumes only one roll");
        check(fx::d20({true, false}, advantage) >= roll &&
              fx::d20({false, true}, disadvantage) <= roll,
              "Advantage chooses high, disadvantage low");
    }
    check(std::all_of(seen.begin(), seen.end(),
                      [](bool b)
    {
        return b;
    }),
    "Every natural save result exercised");
    check(fx::saving_modifiers(fx::Ability::dexterity, true, true).mode() == 0,
          "Dodge and untrained armor cancel for Dex saves");
    check(fx::saving_modifiers(fx::Ability::strength, true, true).mode() == -1,
          "Dodge does not improve Strength saves");
    for (bool a :
            {
                false, true
            })
        for (bool t :
                {
                    false, true
                })
            for (bool dodge :
                    {
                        false, true
                    })
                for (bool other :
                        {
                            false, true
                        })
                {
                    const auto actual = fx::attack_modifiers(fx::AttackConditions{
                        .attacker_blind = a,
                        .target_blind = t,
                        .target_dodging = dodge,
                        .other_disadvantage = other});
                    check(actual.mode() == int(t) - int(a || (dodge && !t) || other),
                          "Complete attack modifier truth table");
                }
}

void lifecycle()
{
    auto one = blind(), many = one;
    fx::apply_blindness(one, {.scope = 88, .actor = 99}, "Another caster",
                        {.dc = 13, .first_save_ms = 3000});
    many = one;
    one.active[0].remaining_ms = many.active[0].remaining_ms = 18000;
    std::uint64_t a = 25, b = 25;
    unsigned events_a = 0, events_b = 0;
    std::array<int, 6> saves{};
    saves[2] = -100;
    std::array<fx::EffectSubject, 1> subjects_a{{{1, one, saves}}}, subjects_b{{{1, many, saves}}};
    fx::elapse_effects(subjects_a, 19000, a,
                       [&](const auto &)
    {
        ++events_a;
    });
    for (unsigned i = 0; i < 19; ++i)
        fx::elapse_effects(subjects_b, 1000, b,
                           [&](const auto &)
    {
        ++events_b;
    });
    check(one == many && a == b && events_a == events_b,
          "Time chunking preserves effects, recovery rolls and notifications");
    check(one.active.size() == 1 && fx::blinded(one),
          "Expiring one application preserves overlapping blindness");
    fx::elapse_effects(subjects_a, 41000, a);
    check(one.active.empty(), "Duration expires despite failed recovery rolls");
    auto success = blind(13);
    saves[2] = 100;
    std::array<fx::EffectSubject, 1> subject{{{1, success, saves}}};
    fx::elapse_effects(subject, 5999, a);
    check(fx::blinded(success), "Recovery waits for its scheduled boundary");
    fx::elapse_effects(subject, 1, a);
    check(!fx::blinded(success), "Successful recovery removes its application");
    // Globally order simultaneous saves by entity ID, including outside combat.
    auto low = blind(13), high = blind(13), low2 = low, high2 = high;
    std::array<fx::EffectSubject, 2> order1{{{2, high, {}}, {1, low, {}}}},
    order2{{{1, low2, {}}, {2, high2, {}}}};
    a = b = 42;
    fx::elapse_effects(order1, 6000, a);
    fx::elapse_effects(order2, 6000, b);
    check(low == low2 && high == high2 && a == b,
          "Roster order does not change simultaneous recovery");
    auto dead = blind();
    std::array<fx::EffectSubject, 1> corpse{{{1, dead, {}, true}}};
    const auto before = a;
    fx::elapse_effects(corpse, 60000, a);
    check(dead.active.empty() && a == before,
          "Dead creatures expire effects without rolling saves");
}

void codec()
{
    const auto original = blind();
    std::ostringstream output;
    fx::write_effects(output, original);
    std::istringstream input(output.str());
    check(fx::read_effects(input) == original, "All effect fields round trip");
    for (const auto *bad :
            {"FX1 2 0", "FX8 0 0 0", "FX8 -1 0 0", "FX8 2 129 0",
             "FX8 2 1 1 99 77 99 \"Caster\" 13 60000 6000 0",
             "FX8 2 1 1 1 0 99 \"Caster\" 13 60000 6000 0",
             "FX8 2 1 1 1 77 99 \"Caster\" 13 0 6000 0",
             "FX8 2 1 1 1 77 99 \"Caster\" 13 60001 6000 0",
             "FX8 2 1 1 1 77 99 \"Caster\" 13 60000 0 0",
             "FX8 2 1 2 1 77 99 \"Caster\" 13 60000 6000 0"
            })
        rejects(
            [&]
    {
        std::istringstream in(bad);
        (void)fx::read_effects(in);
    });
    for (std::size_t n = 0; n < output.str().size() - 1; ++n)
    {
        // Only require failure where truncation removes a whole required field.
        if (output.str()[n] == ' ')
            rejects(
                [&]
        {
            std::istringstream in(output.str().substr(0, n));
            (void)fx::read_effects(in);
        });
    }
}

void combat()
{
    auto rules = module();
    auto e = encounter();
    auto s = rules->create(e, 3);
    check(s->snapshot().actor == 1 && offers(*s, "blindness"),
          "Fixture caster can select Blindness");
    const auto cast = command(*s, "blindness");
    const auto before = s->save();
    auto invalid = cast;
    invalid.target = 12345;
    check(!s->submit(invalid) && s->save() == before,
          "Rejected cast changes neither effects nor RNG");
    check(s->submit(cast) && !s->submit(cast), "Cast accepted once");
    const auto target = unit(*s, 2);
    const auto effect = effects(target.persistent);
    check(effect.active.size() == 1 && effect.active[0].source_scope == 123 &&
          effect.active[0].source_actor == 1 && effect.active[0].dc == 13,
          "Application retains scoped source and original DC");
    check(target.conditions.size() == 1 &&
          unit(*s, 1).persistent.resources.starts_with("SRD11 0 2 1 "),
          "Status appears and exactly one second-level slot is spent");
    check(!offers(*s, "blindness") && !offers(*s, "magic_missile"),
          "Action and one-slot-per-turn limits enforced");
    auto restored = rules->restore(s->save());
    check(restored->save() == s->save(), "Active effects survive checkpoint exactly");
    auto corrupt = s->save();
    const auto fx_position = corrupt.find("FX8 2 1");
    check(fx_position != std::string::npos, "Active effect is encoded");
    corrupt.replace(fx_position, 3, "FX9");
    rejects(
        [&]
    {
        (void)rules->restore(corrupt);
    });
    for (unsigned i = 0; i < 24; ++i)
    {
        const auto end = command(*s, "end");
        check(s->submit(end) && restored->submit(end), "Continued end turn accepted");
        check(s->save() == restored->save(), "Recovery saves replay identically");
    }
    check(effects(unit(*s, 2).persistent).active.empty(),
          "Effect expires or is saved against within one minute");
    check(s->snapshot().elapsed_milliseconds == 72000, "Twelve full rounds consume 72 seconds");
    // Sight-dependent spells and opportunity attacks consult the same condition.
    auto baseline = rules->create(e, 3);
    e.participants[0].state = with_effects(unit(*baseline, 1).persistent, blind());
    s = rules->create(e, 3);
    check(!offers(*s, "blindness") && !offers(*s, "magic_missile") && offers(*s, "fire_bolt"),
          "Blindness blocks sight-required spells; attack spells remain possible");
    s->submit(command(*s, "fire_bolt"));
    const auto logs = s->snapshot().log();
    check(std::any_of(logs.begin(), logs.end(),
                      [](const auto & l)
    {
        return l.find("(disadvantage)") != std::string::npos;
    }),
    "Blind attacker has disadvantage");
    e = encounter();
    baseline = rules->create(e, 3);
    e.participants[1].state = with_effects(unit(*baseline, 2).persistent, blind());
    s = rules->create(e, 3);
    auto movement = command(*s, "move");
    for (const auto &c : s->legal_commands())
        if (c.verb == "move" && c.destination == Cell{1, 4})
            movement = c;
    s->submit(movement);
    check(!s->snapshot().reaction_pending, "Blind enemy cannot make an opportunity attack");
    // Attacks against blinded targets gain advantage; their Dodge cannot see us.
    s = rules->create(e, 3);
    s->submit(command(*s, "melee"));
    const auto advantage_log = s->snapshot().log();
    check(std::any_of(advantage_log.begin(), advantage_log.end(),
                      [](const auto & l)
    {
        return l.find("(advantage)") != std::string::npos;
    }),
    "Attacks against blinded targets gain advantage");
    // First save succeeds with this independent seed, still spending the spell.
    e = encounter();
    s = rules->create(e, 42);
    s->submit(command(*s, "blindness"));
    check(unit(*s, 2).conditions.empty() &&
          unit(*s, 1).persistent.resources.starts_with("SRD11 0 2 1 "),
          "Initial successful save prevents condition but spends slot");
    // Non-divisor actor counts must still telescope to exactly six seconds.
    e = encounter();
    for (unsigned i = 3; i <= 7; ++i)
        e.participants.push_back({i, "bandit", "Extra", Side::opposition, {int(i), 1}});
    s = rules->create(e, 3);
    for (unsigned i = 0; i < 7; ++i)
        s->submit(command(*s, "end"));
    check(s->snapshot().elapsed_milliseconds == 6000,
          "Seven initiative slots still equal six seconds");
    // Class-trained saves reach combat for both created characters and recruited
    // NPCs, with the same resolution path used for curated monster profiles.
    for (const auto *klass :
            {"fighter", "wizard", "cleric"
            })
    {
        auto person = character(klass, klass);
        const auto profile = rules->character_profile(person.sheet(), {});
        e = encounter();
        e.participants[1].character_profile = profile.data;
        s = rules->create(e, 3);
        check(s->snapshot().actor == 1, "Save-bonus fixture initiative");
        s->submit(command(*s, "blindness"));
        const auto log = s->snapshot().log();
        const auto expected = " + " + std::to_string(person.sheet().saving_throws[2]) + " vs DC 13";
        check(std::any_of(log.begin(), log.end(),
                          [&](const auto & line)
        {
            return line.find(expected) != std::string::npos;
        }),
        "Character Constitution proficiency is used by actual combat saves");
    }
}

void campaign()
{
    auto rules = module();
    CampaignParty party(module());
    const auto pc = party.add_pc(character("wizard", "Player"));
    const auto npc = party.recruit("test:npc", character("cleric", "Companion"), 100);
    const auto reserve = party.add_pc(character("fighter", "Reserve"));
    party.remove(reserve);
    party.award_experience(900, "condition-test");
    for (auto id :
            {
                pc, npc
            })
        for (unsigned level = 2; level <= 3; ++level)
        {
            auto choice = party.default_advancement(id);
            if (level == 3 && !choice.spell_learning)
                choice.spells = {"blindness"};
            party.advance(id, choice);
        }
    auto actors = party.participants();
    check(actors.size() == 2, "PC and recruited NPC share participants");
    for (std::size_t i = 0; i < actors.size(); ++i)
        actors[i].cell = {int(i), 0};
    actors.push_back({999, "bandit", "Enemy", Side::opposition, {10, 0}});
    auto session = rules->create({{12, 9, std::vector<Terrain>(108)}, actors}, 3);
    auto state = party.checkpoint();
    for (auto &member : state.roster)
    {
        VitalState vitals;
        if (member.id == reserve)
        {
            const auto p = party.profile(reserve);
            Participant r{reserve, "campaign-character", "Reserve", Side::party, {1, 1},
                          p.data,  member.vitals};
            auto e = encounter();
            e.participants[0] = r;
            auto fixture = rules->create(e, 3);
            vitals = unit(*fixture, reserve).persistent;
        }
        else
            vitals = unit(*session, member.id).persistent;
        member.vitals = with_effects(vitals, blind());
    }
    party.restore(state);
    party.advance_time(std::chrono::milliseconds(1234));
    const auto saved = encode_campaign(party, nullptr, "conditions");
    auto loaded = decode_campaign(saved, *srd5::character_rules(), *rules, "conditions", nullptr);
    CampaignParty restored(module());
    restored.restore(loaded.party);
    check(encode_campaign(restored, nullptr, "conditions") == saved,
          "PC, NPC, reserve effects and sub-minute clock survive campaign save");
    party.advance_time(std::chrono::milliseconds(59999));
    restored.advance_time(std::chrono::milliseconds(59999));
    check(encode_campaign(party, nullptr, "conditions") ==
          encode_campaign(restored, nullptr, "conditions"),
          "Exploration recovery continues deterministically");
    for (const auto &m : party.state().roster)
        check(effects(m.vitals).active.empty(),
              "PC, NPC and reserve effects expire on elapsed time");
    // Begin a later encounter without changing the persistent source provenance.
    restored.restore(loaded.party);
    actors = restored.participants();
    actors.push_back({999, "bandit", "Enemy", Side::opposition, {10, 0}});
    session = rules->create(
    {{12, 9, std::vector<Terrain>(108)}, actors, restored.state().next_combat_scope}, 3);
    check(effects(unit(*session, pc).persistent).active[0].source_scope == 77,
          "Original source identity survives a new encounter");
    restored.begin_combat();
    restored.apply_combat(session->snapshot());
    for (unsigned i = 0; i < 3; ++i)
    {
        session->submit(command(*session, "end"));
        restored.apply_combat(session->snapshot());
    }
    const auto once = restored.checkpoint();
    restored.apply_combat(session->snapshot());
    check(restored.state().time_minutes == once.time_minutes &&
          restored.state().subminute_milliseconds == once.subminute_milliseconds,
          "Repeated synchronization cannot advance time twice");
    check(restored.state().subminute_milliseconds == 7234,
          "Campaign time includes exactly one combat round");
    const auto reserve_fx = effects(restored.member(reserve).vitals);
    check(reserve_fx.active.empty() || reserve_fx.active[0].remaining_ms == 52766,
          "Reserve effects advance during combat too");
    restored.end_combat();
    check(restored.rest(), "Eligible rest remains supported");
    for (const auto &m : restored.state().roster)
        check(effects(m.vitals).active.empty(), "Rest advances lasting effects for entire roster");
    const auto rest_save = encode_campaign(restored, nullptr, "conditions");
    CampaignParty rested_copy(module());
    rested_copy.restore(
        decode_campaign(rest_save, *srd5::character_rules(), *rules, "conditions", nullptr).party);
    check(encode_campaign(rested_copy, nullptr, "conditions") == rest_save,
          "Fractional rest completion survives saving");
    while (restored.state().spell_rest)
        restored.keep_rest_spells(restored.state().spell_rest->members.front());
    restored.advance_time(std::chrono::milliseconds(960ULL * 60000 - 1));
    check(!restored.rest(), "Rest cannot become eligible one millisecond early");
    restored.advance_time(std::chrono::milliseconds(1));
    check(restored.rest(), "Rest is eligible at the exact sixteen-hour boundary");
}

void original_encounter_scope()
{
    auto party = std::make_shared<CampaignParty>(module());
    const auto id = party->add_pc(character("wizard", "Expedition caster"));
    party->award_experience(900, "scope-test");
    for (unsigned level = 2; level <= 3; ++level)
    {
        auto choice = party->default_advancement(id);
        if (level == 3 && !choice.spell_learning)
            choice.spells = {"blindness"};
        party->advance(id, choice);
    }
    auto state = party->checkpoint();
    state.next_combat_scope = 11;
    party->restore(state);
    CombatDemo demo(module());
    demo.campaign_party(party);
    CampaignEncounter encounter;
    encounter.field.geometry = {40, 25, std::vector<Terrain>(1000)};
    encounter.enemies = {{999, "bandit", "Original encounter target", Side::opposition, {}}};
    demo.encounter(std::move(encounter), 3);
    check(party->state().next_combat_scope == 12,
          "Original encounter reserves its unique campaign identity");
    check(demo.submit(command(demo.combat(), "blindness")),
          "Created wizard casts in original-geometry adapter");
    const auto applied = effects(unit(demo.combat(), 999).persistent);
    check(applied.active.size() == 1 && applied.active[0].source_scope == 11,
          "Original encounter propagates its source scope into effects");
}

void checkpoint_capacity()
{
    auto rules = module();
    auto full = blind();
    full.active.front().source_name = std::string(160, '"');
    while (fx::can_apply(full))
        fx::apply_blindness(full, {.scope = 77, .actor = 99}, std::string(160, '"'),
                            {.dc = 13, .first_save_ms = 6000});
    check(full.active.size() == fx::effect_limit, "Application count is bounded");
    const auto bandit = unit(*rules->create(encounter(), 3), 2).persistent;
    Encounter e{{12, 9, std::vector<Terrain>(108)}, {}};
    for (unsigned n = 0; n < 64; ++n)
    {
        const auto side = n % 2 ? Side::opposition : Side::party;
        Participant p{n + 1, "bandit", "Crowded actor", side, {int(n % 12), int(n / 12)}};
        p.state = with_effects(bandit, full);
        e.participants.push_back(std::move(p));
    }
    auto session = rules->create(e, 3);
    const auto saved = session->save();
    check(saved.size() > 1024 * 1024 && saved.size() < 4 * 1024 * 1024,
          "Worst supported collections fit the checkpoint budget");
    check(rules->restore(saved)->save() == saved,
          "Maximum actors/effects and escaped source names remain loadable");
}

using opengold::test::logged;

#include "mastery_combat_checks.h"
#include "nick_attack_checks.h"
#include "mastery_choice_checks.h"
#include "slow_mastery_checks.h"
#include "graze_checks.h"
#include "optional_mastery_checks.h"
} // namespace

int main()
{
    try
    {
        optional_mastery_checks::run();
        graze_checks::run();
        mastery_combat_checks::run();
        nick_attack_checks::run();
        slow_mastery_checks::run();
        saving_throws();
        lifecycle();
        codec();
        combat();
        campaign();
        original_encounter_scope();
        checkpoint_capacity();
        std::cout << "Status effect tests passed\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
