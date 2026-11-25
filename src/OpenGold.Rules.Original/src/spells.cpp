#include "opengold/original_spells.h"
#include "death_knights_spell_table.h"
#include "dark_queen_spell_table.h"
#include <cctype>
#include <map>

#include <array>
#include <algorithm>

namespace opengold::original
{
namespace
{

// The original's spell table (gdconsts.pas, spellTable): spells 1 to 56,
// then the powers of items, 57 to 67, cast at level 12.
constexpr std::array<Spell, 67> spells{{
    {"Bless", SpellClass::cleric, 1, 6, 0, 6, 0, 10, 4, 0, 4, 1, 2, 10, 2, false, 0},
    {"Curse", SpellClass::cleric, 1, 6, 0, 6, 0, 10, 0, 0, 4, 2, 1, 10, 3, true, 0},
    {"Cure Light Wounds", SpellClass::cleric, 1, 0, 0, 0, 0, 4, 2, 0, 4, 0, 2, 5, 1, false, 0},
    {"Cause Light Wounds", SpellClass::cleric, 1, 255, 0, 0, 0, 4, 0, 0, 4, 0, 1, 5, 2, true, 0},
    {"Detect Magic", SpellClass::cleric, 1, 3, 0, 10, 0, 0, 1, 0, 4, 5, 2, 1, 0, false, 0},
    {"Protection from Evil", SpellClass::cleric, 1, 0, 0, 0, 3, 4, 2, 0, 4, 8, 2, 4, 1, false, 0},
    {"Protection from Good", SpellClass::cleric, 1, 0, 0, 0, 3, 4, 2, 0, 4, 9, 2, 4, 2, false, 0},
    {"Resist Cold", SpellClass::cleric, 1, 0, 0, 0, 10, 4, 2, 0, 4, 10, 2, 10, 0, false, 0},
    {"Burning Hands", SpellClass::magic_user, 1, 0, 0, 0, 0, 4, 0, 0, 4, 0, 1, 1, 4, true, 0},
    {"Charm Person", SpellClass::magic_user, 1, 12, 0, 0, 0, 4, 0, 1, 4, 11, 1, 1, 6, true, 0},
    {"Detect Magic", SpellClass::magic_user, 1, 0, 0, 0, 2, 0, 1, 0, 4, 5, 2, 1, 0, false, 0},
    {"Enlarge", SpellClass::magic_user, 1, 0, 2, 0, 10, 4, 2, 0, 4, 12, 2, 1, 0, false, 0},
    {"Reduce", SpellClass::magic_user, 1, 0, 2, 0, 10, 4, 2, 1, 4, 13, 2, 1, 0, true, 0},
    {"Friends", SpellClass::magic_user, 1, 0, 0, 0, 1, 0, 1, 0, 4, 14, 0, 1, 0, false, 0},
    {"Magic Missile", SpellClass::magic_user, 1, 6, 4, 0, 0, 4, 0, 0, 4, 0, 1, 1, 6, true, 0},
    {"Protection from Evil", SpellClass::magic_user, 1, 0, 0, 0, 2, 4, 2, 0, 4, 8, 2, 1, 1, false,
     0},
    {"Protection from Good", SpellClass::magic_user, 1, 0, 0, 0, 2, 4, 2, 0, 4, 9, 2, 1, 2, false,
     0},
    {"Read Magic", SpellClass::magic_user, 1, 0, 0, 0, 2, 0, 1, 0, 4, 16, 0, 10, 0, false, 0},
    {"Shield", SpellClass::magic_user, 1, 0, 0, 0, 5, 0, 1, 0, 4, 17, 2, 1, 3, false, 0},
    {"Shocking Grasp", SpellClass::magic_user, 1, 255, 0, 0, 0, 4, 0, 0, 4, 0, 1, 1, 4, true, 0},
    {"Sleep", SpellClass::magic_user, 1, 3, 4, 0, 5, 9, 0, 0, 4, 53, 1, 1, 7, true, 1},
    {"Find Traps", SpellClass::cleric, 2, 0, 0, 30, 0, 0, 1, 0, 4, 19, 0, 5, 0, false, 0},
    {"Hold Person", SpellClass::cleric, 2, 6, 0, 4, 1, 6, 0, 1, 4, 52, 1, 5, 7, true, 0},
    {"Resist Fire", SpellClass::cleric, 2, 0, 0, 0, 10, 4, 2, 0, 4, 20, 2, 5, 2, false, 0},
    {"Silence, 15' Radius", SpellClass::cleric, 2, 12, 0, 0, 2, 31, 0, 3, 4, 21, 1, 5, 4, true, 1},
    {"Slow Poison", SpellClass::cleric, 2, 0, 0, 0, 60, 4, 2, 0, 4, 22, 2, 1, 0, false, 0},
    {"Snake Charm", SpellClass::cleric, 2, 3, 0, 0, 0, 240, 0, 0, 4, 51, 1, 5, 0, true, 0},
    {"Spiritual Hammer", SpellClass::cleric, 2, 3, 0, 0, 1, 0, 1, 0, 4, 23, 2, 5, 1, false, 0},
    {"Detect Invisibility", SpellClass::magic_user, 2, 0, 4, 0, 5, 0, 1, 0, 4, 24, 2, 2, 1, false,
     0},
    {"Invisibility", SpellClass::magic_user, 2, 0, 0, 0, 0, 4, 2, 0, 4, 25, 2, 2, 2, false, 0},
    {"Knock", SpellClass::magic_user, 2, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 1, 0, false, 0},
    {"Mirror Image", SpellClass::magic_user, 2, 0, 0, 0, 2, 0, 1, 0, 4, 28, 2, 2, 3, false, 0},
    {"Ray of Enfeeblement", SpellClass::magic_user, 2, 1, 1, 0, 1, 4, 0, 1, 4, 29, 1, 2, 2, true,
     0},
    {"Stinking Cloud", SpellClass::magic_user, 2, 3, 0, 0, 1, 9, 0, 3, 0, 30, 1, 2, 7, true, 1},
    {"Strength", SpellClass::magic_user, 2, 0, 0, 0, 60, 0, 2, 0, 4, 38, 0, 10, 0, false, 0},
    {"Animate Dead", SpellClass::cleric, 3, 1, 0, 0, 0, 240, 4, 0, 4, 32, 2, 10, 2, false, 0},
    {"Cure Blindness", SpellClass::cleric, 3, 0, 0, 0, 0, 4, 2, 0, 4, 0, 2, 10, 0, false, 0},
    {"Cause Blindness", SpellClass::cleric, 3, 255, 0, 0, 0, 4, 0, 1, 4, 33, 1, 10, 3, true, 0},
    {"Cure Disease", SpellClass::cleric, 3, 0, 0, 0, 0, 0, 2, 0, 4, 0, 0, 100, 0, false, 0},
    {"Cause Disease", SpellClass::cleric, 3, 255, 0, 0, 0, 4, 0, 1, 4, 34, 1, 100, 4, true, 0},
    {"Dispel Magic", SpellClass::cleric, 3, 6, 0, 0, 0, 9, 2, 0, 4, 0, 2, 4, 5, true, 1},
    {"Prayer", SpellClass::cleric, 3, 0, 0, 0, 1, 0, 1, 0, 4, 49, 2, 6, 6, false, 0},
    {"Remove Curse", SpellClass::cleric, 3, 0, 0, 0, 0, 4, 2, 0, 4, 0, 2, 6, 0, false, 0},
    {"Bestow Curse", SpellClass::cleric, 3, 255, 0, 0, 10, 4, 0, 1, 4, 36, 1, 6, 4, true, 0},
    {"Blink", SpellClass::magic_user, 3, 0, 0, 0, 1, 0, 1, 0, 4, 37, 1, 1, 2, false, 0},
    {"Dispel Magic", SpellClass::magic_user, 3, 12, 0, 0, 1, 9, 2, 0, 4, 0, 2, 3, 2, true, 1},
    {"Fireball", SpellClass::magic_user, 3, 10, 1, 0, 0, 11, 0, 2, 4, 0, 1, 3, 7, true, 3},
    {"Haste", SpellClass::magic_user, 3, 6, 0, 3, 1, 10, 4, 0, 4, 39, 2, 3, 3, false, 0},
    {"Hold Person", SpellClass::magic_user, 3, 12, 0, 0, 2, 7, 0, 1, 4, 52, 1, 3, 6, true, 0},
    {"Invisibility, 10' Radius", SpellClass::magic_user, 3, 0, 0, 0, 0, 9, 4, 0, 4, 25, 2, 3, 1,
     false, 0},
    {"Lightning Bolt", SpellClass::magic_user, 3, 4, 1, 0, 0, 8, 0, 2, 4, 0, 1, 3, 7, true, 0},
    {"Protection from Evil, 10' Radius", SpellClass::magic_user, 3, 0, 0, 0, 2, 4, 2, 0, 4, 45, 2,
     3, 1, false, 0},
    {"Protection from Good, 10' Radius", SpellClass::magic_user, 3, 0, 0, 0, 2, 4, 2, 0, 4, 46, 2,
     3, 2, false, 0},
    {"Protection from Normal Missiles", SpellClass::magic_user, 3, 0, 0, 0, 10, 4, 2, 0, 4, 41, 2,
     3, 3, false, 0},
    {"Slow", SpellClass::magic_user, 3, 9, 1, 3, 1, 10, 0, 0, 4, 42, 1, 3, 5, true, 0},
    {"Restoration", SpellClass::cleric, 7, 0, 0, 0, 0, 4, 2, 0, 4, 0, 2, 6, 0, false, 0},
    {"Potion of Speed", SpellClass::item, 6, 0, 0, 0, 0, 0, 1, 0, 4, 39, 2, 0, 3, false, 0},
    {"Keoghtum's Ointment", SpellClass::item, 6, 0, 0, 0, 0, 4, 2, 0, 4, 0, 2, 0, 1, false, 0},
    {"Potion of Giant Strength", SpellClass::item, 6, 0, 0, 0, 0, 0, 1, 0, 4, 38, 2, 0, 1, false,
     0},
    {"Javelin of Lightning", SpellClass::item, 6, 4, 4, 0, 0, 4, 0, 2, 4, 0, 1, 0, 7, true, 0},
    {"Wand of Paralyzation", SpellClass::item, 6, 6, 0, 0, 0, 4, 0, 1, 0, 52, 1, 0, 7, true, 0},
    {"Potion of Healing", SpellClass::item, 6, 0, 0, 0, 0, 0, 1, 0, 4, 39, 2, 0, 1, false, 0},
    {"Dust of Disappearance", SpellClass::item, 6, 0, 0, 0, 0, 7, 4, 0, 4, 71, 2, 0, 2, false, 0},
    {"Necklace of Missiles", SpellClass::item, 6, 7, 0, 0, 0, 11, 0, 2, 4, 0, 1, 0, 7, true, 3},
    {"Wand of Magic Missiles", SpellClass::item, 6, 12, 0, 0, 0, 4, 0, 0, 4, 0, 1, 0, 6, true, 0},
    {"Unused power", SpellClass::item, 6, 0, 0, 0, 0, 0, 1, 0, 4, 0, 2, 0, 0, false, 0},
    {"Manual of Bodily Health", SpellClass::item, 6, 0, 0, 0, 0, 0, 1, 0, 4, 4, 0, 0, 0, false, 0},
}};

// Curse of the Azure Bonds's spells beyond Pool of Radiance's (its spell
// table, spells 58 and 66 to 100, in its reconstruction), by the numbers the
// rules give them: 100 more than Curse's own (curse_spell_offset), with their
// effects by the fight's numbers (canonical_effect). Curse keeps Pool of
// Radiance's item powers 57 and 59 to 65.
struct NumberedSpell
{
    int number;
    Spell spell;
};

constexpr std::array<NumberedSpell, 36> curse_spells{{
    {158,
     {"Cure Serious Wounds", SpellClass::cleric, 4, 0, 0, 0, 0, 4, 2, 0, 4, 0, 2, 7, 1, false, 0}},
    {166,
     {"Cause Serious Wounds", SpellClass::cleric, 4, 0, 0, 0, 0, 4, 0, 0, 4, 0, 1, 7, 5, true, 0}},
    {167,
     {"Neutralize Poison", SpellClass::cleric, 4, 0, 0, 0, 0, 0, 2, 0, 4, 0, 0, 7, 0, false, 0}},
    {168, {"Poison", SpellClass::cleric, 4, 0, 0, 0, 0, 4, 0, 1, 0, 0, 1, 7, 6, true, 0}},
    {169,
     {"Protection from Evil, 10' Radius", SpellClass::cleric, 4, 3, 0, 0, 10, 4, 2, 0, 4, 45, 2, 7,
      2, false, 0}},
    {170,
     {"Sticks to Snakes", SpellClass::cleric, 4, 3, 0, 0, 2, 4, 0, 0, 4, 203, 1, 7, 4, true, 0}},
    {171,
     {"Cure Critical Wounds", SpellClass::cleric, 5, 0, 0, 0, 0, 4, 2, 0, 4, 0, 2, 8, 2, false, 0}},
    {172,
     {"Cause Critical Wounds", SpellClass::cleric, 5, 255, 0, 0, 0, 4, 0, 1, 4, 0, 1, 8, 6, true,
      0}},
    {173, {"Dispel Evil", SpellClass::cleric, 5, 0, 0, 0, 1, 0, 0, 0, 4, 345, 1, 8, 3, false, 0}},
    {174, {"Flame Strike", SpellClass::cleric, 5, 6, 0, 0, 0, 4, 0, 2, 4, 0, 1, 8, 6, true, 0}},
    {175, {"Raise Dead", SpellClass::cleric, 5, 0, 0, 0, 0, 0, 2, 0, 4, 0, 0, 10, 1, false, 0}},
    {176, {"Slay Living", SpellClass::cleric, 5, 3, 0, 0, 0, 4, 0, 1, 4, 0, 1, 10, 7, true, 0}},
    {177, {"Detect Magic", SpellClass::druid, 1, 3, 0, 12, 0, 0, 1, 0, 4, 5, 2, 3, 1, false, 0}},
    {178, {"Entangle", SpellClass::druid, 1, 8, 0, 10, 0, 11, 0, 1, 4, 336, 1, 3, 3, true, 0}},
    {179, {"Faerie Fire", SpellClass::druid, 1, 8, 0, 0, 4, 5, 0, 1, 4, 207, 1, 3, 4, true, 0}},
    {180,
     {"Invisibility to Animals", SpellClass::druid, 1, 255, 0, 10, 1, 4, 2, 0, 4, 269, 2, 4, 1,
      true, 0}},
    {181,
     {"Charm Monsters", SpellClass::magic_user, 4, 6, 0, 0, 0, 5, 0, 1, 4, 11, 1, 4, 6, true, 0}},
    {182,
     {"Confusion", SpellClass::magic_user, 4, 12, 0, 2, 1, 11, 0, 1, 4, 235, 1, 4, 7, true, 0}},
    {183,
     {"Dimension Door", SpellClass::magic_user, 4, 0, 3, 0, 0, 8, 0, 0, 4, 0, 1, 1, 0, true, 0}},
    {184, {"Fear", SpellClass::magic_user, 4, 6, 0, 0, 1, 8, 0, 1, 4, 342, 1, 4, 6, true, 0}},
    {185, {"Fire Shield", SpellClass::magic_user, 4, 0, 0, 2, 1, 0, 1, 0, 4, 0, 2, 4, 8, false, 0}},
    {186, {"Fumble", SpellClass::magic_user, 4, 0, 1, 0, 1, 4, 0, 1, 4, 31, 1, 4, 4, true, 0}},
    {187, {"Ice Storm", SpellClass::magic_user, 4, 0, 1, 0, 0, 10, 0, 0, 4, 0, 1, 4, 7, true, 0}},
    {188,
     {"Minor Globe of Invulnerability", SpellClass::magic_user, 4, 0, 1, 0, 1, 0, 1, 0, 4, 263, 2,
      4, 5, false, 0}},
    {189,
     {"Remove Curse", SpellClass::magic_user, 4, 0, 0, 0, 0, 4, 2, 0, 4, 0, 2, 4, 0, false, 0}},
    {190, {"Animate Dead", SpellClass::item, 5, 1, 0, 0, 0, 240, 4, 0, 4, 32, 2, 5, 0, false, 0}},
    {191, {"Cloudkill", SpellClass::magic_user, 5, 2, 0, 0, 1, 9, 0, 0, 4, 0, 2, 5, 5, true, 0}},
    {192, {"Cone of Cold", SpellClass::magic_user, 5, 6, 0, 0, 0, 8, 0, 2, 4, 0, 1, 5, 6, true, 0}},
    {193,
     {"Feeblemind", SpellClass::magic_user, 5, 16, 0, 0, 0, 4, 0, 1, 4, 268, 1, 5, 6, true, 0}},
    {194,
     {"Hold Monster", SpellClass::magic_user, 5, 0, 1, 0, 1, 7, 0, 1, 4, 52, 1, 5, 7, true, 0}},
    {195,
     {"Protection from Dragon Breath", SpellClass::item, 6, 0, 0, 0, 0, 0, 1, 0, 4, 273, 2, 10, 1,
      false, 0}},
    {196,
     {"Resist Paralysis", SpellClass::item, 6, 0, 0, 0, 0, 0, 1, 0, 4, 109, 2, 10, 1, false, 0}},
    {197, {"Invisibility", SpellClass::item, 6, 0, 0, 0, 0, 0, 1, 0, 4, 25, 2, 0, 1, false, 0}},
    {198, {"Blast", SpellClass::item, 6, 3, 0, 0, 0, 11, 0, 1, 4, 0, 1, 0, 1, true, 0}},
    {199, {"Healing", SpellClass::item, 6, 0, 0, 0, 0, 0, 1, 0, 4, 0, 2, 0, 1, false, 0}},
    {200,
     {"Bestow Curse", SpellClass::magic_user, 4, 0, 0, 0, 10, 4, 0, 1, 4, 36, 1, 4, 4, true, 0}},
}};

// Secret of the Silver Blades' own spells (its spell table, in its
// reconstruction's gdconsts.pas), by the numbers the rules give them: 200
// more than Secret's own (secret_spell_offset), with their effects by the
// fight's numbers (canonical_effect) and the party as camp target 4, as Pool
// of Radiance's table has it. Its numbers 101 to 109 cast nothing and are
// left out. Youth (63), an item's power, makes its user younger whatever it
// aims at (SpellYouth), so it takes no aim (area 0) and goes on its user out
// of fights (camp target 1).
constexpr std::array<NumberedSpell, 14> secret_spells{{
    {236, {"Heal", SpellClass::cleric, 6, 5, 0, 0, 0, 4, 2, 0, 4, 0, 2, 0, 2, false, 0}},
    {256, {"Harm", SpellClass::cleric, 6, 0, 0, 0, 0, 4, 0, 0, 4, 0, 1, 10, 0, true, 0}},
    {263, {"Youth", SpellClass::item, 6, 0, 0, 0, 0, 0, 1, 0, 4, 0, 2, 0, 0, false, 0}},
    {290, {"Barkskin", SpellClass::druid, 2, 1, 0, 0, 0, 240, 4, 0, 4, 413, 2, 3, 0, false, 0}},
    {296,
     {"Charm Person or Mammal", SpellClass::druid, 2, 8, 0, 60, 60, 4, 0, 1, 4, 11, 1, 4, 5, true,
      0}},
    {298,
     {"Cure Light Wounds", SpellClass::druid, 2, 255, 0, 0, 0, 4, 2, 0, 4, 0, 2, 4, 2, false, 0}},
    {310, {"Death Spell", SpellClass::magic_user, 6, 0, 4, 0, 0, 9, 0, 0, 4, 0, 1, 6, 3, true, 0}},
    {311, {"Disintegrate", SpellClass::magic_user, 6, 0, 2, 0, 0, 4, 0, 1, 4, 0, 1, 6, 8, true, 0}},
    {312,
     {"Globe of Invulnerability", SpellClass::magic_user, 6, 0, 0, 0, 1, 4, 1, 0, 4, 457, 2, 1, 3,
      false, 0}},
    {313,
     {"Stone to Flesh", SpellClass::magic_user, 6, 0, 4, 0, 0, 4, 2, 0, 4, 0, 2, 6, 2, false, 0}},
    {314,
     {"Flesh to Stone", SpellClass::magic_user, 6, 0, 4, 0, 0, 4, 0, 1, 4, 465, 1, 6, 7, true, 0}},
    {315,
     {"Delayed Blast Fireball", SpellClass::magic_user, 7, 10, 4, 0, 0, 11, 0, 2, 4, 0, 1, 1, 9,
      true, 0}},
    {316,
     {"Mass Invisibility", SpellClass::magic_user, 7, 0, 4, 0, 0, 10, 4, 0, 4, 25, 2, 7, 3, false,
      0}},
    {317,
     {"Power Word Stun", SpellClass::magic_user, 7, 0, 2, 0, 0, 4, 0, 0, 4, 506, 1, 1, 3, true, 0}},
}};

// Pools of Darkness's own spells (its reconstruction's spellTable, 101 to 109
// and 118 to 126), numbered 200 higher as Secret's are, with their effects by
// the fight's numbers: its own beyond Secret's (118 to 123) secret_effect_offset
// higher, as canonical_effect numbers Secret's. Treasures of the Savage
// Frontier's table is the same.
constexpr std::array<NumberedSpell, 18> darkness_spells{{
    {301, {"Blade Barrier", SpellClass::cleric, 6, 3, 0, 0, 3, 9, 0, 1, 4, 0, 1, 9, 9, true, 0}},
    {302, {"Restoration", SpellClass::cleric, 7, 0, 0, 0, 0, 0, 2, 0, 4, 0, 0, 1, 0, false, 0}},
    {303, {"Energy Drain", SpellClass::cleric, 7, 0, 0, 0, 0, 4, 0, 0, 4, 0, 1, 3, 8, true, 0}},
    {304, {"Destruction", SpellClass::cleric, 7, 0, 0, 0, 0, 4, 0, 0, 4, 0, 1, 10, 9, true, 0}},
    {305, {"Resurrection", SpellClass::cleric, 7, 0, 0, 0, 0, 0, 2, 0, 4, 0, 0, 10, 0, false, 0}},
    {306, {"Cure Disease", SpellClass::druid, 3, 0, 0, 0, 0, 0, 2, 0, 4, 0, 0, 1, 0, false, 0}},
    {307,
     {"Neutralize Poison", SpellClass::druid, 3, 0, 0, 0, 0, 0, 2, 0, 4, 0, 0, 1, 0, false, 0}},
    {308, {"Hold Animal", SpellClass::druid, 3, 8, 0, 0, 2, 7, 0, 1, 4, 52, 1, 5, 4, true, 0}},
    {309,
     {"Protection from Fire", SpellClass::druid, 3, 0, 0, 0, 0, 4, 2, 0, 4, 519, 2, 5, 3, false,
      0}},
    {318,
     {"Fire Touch", SpellClass::magic_user, 5, 0, 0, 0, 1, 4, 2, 0, 4, 521, 2, 6, 4, false, 0}},
    {319, {"Iron Skin", SpellClass::magic_user, 5, 0, 0, 0, 1, 0, 1, 0, 4, 522, 2, 5, 3, false, 0}},
    {320, {"Mass Charm", SpellClass::magic_user, 8, 0, 1, 0, 0, 9, 0, 1, 4, 11, 1, 8, 6, true, 0}},
    {321,
     {"Otto's Irresistible Dance", SpellClass::magic_user, 8, 255, 0, 0, 0, 4, 0, 0, 4, 518, 1, 5,
      5, true, 0}},
    {322,
     {"Mind Blank", SpellClass::magic_user, 8, 3, 0, 0, 0, 4, 2, 0, 4, 523, 2, 1, 0, false, 0}},
    {323,
     {"Power Word Blind", SpellClass::magic_user, 8, 0, 1, 0, 0, 10, 0, 0, 4, 0, 1, 1, 3, true, 0}},
    {324, {"Meteor Swarm", SpellClass::magic_user, 9, 4, 1, 0, 0, 8, 0, 2, 4, 0, 1, 9, 3, true, 0}},
    {325,
     {"Power Word Kill", SpellClass::magic_user, 9, 0, 1, 0, 0, 9, 0, 0, 4, 0, 1, 1, 6, true, 0}},
    {326,
     {"Monster Summoning", SpellClass::magic_user, 9, 9, 0, 0, 0, 0, 0, 0, 4, 0, 1, 9, 0, true, 0}},
}};

// The four rows Gateway to the Savage Frontier's table changes from Curse's
// (its reconstruction's spellTable): Strength in fights too, Prayer on up
// to four creatures within reach, Blink out of fights too, and Invisibility
// 10' Radius aimed beside the caster and reaching 6 squares around.
constexpr std::array<NumberedSpell, 4> gateway_rows{{
    {35, {"Strength", SpellClass::magic_user, 2, 0, 0, 0, 60, 0, 2, 0, 4, 38, 2, 10, 0, false, 0}},
    {42, {"Prayer", SpellClass::cleric, 3, 0, 0, 0, 1, 3, 1, 0, 4, 49, 2, 6, 6, false, 0}},
    {45, {"Blink", SpellClass::magic_user, 3, 0, 0, 0, 1, 0, 1, 0, 4, 37, 2, 1, 2, false, 0}},
    {50,
     {"Invisibility, 10' Radius", SpellClass::magic_user, 3, 1, 0, 0, 0, 14, 4, 0, 4, 25, 2, 3, 1,
      false, 0}},
}};

// What Treasures of the Savage Frontier's table (its reconstruction's
// gdconsts.pas, spellTable) changes in the rows it shares with the earlier
// games', by the rules' numbers (canonical_spell): ranges, durations, what a
// save does, when it may be cast, and how its AI weighs it. Its own way of
// numbering camp targets and effects is the rules' as before. Pools of
// Darkness's table is the same in all these (its gdconsts.pas), so the
// 256-color engine plays them in both games.
struct TreasuresRow
{
    int number;
    int range_fixed, range_per_level, minutes_fixed, minutes_per_level, save_result, when,
        ai_priority;
};

constexpr std::array<TreasuresRow, 45> treasures_changes{{
    {1, 6, 0, 6, 0, 0, 2, 1},     // Bless
    {2, 6, 0, 6, 0, 0, 1, 1},     // Curse
    {3, 0, 0, 0, 0, 0, 2, 0},     // Cure Light Wounds
    {4, 255, 0, 0, 0, 0, 1, 1},   // Cause Light Wounds
    {7, 0, 0, 0, 3, 0, 2, 1},     // Protection from Good
    {9, 0, 0, 0, 0, 0, 1, 2},     // Burning Hands
    {10, 12, 0, 60, 60, 1, 1, 5}, // Charm Person
    {15, 6, 4, 0, 0, 0, 1, 5},    // Magic Missile
    {17, 0, 0, 0, 2, 0, 2, 1},    // Protection from Good
    {19, 0, 0, 0, 5, 0, 2, 2},    // Shield
    {20, 255, 0, 0, 0, 0, 1, 2},  // Shocking Grasp
    {21, 3, 4, 0, 5, 0, 1, 4},    // Sleep
    {24, 0, 0, 0, 10, 0, 2, 0},   // Resist Fire
    {25, 12, 0, 0, 2, 1, 1, 2},   // Silence, 15' Radius
    {26, 0, 0, 0, 60, 0, 0, 0},   // Slow Poison
    {33, 1, 1, 0, 1, 1, 1, 1},    // Ray of Enfeeblement
    {34, 3, 0, 0, 1, 3, 1, 4},    // Stinking Cloud
    {41, 6, 0, 0, 0, 0, 2, 1},    // Dispel Magic
    {42, 0, 0, 0, 1, 0, 2, 5},    // Prayer
    {44, 255, 0, 0, 10, 1, 1, 2}, // Bestow Curse
    {45, 0, 0, 0, 1, 0, 1, 4},    // Blink
    {46, 12, 0, 0, 1, 0, 2, 1},   // Dispel Magic
    {47, 10, 4, 0, 0, 2, 1, 7},   // Fireball
    {48, 6, 0, 3, 1, 0, 2, 5},    // Haste
    {51, 4, 4, 0, 0, 2, 1, 7},    // Lightning Bolt
    {55, 9, 4, 3, 1, 0, 1, 4},    // Slow
    {158, 0, 0, 0, 0, 0, 2, 0},   // Cure Serious Wounds
    {166, 0, 0, 0, 0, 0, 1, 3},   // Cause Serious Wounds
    {169, 3, 0, 0, 10, 0, 2, 1},  // Protection from Evil, 10' Radius
    {172, 255, 0, 0, 0, 1, 1, 4}, // Cause Critical Wounds
    {173, 0, 0, 0, 1, 0, 1, 5},   // Dispel Evil
    {175, 0, 0, 0, 0, 0, 0, 0},   // Raise Dead
    {178, 8, 0, 10, 0, 1, 1, 1},  // Entangle
    {179, 8, 0, 0, 4, 1, 1, 1},   // Faerie Fire
    {183, 0, 12, 0, 0, 0, 1, 0},  // Dimension Door
    {185, 0, 0, 2, 1, 0, 2, 7},   // Fire Shield
    {186, 0, 4, 0, 1, 1, 1, 4},   // Fumble
    {187, 0, 4, 0, 0, 0, 1, 7},   // Ice Storm
    {188, 0, 16, 0, 1, 0, 2, 6},  // Minor Globe of Invulnerability
    {193, 16, 0, 0, 0, 1, 1, 5},  // Feeblemind
    {194, 0, 2, 0, 1, 1, 1, 7},   // Hold Monster
    {195, 0, 0, 0, 1, 0, 2, 1},   // Protection from Dragon Breath
    {200, 3, 0, 0, 3, 1, 1, 4},   // Bestow Curse
    {236, 0, 0, 0, 0, 0, 2, 2},   // Heal
    {313, 0, 4, 0, 0, 0, 2, 0},   // Stone to Flesh
}};

// Curse's spells that its characters can know, in its table's order: Pool of
// Radiance's 56, then its own.
constexpr std::array<int, 30> curse_castable{158, 166, 167, 168, 169, 170, 171, 172, 173, 174,
                                             175, 176, 177, 178, 179, 180, 181, 182, 183, 184,
                                             185, 186, 187, 188, 189, 191, 192, 193, 194, 200};

// Pools of Darkness's table of 126 spells, which Treasures shares and
// Unlimited Adventures' kit keeps, its spells numbered as Pools of
// Darkness's from 1 to 126 (its spelltab.c, docs/games/ua.md, Monsters'
// spells).
[[nodiscard]] constexpr bool darkness_table(GoldBoxGame game) noexcept
{
    return vga_engine(game) || game == GoldBoxGame::unlimited_adventures;
}

} // namespace

int canonical_spell(int number, GoldBoxGame game) noexcept
{
    if (game == GoldBoxGame::dark_queen_of_krynn)
    {
        return number >= 1 && number <= static_cast<int>(dark_queen_data::spells.size())
                   ? dark_queen_data::spells[static_cast<std::size_t>(number - 1)].canonical
                   : 0;
    }
    if (game == GoldBoxGame::death_knights_of_krynn)
    {
        return number >= 1 && number <= 114
                   ? death_knights_data::spells[static_cast<std::size_t>(number - 1)].canonical
                   : 0;
    }
    if (game == GoldBoxGame::champions_of_krynn && number >= 101 && number <= 107)
    {
        return number + 400;
    }
    // Pools of Darkness numbers its spells as Secret does, and its new ones
    // (101 to 109, 118 to 126) take numbers Secret leaves free.
    if (game == GoldBoxGame::secret_of_the_silver_blades || darkness_table(game))
    {
        // Secret keeps Curse's numbers but where its own spells took them.
        static constexpr std::array<int, 6> own{36, 56, 63, 90, 96, 98};
        if (std::ranges::find(own, number) != own.end() || number > 100)
        {
            return number + secret_spell_offset;
        }
    }
    const bool curse_only = number == 58 || number >= 66;
    return game != GoldBoxGame::pool_of_radiance && curse_only ? number + curse_spell_offset
                                                               : number;
}

int canonical_item_power(int number, GoldBoxGame game) noexcept
{
    if (game == GoldBoxGame::dark_queen_of_krynn)
    {
        // charview.c, use_item: these item powers share stored numbers
        // with unrelated ordinary spells and are remapped only on use.
        static constexpr std::array<std::pair<int, int>, 11> moved{{{57, 127},
                                                                    {59, 128},
                                                                    {60, 129},
                                                                    {61, 130},
                                                                    {62, 131},
                                                                    {63, 132},
                                                                    {64, 133},
                                                                    {65, 134},
                                                                    {95, 135},
                                                                    {97, 136},
                                                                    {99, 137}}};
        const auto found = std::ranges::find(moved, number, &std::pair<int, int>::first);
        return canonical_spell(found == moved.end() ? number : found->second, game);
    }
    return game == GoldBoxGame::pool_of_radiance ? (number > 56 ? number - 23 : number)
                                                 : canonical_spell(number & 127, game);
}

int native_spell(int number, GoldBoxGame game) noexcept
{
    if (game == GoldBoxGame::dark_queen_of_krynn)
    {
        if (number > 0)
        {
            for (std::size_t at = 0; at < dark_queen_data::spells.size(); ++at)
            {
                if (dark_queen_data::spells[at].canonical == number)
                {
                    return static_cast<int>(at + 1);
                }
            }
        }
        return 0;
    }
    if (game == GoldBoxGame::death_knights_of_krynn)
    {
        if (number <= 0)
        {
            return 0;
        }
        for (std::size_t at = 0; at < death_knights_data::spells.size(); ++at)
        {
            if (death_knights_data::spells[at].canonical == number)
            {
                return static_cast<int>(at + 1);
            }
        }
        return 0;
    }
    return number >= 501 && number <= 507 ? number - 400
           : number > secret_spell_offset ? number - secret_spell_offset
           : number > curse_spell_offset  ? number - curse_spell_offset
                                          : number;
}

std::vector<int> castable_spells(GoldBoxGame game)
{
    std::vector<int> numbers;
    if (game == GoldBoxGame::dark_queen_of_krynn)
    {
        for (const auto &row : dark_queen_data::spells)
        {
            if (row.canonical && row.cast_class != 255 && row.cast_class <= 3 &&
                std::ranges::find(numbers, row.canonical) == numbers.end())
            {
                numbers.push_back(row.canonical);
            }
        }
        return numbers;
    }
    if (game == GoldBoxGame::death_knights_of_krynn)
    {
        for (const auto &row : death_knights_data::spells)
        {
            if (const auto *known = spell(row.canonical);
                known && known->caster_class != SpellClass::item)
            {
                numbers.push_back(row.canonical);
            }
        }
        return numbers;
    }
    if (game == GoldBoxGame::secret_of_the_silver_blades || darkness_table(game))
    {
        // Secret's table in its order: Curse's spells, with its own where
        // they took their numbers (Heal for Animate Dead, Harm for
        // Restoration) and after them; the 256-color engine's (Pools of
        // Darkness's and Treasures', the same table) goes on to 126.
        const int table_size = darkness_table(game) ? 126 : 117;
        for (int number = 1; number <= table_size; ++number)
        {
            // Secret's rows 101 to 109 cast nothing; Pools of Darkness's
            // new spells took them.
            const bool secret_blank = !darkness_table(game) && number >= 101 && number <= 109;
            if (secret_blank)
            {
                continue;
            }
            const int canonical = canonical_spell(number, game);
            const auto *known = spell(canonical);
            if (known && known->caster_class != SpellClass::item)
            {
                numbers.push_back(canonical);
            }
        }
        return numbers;
    }
    for (int number = 1; number <= 56; ++number)
    {
        numbers.push_back(number);
    }
    if (game != GoldBoxGame::pool_of_radiance)
    {
        numbers.insert(numbers.end(), curse_castable.begin(), curse_castable.end());
    }
    if (game == GoldBoxGame::champions_of_krynn)
    {
        std::erase(numbers, 36); // Champions' Animate Dead has no implementation in the original.
        std::erase(numbers, 190);
        for (int gift = 501; gift <= 507; ++gift)
        {
            numbers.push_back(gift);
        }
    }
    return numbers;
}

std::string spell_id(int number)
{
    const auto *known = spell(number);
    if (!known || known->caster_class == SpellClass::item)
    {
        return {};
    }
    std::string id = number >= 501 && number <= 507              ? "deity:"
                     : known->caster_class == SpellClass::cleric ? "cleric:"
                     : known->caster_class == SpellClass::druid  ? "druid:"
                                                                 : "magic_user:";
    bool gap = false;
    for (const char c : known->name)
    {
        if (std::isalnum(static_cast<unsigned char>(c)))
        {
            if (gap && id.back() != ':')
            {
                id += '_';
            }
            id += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            gap = false;
        }
        else
        {
            gap = gap || c == ' ' || c == ',';
        }
    }
    return id;
}

namespace
{
using SpellIndex = std::map<std::string, int, std::less<>>;

void index_spell(SpellIndex &index, int number)
{
    auto id = spell_id(number);
    if (!id.empty())
    {
        // Duplicate names keep the first match in the original search order.
        index.try_emplace(std::move(id), number);
    }
}

const SpellIndex &generic_spell_index()
{
    static const auto index = []
    {
        SpellIndex result;
        for (int number = 1; number <= static_cast<int>(spells.size()); ++number)
        {
            index_spell(result, number);
        }
        for (const auto &entry : curse_spells)
        {
            index_spell(result, entry.number);
        }
        for (const auto &entry : secret_spells)
        {
            index_spell(result, entry.number);
        }
        for (const auto &entry : darkness_spells)
        {
            index_spell(result, entry.number);
        }
        for (int gift = 501; gift <= 507; ++gift)
        {
            index_spell(result, gift);
        }
        return result;
    }();
    return index;
}

const auto &game_spell_indexes()
{
    static const auto indexes = []
    {
        std::array<SpellIndex, gold_box_games.size()> result;
        for (std::size_t at = 0; at < gold_box_games.size(); ++at)
        {
            for (const int number : castable_spells(gold_box_games[at]))
            {
                index_spell(result[at], number);
            }
        }
        return result;
    }();
    return indexes;
}
} // namespace

std::optional<int> spell_number(std::string_view id)
{
    id = id.substr(0, id.find('#'));
    if (id.empty())
    {
        return std::nullopt;
    }
    const auto &index = generic_spell_index();
    const auto found = index.find(id);
    return found == index.end() ? std::nullopt : std::optional<int>{found->second};
}

std::optional<int> spell_number(std::string_view id, GoldBoxGame game)
{
    id = id.substr(0, id.find('#'));
    const auto found_game = std::ranges::find(gold_box_games, game);
    if (found_game != gold_box_games.end())
    {
        const auto &index =
            game_spell_indexes()[static_cast<std::size_t>(found_game - gold_box_games.begin())];
        if (const auto found = index.find(id); found != index.end())
        {
            return found->second;
        }
    }
    else
    {
        // Preserve the existing fallback for enum values no installed game uses.
        for (const int number : castable_spells(game))
        {
            if (spell_id(number) == id)
            {
                return number;
            }
        }
    }
    return spell_number(id);
}

const Spell *spell(int number) noexcept
{
    static constexpr Spell native_youth{
        "Elixir of Youth", SpellClass::item, 6, 0, 0, 0, 0, 0, 1, 0, 4, 0, 2, 0, 0, false, 0};
    if (number == native_elixir_youth)
    {
        return &native_youth;
    }
    // Champions' deity gifts share a separate level-zero preparation allowance.
    // Their caster level is the cleric's; IDs stay distinct from ordinary spells.
    static constexpr std::array<Spell, 7> gifts{
        {{"Protection From Evil 10' Radius", SpellClass::cleric, 0, 0, 0, 0, 2, 4, 2, 0, 4, 45, 2,
          3, 1, false, 0},
         {"Silence 15' Radius", SpellClass::cleric, 0, 12, 0, 0, 2, 31, 0, 1, 4, 21, 1, 5, 2, true,
          1},
         {"Detect Magic", SpellClass::cleric, 0, 0, 0, 10, 0, 0, 1, 0, 4, 5, 2, 0, 1, false, 0},
         {"Remove Curse", SpellClass::cleric, 0, 0, 0, 0, 0, 4, 2, 0, 4, 2, 2, 6, 0, false, 0},
         {"Bless", SpellClass::cleric, 0, 6, 0, 6, 0, 10, 4, 0, 4, 1, 2, 10, 1, false, 0},
         {"Charm Person", SpellClass::cleric, 0, 12, 0, 0, 0, 4, 0, 1, 4, 11, 1, 1, 4, true, 0},
         {"Burning Hands", SpellClass::cleric, 0, 0, 0, 0, 0, 4, 1, 0, 4, 0, 1, 1, 3, true, 0}}};
    if (number >= 501 && number <= 507)
    {
        return &gifts[static_cast<std::size_t>(number - 501)];
    }
    if (number >= 1 && number <= static_cast<int>(spells.size()))
    {
        return &spells[static_cast<std::size_t>(number - 1)];
    }
    for (const auto &entry : curse_spells)
    {
        if (entry.number == number)
        {
            return &entry.spell;
        }
    }
    for (const auto &entry : secret_spells)
    {
        if (entry.number == number)
        {
            return &entry.spell;
        }
    }
    for (const auto &entry : darkness_spells)
    {
        if (entry.number == number)
        {
            return &entry.spell;
        }
    }
    return nullptr;
}

const Spell *spell(int number, GoldBoxGame game) noexcept
{
    if (game == GoldBoxGame::dark_queen_of_krynn)
    {
        static const auto changed = []
        {
            std::array<NumberedSpell, dark_queen_data::spells.size()> rows{};
            for (std::size_t at = 0; at < rows.size(); ++at)
            {
                const auto &native = dark_queen_data::spells[at];
                const auto *base = spell(native.canonical);
                if (!base)
                {
                    continue;
                }
                auto value = *base;
                value.level = native.cast_class == 3 ? 0 : native.level;
                value.range_fixed = native.range;
                value.range_per_level = native.range_per_level;
                value.minutes_fixed = native.duration;
                value.minutes_per_level = native.duration_per_level;
                value.area =
                    (native.targeting & 15) == 15 ? native.targeting : native.targeting & 15;
                value.camp_target = native.camp_target == 3 ? 4 : native.camp_target;
                value.save_result = native.save_effect;
                value.save_type = native.save_type;
                value.when = native.combat_ok;
                value.casting_time = native.casting_time;
                value.ai_priority = native.ai_priority;
                value.offensive = native.hostile != 0;
                if (native.canonical == native_elixir_youth)
                {
                    // The native handler always changes its user, as Youth
                    // does in the earlier games, regardless of the target bits.
                    value.area = 0;
                    value.camp_target = 1;
                }
                rows[at] = {native.canonical, value};
            }
            return rows;
        }();
        if (number > 0)
        {
            for (const auto &row : changed)
            {
                if (row.number == number)
                {
                    return &row.spell;
                }
            }
        }
        return nullptr;
    }
    if (game == GoldBoxGame::death_knights_of_krynn)
    {
        static const auto changed = []
        {
            std::array<NumberedSpell, death_knights_data::spells.size()> rows{};
            for (std::size_t at = 0; at < rows.size(); ++at)
            {
                const auto &native = death_knights_data::spells[at];
                const auto *base = spell(native.canonical);
                if (!base)
                {
                    continue;
                }
                auto value = *base;
                // Deity gifts use the existing separate level-zero allowance.
                value.level = native.castClass == 2 ? 0 : native.level;
                value.range_fixed = native.range;
                value.range_per_level = native.rangePerLevel;
                value.minutes_fixed = native.duration;
                value.minutes_per_level = native.durationPerLevel;
                value.area = native.targeting & 15;
                value.camp_target = native.campTarget == 3 ? 4 : native.campTarget;
                value.save_result = native.saveEffect;
                value.save_type = native.saveType;
                value.when = native.combatOk;
                value.casting_time = native.castingTime;
                value.ai_priority = native.aiPriority;
                value.offensive = native.hostile != 0;
                rows[at] = {native.canonical, value};
            }
            return rows;
        }();
        if (number > 0)
        {
            for (const auto &entry : changed)
            {
                if (entry.number == number)
                {
                    return &entry.spell;
                }
            }
        }
        return nullptr;
    }
    if (vga_engine(game))
    {
        static const auto changed = []
        {
            std::array<NumberedSpell, treasures_changes.size()> rows{};
            for (std::size_t n = 0; n < rows.size(); ++n)
            {
                const auto &change = treasures_changes[n];
                auto row = *spell(change.number);
                row.range_fixed = change.range_fixed;
                row.range_per_level = change.range_per_level;
                row.minutes_fixed = change.minutes_fixed;
                row.minutes_per_level = change.minutes_per_level;
                row.save_result = change.save_result;
                row.when = change.when;
                row.ai_priority = change.ai_priority;
                rows[n] = {change.number, row};
            }
            return rows;
        }();
        for (const auto &entry : changed)
        {
            if (entry.number == number)
            {
                return &entry.spell;
            }
        }
    }
    // Secret's, the 256-color engine's and the kit's Lightning Bolt reaches 4
    // squares a level, not 1 (their spell tables, row 51; the kit's
    // spelltab.c).
    constexpr int lightning_bolt = 51;
    if (number == lightning_bolt &&
        (game == GoldBoxGame::secret_of_the_silver_blades || darkness_table(game)))
    {
        static const Spell far_bolt = []
        {
            auto bolt = spells[lightning_bolt - 1];
            bolt.range_per_level = 4;
            return bolt;
        }();
        return &far_bolt;
    }
    if (game == GoldBoxGame::gateway_to_the_savage_frontier)
    {
        for (const auto &entry : gateway_rows)
        {
            if (entry.number == number)
            {
                return &entry.spell;
            }
        }
    }
    return spell(number);
}

} // namespace opengold::original
