#include "campaign_fixture.h"
#include "opengold/campaign_save.h"
#include "opengold/srd5.h"
#include "life_cycle.h"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
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
    check(caught, "Invalid recovery operation must reject");
}

auto module()
{
    return srd5::load(std::filesystem::path(OPENGOLD_SOURCE_DIR) /
                      "data/rules/srd-5.2.1/combat.rules");
}

Character hero()
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = "fighter";
    d.background = "soldier";
    d.alignment = "neutral_good";
    d.name = "Recovery tester";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    Character result(*srd5::character_rules(), d, {});
    VitalState scratch;
    check(result.advance(*module(), scratch), "Fixture has two levels");
    return result;
}

void golden_transitions()
{
    life::LifeState state{0, 2, 1, false, false, {1234, 0}};
    auto rng = std::uint64_t{17};
    check(life::death_save(state, rng) == 20 && state.hp == 1 && !state.stable && !state.dead &&
              state.successes == 0 && state.failures == 0 &&
              state.recovery == life::RecoveryClock{} && rng == 11400714819323198502ULL,
          "Natural 20 heals one HP and clears counters/timers with one draw");
    state = {0, 2, 1, false, false, {0, 0}};
    rng = 29;
    check(life::death_save(state, rng) == 1 && state.dead && state.failures == 3 &&
              state.recovery == life::RecoveryClock{} && rng == 11400714819323198514ULL,
          "Natural 1 adds two failures, kills and cancels clocks");
    state = {0, 2, 1, false, false, {0, 0}};
    rng = 9;
    check(life::death_save(state, rng) == 9 && !state.dead && state.successes == 2 &&
              state.failures == 2 && state.recovery.death_save_in_ms == 6000,
          "An ordinary failure retains prior successes and schedules the next turn");
    state = {0, 2, 1, false, false, {0, 0}};
    rng = 34;
    check(
        life::death_save(state, rng) == 10 && state.stable && state.hp == 0 && !state.dead &&
            state.successes == 0 && state.failures == 0 && state.recovery.death_save_in_ms == 0 &&
            state.recovery.stable_recovery_in_ms == 7200000 && rng == 4354685564936845388ULL,
        "Third success clears both counters and consumes exactly one known 1d4-hour recovery roll");
    const auto stable = state;
    const auto rolled = rng;
    life::stabilize(state, rng);
    life::start_stable_recovery(state, rng);
    check(state == stable && rng == rolled,
          "Repeated stabilization cannot reroll the existing recovery duration");
    rejects(
        [&]
        {
            (void)life::death_save(state, rng);
        });
    check(state == stable && rng == rolled, "Stable creatures make no death save");
    check(!life::advance_recovery_clock(state, 7199999) && state.hp == 0 &&
              state.recovery.stable_recovery_in_ms == 1,
          "Stable recovery does not heal a millisecond early");
    check(life::advance_recovery_clock(state, 1) && state.hp == 1 && !state.stable &&
              state.recovery == life::RecoveryClock{},
          "Recovery grants exactly one HP at its deadline");
    check(!life::advance_recovery_clock(state, std::numeric_limits<std::uint64_t>::max()) &&
              state.hp == 1 && rng == rolled,
          "Later elapsed time repeats neither healing nor a recovery roll");
    state = stable;
    life::damage_life(state, 0, 20);
    check(state == stable, "Zero damage does not end Stable");
    life::damage_life(state, 1, 20);
    check(!state.stable && !state.dead && state.failures == 1 &&
              state.recovery == life::RecoveryClock{6000, 0},
          "Damage at zero HP ends Stable, adds a failure and cancels natural recovery");
    life::damage_life(state, 1, 20, true);
    check(state.dead && state.failures == 3 && state.recovery == life::RecoveryClock{},
          "A critical hit at zero HP adds two failures");
    const auto dead = state;
    rejects(
        [&]
        {
            (void)life::heal_life(state, 1, 20);
        });
    check(state == dead, "Ordinary healing cannot revive the dead");
    state = stable;
    check(life::heal_life(state, 0, 20) == 0 && state == stable,
          "Zero healing cannot cancel unconscious recovery");
    check(life::heal_life(state, 4, 20) == 4 && state.hp == 4 && !state.stable &&
              state.recovery == life::RecoveryClock{},
          "Positive healing cancels both clocks");
    state = {5, 0, 0, false, false, {}};
    life::damage_life(state, 25, 20);
    check(state.dead && state.recovery == life::RecoveryClock{},
          "Damage left over equal to maximum HP kills immediately");
}

void legacy_and_validation()
{
    life::LifeState state{0, 2, 1, false, false, {}};
    life::initialize_legacy_recovery(state);
    check(state.recovery == life::RecoveryClock{6000, 0},
          "Legacy unstable state starts with a full campaign turn and no invented elapsed time");
    state = {0, 0, 0, true, false, {}};
    life::initialize_legacy_recovery(state);
    auto rng = std::uint64_t{42};
    check(state.recovery == life::RecoveryClock{},
          "Legacy Stable state defers its duration roll until time starts");
    life::start_stable_recovery(state, rng);
    check(state.recovery.stable_recovery_in_ms == 7200000 && rng == 11400714819323198527ULL,
          "Deferred legacy recovery rolls once when initialized");
    for (const auto invalid : std::vector<life::LifeState>{{1, 0, 0, false, false, {1, 0}},
                                                           {0, 0, 0, false, true, {1, 0}},
                                                           {0, 0, 0, false, false, {6001, 0}},
                                                           {0, 0, 0, true, false, {1, 1}},
                                                           {0, 0, 0, false, false, {0, 1}},
                                                           {0, 0, 0, true, false, {0, 14400001}}})
        rejects(
            [&]
            {
                life::validate_recovery(invalid);
            });
}

std::string fixture(const char *name)
{
    std::ifstream in(std::filesystem::path(OPENGOLD_SOURCE_DIR) / "tests/fixtures" / name);
    check(bool(in), "Frozen fixture exists");
    return {std::istreambuf_iterator<char>(in), {}};
}

std::vector<std::string> rows(std::string_view bytes)
{
    std::istringstream in{std::string(bytes)};
    std::vector<std::string> result;
    for (std::string line; std::getline(in, line);)
        result.push_back(line);
    return result;
}

std::string join(const std::vector<std::string> &lines)
{
    std::string result;
    for (const auto &line : lines)
        result += line + '\n';
    return result;
}

std::string upgraded(std::string_view bytes)
{
    auto lines = rows(bytes);
    check(lines[0].starts_with("OGCOMBAT 9 "), "Frozen writer is combat nine");
    lines[0].replace(9, 1, "13");
    lines[0].replace(lines[0].find("0.6.10"), 6, module()->identity().version);
    const std::string old_content = "srd-5.2.1-demo.1/15052881321234871607";
    lines[0].replace(lines[0].find(old_content), old_content.size(), module()->identity().content);
    for (unsigned i = 4; i < 8; ++i)
    {
        lines[i] += lines[i].starts_with("2 ") ? " 4500 0" : " 0 0";
        lines[i] += " 0 \"\" 0 0";
    }
    lines.push_back("0");
    lines.push_back("0");
    return join(lines);
}

void frozen_saves()
{
    auto rules = module();
    const auto before = fixture("combat-v9-recovery.save");
    auto combat = rules->restore(before);
    check(
        combat->save() == upgraded(before),
        "Old combat retains all state and RNG while adding known next-turn timing and deferred Stable recovery");
    const auto current = combat->save();
    check(rules->restore(current)->save() == current,
          "Current restore adds no roll, time or duplicate recovery");
    const auto commands = combat->legal_commands();
    const auto decline = std::find_if(commands.begin(), commands.end(),
                                      [](const auto &c)
                                      {
                                          return c.verb == "decline";
                                      });
    check(decline != commands.end() && combat->submit(*decline) &&
              combat->save() == upgraded(fixture("combat-v9-recovery-continued.save")),
          "Pending opportunity decisions preserve the previous writer's exact continuation");
    const auto old = fixture("campaign-v10-recovery.ogs");
    auto disk = decode_campaign(old, *srd5::character_rules(), *rules, "recovery-fixture", nullptr);
    CampaignParty party(module());
    party.restore(disk.party);
    const auto saved = encode_campaign(party, nullptr, "recovery-fixture");
    auto body = old.substr(old.find('\n', old.find('\n') + 1) + 1);
    body.replace(body.find("0.6.10"), 6, rules->identity().version);
    const std::string old_content = "srd-5.2.1-demo.1/15052881321234871607";
    body.replace(body.find(old_content), old_content.size(), rules->identity().content);
    check(
        saved.substr(saved.find('\n', saved.find('\n') + 1) + 1) ==
            test::with_action_surge_grants(test::with_initial_wizard_spell_grants(body),
                                           {true, true, true, true}),
        "Campaign migration adds sourced spell grants and updates module identity; unknown elapsed recovery is never invented");
    check(party.member(1).vitals.hit_points == 0 && party.member(3).vitals.dead &&
              party.member(5).vitals.hit_points == 0,
          "Stable, dead and reserve fixtures retain vitality");
}

CombatantView actor(const CombatSession &combat, EntityId id)
{
    const auto s = combat.snapshot();
    for (const auto &a : s.combatants)
        if (a.id == id)
            return a;
    throw std::runtime_error("Missing actor");
}

void combat_and_campaign()
{
    auto rules = module();
    const auto character = hero();
    const auto profile = rules->character_profile(character.sheet(), {});
    const VitalState stable{0, false, "SRD5 1 0 0 0 0 1 1 0 1000 FX1 1 0"};
    Encounter encounter{{8, 8, std::vector<std::uint8_t>(64)},
                        {{1, "campaign-character", "Patient", 0, {0, 0}, profile.data, stable},
                         {2, "vanguard", "Companion", 0, {2, 0}},
                         {99, "vanguard", "Enemy", 1, {7, 7}}}};
    encounter.participants[0].state->resources = "SRD5 1 0 0 0 0 1 1 0 10000 FX1 1 0";
    auto combat = rules->create(encounter, 42);
    auto copy = rules->restore(combat->save());
    for (unsigned n = 0; n < 12 && actor(*combat, 1).hit_points == 0; ++n)
    {
        auto commands = combat->legal_commands();
        const auto end = std::find_if(commands.begin(), commands.end(),
                                      [](const auto &c)
                                      {
                                          return c.verb == "end";
                                      });
        check(end != commands.end() && combat->submit(*end) && copy->submit(*end) &&
                  combat->save() == copy->save(),
              "Countdown continues identically after a combat checkpoint");
    }
    check(actor(*combat, 1).hit_points == 1 &&
              actor(*combat, 1).persistent.resources == "SRD4 1 0 0 0 0 0 1 FX4 1 0 0 1",
          "Natural recovery preserves the spent Hit Die and Second Wind use");
    auto state = stable;
    rules->set_hit_points(state, character.sheet(), 4);
    check(state.hit_points == 4 && state.resources == "SRD4 1 0 0 0 0 0 1 FX4 1 0 0 1",
          "Script healing clears mortality clocks without restoring resources");
    rules->set_hit_points(state, character.sheet(), 0);
    check(state.resources == "SRD5 1 0 0 0 0 0 1 6000 0 FX4 1 0 0 1",
          "Script loss to zero HP begins a fresh cadence");
    const auto fallen = state;
    rules->set_hit_points(state, character.sheet(), 0);
    check(state == fallen, "Reading unchanged script HP does not restart the timer");
    CampaignParty party(module());
    const auto id = party.add_pc(character);
    auto checkpoint = party.checkpoint();
    checkpoint.roster[0].vitals = stable;
    party.restore(checkpoint);
    const auto saved = encode_campaign(party, nullptr, "clock");
    auto disk = decode_campaign(saved, *srd5::character_rules(), *rules, "clock", nullptr);
    CampaignParty restored(module());
    restored.restore(disk.party);
    check(encode_campaign(restored, nullptr, "clock") == saved,
          "Campaign persists rolled timers exactly without a load-time RNG draw");
    party.complete_training(id, *srd5::character_rules(),
                            {{"origin:languages", {"elvish", "orc"}},
                             {"class:fighter:fighting_style", {"archery"}},
                             {"class:fighter", {"athletics", "history"}},
                             {"background:soldier:gaming_set", {"dice"}}});
    check(party.member(id).vitals == stable, "Review Training preserves the recovery continuation");
    party.award_experience(900, "recovery-xp");
    party.advance(id, party.default_advancement(id));
    check(party.member(id).vitals.resources == "SRD5 1 0 0 0 0 1 2 0 1000 FX1 1 0",
          "Advancement adds only its new Hit Die and retains the exact Stable deadline");
    auto healed = stable;
    auto rng = std::uint64_t{42};
    rules->temple_heal(healed, character.sheet(), rng);
    check(healed.hit_points > 0 && healed.resources == "SRD4 1 0 0 0 0 0 1 FX4 1 0 0 1",
          "Temple healing cancels the recovery clock without replenishing pools");
    for (const auto invalid :
         {"SRD5 1 0 0 0 0 1 1 1 1000 FX1 1 0", "SRD5 1 0 0 0 0 1 1 0 14400001 FX1 1 0",
          "SRD5 1 0 0 0 0 0 1 6001 0 FX1 1 0", "SRD5 1 0 0 0 0 0 1 0 1 FX1 1 0",
          "SRD5 1 0 0 0 0 0 1 -1 0 FX1 1 0"})
        rejects(
            [&]
            {
                rules->validate_character_state(character.sheet(), {0, false, invalid});
            });
    rejects(
        [&]
        {
            rules->validate_character_state(character.sheet(), {1, false, stable.resources});
        });
    auto bad = rows(combat->save());
    for (unsigned i = 4; i < 7; ++i)
        if (bad[i].starts_with("1 "))
            bad[i].replace(bad[i].rfind(' ') + 1, std::string::npos, "1");
    rejects(
        [&]
        {
            (void)rules->restore(join(bad));
        });
}
} // namespace

int main()
{
    try
    {
        golden_transitions();
        legacy_and_validation();
        frozen_saves();
        combat_and_campaign();
        std::cout << "Recovery clock tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
