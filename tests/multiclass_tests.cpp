// Multiclassing (MULTI-1 in docs/SRD-DECISIONS.md): a character takes its
// next level in another class whose prerequisites it meets, with that class's
// level-one features, part of its training and its own level-one choices;
// its levels together keep the SRD's single experience table, and a save
// keeps the class of each level.
#include "opengold/campaign_party.h"
#include "opengold/campaign_save.h"
#include "opengold/srd5.h"
#include "test_support.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

using namespace opengold;
using namespace opengold::rules;

namespace
{
using opengold::test::check;

std::unique_ptr<RulesModule> module()
{
    std::ifstream in(std::filesystem::path(OPENGOLD_SOURCE_DIR) /
                     "data/rules/srd-5.2.1/combat.rules");
    check(bool(in), "Read the rules");
    const std::string content{std::istreambuf_iterator<char>(in), {}};
    return srd5::parse_content(content);
}

// A human Fighter whose ability rolls are each `die` three times: 5 makes
// every score 15, which meets every class's 13.
Character fighter(int die = 5)
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = "fighter";
    d.background = "soldier";
    d.alignment = "neutral_good";
    d.name = "Fighter";
    d.rolled = true;
    for (auto &r : d.rolls)
    {
        r = {{die, die, die, 1}, 3};
    }
    return Character(*srd5::character_rules(), d, {});
}

bool has_grant(const CharacterSheet &sheet, std::string_view id, std::string_view source)
{
    return std::any_of(sheet.grants.begin(), sheet.grants.end(),
                       [&](const auto &g)
                       {
                           return g.id == id && g.source_id == source;
                       });
}

// The party member, a Fighter 3, ready for level 4.
MemberId fighter_three(CampaignParty &party, int die = 5)
{
    const auto id = party.add_pc(fighter(die));
    party.award_experience(2700, "multiclass-xp");
    party.advance(id, party.default_advancement(id));
    party.advance(id, party.default_advancement(id));
    party.award_experience(2700, "multiclass-xp-2");
    return id;
}

// The choices a Rogue 1 taken by this character needs: its skill, Expertise
// and Weapon Mastery, the first of each offered.
AdvancementChoice rogue_entry(const CampaignParty &party, MemberId id)
{
    AdvancementChoice choice;
    choice.character_class = "rogue";
    for (int pass = 0; pass < 2; ++pass)
    {
        for (const auto &group : party.advancement_options(id, choice).training)
        {
            auto &picked = choice.training[group.id];
            picked.clear();
            for (const auto &option : group.options)
            {
                if (picked.size() < group.count)
                {
                    picked.push_back(option.id);
                }
            }
        }
    }
    return choice;
}

// The classes offered at level 4, the starting class first; all available
// with every score 15.
void offered_classes()
{
    CampaignParty party(module());
    const auto id = fighter_three(party);
    const auto classes = party.advancement_classes(id);
    check(classes.size() == 12 && classes.front().id == "fighter" &&
              std::all_of(classes.begin(), classes.end(),
                          [](const auto &c)
                          {
                              return c.available;
                          }),
          "Every class is offered, the Fighter first");
    const auto rogue = party.advancement_options(id, rogue_entry(party, id));
    check(rogue.level == 4 && rogue.class_level == 1 && rogue.character_class == "rogue",
          "Rogue is offered at level 1, as the character's level 4");
    check(std::any_of(rogue.training.begin(), rogue.training.end(),
                      [](const auto &g)
                      {
                          return g.id == "multiclass:rogue:skill" && g.count == 1;
                      }),
          "A Rogue brings one skill from its list");
}

// SRD 5.2.1 p. 24: 13 in the new class's primary ability and the current
// class's. Rolls of 12 leave Intelligence 12; a Soldier's +2 raises Strength.
void prerequisites()
{
    CampaignParty party(module());
    const auto id = fighter_three(party, 4);
    const auto &sheet = party.member(id).character.sheet();
    const auto classes = party.advancement_classes(id);
    const auto find = [&](std::string_view klass)
    {
        return *std::find_if(classes.begin(), classes.end(),
                             [&](const auto &c)
                             {
                                 return c.id == klass;
                             });
    };
    check(sheet.scores[Ability::intelligence] == 12 && sheet.scores[Ability::strength] >= 13,
          "The fixture's scores");
    check(classes.front().id == "fighter" && classes.front().available,
          "The current class is always offered");
    check(!find("wizard").available && !find("wizard").description.empty(),
          "Intelligence 12 does not open Wizard, and the list says why");
    check(find("barbarian").available, "Strength 13 opens Barbarian");
    AdvancementChoice wizard;
    wizard.character_class = "wizard";
    check(party.advancement_options(id, wizard).level == 0,
          "Wizard offers no level");
}

// Fighter 3 / Rogue 1: the Rogue's d8 and level-one features, its partial
// training, the Fighter's saves only, and Proficiency by total level.
void fighter_rogue()
{
    CampaignParty party(module());
    const auto id = fighter_three(party);
    const auto before = party.member(id).character.sheet().hit_points;
    party.advance(id, rogue_entry(party, id));
    const auto &sheet = party.member(id).character.sheet();
    check(sheet.level == 4 &&
              sheet.level_classes ==
                  std::vector<std::string>{"fighter", "fighter", "fighter", "rogue"},
          "The level is taken in Rogue");
    // d8 averages 5, Constitution 15 adds 2.
    check(sheet.hit_points == before + 7, "A Rogue level adds the Rogue's die");
    check(has_grant(sheet, "multiclass:rogue", "class:rogue") &&
              has_grant(sheet, "feature:sneak_attack", "class:rogue"),
          "Rogue 1's features and partial training");
    check(!sheet.save_proficiencies[Ability::dexterity] &&
              sheet.save_proficiencies[Ability::strength],
          "No Rogue saves: only the starting class's");
    check(std::count_if(sheet.grants.begin(), sheet.grants.end(),
                        [](const auto &g)
                        {
                            return g.source_id == "class:rogue:expertise";
                        }) == 2,
          "Rogue Expertise in two skills");
    const auto profile = module()->character_profile(sheet, {});
    check(profile.data.find(" fighter fighter fighter rogue ") != std::string::npos,
          "The combat profile carries each level's class");
    // Level 5 in Fighter: Extra Attack at Fighter 4 is not yet; the level is
    // the character's 5th, with the Proficiency Bonus of level 5.
    party.award_experience(6500, "multiclass-xp-3");
    party.advance(id, party.default_advancement(id));
    const auto &next = party.member(id).character.sheet();
    check(next.level == 5 && next.level_classes.back() == "fighter" &&
              next.saving_throws[Ability::strength] ==
                  next.modifiers[Ability::strength] + 3,
          "Fighter 4 / Rogue 1 has the Proficiency Bonus of level 5");
}

// A character of the class, every score 15.
Character starting(std::string_view klass)
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = std::string(klass);
    d.background = "sage";
    d.alignment = "neutral_good";
    d.name = "Caster";
    d.rolled = true;
    for (auto &r : d.rolls)
    {
        r = {{5, 5, 5, 1}, 3};
    }
    auto creation = srd5::character_rules();
    // A group's options can follow another's picks (Expertise), so each
    // group is filled once, after those before it.
    for (const auto &first : creation->training_options(d))
    {
        const auto groups = creation->training_options(d);
        const auto group = std::find_if(groups.begin(), groups.end(),
                                        [&](const auto &g)
                                        {
                                            return g.id == first.id;
                                        });
        auto &picked = d.training[group->id];
        for (const auto &option : group->options)
        {
            if (picked.size() < group->count &&
                std::find(picked.begin(), picked.end(), option.id) == picked.end())
            {
                picked.push_back(option.id);
            }
        }
    }
    return Character(*creation, d, {});
}

// A level taken in `klass`: its training and spell choices, the first of each
// offered, its prepared spells as many as it may.
AdvancementChoice take_level(const CampaignParty &party, MemberId id, std::string_view klass)
{
    const auto &rules = party.rule_module();
    const auto &sheet = party.member(id).character.sheet();
    AdvancementChoice choice;
    choice.character_class = std::string(klass);
    for (int pass = 0; pass < 2; ++pass)
    {
        for (const auto &group : party.advancement_options(id, choice).training)
        {
            auto &picked = choice.training[group.id];
            picked.clear();
            for (const auto &option : group.options)
            {
                if (picked.size() < group.count)
                {
                    picked.push_back(option.id);
                }
            }
        }
    }
    auto next = rules.spell_choice_sheet(sheet, choice);
    const auto learning = rules.spell_choice_options(next, SpellChoiceContext::advancement);
    choice.spell_learning.emplace();
    for (const auto &group : learning.learning)
    {
        auto &picked = (*choice.spell_learning)[group.id];
        for (const auto &option : group.options)
        {
            if (picked.size() < group.count)
            {
                picked.push_back(option.id);
            }
        }
    }
    rules.apply_spell_choices(next, SpellChoices{*choice.spell_learning, {}, {}, {}},
                              SpellChoiceContext::advancement, ChoiceCompleteness::partial);
    const auto preparation = rules.spell_choice_options(next, SpellChoiceContext::advancement);
    choice.spells = preparation.locked_prepared;
    for (const auto &spell : preparation.preparation)
    {
        if (choice.spells.size() < preparation.prepared_count &&
            std::find(choice.spells.begin(), choice.spells.end(), spell.id) == choice.spells.end())
        {
            choice.spells.push_back(spell.id);
        }
    }
    return choice;
}

unsigned slots(const CampaignParty &party, MemberId id, unsigned level)
{
    const auto info = party.rule_module().recovery_info(party.member(id).character.sheet(),
                                                        party.member(id).vitals);
    for (const auto &pool : info.resources)
    {
        if (pool.id == "spell_slot:" + std::to_string(level))
        {
            return pool.capacity;
        }
    }
    return 0;
}

// Fighter 3 / Wizard 1: a Wizard's spellbook, cantrips and preparation at its
// level 1, and its own slots.
void fighter_wizard()
{
    CampaignParty party(module());
    const auto id = fighter_three(party);
    party.advance(id, take_level(party, id, "wizard"));
    const auto &sheet = party.member(id).character.sheet();
    const auto access = party.rule_module().spell_access(sheet);
    check(access.cantrips.size() == 3 && access.spellbook.size() == 6 &&
              access.prepared.size() == 4,
          "Wizard 1: three cantrips, six spells in the book, four prepared");
    check(std::all_of(sheet.prepared_spells.begin(), sheet.prepared_spells.end(),
                      [](const auto &entry)
                      {
                          return entry.ends_with("@wizard");
                      }),
          "A later class's prepared spells name it");
    check(slots(party, id, 1) == 2 && slots(party, id, 2) == 0,
          "A single Spellcasting class keeps its own table: two level-1 slots");
}

// Cleric 1 / Wizard 2: the Multiclass Spellcaster table at level 3 (four
// level-1 and two level-2 slots), each class preparing its own spells, and a
// Long Rest preparing for each.
void cleric_wizard()
{
    CampaignParty party(module());
    const auto id = party.add_pc(starting("cleric"));
    party.award_experience(900, "caster-xp");
    party.advance(id, take_level(party, id, "wizard"));
    party.advance(id, take_level(party, id, "wizard"));
    const auto &sheet = party.member(id).character.sheet();
    check(sheet.level_classes == std::vector<std::string>{"cleric", "wizard", "wizard"},
          "Cleric 1 / Wizard 2");
    check(slots(party, id, 1) == 4 && slots(party, id, 2) == 2,
          "Combined slots follow the Multiclass Spellcaster table");
    const auto options =
        party.rule_module().spell_choice_options(sheet, SpellChoiceContext::long_rest);
    check(options.class_preparation.size() == 2 &&
              options.class_preparation[0].character_class == "cleric" &&
              options.class_preparation[0].tag.empty() &&
              options.class_preparation[1].tag == "@wizard",
          "A Long Rest prepares for each spellcasting class");
    // Prepare the Cleric's first spells and keep the Wizard's.
    std::vector<std::string> prepared;
    const auto &cleric = options.class_preparation[0];
    for (const auto &spell : cleric.preparation)
    {
        if (prepared.size() < cleric.count)
        {
            prepared.push_back(spell.id);
        }
    }
    for (const auto &entry : sheet.prepared_spells)
    {
        if (entry.ends_with("@wizard"))
        {
            prepared.push_back(entry);
        }
    }
    auto rested = sheet;
    party.rule_module().apply_spell_choices(rested, SpellChoices{{}, prepared, {}, {}},
                                            SpellChoiceContext::long_rest,
                                            ChoiceCompleteness::complete);
    check(rested.prepared_spells == prepared, "Each class's preparation is kept");
    auto too_many = prepared;
    too_many.push_back(cleric.preparation.back().id);
    auto refused = sheet;
    bool rejected = false;
    try
    {
        party.rule_module().apply_spell_choices(refused, SpellChoices{{}, too_many, {}, {}},
                                                SpellChoiceContext::long_rest,
                                                ChoiceCompleteness::complete);
    }
    catch (const std::exception &)
    {
        rejected = true;
    }
    check(rejected, "A class prepares no more than its own count");
}

// Paladin 1 / Sorcerer 2: Lay On Hands and Sorcery Points are each class's
// own pool, full at once, not one shared count.
void own_pools()
{
    CampaignParty party(module());
    const auto id = party.add_pc(starting("paladin"));
    party.award_experience(900, "pool-xp");
    party.advance(id, take_level(party, id, "sorcerer"));
    party.advance(id, take_level(party, id, "sorcerer"));
    const auto info = party.rule_module().recovery_info(party.member(id).character.sheet(),
                                                        party.member(id).vitals);
    const auto pool = [&](std::string_view name) -> std::pair<unsigned, unsigned>
    {
        for (const auto &p : info.resources)
        {
            if (p.id == name)
            {
                return {p.remaining, p.capacity};
            }
        }
        return {0, 0};
    };
    check(pool("lay_on_hands") == std::pair{5u, 5u} && pool("sorcery_points") == std::pair{2u, 2u},
          "Lay On Hands (5) and Sorcery Points (2) are separate, both full");
}

// A save keeps the class of each level.
void saved()
{
    CampaignParty party(module());
    const auto id = fighter_three(party);
    party.advance(id, rogue_entry(party, id));
    const auto rules = module();
    CampaignParty restored(module());
    restored.restore(decode_campaign(encode_campaign(party, nullptr, "multiclass"),
                                     *srd5::character_rules(), *rules, "multiclass", nullptr)
                         .party);
    check(restored.member(id).character.sheet().level_classes ==
              party.member(id).character.sheet().level_classes &&
              restored.member(id).character.sheet().hit_points ==
                  party.member(id).character.sheet().hit_points,
          "A save keeps each level's class");
}
} // namespace

int main()
{
    try
    {
        offered_classes();
        prerequisites();
        fighter_rogue();
        fighter_wizard();
        cleric_wizard();
        own_pools();
        saved();
        std::cout << "Multiclass tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
