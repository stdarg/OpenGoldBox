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

constexpr std::string_view profile_format = "combatant8";

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
                                   static_cast<int>(c.morale), c.intelligence, c.thief_level,
                                   c.backstabs ? 1 : 0, c.large ? 1 : 0, c.large_attack ? 1 : 0});
    const auto large = c.large_attack.value_or(AttackForm{});
    numbers.insert(numbers.end(), {large.dice, large.sides, large.modifier});
    const auto missile = c.missile.value_or(Missile{});
    const auto &after = missile.empty_handed_attack;
    numbers.insert(numbers.end(),
                   {c.missile ? 1 : 0, missile.range, missile.rate_per_two_rounds,
                    missile.thrown ? 1 : 0, missile.throwable ? 1 : 0, missile.loaded ? 1 : 0,
                    missile.needs_no_ammunition ? 1 : 0, missile.quantity,
                    missile.empty_handed_thac0, after.rate_per_two_rounds, after.dice, after.sides,
                    after.modifier, missile.empty_handed_backstabs ? 1 : 0});
    numbers.insert(numbers.end(), c.class_levels.begin(), c.class_levels.end());
    numbers.insert(numbers.end(),
                   {c.strength, c.undead_level, c.race, c.striker ? 1 : 0,
                    c.striker ? c.striker->plus : 0, c.striker && c.striker->silver ? 1 : 0,
                    c.striker ? c.striker->weapon_class : 0, c.weapon_type, c.wards_gaze ? 1 : 0,
                    c.save_bonus, static_cast<int>(c.effects.size())});
    for (const auto &effect : c.effects)
    {
        numbers.insert(numbers.end(),
                       {effect.code, effect.minutes, effect.strength, effect.on_remove ? 1 : 0});
    }
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
    // The fixed numbers, then four for each effect.
    const auto fixed = numbers_of(Combatant{}).size();
    if (numbers.size() < fixed || (numbers.size() - fixed) % 4 != 0 ||
        static_cast<std::size_t>(numbers[fixed - 1]) != (numbers.size() - fixed) / 4)
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
    c.thief_level = *next++;
    c.backstabs = *next++ != 0;
    c.large = *next++ != 0;
    const bool large_attack = *next++ != 0;
    if (large_attack)
    {
        c.large_attack = AttackForm{c.attacks[0].rate_per_two_rounds, next[0], next[1], next[2]};
    }
    next += 3;
    if (*next++ != 0)
    {
        Missile m;
        m.range = next[0];
        m.rate_per_two_rounds = next[1];
        m.thrown = next[2] != 0;
        m.throwable = next[3] != 0;
        m.loaded = next[4] != 0;
        m.needs_no_ammunition = next[5] != 0;
        m.quantity = next[6];
        m.empty_handed_thac0 = next[7];
        m.empty_handed_attack = {next[8], next[9], next[10], next[11]};
        m.empty_handed_backstabs = next[12] != 0;
        if (m.range < 1 || m.rate_per_two_rounds < 2 || m.quantity < 0)
        {
            return std::nullopt;
        }
        c.missile = m;
    }
    next += 13;
    for (auto &level : c.class_levels)
    {
        level = *next++;
        if (level < 0)
        {
            return std::nullopt;
        }
    }
    c.strength = *next++;
    c.undead_level = *next++;
    c.race = *next++;
    if (*next++ != 0)
    {
        c.striker = StrikingItem{next[0], next[1] != 0, next[2]};
    }
    next += 3;
    c.weapon_type = *next++;
    c.wards_gaze = *next++ != 0;
    c.save_bonus = *next++;
    const auto effect_count = *next++;
    for (int n = 0; n < effect_count; ++n, next += 4)
    {
        if (next[0] < 1 || next[0] > 255 || next[1] < 0 || next[2] < 0)
        {
            return std::nullopt;
        }
        c.effects.push_back({next[0], next[1], next[2], next[3] != 0});
    }
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
constexpr std::uint8_t weapon_slot = 0, shield_slot = 1, first_ring_slot = 9, arrow_slot = 11,
                       bolt_slot = 12;
constexpr std::uint8_t monster_race = 0, elf_race = 2;

// The original keeps Armor Class as 60 minus it, so that better is more: its
// adjustments add in that form.
constexpr int stored(int armor_class) noexcept
{
    return 60 - armor_class;
}

const CarriedItem *readied_in(std::span<const CarriedItem> carried, std::uint8_t slot)
{
    const auto found = std::find_if(carried.begin(), carried.end(),
                                    [slot](const CarriedItem &c)
                                    {
                                        return c.item.readied() && c.type.worn_location == slot;
                                    });
    return found == carried.end() ? nullptr : &*found;
}

// Name word 186 is "Holding": a readied bag of holding carries 5,000 coins'
// weight for nothing (charhlp.pas, RecalcStats).
constexpr std::uint8_t holding_word = 186;
constexpr int bag_of_holding_weight = 5000;

// The coins' weight of every item carried and every coin.
int load_of(const por::CharacterRecord &record, std::span<const CarriedItem> carried)
{
    int load = 0, readied_load = 0;
    bool bag = false;
    for (const auto &c : carried)
    {
        const int weight = c.item.weight * std::max<int>(c.item.stack_size, 1);
        load += weight;
        if (c.item.readied())
        {
            readied_load += weight;
            bag = bag || c.item.name_components[0] == holding_word;
        }
    }
    for (const auto coins : record.wealth)
    {
        load += coins;
    }
    if (bag)
    {
        load =
            std::max(load < bag_of_holding_weight ? 0 : load - bag_of_holding_weight, readied_load);
    }
    return load;
}

// An elf's +1 to hit with a bow or a sword: item types 36, 37 and 41 to 44.
bool elf_weapon(std::uint8_t type) noexcept
{
    return type == 36 || type == 37 || (type > 40 && type < 45);
}

// The readied missile weapon (cbtacts.pas, RecalcAttacksLeft and CombatAimMenu;
// charhlp.pas, FindDamagingRangedWeapon). A thrown weapon is its own
// ammunition; a launcher shoots the arrows or quarrels readied, and one whose
// only properties are Dexterity and firing (a sling) shoots with none.
Missile missile_of(const por::CharacterRecord &record, std::span<const CarriedItem> carried,
                   const CarriedItem &weapon)
{
    namespace bit = por::item_property;
    const auto &type = weapon.type;
    Missile m;
    m.range = type.range - 1;
    m.rate_per_two_rounds = std::max<int>(type.rate_of_fire, 2);
    m.thrown = type.has(bit::thrown);
    m.throwable = m.thrown && type.has(bit::strength_bonus);
    const CarriedItem *shot = m.thrown ? &weapon : nullptr;
    if (type.has(bit::fires_ammunition))
    {
        if (type.has(bit::fires_arrows))
        {
            shot = readied_in(carried, arrow_slot);
        }
        if (type.has(bit::fires_quarrels))
        {
            shot = readied_in(carried, bolt_slot);
        }
    }
    m.needs_no_ammunition =
        !shot && type.properties == (bit::dexterity_missile | bit::fires_ammunition);
    m.loaded = shot || m.needs_no_ammunition;
    m.quantity = shot ? shot->item.stack_size : 0;
    if (m.thrown)
    {
        std::vector<CarriedItem> rest;
        for (const auto &c : carried)
        {
            if (&c != &weapon)
            {
                rest.push_back(c);
            }
        }
        const auto empty_handed = combatant_from_record(record, rest);
        m.empty_handed_thac0 = empty_handed.thac0;
        m.empty_handed_attack = empty_handed.attacks[0];
        m.empty_handed_backstabs = empty_handed.backstabs;
    }
    return m;
}

// The item a creature's blows land with: a thrown weapon itself, the arrows
// or quarrels a launcher fires, else the readied weapon (charhlp.pas,
// FindDamagingRangedWeapon); none without a weapon.
std::optional<StrikingItem> striker_of(std::span<const CarriedItem> carried,
                                       const CarriedItem *weapon)
{
    if (!weapon)
    {
        return std::nullopt;
    }
    namespace bit = por::item_property;
    const CarriedItem *strikes = weapon;
    if (!weapon->type.has(bit::thrown) && weapon->type.has(bit::fires_ammunition))
    {
        const auto *shot = weapon->type.has(bit::fires_quarrels) ? readied_in(carried, bolt_slot)
                           : weapon->type.has(bit::fires_arrows) ? readied_in(carried, arrow_slot)
                                                                 : nullptr;
        strikes = shot ? shot : weapon;
    }
    // Name word 177 is "Silver", in the item's third name part.
    return StrikingItem{strikes->item.magic_bonus, strikes->item.name_components[2] == 177,
                        strikes->type.damage_type};
}

// A mirror (name word 118 in any part) or the wooden holy symbol (word 152
// with 252 in its third part) (effhndlr.pas, EffGazeBlock).
bool wards_gaze(const por::ItemRecord &item)
{
    const auto &words = item.name_components;
    const bool mirror = std::find(words.begin(), words.end(), 118) != words.end();
    const bool holy_symbol =
        std::find(words.begin(), words.end(), 152) != words.end() && words[2] == 252;
    return mirror || holy_symbol;
}

} // namespace

Combatant combatant_from_record(const por::CharacterRecord &record,
                                std::span<const CarriedItem> carried,
                                std::span<const por::EffectRecord> effects)
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
    const auto *weapon = readied_in(carried, weapon_slot);
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
        const auto *bolts = readied_in(carried, bolt_slot);
        const auto *arrows = readied_in(carried, arrow_slot);
        if (type.has(bit::fires_quarrels) && bolts)
        {
            plus += bolts->item.magic_bonus;
        }
        if (type.has(bit::fires_arrows) && arrows)
        {
            plus += arrows->item.magic_bonus;
        }
        c.attacks[0].modifier += plus;
        // Against a large creature the weapon's large dice and modifier take
        // the small ones' place (cbtacts.pas, RollAttacks).
        c.large_attack = AttackForm{
            c.attacks[0].rate_per_two_rounds, type.large_damage.count, type.large_damage.sides,
            c.attacks[0].modifier - type.small_medium_damage.modifier + type.large_damage.modifier};
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
    for (const auto &r : carried)
    {
        if (!r.item.readied() || r.type.protection_raw <= 0x7F)
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
            c.save_bonus += r.item.save_bonus;
        }
        else
        {
            adjustments[4] = std::max(adjustments[4], protection + r.item.magic_bonus);
        }
    }
    adjustments[4] = std::max(adjustments[4], stored(record.base_armor_class));
    c.movement = encumbered_movement(c.movement, load_of(record, carried),
                                     weight_allowance(strength, exceptional));
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
    c.class_levels = {l.cleric, l.druid, l.fighter, l.paladin, l.ranger, l.mage, l.thief, l.monk};
    c.attack_level = l.fighter > 0 && record.race > monster_race ? l.fighter : 1;
    c.initiative_bonus = dexterity_missile_bonus(dexterity);
    c.morale = record.npc_flag;
    c.intelligence = record.current_abilities.intelligence;
    c.thief_level = l.thief;
    // A backstab needs no armor or leather (type 50), and no weapon, a club (7),
    // a dagger (8) or a broad, long or short sword (35 to 37) (cbtacts.pas, IsBackstab).
    const auto *armor = readied_in(carried, 2);
    const auto weapon_type = weapon ? weapon->item.type : std::uint8_t{0};
    c.backstabs = l.thief > 0 && (!armor || armor->item.type == 50) &&
                  (!weapon || weapon_type == 7 || weapon_type == 8 ||
                   (weapon_type >= 35 && weapon_type <= 37));
    // The combat icon's size: above 0x80, or a size above 1 (cbtacts.pas, RollAttacks).
    c.large = record.icon_size_raw > 0x80 || (record.icon_size_raw & 7) > 1;
    if (weapon && weapon->type.range > 1)
    {
        c.missile = missile_of(record, carried, *weapon);
    }
    c.strength = strength;
    c.undead_level = record.undead_level;
    c.race = record.race;
    c.weapon_type = weapon ? weapon->item.type : -1;
    c.striker = striker_of(carried, weapon);
    c.wards_gaze = std::any_of(carried.begin(), carried.end(),
                               [](const CarriedItem &item)
                               {
                                   return item.item.readied() && wards_gaze(item.item);
                               });
    // Code 0 is no effect: the original dispatches none for it.
    for (const auto &effect : effects)
    {
        if (effect.code != 0)
        {
            c.effects.push_back(
                {effect.code, effect.duration_raw, effect.data, effect.table_flag != 0});
        }
    }
    return c;
}

} // namespace opengold::original
