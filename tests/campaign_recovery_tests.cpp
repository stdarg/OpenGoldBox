#include "opengold/campaign_save.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
using namespace opengold;
using namespace opengold::rules;

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
    check(caught, "Invalid elapsed operation must reject");
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

std::string saved(const CampaignParty &p)
{
    return encode_campaign(p, nullptr, "campaign-recovery");
}

CampaignParty loaded(std::string_view bytes)
{
    CampaignParty p(module());
    p.restore(
        decode_campaign(bytes, *srd5::character_rules(), *module(), "campaign-recovery", nullptr)
        .party);
    return p;
}

VitalState unstable(unsigned delay = 6000)
{
    return {0, false, "SRD9 0 0 0 2 1 0 1 " + std::to_string(delay) + " 0 0 \"\" 0 1 0 FX8 1 0 0",
            "Second Wind uses: 0 / 2\nUnconscious; death saves 2 successes, 1 failures"};
}

VitalState stable(unsigned delay)
{
    return {0, false, "SRD9 0 0 0 0 0 1 1 0 " + std::to_string(delay) + " 0 \"\" 0 1 0 FX8 1 0 0",
            "Second Wind uses: 0 / 2\nStable, unconscious"};
}

Participant patient(EntityId id, VitalState state)
{
    return {id,
            "campaign-character",
            "Patient",
            0,
            {},
            module()->character_profile(hero().sheet(), {}).data,
            std::move(state)};
}

void golden_events()
{
    auto rules = module();
    std::vector<Participant> people{patient(1, unstable())};
    auto rng = std::uint64_t{17};
    rules->elapse(people, 0, rng);
    check(people[0].state == unstable() && rng == 17, "Zero elapsed time rolls nothing");
    rules->elapse(people, 1, rng);
    check(people[0].state->hit_points == 1 &&
          people[0].state->resources == "SRD9 0 0 0 0 0 0 1 0 0 0 \"\" 0 1 0 FX8 1 0 0" &&
          rng == 11400714819323198502ULL,
          "Outside combat the remaining death saves resolve at once; a natural 20 restores one HP "
          "without replenishing spent pools");
    // Death saves resolve first in entity order: entity 1 stabilizes (10) and
    // rolls two hours (2); entity 2 succeeds (18) and rolls four hours (4).
    // Entity 1 then succeeds on its Blinded save (13) at the six-second deadline.
    auto a = unstable();
    a.resources = "SRD9 0 0 0 2 1 0 1 6000 0 0 \"\" 0 1 0 "
                  "FX8 2 1 1 1 77 99 \"Caster\" 13 60000 6000 0";
    people = {patient(2, unstable()), patient(1, a)};
    rng = 34;
    rules->elapse(people, 6000, rng);
    check(
        people[1].state->resources == "SRD9 0 0 0 0 0 1 1 0 7194000 0 \"\" 0 1 0 FX8 2 0 0" &&
        people[0].state->resources == "SRD9 0 0 0 0 0 1 1 0 14394000 0 \"\" 0 1 0 FX8 1 0 0" &&
        rng == 1663341875487337611ULL,
        "Death saves resolve in entity order before simultaneous effect saves");
    rules->elapse(people, 7193999, rng);
    check(people[1].state->hit_points == 0, "Stable recovery waits until its exact deadline");
    rules->elapse(people, 1, rng);
    check(people[1].state->hit_points == 1 && people[0].state->hit_points == 0 &&
          rng == 1663341875487337611ULL,
          "Natural recovery adds no second duration roll");
    // Death suppresses an effect saving throw at the same instant; expiry does not roll.
    a.resources = "SRD9 0 0 0 2 1 0 1 6000 0 0 \"\" 0 1 0 "
                  "FX8 2 1 1 1 77 99 \"Caster\" 38 60000 6000 0";
    people = {patient(1, a)};
    rng = 29;
    rules->elapse(people, 60000, rng);
    check(people[0].state->dead &&
          people[0].state->resources == "SRD9 0 0 0 2 3 0 1 0 0 0 \"\" 0 1 0 FX8 2 0 0" &&
          rng == 11400714819323198514ULL,
          "A natural-one death ends mortality rolls and skips saves on lingering effects");
    people = {patient(1, stable(7200000))};
    rng = 42;
    rules->elapse(people, std::numeric_limits<std::uint64_t>::max(), rng);
    check(people[0].state->hit_points == 1 && rng == 42,
          "Very large elapsed time finishes without overflow or redundant rolls");
}

void partitions_and_rejection()
{
    auto rules = module();
    std::vector<Participant> initial{patient(7, stable(7001)), patient(2, unstable(0)),
                                     patient(4, unstable(2111)), patient(1, stable(0))};
    initial[2].state->resources =
        "SRD9 0 0 0 2 1 0 1 2111 0 0 \"\" 0 1 0 FX8 3 2 1 1 77 99 \"First\" 38 43123 1111 "
        "2 1 77 98 \"Second\" 18 57000 5111 0";
    for (std::uint64_t seed = 0; seed < 32; ++seed)
    {
        auto whole = initial, split = initial;
        std::reverse(split.begin(), split.end());
        auto big_rng = seed, small_rng = seed;
        rules->elapse(whole, 14406001, big_rng);
        std::uint64_t remaining = 14406001;
        for (const auto amount :
                {
                    1ULL, 1110ULL, 1000ULL, 3890ULL, 7123ULL, 48001ULL, 3523456ULL, 61ULL
                })
        {
            rules->elapse(split, amount, small_rng);
            remaining -= amount;
        }
        rules->elapse(split, remaining, small_rng);
        std::reverse(split.begin(), split.end());
        for (std::size_t i = 0; i < whole.size(); ++i)
            check(
                whole[i].state == split[i].state,
                "Arbitrary time partitions and roster order retain identical vitality and effects");
        check(big_rng == small_rng, "Partitions cannot reorder recovery RNG draws");
    }
    auto bad = initial;
    bad.back().state->resources = "bad";
    auto rng = std::uint64_t{34};
    const auto first = *bad.front().state;
    rejects(
        [&]
    {
        rules->elapse(bad, 60000, rng);
    });
    check(*bad.front().state == first && rng == 34,
          "Malformed late member cannot partly advance earlier members or RNG");
    bad = initial;
    bad.back().id = bad.front().id;
    rejects(
        [&]
    {
        rules->elapse(bad, 60000, rng);
    });
    check(*bad.front().state == first && rng == 34,
          "Ambiguous participant ordering rejects atomically");
}

void campaign_continuation()
{
    CampaignParty party(module());
    const auto pc = party.add_pc(hero()), npc = party.recruit("clock-npc", hero()),
               reserve = party.add_pc(hero());
    party.remove(reserve);
    auto state = party.checkpoint();
    state.random_state = 34;
    state.roster[0].vitals = unstable();
    state.roster[1].vitals = stable(6001);
    state.roster[2].vitals = unstable(6000);
    party.restore(state);
    const auto before = saved(party);
    party.advance_time_milliseconds(0);
    check(saved(party) == before, "Zero campaign time is read-only");
    party.advance_time_milliseconds(5999);
    auto copy = loaded(saved(party));
    check(saved(copy) == saved(party), "Partial cadence saves with exact RNG and timers");
    party.advance_time_milliseconds(14400001);
    copy.advance_time_milliseconds(1);
    copy.advance_time_milliseconds(14400000);
    check(saved(party) == saved(copy) && party.member(pc).vitals.hit_points == 1 &&
          party.member(npc).vitals.hit_points == 1,
          "PCs, recruited NPCs and reserves continue through save/load and subdivisions");
    check(party.member(reserve).vitals.hit_points != 0 || party.member(reserve).vitals.dead,
          "Reserve mortality continues outside the active party");
    // Waking during another member's rest never grants eligibility retroactively.
    party.restore(state);
    state = party.checkpoint();
    state.roster[0].vitals.hit_points = 1;
    state.roster[0].vitals.resources = "SRD9 0 0 0 0 0 0 1 0 0 0 \"\" 0 1 0 FX8 1 0 0";
    state.roster[1].vitals = stable(1000);
    state.roster[2].vitals = stable(1000);
    party.restore(state);
    const auto rest = party.rest(RestKind::long_rest);
    check(rest && rest->members == std::vector<MemberId> {pc} &&
          party.member(npc).vitals.hit_points == 1 &&
          party.member(reserve).vitals.hit_points == 1,
          "An eight-hour rest advances natural recovery for ineligible companions and reserves");
    for (auto id :
            {
                npc, reserve
            })
        check(!party.member(id).last_rest_minutes &&
              party.member(id).vitals.resources ==
              "SRD9 0 0 0 0 0 0 1 0 0 0 \"\" 0 1 0 FX8 1 0 0",
              "Natural recovery grants neither recharge, Hit Dice nor a rest completion timestamp");
    state = party.checkpoint();
    state.time_minutes = std::numeric_limits<std::uint64_t>::max();
    state.roster[1].vitals = unstable();
    party.restore(state);
    const auto overflow = saved(party);
    rejects(
        [&]
    {
        party.advance_time(1);
    });
    check(saved(party) == overflow, "Clock overflow preserves all vitality and RNG");
}

Command end(const CombatSession &combat)
{
    for (const auto &c : combat.legal_commands())
        if (c.verb == "end")
            return c;
    throw std::runtime_error("No conscious turn available");
}

void combat_handoff()
{
    auto rules = module();
    CampaignParty party(module());
    const auto pc = party.add_pc(hero()), npc = party.recruit("combat-clock-npc", hero()),
               reserve = party.add_pc(hero());
    party.remove(reserve);
    auto state = party.checkpoint();
    state.random_state = 17;
    state.roster[1].vitals = stable(10000);
    state.roster[2].vitals = unstable();
    party.restore(state);
    auto actors = party.participants();
    actors.push_back({999, "bandit", "Enemy", 1, {7, 7}});
    auto combat = rules->create({{8, 8, std::vector<std::uint8_t>(64)}, actors}, 42);
    auto reloaded = rules->restore(combat->save()), twin = rules->restore(combat->save());
    check(reloaded->save() == combat->save(), "Recovery checkpoint reload is exact");
    check(reloaded->submit(end(*reloaded)) && twin->submit(end(*twin)) &&
          reloaded->save() == twin->save(),
          "Reloaded recovery checkpoints continue identically");
    auto direct = loaded(saved(party));
    party.begin_combat();
    party.apply_combat(combat->snapshot());
    std::uint64_t previous = 0;
    for (unsigned turn = 0; turn < 8; ++turn)
    {
        check(combat->submit(end(*combat)), "Complete the conscious combat turn");
        const auto snap = combat->snapshot();
        direct.advance_time_milliseconds(snap.elapsed_milliseconds - previous);
        previous = snap.elapsed_milliseconds;
        party.apply_combat(snap);
        const auto once = party.checkpoint();
        party.apply_combat(snap);
        check(party.state().random_state == once.random_state &&
              party.member(reserve).vitals == once.roster[2].vitals,
              "Repeated combat snapshots never repeat reserve mortality or RNG");
        check(party.member(reserve).vitals == direct.member(reserve).vitals &&
              party.state().random_state == direct.state().random_state,
              "Reserve mortality advances exactly once during combat");
        for (const auto &a : snap.combatants)
            if (a.id == npc)
                check(party.member(npc).vitals == a.persistent,
                      "Combat participant receives only its combat-owned continuation");
        auto bad = snap;
        bad.elapsed_milliseconds += 6000;
        for (auto &a : bad.combatants)
            if (a.id == pc)
                a.max_hit_points += 1;
        rejects(
            [&]
        {
            party.apply_combat(bad);
        });
        check(party.state().random_state == once.random_state &&
              party.member(reserve).vitals == once.roster[2].vitals &&
              party.state().subminute_milliseconds == once.subminute_milliseconds,
              "Rejected snapshot rolls back candidate reserve recovery and time");
    }
    party.end_combat();
    check(party.member(npc).vitals.hit_points == 1,
          "Stable combat countdown wakes its companion once");
    auto copy = loaded(saved(party));
    party.advance_time_milliseconds(6111);
    copy.advance_time_milliseconds(6111);
    check(saved(party) == saved(copy), "Encounter exit continues through campaign save/load");
    // A fresh encounter rebases unstable saves onto initiative, including slot zero.
    state = party.checkpoint();
    state.roster[1].vitals = unstable(123);
    party.restore(state);
    actors = party.participants();
    actors.push_back({999, "bandit", "Enemy", 1, {7, 7}});
    combat = rules->create({{8, 8, std::vector<std::uint8_t>(64)}, actors}, 42);
    party.begin_combat();
    party.apply_combat(combat->snapshot());
    party.end_combat();
    copy = loaded(saved(party));
    party.advance_time_milliseconds(18000);
    copy.advance_time_milliseconds(1);
    copy.advance_time_milliseconds(17999);
    check(saved(party) == saved(copy),
          "Death cadence transfers out of a new initiative without restarting on campaign load");
}

using Bytes = std::vector<std::uint8_t>;

std::shared_ptr<const por::EclProgram> program(Bytes body)
{
    Bytes bytes{0, 0};
    for (int n = 0; n < 5; ++n)
        bytes.insert(bytes.end(), {1, 1, 0x15, 0x99});
    bytes.push_back(0);
    bytes.insert(bytes.end(), body.begin(), body.end());
    return std::make_shared<const por::EclProgram>(por::EclProgram::decode(bytes, "recovery host"));
}

void settle(por::RolfTourSession &town)
{
    for (unsigned n = 0; n < 100 && town.snapshot().phase == por::TourPhase::running; ++n)
        town.advance(.5);
    check(town.snapshot().phase != por::TourPhase::faulted, "Recovery fixture script fault");
}

void event_rollback()
{
    auto party = std::make_shared<CampaignParty>(module());
    party->add_pc(hero());
    const auto reserve = party->add_pc(hero());
    party->remove(reserve);
    auto state = party->checkpoint();
    state.random_state = 17;
    state.roster[1].vitals = unstable();
    party->restore(state);
    auto resources = std::make_shared<por::PhlanResources>();
    resources->programs[0] = program({0});
    // PROGRAM 9 completes a rest; the following unsupported PROGRAM 0 fails.
    auto failed = program({56, 0, 9, 56, 0, 0, 0});
    por::RolfTourSession town({}, failed, {}, 0x9914, {}, resources);
    town.campaign_party(party);
    settle(town);
    const auto before = saved(*party);
    check(town.explore(por::ExplorationCommand::look), "Start the original rest event");
    settle(town);
    check(saved(*party) == before && !town.script_diagnostics().empty(),
          "Failed original event restores mortality rolls, timers, RNG, rest and time");
    auto safe = program({0});
    por::RolfTourSession travel({}, safe, {}, 0x9914, {}, resources);
    travel.campaign_party(party);
    settle(travel);
    check(travel.explore(por::ExplorationCommand::forward), "Ordinary exploration starts");
    settle(travel);
    check(party->member(reserve).vitals.hit_points == 1 &&
          party->state().random_state == 11400714819323198502ULL,
          "Ordinary exploration advances reserve death saves through the original host");
}
} // namespace

int main()
{
    try
    {
        golden_events();
        partitions_and_rejection();
        campaign_continuation();
        combat_handoff();
        event_rollback();
        std::cout << "Campaign recovery tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
