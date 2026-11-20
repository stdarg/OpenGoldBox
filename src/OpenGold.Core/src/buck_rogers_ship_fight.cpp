#include "opengold/buck_rogers_ship_fight.h"

#include "opengold/dice.h"
#include "opengold/vga_picture.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace opengold::por
{
namespace
{
using buck_rogers::Ship;

constexpr unsigned first_enemy_record = 40;
constexpr unsigned first_party_icon = 27, first_enemy_icon = 33;

// The names the enemy ships take, five a type (gdconsts.pas shipNames).
constexpr std::array<std::array<const char *, 5>, enemy_ship_types> ship_names{{
    {"Van Allen", "Hubble", "Copernicus", "Kepler", "Goddard"},
    {"Adder", "Viper", "Anaconda", "Sidewinder", "Boomslang"},
    {"Mohave", "Sahara", "Gobi", "Outback", "Kalihari"},
    {"Flare", "Corona", "Hermes", "Osiris", "Apollo"},
    {"Liberator", "Eclipse", "Daisy", "Gambit", "Dervish"},
}};

[[nodiscard]] const std::vector<std::uint8_t> &record(const DaxDecodeResult &ships, unsigned id)
{
    const auto found = std::ranges::find(ships.records, id, &DaxRecord::id);
    if (found == ships.records.end())
    {
        throw std::runtime_error("SHIPS.DAX lacks record " + std::to_string(id));
    }
    return found->bytes;
}

// A picture in the colors it sets; black is see-through, so the screen can
// draw the ship over a scene of its own size.
[[nodiscard]] Image picture(const DaxDecodeResult &ships, unsigned id, bool black_clear)
{
    const auto decoded = decode_vga_picture(record(ships, id));
    if (!decoded || decoded->frames.empty())
    {
        throw std::runtime_error("SHIPS.DAX picture " + std::to_string(id) + " does not decode");
    }
    return vga_rgba(decoded->frames.front(), vga_color_table(decoded->palette),
                    black_clear ? std::optional<std::uint8_t>{0} : std::nullopt);
}

[[nodiscard]] bool alive(const original::TeamMember &member) noexcept
{
    return member.character.enabled != 0;
}
} // namespace

ShipFightFiles read_ship_fight_files(const DaxDecodeResult &ships)
{
    ShipFightFiles files;
    for (unsigned type = 1; type <= enemy_ship_types; ++type)
    {
        auto ship = buck_rogers::decode_ship(record(ships, first_enemy_record + type - 1));
        if (!ship)
        {
            throw std::runtime_error("SHIPS.DAX ship " + std::to_string(type) + " does not decode");
        }
        files.enemy_ships[type - 1] = std::move(*ship);
        files.enemy_pictures[type - 1] = picture(ships, type - 1, true);
    }
    for (unsigned system = 0; system < Ship::systems; ++system)
    {
        files.party_system_icons[system] = picture(ships, first_party_icon + system, false);
        files.enemy_system_icons[system] = picture(ships, first_enemy_icon + system, false);
    }
    return files;
}

std::string enemy_ship_name(unsigned type, unsigned count)
{
    if (type < 1 || type > enemy_ship_types)
    {
        throw std::invalid_argument("No enemy ship of that type");
    }
    std::string name = ship_names[type - 1][count % 5];
    if (count > 1)
    {
        name += ' ' + std::to_string(count);
    }
    return name;
}

std::uint16_t enemy_ship_count_cell(unsigned type)
{
    if (type < 1 || type > enemy_ship_types)
    {
        throw std::invalid_argument("No enemy ship of that type");
    }
    return static_cast<std::uint16_t>(0x4D91 + type - 1);
}

original::ShipCombatStart ship_combat_start(GoldBoxGame game, Ship party_ship,
                                            const ShipFightFiles &files, const ShipCombatCall &call,
                                            std::string enemy_name,
                                            std::vector<original::TeamMember> crew,
                                            original::ShipCombatOptions options)
{
    if (call.enemy_type < 1 || call.enemy_type > enemy_ship_types)
    {
        throw std::invalid_argument("SHIP COMBAT names no enemy ship of type " +
                                    std::to_string(call.enemy_type));
    }
    auto enemy = files.enemy_ships[call.enemy_type - 1];
    enemy.name = std::move(enemy_name);
    return {.game = game,
            .party_ship = std::move(party_ship),
            .enemy_ship = std::move(enemy),
            .enemy_ship_number = static_cast<int>(call.enemy_type),
            .enemy_pilot_skill = static_cast<int>(call.pilot_skill),
            .enemy_to_hit = static_cast<int>(call.to_hit),
            .crew = std::move(crew),
            .options = options};
}

ShipFightAftermath follow_ship_fight(GoldBoxGame game, original::ShipCombatEnd end,
                                     const original::ShipCombatOptions &options,
                                     RandomState &random)
{
    ShipFightAftermath aftermath{.end = std::move(end)};
    auto &crew = aftermath.end.crew;
    // No one left alive: the team is destroyed (DetermineCombatResults).
    if (std::ranges::none_of(crew, alive))
    {
        aftermath.team_destroyed = true;
        return aftermath;
    }
    original::recover_after_fight(crew, false);
    if (aftermath.end.treatment_pending)
    {
        // The antidote is used whenever one is carried, as after a ground
        // fight (buck_rogers_host.cpp).
        aftermath.treatment = original::treat_injuries(
            crew,
            [](std::size_t, std::size_t)
            {
                return true;
            },
            random);
    }
    if (aftermath.end.repair_pending)
    {
        // The enemy's fuel is taken from a copy: the script's salvage reads
        // the enemy as the fight left it.
        auto enemy = aftermath.end.enemy_ship;
        aftermath.repairs = original::repair_ship(game, aftermath.end.party_ship, crew,
                                                  aftermath.end.experience, &enemy, false, options,
                                                  [&random](int sides)
                                                  {
                                                      return roll_die(random, sides);
                                                  });
        for (auto &member : crew)
        {
            if (alive(member))
            {
                member.character.experience +=
                    static_cast<std::uint32_t>(std::max(aftermath.repairs->experience_share, 0));
            }
        }
    }
    return aftermath;
}

std::vector<ShipMemoryCell> ship_memory_cells(std::span<const original::ShipMemoryWrite> writes)
{
    // The team's ship from 4D00h keeps its numbers of two bytes (16 to 48) as
    // words, each high byte also in the cell after it (buck_rogers_host.cpp
    // configure_buck_rogers); the saved enemy systems, from 4D6Ch, are words
    // the same way, as the scripts read them.
    constexpr std::uint16_t ship = 0x4D00;
    constexpr std::uint16_t first_system_word = 0x4D6C, last_system_word = 0x4D76;
    const auto starts_word = [](std::uint16_t address)
    {
        const bool ship_word = address >= ship + 16 && address <= ship + 48;
        const bool system_word = address >= first_system_word && address <= last_system_word;
        return (ship_word || system_word) && address % 2 == 0;
    };
    std::vector<ShipMemoryCell> cells;
    for (const auto &write : writes)
    {
        std::uint16_t value = write.value;
        if (starts_word(write.address))
        {
            const auto high =
                std::ranges::find(writes, static_cast<std::uint16_t>(write.address + 1),
                                  &original::ShipMemoryWrite::address);
            value =
                static_cast<std::uint16_t>(value | (high == writes.end() ? 0 : high->value << 8));
        }
        cells.push_back({write.address, value});
    }
    return cells;
}

} // namespace opengold::por
