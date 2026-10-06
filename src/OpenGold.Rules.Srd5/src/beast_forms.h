#ifndef OPENGOLD_SRD5_BEAST_FORMS_H
#define OPENGOLD_SRD5_BEAST_FORMS_H
#include "damage_roll.h"
#include <array>
#include <string_view>

namespace opengold::srd5::detail
{
// Wild Shape's Beast forms (CLASS-10): the SRD 5.2.1 stat blocks of the Beasts
// the game has combat art for that a Druid of levels 2-4 may adopt.
struct BeastForm
{
    std::string_view key, label, attack_label;
    int size; // 2 Medium, 3 Large
    int armor_class, speed;
    int strength, dexterity; // ability modifiers
    std::array<int, 3> saves; // Strength, Dexterity and Constitution save bonuses
    int attack_bonus;
    DamageDice attack;
    bool pack_tactics{};  // Wolf: Advantage with an ally beside the target
    bool prone_bite{};    // Wolf: a hit knocks a Medium or smaller target Prone
    bool bloodied_fury{}; // Boar: Advantage on attack rolls while Bloodied
};

inline constexpr std::array beast_forms{
    BeastForm{"wolf", "Wolf", "Bite", 2, 12, 40, 2, 2, {2, 2, 1}, 4, {1, 6, 2}, true, true},
    BeastForm{"boar", "Boar", "Gore", 2, 11, 40, 1, 0, {1, 0, 2}, 3, {1, 6, 1}, false, false, true},
    BeastForm{"giant_lizard", "Giant Lizard", "Bite", 3, 12, 40, 2, 1, {2, 3, 1}, 4, {1, 8, 2}},
    BeastForm{"giant_snake", "Giant Snake", "Bite", 3, 13, 30, 2, 2, {2, 2, 1}, 4, {1, 8, 2}}};

inline const BeastForm *beast_form(std::string_view key)
{
    for (const auto &form : beast_forms)
        if (form.key == key)
            return &form;
    return nullptr;
}
} // namespace opengold::srd5::detail
#endif
