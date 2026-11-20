#ifndef OPENGOLD_BUCK_ROGERS_RECORDS_H
#define OPENGOLD_BUCK_ROGERS_RECORDS_H

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

// The records of the two Buck Rogers games, Countdown to Doomsday and Matrix
// Cubed (docs/games/countdown.md, "Records"). Both games share every layout.
namespace opengold::buck_rogers
{

// A character or creature: the 259-byte record a MON0CHA.DAX entry and a
// saved character hold. Countdown's creature entries are 263 bytes, of which
// the game reads the first 259.
struct Creature
{
    static constexpr std::size_t size = 259;

    std::string name;
    // Strength, Dexterity, Constitution, Intelligence, Wisdom, Charisma, Tech.
    std::array<std::uint8_t, 7> abilities{}, current_abilities{};
    std::array<std::uint8_t, 8> saves{};
    std::uint8_t gender{}, race{}, career{};
    int level{};
    std::uint32_t credits{}, experience{};
    std::uint8_t base_combat{}, base_movement{};
    int base_armor_class{};
    std::uint16_t hit_point_maximum{}, hit_points{};
    // Skill levels by skill number, 1 to 84 at [0] to [83].
    std::array<std::uint8_t, 84> skills{};
    std::uint16_t experience_award{};
    std::uint8_t status{}, npc{};
    std::vector<std::uint8_t> raw;
};

// One record of an item list (MON0ITM.DAX and the saved .stf files): 62
// bytes, of which the last 16 are the item's template. The first 46 hold the
// name and list link the game rebuilds as it loads the item, so the files'
// copies are leftovers and are not read.
struct Item
{
    static constexpr std::size_t size = 62;
    // types.pas C_Item.EMPTY_TEMPLATE: a template slot with no item.
    static constexpr std::uint8_t no_type = 255;

    std::uint8_t type{};
    std::array<std::uint8_t, 3> name_parts{};
    int plus{}, save_bonus{};
    std::uint8_t flags{};
    std::uint16_t weight{};
    std::uint8_t quantity{};
    std::uint16_t value{};
    std::array<std::uint8_t, 3> special{};
};

// ITEM0.DAX record 1: the 16-byte item templates scripts, shops and treasure
// copy, numbered from 1. Every item's type is a row of ITEMS (below 129) or no_type.
[[nodiscard]] std::optional<std::vector<Item>>
decode_item_templates(std::span<const std::uint8_t> bytes);

// What every item of one type shares: a 9-byte row of ITEMS, numbered from 0.
struct ItemType
{
    static constexpr std::size_t size = 9;

    std::uint8_t slot{}, attacks_doubled{}, weapon_class{}, heavy{}, dice_count{}, dice_sides{};
    int damage_bonus{};
    std::uint8_t range{}, consumable{};
};

[[nodiscard]] std::optional<std::vector<ItemType>>
decode_item_types(std::span<const std::uint8_t> bytes);

// A rocketship (SHIPS.DAX records 40 to 44 and 50): 108 bytes.
struct Ship
{
    static constexpr std::size_t size = 108;
    // Hull, sensors, controls, life support, fuel and engines.
    static constexpr std::size_t systems = 6;

    struct WeaponMount
    {
        std::uint8_t kind{}, maximum{}, status{}, ammunition{}, reloads{};
    };

    std::string name;
    int tonnage{}, length{}, width{};
    std::array<int, systems> system_status{}, system_maximum{};
    int cargo{}, crew{}, armor_class{};
    std::uint8_t pilot_skill{}, system_skill{}, gunnery{};
    int speed{}, reaction{};
    std::uint8_t maneuver{}, type{}, race{};
    std::array<WeaponMount, 8> weapons{};
};

[[nodiscard]] std::optional<Creature> decode_creature(std::span<const std::uint8_t> bytes);
[[nodiscard]] std::optional<std::vector<Item>> decode_items(std::span<const std::uint8_t> bytes);
[[nodiscard]] std::optional<Ship> decode_ship(std::span<const std::uint8_t> bytes);

} // namespace opengold::buck_rogers

#endif
