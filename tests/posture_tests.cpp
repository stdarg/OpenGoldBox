// Posture, gear and dying rules after SIMPLIFY-1: a rest-interrupting encounter
// finds sleepers awake but Prone, downed characters keep their gear, Prone ends
// with combat, and whoever is still dying at victory is bandaged, as in the
// original game.
#include "opengold/campaign_party.h"
#include "opengold/srd5.h"
#include "combat_grid.h"
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
void check(bool ok, const char *message)
{
    if (!ok)
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
    check(failed, "Invalid posture state accepted");
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
    d.name = "Sleeper";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    return Character(*srd5::character_rules(), d, {});
}

CombatantView unit(const CombatSession &s, EntityId id)
{
    for (const auto &a : s.snapshot().combatants)
        if (a.id == id)
            return a;
    throw std::runtime_error("Missing actor");
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
    const auto commands = s.legal_commands();
    return std::any_of(commands.begin(), commands.end(),
                       [&](const auto & c)
    {
        return c.verb == verb;
    });
}

void turn(CombatSession &s, EntityId id)
{
    for (unsigned n = 0; n < 20 && s.snapshot().actor != id; ++n)
        check(s.submit(command(s, "end")), "Advance fixture turn");
    check(s.snapshot().actor == id, "Requested conscious turn reached");
}

std::size_t log_count(const CombatSession &s, std::string_view line)
{
    const auto log = s.snapshot().log();
    return std::size_t(std::count(log.begin(), log.end(), std::string(line)));
}

void codec()
{
    fx::EffectState state;
    state.prone = true;
    std::ostringstream out;
    fx::write_effects(out, state);
    check(out.str() == "FX8 1 0 1", "Canonical posture codec");
    std::istringstream in(out.str());
    check(fx::read_effects(in) == state, "Posture round trips");
    for (const auto bad :
            {"FX8 1 0 2", "FX8 1 0 -1", "FX8 1 0", "FX7 1 0 0 1"
            })
        rejects(
            [&]
    {
        std::istringstream input(bad);
        (void)fx::read_effects(input);
    });
}

void rest_ambush()
{
    const auto rules = module();
    CampaignParty party(module());
    const auto sleeper = party.add_pc(hero());
    const auto watcher = party.add_pc(hero());
    auto participants = party.participants();
    participants[0].cell = {2, 2};
    participants[0].resting = true;
    participants[1].cell = {2, 3};
    participants.push_back({3, "bandit", "Enemy", Side::opposition, {6, 2}});
    auto s = rules->create({{9, 7, std::vector<Terrain>(63)}, participants}, 37);
    const auto woken = unit(*s, sleeper);
    check(woken.prone && woken.conscious && !unit(*s, watcher).prone,
          "A rest-interrupting encounter starts the resting character awake and Prone");
    check(log_count(*s, "Sleeper wakes up prone.") == 1, "Waking up Prone is logged once");
    check(!offers(*s, "wake_ally"), "There is no Wake ally action");
    turn(*s, sleeper);
    const auto output = std::filesystem::path(OPENGOLD_BINARY_DIR) / "posture-fixtures";
    std::filesystem::create_directories(output);
    {
        std::ofstream out(output / "stand.save", std::ios::binary);
        out << s->save();
        check(bool(out), "Posture UI fixture written");
    }
    const auto movement = unit(*s, sleeper).movement_feet;
    check(unit(*s, sleeper).action, "A woken character keeps its Action");
    check(s->submit(command(*s, "stand_up")), "A woken character can stand up");
    check(!unit(*s, sleeper).prone && unit(*s, sleeper).action &&
          unit(*s, sleeper).movement_feet == movement - 15,
          "Standing costs half Speed without an Action");
    const auto stood = s->save();
    check(rules->restore(stood)->save() == stood, "Standing continuation round trips");
}

void downed_keeps_gear()
{
    const auto rules = module();
    const auto person = hero();
    const std::array<std::string, 2> gear{"longsword", "shield"};
    const auto armed = rules->character_profile(person.sheet(), gear);
    bool witnessed = false;
    for (unsigned seed = 1; seed < 100 && !witnessed; ++seed)
    {
        auto hit = rules->create({{8, 8, std::vector<Terrain>(64)},
            {   {1, "bandit", "Attacker", Side::opposition, {2, 2}},
                {2, "campaign-character", "Wounded", Side::party, {3, 2}, armed.data, VitalState{1}}
            }},
        seed);
        if (hit->snapshot().actor != 1)
            continue;
        check(hit->submit(command(*hit, "melee", 2)), "Damage fixture attack");
        const auto fallen = unit(*hit, 2);
        if (fallen.hit_points)
            continue;
        witnessed = true;
        const auto items = hit->snapshot().held_items;
        check(std::all_of(items.begin(), items.end(),
                          [](const auto & item)
        {
            return item.holder == 2;
        }),
        "A character at zero HP keeps its weapon and shield");
        check(fallen.prone && fallen.armor_class == armed.armor_class,
              "The downed character lies Prone and its shield still counts");
        check(!offers(*hit, "pick_up"), "There is nothing to pick up");
        check(rules->restore(hit->save())->save() == hit->save(),
              "A downed character with its gear round trips");
    }
    check(witnessed, "An actual attack dropped a character to zero HP");
}

// A level-one Fighter at zero HP, dying, with its next death save due.
VitalState dying()
{
    return {0, false, "SRD11 0 0 0 0 0 0 1 6000 0 0 \"\" 0 0 0 0 0 0 FX8 1 0 1"};
}

void bandaged_at_victory()
{
    const auto rules = module();
    const auto person = hero();
    const auto profile = rules->character_profile(person.sheet(), {}).data;
    bool witnessed = false;
    for (unsigned seed = 1; seed < 200 && !witnessed; ++seed)
    {
        auto c = rules->create({{8, 8, std::vector<Terrain>(64)},
            {   {1, "campaign-character", "Victor", Side::party, {2, 2}, profile},
                {2, "campaign-character", "Fallen", Side::party, {1, 1}, profile, dying()},
                {3, "bandit", "Enemy", Side::opposition, {3, 2}, {}, VitalState{1}}
            }},
        seed);
        for (unsigned n = 0; n < 20 && c->snapshot().outcome == Outcome::ongoing; ++n)
            check(c->submit(command(*c, c->snapshot().actor == 1 && offers(*c, "melee")
                                    ? "melee" : "end")),
                  "Victory fixture command");
        if (c->snapshot().outcome != Outcome::victory)
            continue;
        const auto log = c->snapshot().log();
        const auto victory = std::find(log.begin(), log.end(), "Victory.");
        const auto bandaged = std::find(victory, log.end(), "Fallen is bandaged and stable.");
        if (bandaged == log.end())
            continue;
        witnessed = true;
        const auto fallen = unit(*c, 2);
        check(!fallen.dead && fallen.hit_points == 0 &&
              fallen.persistent.description.find("Stable") != std::string::npos,
              "A character still dying at victory is bandaged and Stable");
        check(std::none_of(victory, log.end(), [](const auto & line)
        {
            return line.starts_with("Fallen death save: ");
        }), "No death save is rolled after victory");
        check(std::count(log.begin(), log.end(), "Victory.") == 1, "Victory is announced once");
        check(!fallen.prone && !unit(*c, 1).prone, "Prone does not outlast combat");
        check(rules->restore(c->save())->save() == c->save(), "Resolved victory round trips");
    }
    check(witnessed, "A victory with a dying ally was exercised");
}

void campaign_death_saves()
{
    for (unsigned seed = 1; seed <= 32; ++seed)
    {
        fx::LifeState life{0, 0, 0, false, false, fx::RecoveryClock{6000, 0}};
        fx::EffectState effects;
        std::uint64_t rng = seed;
        std::array<fx::RecoverySubject, 1> subjects{{{{1, effects, {}}, life}}};
        fx::elapse_recovery(subjects, 1, rng);
        check(life.dead || life.hp == 1 || life.stable,
              "Outside combat a dying character's death saves resolve in the first moment");
        check(!effects.prone, "Recovering outside combat does not leave the character Prone");
        fx::LifeState combat_life{0, 0, 0, false, false, fx::RecoveryClock{6000, 0}};
        std::array<fx::RecoverySubject, 1> combatants{{{{1, effects, {}}, combat_life}}};
        std::uint64_t combat_rng = seed;
        fx::elapse_recovery(combatants, 1, combat_rng, fx::RecoveryMode::combat);
        check(combat_life.hp == 0 && !combat_life.stable && !combat_life.dead &&
              combat_rng == seed,
              "Combat keeps its turn-entry death saves");
    }
}

void movement()
{
    Battlefield board{5, 5, std::vector<Terrain>(25)};
    board.terrain[2 * 5 + 3] = Terrain::difficult;
    fx::MovementGrid grid(board, {2, 2}, {}, true);
    check(grid.step_cost({2, 2}, {2, 3}) == 10 && grid.step_cost({2, 2}, {3, 2}) == 15,
          "Crawling and difficult terrain add independent movement costs");
    check(grid.reachable(14).cost_to({3, 2}) == std::nullopt &&
          grid.reachable(15).cost_to({3, 2}) == 15,
          "Crawling reach uses exact weighted path cost");
}
} // namespace

int main()
{
    try
    {
        codec();
        rest_ambush();
        downed_keeps_gear();
        bandaged_at_victory();
        campaign_death_saves();
        movement();
        std::cout << "Posture tests passed\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
