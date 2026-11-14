#ifndef OPENGOLD_POOL_RADIANCE_H
#define OPENGOLD_POOL_RADIANCE_H

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "opengold/gold_box_game.h"

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

// A creature or character record of any Gold Box game, decoded into the same
// fields: Pool of Radiance's 285 bytes (decode_character) or Curse of the Azure
// Bonds' 422 (curse::decode_creature, docs/games/curse.md).
struct CharacterRecord
{
    GoldBoxGame game{GoldBoxGame::pool_of_radiance};
    std::vector<std::uint8_t> raw; // The record's bytes in its game's layout.
    std::string name;
    AbilityScores abilities;
    // The scores effects and items make of them; Pool of Radiance keeps only one.
    AbilityScores current_abilities;
    int base_thac0{}, base_armor_class{}; // Descending AC; 60 minus encoded byte.
    std::uint8_t max_hit_points{}, rolled_hit_points{}, base_movement{};
    std::array<BaseAttack, 2> base_attacks;
    SavingThrows saves;
    ClassLevels levels;
    // A dual-class character's levels before the change; zero in Pool of Radiance.
    ClassLevels levels_before_class_change;
    // The NPC byte; for a monster, from 0x80 up, its morale (creature_art.cpp).
    std::uint8_t npc_flag{};
    // The race by Pool of Radiance's numbers (0 a monster, 1 to 7 dwarf, elf,
    // gnome, half-elf, halfling, half-orc and human), Secret's translated.
    std::uint8_t race{}, character_class{}, gender{}, alignment{}, type{};
    // The combat icon's size: 2 one by two squares, 3 two by one, 4 two by two.
    std::uint8_t icon_size_raw{};
    std::uint16_t age{};
    std::uint8_t attack_level{}, highest_level_raw{}, drained_levels{}, drained_hit_points{},
        undead_level{};
    std::array<std::uint8_t, 8> thief_skills{};
    // Secret of the Silver Blades judges a creature by flag bits where the
    // earlier games read its type (docs/games/secret.md): what kind of
    // creature it is to effects and spells (secret::creature_type bits), what
    // its foes' weapons and spells may do to it (secret::target bits), and its
    // magic resistance in percent. Zero in the other games.
    std::uint8_t creature_type_flags{}, target_flags{}, magic_resistance{};
    // Byte values are retained: known-spell flags/counters and memorized spell IDs.
    std::vector<std::uint8_t> spell_book, memorized_spells;
    // Spell slots by list (cleric, druid, magic-user) and spell level from 1.
    std::array<std::array<std::uint8_t, 5>, 3> spell_slots{};
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
    std::uint8_t readied_raw{}, cursed_raw{};
    // Name words still hidden until the item is identified: bit 4 hides the
    // byte-47 word, bit 2 byte 48's and bit 1 byte 49's.
    std::uint8_t hidden_name_words{};
    std::uint16_t weight{}, value{};
    std::uint8_t stack_size{};
    std::array<std::uint8_t, 3> effect_codes{};

    [[nodiscard]] bool readied() const noexcept
    {
        return readied_raw != 0;
    }
};

// The property bits of an item type (ItemTemplate::properties).
namespace item_property
{
inline constexpr std::uint8_t fires_arrows = 0x01;
// Dexterity's missile bonus adds to its to-hit roll.
inline constexpr std::uint8_t dexterity_missile = 0x02;
// Strength's bonuses add to its to-hit roll and damage.
inline constexpr std::uint8_t strength_bonus = 0x04;
inline constexpr std::uint8_t fires_ammunition = 0x08;
inline constexpr std::uint8_t thrown = 0x10;
inline constexpr std::uint8_t fires_quarrels = 0x80;
} // namespace item_property

struct ItemTemplate
{
    std::array<std::uint8_t, 16> raw{};
    std::uint8_t worn_location{}, hands{}, rate_of_fire{}, protection_raw{}, damage_type{},
        melee_flag{};
    DamageDice large_damage, small_medium_damage;
    std::uint8_t range{}, class_restrictions{};
    // The weapon's property bits (ItemProperty), byte 14 of the original's item types.
    std::uint8_t properties{};

    [[nodiscard]] bool has(std::uint8_t property) const noexcept
    {
        return (properties & property) != 0;
    }
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
// A creature record in the game's own layout; nullopt for a game whose records
// are not decoded yet.
[[nodiscard]] std::optional<CharacterRecord> decode_character(std::span<const std::uint8_t> bytes,
                                                              GoldBoxGame game);
[[nodiscard]] std::optional<std::vector<ItemRecord>>
decode_items(std::span<const std::uint8_t> bytes);
// The games after Pool of Radiance build an item's name from its name words
// each time it is shown and leave scratch in the record's name bytes, so their
// lists are read with no stored names.
[[nodiscard]] std::optional<std::vector<ItemRecord>>
decode_items(std::span<const std::uint8_t> bytes, GoldBoxGame game);
[[nodiscard]] std::optional<std::vector<EffectRecord>>
decode_effects(std::span<const std::uint8_t> bytes);
// An item as rules that keep the original's items key it (the original
// rules): its record whole, so they read its type, magic and powers as the
// original does ("original:" and the record's bytes in hex), less whether it
// is readied and its count, which the inventory keeps. The campaign may add
// ":unidentified".
[[nodiscard]] std::string original_item_key(const ItemRecord &item);
// The record such a key holds, or nothing for any other key.
[[nodiscard]] std::optional<ItemRecord> item_of_original_key(std::string_view key);
// DOS ITEMS: two uninterpreted header bytes followed by 16-byte templates, indexed by item type.
[[nodiscard]] std::optional<std::vector<ItemTemplate>>
decode_item_templates(std::span<const std::uint8_t> bytes);
// The game's own ITEMS: Pools of Darkness' and Treasures' (and Unlimited
// Adventures' ITEMS.DAT) have no header bytes;
// Gateway to the Savage Frontier's ITEMS.1 holds 24 bytes a type, the 16 above
// and 8 unused.
[[nodiscard]] std::optional<std::vector<ItemTemplate>>
decode_item_templates(std::span<const std::uint8_t> bytes, GoldBoxGame game);
// START.EXE's name tables, read from its unpacked image (unpack_sound_executable). Item words
// are indexed by name component: 0 is empty, 1 is "Battle Axe", 255 the last. Spells are
// indexed by spell number: 0 is empty, 1 is "Bless".
[[nodiscard]] std::optional<std::vector<std::string>>
decode_item_words(std::span<const std::uint8_t> image);
// The game's own table: Secret of the Silver Blades has 123 words.
[[nodiscard]] std::optional<std::vector<std::string>>
decode_item_words(std::span<const std::uint8_t> image, GoldBoxGame game);
[[nodiscard]] std::optional<std::vector<std::string>>
decode_spell_names(std::span<const std::uint8_t> image);
// The name shown: the components' words, last component first ("Ring Of Protection +1"),
// leaving out the ones `hidden` masks as the record's hidden_name_words does ("Ring").
[[nodiscard]] std::string item_name(const ItemRecord &item, std::span<const std::string> words,
                                    std::uint8_t hidden = 0);

} // namespace opengold::por
#endif
