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
    // A quarrel named with Curse's word 135, which sends a rakshasa (its
    // effect 130) away for good (effhndlr.pas, EffBanishByWeapon).
    bool banishing{};

    bool operator==(const StrikingItem &) const = default;
};

// An effect on a creature (fxengine.pas): its code, the minutes left (0 for
// one that lasts), the strength its handler keeps data in, and whether its
// handler runs as it is removed.
// Curse's effects that Pool of Radiance has under no number are numbered
// this much higher than Curse's own (canonical_effect).
inline constexpr int curse_effect_offset = 200;
// Secret of the Silver Blades' effects that neither earlier game has are
// numbered this much higher than Secret's own.
inline constexpr int secret_effect_offset = 400;

// The number the fight knows a game's own effect by (its records', items'
// and spells'): Pool of Radiance's as they are; Curse's that work as Pool of
// Radiance's of the same number as they are, its fumbling as Pool of
// Radiance's helplessness (31), and the rest curse_effect_offset higher
// (docs/ORIGINAL-RULES.md, Curse of the Azure Bonds); Secret's as the earlier
// game's effect that works as it does, and the rest secret_effect_offset
// higher (Secret of the Silver Blades).
[[nodiscard]] int canonical_effect(int code, GoldBoxGame game) noexcept;

struct Effect
{
    int code{};
    int minutes{};
    int strength{};
    bool on_remove{};

    bool operator==(const Effect &) const = default;
};

// An item a creature can use in a fight (viewchar.pas, UseItem): a scroll
// with up to three spells, or another item with a spell or power (spell
// numbers 1 to 67) and its charges.
struct UsableItem
{
    // Its place among the creature's items, for the game to keep what is spent.
    int ordinal{};
    bool readied{};
    // A scroll, and its kind by its equip slot (11 to 13; 12 is a cleric's),
    // and whether its spells are known.
    bool scroll{};
    int slot{};
    bool identified{};
    // A scroll's spells, 0 for one read; another item's spell or power first.
    std::array<int, 3> spells{};
    // Charges and the stack: 0 charges never run out.
    int charges{}, quantity{};

    bool operator==(const UsableItem &) const = default;
};

// A party member turned to stone stays a statue after the fight (the
// original's status PETRIFIED) until Stone to Flesh: its state outside fights
// says so in its description.
inline constexpr std::string_view stoned_description = "Turned to stone.";

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
    // Its Strength score, which a holding bite cannot hold from 19 up, with
    // its exceptional Strength at 18; its own, which the Strength spells and
    // potions raise for a while; and whether its blows take Strength (a
    // creature that strikes with Strength unarmed, or with a weapon that
    // takes it), so that a change of Strength changes their to-hit and damage.
    int strength{10}, exceptional_strength{};
    int natural_strength{10}, natural_exceptional_strength{};
    bool strength_counts{};
    // Its undead level for turning (1 skeleton to 10 vampire), 0 for the living.
    int undead_level{};
    // Its Constitution, for the racial saving throw bonuses; its alignment
    // (0 lawful good to 8 chaotic evil, in the original's order); and its
    // creature type (above 1, Charm Person cannot charm it).
    int constitution{10}, alignment{4}, creature_type{};
    // The kind of foe it is to the races that know it (Combatant::kin bits):
    // a dwarf's or a gnome's sworn foe, a giant, a bugbear or gnoll.
    int kin{};
    // What Secret of the Silver Blades' effects and spells take it for, by
    // its record's flags (trait bits); none in the earlier games.
    int traits{};
    // Its magic resistance in percent, read from its record in Secret of the
    // Silver Blades; the earlier games give it by effects.
    int magic_resistance{};
    // The spells it has memorized, by number (original_spells.h), a copy each.
    std::vector<int> spells;
    // The items it can use in a fight.
    std::vector<UsableItem> items;
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
    // look upon (effhndlr.pas, EffGazeBlock); and a readied mirror, which turns
    // a petrifying gaze back (EffPetrifyingGaze); in Secret, a readied item or
    // a record that has the effect of one (72).
    bool wards_gaze{};
    bool mirror{};
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

// What Secret of the Silver Blades' effects and spells take a creature for
// (Combatant::traits), from its record's type and target flags
// (secret_records.h).
namespace trait
{
// Dispel Evil keeps it 7 worse off and sends it away.
inline constexpr int dispel_evil = 1;
// Charm Person or Mammal may charm it.
inline constexpr int mammal = 2;
// A ranger next to it adds its level to its blows.
inline constexpr int ranger_foe = 4;
// Snake Charm may take it.
inline constexpr int snake = 8;
// A giant-bane weapon strikes it for 6d6.
inline constexpr int giant_bane = 16;
// Charm Person may charm it though it is a monster.
inline constexpr int person = 32;
// It breathes a stinking cloud unharmed; Slay Living and Death Spell cannot slay it.
inline constexpr int unliving = 64;
} // namespace trait

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

// The item types of a game's arrows and quarrels (charhlp.pas, RecalcStats):
// 73 and 28 in Pool of Radiance and Curse, 30 and 12 in Secret.
struct AmmunitionTypes
{
    std::uint8_t arrows{}, quarrels{};
};

[[nodiscard]] AmmunitionTypes ammunition_types(GoldBoxGame game) noexcept;
// Whether an elf strikes 1 better with an item of this type, a bow or a
// sword: 36, 37 and 41 to 44, and in Secret 18, 19 and 22 to 26
// (charhlp.pas, WeaponAdjust).
[[nodiscard]] bool elf_weapon(std::uint8_t type, GoldBoxGame game) noexcept;

// The conditions that outlast a fight and a day's rest until they are cured
// or run out, by the fight's numbers: blindness (33), disease (34, 43, 44,
// and Pool of Radiance's rot, 50 and 57), the helplessness a disease brings
// for good (31 with no end), poison (55, and Slow Poison's 15 and 22), Bestow
// Curse (36), feeblemind (268), Enlarge (12) and Strength (38), and the
// effects of the spells cast in camp before a fight (Bless 1, the
// protections 8, 9, 45, 46 and 41, the resistances 10 and 20, Shield 17,
// Detect Invisibility 24, Invisibility 25, Mirror Image 28, Blink 37, Haste
// 39 and Prayer 49), cast in camp or left from a fight; what a paladin has
// spent, the day's laying on of
// hands (340) and the week's cures of disease (341, its strength the cures
// spent); and the Strength a disease has wasted (601, OpenGoldBox's own, its
// strength the points).
[[nodiscard]] bool lasting_condition(const Effect &effect, GoldBoxGame game) noexcept;
// The marker of the Strength a disease has wasted outside fights.
inline constexpr int strength_wasted = 601;
// A party member's age in years, OpenGoldBox's own marker (its strength): a
// member's numbers carry it into a fight, where Haste and Youth change it,
// and the game keeps the change in the member's level history
// (RulesModule::lasting_changes).
inline constexpr int age_in_years = 602;

// What a party member's state carries between fights
// (rules::VitalState::resources): the numbers its last fight left it, its
// unspent spells among them, or after a day's rest only its lasting
// conditions, its spells memorized again.
struct CarriedState
{
    std::optional<Combatant> last_fight;
    std::vector<Effect> conditions;
};

[[nodiscard]] CarriedState carried_state(std::string_view resources, GoldBoxGame game);
// A rested member's state: its lasting conditions, or empty for none.
[[nodiscard]] std::string rested_state(std::span<const Effect> conditions);

// What the original rules' module knows of a game: its monsters, by the key a
// fight's participant names them with (por::CreatureId::key(), "MON2CHA.DAX:0"),
// and its item types (ITEMS), by type.
struct Content
{
    std::map<std::string, Combatant, std::less<>> bestiary;
    std::vector<por::ItemTemplate> item_types;
    // The game whose original rules these are, its monsters and items.
    GoldBoxGame game{GoldBoxGame::pool_of_radiance};
};

// The original rules as the module the game's fights and party run through.
[[nodiscard]] std::unique_ptr<rules::RulesModule> rules_module(Content content = {});

} // namespace opengold::original
#endif
