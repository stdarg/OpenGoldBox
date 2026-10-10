#include "opengold/campaign_save.h"
#include "opengold/srd5.h"
#include "life_cycle.h"
#include <algorithm>
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

void validation()
{
    life::LifeState state{0, 0, 0, true, false, {}};
    auto rng = std::uint64_t{42};
    life::start_stable_recovery(state, rng);
    check(state.recovery.stable_recovery_in_ms == 7200000 && rng == 11400714819323198527ULL,
          "Starting Stable recovery rolls its duration once");
    for (const auto &invalid : std::vector<life::LifeState> {{1, 0, 0, false, false, {1, 0}},
    {0, 0, 0, false, true, {1, 0}},
    {0, 0, 0, false, false, {6001, 0}},
    {0, 0, 0, true, false, {1, 1}},
    {0, 0, 0, false, false, {0, 1}},
    {0, 0, 0, true, false, {0, 14400001}}
})
    rejects(
        [&]
    {
        life::validate_recovery(invalid);
    });
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

// Replaces one whitespace-separated (possibly quoted) field of a checkpoint row.
std::string with_field(const std::string &row, unsigned index, std::string_view value)
{
    std::istringstream fields(row);
    std::string field;
    for (unsigned n = 0; n < index; ++n)
        fields >> std::quoted(field);
    fields >> std::ws;
    const auto begin = static_cast<std::size_t>(fields.tellg());
    fields >> field;
    return row.substr(0, begin) + std::string(value) + row.substr(begin + field.size());
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
    const VitalState stable{0, false, "SRD11 1 0 0 0 0 1 1 0 1000 0 \"\" 0 1 0 0 0 0 FX8 1 0 0"};
    // Awake again: the spent Hit Die and Second Wind use remain, the clocks are gone.
    // Only combat leaves a recovered character Prone.
    const std::string recovered = "SRD11 1 0 0 0 0 0 1 0 0 0 \"\" 0 1 0 0 0 0 FX8 1 0 0";
    const std::string recovered_prone = "SRD11 1 0 0 0 0 0 1 0 0 0 \"\" 0 1 0 0 0 0 FX8 1 0 1";
    Encounter encounter{{8, 8, std::vector<std::uint8_t>(64)},
        {   {1, "campaign-character", "Patient", 0, {0, 0}, profile.data, stable},
            {2, "vanguard", "Companion", 0, {2, 0}},
            {99, "vanguard", "Enemy", 1, {7, 7}}
        }};
    encounter.participants[0].state->resources =
        "SRD11 1 0 0 0 0 1 1 0 10000 0 \"\" 0 1 0 0 0 0 FX8 1 0 0";
    auto combat = rules->create(encounter, 42);
    auto copy = rules->restore(combat->save());
    for (unsigned n = 0; n < 12 && actor(*combat, 1).hit_points == 0; ++n)
    {
        auto commands = combat->legal_commands();
        const auto end = std::find_if(commands.begin(), commands.end(),
                                      [](const auto & c)
        {
            return c.verb == "end";
        });
        check(end != commands.end() && combat->submit(*end) && copy->submit(*end) &&
              combat->save() == copy->save(),
              "Countdown continues identically after a combat checkpoint");
    }
    check(actor(*combat, 1).hit_points == 1 &&
          actor(*combat, 1).persistent.resources == recovered_prone,
          "Natural recovery preserves the spent Hit Die and Second Wind use");
    auto state = stable;
    rules->set_hit_points(state, character.sheet(), 4);
    check(state.hit_points == 4 && state.resources == recovered,
          "Script healing clears mortality clocks without restoring resources");
    rules->set_hit_points(state, character.sheet(), 0);
    check(state.resources == "SRD11 1 0 0 0 0 0 1 6000 0 0 \"\" 0 1 0 0 0 0 FX8 1 0 0",
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
    party.award_experience(900, "recovery-xp");
    party.advance(id, party.default_advancement(id));
    check(party.member(id).vitals.resources ==
          "SRD11 1 0 0 0 0 1 2 0 1000 0 \"\" 0 1 0 0 0 0 FX8 1 0 0",
          "Advancement adds only its new Hit Die and retains the exact Stable deadline");
    auto healed = stable;
    RandomState rng{42};
    rules->temple_heal(healed, character.sheet(), rng);
    check(healed.hit_points > 0 && healed.resources == recovered,
          "Temple healing cancels the recovery clock without replenishing pools");
    for (const auto invalid :
            {"SRD11 1 0 0 0 0 1 1 1 1000 0 \"\" 0 1 0 0 0 0 FX8 1 0 0",
             "SRD11 1 0 0 0 0 1 1 0 14400001 0 \"\" 0 1 0 0 0 0 FX8 1 0 0",
             "SRD11 1 0 0 0 0 0 1 6001 0 0 \"\" 0 1 0 0 0 0 FX8 1 0 0",
             "SRD11 1 0 0 0 0 0 1 0 1 0 \"\" 0 1 0 0 0 0 FX8 1 0 0",
             "SRD11 1 0 0 0 0 0 1 -1 0 0 \"\" 0 1 0 0 0 0 FX8 1 0 0"
            })
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
    // Published actor field 28 is the death-save deadline; a conscious actor
    // cannot carry one.
    auto bad = rows(combat->save());
    for (unsigned i = 4; i < 7; ++i)
        if (bad[i].starts_with("1 "))
            bad[i] = with_field(bad[i], 28, "1");
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
        validation();
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
