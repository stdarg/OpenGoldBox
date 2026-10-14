#include "dice.h"
#include "character_class.h"
#include "feature_grants.h"
#include "training.h"
#include "spell_access.h"
#include "opengold/srd5.h"
#include "status_effects.h"
#include <algorithm>
#include <cstdlib>
#include <set>
#include <stdexcept>

namespace opengold::srd5
{
using namespace rules;

namespace
{
const AbilityArray<std::string> ability_names{"STR", "DEX", "CON", "INT", "WIS", "CHA"};

// Whether another learning group of the draft than `group` has chosen the spell.
bool picked_elsewhere(const CharacterDraft &draft, std::string_view group, std::string_view spell)
{
    return draft.spells &&
           std::any_of(draft.spells->learning.begin(), draft.spells->learning.end(),
                       [&](const auto & picks)
    {
        return picks.first != group &&
               std::find(picks.second.begin(), picks.second.end(), spell) != picks.second.end();
    });
}

// A spell is learned once, so a group does not offer one another group of the
// draft has chosen (a Sage Wizard's spellbook and Magic Initiate share a list).
SpellChoiceOptions without_other_picks(SpellChoiceOptions options, const CharacterDraft &draft)
{
    for (auto &group : options.learning)
        std::erase_if(group.options, [&](const auto & option)
    {
        return picked_elsewhere(draft, group.id, option.id);
    });
    return options;
}

struct Class
{
    detail::CharacterClass character_class;
    int die;
    const char *description;
};

constexpr std::array<Class, 12> classes
{
    {   {
            detail::CharacterClass::barbarian, 12,
            "A fierce warrior. Primary ability: Strength. Hit Die: d12."
        },
        {
            detail::CharacterClass::bard, 8,
            "An inspiring performer and spellcaster. Primary ability: Charisma. Hit Die: d8."
        },
        {
            detail::CharacterClass::cleric, 8,
            "A wielder of divine magic. Primary ability: Wisdom. Hit Die: d8."
        },
        {
            detail::CharacterClass::druid, 8,
            "A guardian of nature. Primary ability: Wisdom. Hit Die: d8."
        },
        {
            detail::CharacterClass::fighter, 10,
            "A master of weapons. Primary ability: Strength or Dexterity. Hit Die: d10."
        },
        {
            detail::CharacterClass::monk, 8,
            "A disciplined martial artist. Primary abilities: Dexterity and Wisdom. Hit Die: d8."
        },
        {
            detail::CharacterClass::paladin, 10,
            "An oathbound warrior. Primary abilities: Strength and Charisma. Hit Die: d10."
        },
        {
            detail::CharacterClass::ranger, 10,
            "A hunter and wilderness explorer. Primary abilities: Dexterity and Wisdom. Hit Die: d10."
        },
        {
            detail::CharacterClass::rogue, 8,
            "An expert in stealth and precision. Primary ability: Dexterity. Hit Die: d8."
        },
        {
            detail::CharacterClass::sorcerer, 6,
            "A spellcaster with innate magic. Primary ability: Charisma. Hit Die: d6."
        },
        {
            detail::CharacterClass::warlock, 8,
            "A spellcaster empowered by a pact. Primary ability: Charisma. Hit Die: d8."
        },
        {
            detail::CharacterClass::wizard, 6,
            "A scholar of arcane magic. Primary ability: Intelligence. Hit Die: d6."
        }
    }};

class CreatorRules final : public CharacterRules
{
  public:
    std::array<Ability, 6> preset_ability_priority(std::string_view id,
            unsigned variant) const override
    {
        using enum detail::CharacterClass;
        const auto klass = detail::class_from_id(id);
        const Ability primary =
            klass == wizard                                            ? Ability::intelligence
            : (klass == cleric || klass == druid)                      ? Ability::wisdom
            : (klass == bard || klass == sorcerer || klass == warlock) ? Ability::charisma
            : (klass == monk || klass == ranger || klass == rogue ||
               (klass == fighter && variant % 2))
            ? Ability::dexterity
            : Ability::strength;
        const Ability secondary = (klass == monk || klass == ranger) ? Ability::wisdom
                                  : klass == paladin                  ? Ability::charisma
                                  : Ability::constitution;
        std::vector<Ability> priority{primary, secondary};
        for (const auto ability :
                {
                    Ability::constitution, Ability::dexterity, Ability::wisdom, Ability::strength,
                    Ability::intelligence, Ability::charisma
                })
            if (std::find(priority.begin(), priority.end(), ability) == priority.end())
                priority.push_back(ability);
        std::array<Ability, 6> result{};
        std::copy(priority.begin(), priority.end(), result.begin());
        return result;
    }

    ClassRequirements class_requirements(std::string_view id) const override;

    Identity identity() const override
    {
        return {"srd5", "5.2.1", "character-creation.1"};
    }

    std::vector<CreationChoice> choices(CreationField field) const override;
    std::vector<ScoreAdjustment> adjustments(std::string_view background) const override;
    std::array<AbilityRoll, 6> roll(RandomState &state) const override;
    std::optional<int> ability_score(const CharacterDraft &draft, Ability ability) const override;
    CharacterSheet evaluate(const CharacterDraft &draft, NameRequirement name) const override;

    std::vector<TrainingChoiceGroup> training_options(const CharacterDraft &draft) const override
    {
        return detail::training_options(draft);
    }

    SpellChoiceOptions spell_choice_options(const CharacterDraft &draft) const override
    {
        using enum detail::CharacterClass;
        const auto klass = detail::draft_class(draft);
        if (!klass)
            return {};
        // At creation only Magic Initiate, from an Acolyte's or a Sage's
        // background or a Human's Versatile, brings spells to a class without
        // its own.
        const auto versatile = draft.training.find("species:human:versatile");
        const bool initiate = draft.background == "acolyte" || draft.background == "sage" ||
                              (versatile != draft.training.end() &&
                               std::any_of(versatile->second.begin(), versatile->second.end(),
                                           [](const auto & feat)
        {
            return feat.starts_with("magic_initiate_");
        }));
        if (!detail::prepares_spells(*klass) && !initiate)
            return {};
        // The options come from the evaluated sheet, which needs every score.
        if (!draft.rolled || std::any_of(draft.assignment.begin(), draft.assignment.end(),
                                         [](unsigned roll)
    {
        return roll >= 6;
    }))
        return {};
        if (klass != wizard)
        {
            // These classes prepare from the class list, or learn only Magic
            // Initiate's spells; the selection never changes the options.
            auto base = draft;
            base.spells = SpellChoices{};
            return without_other_picks(
                       detail::spell_choice_options(evaluate(base, NameRequirement::optional),
                               SpellChoiceContext::advancement),
                       draft);
        }
        auto base = draft;
        base.spells = SpellChoices{};
        auto sheet = evaluate(base, NameRequirement::optional);
        auto options = detail::spell_choice_options(sheet, SpellChoiceContext::advancement);
        std::erase_if(options.learning,
                      [](const auto & g)
        {
            return g.id.starts_with("cantrips:");
        });
        const auto selected = evaluate(draft, NameRequirement::optional);
        options.preparation =
            detail::spell_choice_options(selected, SpellChoiceContext::advancement).preparation;
        return without_other_picks(std::move(options), draft);
    }

    TrainingChoiceGroup cantrip_options(const CharacterDraft &draft) const override
    {
        const auto klass = detail::draft_class(draft);
        if (!klass)
            return {};
        auto group = detail::starting_cantrip_options(*klass);
        // The Thaumaturge Divine Order adds one Cleric cantrip.
        const auto order = draft.training.find("class:cleric:divine_order");
        if (klass == detail::CharacterClass::cleric && order != draft.training.end() &&
                order->second == std::vector<std::string> {"thaumaturge"})
            ++group.count;
        // So does the Druid's Magician Primal Order.
        const auto primal = draft.training.find("class:druid:primal_order");
        if (klass == detail::CharacterClass::druid && primal != draft.training.end() &&
                primal->second == std::vector<std::string> {"magician"})
            ++group.count;
        // A cantrip Magic Initiate already teaches is not learned twice.
        std::erase_if(group.options, [&](const auto & option)
        {
            return picked_elsewhere(draft, {}, option.id);
        });
        return group;
    }

    AbilityCheckModifier ability_check(const CharacterSheet &sheet, Ability ability,
                                       std::string_view skill) const override
    {
        return detail::ability_check(sheet.grants, detail::class_of(sheet), sheet.race,
                                     detail::background_id(sheet.background), sheet.level,
                                     sheet.scores, ability, skill);
    }
};

std::vector<CreationChoice> CreatorRules::choices(CreationField field) const
{
    switch (field)
    {
    case CreationField::race:
        // Each species' SRD 5.2.1 traits (pp. 84-86) in brief, marked as not
        // in the game yet where the rules do not apply them. Traits SCOPE-2 and
        // DM-1 left out are not listed.
        return {{"dragonborn", "Dragonborn",
                "Dragonborn trace their line to dragons, whose power shows in their scales and their breath.\n\nRacial traits:\n- Draconic Ancestry: a kind of dragon that sets your breath's damage and your resistance.\n- Breath Weapon: in place of an attack, a 15-foot cone or 30-foot line of 1d10 damage, Dexterity save for half, twice per Long Rest.\n- Damage Resistance to your ancestry's damage type.\n- Darkvision, 60 feet. Not in the game yet."},
            {"dwarf", "Dwarf",
                "Dwarves are stout, hardy folk, as enduring as the stone they work.\n\nRacial traits:\n- Dwarven Toughness: 1 more Hit Point at every level.\n- Dwarven Resilience: Resistance to Poison damage, and Advantage on saves against being Poisoned.\n- Darkvision, 120 feet. Not in the game yet."},
            {"elf", "Elf",
                "Elves are long-lived folk with the blood of the Feywild in their veins.\n\nRacial traits:\n- Trance: magic cannot put you to sleep.\n- Elven Lineage: High Elf or Wood Elf, each with its own magic.\n- Fey Ancestry: Advantage on saves against being Charmed.\n- Keen Senses: training in Insight, Perception or Survival.\n- Darkvision, 60 feet. Not in the game yet."},
            {"gnome", "Gnome",
                "Gnomes are small, quick-witted folk with a knack for magic and invention.\n\nRacial traits:\n- Small.\n- Gnomish Cunning: Advantage on Intelligence, Wisdom and Charisma saves.\n- Darkvision, 60 feet. Not in the game yet."},
            {"goliath", "Goliath",
                "Goliaths are towering folk descended from giants.\n\nRacial traits:\n- Speed of 35 feet.\n- Giant Ancestry: a boon from cloud, fire, frost, hill, stone or storm giants, used a few times between Long Rests. Not in the game yet."},
            {"halfling", "Halfling",
                "Halflings are small, cheerful folk known for their courage and uncanny luck.\n\nRacial traits:\n- Luck: reroll a 1 on an attack roll, ability check or saving throw.\n- Brave: Advantage on saves against being Frightened.\n- Halfling Nimbleness: move through the space of larger creatures, without stopping there.\n- Naturally Stealthy: hide behind larger creatures."},
            {"human", "Human",
                "Humans are ambitious, adaptable folk found in every land.\n\nRacial traits:\n- Resourceful: Heroic Inspiration after every Long Rest, which rolls your first failed d20 again.\n- Skillful: training in one skill of your choice.\n- Versatile: an Origin feat of your choice."},
            {"orc", "Orc",
                "Orcs are strong, tireless folk who press on where others falter.\n\nRacial traits:\n- Adrenaline Rush: Dash as a Bonus Action and gain Temporary Hit Points, a few times between rests.\n- Relentless Endurance: drop to 1 Hit Point instead of 0, once per Long Rest.\n- Darkvision, 120 feet. Not in the game yet."},
            {"tiefling", "Tiefling",
                "Tieflings carry the mark of the Lower Planes in their blood and bearing.\n\nRacial traits:\n- Fiendish Legacy: Abyssal, Chthonic or Infernal, each with a resistance and its own magic.\n- Darkvision, 60 feet. Not in the game yet."}};
    case CreationField::gender:
        return {{"female", "Female",
                "Gender does not change attributes or restrict class or appearance choices."
            },
            {
                "male", "Male",
                "Gender does not change attributes or restrict class or appearance choices."
            },
            {
                "nonbinary", "Nonbinary",
                "Gender does not change attributes or restrict class or appearance choices."
            }};
    case CreationField::character_class:
    {
        std::vector<CreationChoice> result;
        for (const auto &c : classes)
            result.push_back({std::string(detail::class_id(c.character_class)),
                              std::string(detail::class_label(c.character_class)), c.description});
        return result;
    }
    case CreationField::alignment:
        return
        {
            {"lawful_good", "Lawful Good", "Guided by compassion, duty, and order."},
            {"neutral_good", "Neutral Good", "Seeks to help others, with flexibility about rules."},
            {"chaotic_good", "Chaotic Good", "Values kindness and individual freedom."},
            {"lawful_neutral", "Lawful Neutral", "Guided by rules, tradition, or a personal code."},
            {
                "neutral", "Neutral",
                "Balances competing principles or acts according to circumstances."
            },
            {"chaotic_neutral", "Chaotic Neutral", "Values personal freedom and independence."},
            {"lawful_evil", "Lawful Evil", "Pursues selfish ends through structure and rules."},
            {
                "neutral_evil", "Neutral Evil",
                "Pursues selfish ends without loyalty to order or freedom."
            },
            {
                "chaotic_evil", "Chaotic Evil",
                "Acts destructively with little regard for rules or others."
            }};
    case CreationField::background:
        return
        {
            {"acolyte", "Acolyte", "Attribute bonuses: Intelligence, Wisdom, Charisma."},
            {"criminal", "Criminal", "Attribute bonuses: Dexterity, Constitution, Intelligence."},
            {"sage", "Sage", "Attribute bonuses: Constitution, Intelligence, Wisdom."},
            {"soldier", "Soldier", "Attribute bonuses: Strength, Dexterity, Constitution."}};
    }
    throw std::runtime_error("Unknown creation field");
}

std::vector<ScoreAdjustment> CreatorRules::adjustments(std::string_view background) const
{
    using enum Ability;
    std::array<Ability, 3> allowed;
    if (background == "acolyte")
        allowed = {intelligence, wisdom, charisma};
    else if (background == "criminal")
        allowed = {dexterity, constitution, intelligence};
    else if (background == "sage")
        allowed = {constitution, intelligence, wisdom};
    else if (background == "soldier")
        allowed = {strength, dexterity, constitution};
    else
        throw std::runtime_error("Unknown background");
    std::vector<ScoreAdjustment> result;
    for (auto first : allowed)
        for (auto second : allowed)
            if (first != second)
            {
                ScoreAdjustment a;
                a.label = ability_names[first] + " +2, " + ability_names[second] + " +1";
                a.bonuses[first] = 2;
                a.bonuses[second] = 1;
                result.push_back(a);
            }
    ScoreAdjustment a;
    a.label = "+1 to " + ability_names[allowed[0]] + ", " + ability_names[allowed[1]] + ", " +
              ability_names[allowed[2]];
    for (auto ability : allowed)
        a.bonuses[ability] = 1;
    result.push_back(a);
    return result;
}

ClassRequirements CreatorRules::class_requirements(std::string_view id) const
{
    using enum Ability;
    const std::array<std::vector<Ability>, 12> primary
    {
        {   {strength}, {charisma}, {wisdom}, {wisdom}, {strength, dexterity},
            {dexterity, wisdom}, {strength, charisma}, {dexterity, wisdom}, {dexterity},
            {charisma}, {charisma}, {intelligence}
        }};
    const auto found = std::find_if(classes.begin(), classes.end(),
                                    [&](const auto & c)
    {
        return detail::class_id(c.character_class) == id;
    });
    if (found == classes.end())
        throw std::runtime_error("Unknown class prerequisite");
    ClassRequirements result{primary.at(found - classes.begin()),
                             found->character_class == detail::CharacterClass::fighter, 13, {}};
    for (auto ability : result.abilities)
    {
        if (!result.description.empty())
            result.description += result.any ? " or " : " and ";
        result.description += ability_names[ability] + " 13";
    }
    return result;
}

std::array<AbilityRoll, 6> CreatorRules::roll(RandomState &state) const
{
    std::array<AbilityRoll, 6> result;
    for (auto &r : result)
    {
        for (auto &n : r.dice)
            n = roll_die(state, 6);
        r.discarded =
            static_cast<unsigned>(std::min_element(r.dice.begin(), r.dice.end()) - r.dice.begin());
    }
    return result;
}

std::optional<int> CreatorRules::ability_score(const CharacterDraft &d, Ability ability) const
{
    if (ability_index(ability) >= d.assignment.size() || d.assignment[ability] > 6)
        throw std::runtime_error("Invalid ability assignment");
    if (!d.rolled || d.assignment[ability] == 6)
        return std::nullopt;
    const auto options = adjustments(d.background);
    if (d.adjustment >= options.size())
        throw std::runtime_error("Invalid background bonuses");
    const auto score =
        d.rolls[d.assignment[ability]].total() + options[d.adjustment].bonuses[ability];
    if (score > 20)
        throw std::runtime_error("Background bonuses cannot raise a score above 20");
    return score;
}

CharacterSheet CreatorRules::evaluate(const CharacterDraft &d, NameRequirement name) const
{
    const auto label = [&](CreationField field, const std::string & id)
    {
        const auto options = choices(field);
        const auto found = std::find_if(options.begin(), options.end(),
                                        [&](const auto & c)
        {
            return c.id == id;
        });
        if (found == options.end())
            throw std::runtime_error("Invalid character selection: " + id);
        return found->label;
    };
    CharacterSheet s;
    s.identity = identity();
    s.name = d.name;
    s.race = label(CreationField::race, d.race);
    s.gender = label(CreationField::gender, d.gender);
    s.character_class = label(CreationField::character_class, d.character_class);
    const auto klass = detail::class_from_id(d.character_class);
    s.alignment = label(CreationField::alignment, d.alignment);
    s.background = label(CreationField::background, d.background);
    s.grants = detail::starting_grants(klass, d.race, d.background);
    const auto training = detail::training_grants(klass, d.race, d.background, d.training);
    s.grants.insert(s.grants.end(), training.begin(), training.end());
    if (name == NameRequirement::required && (d.name.empty() || d.name.size() > 160 ||
                         d.name.find_first_not_of(" \t\r\n") == std::string::npos ||
                         std::any_of(d.name.begin(), d.name.end(),
                                     [](unsigned char c)
{
    return c < 32 || c == 127;
})))
    throw std::runtime_error("Enter a name before finishing your character.");
    if (!d.rolled)
        throw std::runtime_error("Roll your six attribute scores first.");
    const auto options = adjustments(d.background);
    if (d.adjustment >= options.size())
        throw std::runtime_error("Invalid background bonuses");
    s.bonuses = options[d.adjustment].bonuses;
    s.ability_adjustments.push_back(
    {
        "background:" + d.background,
        s.background + " background",
        1,
        s.bonuses,
        {"{background} background", {{"background", s.background, true}}}});
    std::set<unsigned> used;
    for (const auto ability : all_abilities)
    {
        if (d.assignment[ability] >= 6 || !used.insert(d.assignment[ability]).second)
            throw std::runtime_error("Each roll must be assigned exactly once");
        s.base[ability] = d.rolls[d.assignment[ability]].total();
        s.scores[ability] = *ability_score(d, ability);
        s.modifiers[ability] = ability_modifier(s.scores[ability]);
    }
    const auto c = std::find_if(classes.begin(), classes.end(),
                                [&](const auto & c)
    {
        return c.character_class == klass;
    });
    s.hit_die = c->die;
    const int racial_hp = d.race == "dwarf" ? 1 : 0;
    // Core class traits, SRD 5.2.1. Single-class level-one creation.
    const auto trained = detail::class_save_proficiencies(c->character_class);
    for (const auto ability : all_abilities)
    {
        s.save_proficiencies[ability] = ability == trained[0] || ability == trained[1];
        s.saving_throws[ability] = s.modifiers[ability] + (s.save_proficiencies[ability] ? 2 : 0);
    }
    s.class_modifiers = "Source: " + s.character_class +
                        " class, level 1. Saving-throw training adds +2 proficiency to " +
                        ability_names[trained[0]] + " and " + ability_names[trained[1]] +
                        ".\nSource: " + s.character_class + " Hit Die and Constitution score " +
                        std::to_string(s.scores[Ability::constitution]) +
                        ". Starting HP: maximum d" +
                        std::to_string(s.hit_die) + " + Constitution modifier (" +
                        std::to_string(s.modifiers[Ability::constitution]) + ").";
    s.racial_modifiers =
        d.race == "dwarf" ? "Dwarven Toughness: +1 maximum HP."
        : d.race == "goliath"
        ? "Source: Goliath / Speed trait. Speed is 35 feet (5 feet above the default)."
        : d.race == "orc"
        ? "Orc / Adrenaline Rush: Bonus Action Dash and Temporary HP equal to proficiency bonus. Uses equal to proficiency bonus; all recover on a Short or Long Rest."
        : "No numeric racial modifiers are currently applied.";
    s.racial_modifiers += "\nOther racial traits and conditional effects are not implemented.";
    s.background_modifiers =
        "Source: " + s.background + " background, selected ability increases. " +
        options[d.adjustment].label + ". Other background features are not implemented.";
    s.hit_points = s.hit_die + s.modifiers[Ability::constitution] + racial_hp;
    s.hit_point_modifiers = {s.modifiers[Ability::constitution]};
    s.class_messages =
    {
        {
            "Source: {class} class, level 1. Saving-throw training adds +2 proficiency to {first} and {second}.",
            {   {"class", s.character_class, true},
                {"first", ability_names[trained[0]], true},
                {"second", ability_names[trained[1]], true}
            }
        },
        {
            "Source: {class} Hit Die and Constitution score {score}. Starting HP: maximum d{die} + Constitution modifier ({modifier}).",
            {   {"class", s.character_class, true},
                {"score", std::to_string(s.scores[Ability::constitution])},
                {"die", std::to_string(s.hit_die)},
                {"modifier", std::to_string(s.modifiers[Ability::constitution])}
            }
        }
    };
    if (klass == detail::CharacterClass::rogue)
    {
        s.class_modifiers +=
            "\nSneak Attack: once per turn, extra weapon damage is added automatically to the first eligible hit; the combat log shows the dice.";
        s.class_messages.push_back(
        {
            "Sneak Attack: once per turn, extra weapon damage is added automatically to the first eligible hit; the combat log shows the dice.",
            {}});
    }
    if (racial_hp)
        s.racial_messages.push_back({"Dwarven Toughness: +{hp} maximum HP.", {{"hp", "1"}}});
    else if (d.race == "goliath")
        s.racial_messages.push_back(
    {"Source: Goliath / Speed trait. Speed is 35 feet (5 feet above the default).", {}});
    else if (d.race == "orc")
        s.racial_messages.push_back(
    {
        "Orc / Adrenaline Rush: Bonus Action Dash and Temporary HP equal to proficiency bonus. Uses equal to proficiency bonus; all recover on a Short or Long Rest.",
        {}});
    else
        s.racial_messages.push_back({"No numeric racial modifiers are currently applied.", {}});
    if (d.race == "dwarf")
    {
        s.racial_modifiers +=
            "\nSource: Dwarf / Dwarven Resilience. Resistance to Poison damage (half, rounded down).";
        s.racial_messages.push_back(
        {
            "Source: Dwarf / Dwarven Resilience. Resistance to Poison damage (half, rounded down).",
            {}});
    }
    s.racial_messages.push_back(
    {"Other racial traits and conditional effects are not implemented.", {}});
    s.background_messages.push_back({"Source: {background} background, selected ability increases.",
        {{"background", s.background, true}}});
    for (const auto ability : all_abilities)
        if (s.bonuses[ability])
            s.background_messages.push_back(
        {
            "{ability} +{bonus}",
            {   {"ability", ability_names[ability], true},
                {"bonus", std::to_string(s.bonuses[ability])}
            }});
    s.background_messages.push_back({"Other background features are not implemented.", {}});
    s.hp_messages.push_back(
    {
        "{die} (maximum d{die}) {modifier} (Constitution) + {racial} (racial bonus) = {hp} HP",
        {   {"die", std::to_string(s.hit_die)},
            {"modifier", std::string(s.modifiers[Ability::constitution] < 0 ? "" : "+") +
                std::to_string(s.modifiers[Ability::constitution])},
            {"racial", std::to_string(racial_hp)},
            {"hp", std::to_string(s.hit_points)}
        }});
    s.hp_explanation = std::to_string(s.hit_die) + " (maximum d" + std::to_string(s.hit_die) +
                       ") " + (s.modifiers[Ability::constitution] < 0 ? "- " : "+ ") +
                       std::to_string(std::abs(s.modifiers[Ability::constitution])) +
                       " (Constitution)" +
                       (racial_hp ? " + 1 (Dwarven Toughness)" : "") + " = " +
                       std::to_string(s.hit_points) + " HP";
    s.training =
        detail::training_profile(s.grants, klass, d.race, d.background, s.level, s.scores);
    const auto spells = detail::starting_spell_grants(klass, d.cantrips);
    s.grants.insert(s.grants.end(), spells.begin(), spells.end());
    using enum detail::CharacterClass;
    if (klass == wizard)
        s.prepared_spells = {"magic_missile"};
    if (d.spells && klass == wizard)
    {
        std::erase_if(s.grants,
                      [](const auto & g)
        {
            return g.id == "spell:magic_missile";
        });
        s.prepared_spells.clear();
    }
    // Any class may learn Magic Initiate's spells; only casters prepare.
    if (d.spells)
        detail::apply_spell_choices(s, *d.spells, SpellChoiceContext::advancement,
                                    ChoiceCompleteness::partial);
    return s;
}
} // namespace

std::unique_ptr<rules::CharacterRules> character_rules()
{
    return std::make_unique<CreatorRules>();
}
} // namespace opengold::srd5
