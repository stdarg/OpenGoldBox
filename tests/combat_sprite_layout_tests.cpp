#include "../src/OpenGoldBox/combat_sprite_layout.h"
#include <iostream>
#include <stdexcept>

using namespace opengold::rules;

namespace
{
void check(bool ok, const char *message)
{
    if (!ok)
    {
        throw std::runtime_error(message);
    }
}

void ordering()
{
    const auto actor = [](EntityId id, opengold::rules::Side side, Cell cell)
    {
        CombatantView value;
        value.id = id;
        value.side = side;
        value.cell = cell;
        return value;
    };
    const auto party = opengold::rules::Side::party, opposition = opengold::rules::Side::opposition;
    std::vector<CombatantView> actors{actor(11, opposition, {4, 4}), actor(20, party, {4, 5}),
                                      actor(9, opposition, {3, 7}), actor(12, opposition, {2, 4})};
    const auto original = actors;
    const auto ids = [&]
    {
        std::vector<EntityId> result;
        for (auto index : presentation::combat_sprite_draw_order(actors))
        {
            result.push_back(actors[index].id);
        }
        return result;
    };
    check(ids() == std::vector<EntityId>{9, 20, 12, 11},
          "Lower monster and Goliath draw before monsters above; same-row order is deterministic");
    for (std::size_t i = 0; i < actors.size(); ++i)
    {
        check(actors[i].id == original[i].id && actors[i].cell == original[i].cell,
              "Rendering preserves initiative order and occupied cells");
    }
    std::reverse(actors.begin(), actors.end());
    check(ids() == std::vector<EntityId>{9, 20, 12, 11},
          "Initiative order cannot change sprite overlap");
    check(presentation::combat_sprite_draw_order({}).empty(),
          "Empty battlefield has no figures to draw");
    // A troll filling rows 4 and 5 stands on row 5 (FOOT-1): it draws before
    // a creature on row 4 above it and after one on row 6.
    auto troll = actor(30, opposition, {6, 4});
    troll.footprint = {1, 2};
    actors = {actor(31, party, {6, 3}), troll, actor(32, party, {5, 6})};
    check(ids() == std::vector<EntityId>{32, 30, 31},
          "A tall creature draws in the row of its lower square");
}
} // namespace

int main()
{
    try
    {
        ordering();
        std::cout << "Combat sprite layout checks passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
