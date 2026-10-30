#include "opengold/campaign_party.h"
#include "opengold/character.h"
#include "opengold/character_creator.h"
#include "opengold/dice.h"
#include "opengold/srd5.h"
#include "status_effects.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <set>
#include <iterator>
#include <fstream>
#include <map>
#include <set>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace opengold;
using namespace opengold::rules;
namespace fx = opengold::srd5::detail;

// Species traits in play (SRD 5.2.1 pp. 83-86).
namespace
{
const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR);

void check(bool ok, const std::string &message)
{
    if (!ok)
    {
        throw std::runtime_error(message);
    }
}

std::unique_ptr<RulesModule> module()
{
    return srd5::load(root / "data/rules/srd-5.2.1/combat.rules");
}

std::string read_rules()
{
    std::ifstream in(root / "data/rules/srd-5.2.1/combat.rules");
    return {std::istreambuf_iterator<char>(in), {}};
}

bool submit(CombatSession &c, std::string_view verb, EntityId target = 0)
{
    for (const auto &command : c.legal_commands())
    {
        if (command.verb == verb && (!target || command.target == target))
        {
            return c.submit(command);
        }
    }
    return false;
}

// A level-one Fighter of the species, every roll the same, with the species'
// creation choices (group id to pick).
Character fighter(std::string race, std::map<std::string, std::string> species = {})
{
    CharacterDraft d;
    d.race = std::move(race);
    d.gender = "female";
    d.character_class = "fighter";
    d.background = "soldier";
    d.alignment = "neutral_good";
    d.name = "Target";
    d.rolled = true;
    for (auto &r : d.rolls)
    {
        r = {{6, 5, 4, 1}, 3};
    }
    d.training = {{"class:fighter", {"acrobatics", "history"}},
                  {"class:fighter:fighting_style", {"defense"}}};
    for (const auto &[group, pick] : species)
    {
        d.training[group] = {pick};
    }
    return Character(*srd5::character_rules(), d, {});
}

// An enemy Cleric who commands the target to halt; returns the natural roll
// of its Wisdom save.
int wisdom_save(const RulesModule &rules, const Character &target, std::uint64_t seed)
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = "cleric";
    d.background = "sage";
    d.alignment = "neutral_good";
    d.name = "Foe";
    d.rolled = true;
    for (auto &r : d.rolls)
    {
        r = {{6, 5, 4, 1}, 3};
    }
    d.cantrips = {"sacred_flame"};
    d.spells = SpellChoices{{}, std::vector<std::string>{"command"}, {}, {}};
    d.training = {{"class:cleric", {"medicine", "persuasion"}},
                  {"class:cleric:divine_order", {"protector"}}};
    const Character foe(*srd5::character_rules(), d, {});
    auto c = rules.create({{12, 6, std::vector<Terrain>(72)},
                           {{1,
                             "campaign-character",
                             "Target",
                             Side::party,
                             {1, 1},
                             rules.character_profile(target.sheet(), {}).data},
                            {98,
                             "campaign-character",
                             "Foe",
                             Side::opposition,
                             {6, 1},
                             rules.character_profile(foe.sheet(), {}).data}}},
                          seed);
    for (unsigned turns = 0; c->snapshot().actor != 98 && turns < 4; ++turns)
    {
        check(submit(*c, "end"), "Reach the Cleric's turn");
    }
    check(submit(*c, "command_halt", 1), "The Cleric commands the target to halt");
    for (const auto &line : c->snapshot().log())
    {
        if (const auto at = line.find("Target Wisdom save: d20 "); at != std::string::npos)
        {
            return std::stoi(line.substr(at + 24));
        }
    }
    throw std::runtime_error("No Wisdom save in the log");
}

// Gnomish Cunning: Advantage on Intelligence, Wisdom and Charisma saves. With
// the same rolls, Advantage keeps the first die or a higher second one.
void gnome_checks()
{
    const auto rules = module();
    const auto gnome = fighter("gnome"), human = fighter("human");
    bool higher = false;
    for (std::uint64_t seed = 1; seed <= 24; ++seed)
    {
        const int plain = wisdom_save(*rules, human, seed),
                  cunning = wisdom_save(*rules, gnome, seed);
        check(cunning >= plain, "Gnomish Cunning never lowers a Wisdom save");
        higher |= cunning > plain;
    }
    check(higher, "Gnomish Cunning gives Advantage on Wisdom saves");

    // The repeated saves at the end of a turn, such as against Hold Person.
    fx::EffectState effects;
    fx::EffectSubject subject{1, effects, {}, false, false, false, true};
    check(fx::mental_save_modifiers(fx::Ability::wisdom, subject).advantage &&
              fx::mental_save_modifiers(fx::Ability::charisma, subject).advantage &&
              !fx::mental_save_modifiers(fx::Ability::constitution, subject).advantage,
          "Repeated mental saves have Advantage too; others do not");
}

CombatantView unit(const CombatSession &c, EntityId id)
{
    for (const auto &a : c.snapshot().combatants)
    {
        if (a.id == id)
        {
            return a;
        }
    }
    throw std::runtime_error("Missing combatant");
}

bool logged(const CombatSession &c, std::string_view text)
{
    for (const auto &line : c.snapshot().log())
    {
        if (line.find(text) != std::string::npos)
        {
            return true;
        }
    }
    return false;
}

std::size_t count_logged(const CombatSession &c, std::string_view text)
{
    std::size_t count = 0;
    for (const auto &line : c.snapshot().log())
    {
        count += line.find(text) != std::string::npos;
    }
    return count;
}

// The bandit attacks the Orc until a blow lands; false if none does.
bool hit_orc(CombatSession &c)
{
    for (unsigned attempt = 0; attempt < 12; ++attempt)
    {
        const auto can_attack = [&]
        {
            const auto commands = c.legal_commands();
            return c.snapshot().actor == 98 && std::any_of(commands.begin(), commands.end(),
                                                           [](const auto &command)
                                                           {
                                                               return command.verb == "melee" &&
                                                                      command.target == 1;
                                                           });
        };
        for (unsigned turns = 0; !can_attack() && turns < 6; ++turns)
        {
            check(submit(c, "end"), "Reach the bandit's next attack");
        }
        const int before = unit(c, 1).hit_points;
        const auto endured = count_logged(c, "Relentless Endurance");
        if (!submit(c, "melee", 1))
        {
            throw std::runtime_error("The bandit cannot attack the Orc (HP " +
                                     std::to_string(before) + ", actor " +
                                     std::to_string(c.snapshot().actor) + ")");
        }
        if (unit(c, 1).hit_points < before || count_logged(c, "Relentless Endurance") > endured)
        {
            return true;
        }
        check(submit(c, "end"), "The bandit missed; end its turn");
    }
    return false;
}

// Relentless Endurance: an Orc dropped to 0 Hit Points drops to 1 instead,
// once per Long Rest.
void orc_checks()
{
    const auto rules = module();
    CampaignParty party(module());
    const auto id = party.add_pc(fighter("orc"));
    auto state = party.checkpoint();
    state.roster.front().vitals.hit_points = 1;
    party.restore(std::move(state));
    const auto encounter = [&]
    {
        Encounter e{{12, 6, std::vector<Terrain>(72)}, party.participants()};
        e.participants.front().cell = {1, 1};
        e.participants.push_back({98, "bandit", "Bandit", Side::opposition, {2, 1}});
        return e;
    };
    auto c = rules->create(encounter(), 3);
    check(hit_orc(*c) && unit(*c, 1).hit_points == 1 && logged(*c, "Relentless Endurance"),
          "The first blow to drop the Orc leaves it at 1 Hit Point");
    check(rules->restore(c->save())->save() == c->save(), "The spent trait survives a checkpoint");
    // The fight so far comes back to the party: the Orc stands at 1 Hit Point.
    party.begin_combat();
    party.apply_combat(c->snapshot());
    party.end_combat();
    check(hit_orc(*c) && unit(*c, 1).hit_points == 0, "The next blow drops it: once per Long Rest");

    // Before a Long Rest the trait stays spent; after one it is back.
    auto before_rest = rules->create(encounter(), 3);
    check(hit_orc(*before_rest) && unit(*before_rest, 1).hit_points == 0,
          "Without a Long Rest the trait does not return");
    party.advance_time(std::chrono::hours{24});
    check(bool(party.rest(RestKind::long_rest)), "The Orc takes a Long Rest");
    state = party.checkpoint();
    state.roster.front().vitals.hit_points = 1;
    party.restore(std::move(state));
    auto again = rules->create(encounter(), 3);
    check(hit_orc(*again) && unit(*again, 1).hit_points == 1,
          "After a Long Rest the Orc endures again");
    (void)id;
}

// Halfling Luck: a d20 that shows 1 is rolled again. Across many fights a
// Halfling's attack never stands on a natural 1, and the log says when Luck
// rerolled one; a Human with the same rolls does roll 1s.
void halfling_checks()
{
    // The roll itself.
    std::uint64_t seed = 0;
    for (;; ++seed)
    {
        auto probe = seed;
        if (opengold::roll_die(probe, 20) == 1)
        {
            break;
        }
    }
    auto plain = seed, lucky = seed;
    bool rerolled = false;
    check(fx::d20({}, plain) == 1 && fx::d20({false, false, true}, lucky, &rerolled) != 1 &&
              rerolled,
          "Luck rolls a 1 again");

    const auto rules = module();
    const auto attacks = [&](const Character &hero, std::uint64_t fight)
    {
        auto c = rules->create(
            {{12, 6, std::vector<Terrain>(72)},
             {{1,
               "campaign-character",
               "Hero",
               Side::party,
               {1, 1},
               rules->character_profile(hero.sheet(), std::vector<std::string>{"longsword"}).data},
              {98, "bandit", "Bandit", Side::opposition, {2, 1}}}},
            fight);
        for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 4; ++turns)
        {
            check(submit(*c, "end"), "Reach the hero's turn");
        }
        check(submit(*c, "melee", 98), "The hero attacks");
        return c;
    };
    bool human_one = false, luck = false;
    for (std::uint64_t fight = 1; fight <= 120; ++fight)
    {
        auto halfling = attacks(fighter("halfling"), fight);
        check(!logged(*halfling, "Hero -> Bandit: d20 1 +"), "A Halfling never keeps a natural 1");
        luck |= logged(*halfling, "Hero rolls a 1 and rerolls it with Luck.");
        human_one |= logged(*attacks(fighter("human"), fight), "Hero -> Bandit: d20 1 +");
    }
    check(luck && human_one, "Luck rerolled 1s that a Human kept");
}

// Halfling Nimbleness: a Halfling moves through a larger creature's space but
// cannot stop there. In a corridor one square wide a Medium bandit blocks a
// Human; a Halfling slips past it, and a Small kobold still blocks it.
void nimbleness_checks()
{
    const auto rules = module();
    std::vector<Terrain> corridor(36, Terrain::obstacle);
    for (int x = 0; x < 12; ++x)
    {
        corridor[12 + x] = Terrain::open;
    }
    const auto offers_move = [&](const Character &hero, const std::string &blocker, Cell to)
    {
        auto c = rules->create({{12, 3, corridor},
                                {{1,
                                  "campaign-character",
                                  "Hero",
                                  Side::party,
                                  {1, 1},
                                  rules->character_profile(hero.sheet(), {}).data},
                                 {98, blocker, "Blocker", Side::opposition, {2, 1}}}},
                               3);
        for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 4; ++turns)
        {
            check(submit(*c, "end"), "Reach the hero's turn");
        }
        const auto commands = c->legal_commands();
        return std::any_of(commands.begin(), commands.end(),
                           [&](const auto &command)
                           {
                               return command.verb == "move" && command.destination == to;
                           });
    };
    check(!offers_move(fighter("human"), "bandit", {3, 1}), "A bandit blocks a Human");
    check(offers_move(fighter("halfling"), "bandit", {3, 1}),
          "A Halfling moves through the bandit's space");
    check(!offers_move(fighter("halfling"), "bandit", {2, 1}),
          "A Halfling cannot stop in the bandit's space");
    check(!offers_move(fighter("halfling"), "slums-kobold", {3, 1}),
          "A Small kobold blocks a Halfling");
}

// A species' creation choices: what each species is offered, and the size,
// speed, skill and resistance they set (SRD 5.2.1 pp. 84-86).
void species_choice_checks()
{
    const auto rules = module();
    const auto groups = [](std::string race)
    {
        CharacterDraft d;
        d.race = std::move(race);
        std::vector<std::string> ids;
        for (const auto &g : srd5::character_rules()->training_options(d))
        {
            if (g.id.starts_with("species:"))
            {
                ids.push_back(g.id);
            }
        }
        return ids;
    };
    check(groups("elf") == std::vector<std::string>{"species:elf:lineage",
                                                    "species:elf:keen_senses",
                                                    "species:elf:ability"} &&
              groups("tiefling") == std::vector<std::string>{"species:tiefling:legacy",
                                                             "species:tiefling:size",
                                                             "species:tiefling:ability"} &&
              groups("dragonborn") == std::vector<std::string>{"species:dragonborn:ancestry"} &&
              groups("goliath") == std::vector<std::string>{"species:goliath:ancestry"} &&
              groups("human") == std::vector<std::string>{"species:human:size",
                                                          "species:human:skillful",
                                                          "species:human:versatile"} &&
              groups("dwarf").empty(),
          "Each species offers its own choices");
    const auto speed = [&](const Character &hero)
    {
        return rules->character_profile(hero.sheet(), {}).movement_feet;
    };
    const auto wood = fighter(
        "elf", {{"species:elf:lineage", "wood_elf"}, {"species:elf:keen_senses", "perception"}});
    check(speed(wood) == 35 && speed(fighter("elf")) == 30, "A Wood Elf moves 35 feet");
    const auto &skills = wood.sheet().training.skills;
    check(std::any_of(skills.begin(), skills.end(),
                      [](const auto &s)
                      {
                          return s.id == "perception" && s.proficient;
                      }),
          "Keen Senses trains Perception");

    // The same fire attack, with the same dice, against each hero: an Infernal
    // Tiefling and a Red Dragonborn take half what a Human takes.
    const auto burner = srd5::parse_content(
        read_rules() + "\ncreature burner 10 500 0 30 30 1 8 3 0 0 0 0 0 0 0 0 0 1 0\n"
                       "damage_types burner fire fire\n");
    const auto burned = [&](const Character &hero)
    {
        auto c = burner->create({{12, 6, std::vector<Terrain>(72)},
                                 {{1,
                                   "campaign-character",
                                   "Hero",
                                   Side::party,
                                   {1, 1},
                                   burner->character_profile(hero.sheet(), {}).data},
                                  {98, "burner", "Burner", Side::opposition, {2, 1}}}},
                                3);
        for (unsigned turns = 0; c->snapshot().actor != 98 && turns < 4; ++turns)
        {
            check(submit(*c, "end"), "Reach the burner's turn");
        }
        const int before = unit(*c, 1).hit_points;
        check(submit(*c, "melee", 1), "The burner attacks");
        return before - unit(*c, 1).hit_points;
    };
    const int human = burned(fighter("human"));
    check(human > 1, "The burner's fire lands on the Human");
    check(burned(fighter("tiefling", {{"species:tiefling:legacy", "infernal"}})) == human / 2 &&
              burned(fighter("dragonborn", {{"species:dragonborn:ancestry", "red"}})) ==
                  human / 2 &&
              burned(fighter("dragonborn", {{"species:dragonborn:ancestry", "white"}})) == human,
          "Infernal and Red Dragonborn heroes resist Fire; a White Dragonborn does not");
    check(rules->character_profile(fighter("human", {{"species:human:size", "small"}}).sheet(), {})
                  .data != rules->character_profile(fighter("human").sheet(), {}).data,
          "A Small Human differs from a Medium one");
}

// Lineage spells (SRD 5.2.1 pp. 84, 86): a Tiefling's cantrip and, from level
// 3, its spell with a free cast each Long Rest; a High Elf's chosen Wizard
// cantrip; both cast with the ability chosen for the lineage.
void lineage_spell_checks()
{
    const auto rules = module();
    const auto offered = [](const CombatSession &c, std::string_view verb)
    {
        const auto commands = c.legal_commands();
        return std::any_of(commands.begin(), commands.end(),
                           [&](const auto &command)
                           {
                               return command.verb == verb;
                           });
    };
    const auto fight = [&](const Character &hero)
    {
        auto c = rules->create(
            {{12, 6, std::vector<Terrain>(72)},
             {{1,
               "campaign-character",
               "Hero",
               Side::party,
               {1, 1},
               rules->character_profile(hero.sheet(), std::vector<std::string>{}).data},
              {98, "bandit", "Bandit", Side::opposition, {2, 1}}}},
            3);
        for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 4; ++turns)
        {
            check(submit(*c, "end"), "Reach the hero's turn");
        }
        return c;
    };
    const std::map<std::string, std::string> infernal{{"species:tiefling:legacy", "infernal"},
                                                      {"species:tiefling:ability", "charisma"}};
    auto c = fight(fighter("tiefling", infernal));
    check(offered(*c, "fire_bolt"), "An Infernal Tiefling Fighter knows Fire Bolt");
    check(submit(*c, "fire_bolt", 98) && logged(*c, "Hero -> Bandit: d20 ") &&
              logged(*c, " + 4 vs AC"),
          "Fire Bolt attacks with Charisma (+2) and proficiency (+2)");
    auto high = fight(fighter("elf", {{"species:elf:lineage", "high_elf"},
                                      {"species:elf:ability", "intelligence"},
                                      {"species:elf:cantrip", "ray_of_frost"}}));
    check(offered(*high, "ray_of_frost") && !offered(*fight(fighter("elf")), "ray_of_frost"),
          "A High Elf casts the Wizard cantrip it chose");

    // At level 3 the Tiefling gains Hellish Rebuke, cast once per Long Rest
    // without a slot: a Fighter has no slots.
    CampaignParty party(module());
    const auto id = party.add_pc(fighter("tiefling", infernal));
    party.award_experience(900, "lineage-xp");
    party.advance(id, party.default_advancement(id));
    party.advance(id, party.default_advancement(id));
    const auto &sheet = party.member(id).character.sheet();
    const auto pools = rules->recovery_info(sheet, party.member(id).vitals).resources;
    check(std::any_of(pools.begin(), pools.end(),
                      [](const auto &pool)
                      {
                          return pool.id == "magic_initiate:hellish_rebuke" &&
                                 pool.remaining == 1 && pool.label.source == "Lineage: {spell}";
                      }),
          "Level 3 brings Hellish Rebuke's free cast");
    auto rebuke =
        rules->create({{12, 6, std::vector<Terrain>(72)},
                       {{1,
                         "campaign-character",
                         "Hero",
                         Side::party,
                         {1, 1},
                         rules->character_profile(sheet, std::vector<std::string>{}).data},
                        {98, "bandit", "Bandit", Side::opposition, {2, 1}}}},
                      3);
    bool asked = false;
    for (unsigned attempt = 0; attempt < 12 && !asked; ++attempt)
    {
        for (unsigned turns = 0; rebuke->snapshot().actor != 98 && turns < 6; ++turns)
        {
            check(submit(*rebuke, "end"), "Reach the bandit's turn");
        }
        check(submit(*rebuke, "melee", 1), "The bandit attacks");
        asked = offered(*rebuke, "rebuke");
        if (!asked)
        {
            check(submit(*rebuke, "end"), "The bandit ends its turn");
        }
    }
    check(asked && submit(*rebuke, "rebuke") && logged(*rebuke, "casts Hellish Rebuke at Bandit"),
          "A hit asks about Hellish Rebuke, paid by the free cast");

    // Each lineage's later spells come with their free casts: a Tiefling's at
    // levels 3 and 5, a High Elf's Detect Magic and Misty Step, a Wood Elf's
    // Longstrider.
    const auto lineage_pools = [&](const std::map<std::string, std::string> &choices,
                                   std::string_view species, unsigned level)
    {
        CampaignParty grown(module());
        const auto member = grown.add_pc(fighter(std::string(species), choices));
        grown.award_experience(level >= 5 ? 6500 : 900, "lineage-xp");
        for (unsigned n = 1; n < level; ++n)
        {
            grown.advance(member, grown.default_advancement(member));
        }
        std::set<std::string> free;
        for (const auto &pool : rules->recovery_info(grown.member(member).character.sheet(),
                                                     grown.member(member).vitals)
                                    .resources)
        {
            if (pool.id.starts_with("magic_initiate:") && pool.remaining == 1)
            {
                free.insert(pool.id.substr(15));
            }
        }
        return free;
    };
    check(lineage_pools(infernal, "tiefling", 5) ==
              std::set<std::string>{"hellish_rebuke", "darkness"},
          "An Infernal Tiefling casts Darkness free from level 5");
    check(lineage_pools({{"species:tiefling:legacy", "abyssal"},
                         {"species:tiefling:ability", "charisma"}},
                        "tiefling", 5) == std::set<std::string>{"ray_of_sickness", "hold_person"},
          "An Abyssal Tiefling casts Hold Person free from level 5");
    check(lineage_pools({{"species:tiefling:legacy", "chthonic"},
                         {"species:tiefling:ability", "charisma"}},
                        "tiefling", 5) ==
              std::set<std::string>{"false_life", "ray_of_enfeeblement"},
          "A Chthonic Tiefling casts Ray of Enfeeblement free from level 5");
    const std::map<std::string, std::string> high_elf{{"species:elf:lineage", "high_elf"},
                                                      {"species:elf:ability", "intelligence"},
                                                      {"species:elf:cantrip", "ray_of_frost"}};
    check(lineage_pools(high_elf, "elf", 3) == std::set<std::string>{"detect_magic"} &&
              lineage_pools(high_elf, "elf", 5) ==
                  std::set<std::string>{"detect_magic", "misty_step"},
          "A High Elf casts Detect Magic free from level 3 and Misty Step from level 5");
    check(lineage_pools({{"species:elf:lineage", "wood_elf"}, {"species:elf:ability", "wisdom"}},
                        "elf", 3) == std::set<std::string>{"longstrider"},
          "A Wood Elf casts Longstrider free from level 3");
    auto chthonic = fight(fighter("tiefling", {{"species:tiefling:legacy", "chthonic"},
                                               {"species:tiefling:ability", "charisma"}}));
    check(offered(*chthonic, "chill_touch"), "A Chthonic Tiefling knows Chill Touch");
}

// Breath Weapon: a Red Dragonborn exhales a 5-foot-wide line of fire in place
// of its attack, DC 8 + Constitution + Proficiency Bonus, twice per Long Rest
// at level 1.
void breath_weapon_checks()
{
    // Targets with 1000 Hit Points that do not fight back.
    const auto rules = srd5::parse_content(
        read_rules() + "\ncreature target 1 1000 0 30 20 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n");
    const auto hero = fighter("dragonborn", {{"species:dragonborn:ancestry", "red"}});
    const auto pools = rules->recovery_info(hero.sheet(), {}).resources;
    check(std::any_of(pools.begin(), pools.end(),
                      [](const auto &pool)
                      {
                          return pool.id == "breath_weapon" && pool.remaining == 2 &&
                                 pool.capacity == 2 && !pool.short_rest_recovery;
                      }),
          "Breath Weapon has two uses, restored only by a Long Rest");
    auto c =
        rules->create({{12, 6, std::vector<Terrain>(72)},
                       {{1,
                         "campaign-character",
                         "Hero",
                         Side::party,
                         {1, 1},
                         rules->character_profile(hero.sheet(), std::vector<std::string>{}).data},
                        {98, "target", "Near", Side::opposition, {2, 1}},
                        {99, "target", "Aside", Side::opposition, {2, 0}}}},
                      3);
    const auto offered = [&](std::string_view verb)
    {
        const auto commands = c->legal_commands();
        return std::any_of(commands.begin(), commands.end(),
                           [&](const auto &command)
                           {
                               return command.verb == verb;
                           });
    };
    const auto aim = [&](Cell cell)
    {
        for (const auto &command : c->legal_commands())
        {
            if (command.verb == "area_move" && command.destination == cell)
            {
                return c->submit(command);
            }
        }
        return false;
    };
    const auto breathe = [&]
    {
        for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 4; ++turns)
        {
            check(submit(*c, "end"), "Reach the hero's turn");
        }
        return offered("breath_weapon_cone") && submit(*c, "breath_weapon_line") &&
               aim(Cell{7, 1}) && submit(*c, "area_cast");
    };
    check(breathe() && logged(*c, "Hero uses Breath Weapon.") &&
              logged(*c, "Near Dexterity save: d20 ") && logged(*c, " vs DC 12 (") &&
              logged(*c, " Fire damage.") && !logged(*c, "Aside Dexterity save"),
          "The line burns the creature in it and misses the one beside it");
    check(!offered("breath_weapon_line") && !offered("melee"),
          "The breath takes the place of the turn's attack");
    check(submit(*c, "end") && breathe(), "A second use on the next turn");
    check(submit(*c, "end"), "The hero ends the turn");
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 4; ++turns)
    {
        check(submit(*c, "end"), "Reach the hero's turn");
    }
    check(!offered("breath_weapon_cone") && offered("melee"), "Both uses are spent");
    VitalState state;
    for (const auto &unit : c->snapshot().combatants)
    {
        if (unit.id == 1)
        {
            state = unit.persistent;
        }
    }
    const auto uses_left = [&]
    {
        for (const auto &pool : rules->recovery_info(hero.sheet(), state).resources)
        {
            if (pool.id == "breath_weapon")
            {
                return pool.remaining;
            }
        }
        throw std::runtime_error("No Breath Weapon pool");
    };
    check(uses_left() == 0, "The spent uses outlast the fight");
    rules->recover(state, hero.sheet());
    check(uses_left() == 2, "A Long Rest restores both uses");

    // With Extra Attack the exhalation replaces only one of the attacks, and
    // only one per Attack action.
    CampaignParty party(module());
    const auto id = party.add_pc(fighter("dragonborn", {{"species:dragonborn:ancestry", "red"}}));
    party.award_experience(6500, "breath-xp");
    for (unsigned n = 1; n < 5; ++n)
    {
        party.advance(id, party.default_advancement(id));
    }
    c = rules->create({{12, 6, std::vector<Terrain>(72)},
                       {{1,
                         "campaign-character",
                         "Hero",
                         Side::party,
                         {1, 1},
                         rules
                             ->character_profile(party.member(id).character.sheet(),
                                                 std::vector<std::string>{})
                             .data},
                        {98, "target", "Near", Side::opposition, {2, 1}},
                        {99, "target", "Aside", Side::opposition, {2, 0}}}},
                      3);
    check(breathe() && offered("melee") && !offered("breath_weapon_line"),
          "Extra Attack's attack follows the exhalation, but no second breath");
    check(submit(*c, "melee", 98) && !offered("melee"), "The Attack action's two attacks are made");
    check(rules->restore(c->save())->save() == c->save(), "A checkpoint keeps the exhalation");
}

// Versatile: a Human's Origin feat, not the background's, with Skilled's
// skills and Magic Initiate's spells chosen like the background's (#71).
void versatile_checks()
{
    const auto creation = srd5::character_rules();
    const auto offered = [&](std::string background)
    {
        CharacterDraft d;
        d.race = "human";
        d.background = std::move(background);
        std::vector<std::string> ids;
        for (const auto &g : creation->training_options(d))
        {
            if (g.id == "species:human:versatile")
            {
                for (const auto &o : g.options)
                {
                    ids.push_back(o.id);
                }
            }
        }
        return ids;
    };
    const auto soldier = offered("soldier"), criminal = offered("criminal");
    check(soldier.front() == "skilled" &&
              std::find(soldier.begin(), soldier.end(), "savage_attacker") == soldier.end() &&
              std::find(soldier.begin(), soldier.end(), "alert") != soldier.end() &&
              std::find(criminal.begin(), criminal.end(), "alert") == criminal.end(),
          "Versatile offers the Origin feats the background does not give");

    const auto alert = fighter("human", {{"species:human:versatile", "alert"}});
    const auto has = [](const Character &hero, const FeatureGrant &grant)
    {
        const auto &grants = hero.sheet().grants;
        return std::find(grants.begin(), grants.end(), grant) != grants.end();
    };
    check(has(alert, {"feat:alert", "species:human:versatile", 1, {}}),
          "Versatile Alert is a feat grant from the species");
    const auto rules = module();
    auto c =
        rules->create({{12, 6, std::vector<Terrain>(72)},
                       {{1,
                         "campaign-character",
                         "Hero",
                         Side::party,
                         {1, 1},
                         rules->character_profile(alert.sheet(), std::vector<std::string>{}).data},
                        {2, "bandit", "Ally", Side::party, {1, 2}},
                        {98, "bandit", "Bandit", Side::opposition, {6, 1}}}},
                      3);
    check(c->snapshot().initiative_choices == std::vector<EntityId>{1},
          "Versatile Alert offers the Initiative swap");

    CharacterDraft skilled;
    skilled.race = "human";
    skilled.gender = "female";
    skilled.character_class = "fighter";
    skilled.background = "soldier";
    skilled.alignment = "neutral_good";
    skilled.name = "Skilled";
    skilled.rolled = true;
    for (auto &r : skilled.rolls)
    {
        r = {{6, 5, 4, 1}, 3};
    }
    skilled.training = {{"class:fighter", {"acrobatics", "history"}},
                        {"class:fighter:fighting_style", {"defense"}},
                        {"species:human:versatile", {"skilled"}},
                        {"species:human:skilled", {"arcana", "nature", "stealth"}}};
    const Character trained(*creation, skilled, {});
    check(has(trained, {"feat:skilled", "species:human:versatile", 1, {}}) &&
              has(trained, {"skill:stealth", "species:human:skilled", 1, {}}),
          "Versatile Skilled teaches its three skills");

    auto initiate = skilled;
    initiate.training.erase("species:human:skilled");
    initiate.training["species:human:versatile"] = {"magic_initiate_wizard"};
    const auto spells = creation->spell_choice_options(initiate);
    check(std::any_of(spells.learning.begin(), spells.learning.end(),
                      [](const auto &g)
                      {
                          return g.id == "feat:magic_initiate_wizard:spell";
                      }),
          "Versatile Magic Initiate asks for its spells");
    initiate.spells =
        SpellChoices{{{"feat:magic_initiate_wizard:ability", {"intelligence"}},
                      {"feat:magic_initiate_wizard:cantrips", {"fire_bolt", "ray_of_frost"}},
                      {"feat:magic_initiate_wizard:spell", {"magic_missile"}}},
                     {},
                     {},
                     {}};
    const Character wizardly(*creation, initiate, {});
    check(has(wizardly, {"feat:magic_initiate_wizard", "species:human:versatile", 1, {}}) &&
              std::any_of(wizardly.sheet().grants.begin(), wizardly.sheet().grants.end(),
                          [](const auto &g)
                          {
                              return g.id == "spell:magic_missile";
                          }),
          "Versatile Magic Initiate learns its spells");

    // A background that gives the chosen feat takes it back; the first
    // option it leaves fills the choice again.
    CharacterCreator creator(srd5::character_rules(), 7);
    creator.select(CreationField::race, "human");
    creator.training_choice({"species:human:versatile", "skilled", false});
    creator.training_choice({"species:human:versatile", "alert", true});
    creator.select(CreationField::background, "criminal");
    const auto &picks = creator.draft().training.at("species:human:versatile");
    check(picks == std::vector<std::string>{"skilled"} && creator.species_complete(),
          "The Criminal's Alert replaces Versatile's Alert with Skilled");
}

// Resourceful: a Human gains Heroic Inspiration on finishing a Long Rest (a
// new character has none) and spends it rolling its first miss again.
void heroic_inspiration_checks()
{
    const auto rules = srd5::parse_content(
        read_rules() + "\ncreature wall 30 1000 0 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n");
    const auto hero = fighter("human");
    const auto inspired = [&](const VitalState &state)
    {
        for (const auto &pool : rules->recovery_info(hero.sheet(), state).resources)
        {
            if (pool.id == "heroic_inspiration")
            {
                return pool.remaining;
            }
        }
        throw std::runtime_error("No Heroic Inspiration pool");
    };
    VitalState state;
    state.hit_points = hero.sheet().hit_points;
    check(inspired(state) == 0, "A new Human has no Heroic Inspiration yet");
    rules->recover(state, hero.sheet());
    check(inspired(state) == 1, "A Long Rest gives Heroic Inspiration");
    const VitalState rested = state;
    auto c =
        rules->create({{12, 6, std::vector<Terrain>(72)},
                       {{1,
                         "campaign-character",
                         "Hero",
                         Side::party,
                         {1, 1},
                         rules->character_profile(hero.sheet(), std::vector<std::string>{}).data,
                         rested},
                        {98, "wall", "Wall", Side::opposition, {2, 1}}}},
                      3);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 4; ++turns)
    {
        check(submit(*c, "end"), "Reach the hero's turn");
    }
    check(submit(*c, "melee", 98) &&
              logged(*c, "Hero uses Heroic Inspiration and rolls the d20 again: "),
          "The first miss is rolled again");
    VitalState after;
    for (const auto &unit : c->snapshot().combatants)
    {
        if (unit.id == 1)
        {
            after = unit.persistent;
        }
    }
    check(inspired(after) == 0, "Heroic Inspiration is spent");
}

// A Goliath of the ancestry beside an AC 1 creature with 1000 Hit Points
// that hits back with +20 ("Near") or casts Magic Missile, and a Huge one
// ("Giant") on its other side.
struct GoliathFight
{
    std::unique_ptr<RulesModule> rules = srd5::parse_content(
        read_rules() + "\ncreature target 1 1000 0 30 20 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n" +
        "creature huge 1 1000 0 30 20 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n" + "size huge huge\n" +
        "creature caster 1 1000 0 30 0 1 4 0 0 0 0 0 0 0 0 2 5 1 4\n");
    Character hero;
    std::unique_ptr<CombatSession> c;

    explicit GoliathFight(std::string boon, std::string near = "target", std::uint64_t seed = 3)
        : hero(fighter("goliath", {{"species:goliath:ancestry", std::move(boon)}}))
    {
        c = rules->create(
            {{12, 6, std::vector<Terrain>(72)},
             {{1,
               "campaign-character",
               "Hero",
               Side::party,
               {1, 1},
               rules->character_profile(hero.sheet(), std::vector<std::string>{}).data},
              {98, std::move(near), "Near", Side::opposition, {2, 1}},
              {99, "huge", "Giant", Side::opposition, {0, 1}}}},
            seed);
    }

    bool offered(std::string_view verb) const
    {
        const auto commands = c->legal_commands();
        return std::any_of(commands.begin(), commands.end(),
                           [&](const auto &command)
                           {
                               return command.verb == verb;
                           });
    }

    // Ends turns, the enemies' without attacking, until the actor's turn.
    void reach(EntityId actor)
    {
        for (unsigned turns = 0; c->snapshot().actor != actor && turns < 6; ++turns)
        {
            check(submit(*c, "end"), "Reach the next turn");
        }
        check(c->snapshot().actor == actor, "The actor's turn");
    }

    // The hero attacks the creature until a hit asks about the boon.
    void hit(EntityId target)
    {
        for (unsigned attempt = 0; attempt < 8; ++attempt)
        {
            reach(1);
            check(submit(*c, "melee", target), "The hero attacks");
            if (c->snapshot().reaction_pending)
            {
                return;
            }
            check(submit(*c, "end"), "The hero missed; end the turn");
        }
        throw std::runtime_error("The hero never hits");
    }

    // Near attacks the hero until a hit asks about the Reaction.
    void struck()
    {
        for (unsigned attempt = 0; attempt < 8; ++attempt)
        {
            reach(98);
            check(submit(*c, "melee", 1), "Near attacks the hero");
            if (c->snapshot().reaction_pending)
            {
                return;
            }
            check(submit(*c, "end"), "Near missed; end its turn");
        }
        throw std::runtime_error("Near never hits");
    }

    unsigned uses_left() const
    {
        for (const auto &unit : c->snapshot().combatants)
        {
            if (unit.id == 1)
            {
                for (const auto &pool :
                     rules->recovery_info(hero.sheet(), unit.persistent).resources)
                {
                    if (pool.id == "giant_ancestry")
                    {
                        return pool.remaining;
                    }
                }
            }
        }
        throw std::runtime_error("No Giant Ancestry pool");
    }
};

// Giant Ancestry: Fire's Burn, Frost's Chill and Hill's Tumble are offered
// after a damaging hit, Stone's Endurance and Storm's Thunder after an
// enemy's, Cloud's Jaunt as a Bonus Action; two uses per Long Rest.
void giant_ancestry_checks()
{
    GoliathFight fire("fire");
    const auto pools = fire.rules->recovery_info(fire.hero.sheet(), {}).resources;
    check(std::any_of(pools.begin(), pools.end(),
                      [](const auto &pool)
                      {
                          return pool.id == "giant_ancestry" &&
                                 pool.label.source == "Fire's Burn" && pool.remaining == 2 &&
                                 pool.capacity == 2 && !pool.short_rest_recovery;
                      }),
          "Giant Ancestry has two uses, restored only by a Long Rest");
    fire.hit(98);
    check(fire.offered("giant_boon") && fire.offered("decline"), "The hit asks about Fire's Burn");
    const auto saved = fire.c->save();
    check(fire.rules->restore(saved)->save() == saved, "A checkpoint keeps the question");
    check(submit(*fire.c, "giant_boon") && logged(*fire.c, "Hero uses Fire's Burn.") &&
              logged(*fire.c, " Fire damage.") && fire.uses_left() == 1,
          "Fire's Burn adds Fire damage and spends a use");
    check(submit(*fire.c, "end"), "The hero ends the turn");
    fire.hit(98);
    check(submit(*fire.c, "decline") && fire.uses_left() == 1, "Declining spends nothing");
    check(submit(*fire.c, "end"), "The hero ends the turn");
    fire.hit(98);
    check(submit(*fire.c, "giant_boon") && fire.uses_left() == 0, "The second use");
    const auto hero = fire.c->snapshot().combatants.front();
    check(std::any_of(hero.status_effects.begin(), hero.status_effects.end(),
                      [](const auto &effect)
                      {
                          return effect.name.source == "Species trait used" &&
                                 effect.description.arguments.size() == 1 &&
                                 effect.description.arguments[0].value == "2" &&
                                 effect.source.empty() && effect.end == EffectEnd::long_rest;
                      }),
          "The spent uses show until a Long Rest, with no source for the hero's own trait");
    check(submit(*fire.c, "end"), "The hero ends the turn");
    fire.reach(1);
    check(submit(*fire.c, "melee", 98) && !fire.c->snapshot().reaction_pending,
          "With no uses left a hit asks nothing");

    GoliathFight frost("frost");
    frost.hit(98);
    check(submit(*frost.c, "giant_boon") && logged(*frost.c, " Cold damage.") &&
              logged(*frost.c, "Near's Speed drops by 10 feet."),
          "Frost's Chill adds Cold damage and slows");
    const auto chilled = [&]
    {
        for (const auto &unit : frost.c->snapshot().combatants)
        {
            if (unit.id == 98)
            {
                return std::any_of(unit.conditions.begin(), unit.conditions.end(),
                                   [](const auto &m)
                                   {
                                       return m.source ==
                                              "Frost's Chill: Speed reduced by 10 feet.";
                                   });
            }
        }
        return false;
    };
    check(chilled(), "Near shows the chill");
    // The combat panel lists every effect with who caused it and its time left.
    const auto chill_view = frost.c->snapshot();
    const auto near = std::find_if(chill_view.combatants.begin(), chill_view.combatants.end(),
                                   [](const auto &unit)
                                   {
                                       return unit.id == 98;
                                   });
    check(std::any_of(near->status_effects.begin(), near->status_effects.end(),
                      [](const auto &effect)
                      {
                          return effect.name.source == "Frost's Chill" &&
                                 effect.description.source == "Speed reduced by 10 feet." &&
                                 effect.source == "Hero" && effect.end == EffectEnd::timed &&
                                 effect.remaining.count() > 0 && effect.remaining.count() <= 6000;
                      }),
          "Near's status effects name Frost's Chill, the hero, and under a round left");
    check(submit(*frost.c, "end") && chilled(), "The chill lasts through Near's turn");
    frost.reach(1);
    check(!chilled(), "It ends when the hero's turn starts");

    GoliathFight hill("hill");
    hill.hit(98);
    check(submit(*hill.c, "giant_boon") && logged(*hill.c, "Near is knocked Prone."),
          "Hill's Tumble knocks Near Prone");
    check(submit(*hill.c, "end"), "The hero ends the turn");
    hill.reach(1);
    check(submit(*hill.c, "melee", 99) && !logged(*hill.c, "Giant: d20 1 ") &&
              !hill.c->snapshot().reaction_pending,
          "A Huge creature hit cannot be tumbled");

    GoliathFight stone("stone");
    stone.struck();
    check(stone.offered("stones_endurance"), "Near's hit asks about Stone's Endurance");
    const int before = [&]
    {
        for (const auto &unit : stone.c->snapshot().combatants)
        {
            if (unit.id == 1)
            {
                return unit.hit_points;
            }
        }
        return 0;
    }();
    check(submit(*stone.c, "stones_endurance") &&
              logged(*stone.c, "Hero uses Stone's Endurance: ") && stone.uses_left() == 1,
          "Stone's Endurance lessens the damage");
    for (const auto &unit : stone.c->snapshot().combatants)
    {
        if (unit.id == 1)
        {
            check(unit.hit_points == before && !unit.reaction,
                  "1d4 is fully absorbed; the Reaction is spent");
        }
    }

    GoliathFight storm("storm");
    storm.struck();
    check(storm.offered("storms_thunder") && submit(*storm.c, "storms_thunder") &&
              logged(*storm.c, "Hero uses Storm's Thunder: Near takes ") && storm.uses_left() == 1,
          "Storm's Thunder strikes back at Near");

    // Damage without an attack roll is endured too (#164).
    GoliathFight darts("stone", "caster");
    darts.reach(98);
    for (unsigned dart = 0; dart < 3 && !darts.c->snapshot().reaction_pending; ++dart)
    {
        check(submit(*darts.c, "magic_missile", 1), "Near aims a dart at the hero");
    }
    check(darts.offered("stones_endurance") && submit(*darts.c, "stones_endurance") &&
              logged(*darts.c, "Hero uses Stone's Endurance: "),
          "Stone's Endurance lessens a Magic Missile dart");

    GoliathFight cloud("cloud");
    cloud.reach(1);
    check(submit(*cloud.c, "clouds_jaunt"), "Cloud's Jaunt is a Bonus Action");
    bool aimed = false;
    for (const auto &command : cloud.c->legal_commands())
    {
        if (command.verb == "area_move" && command.destination == Cell{7, 1})
        {
            aimed = cloud.c->submit(command);
        }
    }
    check(aimed && submit(*cloud.c, "area_cast") && logged(*cloud.c, "Hero uses Cloud's Jaunt."),
          "The Goliath teleports 30 feet");
    for (const auto &unit : cloud.c->snapshot().combatants)
    {
        if (unit.id == 1)
        {
            check(unit.cell == Cell{7, 1} && !unit.bonus_action && unit.action,
                  "It lands on the square, its Action left");
        }
    }
    check(cloud.uses_left() == 1 && !cloud.offered("clouds_jaunt"),
          "A use and the Bonus Action are spent");
}
} // namespace

int main()
{
    try
    {
        gnome_checks();
        orc_checks();
        halfling_checks();
        nimbleness_checks();
        species_choice_checks();
        lineage_spell_checks();
        breath_weapon_checks();
        versatile_checks();
        heroic_inspiration_checks();
        giant_ancestry_checks();
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Species tests passed\n";
}
