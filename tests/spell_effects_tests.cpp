#include "../src/OpenGoldBox/spell_effects.h"
#include "spell_table.h"
#include <cstdlib>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>

using namespace opengold::srd5::detail;
using presentation::spell_effect_for_verb;
using presentation::spell_effects;

namespace
{
void check(bool ok, const std::string &message)
{
    if (!ok)
    {
        throw std::runtime_error(message);
    }
}

bool cast_in_combat(const SpellDef &spell)
{
    return spell.pattern != SpellPattern::camp && spell.pattern != SpellPattern::exploration;
}

void every_spell_cast_in_combat_has_its_own_look()
{
    for (const auto &spell : spell_table)
    {
        const auto *effect = spell_effect_for_verb(spell.id);
        const bool has_row = effect && effect->spell == spell.id;
        check(has_row == cast_in_combat(spell),
              "A spell's look follows whether it can be cast in combat: " + std::string(spell.id));
    }
    std::set<std::tuple<std::string_view, std::string_view, std::uint32_t, std::uint32_t>> looks;
    for (const auto &effect : spell_effects)
    {
        const auto *spell = find_spell(effect.spell);
        check(spell && cast_in_combat(*spell),
              "A look names a spell cast in combat: " + std::string(effect.spell));
        check(looks.emplace(effect.travel, effect.arrive, effect.color, effect.accent).second,
              "Two spells look the same: " + std::string(effect.spell));
        check(effect.seconds > 0.0F && effect.size > 0.0F && effect.amount > 0,
              "A look can be seen: " + std::string(effect.spell));
    }
}

// Only a ranged spell attack can fly past, and every one that flies does.
void a_spell_attack_that_misses_flies_past()
{
    for (const auto &effect : spell_effects)
    {
        const auto *spell = find_spell(effect.spell);
        const bool attack = spell->pattern == SpellPattern::spell_attack ||
                            spell->pattern == SpellPattern::repeat_attack;
        check(effect.flies_past == (attack && !effect.travel.empty()),
              "Flies past only on an attack roll: " + std::string(effect.spell));
    }
}

void a_verb_shows_as_its_spell()
{
    const auto shown = [](std::string_view verb) -> std::string_view
    {
        const auto *effect = spell_effect_for_verb(verb);
        return effect ? effect->spell : std::string_view{};
    };
    check(shown("magic_missile") == "magic_missile", "A spell shows as itself");
    check(shown("magic_missile_2") == "magic_missile", "An upcast shows as its spell");
    check(shown("shield_of_faith_scroll") == "shield_of_faith", "The longest id wins");
    check(shown("shield_2") == "shield", "Shield keeps its own look");
    check(shown("healing_word_innate_bonus") == "healing_word", "An innate cast shows");
    check(shown("heal_2") == "heal", "Heal is not Healing Word");
    check(shown("rebuke") == "hellish_rebuke", "An action of a spell shows as its spell");
    check(shown("hand_fist") == "arcane_hand", "Arcane Hand's fist shows as the spell");
    for (const char *other : {"melee", "ranged", "throw", "end", "knock_out"})
    {
        check(shown(other).empty(), "Not a spell: " + std::string(other));
    }
}
} // namespace

int main()
{
    try
    {
        every_spell_cast_in_combat_has_its_own_look();
        a_spell_attack_that_misses_flies_past();
        a_verb_shows_as_its_spell();
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "Spell effect checks passed\n";
    return EXIT_SUCCESS;
}
