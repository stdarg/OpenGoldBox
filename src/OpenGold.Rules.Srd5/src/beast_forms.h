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
    int strength, dexterity;  // ability modifiers
    std::array<int, 3> saves; // Strength, Dexterity and Constitution save bonuses
    int attack_bonus;
    DamageDice attack;
    bool pack_tactics{};  // Wolf: Advantage with an ally beside the target
    bool prone_bite{};    // Wolf: a hit knocks a Medium or smaller target Prone
    bool bloodied_fury{}; // Boar: Advantage on attack rolls while Bloodied
    // Boar: a hit after moving 20+ feet straight toward a Medium or smaller
    // target deals 1d6 more Piercing damage and knocks it Prone.
    bool charge{};
    // The stat block's Hit Points: Polymorph's Temporary Hit Points.
    int hit_points{};
    // The Druid level that may take the form: CR 1 from level 8.
    int level{2};
    // Tiger: Slashing damage, and a Large or smaller target falls Prone.
    bool slashing{}, prone_large{};
};

inline constexpr std::array beast_forms{
    BeastForm{"wolf",
              "Wolf",
              "Bite",
              2,
              12,
              40,
              2,
              2,
              {2, 2, 1},
              4,
              {1, 6, 2},
              true,
              true,
              false,
              false,
              11},
    BeastForm{"boar",
              "Boar",
              "Gore",
              2,
              11,
              40,
              1,
              0,
              {1, 0, 2},
              3,
              {1, 6, 1},
              false,
              false,
              true,
              true,
              13},
    BeastForm{"giant_lizard",
              "Giant Lizard",
              "Bite",
              3,
              12,
              40,
              2,
              1,
              {2, 3, 1},
              4,
              {1, 8, 2},
              false,
              false,
              false,
              false,
              19},
    BeastForm{"giant_snake",
              "Giant Snake",
              "Bite",
              3,
              13,
              30,
              2,
              2,
              {2, 2, 1},
              4,
              {1, 8, 2},
              false,
              false,
              false,
              false,
              13},
    // SRD 5.2.1 Tiger, CR 1: its Bonus Action Nimble Escape is left out.
    BeastForm{"tiger",
              "Tiger",
              "Rend",
              3,
              13,
              40,
              3,
              3,
              {3, 3, 2},
              5,
              {2, 6, 3},
              false,
              false,
              false,
              false,
              30,
              8,
              true,
              true}};

inline constexpr const BeastForm *beast_form(std::string_view key)
{
    for (const auto &form : beast_forms)
    {
        if (form.key == key)
        {
            return &form;
        }
    }
    return nullptr;
}
} // namespace opengold::srd5::detail
#endif
