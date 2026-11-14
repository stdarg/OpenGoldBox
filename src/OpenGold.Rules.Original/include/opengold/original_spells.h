#ifndef OPENGOLD_ORIGINAL_SPELLS_H
#define OPENGOLD_ORIGINAL_SPELLS_H

#include "opengold/gold_box_game.h"
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// The original rules' spells as Pool of Radiance casts them, and Curse of the
// Azure Bonds's besides (docs/ORIGINAL-RULES.md, "Combat", step 8).
namespace opengold::original
{

enum class SpellClass
{
    cleric,
    magic_user,
    // An item's own power, cast at level 12, or a monster's.
    item,
    // Curse's: a ranger's from level 8.
    druid
};

// A spell's facts by its number in the original's spell table.
struct Spell
{
    std::string_view name;
    SpellClass caster_class{};
    int level{};
    // Its range in squares: the fixed part and so much a caster level. A
    // fixed 255 is a touch that needs an attack roll; 0 with an area is a touch.
    int range_fixed{}, range_per_level{};
    // Its duration in minutes (a combat round is one): fixed, and per level.
    int minutes_fixed{}, minutes_per_level{};
    // Its targets, by the low 4 bits: 0 the caster; 1 to 7 separate creatures
    // ((area & 3) + 1 of them); 8 to 14 everyone within (area & 7) of a
    // square; 15 one creature.
    int area{};
    // Out of combat: 0 a fight only, 1 the caster, 2 one member, 4 the party.
    int camp_target{};
    // What a save does: 0 none is rolled, 1 negates, 2 halves damage, 3
    // rolled but changes nothing; and the save it is (0 to 4).
    int save_result{}, save_type{};
    // The effect each target gets, 0 for none.
    int effect{};
    // When it can be cast: 0 out of combat, 1 in a fight, 2 either.
    int when{};
    // The casting time; in a fight it takes casting_time / 3 off the
    // caster's initiative and goes off later in the round.
    int casting_time{};
    // How the original's AI weighs it: priority 0 (never) to 7, whether it
    // aims at an enemy, and how far around its aim it keeps friends clear.
    int ai_priority{};
    bool offensive{};
    int ai_radius{};
};

// Curse's spells beyond Pool of Radiance's are numbered this much higher than
// in Curse's own table, so one table holds both games' (canonical_spell).
inline constexpr int curse_spell_offset = 100;
// Secret of the Silver Blades' own spells and powers (Heal, Harm, Youth,
// Barkskin, Charm Person or Mammal, the druid's Cure Light Wounds and its
// numbers 101 to 117) are this much higher than in Secret's table.
inline constexpr int secret_spell_offset = 200;

// The spell or item power of this number (1 to 67, Curse's 158 and 166 to
// 200, and Secret's 236, 256, 290, 296, 298 and 310 to 317), or nothing.
[[nodiscard]] const Spell *spell(int number) noexcept;
// The spell as a game's table has it: Gateway's Strength may be cast in
// fights, its Prayer goes on up to four creatures the caster touches, its
// Blink may be cast out of fights, and its Invisibility 10' Radius reaches
// all within 6 squares of a square beside the caster (its spellTable).
[[nodiscard]] const Spell *spell(int number, GoldBoxGame game) noexcept;
// The number spell() knows for a game's own (its records', items' and
// scripts'): Curse's spells beyond Pool of Radiance's, 58 and 66 up, are
// curse_spell_offset higher.
[[nodiscard]] int canonical_spell(int number, GoldBoxGame game) noexcept;
// The spells a game's characters may know, in its table's order.
[[nodiscard]] std::vector<int> castable_spells(GoldBoxGame game);

// A spell's stable ID, its class and name ("magic_user:sleep"), as sheets
// and saves keep it; empty for a number that is no cleric or magic-user spell.
[[nodiscard]] std::string spell_id(int number);
// The cleric or magic-user spell an ID names, or nothing; a copy's "#2" is
// left out.
[[nodiscard]] std::optional<int> spell_number(std::string_view id);

} // namespace opengold::original
#endif
