#include "opengold/campaign_party.h"
#include "opengold/campaign_save.h"
#include "opengold/character.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace opengold;
using namespace opengold::rules;

// A Cleric (Life Domain) from level 5 to 8 (LEVELS-1, LEVELS-2): level-3 and
// level-4 spells, Sear Undead, Blessed Healer, a third Channel Divinity use,
// Blessed Strikes and Divine Spark's 2d8.
namespace
{
const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR);
constexpr std::string_view strikes = "class:cleric:blessed_strikes";

void check(bool ok, const std::string &message)
{
    if (!ok)
        throw std::runtime_error(message);
}

std::unique_ptr<RulesModule> module()
{
    return srd5::load(root / "data/rules/srd-5.2.1/combat.rules");
}

// An AC 1 dummy, a brute that always hits, an Undead walker and a Humanoid
// bandit's corpse.
std::unique_ptr<RulesModule> dummy_rules()
{
    std::ifstream in(root / "data/rules/srd-5.2.1/combat.rules");
    return srd5::parse_content(std::string{std::istreambuf_iterator<char>(in), {}} +
                               "\ncreature dummy 1 1000 -10 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n"
                               "creature brute 1 1000 20 30 30 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n"
                               "creature walker 1 1000 -10 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n"
                               "type walker undead\n");
}

CharacterDraft draft()
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = "cleric";
    d.background = "acolyte";
    d.alignment = "lawful_good";
    d.name = "Cleric";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    d.rolls[4] = {{6, 6, 6, 1}, 3};
    d.cantrips = std::vector<std::string> {"sacred_flame", "spare_the_dying", "resistance"};
    d.spells = SpellChoices{{}, std::vector<std::string>{"cure_wounds"}, {}, {}};
    d.training = {{"class:cleric", {"medicine", "persuasion"}},
        {"class:cleric:divine_order", {"protector"}}
    };
    return d;
}

// A Cleric of the level, advanced through the rules' default level-ups, with
// the Blessed Strikes option `strike` at level 7.
Character cleric(unsigned level, std::string strike = "divine_strike")
{
    Character hero(*srd5::character_rules(), draft(), {});
    VitalState life{hero.sheet().hit_points, false, {}};
    const auto rules = module();
    while (hero.sheet().level < static_cast<int>(level))
    {
        auto choice = rules->default_advancement(hero.sheet());
        if (choice.training.contains(std::string(strikes)))
            choice.training[std::string(strikes)] = {strike};
        check(hero.advance(*rules, life, choice), "An ordinary level-up");
    }
    return hero;
}

// The Cleric's sheet with `spell` prepared in place of its first prepared spell.
CharacterSheet with_spell(const Character &hero, const std::string &spell)
{
    auto sheet = hero.sheet();
    auto &prepared = sheet.prepared_spells;
    if (std::find(prepared.begin(), prepared.end(), spell) == prepared.end())
        prepared.front() = spell;
    return sheet;
}

bool offered(const CombatSession &c, std::string_view verb, EntityId target = 0)
{
    const auto commands = c.legal_commands();
    return std::any_of(commands.begin(), commands.end(), [&](const auto & command)
    {
        return command.verb == verb && (!target || command.target == target);
    });
}

bool submit(CombatSession &c, std::string_view verb, EntityId target = 0)
{
    for (const auto &command : c.legal_commands())
        if (command.verb == verb && (!target || command.target == target))
            return c.submit(command);
    return false;
}

std::vector<std::string> log_lines(const CombatSession &c)
{
    return c.snapshot().log();
}

bool logged(const CombatSession &c, std::string_view text)
{
    const auto log = log_lines(c);
    return std::any_of(log.begin(), log.end(), [&](const auto & line)
    {
        return line.find(text) != std::string::npos;
    });
}

CombatantView unit(const CombatSession &c, EntityId id)
{
    for (const auto &a : c.snapshot().combatants)
        if (a.id == id)
            return a;
    throw std::runtime_error("Missing combatant");
}

// Spell slots of a level left: from the third field of the saved vital state,
// one field per level.
unsigned slots(const CombatSession &c, unsigned level)
{
    std::istringstream in(unit(c, 1).persistent.resources);
    std::string magic;
    unsigned winds{}, count{};
    in >> magic >> winds;
    for (unsigned n = 1; n <= level; ++n)
        in >> count;
    return count;
}

unsigned remaining(const CombatSession &c, std::string_view pool)
{
    for (const auto &resource : unit(c, 1).resources)
        if (resource.id == pool)
            return resource.remaining;
    return 0;
}

void to_turn(CombatSession &c, EntityId id)
{
    for (unsigned turns = 0; c.snapshot().actor != id && turns < 12; ++turns)
        check(submit(c, "end"), "End a turn");
    check(c.snapshot().actor == id, "Reach the turn");
}

// The Cleric (1, Hit Points `hp` if given), two wounded allies (2, 3) and a foe
// (98) of `kind`, 10 feet away, on the Cleric's turn.
std::unique_ptr<CombatSession> chapel(const RulesModule &rules, const CharacterSheet &sheet,
                                      std::string kind, std::uint64_t seed, int hp = 0)
{
    const std::vector<std::string> gear{"mace"};
    std::optional<VitalState> life;
    if (hp)
        life = VitalState{hp, false, {}};
    auto c = rules.create({{12, 5, std::vector<Terrain>(60)},
        {   {1, "campaign-character", "Cleric", Side::party, {1, 2},
                rules.character_profile(sheet, gear).data, life},
            {2, "dummy", "Ally", Side::party, {1, 1}, {}, VitalState{3, false, {}}},
            {3, "dummy", "Friend", Side::party, {1, 3}, {}, VitalState{3, false, {}}},
            {98, std::move(kind), "Foe", Side::opposition, {3, 2}}
        }},
    seed);
    to_turn(*c, 1);
    return c;
}

bool grants(const Character &hero, std::string_view id, int level)
{
    const auto &all = hero.sheet().grants;
    return std::any_of(all.begin(), all.end(), [&](const auto & g)
    {
        return g.id == id && static_cast<int>(g.level) == level;
    });
}

void progression_checks()
{
    const auto rules = module();
    CampaignParty party(module());
    const auto id = party.add_pc(Character(*srd5::character_rules(), draft(), {}));
    party.award_experience(165000, "levels-xp");
    while (party.can_advance(id))
        party.advance(id, party.default_advancement(id));
    check(party.member(id).character.sheet().level == 8,
          "A Cleric stops at 8 until its level-5 spells exist");
    check(grants(cleric(5), "feature:sear_undead", 5) &&
          grants(cleric(6), "feature:blessed_healer", 6),
          "Sear Undead at 5, Blessed Healer at 6");
    const auto training = rules->advancement_options(cleric(6).sheet()).training;
    check(training.size() == 1 && training.front().id == strikes,
          "Level 7 offers Blessed Strikes");
    check(grants(cleric(7), "strikes:divine_strike", 7) &&
          grants(cleric(7, "potent_spellcasting"), "strikes:potent_spellcasting", 7),
          "Blessed Strikes takes the chosen option");
    for (const auto &[level, count] : std::vector<std::pair<unsigned, unsigned>>
{
    {5, 9}, {6, 10}, {7, 11}, {8, 12}
})
    check(rules->spell_access(cleric(level).sheet()).prepared_choices == count,
          "Prepared spells: 9 at 5 to 12 at 8");
    const auto preparable = [&](const Character & hero, std::string_view spell)
    {
        const auto options = rules->spell_choice_options(hero.sheet(), SpellChoiceContext::long_rest);
        return std::any_of(options.preparation.begin(), options.preparation.end(),
                           [&](const auto & o)
        {
            return o.id == spell;
        });
    };
    for (const auto *spell : {"animate_dead", "beacon_of_hope", "bestow_curse", "dispel_magic",
                              "magic_circle", "protection_from_energy", "remove_curse",
                              "spirit_guardians"
                             })
        check(preparable(cleric(5), spell), std::string("A level-5 Cleric can prepare ") + spell);
    check(!preparable(cleric(4), "spirit_guardians"), "No level-3 spells at 4");
    for (const auto *spell : {"banishment", "freedom_of_movement", "guardian_of_faith"})
        check(preparable(cleric(7), spell), std::string("A level-7 Cleric can prepare ") + spell);
    const auto dummies = dummy_rules();
    auto c5 = chapel(*dummies, cleric(5).sheet(), "dummy", 1);
    check(slots(*c5, 3) == 2 && remaining(*c5, "channel_divinity") == 2,
          "Two level-3 slots and two Channel Divinity uses at 5");
    check(offered(*c5, "mass_healing_word"), "Mass Healing Word is always prepared at 5");
    auto c6 = chapel(*dummies, cleric(6).sheet(), "dummy", 1);
    check(slots(*c6, 3) == 3 && remaining(*c6, "channel_divinity") == 3,
          "Three level-3 slots and three Channel Divinity uses at 6");
    auto c7 = chapel(*dummies, cleric(7).sheet(), "dummy", 1);
    check(slots(*c7, 4) == 1 && slots(*chapel(*dummies, cleric(8).sheet(), "dummy", 1), 4) == 2,
          "One level-4 slot at 7, two at 8");
    check(offered(*c7, "aura_of_life") && offered(*c7, "death_ward"),
          "Aura of Life and Death Ward are always prepared at 7");
    const auto bytes = encode_campaign(party, nullptr, "cleric-levels");
    CampaignParty next(module());
    next.restore(decode_campaign(bytes, *srd5::character_rules(), party.rule_module(),
                                 "cleric-levels", nullptr).party);
    check(encode_campaign(next, nullptr, "cleric-levels") == bytes,
          "A level-8 Cleric saves and reloads");
}

void mass_healing_word_checks()
{
    const auto rules = dummy_rules();
    auto c = chapel(*rules, cleric(5).sheet(), "dummy", 1, 10);
    check(submit(*c, "mass_healing_word", 2) && c->snapshot().spell_targeting &&
          submit(*c, "mass_healing_word", 3) && submit(*c, "spell_cast"),
          "Mass Healing Word chooses its creatures");
    check(unit(*c, 2).hit_points > 3 && unit(*c, 3).hit_points > 3 && slots(*c, 3) == 1 &&
          !unit(*c, 1).bonus_action && unit(*c, 1).action && unit(*c, 1).hit_points == 10,
          "Both allies heal for a level-3 slot and the Bonus Action; no Blessed Healer at 5");
    auto six = chapel(*rules, cleric(6).sheet(), "dummy", 1, 10);
    check(submit(*six, "mass_healing_word", 2) && submit(*six, "spell_cast") &&
          unit(*six, 1).hit_points == 15 && logged(*six, "Blessed Healer: Cleric is healed too."),
          "Blessed Healer: 2 + the slot level back to the Cleric at 6");
}

void spirit_guardians_checks()
{
    const auto rules = dummy_rules();
    bool struck = false;
    for (std::uint64_t seed = 1; seed < 40 && !struck; ++seed)
    {
        auto c = chapel(*rules, with_spell(cleric(5), "spirit_guardians"), "dummy", seed);
        const auto reach = c->movement_reach(98).size();
        check(submit(*c, "spirit_guardians", 1) && slots(*c, 3) == 1 &&
              logged(*c, "Spirit guardians flit around Cleric.") &&
              !c->snapshot().spirits.empty(), "Spirit Guardians surround the Cleric");
        check(c->movement_reach(98).size() < reach, "The guardians slow the foe within them");
        const auto saved = c->save();
        check(rules->restore(saved)->save() == saved, "A checkpoint keeps the guardians");
        to_turn(*c, 98);
        check(submit(*c, "end"), "The foe ends its turn within the guardians");
        check(logged(*c, "Foe Wisdom save:"), "Ending a turn within them forces a save");
        struck = logged(*c, "Radiant damage from the spirit guardians.");
    }
    check(struck, "The guardians deal Radiant damage");
}

void bestow_curse_checks()
{
    const auto rules = dummy_rules();
    auto c = chapel(*rules, with_spell(cleric(5), "bestow_curse"), "dummy", 1);
    Command step;
    for (const auto &command : c->legal_commands())
        if (command.verb == "move" && command.destination == Cell{2, 2})
            step = command;
    check(step.actor == 1 && c->submit(step), "Step beside the foe");
    for (const auto *curse : {"strength", "dexterity", "constitution", "intelligence", "wisdom",
                              "charisma", "attacks", "dodge", "necrotic"
                             })
        check(offered(*c, std::string("bestow_curse_") + curse, 98),
              std::string("Bestow Curse offers ") + curse);
    bool cursed = false;
    for (std::uint64_t seed = 1; seed < 40 && !cursed; ++seed)
    {
        auto trial = chapel(*rules, with_spell(cleric(5), "bestow_curse"), "dummy", seed);
        Command near;
        for (const auto &command : trial->legal_commands())
            if (command.verb == "move" && command.destination == Cell{2, 2})
                near = command;
        check(trial->submit(near) && submit(*trial, "bestow_curse_necrotic", 98),
              "Cast Bestow Curse");
        if (!logged(*trial, "Foe is cursed: extra Necrotic damage."))
            continue;
        cursed = true;
        const auto saved = trial->save();
        check(rules->restore(saved)->save() == saved, "A checkpoint keeps the curse");
        for (unsigned swings = 0; swings < 4 && !logged(*trial, "Necrotic damage from the curse.");
                ++swings)
        {
            check(submit(*trial, "end"), "End the Cleric's turn");
            to_turn(*trial, 1);
            check(submit(*trial, "melee", 98), "Strike the cursed foe");
        }
        check(logged(*trial, "Necrotic damage from the curse."),
              "The cursed foe takes 1d8 Necrotic from the Cleric's hits");
    }
    check(cursed, "A seed where the foe fails its save");
}

void animate_dead_checks()
{
    const auto rules = dummy_rules();
    const auto sheet = with_spell(cleric(5), "animate_dead");
    const auto battle = [&]
    {
        const std::vector<std::string> gear{"mace"};
        auto c = rules->create({{12, 5, std::vector<Terrain>(60)},
            {   {1, "campaign-character", "Cleric", Side::party, {1, 2},
                    rules->character_profile(sheet, gear).data},
                {97, "bandit", "Corpse", Side::opposition, {2, 2}, {}, VitalState{0, true, {}}},
                {98, "dummy", "Foe", Side::opposition, {10, 4}}
            }},
        1);
        to_turn(*c, 1);
        return c;
    };
    auto c = battle();
    check(offered(*c, "animate_dead", 97) && submit(*c, "animate_dead", 97) && slots(*c, 3) == 1 &&
          logged(*c, "Cleric begins casting Animate Dead."), "Animate Dead begins on the corpse");
    for (unsigned turns = 0; turns < 40 && !logged(*c, "Corpse rises as a Zombie."); ++turns)
        check(submit(*c, "end"), "Pass the minute of casting");
    check(logged(*c, "Cleric keeps casting Animate Dead.") && logged(*c, "Corpse rises as a Zombie.") &&
          unit(*c, 97).side == Side::party && unit(*c, 97).hit_points == 15,
          "After a minute the corpse rises as a Zombie fighting for the party");
    const auto saved = c->save();
    check(rules->restore(saved)->save() == saved, "A checkpoint keeps the Zombie");
    const auto order = c->snapshot().combatants;
    const auto place = [&](EntityId id)
    {
        return std::find_if(order.begin(), order.end(), [&](const auto & a)
        {
            return a.id == id;
        }) - order.begin();
    };
    check(place(97) == place(1) + 1, "The Zombie acts right after the Cleric");
    auto held = battle();
    check(submit(*held, "animate_dead", 97), "Cast Animate Dead");
    to_turn(*held, 98);
    to_turn(*held, 1);
    check(!unit(*held, 1).action && logged(*held, "Cleric keeps casting Animate Dead."),
          "The casting takes the Action each turn of its minute");
}

void sear_undead_checks()
{
    const auto rules = dummy_rules();
    bool seared = false;
    for (std::uint64_t seed = 1; seed < 40 && !seared; ++seed)
    {
        auto four = chapel(*rules, cleric(4).sheet(), "walker", seed);
        check(submit(*four, "turn_undead", 1), "Turn Undead at 4");
        check(!logged(*four, "Radiant damage from Sear Undead."), "No Sear Undead before 5");
        auto c = chapel(*rules, cleric(5).sheet(), "walker", seed);
        check(submit(*c, "turn_undead", 1), "Turn Undead at 5");
        seared = logged(*c, "Foe takes ") && logged(*c, " Radiant damage from Sear Undead.");
        if (seared)
            check(logged(*c, "Foe is turned."), "The searing does not end the turning");
    }
    check(seared, "Sear Undead burns an Undead that fails its save");
}

void blessed_strikes_checks()
{
    const auto rules = dummy_rules();
    bool struck = false;
    for (std::uint64_t seed = 1; seed < 40 && !struck; ++seed)
    {
        auto c = chapel(*rules, cleric(7).sheet(), "dummy", seed);
        Command step;
        for (const auto &command : c->legal_commands())
            if (command.verb == "move" && command.destination == Cell{2, 2})
                step = command;
        check(c->submit(step) && submit(*c, "melee", 98), "The Cleric swings its mace");
        if (!offered(*c, "divine_strike_radiant", 98))
            continue;
        check(offered(*c, "divine_strike_necrotic", 98), "Divine Strike offers both types");
        check(submit(*c, "divine_strike_necrotic", 98) &&
              logged(*c, " Necrotic damage from Divine Strike.") &&
              !offered(*c, "divine_strike_radiant", 98), "Divine Strike, once a turn");
        struck = true;
    }
    check(struck, "A seed where the mace hits");
    auto potent = chapel(*rules, cleric(7, "potent_spellcasting").sheet(), "dummy", 1);
    Command step;
    for (const auto &command : potent->legal_commands())
        if (command.verb == "move" && command.destination == Cell{2, 2})
            step = command;
    check(potent->submit(step) && submit(*potent, "melee", 98) &&
          !offered(*potent, "divine_strike_radiant"), "Potent Spellcasting has no Divine Strike");
}
} // namespace

int main()
{
    try
    {
        progression_checks();
        mass_healing_word_checks();
        spirit_guardians_checks();
        bestow_curse_checks();
        animate_dead_checks();
        sear_undead_checks();
        blessed_strikes_checks();
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Cleric level checks passed\n";
}
