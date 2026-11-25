// Fights under the original rules (docs/ORIGINAL-RULES.md, plan step 4).
#include "opengold/creature_catalog.h"
#include "opengold/original.h"
#include "opengold/original_combat.h"
#include "opengold/original_content.h"
#include "opengold/original_rules.h"
#include "opengold/original_spells.h"
#include "installed_games.h"
#include "original_creation_support.h"
#include "test_support.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace
{
using namespace opengold;
using opengold::test::check;
using rules::Ability;

constexpr std::string_view kobold_key = "MON2CHA.DAX:0";

rules::CharacterSheet fighter(int strength, int hit_points)
{
    rules::CharacterSheet sheet;
    sheet.identity = original::identity();
    sheet.name = "Fighter " + std::to_string(strength);
    sheet.scores = {};
    sheet.scores[Ability::strength] = strength;
    sheet.scores[Ability::dexterity] = 12;
    sheet.scores[Ability::constitution] = 14;
    sheet.scores[Ability::intelligence] = 10;
    sheet.scores[Ability::wisdom] = 10;
    sheet.scores[Ability::charisma] = 10;
    sheet.hit_points = hit_points;
    rules::CharacterSheet::Original numbers;
    numbers.class_levels[static_cast<std::size_t>(original::ClassSlot::fighter)] = 1;
    numbers.thac0 = original::thac0(numbers.class_levels, opengold::GoldBoxGame::pool_of_radiance);
    numbers.saving_throws =
        original::saving_throws(numbers.class_levels, opengold::GoldBoxGame::pool_of_radiance);
    sheet.original = numbers;
    return sheet;
}

// A kobold as its record holds it: THAC0 20, Armor Class 7, 4 Hit Points,
// movement 6 and a 1d4 attack each round.
por::CharacterRecord kobold_record()
{
    por::CharacterRecord record;
    record.name = "KOBOLD";
    record.base_thac0 = 20;
    record.base_armor_class = 7;
    record.max_hit_points = 4;
    record.base_movement = 6;
    record.base_attacks[0] = {2, {1, 4, 0}};
    record.current_abilities = {10, 8, 10, 10, 10, 10, 0};
    record.abilities = record.current_abilities;
    return record;
}

struct Fight
{
    std::unique_ptr<rules::RulesModule> module;
    std::unique_ptr<rules::CombatSession> session;
};

Fight start(std::uint64_t seed)
{
    original::Content content;
    content.bestiary.emplace(std::string(kobold_key),
                             original::combatant_from_record(kobold_record()));
    Fight fight{original::rules_module(std::move(content)), nullptr};
    rules::Encounter encounter;
    encounter.battlefield = {8, 6, std::vector<rules::Terrain>(48, rules::Terrain::open)};
    rules::EntityId id = 1;
    for (const auto &sheet : {fighter(17, 10), fighter(12, 8)})
    {
        rules::Participant member;
        member.id = id++;
        member.name = sheet.name;
        member.definition = "member";
        member.side = rules::Side::party;
        member.cell = {0, static_cast<int>(id)};
        member.character_profile = fight.module->character_profile(sheet, {}).data;
        encounter.participants.push_back(member);
    }
    for (int n = 0; n < 3; ++n)
    {
        rules::Participant kobold;
        kobold.id = id++;
        kobold.name = "Kobold " + std::to_string(n + 1);
        kobold.definition = std::string(kobold_key);
        kobold.side = rules::Side::opposition;
        kobold.cell = {7, 1 + n * 2};
        encounter.participants.push_back(kobold);
    }
    fight.session = fight.module->create(std::move(encounter), seed);
    return fight;
}

void all_quick(rules::CombatSession &session, const std::function<bool()> &done,
               unsigned rounds = 30);

// Attack an enemy in reach, else step to the square nearest an enemy, else end.
rules::Command choose(const rules::CombatSession &session)
{
    const auto commands = session.legal_commands();
    const auto melee = std::find_if(commands.begin(), commands.end(),
                                    [](const rules::Command &c)
                                    {
                                        return c.verb == "melee";
                                    });
    if (melee != commands.end())
    {
        return *melee;
    }
    const auto state = session.snapshot();
    const auto &actor = *std::find_if(state.combatants.begin(), state.combatants.end(),
                                      [&state](const rules::CombatantView &v)
                                      {
                                          return v.id == state.actor;
                                      });
    const auto distance = [&](rules::Cell cell)
    {
        int best = 1000;
        for (const auto &v : state.combatants)
        {
            if (v.side != actor.side && v.conscious)
            {
                best = std::min(best,
                                std::max(std::abs(v.cell.x - cell.x), std::abs(v.cell.y - cell.y)));
            }
        }
        return best;
    };
    const rules::Command *best = nullptr;
    for (const auto &c : commands)
    {
        if (c.verb == "move" && distance(c.destination) < distance(actor.cell) &&
            (!best || distance(c.destination) < distance(best->destination)))
        {
            best = &c;
        }
    }
    if (best)
    {
        return *best;
    }
    return *std::find_if(commands.begin(), commands.end(),
                         [](const rules::Command &c)
                         {
                             return c.verb == "end";
                         });
}

std::vector<std::string> play(rules::CombatSession &session, unsigned steps = 1000)
{
    for (unsigned step = 0; step < steps && session.snapshot().outcome == rules::Outcome::ongoing;
         ++step)
    {
        check(session.submit(choose(session)), "Every chosen command is legal");
    }
    return session.snapshot().log();
}

void a_combatant_survives_its_encoding()
{
    const auto kobold = original::combatant_from_record(kobold_record());
    check(kobold.thac0 == 20 && kobold.armor_class == 7 && kobold.hit_points == 4 &&
              kobold.movement == 6 && kobold.attacks[0].rate_per_two_rounds == 2 &&
              kobold.attacks[0].sides == 4 && kobold.level == 0,
          "A monster fights with its record's numbers");
    check(original::Combatant::decode(kobold.encode()) == kobold,
          "A combatant's profile round-trips");
    check(!original::Combatant::decode("combatant1 1 2"), "A short profile is refused");
    auto native = kobold;
    native.native_progression = original::NativeProgression{};
    native.native_progression->race = static_cast<int>(original::Race::krynn_human);
    native.native_progression->wisdom = 12;
    native.native_progression->experience = 12345;
    native.native_progression->drains.push_back({2, 8, 6, 5000, false, 5, 12345, 40});
    check(native.encode().starts_with("combatant15 ") &&
              original::Combatant::decode(native.encode()) == native &&
              kobold.encode().starts_with("combatant14 ") &&
              !original::Combatant::decode(native.encode().substr(0, native.encode().size() - 3)),
          "Native receipts round-trip without changing legacy profile14 "
          "encoding");
    const auto module = original::rules_module();
    const auto strong =
        original::Combatant::decode(module->character_profile(fighter(18, 9), {}).data);
    check(strong && strong->thac0 == 20 - 1 && strong->attacks[0].dice == 1 &&
              strong->attacks[0].sides == 2 && strong->attacks[0].modifier == 2 &&
              strong->attacks[0].rate_per_two_rounds == 2,
          "An unarmed fighter of Strength 18: 1d2 + 2, +1 to hit, one attack a round");
}

void a_melee_fight_resolves()
{
    auto fight = start(42);
    const auto log = play(*fight.session);
    const auto end = fight.session->snapshot();
    check(end.outcome != rules::Outcome::ongoing, "The fight ends");
    // Each creature's Hit Points lost are the damage the log gives it.
    std::map<std::string, int> damage;
    for (const auto &entry : end.log_entries)
    {
        if (entry.message.source == "{attacker} hits {name} for {damage}.")
        {
            std::string name, amount;
            for (const auto &argument : entry.message.arguments)
            {
                if (argument.name == "name")
                {
                    name = argument.value;
                }
                else if (argument.name == "damage")
                {
                    amount = argument.value;
                }
            }
            damage[name] += std::stoi(amount);
        }
    }
    check(!damage.empty(), "Blows land");
    for (const auto &creature : end.combatants)
    {
        if (creature.dead || !creature.conscious)
        {
            continue;
        }
        check(creature.max_hit_points - creature.hit_points == damage[creature.name],
              creature.name + "'s Hit Points account for every hit");
    }
    check(play(*start(42).session) == log, "The same dice play the same fight");
    check(play(*start(43).session) != log, "Other dice play another");
}

void a_saved_fight_goes_on_unchanged()
{
    auto whole = start(7);
    const auto log = play(*whole.session);
    auto first = start(7);
    play(*first.session, 6);
    const auto checkpoint = first.session->save();
    auto restored = first.module->restore(checkpoint);
    check(restored->save() == checkpoint, "A restored fight saves as it was saved");
    check(play(*restored) == log, "A fight saved partway goes on as it would have");
}

// A sword's damage, Strength and magic, and armor and a shield, as the
// original works them out for a creature (charhlp.pas, RecalcStats).
void readied_gear_counts()
{
    auto record = kobold_record();
    record.strength_bonus_flag_raw = 1;
    record.current_abilities.strength = 18;
    record.current_abilities.exceptional_strength = 50;
    por::ItemTemplate sword;
    sword.worn_location = 0;
    sword.small_medium_damage = {1, 8, 0};
    sword.properties = por::item_property::strength_bonus;
    por::ItemRecord magic_sword;
    magic_sword.type = 36;
    magic_sword.magic_bonus = 1;
    magic_sword.readied_raw = 1;
    por::ItemTemplate chain_mail;
    chain_mail.worn_location = 2;
    chain_mail.protection_raw = 0x80 | 55; // Armor Class 5.
    por::ItemRecord mail;
    mail.weight = 300;
    mail.readied_raw = 1;
    por::ItemRecord shield;
    shield.readied_raw = 1;
    por::ItemTemplate shield_type;
    shield_type.worn_location = 1;
    shield_type.protection_raw = 0x80 | 1;
    const std::vector<original::CarriedItem> gear{
        {magic_sword, sword}, {mail, chain_mail}, {shield, shield_type}};
    const auto armed = original::combatant_from_record(record, gear);
    check(armed.attacks[0].dice == 1 && armed.attacks[0].sides == 8 &&
              armed.attacks[0].modifier == 3 + 1,
          "A long sword +1 in the hands of Strength 18/50: 1d8 + 3 + 1");
    check(armed.thac0 == 20 - 1 - 1, "+1 to hit for Strength, +1 for the sword's magic");
    check(armed.armor_class == 4 && armed.rear_armor_class == 7,
          "Chain mail and a shield: Armor Class 4, and 7 from behind without the shield");
    // Armor slows only by its weight in the load (the user, 2026-11-12): 300
    // coins is well within Strength 18/50's 1,000.
    check(armed.movement == 6, "Chain mail does not change a kobold's 6");
    record.base_movement = 12;
    check(original::combatant_from_record(record, gear).movement == 12,
          "Nor a creature's 12, within its allowance");
    record.wealth[3] = 2000;
    check(original::combatant_from_record(record, gear).movement == 3,
          "2,000 gold more is 1,300 coins over the allowance: movement 3");
    auto unready = gear;
    unready[1].item.readied_raw = 0;
    check(original::combatant_from_record(record, unready).armor_class == 6 &&
              original::combatant_from_record(record, unready).movement == 3,
          "Mail carried but not worn protects nothing and still weighs");
}

// Pool of Radiance's own records, when its files are here (installed_games.h):
// every creature fights with sensible numbers, and kobolds of the Slums fight
// a party to the end.
void real_records_fight()
{
    const auto game =
        opengold::test::installed_game_folder(opengold::GoldBoxGame::pool_of_radiance);
    if (game.empty())
    {
        std::cout << "Pool of Radiance's files are not here; its records are not checked.\n";
        return;
    }
    const auto catalog = por::CreatureCatalog::load(game);
    auto content = load_original_content(game, GoldBoxGame::pool_of_radiance);
    for (const auto &[id, creature] : catalog.all())
    {
        const auto &numbers = content.bestiary.at(id.key());
        check(original::Combatant::decode(numbers.encode()) == numbers,
              id.key() + " survives its profile's encoding");
        check(numbers.thac0 >= -20 && numbers.thac0 <= 25 && numbers.armor_class >= -20 &&
                  numbers.armor_class <= 20 && numbers.hit_points > 0 && numbers.movement >= 0,
              id.key() + " " + creature.stored.name + " fights with sensible numbers");
    }
    check(content.bestiary.size() == catalog.all().size(), "Every record is in the bestiary");
    const auto module = original::rules_module(std::move(content));
    rules::Encounter encounter;
    encounter.battlefield = {10, 8, std::vector<rules::Terrain>(80, rules::Terrain::open)};
    rules::Participant member;
    member.id = 1;
    member.name = "Fighter";
    member.definition = "member";
    member.cell = {1, 3};
    member.character_profile = module->character_profile(fighter(17, 10), {}).data;
    encounter.participants.push_back(member);
    for (rules::EntityId n = 0; n < 2; ++n)
    {
        rules::Participant kobold;
        kobold.id = 2 + n;
        kobold.name = "Kobold";
        kobold.definition = "MON2CHA.DAX:0";
        kobold.side = rules::Side::opposition;
        kobold.cell = {8, 2 + static_cast<int>(n) * 2};
        encounter.participants.push_back(kobold);
    }
    auto ai_encounter = encounter;
    auto session = module->create(std::move(encounter), 11);
    play(*session);
    check(session->snapshot().outcome != rules::Outcome::ongoing,
          "A fight with the Slums' kobolds ends");
    // The same fight played wholly by the original's AI.
    auto quick = module->create(std::move(ai_encounter), 11);
    all_quick(
        *quick,
        [&]
        {
            return quick->snapshot().outcome != rules::Outcome::ongoing;
        },
        100);
    check(quick->snapshot().outcome != rules::Outcome::ongoing,
          "Played by the AI, the fight with the Slums' kobolds ends");
}

// A creature of hand-made numbers placed on an open field.
struct Placed
{
    std::string name;
    rules::Side side;
    rules::Cell cell;
    original::Combatant numbers;
};

original::Combatant creature(int hit_points, int level = 1)
{
    original::Combatant c;
    c.hit_points = c.max_hit_points = hit_points;
    c.attacks[0] = {2, 1, 2, 0};
    c.level = level;
    return c;
}

Fight arrange(const std::vector<Placed> &placed, std::uint64_t seed,
              const std::vector<rules::Cell> &walls = {}, unsigned morale = 100,
              GoldBoxGame game = GoldBoxGame::pool_of_radiance, bool outdoors = false,
              bool no_magic = false,
              const std::function<void(rules::Encounter &)> &adjust = nullptr,
              std::vector<por::ItemTemplate> item_types = {}, bool gazes_played = false)
{
    original::Content content;
    content.game = game;
    content.item_types = std::move(item_types);
    Fight fight{original::rules_module(std::move(content)), nullptr};
    fight.module->play_unplayed_gazes(gazes_played);
    rules::Encounter encounter;
    encounter.morale = morale;
    encounter.outdoors = outdoors;
    encounter.no_magic = no_magic;
    encounter.battlefield = {8, 8, std::vector<rules::Terrain>(64, rules::Terrain::open)};
    for (const auto wall : walls)
    {
        encounter.battlefield.terrain[static_cast<std::size_t>(wall.y * 8 + wall.x)] =
            rules::Terrain::obstacle;
    }
    rules::EntityId id = 1;
    for (const auto &p : placed)
    {
        rules::Participant participant;
        participant.id = id++;
        participant.name = p.name;
        participant.definition = p.side == rules::Side::party ? "member" : "monster";
        participant.side = p.side;
        participant.cell = p.cell;
        participant.character_profile = p.numbers.encode();
        encounter.participants.push_back(participant);
    }
    if (adjust)
    {
        adjust(encounter);
    }
    fight.session = fight.module->create(std::move(encounter), seed);
    return fight;
}

const rules::CombatantView &view_of(const rules::Snapshot &state, std::string_view name)
{
    return *std::find_if(state.combatants.begin(), state.combatants.end(),
                         [name](const rules::CombatantView &v)
                         {
                             return v.name == name;
                         });
}

// A temporary snapshot cannot own a borrowed view after this call returns.
rules::CombatantView view_of(rules::Snapshot &&state, std::string_view name)
{
    return view_of(static_cast<const rules::Snapshot &>(state), name);
}

std::string actor_name(const rules::CombatSession &session)
{
    const auto state = session.snapshot();
    for (const auto &v : state.combatants)
    {
        if (v.id == state.actor)
        {
            return v.name;
        }
    }
    return {};
}

// The legal command of this verb, at this target or destination when given.
std::optional<rules::Command> offered(const rules::CombatSession &session, std::string_view verb,
                                      std::string_view target = {},
                                      std::optional<rules::Cell> destination = {})
{
    const auto state = session.snapshot();
    for (const auto &c : session.legal_commands())
    {
        if (c.verb == verb && (target.empty() || c.target == view_of(state, target).id) &&
            (!destination || c.destination == *destination))
        {
            return c;
        }
    }
    return std::nullopt;
}

rules::Command order(const rules::CombatSession &session, std::string_view verb,
                     std::string_view target = {}, std::optional<rules::Cell> destination = {})
{
    const auto command = offered(session, verb, target, destination);
    check(command.has_value(), actor_name(session) + " is offered " + std::string(verb));
    return *command;
}

// How many log entries have this template, from this attacker when given.
int logged(const rules::CombatSession &session, std::string_view source,
           std::string_view attacker = {})
{
    int count = 0;
    for (const auto &entry : session.snapshot().log_entries)
    {
        const bool by = attacker.empty() ||
                        std::any_of(entry.message.arguments.begin(), entry.message.arguments.end(),
                                    [attacker](const rules::MessageArgument &a)
                                    {
                                        return a.name == "attacker" && a.value == attacker;
                                    });
        count += entry.message.source == source && by ? 1 : 0;
    }
    return count;
}

// Each creature does what the policy gives it until the condition holds.
void play_until(rules::CombatSession &session,
                const std::function<rules::Command(const std::string &actor)> &policy,
                const std::function<bool()> &done, unsigned steps = 200)
{
    for (unsigned step = 0;
         step < steps && !done() && session.snapshot().outcome == rules::Outcome::ongoing; ++step)
    {
        check(session.submit(policy(actor_name(session))), "Every chosen command is legal");
    }
}

void until_turn_of(rules::CombatSession &session, std::string_view name);

// Read the stored cost as a number, rather than letting an out-of-range
// number wrap to a different, valid cost. Exercise both cost-bearing layouts.
void checkpoint_square_cost_values(const Fight &fight, std::size_t costs_at)
{
    const auto checkpoint = fight.session->save();
    const auto first_cost = checkpoint.find(' ', costs_at) + 1;
    const auto cost_end = checkpoint.find(' ', first_cost);
    check(first_cost > costs_at && cost_end != std::string::npos,
          "The checkpoint fixture contains a square cost");
    const auto with_cost = [&](std::string_view cost)
    {
        auto changed = checkpoint;
        changed.replace(first_cost, cost_end - first_cost, cost);
        return changed;
    };
    for (unsigned cost = 1; cost <= 255; ++cost)
    {
        const auto changed = with_cost(std::to_string(cost));
        const auto restored = fight.module->restore(changed);
        check(restored->snapshot().battlefield.move_cost.front() == cost &&
                  restored->save() == changed,
              "A checkpoint preserves every valid square cost: " + std::to_string(cost));
    }
    for (const auto cost : {"0", "256", "257", "4294967297", "18446744073709551616"})
    {
        test::rejects(
            [&]
            {
                (void)fight.module->restore(with_cost(cost));
            },
            "A checkpoint rejects an out-of-range square cost without narrowing it: " +
                std::string(cost));
    }
}

// The checkpoint keeps each square's cost (format 30); one written before
// the costs (27) still reads, with none.
void a_checkpoint_keeps_the_costs()
{
    auto costly = arrange({{"Walker", rules::Side::party, {1, 3}, creature(50)},
                           {"Kobold", rules::Side::opposition, {7, 7}, creature(50)}},
                          9, {}, 100, GoldBoxGame::curse_of_the_azure_bonds, false, false,
                          [](rules::Encounter &encounter)
                          {
                              encounter.battlefield.move_cost.assign(64, 2);
                          });
    const auto save = costly.session->save();
    check(save.find("OGORIGINALCOMBAT 30") != std::string::npos, "Checkpoints are format 30");
    const auto restored = costly.module->restore(save);
    check(restored->snapshot().battlefield.move_cost == std::vector<std::uint8_t>(64, 2) &&
              restored->save() == save,
          "A restored fight keeps each square's cost");
    auto plain = arrange({{"Walker", rules::Side::party, {1, 3}, creature(50)},
                          {"Kobold", rules::Side::opposition, {7, 7}, creature(50)}},
                         9);
    const auto now = plain.session->save();
    // Width, height, 64 squares, no plants and no heights: the cost count
    // is the 71st value.
    std::size_t at = 0;
    for (int value = 0; value < 70; ++value)
    {
        at = now.find(' ', at) + 1;
    }
    check(now.compare(at, 2, "0 ") == 0, "A fight without costs records none");
    auto older = now;
    older.erase(at, 2);
    older.replace(older.find("OGORIGINALCOMBAT 30"), 19, "OGORIGINALCOMBAT 27");
    const auto read = plain.module->restore(older);
    check(read->snapshot().battlefield.move_cost.empty() && read->save() == now,
          "A format 27 checkpoint reads with no costs");
    checkpoint_square_cost_values(costly, at);
}

// The same fight, restored to move as the original or not (the game's
// Original movement option).
void move_as(Fight &fight, bool original)
{
    fight.module->move_as_the_original(original);
    fight.session = fight.module->restore(fight.session->save());
}

// The Original movement option, on by default (docs/ORIGINAL-RULES.md). On, a
// fight moves as the original (Curse's cbtacts.pas CombatMoveCreature,
// cbtmap.pas ProbeDirection, combatai.pas AiCanStep, combat.pas's move menu):
// a square costs its terrain table's cost, a diagonal step may pass a wall's
// corner, the computer steps only while it keeps some movement (Pool of
// Radiance's spends its last half-moves) and a last half-move is lost as a
// move ends. Off, it moves as OpenGoldBox did before.
void the_original_movement_option()
{
    for (const bool original : {true, false})
    {
        const std::string setting = original ? "On: " : "Off: ";
        auto costly = arrange({{"Walker", rules::Side::party, {1, 3}, creature(50)},
                               {"Kobold", rules::Side::opposition, {7, 7}, creature(50)}},
                              9, {}, 100, GoldBoxGame::curse_of_the_azure_bonds, false, false,
                              [](rules::Encounter &encounter)
                              {
                                  encounter.battlefield.move_cost.assign(64, 1);
                                  encounter.battlefield.move_cost[3 * 8 + 2] = 4;
                              });
        move_as(costly, original);
        until_turn_of(*costly.session, "Walker");
        const int before = view_of(costly.session->snapshot(), "Walker").movement_feet;
        check(costly.session->submit(order(*costly.session, "move", {}, rules::Cell{2, 3})),
              setting + "the walker steps onto ground costing 4");
        check(before - view_of(costly.session->snapshot(), "Walker").movement_feet ==
                  (original ? 20 : 5),
              setting + "it pays 4 straight steps, or 1 off");
        auto walled = arrange({{"Walker", rules::Side::party, {0, 0}, creature(50)},
                               {"Kobold", rules::Side::opposition, {7, 7}, creature(50)}},
                              9, {{1, 0}, {0, 1}}, 100, GoldBoxGame::curse_of_the_azure_bonds);
        move_as(walled, original);
        until_turn_of(*walled.session, "Walker");
        check(offered(*walled.session, "move", {}, rules::Cell{1, 1}).has_value() == original,
              setting + "walled in on two sides, it steps out between the walls' corners only on");
        for (const auto game :
             {GoldBoxGame::pool_of_radiance, GoldBoxGame::curse_of_the_azure_bonds})
        {
            auto walker = creature(50);
            walker.movement = 3;
            auto chase = arrange({{"Target", rules::Side::party, {7, 3}, creature(500)},
                                  {"Walker", rules::Side::opposition, {0, 3}, walker}},
                                 5, {}, 100, game);
            move_as(chase, original);
            auto &session = *chase.session;
            const auto first_round = session.snapshot().round;
            play_until(
                session,
                [&](const std::string &actor)
                {
                    return order(session, actor == "Walker" ? "quick" : "end");
                },
                [&]
                {
                    return session.snapshot().round > first_round;
                });
            const bool keeps_some = original && game != GoldBoxGame::pool_of_radiance;
            check(view_of(session.snapshot(), "Walker").cell == rules::Cell{keeps_some ? 2 : 3, 3},
                  setting + "movement 3 walks two squares where the computer keeps its last "
                            "half-moves, three where it spends them");
        }
        auto walker = creature(50);
        walker.movement = 6;
        auto mover = arrange({{"Walker", rules::Side::party, {0, 0}, walker},
                              {"Kobold", rules::Side::opposition, {7, 7}, creature(50)}},
                             9, {}, 100, GoldBoxGame::curse_of_the_azure_bonds);
        move_as(mover, original);
        until_turn_of(*mover.session, "Walker");
        check(mover.session->submit(order(*mover.session, "move", {}, rules::Cell{4, 3})),
              setting + "three diagonal steps and a straight one: 11 of its 12 half-moves");
        check(view_of(mover.session->snapshot(), "Walker").movement_feet == (original ? 0 : 2),
              setting + "the half-move left is lost, or kept off");
    }
}

// A guard strikes an enemy that comes next to it; one that only ended its
// actions does not (cbtacts.pas, GuardersAttack).
void a_guard_strikes_who_comes_near()
{
    for (const bool guards : {true, false})
    {
        auto kobold = creature(50, 0);
        kobold.movement = 6;
        auto fight = arrange({{"Guard", rules::Side::party, {1, 3}, creature(50)},
                              {"Kobold", rules::Side::opposition, {5, 3}, kobold}},
                             3);
        auto &session = *fight.session;
        bool came = false;
        play_until(
            session,
            [&](const std::string &actor)
            {
                if (actor == "Guard")
                {
                    return order(session, guards ? "guard" : "end");
                }
                const bool ready = logged(session, "{name} guards.") > 0 || !guards;
                if (ready && !came && session.snapshot().round > 1)
                {
                    came = true;
                    return order(session, "move", {}, rules::Cell{2, 3});
                }
                return order(session, "end");
            },
            [&]
            {
                return came;
            });
        check(came, "The kobold comes next to the guard");
        check(logged(session, "{attacker} strikes from guard.", "Guard") == (guards ? 1 : 0),
              guards ? "The guard strikes it" : "One not guarding does not");
    }
}

// Stepping away from an enemy draws its free attack, as from behind
// (cbtacts.pas, AttacksOfOpportunity).
void leaving_an_enemy_draws_a_free_attack()
{
    auto fight = arrange({{"Fighter", rules::Side::party, {3, 3}, creature(50)},
                          {"Kobold", rules::Side::opposition, {4, 3}, creature(50, 0)}},
                         5);
    auto &session = *fight.session;
    bool left = false;
    play_until(
        session,
        [&](const std::string &actor)
        {
            if (actor == "Kobold")
            {
                left = true;
                return order(session, "move", {}, rules::Cell{7, 3});
            }
            return order(session, "end");
        },
        [&]
        {
            return left;
        });
    check(logged(session, "{attacker} strikes {name} leaving.", "Fighter") == 1,
          "The fighter strikes the kobold as it leaves");
    check(logged(session, "{attacker} hits {name} for {damage}.", "Fighter") +
                  logged(session, "{attacker} misses {name}.", "Fighter") ==
              1,
          "Once");
}

// Delay: the creature acts again later in the round, at initiative 1 (combat.pas, DoneMenu).
void delay_acts_later_in_the_round()
{
    auto fight = arrange({{"A", rules::Side::party, {0, 1}, creature(20)},
                          {"B", rules::Side::party, {0, 3}, creature(20)},
                          {"C", rules::Side::party, {0, 5}, creature(20)},
                          {"Kobold", rules::Side::opposition, {7, 3}, creature(20, 0)}},
                         9);
    auto &session = *fight.session;
    const auto first = actor_name(session);
    session.submit(order(session, "delay"));
    check(view_of(session.snapshot(), first).initiative == 1, "Delaying leaves initiative 1");
    check(actor_name(session) != first || session.snapshot().combatants.front().initiative == 1,
          "Another acts first unless all are at 1");
    bool again = false;
    play_until(
        session,
        [&](const std::string &actor)
        {
            again = actor == first && session.snapshot().round == 1;
            return order(session, "end");
        },
        [&]
        {
            return again || session.snapshot().round > 1;
        });
    check(again, first + " acts again in the same round");
}

// A thief behind a defender that has stopped turning to face its attackers
// backstabs: against its Armor Class from behind, for its level div 4 + 2
// times the damage (cbtacts.pas, IsBackstab). Another in that place does not.
void a_thief_backstabs_from_behind()
{
    for (const bool thief : {true, false})
    {
        auto third = creature(50);
        third.thief_level = thief ? 1 : 0;
        third.backstabs = thief;
        auto fight = arrange({{"A", rules::Side::party, {2, 3}, creature(50)},
                              {"B", rules::Side::party, {3, 2}, creature(50)},
                              {"Thief", rules::Side::party, {3, 4}, third},
                              {"Ogre", rules::Side::opposition, {3, 3}, creature(1000, 0)}},
                             13);
        auto &session = *fight.session;
        play_until(
            session,
            [&](const std::string &actor)
            {
                if (actor == "Ogre")
                {
                    return order(session, "end");
                }
                if (actor == "Thief" && view_of(session.snapshot(), actor).initiative > 1)
                {
                    return order(session, "delay");
                }
                return order(session, "melee", "Ogre");
            },
            [&]
            {
                return logged(session, "{attacker} backstabs {name}.") > 0;
            });
        check((logged(session, "{attacker} backstabs {name}.", "Thief") > 0) == thief,
              thief ? "The thief backstabs" : "A fighter there does not");
        const auto &entries = session.snapshot().log_entries;
        for (std::size_t n = 0; n + 1 < entries.size(); ++n)
        {
            if (entries[n].message.source == "{attacker} backstabs {name}." &&
                entries[n + 1].message.source == "{attacker} hits {name} for {damage}.")
            {
                check(std::stoi(entries[n + 1].message.arguments[2].value) % 2 == 0,
                      "A level-1 thief's backstab does double damage");
            }
        }
    }
}

// A fighter among more level-0 enemies than its attacks sweeps through as
// many as its level (cbtacts.pas, SweepAttack); a level-1 fighter does not.
void a_fighter_sweeps_level_0_enemies()
{
    for (const int level : {3, 1})
    {
        auto fighter_numbers = creature(50, level);
        fighter_numbers.attack_level = level;
        auto fight = arrange({{"Fighter", rules::Side::party, {3, 3}, fighter_numbers},
                              {"K1", rules::Side::opposition, {2, 3}, creature(50, 0)},
                              {"K2", rules::Side::opposition, {4, 3}, creature(50, 0)},
                              {"K3", rules::Side::opposition, {3, 2}, creature(50, 0)},
                              {"K4", rules::Side::opposition, {3, 4}, creature(50, 0)}},
                             17);
        auto &session = *fight.session;
        bool struck = false;
        play_until(
            session,
            [&](const std::string &actor)
            {
                if (actor == "Fighter")
                {
                    struck = true;
                    return order(session, "melee", "K1");
                }
                return order(session, "end");
            },
            [&]
            {
                return struck;
            });
        const int swings = logged(session, "{attacker} hits {name} for {damage}.", "Fighter") +
                           logged(session, "{attacker} misses {name}.", "Fighter");
        check(logged(session, "{attacker} sweeps.", "Fighter") == (level == 3 ? 1 : 0) &&
                  swings == (level == 3 ? 3 : 1),
              level == 3 ? "A level-3 fighter sweeps through three kobolds"
                         : "A level-1 fighter strikes once");
    }
}

// Only a party member next to a dying one can bandage it (the user, 2026-11-12).
void bandage_needs_a_neighbor()
{
    auto ogre = creature(1000, 0);
    ogre.thac0 = 0;
    ogre.movement = 0;
    ogre.attacks[0] = {2, 1, 1, 5};
    auto fight = arrange({{"Victim", rules::Side::party, {4, 3}, creature(1)},
                          {"Helper", rules::Side::party, {3, 3}, creature(50)},
                          {"Far", rules::Side::party, {0, 0}, creature(50)},
                          {"Ogre", rules::Side::opposition, {5, 3}, ogre}},
                         19);
    auto &session = *fight.session;
    bool far_offered = false;
    play_until(
        session,
        [&](const std::string &actor)
        {
            const bool dying = view_of(session.snapshot(), "Victim").status == "dying";
            if (actor == "Ogre" && !dying && view_of(session.snapshot(), "Victim").conscious)
            {
                return order(session, "melee", "Victim");
            }
            if (actor == "Far")
            {
                far_offered = far_offered || offered(session, "bandage", "Victim").has_value();
            }
            if (actor == "Helper" && dying)
            {
                return order(session, "bandage", "Victim");
            }
            return order(session, "end");
        },
        [&]
        {
            return logged(session, "{name} is bandaged.") > 0;
        });
    check(logged(session, "{name} is bandaged.") == 1 &&
              view_of(session.snapshot(), "Victim").status == "unconscious",
          "The member next to the dying one bandages it");
    check(!far_offered, "One across the field cannot");
}

// A bow, a sling and a thrown dagger as the original reads them from a
// creature's readied items (cbtacts.pas, RecalcAttacksLeft and CombatAimMenu).
void readied_missiles_count()
{
    namespace bit = por::item_property;
    const auto record = kobold_record();
    por::ItemTemplate bow_type;
    bow_type.worn_location = 0;
    bow_type.range = 8;
    bow_type.rate_of_fire = 4;
    bow_type.small_medium_damage = {1, 6, 0};
    bow_type.properties = bit::fires_arrows | bit::dexterity_missile | bit::fires_ammunition;
    por::ItemRecord bow;
    bow.type = 42;
    bow.readied_raw = 1;
    por::ItemTemplate arrow_type;
    // Arrows' type sits in place 10 with quarrels; arrows are item type 73.
    arrow_type.worn_location = 10;
    por::ItemRecord arrows;
    arrows.type = 73;
    arrows.readied_raw = 1;
    arrows.stack_size = 10;
    const std::vector<original::CarriedItem> archer{{bow, bow_type}, {arrows, arrow_type}};
    const auto shooting = original::combatant_from_record(record, archer);
    check(shooting.missile && shooting.missile->range == 7 &&
              shooting.missile->rate_per_two_rounds == 4 && shooting.missile->loaded &&
              shooting.missile->quantity == 10 && !shooting.missile->thrown,
          "A bow reaches its range less 1, fires twice a round and has its 10 arrows");
    check(original::Combatant::decode(shooting.encode()) == shooting,
          "A missile weapon survives the profile's encoding");
    const std::vector<original::CarriedItem> bow_alone{{bow, bow_type}};
    check(!original::combatant_from_record(record, bow_alone).missile->loaded,
          "A bow without arrows readied has nothing to shoot");
    auto sling_type = bow_type;
    sling_type.properties = bit::dexterity_missile | bit::fires_ammunition;
    sling_type.rate_of_fire = 0;
    const std::vector<original::CarriedItem> slinger{{bow, sling_type}};
    const auto sling = original::combatant_from_record(record, slinger).missile;
    check(sling && sling->loaded && sling->needs_no_ammunition && sling->rate_per_two_rounds == 2,
          "A sling shoots with nothing readied, at least once a round");
    por::ItemTemplate dagger_type;
    dagger_type.worn_location = 0;
    dagger_type.range = 4;
    dagger_type.small_medium_damage = {1, 4, 0};
    dagger_type.properties = bit::thrown | bit::strength_bonus;
    por::ItemRecord dagger;
    dagger.type = 8;
    dagger.readied_raw = 1;
    dagger.magic_bonus = 1;
    const std::vector<original::CarriedItem> thrower{{dagger, dagger_type}};
    const auto throwing = original::combatant_from_record(record, thrower);
    const auto empty_handed = original::combatant_from_record(record);
    check(throwing.missile && throwing.missile->thrown && throwing.missile->throwable &&
              throwing.missile->loaded && throwing.missile->quantity == 0 &&
              throwing.missile->empty_handed_thac0 == empty_handed.thac0 &&
              throwing.missile->empty_handed_attack == empty_handed.attacks[0] &&
              throwing.thac0 == empty_handed.thac0 - 1,
          "A dagger +1 is its own single shot, and its thrower then fights empty-handed");
}

original::Combatant archer(int arrows)
{
    auto c = creature(50);
    original::Missile bow;
    bow.range = 7;
    bow.rate_per_two_rounds = 4;
    bow.loaded = true;
    bow.quantity = arrows;
    c.missile = bow;
    return c;
}

// The archer's turn, the others ending theirs until it comes.
void until_turn_of(rules::CombatSession &session, std::string_view name)
{
    play_until(
        session,
        [&](const std::string &)
        {
            return order(session, "end");
        },
        [&]
        {
            return actor_name(session) == name;
        });
}

// A missile weapon shoots what it sees within its range, with no penalty for
// distance (the user, 2026-11-12), but not with an enemy next to it; it cannot
// guard (cbtacts.pas, AimTargetMenu; combat.pas, DoneMenu).
void missiles_shoot_what_they_see()
{
    const auto can_shoot = [](rules::Cell target, const std::vector<rules::Cell> &walls)
    {
        auto fight = arrange({{"Archer", rules::Side::party, {0, 3}, archer(10)},
                              {"Ogre", rules::Side::opposition, target, creature(1000, 0)}},
                             21, walls);
        until_turn_of(*fight.session, "Archer");
        check(!offered(*fight.session, "guard"), "An archer cannot guard");
        return offered(*fight.session, "ranged", "Ogre").has_value();
    };
    check(can_shoot({7, 3}, {}), "7 squares away is within a range of 7");
    check(can_shoot({6, 6}, {}), "6 across and 3 down counts 7");
    check(!can_shoot({7, 6}, {}), "7 across and 3 down counts 8, out of range");
    check(!can_shoot({7, 3}, {{3, 3}}), "A wall between blocks the shot");
    auto fight = arrange({{"Archer", rules::Side::party, {0, 3}, archer(10)},
                          {"Kobold", rules::Side::opposition, {1, 3}, creature(1000, 0)},
                          {"Ogre", rules::Side::opposition, {5, 3}, creature(1000, 0)}},
                         21);
    until_turn_of(*fight.session, "Archer");
    check(!offered(*fight.session, "ranged") && !offered(*fight.session, "melee"),
          "With a kobold next to it the archer neither shoots nor strikes");
}

// Each shot spends an arrow: 2 a round from 3 arrows, then 1, then none
// (cbtacts.pas, RecalcAttacksLeft and MakeAttack).
void shots_spend_ammunition()
{
    auto fight = arrange({{"Archer", rules::Side::party, {0, 3}, archer(3)},
                          {"Ogre", rules::Side::opposition, {5, 3}, creature(1000, 0)}},
                         23);
    auto &session = *fight.session;
    std::vector<int> volleys;
    play_until(
        session,
        [&](const std::string &actor)
        {
            if (actor == "Archer")
            {
                if (const auto shot = offered(session, "ranged", "Ogre"))
                {
                    const int before =
                        logged(session, "{attacker} hits {name} for {damage}.", "Archer") +
                        logged(session, "{attacker} misses {name}.", "Archer");
                    session.submit(*shot);
                    volleys.push_back(
                        logged(session, "{attacker} hits {name} for {damage}.", "Archer") +
                        logged(session, "{attacker} misses {name}.", "Archer") - before);
                    return order(session, "end");
                }
                volleys.push_back(0);
            }
            return order(session, "end");
        },
        [&]
        {
            return !volleys.empty() && volleys.back() == 0;
        },
        60);
    check(volleys == std::vector<int>{2, 1, 0}, "Two arrows, then the last, then none");
}

// A thrown dagger strikes next to an enemy as in melee and keeps it; thrown,
// its single dagger is gone and its thrower fights empty-handed.
void a_thrown_weapon_leaves_the_hand()
{
    auto thrower = creature(50);
    thrower.thac0 = 15;
    thrower.attacks[0] = {2, 1, 4, 1};
    original::Missile dagger;
    dagger.range = 3;
    dagger.thrown = dagger.throwable = dagger.loaded = true;
    dagger.empty_handed_thac0 = 16;
    dagger.empty_handed_attack = {2, 1, 2, 0};
    thrower.missile = dagger;
    auto fight = arrange({{"Thrower", rules::Side::party, {0, 3}, thrower},
                          {"Kobold", rules::Side::opposition, {1, 3}, creature(1000, 0)},
                          {"Ogre", rules::Side::opposition, {3, 3}, creature(1000, 0)}},
                         29);
    auto &session = *fight.session;
    until_turn_of(session, "Thrower");
    check(offered(session, "melee", "Kobold").has_value() &&
              offered(session, "throw", "Ogre").has_value() && offered(session, "guard"),
          "A dagger strikes the kobold next to it, can be thrown at the ogre, and guards");
    session.submit(order(session, "throw", "Ogre"));
    until_turn_of(session, "Thrower");
    check(!offered(session, "throw", "Ogre") && offered(session, "melee", "Kobold"),
          "Thrown, the dagger is gone; empty-handed, its thrower still strikes in melee");
    const auto restored = fight.module->restore(session.save());
    check(!offered(*restored, "throw", "Ogre"), "A saved fight remembers the dagger is gone");
}

// A monster with morale 50 (its NPC byte 0x80 + 25) that has lost three
// quarters of its Hit Points, in an encounter of morale 50.
original::Combatant shaken(int movement, int intelligence)
{
    auto c = creature(20, 0);
    c.hit_points = 5;
    c.morale = 0x80 + 25;
    c.movement = movement;
    c.intelligence = intelligence;
    return c;
}

// What happens at the shaken monster's first turn, the fighter ending its own.
std::string monster_turn(const original::Combatant &monster, unsigned morale = 50)
{
    auto fight = arrange({{"Fighter", rules::Side::party, {0, 3}, creature(50)},
                          {"Monster", rules::Side::opposition, {5, 3}, monster}},
                         31, {}, morale);
    auto &session = *fight.session;
    play_until(
        session,
        [&](const std::string &)
        {
            return order(session, "end");
        },
        [&]
        {
            return actor_name(session) == "Monster" ||
                   session.snapshot().outcome != rules::Outcome::ongoing;
        });
    const auto &view = view_of(session.snapshot(), "Monster");
    if (view.panicked)
    {
        return "flees";
    }
    if (view.surrendered)
    {
        check(view.hit_points == 0, "One that gives up has no Hit Points left");
        play_until(
            session,
            [&](const std::string &)
            {
                return order(session, "end");
            },
            [&]
            {
                return session.snapshot().outcome != rules::Outcome::ongoing;
            });
        check(session.snapshot().outcome == rules::Outcome::victory,
              "As the round ends, its side has lost");
        return "surrenders";
    }
    return "fights";
}

// Morale (combatai.pas, CheckMorale): a monster holds while its morale is at
// least the share of Hit Points it has lost, or its side's Hit Points left are
// at least 100 less the encounter's morale. Broken, it runs when no enemy is
// faster, gives up when its Intelligence is 6 or more, and fights on otherwise.
void morale_breaks()
{
    check(monster_turn(shaken(12, 10)) == "flees", "No enemy faster: it flees in panic");
    check(monster_turn(shaken(6, 10)) == "surrenders", "Slower and clever: it surrenders");
    check(monster_turn(shaken(6, 3)) == "fights", "Slower and dull: it fights on");
    check(monster_turn(shaken(6, 10), 100) == "fights", "An encounter of morale 100 never breaks");
    auto steady = shaken(6, 10);
    steady.morale = 0x80 + 40;
    check(monster_turn(steady) == "fights", "Morale 80 holds with 75 lost");
    auto character = shaken(6, 10);
    character.morale = 0;
    check(monster_turn(character) == "fights", "A player's character never checks");
}

// Fleeing off the field (cbtacts.pas, AttemptFlee): it gets away with more
// movement than the fastest enemy and is blocked with less; with no one of its
// party left, the party has fled (postcbt.pas, DetermineCombatResults).
void fleeing_off_the_field()
{
    const auto flee = [](int enemy_movement, bool hired)
    {
        auto runner = creature(20);
        if (hired)
        {
            runner.morale = 0x80 + 30;
        }
        auto ogre = creature(1000, 0);
        ogre.movement = enemy_movement;
        auto fallen = creature(5);
        fallen.hit_points = 0;
        auto fight = arrange({{"Runner", rules::Side::party, {0, 3}, runner},
                              {"Fallen", rules::Side::party, {0, 5}, fallen},
                              {"Ogre", rules::Side::opposition, {6, 3}, ogre}},
                             37);
        auto &session = *fight.session;
        until_turn_of(session, "Runner");
        const auto step = offered(session, "flee");
        check(step && !session.snapshot().battlefield.contains(step->destination),
              "On the edge it is offered a step off the field");
        session.submit(*step);
        // A fight ends as its round ends (combat.pas, EndOfRound).
        const auto round = session.snapshot().round;
        play_until(
            session,
            [&](const std::string &)
            {
                return order(session, "end");
            },
            [&]
            {
                return session.snapshot().round > round;
            });
        return session.snapshot();
    };
    const auto away = flee(6, false);
    check(view_of(away, "Runner").fled && away.outcome == rules::Outcome::fled,
          "Faster than the ogre it gets away, and the party has fled");
    const auto blocked = flee(15, false);
    check(!view_of(blocked, "Runner").fled && blocked.outcome == rules::Outcome::ongoing,
          "Slower than the ogre its escape is blocked");
    check(flee(6, true).outcome == rules::Outcome::defeat,
          "A hired NPC getting away does not save a party whose own are down");
    auto fight = arrange({{"Runner", rules::Side::party, {3, 3}, creature(20)},
                          {"Ogre", rules::Side::opposition, {6, 3}, creature(1000, 0)}},
                         37);
    until_turn_of(*fight.session, "Runner");
    check(!offered(*fight.session, "flee"), "Away from the edge there is no fleeing");
}

// A fight saved with a monster in panic goes on as it would have.
void a_panic_survives_a_save()
{
    auto fight = arrange({{"Fighter", rules::Side::party, {0, 3}, creature(50)},
                          {"Monster", rules::Side::opposition, {5, 3}, shaken(12, 10)}},
                         41, {}, 50);
    auto &session = *fight.session;
    play_until(
        session,
        [&](const std::string &)
        {
            return order(session, "end");
        },
        [&]
        {
            return view_of(session.snapshot(), "Monster").panicked;
        });
    const auto checkpoint = session.save();
    const auto restored = fight.module->restore(checkpoint);
    check(restored->save() == checkpoint && view_of(restored->snapshot(), "Monster").panicked,
          "The panic and the encounter's morale are saved");
}

// A monster's effects come from its record (MONnSPC), code 0 being none, and
// a readied ring's save bonus adds to its saving throws (charhlp.pas, RecalcStats).
void effects_come_from_the_record()
{
    por::EffectRecord paralysis;
    paralysis.code = 68;
    por::EffectRecord none;
    por::EffectRecord timed;
    timed.code = 52;
    timed.duration_raw = 3;
    timed.data = 12;
    const std::vector<por::EffectRecord> effects{paralysis, none, timed};
    por::ItemTemplate ring_type;
    ring_type.worn_location = 9;
    ring_type.protection_raw = 0x80;
    por::ItemRecord ring;
    ring.readied_raw = 1;
    ring.magic_bonus = 1;
    ring.save_bonus = 1;
    const std::vector<original::CarriedItem> carried{{ring, ring_type}};
    const auto ghoul = original::combatant_from_record(kobold_record(), carried, effects);
    check(ghoul.effects == std::vector<original::Effect>{{68, 0, 0, false}, {52, 3, 12, false}},
          "The record's effects, without the empty one");
    check(ghoul.save_bonus == 1, "A ring of protection +1 adds 1 to saving throws");
    check(original::Combatant::decode(ghoul.encode()) == ghoul, "Effects survive the encoding");
}

// A helpless creature (paralysed here, for 2 minutes) loses its turns until
// the paralysis wears off at the second round's end; an attack on it is one
// blow that leaves it dying at -5, and it makes no free attack
// (charhlp.pas, IsHelpless; cbtacts.pas, RollAttacks).
void the_helpless_are_slain()
{
    auto held = creature(20, 0);
    held.effects.push_back({52, 2, 12, false});
    auto fight = arrange({{"Fighter", rules::Side::party, {3, 3}, creature(50)},
                          {"Kobold", rules::Side::opposition, {4, 3}, held}},
                         43);
    auto &session = *fight.session;
    std::vector<unsigned> kobold_turns;
    play_until(
        session,
        [&](const std::string &actor)
        {
            if (actor == "Kobold")
            {
                kobold_turns.push_back(session.snapshot().round);
            }
            return order(session, "end");
        },
        [&]
        {
            return session.snapshot().round > 3;
        });
    check(kobold_turns == std::vector<unsigned>{3}, "It acts first in round 3, once it can move");

    auto second = arrange({{"Fighter", rules::Side::party, {3, 3}, creature(50)},
                           {"Kobold", rules::Side::opposition, {4, 3}, held}},
                          43);
    auto &again = *second.session;
    until_turn_of(again, "Fighter");
    check(view_of(again.snapshot(), "Kobold").status == "helpless", "Paralysed, it is helpless");
    again.submit(order(again, "melee", "Kobold"));
    const auto &kobold = view_of(again.snapshot(), "Kobold");
    check(logged(again, "{attacker} slays the helpless {name}.", "Fighter") == 1 &&
              kobold.status == "dying" &&
              kobold.hit_points == (again.snapshot().outcome == rules::Outcome::ongoing ? -5 : -6),
          "One blow leaves it dying 5 below 0, a point less once the round ends");
    check(actor_name(again) != "Fighter", "The blow spends the fighter's attacks");

    auto third = arrange({{"Fighter", rules::Side::party, {3, 3}, creature(50)},
                          {"Kobold", rules::Side::opposition, {4, 3}, held}},
                         43);
    until_turn_of(*third.session, "Fighter");
    third.session->submit(order(*third.session, "move", {}, rules::Cell{0, 3}));
    check(logged(*third.session, "{attacker} strikes {name} leaving.") == 0,
          "A helpless kobold strikes no one leaving it");
}

// A monster that always hits, with these attack forms and effects.
original::Combatant monster(original::AttackForm primary, original::AttackForm secondary,
                            std::vector<original::Effect> effects)
{
    auto c = creature(1000, 1);
    c.thac0 = -20;
    c.attacks = {primary, secondary};
    c.effects = std::move(effects);
    return c;
}

// A victim that fails every saving throw but on a natural 20.
original::Combatant victim(int hit_points = 500)
{
    auto c = creature(hit_points);
    c.saving_throws = {21, 21, 21, 21, 21};
    return c;
}

// The monster attacks the victim at each of its turns, the victim ending its
// own, until the condition holds or 20 rounds pass.
Fight attack_until(const original::Combatant &attacker, const original::Combatant &target,
                   const std::function<bool(const rules::CombatSession &)> &done,
                   std::uint64_t seed = 47, GoldBoxGame game = GoldBoxGame::pool_of_radiance)
{
    auto fight = arrange({{"Victim", rules::Side::party, {3, 3}, target},
                          {"Monster", rules::Side::opposition, {4, 3}, attacker}},
                         seed, {}, 100, game);
    auto &session = *fight.session;
    play_until(
        session,
        [&](const std::string &actor)
        {
            if (actor == "Monster")
            {
                if (const auto melee = offered(session, "melee", "Victim"))
                {
                    return *melee;
                }
            }
            return order(session, "end");
        },
        [&]
        {
            return done(session) || session.snapshot().round > 20;
        },
        400);
    return fight;
}

// Poison on a secondary attack's hit kills unless saved against; immunity to
// poison saves but on a natural 1 (effhndlr.pas, 64 to 70, 111).
void poison_kills()
{
    const auto spider = monster({0, 0, 0, 0}, {2, 1, 1, 0}, {{64, 0, 0, false}});
    const auto poisoned = [](const rules::CombatSession &session)
    {
        return logged(session, "{name} is poisoned.") > 0;
    };
    auto fight = attack_until(spider, victim(), poisoned);
    check(poisoned(*fight.session) && view_of(fight.session->snapshot(), "Victim").dead,
          "A failed save against the spider's bite kills");
    int immune_deaths = 0;
    for (std::uint64_t seed = 1; seed <= 20; ++seed)
    {
        auto immune = victim();
        immune.effects.push_back({111, 0, 0, false});
        auto first_bite = attack_until(
            spider, immune,
            [](const rules::CombatSession &session)
            {
                return logged(session, "{attacker} hits {name} for {damage}.", "Monster") > 0;
            },
            seed);
        immune_deaths += poisoned(*first_bite.session) ? 1 : 0;
    }
    check(immune_deaths < 5, "Immune to poison, it dies of a bite only on a natural 1");
    const auto primary_only = monster({2, 1, 1, 0}, {0, 0, 0, 0}, {{64, 0, 0, false}});
    auto fangless = attack_until(primary_only, victim(),
                                 [](const rules::CombatSession &session)
                                 {
                                     return session.snapshot().round > 5;
                                 });
    check(!poisoned(*fangless.session), "Poison rides only on a secondary attack");
}

// A ghoul's touch paralyses for 63 minutes, but not an elf (effhndlr.pas, 68).
void a_ghoul_paralyses()
{
    const auto ghoul = monster({2, 1, 1, 0}, {0, 0, 0, 0}, {{68, 0, 0, false}});
    const auto paralysed = [](const rules::CombatSession &session)
    {
        return logged(session, "{name} is paralyzed.") > 0;
    };
    auto fight = attack_until(ghoul, victim(), paralysed);
    check(paralysed(*fight.session), "A ghoul's touch paralyses");
    auto elf = victim();
    elf.race = 2;
    auto spared = attack_until(ghoul, elf,
                               [](const rules::CombatSession &session)
                               {
                                   return session.snapshot().round > 5;
                               });
    check(!paralysed(*spared.session), "An elf is not paralysed");
}

// A stirge's hit fastens it to its victim, which cannot move; at each of its
// turns it drinks 1d4 until it has had 12 to 15, then lets go and flies off
// (effhndlr.pas, 75 and 76).
void a_stirge_drinks_its_fill()
{
    const auto stirge = monster({2, 1, 1, 0}, {0, 0, 0, 0}, {{76, 0, 0, false}});
    auto fight = attack_until(stirge, victim(),
                              [](const rules::CombatSession &session)
                              {
                                  return logged(session, "{name} is forced to flee.") > 0;
                              });
    auto &session = *fight.session;
    int drunk = 0;
    for (const auto &entry : session.snapshot().log_entries)
    {
        if (entry.message.source == "{attacker} drinks {name}'s blood for {damage}.")
        {
            drunk += std::stoi(entry.message.arguments[2].value);
        }
    }
    check(logged(session, "{name} is forced to flee.") == 1 && drunk >= 12 && drunk <= 15,
          "Full after 12 to 15, the stirge flees");
    until_turn_of(session, "Victim");
    check(offered(session, "move").has_value(), "Let go, its victim can move again");
}

// A holding bite fastens on a victim of Strength below 19 and bites it from
// behind with 3d4 at each of its turns (effhndlr.pas, 74 and 77).
void a_bite_holds()
{
    const auto spider = monster({2, 1, 1, 0}, {0, 0, 0, 0}, {{77, 0, 0, false}});
    const auto held = [](const rules::CombatSession &session)
    {
        return logged(session, "{name} is held fast.") > 0;
    };
    auto fight = attack_until(spider, victim(), held);
    auto &session = *fight.session;
    check(held(session), "Strength 10 is held fast");
    const int bites = logged(session, "{attacker} hits {name} for {damage}.", "Monster") +
                      logged(session, "{attacker} misses {name}.", "Monster");
    until_turn_of(session, "Victim");
    check(!offered(session, "move"), "Held, it cannot move");
    const auto swings = [&]
    {
        return logged(session, "{attacker} hits {name} for {damage}.", "Monster") +
               logged(session, "{attacker} misses {name}.", "Monster");
    };
    const auto round = session.snapshot().round;
    play_until(
        session,
        [&](const std::string &)
        {
            return order(session, "end");
        },
        [&]
        {
            return swings() > bites || session.snapshot().round > round + 2;
        });
    check(swings() > bites && actor_name(session) != "Monster",
          "The spider bites again at its turn, unbidden");
    auto strong = victim();
    strong.strength = 19;
    auto free = attack_until(spider, strong,
                             [](const rules::CombatSession &s)
                             {
                                 return s.snapshot().round > 5;
                             });
    check(!held(*free.session), "Strength 19 is not held");
}

// Two claws that both hit bring two rakes of 2d4 (cbtacts.pas, RearClawRake).
void both_claws_rake()
{
    const auto cat = monster({4, 1, 2, 0}, {2, 1, 6, 0}, {{73, 0, 0, false}});
    auto fight = attack_until(cat, victim(),
                              [](const rules::CombatSession &session)
                              {
                                  return logged(session, "{attacker} rakes {name}.") > 0;
                              });
    check(logged(*fight.session, "{attacker} rakes {name}.") > 0, "Both claws hit: it rakes");
}

// A mummy's touch rots: the victim can no longer be healed (effhndlr.pas, 87).
void a_mummy_rots()
{
    const auto mummy = monster({2, 1, 1, 0}, {0, 0, 0, 0}, {{87, 0, 0, false}});
    auto fight = attack_until(mummy, victim(),
                              [](const rules::CombatSession &session)
                              {
                                  return logged(session, "{name} is diseased.") > 0;
                              });
    check(logged(*fight.session, "{name} is diseased.") == 1, "Mummy rot, once");
}

// The victim's numbers as the fight leaves them (CombatantView::persistent).
original::Combatant left_by_fight(const rules::CombatSession &session, std::string_view name)
{
    const auto numbers =
        original::Combatant::decode(view_of(session.snapshot(), name).persistent.resources);
    check(numbers.has_value(), std::string(name) + "'s numbers outlast the fight");
    return *numbers;
}

// A wight's touch drains a level, no save: the Hit Points of a level, and the
// class whose level took the most experience, its THAC0 and saves following;
// below level 2 the victim is dead forever (effhndlr.pas, EffDrainOne).
void native_drains_keep_ordered_exact_receipts()
{
    using original::ClassSlot;
    constexpr auto native = GoldBoxGame::dark_queen_of_krynn;
    auto person = victim(50);
    person.class_levels = {0, 0, 8, 0, 0, 8, 8, 0};
    person.level = 8;
    person.constitution = 16;
    person.intelligence = 18;
    person.native_progression = original::NativeProgression{};
    auto &progression = *person.native_progression;
    progression.profession = static_cast<int>(original::Profession::fighter_magic_user_thief);
    progression.race = static_cast<int>(original::Race::silvanesti_elf);
    progression.robe_order = 1;
    progression.wisdom = 12;
    progression.rolled_hit_points = 40;
    progression.experience = 300000;
    const auto drain = [&](bool two)
    {
        return attack_until(
            monster({2, 1, 1, 0}, {0, 0, 0, 0}, {{two ? 86 : 85, 0, 0, false}}), person,
            [two](const rules::CombatSession &fight)
            {
                return logged(fight, two ? "{name} is drained 2 levels."
                                         : "{name} is drained a level.") > 0;
            },
            47, native);
    };
    auto one = drain(false);
    const auto after = left_by_fight(*one.session, "Victim");
    check(after.native_progression && after.native_progression->drains.size() == 1,
          "A real native drain retains one exact receipt");
    const auto &receipt = after.native_progression->drains.front();
    const unsigned lower =
        static_cast<unsigned>(original::experience_for_level(ClassSlot::thief, 7, native).value());
    const unsigned upper =
        static_cast<unsigned>(original::experience_for_level(ClassSlot::thief, 8, native).value());
    check(receipt.slot == static_cast<unsigned>(ClassSlot::thief) && receipt.before_level == 8 &&
              after.class_levels[6] == 7 && after.class_levels[2] == 8 &&
              after.class_levels[5] == 8 && receipt.experience == lower + (upper - lower) / 2 &&
              receipt.previous_experience == 300000 &&
              after.native_progression->rolled_hit_points == 40 - receipt.rolled_hit_points &&
              after.max_hit_points == 50 - receipt.hit_points &&
              view_of(one.session->snapshot(), "Victim").hit_points == after.max_hit_points - 1,
          "Native equal-level drain chooses the smaller threshold, preserves "
          "damage and exact raw HP");
    const auto saved = one.session->save();
    check(one.module->restore(saved)->save() == saved,
          "A native combat checkpoint retains its exact drain receipt byte for "
          "byte");
    auto two = drain(true);
    const auto twice = left_by_fight(*two.session, "Victim");
    check(twice.native_progression && twice.native_progression->drains.size() == 2 &&
              twice.native_progression->drains[0].slot == static_cast<unsigned>(ClassSlot::thief) &&
              twice.native_progression->drains[1].slot ==
                  static_cast<unsigned>(ClassSlot::fighter) &&
              twice.native_progression->previous_experience == 300000,
          "A two-level native drain rolls and retains each class loss in its "
          "actual order");
}

void a_wight_drains_levels()
{
    const auto wight = monster({2, 1, 1, 0}, {0, 0, 0, 0}, {{85, 0, 0, false}});
    const auto drained = [](const rules::CombatSession &session)
    {
        return logged(session, "{name} is drained a level.") > 0;
    };
    const auto character = [](std::array<int, 8> levels, int hit_points)
    {
        auto c = victim(hit_points);
        c.class_levels = levels;
        c.level = *std::max_element(levels.begin(), levels.end());
        c.thac0 = original::thac0(levels, opengold::GoldBoxGame::pool_of_radiance);
        c.saving_throws = original::saving_throws(levels, opengold::GoldBoxGame::pool_of_radiance);
        return c;
    };
    using original::ClassSlot;
    std::array<int, 8> fighter_3{};
    fighter_3[static_cast<std::size_t>(ClassSlot::fighter)] = 3;
    auto fight = attack_until(wight, character(fighter_3, 30), drained);
    auto fighter_2 = fighter_3;
    fighter_2[static_cast<std::size_t>(ClassSlot::fighter)] = 2;
    const auto after = left_by_fight(*fight.session, "Victim");
    check(after.class_levels == fighter_2 && after.level == 2 && after.max_hit_points == 20 &&
              after.thac0 == original::thac0(fighter_2, opengold::GoldBoxGame::pool_of_radiance) &&
              after.saving_throws ==
                  original::saving_throws(fighter_2, opengold::GoldBoxGame::pool_of_radiance),
          "A level-3 fighter of 30 Hit Points is left a level-2 fighter of 20");
    check(view_of(fight.session->snapshot(), "Victim").hit_points == 30 - 1 - 10,
          "Its own Hit Points lose the level's 10 after the touch's 1");

    std::array<int, 8> fighter_1{};
    fighter_1[static_cast<std::size_t>(ClassSlot::fighter)] = 1;
    auto last = attack_until(wight, character(fighter_1, 10), drained);
    const auto &gone = view_of(last.session->snapshot(), "Victim");
    check(gone.dead && gone.status == "gone", "Drained below level 2, it is dead forever");

    std::array<int, 8> multi{};
    multi[static_cast<std::size_t>(ClassSlot::fighter)] = 2;
    multi[static_cast<std::size_t>(ClassSlot::thief)] = 3;
    const bool thief_cost_more =
        original::experience_for_level(ClassSlot::thief, 3, opengold::GoldBoxGame::pool_of_radiance)
            .value_or(0) >= original::experience_for_level(ClassSlot::fighter, 2,
                                                           opengold::GoldBoxGame::pool_of_radiance)
                                .value_or(0);
    auto expected = multi;
    --expected[static_cast<std::size_t>(thief_cost_more ? ClassSlot::thief : ClassSlot::fighter)];
    auto split = attack_until(wight, character(multi, 30), drained);
    check(left_by_fight(*split.session, "Victim").class_levels == expected,
          "A fighter/thief loses the level that took the most experience");
}

// The item a creature's blows land with (charhlp.pas, FindDamagingRangedWeapon):
// a bow's silver arrows, a thrown dagger itself, none bare-handed; and a
// readied mirror wards off a gaze.
void strikers_come_from_the_gear()
{
    namespace bit = por::item_property;
    const auto record = kobold_record();
    por::ItemTemplate bow_type;
    bow_type.range = 8;
    bow_type.properties = bit::fires_arrows | bit::dexterity_missile | bit::fires_ammunition;
    por::ItemRecord bow;
    bow.type = 42;
    bow.readied_raw = 1;
    por::ItemTemplate arrow_type;
    arrow_type.worn_location = 10;
    arrow_type.damage_type = 0x01;
    por::ItemRecord arrows;
    arrows.type = 73;
    arrows.readied_raw = 1;
    arrows.magic_bonus = 1;
    arrows.name_components = {0, 0, 177};
    por::ItemTemplate mirror_type;
    mirror_type.worn_location = 13;
    por::ItemRecord mirror;
    mirror.readied_raw = 1;
    mirror.name_components = {118, 0, 0};
    const std::vector<original::CarriedItem> archer{
        {bow, bow_type}, {arrows, arrow_type}, {mirror, mirror_type}};
    const auto shooting = original::combatant_from_record(record, archer);
    check(shooting.striker == original::StrikingItem{1, true, 0x01} && shooting.weapon_type == 42,
          "A bow strikes with its silver arrows +1, which pierce");
    check(shooting.wards_gaze, "A readied mirror wards off a gaze");
    check(original::Combatant::decode(shooting.encode()) == shooting,
          "And it all survives encoding");
    const auto bare = original::combatant_from_record(record);
    check(!bare.striker && bare.weapon_type == -1 && !bare.wards_gaze,
          "Bare-handed, nothing strikes");
}

// The party's attacker strikes the target at each of its turns until the
// condition holds or 30 rounds pass; a far-off ogre keeps the fight going.
Fight strike_until(const original::Combatant &attacker, const original::Combatant &target,
                   const std::function<bool(const rules::CombatSession &)> &done,
                   std::uint64_t seed = 53, GoldBoxGame game = GoldBoxGame::pool_of_radiance)
{
    auto ogre = creature(1000, 0);
    ogre.movement = 0;
    auto fight = arrange({{"Attacker", rules::Side::party, {3, 3}, attacker},
                          {"Target", rules::Side::opposition, {4, 3}, target},
                          {"Ogre", rules::Side::opposition, {7, 7}, ogre}},
                         seed, {}, 100, game);
    auto &session = *fight.session;
    play_until(
        session,
        [&](const std::string &actor)
        {
            if (actor == "Attacker")
            {
                if (const auto melee = offered(session, "melee", "Target"))
                {
                    return *melee;
                }
            }
            return order(session, "end");
        },
        [&]
        {
            return done(session) || session.snapshot().round > 30;
        },
        600);
    return fight;
}

// The damage of the attacker's first hit on the target.
int first_hit(const original::Combatant &attacker, const original::Combatant &target,
              GoldBoxGame game = GoldBoxGame::pool_of_radiance)
{
    auto fight = strike_until(
        attacker, target,
        [](const rules::CombatSession &session)
        {
            return logged(session, "{attacker} hits {name} for {damage}.", "Attacker") > 0;
        },
        53, game);
    for (const auto &entry : fight.session->snapshot().log_entries)
    {
        if (entry.message.source == "{attacker} hits {name} for {damage}.")
        {
            return std::stoi(entry.message.arguments[2].value);
        }
    }
    return -1;
}

// What a target's defenses make of 10 damage (effhndlr.pas, WEAPON_DAMAGE_DEFENDER).
void defenses_weigh_the_weapon()
{
    const auto hitter = [](std::optional<original::StrikingItem> striker, int race, int level)
    {
        auto c = monster({2, 1, 1, 9}, {0, 0, 0, 0}, {});
        c.striker = striker;
        c.race = race;
        c.level = level;
        return c;
    };
    const auto defended = [](int code)
    {
        auto c = creature(1000, 0);
        c.effects.push_back({code, 0, 0, false});
        return c;
    };
    const original::StrikingItem mundane{0, false, 0}, magic{1, false, 0}, silver{0, true, 0},
        mace{0, false, 0x80};
    check(first_hit(hitter(mundane, 7, 5), defended(119)) == 0,
          "A character's mundane sword cannot harm what needs magic");
    check(first_hit(hitter(magic, 7, 5), defended(119)) == 10, "A magic one can");
    check(first_hit(hitter(std::nullopt, 0, 4), defended(119)) == 10,
          "Nor can a monster below level 4; one of level 4 can, bare-clawed");
    check(first_hit(hitter(silver, 7, 1), defended(123)) == 5 &&
              first_hit(hitter(mundane, 7, 1), defended(123)) == 0,
          "A wraith takes half from silver, nothing from iron");
    check(first_hit(hitter(silver, 7, 1), defended(96)) == 10 &&
              first_hit(hitter(std::nullopt, 0, 9), defended(96)) == 0,
          "A werecreature takes silver whole, claws not at all");
    check(first_hit(hitter(mace, 7, 1), defended(94)) == 5 &&
              first_hit(hitter(mundane, 7, 1), defended(94)) == 10,
          "Half from a mace, all from a sword (94)");
    check(first_hit(hitter(mundane, 7, 1), defended(115)) == 5 &&
              first_hit(hitter(mace, 7, 1), defended(115)) == 10,
          "Half from a sword, all from a mace (115)");
    check(first_hit(hitter(magic, 7, 1), defended(116)) == 5, "Half from magic (116)");
    auto vial = hitter(mundane, 7, 1);
    vial.weapon_type = 85;
    const int holy = first_hit(vial, defended(117));
    check(holy >= 2 && holy <= 7, "Holy water burns the undead for 1d6+1");
}

// A troll starts regenerating 3 rounds after its first wound, 3 a round, and
// downed by a weapon rises again with all its Hit Points 3d6 rounds later
// (effhndlr.pas, 98 to 102).
void a_troll_rises_again()
{
    auto troll = creature(30, 6);
    troll.effects = {{100, 0, 0, false}, {101, 0, 0, false}};
    auto hitter = monster({2, 1, 1, 0}, {0, 0, 0, 0}, {});
    auto fight = strike_until(hitter, troll,
                              [](const rules::CombatSession &session)
                              {
                                  return session.snapshot().round > 6;
                              });
    // One point a hit, regenerating 3 a round from the 3rd round after the first.
    const auto &healing = view_of(fight.session->snapshot(), "Target");
    check(healing.hit_points == 30, "Its wounds close as fast as one sword opens them");
    auto killer = monster({2, 1, 1, 40}, {0, 0, 0, 0}, {});
    auto again = strike_until(killer, troll,
                              [](const rules::CombatSession &session)
                              {
                                  return logged(session, "{name} rises again.") > 0;
                              });
    check(logged(*again.session, "{name} rises again.") > 0 &&
              view_of(again.session->snapshot(), "Target").hit_points == 30,
          "Downed by a sword, it rises again whole");
}

// One that fights while down stands again with 6 less its wounds, and falls
// dead 1d4+1 rounds later (effhndlr.pas, 95 and 99).
void fighting_on_while_down()
{
    auto berserker = creature(5, 1);
    berserker.effects = {{99, 0, 0, false}};
    auto hitter = monster({2, 1, 1, 6}, {0, 0, 0, 0}, {});
    auto fight = strike_until(hitter, berserker,
                              [](const rules::CombatSession &session)
                              {
                                  return logged(session, "{name} falls dead.") > 0 ||
                                         logged(session, "{name} is killed.") > 0;
                              });
    auto &session = *fight.session;
    check(logged(session, "{name} gets back up.") == 1, "Downed, it gets back up");
    check(view_of(session.snapshot(), "Target").dead, "And soon after it is dead");
}

// Displacement turns aside the first attack each round (effhndlr.pas, EffDisplaced).
void displacement_turns_the_first_attack()
{
    auto displacer = creature(1000, 0);
    displacer.effects = {{89, 0, 0, false}};
    auto hitter = monster({4, 1, 1, 0}, {0, 0, 0, 0}, {});
    auto fight = strike_until(hitter, displacer,
                              [](const rules::CombatSession &session)
                              {
                                  return session.snapshot().round > 8;
                              });
    auto &session = *fight.session;
    std::vector<std::string> swings;
    for (const auto &entry : session.snapshot().log_entries)
    {
        if (entry.message.source == "{attacker} misses {name}." ||
            entry.message.source == "{attacker} hits {name} for {damage}.")
        {
            swings.push_back(entry.message.source);
        }
    }
    bool first_missed = true;
    for (std::size_t n = 0; n + 1 < swings.size(); n += 2)
    {
        first_missed = first_missed && swings[n] == "{attacker} misses {name}.";
    }
    check(swings.size() >= 8 && first_missed, "Each round's first swing misses");
}

// An invisible creature cannot be singled out but by one that sees the
// invisible, nor one warded by a mirror by a gazing monster (cbtacts.pas, CanTarget).
void some_cannot_be_targeted()
{
    const auto can_strike =
        [](const original::Combatant &attacker, const original::Combatant &target)
    {
        auto fight = arrange({{"Attacker", rules::Side::party, {3, 3}, attacker},
                              {"Target", rules::Side::opposition, {4, 3}, target}},
                             59);
        until_turn_of(*fight.session, "Attacker");
        return offered(*fight.session, "melee", "Target").has_value();
    };
    auto hidden = creature(50, 0);
    hidden.effects = {{25, 0, 12, false}};
    auto seer = creature(50);
    seer.effects = {{24, 0, 0, false}};
    check(!can_strike(creature(50), hidden) && can_strike(seer, hidden),
          "Only one that sees the invisible can strike an invisible creature");
    auto basilisk = creature(50);
    basilisk.effects = {{126, 0, 0, false}};
    auto warded = creature(50, 0);
    warded.wards_gaze = true;
    check(!can_strike(basilisk, warded) && can_strike(basilisk, creature(50, 0)),
          "A gazing monster cannot single out one holding up a mirror");
}

// A mummy's fear paralyses its enemies for 1d4 rounds as the fight starts,
// unless saved against or immune to paralysis (effhndlr.pas, EffMummyFear).
void a_mummy_frightens()
{
    auto mummy = creature(50, 0);
    mummy.effects = {{82, 0, 0, false}};
    auto brave = victim();
    brave.effects = {{109, 0, 0, false}};
    auto fight = arrange({{"Coward", rules::Side::party, {0, 1}, victim()},
                          {"Brave", rules::Side::party, {0, 3}, brave},
                          {"Mummy", rules::Side::opposition, {7, 3}, mummy}},
                         61);
    auto &session = *fight.session;
    check(view_of(session.snapshot(), "Coward").status == "helpless" &&
              view_of(session.snapshot(), "Brave").status.empty(),
          "The coward is paralysed with fear, the immune one is not");
}

// A cleric of this level.
original::Combatant cleric(int level)
{
    auto c = creature(30, level);
    c.class_levels[static_cast<std::size_t>(original::ClassSlot::cleric)] = level;
    return c;
}

original::Combatant undead(int undead_level)
{
    auto c = creature(30, 0);
    c.undead_level = undead_level;
    c.morale = 0x80 + 50;
    return c;
}

// Turning undead, once a fight (cbtacts.pas, CombatTurnUndead): a level-8
// cleric destroys every skeleton it sees, a level-1 one cannot turn a vampire,
// and a turned skeleton is forced to flee.
void clerics_turn_undead()
{
    auto fight = arrange({{"Cleric", rules::Side::party, {1, 3}, cleric(8)},
                          {"Skeleton 1", rules::Side::opposition, {5, 2}, undead(1)},
                          {"Skeleton 2", rules::Side::opposition, {5, 4}, undead(1)},
                          {"Skeleton 3", rules::Side::opposition, {6, 3}, undead(1)},
                          {"Hidden", rules::Side::opposition, {7, 7}, undead(1)}},
                         67, {{6, 6}, {6, 7}, {7, 6}});
    auto &session = *fight.session;
    until_turn_of(session, "Cleric");
    session.submit(order(session, "turn"));
    const auto state = session.snapshot();
    check(view_of(state, "Skeleton 1").status == "gone" &&
              view_of(state, "Skeleton 2").status == "gone" &&
              view_of(state, "Skeleton 3").status == "gone",
          "A level-8 cleric destroys every skeleton in sight");
    check(view_of(state, "Hidden").status.empty(), "Not one behind a wall");
    until_turn_of(session, "Cleric");
    check(!offered(session, "turn"), "Turning is once a fight");

    auto vampire = arrange({{"Cleric", rules::Side::party, {1, 3}, cleric(1)},
                            {"Vampire", rules::Side::opposition, {5, 3}, undead(10)}},
                           67);
    until_turn_of(*vampire.session, "Cleric");
    vampire.session->submit(order(*vampire.session, "turn"));
    check(logged(*vampire.session, "Nothing happens.") == 1,
          "A level-1 cleric cannot turn a vampire");

    auto turned = arrange({{"Cleric", rules::Side::party, {1, 3}, cleric(4)},
                           {"Skeleton", rules::Side::opposition, {5, 3}, undead(1)}},
                          67);
    auto &rout = *turned.session;
    until_turn_of(rout, "Cleric");
    rout.submit(order(rout, "turn"));
    play_until(
        rout,
        [&](const std::string &)
        {
            return order(rout, "end");
        },
        [&]
        {
            return view_of(rout.snapshot(), "Skeleton").panicked;
        });
    check(logged(rout, "{name} is turned.") == 1 && logged(rout, "{name} is forced to flee.") == 1,
          "A turned skeleton is forced to flee");
}

// Treasures' turning (its cbtacts.pas, CombatTurnUndead; combat.pas,
// CombatMenu): a paladin past level 2 turns too, at its level less 2; a
// turner may try as many times a fight as the undead it began with; and the
// strongest undead in sight is tried first, a failed roll going on to the
// weaker.
void treasures_turns_its_own_way()
{
    constexpr auto treasures = GoldBoxGame::treasures_of_the_savage_frontier;
    auto paladin = creature(30, 5);
    paladin.class_levels[static_cast<std::size_t>(original::ClassSlot::paladin)] = 5;
    auto low_paladin = creature(30, 2);
    low_paladin.class_levels[static_cast<std::size_t>(original::ClassSlot::paladin)] = 2;
    auto offers = arrange({{"Paladin", rules::Side::party, {1, 3}, paladin},
                           {"Squire", rules::Side::party, {1, 5}, low_paladin},
                           {"Zombie", rules::Side::opposition, {5, 3}, undead(2)},
                           {"Ghoul", rules::Side::opposition, {5, 5}, undead(3)}},
                          67, {}, 100, treasures);
    auto &session = *offers.session;
    until_turn_of(session, "Squire");
    check(!offered(session, "turn"), "A paladin of level 2 cannot turn");
    until_turn_of(session, "Paladin");
    check(offered(session, "turn").has_value(), "A paladin of level 5 turns, at level 3");
    session.submit(order(session, "turn"));
    until_turn_of(session, "Paladin");
    check(offered(session, "turn").has_value(), "Two undead began the fight: it may turn again");
    session.submit(order(session, "turn"));
    until_turn_of(session, "Paladin");
    check(!offered(session, "turn"), "Twice is all");

    // A level-1 cleric cannot turn the vampire, tried first, so its roll goes
    // on to the skeleton behind it.
    const auto skeleton_turned = [](GoldBoxGame game)
    {
        auto fight = arrange({{"Cleric", rules::Side::party, {1, 3}, cleric(1)},
                              {"Vampire", rules::Side::opposition, {3, 3}, undead(10)},
                              {"Skeleton", rules::Side::opposition, {6, 3}, undead(1)}},
                             67, {}, 100, game);
        auto &turning = *fight.session;
        for (int attempt = 0; attempt < 6; ++attempt)
        {
            until_turn_of(turning, "Cleric");
            if (!offered(turning, "turn"))
            {
                break;
            }
            turning.submit(order(turning, "turn"));
            if (logged(turning, "{name} is turned.") > 0)
            {
                return true;
            }
        }
        return false;
    };
    check(skeleton_turned(treasures),
          "Treasures' cleric fails at the vampire and turns the skeleton");
}

// A caster of this class and level with these spells memorized.
original::Combatant caster(original::ClassSlot slot, int level, std::vector<int> spells)
{
    auto c = creature(30, level);
    c.class_levels[static_cast<std::size_t>(slot)] = level;
    c.spells = std::move(spells);
    return c;
}

std::optional<rules::Command> cast_command(const rules::CombatSession &session, int spell)
{
    for (const auto &c : session.legal_commands())
    {
        if (c.verb == "cast" && c.item == static_cast<unsigned>(spell))
        {
            return c;
        }
    }
    return std::nullopt;
}

std::optional<rules::Command> aim_at(const rules::CombatSession &session, std::string_view name)
{
    return offered(session, "aim", name);
}

std::optional<rules::Command> aim_at_cell(const rules::CombatSession &session, rules::Cell cell)
{
    return offered(session, "aim", {}, cell);
}

// An Unlimited Adventures design's "no magic" fight: no one casts, its spells
// kept, and the bar lasts through a saved fight (rules::Encounter::no_magic).
void no_one_casts_in_a_no_magic_fight()
{
    using original::ClassSlot;
    const std::vector<Placed> placed{
        {"Cleric", rules::Side::party, {1, 3}, caster(ClassSlot::cleric, 3, {1})},
        {"Kobold", rules::Side::opposition, {3, 1}, creature(20, 0)}};
    auto fight = arrange(placed, 71, {}, 100, GoldBoxGame::unlimited_adventures, false, true);
    auto &session = *fight.session;
    until_turn_of(session, "Cleric");
    check(!cast_command(session, 1), "No one casts in a no-magic fight");
    auto restored = fight.module->restore(session.save());
    check(!cast_command(*restored, 1), "The bar lasts through a saved fight");
    auto open = arrange(placed, 71, {}, 100, GoldBoxGame::unlimited_adventures);
    until_turn_of(*open.session, "Cleric");
    check(cast_command(*open.session, 1).has_value(), "Another fight lets the cleric cast");
}

// Bless takes 3 off the caster's initiative and goes off later in the round,
// aimed then: the caster's side within 2 of the aim, but not one with an enemy
// next to it (magic.pas; combat.pas, CastSpellCombat).
void bless_waits_for_its_turn()
{
    using original::ClassSlot;
    auto fight = arrange({{"Cleric", rules::Side::party, {1, 3}, caster(ClassSlot::cleric, 3, {1})},
                          {"Ally", rules::Side::party, {1, 4}, creature(20)},
                          {"Engaged", rules::Side::party, {2, 2}, creature(20)},
                          {"Kobold", rules::Side::opposition, {3, 1}, creature(20, 0)}},
                         71);
    auto &session = *fight.session;
    until_turn_of(session, "Cleric");
    const auto round = session.snapshot().round;
    check(session.submit(*cast_command(session, 1)), "The cleric casts Bless");
    check(logged(session, "{name} begins casting {spell}.") == 1 && actor_name(session) != "Cleric",
          "It begins casting, and its turn passes");
    play_until(
        session,
        [&](const std::string &)
        {
            return order(session, "end");
        },
        [&]
        {
            return actor_name(session) == "Cleric";
        });
    check(session.snapshot().round == round && aim_at_cell(session, {1, 3}).has_value(),
          "Later in the round it aims Bless");
    session.submit(*aim_at_cell(session, {1, 3}));
    const auto blessed = [&](std::string_view name)
    {
        const auto numbers = left_by_fight(session, name);
        return std::any_of(numbers.effects.begin(), numbers.effects.end(),
                           [](const original::Effect &e)
                           {
                               return e.code == 1;
                           });
    };
    check(blessed("Cleric") && blessed("Ally") && !blessed("Engaged") && !blessed("Kobold"),
          "Its side is blessed, but not one beside an enemy, nor the enemy");
    check(left_by_fight(session, "Cleric").spells.empty(), "The memorized Bless is spent");
}

// A spell being cast is lost to damage before it goes off, and its memorized
// copy with it (cbtacts.pas, ProcessAttackResult).
void damage_loses_a_spell()
{
    using original::ClassSlot;
    int lost = 0;
    for (std::uint64_t seed = 1; seed <= 30 && lost == 0; ++seed)
    {
        auto fight = arrange(
            {{"Cleric", rules::Side::party, {1, 3}, caster(ClassSlot::cleric, 3, {2})},
             {"Ogre", rules::Side::opposition, {2, 3}, monster({2, 1, 1, 3}, {0, 0, 0, 0}, {})}},
            seed);
        auto &session = *fight.session;
        play_until(
            session,
            [&](const std::string &actor)
            {
                if (actor == "Cleric")
                {
                    if (const auto cast = cast_command(session, 2))
                    {
                        return *cast;
                    }
                    if (const auto aim = aim_at_cell(session, {2, 3}))
                    {
                        return *aim;
                    }
                }
                if (actor == "Ogre")
                {
                    if (const auto melee = offered(session, "melee", "Cleric"))
                    {
                        return *melee;
                    }
                }
                return order(session, "end");
            },
            [&]
            {
                return session.snapshot().round > 1;
            });
        if (logged(session, "{name} loses a spell.") > 0)
        {
            ++lost;
            check(left_by_fight(session, "Cleric").spells.empty() &&
                      logged(session, "{name} casts {spell}.") == 0,
                  "The lost Curse is gone from memory and never cast");
        }
    }
    check(lost > 0, "An ogre's blow between beginning and casting loses the spell");
}

// Magic Missile: a missile of 1d4+1 at level 1, magic damage that Shield stops
// (magic.pas; effhndlr.pas, EffShield).
void magic_missiles()
{
    using original::ClassSlot;
    const auto missile = [](original::Combatant target)
    {
        auto fight =
            arrange({{"Mage", rules::Side::party, {1, 3}, caster(ClassSlot::magic_user, 1, {15})},
                     {"Target", rules::Side::opposition, {6, 3}, target}},
                    73);
        auto &session = *fight.session;
        until_turn_of(session, "Mage");
        session.submit(*cast_command(session, 15));
        session.submit(*aim_at(session, "Target"));
        for (const auto &entry : session.snapshot().log_entries)
        {
            if (entry.message.source == "{name} takes {damage}.")
            {
                return std::stoi(entry.message.arguments[1].value);
            }
        }
        return 0;
    };
    const int damage = missile(creature(50, 0));
    check(damage >= 2 && damage <= 5, "One missile of 1d4+1 at level 1");
    auto shielded = creature(50, 0);
    shielded.effects = {{17, 5, 1, false}};
    check(missile(shielded) == 0, "Shield stops it");
}

// Curse's caster levels (charhlp.pas, GetCasterLevel): a ranger casts a
// magic-user's spell at its level less 8, and one with no caster's levels
// at level 6.
void curse_caster_levels()
{
    using original::ClassSlot;
    const auto missile = [](const original::Combatant &mage, std::uint64_t seed)
    {
        auto fight = arrange({{"Mage", rules::Side::party, {1, 3}, mage},
                              {"Target", rules::Side::opposition, {6, 3}, creature(50, 0)}},
                             seed, {}, 100, GoldBoxGame::curse_of_the_azure_bonds);
        auto &session = *fight.session;
        until_turn_of(session, "Mage");
        session.submit(*cast_command(session, 15));
        session.submit(*aim_at(session, "Target"));
        return 50 - view_of(session.snapshot(), "Target").hit_points;
    };
    int ranger_most = 0, unlevelled_least = 100;
    for (std::uint64_t seed = 301; seed < 311; ++seed)
    {
        ranger_most = std::max(ranger_most, missile(caster(ClassSlot::ranger, 12, {15}), seed));
        auto monster = creature(30, 4);
        monster.spells = {15};
        unlevelled_least = std::min(unlevelled_least, missile(monster, seed));
    }
    check(ranger_most >= 4 && ranger_most <= 10, "A ranger of level 12 throws two missiles");
    check(unlevelled_least >= 6, "One without a caster's levels throws three");
}

// Sleep: 4d4 levels' worth within 1 of the aim, the nearest first; anything
// of level 6 or more never sleeps (magic.pas, the sleep).
void sleep_takes_the_weak()
{
    using original::ClassSlot;
    auto fight =
        arrange({{"Mage", rules::Side::party, {0, 3}, caster(ClassSlot::magic_user, 1, {21})},
                 {"Kobold 1", rules::Side::opposition, {4, 3}, creature(5, 0)},
                 {"Kobold 2", rules::Side::opposition, {5, 3}, creature(5, 0)},
                 {"Ogre", rules::Side::opposition, {4, 4}, creature(50, 6)}},
                79);
    auto &session = *fight.session;
    until_turn_of(session, "Mage");
    session.submit(*cast_command(session, 21));
    session.submit(*aim_at_cell(session, {4, 3}));
    const auto state = session.snapshot();
    check(view_of(state, "Kobold 1").status == "helpless" &&
              view_of(state, "Kobold 2").status == "helpless",
          "The kobolds fall asleep");
    check(view_of(state, "Ogre").status.empty(), "A level-6 ogre does not");
}

// Charm Person takes a creature over unless it saves (magic.pas, the charm).
void charm_person_turns_a_foe()
{
    using original::ClassSlot;
    auto foe = victim(20);
    foe.creature_type = 0;
    auto fight =
        arrange({{"Mage", rules::Side::party, {0, 3}, caster(ClassSlot::magic_user, 1, {10})},
                 {"Bandit", rules::Side::opposition, {4, 3}, foe}},
                83);
    auto &session = *fight.session;
    until_turn_of(session, "Mage");
    session.submit(*cast_command(session, 10));
    session.submit(*aim_at(session, "Bandit"));
    check(view_of(session.snapshot(), "Bandit").side == rules::Side::party,
          "The charmed bandit fights for the party");
}

// Cure Light Wounds reaches a downed member next to the cleric and heals it,
// though it stays down in the fight (magic.pas, the cure).
void cure_light_wounds_stops_bleeding()
{
    using original::ClassSlot;
    auto downed = creature(10);
    downed.hit_points = 0;
    auto fight = arrange({{"Cleric", rules::Side::party, {1, 3}, caster(ClassSlot::cleric, 1, {3})},
                          {"Wounded", rules::Side::party, {2, 3}, downed},
                          {"Kobold", rules::Side::opposition, {7, 7}, creature(5, 0)}},
                         89);
    auto &session = *fight.session;
    until_turn_of(session, "Cleric");
    session.submit(*cast_command(session, 3));
    play_until(
        session,
        [&](const std::string &)
        {
            return order(session, "end");
        },
        [&]
        {
            return actor_name(session) == "Cleric";
        });
    check(aim_at(session, "Wounded").has_value(), "Within a touch, the wounded can be healed");
    const auto checkpoint = session.save();
    const auto restored = fight.module->restore(checkpoint);
    check(restored->save() == checkpoint && aim_at(*restored, "Wounded").has_value(),
          "A fight saved while aiming goes on aiming");
    session.submit(*aim_at(session, "Wounded"));
    const auto &healed = view_of(session.snapshot(), "Wounded");
    check(logged(session, "{name} is healed.") == 1 && healed.status == "unconscious",
          "The cure lands, and the healed member stays down");
}

// The kinds of foe the races know, from a monster's record name (gdconsts.pas).
void races_know_their_foes()
{
    auto orc = kobold_record();
    orc.name = "ORC";
    orc.type = 1;
    orc.icon_size_raw = 1;
    auto giant = orc;
    giant.name = "HILL GIANT";
    giant.icon_size_raw = 2;
    check(original::combatant_from_record(orc).kin == original::kin::dwarf_foe &&
              original::combatant_from_record(giant).kin == original::kin::giant,
          "An orc is a dwarf's foe, a hill giant a giant");

    // A giant hitting on 11 or better strikes a gnome, 4 harder to hit, less often.
    const auto hits_on = [](int race)
    {
        auto target = creature(10000);
        target.race = race;
        if (race == 3)
        {
            target.effects = {{47, 0, 255, false}};
        }
        auto ogre = creature(10000, 6);
        ogre.thac0 = 21;
        ogre.kin = original::kin::giant;
        auto fight = arrange({{"Target", rules::Side::party, {3, 3}, target},
                              {"Ogre", rules::Side::opposition, {4, 3}, ogre}},
                             97);
        auto &session = *fight.session;
        play_until(
            session,
            [&](const std::string &actor)
            {
                if (actor == "Ogre")
                {
                    return order(session, "melee", "Target");
                }
                return order(session, "end");
            },
            [&]
            {
                return session.snapshot().round > 200;
            },
            1000);
        return logged(session, "{attacker} hits {name} for {damage}.", "Ogre");
    };
    const int human = hits_on(7), gnome = hits_on(3);
    check(gnome + 25 < human, "A giant hits a gnome far less often than a human");
}

// The caster casts the spell at once at the creature or square, from its turn.
void cast_now(rules::CombatSession &session, std::string_view caster_name, int spell,
              const std::function<std::optional<rules::Command>()> &aim)
{
    until_turn_of(session, caster_name);
    check(session.submit(*cast_command(session, spell)), "The spell is cast");
    play_until(
        session,
        [&](const std::string &)
        {
            return order(session, "end");
        },
        [&]
        {
            return actor_name(session) == caster_name;
        });
    const auto chosen = aim();
    check(chosen.has_value(), "Its aim is offered");
    session.submit(*chosen);
}

// Treasures' Charm Person frees a creature charmed already, with no save
// (its SpellCharmPerson). It takes a monster its flags call a person, of
// whatever type; a guard keeps the fight going.
void treasures_charm_person_frees_the_charmed()
{
    using original::ClassSlot;
    auto foe = victim(20);
    foe.creature_type = 5;
    foe.traits = original::trait::person;
    auto fight =
        arrange({{"Mage", rules::Side::party, {0, 3}, caster(ClassSlot::magic_user, 1, {10, 10})},
                 {"Bandit", rules::Side::opposition, {4, 3}, foe},
                 {"Guard", rules::Side::opposition, {7, 6}, victim(20)}},
                83, {}, 100, GoldBoxGame::treasures_of_the_savage_frontier);
    auto &session = *fight.session;
    const auto cast_at_bandit = [&]
    {
        until_turn_of(session, "Mage");
        check(session.submit(*cast_command(session, 10)), "Charm Person is cast");
        check(session.submit(*aim_at(session, "Bandit")), "It is aimed at the bandit");
    };
    cast_at_bandit();
    check(view_of(session.snapshot(), "Bandit").side == rules::Side::party,
          "The bandit is charmed");
    cast_at_bandit();
    check(view_of(session.snapshot(), "Bandit").side == rules::Side::opposition &&
              logged(session, "{name} is uncharmed.") == 1,
          "A second Charm Person frees it");
}

// Treasures' Hold Person holds any character, whatever its size, and never
// a monster, though its flags call it a person (its ApplyEffectToTargets).
void treasures_hold_person_holds_characters_only()
{
    using original::ClassSlot;
    auto tall = victim(20);
    tall.race = 7;
    tall.large = true;
    auto brute = victim(20);
    brute.traits = original::trait::person;
    auto fight =
        arrange({{"Cleric", rules::Side::party, {0, 3}, caster(ClassSlot::cleric, 3, {23, 23})},
                 {"Fighter", rules::Side::opposition, {4, 3}, tall},
                 {"Brute", rules::Side::opposition, {4, 5}, brute}},
                101, {}, 100, GoldBoxGame::treasures_of_the_savage_frontier);
    auto &session = *fight.session;
    for (const auto *target : {"Fighter", "Brute"})
    {
        cast_now(session, "Cleric", 23,
                 [&]
                 {
                     return aim_at(session, target);
                 });
        session.submit(order(session, "aim_done"));
    }
    check(view_of(session.snapshot(), "Fighter").status == "helpless",
          "Treasures' Hold Person holds a tall character");
    check(view_of(session.snapshot(), "Brute").status.empty(), "It does not hold a monster");
}

// Treasures' scripts hold creatures back (its LOAD MONSTER, cell 44h): one
// joins the fight as its round ends, where it would have stood or beside it
// (its JoinDelayedCombatants).
void treasures_creatures_join_late()
{
    auto fight = arrange({{"Guard", rules::Side::party, {0, 0}, victim(50)},
                          {"Early", rules::Side::opposition, {7, 7}, victim(50)},
                          {"Late", rules::Side::opposition, {6, 7}, victim(50)},
                          {"Blocker", rules::Side::opposition, {6, 6}, victim(50)}},
                         61, {}, 100, GoldBoxGame::treasures_of_the_savage_frontier, false, false,
                         [](rules::Encounter &encounter)
                         {
                             encounter.participants[2].arrival_round = 1;
                         });
    auto &session = *fight.session;
    check(view_of(session.snapshot(), "Late").fled, "The late one is not there at first");
    play_until(
        session,
        [&](const std::string &)
        {
            return order(session, "end");
        },
        [&]
        {
            return session.snapshot().round > 1;
        });
    const auto late = view_of(session.snapshot(), "Late");
    check(!late.fled && late.cell.x == 6 && late.cell.y == 7 &&
              logged(session, "{name} joins the battle.") == 1,
          "It joins the fight as the first round ends, where it would have stood");
}

// Treasures' own spell table: Charm Person lasts an hour and an hour a level
// and its AI weighs it 5 (its gdconsts.pas).
void treasures_spell_table()
{
    const auto *charm = original::spell(10, GoldBoxGame::treasures_of_the_savage_frontier);
    check(charm->minutes_fixed == 60 && charm->minutes_per_level == 60 && charm->ai_priority == 5,
          "Treasures' Charm Person lasts as its table says");
    check(original::spell(10, GoldBoxGame::pool_of_radiance)->minutes_fixed == 0,
          "Pool of Radiance's keeps its own");
}

// Hold Person holds a man but not a large creature (magic.pas, the hold).
void hold_person_holds()
{
    using original::ClassSlot;
    auto ogre = victim(50);
    ogre.large = true;
    auto fight =
        arrange({{"Cleric", rules::Side::party, {0, 3}, caster(ClassSlot::cleric, 3, {23, 23})},
                 {"Bandit", rules::Side::opposition, {4, 3}, victim(20)},
                 {"Ogre", rules::Side::opposition, {4, 5}, ogre}},
                101);
    auto &session = *fight.session;
    cast_now(session, "Cleric", 23,
             [&]
             {
                 return aim_at(session, "Bandit");
             });
    // Two more picks are offered; the cleric stops at one.
    session.submit(order(session, "aim_done"));
    check(view_of(session.snapshot(), "Bandit").status == "helpless", "The bandit is held");
    cast_now(session, "Cleric", 23,
             [&]
             {
                 return aim_at(session, "Ogre");
             });
    session.submit(order(session, "aim_done"));
    check(view_of(session.snapshot(), "Ogre").status.empty(), "The ogre is not");
}

// Fireball: one roll of the caster's level in d6 for all within 3 of the aim,
// the caster's own side too (magic.pas, the fireball).
void fireball_burns_all_around()
{
    using original::ClassSlot;
    auto fight =
        arrange({{"Mage", rules::Side::party, {0, 0}, caster(ClassSlot::magic_user, 5, {47})},
                 {"Friend", rules::Side::party, {5, 4}, victim(100)},
                 {"Foe", rules::Side::opposition, {6, 5}, victim(100)}},
                103);
    auto &session = *fight.session;
    cast_now(session, "Mage", 47,
             [&]
             {
                 return aim_at_cell(session, {6, 5});
             });
    const auto state = session.snapshot();
    const int friend_lost = 100 - view_of(state, "Friend").hit_points;
    const int foe_lost = 100 - view_of(state, "Foe").hit_points;
    check(friend_lost >= 5 && friend_lost <= 30 && friend_lost == foe_lost,
          "Friend and foe take the same 5d6, neither saving but on a 20");
}

// Haste doubles a creature's attacks (effhndlr.pas, EffHasted).
void haste_doubles_attacks()
{
    using original::ClassSlot;
    auto fighter = creature(50);
    fighter.thac0 = -20;
    auto fight =
        arrange({{"Mage", rules::Side::party, {0, 0}, caster(ClassSlot::magic_user, 5, {48})},
                 {"Fighter", rules::Side::party, {3, 3}, fighter},
                 {"Ogre", rules::Side::opposition, {4, 3}, creature(1000, 0)}},
                107);
    auto &session = *fight.session;
    cast_now(session, "Mage", 48,
             [&]
             {
                 return aim_at_cell(session, {2, 2});
             });
    const auto round = session.snapshot().round;
    play_until(
        session,
        [&](const std::string &)
        {
            return order(session, "end");
        },
        [&]
        {
            return session.snapshot().round > round && actor_name(session) == "Fighter";
        });
    const int before = logged(session, "{attacker} hits {name} for {damage}.", "Fighter");
    session.submit(order(session, "melee", "Ogre"));
    check(logged(session, "{attacker} hits {name} for {damage}.", "Fighter") - before == 2,
          "Hasted, the fighter strikes twice a round");
}

// Mirror images take blows (effhndlr.pas, EffMirrorImage).
void mirror_images_take_blows()
{
    por::CharacterRecord native;
    native.game = GoldBoxGame::dark_queen_of_krynn;
    por::EffectRecord packed;
    packed.code = 28;
    packed.duration_raw = 10;
    packed.data = 4 * 32 + 7;
    const auto decoded = original::combatant_from_record(native, {}, std::span{&packed, 1});
    const auto images = std::ranges::find(decoded.effects, 28, &original::Effect::code);
    check(images != decoded.effects.end() && images->strength == 4 * 32 + 7,
          "Dark Queen's Mirror Image retains its four images and level-seven caster");
    auto native_mage = victim(200);
    native_mage.effects = {{28, 100, 4 * 32 + 7, false}};
    auto native_fight = attack_until(
        monster({2, 1, 1, 0}, {0, 0, 0, 0}, {}), native_mage,
        [](const rules::CombatSession &session)
        {
            return logged(session, "{name} loses an image.") > 0;
        },
        47, GoldBoxGame::dark_queen_of_krynn);
    const auto remaining = left_by_fight(*native_fight.session, "Victim").effects;
    const auto native_images = std::ranges::find(remaining, 28, &original::Effect::code);
    check(native_images != remaining.end() && native_images->strength == 3 * 32 + 7,
          "Losing one native image leaves three images and the same caster level");
    const auto saved_native = native_fight.session->save();
    check(native_fight.module->restore(saved_native)->save() == saved_native,
          "A combat save preserves both native mirror-image fields");
    auto native_cast = arrange(
        {{"Mage", rules::Side::party, {0, 3}, caster(original::ClassSlot::magic_user, 17, {32})},
         {"Foe", rules::Side::opposition, {7, 7}, creature(100)}},
        47, {}, 100, GoldBoxGame::dark_queen_of_krynn);
    until_turn_of(*native_cast.session, "Mage");
    check(native_cast.session->submit(*cast_command(*native_cast.session, 32)),
          "The native mage casts Mirror Image");
    all_quick(*native_cast.session,
              [&]
              {
                  const auto effects = left_by_fight(*native_cast.session, "Mage").effects;
                  return std::ranges::find(effects, 28, &original::Effect::code) != effects.end();
              });
    const auto cast_effects = left_by_fight(*native_cast.session, "Mage").effects;
    const auto cast_images = std::ranges::find(cast_effects, 28, &original::Effect::code);
    check(cast_images != cast_effects.end() && (cast_images->strength & 31) == 17 &&
              (cast_images->strength >> 5) >= 1 && (cast_images->strength >> 5) <= 4,
          "Casting native Mirror Image packs its one to four images above caster level seventeen");
    auto mage = victim(50);
    mage.effects = {{28, 10, 4, false}};
    auto fight = attack_until(monster({4, 1, 1, 0}, {0, 0, 0, 0}, {}), mage,
                              [](const rules::CombatSession &session)
                              {
                                  return logged(session, "{name} loses an image.") > 0;
                              });
    check(logged(*fight.session, "{name} loses an image.") > 0, "A blow takes an image");
}

// Silence stops casting within a square of the silenced (effhndlr.pas, EffSilence).
void silence_stops_casting()
{
    using original::ClassSlot;
    auto quiet = creature(20);
    quiet.effects = {{21, 10, 1, false}};
    auto fight =
        arrange({{"Mage", rules::Side::party, {3, 3}, caster(ClassSlot::magic_user, 1, {15})},
                 {"Quiet", rules::Side::party, {3, 4}, quiet},
                 {"Kobold", rules::Side::opposition, {7, 7}, creature(5, 0)}},
                109);
    until_turn_of(*fight.session, "Mage");
    check(!cast_command(*fight.session, 15), "Beside the silenced, the mage cannot cast");
}

// Native case15 targets one creature or the occupants around empty ground
// (cbaction.c, combat_spell_targets); each occupant still saves and resists.
void native_silence_can_be_aimed_at_empty_ground()
{
    using original::ClassSlot;
    auto priest = caster(ClassSlot::cleric, 6, {25});
    priest.initiative_bonus = 100;
    auto carrier = caster(ClassSlot::magic_user, 1, {15});
    carrier.saving_throws.fill(100);
    auto saved = carrier;
    saved.saving_throws.fill(-20);
    auto resistant = carrier;
    resistant.magic_resistance = 200;
    const std::vector<Placed> placed{{"Priest", rules::Side::party, {1, 1}, priest},
                                     {"Carrier A", rules::Side::party, {3, 3}, carrier},
                                     {"Carrier B", rules::Side::party, {4, 4}, carrier},
                                     {"Saved", rules::Side::party, {4, 2}, saved},
                                     {"Resistant", rules::Side::opposition, {5, 3}, resistant},
                                     {"Outside", rules::Side::party, {6, 6}, carrier}};
    auto fight = arrange(placed, 123, {}, 100, GoldBoxGame::dark_queen_of_krynn);
    auto &session = *fight.session;
    until_turn_of(session, "Priest");
    check(session.submit(*cast_command(session, 25)), "The Native cleric starts actual Silence");
    until_turn_of(session, "Priest");
    const auto ground = aim_at_cell(session, {4, 3});
    check(ground && ground->target == 0,
          "Native Silence offers empty ground separately from a creature target");
    check(aim_at(session, "Carrier A").has_value(),
          "Native Silence keeps its ordinary single-creature target");
    const auto checkpoint = session.save();
    auto occupied_ground = *ground;
    occupied_ground.destination = {3, 3};
    check(
        !session.submit(occupied_ground) && session.save() == checkpoint,
        "A forged empty-ground target on an occupied square is refused without changing the checkpoint");
    bool malformed_refused = false;
    try
    {
        (void)fight.module->restore(checkpoint + " malformed");
    }
    catch (const std::runtime_error &)
    {
        malformed_refused = true;
    }
    check(malformed_refused, "A malformed pending-ground checkpoint remains refused");
    auto restored = fight.module->restore(checkpoint);
    check(restored->save() == checkpoint && aim_at_cell(*restored, {4, 3}).has_value(),
          "An ordinary checkpoint preserves Native ground aiming without new save fields");
    check(session.submit(*ground), "The cleric aims actual Silence at empty ground");
    check(restored->submit(*aim_at_cell(*restored, {4, 3})) && restored->save() == session.save(),
          "A restored Native ground spell continues with identical dice and effects");
    const auto has_silence = [&](std::string_view name)
    {
        const auto state = session.snapshot();
        const auto numbers = original::Combatant::decode(view_of(state, name).persistent.resources);
        return numbers && std::ranges::any_of(numbers->effects,
                                              [](const auto &effect)
                                              {
                                                  return effect.code == 21;
                                              });
    };
    check(has_silence("Carrier A") && has_silence("Carrier B") && !has_silence("Saved") &&
              !has_silence("Resistant") && !has_silence("Outside"),
          "Native ground Silence preserves saves/MR/radius: A=" +
              std::to_string(has_silence("Carrier A")) +
              " B=" + std::to_string(has_silence("Carrier B")) +
              " saved=" + std::to_string(has_silence("Saved")) +
              " resistant=" + std::to_string(has_silence("Resistant")) +
              " outside=" + std::to_string(has_silence("Outside")));
    until_turn_of(session, "Carrier A");
    check(!cast_command(session, 15), "A real recipient's Silence aura blocks its casting");

    auto other = arrange(placed, 123, {}, 100, GoldBoxGame::pool_of_radiance);
    until_turn_of(*other.session, "Priest");
    other.session->submit(*cast_command(*other.session, 25));
    until_turn_of(*other.session, "Priest");
    check(!aim_at_cell(*other.session, {4, 3}) && aim_at(*other.session, "Carrier A"),
          "Other released games retain their established creature-only targeting");
}

// Protection from Normal Missiles turns away a mundane arrow from afar
// (effhndlr.pas, EffProtNormalMissiles).
void missiles_turned_away()
{
    auto shot = archer(20);
    shot.thac0 = -20;
    shot.striker = original::StrikingItem{0, false, 1};
    shot.weapon_type = 42;
    auto warded = creature(100, 0);
    warded.effects = {{41, 10, 1, false}};
    auto fight = arrange({{"Archer", rules::Side::party, {0, 3}, shot},
                          {"Warded", rules::Side::opposition, {5, 3}, warded}},
                         113);
    auto &session = *fight.session;
    until_turn_of(session, "Archer");
    session.submit(*offered(session, "ranged", "Warded"));
    check(logged(session, "{name} avoids it.") > 0 &&
              view_of(session.snapshot(), "Warded").hit_points == 100,
          "The arrows do nothing");
}

// Lightning Bolt: one roll at the aim and along the bolt beyond it, away
// from the caster (magic.pas, SpellLightningBolt and LightningPath).
void lightning_runs_on()
{
    using original::ClassSlot;
    auto fight =
        arrange({{"Mage", rules::Side::party, {0, 3}, caster(ClassSlot::magic_user, 5, {51})},
                 {"First", rules::Side::opposition, {2, 3}, victim(100)},
                 {"Second", rules::Side::opposition, {5, 3}, victim(100)},
                 {"Aside", rules::Side::opposition, {5, 6}, victim(100)}},
                127);
    auto &session = *fight.session;
    cast_now(session, "Mage", 51,
             [&]
             {
                 return aim_at_cell(session, {2, 3});
             });
    const auto state = session.snapshot();
    const int first = 100 - view_of(state, "First").hit_points;
    const int second = 100 - view_of(state, "Second").hit_points;
    check(first >= 5 && first <= 30 && first == second, "Both in line take the same 5d6");
    check(view_of(state, "Aside").hit_points == 100, "One off the line is spared");
}

// Each game's Lightning Bolt (its magic.pas, SpellLightningBolt, and spell
// table): Pool of Radiance's bolt runs 8 squares, Treasures' 9 and the
// others' 7; it reaches 4 squares and 1 a level, from Secret on (and in the
// kit) 4 a level.
void each_game_bolts_its_own_length()
{
    using original::ClassSlot;
    const auto far_hit = [](GoldBoxGame game)
    {
        auto fight =
            arrange({{"Mage", rules::Side::party, {0, 3}, caster(ClassSlot::magic_user, 5, {51})},
                     {"Near", rules::Side::opposition, {1, 3}, victim(100)},
                     {"Far", rules::Side::opposition, {9, 3}, victim(100)}},
                    127, {}, 100, game, false, false,
                    [](rules::Encounter &encounter)
                    {
                        encounter.battlefield = {
                            16, 8, std::vector<rules::Terrain>(128, rules::Terrain::open)};
                    });
        auto &session = *fight.session;
        cast_now(session, "Mage", 51,
                 [&]
                 {
                     return aim_at_cell(session, {1, 3});
                 });
        return view_of(session.snapshot(), "Far").hit_points < 100;
    };
    check(far_hit(GoldBoxGame::pool_of_radiance) &&
              far_hit(GoldBoxGame::treasures_of_the_savage_frontier) &&
              !far_hit(GoldBoxGame::curse_of_the_azure_bonds) &&
              !far_hit(GoldBoxGame::gateway_to_the_savage_frontier),
          "Pool of Radiance's and Treasures' bolts reach 8 squares past the aim, Curse's and "
          "Gateway's do not");
    check(original::spell(51, GoldBoxGame::curse_of_the_azure_bonds)->range_per_level == 1 &&
              original::spell(51, GoldBoxGame::gateway_to_the_savage_frontier)->range_per_level ==
                  1 &&
              original::spell(51, GoldBoxGame::secret_of_the_silver_blades)->range_per_level == 4 &&
              original::spell(51, GoldBoxGame::pools_of_darkness)->range_per_level == 4 &&
              original::spell(51, GoldBoxGame::unlimited_adventures)->range_per_level == 4,
          "Lightning Bolt reaches 1 square a level to Gateway, 4 from Secret on");
}

// Treasures' bolt (its magic.pas, LightningPath) counts each step once: a
// leg costs what it walked, so creatures on its way do not shorten it. Pool
// of Radiance's takes the whole distance walked at each, and ends sooner.
void treasures_bolt_counts_each_step_once()
{
    using original::ClassSlot;
    const auto far_hit = [](GoldBoxGame game)
    {
        auto fight =
            arrange({{"Mage", rules::Side::party, {0, 3}, caster(ClassSlot::magic_user, 5, {51})},
                     {"Near", rules::Side::opposition, {1, 3}, victim(100)},
                     {"Second", rules::Side::opposition, {2, 3}, victim(100)},
                     {"Third", rules::Side::opposition, {3, 3}, victim(100)},
                     {"Fourth", rules::Side::opposition, {4, 3}, victim(100)},
                     {"Far", rules::Side::opposition, {9, 3}, victim(100)}},
                    127, {}, 100, game, false, false,
                    [](rules::Encounter &encounter)
                    {
                        encounter.battlefield = {
                            16, 8, std::vector<rules::Terrain>(128, rules::Terrain::open)};
                    });
        auto &session = *fight.session;
        cast_now(session, "Mage", 51,
                 [&]
                 {
                     return aim_at_cell(session, {1, 3});
                 });
        return view_of(session.snapshot(), "Far").hit_points < 100;
    };
    check(far_hit(GoldBoxGame::treasures_of_the_savage_frontier),
          "Treasures' bolt runs on past four creatures to one 8 squares beyond its aim");
    check(!far_hit(GoldBoxGame::pool_of_radiance),
          "Pool of Radiance's bolt, its length taken at each creature, ends before it");
}

// Treasures' bolt turns at a wall only indoors (its magic.pas, LightningPath:
// location 1, not overMap); out of doors it runs on through the wall. Turned,
// it strikes the creature it struck before again.
void treasures_bolt_turns_only_indoors()
{
    using original::ClassSlot;

    struct Struck
    {
        int aim_damage, beyond_damage;
    };

    const auto bolt = [](bool outdoors)
    {
        auto fight =
            arrange({{"Mage", rules::Side::party, {0, 3}, caster(ClassSlot::magic_user, 5, {51})},
                     {"Aim", rules::Side::opposition, {2, 3}, victim(100)},
                     {"Beyond", rules::Side::opposition, {6, 3}, victim(100)}},
                    127, {{4, 3}}, 100, GoldBoxGame::treasures_of_the_savage_frontier, outdoors);
        auto &session = *fight.session;
        cast_now(session, "Mage", 51,
                 [&]
                 {
                     return aim_at_cell(session, {2, 3});
                 });
        const auto state = session.snapshot();
        return Struck{100 - view_of(state, "Aim").hit_points,
                      100 - view_of(state, "Beyond").hit_points};
    };
    const auto inside = bolt(false), outside = bolt(true);
    check(inside.beyond_damage == 0 && outside.beyond_damage > 0,
          "Out of doors the bolt runs on through the wall; indoors it turns there");
    check(inside.aim_damage > outside.aim_damage,
          "Turned by the wall, it strikes its aim again on the way back");
}

// Treasures' romance in a fight: a member and an NPC a script has paired
// (romance.pas, TagRelationshipPair, by name and whatever its case), with
// these loves for each other.
Fight romance_fight(const std::vector<Placed> &placed, int member_love, int npc_love,
                    std::uint64_t seed = 31)
{
    return arrange(
        placed, seed, {}, 100, GoldBoxGame::treasures_of_the_savage_frontier, false, false,
        [=](rules::Encounter &encounter)
        {
            encounter.romance = rules::Romance{"HERO", "SIULAJIA", member_love, npc_love};
        });
}

// Plays the fight, `striker` meleeing `target` at each of its turns and the
// others ending theirs, until the fight ends or `done` holds.
void strike_until(
    rules::CombatSession &session, std::string_view striker, std::string_view target,
    const std::function<bool()> &done =
        []
    {
        return false;
    })
{
    play_until(
        session,
        [&](const std::string &actor)
        {
            if (actor == striker)
            {
                if (const auto melee = offered(session, "melee", target))
                {
                    return *melee;
                }
            }
            // A lover in a frenzy is offered only the computer's play.
            if (const auto quick = offered(session, "quick"); quick && !offered(session, "end"))
            {
                return *quick;
            }
            return order(session, "end");
        },
        done, 400);
}

// A foe that falls to a lover moves the other's love (effhndlr.pas,
// EffInLove, condition 25): 2 up for an evil foe, 20 down for a good one, to 0
// at least.
void a_lovers_kills_move_the_others_love()
{
    const auto love_after = [](std::string_view killer, int alignment, int member_love)
    {
        auto striker = creature(30);
        striker.thac0 = -20;
        auto foe = creature(1);
        foe.alignment = alignment;
        auto fight = romance_fight(
            {{"Hero", rules::Side::party, {3, 2}, killer == "Hero" ? striker : creature(30)},
             {"Siulajia",
              rules::Side::party,
              {3, 4},
              killer == "Siulajia" ? striker : creature(30)},
             {"Foe", rules::Side::opposition, {4, 3}, foe}},
            member_love, 40);
        strike_until(*fight.session, killer, "Foe");
        const auto romance = fight.session->snapshot().romance;
        check(romance.has_value(), "The fight keeps the romance");
        return std::pair{romance->member_love, romance->npc_love};
    };
    check(love_after("Siulajia", 2, 10) == std::pair{12, 40},
          "The NPC fells an evil foe: the member loves it 2 more");
    check(love_after("Siulajia", 0, 30) == std::pair{10, 40},
          "The NPC fells a good foe: the member loves it 20 less");
    check(love_after("Siulajia", 6, 10) == std::pair{0, 40}, "A love falls to 0 at least");
    check(love_after("Siulajia", 4, 10) == std::pair{10, 40}, "A neutral foe moves no love");
    check(love_after("Hero", 8, 10) == std::pair{10, 42},
          "The member fells an evil foe: the NPC loves it 2 more");
}

// A lover falls (EffInLove at ZERO_HIT_POINTS): the other, loving it 100 or
// more, goes into a frenzy, the computer playing it to the fight's end.
void a_fallen_lover_frenzies_the_other()
{
    const auto fight_for = [](int member_love)
    {
        auto foe = creature(1000);
        foe.thac0 = -20;
        auto fight = romance_fight({{"Hero", rules::Side::party, {1, 1}, creature(50)},
                                    {"Siulajia", rules::Side::party, {3, 3}, creature(1)},
                                    {"Foe", rules::Side::opposition, {4, 3}, foe}},
                                   member_love, 0);
        auto &session = *fight.session;
        strike_until(session, "Foe", "Siulajia",
                     [&]
                     {
                         return !view_of(session.snapshot(), "Siulajia").conscious;
                     });
        return fight;
    };
    auto loving = fight_for(100);
    auto &session = *loving.session;
    check(logged(session, "{name} goes into a frenzy.") == 1 &&
              view_of(session.snapshot(), "Hero").computer_plays,
          "The member who loves the fallen NPC 100 goes into a frenzy");
    until_turn_of(session, "Hero");
    const auto commands = session.legal_commands();
    check(commands.size() == 1 && commands.front().verb == "quick",
          "A lover in a frenzy fights under the computer's control");
    auto cooler = fight_for(99);
    check(logged(*cooler.session, "{name} goes into a frenzy.") == 0 &&
              !view_of(cooler.session->snapshot(), "Hero").computer_plays,
          "One who loves it 99 goes into no frenzy");
}

// Love on attack rolls (EffInLove at ATTACKER_ATTACK_ROLL and the
// defender's): a lover in a frenzy strikes 2 better, a refused one 2 worse,
// and either is struck 2 better; one loving and loved 100 strikes 1 better,
// one whose beloved is not in the fight 1 worse. Counted over many blows.
void love_moves_attack_rolls()
{
    using Love = std::optional<int>;
    const auto hits =
        [](Love attacker_love, Love defender_love, std::optional<rules::Romance> romance)
    {
        auto attacker = creature(1000);
        attacker.attacks[0] = {40, 1, 1, 0};
        attacker.thac0 = 11;
        if (attacker_love)
        {
            attacker.effects.push_back({1331, 0, *attacker_love, false});
        }
        auto target = creature(1000000);
        target.armor_class = 0;
        if (defender_love)
        {
            target.effects.push_back({1331, 0, *defender_love, false});
        }
        std::vector<Placed> placed{{"Hero", rules::Side::party, {3, 3}, attacker},
                                   {"Foe", rules::Side::opposition, {4, 3}, target}};
        if (romance && romance->npc == "SIULAJIA")
        {
            placed.push_back({"Siulajia", rules::Side::party, {1, 1}, creature(30)});
        }
        auto fight = arrange(placed, 57, {}, 100, GoldBoxGame::treasures_of_the_savage_frontier,
                             false, false,
                             [&](rules::Encounter &encounter)
                             {
                                 encounter.romance = romance;
                             });
        auto &session = *fight.session;
        strike_until(session, "Hero", "Foe",
                     [&]
                     {
                         return session.snapshot().round > 60;
                     });
        return logged(session, "{attacker} hits {name} for {damage}.", "Hero");
    };
    const int plain = hits({}, {}, {});
    check(hits(2, {}, {}) > plain && hits(1, {}, {}) < plain && hits({}, 1, {}) > plain &&
              hits({}, 2, {}) > plain,
          "A frenzied lover strikes better and a refused one worse; either is struck better");
    const rules::Romance requited{"HERO", "SIULAJIA", 100, 100}, alone{"HERO", "NOBODY", 0, 0};
    check(hits({}, {}, requited) > plain, "One loving and loved 100 strikes better");
    check(hits({}, {}, alone) < plain, "One whose beloved is not fighting strikes worse");
}

// Every game after Pool of Radiance ends a fight in which no one has
// attacked for 15 rounds (combatst.pas, CombatStart, fightEndRound 15;
// cbtacts.pas, MakeAttack, which sets it 15 rounds on; combat.pas,
// EndOfRound), and counts it won (postcbt.pas, DetermineCombatResults), the
// monsters still standing; Pool of Radiance's fight goes on. A checkpoint
// keeps the count.
void a_fight_without_attacks_ends_after_15_rounds()
{
    const auto play_quietly =
        [](rules::CombatSession &session, unsigned attack_round, unsigned until_round)
    {
        bool attacked = false;
        play_until(
            session,
            [&](const std::string &actor)
            {
                if (actor == "Fighter" && session.snapshot().round == attack_round && !attacked)
                {
                    attacked = true;
                    return order(session, "melee", "Kobold");
                }
                return order(session, "end");
            },
            [&]
            {
                return session.snapshot().round > until_round;
            },
            400);
    };
    const auto quiet_fight = [](GoldBoxGame game)
    {
        return arrange({{"Fighter", rules::Side::party, {3, 3}, creature(500)},
                        {"Kobold", rules::Side::opposition, {4, 3}, creature(500, 0)}},
                       5, {}, 100, game);
    };
    auto pool = quiet_fight(GoldBoxGame::pool_of_radiance);
    play_quietly(*pool.session, 0, 40);
    check(pool.session->snapshot().outcome == rules::Outcome::ongoing,
          "Pool of Radiance's fight goes on however long no one attacks");
    for (const auto game :
         {GoldBoxGame::curse_of_the_azure_bonds, GoldBoxGame::secret_of_the_silver_blades,
          GoldBoxGame::gateway_to_the_savage_frontier, GoldBoxGame::pools_of_darkness,
          GoldBoxGame::treasures_of_the_savage_frontier, GoldBoxGame::unlimited_adventures})
    {
        auto fight = quiet_fight(game);
        play_quietly(*fight.session, 0, 40);
        const auto ended = fight.session->snapshot();
        check(ended.outcome == rules::Outcome::victory && ended.round_limit_reached &&
                  ended.round == 15 && view_of(ended, "Kobold").conscious,
              "A fight with no attack ends won after its 15th round, the monster standing");
    }
    // An attack in round 5 puts the end off to 14 rounds after it, through
    // a checkpoint.
    auto fight = quiet_fight(GoldBoxGame::secret_of_the_silver_blades);
    play_quietly(*fight.session, 5, 8);
    auto restored = fight.module->restore(fight.session->save());
    check(restored->snapshot().outcome == rules::Outcome::ongoing, "Round 9 begins the same");
    play_quietly(*restored, 0, 40);
    const auto later = restored->snapshot();
    check(later.outcome == rules::Outcome::victory && later.round_limit_reached &&
              later.round == 19,
          "An attack puts the end off to 14 rounds after the round it is made in");
}

// Ice Storm's element (magic.pas, SpellIceStorm; the kit's spellfx.c,
// spell_ice_storm), and Cone of Cold's (SpellConeOfCold): magic resistance
// turns them in every game.
void each_game_storms_by_its_own_element()
{
    using original::ClassSlot;
    const auto resisted = [](GoldBoxGame game, int spell)
    {
        auto warded = victim(100);
        // Magic resistance that turns every spell of a caster below level 8.
        warded.effects.push_back({106, 0, 0, false});
        auto fight = arrange(
            {{"Mage", rules::Side::party, {0, 3}, caster(ClassSlot::magic_user, 7, {spell})},
             {"Warded", rules::Side::opposition, {2, 3}, warded}},
            31, {}, 100, game);
        auto &session = *fight.session;
        cast_now(session, "Mage", spell,
                 [&]
                 {
                     return aim_at_cell(session, {2, 3});
                 });
        return view_of(session.snapshot(), "Warded").hit_points == 100;
    };
    for (const int spell : {187, 192})
    {
        for (const auto game :
             {GoldBoxGame::curse_of_the_azure_bonds, GoldBoxGame::secret_of_the_silver_blades,
              GoldBoxGame::gateway_to_the_savage_frontier, GoldBoxGame::pools_of_darkness,
              GoldBoxGame::treasures_of_the_savage_frontier, GoldBoxGame::unlimited_adventures})
        {
            check(resisted(game, spell), "Magic resistance turns each game's Ice Storm and Cone "
                                         "of Cold");
        }
    }
}

void native_stinking_cloud_uses_its_actual_dispatch_context()
{
    bool affected = false, coughed = false;
    for (unsigned seed = 31; seed < 39; ++seed)
    {
        auto target = victim(500);
        target.magic_resistance = 60;
        target.saving_throws.fill(-20);
        auto mage = caster(original::ClassSlot::magic_user, 20, {34});
        // Observe initial placement before the next round decrements the
        // cloud's new effects; their subsequent clock is tested separately.
        mage.initiative_bonus = 30;
        auto fight = arrange({{"Mage", rules::Side::party, {2, 3}, mage},
                              {"Target", rules::Side::opposition, {5, 3}, target}},
                             seed, {}, 100, GoldBoxGame::dark_queen_of_krynn);
        cast_now(*fight.session, "Mage", 34,
                 [&]
                 {
                     return aim_at_cell(*fight.session, {5, 3});
                 });
        const auto left = left_by_fight(*fight.session, "Target");
        for (const auto &effect : left.effects)
        {
            if (effect.code == 30 || effect.code == 31)
            {
                affected = true;
                check(effect.minutes >= 2 && effect.minutes <= 5,
                      "Native cloud coughing and nausea both last one d4 plus one rounds");
                coughed = coughed || effect.code == 30;
            }
        }
    }
    check(
        affected && coughed,
        "An actual level-twenty native cloud can overcome resistance and makes saved targets cough");
}

void native_free_action_counters_its_shipped_spells()
{
    constexpr auto game = GoldBoxGame::dark_queen_of_krynn;
    for (const int number : {23, 49, 194, 55, 186})
    {
        for (const bool ring : {false, true})
        {
            auto target = victim(500);
            target.saving_throws.fill(100);
            if (ring)
            {
                target.effects.push_back({2150, 0, 255, false});
            }
            const auto slot =
                number == 23 ? original::ClassSlot::cleric : original::ClassSlot::magic_user;
            auto fight =
                arrange({{"Caster", rules::Side::party, {3, 3}, caster(slot, 20, {number})},
                         {"Target", rules::Side::opposition, {4, 3}, target}},
                        53, {}, 100, game);
            try
            {
                cast_now(*fight.session, "Caster", number,
                         [&]
                         {
                             return number == 55 ? aim_at_cell(*fight.session, {4, 3})
                                                 : aim_at(*fight.session, "Target");
                         });
            }
            catch (const std::exception &error)
            {
                throw std::runtime_error("Native spell " + std::to_string(number) +
                                         (ring ? " with ring: " : " without ring: ") +
                                         error.what());
            }
            if (const auto done = offered(*fight.session, "aim_done"))
            {
                check(fight.session->submit(*done), "Confirm the native spell's selected targets");
            }
            const auto left = left_by_fight(*fight.session, "Target");
            const int effect = number == 55 ? 42 : number == 186 ? 506 : 52;
            check(std::ranges::any_of(left.effects,
                                      [effect](const auto &entry)
                                      {
                                          return entry.code == effect;
                                      }) != ring,
                  "Native Free Action counters Hold, Slow and Fumble: " + std::to_string(number) +
                      (ring ? " with ring" : " without ring"));
        }
    }
    auto slowed = victim(500);
    slowed.effects.push_back({42, 100, 12, false});
    auto freed = slowed;
    freed.effects.push_back({2150, 0, 255, false});
    auto fight = arrange({{"Slowed", rules::Side::party, {1, 3}, slowed},
                          {"Freed", rules::Side::opposition, {6, 3}, freed}},
                         53, {}, 100, game);
    const auto state = fight.session->snapshot();
    check(view_of(state, "Freed").movement_feet == 2 * view_of(state, "Slowed").movement_feet,
          "The native ring suppresses an existing Slow effect's movement penalty");
}

void native_untriggered_aurak_generator_stays_passive()
{
    // Installed native raw151 is permanent255 with no removal callback.
    // Its handler is registered but absent from all resident trigger groups.
    for (const auto game : {GoldBoxGame::dark_queen_of_krynn, GoldBoxGame::death_knights_of_krynn})
    {
        auto aurak = victim(500);
        aurak.movement = 0;
        aurak.effects.push_back({1404, 0, 255, false});
        auto fight = arrange({{"Hero", rules::Side::party, {1, 3}, victim(500)},
                              {"Aurak", rules::Side::opposition, {5, 3}, aurak}},
                             37, {}, 100, game);
        const auto initial = original::Combatant::decode(
            view_of(fight.session->snapshot(), "Aurak").persistent.resources);
        const auto generator = std::ranges::find(initial->effects, 1404, &original::Effect::code);
        check(
            generator != initial->effects.end() &&
                generator->strength == (game == GoldBoxGame::dark_queen_of_krynn ? 255 : 3),
            "Native keeps the inherent generator metadata while the earlier Krynn game initializes its active counter");
        play_until(
            *fight.session,
            [&](const std::string &)
            {
                return order(*fight.session, "end");
            },
            [&]
            {
                return fight.session->snapshot().round >= 4;
            });
        const bool active = logged(*fight.session, "{name} fires an energy bolt.") > 0 ||
                            logged(*fight.session, "{name} breathes a blinding cloud.") > 0;
        check(
            active == (game == GoldBoxGame::death_knights_of_krynn),
            "Native's untriggered handler stays passive without disabling earlier Krynn energy powers");
    }
}

void native_enchanted_auraks_keep_their_native_damage()
{
    const auto damage = [](bool enchanted, bool bolt)
    {
        auto aurak = victim(500);
        aurak.effects.push_back({1459, 0, 255, false});
        aurak.effects.push_back({bolt ? 1466 : 1460, bolt ? 0 : 1, 255, !bolt});
        if (enchanted)
        {
            aurak.effects.push_back({2191, 0, 255, false});
        }
        auto fight = arrange({{"Hero", rules::Side::party, {3, 3}, victim(500)},
                              {"Aurak", rules::Side::opposition, {4, 3}, aurak}},
                             37, {}, 100, GoldBoxGame::dark_queen_of_krynn);
        play_until(
            *fight.session,
            [&](const std::string &)
            {
                return order(*fight.session, "end");
            },
            [&]
            {
                return view_of(fight.session->snapshot(), "Hero").hit_points < 500;
            });
        for (const auto &entry : fight.session->snapshot().log_entries)
        {
            if (entry.message.source == "{name} takes {damage}." &&
                entry.message.arguments.front().value == "Hero")
            {
                return std::stoi(entry.message.arguments[1].value);
            }
        }
        return 0;
    };
    const int ordinary_bolt = damage(false, true), enchanted_bolt = damage(true, true);
    check(ordinary_bolt >= 2 && ordinary_bolt <= 12 && enchanted_bolt > 12 && enchanted_bolt <= 64,
          "Native ordinary and enchanted Aurak bolt damage: " + std::to_string(ordinary_bolt) +
              ", " + std::to_string(enchanted_bolt));
    const int ordinary_blast = damage(false, false), enchanted_blast = damage(true, false);
    check(ordinary_blast >= 3 && ordinary_blast <= 18 && enchanted_blast > 18 &&
              enchanted_blast <= 128,
          "Native ordinary and enchanted Aurak explosion damage: " +
              std::to_string(ordinary_blast) + ", " + std::to_string(enchanted_blast));

    auto aurak = victim(500);
    aurak.effects.push_back({1458, 1, 255, true});
    aurak.effects.push_back({1404, 0, 0, false});
    auto fight = arrange({{"Hero", rules::Side::party, {3, 3}, victim(500)},
                          {"Aurak", rules::Side::opposition, {4, 3}, aurak}},
                         37, {}, 100, GoldBoxGame::dark_queen_of_krynn);
    play_until(
        *fight.session,
        [&](const std::string &)
        {
            return order(*fight.session, "end");
        },
        [&]
        {
            const auto numbers = original::Combatant::decode(
                view_of(fight.session->snapshot(), "Aurak").persistent.resources);
            return numbers && std::ranges::any_of(numbers->effects,
                                                  [](const auto &effect)
                                                  {
                                                      return effect.code == 1459;
                                                  });
        });
    const auto numbers = original::Combatant::decode(
        view_of(fight.session->snapshot(), "Aurak").persistent.resources);
    check(
        numbers &&
            std::ranges::any_of(numbers->effects,
                                [](const auto &effect)
                                {
                                    return effect.code == 1459;
                                }) &&
            std::ranges::none_of(numbers->effects,
                                 [](const auto &effect)
                                 {
                                     return effect.code == 1466;
                                 }),
        "A native Aurak whose frenzy expires reaches its final form without the slain form's zap");
    check(numbers && std::ranges::none_of(numbers->effects,
                                          [](const auto &effect)
                                          {
                                              return effect.code == 1404;
                                          }),
          "A native Aurak's final form loses its earlier energy-bolt power");

    // Native effects.c excludes all three later Aurak forms from the
    // explosion's stun, independently of whether its damage gets through.
    // Native raw 152, 160 and 166 map to the shared madness, frenzy and
    // explosion markers 1413, 1432 and 1460 (raw 167, the zap 1466, is not
    // among them). An ordinary neighbour on a failed save is stunned.
    for (const int phase : {1413, 1432, 1460, 0})
    {
        auto other = victim(500);
        other.saving_throws.fill(30);
        if (phase != 0)
        {
            other.effects.push_back({phase, 999, 255, false});
        }
        auto exploding = victim(500);
        exploding.effects.push_back({1459, 0, 255, false});
        exploding.effects.push_back({1460, 1, 255, true});
        auto blast = arrange({{"Other Aurak", rules::Side::party, {3, 3}, other},
                              {"Exploding Aurak", rules::Side::opposition, {4, 3}, exploding}},
                             37, {}, 100, GoldBoxGame::dark_queen_of_krynn);
        play_until(
            *blast.session,
            [&](const std::string &)
            {
                return order(*blast.session, "end");
            },
            [&]
            {
                return logged(*blast.session, "{name} explodes.") > 0;
            });
        const auto survivor = original::Combatant::decode(
            view_of(blast.session->snapshot(), "Other Aurak").persistent.resources);
        const bool stunned = survivor && std::ranges::any_of(survivor->effects,
                                                             [](const auto &effect)
                                                             {
                                                                 return effect.code == 506;
                                                             });
        check(survivor && view_of(blast.session->snapshot(), "Other Aurak").hit_points < 500 &&
                  stunned == (phase == 0),
              "Only an ordinary neighbour is stunned by a native Aurak's explosion: " +
                  std::to_string(phase));
    }
}

// The index of the first log entry of this template naming this creature.
std::size_t first_logged(const rules::CombatSession &session, std::string_view source,
                         std::string_view name)
{
    const auto entries = session.snapshot().log_entries;
    for (std::size_t at = 0; at < entries.size(); ++at)
    {
        if (entries[at].message.source == source &&
            std::ranges::any_of(entries[at].message.arguments,
                                [name](const rules::MessageArgument &a)
                                {
                                    return a.value == name;
                                }))
        {
            return at;
        }
    }
    return entries.size();
}

void native_final_aurak_forms_follow_their_shapes()
{
    // cbaction.c: neither final shape (raw 210 or 213, both carrying 165)
    // strikes a creature leaving it; an ordinary Aurak does.
    for (const bool final_form : {true, false})
    {
        auto aurak = victim(500);
        aurak.thac0 = -20;
        aurak.movement = 0;
        if (final_form)
        {
            aurak.effects.push_back({1459, 0, 255, false});
            aurak.effects.push_back({1460, 3, 255, true});
        }
        aurak.effects.push_back({1466, 0, 255, false});
        auto fight = arrange({{"Hero", rules::Side::party, {3, 3}, victim(500)},
                              {"Aurak", rules::Side::opposition, {4, 3}, aurak}},
                             37, {}, 100, GoldBoxGame::dark_queen_of_krynn);
        auto &session = *fight.session;
        // The revived shape keeps its turn: its zap strikes the hero next to it.
        play_until(
            session,
            [&](const std::string &actor)
            {
                return order(session, actor == "Aurak" ? "quick" : "end");
            },
            [&]
            {
                return logged(session, "{name} is struck by an energy bolt.") > 0;
            });
        check(logged(session, "{name} is struck by an energy bolt.") > 0,
              "A native Aurak with the zap strikes the hero next to it");
        until_turn_of(session, "Hero");
        session.submit(order(session, "move", {}, rules::Cell{0, 3}));
        check(logged(session, "{attacker} strikes {name} leaving.") == (final_form ? 0 : 1),
              final_form ? "A native final Aurak strikes no one leaving it"
                         : "An ordinary native Aurak strikes the hero leaving it");
    }

    // aiturn.c: the shape the frenzy's timer makes (213, without the zap)
    // ends its turn at once, yet still explodes after 3 rounds.
    auto aurak = victim(500);
    aurak.thac0 = -20;
    aurak.effects.push_back({1458, 1, 255, true});
    auto fight = arrange({{"Hero", rules::Side::party, {3, 3}, victim(500)},
                          {"Aurak", rules::Side::opposition, {4, 3}, aurak}},
                         37, {}, 100, GoldBoxGame::dark_queen_of_krynn);
    auto &session = *fight.session;
    const auto quick_aurak = [&](const std::string &actor)
    {
        return order(session, actor == "Aurak" ? "quick" : "end");
    };
    play_until(session, quick_aurak,
               [&]
               {
                   return logged(session, "{name} becomes a blazing shape.") > 0;
               });
    const auto shaped_round = session.snapshot().round;
    const auto shaped_at = first_logged(session, "{name} becomes a blazing shape.", "Aurak");
    play_until(session, quick_aurak,
               [&]
               {
                   return logged(session, "{name} explodes.") > 0;
               });
    const auto exploded_at = first_logged(session, "{name} explodes.", "Aurak");
    const auto entries = session.snapshot().log_entries;
    // Shaped as round 1 ends, it lives through rounds 2 to 4 and explodes as
    // the last of them ends, which ends the fight.
    check(exploded_at < entries.size() && shaped_round == 2 && session.snapshot().round == 4,
          "The timer's final Aurak still explodes after 3 rounds");
    const bool attacked_before = std::any_of(
        entries.begin() + static_cast<std::ptrdiff_t>(shaped_at),
        entries.begin() + static_cast<std::ptrdiff_t>(std::min(exploded_at, entries.size())),
        [](const rules::LogEntry &entry)
        {
            return std::ranges::any_of(entry.message.arguments,
                                       [](const rules::MessageArgument &a)
                                       {
                                           return a.name == "attacker" && a.value == "Aurak";
                                       });
        });
    check(
        logged(session, "{attacker} hits {name} for {damage}.", "Aurak") > 0 && !attacked_before,
        "The timer's final Aurak, which struck in its frenzy, makes no attack before it explodes");
}

void native_frenzy_burns_only_the_other_side_with_fire()
{
    // effects.c fx_immolate: only the enemy team, as fire. The earlier Krynn
    // games keep their own untyped burn of every neighbour.
    for (const auto game : {GoldBoxGame::dark_queen_of_krynn, GoldBoxGame::champions_of_krynn})
    {
        auto aurak = victim(500);
        aurak.attacks = {};
        aurak.movement = 0;
        aurak.effects.push_back({1448, 0, 255, false});
        auto fire_proof = victim(500);
        fire_proof.effects.push_back({1776, 0, 255, false});
        auto fight = arrange({{"Hero", rules::Side::party, {3, 3}, victim(500)},
                              {"Fireproof", rules::Side::party, {4, 4}, fire_proof},
                              {"Friend", rules::Side::opposition, {5, 3}, victim(500)},
                              {"Aurak", rules::Side::opposition, {4, 3}, aurak}},
                             37, {}, 100, game);
        auto &session = *fight.session;
        play_until(
            session,
            [&](const std::string &)
            {
                return order(session, "end");
            },
            [&]
            {
                return view_of(session.snapshot(), "Hero").hit_points < 500;
            });
        const auto state = session.snapshot();
        const bool native = game == GoldBoxGame::dark_queen_of_krynn;
        check(view_of(state, "Hero").hit_points < 500,
              "A frenzied Aurak burns the enemy next to it");
        check((view_of(state, "Friend").hit_points == 500) == native,
              native ? "A native frenzied Aurak spares its own side"
                     : "An earlier Krynn frenzied Aurak still burns its own side");
        check((view_of(state, "Fireproof").hit_points == 500) == native,
              native ? "A native frenzied Aurak's burn is fire"
                     : "An earlier Krynn frenzied Aurak's burn stays untyped");
    }
}

void native_blasts_respect_defenses()
{
    const auto damage = [](int number, int resistance, bool ring, int shield = 0)
    {
        auto target = victim(500);
        target.magic_resistance = resistance;
        target.saving_throws.fill(-20);
        if (shield)
        {
            target.effects.push_back({shield, 100, 15, false});
        }
        if (ring)
        {
            target.effects.push_back({661, 0, 255, false});
        }
        auto fight = arrange({{"Mage",
                               rules::Side::party,
                               {0, 3},
                               caster(original::ClassSlot::magic_user, 11, {number})},
                              {"Warded", rules::Side::opposition, {5, 3}, target}},
                             287, {}, 100, GoldBoxGame::dark_queen_of_krynn);
        cast_now(*fight.session, "Mage", number,
                 [&]
                 {
                     if (number == 315)
                     {
                         check(fight.session->submit(order(*fight.session, "blast_delay")),
                               "Cast the native delayed blast without a delay");
                     }
                     return aim_at_cell(*fight.session, {5, 3});
                 });
        return 500 - view_of(fight.session->snapshot(), "Warded").hit_points;
    };
    for (const int number : {47, 315, 324})
    {
        check(damage(number, 100, false) == 0,
              "Native magic resistance also turns magical damage without an added effect");
    }
    for (const int number : {315, 324})
    {
        check(damage(number, 0, true) < damage(number, 0, false),
              "Native fire resistance counts each delayed-blast and meteor damage die");
        check(damage(number, 0, false, 254) == 0 && damage(number, 0, false, 250) > 0,
              "A successful native save under cold Fire Shield negates the fire blast");
    }
}

// Curse's effects by the fight's numbers: those that work as Pool of
// Radiance's of the same number keep it, fumbling is helplessness, and the
// rest move 200 up, so a dracolich's paralyzing touch (122) is not Pool of
// Radiance's mummy fire.
void curse_effects_take_their_own_numbers()
{
    const auto curse = opengold::GoldBoxGame::curse_of_the_azure_bonds;
    const auto pool = opengold::GoldBoxGame::pool_of_radiance;
    check(original::canonical_effect(122, curse) == 322 &&
              original::canonical_effect(122, pool) == 122 &&
              original::canonical_effect(64, curse) == 64 &&
              original::canonical_effect(27, curse) == 31 &&
              original::canonical_effect(105, curse) == 305,
          "Curse's effects keep Pool of Radiance's numbers only where they work alike");
}

// Curse's defenses (ovr013): a creature that resists normal weapons takes
// nothing from a plain blow (260); protection from magic stops a spell's
// damage (329); a shambling mound grows on lightning instead (284).
void curse_defenses_hold()
{
    using original::ClassSlot;
    auto hard = victim(500);
    hard.effects.push_back({260, 0, 0, false});
    auto blows = attack_until(creature(20), hard,
                              [](const rules::CombatSession &session)
                              {
                                  return logged(session, "{attacker} hits {name} for {damage}.",
                                                "Monster") > 0 ||
                                         session.snapshot().round > 6;
                              });
    check(view_of(blows.session->snapshot(), "Victim").hit_points == 500,
          "A plain weapon does nothing to one that resists normal weapons");

    auto warded = victim(100);
    warded.effects.push_back({329, 0, 0, false});
    auto mound = victim(100);
    mound.effects.push_back({284, 0, 0, false});
    auto fight =
        arrange({{"Mage", rules::Side::party, {0, 3}, caster(ClassSlot::magic_user, 5, {51})},
                 {"Warded", rules::Side::opposition, {2, 3}, warded},
                 {"Mound", rules::Side::opposition, {5, 3}, mound}},
                151);
    auto &session = *fight.session;
    cast_now(session, "Mage", 51,
             [&]
             {
                 return aim_at_cell(session, {2, 3});
             });
    const auto state = session.snapshot();
    check(view_of(state, "Warded").hit_points == 100,
          "Protection from magic stops the bolt's damage");
    check(view_of(state, "Mound").hit_points > 100,
          "The shambling mound grows on the bolt past its maximum");
}

// Curse's touches and holds (ovr013, ovr014): a dracolich's touch
// paralyzes; a shambling mound whose two blows both hit engulfs its victim,
// which suffocates within 2d4 of its turns.
void curse_touches_and_engulfs()
{
    auto dracolich = creature(50);
    dracolich.effects.push_back({322, 0, 0, false});
    auto held = attack_until(dracolich, victim(500),
                             [](const rules::CombatSession &session)
                             {
                                 return view_of(session.snapshot(), "Victim").status == "helpless";
                             });
    check(view_of(held.session->snapshot(), "Victim").status == "helpless",
          "A dracolich's touch paralyzes");

    auto mound = creature(50);
    mound.attacks[0] = {4, 1, 2, 0};
    mound.effects.push_back({257, 0, 0, false});
    auto engulfed = attack_until(mound, victim(500),
                                 [](const rules::CombatSession &session)
                                 {
                                     return logged(session, "{name} suffocates.") > 0;
                                 });
    check(logged(*engulfed.session, "{attacker} engulfs {name}.", "Monster") > 0 &&
              logged(*engulfed.session, "{name} suffocates.") > 0 &&
              view_of(engulfed.session->snapshot(), "Victim").dead,
          "Engulfed, the victim suffocates");
}

// Curse's spells: Cone of Cold takes those on its path to half the caster's
// level, two wide (ovr023, SpellConeOfCold); Cause Critical Wounds is 3d8+3;
// Slay Living kills on a failed save and wounds on a saved one.
void curse_spells_strike()
{
    using original::ClassSlot;
    auto fight =
        arrange({{"Mage", rules::Side::party, {0, 3}, caster(ClassSlot::magic_user, 9, {192})},
                 {"First", rules::Side::opposition, {2, 3}, victim(100)},
                 {"Second", rules::Side::opposition, {4, 3}, victim(100)},
                 {"Beyond", rules::Side::opposition, {7, 3}, victim(100)},
                 {"Aside", rules::Side::opposition, {3, 6}, victim(100)}},
                141);
    auto &session = *fight.session;
    cast_now(session, "Mage", 192,
             [&]
             {
                 return aim_at_cell(session, {2, 3});
             });
    const auto state = session.snapshot();
    const int first = 100 - view_of(state, "First").hit_points;
    const int second = 100 - view_of(state, "Second").hit_points;
    check(first >= 9 && first <= 45 && second >= 9 && second <= 45,
          "Cone of Cold: 9 and 9d4 to each on its path, or half on a save");
    check(view_of(state, "Beyond").hit_points == 100 && view_of(state, "Aside").hit_points == 100,
          "Those past its reach or off its path are spared");

    auto touch =
        arrange({{"Priest", rules::Side::party, {0, 3}, caster(ClassSlot::cleric, 9, {172, 176})},
                 {"Orc", rules::Side::opposition, {1, 3}, victim(100)}},
                143);
    cast_now(*touch.session, "Priest", 172,
             [&]
             {
                 return aim_at(*touch.session, "Orc");
             });
    const int wound = 100 - view_of(touch.session->snapshot(), "Orc").hit_points;
    check(wound == 0 || (wound >= 6 && wound <= 27),
          "Cause Critical Wounds touches for 3d8+3, or misses");
}

// Stinking Cloud: one in it fails its save and is helpless with nausea; the
// cloud clears after the caster's level in rounds (magic.pas, effhndlr.pas 40).
void a_cloud_sickens()
{
    using original::ClassSlot;
    auto fight =
        arrange({{"Mage", rules::Side::party, {0, 3}, caster(ClassSlot::magic_user, 2, {34})},
                 {"Kobold", rules::Side::opposition, {3, 3}, victim(20)}},
                131);
    auto &session = *fight.session;
    cast_now(session, "Mage", 34,
             [&]
             {
                 return aim_at_cell(session, {3, 3});
             });
    check(view_of(session.snapshot(), "Kobold").status == "helpless", "The kobold gags, helpless");
    play_until(
        session,
        [&](const std::string &)
        {
            return order(session, "end");
        },
        [&]
        {
            return logged(session, "The air clears a little.") > 0 || session.snapshot().round > 6;
        });
    check(logged(session, "The air clears a little.") == 1 && session.snapshot().round <= 4,
          "The cloud clears after 2 rounds");
}

// Curse's Cloudkill (SpellCloudkill, ConsiderTerrainEffects): a cloud on the
// aim and the squares around it whose poison kills any of level 4 or less
// at once, one of level 5 or 6 that fails to save, never one above; it
// clears after the caster's level in rounds, and a checkpoint keeps it.
void cloudkill_poisons()
{
    using original::ClassSlot;
    auto strong = victim(50);
    strong.level = 7;
    auto fight =
        arrange({{"Mage", rules::Side::party, {1, 3}, caster(ClassSlot::magic_user, 9, {191})},
                 {"Kobold", rules::Side::opposition, {2, 3}, victim(20)},
                 {"Orc", rules::Side::opposition, {3, 4}, victim(20)},
                 {"Ogre", rules::Side::opposition, {3, 3}, strong},
                 {"Outside", rules::Side::opposition, {5, 3}, victim(20)}},
                317, {}, 100, GoldBoxGame::curse_of_the_azure_bonds);
    auto &session = *fight.session;
    cast_now(session, "Mage", 191,
             [&]
             {
                 return aim_at_cell(session, {3, 3});
             });
    const auto state = session.snapshot();
    check(view_of(state, "Kobold").dead && view_of(state, "Orc").dead &&
              !view_of(state, "Ogre").dead && !view_of(state, "Outside").dead,
          "Cloudkill kills the weak within it, not the strong nor those outside");
    const auto saved = session.save();
    check(fight.module->restore(saved)->save() == saved, "A checkpoint keeps the cloud");
    play_until(
        session,
        [&](const std::string &)
        {
            return order(session, "end");
        },
        [&]
        {
            return logged(session, "The air clears a little.") > 0 || session.snapshot().round > 12;
        });
    check(logged(session, "The air clears a little.") == 1, "The cloud clears");
}

// Secret's AI balks at a step into a cloudkill when its movement is below 7
// (combatai.pas, AiCanStep, in its reconstruction): the slow monster goes
// round the cloud or waits, never into it, while it lasts.
void secret_ai_shuns_clouds()
{
    using original::ClassSlot;
    auto slow = victim(500);
    slow.level = 7;
    slow.movement = 6;
    auto fight =
        arrange({{"Mage", rules::Side::party, {1, 3}, caster(ClassSlot::magic_user, 9, {191})},
                 {"Slow", rules::Side::opposition, {5, 3}, slow}},
                331, {}, 100, GoldBoxGame::secret_of_the_silver_blades);
    auto &session = *fight.session;
    cast_now(session, "Mage", 191,
             [&]
             {
                 return aim_at_cell(session, {3, 3});
             });
    bool entered = false;
    play_until(
        session,
        [&](const std::string &actor)
        {
            return order(session, actor == "Slow" ? "quick" : "guard");
        },
        [&]
        {
            const auto cell = view_of(session.snapshot(), "Slow").cell;
            entered = entered || (cell.x >= 2 && cell.x <= 4 && cell.y >= 2 && cell.y <= 4);
            return logged(session, "The air clears a little.") > 0 || session.snapshot().round > 8;
        });
    check(!entered, "A monster slower than 7 never steps into the cloudkill");
}

// Darkness's Fire Shield returns twice an adjacent weapon's damage as
// magic (effhndlr.pas, EffZapAttacker), including the minions' innate shield.
void darkness_fire_shield_retaliates()
{
    constexpr auto game = GoldBoxGame::pools_of_darkness;
    constexpr int shield = 343;
    auto blade = creature(200);
    blade.thac0 = -100;
    blade.attacks[0] = {2, 1, 1, 4};
    auto guarded = victim(200);
    guarded.effects = {{shield, 0, 0, false}};
    const auto hit = [](const rules::CombatSession &session)
    {
        return logged(session, "{attacker} hits {name} for {damage}.", "Monster") > 0;
    };
    auto melee = attack_until(blade, guarded, hit, 47, game);
    const auto struck = view_of(melee.session->snapshot(), "Victim");
    const auto striker = view_of(melee.session->snapshot(), "Monster");
    check(struck.hit_points < 200 && 200 - striker.hit_points == 2 * (200 - struck.hit_points),
          "A shield returns twice the adjacent weapon damage");

    auto fragile = blade;
    fragile.hit_points = fragile.max_hit_points = 10;
    auto lethal = attack_until(fragile, guarded, hit, 47, game);
    check(view_of(lethal.session->snapshot(), "Monster").hit_points == 0 &&
              view_of(lethal.session->snapshot(), "Victim").hit_points < 200,
          "A lethal retaliation still lets the triggering blow land");

    // Native MakeAttack clears an attacker's remaining blows when its own
    // Fire Shield retaliation kills it, including the later attack form.
    auto native_fragile = fragile;
    native_fragile.attacks[0] = {8, 1, 1, 4};
    native_fragile.attacks[1] = {4, 1, 1, 10};
    auto native_lethal =
        attack_until(native_fragile, guarded, hit, 47, GoldBoxGame::dark_queen_of_krynn);
    check(view_of(native_lethal.session->snapshot(), "Monster").hit_points == 0 &&
              logged(*native_lethal.session, "{attacker} hits {name} for {damage}.", "Monster") ==
                  1,
          "Native lethal retaliation ends the attacker's remaining blows after its triggering hit");

    guarded.effects.push_back({460, 0, 0, false});
    auto turned = attack_until(blade, guarded, hit, 47, game);
    check(view_of(turned.session->snapshot(), "Victim").hit_points == 200 &&
              view_of(turned.session->snapshot(), "Monster").hit_points == 200,
          "A weapon turned aside causes no retaliation");

    guarded.effects = {{shield, 0, 0, false}};
    auto bow = archer(20);
    bow.thac0 = -100;
    auto afar = arrange({{"Archer", rules::Side::party, {1, 3}, bow},
                         {"Shield", rules::Side::opposition, {4, 3}, guarded}},
                        47, {}, 100, game);
    play_until(
        *afar.session,
        [&](const std::string &actor)
        {
            return actor == "Archer" ? order(*afar.session, "ranged", "Shield")
                                     : order(*afar.session, "end");
        },
        [&]
        {
            return view_of(afar.session->snapshot(), "Shield").hit_points < 200;
        });
    check(view_of(afar.session->snapshot(), "Archer").hit_points == 50,
          "The shield does not return a distant arrow's damage");

    auto cast = arrange(
        {{"Mage", rules::Side::party, {0, 3}, caster(original::ClassSlot::magic_user, 9, {185})},
         {"Kobold", rules::Side::opposition, {7, 7}, victim(20)}},
        319, {}, 100, game);
    cast_now(*cast.session, "Mage", 185,
             [&]
             {
                 return offered(*cast.session, "flames");
             });
    const auto protected_mage = left_by_fight(*cast.session, "Mage");
    const auto has = [&](int code)
    {
        return std::ranges::any_of(protected_mage.effects,
                                   [code](const original::Effect &effect)
                                   {
                                       return effect.code == code;
                                   });
    };
    check(has(shield) && !has(335),
          "The cast shield grants retaliation rather than electricity immunity");
}

// Fire Shield's caster chooses its flames as it goes off: hot doubles fire,
// cold doubles cold (Curse's SpellFireShield).
void fire_shield_flames_are_chosen()
{
    using original::ClassSlot;
    // The choices' items, as the session offers them.
    enum class Flames : unsigned
    {
        hot = 1,
        cold = 2
    };
    const auto shield_of = [](Flames flames)
    {
        auto fight =
            arrange({{"Mage", rules::Side::party, {0, 3}, caster(ClassSlot::magic_user, 9, {185})},
                     {"Kobold", rules::Side::opposition, {7, 7}, victim(20)}},
                    319, {}, 100, GoldBoxGame::curse_of_the_azure_bonds);
        auto &session = *fight.session;
        cast_now(session, "Mage", 185,
                 [&]
                 {
                     const auto legal = session.legal_commands();
                     const auto chosen = std::find_if(
                         legal.begin(), legal.end(),
                         [flames](const rules::Command &c)
                         {
                             return c.verb == "flames" && c.item == static_cast<unsigned>(flames);
                         });
                     return chosen == legal.end() ? std::nullopt
                                                  : std::optional<rules::Command>{*chosen};
                 });
        const auto mage = left_by_fight(session, "Mage");
        return std::ranges::any_of(mage.effects,
                                   [](const original::Effect &e)
                                   {
                                       return e.code == 250;
                                   })
                   ? Flames::hot
                   : Flames::cold;
    };
    check(shield_of(Flames::hot) == Flames::hot && shield_of(Flames::cold) == Flames::cold,
          "The caster's choice of flames is the shield it gets");

    auto cursed = creature(30);
    cursed.effects = {{36, 0, 1, false}};
    auto lift =
        arrange({{"Mage", rules::Side::party, {0, 3}, caster(ClassSlot::magic_user, 9, {189})},
                 {"Cursed", rules::Side::party, {1, 3}, cursed},
                 {"Kobold", rules::Side::opposition, {7, 7}, victim(20)}},
                321, {}, 100, GoldBoxGame::curse_of_the_azure_bonds);
    cast_now(*lift.session, "Mage", 189,
             [&]
             {
                 return aim_at(*lift.session, "Cursed");
             });
    check(logged(*lift.session, "{name} is un-cursed.") == 1,
          "A magic-user's Remove Curse lifts a curse in a fight");
}

// Dispel Magic: an effect of level 0 goes for a level-10 caster; one of
// strength 255 (a race's own) never does (magic.pas, the dispel).
void dispel_magic_strips()
{
    using original::ClassSlot;
    auto hasted = creature(30, 0);
    hasted.effects = {{39, 5, 0, false}, {90, 0, 255, false}};
    auto fight =
        arrange({{"Mage", rules::Side::party, {0, 3}, caster(ClassSlot::magic_user, 10, {46})},
                 {"Foe", rules::Side::opposition, {5, 3}, hasted}},
                137);
    auto &session = *fight.session;
    cast_now(session, "Mage", 46,
             [&]
             {
                 return aim_at_cell(session, {5, 3});
             });
    const auto left = left_by_fight(session, "Foe").effects;
    check(left == std::vector<original::Effect>{{90, 0, 255, false}},
          "Haste is dispelled, the race's own save bonus is not");
    bool native_effect_survived = false;
    for (std::uint64_t seed = 137; seed < 145; ++seed)
    {
        auto protected_foe = creature(30, 0);
        protected_foe.effects = {{39, 100, 31, false}};
        auto native =
            arrange({{"Mage", rules::Side::party, {0, 3}, caster(ClassSlot::magic_user, 31, {46})},
                     {"Foe", rules::Side::opposition, {5, 3}, protected_foe}},
                    seed, {}, 100, GoldBoxGame::dark_queen_of_krynn);
        cast_now(*native.session, "Mage", 46,
                 [&]
                 {
                     return aim_at_cell(*native.session, {5, 3});
                 });
        const auto effects = left_by_fight(*native.session, "Foe").effects;
        native_effect_survived =
            native_effect_survived ||
            std::ranges::find(effects, 39, &original::Effect::code) != effects.end();
    }
    check(
        native_effect_survived,
        "Native Dispel Magic contests a level-31 effect at equal caster level; bit four is not lost");
}

// Treasures' Dispel Magic leaves the romance's love, a kampfult's crushing, a
// stone golem's stone and a yeti's grab (its magic.pas, SpellDispelMagic).
void treasures_dispel_keeps_its_own()
{
    using original::ClassSlot;
    auto foe = creature(30, 0);
    foe.effects = {{39, 5, 0, false},
                   {1331, 0, 0, false},
                   {1344, 0, 0, false},
                   {1347, 0, 0, false},
                   {1349, 0, 0, false}};
    auto fight =
        arrange({{"Mage", rules::Side::party, {0, 3}, caster(ClassSlot::magic_user, 10, {46})},
                 {"Foe", rules::Side::opposition, {5, 3}, foe}},
                137, {}, 100, GoldBoxGame::treasures_of_the_savage_frontier);
    auto &session = *fight.session;
    cast_now(session, "Mage", 46,
             [&]
             {
                 return aim_at_cell(session, {5, 3});
             });
    const auto left = left_by_fight(session, "Foe").effects;
    check(left == std::vector<original::Effect>{{1331, 0, 0, false},
                                                {1344, 0, 0, false},
                                                {1347, 0, 0, false},
                                                {1349, 0, 0, false}},
          "Haste is dispelled; Treasures' love, crushing, stone golem and grab are not");
}

// Animate Dead raises a slain human for the cleric, whole (magic.pas).
void the_dead_rise()
{
    using original::ClassSlot;
    auto killer = monster({2, 1, 1, 50}, {0, 0, 0, 0}, {});
    auto bandit = creature(10, 0);
    bandit.creature_type = 0;
    auto fight =
        arrange({{"Killer", rules::Side::party, {2, 3}, killer},
                 {"Cleric", rules::Side::party, {0, 3}, caster(ClassSlot::cleric, 5, {36})},
                 {"Bandit", rules::Side::opposition, {3, 3}, bandit},
                 {"Ogre", rules::Side::opposition, {7, 7}, creature(1000, 0)}},
                139);
    auto &session = *fight.session;
    play_until(
        session,
        [&](const std::string &actor)
        {
            if (actor == "Killer")
            {
                if (const auto melee = offered(session, "melee", "Bandit"))
                {
                    return *melee;
                }
            }
            return order(session, "end");
        },
        [&]
        {
            return view_of(session.snapshot(), "Bandit").dead;
        });
    until_turn_of(session, "Cleric");
    session.submit(*cast_command(session, 36));
    play_until(
        session,
        [&](const std::string &)
        {
            return order(session, "end");
        },
        [&]
        {
            return logged(session, "{name} is animated.") > 0 || session.snapshot().round > 4;
        });
    const auto &risen = view_of(session.snapshot(), "Bandit");
    check(risen.side == rules::Side::party && !risen.dead && risen.hit_points == 10,
          "The slain bandit rises whole for the cleric");
}

// Slow Poison stands one killed by poison back up with 1 Hit Point
// (magic.pas, the slow poison; effhndlr.pas, EffDelayedRevival).
void slow_poison_raises()
{
    using original::ClassSlot;
    auto spider = monster({0, 0, 0, 0}, {2, 1, 1, 0}, {{64, 0, 0, false}});
    auto fight =
        arrange({{"Victim", rules::Side::party, {3, 3}, victim()},
                 {"Cleric", rules::Side::party, {2, 3}, caster(ClassSlot::cleric, 3, {26})},
                 {"Spider", rules::Side::opposition, {4, 3}, spider}},
                149);
    auto &session = *fight.session;
    play_until(
        session,
        [&](const std::string &actor)
        {
            if (actor == "Spider")
            {
                if (const auto melee = offered(session, "melee", "Victim"))
                {
                    return *melee;
                }
            }
            return order(session, "end");
        },
        [&]
        {
            return view_of(session.snapshot(), "Victim").dead;
        });
    cast_now(session, "Cleric", 26,
             [&]
             {
                 return aim_at(session, "Victim");
             });
    const auto &raised = view_of(session.snapshot(), "Victim");
    check(!raised.dead && raised.conscious && raised.hit_points == 1,
          "The poisoned dead stand again with 1 Hit Point");
}

// Snake Charm takes the snakes the caster's Hit Points can pay for
// (magic.pas, the snake charm).
void snakes_are_charmed()
{
    using original::ClassSlot;
    auto snake = creature(10, 0);
    snake.creature_type = 14;
    auto fight =
        arrange({{"Cleric", rules::Side::party, {0, 3}, caster(ClassSlot::cleric, 3, {27})},
                 {"Snake", rules::Side::opposition, {5, 3}, snake}},
                151);
    auto &session = *fight.session;
    until_turn_of(session, "Cleric");
    session.submit(*cast_command(session, 27));
    play_until(
        session,
        [&](const std::string &)
        {
            return order(session, "end");
        },
        [&]
        {
            return view_of(session.snapshot(), "Snake").status == "helpless" ||
                   session.snapshot().round > 3;
        });
    check(view_of(session.snapshot(), "Snake").status == "helpless", "The snake is charmed");
}

// The items a creature can use, from its record's items (viewchar.pas, UseItem):
// a wand's power (item power 88 is 65, the Wand of Magic Missiles) with its
// charges, and a scroll's spells; an item without either is not one.
void usable_items_come_from_the_gear()
{
    por::ItemTemplate potion_type;
    potion_type.worn_location = 0;
    for (const auto [stored, canonical] : {std::pair{57, 57},
                                           {59, 59},
                                           {60, 60},
                                           {61, 61},
                                           {62, 62},
                                           {63, original::native_elixir_youth},
                                           {64, 64},
                                           {65, 65},
                                           {95, 195},
                                           {97, 197},
                                           {99, 199}})
    {
        por::ItemRecord potion;
        potion.type = 47;
        potion.effect_codes = {1, static_cast<std::uint8_t>(stored), 0};
        const std::array carried{original::CarriedItem{potion, potion_type}};
        const auto native = original::usable_items(carried, GoldBoxGame::dark_queen_of_krynn);
        check(native.size() == 1 && native.front().spells[0] == canonical,
              "Native item use remaps the item's power before looking up a spell: " +
                  std::to_string(stored));
    }
    por::ItemTemplate wand_type;
    wand_type.worn_location = 14;
    por::ItemRecord wand;
    wand.readied_raw = 1;
    wand.effect_codes = {30, 88, 0};
    por::ItemTemplate scroll_type;
    scroll_type.worn_location = 11;
    por::ItemRecord scroll;
    scroll.effect_codes = {15, 21, 0};
    por::ItemRecord plain;
    const std::vector<original::CarriedItem> carried{
        {wand, wand_type}, {plain, wand_type}, {scroll, scroll_type}};
    const auto mage = original::combatant_from_record(kobold_record(), carried);
    check(mage.items.size() == 2 && mage.items[0].spells[0] == 65 && mage.items[0].charges == 30 &&
              mage.items[0].readied && mage.items[1].scroll && mage.items[1].ordinal == 2 &&
              mage.items[1].spells == std::array<int, 3>{15, 21, 0},
          "A wand of 30 charges and a scroll of Magic Missile and Sleep");
    check(original::Combatant::decode(mage.encode()) == mage, "Usable items survive the encoding");
}

std::optional<rules::Command> use_command(const rules::CombatSession &session, int ordinal,
                                          int spell)
{
    for (const auto &c : session.legal_commands())
    {
        if (c.verb == "use" && c.item == static_cast<unsigned>(ordinal * 256 + spell))
        {
            return c;
        }
    }
    return std::nullopt;
}

// A wand's power goes off at once, at level 12, and spends a charge; Shield
// does not stop a wand's missiles; giving up the aim keeps the charge.
void wands_and_potions()
{
    // Native items dispatch their use-time power, rather than the ordinary
    // spell that happens to share its stored byte. Exercise actual commands
    // and consumed charges, including Youth's byte-sized command identity.
    for (const int stored : {57, 59, 62, 63, 97, 99})
    {
        por::ItemTemplate type;
        type.worn_location = 14;
        por::ItemRecord potion;
        potion.type = 47;
        potion.readied_raw = 1;
        potion.effect_codes = {1, static_cast<std::uint8_t>(stored), 0};
        const std::array carried{original::CarriedItem{potion, type}};
        auto drinker = creature(80);
        drinker.hit_points = 10;
        drinker.strength = drinker.natural_strength = 18;
        drinker.effects.push_back({original::age_in_years, 0, 50, false});
        drinker.items = original::usable_items(carried, GoldBoxGame::dark_queen_of_krynn);
        auto native = arrange({{"Drinker", rules::Side::party, {0, 0}, drinker},
                               {"Distant foe", rules::Side::opposition, {7, 7}, creature(500)}},
                              159, {}, 100, GoldBoxGame::dark_queen_of_krynn);
        auto &combat = *native.session;
        until_turn_of(combat, "Drinker");
        const auto power = original::canonical_item_power(stored, GoldBoxGame::dark_queen_of_krynn);
        const auto use = use_command(combat, 0, power);
        check(use && combat.submit(*use), "Use the native consumable: " + std::to_string(stored));
        const auto after = left_by_fight(combat, "Drinker");
        check(after.items.empty(), "The actual native use spends its only charge");
        const auto effect = [&](int code)
        {
            return std::ranges::find(after.effects, code, &original::Effect::code);
        };
        if (stored == 57 || stored == 97)
        {
            check(effect(stored == 57 ? 39 : 25) != after.effects.end(),
                  "The native potion grants Haste or Invisibility");
        }
        else if (stored == 59)
        {
            const auto frost = effect(113);
            check(after.strength == 21 && frost != after.effects.end() && frost->minutes >= 50 &&
                      frost->minutes <= 80 &&
                      original::lasting_condition(*frost, GoldBoxGame::dark_queen_of_krynn),
                  "Frost strength lasts its native 1d4 times ten plus forty minutes");
        }
        else if (stored == 63)
        {
            const auto age = effect(original::age_in_years);
            check(age != after.effects.end() && age->strength >= 45 && age->strength <= 48,
                  "The native elixir makes its actual user younger");
        }
        else
        {
            const int healed = view_of(combat.snapshot(), "Drinker").hit_points;
            check(healed >= 14 && healed <= 20,
                  "Both native healing potions heal their actual user for 2d4 plus two: " +
                      std::to_string(stored) + " HP " + std::to_string(healed));
        }
    }
    auto mage = creature(30);
    mage.items = {{0, true, false, 14, true, {65, 0, 0}, 30, 0},
                  {1, true, false, 14, true, {62, 0, 0}, 1, 0}};
    mage.hit_points = 10;
    auto shielded = creature(100, 0);
    shielded.effects = {{17, 10, 1, false}};
    auto fight = arrange({{"Mage", rules::Side::party, {0, 3}, mage},
                          {"Target", rules::Side::opposition, {5, 3}, shielded}},
                         157);
    auto &session = *fight.session;
    until_turn_of(session, "Mage");
    session.submit(*use_command(session, 0, 65));
    session.submit(*aim_at(session, "Target"));
    const int lost = 100 - view_of(session.snapshot(), "Target").hit_points;
    check(lost >= 4 && lost <= 10, "The wand's missiles strike for 2d4+2 through Shield");
    check(left_by_fight(session, "Mage").items[0].charges == 29, "A charge is spent");

    until_turn_of(session, "Mage");
    session.submit(*use_command(session, 0, 65));
    session.submit(order(session, "abort"));
    check(left_by_fight(session, "Mage").items[0].charges == 29, "An aim given up spends nothing");

    until_turn_of(session, "Mage");
    session.submit(*use_command(session, 1, 62));
    const auto after = left_by_fight(session, "Mage");
    check(view_of(session.snapshot(), "Mage").hit_points > 10 && after.items.size() == 1,
          "The potion heals and is gone");
}

// Pools of Darkness's antiMagic (its scripts' cell 2, which the finale sets
// for Gothmenes' last stand) bars the Use menu as well as Cast (viewchar.pas,
// UseItemMenu; combat.pas, the Cast menu).
void darkness_no_magic_bars_use()
{
    using original::ClassSlot;
    auto wielder = caster(ClassSlot::cleric, 3, {1});
    wielder.items = {{0, true, false, 14, true, {65, 0, 0}, 30, 0}};
    const std::vector<Placed> armed{{"Cleric", rules::Side::party, {1, 3}, wielder},
                                    {"Kobold", rules::Side::opposition, {3, 1}, creature(20, 0)}};
    auto barred = arrange(armed, 71, {}, 100, GoldBoxGame::pools_of_darkness, false, true);
    until_turn_of(*barred.session, "Cleric");
    check(!cast_command(*barred.session, 1) && !use_command(*barred.session, 0, 65),
          "Pools of Darkness's no-magic fight bars Cast and Use");
    auto open = arrange(armed, 71, {}, 100, GoldBoxGame::pools_of_darkness);
    until_turn_of(*open.session, "Cleric");
    check(use_command(*open.session, 0, 65).has_value(), "Another fight lets the wand be used");
}

// A scroll is read once its spells are known, each spell once (magic.pas).
void scrolls_are_read()
{
    auto reader = creature(30);
    reader.items = {{0, true, true, 11, false, {15, 0, 0}, 0, 0},
                    {1, true, true, 11, true, {15, 15, 0}, 0, 0}};
    auto fight = arrange({{"Reader", rules::Side::party, {0, 3}, reader},
                          {"Target", rules::Side::opposition, {5, 3}, creature(100, 0)}},
                         163);
    auto &session = *fight.session;
    until_turn_of(session, "Reader");
    check(!use_command(session, 0, 15) && use_command(session, 1, 15).has_value(),
          "Only the known scroll is offered");
    session.submit(*use_command(session, 1, 15));
    session.submit(*aim_at(session, "Target"));
    check(left_by_fight(session, "Reader").items[1].spells == std::array<int, 3>{0, 15, 0},
          "One of its two Magic Missiles is read");
}

// Everyone plays by the original's AI (Quick) until the condition holds or
// the rounds run out.
void all_quick(rules::CombatSession &session, const std::function<bool()> &done, unsigned rounds)
{
    play_until(
        session,
        [&](const std::string &)
        {
            return order(session, "quick");
        },
        [&]
        {
            return done() || session.snapshot().round > rounds;
        },
        2000);
}

// A fight played wholly by the AI ends, and the same dice play it the same
// (combatai.pas, CombatAiControl).
void the_ai_fights_to_the_end()
{
    const auto play = [](std::uint64_t seed)
    {
        auto kobold = creature(8, 0);
        kobold.morale = 0x80 + 50;
        auto fight = arrange({{"Fighter", rules::Side::party, {0, 2}, creature(20)},
                              {"Ranger", rules::Side::party, {0, 5}, creature(20)},
                              {"Kobold 1", rules::Side::opposition, {7, 1}, kobold},
                              {"Kobold 2", rules::Side::opposition, {7, 4}, kobold},
                              {"Kobold 3", rules::Side::opposition, {7, 6}, kobold}},
                             seed);
        auto &session = *fight.session;
        all_quick(
            session,
            [&]
            {
                return session.snapshot().outcome != rules::Outcome::ongoing;
            },
            60);
        check(session.snapshot().outcome != rules::Outcome::ongoing, "The AI's fight ends");
        check(logged(session, "{attacker} hits {name} for {damage}.") > 0, "Blows land");
        return session.snapshot().log();
    };
    check(play(173) == play(173), "The same dice play the same fight");
}

// A lone magic-user with nothing in reach guards rather than walk (combatai.pas, AiMoveStep).
void a_lone_mage_stays()
{
    using original::ClassSlot;
    auto mage = caster(ClassSlot::magic_user, 1, {});
    mage.morale = 0x80 + 50;
    auto fight = arrange({{"Fighter", rules::Side::party, {0, 3}, creature(20)},
                          {"Mage", rules::Side::opposition, {7, 3}, mage}},
                         179);
    auto &session = *fight.session;
    until_turn_of(session, "Mage");
    session.submit(order(session, "quick"));
    check(view_of(session.snapshot(), "Mage").cell == rules::Cell{7, 3} &&
              logged(session, "{name} guards.") == 1,
          "The mage guards where it stands");
}

// A monster caster casts at the party (combatai.pas, SelectSpellToCast).
void a_monster_casts()
{
    using original::ClassSlot;
    auto shaman = caster(ClassSlot::magic_user, 3, {15, 15, 15});
    shaman.morale = 0x80 + 50;
    auto fight = arrange({{"Fighter", rules::Side::party, {0, 3}, creature(50)},
                          {"Shaman", rules::Side::opposition, {6, 3}, shaman}},
                         181);
    auto &session = *fight.session;
    all_quick(
        session,
        [&]
        {
            return logged(session, "{name} casts {spell}.") > 0;
        },
        10);
    check(logged(session, "{name} casts {spell}.") > 0 &&
              view_of(session.snapshot(), "Fighter").hit_points < 50,
          "The shaman's missiles strike the fighter");
}

// The 256-color engine's computer (Treasures' and Pools of Darkness's) casts
// its spell of highest AI weight that has a target: Lightning Bolt (7) before
// Charm Person and Magic Missile (5), which it memorized first (their
// combatai.pas, PickCastableSpell).
void treasures_ai_casts_its_weightiest_spell(GoldBoxGame game)
{
    using original::ClassSlot;
    auto sorceress = caster(ClassSlot::magic_user, 5, {10, 15, 51});
    sorceress.morale = 0x80 + 50;
    auto fight = arrange({{"Fighter", rules::Side::party, {0, 3}, creature(50)},
                          {"Sorceress", rules::Side::opposition, {6, 3}, sorceress}},
                         181, {}, 100, game);
    auto &session = *fight.session;
    const auto first_spell = [&]
    {
        for (const auto &entry : session.snapshot().log_entries)
        {
            if (entry.english.starts_with("Sorceress begins casting ") ||
                entry.english.starts_with("Sorceress casts "))
            {
                return entry.english;
            }
        }
        return std::string();
    };
    all_quick(
        session,
        [&]
        {
            return !first_spell().empty();
        },
        10);
    check(first_spell().find("Lightning Bolt") != std::string::npos,
          "The sorceress casts Lightning Bolt first: " + first_spell());
}

// How many spells a Dark Queen sorceress (magic-user 5, Magic Missile only,
// 5 squares from a lone hero) casts or uses in 10 rounds, the hero and her
// own record adjusted first.
int native_sorceress_casts(std::uint64_t seed,
                           const std::function<void(original::Combatant &)> &adjust_hero,
                           const std::function<void(original::Combatant &)> &adjust_sorceress)
{
    using original::ClassSlot;
    auto hero = victim(500);
    hero.attacks = {};
    hero.movement = 0;
    adjust_hero(hero);
    auto sorceress = caster(ClassSlot::magic_user, 5, {15});
    sorceress.morale = 0x80 + 50;
    sorceress.movement = 0;
    adjust_sorceress(sorceress);
    auto fight = arrange({{"Hero", rules::Side::party, {0, 3}, hero},
                          {"Sorceress", rules::Side::opposition, {5, 3}, sorceress}},
                         seed, {}, 100, GoldBoxGame::dark_queen_of_krynn);
    auto &session = *fight.session;
    all_quick(
        session,
        []
        {
            return false;
        },
        10);
    return logged(session, "{name} casts {spell}.") + logged(session, "{name} uses an item.");
}

// How many times a Dark Queen sorceress casts her one area spell in 5 rounds
// down a corridor at a hero with her own friends 3 squares either side of
// him, out of doors or under water.
int native_corridor_casts(std::uint64_t seed, int spell, bool outdoors, bool underwater)
{
    using original::ClassSlot;
    const auto still = [](original::Combatant c)
    {
        c.attacks = {};
        c.movement = 0;
        return c;
    };
    auto sorceress = caster(ClassSlot::magic_user, 9, {spell});
    sorceress.morale = 0x80 + 50;
    auto fight =
        arrange({{"Sorceress", rules::Side::opposition, {0, 1}, still(sorceress)},
                 {"Left", rules::Side::opposition, {3, 1}, still(victim(500))},
                 {"Hero", rules::Side::party, {6, 1}, still(victim(500))},
                 {"Right", rules::Side::opposition, {9, 1}, still(victim(500))}},
                seed, {}, 100, GoldBoxGame::dark_queen_of_krynn, outdoors, false,
                [underwater](rules::Encounter &encounter)
                {
                    encounter.underwater = underwater;
                    std::vector<rules::Terrain> ground(12 * 3, rules::Terrain::obstacle);
                    std::fill(ground.begin() + 12, ground.begin() + 24, rules::Terrain::open);
                    encounter.battlefield = {12, 3, ground};
                });
    auto &session = *fight.session;
    all_quick(
        session,
        []
        {
            return false;
        },
        5);
    return logged(session, "{name} casts {spell}.");
}

// Dark Queen's area aim takes each burst as the spell then goes off
// (cbaction.c, area_aim, 2075-2082): a Fireball's 1 square less out of doors,
// an Ice Storm's 3 squares under water. Indoors every square a Fireball's
// burst of 3 takes the hero from reaches one of her friends, out of doors
// one does not; an Ice Storm's burst of 2 finds such a square, under water
// it does not.
void native_area_aim_takes_bursts_as_they_go_off()
{
    constexpr int fireball = 47, ice_storm = 187;
    int indoors = 0, outside = 0, dry = 0, underwater = 0;
    for (std::uint64_t seed = 1; seed <= 4; ++seed)
    {
        indoors += native_corridor_casts(seed, fireball, false, false);
        outside += native_corridor_casts(seed, fireball, true, false);
        dry += native_corridor_casts(seed, ice_storm, false, false);
        underwater += native_corridor_casts(seed, ice_storm, false, true);
    }
    check(indoors == 0, "Indoors she keeps her Fireball off her friends");
    check(outside > 0, "Out of doors she finds the square that takes the hero alone");
    check(dry > 0, "Her Ice Storm finds the square that takes the hero alone");
    check(underwater == 0, "Under water its wider burst would take her friends");
}

// Dark Queen's computer weighs its spells as the 256-color engine's does
// (aiturn.c, try_spell and spell_pick; cbaction.c, rank_target): the
// weightiest with a target first; never above four times its knight,
// fighter, paladin and ranger levels on 1d100; no spell at a target a globe
// keeps it off or of magic resistance above 130, and never an item.
void native_ai_weighs_its_spells()
{
    const auto none = [](original::Combatant &)
    {
    };
    int plain = 0, knight = 0, globed = 0, resisting = 0, resisting_less = 0, wand = 0;
    for (std::uint64_t seed = 1; seed <= 6; ++seed)
    {
        plain += native_sorceress_casts(seed, none, none);
        knight += native_sorceress_casts(
            seed, none,
            [](original::Combatant &c)
            {
                c.class_levels[static_cast<std::size_t>(original::ClassSlot::monk)] = 25;
            });
        globed += native_sorceress_casts(
            seed,
            [](original::Combatant &c)
            {
                c.effects.push_back(
                    {original::canonical_effect(63, GoldBoxGame::dark_queen_of_krynn), 0, 255,
                     false});
            },
            none);
        resisting += native_sorceress_casts(
            seed,
            [](original::Combatant &c)
            {
                c.magic_resistance = 131;
            },
            none);
        resisting_less += native_sorceress_casts(
            seed,
            [](original::Combatant &c)
            {
                c.magic_resistance = 60;
            },
            none);
        wand +=
            native_sorceress_casts(seed, none,
                                   [](original::Combatant &c)
                                   {
                                       c.spells.clear();
                                       c.items = {{0, true, false, 14, true, {47, 0, 0}, 30, 0}};
                                   });
    }
    check(plain > 0, "A native sorceress casts Magic Missile at the hero");
    check(knight == 0, "One with 25 knight levels never passes her 1d100 above 100");
    check(globed == 0, "She casts no first-level spell at a hero under a Minor Globe");
    check(resisting == 0, "Nor at one whose magic resistance is above 130");
    check(resisting_less > 0, "Magic resistance of 60 only lowers her rating of him");
    check(wand == 0, "She never uses an item");
    const auto fireball = [](original::Combatant &c)
    {
        c.spells = {47};
    };
    int burst = 0, burst_globed = 0;
    for (std::uint64_t seed = 1; seed <= 6; ++seed)
    {
        burst += native_sorceress_casts(seed, none, fireball);
        burst_globed += native_sorceress_casts(
            seed,
            [](original::Combatant &c)
            {
                c.effects.push_back(
                    {original::canonical_effect(57, GoldBoxGame::dark_queen_of_krynn), 0, 255,
                     false});
            },
            fireball);
    }
    check(burst > 0, "She aims a Fireball at the hero");
    check(burst_globed == 0, "But no Fireball where a Globe of Invulnerability is all it takes");
}

// A pre-trait Native checkpoint still carries its hero identity or monster
// immunity record. Derive the AI's Charm/Hold eligibility without rewriting
// it. Formats 29 and 30 retain their existing canonical upgrade to 31.
void native_legacy_mind_targets_preserve_checkpoint_bytes()
{
    constexpr auto game = GoldBoxGame::dark_queen_of_krynn;
    constexpr int mind = 0x2000;
    const std::shared_ptr<const rules::CharacterRules> creation =
        original::character_rules(original::default_aging, game);
    CharacterCreator creator(creation, 45);
    creator.select(rules::CreationField::race, "krynn_human");
    creator.select(rules::CreationField::character_class, "fighter");
    creator.name("Legacy hero");
    test::walk_original_creation(creator);
    const auto character = creator.create_character();
    original::Content profile_content;
    profile_content.game = game;
    const auto profile_module = original::rules_module(profile_content);
    const auto profile = profile_module->character_profile(character.sheet(), {});
    const auto current = original::Combatant::decode(profile.data);
    check(current && current->native_progression && (current->traits & mind),
          "The fixture starts with an actual current Native hero profile");

    // Both hero profile versions, then susceptible and immune humanoid
    // monsters. Their nonzero race must not override a monster's immunity.
    for (int kind = 0; kind < 4; ++kind)
    {
        for (const int spell : {181, 194}) // Charm Monsters, Hold Monster
        {
            original::Content content;
            content.game = game;
            auto old = *current;
            old.traits &= ~mind;
            if (kind == 1)
            {
                old.native_progression.reset(); // Released combatant14 hero.
            }
            if (kind >= 2)
            {
                auto record = kobold_record();
                record.game = game;
                record.race = 6; // Native human, including the immune control.
                record.target_flags = kind == 2 ? 0x02 : 0;
                old = original::combatant_from_record(record);
                old.traits &= ~mind;
                check(old.race != 0, "The monster control is a humanoid");
                content.bestiary["Legacy target"] = old;
                content.gear["Legacy target"] = {record, {}};
            }
            Fight fight{original::rules_module(std::move(content)), nullptr};
            rules::Encounter encounter;
            encounter.battlefield = {8, 8, std::vector<rules::Terrain>(64, rules::Terrain::open)};
            rules::Participant target;
            target.id = 1;
            target.name = target.definition = "Legacy target";
            target.side = rules::Side::party;
            target.cell = {0, 3};
            if (kind < 2)
            {
                target.character_profile = old.encode();
                target.character_record = profile.record;
            }
            encounter.participants.push_back(std::move(target));
            rules::Participant sorceress;
            sorceress.id = 2;
            sorceress.name = "Sorceress";
            sorceress.side = rules::Side::opposition;
            sorceress.cell = {5, 3};
            auto spells_only = caster(original::ClassSlot::magic_user, 9, {spell});
            // Quick permits NPC magic through its morale byte. A player-style
            // morale of zero would reject casting before rating any target.
            spells_only.morale = 0x80 + 50;
            spells_only.movement = 0;
            spells_only.attacks = {};
            sorceress.character_profile = spells_only.encode();
            encounter.participants.push_back(std::move(sorceress));
            fight.session = fight.module->create(std::move(encounter), 103);
            const auto canonical = fight.session->save();
            check((left_by_fight(*fight.session, "Legacy target").traits & mind) == 0,
                  "The synthetic legacy payload genuinely lacks the new trait");

            for (const int format : {29, 30, 31})
            {
                auto legacy = canonical;
                constexpr std::string_view header = "OGORIGINALCOMBAT 31";
                const auto header_at = legacy.find(header);
                check(header_at != std::string::npos, "The fixture uses Native checkpoint31");
                if (format == 29)
                {
                    // The existing 8x8 Native layout: no plants or heights,
                    // and the empty cost count follows 72 spaced values.
                    std::size_t at = 0;
                    for (int value = 0; value < 72; ++value)
                    {
                        at = legacy.find(' ', at) + 1;
                    }
                    check(legacy.compare(at, 2, "0 ") == 0, "The legacy fixture has no costs");
                    legacy.erase(at, 2);
                }
                else if (format == 30)
                {
                    auto context = header_at + header.size() + 1;
                    for (int dimension = 0; dimension < 2; ++dimension)
                    {
                        context = legacy.find(' ', context) + 1;
                    }
                    check(legacy.compare(context, 4, "0 0 ") == 0,
                          "The old generic layout has neither Native context field");
                    legacy.erase(context, 4);
                }
                legacy.replace(header_at, header.size(),
                               "OGORIGINALCOMBAT " + std::to_string(format));
                auto restored = fight.module->restore(legacy);
                check(
                    restored->save() == canonical,
                    "Legacy Native mind eligibility leaves the established resave bytes unchanged: " +
                        std::to_string(format));
                std::string decisions;
                play_until(
                    *restored,
                    [&](const std::string &actor)
                    {
                        auto command = order(*restored, actor == "Sorceress" ? "quick" : "end");
                        decisions += "\nround " + std::to_string(restored->snapshot().round) + " " +
                                     actor + ": " + command.verb;
                        return command;
                    },
                    [&]
                    {
                        return logged(*restored, "{name} casts {spell}.") > 0 ||
                               restored->snapshot().round > 5;
                    });
                const auto state = restored->snapshot();
                std::string context = "\nround " + std::to_string(state.round) + " outcome " +
                                      std::to_string(static_cast<int>(state.outcome)) + decisions;
                for (const auto &entry : state.log_entries)
                {
                    context += "\n" + entry.english;
                }
                check(
                    (logged(*restored, "{name} casts {spell}.") > 0) == (kind != 3),
                    "Restored Charm/Hold targets eligible minds and preserves monster immunity: " +
                        std::to_string(format) + "/" + std::to_string(kind) + "/" +
                        std::to_string(spell) + context);
            }
        }
    }
}

// A creature an outlook reaches.
const rules::CreatureOutlook &outlook_for(const rules::AreaOutlook &outlook, rules::EntityId id)
{
    const auto found = std::ranges::find(outlook.creatures, id, &rules::CreatureOutlook::id);
    check(found != outlook.creatures.end(), "The outlook reaches " + std::to_string(id));
    return *found;
}

bool reaches(const rules::AreaOutlook &outlook, rules::EntityId id)
{
    return std::ranges::find(outlook.creatures, id, &rules::CreatureOutlook::id) !=
           outlook.creatures.end();
}

double expected_loss(const rules::CreatureOutlook &creature)
{
    double expected = 0;
    for (std::size_t lost = 0; lost < creature.loss.size(); ++lost)
    {
        expected += static_cast<double>(lost) * creature.loss[lost];
    }
    return expected;
}

bool near(double first, double second)
{
    return std::abs(first - second) < 1e-9;
}

rules::EntityId id_of(const rules::CombatSession &session, std::string_view name)
{
    return view_of(session.snapshot(), name).id;
}

// How much a party mage's Fireball is expected to take from a Dark Queen
// White Robe foe (save 10) whose moon is in a given phase.
double native_robed_foe_loss(int phase)
{
    using original::ClassSlot;
    constexpr int casting_moon = 1700 + 250, white_robe = 1;
    auto robe = victim(100);
    robe.level = 10;
    robe.class_levels[static_cast<std::size_t>(ClassSlot::magic_user)] = 10;
    robe.saving_throws = {10, 10, 10, 10, 10};
    robe.effects.push_back({casting_moon, 0, 4 * white_robe + phase, false});
    auto fight =
        arrange({{"Mage", rules::Side::party, {0, 0}, caster(ClassSlot::magic_user, 5, {47})},
                 {"Robe", rules::Side::opposition, {6, 5}, robe}},
                103, {}, 100, GoldBoxGame::dark_queen_of_krynn);
    const auto &session = *fight.session;
    const auto outlook = session.area_outlook(id_of(session, "Mage"), 47, {6, 5});
    check(outlook.has_value(), "The Fireball's outlook reaches the robe");
    return expected_loss(outlook_for(*outlook, id_of(session, "Robe")));
}

// A Dark Queen mage of a robe saves 1 worse in its moon's phase 0 and 1
// better in phase 2 (fxcore.c, fx_saving_throw), as its caster level
// follows the same phases.
void native_robed_mages_save_by_their_moon()
{
    const double phase_0 = native_robed_foe_loss(0), phase_1 = native_robed_foe_loss(1),
                 phase_2 = native_robed_foe_loss(2), phase_3 = native_robed_foe_loss(3);
    check(near(phase_1, phase_3), "Phases 1 and 3 leave the save alone");
    check(phase_0 > phase_1, "Phase 0 makes the robe's save worse");
    check(phase_2 < phase_1, "Phase 2 makes it better");
}

// The chance a party mage's Stinking Cloud sickens a Dark Queen foe (save
// 10 against poison) that bears the given native effect, or none.
double native_foe_sickened(int native_effect)
{
    using original::ClassSlot;
    auto foe = victim(100);
    foe.level = 10;
    foe.saving_throws = {10, 10, 10, 10, 10};
    if (native_effect != 0)
    {
        foe.effects.push_back(
            {original::canonical_effect(native_effect, GoldBoxGame::dark_queen_of_krynn), 0, 255,
             false});
    }
    auto fight =
        arrange({{"Mage", rules::Side::party, {0, 0}, caster(ClassSlot::magic_user, 5, {34})},
                 {"Foe", rules::Side::opposition, {3, 3}, foe}},
                103, {}, 100, GoldBoxGame::dark_queen_of_krynn);
    const auto &session = *fight.session;
    const auto outlook = session.area_outlook(id_of(session, "Mage"), 34, {3, 3});
    check(outlook.has_value(), "The cloud's outlook reaches the foe");
    return outlook_for(*outlook, id_of(session, "Foe")).disabled;
}

// Dark Queen's saving throw takes its effects' bonuses from trigger group
// 0Ch (fxcore.c, fx_trigger_group): Barkskin (13) saves 1 better against
// what is not magic (effects.c, fx_barkskin). A magic stone (78) is not in
// the group, so it does not help as Secret's luckstone does.
void native_save_bonuses_follow_their_trigger_group()
{
    constexpr int barkskin = 13, magic_stone = 78;
    const double plain = native_foe_sickened(0);
    check(plain > 0, "The cloud may sicken the foe");
    check(native_foe_sickened(barkskin) < plain, "Barkskin saves 1 better against the cloud");
    check(near(native_foe_sickened(magic_stone), plain), "A magic stone does not help the save");
}

// A party member's outlook for its Fireball (docs/ORIGINAL-RULES.md, Party
// area spells): each creature's 5d6, rolled once for all, weighed with its
// save, the whole on a failed one and half on a made one; a friend's Hit
// Points as they are, a foe's estimated from its Hit Dice (4.5 a die).
void a_fireball_outlook_weighs_each_save()
{
    using original::ClassSlot;
    auto fight =
        arrange({{"Mage", rules::Side::party, {0, 0}, caster(ClassSlot::magic_user, 5, {47})},
                 {"Friend", rules::Side::party, {5, 4}, victim(100)},
                 {"Foe", rules::Side::opposition, {6, 5}, victim(100)}},
                103);
    const auto &session = *fight.session;
    const auto outlook = session.area_outlook(id_of(session, "Mage"), 47, {6, 5});
    check(outlook.has_value() && outlook->creatures.size() == 2,
          "Fireball's outlook reaches the friend and the foe");
    // Each way 5d6 falls; a victim saves only on a natural 20.
    double whole = 0, half = 0;
    std::vector<double> totals(31, 0.0);
    for (int way = 0; way < 7776; ++way)
    {
        int total = 0;
        for (int die = 0, rest = way; die < 5; ++die, rest /= 6)
        {
            total += rest % 6 + 1;
        }
        whole += total / 7776.0;
        half += (total / 2) / 7776.0;
        totals[static_cast<std::size_t>(total)] += 1 / 7776.0;
    }
    const auto &ally = outlook_for(*outlook, id_of(session, "Friend"));
    check(ally.ally && ally.hit_points == 100 && ally.max_hit_points == 100 &&
              near(expected_loss(ally), 0.95 * whole + 0.05 * half) &&
              near(ally.loss[30], 0.95 * totals[30]),
          "The friend loses the whole 5d6 unless it saves, half if it does");
    const auto &foe = outlook_for(*outlook, id_of(session, "Foe"));
    check(!foe.ally && foe.hit_points == 5 && foe.max_hit_points == 5,
          "A foe of one Hit Die is taken for 5 Hit Points, not its own 100");
    double felled_saving = 0;
    for (int total = 10; total <= 30; ++total)
    {
        felled_saving += totals[static_cast<std::size_t>(total)];
    }
    check(near(outlook->every_enemy_down, 0.95 + 0.05 * felled_saving),
          "The foe goes down unless it saves and the dice fall under 10");
    const auto judgement = rules::judge_area_spell(*outlook, {});
    check(judgement.cast && !judgement.hurts_party_badly,
          "A friend's light burn is worth the foe's likely fall");
}

// Cone of Cold reaches a friend between the caster and its aim, and a
// Lightning Bolt runs on past its aim into a friend there, each as the
// rules' own shapes draw them (path_area, bolt_walk).
void cone_and_bolt_outlooks_reach_friends()
{
    using original::ClassSlot;
    auto fight =
        arrange({{"Mage", rules::Side::party, {0, 3}, caster(ClassSlot::magic_user, 10, {192, 51})},
                 {"Friend", rules::Side::party, {2, 3}, victim(30)},
                 {"Foe", rules::Side::opposition, {4, 3}, victim(200)},
                 {"Beyond", rules::Side::party, {6, 3}, victim(30)}},
                211, {}, 100, GoldBoxGame::curse_of_the_azure_bonds);
    const auto &session = *fight.session;
    const auto mage = id_of(session, "Mage"), ally = id_of(session, "Friend"),
               foe = id_of(session, "Foe"), beyond = id_of(session, "Beyond");
    const auto cone = session.area_outlook(mage, 192, {4, 3});
    check(cone && reaches(*cone, foe) && reaches(*cone, ally) && !reaches(*cone, beyond) &&
              expected_loss(outlook_for(*cone, ally)) > 15,
          "Cone of Cold aimed at the foe reaches the friend before it");
    const auto bolt = session.area_outlook(mage, 51, {4, 3});
    check(bolt && reaches(*bolt, foe) && reaches(*bolt, beyond) && !reaches(*bolt, ally) &&
              expected_loss(outlook_for(*bolt, beyond)) > 15,
          "Lightning Bolt aimed at the foe runs on into the friend beyond it");
    check(!rules::judge_area_spell(*cone, {}).cast && !rules::judge_area_spell(*bolt, {}).cast,
          "Neither is cast so: the friends would likely go down");
}

// A party mage on Quick, with Quick magic, plays its turns while the others
// end theirs, until it casts or six rounds pass, under these limits when
// given (the Options reach the fight through the rules module): how many
// spells it cast, and its friend's Hit Points left.
std::pair<int, int> quick_mage_plays(Fight fight, std::optional<rules::AreaSpellLimits> limits)
{
    if (limits)
    {
        fight.module->limit_party_area_spells(*limits);
        fight.session = fight.module->restore(fight.session->save());
    }
    auto &session = *fight.session;
    play_until(
        session,
        [&](const std::string &actor)
        {
            // A spell coming due is aimed by Quick.
            const bool aiming = !offered(session, "quick_magic");
            return order(session, actor != "Mage" ? "end" : aiming ? "quick" : "quick_magic");
        },
        [&]
        {
            return logged(session, "{name} casts {spell}.") > 0 || session.snapshot().round > 6;
        });
    return {logged(session, "{name} casts {spell}."),
            view_of(session.snapshot(), "Friend").hit_points};
}

// Limits that let a party member risk anything.
constexpr rules::AreaSpellLimits reckless{1.0, 0.0, 0.0, 0.0, 1.0};

// A party mage on Quick with only Cone of Cold, its friend between it and
// the foe in a corridor, finds no aim its player's limits allow and casts
// none; the original's AI cones through a friend (Cone of Cold keeps none
// clear). With limits that risk anything the same mage cones them both.
void a_quick_mage_keeps_its_cone_off_a_friend()
{
    using original::ClassSlot;
    std::vector<rules::Cell> walls;
    for (int x = 0; x < 8; ++x)
    {
        walls.push_back({x, 2});
        walls.push_back({x, 4});
    }
    const auto fight = [&]
    {
        return arrange({{"Mage",
                         rules::Side::party,
                         {0, 3},
                         caster(ClassSlot::magic_user, 10, {192, 192, 192})},
                        {"Friend", rules::Side::party, {1, 3}, victim(30)},
                        {"Foe", rules::Side::opposition, {3, 3}, victim(200)}},
                       223, walls, 100, GoldBoxGame::curse_of_the_azure_bonds);
    };
    const auto [cast, friend_left] = quick_mage_plays(fight(), std::nullopt);
    check(cast == 0 && friend_left == 30, "A Quick mage casts no Cone of Cold through its friend");
    const auto [reckless_cast, reckless_friend_left] = quick_mage_plays(fight(), reckless);
    check(reckless_cast > 0 && reckless_friend_left < 30,
          "With limits that risk anything it cones its friend too");
}

// Treasures' computer (the 256-color engine's) aims a Lightning Bolt at the
// enemy it singles out, and the bolt runs on beyond; a party mage on Quick
// weighs the friend beyond it and casts none, or with limits that risk
// anything, bolts them both.
void a_treasures_party_mage_keeps_its_bolt_off_a_friend()
{
    using original::ClassSlot;
    const auto fight = [&]
    {
        return arrange(
            {{"Mage", rules::Side::party, {0, 3}, caster(ClassSlot::magic_user, 5, {51, 51})},
             {"Foe", rules::Side::opposition, {4, 3}, victim(200)},
             {"Friend", rules::Side::party, {5, 3}, victim(12)}},
            239, {}, 100, GoldBoxGame::treasures_of_the_savage_frontier);
    };
    const auto [cast, friend_left] = quick_mage_plays(fight(), std::nullopt);
    check(cast == 0 && friend_left == 12,
          "A Quick mage casts no bolt that runs on into its friend");
    const auto [reckless_cast, reckless_friend_left] = quick_mage_plays(fight(), reckless);
    check(reckless_cast > 0 && reckless_friend_left < 12,
          "With limits that risk anything it bolts its friend too");
}

// A party mage on Quick weighs its Wand of Fireballs as the wand casts it,
// at level 6 (range 16), not at its own level 10 (range 20): with the only
// foe beyond the wand's reach it leaves the wand alone, where it once took
// the wand up on its own range and gave the aim up on the wand's; with the
// foe within reach it fires.
void a_quick_mage_weighs_a_wand_at_the_wands_level()
{
    using original::ClassSlot;
    const auto plays = [](rules::Cell foe, std::uint64_t seed)
    {
        auto mage = caster(ClassSlot::magic_user, 10, {});
        mage.items = {{0, true, false, 14, true, {47, 0, 0}, 30, 0}};
        auto fight =
            arrange({{"Mage", rules::Side::party, {0, 3}, mage},
                     {"Foe", rules::Side::opposition, foe, victim(200)}},
                    seed, {}, 100, GoldBoxGame::curse_of_the_azure_bonds, false, false,
                    [](rules::Encounter &encounter)
                    {
                        encounter.battlefield = {
                            24, 8, std::vector<rules::Terrain>(24 * 8, rules::Terrain::open)};
                    });
        auto &session = *fight.session;
        until_turn_of(session, "Mage");
        session.submit(order(session, "quick_magic"));
        return std::pair{logged(session, "{name} uses an item."),
                         logged(session, "{name} aborts the spell.")};
    };
    int far_uses = 0, far_aborts = 0, near_uses = 0, near_aborts = 0;
    for (std::uint64_t seed = 1; seed <= 20; ++seed)
    {
        const auto [uses, aborts] = plays({21, 3}, seed);
        far_uses += uses;
        far_aborts += aborts;
        const auto [near_use, near_abort] = plays({14, 3}, seed);
        near_uses += near_use;
        near_aborts += near_abort;
    }
    check(far_uses == 0 && far_aborts == 0,
          "A Quick mage takes up no Wand of Fireballs whose own reach falls short of the foe");
    check(near_uses > 0 && near_aborts == 0, "With the foe within the wand's reach it fires");
}

// Secret's Dispel Magic tries each creature in its area once, as the rules
// play it (the original tried its first target again and again:
// docs/ORIGINAL-RULES.md, Bugs), and the outlook weighs each so: a friend's
// Haste lost is a cost, a held friend freed and a foe's Haste lost gains.
void secret_dispel_outlook_weighs_each_creature()
{
    using original::ClassSlot;
    const auto with = [](int code)
    {
        auto c = creature(30, 1);
        c.effects = {{code, 5, 7, false}};
        return c;
    };
    const auto start = [&](int cleric_level)
    {
        return arrange(
            {{"Cleric", rules::Side::party, {0, 3}, caster(ClassSlot::cleric, cleric_level, {41})},
             {"Hasted", rules::Side::party, {4, 3}, with(39)},
             {"Held", rules::Side::party, {4, 4}, with(52)},
             {"Foe", rules::Side::opposition, {5, 3}, with(39)}},
            229, {}, 100, GoldBoxGame::secret_of_the_silver_blades);
    };
    auto even = start(7);
    const auto &session = *even.session;
    const auto outlook = session.area_outlook(id_of(session, "Cleric"), 41, {4, 3});
    check(outlook.has_value(), "Dispel Magic has an outlook");
    const auto &hasted = outlook_for(*outlook, id_of(session, "Hasted"));
    const auto &held = outlook_for(*outlook, id_of(session, "Held"));
    const auto &foe = outlook_for(*outlook, id_of(session, "Foe"));
    check(near(hasted.helps_lost, 0.5) && near(held.freed, 0.5) && near(foe.helps_lost, 0.5),
          "At the effects' own level each creature keeps each of them half the time");
    check(rules::judge_area_spell(*outlook, {}).cast,
          "Freeing a held friend is worth a friend's Haste");
    auto sure = start(20);
    auto &dispelling = *sure.session;
    const auto certain = dispelling.area_outlook(id_of(dispelling, "Cleric"), 41, {4, 3});
    check(certain && near(outlook_for(*certain, id_of(dispelling, "Foe")).helps_lost, 1.0) &&
              near(outlook_for(*certain, id_of(dispelling, "Held")).freed, 1.0),
          "Far above the effects' level, every one goes");
    cast_now(dispelling, "Cleric", 41,
             [&]
             {
                 return aim_at_cell(dispelling, {4, 3});
             });
    for (const auto *name : {"Hasted", "Held", "Foe"})
    {
        check(left_by_fight(dispelling, name).effects.empty(),
              std::string(name) + " is dispelled too, as the outlook said");
    }
}

// Sleep that would take a friend standing beside an able foe counts as
// felling it, and is not cast so; aimed at the foe alone, it is.
void sleep_outlook_counts_a_friend_beside_a_foe()
{
    using original::ClassSlot;
    auto fight =
        arrange({{"Mage", rules::Side::party, {0, 3}, caster(ClassSlot::magic_user, 1, {21})},
                 {"Friend", rules::Side::party, {4, 3}, creature(8, 1)},
                 {"Foe", rules::Side::opposition, {5, 3}, creature(8, 1)}},
                227);
    const auto &session = *fight.session;
    const auto mage = id_of(session, "Mage"), ally = id_of(session, "Friend"),
               foe = id_of(session, "Foe");
    const auto both = session.area_outlook(mage, 21, {4, 3});
    check(both && near(outlook_for(*both, ally).disabled, 1.0) &&
              outlook_for(*both, ally).enemy_beside && near(outlook_for(*both, foe).disabled, 1.0),
          "Sleep there takes the friend beside the foe, and the foe");
    check(!rules::judge_area_spell(*both, {}).cast,
          "A friend asleep beside a foe is as good as down");
    const auto foe_alone = session.area_outlook(mage, 21, {6, 3});
    check(foe_alone && !reaches(*foe_alone, ally) && rules::judge_area_spell(*foe_alone, {}).cast,
          "Sleep beyond the foe takes the foe alone, and is cast");
}

// A monster's caster keeps the original's AI (combatai.pas, DecideCastSpell):
// Lightning Bolt keeps no friend clear, so it bolts the fighter though its
// own orc stands beyond, where the bolt runs on.
void a_monster_bolts_through_its_own()
{
    using original::ClassSlot;
    auto shaman = caster(ClassSlot::magic_user, 5, {51, 51, 51});
    shaman.morale = 0x80 + 50;
    auto fight = arrange({{"Fighter", rules::Side::party, {3, 3}, creature(50)},
                          {"Orc", rules::Side::opposition, {1, 3}, victim(60)},
                          {"Shaman", rules::Side::opposition, {6, 3}, shaman}},
                         233);
    auto &session = *fight.session;
    play_until(
        session,
        [&](const std::string &actor)
        {
            return order(session, actor == "Shaman" ? "quick" : "end");
        },
        [&]
        {
            return logged(session, "{name} casts {spell}.") > 0 || session.snapshot().round > 10;
        });
    check(logged(session, "{name} casts {spell}.") > 0 &&
              view_of(session.snapshot(), "Orc").hit_points < 60,
          "The shaman's bolt runs on into its own orc");
}

// The AI steps only onto an empty square, so a monster behind another in a
// corridor waits rather than share its square (combatai.pas, AiMoveStep).
void the_ai_never_shares_a_square()
{
    auto kobold = creature(30, 0);
    kobold.morale = 0x80 + 50;
    std::vector<rules::Cell> walls;
    for (int x = 0; x < 8; ++x)
    {
        walls.push_back({x, 2});
        walls.push_back({x, 4});
    }
    auto fight = arrange({{"Fighter", rules::Side::party, {0, 3}, creature(200)},
                          {"Kobold 1", rules::Side::opposition, {5, 3}, kobold},
                          {"Kobold 2", rules::Side::opposition, {6, 3}, kobold}},
                         197, walls);
    auto &session = *fight.session;
    bool shared = false;
    all_quick(
        session,
        [&]
        {
            const auto state = session.snapshot();
            std::set<std::pair<int, int>> taken;
            for (const auto &unit : state.combatants)
            {
                shared =
                    shared || (unit.conscious && !taken.insert({unit.cell.x, unit.cell.y}).second);
            }
            return shared;
        },
        6);
    check(!shared, "No two creatures stand on one square");
    check(logged(session, "{name} guards.") > 0, "The kobold behind waits its turn");
}

// A monster whose turn leaves it stuck with no way to fight takes the
// shortest way round walls (a departure from the original,
// docs/DEVIATIONS.md): a kobold in a cup that opens away from the fighter
// walks out of it and reaches him.
void the_ai_walks_around_walls()
{
    auto kobold = creature(30, 0);
    kobold.morale = 0x80 + 50;
    const std::vector<rules::Cell> cup{{3, 2}, {4, 2}, {5, 2}, {6, 2},
                                       {3, 3}, {3, 4}, {6, 3}, {6, 4}};
    auto fight = arrange({{"Fighter", rules::Side::party, {4, 0}, creature(200)},
                          {"Kobold", rules::Side::opposition, {4, 3}, kobold}},
                         197, cup);
    auto &session = *fight.session;
    const auto beside = [&]
    {
        const auto state = session.snapshot();
        const auto fighter = view_of(state, "Fighter").cell;
        const auto cell = view_of(state, "Kobold").cell;
        return std::max(std::abs(cell.x - fighter.x), std::abs(cell.y - fighter.y)) <= 1;
    };
    play_until(
        session,
        [&](const std::string &)
        {
            const auto state = session.snapshot();
            return order(session, state.actor == view_of(state, "Fighter").id ? "guard" : "quick");
        },
        [&]
        {
            return beside() || session.snapshot().round > 8;
        },
        2000);
    check(beside(), "The kobold walks out of the cup to the fighter");
}

// A party member on Quick casts only with Quick magic on (Curse's reference
// card: ALT M toggles magic for characters set to Quick).
void quick_magic_lets_the_party_cast()
{
    using original::ClassSlot;
    const auto casts = [](const std::string &verb)
    {
        auto fight = arrange(
            {{"Mage", rules::Side::party, {0, 3}, caster(ClassSlot::magic_user, 3, {15, 15, 15})},
             {"Kobold", rules::Side::opposition, {6, 3}, creature(50, 0)}},
            181);
        auto &session = *fight.session;
        play_until(
            session,
            [&](const std::string &)
            {
                return order(session,
                             session.snapshot().actor == view_of(session.snapshot(), "Mage").id
                                 ? verb
                                 : "quick");
            },
            [&]
            {
                return session.snapshot().round > 3;
            },
            2000);
        return logged(session, "{name} casts {spell}.") > 0;
    };
    check(!casts("quick"), "With Quick magic off the mage casts nothing");
    check(casts("quick_magic"), "With Quick magic on the mage casts");
}

// A fleeing monster runs for the edge and gets away (combatai.pas, AiMoveStep).
void a_routed_monster_runs()
{
    auto coward = creature(20, 0);
    coward.hit_points = 2;
    coward.morale = 0x80 + 10;
    coward.movement = 12;
    auto slow = creature(50);
    slow.movement = 3;
    auto fight = arrange({{"Fighter", rules::Side::party, {0, 3}, slow},
                          {"Coward", rules::Side::opposition, {5, 3}, coward}},
                         191, {}, 50);
    auto &session = *fight.session;
    all_quick(
        session,
        [&]
        {
            return view_of(session.snapshot(), "Coward").fled;
        },
        10);
    check(view_of(session.snapshot(), "Coward").fled, "The coward runs off the field");
}

// A cleric on Quick turns undead in sight (combatai.pas, TurnUndeadAi).
void the_ai_turns_undead()
{
    auto fight = arrange({{"Cleric", rules::Side::party, {1, 3}, cleric(8)},
                          {"Skeleton", rules::Side::opposition, {5, 3}, undead(1)}},
                         193);
    auto &session = *fight.session;
    until_turn_of(session, "Cleric");
    session.submit(order(session, "quick"));
    check(view_of(session.snapshot(), "Skeleton").status == "gone",
          "The cleric destroys the skeleton");
}

// A monster of the AI with these effects.
original::Combatant special(std::vector<original::Effect> effects, int hit_points = 30)
{
    auto c = creature(hit_points, 4);
    c.morale = 0x80 + 50;
    c.effects = std::move(effects);
    return c;
}

// The petrifying gaze stones whom it looks at, unless a mirror turns a
// reflectable gaze back (effhndlr.pas, EffPetrifyingGaze).
void a_gaze_stones()
{
    const auto gaze_at = [](original::Combatant fighter)
    {
        auto fight = arrange({{"Fighter", rules::Side::party, {0, 3}, fighter},
                              {"Basilisk",
                               rules::Side::opposition,
                               {6, 3},
                               special({{83, 0, 0, false}, {127, 0, 0, false}})}},
                             197);
        auto &session = *fight.session;
        until_turn_of(session, "Basilisk");
        session.submit(order(session, "quick"));
        return session.snapshot();
    };
    check(view_of(gaze_at(victim()), "Fighter").status == "stoned", "The fighter is stoned");
    auto mirrored = victim();
    mirrored.mirror = true;
    auto basilisk = special({{83, 0, 0, false}, {127, 0, 0, false}});
    basilisk.saving_throws = {21, 21, 21, 21, 21};
    auto fight = arrange({{"Fighter", rules::Side::party, {0, 3}, mirrored},
                          {"Basilisk", rules::Side::opposition, {6, 3}, basilisk}},
                         197);
    auto &session = *fight.session;
    until_turn_of(session, "Basilisk");
    session.submit(order(session, "quick"));
    check(view_of(session.snapshot(), "Basilisk").status == "stoned" &&
              view_of(session.snapshot(), "Fighter").status.empty(),
          "A mirror turns the gaze back on the basilisk");
}

// A dragon breathes lightning in the first round for its maximum Hit Points,
// saved against for half (effhndlr.pas, EffBreatheLightning).
void a_dragon_breathes()
{
    auto dragon = special({{88, 0, 255, false}}, 40);
    dragon.hit_points = 25;
    auto fight = arrange({{"Fighter", rules::Side::party, {2, 3}, victim(100)},
                          {"Dragon", rules::Side::opposition, {5, 3}, dragon}},
                         199);
    auto &session = *fight.session;
    until_turn_of(session, "Dragon");
    session.submit(order(session, "quick"));
    const int lost = 100 - view_of(session.snapshot(), "Fighter").hit_points;
    check(logged(session, "{name} breathes.") == 1 && (lost == 40 || lost == 20),
          "The breath strikes for the dragon's 40, or 20 saved");
}

// The ankheg spits acid once a fight (effhndlr.pas, EffSpitAcid).
void an_ankheg_spits_once()
{
    auto fight = arrange({{"Fighter", rules::Side::party, {3, 3}, victim(500)},
                          {"Ankheg",
                           rules::Side::opposition,
                           {5, 3},
                           special({{121, 0, 0, false}, {80, 0, 0, false}}, 500)}},
                         211);
    auto &session = *fight.session;
    play_until(
        session,
        [&](const std::string &actor)
        {
            return order(session, actor == "Ankheg" ? "quick" : "end");
        },
        [&]
        {
            return session.snapshot().round > 25;
        },
        400);
    check(logged(session, "{name} spits acid.") == 1, "Once, and no more");
}

// Curse's breaths and powers (its reconstruction's ovr023): a black dragon
// breathes acid down a line for its Hit Points, three times a fight; a hell
// hound breathes 7 of fire at one beside it; a beholder at 4 squares fires
// its death ray.
void curse_monsters_breathe()
{
    auto dragon = special({{290, 0, 0, false}}, 40);
    auto fight = arrange({{"Fighter", rules::Side::party, {2, 3}, victim(500)},
                          {"Dragon", rules::Side::opposition, {5, 3}, dragon}},
                         221);
    auto &session = *fight.session;
    play_until(
        session,
        [&](const std::string &actor)
        {
            return order(session, actor == "Dragon" ? "quick" : "end");
        },
        [&]
        {
            return session.snapshot().round > 8;
        },
        400);
    const int lost = 500 - view_of(session.snapshot(), "Fighter").hit_points;
    check(logged(session, "{name} breathes.") == 3 && lost >= 60,
          "A black dragon breathes acid three times, 40 or 20 each, and no more");

    auto hound = special({{331, 0, 0, false}}, 30);
    auto burn = arrange({{"Fighter", rules::Side::party, {3, 3}, victim(500)},
                         {"Hound", rules::Side::opposition, {4, 3}, hound}},
                        223);
    play_until(
        *burn.session,
        [&](const std::string &actor)
        {
            return order(*burn.session, actor == "Hound" ? "quick" : "end");
        },
        [&]
        {
            return logged(*burn.session, "{name} breathes fire.") > 0 ||
                   burn.session->snapshot().round > 10;
        },
        400);
    check(logged(*burn.session, "{name} breathes fire.") > 0, "A hell hound breathes fire");

    auto beholder = special({{287, 0, 0, false}}, 60);
    auto stare = arrange({{"Fighter", rules::Side::party, {1, 3}, victim(500)},
                          {"Beholder", rules::Side::opposition, {5, 3}, beholder}},
                         227);
    until_turn_of(*stare.session, "Beholder");
    stare.session->submit(order(*stare.session, "quick"));
    check(logged(*stare.session, "{name} fires a death ray.") == 1,
          "At 4 squares a beholder fires its death ray");
}

// Curse's conditions (ovr023, ovr013): Fear sends those on its path running;
// Fumble leaves one that fails its save helpless; Dimension Door moves the
// caster to the square aimed at; an animal cannot single out one invisible to
// animals.
void curse_conditions_take_hold()
{
    using original::ClassSlot;
    auto fight = arrange(
        {{"Mage", rules::Side::party, {0, 3}, caster(ClassSlot::magic_user, 9, {184, 186, 183})},
         {"First", rules::Side::opposition, {2, 3}, victim(100)},
         {"Second", rules::Side::opposition, {3, 3}, victim(100)}},
        231);
    auto &session = *fight.session;
    cast_now(session, "Mage", 184,
             [&]
             {
                 return aim_at_cell(session, {2, 3});
             });
    check(logged(session, "{name} is affected by {spell}.") >= 2, "Fear takes both on its path");

    auto fumble =
        arrange({{"Mage", rules::Side::party, {0, 3}, caster(ClassSlot::magic_user, 9, {186})},
                 {"Orc", rules::Side::opposition, {2, 3}, victim(100)}},
                233);
    cast_now(*fumble.session, "Mage", 186,
             [&]
             {
                 return aim_at(*fumble.session, "Orc");
             });
    check(view_of(fumble.session->snapshot(), "Orc").status == "helpless",
          "Fumble leaves one that fails its save helpless");

    auto door =
        arrange({{"Mage", rules::Side::party, {0, 3}, caster(ClassSlot::magic_user, 9, {183})},
                 {"Orc", rules::Side::opposition, {7, 7}, victim(100)}},
                235);
    cast_now(*door.session, "Mage", 183,
             [&]
             {
                 return aim_at_cell(*door.session, {3, 5});
             });
    check(view_of(door.session->snapshot(), "Mage").cell == rules::Cell{3, 5},
          "Dimension Door moves the caster to the square aimed at");

    auto wolf = creature(20);
    wolf.creature_type = 19;
    auto hidden = victim(100);
    hidden.effects.push_back({269, 0, 0, false});
    auto animals = arrange({{"Wolf", rules::Side::party, {2, 3}, wolf},
                            {"Ranger", rules::Side::opposition, {3, 3}, hidden}},
                           237);
    until_turn_of(*animals.session, "Wolf");
    check(!offered(*animals.session, "melee", "Ranger"),
          "An animal cannot single out one invisible to animals");
}

constexpr auto secret_game = GoldBoxGame::secret_of_the_silver_blades;

Fight arrange_secret(const std::vector<Placed> &placed, std::uint64_t seed)
{
    return arrange(placed, seed, {}, 100, secret_game);
}

// The named creature plays its turns by the original's AI and the others end
// theirs, until the condition holds or the rounds pass.
void ai_plays(rules::CombatSession &session, std::string_view name,
              const std::function<bool()> &done, unsigned rounds)
{
    play_until(
        session,
        [&](const std::string &actor)
        {
            return order(session, actor == name ? "quick" : "end");
        },
        [&]
        {
            return done() || session.snapshot().round > rounds;
        },
        400);
}

// Secret's effects by the fight's numbers (canonical_effect): Curse's below
// 57 but for five, the earlier games' where a moved one works as theirs, the
// rest 400 up.
void secret_effects_take_their_own_numbers()
{
    const auto mapped = [](int code)
    {
        return original::canonical_effect(code, secret_game);
    };
    check(mapped(4) == 204 && mapped(37) == 237 && mapped(55) == 55 && mapped(6) == 335 &&
              mapped(7) == 18 && mapped(18) == 124 && mapped(32) == 345,
          "Below 57 Secret's effects are Curse's, but for five");
    check(mapped(24) == 24 && mapped(56) == 56 && mapped(59) == 127 && mapped(73) == 273 &&
              mapped(89) == 89 && mapped(93) == 112 && mapped(96) == 108 && mapped(98) == 310 &&
              mapped(99) == 111 && mapped(103) == 119 && mapped(111) == 342,
          "A moved effect takes the earlier games' number that works as it does");
    check(mapped(58) == 458 && mapped(60) == 460 && mapped(64) == 464 && mapped(79) == 479 &&
              mapped(86) == 486 && mapped(88) == 488 && mapped(104) == 504,
          "Secret's own move 400 up");
}

// Secret's creatures as its files make them (installed_games.h): their
// type and target flags, magic resistance and races, and a fight with them
// that ends.
void secret_records_fight()
{
    const auto secret = opengold::test::installed_game_folder(secret_game);
    if (secret.empty())
    {
        std::cout << "Secret of the Silver Blades' files are not here; its records are not "
                     "checked.\n";
        return;
    }
    auto content = load_original_content(secret, secret_game);
    for (const auto &[key, numbers] : content.bestiary)
    {
        check(original::Combatant::decode(numbers.encode()) == numbers,
              key + " survives its profile's encoding");
    }
    const auto creature_at = [&content](int record) -> const original::Combatant &
    {
        return content.bestiary.at("MON1CHA.DAX:" + std::to_string(record));
    };
    const auto has = [](const original::Combatant &c, int code)
    {
        return std::ranges::any_of(c.effects,
                                   [code](const original::Effect &e)
                                   {
                                       return e.code == code;
                                   });
    };
    const auto &golem = creature_at(17);
    check(has(golem, 460) && has(golem, 479) && (golem.traits & original::trait::unliving) &&
              golem.race == 0,
          "The iron golem needs +3 weapons, takes no magic and is no living monster");
    check(creature_at(9).magic_resistance == 15 && creature_at(29).magic_resistance == 20,
          "The drider and the lizard king resist magic");
    const auto &giant = creature_at(12);
    check((giant.kin & original::kin::giant) && (giant.kin & original::kin::bugbear) &&
              (giant.traits & original::trait::ranger_foe) &&
              (giant.traits & original::trait::giant_bane),
          "A cloud giant is a giant to dwarves, gnomes, rangers and giant-bane weapons");
    const auto &bugbear = creature_at(49);
    check((bugbear.kin & original::kin::gnome_foe) && (bugbear.kin & original::kin::bugbear) &&
              !(bugbear.kin & original::kin::giant),
          "A gnome strikes a bugbear better and it strikes a gnome worse");
    check((creature_at(20).traits & original::trait::dispel_evil) && has(creature_at(20), 480),
          "A hell hound breathes fire and Dispel Evil sends it away");
    check(has(creature_at(33), 458) && has(creature_at(33), 127) && has(creature_at(33), 464),
          "A medusa's gaze stones, a mirror turns it back, and her bite poisons");
    check(creature_at(50).race == 7 && creature_at(43).traits == original::trait::snake,
          "Sieglinda is human, and a giant snake a snake");

    const auto module = original::rules_module(std::move(content));
    rules::Encounter encounter;
    encounter.battlefield = {10, 8, std::vector<rules::Terrain>(80, rules::Terrain::open)};
    auto champion = monster({4, 2, 8, 30}, {0, 0, 0, 0}, {});
    champion.morale = 0;
    rules::Participant member;
    member.id = 1;
    member.name = "Champion";
    member.definition = "member";
    member.cell = {1, 3};
    member.character_profile = champion.encode();
    encounter.participants.push_back(member);
    for (const auto &[n, record] : {std::pair{0, 20}, std::pair{1, 33}, std::pair{2, 42}})
    {
        rules::Participant monster;
        monster.id = 2 + static_cast<rules::EntityId>(n);
        monster.name = "Monster " + std::to_string(n);
        monster.definition = "MON1CHA.DAX:" + std::to_string(record);
        monster.side = rules::Side::opposition;
        monster.cell = {8, 1 + n * 3};
        encounter.participants.push_back(monster);
    }
    auto session = module->create(std::move(encounter), 13);
    all_quick(
        *session,
        [&]
        {
            return session->snapshot().outcome != rules::Outcome::ongoing;
        },
        100);
    check(session->snapshot().outcome != rules::Outcome::ongoing,
          "A fight with a hell hound, a medusa and a giant slug, played by the AI, ends");
}

// An iron golem takes harm only from a weapon of +3 or better; magical fire
// heals it and electricity slows it (effhndlr.pas, EffNeedMagicWeapon and
// EffIronGolem, in Secret's reconstruction).
void an_iron_golem_shrugs_off_harm()
{
    using original::ClassSlot;
    auto golem = victim(100);
    golem.effects = {{460, 0, 0, false}, {479, 0, 0, false}};
    const auto hit = [](const rules::CombatSession &session)
    {
        return logged(session, "{attacker} hits {name} for {damage}.", "Monster") > 0;
    };
    const auto fists = monster({2, 1, 6, 10}, {0, 0, 0, 0}, {});
    auto plain = attack_until(fists, golem, hit, 47, secret_game);
    check(hit(*plain.session) && view_of(plain.session->snapshot(), "Victim").hit_points == 100,
          "Fists do the iron golem no harm");
    auto sword = fists;
    sword.striker = original::StrikingItem{3, false, 0};
    auto magic = attack_until(sword, golem, hit, 47, secret_game);
    check(view_of(magic.session->snapshot(), "Victim").hit_points < 100, "A +3 sword does");

    auto hurt = golem;
    hurt.hit_points = 40;
    auto fire = arrange_secret(
        {{"Mage", rules::Side::party, {0, 0}, caster(ClassSlot::magic_user, 5, {47})},
         {"Golem", rules::Side::opposition, {6, 5}, hurt}},
        103);
    cast_now(*fire.session, "Mage", 47,
             [&]
             {
                 return aim_at_cell(*fire.session, {6, 5});
             });
    check(view_of(fire.session->snapshot(), "Golem").hit_points > 40, "A Fireball heals it");
    auto bolt = arrange_secret(
        {{"Mage", rules::Side::party, {0, 3}, caster(ClassSlot::magic_user, 5, {51})},
         {"Golem", rules::Side::opposition, {2, 3}, golem}},
        127);
    cast_now(*bolt.session, "Mage", 51,
             [&]
             {
                 return aim_at_cell(*bolt.session, {2, 3});
             });
    check(logged(*bolt.session, "{name} is slowed.") > 0 &&
              view_of(bolt.session->snapshot(), "Golem").hit_points == 100,
          "Lightning only slows it");
}

// Secret's touches (fxengine.pas, its conditions 2 and 3): a cockatrice's
// stones, a drider's holds and a centipede's poisons with its only attack; a
// blow that does no damage carries nothing; a remorhaz swallows its victim
// whole on a natural 20.
void secret_touches()
{
    const auto status_is = [](std::string_view status)
    {
        return [status](const rules::CombatSession &session)
        {
            return view_of(session.snapshot(), "Victim").status == status;
        };
    };
    const auto cockatrice = monster({2, 1, 1, 0}, {0, 0, 0, 0}, {{465, 0, 0, false}});
    auto stone = attack_until(cockatrice, victim(), status_is("stoned"), 47, secret_game);
    check(status_is("stoned")(*stone.session), "A cockatrice's touch turns to stone");
    const auto drider = monster({0, 0, 0, 0}, {2, 1, 1, 0}, {{488, 0, 0, false}});
    auto held = attack_until(drider, victim(), status_is("helpless"), 47, secret_game);
    check(status_is("helpless")(*held.session), "A drider's touch paralyzes");

    const auto poisoned = [](const rules::CombatSession &session)
    {
        return logged(session, "{name} is poisoned.") > 0;
    };
    const auto centipede = monster({2, 1, 1, 0}, {0, 0, 0, 0}, {{464, 0, 0, false}});
    auto bitten = attack_until(centipede, victim(), poisoned, 47, secret_game);
    check(poisoned(*bitten.session) && view_of(bitten.session->snapshot(), "Victim").dead,
          "A centipede's only attack poisons");
    const auto toothless = monster({2, 0, 0, 0}, {0, 0, 0, 0}, {{464, 0, 0, false}});
    auto grazed = attack_until(
        toothless, victim(),
        [](const rules::CombatSession &session)
        {
            return session.snapshot().round > 5;
        },
        47, secret_game);
    check(!poisoned(*grazed.session), "A blow that does no damage does not poison");

    const auto remorhaz = monster({10, 1, 1, 0}, {0, 0, 0, 0}, {{466, 0, 0, false}});
    auto swallowed = attack_until(
        remorhaz, victim(),
        [](const rules::CombatSession &session)
        {
            return logged(session, "{name} is swallowed whole.") > 0;
        },
        47, secret_game);
    check(logged(*swallowed.session, "{name} is swallowed whole.") == 1 &&
              view_of(swallowed.session->snapshot(), "Victim").dead,
          "A remorhaz swallows its victim whole");
}

// Secret's monsters' powers at their turns (fxengine.pas, its condition 14):
// a pyrohydra's fire from the second round, a hell hound's at once, the umber
// hulk's confusing gaze, a giant slug's acid, the petrifying gaze within 6
// squares, and a dragon's breath, three a fight and never through its own.
void secret_monsters_breathe()
{
    auto hydra = special({{467, 0, 0, false}}, 300);
    hydra.attacks[0] = {24, 1, 1, 0};
    auto heads = arrange_secret({{"Fighter", rules::Side::party, {3, 3}, victim(500)},
                                 {"Hydra", rules::Side::opposition, {4, 3}, hydra}},
                                241);
    ai_plays(
        *heads.session, "Hydra",
        []
        {
            return false;
        },
        1);
    check(logged(*heads.session, "{name} breathes fire.") == 0, "No fire in the first round");
    ai_plays(
        *heads.session, "Hydra",
        []
        {
            return false;
        },
        2);
    check(logged(*heads.session, "{name} breathes fire.") > 0, "Fire in the second");

    auto hound =
        arrange_secret({{"Fighter", rules::Side::party, {3, 3}, victim(500)},
                        {"Hound", rules::Side::opposition, {4, 3}, special({{480, 0, 0, false}})}},
                       243);
    until_turn_of(*hound.session, "Hound");
    hound.session->submit(order(*hound.session, "quick"));
    check(logged(*hound.session, "{name} breathes fire.") == 1 &&
              view_of(hound.session->snapshot(), "Fighter").hit_points < 500,
          "A hell hound breathes fire at its first turn");

    auto hulk =
        arrange_secret({{"Fighter", rules::Side::party, {3, 3}, victim(500)},
                        {"Hulk", rules::Side::opposition, {4, 3}, special({{470, 0, 0, false}})}},
                       245);
    until_turn_of(*hulk.session, "Hulk");
    hulk.session->submit(order(*hulk.session, "quick"));
    check(logged(*hulk.session, "{name} gazes.") == 1 &&
              logged(*hulk.session, "{name} is affected by {spell}.") == 1,
          "The umber hulk's gaze confuses one beside it");

    auto slug = arrange_secret(
        {{"Fighter", rules::Side::party, {1, 3}, victim(500)},
         {"Slug", rules::Side::opposition, {5, 3}, special({{481, 0, 0, false}}, 300)}},
        247);
    ai_plays(
        *slug.session, "Slug",
        []
        {
            return false;
        },
        3);
    check(logged(*slug.session, "{name} spits acid.") +
                  logged(*slug.session, "{name} spits acid and misses.") >=
              3,
          "A giant slug spits at every turn");
    auto slimy = victim(100);
    slimy.effects = {{490, 0, 0, false}};
    auto club = monster({2, 1, 6, 10}, {0, 0, 0, 0}, {});
    club.striker = original::StrikingItem{0, false, 0x80};
    auto clubbed = attack_until(
        club, slimy,
        [](const rules::CombatSession &session)
        {
            return logged(session, "{attacker} hits {name} for {damage}.", "Monster") > 0;
        },
        47, secret_game);
    check(view_of(clubbed.session->snapshot(), "Victim").hit_points == 100,
          "A club does a giant slug no harm");

    const auto gaze_at = [](rules::Cell fighter, const original::Combatant &numbers)
    {
        auto basilisk = special({{458, 0, 0, false}, {127, 0, 0, false}});
        basilisk.saving_throws = {21, 21, 21, 21, 21};
        auto fight = arrange_secret({{"Fighter", rules::Side::party, fighter, numbers},
                                     {"Basilisk", rules::Side::opposition, {6, 3}, basilisk}},
                                    249);
        until_turn_of(*fight.session, "Basilisk");
        fight.session->submit(order(*fight.session, "quick"));
        return fight;
    };
    auto near = gaze_at({0, 3}, victim());
    check(view_of(near.session->snapshot(), "Fighter").status == "stoned",
          "A gaze 6 squares away stones");
    auto mirrored = victim();
    mirrored.mirror = true;
    auto turned = gaze_at({0, 3}, mirrored);
    check(view_of(turned.session->snapshot(), "Basilisk").status == "stoned",
          "A mirror turns it back");
    auto far = gaze_at({0, 7}, victim());
    check(logged(*far.session, "{name} gazes.") == 0, "Farther away it does not gaze");

    const auto dragon = special({{504, 0, 0, false}}, 40);
    auto red = arrange_secret({{"Fighter", rules::Side::party, {2, 3}, victim(500)},
                               {"Dragon", rules::Side::opposition, {5, 3}, dragon}},
                              251);
    ai_plays(
        *red.session, "Dragon",
        []
        {
            return false;
        },
        8);
    check(logged(*red.session, "{name} breathes.") == 3,
          "A red dragon breathes three times a fight");
    auto shielded = arrange_secret({{"Fighter", rules::Side::party, {2, 3}, victim(500)},
                                    {"Kobold", rules::Side::opposition, {3, 3}, victim(500)},
                                    {"Dragon", rules::Side::opposition, {5, 3}, dragon}},
                                   253);
    until_turn_of(*shielded.session, "Dragon");
    shielded.session->submit(order(*shielded.session, "quick"));
    check(logged(*shielded.session, "{name} breathes.") == 0,
          "It does not breathe through its own side");
}

// Secret judges by a creature's race and flags (magic.pas and effhndlr.pas,
// in its reconstruction): Charm Person takes the player's races and monsters
// its flags call persons, Hold Person no monster, Slay Living none unliving,
// and magic resistance the record's percent; Dispel Evil sends away what its
// flags call evil; a ranger strikes its flagged foes harder from beside them.
void secret_flags_decide()
{
    using original::ClassSlot;
    const auto charmed = [](const original::Combatant &target)
    {
        auto fight = arrange_secret(
            {{"Mage", rules::Side::party, {0, 3}, caster(ClassSlot::magic_user, 5, {10})},
             {"Target", rules::Side::opposition, {4, 3}, target},
             {"Guard", rules::Side::opposition, {7, 7}, victim(100)}},
            83);
        until_turn_of(*fight.session, "Mage");
        fight.session->submit(*cast_command(*fight.session, 10));
        fight.session->submit(*aim_at(*fight.session, "Target"));
        return view_of(fight.session->snapshot(), "Target").side == rules::Side::party;
    };
    auto man = victim(20);
    man.race = 7;
    auto person = victim(20);
    person.traits = original::trait::person;
    auto warded = man;
    warded.magic_resistance = 100;
    check(charmed(man) && charmed(person) && !charmed(victim(20)) && !charmed(warded),
          "Charm Person takes a man or a monster flagged a person, unless it resists magic");

    auto fight = arrange_secret(
        {{"Cleric", rules::Side::party, {0, 3}, caster(ClassSlot::cleric, 9, {23, 23, 176})},
         {"Man", rules::Side::opposition, {4, 3}, man},
         {"Brute", rules::Side::opposition, {4, 5}, victim(20)},
         {"Golem",
          rules::Side::opposition,
          {2, 3},
          []
          {
              auto golem = victim(20);
              golem.traits = original::trait::unliving;
              return golem;
          }()}},
        101);
    auto &session = *fight.session;
    check(!offered(session, "turn"), "Secret has no turning undead");
    for (const auto *name : {"Man", "Brute"})
    {
        cast_now(session, "Cleric", 23,
                 [&]
                 {
                     return aim_at(session, name);
                 });
        session.submit(order(session, "aim_done"));
    }
    check(view_of(session.snapshot(), "Man").status == "helpless" &&
              view_of(session.snapshot(), "Brute").status.empty(),
          "Hold Person holds a man but no monster");
    cast_now(session, "Cleric", 176,
             [&]
             {
                 return aim_at(session, "Golem");
             });
    check(!view_of(session.snapshot(), "Golem").dead &&
              view_of(session.snapshot(), "Golem").hit_points == 20,
          "Slay Living cannot slay the unliving");

    auto paladin = monster({2, 1, 1, 0}, {0, 0, 0, 0}, {{204, 0, 0, false}, {345, 0, 0, false}});
    const auto dispelled = [&paladin](const original::Combatant &foe)
    {
        auto touch = arrange_secret({{"Paladin", rules::Side::party, {3, 3}, paladin},
                                     {"Foe", rules::Side::opposition, {4, 3}, foe},
                                     {"Guard", rules::Side::opposition, {7, 7}, victim(100)}},
                                    257);
        play_until(
            *touch.session,
            [&](const std::string &actor)
            {
                const auto melee = offered(*touch.session, "melee", "Foe");
                return actor == "Paladin" && melee ? *melee : order(*touch.session, "end");
            },
            [&]
            {
                return touch.session->snapshot().round > 4;
            },
            200);
        return logged(*touch.session, "{name} is dispelled.") > 0;
    };
    auto hound = victim(500);
    hound.traits = original::trait::dispel_evil;
    auto villain = victim(500);
    villain.alignment = 8;
    check(dispelled(hound) && !dispelled(villain),
          "Dispel Evil sends away what Secret flags evil, not the evil-aligned");

    auto ranger = monster({2, 1, 1, 0}, {0, 0, 0, 0}, {{334, 0, 0, false}});
    ranger.class_levels[static_cast<std::size_t>(ClassSlot::ranger)] = 10;
    auto ogre = victim(500);
    ogre.traits = original::trait::ranger_foe;
    auto struck = attack_until(
        ranger, ogre,
        [](const rules::CombatSession &session)
        {
            return logged(session, "{attacker} hits {name} for {damage}.", "Monster") > 0;
        },
        47, secret_game);
    check(view_of(struck.session->snapshot(), "Victim").hit_points == 489,
          "A ranger adds its level to a blow at a foe beside it");
}

// Secret's rounds (combat.pas, in its reconstruction): one ordered at the
// round's start, a delay going to the end; a ring of invisibility hides its
// wearer from the start; a dragon's fear sends the weaker of its foes running.
void secret_rounds()
{
    auto swift = creature(50);
    swift.initiative_bonus = 12;
    auto steady = creature(50);
    steady.initiative_bonus = 6;
    auto fight = arrange_secret({{"Swift", rules::Side::party, {0, 1}, swift},
                                 {"Steady", rules::Side::party, {0, 3}, steady},
                                 {"Slow", rules::Side::opposition, {7, 5}, creature(50)}},
                                259);
    auto &session = *fight.session;
    check(actor_name(session) == "Swift", "The highest initiative acts first");
    session.submit(order(session, "delay"));
    std::vector<std::string> acted;
    for (int turn = 0; turn < 3; ++turn)
    {
        acted.push_back(actor_name(session));
        session.submit(order(session, "end"));
    }
    check(acted == std::vector<std::string>{"Steady", "Slow", "Swift"},
          "One that delays acts last of all");

    auto ringed = special({{56, 0, 0, false}});
    auto hidden = arrange_secret({{"Fighter", rules::Side::party, {3, 3}, creature(50)},
                                  {"Dreadlord", rules::Side::opposition, {4, 3}, ringed}},
                                 261);
    until_turn_of(*hidden.session, "Fighter");
    check(!offered(*hidden.session, "melee", "Dreadlord"),
          "A ring of invisibility hides its wearer from the fight's start");

    auto knight = creature(50, 5);
    auto feared = arrange_secret(
        {{"Squire", rules::Side::party, {1, 3}, creature(50, 1)},
         {"Knight", rules::Side::party, {1, 5}, knight},
         {"Dragon", rules::Side::opposition, {6, 3}, special({{482, 0, 0, false}}, 60)}},
        263);
    check(logged(*feared.session, "{name} is affected by {spell}.") == 1,
          "A dragon's fear grips only the weaker");
    until_turn_of(*feared.session, "Squire");
    check(view_of(feared.session->snapshot(), "Squire").panicked, "The squire runs");
}

// Secret's spell numbers: its own 200 up (canonical_spell), its characters'
// table in its order with Heal and Harm where Animate Dead and Restoration
// were.
void secret_spells_take_their_own_numbers()
{
    check(original::canonical_spell(36, secret_game) == 236 &&
              original::canonical_spell(110, secret_game) == 310 &&
              original::canonical_spell(58, secret_game) == 158 &&
              original::canonical_spell(100, secret_game) == 200,
          "Secret's own spells are 200 up, Curse's 100 up");
    const auto castable = original::castable_spells(secret_game);
    const auto at = [&castable](int number)
    {
        return std::ranges::find(castable, number) - castable.begin();
    };
    check(at(236) == 35 && at(256) == 55 && at(36) == std::ssize(castable) &&
              at(56) == std::ssize(castable),
          "Heal and Harm take Animate Dead's and Restoration's places");
    check(at(310) < at(317) && at(317) == std::ssize(castable) - 1 &&
              original::spell(316)->name == "Mass Invisibility" &&
              original::spell_number("magic_user:death_spell") == 310 &&
              original::spell_number("druid:charm_person_or_mammal") == 296,
          "Its new spells close its table");
}

// Secret's cleric spells (magic.pas, SpellHeal and SpellHarm): Heal cures
// disease and blindness and leaves 1d4 Hit Points short; Harm diseases and
// leaves 1d4.
void secret_heal_and_harm()
{
    using original::ClassSlot;
    auto wounded = creature(30);
    wounded.hit_points = 5;
    wounded.effects = {{33, 0, 0, false}, {34, 0, 0, false}};
    auto fight = arrange_secret(
        {{"Priest", rules::Side::party, {1, 3}, caster(ClassSlot::cleric, 12, {236, 256})},
         {"Wounded", rules::Side::party, {2, 4}, wounded},
         {"Ogre", rules::Side::opposition, {2, 3}, victim(100)}},
        271);
    auto &session = *fight.session;
    cast_now(session, "Priest", 236,
             [&]
             {
                 return aim_at(session, "Wounded");
             });
    const int healed = view_of(session.snapshot(), "Wounded").hit_points;
    check(healed >= 26 && healed <= 29, "Heal leaves 1d4 short of the most");
    check(logged(session, "{name} can see.") == 1 && logged(session, "{name} is cured.") == 1,
          "Heal cures blindness and disease");
    cast_now(session, "Priest", 256,
             [&]
             {
                 return aim_at(session, "Ogre");
             });
    const int left = view_of(session.snapshot(), "Ogre").hit_points;
    check(left >= 1 && left <= 4, "Harm leaves 1d4 Hit Points");
}

// Secret's slaying spells (SpellDeathSpell, SpellDisintegrate,
// SpellFleshToStone, SpellStoneToFlesh): Death Spell drops the weak its
// budget pays for, never the mighty or the unliving; Disintegrate and Flesh
// to Stone slay on a failed save but not an iron golem; Stone to Flesh brings
// a statue back with 1 Hit Point.
void secret_spells_slay()
{
    using original::ClassSlot;
    auto mighty = victim(100);
    mighty.level = 9;
    auto unliving = victim(100);
    unliving.level = 0;
    unliving.traits = original::trait::unliving;
    auto fight = arrange_secret(
        {{"Mage", rules::Side::party, {0, 3}, caster(ClassSlot::magic_user, 12, {310})},
         {"Kobold", rules::Side::opposition, {5, 3}, victim(5)},
         {"Goblin", rules::Side::opposition, {5, 4}, victim(5)},
         {"Giant", rules::Side::opposition, {6, 3}, mighty},
         {"Golem", rules::Side::opposition, {6, 4}, unliving}},
        273);
    auto &session = *fight.session;
    cast_now(session, "Mage", 310,
             [&]
             {
                 return aim_at_cell(session, {5, 3});
             });
    check(logged(session, "{name} drops dead.") == 2 &&
              view_of(session.snapshot(), "Giant").hit_points == 100 &&
              view_of(session.snapshot(), "Golem").hit_points == 100,
          "Death Spell drops the weak, not the mighty or the unliving");

    auto iron = victim(100);
    iron.effects = {{479, 0, 0, false}};
    auto blast = arrange_secret(
        {{"Mage", rules::Side::party, {0, 3}, caster(ClassSlot::magic_user, 12, {311, 311})},
         {"Orc", rules::Side::opposition, {3, 3}, victim(100)},
         {"Iron", rules::Side::opposition, {3, 5}, iron}},
        277);
    cast_now(*blast.session, "Mage", 311,
             [&]
             {
                 return aim_at(*blast.session, "Orc");
             });
    check(logged(*blast.session, "{name} disintegrates.") == 1, "A failed save disintegrates");
    cast_now(*blast.session, "Mage", 311,
             [&]
             {
                 return aim_at(*blast.session, "Iron");
             });
    check(logged(*blast.session, "{name} disintegrates.") == 1 &&
              view_of(blast.session->snapshot(), "Iron").hit_points == 100,
          "An iron golem takes no spell");

    auto statue = arrange_secret(
        {{"Mage", rules::Side::party, {0, 3}, caster(ClassSlot::magic_user, 12, {314, 313})},
         {"Orc", rules::Side::opposition, {3, 3}, victim(100)},
         {"Far", rules::Side::opposition, {7, 7}, victim(100)}},
        282);
    cast_now(*statue.session, "Mage", 314,
             [&]
             {
                 return aim_at(*statue.session, "Orc");
             });
    check(logged(*statue.session, "{name} is stoned.") == 1, "Flesh to Stone turns it to stone");
    cast_now(*statue.session, "Mage", 313,
             [&]
             {
                 return aim_at(*statue.session, "Orc");
             });
    check(logged(*statue.session, "{name} is no longer a statue.") == 1 &&
              view_of(statue.session->snapshot(), "Orc").hit_points == 1,
          "Stone to Flesh brings the statue back with 1 Hit Point");
}

// Secret's other new spells: Power Word Stun by the Hit Points left, Mass
// Invisibility on the caster's side away from enemies, the Delayed Blast
// Fireball bursting at once, Charm Person or Mammal on a mammal only.
void secret_spells_stun_hide_and_burn()
{
    using original::ClassSlot;
    auto fight = arrange_secret(
        {{"Mage", rules::Side::party, {0, 3}, caster(ClassSlot::magic_user, 14, {317, 317, 316})},
         {"Ally", rules::Side::party, {1, 4}, creature(30)},
         {"Engaged", rules::Side::party, {2, 1}, creature(30)},
         {"Weak", rules::Side::opposition, {3, 1}, victim(20)},
         {"Strong", rules::Side::opposition, {4, 6}, victim(120)}},
        283);
    auto &session = *fight.session;
    cast_now(session, "Mage", 317,
             [&]
             {
                 return aim_at(session, "Weak");
             });
    cast_now(session, "Mage", 317,
             [&]
             {
                 return aim_at(session, "Strong");
             });
    check(logged(session, "{name} is affected by {spell}.") == 1 &&
              logged(session, "{name} is unaffected.") == 1,
          "Power Word Stun stuns the weak, not one above 90 Hit Points");
    cast_now(session, "Mage", 316,
             [&]
             {
                 return aim_at_cell(session, {1, 3});
             });
    check(logged(session, "{name} is affected by {spell}.") == 3,
          "Mass Invisibility hides the caster and its ally");

    auto burn = arrange_secret(
        {{"Mage", rules::Side::party, {0, 3}, caster(ClassSlot::magic_user, 14, {315})},
         {"Near", rules::Side::opposition, {5, 3}, victim(200)},
         {"Past", rules::Side::opposition, {5, 7}, victim(200)}},
        287);
    cast_now(*burn.session, "Mage", 315,
             [&]
             {
                 burn.session->submit(order(*burn.session, "blast_delay"));
                 return aim_at_cell(*burn.session, {5, 3});
             });
    const int burned = 200 - view_of(burn.session->snapshot(), "Near").hit_points;
    check(burned >= 28 && burned <= 98 &&
              view_of(burn.session->snapshot(), "Past").hit_points == 200,
          "The fireball bursts at once for the level and as many d6 within 3 squares");

    auto mammal = victim(30);
    mammal.traits = original::trait::mammal;
    auto charm = arrange_secret(
        {{"Ranger", rules::Side::party, {0, 3}, caster(ClassSlot::ranger, 12, {296, 296})},
         {"Bear", rules::Side::opposition, {3, 3}, mammal},
         {"Snake", rules::Side::opposition, {3, 5}, victim(30)}},
        289);
    cast_now(*charm.session, "Ranger", 296,
             [&]
             {
                 return aim_at(*charm.session, "Snake");
             });
    cast_now(*charm.session, "Ranger", 296,
             [&]
             {
                 return aim_at(*charm.session, "Bear");
             });
    check(logged(*charm.session, "{name} is unaffected.") == 1 &&
              view_of(charm.session->snapshot(), "Bear").side == rules::Side::party,
          "Charm Person or Mammal charms a mammal and no other monster");
}

// Pools of Darkness's new spells in fights (its reconstruction's magic.pas):
// the power words by Hit Points, Destruction and Energy Drain, Blade Barrier's
// blades, Otto's Irresistible Dance, Meteor Swarm and a summons no one answers.
void darkness_spells_fight()
{
    using original::ClassSlot;
    constexpr auto darkness = GoldBoxGame::pools_of_darkness;
    const auto arrange_darkness = [](const std::vector<Placed> &placed, std::uint64_t seed)
    {
        return arrange(placed, seed, {}, 100, darkness);
    };
    auto words = arrange_darkness(
        {{"Mage", rules::Side::party, {0, 3}, caster(ClassSlot::magic_user, 20, {325, 325, 323})},
         {"Ogre", rules::Side::opposition, {5, 3}, victim(100)},
         {"Kobold", rules::Side::opposition, {5, 4}, victim(8)},
         {"Goblin", rules::Side::opposition, {6, 4}, victim(9)},
         {"Orc", rules::Side::opposition, {2, 6}, victim(40)},
         {"Troll", rules::Side::opposition, {7, 7}, victim(30)},
         {"Gnoll", rules::Side::opposition, {6, 7}, victim(30)},
         {"Giant", rules::Side::opposition, {7, 6}, victim(50)}},
        301);
    auto &session = *words.session;
    cast_now(session, "Mage", 325,
             [&]
             {
                 return aim_at_cell(session, {5, 3});
             });
    check(logged(session, "{name} is slain.") == 2 &&
              view_of(session.snapshot(), "Ogre").hit_points == 100,
          "Power Word Kill slays those of 10 Hit Points or fewer past a stronger first target");
    cast_now(session, "Mage", 325,
             [&]
             {
                 return aim_at_cell(session, {2, 6});
             });
    check(logged(session, "{name} is slain.") == 3, "or its first target of 11 to 60");
    cast_now(session, "Mage", 323,
             [&]
             {
                 return aim_at_cell(session, {7, 7});
             });
    check(logged(session, "{name} is affected by {spell}.") == 2,
          "Power Word Blind blinds up to 100 Hit Points of them");

    auto priest = arrange_darkness(
        {{"Priest", rules::Side::party, {0, 3}, caster(ClassSlot::cleric, 18, {304, 303, 301})},
         {"Wight", rules::Side::opposition, {1, 3}, victim(60)},
         {"Fighter", rules::Side::opposition, {0, 4}, caster(ClassSlot::fighter, 9, {})},
         {"Hobgoblin", rules::Side::opposition, {3, 4}, victim(200)}},
        303);
    auto &holy = *priest.session;
    cast_now(holy, "Priest", 304,
             [&]
             {
                 return aim_at(holy, "Wight");
             });
    check(logged(holy, "{name} is destroyed.") == 1, "Destruction destroys");
    cast_now(holy, "Priest", 303,
             [&]
             {
                 return aim_at(holy, "Fighter");
             });
    check(logged(holy, "{name} is drained a level.") == 1, "Energy Drain drains a level");
    cast_now(holy, "Priest", 301,
             [&]
             {
                 return aim_at_cell(holy, {3, 4});
             });
    const int cut = 200 - view_of(holy.snapshot(), "Hobgoblin").hit_points;
    // Secret's order of the round lets the hobgoblin take its turn in the
    // blades too before the check.
    const int cuts = logged(holy, "{name} is cut by the blades.");
    check(cuts >= 1 && cut >= 8 * cuts && cut <= 64 * cuts, "Blade Barrier cuts one in it for 8d8");

    auto dance = arrange_darkness(
        {{"Mage", rules::Side::party, {0, 3}, caster(ClassSlot::magic_user, 20, {321, 324, 326})},
         {"Dancer", rules::Side::opposition, {1, 3}, creature(200)},
         {"Far", rules::Side::opposition, {6, 6}, creature(200)}},
        307);
    auto &ball = *dance.session;
    cast_now(ball, "Mage", 321,
             [&]
             {
                 return aim_at(ball, "Dancer");
             });
    check(logged(ball, "{name} is affected by {spell}.") == 1, "Otto's dance takes its target");
    cast_now(ball, "Mage", 324,
             [&]
             {
                 return aim_at_cell(ball, {6, 6});
             });
    const int burned = 200 - view_of(ball.snapshot(), "Far").hit_points;
    check(burned >= 20, "Meteor Swarm's four meteors burst around its aim");
    until_turn_of(ball, "Mage");
    check(ball.submit(*cast_command(ball, 326)), "Monster Summoning is cast");
    play_until(
        ball,
        [&](const std::string &)
        {
            return order(ball, "end");
        },
        [&]
        {
            return logged(ball, "{name}'s summons is not answered.") > 0;
        });
    check(logged(ball, "{name}'s summons is not answered.") == 1,
          "Monster Summoning finds no one waiting");

    // Mass Charm charms enemies its records call persons (SpellMassCharm),
    // but not one Mind Blank shields, nor a monster its record does not call
    // a person.
    auto person = victim(30);
    person.traits = original::trait::person;
    auto blank = person;
    blank.effects = {{523, 0, 0, false}};
    auto charm = arrange_darkness(
        {{"Mage", rules::Side::party, {0, 3}, caster(ClassSlot::magic_user, 16, {320})},
         {"Orc", rules::Side::opposition, {4, 3}, person},
         {"Shielded", rules::Side::opposition, {4, 4}, blank},
         {"Ooze", rules::Side::opposition, {4, 2}, victim(30)}},
        311);
    cast_now(*charm.session, "Mage", 320,
             [&]
             {
                 return aim_at_cell(*charm.session, {4, 3});
             });
    const auto charmed = charm.session->snapshot();
    check(view_of(charmed, "Orc").side == rules::Side::party &&
              view_of(charmed, "Shielded").side == rules::Side::opposition,
          "Mass Charm charms an enemy; Mind Blank turns it away");
    const bool ooze_unaffected = std::ranges::any_of(
        charmed.log_entries,
        [](const auto &entry)
        {
            return entry.message.source == "{name} is unaffected." &&
                   std::ranges::any_of(entry.message.arguments,
                                       [](const rules::MessageArgument &a)
                                       {
                                           return a.name == "name" && a.value == "Ooze";
                                       });
        });
    check(view_of(charmed, "Ooze").side == rules::Side::opposition && ooze_unaffected,
          "Mass Charm passes over a monster its record does not call a person");
}

constexpr auto darkness_game = GoldBoxGame::pools_of_darkness;

Fight arrange_darkness(const std::vector<Placed> &placed, std::uint64_t seed)
{
    return arrange(placed, seed, {}, 100, darkness_game);
}

// Pools of Darkness's effects by the fight's numbers (canonical_effect):
// Secret's where its handler does as Secret's, the earlier games' where it
// does as theirs, and its own 400 up, or 470 where Secret's own take that.
void darkness_effects_take_their_own_numbers()
{
    const auto mapped = [](int code)
    {
        return original::canonical_effect(code, darkness_game);
    };
    check(mapped(6) == 335 && mapped(57) == 457 && mapped(58) == 458 && mapped(60) == 460 &&
              mapped(64) == 464 && mapped(70) == 470 && mapped(93) == 112 && mapped(103) == 119,
          "An effect that works as Secret's takes Secret's number");
    check(mapped(66) == 115 && mapped(84) == 504 && mapped(85) == 483 && mapped(101) == 85 &&
              mapped(102) == 86 && mapped(108) == 521,
          "One that works as another's takes that one's number");
    check(mapped(40) == 640 && mapped(80) == 680 && mapped(81) == 681 && mapped(125) == 725 &&
              mapped(126) == 726,
          "The Rakshasa's protection, weapons below +3 and +4, the +2 weapon and gaze immunity "
          "take the kit's numbers, which play them alike");
    check(mapped(118) == 518 && mapped(67) == 537 && mapped(100) == 570 && mapped(104) == 574,
          "Its own are 400 up, or 470 where one of Secret's has that number");
}

// Pools of Darkness's creatures as its files make them (installed_games.h):
// their effects by the fight's numbers, their flags and magic resistance.
void darkness_records_fight_by_their_effects()
{
    const auto darkness = opengold::test::installed_game_folder(darkness_game);
    if (darkness.empty())
    {
        std::cout << "Pools of Darkness's files are not here; its records are not checked.\n";
        return;
    }
    const auto content = load_original_content(darkness, darkness_game);
    const auto creature_at = [&content](int record) -> const original::Combatant &
    {
        return content.bestiary.at("MON1CHA.DAX:" + std::to_string(record));
    };
    const auto has = [](const original::Combatant &c, int code)
    {
        return std::ranges::any_of(c.effects,
                                   [code](const original::Effect &e)
                                   {
                                       return e.code == code;
                                   });
    };
    check(has(creature_at(55), 85) && has(creature_at(55), 119),
          "A wight's touch drains a level, and only magic harms it");
    check(has(creature_at(26), 504) && has(creature_at(26), 570) && has(creature_at(26), 273),
          "A red dragon breathes fire, frightens and shrugs off breath");
    check(has(creature_at(70), 640) && has(creature_at(70), 680),
          "A rakshasa takes no spell below the eighth level and half from weapons below +3");
    check(creature_at(40).magic_resistance == 80, "A drow wizard resists magic");
    check((creature_at(15).traits & original::trait::ranger_foe) &&
              (creature_at(15).traits & original::trait::giant_bane) &&
              (creature_at(55).traits & original::trait::unliving),
          "A hill giant is a ranger's foe and a giant-bane weapon's; a wight is unliving");
    check(has(creature_at(78), 457), "Marcus is in a Globe of Invulnerability");
    check(has(creature_at(13), 516), "Kalistes strikes with six weapons");
    check(creature_at(75).traits & original::trait::snake, "A poisonous snake is a snake");
}

// A Pools of Darkness monster's effects in a fight: its Globe of
// Invulnerability (57) turns a spell of the fourth level or below, a wight's
// touch (101) drains a level, and a dragon's fear (100) terrifies each enemy
// that fails a save at -4, without the run Secret's sends them on.
void darkness_monsters_fight_by_their_effects()
{
    using original::ClassSlot;
    por::CharacterRecord record;
    record.game = darkness_game;
    record.name = "LICH";
    record.max_hit_points = 60;
    record.base_thac0 = 10;
    record.base_armor_class = 0;
    por::EffectRecord globe;
    globe.code = 57;
    const auto lich = original::combatant_from_record(record, {}, std::array{globe});
    auto shielded = arrange_darkness(
        {{"Mage", rules::Side::party, {0, 3}, caster(ClassSlot::magic_user, 5, {15})},
         {"Lich", rules::Side::opposition, {5, 3}, lich}},
        307);
    cast_now(*shielded.session, "Mage", 15,
             [&]
             {
                 return aim_at(*shielded.session, "Lich");
             });
    check(logged(*shielded.session, "{name} is unaffected.") == 1 &&
              view_of(shielded.session->snapshot(), "Lich").hit_points == 60,
          "Its Globe of Invulnerability turns Magic Missile");

    const auto drain_touch = original::canonical_effect(101, darkness_game);
    auto fighter = victim(30);
    fighter.class_levels[static_cast<std::size_t>(ClassSlot::fighter)] = 3;
    fighter.level = 3;
    const auto drained = attack_until(
        monster({2, 1, 1, 0}, {0, 0, 0, 0}, {{drain_touch, 0, 255, false}}), fighter,
        [](const rules::CombatSession &session)
        {
            return logged(session, "{name} is drained a level.") > 0;
        },
        47, darkness_game);
    check(logged(*drained.session, "{name} is drained a level.") == 1,
          "A wight's touch drains a level");

    const auto fear = original::canonical_effect(100, darkness_game);
    auto feared = arrange_darkness(
        {{"Squire", rules::Side::party, {1, 3}, victim(50)},
         {"Knight", rules::Side::party, {1, 5}, victim(50)},
         {"Dragon", rules::Side::opposition, {6, 3}, special({{fear, 0, 255, false}}, 60)}},
        263);
    check(logged(*feared.session, "{name} is terrified.") == 2,
          "A dragon's fear terrifies each enemy that fails its save");
    until_turn_of(*feared.session, "Squire");
    check(!view_of(feared.session->snapshot(), "Squire").panicked,
          "The terrified fight on, worse off");
}

// Kalistes' six weapons (Pools of Darkness's effect 116): each of her first
// six blows at a defender lands with the next weapon, its dice and 4 more
// with her Strength's bonus, whatever her record's dice (effhndlr.pas,
// EffMultiAttackNewWeapons).
void darkness_kalistes_strikes_with_six_weapons()
{
    const auto six_weapons = original::canonical_effect(116, darkness_game);
    const auto struck = [](const rules::CombatSession &session)
    {
        return logged(session, "{attacker} strikes with a {weapon}.") >= 6;
    };
    const auto fight =
        attack_until(monster({12, 1, 1, 0}, {0, 0, 0, 0}, {{six_weapons, 0, 0, false}}),
                     victim(500), struck, 47, darkness_game);
    const auto &session = *fight.session;
    std::vector<std::string> weapons;
    for (const auto &entry : session.snapshot().log_entries)
    {
        for (const auto &argument : entry.message.arguments)
        {
            if (argument.name == "weapon")
            {
                weapons.push_back(argument.value);
            }
        }
    }
    check(weapons == std::vector<std::string>{"Two-Handed Sword +4", "Bastard Sword +4",
                                              "Long Sword +4", "Scimitar +4", "Dagger +4",
                                              "Spear +4"},
          "Her six blows land with her six weapons in turn");
    const int harm = 500 - view_of(session.snapshot(), "Victim").hit_points;
    check(harm >= 31 && harm <= 68, "Each does its weapon's dice and 4 more, not her 1d1");
}

// Pools of Darkness's dragons breathe three times a fight (magic.pas,
// EffDragonBreath, EffBreatheLightning): the biggest red fire for 24d10+12, a
// black acid along a path one square wide, a blue lightning; the gaze
// spider's and the vampires' gazes, which the original never runs, stay still.
void darkness_dragons_breathe_and_spiders_gaze()
{
    const auto breathing = [](int code, std::uint64_t seed)
    {
        auto fight = arrange_darkness(
            {{"Fighter", rules::Side::party, {2, 3}, victim(5000)},
             {"Dragon",
              rules::Side::opposition,
              {5, 3},
              special({{original::canonical_effect(code, darkness_game), 0, 0, false}}, 40)}},
            seed);
        ai_plays(
            *fight.session, "Dragon",
            []
            {
                return false;
            },
            8);
        return fight;
    };
    const auto big_red = breathing(83, 251);
    const int burnt = 5000 - view_of(big_red.session->snapshot(), "Fighter").hit_points;
    check(logged(*big_red.session, "{name} breathes.") == 3 && burnt >= 3 * 18 && burnt > 3 * 40,
          "The biggest red dragon breathes 24d10+12 three times a fight");
    check(logged(*breathing(91, 251).session, "{name} breathes.") == 3,
          "A blue dragon breathes lightning three times a fight");
    check(logged(*breathing(96, 251).session, "{name} breathes.") == 3,
          "A black dragon breathes acid three times a fight");
    auto beside = arrange_darkness(
        {{"Fighter", rules::Side::party, {2, 3}, victim(500)},
         {"Squire", rules::Side::party, {2, 4}, victim(500)},
         {"Dragon",
          rules::Side::opposition,
          {5, 3},
          special({{original::canonical_effect(96, darkness_game), 0, 0, false}}, 40)}},
        251);
    until_turn_of(*beside.session, "Dragon");
    beside.session->submit(order(*beside.session, "quick"));
    const int burnt_members = (view_of(beside.session->snapshot(), "Squire").hit_points < 500) +
                              (view_of(beside.session->snapshot(), "Fighter").hit_points < 500);
    check(logged(*beside.session, "{name} breathes.") == 1 && burnt_members == 1,
          "The black dragon's acid is one square wide");

    // The gazes the original registers but never runs stay still, the
    // game's option off.
    auto spider =
        arrange_darkness({{"Fighter", rules::Side::party, {2, 3}, victim(50)},
                          {"Gazer",
                           rules::Side::opposition,
                           {5, 3},
                           special({{original::canonical_effect(114, darkness_game), 0, 0, false},
                                    {original::canonical_effect(124, darkness_game), 0, 0, false}},
                                   40)}},
                         257);
    until_turn_of(*spider.session, "Gazer");
    spider.session->submit(order(*spider.session, "quick"));
    check(logged(*spider.session, "{name} gazes.") == 0,
          "A gaze spider and a vampire never gaze, as in the original");
}

// With the game's option Darkness's gazes on (the user, 2026-11-15), the
// gaze spider's paralyzing gaze (its 114) holds the enemy it locks onto
// unless it saves (effhndlr.pas, EffParalyzeGaze), and the vampire's
// charming gaze (its 124) casts Charm Person at it (magic.pas,
// EffGazeCharm); with it off they do nothing, as in the original.
void darkness_gazes_work_with_the_option_on()
{
    const auto gazing = [](int code, bool played, std::uint64_t seed)
    {
        auto fight = arrange(
            {{"Fighter", rules::Side::party, {2, 3}, victim(500)},
             {"Gazer",
              rules::Side::opposition,
              {4, 3},
              special({{original::canonical_effect(code, darkness_game), 0, 0, false}}, 40)}},
            seed, {}, 100, darkness_game, false, false, nullptr, {}, played);
        ai_plays(
            *fight.session, "Gazer",
            []
            {
                return false;
            },
            6);
        return fight;
    };
    int paralyzed = 0, charmed = 0, gazed_off = 0;
    for (std::uint64_t seed = 1; seed <= 12; ++seed)
    {
        const auto spider = gazing(114, true, seed);
        paralyzed += logged(*spider.session, "{name} is paralyzed.");
        const auto vampire = gazing(124, true, seed);
        charmed += view_of(vampire.session->snapshot(), "Fighter").side == rules::Side::opposition;
        gazed_off += logged(*gazing(114, false, seed).session, "{name} gazes.") +
                     logged(*gazing(124, false, seed).session, "{name} gazes.");
    }
    check(paralyzed > 0, "With the option on, the gaze spider's gaze paralyzes");
    check(charmed > 0, "With the option on, the vampire's gaze charms");
    check(gazed_off == 0, "With the option off, neither gazes, as in the original");
}

// Pools of Darkness plays Secret's rules where its reconstruction does as
// Secret's: Hold Person never holds a monster (magic.pas,
// ApplyEffectToTargets), however small.
void darkness_holds_no_monster()
{
    using original::ClassSlot;
    auto fight = arrange_darkness(
        {{"Cleric", rules::Side::party, {0, 3}, caster(ClassSlot::cleric, 3, {23})},
         {"Bandit", rules::Side::opposition, {4, 3}, victim(20)}},
        101);
    auto &session = *fight.session;
    cast_now(session, "Cleric", 23,
             [&]
             {
                 return aim_at(session, "Bandit");
             });
    session.submit(order(session, "aim_done"));
    check(view_of(session.snapshot(), "Bandit").status.empty() &&
              logged(session, "{name} is unaffected.") == 1,
          "Hold Person does not hold a monster");
}

// Pools of Darkness's monsters' and items' effects that play as the kit's of
// the same numbers (effhndlr.pas): a vorpal blade beheads with a primary
// blow of a natural 17 or more, never one whose record's "immunities" have
// bit 20h (EffVorpal, 115); a giant-slaying sword strikes its quarry for 6d6
// and the Strength bonus (EffGiantSlayingSword, 75); a bulette strikes 4
// times with its second attack, 8 one time in 4 (EffBulletRake, 99).
void darkness_blades_and_the_bulette_strike()
{
    const auto mapped = [](int code)
    {
        return original::canonical_effect(code, darkness_game);
    };
    check(
        mapped(75) == 675 && mapped(99) == 699 && mapped(115) == 715,
        "The giant-slaying sword, the bulette's rake and the vorpal blade take the kit's numbers");
    const auto wielder = [&](int code)
    {
        return monster({2, 1, 1, 0}, {0, 0, 0, 0}, {{mapped(code), 0, 0, false}});
    };
    const auto target_of = [](int traits)
    {
        auto c = creature(1000, 0);
        c.traits = traits;
        return c;
    };
    const auto beheading = [&](int traits)
    {
        return strike_until(
            wielder(115), target_of(traits),
            [](const rules::CombatSession &session)
            {
                return logged(session, "{name} is beheaded.") > 0;
            },
            53, darkness_game);
    };
    const auto blade = beheading(0);
    check(logged(*blade.session, "{name} is beheaded.") == 1 &&
              logged(*blade.session, "{attacker} hits {name} for {damage}.", "Attacker") > 1,
          "A vorpal blade beheads with a blow of a natural 17 or more, not with every blow");
    const auto proof = beheading(original::trait::beheading_proof);
    check(logged(*proof.session, "{name} is beheaded.") == 0 &&
              logged(*proof.session, "{attacker} hits {name} for {damage}.", "Attacker") > 20,
          "Never one proof against it");
    const int bane = first_hit(wielder(75), target_of(original::trait::giant_bane), darkness_game);
    check(bane >= 6 && bane <= 36 && first_hit(wielder(75), target_of(0), darkness_game) == 1,
          "A giant-slaying sword strikes its quarry for 6d6, others as it would");

    auto flurry =
        arrange_darkness({{"Fighter", rules::Side::party, {3, 3}, victim()},
                          {"Bulette",
                           rules::Side::opposition,
                           {4, 3},
                           monster({0, 0, 0, 0}, {2, 1, 1, 0}, {{mapped(99), 0, 0, false}})}},
                         253);
    until_turn_of(*flurry.session, "Bulette");
    flurry.session->submit(order(*flurry.session, "melee", "Fighter"));
    const int attacks = logged(*flurry.session, "{attacker} hits {name} for {damage}.", "Bulette") +
                        logged(*flurry.session, "{attacker} misses {name}.", "Bulette");
    check(attacks == 4 || attacks == 8, "A bulette strikes 4 or 8 times with its second attack");

    // Its records' "immunities" (byte 421): the bulette rakes, a red dragon
    // (bit 20h) keeps its head, a spectre (bit 10h) is proof against poison.
    const auto darkness = opengold::test::installed_game_folder(darkness_game);
    if (darkness.empty())
    {
        return;
    }
    const auto content = load_original_content(darkness, darkness_game);
    const auto creature_at = [&content](int record) -> const original::Combatant &
    {
        return content.bestiary.at("MON1CHA.DAX:" + std::to_string(record));
    };
    check(std::ranges::find(creature_at(1).effects, 699, &original::Effect::code) !=
              creature_at(1).effects.end(),
          "The bulette rakes");
    check((creature_at(26).traits & original::trait::beheading_proof) &&
              (creature_at(53).traits & original::trait::poison_proof) &&
              !(creature_at(15).traits & original::trait::beheading_proof),
          "A red dragon keeps its head, a spectre is proof against poison, a hill giant neither");
}

// Pools of Darkness's turning (cbtacts.pas, CombatTurnUndead; combat.pas,
// CombatMenu; combatai.pas, TurnUndeadAi): a paladin turns from level 3; each
// member tries as often as the fight's LOAD MONSTERs brought undead; the
// highest undead in sight comes first and a failure goes on to the levels
// below; one turned leaves the field at once; the area's penalty takes off
// the turning column (rules::Encounter::turning_tries, turning_penalty).
void later_games_turn_undead_their_own_way(GoldBoxGame game)
{
    using original::ClassSlot;
    const auto paladin = [](int level)
    {
        auto c = creature(30, level);
        c.class_levels[static_cast<std::size_t>(ClassSlot::paladin)] = level;
        return c;
    };
    const auto turning =
        [game](const std::vector<Placed> &placed, std::uint64_t seed, int tries, int penalty = 0)
    {
        return arrange(placed, seed, {}, 100, game, false, false,
                       [tries, penalty](rules::Encounter &encounter)
                       {
                           encounter.turning_tries = tries;
                           encounter.turning_penalty = penalty;
                       });
    };
    auto young = turning({{"Squire", rules::Side::party, {1, 3}, paladin(2)},
                          {"Skeleton", rules::Side::opposition, {5, 3}, undead(1)}},
                         67, 1);
    until_turn_of(*young.session, "Squire");
    check(!offered(*young.session, "turn"), "A paladin of level 2 cannot turn undead");

    // A paladin of level 5 turns at a cleric's level 3; the fight loaded
    // undead twice, so it tries twice.
    auto knight = turning({{"Knight", rules::Side::party, {1, 3}, paladin(5)},
                           {"Wight", rules::Side::opposition, {5, 3}, undead(9)},
                           {"Wight 2", rules::Side::opposition, {5, 5}, undead(9)}},
                          67, 2);
    auto &tries = *knight.session;
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        until_turn_of(tries, "Knight");
        check(offered(tries, "turn").has_value(), "A paladin of level 5 may turn undead twice");
        tries.submit(order(tries, "turn"));
    }
    until_turn_of(tries, "Knight");
    check(!offered(tries, "turn"), "Each member tries as often as the fight loaded undead");
    check(logged(tries, "{name} turns undead.") == 2, "The knight tried twice");

    // A skeleton turned leaves the field at once.
    auto rout = turning({{"Cleric", rules::Side::party, {1, 3}, cleric(4)},
                         {"Skeleton", rules::Side::opposition, {5, 3}, undead(1)}},
                        67, 1);
    until_turn_of(*rout.session, "Cleric");
    rout.session->submit(order(*rout.session, "turn"));
    check(logged(*rout.session, "{name} is turned.") == 1 &&
              view_of(rout.session->snapshot(), "Skeleton").fled,
          "A turned skeleton leaves the field at once");

    // The vampire, the highest, is tried before the skeleton; failing it,
    // the attempt goes on to the skeleton, which a level-8 cleric destroys.
    bool vampire_first = false, failure_goes_on = false;
    for (std::uint64_t seed = 1; seed < 80 && !(vampire_first && failure_goes_on); ++seed)
    {
        auto fight = turning({{"Cleric", rules::Side::party, {1, 3}, cleric(8)},
                              {"Skeleton", rules::Side::opposition, {5, 2}, undead(1)},
                              {"Vampire", rules::Side::opposition, {5, 4}, undead(10)}},
                             seed, 1);
        auto &session = *fight.session;
        until_turn_of(session, "Cleric");
        session.submit(order(session, "turn"));
        std::vector<std::string> affected;
        for (const auto &entry : session.snapshot().log_entries)
        {
            const auto &source = entry.message.source;
            if (source == "{name} is turned." || source == "{name} is destroyed.")
            {
                affected.push_back(entry.message.arguments.front().value);
            }
        }
        vampire_first =
            vampire_first || affected == std::vector<std::string>{"Vampire", "Skeleton"};
        failure_goes_on = failure_goes_on || affected == std::vector<std::string>{"Skeleton"};
    }
    check(vampire_first, "The highest undead comes first");
    check(failure_goes_on, "A failure goes on to the undead below");

    // The area's penalty can leave no column to turn with.
    auto cursed = turning({{"Cleric", rules::Side::party, {1, 3}, cleric(8)},
                           {"Skeleton", rules::Side::opposition, {5, 3}, undead(1)}},
                          67, 1, 20);
    until_turn_of(*cursed.session, "Cleric");
    cursed.session->submit(order(*cursed.session, "turn"));
    check(logged(*cursed.session, "Nothing happens.") == 1 &&
              view_of(cursed.session->snapshot(), "Skeleton").status.empty(),
          "Where the area's penalty passes the column, no undead turns");

    // With no undead loaded no one may turn; a paladin on Quick turns them.
    auto none = turning({{"Cleric", rules::Side::party, {1, 3}, cleric(8)},
                         {"Skeleton", rules::Side::opposition, {5, 3}, undead(1)}},
                        67, 0);
    until_turn_of(*none.session, "Cleric");
    check(!offered(*none.session, "turn"), "A fight that loaded no undead offers no turning");
    auto quick = turning({{"Knight", rules::Side::party, {1, 3}, paladin(10)},
                          {"Skeleton", rules::Side::opposition, {5, 3}, undead(1)}},
                         193, 1);
    until_turn_of(*quick.session, "Knight");
    quick.session->submit(order(*quick.session, "quick"));
    check(logged(*quick.session, "{name} turns undead.") == 1,
          "A paladin on Quick turns undead in sight");
}

void native_knights_can_lead_allied_creatures()
{
    const auto make = [](int rank, int charisma, int npc, int intelligence, bool knight,
                         int leader_hp, int follower_hp, std::uint64_t seed,
                         GoldBoxGame game = GoldBoxGame::dark_queen_of_krynn,
                         bool npc_leader = false, bool second_leader = false, int leader_status = 0,
                         int follower_status = 0)
    {
        auto leader = creature(100, 10);
        leader.hit_points = leader_hp;
        leader.class_levels[static_cast<std::size_t>(original::ClassSlot::monk)] = knight ? 10 : 0;
        auto follower = creature(100, 10);
        follower.hit_points = follower_hp;
        follower.morale = static_cast<unsigned>(npc);
        follower.intelligence = intelligence;
        original::Content content;
        content.game = game;
        content.bestiary = {{"Knight", leader}, {"Ally", follower}, {"Foe", creature(100)}};
        auto record = kobold_record();
        record.game = game;
        record.knight_order = static_cast<std::uint8_t>(rank);
        record.npc_flag = npc_leader ? 128 : 0;
        record.status = static_cast<std::uint8_t>(leader_status);
        record.current_abilities.charisma = static_cast<std::uint8_t>(charisma);
        // The native MON-record path retains status; FIGHTRECORD2 intentionally
        // omits it, so a serialized character fixture would lose this case.
        content.gear["Knight"] = {record, {}};
        auto ally_record = record;
        ally_record.knight_order = 0;
        ally_record.npc_flag = static_cast<std::uint8_t>(npc);
        ally_record.status = static_cast<std::uint8_t>(follower_status);
        ally_record.current_abilities.intelligence = static_cast<std::uint8_t>(intelligence);
        content.gear["Ally"] = {ally_record, {}};
        rules::Encounter encounter;
        encounter.morale = 100;
        encounter.battlefield = {8, 8, std::vector<rules::Terrain>(64, rules::Terrain::open)};
        const auto add =
            [&](rules::EntityId id, std::string name, rules::Side side, rules::Cell cell)
        {
            rules::Participant p;
            p.id = id;
            p.name = p.definition = std::move(name);
            p.side = side;
            p.cell = cell;
            encounter.participants.push_back(std::move(p));
        };
        add(1, "Knight", rules::Side::party, {1, 2});
        add(2, "Ally", rules::Side::party, {3, 2});
        add(3, "Foe", rules::Side::opposition, {6, 5});
        if (second_leader)
        {
            record.knight_order = 3;
            record.current_abilities.charisma = 25;
            content.gear["Second knight"] = {record, {}};
            content.bestiary["Second knight"] = leader;
            add(4, "Second knight", rules::Side::party, {1, 5});
        }
        Fight fight{original::rules_module(std::move(content)), nullptr};
        fight.session = fight.module->create(std::move(encounter), seed);
        return fight;
    };
    auto led = make(3, 25, 128, 10, true, 100, 100, 37);
    check(view_of(led.session->snapshot(), "Ally").player_controls_ally,
          "A Native knight's successful leadership gives the player its living NPC ally");
    const auto checkpoint = led.session->save();
    auto restored = led.module->restore(checkpoint);
    check(restored->save() == checkpoint &&
              view_of(restored->snapshot(), "Ally").player_controls_ally,
          "A combat checkpoint retains leadership without rolling it again");
    until_turn_of(*led.session, "Ally");
    check(led.session->submit(order(*led.session, "end")),
          "The controlled ally accepts the ordinary manual combat commands");
    for (const auto &refused :
         {make(0, 25, 128, 10, true, 100, 100, 37), make(3, 25, 128, 3, true, 100, 100, 37),
          make(3, 25, 0, 10, true, 100, 100, 37), make(3, 25, 128, 10, false, 100, 100, 37),
          make(3, 25, 128, 10, true, 0, 100, 37), make(3, 25, 128, 10, true, 100, 0, 37),
          make(3, 25, 128, 10, true, 100, 100, 37, GoldBoxGame::dark_queen_of_krynn, true),
          make(3, 25, 128, 10, true, 100, 100, 37, GoldBoxGame::pool_of_radiance),
          make(3, 25, 128, 10, true, 100, 100, 37, GoldBoxGame::dark_queen_of_krynn, false, false,
               1, 0),
          make(3, 25, 128, 10, true, 100, 100, 37, GoldBoxGame::dark_queen_of_krynn, false, false,
               0, 1)})
    {
        check(
            !view_of(refused.session->snapshot(), "Ally").player_controls_ally,
            "Leadership excludes invalid ranks, dull/non-NPC/downed allies, non-knights, downed leaders and other games");
    }
    // These seeds' first d100 rolls are exactly20,40,80. Equality fails,
    // while one extra point of current Charisma adds the source's five.
    for (const auto [rank, seed] : {std::pair{1, 119ULL}, std::pair{2, 50ULL}, std::pair{3, 17ULL}})
    {
        auto boundary = make(rank, 13, 128, 10, true, 100, 100, seed);
        auto bonus = make(rank, 14, 128, 10, true, 100, 100, seed);
        check(!view_of(boundary.session->snapshot(), "Ally").player_controls_ally &&
                  view_of(bonus.session->snapshot(), "Ally").player_controls_ally,
              "Each native rank uses a strict roll comparison and current Charisma's bonus");
        const auto unled = boundary.session->save();
        auto unled_restored = boundary.module->restore(unled);
        check(unled.find("OGORIGINALCOMBAT 31") != std::string::npos &&
                  unled_restored->save() == unled &&
                  !view_of(unled_restored->snapshot(), "Ally").player_controls_ally &&
                  view_of(unled_restored->snapshot(), "Ally").computer_plays,
              "Unmarked combat checkpoints stay automatic without rerolling");
    }
    auto later =
        make(1, 13, 128, 10, true, 100, 100, 119, GoldBoxGame::dark_queen_of_krynn, false, true);
    check(view_of(later.session->snapshot(), "Ally").player_controls_ally &&
              logged(*later.session, "{leader} leads {name}.") == 1,
          "A later eligible knight can lead an ally after the first knight fails");
}

void dark_queen_turning_honors_the_native_deity()
{
    constexpr auto game = GoldBoxGame::dark_queen_of_krynn;
    bool turned_strongest = false;
    for (std::uint64_t seed = 1; seed <= 40; ++seed)
    {
        for (const int deity : {0, 2})
        {
            auto fight = arrange({{"Cleric", rules::Side::party, {1, 3}, cleric(7)},
                                  {"Greater undead", rules::Side::opposition, {5, 3}, undead(12)}},
                                 seed, {}, 100, game, false, false,
                                 [deity](rules::Encounter &encounter)
                                 {
                                     encounter.turning_tries = 1;
                                     auto record = kobold_record();
                                     record.game = game;
                                     record.deity = static_cast<std::uint8_t>(deity);
                                     encounter.participants.front().character_record =
                                         original::encode_fight_record(record);
                                 });
            until_turn_of(*fight.session, "Cleric");
            fight.session->submit(order(*fight.session, "turn"));
            const bool turned = view_of(fight.session->snapshot(), "Greater undead").fled;
            check(deity == 2 || !turned,
                  "A native level-seven cleric cannot turn rank-twelve undead without the gift");
            turned_strongest = turned_strongest || turned;
            const auto saved = fight.session->save();
            check(fight.module->restore(saved)->save() == saved,
                  "Native turning attempts and deity survive an ordinary combat checkpoint");
        }
    }
    check(turned_strongest,
          "The native deity's two levels reach the higher turning band and strong undead");
}

// Pools of Darkness's AI never steps into a cloud that would harm it, with no
// save and whatever its movement (combatai.pas, AiCanStep): a fast monster
// keeps out of a cloudkill, one proof against poison walks into it.
void darkness_ai_shuns_clouds()
{
    using original::ClassSlot;
    const auto walks_in = [](int traits)
    {
        auto runner = victim(500);
        runner.level = 7;
        runner.movement = 12;
        runner.traits = traits;
        auto fight = arrange_darkness(
            {{"Mage", rules::Side::party, {1, 3}, caster(ClassSlot::magic_user, 9, {191})},
             {"Runner", rules::Side::opposition, {5, 3}, runner}},
            331);
        auto &session = *fight.session;
        cast_now(session, "Mage", 191,
                 [&]
                 {
                     return aim_at_cell(session, {3, 3});
                 });
        bool entered = false;
        play_until(
            session,
            [&](const std::string &actor)
            {
                return order(session, actor == "Runner" ? "quick" : "guard");
            },
            [&]
            {
                const auto cell = view_of(session.snapshot(), "Runner").cell;
                entered = entered || (cell.x >= 2 && cell.x <= 4 && cell.y >= 2 && cell.y <= 4);
                return entered || logged(session, "The air clears a little.") > 0 ||
                       session.snapshot().round > 8;
            });
        return entered;
    };
    check(!walks_in(0), "A monster of movement 12 never steps into the cloudkill");
    check(walks_in(original::trait::poison_proof), "One proof against poison walks into it");
}

// Pools of Darkness's own clouds (fxengine.pas, ConsiderTerrainEffects): a
// cloudkill spares one proof against poison and harms one of more than 6 Hit
// Dice for 1d10; a stinking cloud spares one proof against poison, and one
// that saves coughs for 1d4+1 rounds.
void darkness_clouds_spare_the_poison_proof()
{
    using original::ClassSlot;
    const auto foe = [](int level, int traits)
    {
        auto c = victim(100);
        c.level = level;
        c.traits = traits;
        return c;
    };
    auto deadly = arrange_darkness(
        {{"Mage", rules::Side::party, {1, 3}, caster(ClassSlot::magic_user, 9, {191})},
         {"Weak", rules::Side::opposition, {4, 2}, foe(3, 0)},
         {"Proof", rules::Side::opposition, {4, 3}, foe(3, original::trait::poison_proof)},
         {"Strong", rules::Side::opposition, {4, 4}, foe(9, 0)}},
        337);
    auto &session = *deadly.session;
    cast_now(session, "Mage", 191,
             [&]
             {
                 return aim_at_cell(session, {3, 3});
             });
    const auto snapshot = session.snapshot();
    check(view_of(snapshot, "Weak").status == "dead", "A cloudkill kills one of 3 Hit Dice");
    check(view_of(snapshot, "Proof").status != "dead" &&
              view_of(snapshot, "Proof").hit_points == 100,
          "It spares one proof against poison");
    const int strong = view_of(snapshot, "Strong").hit_points;
    check(view_of(snapshot, "Strong").status != "dead" && strong < 100 && strong >= 90,
          "One of 9 Hit Dice takes 1d10");

    auto sure = foe(3, 0);
    sure.saving_throws = {2, 2, 2, 2, 2};
    auto stench = arrange_darkness(
        {{"Mage", rules::Side::party, {1, 3}, caster(ClassSlot::magic_user, 9, {34})},
         {"Saver", rules::Side::opposition, {4, 3}, sure},
         {"Proof", rules::Side::opposition, {4, 2}, foe(3, original::trait::poison_proof)}},
        339);
    auto &smelly = *stench.session;
    cast_now(smelly, "Mage", 34,
             [&]
             {
                 return aim_at_cell(smelly, {3, 2});
             });
    check(logged(smelly, "{name} is affected by {spell}.") == 1,
          "A stinking cloud takes only the one not proof against poison");
}

// Pools of Darkness's Fear (magic.pas, SpellFear) sends one that fails its
// save running only while the fright lasts, a round a caster level: its
// EffFear ends the flight as the effect goes (effhndlr.pas). Its dragons'
// fear makes no one run (darkness_monsters_fight_by_their_effects).
void darkness_fear_ends_with_the_fright()
{
    using original::ClassSlot;
    auto rooted = victim(500);
    rooted.saving_throws = {20, 20, 20, 20, 20};
    rooted.movement = 0;
    auto fight = arrange_darkness(
        {{"Mage", rules::Side::party, {1, 3}, caster(ClassSlot::magic_user, 7, {184})},
         {"Ogre", rules::Side::opposition, {3, 3}, rooted}},
        341);
    auto &session = *fight.session;
    cast_now(session, "Mage", 184,
             [&]
             {
                 return aim_at_cell(session, {3, 3});
             });
    check(view_of(session.snapshot(), "Ogre").panicked, "One that fails its save runs in terror");
    const auto cast_round = session.snapshot().round;
    play_until(
        session,
        [&](const std::string &)
        {
            return order(session, "end");
        },
        [&]
        {
            return session.snapshot().round > cast_round + 1;
        });
    check(view_of(session.snapshot(), "Ogre").panicked,
          "It still runs at its next turn, the fright not over");
    play_until(
        session,
        [&](const std::string &)
        {
            return order(session, "end");
        },
        [&]
        {
            return session.snapshot().round > cast_round + 9;
        });
    check(!view_of(session.snapshot(), "Ogre").panicked, "Its flight ends with the fright");
}

// Pools of Darkness's beholders fire their rays (its 117; cbtacts.pas,
// AttackEyeRays, which its AI runs as its move begins, combatai.pas,
// AiMoveAttack): as the kit's, up to four tries a turn at the enemy it locks
// onto, each power once a turn, a ray within its distance.
void darkness_beholder_fires_its_rays()
{
    const auto eyes = original::canonical_effect(117, darkness_game);
    check(eyes == 717, "Pools of Darkness's 117 is the kit's eye rays");
    const auto beholder = special({{eyes, 0, 255, false}}, 60);
    auto near = arrange_darkness({{"Fighter", rules::Side::party, {2, 3}, victim(500)},
                                  {"Beholder", rules::Side::opposition, {0, 3}, beholder}},
                                 265);
    until_turn_of(*near.session, "Beholder");
    near.session->submit(order(*near.session, "quick"));
    check(logged(*near.session, "{name} fires a disintegrating ray.") == 1,
          "Within 2 squares a beholder's first ray is the disintegrating one");

    auto sure = victim(500);
    sure.saving_throws = {2, 2, 2, 2, 2};
    auto far = arrange_darkness({{"Fighter", rules::Side::party, {3, 3}, sure},
                                 {"Beholder", rules::Side::opposition, {0, 3}, beholder}},
                                269);
    until_turn_of(*far.session, "Beholder");
    far.session->submit(order(*far.session, "quick"));
    const auto fired = [&](std::string_view ray)
    {
        return logged(*far.session, std::string("{name} fires a ") + std::string(ray) + " ray.");
    };
    check(fired("disintegrating") == 0 && fired("petrifying") == 1 && fired("death") == 1 &&
              fired("wounding") == 1 &&
              view_of(far.session->snapshot(), "Fighter").hit_points < 500,
          "At 3 squares a beholder fires the stone, death and wounding rays, each once a turn");
}

// Gateway's changed rows (its spellTable): Prayer on up to four creatures the
// caster touches, Invisibility 10' Radius aimed beside the caster and
// reaching 6 squares around, Blink and Strength cast in and out of fights.
void gateway_rows_change()
{
    using original::ClassSlot;
    constexpr auto gateway = GoldBoxGame::gateway_to_the_savage_frontier;
    check(original::spell(45, gateway)->when == 2 && original::spell(35, gateway)->when == 2 &&
              original::spell(45)->when == 1 && original::spell(35)->when == 0,
          "Gateway casts Blink out of fights and Strength in them");
    auto fight =
        arrange({{"Priest", rules::Side::party, {1, 3}, caster(ClassSlot::cleric, 6, {42})},
                 {"Fighter", rules::Side::party, {2, 3}, creature(30)},
                 {"Archer", rules::Side::party, {1, 4}, creature(30)},
                 {"Far", rules::Side::party, {5, 5}, creature(30)},
                 {"Kobold", rules::Side::opposition, {7, 0}, creature(20, 0)}},
                311, {}, 100, gateway);
    auto &session = *fight.session;
    cast_now(session, "Priest", 42,
             [&]
             {
                 return aim_at(session, "Fighter");
             });
    check(!aim_at(session, "Far"), "Prayer touches: one far off cannot be picked");
    session.submit(*aim_at(session, "Archer"));
    session.submit(order(session, "aim_done"));
    check(logged(session, "{name} is affected by {spell}.") == 2, "Prayer goes on each it touches");

    auto hide =
        arrange({{"Mage", rules::Side::party, {0, 3}, caster(ClassSlot::magic_user, 6, {50})},
                 {"Near", rules::Side::party, {1, 7}, creature(30)},
                 {"Far", rules::Side::party, {7, 3}, creature(30)},
                 {"Kobold", rules::Side::opposition, {7, 0}, creature(20, 0)}},
                313, {}, 100, gateway);
    cast_now(*hide.session, "Mage", 50,
             [&]
             {
                 check(!aim_at_cell(*hide.session, {3, 3}),
                       "Invisibility 10' Radius is aimed beside the caster");
                 return aim_at_cell(*hide.session, {1, 3});
             });
    check(logged(*hide.session, "{name} is affected by {spell}.") == 3,
          "Invisibility 10' Radius reaches 6 squares around its square");
}

// The Strength spells in a fight (magic.pas): Enlarge gives a level's
// Strength (18/76 at level 4), which moves the blows' to-hit and damage;
// Reduce takes it away on a failed save; Gateway's Strength, cast in a fight
// on its caster, adds a d4 for a magic-user.
void strength_spells_strengthen()
{
    using original::ClassSlot;
    auto fighter = creature(30, 4);
    fighter.class_levels[static_cast<std::size_t>(ClassSlot::fighter)] = 4;
    fighter.strength = fighter.natural_strength = 16;
    fighter.strength_counts = true;
    fighter.saving_throws = {21, 21, 21, 21, 21};
    auto fight = arrange(
        {{"Mage", rules::Side::party, {0, 3}, caster(ClassSlot::magic_user, 4, {12})},
         {"Fighter", rules::Side::party, {1, 3}, fighter},
         {"Witch", rules::Side::opposition, {4, 3}, caster(ClassSlot::magic_user, 4, {13})}},
        331);
    auto &session = *fight.session;
    cast_now(session, "Mage", 12,
             [&]
             {
                 return aim_at(session, "Fighter");
             });
    const auto enlarged = left_by_fight(session, "Fighter");
    namespace o = original;
    check(enlarged.strength == 18 && enlarged.exceptional_strength == 76 &&
              enlarged.thac0 == fighter.thac0 - o::strength_to_hit_bonus(18, 76) +
                                    o::strength_to_hit_bonus(16, 0) &&
              enlarged.attacks[0].modifier == fighter.attacks[0].modifier +
                                                  o::strength_damage_bonus(18, 76) -
                                                  o::strength_damage_bonus(16, 0),
          "Enlarge gives 18/76 at level 4, and its to-hit and damage");
    cast_now(session, "Witch", 13,
             [&]
             {
                 return aim_at(session, "Fighter");
             });
    const auto reduced = left_by_fight(session, "Fighter");
    check(reduced.strength == 16 && reduced.thac0 == fighter.thac0 &&
              reduced.attacks[0].modifier == fighter.attacks[0].modifier,
          "Reduce takes the Enlarge away");

    // Gateway's Strength, cast in a fight on the caster: a magic-user's 1d4.
    auto strong =
        arrange({{"Mage", rules::Side::party, {0, 3}, caster(ClassSlot::magic_user, 4, {35})},
                 {"Kobold", rules::Side::opposition, {7, 7}, creature(20, 0)}},
                337, {}, 100, GoldBoxGame::gateway_to_the_savage_frontier);
    until_turn_of(*strong.session, "Mage");
    check(strong.session->submit(*cast_command(*strong.session, 35)), "Strength is cast");
    play_until(
        *strong.session,
        [&](const std::string &)
        {
            return order(*strong.session, "end");
        },
        [&]
        {
            return logged(*strong.session, "{name} casts {spell}.") > 0;
        });
    const auto raised = left_by_fight(*strong.session, "Mage");
    check(raised.strength >= 11 && raised.strength <= 14,
          "Gateway's Strength adds a magic-user's 1d4");
}

// Out of doors (Curse's SpellFireball and SpellEntangle): a fireball catches
// only those within 2 squares of its aim, and Entangle's plants hold those
// that fail to save; in a dungeon Entangle does nothing.
void out_of_doors()
{
    using original::ClassSlot;
    const auto curse = GoldBoxGame::curse_of_the_azure_bonds;
    for (const bool outdoors : {false, true})
    {
        auto fight =
            arrange({{"Mage", rules::Side::party, {0, 3}, caster(ClassSlot::magic_user, 6, {47})},
                     {"Near", rules::Side::opposition, {5, 3}, victim(100)},
                     {"Third", rules::Side::opposition, {5, 6}, victim(100)}},
                    347, {}, 100, curse, outdoors);
        cast_now(*fight.session, "Mage", 47,
                 [&]
                 {
                     return aim_at_cell(*fight.session, {5, 3});
                 });
        const auto state = fight.session->snapshot();
        check(view_of(state, "Near").hit_points < 100 &&
                  (view_of(state, "Third").hit_points == 100) == outdoors,
              outdoors ? "Out of doors a fireball reaches 2 squares"
                       : "In a dungeon a fireball reaches 3 squares");

        auto grove =
            arrange({{"Druid", rules::Side::party, {0, 3}, caster(ClassSlot::ranger, 9, {178})},
                     {"Orc", rules::Side::opposition, {4, 3}, victim(100)}},
                    349, {}, 100, curse, outdoors);
        cast_now(*grove.session, "Druid", 178,
                 [&]
                 {
                     return aim_at_cell(*grove.session, {4, 3});
                 });
        check((logged(*grove.session, "{name} is affected by {spell}.") == 1) == outdoors &&
                  (logged(*grove.session, "Nothing happens.") == 1) == !outdoors,
              outdoors ? "Entangle holds out of doors" : "Entangle does nothing in a dungeon");
    }
}

// Curse's rakshasa (its effect 130) is sent away for good by a quarrel of
// word 135 (effhndlr.pas, EffBanishByWeapon), and only by that.
void a_quarrel_banishes_the_rakshasa()
{
    const auto curse = GoldBoxGame::curse_of_the_azure_bonds;
    auto rakshasa = creature(200);
    rakshasa.effects = {{330, 0, 0, false}};
    auto archer = monster({2, 1, 4, 0}, {}, {});
    archer.striker = original::StrikingItem{0, false, 0, true};
    const auto banished = attack_until(
        archer, rakshasa,
        [](const rules::CombatSession &session)
        {
            return logged(session, "{name} is banished.") > 0;
        },
        353, curse);
    check(logged(*banished.session, "{name} is banished.") == 1,
          "A banishing quarrel sends the rakshasa away");
    archer.striker = original::StrikingItem{0, false, 0, false};
    const auto plain = attack_until(
        archer, rakshasa,
        [](const rules::CombatSession &session)
        {
            return logged(session, "{attacker} hits {name} for {damage}.") > 2;
        },
        353, curse);
    check(logged(*plain.session, "{name} is banished.") == 0, "Another blow only wounds it");
}

// The kit's monster with its effect 82 dies of a blow of its quarrel of
// slaying (effects.c, fx_slain_by_type), as Pools of Darkness's rakshasa of
// a blessed bolt, and of no other blow outright.
void a_quarrel_of_slaying_slays_its_quarry()
{
    const auto kit = GoldBoxGame::unlimited_adventures;
    auto quarry = creature(200);
    quarry.effects = {{original::canonical_effect(82, kit), 0, 0xFF, true}};
    auto archer = monster({2, 1, 4, 0}, {}, {});
    archer.striker = original::StrikingItem{0, false, 0, false, true};
    const auto slain = attack_until(
        archer, quarry,
        [](const rules::CombatSession &session)
        {
            return logged(session, "{name} is slain.") > 0;
        },
        353, kit);
    check(logged(*slain.session, "{name} is slain.") == 1, "A quarrel of slaying slays its quarry");
    archer.striker = original::StrikingItem{0, false, 0, false, false};
    const auto plain = attack_until(
        archer, quarry,
        [](const rules::CombatSession &session)
        {
            return logged(session, "{attacker} hits {name} for {damage}.") > 2;
        },
        353, kit);
    check(logged(*plain.session, "{name} is slain.") == 0, "Another blow only wounds it");
}

// Pools of Darkness's rakshasa (its effect 82) is slain by a blessed bolt
// (item type 74; effhndlr.pas, EffBlessedBoltKills), and a crossbow shoots
// blessed bolts as quarrels (charhlp.pas, RecalcStats).
void a_blessed_bolt_slays_the_rakshasa()
{
    const auto slain_by_blessed_bolt = original::canonical_effect(82, darkness_game);
    auto rakshasa = creature(200);
    rakshasa.effects = {{slain_by_blessed_bolt, 0, 0, false}};
    auto archer = monster({2, 1, 4, 0}, {}, {});
    archer.striker = original::StrikingItem{.blessed_bolt = true};
    const auto slain = attack_until(
        archer, rakshasa,
        [](const rules::CombatSession &session)
        {
            return logged(session, "{name} is slain.") > 0;
        },
        353, darkness_game);
    check(logged(*slain.session, "{name} is slain.") == 1 &&
              view_of(slain.session->snapshot(), "Victim").hit_points == 0,
          "A blessed bolt slays the rakshasa");
    archer.striker = original::StrikingItem{.plus = 4};
    const auto wounded = attack_until(
        archer, rakshasa,
        [](const rules::CombatSession &session)
        {
            return logged(session, "{attacker} hits {name} for {damage}.") > 2;
        },
        353, darkness_game);
    check(logged(*wounded.session, "{name} is slain.") == 0, "Another bolt only wounds it");

    const auto ammunition = original::ammunition_types(darkness_game);
    check(ammunition.arrows == 30 && ammunition.shot_as_quarrel(12) &&
              ammunition.shot_as_quarrel(74) &&
              !original::ammunition_types(GoldBoxGame::secret_of_the_silver_blades)
                   .shot_as_quarrel(74),
          "Pools of Darkness's crossbow shoots quarrels and blessed bolts");
}

// Sight weighs the original's heights (pathing.pas, WalkLine): a square
// blocks it when its blocking is above the clearance of the square looked
// from, so ground that stops a step may not stop an arrow, and one on high
// ground shoots over what stops the others.
void heights_decide_sight()
{
    const auto can_shoot = [](int blocking, int archer_clearance)
    {
        auto fight = arrange({{"Archer", rules::Side::party, {0, 3}, archer(10)},
                              {"Ogre", rules::Side::opposition, {7, 3}, creature(1000, 0)}},
                             21, {{3, 3}}, 100, GoldBoxGame::pool_of_radiance, false, false,
                             [&](rules::Encounter &encounter)
                             {
                                 auto &board = encounter.battlefield;
                                 board.clearance.assign(board.terrain.size(), 1);
                                 board.blocking.assign(board.terrain.size(), 0);
                                 board.blocking[3 * 8 + 3] = static_cast<std::uint8_t>(blocking);
                                 board.clearance[3 * 8 + 0] =
                                     static_cast<std::uint8_t>(archer_clearance);
                             });
        until_turn_of(*fight.session, "Archer");
        return offered(*fight.session, "ranged", "Ogre").has_value();
    };
    check(can_shoot(0, 1), "Impassable ground of no height lets the arrow by");
    check(!can_shoot(2, 1), "A tree stops it");
    check(can_shoot(2, 2), "From a hill the archer shoots over the tree");
}

// The party's facing (combatst.pas and combatai.pas): its enemies flee along
// it, east when it faced east and west when it faced west; and the script's
// change to the party's movement moves only the party (cbtacts.pas,
// CalcCreatureMovement), a byte that wraps.
void the_party_facing_and_movement()
{
    const auto flees_to = [](int facing)
    {
        auto coward = creature(20, 0);
        coward.hit_points = 2;
        coward.morale = 0x80 + 10;
        coward.movement = 12;
        auto slow = creature(50);
        slow.movement = 1;
        auto fight = arrange({{"Fighter", rules::Side::party, {3, 7}, slow},
                              {"Coward", rules::Side::opposition, {4, 3}, coward}},
                             191, {}, 50, GoldBoxGame::pool_of_radiance, false, false,
                             [facing](rules::Encounter &encounter)
                             {
                                 encounter.party_facing = facing;
                             });
        auto &session = *fight.session;
        all_quick(
            session,
            [&]
            {
                return view_of(session.snapshot(), "Coward").fled;
            },
            10);
        const auto &view = view_of(session.snapshot(), "Coward");
        return view.fled ? view.cell.x : -1;
    };
    check(flees_to(2) == 7 && flees_to(6) == 0,
          "The party's enemies flee along its facing, off the side it faced");

    const auto movement_feet = [](int modifier)
    {
        auto walker = creature(50);
        walker.movement = 6;
        auto fight = arrange({{"Walker", rules::Side::party, {0, 3}, walker},
                              {"Kobold", rules::Side::opposition, {7, 3}, walker}},
                             21, {}, 100, GoldBoxGame::pool_of_radiance, false, false,
                             [modifier](rules::Encounter &encounter)
                             {
                                 encounter.party_move_modifier = modifier;
                             });
        const auto state = fight.session->snapshot();
        return std::pair{view_of(state, "Walker").movement_feet,
                         view_of(state, "Kobold").movement_feet};
    };
    check(movement_feet(3) == std::pair{45, 30}, "3 more squares for the party alone");
    check(movement_feet(253) == std::pair{15, 30}, "253 takes 3 away");
    check(movement_feet(250) == std::pair{5, 30}, "Below 1 is 1");

    // Pools of Darkness adds the modifier after Haste (charhlp.pas,
    // RecalcMovement): a hasted 12 less 6 is 18, where Pool of Radiance
    // doubles 12 less 6, 12.
    const auto hasted_feet = [](GoldBoxGame game, int modifier)
    {
        auto walker = creature(50);
        walker.movement = 12;
        walker.effects = {{39, 5, 0, false}};
        auto fight = arrange({{"Walker", rules::Side::party, {0, 3}, walker},
                              {"Kobold", rules::Side::opposition, {7, 3}, creature(50)}},
                             21, {}, 100, game, false, false,
                             [modifier](rules::Encounter &encounter)
                             {
                                 encounter.party_move_modifier = modifier;
                             });
        return view_of(fight.session->snapshot(), "Walker").movement_feet;
    };
    check(hasted_feet(GoldBoxGame::pools_of_darkness, 250) == 90 &&
              hasted_feet(GoldBoxGame::pool_of_radiance, 250) == 60,
          "Pools of Darkness takes the 6 from the hasted movement");
    check(hasted_feet(GoldBoxGame::pools_of_darkness, 230) == 5, "Below 1 is 1 there too");
}

// Spiritual Hammer (magic.pas, SpellSpiritualHammer; effhndlr.pas,
// EffSpiritualHammer): its caster gains a hammer +1 while the spell lasts, a
// minute (a round) a level. Pool of Radiance leaves it to be readied and ends
// the spell when it is put away; Curse readies it at once and keeps it put
// away; Secret readies it when no weapon is.
void spiritual_hammer_gives_its_hammer()
{
    using original::ClassSlot;

    struct Cast
    {
        Fight fight;
        bool gained{};
    };

    const auto cast_hammer = [](GoldBoxGame game, std::uint8_t hammer_type)
    {
        std::vector<por::ItemTemplate> types(256);
        auto &hammer = types[hammer_type];
        hammer.hands = 1;
        hammer.small_medium_damage = {1, 4, 1};
        hammer.large_damage = {1, 4, 0};
        hammer.class_restrictions = 0xFF;
        por::CharacterRecord record;
        record.game = game;
        record.base_thac0 = 20;
        record.base_armor_class = 10;
        record.max_hit_points = 30;
        record.base_movement = 12;
        record.base_attacks[0] = {2, {1, 2, 0}};
        record.current_abilities = {10, 10, 10, 10, 10, 10, 0};
        record.levels.cleric = 3;
        record.item_limit = 0xFF;
        Cast cast{arrange(
            {{"Cleric", rules::Side::party, {1, 3}, caster(ClassSlot::cleric, 3, {28})},
             {"Kobold", rules::Side::opposition, {6, 3}, creature(200, 0)}},
            71, {}, 100, game, false, false,
            [&record](rules::Encounter &encounter)
            {
                encounter.participants.front().character_record =
                    original::encode_fight_record(record);
            },
            types)};
        auto &session = *cast.fight.session;
        until_turn_of(session, "Cleric");
        check(session.submit(*cast_command(session, 28)), "The cleric casts Spiritual Hammer");
        for (int step = 0; step < 40 && logged(session, "{name} casts {spell}.") == 0; ++step)
        {
            if (actor_name(session) == "Cleric")
            {
                if (const auto aim = aim_at_cell(session, {1, 3}))
                {
                    session.submit(*aim);
                    continue;
                }
            }
            session.submit(order(session, "end"));
        }
        cast.gained = logged(session, "{name} gains an item.") == 1;
        return cast;
    };
    const auto hammer_command = [](const rules::CombatSession &session,
                                   std::string_view verb) -> std::optional<rules::Command>
    {
        for (const auto &c : session.legal_commands())
        {
            if (c.verb == verb && c.label == "Spiritual Hammer")
            {
                return c;
            }
        }
        return std::nullopt;
    };
    const auto offered_hammer = [&](const rules::CombatSession &session, std::string_view verb)
    {
        return hammer_command(session, verb).has_value();
    };
    const auto hammer_blow = [](const rules::CombatSession &session)
    {
        const auto blow = left_by_fight(session, "Cleric").attacks[0];
        return blow.dice == 1 && blow.sides == 4 && blow.modifier == 2;
    };

    auto pool = cast_hammer(GoldBoxGame::pool_of_radiance, 20);
    auto &pool_fight = *pool.fight.session;
    until_turn_of(pool_fight, "Cleric");
    check(pool.gained && offered_hammer(pool_fight, "ready") && !hammer_blow(pool_fight),
          "Pool of Radiance's hammer comes to be readied");
    pool_fight.submit(*hammer_command(pool_fight, "ready"));
    check(hammer_blow(pool_fight), "Readied, it strikes for 1d4+1 and its +1");
    pool_fight.submit(*hammer_command(pool_fight, "unready"));
    check(!offered_hammer(pool_fight, "ready") && !offered_hammer(pool_fight, "unready"),
          "Put away, it ends its spell and is gone");

    auto curse = cast_hammer(GoldBoxGame::curse_of_the_azure_bonds, 20);
    auto &curse_fight = *curse.fight.session;
    check(curse.gained && hammer_blow(curse_fight), "Curse's hammer comes readied");
    until_turn_of(curse_fight, "Cleric");
    curse_fight.submit(*hammer_command(curse_fight, "unready"));
    check(offered_hammer(curse_fight, "ready"), "Put away, Curse's hammer stays");

    auto secret = cast_hammer(secret_game, 6);
    auto &secret_fight = *secret.fight.session;
    check(secret.gained && hammer_blow(secret_fight),
          "Secret's hammer comes readied to an empty hand");
    for (int step = 0; step < 200 && secret_fight.snapshot().round < 6; ++step)
    {
        secret_fight.submit(order(secret_fight, "end"));
    }
    until_turn_of(secret_fight, "Cleric");
    check(!offered_hammer(secret_fight, "unready") && !hammer_blow(secret_fight),
          "Its 3 rounds over, the hammer is gone");
    check(view_of(secret_fight.snapshot(), "Cleric").items_gained.empty(),
          "A hammer gone with its spell does not outlast the fight");
    const auto kept = view_of(curse_fight.snapshot(), "Cleric").items_gained;
    const auto record =
        kept.empty() ? std::nullopt : por::item_of_original_key(kept.front().definition);
    check(kept.size() == 1 && kept.front().name == "Spiritual Hammer" && record &&
              original::is_spiritual_hammer(*record, GoldBoxGame::curse_of_the_azure_bonds),
          "While its spell lasts, the hammer outlasts the fight as an item its caster carries");
}

// A readied item lays its own effect on its bearer (viewchar.pas,
// ToggleReadyItem and ToggleItemEffect): in Pool of Radiance by its special
// byte's handler, from Curse on by the byte's 0x80 alone, read by the fight's
// numbers; a record holding the effect already keeps one.
void readied_items_give_their_effects()
{
    const auto effects_with = [](GoldBoxGame game, std::array<std::uint8_t, 3> bytes, bool readied,
                                 std::vector<por::EffectRecord> own = {})
    {
        auto record = kobold_record();
        record.game = game;
        por::ItemRecord item;
        item.readied_raw = readied ? 1 : 0;
        item.effect_codes = bytes;
        por::ItemTemplate ring;
        ring.worn_location = 9;
        const std::vector<original::CarriedItem> gear{{item, ring}};
        std::vector<int> codes;
        for (const auto &e : original::combatant_from_record(record, gear, own).effects)
        {
            codes.push_back(e.code);
        }
        return codes;
    };
    constexpr auto pool = GoldBoxGame::pool_of_radiance;
    check(effects_with(pool, {0, 61, 0x81}, true) == std::vector<int>{61} &&
              effects_with(pool, {0, 61, 0x81}, false).empty(),
          "A Ring of Fire Resistance gives its effect while readied");
    check(effects_with(pool, {0, 38, 0x83}, true).empty(),
          "Gauntlets of Ogre Power's handler grants no effect");
    check(effects_with(secret_game, {0, 75, 0x80}, true) == std::vector<int>{475} &&
              effects_with(secret_game, {0, 5, 0x85}, true).empty(),
          "Secret's giant-bane sword gives its effect by Secret's numbers");
    check(effects_with(GoldBoxGame::curse_of_the_azure_bonds, {0, 89, 0x80}, true) ==
              std::vector<int>{89},
          "Curse's Cloak of Displacement displaces its wearer");
    por::EffectRecord displaced;
    displaced.code = 89;
    check(effects_with(secret_game, {0, 89, 0x80}, true, {displaced}) == std::vector<int>{89},
          "A record holding the item's effect keeps one");
    constexpr auto treasures = GoldBoxGame::treasures_of_the_savage_frontier;
    check(effects_with(GoldBoxGame::pools_of_darkness, {0, 89, 0x80}, true) ==
                  std::vector<int>{89} &&
              effects_with(GoldBoxGame::pools_of_darkness, {0, 89, 0x8F}, true).empty(),
          "Pools of Darkness grants by the special byte's 0x80 alone, by its numbers");
    check(effects_with(treasures, {0, 89, 0x8F}, true) == std::vector<int>{89} &&
              effects_with(treasures, {0, 89, 0x0F}, true) == std::vector<int>{89} &&
              effects_with(treasures, {0, 23, 0x80}, true).empty(),
          "Treasures grants by 0x0F and 0x8F too, but never Spiritual Hammer's effect");
    check(effects_with(GoldBoxGame::gateway_to_the_savage_frontier, {0, 89, 0x80}, true) ==
                  std::vector<int>{89} &&
              effects_with(GoldBoxGame::unlimited_adventures, {0, 89, 0x80}, true) ==
                  std::vector<int>{89},
          "Gateway's and Unlimited Adventures' cloaks of displacement displace their wearers");
}

// The original rules' difficulty changes the monsters only (Difficulty,
// 2026-11-15): their to-hit, Hit Points and Armor Class, Normal nothing; a game
// that scales Hit Points by its own levels keeps that scaling.
void difficulty_changes_the_monsters()
{
    using original::DifficultyLevel;
    check(original::game_hit_points_percent(DifficultyLevel::easy) == 75 &&
              original::game_hit_points_percent(DifficultyLevel::normal) == 100 &&
              original::game_hit_points_percent(DifficultyLevel::nightmare) == 150 &&
              original::game_hit_points_percent(DifficultyLevel::infernal) == 200,
          "LOAD MONSTER's quarters: Squire to Champion, and Infernal twice");
    check(original::scales_monster_hit_points(secret_game) &&
              original::scales_monster_hit_points(GoldBoxGame::treasures_of_the_savage_frontier) &&
              !original::scales_monster_hit_points(GoldBoxGame::pool_of_radiance) &&
              !original::scales_monster_hit_points(GoldBoxGame::curse_of_the_azure_bonds),
          "Secret and the later games scale Hit Points themselves");
    for (const rules::OriginalStrength strength :
         {rules::OriginalStrength{75, -2, -2}, rules::OriginalStrength{},
          rules::OriginalStrength{200, 3, 3}})
    {
        auto monster = creature(41);
        monster.thac0 = 15;
        monster.armor_class = monster.rear_armor_class = 4;
        auto fight = arrange({{"Fighter", rules::Side::party, {1, 3}, creature(30)},
                              {"Kobold", rules::Side::opposition, {6, 3}, monster}},
                             5, {}, 100, GoldBoxGame::pool_of_radiance, false, false,
                             [&strength](rules::Encounter &encounter)
                             {
                                 encounter.participants.back().original_strength = strength;
                             });
        const auto &kobold = view_of(fight.session->snapshot(), "Kobold");
        const auto &fighter = view_of(fight.session->snapshot(), "Fighter");
        check(kobold.max_hit_points == 41 * static_cast<int>(strength.hit_points_percent) / 100 &&
                  kobold.armor_class == 4 - strength.armor_class_bonus &&
                  fighter.max_hit_points == 30,
              "The monster's Hit Points and Armor Class follow the level, the party's not");
    }
}

// Gateway's effects by the fight's numbers are Curse's, but for its own; Pools
// of Darkness' and Treasures' are Secret's, but for those their engine moved
// and its own (canonical_effect, docs/ORIGINAL-RULES.md).
void later_games_effects_take_their_own_numbers()
{
    constexpr auto gateway = GoldBoxGame::gateway_to_the_savage_frontier;
    constexpr auto darkness = GoldBoxGame::pools_of_darkness;
    constexpr auto treasures = GoldBoxGame::treasures_of_the_savage_frontier;
    for (const int code : {3, 7, 13, 27, 35, 57, 87, 122, 139, 144, 147})
    {
        check(original::canonical_effect(code, gateway) ==
                  original::canonical_effect(code, GoldBoxGame::curse_of_the_azure_bonds),
              "Gateway's effect " + std::to_string(code) + " is Curse's");
    }
    check(original::canonical_effect(160, gateway) == 137 &&
              original::canonical_effect(126, gateway) == 126 + original::gateway_effect_offset &&
              original::canonical_effect(158, gateway) == 158 + original::gateway_effect_offset,
          "Gateway's own effects take numbers of their own");
    for (int code = 1; code <= 127; ++code)
    {
        check(original::canonical_effect(code, treasures) ==
                  original::canonical_effect(code, darkness),
              "Treasures' effect " + std::to_string(code) + " is Pools of Darkness's");
    }
    check(original::canonical_effect(156, treasures) == 336 &&
              original::canonical_effect(140, treasures) == 140 + original::vga_effect_offset,
          "Treasures' later effects: holding still as Curse's 136, the rest its own");
    original::Combatant vorpal;
    vorpal.effects.push_back({original::canonical_effect(160, treasures), 0, 255, true});
    check(original::Combatant::decode(vorpal.encode()) == vorpal,
          "A creature keeps the engine's own effects through its profile");
}

// Gateway's effects that share Curse's numbers but not its rules (its
// effhndlr.pas): a shambling mound engulfs on 1 to 4 of a d10 at any hit, not
// on two hits; poison rides the first attack. And its own: a stirge's and a
// squid's holds, a harpy's charming touch, a mummy's disease, Vaalgamon's
// regeneration and a black dragon's surrender when last.
void gateway_effects_play_their_own()
{
    constexpr auto gateway = GoldBoxGame::gateway_to_the_savage_frontier;
    constexpr auto curse = GoldBoxGame::curse_of_the_azure_bonds;
    const auto with = [](int code, int rate = 2)
    {
        auto c = creature(500);
        c.attacks[0] = {rate, 1, 2, 0};
        c.effects.push_back({code, 0, 0, false});
        return c;
    };
    const auto logs = [&](const original::Combatant &attacker, std::string_view line,
                          GoldBoxGame game, std::string_view who = {})
    {
        auto fight = attack_until(
            attacker, victim(500),
            [line, who](const rules::CombatSession &session)
            {
                return logged(session, line, who) > 0;
            },
            47, game);
        return logged(*fight.session, line, who) > 0;
    };
    check(logs(with(257), "{attacker} engulfs {name}.", gateway, "Monster") &&
              !logs(with(257), "{attacker} engulfs {name}.", curse, "Monster"),
          "Gateway's shambling mound engulfs at a single blow; Curse's needs two");
    check(logs(with(64), "{name} is poisoned.", gateway) &&
              !logs(with(64), "{name} is poisoned.", curse),
          "Gateway's poison rides the first attack, Curse's the second");
    check(logs(with(original::canonical_effect(149, gateway)), "{attacker} attaches to {name}.",
               gateway, "Monster"),
          "A Gateway stirge takes hold");
    check(logs(with(original::canonical_effect(155, gateway)), "{attacker} constricts {name}.",
               gateway, "Monster"),
          "A Gateway squid constricts");
    check(logs(with(original::canonical_effect(154, gateway)), "{name} is charmed.", gateway),
          "A Gateway harpy's touch charms");
    check(logs(with(original::canonical_effect(159, gateway)), "{name} is diseased.", gateway),
          "A Gateway mummy's touch diseases");

    auto dragon = with(original::canonical_effect(158, gateway));
    dragon.hit_points = 15;
    check(logs(dragon, "{name} surrenders.", gateway),
          "Gateway's black dragon, last of its side, surrenders below 20 Hit Points");

    auto vaalgamon = with(original::canonical_effect(161, gateway));
    vaalgamon.hit_points = 400;
    auto fight = attack_until(
        vaalgamon, victim(500),
        [](const rules::CombatSession &session)
        {
            return session.snapshot().round > 3;
        },
        47, gateway);
    check(view_of(fight.session->snapshot(), "Monster").hit_points > 400, "Vaalgamon regenerates");
}

// Treasures' crystal battles (its effhndlr.pas, EffRedCrystal and
// ShowCrystalSmash): the first creature carrying a crystal to fall gives it
// up, and the others smash theirs; outside such a battle no crystal is won.
void treasures_crystal_battle_yields_one_crystal()
{
    constexpr auto treasures = GoldBoxGame::treasures_of_the_savage_frontier;
    auto fighter = creature(50);
    fighter.thac0 = -20;
    fighter.attacks[0] = {2, 1, 2, 40};
    const auto bearer = [&](int code)
    {
        auto c = creature(5);
        c.effects.push_back({original::canonical_effect(code, treasures), 0, 0, false});
        return c;
    };
    const auto battle = [&](bool crystals)
    {
        return arrange({{"Fighter", rules::Side::party, {3, 3}, fighter},
                        {"Red", rules::Side::opposition, {2, 2}, bearer(128)},
                        {"Green", rules::Side::opposition, {4, 3}, bearer(129)},
                        {"Blue", rules::Side::opposition, {2, 4}, bearer(130)}},
                       47, {}, 100, treasures, false, false,
                       [crystals](rules::Encounter &encounter)
                       {
                           encounter.crystals = crystals;
                       });
    };
    auto fight = battle(true);
    auto &session = *fight.session;
    until_turn_of(session, "Fighter");
    session.submit(order(session, "melee", "Green"));
    check(view_of(session.snapshot(), "Green").hit_points <= 0 && session.snapshot().crystal == 1U,
          "The green crystal's bearer falls first: the party has its crystal");
    const auto smashes = [&](std::string_view name)
    {
        const auto line = std::string(name) + " smashes a crystal.";
        return std::ranges::count(session.snapshot().log_entries, line, &rules::LogEntry::english);
    };
    check(smashes("Red") == 1 && smashes("Blue") == 1 && smashes("Green") == 0,
          "The other bearers smash theirs");
    until_turn_of(session, "Fighter");
    session.submit(order(session, "melee", "Red"));
    check(view_of(session.snapshot(), "Red").hit_points <= 0 && session.snapshot().crystal == 1U,
          "A second bearer's fall gives no other crystal");
    const auto restored = fight.module->restore(session.save());
    check(restored && restored->snapshot().crystal == 1U, "A saved fight keeps the crystal won");

    auto ordinary = battle(false);
    until_turn_of(*ordinary.session, "Fighter");
    ordinary.session->submit(order(*ordinary.session, "melee", "Green"));
    check(!ordinary.session->snapshot().crystal &&
              logged(*ordinary.session, "{name} smashes a crystal.") == 0,
          "Outside a crystal battle no crystal is won");
}

// Treasures' own effects (its effhndlr.pas): a kampfult crushes what it
// hits, a yeti grabs and a remorhaz swallows whole at an attack roll of 20.
void treasures_effects_play_their_own()
{
    constexpr auto treasures = GoldBoxGame::treasures_of_the_savage_frontier;
    const auto with = [](int code)
    {
        auto c = creature(500);
        c.attacks[0] = {20, 1, 2, 0};
        c.effects.push_back({code, 0, 0, false});
        return c;
    };
    const auto logs = [&](const original::Combatant &attacker, std::string_view line)
    {
        auto fight = attack_until(
            attacker, victim(500),
            [line](const rules::CombatSession &session)
            {
                return logged(session, line) > 0;
            },
            47, treasures);
        return logged(*fight.session, line) > 0;
    };
    check(logs(with(original::canonical_effect(144, treasures)), "{name} is being crushed.") &&
              logs(with(original::canonical_effect(144, treasures)),
                   "{attacker} squeezes {name} for {damage}."),
          "A kampfult crushes what it holds");
    check(logs(with(original::canonical_effect(149, treasures)), "{attacker} grabs {name}."),
          "A yeti grabs at an attack roll of 20");
    check(logs(with(original::canonical_effect(155, treasures)), "{name} is swallowed whole."),
          "A remorhaz swallows whole");
    check(original::canonical_effect(132, treasures) ==
                  original::canonical_effect(154, GoldBoxGame::gateway_to_the_savage_frontier) &&
              original::canonical_effect(136, treasures) ==
                  original::canonical_effect(151, GoldBoxGame::gateway_to_the_savage_frontier),
          "Treasures' harpy and Icewind Dale effects are Gateway's");
}

// A Treasures stone guardian (its effect 141, EffResistItemTypes) takes a
// quarter from an edge or a point, and nothing from a weapon whose type may
// be thrown (property 10h), even one swung in melee.
void a_stone_guardian_turns_thrown_weapons()
{
    constexpr auto treasures = GoldBoxGame::treasures_of_the_savage_frontier;
    const auto first_blow = [&](const original::StrikingItem &weapon)
    {
        auto striker = monster({2, 1, 1, 7}, {0, 0, 0, 0}, {});
        striker.striker = weapon;
        auto guardian = victim(500);
        guardian.effects.push_back({original::canonical_effect(141, treasures), 0, 0, false});
        auto fight = attack_until(
            striker, guardian,
            [](const rules::CombatSession &session)
            {
                return logged(session, "{attacker} hits {name} for {damage}.", "Monster") > 0;
            },
            47, treasures);
        return 500 - view_of(fight.session->snapshot(), "Victim").hit_points;
    };
    constexpr int slashing = 0, bludgeoning = 0x80;
    check(first_blow({0, false, bludgeoning}) == 8, "A mace does its 8");
    check(first_blow({0, false, slashing}) == 2, "A sword does a quarter of its 8");
    original::StrikingItem hammer{0, false, bludgeoning};
    hammer.thrown = true;
    check(first_blow(hammer) == 0, "A hammer of a type that may be thrown does nothing");
    original::Combatant thrown;
    thrown.striker = hammer;
    check(original::Combatant::decode(thrown.encode())->striker == hammer,
          "A profile keeps that the weapon may be thrown");
}

// A Treasures yeti's fear when it surprises (its effect 152,
// EffFrightenAura): with the script's cell 53h set as the fight begins,
// each enemy that fails a save is frightened; without it, none is.
void yetis_frighten_when_they_surprise()
{
    constexpr auto treasures = GoldBoxGame::treasures_of_the_savage_frontier;
    auto yeti = creature(50);
    yeti.effects.push_back({original::canonical_effect(152, treasures), 0, 0, false});
    const auto frightened = [&](bool surprise)
    {
        const auto fight = arrange({{"Victim", rules::Side::party, {3, 3}, victim(50)},
                                    {"Yeti", rules::Side::opposition, {6, 3}, yeti}},
                                   47, {}, 100, treasures, false, false,
                                   [surprise](rules::Encounter &encounter)
                                   {
                                       encounter.frightening_surprise = surprise;
                                   });
        return logged(*fight.session, "{name} is frightened.") > 0;
    };
    check(frightened(true), "Surprised by the yeti, the victim is frightened");
    check(!frightened(false), "Met otherwise, it is not");

    // The fright lasts 2 rounds, and its flight with it (effhndlr.pas,
    // EffFear, as the effect is removed): the victim is forced to flee no
    // more after that.
    auto fight = arrange({{"Victim", rules::Side::party, {3, 3}, victim(50)},
                          {"Yeti", rules::Side::opposition, {6, 3}, yeti}},
                         47, {}, 100, treasures, false, false,
                         [](rules::Encounter &encounter)
                         {
                             encounter.frightening_surprise = true;
                         });
    auto &session = *fight.session;
    play_until(
        session,
        [&](const std::string &)
        {
            return order(session, "end");
        },
        [&]
        {
            return session.snapshot().round > 6;
        });
    const int flights = logged(session, "{name} is forced to flee.");
    check(flights > 0 && flights <= 3, "The flight ends with the fright: forced to flee " +
                                           std::to_string(flights) + " times in 6 rounds");
}

// A Treasures stone golem (its effect 147, EffSlowAura): stone, it slows an
// enemy at its turn and takes nothing from a weapon below +2 nor from a
// spell; flesh (after Stone to Flesh), it takes any weapon's blow and counts
// its turns down to stone again.
void a_stone_golem_slows_and_turns_weapons_aside()
{
    constexpr auto treasures = GoldBoxGame::treasures_of_the_savage_frontier;
    const auto golem_effect = [](int strength)
    {
        return original::Effect{original::canonical_effect(147, treasures), 0, strength, false};
    };
    const auto golem = monster({2, 1, 1, 0}, {0, 0, 0, 0}, {golem_effect(255)});
    // The golem plays by the computer, as its specials come at its turn.
    const auto logs = [&](const original::Combatant &attacker, std::string_view line)
    {
        auto fight = arrange({{"Victim", rules::Side::party, {3, 3}, victim(500)},
                              {"Monster", rules::Side::opposition, {4, 3}, attacker}},
                             47, {}, 100, treasures);
        auto &session = *fight.session;
        play_until(
            session,
            [&](const std::string &actor)
            {
                return order(session, actor == "Monster" ? "quick" : "end");
            },
            [&]
            {
                return logged(session, line) > 0 || session.snapshot().round > 10;
            },
            400);
        return logged(session, line) > 0;
    };
    check(logs(golem, "{name} slows {target}."), "The stone golem slows its enemy");
    check(logs(monster({2, 1, 1, 0}, {0, 0, 0, 0}, {golem_effect(1)}), "{name} returns to stone."),
          "Flesh, it returns to stone");
    const auto first_blow = [&](int plus, int strength)
    {
        auto striker = monster({2, 1, 1, 7}, {0, 0, 0, 0}, {});
        striker.striker = original::StrikingItem{plus, false, 0x80};
        auto target = victim(500);
        target.effects.push_back(golem_effect(strength));
        auto fight = attack_until(
            striker, target,
            [](const rules::CombatSession &session)
            {
                return logged(session, "{attacker} hits {name} for {damage}.", "Monster") > 0;
            },
            47, treasures);
        return 500 - view_of(fight.session->snapshot(), "Victim").hit_points;
    };
    check(first_blow(1, 255) == 0 && first_blow(2, 255) > 0,
          "Stone, it takes nothing from a +1 weapon and a +2's blow");
    check(first_blow(0, 1) > 0, "Flesh, a plain weapon harms it");
}

// Secret's giant-bane weapon (its effect 75, EffHolyWater): against one its
// record flags for it, 1 better to hit and 6d6 with the Strength bonus in
// place of the weapon's own damage; against others the weapon's.
void a_giant_bane_weapon_bites_giants()
{
    auto slayer = monster({2, 1, 1, 0}, {0, 0, 0, 0}, {{475, 0, 255, true}});
    slayer.strength = slayer.natural_strength = 18;
    slayer.exceptional_strength = slayer.natural_exceptional_strength = 100;
    const auto first_blow = [&](const original::Combatant &target)
    {
        auto fight = attack_until(
            slayer, target,
            [](const rules::CombatSession &session)
            {
                return logged(session, "{attacker} hits {name} for {damage}.", "Monster") > 0;
            },
            47, secret_game);
        return 500 - view_of(fight.session->snapshot(), "Victim").hit_points;
    };
    auto giant = victim(500);
    giant.traits = original::trait::giant_bane;
    const int bitten = first_blow(giant);
    check(bitten >= 6 + 6 && bitten <= 36 + 6,
          "A giant takes 6d6 and 18/00's 6 from the giant-bane weapon");
    check(first_blow(victim(500)) == 1, "Another takes the weapon's own 1");
}

// Secret's Delayed Blast Fireball waits the rounds its caster chooses on its
// square, then catches all within 3 (SpellDelayedBlastFireball, effect 100);
// a checkpoint keeps it waiting.
void a_blast_waits()
{
    using original::ClassSlot;
    auto fight = arrange_secret(
        {{"Mage", rules::Side::party, {0, 3}, caster(ClassSlot::magic_user, 14, {315})},
         {"Near", rules::Side::opposition, {5, 3}, victim(200)}},
        359);
    auto &session = *fight.session;
    cast_now(session, "Mage", 315,
             [&]
             {
                 const auto legal = session.legal_commands();
                 const auto two =
                     std::ranges::find_if(legal,
                                          [](const rules::Command &c)
                                          {
                                              return c.verb == "blast_delay" && c.item == 2;
                                          });
                 check(two != legal.end(), "The caster chooses the delay");
                 session.submit(*two);
                 return aim_at_cell(session, {5, 3});
             });
    check(logged(session, "{name} sets a delayed blast fireball.") == 1 &&
              view_of(session.snapshot(), "Near").hit_points == 200,
          "The blast waits");
    const auto saved = session.save();
    check(fight.module->restore(saved)->save() == saved, "A checkpoint keeps the blast");
    play_until(
        session,
        [&](const std::string &)
        {
            return order(session, "end");
        },
        [&]
        {
            return logged(session, "Delayed Blast Fireball goes off.") > 0 ||
                   session.snapshot().round > 6;
        });
    check(logged(session, "Delayed Blast Fireball goes off.") == 1 &&
              view_of(session.snapshot(), "Near").hit_points < 200,
          "It goes off on its square when its rounds are up");
}

// The fight the original rules' combat view check plays (tests/
// original_combat_view_tests.gd): a magic-user's Fire Shield coming due,
// its flames to choose, and a Delayed Blast Fireball to cast.
void write_view_fixture()
{
    using original::ClassSlot;
    auto fight = arrange(
        {{"Mage", rules::Side::party, {1, 3}, caster(ClassSlot::magic_user, 14, {185, 315})},
         {"Kobold", rules::Side::opposition, {7, 6}, victim(200)}},
        361);
    auto &session = *fight.session;
    until_turn_of(session, "Mage");
    check(session.submit(*cast_command(session, 185)), "Fire Shield is begun");
    play_until(
        session,
        [&](const std::string &)
        {
            return order(session, "end");
        },
        [&]
        {
            return offered(session, "flames").has_value();
        });
    check(offered(session, "flames").has_value(), "Its flames are asked for");
    const auto folder = std::filesystem::path(OPENGOLD_BINARY_DIR) / "original-combat-fixtures";
    std::filesystem::create_directories(folder);
    std::ofstream out(folder / "fire_shield.save", std::ios::binary);
    out << session.save();
    check(bool(out), "The view fixture is written");
}

// One of the kit's own effects (docs/games/ua.md, Monsters' effects), at its
// code past the kit's (kit_effect_offset).
original::Effect kit_effect(int code, int strength = 0)
{
    return {code + original::kit_effect_offset, 0, strength, false};
}

constexpr auto kit_game = GoldBoxGame::unlimited_adventures;

// The kit's defenses against blows (effects.c): half from a weapon below +3
// or +4 (80, 81); nothing from one below +2, nor from bare blows but a
// monster's of 7 Hit Dice (125); nothing from any blow (193).
void the_kits_defenses_weigh_the_blow()
{
    const auto hitter = [](std::optional<original::StrikingItem> striker, int race, int level)
    {
        auto c = monster({2, 1, 1, 9}, {0, 0, 0, 0}, {});
        c.striker = striker;
        c.race = race;
        c.level = level;
        return c;
    };
    const auto defended = [](int code)
    {
        auto c = creature(1000, 0);
        c.effects.push_back(kit_effect(code));
        return c;
    };
    const auto plus = [](int magic)
    {
        return original::StrikingItem{magic, false, 0};
    };
    check(first_hit(hitter(plus(2), 7, 1), defended(80), kit_game) == 5 &&
              first_hit(hitter(plus(3), 7, 1), defended(80), kit_game) == 10 &&
              first_hit(hitter(std::nullopt, 0, 1), defended(80), kit_game) == 10,
          "A rakshasa takes half from a weapon below +3, and claws whole");
    check(first_hit(hitter(plus(3), 7, 1), defended(81), kit_game) == 5 &&
              first_hit(hitter(plus(4), 7, 1), defended(81), kit_game) == 10,
          "A rakshasa rukh takes half from one below +4");
    check(first_hit(hitter(plus(1), 7, 1), defended(125), kit_game) == 0 &&
              first_hit(hitter(plus(2), 7, 1), defended(125), kit_game) == 10,
          "An elemental takes nothing from a weapon below +2");
    check(first_hit(hitter(std::nullopt, 0, 6), defended(125), kit_game) == 0 &&
              first_hit(hitter(std::nullopt, 0, 7), defended(125), kit_game) == 10 &&
              first_hit(hitter(std::nullopt, 7, 9), defended(125), kit_game) == 0,
          "Nor from bare blows but a monster's of 7 Hit Dice or more");
    check(first_hit(hitter(plus(5), 7, 1), defended(193), kit_game) == 0 &&
              first_hit(hitter(std::nullopt, 0, 20), defended(193), kit_game) == 0,
          "A black pudding takes nothing from any blow");
}

// The kit's defenses against spells (effects.c): a rakshasa shrugs off every
// spell below the eighth level (40); a mummy takes 1 more a die of magical
// fire (168), an efreeti 1 less, never under 1 a die (219); an otyugh takes
// no disease (76).
void the_kits_defenses_weigh_the_spell()
{
    using original::ClassSlot;
    auto mummy = victim(100), efreeti = victim(100), rakshasa = victim(100);
    mummy.effects.push_back(kit_effect(168));
    efreeti.effects.push_back(kit_effect(219));
    rakshasa.effects.push_back(kit_effect(40));
    auto fight =
        arrange({{"Mage", rules::Side::party, {0, 0}, caster(ClassSlot::magic_user, 5, {47})},
                 {"Plain", rules::Side::opposition, {6, 5}, victim(100)},
                 {"Mummy", rules::Side::opposition, {5, 5}, mummy},
                 {"Efreeti", rules::Side::opposition, {6, 4}, efreeti},
                 {"Rakshasa", rules::Side::opposition, {5, 4}, rakshasa}},
                105, {}, 100, kit_game);
    auto &session = *fight.session;
    cast_now(session, "Mage", 47,
             [&]
             {
                 return aim_at_cell(session, {6, 5});
             });
    const auto state = session.snapshot();
    const auto lost = [&](std::string_view name)
    {
        return 100 - view_of(state, name).hit_points;
    };
    check(lost("Plain") >= 5 && lost("Mummy") == lost("Plain") + 5,
          "The fireball's 5 dice burn a mummy 5 more");
    check(lost("Efreeti") == std::max(lost("Plain") - 5, 5),
          "And an efreeti 5 less, never under 1 a die");
    check(lost("Rakshasa") == 0, "A rakshasa takes no harm from a spell below the eighth level");

    auto held = arrange({{"Cleric", rules::Side::party, {0, 3}, caster(ClassSlot::cleric, 3, {23})},
                         {"Rakshasa", rules::Side::opposition, {4, 3}, rakshasa}},
                        101, {}, 100, kit_game);
    cast_now(*held.session, "Cleric", 23,
             [&]
             {
                 return aim_at(*held.session, "Rakshasa");
             });
    held.session->submit(order(*held.session, "aim_done"));
    check(view_of(held.session->snapshot(), "Rakshasa").status.empty(),
          "Nor is it held by Hold Person");

    auto otyugh = victim(100);
    otyugh.effects.push_back(kit_effect(76));
    auto healer = caster(ClassSlot::cleric, 5, {40, 40});
    healer.thac0 = -20;
    auto sick = arrange({{"Cleric", rules::Side::party, {3, 3}, healer},
                         {"Otyugh", rules::Side::opposition, {4, 3}, otyugh},
                         {"Plain", rules::Side::opposition, {3, 4}, victim(100)}},
                        107, {}, 100, kit_game);
    const auto diseased = [&](std::string_view name)
    {
        cast_now(*sick.session, "Cleric", 40,
                 [&]
                 {
                     return aim_at(*sick.session, name);
                 });
        const auto left = left_by_fight(*sick.session, name);
        return std::ranges::find(left.effects, 34, &original::Effect::code) != left.effects.end();
    };
    check(diseased("Plain") && !diseased("Otyugh"),
          "Cause Disease takes hold, but not on an otyugh");
}

// The kit's monsters at their turns (effects.c and fxcombat.c): a vampire
// meets a gaze unharmed (126); a ghast's stench leaves an enemy beside it
// that fails its save 2 worse at its attacks for the round (137, the kit's
// 148); a bulette strikes 4 times with its second attack, or 8 (99).
void the_kits_monsters_act_as_the_kit_has_them()
{
    const auto gaze_at = [](const original::Combatant &numbers)
    {
        auto fight =
            arrange({{"Fighter", rules::Side::party, {0, 3}, numbers},
                     {"Basilisk", rules::Side::opposition, {6, 3}, special({{458, 0, 0, false}})}},
                    249, {}, 100, kit_game);
        until_turn_of(*fight.session, "Basilisk");
        fight.session->submit(order(*fight.session, "quick"));
        return fight;
    };
    auto vampire = victim();
    vampire.effects.push_back(kit_effect(126));
    auto stoned = gaze_at(victim());
    auto spared = gaze_at(vampire);
    check(view_of(stoned.session->snapshot(), "Fighter").status == "stoned" &&
              logged(*spared.session, "{name} gazes.") == 1 &&
              view_of(spared.session->snapshot(), "Fighter").status.empty(),
          "A gaze that stones another leaves a vampire as it was");

    const int sick = kit_effect(148).code;
    auto stench = arrange({{"Fighter", rules::Side::party, {3, 3}, victim()},
                           {"Ghast", rules::Side::opposition, {4, 3}, special({kit_effect(137)})}},
                          251, {}, 100, kit_game);
    until_turn_of(*stench.session, "Ghast");
    const auto fighter = left_by_fight(*stench.session, "Fighter");
    const auto ghast = left_by_fight(*stench.session, "Ghast");
    check(logged(*stench.session, "{name} emits an evil stench.") == 1 &&
              logged(*stench.session, "{name} is sickened.") == 1 &&
              std::ranges::find(fighter.effects, sick, &original::Effect::code) !=
                  fighter.effects.end(),
          "The ghast's stench sickens the fighter beside it");
    check(std::ranges::find(ghast.effects, sick, &original::Effect::code) == ghast.effects.end(),
          "The ghast itself strikes as well as ever");

    auto flurry = arrange({{"Fighter", rules::Side::party, {3, 3}, victim()},
                           {"Bulette",
                            rules::Side::opposition,
                            {4, 3},
                            monster({0, 0, 0, 0}, {2, 1, 1, 0}, {kit_effect(99)})}},
                          253, {}, 100, kit_game);
    until_turn_of(*flurry.session, "Bulette");
    flurry.session->submit(order(*flurry.session, "melee", "Fighter"));
    const int attacks = logged(*flurry.session, "{attacker} hits {name} for {damage}.", "Bulette") +
                        logged(*flurry.session, "{attacker} misses {name}.", "Bulette");
    check(attacks == 4 || attacks == 8, "A bulette strikes 4 or 8 times with its second attack");
}

// The kit's breath (fxcombat.c and effects.c): a green dragon breathes poison
// three times a fight, its maximum Hit Points or half saved (90); a gorgon
// four times, turning to stone what fails its save (195).
void the_kits_monsters_breathe()
{
    const auto never = []
    {
        return false;
    };
    auto poison =
        arrange({{"Fighter", rules::Side::party, {3, 3}, victim(500)},
                 {"Dragon", rules::Side::opposition, {5, 3}, special({kit_effect(90, 255)}, 40)}},
                257, {}, 100, kit_game);
    ai_plays(*poison.session, "Dragon", never, 8);
    const int lost = 500 - view_of(poison.session->snapshot(), "Fighter").hit_points;
    check(logged(*poison.session, "{name} breathes poison.") == 3 && lost >= 60,
          "A green dragon breathes poison three times, 40 or 20 each, and no more");

    const auto gorgon = special({kit_effect(195, 255)}, 36);
    auto stone = arrange({{"Fighter", rules::Side::party, {1, 3}, victim(500)},
                          {"Gorgon", rules::Side::opposition, {5, 3}, gorgon}},
                         259, {}, 100, kit_game);
    until_turn_of(*stone.session, "Gorgon");
    stone.session->submit(order(*stone.session, "quick"));
    check(logged(*stone.session, "{name} breathes.") == 1 &&
              view_of(stone.session->snapshot(), "Fighter").status == "stoned",
          "A gorgon's breath turns the fighter to stone");
    auto sure = victim(500);
    sure.saving_throws = {2, 2, 2, 2, 2};
    auto breaths = arrange({{"Fighter", rules::Side::party, {1, 3}, sure},
                            {"Gorgon", rules::Side::opposition, {5, 3}, gorgon}},
                           259, {}, 100, kit_game);
    ai_plays(*breaths.session, "Gorgon", never, 10);
    check(logged(*breaths.session, "{name} breathes.") == 4, "Four breaths a fight, and no more");
}

// The kit's blue dragon breathes as Pools of Darkness's does (its 91, the
// fight's 561; fxcombat.c, fx_lightning_breath): three a fight, at an enemy
// its Lightning Bolt would be aimed at, its maximum Hit Points to the aim and
// along a bolt of 9 squares past it, never at one immune to electricity.
void the_kits_blue_dragon_breathes_lightning()
{
    const auto blue = original::canonical_effect(91, kit_game);
    check(blue == 561, "The kit's 91 is Pools of Darkness's blue dragon's breath");
    const auto dragon = special({{blue, 0, 255, false}}, 40);
    const auto wide = [](rules::Encounter &encounter)
    {
        encounter.battlefield = {16, 8, std::vector<rules::Terrain>(128, rules::Terrain::open)};
    };
    auto bolt = arrange({{"Fighter", rules::Side::party, {2, 3}, victim(500)},
                         {"Dragon", rules::Side::opposition, {0, 3}, dragon},
                         {"Ninth", rules::Side::opposition, {11, 3}, victim(500)},
                         {"Tenth", rules::Side::opposition, {12, 3}, victim(500)}},
                        263, {}, 100, kit_game, false, false, wide);
    until_turn_of(*bolt.session, "Dragon");
    bolt.session->submit(order(*bolt.session, "quick"));
    const auto hit_points = [&](std::string_view name)
    {
        return view_of(bolt.session->snapshot(), name).hit_points;
    };
    check(logged(*bolt.session, "{name} breathes.") == 1 && hit_points("Fighter") == 460 &&
              hit_points("Ninth") == 460 && hit_points("Tenth") == 500,
          "The blue dragon's bolt runs 9 squares past its aim, its own side's in it too");

    const auto never = []
    {
        return false;
    };
    auto breaths = arrange({{"Fighter", rules::Side::party, {2, 3}, victim(5000)},
                            {"Dragon", rules::Side::opposition, {5, 3}, dragon}},
                           265, {}, 100, kit_game);
    ai_plays(*breaths.session, "Dragon", never, 8);
    check(logged(*breaths.session, "{name} breathes.") == 3,
          "The kit's blue dragon breathes three times a fight, and no more");

    auto grounded = victim(5000);
    grounded.effects.push_back({335, 0, 0, false});
    auto spared = arrange({{"Fighter", rules::Side::party, {2, 3}, grounded},
                           {"Dragon", rules::Side::opposition, {5, 3}, dragon}},
                          265, {}, 100, kit_game);
    ai_plays(*spared.session, "Dragon", never, 3);
    check(logged(*spared.session, "{name} breathes.") == 0,
          "It never breathes at one immune to electricity");
}

// The kit's beholder's eyes (its 117; cbaction.c, eye_rays): up to four a
// turn at the enemy it locks onto, each once a turn, a ray within its
// distance: disintegration within 2 squares, stone within 3, death within 4,
// a wound within 5, then its spells.
void the_kits_beholder_fires_its_rays()
{
    const auto eyes = original::canonical_effect(117, kit_game);
    check(eyes == 717, "The kit's 117 is its own");
    const auto beholder = special({{eyes, 0, 255, false}}, 60);
    auto near = arrange({{"Fighter", rules::Side::party, {2, 3}, victim(500)},
                         {"Beholder", rules::Side::opposition, {0, 3}, beholder}},
                        265, {}, 100, kit_game);
    until_turn_of(*near.session, "Beholder");
    near.session->submit(order(*near.session, "quick"));
    check(logged(*near.session, "{name} fires a disintegrating ray.") == 1 &&
              view_of(near.session->snapshot(), "Fighter").status == "gone",
          "Within 2 squares its first ray disintegrates one that fails its save");

    auto sure = victim(500);
    sure.saving_throws = {2, 2, 2, 2, 2};
    auto far = arrange({{"Fighter", rules::Side::party, {3, 3}, sure},
                        {"Beholder", rules::Side::opposition, {0, 3}, beholder}},
                       269, {}, 100, kit_game);
    until_turn_of(*far.session, "Beholder");
    far.session->submit(order(*far.session, "quick"));
    const auto fired = [&](std::string_view ray)
    {
        return logged(*far.session, std::string("{name} fires a ") + std::string(ray) + " ray.");
    };
    check(fired("disintegrating") == 0 && fired("petrifying") == 1 && fired("death") == 1 &&
              fired("wounding") == 1 &&
              view_of(far.session->snapshot(), "Fighter").hit_points < 500,
          "At 3 squares it fires the stone, death and wounding rays, each once");
    until_turn_of(*far.session, "Beholder");
    far.session->submit(order(*far.session, "quick"));
    check(fired("petrifying") == 2 && fired("death") == 2, "Its rays fire again at its next turn");
}

// The kit's mummy rots its victim (its 170; effects.c, fx_disease_touch): no
// healing for 10 days (the fight's 50) and 1d6 months of rot, its strength
// the months by 32s, once; the months pass outside the fight.
void the_kits_mummy_rots()
{
    const auto touch = original::canonical_effect(170, kit_game);
    check(touch == 770, "The kit's 170 is its own");
    const auto mummy = monster({2, 1, 1, 0}, {0, 0, 0, 0}, {{touch, 0, 255, false}});
    const auto rotted = [](const rules::CombatSession &session)
    {
        return logged(session, "{name} is diseased.") > 0;
    };
    auto fight = attack_until(mummy, victim(), rotted, 47, kit_game);
    const auto left = left_by_fight(*fight.session, "Victim");
    const auto lasting = [&](int code)
    {
        return std::ranges::find(left.effects, code, &original::Effect::code);
    };
    const auto rot = lasting(original::kit_rotting);
    check(rotted(*fight.session) && lasting(50) != left.effects.end() &&
              lasting(50)->minutes == 14400 && rot != left.effects.end() && rot->minutes == 43200 &&
              rot->strength % 32 == 31 && rot->strength / 32 >= 1 && rot->strength / 32 <= 6,
          "The kit's mummy's touch rots for 1d6 months, without healing for 10 days");
    auto again = attack_until(
        mummy, victim(),
        [](const rules::CombatSession &session)
        {
            return session.snapshot().round > 8;
        },
        47, kit_game);
    check(logged(*again.session, "{name} is diseased.") == 1, "It rots its victim once");
}

// The kit's monster types (+BFh): a ranger beside a ranger's foe adds its
// level to the blow (105); a giant the kit does not call one takes it as it is.
// The kit's items' effects while readied (docs/games/ua.md, Items' special
// effects): a vorpal blade beheads with a blow of a natural 17 or more but a
// creature proof against it (115); a giant-slaying sword strikes its quarry
// 1 better for 6d6 (75); a ring of fire resistance takes 2 a die off magical
// fire, never under 1 a die (61); boots of speed double movement (74).
void the_kits_items_strike_as_the_kit_has_them()
{
    const auto wielder = [](int code)
    {
        return monster({2, 1, 1, 0}, {0, 0, 0, 0}, {kit_effect(code)});
    };
    const auto target_of = [](int traits)
    {
        auto c = creature(1000, 0);
        c.traits = traits;
        return c;
    };
    const auto beheading = [&](int traits)
    {
        return strike_until(
            wielder(115), target_of(traits),
            [](const rules::CombatSession &session)
            {
                return logged(session, "{name} is beheaded.") > 0;
            },
            53, kit_game);
    };
    const auto blade = beheading(0);
    check(logged(*blade.session, "{name} is beheaded.") == 1 &&
              logged(*blade.session, "{attacker} hits {name} for {damage}.", "Attacker") > 1,
          "A vorpal blade beheads with the kit's critical hit, not with every blow");
    const auto proof = beheading(original::trait::beheading_proof);
    check(logged(*proof.session, "{name} is beheaded.") == 0 &&
              logged(*proof.session, "{attacker} hits {name} for {damage}.", "Attacker") > 20,
          "Never one proof against it");
    const int bane = first_hit(wielder(75), target_of(original::trait::giant_bane), kit_game);
    check(bane >= 6 && bane <= 36 && first_hit(wielder(75), target_of(0), kit_game) == 1,
          "A giant-slaying sword strikes its quarry for 6d6, others as it would");

    using original::ClassSlot;
    auto ringed = victim(100);
    ringed.effects.push_back(kit_effect(61));
    auto burning =
        arrange({{"Mage", rules::Side::party, {0, 0}, caster(ClassSlot::magic_user, 5, {47})},
                 {"Plain", rules::Side::opposition, {6, 5}, victim(100)},
                 {"Ringed", rules::Side::opposition, {5, 5}, ringed}},
                105, {}, 100, kit_game);
    cast_now(*burning.session, "Mage", 47,
             [&]
             {
                 return aim_at_cell(*burning.session, {6, 5});
             });
    const auto state = burning.session->snapshot();
    const int plain = 100 - view_of(state, "Plain").hit_points;
    check(plain >= 5 && 100 - view_of(state, "Ringed").hit_points == std::max(plain - 10, 5),
          "A ring of fire resistance takes 2 a die off the fireball's 5");

    auto booted = creature(30, 0);
    booted.movement = 6;
    booted.effects.push_back(kit_effect(74));
    auto walker = creature(30, 0);
    walker.movement = 6;
    auto walking = arrange({{"Booted", rules::Side::party, {0, 0}, booted},
                            {"Walker", rules::Side::party, {0, 2}, walker},
                            {"Foe", rules::Side::opposition, {7, 7}, creature(30, 0)}},
                           61, {}, 100, kit_game);
    const auto feet = [&](std::string_view name)
    {
        until_turn_of(*walking.session, name);
        return view_of(walking.session->snapshot(), name).movement_feet;
    };
    const int booted_feet = feet("Booted");
    check(booted_feet == 2 * feet("Walker"), "Boots of speed double their wearer's movement");
}

// The kit's monsters where its effects differ from every game's
// (docs/games/ua.md, Monsters' effects): the need of a +1 or +3 weapon, as
// its +2 (103, 60); a ghoul's paralysing blow, never on an elf (224), and a
// second blow's beside it (88); 2d6 of fire with a second blow (108); and
// its terror, for those below 6 Hit Dice that fail a
// save against spells (100).
void dark_queen_elemental_and_weapon_defenses()
{
    constexpr auto game = GoldBoxGame::dark_queen_of_krynn;
    const auto native = [](int code)
    {
        return original::Effect{original::canonical_effect(code, game), 0, 255, false};
    };
    auto plain_blow = monster({2, 1, 1, 9}, {0, 0, 0, 0}, {});
    plain_blow.race = 7;
    plain_blow.striker = original::StrikingItem{0, false, 0};
    auto shadow = victim(1000);
    shadow.effects.push_back(native(197));
    auto magic_blow = plain_blow;
    magic_blow.striker->plus = 1;
    check(first_hit(plain_blow, shadow, game) == 0 && first_hit(magic_blow, shadow, game) == 10,
          "A native fireshadow rejects an adjacent ordinary weapon and takes a magical blow");

    auto native_dragon = kobold_record();
    native_dragon.game = game;
    native_dragon.race = 7;
    native_dragon.target_flags = 0x80;
    native_dragon.creature_type_flags = 0x08;
    const auto dragon_numbers = original::combatant_from_record(native_dragon);
    auto dragon_target = victim(1000);
    dragon_target.traits = dragon_numbers.traits;
    auto lance = plain_blow;
    lance.effects.push_back(native(200));
    check(first_hit(lance, dragon_target, game) == 1000 &&
              first_hit(lance, victim(1000), game) == 10,
          "The native dragonlance uses current wielder HP only against immunity-bit-80 dragons");
    auto wrong_dragon_bit = native_dragon;
    wrong_dragon_bit.target_flags = 1;
    auto giant_target = victim(1000);
    giant_target.traits = original::combatant_from_record(wrong_dragon_bit).traits;
    check(first_hit(lance, giant_target, game) == 10 &&
              (dragon_numbers.traits & original::trait::ranger_foe) != 0,
          "A native giant-quarry bit is not an earlier Krynn dragon bit");
    auto mace = plain_blow;
    mace.effects.push_back(native(147));
    auto corporeal = native_dragon;
    corporeal.creature_type_flags = 0x20;
    auto disruption_target = victim(1000);
    disruption_target.traits = original::combatant_from_record(corporeal).traits;
    check(first_hit(mace, disruption_target, game) == 20,
          "Native disruption doubles its authored type-20 quarry's weapon damage");
    const auto blast = [&](int spell, int code, std::uint64_t seed)
    {
        auto defended = victim(1000);
        defended.saving_throws.fill(100);
        defended.effects.push_back(native(code));
        auto plain = victim(1000);
        plain.saving_throws.fill(100);
        auto fight = arrange({{"Mage",
                               rules::Side::party,
                               {0, 3},
                               caster(original::ClassSlot::magic_user, 12, {spell})},
                              {"Plain", rules::Side::opposition, {4, 3}, plain},
                              {"Defended", rules::Side::opposition, {3, 3}, defended}},
                             seed, {}, 100, game);
        cast_now(*fight.session, "Mage", spell,
                 [&]
                 {
                     return aim_at_cell(*fight.session, {3, 3});
                 });
        const auto state = fight.session->snapshot();
        return std::pair{1000 - view_of(state, "Plain").hit_points,
                         1000 - view_of(state, "Defended").hit_points};
    };
    const auto [cold, hurt_more] = blast(192, 192, 105);
    check(cold > 0 && hurt_more == cold + cold / 5,
          "A native bakali takes one fifth more cold damage: " + std::to_string(cold) + ", " +
              std::to_string(hurt_more));
    const auto [fire, resisted] = blast(47, 205, 105);
    check(fire > 0 && resisted == fire / 5 * 4,
          "A native disir applies its shipped fire-damage rounding: " + std::to_string(fire) +
              ", " + std::to_string(resisted));
    const auto [lightning, growth] = blast(51, 173, 105);
    check(lightning > 0 && growth == -8,
          "Native shambling-mound lightning growth is exactly eight HP: " +
              std::to_string(lightning) + ", " + std::to_string(growth));
}

// Native abilities use their own effect IDs, saving throws and durations.
void dark_queen_rays_and_touches()
{
    constexpr auto game = GoldBoxGame::dark_queen_of_krynn;
    const auto native = [](int code, int strength = 255)
    {
        return original::Effect{original::canonical_effect(code, game), 0, strength, false};
    };
    auto never_saves = victim(1000);
    never_saves.saving_throws.fill(100);
    check(native(159).code == 109 &&
              original::canonical_effect(159, GoldBoxGame::unlimited_adventures) == 117,
          "Native paralysis immunity does not borrow the kit's holy-water effect");
    for (const int code : {155, 199, 211})
    {
        auto touch = monster({2, 1, 1, 0}, {0, 0, 0, 0}, {native(code)});
        auto fight = strike_until(
            touch, never_saves,
            [](const rules::CombatSession &session)
            {
                return view_of(session.snapshot(), "Target").status == "helpless";
            },
            53, game);
        const auto target = left_by_fight(*fight.session, "Target");
        const auto held = std::ranges::find(target.effects, 52, &original::Effect::code);
        check(held != target.effects.end(),
              "The native touch paralyzes its target: " + std::to_string(code));
        check(code == 211 ? held->minutes >= 3 && held->minutes <= 18 : held->minutes == 10,
              "The native paralysis duration is retained: " + std::to_string(code));
        auto protected_target = never_saves;
        protected_target.effects.push_back(native(159));
        auto protected_fight = strike_until(
            touch, protected_target,
            [](const rules::CombatSession &session)
            {
                return logged(session, "{attacker} hits {name} for {damage}.", "Attacker") >= 2;
            },
            53, game);
        const auto protected_left = left_by_fight(*protected_fight.session, "Target");
        check(logged(*protected_fight.session, "{attacker} hits {name} for {damage}.",
                     "Attacker") >= 2 &&
                  std::ranges::none_of(protected_left.effects,
                                       [](const auto &effect)
                                       {
                                           return effect.code == 52;
                                       }),
              "Native paralysis immunity protects even after failed saves against touch " +
                  std::to_string(code));
        auto ring_wearer = never_saves;
        ring_wearer.effects.push_back(native(150));
        auto natural_touch = strike_until(
            touch, ring_wearer,
            [](const rules::CombatSession &session)
            {
                return view_of(session.snapshot(), "Target").status == "helpless";
            },
            53, game);
        const auto ring_left = left_by_fight(*natural_touch.session, "Target");
        check(std::ranges::any_of(ring_left.effects,
                                  [](const auto &effect)
                                  {
                                      return effect.code == 52;
                                  }),
              "The native Free Action ring retains its shipped limit against natural touch " +
                  std::to_string(code));
    }
    auto venom = strike_until(
        monster({2, 1, 1, 0}, {0, 0, 0, 0}, {native(206)}), never_saves,
        [](const rules::CombatSession &session)
        {
            return logged(session, "{name} is crippled by venom.") > 0;
        },
        53, game);
    const auto envenomed = left_by_fight(*venom.session, "Target");
    const auto venom_left =
        std::ranges::find(envenomed.effects, native(207).code, &original::Effect::code);
    check(venom_left != envenomed.effects.end() && venom_left->minutes >= 1 &&
              venom_left->minutes <= 4,
          "Native venom lasts 1d4 minutes and survives combat handoff");
    auto squid = strike_until(
        monster({4, 1, 1, 0}, {0, 0, 0, 0}, {native(208)}), never_saves,
        [](const rules::CombatSession &session)
        {
            return logged(session, "{attacker} constricts {name}.") > 0;
        },
        53, game);
    const auto squeezed = left_by_fight(*squid.session, "Target");
    check(logged(*squid.session, "{attacker} constricts {name}.") == 1 &&
              view_of(squid.session->snapshot(), "Target").movement_feet == 0 &&
              std::ranges::find(squeezed.effects, 213, &original::Effect::code) !=
                  squeezed.effects.end(),
          "A native squid constricts and immobilizes after two hits");
    auto fighter_record = kobold_record();
    fighter_record.game = game;
    fighter_record.race = 6;
    fighter_record.base_thac0 = 0;
    fighter_record.base_armor_class = 10;
    fighter_record.max_hit_points = fighter_record.current.hit_points_raw = 100;
    fighter_record.item_limit = 255;
    fighter_record.base_attacks[0] = {2, {1, 1, 0}};
    fighter_record.current_abilities = fighter_record.abilities = {10, 10, 10, 3, 10, 10, 0};
    std::vector<std::uint8_t> raw_weapon(63);
    raw_weapon[46] = 18;
    raw_weapon[52] = raw_weapon[57] = 1;
    const auto weapon = opengold::por::decode_items(raw_weapon)->front();
    opengold::por::ItemTemplate weapon_type;
    weapon_type.worn_location = 0;
    weapon_type.hands = 1;
    weapon_type.class_restrictions = 255;
    weapon_type.small_medium_damage = weapon_type.large_damage = {1, 1, 2};
    const std::array<original::CarriedItem, 1> gear{{{weapon, weapon_type}}};
    auto baaz = never_saves;
    baaz.effects.push_back(native(187));
    std::vector<opengold::por::ItemTemplate> weapon_types(19);
    weapon_types[18] = weapon_type;
    auto disarm = arrange(
        {{"Attacker",
          rules::Side::party,
          {3, 3},
          original::combatant_from_record(fighter_record, gear)},
         {"Target", rules::Side::opposition, {4, 3}, baaz}},
        53, {}, 100, game, false, false,
        [&](rules::Encounter &encounter)
        {
            auto &hero = encounter.participants.front();
            hero.character_record = original::encode_fight_record(fighter_record);
            hero.inventory.push_back({7, opengold::por::original_item_key(weapon), 1, 0, "Weapon"});
        },
        weapon_types);
    play_until(
        *disarm.session,
        [&](const std::string &actor)
        {
            return actor == "Attacker" ? order(*disarm.session, "melee", "Target")
                                       : order(*disarm.session, "end");
        },
        [&]
        {
            return logged(*disarm.session, "{name} loses a weapon.") > 0;
        });
    check(logged(*disarm.session, "{name} loses a weapon.") == 1 &&
              view_of(disarm.session->snapshot(), "Target").hit_points > 0,
          "A living enchanted Baaz disarms an actual equipped weapon on a divisible-by-three blow");
    until_turn_of(*disarm.session, "Attacker");
    check(std::ranges::none_of(disarm.session->legal_commands(),
                               [](const rules::Command &command)
                               {
                                   return command.verb == "ready";
                               }),
          "The dropped weapon cannot be readied again during the fight");
    check(
        first_hit(monster({2, 1, 1, 9}, {0, 0, 0, 0}, {native(211)}), never_saves, game) == 10,
        "The anemone's shipped post-hit poison roll does not retroactively add another damage die");
    auto burning = strike_until(
        monster({2, 1, 1, 0}, {0, 0, 0, 0}, {native(201)}), never_saves,
        [](const rules::CombatSession &session)
        {
            return logged(session, "{name} is turning to green fire!") > 0;
        },
        53, game);
    const int burning_round = static_cast<int>(burning.session->snapshot().round);
    const int before_burning = view_of(burning.session->snapshot(), "Target").hit_points;
    play_until(
        *burning.session,
        [&](const std::string &)
        {
            return order(*burning.session, "end");
        },
        [&]
        {
            return static_cast<int>(burning.session->snapshot().round) > burning_round;
        });
    const int burned = before_burning - view_of(burning.session->snapshot(), "Target").hit_points;
    const auto lingering_fire = left_by_fight(*burning.session, "Target");
    check(burned >= 1 && burned <= 8 &&
              std::ranges::find(lingering_fire.effects, native(196).code,
                                &original::Effect::code) != lingering_fire.effects.end(),
          "A fireshadow touch burns for 1d8 at the next round and keeps burning");
    for (const int code : {182, 188, 189})
    {
        auto explosive = victim(1);
        explosive.effects.push_back(native(code));
        auto burst = strike_until(
            monster({2, 1, 1, 19}, {0, 0, 0, 0}, {}), explosive,
            [](const rules::CombatSession &session)
            {
                return logged(session, "{name} explodes.") > 0;
            },
            53, game);
        check(logged(*burst.session, "{name} explodes.") >= 1 &&
                  view_of(burst.session->snapshot(), "Attacker").hit_points < 1000,
              "The native death burst damages its nearby attacker: " + std::to_string(code));
    }
    auto spores = strike_until(
        monster({2, 1, 1, 0}, {0, 0, 0, 0}, {native(182), native(184)}), never_saves,
        [](const rules::CombatSession &session)
        {
            return logged(session, "{name} is diseased.") > 0;
        },
        53, game);
    const auto infected = left_by_fight(*spores.session, "Target");
    check(view_of(spores.session->snapshot(), "Attacker").dead &&
              logged(*spores.session, "{name} explodes.") == 0 &&
              std::ranges::find(infected.effects, native(185).code, &original::Effect::code) !=
                  infected.effects.end(),
          "A gas spore's touch infects, dies, and suppresses its ordinary burst");
    auto mummy = strike_until(
        monster({2, 1, 1, 0}, {0, 0, 0, 0}, {native(170)}), never_saves,
        [](const rules::CombatSession &session)
        {
            return logged(session, "{name} is diseased.") > 0;
        },
        53, game);
    const auto rotting = left_by_fight(*mummy.session, "Target");
    check(std::ranges::find(rotting.effects, native(215).code, &original::Effect::code) !=
                  rotting.effects.end() &&
              std::ranges::find(rotting.effects, original::kit_rotting, &original::Effect::code) !=
                  rotting.effects.end(),
          "A native mummy's touch carries its recovery timer and monthly rot");
    auto acid_target = never_saves;
    acid_target.effects.push_back(native(212));
    auto acid = strike_until(
        monster({2, 1, 1, 0}, {0, 0, 0, 0}, {}), acid_target,
        [](const rules::CombatSession &session)
        {
            return logged(session, "{name} is squirted by anemone's acid!") > 0;
        },
        53, game);
    check(logged(*acid.session, "{name} is squirted by anemone's acid!") == 1 &&
              view_of(acid.session->snapshot(), "Attacker").hit_points >= 988 &&
              view_of(acid.session->snapshot(), "Attacker").hit_points <= 997,
          "A struck anemone splashes acid on the adjacent attacker");

    auto steam = arrange({{"Fighter", rules::Side::party, {3, 3}, victim(1000)},
                          {"Dragon", rules::Side::opposition, {5, 3}, special({native(204)}, 40)}},
                         257, {}, 100, game);
    ai_plays(
        *steam.session, "Dragon",
        []
        {
            return false;
        },
        8);
    check(logged(*steam.session, "{name} breathes steam.") == 3,
          "A native sea dragon uses exactly three steam breaths");

    for (const int code : {198, 209, 202})
    {
        auto ray = arrange({{"Fighter", rules::Side::party, {1, 3}, never_saves},
                            {"Eye", rules::Side::opposition, {4, 3}, special({native(code)}, 100)}},
                           259, {}, 100, game);
        until_turn_of(*ray.session, "Eye");
        const auto before_ray_round = ray.session->snapshot().round;
        ray.session->submit(order(*ray.session, "quick"));
        const auto state = ray.session->snapshot();
        if (code == 198)
        {
            const int lost = 1000 - view_of(state, "Fighter").hit_points;
            check(logged(*ray.session, "{name} flashes a ray.") == 1 && lost >= 4 && lost <= 16,
                  "A native fireshadow ray inflicts 4d4 damage");
        }
        else
        {
            const auto target = left_by_fight(*ray.session, "Fighter");
            const auto held = std::ranges::find(target.effects, 52, &original::Effect::code);
            check(held != target.effects.end() &&
                      (code == 202
                           ? held->minutes == 10 - static_cast<int>(state.round - before_ray_round)
                           : held->minutes >= 2 && held->minutes <= 8),
                  "Native breath/ray paralysis retains its exact duration: " +
                      std::to_string(code));
        }
    }

    int protected_statues = 0, plain_statues = 0;
    for (std::uint64_t seed = 1; seed <= 40; ++seed)
    {
        for (const bool protected_target : {false, true})
        {
            auto target = victim(100);
            target.saving_throws.fill(20);
            if (protected_target)
            {
                target.effects.push_back(native(126));
            }
            auto gaze =
                arrange({{"Fighter", rules::Side::party, {0, 3}, target},
                         {"Basilisk", rules::Side::opposition, {6, 3}, special({native(58)})}},
                        seed, {}, 100, game);
            until_turn_of(*gaze.session, "Basilisk");
            gaze.session->submit(order(*gaze.session, "quick"));
            const bool statue = view_of(gaze.session->snapshot(), "Fighter").status == "stoned";
            (protected_target ? protected_statues : plain_statues) += statue;
        }
    }
    check(protected_statues > 0 && protected_statues < 10 && plain_statues > 25,
          "The native gaze defense grants a save bonus while natural ones still fail: " +
              std::to_string(protected_statues) + ", " + std::to_string(plain_statues));
}

void dark_queen_underwater_context_changes_combat()
{
    constexpr auto game = GoldBoxGame::dark_queen_of_krynn;
    auto native = [](int code)
    {
        return original::Effect{original::canonical_effect(code, game), 0, 255, false};
    };
    auto swimmer = victim(1000);
    swimmer.movement = 12;
    const auto movement = [&](bool wet, bool unrestricted)
    {
        auto numbers = swimmer;
        if (unrestricted)
        {
            numbers.effects.push_back(native(150));
        }
        auto fight = arrange({{"Swimmer", rules::Side::party, {0, 3}, numbers},
                              {"Guard", rules::Side::opposition, {7, 3}, victim(1000)}},
                             263, {}, 100, game, false, false,
                             [wet](rules::Encounter &encounter)
                             {
                                 encounter.underwater = wet;
                             });
        return view_of(fight.session->snapshot(), "Swimmer").movement_feet;
    };
    check(movement(false, false) == 60 && movement(true, false) == 20 && movement(true, true) == 60,
          "Native underwater movement is a third, with its authored unrestricted marker exempt");

    // These 8x8 fixtures have no plants or heights. Native adds its starting
    // time and underwater flag before the squares, so the costs follow the
    // first 72 space-separated values (the format name contains one space).
    const auto cost_offset = [](const std::string &checkpoint)
    {
        std::size_t at = 0;
        for (int value = 0; value < 72; ++value)
        {
            const auto next = checkpoint.find(' ', at);
            check(next != std::string::npos, "The Native fixture has its full board prefix");
            at = next + 1;
        }
        return at;
    };
    auto costly = arrange({{"Swimmer", rules::Side::party, {0, 3}, swimmer},
                           {"Guard", rules::Side::opposition, {7, 3}, victim(1000)}},
                          263, {}, 100, game, false, false,
                          [](rules::Encounter &encounter)
                          {
                              encounter.underwater = true;
                              encounter.time_minutes = 34567;
                              encounter.battlefield.move_cost.assign(64, 2);
                              encounter.battlefield.move_cost[3] = 4;
                          });
    const auto costly_checkpoint = costly.session->save();
    auto costly_restored = costly.module->restore(costly_checkpoint);
    check(costly_checkpoint.find("OGORIGINALCOMBAT 31") != std::string::npos &&
              costly_restored->snapshot().battlefield.move_cost ==
                  costly.session->snapshot().battlefield.move_cost &&
              costly_restored->save() == costly_checkpoint,
          "Native format 31 preserves its nonuniform movement costs and underwater context");
    const auto costs_at = cost_offset(costly_checkpoint);
    check(costly_checkpoint.compare(costs_at, 3, "64 ") == 0,
          "The Native fixture records all 64 square costs");
    checkpoint_square_cost_values(costly, costs_at);
    for (const auto count : {"63", "65"})
    {
        auto malformed = costly_checkpoint;
        malformed.replace(costs_at, 2, count);
        test::rejects(
            [&]
            {
                (void)costly.module->restore(malformed);
            },
            "A Native checkpoint refuses a cost count different from its board size");
    }
    auto zero_cost = costly_checkpoint;
    zero_cost.replace(costs_at + 3, 1, "0");
    test::rejects(
        [&]
        {
            (void)costly.module->restore(zero_cost);
        },
        "A Native checkpoint refuses an invalid zero square cost");

    auto fireproof = victim(1000);
    fireproof.effects.push_back(native(93));
    auto fight = arrange({{"Mage",
                           rules::Side::party,
                           {0, 3},
                           caster(original::ClassSlot::magic_user, 12, {47, 51, 187})},
                          {"Plain", rules::Side::opposition, {4, 3}, victim(1000)},
                          {"Fireproof", rules::Side::opposition, {5, 3}, fireproof},
                          {"Aside", rules::Side::opposition, {4, 6}, victim(1000)},
                          {"Far", rules::Side::opposition, {7, 7}, victim(1000)}},
                         263, {}, 100, game, false, false,
                         [](rules::Encounter &encounter)
                         {
                             encounter.underwater = true;
                             encounter.time_minutes = 34567;
                         });
    auto &session = *fight.session;
    until_turn_of(session, "Mage");
    check(
        !cast_command(session, 47) && cast_command(session, 51) && cast_command(session, 187),
        "Native underwater casting keeps Lightning Bolt and Ice Storm while suppressing Fireball");
    const auto checkpoint = session.save();
    auto restored = fight.module->restore(checkpoint);
    check(checkpoint.find("OGORIGINALCOMBAT 31") != std::string::npos &&
              restored->save() == checkpoint && !cast_command(*restored, 47),
          "Native format 31 roundtrips underwater combat and its starting calendar");
    // Released Native format 29 has both context fields, but no cost count.
    // It upgrades to 31 with an empty cost vector without losing the active turn.
    const auto costs_at_empty = cost_offset(checkpoint);
    check(checkpoint.compare(costs_at_empty, 2, "0 ") == 0,
          "The legacy Native fixture has no square costs");
    auto legacy = checkpoint;
    legacy.erase(costs_at_empty, 2);
    legacy.replace(legacy.find("OGORIGINALCOMBAT 31"), 19, "OGORIGINALCOMBAT 29");
    auto legacy_restored = fight.module->restore(legacy);
    check(
        legacy_restored->snapshot().battlefield.move_cost.empty() &&
            legacy_restored->save() == checkpoint && !cast_command(*legacy_restored, 47) &&
            cast_command(*legacy_restored, 51),
        "A released Native 29 checkpoint upgrades with its calendar, underwater rules and turn intact");
    auto unsupported = checkpoint;
    unsupported.replace(unsupported.find("OGORIGINALCOMBAT 31"), 19, "OGORIGINALCOMBAT 32");
    test::rejects(
        [&]
        {
            (void)fight.module->restore(unsupported);
        },
        "An unsupported Native combat format remains rejected");
    cast_now(*legacy_restored, "Mage", 51,
             [&]
             {
                 return aim_at_cell(*legacy_restored, {4, 3});
             });
    cast_now(session, "Mage", 51,
             [&]
             {
                 return aim_at_cell(session, {4, 3});
             });
    check(legacy_restored->save() == session.save(),
          "A released Native 29 fight continues the real spell with identical dice and effects");
    const auto lightning = session.snapshot();
    check(view_of(lightning, "Plain").hit_points < 1000 &&
              view_of(lightning, "Fireproof").hit_points == 1000 &&
              view_of(lightning, "Aside").hit_points == 1000 &&
              view_of(lightning, "Far").hit_points == 1000,
          "The shipped underwater bolt becomes a two-radius blast with the native fire element");
    const int plain_before = view_of(lightning, "Plain").hit_points;
    cast_now(session, "Mage", 187,
             [&]
             {
                 return aim_at_cell(session, {4, 3});
             });
    const auto ice = session.snapshot();
    const int ice_damage = 1000 - view_of(ice, "Fireproof").hit_points;
    check(ice_damage >= 1 && ice_damage <= 10 &&
              plain_before - view_of(ice, "Plain").hit_points == ice_damage &&
              1000 - view_of(ice, "Aside").hit_points == ice_damage &&
              view_of(ice, "Far").hit_points == 1000,
          "Native underwater Ice Storm inflicts one shared d10 across its three-square radius");
}

void the_kits_monsters_differ_as_the_kit_has_them()
{
    const auto hitter = [](std::optional<original::StrikingItem> striker, int level)
    {
        auto c = monster({2, 1, 1, 9}, {0, 0, 0, 0}, {});
        c.striker = striker;
        c.race = striker ? 7 : 0;
        c.level = level;
        return c;
    };
    const auto defended = [](int code)
    {
        auto c = creature(1000, 0);
        c.effects.push_back(kit_effect(code));
        return c;
    };
    const auto plus = [](int magic)
    {
        return original::StrikingItem{magic, false, 0};
    };
    check(first_hit(hitter(plus(0), 1), defended(103), kit_game) == 0 &&
              first_hit(hitter(plus(1), 1), defended(103), kit_game) == 10 &&
              first_hit(hitter(std::nullopt, 4), defended(103), kit_game) == 0 &&
              first_hit(hitter(std::nullopt, 5), defended(103), kit_game) == 10,
          "A +1 weapon or a monster of 5 Hit Dice harms what needs +1");
    check(first_hit(hitter(plus(2), 1), defended(60), kit_game) == 0 &&
              first_hit(hitter(plus(3), 1), defended(60), kit_game) == 10 &&
              first_hit(hitter(std::nullopt, 8), defended(60), kit_game) == 0 &&
              first_hit(hitter(std::nullopt, 9), defended(60), kit_game) == 10,
          "A +3 weapon or a monster of 9 Hit Dice harms what needs +3");

    const auto paralyzes = [](const original::Combatant &attacker, int race)
    {
        auto target = victim(500);
        target.race = race;
        auto fight = strike_until(
            attacker, target,
            [](const rules::CombatSession &session)
            {
                return logged(session, "{name} is paralyzed.") > 0;
            },
            53, kit_game);
        return logged(*fight.session, "{name} is paralyzed.") > 0;
    };
    const auto ghoul = monster({2, 1, 1, 0}, {0, 0, 0, 0}, {kit_effect(224)});
    check(paralyzes(ghoul, 7) && !paralyzes(ghoul, 2),
          "A ghoul's blow paralyzes, but never an elf");
    const auto holder = monster({0, 0, 0, 0}, {2, 1, 1, 0}, {kit_effect(88)});
    check(paralyzes(holder, 7), "A paralysing second blow holds one beside it");

    const auto burner = monster({0, 0, 0, 0}, {2, 1, 1, 0}, {kit_effect(108)});
    auto burnt = strike_until(
        burner, victim(500),
        [](const rules::CombatSession &session)
        {
            return logged(session, "{attacker} hits {name} for {damage}.", "Attacker") >= 4;
        },
        53, kit_game);
    const int blows = logged(*burnt.session, "{attacker} hits {name} for {damage}.", "Attacker");
    const int lost = 500 - view_of(burnt.session->snapshot(), "Target").hit_points;
    check(blows >= 4 && lost >= blows * 3, "A burning second blow burns for 2d6 more");

    auto squire = victim(50), knight = victim(50), lord = victim(50), brave = creature(50, 1);
    knight.level = 5;
    lord.level = 6;
    auto dragon = creature(60, 9);
    dragon.effects = {{482, 0, 0, false}};
    auto feared = arrange({{"Squire", rules::Side::party, {1, 3}, squire},
                           {"Knight", rules::Side::party, {1, 5}, knight},
                           {"Lord", rules::Side::party, {1, 1}, lord},
                           {"Brave", rules::Side::party, {1, 7}, brave},
                           {"Dragon", rules::Side::opposition, {6, 3}, dragon}},
                          263, {}, 100, kit_game);
    check(logged(*feared.session, "{name} is affected by {spell}.") == 2,
          "The kit's terror grips those below 6 Hit Dice that fail their save");

    // Its stoning gaze (58, the fight's 458) spares an iron golem (79, 479).
    const auto gazed_at = [](original::Combatant target)
    {
        auto fight = arrange({{"Fighter", rules::Side::party, {0, 3}, target},
                              {"Gorgon",
                               rules::Side::opposition,
                               {5, 3},
                               special({{original::canonical_effect(58, kit_game), 0, 0, false}})}},
                             197, {}, 100, kit_game);
        until_turn_of(*fight.session, "Gorgon");
        fight.session->submit(order(*fight.session, "quick"));
        return view_of(fight.session->snapshot(), "Fighter").status;
    };
    auto golem = victim();
    golem.effects.push_back({original::canonical_effect(79, kit_game), 0, 0, false});
    check(gazed_at(victim()) == "stoned" && gazed_at(golem).empty(),
          "The kit's stoning gaze spares an iron golem");
    // Its holy water is its item type 36h (159, the fight's 117).
    auto vial = hitter(std::nullopt, 1);
    vial.weapon_type = 0x36;
    auto undead = creature(1000, 0);
    undead.effects.push_back({original::canonical_effect(159, kit_game), 0, 0, false});
    const int holy = first_hit(vial, undead, kit_game);
    check(holy >= 2 && holy <= 7, "The kit's holy water burns the undead for 1d6+1");
}

void the_kits_rangers_know_their_foes()
{
    auto ranger = monster({2, 1, 1, 9}, {0, 0, 0, 0}, {{334, 0, 0, false}});
    ranger.race = 7;
    ranger.class_levels[static_cast<std::size_t>(original::ClassSlot::ranger)] = 5;
    auto kobold = creature(1000, 0);
    kobold.traits = original::trait::ranger_foe;
    check(first_hit(ranger, kobold, kit_game) == 15, "A ranger strikes a kobold 5 harder");
    auto giant = creature(1000, 0);
    giant.kin = original::kin::giant;
    check(first_hit(ranger, giant, kit_game) == 10,
          "A giant the kit does not call a ranger's foe takes the blow as it is");
}

} // namespace

int main()
{
    try
    {
        a_combatant_survives_its_encoding();
        a_melee_fight_resolves();
        a_saved_fight_goes_on_unchanged();
        readied_gear_counts();
        a_checkpoint_keeps_the_costs();
        the_original_movement_option();
        a_guard_strikes_who_comes_near();
        leaving_an_enemy_draws_a_free_attack();
        delay_acts_later_in_the_round();
        a_thief_backstabs_from_behind();
        a_fighter_sweeps_level_0_enemies();
        bandage_needs_a_neighbor();
        readied_missiles_count();
        missiles_shoot_what_they_see();
        shots_spend_ammunition();
        a_thrown_weapon_leaves_the_hand();
        morale_breaks();
        fleeing_off_the_field();
        a_panic_survives_a_save();
        effects_come_from_the_record();
        the_helpless_are_slain();
        poison_kills();
        a_ghoul_paralyses();
        a_stirge_drinks_its_fill();
        a_bite_holds();
        both_claws_rake();
        a_mummy_rots();
        a_wight_drains_levels();
        native_drains_keep_ordered_exact_receipts();
        strikers_come_from_the_gear();
        defenses_weigh_the_weapon();
        a_troll_rises_again();
        fighting_on_while_down();
        displacement_turns_the_first_attack();
        some_cannot_be_targeted();
        a_mummy_frightens();
        clerics_turn_undead();
        treasures_turns_its_own_way();
        bless_waits_for_its_turn();
        no_one_casts_in_a_no_magic_fight();
        darkness_no_magic_bars_use();
        damage_loses_a_spell();
        magic_missiles();
        curse_caster_levels();
        gateway_rows_change();
        strength_spells_strengthen();
        out_of_doors();
        a_quarrel_banishes_the_rakshasa();
        a_quarrel_of_slaying_slays_its_quarry();
        a_blessed_bolt_slays_the_rakshasa();
        readied_items_give_their_effects();
        later_games_effects_take_their_own_numbers();
        gateway_effects_play_their_own();
        each_game_bolts_its_own_length();
        a_fight_without_attacks_ends_after_15_rounds();
        each_game_storms_by_its_own_element();
        native_blasts_respect_defenses();
        native_untriggered_aurak_generator_stays_passive();
        native_enchanted_auraks_keep_their_native_damage();
        native_final_aurak_forms_follow_their_shapes();
        native_frenzy_burns_only_the_other_side_with_fire();
        native_free_action_counters_its_shipped_spells();
        native_stinking_cloud_uses_its_actual_dispatch_context();
        treasures_effects_play_their_own();
        treasures_crystal_battle_yields_one_crystal();
        a_stone_guardian_turns_thrown_weapons();
        yetis_frighten_when_they_surprise();
        a_stone_golem_slows_and_turns_weapons_aside();
        difficulty_changes_the_monsters();
        heights_decide_sight();
        the_party_facing_and_movement();
        spiritual_hammer_gives_its_hammer();
        a_giant_bane_weapon_bites_giants();
        a_blast_waits();
        write_view_fixture();
        sleep_takes_the_weak();
        charm_person_turns_a_foe();
        treasures_charm_person_frees_the_charmed();
        cure_light_wounds_stops_bleeding();
        races_know_their_foes();
        hold_person_holds();
        treasures_hold_person_holds_characters_only();
        treasures_spell_table();
        treasures_creatures_join_late();
        fireball_burns_all_around();
        haste_doubles_attacks();
        mirror_images_take_blows();
        silence_stops_casting();
        native_silence_can_be_aimed_at_empty_ground();
        missiles_turned_away();
        lightning_runs_on();
        treasures_bolt_counts_each_step_once();
        treasures_bolt_turns_only_indoors();
        a_lovers_kills_move_the_others_love();
        treasures_dispel_keeps_its_own();
        a_fallen_lover_frenzies_the_other();
        love_moves_attack_rolls();
        curse_spells_strike();
        curse_effects_take_their_own_numbers();
        curse_defenses_hold();
        curse_touches_and_engulfs();
        curse_monsters_breathe();
        curse_conditions_take_hold();
        a_cloud_sickens();
        cloudkill_poisons();
        secret_ai_shuns_clouds();
        fire_shield_flames_are_chosen();
        darkness_fire_shield_retaliates();
        dispel_magic_strips();
        the_dead_rise();
        slow_poison_raises();
        snakes_are_charmed();
        usable_items_come_from_the_gear();
        wands_and_potions();
        scrolls_are_read();
        the_ai_fights_to_the_end();
        a_lone_mage_stays();
        a_monster_casts();
        a_fireball_outlook_weighs_each_save();
        cone_and_bolt_outlooks_reach_friends();
        a_quick_mage_keeps_its_cone_off_a_friend();
        a_quick_mage_weighs_a_wand_at_the_wands_level();
        a_treasures_party_mage_keeps_its_bolt_off_a_friend();
        secret_dispel_outlook_weighs_each_creature();
        sleep_outlook_counts_a_friend_beside_a_foe();
        a_monster_bolts_through_its_own();
        treasures_ai_casts_its_weightiest_spell(GoldBoxGame::treasures_of_the_savage_frontier);
        treasures_ai_casts_its_weightiest_spell(GoldBoxGame::pools_of_darkness);
        treasures_ai_casts_its_weightiest_spell(GoldBoxGame::dark_queen_of_krynn);
        native_ai_weighs_its_spells();
        native_area_aim_takes_bursts_as_they_go_off();
        native_legacy_mind_targets_preserve_checkpoint_bytes();
        native_robed_mages_save_by_their_moon();
        native_save_bonuses_follow_their_trigger_group();
        quick_magic_lets_the_party_cast();
        the_ai_never_shares_a_square();
        the_ai_walks_around_walls();
        a_routed_monster_runs();
        the_ai_turns_undead();
        a_gaze_stones();
        a_dragon_breathes();
        an_ankheg_spits_once();
        real_records_fight();
        secret_effects_take_their_own_numbers();
        secret_records_fight();
        an_iron_golem_shrugs_off_harm();
        secret_touches();
        secret_monsters_breathe();
        secret_flags_decide();
        secret_rounds();
        secret_spells_take_their_own_numbers();
        secret_heal_and_harm();
        secret_spells_slay();
        secret_spells_stun_hide_and_burn();
        the_kits_defenses_weigh_the_blow();
        the_kits_defenses_weigh_the_spell();
        the_kits_monsters_act_as_the_kit_has_them();
        the_kits_monsters_breathe();
        the_kits_blue_dragon_breathes_lightning();
        the_kits_beholder_fires_its_rays();
        the_kits_mummy_rots();
        the_kits_rangers_know_their_foes();
        the_kits_items_strike_as_the_kit_has_them();
        the_kits_monsters_differ_as_the_kit_has_them();
        dark_queen_elemental_and_weapon_defenses();
        dark_queen_rays_and_touches();
        dark_queen_underwater_context_changes_combat();
        darkness_spells_fight();
        darkness_effects_take_their_own_numbers();
        darkness_records_fight_by_their_effects();
        darkness_monsters_fight_by_their_effects();
        darkness_kalistes_strikes_with_six_weapons();
        darkness_dragons_breathe_and_spiders_gaze();
        darkness_gazes_work_with_the_option_on();
        darkness_holds_no_monster();
        darkness_blades_and_the_bulette_strike();
        later_games_turn_undead_their_own_way(darkness_game);
        later_games_turn_undead_their_own_way(GoldBoxGame::dark_queen_of_krynn);
        dark_queen_turning_honors_the_native_deity();
        native_knights_can_lead_allied_creatures();
        darkness_ai_shuns_clouds();
        darkness_clouds_spare_the_poison_proof();
        darkness_beholder_fires_its_rays();
        darkness_fear_ends_with_the_fright();
        std::cout << "Original-rules combat passed.\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
