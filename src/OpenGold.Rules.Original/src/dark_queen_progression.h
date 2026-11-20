#ifndef OPENGOLD_DARK_QUEEN_PROGRESSION_H
#define OPENGOLD_DARK_QUEEN_PROGRESSION_H

#include "dark_queen_rules.h"
#include "death_knights_effects.h"
#include "opengold/dice.h"
#include "opengold/original_combat.h"
#include <algorithm>
#include <array>
#include <limits>
#include <map>
#include <optional>

namespace opengold::original
{
inline constexpr std::array native_class_order{
    ClassSlot::cleric, ClassSlot::monk,       ClassSlot::fighter, ClassSlot::paladin,
    ClassSlot::ranger, ClassSlot::magic_user, ClassSlot::thief};

[[nodiscard]] inline KrynnOrders native_orders(const NativeProgression &progression)
{
    KrynnOrders orders{};
    orders.knight = progression.knight_order;
    orders.deity = progression.deity;
    orders.robe = progression.robe_order;
    return orders;
}

// Shared by PROGRAM 3 and combat. The choice retains each roll, so replay
// never rolls again and Restoration can recover the XP that was actually lost.
[[nodiscard]] inline std::optional<NativeDrainReceipt>
roll_native_drain(const NativeProgression &progression, const ClassLevels &levels, int maximum,
                  int constitution, int race, RandomState &random)
{
    std::optional<ClassSlot> chosen;
    int active = 0;
    const auto threshold = [&](ClassSlot slot, int level)
    {
        return std::max(
            0,
            dark_queen_experience_for_level(slot, level, native_orders(progression)).value_or(0));
    };
    for (const auto slot : native_class_order)
    {
        const auto at = static_cast<std::size_t>(slot);
        const int level = levels[at];
        active += level > 0 || progression.peak_class_levels[at] > 0 ? 1 : 0;
        if (level > 0 && (!chosen || level > level_in(levels, *chosen) ||
                          (level == level_in(levels, *chosen) &&
                           threshold(slot, level) < threshold(*chosen, level))))
        {
            chosen = slot;
        }
    }
    if (!chosen)
    {
        return std::nullopt;
    }
    const auto slot = *chosen;
    const int before = level_in(levels, slot);
    auto after = levels;
    --after[static_cast<std::size_t>(slot)];
    int rolled = 0;
    if (before == 2 && (slot == ClassSlot::monk || slot == ClassSlot::ranger))
    {
        const int sides = slot == ClassSlot::monk ? 10 : 8;
        const int first = roll_die(random, sides) + roll_die(random, sides);
        const int second = roll_die(random, sides) + roll_die(random, sides);
        rolled = std::max(first, second);
    }
    else if (before > 1)
    {
        const std::array trained{slot};
        rolled = roll_training_hit_dice(after, trained, GoldBoxGame::dark_queen_of_krynn, random);
    }
    const int raw_loss = std::max(1, rolled / active);
    int hp_loss = raw_loss;
    constexpr std::array<int, class_slot_count> dice_caps{10, 0, 10, 10, 11, 12, 11, 10};
    if (before > 1 && before - 1 < dice_caps[static_cast<std::size_t>(slot)])
    {
        const int dice =
            before == 2 && (slot == ClassSlot::monk || slot == ClassSlot::ranger) ? 2 : 1;
        hp_loss += constitution_hit_point_bonus(static_cast<Profession>(slot), constitution,
                                                GoldBoxGame::dark_queen_of_krynn) *
                   dice / active;
    }
    const bool no_levels = std::ranges::none_of(after,
                                                [](int level)
                                                {
                                                    return level > 0;
                                                });
    const ModifiedCharacter remaining{
        static_cast<Race>(race), progression.female ? Gender::female : Gender::male,
        static_cast<Profession>(progression.profession), after, native_orders(progression)};
    const int bound = no_levels ? 0 : dark_queen_hit_point_maximum(remaining, constitution);
    const int hp_after = std::max(0, std::min(maximum - hp_loss, bound));
    const unsigned upper = before <= 1 ? 0U : static_cast<unsigned>(threshold(slot, before));
    const unsigned lower = before <= 2 ? 0U : static_cast<unsigned>(threshold(slot, before - 1));
    return NativeDrainReceipt{static_cast<unsigned>(slot),
                              maximum - hp_after,
                              raw_loss,
                              lower + (upper - lower) / 2,
                              no_levels || hp_after == 0,
                              before,
                              std::max(progression.previous_experience, progression.experience),
                              std::max(progression.previous_max_hit_points, maximum)};
}

// The source forgets memorized copies that no longer fit after each loss.
inline void trim_native_spells(Combatant &body)
{
    if (!body.native_progression)
    {
        return;
    }
    const auto &p = *body.native_progression;
    const auto slots =
        dark_queen_base_slots(body.class_levels, p.wisdom, body.intelligence, native_orders(p));
    int moon = 0;
    for (const auto &effect : body.effects)
    {
        if (effect.code == death_knights_effect::casting_moon)
        {
            const int phase = effect.strength % 4;
            moon = phase == 1 || phase == 2 ? phase : 0;
        }
    }
    std::map<std::pair<SpellClass, int>, int> taken;
    int extra = 0;
    std::erase_if(body.spells,
                  [&](int number)
                  {
                      const auto *known = spell(number, GoldBoxGame::dark_queen_of_krynn);
                      if (!known)
                      {
                          return true;
                      }
                      int base = 0;
                      if (known->caster_class == SpellClass::cleric && known->level == 0)
                      {
                          base = level_in(body.class_levels, ClassSlot::cleric) > 0 && p.deity != 6
                                     ? (p.deity == 4  ? 3
                                        : p.deity > 0 ? 1
                                                      : 0)
                                     : 0;
                      }
                      else if (known->level >= 1 && known->level <= 9)
                      {
                          const auto row = known->caster_class == SpellClass::cleric  ? 0U
                                           : known->caster_class == SpellClass::druid ? 1U
                                                                                      : 3U;
                          base = slots[row][static_cast<std::size_t>(known->level - 1)];
                      }
                      if (++taken[{known->caster_class, known->level}] <= base)
                      {
                          return false;
                      }
                      return known->caster_class != SpellClass::magic_user || base == 0 ||
                             ++extra > moon;
                  });
}
} // namespace opengold::original
#endif
