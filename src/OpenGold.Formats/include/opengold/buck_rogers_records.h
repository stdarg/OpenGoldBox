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
// saved character (.who) hold (types.pas TCharacter). Countdown's creature
// entries are 263 bytes, of which the game reads the first 259. The fields
// below are the ones the rules read and set; raw keeps every byte as read,
// so encode_creature gives back the record it came from, the bytes no field
// names (pointers and combat leftovers) included.
struct Creature
{
    static constexpr std::size_t size = 259;
    static constexpr std::size_t name_capacity = 15;

    std::string name;
    // Strength, Dexterity, Constitution, Intelligence, Wisdom, Charisma, Tech.
    std::array<std::uint8_t, 7> abilities{}, current_abilities{};
    // The roll each saving throw needs, by category 0 to 7.
    std::array<std::uint8_t, 8> saves{};
    std::uint8_t gender{}, race{}, career{};
    int level{};
    std::uint32_t credits{}, experience{};
    std::uint8_t base_combat{}, icon_dimensions{}, base_movement{};
    int base_armor_class{};
    std::uint8_t age{};
    // Attacks a round, doubled: unarmed (the first attack slot) and armed.
    std::uint8_t unarmed_attacks_doubled{}, attacks_doubled{};
    // Each attack slot's unarmed damage: dice, sides and bonus.
    std::array<std::uint8_t, 2> unarmed_dice_count{}, unarmed_dice_sides{};
    std::array<int, 2> unarmed_damage_bonus{};
    std::uint16_t hit_point_maximum{}, hit_points{};
    std::uint8_t armor_flags{}, companion{};
    // Skill levels by skill number, 1 to 84 at [0] to [83].
    std::array<std::uint8_t, 84> skills{};
    std::uint16_t experience_award{};
    std::uint8_t icon{}, order_number{};
    // The item types a Warrior has specialized in, one a slot, 0 for none.
    std::array<std::uint8_t, 10> specializations{};
    std::array<std::uint8_t, 16> icon_sequence{};
    std::uint8_t status{}, enabled{};
    int side{};
    std::uint8_t quickfight{}, attack_bonus{};
    // From the front and from behind.
    std::array<std::uint8_t, 2> armor_class{};
    std::uint8_t current_movement{}, npc{};
    std::uint16_t encumbrance{};
    std::uint8_t pilot{};
    std::vector<std::uint8_t> raw;
};

// One record of an item list (MON0ITM.DAX and the saved .stf and .hid files):
// 62 bytes, of which the last 16 are the item's template. The first 46 hold
// the name and list link the game rebuilds as it loads the item, so the
// files' copies are leftovers: the rules never read them, and list_bytes
// keeps them only so encode_items writes the record back as it was.
struct Item
{
    static constexpr std::size_t size = 62;
    static constexpr std::size_t list_size = 46;
    // types.pas C_Item.EMPTY_TEMPLATE: a template slot with no item.
    static constexpr std::uint8_t no_type = 255;
    // The flag bit of a readied item.
    static constexpr std::uint8_t readied = 0x80;

    std::uint8_t type{};
    std::array<std::uint8_t, 3> name_parts{};
    int plus{}, save_bonus{};
    std::uint8_t flags{}, unidentified{};
    std::uint16_t weight{};
    std::uint8_t quantity{};
    std::uint16_t value{};
    std::array<std::uint8_t, 3> special{};
    std::array<std::uint8_t, list_size> list_bytes{};
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
        // The weapon kind in the low seven bits (0 for an empty mount); the
        // top bit marks a mount the crew fires, not the pilot.
        std::uint8_t kind{}, maximum{}, status{}, ammunition{}, reloads{};
    };

    std::string name;
    int tonnage{}, length{}, width{};
    std::array<int, systems> system_status{}, system_maximum{};
    int cargo{}, crew{}, armor_class{};
    std::uint8_t pilot_skill{}, system_skill{}, gunnery{};
    int speed{}, reaction{};
    // Added to the total of a gunner aiming at this ship: -2 for the small
    // Hubble, +2 for the 500-ton Mohave.
    int maneuver{};
    std::uint8_t type{}, race{};
    std::array<WeaponMount, 8> weapons{};
    // The record as read, so a ship written back keeps the bytes no field
    // holds.
    std::vector<std::uint8_t> raw;
};

[[nodiscard]] std::optional<Creature> decode_creature(std::span<const std::uint8_t> bytes);
[[nodiscard]] std::optional<std::vector<Item>> decode_items(std::span<const std::uint8_t> bytes);
[[nodiscard]] std::optional<Ship> decode_ship(std::span<const std::uint8_t> bytes);
// The 108 bytes of a ship, as the party's ship is kept at 4D00h: its raw
// record with every field written over it. nullopt for a name that does not
// fit in 15 characters or a value its byte or word cannot hold.
[[nodiscard]] std::optional<std::vector<std::uint8_t>> encode_ship(const Ship &ship);

// The 259-byte record of a creature, its raw bytes (zeros for a new one) with
// every field written over them. Throws std::invalid_argument for a name
// longer than 15 letters.
[[nodiscard]] std::vector<std::uint8_t> encode_creature(const Creature &creature);
// An item list's records, 62 bytes an item.
[[nodiscard]] std::vector<std::uint8_t> encode_items(std::span<const Item> items);

} // namespace opengold::buck_rogers

#endif
