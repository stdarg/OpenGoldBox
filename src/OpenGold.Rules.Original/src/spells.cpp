#include "opengold/original_spells.h"
#include <cctype>

#include <array>

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

} // namespace

std::string spell_id(int number)
{
    const auto *known = spell(number);
    if (!known || known->caster_class == SpellClass::item)
    {
        return {};
    }
    std::string id = known->caster_class == SpellClass::cleric ? "cleric:" : "magic_user:";
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

std::optional<int> spell_number(std::string_view id)
{
    id = id.substr(0, id.find('#'));
    for (int number = 1; number <= static_cast<int>(spells.size()); ++number)
    {
        if (!id.empty() && spell_id(number) == id)
        {
            return number;
        }
    }
    return std::nullopt;
}

const Spell *spell(int number) noexcept
{
    if (number < 1 || number > static_cast<int>(spells.size()))
    {
        return nullptr;
    }
    return &spells[static_cast<std::size_t>(number - 1)];
}

} // namespace opengold::original
