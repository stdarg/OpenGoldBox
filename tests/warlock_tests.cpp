#include "opengold/campaign_party.h"
#include "opengold/character.h"
#include "opengold/combat_demo.h"
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

namespace
{
const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR);

void check(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}

std::string read(const std::filesystem::path &p)
{
    std::ifstream in(p);
    check(bool(in), "Read rules");
    return {std::istreambuf_iterator<char>(in), {}};
}

// A sturdy AC 1 target that hits back.
std::unique_ptr<RulesModule> rules()
{
    return srd5::parse_content(read(root / "data/rules/srd-5.2.1/combat.rules") +
                               "\ncreature target 1 1000 0 30 20 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n" +
                               "creature weakling 1 1 0 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n");
}

std::unique_ptr<RulesModule> module_rules()
{
    return srd5::load(root / "data/rules/srd-5.2.1/combat.rules");
}

// A Warlock with Hex and Hellish Rebuke prepared and Charisma 18, advanced to
// the given level, with the given invocations at levels 1 and 2 and spells
// added to its preparation at level 3.
Character warlock(unsigned level = 1, std::vector<std::string> first = {},
                  std::vector<std::string> second = {}, std::vector<std::string> third = {})
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = "warlock";
    d.background = "sage";
    d.alignment = "neutral_good";
    d.name = "Warlock";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    d.rolls[5] = {{6, 6, 6, 1}, 3};
    d.training = {{"class:warlock", {"arcana", "deception"}}};
    if (!first.empty())
        d.training["class:warlock:invocations"] = first;
    d.cantrips = {"eldritch_blast", "chill_touch"};
    d.spells = SpellChoices{{}, std::vector<std::string> {"hex", "hellish_rebuke"}, {}, {}};
    CampaignParty party(module_rules());
    const auto id = party.add_pc(Character(*srd5::character_rules(), d, {}));
    party.award_experience(2700, "warlock-xp");
    for (unsigned n = 1; n < level; ++n)
    {
        auto choice = party.default_advancement(id);
        if (n == 1 && !second.empty())
            choice.training["class:warlock:invocations:2"] = second;
        if (n == 2 && !third.empty())
        {
            choice.spells = party.member(id).character.sheet().prepared_spells;
            choice.spells.insert(choice.spells.end(), third.begin(), third.end());
        }
        party.advance(id, choice);
    }
    return party.member(id).character;
}

CombatantView unit(const CombatSession &c, EntityId id)
{
    for (const auto &a : c.snapshot().combatants)
        if (a.id == id)
            return a;
    throw std::runtime_error("Missing combatant");
}

bool submit(CombatSession &c, std::string_view verb, EntityId target = 0)
{
    for (const auto &command : c.legal_commands())
        if (command.verb == verb && (!target || command.target == target))
            return c.submit(command);
    return false;
}

bool offered(const CombatSession &c, std::string_view verb)
{
    const auto commands = c.legal_commands();
    return std::any_of(commands.begin(), commands.end(), [&](const auto & command)
    {
        return command.verb == verb;
    });
}

bool logged(const CombatSession &c, std::string_view text)
{
    const auto log = c.snapshot().log;
    return std::any_of(log.begin(), log.end(), [&](const auto & line)
    {
        return line.find(text) != std::string::npos;
    });
}

// Level-one and level-two slots in "SRD11 winds slots slots2 ...".
std::pair<int, int> slots(const VitalState &vitals)
{
    std::istringstream in(vitals.resources);
    std::string magic;
    int winds{}, first{}, second{};
    in >> magic >> winds >> first >> second;
    return {first, second};
}

// Ends the Warlock's turn and comes back to it.
void reach_turn(CombatSession &c);

// The Warlock (1) and an enemy (98) beside it.
std::unique_ptr<CombatSession> battle(const RulesModule &module, const Character &hero,
                                      std::uint64_t seed = 5, std::string enemy = "target")
{
    const auto profile = module.character_profile(hero.sheet(), std::vector<std::string> {}).data;
    auto c = module.create({{12, 6, std::vector<std::uint8_t>(72)},
        {   {1, "campaign-character", "Warlock", 0, {1, 1}, profile},
            {98, enemy, "Enemy", 1, {2, 1}}
        }},
    seed);
    if (!c->snapshot().initiative_choices.empty())
        check(submit(*c, "initiative_keep"), "Keep Initiative");
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 4; ++turns)
        check(submit(*c, "end"), "Reach the Warlock's turn");
    check(c->snapshot().actor == 1, "The Warlock acts");
    return c;
}

void reach_turn(CombatSession &c)
{
    check(submit(c, "end"), "End the Warlock's turn");
    for (unsigned turns = 0; c.snapshot().actor != 1 && turns < 4; ++turns)
        check(submit(c, "end"), "Back to the Warlock");
}

void pact_magic_checks()
{
    auto module = rules();
    auto c = battle(*module, warlock());
    check(slots(unit(*c, 1).persistent) == std::pair{1, 0}, "One level-1 Pact Magic slot");
    check(submit(*c, "hex", 98) && logged(*c, "Enemy is hexed.") && unit(*c, 1).action,
          "Hex is a Bonus Action");
    check(submit(*c, "eldritch_blast", 98), "Eldritch Blast");
    check(!logged(*c, " hits for ") || logged(*c, "Necrotic damage from Hex."),
          "A hit on the hexed creature adds 1d6 Necrotic");
    auto rules_module = module_rules();
    auto spent = unit(*c, 1).persistent;
    check(slots(spent).first == 0, "Hex spent the slot");
    rules_module->recover_short_rest(spent, warlock().sheet());
    check(slots(spent).first == 1, "A Short Rest restores Pact Magic slots");
}

void hellish_rebuke_checks()
{
    auto module = rules();
    for (std::uint64_t seed = 1; seed < 64; ++seed)
    {
        auto c = battle(*module, warlock(), seed);
        check(submit(*c, "end") && submit(*c, "melee", 1), "The enemy attacks");
        if (!c->snapshot().reaction_pending)
            continue;
        check(c->snapshot().actor == 1 && offered(*c, "rebuke") && offered(*c, "decline"),
              "A hurt Warlock is asked about Hellish Rebuke");
        const auto saved = c->save();
        check(module->restore(saved)->save() == saved, "The question survives a checkpoint");
        check(submit(*c, "rebuke") && logged(*c, "Warlock casts Hellish Rebuke at Enemy.") &&
              logged(*c, "Enemy Dexterity save") && logged(*c, "Fire damage") &&
              slots(unit(*c, 1).persistent).first == 0 && !unit(*c, 1).reaction,
              "Hellish Rebuke spends the Reaction and the slot");
        return;
    }
    throw std::runtime_error("No seed hurts the Warlock");
}

void advancement_checks()
{
    auto module = rules();
    auto rules_module = module_rules();
    const auto second = warlock(2);
    auto c = battle(*module, second);
    check(slots(unit(*c, 1).persistent) == std::pair{2, 0}, "Two level-1 slots at level 2");
    check(submit(*c, "hex", 98), "Spend a slot on Hex");
    const auto actions = rules_module->camp_actions(second.sheet(), unit(*c, 1).persistent);
    check(std::any_of(actions.begin(), actions.end(), [](const auto & a)
    {
        return a.id == "magical_cunning";
    }),
    "Magical Cunning is offered at camp with a slot spent");
    auto vitals = unit(*c, 1).persistent;
    std::vector<CampTarget> party{{&second.sheet(), &vitals}};
    std::uint64_t random = 7;
    // Reaching level 3 with a slot spent: the Pact slots become level 2 and
    // the spent one stays spent.
    auto spent = unit(*c, 1).persistent;
    auto sheet = second.sheet();
    check(rules_module->advance_character(sheet, spent, rules_module->default_advancement(sheet)) &&
          slots(spent) == std::pair{0, 1},
          "A spent Pact slot stays spent when the slots change level");
    rules_module->use_party_camp_action(second.sheet(), vitals, party, "magical_cunning", random);
    check(slots(vitals).first == 2, "Magical Cunning restores the spent slot");

    const auto third = warlock(3);
    auto d = battle(*module, third);
    check(slots(unit(*d, 1).persistent) == std::pair{0, 2} && offered(*d, "hex_2") &&
          !offered(*d, "hex"),
          "From level 3 two level-2 slots cast level-1 spells at level 2");
    check(warlock(4).sheet().level == 4, "A Warlock reaches level 4");
}

void invocation_checks()
{
    auto creation = srd5::character_rules();
    CharacterDraft draft;
    draft.character_class = "warlock";
    draft.background = "sage";
    std::vector<std::string> first;
    for (const auto &group : creation->training_options(draft))
        if (group.id == "class:warlock:invocations")
            for (const auto &option : group.options)
                first.push_back(option.id);
    check(first == std::vector<std::string> {"armor_of_shadows", "eldritch_mind", "pact_of_the_blade"},
          "Level 1 offers the invocations without prerequisites");
    auto module = rules();
    {
        auto c = battle(*module, warlock(1, {"armor_of_shadows"}));
        const int ac = unit(*c, 1).armor_class;
        check(submit(*c, "armor_of_shadows", 1) && unit(*c, 1).armor_class == ac + 3 &&
              slots(unit(*c, 1).persistent).first == 1,
              "Armor of Shadows casts Mage Armor without a slot");
    }
    {
        const auto hero = warlock(2, {"eldritch_mind"}, {"agonizing_blast", "repelling_blast"});
        auto c = battle(*module, hero);
        check(submit(*c, "eldritch_blast", 98) && logged(*c, "Enemy is pushed 10 feet."),
              "Repelling Blast pushes the creature Eldritch Blast hits");
        for (const auto &line : c->snapshot().log)
            if (line.starts_with("Warlock -> Enemy") && line.find(" hits for ") != std::string::npos)
            {
                const int damage = std::stoi(line.substr(line.find(" hits for ") + 10));
                check(damage >= 5 && damage <= 14, "Agonizing Blast adds +4 to 1d10");
            }
    }
    {
        const auto hero = warlock(2, {"eldritch_mind"}, {"fiendish_vigor", "lessons_alert"});
        auto c = battle(*module, hero);
        check(submit(*c, "fiendish_vigor", 1) && unit(*c, 1).temporary_hp.amount == 12,
              "Fiendish Vigor grants False Life's highest result");
        const auto profile = module->character_profile(hero.sheet(), std::vector<std::string> {}).data;
        auto start = module->create({{12, 6, std::vector<std::uint8_t>(72)},
            {   {1, "campaign-character", "Warlock", 0, {1, 1}, profile},
                {2, "target", "Ally", 0, {1, 2}},
                {98, "target", "Enemy", 1, {2, 1}}
            }},
        5);
        check(start->snapshot().initiative_choices == std::vector<EntityId> {1},
              "Lessons of the First Ones grants Alert's Initiative swap");
    }
    {
        // Pact of the Blade: a longsword with proficiency and Charisma (+4).
        auto blade = rules();
        const auto hero = warlock(1, {"pact_of_the_blade"});
        const auto profile = blade->character_profile(hero.sheet(), std::vector<std::string> {"longsword"});
        check(profile.melee_attack_bonus == 6, "Pact of the Blade uses Charisma and proficiency");
    }
    {
        auto c = battle(*module, warlock(3, {"eldritch_mind"}, {"devils_sight", "agonizing_blast"},
                                         {"darkness"}));
        check(submit(*c, "darkness") && submit(*c, "area_cast"), "Darkness over both creatures");
        reach_turn(*c);
        check(submit(*c, "eldritch_blast", 98) && logged(*c, "(advantage)"),
              "Devil's Sight sees through Darkness the enemy cannot");
    }
}

bool has_grant(const Character &hero, std::string_view id)
{
    const auto &grants = hero.sheet().grants;
    return std::any_of(grants.begin(), grants.end(), [&](const auto & g)
    {
        return g.id == id;
    });
}

void fiend_checks()
{
    const auto third = warlock(3);
    check(has_grant(third, "subclass:fiend") && has_grant(third, "feature:dark_ones_blessing") &&
          has_grant(third, "feature:fiend_spells"),
          "Level three brings the Fiend Patron");
    auto module = rules();
    auto c = battle(*module, third);
    check(offered(*c, "burning_hands_2") && offered(*c, "command_halt_2") &&
          offered(*c, "scorching_ray"),
          "Fiend Spells are always prepared");
    for (std::uint64_t seed = 1; seed < 32; ++seed)
    {
        auto d = battle(*module, third, seed, "weakling");
        check(submit(*d, "eldritch_blast", 98), "Eldritch Blast at a weakling");
        if (unit(*d, 98).hit_points > 0)
            continue;
        // Charisma 18 (+4) and Warlock level 3.
        check(logged(*d, "Dark One's Blessing: Warlock gains 7 Temporary Hit Points.") &&
              unit(*d, 1).temporary_hp.amount == 7,
              "Dropping an enemy grants Dark One's Blessing");
        return;
    }
    throw std::runtime_error("No seed drops the weakling");
}



// The automated policy marks its foe with Hex, then blasts it.
void policy_checks()
{
    auto module = rules();
    auto c = battle(*module, warlock());
    const auto hex = choose_demo_command(*c);
    check(hex.verb == "hex" && hex.target == 98 && c->submit(hex), "The policy casts Hex first");
    check(choose_demo_command(*c).verb == "eldritch_blast", "Then it casts Eldritch Blast");
}

} // namespace

int main()
{
    try
    {
        policy_checks();
        pact_magic_checks();
        hellish_rebuke_checks();
        advancement_checks();
        invocation_checks();
        fiend_checks();
        std::cout << "Warlock tests passed\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
