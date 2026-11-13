// Fights under the original rules (docs/ORIGINAL-RULES.md, plan step 4).
#include "opengold/creature_catalog.h"
#include "opengold/original.h"
#include "opengold/original_combat.h"
#include "opengold/original_rules.h"
#include "test_support.h"

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
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
    numbers.thac0 = original::thac0(numbers.class_levels);
    numbers.saving_throws = original::saving_throws(numbers.class_levels);
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
    original::Content content;
    for (const auto &[id, creature] : catalog.all())
    {
        std::vector<original::CarriedItem> carried;
        for (const auto &item : creature.equipment)
        {
            carried.push_back({item.stored, item.base});
        }
        std::vector<por::EffectRecord> effects;
        for (const auto &effect : creature.effects)
        {
            effects.push_back(effect.stored);
        }
        const auto numbers = original::combatant_from_record(creature.stored, carried, effects);
        check(original::Combatant::decode(numbers.encode()) == numbers,
              id.key() + " survives its profile's encoding");
        check(numbers.thac0 >= -20 && numbers.thac0 <= 25 && numbers.armor_class >= -20 &&
                  numbers.armor_class <= 20 && numbers.hit_points > 0 && numbers.movement >= 0,
              id.key() + " " + creature.stored.name + " fights with sensible numbers");
        content.bestiary.emplace(id.key(), numbers);
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
    auto session = module->create(std::move(encounter), 11);
    play(*session);
    check(session->snapshot().outcome != rules::Outcome::ongoing,
          "A fight with the Slums' kobolds ends");
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
              const std::vector<rules::Cell> &walls = {}, unsigned morale = 100)
{
    Fight fight{original::rules_module(), nullptr};
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
    arrow_type.worn_location = 11;
    por::ItemRecord arrows;
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
                   std::uint64_t seed = 47)
{
    auto fight = arrange({{"Victim", rules::Side::party, {3, 3}, target},
                          {"Monster", rules::Side::opposition, {4, 3}, attacker}},
                         seed);
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
        c.thac0 = original::thac0(levels);
        c.saving_throws = original::saving_throws(levels);
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
              after.thac0 == original::thac0(fighter_2) &&
              after.saving_throws == original::saving_throws(fighter_2),
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
    const bool thief_cost_more = original::experience_for_level(ClassSlot::thief, 3).value_or(0) >=
                                 original::experience_for_level(ClassSlot::fighter, 2).value_or(0);
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
    arrow_type.worn_location = 11;
    arrow_type.damage_type = 0x01;
    por::ItemRecord arrows;
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
        a_cloud_sickens();
        dispel_magic_strips();
        the_dead_rise();
        slow_poison_raises();
        snakes_are_charmed();
        usable_items_come_from_the_gear();
        wands_and_potions();
        scrolls_are_read();
        real_records_fight();
        std::cout << "Original-rules combat passed.\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
