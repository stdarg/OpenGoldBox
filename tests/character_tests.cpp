#include "opengold/character_art.h"
#include "opengold/character_creator.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <set>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

using namespace opengold;
using namespace opengold::por;

namespace
{
IconPose pose_of(bool action)
{
    return action ? IconPose::action : IconPose::ready;
}

void check(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}

template <class F> void rejects(F action, const char *message)
{
    bool rejected = false;
    try
    {
        action();
    }
    catch (const std::exception &)
    {
        rejected = true;
    }
    check(rejected, message);
}

// A roll and an ability are different kinds of number, so passing them in the
// wrong order must not compile (Effective C++ Item 18).
template <class Roll, class Target>
concept AssignsRoll = requires(CharacterCreator &creator, Roll roll, Target target)
{
    creator.assign_roll(roll, target);
};
static_assert(AssignsRoll<unsigned, rules::Ability>);
static_assert(!AssignsRoll<unsigned, unsigned>);

void creation_tests()
{
    using namespace rules;
    auto module = srd5::character_rules();
    check(module->identity().version == "5.2.1", "Creator identifies its rules edition");
    check(module->choices(CreationField::race).size() == 9 &&
          module->choices(CreationField::character_class).size() == 12,
          "SRD species and all twelve classes available");
    RandomState seed{123}, again{123};
    check(module->roll(seed) == module->roll(again) && seed == again,
          "Seeded rolls reproduce across sessions");
    std::set<int> totals;
    for (int i = 0; i < 2000; ++i)
        for (const auto &roll : module->roll(seed))
        {
            check(roll.total() >= 3 && roll.total() <= 18, "4d6 result remains in range");
            totals.insert(roll.total());
            check(roll.dice[roll.discarded] ==
                  *std::min_element(roll.dice.begin(), roll.dice.end()),
                  "Lowest die is discarded");
        }
    check(totals.size() == 16, "The roller can produce every score, including low results");
    AbilityRoll example{{6, 1, 4, 3}, 1};
    check(example.total() == 13, "Four dice retain the three highest results");
    example.discarded = 0;
    rejects(
        [&]
    {
        (void)example.total();
    },
    "Cannot discard a higher die");
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = "fighter";
    d.alignment = "neutral_good";
    d.background = "soldier";
    d.name = "Mira";
    d.rolled = true;
    // Deliberately distinct base scores: STR 12, DEX 9, CON 14, INT 3, WIS 15, CHA 18.
    const std::array<std::array<int, 4>, 6> dice
    {
        {{4, 4, 4, 1}, {3, 3, 3, 1}, {5, 5, 4, 1}, {1, 1, 1, 1}, {5, 5, 5, 1}, {6, 6, 6, 1}}};
    for (unsigned i = 0; i < 6; ++i)
        d.rolls[i] = {dice[i], 3};
    auto s = module->evaluate(d, NameRequirement::required);
    check(s.scores[0] == 14 && s.scores[1] == 10 && s.scores[2] == 14 && s.hit_points == 12,
          "Fighter has maximum d10 plus Constitution");
    check(s.modifiers[3] == -4, "Odd negative ability modifiers round down");
    check(srd5::minimum_save_roll(15, srd5::ability_modifier(20)) == 10,
          "Strength 20 needs 10 to meet DC 15 without proficiency");
    check(srd5::minimum_save_roll(15, -4) == 19, "Negative save bonuses raise the required roll");
    check(srd5::minimum_save_roll(20, 0) == 20 && srd5::minimum_save_roll(21, 0) == 21,
          "Natural 20 does not automatically save");
    check(srd5::minimum_save_roll(5, 4) == 1 && srd5::minimum_save_roll(5, 8) == 1,
          "Natural 1 can save when its total meets DC");
    check(s.saving_throws == std::array<int, 6> {4, 0, 4, -4, 2, 4},
          "Fighter saves include proficiency only for Strength and Constitution");
    d.character_class = "wizard";
    const auto wizard = module->evaluate(d, NameRequirement::required);
    check(wizard.saving_throws == std::array<int, 6> {2, 0, 2, -2, 4, 4},
          "Wizard saves retain negative modifiers and add Intelligence/Wisdom proficiency");
    d.character_class = "fighter";
    const std::array<int, 12> hp{14, 10, 10, 10, 12, 10, 12, 12, 10, 8, 10, 8};
    const auto classes = module->choices(CreationField::character_class);
    for (unsigned i = 0; i < classes.size(); ++i)
    {
        d.character_class = classes[i].id;
        check(module->evaluate(d, NameRequirement::required).hit_points == hp[i],
              "Starting HP is correct for each class");
    }
    d.character_class = "fighter";
    d.race = "dwarf";
    check(module->evaluate(d, NameRequirement::required).hit_points == 13, "Dwarven Toughness adds one HP");
    for (const auto &bg : module->choices(CreationField::background))
    {
        d.background = bg.id;
        const auto adjustments = module->adjustments(bg.id);
        check(adjustments.size() == 7, "Every background offers six +2/+1 allocations and +1 each");
        for (unsigned n = 0; n < adjustments.size(); ++n)
        {
            d.adjustment = n;
            const auto evaluated = module->evaluate(d, NameRequirement::required);
            int bonus = 0;
            for (unsigned k = 0; k < 6; ++k)
            {
                bonus += evaluated.bonuses[k];
                check(evaluated.scores[k] <= 20, "Background bonuses cap at twenty");
            }
            check(evaluated.ability_adjustments.size() == 1 &&
                  evaluated.ability_adjustments[0].source_id == "background:" + bg.id &&
                  evaluated.ability_adjustments[0].level == 1 &&
                  evaluated.ability_adjustments[0].bonuses == adjustments[n].bonuses,
                  "Every background allocation retains its original source and level");
            check(bonus == 3, "Background grants exactly three points");
        }
    }
    d.background = "soldier";
    d.adjustment = 0;
    check(rules::class_eligible(*module, d, "fighter") &&
          rules::class_eligible(*module, d, "paladin"),
          "Bonuses count toward primary prerequisites");
    check(!rules::class_eligible(*module, d, "monk") &&
          !rules::class_eligible(*module, d, "wizard"),
          "Both Monk primaries and Wizard Intelligence required");
    d.target_classes = {"fighter", "monk", "wizard"};
    check(rules::unmet_targets(*module, d) ==
          std::array<bool, 6> {false, true, false, true, false, false},
          "Warnings flag only failing abilities of unmet targets");
    std::swap(d.assignment[0], d.assignment[1]);
    check(rules::class_eligible(*module, d, "fighter") && !rules::unmet_targets(*module, d)[0],
          "Qualified Dexterity satisfies Fighter without a Strength warning");
    std::swap(d.assignment[0], d.assignment[1]);
    d.target_classes.clear();
    check(rules::unmet_targets(*module, d) == std::array<bool, 6> {},
          "Removing targets clears warnings");
    for (const auto &option : classes)
    {
        auto boundary = d;
        boundary.background = "acolyte";
        boundary.adjustment = 1;
        for (auto &roll : boundary.rolls)
            roll = {{{4, 4, 4, 1}}, 3};
        // Choose a background bonus allocation outside each tested primary.
        const auto req = module->class_requirements(option.id);
        for (auto ability : req.abilities)
        {
            const auto bonuses =
                module->adjustments(boundary.background)[boundary.adjustment].bonuses;
            const int total = 13 - bonuses[ability_index(ability)];
            boundary.rolls[ability_index(ability)] = {{{total - 8, 4, 4, 1}}, 3};
        }
        check(rules::class_eligible(*module, boundary, option.id),
              "Every class accepts exactly 13 in its primary abilities");
        for (auto ability : req.abilities)
            --boundary.rolls[ability_index(ability)].dice[0];
        check(!rules::class_eligible(*module, boundary, option.id),
              "Every class rejects primaries below 13");
    }
    d.background = "soldier";
    d.adjustment = 0;
    d.assignment[0] = 1;
    rejects(
        [&]
    {
        (void)module->evaluate(d, NameRequirement::required);
    },
    "Duplicate roll assignments rejected");
    d.assignment[0] = 0;
    d.name = "  ";
    rejects(
        [&]
    {
        (void)module->evaluate(d, NameRequirement::required);
    },
    "Blank names rejected");
    d.name = "Mira";
    d.character_class = "invented";
    rejects(
        [&]
    {
        (void)module->evaluate(d, NameRequirement::required);
    },
    "Unknown class rejected");
    // A misspelt class ID used to match none of the rules' class checks and so
    // quietly offered nothing; now every rule that reads the class rejects it.
    d.character_class = "clerik";
    rejects(
        [&]
    {
        (void)module->cantrip_options(d);
    },
    "Cantrip options reject an unknown class");
    rejects(
        [&]
    {
        (void)module->spell_choice_options(d);
    },
    "Spell choice options reject an unknown class");
    rejects(
        [&]
    {
        (void)module->preset_ability_priority("clerik", 0);
    },
    "Preset ability priority rejects an unknown class");
    d.character_class.clear();
    check(module->cantrip_options(d).options.empty() &&
          module->spell_choice_options(d).learning.empty(),
          "A draft with no class yet is offered no spells");
    d.character_class = "invented";
    CharacterCreator creator(srd5::character_rules(), 42);
    rejects(
        [&]
    {
        (void)creator.create_character();
    },
    "Incomplete drafts cannot become characters");
    creator.select(CreationField::gender, "male");
    creator.next();
    check(creator.step() == CreationStep::alignment, "Race and gender advance to Alignment");
    creator.back();
    check(creator.step() == CreationStep::race && creator.draft().gender == "male",
          "Back retains gender on combined first step");
    for (int i = 0; i < 2; ++i)
        creator.next();
    rejects(
        [&]
    {
        creator.next();
    },
    "Cannot advance without rolling");
    check(creator.step() == CreationStep::attributes, "Invalid transition leaves step unchanged");
    creator.roll();
    const auto original = creator.draft().rolls;
    check(!creator.scores_assigned() &&
          std::all_of(creator.draft().assignment.begin(), creator.draft().assignment.end(),
                      [](auto n)
    {
        return n == 6;
    }),
    "Rolls start unassigned");
    rejects(
        [&]
    {
        creator.next();
    },
    "Cannot continue with empty ability boxes");
    creator.assign_roll(0, Ability::intelligence);
    creator.select(CreationField::background, "acolyte");
    const auto intelligence = [&]
    {
        return creator.rules().ability_score(creator.draft(), Ability::intelligence);
    };
    check(intelligence() == original[0].total() + 2,
          "Assigned Intelligence includes Acolyte bonus before other rolls are assigned");
    creator.select(CreationField::background, "sage");
    check(intelligence() == original[0].total() + 1,
          "Background changes recalculate partial scores");
    creator.select_adjustment(1);
    check(intelligence() == original[0].total() &&
          !creator.rules().ability_score(creator.draft(), Ability::strength),
          "Bonus changes update assigned scores and leave empty abilities empty");
    check(creator.draft().rolls == original, "Bonus changes preserve original dice");
    creator.select(CreationField::background, "acolyte");
    creator.assign_roll(1, Ability::dexterity);
    creator.assign_roll(0, Ability::dexterity);
    check(creator.draft().assignment[1] == 0 && creator.draft().assignment[3] == 1,
          "Assigned results swap when dropped on another ability");
    creator.assign_roll(2, Ability::dexterity);
    check(std::find(creator.draft().assignment.begin(), creator.draft().assignment.end(), 0) ==
          creator.draft().assignment.end(),
          "A displaced result returns to unassigned rolls");
    rejects(
        [&]
    {
        creator.assign_roll(6, Ability::strength);
    },
    "Invalid dice assignment rejected");
    rejects(
        [&]
    {
        creator.assign_roll(0, static_cast<Ability>(6));
    },
    "An ability converted from an out-of-range box index is rejected");
    for (unsigned i = 0; i < 6; ++i)
        creator.assign_roll(i, static_cast<Ability>(i));
    check(creator.scores_assigned(), "All six assignments permit review");
    creator.swap_scores(0, 2);
    check(creator.sheet().base[0] == original[2].total() && creator.draft().rolls == original,
          "Swaps preserve dice provenance");
    const auto assignment = creator.draft().assignment;
    rejects(
        [&]
    {
        creator.swap_scores(0, 6);
    },
    "Invalid swap rejected");
    check(creator.draft().assignment == assignment, "Rejected swap is atomic");
    creator.roll();
    check(creator.draft().rolls != original && creator.draft().assignment[0] == 6,
          "Full reroll replaces all rolls and empties assignments");
    for (unsigned i = 0; i < 6; ++i)
        creator.assign_roll(i, static_cast<Ability>(i));
    creator.target_class("wizard", true);
    creator.target_class("wizard", true);
    check(creator.draft().target_classes.size() == 1, "Repeated target selection is idempotent");
    creator.target_class("monk", true);
    creator.target_class("monk", false);
    rejects(
        [&]
    {
        creator.target_class("invented", true);
    },
    "Unknown targets rejected");
    creator.next();
    check(creator.step() == CreationStep::character_class, "Attributes advance to Class");
    bool selected = false;
    for (const auto &option : classes)
    {
        if (rules::class_eligible(*module, creator.draft(), option.id))
        {
            if (!selected)
            {
                creator.select(CreationField::character_class, option.id);
                selected = true;
            }
        }
        else
            rejects(
                [&]
        {
            creator.select(CreationField::character_class, option.id);
        },
        "Unqualified starting classes cannot be selected");
    }
    check(selected, "Fixture has a qualified class");
    creator.next();
    check(creator.step() == CreationStep::training,
          "Class advances to Training regardless of future target eligibility");
    rejects(
        [&]
    {
        creator.next();
    },
    "Required training blocks Name");
    creator.training_choice({.group = "class:cleric", .option = "medicine", .selected = true});
    creator.training_choice({.group = "class:cleric", .option = "persuasion", .selected = true});
    rejects(
        [&]
    {
        creator.next();
    },
    "A Cleric chooses a Divine Order");
    creator.training_choice({.group = "class:cleric:divine_order",
                             .option = "protector",
                             .selected = true});
    creator.next();
    check(creator.draft().character_class == "cleric" &&
          creator.step() == CreationStep::spell_choices,
          "Completed Cleric training advances to Spell Choices");
    creator.cantrip_choice("sacred_flame", true);
    creator.cantrip_choice("spare_the_dying", true);
    creator.cantrip_choice("resistance", true);
    rejects(
        [&]
    {
        creator.next();
    },
    "A Cleric prepares the available spells");
    for (const auto *spell :
            {"cure_wounds", "healing_word", "inflict_wounds", "shield_of_faith"
            })
        creator.spell_choice({.group = "prepared", .option = spell, .selected = true});
    creator.next();
    check(creator.step() == CreationStep::name, "Spell Choices advances to Name");
    rejects(
        [&]
    {
        creator.next();
    },
    "Name required before portrait");
    creator.name("  Mira Stoneward  ");
    creator.next();
    check(creator.step() == CreationStep::combat_icon,
          "Name advances directly to combat appearance");
    auto appearance = creator.appearance();
    appearance.portrait_head = 261;
    appearance.combat_head = 9;
    appearance.colors[1][5] = 0;
    creator.appearance(appearance);
    creator.next();
    check(creator.step() == CreationStep::sheet && creator.sheet().name == "Mira Stoneward",
          "Completed sheet retains trimmed name");
    auto finished = creator.create_character();
    check(finished.creation_data().target_classes == std::vector<std::string> {"wizard"},
          "Future targets persist without adding class levels");
    auto revised = appearance;
    revised.portrait_body = 2;
    creator.appearance(revised);
    check(creator.step() == CreationStep::sheet &&
          creator.create_character().appearance() == revised,
          "Portrait can change while reviewing a completed sheet");
    creator.appearance(appearance);
    const auto retained = creator.draft();
    const auto sheet = creator.sheet();
    check(finished.inventory().empty() && finished.appearance() == appearance,
          "Finished character owns appearance and an empty inventory");
    check(Character(creator.rules(), retained, appearance).appearance() == appearance,
          "The same draft builds a character with a valid appearance");
    auto unknown_body = appearance;
    unknown_body.combat_body = 35;
    rejects(
        [&]
    {
        (void)Character(creator.rules(), retained, unknown_body);
    },
    "A character is never built with an invalid appearance");
    rejects(
        [&]
    {
        creator.roll();
    },
    "Completed sheet cannot be silently rerolled");
    creator.back();
    creator.next();
    check(creator.appearance() == appearance, "Review retains both appearance banks");
    creator.back();
    creator.back();
    creator.back();
    creator.back();
    creator.back();
    creator.back();
    check(creator.step() == CreationStep::attributes, "Back navigation reaches attributes");
    creator.select(CreationField::race, "dwarf");
    creator.select(CreationField::character_class, "barbarian");
    check(creator.sheet().hit_points == 13 + creator.sheet().modifiers[2],
          "HP recalculates after earlier edits");
    creator.restart();
    check(!creator.draft().rolled && creator.draft().name.empty() &&
          creator.step() == CreationStep::race,
          "Restart clears the single character");
    const auto &data = finished.creation_data();
    const auto &saved = finished.sheet();
    check(data.race == retained.race && data.gender == retained.gender &&
          data.character_class == retained.character_class &&
          data.alignment == retained.alignment && data.background == retained.background &&
          data.name == retained.name && data.rolls == retained.rolls &&
          data.assignment == retained.assignment && data.adjustment == retained.adjustment &&
          data.rolled,
          "Character retains all creation choices and dice after creator edits/restart");
    check(saved.identity.version == "5.2.1" && saved.name == sheet.name &&
          saved.race == sheet.race && saved.gender == sheet.gender &&
          saved.character_class == sheet.character_class &&
          saved.alignment == sheet.alignment && saved.background == sheet.background &&
          saved.base == sheet.base && saved.bonuses == sheet.bonuses &&
          saved.scores == sheet.scores && saved.modifiers == sheet.modifiers &&
          saved.hit_die == sheet.hit_die && saved.hit_points == sheet.hit_points &&
          saved.hp_explanation == sheet.hp_explanation && saved.level == 1,
          "Character retains rules identity and evaluated sheet values");
    const auto sword = finished.add_item({.definition_id = "test:longsword",
                                                 .name = "Longsword"});
    auto copy = finished;
    copy.remove_item(sword);
    auto recolored = appearance;
    recolored.colors[0][0] = 2;
    copy.appearance(recolored);
    check(!finished.inventory().empty() && finished.appearance() == appearance &&
          copy.inventory().empty(),
          "Copies independently own inventory and appearance");
    recolored.colors[1][3] = 16;
    rejects(
        [&]
    {
        copy.appearance(recolored);
    },
    "Character rejects invalid appearance colors");
    check(copy.appearance().colors[1][3] == appearance.colors[1][3],
          "Rejected appearance changes are atomic");
    // The temporary module is destroyed at this statement's end.
    Character detached(*srd5::character_rules(), retained, appearance);
    check(detached.sheet().hit_points == saved.hit_points &&
          detached.creation_data().rolls == data.rolls,
          "Character does not borrow its rules module");
    auto invalid = retained;
    invalid.name = "";
    rejects(
        [&]
    {
        Character rejected(*srd5::character_rules(), invalid, appearance);
    },
    "Direct character construction validates creation data");
}

void inventory_tests()
{
    Inventory inventory;
    check(inventory.empty() && !inventory.find(1), "New inventory is empty");
    rejects(
        [&]
    {
        inventory.add({.definition_id = "", .name = "Arrows"});
    },
    "Items require a stable definition key");
    rejects(
        [&]
    {
        inventory.add({.definition_id = "test:arrow", .name = "  "});
    },
    "Items require a display name");
    rejects(
        [&]
    {
        inventory.add({.definition_id = "test:arrow", .name = "Arrows", .quantity = 0});
    },
    "Empty stacks rejected");
    const auto first = inventory.add({.definition_id = "test:arrow",
                                      .name = "Arrows",
                                      .quantity = 20}),
               second = inventory.add({.definition_id = "test:arrow",
                                       .name = "Arrows",
                                       .quantity = 5});
    check(first != second && inventory.items().size() == 2 &&
          inventory.find(first)->get().definition_id == "test:arrow",
          "Separate stacks have stable IDs");
    const auto before =
        std::vector<InventoryItem>(inventory.items().begin(), inventory.items().end());
    rejects(
        [&]
    {
        inventory.remove(first, 21);
    },
    "Cannot remove more than a stack holds");
    rejects(
        [&]
    {
        inventory.remove(first, 0);
    },
    "Zero removals rejected");
    rejects(
        [&]
    {
        inventory.remove(999);
    },
    "Unknown items rejected");
    check(std::equal(before.begin(), before.end(), inventory.items().begin(),
                     inventory.items().end()),
          "Rejected inventory operations leave all stacks intact");
    inventory.remove(first, 19);
    check(inventory.find(first)->get().quantity == 1 && inventory.find(second)->get().quantity == 5,
          "Partial removal affects only the requested stack");
    inventory.remove(first);
    check(!inventory.find(first) && inventory.items().size() == 1,
          "Removing the final item removes its stack");
    const auto third = inventory.add({.definition_id = "test:shield", .name = "Shield"});
    check(third != first && third != second, "Removed stack IDs are not reused");
    inventory.remove(second, 5);
    inventory.remove(third);
    check(inventory.empty(), "All inventory can be removed");
}

void additional_portrait_tests()
{
    Image source;
    source.width = 176;
    source.height = 84;
    source.rgba.resize(176 * 84 * 4);
    for (unsigned y = 0; y < 84; ++y)
        for (unsigned x = 0; x < 176; ++x)
        {
            const auto p = (y * 176 + x) * 4;
            source.rgba[p + 3] = 255;
            if (y < 80)
            {
                source.rgba[p] = 31 + x / 2;
                source.rgba[p + 1] = 61 + y / 2;
                source.rgba[p + 2] = 173;
            }
        }
    const auto original = source.rgba;
    const auto panel = prepare_portrait_head(source, 256);
    check(panel.width == 88 && panel.height == 40 && source.rgba == original,
          "Preparing a head leaves source artwork intact");
    for (unsigned y = 0; y < 40; ++y)
        for (unsigned x = 0; x < 88; ++x)
        {
            const auto p = (y * 88 + x) * 4;
            check(panel.rgba[p] == 31 + x && panel.rgba[p + 1] == 61 + y &&
                  panel.rgba[p + 2] == 173 && panel.rgba[p + 3] == 255,
                  "Nearest sampling preserves approved colors and removes the bottom gap");
        }
    Image translucent;
    translucent.width = translucent.height = 1;
    translucent.rgba = {240, 160, 80, 128};
    const auto opaque = prepare_portrait_head(translucent, 256);
    check(opaque.rgba[0] == 120 && opaque.rgba[1] == 80 && opaque.rgba[2] == 40 &&
          opaque.rgba[3] == 255,
          "Alpha composites onto the portrait's black background");
    rejects(
        [&]
    {
        (void)prepare_portrait_head(Image{}, 256);
    },
    "Empty portrait sources rejected");
    translucent.rgba = {0, 0, 0, 255};
    rejects(
        [&]
    {
        (void)prepare_portrait_head(translucent, 256);
    },
    "Blank portrait sources rejected");
    source.rgba.pop_back();
    rejects(
        [&]
    {
        (void)prepare_portrait_head(source, 256);
    },
    "Truncated portrait pixels rejected");
    CharacterArt art;
    art.set_original_head(1, PortraitPart{"original", panel});
    Image body;
    body.width = 88;
    body.height = 48;
    body.rgba.assign(88 * 48 * 4, 128);
    art.set_original_body(1, PortraitPart{"original", body});
    std::set<unsigned> ids;
    std::set<std::string_view> files;
    const auto additions = additional_portrait_heads();
    check(additions.size() == 10, "All ten additional heads are cataloged");
    for (const auto &head : additions)
    {
        check(head.id > 255 && ids.insert(head.id).second && files.insert(head.filename).second,
              "Additional heads have unique IDs outside the original archive range");
        check(matching_portrait_head(head.race, head.gender) == head.id,
              "Race and gender recommend the matching approved portrait");
        art.add_portrait_head(head.id, panel);
        CharacterAppearance a;
        a.portrait_head = head.id;
        const auto portrait = art.portrait(a);
        check(portrait.width == 88 && portrait.height == 88 && portrait.rgba.size() == 88 * 88 * 4,
              "Additional heads compose a complete portrait");
        check(std::equal(body.rgba.begin(), body.rgba.end(), portrait.rgba.begin() + 88 * 40 * 4),
              "Neck fitting leaves the original body unchanged");
        check(art.heads().at(head.id).label == head.label,
              "Additional heads retain readable selection labels");
    }
    check(!matching_portrait_head("human", "female") &&
          !matching_portrait_head("goliath", "nonbinary"),
          "No unsupported portrait recommendation is invented");
    rejects(
        [&]
    {
        (void)prepare_portrait_head(translucent, 266);
    },
    "Unknown head fitting profiles rejected");
    rejects(
        [&]
    {
        art.add_portrait_head(1, panel);
    },
    "Additional artwork cannot replace an original archive ID");
    rejects(
        [&]
    {
        art.add_portrait_head(256, panel);
    },
    "Duplicate new head IDs rejected");
    CharacterArt unloaded;
    rejects(
        [&]
    {
        unloaded.add_portrait_head(256, body);
    },
    "Additional head must be 88 by 40");
    CharacterAppearance invalid;
    invalid.portrait_head = 266;
    rejects(
        [&]
    {
        validate_character_appearance(invalid);
    },
    "Unregistered extended head IDs rejected");
    CharacterAppearance original_appearance;
    const auto unchanged = art.portrait(original_appearance);
    check(std::equal(panel.rgba.begin(), panel.rgba.end(), unchanged.rgba.begin()),
          "Original heads retain their exact placement and pixels");
    // Independent join geometry: male/female openings, including an off-center
    // narrow neck and unrelated collar pixels on the same scanline.
    for (const auto &head : additions)
        for (const auto opening :
                {
                    std::pair{36u, 56u}, std::pair{37u, 55u}, std::pair{36u, 52u}
                })
        {
            Image neck;
            neck.width = 88;
            neck.height = 40;
            neck.rgba.assign(88 * 40 * 4, 0);
            for (unsigned p = 3; p < neck.rgba.size(); p += 4)
                neck.rgba[p] = 255;
            for (unsigned y = 34; y < 40; ++y)
                for (unsigned x = head.neck_left; x < head.neck_right; ++x)
                {
                    const auto p = (y * 88 + x) * 4;
                    neck.rgba[p] = 80;
                    neck.rgba[p + 1] = 120;
                    neck.rgba[p + 2] = 200;
                }
            // A face feature above the fitted rows must retain its size and colors.
            for (unsigned x = 40; x < 44; ++x)
                neck.rgba[(20 * 88 + x) * 4] = 211;
            Image torso = body;
            torso.rgba.assign(88 * 48 * 4, 0);
            for (unsigned p = 3; p < torso.rgba.size(); p += 4)
                torso.rgba[p] = 255;
            for (unsigned x = opening.first; x < opening.second; ++x)
            {
                torso.rgba[x * 4] = 255;
                torso.rgba[x * 4 + 1] = torso.rgba[x * 4 + 2] = 85;
            }
            torso.rgba[32 * 4] = 170; // red collar, not skin
            CharacterArt joined;
            joined.set_original_body(1, PortraitPart{"synthetic", torso});
            joined.add_portrait_head(head.id, neck);
            CharacterAppearance a;
            a.portrait_head = head.id;
            const auto image = joined.portrait(a);
            unsigned face_pixels = 0;
            for (unsigned x = 0; x < 88; ++x)
            {
                check(
                    (image.rgba[(39 * 88 + x) * 4] == 80) ==
                    (x >= opening.first && x < opening.second),
                    "Every new head meets the selected body's full neck opening with no overhang");
                if (image.rgba[(20 * 88 + x) * 4] == 211)
                    ++face_pixels;
            }
            check(face_pixels == 4, "Neck fitting translates the face without stretching it");
            check(
                std::equal(torso.rgba.begin(), torso.rgba.end(), image.rgba.begin() + 88 * 40 * 4),
                "Fitting never recolors or moves original body pixels");
        }
    Image rounded;
    rounded.width = 88;
    rounded.height = 40;
    rounded.rgba.assign(88 * 40 * 4, 255);
    for (unsigned y = 0; y < 40; ++y)
        for (unsigned x = 0; x < 88; ++x)
            rounded.rgba[(y * 88 + x) * 4] = y;
    for (const auto id :
            {
                258u, 260u, 262u, 264u
            })
        check(
            prepare_portrait_head(rounded, id).rgba[39 * 88 * 4] == 39,
            "Revised male heads retain the full neck below the chin instead of cropping it into the armor");
}

void art_tests()
{
    std::vector<std::uint8_t> raw(17 + 24 * 24 / 2);
    raw[0] = 24;
    raw[2] = 3;
    raw[8] = 1;
    raw[17] = 0x6e;
    auto body = decode_character_icon(raw);
    check(body.pixels[0] == 6 && body.pixels[1] == 14, "Preserve region indices and nibble order");
    raw.pop_back();
    rejects(
        [&]
    {
        (void)decode_character_icon(raw);
    },
    "Reject truncated components");
    IndexedIcon head{24, 10, std::vector<std::uint8_t>(240)};
    CharacterAppearance a;
    const std::array<unsigned, 6> masks{7, 1, 4, 6, 2, 3};
    for (unsigned bank = 0; bank < 2; ++bank)
        for (unsigned region = 0; region < 6; ++region)
        {
            body.pixels.assign(576, 0);
            body.pixels[0] = masks[region] + bank * 8;
            body.pixels[1] = 8;
            body.pixels[2] = 5;
            a.colors[bank][region] = 0;
            const auto black = compose_character_icon(head, body, a);
            check(black.rgba[3] == 255 && black.rgba[0] == 0, "Chosen black must remain opaque");
            a.colors[bank][region] = 14;
            const auto yellow = compose_character_icon(head, body, a);
            check(yellow.rgba[0] == 255 && yellow.rgba[1] == 255 && yellow.rgba[2] == 85,
                  "Each bank/region maps independently");
            check(yellow.rgba[4] == 0 && yellow.rgba[7] == 255 && yellow.rgba[15] == 0,
                  "Outline and transparency preserved");
            check(yellow.rgba[8] == black.rgba[8], "Recolor does not alter fixed cap pixels");
        }
    head.pixels[0] = 4;
    a.colors[0][2] = 10;
    body.pixels[0] = 6;
    auto composed = compose_character_icon(head, body, a);
    check(composed.rgba[0] == 85 && composed.rgba[1] == 255,
          "Head overlays body using indexed colors");
    a.colors[1][5] = 16;
    rejects(
        [&]
    {
        (void)compose_character_icon(head, body, a);
    },
    "Reject invalid colors");
    if (const auto directory = std::getenv("OPENGOLD_GAME_DIR"))
    {
        const auto art = CharacterArt::load(directory);
        a = CharacterAppearance{};
        check(art.portrait(a).rgba.size() == 88 * 88 * 4,
              "Original portrait parts join into a sheet image");
        for (bool tall :
                {
                    false, true
                })
        {
            a.tall = tall;
            a.combat_body = 0;
            auto usage = art.color_usage(a);
            for (unsigned bank = 0; bank < 2; ++bank)
                check(!usage.contains(bank, 0) && !usage.contains(bank, 3),
                      "Original unarmed sprite has no weapon or shield to recolor");
            a.combat_body = 1;
            usage = art.color_usage(a);
            for (unsigned bank = 0; bank < 2; ++bank)
                check(usage.contains(bank, 0) && !usage.contains(bank, 3),
                      "Original bow uses weapon indices, without a shield");
            a.combat_body = 4;
            a.combat_head = 2;
            usage = art.color_usage(a);
            for (bool action :
                    {
                        false, true
                    })
            {
                const auto bank = (tall ? 64u : 0u) + (action ? 128u : 0u);
                auto source = art.combat_bodies().at(bank + 4).pixels;
                const auto &head_pixels = art.combat_heads().at(bank + 2).pixels;
                for (unsigned p = 0; p < head_pixels.size(); ++p)
                    if (head_pixels[p])
                        source[p] = head_pixels[p];
                const auto original = art.icon(a, pose_of(action));
                for (unsigned color_bank = 0; color_bank < 2; ++color_bank)
                    for (unsigned part = 0; part < 6; ++part)
                    {
                        auto changed = a;
                        changed.colors[color_bank][part] = (a.colors[color_bank][part] + 3) % 16;
                        const auto recolored = art.icon(changed, pose_of(action));
                        unsigned count = 0;
                        for (unsigned p = 0; p < source.size(); ++p)
                        {
                            const bool differs = !std::equal(original.rgba.begin() + p * 4,
                                                             original.rgba.begin() + p * 4 + 4,
                                                             recolored.rgba.begin() + p * 4);
                            const bool targeted = source[p] == masks[part] + color_bank * 8;
                            check(differs == targeted,
                                  "Only the selected original source mask changes in each pose");
                            if (targeted)
                                ++count;
                        }
                        check(count == (action ? usage.action : usage.ready)[color_bank][part],
                              "Visible region counts match the composed original art");
                        check(
                            usage.contains(color_bank, part) ==
                            (part != 2 || color_bank != 0 || !tall),
                            "The tall helmet covers hair; other armed test sprite regions remain visible");
                    }
            }
        }
        for (unsigned bank :
                {
                    0u, 64u, 128u, 192u
                })
        {
            const auto &original = art.combat_bodies().at(bank + 21).pixels;
            const auto &derived = art.combat_bodies().at(bank + 32).pixels;
            check(original.size() == derived.size(),
                  "Derived shield body retains original dimensions");
            unsigned removed = 0;
            for (std::size_t p = 0; p < original.size(); ++p)
                if (original[p] != derived[p])
                {
                    check(original[p] == 8 && derived[p] == 0,
                          "Derived shield body only removes gray wand pixels");
                    ++removed;
                }
            check(removed > 0, "Derived shield body removes wand in each size and pose");
        }
        for (bool tall :
                {
                    false, true
                })
            for (unsigned h = 0; h < 14; ++h)
                for (unsigned b = 0; b < 35; ++b)
                {
                    a.tall = tall;
                    a.combat_head = h;
                    a.combat_body = b;
                    check(art.icon(a, IconPose::ready).rgba.size() == 576 * 4 &&
                          art.icon(a, IconPose::action).rgba.size() == 576 * 4,
                          "Every original head/body combination has both poses");
                }
        std::cout << "Original character art: " << art.heads().size() << " portrait heads, "
                  << art.bodies().size()
                  << " bodies; 14 combat heads, 35 bodies, both sizes and poses\n";
    }
}
} // namespace

int main()
{
    try
    {
        creation_tests();
        inventory_tests();
        additional_portrait_tests();
        art_tests();
        std::cout << "Character tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
