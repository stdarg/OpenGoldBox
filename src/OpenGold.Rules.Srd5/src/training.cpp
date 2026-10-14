#include "training.h"
#include "weapon_mastery.h"
#include "feature_grants.h"
#include "spell_access.h"
#include "spell_table.h"
#include <algorithm>
#include <cctype>
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
    Ability ability;
};

// SRD 5.2.1 p. 9. The query also accepts another governing ability when a rule
// calls for it; these are the ordinary character-sheet associations.
constexpr std::array skills{Skill{"acrobatics", "Acrobatics", Ability::dexterity},
    Skill{"animal_handling", "Animal Handling", Ability::wisdom},
    Skill{"arcana", "Arcana", Ability::intelligence},
    Skill{"athletics", "Athletics", Ability::strength},
    Skill{"deception", "Deception", Ability::charisma},
    Skill{"history", "History", Ability::intelligence},
    Skill{"insight", "Insight", Ability::wisdom},
    Skill{"intimidation", "Intimidation", Ability::charisma},
    Skill{"investigation", "Investigation", Ability::intelligence},
    Skill{"medicine", "Medicine", Ability::wisdom},
    Skill{"nature", "Nature", Ability::intelligence},
    Skill{"perception", "Perception", Ability::wisdom},
    Skill{"performance", "Performance", Ability::charisma},
    Skill{"persuasion", "Persuasion", Ability::charisma},
    Skill{"religion", "Religion", Ability::intelligence},
    Skill{"sleight_of_hand", "Sleight of Hand", Ability::dexterity},
    Skill{"stealth", "Stealth", Ability::dexterity},
    Skill{"survival", "Survival", Ability::wisdom}};

struct ClassSkills
{
    std::string_view id, label;
    unsigned count;
    std::vector<std::string_view> skills;
};

// SRD 5.2.1 Core Traits tables; an empty list denotes Bard's unrestricted list.
// Built on first use: its vectors need dynamic initialization, which another
// file's static initializer could otherwise see unfinished (Effective C++
// Item 4).
const auto &class_skills()
{
    static const std::array table
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
    return table;
}

constexpr std::string_view skilled = "feat:skilled";
// Lessons of the First Ones' Skilled: its three skills' source.
constexpr std::string_view lessons_skilled = "invocation:lessons_skilled";
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
    return 2 + static_cast<int>((level - 1) / 4);
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

// Returns a reference into `choices`, so a temporary is refused: its result
// would dangle at the end of the statement (Effective C++ Item 21).
const std::vector<std::string> &selected(TrainingChoices &&, std::string_view) = delete;

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

// A species id ("elf") from an id or a label ("Elf").
std::string species_id(std::string_view race)
{
    std::string id(race);
    std::transform(id.begin(), id.end(), id.begin(), [](unsigned char c)
    {
        return static_cast<char>(std::tolower(c));
    });
    return id;
}

TrainingChoiceGroup one_of(std::string id, std::string label, std::vector<CreationChoice> options)
{
    return {std::move(id), std::move(label), 1, std::move(options),
            TrainingChoiceControl::single_selection};
}

// The choices a species makes at creation (SRD 5.2.1 pp. 84-86). Drow
// (DROW-1), Darkvision and the spells DM-1 removed are not offered.
std::vector<TrainingChoiceGroup> species_options(std::string_view race,
        std::string_view background, const TrainingChoices &choices)
{
    const auto species = species_id(race);
    std::vector<TrainingChoiceGroup> result;
    // The ability an Elf's or a Tiefling's lineage spells are cast with.
    const auto ability = [&]
    {
        return one_of("species:" + species + ":ability", "Spellcasting ability",
        {   {"intelligence", "Intelligence", "Attack rolls and save DCs of your lineage's spells."},
            {"wisdom", "Wisdom", "Attack rolls and save DCs of your lineage's spells."},
            {"charisma", "Charisma", "Attack rolls and save DCs of your lineage's spells."}
        });
    };
    const auto size = [&]
    {
        return one_of("species:" + species + ":size", "Size",
        {   {"medium", "Medium", {}},
            {"small", "Small", {}}
        });
    };
    if (species == "elf")
    {
        result.push_back(one_of("species:elf:lineage", "Elven Lineage",
        {   {"high_elf", "High Elf", "A Wizard cantrip of your choice."},
            {"wood_elf", "Wood Elf", "Speed 35 feet; Longstrider from level 3."}
        }));
        result.push_back(one_of("species:elf:keen_senses", "Keen Senses",
        {{"insight", "Insight", {}}, {"perception", "Perception", {}}, {"survival", "Survival", {}}}));
        result.push_back(ability());
        // A High Elf's Wizard cantrip (it takes the removed Prestidigitation's place, DM-1).
        const auto &lineage = selected(choices, "species:elf:lineage");
        if (std::find(lineage.begin(), lineage.end(), "high_elf") != lineage.end())
        {
            auto cantrip = starting_cantrip_options(CharacterClass::wizard);
            result.push_back(one_of("species:elf:cantrip", "High Elf cantrip",
                                    std::move(cantrip.options)));
        }
    }
    if (species == "tiefling")
    {
        result.push_back(one_of("species:tiefling:legacy", "Fiendish Legacy",
        {   {"abyssal", "Abyssal", "Resistance to Poison; Poison Spray; Ray of Sickness from level 3."},
            {"chthonic", "Chthonic", "Resistance to Necrotic; Chill Touch; False Life from level 3."},
            {"infernal", "Infernal", "Resistance to Fire; Fire Bolt; Hellish Rebuke from level 3."}
        }));
        result.push_back(size());
        result.push_back(ability());
    }
    if (species == "dragonborn")
        result.push_back(one_of("species:dragonborn:ancestry", "Draconic Ancestry",
        {   {"black", "Black", "Acid"}, {"blue", "Blue", "Lightning"}, {"brass", "Brass", "Fire"},
            {"bronze", "Bronze", "Lightning"}, {"copper", "Copper", "Acid"}, {"gold", "Gold", "Fire"},
            {"green", "Green", "Poison"}, {"red", "Red", "Fire"}, {"silver", "Silver", "Cold"},
            {"white", "White", "Cold"}
        }));
    if (species == "goliath")
        result.push_back(one_of("species:goliath:ancestry", "Giant Ancestry",
        {   {"cloud", "Cloud's Jaunt", "Bonus Action: teleport up to 30 feet to a space you can see."},
            {"fire", "Fire's Burn", "An attack hit deals 1d10 more Fire damage."},
            {"frost", "Frost's Chill", "An attack hit deals 1d6 more Cold damage and slows the target by 10 feet."},
            {"hill", "Hill's Tumble", "An attack hit knocks a Large or smaller target Prone."},
            {"stone", "Stone's Endurance", "Reaction when hit: reduce the damage by 1d12 + your Constitution modifier."},
            {"storm", "Storm's Thunder", "Reaction when damaged: deal 1d8 Thunder damage to the attacker within 60 feet."}
        }));
    if (species == "human")
    {
        result.push_back(size());
        TrainingChoiceGroup skillful = one_of("species:human:skillful", "Skillful", {});
        skillful.control = TrainingChoiceControl::checkboxes;
        for (const auto &skill : skills)
            skillful.options.push_back({std::string(skill.id), std::string(skill.label), {}});
        result.push_back(std::move(skillful));
        // Versatile: an Origin feat, other than the one the background gives.
        // Skilled comes first, the creator's default: it changes no fight.
        auto versatile = one_of("species:human:versatile", "Versatile",
        {   {"skilled", "Skilled", "Training in three skills of your choice."},
            {"alert", "Alert", "Add your Proficiency Bonus to Initiative, and swap Initiative with an ally."},
            {"magic_initiate_cleric", "Magic Initiate (Cleric)", "Two Cleric cantrips and a level 1 Cleric spell, cast once per Long Rest without a slot."},
            {"magic_initiate_druid", "Magic Initiate (Druid)", "Two Druid cantrips and a level 1 Druid spell, cast once per Long Rest without a slot."},
            {"magic_initiate_wizard", "Magic Initiate (Wizard)", "Two Wizard cantrips and a level 1 Wizard spell, cast once per Long Rest without a slot."},
            {"savage_attacker", "Savage Attacker", "Once per turn, roll a weapon's damage dice twice and use either roll."}
        });
        std::erase_if(versatile.options, [&](const auto & option)
        {
            return background_feat(background) == "feat:" + option.id;
        });
        result.push_back(std::move(versatile));
        // Skilled's three skills, apart from the one Skillful took.
        const auto &feat = selected(choices, "species:human:versatile");
        if (std::find(feat.begin(), feat.end(), "skilled") != feat.end())
        {
            TrainingChoiceGroup skilled = one_of("species:human:skilled", "Skilled", {});
            skilled.count = 3;
            skilled.control = TrainingChoiceControl::checkboxes;
            const auto &skillful_pick = selected(choices, "species:human:skillful");
            for (const auto &skill : skills)
                if (std::find(skillful_pick.begin(), skillful_pick.end(), skill.id) ==
                        skillful_pick.end())
                    skilled.options.push_back({std::string(skill.id), std::string(skill.label), {}});
            result.push_back(std::move(skilled));
        }
    }
    return result;
}

// The grant prefix of a species choice group's picks.
std::string_view species_prefix(std::string_view group)
{
    return group.ends_with(":lineage")       ? "lineage:"
           : group.ends_with(":ability")     ? "spellcasting_ability:"
           : group.ends_with(":cantrip")     ? "cantrip:"
           : group.ends_with(":legacy")      ? "legacy:"
           : group.ends_with(":size")        ? "size:"
           : group.ends_with(":keen_senses") || group.ends_with(":skillful") ||
             group.ends_with(":skilled") ? "skill:"
           : group.ends_with(":versatile")   ? "feat:"
           : group == "species:dragonborn:ancestry" ? "dragon:"
           : "giant:";
}

// A draft offers its background's choices before any class is chosen.
std::vector<TrainingChoiceGroup> options(std::optional<CharacterClass> klass,
        std::string_view race, std::string_view background, const TrainingChoices &choices)
{
    std::vector<TrainingChoiceGroup> result = species_options(race, background, choices);
    if (klass == CharacterClass::fighter)
        result.push_back({"class:fighter:fighting_style",
                          "Fighting Style",
                          1,
    {   {"defense", "Defense", "+1 AC while wearing armor."},
        {"archery", "Archery", "+2 to attack rolls with Ranged weapons."}
    },
    TrainingChoiceControl::single_selection});
    if (klass == CharacterClass::fighter)
        for (auto &group : result)
            if (group.id == "class:fighter:fighting_style")
                group.options.push_back(
            {
                "great_weapon_fighting", "Great Weapon Fighting",
                "Treat damage dice showing 1 or 2 as 3 with an eligible Melee weapon held in two hands."});
    if (klass == CharacterClass::fighter)
        for (auto &group : result)
            if (group.id == "class:fighter:fighting_style")
                group.options.push_back(
            {
                "two_weapon_fighting", "Two-Weapon Fighting",
                "Add your ability modifier to the extra attack granted by the Light property."});
    if (klass == CharacterClass::warlock)
        result.push_back(invocation_options(1, {}, "class:warlock:invocations"));
    if (klass == CharacterClass::druid)
        result.push_back({"class:druid:primal_order",
                          "Primal Order",
                          1,
    {   {"magician", "Magician", "One extra Druid cantrip."},
        {"warden", "Warden", "Training with Martial weapons and Medium armor."}
    },
    TrainingChoiceControl::single_selection});
    if (klass == CharacterClass::cleric)
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
    if (klass)
    {
        const auto data = std::find_if(class_skills().begin(), class_skills().end(),
                                       [&](const auto & c)
        {
            return c.id == class_id(*klass);
        });
        require(data != class_skills().end());
        TrainingChoiceGroup group{"class:" + std::string(class_id(*klass)),
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
    if (klass == CharacterClass::rogue)
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
    if (klass)
    {
        auto group = mastery_options(*klass, 1);
        const auto &later = selected(choices, mastery_options(*klass, 4).id);
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
                                    const AbilityArray<int> &scores, unsigned level,
                                    Ability ability, std::string_view skill)
{
    require(ability_index(ability) < scores.size() && std::all_of(
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
    if (ability == Ability::strength && skill == "athletics")
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
    if (ability == Ability::intelligence && (skill == "arcana" || skill == "religion"))
        for (const auto &g : grants)
            if (g.id == "order:thaumaturge" && g.source_id == divine_order)
            {
                result.total += std::max(1, modifier(scores[Ability::wisdom]));
                result.sources.push_back(g);
            }
    return result;
}
} // namespace

bool is_training_grant(const FeatureGrant &grant)
{
    return is_mastery_grant(grant) || grant.id.starts_with("skill:") ||
           grant.id.starts_with("expertise:") || grant.id.starts_with("order:") ||
           grant.id.starts_with("metamagic:") || grant.id.starts_with("invocation:") ||
           grant.id.starts_with("land:") ||
           (grant.source_id.starts_with("species:") &&
            (grant.id.starts_with("lineage:") || grant.id.starts_with("legacy:") ||
             grant.id.starts_with("size:") || grant.id.starts_with("dragon:") ||
             grant.id.starts_with("giant:") || grant.id.starts_with("spellcasting_ability:") ||
             grant.id.starts_with("cantrip:")));
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
    {
        // Expertise from another source, such as a training hall, is not offered twice.
        const auto expertise = "expertise:" + std::string(skill.id);
        const bool expert = std::any_of(grants.begin(), grants.end(), [&](const auto & g)
        {
            return g.id == expertise && g.source_id != group.id;
        });
        if ((skill.id == "arcana" || skill.id == "history" || skill.id == "investigation" ||
                skill.id == "medicine" || skill.id == "nature" || skill.id == "religion") &&
                source(grants, "skill:" + std::string(skill.id)) && !expert)
            group.options.push_back({std::string(skill.id), std::string(skill.label), {}});
    }
    return group;
}

// Eldritch Invocations a Warlock of the given level qualifies for (SRD 5.2.1
// pp. 72-74), less those it holds from another entitlement. Agonizing Blast,
// Eldritch Spear and Repelling Blast improve Eldritch Blast, the Warlock
// cantrip they suit. Invocations that only cast removed spells and Pact of the
// Chain are not offered.
namespace
{
// The Warlock cantrips that deal damage, from the class list or Pact of the
// Tome: those a blast invocation can improve (SRD 5.2.1 p. 72).
std::vector<const SpellDef *> damage_cantrips(std::span<const FeatureGrant> grants)
{
    std::vector<const SpellDef *> result;
    for (const auto &g : grants)
        if (g.id.starts_with("spell:") && (g.source_id == "class:warlock:pact_magic" ||
                                           g.source_id == "invocation:pact_of_the_tome"))
            if (const auto *spell = find_spell(g.id.substr(6)); spell && !spell->level &&
                    spell->dice.count)
                result.push_back(spell);
    std::stable_partition(result.begin(), result.end(), [](const auto * spell)
    {
        return spell->id == "eldritch_blast";
    });
    return result;
}
} // namespace

TrainingChoiceGroup invocation_options(unsigned level, std::span<const FeatureGrant> grants,
                                       std::string_view source)
{
    TrainingChoiceGroup group{std::string(source), "Eldritch Invocations", level == 1 ? 1u : 2u,
                              {}, TrainingChoiceControl::checkboxes};
    group.acquired_level = level;
    struct Invocation
    {
        std::string_view id, label, description;
        unsigned level;
    };
    static constexpr std::array<Invocation, 12> all{{
        {"armor_of_shadows", "Armor of Shadows", "Cast Mage Armor on yourself without a spell slot.", 1},
        {"devils_sight", "Devil's Sight", "See normally in magical and nonmagical Darkness.", 2},
        {"eldritch_mind", "Eldritch Mind", "Advantage on Constitution saves to keep Concentration.", 1},
        {"fiendish_vigor", "Fiendish Vigor", "Cast False Life on yourself without a slot, for its highest result.", 2},
        {"lessons_alert", "Lessons of the First Ones: Alert", "Gain the Alert feat.", 2},
        {"lessons_savage_attacker", "Lessons of the First Ones: Savage Attacker", "Gain the Savage Attacker feat.", 2},
        {"lessons_skilled", "Lessons of the First Ones: Skilled", "Gain the Skilled feat: training in three skills.", 2},
        {"lessons_magic_initiate_cleric", "Lessons of the First Ones: Magic Initiate (Cleric)", "Gain the Magic Initiate feat with the Cleric spell list.", 2},
        {"lessons_magic_initiate_druid", "Lessons of the First Ones: Magic Initiate (Druid)", "Gain the Magic Initiate feat with the Druid spell list.", 2},
        {"lessons_magic_initiate_wizard", "Lessons of the First Ones: Magic Initiate (Wizard)", "Gain the Magic Initiate feat with the Wizard spell list.", 2},
        {"pact_of_the_blade", "Pact of the Blade", "Your melee weapon is your pact weapon: proficiency, Charisma for its attack and damage rolls, and Necrotic, Psychic or Radiant damage if you like.", 1},
        {"pact_of_the_tome", "Pact of the Tome", "Three cantrips and two level 1 spells from any class's list, prepared as Warlock spells.", 1}}};
    const auto held = [&](const std::string & id, const std::string & feat)
    {
        return std::any_of(grants.begin(), grants.end(), [&](const auto & g)
        {
            return (g.id == id && g.source_id != source) || (!feat.empty() && g.id == feat);
        });
    };
    for (const auto &invocation : all)
    {
        // Magic Initiate and Skilled repeat only through another feat, so a
        // list a feat already gave is not offered again.
        const std::string id(invocation.id);
        const auto feat = id.starts_with("lessons_magic_initiate_") || id == "lessons_skilled"
                          ? "feat:" + id.substr(8)
                          : std::string();
        if (invocation.level <= level && !held("invocation:" + id, feat))
            group.options.push_back({id, std::string(invocation.label),
                                     std::string(invocation.description)});
    }
    // The blast invocations each improve one damaging Warlock cantrip, and may
    // be taken again for another (SRD 5.2.1 pp. 71-73). Repelling Blast needs
    // a cantrip that can hit; Eldritch Spear one with a range of 10 feet or more.
    if (level >= 2)
        for (const auto *cantrip : damage_cantrips(grants))
        {
            const std::string name(cantrip->label), key(cantrip->id);
            struct Blast
            {
                std::string_view id, label, description;
                bool open;
            };
            for (const auto &blast :
                    {Blast{"agonizing_blast", "Agonizing Blast", "Add your Charisma modifier to the cantrip's damage.", true},
                     Blast{"eldritch_spear", "Eldritch Spear", "The cantrip reaches 30 feet farther per Warlock level.", cantrip->range >= 10},
                     Blast{"repelling_blast", "Repelling Blast", "A hit with the cantrip pushes a Large or smaller creature 10 feet away.", cantrip->pattern == SpellPattern::spell_attack}
                    })
            {
                const auto id = std::string(blast.id) + ":" + key;
                if (blast.open && !held("invocation:" + id, {}))
                    group.options.push_back({id, std::string(blast.label) + " (" + name + ")",
                                             std::string(blast.description)});
            }
        }
    // Alphabetical by invocation, Eldritch Blast's first: the order the
    // default level-up takes them in.
    std::stable_sort(group.options.begin(), group.options.end(), [](const auto & a, const auto & b)
    {
        const auto name = [](std::string_view id)
        {
            return id.substr(0, id.find(':'));
        };
        return name(a.id) < name(b.id);
    });
    return group;
}

// College of Lore's Bonus Proficiencies: three skills not yet held.
TrainingChoiceGroup lore_options(std::span<const FeatureGrant> grants)
{
    TrainingChoiceGroup group{"subclass:bard:lore", "Bonus Proficiencies", 3, {},
                              TrainingChoiceControl::checkboxes};
    group.acquired_level = 3;
    for (const auto &skill : skills)
    {
        const auto id = "skill:" + std::string(skill.id);
        if (std::none_of(grants.begin(), grants.end(), [&](const auto & g)
    {
        return g.id == id && g.source_id != group.id;
    }))
        group.options.push_back({std::string(skill.id), std::string(skill.label), {}});
    }
    return group;
}

// Hunter's Prey: the Hunter Ranger's choice at level three (SRD 5.2.1 p. 59).
TrainingChoiceGroup hunters_prey_options()
{
    TrainingChoiceGroup group{"subclass:ranger:hunter", "Hunter's Prey", 1, {},
                              TrainingChoiceControl::single_selection};
    group.acquired_level = 3;
    group.options = {
        {   "colossus_slayer", "Colossus Slayer",
            "Once per turn, a weapon hit deals 1d8 extra damage to a creature missing any Hit Points."
        },
        {   "horde_breaker", "Horde Breaker",
            "Once per turn, after a weapon attack, attack another creature within 5 feet of the first target."
        }};
    return group;
}

// Circle of the Land: the land type that picks the Circle Spells (SRD 5.2.1 p. 46).
TrainingChoiceGroup land_options()
{
    TrainingChoiceGroup group{"subclass:druid:land", "Circle of the Land", 1, {},
                              TrainingChoiceControl::single_selection};
    group.acquired_level = 3;
    group.options = {
        {"arid", "Arid Land", "Circle Spells: Blur, Burning Hands and Fire Bolt."},
        {"polar", "Polar Land", "Circle Spells: Fog Cloud, Hold Person and Ray of Frost."},
        {"temperate", "Temperate Land", "Circle Spells: Misty Step, Shocking Grasp and Sleep."},
        {"tropical", "Tropical Land", "Circle Spells: Acid Splash, Ray of Sickness and Web."}};
    return group;
}

// Metamagic: two options at Sorcerer level two (SRD 5.2.1 pp. 66-67).
TrainingChoiceGroup metamagic_options()
{
    TrainingChoiceGroup group{"class:sorcerer:metamagic", "Metamagic", 2, {},
                              TrainingChoiceControl::checkboxes};
    group.acquired_level = 2;
    group.options = {
        {"careful", "Careful Spell", "1 Sorcery Point: allies in your save spell's area succeed and take no half damage."},
        {"distant", "Distant Spell", "1 Sorcery Point: double a spell's range, or Touch becomes 30 feet."},
        {"empowered", "Empowered Spell", "1 Sorcery Point: reroll your lowest damage dice, up to your Charisma modifier."},
        {"extended", "Extended Spell", "1 Sorcery Point: double a Concentration spell's duration and gain Advantage to keep it."},
        {"heightened", "Heightened Spell", "2 Sorcery Points: one target has Disadvantage on its save against the spell."},
        {"quickened", "Quickened Spell", "2 Sorcery Points: cast an Action spell as a Bonus Action."},
        {"seeking", "Seeking Spell", "1 Sorcery Point: reroll a missed spell attack roll."},
        {"transmuted", "Transmuted Spell", "1 Sorcery Point: change Acid, Cold, Fire, Lightning, Poison or Thunder damage to another of them."},
        {"twinned", "Twinned Spell", "1 Sorcery Point: a spell that can take more creatures from a higher slot takes one more."}};
    return group;
}

// Primal Knowledge: one more Barbarian skill at level three. Its own grant is
// left out, so the skill it chose stays a valid option.
TrainingChoiceGroup primal_knowledge_options(std::span<const FeatureGrant> grants)
{
    TrainingChoiceGroup group{"class:barbarian:primal_knowledge", "Primal Knowledge", 1, {}};
    group.acquired_level = 3;
    for (const auto &skill : skills)
    {
        const auto id = "skill:" + std::string(skill.id);
        const bool held = std::any_of(grants.begin(), grants.end(), [&](const auto & g)
        {
            return g.id == id && g.source_id != group.id;
        });
        if ((skill.id == "animal_handling" || skill.id == "athletics" ||
                skill.id == "intimidation" || skill.id == "nature" || skill.id == "perception" ||
                skill.id == "survival") && !held)
            group.options.push_back({std::string(skill.id), std::string(skill.label), {}});
    }
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

TrainingChoiceGroup lessons_skilled_options(std::span<const FeatureGrant> grants, unsigned level)
{
    auto group = skilled_options(grants);
    group.id = std::string(lessons_skilled);
    group.acquired_level = level;
    return group;
}

TrainingChoiceGroup hall_training_options(std::span<const FeatureGrant> grants,
        std::span<const CharacterClass> classes)
{
    TrainingChoiceGroup group{std::string(hall_training), "Training hall", 1, {},
                              TrainingChoiceControl::checkboxes};
    const auto taught = [&](std::string_view skill)
    {
        return std::any_of(classes.begin(), classes.end(), [&](CharacterClass klass)
        {
            const auto data = std::find_if(class_skills().begin(), class_skills().end(),
                                           [&](const auto & entry)
            {
                return entry.id == class_id(klass);
            });
            require(data != class_skills().end());
            return data->skills.empty() ||
                   std::find(data->skills.begin(), data->skills.end(), skill) !=
                   data->skills.end();
        });
    };
    for (const auto &s : skills)
    {
        const auto id = std::string(s.id);
        if (!source(grants, "skill:" + id) && taught(s.id))
            group.options.push_back({"skill:" + id, std::string(s.label), {}});
        else if (source(grants, "skill:" + id) && !source(grants, "expertise:" + id))
            group.options.push_back({"expertise:" + id, std::string(s.label), {}});
    }
    return group;
}

std::vector<TrainingChoiceGroup> training_options(const CharacterDraft &draft)
{
    return options(draft_class(draft), draft.race, draft.background, draft.training);
}

std::vector<FeatureGrant> training_grants(CharacterClass klass, std::string_view race,
        std::string_view background, const TrainingChoices &choices)
{
    auto result = fixed(background);
    const auto groups = options(klass, race, background, choices);
    for (const auto &[id, values] : choices)
        require((id.starts_with("species:") && !id.starts_with("species:" + species_id(race) + ":")) ||
                (std::any_of(groups.begin(), groups.end(),
                             [&](const auto & g)
    {
        return g.id == id;
    }) &&
    !values.empty()));
    // Another species' choices (a draft whose species changed) give nothing.
    for (const auto &group : groups)
        add_choices(
            result, choices, group,
            group.id.starts_with("species:")             ? species_prefix(group.id)
            : group.id.ends_with(":weapon_mastery")        ? "mastery:"
            : group.id == "class:fighter:fighting_style" ? "feat:"
            : group.id == divine_order || group.id == "class:druid:primal_order" ? "order:"
            : group.id == "class:warlock:invocations"    ? "invocation:"
            : group.id == "class:" + std::string(class_id(klass)) ? "skill:"
            : "expertise:");
    return result;
}

TrainingChoices training_choices(std::span<const FeatureGrant> grants, CharacterClass klass,
                                 std::string_view race, std::string_view background)
{
    auto required = fixed(background);
    TrainingChoices choices;
    std::vector<FeatureGrant> actual;
    bool swapped_first_invocation = false;
    // Style selections emit feats; keep them in feature validation as well.
    for (const auto &grant : grants)
        if (is_training_grant(grant) || grant.source_id == "class:fighter:fighting_style" ||
                grant.source_id == "species:human:versatile")
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
            // A training hall teaches one skill or Expertise per level
            // (docs/TRAINING-HALL.md). Which hall may teach it is the campaign's.
            if (grant.source_id == hall_training)
            {
                const auto colon = grant.id.find(':');
                require(grant.level >= 1 && grant.choices.empty() && colon != grant.id.npos);
                const auto kind = std::string_view(grant.id).substr(0, colon);
                const auto skill = std::string_view(grant.id).substr(colon + 1);
                require((kind == "skill" || kind == "expertise") &&
                        std::any_of(skills.begin(), skills.end(), [&](const auto & s)
                {
                    return s.id == skill;
                }));
                require(std::count_if(grants.begin(), grants.end(), [&](const auto & other)
                {
                    return other.source_id == grant.source_id && other.level == grant.level;
                }) == 1);
                require(std::count_if(grants.begin(), grants.end(), [&](const auto & other)
                {
                    return other.id == grant.id;
                }) == 1);
                require(kind == "skill" || source(grants, "skill:" + std::string(skill)));
                choices[grant.source_id].push_back(grant.id);
                continue;
            }
            if (grant.source_id == "subclass:bard:lore")
            {
                require(klass == CharacterClass::bard && grant.level == 3 && grant.choices.empty());
                const auto group = lore_options(grants);
                require(std::any_of(group.options.begin(), group.options.end(),
                                    [&](const auto & option)
                {
                    return grant.id == "skill:" + option.id;
                }));
                auto &selected = choices[grant.source_id];
                require(selected.size() < 3);
                selected.push_back(grant.id.substr(6));
                continue;
            }
            if (grant.source_id == "subclass:druid:land")
            {
                require(klass == CharacterClass::druid && grant.level == 3 &&
                        grant.choices.empty());
                const auto group = land_options();
                require(std::any_of(group.options.begin(), group.options.end(),
                                    [&](const auto & option)
                {
                    return grant.id == "land:" + option.id;
                }));
                auto &selected = choices[grant.source_id];
                require(selected.empty());
                selected.push_back(grant.id.substr(5));
                continue;
            }
            if (grant.source_id == "class:warlock:invocations:2")
            {
                // Level two, or the later level it was taken at in a swap.
                require(klass == CharacterClass::warlock && grant.level >= 2 &&
                        grant.choices.empty());
                const auto group = invocation_options(grant.level, grants, grant.source_id);
                require(std::any_of(group.options.begin(), group.options.end(),
                                    [&](const auto & option)
                {
                    return grant.id == "invocation:" + option.id;
                }));
                auto &selected = choices[grant.source_id];
                require(selected.size() < 2);
                selected.push_back(grant.id.substr(11));
                continue;
            }
            if (grant.source_id == "class:sorcerer:metamagic")
            {
                require(klass == CharacterClass::sorcerer && grant.level == 2 &&
                        grant.choices.empty());
                // Subtle Spell, retired with spell components (CLASS-11), stays
                // valid in older saves and does nothing.
                const auto group = metamagic_options();
                require(grant.id == "metamagic:subtle" ||
                        std::any_of(group.options.begin(), group.options.end(),
                                    [&](const auto & option)
                {
                    return grant.id == "metamagic:" + option.id;
                }));
                auto &selected = choices[grant.source_id];
                require(selected.size() < 2 &&
                        std::find(selected.begin(), selected.end(), grant.id.substr(10)) ==
                        selected.end());
                selected.push_back(grant.id.substr(10));
                continue;
            }
            if (grant.source_id == "class:barbarian:primal_knowledge")
            {
                require(klass == CharacterClass::barbarian && grant.level == 3 &&
                        grant.choices.empty());
                const auto group = primal_knowledge_options(grants);
                require(std::any_of(group.options.begin(), group.options.end(),
                                    [&](const auto & option)
                {
                    return grant.id == "skill:" + option.id;
                }));
                auto &selected = choices[grant.source_id];
                require(selected.empty());
                selected.push_back(grant.id.substr(6));
                continue;
            }
            if (grant.source_id == "class:wizard:scholar")
            {
                require(klass == CharacterClass::wizard &&
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
            // Lessons of the First Ones' Skilled: three skills, taken with it.
            if (grant.source_id == lessons_skilled)
            {
                require(klass == CharacterClass::warlock && grant.level >= 2 &&
                        grant.choices.empty() && grant.id.starts_with("skill:") &&
                        source(grants, std::string(lessons_skilled)));
                auto &selected = choices[grant.source_id];
                require(selected.size() < 3 &&
                        std::find(selected.begin(), selected.end(), grant.id) == selected.end());
                require(std::none_of(grants.begin(), grants.end(), [&](const auto & other)
                {
                    return other.id == grant.id && other.source_id != grant.source_id;
                }));
                selected.push_back(grant.id);
                continue;
            }
            // The level-1 invocation, swapped on gaining a later level for
            // any invocation open at that level.
            if (grant.source_id == "class:warlock:invocations" && grant.level > 1)
            {
                require(klass == CharacterClass::warlock && grant.choices.empty());
                const auto group = invocation_options(grant.level, grants, grant.source_id);
                require(std::any_of(group.options.begin(), group.options.end(),
                                    [&](const auto & option)
                {
                    return grant.id == "invocation:" + option.id;
                }));
                auto &selected = choices[grant.source_id];
                require(selected.empty());
                selected.push_back(grant.id.substr(11));
                swapped_first_invocation = true;
                continue;
            }
            const bool replacement = klass == CharacterClass::fighter &&
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
            const auto prefix = grant.source_id.starts_with("species:")
                                ? species_prefix(grant.source_id)
                                : grant.source_id.ends_with(":weapon_mastery")        ? "mastery:"
                                : grant.source_id == "class:fighter:fighting_style" ? "feat:"
                                : grant.source_id == divine_order ||
                                  grant.source_id == "class:druid:primal_order" ? "order:"
                                : grant.source_id == "class:warlock:invocations"    ? "invocation:"
                                : grant.source_id == "class:" + std::string(class_id(klass))
                                ? "skill:"
                                : "expertise:";
            require(grant.id.starts_with(prefix));
            choices[grant.source_id].push_back(grant.id.substr(std::string_view(prefix).size()));
        }
    require(required.empty());
    auto starting = choices;
    starting.erase("class:wizard:scholar");
    starting.erase("class:barbarian:primal_knowledge");
    starting.erase("class:sorcerer:metamagic");
    starting.erase("class:warlock:invocations:2");
    starting.erase(std::string(lessons_skilled));
    // A swapped level-1 invocation is checked above, not as a starting choice.
    if (swapped_first_invocation)
        starting.erase("class:warlock:invocations");
    starting.erase("subclass:bard:lore");
    starting.erase("subclass:druid:land");
    starting.erase(std::string(skilled));
    starting.erase(std::string(hall_training));
    starting.erase(mastery_options(klass, 4).id);
    auto expected = training_grants(klass, race, background, starting);
    const auto order = [](const FeatureGrant & a, const FeatureGrant & b)
    {
        return std::tie(a.id, a.source_id, a.level) < std::tie(b.id, b.source_id, b.level);
    };
    std::sort(actual.begin(), actual.end(), order);
    std::sort(expected.begin(), expected.end(), order);
    require(actual == expected);
    return choices;
}

TrainingProfile training_profile(std::span<const FeatureGrant> grants, CharacterClass klass,
                                 std::string_view race, std::string_view background,
                                 unsigned level, const AbilityArray<int> &scores)
{
    const auto choices = training_choices(grants, klass, race, background);
    const auto groups = options(klass, race, background, choices);
    TrainingProfile result;
    result.complete = true;
    // The creation screen asks for a species' choices before leaving its page;
    // the rules accept a character made without them (then without their benefits).
    for (const auto &group : groups)
        if (!group.id.starts_with("species:"))
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
    // The level-four Skilled; a Human's Versatile Skilled has its own skills.
    const bool has_skilled = std::any_of(grants.begin(), grants.end(), [](const auto & g)
    {
        return g.id == skilled && g.source_id.ends_with(":ability_score_improvement");
    });
    require((picked.empty() && !has_skilled) || level >= 4);
    if (has_skilled)
        result.complete &= picked.size() == 3;
    if (source(grants, std::string(lessons_skilled)))
        result.complete &= selected(choices, lessons_skilled).size() == 3;
    for (const auto &g : grants)
        require(g.source_id != hall_training || g.level <= level);
    const auto &lore = selected(choices, "subclass:bard:lore");
    require(lore.empty() || level >= 3);
    if (klass == CharacterClass::bard && level >= 3)
        result.complete &= lore.size() == 3;
    const auto &invocations = selected(choices, "class:warlock:invocations:2");
    require(invocations.empty() || level >= 2);
    for (const auto &g : grants)
        require(!g.source_id.starts_with("class:warlock:invocations") || g.level <= level);
    const auto &metamagic = selected(choices, "class:sorcerer:metamagic");
    require(metamagic.empty() || level >= 2);
    if (klass == CharacterClass::sorcerer && level >= 2)
        result.complete &= metamagic.size() == 2;
    const auto &primal = selected(choices, "class:barbarian:primal_knowledge");
    require(primal.empty() || level >= 3);
    if (klass == CharacterClass::barbarian && level >= 3)
        result.complete &= primal.size() == 1;
    const auto &scholar = selected(choices, "class:wizard:scholar");
    require(scholar.empty() || level >= 2);
    if (klass == CharacterClass::wizard && level >= 2)
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

AbilityCheckModifier ability_check(std::span<const FeatureGrant> grants, CharacterClass klass,
                                   std::string_view race, std::string_view background,
                                   unsigned level, const AbilityArray<int> &scores,
                                   Ability ability, std::string_view skill)
{
    (void)training_profile(grants, klass, race, background, level, scores);
    return check_modifier(grants, scores, level, ability, skill);
}
SwapOffer option_swap_offer(const CharacterSheet &sheet)
{
    const auto klass = class_of(sheet);
    if (sheet.level >= 4)
        return {};
    const auto next = static_cast<unsigned>(sheet.level) + 1;
    const auto held = [&](std::string_view prefix, std::string_view source)
    {
        std::vector<std::string> ids;
        for (const auto &g : sheet.grants)
            if (g.source_id.starts_with(source) && g.id.starts_with(prefix))
                ids.push_back(g.id.substr(prefix.size()));
        return ids;
    };
    if (klass == CharacterClass::warlock)
    {
        SwapOffer offer{"Eldritch Invocation", {}, {}};
        // Every invocation's label, whatever its prerequisites: the held
        // cantrips name the blast invocations.
        std::vector<FeatureGrant> cantrips;
        for (const auto &g : sheet.grants)
            if (g.id.starts_with("spell:"))
                cantrips.push_back({g.id, "class:warlock:pact_magic", 1, {}});
        const auto all = invocation_options(4, cantrips, "");
        for (const auto &id : held("invocation:", "class:warlock:invocations"))
            for (const auto &option : all.options)
                if (option.id == id)
                    offer.replaceable.push_back({option.id, option.label, option.description});
        if (offer.replaceable.empty())
            return offer;
        // Held invocations come from other sources than this one, so none is offered.
        for (const auto &option : invocation_options(next, sheet.grants, "swap").options)
            offer.replacements.push_back({option.id, option.label, option.description});
        return offer;
    }
    if (klass == CharacterClass::sorcerer)
    {
        SwapOffer offer{"Metamagic", {}, {}};
        const auto known = held("metamagic:", "class:sorcerer:metamagic");
        for (const auto &option : metamagic_options().options)
        {
            const bool mine = std::find(known.begin(), known.end(), option.id) != known.end();
            (mine ? offer.replaceable : offer.replacements)
            .push_back({option.id, option.label, option.description});
        }
        if (offer.replaceable.empty())
            offer.replacements.clear();
        return offer;
    }
    return {};
}

void apply_option_swap(CharacterSheet &sheet, const LevelSwap &swap, const SwapOffer &offer)
{
    if (swap.replaced.empty() && swap.replacement.empty())
        return;
    const auto listed = [](std::span<const AdvancementOption> options, std::string_view id)
    {
        return std::any_of(options.begin(), options.end(), [&](const auto & o)
        {
            return o.id == id;
        });
    };
    if (!listed(offer.replaceable, swap.replaced) || !listed(offer.replacements, swap.replacement))
        throw std::runtime_error("Choose a listed replacement");
    const bool invocation = offer.label == "Eldritch Invocation";
    const std::string prefix = invocation ? "invocation:" : "metamagic:";
    const std::string source = invocation ? "class:warlock:invocations" : "class:sorcerer:metamagic";
    const auto found = std::find_if(sheet.grants.begin(), sheet.grants.end(), [&](const auto & g)
    {
        return g.source_id.starts_with(source) && g.id == prefix + swap.replaced;
    });
    require(found != sheet.grants.end());
    found->id = prefix + swap.replacement;
    if (!invocation)
        return;
    // Taken at this level: Pact of the Tome's or Magic Initiate's spells are
    // chosen now, and a replaced one's spells go with it.
    found->level = static_cast<unsigned>(sheet.level);
    const std::string picks = swap.replaced == "pact_of_the_tome" ? "invocation:pact_of_the_tome"
                              : swap.replaced == "lessons_skilled" ? std::string(lessons_skilled)
                              : swap.replaced.starts_with("lessons_magic_initiate_")
                              ? "feat:" + swap.replaced.substr(8)
                              : std::string();
    if (!picks.empty())
        std::erase_if(sheet.grants, [&](const auto & g)
    {
        return g.source_id == picks;
    });
}

} // namespace opengold::srd5::detail
