#include "opengold/original_combat.h"
#include "opengold/original_rules.h"
#include "opengold/original_spells.h"
#include "opengold/secret_records.h"

#include <algorithm>
#include <charconv>
#include <string>
#include <vector>

namespace opengold::original
{
namespace
{

constexpr std::string_view profile_format = "combatant13";

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
                   {c.strength, c.undead_level, c.constitution, c.alignment, c.creature_type, c.kin,
                    c.traits, c.magic_resistance, c.race, c.striker ? 1 : 0,
                    c.striker ? c.striker->plus : 0, c.striker && c.striker->silver ? 1 : 0,
                    c.striker ? c.striker->weapon_class : 0, c.weapon_type, c.wards_gaze ? 1 : 0,
                    c.mirror ? 1 : 0, c.save_bonus, static_cast<int>(c.effects.size())});
    for (const auto &effect : c.effects)
    {
        numbers.insert(numbers.end(),
                       {effect.code, effect.minutes, effect.strength, effect.on_remove ? 1 : 0});
    }
    numbers.push_back(static_cast<int>(c.spells.size()));
    numbers.insert(numbers.end(), c.spells.begin(), c.spells.end());
    numbers.push_back(static_cast<int>(c.items.size()));
    for (const auto &item : c.items)
    {
        numbers.insert(numbers.end(),
                       {item.ordinal, item.readied ? 1 : 0, item.scroll ? 1 : 0, item.slot,
                        item.identified ? 1 : 0, item.spells[0], item.spells[1], item.spells[2],
                        item.charges, item.quantity});
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
    // The fixed numbers ending with the effects' count, four for each effect,
    // the spells' count and a number for each spell, then the items' count and
    // ten for each item.
    const auto fixed = numbers_of(Combatant{}).size() - 2;
    std::size_t at = fixed;
    for (const std::size_t width : {std::size_t{4}, std::size_t{1}, std::size_t{10}})
    {
        if (numbers.size() < at || numbers[at - 1] < 0)
        {
            return std::nullopt;
        }
        at += static_cast<std::size_t>(numbers[at - 1]) * width + 1;
    }
    if (numbers.size() != at - 1)
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
    c.constitution = *next++;
    c.alignment = *next++;
    c.creature_type = *next++;
    c.kin = *next++;
    c.traits = *next++;
    c.magic_resistance = *next++;
    c.race = *next++;
    if (*next++ != 0)
    {
        c.striker = StrikingItem{next[0], next[1] != 0, next[2]};
    }
    next += 3;
    c.weapon_type = *next++;
    c.wards_gaze = *next++ != 0;
    c.mirror = *next++ != 0;
    c.save_bonus = *next++;
    const auto effect_count = *next++;
    for (int n = 0; n < effect_count; ++n, next += 4)
    {
        // Curse's and Secret's own effects run past 255 (canonical_effect).
        if (next[0] < 1 || next[0] > 255 + secret_effect_offset || next[1] < 0 || next[2] < 0)
        {
            return std::nullopt;
        }
        c.effects.push_back({next[0], next[1], next[2], next[3] != 0});
    }
    const auto spell_count = *next++;
    for (int n = 0; n < spell_count; ++n)
    {
        c.spells.push_back(*next++);
    }
    const auto item_count = *next++;
    for (int n = 0; n < item_count; ++n, next += 10)
    {
        c.items.push_back({next[0],
                           next[1] != 0,
                           next[2] != 0,
                           next[3],
                           next[4] != 0,
                           {next[5], next[6], next[7]},
                           next[8],
                           next[9]});
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
constexpr std::uint8_t weapon_slot = 0, shield_slot = 1, first_ring_slot = 9;
// Arrows and quarrels take their places by their item type, not their type's
// slot (charhlp.pas, RecalcStats).
constexpr std::uint8_t arrows_type = 73, quarrels_type = 28;
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

const CarriedItem *readied_of_type(std::span<const CarriedItem> carried, std::uint8_t type)
{
    const auto found = std::find_if(carried.begin(), carried.end(),
                                    [type](const CarriedItem &c)
                                    {
                                        return c.item.readied() && c.item.type == type;
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
            shot = readied_of_type(carried, arrows_type);
        }
        if (type.has(bit::fires_quarrels))
        {
            shot = readied_of_type(carried, quarrels_type);
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
        const auto *shot =
            weapon->type.has(bit::fires_quarrels) ? readied_of_type(carried, quarrels_type)
            : weapon->type.has(bit::fires_arrows) ? readied_of_type(carried, arrows_type)
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

// The kind of foe a monster is to the races that know it, by its record's
// name, for a monster of creature type 1: a dwarf's foes and a gnome's of one
// square, giants that stand two squares tall, and bugbears and gnolls
// (effhndlr.pas, the racial attack bonuses and defenses).
int kin_of(const por::CharacterRecord &record)
{
    if (record.type != 1)
    {
        return 0;
    }
    const auto named = [&record](std::initializer_list<std::string_view> names)
    {
        return std::find(names.begin(), names.end(), record.name) != names.end();
    };
    const int icon = record.icon_size_raw;
    int kinds = 0;
    if (icon == 1 && named({"ORC", "ORC LEADER", "GOBLIN", "GOBLIN LEADER", "HOBGOBLIN",
                            "HOBGOBLIN CHIEF", "GAGOOL", "MACE"}))
    {
        kinds |= kin::dwarf_foe;
    }
    if (icon == 1 && named({"KOBOLD", "KOBOLD LEADER", "GOBLIN", "GOBLIN LEADER"}))
    {
        kinds |= kin::gnome_foe;
    }
    if ((icon & 0x7F) == 2 && named({"OGRE", "OGRE LEADER", "TROLL", "FIRE GIANT", "HILL GIANT"}))
    {
        kinds |= kin::giant;
    }
    if (named({"BUGBEAR", "GNOLL"}))
    {
        kinds |= kin::bugbear;
    }
    return kinds;
}

// A Secret of the Silver Blades creature's kinds of foe and traits, from its
// record's type and target flags (effhndlr.pas and magic.pas in its
// reconstruction): the bits its effects and spells test.
int secret_kin(const por::CharacterRecord &record)
{
    namespace type = secret::creature_type;
    const auto flags = record.creature_type_flags;
    int kinds = 0;
    kinds |= (flags & type::dwarf_foe) ? kin::dwarf_foe : 0;
    kinds |= (record.target_flags & secret::target::gnome_foe) ? kin::gnome_foe : 0;
    kinds |= (flags & type::dwarf_defends) ? kin::giant : 0;
    kinds |= (flags & type::gnome_defends) ? kin::bugbear : 0;
    return kinds;
}

int secret_traits(const por::CharacterRecord &record)
{
    namespace type = secret::creature_type;
    namespace target = secret::target;
    const auto flags = record.creature_type_flags, targets = record.target_flags;
    int traits = 0;
    traits |= (flags & type::dispel_evil) ? trait::dispel_evil : 0;
    traits |= (flags & type::mammal) ? trait::mammal : 0;
    traits |= (flags & type::ranger_foe) ? trait::ranger_foe : 0;
    traits |= (flags & type::snake) ? trait::snake : 0;
    traits |= (targets & target::giant_bane) ? trait::giant_bane : 0;
    traits |= (targets & target::person) ? trait::person : 0;
    traits |= (targets & target::unliving) ? trait::unliving : 0;
    return traits;
}

// Secret's effect that turns a gaze back on the gazer (EffPetrifyingGaze in
// its reconstruction), which an item gives while it is readied: the item's
// effect byte names it, and its special byte's 0x80 with no other bit says it
// is an effect (viewchar.pas, ToggleItemEffect).
constexpr int secret_reflection = 72;

bool reflects_gazes(const CarriedItem &carried)
{
    const auto &bytes = carried.item.effect_codes;
    return carried.item.readied() && bytes[1] == secret_reflection && bytes[2] == 0x80;
}

// A thief backstabs with no weapon, a club or a dagger (item types 3 and 4)
// or a broad, long or short sword (17 to 19) in any armor (cbtacts.pas,
// IsBackstab, in Secret's reconstruction).
bool secret_backstab_weapon(const CarriedItem *weapon)
{
    const int type = weapon ? weapon->item.type : 0;
    return !weapon || type == 3 || type == 4 || (type >= 17 && type <= 19);
}

// The items a creature can use: a scroll (an item whose type sits in slots
// 11 to 13) with a spell, or another item with a spell or power. In Pool of
// Radiance numbers above 56 are item powers 23 down (viewchar.pas, UseItem;
// magic.pas, ItemReadable); Curse and Secret name the spell itself (ovr020,
// UseMagicItem).
std::vector<UsableItem> usable_items(std::span<const CarriedItem> carried, GoldBoxGame game)
{
    const bool names_spells = game != GoldBoxGame::pool_of_radiance;
    std::vector<UsableItem> items;
    for (std::size_t n = 0; n < carried.size(); ++n)
    {
        const auto &c = carried[n];
        const auto &bytes = c.item.effect_codes;
        UsableItem usable;
        usable.ordinal = static_cast<int>(n);
        usable.readied = c.item.readied();
        usable.slot = c.type.worn_location;
        usable.identified = c.item.hidden_name_words == 0;
        usable.quantity = c.item.stack_size;
        if (usable.slot > 10 && usable.slot < 14)
        {
            usable.scroll = true;
            for (std::size_t s = 0; s < bytes.size(); ++s)
            {
                const int number = canonical_spell(bytes[s] & 0x7F, game);
                const auto *known = spell(number);
                usable.spells[s] = known && known->caster_class != SpellClass::item ? number : 0;
            }
            if (usable.spells == std::array<int, 3>{})
            {
                continue;
            }
        }
        else if (bytes[1] > 0 && bytes[2] < 0x80)
        {
            usable.spells[0] = names_spells    ? canonical_spell(bytes[1] & 0x7F, game)
                               : bytes[1] > 56 ? bytes[1] - 23
                                               : bytes[1];
            usable.charges = bytes[0];
        }
        else
        {
            continue;
        }
        items.push_back(usable);
    }
    return items;
}

} // namespace

namespace
{

// Secret's effects by the fight's numbers (effhndlr.pas in its reconstruction,
// judged by what each does, not by its name). Below 57 Secret keeps Curse's
// effects but for five: immunity to electricity (6, Curse's 135), a gnome's
// blow at its foes (7, Pool of Radiance's 18), Barkskin (13), the half-elf's
// resistance (18, Pool of Radiance's 124) and Dispel Evil's touch (32,
// Curse's 145). From 57 its effects are new or moved.
int secret_canonical_effect(int code) noexcept
{
    constexpr std::array<std::pair<int, int>, 27> moved{{
        {6, 335},   {7, 18},    {18, 124},  {32, 345},  {59, 127},  {61, 61},   {63, 263},
        {68, 268},  {69, 269},  {71, 207},  {73, 273},  {77, 277},  {89, 89},   {91, 291},
        {93, 112},  {95, 107},  {96, 108},  {97, 97},   {98, 310},  {99, 111},  {103, 119},
        {105, 334}, {107, 337}, {108, 338}, {110, 341}, {111, 342}, {112, 343},
    }};
    const auto found = std::ranges::find(moved, code, &std::pair<int, int>::first);
    if (found != moved.end())
    {
        return found->second;
    }
    if (code < 57 && code != 13)
    {
        return canonical_effect(code, GoldBoxGame::curse_of_the_azure_bonds);
    }
    return code + secret_effect_offset;
}

} // namespace

int canonical_effect(int code, GoldBoxGame game) noexcept
{
    if (game == GoldBoxGame::secret_of_the_silver_blades && code != 0)
    {
        return secret_canonical_effect(code);
    }
    if (game != GoldBoxGame::curse_of_the_azure_bonds || code == 0)
    {
        return code;
    }
    // Curse's codes that do what Pool of Radiance's of the same number do.
    constexpr std::array<int, 74> same{
        1,  2,  5,  6,  8,  9,  10, 11,  12,  13,  14,  15,  16,  17,  18,  19,  20, 21, 22,
        23, 24, 25, 26, 28, 29, 30, 31,  32,  33,  34,  36,  38,  39,  40,  41,  42, 43, 44,
        45, 46, 47, 48, 49, 51, 52, 53,  55,  56,  58,  59,  61,  64,  65,  66,  67, 70, 71,
        72, 79, 80, 83, 88, 89, 98, 100, 101, 102, 104, 107, 108, 109, 112, 113, 119};
    constexpr std::array<int, 7> also_same{121, 124, 125, 127, 114, 115, 116};
    if (std::ranges::find(same, code) != same.end() ||
        std::ranges::find(also_same, code) != also_same.end())
    {
        return code;
    }
    // Fumbling ends its bearer's actions as Pool of Radiance's helplessness does.
    if (code == 27)
    {
        return 31;
    }
    return code + curse_effect_offset;
}

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
        const auto *bolts = readied_of_type(carried, quarrels_type);
        const auto *arrows = readied_of_type(carried, arrows_type);
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
    const bool secret = record.game == GoldBoxGame::secret_of_the_silver_blades;
    if (secret)
    {
        c.backstabs = l.thief > 0 && secret_backstab_weapon(weapon);
    }
    // The combat icon's size: above 0x80, or a size above 1 (cbtacts.pas, RollAttacks).
    c.large = record.icon_size_raw > 0x80 || (record.icon_size_raw & 7) > 1;
    if (weapon && weapon->type.range > 1)
    {
        c.missile = missile_of(record, carried, *weapon);
    }
    c.strength = strength;
    c.undead_level = record.undead_level;
    c.constitution = record.current_abilities.constitution;
    c.alignment = record.alignment;
    c.creature_type = record.type;
    c.kin = secret ? secret_kin(record) : kin_of(record);
    c.traits = secret ? secret_traits(record) : 0;
    c.magic_resistance = record.magic_resistance;
    c.items = usable_items(carried, record.game);
    // Memorized spells; those still being learned (bit 0x80) are not ready.
    for (const auto memorized : record.memorized_spells)
    {
        if (memorized != 0 && memorized < 0x80)
        {
            c.spells.push_back(canonical_spell(memorized, record.game));
        }
    }
    c.race = record.race;
    c.weapon_type = weapon ? weapon->item.type : -1;
    c.striker = striker_of(carried, weapon);
    c.wards_gaze = std::any_of(carried.begin(), carried.end(),
                               [](const CarriedItem &item)
                               {
                                   return item.item.readied() && wards_gaze(item.item);
                               });
    c.mirror = std::any_of(carried.begin(), carried.end(),
                           [](const CarriedItem &item)
                           {
                               const auto &words = item.item.name_components;
                               return item.item.readied() &&
                                      std::find(words.begin(), words.end(), 118) != words.end();
                           });
    if (secret)
    {
        c.mirror = std::ranges::any_of(carried, reflects_gazes) ||
                   std::ranges::any_of(effects,
                                       [](const por::EffectRecord &effect)
                                       {
                                           return effect.code == secret_reflection;
                                       });
    }
    // Code 0 is no effect: the original dispatches none for it.
    for (const auto &effect : effects)
    {
        if (effect.code != 0)
        {
            c.effects.push_back({canonical_effect(effect.code, record.game), effect.duration_raw,
                                 effect.data, effect.table_flag != 0});
        }
    }
    return c;
}

} // namespace opengold::original
