#include "spell_access.h"
#include <algorithm>
#include <array>
#include <set>
#include <stdexcept>

namespace opengold::srd5::detail
{
using namespace rules;

namespace
{
constexpr std::string_view source = "class:wizard:spellcasting";

// Class spell lists a row belongs to. A per-row property means a new spell
// describes itself in one place.
enum SpellList : unsigned
{
    wizard_list = 1,
    cleric_list = 2,
    paladin_list = 4,
    ranger_list = 8,
    druid_list = 16
};

struct Spell
{
    std::string_view id, label;
    unsigned level, mask;
    unsigned lists{wizard_list};
};

// Existing spell implementations only. This is not any class's complete list.
constexpr std::array spells{Spell{"chill_touch", "Chill Touch", 0, 2048},
    Spell{"shocking_grasp", "Shocking Grasp", 0, 1024},
    Spell{"eldritch_blast", "Eldritch Blast", 0, 512, 0},
    Spell{"ray_of_frost", "Ray of Frost", 0, 256},
    Spell{"sacred_flame", "Sacred Flame", 0, 128, cleric_list},
    Spell{"fire_bolt", "Fire Bolt", 0, 1},
    Spell{"poison_spray", "Poison Spray", 0, 64, wizard_list | druid_list},
    Spell{"spare_the_dying", "Spare the Dying", 0, 0, cleric_list | druid_list},
    Spell{"magic_missile", "Magic Missile", 1, 4},
    Spell{"scorching_ray", "Scorching Ray", 2, 16},
    Spell{"blindness", "Blindness", 2, 32, wizard_list | cleric_list},
    Spell{"inflict_wounds", "Inflict Wounds", 1, 0, cleric_list},
    Spell{"cure_wounds", "Cure Wounds", 1, 0, cleric_list | paladin_list | ranger_list},
    Spell{"healing_word", "Healing Word", 1, 0, cleric_list},
    Spell{"divine_smite", "Divine Smite", 1, 0, paladin_list},
    Spell{"searing_smite", "Searing Smite", 1, 0, paladin_list},
    Spell{"shield_of_faith", "Shield of Faith", 1, 0, cleric_list | paladin_list},
    Spell{"heroism", "Heroism", 1, 0, paladin_list},
    Spell{"divine_favor", "Divine Favor", 1, 0, paladin_list},
    Spell{"bless", "Bless", 1, 0, cleric_list | paladin_list},
    // Also on other class lists; added with those classes' increments.
    Spell{"protection_from_evil_and_good", "Protection from Evil and Good", 1, 0,
        cleric_list | paladin_list},
    Spell{"command", "Command", 1, 0, cleric_list | paladin_list},
    Spell{"hunters_mark", "Hunter's Mark", 1, 0, ranger_list},
    // Also on the Bard, Druid and Wizard lists; added with those classes.
    Spell{"longstrider", "Longstrider", 1, 0, ranger_list},
    Spell{"goodberry", "Goodberry", 1, 0, ranger_list | druid_list},
    Spell{"ensnaring_strike", "Ensnaring Strike", 1, 0, ranger_list},
    Spell{"entangle", "Entangle", 1, 0, ranger_list | druid_list},
    // Also on the Sorcerer and Wizard lists; added with those classes.
    Spell{"fog_cloud", "Fog Cloud", 1, 0, ranger_list | druid_list},
    // Also on the Bard, Druid, Paladin and Ranger lists; added with those classes.
    Spell{"lesser_restoration", "Lesser Restoration", 2, 0, cleric_list},
    // Also on the Bard, Druid, Paladin and Ranger lists; added with those classes.
    Spell{"aid", "Aid", 2, 0, cleric_list},
    Spell{"guiding_bolt", "Guiding Bolt", 1, 0, cleric_list},
    // Also on the Bard and Warlock lists; added with those classes.
    Spell{"bane", "Bane", 1, 0, cleric_list}};

// A class that prepares spells from its whole class list instead of a
// spellbook. Arrays are indexed by class level minus one (levels 1-4).
struct PreparedCaster
{
    std::string_view klass, source, cantrip_label;
    unsigned list, cantrip_list;
    std::array<unsigned, 4> cantrips, prepared, highest_slot;
    // A Paladin or Ranger replaces one prepared spell after a Long Rest; a Cleric any.
    bool rest_replaces_one;
};

constexpr std::array prepared_casters{
    PreparedCaster{
        "Cleric", "class:cleric:spellcasting", "Cleric cantrips", cleric_list, cleric_list,
        {3, 3, 3, 4}, {4, 5, 6, 7}, {1, 1, 2, 2}, false},
    PreparedCaster{
        // Paladin cantrips come only from Blessed Warrior, from the Cleric list.
        "Paladin", "class:paladin:spellcasting", "Blessed Warrior cantrips", paladin_list,
        cleric_list, {0, 0, 0, 0},
        {2, 3, 4, 5}, {1, 1, 1, 1}, true},
    // Ranger cantrips come only from Druidic Warrior, from the Druid list.
    PreparedCaster{
        "Ranger", "class:ranger:spellcasting", "Druidic Warrior cantrips", ranger_list,
        druid_list,
        {0, 0, 0, 0}, {2, 3, 4, 4}, {1, 1, 1, 1}, true}};

// Spells a class always has prepared from a class level on.
struct AlwaysPrepared
{
    std::string_view klass, spell;
    unsigned level;
};

// Paladin's Smite, SRD 5.2.1 p. 54, the Oath of Devotion spells, p. 56, the
// Ranger's Favored Enemy and the Cleric's Life Domain.
constexpr std::array always_prepared_table{
    AlwaysPrepared{"Paladin", "divine_smite", 2},
    AlwaysPrepared{"Paladin", "protection_from_evil_and_good", 3},
    AlwaysPrepared{"Paladin", "shield_of_faith", 3},
    // Favored Enemy, SRD 5.2.1 p. 57.
    AlwaysPrepared{"Ranger", "hunters_mark", 1},
    // Life Domain spells, p. 37.
    AlwaysPrepared{"Cleric", "aid", 3},
    AlwaysPrepared{"Cleric", "bless", 3},
    AlwaysPrepared{"Cleric", "cure_wounds", 3},
    AlwaysPrepared{"Cleric", "lesser_restoration", 3}};

const PreparedCaster *prepared_caster(std::string_view klass)
{
    for (const auto &caster : prepared_casters)
        if (caster.klass == klass)
            return &caster;
    return nullptr;
}

void require(bool ok)
{
    if (!ok)
        throw std::runtime_error("Invalid spell grant, spellbook entry or preparation");
}

const Spell &find(std::string_view id)
{
    for (const auto &spell : spells)
        if (spell.id == id)
            return spell;
    throw std::runtime_error("Unsupported spell knowledge");
}

FeatureGrant grant(std::string_view id, unsigned level, std::string_view origin = source)
{
    return {"spell:" + std::string(id),
            std::string(origin),
            level,
    {{"access", find(id).level ? "spellbook" : "cantrip"}}};
}

std::string_view class_source(std::string_view klass)
{
    const auto *caster = prepared_caster(klass);
    return caster ? caster->source : source;
}

// The Thaumaturge Divine Order adds one Cleric cantrip.
bool thaumaturge(std::span<const FeatureGrant> grants)
{
    return std::find(grants.begin(), grants.end(),
                     FeatureGrant{"order:thaumaturge", "class:cleric:divine_order", 1, {}}) !=
           grants.end();
}

// Paladin's Blessed Warrior Fighting Style adds two Cleric cantrips at level two.
bool blessed_warrior(std::span<const FeatureGrant> grants)
{
    return std::find(grants.begin(), grants.end(),
                     FeatureGrant{"feature:blessed_warrior", "class:paladin:fighting_style", 2,
                                  {}}) != grants.end();
}

// The Ranger's Druidic Warrior Fighting Style adds two Druid cantrips at level two.
bool druidic_warrior(std::span<const FeatureGrant> grants)
{
    return std::find(grants.begin(), grants.end(),
                     FeatureGrant{"feature:druidic_warrior", "class:ranger:fighting_style", 2,
                                  {}}) != grants.end();
}

// The cantrips a caster knows at a class level, its features included.
unsigned cantrips_at(const PreparedCaster &caster, std::span<const FeatureGrant> grants,
                     unsigned level)
{
    return caster.cantrips[level - 1] +
           (caster.klass == "Cleric" && thaumaturge(grants) ? 1 : 0) +
           (caster.klass == "Paladin" && level >= 2 && blessed_warrior(grants) ? 2 : 0) +
           (caster.klass == "Ranger" && level >= 2 && druidic_warrior(grants) ? 2 : 0);
}

SpellAccess prepared_access(const PreparedCaster &caster, std::span<const FeatureGrant> grants,
                            unsigned level, std::span<const std::string> prepared)
{
    require(level >= 1 && level <= 4);
    // "class:cleric:spellcasting" is granted by the "class:cleric" Spellcasting feature.
    const std::string feature_source(caster.source.substr(0, caster.source.rfind(':')));
    require(std::find(grants.begin(), grants.end(),
                      FeatureGrant{"feature:spellcasting", feature_source, 1, {}}) != grants.end());
    SpellAccess result;
    result.cantrip_choices = cantrips_at(caster, grants, level);
    result.prepared_choices = caster.prepared[level - 1];
    result.always_prepared = always_prepared_spells(caster.klass, level);
    std::set<std::string> known;
    std::array<unsigned, 5> learned{};
    for (const auto &g : grants)
        if (is_spell_grant(g))
        {
            const auto &spell = find(std::string_view(g.id).substr(6));
            // Cantrips are learned at level one or at a level whose count grows.
            const bool learning_level =
                g.level == 1 || (g.level >= 2 && g.level <= 4 &&
                                 cantrips_at(caster, grants, g.level) >
                                 cantrips_at(caster, grants, g.level - 1));
            require((spell.lists & caster.cantrip_list) && spell.level == 0 && learning_level &&
                    g.level <= level && known.insert(g.id).second &&
                    g == grant(spell.id, g.level, caster.source));
            result.cantrips.push_back(
            {std::string(spell.id), std::string(spell.label), g.source_id, g.level});
            ++learned[g.level];
        }
    require(learned[1] <= cantrips_at(caster, grants, 1));
    for (unsigned n = 2; n <= 4; ++n)
        require(learned[n] <= cantrips_at(caster, grants, n) - cantrips_at(caster, grants, n - 1));
    std::set<std::string> selected;
    for (const auto &id : prepared)
    {
        const auto &spell = find(id);
        require((spell.lists & caster.list) && spell.level >= 1 &&
                spell.level <= caster.highest_slot[level - 1] && selected.insert(id).second);
        result.prepared.push_back(id);
    }
    require(result.prepared.size() <= result.prepared_choices);
    return result;
}

// Gaining a level whose cantrip count grows learns cantrips; a Long Rest
// changes prepared spells. Creation is level one. Replacing a cantrip on
// gaining a level waits for a second implemented cantrip on the class list.
SpellChoiceOptions prepared_choice_options(const PreparedCaster &caster,
        const CharacterSheet &sheet, SpellChoiceContext context)
{
    SpellChoiceOptions result;
    const auto access = prepared_access(caster, sheet.grants, sheet.level, sheet.prepared_spells);
    const auto known = [&](std::string_view id)
    {
        return std::any_of(access.cantrips.begin(), access.cantrips.end(),
                           [&](const auto & s)
        {
            return s.id == id;
        });
    };
    const unsigned level = sheet.level;
    const unsigned capacity = level > 1 ? cantrips_at(caster, sheet.grants, level) -
                              cantrips_at(caster, sheet.grants, level - 1)
                              : 0;
    if (context == SpellChoiceContext::advancement && capacity)
    {
        const unsigned used = std::count_if(sheet.grants.begin(), sheet.grants.end(),
                                            [&](const auto & g)
        {
            return is_spell_grant(g) && g.level == level;
        });
        if (used < capacity)
        {
            TrainingChoiceGroup group{"cantrips:" + std::to_string(level),
                                      std::string(caster.cantrip_label), capacity - used, {}};
            group.acquired_level = level;
            for (const auto &spell : spells)
                if ((spell.lists & caster.cantrip_list) && spell.level == 0 && !known(spell.id))
                    group.options.push_back({std::string(spell.id), std::string(spell.label), {}});
            result.learning.push_back(std::move(group));
        }
    }
    result.prepared_count = access.prepared_choices;
    result.may_prepare = true;
    for (const auto &spell : spells)
        if ((spell.lists & caster.list) && spell.level >= 1 &&
                spell.level <= caster.highest_slot[level - 1] &&
                std::find(access.always_prepared.begin(), access.always_prepared.end(),
                          spell.id) == access.always_prepared.end())
            result.preparation.push_back({std::string(spell.id), std::string(spell.label), {}});
    // A spell that becomes always prepared at this level frees its place.
    if (context == SpellChoiceContext::advancement)
        for (const auto &id : sheet.prepared_spells)
            if (std::find(access.always_prepared.begin(), access.always_prepared.end(), id) ==
                    access.always_prepared.end())
                result.locked_prepared.push_back(id);
    return result;
}
} // namespace

bool is_spell_grant(const FeatureGrant &grant)
{
    return grant.id.starts_with("spell:");
}

std::vector<FeatureGrant> without_spell_grants(std::span<const FeatureGrant> grants)
{
    std::vector<FeatureGrant> result;
    for (const auto &g : grants)
        if (!is_spell_grant(g))
            result.push_back(g);
    return result;
}

TrainingChoiceGroup starting_cantrip_options(std::string_view klass)
{
    if (klass == "warlock")
        return
    {
        "class:warlock:pact_magic",
        "Warlock cantrips",
        2,
        {   {
                "eldritch_blast", "Eldritch Blast",
                "Ranged spell attack: 1d10 Force damage, 120 feet; creature targets currently supported."
            },
            {"poison_spray", "Poison Spray", "Ranged spell attack: 1d12 Poison damage, 30 feet."},
            {
                "chill_touch", "Chill Touch",
                "Melee spell attack: 1d10 Necrotic damage, Touch; prevents healing until the end of your next turn."
            }
        }};
    if (klass == "cleric")
        return {"class:cleric:spellcasting",
                "Cleric cantrips",
                3,
    {   {
            "sacred_flame", "Sacred Flame",
            "Dexterity save: 1d8 Radiant damage, visible creature within 60 feet."
        },
        {
            "spare_the_dying", "Spare the Dying",
            "A creature at 0 Hit Points within 15 feet becomes Stable."
        }
    }};
    if (klass != "wizard" && klass != "sorcerer")
        return {};
    return
    {
        klass == "sorcerer" ? "class:sorcerer:spellcasting" : std::string(source),
        klass == "sorcerer" ? "Sorcerer cantrips" : "Wizard cantrips",
        klass == "sorcerer" ? 4u : 3u,
        {   {"fire_bolt", "Fire Bolt", "Ranged spell attack: 1d10 Fire damage, 120 feet."},
            {"poison_spray", "Poison Spray", "Ranged spell attack: 1d12 Poison damage, 30 feet."},
            {
                "ray_of_frost", "Ray of Frost",
                "Ranged spell attack: 1d8 Cold damage, 60 feet; Speed reduced by 10 feet until your next turn."
            },
            {
                "shocking_grasp", "Shocking Grasp",
                "Melee spell attack: 1d8 Lightning damage, Touch; prevents Opportunity Attacks until the target’s next turn."
            },
            {
                "chill_touch", "Chill Touch",
                "Melee spell attack: 1d10 Necrotic damage, Touch; prevents healing until the end of your next turn."
            }
        }};
}

std::vector<FeatureGrant>
starting_spell_grants(std::string_view klass,
                      const std::optional<std::vector<std::string>> &cantrips)
{
    if (klass == "sorcerer")
    {
        std::vector<FeatureGrant> result;
        std::set<std::string> unique;
        const auto chosen = cantrips.value_or(std::vector<std::string> {});
        require(chosen.size() <= 4);
        for (const auto &id : chosen)
        {
            require((id == "fire_bolt" || id == "poison_spray" || id == "ray_of_frost" ||
                     id == "shocking_grasp" || id == "chill_touch") &&
                    unique.insert(id).second);
            result.push_back(grant(id, 1, "class:sorcerer:spellcasting"));
        }
        return result;
    }
    if (klass == "warlock")
    {
        require(!cantrips || cantrips->size() <= 2);
        std::vector<FeatureGrant> result;
        std::set<std::string> unique;
        for (const auto &id : cantrips.value_or(std::vector<std::string> {}))
        {
            require((id == "eldritch_blast" || id == "poison_spray" || id == "chill_touch") &&
                    unique.insert(id).second);
            result.push_back(grant(id, 1, "class:warlock:pact_magic"));
        }
        return result;
    }
    if (klass == "cleric")
    {
        std::vector<FeatureGrant> result;
        std::set<std::string> unique;
        const auto offered = starting_cantrip_options("cleric").options;
        for (const auto &id : cantrips.value_or(std::vector<std::string> {}))
        {
            require(std::any_of(offered.begin(), offered.end(), [&](const auto & option)
            {
                return option.id == id;
            }) && unique.insert(id).second);
            result.push_back(grant(id, 1, "class:cleric:spellcasting"));
        }
        return result;
    }
    if (klass != "wizard")
    {
        require(!cantrips || cantrips->empty());
        return {};
    }
    const auto chosen = cantrips.value_or(std::vector<std::string> {"fire_bolt"});
    require(chosen.size() <= 3);
    std::set<std::string> unique;
    std::vector<FeatureGrant> result;
    for (const auto &id : chosen)
    {
        require((id == "fire_bolt" || id == "poison_spray" || id == "ray_of_frost" ||
                 id == "shocking_grasp" || id == "chill_touch") &&
                unique.insert(id).second);
        result.push_back(grant(id, 1));
    }
    result.push_back(grant("magic_missile", 1));
    return result;
}

SpellAccess spell_access(std::span<const FeatureGrant> grants, std::string_view klass,
                         unsigned level, std::span<const std::string> prepared)
{
    SpellAccess result;
    if (klass == "Sorcerer")
    {
        // Starting cantrips only. Leveled spells, replacement and advancement remain #132.
        require(level == 1 && prepared.empty());
        result.cantrip_choices = 4;
        std::set<std::string> known;
        for (const auto &g : grants)
            if (is_spell_grant(g))
            {
                require(g.id == "spell:fire_bolt" || g.id == "spell:poison_spray" ||
                        g.id == "spell:ray_of_frost" || g.id == "spell:shocking_grasp" ||
                        g.id == "spell:chill_touch");
                const auto &spell = find(std::string_view(g.id).substr(6));
                require(g == grant(spell.id, 1, "class:sorcerer:spellcasting") &&
                        known.insert(g.id).second);
                result.cantrips.push_back(
                {std::string(spell.id), std::string(spell.label), g.source_id, g.level});
            }
        require(result.cantrips.size() <= 4);
        return result;
    }
    if (klass == "Warlock")
    {
        // Cantrip portion of Pact Magic only; slots and advancement remain separate.
        require(level == 1 && prepared.empty());
        result.cantrip_choices = 2;
        std::set<std::string> known;
        for (const auto &g : grants)
            if (is_spell_grant(g))
            {
                require(g.id == "spell:eldritch_blast" || g.id == "spell:poison_spray" ||
                        g.id == "spell:chill_touch");
                const auto &spell = find(std::string_view(g.id).substr(6));
                require(g == grant(spell.id, 1, "class:warlock:pact_magic") &&
                        known.insert(g.id).second);
                result.cantrips.push_back(
                {std::string(spell.id), std::string(spell.label), g.source_id, g.level});
            }
        require(result.cantrips.size() <= 2);
        return result;
    }
    if (const auto *caster = prepared_caster(klass))
        return prepared_access(*caster, grants, level, prepared);
    if (klass != "Wizard")
    {
        require(std::none_of(grants.begin(), grants.end(), is_spell_grant));
        return result; // Each other class's preparation policy has its own issue.
    }
    require(level >= 1 && level <= 4);
    require(std::find(grants.begin(), grants.end(),
                      FeatureGrant{"feature:spellcasting", "class:wizard", 1, {}}) != grants.end());
    result.cantrip_choices = level == 4 ? 4 : 3;
    result.spellbook_choices = 6 + 2 * (level - 1);
    result.prepared_choices = level + 3;
    std::set<std::string> known;
    std::array<unsigned, 5> books{}, cantrips{};
    for (const auto &g : grants)
        if (is_spell_grant(g))
        {
            require(g.source_id == source && g.level >= 1 && g.level <= level &&
                    known.insert(g.id).second);
            const auto &spell = find(std::string_view(g.id).substr(6));
            require(spell.lists & wizard_list);
            auto expected = grant(spell.id, g.level);
            unsigned learned = g.level;
            if (const auto replacement = g.choices.find("learned_at");
                    replacement != g.choices.end())
            {
                require(spell.level == 0);
                bool valid = false;
                for (unsigned n = g.level; n <= level; ++n)
                    if (replacement->second == std::to_string(n))
                    {
                        learned = n;
                        valid = true;
                    }
                require(valid);
                expected.choices.emplace("learned_at", replacement->second);
            }
            require(g == expected);
            require(spell.level == 0 || spell.level <= (g.level >= 3 ? 2u : 1u));
            auto &list = spell.level ? result.spellbook : result.cantrips;
            list.push_back({std::string(spell.id), std::string(spell.label), g.source_id, learned});
            if (spell.level)
                ++books[g.level];
            else
                ++cantrips[g.level];
        }
    require(books[1] <= 6 && cantrips[1] <= 3 && cantrips[2] == 0 && cantrips[3] == 0 &&
            cantrips[4] <= 1);
    for (unsigned n = 2; n <= level; ++n)
        require(books[n] <= 2);
    std::set<std::string> selected;
    for (const auto &id : prepared)
    {
        require(selected.insert(id).second &&
                std::any_of(result.spellbook.begin(), result.spellbook.end(),
                            [&](const auto & s)
        {
            return s.id == id;
        }));
        result.prepared.push_back(id);
    }
    require(result.prepared.size() <= result.prepared_choices);
    return result;
}

SpellChoiceOptions spell_choice_options(const CharacterSheet &sheet, SpellChoiceContext context)
{
    SpellChoiceOptions result;
    if (const auto *caster = prepared_caster(sheet.character_class))
        return prepared_choice_options(*caster, sheet, context);
    if (sheet.character_class != "Wizard")
        return result;
    const auto access =
        spell_access(sheet.grants, sheet.character_class, sheet.level, sheet.prepared_spells);
    auto known = [&](std::string_view id)
    {
        return std::any_of(sheet.grants.begin(), sheet.grants.end(),
                           [&](const auto & g)
        {
            return g.id == "spell:" + std::string(id);
        });
    };
    // Advancement (including creation at level one) learns the current level's
    // spells; a Long Rest only prepares or replaces.
    if (context == SpellChoiceContext::advancement)
    {
        const unsigned level = sheet.level;
        for (bool cantrip :
                {
                    true, false
                })
        {
            const unsigned capacity = cantrip ? (level == 1   ? 3
                                                 : level == 4 ? 1
                                                 : 0)
                                      : (level == 1 ? 6 : 2);
            const unsigned used = std::count_if(
                                      sheet.grants.begin(), sheet.grants.end(),
                                      [&](const auto & g)
            {
                return is_spell_grant(g) && g.level == level &&
                       (find(std::string_view(g.id).substr(6)).level == 0) == cantrip;
            });
            if (capacity == used)
                continue;
            TrainingChoiceGroup group;
            group.id =
                std::string(cantrip ? "cantrips:" : "spellbook:") + std::to_string(level);
            group.label = cantrip ? "Wizard cantrips" : "Spellbook";
            group.count = capacity - used;
            group.acquired_level = level;
            for (const auto &spell : spells)
                if ((spell.level == 0) == cantrip && (spell.lists & wizard_list) &&
                        spell.level <= (level >= 3 ? 2u : 1u) && !known(spell.id))
                    group.options.push_back(
                {std::string(spell.id), std::string(spell.label), {}});
            result.learning.push_back(std::move(group));
        }
    }
    result.prepared_count = access.prepared_choices;
    result.may_prepare = true;
    for (const auto &spell : access.spellbook)
        result.preparation.push_back({spell.id, spell.label, {}});
    if (context == SpellChoiceContext::advancement)
        result.locked_prepared = sheet.prepared_spells;
    result.may_replace = context == SpellChoiceContext::long_rest;
    if (result.may_replace)
    {
        for (const auto &spell : access.cantrips)
            result.replaceable.push_back({spell.id, spell.label, {}});
        for (const auto &choice : starting_cantrip_options("wizard").options)
            if (!known(choice.id))
                result.replacements.push_back(choice);
    }
    return result;
}

void apply_spell_choices(CharacterSheet &sheet, const SpellChoices &choices,
                         SpellChoiceContext context, bool complete)
{
    const auto *caster = prepared_caster(sheet.character_class);
    require(sheet.character_class == "Wizard" || caster);
    const auto origin = class_source(sheet.character_class);
    auto candidate = sheet;
    const auto options = spell_choice_options(sheet, context);
    for (const auto &[id, values] : choices.learning)
    {
        const auto group = std::find_if(options.learning.begin(), options.learning.end(),
                                        [&](const auto & g)
        {
            return g.id == id;
        });
        require(group != options.learning.end() && values.size() <= group->count);
        for (const auto &value : values)
        {
            require(std::any_of(group->options.begin(), group->options.end(),
                                [&](const auto & o)
            {
                return o.id == value;
            }));
            candidate.grants.push_back(grant(value, group->acquired_level, origin));
        }
    }
    require(choices.replace_cantrip.empty() == choices.replacement.empty());
    if (!choices.replace_cantrip.empty())
    {
        require(options.may_replace &&
                std::any_of(options.replaceable.begin(), options.replaceable.end(),
                            [&](const auto & o)
        {
            return o.id == choices.replace_cantrip;
        }) &&
        std::any_of(options.replacements.begin(), options.replacements.end(),
                    [&](const auto & o)
        {
            return o.id == choices.replacement;
        }));
        auto existing = std::find_if(candidate.grants.begin(), candidate.grants.end(),
                                     [&](const auto & g)
        {
            return g.id == "spell:" + choices.replace_cantrip;
        });
        *existing = grant(choices.replacement, existing->level, origin);
        existing->choices.emplace("learned_at", std::to_string(sheet.level));
    }
    if (choices.prepared)
    {
        require(options.may_prepare);
        for (const auto &id : options.locked_prepared)
            require(std::find(choices.prepared->begin(), choices.prepared->end(), id) !=
                    choices.prepared->end());
        if (caster && caster->rest_replaces_one && context == SpellChoiceContext::long_rest)
            require(std::count_if(sheet.prepared_spells.begin(), sheet.prepared_spells.end(),
                                  [&](const auto & id)
        {
            return std::find(choices.prepared->begin(), choices.prepared->end(), id) ==
                   choices.prepared->end();
        }) <= 1);
        const auto always = always_prepared_spells(sheet.character_class, sheet.level);
        for (const auto &id : *choices.prepared)
            require(std::find(always.begin(), always.end(), id) == always.end());
        candidate.prepared_spells = *choices.prepared;
    }
    const auto access = spell_access(candidate.grants, candidate.character_class, candidate.level,
                                     candidate.prepared_spells);
    if (complete)
    {
        const auto remaining = spell_choice_options(candidate, context);
        for (const auto &group : remaining.learning)
            if (!group.options.empty() && group.count)
                throw std::runtime_error("Complete the available spell choices.");
        if (options.may_prepare)
            require(candidate.prepared_spells.size() ==
                    std::min<std::size_t>(access.prepared_choices, remaining.preparation.size()));
    }
    sheet = std::move(candidate);
}

std::vector<std::string> always_prepared_spells(std::string_view klass, unsigned level)
{
    std::vector<std::string> result;
    for (const auto &entry : always_prepared_table)
        if (entry.klass == klass && level >= entry.level)
            result.emplace_back(entry.spell);
    return result;
}

bool prepares_spells(std::string_view klass)
{
    return klass == "Wizard" || prepared_caster(klass);
}

std::vector<std::string> known_cantrip_ids(const SpellAccess &access)
{
    std::vector<std::string> result;
    // find() still rejects an unsupported id, as the mask lookup used to.
    for (const auto &s : access.cantrips)
    {
        (void)find(s.id);
        result.push_back(s.id);
    }
    return result;
}

std::vector<std::string> casting_ids(const SpellAccess &access)
{
    auto result = known_cantrip_ids(access);
    for (const auto &id : access.prepared)
    {
        (void)find(id);
        result.push_back(id);
    }
    // A spell prepared before a feature made it always prepared appears once.
    for (const auto &id : access.always_prepared)
        if (std::find(result.begin(), result.end(), id) == result.end())
            result.push_back(id);
    return result;
}
} // namespace opengold::srd5::detail
