#include "opengold/campaign_party.h"
#include "opengold/campaign_save.h"
#include "opengold/character.h"
#include "opengold/character_pool.h"
#include "opengold/srd5.h"
#include "opengold/training_hall.h"
#include "test_support.h"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace opengold;
using namespace opengold::rules;

// New Phlan's training halls (docs/TRAINING-HALL.md): a hall sells its classes
// one session of experience per level and teaches one skill or Expertise per
// level, all paid from the party's gold.
namespace
{
const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR);

using opengold::test::check;
using opengold::test::rejects;

std::unique_ptr<RulesModule> module()
{
    return srd5::load(root / "data/rules/srd-5.2.1/combat.rules");
}

// A level-one Fighter Acolyte: Acrobatics and History from the Fighter list,
// Insight and Religion from the background.
Character fighter()
{
    CharacterDraft d;
    d.race = "orc";
    d.gender = "female";
    d.character_class = "fighter";
    d.background = "acolyte";
    d.alignment = "neutral_good";
    d.name = "Trainee";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    d.training = {{"class:fighter", {"acrobatics", "history"}},
                  {"class:fighter:fighting_style", {"defense"}}};
    d.spells = SpellChoices{{{"feat:magic_initiate_cleric:ability", {"wisdom"}},
                             {"feat:magic_initiate_cleric:cantrips", {"sacred_flame", "guidance"}},
                             {"feat:magic_initiate_cleric:spell", {"guiding_bolt"}}},
                            {},
                            {},
                            {}};
    return Character(*srd5::character_rules(), d, {});
}

// A Character Pool character of the class proficient in `skill`.
Character pool_character(std::string_view klass, std::string_view skill)
{
    por::CharacterArt art;
    Image head;
    head.width = 88;
    head.height = 40;
    head.rgba.assign(88 * 40 * 4, 128);
    Image body;
    body.width = 88;
    body.height = 48;
    body.rgba.assign(88 * 48 * 4, 128);
    art.set_original_head(1, por::PortraitPart{"fixture", head});
    art.set_original_body(1, por::PortraitPart{"fixture", body});
    for (const auto &preset : character_pool(*srd5::character_rules(), art))
    {
        if (preset.creation_data().character_class != klass)
            continue;
        for (const auto &trained : preset.sheet().training.skills)
            if (trained.id == skill && trained.proficient)
                return preset;
    }
    throw std::runtime_error("No pool character has the skill");
}

const SkillTraining &skill(const CampaignParty &party, MemberId id, std::string_view skill_id)
{
    for (const auto &trained : party.member(id).character.sheet().training.skills)
        if (trained.id == skill_id)
            return trained;
    throw std::runtime_error("Unknown skill");
}

bool offers(const TrainingChoiceGroup &group, std::string_view id)
{
    return std::any_of(group.options.begin(), group.options.end(),
                       [&](const auto &option)
                       {
                           return option.id == id;
                       });
}

void halls_and_fees()
{
    check(training_hall_for_class("barbarian") == TrainingHall::fighters &&
              training_hall_for_class("druid") == TrainingHall::clerics &&
              training_hall_for_class("warlock") == TrainingHall::magic_users &&
              training_hall_for_class("monk") == TrainingHall::thieves &&
              !training_hall_for_class("Fighter"),
          "Each class trains at its AD&D group's hall, by class ID");
    check(hall_classes(TrainingHall::clerics) == std::vector<std::string_view>{"cleric", "druid"},
          "The Clerics' hall trains Clerics and Druids");
    check(training_hall_named("MAGIC USERS ") == TrainingHall::magic_users &&
              training_hall_named("THIEVES ") == TrainingHall::thieves &&
              !training_hall_named("BARDS "),
          "The original script names its hall");
    static_assert(hall_level_limit == 15, "The hall trains every class to level 15 (LEVELS-2)");
    check(experience_session_fee(1) == 1000 && experience_session_fee(3) == 4000 &&
              experience_session_fee(8) == 128000,
          "A session costs 1,000 gp at level 1 and doubles at each level");
    check(skill_training_fee("skill:stealth") == 250 &&
              skill_training_fee("expertise:stealth") == 500,
          "Proficiency costs 250 gp and Expertise 500 gp");
}

void experience_sessions()
{
    CampaignParty party(module());
    const auto id = party.add_pc(fighter());
    party.set_wealth({0, 0, 0, 1500, 0, 0, 0});
    auto offer = party.experience_offer(id, TrainingHall::fighters);
    check(offer.fee == 1000 && offer.experience == 150 && offer.denial == TrainingDenial::none,
          "A level-one session grants half of the 300 experience to level 2 for 1,000 gp");
    check(party.experience_offer(id, TrainingHall::thieves).denial == TrainingDenial::other_hall,
          "Another hall turns the Fighter away");
    rejects(
        [&]
        {
            party.buy_experience(id, TrainingHall::thieves);
        },
        "Another hall cannot sell the Fighter experience");
    party.buy_experience(id, TrainingHall::fighters);
    check(party.member(id).experience == 150 && party.state().wealth[3] == 500,
          "The session grants its experience and the party pays");
    check(party.experience_offer(id, TrainingHall::fighters).denial ==
              TrainingDenial::already_trained,
          "One session per level");
    rejects(
        [&]
        {
            party.buy_experience(id, TrainingHall::fighters);
        },
        "A second session at the same level is refused");
    check(party.member(id).experience == 150 && party.state().wealth[3] == 500,
          "A refused session changes nothing");
    check(!party.can_advance(id), "Gold alone does not gain a level");

    party.award_experience(150, "test:earned");
    check(party.experience_offer(id, TrainingHall::fighters).denial ==
              TrainingDenial::ready_to_advance,
          "A member with enough experience advances first");
    party.advance(id, party.default_advancement(id));
    offer = party.experience_offer(id, TrainingHall::fighters);
    check(offer.fee == 2000 && offer.experience == 300 && offer.denial == TrainingDenial::gold,
          "At level 2 the session costs 2,000 gp, more than the party has");
    party.set_wealth({0, 0, 0, 2000, 0, 0, 0});
    party.buy_experience(id, TrainingHall::fighters);
    check(party.member(id).experience == 600 && party.state().wealth[3] == 0,
          "The level-2 session grants half of the 600 between levels 2 and 3");

    party.set_wealth({0, 0, 0, 99999, 0, 0, 0});
    party.award_experience(165000, "test:veteran");
    while (party.can_advance(id))
        party.advance(id, party.default_advancement(id));
    check(party.experience_offer(id, TrainingHall::fighters).denial ==
              TrainingDenial::highest_level,
          "There is no session past the highest level");

    const auto reserve = party.add_pc(fighter());
    party.remove(reserve);
    check(party.experience_offer(reserve, TrainingHall::fighters).denial ==
              TrainingDenial::not_active,
          "Only active members train");
}

void skill_lessons()
{
    CampaignParty party(module());
    const auto id = party.add_pc(fighter());
    const auto lessons = party.skill_lessons(id, TrainingHall::fighters);
    check(offers(lessons, "skill:stealth") && offers(lessons, "skill:medicine") &&
              offers(lessons, "skill:nature"),
          "The Fighters' hall teaches the Ranger, Paladin and Barbarian skills too");
    check(!offers(lessons, "skill:arcana") && !offers(lessons, "skill:deception") &&
              !offers(lessons, "skill:acrobatics"),
          "It teaches neither other halls' skills nor one already held");
    check(offers(lessons, "expertise:acrobatics") && offers(lessons, "expertise:religion") &&
              !offers(lessons, "expertise:stealth"),
          "Expertise is taught only in a skill the character is proficient in");
    check(offers(party.skill_lessons(id, TrainingHall::thieves), "skill:arcana"),
          "The Thieves' hall teaches the Bard's whole list");
    check(party.skill_training_denial(id, TrainingHall::fighters) == TrainingDenial::gold,
          "An empty purse pays for no lesson");

    party.set_wealth({0, 0, 0, 400, 0, 0, 0});
    rejects(
        [&]
        {
            party.train_skill(id, TrainingHall::thieves, "skill:arcana");
        },
        "A Fighter does not train at the Thieves' hall");
    rejects(
        [&]
        {
            party.train_skill(id, TrainingHall::fighters, "skill:arcana");
        },
        "The Fighters' hall does not teach Arcana");
    const auto bonus = skill(party, id, "stealth").bonus;
    party.train_skill(id, TrainingHall::fighters, "skill:stealth");
    check(skill(party, id, "stealth").proficient &&
              skill(party, id, "stealth").bonus == bonus + 2 && party.state().wealth[3] == 150,
          "A lesson makes the Fighter proficient in Stealth for 250 gp");
    const auto &grants = party.member(id).character.sheet().grants;
    check(std::count(grants.begin(), grants.end(),
                     FeatureGrant{"skill:stealth", "training_hall", 1, {}}) == 1,
          "The hall is the proficiency's recorded source");
    check(party.skill_training_denial(id, TrainingHall::fighters) ==
              TrainingDenial::already_trained,
          "One lesson per level");
    rejects(
        [&]
        {
            party.train_skill(id, TrainingHall::fighters, "skill:medicine");
        },
        "A second lesson at the same level is refused");
    check(party.state().wealth[3] == 150, "A refused lesson costs nothing");

    party.award_experience(300, "test:level");
    party.advance(id, party.default_advancement(id));
    rejects(
        [&]
        {
            party.train_skill(id, TrainingHall::fighters, "expertise:stealth");
        },
        "Expertise costs 500 gp, more than the party has");
    party.set_wealth({0, 0, 0, 500, 0, 0, 0});
    party.train_skill(id, TrainingHall::fighters, "expertise:stealth");
    check(skill(party, id, "stealth").expertise && skill(party, id, "stealth").bonus == bonus + 4 &&
              party.state().wealth[3] == 0,
          "Expertise doubles the proficiency bonus");
    check(!offers(party.skill_lessons(id, TrainingHall::fighters), "expertise:stealth"),
          "Expertise is not taught twice");

    auto sheet = party.member(id).character.sheet();
    rejects(
        [&]
        {
            party.rule_module().train_at_hall(sheet, "skill:medicine");
        },
        "The rules allow one hall lesson per level");
}

// A Wizard's level-2 Scholar does not offer Expertise the hall already taught.
void scholar_after_hall()
{
    CampaignParty party(module());
    const auto id = party.add_pc(pool_character("wizard", "arcana"));
    party.set_wealth({0, 0, 0, 500, 0, 0, 0});
    party.train_skill(id, TrainingHall::magic_users, "expertise:arcana");
    party.award_experience(300, "test:level");
    const auto options = party.advancement_options(id);
    const auto scholar = std::find_if(options.training.begin(), options.training.end(),
                                      [](const auto &group)
                                      {
                                          return group.id == "class:wizard:scholar";
                                      });
    check(scholar != options.training.end() && !offers(*scholar, "arcana"),
          "Scholar leaves out the Expertise the hall taught");
}

void saved_training()
{
    const auto rules = module();
    CampaignParty party(module());
    const auto id = party.add_pc(fighter());
    party.set_wealth({0, 0, 0, 5000, 0, 0, 0});
    party.buy_experience(id, TrainingHall::fighters);
    party.train_skill(id, TrainingHall::fighters, "skill:stealth");
    party.award_experience(150, "test:earned");
    party.advance(id, party.default_advancement(id));
    party.train_skill(id, TrainingHall::fighters, "expertise:stealth");
    const auto saved = encode_campaign(party, nullptr, "hall");
    auto loaded = decode_campaign(saved, *srd5::character_rules(), *rules, "hall", nullptr);
    CampaignParty restored(module());
    restored.restore(std::move(loaded.party));
    const auto &member = restored.member(id);
    check(member.character.sheet().grants == party.member(id).character.sheet().grants &&
              member.character.hall_training() == party.member(id).character.hall_training() &&
              member.hall_experience_level == 1 && restored.state().wealth[3] == 3250,
          "Lessons, the session's level and the party's gold survive a reload");
    check(restored.experience_offer(id, TrainingHall::fighters).denial == TrainingDenial::none &&
              restored.skill_training_denial(id, TrainingHall::fighters) ==
                  TrainingDenial::already_trained,
          "After a reload the level-2 session is still for sale and the lesson is not");
    check(encode_campaign(restored, nullptr, "hall") == saved, "A reloaded party saves the same");
}
} // namespace

int main()
{
    try
    {
        halls_and_fees();
        experience_sessions();
        skill_lessons();
        scholar_after_hall();
        saved_training();
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Training hall tests passed\n";
}
