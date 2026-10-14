#include "opengold/campaign_party.h"
#include "opengold/campaign_save.h"
#include "opengold/character.h"
#include "opengold/combat_demo.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

using namespace opengold;
using namespace opengold::rules;

// A Fighter (Champion) from level 4 to 10 (LEVELS-1): the Proficiency Bonus,
// feats at 6 and 8, Second Wind, the level-10 Weapon Mastery, the Champion's
// second Fighting Style and the class features, through ordinary level-ups.
namespace
{
const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR);

void check(bool ok, const std::string &message)
{
    if (!ok)
        throw std::runtime_error(message);
}

std::unique_ptr<RulesModule> module()
{
    return srd5::load(root / "data/rules/srd-5.2.1/combat.rules");
}

CharacterDraft draft(std::string klass)
{
    CharacterDraft d;
    d.race = "dwarf";
    d.gender = "female";
    d.character_class = std::move(klass);
    d.background = "soldier";
    d.alignment = "neutral_good";
    d.name = "Veteran";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    d.rolls[0] = {{6, 6, 6, 1}, 3};
    const auto rules = srd5::character_rules();
    for (bool filled = true; filled;)
    {
        filled = false;
        for (const auto &group : rules->training_options(d))
            if (!d.training.contains(group.id))
            {
                for (unsigned i = 0; i < group.count; ++i)
                    d.training[group.id].push_back(group.options.at(i).id);
                filled = true;
                break;
            }
    }
    return d;
}

bool holds(const CharacterSheet &sheet, std::string_view id, unsigned level)
{
    return std::any_of(sheet.grants.begin(), sheet.grants.end(), [&](const auto & g)
    {
        return g.id == id && g.level == level;
    });
}

unsigned count_of(const CharacterSheet &sheet, std::string_view prefix)
{
    return static_cast<unsigned>(std::count_if(sheet.grants.begin(), sheet.grants.end(),
                                 [&](const auto & g)
    {
        return g.id.starts_with(prefix);
    }));
}

unsigned capacity(const CampaignParty &party, MemberId id, std::string_view pool)
{
    for (const auto &resource : party.recovery_info(id).resources)
        if (resource.id == pool)
            return resource.capacity;
    return 0;
}

// A Fighter of the level, advanced through the rules' ordinary level-ups
// (the campaign stops at the Training Hall's limit; CAP-1).
Character fighter(unsigned level)
{
    Character hero(*srd5::character_rules(), draft("fighter"), {});
    VitalState life;
    const auto rules = module();
    while (hero.sheet().level < static_cast<int>(level))
        check(hero.advance(*rules, life), "An ordinary level-up");
    return hero;
}

void fighter_checks()
{
    {
        // The campaign: Phlan's hall trains Fighters to level 8.
        CampaignParty party(module());
        const auto id = party.add_pc(Character(*srd5::character_rules(), draft("fighter"), {}));
        party.award_experience(64000, "levels-xp");
        while (party.can_advance(id))
            party.advance(id, party.default_advancement(id));
        check(party.member(id).character.sheet().level == 8,
              "In the campaign a Fighter stops at the hall's level 8");
    }
    CampaignParty party(module());
    const auto id = party.add_pc(fighter(10));
    const auto &sheet = [&]() -> const CharacterSheet &
    {
        return party.member(id).character.sheet();
    };
    check(sheet().level == 10, "The rules advance a Fighter to level 10");
    check(!party.rule_module().advancement_options(sheet()).level,
          "Level 10 is the Fighter's rules cap for now");
    check(holds(sheet(), "feature:extra_attack", 5) && holds(sheet(), "feature:tactical_shift", 5) &&
          holds(sheet(), "feature:additional_fighting_style", 7) &&
          holds(sheet(), "feature:indomitable", 9) && holds(sheet(), "feature:tactical_master", 9) &&
          holds(sheet(), "feature:heroic_warrior", 10),
          "The class and Champion features arrive at their levels");
    check(std::count_if(sheet().grants.begin(), sheet().grants.end(), [](const auto & g)
    {
        return g.source_id == "class:fighter:ability_score_improvement";
    }) == 3, "Feats or ability points at levels 4, 6 and 8");
    check(sheet().ability_adjustments.size() == 4,
          "Each Ability Score Improvement records its own adjustment");
    check(std::count_if(sheet().grants.begin(), sheet().grants.end(), [](const auto & g)
    {
        return g.source_id == "subclass:fighter:champion:fighting_style" && g.level == 7;
    }) == 1, "The Champion's second Fighting Style feat");
    check(count_of(sheet(), "mastery:") == 5, "Five Weapon Mastery kinds at level 10");
    check(capacity(party, id, "second_wind") == 4, "Second Wind has four uses at level 10");
    const auto profile = party.rule_module().character_profile(sheet(), {});
    const auto modifier = (sheet().scores[Ability::strength] - 10) / 2;
    check(profile.melee_attack_bonus == modifier + 4,
          "Unarmed attacks add the level-10 Proficiency Bonus of +4");
    check(sheet().saving_throws[Ability::strength] == modifier + 4,
          "A proficient save adds +4");

    const auto bytes = encode_campaign(party, nullptr, "fighter-levels");
    CampaignParty next(module());
    next.restore(decode_campaign(bytes, *srd5::character_rules(), party.rule_module(),
                                 "fighter-levels", nullptr).party);
    check(encode_campaign(next, nullptr, "fighter-levels") == bytes,
          "A level-10 Fighter saves and reloads exactly");
}

bool offered(const CombatSession &c, std::string_view verb)
{
    const auto commands = c.legal_commands();
    return std::any_of(commands.begin(), commands.end(), [&](const auto & command)
    {
        return command.verb == verb;
    });
}

bool submit(CombatSession &c, std::string_view verb, EntityId target = 0)
{
    for (const auto &command : c.legal_commands())
        if (command.verb == verb && (!target || command.target == target))
            return c.submit(command);
    return false;
}

std::size_t attack_lines(const CombatSession &c)
{
    const auto log = c.snapshot().log();
    return static_cast<std::size_t>(std::count_if(log.begin(), log.end(), [](const auto & line)
    {
        return line.starts_with("Veteran -> Dummy: d20");
    }));
}

// A fight between the Fighter of the level, with the gear, and a dummy beside
// it, reaching the Fighter's turn.
std::unique_ptr<CombatSession> duel(const RulesModule &rules, const Character &hero,
                                    std::vector<std::string> gear = {},
                                    std::optional<VitalState> state = std::nullopt)
{
    const auto profile = rules.character_profile(hero.sheet(), gear).data;
    auto c = rules.create({{8, 3, std::vector<Terrain>(24)},
        {   {1, "campaign-character", "Veteran", Side::party, {1, 1}, profile, state},
            {98, "dummy", "Dummy", Side::opposition, {2, 1}}
        }},
    3);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 4; ++turns)
        check(submit(*c, "initiative_keep") || submit(*c, "end"), "Reach the Fighter's turn");
    return c;
}

std::unique_ptr<RulesModule> dummy_rules()
{
    std::ifstream in(root / "data/rules/srd-5.2.1/combat.rules");
    return srd5::parse_content(std::string{std::istreambuf_iterator<char>(in), {}} +
                               "\ncreature dummy 10 1000 -10 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n");
}

bool logged(const CombatSession &c, std::string_view text)
{
    const auto log = c.snapshot().log();
    return std::any_of(log.begin(), log.end(), [&](const auto & line)
    {
        return line.find(text) != std::string::npos;
    });
}

// Extra Attack: the Attack action makes two attacks; Action Surge's action
// makes two more; a checkpoint between them keeps the one left.
void extra_attack_checks()
{
    const auto rules = dummy_rules();
    for (const unsigned level : {4u, 5u})
    {
        const auto hero = fighter(level);
        const auto profile = rules->character_profile(hero.sheet(), {}).data;
        auto c = rules->create({{8, 3, std::vector<Terrain>(24)},
            {   {1, "campaign-character", "Veteran", Side::party, {1, 1}, profile},
                {98, "dummy", "Dummy", Side::opposition, {2, 1}}
            }},
        3);
        for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 4; ++turns)
            check(submit(*c, "initiative_keep") || submit(*c, "end"), "Reach the Fighter's turn");
        check(submit(*c, "melee", 98) && attack_lines(*c) == 1, "The first attack");
        if (level == 4)
        {
            check(!offered(*c, "melee"), "A level-4 Fighter attacks once");
            continue;
        }
        check(offered(*c, "melee") && !offered(*c, "dodge"),
              "Extra Attack offers a second attack, not another action");
        const auto saved = c->save();
        auto restored = rules->restore(saved);
        check(restored->save() == saved && offered(*restored, "melee"),
              "A checkpoint keeps the attack left");
        check(submit(*c, "melee", 98) && attack_lines(*c) == 2 && !offered(*c, "melee"),
              "The second attack ends the Attack action");
        check(submit(*c, "action_surge") && submit(*c, "melee", 98) &&
              submit(*c, "melee", 98) && attack_lines(*c) == 4 && !offered(*c, "melee"),
              "Action Surge's Attack action makes two attacks too");
    }
}

// Tactical Shift: after Second Wind, half Speed of movement that provokes no
// Opportunity Attack; the turn's Action is left.
void tactical_shift_checks()
{
    const auto rules = dummy_rules();
    const auto hero = fighter(5);
    auto c = duel(*rules, hero, {}, VitalState{5, false, {}, {}});
    check(submit(*c, "second_wind") && logged(*c, "Tactical Shift: Veteran may move 15 feet"),
          "Second Wind opens Tactical Shift's 15 feet");
    const auto free = c->snapshot().free_movement;
    check(free && free->actor == 1 && free->remaining_feet == 15, "The free move is shown");
    const auto saved = c->save();
    check(rules->restore(saved)->save() == saved, "A checkpoint keeps the shift");
    bool moved = false;
    for (const auto &command : c->legal_commands())
        if (command.verb == "move" && command.destination == Cell{3, 1})
            moved = c->submit(command);
    check(moved && !c->snapshot().reaction_pending && !logged(*c, "Dummy -> Veteran"),
          "Leaving the dummy's reach provokes nothing");
    check(submit(*c, "end") && !c->snapshot().free_movement && c->snapshot().actor == 1 &&
          offered(*c, "dash"), "Finishing the shift leaves the turn's Action");
}

// Tactical Master: Push, Sap or Slow in place of the weapon's own mastery,
// with Extra Attack's second attack too.
void tactical_master_checks()
{
    const auto rules = dummy_rules();
    const auto hero = fighter(9);
    std::string weapon;
    for (const auto &g : hero.sheet().grants)
        if (g.id.starts_with("mastery:") && weapon.empty())
            weapon = g.id.substr(8);
    auto c = duel(*rules, hero, {weapon});
    const bool push = offered(*c, "melee_push"), sap = offered(*c, "melee_sap"),
               slow = offered(*c, "melee_slow");
    check(push + sap + slow == 2, "Tactical Master offers the two masteries the weapon lacks: " +
          weapon);
    const auto verb = push ? "melee_push" : "melee_slow";
    check(submit(*c, verb, 98), "A Tactical Master attack");
    const auto saved = c->save();
    check(rules->restore(saved)->save() == saved, "A checkpoint keeps a pending replaced mastery");
    // A hit's Push or Slow is asked about; skipping it leaves the second attack.
    if (offered(*c, "effect_skip"))
        check(submit(*c, "effect_skip"), "Skip the replaced mastery");
    check(!offered(*c, "dodge") && offered(*c, verb),
          "Extra Attack's second attack may replace the mastery too");
}

// Heroic Warrior: Heroic Inspiration at the start of a level-10 Champion's turn.
void heroic_warrior_checks()
{
    const auto rules = dummy_rules();
    auto c = duel(*rules, fighter(10));
    check(logged(*c, "Veteran gains Heroic Inspiration (Heroic Warrior)."),
          "Heroic Warrior grants Heroic Inspiration at the turn's start");
    check(!logged(*duel(*rules, fighter(9)), "Heroic Warrior"), "Not before level 10");
}

// Indomitable: a level-9 Fighter rerolls a failed save it could still pass,
// adding its level, once per Long Rest.
void indomitable_checks()
{
    const auto rules = module();
    const auto profile = rules->character_profile(fighter(9).sheet(), {}).data;
    for (std::uint64_t seed = 1; seed < 400; ++seed)
    {
        auto c = rules->create({{8, 3, std::vector<Terrain>(24)},
            {   {1, "campaign-character", "Veteran", Side::party, {1, 1}, profile},
                {98, "blindness-adept", "Adept", Side::opposition, {6, 1}}
            }},
        seed);
        for (unsigned turns = 0; turns < 8 && !logged(*c, "uses Indomitable"); ++turns)
            if (c->snapshot().actor == 1 || c->snapshot().reaction_pending)
                check(submit(*c, "initiative_keep") || submit(*c, "decline") || submit(*c, "end"),
                      "The Fighter waits");
            else
                check(c->submit(choose_demo_command(*c)), "The adept acts");
        if (!logged(*c, "Veteran uses Indomitable and rolls the save again."))
            continue;
        unsigned left = 1;
        for (const auto &unit : c->snapshot().combatants)
            if (unit.id == 1)
                for (const auto &pool : unit.resources)
                    if (pool.id == "indomitable")
                        left = pool.remaining;
        check(left == 0, "Indomitable's one use is spent");
        return;
    }
    throw std::runtime_error("No seed's Blindness save calls for Indomitable");
}

// Other classes stay at level 4 until their features through 10 work.
void cap_checks()
{
    CampaignParty party(module());
    const auto id = party.add_pc(Character(*srd5::character_rules(), draft("barbarian"), {}));
    party.award_experience(64000, "levels-xp");
    while (party.can_advance(id))
        party.advance(id, party.default_advancement(id));
    check(party.member(id).character.sheet().level == 4, "A Barbarian stops at level 4 for now");
}
} // namespace

int main()
{
    try
    {
        fighter_checks();
        extra_attack_checks();
        tactical_shift_checks();
        tactical_master_checks();
        heroic_warrior_checks();
        indomitable_checks();
        cap_checks();
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Fighter level tests passed\n";
}
