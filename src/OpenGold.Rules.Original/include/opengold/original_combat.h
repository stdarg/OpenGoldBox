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

    [[nodiscard]] std::string encode() const;
    [[nodiscard]] static std::optional<Combatant> decode(std::string_view data);
    bool operator==(const Combatant &) const = default;
};

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
                                              std::span<const CarriedItem> carried = {});

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
