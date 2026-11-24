#include "opengold/buck_rogers_combat.h"
#include "opengold/buck_rogers_ship_combat.h"

#include <algorithm>
#include <stdexcept>

namespace opengold::original
{
namespace
{
using buck_rogers::Creature;
using buck_rogers::Item;
using buck_rogers::ItemType;

constexpr std::uint8_t rogue_career = 5;

[[nodiscard]] int ability(const Creature &creature, BuckRogersAbility which)
{
    return creature.abilities[static_cast<std::size_t>(which)];
}

[[nodiscard]] bool is_rogue(const Creature &creature) noexcept
{
    return creature.career == rogue_career;
}

[[nodiscard]] int dice(RandomState &random, int count, int sides)
{
    int total = 0;
    for (int die = 0; die < count; ++die)
    {
        total += roll_die(random, std::max(sides, 1));
    }
    return total;
}

// PercentileRoll: true `percent` times in a hundred.
[[nodiscard]] bool percent(RandomState &random, int chance)
{
    return chance > roll_die(random, 100) - 1;
}

// How many of a type's specializations the creature has (charhlp.pas SpecializationLevel).
[[nodiscard]] int specializations(const Creature &creature, std::uint8_t type)
{
    return static_cast<int>(
        std::count(creature.specializations.begin(), creature.specializations.end(), type));
}
} // namespace

void set_command_status(Creature &creature, BuckRogersStatus status) noexcept
{
    creature.status = static_cast<std::uint8_t>(status);
}

bool can_act(BuckRogersStatus status) noexcept
{
    return status == BuckRogersStatus::okay || status >= BuckRogersStatus::dodging;
}

bool has_ground_effect(std::span<const GroundEffect> effects, int code) noexcept
{
    return std::any_of(effects.begin(), effects.end(),
                       [code](const GroundEffect &effect)
                       {
                           return effect.code == code;
                       });
}

bool give_ground_effect(std::vector<GroundEffect> &effects, GroundEffect given, bool saved_cancels,
                        bool from_dazzle, bool from_stun_weapon)
{
    const auto blocks = [&](int block, bool applies)
    {
        return applies && has_ground_effect(effects, block);
    };
    // A holder of the unkillable effect takes no new effect (EffUnkillable at
    // COND_EFFECT_IMMUNITY).
    if (saved_cancels || has_ground_effect(effects, ground_effect::unkillable) ||
        blocks(ground_effect::block_scared, given.code == ground_effect::scared) ||
        blocks(ground_effect::block_helpless, given.code == ground_effect::helpless) ||
        blocks(ground_effect::block_dazzle, from_dazzle) ||
        blocks(ground_effect::block_stun, from_stun_weapon))
    {
        return false;
    }
    const auto there = std::find_if(effects.begin(), effects.end(),
                                    [&given](const GroundEffect &effect)
                                    {
                                        return effect.code == given.code;
                                    });
    if (there == effects.end())
    {
        effects.push_back(given);
    }
    else if (there->minutes != 0 && given.minutes > there->minutes)
    {
        there->minutes = given.minutes;
    }
    return true;
}

void remove_incapacitating(std::vector<GroundEffect> &effects)
{
    for (const int code : {ground_effect::scared, ground_effect::helpless, ground_effect::stunned,
                           ground_effect::dizzy})
    {
        const auto found = std::find_if(effects.begin(), effects.end(),
                                        [code](const GroundEffect &effect)
                                        {
                                            return effect.code == code;
                                        });
        if (found != effects.end())
        {
            effects.erase(found);
        }
    }
}

void age_ground_effects(std::vector<GroundEffect> &effects)
{
    std::erase_if(effects,
                  [](GroundEffect &effect)
                  {
                      return effect.minutes != 0 && --effect.minutes == 0;
                  });
}

namespace
{
// Version 2 keeps the second item list (TCharacter alternateItems), which
// version 1 did not; version 1 reads with none.
constexpr std::string_view ground_record_prefix = "OGBUCKROGERS ",
                           ground_record_format = "OGBUCKROGERS 2";

void put_hex(std::string &text, std::span<const std::uint8_t> bytes)
{
    constexpr std::string_view digits = "0123456789ABCDEF";
    // An empty run is a dash, so that every run is a word.
    text += bytes.empty() ? " -" : " ";
    for (const auto byte : bytes)
    {
        text += digits[byte >> 4];
        text += digits[byte & 0x0F];
    }
}

// The words of a text, split at spaces.
std::vector<std::string_view> words_of(std::string_view text)
{
    std::vector<std::string_view> words;
    while (!text.empty())
    {
        const auto space = text.find(' ');
        if (space != 0)
        {
            words.push_back(text.substr(0, space));
        }
        if (space == std::string_view::npos)
        {
            break;
        }
        text.remove_prefix(space + 1);
    }
    return words;
}

std::optional<std::vector<std::uint8_t>> hex_bytes(std::string_view word)
{
    if (word == "-")
    {
        return std::vector<std::uint8_t>{};
    }
    if (word.size() % 2 != 0)
    {
        return std::nullopt;
    }
    const auto digit = [](char c) -> int
    {
        return c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
    };
    std::vector<std::uint8_t> bytes;
    for (std::size_t at = 0; at < word.size(); at += 2)
    {
        const int high = digit(word[at]), low = digit(word[at + 1]);
        if (high < 0 || low < 0)
        {
            return std::nullopt;
        }
        bytes.push_back(static_cast<std::uint8_t>(high * 16 + low));
    }
    return bytes;
}

std::optional<std::size_t> count_of(std::string_view word)
{
    std::size_t value = 0;
    if (word.empty() || word.size() > 3)
    {
        return std::nullopt;
    }
    for (const char c : word)
    {
        if (c < '0' || c > '9')
        {
            return std::nullopt;
        }
        value = value * 10 + static_cast<std::size_t>(c - '0');
    }
    return value;
}
} // namespace

std::string encode_ground_record(const GroundRecord &record)
{
    std::string text(ground_record_format);
    put_hex(text, buck_rogers::encode_creature(record.creature));
    put_hex(text, buck_rogers::encode_items(record.items));
    put_hex(text, buck_rogers::encode_items(record.alternate_items));
    std::vector<std::uint8_t> named;
    for (const auto *list : {&record.items, &record.alternate_items})
    {
        for (const auto &item : *list)
        {
            if (item.type < record.types.size() &&
                std::find(named.begin(), named.end(), item.type) == named.end())
            {
                named.push_back(item.type);
            }
        }
    }
    text += ' ' + std::to_string(named.size());
    for (const auto type : named)
    {
        const auto &row = record.types[type];
        const std::array<std::uint8_t, ItemType::size> bytes{
            row.slot,
            row.attacks_doubled,
            row.weapon_class,
            row.heavy,
            row.dice_count,
            row.dice_sides,
            static_cast<std::uint8_t>(row.damage_bonus),
            row.range,
            row.consumable};
        text += ' ' + std::to_string(type);
        put_hex(text, bytes);
    }
    return text;
}

std::optional<GroundRecord> decode_ground_record(std::string_view text)
{
    if (!text.starts_with(ground_record_prefix) || text.size() <= ground_record_prefix.size())
    {
        return std::nullopt;
    }
    const char version = text[ground_record_prefix.size()];
    if (version != '1' && version != '2')
    {
        return std::nullopt;
    }
    const auto words = words_of(text.substr(ground_record_prefix.size() + 1));
    const std::size_t lists = version == '2' ? 2 : 1;
    if (words.size() < 2 + lists)
    {
        return std::nullopt;
    }
    const auto creature_bytes = hex_bytes(words[0]);
    const auto item_bytes = hex_bytes(words[1]);
    const auto alternate_bytes =
        lists == 2 ? hex_bytes(words[2])
                   : std::optional<std::vector<std::uint8_t>>{std::vector<std::uint8_t>{}};
    const auto rows = count_of(words[1 + lists]);
    if (!creature_bytes || !item_bytes || !alternate_bytes || !rows ||
        words.size() != 2 + lists + 2 * *rows)
    {
        return std::nullopt;
    }
    const auto list_of = [](const std::vector<std::uint8_t> &bytes)
    {
        return bytes.empty() ? std::optional<std::vector<Item>>{std::vector<Item>{}}
                             : buck_rogers::decode_items(bytes);
    };
    auto creature = buck_rogers::decode_creature(*creature_bytes);
    auto items = list_of(*item_bytes);
    auto alternates = list_of(*alternate_bytes);
    if (!creature || !items || !alternates)
    {
        return std::nullopt;
    }
    GroundRecord record{std::move(*creature), std::move(*items), {}, std::move(*alternates)};
    const std::size_t first_row = 2 + lists;
    for (std::size_t row = 0; row < *rows; ++row)
    {
        const auto type = count_of(words[first_row + 2 * row]);
        const auto bytes = hex_bytes(words[first_row + 1 + 2 * row]);
        if (!type || *type > 128 || !bytes || bytes->size() != ItemType::size)
        {
            return std::nullopt;
        }
        if (record.types.size() <= *type)
        {
            record.types.resize(*type + 1);
        }
        record.types[*type] = buck_rogers::decode_item_types(*bytes)->front();
    }
    return record;
}

const ItemType *type_of(const Item &item, std::span<const ItemType> types)
{
    return item.type < types.size() ? &types[item.type] : nullptr;
}

bool readied(const Item &item) noexcept
{
    return (item.flags & Item::readied) != 0;
}

std::optional<std::size_t> readied_in_slot(std::span<const Item> items,
                                           std::span<const ItemType> types, std::uint8_t slot)
{
    for (std::size_t at = 0; at < items.size(); ++at)
    {
        const auto *type = type_of(items[at], types);
        if (readied(items[at]) && type && type->slot == slot)
        {
            return at;
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> readied_weapon(std::span<const Item> items,
                                          std::span<const ItemType> types)
{
    std::optional<std::size_t> weapon;
    for (std::size_t at = 0; at < items.size(); ++at)
    {
        const auto *type = type_of(items[at], types);
        if (readied(items[at]) && type && type->slot == gear_slot::weapon)
        {
            weapon = at;
        }
    }
    return weapon;
}

WeaponClass weapon_class_of(std::span<const Item> items, std::span<const ItemType> types)
{
    const auto weapon = readied_weapon(items, types);
    return weapon ? static_cast<WeaponClass>(type_of(items[*weapon], types)->weapon_class)
                  : WeaponClass::melee;
}

int readied_weapon_range(std::span<const Item> items, std::span<const ItemType> types)
{
    const auto weapon = readied_weapon(items, types);
    if (!weapon)
    {
        return 1;
    }
    const auto &type = *type_of(items[*weapon], types);
    int range = type.range == 0 || type.range == 255 ? 1 : type.range;
    if (static_cast<WeaponClass>(type.weapon_class) == WeaponClass::grenade &&
        readied_in_slot(items, types, gear_slot::grenade_launcher))
    {
        range = 12;
    }
    return range;
}

int projectile_kind(const Item &item) noexcept
{
    const int kind = item.special[1];
    return item.special[2] != 0 && kind >= 5 && kind <= 12 ? kind : 0;
}

GroundNumbers ground_numbers(const Creature &creature, std::span<const Item> items,
                             std::span<const ItemType> types, bool side_tactics)
{
    GroundNumbers numbers;
    const auto weapon = readied_weapon(items, types);
    for (const auto &item : items)
    {
        const auto *type = type_of(item, types);
        if (!type || (type->slot == gear_slot::armor && readied(item)))
        {
            continue;
        }
        const bool counted = item.quantity > 0 && (item.flags & 0x20) == 0;
        numbers.encumbrance += item.weight * (counted ? item.quantity : 1);
    }
    for (std::size_t slot = 0; slot < numbers.damage.size(); ++slot)
    {
        numbers.damage[slot] = {creature.unarmed_dice_count[slot],
                                creature.unarmed_dice_sides[slot],
                                creature.unarmed_damage_bonus[slot]};
    }
    const int strength = ability(creature, BuckRogersAbility::strength);
    const int dexterity = ability(creature, BuckRogersAbility::dexterity);
    numbers.attack_bonus = creature.base_combat;
    if (!weapon)
    {
        numbers.attack_bonus += buck_rogers_strength_to_hit(strength);
        numbers.damage[0].bonus += buck_rogers_strength_damage(strength);
    }
    else
    {
        const auto &item = items[*weapon];
        const auto &type = *type_of(item, types);
        numbers.damage[0] = {type.dice_count, type.dice_sides, type.damage_bonus};
        if (static_cast<WeaponClass>(type.weapon_class) == WeaponClass::melee)
        {
            numbers.attack_bonus += buck_rogers_strength_to_hit(strength);
            numbers.damage[0].bonus += buck_rogers_strength_damage(strength);
        }
        else
        {
            numbers.attack_bonus += buck_rogers_missile_adjustment(dexterity);
        }
        const int adjustment = specializations(creature, item.type) + item.plus;
        numbers.attack_bonus += adjustment;
        if (item.type != ground_item::sonic_stunner)
        {
            numbers.damage[0].bonus += adjustment;
        }
    }
    const auto status = status_of(creature);
    numbers.attack_bonus -= status == BuckRogersStatus::failed_zero_g ? 2 : 0;
    numbers.attack_bonus += side_tactics ? 1 : 0;
    int front = creature.base_armor_class;
    for (const auto &item : items)
    {
        const auto *type = type_of(item, types);
        if (!readied(item) || !type)
        {
            continue;
        }
        switch (type->slot)
        {
        case gear_slot::armor:
            front += type->weapon_class - 50 + item.plus;
            numbers.armor_flags |= armor_bit::body | armor_bit::breathing;
            if (item.type == ground_item::battle_armor_with_fields)
            {
                numbers.armor_flags |= armor_bit::fields;
            }
            break;
        case gear_slot::goggles:
            numbers.armor_flags |= armor_bit::goggles;
            break;
        case gear_slot::breathing_mask:
            numbers.armor_flags |= armor_bit::breathing;
            break;
        case gear_slot::ecm:
            numbers.armor_flags |= armor_bit::ecm;
            break;
        default:
            break;
        }
    }
    const int movement = creature.base_movement;
    numbers.movement =
        buck_rogers_weight_allowance(strength) / 2 < numbers.encumbrance ? movement / 2 : movement;
    if (status == BuckRogersStatus::dodging)
    {
        front += dodge_armor_bonus(creature.level);
    }
    numbers.armor_class = {front + buck_rogers_armor_adjustment(dexterity), front - 2};
    return numbers;
}

int dodge_armor_bonus(int level) noexcept
{
    return level < 4 ? 2 : level / 2;
}

bool ground_attack_hits(int natural, int attack_bonus, int modifier, int target) noexcept
{
    if (natural <= 1)
    {
        return false;
    }
    const int counted = natural >= 20 ? 100 : natural;
    return counted + attack_bonus + modifier >= target;
}

int party_to_hit(BuckRogersDifficulty level) noexcept
{
    static constexpr std::array<int, 6> to_hit{2, 1, 0, -1, -2, -3};
    return to_hit[static_cast<std::size_t>(level)];
}

unsigned monster_hit_points_percent(BuckRogersDifficulty level) noexcept
{
    static constexpr std::array<unsigned, 6> percent{50, 75, 100, 125, 150, 200};
    return percent[static_cast<std::size_t>(level)];
}

int loaded_hit_points(int hit_points, BuckRogersDifficulty level) noexcept
{
    return std::max(hit_points * static_cast<int>(monster_hit_points_percent(level)) / 100, 1);
}

int range_penalty(int distance, int weapon_range) noexcept
{
    if (weapon_range <= 1)
    {
        return 0;
    }
    if (distance > weapon_range * 3 / 4)
    {
        return 5;
    }
    return distance > weapon_range / 2 ? 2 : 0;
}

int target_number(const GroundNumbers &defender, const AttackSetting &setting) noexcept
{
    int target = defender.armor_class[setting.from_behind ? 1 : 0];
    // The original adds 2 here, which makes a Rogue worse from behind than
    // anyone; these games count Armor Class upward, so the earlier games'
    // thief bonus kept its sign unturned. Fixed as a bug: 2 easier.
    target -= setting.from_behind && setting.rogue ? 2 : 0;
    if (setting.ranged_weapon && setting.guarding)
    {
        return target + 5;
    }
    return target + (setting.line.blocked ? 2 : 0) +
           range_penalty(setting.line.distance, setting.weapon_range);
}

bool rear_attack(int defender_facing, int attack_direction, int attacks_drawn,
                 bool one_square) noexcept
{
    const int apart = ((defender_facing - attack_direction) % 8 + 8) % 8;
    return attacks_drawn > 1 && one_square && (apart <= 1 || apart == 7);
}

bool backstab_position(int defender_facing, int attack_direction, int weapon_range) noexcept
{
    return weapon_range < 2 && attack_direction == defender_facing;
}

bool backstab_check(const Creature &attacker, RandomState &random)
{
    const int difficulty = is_rogue(attacker) ? 1 : 2;
    return ground_skill_succeeds(attacker, Skill::move_silently, random, difficulty) ||
           ground_skill_succeeds(attacker, Skill::distract, random, difficulty);
}

int backstab_damage(int damage, const Creature &attacker) noexcept
{
    return is_rogue(attacker) ? damage * ((attacker.level + 7) / 4) : damage * 2;
}

int damage_after_defenses(int damage, const HitDefense &defense,
                          std::span<const GroundEffect> defender_effects, RandomState &random)
{
    const auto has = [&](int code)
    {
        return has_ground_effect(defender_effects, code);
    };
    if (const auto weapon = defense.weapon)
    {
        const auto in = [&](WeaponClass low, WeaponClass high)
        {
            return *weapon >= low && *weapon <= high;
        };
        if ((has(ground_effect::immune_to_rocket) && *weapon == WeaponClass::rocket) ||
            (has(ground_effect::immune_to_microwave) && *weapon == WeaponClass::microwave) ||
            (has(ground_effect::immune_to_heat_and_plasma) &&
             (*weapon == WeaponClass::heat || *weapon == WeaponClass::plasma)) ||
            (has(ground_effect::immune_to_laser) && *weapon == WeaponClass::laser))
        {
            damage = 0;
        }
        // The two chances are rolled where the original rolls them, after the
        // immunities that come before them in its list.
        if (has(ground_effect::dodges_slugs) && *weapon == WeaponClass::slug &&
            defense.distance > 1 && percent(random, 50))
        {
            damage = 0;
        }
        if (has(ground_effect::resists_ranged) && in(WeaponClass::slug, WeaponClass::heat) &&
            defense.distance > 1 && percent(random, 50))
        {
            damage = 0;
        }
        if (has(ground_effect::resists_melee) && *weapon == WeaponClass::melee &&
            percent(random, 50))
        {
            damage = 0;
        }
    }
    if (has(ground_effect::unkillable))
    {
        damage = std::min(damage, defense.hit_points);
    }
    if (!defense.weapon)
    {
        return damage;
    }
    switch (*defense.weapon)
    {
    case WeaponClass::rocket:
        if (defense.line.chaff ||
            ((defense.armor_flags & armor_bit::ecm) != 0 && percent(random, 50)) ||
            ((defense.armor_flags & armor_bit::fields) != 0 && percent(random, 75)))
        {
            damage = 0;
        }
        break;
    case WeaponClass::laser:
        damage = defense.line.mist ? 0 : damage;
        break;
    case WeaponClass::microwave:
        damage = (defense.armor_flags & armor_bit::body) != 0 ? 0 : damage;
        break;
    default:
        break;
    }
    return damage;
}

int stun_rounds(RandomState &random)
{
    return roll_die(random, 4) + 4;
}

bool ground_save_made(int natural, int modifier, int needed) noexcept
{
    if (natural <= 1)
    {
        return false;
    }
    return natural >= 20 || natural + modifier >= needed;
}

bool roll_ground_save(const Creature &creature, int category, int modifier, bool blind,
                      RandomState &random)
{
    const int natural = roll_die(random, 20);
    return ground_save_made(natural, modifier - (blind ? 4 : 0),
                            creature.saves[static_cast<std::size_t>(category)]);
}

Injury injury_from(int hit_points, int damage) noexcept
{
    if (hit_points > damage)
    {
        return {BuckRogersStatus::okay, hit_points - damage, 0};
    }
    const int below = damage - hit_points;
    const auto status = below > 29  ? BuckRogersStatus::dead
                        : below > 9 ? BuckRogersStatus::comatose
                        : below > 0 ? BuckRogersStatus::dying
                                    : BuckRogersStatus::unconscious;
    return {status, 0, below};
}

void inflict_ground_damage(Creature &creature, int damage, int &dying_count)
{
    const auto injury = injury_from(creature.hit_points, damage);
    creature.hit_points = static_cast<std::uint16_t>(injury.hit_points);
    if (injury.status == BuckRogersStatus::okay)
    {
        return;
    }
    set_status(creature, injury.status);
    creature.enabled = 0;
    dying_count = injury.status == BuckRogersStatus::dying ? injury.below_zero : 0;
}

bool bleed(Creature &creature, int &dying_count)
{
    if (status_of(creature) != BuckRogersStatus::dying || ++dying_count <= 9)
    {
        return false;
    }
    set_status(creature, BuckRogersStatus::comatose);
    return true;
}

void poison_kills(Creature &creature)
{
    const auto status = status_of(creature);
    if (status == BuckRogersStatus::dead || status == BuckRogersStatus::gone)
    {
        return;
    }
    set_status(creature, BuckRogersStatus::poisoned);
    creature.enabled = 0;
    creature.hit_points = 0;
}

const ProjectileKind &projectile(int kind)
{
    using S = SaveEffect;
    static constexpr std::array<ProjectileKind, 9> kinds{{
        {ground_effect::helpless, 1, true, 1, S::cancels},
        {ground_effect::blind, 1, false, 3, S::cancels},
        {0, 1, false, 2, S::halves},
        {ground_effect::helpless, 1, true, 1, S::cancels},
        {0, 1, true, 7, S::none},
        {0, 1, true, 7, S::none},
        {0, 3, false, 2, S::halves},
        {0, 2, false, 2, S::halves},
        {ground_effect::stunned, 1, false, 0, S::cancels},
    }};
    if (kind < 4 || kind > 12)
    {
        throw std::invalid_argument("No projectile of that kind");
    }
    return kinds[static_cast<std::size_t>(kind - 4)];
}

Blast roll_blast(int kind, const ItemType &type, int plus, RandomState &random)
{
    Blast blast{dice(random, type.dice_count, type.dice_sides) + type.damage_bonus + plus, 255};
    switch (kind)
    {
    case 5:
        blast.minutes = roll_die(random, 6) + 1;
        break;
    case 7:
        blast.minutes = roll_die(random, 6) + 1;
        blast.damage = 0;
        break;
    case 12:
        blast.minutes = blast.damage;
        blast.damage = 0;
        break;
    default:
        break;
    }
    blast.damage = std::max(blast.damage, 0);
    return blast;
}

bool projectile_strays(int attack_bonus, int natural, int distance, int weapon_range) noexcept
{
    return attack_bonus + natural - range_penalty(distance, weapon_range) <= 50;
}

int scatter_offset(int offset, RandomState &random)
{
    const int amount = roll_die(random, 4) / 2;
    const int way = roll_die(random, 2) * 2 - 3;
    if (offset < -8 || offset > 8)
    {
        return offset + amount * way;
    }
    if (offset < 0)
    {
        return offset - amount;
    }
    return offset > 0 ? offset + amount : offset + way;
}

bool spared_by_gear(int kind, std::uint8_t armor_flags) noexcept
{
    return (kind == 5 && (armor_flags & armor_bit::goggles) != 0) ||
           (kind == 7 && (armor_flags & armor_bit::breathing) != 0);
}

int blast_damage(int damage, SaveEffect saved_effect, bool saved) noexcept
{
    if (!saved)
    {
        return damage;
    }
    return saved_effect == SaveEffect::cancels  ? 0
           : saved_effect == SaveEffect::halves ? damage / 2
                                                : damage;
}

bool within_blast(int doubled_distance, int radius) noexcept
{
    return doubled_distance <= radius * 2 + 1;
}

std::array<rules::Cell, 9> block_around(rules::Cell center) noexcept
{
    // Center, then east, south-east, south, south-west, west, north-west,
    // north and north-east.
    static constexpr std::array<int, 9> dx{0, 1, 1, 0, -1, -1, -1, 0, 1};
    static constexpr std::array<int, 9> dy{0, 0, 1, 1, 1, 0, -1, -1, -1};
    std::array<rules::Cell, 9> block{};
    for (std::size_t at = 0; at < block.size(); ++at)
    {
        block[at] = {center.x + dx[at], center.y + dy[at]};
    }
    return block;
}

std::vector<rules::Cell> GroundCloud::squares() const
{
    std::vector<rules::Cell> covered;
    const auto block = block_around(center);
    for (std::size_t at = 0; at < block.size(); ++at)
    {
        if (covers[at])
        {
            covered.push_back(block[at]);
        }
    }
    return covered;
}

bool GroundCloud::covers_square(rules::Cell cell) const
{
    const auto covered = squares();
    return std::find(covered.begin(), covered.end(), cell) != covered.end();
}

std::optional<CloudMatter> cloud_of(int kind) noexcept
{
    switch (kind)
    {
    case 7:
        return CloudMatter::gas;
    case 8:
        return CloudMatter::chaff;
    case 9:
        return CloudMatter::mist;
    default:
        return std::nullopt;
    }
}

bool cloud_lingers(GroundCloud &cloud) noexcept
{
    if (cloud.rounds_left <= 1)
    {
        return false;
    }
    --cloud.rounds_left;
    return true;
}

std::optional<int> gassed(const Creature &creature, std::uint8_t armor_flags, bool helpless,
                          bool blind, RandomState &random)
{
    if (creature.enabled == 0 || helpless || (armor_flags & armor_bit::breathing) != 0)
    {
        return std::nullopt;
    }
    if (roll_ground_save(creature, ground_save::gas, 0, blind, random))
    {
        return std::nullopt;
    }
    return roll_die(random, 6) + 1;
}

std::array<int, 2> ground_attacks(const Creature &creature, std::span<const Item> items,
                                  std::span<const ItemType> types, int round, bool dizzy)
{
    const auto halved = [round, dizzy](int rate)
    {
        rate = dizzy ? rate / 2 : rate;
        return (rate + (round & 1)) / 2;
    };
    const auto weapon = readied_weapon(items, types);
    int first = halved(weapon ? type_of(items[*weapon], types)->attacks_doubled
                              : creature.unarmed_attacks_doubled);
    if (weapon && static_cast<WeaponClass>(type_of(items[*weapon], types)->weapon_class) !=
                      WeaponClass::melee)
    {
        first = std::min(first, static_cast<int>(items[*weapon].quantity));
    }
    return {first, halved(creature.attacks_doubled)};
}

bool spend_rounds(Item &weapon, const ItemType &type, int shots)
{
    if (static_cast<WeaponClass>(type.weapon_class) == WeaponClass::melee || shots <= 0)
    {
        return false;
    }
    const int left = std::max(static_cast<int>(weapon.quantity) - shots, 0);
    weapon.quantity = static_cast<std::uint8_t>(left);
    return left == 0 && type.consumable != 0;
}

bool put_away_launcher(std::span<Item> items, std::span<const ItemType> types, std::size_t weapon)
{
    const auto &type = *type_of(items[weapon], types);
    const auto weapon_class = static_cast<WeaponClass>(type.weapon_class);
    if (weapon_class == WeaponClass::rocket_launcher || weapon_class == WeaponClass::plasma)
    {
        items[weapon].flags &= static_cast<std::uint8_t>(~Item::readied);
        return false;
    }
    if (weapon_class != WeaponClass::grenade)
    {
        return false;
    }
    const auto launcher = readied_in_slot(items, types, gear_slot::grenade_launcher);
    if (!launcher)
    {
        return false;
    }
    items[*launcher].flags &= static_cast<std::uint8_t>(~Item::readied);
    return true;
}

bool readying_ends_turn(const ItemType &type) noexcept
{
    return type.heavy != 0;
}

bool may_ready(std::span<const Item> items, std::span<const ItemType> types, std::size_t item)
{
    const auto *type = type_of(items[item], types);
    if (!type)
    {
        return false;
    }
    if (readied(items[item]))
    {
        return true;
    }
    return type->slot > gear_slot::ecm || !readied_in_slot(items, types, type->slot);
}

GroundMenu ground_menu(const Creature &creature, std::span<const Item> items,
                       std::span<const ItemType> types, const MenuSituation &situation)
{
    const auto status = status_of(creature);
    GroundMenu menu;
    menu.move = situation.movement_left;
    menu.intimidate = level_of(creature.skills, Skill::intimidate) > 0 && situation.enemy_next_to &&
                      status != BuckRogersStatus::dodging &&
                      status != BuckRogersStatus::failed_zero_g;
    menu.aid = situation.patient;
    if (situation.movement_left && status != BuckRogersStatus::dodging)
    {
        if (status != BuckRogersStatus::sprinting)
        {
            menu.sprint = status != BuckRogersStatus::used_jetpack &&
                          status != BuckRogersStatus::failed_zero_g;
            menu.dodge = true;
        }
        menu.jetpack = status != BuckRogersStatus::used_jetpack && situation.outdoors &&
                       readied_in_slot(items, types, gear_slot::jetpack).has_value();
    }
    menu.guard = status != BuckRogersStatus::sprinting && status != BuckRogersStatus::dodging;
    return menu;
}

bool may_strike(const Creature &creature, std::span<const Item> items,
                std::span<const ItemType> types)
{
    if (status_of(creature) == BuckRogersStatus::sprinting)
    {
        return false;
    }
    const auto weapon = readied_weapon(items, types);
    return !weapon ||
           static_cast<WeaponClass>(type_of(items[*weapon], types)->weapon_class) !=
               WeaponClass::slug ||
           items[*weapon].quantity > 0;
}

bool may_guard_with(std::span<const Item> items, std::span<const ItemType> types)
{
    return !hits_an_area(weapon_class_of(items, types));
}

bool ground_skill_succeeds(const Creature &creature, Skill skill, RandomState &random,
                           int difficulty)
{
    const std::array<int, 1> value{skill_value(creature, skill)};
    return check_skill(value, difficulty, random).grade >= SkillCheckGrade::success;
}

int sprint_movement(int half_moves) noexcept
{
    return half_moves * 2;
}

int zero_g_movement(int half_moves) noexcept
{
    return half_moves / 4;
}

bool rolls_zero_g(bool zero_gravity, bool enabled, int initiative) noexcept
{
    return zero_gravity && enabled && initiative > 1;
}

bool rolls_tactics(const Creature &creature, bool stunned) noexcept
{
    const auto status = status_of(creature);
    return creature.enabled != 0 && !stunned &&
           (status == BuckRogersStatus::okay || status == BuckRogersStatus::dodging ||
            status == BuckRogersStatus::used_jetpack);
}

bool may_lead(const Creature &leader) noexcept
{
    return leader.companion == 0 && level_of(leader.skills, Skill::leadership) > 0 &&
           status_of(leader) == BuckRogersStatus::okay;
}

bool may_follow(const Creature &follower, bool unkillable) noexcept
{
    return follower.companion != 0 && status_of(follower) == BuckRogersStatus::okay &&
           ability(follower, BuckRogersAbility::intelligence) > 3 && !unkillable;
}

GroundEffect scared_effect() noexcept
{
    return {ground_effect::scared, 10, 12};
}

rules::Cell lurch_step(rules::Cell from, RandomState &random) noexcept
{
    int roll = roll_die(random, 24) - 1;
    roll += roll >= 12 ? 1 : 0;
    return {from.x + roll % 5 - 2, from.y + roll / 5 - 2};
}
} // namespace opengold::original
