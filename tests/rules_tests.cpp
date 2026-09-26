#include "opengold/srd5.h"
#include "opengold/combat_demo.h"
#include "opengold/character_rules.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
using namespace opengold;
using namespace opengold::rules;
namespace
{
void check(bool value, const char *message)
{
    if (!value)
        throw std::runtime_error(message);
}
template <class F> void rejects(F &&f, const char *message)
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
    check(failed, message);
}
std::filesystem::path pack()
{
    return std::filesystem::path(OPENGOLD_SOURCE_DIR) / "data/rules/srd-5.2.1/combat.rules";
}
Encounter duel(std::string profile = "vanguard")
{
    Battlefield b{12, 9, std::vector<std::uint8_t>(108, 0)};
    b.terrain[4 * 12 + 4] = 1;
    b.terrain[2 * 12 + 1] = 2;
    return {b, {{1, profile, "Hero", 0, {2, 2}}, {2, "bandit", "Bandit", 1, {3, 2}}}};
}
Command command(const CombatSession &session, std::string_view verb, Cell cell = {})
{
    const auto offered = session.legal_commands();
    auto it = std::find_if(offered.begin(), offered.end(),
                           [&](const auto &c)
                           {
                               return c.verb == verb && (verb != "move" || c.destination == cell);
                           });
    if (it == offered.end())
        throw std::runtime_error("Missing command: " + std::string(verb));
    return *it;
}
std::unique_ptr<CombatSession> hero_first(const RulesModule &module, Encounter encounter)
{
    for (unsigned seed = 0; seed < 100; ++seed)
    {
        auto session = module.create(encounter, seed);
        if (session->snapshot().actor == 1)
            return session;
    }
    throw std::runtime_error("No hero-first seed");
}
std::unique_ptr<CombatSession> actor_first(const RulesModule &module, Encounter encounter,
                                           EntityId id)
{
    for (unsigned seed = 0; seed < 100; ++seed)
    {
        auto session = module.create(encounter, seed);
        if (session->snapshot().actor == id)
            return session;
    }
    throw std::runtime_error("No matching first actor seed");
}
std::unique_ptr<CombatSession> hero_first(const RulesModule &module,
                                          std::string profile = "vanguard")
{
    return hero_first(module, duel(profile));
}
bool offers(const CombatSession &session, std::string_view verb)
{
    const auto commands = session.legal_commands();
    return std::any_of(commands.begin(), commands.end(),
                       [&](const auto &c)
                       {
                           return c.verb == verb;
                       });
}
CombatantView unit(const CombatSession &session, EntityId id)
{
    const auto state = session.snapshot();
    for (const auto &a : state.combatants)
        if (a.id == id)
            return a;
    throw std::runtime_error("Missing combatant");
}
void next_round(CombatSession &session)
{
    const auto round = session.snapshot().round;
    do
    {
        check(session.submit(command(session, "end")), "End turn accepted");
    } while (session.snapshot().round == round || session.snapshot().actor != 1);
}
void turn_budget_tests()
{
    auto module = srd5::load(pack());
    CharacterDraft draft;
    draft.race = "human";
    draft.gender = "female";
    draft.character_class = "wizard";
    draft.background = "sage";
    draft.alignment = "neutral_good";
    draft.name = "Mage";
    draft.rolled = true;
    for (auto &roll : draft.rolls)
        roll = {{6, 5, 4, 1}, 3};
    auto sheet = srd5::character_rules()->evaluate(draft, true);
    VitalState unused;
    for (unsigned level = 2; level <= 3; ++level)
    {
        auto choice = module->default_advancement(sheet);
        if (level == 3)
            choice.spells = {"magic_missile", "scorching_ray", "blindness"};
        check(module->advance_character(sheet, unused, choice),
              "Create a caster with level-two spells");
    }
    for (const auto verb :
         {"melee", "ranged", "fire_bolt", "magic_missile", "magic_missile_2", "scorching_ray"})
        for (const bool move_first : {false, true})
        {
            auto e = duel();
            e.participants[1].definition = "vanguard";
            e.participants.push_back({3, "vanguard", "Reserve enemy", 1, {10, 7}});
            const bool weapon =
                std::string_view(verb) == "melee" || std::string_view(verb) == "ranged";
            if (weapon)
                e.participants[0].state = VitalState{10, false, "SRD1 2 0 0 0 0"};
            else
                e.participants[0].character_profile = module->character_profile(sheet, {}).data;
            auto session = hero_first(*module, e);
            if (move_first)
                check(session->submit(command(*session, "move", {2, 3})), "Move before attacking");
            const auto before = unit(*session, 1);
            const auto attack = command(*session, verb);
            check(session->submit(attack), "Offensive action accepted");
            check(session->snapshot().actor == 1 && !session->snapshot().reaction_pending &&
                      session->snapshot().elapsed_milliseconds == 0,
                  "Attacks and damaging spells retain the active turn without advancing time");
            const auto after = unit(*session, 1);
            check(!after.action && after.bonus_action == before.bonus_action &&
                      after.reaction == before.reaction &&
                      after.movement_feet == before.movement_feet,
                  "Only the action and applicable spell slot are spent by the attack");
            const std::string resources =
                weapon                                      ? "SRD1 2 0 0 0 0"
                : std::string_view(verb) == "fire_bolt"     ? "SRD2 0 4 2 0 0 0"
                : std::string_view(verb) == "magic_missile" ? "SRD2 0 3 2 0 0 0"
                                                            : "SRD2 0 4 1 0 0 0";
            check(after.persistent.resources == resources,
                  "Cantrips preserve slots; leveled spells spend exactly the selected slot");
            const auto saved = session->save();
            auto restored = module->restore(saved);
            check(restored->save() == saved,
                  "A post-attack checkpoint retains unused turn resources");
            auto repeated = attack;
            repeated.revision = session->snapshot().revision;
            check(!session->submit(repeated) && !session->submit(attack) &&
                      session->save() == saved,
                  "Spent actions and stale tickets reject without consuming state or randomness");
            const auto move = command(*session, "move", {3, 3});
            check(session->submit(move) && restored->submit(move) &&
                      session->save() == restored->save(),
                  "Movement after an attack continues identically after reload");
            check(unit(*session, 1).movement_feet == before.movement_feet - 5 &&
                      !unit(*session, 1).action,
                  "Movement neither refreshes the action nor the movement budget");
            if (weapon)
            {
                const auto wind = command(*session, "second_wind");
                check(session->submit(wind) && restored->submit(wind) &&
                          session->save() == restored->save(),
                      "Second Wind remains usable after attacking and moving");
                check(!unit(*session, 1).bonus_action && !unit(*session, 1).action &&
                          unit(*session, 1).persistent.resources == "SRD1 1 0 0 0 0",
                      "Bonus healing does not refund the action or Second Wind");
            }
            const auto end = command(*session, "end");
            check(session->submit(end) && restored->submit(end) &&
                      session->save() == restored->save(),
                  "Explicit End Turn continues deterministically");
            check(session->snapshot().actor != 1 && session->snapshot().elapsed_milliseconds > 0,
                  "End Turn advances initiative and time");
        }
    // Every targeted offensive action may turn the sprite without provoking.
    for (const auto verb : {"melee", "ranged", "fire_bolt", "magic_missile", "magic_missile_2",
                            "scorching_ray", "blindness"})
    {
        auto e = duel(std::string_view(verb) == "blindness" ? "blindness-adept" : "vanguard");
        e.participants[0].facing_left = true;
        e.participants[1].definition = "vanguard";
        e.participants.push_back({3, "vanguard", "Behind attacker", 1, {1, 2}});
        if (std::string_view(verb) != "melee" && std::string_view(verb) != "ranged" &&
            std::string_view(verb) != "blindness")
            e.participants[0].character_profile = module->character_profile(sheet, {}).data;
        auto session = hero_first(*module, e);
        auto attack = command(*session, verb);
        attack.target = 2;
        check(session->submit(attack), "Turning attack or spell accepted");
        check(!unit(*session, 1).facing_left && !session->snapshot().reaction_pending &&
                  session->snapshot().actor == 1,
              "An offensive spell or weapon attack turns only the presentation state");
        check(!unit(*session, 1).action && unit(*session, 3).reaction &&
                  session->snapshot().elapsed_milliseconds == 0,
              "Turning preserves turn budgets and the unprovoked enemy reaction");
        check(module->restore(session->save())->save() == session->save(),
              "Turned attacks and spell expenditure survive reload");
    }
    // A used Bonus Action must stay used when the action is taken afterward.
    auto e = duel();
    e.participants[1].definition = "vanguard";
    e.participants[0].state = VitalState{10, false, "SRD1 2 0 0 0 0"};
    auto session = hero_first(*module, e);
    session->submit(command(*session, "second_wind"));
    session->submit(command(*session, "melee"));
    check(session->snapshot().actor == 1 && !unit(*session, 1).bonus_action &&
              !offers(*session, "second_wind"),
          "Attacking does not refresh a previously used Bonus Action");
    // Movement reactions still interrupt before the step and resume this turn.
    e.participants[0].state.reset();
    for (const auto resolution : {"decline", "opportunity"})
    {
        session = hero_first(*module, e);
        session->submit(command(*session, "melee"));
        check(session->submit(command(*session, "move", {1, 2})) &&
                  session->snapshot().reaction_pending,
              "Moving after an attack can provoke an opportunity reaction");
        auto restored = module->restore(session->save());
        const auto response = command(*session, resolution);
        check(session->submit(response) && restored->submit(response) &&
                  session->save() == restored->save(),
              "Post-attack movement reaction restores and resolves in order");
        check(session->snapshot().actor == 1 && unit(*session, 1).cell == Cell{1, 2} &&
                  unit(*session, 1).movement_feet == 20 && !unit(*session, 1).action,
              "Resolved reaction resumes remaining movement without another action");
    }
    // Multiple leave-reach reactions resolve in order before the move resumes.
    e = duel();
    e.participants[1].definition = "vanguard";
    e.participants.push_back({3, "bandit", "Other guard", 1, {3, 1}});
    session = hero_first(*module, e);
    session->submit(command(*session, "melee"));
    check(session->submit(command(*session, "move", {1, 2})),
          "Post-attack move leaves both enemies' reach");
    unsigned reactions = 0;
    while (session->snapshot().reaction_pending)
    {
        check(++reactions <= 2 && !offers(*session, "end") && !offers(*session, "move"),
              "Pending reactions prevent advancing or moving early");
        auto restored = module->restore(session->save());
        const auto decline = command(*session, "decline");
        check(session->submit(decline) && restored->submit(decline) &&
                  session->save() == restored->save(),
              "Every queued reaction preserves checkpoint continuation");
    }
    check(reactions == 2 && session->snapshot().actor == 1 && !unit(*session, 1).action &&
              unit(*session, 1).movement_feet == 20,
          "All reactions resolve before movement resumes");
    // The enemy controller explicitly ends a spent turn instead of moving
    // toward another attack it cannot make. Available bonus recovery runs first.
    for (const bool injured : {false, true})
    {
        e = duel();
        e.participants[1].definition = "vanguard";
        e.participants[1].cell = {8, 2};
        e.participants[1].facing_left = true;
        if (injured)
            e.participants[1].state = VitalState{5, false, "SRD1 2 0 0 0 0"};
        session = actor_first(*module, e, 2);
        session->submit(command(*session, "ranged"));
        check(session->snapshot().actor == 2, "Enemy attacks also retain their turn");
        if (injured)
        {
            auto choice = choose_demo_command(*session);
            check(choice.verb == "second_wind" && session->submit(choice),
                  "Enemy uses remaining bonus recovery after attacking");
        }
        const auto choice = choose_demo_command(*session);
        check(choice.verb == "end" && session->submit(choice),
              "Enemy explicitly completes its spent turn");
        check(session->snapshot().actor != 2, "Enemy turn cannot stall after its attack");
    }
}
void boundary_tests()
{
    auto module = srd5::load(pack());
    bool reduced = false;
    for (unsigned seed = 0; seed < 40; ++seed)
    {
        auto plain = duel();
        auto ambushed = plain;
        ambushed.participants[0].surprised = true;
        const auto normal = module->create(plain, seed), surprise = module->create(ambushed, seed);
        check(unit(*surprise, 1).initiative <= unit(*normal, 1).initiative,
              "Surprise imposes initiative disadvantage");
        reduced |= unit(*surprise, 1).initiative < unit(*normal, 1).initiative;
        check(module->restore(surprise->save())->save() == surprise->save(),
              "Surprised initiative and RNG survive combat checkpoint");
    }
    check(reduced, "Surprise affects initiative across deterministic seeds");
    auto wide = duel();
    wide.battlefield = {50, 25, std::vector<std::uint8_t>(1250)};
    auto original = module->create(wide, 42);
    check(module->restore(original->save())->snapshot().battlefield.width == 50,
          "Original arena dimensions round trip");
    check(srd5::ability_modifier(9) == -1 && srd5::ability_modifier(13) == 1,
          "Signed ability rounding");
    check(!srd5::attack_hits(1, 100, 1) && srd5::attack_hits(20, -100, 40),
          "Natural attack extremes");
    check(srd5::attack_hits(12, 3, 15) && !srd5::attack_hits(11, 3, 15),
          "Attack meets ascending AC");
    auto session = hero_first(*module);
    const auto before = session->save();
    const auto preview = session->movement_reach(1);
    const auto legal = session->legal_commands();
    check(std::count_if(legal.begin(), legal.end(),
                        [](const auto &c)
                        {
                            return c.verb == "move";
                        }) == preview.size(),
          "Active movement preview matches legal move count");
    for (const auto &cell : preview)
        check(std::any_of(legal.begin(), legal.end(),
                          [&](const auto &c)
                          {
                              return c.verb == "move" && c.destination == cell;
                          }),
              "Active movement preview uses legal destinations");
    check(!session->movement_reach(2).empty() && session->movement_reach(999).empty(),
          "Off-turn combatants have rules-owned movement previews");
    auto invalid = command(*session, "melee");
    invalid.actor = 999;
    check(!session->submit(invalid) && session->save() == before,
          "Invalid actor cannot change state or spend randomness");
    invalid = command(*session, "move", {1, 2});
    invalid.destination = {4, 4};
    check(!session->submit(invalid) && session->save() == before,
          "Invalid path cannot change state");
    auto attack = command(*session, "melee");
    check(session->submit(attack), "Melee command");
    check(session->snapshot().outcome != Outcome::ongoing || session->snapshot().actor == 1,
          "Melee attack preserves the actor's remaining turn");
    const auto after = session->save();
    check(!session->submit(attack) && session->save() == after,
          "Duplicate command rejected atomically");
    for (const auto &c : session->legal_commands())
        check(c.verb != "melee" && c.verb != "ranged", "Attack consumes the action");
    auto restored = module->restore(after);
    check(restored->save() == after, "Checkpoint preserves exact module state");
    auto mismatch = after;
    const auto version = session->snapshot().identity.version;
    const auto where = mismatch.find(version);
    mismatch.replace(where, version.size(), "9.9.9");
    rejects(
        [&]
        {
            (void)module->restore(mismatch);
        },
        "Wrong rules version rejected");
    rejects(
        [&]
        {
            (void)module->restore(after + "junk");
        },
        "Trailing checkpoint data rejected");
    rejects(
        [&]
        {
            (void)module->restore(after.substr(0, after.size() / 2));
        },
        "Truncated checkpoint rejected");
    auto wrong = duel();
    wrong.participants[0].definition = "unsupported dragon";
    rejects(
        [&]
        {
            (void)module->create(wrong, 1);
        },
        "Unsupported content fails explicitly");
    wrong = duel();
    wrong.participants[1].cell = wrong.participants[0].cell;
    rejects(
        [&]
        {
            (void)module->create(wrong, 1);
        },
        "Overlapping starting participants rejected");

    session = hero_first(*module);
    check(session->submit(command(*session, "move", {1, 2})), "Move away from enemy");
    check(session->snapshot().reaction_pending && session->snapshot().actor == 2,
          "Leaving reach offers a reaction before movement");
    const auto reaction = session->save();
    restored = module->restore(reaction);
    check(restored->save() == reaction, "Pending reaction survives save/load");
    const auto react = command(*session, "opportunity");
    check(session->submit(react) && restored->submit(react), "Reaction resolves");
    check(session->save() == restored->save(), "Restored reaction rolls replay deterministically");
    const auto snap = session->snapshot();
    const auto hero = std::find_if(snap.combatants.begin(), snap.combatants.end(),
                                   [](const auto &a)
                                   {
                                       return a.id == 1;
                                   });
    check(hero->cell == Cell{1, 2} && hero->movement_feet == 20,
          "Difficult terrain costs ten feet");
    session = hero_first(*module);
    session->submit(command(*session, "disengage"));
    session->submit(command(*session, "move", {1, 2}));
    check(!session->snapshot().reaction_pending, "Disengage prevents opportunity attacks");

    auto vulnerable = duel();
    vulnerable.participants[0].state = VitalState{1, false, "SRD1 1 0 0 0 0"};
    vulnerable.participants.push_back({3, "vanguard", "Second reactor", 1, {3, 1}});
    vulnerable.participants.push_back({4, "vanguard", "Conscious ally", 0, {8, 6}});
    bool interrupted = false;
    for (unsigned seed = 0; seed < 100 && !interrupted; ++seed)
    {
        auto candidate = module->create(vulnerable, seed);
        if (candidate->snapshot().actor != 1)
            continue;
        candidate->submit(command(*candidate, "move", {1, 2}));
        candidate->submit(command(*candidate, "opportunity"));
        if (unit(*candidate, 1).hit_points > 0)
            continue;
        check(
            !candidate->snapshot().reaction_pending && unit(*candidate, 1).cell == Cell{2, 2} &&
                candidate->snapshot().actor != 1,
            "An opportunity attack that incapacitates the mover cancels the step and remaining reactions");
        check(module->restore(candidate->save())->save() == candidate->save(),
              "Interrupted movement leaves a valid deterministic checkpoint");
        interrupted = true;
    }
    check(interrupted, "Exercise a movement interruption with another queued reactor");

    auto flank = duel();
    flank.participants[1].cell = {1, 2};
    flank.participants.push_back({3, "bandit", "Right Guard", 1, {3, 2}});
    session = hero_first(*module, flank);
    const auto left_attack = [&](const CombatSession &combat, EntityId target)
    {
        const auto commands = combat.legal_commands();
        const auto found = std::find_if(commands.begin(), commands.end(),
                                        [&](const auto &c)
                                        {
                                            return c.verb == "melee" && c.target == target;
                                        });
        check(found != commands.end(), "Expected left-side melee target");
        return *found;
    };
    check(!unit(*session, 1).facing_left, "Combatants initially face right");
    check(session->submit(left_attack(*session, 2)), "Hero attacks to the left");
    check(unit(*session, 1).facing_left && !session->snapshot().reaction_pending &&
              session->snapshot().actor == 1,
          "Turning left toward an attack target does not provoke a reaction");
    const auto turning_checkpoint = session->save();
    restored = module->restore(turning_checkpoint);
    check(restored->save() == turning_checkpoint && unit(*restored, 1).facing_left,
          "Facing remains saved presentation state");
    check(unit(*session, 3).reaction && !offers(*session, "opportunity"),
          "Turning does not spend an adjacent enemy's reaction");
    Command forbidden{session->snapshot().revision, 3, 1, "opportunity", "Opportunity attack", {}};
    check(!session->submit(forbidden) && session->save() == turning_checkpoint,
          "Unprovoked reaction rejects atomically");

    auto reverse = duel();
    reverse.participants[0].facing_left = true;
    reverse.participants.push_back({3, "bandit", "Left Guard", 1, {1, 2}});
    session = hero_first(*module, reverse);
    check(session->submit(left_attack(*session, 2)),
          "Hero attacks to the right from a left-facing pose");
    check(!unit(*session, 1).facing_left && !session->snapshot().reaction_pending &&
              session->snapshot().actor == 1,
          "Turning right toward an attack target does not provoke a reaction");

    auto monster_flank = duel();
    monster_flank.participants[0].cell = {1, 2};
    monster_flank.participants[1].cell = {2, 2};
    monster_flank.participants.push_back({3, "vanguard", "Right Hero", 0, {3, 2}});
    session = actor_first(*module, monster_flank, 2);
    check(session->submit(left_attack(*session, 1)), "Monster attacks to the left");
    check(unit(*session, 2).facing_left && !session->snapshot().reaction_pending &&
              session->snapshot().actor == 2,
          "A monster's change of facing does not provoke a party reaction");

    auto a = module->create(duel(), 77), b = module->create(duel(), 77);
    for (unsigned turns = 0; a->snapshot().outcome == Outcome::ongoing; ++turns)
    {
        check(turns < 200, "Duel finishes within bounded commands");
        const auto next = choose_demo_command(*a);
        check(a->submit(next) && b->submit(next), "Same commands accepted");
        check(a->save() == b->save(), "Identical seeds and commands yield identical combat");
    }
    check(a->legal_commands().empty(), "Completed combat cannot accept extra actions");
    // Sessions retain immutable content independently of the module lifetime.
    module.reset();
    check(!session->snapshot().combatants.empty(),
          "Session owns shared immutable content lifetime");
}
void mechanics_tests()
{
    auto legacy_rules = srd5::load(pack());
    check(legacy_rules->accepts_campaign_identity(
              {"opengold.srd5", "0.5.0", "srd-5.2.1-demo.1/15286736505479635800"}),
          "Saved campaigns from the previous Kobold rules remain loadable");
    auto kobolds = duel();
    kobolds.participants[1].definition = "slums-kobold";
    auto leader = kobolds;
    leader.participants[1].definition = "slums-kobold-leader";
    auto kobold_rules = srd5::load(pack());
    bool checked_kobold = false, checked_leader = false;
    for (unsigned seed = 0; seed < 100 && (!checked_kobold || !checked_leader); ++seed)
    {
        if (!checked_kobold)
        {
            auto fight = kobold_rules->create(kobolds, seed);
            if (fight->snapshot().actor == 2)
            {
                check(offers(*fight, "melee") && !offers(*fight, "ranged"),
                      "Dagger Kobold has no ranged attack");
                check(unit(*fight, 2).type_name == "Kobold" &&
                          unit(*fight, 2).melee_weapon == "Dagger" &&
                          !unit(*fight, 2).ranged_attack_available,
                      "Kobold hover data follows its melee-only rules");
                check(command(*fight, "melee").label == "Dagger attack",
                      "Kobold attack names its visible weapon");
                checked_kobold = true;
            }
        }
        if (!checked_leader)
        {
            auto fight = kobold_rules->create(leader, seed);
            if (fight->snapshot().actor == 2)
            {
                check(offers(*fight, "melee") && offers(*fight, "ranged"),
                      "Kobold leader retains bow attack");
                check(unit(*fight, 2).type_name == "Kobold Leader" &&
                          unit(*fight, 2).ranged_weapon == "Short bow" &&
                          unit(*fight, 2).ranged_attack_available,
                      "Leader hover data follows its short bow rules");
                check(command(*fight, "ranged").label == "Short bow attack",
                      "Leader ranged attack names its bow");
                checked_leader = true;
            }
        }
    }
    check(checked_kobold && checked_leader, "Exercised both Kobold attack profiles");
    auto sword_leader = leader;
    sword_leader.participants[1].definition = "slums-kobold-leader-sword";
    bool checked_sword_leader = false;
    for (unsigned seed = 0; seed < 100 && !checked_sword_leader; ++seed)
    {
        auto fight = kobold_rules->create(sword_leader, seed);
        if (fight->snapshot().actor != 2)
            continue;
        check(offers(*fight, "melee") && !offers(*fight, "ranged") &&
                  command(*fight, "melee").label == "Short sword attack",
              "Original record 11 leader cannot fire a bow it does not carry");
        checked_sword_leader = true;
    }
    check(checked_sword_leader, "Exercised sword-only Kobold leader");
    kobolds.participants[1].cell = {8, 2};
    leader.participants[1].cell = {8, 2};
    checked_kobold = checked_leader = false;
    for (unsigned seed = 0; seed < 100 && (!checked_kobold || !checked_leader); ++seed)
    {
        if (!checked_kobold)
        {
            auto fight = kobold_rules->create(kobolds, seed);
            if (fight->snapshot().actor == 2)
            {
                check(!offers(*fight, "ranged"),
                      "Distant ordinary Kobold cannot attack without a bow");
                checked_kobold = true;
            }
        }
        if (!checked_leader)
        {
            auto fight = kobold_rules->create(leader, seed);
            if (fight->snapshot().actor == 2)
            {
                check(!offers(*fight, "melee") && fight->submit(command(*fight, "ranged")),
                      "Distant leader can fire its short bow");
                checked_leader = true;
            }
        }
    }
    check(checked_kobold && checked_leader, "Exercised distant Kobold attacks");
    CombatDemo training(srd5::load(pack()));
    training.training();
    for (unsigned i = 0; training.combat().snapshot().outcome == Outcome::ongoing; ++i)
    {
        check(i < 200, "Training encounter finishes");
        check(training.submit(choose_demo_command(training.combat())),
              "Training AI command accepted");
    }
    auto module = srd5::load(pack());
    auto encounter = duel("adept");
    encounter.participants[1] = {2, "vanguard", "Enemy", 1, {8, 2}};
    encounter.participants.push_back({3, "vanguard", "Reserve", 1, {8, 6}});
    auto session = hero_first(*module, encounter);
    for (int cast = 0; cast < 2; ++cast)
    {
        check(session->submit(command(*session, "magic_missile")), "Spell command accepted");
        check(session->snapshot().outcome != Outcome::ongoing || session->snapshot().actor == 1,
              "Damaging spell preserves remaining turn resources");
        check(!unit(*session, 1).action && unit(*session, 1).movement_feet == 30,
              "Spell consumes action, not movement");
        next_round(*session);
    }
    check(!offers(*session, "magic_missile") && offers(*session, "fire_bolt"),
          "Exhausted slots prevent leveled spells but allow cantrips");
    auto restored = module->restore(session->save());
    check(restored->save() == session->save(), "Spent spell slots persist");

    encounter = duel("adept");
    encounter.participants[1].cell = {6, 2};
    encounter.battlefield.terrain[2 * 12 + 4] = 1;
    session = hero_first(*module, encounter);
    check(!offers(*session, "ranged") && !offers(*session, "fire_bolt") &&
              !offers(*session, "magic_missile"),
          "Opaque obstacles block weapon and spell targeting");
    encounter.battlefield.terrain[2 * 12 + 4] = 0;
    session = hero_first(*module, encounter);
    check(offers(*session, "ranged") && offers(*session, "magic_missile"),
          "Clear sight enables targeting");
    encounter = duel();
    session = hero_first(*module, encounter);
    session->submit(command(*session, "ranged"));
    const auto ranged_log = session->snapshot().log;
    check(std::any_of(ranged_log.begin(), ranged_log.end(),
                      [](const auto &line)
                      {
                          return line.find("disadvantage") != std::string::npos;
                      }),
          "Adjacent hostile imposes ranged disadvantage");

    // A critical can kill the adjacent bandit; its vacated cell is traversable
    // and a checkpoint may legitimately contain a living actor over a corpse.
    encounter.participants.push_back({3, "bandit", "Reserve", 1, {10, 7}});
    bool tested = false;
    for (unsigned seed = 0; seed < 500 && !tested; ++seed)
    {
        session = module->create(encounter, seed);
        if (session->snapshot().actor != 1)
            continue;
        session->submit(command(*session, "melee"));
        if (!unit(*session, 2).dead)
            continue;
        check(session->snapshot().actor == 1,
              "Lethal attack preserves the actor's turn while other enemies remain");
        const auto log = session->snapshot().log;
        if (std::none_of(log.begin(), log.end(),
                         [](const auto &line)
                         {
                             return line.find("CRITICAL") != std::string::npos;
                         }))
            continue;
        check(session->submit(command(*session, "move", {3, 2})),
              "Can move onto defeated enemy cell");
        restored = module->restore(session->save());
        check(restored->save() == session->save(), "Corpse overlap survives checkpoint restore");
        tested = true;
    }
    check(tested, "Critical damage can exceed normal attack maximum");

    for (const auto profile : {"healer", "vanguard"})
    {
        encounter = duel(profile);
        encounter.participants[1] = {2, "adept", "Enemy caster", 1, {8, 2}};
        session = hero_first(*module, encounter);
        session->submit(command(*session, "end"));
        check(session->snapshot().actor == 2, "Enemy caster turn");
        session->submit(command(*session, "magic_missile"));
        const auto before = unit(*session, 1);
        check(before.hit_points < before.max_hit_points, "Damage before healing");
        check(session->submit(command(*session, "end")),
              "Enemy caster explicitly finishes its turn");
        const bool cleric = std::string_view(profile) == "healer";
        session->submit(command(*session, cleric ? "cure_wounds" : "second_wind"));
        const auto healed = unit(*session, 1);
        check(healed.hit_points > before.hit_points && healed.hit_points <= healed.max_hit_points,
              "Healing restores HP up to maximum");
        check(cleric ? !healed.action : (!healed.bonus_action && healed.action),
              "Healing spends the correct action budget");
    }

    encounter = duel("bandit");
    encounter.participants[1] = {2, "adept", "Enemy caster", 1, {8, 2}};
    encounter.participants.push_back({3, "vanguard", "Ally", 0, {1, 5}});
    tested = false;
    for (unsigned seed = 0; seed < 100 && !tested; ++seed)
    {
        session = module->create(encounter, seed);
        if (session->snapshot().actor != 2)
            continue;
        auto missile = command(*session, "magic_missile");
        missile.target = 1;
        check(session->submit(missile), "Target player with spell");
        if (unit(*session, 1).hit_points > 0)
            continue;
        check(!unit(*session, 1).dead && session->snapshot().outcome == Outcome::ongoing,
              "Zero HP incapacitates player while ally can fight");
        for (int i = 0; i < 3; ++i)
            session->submit(command(*session, "end"));
        const auto log = session->snapshot().log;
        check(std::any_of(log.begin(), log.end(),
                          [](const auto &line)
                          {
                              return line.find("death save") != std::string::npos;
                          }),
              "Unconscious player makes death saves on its turn");
        tested = true;
    }
    check(tested, "Exercised player unconscious/death-save flow");

    encounter = duel("bandit");
    encounter.participants[1] = {2, "adept", "Enemy caster", 1, {8, 2}};
    tested = false;
    for (unsigned seed = 0; seed < 500 && !tested; ++seed)
    {
        session = module->create(encounter, seed);
        if (session->snapshot().actor != 2)
            continue;
        auto missile = command(*session, "magic_missile");
        missile.target = 1;
        session->submit(missile);
        if (unit(*session, 1).hit_points > 0)
            continue;
        check(!unit(*session, 1).dead && session->snapshot().outcome == Outcome::defeat,
              "Combat ends when the last party member is unconscious, without declaring them dead");
        tested = true;
    }
    check(tested, "Exercised all-unconscious party defeat flow");
}
void death_save_turn_entry_tests()
{
    auto module = srd5::load(pack());
    auto encounter = duel();
    encounter.participants.push_back({3, "vanguard", "Ally", 0, {1, 5}});
    const auto death_rolls = [](const CombatSession &session)
    {
        std::vector<int> rolls;
        for (const auto &line : session.snapshot().log)
            if (line.starts_with("Hero death save: "))
                rolls.push_back(std::stoi(line.substr(17)));
        return rolls;
    };
    // Exercise all four outcomes both in the first initiative slot and later.
    for (const bool first : {true, false})
    {
        bool natural_one = false, natural_twenty = false, success = false, failure = false;
        for (unsigned seed = 0;
             seed < 1000 && !(natural_one && natural_twenty && success && failure); ++seed)
        {
            const auto healthy = module->create(encounter, seed);
            if ((healthy->snapshot().actor == 1) != first)
                continue;
            auto wounded = encounter;
            wounded.participants[0].state = VitalState{0, false, "SRD1 1 0 2 1 0"};
            auto combat = module->create(wounded, seed);
            if (!first)
            {
                check(death_rolls(*combat).empty(),
                      "Death saves wait for the wounded actor's initiative slot");
                for (unsigned turns = 0; turns < 3 && death_rolls(*combat).empty(); ++turns)
                    check(combat->submit(command(*combat, "end")),
                          "Advance to the wounded actor's turn");
            }
            const auto rolls = death_rolls(*combat);
            check(
                rolls.size() == 1,
                "Every unstable turn entry makes exactly one death save, including the first slot");
            const auto hero = unit(*combat, 1);
            const int roll = rolls.front();
            if (roll == 20)
            {
                natural_twenty = true;
                check(
                    hero.hit_points == 1 && !hero.dead && hero.prone &&
                        hero.persistent.resources == "SRD3 1 0 0 0 0 0 FX4 1 0 0 1",
                    "Natural 20 restores 1 HP and clears both counters without restoring spent resources");
                check(combat->snapshot().actor == 1 && hero.action && hero.bonus_action &&
                          hero.reaction,
                      "Natural-20 recovery permits the current turn");
                if (first)
                    check(combat->snapshot().elapsed_milliseconds == 0,
                          "Natural-20 recovery does not skip the first initiative slot");
            }
            else if (roll >= 10)
            {
                success = true;
                check(hero.hit_points == 0 && !hero.dead &&
                          hero.persistent.resources.starts_with("SRD5 1 0 0 0 0 1 0 "),
                      "Third success stabilizes and resets successes and failures");
                check(combat->snapshot().actor != 1, "Stable unconscious actors cannot act");
            }
            else if (roll == 1)
            {
                natural_one = true;
                check(hero.dead && hero.persistent.resources == "SRD1 1 0 2 3 0",
                      "Natural 1 adds two failures and reaches death at three");
            }
            else
            {
                failure = true;
                check(!hero.dead && hero.persistent.resources.starts_with("SRD5 1 0 0 2 2 0 0 "),
                      "Ordinary failure adds one and retains prior successes");
            }
            const auto saved = combat->save();
            auto restored = module->restore(saved);
            check(restored->save() == saved && death_rolls(*restored) == rolls,
                  "Checkpoint restore does not replay a turn-entry save");
            for (unsigned turns = 0; turns < 3; ++turns)
            {
                const auto end = command(*combat, "end");
                check(combat->submit(end) && restored->submit(end) &&
                          combat->save() == restored->save(),
                      "Death-save continuation retains exact RNG and state after restore");
            }
        }
        check(natural_one && natural_twenty && success && failure,
              "Exercise all death-save outcomes at each turn-entry position");
    }
    // Find the same first-slot seed without relying on the private dice stream.
    unsigned first_seed = 0;
    while (module->create(encounter, first_seed)->snapshot().actor != 1)
    {
        check(++first_seed < 100, "Find first-slot seed");
    }
    for (const bool dead : {false, true})
    {
        auto skipped = encounter;
        skipped.participants[0].state =
            VitalState{0, dead, dead ? "SRD1 1 0 0 3 0" : "SRD1 1 0 0 0 1"};
        auto combat = module->create(skipped, first_seed);
        check(combat->snapshot().actor != 1 && death_rolls(*combat).empty(),
              "Stable and dead initial actors do not make death saves");
        auto restored = module->restore(combat->save());
        check(restored->save() == combat->save(),
              "Skipped initial actor preserves exact checkpoint state");
    }
    auto legacy = encounter;
    legacy.participants[0].state = VitalState{0, false, "SRD1 1 0 3 2 1"};
    auto migrated = module->create(legacy, first_seed);
    check(death_rolls(*migrated).empty() &&
              unit(*migrated, 1).persistent.resources.starts_with("SRD5 1 0 0 0 0 1 0 "),
          "Older stable resource state is normalized without rolling or refilling resources");
}
void allied_transit_tests()
{
    auto module = srd5::load(pack());
    Battlefield corridor{7, 3, std::vector<std::uint8_t>(21, 1)};
    for (int x = 0; x < 7; ++x)
        corridor.terrain[7 + x] = 0;
    for (const unsigned side : {0u, 1u})
        for (const bool difficult : {false, true})
        {
            Encounter e{corridor,
                        {{1, "vanguard", "Mover", side, {0, 1}},
                         {2, "bandit", "Enemy", 1 - side, {6, 1}},
                         {3, "vanguard", "Ally", side, {1, 1}},
                         {4, "vanguard", "Second ally", side, {2, 1}}}};
            if (difficult)
                e.battlefield.terrain[8] = 2;
            auto session = hero_first(*module, e);
            const auto saved = session->save();
            const auto moves = session->movement_reach(1);
            check(std::find(moves.begin(), moves.end(), Cell{4, 1}) != moves.end(),
                  "Movement highlights include free cells beyond successive allies");
            for (const Cell occupied : std::array<Cell, 3>{{{1, 1}, {2, 1}, {6, 1}}})
            {
                Command blocked{session->snapshot().revision, 1, 0, "move", "Move", occupied};
                check(!session->submit(blocked) && session->save() == saved,
                      "Voluntary stops on allies or enemies reject atomically");
            }
            check(session->submit(command(*session, "move", {4, 1})) &&
                      unit(*session, 1).cell == Cell{4, 1},
                  "Both sides can cross their own allies");
            check(unit(*session, 1).movement_feet == (difficult ? 5 : 10),
                  "Allied occupancy adds no cost; terrain retains its surcharge");
            check(unit(*session, 1).action && unit(*session, 1).bonus_action,
                  "Transit spends movement, not an action or Bonus Action");
            check(module->restore(session->save())->save() == session->save(),
                  "Completed allied transit round trips");
        }
    // Pause on an allied space after a travelled prefix, before leaving reach.
    Battlefield board{5, 3, std::vector<std::uint8_t>(15, 1)};
    for (int x = 0; x < 5; ++x)
        board.terrain[5 + x] = 0;
    board.terrain[1] = board.terrain[2] = 0;
    Encounter e{board,
                {{1, "vanguard", "Mover", 0, {0, 1}},
                 {2, "bandit", "Reactor", 1, {1, 0}},
                 {3, "healer", "Ally", 0, {2, 1}}}};
    for (const auto response : {"decline", "opportunity"})
    {
        auto session = hero_first(*module, e);
        session->submit(command(*session, "move", {4, 1}));
        check(session->snapshot().reaction_pending &&
                  unit(*session, 1).cell == unit(*session, 3).cell &&
                  unit(*session, 1).movement_feet == 20,
              "The reaction pauses on an allied transit cell after spending the prefix once");
        std::vector<std::string> rows;
        std::istringstream saved(session->save());
        for (std::string line; std::getline(saved, line);)
            rows.push_back(line);
        const auto path_header = 4 + session->snapshot().combatants.size();
        const auto reject_rows = [&](const auto &invalid)
        {
            std::string bytes;
            for (const auto &row : invalid)
                bytes += row + '\n';
            rejects(
                [&]
                {
                    (void)module->restore(bytes);
                },
                "Malformed allied transit checkpoint accepted");
        };
        auto invalid = rows;
        invalid[path_header + 1] = "1 1 2 1 3 1 2 1 ";
        reject_rows(invalid); // A valid sequence of steps may not end on the ally.
        invalid = rows;
        invalid[path_header] = "0 0";
        invalid[path_header + 1] = "";
        invalid[path_header + 2] = "0 0";
        invalid[path_header + 3] = "";
        reject_rows(invalid); // Unpaused, voluntary living overlap remains invalid.
        auto restored = module->restore(session->save());
        const auto react = command(*session, response);
        check(session->submit(react) && restored->submit(react) &&
                  session->save() == restored->save(),
              "Shared-cell reaction resumes deterministically after reload");
        check(unit(*session, 1).cell == Cell{4, 1} && unit(*session, 1).movement_feet == 10,
              "Resumed route spends only its remaining suffix");
    }
    // Knockdown during transit is involuntary: persist the shared position
    // through healing, and clear the exceptional state when either ally leaves.
    e.participants[0].state = VitalState{1, false, "SRD1 1 0 0 0 0"};
    bool recovered = false, died = false, natural_recovery = false;
    for (unsigned seed = 0; seed < 500 && !(recovered && died && natural_recovery); ++seed)
    {
        auto session = module->create(e, seed);
        if (session->snapshot().actor != 1)
            continue;
        session->submit(command(*session, "move", {4, 1}));
        session->submit(command(*session, "opportunity"));
        if (unit(*session, 1).hit_points > 0)
            continue;
        check(!session->snapshot().reaction_pending && unit(*session, 1).cell == Cell{2, 1},
              "Incapacitated mover remains where the interrupt hit, on the ally");
        auto waiting = module->restore(session->save());
        for (unsigned turns = 0;
             turns < 20 && !unit(*waiting, 1).dead && unit(*waiting, 1).hit_points == 0; ++turns)
        {
            check(waiting->submit(command(*waiting, "end")),
                  "Advance automatic recovery while sharing an allied space");
            check(
                module->restore(waiting->save())->save() == waiting->save(),
                "Death saves, stabilization and recovery preserve a valid shared-space checkpoint");
        }
        died |= unit(*waiting, 1).dead;
        natural_recovery |= unit(*waiting, 1).hit_points > 0;
        if (recovered)
            continue;
        auto restored = module->restore(session->save());
        for (unsigned turns = 0; session->snapshot().actor != 3; ++turns)
        {
            check(turns < 3, "Ally gets a healing turn");
            const auto end = command(*session, "end");
            check(session->submit(end) && restored->submit(end), "Advance both copies to healer");
        }
        auto heal = command(*session, "cure_wounds");
        heal.target = 1;
        check(session->submit(heal) && restored->submit(heal) &&
                  session->save() == restored->save(),
              "Healing an interrupted ally preserves deterministic overlapping state");
        check(unit(*session, 1).hit_points > 0 && unit(*session, 1).cell == unit(*session, 3).cell,
              "Revived ally has not been silently teleported");
        restored = module->restore(session->save());
        const auto move = command(*session, "move", {3, 1});
        check(session->submit(move) && restored->submit(move) &&
                  session->save() == restored->save(),
              "Either ally can leave an involuntarily shared cell");
        check(unit(*session, 1).cell != unit(*session, 3).cell,
              "Recovery resolves overlap without duplicating a movement budget");
        check(module->restore(session->save())->save() == session->save(),
              "Separated actors retain a valid checkpoint");
        recovered = true;
    }
    check(recovered && died && natural_recovery,
          "Exercise healing, death and natural-20 recovery after interruption on an ally");
}
void opportunity_migration_tests()
{
    auto module = srd5::load(pack());
    const auto fixture = [](const char *name)
    {
        std::ifstream file(std::filesystem::path(OPENGOLD_SOURCE_DIR) / "tests/fixtures" / name);
        check(bool(file), "Frozen previous-module combat fixture exists");
        return std::string(std::istreambuf_iterator<char>(file), {});
    };
    const auto lines = [](const std::string &bytes)
    {
        std::vector<std::string> result;
        std::istringstream input(bytes);
        for (std::string line; std::getline(input, line);)
            result.push_back(line);
        return result;
    };
    const auto encode = [](const std::vector<std::string> &rows)
    {
        std::string bytes;
        for (const auto &row : rows)
            bytes += row + '\n';
        return bytes;
    };
    const auto upgraded = [&](const std::string &bytes)
    {
        auto rows = lines(bytes);
        rows[0].replace(9, 1, "13");
        for (std::size_t i = 4; i < 8; ++i)
            rows[i] += " 0 0 0 0 0 0 \"\" 0 0";
        rows[0].replace(rows[0].find("0.6.4"), 5, module->identity().version);
        const std::string old_content = "srd-5.2.1-demo.1/15052881321234871607";
        rows[0].replace(rows[0].find(old_content), old_content.size(), module->identity().content);
        rows.back() = "0";
        rows.push_back("0");
        return rows;
    };
    const auto facing = fixture("combat-v5-facing.save");
    auto session = module->restore(facing);
    auto expected = upgraded(facing);
    // The frozen old writer recorded ticket 4, two facing reactors and no path.
    expected[3] = "6018027440424182934 5 0 1 0 4";
    expected[10] = "0 0";
    expected[11] = "";
    check(session->save() == encode(expected),
          "Facing migration changes only identity/format, obsolete queue and command ticket");
    const auto hero = unit(*session, 1);
    check(
        session->snapshot().actor == 1 && !session->snapshot().reaction_pending &&
            hero.facing_left && hero.cell == Cell{2, 3} && hero.hit_points == 20 && !hero.action &&
            !hero.bonus_action && hero.movement_feet == 25 &&
            hero.persistent.resources == "SRD1 0 0 0 0 0",
        "Obsolete reaction cancellation preserves wounds, spent action/recovery and remaining movement");
    auto restored = module->restore(session->save());
    const auto move = command(*session, "move", {1, 2});
    check(session->submit(move) && restored->submit(move) && session->save() == restored->save(),
          "Migrated facing turn can begin a genuine leave-reach reaction deterministically");
    while (session->snapshot().reaction_pending)
    {
        const auto decline = command(*session, "decline");
        check(session->submit(decline) && restored->submit(decline) &&
                  session->save() == restored->save(),
              "Migrated movement resumes identically after each declined reaction");
    }
    check(unit(*session, 1).cell == Cell{1, 2} && !unit(*session, 1).action,
          "Cancellation neither ends the turn nor refunds the action");
    auto old_end = lines(facing);
    old_end.back() = "2";
    check(module->restore(encode(old_end))->save() == encode(expected),
          "Legacy end-after-facing state also resumes the remaining turn");
    const auto movement = fixture("combat-v5-movement.save");
    session = module->restore(movement);
    check(session->save() == encode(upgraded(movement)),
          "Movement migration retains every queue position, resource, ticket, RNG and clock value");
    check(session->snapshot().actor == 4 && session->snapshot().reaction_pending &&
              unit(*session, 1).cell == Cell{2, 2},
          "Already-declined reactor stays declined and movement still waits before leaving reach");
    for (const auto reactor : {4u, 2u})
    {
        check(session->snapshot().actor == reactor,
              "Pending movement reactors preserve initiative order");
        restored = module->restore(session->save());
        const auto attack = command(*session, "opportunity");
        check(session->submit(attack) && restored->submit(attack) &&
                  session->save() == restored->save(),
              "Each migrated opportunity attack retains deterministic rolls and continuation");
    }
    check(session->save() == encode(upgraded(fixture("combat-v5-movement-resolved.save"))),
          "Resolved movement matches the frozen old writer's RNG, log, HP and movement");
    check(unit(*session, 3).reaction && !unit(*session, 4).reaction && !unit(*session, 2).reaction,
          "Declining keeps a reaction; attacking spends exactly one");
    session->submit(command(*session, "move", {2, 2}));
    session->submit(command(*session, "move", {1, 2}));
    check(session->snapshot().actor == 3 && session->snapshot().reaction_pending,
          "Only the previously declining enemy can react to a second departure");
    session->submit(command(*session, "decline"));
    check(!session->snapshot().reaction_pending, "Spent reactions cannot be reused");
    for (const auto &bytes : {facing, movement})
    {
        for (const auto version : {"0.6.3", "0.7.0"})
        {
            auto wrong = bytes;
            wrong.replace(wrong.find("0.6.4"), 5, version);
            rejects(
                [&]
                {
                    (void)module->restore(wrong);
                },
                "Only the explicitly supported old module may migrate combat");
        }
        auto wrong = bytes;
        wrong.replace(wrong.find("15052881321234871607"), 20, "00000000000000000000");
        rejects(
            [&]
            {
                (void)module->restore(wrong);
            },
            "Combat migration rejects unrelated content");
    }
    for (const auto state : {"3", "0"})
    {
        auto malformed = lines(facing);
        malformed.back() = state;
        rejects(
            [&]
            {
                (void)module->restore(encode(malformed));
            },
            "Malformed legacy facing state must not be silently discarded");
    }
    auto malformed = lines(facing);
    malformed[11] = "1 4";
    rejects(
        [&]
        {
            (void)module->restore(encode(malformed));
        },
        "Invalid legacy reactor is validated before queue cancellation");
    // The application wrapper must replace only a fully validated candidate.
    CombatDemo demo(srd5::load(pack()));
    demo.training();
    demo.restore_combat(facing);
    const auto valid = demo.save_combat();
    rejects(
        [&]
        {
            demo.restore_combat(encode(malformed));
        },
        "Invalid training restore rejects");
    check(demo.save_combat() == valid, "Failed migrated restore preserves the live combat");
}
void checkpoint_validation_tests()
{
    auto module = srd5::load(pack());
    auto session = hero_first(*module);
    session->submit(command(*session, "move", {1, 2}));
    const auto checkpoint = session->save();
    std::vector<std::string> lines;
    std::istringstream input(checkpoint);
    for (std::string line; std::getline(input, line);)
        lines.push_back(line);
    const auto path_header = 4 + session->snapshot().combatants.size();
    const auto encode = [](const std::vector<std::string> &fields)
    {
        std::string result;
        for (const auto &line : fields)
            result += line + '\n';
        return result;
    };
    const auto reject_changes =
        [&](std::initializer_list<std::pair<std::size_t, std::string>> changes)
    {
        auto corrupt = lines;
        for (const auto &[line, text] : changes)
            corrupt.at(line) = text;
        rejects(
            [&]
            {
                (void)module->restore(encode(corrupt));
            },
            "Malformed checkpoint accepted");
        check(session->save() == checkpoint, "Rejected restore altered the live session");
    };
    reject_changes({{path_header, "1025 0"}});
    reject_changes({{path_header, "1 2"}});
    reject_changes({{path_header, "0 0"}, {path_header + 1, ""}}); // Reaction with no next step.
    reject_changes({{path_header + 1, "99 99"}});
    reject_changes({{path_header + 1, "0 8"}}); // Nonadjacent step on an otherwise open cell.
    reject_changes({{path_header + 1, "3 2"}}); // Path enters the enemy's occupied cell.
    reject_changes({{path_header + 2, "0 0"}, {path_header + 3, ""}}); // Path without a pause.
    reject_changes({{path_header + 2, "1 2"}});
    reject_changes({{path_header + 2, "2 0"}, {path_header + 3, "2 2"}}); // Duplicate reactor.
    reject_changes({{path_header + 3, "999"}});
    reject_changes({{path_header + 3, "1"}}); // Mover cannot react against itself.
    reject_changes({{path_header + 4, "81"}});
    // Fuzz regression: extra movement requires an already spent action. An
    // accepted 31-foot budget plus an unused Dash used to produce 61 feet,
    // which the next restore rejected. Preserve the genuine 60-foot boundary.
    auto actor_line = lines[4];
    std::istringstream actor_input(actor_line);
    EntityId id;
    std::string definition, name;
    int side, x, y, hp, initiative, movement;
    actor_input >> id >> std::quoted(definition) >> std::quoted(name) >> side >> x >> y >> hp >>
        initiative >> std::ws;
    const auto movement_start = static_cast<std::size_t>(actor_input.tellg());
    actor_input >> movement;
    const auto movement_end = static_cast<std::size_t>(actor_input.tellg());
    check(movement == 30, "Regression fixture has its original movement allowance");
    for (const auto extra : {31, 60})
    {
        auto corrupt_actor = actor_line;
        corrupt_actor.replace(movement_start, movement_end - movement_start, std::to_string(extra));
        reject_changes({{4, corrupt_actor}});
    }
    auto dashed = hero_first(*module);
    check(dashed->submit(command(*dashed, "dash")),
          "Dash is accepted from the normal movement boundary");
    check(unit(*dashed, 1).movement_feet == 60 && !offers(*dashed, "dash"),
          "Dash spends the action for extra movement");
    check(module->restore(dashed->save())->save() == dashed->save(),
          "Legitimate doubled movement round trips");
    // Tickets wrap past reserved zero; round numbers saturate. Both boundary
    // states must remain playable and saveable after offered commands execute.
    for (const auto round : {100000u, std::numeric_limits<unsigned>::max()})
    {
        auto boundary = lines;
        std::istringstream counters(boundary[3]);
        std::uint64_t rng;
        counters >> rng;
        boundary[3] = std::to_string(rng) + " " +
                      std::to_string(std::numeric_limits<std::uint64_t>::max()) + " 0 " +
                      std::to_string(round) + " 0 2";
        auto continued = module->restore(encode(boundary));
        const auto decline = command(*continued, "decline");
        check(continued->submit(decline) && continued->snapshot().revision == 1,
              "Ticket rollover skips reserved zero");
        check(!continued->submit(decline), "Pre-rollover ticket is stale");
        continued->submit(command(*continued, "end"));
        continued->submit(command(*continued, "end"));
        check(continued->snapshot().round == (round == 100000 ? 100001 : round),
              "Round increments or saturates without wrapping");
        check(module->restore(continued->save())->save() == continued->save(),
              "Counter boundary continuation round trips");
    }
    for (std::size_t count = 0; count < lines.size(); ++count)
    {
        auto truncated = lines;
        truncated.resize(count);
        rejects(
            [&]
            {
                (void)module->restore(encode(truncated));
            },
            "Truncated checkpoint section accepted");
    }

    auto previous = lines;
    previous.pop_back();
    previous.pop_back();
    previous[0].replace(9, 2, "6");
    previous[0].replace(previous[0].find(module->identity().version),
                        module->identity().version.size(), "0.6.5");
    for (std::size_t actor = 4; actor < path_header; ++actor)
        for (unsigned field = 0; field < 9; ++field)
            previous[actor].resize(previous[actor].find_last_of(' '));
    check(module->restore(encode(previous))->save() == checkpoint,
          "Pre-transit 0.6.5 movement checkpoint upgrades without changing its continuation");
    auto invalid_overlap = lines[4];
    auto no_clocks = invalid_overlap;
    for (unsigned n = 0; n < 6; ++n)
        no_clocks.resize(no_clocks.find_last_of(' '));
    const auto grip_separator = no_clocks.rfind(' ', no_clocks.find_last_of(' ') - 1);
    invalid_overlap[grip_separator - 1] = '1';
    reject_changes({{4, invalid_overlap}});

    previous = lines;
    previous.pop_back();
    previous.pop_back();
    previous[0].replace(9, 2, "4");
    for (std::size_t actor = 4; actor < path_header; ++actor)
        for (unsigned field = 0; field < 9; ++field)
            previous[actor].resize(
                previous[actor].find_last_of(' ')); // No Temporary HP, recovery clocks, Hit Dice,
                                                    // grip or overlap marker before v7.
    for (std::size_t actor = 4; actor < path_header; ++actor)
        previous[actor].resize(previous[actor].find_last_of(' ')); // Version 4 has no facing field.
    check(module->restore(encode(previous))->save() == checkpoint,
          "Version 4 checkpoint migrates to facing right");

    // Earlier formats omitted the empty character profile (v1), and the
    // second-level slot/feat flags (v1/v2). Their defaults must still round trip.
    for (const unsigned version : {1u, 2u})
    {
        auto legacy = lines;
        legacy[0].replace(9, 2, std::to_string(version));
        legacy.resize(legacy.size() -
                      5); // v4 scope/clock, two effect collections and later pending choices.
        for (std::size_t actor = 4; actor < path_header; ++actor)
        {
            const auto profile = legacy[actor].find("\"\"");
            check(profile != std::string::npos, "Expected fixture with no character profile");
            legacy[actor].resize(profile + (version == 2 ? 2 : 0));
        }
        check(module->restore(encode(legacy))->save() == checkpoint,
              "Legacy checkpoint defaults changed");
    }

    // A route pauses after spending 10 feet while leaving an enemy's reach.
    // Only its remaining movement may be charged when validating a restore.
    Battlefield corridor{5, 3, std::vector<std::uint8_t>(15, 1)};
    for (int x = 0; x < 5; ++x)
        corridor.terrain[5 + x] = 0;
    corridor.terrain[1] = corridor.terrain[2] = 0; // Clear sight from the reactor.
    Encounter encounter{corridor,
                        {{1, "vanguard", "Mover", 0, {0, 1}},
                         {2, "bandit", "Reactor", 1, {1, 0}},
                         {3, "vanguard", "Ally", 0, {2, 1}}}};
    session = hero_first(*module, encounter);
    check(session->submit(command(*session, "move", {4, 1})), "Clear corridor move accepted");
    check(session->snapshot().reaction_pending && unit(*session, 1).cell == Cell{2, 1},
          "Route pauses when leaving the reactor's reach");
    check(unit(*session, 1).movement_feet == 20, "Prefix spends its movement once");
    auto restored = module->restore(session->save());
    check(restored->save() == session->save(), "Paused movement checkpoint restores exactly");
    const auto decline = command(*session, "decline");
    check(session->submit(decline) && restored->submit(decline),
          "Both routes resume after the reaction");
    check(restored->save() == session->save(),
          "Restored suffix preserves deterministic continuation");
    check(unit(*session, 1).cell == Cell{4, 1} && unit(*session, 1).movement_feet == 10,
          "Only the remaining route suffix spends movement after restore");
}
void installed()
{
    const auto directory = std::getenv("OPENGOLD_GAME_DIR");
    if (!directory)
        return;
    CombatDemo demo(srd5::load(pack()));
    demo.slums(directory, 42);
    for (unsigned i = 0; !demo.has_combat(); ++i)
    {
        check(i < 10 && demo.waiting(), "Original script reaches combat");
        demo.continue_script();
    }
    check(demo.combat().snapshot().combatants.size() == 8,
          "Fixed four-person party and four original orcs");
    for (unsigned i = 0; demo.combat().snapshot().outcome == Outcome::ongoing; ++i)
    {
        check(i < 1000, "Original Slums combat completes");
        check(demo.submit(choose_demo_command(demo.combat())), "Real combat command accepted");
    }
    const auto outcome = demo.combat().snapshot().outcome;
    while (demo.waiting())
        demo.continue_script();
    check(demo.script_complete(), "Original ECL resumes after real combat");
    check(demo.script_variable(0x6DC7) == (outcome == Outcome::victory ? 0 : 128),
          "Actual outcome mapped to original ECL");
    if (outcome == Outcome::victory)
    {
        check(demo.script_variable(0x6DC8) == 4, "Actual defeated count returned");
        check(demo.script_variable(0x4ACA) == 255 && demo.script_variable(0x4ABB) == 1,
              "Victory updates original event state");
        demo.revisit();
        check(demo.script_complete() && !demo.waiting(),
              "Completed event does not replay the fight");
        check(demo.script_variable(0x4ABB) == 1, "Revisit preserves fight count");
    }
    std::cout << "Original Slums event completed with real rules combat: "
              << (outcome == Outcome::victory ? "victory" : "defeat") << ".\n";
}
#include "unconscious_transit_checks.h"
} // namespace
int main(int argc, char **argv)
{
    try
    {
        if (argc == 2 && std::string_view(argv[1]) == "--freeze-unconscious-transit")
        {
            unconscious_transit::freeze();
            return 0;
        }
        unconscious_transit::prior_writer();
        unconscious_transit::run();
        turn_budget_tests();
        boundary_tests();
        mechanics_tests();
        death_save_turn_entry_tests();
        opportunity_migration_tests();
        allied_transit_tests();
        checkpoint_validation_tests();
        installed();
        std::cout << "Rules tests passed.\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
