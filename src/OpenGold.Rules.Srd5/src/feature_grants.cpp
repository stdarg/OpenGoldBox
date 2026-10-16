#include "feature_grants.h"
#include "opengold/srd5.h"
#include "spell_access.h"
#include "spell_table.h"
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
constexpr rules::AbilityArray<std::string_view> ability_ids{"strength", "dexterity",
    "constitution", "intelligence", "wisdom", "charisma"};

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

std::string background_feat(std::string_view background)
{
    // Each list of Magic Initiate is its own feat, so Acolyte and Sage name theirs.
    return background == "criminal"  ? "feat:alert"
           : background == "soldier" ? "feat:savage_attacker"
           : background == "acolyte" ? "feat:magic_initiate_cleric"
           : background == "sage"    ? "feat:magic_initiate_wizard"
           : std::string();
}

std::vector<rules::FeatureGrant> starting_grants(CharacterClass klass, std::string_view race,
        std::string_view background)
{
    std::vector<rules::FeatureGrant> result;
    if (const auto feat = background_feat(background); !feat.empty())
        result.push_back({feat, "background:" + std::string(background), 1, {}});
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
    for (const auto ability : rules::all_abilities)
        if (choice.abilities[ability])
            result.choices.emplace(ability_ids[ability], std::to_string(choice.abilities[ability]));
    return result;
}

std::vector<rules::FeatureGrant> higher_level_features(CharacterClass klass, unsigned level)
{
    std::vector<rules::FeatureGrant> result;
    const auto add = [&](std::string_view id, std::string_view source)
    {
        result.push_back({std::string(id), std::string(source), level, {}});
    };
    if (klass == CharacterClass::fighter)
    {
        if (level == 5)
        {
            add("feature:extra_attack", "class:fighter");
            add("feature:tactical_shift", "class:fighter");
        }
        if (level == 7)
            add("feature:additional_fighting_style", "subclass:fighter:champion");
        if (level == 9)
        {
            add("feature:indomitable", "class:fighter");
            add("feature:tactical_master", "class:fighter");
        }
        if (level == 10)
            add("feature:heroic_warrior", "subclass:fighter:champion");
        if (level == 11)
            add("feature:two_extra_attacks", "class:fighter");
        if (level == 13)
            add("feature:studied_attacks", "class:fighter");
        if (level == 15)
            add("feature:superior_critical", "subclass:fighter:champion");
    }
    if (klass == CharacterClass::barbarian)
    {
        if (level == 5)
        {
            add("feature:extra_attack", "class:barbarian");
            add("feature:fast_movement", "class:barbarian");
        }
        if (level == 6)
            add("feature:mindless_rage", "subclass:barbarian:berserker");
        if (level == 7)
        {
            add("feature:feral_instinct", "class:barbarian");
            add("feature:instinctive_pounce", "class:barbarian");
        }
        if (level == 9)
            add("feature:brutal_strike", "class:barbarian");
        if (level == 10)
            add("feature:retaliation", "subclass:barbarian:berserker");
        if (level == 11)
            add("feature:relentless_rage", "class:barbarian");
        if (level == 13)
            add("feature:improved_brutal_strike", "class:barbarian");
        if (level == 14)
            add("feature:intimidating_presence", "subclass:barbarian:berserker");
        if (level == 15)
            add("feature:persistent_rage", "class:barbarian");
    }
    if (klass == CharacterClass::rogue)
    {
        if (level == 5)
        {
            add("feature:cunning_strike", "class:rogue");
            add("feature:uncanny_dodge", "class:rogue");
        }
        if (level == 7)
        {
            add("feature:evasion", "class:rogue");
            add("feature:reliable_talent", "class:rogue");
        }
        if (level == 9)
            add("feature:supreme_sneak", "subclass:rogue:thief");
        if (level == 11)
            add("feature:improved_cunning_strike", "class:rogue");
        // Use Magic Device waits for magic items (#234), as Fast Hands does.
        if (level == 13)
            add("feature:use_magic_device", "subclass:rogue:thief");
        if (level == 14)
            add("feature:devious_strikes", "class:rogue");
        if (level == 15)
            add("feature:slippery_mind", "class:rogue");
    }
    // Faithful Steed (Paladin 5) is cut with Find Steed (SCOPE-1: no mounts).
    if (klass == CharacterClass::paladin)
    {
        if (level == 5)
            add("feature:extra_attack", "class:paladin");
        if (level == 6)
            add("feature:aura_of_protection", "class:paladin");
        if (level == 7)
            add("feature:aura_of_devotion", "subclass:paladin:devotion");
        if (level == 9)
            add("feature:abjure_foes", "class:paladin");
        if (level == 10)
            add("feature:aura_of_courage", "class:paladin");
        if (level == 11)
            add("feature:radiant_strikes", "class:paladin");
        if (level == 14)
            add("feature:restoring_touch", "class:paladin");
        if (level == 15)
            add("feature:smite_of_protection", "subclass:paladin:devotion");
    }
    if (klass == CharacterClass::cleric)
    {
        if (level == 5)
            add("feature:sear_undead", "class:cleric");
        if (level == 6)
            add("feature:blessed_healer", "subclass:cleric:life");
        if (level == 10)
            add("feature:divine_intervention", "class:cleric");
    }
    if (klass == CharacterClass::wizard)
    {
        if (level == 5)
            add("feature:memorize_spell", "class:wizard");
        if (level == 6)
            add("feature:sculpt_spells", "subclass:wizard:evoker");
        if (level == 10)
            add("feature:empowered_evocation", "subclass:wizard:evoker");
        if (level == 14)
            add("feature:overchannel", "subclass:wizard:evoker");
    }
    if (klass == CharacterClass::warlock && level == 6)
        add("feature:dark_ones_own_luck", "subclass:warlock:fiend");
    if (klass == CharacterClass::warlock && level == 14)
        add("feature:hurl_through_hell", "subclass:warlock:fiend");
    if (klass == CharacterClass::bard)
    {
        if (level == 5)
            add("feature:font_of_inspiration", "class:bard");
        if (level == 7)
            add("feature:countercharm", "class:bard");
        if (level == 10)
            add("feature:magical_secrets", "class:bard");
        if (level == 14)
            add("feature:peerless_skill", "subclass:bard:lore");
    }
    if (klass == CharacterClass::sorcerer)
    {
        if (level == 5)
            add("feature:sorcerous_restoration", "class:sorcerer");
        if (level == 7)
            add("feature:sorcery_incarnate", "class:sorcerer");
        if (level == 14)
            add("feature:dragon_wings", "subclass:sorcerer:draconic");
    }
    // Roving's Climb and Swim Speeds are cut (SCOPE-2); its Speed stays.
    if (klass == CharacterClass::ranger)
    {
        if (level == 5)
            add("feature:extra_attack", "class:ranger");
        if (level == 6)
            add("feature:roving", "class:ranger");
        if (level == 10)
            add("feature:tireless", "class:ranger");
        if (level == 11)
            add("feature:superior_hunters_prey", "subclass:ranger:hunter");
        if (level == 13)
            add("feature:relentless_hunter", "class:ranger");
        if (level == 14)
            add("feature:natures_veil", "class:ranger");
        if (level == 15)
            add("feature:superior_hunters_defense", "subclass:ranger:hunter");
    }
    // Acrobatic Movement (Monk 9) is cut (SCOPE-2).
    if (klass == CharacterClass::monk)
    {
        if (level == 5)
        {
            add("feature:extra_attack", "class:monk");
            add("feature:stunning_strike", "class:monk");
        }
        if (level == 6)
        {
            add("feature:empowered_strikes", "class:monk");
            add("feature:wholeness_of_body", "subclass:monk:open_hand");
        }
        if (level == 7)
            add("feature:evasion", "class:monk");
        if (level == 10)
        {
            add("feature:heightened_focus", "class:monk");
            add("feature:self_restoration", "class:monk");
        }
        if (level == 11)
            add("feature:fleet_step", "subclass:monk:open_hand");
        if (level == 13)
            add("feature:deflect_energy", "class:monk");
        if (level == 14)
            add("feature:disciplined_survivor", "class:monk");
        if (level == 15)
            add("feature:perfect_focus", "class:monk");
    }
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
    require(level >= 1 && level <= level_cap(klass) && grants.size() <= 64);
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
    for (unsigned gained = 5; gained <= level; ++gained)
        for (auto &grant : higher_level_features(klass, gained))
            required.push_back(std::move(grant));
    std::set<std::string> nonrepeatable;
    std::set<std::pair<std::string, unsigned>> entitlements;
    GrantEffects effects;
    unsigned advancement_count = 0;
    for (const auto &grant : grants)
    {
        require(grant.level >= 1 && grant.level <= level);
        // ASI and Skilled are repeatable, but never twice from the same
        // entitlement; the entitlements set below enforces that. Magic Initiate
        // repeats only with another list, which is another feat id. Other
        // implemented feats/features are not repeatable (SRD pp. 87–88).
        if (grant.id != "feat:ability_score_improvement" && grant.id != "feat:skilled")
            require(nonrepeatable.insert(grant.id).second);
        const auto fixed = std::find(required.begin(), required.end(), grant);
        if (fixed != required.end())
            required.erase(fixed);
        else if (grant.source_id == "species:human:versatile")
        {
            // Versatile: a Human's Origin feat (SRD 5.2.1 p. 85), chosen at creation.
            require(race == "human" && grant.level == 1 && grant.choices.empty() &&
                    (grant.id == "feat:alert" || grant.id == "feat:savage_attacker" ||
                     grant.id == "feat:skilled" || grant.id == "feat:magic_initiate_cleric" ||
                     grant.id == "feat:magic_initiate_druid" ||
                     grant.id == "feat:magic_initiate_wizard"));
            require(entitlements.emplace(grant.source_id, 0).second);
        }
        else if (grant.source_id == champion_style)
        {
            // The Champion's second Fighting Style feat, chosen at level 7.
            require(klass == CharacterClass::fighter && grant.level == 7 && grant.choices.empty() &&
                    (grant.id == "feat:defense" || grant.id == "feat:archery" ||
                     grant.id == "feat:great_weapon_fighting" ||
                     grant.id == "feat:two_weapon_fighting"));
            require(entitlements.emplace(grant.source_id, 0).second);
        }
        else if (grant.source_id == "subclass:ranger:hunter")
        {
            // Hunter's Prey: one of its two options, chosen at level three.
            require(klass == CharacterClass::ranger && grant.level == 3 && grant.choices.empty() &&
                    (grant.id == "prey:colossus_slayer" || grant.id == "prey:horde_breaker"));
            require(entitlements.emplace(grant.source_id, 0).second);
        }
        else if (grant.source_id == blessed_strikes)
        {
            // Blessed Strikes: one of its two options, chosen at level seven.
            require(klass == CharacterClass::cleric && grant.level == 7 && grant.choices.empty() &&
                    (grant.id == "strikes:divine_strike" ||
                     grant.id == "strikes:potent_spellcasting"));
            require(entitlements.emplace(grant.source_id, 0).second);
        }
        else if (grant.source_id == elemental_affinity)
        {
            // Elemental Affinity: one of its five types, chosen at level six.
            require(klass == CharacterClass::sorcerer && grant.level == 6 && grant.choices.empty() &&
                    (grant.id == "affinity:acid" || grant.id == "affinity:cold" ||
                     grant.id == "affinity:fire" || grant.id == "affinity:lightning" ||
                     grant.id == "affinity:poison"));
            require(entitlements.emplace(grant.source_id, 0).second);
        }
        else if (grant.source_id.starts_with("class:warlock:arcanum:"))
        {
            // Mystic Arcanum: a Warlock spell of level 6, 7 or 8, chosen at
            // level 11, 13 or 15.
            const auto *spell = find_spell(std::string_view(grant.id).substr(8));
            require(klass == CharacterClass::warlock &&
                    (grant.level == 11 || grant.level == 13 || grant.level == 15) &&
                    grant.source_id == "class:warlock:arcanum:" + std::to_string(grant.level) &&
                    grant.choices.empty() && grant.id.starts_with("arcanum:") && spell &&
                    spell->id == std::string_view(grant.id).substr(8) &&
                    spell->level == (grant.level + 1) / 2 &&
                    on_class_list(CharacterClass::warlock, spell->id));
            require(entitlements.emplace(grant.source_id, 0).second);
        }
        else if (grant.source_id == "subclass:bard:discoveries")
        {
            // Magical Discoveries: two Cleric, Druid or Wizard spells, a cantrip
            // or of level 1 to 3, chosen at level six.
            const auto *spell = find_spell(std::string_view(grant.id).substr(10));
            require(klass == CharacterClass::bard && grant.level == 6 && grant.choices.empty() &&
                    grant.id.starts_with("discovery:") && spell &&
                    spell->id == std::string_view(grant.id).substr(10) && spell->level <= 3 &&
                    (on_class_list(CharacterClass::cleric, spell->id) ||
                     on_class_list(CharacterClass::druid, spell->id) ||
                     on_class_list(CharacterClass::wizard, spell->id)));
            const auto index = static_cast<unsigned>(std::count_if(
                                   entitlements.begin(), entitlements.end(), [&](const auto & held)
            {
                return held.first == grant.source_id;
            }));
            require(index < 2 && entitlements.emplace(grant.source_id, index).second);
        }
        else if (grant.source_id == fiendish_resilience)
        {
            // Fiendish Resilience: any type but Force, chosen at level ten.
            constexpr std::array<std::string_view, 12> types{"acid", "bludgeoning", "cold", "fire",
                "lightning", "necrotic", "piercing", "poison", "psychic", "radiant", "slashing",
                "thunder"};
            require(klass == CharacterClass::warlock && grant.level == 10 && grant.choices.empty() &&
                    grant.id.starts_with("resilience:") &&
                    std::find(types.begin(), types.end(), std::string_view(grant.id).substr(11)) !=
                    types.end());
            require(entitlements.emplace(grant.source_id, 0).second);
        }
        else if (grant.source_id == defensive_tactics)
        {
            // Defensive Tactics: one of its two options, chosen at level seven.
            require(klass == CharacterClass::ranger && grant.level == 7 && grant.choices.empty() &&
                    (grant.id == "tactics:escape_the_horde" ||
                     grant.id == "tactics:multiattack_defense"));
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
            require(gains_feat(klass, grant.level) &&
                    grant.source_id == class_source(klass) + ":ability_score_improvement");
            require(entitlements.emplace(grant.source_id, grant.level).second);
            ++advancement_count;
            if (grant.id == "feat:ability_score_improvement")
            {
                unsigned points = 0;
                for (const auto &[key, value] : grant.choices)
                {
                    const auto found = std::find(ability_ids.begin(), ability_ids.end(), key);
                    require(found != ability_ids.end() && (value == "1" || value == "2"));
                    const auto amount = value == "1" ? 1 : 2;
                    effects.abilities[rules::all_abilities[found - ability_ids.begin()]] += amount;
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
                            grant.id == "feat:skilled" ||
                            grant.id == "feat:magic_initiate_cleric" ||
                            grant.id == "feat:magic_initiate_druid" ||
                            grant.id == "feat:magic_initiate_wizard");
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
    if (klass == CharacterClass::ranger && level >= 7)
        require(entitlements.contains({std::string(defensive_tactics), 0}));
    if (klass == CharacterClass::cleric && level >= 7)
        require(entitlements.contains({std::string(blessed_strikes), 0}));
    if (klass == CharacterClass::sorcerer && level >= 6)
        require(entitlements.contains({std::string(elemental_affinity), 0}));
    if (klass == CharacterClass::warlock && level >= 10)
        require(entitlements.contains({std::string(fiendish_resilience), 0}));
    if (klass == CharacterClass::bard && level >= 6)
        require(entitlements.contains({"subclass:bard:discoveries", 1}));
    if (klass == CharacterClass::warlock)
        for (const unsigned arcanum : {11u, 13u, 15u})
            if (level >= arcanum)
                require(entitlements.contains({"class:warlock:arcanum:" + std::to_string(arcanum), 0}));
    if (klass == CharacterClass::fighter && level >= 7)
        require(entitlements.contains({std::string(champion_style), 0}));
    require(required.empty() && advancement_count == feats_by(klass, level));
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
    require(static_cast<bool>(in) && count <= 64);
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
