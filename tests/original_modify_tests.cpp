#include "opengold/campaign_party.h"
#include "opengold/campaign_save.h"
#include "opengold/character_creator.h"
#include "opengold/original.h"
#include "opengold/original_combat.h"
#include "opengold/srd5.h"
#include "original_creation_support.h"
#include "test_support.h"
#include <iostream>

// Modify Character under the original rules (docs/ORIGINAL-RULES.md, Modify
// Character): each game's chargen.pas ModifyCharacterMenu, its HpMinimum and
// HpMaximum (Pools of Darkness's, Treasures' and the kit's HpMaximum and
// HitPointCap) and its Keep.
using namespace opengold;
using opengold::test::check;
using opengold::test::throws;
using original::ClassSlot;
using original::Gender;
using original::ModifiedCharacter;
using original::ModifiedNumbers;
using original::Profession;
using original::Race;
using rules::Ability;
using rules::CreationField;
using rules::ModifyRow;
using rules::ModifyStep;

namespace
{
constexpr auto pool = GoldBoxGame::pool_of_radiance;
constexpr auto curse = GoldBoxGame::curse_of_the_azure_bonds;
constexpr auto secret = GoldBoxGame::secret_of_the_silver_blades;
constexpr auto gateway = GoldBoxGame::gateway_to_the_savage_frontier;
constexpr auto darkness = GoldBoxGame::pools_of_darkness;
constexpr auto treasures = GoldBoxGame::treasures_of_the_savage_frontier;
constexpr auto kit = GoldBoxGame::unlimited_adventures;

ModifiedCharacter human(Profession profession, original::ClassLevels levels)
{
    return {Race::human, Gender::male, profession, levels};
}

original::ClassLevels levels_in(ClassSlot slot, int level, ClassSlot other = ClassSlot::monk,
                                int other_level = 0)
{
    original::ClassLevels levels{};
    original::level_in(levels, slot) = level;
    if (other_level > 0)
    {
        original::level_in(levels, other) = other_level;
    }
    return levels;
}

// ModifyCharacterMenu's refusal: "can't be modified" once the experience is
// not what its game's creation gives.
void each_game_stops_at_its_first_experience()
{
    check(original::modify_allowed(0, pool) && !original::modify_allowed(1, pool),
          "Pool of Radiance modifies a character with no experience only");
    check(original::modify_allowed(25000, curse) && original::modify_allowed(12500, curse) &&
              original::modify_allowed(8333, curse) && original::modify_allowed(0, curse) &&
              !original::modify_allowed(25001, curse),
          "Curse modifies one with its starting 25,000, 12,500 or 8,333, or none");
    check(original::modify_allowed(200000, secret) && original::modify_allowed(66667, secret) &&
              !original::modify_allowed(200010, secret),
          "Secret's are 200,000, 100,000 and 66,667");
    check(original::modify_allowed(3000, gateway) && original::modify_allowed(1000, gateway) &&
              !original::modify_allowed(3100, gateway),
          "Gateway's are 3,000, 1,500 and 1,000");
    check(original::modify_allowed(1500001, darkness) &&
              original::modify_allowed(500000, darkness) && !original::modify_allowed(0, darkness),
          "Pools of Darkness's are 1,500,001, 750,000 and 500,000, and never none");
    check(original::modify_allowed(22600, treasures) && original::modify_allowed(7533, treasures) &&
              !original::modify_allowed(22601, treasures),
          "Treasures' are 22,600, 11,300 and 7,533");
    check(original::modify_allowed(50000, kit, 50000) &&
              original::modify_allowed(25000, kit, 50000) &&
              original::modify_allowed(16666, kit, 50000) &&
              !original::modify_allowed(40001, kit, 50000),
          "A design's (modify.c): its starting experience, a half or a third");
}

// Pool of Radiance's HpMinimum and HpMaximum: a point a level and the dice's
// highest, the Constitution's bonus for one level, divided among the classes.
void pool_hit_point_limits()
{
    const auto fighter = human(Profession::fighter, levels_in(ClassSlot::fighter, 1));
    check(original::modify_hit_point_minimum(fighter, 16, pool) == 3 &&
              original::modify_hit_point_maximum(fighter, 16, pool) == 12,
          "A fighter of Constitution 16 has 3 to 12");
    check(original::modify_hit_point_minimum(fighter, 18, pool) == 5 &&
              original::modify_hit_point_maximum(fighter, 18, pool) == 14,
          "A single fighter gets 2 more at Constitution 18");
    const auto fighter_thief =
        human(Profession::fighter_thief, levels_in(ClassSlot::fighter, 1, ClassSlot::thief, 1));
    check(original::modify_hit_point_minimum(fighter_thief, 15, pool) == 2 &&
              original::modify_hit_point_maximum(fighter_thief, 15, pool) == 9,
          "A fighter/thief divides its dice and both classes' bonus by two");
    const auto magic_user = human(Profession::magic_user, levels_in(ClassSlot::magic_user, 1));
    check(original::modify_hit_point_minimum(magic_user, 3, pool) == 1 &&
              original::modify_hit_point_maximum(magic_user, 3, pool) == 2,
          "A weak magic-user keeps at least 1");
}

// Curse's and Gateway's HpMaximum: each class's die and ClassConAdj at every
// level, and past its Hit Dice a fixed amount, added to the other classes'
// (the original put it in place of them).
void curse_hit_point_limits()
{
    const auto fighter = human(Profession::fighter, levels_in(ClassSlot::fighter, 5));
    for (const auto game : {curse, gateway})
    {
        check(original::modify_hit_point_maximum(fighter, 17, game) == 65 &&
                  original::modify_hit_point_minimum(fighter, 17, game) == 8,
              "A level 5 fighter of Constitution 17 has 8 to 13 a level");
    }
    const auto ranger = human(Profession::ranger, levels_in(ClassSlot::ranger, 1));
    check(original::modify_hit_point_maximum(ranger, 16, curse) == 20 &&
              original::modify_hit_point_minimum(ranger, 16, curse) == 2 + 2 * 2,
          "A first-level ranger counts two dice, and its bonus twice");
    const auto veteran = human(Profession::fighter_magic_user,
                               levels_in(ClassSlot::fighter, 12, ClassSlot::magic_user, 13));
    check(original::modify_hit_point_maximum(veteran, 12, curse) == (3 * 3 + 90 + 2 + 44) / 2,
          "Past their Hit Dice both classes' fixed amounts count");
}

// Secret's: each class with its own bonus, and 2 a level past a fighter's
// Hit Dice.
void secret_hit_point_limits()
{
    const auto fighter = human(Profession::fighter, levels_in(ClassSlot::fighter, 8));
    check(original::modify_hit_point_maximum(fighter, 18, secret) == 14 * 8 &&
              original::modify_hit_point_minimum(fighter, 18, secret) == 8 + 4,
          "A level 8 fighter of Constitution 18 has 12 to 112");
    const auto old_hand = human(Profession::fighter, levels_in(ClassSlot::fighter, 12));
    check(original::modify_hit_point_maximum(old_hand, 18, secret) == 3 * 2 + 88,
          "Past its Hit Dice a fighter adds 2 a level");
}

// Pools of Darkness's HpMaximum (the lowest) counts the bonus at every level
// when it is above 0; its HitPointCap (the highest) each die with its
// ClassConAdj and the fixed amounts past them.
void darkness_hit_point_limits()
{
    const auto fighter = human(Profession::fighter, levels_in(ClassSlot::fighter, 14));
    for (const auto game : {darkness, treasures, kit})
    {
        check(original::modify_hit_point_minimum(fighter, 18, game) == 5 * 3 + 9 + 4 * 9 &&
                  original::modify_hit_point_maximum(fighter, 18, game) == 14 * 9 + 5 * 3,
              "A level 14 fighter of Constitution 18 has 60 to 141");
    }
    const auto magic_user = human(Profession::magic_user, levels_in(ClassSlot::magic_user, 14));
    check(original::modify_hit_point_minimum(magic_user, 5, darkness) == 3 + 11,
          "A negative bonus leaves the lowest as it is");
    const auto ranger = human(Profession::ranger, levels_in(ClassSlot::ranger, 13));
    check(original::modify_hit_point_maximum(ranger, 16, darkness) == 10 * 10 + 10 + 3 * 2,
          "A ranger's extra die counts past its Hit Dice");
}

// The rows' steps: a score within its race's limits and its class's minimum,
// exceptional Strength a point at a time, Constitution pulling the Hit Points
// within their new limits.
void scores_step_within_their_limits()
{
    const auto fighter = human(Profession::fighter, levels_in(ClassSlot::fighter, 1));
    ModifiedNumbers numbers{{18, 12, 16, 10, 10, 10}, 0, 10};
    auto raised =
        original::modify_step(numbers, fighter, ModifyRow::strength, ModifyStep::raise, pool);
    check(raised.scores[Ability::strength] == 18 && raised.exceptional_strength == 1,
          "A fighter's 18 raises its exceptional Strength");
    raised.exceptional_strength = 100;
    check(original::modify_step(raised, fighter, ModifyRow::strength, ModifyStep::raise, pool) ==
              raised,
          "18/00 is a human's highest");
    auto lowered =
        original::modify_step(raised, fighter, ModifyRow::strength, ModifyStep::lower, pool);
    check(lowered.scores[Ability::strength] == 18 && lowered.exceptional_strength == 99,
          "Lowering takes the exceptional Strength first");
    lowered.exceptional_strength = 0;
    check(original::modify_step(lowered, fighter, ModifyRow::strength, ModifyStep::lower, pool)
                  .scores[Ability::strength] == 17,
          "then the score");
    const auto thief = human(Profession::thief, levels_in(ClassSlot::thief, 1));
    check(original::modify_step(numbers, thief, ModifyRow::strength, ModifyStep::raise, pool) ==
              numbers,
          "A thief's 18 has no exceptional Strength");
    const auto paladin = human(Profession::paladin, levels_in(ClassSlot::paladin, 1));
    check(original::modify_step(numbers, paladin, ModifyRow::strength, ModifyStep::raise, curse)
                  .exceptional_strength == 1,
          "From Curse on a paladin's does");
    ModifiedNumbers weak{{9, 12, 16, 10, 10, 10}, 0, 10};
    check(original::modify_step(weak, fighter, ModifyRow::strength, ModifyStep::lower, pool) ==
              weak,
          "A fighter's Strength stops at its class's 9 (Pool of Radiance's went on to 3)");
    ModifiedNumbers tough{{12, 12, 16, 10, 10, 10}, 0, 12};
    const auto weaker =
        original::modify_step(tough, fighter, ModifyRow::constitution, ModifyStep::lower, pool);
    check(weaker.scores[Ability::constitution] == 15 && weaker.hit_points == 11,
          "Lowering Constitution brings the Hit Points down to its new highest");
    ModifiedNumbers frail{{12, 12, 16, 10, 10, 10}, 0, 3};
    const auto healthier =
        original::modify_step(frail, fighter, ModifyRow::constitution, ModifyStep::raise, pool);
    check(healthier.scores[Ability::constitution] == 17 && healthier.hit_points == 4,
          "Raising it brings them up to its new lowest");
    check(original::modify_step(tough, fighter, ModifyRow::hit_points, ModifyStep::raise, pool) ==
                  tough &&
              original::modify_step(frail, fighter, ModifyRow::hit_points, ModifyStep::lower,
                                    pool) == frail,
          "The Hit Points stop at the dice's highest and lowest");
    ModifiedNumbers above{{12, 12, 16, 10, 10, 10}, 0, 20};
    const auto lowered_once =
        original::modify_step(above, fighter, ModifyRow::hit_points, ModifyStep::lower, pool);
    check(original::modify_step(above, fighter, ModifyRow::hit_points, ModifyStep::raise, pool) ==
                  above &&
              lowered_once.hit_points == 19,
          "Hit Points above the highest never fall on a raise, and fall a point on a lower");
}

// Keep's hpRolled: Pool of Radiance keeps the dice's; from Curse on the
// maximum less the Constitution's share.
void keep_works_out_the_rolled_hit_points()
{
    const auto fighter = human(Profession::fighter, levels_in(ClassSlot::fighter, 5));
    check(original::modified_rolled_hit_points(fighter, 17, 50, 41, pool) == 41,
          "Pool of Radiance keeps the rolled Hit Points");
    check(original::modified_rolled_hit_points(fighter, 17, 50, 41, curse) == 50 - 3 * 5,
          "Curse takes its Constitution's 3 a level off");
}

rules::CharacterDraft rolled_fighter(const rules::CharacterRules &rules)
{
    rules::CharacterDraft draft;
    draft.race = "human";
    draft.gender = "male";
    draft.character_class = "fighter";
    draft.alignment = "neutral_good";
    draft.name = "Gareth";
    RandomState random{5};
    rules.roll_in_place(draft, random);
    return draft;
}

// A draft's Modify through the rules interface: each step a new draft, its
// sheet showing the change, an unchanged draft at a limit, and a draft never
// modified as it was.
void a_draft_is_modified_a_step_at_a_time()
{
    const auto rules = original::character_rules();
    const auto draft = rolled_fighter(*rules);
    const auto before = rules->evaluate(draft, rules::NameRequirement::required);
    check(draft.rules_data.find("modified") == std::string::npos,
          "A rolled draft has no Modify numbers");
    auto changed = draft;
    for (int step = 0; step < 30; ++step)
    {
        changed = rules->modified(changed, ModifyRow::hit_points, ModifyStep::raise);
    }
    const auto after = rules->evaluate(changed, rules::NameRequirement::required);
    const ModifiedCharacter character{Race::human, Gender::male, Profession::fighter,
                                      after.original->class_levels};
    check(after.hit_points == original::modify_hit_point_maximum(
                                  character, after.scores[Ability::constitution], pool) &&
              after.scores == before.scores,
          "The Hit Points rise to the dice's highest and the scores stay");
    check(rules->modified(changed, ModifyRow::hit_points, ModifyStep::raise).rules_data ==
              changed.rules_data,
          "At the highest the draft is unchanged");
    const auto charmed = rules->modified(changed, ModifyRow::charisma, ModifyStep::raise);
    check(rules->evaluate(charmed, rules::NameRequirement::required).scores[Ability::charisma] ==
              std::min(after.scores[Ability::charisma] + 1, 18),
          "Charisma rises a point");
    check(rules->modifiable(changed, 0) && !rules->modifiable(changed, 10),
          "It can be modified until it has experience");
    check(!rules->modify_renames() &&
              original::character_rules(original::default_aging, curse)->modify_renames(),
          "From Curse on Modify changes the name too");
    const auto srd = srd5::character_rules();
    check(!srd->modifiable(draft, 0) && throws(
                                            [&]
                                            {
                                                (void)srd->modified(draft, ModifyRow::strength,
                                                                    ModifyStep::raise);
                                            }),
          "The SRD's creation has no Modify");
}

// A member of the party, modified on the party screen as the original's
// party menu does, keeps the change in a save; its first experience ends it.
void a_party_member_is_modified_and_saved()
{
    std::shared_ptr<const rules::CharacterRules> creation = original::character_rules();
    const auto module = original::rules_module();
    CampaignParty party(original::rules_module(), creation);
    const auto id = party.add_pc(Character(*creation, rolled_fighter(*creation), {}));
    check(party.modifiable(id), "A new member can be modified");
    auto draft = party.member(id).character.creation_data();
    draft = creation->modified(draft, ModifyRow::wisdom, ModifyStep::lower);
    draft = creation->modified(draft, ModifyRow::hit_points, ModifyStep::lower);
    const auto expected = creation->evaluate(draft, rules::NameRequirement::required);
    auto renamed = draft;
    renamed.character_class = "thief";
    check(throws(
              [&]
              {
                  party.modify(id, renamed);
              }),
          "Modify changes no class");
    party.modify(id, draft);
    const auto &member = party.member(id);
    check(member.character.sheet().scores == expected.scores &&
              member.character.sheet().hit_points == expected.hit_points &&
              member.vitals.hit_points == expected.hit_points,
          "The member takes the scores and Hit Points, at full Hit Points");
    const auto loaded = decode_campaign(encode_campaign(party, nullptr, "modify-check"), *creation,
                                        *module, "modify-check", nullptr);
    check(loaded.party.roster.front().character.sheet().scores == expected.scores &&
              loaded.party.roster.front().character.sheet().hit_points == expected.hit_points,
          "A save keeps the change");
    party.award_experience(10, "test:modify");
    check(!party.modifiable(id) && throws(
                                       [&]
                                       {
                                           party.modify(id, draft);
                                       }),
          "Its first experience ends Modify");
}

// Curse's character starts trained; Modify keeps its levels and works out
// its Hit Points rolled as Keep does.
void a_curse_character_keeps_its_training()
{
    const auto rules = original::character_rules(original::default_aging, curse);
    auto draft = rolled_fighter(*rules);
    const auto before = rules->evaluate(draft, rules::NameRequirement::required);
    for (int step = 0; step < 3; ++step)
    {
        draft = rules->modified(draft, ModifyRow::intelligence, ModifyStep::raise);
        draft = rules->modified(draft, ModifyRow::hit_points, ModifyStep::lower);
    }
    const auto after = rules->evaluate(draft, rules::NameRequirement::required);
    const ModifiedCharacter character{Race::human, Gender::male, Profession::fighter,
                                      after.original->class_levels};
    check(after.original->class_levels == before.original->class_levels &&
              after.original->experience == 25000 && rules->modifiable(draft, 25000),
          "The levels and experience stay");
    check(after.original->rolled_hit_points ==
              original::modified_rolled_hit_points(character, after.scores[Ability::constitution],
                                                   after.hit_points, 0, curse),
          "The Hit Points rolled are Keep's");
}

// The kit offers Modify on its creation's roll as well as on its party menu
// (create.c), and a creator's draft can take it.
void a_new_character_is_modified_while_it_is_made()
{
    CharacterCreator creator(
        original::design_character_rules(original::default_aging, {50000, 200}), 9);
    creator.select(CreationField::race, "human");
    creator.select(CreationField::character_class, "fighter");
    creator.name("Ava");
    for (int page = 0; page < 3; ++page)
    {
        creator.next();
    }
    creator.roll();
    check(creator.rules().modifiable(creator.draft(),
                                     static_cast<unsigned>(creator.sheet().original->experience)),
          "The kit's new character can be modified on its roll");
    auto draft = creator.rules().modified(creator.draft(), ModifyRow::dexterity, ModifyStep::lower);
    creator.modify(draft);
    check(creator.draft().rules_data == draft.rules_data, "The creator takes the change");
    creator.roll();
    check(creator.draft().rules_data.find("modified") == std::string::npos,
          "Rolling again starts over");

    CharacterCreator pool_creator(original::character_rules(), 9);
    pool_creator.select(CreationField::race, "human");
    pool_creator.select(CreationField::character_class, "fighter");
    pool_creator.name("Ava");
    for (int page = 0; page < 3; ++page)
    {
        pool_creator.next();
    }
    pool_creator.roll();
    check(!pool_creator.rules().modify_while_made() &&
              throws(
                  [&]
                  {
                      pool_creator.modify(pool_creator.draft());
                  }),
          "Pool of Radiance offers Modify on its party menu only");
}
} // namespace

int main()
{
    try
    {
        each_game_stops_at_its_first_experience();
        pool_hit_point_limits();
        curse_hit_point_limits();
        secret_hit_point_limits();
        darkness_hit_point_limits();
        scores_step_within_their_limits();
        keep_works_out_the_rolled_hit_points();
        a_draft_is_modified_a_step_at_a_time();
        a_party_member_is_modified_and_saved();
        a_curse_character_keeps_its_training();
        a_new_character_is_modified_while_it_is_made();
    }
    catch (const std::exception &e)
    {
        std::cerr << "FAILED: " << e.what() << '\n';
        return 1;
    }
    std::cout << "Original Modify Character tests passed\n";
}
