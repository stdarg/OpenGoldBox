#ifndef OPENGOLD_SRD5_COMBAT_MODEL_H
#define OPENGOLD_SRD5_COMBAT_MODEL_H
#include "dice.h"
#include "character_class.h"
#include "damage_roll.h"
#include "sneak_attack.h"
#include "action_budget.h"
#include "feature_grants.h"
#include "training.h"
#include "weapon_mastery.h"
#include "spell_access.h"
#include "spell_table.h"
#include "spell_traits.h"
#include "class_spell_lists.h"
#include "equipment_rules.h"
#include "beast_forms.h"
#include "combat_grid.h"
#include "status_effects.h"
#include "concentration.h"
#include "life_cycle.h"
#include "recovery_timeline.h"
#include "weapons.h"
#include "thrown_gear.h"
#include "armor.h"
#include "magic_items.h"
#include "worn_items.h"
#include "effect_labels.h"
#include "opengold/item_key.h"
#include "opengold/scroll_key.h"
#include "opengold/rules.h"
#include "opengold/character_rules.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iosfwd>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// The SRD module's value model of a fight and of a character in it: a
// creature's Definition (its statistics as the content and the profile give
// them), an Actor (one definition fielded, with its life, resources and
// effects), the Content a rules module loads, and the helpers that read and
// change them. Session (the combat) and Module (the campaign rules) build on
// these; none of this draws dice or knows the presentation.
namespace opengold::srd5::detail
{
using rules::AdvancementChoice;
using rules::Cell;
using rules::Command;
using rules::EntityId;
using rules::FeatureGrant;
using rules::Footprint;
using rules::Identity;
using rules::Message;
using rules::MessageArgument;
using rules::Missile;
using rules::Participant;
using rules::ResourcePool;
using rules::StatusEffectView;
using rules::TempleService;
using rules::Terrain;
using rules::VitalState;
using rules::WeaponSetShown;

inline constexpr std::string_view rush_source = "species:orc/trait:adrenaline_rush";

// The only character profile tag this module reads or writes. Profiles live
// inside campaign saves and combat checkpoints, whose own format numbers reject
// anything older; change this recipe in place until 1.0.
inline constexpr std::string_view profile_magic = "PC44";

// The only combat checkpoint format this module reads or writes. Older
// checkpoints are rejected rather than migrated; change it in place until 1.0.
inline constexpr unsigned checkpoint_format = 77;

// A broken monster's turns of getting no nearer the edge before it charges
// (MORALE-2).
inline constexpr int panic_charge_after_turns = 2;

// MELEE-1: as in the original game, melee reaches adjacent squares only; the
// SRD's 10-foot reach (polearms, the whip, a troll's claws) is not used.
inline constexpr int melee_reach = 5;

using Dice = detail::DamageDice;

// A Goliath's Giant Ancestry boon (SRD 5.2.1 p. 85), in the order of the
// creator's options.
enum class GiantBoon : unsigned
{
    none,
    cloud, // Cloud's Jaunt: a Bonus Action teleport of up to 30 feet
    fire,  // Fire's Burn: 1d10 Fire damage with an attack roll's hit
    frost, // Frost's Chill: 1d6 Cold damage and Speed 10 feet lower
    hill,  // Hill's Tumble: a Large or smaller creature hit falls Prone
    stone, // Stone's Endurance: a Reaction lessening damage
    storm  // Storm's Thunder: a Reaction dealing 1d8 Thunder damage
};

[[nodiscard]] std::string_view giant_boon_label(GiantBoon boon);

// Spell slots by spell level, 1 to 9; index 0 is unused.
using SpellSlots = std::array<int, 10>;

inline constexpr unsigned max_slot_level = 9;

// "Level-one spell slots" to "Level-nine spell slots", by slot level.
inline constexpr std::array<std::string_view, 10> slot_labels{"",
                                                              "Level-one spell slots",
                                                              "Level-two spell slots",
                                                              "Level-three spell slots",
                                                              "Level-four spell slots",
                                                              "Level-five spell slots",
                                                              "Level-six spell slots",
                                                              "Level-seven spell slots",
                                                              "Level-eight spell slots",
                                                              "Level-nine spell slots"};

[[nodiscard]] int slot_total(const SpellSlots &slots);

// Spell slots by class level, SRD 5.2.1's class tables: the full casters, the
// Paladin and Ranger, and the Warlock's Pact Magic, whose slots share one level.
[[nodiscard]] SpellSlots spell_slots(CharacterClass klass, unsigned level);

struct Definition
{
    int ac{}, hp{}, initiative{}, speed{}, melee_bonus{};
    bool alert{};
    int melee_ability{}, ranged_ability{}, size{2};
    // A Beast form's squares in place of the creature's own (FOOT-1).
    std::optional<Footprint> footprint;
    std::string creature_type{"humanoid"}; // SRD creature type; player characters are Humanoid
    Dice melee;
    int ranged_bonus{};
    // The wielded weapon's enchantment (MAGIC-1), kept out of melee_bonus and
    // the damage dice so a light off-hand attack, which drops a positive
    // damage modifier, still adds it. bane_undead adds 2 more against Undead.
    int melee_magic{}, ranged_magic{};
    bool bane_undead{};
    bool flame_tongue{}; // The wielded weapon is a Flame Tongue.
    // The original's worn items (MAGIC-4): a Ring of Fire Resistance, and a
    // Cloak of Displacement, whose first attack roll against the wearer in a
    // fight misses.
    bool fire_ward{}, displacement{};
    int item_saves{}; // What worn items add to every saving throw.
    // The Rogue's Shop's gear (SECRET-1): Boots or a Cloak of Elvenkind give
    // Stealth checks Advantage, and the cloak gives checks to find its hidden
    // wearer Disadvantage; a Stone of Good Luck adds to every ability check;
    // Magic Lock Picks give the check to pick a lock Advantage.
    bool stealth_advantage{}, hard_to_perceive{}, pick_advantage{};
    int check_bonus{};
    int reach{5};
    Dice ranged;
    int range{}, long_range{}, winds{}, casting{}, level{};
    // The levels in each class (MULTI-1); `level` is their total.
    detail::ClassLevels class_levels;
    // Each level's Hit Point Die, smallest first; empty for a monster.
    std::vector<int> level_dice;
    SpellSlots slots{};
    int extra_attacks{}; // Extra Attack: attacks after the Attack action's first
    // The lowest d20 that is a Critical Hit with a weapon or Unarmed Strike:
    // the Champion's Improved Critical (19) and Superior Critical (18).
    int critical_from{20};
    int indomitable{}; // Indomitable's uses per Long Rest
    // Known and prepared spells, by id. Not a bitmask: an int caps the catalog
    // at 31 spells and the level-four milestone needs 139.
    std::vector<std::string> spells;
    // Cantrip knowledge for display. Captured before equipment is applied,
    // because untrained armor clears `spells` while the character still knows
    // the cantrip -- knowledge persists while casting is unavailable.
    std::vector<std::string> known_cantrips;
    bool str_dex_disadvantage{}, savage{}, stealth_disadvantage{};
    std::string weapon_label;
    bool melee_heavy_disadvantage{}, ranged_heavy_disadvantage{};
    std::array<int, 6> saves{};
    unsigned weapon_hands{};
    int versatile_sides{};
    bool shield{}, other_weapon{};
    // A monster's `equipment` row: it wears Medium or Heavy metal armor.
    bool metal_armor{};
    // A monster's `multiattack` row: melee attacks its Attack action makes.
    int multiattack{1};
    // A monster's `enthralling_panache` row: the save DC of the Pirate's
    // charm, which replaces one attack of its Multiattack.
    int panache_dc{};
    // A monster's `multiattack_ranged` row: its Multiattack's attacks may be
    // ranged too (a Wight's Necrotic Bow, a Priest's Radiant Flame).
    bool multiattack_ranged{};
    // A monster's `life_drain` row: a Wight's Life Drain, which replaces one
    // melee attack of its Multiattack. On a failed Constitution save it deals
    // Necrotic damage and lowers the Hit Point maximum by as much until a Long
    // Rest.
    int life_drain_dc{};
    detail::DamageDice life_drain;

    // A monster's `last_attack` row: the statistics of its Multiattack's last
    // melee attack, when that is another attack (a Giant Scorpion's Sting after
    // its two Claws).
    struct LastAttack
    {
        int bonus{};
        detail::DamageDice dice, extra;
        detail::DamageType type{}, extra_type{};
        // Its hits' riders, from `hampering` and `poisoning` rows naming it.
        bool hampering{}, poisoning{}, poisoning_own_turn{};
    };

    std::optional<LastAttack> last_attack;
    // A monster's `paralyzing_claw` row: the attack it makes instead of its
    // Multiattack at a creature it can paralyze, a hit being a Constitution
    // save against `claw_dc` or Paralyzed until the end of the target's next
    // turn (a Ghoul's Claw). `paralysis_dc` is set while the Claw is made.
    std::optional<LastAttack> claw;
    int claw_dc{}, paralysis_dc{};
    // A monster's `gore` row: the attack it makes at a Large or smaller
    // creature it has just charged `gore_feet` or more, the charge adding
    // `gore_charge` and knocking it Prone (a Minotaur's Gore).
    std::optional<LastAttack> gore;
    int gore_feet{};
    detail::DamageDice gore_charge;
    // A monster's `paralyzing_venom` row: a melee hit that drops a creature to
    // 0 Hit Points leaves it Stable, and Poisoned and Paralyzed for an hour (a
    // Phase Spider's Bite).
    bool paralyzing_venom{};
    // A monster's `bonus_charge` row: a Bonus Action that moves it up to half
    // its Speed toward an enemy it sees (a Troll's Charge). Its `rampage` row:
    // once a day, its `trait_uses`, dealing damage to a creature already
    // Bloodied on its turn spends its Bonus Action on moving up to half its
    // Speed and one melee attack (a Gnoll Warrior's Rampage).
    bool bonus_charge{}, rampage{};
    // A monster's `trampling_charge` row: a Bonus Action, Recharge
    // `trample_recharge`, moving it up to its Speed without provoking
    // Opportunity Attacks through Medium or smaller creatures, each a Strength
    // save against `trample_dc` or `trample_damage` Bludgeoning and Prone (a
    // Centaur Trooper's Trampling Charge). Its use is its `trait_uses`.
    int trample_dc{}, trample_recharge{};
    detail::DamageDice trample_damage;
    // A monster's `ethereal_jaunt` row: a Bonus Action that takes it to the
    // Ethereal Plane, and back (a Phase Spider's Ethereal Jaunt).
    bool ethereal_jaunt{};
    // A monster's Legendary Actions (SRD 5.2.1): a Vampire's Beguile and
    // Deathless Strike, an Adult Bronze Dragon's Guiding Light, Pounce and
    // Thunderclap.
    enum class LegendaryAction
    {
        beguile,
        deathless_strike,
        guiding_light,
        pounce,
        thunderclap
    };
    // A monster's `sunlight_sensitivity` row: Disadvantage on attack rolls and
    // ability checks while in sunlight.
    bool sunlight_sensitive{};

    // A monster's `innate` rows: pools of daily spell uses that spend no slot,
    // each cast with the Action or a Bonus Action (a Priest's Spellcasting and
    // Divine Aid). At most four pools of at most fifteen uses. A pool with a
    // `recharge` has one use, regained at the start of the creature's turn on
    // a d6 roll of at least that number (a Drider's Magic of the Spider Queen).
    struct InnatePool
    {
        std::string label;
        int uses{};
        int recharge{};
        bool bonus_action{};
        std::vector<std::string> spells;
    };

    std::vector<InnatePool> innate;
    // A monster's `life_drain_attack` row: its melee hits lower the target's
    // Hit Point maximum by the damage taken (a Specter's Life Drain).
    bool life_drain_attack{};
    // A monster's `flies` row: it flies over Difficult Terrain.
    bool flies{};
    // A monster's `attach` row: a melee hit attaches it to the target, which
    // it then drains of this damage at the start of each of its turns (a
    // Stirge's Proboscis).
    std::optional<detail::DamageDice> attach_drain;
    detail::DamageType attach_type{detail::DamageType::necrotic};
    // A monster's `web_walker` row: webs neither catch nor slow it (a
    // Drider's Web Walker).
    bool web_walker{};
    // A monster's `incorporeal` row: it moves through creatures and objects as
    // if they were Difficult Terrain, never ending its move inside them.
    bool incorporeal{};
    // A monster's `petrifying_gaze` row: a Basilisk's Bonus Action gaze, its
    // save DC, and the d6 roll that recharges it.
    int gaze_dc{}, gaze_recharge{};
    // A monster's `parry` row: hit by a melee attack roll, it adds this to its
    // AC with its Reaction (an SRD Knight's Parry, 2).
    int parry{};
    // A monster's `dreadful_glare` row: a Mummy's glare with its Multiattack
    // at the creature it attacks, seen within `glare_range` feet. A failed
    // Wisdom save against `glare_dc` leaves it Frightened until the end of the
    // monster's next turn; a success makes it immune to that glare for a day.
    int glare_dc{}, glare_range{};
    // A monster's `bite` row: a Vampire's Bite with its Multiattack, before its
    // attacks, at the adjacent creature it attacks when it has Charmed it or the
    // creature is Incapacitated or Restrained. A failed Constitution save
    // against `bite_dc` takes `bite_pierce` Piercing and `bite_necrotic`
    // Necrotic damage; the Necrotic lowers the Hit Point maximum and heals the
    // monster.
    int bite_dc{};
    detail::DamageDice bite_pierce, bite_necrotic;
    // A monster's `rotting_curse` row: its melee hits curse (a Mummy's
    // Rotting Fist).
    bool rotting_curse{};
    // A monster's `breath` row: its breath weapon's spell id, such as an
    // Adult Bronze Dragon's "lightning_breath", taken with its action, its save
    // DC, and the d6 roll that recharges it.
    std::string monster_breath;
    int breath_dc{}, breath_recharge{};
    // A monster's `repulsion_breath` row: an Adult Bronze Dragon's Repulsion
    // Breath in place of its Multiattack's last attack, its Strength save DC.
    int repulsion_dc{};
    // A monster's `legendary` row: its Legendary Action uses a round and the
    // options it takes them with, one right after another creature's turn.
    int legendary_uses{};
    std::vector<LegendaryAction> legendary_actions;
    // A monster's `legendary_resistance` row: the failed saves a day it turns
    // into successes.
    int legendary_resistance{};
    // A monster's hit riders by the attack that carries them, from its
    // `hampering`, `poisoning` and `pushing` rows: Disadvantage on the target's
    // next attack roll before the end of its next turn (an Ettin's Morningstar),
    // Poisoned until the end of the target's next turn or, `own_turn`, until the
    // start of the monster's next turn (a Hill Giant's Trash Lob), and a push
    // straight away (a Fire Giant's Hammer Throw).
    enum class Strike
    {
        none,
        melee,
        ranged
    };
    Strike hampering{}, poisoning{}, pushing{};
    // A monster's `flyby` row: flying out of an enemy's reach draws no
    // Opportunity Attack (a Hippogriff's Flyby).
    bool flyby{};
    // A monster's `magic_resistance` row: Advantage on saves against spells
    // (an Efreeti's Magic Resistance).
    bool magic_resistance{};
    // A monster's `protective_magic` row: its Reaction casts Shield when that
    // turns a hit into a miss, or Counterspell on an enemy's spell of level 1
    // or higher it sees within 60 feet, sharing `trait_uses` a day (a Mage's
    // Protective Magic).
    bool protective_magic{};
    bool poisoning_own_turn{};
    int push_feet{};
    // A monster's `hit_advantage` row: a melee hit gives Advantage on its next
    // attack roll this turn (a Pirate Captain's Rapier).
    bool hit_advantage{};
    // A monster's `captains_charm` row: with its Attack action it uses its
    // Bonus Action to charm the creature it attacks, seen within `charm_range`
    // feet: a Wisdom save against `charm_dc` or Charmed until the start of its
    // next turn (a Pirate Captain's Captain's Charm).
    int charm_dc{}, charm_range{};
    // A monster's `riposte` row: its Parry, and on the parried miss one melee
    // attack at the attacker (a Pirate Captain's Riposte).
    bool riposte{};
    // A monster's `create_specter` row: it raises a Humanoid corpse within 10
    // feet as the creature `specter_key` names, under its control, at most
    // `specter_limit` of them (a Wraith's Create Specter).
    int specter_limit{};
    std::string specter_key;
    // A monster's `condition_immunity` row: the conditions it never suffers,
    // as effect labels name them ("Restrained", "Prone").
    std::vector<std::string> condition_immunities;
    int hit_die{}, constitution{}, rushes{}, surges{}, arcane{};
    int lay_on_hands{};  // Paladin healing pool: five times Paladin level
    int free_smite{};    // Paladin's Smite: one Divine Smite without a slot per Long Rest
    int favored_enemy{}; // Favored Enemy: two Hunter's Marks without a slot per Long Rest
    int channel_divinity{};
    bool sacred_weapon{}; // Oath of Devotion, Paladin level 3
    bool divine_spark{};  // Cleric Channel Divinity: Divine Spark and Turn Undead
    bool evil{};          // an evil alignment: Spirit Guardians deal Necrotic damage
    // Cleric 5: Sear Undead; Life Domain 6: Blessed Healer; Cleric 7: Blessed
    // Strikes' Divine Strike (its d8s, two from 14) or Potent Spellcasting.
    bool sear_undead{}, blessed_healer{}, potent_spellcasting{};
    // Druid: the Circle of the Land's land; Wild Resurgence (5); Natural
    // Recovery's use (Land 6, in Actor::arcane); Elemental Fury (7): Potent
    // Spellcasting for Druid cantrips, or Primal Strike's d8s; Nature's Ward
    // (Land 10).
    std::string land;
    std::set<std::string, std::less<>> circle; // the land's levelled Circle Spells
    bool wild_resurgence{}, druid_potent{}, natures_ward{}, natures_sanctuary{};
    int natural_recovery{}, primal_strike{};
    int divine_strike{};
    // Cleric 10: Divine Intervention's use per Long Rest (in Actor::arcane).
    int divine_intervention{};
    // A Zombie's Undead Fortitude: a save to stay at 1 Hit Point.
    bool undead_fortitude{};
    bool life_domain{};  // Cleric level 3: Disciple of Life and Preserve Life
    bool evoker{};       // Wizard level 3: Potent Cantrip
    bool sculptor{};     // Wizard level 6: Sculpt Spells
    bool empowered{};    // Wizard level 10: Empowered Evocation
    bool overchannel{};  // Wizard level 14
    int mage_armor_ac{}; // the AC Mage Armor gives; 0 while wearing armor
    bool sleepless{};    // an Elf: Sleep has no effect
    // Barbarian Rage: uses (kept in Actor::channel_divinity, which no Barbarian
    // has) and the bonus to Strength-based weapon damage.
    int rages{}, rage_damage{};
    int strength{};                  // Strength modifier, to tell Strength-based attacks
    bool danger_sense{}, reckless{}; // Barbarian level 2
    // Monk Martial Arts: unarmored, unarmed or with only Monk weapons. The
    // modifier is the better of Strength and Dexterity.
    bool martial_arts{};
    int martial_modifier{};
    // Monk level 2: Focus Points (kept in Actor::surges, which no Monk has) and
    // Uncanny Metabolism's once-per-Long-Rest use (kept in Actor::arcane).
    int focus{}, metabolism{};
    int innate_sorcery{}; // Sorcerer: Innate Sorcery uses, in Actor::free_casts
    // Sorcerer level 5: Sorcerous Restoration's use, in Actor::arcane.
    int sorcerous_restoration{};
    // Sorcerer level 6: Elemental Affinity's type; level 7: Sorcery
    // Incarnate; level 14: Dragon Wings.
    std::optional<detail::DamageType> elemental_affinity;
    bool sorcery_incarnate{}, dragon_wings{};
    // Warlock invocations: Thirsting Blade, Eldritch Smite and Lifedrinker;
    // the Fiend's Dark One's Own Luck uses (level 6).
    bool thirsting_blade{}, eldritch_smite{}, lifedrinker{};
    int dark_luck{};
    // Witch Sight (invocation); Hurl Through Hell (Fiend 14); Mystic
    // Arcanum's spells (Warlock 11, 13, 15).
    bool witch_sight{}, hurl_through_hell{};
    // Gift of the Protectors (invocation, LEVELS-5).
    bool gift_of_protectors{};
    std::set<std::string, std::less<>> arcanum;
    // Magic Initiate: the spell attack bonus (the feat's ability modifier plus
    // Proficiency Bonus) of each spell it teaches, and each feat's level-one
    // spell in feat order, cast once per Long Rest without a slot.
    std::map<std::string, int, std::less<>> initiate_casting;
    // The free casts that come from a lineage rather than Magic Initiate.
    std::set<std::string, std::less<>> lineage_spells;
    std::vector<std::string> initiate_spells;
    int sorcery_points{};               // Sorcerer level 2: Font of Magic, in Actor::lay_on_hands
    std::vector<std::string> metamagic; // the Sorcerer's Metamagic options
    bool pact_magic{};                  // Warlock: slots of one level, back on a Short Rest
    // Pact Magic's slots, of one level. A Warlock with another Spellcasting
    // class has them among `slots`, and a Short Rest gives back up to this
    // many of that level (MULTI-1).
    int pact_slot_level{}, pact_slots{};
    // The scrolls it can read (MAGIC-6), as the original's magic-user and
    // cleric scrolls: an arcane caster reads arcane scrolls, a divine or primal
    // caster divine ones.
    bool reads_arcane_scrolls{}, reads_divine_scrolls{};
    // Eldritch Invocations.
    bool armor_of_shadows{}, devils_sight{}, eldritch_mind{}, fiendish_vigor{}, pact_of_the_blade{};
    // The cantrips Agonizing Blast, Eldritch Spear and Repelling Blast improve.
    std::set<std::string, std::less<>> agonizing, eldritch_spear, repelling;
    int magical_cunning{}; // Warlock level 2: once per Long Rest, in Actor::arcane
    // Fiend Patron, Warlock level 3: Temporary Hit Points when an enemy drops.
    int dark_ones_blessing{};
    // Bard: Bardic Inspiration uses (Charisma modifier, at least one), kept in
    // Actor::arcane, which no Bard has; Cutting Words from level 3.
    int bardic_inspiration{};
    bool cutting_words{};
    // The Bardic Inspiration die's sides (d8 at Bard 5, d10 at 10, d12 at 15);
    // Font of Inspiration (5), Countercharm (7) and Peerless Skill (Lore 14).
    int bardic_die{6};
    bool font_of_inspiration{}, countercharm{}, peerless_skill{};
    bool shillelagh_weapon{}; // the melee weapon is a Club or Quarterstaff
    // Druid: Wild Shape uses, kept in Actor::channel_divinity, which no Druid
    // has; Land's Aid spends them from level 3.
    int wild_shapes{};
    bool lands_aid{};
    bool prone_bite{}, bloodied_fury{}, charge{}; // a Beast form's attack traits
    // A charge's run, its extra damage and the largest size it knocks Prone:
    // a Boar's unless a Gore is being made.
    int charge_feet{20}, charge_size{2};
    detail::DamageDice charge_dice{1, 6, 0};
    bool prone_large{}; // the Tiger's: a Large or smaller target falls Prone
    // Nimble Escape (the Tiger's, a monster's row): Disengage or Hide as a
    // Bonus Action.
    bool nimble_escape{};
    bool deflect{};     // Monk level 3: Deflect Attacks
    bool open_hand{};   // Warrior of the Open Hand, Monk level 3
    int focus_dc{};     // 8 + Wisdom + Proficiency, for Focus features' saves
    int dexterity{};    // Dexterity modifier
    bool frenzy{};      // Berserker, Barbarian level 3
    bool heavy_armor{}; // wearing Heavy armor, which prevents Rage
    // A Wizard or Sorcerer wearing no armor, or a monster magic-user, which the
    // computer keeps out of melee as the original did its magic-users (#266).
    bool unarmored_arcane_caster{};
    // Hunter, Ranger level 3: Hunter's Lore and one Hunter's Prey option.
    bool hunters_lore{}, colossus_slayer{}, horde_breaker{};
    // Hunter, Ranger level 7: one Defensive Tactics option.
    bool escape_the_horde{}, multiattack_defense{};
    // Ranger levels 10-15: Tireless's uses per Long Rest (kept in
    // Actor::arcane, which no Ranger has otherwise), Superior Hunter's Prey,
    // Relentless Hunter, Nature's Veil's uses (in Actor::lay_on_hands) and
    // Superior Hunter's Defense.
    int tireless{}, natures_veil{};
    bool superior_prey{}, relentless_hunter{}, superior_defense{};
    bool gnomish_cunning{}; // Gnome: Advantage on Intelligence, Wisdom and Charisma saves
    bool relentless{};      // Orc: Relentless Endurance, once per Long Rest
    // Dragonborn: the Breath Weapon's damage type, from the Draconic Ancestry.
    std::optional<detail::DamageType> breath;
    GiantBoon giant{GiantBoon::none}; // Goliath: the Giant Ancestry's boon
    int trait_uses{};   // uses of a species trait per Long Rest (Breath Weapon, Giant Ancestry)
    bool resourceful{}; // Human: Heroic Inspiration, back after each Long Rest
    bool lucky{};       // Halfling: a d20 test's 1 is rolled again
    bool nimble{};      // Halfling Nimbleness: moves through larger creatures' spaces
    bool naturally_stealthy{}; // Halfling: a larger creature is enough to hide behind
    bool tactical_shift{}, tactical_master{}, heroic_warrior{}, studied_attacks{};
    // Barbarian levels 5-10.
    bool feral_instinct{}, mindless_rage{}, instinctive_pounce{}, brutal_strike{}, retaliation{};
    // Barbarian levels 11-15: Relentless Rage's tries before a Short Rest (kept
    // in Actor::lay_on_hands, which no Barbarian has), Intimidating Presence's
    // use (in Actor::free_casts) and Persistent Rage's (in Actor::arcane), each
    // once per Long Rest; Improved Brutal Strike's two more blows.
    int relentless_rage{}, intimidating_presence{}, persistent_rage{};
    bool improved_cunning{}, devious_strikes{}; // Rogue 11 and 14
    bool improved_brutal{};
    bool brave{};        // Halfling: Advantage on saves to avoid or end Frightened
    bool fey_ancestry{}; // Elf: Advantage on saves to avoid Charmed
    bool elf{};          // a Ghoul's Claw cannot paralyze an elf
    // Rogue levels 5-10.
    bool cunning_strike{}, uncanny_dodge{}, evasion{}, supreme_sneak{};
    bool fast_hands{};       // Thief: a magic item used as a Bonus Action (MAGIC-3)
    bool use_magic_device{}; // Thief 13: a 6 on 1d6 keeps a spent charge; reads any scroll
    // Monk levels 5-9: the Martial Arts die's sides, Stunning Strike and
    // Wholeness of Body's uses per Long Rest.
    int martial_die{6}, wholeness{}, wisdom{};
    bool stunning_strike{}, empowered_strikes{};
    // Monk levels 10-15: Heightened Focus, Self-Restoration, Fleet Step,
    // Deflect Energy and Perfect Focus.
    bool heightened_focus{}, self_restoration{}, fleet_step{}, deflect_energy{}, perfect_focus{};
    bool disciplined_survivor{};
    // Paladin: Aura of Protection's bonus to saves (Charisma modifier, at
    // least +1; 0 below level 6) and Aura of Devotion (level 7).
    int aura_of_protection{};
    bool aura_of_devotion{};
    // Paladin level 9: Abjure Foes' creatures (Charisma modifier, at least 1);
    // level 10: Aura of Courage.
    int abjure_foes{};
    bool aura_of_courage{};
    bool radiant_strikes{}; // Paladin level 11
    // Paladin level 14: Restoring Touch; level 15: Smite of Protection.
    bool restoring_touch{}, smite_of_protection{};
    // Reliable Talent (Rogue 7): the proficient skills whose d20 counts as at least 10.
    std::set<std::string, std::less<>> reliable;
    bool dwarf{}, cunning{}, tactical_mind{}, champion{}, great_weapon_fighting{},
        two_weapon_fighting{};
    unsigned sneak_level{};
    bool finesse{}, ranged_weapon{};
    bool ranged_loading{}; // the ranged weapon is a Loading one: one shot per action
    // Its ranged attack is no carried weapon, such as a spell or a thrown rock,
    // so it keeps it without one.
    bool innate_ranged{};
    bool ranged_two_handed{}; // The ranged weapon it holds needs both hands.
    // What its ranged attack sends flying; unset, a stat block's weapon by range.
    std::optional<rules::Missile> ranged_missile;
    int medicine{};
    int athletics{};     // a character's Strength (Athletics) check bonus
    int investigation{}; // and Intelligence (Investigation) check bonus
    int arcana{};        // and Intelligence (Arcana), for Use Magic Device's scrolls
    // Wisdom (Perception) and Dexterity (Stealth) check bonuses; a creature's
    // Perception comes from its stat block.
    int perception{}, stealth{};
    detail::DamageType melee_type{detail::DamageType::bludgeoning},
        ranged_type{detail::DamageType::bludgeoning};
    std::vector<detail::DamageAffinity> affinities;
    std::vector<std::string> equipment_keys, masteries;
    // Monster traits from supplemental content rows.
    bool pack_tactics{}, aggressive{};
    Dice advantage_damage; // Extra weapon damage when the attack roll had Advantage.
    // A second damage type a ranged hit adds (the Hobgoblin Warrior's poisoned
    // arrows), and one a melee hit adds (a Ghoul's Necrotic bite).
    Dice ranged_extra, melee_extra;
    detail::DamageType ranged_extra_type{detail::DamageType::poison},
        melee_extra_type{detail::DamageType::necrotic};
    // Hit Points regained at the start of each turn; Acid or Fire damage stops it
    // for the next turn, and the creature dies only at 0 HP without regenerating.
    int regeneration{};
};

// What a weapon sends flying: the original's missile art has a stone for a
// sling and one thrown weapon, the axe, for every thrown weapon.
[[nodiscard]] rules::Missile weapon_missile(const detail::Weapon &weapon);

struct CombatDisplay
{
    const char *type;
    const char *melee;
    const char *ranged;
};

[[nodiscard]] CombatDisplay combat_display(std::string_view definition);

struct Content
{
    Identity identity;
    std::map<std::string, Definition> definitions;
};

// A class resource's count, one for each class (MULTI-1): a multiclass
// character's Bardic Inspiration and Arcane Recovery are its own, though both
// kinds are kept in Actor::arcane. A monster's resources sit under the class
// whose feature they copy.
struct ClassCounts
{
    std::array<int, 12> by_class{};

    [[nodiscard]] int &operator[](CharacterClass klass) noexcept
    {
        return by_class[static_cast<std::size_t>(klass)];
    }

    [[nodiscard]] int operator[](CharacterClass klass) const noexcept
    {
        return by_class[static_cast<std::size_t>(klass)];
    }

    bool operator==(const ClassCounts &) const = default;
};

// An actor has a life state (hit points, death saves, recovery); it is not
// one, so it holds it rather than inheriting it, and two actors do not compare
// equal merely because their hit points match (Effective C++ Item 38).
struct Actor
{
    detail::LifeState life;
    Participant source;
    Definition definition;
    int initiative{}, movement{}, winds{};
    SpellSlots slots{};
    int hit_dice{}, rushes{}, dashes{};
    // Class resources, each class's its own (MULTI-1): Action Surge and Focus;
    // the once-a-rest features; Lay On Hands' pool and other point pools; free
    // casts (Paladin's Smite, Favored Enemy, Innate Sorcery and the like);
    // Channel Divinity (a Cleric's and a Paladin's together), Rage and Wild Shape.
    ClassCounts surges, arcane, lay_on_hands, free_casts, channel_divinity;
    // Magic Initiate's unspent free casts: bit n is Definition::initiate_spells[n].
    int initiate_casts{};
    // The uses each of Definition::innate's pools has spent, four bits a pool.
    int innate_spent{};
    // The creature this actor just hit with a melee weapon on its own turn, which
    // a smite may follow; cleared by any other command or a new turn.
    EntityId smite_target{};
    bool smite_critical{};
    bool
        smite_melee{}; // Divine and Searing Smite need a Melee hit; Ensnaring Strike any weapon hit
    bool rush_used{}, surge_used{};
    detail::ActionBudget actions;
    bool bonus{true}, reaction{true}, dodge{}, disengaged{};
    bool spent_slot{}, savage_used{}, facing_left{};
    bool sneak_used{}, aim_used{}, aim_ready{}, moved{};
    bool attacked{};    // made an attack roll on its own turn (Reckless Attack's first)
    int pounce{};       // Instinctive Pounce's feet, usable only right after entering a Rage
    bool brutal_used{}; // Brutal Strike, once on each of its turns
    bool stun_used{};   // Stunning Strike, once on each turn
    // Fleet Step (Monk 11): after another Bonus Action, Step of the Wind may
    // still be used this turn.
    bool fleet_step{};
    // Heightened Focus (Monk 10): the ally Step of the Wind carries this turn.
    EntityId carried{};
    // The straight line just moved along this turn, for a Boar's charge: its
    // first and last cells, the step repeated and its length. Any command but
    // moving or a melee attack ends it.
    Cell run_start{}, run_end{}, run_step{};
    int run_feet{};
    unsigned selected_weapon{};
    std::vector<unsigned> light_origins;
    unsigned nick_origin{}; // Light weapon used by the current Attack action.
    unsigned light_extra{}; // 0: unused, 1: Bonus Action, 2: Nick; shared once per turn.
    bool cleave_used{}, cleave_damage{};
    // Hunter's Prey, once per turn. horde_origin is the first creature this
    // actor attacked with a weapon this turn, which Horde Breaker attacks beside.
    bool colossus_used{}, horde_used{};
    // Superior Hunter's Prey, once per turn: prey_origin is the marked creature
    // just damaged, within 30 feet of which the mark's damage may strike another.
    bool prey_used{};
    EntityId prey_origin{};
    bool strike_used{}; // Divine Strike, once on each of its turns
    // Eldritch Smite, Lifedrinker and Hurl Through Hell (Warlock), each once
    // on each of its turns.
    bool smite_used{}, drink_used{}, hurl_used{};
    // Dominated (Dominate Beast): it fights on its controller's side, not its
    // own. A controlled creature (dominated or raised) waits out the turn it
    // would get again in the round it came under control after it had acted.
    bool turned{}, skips_turn{};
    bool resistance_used{};      // Resistance (the cantrip) reduces damage once per turn
    bool regeneration_blocked{}; // Took Acid or Fire damage since its last turn began
    // Downed with Regeneration: the round it may get back up, 3d6 rounds after it
    // fell (the original game's troll); 0 otherwise.
    int rises_on_round{};
    // Thrown gear (Acid, Alchemist's Fire, Oil, by ThrownGearEffect) still carried:
    // counted from the inventory when combat begins and spent by each throw.
    std::array<unsigned, 3> thrown_gear_left{};
    // Charged items (MAGIC-3) by inventory ID: the charges, missiles or potions
    // left, counted from the inventory when combat begins and spent by each use.
    std::vector<std::pair<std::uint64_t, unsigned>> item_uses;
    bool burning{};            // Burning: 1d4 Fire damage at the start of each of its turns
    bool flame_lit{};          // Its Flame Tongue's blade is lit (a Bonus Action lights it)
    bool lightning_throw{};    // Its Javelin of Lightning's bolt flies, within one command
    bool keeps_charge{};       // Use Magic Device saved this use's charge, within one command
    bool curse_lifted{};       // Remove Curse freed it from its cursed weapon (MAGIC-1)
    bool displacement_spent{}; // Its Cloak of Displacement's miss is used this fight (MAGIC-4)
    // The enemy its last command on its own turn aimed at, for the computer (#266).
    rules::EntityId last_target{};
    int oiled_until_round{};  // Covered in Oil through this round: Fire damage deals 5 more
    int coated_until_round{}; // Its weapon carries Basic Poison through this round; 0 for none
    bool shield_off{};        // Took its shield off this fight (for a two-handed bow)
    // The weapon it drew to fire, in place of the one it holds, until it strikes in
    // melee or selects a weapon. Its Definition is unchanged: drawing costs nothing.
    std::string drawn_weapon;
    rules::WeaponSetShown weapon_set_shown{rules::WeaponSetShown::staged};
    // Tried to run off the field as fast as the fastest enemy and failed: a
    // party member stays until the fight ends (the original's rule). A broken
    // monster that still cannot get away charges in panic instead (MORALE-2).
    bool must_stay{};
    // Ran off the field this turn; it leaves the fight when the turn ends.
    bool fled{};
    // Its morale broke: it runs for the edge until it rallies.
    bool panicked{};
    // How near its flight has come to the field's edge, and for how many of its
    // turns since then it has come no nearer.
    int closest_to_edge{}, turns_stuck{};
    // Gave up: it leaves the fight, counted as defeated.
    bool surrendered{};
    EntityId horde_origin{};
    detail::ConcentrationState concentration; // the one Concentration spell this actor keeps
    // Wild Shape: the Beast form's statistics, derived from the wild_shape effect.
    std::optional<Definition> form;
    bool light_damage{};        // Lent for one attack only; pending hits carry their own flag.
    bool involuntary_overlap{}; // Interrupted in an occupied space; retained through recovery until
    // separated.
    detail::EffectState effects;
    // Spell effects an Antimagic Field suppresses while the creature stands in
    // it; their time still runs, and they return when it leaves.
    std::vector<detail::Effect> suppressed;
};

// The actor's effects with those an Antimagic Field suppresses, in the order
// they were applied: what a save or a checkpoint keeps.
[[nodiscard]] detail::EffectState all_effects(const Actor &a);

// What the value placeholder names in an effect's description.
[[nodiscard]] rules::MessageArgument effect_value(const detail::Effect &effect,
                                                  detail::EffectValue value);

// Durations past this are the "until Greater Restoration" kinds, never counted
// down to an end.
inline constexpr unsigned lasting_effect_ms = 2'592'000'000u; // 30 days

// Every status effect a player would notice on the actor, in the order they
// were applied, then the states that are not effects: Prone, Burning, oil,
// panic and Concentration.
[[nodiscard]] std::vector<rules::StatusEffectView> status_effect_views(const Actor &a, bool oiled);

// The actor's AC with its lasting effects. Mage Armor replaces an unarmored
// base AC. Barkskin: an AC of 17 if it was lower. Shield of Faith: +2. Warding
// Bond: +1. Shield: +5.
[[nodiscard]] int armor_class_of(const Actor &target);

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

// The slot level a spell verb casts at: a "_N" verb names a slot above the
// spell's level; otherwise the spell's own level.
[[nodiscard]] unsigned slot_level_of(std::string_view verb, const detail::SpellDef &spell);

// Slot levels above the spell's own: each adds its upcast once more.
[[nodiscard]] unsigned upcast_steps(std::string_view verb, const detail::SpellDef &spell);

// Whether a slot of `level`, above the spell's own, changes the spell: more
// dice, creatures or area, or a rider that grows with the slot.
[[nodiscard]] bool upcast_matters(const detail::SpellDef &spell, unsigned level);

// The slot levels a spell is offered at: each level that changes it, and the
// lowest with a slot left, its own level while those last.
[[nodiscard]] std::vector<unsigned> cast_levels(const Actor &a, const detail::SpellDef &spell);

// The verb and label for a cast at `level`; above the spell's own level, the
// verb gains "_3" and the label " (level 3 slot)" for a level-3 slot.
[[nodiscard]] std::pair<std::string, std::string>
cast_at(const detail::SpellDef &spell, unsigned level, std::string verb, std::string label);

// Whether a Mystic Arcanum spell of this level was cast since the last Long Rest.
[[nodiscard]] bool arcanum_spent(const Actor &a, unsigned level);

// The charged item `a` can use for `spell` now (MAGIC-3): one with uses left,
// and a wand only while one of its kind is held. Null when there is none.
[[nodiscard]] const detail::MagicItem *usable_item(const Actor &a, const detail::SpellDef &spell);

// The uses left of the first carried vial of Basic Poison with any, or null.
[[nodiscard]] const unsigned *basic_poison_left(const Actor &a);

[[nodiscard]] unsigned *basic_poison_left(Actor &a);

// Spends a use of the first carried item of `kind` with uses left.
void spend_item_use(Actor &a, const detail::MagicItem &kind);

// A spell still on a carried scroll (MAGIC-6): the scroll's inventory ID, the
// spell's place on it and the slot level it is cast at.
struct ScrollEntry
{
    std::uint64_t item{};
    unsigned index{};
    unsigned level{};
};

// The first spell `spell` that `a` can read from a carried scroll this fight,
// at `level` when it is not 0: an identified scroll of its kind of magic, with
// the spell not yet read. Wearing armor without training, it reads none.
[[nodiscard]] std::optional<ScrollEntry>
readable_scroll(const Actor &a, const detail::SpellDef &spell, unsigned level = 0);

// SRD 5.2.1's Spell Scroll: the save DC by the spell's level; its attack
// bonus is the DC less 8.
// Use Magic Device (MAGIC-6, SRD 5.2.1): a Thief who reads scrolls only
// through it first makes an Intelligence (Arcana) check, DC 10 + the level,
// for a spell above level 1. A failure loses the spell.
[[nodiscard]] bool needs_arcana_check(const Definition &d, unsigned level);

[[nodiscard]] int scroll_dc(unsigned level);

// Whether a verb uses a carried item, and the use is not the casting of a
// spell: a potion, a missile or dust. A wand casts its spell.
[[nodiscard]] bool item_effect(std::string_view verb);

// The creature a Stirge clings to, if it clings to one.
[[nodiscard]] std::optional<EntityId> attached_host(const Actor &a);

// The uses a pool of the actor's innate spells has left.
[[nodiscard]] int innate_left(const Actor &a, std::size_t pool);

// The pool of the actor's innate spells, cast with a Bonus Action or with the
// Action, that can cast the spell now; none when no such pool has a use left.
[[nodiscard]] std::optional<std::size_t> innate_pool(const Actor &a, const detail::SpellDef &spell,
                                                     bool bonus_action);

// Whether `spent` is a count of uses each innate pool of `d` could have spent.
[[nodiscard]] bool valid_innate_spent(const Definition &d, int spent);

// Whether a verb casts a spell with a Bonus Action that is not the spell's own
// casting time: an item used with Fast Hands, or an innate Bonus Action spell.
[[nodiscard]] bool bonus_action_verb(std::string_view verb);

// Pays for a leveled spell: with Magic Initiate's free cast, or from a slot of
// the level it is cast at, the turn's one slot.
void pay_for_spell(Actor &a, std::string_view verb, const detail::SpellDef &spell);

[[nodiscard]] bool unconscious(const Actor &a);

// Raise Dead's penalty to D20 Tests, one less after each Long Rest.
[[nodiscard]] int return_penalty(const Actor &a);

[[nodiscard]] bool conscious(const Actor &a);

// Banished to a demiplane, or lost in Maze's: off the field until the spell ends.
[[nodiscard]] bool absent(const Actor &a);

// Fallen upward by Reverse Gravity: out of melee reach until it ends.
[[nodiscard]] bool aloft(const Actor &a);

// One Arcane Recovery choice: how many slots of each level from 1 to 5 return.
struct ArcaneAllocation
{
    std::string id;
    Message label;
    std::array<unsigned, 5> slots{};
};

// The slot levels a choice restores, highest first: "3, 1, 1".
[[nodiscard]] std::string recovered_levels(const std::array<unsigned, 5> &slots);

// Arcane Recovery's choices: expended slots of levels 1 to 5 whose levels add
// up to at most `budget`, half the Wizard level rounded up (SRD 5.2.1 p. 78).
// The three small choices keep the names they had before level 5.
[[nodiscard]] std::vector<ArcaneAllocation> arcane_allocations(unsigned budget);

// Whether an effect gives the named Lesser Restoration condition.
[[nodiscard]] bool gives_condition(detail::EffectKind kind, std::string_view condition);

[[nodiscard]] std::string condition_name(std::string_view condition);

[[nodiscard]] bool has_condition(const Actor &a, std::string_view condition);

// Poisoned: Disadvantage on attack rolls and ability checks. Heroes' Feast
// makes a creature immune.
[[nodiscard]] bool suffers_poisoned(const Actor &a);

// The conditions Restoring Touch ends, most disabling first, and the effects
// that give each. Deafened is not modeled (DEAF-1).
inline constexpr std::array<std::pair<std::string_view, std::array<detail::EffectKind, 3>>, 5>
    restorable_conditions{
        {{"Paralyzed",
          {detail::EffectKind::hold_person, detail::EffectKind::paralysis_ray,
           detail::EffectKind::clawed}},
         {"Stunned",
          {detail::EffectKind::stunned, detail::EffectKind::stunned, detail::EffectKind::stunned}},
         {"Blinded",
          {detail::EffectKind::blindness, detail::EffectKind::dazzled,
           detail::EffectKind::dazzled}},
         {"Frightened",
          {detail::EffectKind::frightened, detail::EffectKind::abjured,
           detail::EffectKind::abjured}},
         {"Charmed",
          {detail::EffectKind::charmed, detail::EffectKind::enthralled,
           detail::EffectKind::turn_charmed}}}};

[[nodiscard]] bool touch_restorable(const Actor &a);

// A curse on the creature, which Remove Curse ends: Hex so far.
[[nodiscard]] bool curse(detail::EffectKind kind);

// Raise Dead, Resurrection and Reincarnate bring a dead creature back with
// `hit_points`, its poisons neutralized and a body turned to dust restored.
void bring_back(Actor &patient, int hit_points);

// A Poisoned effect other than Contagion's disease.
[[nodiscard]] bool poison(detail::EffectKind kind);

// Whether a temple's service would help the creature (TEMPLE-1).
[[nodiscard]] bool temple_helps(const Actor &a, TempleService service);

// It wields or wears a cursed item and cannot put it down until Remove Curse
// (MAGIC-1). With `weapons_only`, only a cursed weapon counts: a cursed necklace
// does not stop a weapon swap.
[[nodiscard]] bool bound_to_cursed_item(const Actor &a, bool weapons_only = false);

// A snake or scorpion, the original's creature kind that Snake Charm affects
// (MAGIC-6), known by its combat creature's name.
[[nodiscard]] bool charmable_snake(const Actor &c);

[[nodiscard]] bool cursed(const Actor &a);

// "Celestials", "Elementals", "Fey", "Fiends" or "Undead".
[[nodiscard]] std::string ward_label(std::size_t type);

// A Bestow Curse curse as players read it, by its index in curses.
[[nodiscard]] std::string curse_label(std::size_t curse);

// A wall's direction as players read it, by its index in wall_directions.
[[nodiscard]] std::string wall_direction_label(std::size_t direction);

// The directions Compulsion names, 0 north and on clockwise to 7.
inline constexpr std::array<Cell, 8> compass{Cell{0, -1}, Cell{1, -1}, Cell{1, 0},  Cell{1, 1},
                                             Cell{0, 1},  Cell{-1, 1}, Cell{-1, 0}, Cell{-1, -1}};

inline constexpr std::array<std::string_view, 8> compass_names{
    "north", "northeast", "east", "southeast", "south", "southwest", "west", "northwest"};

// Tsunami's headings, by the index its zone keeps in `dealt`.
inline constexpr std::array<Cell, 8> wave_headings{Cell{0, 1},  Cell{0, -1}, Cell{1, 0},
                                                   Cell{-1, 0}, Cell{1, 1},  Cell{-1, -1},
                                                   Cell{1, -1}, Cell{-1, 1}};

// Tsunami's heading: across its wall, away from the caster at `origin`.
[[nodiscard]] std::size_t wave_heading(std::string_view verb, Cell origin, Cell center);

// The ward type a "magic_circle_fey" verb names.
[[nodiscard]] std::size_t ward_of(std::string_view verb);

// Whether Greater Restoration has a harm to end on the creature, by its
// index in greater_harms.
[[nodiscard]] bool has_greater_harm(const Actor &a, std::size_t harm);

// Whether Lesser Restoration has a condition to end on the creature.
[[nodiscard]] bool restorable(const Actor &a);

// The Hit Point Die a creature with `remaining` of them spends next: the
// largest it has left. A multiclass character's dice differ (MULTI-1); it
// spends its largest first, so those left are always its smallest, and a Long
// Rest gives all of them back.
[[nodiscard]] int next_hit_die(const Definition &d, int remaining);

// The level Arcane Recovery (Wizard) or Natural Recovery (Land Druid) counts:
// that class's, the higher of the two for a character with both (MULTI-1).
[[nodiscard]] int recovery_class_level(const Definition &d);

[[nodiscard]] bool can_recover(const Actor &actor, const ArcaneAllocation &choice);

[[nodiscard]] int movement_left(const Actor &a);

struct ChampionMove
{
    EntityId actor{}, target{};
    int natural{}, remaining{};
    bool spell{};
    Cell origin;
    bool triggered{}, cleave{}, helpless{};
    Cell trigger_origin{}, target_origin{};
    // Tactical Shift (Fighter 5): the same free move, after Second Wind.
    bool shift{};
    // Forceful Blow (Barbarian 9): the free move must close on this creature.
    EntityId toward{};
    // Cunning Strike's Withdraw (Rogue 5): the same free move after the attack.
    bool withdraw{};
};

struct PendingMastery
{
    EntityId actor{}, target{};
    detail::Mastery kind{detail::Mastery::none};
    int natural{};
    bool ranged{}, targeting{};
    std::string weapon;
    Cell origin;
    unsigned thrown_item{};
    bool tactical{}; // Tactical Master replaced the weapon's own mastery
};

struct EffectReaction
{
    EntityId actor{};
    Cell source{}, mover{};
    int movement{};
    bool prone{};
};

struct PendingGraze
{
    EntityId actor{}, target{};
    int natural{};
};

struct PendingCheck
{
    EntityId actor{}, target{};
    int natural{};
    bool surge_spent{};
    int guidance{}; // Guidance's 1d4, rolled with the check; 0 without it.
};

// `draconic`: a Draconic Sorcerer gains its level in Hit Points from level 3.
// The Manual of Bodily Health (MAGIC-3): each one read adds this grant, from
// this source, and 2 to Constitution and its maximum.
inline constexpr std::string_view bodily_health = "boon:bodily_health";

// A spell a Wizard copied from a scroll (MAGIC-6): "boon:scroll:fireball".
inline constexpr std::string_view copied_spell = "boon:scroll:";

inline constexpr std::string_view manual_source = "item:manual_of_bodily_health";

// The grants a class, species, background or feat gives, without a Manual's.
[[nodiscard]] std::vector<FeatureGrant> without_manuals(std::vector<FeatureGrant> grants);

// The starting class's whole die at level 1, then each level's class's
// average (SRD 5.2.1 pp. 23 and 25, MULTI-1), with Constitution's history, a
// Dwarf's Toughness and Draconic Resilience's 1 per Sorcerer level from 3.
[[nodiscard]] int maximum_hit_points(const detail::ClassHistory &history, bool dwarf,
                                     std::span<const int> modifiers);

// Module-owned character recipe. Original item IDs never enter this layer.
[[nodiscard]] Definition
character_definition(std::string_view bytes,
                     std::optional<std::span<const std::string>> equipment_override = std::nullopt);

// The only vital-state tag this module reads or writes; see profile_magic.
inline constexpr std::string_view vitals_magic = "SRD13";

void restore_vitals(Actor &a, const VitalState &state);

// An actor for a rules call outside combat: full Second Winds and slots from
// its definition, then its saved vitals. One place, so a new resource cannot be
// missed in one of the many callers.
[[nodiscard]] Actor actor_from(const Definition &definition, const VitalState &state);

// Befuddlement lasts the fight (LEVELS-5).
void end_befuddlement(Actor &a);

[[nodiscard]] VitalState vitals(const Actor &a);

constexpr int distance(Cell a, Cell b)
{
    // std::abs is not constexpr before C++23.
    const int across = a.x > b.x ? a.x - b.x : b.x - a.x;
    const int down = a.y > b.y ? a.y - b.y : b.y - a.y;
    return std::max(across, down) * 5;
}

// The squares a creature fills now: its Beast form's while it has one (FOOT-1).
[[nodiscard]] Footprint footprint(const Actor &a);

[[nodiscard]] std::vector<Cell> squares_of(Cell anchor, Footprint footprint);

[[nodiscard]] std::vector<Cell> squares_of(const Actor &a);

[[nodiscard]] bool occupies(const Actor &a, Cell cell);

// Whether any of the creature's squares is one of `cells`.
[[nodiscard]] bool touches(const std::vector<Cell> &cells, const Actor &a);

// Feet between the nearest squares of two footprints: 5 when they touch.
constexpr int distance(Cell a, Footprint a_size, Cell b, Footprint b_size)
{
    const int across =
        std::max({0, a.x - (b.x + b_size.width - 1), b.x - (a.x + a_size.width - 1)});
    const int down =
        std::max({0, a.y - (b.y + b_size.height - 1), b.y - (a.y + a_size.height - 1)});
    return std::max(across, down) * 5;
}

[[nodiscard]] int distance(const Actor &a, const Actor &b);

[[nodiscard]] int distance(const Actor &a, Cell cell);

[[nodiscard]] int distance(Cell cell, const Actor &a);

// To a creature of `size` with its top-left square at `anchor`, such as a
// mover's next square.
[[nodiscard]] int distance(const Actor &a, Cell anchor, Footprint size);

// The creature's square nearest `toward`: where an area it casts or breathes
// starts and its range is measured from (FOOT-1).
[[nodiscard]] Cell origin_toward(const Actor &a, Cell toward);

// A summoned creature's squares, as its combat art fills them: a Giant
// Insect's wide art, a Draconic Spirit's 2 by 2 (FOOT-1).
[[nodiscard]] Footprint summoned_footprint(std::string_view definition);

// A creature holding the weapons its art shows (#274): their dice, damage
// types, enchantment and range in place of its stat block's, with the stat
// block's own attack and damage bonuses. Reach stays 5 feet (MELEE-1).
// A monster as the difficulty made it (DIFF-2).
[[nodiscard]] Definition strengthened(Definition d, const rules::Participant &who);

[[nodiscard]] Definition wielding(Definition d, std::string_view melee_key,
                                  std::string_view ranged_key);

// A monster as a fight fields it: its stat line, the weapons it holds and
// what the difficulty does to it.
[[nodiscard]] Definition fielded(const Content &content, const rules::Participant &who);

[[nodiscard]] bool same_command(const Command &a, const Command &b);

// A character's statistics after taking off its shield: the same equipment
// without it.
[[nodiscard]] Definition without_shield(std::string_view profile, const Definition &d);

// A monster's statistics after taking off its shield, which gives +2 Armor Class.
[[nodiscard]] Definition monster_without_shield(Definition d);

// What the computer weighs a melee weapon by, over its attacks a turn.
[[nodiscard]] int melee_rating(const Definition &d);

[[nodiscard]] bool wears_shield(const Definition &d);

// Tactical Master's three masteries, by the attack verbs' suffixes, with
// their commands' labels.
struct TacticalOption
{
    std::string_view suffix;
    detail::Mastery kind;
    std::string_view melee_label, ranged_label;
};

inline constexpr std::array<TacticalOption, 3> tactical_masteries{
    {{"push", detail::Mastery::push, "Melee attack (Push)", "Ranged attack (Push)"},
     {"sap", detail::Mastery::sap, "Melee attack (Sap)", "Ranged attack (Sap)"},
     {"slow", detail::Mastery::slow, "Melee attack (Slow)", "Ranged attack (Slow)"}}};

// Brutal Strike's blows (Barbarian 9; Staggering and Sundering from 13).
enum class BrutalBlow : unsigned
{
    forceful,
    hamstring,
    staggering,
    sundering
};

// Brutal Strike's blows by verb, with their commands' labels.
inline constexpr std::array<std::tuple<std::string_view, std::string_view, BrutalBlow>, 4>
    brutal_blows{{{"brutal_forceful", "Brutal Strike (Forceful Blow)", BrutalBlow::forceful},
                  {"brutal_hamstring", "Brutal Strike (Hamstring Blow)", BrutalBlow::hamstring},
                  {"brutal_staggering", "Brutal Strike (Staggering Blow)", BrutalBlow::staggering},
                  {"brutal_sundering", "Brutal Strike (Sundering Blow)", BrutalBlow::sundering}}};

// Cunning Strike's effects (Rogue 5).
enum class CunningStrike : unsigned
{
    poison, // with a Poisoner's Kit
    trip,
    withdraw,
    stealth, // Supreme Sneak's Stealth Attack (Thief 9)
    daze,    // Devious Strikes (Rogue 14)
    knock_out,
    obscure
};

// Cunning Strike's options by verb suffix, with their names and the Sneak
// Attack dice they cost.
// A command's verb is "melee_" or "ranged_" and one suffix, or two joined by
// "+" with Improved Cunning Strike (Rogue 11).
struct CunningOption
{
    std::string_view suffix;
    CunningStrike kind;
    std::string_view name;
    int cost;
};

inline constexpr std::array<CunningOption, 7> cunning_strikes{
    {{"poison", CunningStrike::poison, "Poison", 1},
     {"trip", CunningStrike::trip, "Trip", 1},
     {"withdraw", CunningStrike::withdraw, "Withdraw", 1},
     {"stealth", CunningStrike::stealth, "Stealth Attack", 1},
     {"daze", CunningStrike::daze, "Daze", 2},
     {"knock_out", CunningStrike::knock_out, "Knock Out", 6},
     {"obscure", CunningStrike::obscure, "Obscure", 3}}};

[[nodiscard]] const CunningOption &cunning_option(CunningStrike kind);

// Loading: a weapon fired in this action shoots again only with another
// action, such as an Action Surge's or Haste's.
[[nodiscard]] bool reloaded(const Actor &a);

// An attack the Attack action makes, which Extra Attack repeats; Reckless
// Attack and the pact weapon's damage types arrive here as "melee".
[[nodiscard]] bool attack_action(std::string_view verb);

[[nodiscard]] bool turns_to_attack(std::string_view verb);

// Named rather than a bool so a melee/ranged argument cannot be confused with
// the other flags beside it at a call site.
enum class AttackRange
{
    melee,
    ranged
};

// Whether an attack roll is a weapon's or a spell's; several modifiers apply to only one.
enum class AttackSource
{
    weapon,
    spell
};

// What a spell attack deals; a weapon attack takes its damage from the weapon.
struct SpellAttack
{
    Dice dice;
    detail::DamageType type{};
    int bursts{};             // how many extra d8s an 8 may add (Sorcerous Burst)
    std::string_view spell{}; // its attack bonus follows Magic Initiate's ability
};

// What attack() settled before apply_hit reports and applies it. Four ints and
// three flags in a row were easy to pass out of order, so callers name each one.
// " (2d6: 3 + 5, 1d8: 4, + 3 = 15)" for the dice behind a damage total: each run
// of one die size, then what was added and the total; with no total, the dice
// alone. Empty with no dice.
[[nodiscard]] std::string dice_detail(const std::vector<RolledDie> &dice,
                                      std::optional<int> total = {});

struct ResolvedAttack
{
    int natural{}; // the d20 as rolled
    int bonus{};   // everything added to the d20
    int mode{};    // below zero with Disadvantage, above zero with Advantage
    int damage{};  // after every reduction; zero on a miss
    detail::DamageType type{};
    AttackRange range{AttackRange::melee};
    bool spell{};
    bool savage{};    // Savage Attacker rerolled the damage
    int parry{};      // the AC a Parry added against this attack
    std::string dice; // the damage dice for the log, as dice_detail writes them
};

// Character definitions by profile and equipment, each parsed once. A pure
// function of its inputs, so copies of a session share one cache; a session
// is used from one thread at a time (Effective Modern C++ Item 16).
class ProfileDefinitions
{
  public:
    // `keys` replaces the profile's own equipment when given.
    Definition of(std::string_view profile,
                  std::optional<std::span<const std::string>> keys = std::nullopt)
    {
        std::string key(profile);
        key += keys ? '\x1e' : '\x1f';
        if (keys)
        {
            for (const auto &item : *keys)
            {
                (key += item) += '\x1f';
            }
        }
        if (const auto found = parsed_.find(key); found != parsed_.end())
        {
            return found->second;
        }
        // A long fight with many changes of gear keeps the cache bounded.
        if (parsed_.size() >= 512)
        {
            parsed_.clear();
        }
        return parsed_.emplace(std::move(key), character_definition(profile, keys)).first->second;
    }

  private:
    std::unordered_map<std::string, Definition> parsed_;
};

// What an attack is made with when it is not the attacker's own loadout: an
// Unarmed Strike, a thrown, off-hand or Torch strike, True Strike.
struct Loadout
{
    Definition definition;
    unsigned selected_weapon{};
    bool light_damage{};
};
} // namespace opengold::srd5::detail
#endif
