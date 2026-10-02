#include "training.h"
#include "weapon_mastery.h"
#include "feature_grants.h"
#include <algorithm>
#include <set>
#include <stdexcept>
#include <tuple>

namespace opengold::srd5::detail
{
using namespace rules;

namespace
{
struct Skill
{
    std::string_view id, label;
    unsigned ability;
};

// SRD 5.2.1 p. 9. The query also accepts another governing ability when a rule
// calls for it; these are the ordinary character-sheet associations.
constexpr std::array skills{Skill{"acrobatics", "Acrobatics", 1},
    Skill{"animal_handling", "Animal Handling", 4},
    Skill{"arcana", "Arcana", 3},
    Skill{"athletics", "Athletics", 0},
    Skill{"deception", "Deception", 5},
    Skill{"history", "History", 3},
    Skill{"insight", "Insight", 4},
    Skill{"intimidation", "Intimidation", 5},
    Skill{"investigation", "Investigation", 3},
    Skill{"medicine", "Medicine", 4},
    Skill{"nature", "Nature", 3},
    Skill{"perception", "Perception", 4},
    Skill{"performance", "Performance", 5},
    Skill{"persuasion", "Persuasion", 5},
    Skill{"religion", "Religion", 3},
    Skill{"sleight_of_hand", "Sleight of Hand", 1},
    Skill{"stealth", "Stealth", 1},
    Skill{"survival", "Survival", 4}};

struct ClassSkills
{
    std::string_view id, label;
    unsigned count;
    std::vector<std::string_view> skills;
};

// SRD 5.2.1 Core Traits tables; an empty list denotes Bard's unrestricted list.
const std::array class_skills
{
    ClassSkills{
        "barbarian",
        "Barbarian skills",
        2,
        {"animal_handling", "athletics", "intimidation", "nature", "perception", "survival"}},
    ClassSkills{"bard", "Bard skills", 3, {}},
    ClassSkills{
        "cleric", "Cleric skills", 2, {"history", "insight", "medicine", "persuasion", "religion"}},
    ClassSkills{"druid",
        "Druid skills",
        2,
        {
            "animal_handling", "arcana", "insight", "medicine", "nature", "perception",
            "religion", "survival"
        }},
    ClassSkills{"fighter",
        "Fighter skills",
        2,
        {
            "acrobatics", "animal_handling", "athletics", "history", "insight", "intimidation",
            "persuasion", "perception", "survival"
        }},
    ClassSkills{"monk",
        "Monk skills",
        2,
        {"acrobatics", "athletics", "history", "insight", "religion", "stealth"}},
    ClassSkills{"paladin",
        "Paladin skills",
        2,
        {"athletics", "insight", "intimidation", "medicine", "persuasion", "religion"}},
    ClassSkills{"ranger",
        "Ranger skills",
        3,
        {
            "animal_handling", "athletics", "insight", "investigation", "nature", "perception",
            "stealth", "survival"
        }},
    ClassSkills{"rogue",
        "Rogue skills",
        4,
        {
            "acrobatics", "athletics", "deception", "insight", "intimidation", "investigation",
            "perception", "persuasion", "sleight_of_hand", "stealth"
        }},
    ClassSkills{"sorcerer",
        "Sorcerer skills",
        2,
        {"arcana", "deception", "insight", "intimidation", "persuasion", "religion"}},
    ClassSkills{
        "warlock",
        "Warlock skills",
        2,
        {"arcana", "deception", "history", "intimidation", "investigation", "nature", "religion"}},
    ClassSkills{
        "wizard",
        "Wizard skills",
        2,
        {"arcana", "history", "insight", "investigation", "medicine", "nature", "religion"}}};

constexpr std::string_view skilled = "feat:skilled";
constexpr std::string_view divine_order = "class:cleric:divine_order";
constexpr std::string_view rogue = "class:rogue", expertise = "class:rogue:expertise";

void require(bool ok)
{
    if (!ok)
        throw std::runtime_error("Invalid training choices or grant sources");
}

int modifier(int score)
{
    return score < 10 ? (score - 11) / 2 : (score - 10) / 2;
}

int proficiency(unsigned level)
{
    require(level >= 1 && level <= 20);
    return 2 + int((level - 1) / 4);
}

bool source(std::span<const FeatureGrant> grants, std::string_view id)
{
    return std::any_of(grants.begin(), grants.end(),
                       [&](const auto & g)
    {
        return g.id == id;
    });
}

std::vector<FeatureGrant> fixed(std::string_view background)
{
    std::vector<FeatureGrant> result;
    if (background == "sage")
        for (const auto id :
                {"skill:arcana", "skill:history"
                })
            result.push_back({id, "background:sage", 1, {}});
    if (background == "acolyte")
        for (const auto id :
                {"skill:insight", "skill:religion"
                })
            result.push_back({id, "background:acolyte", 1, {}});
    if (background == "soldier")
        for (const auto id :
                {"skill:athletics", "skill:intimidation"
                })
            result.push_back({id, "background:soldier", 1, {}});
    if (background == "criminal")
        for (const auto id :
                {"skill:sleight_of_hand", "skill:stealth"
                })
            result.push_back({id, "background:criminal", 1, {}});
    return result;
}

const std::vector<std::string> &selected(const TrainingChoices &choices, std::string_view id)
{
    static const std::vector<std::string> empty;
    const auto found = choices.find(std::string(id));
    return found == choices.end() ? empty : found->second;
}

void add_choices(std::vector<FeatureGrant> &grants, const TrainingChoices &choices,
                 const TrainingChoiceGroup &group, std::string_view prefix)
{
    const auto &values = selected(choices, group.id);
    require(values.size() <= group.count);
    std::set<std::string> seen;
    for (const auto &value : values)
    {
        require(seen.insert(value).second && std::any_of(group.options.begin(), group.options.end(),
                [&](const auto & o)
        {
            return o.id == value;
        }));
        grants.push_back({std::string(prefix) + value, group.id, 1, {}});
    }
}

std::vector<TrainingChoiceGroup> options(std::string_view klass, std::string_view background,
        const TrainingChoices &choices)
{
    std::vector<TrainingChoiceGroup> result;
    if (klass == "fighter")
        result.push_back({"class:fighter:fighting_style",
                          "Fighting Style",
                          1,
    {   {"defense", "Defense", "+1 AC while wearing armor."},
        {"archery", "Archery", "+2 to attack rolls with Ranged weapons."}
    },
    TrainingChoiceControl::single_selection});
    if (klass == "fighter")
        for (auto &group : result)
            if (group.id == "class:fighter:fighting_style")
                group.options.push_back(
            {
                "great_weapon_fighting", "Great Weapon Fighting",
                "Treat damage dice showing 1 or 2 as 3 with an eligible Melee weapon held in two hands."});
    if (klass == "fighter")
        for (auto &group : result)
            if (group.id == "class:fighter:fighting_style")
                group.options.push_back(
            {
                "two_weapon_fighting", "Two-Weapon Fighting",
                "Add your ability modifier to the extra attack granted by the Light property."});
    if (klass == "cleric")
        result.push_back({std::string(divine_order),
                          "Divine Order",
                          1,
    {   {"protector", "Protector", "Training with Martial weapons and Heavy armor."},
        {
            "thaumaturge", "Thaumaturge",
            "One extra Cleric cantrip; add your Wisdom modifier (minimum +1) to Intelligence (Arcana or Religion) checks."
        }
    },
    TrainingChoiceControl::single_selection});
    if (!klass.empty())
    {
        const auto data = std::find_if(class_skills.begin(), class_skills.end(),
                                       [&](const auto & c)
        {
            return c.id == klass;
        });
        require(data != class_skills.end());
        TrainingChoiceGroup group{"class:" + std::string(klass),
                                  std::string(data->label),
                                  data->count,
                                  {},
                                  TrainingChoiceControl::checkboxes,
                                  "class_skills"};
        for (const auto &skill : skills)
            if (data->skills.empty() ||
                    std::find(data->skills.begin(), data->skills.end(), skill.id) != data->skills.end())
                group.options.push_back({std::string(skill.id), std::string(skill.label), {}});
        result.push_back(std::move(group));
    }
    if (klass == "rogue")
    {
        TrainingChoiceGroup group{std::string(expertise), "Rogue Expertise", 2, {}};
        const auto known = fixed(background);
        const auto &picked = selected(choices, rogue);
        for (const auto &s : skills)
            if (source(known, "skill:" + std::string(s.id)) ||
                    std::find(picked.begin(), picked.end(), s.id) != picked.end())
                group.options.push_back({std::string(s.id), std::string(s.label), {}});
        result.push_back(std::move(group));
    }
    {
        auto group = mastery_options(klass, 1);
        const auto &later = selected(choices, mastery_options(klass, 4).id);
        std::erase_if(group.options,
                      [&](const auto & option)
        {
            return std::find(later.begin(), later.end(), option.id) != later.end();
        });
        if (group.count)
            result.push_back(std::move(group));
    }
    return result;
}

AbilityCheckModifier check_modifier(std::span<const FeatureGrant> grants,
                                    const std::array<int, 6> &scores, unsigned level,
                                    unsigned ability, std::string_view skill)
{
    require(ability < 6 && std::all_of(
                scores.begin(), scores.end(),
                [](int score)
    {
        return score >= 3 && score <= 20;
    }));
    const auto pb = proficiency(level);
    require(skill.empty() || std::any_of(skills.begin(), skills.end(),
                                         [&](const auto & s)
    {
        return s.id == skill;
    }));
    const auto skill_id = "skill:" + std::string(skill),
               expert_id = "expertise:" + std::string(skill);
    const bool trained_skill = !skill.empty() && source(grants, skill_id);
    AbilityCheckModifier result;
    result.ability_modifier = modifier(scores[ability]);
    result.expertise = trained_skill && source(grants, expert_id);
    result.proficiency = result.expertise ? pb * 2 : trained_skill ? pb : 0;
    result.total = result.ability_modifier + result.proficiency;
    if (ability == 0 && skill == "athletics")
        for (const auto &g : grants)
            if (g.id == "feature:remarkable_athlete" &&
                    g.source_id == "subclass:fighter:champion" && g.level == 3 && level >= 3)
            {
                result.advantage = true;
                result.sources.push_back(g);
            }
    for (const auto &g : grants)
        if (!skill.empty() && (g.id == skill_id || g.id == expert_id))
            result.sources.push_back(g);
    if (ability == 3 && (skill == "arcana" || skill == "religion"))
        for (const auto &g : grants)
            if (g.id == "order:thaumaturge" && g.source_id == divine_order)
            {
                result.total += std::max(1, modifier(scores[4]));
                result.sources.push_back(g);
            }
    return result;
}
} // namespace

bool is_training_grant(const FeatureGrant &grant)
{
    return is_mastery_grant(grant) || grant.id.starts_with("skill:") ||
           grant.id.starts_with("expertise:") || grant.id.starts_with("order:");
}

std::vector<FeatureGrant> without_training(std::span<const FeatureGrant> grants)
{
    std::vector<FeatureGrant> result;
    for (const auto &g : grants)
        if (!is_training_grant(g))
            result.push_back(g);
    return result;
}

TrainingChoiceGroup scholar_options(std::span<const FeatureGrant> grants)
{
    TrainingChoiceGroup group{"class:wizard:scholar", "Scholar Expertise", 1, {}};
    group.acquired_level = 2;
    for (const auto &skill : skills)
        if ((skill.id == "arcana" || skill.id == "history" || skill.id == "investigation" ||
                skill.id == "medicine" || skill.id == "nature" || skill.id == "religion") &&
                source(grants, "skill:" + std::string(skill.id)))
            group.options.push_back({std::string(skill.id), std::string(skill.label), {}});
    return group;
}

// Skilled grants three skill proficiencies chosen from the whole catalog. Skills the
// character already holds are omitted, which is what makes an already-known pick
// fail the membership check rather than silently add a second identical grant.
TrainingChoiceGroup skilled_options(std::span<const FeatureGrant> grants)
{
    TrainingChoiceGroup group{std::string(skilled),             "Skilled Training", 3, {},
                              TrainingChoiceControl::checkboxes};
    group.acquired_level = 4;
    for (const auto &s : skills)
        if (!source(grants, "skill:" + std::string(s.id)))
            group.options.push_back({"skill:" + std::string(s.id), std::string(s.label), {}});
    return group;
}

std::vector<TrainingChoiceGroup> training_options(const CharacterDraft &draft)
{
    return options(draft.character_class, draft.background, draft.training);
}

std::vector<FeatureGrant> training_grants(std::string_view klass, std::string_view background,
        const TrainingChoices &choices)
{
    auto result = fixed(background);
    const auto groups = options(klass, background, choices);
    for (const auto &[id, values] : choices)
        require(std::any_of(groups.begin(), groups.end(),
                            [&](const auto & g)
    {
        return g.id == id;
    }) &&
    !values.empty());
    for (const auto &group : groups)
        add_choices(
            result, choices, group,
            group.id.ends_with(":weapon_mastery")        ? "mastery:"
            : group.id == "class:fighter:fighting_style" ? "feat:"
            : group.id == divine_order                   ? "order:"
            : group.id == "class:" + std::string(klass)  ? "skill:"
            : "expertise:");
    return result;
}

TrainingChoices training_choices(std::span<const FeatureGrant> grants, std::string_view klass,
                                 std::string_view background)
{
    auto required = fixed(background);
    TrainingChoices choices;
    std::vector<FeatureGrant> actual;
    // Style selections emit feats; keep them in feature validation as well.
    for (const auto &grant : grants)
        if (is_training_grant(grant) || grant.source_id == "class:fighter:fighting_style")
        {
            if (is_mastery_grant(grant) && grant.level == 4)
            {
                choices[grant.source_id].push_back(grant.id.substr(8));
                continue;
            }
            // Skilled proficiencies are chosen at level four and keep the whole
            // prefixed skill id.
            if (grant.source_id == skilled)
            {
                require(grant.level == 4 &&
                        grant.choices.empty());
                auto &selected = choices[grant.source_id];
                require(selected.size() < 3 &&
                        std::find(selected.begin(), selected.end(), grant.id) == selected.end());
                // A proficiency already held from any other source is not offered,
                // so asserting it here is an invalid save rather than a second grant.
                require(std::none_of(grants.begin(), grants.end(),
                                     [&](const auto & other)
                {
                    return other.id == grant.id && other.source_id != grant.source_id;
                }));
                selected.push_back(grant.id);
                continue;
            }
            if (grant.source_id == "class:wizard:scholar")
            {
                require(klass == "wizard" &&
                        grant.level == 2 && grant.choices.empty());
                const auto group = scholar_options(grants);
                require(std::any_of(group.options.begin(), group.options.end(),
                                    [&](const auto & option)
                {
                    return grant.id == "expertise:" + option.id;
                }));
                auto &selected = choices[grant.source_id];
                require(selected.empty());
                selected.push_back(grant.id.substr(10));
                continue;
            }
            const bool replacement = klass == "fighter" &&
                                     grant.source_id == "class:fighter:fighting_style";
            require((grant.level == 1 || (replacement && grant.level <= 4)) &&
                    grant.choices.empty());
            actual.push_back(grant);
            if (replacement)
                actual.back().level = 1;
            const auto found = std::find(required.begin(), required.end(), grant);
            if (found != required.end())
            {
                required.erase(found);
                continue;
            }
            const auto prefix = grant.source_id.ends_with(":weapon_mastery")        ? "mastery:"
                                : grant.source_id == "class:fighter:fighting_style" ? "feat:"
                                : grant.source_id == divine_order                   ? "order:"
                                : grant.source_id == "class:" + std::string(klass)  ? "skill:"
                                : "expertise:";
            require(grant.id.starts_with(prefix));
            choices[grant.source_id].push_back(grant.id.substr(std::string_view(prefix).size()));
        }
    require(required.empty());
    auto starting = choices;
    starting.erase("class:wizard:scholar");
    starting.erase(std::string(skilled));
    starting.erase(mastery_options(klass, 4).id);
    auto expected = training_grants(klass, background, starting);
    const auto order = [](const FeatureGrant & a, const FeatureGrant & b)
    {
        return std::tie(a.id, a.source_id, a.level) < std::tie(b.id, b.source_id, b.level);
    };
    std::sort(actual.begin(), actual.end(), order);
    std::sort(expected.begin(), expected.end(), order);
    require(actual == expected);
    return choices;
}

TrainingProfile training_profile(std::span<const FeatureGrant> grants, std::string_view klass,
                                 std::string_view background, unsigned level,
                                 const std::array<int, 6> &scores)
{
    const auto choices = training_choices(grants, klass, background);
    const auto groups = options(klass, background, choices);
    TrainingProfile result;
    result.complete = true;
    for (const auto &group : groups)
        result.complete &= selected(choices, group.id).size() == group.count;
    const auto mastery = mastery_choices(grants, klass, level);
    if (level >= 4)
    {
        const auto extra = mastery_options(klass, 4);
        if (extra.count)
            result.complete &= selected(choices, extra.id).size() == extra.count;
    }
    for (const auto &g : grants)
        if (is_mastery_grant(g))
        {
            const auto *item = weapon(g.id.substr(8));
            result.masteries.push_back({std::string(item->key), std::string(item->label), {g}});
        }
    const auto &picked = selected(choices, skilled);
    const bool has_skilled = bool(source(grants, std::string(skilled)));
    require((picked.empty() && !has_skilled) || level >= 4);
    if (has_skilled)
        result.complete &= picked.size() == 3;
    const auto &scholar = selected(choices, "class:wizard:scholar");
    require(scholar.empty() || level >= 2);
    if (klass == "wizard" && level >= 2)
        result.complete &= scholar.size() == 1;
    for (const auto &s : skills)
    {
        const auto bonus = check_modifier(grants, scores, level, s.ability, s.id);
        result.skills.push_back({std::string(s.id), std::string(s.label), s.ability, bonus.total,
                                 source(grants, "skill:" + std::string(s.id)), bonus.expertise,
                                 bonus.sources});
    }
    return result;
}

AbilityCheckModifier ability_check(std::span<const FeatureGrant> grants, std::string_view klass,
                                   std::string_view background, unsigned level,
                                   const std::array<int, 6> &scores, unsigned ability,
                                   std::string_view skill)
{
    (void)training_profile(grants, klass, background, level, scores);
    return check_modifier(grants, scores, level, ability, skill);
}
} // namespace opengold::srd5::detail
