#include "feature_grants.h"
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
} // namespace

std::string grant_source_id(std::string_view label)
{
    std::string result(label);
    for (auto &c : result)
        if (c >= 'A' && c <= 'Z')
            c += 32;
    return result;
}

std::vector<rules::FeatureGrant> starting_grants(std::string_view klass, std::string_view race,
        std::string_view background)
{
    std::vector<rules::FeatureGrant> result;
    if (background == "criminal")
        result.push_back({"feat:alert", "background:criminal", 1, {}});
    if (background == "soldier")
        result.push_back({"feat:savage_attacker", "background:soldier", 1, {}});
    if (klass == "fighter")
    {
        // Entitlement already used by the existing Defense selection. Completing
        // the level-one style selector is a separate class increment.
        result.push_back({"feature:fighting_style", "class:fighter", 1, {}});
        result.push_back({"feature:second_wind", "class:fighter", 1, {}});
    }
    if (klass == "barbarian" || klass == "monk")
        result.push_back({"feature:unarmored_defense", "class:" + std::string(klass), 1, {}});
    if (klass == "cleric" || klass == "wizard" || klass == "paladin" || klass == "ranger")
        result.push_back({"feature:spellcasting", "class:" + std::string(klass), 1, {}});
    if (klass == "rogue")
        result.push_back({"feature:sneak_attack", "class:rogue", 1, {}});
    if (klass == "wizard")
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

rules::FeatureGrant advancement_grant(std::string_view klass, unsigned level,
                                      const rules::AdvancementChoice &choice)
{
    rules::FeatureGrant result{"feat:" + choice.feat,
                               "class:" + std::string(klass) + ":ability_score_improvement",
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

GrantEffects validate_grants(std::span<const rules::FeatureGrant> grants, std::string_view klass,
                             std::string_view race, std::string_view background, unsigned level)
{
    require(level >= 1 && level <= 4 && grants.size() <= 32);
    require(background == "acolyte" || background == "criminal" || background == "sage" ||
            background == "soldier");
    auto required = starting_grants(klass, race, background);
    if ((klass == "paladin" || klass == "ranger") && level >= 2)
        required.push_back({"feature:fighting_style", "class:" + std::string(klass), 2, {}});
    if (klass == "rogue" && level >= 3)
    {
        required.push_back({"feature:steady_aim", "class:rogue", 3, {}});
        required.push_back({"subclass:thief", "class:rogue", 3, {}});
        required.push_back({"feature:fast_hands", "subclass:rogue:thief", 3, {}});
    }
    if (klass == "cleric" && level >= 2)
        required.push_back({"feature:channel_divinity", "class:cleric", 2, {}});
    if (klass == "cleric" && level >= 3)
    {
        required.push_back({"subclass:life", "class:cleric", 3, {}});
        required.push_back({"feature:disciple_of_life", "subclass:cleric:life", 3, {}});
        required.push_back({"feature:preserve_life", "subclass:cleric:life", 3, {}});
    }
    if (klass == "wizard" && level >= 3)
    {
        required.push_back({"subclass:evoker", "class:wizard", 3, {}});
        required.push_back({"feature:potent_cantrip", "subclass:wizard:evoker", 3, {}});
        required.push_back({"feature:sculpt_spells", "subclass:wizard:evoker", 3, {}});
    }
    if (klass == "ranger" && level >= 3)
    {
        required.push_back({"subclass:hunter", "class:ranger", 3, {}});
        required.push_back({"feature:hunters_lore", "subclass:ranger:hunter", 3, {}});
    }
    if (klass == "paladin" && level >= 3)
    {
        required.push_back({"feature:channel_divinity", "class:paladin", 3, {}});
        required.push_back({"subclass:devotion", "class:paladin", 3, {}});
        required.push_back({"feature:sacred_weapon", "subclass:paladin:devotion", 3, {}});
    }
    if (klass == "rogue" && level >= 2)
        required.push_back({"feature:cunning_action", "class:rogue", 2, {}});
    if (klass == "monk" && level >= 2)
    {
        required.push_back({"feature:monks_focus", "class:monk", 2, {}});
        required.push_back({"feature:unarmored_movement", "class:monk", 2, {}});
        required.push_back({"feature:uncanny_metabolism", "class:monk", 2, {}});
    }
    if (klass == "monk" && level >= 3)
    {
        required.push_back({"feature:deflect_attacks", "class:monk", 3, {}});
        required.push_back({"subclass:open_hand", "class:monk", 3, {}});
        required.push_back({"feature:open_hand_technique", "subclass:monk:open_hand", 3, {}});
    }
    if (klass == "barbarian" && level >= 2)
    {
        required.push_back({"feature:danger_sense", "class:barbarian", 2, {}});
        required.push_back({"feature:reckless_attack", "class:barbarian", 2, {}});
    }
    if (klass == "barbarian" && level >= 3)
    {
        required.push_back({"subclass:berserker", "class:barbarian", 3, {}});
        required.push_back({"feature:frenzy", "subclass:barbarian:berserker", 3, {}});
        required.push_back({"feature:primal_knowledge", "class:barbarian", 3, {}});
    }
    if (klass == "fighter" && level >= 2)
    {
        required.push_back({"feature:action_surge", "class:fighter", 2, {}});
        required.push_back({"feature:tactical_mind", "class:fighter", 2, {}});
    }
    if (klass == "fighter" && level >= 3)
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
            require(klass == "ranger" && grant.level == 3 && grant.choices.empty() &&
                    (grant.id == "prey:colossus_slayer" || grant.id == "prey:horde_breaker"));
            require(entitlements.emplace(grant.source_id, 0).second);
        }
        else if (grant.source_id == "class:" + std::string(klass) + ":fighting_style")
        {
            require(
                (klass == "fighter" ||
                 ((klass == "paladin" || klass == "ranger") && grant.level == 2)) &&
                grant.choices.empty() &&
                (grant.id == "feat:defense" || grant.id == "feat:archery" ||
                 grant.id == "feat:great_weapon_fighting" ||
                 grant.id == "feat:two_weapon_fighting" ||
                 (klass == "paladin" && grant.id == "feature:blessed_warrior") ||
                 (klass == "ranger" && grant.id == "feature:druidic_warrior")) &&
                has_grant(grants, "feature:fighting_style"));
            require(entitlements.emplace(grant.source_id, 0).second);
        }
        else
        {
            require((klass == "fighter" || klass == "cleric" || klass == "wizard" ||
                     klass == "barbarian" || klass == "monk" ||
                     klass == "rogue" || klass == "paladin" || klass == "ranger") &&
                    grant.level == 4 &&
                    grant.source_id ==
                    "class:" + std::string(klass) + ":ability_score_improvement");
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
    if ((klass == "paladin" || klass == "ranger") && level >= 2)
        require(entitlements.contains({"class:" + std::string(klass) + ":fighting_style", 0}));
    if (klass == "ranger" && level >= 3)
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
    require(bool(in) && count <= 32);
    std::vector<rules::FeatureGrant> result;
    for (unsigned i = 0; i < count; ++i)
    {
        rules::FeatureGrant grant;
        unsigned choices{};
        in >> std::quoted(grant.id) >> std::quoted(grant.source_id) >> grant.level >> choices;
        require(bool(in) && grant.id.size() <= 128 && grant.source_id.size() <= 128 &&
                choices <= 6);
        for (unsigned n = 0; n < choices; ++n)
        {
            std::string key, value;
            in >> std::quoted(key) >> std::quoted(value);
            require(bool(in) && key.size() <= 128 && value.size() <= 128 &&
                    grant.choices.emplace(key, value).second);
        }
        result.push_back(std::move(grant));
    }
    return result;
}
} // namespace opengold::srd5::detail
