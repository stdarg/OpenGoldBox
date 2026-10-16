// Where a scripted fight's two sides start, as the original engine placed them
// (PLACE-1 in docs/SRD-DECISIONS.md). Expected squares are worked out by hand
// from the rules in src/OpenGold.Core/src/encounter_formation.cpp.
#include "opengold/encounter_formation.h"
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace opengold::por;
using opengold::rules::Cell;
using opengold::rules::Terrain;

namespace
{
void check(bool value, const char *message)
{
    if (!value)
    {
        throw std::runtime_error(message);
    }
}

// An all-floor arena whose exploration squares have no walls.
DungeonBattlefield open_field()
{
    return {{50, 25, std::vector<Terrain>(50 * 25)}, std::vector<std::uint8_t>(50 * 25, 22)};
}

std::vector<std::optional<Cell>> cells(std::initializer_list<Cell> list)
{
    return {list.begin(), list.end()};
}

// A solid wall on one side of the party's exploration square.
void wall(DungeonBattlefield &field, MapDirection side)
{
    field.walled[2][6][index(side)] = true;
}

// The enemies' half of the party's square, at distance 0 facing north: the
// arena rows 10 to 12, columns 22 to 30 (and further west when `wide`).
void fill_enemy_half(DungeonBattlefield &field, bool wide)
{
    for (int y = 10; y <= 12; ++y)
    {
        for (int x = wide ? 16 : 22; x <= 30; ++x)
        {
            field.geometry.terrain[y * 50 + x] = Terrain::obstacle;
        }
    }
}

void groups_face_each_other()
{
    // Facing north, the party stands in the south half of its square and the
    // monsters in the north half, front ranks touching.
    const auto close = encounter_formation(open_field(), MapDirection::north, 0, 4, 3);
    check(close.party == cells({{27, 13}, {28, 13}, {28, 14}, {29, 14}}),
          "The party's front rank holds half the party, the rest stand behind it");
    check(close.enemies == cells({{27, 12}, {26, 12}, {26, 11}}),
          "At distance 0 the monsters stand directly in front of the party");

    // Each exploration square north is five arena squares up and five left.
    const auto far = encounter_formation(open_field(), MapDirection::north, 2, 4, 3);
    check(far.party == close.party, "The distance moves only the monsters");
    check(far.enemies == cells({{17, 2}, {16, 2}, {16, 1}}),
          "At distance 2 the monsters stand two exploration squares further ahead");

    // Facing east, the monsters one square east face west; a single rank of
    // one, then one behind it.
    const auto east = encounter_formation(open_field(), MapDirection::east, 1, 1, 2);
    check(east.party == cells({{26, 12}}) && east.enemies == cells({{34, 13}, {35, 13}}),
          "Facing east, the monsters stand one exploration square to the east");
}

void party_facing_east_leaves_a_gap()
{
    // The original leaves one rank empty behind a party facing east or west
    // when it has somewhere to spill.
    const auto formation = encounter_formation(open_field(), MapDirection::east, 0, 4, 0);
    check(formation.party == cells({{26, 12}, {27, 13}, {24, 12}, {25, 13}}),
          "A party facing east leaves a gap behind its front rank");
}

void crowded_band_spills_behind_then_beside()
{
    // With their own half-square full of rubble, the monsters spill into the
    // square behind them, from its edge nearest the party.
    auto blocked = open_field();
    fill_enemy_half(blocked, true);
    check(encounter_formation(blocked, MapDirection::north, 0, 1, 1).enemies == cells({{23, 9}}),
          "A band that does not fit stands in the square behind it");

    // A wall behind them sends them into the square to the west first...
    auto walled_behind = open_field();
    fill_enemy_half(walled_behind, false);
    wall(walled_behind, MapDirection::north);
    check(encounter_formation(walled_behind, MapDirection::north, 0, 1, 1).enemies ==
              cells({{21, 10}}),
          "With a wall behind it, the band spills into the square to the west");

    // ...then to the east.
    wall(walled_behind, MapDirection::west);
    check(encounter_formation(walled_behind, MapDirection::north, 0, 1, 1).enemies ==
              cells({{31, 12}}),
          "With walls behind and to the west, the band spills to the east");

    // Walled in on three sides, the original finds no room for it.
    wall(walled_behind, MapDirection::east);
    const auto none = encounter_formation(walled_behind, MapDirection::north, 0, 1, 1);
    check(none.enemies.size() == 1 && !none.enemies[0], "A band with no room is left unplaced");
}

void full_squares_and_walls()
{
    // A whole half-square holds 23 monsters; the 24th spills behind.
    const auto band = encounter_formation(open_field(), MapDirection::north, 0, 1, 25);
    for (std::size_t n = 0; n < 23; ++n)
    {
        check(band.enemies[n] && band.enemies[n]->y >= 10 && band.enemies[n]->y <= 12 &&
                  band.enemies[n]->x >= 22 && band.enemies[n]->x <= 30,
              "The first 23 monsters fill their own half-square");
    }
    check(band.enemies[23] == Cell{21, 10} && band.enemies[24] == Cell{23, 9},
          "Further monsters stand in the square behind");

    // A blocked square is skipped but still counts towards the front rank's
    // length, so the third monster starts the second rank.
    auto rubble = open_field();
    rubble.geometry.terrain[12 * 50 + 27] = Terrain::obstacle;
    check(encounter_formation(rubble, MapDirection::north, 0, 1, 3).enemies ==
              cells({{26, 12}, {26, 11}, {25, 11}}),
          "An obstacle in the front rank is skipped");
}
} // namespace

int main()
{
    try
    {
        groups_face_each_other();
        party_facing_east_leaves_a_gap();
        crowded_band_spills_behind_then_beside();
        full_squares_and_walls();
        std::cout << "Encounter formation: original placement checks passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
