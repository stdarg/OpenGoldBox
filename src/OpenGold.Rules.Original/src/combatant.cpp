#include "opengold/original_combat.h"

#include <algorithm>
#include <charconv>
#include <string>
#include <vector>

namespace opengold::original
{
namespace
{

constexpr std::string_view profile_format = "combatant1";

std::vector<int> numbers_of(const Combatant &c)
{
    std::vector<int> numbers{c.thac0,      c.armor_class,    c.rear_armor_class,
                             c.hit_points, c.max_hit_points, c.movement};
    for (const auto &attack : c.attacks)
    {
        numbers.insert(numbers.end(),
                       {attack.rate_per_two_rounds, attack.dice, attack.sides, attack.modifier});
    }
    numbers.insert(numbers.end(), c.saving_throws.begin(), c.saving_throws.end());
    numbers.insert(numbers.end(), {c.level, c.attack_level, c.initiative_bonus,
                                   static_cast<int>(c.morale), c.intelligence});
    return numbers;
}

} // namespace

std::string Combatant::encode() const
{
    std::string text(profile_format);
    for (const auto number : numbers_of(*this))
    {
        text += ' ' + std::to_string(number);
    }
    return text;
}

std::optional<Combatant> Combatant::decode(std::string_view data)
{
    if (!data.starts_with(profile_format))
    {
        return std::nullopt;
    }
    std::vector<int> numbers;
    auto rest = data.substr(profile_format.size());
    while (!rest.empty())
    {
        if (rest.front() != ' ')
        {
            return std::nullopt;
        }
        rest.remove_prefix(1);
        int value{};
        const auto [end, error] = std::from_chars(rest.data(), rest.data() + rest.size(), value);
        if (error != std::errc{})
        {
            return std::nullopt;
        }
        numbers.push_back(value);
        rest.remove_prefix(static_cast<std::size_t>(end - rest.data()));
    }
    if (numbers.size() != numbers_of(Combatant{}).size())
    {
        return std::nullopt;
    }
    Combatant c;
    auto next = numbers.begin();
    for (auto *field : {&c.thac0, &c.armor_class, &c.rear_armor_class, &c.hit_points,
                        &c.max_hit_points, &c.movement})
    {
        *field = *next++;
    }
    for (auto &attack : c.attacks)
    {
        attack = {next[0], next[1], next[2], next[3]};
        next += 4;
    }
    std::copy_n(next, c.saving_throws.size(), c.saving_throws.begin());
    next += static_cast<std::ptrdiff_t>(c.saving_throws.size());
    c.level = *next++;
    c.attack_level = *next++;
    c.initiative_bonus = *next++;
    c.morale = static_cast<unsigned>(*next++);
    c.intelligence = *next++;
    if (c.max_hit_points < 1 || c.movement < 0 ||
        std::any_of(
            c.attacks.begin(), c.attacks.end(),
            [](const AttackForm &a)
            {
                return a.rate_per_two_rounds < 0 || a.dice < 0 || a.sides < 0;
            }))
    {
        return std::nullopt;
    }
    return c;
}

Combatant combatant_from_record(const por::CharacterRecord &record)
{
    Combatant c;
    c.thac0 = record.base_thac0;
    c.armor_class = record.base_armor_class;
    // Without its Dexterity bonus, which a monster's record leaves out.
    c.rear_armor_class = record.base_armor_class;
    c.max_hit_points = std::max<int>(record.max_hit_points, 1);
    // The original loads a monster's record as it is, its current Hit Points
    // included (eclvm.pas, EclLoadMonster).
    c.hit_points = record.current.hit_points_raw ? record.current.hit_points_raw : c.max_hit_points;
    c.movement = record.base_movement;
    for (std::size_t form = 0; form < c.attacks.size(); ++form)
    {
        const auto &attack = record.base_attacks[form];
        c.attacks[form] = {attack.attacks_per_two_rounds, attack.damage.count, attack.damage.sides,
                           attack.damage.modifier};
    }
    c.saving_throws = {record.saves.paralysis_poison_death, record.saves.petrification_polymorph,
                       record.saves.rods_staves_wands, record.saves.breath, record.saves.spells};
    const auto &l = record.levels;
    c.level =
        std::max({l.cleric, l.druid, l.fighter, l.paladin, l.ranger, l.mage, l.thief, l.monk});
    c.level = std::max<int>(c.level, record.highest_level_raw);
    c.attack_level = std::max<int>(record.attack_level, 1);
    c.morale = record.npc_flag;
    c.intelligence = record.current_abilities.intelligence;
    return c;
}

} // namespace opengold::original
