// Fights under the original rules (docs/ORIGINAL-RULES.md, plan step 4).
#include "opengold/creature_catalog.h"
#include "opengold/original.h"
#include "opengold/original_combat.h"
#include "opengold/original_content.h"
#include "opengold/original_rules.h"
#include "opengold/original_spells.h"
#include "test_support.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
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

// Pool of Radiance's own records, when its files are here (OPENGOLD_GAME_DIR):
// every creature fights with sensible numbers, and kobolds of the Slums fight
// a party to the end.
void real_records_fight()
{
    const char *game = std::getenv("OPENGOLD_GAME_DIR");
    if (!game)
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
              GoldBoxGame game = GoldBoxGame::pool_of_radiance)
{
    original::Content content;
    content.game = game;
    Fight fight{original::rules_module(std::move(content)), nullptr};
    rules::Encounter encounter;
    encounter.morale = morale;
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
                   std::uint64_t seed = 53)
{
    auto ogre = creature(1000, 0);
    ogre.movement = 0;
    auto fight = arrange({{"Attacker", rules::Side::party, {3, 3}, attacker},
                          {"Target", rules::Side::opposition, {4, 3}, target},
                          {"Ogre", rules::Side::opposition, {7, 7}, ogre}},
                         seed);
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
int first_hit(const original::Combatant &attacker, const original::Combatant &target)
{
    auto fight = strike_until(attacker, target,
                              [](const rules::CombatSession &session)
                              {
                                  return logged(session, "{attacker} hits {name} for {damage}.",
                                                "Attacker") > 0;
                              });
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

// Secret's creatures as its files make them (OPENGOLD_GOLDBOX_DIR): their
// type and target flags, magic resistance and races, and a fight with them
// that ends.
void secret_records_fight()
{
    const char *games = std::getenv("OPENGOLD_GOLDBOX_DIR");
    if (!games)
    {
        std::cout << "Secret of the Silver Blades' files are not here; its records are not "
                     "checked.\n";
        return;
    }
    auto content = load_original_content(std::filesystem::path(games) / "SECRET", secret_game);
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

} // namespace

int main()
{
    try
    {
        a_combatant_survives_its_encoding();
        a_melee_fight_resolves();
        a_saved_fight_goes_on_unchanged();
        readied_gear_counts();
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
        strikers_come_from_the_gear();
        defenses_weigh_the_weapon();
        a_troll_rises_again();
        fighting_on_while_down();
        displacement_turns_the_first_attack();
        some_cannot_be_targeted();
        a_mummy_frightens();
        clerics_turn_undead();
        bless_waits_for_its_turn();
        damage_loses_a_spell();
        magic_missiles();
        curse_caster_levels();
        gateway_rows_change();
        sleep_takes_the_weak();
        charm_person_turns_a_foe();
        cure_light_wounds_stops_bleeding();
        races_know_their_foes();
        hold_person_holds();
        fireball_burns_all_around();
        haste_doubles_attacks();
        mirror_images_take_blows();
        silence_stops_casting();
        missiles_turned_away();
        lightning_runs_on();
        curse_spells_strike();
        curse_effects_take_their_own_numbers();
        curse_defenses_hold();
        curse_touches_and_engulfs();
        curse_monsters_breathe();
        curse_conditions_take_hold();
        a_cloud_sickens();
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
        std::cout << "Original-rules combat passed.\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
