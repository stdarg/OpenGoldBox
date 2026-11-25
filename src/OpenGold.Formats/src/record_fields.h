#ifndef OPENGOLD_FORMATS_RECORD_FIELDS_H
#define OPENGOLD_FORMATS_RECORD_FIELDS_H
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "opengold/pool_radiance.h"

// The fields every Gold Box game's creature records keep the same way, read
// from the offsets each game's layout gives them (docs/games/*.md); shared by
// the record decoders instead of each keeping its own copy. The caller has
// already checked that the bytes are there.
namespace opengold::format
{

[[nodiscard]] constexpr int signed_byte(std::uint8_t value) noexcept
{
    return value < 128 ? value : static_cast<int>(value) - 256;
}

template <std::size_t N>
void copy_bytes(std::span<const std::uint8_t> bytes, std::size_t offset,
                std::array<std::uint8_t, N> &out)
{
    std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(offset), N, out.begin());
}

// Which of an ability's two scores: the one rolled, or the one effects and
// items make of it.
enum class AbilityScore : std::size_t
{
    permanent = 0,
    current = 1
};

// Curse of the Azure Bonds and the games after it keep each of the six
// abilities as a permanent score, then the current one, and exceptional
// Strength after them with the current score first.
[[nodiscard]] inline por::AbilityScores paired_abilities(std::span<const std::uint8_t> bytes,
                                                         std::size_t offset, AbilityScore which)
{
    const auto score = [bytes, offset, which](std::size_t ability)
    {
        return bytes[offset + 2 * ability + static_cast<std::size_t>(which)];
    };
    return {score(0),
            score(1),
            score(2),
            score(3),
            score(4),
            score(5),
            bytes[offset + 13 - static_cast<std::size_t>(which)]};
}

[[nodiscard]] inline por::SavingThrows saving_throws(std::span<const std::uint8_t> bytes,
                                                     std::size_t offset)
{
    return {bytes[offset], bytes[offset + 1], bytes[offset + 2], bytes[offset + 3],
            bytes[offset + 4]};
}

// The levels in `classes` classes, a byte each: cleric, druid, fighter,
// paladin, ranger, magic-user, thief and, where the game has monks, monk.
[[nodiscard]] inline por::ClassLevels class_levels(std::span<const std::uint8_t> bytes,
                                                   std::size_t offset, std::size_t classes = 8)
{
    return {bytes[offset],     bytes[offset + 1],
            bytes[offset + 2], bytes[offset + 3],
            bytes[offset + 4], bytes[offset + 5],
            bytes[offset + 6], classes > 7 ? bytes[offset + 7] : std::uint8_t{0}};
}

// The two attacks' dice are kept field by field: both counts, both sides, both
// modifiers.
[[nodiscard]] inline por::DamageDice attack_damage(std::span<const std::uint8_t> bytes,
                                                   std::size_t counts, std::size_t attack)
{
    return {bytes[counts + attack], bytes[counts + 2 + attack],
            signed_byte(bytes[counts + 4 + attack])};
}

// The two attacks: their rates, then their dice.
[[nodiscard]] inline std::array<por::BaseAttack, 2>
base_attacks(std::span<const std::uint8_t> bytes, std::size_t offset)
{
    std::array<por::BaseAttack, 2> attacks;
    for (std::size_t attack = 0; attack < attacks.size(); ++attack)
    {
        attacks[attack] = {bytes[offset + attack], attack_damage(bytes, offset + 2, attack)};
    }
    return attacks;
}

// The fight's current figures that end every game's record: THAC0, Armor
// Class and rear Armor Class (60 less their bytes), two attack counts, two
// attacks' dice, Hit Points and movement.
[[nodiscard]] inline por::StoredCurrentStats current_stats(std::span<const std::uint8_t> bytes,
                                                           std::size_t offset)
{
    por::StoredCurrentStats current;
    current.thac0 = 60 - bytes[offset];
    current.armor_class = 60 - bytes[offset + 1];
    current.rear_armor_class = 60 - bytes[offset + 2];
    current.attacks_raw = {bytes[offset + 3], bytes[offset + 4]};
    for (std::size_t attack = 0; attack < current.damage.size(); ++attack)
    {
        current.damage[attack] = attack_damage(bytes, offset + 5, attack);
    }
    current.hit_points_raw = bytes[offset + 11];
    current.movement = bytes[offset + 12];
    return current;
}

} // namespace opengold::format
#endif
