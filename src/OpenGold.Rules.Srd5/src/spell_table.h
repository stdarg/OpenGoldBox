#ifndef OPENGOLD_SRD5_SPELL_TABLE_H
#define OPENGOLD_SRD5_SPELL_TABLE_H
#include "damage.h"
#include "damage_roll.h"
#include "status_effects.h"
#include <array>
#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace opengold::srd5::detail
{
// SRD 5.2.1 spell definitions. One row per spell; behaviour lives in
// Session::offer_spells and Session::resolve_spell, keyed on `pattern`.
// Adding a supported spell is a row here plus its class grant in
// spell_access.cpp -- not an edit to legal_commands() or submit().

// Every resolution shape the implemented catalog uses. Each is proved by at
// least one existing spell; none is speculative.
enum class SpellPattern : unsigned
{
    spell_attack,   // one attack roll, damage on hit, optional rider
    save_damage,    // save against 8 + casting; damage on failure, optionally half on success
    save_condition, // save against 8 + casting; rider on failure
    auto_damage,    // no roll; `instances` separately resolved damage instances
    repeat_attack,  // `instances` attack rolls against one target
    heal,           // restore HP from dice plus the caster's spellcasting modifier
    smite,          // Bonus Action right after the caster's own melee hit; extra damage to that target
    buff,           // a lasting benefit on the target, no roll; `rider` names it
    camp            // used only from the Camp dialog (CLASS-3), never offered in combat
};

// Which creatures the spell may be offered against. These reproduce the
// existing per-spell scopes exactly. Fire Bolt and Magic Missile are
// enemy-only today although SRD 5.2.1 allows any creature; that discrepancy is
// deliberately preserved here so the refactor is verifiable, and is recorded as
// a separate rules finding.
enum class SpellTarget : unsigned
{
    enemy,        // opposing side with hit points remaining
    any_creature, // any living actor in line of sight, either side
    wounded_ally, // same side, below maximum hit points
    ally,         // same side, including the caster
    self,         // the caster only
    area          // an area aimed at a point within range (CLASS-5)
};

// A lasting effect applied by the spell; each maps to one apply_* function.
enum class Rider : unsigned
{
    none,
    chill_touch,
    shocking_grasp,
    ray_of_frost,
    blindness,
    shield_of_faith,
    heroism,
    divine_favor,
    bless,
    protection_from_evil_and_good,
    command,
    hunters_mark,
    longstrider,
    entangle,
    fog_cloud,
    lesser_restoration,
    aid
};

// Added when cast from a level-two slot. Zeroed means the spell does not upcast.
struct Upcast
{
    unsigned extra_dice{}, extra_instances{};
    int extra_radius{}; // feet, for a sphere `area` spell
};

struct SpellDef
{
    std::string_view id, label;
    unsigned level{}; // 0 = cantrip
    unsigned mask{};  // combat.rules creature wire encoding; 0 past 31 bits
    SpellPattern pattern{};
    SpellTarget target{};
    int range{}; // feet
    bool verbal{true}, somatic{true};
    bool bonus_action{};
    bool melee{};                    // passes ranged=false to attack()
    bool requires_sight{};           // gated on can_see()
    bool requires_effect_capacity{}; // gated on can_apply()
    Ability save{Ability::strength}; // save patterns only
    bool half_on_success{};          // save_damage: a successful save halves the damage
    DamageType damage{DamageType::fire};
    DamageDice dice{};           // {count, sides, bonus}
    bool add_casting_modifier{}; // heal: bonus becomes casting - 2
    unsigned instances{1};       // darts / rays; for a buff, the creatures it may affect
    Upcast upcast{};
    Rider rider{Rider::none};
    bool concentration{}; // a caster keeps one Concentration spell at a time
    int area{};           // side of an `area` spell's square, feet
    int radius{};         // or the radius of its sphere, feet
};

// Order matches the sequence legal_commands() emitted before the table existed:
// the cantrips offered against any creature, then the enemy-only spells, then
// the ally-targeted healing. Offer order is observable, so keep it.
// Healing Word is the exception: as a Bonus Action it was emitted in its own
// pass ahead of the Action offers, so offer_spells() runs once per pass and
// filters on `bonus_action` rather than relying on this row order.
inline constexpr std::array spell_table
{
    SpellDef{
        .id = "chill_touch",
        .label = "Chill Touch",
        .level = 0,
        .mask = 2048,
        .pattern = SpellPattern::spell_attack,
        .target = SpellTarget::any_creature,
        .range = 5,
        .melee = true,
        .requires_effect_capacity = true,
        .damage = DamageType::necrotic,
        .dice = {1, 10, 0},
        .rider = Rider::chill_touch},
    SpellDef{
        .id = "shocking_grasp",
        .label = "Shocking Grasp",
        .level = 0,
        .mask = 1024,
        .pattern = SpellPattern::spell_attack,
        .target = SpellTarget::any_creature,
        .range = 5,
        .melee = true,
        .requires_effect_capacity = true,
        .damage = DamageType::lightning,
        .dice = {1, 8, 0},
        .rider = Rider::shocking_grasp},
    SpellDef{
        .id = "eldritch_blast",
        .label = "Eldritch Blast",
        .level = 0,
        .mask = 512,
        .pattern = SpellPattern::spell_attack,
        .target = SpellTarget::any_creature,
        .range = 120,
        .damage = DamageType::force,
        .dice = {1, 10, 0}},
    SpellDef{
        .id = "ray_of_frost",
        .label = "Ray of Frost",
        .level = 0,
        .mask = 256,
        .pattern = SpellPattern::spell_attack,
        .target = SpellTarget::any_creature,
        .range = 60,
        .requires_effect_capacity = true,
        .damage = DamageType::cold,
        .dice = {1, 8, 0},
        .rider = Rider::ray_of_frost},
    SpellDef{
        .id = "sacred_flame",
        .label = "Sacred Flame",
        .level = 0,
        .mask = 128,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::any_creature,
        .range = 60,
        .requires_sight = true,
        .save = Ability::dexterity,
        .damage = DamageType::radiant,
        .dice = {1, 8, 0}},
    SpellDef{
        .id = "poison_spray",
        .label = "Poison Spray",
        .level = 0,
        .mask = 64,
        .pattern = SpellPattern::spell_attack,
        .target = SpellTarget::any_creature,
        .range = 30,
        .damage = DamageType::poison,
        .dice = {1, 12, 0}},
    // Fire Bolt had no resolution branch of its own: it fell through to the
    // default case and relied on attack()'s default 1d10 fire arguments.
    SpellDef{
        .id = "fire_bolt",
        .label = "Fire Bolt",
        .level = 0,
        .mask = 1,
        .pattern = SpellPattern::spell_attack,
        .target = SpellTarget::enemy,
        .range = 120,
        .damage = DamageType::fire,
        .dice = {1, 10, 0}},
    SpellDef{
        .id = "magic_missile",
        .label = "Magic Missile",
        .level = 1,
        .mask = 4,
        .pattern = SpellPattern::auto_damage,
        .target = SpellTarget::enemy,
        .range = 120,
        .requires_sight = true,
        .damage = DamageType::force,
        .dice = {1, 4, 1},
        .instances = 3,
        .upcast = {.extra_instances = 1}},
    SpellDef{
        .id = "scorching_ray",
        .label = "Scorching Ray",
        .level = 2,
        .mask = 16,
        .pattern = SpellPattern::repeat_attack,
        .target = SpellTarget::enemy,
        .range = 120,
        .damage = DamageType::fire,
        .dice = {2, 6, 0},
        .instances = 3},
    SpellDef{
        .id = "blindness",
        .label = "Blindness",
        .level = 2,
        .mask = 32,
        .pattern = SpellPattern::save_condition,
        .target = SpellTarget::enemy,
        .range = 120,
        .somatic = false,
        .requires_sight = true,
        .requires_effect_capacity = true,
        .save = Ability::constitution,
        .rider = Rider::blindness},
    SpellDef{
        .id = "cure_wounds",
        .label = "Cure Wounds",
        .level = 1,
        .mask = 2,
        .pattern = SpellPattern::heal,
        .target = SpellTarget::wounded_ally,
        .range = 5,
        .dice = {2, 8, 0},
        .add_casting_modifier = true,
        .upcast = {.extra_dice = 2}},
    // SRD 5.2.1: a touched creature makes a Constitution save, taking 2d10
    // Necrotic damage on a failure or half on a success. Mask 0 deliberately:
    // bits exist only so profiles written before the explicit spell list can
    // still be read, and a new spell has no such history. New spells never get a bit.
    SpellDef{
        .id = "inflict_wounds",
        .label = "Inflict Wounds",
        .level = 1,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::enemy,
        .range = 5,
        .save = Ability::constitution,
        .half_on_success = true,
        .damage = DamageType::necrotic,
        .dice = {2, 10, 0},
        .upcast = {.extra_dice = 1}},
    // SRD 5.2.1 pp. 125 and 160: cast as a Bonus Action immediately after
    // hitting with a Melee weapon or an Unarmed Strike; the damage is part of
    // that attack, so a critical hit doubles its dice. Offered by the smite
    // window, never by the generic spell offers.
    SpellDef{
        .id = "divine_smite",
        .label = "Divine Smite",
        .level = 1,
        .pattern = SpellPattern::smite,
        .target = SpellTarget::enemy,
        .range = 5,
        .somatic = false,
        .bonus_action = true,
        .damage = DamageType::radiant,
        .dice = {2, 8, 0},
        .upcast = {.extra_dice = 1}},
    SpellDef{
        .id = "searing_smite",
        .label = "Searing Smite",
        .level = 1,
        .pattern = SpellPattern::smite,
        .target = SpellTarget::enemy,
        .range = 5,
        .somatic = false,
        .bonus_action = true,
        .save = Ability::constitution,
        .damage = DamageType::fire,
        .dice = {1, 6, 0},
        .upcast = {.extra_dice = 1}},
    // SRD 5.2.1 p. 128: after a weapon hit, Melee or Ranged; a Strength save or
    // Restrained, 1d6 Piercing at the start of each of its turns. A level-two
    // slot's extra die waits for Rangers of level five.
    SpellDef{
        .id = "ensnaring_strike",
        .label = "Ensnaring Strike",
        .level = 1,
        .pattern = SpellPattern::smite,
        .target = SpellTarget::enemy,
        .range = 5,
        .somatic = false,
        .bonus_action = true,
        .save = Ability::strength,
        .damage = DamageType::piercing,
        .dice = {1, 6, 0},
        .concentration = true},
    // SRD 5.2.1 p. 128: a 20-foot square of Difficult Terrain; each creature
    // in it when cast makes a Strength save or is Restrained.
    SpellDef{
        .id = "entangle",
        .label = "Entangle",
        .level = 1,
        .pattern = SpellPattern::save_condition,
        .target = SpellTarget::area,
        .range = 90,
        .somatic = true,
        .save = Ability::strength,
        .rider = Rider::entangle,
        .concentration = true,
        .area = 20},
    // SRD 5.2.1 p. 133: a 20-foot-radius sphere, Heavily Obscured; 20 feet
    // more radius per slot level above 1.
    SpellDef{
        .id = "fog_cloud",
        .label = "Fog Cloud",
        .level = 1,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::area,
        .range = 120,
        .upcast = {.extra_radius = 20},
        .rider = Rider::fog_cloud,
        .concentration = true,
        .radius = 20},
    // SRD 5.2.1 p. 108: up to three creatures' Hit Point maximum and current
    // Hit Points rise by 5 for 8 hours. Higher slots wait for level-three slots.
    SpellDef{
        .id = "aid",
        .label = "Aid",
        .level = 2,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::any_creature,
        .range = 30,
        .instances = 3,
        .rider = Rider::aid},
    // SRD 5.2.1 p. 144: ends one condition on a touched creature; Blinded is
    // the only one of its four the game has so far.
    SpellDef{
        .id = "lesser_restoration",
        .label = "Lesser Restoration",
        .level = 2,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::ally,
        .range = 5,
        .bonus_action = true,
        .rider = Rider::lesser_restoration},
    // SRD 5.2.1 pp. 162, 140 and 125. Benefits that need no roll.
    SpellDef{
        .id = "shield_of_faith",
        .label = "Shield of Faith",
        .level = 1,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::ally,
        .range = 60,
        .bonus_action = true,
        .rider = Rider::shield_of_faith,
        .concentration = true},
    SpellDef{
        .id = "heroism",
        .label = "Heroism",
        .level = 1,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::ally,
        .range = 5,
        .rider = Rider::heroism,
        .concentration = true},
    // SRD 5.2.1 p. 113: up to three creatures, one more per slot level above 1.
    SpellDef{
        .id = "bless",
        .label = "Bless",
        .level = 1,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::any_creature,
        .range = 30,
        .instances = 3,
        .upcast = {.extra_instances = 1},
        .rider = Rider::bless,
        .concentration = true},
    // SRD 5.2.1 p. 157. Its consumed holy water is not required (CLASS-3).
    SpellDef{
        .id = "protection_from_evil_and_good",
        .label = "Protection from Evil and Good",
        .level = 1,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::ally,
        .range = 5,
        .rider = Rider::protection_from_evil_and_good,
        .concentration = true},
    // SRD 5.2.1 p. 116: a Wisdom save or the target obeys on its next turn; one
    // more creature per slot level above 1. Each option is its own verb,
    // "command_<option>"; Drop is left out because the game has no dropped gear.
    SpellDef{
        .id = "command",
        .label = "Command",
        .level = 1,
        .pattern = SpellPattern::save_condition,
        .target = SpellTarget::any_creature,
        .range = 60,
        .somatic = false,
        .requires_sight = true,
        .save = Ability::wisdom,
        .upcast = {.extra_instances = 1},
        .rider = Rider::command},
    // SRD 5.2.1 p. 141. The mark's 1d6 Force damage follows the caster's
    // attack-roll hits; moving it after the target drops is a separate Bonus Action.
    SpellDef{
        .id = "hunters_mark",
        .label = "Hunter's Mark",
        .level = 1,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::enemy,
        .range = 90,
        .somatic = false,
        .bonus_action = true,
        .requires_sight = true,
        .rider = Rider::hunters_mark,
        .concentration = true},
    // SRD 5.2.1 p. 145: one more creature per slot level above 1.
    SpellDef{
        .id = "longstrider",
        .label = "Longstrider",
        .level = 1,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::ally,
        .range = 5,
        .upcast = {.extra_instances = 1},
        .rider = Rider::longstrider},
    // SRD 5.2.1 p. 137. Its ten berries are eaten at once in camp (CLASS-6):
    // the chosen member regains up to 10 Hit Points.
    SpellDef{
        .id = "goodberry",
        .label = "Goodberry",
        .level = 1,
        .pattern = SpellPattern::camp,
        .target = SpellTarget::ally,
        .range = 5},
    SpellDef{
        .id = "divine_favor",
        .label = "Divine Favor",
        .level = 1,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::self,
        .range = 5,
        .bonus_action = true,
        .rider = Rider::divine_favor},
    SpellDef{
        .id = "healing_word",
        .label = "Healing Word",
        .level = 1,
        .mask = 8,
        .pattern = SpellPattern::heal,
        .target = SpellTarget::wounded_ally,
        .range = 60,
        .somatic = false,
        .bonus_action = true,
        .requires_sight = true,
        .dice = {2, 4, 0},
        .add_casting_modifier = true,
        .upcast = {.extra_dice = 2}}};

// Accepts the "_2" upcast verb form, so callers can pass a command verb directly.
// Command's options, in the order the effect stores them (1-based).
inline constexpr std::array<std::string_view, 4> command_options{"approach", "flee", "grovel",
    "halt"};
enum class CommandOption : int
{
    approach = 1,
    flee,
    grovel,
    halt
};

// The option a Command verb names, 1-based; 0 when the verb is not one.
inline int command_option(std::string_view verb)
{
    if (verb.ends_with("_2"))
        verb.remove_suffix(2);
    if (!verb.starts_with("command_"))
        return 0;
    verb.remove_prefix(8);
    const auto found = std::find(command_options.begin(), command_options.end(), verb);
    return found == command_options.end() ? 0 : int(found - command_options.begin()) + 1;
}

inline const SpellDef *find_spell(std::string_view id)
{
    if (id.ends_with("_2"))
        id.remove_suffix(2);
    if (command_option(id))
        id = "command";
    for (const auto &spell : spell_table)
        if (spell.id == id)
            return &spell;
    return nullptr;
}

// `mask` is a wire encoding, not the identity of a spell. It exists only so
// authored creature rows in combat.rules can still be read. An int holds at
// most 31 of them, which is why it is not the internal representation: a row
// added past that limit simply carries mask 0. Nothing in the rules should
// branch on a mask.
inline std::vector<std::string> spells_from_mask(unsigned mask)
{
    std::vector<std::string> result;
    for (const auto &spell : spell_table)
        if (spell.mask && (mask & spell.mask))
            result.emplace_back(spell.id);
    return result;
}

// Zero for a set that cannot be expressed as a mask.
inline unsigned mask_from_spells(const std::vector<std::string> &ids)
{
    unsigned mask = 0;
    for (const auto &id : ids)
    {
        const auto *spell = find_spell(id);
        if (!spell || !spell->mask)
            return 0;
        mask |= spell->mask;
    }
    return mask;
}

inline bool knows_spell(const std::vector<std::string> &ids, std::string_view id)
{
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

// Split a stored set by spell level. Unknown ids are dropped; callers that must
// reject them validate with find_spell first.
inline std::vector<std::string> spells_of_level(const std::vector<std::string> &ids, bool cantrips)
{
    std::vector<std::string> result;
    for (const auto &id : ids)
    {
        const auto *spell = find_spell(id);
        if (spell && (spell->level == 0) == cantrips)
            result.push_back(id);
    }
    return result;
}
} // namespace opengold::srd5::detail
#endif
