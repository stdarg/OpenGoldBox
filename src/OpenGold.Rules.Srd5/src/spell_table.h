#ifndef OPENGOLD_SRD5_SPELL_TABLE_H
#define OPENGOLD_SRD5_SPELL_TABLE_H
#include "damage.h"
#include "damage_roll.h"
#include "status_effects.h"
#include <array>
#include <algorithm>
#include <optional>
#include <span>
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
    smite,       // Bonus Action right after the caster's own melee hit; extra damage to that target
    buff,        // a lasting benefit on the target, no roll; `rider` names it
    camp,        // used only from the Camp dialog (CLASS-3), never offered in combat
    exploration, // cast while exploring, such as Knock at a locked door
    weapon_strike, // a weapon attack made by casting it, such as True Strike
    stabilize,     // a dying creature becomes Stable
    reaction       // cast only when its trigger asks the caster (React/Decline)
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
    animal_shapes,
    slow,
    tsunami,
    // The original's charged items (MAGIC-3).
    paralysis_ray,
    giant_strength,
    disappearance,
    // Spells found on the original's scrolls (MAGIC-6).
    enhance_ability,
    snake_charm,
    missile_ward,
    // A Basilisk's Petrifying Gaze (MENDOR-1).
    petrifying_gaze,
    compulsion
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

// Every SRD spell; a spell's place in it names the spell in a saved effect.
std::span<const SpellDef> spell_table();

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
// A carried item's spell or effect (MAGIC-3): a charge or use of the item, not
// a slot.
inline constexpr std::string_view item_suffix = "_item";
// The same, used as a Bonus Action with a Thief's Fast Hands.
inline constexpr std::string_view item_bonus_suffix = "_item_bonus";
// A monster's innate spell (its stat block's `innate` rows): a daily use, not a
// slot, cast with the Action, or with a Bonus Action (a Priest's Divine Aid).
inline constexpr std::string_view innate_suffix = "_innate";
inline constexpr std::string_view innate_bonus_suffix = "_innate_bonus";
// A monster's Legendary Action that casts a spell (a Vampire's Beguile), before
// the "_N" of a higher slot: "guiding_bolt_legendary_2".
inline constexpr std::string_view legendary_suffix = "_legendary";

// Whether a verb uses a carried item, with the Action or a Bonus Action.
inline constexpr bool item_verb(std::string_view verb)
{
    return verb.ends_with(item_suffix) || verb.ends_with(item_bonus_suffix);
}

// A spell read from a carried scroll (MAGIC-6) ends in "_scroll", before the
// "_N" of a slot above the spell's own: "fireball_scroll",
// "invisibility_scroll_3".
inline constexpr std::string_view scroll_suffix = "_scroll";

// The slot level a "_N" verb names, or 0.
inline constexpr unsigned cast_slot_level(std::string_view verb)
{
    if (verb.size() < 3 || verb[verb.size() - 2] != '_')
    {
        return 0;
    }
    const char digit = verb.back();
    return digit >= '2' && digit <= '9' ? static_cast<unsigned>(digit - '0') : 0;
}

// The verb without its "_N", "_initiate" or "_intervention" form, naming the
// spell or option cast.
inline constexpr std::string_view without_cast_suffix(std::string_view verb)
{
    if (cast_slot_level(verb))
    {
        verb.remove_suffix(2);
    }
    else if (verb.ends_with(initiate_suffix))
    {
        verb.remove_suffix(initiate_suffix.size());
    }
    else if (verb.ends_with(intervention_suffix))
    {
        verb.remove_suffix(intervention_suffix.size());
    }
    else if (verb.ends_with(arcanum_suffix))
    {
        verb.remove_suffix(arcanum_suffix.size());
    }
    else if (verb.ends_with(natural_suffix))
    {
        verb.remove_suffix(natural_suffix.size());
    }
    else if (verb.ends_with(item_suffix))
    {
        verb.remove_suffix(item_suffix.size());
    }
    else if (verb.ends_with(item_bonus_suffix))
    {
        verb.remove_suffix(item_bonus_suffix.size());
    }
    else if (verb.ends_with(innate_suffix))
    {
        verb.remove_suffix(innate_suffix.size());
    }
    else if (verb.ends_with(innate_bonus_suffix))
    {
        verb.remove_suffix(innate_bonus_suffix.size());
    }
    if (verb.ends_with(legendary_suffix))
    {
        verb.remove_suffix(legendary_suffix.size());
    }
    if (verb.ends_with(scroll_suffix))
    {
        verb.remove_suffix(scroll_suffix.size());
    }
    return verb;
}

// Whether a verb casts a spell as a monster's Legendary Action.
inline constexpr bool legendary_verb(std::string_view verb)
{
    if (cast_slot_level(verb))
    {
        verb.remove_suffix(2);
    }
    return verb.ends_with(legendary_suffix);
}

// Whether a verb reads a spell from a scroll (MAGIC-6).
inline constexpr bool scroll_verb(std::string_view verb)
{
    if (cast_slot_level(verb))
    {
        verb.remove_suffix(2);
    }
    return verb.ends_with(scroll_suffix);
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
    {
        return std::nullopt;
    }
    verb.remove_prefix(8);
    const auto found = std::find(command_options.begin(), command_options.end(), verb);
    if (found == command_options.end())
    {
        return std::nullopt;
    }
    return static_cast<CommandOption>(found - command_options.begin() + 1);
}

// Chromatic Orb's damage types, each offered as "chromatic_orb_<type>".
inline constexpr std::array<std::string_view, 6> chromatic_types{"acid",      "cold",   "fire",
                                                                 "lightning", "poison", "thunder"};

// The damage type a Chromatic Orb verb names, if it is one.
inline std::optional<DamageType> chromatic_type(std::string_view verb)
{
    verb = without_cast_suffix(verb);
    if (!verb.starts_with("chromatic_orb_"))
    {
        return std::nullopt;
    }
    verb.remove_prefix(14);
    if (std::find(chromatic_types.begin(), chromatic_types.end(), verb) == chromatic_types.end())
    {
        return std::nullopt;
    }
    return damage_type(verb);
}

// The conditions Lesser Restoration ends, each offered as
// "lesser_restoration_<condition>".
inline constexpr std::array<std::string_view, 3> restoration_conditions{"blinded", "paralyzed",
                                                                        "poisoned"};

// The condition a Lesser Restoration verb names, if it is one.
inline std::optional<std::string_view> restoration_condition(std::string_view verb)
{
    verb = without_cast_suffix(verb);
    if (!verb.starts_with("lesser_restoration_"))
    {
        return std::nullopt;
    }
    verb.remove_prefix(19);
    const auto found =
        std::find(restoration_conditions.begin(), restoration_conditions.end(), verb);
    if (found == restoration_conditions.end())
    {
        return std::nullopt;
    }
    return *found;
}

// Dragon's Breath's damage types, each offered as "dragons_breath_<type>" and
// exhaled as "dragons_breath_exhale_<type>".
inline constexpr std::array<std::string_view, 5> dragon_types{"acid", "cold", "fire", "lightning",
                                                              "poison"};

// The damage type a Dragon's Breath or exhale verb names, if it is one.
inline std::optional<DamageType> dragon_type(std::string_view verb)
{
    verb = without_cast_suffix(verb);
    for (const std::string_view prefix : {"dragons_breath_exhale_", "dragons_breath_"})
    {
        if (verb.starts_with(prefix))
        {
            verb.remove_prefix(prefix.size());
            if (std::find(dragon_types.begin(), dragon_types.end(), verb) == dragon_types.end())
            {
                return std::nullopt;
            }
            return damage_type(verb);
        }
    }
    return std::nullopt;
}

// Sorcerous Burst's damage types, each offered as "sorcerous_burst_<type>".
inline constexpr std::array<std::string_view, 7> burst_types{
    "acid", "cold", "fire", "lightning", "poison", "psychic", "thunder"};

// The damage type a Sorcerous Burst verb names, if it is one.
inline std::optional<DamageType> burst_type(std::string_view verb)
{
    if (!verb.starts_with("sorcerous_burst_"))
    {
        return std::nullopt;
    }
    verb.remove_prefix(16);
    if (std::find(burst_types.begin(), burst_types.end(), verb) == burst_types.end())
    {
        return std::nullopt;
    }
    return damage_type(verb);
}

// Resistance's damage types (CLASS-7), each offered as "resistance_<type>".
inline constexpr std::array<std::string_view, 11> resistance_types{
    "acid",     "bludgeoning", "cold",    "fire",     "lightning", "necrotic",
    "piercing", "poison",      "radiant", "slashing", "thunder"};

// The damage type a Resistance verb names, if it is one.
inline std::optional<DamageType> resistance_type(std::string_view verb)
{
    if (!verb.starts_with("resistance_"))
    {
        return std::nullopt;
    }
    verb.remove_prefix(11);
    if (std::find(resistance_types.begin(), resistance_types.end(), verb) == resistance_types.end())
    {
        return std::nullopt;
    }
    return damage_type(verb);
}

// The skills Guidance is offered for: those the game makes checks with in a
// fight (GUIDE-1), each offered as "guidance_<skill>".
inline constexpr std::array<std::string_view, 2> guidance_skills{"athletics", "medicine"};

// The index in guidance_skills that a Guidance verb names, if it is one.
inline std::optional<int> guidance_skill(std::string_view verb)
{
    if (!verb.starts_with("guidance_"))
    {
        return std::nullopt;
    }
    verb.remove_prefix(9);
    const auto found = std::find(guidance_skills.begin(), guidance_skills.end(), verb);
    if (found == guidance_skills.end())
    {
        return std::nullopt;
    }
    return static_cast<int>(found - guidance_skills.begin());
}

// Protection from Energy's damage types, each offered as
// "protection_from_energy_<type>".
inline constexpr std::array<std::string_view, 5> energy_types{"acid", "cold", "fire", "lightning",
                                                              "thunder"};

// The damage type a Protection from Energy verb names, if it is one.
inline std::optional<DamageType> energy_type(std::string_view verb)
{
    verb = without_cast_suffix(verb);
    if (!verb.starts_with("protection_from_energy_"))
    {
        return std::nullopt;
    }
    verb.remove_prefix(23);
    if (std::find(energy_types.begin(), energy_types.end(), verb) == energy_types.end())
    {
        return std::nullopt;
    }
    return damage_type(verb);
}

// Bestow Curse's curses, each offered as "bestow_curse_<curse>": Disadvantage
// on one ability's checks and saves, on attacks against the caster, a Wisdom
// save each turn or only the Dodge action, or 1d8 more Necrotic damage from
// the caster.
inline constexpr std::array<std::string_view, 9> curses{"strength",     "dexterity", "constitution",
                                                        "intelligence", "wisdom",    "charisma",
                                                        "attacks",      "dodge",     "necrotic"};

// The index in curses a Bestow Curse verb names, if it is one.
inline std::optional<int> curse_of(std::string_view verb)
{
    verb = without_cast_suffix(verb);
    if (!verb.starts_with("bestow_curse_"))
    {
        return std::nullopt;
    }
    verb.remove_prefix(13);
    const auto found = std::find(curses.begin(), curses.end(), verb);
    if (found == curses.end())
    {
        return std::nullopt;
    }
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
    {
        return std::nullopt;
    }
    verb.remove_prefix(20);
    const auto found = std::find(greater_harms.begin(), greater_harms.end(), verb);
    if (found == greater_harms.end())
    {
        return std::nullopt;
    }
    return static_cast<std::size_t>(found - greater_harms.begin());
}

// The ability a Contagion verb ("contagion_<ability>") names, if it is one.
inline std::optional<std::size_t> contagion_ability(std::string_view verb)
{
    static constexpr std::array<std::string_view, 6> abilities{
        "strength", "dexterity", "constitution", "intelligence", "wisdom", "charisma"};
    verb = without_cast_suffix(verb);
    if (!verb.starts_with("contagion_"))
    {
        return std::nullopt;
    }
    verb.remove_prefix(10);
    const auto found = std::find(abilities.begin(), abilities.end(), verb);
    if (found == abilities.end())
    {
        return std::nullopt;
    }
    return static_cast<std::size_t>(found - abilities.begin());
}

// A wall's directions on the battle map (LEVELS-3), each offered as
// "<spell>_<direction>": along a row, along a column, along either diagonal,
// or, for a wall that allows it, as a ring.
inline constexpr std::array<std::string_view, 5> wall_directions{"horizontal", "vertical", "rising",
                                                                 "falling", "ring"};

// Contingency's triggers (LEVELS-3), each offered as
// "contingency_<spell>_<trigger>": dropping to half Hit Points, a fight
// starting, or dropping to 0.
inline constexpr std::array<std::string_view, 3> contingency_triggers{"bloodied", "fight",
                                                                      "fallen"};

// The burning side of Wall of Fire, two for each direction in wall_directions
// order, each offered as "<spell>_<direction>_<side>".
inline constexpr std::array<std::string_view, 10> wall_sides{
    "north",     "south",     "west",      "east",  "northwest",
    "southeast", "northeast", "southwest", "inner", "outer"};

// The side a wall verb names, as an index into wall_sides, if it names one.
inline std::optional<std::size_t> wall_side(std::string_view verb)
{
    verb = without_cast_suffix(verb);
    for (std::size_t n = 0; n < wall_sides.size(); ++n)
    {
        if (verb.ends_with(wall_sides[n]) && verb.size() > wall_sides[n].size() &&
            verb[verb.size() - wall_sides[n].size() - 1] == '_' && verb.find("wall_of_fire") == 0)
        {
            return n;
        }
    }
    return std::nullopt;
}

// The direction a wall verb names, as an index into wall_directions, if it
// names one.
inline std::optional<std::size_t> wall_direction(std::string_view verb)
{
    verb = without_cast_suffix(verb);
    if (const auto side = wall_side(verb))
    {
        verb.remove_suffix(wall_sides[*side].size() + 1);
    }
    for (std::size_t n = 0; n < wall_directions.size(); ++n)
    {
        if (verb.ends_with(wall_directions[n]) && verb.size() > wall_directions[n].size() &&
            verb[verb.size() - wall_directions[n].size() - 1] == '_')
        {
            return n;
        }
    }
    return std::nullopt;
}

const SpellDef *find_spell(std::string_view id);

// The spell's place in spell_table, which names it in a saved effect; -1 if
// there is none.
int spell_index(std::string_view id);

// `mask` is a wire encoding, not the identity of a spell. It exists only so
// authored creature rows in combat.rules can still be read. An int holds at
// most 31 of them, which is why it is not the internal representation: a row
// added past that limit simply carries mask 0. Nothing in the rules should
// branch on a mask.
std::vector<std::string> spells_from_mask(unsigned mask);

// Zero for a set that cannot be expressed as a mask.
unsigned mask_from_spells(const std::vector<std::string> &ids);

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
        {
            result.push_back(id);
        }
    }
    return result;
}
} // namespace opengold::srd5::detail
#endif
