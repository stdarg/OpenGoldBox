#include "../src/OpenGoldBox/combat_sprite_layout.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace godot;
using namespace opengold::rules;

namespace
{
void check(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}

bool near(double a, double b)
{
    return std::abs(a - b) < .001;
}

void sizing()
{
    // Different padding and proportions represent ready/action and customized art.
    for (const Rect2 visible : {Rect2(2, 1, 20, 22), Rect2(1, 3, 22, 19), Rect2(6, 0, 12, 24)})
        for (const double tile : {2.4, 24., 60., 72., 240.})
        {
            const Vector2 source(24, 24);
            const Rect2 occupied(Vector2(4 * tile, 5 * tile), Vector2(tile, tile));
            const auto rect = presentation::combat_sprite_rect(source, visible, occupied, true);
            const auto scale = rect.size / source;
            const Rect2 rendered(rect.position + visible.position * scale, visible.size * scale);
            check(near(rendered.size.x, tile) && near(rendered.size.y, 1.25 * tile),
                  "Goliath has stretched one-square width and 1.25-square visible height");
            check(near(rendered.position.x, occupied.position.x) &&
                      near(rendered.get_end().y, occupied.get_end().y),
                  "Visible feet align with the occupied square regardless of padding");
            check(near(rendered.position.y, occupied.position.y - .25 * tile),
                  "Only the lower quarter of the upper square receives Goliath art");
            const auto normal = presentation::combat_sprite_rect(source, visible, occupied, false);
            const auto normal_scale = normal.size / source;
            const Rect2 normal_visible(normal.position + visible.position * normal_scale,
                                       visible.size * normal_scale);
            check(normal_visible.get_center().is_equal_approx(occupied.get_center()),
                  "Visible figure stays centered regardless of transparent padding");
            check(near(std::max(normal_visible.size.x, normal_visible.size.y), .9 * tile),
                  "Visible figure fills 90 percent of the zoomed cell");
        }
    check(presentation::combat_sprite_rect({24, 24}, {}, {0, 0, 24, 24}, true).size == Vector2(),
          "Empty visible art does not divide by zero");
    check(presentation::combat_sprite_rect({24, 24}, {}, {0, 0, 24, 24}, false).size == Vector2(),
          "Empty normal art does not divide by zero");
}

void ordering()
{
    const auto actor = [](EntityId id, unsigned side, Cell cell)
    {
        CombatantView value;
        value.id = id;
        value.side = side;
        value.cell = cell;
        return value;
    };
    std::vector<CombatantView> actors{actor(11, 1, {4, 4}), actor(20, 0, {4, 5}),
                                      actor(9, 1, {3, 7}), actor(12, 1, {2, 4})};
    const auto original = actors;
    const auto ids = [&]
    {
        std::vector<EntityId> result;
        for (auto index : presentation::combat_sprite_draw_order(actors))
            result.push_back(actors[index].id);
        return result;
    };
    check(ids() == std::vector<EntityId>{9, 20, 12, 11},
          "Lower monster and Goliath draw before monsters above; same-row order is deterministic");
    for (std::size_t i = 0; i < actors.size(); ++i)
        check(actors[i].id == original[i].id && actors[i].cell == original[i].cell,
              "Rendering preserves initiative order and occupied cells");
    std::reverse(actors.begin(), actors.end());
    check(ids() == std::vector<EntityId>{9, 20, 12, 11},
          "Initiative order cannot change sprite overlap");
    check(presentation::combat_sprite_draw_order({}).empty(),
          "Empty battlefield has no figures to draw");
}
} // namespace

int main()
{
    try
    {
        sizing();
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
