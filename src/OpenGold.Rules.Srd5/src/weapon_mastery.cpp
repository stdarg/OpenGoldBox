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
        throw std::runtime_error("Invalid Weapon Mastery choices or provenance");
}

unsigned count(CharacterClass klass, unsigned acquired)
{
    if (acquired == 4)
        return klass == CharacterClass::fighter || klass == CharacterClass::barbarian ? 1 : 0;
    if (acquired != 1)
        return 0;
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

bool eligible(CharacterClass klass, const Weapon &item)
{
    return count(klass, 1) && item.mastery != Mastery::none && weapon_proficient(klass, item) &&
           (klass != CharacterClass::barbarian || !item.ranged);
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
    rules::TrainingChoiceGroup group
    {
        source(klass, acquired), "Weapon Mastery", count(klass, acquired), {}};
    group.acquired_level = acquired;
    group.continuity_id = acquired == 1 ? "weapon_mastery" : "";
    if (!group.count)
        return group;
    for (const auto &item : weapons)
        if (eligible(klass, item) && std::none_of(grants.begin(), grants.end(),
                [&](const auto & g)
    {
        return g.id == "mastery:" +
               std::string(item.key) &&
                   g.source_id != group.id;
        }))
    group.options.push_back({std::string(item.key), std::string(item.label),
                             std::string(mastery_name(item.mastery))});
    return group;
}

rules::TrainingChoices mastery_choices(std::span<const rules::FeatureGrant> grants,
                                       CharacterClass klass, unsigned level)
{
    require(level >= 1 && level <= 20);
    rules::TrainingChoices result;
    std::set<std::string> kinds;
    for (const auto &grant : grants)
        if (is_mastery_grant(grant) || grant.source_id.find(":weapon_mastery") != std::string::npos)
        {
            require(is_mastery_grant(grant) && grant.level <= level && grant.choices.empty());
            const auto group = mastery_options(klass, grant.level);
            const auto key = grant.id.substr(8);
            require(group.count && grant.source_id == group.id && kinds.insert(key).second);
            require(std::any_of(group.options.begin(), group.options.end(),
                                [&](const auto & o)
            {
                return o.id == key;
            }));
            auto &choices = result[group.id];
            choices.push_back(key);
            require(choices.size() <= group.count);
        }
    return result;
}

unsigned mastery_replacements(CharacterClass klass)
{
    return klass == CharacterClass::fighter || klass == CharacterClass::barbarian ? 1
           : klass == CharacterClass::rogue || klass == CharacterClass::paladin ||
           klass == CharacterClass::ranger
           ? 2
           : 0;
}

std::vector<rules::FeatureGrant> replace_masteries(std::span<const rules::FeatureGrant> grants,
        CharacterClass klass, unsigned level,
        std::span<const std::string> selected)
{
    require(level <= 4);
    const auto previous = mastery_choices(grants, klass, level);
    unsigned capacity = 0;
    for (const unsigned acquired :
            {
                1u, 4u
            })
        if (acquired <= level)
        {
            const auto n = count(klass, acquired);
            capacity += n;
            const auto found = previous.find(source(klass, acquired));
            require(!n || (found != previous.end() && found->second.size() == n));
        }
    require(capacity && selected.size() == capacity);
    std::set<std::string> distinct;
    for (const auto &key : selected)
    {
        const auto *item = weapon(key);
        require(item && eligible(klass, *item) && distinct.insert(key).second);
    }
    std::vector<rules::FeatureGrant> result(grants.begin(), grants.end());
    std::vector<std::size_t> vacant;
    std::set<std::string> retained;
    for (std::size_t i = 0; i < result.size(); ++i)
        if (is_mastery_grant(result[i]))
        {
            const auto key = result[i].id.substr(8);
            if (distinct.contains(key))
                retained.insert(key);
            else
                vacant.push_back(i);
        }
    require(vacant.size() <= mastery_replacements(klass));
    std::size_t slot = 0;
    for (const auto &key : selected)
        if (!retained.contains(key))
            result.at(vacant.at(slot++)).id = "mastery:" + key;
    (void)mastery_choices(result, klass, level);
    return result;
}
} // namespace opengold::srd5::detail
