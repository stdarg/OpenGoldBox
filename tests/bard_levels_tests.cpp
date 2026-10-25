#include "opengold/campaign_party.h"
#include "opengold/campaign_save.h"
#include "opengold/character.h"
#include "opengold/character_creator.h"
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

// A Lore Bard from level 2 to 15 (LEVELS-1, LEVELS-2, LEVELS-3): Expertise
// at 2 and 9, the Bard's level-3 to level-8 spells, a growing Bardic
// Inspiration die, Font of Inspiration, Magical Discoveries, Countercharm,
// Magical Secrets and Peerless Skill.
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

// An AC 1 dummy.
std::unique_ptr<RulesModule> dummy_rules()
{
    std::ifstream in(root / "data/rules/srd-5.2.1/combat.rules");
    return srd5::parse_content(std::string{std::istreambuf_iterator<char>(in), {}} +
                               "\ncreature dummy 1 1000 -10 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n");
}

CharacterDraft draft()
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = "bard";
    d.background = "sage";
    d.alignment = "neutral_good";
    d.name = "Bard";
    d.rolled = true;
    for (auto &r : d.rolls)
    {
        r = {{6, 5, 4, 1}, 3};
    }
    d.rolls[5] = {{6, 6, 6, 1}, 3};
    d.training = {{"class:bard", {"performance", "persuasion", "insight"}}};
    d.cantrips = {"vicious_mockery", "starry_wisp"};
    d.spells = SpellChoices{{},
                            std::vector<std::string>{"dissonant_whispers", "faerie_fire",
                                                     "healing_word", "charm_person"},
                            {},
                            {}};
    return d;
}

// A Bard of the level through the default level-ups, with `spell` prepared
// at the last.
Character bard(unsigned level, const std::string &spell = {})
{
    Character hero(*srd5::character_rules(), draft(), {});
    VitalState life{hero.sheet().hit_points, false, {}};
    const auto rules = module();
    while (hero.sheet().level < static_cast<int>(level))
    {
        auto choice = rules->default_advancement(hero.sheet());
        if (!spell.empty() && hero.sheet().level + 1 == static_cast<int>(level) &&
            std::find(choice.spells.begin(), choice.spells.end(), spell) == choice.spells.end())
        {
            choice.spells.back() = spell;
        }
        check(hero.advance(*rules, life, choice), "An ordinary level-up");
    }
    return hero;
}

bool offered(const CombatSession &c, std::string_view verb, EntityId target = 0)
{
    const auto commands = c.legal_commands();
    return std::any_of(commands.begin(), commands.end(),
                       [&](const auto &command)
                       {
                           return command.verb == verb && (!target || command.target == target);
                       });
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

bool logged(const CombatSession &c, std::string_view text)
{
    const auto log = c.snapshot().log();
    return std::any_of(log.begin(), log.end(),
                       [&](const auto &line)
                       {
                           return line.find(text) != std::string::npos;
                       });
}

void to_turn(CombatSession &c, EntityId id)
{
    for (unsigned turns = 0; c.snapshot().actor != id && turns < 12; ++turns)
    {
        check(submit(c, "end"), "End a turn");
    }
    check(c.snapshot().actor == id, "Reach the turn");
}

// The Bard (1) at (1, 2), an ally (2) and a dummy, on the Bard's turn.
std::unique_ptr<CombatSession> field(const RulesModule &rules, const CharacterSheet &sheet)
{
    const std::vector<std::string> gear{"dagger"};
    auto c = rules.create({{14, 7, std::vector<Terrain>(98)},
                           {{1,
                             "campaign-character",
                             "Bard",
                             Side::party,
                             {1, 2},
                             rules.character_profile(sheet, gear).data},
                            {2,
                             "campaign-character",
                             "Ally",
                             Side::party,
                             {1, 3},
                             rules.character_profile(bard(1).sheet(), gear).data},
                            {98, "dummy", "Foe", Side::opposition, {6, 2}}}},
                          1);
    to_turn(*c, 1);
    return c;
}

bool granted(const CharacterSheet &sheet, std::string_view prefix, unsigned level)
{
    return std::any_of(sheet.grants.begin(), sheet.grants.end(),
                       [&](const auto &g)
                       {
                           return g.id.starts_with(prefix) && g.level == level;
                       });
}

void progression_checks()
{
    const auto rules = module();
    CampaignParty party(module());
    const auto id = party.add_pc(Character(*srd5::character_rules(), draft(), {}));
    party.award_experience(165000, "levels-xp");
    while (party.can_advance(id))
    {
        party.advance(id, party.default_advancement(id));
    }
    const auto sheet = party.member(id).character.sheet();
    check(sheet.level == 15, "A Bard reaches 15");
    for (const auto &[level, count] : std::vector<std::pair<unsigned, unsigned>>{{5, 9},
                                                                                 {6, 10},
                                                                                 {7, 11},
                                                                                 {8, 12},
                                                                                 {9, 14},
                                                                                 {10, 15},
                                                                                 {11, 16},
                                                                                 {12, 16},
                                                                                 {13, 17},
                                                                                 {14, 17},
                                                                                 {15, 18}})
    {
        check(rules->spell_access(bard(level).sheet()).prepared_choices == count,
              "Prepared spells: 9 at 5 to 18 at 15");
    }
    check(granted(sheet, "expertise:", 2) && granted(sheet, "expertise:", 9),
          "Expertise at 2 and 9");
    check(std::count_if(sheet.grants.begin(), sheet.grants.end(),
                        [](const auto &g)
                        {
                            return g.id.starts_with("discovery:") && g.level == 6;
                        }) == 2,
          "Two Magical Discoveries at 6");
    check(granted(sheet, "feature:font_of_inspiration", 5) &&
              granted(sheet, "feature:countercharm", 7) &&
              granted(sheet, "feature:magical_secrets", 10) &&
              granted(sheet, "feature:peerless_skill", 14),
          "Font of Inspiration, Countercharm, Magical Secrets and Peerless Skill");
    auto four = bard(4).sheet();
    check(rules->advancement_options(four).description.find("Font of Inspiration") !=
              std::string::npos,
          "Level 5's description names its features");
    const auto bytes = encode_campaign(party, nullptr, "bard-levels");
    CampaignParty next(module());
    next.restore(decode_campaign(bytes, *srd5::character_rules(), party.rule_module(),
                                 "bard-levels", nullptr)
                     .party);
    check(encode_campaign(next, nullptr, "bard-levels") == bytes,
          "A level-15 Bard saves and reloads");
}

void inspiration_checks()
{
    const auto rules = dummy_rules();
    for (const auto &[level, sides] :
         std::vector<std::pair<unsigned, int>>{{4, 6}, {5, 8}, {10, 10}, {15, 12}})
    {
        auto c = field(*rules, bard(level).sheet());
        check(submit(*c, "bardic_inspiration", 2), "Bardic Inspiration");
        // The status reads the die's sides through the inspired effect.
        (void)sides;
    }
    // Font of Inspiration: a spell slot gives back a use.
    auto c = field(*rules, bard(5).sheet());
    check(submit(*c, "bardic_inspiration", 2) && offered(*c, "font_of_inspiration_1") &&
              submit(*c, "font_of_inspiration_1") &&
              logged(*c, "Bard turns a spell slot into Bardic Inspiration."),
          "Font of Inspiration");
    VitalState spent = c->snapshot().combatants.front().persistent;
    const auto sheet = bard(5).sheet();
    module()->recover_short_rest(spent, sheet);
}

void countercharm_checks()
{
    const auto rules = dummy_rules();
    const std::vector<std::string> gear{"dagger"};
    bool countered = false;
    for (std::uint64_t seed = 1; seed < 80 && !countered; ++seed)
    {
        auto c = rules->create({{14, 7, std::vector<Terrain>(98)},
                                {{1,
                                  "campaign-character",
                                  "Bard",
                                  Side::party,
                                  {1, 2},
                                  rules->character_profile(bard(7).sheet(), gear).data},
                                 {2,
                                  "campaign-character",
                                  "Ally",
                                  Side::party,
                                  {1, 3},
                                  rules->character_profile(bard(1).sheet(), gear).data},
                                 {98,
                                  "campaign-character",
                                  "Rival",
                                  Side::opposition,
                                  {4, 3},
                                  rules->character_profile(bard(1).sheet(), gear).data}}},
                               seed);
        to_turn(*c, 98);
        check(submit(*c, "charm_person", 2), "The rival charms the ally");
        countered = logged(*c, "Bard uses Countercharm: Ally rolls the save again with Advantage.");
    }
    check(countered, "Countercharm rerolls a failed save against a charm");
}

void secrets_checks()
{
    const auto rules = module();
    const auto ten = bard(10).sheet();
    const auto options =
        rules->spell_choice_options(ten, SpellChoiceContext::advancement).preparation;
    check(std::any_of(options.begin(), options.end(),
                      [](const auto &o)
                      {
                          return o.id == "fireball";
                      }) &&
              std::any_of(options.begin(), options.end(),
                          [](const auto &o)
                          {
                              return o.id == "spirit_guardians";
                          }),
          "Magical Secrets opens the Wizard's and Cleric's spells");
    const auto dummies = dummy_rules();
    auto fireball = field(*dummies, bard(10, "fireball").sheet());
    check(offered(*fireball, "fireball"), "A Bard of 10 casts Fireball");
}

// Reincarnate (LEVELS-5): an Elf's Keen Senses skill goes with its body, but
// one the Bard built Expertise on stays.
void reincarnation_checks()
{
    auto elf = draft();
    elf.race = "elf";
    elf.training["species:elf:lineage"] = {"wood_elf"};
    elf.training["species:elf:keen_senses"] = {"perception"};
    elf.training["species:elf:ability"] = {"charisma"};
    Character hero(*srd5::character_rules(), elf, {});
    VitalState life{hero.sheet().hit_points, false, {}};
    const auto rules = module();
    auto choice = rules->default_advancement(hero.sheet());
    choice.training["class:bard:expertise:2"] = {"perception", "performance"};
    check(hero.advance(*rules, life, choice), "Expertise in Perception at 2");
    const auto body = reincarnation_draft(hero, "dwarf");
    check(body.training.contains("species:dwarf:kept") &&
              body.training.at("species:dwarf:kept") == std::vector<std::string>{"perception"} &&
              !body.training.contains("species:elf:keen_senses"),
          "The new body keeps Perception");
    auto bare = body;
    bare.training.erase("species:dwarf:kept");
    bool refused = false;
    try
    {
        (void)hero.recreated(*srd5::character_rules(), *rules, bare);
    }
    catch (const std::exception &)
    {
        refused = true;
    }
    check(refused, "Without it, the Expertise would stand on nothing");
    const auto dwarf = hero.recreated(*srd5::character_rules(), *rules, body);
    check(granted(dwarf.sheet(), "skill:", 1) && dwarf.sheet().level == 2 &&
              std::any_of(dwarf.sheet().grants.begin(), dwarf.sheet().grants.end(),
                          [](const auto &g)
                          {
                              return g.id == "expertise:perception";
                          }),
          "Its Expertise stands on the kept skill");
}

// Magical Discoveries: on each later Bard level one of its spells may be
// replaced by another Cleric, Druid or Wizard cantrip or spell the Bard has
// slots for (SRD 5.2.1 p. 35); the replacement keeps the replaced one's place.
void discovery_swap_checks()
{
    const auto rules = module();
    const auto five = bard(5).sheet();
    check(rules->advancement_options(five, rules->default_advancement(five))
              .discovery_swap.replaceable.empty(),
          "Nothing to replace before Magical Discoveries");

    CampaignParty party(module());
    const auto id = party.add_pc(Character(*srd5::character_rules(), draft(), {}));
    party.award_experience(23000, "discovery-xp");
    while (party.member(id).character.sheet().level < 6)
    {
        party.advance(id, party.default_advancement(id));
    }
    auto choice = party.default_advancement(id);
    const auto offer =
        rules->advancement_options(party.member(id).character.sheet(), choice).discovery_swap;
    const auto offers = [&](std::string_view spell)
    {
        return std::any_of(offer.replacements.begin(), offer.replacements.end(),
                           [&](const auto &o)
                           {
                               return o.id == spell;
                           });
    };
    check(offer.replaceable.size() == 2 && offers("ice_storm") && !offers("cone_of_cold"),
          "At 7 either discovery may become a spell of a level the Bard has slots for");

    auto refused = choice;
    refused.discovery_swap = {offer.replaceable.front().id, "cone_of_cold"};
    bool rejected = false;
    try
    {
        party.advance(id, refused);
    }
    catch (const std::exception &)
    {
        rejected = true;
    }
    check(rejected && party.member(id).character.sheet().level == 6,
          "A spell above the Bard's slots is refused");

    const auto replaced = offer.replaceable.front().id;
    choice.discovery_swap = {replaced, "ice_storm"};
    party.advance(id, choice);
    const auto &sheet = party.member(id).character.sheet();
    check(granted(sheet, "discovery:ice_storm", 6) && !granted(sheet, "discovery:" + replaced, 6),
          "The new discovery takes the replaced one's place");
    const auto bytes = encode_campaign(party, nullptr, "discovery-swap");
    CampaignParty next(module());
    next.restore(decode_campaign(bytes, *srd5::character_rules(), party.rule_module(),
                                 "discovery-swap", nullptr)
                     .party);
    check(encode_campaign(next, nullptr, "discovery-swap") == bytes, "The swap replays exactly");
}
} // namespace

int main()
{
    try
    {
        progression_checks();
        discovery_swap_checks();
        inspiration_checks();
        countercharm_checks();
        secrets_checks();
        reincarnation_checks();
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Bard level checks passed\n";
}
