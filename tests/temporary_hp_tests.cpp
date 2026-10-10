#include "opengold/campaign_save.h"
#include "opengold/srd5.h"
#include "life_cycle.h"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace opengold;
using namespace opengold::rules;
namespace life = opengold::srd5::detail;

namespace
{
void check(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
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
    check(caught, "Invalid Temporary HP operation must reject");
}

std::string read(const std::filesystem::path &path)
{
    std::ifstream in(path);
    check(bool(in), "Fixture exists");
    return {std::istreambuf_iterator<char>(in), {}};
}

std::string content()
{
    return read(std::filesystem::path(OPENGOLD_SOURCE_DIR) / "data/rules/srd-5.2.1/combat.rules");
}

auto module()
{
    return srd5::parse_content(content());
}

Character hero(std::string klass = "fighter", unsigned level = 2, bool trained = false)
{
    CharacterDraft d;
    d.race = "dwarf";
    d.gender = "female";
    d.character_class = klass;
    d.background = "soldier";
    d.alignment = "neutral_good";
    d.name = "Temporary HP tester";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    if (trained)
        d.training = {{"class:fighter:fighting_style", {"archery"}},
            {"class:fighter:weapon_mastery", {"dagger", "longsword", "shortbow"}},
            {"class:fighter", {"athletics", "history"}}
        };
    Character result(*srd5::character_rules(), d, {});
    VitalState scratch;
    for (unsigned i = 1; i < level; ++i)
        check(result.advance(*module(), scratch), "Fixture can advance");
    return result;
}

TemporaryHitPoints pool(const Character &character, const VitalState &state)
{
    return module()->recovery_info(character.sheet(), state).temporary_hp;
}

void golden_life()
{
    struct Case
    {
        int hp, temp, damage, want_hp, want_temp;
        bool dead;
    };

    for (const auto &c :
            {
                Case{10, 5, 7, 8, 0, false}, Case{10, 10, 3, 10, 7, false},
                Case{5, 7, 17, 0, 0, false}, Case{5, 7, 22, 0, 0, true}
            })
    {
        life::LifeState s{c.hp};
        s.temporary_hp = {c.temp, "spell:false_life"};
        life::damage_life(s, c.damage, 10);
        check(s.hp == c.want_hp && s.temporary_hp.amount == c.want_temp && s.dead == c.dead,
              "SRD absorption and massive-damage examples use overflow after the buffer");
        check(s.temporary_hp.source_id == (c.want_temp ? "spell:false_life" : ""),
              "Depletion clears only its exhausted source");
    }
    life::LifeState s{0, 0, 0, true, false, {0, 7200000}, {20, "trait:adrenaline_rush"}};
    const auto stable = s;
    life::damage_life(s, 0, 10, true);
    check(s == stable, "Zero resolved damage leaves pool, Stable and timer untouched");
    life::damage_life(s, 1, 10);
    check(
        s.hp == 0 && s.temporary_hp.amount == 19 && !s.stable && s.failures == 1 &&
        s.recovery.death_save_in_ms == 6000,
        "Taking damage at zero HP still ends Stable and causes a failure even when the buffer absorbs it");
    s = stable;
    life::damage_life(s, 1, 10, true);
    check(s.failures == 2 && s.temporary_hp.amount == 19,
          "A buffered critical at zero HP causes two failures");
    s = stable;
    life::damage_life(s, 10, 10);
    check(s.dead && s.temporary_hp.amount == 10,
          "Damage at zero HP reaching maximum HP still kills outright");
    s = stable;
    life::grant_temporary_hp(s, {4, "spell:false_life"}, TemporaryHpChoice::use_new);
    check(s.hp == 0 && s.stable && s.recovery == stable.recovery && s.temporary_hp.amount == 4,
          "A new pool is not healing or stabilization");
    const auto before = s;
    life::grant_temporary_hp(s, {12, "feature:other"}, TemporaryHpChoice::keep_current);
    check(s == before, "Keep explicitly retains the entire old pool and its source");
    life::grant_temporary_hp(s, {2, "feature:other"}, TemporaryHpChoice::use_new);
    check(s.temporary_hp == TemporaryHitPoints{2, "feature:other"},
          "Choosing a smaller new pool replaces, never adds");
    check(life::heal_life(s, 3, 10) == 3 && s.temporary_hp.amount == 2 && !s.stable,
          "Healing restores actual HP and preserves Temporary HP");
    life::set_life_hit_points(s, 1, 10);
    check(s.hp == 1 && s.temporary_hp.amount == 2, "Explicit script HP loss bypasses the buffer");
    s = {10};
    s.temporary_hp = {std::numeric_limits<int>::max(), "feature:large"};
    life::damage_life(s, std::numeric_limits<int>::max(), 10);
    check(s.hp == 10 && s.temporary_hp.amount == 0,
          "Large valid damage/pools do not overflow or manufacture HP loss");
}

void rule_operations()
{
    auto rules = module();
    const auto c = hero();
    VitalState state{4, false, "SRD11 1 0 0 0 0 0 1 0 0 0 \"\" 0 1 0 0 0 0 FX8 1 0 0"};
    rules->grant_temporary_hit_points(state, c.sheet(), {8, "spell:false_life"},
                                      TemporaryHpChoice::use_new);
    check(state.resources ==
          "SRD11 1 0 0 0 0 0 1 0 0 8 \"spell:false_life\" 0 1 0 0 0 0 FX8 1 0 0",
          "Vital state independently records dice, clocks and sourced buffer");
    RandomState rng{42};
    const auto die = rules->spend_hit_die(state, c.sheet(), rng);
    check(die.roll == 4 && die.healing == 6 && state.hit_points == 10 && pool(c, state).amount == 8,
          "Hit Die healing preserves buffer and spends its own die/RNG");
    rules->recover_short_rest(state, c.sheet());
    check(pool(c, state).amount == 8, "Short Rest does not expire Temporary HP");
    rules->set_hit_points(state, c.sheet(), 1);
    check(state.hit_points == 1 && pool(c, state).amount == 8,
          "Campaign script assignment is not typed damage");
    rules->temple_heal(state, c.sheet(), rng);
    check(state.hit_points > 1 && pool(c, state).amount == 8,
          "Temple healing neither spends nor restores Temporary HP");
    auto participants = std::vector<Participant> {{
            1,
            "campaign-character",
            "Patient",
            0,
            {0, 0},
            rules->character_profile(c.sheet(), {}).data,
            state
        }
    };
    const auto before = state;
    rules->elapse(participants, 24ULL * 60 * 60 * 1000, rng);
    check(*participants[0].state == before,
          "Elapsed time alone does not expire an until-Long-Rest pool");
    rules->recover(state, c.sheet());
    check(state.hit_points == c.sheet().hit_points && pool(c, state).amount == 0,
          "An eligible completed Long Rest clears the pool");
    VitalState stable{0, false, "SRD11 1 0 0 0 0 1 1 0 1000 0 \"\" 0 1 0 0 0 0 FX8 1 0 0"};
    rules->grant_temporary_hit_points(stable, c.sheet(), {7, "feature:ward"},
                                      TemporaryHpChoice::use_new);
    participants[0].state = stable;
    const auto prior_rng = rng;
    rules->elapse(participants, 999, rng);
    check(participants[0].state->hit_points == 0 && pool(c, *participants[0].state).amount == 7,
          "Vital state preserves the countdown and buffer before natural recovery");
    rules->elapse(participants, 1, rng);
    check(participants[0].state->hit_points == 1 && pool(c, *participants[0].state).amount == 7 &&
          rng == prior_rng,
          "Natural recovery preserves the pool and does not reroll a saved deadline");
    for (const auto &klass : srd5::character_rules()->choices(CreationField::character_class))
    {
        const auto character = hero(klass.id, 1);
        VitalState vitals{character.sheet().hit_points};
        rules->grant_temporary_hit_points(vitals, character.sheet(), {3, "feature:fixture"},
                                          TemporaryHpChoice::use_new);
        check(pool(character, vitals).amount == 3,
              "Shared Temporary HP works for every class without granting a class feature");
    }
    for (const auto &bad : std::vector<TemporaryHitPoints> {{-1, "source"},
    {1, ""},
    {0, "source"},
    {0, ""},
    {1, "bad source"},
    {1, std::string(129, 'a')}
    })
    {
        const auto unchanged = state;
        rejects(
            [&]
        {
            rules->grant_temporary_hit_points(state, c.sheet(), bad,
            TemporaryHpChoice::use_new);
        });
        check(state == unchanged, "Invalid grant is atomic");
    }
    rejects(
        [&]
    {
        rules->grant_temporary_hit_points(state, c.sheet(), {1, "source"},
        TemporaryHpChoice::keep_current);
    });
    rules->grant_temporary_hit_points(state, c.sheet(), {8, "source"}, TemporaryHpChoice::use_new);
    const auto original = state;
    rejects(
        [&]
    {
        rules->grant_temporary_hit_points(state, c.sheet(), {1, "source"},
        static_cast<TemporaryHpChoice>(99));
    });
    check(state == original, "Unknown replacement choice cannot mutate state");
    for (const auto &bad : std::vector<std::string> {"-1 \"source\"", "1 \"\"", "0 \"source\"",
            "1 \"bad source\"", "2147483648 \"source\""
                                                    })
    {
        auto broken = state;
        broken.resources = "SRD11 1 0 0 0 0 0 1 0 0 " + bad + " 0 1 0 0 0 0 FX8 1 0 0";
        rejects(
            [&]
        {
            rules->validate_character_state(c.sheet(), broken);
        });
    }
}

CombatantView unit(const CombatSession &combat, EntityId id)
{
    for (const auto &a : combat.snapshot().combatants)
        if (a.id == id)
            return a;
    throw std::runtime_error("Missing actor");
}

Command command(const CombatSession &combat, std::string_view verb, EntityId target = 0)
{
    for (const auto &c : combat.legal_commands())
        if (c.verb == verb && (!target || c.target == target))
            return c;
    throw std::runtime_error("Missing command");
}

void actual_combat()
{
    auto rules = srd5::parse_content(
                     content() +
                     "creature toxin 10 500 0 30 30 1 4 3 30 1 4 3 80 320 0 4 30 3 5\ndamage_types toxin poison poison\n");
    const auto c = hero();
    VitalState state{c.sheet().hit_points};
    rules->grant_temporary_hit_points(state, c.sheet(), {1, "spell:fixture"},
                                      TemporaryHpChoice::use_new);
    Encounter encounter{{8, 8, std::vector<std::uint8_t>(64)},
        {   {
                1,
                "campaign-character",
                "Dwarf",
                0,
                {2, 2},
                rules->character_profile(c.sheet(), {}).data,
                state
            },
            {2, "toxin", "Poison attacker", 1, {3, 2}}
        }};
    auto combat = rules->create(encounter, 42);
    while (combat->snapshot().actor != 2)
        check(combat->submit(command(*combat, "end")), "Wait for attacker");
    auto copy = rules->restore(combat->save());
    const auto attack = command(*combat, "melee", 1);
    check(combat->submit(attack) && copy->submit(attack) && combat->save() == copy->save(),
          "Sourced buffer survives checkpoint and deterministic damage continuation");
    int raw = 0, resisted = 0;
    for (const auto &m : combat->snapshot().log_messages)
        if (m.source == "{name}: {type} damage {before} -> {after}.")
            for (const auto &a : m.arguments)
            {
                if (a.name == "before")
                    raw = std::stoi(a.value);
                if (a.name == "after")
                    resisted = std::stoi(a.value);
            }
    check(raw >= 4 && resisted == raw / 2 &&
          unit(*combat, 1).hit_points == c.sheet().hit_points - (resisted - 1) &&
          unit(*combat, 1).temporary_hp.amount == 0,
          "Real Poison attack applies Dwarf resistance before absorbing Temporary HP");
    const auto saved = combat->save();
    check(!combat->submit(attack) && combat->save() == saved,
          "Stale attack cannot consume a pool twice");
    // A forged actor row cannot introduce a negative pool.
    auto corrupt = saved;
    const auto at = corrupt.find(" 0 \"\" 0 0", corrupt.find("PC42"));
    check(at != corrupt.npos, "Current actor has the empty pool suffix");
    corrupt.replace(at, 9, " -1 \"x\" 0 0");
    rejects(
        [&]
    {
        (void)rules->restore(corrupt);
    });
    check(combat->save() == saved, "Rejected restore leaves the original session intact");
}

std::string saved(const CampaignParty &party)
{
    return encode_campaign(party, nullptr, "temporary-hp");
}

CampaignParty loaded(std::string_view bytes)
{
    CampaignParty party(module());
    party.restore(
        decode_campaign(bytes, *srd5::character_rules(), *module(), "temporary-hp", nullptr).party);
    return party;
}

void campaign()
{
    auto rules = module();
    CampaignParty party(module());
    const auto active = party.add_pc(hero("fighter", 2, true)), reserve = party.add_pc(hero());
    party.remove(reserve);
    const auto npc = party.recruit("temp-hp-companion", hero());
    auto state = party.checkpoint();
    for (auto &member : state.roster)
    {
        member.vitals.hit_points = 1;
        rules->grant_temporary_hit_points(member.vitals, member.character.sheet(),
        {8, "spell:fixture"}, TemporaryHpChoice::use_new);
    }
    party.restore(state);
    const auto bytes = saved(party);
    auto copy = loaded(bytes);
    check(saved(copy) == bytes,
          "Campaign canonically preserves sourced pools for active, reserve and NPC members");
    party.award_experience(900, "temporary-hp-xp");
    party.advance(active, party.default_advancement(active));
    check(pool(party.member(active).character, party.member(active).vitals).amount == 8,
          "Advancement preserves the existing pool without scaling it");
    const auto short_rest = party.rest(RestKind::short_rest);
    check(bool(short_rest), "Short Rest completes");
    (void)party.heal_with_hit_dice(*short_rest->spending, active);
    party.finish_short_rest(party.state().short_rest->ticket);
    for (auto id :
            {
                active, reserve, npc
            })
        check(pool(party.member(id).character, party.member(id).vitals).amount == 8,
              "Short Rest time and spending retain all pools");
    const auto rest = party.rest(RestKind::long_rest);
    check(rest && rest->members == std::vector<MemberId> {active, npc},
          "Only active eligible members finish the Long Rest");
    check(pool(party.member(active).character, party.member(active).vitals).amount == 0 &&
          pool(party.member(npc).character, party.member(npc).vitals).amount == 0 &&
          pool(party.member(reserve).character, party.member(reserve).vitals).amount == 8,
          "Long Rest expiry is individual and never applies to a reserve");
    copy = loaded(saved(party));
    check(saved(copy) == saved(party), "Pool expiry remains canonical after reload");
    if (copy.state().training_rest)
    {
        rejects(
            [&]
        {
            copy.begin_combat();
        });
        while (copy.state().training_rest)
            copy.keep_rest_training(copy.state().training_rest->ticket,
                                    copy.state().training_rest->members.front());
    }
    auto actors = copy.participants();
    actors.push_back({99, "bandit", "Enemy", 1, {7, 7}});
    auto battle = rules->create({{8, 8, std::vector<std::uint8_t>(64)}, actors}, 42);
    copy.begin_combat();
    copy.apply_combat(battle->snapshot());
    copy.end_combat();
    check(pool(copy.member(reserve).character, copy.member(reserve).vitals).amount == 8,
          "Encounter snapshots do not overwrite reserve pools");
}
} // namespace

int main()
{
    try
    {
        golden_life();
        rule_operations();
        actual_combat();
        campaign();
        std::cout << "Temporary HP tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
