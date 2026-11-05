#include "weapon_mastery.h"
#include <algorithm>
#include <set>
#include <stdexcept>

namespace opengold::srd5::detail
{
namespace
{
void require(bool value)
{
    if (!value)
    {
        throw std::runtime_error("Invalid Weapon Mastery choices or provenance");
    }
}

unsigned count(CharacterClass klass, unsigned acquired)
{
    if (acquired == 4 || acquired == 10)
    {
        return klass == CharacterClass::fighter || klass == CharacterClass::barbarian ? 1 : 0;
    }
    if (acquired != 1)
    {
        return 0;
    }
    return klass == CharacterClass::fighter ? 3
           : klass == CharacterClass::barbarian || klass == CharacterClass::paladin ||
                   klass == CharacterClass::ranger || klass == CharacterClass::rogue
               ? 2
               : 0;
}

std::string source(CharacterClass klass, unsigned acquired)
{
    return "class:" + std::string(class_id(klass)) + ":weapon_mastery" +
           (acquired == 1 ? "" : ":" + std::to_string(acquired));
}

// Firearms never appear in Pool of Radiance's world or its shops, so no one
// trains with them.
bool eligible(CharacterClass klass, const Weapon &item)
{
    return count(klass, 1) && item.mastery != Mastery::none && weapon_proficient(klass, item) &&
           (klass != CharacterClass::barbarian || !item.ranged) &&
           item.ammunition != Ammunition::firearm_bullet;
}
} // namespace

std::string_view mastery_name(Mastery mastery)
{
    switch (mastery)
    {
    case Mastery::cleave:
        return "Cleave";
    case Mastery::graze:
        return "Graze";
    case Mastery::nick:
        return "Nick";
    case Mastery::push:
        return "Push";
    case Mastery::sap:
        return "Sap";
    case Mastery::slow:
        return "Slow";
    case Mastery::topple:
        return "Topple";
    case Mastery::vex:
        return "Vex";
    default:
        return {};
    }
}

bool is_mastery_grant(const rules::FeatureGrant &grant)
{
    return grant.id.starts_with("mastery:");
}

rules::TrainingChoiceGroup mastery_options(CharacterClass klass, unsigned acquired,
                                           std::span<const rules::FeatureGrant> grants)
{
    rules::TrainingChoiceGroup group{
        source(klass, acquired), "Weapon Mastery", count(klass, acquired), {}};
    group.acquired_level = acquired;
    group.continuity_id = acquired == 1 ? "weapon_mastery" : "";
    if (!group.count)
    {
        return group;
    }
    for (const auto &item : weapons)
    {
        if (eligible(klass, item) && std::none_of(grants.begin(), grants.end(),
                                                  [&](const auto &g)
                                                  {
                                                      return g.id == "mastery:" +
                                                                         std::string(item.key) &&
                                                             g.source_id != group.id;
                                                  }))
        {
            group.options.push_back({std::string(item.key), std::string(item.label),
                                     std::string(mastery_name(item.mastery))});
        }
    }
    return group;
}

namespace
{
// The class and its level a mastery grant's source names:
// "class:fighter:weapon_mastery:4" is a Fighter's at its level 4.
std::pair<CharacterClass, unsigned> mastery_entitlement(std::string_view source_id)
{
    constexpr std::string_view prefix = "class:", infix = ":weapon_mastery";
    require(source_id.starts_with(prefix));
    const auto rest = source_id.substr(prefix.size());
    const auto at = rest.find(infix);
    require(at != std::string_view::npos);
    const auto klass = class_from_id(rest.substr(0, at));
    const auto tail = rest.substr(at + infix.size());
    const unsigned acquired = tail.empty() ? 1u : tail == ":4" ? 4u : tail == ":10" ? 10u : 0u;
    require(acquired && source(klass, acquired) == source_id);
    return {klass, acquired};
}
} // namespace

rules::TrainingChoices mastery_choices(std::span<const rules::FeatureGrant> grants,
                                       const ClassLevels &levels)
{
    require(levels.total() >= 1 && levels.total() <= 20);
    rules::TrainingChoices result;
    std::set<std::string> kinds;
    for (const auto &grant : grants)
    {
        if (is_mastery_grant(grant) || grant.source_id.find(":weapon_mastery") != std::string::npos)
        {
            // Each class's masteries come from its own levels (MULTI-1).
            const auto [klass, acquired] = mastery_entitlement(grant.source_id);
            require(is_mastery_grant(grant) && levels[klass] >= static_cast<int>(acquired) &&
                    grant.level <= static_cast<unsigned>(levels.total()) && grant.choices.empty());
            const auto group = mastery_options(klass, acquired);
            const auto key = grant.id.substr(8);
            require(group.count && kinds.insert(key).second);
            require(std::any_of(group.options.begin(), group.options.end(),
                                [&](const auto &o)
                                {
                                    return o.id == key;
                                }));
            auto &choices = result[group.id];
            choices.push_back(key);
            require(choices.size() <= group.count);
        }
    }
    return result;
}

std::string mastery_source(CharacterClass klass, unsigned acquired_level)
{
    return source(klass, acquired_level);
}

unsigned mastery_replacements(const ClassLevels &levels)
{
    unsigned total = 0;
    for (const auto klass : {CharacterClass::fighter, CharacterClass::barbarian})
    {
        total += levels[klass] ? 1u : 0u;
    }
    for (const auto klass : {CharacterClass::rogue, CharacterClass::paladin, CharacterClass::ranger})
    {
        total += levels[klass] ? 2u : 0u;
    }
    return total;
}

std::vector<rules::FeatureGrant> replace_masteries(std::span<const rules::FeatureGrant> grants,
                                                   const ClassLevels &levels,
                                                   std::span<const std::string> selected)
{
    const auto previous = mastery_choices(grants, levels);
    unsigned capacity = 0;
    for (const auto klass :
         {CharacterClass::barbarian, CharacterClass::fighter, CharacterClass::paladin,
          CharacterClass::ranger, CharacterClass::rogue})
    {
        for (const unsigned acquired : mastery_levels)
        {
            if (static_cast<int>(acquired) <= levels[klass])
            {
                const auto n = count(klass, acquired);
                capacity += n;
                const auto found = previous.find(source(klass, acquired));
                require(!n || (found != previous.end() && found->second.size() == n));
            }
        }
    }
    require(capacity && selected.size() == capacity);
    std::set<std::string> distinct;
    for (const auto &key : selected)
    {
        const auto *item = weapon(key);
        require(item && distinct.insert(key).second);
    }
    std::vector<rules::FeatureGrant> result(grants.begin(), grants.end());
    std::vector<std::size_t> vacant;
    std::set<std::string> retained;
    for (std::size_t i = 0; i < result.size(); ++i)
    {
        if (is_mastery_grant(result[i]))
        {
            const auto key = result[i].id.substr(8);
            if (distinct.contains(key))
            {
                retained.insert(key);
            }
            else
            {
                vacant.push_back(i);
            }
        }
    }
    require(vacant.size() <= mastery_replacements(levels));
    // Each new kind takes a vacant place of a class that can master it.
    for (const auto &key : selected)
    {
        if (retained.contains(key))
        {
            continue;
        }
        const auto *item = weapon(key);
        const auto place = std::find_if(vacant.begin(), vacant.end(),
                                        [&](std::size_t i)
                                        {
                                            return eligible(
                                                mastery_entitlement(result[i].source_id).first, *item);
                                        });
        require(place != vacant.end());
        result[*place].id = "mastery:" + key;
        vacant.erase(place);
    }
    (void)mastery_choices(result, levels);
    return result;
}
} // namespace opengold::srd5::detail
