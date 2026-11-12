#include "opengold/original_combat.h"
#include "opengold/original_rules.h"

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

namespace
{

// The original's equipment slots (ItemTemplate::worn_location) and race codes.
constexpr std::uint8_t weapon_slot = 0, shield_slot = 1, armor_slot = 2, first_ring_slot = 9,
                       arrow_slot = 11, bolt_slot = 12;
constexpr std::uint8_t monster_race = 0, elf_race = 2;

// The original keeps Armor Class as 60 minus it, so that better is more: its
// adjustments add in that form.
constexpr int stored(int armor_class) noexcept
{
    return 60 - armor_class;
}

const ReadiedItem *readied_in(std::span<const ReadiedItem> readied, std::uint8_t slot)
{
    const auto found = std::find_if(readied.begin(), readied.end(),
                                    [slot](const ReadiedItem &r)
                                    {
                                        return r.type.worn_location == slot;
                                    });
    return found == readied.end() ? nullptr : &*found;
}

// An elf's +1 to hit with a bow or a sword: item types 36, 37 and 41 to 44.
bool elf_weapon(std::uint8_t type) noexcept
{
    return type == 36 || type == 37 || (type > 40 && type < 45);
}

} // namespace

Combatant combatant_from_record(const por::CharacterRecord &record,
                                std::span<const ReadiedItem> readied)
{
    Combatant c;
    const int strength = record.current_abilities.strength;
    const int exceptional = record.current_abilities.exceptional_strength;
    const int dexterity = record.current_abilities.dexterity;
    const bool uses_strength = record.strength_bonus_flag_raw != 0;
    c.max_hit_points = std::max<int>(record.max_hit_points, 1);
    c.hit_points = record.current.hit_points_raw ? record.current.hit_points_raw : c.max_hit_points;
    c.movement = record.base_movement;
    for (std::size_t form = 0; form < c.attacks.size(); ++form)
    {
        const auto &attack = record.base_attacks[form];
        c.attacks[form] = {attack.attacks_per_two_rounds, attack.damage.count, attack.damage.sides,
                           attack.damage.modifier};
    }
    c.thac0 = record.base_thac0;
    const auto *weapon = readied_in(readied, weapon_slot);
    if (!weapon && uses_strength)
    {
        c.thac0 -= strength_to_hit_bonus(strength, exceptional);
        c.attacks[0].modifier += strength_damage_bonus(strength, exceptional);
    }
    if (weapon)
    {
        namespace bit = por::item_property;
        const auto &type = weapon->type;
        if (type.has(bit::dexterity_missile))
        {
            c.thac0 -= dexterity_missile_bonus(dexterity);
        }
        c.attacks[0].dice = type.small_medium_damage.count;
        c.attacks[0].sides = type.small_medium_damage.sides;
        c.attacks[0].modifier = type.small_medium_damage.modifier;
        if (type.has(bit::strength_bonus) && uses_strength)
        {
            c.thac0 -= strength_to_hit_bonus(strength, exceptional);
            c.attacks[0].modifier += strength_damage_bonus(strength, exceptional);
        }
        int plus = weapon->item.magic_bonus;
        const auto *bolts = readied_in(readied, bolt_slot);
        const auto *arrows = readied_in(readied, arrow_slot);
        if (type.has(bit::fires_quarrels) && bolts)
        {
            plus += bolts->item.magic_bonus;
        }
        if (type.has(bit::fires_arrows) && arrows)
        {
            plus += arrows->item.magic_bonus;
        }
        c.attacks[0].modifier += plus;
        if (record.race == elf_race && elf_weapon(weapon->item.type))
        {
            ++plus;
        }
        c.thac0 -= plus;
    }
    // Armor Class: Dexterity, then the shield, the pluses of items with no
    // armor of their own, the best ring and the best armor, never worse than
    // the creature's own (charhlp.pas, AddItemArmorAdjust).
    std::array<int, 5> adjustments{dexterity_armor_class_bonus(dexterity), 0, 0, 0, 0};
    for (const auto &r : readied)
    {
        if (r.type.worn_location == armor_slot)
        {
            c.movement = armored_movement(c.movement, r.item.weight, r.item.magic_bonus != 0);
        }
        if (r.type.protection_raw <= 0x7F)
        {
            continue;
        }
        const int protection = r.type.protection_raw & 0x7F;
        if (r.type.worn_location == shield_slot)
        {
            adjustments[1] = protection + r.item.magic_bonus;
        }
        else if (protection == 0)
        {
            if (r.type.worn_location == first_ring_slot)
            {
                adjustments[3] = std::max(adjustments[3], r.item.magic_bonus);
            }
            else
            {
                adjustments[2] += r.item.magic_bonus;
            }
        }
        else
        {
            adjustments[4] = std::max(adjustments[4], protection + r.item.magic_bonus);
        }
    }
    adjustments[4] = std::max(adjustments[4], stored(record.base_armor_class));
    c.armor_class =
        stored(adjustments[0] + adjustments[1] + adjustments[2] + adjustments[3] + adjustments[4]);
    // From behind: the armor, its pluses and the ring, without Dexterity and
    // the shield, and 2 worse (charhlp.pas, RecalcStats).
    c.rear_armor_class = stored(adjustments[4] + adjustments[2] + adjustments[3] - 2);
    c.saving_throws = {record.saves.paralysis_poison_death, record.saves.petrification_polymorph,
                       record.saves.rods_staves_wands, record.saves.breath, record.saves.spells};
    const auto &l = record.levels;
    c.level =
        std::max({l.cleric, l.druid, l.fighter, l.paladin, l.ranger, l.mage, l.thief, l.monk});
    c.level = std::max<int>(c.level, record.highest_level_raw);
    c.attack_level = l.fighter > 0 && record.race > monster_race ? l.fighter : 1;
    c.initiative_bonus = dexterity_missile_bonus(dexterity);
    c.morale = record.npc_flag;
    c.intelligence = record.current_abilities.intelligence;
    return c;
}

} // namespace opengold::original
