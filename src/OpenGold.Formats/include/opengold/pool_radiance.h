#ifndef OPENGOLD_POOL_RADIANCE_H
#define OPENGOLD_POOL_RADIANCE_H

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace opengold::por
{

struct AbilityScores
{
    std::uint8_t strength{}, intelligence{}, wisdom{}, dexterity{}, constitution{}, charisma{};
    // 1..100 means 18/01..18/00; zero means ordinary strength 18.
    std::uint8_t exceptional_strength{};
};

struct DamageDice
{
    std::uint8_t count{}, sides{};
    int modifier{}; // Signed byte, independent of equipment/ability adjustments.
};

struct BaseAttack
{
    std::uint8_t attacks_per_two_rounds{};
    DamageDice damage;
    [[nodiscard]] double attacks_per_round() const noexcept
    {
        return attacks_per_two_rounds / 2.0;
    }
};

struct SavingThrows
{
    std::uint8_t paralysis_poison_death{}, petrification_polymorph{}, rods_staves_wands{}, breath{},
        spells{};
};

struct ClassLevels
{
    std::uint8_t cleric{}, druid{}, fighter{}, paladin{}, ranger{}, mage{}, thief{}, monk{};
};

// These fields can be zero/uninitialized in MON templates. They are NOT calculated totals.
struct StoredCurrentStats
{
    int thac0{}, armor_class{}, rear_armor_class{};
    std::array<std::uint8_t, 2> attacks_raw{}; // May be attacks remaining, not a rate.
    std::array<DamageDice, 2> damage;
    std::uint8_t hit_points_raw{}, movement{};
};

struct CharacterRecord
{
    std::array<std::uint8_t, 285> raw{};
    std::string name;
    AbilityScores abilities;
    int base_thac0{}, base_armor_class{}; // Descending AC; 60 minus encoded byte.
    std::uint8_t max_hit_points{}, rolled_hit_points{}, base_movement{};
    std::array<BaseAttack, 2> base_attacks;
    SavingThrows saves;
    ClassLevels levels;
    std::uint8_t race{}, character_class{}, gender{}, alignment{}, type{}, npc_flag{};
    std::uint16_t age{};
    std::uint8_t attack_level{}, highest_level_raw{}, drained_levels{}, drained_hit_points{},
        undead_level{};
    std::array<std::uint8_t, 8> thief_skills{};
    // Byte values are retained: known-spell flags/counters and memorized spell IDs.
    std::array<std::uint8_t, 55> spell_book{};
    std::array<std::uint8_t, 21> memorized_spells{};
    std::array<std::uint8_t, 3> cleric_spell_slots{}, mage_spell_slots{};
    std::uint8_t spell_byte_44_raw{};
    std::uint32_t experience{};
    std::uint16_t base_xp_award{};
    std::uint8_t xp_per_hit_point{};
    std::array<std::uint16_t, 7>
        wealth{}; // Copper, silver, electrum, gold, platinum, gems, jewelry.
    std::uint8_t strength_bonus_flag_raw{}, item_count{}, item_limit{}, hands_equipped{};
    int save_bonus{};
    std::uint16_t encumbrance{};
    std::uint8_t status{}, enabled{}, hostile{}, quick_fight{};
    StoredCurrentStats current;
};

struct ItemRecord
{
    std::array<std::uint8_t, 63> raw{};
    std::string stored_name; // Often empty; never reinterpret the pointer bytes as text.
    std::uint8_t type{};
    std::array<std::uint8_t, 3> name_components{};
    int magic_bonus{}, save_bonus{};
    std::uint8_t readied_raw{}, revealed_components{}, cursed_raw{};
    std::uint16_t weight{}, value{};
    std::uint8_t stack_size{};
    std::array<std::uint8_t, 3> effect_codes{};
    [[nodiscard]] bool readied() const noexcept
    {
        return readied_raw != 0;
    }
};

struct ItemTemplate
{
    std::array<std::uint8_t, 16> raw{};
    std::uint8_t worn_location{}, hands{}, rate_of_fire{}, protection_raw{}, damage_type{},
        melee_flag{};
    DamageDice large_damage, small_medium_damage;
    std::uint8_t range{}, class_restrictions{}, ammunition_type{};
};

struct EffectRecord
{
    std::array<std::uint8_t, 9> raw{};
    std::uint8_t code{};
    std::uint16_t duration_raw{}; // Zero = permanent. Unit of nonzero durations is not assumed.
    std::uint8_t data{}, table_flag{};
    std::uint32_t next_pointer_raw{}; // DOS address for provenance only; never followed.
};

// nullopt means invalid length/content; an empty vector means a valid empty record.
[[nodiscard]] std::optional<CharacterRecord> decode_character(std::span<const std::uint8_t> bytes);
[[nodiscard]] std::optional<std::vector<ItemRecord>>
decode_items(std::span<const std::uint8_t> bytes);
[[nodiscard]] std::optional<std::vector<EffectRecord>>
decode_effects(std::span<const std::uint8_t> bytes);
// DOS ITEMS: two uninterpreted header bytes followed by 16-byte templates, indexed by item type.
[[nodiscard]] std::optional<std::vector<ItemTemplate>>
decode_item_templates(std::span<const std::uint8_t> bytes);

} // namespace opengold::por
#endif
