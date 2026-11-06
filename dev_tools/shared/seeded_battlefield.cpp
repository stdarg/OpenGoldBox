#include "seeded_battlefield.h"
#include "opengold/rolf_tour.h"
#include <algorithm>
#include <array>
#include <numeric>
#include <random>
#include <vector>

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
// Whether a creature of `footprint` starting at `start` can walk to a place
// beside one of `party`'s squares: every square it fills open, as the game's
// movement requires. A field whose walls part the two sides gives a fight
// that never ends, with nobody able to reach or see the other side.
bool reaches_party(const opengold::rules::Battlefield &field, opengold::rules::Cell start,
                   opengold::rules::Footprint footprint,
                   const std::vector<std::optional<opengold::rules::Cell>> &party)
{
    using opengold::rules::Cell;
    const auto fits = [&](Cell anchor)
    {
        for (int dy = 0; dy < footprint.height; ++dy)
        {
            for (int dx = 0; dx < footprint.width; ++dx)
            {
                const Cell cell{anchor.x + dx, anchor.y + dy};
                if (!field.contains(cell) ||
                    field.at(cell) == opengold::rules::Terrain::obstacle)
                {
                    return false;
                }
            }
        }
        return true;
    };
    const auto beside_party = [&](Cell anchor)
    {
        return std::any_of(party.begin(), party.end(),
                           [&](const auto &member)
                           {
                               return member && member->x >= anchor.x - 1 &&
                                      member->x <= anchor.x + footprint.width &&
                                      member->y >= anchor.y - 1 &&
                                      member->y <= anchor.y + footprint.height;
                           });
    };
    std::vector<bool> seen(static_cast<std::size_t>(field.width * field.height));
    std::vector<Cell> frontier{start};
    seen[static_cast<std::size_t>(start.y * field.width + start.x)] = true;
    while (!frontier.empty())
    {
        const auto at = frontier.back();
        frontier.pop_back();
        if (beside_party(at))
        {
            return true;
        }
        for (int dy = -1; dy <= 1; ++dy)
        {
            for (int dx = -1; dx <= 1; ++dx)
            {
                const Cell next{at.x + dx, at.y + dy};
                if (!field.contains(next))
                {
                    continue;
                }
                const auto index = static_cast<std::size_t>(next.y * field.width + next.x);
                if (!seen[index] && fits(next))
                {
                    seen[index] = true;
                    frontier.push_back(next);
                }
            }
        }
    }
    return false;
}

bool sides_meet(const opengold::por::DungeonBattlefield &field,
                const opengold::por::EncounterFormation &formation,
                std::span<const opengold::rules::Footprint> enemies)
{
    for (std::size_t n = 0; n < formation.enemies.size(); ++n)
    {
        if (!reaches_party(field.geometry, *formation.enemies[n], enemies[n], formation.party))
        {
            return false;
        }
    }
    return true;
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
            if (everyone_placed(formation) && sides_meet(field, formation, enemies))
            {
                return SeededBattlefield{std::move(field), facing, std::move(formation)};
            }
        }
    }
    return std::nullopt;
}
