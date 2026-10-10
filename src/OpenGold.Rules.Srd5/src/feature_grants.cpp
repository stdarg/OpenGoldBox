#include "feature_grants.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <iomanip>
#include <istream>
#include <ostream>
#include <set>
#include <stdexcept>

namespace opengold::srd5::detail
{
namespace
{
constexpr std::array<std::string_view, 6> abilities{"strength",     "dexterity", "constitution",
    "intelligence", "wisdom",    "charisma"};

void require(bool value)
{
    if (!value)
        throw std::runtime_error(
            "Invalid feature or feat grant provenance, choices or prerequisites");
}

// The grant source ID of a class's own features, such as "class:fighter".
std::string class_source(CharacterClass klass)
{
    return "class:" + std::string(class_id(klass));
}

// The stable ID of the creation choice a sheet names by its label.
std::string choice_id(rules::CreationField field, std::string_view label, const char *unknown)
{
    const auto choices = character_rules()->choices(field);
    const auto found = std::find_if(choices.begin(), choices.end(),
                                    [&](const rules::CreationChoice & choice)
    {
        return choice.label == label;
    });
    if (found == choices.end())
        throw std::runtime_error(unknown);
    return found->id;
}
} // namespace

std::string race_id(std::string_view label)
{
    return choice_id(rules::CreationField::race, label, "Unknown race");
}

std::string background_id(std::string_view label)
{
    return choice_id(rules::CreationField::background, label, "Unknown background");
}

std::vector<rules::FeatureGrant> starting_grants(CharacterClass klass, std::string_view race,
        std::string_view background)
{
    std::vector<rules::FeatureGrant> result;
    if (background == "criminal")
        result.push_back({"feat:alert", "background:criminal", 1, {}});
    if (background == "soldier")
        result.push_back({"feat:savage_attacker", "background:soldier", 1, {}});
    if (klass == CharacterClass::fighter)
    {
        // Entitlement already used by the existing Defense selection. Completing
        // the level-one style selector is a separate class increment.
        result.push_back({"feature:fighting_style", "class:fighter", 1, {}});
        result.push_back({"feature:second_wind", "class:fighter", 1, {}});
    }
    if (klass == CharacterClass::barbarian || klass == CharacterClass::monk)
        result.push_back({"feature:unarmored_defense", class_source(klass), 1, {}});
    if (klass == CharacterClass::cleric || klass == CharacterClass::wizard ||
            klass == CharacterClass::paladin || klass == CharacterClass::ranger ||
            klass == CharacterClass::sorcerer || klass == CharacterClass::bard ||
            klass == CharacterClass::druid)
        result.push_back({"feature:spellcasting", class_source(klass), 1, {}});
    if (klass == CharacterClass::sorcerer)
        result.push_back({"feature:innate_sorcery", "class:sorcerer", 1, {}});
    if (klass == CharacterClass::bard)
        result.push_back({"feature:bardic_inspiration", "class:bard", 1, {}});
    if (klass == CharacterClass::warlock)
        result.push_back({"feature:pact_magic", "class:warlock", 1, {}});
    if (klass == CharacterClass::rogue)
        result.push_back({"feature:sneak_attack", "class:rogue", 1, {}});
    if (klass == CharacterClass::wizard)
        result.push_back({"feature:arcane_recovery", "class:wizard", 1, {}});
    if (race == "dwarf")
    {
        result.push_back({"trait:dwarven_toughness", "species:dwarf", 1, {}});
        result.push_back({"trait:dwarven_resilience", "species:dwarf", 1, {}});
    }
    if (race == "orc")
        result.push_back({"trait:adrenaline_rush", "species:orc", 1, {}});
    if (race == "goliath")
        result.push_back({"trait:speed", "species:goliath", 1, {}});
    return result;
}

rules::FeatureGrant advancement_grant(CharacterClass klass, unsigned level,
                                      const rules::AdvancementChoice &choice)
{
    rules::FeatureGrant result{"feat:" + choice.feat,
                               class_source(klass) + ":ability_score_improvement",
                               level,
                               {}};
    for (unsigned i = 0; i < abilities.size(); ++i)
        if (choice.abilities[i])
            result.choices.emplace(abilities[i], std::to_string(choice.abilities[i]));
    return result;
}

std::vector<rules::AdvancementOption> fighting_styles()
{
    return
    {
        {"defense", "Defense", "+1 AC while wearing armor."},
        {"archery", "Archery", "+2 to attack rolls with Ranged weapons."},
        {
            "great_weapon_fighting", "Great Weapon Fighting",
            "Treat damage dice showing 1 or 2 as 3 with an eligible Melee weapon held in two hands."
        },
        {
            "two_weapon_fighting", "Two-Weapon Fighting",
            "Add your ability modifier to the extra attack granted by the Light property."
        }};
}

bool has_grant(std::span<const rules::FeatureGrant> grants, std::string_view id)
{
    return std::any_of(grants.begin(), grants.end(),
                       [&](const auto & grant)
    {
        return grant.id == id;
    });
}

GrantEffects validate_grants(std::span<const rules::FeatureGrant> grants, CharacterClass klass,
                             std::string_view race, std::string_view background, unsigned level)
{
    require(level >= 1 && level <= 4 && grants.size() <= 32);
    require(background == "acolyte" || background == "criminal" || background == "sage" ||
            background == "soldier");
    auto required = starting_grants(klass, race, background);
    if ((klass == CharacterClass::paladin || klass == CharacterClass::ranger) && level >= 2)
        required.push_back({"feature:fighting_style", class_source(klass), 2, {}});
    if (klass == CharacterClass::rogue && level >= 3)
    {
        required.push_back({"feature:steady_aim", "class:rogue", 3, {}});
        required.push_back({"subclass:thief", "class:rogue", 3, {}});
        required.push_back({"feature:fast_hands", "subclass:rogue:thief", 3, {}});
    }
    if (klass == CharacterClass::cleric && level >= 2)
        required.push_back({"feature:channel_divinity", "class:cleric", 2, {}});
    if (klass == CharacterClass::cleric && level >= 3)
    {
        required.push_back({"subclass:life", "class:cleric", 3, {}});
        required.push_back({"feature:disciple_of_life", "subclass:cleric:life", 3, {}});
        required.push_back({"feature:preserve_life", "subclass:cleric:life", 3, {}});
    }
    if (klass == CharacterClass::wizard && level >= 3)
    {
        required.push_back({"subclass:evoker", "class:wizard", 3, {}});
        required.push_back({"feature:potent_cantrip", "subclass:wizard:evoker", 3, {}});
        required.push_back({"feature:sculpt_spells", "subclass:wizard:evoker", 3, {}});
    }
    if (klass == CharacterClass::ranger && level >= 3)
    {
        required.push_back({"subclass:hunter", "class:ranger", 3, {}});
        required.push_back({"feature:hunters_lore", "subclass:ranger:hunter", 3, {}});
    }
    if (klass == CharacterClass::paladin && level >= 3)
    {
        required.push_back({"feature:channel_divinity", "class:paladin", 3, {}});
        required.push_back({"subclass:devotion", "class:paladin", 3, {}});
        required.push_back({"feature:sacred_weapon", "subclass:paladin:devotion", 3, {}});
    }
    if (klass == CharacterClass::rogue && level >= 2)
        required.push_back({"feature:cunning_action", "class:rogue", 2, {}});
    if (klass == CharacterClass::sorcerer && level >= 2)
    {
        required.push_back({"feature:font_of_magic", "class:sorcerer", 2, {}});
        required.push_back({"feature:metamagic", "class:sorcerer", 2, {}});
    }
    if (klass == CharacterClass::bard && level >= 2)
        required.push_back({"feature:jack_of_all_trades", "class:bard", 2, {}});
    if (klass == CharacterClass::bard && level >= 3)
    {
        required.push_back({"subclass:lore", "class:bard", 3, {}});
        required.push_back({"feature:bonus_proficiencies", "subclass:bard:lore", 3, {}});
        required.push_back({"feature:cutting_words", "subclass:bard:lore", 3, {}});
    }
    if (klass == CharacterClass::druid && level >= 2)
        required.push_back({"feature:wild_shape", "class:druid", 2, {}});
    if (klass == CharacterClass::druid && level >= 3)
    {
        required.push_back({"subclass:land", "class:druid", 3, {}});
        required.push_back({"feature:circle_spells", "subclass:druid:land", 3, {}});
        required.push_back({"feature:lands_aid", "subclass:druid:land", 3, {}});
    }
    if (klass == CharacterClass::warlock && level >= 2)
        required.push_back({"feature:magical_cunning", "class:warlock", 2, {}});
    if (klass == CharacterClass::warlock && level >= 3)
    {
        required.push_back({"subclass:fiend", "class:warlock", 3, {}});
        required.push_back({"feature:dark_ones_blessing", "subclass:warlock:fiend", 3, {}});
        required.push_back({"feature:fiend_spells", "subclass:warlock:fiend", 3, {}});
    }
    if (klass == CharacterClass::sorcerer && level >= 3)
    {
        required.push_back({"subclass:draconic", "class:sorcerer", 3, {}});
        required.push_back({"feature:draconic_resilience", "subclass:sorcerer:draconic", 3, {}});
        required.push_back({"feature:draconic_spells", "subclass:sorcerer:draconic", 3, {}});
    }
    if (klass == CharacterClass::monk && level >= 2)
    {
        required.push_back({"feature:monks_focus", "class:monk", 2, {}});
        required.push_back({"feature:unarmored_movement", "class:monk", 2, {}});
        required.push_back({"feature:uncanny_metabolism", "class:monk", 2, {}});
    }
    if (klass == CharacterClass::monk && level >= 3)
    {
        required.push_back({"feature:deflect_attacks", "class:monk", 3, {}});
        required.push_back({"subclass:open_hand", "class:monk", 3, {}});
        required.push_back({"feature:open_hand_technique", "subclass:monk:open_hand", 3, {}});
    }
    if (klass == CharacterClass::barbarian && level >= 2)
    {
        required.push_back({"feature:danger_sense", "class:barbarian", 2, {}});
        required.push_back({"feature:reckless_attack", "class:barbarian", 2, {}});
    }
    if (klass == CharacterClass::barbarian && level >= 3)
    {
        required.push_back({"subclass:berserker", "class:barbarian", 3, {}});
        required.push_back({"feature:frenzy", "subclass:barbarian:berserker", 3, {}});
        required.push_back({"feature:primal_knowledge", "class:barbarian", 3, {}});
    }
    if (klass == CharacterClass::fighter && level >= 2)
    {
        required.push_back({"feature:action_surge", "class:fighter", 2, {}});
        required.push_back({"feature:tactical_mind", "class:fighter", 2, {}});
    }
    if (klass == CharacterClass::fighter && level >= 3)
    {
        required.push_back({"subclass:champion", "class:fighter", 3, {}});
        required.push_back({"feature:improved_critical", "subclass:fighter:champion", 3, {}});
        required.push_back({"feature:remarkable_athlete", "subclass:fighter:champion", 3, {}});
    }
    std::set<std::string> nonrepeatable;
    std::set<std::pair<std::string, unsigned>> entitlements;
    GrantEffects effects;
    unsigned advancement_count = 0;
    for (const auto &grant : grants)
    {
        require(grant.level >= 1 && grant.level <= level);
        // ASI and Skilled are repeatable, but never twice from the same
        // entitlement; the entitlements set below enforces that. Other
        // implemented feats/features are not repeatable (SRD pp. 87–88).
        if (grant.id != "feat:ability_score_improvement" && grant.id != "feat:skilled")
            require(nonrepeatable.insert(grant.id).second);
        const auto fixed = std::find(required.begin(), required.end(), grant);
        if (fixed != required.end())
            required.erase(fixed);
        else if (grant.source_id == "subclass:ranger:hunter")
        {
            // Hunter's Prey: one of its two options, chosen at level three.
            require(klass == CharacterClass::ranger && grant.level == 3 && grant.choices.empty() &&
                    (grant.id == "prey:colossus_slayer" || grant.id == "prey:horde_breaker"));
            require(entitlements.emplace(grant.source_id, 0).second);
        }
        else if (grant.source_id == class_source(klass) + ":fighting_style")
        {
            require(
                (klass == CharacterClass::fighter ||
                 ((klass == CharacterClass::paladin || klass == CharacterClass::ranger) &&
                  grant.level == 2)) &&
                grant.choices.empty() &&
                (grant.id == "feat:defense" || grant.id == "feat:archery" ||
                 grant.id == "feat:great_weapon_fighting" ||
                 grant.id == "feat:two_weapon_fighting" ||
                 (klass == CharacterClass::paladin && grant.id == "feature:blessed_warrior") ||
                 (klass == CharacterClass::ranger && grant.id == "feature:druidic_warrior")) &&
                has_grant(grants, "feature:fighting_style"));
            require(entitlements.emplace(grant.source_id, 0).second);
        }
        else
        {
            require(grant.level == 4 &&
                    grant.source_id == class_source(klass) + ":ability_score_improvement");
            require(entitlements.emplace(grant.source_id, grant.level).second);
            ++advancement_count;
            if (grant.id == "feat:ability_score_improvement")
            {
                unsigned points = 0;
                for (const auto &[key, value] : grant.choices)
                {
                    const auto found = std::find(abilities.begin(), abilities.end(), key);
                    require(found != abilities.end() && (value == "1" || value == "2"));
                    const auto amount = value == "1" ? 1 : 2;
                    effects.abilities[found - abilities.begin()] += amount;
                    points += amount;
                }
                require(points == 2);
            }
            else
            {
                require(grant.choices.empty());
                if (grant.id == "feat:defense" || grant.id == "feat:archery" ||
                        grant.id == "feat:great_weapon_fighting" ||
                        grant.id == "feat:two_weapon_fighting")
                    require(has_grant(grants, "feature:fighting_style"));
                else
                    require(grant.id == "feat:savage_attacker" || grant.id == "feat:alert" ||
                            grant.id == "feat:skilled");
            }
        }
        if (grant.id == "feat:alert")
            effects.feats |= 32;
        if (grant.id == "feat:defense")
            effects.feats |= 1;
        if (grant.id == "feat:savage_attacker")
            effects.feats |= 2;
        if (grant.id == "feat:archery")
            effects.feats |= 4;
        if (grant.id == "feat:great_weapon_fighting")
            effects.feats |= 8;
        if (grant.id == "feat:two_weapon_fighting")
            effects.feats |= 16;
    }
    if ((klass == CharacterClass::paladin || klass == CharacterClass::ranger) && level >= 2)
        require(entitlements.contains({class_source(klass) + ":fighting_style", 0}));
    if (klass == CharacterClass::ranger && level >= 3)
        require(entitlements.contains({"subclass:ranger:hunter", 0}));
    require(required.empty() && advancement_count == (level == 4 ? 1u : 0u));
    return effects;
}

void write_grants(std::ostream &out, std::span<const rules::FeatureGrant> grants)
{
    out << ' ' << grants.size();
    for (const auto &grant : grants)
    {
        out << ' ' << std::quoted(grant.id) << ' ' << std::quoted(grant.source_id) << ' '
            << grant.level << ' ' << grant.choices.size();
        for (const auto &[key, value] : grant.choices)
            out << ' ' << std::quoted(key) << ' ' << std::quoted(value);
    }
}

std::vector<rules::FeatureGrant> read_grants(std::istream &in)
{
    unsigned count{};
    in >> count;
    require(static_cast<bool>(in) && count <= 32);
    std::vector<rules::FeatureGrant> result;
    for (unsigned i = 0; i < count; ++i)
    {
        rules::FeatureGrant grant;
        unsigned choices{};
        in >> std::quoted(grant.id) >> std::quoted(grant.source_id) >> grant.level >> choices;
        require(static_cast<bool>(in) && grant.id.size() <= 128 && grant.source_id.size() <= 128 &&
                choices <= 6);
        for (unsigned n = 0; n < choices; ++n)
        {
            std::string key, value;
            in >> std::quoted(key) >> std::quoted(value);
            require(static_cast<bool>(in) && key.size() <= 128 && value.size() <= 128 &&
                    grant.choices.emplace(key, value).second);
        }
        result.push_back(std::move(grant));
    }
    return result;
}
} // namespace opengold::srd5::detail
