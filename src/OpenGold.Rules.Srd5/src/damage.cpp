#include "damage.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace opengold::srd5::detail
{
namespace
{
constexpr std::array<std::string_view, 13> keys{
    "acid",     "bludgeoning", "cold",    "fire",    "force",    "lightning", "necrotic",
    "piercing", "poison",      "psychic", "radiant", "slashing", "thunder"};
constexpr std::array<std::string_view, 13> names{
    "Acid",     "Bludgeoning", "Cold",    "Fire",    "Force",    "Lightning", "Necrotic",
    "Piercing", "Poison",      "Psychic", "Radiant", "Slashing", "Thunder"};

std::size_t index(DamageType type)
{
    const auto i = static_cast<unsigned>(type);
    if (i >= keys.size())
        throw std::runtime_error("Invalid damage type");
    return i;
}
} // namespace

DamageType damage_type(std::string_view name)
{
    const auto found = std::find(keys.begin(), keys.end(), name);
    if (found == keys.end())
        throw std::runtime_error("Unknown damage type");
    return static_cast<DamageType>(found - keys.begin());
}

std::string_view damage_name(DamageType type)
{
    return names[index(type)];
}

DamageResult resolve_damage(std::span<const DamagePart> parts,
                            std::span<const DamageAffinity> affinities)
{
    if (parts.size() > 128 || affinities.size() > 128)
        throw std::runtime_error("Damage instance exceeds limit");
    std::array<std::int64_t, 13> amounts{};
    std::array<bool, 13> present{}, resistant{}, vulnerable{}, immune{};
    for (const auto &part : parts)
    {
        if (part.amount < 0)
            throw std::runtime_error("Negative damage amount");
        const auto i = index(part.type);
        amounts[i] += part.amount;
        present[i] = true;
    }
    for (const auto &affinity : affinities)
    {
        if (affinity.type)
            (void)index(*affinity.type);
        for (unsigned i = 0; i < keys.size(); ++i)
            if (!affinity.type || static_cast<unsigned>(*affinity.type) == i)
            {
                switch (affinity.kind)
                {
                case AffinityKind::resistance:
                    resistant[i] = true;
                    break;
                case AffinityKind::vulnerability:
                    vulnerable[i] = true;
                    break;
                case AffinityKind::immunity:
                    immune[i] = true;
                    break;
                default:
                    throw std::runtime_error("Invalid damage affinity");
                }
            }
    }
    DamageResult result;
    std::int64_t total = 0;
    for (unsigned i = 0; i < keys.size(); ++i)
        if (present[i])
        {
            auto amount = amounts[i];
            if (resistant[i])
                amount /= 2;
            if (vulnerable[i])
                amount *= 2;
            if (immune[i])
                amount = 0;
            total += amount;
            if (amounts[i] > std::numeric_limits<int>::max() ||
                total > std::numeric_limits<int>::max())
                throw std::runtime_error("Damage amount overflow");
            result.parts.push_back({static_cast<DamageType>(i), static_cast<int>(amounts[i]),
                                    static_cast<int>(amount)});
        }
    result.total = static_cast<int>(total);
    return result;
}
} // namespace opengold::srd5::detail
