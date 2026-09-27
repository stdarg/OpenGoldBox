#include "opengold/character_art.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace opengold::por
{
namespace
{
bool robe(unsigned id)
{
    return id >= 27 && id <= 31;
}

bool garment(unsigned color)
{
    return color == 1 || color == 9 || color == 3 || color == 11;
}

bool limb(unsigned color)
{
    return color == 2 || color == 10 || color == 4 || color == 12;
}

unsigned bank(const CharacterAppearance &a, bool action)
{
    return (a.tall ? 64u : 0u) + (action ? 128u : 0u);
}

const IndexedIcon &body_at(const CharacterArt &art, unsigned id)
{
    const auto &body = art.combat_bodies.at(id);
    if (body.width != 24 || body.height != 24 || body.pixels.size() != 576)
        throw std::runtime_error("Invalid combat layer dimensions");
    return body;
}

// Region metadata, not extracted artwork. Index 8 is shared by equipment,
// outlines and boots, so palette filtering alone cannot separate these images.
// Coordinates below describe tall poses; short equipment is two rows lower.
bool gray_equipment(unsigned id, bool action, bool tall, int x, int y)
{
    if (!tall)
        y -= 2;
    const auto rect = [&](int l, int t, int r, int b)
    {
        return x >= l && x <= r && y >= t && y <= b;
    };
    const auto stroke = [&](double x1, double y1, double x2, double y2, double width)
    {
        const auto dx = x2 - x1, dy = y2 - y1;
        const auto t = std::clamp(((x - x1) * dx + (y - y1) * dy) / (dx * dx + dy * dy), 0.0, 1.0);
        return std::hypot(x - x1 - t * dx, y - y1 - t * dy) <= width;
    };
    if (id == 0 || id == 27 || id == 29 || id == 32)
        return false;
    if (id == 1)
        return rect(16, 4, 23, 18) || rect(4, 4, 8, 8);
    if (id == 16)
        return rect(12, 7, 23, 12) || rect(4, 4, 8, 8);
    if (id == 5)
        return action ? rect(18, 9, 23, 14) : rect(18, 5, 22, 5);
    if (id == 21)
        return action ? rect(15, 13, 21, 13) : stroke(14, 13, 20, 16, 0.8);
    if (id == 28)
        return action ? rect(18, 2, 21, 19) : rect(19, 3, 20, 21);
    if (id == 30)
        return action ? rect(19, 11, 20, 14) : rect(19, 5, 21, 5);
    if (id == 31)
        return action ? stroke(3, 16, 21, 9, 0.8) : stroke(18, 2, 9, 19, 0.9);
    if (id == 19)
        return action ? stroke(3, 15, 22, 11, 1.0) : stroke(13, 18, 20, 2, 0.9);
    if (action)
    {
        if (id == 4)
            return rect(17, 8, 23, 20) || stroke(12, 14, 18, 8, 0.9);
        return rect(16, 0, 23, 20);
    }
    return rect(0, 0, 7, 15);
}

bool equipment(unsigned color, unsigned id, bool action, bool tall, int x, int y)
{
    return color == 6 || color == 14 || color == 7 || color == 15 ||
           (color == 8 && gray_equipment(id, action, tall, x, y));
}
} // namespace

IndexedIcon CharacterArt::combat_anatomy(const CharacterAppearance &a, bool action) const
{
    validate(a);
    const auto offset = bank(a, action);
    const auto &original = body_at(*this, offset + a.combat_body);
    IndexedIcon result{24, 24, std::vector<std::uint8_t>(576)};
    const int top = a.tall ? 8 : 10;
    const int waist = top + (robe(a.combat_body) ? 6 : 8);
    for (int y = 0; y < 24; ++y)
        for (int x = 0; x < 24; ++x)
        {
            const auto p = y * 24 + x;
            const auto color = original.pixels[p];
            if (garment(color))
                result.pixels[p] = color;
            else if (color == 8 && !gray_equipment(a.combat_body, action, a.tall, x, y) &&
                     ((y >= top && x >= 8 && x <= 14) || y >= waist))
                result.pixels[p] = color;
            // Restore only anatomy hidden by the original wielding arm/equipment.
            // Vote among homologous, locally decoded bodies of the same clothing
            // family. Visible saved garment pixels always take precedence.
            const bool torso = y >= top && y < waist && x >= 8 && x <= 14;
            const bool covered_leg =
                y >= waist && equipment(color, a.combat_body, action, a.tall, x, y);
            if (!result.pixels[p] && (torso || covered_leg))
            {
                std::array<unsigned, 16> count{};
                for (unsigned id = 0; id < 32; ++id)
                    if (robe(id) == robe(a.combat_body))
                    {
                        const auto c = body_at(*this, offset + id).pixels[p];
                        if (garment(c) || (c == 8 && !gray_equipment(id, action, a.tall, x, y)))
                            ++count[c];
                    }
                const auto chosen = std::max_element(count.begin(), count.end());
                if (*chosen)
                    result.pixels[p] = static_cast<std::uint8_t>(chosen - count.begin());
            }
        }
    return result;
}

Image CharacterArt::equipped_icon(const CharacterAppearance &a, unsigned equipment_body,
                                  bool action) const
{
    auto composed = combat_anatomy(a, action);
    const auto offset = bank(a, action);
    const auto &wielding = body_at(*this, offset + equipment_body);
    const int top = a.tall ? 8 : 10, waist = top + (robe(equipment_body) ? 6 : 8);
    for (int y = 0; y < 24; ++y)
        for (int x = 0; x < 24; ++x)
        {
            const auto p = y * 24 + x;
            const auto color = wielding.pixels[p];
            const bool arm_outline = color == 8 && y < waist && (x < 8 || x > 14) &&
                                     !gray_equipment(equipment_body, action, a.tall, x, y);
            if (limb(color) || arm_outline ||
                equipment(color, equipment_body, action, a.tall, x, y))
                composed.pixels[p] = color;
        }
    return compose_character_icon(combat_heads.at(offset + a.combat_head), composed, a);
}
} // namespace opengold::por
