#include "death_knights_rules.h"
#include "champions_rules.h"
#include "death_knights_tables.h"
#include "opengold/original_spells.h"

#include <algorithm>
#include <stdexcept>

namespace opengold::original
{
namespace
{
using C = ClassSlot;
using P = Profession;
namespace data = death_knights_data;

std::size_t race_index(Race race)
{
    if (!champions_race(race))
    {
        throw std::invalid_argument("Not a Krynn race");
    }
    return static_cast<std::size_t>(race) - static_cast<std::size_t>(Race::silvanesti_elf);
}

std::size_t native_profession(P profession)
{
    return profession == P::knight ? 7 : static_cast<std::size_t>(profession);
}

std::optional<std::size_t> progression(C slot, KrynnOrders orders)
{
    switch (slot)
    {
    case C::cleric:
        return orders.deity > 4 ? 1 : 0;
    case C::fighter:
        return 2;
    case C::monk:
        return static_cast<std::size_t>(2 + std::clamp(orders.knight, 1, 3));
    case C::ranger:
        return 6;
    case C::magic_user:
        return orders.robe == 1 ? 7 : 8;
    case C::thief:
        return 9;
    case C::paladin:
        return 10;
    case C::druid:
        return std::nullopt;
    }
    return std::nullopt;
}
} // namespace

AbilityLimits death_knights_ability_limits(Race race, Gender gender)
{
    auto limits = champions_ability_limits(race, gender);
    if (race == Race::qualinesti_elf)
    {
        limits.minimum[rules::Ability::wisdom] = 6;
    }
    return limits;
}

rules::AbilityArray<int> death_knights_class_minimums(P profession)
{
    const auto &row = data::classStatMinimums.at(native_profession(profession));
    return {row[0], row[3], row[4], row[1], row[2], row[5]};
}

std::span<const P> death_knights_professions(Race race)
{
    if (!champions_race(race))
    {
        return {};
    }
    // Static value containers own the lists; returned spans borrow from them.
    static const auto choices = []
    {
        std::array<std::vector<P>, 7> result;
        for (std::size_t row = 0; row < result.size(); ++row)
        {
            const auto &native = data::raceClasses[row];
            for (int at = 1; at <= native[0]; ++at)
            {
                const int id = native[static_cast<std::size_t>(at)];
                result[row].push_back(id == 7 ? P::knight : static_cast<P>(id));
            }
        }
        return result;
    }();
    return choices[race_index(race)];
}

bool death_knights_alignment_allowed(P profession, Alignment alignment)
{
    static constexpr std::array<Alignment, 6> choices{
        Alignment::lawful_good,  Alignment::lawful_neutral, Alignment::neutral_good,
        Alignment::true_neutral, Alignment::chaotic_good,   Alignment::chaotic_neutral};
    const auto at = std::ranges::find(choices, alignment);
    return at != choices.end() && (data::classAlignments[0].at(native_profession(profession)) &
                                   (1 << (at - choices.begin()))) != 0;
}

int death_knights_level_limit(Race race, C slot)
{
    return data::racialLevelCaps.at(race_index(race)).at(static_cast<std::size_t>(slot));
}

std::optional<int> death_knights_experience_for_level(C slot, int level, KrynnOrders orders)
{
    const auto table = progression(slot, orders);
    if (!table || level < 1 || level > 18)
    {
        return std::nullopt;
    }
    if (level == 1)
    {
        return 0;
    }
    const int needed = data::progressions[*table].experience[static_cast<std::size_t>(level - 2)];
    return needed > 0 ? std::optional<int>{needed} : std::nullopt;
}

KrynnSpellRows death_knights_base_slots(const ClassLevels &levels, int wisdom, KrynnOrders orders)
{
    KrynnSpellRows slots{};
    const auto add = [&](std::size_t table, int first, int held, std::size_t source,
                         std::size_t target, std::size_t count)
    {
        for (int level = first; level <= std::min(held, 18); ++level)
        {
            const auto &increments =
                data::progressions[table].slots[static_cast<std::size_t>(level - 2)];
            for (std::size_t at = 0; at < count; ++at)
            {
                slots[target][at] += increments[source + at];
            }
        }
    };
    const int cleric = level_in(levels, C::cleric);
    if (cleric > 0)
    {
        slots[0][0] = orders.deity > 4 ? 2 : 1;
        add(orders.deity > 4 ? 1 : 0, 2, cleric, 0, 0, 7);
        static constexpr std::array<int, 6> wisdom_thresholds{12, 13, 14, 15, 16, 17};
        static constexpr std::array<std::size_t, 6> spell_levels{0, 0, 1, 1, 2, 3};
        for (std::size_t at = 0; at < wisdom_thresholds.size(); ++at)
        {
            if (slots[0][spell_levels[at]] > 0 && wisdom > wisdom_thresholds[at])
            {
                ++slots[0][spell_levels[at]];
            }
        }
    }
    add(10, 9, level_in(levels, C::paladin), 0, 0, 4);
    if (orders.knight > 1)
    {
        add(orders.knight == 2 ? 4 : 5, 6, level_in(levels, C::monk), 0, 0, 7);
    }
    add(6, 8, level_in(levels, C::ranger), 0, 1, 4);
    add(6, 8, level_in(levels, C::ranger), 4, 3, 4);
    const int mage = level_in(levels, C::magic_user);
    if (mage > 0)
    {
        ++slots[3][0];
        add(orders.robe == 1 ? 7 : 8, 2, mage, 0, 3, 8);
    }
    return slots;
}

std::uint64_t death_knights_calendar_day(std::uint64_t minutes) noexcept
{
    // Campaign time begins at noon; the first calendar day changes at minute 720.
    return minutes / 1440 + (minutes % 1440 + 720) / 1440;
}

void death_knights_calendar(rules::CharacterSheet::Original &numbers, std::uint64_t days)
{
    static constexpr std::array<unsigned, 3> periods{9, 2, 7};
    numbers.krynn_calendar_days = days;
    for (std::size_t moon = 0; moon < periods.size(); ++moon)
    {
        numbers.krynn_moon_phases[moon] = static_cast<int>((2 + (days / periods[moon]) % 4) % 4);
        numbers.krynn_moon_counters[moon] = static_cast<int>(days % periods[moon]);
    }
}

int death_knights_moon_allowance(const rules::CharacterSheet::Original &numbers)
{
    if (level_in(numbers.class_levels, C::magic_user) == 0 || numbers.robe_order < 1 ||
        numbers.robe_order > 3)
    {
        return 0;
    }
    const int phase = numbers.krynn_moon_phases[static_cast<std::size_t>(numbers.robe_order - 1)];
    return phase == 1 || phase == 2 ? phase : 0;
}

int death_knights_caster_level(const ClassLevels &levels, SpellClass caster, int robe, int phase)
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
    int effective = mage;
    if (mage > 0 && robe >= 1 && robe <= 3)
    {
        effective += phase == 0 ? -1 : phase == 2 && mage > 5 ? 1 : 0;
    }
    return std::max(effective, ranger);
}

int death_knights_thac0(const ClassLevels &levels)
{
    int best = 40;
    for (std::size_t at = 0; at < levels.size(); ++at)
    {
        if (levels[at] > 0)
        {
            best = std::max(best, data::thac0Table[at][std::clamp(levels[at], 1, 18)]);
        }
    }
    return 60 - best;
}

std::array<int, 5> death_knights_saving_throws(const ClassLevels &levels)
{
    std::array<int, 5> saves{20, 20, 20, 20, 20};
    for (std::size_t at = 0; at < levels.size(); ++at)
    {
        if (levels[at] <= 0)
        {
            continue;
        }
        const auto &row = data::savingThrows[at][std::clamp(levels[at], 1, 18) - 1];
        for (std::size_t save = 0; save < saves.size(); ++save)
        {
            saves[save] = std::min(saves[save], row[save]);
        }
    }
    return saves;
}

void recalculate_death_knights_numbers(rules::CharacterSheet::Original &numbers,
                                       const rules::AbilityArray<int> &scores)
{
    constexpr auto game = GoldBoxGame::death_knights_of_krynn;
    death_knights_calendar(numbers, numbers.krynn_calendar_days);
    const auto slots =
        death_knights_base_slots(numbers.class_levels, scores[rules::Ability::wisdom],
                                 {numbers.deity, numbers.robe_order, numbers.knight_order});
    numbers.cleric_slots = slots[0];
    numbers.druid_slots = slots[1];
    numbers.magic_user_slots = slots[3];
    const auto learn = [&](int number)
    {
        if (number && std::ranges::find(numbers.known_spells, number) == numbers.known_spells.end())
        {
            numbers.known_spells.push_back(number);
        }
    };
    for (std::size_t level = 0; level < 8; ++level)
    {
        if (numbers.cleric_slots[level] > 0)
        {
            for (const auto own : data::spellsByLevel[0][level])
            {
                learn(canonical_spell(own, game));
            }
        }
    }
    for (const auto number : castable_spells(game))
    {
        const auto *known = spell(number, game);
        if (known && known->caster_class == SpellClass::druid && known->level > 0 &&
            numbers.druid_slots[static_cast<std::size_t>(known->level - 1)] > 0)
        {
            learn(number);
        }
    }
    // Gifts have an independent allowance; their native table row is not a mage slot.
    numbers.deity_spell_slots = {};
    if (level_in(numbers.class_levels, C::cleric) > 0 && numbers.deity >= 1 && numbers.deity <= 7)
    {
        static constexpr std::array<std::array<int, 3>, 7> gifts{
            {{501}, {502}, {503}, {504, 505, 506}, {507}, {}, {506}}};
        numbers.deity_spell_slots[0] = numbers.deity == 4 ? 3 : numbers.deity == 6 ? 0 : 1;
        for (const auto gift : gifts[numbers.deity - 1])
        {
            learn(gift);
        }
    }
    numbers.thac0 = death_knights_thac0(numbers.class_levels);
    const auto race = static_cast<Race>(numbers.race);
    if (numbers.deity == 3 ||
        (numbers.deity == 6 && (race == Race::mountain_dwarf || race == Race::hill_dwarf)))
    {
        --numbers.thac0;
    }
    numbers.saving_throws = death_knights_saving_throws(numbers.class_levels);
}

std::vector<int> death_knights_learning_spells(int robe, int highest)
{
    std::vector<int> result;
    for (int level = 0; level < std::clamp(highest, 0, 8); ++level)
    {
        for (const auto own : data::spellsByLevel[robe == 1 ? 1 : 2][level])
        {
            if (const auto number = canonical_spell(own, GoldBoxGame::death_knights_of_krynn))
            {
                result.push_back(number);
            }
        }
    }
    return result;
}

std::vector<int> death_knights_starting_spells(int robe, int mage_level)
{
    std::vector<int> result;
    if (mage_level <= 0)
    {
        return result;
    }
    std::vector<int> native{10, 11, 12, 15, 16, 18, 19};
    const auto append = [&](int level, std::initializer_list<int> spells)
    {
        if (mage_level >= level)
        {
            native.insert(native.end(), spells);
        }
    };
    if (robe == 1)
    {
        append(3, {29, 34, 35});
        append(5, {47, 49, 51});
        append(7, {81, 82, 85, 87});
    }
    else
    {
        append(3, {30, 31, 32, 35});
        append(5, {45, 49, 48, 50});
        append(7, {83, 85, 84, 87});
    }
    for (const auto own : native)
    {
        result.push_back(canonical_spell(own, GoldBoxGame::death_knights_of_krynn));
    }
    return result;
}
} // namespace opengold::original
