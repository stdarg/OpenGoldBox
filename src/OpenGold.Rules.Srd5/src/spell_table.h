#ifndef OPENGOLD_SRD5_SPELL_TABLE_H
#define OPENGOLD_SRD5_SPELL_TABLE_H
#include "damage.h"
#include "damage_roll.h"
#include "status_effects.h"
#include <array>
#include <string_view>
namespace opengold::srd5::detail {
// SRD 5.2.1 spell definitions. One row per spell; behaviour lives in
// Session::offer_spells and Session::resolve_spell, keyed on `pattern`.
// Adding a supported spell is a row here plus its class grant in
// spell_access.cpp -- not an edit to legal_commands() or submit().

// Every resolution shape the implemented catalog uses. Each is proved by at
// least one existing spell; none is speculative.
enum class SpellPattern : unsigned {
    spell_attack,   // one attack roll, damage on hit, optional rider
    save_damage,    // save against 8 + casting; damage on failure
    save_condition, // save against 8 + casting; rider on failure
    auto_damage,    // no roll; `instances` separately resolved damage instances
    repeat_attack,  // `instances` attack rolls against one target
    heal            // restore HP from dice plus the caster's spellcasting modifier
};

// Which creatures the spell may be offered against. These reproduce the
// existing per-spell scopes exactly. Fire Bolt and Magic Missile are
// enemy-only today although SRD 5.2.1 allows any creature; that discrepancy is
// deliberately preserved here so the refactor is verifiable, and is recorded as
// a separate rules finding.
enum class SpellTarget : unsigned {
    enemy,        // opposing side with hit points remaining
    any_creature, // any living actor in line of sight, either side
    wounded_ally  // same side, below maximum hit points
};

// A lasting effect applied by the spell; each maps to one apply_* function.
enum class Rider : unsigned { none, chill_touch, shocking_grasp, ray_of_frost, blindness };

// Added when cast from a level-two slot. Zeroed means the spell does not upcast.
struct Upcast { unsigned extra_dice{}, extra_instances{}; };

struct SpellDef {
    std::string_view id, label;
    unsigned level{};                // 0 = cantrip
    unsigned mask{};                 // legacy character-profile bit; retired with the PC chain
    SpellPattern pattern{};
    SpellTarget target{};
    int range{};                     // feet
    bool verbal{true}, somatic{true};
    bool bonus_action{};
    bool melee{};                    // passes ranged=false to attack()
    bool requires_sight{};           // gated on can_see()
    bool requires_effect_capacity{}; // gated on can_apply()
    Ability save{Ability::strength};  // save patterns only
    DamageType damage{DamageType::fire};
    DamageDice dice{};               // {count, sides, bonus}
    bool add_casting_modifier{};     // heal: bonus becomes casting - 2
    unsigned instances{1};           // darts / rays
    Upcast upcast{};
    Rider rider{Rider::none};
};

// Order matches the sequence legal_commands() emitted before the table existed:
// the cantrips offered against any creature, then the enemy-only spells, then
// the ally-targeted healing. Offer order is observable, so keep it.
// Healing Word is the exception: as a Bonus Action it was emitted in its own
// pass ahead of the Action offers, so offer_spells() runs once per pass and
// filters on `bonus_action` rather than relying on this row order.
inline constexpr std::array spell_table{
    SpellDef{.id="chill_touch",.label="Chill Touch",.level=0,.mask=2048,
             .pattern=SpellPattern::spell_attack,.target=SpellTarget::any_creature,.range=5,
             .melee=true,.requires_effect_capacity=true,
             .damage=DamageType::necrotic,.dice={1,10,0},.rider=Rider::chill_touch},
    SpellDef{.id="shocking_grasp",.label="Shocking Grasp",.level=0,.mask=1024,
             .pattern=SpellPattern::spell_attack,.target=SpellTarget::any_creature,.range=5,
             .melee=true,.requires_effect_capacity=true,
             .damage=DamageType::lightning,.dice={1,8,0},.rider=Rider::shocking_grasp},
    SpellDef{.id="eldritch_blast",.label="Eldritch Blast",.level=0,.mask=512,
             .pattern=SpellPattern::spell_attack,.target=SpellTarget::any_creature,.range=120,
             .damage=DamageType::force,.dice={1,10,0}},
    SpellDef{.id="ray_of_frost",.label="Ray of Frost",.level=0,.mask=256,
             .pattern=SpellPattern::spell_attack,.target=SpellTarget::any_creature,.range=60,
             .requires_effect_capacity=true,
             .damage=DamageType::cold,.dice={1,8,0},.rider=Rider::ray_of_frost},
    SpellDef{.id="sacred_flame",.label="Sacred Flame",.level=0,.mask=128,
             .pattern=SpellPattern::save_damage,.target=SpellTarget::any_creature,.range=60,
             .requires_sight=true,.save=Ability::dexterity,
             .damage=DamageType::radiant,.dice={1,8,0}},
    SpellDef{.id="poison_spray",.label="Poison Spray",.level=0,.mask=64,
             .pattern=SpellPattern::spell_attack,.target=SpellTarget::any_creature,.range=30,
             .damage=DamageType::poison,.dice={1,12,0}},
    // Fire Bolt had no resolution branch of its own: it fell through to the
    // default case and relied on attack()'s default 1d10 fire arguments.
    SpellDef{.id="fire_bolt",.label="Fire Bolt",.level=0,.mask=1,
             .pattern=SpellPattern::spell_attack,.target=SpellTarget::enemy,.range=120,
             .damage=DamageType::fire,.dice={1,10,0}},
    SpellDef{.id="magic_missile",.label="Magic Missile",.level=1,.mask=4,
             .pattern=SpellPattern::auto_damage,.target=SpellTarget::enemy,.range=120,
             .requires_sight=true,.damage=DamageType::force,.dice={1,4,1},.instances=3,
             .upcast={.extra_instances=1}},
    SpellDef{.id="scorching_ray",.label="Scorching Ray",.level=2,.mask=16,
             .pattern=SpellPattern::repeat_attack,.target=SpellTarget::enemy,.range=120,
             .damage=DamageType::fire,.dice={2,6,0},.instances=3},
    SpellDef{.id="blindness",.label="Blindness",.level=2,.mask=32,
             .pattern=SpellPattern::save_condition,.target=SpellTarget::enemy,.range=120,
             .somatic=false,.requires_sight=true,.requires_effect_capacity=true,
             .save=Ability::constitution,.rider=Rider::blindness},
    SpellDef{.id="cure_wounds",.label="Cure Wounds",.level=1,.mask=2,
             .pattern=SpellPattern::heal,.target=SpellTarget::wounded_ally,.range=5,
             .dice={2,8,0},.add_casting_modifier=true,.upcast={.extra_dice=2}},
    SpellDef{.id="healing_word",.label="Healing Word",.level=1,.mask=8,
             .pattern=SpellPattern::heal,.target=SpellTarget::wounded_ally,.range=60,
             .somatic=false,.bonus_action=true,.requires_sight=true,
             .dice={2,4,0},.add_casting_modifier=true,.upcast={.extra_dice=2}}};

// Accepts the "_2" upcast verb form, so callers can pass a command verb directly.
inline const SpellDef* find_spell(std::string_view id){
    if(id.ends_with("_2"))id.remove_suffix(2);
    for(const auto& spell:spell_table)if(spell.id==id)return &spell;
    return nullptr;
}
}
#endif
