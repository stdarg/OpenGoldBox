#ifndef OPENGOLD_BUCK_ROGERS_COMBAT_H
#define OPENGOLD_BUCK_ROGERS_COMBAT_H
#include "opengold/buck_rogers_records.h"
#include "opengold/buck_rogers_rules.h"
#include "opengold/buck_rogers_upkeep.h"
#include "opengold/dice.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Ground combat in the two Buck Rogers games, Countdown to Doomsday and Matrix
// Cubed, under their original rules (docs/games/countdown.md, "Ground
// combat"). The combat session keeps its grid, turns and computer play; these
// are the game-keyed rules it asks: the numbers a creature fights with, the
// ascending attack roll, range and cover, what stops a hit, backstabs and
// stuns, area weapons and their clouds, ammunition, the injury states, saving
// throws, the new commands and the skills a fight uses, and the difficulty
// levels. Matrix Cubed's combat is Countdown's. A creature is the game's own
// record (buck_rogers::Creature) with its items and the ITEMS rows they name.
namespace opengold::original
{
// ITEMS' weapon classes (C_WClass and effhndlr.pas): the last three hit an
// area. Armor keeps its Armor Class there instead (50 and up).
enum class WeaponClass : std::uint8_t
{
    melee = 0,
    slug = 1,
    rocket = 2,
    laser = 3,
    microwave = 4,
    heat = 5,
    sonic = 6,
    rocket_launcher = 7,
    grenade = 8,
    plasma = 9
};

[[nodiscard]] constexpr bool hits_an_area(WeaponClass weapon_class) noexcept
{
    return weapon_class >= WeaponClass::rocket_launcher;
}

// The record's armor bits, set for the readied gear (C_Armor).
namespace armor_bit
{
inline constexpr std::uint8_t goggles = 0x08, ecm = 0x10, fields = 0x20, body = 0x40,
                              breathing = 0x80;
} // namespace armor_bit

// ITEMS' slots the fight reads (C_Equip, charhlp.pas).
namespace gear_slot
{
inline constexpr std::uint8_t weapon = 0, armor = 1, jetpack = 2, goggles = 3, breathing_mask = 4,
                              grenade_launcher = 6, ecm = 7;
} // namespace gear_slot

// The item types the fight knows by number: the Sonic Stunner, the stun and
// dazzle grenades and Battle Armor with fields (C_Item, charhlp.pas).
namespace ground_item
{
inline constexpr std::uint8_t sonic_stunner = 15, stun_grenade = 35, dazzle_grenade = 36,
                              battle_armor_with_fields = 24;
} // namespace ground_item

// The statuses of the new commands (dodging to using a jetpack) leave a
// creature able: set_status (buck_rogers_upkeep.h) disables every status but
// okay, as an injury does, so the fight sets these without it.
void set_command_status(buck_rogers::Creature &creature, BuckRogersStatus status) noexcept;
// Whether a creature of the status can act: okay, or in one of the new
// commands' statuses (dodging to using a jetpack).
[[nodiscard]] bool can_act(BuckRogersStatus status) noexcept;

// The effects a creature can be under (C_Effect), by the game's numbers.
namespace ground_effect
{
inline constexpr int helpless = 1, scared = 2, unkillable = 3, blind = 13, stunned = 14,
                     block_scared = 16, immune_to_microwave = 18, block_helpless = 19,
                     immune_to_rocket = 20, block_dazzle = 21, block_stun = 22,
                     immune_to_heat_and_plasma = 23, immune_to_laser = 24, dodges_slugs = 25,
                     resists_ranged = 26, resists_melee = 27, dizzying_poison = 28, dizzy = 29,
                     lethal_poison = 30, strikes_adjacent = 31, spits_acid = 32;
} // namespace ground_effect

// An effect on a creature: its number, the minutes (rounds) left, 0 for one
// that lasts the fight, and the strength its handler reads.
struct GroundEffect
{
    int code{};
    int minutes{};
    int strength{};
    bool operator==(const GroundEffect &) const = default;
};

[[nodiscard]] bool has_ground_effect(std::span<const GroundEffect> effects, int code) noexcept;
// Gives an effect, or lengthens the one there (fxengine.pas
// AddEffectWithMessage): false when a blocking effect or a made save that
// cancels keeps it off ("is Unaffected"). The blocks: helplessness, fear,
// the dazzle grenade's blindness and the stun of a sonic weapon or stun
// grenade.
bool give_ground_effect(std::vector<GroundEffect> &effects, GroundEffect given, bool saved_cancels,
                        bool from_dazzle, bool from_stun_weapon);
// The effects a creature loses as it goes down or the fight ends: fear,
// helplessness, the stun and dizziness (gdconsts.pas incapacitatingEffects).
void remove_incapacitating(std::vector<GroundEffect> &effects);
// A round passes: effects with minutes left lose one, and those that run out
// go (gametime.pas TimeOutEffects).
void age_ground_effects(std::vector<GroundEffect> &effects);

// A creature as a fight takes it in and gives it back: its record, its items
// and ITEMS' rows by type, of which a fight needs those its items name. As
// text, it is a participant's character profile and what the fight leaves
// of it (rules::VitalState::resources): its Hit Points, status, rounds and
// readied items, for the game to keep.
struct GroundRecord
{
    buck_rogers::Creature creature;
    std::vector<buck_rogers::Item> items;
    std::vector<buck_rogers::ItemType> types;
    // The second item list (types.pas TCharacter alternateItems, saved in
    // the .hid file), where SWAP ITEM LISTS puts the gear a script takes
    // away; a fight never reads it.
    std::vector<buck_rogers::Item> alternate_items{};
};

[[nodiscard]] std::string encode_ground_record(const GroundRecord &record);
[[nodiscard]] std::optional<GroundRecord> decode_ground_record(std::string_view text);

// The item type of an item, nothing for an empty template or a row ITEMS
// lacks.
[[nodiscard]] const buck_rogers::ItemType *type_of(const buck_rogers::Item &item,
                                                   std::span<const buck_rogers::ItemType> types);
[[nodiscard]] bool readied(const buck_rogers::Item &item) noexcept;
// The first readied item of a slot (viewchar.pas FindReadiedItem), and the
// readied weapon, the last one listed (charhlp.pas RecalcStats).
[[nodiscard]] std::optional<std::size_t>
readied_in_slot(std::span<const buck_rogers::Item> items,
                std::span<const buck_rogers::ItemType> types, std::uint8_t slot);
[[nodiscard]] std::optional<std::size_t>
readied_weapon(std::span<const buck_rogers::Item> items,
               std::span<const buck_rogers::ItemType> types);
// The readied weapon's class, melee without one.
[[nodiscard]] WeaponClass weapon_class_of(std::span<const buck_rogers::Item> items,
                                          std::span<const buck_rogers::ItemType> types);
// How far the readied weapon reaches (viewchar.pas ReadiedWeaponRange): 1
// without one or for a range of 0 or 255, 12 for a grenade with a grenade
// launcher readied.
[[nodiscard]] int readied_weapon_range(std::span<const buck_rogers::Item> items,
                                       std::span<const buck_rogers::ItemType> types);
// The projectile an item fires, 5 to 12 (its special bytes; cbtacts.pas
// MakeAttack), or 0: the grenades, the plasma thrower and the rocket
// launcher.
[[nodiscard]] int projectile_kind(const buck_rogers::Item &item) noexcept;

struct DamageDice
{
    int count{}, sides{}, bonus{};
    bool operator==(const DamageDice &) const = default;
};

// What a creature fights with (charhlp.pas RecalcStats and WeaponAdjust):
// its combat value, with Strength's adjustment unarmed or with a melee
// weapon and Dexterity's with any other, the weapon's specializations and
// plus, 2 less after a failed Zero-G roll and 1 more while its side made its
// tactics roll; its Armor Class from 50, raised by its armor and its plus,
// by dodging (2, or half the level from level 4) and, in front, by
// Dexterity, the rear 2 lower; the bits of its readied armor, goggles, mask
// and ECM; each attack slot's damage, the weapon's in the first; and its
// movement in squares, halved past half the weight its Strength allows.
struct GroundNumbers
{
    int attack_bonus{};
    std::array<int, 2> armor_class{50, 48};
    std::uint8_t armor_flags{};
    std::array<DamageDice, 2> damage{};
    int movement{};
    int encumbrance{};
    bool operator==(const GroundNumbers &) const = default;
};

[[nodiscard]] GroundNumbers ground_numbers(const buck_rogers::Creature &creature,
                                           std::span<const buck_rogers::Item> items,
                                           std::span<const buck_rogers::ItemType> types,
                                           bool side_tactics);
// The armor class dodging adds (RecalcStats): 2 below level 4, half the
// level from there.
[[nodiscard]] int dodge_armor_bonus(int level) noexcept;

// The attack roll (fxengine.pas AttackRoll): a natural 1 misses, a natural
// 20 counts as 100 and so always hits; otherwise the roll, the attacker's
// combat value and its side's modifier must reach the target number.
[[nodiscard]] bool ground_attack_hits(int natural, int attack_bonus, int modifier,
                                      int target) noexcept;
// What the difficulty adds to the party's side's rolls, Novice +2 to Admiral
// -3 (AttackRoll; docs/BUCK-ROGERS.md, Difficulty).
enum class BuckRogersDifficulty : unsigned
{
    novice,
    cadet,
    trooper,
    veteran,
    ace,
    admiral
};

// The levels' names on the Options screen, Novice to Admiral.
inline constexpr std::array<std::string_view, 6> buck_rogers_difficulty_names{
    "Novice", "Cadet", "Trooper", "Veteran", "Ace", "Admiral"};
[[nodiscard]] int party_to_hit(BuckRogersDifficulty level) noexcept;
// The percent of its Hit Points LOAD MONSTER gives a hostile creature
// (eclvm.pas EclLoadMonster): (level + 2) quarters, Novice's half to Ace's
// one and a half, and Admiral twice them.
[[nodiscard]] unsigned monster_hit_points_percent(BuckRogersDifficulty level) noexcept;
// A loaded hostile creature's Hit Points, at least 1.
[[nodiscard]] int loaded_hit_points(int hit_points, BuckRogersDifficulty level) noexcept;

// The penalty for a long shot (cbtacts.pas ProjectileRangePenalty): 5 past
// three quarters of the range, 2 past half, nothing for a weapon of range 1.
[[nodiscard]] int range_penalty(int distance, int weapon_range) noexcept;

// The squares a shot crosses, as the arena reports them: how far it is, and
// whether cover (a square that blocks the line past the shooter's first two),
// chaff or aerosol mist lies on it, its ends included.
struct GroundLine
{
    int distance{};
    bool blocked{}, chaff{}, mist{};
};

// The number an attack must reach (cbtacts.pas RollAttacks and
// ApplyRangePenalty): the defender's Armor Class, the rear one from behind,
// 2 lower for a Rogue from behind (the original's 2 higher, a bug, fixed);
// 5 higher for a ranged weapon fired by a
// guard, or otherwise 2 for cover and the range's penalty.
struct AttackSetting
{
    bool from_behind{}, rogue{}, ranged_weapon{}, guarding{};
    int weapon_range{1};
    GroundLine line;
};

[[nodiscard]] int target_number(const GroundNumbers &defender,
                                const AttackSetting &setting) noexcept;
// A rear attack (cbtacts.pas IsRearAttack): a one-square defender that has
// drawn more than one attack, struck along its own facing or one direction
// to either side. The original's test missed one of the eight such pairs
// (facing north, struck heading north-west); here every pair counts
// (docs/games/countdown.md, Bugs).
[[nodiscard]] bool rear_attack(int defender_facing, int attack_direction, int attacks_drawn,
                               bool one_square) noexcept;
// A backstab's place (cbtacts.pas IsBackstab): a weapon of range below 2,
// struck straight along the defender's facing. Then a Move Silently or a
// Distract check decides it, twice the skill for a Rogue.
[[nodiscard]] bool backstab_position(int defender_facing, int attack_direction,
                                     int weapon_range) noexcept;
[[nodiscard]] bool backstab_check(const buck_rogers::Creature &attacker, RandomState &random);
// A backstab's damage (cbtacts.pas CalcWeaponDamage): a Rogue's times
// (level + 7) / 4, anyone else's twice.
[[nodiscard]] int backstab_damage(int damage, const buck_rogers::Creature &attacker) noexcept;

// What stops a hit's damage (CalcWeaponDamage, and effhndlr.pas at
// WEAPON_DAMAGE_DEFENDER), in the original's order: the defender's
// immunities by the weapon's class; half the time a slug from beyond the
// next square dodged, a ranged shot from there or a melee blow resisted; an
// unkillable one never taken below 0; then a rocket stopped by chaff on the
// line, by ECM half the time and by fielded battle armor three times in
// four, a laser by aerosol mist, a microwave beam by body armor. A blow
// without a weapon meets only the unkillable one's luck.
struct HitDefense
{
    std::optional<WeaponClass> weapon;
    int distance{1};
    GroundLine line;
    std::uint8_t armor_flags{};
    int hit_points{};
};

[[nodiscard]] int damage_after_defenses(int damage, const HitDefense &defense,
                                        std::span<const GroundEffect> defender_effects,
                                        RandomState &random);
// A Sonic Stunner's hit stuns for 1d4 + 4 rounds unless a save 0 is made
// (cbtacts.pas RollAttacks); it does no damage.
[[nodiscard]] int stun_rounds(RandomState &random);

// The saving throws a fight rolls, by category (the record's saves).
namespace ground_save
{
inline constexpr int stun = 0, gas = 1, blast = 2, dazzle = 3, acid = 6, cloud = 7;
} // namespace ground_save

// A saving throw (fxengine.pas RollSavingThrow): a natural 1 fails, a
// natural 20 saves, otherwise the roll and its modifier must reach the
// category's number. Blindness takes 4 off (EffBlind).
[[nodiscard]] bool ground_save_made(int natural, int modifier, int needed) noexcept;
[[nodiscard]] bool roll_ground_save(const buck_rogers::Creature &creature, int category,
                                    int modifier, bool blind, RandomState &random);

// What damage does (charhlp.pas InflictDamage): at exactly 0 a creature
// falls unconscious; past 0 it bleeds (dying), comatose past 9 below and
// dead past 29. It counts rounds while it bleeds from how far below 0 it
// went, and past 9 slips into a coma (ProcessDyingCharacters).
struct Injury
{
    BuckRogersStatus status{BuckRogersStatus::okay};
    int hit_points{};
    // How far below 0 it went: where a bleeding creature's count starts.
    int below_zero{};
};

[[nodiscard]] Injury injury_from(int hit_points, int damage) noexcept;
// Applies damage to the record: Hit Points, status and enabled flag; a
// bleeding creature's count in `dying_count`.
void inflict_ground_damage(buck_rogers::Creature &creature, int damage, int &dying_count);
// A round ends for a bleeding creature: true when it slips into a coma.
bool bleed(buck_rogers::Creature &creature, int &dying_count);
// A lethal poison's victim is poisoned: down with 0 Hit Points (effhndlr.pas
// EffLethalPoison, fxengine.pas KillCreature).
void poison_kills(buck_rogers::Creature &creature);

// The area weapons' projectiles (gdconsts.pas projectileKinds, 4 to 12): the
// effect a hit creature takes, the blast's radius, whether it leaves a
// cloud, the saving throw and what a made save does. 5 is the dazzle
// grenade, 6 the explosive ones, 7 gas, 8 chaff, 9 aerosol mist, 10 the
// plasma thrower, 11 the rocket launcher, 12 the stun grenade.
enum class SaveEffect : std::uint8_t
{
    none,
    cancels,
    halves
};

struct ProjectileKind
{
    int effect{}, radius{1};
    bool lingers{};
    int save{};
    SaveEffect saved{SaveEffect::none};
};

[[nodiscard]] const ProjectileKind &projectile(int kind);

// A blast's damage and the minutes its effect lasts (cbtacts.pas
// CombatLaunchProjectile): the weapon's dice, bonus and plus rolled once for
// every creature it reaches; the dazzle grenade blinds for 1d6 + 1; the stun
// grenade's dice are the minutes of its stun and it does no damage; any
// other effect lasts 255 minutes. The gas grenade's dice are 0d0, so the
// original left its victims helpless for good; here its gas lasts 1d6 + 1
// rounds, as the cloud's does (docs/games/countdown.md, Bugs).
struct Blast
{
    int damage{}, minutes{255};
};

[[nodiscard]] Blast roll_blast(int kind, const buck_rogers::ItemType &type, int plus,
                               RandomState &random);
// Whether a thrown or launched projectile goes astray: the combat value and a
// d20, less the range's penalty, at most 50.
[[nodiscard]] bool projectile_strays(int attack_bonus, int natural, int distance,
                                     int weapon_range) noexcept;
// One axis of a stray shot's landing, relative to the shooter (ScatterOffset):
// d4 / 2 farther out, a random way at 0 or beyond 8.
[[nodiscard]] int scatter_offset(int offset, RandomState &random);
// Whether a creature's gear spares it a projectile: goggles the dazzle
// grenade, a breathing mask or body armor the gas.
[[nodiscard]] bool spared_by_gear(int kind, std::uint8_t armor_flags) noexcept;
// The damage a blast does one creature.
[[nodiscard]] int blast_damage(int damage, SaveEffect saved_effect, bool saved) noexcept;
// Whether a blast reaches a square this many doubled steps (2 straight, 3
// diagonal) from where it lands (pathing.pas BuildTargetList, WalkLine).
[[nodiscard]] bool within_blast(int doubled_distance, int radius) noexcept;

// A cloud of chaff, mist or gas on the arena (types.pas TAreaEffect): the
// square it came down on and the eight around, those it covers, and the
// rounds it has left, 4, or 1 in zero gravity. It clears as a round ends
// with at most 1 left (fxengine.pas CountDownAreaEffects).
enum class CloudMatter : std::uint8_t
{
    gas,
    chaff,
    mist
};

struct GroundCloud
{
    rules::Cell center;
    // The center first, then east and clockwise (gdconsts.pas blockCellDirections).
    std::array<bool, 9> covers{};
    int rounds_left{4};
    CloudMatter matter{CloudMatter::gas};

    [[nodiscard]] std::vector<rules::Cell> squares() const;
    [[nodiscard]] bool covers_square(rules::Cell cell) const;
    bool operator==(const GroundCloud &) const = default;
};

[[nodiscard]] std::optional<CloudMatter> cloud_of(int kind) noexcept;
// The cells of the block round a center, center first (blockCellDirections).
[[nodiscard]] std::array<rules::Cell, 9> block_around(rules::Cell center) noexcept;

// An item's name as the game prints it (charhlp.pas PrintItemString): its
// words (the game's item words, by number), last first, and its bonus; "?"
// with no words.
[[nodiscard]] std::string item_name(const buck_rogers::Item &item,
                                    std::span<const std::string> words);

// A cloud laid where a projectile lands: each square of the block on the
// arena that can be walked.
template <typename Walkable>
[[nodiscard]] GroundCloud lay_ground_cloud(rules::Cell center, CloudMatter matter,
                                           bool zero_gravity, Walkable walkable)
{
    GroundCloud cloud{center, {}, zero_gravity ? 1 : 4, matter};
    const auto block = block_around(center);
    for (std::size_t at = 0; at < block.size(); ++at)
    {
        cloud.covers[at] = walkable(block[at]);
    }
    return cloud;
}

// A round ends for a cloud: false once it clears.
bool cloud_lingers(GroundCloud &cloud) noexcept;
// Gas on a creature's square (fxengine.pas ConsiderTerrainEffects): an
// able creature that is not helpless and lacks breathing protection is made
// helpless for 1d6 + 1 rounds unless it makes a save 1. The minutes, or
// nothing.
[[nodiscard]] std::optional<int> gassed(const buck_rogers::Creature &creature,
                                        std::uint8_t armor_flags, bool helpless, bool blind,
                                        RandomState &random);

// The attacks a creature makes this round (cbtacts.pas RecalcAttacksLeft and
// BeginRound): the first slot at the readied weapon's rate, or unarmed at
// the record's, the second at the record's second rate, halves rounded up
// on odd rounds, a ranged weapon's no more than its rounds. Dizziness halves
// the rates.
[[nodiscard]] std::array<int, 2> ground_attacks(const buck_rogers::Creature &creature,
                                                std::span<const buck_rogers::Item> items,
                                                std::span<const buck_rogers::ItemType> types,
                                                int round, bool dizzy);
// The rounds a weapon has left after shots: a ranged weapon spends one an
// attack (cbtacts.pas MakeAttack), and an area weapon one a launch. True when
// the weapon is used up and, being consumable (grenades), goes.
[[nodiscard]] bool spend_rounds(buck_rogers::Item &weapon, const buck_rogers::ItemType &type,
                                int shots);
// After a launch the rocket launcher and the plasma thrower must be readied
// again, and so must a grenade launcher that fired a grenade: readying a
// heavy item ends a creature's turn (viewchar.pas ToggleReadyItem). Puts the
// launcher away; true when it was a grenade launched rather than thrown.
bool put_away_launcher(std::span<buck_rogers::Item> items,
                       std::span<const buck_rogers::ItemType> types, std::size_t weapon);
[[nodiscard]] bool readying_ends_turn(const buck_rogers::ItemType &type) noexcept;
// Whether an item that is not readied may be (ToggleReadyItem): not while
// another of its slot is, for a weapon, armor, a jetpack, goggles, a mask, a
// grenade launcher or an ECM package. A readied one may always be put away.
[[nodiscard]] bool may_ready(std::span<const buck_rogers::Item> items,
                             std::span<const buck_rogers::ItemType> types, std::size_t item);

// The combat menu's choices beyond Target, View, Quick, Wait and End
// (combat.pas CombatMenu): Move with movement left; Intimidate with the
// skill and an enemy next to it, unless dodging or after a failed Zero-G
// roll; Aid with a patient; Sprint and Dodge with movement left, Sprint
// neither after a jetpack flight nor a failed Zero-G roll, neither while
// sprinting, Dodge not while dodging; Jetpack with movement left, a readied
// jetpack, out of doors, and not again after a flight; Guard while neither
// sprinting nor dodging.
struct GroundMenu
{
    bool move{}, intimidate{}, aid{}, sprint{}, dodge{}, jetpack{}, guard{};
    bool operator==(const GroundMenu &) const = default;
};

struct MenuSituation
{
    bool movement_left{}, enemy_next_to{}, patient{}, outdoors{};
};

[[nodiscard]] GroundMenu ground_menu(const buck_rogers::Creature &creature,
                                     std::span<const buck_rogers::Item> items,
                                     std::span<const buck_rogers::ItemType> types,
                                     const MenuSituation &situation);
// Whether its weapon may strike (combat.pas CombatOptionAllowed): not while
// sprinting, nor with an empty slug weapon.
[[nodiscard]] bool may_strike(const buck_rogers::Creature &creature,
                              std::span<const buck_rogers::Item> items,
                              std::span<const buck_rogers::ItemType> types);
// Guard sets it guarding unless it holds an area weapon (cbtacts.pas Guard).
[[nodiscard]] bool may_guard_with(std::span<const buck_rogers::Item> items,
                                  std::span<const buck_rogers::ItemType> types);

// A single character's skill check, as the fight's commands roll them, at
// difficulty 2 unless named (charhlp.pas SelectSkillCharacter).
[[nodiscard]] bool ground_skill_succeeds(const buck_rogers::Creature &creature, Skill skill,
                                         RandomState &random, int difficulty = 2);
// Sprint doubles the movement left (combat.pas ManualControl); a failed
// Zero-G roll quarters it (cbtacts.pas RollZeroG).
[[nodiscard]] int sprint_movement(int half_moves) noexcept;
[[nodiscard]] int zero_g_movement(int half_moves) noexcept;
// Who rolls Maneuver in Zero-G as its turn starts: an able creature in zero
// gravity whose initiative is above 1, so not one that waited.
[[nodiscard]] bool rolls_zero_g(bool zero_gravity, bool enabled, int initiative) noexcept;
// Who may make its side's tactics roll as a round begins (cbtacts.pas
// RollTactics): an able creature, okay, dodging or just off a jetpack
// flight, not stunned. A side stops at its first success; each made roll
// gives the side 1 to hit for the round.
[[nodiscard]] bool rolls_tactics(const buck_rogers::Creature &creature, bool stunned) noexcept;
// Leadership as a fight begins (cbtacts.pas UseLeadership): a party member
// that is no companion, okay and with the skill may take a companion off the
// computer's control: one okay, of Intelligence above 3, not unkillable,
// still under the computer, on a Leadership check.
[[nodiscard]] bool may_lead(const buck_rogers::Creature &leader) noexcept;
[[nodiscard]] bool may_follow(const buck_rogers::Creature &follower, bool unkillable) noexcept;
// Intimidate's victims (cbtacts.pas IntimidateTarget): on an Intimidate check,
// each enemy next to it that is not helpless is scared, unless it makes a
// save 0: it loses its next turn (effhndlr.pas EffScared). The effect it
// takes: 10 minutes, strength 12.
[[nodiscard]] GroundEffect scared_effect() noexcept;
// A failed jetpack flight lurches through eleven random squares, each up to
// two squares from the last in every direction (cbtacts.pas
// CombatUseJetpack), and lands on the last of them it may stand on, or stays
// where it was. The original's rows ran from one above to three below, so
// the lurch drifted south; here it runs two either way (docs/games/countdown.md,
// Bugs).
[[nodiscard]] rules::Cell lurch_step(rules::Cell from, RandomState &random) noexcept;
inline constexpr int lurch_steps = 11;

} // namespace opengold::original
#endif
