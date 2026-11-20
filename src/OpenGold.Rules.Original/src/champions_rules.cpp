#include "champions_rules.h"
#include "opengold/buck_rogers_rules.h"
#include "opengold/dice.h"
#include "opengold/original_spells.h"
#include <algorithm>
#include <stdexcept>

namespace opengold::original
{
namespace
{
using P = Profession;
using C = ClassSlot;
using Row = std::array<int, 5>;

std::size_t race_index(Race race)
{
    if (!champions_race(race))
    {
        throw std::invalid_argument("Not a Krynn race");
    }
    return static_cast<std::size_t>(race) - static_cast<std::size_t>(Race::silvanesti_elf);
}

std::size_t progression(C slot, KrynnOrders orders)
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
        return orders.robe > 1 ? 8 : 7;
    case C::thief:
        return 9;
    default:
        return 0;
    }
}

// Numeric tables from GDConsts, interpreted by ClassCal and TrainingMenu.
constexpr std::array<std::array<int, 8>, 10> experience{
    {{4000, 7500, 15250, 25000, 40000, 90000, 160000},
     {3000, 6000, 13000, 27500, 55000, 110000, 225000},
     {2001, 4001, 8001, 18001, 35001, 70001, 125001},
     {5000, 10000, 18500, 37000, 85000, 140000, 220000},
     {5000, 12000, 24000, 45000, 95000, 175000, 350000},
     {5000, 12000, 27000, 60000, 125000, 200000, 425000},
     {2251, 4501, 10001, 20001, 40001, 90001},
     {5000, 10000, 20000, 38000, 55000, 100000, 200000},
     {5000, 10000, 18000, 36000, 50000, 90000, 180000},
     {1251, 2501, 5001, 10001, 20001, 42501, 70001, 110001}}};

constexpr std::array<std::array<Row, 7>, 4> slot_increments{{{{{1, 0, 0, 0, 0},
                                                               {0, 1, 0, 0, 0},
                                                               {0, 1, 0, 0, 0},
                                                               {1, 1, 1, 0, 0},
                                                               {0, 0, 1, 0, 0},
                                                               {0, 0, 0, 1, 0},
                                                               {0, 0, 1, 1, 0}}},
                                                             {{{0, 1, 0, 0, 0},
                                                               {1, 1, 1, 0, 0},
                                                               {1, 0, 1, 0, 0},
                                                               {0, 1, 0, 0, 0},
                                                               {0, 0, 0, 1, 0},
                                                               {0, 1, 1, 0, 0},
                                                               {0, 0, 0, 1, 0}}},
                                                             {{{1, 0, 0, 0, 0},
                                                               {0, 1, 0, 0, 0},
                                                               {1, 1, 0, 0, 0},
                                                               {1, 0, 1, 0, 0},
                                                               {0, 0, 1, 0, 0},
                                                               {0, 1, 0, 1, 0},
                                                               {0, 0, 1, 1, 0}}},
                                                             {{{1, 0, 0, 0, 0},
                                                               {0, 1, 0, 0, 0},
                                                               {1, 1, 1, 0, 0},
                                                               {1, 1, 0, 0, 0},
                                                               {0, 0, 1, 0, 0},
                                                               {0, 0, 0, 1, 0},
                                                               {0, 0, 1, 1, 0}}}}};
} // namespace

unsigned record_race(Race race, GoldBoxGame game) noexcept
{
    if (is_buck_rogers(game) && buck_rogers_race(race))
    {
        return static_cast<unsigned>(race) - static_cast<unsigned>(Race::monster);
    }
    if (krynn_game(game) && champions_race(race))
    {
        return static_cast<unsigned>(race) - static_cast<unsigned>(Race::silvanesti_elf);
    }
    return static_cast<unsigned>(race) + 1;
}

Race race_from_record(unsigned code, GoldBoxGame game) noexcept
{
    if (is_buck_rogers(game))
    {
        constexpr unsigned last_race = 8;
        return code <= last_race ? static_cast<Race>(static_cast<unsigned>(Race::monster) + code)
                                 : Race::monster;
    }
    if (krynn_game(game))
    {
        return code <= 6 ? static_cast<Race>(static_cast<unsigned>(Race::silvanesti_elf) + code)
                         : Race::krynn_human;
    }
    return code >= 1 && code <= 7 && code != 6 ? static_cast<Race>(code - 1) : Race::human;
}

AbilityLimits champions_ability_limits(Race race, Gender gender)
{
    // Strength minima, male/female maxima, exceptional maxima, then the five
    // other abilities' minima and maxima in the original record's order.
    constexpr std::array<std::array<int, 15>, 7> limits{
        {{3, 18, 16, 75, 0, 10, 18, 6, 18, 7, 19, 6, 18, 12, 18},
         {7, 18, 16, 75, 0, 8, 18, 3, 18, 7, 19, 7, 18, 8, 18},
         {3, 18, 17, 90, 0, 4, 18, 3, 18, 6, 18, 6, 18, 3, 18},
         {8, 18, 17, 99, 0, 3, 18, 3, 18, 3, 17, 12, 19, 3, 16},
         {9, 18, 17, 99, 0, 3, 18, 3, 18, 3, 17, 14, 19, 3, 12},
         {6, 16, 16, 0, 0, 6, 18, 3, 16, 8, 19, 10, 18, 6, 18},
         {3, 18, 18, 100, 50, 3, 18, 3, 18, 3, 18, 3, 18, 3, 18}}};
    const auto &r = limits[race_index(race)];
    const bool female = gender == Gender::female;
    return {{r[0], r[9], r[11], r[5], r[7], r[13]},
            {r[female ? 2 : 1], r[10], r[12], r[6], r[8], r[14]},
            r[female ? 4 : 3]};
}

std::span<const Profession> champions_professions(Race race)
{
    constexpr static std::array<P, 9> silvanesti{
        P::cleric,        P::fighter,           P::magic_user,
        P::ranger,        P::cleric_fighter,    P::cleric_fighter_magic_user,
        P::cleric_ranger, P::cleric_magic_user, P::fighter_magic_user};
    constexpr static std::array<P, 13> qualinesti{P::cleric,
                                                  P::fighter,
                                                  P::magic_user,
                                                  P::thief,
                                                  P::ranger,
                                                  P::cleric_fighter,
                                                  P::cleric_ranger,
                                                  P::cleric_fighter_magic_user,
                                                  P::cleric_magic_user,
                                                  P::fighter_magic_user,
                                                  P::fighter_thief,
                                                  P::fighter_magic_user_thief,
                                                  P::magic_user_thief};
    constexpr static std::array<P, 14> half_elf{P::cleric,
                                                P::fighter,
                                                P::magic_user,
                                                P::thief,
                                                P::ranger,
                                                P::knight,
                                                P::cleric_fighter,
                                                P::cleric_ranger,
                                                P::cleric_fighter_magic_user,
                                                P::cleric_magic_user,
                                                P::fighter_magic_user,
                                                P::fighter_thief,
                                                P::fighter_magic_user_thief,
                                                P::magic_user_thief};
    constexpr static std::array<P, 6> mountain{
        P::cleric, P::fighter, P::thief, P::cleric_fighter, P::cleric_thief, P::fighter_thief};
    constexpr static std::array<P, 8> small{P::cleric,       P::fighter,        P::thief,
                                            P::ranger,       P::cleric_fighter, P::cleric_ranger,
                                            P::cleric_thief, P::fighter_thief};
    constexpr static std::array<P, 6> human{P::cleric, P::fighter, P::magic_user,
                                            P::thief,  P::ranger,  P::knight};
    switch (race)
    {
    case Race::silvanesti_elf:
        return silvanesti;
    case Race::qualinesti_elf:
        return qualinesti;
    case Race::krynn_half_elf:
        return half_elf;
    case Race::mountain_dwarf:
        return mountain;
    case Race::hill_dwarf:
    case Race::kender:
        return small;
    case Race::krynn_human:
        return human;
    default:
        return {};
    }
}

std::array<int, 4> champions_age_categories(Race race)
{
    constexpr std::array<std::array<int, 4>, 7> ages{{{250, 650, 1000, 1500},
                                                      {250, 650, 1000, 1500},
                                                      {40, 100, 175, 250},
                                                      {60, 175, 275, 400},
                                                      {50, 150, 250, 350},
                                                      {33, 68, 101, 144},
                                                      {20, 40, 60, 90}}};
    return ages[race_index(race)];
}

int champions_starting_age(Race race, Profession profession, RandomState &random)
{
    // Rows give cleric, fighter, ranger, mage, thief and knight age dice.
    using Age = std::array<int, 3>;
    constexpr std::array<std::array<Age, 6>, 7> ages{
        {{{{250, 10, 10}, {150, 5, 6}, {150, 5, 6}, {250, 10, 10}, {150, 5, 4}, {130, 5, 6}}},
         {{{250, 10, 10}, {150, 5, 6}, {150, 5, 6}, {250, 10, 10}, {150, 5, 4}, {130, 5, 6}}},
         {{{40, 3, 6}, {22, 3, 4}, {22, 3, 4}, {40, 3, 6}, {22, 3, 8}, {22, 3, 4}}},
         {{{60, 3, 10}, {60, 3, 6}, {60, 3, 6}, {60, 3, 10}, {22, 3, 4}, {22, 3, 6}}},
         {{{60, 3, 10}, {60, 3, 6}, {60, 3, 6}, {60, 3, 10}, {22, 3, 4}, {22, 3, 6}}},
         {{{33, 3, 6}, {22, 3, 4}, {22, 3, 6}, {33, 3, 6}, {20, 2, 4}, {22, 1, 4}}},
         {{{18, 1, 4}, {15, 1, 4}, {20, 1, 4}, {24, 2, 4}, {18, 1, 4}, {20, 1, 4}}}}};
    const auto classes = classes_of(profession);
    const bool multi = classes.size() > 1;
    const auto has = [&](C slot)
    {
        return std::ranges::find(classes, slot) != classes.end();
    };
    const std::size_t row = has(C::cleric)       ? 0
                            : has(C::magic_user) ? 3
                            : has(C::fighter)    ? 1
                            : has(C::ranger)     ? 2
                            : has(C::thief)      ? 4
                                                 : 5;
    const auto &dice = ages[race_index(race)][row];
    int age = dice[0];
    for (int count = 0; count < dice[1]; ++count)
    {
        age += multi ? dice[2] : roll_die(random, dice[2]);
    }
    return age;
}

std::array<int, 8> champions_thief_adjustments(Race race)
{
    constexpr std::array<std::array<int, 8>, 7> adjustments{{{0, 10, 15, 0, 0, 0, -10, -5},
                                                             {5, -5, 0, 5, 10, 5, 0, 0},
                                                             {0, 5, 10, 5, 5, 10, -15, 0},
                                                             {10, 0, 0, 0, 5, 0, 0, 0},
                                                             {5, 5, 5, 10, 15, 5, -15, -5},
                                                             {-5, 5, 5, 0, 0, 5, 5, -10},
                                                             {}}};
    return adjustments[race_index(race)];
}

int champions_thac0(const ClassLevels &levels)
{
    constexpr std::array<std::array<int, 13>, 8> stored{
        {{40, 40, 40, 40, 42, 42, 42, 44, 44, 44, 46, 46, 46},
         {40, 40, 40, 40, 42, 42, 42, 44, 44, 44, 46, 46, 46},
         {39, 40, 40, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51},
         {40, 40, 40, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51},
         {40, 40, 40, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51},
         {39, 39, 39, 39, 39, 39, 41, 41, 41, 41, 41, 43, 43},
         {40, 40, 40, 40, 40, 41, 41, 41, 41, 44, 44, 44, 44},
         {40, 40, 40, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51}}};
    int best = 0;
    for (std::size_t slot = 0; slot < levels.size(); ++slot)
    {
        if (levels[slot] > 0)
        {
            best =
                std::max(best, stored[slot][static_cast<std::size_t>(std::min(levels[slot], 12))]);
        }
    }
    return best ? 60 - best : 20;
}

std::array<int, 5> champions_saving_throws(const ClassLevels &levels)
{
    // Knight and ranger saves use the fighter progression. The unused
    // paladin slot retains the original's two-point advantage.
    std::array<int, 5> best{20, 20, 20, 20, 20};
    for (std::size_t slot = 0; slot < levels.size(); ++slot)
    {
        if (levels[slot] <= 0)
        {
            continue;
        }
        ClassLevels one{};
        const auto mapped = slot == 7 ? 2 : slot;
        one[mapped] = std::min(levels[slot], 12);
        const auto saves = saving_throws(one, GoldBoxGame::curse_of_the_azure_bonds);
        for (std::size_t save = 0; save < best.size(); ++save)
        {
            best[save] = std::min(best[save], saves[save]);
        }
    }
    return best;
}

std::optional<int> champions_experience_for_level(C slot, int level, KrynnOrders orders)
{
    if (level == 1)
    {
        return 0;
    }
    if (level < 2 || level > 9)
    {
        return std::nullopt;
    }
    const auto value = experience[progression(slot, orders)][static_cast<std::size_t>(level - 2)];
    return value > 0 ? std::optional<int>{value} : std::nullopt;
}

std::array<int, 9> champions_spell_slots(C slot, int level, int wisdom, KrynnOrders orders)
{
    SpellSlots slots{};
    if (level < 1)
    {
        return slots;
    }
    if (slot == C::monk)
    {
        if (orders.knight > 1 && level >= 6)
        {
            slots[0] = level >= 7 ? 2 : 1;
            slots[1] = level >= 8 ? 1 : 0;
        }
        return slots;
    }
    if (slot != C::cleric && slot != C::magic_user)
    {
        return slots;
    }
    const auto table =
        slot == C::cleric ? (orders.deity > 4 ? 1U : 0U) : (orders.robe > 1 ? 3U : 2U);
    slots[0] = slot == C::cleric && orders.deity > 4 ? 2 : 1;
    for (int at = 0; at < std::min(level - 1, 7); ++at)
    {
        for (std::size_t spell_level = 0; spell_level < 5; ++spell_level)
        {
            slots[spell_level] += slot_increments[table][static_cast<std::size_t>(at)][spell_level];
        }
    }
    if (slot == C::cleric)
    {
        constexpr std::array<int, 4> thresholds{12, 14, 16, 17};
        for (std::size_t at = 0; at < thresholds.size(); ++at)
        {
            if (slots[at])
            {
                slots[at] += wisdom > thresholds[at] ? 1 : 0;
                if (at < 2 && wisdom > thresholds[at] + 1)
                {
                    ++slots[at];
                }
            }
        }
    }
    return slots;
}

std::optional<std::string_view> champions_ready_refusal(const por::ItemRecord &item, unsigned race,
                                                        unsigned robe) noexcept
{
    if (item.type == 67 && race != 5)
    {
        return "Only a kender can ready a hoopak";
    }
    if (item.type == 39 && !((robe == 1 && (item.hidden_name_words & 0x20)) ||
                             (robe == 2 && (item.hidden_name_words & 0x10))))
    {
        return "This robe belongs to another magical order";
    }
    return {};
}

void recalculate_champions_numbers(rules::CharacterSheet::Original &numbers,
                                   const rules::AbilityArray<int> &scores)
{
    const KrynnOrders orders{numbers.deity, numbers.robe_order, numbers.knight_order};
    const auto &levels = numbers.class_levels;
    const auto wisdom = scores[rules::Ability::wisdom];
    numbers.cleric_slots =
        champions_spell_slots(C::cleric, level_in(levels, C::cleric), wisdom, orders);
    const auto knight = champions_spell_slots(C::monk, level_in(levels, C::monk), wisdom, orders);
    for (std::size_t at = 0; at < knight.size(); ++at)
    {
        numbers.cleric_slots[at] += knight[at];
    }
    numbers.magic_user_slots =
        champions_spell_slots(C::magic_user, level_in(levels, C::magic_user), wisdom, orders);
    numbers.druid_slots = {};
    std::erase_if(numbers.known_spells,
                  [](int number)
                  {
                      const auto *s = spell(number);
                      return !s || s->caster_class != SpellClass::magic_user;
                  });
    // Champions caps rangers at seven, before their first druid/mage slots.
    constexpr std::array<std::array<int, 7>, 4> cleric_known{
        {{1, 3, 5, 6, 8}, {22, 23, 24, 25, 26, 27, 28}, {37, 39, 41, 42, 43}, {67, 70, 58, 69}}};
    for (std::size_t level = 0; level < cleric_known.size(); ++level)
    {
        if (!numbers.cleric_slots[level])
        {
            continue;
        }
        for (const auto own : cleric_known[level])
        {
            if (!own)
            {
                continue;
            }
            const auto canonical = canonical_spell(own, GoldBoxGame::champions_of_krynn);
            if (std::ranges::find(numbers.known_spells, canonical) == numbers.known_spells.end())
            {
                numbers.known_spells.push_back(canonical);
            }
        }
    }
    numbers.deity_spell_slots = {};
    if (level_in(levels, C::cleric) > 0 && numbers.deity >= 1 && numbers.deity <= 7)
    {
        numbers.deity_spell_slots[0] = numbers.deity == 4 ? 3 : numbers.deity == 6 ? 0 : 1;
        constexpr std::array<std::array<int, 3>, 7> gifts{
            {{501}, {502}, {503}, {504, 505, 506}, {507}, {}, {506}}};
        for (const auto gift : gifts[static_cast<std::size_t>(numbers.deity - 1)])
        {
            if (gift)
            {
                numbers.known_spells.push_back(gift);
            }
        }
    }
    numbers.thac0 = champions_thac0(levels);
    const auto race = static_cast<Race>(numbers.race);
    if (numbers.deity == 3 ||
        (numbers.deity == 6 && (race == Race::mountain_dwarf || race == Race::hill_dwarf)))
    {
        --numbers.thac0;
    }
    numbers.saving_throws = champions_saving_throws(levels);
}

std::vector<int> champions_learning_spells(int robe, int highest_spell_level)
{
    constexpr std::array<std::array<int, 8>, 4> white{{{10, 11, 14, 15, 16, 18, 19, 21},
                                                       {29, 34, 33},
                                                       {46, 47, 49, 51, 52, 54},
                                                       {85, 87, 81, 82, 86, 88, 89}}};
    constexpr std::array<std::array<int, 8>, 4> red{{{9, 11, 12, 15, 18, 19, 20},
                                                     {29, 30, 31, 32, 34, 35},
                                                     {45, 47, 48, 50, 51, 55},
                                                     {85, 87, 83, 84}}};
    const auto &table = robe == 1 ? white : red;
    std::vector<int> spells;
    for (int level = 0; level < std::clamp(highest_spell_level, 0, 4); ++level)
    {
        for (const auto own : table[static_cast<std::size_t>(level)])
        {
            if (own)
            {
                spells.push_back(canonical_spell(own, GoldBoxGame::champions_of_krynn));
            }
        }
    }
    return spells;
}
} // namespace opengold::original
