#include "dark_queen_rules.h"
#include "dark_queen_effects.h"
#include "opengold/original_combat.h"
#include "opengold/dark_queen_records.h"
#include "dark_queen_tables.h"
#include "dark_queen_spell_table.h"
#include "champions_rules.h"
#include "opengold/dice.h"

#include <algorithm>
#include <stdexcept>

namespace opengold::original
{
int dark_queen_green_fire_damage(int damage, std::vector<Effect> &effects, RandomState &random)
{
    const auto has = [&](int code)
    {
        return std::ranges::find(effects, code, &Effect::code) != effects.end();
    };
    if (has(20))
    {
        damage /= 2;
    }
    if (has(250))
    {
        damage *= 2;
    }
    if (has(254))
    {
        damage /= 2;
    }
    if (has(661) || has(479) || has(112))
    {
        damage = 0;
    }
    if (auto fire = std::ranges::find(effects, 519, &Effect::code); fire != effects.end())
    {
        const int left = std::max(fire->strength, 0) * 12;
        const int taken = std::min(damage, left);
        damage -= taken;
        fire->strength = (left - taken) / 12;
        if (fire->strength == 0)
        {
            effects.erase(fire);
        }
    }
    if (has(282))
    {
        (void)roll_die(random, 20);
        damage /= 2;
    }
    if (has(281))
    {
        damage /= 2;
    }
    if (has(1776))
    {
        damage = 0;
    }
    if (has(dark_queen_effect::resists_fire))
    {
        damage = damage / 5 * 4;
    }
    return std::max(damage, 0);
}

namespace
{
using C = ClassSlot;
using P = Profession;
namespace data = dark_queen_data;

std::size_t native_profession(P profession)
{
    return profession == P::knight ? 1 : static_cast<std::size_t>(profession);
}

std::size_t native_slot(C slot)
{
    return slot == C::monk ? 1 : static_cast<std::size_t>(slot);
}
} // namespace

int dark_queen_import_spell(int canonical) noexcept
{
    const int number = dark_queen::import_spell_number(
        native_spell(canonical, GoldBoxGame::death_knights_of_krynn));
    return canonical_spell(number, GoldBoxGame::dark_queen_of_krynn);
}

bool dark_queen_scroll_spell(int native, unsigned robe) noexcept
{
    if (native < 1 || native > 126 || robe < 1 || robe > 2)
    {
        return false;
    }
    const auto index = static_cast<std::size_t>(native - 1);
    const auto kinds = static_cast<unsigned>(data::scroll_spell_kinds[index]);
    return data::spells[index].cast_class == 2 && (kinds == 3 || kinds == robe);
}

AbilityLimits dark_queen_ability_limits(Race race, Gender gender)
{
    const auto base = krynn_race_index(race) * 16;
    const auto sex = gender == Gender::female ? 1U : 0U;
    const auto &r = data::race_limits;
    return {{r[base + sex], r[base + 10], r[base + 12], r[base + 6], r[base + 8], r[base + 14]},
            {r[base + 2 + sex], r[base + 11], r[base + 13], r[base + 7], r[base + 9], r[base + 15]},
            r[base + 4 + sex]};
}

std::vector<StartingItem> dark_queen_starting_items(Race race, Profession profession)
{
    const auto first = (krynn_race_index(race) * 17 + native_profession(profession)) * 5;
    std::vector<StartingItem> items;
    for (std::size_t slot = 0; slot < 5; ++slot)
    {
        const auto number = static_cast<unsigned>(data::start_items.at(first + slot));
        // Native arrows/quarrels are omitted; the forty darts remain physical
        // thrown weapons. These are item definition IDs, not item types.
        if (!number || number == 80 || number == 91)
        {
            continue;
        }
        const bool readied =
            number != 82 && number != 83 && number != 84 && number != 86 && number != 93;
        items.push_back({number, number == 86 ? std::optional<int>{40} : std::nullopt, readied});
    }
    return items;
}

rules::AbilityArray<int> dark_queen_class_minimums(P profession)
{
    const auto base = native_profession(profession) * 6;
    const auto &r = data::class_mins;
    return {r.at(base),     r.at(base + 3), r.at(base + 4),
            r.at(base + 1), r.at(base + 2), r.at(base + 5)};
}

std::span<const P> dark_queen_professions(Race race)
{
    if (!champions_race(race))
    {
        return {};
    }
    static const auto lists = []
    {
        std::array<std::vector<P>, 7> choices;
        for (std::size_t race = 0; race < choices.size(); ++race)
        {
            const auto base = race * 15;
            for (int at = 1; at <= data::race_classes[base]; ++at)
            {
                const int id = data::race_classes[base + at];
                choices[race].push_back(id == 1 ? P::knight : static_cast<P>(id));
            }
        }
        return choices;
    }();
    return lists[krynn_race_index(race)];
}

bool dark_queen_alignment_allowed(P profession, Alignment alignment)
{
    const auto base = native_profession(profession) * 11;
    for (int at = 1; at <= data::class_aligns.at(base); ++at)
    {
        if (data::class_aligns.at(base + at) == static_cast<int>(alignment))
        {
            return true;
        }
    }
    return false;
}

int dark_queen_level_limit(Race race, C slot, KrynnOrders orders)
{
    if (slot == C::monk && orders.knight >= 2)
    {
        return orders.knight == 3 ? 40 : 18;
    }
    return slot == C::druid ? 0
                            : data::max_level.at(krynn_race_index(race) * 7 + native_slot(slot));
}

std::optional<int> dark_queen_experience_for_level(C slot, int level, KrynnOrders orders)
{
    const auto row = krynn_progression(slot, orders);
    if (!row || level < 1 || level > 40)
    {
        return std::nullopt;
    }
    const auto base = *row * 19;
    return level < 18
               ? data::xp_by_level[base + level]
               : data::xp_by_level[base + 17] + (level - 17) * data::high_level_xp_step[*row];
}

int dark_queen_starting_age(Race race, P profession, RandomState &random)
{
    const auto id = native_profession(profession);
    const bool single = id <= 6;
    const auto row = single ? id : id == 14 ? 2U : id == 13 || id == 15 || id == 16 ? 5U : 0U;
    const auto at = (krynn_race_index(race) * 7 + row) * 3;
    int age = data::start_ages[at] + (single ? 10 : 0);
    for (int die = 0; die < data::start_ages[at + 1]; ++die)
    {
        age += single ? roll_die(random, data::start_ages[at + 2]) : data::start_ages[at + 2];
    }
    return age;
}

KrynnSpellRows dark_queen_base_slots(const ClassLevels &levels, int wisdom, int intelligence,
                                     KrynnOrders orders)
{
    KrynnSpellRows slots{};
    const auto add =
        [&](std::size_t table, int held, std::size_t source, std::size_t target, std::size_t count)
    {
        if (held > 0)
        {
            const auto at = (table * 29 + static_cast<std::size_t>(std::min(held, 29) - 1)) * 9;
            for (std::size_t column = 0; column < count; ++column)
            {
                slots[target][column] += data::spell_slots_by_level[at + source + column];
            }
        }
    };
    if (level_in(levels, C::cleric) > 0)
    {
        add(orders.deity > 4 ? 1 : 0, level_in(levels, C::cleric), 0, 0, 7);
        static constexpr std::array<std::size_t, 6> bonus_levels{0, 0, 1, 1, 2, 3};
        for (std::size_t bonus = 0; bonus < bonus_levels.size(); ++bonus)
        {
            if (wisdom > static_cast<int>(12 + bonus) && slots[0][bonus_levels[bonus]] > 0)
            {
                ++slots[0][bonus_levels[bonus]];
            }
        }
        if (wisdom < 17)
        {
            slots[0][5] = 0;
        }
        if (wisdom < 18)
        {
            slots[0][6] = 0;
        }
    }
    if (level_in(levels, C::monk) > 5 && orders.knight > 1)
    {
        add(orders.knight == 2 ? 2 : 3, level_in(levels, C::monk), 0, 0, 7);
    }
    if (level_in(levels, C::paladin) > 8)
    {
        add(7, level_in(levels, C::paladin), 0, 0, 4);
    }
    if (level_in(levels, C::ranger) > 7)
    {
        add(4, level_in(levels, C::ranger), 0, 1, 4);
        add(4, level_in(levels, C::ranger), 4, 3, 4);
    }
    if (level_in(levels, C::magic_user) > 0)
    {
        add(orders.robe == 1 ? 5 : 6, level_in(levels, C::magic_user), 0, 3, 9);
        // Native recalc_spells keeps an extra first-level mage spell.
        ++slots[3][0];
        static constexpr std::array<int, 4> minimum_intelligence{12, 14, 16, 18};
        for (std::size_t high = 0; high < minimum_intelligence.size(); ++high)
        {
            if (intelligence < minimum_intelligence[high])
            {
                slots[3][5 + high] = 0;
            }
        }
    }
    return slots;
}

int dark_queen_caster_level(const ClassLevels &levels, SpellClass caster, int robe, int phase)
{
    if (caster == SpellClass::item)
    {
        return 12;
    }
    const int cleric = level_in(levels, C::cleric), mage = level_in(levels, C::magic_user);
    const int knight = level_in(levels, C::monk), ranger = level_in(levels, C::ranger);
    if (cleric == 0 && mage == 0 && knight < 9 && ranger < 8)
    {
        return 6;
    }
    if (caster == SpellClass::cleric)
    {
        return std::max(cleric, knight);
    }
    if (caster == SpellClass::druid)
    {
        return std::max(ranger - 7, 0);
    }
    const int moon = mage > 0 && robe >= 1 && robe <= 3 ? phase == 0               ? -1
                                                          : phase == 2 && mage > 5 ? 1
                                                                                   : 0
                                                        : 0;
    return std::max(mage + moon, ranger - 8);
}

namespace
{
int hit_point_bound(const ModifiedCharacter &character, int constitution, bool maximum)
{
    static constexpr std::array<int, 8> caps{10, 0, 10, 10, 11, 12, 11, 10};
    int total = 0, bonus = 0, count = 0;
    for (std::size_t at = 0; at < character.levels.size(); ++at)
    {
        const int level = character.levels[at];
        if (level <= 0 || at == static_cast<std::size_t>(C::druid))
        {
            continue;
        }
        ++count;
        const auto slot = static_cast<C>(at);
        const int con = constitution_hit_point_bonus(static_cast<P>(slot), constitution,
                                                     GoldBoxGame::dark_queen_of_krynn);
        if (level < caps[at])
        {
            if (maximum)
            {
                const int dice =
                    level +
                    (slot == C::monk || (slot == C::cleric && character.orders.deity <= 4) ? 1 : 0);
                const int sides = slot == C::magic_user                   ? 4
                                  : slot == C::thief                      ? 6
                                  : slot == C::paladin || slot == C::monk ? 10
                                                                          : 8;
                total += dice * (sides + con) + (slot == C::ranger ? 1 : 0);
            }
            else
            {
                total += level + (slot == C::ranger || slot == C::monk ? 1 : 0);
                bonus += con * level;
            }
            continue;
        }
        const int over = level - caps[at] + 1;
        if (maximum)
        {
            switch (slot)
            {
            case C::cleric:
                total += 9 * con + 72 + over * 2;
                break;
            case C::fighter:
            case C::paladin:
                total += 9 * con + 90 + over * 3;
                break;
            case C::monk:
                total += 10 * con + 100 + over * 2;
                break;
            case C::ranger:
                total += 11 * con + 88 + over * 2;
                break;
            case C::magic_user:
                total += 11 * con + 44 + over;
                break;
            case C::thief:
                total += 10 * con + 60 + over * 2;
                break;
            case C::druid:
                break;
            }
        }
        else
        {
            // Native Modify's high-level knight branch contributes only Constitution.
            const int fixed = slot == C::fighter || slot == C::paladin ? 3
                              : slot == C::magic_user                  ? 1
                                                                       : 2;
            if (slot != C::monk)
            {
                total += caps[at] - 1 + over * fixed;
            }
            bonus += con * (caps[at] - 1);
        }
    }
    return count ? std::clamp((total + (maximum ? 0 : std::max(bonus, 0))) / count, 1, 255) : 1;
}
} // namespace

int dark_queen_hit_point_maximum(const ModifiedCharacter &character, int constitution)
{
    return hit_point_bound(character, constitution, true);
}

int dark_queen_hit_point_minimum(const ModifiedCharacter &character, int constitution)
{
    return hit_point_bound(character, constitution, false);
}

int dark_queen_thac0(const ClassLevels &levels)
{
    int best = 40;
    for (std::size_t at = 0; at < levels.size(); ++at)
    {
        const auto slot = static_cast<C>(at);
        if (levels[at] > 0 && slot != C::druid)
        {
            best = std::max(
                best,
                data::attack_scores_by_level[native_slot(slot) * 22 + std::min(levels[at], 21)]);
        }
    }
    return 60 - best;
}

namespace
{

// The steps by which Constitution raises the number a character needs
// against paralysis, poison and death (herostat.c, recalc_saves): a dwarf's
// or kender's at 4, 7, 11, 14 and 18, and anyone's above 18.
int constitution_save_raise(Race race, int constitution)
{
    const bool hardy_race =
        race == Race::mountain_dwarf || race == Race::hill_dwarf || race == Race::kender;
    int raise = 0;
    if (hardy_race && constitution >= 4 && constitution <= 18)
    {
        raise += constitution <= 6    ? 1
                 : constitution <= 10 ? 2
                 : constitution <= 13 ? 3
                 : constitution <= 17 ? 4
                                      : 5;
    }
    if (constitution >= 19 && constitution <= 25)
    {
        raise += (constitution - 17) / 2;
    }
    return raise;
}

} // namespace

// Each class's table, the best of them kept, in the original's class order;
// after each class the save against paralysis, poison and death is raised
// by Constitution (constitution_save_raise). The shipped code raises the
// number needed, so it is a penalty, not the bonus the tabletop rules give.
// An equipped item of special 6 raises it as for a dwarf too; no Dark
// Queen item OpenGoldBox reads has that special.
std::array<int, 5> dark_queen_saving_throws(const ClassLevels &levels, Race race, int constitution)
{
    // Cleric, knight, fighter, paladin, ranger, mage, thief.
    static constexpr std::array native_order{C::cleric, C::monk,       C::fighter, C::paladin,
                                             C::ranger, C::magic_user, C::thief};
    const int raise = constitution_save_raise(race, constitution);
    std::array<int, 5> saves{20, 20, 20, 20, 20};
    for (const auto slot : native_order)
    {
        const int level = levels[static_cast<std::size_t>(slot)];
        if (level <= 0)
        {
            continue;
        }
        const auto base = (native_slot(slot) * 22 + std::min(level, 21)) * 5;
        for (std::size_t save = 0; save < saves.size(); ++save)
        {
            saves[save] = std::min(saves[save], data::saving_throws[base + save]);
        }
        saves[0] += raise;
    }
    return saves;
}

void recalculate_dark_queen_numbers(rules::CharacterSheet::Original &numbers,
                                    const rules::AbilityArray<int> &scores)
{
    constexpr auto game = GoldBoxGame::dark_queen_of_krynn;
    death_knights_calendar(numbers, numbers.krynn_calendar_days);
    const auto slots = dark_queen_base_slots(
        numbers.class_levels, scores[rules::Ability::wisdom], scores[rules::Ability::intelligence],
        {numbers.deity, numbers.robe_order, numbers.knight_order});
    numbers.cleric_slots = slots[0];
    numbers.druid_slots = slots[1];
    numbers.magic_user_slots = slots[3];
    for (int level = 0; level < 7; ++level)
    {
        if (numbers.cleric_slots[level] > 0)
        {
            for (int at = 0; at < 10; ++at)
            {
                learn_known_spell(numbers,
                                  canonical_spell(data::spells_by_level[level * 10 + at], game));
            }
        }
    }
    learn_druid_spells(numbers, game);
    learn_deity_gifts(numbers);
    numbers.thac0 = dark_queen_thac0(numbers.class_levels);
    favor_thac0_by_deity(numbers);
    numbers.saving_throws =
        dark_queen_saving_throws(numbers.class_levels, static_cast<Race>(numbers.race),
                                 scores[rules::Ability::constitution]);
}

std::vector<int> dark_queen_learning_spells(int robe, int highest)
{
    std::vector<int> result;
    const auto base = static_cast<std::size_t>(robe == 1 ? 1 : 2) * 90;
    for (int level = 0; level < std::clamp(highest, 0, 9); ++level)
    {
        for (int at = 0; at < 10; ++at)
        {
            if (const auto own = data::spells_by_level[base + level * 10 + at])
            {
                result.push_back(canonical_spell(own, GoldBoxGame::dark_queen_of_krynn));
            }
        }
    }
    return result;
}

std::vector<int> dark_queen_starting_spells(int robe, const std::array<int, 9> &mage_slots)
{
    std::vector<int> native;
    if (mage_slots[0] == 0)
    {
        return {};
    }
    // Creation teaches every first-level mage spell; higher levels use its
    // robe-specific starting selections, with all mage spells at ninth level.
    for (int own = 1; own <= 126; ++own)
    {
        const auto *known = spell(canonical_spell(own, GoldBoxGame::dark_queen_of_krynn),
                                  GoldBoxGame::dark_queen_of_krynn);
        if (known && data::spells[static_cast<std::size_t>(own - 1)].cast_class == 2 &&
            (known->level == 1 || mage_slots[8] > 0))
        {
            native.push_back(own);
        }
    }
    static constexpr std::array<std::array<int, 7>, 7> white{{{29, 33, 34},
                                                              {46, 47, 49, 52, 54},
                                                              {82, 88, 81, 85, 87},
                                                              {91, 92},
                                                              {112},
                                                              {115, 117},
                                                              {121}}};
    static constexpr std::array<std::array<int, 7>, 7> red{{{30, 31, 32, 34, 35},
                                                            {45, 47, 48, 50, 51, 55},
                                                            {84, 83, 85, 87},
                                                            {92, 118, 119},
                                                            {113, 114, 111},
                                                            {117},
                                                            {123}}};
    const auto &selections = robe == 1 ? white : red;
    for (std::size_t at = 0; at < selections.size(); ++at)
    {
        if (mage_slots[at + 1] > 0)
        {
            for (const int own : selections[at])
            {
                if (own && std::ranges::find(native, own) == native.end())
                {
                    native.push_back(own);
                }
            }
        }
    }
    std::vector<int> result;
    for (const int own : native)
    {
        result.push_back(canonical_spell(own, GoldBoxGame::dark_queen_of_krynn));
    }
    return result;
}
} // namespace opengold::original
