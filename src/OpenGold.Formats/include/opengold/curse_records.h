#ifndef OPENGOLD_CURSE_RECORDS_H
#define OPENGOLD_CURSE_RECORDS_H

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "opengold/pool_radiance.h"

// Curse of the Azure Bonds' creature records (MONnCHA.DAX), the 422-byte layout
// that replaced Pool of Radiance's 285 bytes. Items (63 bytes) and effects
// (9 bytes) keep Pool of Radiance's layout. The fields: docs/games/curse.md.
namespace opengold::curse
{

// An ability score as the game keeps it: the score the character owns and the
// one effects and items make of it.
struct AbilityScore
{
    std::uint8_t permanent{}, current{};
};

struct CreatureRecord
{
    std::vector<std::uint8_t> raw;
    std::string name;
    // Strength, Intelligence, Wisdom, Dexterity, Constitution, Charisma.
    std::array<AbilityScore, 6> abilities{};
    // 1..100 means 18/01..18/00; zero means no exceptional Strength.
    std::uint8_t exceptional_strength{}, exceptional_strength_current{};
    // Per slot a spell number, with bit 7 set while it is still being memorized.
    std::array<std::uint8_t, 84> memorized_spells{};
    std::array<std::uint8_t, 100> known_spells{};
    int base_thac0{}, base_armor_class{}; // Descending; 60 minus the stored byte.
    std::uint8_t race{}, character_class{};
    std::uint16_t age{};
    std::uint8_t max_hit_points{}, rolled_hit_points{};
    std::uint8_t attack_level{};
    por::SavingThrows saves;
    std::uint8_t base_movement{};
    std::uint8_t highest_level{}, level_at_class_change{}, drained_levels{}, drained_hit_points{},
        undead_level{};
    std::array<std::uint8_t, 8> thief_skills{};
    std::uint8_t npc_flag{};
    // Copper, silver, electrum, gold, platinum, gems, jewelry.
    std::array<std::uint16_t, 7> wealth{};
    por::ClassLevels levels, levels_before_class_change;
    std::uint8_t gender{}, type{}, alignment{};
    std::array<por::BaseAttack, 2> base_attacks;
    std::uint32_t experience{};
    // Spell slots by list (cleric, druid, magic-user) and spell level 1 to 5.
    std::array<std::array<std::uint8_t, 5>, 3> spell_slots{};
    std::uint16_t base_xp_award{};
    std::uint8_t xp_per_hit_point{};
    std::uint8_t portrait_head{}, portrait_body{}, icon_head{}, icon_body{}, icon_size{};
    std::array<std::uint8_t, 6> icon_colors{};
    std::uint8_t item_count{}, hands_equipped{};
    int save_bonus{};
    std::uint16_t encumbrance{};
    std::uint8_t status{}, enabled{}, hostile{}, quick_fight{};
    por::StoredCurrentStats current;
};

// nullopt for a record that is not 422 bytes or whose name is too long.
[[nodiscard]] std::optional<CreatureRecord> decode_creature(std::span<const std::uint8_t> bytes);

} // namespace opengold::curse

#endif
