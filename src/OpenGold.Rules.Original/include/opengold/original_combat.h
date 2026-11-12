#ifndef OPENGOLD_ORIGINAL_COMBAT_H
#define OPENGOLD_ORIGINAL_COMBAT_H
#include "opengold/pool_radiance.h"
#include "opengold/rules.h"

#include <array>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Fights under the original rules (docs/ORIGINAL-RULES.md, plan step 4): the
// rules module the game's combat runs through, and the numbers a creature
// fights with.
namespace opengold::original
{

// One of a creature's attack forms: how many it makes in two rounds and its
// damage dice.
struct AttackForm
{
    int rate_per_two_rounds{}, dice{}, sides{}, modifier{};
    bool operator==(const AttackForm &) const = default;
};

// A readied missile weapon: one whose type reaches past the next square
// (charhlp.pas, HasRangedWeapon).
struct Missile
{
    // Squares it reaches: its type's range less 1 (cbtacts.pas, CombatAimMenu).
    int range{1};
    // Its attacks in two rounds, never fewer than 2 (cbtacts.pas, RecalcAttacksLeft).
    int rate_per_two_rounds{2};
    // Its own ammunition: each throw spends one (darts, daggers, spears).
    bool thrown{};
    // Thrown with Strength too (a dagger, hand axe, spear or javelin): it
    // strikes an enemy next to it as in melee, shoots with one there, and may
    // guard (C_Item.PROPS_THROWABLE).
    bool throwable{};
    // Whether it has something to shoot: itself when thrown, the readied
    // arrows or quarrels it fires, or nothing at all for a sling
    // (charhlp.pas, FindDamagingRangedWeapon).
    bool loaded{};
    bool needs_no_ammunition{};
    // The count of what it shoots, 0 for a single item: one shot a round at
    // most, and gone after it (cbtacts.pas, MakeAttack).
    int quantity{};
    // What a thrown weapon's thrower fights with once the last has left its
    // hand: its THAC0, first attack and backstab with no weapon.
    int empty_handed_thac0{20};
    AttackForm empty_handed_attack{};
    bool empty_handed_backstabs{};

    bool operator==(const Missile &) const = default;
};

// The item a creature's blows land with, as defenses weigh it: its magic,
// whether it is silver, and its weapon class (ItemTemplate::damage_type: 0
// slashing, bit 0x01 piercing, bit 0x80 bludgeoning).
struct StrikingItem
{
    int plus{};
    bool silver{};
    int weapon_class{};

    bool operator==(const StrikingItem &) const = default;
};

// An effect on a creature (fxengine.pas): its code, the minutes left (0 for
// one that lasts), the strength its handler keeps data in, and whether its
// handler runs as it is removed.
struct Effect
{
    int code{};
    int minutes{};
    int strength{};
    bool on_remove{};

    bool operator==(const Effect &) const = default;
};

// A creature's numbers in a fight under the original rules.
struct Combatant
{
    int thac0{20}, armor_class{10}, rear_armor_class{10};
    int hit_points{1}, max_hit_points{1};
    // Squares a round; the fight counts twice as many half-moves.
    int movement{12};
    // A character's weapon, or a monster's two attack forms (the second
    // unused at a rate of 0).
    std::array<AttackForm, 2> attacks{};
    std::array<int, 5> saving_throws{};
    // The highest level of its classes: a level-0 creature can be swept.
    int level{};
    // Its levels by the original's class slots (ClassSlot), which a level
    // drain takes from.
    std::array<int, 8> class_levels{};
    // How many level-0 enemies a fighter sweeps through in a round.
    int attack_level{1};
    // Dexterity's missile adjustment, which initiative adds.
    int initiative_bonus{};
    // The record's NPC byte: from 0x80 up, (byte & 0x7F) * 2 is its morale.
    unsigned morale{};
    int intelligence{10};
    // A thief's level, and whether its armor and weapon let it backstab: no
    // armor or leather, and no weapon, a club, a dagger or a sword.
    int thief_level{};
    bool backstabs{};
    // The first attack's dice and modifier against a large creature, when a
    // readied weapon has its own for them; its rate is the first attack's.
    std::optional<AttackForm> large_attack;
    // A large creature (its combat icon's size), which weapons strike with
    // their dice for large creatures.
    bool large{};
    // Its readied missile weapon, if any.
    std::optional<Missile> missile;
    // What its readied protective items add to its saving throws.
    int save_bonus{};
    // Its Strength score, which a holding bite cannot hold from 19 up.
    int strength{10};
    // Its undead level for turning (1 skeleton to 10 vampire), 0 for the living.
    int undead_level{};
    // Its Constitution, for the racial saving throw bonuses; its alignment
    // (0 lawful good to 8 chaotic evil, in the original's order); and its
    // creature type (above 1, Charm Person cannot charm it).
    int constitution{10}, alignment{4}, creature_type{};
    // The kind of foe it is to the races that know it (Combatant::kin bits):
    // a dwarf's or a gnome's sworn foe, a giant, a bugbear or gnoll.
    int kin{};
    // The spells it has memorized, by number (original_spells.h), a copy each.
    std::vector<int> spells;
    // Its race by the original's numbers: 0 a monster, then dwarf, elf,
    // gnome, half-elf, halfling, half-orc and human (1 to 7).
    int race{};
    // What its blows land with: the thrown weapon, the arrows or quarrels a
    // launcher fires, or the readied weapon; none for natural attacks
    // (charhlp.pas, FindDamagingRangedWeapon).
    std::optional<StrikingItem> striker;
    // Its readied weapon's item type, -1 for none: holy water, oil and
    // boulders count by it.
    int weapon_type{-1};
    // A readied mirror or wooden holy symbol, which a gazing monster cannot
    // look upon (effhndlr.pas, EffGazeBlock).
    bool wards_gaze{};
    // Its effects: a monster's from its record (MONnSPC), and those the fight
    // adds.
    std::vector<Effect> effects;

    [[nodiscard]] std::string encode() const;
    [[nodiscard]] static std::optional<Combatant> decode(std::string_view data);
    bool operator==(const Combatant &) const = default;
};

// The kinds of foe the races know (chargen.pas and gdconsts.pas): bits of
// Combatant::kin, from the monster's record name.
namespace kin
{
inline constexpr int dwarf_foe = 1, gnome_foe = 2, giant = 4, bugbear = 8;
} // namespace kin

// An item a creature carries, with its type (ITEMS); it fights with those it
// has readied (por::ItemRecord::readied).
struct CarriedItem
{
    por::ItemRecord item;
    por::ItemTemplate type;
};

// A creature as its record and items make it fight. As the original does at
// the start of every fight and turn (charhlp.pas, RecalcStats): its own
// THAC0, Armor Class, Hit Points, movement and attacks, with Strength unarmed
// or with a weapon that takes it, the readied weapon's dice and magic, the
// best armor, shield, ring and protective magic, and its load of items and
// coins on its movement. The original loads a monster's record unchanged, its
// current Hit Points included (eclvm.pas, EclLoadMonster).
[[nodiscard]] Combatant combatant_from_record(const por::CharacterRecord &record,
                                              std::span<const CarriedItem> carried = {},
                                              std::span<const por::EffectRecord> effects = {});

// What the original rules' module knows of a game: its monsters, by the key a
// fight's participant names them with (por::CreatureId::key(), "MON2CHA.DAX:0").
struct Content
{
    std::map<std::string, Combatant, std::less<>> bestiary;
};

// The original rules as the module the game's fights and party run through.
[[nodiscard]] std::unique_ptr<rules::RulesModule> rules_module(Content content = {});

} // namespace opengold::original
#endif
