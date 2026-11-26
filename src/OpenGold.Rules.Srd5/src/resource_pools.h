#ifndef OPENGOLD_SRD5_RESOURCE_POOLS_H
#define OPENGOLD_SRD5_RESOURCE_POOLS_H
#include "actor_model.h"
#include "beast_forms.h"
#include "character_class.h"
#include "definition.h"
#include "opengold/rules.h"
#include <array>
#include <iosfwd>
#include <string_view>
#include <vector>

// Class resources as pools: the capacity a definition gives each, the counts
// an actor keeps, what a rest restores and how a vital record writes them.
namespace opengold::srd5::detail
{
using rules::AdvancementChoice;

// Every class resource that presents as a pool. The member pointers say where
// the spent and maximum values live, so a new resource feature is a row here
// rather than a push_back repeated at each presentation site. Storage is still a
// pair of ints per resource; moving that to a keyed pool needs a combat
// checkpoint format change, and this table is the place it will enumerate.
struct ResourceDescriptor
{
    std::string_view id, label;
    int Actor::*remaining;
    int Definition::*capacity;
    int short_rest;        // uses restored by a Short Rest; -1 means the full capacity
    bool combat_view;      // also listed during combat, not only on the rest screen
    unsigned slot_level{}; // a spell slot row: its level, in place of the members
    // A class resource: its counts and the class whose count it is.
    ClassCounts Actor::*pool{};
    CharacterClass owner{};
};

[[nodiscard]] int remaining_of(const ResourceDescriptor &descriptor, const Actor &a);

[[nodiscard]] int capacity_of(const ResourceDescriptor &descriptor, const Definition &d);

inline constexpr std::array resource_descriptors{
    ResourceDescriptor{"action_surge", "Action Surge", nullptr, &Definition::surges, -1, true, 0,
                       &Actor::surges, CharacterClass::fighter},
    ResourceDescriptor{"adrenaline_rush", "Adrenaline Rush", &Actor::rushes, &Definition::rushes,
                       -1, true},
    ResourceDescriptor{"second_wind", "Second Wind", &Actor::winds, &Definition::winds, 1, false},
    ResourceDescriptor{"spell_slot:1", slot_labels[1], nullptr, nullptr, 0, false, 1},
    ResourceDescriptor{"spell_slot:2", slot_labels[2], nullptr, nullptr, 0, false, 2},
    ResourceDescriptor{"spell_slot:3", slot_labels[3], nullptr, nullptr, 0, false, 3},
    ResourceDescriptor{"spell_slot:4", slot_labels[4], nullptr, nullptr, 0, false, 4},
    ResourceDescriptor{"spell_slot:5", slot_labels[5], nullptr, nullptr, 0, false, 5},
    ResourceDescriptor{"spell_slot:6", slot_labels[6], nullptr, nullptr, 0, false, 6},
    ResourceDescriptor{"spell_slot:7", slot_labels[7], nullptr, nullptr, 0, false, 7},
    ResourceDescriptor{"spell_slot:8", slot_labels[8], nullptr, nullptr, 0, false, 8},
    ResourceDescriptor{"spell_slot:9", slot_labels[9], nullptr, nullptr, 0, false, 9},
    ResourceDescriptor{"arcane_recovery", "Arcane Recovery", nullptr, &Definition::arcane, 0, false,
                       0, &Actor::arcane, CharacterClass::wizard},
    ResourceDescriptor{"lay_on_hands", "Lay On Hands", nullptr, &Definition::lay_on_hands, 0, true,
                       0, &Actor::lay_on_hands, CharacterClass::paladin},
    ResourceDescriptor{"paladins_smite", "Paladin's Smite", nullptr, &Definition::free_smite, 0,
                       true, 0, &Actor::free_casts, CharacterClass::paladin},
    ResourceDescriptor{"favored_enemy", "Favored Enemy", nullptr, &Definition::favored_enemy, 0,
                       true, 0, &Actor::free_casts, CharacterClass::ranger},
    ResourceDescriptor{"channel_divinity", "Channel Divinity", nullptr,
                       &Definition::channel_divinity, 1, true, 0, &Actor::channel_divinity,
                       CharacterClass::cleric},
    ResourceDescriptor{"rage", "Rage", nullptr, &Definition::rages, 1, true, 0,
                       &Actor::channel_divinity, CharacterClass::barbarian},
    ResourceDescriptor{"wild_shape", "Wild Shape", nullptr, &Definition::wild_shapes, 1, true, 0,
                       &Actor::channel_divinity, CharacterClass::druid},
    ResourceDescriptor{"focus", "Focus Points", nullptr, &Definition::focus, -1, true, 0,
                       &Actor::surges, CharacterClass::monk},
    ResourceDescriptor{"innate_sorcery", "Innate Sorcery", nullptr, &Definition::innate_sorcery, 0,
                       true, 0, &Actor::free_casts, CharacterClass::sorcerer},
    ResourceDescriptor{"sorcery_points", "Sorcery Points", nullptr, &Definition::sorcery_points, 0,
                       true, 0, &Actor::lay_on_hands, CharacterClass::sorcerer},
    ResourceDescriptor{"natural_recovery", "Natural Recovery", nullptr,
                       &Definition::natural_recovery, 0, false, 0, &Actor::arcane,
                       CharacterClass::druid},
    ResourceDescriptor{"sorcerous_restoration", "Sorcerous Restoration", nullptr,
                       &Definition::sorcerous_restoration, 0, false, 0, &Actor::arcane,
                       CharacterClass::sorcerer},
    ResourceDescriptor{"magical_cunning", "Magical Cunning", nullptr, &Definition::magical_cunning,
                       0, false, 0, &Actor::arcane, CharacterClass::warlock},
    ResourceDescriptor{"bardic_inspiration", "Bardic Inspiration", nullptr,
                       &Definition::bardic_inspiration, 0, true, 0, &Actor::arcane,
                       CharacterClass::bard},
    ResourceDescriptor{"uncanny_metabolism", "Uncanny Metabolism", nullptr, &Definition::metabolism,
                       0, false, 0, &Actor::arcane, CharacterClass::monk},
    ResourceDescriptor{"indomitable", "Indomitable", nullptr, &Definition::indomitable, 0, true, 0,
                       &Actor::arcane, CharacterClass::fighter},
    ResourceDescriptor{"wholeness_of_body", "Wholeness of Body", nullptr, &Definition::wholeness, 0,
                       true, 0, &Actor::free_casts, CharacterClass::monk},
    ResourceDescriptor{"relentless_rage", "Relentless Rage", nullptr, &Definition::relentless_rage,
                       -1, true, 0, &Actor::lay_on_hands, CharacterClass::barbarian},
    ResourceDescriptor{"intimidating_presence", "Intimidating Presence", nullptr,
                       &Definition::intimidating_presence, 0, true, 0, &Actor::free_casts,
                       CharacterClass::barbarian},
    ResourceDescriptor{"persistent_rage", "Persistent Rage", nullptr, &Definition::persistent_rage,
                       0, true, 0, &Actor::arcane, CharacterClass::barbarian},
    ResourceDescriptor{"tireless", "Tireless", nullptr, &Definition::tireless, 0, true, 0,
                       &Actor::arcane, CharacterClass::ranger},
    ResourceDescriptor{"divine_intervention", "Divine Intervention", nullptr,
                       &Definition::divine_intervention, 0, true, 0, &Actor::arcane,
                       CharacterClass::cleric},
    ResourceDescriptor{"natures_veil", "Nature's Veil", nullptr, &Definition::natures_veil, 0, true,
                       0, &Actor::lay_on_hands, CharacterClass::ranger}};

// Whether a level-up takes Lessons of the First Ones' Skilled, as a new
// invocation or one swapped in.
[[nodiscard]] bool takes_lessons_skilled(const AdvancementChoice &choice);

// The Hit Point maximum with Aid's and Heroes' Feast's increases and Harm's
// lowering, which cannot take it below 1.
[[nodiscard]] int max_hp(const Actor &a);

// Lay On Hands and Sorcery Points share one store, restored by a Long Rest; no
// class has both.
[[nodiscard]] ClassCounts lay_capacity(const Definition &d);

// The level of a Warlock's Pact Magic slots, all of one level.
[[nodiscard]] unsigned pact_level(const SpellSlots &slots);

// Font of Magic (SRD 5.2.1 p. 66): the Sorcery Points a slot of each level
// costs, and the Sorcerer level that can create it.
inline constexpr std::array<int, 6> font_of_magic_cost{0, 2, 3, 5, 6, 7};

inline constexpr std::array<int, 6> font_of_magic_level{0, 2, 3, 5, 7, 9};

// Spell slots of a level, with room for the ones Font of Magic creates from
// Sorcery Points; they vanish on a Long Rest.
[[nodiscard]] int slot_room(const Definition &d, unsigned level);

// Every slot level holds between none and the room for it.
[[nodiscard]] bool slots_in_range(const SpellSlots &slots, const Definition &d);

// The lowest slot level, `from` or above, with a slot left; 0 if none.
[[nodiscard]] unsigned lowest_slot(const Actor &a, unsigned from);

// Paladin's Smite, Favored Enemy, Innate Sorcery and the like, each its
// class's, restored by a Long Rest.
[[nodiscard]] ClassCounts free_cast_capacity(const Definition &d);

// One bit per Magic Initiate feat, each its spell's free cast.
[[nodiscard]] int initiate_capacity(const Definition &d);

// The bit of a spell's Magic Initiate free cast, or 0 when no feat casts it free.
[[nodiscard]] int initiate_bit(const Definition &d, std::string_view spell);

// The spell attack bonus a spell uses: the ability Magic Initiate chose for
// the spells it teaches, the class's spellcasting ability for every other.
[[nodiscard]] int casting_bonus(const Definition &d, std::string_view spell);

// The spellcasting ability modifier: the spell attack bonus without the
// Proficiency Bonus of the creature's level.
[[nodiscard]] int casting_modifier(const Definition &d, std::string_view spell = {});

// A cantrip's damage dice by character level (SRD 5.2.1's Cantrip Upgrade):
// one, two from level 5, three from 11, four from 17; Eldritch Blast's beams
// and a Dragonborn's Breath Weapon dice the same.
[[nodiscard]] int cantrip_tier(const Definition &d);

// Action Surge and Focus Points share one store; no class has both, and a
// Short Rest restores each fully.
[[nodiscard]] ClassCounts surge_capacity(const Definition &d);

// Arcane Recovery, Uncanny Metabolism, Magical Cunning and the like, each its
// class's, once per Long Rest.
[[nodiscard]] ClassCounts arcane_capacity(const Definition &d);

// Channel Divinity (a Cleric's and a Paladin's together, under the Cleric),
// Rage and Wild Shape, each regaining one use on a Short Rest.
[[nodiscard]] ClassCounts channel_capacity(const Definition &d);

// A Strength modifier of `giant` when that is better: a Potion of Giant
// Strength's 21 (+5, MAGIC-3) or Gauntlets of Ogre Power's 19 (+4, MAGIC-4). A
// Strength-based melee attack and its damage, Strength saves and Athletics
// follow it.
// Whether each class's count is between 0 and its capacity.
[[nodiscard]] bool within(const ClassCounts &counts, const ClassCounts &capacity);

// Each class's count no more than its capacity.
void cap(ClassCounts &counts, const ClassCounts &capacity);

// Each class's count moved by what its capacity gained (a new level).
void grow(ClassCounts &counts, const ClassCounts &now, const ClassCounts &before);

std::ostream &operator<<(std::ostream &out, const ClassCounts &counts);

std::istream &operator>>(std::istream &in, ClassCounts &counts);

// A vital record keeps one count for each class with the resource, in class
// order, or a single 0 when none has it: a single-class character's record
// reads as it always has (MULTI-1).
void write_pool(std::ostream &out, const ClassCounts &counts, const ClassCounts &capacity);

void read_pool(std::istream &in, ClassCounts &counts, const ClassCounts &capacity);

[[nodiscard]] Definition strengthened(Definition d, int giant);

// Wild Shape: the Beast's AC, Speed, size, Strength, Dexterity, physical saves
// and attack replace the Druid's; Hit Points, mental scores, proficiencies
// and features stay. Equipment merges into the form, and no spells are cast.
[[nodiscard]] Definition shaped(Definition d, const detail::BeastForm &form);

// Pact Magic slots and, from Font of Inspiration, Bardic Inspiration come back
// whole on a Short Rest, though other classes' slots and uses do not.
[[nodiscard]] unsigned short_rest_recovery(const ResourceDescriptor &descriptor,
                                           const Definition &d, unsigned capacity);

[[nodiscard]] rules::ResourcePool resource_pool(const ResourceDescriptor &descriptor,
                                                const Actor &actor, const Definition &d);

// Magic Initiate's free casts: one pool per feat, named for its spell.
void add_initiate_pools(std::vector<rules::ResourcePool> &pools, const Actor &actor,
                        const Definition &d);

// A hidden creature's Stealth total: the DC to find it.
[[nodiscard]] int stealth_total(const Actor &a);

// Uses of a species trait spent since the last Long Rest.
[[nodiscard]] int trait_uses_spent(const Actor &a);

[[nodiscard]] int trait_uses_left(const Actor &a);

// A species trait's uses, shown with the class resources.
void add_trait_pool(std::vector<rules::ResourcePool> &pools, const Actor &actor,
                    const Definition &d);
} // namespace opengold::srd5::detail
#endif
