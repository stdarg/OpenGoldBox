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
const std::array<std::string, 6> ability_names{"STR", "DEX", "CON", "INT", "WIS", "CHA"};

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
    std::array<unsigned, 6> preset_ability_priority(std::string_view id,
            unsigned variant) const override
    {
        using enum detail::CharacterClass;
        const auto klass = detail::class_from_id(id);
        const unsigned primary =
            klass == wizard                                            ? 3
            : (klass == cleric || klass == druid)                      ? 4
            : (klass == bard || klass == sorcerer || klass == warlock) ? 5
            : (klass == monk || klass == ranger || klass == rogue ||
               (klass == fighter && variant % 2))
            ? 1
            : 0;
        const unsigned secondary = (klass == monk || klass == ranger) ? 4
                                   : klass == paladin                  ? 5
                                   : 2;
        std::vector<unsigned> priority{primary, secondary};
        for (unsigned n :
                {
                    2u, 1u, 4u, 0u, 3u, 5u
                })
            if (std::find(priority.begin(), priority.end(), n) == priority.end())
                priority.push_back(n);
        std::array<unsigned, 6> result{};
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
        if (klass == cleric || klass == paladin || klass == ranger || klass == sorcerer ||
                klass == warlock || klass == bard || klass == druid)
        {
            // These classes prepare from the class list; the selection never changes it.
            auto base = draft;
            base.spells = SpellChoices{};
            return detail::spell_choice_options(evaluate(base, NameRequirement::optional),
                                                SpellChoiceContext::advancement);
        }
        if (klass != wizard)
            return {};
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
        return options;
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
        return group;
    }

    AbilityCheckModifier ability_check(const CharacterSheet &sheet, Ability ability,
                                       std::string_view skill) const override
    {
        return detail::ability_check(sheet.grants, detail::class_of(sheet),
                                     detail::grant_source_id(sheet.background), sheet.level,
                                     sheet.scores, ability, skill);
    }
};

std::vector<CreationChoice> CreatorRules::choices(CreationField field) const
{
    switch (field)
    {
    case CreationField::race:
        return {{"dragonborn", "Dragonborn", "A draconic humanoid."},
            {"dwarf", "Dwarf", "A hardy folk. Dwarven Toughness adds 1 to starting HP."},
            {"elf", "Elf", "A folk with a connection to the Fey."},
            {"gnome", "Gnome", "A small, inventive folk."},
            {"goliath", "Goliath", "A folk with giant ancestry."},
            {"halfling", "Halfling", "A small, resourceful folk."},
            {"human", "Human", "A versatile and adaptable folk."},
            {"orc", "Orc", "A resilient, determined folk."},
            {"tiefling", "Tiefling", "A folk with a fiendish legacy."}};
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
    std::array<unsigned, 3> allowed;
    if (background == "acolyte")
        allowed = {3, 4, 5};
    else if (background == "criminal")
        allowed = {1, 2, 3};
    else if (background == "sage")
        allowed = {2, 3, 4};
    else if (background == "soldier")
        allowed = {0, 1, 2};
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
    for (auto n : allowed)
        a.bonuses[n] = 1;
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
        result.description += ability_names[ability_index(ability)] + " 13";
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
    const auto index = ability_index(ability);
    if (index >= d.assignment.size() || d.assignment[index] > 6)
        throw std::runtime_error("Invalid ability assignment");
    if (!d.rolled || d.assignment[index] == 6)
        return std::nullopt;
    const auto options = adjustments(d.background);
    if (d.adjustment >= options.size())
        throw std::runtime_error("Invalid background bonuses");
    const auto score =
        d.rolls[d.assignment[index]].total() + options[d.adjustment].bonuses[index];
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
    const auto training = detail::training_grants(klass, d.background, d.training);
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
    for (unsigned i = 0; i < 6; ++i)
    {
        if (d.assignment[i] >= 6 || !used.insert(d.assignment[i]).second)
            throw std::runtime_error("Each roll must be assigned exactly once");
        s.base[i] = d.rolls[d.assignment[i]].total();
        s.scores[i] = *ability_score(d, static_cast<Ability>(i));
        s.modifiers[i] = ability_modifier(s.scores[i]);
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
    for (unsigned i = 0; i < 6; ++i)
    {
        s.save_proficiencies[i] = i == trained[0] || i == trained[1];
        s.saving_throws[i] = s.modifiers[i] + (s.save_proficiencies[i] ? 2 : 0);
    }
    s.class_modifiers = "Source: " + s.character_class +
                        " class, level 1. Saving-throw training adds +2 proficiency to " +
                        ability_names[trained[0]] + " and " + ability_names[trained[1]] +
                        ".\nSource: " + s.character_class + " Hit Die and Constitution score " +
                        std::to_string(s.scores[2]) + ". Starting HP: maximum d" +
                        std::to_string(s.hit_die) + " + Constitution modifier (" +
                        std::to_string(s.modifiers[2]) + ").";
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
    s.hit_points = s.hit_die + s.modifiers[2] + racial_hp;
    s.hit_point_modifiers = {s.modifiers[2]};
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
                {"score", std::to_string(s.scores[2])},
                {"die", std::to_string(s.hit_die)},
                {"modifier", std::to_string(s.modifiers[2])}
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
    for (unsigned i = 0; i < 6; ++i)
        if (s.bonuses[i])
            s.background_messages.push_back(
        {
            "{ability} +{bonus}",
            {{"ability", ability_names[i], true}, {"bonus", std::to_string(s.bonuses[i])}}});
    s.background_messages.push_back({"Other background features are not implemented.", {}});
    s.hp_messages.push_back(
    {
        "{die} (maximum d{die}) {modifier} (Constitution) + {racial} (racial bonus) = {hp} HP",
        {   {"die", std::to_string(s.hit_die)},
            {"modifier", std::string(s.modifiers[2] < 0 ? "" : "+") + std::to_string(s.modifiers[2])},
            {"racial", std::to_string(racial_hp)},
            {"hp", std::to_string(s.hit_points)}
        }});
    s.hp_explanation = std::to_string(s.hit_die) + " (maximum d" + std::to_string(s.hit_die) +
                       ") " + (s.modifiers[2] < 0 ? "- " : "+ ") +
                       std::to_string(std::abs(s.modifiers[2])) + " (Constitution)" +
                       (racial_hp ? " + 1 (Dwarven Toughness)" : "") + " = " +
                       std::to_string(s.hit_points) + " HP";
    s.training =
        detail::training_profile(s.grants, klass, d.background, s.level, s.scores);
    const auto spells = detail::starting_spell_grants(klass, d.cantrips);
    s.grants.insert(s.grants.end(), spells.begin(), spells.end());
    using enum detail::CharacterClass;
    if (klass == wizard)
        s.prepared_spells = {"magic_missile"};
    if (d.spells && (klass == cleric || klass == paladin || klass == ranger ||
                     klass == sorcerer || klass == warlock || klass == bard || klass == druid))
        detail::apply_spell_choices(s, *d.spells, SpellChoiceContext::advancement,
                                    ChoiceCompleteness::partial);
    else if (d.spells)
    {
        if (klass != wizard)
            throw std::runtime_error("Spell choices require a spellcasting class");
        std::erase_if(s.grants,
                      [](const auto & g)
        {
            return g.id == "spell:magic_missile";
        });
        s.prepared_spells.clear();
        detail::apply_spell_choices(s, *d.spells, SpellChoiceContext::advancement,
                                    ChoiceCompleteness::partial);
    }
    return s;
}
} // namespace

std::unique_ptr<rules::CharacterRules> character_rules()
{
    return std::make_unique<CreatorRules>();
}
} // namespace opengold::srd5
