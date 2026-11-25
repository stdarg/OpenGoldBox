#ifndef OPENGOLD_ORIGINAL_CAMP_MAGIC_H
#define OPENGOLD_ORIGINAL_CAMP_MAGIC_H
#include "opengold/original_combat.h"
#include "opengold/original_spells.h"
#include "opengold/random_state.h"
#include "opengold/rules.h"

#include <array>
#include <functional>
#include <span>
#include <string_view>
#include <vector>

// The original rules' magic outside a fight, which the module's camp actions,
// potions, temple services and passing time share: the spells cast in camp,
// the conditions a member carries and their cures, a camp caster's level and
// what a casting spends (camp_magic.cpp).
namespace opengold::original
{

constexpr int cure_light_wounds = 3, cure_serious_wounds = 158, cure_critical_wounds = 171,
              raise_dead = 175, heal = 236, barkskin = 290, druid_cure_light_wounds = 298,
              cure_blindness = 37, cure_disease = 39, remove_curse = 43,
              magic_user_remove_curse = 189, neutralize_poison = 167, enlarge = 12, reduce = 13,
              strength_spell = 35, slow_poison = 26, resurrection = 305, druid_cure_disease = 306,
              druid_neutralize_poison = 307, restoration = 302, stone_to_flesh = 313;
// The spells a member may cast in camp: the cures, Curse's Raise Dead and
// Neutralize Poison, Secret's Heal and Barkskin, which its table casts as
// Heal, Slow Poison, the Strength spells, whose effects last into the next
// fight, Pools of Darkness's Resurrection, Restoration and its ranger's
// Cure Disease and Neutralize Poison, and Stone to Flesh, which the later
// games' table lets be cast out of a fight.
constexpr std::array<int, 22> camp_spells{cure_light_wounds,
                                          cure_serious_wounds,
                                          cure_critical_wounds,
                                          raise_dead,
                                          heal,
                                          barkskin,
                                          druid_cure_light_wounds,
                                          cure_blindness,
                                          cure_disease,
                                          remove_curse,
                                          504,
                                          magic_user_remove_curse,
                                          neutralize_poison,
                                          enlarge,
                                          reduce,
                                          strength_spell,
                                          slow_poison,
                                          resurrection,
                                          restoration,
                                          druid_cure_disease,
                                          druid_neutralize_poison,
                                          stone_to_flesh};
constexpr std::string_view cast_prefix = "cast:";
// A paladin's laying on of hands, once between a day's rests; the member's
// state keeps that it was used (as the paladinDailyHealCast effect).
constexpr std::string_view lay_on_hands = "paladin:lay_on_hands";
constexpr int laid_on_hands = 340;
// A paladin's cure of disease, once a week and once more for each five
// levels past the first (Curse's viewchar.pas, PaladinCureDisease); the
// week's cures are counted in the strength of a week-long effect.
constexpr std::string_view cure_disease_action = "paladin:cure_disease";
constexpr int cured_this_week = 341, week_minutes = 7 * 24 * 60;

// The conditions, by the fight's numbers (lasting_condition).
constexpr int blind = 33, bestow_curse = 36, poisoned = 55, slow_poison_hit = 15,
              slow_poison_end = 22, feebleminded = 268;
constexpr std::array<int, 3> poison_codes{poisoned, slow_poison_hit, slow_poison_end};
// A revival that costs nothing, from poison (Secret's and Gateway's temples,
// Curse's Neutralize Poison) or by Pools of Darkness's Resurrection: the
// state keeps it until the raising's cost is waived (raise_character). No
// game's effect has its number.
constexpr int revived_from_poison = 600;

// The spells cast in camp for their effects, which carry into the next fight
// (lasting_condition); the party is camp target 4.
constexpr int protection_from_fire = 309, mind_blank = 322;
constexpr std::string_view hot_fire_shield = "cast:magic_user:fire_shield:hot",
                           cold_fire_shield = "cast:magic_user:fire_shield:cold";
constexpr std::array<int, 29> camp_effect_spells{1,   6,   7,   8,   16,  17,  18,  19,  24, 29,
                                                 30,  32,  42,  45,  48,  50,  52,  53,  54, 169,
                                                 188, 309, 312, 316, 318, 319, 322, 501, 505};
constexpr int party_target = 4;

[[nodiscard]] std::vector<int> disease_codes(GoldBoxGame game);
[[nodiscard]] std::vector<int> heal_cures(GoldBoxGame game);
[[nodiscard]] std::vector<int> spells_left(const rules::CharacterSheet &sheet,
                                           const rules::VitalState &state);
[[nodiscard]] std::vector<Effect> conditions_of(const rules::CharacterSheet &sheet,
                                                const rules::VitalState &state);
[[nodiscard]] const Effect *condition(const std::vector<Effect> &conditions, int code);
[[nodiscard]] bool has_any(const rules::CharacterSheet &sheet, const rules::VitalState &state,
                           std::span<const int> codes);
void waste_away(const rules::CharacterSheet &sheet, rules::VitalState &state,
                std::vector<Effect> &conditions, int hours, int ten_minutes);
void change_conditions(const rules::CharacterSheet &sheet, rules::VitalState &state,
                       const std::function<void(std::vector<Effect> &)> &change);
bool cure(const rules::CharacterSheet &sheet, rules::VitalState &state, std::span<const int> codes);
void stand_unpoisoned(const rules::CharacterSheet &sheet, rules::VitalState &state);
[[nodiscard]] int camp_caster_level(const rules::CharacterSheet &caster, const Spell &known);
void hold_poison_back(const rules::CharacterSheet &caster, const rules::CharacterSheet &target,
                      rules::VitalState &state);
void strengthen_in_camp(const rules::CharacterSheet &caster, const rules::CharacterSheet &target,
                        rules::VitalState &state, int number, RandomState &random_state);
[[nodiscard]] bool sight_spell(int number);
void lay_camp_effect(const rules::CharacterSheet &caster, const rules::CharacterSheet &target,
                     rules::VitalState &state, int number, RandomState &random_state);
[[nodiscard]] bool may_lay_on_hands(const rules::CharacterSheet &sheet,
                                    const rules::VitalState &state);
[[nodiscard]] int cures_left(const rules::CharacterSheet &sheet, const rules::VitalState &state);
void spend_spell(const rules::CharacterSheet &user, rules::VitalState &state, int number);
} // namespace opengold::original
#endif
