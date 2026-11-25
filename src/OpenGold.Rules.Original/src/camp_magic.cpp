#include "camp_magic.h"

#include "dark_queen_effects.h"
#include "dark_queen_rules.h"
#include "death_knights_rules.h"
#include "opengold/dice.h"
#include "opengold/original.h"
#include "rules_module.h"

#include <algorithm>
#include <iterator>
#include <ranges>

namespace opengold::original
{

using namespace rules;

// Disease as its cures take it away (temple.pas, cureDiseaseEffects): the
// helplessness it brings, disease and its losses of Strength and Hit Points,
// and in Pool of Radiance rot and mummy rot.
std::vector<int> disease_codes(GoldBoxGame game)
{
    // And the Strength it wasted, which comes back (Secret's
    // RemoveDiseaseEffects, recounting Strength).
    std::vector<int> codes{31, 34, 43, 44, strength_wasted};
    if (game == GoldBoxGame::pool_of_radiance)
    {
        codes.insert(codes.end(), {50, 57});
    }
    // The kit's cures take its rot (spellfx.c, disease_cure; temple.c,
    // cure_disease: its 215 and 172), not the Charisma it took.
    if (game == GoldBoxGame::unlimited_adventures)
    {
        codes.insert(codes.end(), {50, kit_rotting});
    }
    if (game == GoldBoxGame::dark_queen_of_krynn)
    {
        codes.insert(codes.end(), {dark_queen_effect::rot_recovery, kit_rotting,
                                   dark_queen_effect::spore_infection});
    }
    return codes;
}

// What a temple's Heal cures besides wounds: in Secret blindness, disease and
// feeblemind, in Treasures blindness and feeblemind (their temple.pas,
// CureWounds), elsewhere nothing.
std::vector<int> heal_cures(GoldBoxGame game)
{
    if (game == GoldBoxGame::secret_of_the_silver_blades)
    {
        auto cured = disease_codes(game);
        cured.insert(cured.end(), {blind, feebleminded});
        return cured;
    }
    if (game == GoldBoxGame::treasures_of_the_savage_frontier ||
        game == GoldBoxGame::death_knights_of_krynn)
    {
        return {blind, feebleminded};
    }
    return {};
}

// The spells a member has memorized and not cast: its last fight's, or after
// a day's rest its sheet's.
std::vector<int> spells_left(const CharacterSheet &sheet, const VitalState &state)
{
    if (const auto last = Combatant::decode(state.resources))
    {
        return last->spells;
    }
    std::vector<int> memorized;
    for (const auto &id : sheet.prepared_spells)
    {
        if (const auto number = spell_number(id, game_of(sheet.identity)))
        {
            memorized.push_back(*number);
        }
    }
    return memorized;
}

// The conditions a member carries out of its fights (lasting_condition).
std::vector<Effect> conditions_of(const CharacterSheet &sheet, const VitalState &state)
{
    return carried_state(state.resources, game_of(sheet.identity)).conditions;
}

const Effect *condition(const std::vector<Effect> &conditions, int code)
{
    const auto found = std::ranges::find(conditions, code, &Effect::code);
    return found == conditions.end() ? nullptr : &*found;
}

bool has_any(const CharacterSheet &sheet, const VitalState &state, std::span<const int> codes)
{
    const auto conditions = conditions_of(sheet, state);
    return std::ranges::any_of(codes,
                               [&conditions](int code)
                               {
                                   return condition(conditions, code) != nullptr;
                               });
}

// A disease that has taken hold, as hours and ten minutes pass
// (effhndlr.pas, EffDiseaseStrength and EffDiseaseHitPoints): a point of
// Strength an hour down to 3, and a Hit Point every ten minutes down to 1,
// and then the member is helpless until it is cured.
void waste_away(const CharacterSheet &sheet, VitalState &state, std::vector<Effect> &conditions,
                int hours, int ten_minutes)
{
    constexpr int wasting_strength = 43, wasting_hit_points = 44, helpless = 31;
    bool done_in = false;
    if (condition(conditions, wasting_strength))
    {
        const int own = sheet.scores[Ability::strength];
        auto wasted = std::ranges::find(conditions, strength_wasted, &Effect::code);
        if (wasted == conditions.end())
        {
            conditions.push_back({strength_wasted, 0, 0, false});
            wasted = std::prev(conditions.end());
        }
        wasted->strength = std::min(wasted->strength + hours, std::max(own - 3, 0));
        done_in = own - wasted->strength <= 3;
    }
    if (condition(conditions, wasting_hit_points) && !state.dead)
    {
        state.hit_points = std::max(std::min(state.hit_points, 1), state.hit_points - ten_minutes);
        done_in = done_in || state.hit_points <= 1;
    }
    const bool helpless_already =
        std::ranges::any_of(conditions,
                            [](const Effect &e)
                            {
                                return e.code == helpless && e.minutes == 0;
                            });
    if (done_in && !helpless_already)
    {
        conditions.push_back({helpless, 0, 255, false});
    }
}

// The member's state with its conditions changed: its last fight's numbers
// keep the change, or after a rest only its conditions do.
void change_conditions(const CharacterSheet &sheet, VitalState &state,
                       const std::function<void(std::vector<Effect> &)> &change)
{
    auto carried = carried_state(state.resources, game_of(sheet.identity));
    if (carried.last_fight)
    {
        change(carried.last_fight->effects);
        state.resources = carried.last_fight->encode();
        return;
    }
    change(carried.conditions);
    state.resources = rested_state(carried.conditions);
}

// Takes the conditions of these codes away; true when one was there.
bool cure(const CharacterSheet &sheet, VitalState &state, std::span<const int> codes)
{
    bool cured = false;
    const bool spore_removed =
        game_of(sheet.identity) == GoldBoxGame::dark_queen_of_krynn &&
        std::ranges::find(codes, dark_queen_effect::spore_infection) != codes.end() &&
        condition(conditions_of(sheet, state), dark_queen_effect::spore_infection);
    change_conditions(sheet, state,
                      [&](std::vector<Effect> &effects)
                      {
                          cured = std::erase_if(effects,
                                                [codes](const Effect &e)
                                                {
                                                    return std::ranges::find(codes, e.code) !=
                                                           codes.end();
                                                }) > 0;
                      });
    if (spore_removed)
    {
        state.dead = true;
        state.hit_points = 0;
    }
    return cured;
}

// The poisoned dead stand again with at least 1 Hit Point, their raising
// costing nothing (temple.pas, NeutralizePoison; Curse's SpellNeutralizePoison).
void stand_unpoisoned(const CharacterSheet &sheet, VitalState &state)
{
    state.dead = false;
    state.hit_points = std::max(state.hit_points, 1);
    change_conditions(sheet, state,
                      [](std::vector<Effect> &effects)
                      {
                          effects.push_back({revived_from_poison, 0, 0, false});
                      });
}

// A camp caster's level for a spell (charhlp.pas, GetCasterLevel): its cleric
// or magic-user level, or a paladin's or ranger's less 8.
int camp_caster_level(const CharacterSheet &caster, const Spell &known)
{
    // An item's own power casts at level 12 (charhlp.pas, GetCasterLevel).
    if (known.caster_class == SpellClass::item)
    {
        return 12;
    }
    if (game_of(caster.identity) == GoldBoxGame::death_knights_of_krynn ||
        game_of(caster.identity) == GoldBoxGame::dark_queen_of_krynn)
    {
        const auto &n = *caster.original;
        const int phase = n.robe_order >= 1 && n.robe_order <= 3
                              ? n.krynn_moon_phases[static_cast<std::size_t>(n.robe_order - 1)]
                              : 3;
        return game_of(caster.identity) == GoldBoxGame::dark_queen_of_krynn
                   ? dark_queen_caster_level(n.class_levels, known.caster_class, n.robe_order,
                                             phase)
                   : death_knights_caster_level(n.class_levels, known.caster_class, n.robe_order,
                                                phase);
    }
    const auto &levels = original_numbers(caster).class_levels;
    if (known.caster_class == SpellClass::druid)
    {
        return std::max(level_in(levels, ClassSlot::ranger) - 7, 0);
    }
    if (known.caster_class == SpellClass::cleric)
    {
        return std::max(level_in(levels, ClassSlot::cleric),
                        level_in(levels, ClassSlot::paladin) - 8);
    }
    return std::max(level_in(levels, ClassSlot::magic_user),
                    level_in(levels, ClassSlot::ranger) - 8);
}

// Slow Poison in camp (magic.pas, SpellSlowPoison): one poison killed stands
// with 1 Hit Point for an hour a caster level, and dies again if the poison
// outlasts it (recover).
void hold_poison_back(const CharacterSheet &caster, const CharacterSheet &target, VitalState &state)
{
    constexpr int slow_poison_number = 26;
    if (!has_any(target, state, std::array{poisoned}))
    {
        return;
    }
    const int hour_a_level = 60 * camp_caster_level(caster, *spell(slow_poison_number));
    if (state.dead)
    {
        stand_unpoisoned(target, state);
    }
    change_conditions(target, state,
                      [hour_a_level](std::vector<Effect> &effects)
                      {
                          std::erase_if(effects,
                                        [](const Effect &e)
                                        {
                                            return e.code == slow_poison_end;
                                        });
                          effects.push_back({slow_poison_end, hour_a_level, 255, true});
                      });
}

// Enlarge, Reduce and Strength cast in camp (magic.pas): their effects last
// into the next fight, which works out the Strength they give.
void strengthen_in_camp(const CharacterSheet &caster, const CharacterSheet &target,
                        VitalState &state, int number, RandomState &random_state)
{
    constexpr int enlarge_number = 12, reduce_number = 13, enlarged = 12, strength_effect = 38;
    const auto &known = *spell(number, game_of(caster.identity));
    const auto game = game_of(target.identity);
    const int level = camp_caster_level(caster, known);
    const int minutes = known.minutes_fixed + known.minutes_per_level * level;
    const auto &numbers = original_numbers(target);
    if (number == reduce_number)
    {
        // A failed save against spells loses the Enlarge (SpellReduce).
        const int save = numbers.saving_throws[4];
        if (roll_die(random_state, 20) < save)
        {
            (void)cure(target, state, std::array{enlarged});
        }
        return;
    }
    const int own = target.scores[Ability::strength];
    const int own_exceptional = numbers.exceptional_strength;
    const auto [base, exceptional] =
        number == enlarge_number
            ? enlarged_strength(level, game)
            : strengthened(own, own_exceptional, numbers.class_levels, game,
                           roll_die(random_state, strength_spell_die(numbers.class_levels, game)));
    const int code = number == enlarge_number ? enlarged : strength_effect;
    change_conditions(target, state,
                      [&](std::vector<Effect> &effects)
                      {
                          std::erase_if(effects,
                                        [code](const Effect &e)
                                        {
                                            return e.code == code;
                                        });
                          effects.push_back(
                              {code, minutes, strength_code(base, exceptional), false});
                      });
}

// The spells a player casts outside combat to see what is hidden, spent with
// no effect laid: a script may answer one (Treasures' Cortarra is seen
// through when Detect Magic is cast before her twin).
[[nodiscard]] bool sight_spell(int number)
{
    static constexpr std::array<std::string_view, 4> sight{
        "cleric:detect_magic", "magic_user:detect_magic", "druid:detect_magic",
        "deity:detect_magic"};
    return std::ranges::any_of(sight,
                               [number](std::string_view id)
                               {
                                   return spell_number(id) == number;
                               });
}

// A camp spell's effect on its target (magic.pas, ResolveSpell): its minutes
// by the caster's level, its strength the caster's level; Mirror Image's the
// 1d4 images, Prayer's the party's side (bit 4 clear) and the level.
void lay_camp_effect(const CharacterSheet &caster, const CharacterSheet &target, VitalState &state,
                     int number, RandomState &random_state)
{
    constexpr int mirror_image = 32;
    const auto &known = *spell(number, game_of(caster.identity));
    const int level = camp_caster_level(caster, known);
    int minutes = known.minutes_fixed + known.minutes_per_level * level;
    int strength = known.caster_class == SpellClass::item ? 0
                   : number == mirror_image               ? roll_die(random_state, 4)
                                                          : level;
    int code = known.effect;
    if (number == mirror_image && game_of(caster.identity) == GoldBoxGame::dark_queen_of_krynn)
    {
        strength = (strength << 5) | level;
    }
    // Mind Blank lasts a day (magic.pas, CalcSpellDuration). Protection from
    // Fire gives a ranger the druid's protection, which lasts until it has
    // taken 12 points of fire a caster level, and anyone else Resist Fire,
    // with no end (SpellProtectionFromFire).
    if (number == mind_blank)
    {
        minutes = 1440;
    }
    if (number == protection_from_fire &&
        level_in(original_numbers(target).class_levels, ClassSlot::ranger) == 0)
    {
        constexpr int resist_fire = 20;
        code = resist_fire;
        strength = 0;
    }
    change_conditions(target, state,
                      [&](std::vector<Effect> &effects)
                      {
                          std::erase_if(effects,
                                        [code](const Effect &e)
                                        {
                                            return e.code == code;
                                        });
                          effects.push_back({code, minutes, strength, false});
                      });
}

// What a paladin may still do: lay on hands today, and cure disease this week.
bool may_lay_on_hands(const CharacterSheet &sheet, const VitalState &state)
{
    return level_in(original_numbers(sheet).class_levels, ClassSlot::paladin) > 0 &&
           !condition(conditions_of(sheet, state), laid_on_hands);
}

int cures_left(const CharacterSheet &sheet, const VitalState &state)
{
    const int paladin = level_in(original_numbers(sheet).class_levels, ClassSlot::paladin);
    if (paladin == 0)
    {
        return 0;
    }
    const auto conditions = conditions_of(sheet, state);
    const auto *spent = condition(conditions, cured_this_week);
    return (paladin - 1) / 5 + 1 - (spent ? spent->strength : 0);
}

// What casting a camp spell, or a paladin's gift, spends from the member's
// state: the spell's memorized copy, or the day's or week's use.
void spend_spell(const CharacterSheet &user, VitalState &state, int number)
{
    auto left = spells_left(user, state);
    left.erase(std::ranges::find(left, number));
    auto carried = carried_state(state.resources, game_of(user.identity));
    auto numbers = carried.last_fight.value_or(character_combatant(user));
    if (!carried.last_fight)
    {
        numbers.effects.insert(numbers.effects.end(), carried.conditions.begin(),
                               carried.conditions.end());
    }
    numbers.spells = std::move(left);
    state.resources = numbers.encode();
}
} // namespace opengold::original
