#ifndef OPENGOLD_SRD5_SPELL_TABLE_H
#define OPENGOLD_SRD5_SPELL_TABLE_H
#include "damage.h"
#include "damage_roll.h"
#include "status_effects.h"
#include <array>
#include <algorithm>
#include <optional>
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
    camp,           // used only from the Camp dialog (CLASS-3), never offered in combat
    exploration,    // cast while exploring, such as Knock at a locked door
    weapon_strike,  // a weapon attack made by casting it, such as True Strike
    stabilize,      // a dying creature becomes Stable
    reaction        // cast only when its trigger asks the caster (React/Decline)
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
    dying_ally,   // same side, at 0 Hit Points and neither Stable nor dead
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
    aid,
    guiding_bolt,
    bane,
    hold_person,
    sanctuary,
    warding_bond,
    protection_from_poison,
    resistance,
    silence,
    spiritual_weapon,
    thunderwave,
    mage_armor,
    false_life,
    expeditious_retreat,
    ray_of_sickness,
    ice_knife,
    sleep,
    hideous_laughter,
    color_spray,
    grease,
    web,
    misty_step,
    acid_arrow,
    ray_of_enfeeblement,
    blur,
    mirror_image,
    magic_weapon,
    invisibility,
    see_invisibility,
    darkness,
    flaming_sphere,
    enlarge_reduce,
    dragons_breath,
    charm_person
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
    int cone{};           // or the length of a cone from the caster toward the aim, feet
    int cube{};           // or the side of a cube beside the caster toward the aim, feet
    bool evocation{};     // an Evocation spell, for the Evoker's Sculpt Spells
    bool humanoid_only{}; // offered only against a Humanoid
    bool not_self{};      // offered only on another creature
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
    // SRD 5.2.1 p. 139: the next attack roll against the target has Advantage.
    SpellDef{
        .id = "guiding_bolt",
        .label = "Guiding Bolt",
        .level = 1,
        .pattern = SpellPattern::spell_attack,
        .target = SpellTarget::enemy,
        .range = 120,
        .damage = DamageType::radiant,
        .dice = {4, 6, 0},
        .upcast = {.extra_dice = 1},
        .rider = Rider::guiding_bolt},
    // SRD 5.2.1 p. 111: up to three creatures make a Charisma save or subtract
    // 1d4 from attack rolls and saves; one more per slot level above 1.
    SpellDef{
        .id = "bane",
        .label = "Bane",
        .level = 1,
        .pattern = SpellPattern::save_condition,
        .target = SpellTarget::enemy,
        .range = 30,
        .save = Ability::charisma,
        .instances = 3,
        .upcast = {.extra_instances = 1},
        .rider = Rider::bane,
        .concentration = true},
    // SRD 5.2.1 p. 140: a Humanoid makes a Wisdom save or is Paralyzed,
    // repeating it at the end of each of its turns. A higher slot's extra
    // target waits for level-three slots.
    SpellDef{
        .id = "hold_person",
        .label = "Hold Person",
        .level = 2,
        .pattern = SpellPattern::save_condition,
        .target = SpellTarget::enemy,
        .range = 60,
        .requires_sight = true,
        .requires_effect_capacity = true,
        .save = Ability::wisdom,
        .rider = Rider::hold_person,
        .concentration = true,
        .humanoid_only = true},
    // SRD 5.2.1 p. 160: attackers of the warded creature save or lose the
    // attack (CLASS-9); it ends when the creature attacks, casts or harms.
    SpellDef{
        .id = "sanctuary",
        .label = "Sanctuary",
        .level = 1,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::ally,
        .range = 30,
        .bonus_action = true,
        .rider = Rider::sanctuary},
    // SRD 5.2.1 p. 177: its rings are not required (CLASS-8).
    SpellDef{
        .id = "warding_bond",
        .label = "Warding Bond",
        .level = 2,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::ally,
        .range = 5,
        .rider = Rider::warding_bond,
        .not_self = true},
    // SRD 5.2.1 p. 157: Resistance to Poison damage; the game has no Poisoned
    // condition yet.
    SpellDef{
        .id = "protection_from_poison",
        .label = "Protection from Poison",
        .level = 2,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::ally,
        .range = 5,
        .rider = Rider::protection_from_poison},
    // SRD 5.2.1 p. 158: once per turn the touched creature takes 1d4 less damage
    // of the chosen type, offered once per type (CLASS-7).
    SpellDef{
        .id = "resistance",
        .label = "Resistance",
        .level = 0,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::ally,
        .range = 5,
        .rider = Rider::resistance,
        .concentration = true},
    // SRD 5.2.1 p. 162: a 20-foot-radius sphere where no spell with a Verbal
    // component can be cast and Thunder damage is ignored.
    SpellDef{
        .id = "silence",
        .label = "Silence",
        .level = 2,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::area,
        .range = 120,
        .rider = Rider::silence,
        .concentration = true,
        .radius = 20},
    // SRD 5.2.1 p. 165: a spectral force appears beside a creature within range
    // and makes a melee spell attack for 1d8 + the spellcasting modifier Force;
    // on later turns a Bonus Action moves it up to 20 feet and attacks again.
    SpellDef{
        .id = "spiritual_weapon",
        .label = "Spiritual Weapon",
        .level = 2,
        .pattern = SpellPattern::spell_attack,
        .target = SpellTarget::enemy,
        .range = 65,
        .bonus_action = true,
        .melee = true,
        .damage = DamageType::force,
        .dice = {1, 8, 0},
        .add_casting_modifier = true,
        .rider = Rider::spiritual_weapon,
        .concentration = true},
    // SRD 5.2.1 p. 155: ten minutes of prayer; up to five creatures regain
    // 2d8 + the spellcasting modifier, once each until a Long Rest. A camp spell.
    SpellDef{
        .id = "prayer_of_healing",
        .label = "Prayer of Healing",
        .level = 2,
        .pattern = SpellPattern::camp,
        .target = SpellTarget::ally,
        .range = 30,
        .dice = {2, 8, 0},
        .add_casting_modifier = true,
        .instances = 5},
    // SRD 5.2.1 p. 114: a 15-foot cone, Dexterity save for half.
    SpellDef{
        .id = "burning_hands",
        .label = "Burning Hands",
        .level = 1,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::area,
        .range = 15,
        .save = Ability::dexterity,
        .half_on_success = true,
        .damage = DamageType::fire,
        .dice = {3, 6, 0},
        .upcast = {.extra_dice = 1},
        .cone = 15,
        .evocation = true},
    // SRD 5.2.1 p. 169: a 15-foot cube from the caster, Constitution save for
    // half; a failed save also pushes the creature 10 feet away.
    SpellDef{
        .id = "thunderwave",
        .label = "Thunderwave",
        .level = 1,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::area,
        .range = 15,
        .save = Ability::constitution,
        .half_on_success = true,
        .damage = DamageType::thunder,
        .dice = {2, 8, 0},
        .upcast = {.extra_dice = 1},
        .rider = Rider::thunderwave,
        .cube = 15,
        .evocation = true},
    // SRD 5.2.1 p. 161: a 10-foot-radius sphere, Constitution save for half.
    SpellDef{
        .id = "shatter",
        .label = "Shatter",
        .level = 2,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::area,
        .range = 60,
        .save = Ability::constitution,
        .half_on_success = true,
        .damage = DamageType::thunder,
        .dice = {3, 8, 0},
        .radius = 10,
        .evocation = true},
    // SRD 5.2.1 p. 146: a willing unarmored creature's base AC is 13 + Dexterity.
    SpellDef{
        .id = "mage_armor",
        .label = "Mage Armor",
        .level = 1,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::ally,
        .range = 5,
        .rider = Rider::mage_armor},
    // SRD 5.2.1 p. 130: 2d4 + 4 Temporary Hit Points, 5 more from a level-two slot.
    SpellDef{
        .id = "false_life",
        .label = "False Life",
        .level = 1,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::self,
        .range = 5,
        .rider = Rider::false_life},
    // SRD 5.2.1 p. 130: Dash now and as a Bonus Action while it lasts.
    SpellDef{
        .id = "expeditious_retreat",
        .label = "Expeditious Retreat",
        .level = 1,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::self,
        .range = 5,
        .bonus_action = true,
        .rider = Rider::expeditious_retreat,
        .concentration = true},
    // SRD 5.2.1 p. 158: on a hit, Poisoned until the end of the caster's next turn.
    SpellDef{
        .id = "ray_of_sickness",
        .label = "Ray of Sickness",
        .level = 1,
        .pattern = SpellPattern::spell_attack,
        .target = SpellTarget::enemy,
        .range = 60,
        .damage = DamageType::poison,
        .dice = {2, 8, 0},
        .upcast = {.extra_dice = 1},
        .rider = Rider::ray_of_sickness},
    // SRD 5.2.1 p. 141: 1d10 Piercing on a hit, then, hit or miss, the target
    // and every creature within 5 feet save against 2d6 Cold.
    SpellDef{
        .id = "ice_knife",
        .label = "Ice Knife",
        .level = 1,
        .pattern = SpellPattern::spell_attack,
        .target = SpellTarget::enemy,
        .range = 60,
        .damage = DamageType::piercing,
        .dice = {1, 10, 0},
        .rider = Rider::ice_knife},
    // SRD 5.2.1 p. 115: 3d8 of the chosen type, offered once per type. The
    // leap on matching dice is not modeled.
    SpellDef{
        .id = "chromatic_orb",
        .label = "Chromatic Orb",
        .level = 1,
        .pattern = SpellPattern::spell_attack,
        .target = SpellTarget::enemy,
        .range = 90,
        .dice = {3, 8, 0},
        .upcast = {.extra_dice = 1}},
    // SRD 5.2.1 p. 107: a 5-foot-radius sphere within 60 feet, Dexterity save
    // or 1d6 Acid.
    SpellDef{
        .id = "acid_splash",
        .label = "Acid Splash",
        .level = 0,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::area,
        .range = 60,
        .save = Ability::dexterity,
        .damage = DamageType::acid,
        .dice = {1, 6, 0},
        .radius = 5},
    // SRD 5.2.1 p. 162: enemies in a 5-foot-radius sphere save or grow drowsy,
    // then fall asleep on a second failure. Elves and creatures that do not
    // sleep are unaffected.
    SpellDef{
        .id = "sleep",
        .label = "Sleep",
        .level = 1,
        .pattern = SpellPattern::save_condition,
        .target = SpellTarget::area,
        .range = 60,
        .save = Ability::wisdom,
        .rider = Rider::sleep,
        .concentration = true,
        .radius = 5},
    // SRD 5.2.1 p. 168 (Tasha's Hideous Laughter): Prone and Incapacitated.
    SpellDef{
        .id = "hideous_laughter",
        .label = "Hideous Laughter",
        .level = 1,
        .pattern = SpellPattern::save_condition,
        .target = SpellTarget::enemy,
        .range = 30,
        .requires_sight = true,
        .requires_effect_capacity = true,
        .save = Ability::wisdom,
        .rider = Rider::hideous_laughter,
        .concentration = true},
    // SRD 5.2.1 p. 116: a 15-foot cone; Blinded until the end of the caster's
    // next turn.
    SpellDef{
        .id = "color_spray",
        .label = "Color Spray",
        .level = 1,
        .pattern = SpellPattern::save_condition,
        .target = SpellTarget::area,
        .range = 15,
        .save = Ability::constitution,
        .rider = Rider::color_spray,
        .cone = 15},
    // SRD 5.2.1 p. 141: a 10-foot square of Difficult Terrain for 1 minute;
    // creatures in it, entering it or ending a turn in it save or fall Prone.
    SpellDef{
        .id = "grease",
        .label = "Grease",
        .level = 1,
        .pattern = SpellPattern::save_condition,
        .target = SpellTarget::area,
        .range = 60,
        .save = Ability::dexterity,
        .rider = Rider::grease,
        .area = 10},
    // SRD 5.2.1 p. 179: a 20-foot cube of Difficult Terrain; creatures
    // entering it or starting a turn in it save or are Restrained.
    SpellDef{
        .id = "web",
        .label = "Web",
        .level = 2,
        .pattern = SpellPattern::save_condition,
        .target = SpellTarget::area,
        .range = 60,
        .save = Ability::dexterity,
        .rider = Rider::web,
        .concentration = true,
        .area = 20},
    // SRD 5.2.1 p. 173: a Reaction when hit by an attack roll or targeted by
    // Magic Missile; +5 AC, including against the triggering attack, and no
    // Magic Missile damage until the start of the caster's next turn.
    SpellDef{
        .id = "shield",
        .label = "Shield",
        .level = 1,
        .pattern = SpellPattern::reaction,
        .target = SpellTarget::self,
        .range = 5},
    // SRD 5.2.1 p. 154: a Bonus Action teleport of up to 30 feet to an
    // unoccupied space the caster can see. Verbal only.
    SpellDef{
        .id = "misty_step",
        .label = "Misty Step",
        .level = 2,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::area,
        .range = 30,
        .somatic = false,
        .rider = Rider::misty_step,
        .bonus_action = true,
        .area = 5},
    // SRD 5.2.1 p. 107: 4d4 Acid on a hit and 2d4 more at the end of the
    // target's next turn; half the initial damage on a miss.
    SpellDef{
        .id = "acid_arrow",
        .label = "Acid Arrow",
        .level = 2,
        .pattern = SpellPattern::spell_attack,
        .target = SpellTarget::enemy,
        .range = 90,
        .damage = DamageType::acid,
        .dice = {4, 4, 0},
        .rider = Rider::acid_arrow,
        .evocation = true},
    // SRD 5.2.1 p. 149: a Wisdom save against 3d8 Psychic, half on a success.
    // Somatic only; the location it reveals has no use here.
    SpellDef{
        .id = "mind_spike",
        .label = "Mind Spike",
        .level = 2,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::enemy,
        .range = 120,
        .requires_sight = true,
        .verbal = false,
        .save = Ability::wisdom,
        .damage = DamageType::psychic,
        .dice = {3, 8, 0},
        .half_on_success = true,
        .concentration = true},
    // SRD 5.2.1 p. 157: a failed Constitution save enfeebles the target; a
    // success gives Disadvantage on its next attack roll.
    SpellDef{
        .id = "ray_of_enfeeblement",
        .label = "Ray of Enfeeblement",
        .level = 2,
        .pattern = SpellPattern::save_condition,
        .target = SpellTarget::enemy,
        .range = 60,
        .requires_effect_capacity = true,
        .save = Ability::constitution,
        .rider = Rider::ray_of_enfeeblement,
        .concentration = true},
    // SRD 5.2.1 p. 114: attack rolls against the caster have Disadvantage.
    SpellDef{
        .id = "blur",
        .label = "Blur",
        .level = 2,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::self,
        .range = 5,
        .somatic = false,
        .rider = Rider::blur,
        .concentration = true},
    // SRD 5.2.1 p. 150: three duplicates that may each take a hit.
    SpellDef{
        .id = "mirror_image",
        .label = "Mirror Image",
        .level = 2,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::self,
        .range = 5,
        .rider = Rider::mirror_image},
    // SRD 5.2.1 p. 146: a touched weapon gains +1 to attack and damage rolls.
    SpellDef{
        .id = "magic_weapon",
        .label = "Magic Weapon",
        .level = 2,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::ally,
        .range = 5,
        .bonus_action = true,
        .rider = Rider::magic_weapon},
    // SRD 5.2.1 p. 143: the touched creature is Invisible until it makes an
    // attack roll, deals damage or casts a spell.
    SpellDef{
        .id = "invisibility",
        .label = "Invisibility",
        .level = 2,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::ally,
        .range = 5,
        .rider = Rider::invisibility,
        .concentration = true},
    // SRD 5.2.1 p. 160: the caster sees Invisible creatures for an hour.
    SpellDef{
        .id = "see_invisibility",
        .label = "See Invisibility",
        .level = 2,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::self,
        .range = 5,
        .rider = Rider::see_invisibility},
    // SRD 5.2.1 p. 122: magical Darkness fills a 15-foot-radius sphere.
    SpellDef{
        .id = "darkness",
        .label = "Darkness",
        .level = 2,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::area,
        .range = 60,
        .somatic = false,
        .rider = Rider::darkness,
        .concentration = true,
        .radius = 15},
    // SRD 5.2.1 p. 132: a sphere of fire in an unoccupied space; a creature
    // ending its turn within 5 feet saves against 2d6 Fire, half on a success.
    SpellDef{
        .id = "flaming_sphere",
        .label = "Flaming Sphere",
        .level = 2,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::area,
        .range = 60,
        .save = Ability::dexterity,
        .damage = DamageType::fire,
        .dice = {2, 6, 0},
        .half_on_success = true,
        .rider = Rider::flaming_sphere,
        .concentration = true,
        .area = 5},
    // SRD 5.2.1 p. 127: Enlarge (offered on allies, who are willing) or Reduce
    // (offered on enemies, which make a Constitution save).
    SpellDef{
        .id = "enlarge_reduce",
        .label = "Enlarge/Reduce",
        .level = 2,
        .pattern = SpellPattern::save_condition,
        .target = SpellTarget::any_creature,
        .range = 30,
        .requires_sight = true,
        .requires_effect_capacity = true,
        .save = Ability::constitution,
        .rider = Rider::enlarge_reduce,
        .concentration = true},
    // SRD 5.2.1 p. 171: one attack with the caster's melee weapon using the
    // spellcasting ability; its damage may be Radiant or the weapon's type.
    SpellDef{
        .id = "true_strike",
        .label = "True Strike",
        .level = 0,
        .pattern = SpellPattern::weapon_strike,
        .target = SpellTarget::enemy,
        .range = 5,
        .verbal = false},
    // SRD 5.2.1 p. 124: a touched willing creature may exhale a cone of the
    // chosen type as an action, each offered as "dragons_breath_<type>".
    SpellDef{
        .id = "dragons_breath",
        .label = "Dragon's Breath",
        .level = 2,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::ally,
        .range = 5,
        .bonus_action = true,
        .rider = Rider::dragons_breath,
        .concentration = true},
    // The breath itself, aimed by the creature that holds Dragon's Breath as
    // "dragons_breath_exhale_<type>". No one learns it as a spell.
    SpellDef{
        .id = "dragons_breath_exhale",
        .label = "Dragon's Breath",
        .level = 0,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::area,
        .range = 15,
        .save = Ability::dexterity,
        .damage = DamageType::fire,
        .dice = {3, 6, 0},
        .half_on_success = true,
        .cone = 15},
    // SRD 5.2.1 p. 115: a Humanoid saves, with Advantage while the caster's side
    // is fighting it, or is Charmed by the caster until damaged; one more
    // creature from a level-two slot.
    SpellDef{
        .id = "charm_person",
        .label = "Charm Person",
        .level = 1,
        .pattern = SpellPattern::save_condition,
        .target = SpellTarget::enemy,
        .range = 30,
        .requires_sight = true,
        .requires_effect_capacity = true,
        .save = Ability::wisdom,
        .upcast = {.extra_instances = 1},
        .rider = Rider::charm_person,
        .humanoid_only = true},
    // SRD 5.2.1 p. 143: unlocks a door held by a mundane lock. Offered at a
    // locked door while exploring. Verbal only.
    SpellDef{
        .id = "knock",
        .label = "Knock",
        .level = 2,
        .pattern = SpellPattern::exploration,
        .target = SpellTarget::self,
        .range = 60,
        .somatic = false},
    // SRD 5.2.1 p. 163: a creature at 0 Hit Points within 15 feet becomes Stable.
    SpellDef{
        .id = "spare_the_dying",
        .label = "Spare the Dying",
        .level = 0,
        .pattern = SpellPattern::stabilize,
        .target = SpellTarget::dying_ally,
        .range = 15},
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

// Chromatic Orb's damage types, each offered as "chromatic_orb_<type>".
inline constexpr std::array<std::string_view, 6> chromatic_types{"acid", "cold", "fire",
    "lightning", "poison", "thunder"};

// The damage type a Chromatic Orb verb names, if it is one.
inline std::optional<DamageType> chromatic_type(std::string_view verb)
{
    if (verb.ends_with("_2"))
        verb.remove_suffix(2);
    if (!verb.starts_with("chromatic_orb_"))
        return std::nullopt;
    verb.remove_prefix(14);
    if (std::find(chromatic_types.begin(), chromatic_types.end(), verb) == chromatic_types.end())
        return std::nullopt;
    return damage_type(verb);
}

// Dragon's Breath's damage types, each offered as "dragons_breath_<type>" and
// exhaled as "dragons_breath_exhale_<type>".
inline constexpr std::array<std::string_view, 5> dragon_types{"acid", "cold", "fire",
    "lightning", "poison"};

// The damage type a Dragon's Breath or exhale verb names, if it is one.
inline std::optional<DamageType> dragon_type(std::string_view verb)
{
    for (const std::string_view prefix : {"dragons_breath_exhale_", "dragons_breath_"})
        if (verb.starts_with(prefix))
        {
            verb.remove_prefix(prefix.size());
            if (std::find(dragon_types.begin(), dragon_types.end(), verb) == dragon_types.end())
                return std::nullopt;
            return damage_type(verb);
        }
    return std::nullopt;
}

// Resistance's damage types (CLASS-7), each offered as "resistance_<type>".
inline constexpr std::array<std::string_view, 11> resistance_types{"acid", "bludgeoning", "cold",
    "fire", "lightning", "necrotic", "piercing", "poison", "radiant", "slashing", "thunder"};

// The damage type a Resistance verb names, if it is one.
inline std::optional<DamageType> resistance_type(std::string_view verb)
{
    if (!verb.starts_with("resistance_"))
        return std::nullopt;
    verb.remove_prefix(11);
    if (std::find(resistance_types.begin(), resistance_types.end(), verb) ==
            resistance_types.end())
        return std::nullopt;
    return damage_type(verb);
}

inline const SpellDef *find_spell(std::string_view id)
{
    if (id.ends_with("_2"))
        id.remove_suffix(2);
    if (command_option(id))
        id = "command";
    if (resistance_type(id))
        id = "resistance";
    if (chromatic_type(id))
        id = "chromatic_orb";
    if (id == "enlarge" || id == "reduce")
        id = "enlarge_reduce";
    if (id == "true_strike_radiant")
        id = "true_strike";
    if (dragon_type(id))
        id = id.starts_with("dragons_breath_exhale_") ? "dragons_breath_exhale" : "dragons_breath";
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
