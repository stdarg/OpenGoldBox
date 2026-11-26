#ifndef OPENGOLD_SRD5_DEFINITION_H
#define OPENGOLD_SRD5_DEFINITION_H
#include "character_class.h"
#include "damage.h"
#include "damage_roll.h"
#include "weapons.h"
#include "opengold/rules.h"
#include <array>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

// A creature's Definition, its statistics as the content and the profile give
// them, with the spell slots it is built from and the tags of the formats
// that keep it; and the Content a rules module loads.
namespace opengold::srd5::detail
{
using rules::Footprint;
using rules::Identity;

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
} // namespace opengold::srd5::detail
#endif
