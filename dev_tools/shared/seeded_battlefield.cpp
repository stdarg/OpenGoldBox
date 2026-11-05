#include "seeded_battlefield.h"
#include "opengold/rolf_tour.h"
#include <algorithm>
#include <array>
#include <numeric>
#include <random>

namespace
{
bool everyone_placed(const opengold::por::EncounterFormation &formation)
{
    const auto placed = [](const auto &start)
    {
        return start.has_value();
    };
    return std::all_of(formation.party.begin(), formation.party.end(), placed) &&
           std::all_of(formation.enemies.begin(), formation.enemies.end(), placed);
}
} // namespace

std::optional<SeededBattlefield>
seeded_battlefield(const opengold::por::GeoMap &map, std::uint64_t seed, unsigned distance,
                   std::size_t party_size, std::span<const opengold::rules::Footprint> enemies)
{
    std::mt19937_64 rng(seed);
    std::array<unsigned, 256> squares{};
    std::iota(squares.begin(), squares.end(), 0u);
    std::shuffle(squares.begin(), squares.end(), rng);
    for (const unsigned square : squares)
    {
        for (unsigned turn = 0; turn < 4; ++turn)
        {
            const auto facing = static_cast<opengold::por::MapDirection>((turn + seed) % 4);
            // As in the game, the monsters stand only as far ahead as the party
            // sees before a wall or door.
            if (opengold::por::open_squares_ahead(map, {square % 16, square / 16, facing}) <
                distance)
            {
                continue;
            }
            auto field = opengold::por::dungeon_battlefield(map, square % 16, square / 16);
            auto formation =
                opengold::por::encounter_formation(field, facing, distance, party_size, enemies);
            if (everyone_placed(formation))
            {
                return SeededBattlefield{std::move(field), facing, std::move(formation)};
            }
        }
    }
    return std::nullopt;
}
