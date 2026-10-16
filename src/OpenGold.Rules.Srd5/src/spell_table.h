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
    dead_ally,    // same side and dead; offered on its own (Revivify)
    corpse,       // a Small or Medium Humanoid enemy's corpse; offered on its own (Animate Dead)
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
    charm_person,
    gust_of_wind,
    hex,
    vicious_mockery,
    starry_wisp,
    dissonant_whispers,
    faerie_fire,
    shillelagh,
    produce_flame,
    barkskin,
    flame_blade,
    moonbeam,
    spike_growth,
    heat_metal,
    guidance,
    abjure_foes,
    beacon_of_hope,
    dispel_magic,
    death_ward,
    aura_of_life,
    freedom_of_movement,
    guardian_of_faith,
    banishment,
    remove_curse,
    revivify,
    magic_circle,
    protection_from_energy,
    conjure_animals,
    wind_wall,
    plant_growth,
    stoneskin,
    conjure_woodland_beings,
    dominate_beast,
    mass_healing_word,
    spirit_guardians,
    bestow_curse,
    animate_dead,
    contagion,
    dispel_evil_and_good,
    insect_plague,
    mass_cure_wounds,
    greater_restoration,
    blade_barrier,
    create_undead,
    harm,
    sunbeam,
    true_seeing,
    conjure_celestial,
    divine_word,
    regenerate,
    antimagic_field,
    earthquake,
    holy_aura,
    sunburst,
    haste,
    hypnotic_pattern,
    fear,
    sleet_storm,
    stinking_cloud,
    vampiric_touch,
    blink,
    black_tentacles,
    confusion,
    dimension_door,
    fire_shield,
    greater_invisibility,
    ice_storm,
    phantasmal_killer,
    polymorph,
    resilient_sphere,
    vitriolic_sphere,
    wall_of_fire,
    faithful_hound,
    conjure_minor_elementals,
    arcane_hand,
    cloudkill,
    conjure_elemental,
    dominate_person,
    summon_dragon,
    wall_of_force,
    wall_of_stone,
    animate_objects,
    mislead,
    telekinesis,
    contingency,
    disintegrate,
    eyebite,
    flesh_to_stone,
    globe_of_invulnerability,
    irresistible_dance,
    magic_jar,
    wall_of_ice,
    arcane_sword,
    delayed_blast_fireball,
    finger_of_death,
    forcecage,
    prismatic_spray,
    reverse_gravity,
    befuddlement,
    dominate_monster,
    incendiary_cloud,
    maze,
    mind_blank,
    power_word_stun,
    call_lightning,
    antilife_shell,
    giant_insect,
    conjure_fey,
    wall_of_thorns,
    natures_sanctuary,
    animal_shapes
};

// Added for each slot level above the spell's. Zeroed means the spell does not upcast.
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
    bool bonus_action{};
    bool melee{};                    // a melee spell attack, not a ranged one
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
    int line{};           // or the length of a 10-foot-wide line from the caster, feet
    bool narrow_line{};   // the line is 5 feet wide instead
    // or the length of a straight wall through the aimed square, feet, in a
    // direction the verb names (LEVELS-3)
    int wall{};
    // A second damage type rolled beside `damage`, such as Flame Strike's
    // Radiant; upcasting adds to both.
    DamageType second_damage{DamageType::fire};
    DamageDice second_dice{};
    // A wall that may also stand as a ring of this diameter, feet (LEVELS-3).
    int ring{};
    // Or up to this many 10-foot cubes the caster places one by one, each
    // touching another (Fire Storm, LEVELS-3).
    unsigned storm{};
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
        .target = SpellTarget::any_creature,
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
        .bonus_action = true,
        .save = Ability::constitution,
        .damage = DamageType::fire,
        .dice = {1, 6, 0},
        .upcast = {.extra_dice = 1}},
    // SRD 5.2.1 p. 164: after a Melee weapon or Unarmed Strike hit, 2d6
    // Radiant; until Concentration ends, attack rolls against the target have
    // Advantage and it cannot be Invisible.
    SpellDef{
        .id = "shining_smite",
        .label = "Shining Smite",
        .level = 2,
        .pattern = SpellPattern::smite,
        .target = SpellTarget::enemy,
        .range = 5,
        .bonus_action = true,
        .damage = DamageType::radiant,
        .dice = {2, 6, 0},
        .upcast = {.extra_dice = 1},
        .concentration = true},
    // SRD 5.2.1 p. 128: after a weapon hit, Melee or Ranged; a Strength save or
    // Restrained, 1d6 Piercing at the start of each of its turns, 1d6 more
    // for each slot level above 1.
    SpellDef{
        .id = "ensnaring_strike",
        .label = "Ensnaring Strike",
        .level = 1,
        .pattern = SpellPattern::smite,
        .target = SpellTarget::enemy,
        .range = 5,
        .bonus_action = true,
        .save = Ability::strength,
        .damage = DamageType::piercing,
        .dice = {1, 6, 0},
        .concentration = true,
        .upcast = {.extra_dice = 1}},
    // SRD 5.2.1 p. 128: a 20-foot square of Difficult Terrain; each creature
    // in it when cast makes a Strength save or is Restrained.
    SpellDef{
        .id = "entangle",
        .label = "Entangle",
        .level = 1,
        .pattern = SpellPattern::save_condition,
        .target = SpellTarget::area,
        .range = 90,
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
    // SRD 5.2.1: the touched creature adds 1d4 to ability checks with the
    // chosen skill, offered for each skill the game checks (GUIDE-1).
    SpellDef{
        .id = "guidance",
        .label = "Guidance",
        .level = 0,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::ally,
        .range = 5,
        .rider = Rider::guidance,
        .concentration = true},
    // SRD 5.2.1 p. 162: a 20-foot-radius sphere where Thunder damage is
    // ignored. Spells have no components here (CLASS-11), so it does not stop
    // spellcasting.
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
    // 2d8 + the spellcasting modifier, once each until a Long Rest, 1d8 more
    // for each slot level above 2. A camp spell.
    SpellDef{
        .id = "prayer_of_healing",
        .label = "Prayer of Healing",
        .level = 2,
        .pattern = SpellPattern::camp,
        .target = SpellTarget::ally,
        .range = 30,
        .dice = {2, 8, 0},
        .add_casting_modifier = true,
        .instances = 5,
        .upcast = {.extra_dice = 1}},
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
    // unoccupied space the caster can see.
    SpellDef{
        .id = "misty_step",
        .label = "Misty Step",
        .level = 2,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::area,
        .range = 30,
        .bonus_action = true,
        .rider = Rider::misty_step,
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
    // The location it reveals has no use here.
    SpellDef{
        .id = "mind_spike",
        .label = "Mind Spike",
        .level = 2,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::enemy,
        .range = 120,
        .requires_sight = true,
        .save = Ability::wisdom,
        .half_on_success = true,
        .damage = DamageType::psychic,
        .dice = {3, 8, 0},
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
        .half_on_success = true,
        .damage = DamageType::fire,
        .dice = {2, 6, 0},
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
        .range = 5},
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
        .half_on_success = true,
        .damage = DamageType::fire,
        .dice = {3, 6, 0},
        .cone = 15},
    // A Dragonborn's Breath Weapon (SRD 5.2.1 p. 84), aimed in place of an
    // attack as "breath_weapon_cone" or "breath_weapon_line"; its damage type
    // is the Draconic Ancestry's, and its die grows only from level 5.
    SpellDef{
        .id = "breath_weapon_cone",
        .label = "Breath Weapon",
        .level = 0,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::area,
        .range = 15,
        .save = Ability::dexterity,
        .half_on_success = true,
        .dice = {1, 10, 0},
        .cone = 15},
    SpellDef{
        .id = "breath_weapon_line",
        .label = "Breath Weapon",
        .level = 0,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::area,
        .range = 30,
        .save = Ability::dexterity,
        .half_on_success = true,
        .dice = {1, 10, 0},
        .line = 30,
        .narrow_line = true},
    // A Goliath's Cloud's Jaunt (SRD 5.2.1 p. 85), aimed as "clouds_jaunt"
    // with a use of Giant Ancestry. No one learns it as a spell.
    SpellDef{
        .id = "clouds_jaunt",
        .label = "Cloud's Jaunt",
        .level = 0,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::area,
        .range = 30,
        .bonus_action = true,
        .rider = Rider::misty_step,
        .area = 5},
    // Land's Aid (SRD 5.2.1 p. 46), aimed by a Circle of the Land Druid as
    // "lands_aid" with a use of Wild Shape. No one learns it as a spell.
    SpellDef{
        .id = "lands_aid",
        .label = "Land's Aid",
        .level = 0,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::area,
        .range = 60,
        .save = Ability::constitution,
        .half_on_success = true,
        .damage = DamageType::necrotic,
        .dice = {2, 6, 0},
        .radius = 10},
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
    // SRD 5.2.1 p. 138: a 60-foot Line of wind; each creature in it, and each
    // ending its turn there, makes a Strength save or is pushed 15 feet.
    SpellDef{
        .id = "gust_of_wind",
        .label = "Gust of Wind",
        .level = 2,
        .pattern = SpellPattern::save_condition,
        .target = SpellTarget::area,
        .range = 60,
        .rider = Rider::gust_of_wind,
        .concentration = true,
        .line = 60},
    // SRD 5.2.1 p. 163: a spell attack for 1d8 of a chosen type, offered as
    // "sorcerous_burst_<type>"; each 8 rolls another d8, up to the
    // spellcasting modifier in extra dice.
    SpellDef{
        .id = "sorcerous_burst",
        .label = "Sorcerous Burst",
        .level = 0,
        .pattern = SpellPattern::spell_attack,
        .target = SpellTarget::enemy,
        .range = 120,
        .dice = {1, 8, 0}},
    // SRD 5.2.1 p. 140: a curse adding 1d6 Necrotic to the caster's attack-roll
    // hits; moved after the target drops as a later Bonus Action. The ability
    // check Disadvantage has no use here.
    SpellDef{
        .id = "hex",
        .label = "Hex",
        .level = 1,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::enemy,
        .range = 90,
        .bonus_action = true,
        .requires_sight = true,
        .rider = Rider::hex,
        .concentration = true},
    // SRD 5.2.1 p. 140: a Reaction to taking damage from a creature within 60
    // feet: it makes a Dexterity save against 2d10 Fire, half on a success.
    SpellDef{
        .id = "hellish_rebuke",
        .label = "Hellish Rebuke",
        .level = 1,
        .pattern = SpellPattern::reaction,
        .target = SpellTarget::enemy,
        .range = 60,
        .save = Ability::dexterity,
        .half_on_success = true,
        .damage = DamageType::fire,
        .dice = {2, 10, 0},
        .upcast = {.extra_dice = 1}},
    // SRD 5.2.1 p. 171: a Wisdom save against 1d6 Psychic; a failure also
    // gives Disadvantage on the target's next attack roll.
    SpellDef{
        .id = "vicious_mockery",
        .label = "Vicious Mockery",
        .level = 0,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::enemy,
        .range = 60,
        .requires_sight = true,
        .save = Ability::wisdom,
        .damage = DamageType::psychic,
        .dice = {1, 6, 0},
        .rider = Rider::vicious_mockery},
    // SRD 5.2.1 p. 165: a spell attack for 1d8 Radiant; the target cannot be
    // Invisible until the end of the caster's next turn.
    SpellDef{
        .id = "starry_wisp",
        .label = "Starry Wisp",
        .level = 0,
        .pattern = SpellPattern::spell_attack,
        .target = SpellTarget::enemy,
        .range = 60,
        .damage = DamageType::radiant,
        .dice = {1, 8, 0},
        .rider = Rider::starry_wisp},
    // SRD 5.2.1 p. 124: a Wisdom save against 3d6 Psychic, half on a success;
    // a failure spends the target's Reaction fleeing from the caster.
    SpellDef{
        .id = "dissonant_whispers",
        .label = "Dissonant Whispers",
        .level = 1,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::enemy,
        .range = 60,
        .requires_sight = true,
        .save = Ability::wisdom,
        .half_on_success = true,
        .damage = DamageType::psychic,
        .dice = {3, 6, 0},
        .upcast = {.extra_dice = 1},
        .rider = Rider::dissonant_whispers},
    // SRD 5.2.1 p. 129: creatures in a 20-foot cube that fail a Dexterity save
    // are outlined: attackers who see them have Advantage, and they cannot be
    // Invisible.
    SpellDef{
        .id = "faerie_fire",
        .label = "Faerie Fire",
        .level = 1,
        .pattern = SpellPattern::save_condition,
        .target = SpellTarget::area,
        .range = 60,
        .save = Ability::dexterity,
        .rider = Rider::faerie_fire,
        .concentration = true,
        .area = 20},
    // SRD 5.2.1 p. 143: unlocks a door held by a mundane lock. Offered at a
    // locked door while exploring.
    SpellDef{
        .id = "knock",
        .label = "Knock",
        .level = 2,
        .pattern = SpellPattern::exploration,
        .target = SpellTarget::self,
        .range = 60},
    // SRD 5.2.1 p. 163: a creature at 0 Hit Points within 15 feet becomes Stable.
    SpellDef{
        .id = "spare_the_dying",
        .label = "Spare the Dying",
        .level = 0,
        .pattern = SpellPattern::stabilize,
        .target = SpellTarget::dying_ally,
        .range = 15},
    // SRD 5.2.1 p. 108: up to three creatures' Hit Point maximum and current
    // Hit Points rise by 5 for 8 hours, 5 more for each slot level above 2.
    SpellDef{
        .id = "aid",
        .label = "Aid",
        .level = 2,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::any_creature,
        .range = 30,
        .instances = 3,
        .rider = Rider::aid},
    // SRD 5.2.1 p. 144: ends one condition on a touched creature. The game has
    // Blinded, Paralyzed and Poisoned of its four; nothing deafens yet.
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
    // SRD 5.2.1 p. 140: one more creature for each slot level above 1.
    SpellDef{
        .id = "heroism",
        .label = "Heroism",
        .level = 1,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::ally,
        .range = 5,
        .upcast = {.extra_instances = 1},
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
        .bonus_action = true,
        .requires_sight = true,
        .dice = {2, 4, 0},
        .add_casting_modifier = true,
        .upcast = {.extra_dice = 2}},
    // SRD 5.2.1 p. 162: the held Club or Quarterstaff uses the spellcasting
    // ability and a d8 for a minute. Offered only while holding one.
    SpellDef{
        .id = "shillelagh",
        .label = "Shillelagh",
        .level = 0,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::self,
        .range = 5,
        .bonus_action = true,
        .rider = Rider::shillelagh},
    // SRD 5.2.1 p. 157: a flame in the hand; while it lasts, an Action hurls
    // it as a ranged spell attack ("hurl_flame").
    SpellDef{
        .id = "produce_flame",
        .label = "Produce Flame",
        .level = 0,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::self,
        .range = 5,
        .bonus_action = true,
        .rider = Rider::produce_flame},
    // SRD 5.2.1 p. 115: the touched creature's AC is at least 17 for an hour.
    SpellDef{
        .id = "barkskin",
        .label = "Barkskin",
        .level = 2,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::ally,
        .range = 5,
        .bonus_action = true,
        .rider = Rider::barkskin},
    // SRD 5.2.1 p. 131: a fiery blade in hand; while it lasts, an Action makes
    // a melee spell attack with it ("flame_blade_strike") for 3d6 + the
    // spellcasting modifier Fire.
    SpellDef{
        .id = "flame_blade",
        .label = "Flame Blade",
        .level = 2,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::self,
        .range = 5,
        .bonus_action = true,
        .rider = Rider::flame_blade,
        .concentration = true},
    // SRD 5.2.1 p. 150: a 5-foot-radius beam; creatures in it save against
    // 2d10 Radiant, half on a success, at most once a turn: when it appears or
    // moves onto them, and when they enter it or end their turn there.
    SpellDef{
        .id = "moonbeam",
        .label = "Moonbeam",
        .level = 2,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::area,
        .range = 120,
        .save = Ability::constitution,
        .half_on_success = true,
        .damage = DamageType::radiant,
        .dice = {2, 10, 0},
        .rider = Rider::moonbeam,
        .concentration = true,
        .radius = 5},
    // SRD 5.2.1 p. 164: a 20-foot-radius Sphere of Difficult Terrain; a
    // creature takes 2d4 Piercing for every 5 feet it moves into or within it.
    SpellDef{
        .id = "spike_growth",
        .label = "Spike Growth",
        .level = 2,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::area,
        .range = 150,
        .damage = DamageType::piercing,
        .dice = {2, 4, 0},
        .rider = Rider::spike_growth,
        .concentration = true,
        .radius = 20},
    // SRD 5.2.1 p. 139: metal armor a creature wears glows red-hot: 2d8 Fire,
    // then a Constitution save or Disadvantage on attack rolls until the
    // caster's next turn. A Bonus Action on later turns repeats it. Offered
    // only against a creature known to wear metal armor; worn armor cannot be
    // dropped.
    SpellDef{
        .id = "heat_metal",
        .label = "Heat Metal",
        .level = 2,
        .pattern = SpellPattern::save_condition,
        .target = SpellTarget::any_creature,
        .range = 60,
        .requires_sight = true,
        .save = Ability::constitution,
        .damage = DamageType::fire,
        .dice = {2, 8, 0},
        .rider = Rider::heat_metal,
        .concentration = true},
    // SRD 5.2.1 p. 146: a minute's casting, then for an hour a 10-foot-radius
    // circle within 10 feet that one creature type (Celestials, Elementals,
    // Fey, Fiends or Undead) cannot enter, and whose occupants it attacks with
    // Disadvantage. Offered once per type, "magic_circle_fey" and so on.
    SpellDef{
        .id = "magic_circle",
        .label = "Magic Circle",
        .level = 3,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::area,
        .range = 10,
        .rider = Rider::magic_circle,
        .concentration = true,
        .radius = 10},
    // SRD 5.2.1 p. 157: Resistance to one of five types, chosen at the casting.
    SpellDef{
        .id = "protection_from_energy",
        .label = "Protection from Energy",
        .level = 3,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::ally,
        .range = 5,
        .rider = Rider::protection_from_energy,
        .concentration = true},
    // SRD 5.2.1 p. 117: a Large pack of spirits; creatures that come within 10
    // feet of it make a Dexterity save, once a turn, or take the damage.
    SpellDef{
        .id = "conjure_animals",
        .label = "Conjure Animals",
        .level = 3,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::area,
        .range = 60,
        .save = Ability::dexterity,
        .damage = DamageType::slashing,
        .dice = {3, 10, 0},
        .upcast = {.extra_dice = 1},
        .rider = Rider::conjure_animals,
        .concentration = true,
        .area = 10},
    // SRD 5.2.1 p. 174: a wall 50 feet long; its wind turns ordinary
    // projectiles shot across it.
    SpellDef{
        .id = "wind_wall",
        .label = "Wind Wall",
        .level = 3,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::area,
        .range = 120,
        .save = Ability::strength,
        .half_on_success = true,
        .damage = DamageType::bludgeoning,
        .dice = {4, 8, 0},
        .rider = Rider::wind_wall,
        .concentration = true,
        .evocation = true,
        .wall = 50},
    // SRD 5.2.1 p. 148: up to six creatures within 60 feet regain 2d4 + the
    // spellcasting modifier.
    SpellDef{
        .id = "mass_healing_word",
        .label = "Mass Healing Word",
        .level = 3,
        .pattern = SpellPattern::heal,
        .target = SpellTarget::wounded_ally,
        .range = 60,
        .bonus_action = true,
        .requires_sight = true,
        .dice = {2, 4, 0},
        .add_casting_modifier = true,
        .instances = 6,
        .upcast = {.extra_dice = 1}},
    // SRD 5.2.1 p. 164: spirits in a 15-foot Emanation around the caster halve
    // enemies' Speed; one coming into it or ending its turn there makes a
    // Wisdom save, once a turn.
    SpellDef{
        .id = "spirit_guardians",
        .label = "Spirit Guardians",
        .level = 3,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::self,
        .range = 15,
        .save = Ability::wisdom,
        .half_on_success = true,
        .damage = DamageType::radiant,
        .dice = {3, 8, 0},
        .upcast = {.extra_dice = 1},
        .rider = Rider::spirit_guardians,
        .concentration = true},
    // SRD 5.2.1 pp. 112-113: a touched creature that fails a Wisdom save
    // suffers one curse the caster picks, offered as "bestow_curse_<curse>".
    SpellDef{
        .id = "bestow_curse",
        .label = "Bestow Curse",
        .level = 3,
        .pattern = SpellPattern::save_condition,
        .target = SpellTarget::enemy,
        .range = 5,
        .requires_effect_capacity = true,
        .save = Ability::wisdom,
        .rider = Rider::bestow_curse,
        .concentration = true},
    // SRD 5.2.1 p. 108: a minute's casting (LEVELS-1: ten rounds in a fight)
    // raises a Humanoid corpse as a Zombie the caster commands; two more from
    // each slot level above 3.
    SpellDef{
        .id = "animate_dead",
        .label = "Animate Dead",
        .level = 3,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::corpse,
        .range = 10,
        .upcast = {.extra_instances = 2},
        .rider = Rider::animate_dead},
    // SRD 5.2.1 p. 119: a touched creature that fails a Constitution save takes
    // 11d8 Necrotic damage and is Poisoned, with Disadvantage on one ability's
    // saves, offered as "contagion_<ability>".
    SpellDef{
        .id = "contagion",
        .label = "Contagion",
        .level = 5,
        .pattern = SpellPattern::save_condition,
        .target = SpellTarget::enemy,
        .range = 5,
        .requires_effect_capacity = true,
        .save = Ability::constitution,
        .damage = DamageType::necrotic,
        .dice = {11, 8, 0},
        .rider = Rider::contagion},
    // SRD 5.2.1 p. 124: Celestials, Elementals, Fey, Fiends and Undead attack
    // the caster with Disadvantage; it ends early to free a creature from their
    // charm or fear, or to send one of them home.
    SpellDef{
        .id = "dispel_evil_and_good",
        .label = "Dispel Evil and Good",
        .level = 5,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::self,
        .range = 5,
        .rider = Rider::dispel_evil_and_good,
        .concentration = true},
    // SRD 5.2.1 p. 132: a 10-foot-radius column of fire and radiance.
    SpellDef{
        .id = "flame_strike",
        .label = "Flame Strike",
        .level = 5,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::area,
        .range = 60,
        .save = Ability::dexterity,
        .half_on_success = true,
        .damage = DamageType::fire,
        .dice = {5, 6, 0},
        .upcast = {.extra_dice = 1},
        .radius = 10,
        .evocation = true,
        .second_damage = DamageType::radiant,
        .second_dice = {5, 6, 0}},
    // SRD 5.2.1 p. 143: a 20-foot-radius swarm, Difficult Terrain; a creature
    // in it when it appears, entering it or ending its turn there saves, once
    // a turn.
    SpellDef{
        .id = "insect_plague",
        .label = "Insect Plague",
        .level = 5,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::area,
        .range = 300,
        .save = Ability::constitution,
        .half_on_success = true,
        .damage = DamageType::piercing,
        .dice = {4, 10, 0},
        .upcast = {.extra_dice = 1},
        .rider = Rider::insect_plague,
        .concentration = true,
        .radius = 20},
    // SRD 5.2.1 p. 147: up to six creatures in a 30-foot-radius Sphere regain
    // 5d8 + the spellcasting modifier.
    SpellDef{
        .id = "mass_cure_wounds",
        .label = "Mass Cure Wounds",
        .level = 5,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::area,
        .range = 60,
        .dice = {5, 8, 0},
        .add_casting_modifier = true,
        .instances = 6,
        .upcast = {.extra_dice = 1},
        .rider = Rider::mass_cure_wounds,
        .radius = 30},
    // SRD 5.2.1 p. 137: a touch ends one of Charmed, a curse or another
    // lasting harm, offered as "greater_restoration_<harm>".
    SpellDef{
        .id = "greater_restoration",
        .label = "Greater Restoration",
        .level = 5,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::ally,
        .range = 5,
        .rider = Rider::greater_restoration},
    // SRD 5.2.1 p. 157: an hour's casting revives a creature dead up to ten
    // days with 1 Hit Point and a -4 penalty to D20 Tests; cast in camp for an
    // offering of 300 gp times its level (LEVELS-1).
    SpellDef{
        .id = "raise_dead",
        .label = "Raise Dead",
        .level = 5,
        .pattern = SpellPattern::camp,
        .target = SpellTarget::dead_ally,
        .range = 5},
    // SRD 5.2.1 p. 113: a wall of blades 100 feet long, or a ring 60 feet
    // across, Difficult Terrain; a creature in it, entering it or ending its
    // turn there saves, once a turn.
    SpellDef{
        .id = "blade_barrier",
        .label = "Blade Barrier",
        .level = 6,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::area,
        .range = 90,
        .save = Ability::dexterity,
        .half_on_success = true,
        .damage = DamageType::force,
        .dice = {6, 10, 0},
        .rider = Rider::blade_barrier,
        .concentration = true,
        .evocation = true,
        .wall = 100,
        .ring = 60},
    // SRD 5.2.1 p. 121: as Animate Dead, three corpses rise as Ghouls; four
    // from a level-7 slot, five from a level-8 one.
    SpellDef{
        .id = "create_undead",
        .label = "Create Undead",
        .level = 6,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::corpse,
        .range = 10,
        .instances = 3,
        .upcast = {.extra_instances = 1},
        .rider = Rider::create_undead},
    // SRD 5.2.1 p. 139: 14d6 Necrotic damage that lowers the Hit Point
    // maximum as much; half and no lowering on a success.
    SpellDef{
        .id = "harm",
        .label = "Harm",
        .level = 6,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::enemy,
        .range = 60,
        .requires_sight = true,
        .save = Ability::constitution,
        .half_on_success = true,
        .damage = DamageType::necrotic,
        .dice = {14, 6, 0},
        .rider = Rider::harm},
    // SRD 5.2.1 p. 139: 70 Hit Points, 10 more for each slot level above 6,
    // and an end to Blinded and Poisoned.
    SpellDef{
        .id = "heal",
        .label = "Heal",
        .level = 6,
        .pattern = SpellPattern::heal,
        .target = SpellTarget::ally,
        .range = 60,
        .requires_sight = true,
        .dice = {0, 0, 70}},
    // SRD 5.2.1 p. 140: a feast in camp; up to twelve creatures gain Poison
    // Resistance, Immunity to Frightened and Poisoned and 2d10 more Hit Points
    // for a day. Its bowl is free (LEVELS-3).
    SpellDef{
        .id = "heroes_feast",
        .label = "Heroes' Feast",
        .level = 6,
        .pattern = SpellPattern::camp,
        .target = SpellTarget::ally,
        .range = 30,
        .dice = {2, 10, 0},
        .instances = 12},
    // SRD 5.2.1 p. 166: a 5-foot-wide, 60-foot Line; Constitution save or
    // 6d8 Radiant and Blinded until the caster's next turn; a Magic action
    // fires a new Line while it lasts.
    SpellDef{
        .id = "sunbeam",
        .label = "Sunbeam",
        .level = 6,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::area,
        .range = 60,
        .save = Ability::constitution,
        .half_on_success = true,
        .damage = DamageType::radiant,
        .dice = {6, 8, 0},
        .rider = Rider::sunbeam,
        .concentration = true,
        .evocation = true,
        .line = 60,
        .narrow_line = true},
    // SRD 5.2.1 p. 171: Truesight 120 feet for an hour.
    SpellDef{
        .id = "true_seeing",
        .label = "True Seeing",
        .level = 6,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::ally,
        .range = 5,
        .rider = Rider::true_seeing},
    // SRD 5.2.1 p. 175: outside fights, the party returns to a temple it
    // prepared earlier (LEVELS-3).
    SpellDef{
        .id = "word_of_recall",
        .label = "Word of Recall",
        .level = 6,
        .pattern = SpellPattern::camp,
        .target = SpellTarget::self,
        .range = 5,
        .instances = 8},
    // SRD 5.2.1 p. 118: a pillar of light 10 feet in radius; each creature in
    // it regains 4d12 + the spellcasting modifier or saves for 6d12 Radiant.
    // The caster's allies heal and the others burn; it moves with its caster.
    SpellDef{
        .id = "conjure_celestial",
        .label = "Conjure Celestial",
        .level = 7,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::area,
        .range = 90,
        .save = Ability::dexterity,
        .half_on_success = true,
        .damage = DamageType::radiant,
        .dice = {6, 12, 0},
        .upcast = {.extra_dice = 1},
        .rider = Rider::conjure_celestial,
        .concentration = true,
        .radius = 10},
    // SRD 5.2.1 p. 125: a Bonus Action; each foe within 30 feet makes a
    // Charisma save, and by its Hit Points one that fails dies, is Blinded
    // and Stunned, or is Blinded; Deafened is not modeled (DEAF-1).
    SpellDef{
        .id = "divine_word",
        .label = "Divine Word",
        .level = 7,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::self,
        .range = 30,
        .bonus_action = true,
        .rider = Rider::divine_word,
        .evocation = true},
    // SRD 5.2.1 p. 132: up to ten 10-foot cubes within 150 feet, each
    // touching another; Dexterity save, 7d10 Fire, half on a success.
    SpellDef{
        .id = "fire_storm",
        .label = "Fire Storm",
        .level = 7,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::area,
        .range = 150,
        .save = Ability::dexterity,
        .half_on_success = true,
        .damage = DamageType::fire,
        .dice = {7, 10, 0},
        .evocation = true,
        .storm = 10},
    // SRD 5.2.1 p. 158: a minute's casting; the creature regains 4d8 + 15 Hit
    // Points, then 1 at the start of each of its turns for an hour.
    SpellDef{
        .id = "regenerate",
        .label = "Regenerate",
        .level = 7,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::ally,
        .range = 5,
        .dice = {4, 8, 15},
        .rider = Rider::regenerate},
    // SRD 5.2.1 p. 158: an hour's casting in camp; a dead creature returns
    // with all its Hit Points and a -4 penalty that eases with Long Rests.
    SpellDef{
        .id = "resurrection",
        .label = "Resurrection",
        .level = 7,
        .pattern = SpellPattern::camp,
        .target = SpellTarget::dead_ally,
        .range = 5},
    // SRD 5.2.1 p. 109: a 10-foot Emanation where no spell is cast, which no
    // spell reaches, and where ongoing spells are suppressed.
    SpellDef{
        .id = "antimagic_field",
        .label = "Antimagic Field",
        .level = 8,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::self,
        .range = 5,
        .rider = Rider::antimagic_field,
        .concentration = true},
    // SRD 5.2.1 p. 127: a 100-foot-radius tremor, Difficult Terrain; on
    // casting and at the end of each of the caster's turns a creature there
    // saves or falls Prone and loses Concentration. No fissures or collapsing
    // structures.
    SpellDef{
        .id = "earthquake",
        .label = "Earthquake",
        .level = 8,
        .pattern = SpellPattern::save_condition,
        .target = SpellTarget::area,
        .range = 500,
        .save = Ability::dexterity,
        .rider = Rider::earthquake,
        .concentration = true,
        .radius = 100},
    // SRD 5.2.1 p. 141: a 30-foot Emanation; the caster's allies in it have
    // Advantage on saves and attackers Disadvantage against them, and a Fiend
    // or Undead that hits one in melee saves or is Blinded.
    SpellDef{
        .id = "holy_aura",
        .label = "Holy Aura",
        .level = 8,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::self,
        .range = 5,
        .rider = Rider::holy_aura,
        .concentration = true},
    // SRD 5.2.1 p. 167: a 60-foot-radius Sphere within 150 feet; Constitution
    // save, 12d6 Radiant and Blinded for a minute, saving again at the end of
    // each turn; half and no blindness on a success. It ends spell Darkness.
    SpellDef{
        .id = "sunburst",
        .label = "Sunburst",
        .level = 8,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::area,
        .range = 150,
        .save = Ability::constitution,
        .half_on_success = true,
        .damage = DamageType::radiant,
        .dice = {12, 6, 0},
        .rider = Rider::sunburst,
        .radius = 60,
        .evocation = true},
    // SRD 5.2.1 p. 131: a 20-foot-radius Sphere within 150 feet; Dexterity
    // save, 8d6 Fire, half on a success; 1d6 more for each slot level above 3.
    SpellDef{
        .id = "fireball",
        .label = "Fireball",
        .level = 3,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::area,
        .range = 150,
        .save = Ability::dexterity,
        .half_on_success = true,
        .damage = DamageType::fire,
        .dice = {8, 6, 0},
        .upcast = {.extra_dice = 1},
        .radius = 20,
        .evocation = true},
    // SRD 5.2.1 p. 144: a 100-foot, 5-foot-wide Line from the caster;
    // Dexterity save, 8d6 Lightning, half on a success; 1d6 more for each
    // slot level above 3.
    SpellDef{
        .id = "lightning_bolt",
        .label = "Lightning Bolt",
        .level = 3,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::area,
        .range = 100,
        .save = Ability::dexterity,
        .half_on_success = true,
        .damage = DamageType::lightning,
        .dice = {8, 6, 0},
        .upcast = {.extra_dice = 1},
        .evocation = true,
        .line = 100,
        .narrow_line = true},
    // SRD 5.2.1 p. 120: a Reaction to a creature casting a spell within 60
    // feet; it makes a Constitution save or the spell fails, its slot kept.
    SpellDef{
        .id = "counterspell",
        .label = "Counterspell",
        .level = 3,
        .pattern = SpellPattern::reaction,
        .target = SpellTarget::enemy,
        .range = 60,
        .save = Ability::constitution},
    // SRD 5.2.1 p. 139: a willing creature's Speed doubles, +2 AC, Advantage
    // on Dexterity saves and a second, limited action each turn; when it ends,
    // a turn of lethargy.
    SpellDef{
        .id = "haste",
        .label = "Haste",
        .level = 3,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::ally,
        .range = 30,
        .requires_sight = true,
        .requires_effect_capacity = true,
        .rider = Rider::haste,
        .concentration = true},
    // SRD 5.2.1 p. 141: a 30-foot Cube within 120 feet; a Wisdom save or
    // Charmed, Incapacitated and Speed 0 until damaged or shaken awake.
    SpellDef{
        .id = "hypnotic_pattern",
        .label = "Hypnotic Pattern",
        .level = 3,
        .pattern = SpellPattern::save_condition,
        .target = SpellTarget::area,
        .range = 120,
        .save = Ability::wisdom,
        .rider = Rider::hypnotic_pattern,
        .concentration = true,
        .area = 30},
    // SRD 5.2.1 p. 130: a 30-foot Cone; a Wisdom save or Frightened, fleeing
    // the caster with the Dash action.
    SpellDef{
        .id = "fear",
        .label = "Fear",
        .level = 3,
        .pattern = SpellPattern::save_condition,
        .target = SpellTarget::area,
        .range = 30,
        .save = Ability::wisdom,
        .rider = Rider::fear,
        .concentration = true,
        .cone = 30},
    // SRD 5.2.1 p. 163: a 20-foot-radius Cylinder within 150 feet, Heavily
    // Obscured and Difficult Terrain; entering it or starting a turn there
    // calls for a Dexterity save against Prone and lost Concentration.
    SpellDef{
        .id = "sleet_storm",
        .label = "Sleet Storm",
        .level = 3,
        .pattern = SpellPattern::save_condition,
        .target = SpellTarget::area,
        .range = 150,
        .save = Ability::dexterity,
        .rider = Rider::sleet_storm,
        .concentration = true,
        .radius = 20},
    // SRD 5.2.1 p. 165: a 20-foot-radius Sphere within 90 feet, Heavily
    // Obscured; a creature starting its turn there makes a Constitution save
    // or is Poisoned and takes no action or Bonus Action that turn.
    SpellDef{
        .id = "stinking_cloud",
        .label = "Stinking Cloud",
        .level = 3,
        .pattern = SpellPattern::save_condition,
        .target = SpellTarget::area,
        .range = 90,
        .save = Ability::constitution,
        .rider = Rider::stinking_cloud,
        .concentration = true,
        .radius = 20},
    // SRD 5.2.1 p. 171: a melee spell attack, 3d6 Necrotic, and the caster
    // regains half; a Magic action repeats it while it lasts.
    SpellDef{
        .id = "vampiric_touch",
        .label = "Vampiric Touch",
        .level = 3,
        .pattern = SpellPattern::spell_attack,
        .target = SpellTarget::enemy,
        .range = 5,
        .melee = true,
        .damage = DamageType::necrotic,
        .dice = {3, 6, 0},
        .upcast = {.extra_dice = 1},
        .rider = Rider::vampiric_touch,
        .concentration = true},
    // SRD 5.2.1 p. 113: for a minute, a d6 at the end of each of the caster's
    // turns; on 4-6 it vanishes to the Ethereal Plane until its next turn.
    SpellDef{
        .id = "blink",
        .label = "Blink",
        .level = 3,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::self,
        .range = 5,
        .rider = Rider::blink},
    // SRD 5.2.1 p. 169: a dome around the party's camp for 8 hours; a rest
    // inside it cannot be interrupted (LEVELS-1).
    SpellDef{
        .id = "tiny_hut",
        .label = "Tiny Hut",
        .level = 3,
        .pattern = SpellPattern::camp,
        .target = SpellTarget::self,
        .range = 5,
        .instances = 8},
    // SRD 5.2.1 p. 113: a 20-foot square of tentacles within 90 feet,
    // Difficult Terrain; on casting, entering or ending a turn there, once a
    // turn, a Strength save or 3d6 Bludgeoning and Restrained.
    SpellDef{
        .id = "black_tentacles",
        .label = "Black Tentacles",
        .level = 4,
        .pattern = SpellPattern::save_condition,
        .target = SpellTarget::area,
        .range = 90,
        .save = Ability::strength,
        .damage = DamageType::bludgeoning,
        .dice = {3, 6, 0},
        .rider = Rider::black_tentacles,
        .concentration = true,
        .area = 20},
    // SRD 5.2.1 p. 113: Constitution save, 8d8 Necrotic, half on a success; a
    // Plant fails; 1d8 more for each slot level above 4.
    SpellDef{
        .id = "blight",
        .label = "Blight",
        .level = 4,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::enemy,
        .range = 30,
        .requires_sight = true,
        .save = Ability::constitution,
        .half_on_success = true,
        .damage = DamageType::necrotic,
        .dice = {8, 8, 0},
        .upcast = {.extra_dice = 1}},
    // SRD 5.2.1 p. 114: Charm Person for any creature; one more for each slot
    // level above 4.
    SpellDef{
        .id = "charm_monster",
        .label = "Charm Monster",
        .level = 4,
        .pattern = SpellPattern::save_condition,
        .target = SpellTarget::enemy,
        .range = 30,
        .requires_sight = true,
        .requires_effect_capacity = true,
        .save = Ability::wisdom,
        .upcast = {.extra_instances = 1},
        .rider = Rider::charm_person},
    // SRD 5.2.1 p. 117: a 10-foot-radius Sphere within 90 feet; a Wisdom save
    // or no Bonus Actions or Reactions and a d10 for each turn's behavior,
    // saving again at the end of each turn; 5 feet more radius per slot level.
    SpellDef{
        .id = "confusion",
        .label = "Confusion",
        .level = 4,
        .pattern = SpellPattern::save_condition,
        .target = SpellTarget::area,
        .range = 90,
        .save = Ability::wisdom,
        .upcast = {.extra_radius = 5},
        .rider = Rider::confusion,
        .concentration = true,
        .radius = 10},
    // SRD 5.2.1 p. 123: a teleport up to 500 feet, with one willing creature
    // within 5 feet if the caster likes.
    SpellDef{
        .id = "dimension_door",
        .label = "Dimension Door",
        .level = 4,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::area,
        .range = 500,
        .rider = Rider::dimension_door},
    // SRD 5.2.1 p. 132: 10 minutes of Resistance to Cold (warm) or Fire
    // (chill); a creature within 5 feet that hits the caster in melee takes
    // 2d8 Fire or Cold.
    SpellDef{
        .id = "fire_shield",
        .label = "Fire Shield",
        .level = 4,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::self,
        .range = 5,
        .dice = {2, 8, 0},
        .rider = Rider::fire_shield,
        .evocation = true},
    // SRD 5.2.1 p. 137: touch; Invisible, whether or not it attacks.
    SpellDef{
        .id = "greater_invisibility",
        .label = "Greater Invisibility",
        .level = 4,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::ally,
        .range = 5,
        .requires_effect_capacity = true,
        .rider = Rider::greater_invisibility,
        .concentration = true},
    // SRD 5.2.1 p. 141: a 20-foot-radius Cylinder within 300 feet; Dexterity
    // save, 2d10 Bludgeoning and 4d6 Cold, half on a success; Difficult
    // Terrain until the end of the caster's next turn; 1d10 more Bludgeoning
    // for each slot level above 4.
    SpellDef{
        .id = "ice_storm",
        .label = "Ice Storm",
        .level = 4,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::area,
        .range = 300,
        .save = Ability::dexterity,
        .half_on_success = true,
        .damage = DamageType::bludgeoning,
        .dice = {2, 10, 0},
        .upcast = {.extra_dice = 1},
        .rider = Rider::ice_storm,
        .radius = 20,
        .evocation = true,
        .second_damage = DamageType::cold,
        .second_dice = {4, 6, 0}},
    // SRD 5.2.1 p. 152: a Wisdom save against 4d10 Psychic and Disadvantage
    // on attack rolls, again at the end of each of its turns; half and the
    // end on a success; 1d10 more for each slot level above 4.
    SpellDef{
        .id = "phantasmal_killer",
        .label = "Phantasmal Killer",
        .level = 4,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::enemy,
        .range = 120,
        .requires_sight = true,
        .save = Ability::wisdom,
        .half_on_success = true,
        .damage = DamageType::psychic,
        .dice = {4, 10, 0},
        .upcast = {.extra_dice = 1},
        .rider = Rider::phantasmal_killer,
        .concentration = true},
    // SRD 5.2.1 p. 153: a Wisdom save or a creature becomes one of the game's
    // Beast forms, with Temporary Hit Points of the Beast's Hit Points.
    SpellDef{
        .id = "polymorph",
        .label = "Polymorph",
        .level = 4,
        .pattern = SpellPattern::save_condition,
        .target = SpellTarget::any_creature,
        .range = 60,
        .requires_sight = true,
        .requires_effect_capacity = true,
        .save = Ability::wisdom,
        .rider = Rider::polymorph,
        .concentration = true},
    // SRD 5.2.1 p. 158: a Large or smaller creature, a Dexterity save or sealed
    // in a sphere nothing passes through.
    SpellDef{
        .id = "resilient_sphere",
        .label = "Resilient Sphere",
        .level = 4,
        .pattern = SpellPattern::save_condition,
        .target = SpellTarget::any_creature,
        .range = 30,
        .requires_effect_capacity = true,
        .save = Ability::dexterity,
        .rider = Rider::resilient_sphere,
        .concentration = true},
    // SRD 5.2.1 p. 172: a 20-foot-radius Sphere within 150 feet; Dexterity
    // save, 10d4 Acid and 5d4 more at the end of its next turn, half the first
    // only on a success; 2d4 more for each slot level above 4.
    SpellDef{
        .id = "vitriolic_sphere",
        .label = "Vitriolic Sphere",
        .level = 4,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::area,
        .range = 150,
        .save = Ability::dexterity,
        .half_on_success = true,
        .damage = DamageType::acid,
        .dice = {10, 4, 0},
        .upcast = {.extra_dice = 2},
        .rider = Rider::vitriolic_sphere,
        .radius = 20,
        .evocation = true},
    // SRD 5.2.1 p. 172: a wall of fire 60 feet long or a ring 20 feet across;
    // Dexterity save, 5d8 Fire, half on a success, as it appears; its burning
    // side scorches a creature ending its turn within 10 feet, and one
    // entering or ending its turn in the wall; 1d8 more per slot level.
    SpellDef{
        .id = "wall_of_fire",
        .label = "Wall of Fire",
        .level = 4,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::area,
        .range = 120,
        .save = Ability::dexterity,
        .half_on_success = true,
        .damage = DamageType::fire,
        .dice = {5, 8, 0},
        .upcast = {.extra_dice = 1},
        .rider = Rider::wall_of_fire,
        .concentration = true,
        .evocation = true,
        .wall = 60,
        .ring = 20},
    // SRD 5.2.1 p. 129: a phantom watchdog in a free square within 30 feet
    // for 8 hours; at the start of each of the caster's turns it bites an
    // enemy within 5 feet (Dexterity save or 4d8 Force); a Magic action moves
    // it 50 feet (LEVELS-4).
    SpellDef{
        .id = "faithful_hound",
        .label = "Faithful Hound",
        .level = 4,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::area,
        .range = 30,
        .save = Ability::dexterity,
        .damage = DamageType::force,
        .dice = {4, 8, 0},
        .rider = Rider::faithful_hound},
    // SRD 5.2.1 p. 118: a 15-foot Emanation; the caster's hits on a creature
    // in it deal 2d8 more Acid, Cold, Fire or Lightning, chosen when it is
    // cast (LEVELS-4); Difficult Terrain for enemies; 1d8 more per slot level.
    SpellDef{
        .id = "conjure_minor_elementals",
        .label = "Conjure Minor Elementals",
        .level = 4,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::self,
        .range = 5,
        .damage = DamageType::fire,
        .dice = {2, 8, 0},
        .upcast = {.extra_dice = 1},
        .rider = Rider::conjure_minor_elementals,
        .concentration = true},
    // SRD 5.2.1 p. 110: a Large hand of force in a free square within 120 feet;
    // as it appears, and with a Bonus Action later, it moves up to 60 feet and
    // strikes (Clenched Fist, 5d8 Force) or pushes (Forceful Hand); 2d8 more
    // Force per slot level above 5.
    SpellDef{
        .id = "arcane_hand",
        .label = "Arcane Hand",
        .level = 5,
        .pattern = SpellPattern::spell_attack,
        .target = SpellTarget::area,
        .range = 120,
        .melee = true,
        .damage = DamageType::force,
        .dice = {5, 8, 0},
        .upcast = {.extra_dice = 2},
        .rider = Rider::arcane_hand,
        .concentration = true,
        .evocation = true},
    // SRD 5.2.1 p. 116: a 20-foot-radius Sphere of poison fog within 120 feet,
    // Heavily Obscured; a Constitution save against 5d8 Poison, half on a
    // success, on casting, when it moves onto a creature, when one enters it or
    // ends its turn there, once a turn; it drifts 10 feet away from the caster
    // at the start of each of its turns; 1d8 more per slot level above 5.
    SpellDef{
        .id = "cloudkill",
        .label = "Cloudkill",
        .level = 5,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::area,
        .range = 120,
        .save = Ability::constitution,
        .half_on_success = true,
        .damage = DamageType::poison,
        .dice = {5, 8, 0},
        .upcast = {.extra_dice = 1},
        .rider = Rider::cloudkill,
        .concentration = true,
        .radius = 20},
    // SRD 5.2.1 p. 117: a 60-foot Cone; Constitution save, 8d8 Cold, half on
    // a success; 1d8 more per slot level above 5.
    SpellDef{
        .id = "cone_of_cold",
        .label = "Cone of Cold",
        .level = 5,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::area,
        .range = 60,
        .save = Ability::constitution,
        .half_on_success = true,
        .damage = DamageType::cold,
        .dice = {8, 8, 0},
        .upcast = {.extra_dice = 1},
        .cone = 60,
        .evocation = true},
    // SRD 5.2.1 p. 118: a Large spirit of air, earth, fire or water in a free
    // space within 60 feet; a creature entering its space or starting its turn
    // within 5 feet of it saves (Dexterity) against 8d8 and Restrained, when
    // it holds no one; the held creature saves again at the start of its
    // turns, 4d8 on a failure; 1d8 more per slot level above 5.
    SpellDef{
        .id = "conjure_elemental",
        .label = "Conjure Elemental",
        .level = 5,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::area,
        .range = 60,
        .save = Ability::dexterity,
        .damage = DamageType::lightning,
        .dice = {8, 8, 0},
        .upcast = {.extra_dice = 1},
        .rider = Rider::conjure_elemental,
        .concentration = true},
    // SRD 5.2.1 p. 126: Dominate Beast for a Humanoid.
    SpellDef{
        .id = "dominate_person",
        .label = "Dominate Person",
        .level = 5,
        .pattern = SpellPattern::save_condition,
        .target = SpellTarget::enemy,
        .range = 60,
        .requires_sight = true,
        .requires_effect_capacity = true,
        .save = Ability::wisdom,
        .rider = Rider::dominate_person,
        .concentration = true,
        .humanoid_only = true},
    // SRD 5.2.1 p. 140: Hold Person for any creature; one more per slot level
    // above 5.
    SpellDef{
        .id = "hold_monster",
        .label = "Hold Monster",
        .level = 5,
        .pattern = SpellPattern::save_condition,
        .target = SpellTarget::enemy,
        .range = 90,
        .requires_sight = true,
        .requires_effect_capacity = true,
        .save = Ability::wisdom,
        .upcast = {.extra_instances = 1},
        .rider = Rider::hold_person,
        .concentration = true},
    // SRD 5.2.1 p. 166: a Draconic Spirit in a free space within 60 feet,
    // fighting for the caster; its breath and the caster's shared Resistance
    // are of one type, chosen at the casting.
    SpellDef{
        .id = "summon_dragon",
        .label = "Summon Dragon",
        .level = 5,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::area,
        .range = 60,
        .rider = Rider::summon_dragon,
        .concentration = true},
    // The Draconic Spirit's Breath Weapon: a 30-foot Cone, Dexterity save
    // against 2d6 of its type, half on a success.
    SpellDef{
        .id = "spirit_breath",
        .label = "Breath Weapon",
        .level = 0,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::area,
        .range = 30,
        .save = Ability::dexterity,
        .half_on_success = true,
        .damage = DamageType::fire,
        .dice = {2, 6, 0},
        .cone = 30},
    // SRD 5.2.1 p. 172: an invisible wall of force 100 feet long, or a ring 20
    // feet across (LEVELS-3); nothing passes through it.
    SpellDef{
        .id = "wall_of_force",
        .label = "Wall of Force",
        .level = 5,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::area,
        .range = 120,
        .rider = Rider::wall_of_force,
        .concentration = true,
        .evocation = true,
        .wall = 100,
        .ring = 20},
    // SRD 5.2.1 p. 173: a wall of stone 100 feet long; it blocks movement,
    // sight and every effect.
    SpellDef{
        .id = "wall_of_stone",
        .label = "Wall of Stone",
        .level = 5,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::area,
        .range = 120,
        .rider = Rider::wall_of_stone,
        .concentration = true,
        .evocation = true,
        .wall = 100},
    // SRD 5.2.1 p. 108: as many Medium objects as the spellcasting modifier
    // rise beside the caster (LEVELS-1), fighting for it.
    SpellDef{
        .id = "animate_objects",
        .label = "Animate Objects",
        .level = 5,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::self,
        .range = 5,
        .rider = Rider::animate_objects,
        .concentration = true},
    // SRD 5.2.1 p. 150: the caster turns Invisible and an illusory double
    // stands in its place (LEVELS-1).
    SpellDef{
        .id = "mislead",
        .label = "Mislead",
        .level = 5,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::self,
        .range = 5,
        .rider = Rider::mislead,
        .concentration = true},
    // SRD 5.2.1 p. 167: a Strength save or a Huge or smaller creature within
    // 60 feet moves up to 30 feet where the caster likes and is Restrained
    // until the end of the caster's next turn; a Magic action repeats it.
    SpellDef{
        .id = "telekinesis",
        .label = "Telekinesis",
        .level = 5,
        .pattern = SpellPattern::save_condition,
        .target = SpellTarget::any_creature,
        .range = 60,
        .requires_sight = true,
        .requires_effect_capacity = true,
        .save = Ability::strength,
        .rider = Rider::telekinesis,
        .concentration = true},
    // SRD 5.2.1 p. 114: a creature within 150 feet and up to three others
    // within 30 feet of it, chosen; each makes a Dexterity save against 10d8
    // Lightning, half on a success; one more bolt per slot level above 6.
    SpellDef{
        .id = "chain_lightning",
        .label = "Chain Lightning",
        .level = 6,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::enemy,
        .range = 150,
        .requires_sight = true,
        .save = Ability::dexterity,
        .half_on_success = true,
        .damage = DamageType::lightning,
        .dice = {10, 8, 0},
        .instances = 4,
        .upcast = {.extra_instances = 1},
        .evocation = true},
    // SRD 5.2.1 p. 115: a 60-foot-radius Sphere within 150 feet;
    // Constitution save, 8d8 Necrotic, half on a success; 2d8 more per slot
    // level above 6.
    SpellDef{
        .id = "circle_of_death",
        .label = "Circle of Death",
        .level = 6,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::area,
        .range = 150,
        .save = Ability::constitution,
        .half_on_success = true,
        .damage = DamageType::necrotic,
        .dice = {8, 8, 0},
        .upcast = {.extra_dice = 2},
        .radius = 60},
    // SRD 5.2.1 p. 119: in camp, a spell of level 5 or lower that can target
    // the caster waits for a trigger from a fixed menu (LEVELS-3): dropping to
    // half Hit Points, a fight starting, or dropping to 0.
    SpellDef{
        .id = "contingency",
        .label = "Contingency",
        .level = 6,
        .pattern = SpellPattern::camp,
        .target = SpellTarget::self,
        .range = 5},
    // SRD 5.2.1 p. 124: a Dexterity save or 10d6 + 40 Force; a creature
    // reduced to 0 Hit Points turns to dust; 3d6 more per slot level above 6.
    SpellDef{
        .id = "disintegrate",
        .label = "Disintegrate",
        .level = 6,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::enemy,
        .range = 60,
        .requires_sight = true,
        .save = Ability::dexterity,
        .damage = DamageType::force,
        .dice = {10, 6, 40},
        .upcast = {.extra_dice = 3},
        .rider = Rider::disintegrate},
    // SRD 5.2.1 p. 129: a creature within 60 feet makes a Wisdom save or is
    // put to sleep, panicked or sickened, as chosen; a Magic action targets
    // another on later turns.
    SpellDef{
        .id = "eyebite",
        .label = "Eyebite",
        .level = 6,
        .pattern = SpellPattern::save_condition,
        .target = SpellTarget::enemy,
        .range = 60,
        .requires_sight = true,
        .requires_effect_capacity = true,
        .save = Ability::wisdom,
        .rider = Rider::eyebite,
        .concentration = true},
    // SRD 5.2.1 p. 133: a Constitution save or Restrained, saving again at
    // the end of each turn: three successes end it, three failures Petrify.
    SpellDef{
        .id = "flesh_to_stone",
        .label = "Flesh to Stone",
        .level = 6,
        .pattern = SpellPattern::save_condition,
        .target = SpellTarget::enemy,
        .range = 60,
        .requires_sight = true,
        .requires_effect_capacity = true,
        .save = Ability::constitution,
        .rider = Rider::flesh_to_stone,
        .concentration = true},
    // SRD 5.2.1 p. 134: a 60-foot-radius Sphere within 300 feet; Constitution
    // save, 10d6 Cold, half on a success; 1d6 more per slot level above 6.
    SpellDef{
        .id = "freezing_sphere",
        .label = "Freezing Sphere",
        .level = 6,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::area,
        .range = 300,
        .save = Ability::constitution,
        .half_on_success = true,
        .damage = DamageType::cold,
        .dice = {10, 6, 0},
        .upcast = {.extra_dice = 1},
        .radius = 60,
        .evocation = true},
    // SRD 5.2.1 p. 136: a still 10-foot Emanation; spells of level 5 or lower
    // (one more per slot level above 6) cast from outside do not affect what
    // is inside.
    SpellDef{
        .id = "globe_of_invulnerability",
        .label = "Globe of Invulnerability",
        .level = 6,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::self,
        .range = 5,
        .rider = Rider::globe_of_invulnerability,
        .concentration = true},
    // SRD 5.2.1 p. 143: a Wisdom save or Charmed and dancing: Speed 0,
    // Disadvantage on Dexterity saves and attack rolls, attackers with
    // Advantage; an action repeats the save. A success dances one turn.
    SpellDef{
        .id = "irresistible_dance",
        .label = "Irresistible Dance",
        .level = 6,
        .pattern = SpellPattern::save_condition,
        .target = SpellTarget::enemy,
        .range = 30,
        .requires_sight = true,
        .requires_effect_capacity = true,
        .save = Ability::wisdom,
        .rider = Rider::irresistible_dance,
        .concentration = true},
    // SRD 5.2.1 p. 146: the caster's soul possesses a Humanoid within 100
    // feet that fails a Charisma save, working as Dominate Person while its
    // body lies helpless (LEVELS-3).
    SpellDef{
        .id = "magic_jar",
        .label = "Magic Jar",
        .level = 6,
        .pattern = SpellPattern::save_condition,
        .target = SpellTarget::enemy,
        .range = 100,
        .requires_sight = true,
        .requires_effect_capacity = true,
        .save = Ability::charisma,
        .rider = Rider::magic_jar,
        .humanoid_only = true},
    // SRD 5.2.1 p. 172: a wall of ice 100 feet long or a ring 20 feet across;
    // a creature in its way is pushed aside and saves (Dexterity) against
    // 10d6 Cold; 2d6 more per slot level above 6.
    SpellDef{
        .id = "wall_of_ice",
        .label = "Wall of Ice",
        .level = 6,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::area,
        .range = 120,
        .save = Ability::dexterity,
        .half_on_success = true,
        .damage = DamageType::cold,
        .dice = {10, 6, 0},
        .upcast = {.extra_dice = 2},
        .rider = Rider::wall_of_ice,
        .concentration = true,
        .evocation = true,
        .wall = 100,
        .ring = 20},
    // SRD 5.2.1 p. 110: a spectral sword within 90 feet makes a melee spell
    // attack against a creature beside it for 4d12 + the spellcasting
    // modifier Force; later, a Bonus Action moves it 30 feet and attacks.
    SpellDef{
        .id = "arcane_sword",
        .label = "Arcane Sword",
        .level = 7,
        .pattern = SpellPattern::spell_attack,
        .target = SpellTarget::enemy,
        .range = 95,
        .melee = true,
        .damage = DamageType::force,
        .dice = {4, 12, 0},
        .add_casting_modifier = true,
        .rider = Rider::arcane_sword,
        .concentration = true,
        .evocation = true},
    // SRD 5.2.1 p. 122: a bead within 150 feet bursts in a 20-foot-radius
    // Sphere when the spell ends; Dexterity save, 12d6 Fire, half on a
    // success, 1d6 more for each of the caster's turns it waited and each
    // slot level above 7.
    SpellDef{
        .id = "delayed_blast_fireball",
        .label = "Delayed Blast Fireball",
        .level = 7,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::area,
        .range = 150,
        .save = Ability::dexterity,
        .half_on_success = true,
        .damage = DamageType::fire,
        .dice = {12, 6, 0},
        .upcast = {.extra_dice = 1},
        .rider = Rider::delayed_blast_fireball,
        .concentration = true,
        .radius = 20,
        .evocation = true},
    // SRD 5.2.1 p. 131: a Constitution save against 7d8 + 30 Necrotic, half
    // on a success; a Humanoid it kills rises as the caster's Zombie.
    SpellDef{
        .id = "finger_of_death",
        .label = "Finger of Death",
        .level = 7,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::enemy,
        .range = 60,
        .requires_sight = true,
        .save = Ability::constitution,
        .half_on_success = true,
        .damage = DamageType::necrotic,
        .dice = {7, 8, 30},
        .rider = Rider::finger_of_death},
    // SRD 5.2.1 p. 134: a 20-foot cage or a 10-foot box of force within 100
    // feet; the creatures inside cannot leave it but by teleporting after a
    // Charisma save. The box also stops spells and attacks.
    SpellDef{
        .id = "forcecage",
        .label = "Forcecage",
        .level = 7,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::area,
        .range = 100,
        .rider = Rider::forcecage,
        .concentration = true,
        .area = 20},
    // SRD 5.2.1 p. 154: a 60-foot Cone; each creature makes a Dexterity save
    // against a random ray: 12d6 Fire, Acid, Lightning, Poison or Cold (half
    // on a success), indigo's turning to stone, violet's blinding banishment,
    // or two rays.
    SpellDef{
        .id = "prismatic_spray",
        .label = "Prismatic Spray",
        .level = 7,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::area,
        .range = 60,
        .save = Ability::dexterity,
        .half_on_success = true,
        .damage = DamageType::fire,
        .dice = {12, 6, 0},
        .rider = Rider::prismatic_spray,
        .cone = 60,
        .evocation = true},
    // SRD 5.2.1 p. 159: a 50-foot-radius Cylinder within 100 feet; each
    // creature that fails a Dexterity save falls upward, out of melee reach,
    // and falls back when the spell ends (LEVELS-3).
    SpellDef{
        .id = "reverse_gravity",
        .label = "Reverse Gravity",
        .level = 7,
        .pattern = SpellPattern::save_condition,
        .target = SpellTarget::area,
        .range = 100,
        .save = Ability::dexterity,
        .rider = Rider::reverse_gravity,
        .concentration = true,
        .radius = 50},
    // SRD 5.2.1 p. 114: a storm cloud over the caster; a bolt strikes a point
    // under it, within 60 feet of the caster: Dexterity save, 3d10 Lightning
    // within 5 feet, half on a success; a Magic action calls another; 1d10
    // more per slot level above 3.
    SpellDef{
        .id = "call_lightning",
        .label = "Call Lightning",
        .level = 3,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::area,
        .range = 60,
        .save = Ability::dexterity,
        .half_on_success = true,
        .damage = DamageType::lightning,
        .dice = {3, 10, 0},
        .upcast = {.extra_dice = 1},
        .rider = Rider::call_lightning,
        .concentration = true,
        .radius = 5},
    // SRD 5.2.1 p. 109: a 10-foot Emanation no creature but a Construct or an
    // Undead passes or reaches through.
    SpellDef{
        .id = "antilife_shell",
        .label = "Antilife Shell",
        .level = 5,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::self,
        .range = 5,
        .rider = Rider::antilife_shell,
        .concentration = true},
    // SRD 5.2.1 p. 135: a giant centipede, spider or wasp, chosen at the
    // casting, fights for the caster; its stat block grows with the slot.
    SpellDef{
        .id = "giant_insect",
        .label = "Giant Insect",
        .level = 4,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::area,
        .range = 60,
        .rider = Rider::giant_insect,
        .concentration = true},
    // SRD 5.2.1 p. 118: a fey spirit appears and makes a melee spell attack
    // for 3d12 + the spellcasting modifier Psychic, frightening the target
    // until the caster's next turn; later a Bonus Action teleports it 30 feet
    // and attacks again; 1d12 more per slot level above 6.
    SpellDef{
        .id = "conjure_fey",
        .label = "Conjure Fey",
        .level = 6,
        .pattern = SpellPattern::spell_attack,
        .target = SpellTarget::enemy,
        .range = 65,
        .melee = true,
        .damage = DamageType::psychic,
        .dice = {3, 12, 0},
        .add_casting_modifier = true,
        .upcast = {.extra_dice = 1},
        .rider = Rider::conjure_fey,
        .concentration = true},
    // SRD 5.2.1 p. 173: a wall of thorns 60 feet long or a ring 20 feet
    // across that blocks sight; Dexterity save, 7d8 Piercing as it rises, half
    // on a success; its squares cost 4 feet of movement a foot, and entering or
    // ending a turn in it is a Dexterity save against 7d8 Slashing, once a
    // turn; 1d8 more per slot level above 6.
    SpellDef{
        .id = "wall_of_thorns",
        .label = "Wall of Thorns",
        .level = 6,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::area,
        .range = 120,
        .save = Ability::dexterity,
        .half_on_success = true,
        .damage = DamageType::piercing,
        .dice = {7, 8, 0},
        .upcast = {.extra_dice = 1},
        .rider = Rider::wall_of_thorns,
        .concentration = true,
        .wall = 60,
        .ring = 20},
    // SRD 5.2.1 p. 108: willing creatures within 30 feet take one Beast form,
    // chosen at the casting, keeping their own Hit Points, with Temporary Hit
    // Points equal to the form's; it lasts the fight (LEVELS-3).
    SpellDef{
        .id = "animal_shapes",
        .label = "Animal Shapes",
        .level = 8,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::ally,
        .range = 30,
        .instances = 12,
        .rider = Rider::animal_shapes},
    // Nature's Sanctuary (SRD 5.2.1 p. 46), aimed by a Land Druid of 14 with a
    // use of Wild Shape: a 15-foot cube of spectral trees within 120 feet for a
    // minute. No one learns it as a spell.
    SpellDef{
        .id = "natures_sanctuary",
        .label = "Nature's Sanctuary",
        .level = 0,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::area,
        .range = 120,
        .rider = Rider::natures_sanctuary,
        .area = 15},
    // SRD 5.2.1 p. 162: in camp, twelve hours make a lasting copy of a member
    // with half its Hit Points, which joins the party (LEVELS-3).
    SpellDef{
        .id = "simulacrum",
        .label = "Simulacrum",
        .level = 7,
        .pattern = SpellPattern::camp,
        .target = SpellTarget::ally,
        .range = 5},
    // SRD 5.2.1 p. 112: an Intelligence save against 10d12 Psychic, half on a
    // success; a failure also stops the creature's spells and Magic actions.
    SpellDef{
        .id = "befuddlement",
        .label = "Befuddlement",
        .level = 8,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::enemy,
        .range = 150,
        .requires_sight = true,
        .save = Ability::intelligence,
        .half_on_success = true,
        .damage = DamageType::psychic,
        .dice = {10, 12, 0},
        .rider = Rider::befuddlement},
    // SRD 5.2.1 p. 125: Dominate Person for any creature.
    SpellDef{
        .id = "dominate_monster",
        .label = "Dominate Monster",
        .level = 8,
        .pattern = SpellPattern::save_condition,
        .target = SpellTarget::enemy,
        .range = 60,
        .requires_sight = true,
        .requires_effect_capacity = true,
        .save = Ability::wisdom,
        .rider = Rider::dominate_monster,
        .concentration = true},
    // SRD 5.2.1 p. 142: a 20-foot-radius cloud within 150 feet, Heavily
    // Obscured; Dexterity save, 10d8 Fire, half on a success, when it appears
    // and for a creature it reaches or that ends its turn in it; it drifts 10
    // feet away from the caster each turn.
    SpellDef{
        .id = "incendiary_cloud",
        .label = "Incendiary Cloud",
        .level = 8,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::area,
        .range = 150,
        .save = Ability::dexterity,
        .half_on_success = true,
        .damage = DamageType::fire,
        .dice = {10, 8, 0},
        .rider = Rider::incendiary_cloud,
        .concentration = true,
        .radius = 20},
    // SRD 5.2.1 p. 148: no save; the creature is lost in a maze until a DC 20
    // Intelligence (Investigation) check or the spell's end.
    SpellDef{
        .id = "maze",
        .label = "Maze",
        .level = 8,
        .pattern = SpellPattern::save_condition,
        .target = SpellTarget::enemy,
        .range = 60,
        .requires_sight = true,
        .requires_effect_capacity = true,
        .save = Ability::intelligence,
        .rider = Rider::maze,
        .concentration = true},
    // SRD 5.2.1 p. 149: for 24 hours, Immunity to Psychic damage and the
    // Charmed condition, and no mind control.
    SpellDef{
        .id = "mind_blank",
        .label = "Mind Blank",
        .level = 8,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::ally,
        .range = 5,
        .rider = Rider::mind_blank},
    // SRD 5.2.1 p. 154: no save; a creature with 150 Hit Points or fewer is
    // Stunned, saving (Constitution) at the end of each of its turns; one with
    // more has Speed 0 until the caster's next turn.
    SpellDef{
        .id = "power_word_stun",
        .label = "Power Word Stun",
        .level = 8,
        .pattern = SpellPattern::save_condition,
        .target = SpellTarget::enemy,
        .range = 60,
        .requires_sight = true,
        .requires_effect_capacity = true,
        .save = Ability::constitution,
        .rider = Rider::power_word_stun},
    // SRD 5.2.1 p. 165: Resistance to Bludgeoning, Piercing and Slashing damage.
    SpellDef{
        .id = "stoneskin",
        .label = "Stoneskin",
        .level = 4,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::ally,
        .range = 5,
        .rider = Rider::stoneskin,
        .concentration = true},
    // SRD 5.2.1 p. 118: spirits in a 10-foot Emanation around the caster; a
    // creature that comes into it or ends its turn there makes a Wisdom save,
    // once a turn.
    SpellDef{
        .id = "conjure_woodland_beings",
        .label = "Conjure Woodland Beings",
        .level = 4,
        .pattern = SpellPattern::save_damage,
        .target = SpellTarget::self,
        .range = 10,
        .save = Ability::wisdom,
        .half_on_success = true,
        .damage = DamageType::force,
        .dice = {5, 8, 0},
        .upcast = {.extra_dice = 1},
        .rider = Rider::conjure_woodland_beings,
        .concentration = true},
    // SRD 5.2.1 p. 125: a Beast that fails a Wisdom save fights for the caster,
    // acting right after it (LEVELS-1), saving again whenever it takes damage.
    SpellDef{
        .id = "dominate_beast",
        .label = "Dominate Beast",
        .level = 4,
        .pattern = SpellPattern::save_condition,
        .target = SpellTarget::enemy,
        .range = 60,
        .requires_sight = true,
        .requires_effect_capacity = true,
        .save = Ability::wisdom,
        .rider = Rider::dominate_beast,
        .concentration = true},
    // SRD 5.2.1 p. 153: Overgrowth only; Enrichment has nothing to act on.
    SpellDef{
        .id = "plant_growth",
        .label = "Plant Growth",
        .level = 3,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::area,
        .range = 150,
        .rider = Rider::plant_growth,
        .radius = 100},
    // SRD 5.2.1 p. 159: a creature dead no more than a minute returns with 1
    // Hit Point. The offering is 300 gp times its level (LEVELS-1); the reach
    // is 10 feet, not touch (LEVELS-2).
    SpellDef{
        .id = "revivify",
        .label = "Revivify",
        .level = 3,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::dead_ally,
        .range = 10,
        .rider = Rider::revivify},
    // SRD 5.2.1 p. 158: touch; every curse on the creature ends. Hex is the
    // game's one curse so far (SRD 5.2.1 p. 141: "You place a curse").
    SpellDef{
        .id = "remove_curse",
        .label = "Remove Curse",
        .level = 3,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::any_creature,
        .range = 5,
        .rider = Rider::remove_curse},
    // SRD 5.2.1 p. 112: Advantage on Wisdom saves and Death Saving Throws, and
    // healing restores its maximum, for creatures within 30 feet. The game
    // takes every ally in range.
    SpellDef{
        .id = "beacon_of_hope",
        .label = "Beacon of Hope",
        .level = 3,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::self,
        .range = 30,
        .rider = Rider::beacon_of_hope,
        .concentration = true},
    // SRD 5.2.1 p. 124: spells of level 3 or lower on a creature or an area
    // within 120 feet end; a higher spell needs a spellcasting check against
    // 10 + its level, unless the slot is as high. Aimed at one square.
    SpellDef{
        .id = "dispel_magic",
        .label = "Dispel Magic",
        .level = 3,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::area,
        .range = 120,
        .rider = Rider::dispel_magic,
        .area = 5},
    // SRD 5.2.1 p. 122: 8 hours; the first drop to 0 Hit Points leaves 1.
    SpellDef{
        .id = "death_ward",
        .label = "Death Ward",
        .level = 4,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::ally,
        .range = 5,
        .rider = Rider::death_ward},
    // SRD 5.2.1 p. 111: a 30-foot Emanation; allies in it resist Necrotic
    // damage, and one at 0 Hit Points regains 1 as its turn starts.
    SpellDef{
        .id = "aura_of_life",
        .label = "Aura of Life",
        .level = 4,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::self,
        .range = 30,
        .rider = Rider::aura_of_life,
        .concentration = true},
    // SRD 5.2.1 p. 134: 1 hour, touch; Difficult Terrain costs nothing extra,
    // and magic cannot slow, Paralyze or Restrain the creature.
    SpellDef{
        .id = "freedom_of_movement",
        .label = "Freedom of Movement",
        .level = 4,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::ally,
        .range = 5,
        .rider = Rider::freedom_of_movement},
    // SRD 5.2.1 p. 137: a spectral guardian in an unoccupied square within 30
    // feet; an enemy that comes within 10 feet of it, or starts its turn there,
    // makes a Dexterity save against 20 Radiant damage, half on a success,
    // once a turn. It vanishes once it has dealt 60.
    SpellDef{
        .id = "guardian_of_faith",
        .label = "Guardian of Faith",
        .level = 4,
        .pattern = SpellPattern::buff,
        .target = SpellTarget::area,
        .range = 30,
        .rider = Rider::guardian_of_faith,
        .area = 5},
    // SRD 5.2.1 p. 112: a Charisma save or banished, Incapacitated and off the
    // field, until Concentration ends; one more creature per slot level above 4.
    SpellDef{
        .id = "banishment",
        .label = "Banishment",
        .level = 4,
        .pattern = SpellPattern::save_condition,
        .target = SpellTarget::enemy,
        .range = 30,
        .requires_sight = true,
        .requires_effect_capacity = true,
        .save = Ability::charisma,
        .upcast = {.extra_instances = 1},
        .rider = Rider::banishment,
        .concentration = true},
    // Not a spell: the Paladin's Abjure Foes (level 9), a Magic action for a
    // Channel Divinity use. Its creatures are chosen like a spell's (CLASS-2).
    SpellDef{
        .id = "abjure_foes",
        .label = "Abjure Foes",
        .level = 0,
        .pattern = SpellPattern::save_condition,
        .target = SpellTarget::enemy,
        .range = 60,
        .requires_sight = true,
        .save = Ability::wisdom,
        .rider = Rider::abjure_foes}};

// A verb casting a spell from a slot above its level ends in "_N", the slot's
// level (2 to 9); one casting it with Magic Initiate's free use ends in
// "_initiate", and one cast through a Cleric's Divine Intervention in
// "_intervention".
inline constexpr std::string_view initiate_suffix = "_initiate";
inline constexpr std::string_view intervention_suffix = "_intervention";
// A Warlock's Mystic Arcanum: its spell cast once per Long Rest without a slot.
inline constexpr std::string_view arcanum_suffix = "_arcanum";
// Natural Recovery (Land Druid 6): a Circle spell cast once per Long Rest
// without a slot.
inline constexpr std::string_view natural_suffix = "_natural";

// The slot level a "_N" verb names, or 0.
inline unsigned cast_slot_level(std::string_view verb)
{
    if (verb.size() < 3 || verb[verb.size() - 2] != '_')
        return 0;
    const char digit = verb.back();
    return digit >= '2' && digit <= '9' ? static_cast<unsigned>(digit - '0') : 0;
}

// The verb without its "_N", "_initiate" or "_intervention" form, naming the
// spell or option cast.
inline std::string_view without_cast_suffix(std::string_view verb)
{
    if (cast_slot_level(verb))
        verb.remove_suffix(2);
    else if (verb.ends_with(initiate_suffix))
        verb.remove_suffix(initiate_suffix.size());
    else if (verb.ends_with(intervention_suffix))
        verb.remove_suffix(intervention_suffix.size());
    else if (verb.ends_with(arcanum_suffix))
        verb.remove_suffix(arcanum_suffix.size());
    else if (verb.ends_with(natural_suffix))
        verb.remove_suffix(natural_suffix.size());
    return verb;
}

// Accepts the "_N" and "_initiate" verb forms, so callers can pass a command verb directly.
// Command's options, in CommandOption order.
inline constexpr std::array<std::string_view, 4> command_options{"approach", "flee", "grovel",
    "halt"};

// The option a Command verb names, if it is one.
inline std::optional<CommandOption> command_option(std::string_view verb)
{
    verb = without_cast_suffix(verb);
    if (!verb.starts_with("command_"))
        return std::nullopt;
    verb.remove_prefix(8);
    const auto found = std::find(command_options.begin(), command_options.end(), verb);
    if (found == command_options.end())
        return std::nullopt;
    return static_cast<CommandOption>(found - command_options.begin() + 1);
}

// Chromatic Orb's damage types, each offered as "chromatic_orb_<type>".
inline constexpr std::array<std::string_view, 6> chromatic_types{"acid", "cold", "fire",
    "lightning", "poison", "thunder"};

// The damage type a Chromatic Orb verb names, if it is one.
inline std::optional<DamageType> chromatic_type(std::string_view verb)
{
    verb = without_cast_suffix(verb);
    if (!verb.starts_with("chromatic_orb_"))
        return std::nullopt;
    verb.remove_prefix(14);
    if (std::find(chromatic_types.begin(), chromatic_types.end(), verb) == chromatic_types.end())
        return std::nullopt;
    return damage_type(verb);
}

// The conditions Lesser Restoration ends, each offered as
// "lesser_restoration_<condition>".
inline constexpr std::array<std::string_view, 3> restoration_conditions{"blinded", "paralyzed",
    "poisoned"};

// The condition a Lesser Restoration verb names, if it is one.
inline std::optional<std::string_view> restoration_condition(std::string_view verb)
{
    if (!verb.starts_with("lesser_restoration_"))
        return std::nullopt;
    verb.remove_prefix(19);
    const auto found = std::find(restoration_conditions.begin(), restoration_conditions.end(), verb);
    if (found == restoration_conditions.end())
        return std::nullopt;
    return *found;
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

// Sorcerous Burst's damage types, each offered as "sorcerous_burst_<type>".
inline constexpr std::array<std::string_view, 7> burst_types{"acid", "cold", "fire", "lightning",
    "poison", "psychic", "thunder"};

// The damage type a Sorcerous Burst verb names, if it is one.
inline std::optional<DamageType> burst_type(std::string_view verb)
{
    if (!verb.starts_with("sorcerous_burst_"))
        return std::nullopt;
    verb.remove_prefix(16);
    if (std::find(burst_types.begin(), burst_types.end(), verb) == burst_types.end())
        return std::nullopt;
    return damage_type(verb);
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

// The skills Guidance is offered for: those the game makes checks with in a
// fight (GUIDE-1), each offered as "guidance_<skill>".
inline constexpr std::array<std::string_view, 2> guidance_skills{"athletics", "medicine"};

// The index in guidance_skills that a Guidance verb names, if it is one.
inline std::optional<int> guidance_skill(std::string_view verb)
{
    if (!verb.starts_with("guidance_"))
        return std::nullopt;
    verb.remove_prefix(9);
    const auto found = std::find(guidance_skills.begin(), guidance_skills.end(), verb);
    if (found == guidance_skills.end())
        return std::nullopt;
    return static_cast<int>(found - guidance_skills.begin());
}

// Protection from Energy's damage types, each offered as
// "protection_from_energy_<type>".
inline constexpr std::array<std::string_view, 5> energy_types{"acid", "cold", "fire",
    "lightning", "thunder"};

// The damage type a Protection from Energy verb names, if it is one.
inline std::optional<DamageType> energy_type(std::string_view verb)
{
    verb = without_cast_suffix(verb);
    if (!verb.starts_with("protection_from_energy_"))
        return std::nullopt;
    verb.remove_prefix(23);
    if (std::find(energy_types.begin(), energy_types.end(), verb) == energy_types.end())
        return std::nullopt;
    return damage_type(verb);
}

// Bestow Curse's curses, each offered as "bestow_curse_<curse>": Disadvantage
// on one ability's checks and saves, on attacks against the caster, a Wisdom
// save each turn or only the Dodge action, or 1d8 more Necrotic damage from
// the caster.
inline constexpr std::array<std::string_view, 9> curses{"strength", "dexterity", "constitution",
    "intelligence", "wisdom", "charisma", "attacks", "dodge", "necrotic"};

// The index in curses a Bestow Curse verb names, if it is one.
inline std::optional<int> curse_of(std::string_view verb)
{
    verb = without_cast_suffix(verb);
    if (!verb.starts_with("bestow_curse_"))
        return std::nullopt;
    verb.remove_prefix(13);
    const auto found = std::find(curses.begin(), curses.end(), verb);
    if (found == curses.end())
        return std::nullopt;
    return static_cast<int>(found - curses.begin());
}

// What Greater Restoration ends, each offered as "greater_restoration_<harm>":
// Charmed, a curse, Harm's lowered Hit Point maximum, Petrified, or
// Befuddlement.
inline constexpr std::array<std::string_view, 5> greater_harms{"charmed", "curse", "drained",
    "petrified", "befuddled"};

// The index in greater_harms a Greater Restoration verb names, if it is one.
inline std::optional<std::size_t> greater_harm(std::string_view verb)
{
    verb = without_cast_suffix(verb);
    if (!verb.starts_with("greater_restoration_"))
        return std::nullopt;
    verb.remove_prefix(20);
    const auto found = std::find(greater_harms.begin(), greater_harms.end(), verb);
    if (found == greater_harms.end())
        return std::nullopt;
    return static_cast<std::size_t>(found - greater_harms.begin());
}

// The ability a Contagion verb ("contagion_<ability>") names, if it is one.
inline std::optional<std::size_t> contagion_ability(std::string_view verb)
{
    constexpr std::array<std::string_view, 6> abilities{"strength", "dexterity", "constitution",
        "intelligence", "wisdom", "charisma"};
    verb = without_cast_suffix(verb);
    if (!verb.starts_with("contagion_"))
        return std::nullopt;
    verb.remove_prefix(10);
    const auto found = std::find(abilities.begin(), abilities.end(), verb);
    if (found == abilities.end())
        return std::nullopt;
    return static_cast<std::size_t>(found - abilities.begin());
}

// A wall's directions on the battle map (LEVELS-3), each offered as
// "<spell>_<direction>": along a row, along a column, along either diagonal,
// or, for a wall that allows it, as a ring.
inline constexpr std::array<std::string_view, 5> wall_directions{"horizontal", "vertical",
    "rising", "falling", "ring"};

// Contingency's triggers (LEVELS-3), each offered as
// "contingency_<spell>_<trigger>": dropping to half Hit Points, a fight
// starting, or dropping to 0.
inline constexpr std::array<std::string_view, 3> contingency_triggers{"bloodied", "fight",
    "fallen"};

// The burning side of Wall of Fire, two for each direction in wall_directions
// order, each offered as "<spell>_<direction>_<side>".
inline constexpr std::array<std::string_view, 10> wall_sides{"north", "south", "west", "east",
    "northwest", "southeast", "northeast", "southwest", "inner", "outer"};

// The side a wall verb names, as an index into wall_sides, if it names one.
inline std::optional<std::size_t> wall_side(std::string_view verb)
{
    for (std::size_t n = 0; n < wall_sides.size(); ++n)
        if (verb.ends_with(wall_sides[n]) && verb.size() > wall_sides[n].size() &&
                verb[verb.size() - wall_sides[n].size() - 1] == '_' &&
                verb.find("wall_of_fire") == 0)
            return n;
    return std::nullopt;
}

// The direction a wall verb names, as an index into wall_directions, if it
// names one.
inline std::optional<std::size_t> wall_direction(std::string_view verb)
{
    verb = without_cast_suffix(verb);
    if (const auto side = wall_side(verb))
        verb.remove_suffix(wall_sides[*side].size() + 1);
    for (std::size_t n = 0; n < wall_directions.size(); ++n)
        if (verb.ends_with(wall_directions[n]) && verb.size() > wall_directions[n].size() &&
                verb[verb.size() - wall_directions[n].size() - 1] == '_')
            return n;
    return std::nullopt;
}

inline const SpellDef *find_spell(std::string_view id)
{
    id = without_cast_suffix(id);
    if (energy_type(id))
        id = "protection_from_energy";
    if (curse_of(id))
        id = "bestow_curse";
    if (greater_harm(id))
        id = "greater_restoration";
    if (contagion_ability(id))
        id = "contagion";
    if (id == "sunbeam_line")
        id = "sunbeam";
    if (const auto side = wall_side(id))
        id.remove_suffix(wall_sides[*side].size() + 1);
    if (const auto direction = wall_direction(id))
        id.remove_suffix(wall_directions[*direction].size() + 1);
    if (id.starts_with("fire_shield_"))
        id = "fire_shield";
    if (id.starts_with("conjure_minor_elementals_"))
        id = "conjure_minor_elementals";
    if (id.starts_with("conjure_elemental_"))
        id = "conjure_elemental";
    if (id.starts_with("summon_dragon_"))
        id = "summon_dragon";
    if (id == "telekinesis_again")
        id = "telekinesis";
    if (id.starts_with("eyebite_"))
        id = "eyebite";
    if (id.starts_with("contingency_"))
        id = "contingency";
    if (id.starts_with("forcecage_"))
        id = "forcecage";
    if (id.starts_with("giant_insect_"))
        id = "giant_insect";
    if (id.starts_with("animal_shapes_"))
        id = "animal_shapes";
    if (id == "call_lightning_again")
        id = "call_lightning";
    if (id == "move_sanctuary")
        id = "natures_sanctuary";
    if (id.starts_with("polymorph_"))
        id = "polymorph";
    if (id == "dimension_door_with")
        id = "dimension_door";
    if (command_option(id))
        id = "command";
    if (resistance_type(id))
        id = "resistance";
    if (guidance_skill(id))
        id = "guidance";
    if (id.starts_with("magic_circle_"))
        id = "magic_circle";
    if (chromatic_type(id))
        id = "chromatic_orb";
    if (restoration_condition(id))
        id = "lesser_restoration";
    if (id == "enlarge" || id == "reduce")
        id = "enlarge_reduce";
    if (id == "true_strike_radiant")
        id = "true_strike";
    if (burst_type(id))
        id = "sorcerous_burst";
    if (dragon_type(id))
        id = id.starts_with("dragons_breath_exhale_") ? "dragons_breath_exhale" : "dragons_breath";
    for (const auto &spell : spell_table)
        if (spell.id == id)
            return &spell;
    return nullptr;
}

// The spell's place in spell_table, which names it in a saved effect; -1 if
// there is none.
inline int spell_index(std::string_view id)
{
    const auto *spell = find_spell(id);
    return spell ? static_cast<int>(spell - spell_table.data()) : -1;
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
