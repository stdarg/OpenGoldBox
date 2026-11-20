// Creation in the two Buck Rogers games through the creation pages every
// game shares (docs/CHARACTER-CREATION.md, "The Buck Rogers games"): the
// pages in the original's order (chargen.pas CreateCharacterMenu), the roll
// as a record, the skill screen (skills.pas EditSkills), a Warrior's
// specializations (viewchar.pas SpecializationMenu), the body icon, and a
// made character saved and loaded through its record. The made-up checks run
// everywhere; the save also needs Countdown's files.
#include "opengold/buck_rogers_characters.h"
#include "opengold/buck_rogers_creation.h"
#include "opengold/campaign_save.h"
#include "opengold/character_creator.h"
#include "opengold/original.h"
#include "opengold/original_content.h"
#include "installed_games.h"
#include "test_support.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace
{
using opengold::CharacterCreator;
using opengold::CreationStep;
using opengold::GoldBoxGame;
using opengold::rules::CreationField;
using opengold::test::check;
using opengold::test::throws;
namespace og = opengold::original;
using og::Skill;

constexpr auto countdown = GoldBoxGame::countdown_to_doomsday;
constexpr auto matrix = GoldBoxGame::matrix_cubed;

std::shared_ptr<const opengold::rules::CharacterRules> rules_of(GoldBoxGame game)
{
    return og::character_rules(og::default_aging, game);
}

// The first pages filled in as a player fills them.
CharacterCreator chosen(GoldBoxGame game, const std::string &race, const std::string &career,
                        const std::string &name = "Mira Kade")
{
    CharacterCreator creator(rules_of(game), 42);
    creator.name(name);
    creator.select(CreationField::race, race);
    creator.select(CreationField::gender, "female");
    creator.next();
    creator.select(CreationField::character_class, career);
    creator.next();
    creator.roll();
    return creator;
}

og::GroundRecord record_of(const CharacterCreator &creator)
{
    const auto record = og::buck_rogers_record_of(creator.draft().rules_data);
    check(record.has_value(), "A rolled draft holds its record");
    return *record;
}

void the_menus_list_the_original_choices()
{
    const auto rules = rules_of(countdown);
    const auto races = rules->choices(CreationField::race);
    check(races.size() == 6 && races.front().id == "terran" && races.back().id == "desert_runner",
          "Creation lists the six races (CreateCharacterMenu)");
    const auto careers = rules->choices(CreationField::character_class);
    check(careers.size() == 5 && careers.front().id == "rocket_jock" &&
              careers.back().id == "rogue",
          "Creation lists the five careers, no Scout");
    check(rules->choices(CreationField::alignment).empty() &&
              rules->choices(CreationField::background).empty(),
          "No alignment or background");
    opengold::rules::CharacterDraft draft;
    draft.race = "desert_runner";
    const auto allows = [&](const char *career)
    {
        return rules->allows(draft, CreationField::character_class, career);
    };
    check(allows("rocket_jock") && allows("warrior") && allows("engineer") && !allows("medic") &&
              !allows("rogue"),
          "A Desert Runner may be a Rocketjock, a Warrior or an Engineer (raceCareers)");
    draft.race = "tinker";
    check(allows("medic") && allows("engineer") && !allows("warrior"),
          "A Tinker may be a Medic or an Engineer");
}

void the_pages_follow_the_original()
{
    auto engineer = chosen(countdown, "terran", "engineer");
    check(engineer.pages() == std::vector{CreationStep::race, CreationStep::character_class,
                                          CreationStep::attributes, CreationStep::skills,
                                          CreationStep::combat_icon, CreationStep::sheet},
          "An Engineer's pages: name and race, career, roll, skills, icon, sheet");
    auto warrior = chosen(countdown, "desert_runner", "warrior");
    check(warrior.pages() == std::vector{CreationStep::race, CreationStep::character_class,
                                         CreationStep::attributes, CreationStep::specialization,
                                         CreationStep::skills, CreationStep::combat_icon,
                                         CreationStep::sheet},
          "A Warrior chooses a specialization before the skills (SpecializationMenu)");
}

void the_roll_is_the_record()
{
    auto creator = chosen(countdown, "desert_runner", "engineer");
    auto record = record_of(creator);
    const auto &c = record.creature;
    check(c.name == "Mira Kade" && c.level == 2 && c.credits == 1000 && c.experience == 1250 &&
              c.gender == 1,
          "The roll makes a level-2 Engineer with 1,250 experience and 1,000 credits, named");
    const auto minimums = og::career_ability_minimums(og::Profession::engineer);
    for (std::size_t n = 0; n < og::buck_rogers_ability_count; ++n)
    {
        check(c.abilities[n] >= minimums[n], "Each score reaches the career's minimum");
    }
    check(c.icon == 16 || c.icon == 23 || c.icon == 24 || c.icon == 31,
          "A Desert Runner starts with one of its four icons (SetDefaultIcon)");
    check(creator.sheet().scores[opengold::rules::Ability::strength] == c.current_abilities[0],
          "The sheet's scores are the record's");
    const int age = c.age;
    auto changed = record;
    changed.creature.skills[0] = 7;
    creator.change_record(og::buck_rogers_draft(changed).rules_data);
    creator.roll();
    check(record_of(creator).creature.age == age && record_of(creator).creature.skills[0] == 0,
          "Rolling again keeps the age and starts the skills over");
    creator.back();
    creator.back();
    creator.name("Ava Rook");
    check(record_of(creator).creature.name == "Ava Rook", "A new name goes into the record");
    check(throws(
              [&]
              {
                  creator.name("A name too long here");
              }),
          "A name past the record's 15 letters is refused");
    creator.select(CreationField::gender, "male");
    check(!creator.draft().rolled, "Another gender rolls again: the record holds it");
}

void countdown_spends_skill_points()
{
    auto creator = chosen(countdown, "terran", "engineer");
    creator.next();
    check(creator.step() == CreationStep::skills, "The skill screen follows the roll");
    auto record = record_of(creator);
    auto spending = og::SkillSpending::at_creation(record.creature, countdown);
    check(spending.add(Skill::jury_rig) == og::SkillRefusal::prerequisites_unmet,
          "Jury Rig is locked until Repair Electrical and Mechanical have 10");
    for (int n = 0; n < 10; ++n)
    {
        (void)spending.add(Skill::repair_electrical);
        (void)spending.add(Skill::repair_mechanical);
    }
    check(spending.add(Skill::jury_rig) == og::SkillRefusal::none, "Then Jury Rig unlocks");
    while (spending.add(Skill::repair_mechanical) == og::SkillRefusal::none)
    {
    }
    check(og::level_of(spending.levels(), Skill::repair_mechanical) == 30 &&
              spending.add(Skill::repair_mechanical) == og::SkillRefusal::at_level_maximum,
          "A skill stops at 30 at creation");
    spending.apply_to(record.creature);
    creator.change_record(og::buck_rogers_draft(record).rules_data);
    creator.next();
    creator.next();
    const auto made = creator.create_character();
    const auto kept = og::buck_rogers_record_of(made.sheet().original->game_record);
    check(kept && kept->creature.skills == record.creature.skills,
          "The made character keeps the skills spent");
    check(og::skill_prerequisites(Skill::jury_rig).needs.size() == 2 &&
              og::skill_prerequisites(Skill::composition).any,
          "The screen lists what a locked skill needs");
}

void a_warrior_specializes()
{
    auto creator = chosen(matrix, "terran", "warrior");
    creator.next();
    check(creator.step() == CreationStep::specialization, "The Warrior's page follows the roll");
    check(throws(
              [&]
              {
                  creator.next();
              }),
          "It waits for the specializations");
    auto record = record_of(creator);
    check(og::specialize(record.creature, 4) && og::specialize(record.creature, 4) &&
              og::specialize(record.creature, 9),
          "Three specializations");
    check(og::unspecialize(record.creature, 4) &&
              og::specialization_level(record.creature, 4) == 1 &&
              record.creature.specializations[0] == 4 && record.creature.specializations[1] == 9,
          "One taken back leaves the others in the first slots");
    (void)og::specialize(record.creature, 2);
    creator.change_record(og::buck_rogers_draft(record).rules_data);
    creator.next();
    check(creator.step() == CreationStep::skills, "Three chosen, the skills follow");
}

void matrix_cubed_moves_preset_points()
{
    auto creator = chosen(matrix, "terran", "engineer");
    auto record = record_of(creator);
    check(record.creature.level == 7 && record.creature.experience == 40000,
          "Matrix Cubed starts at level 7");
    auto spending = og::SkillSpending::at_creation(record.creature, matrix);
    check(spending.career_points() == 0 && spending.general_points() == 0, "No points to spend");
    const int jury = og::level_of(spending.levels(), Skill::jury_rig);
    check(spending.add(Skill::jury_rig) == og::SkillRefusal::no_points,
          "Points must be freed first");
    check(spending.subtract(Skill::notice) &&
              spending.add(Skill::jury_rig) == og::SkillRefusal::none,
          "A point moves from one career skill to another");
    check(og::level_of(spending.levels(), Skill::jury_rig) == jury + 1, "Jury Rig rose one");
}

// A made character joins a party with its two free items and comes back
// from a save as it was (the record code of buck_rogers_characters.h).
void a_made_character_saves_and_loads(const std::filesystem::path &folder)
{
    auto creator = chosen(countdown, "mercurian", "rogue");
    creator.next();
    creator.next();
    creator.next();
    const auto creation = rules_of(countdown);
    auto party = std::make_shared<opengold::CampaignParty>(
        og::rules_module(opengold::load_original_content(folder, countdown)), creation);
    const auto id = party->add_pc(creator.create_character());
    opengold::outfit_buck_rogers_character(*party, id,
                                           opengold::load_buck_rogers_stock(folder, countdown));
    const auto record = og::buck_rogers_record_of(party->game_record(id));
    check(record && record->items.size() == 2, "The new character carries its two free items");
    const auto assets = opengold::campaign_asset_identity(folder);
    const auto saved = opengold::encode_campaign(*party, nullptr, assets, countdown);
    const auto restored =
        opengold::decode_campaign(saved, *creation, party->rule_module(), assets, nullptr);
    auto loaded = std::make_shared<opengold::CampaignParty>(
        og::rules_module(opengold::load_original_content(folder, countdown)), creation);
    loaded->restore(restored.party);
    const auto &member = loaded->state().roster.front();
    check(loaded->game_record(member.id) == party->game_record(id),
          "The record comes back byte for byte");
}
} // namespace

int main()
{
    try
    {
        the_menus_list_the_original_choices();
        the_pages_follow_the_original();
        the_roll_is_the_record();
        countdown_spends_skill_points();
        a_warrior_specializes();
        matrix_cubed_moves_preset_points();
        if (const auto folder = opengold::test::installed_game_folder(countdown); !folder.empty())
        {
            a_made_character_saves_and_loads(folder);
        }
        else
        {
            std::cout << "Countdown to Doomsday's files are not here: the save is not checked.\n";
        }
        std::cout << "Buck Rogers creation passed.\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
