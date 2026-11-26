#ifndef OPENGOLD_ORIGINAL_COMBAT_SESSION_IMPL_H
#define OPENGOLD_ORIGINAL_COMBAT_SESSION_IMPL_H

#include "combat_session.h"
#include "checkpoint_text.h"
#include "champions_rules.h"
#include "dark_queen_effects.h"
#include "dark_queen_progression.h"
#include "dark_queen_rules.h"
#include "dark_queen_spell_table.h"
#include "death_knights_effects.h"
#include "death_knights_rules.h"
#include "line_walk.h"
#include "outlook_dice.h"

#include "opengold/buck_rogers_combat.h"
#include "opengold/buck_rogers_ship_combat.h"
#include "opengold/buck_rogers_upkeep.h"
#include "opengold/dice.h"
#include "opengold/movement_grid.h"
#include "opengold/original.h"
#include "opengold/original_rules.h"
#include "opengold/original_spells.h"

#include <algorithm>
#include <charconv>
#include <iterator>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <span>
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

// The Session of a fight under the original rules (combat_session.cpp), shared
// by the translation units that define its members: combat_session.cpp holds
// the CombatSession interface, rounds, movement, attacks, spells and gear;
// ground_combat.cpp Buck Rogers' ground combat; area_forecast.cpp the party's
// area-spell forecast; combat_checkpoint.cpp the checkpoint; game_specials.cpp
// the games' own specials; monster_ai.cpp the computer's play.

namespace opengold::original::detail
{

namespace dkk_effect = ::opengold::original::death_knights_effect;
namespace dq_effect = ::opengold::original::dark_queen_effect;

using original::ClassSlot;
using rules::Cell;
using rules::EntityId;
using rules::Message;
using rules::Side;

// The original counts movement in half-moves: 2 a straight step, 3 a diagonal one.
constexpr rules::StepCosts half_moves{2, 3, 2, 4, 0};
// Format 31 adds Native's underwater state to the movement-cost layout.
// All earlier 27/28/29/30 layouts remain readable.
constexpr std::string_view checkpoint_format = "OGORIGINALCOMBAT 30";
constexpr std::string_view dark_queen_checkpoint_format = "OGORIGINALCOMBAT 31";
constexpr std::string_view costless_checkpoint_format = "OGORIGINALCOMBAT 27";
constexpr std::string_view costless_death_knights_checkpoint_format = "OGORIGINALCOMBAT 28";
constexpr std::string_view costless_dark_queen_checkpoint_format = "OGORIGINALCOMBAT 29";
// The rounds a fight lasts with no one attacking (has_round_limit).
constexpr unsigned rounds_without_attacks = 15;
// Champions' removed creatures wait for a gating power, never a timed arrival.
// Reuse the existing off-field arrival state and its saved representation.
constexpr int gated_reserve_round = std::numeric_limits<int>::max();

enum class Condition : unsigned
{
    okay,
    unconscious,
    dying,
    dead,
    // Off the field: it got away with its Hit Points, or gave up with none.
    fled,
    surrendered,
    // Dead forever, past raising: drained below level 2.
    gone,
    // Turned to stone by a gaze.
    stoned
};

// The record's NPC byte from 0x80 up marks a creature whose morale counts;
// below it are the player's characters, who never check (combatai.pas, CheckMorale).
constexpr unsigned non_player = 0x80;
// A party member the computer plays for a while (the original's
// ANIMATED_PLAYER); 0 again once that ends.
constexpr unsigned animated_player = 0xB3;

// The original's race number of an elf (its records' races run from 1, a
// dwarf, to 7, a human; 0 is a monster).
constexpr int elf_race = 2;

// The effects that leave a creature helpless: nausea, snake charm, paralysis
// and sleep (charhlp.pas, IsHelpless).
constexpr std::array<int, 4> helpless_effects{31, 51, 52, 53};

// The effects a creature loses as it goes down or leaves the field, the first
// of each code (fxengine.pas, RemoveEffectsHp0).
constexpr std::array<int, 16> effects_ending_at_zero{7,  11, 30, 31, 32, 51,  52, 53,
                                                     54, 58, 59, 95, 98, 137, 74, 75};

// Effect codes the fight itself uses.
namespace effect
{
// Curse's holds: a shambling mound's engulfing (339) and its victim's
// suffocating (213), and an owlbear's hug (344).
constexpr int engulfing = 339, suffocating = 213, hugging = 344;
constexpr int bite_hold = 74, blood_drain = 75, rot = 50, poisoned = 55, mummy_rot = 57,
              paralysis = 52, immobile = 58, see_invisible = 24, invisible = 25, blink = 37,
              regenerating_soon = 59, camouflage = 72, mummy_fear = 82, displacement = 89,
              fighting_on = 95, regeneration = 98, fights_while_down = 99, troll_burn = 100,
              troll_heal = 101, troll_return = 102, gaze_block = 126, bless = 1, curse = 2,
              protection_from_evil = 8, protection_from_good = 9, resist_cold = 10, charm = 11,
              shield = 17, resist_fire = 20, sleep = 53, silence = 21, mirror_image = 28,
              // The fight's bar on spells, which every fighter of a "no magic"
              // fight holds (rules::Encounter::no_magic): no game numbers it.
    no_magic = kit_effect_offset, enfeebled = 29, blind = 33, bestow_curse = 36, hasted = 39,
              missile_protection = 41, slowed = 42, protection_from_evil_aura = 45,
              protection_from_good_aura = 46, prayer = 49, coughing = 30, nausea = 31,
              stinking_cloud = 40, cloudkill = 291, enlarged = 12, strengthened = 38,
              strength_wasted = original::strength_wasted, delayed_blast = 500, animated = 32,
              snake_charm = 51, slow_poison_tick = 15, slow_poison = 22, revival = 78,
              read_magic = 16, dust = 71, age_in_years = original::age_in_years,
              spiritual_hammer = 23;
// Haste keeps whether it has aged its creature in this bit of its strength,
// which no caster's level reaches. The original's bit 0x10 is set already by
// a caster of level 16 to 31, whose Haste so never aged (Bugs).
constexpr int hasted_aged = 0x40;
// Displacement keeps whether it has turned an attack aside in this bit of its strength.
constexpr int displaced = 0x10;
// A ring of invisibility's renewal (Pool of Radiance's 56, Secret's too), a
// gaze a mirror turns back (Pool of Radiance's 127) and Curse's fear (342).
constexpr int renew_invisibility = 56, reflectable_gaze = 127, fear = 342;
// Secret of the Silver Blades' own (canonical_effect, its code plus
// secret_effect_offset).
constexpr int globe_of_invulnerability = 457, petrifying_gaze = 458, needs_plus_three = 460,
              poison_touch = 464, stone_touch = 465, swallows_whole = 466, fire_beside = 467,
              confusing_gaze = 470, boots_of_speed = 474, giant_bane = 475, luck = 478,
              iron_golem = 479, fire_at_target = 480, spits_acid = 481, dragon_fear = 482,
              cold_breath = 483, strong_poison_touch = 486, ghoul_touch = 488, bashing_proof = 490,
              fearless = 492, hardy = 494, fire_breath = 504, stunned = 506;
// Unlimited Adventures' own (canonical_effect, its code plus kit_effect_offset).
// Pools of Darkness plays four of them by the same rules: the Rakshasa's
// protection is the kit's spell_proof, its weapons below +3 and +4 halving,
// its need of a +2 weapon and its immunity to gazes the kit's.
constexpr int spell_proof = 640, disease_proof = 676, halved_below_plus_three = 680,
              halved_below_plus_four = 681, poison_breath = 690, bulette_attacks = 699,
              needs_plus_two = 725, gaze_proof = 726, stench = 737, sickened = 748,
              hurt_more_by_magical_fire = 768, eats_armor = 786, blow_proof = 793,
              gorgon_breath = 795, fire_less_by_the_die = 819, eye_rays = 717, rot_touch = 770,
              kit_rotting = original::kit_rotting;
// The kit's items' own, while readied (docs/games/ua.md, Items' special
// effects): a ring of fire resistance, boots of speed, a giant-slaying sword
// and a vorpal blade.
constexpr int fire_resistance_ring = 661, speed_boots = 674, kit_giant_bane = 675, vorpal = 715;
// The kit's monsters' own: the need of a +3 and a +1 weapon, paralysis by a
// second blow beside its target and by a ghoul's first, and 2d6 of magical
// fire with a second blow.
constexpr int needs_plus_three_kit = 660, needs_plus_one = 703, adjacent_paralysis = 688,
              ghoul_paralysis = 824, fire_blow = 708;
// OpenGoldBox's own mark on a creature whose armor a black pudding has eaten
// into, for the campaign to keep: past the kit's last effect (228), so no
// record of the kit's names it.
constexpr int armor_eaten = kit_effect_offset + 250;
// Pools of Darkness's own (its 116 and 118 to 124, plus
// secret_effect_offset): Kalistes' six weapons, a dance, the druid's
// Protection from Fire, dragon fear's terror, Fire Touch, Iron Skin and Mind
// Blank; and the caster's Blade Barrier, which no game's effect numbers (604,
// past the conditions original_combat.h numbers 601 to 603, so wasted
// Strength is not taken for a barrier). Its others no earlier game has (its
// 67, 82, 83, 91, 96, 100 and 104, plus darkness_effect_offset): half harm
// from crushing weapons, death by a blessed bolt, the big red, blue and black
// dragons' breaths, dragon fear and 4d6 of fire with a hit.
constexpr int retaliating_shield = 343;
constexpr int six_weapons = 516, dancing = 518, druid_fire_protection = 519, dragon_feared = 520,
              fire_touch = 521, iron_skin = 522, mind_blank = 523, blade_barrier = 604,
              halves_crushing = 537, slain_by_blessed_bolt = 552, big_red_breath = 553,
              blue_breath = 561, black_breath = 566, darkness_dragon_fear = 570, fire_hit = 574;
// Gateway to the Savage Frontier's own (its code plus gateway_effect_offset;
// effhndlr.pas in its reconstruction): a paralyzing gaze (126), harm from
// fire (148), a stirge's drinking hold (149, holding with 150), more harm
// from weapons of Icewind Dale (151) and of stonecutting (153), half harm
// against glacier armor (152), a charming touch (154), a constricting hold
// (155, holding with 156), surrender when last (158), a diseasing touch
// (159) and regeneration of 1 a round (161). Treasures' Harpy, glacier,
// Icewind Dale and stonecutting effects are these (canonical_effect).
constexpr int gateway_paralyzing_gaze = 1126, fire_harms_more = 1148, drinks_blood = 1149,
              drinking_blood = 1150, icewind_dale_harms = 1151, glacier_halves = 1152,
              stonecutting_harms = 1153, charming_touch = 1154, constricts = 1155,
              constricting = 1156, surrenders_when_last = 1158, disease_touch = 1159,
              regenerates_one = 1161;
// Treasures of the Savage Frontier's own (its code plus vga_effect_offset;
// effhndlr.pas in its reconstruction): half harm from the elements (133),
// death by Stone to Flesh (134), a slug's acid (137), a hell hound's and an
// ice hound's breath (138, 154), a quarter of an edge's harm (141), no
// effect taken (142), a manticore's spines (143), a kampfult's crushing
// (144), a rat's disease (145), a quick start (146), a stone golem (147), a
// gorgimera's stone breath (148), the yetis' grabs (149 to 151) and their
// fear when they surprise (152), more harm from fire (153) and swallowing
// whole (155).
// Its crystals, which a creature gives up as it falls in a crystal battle
// (128 to 130: EffRedCrystal, EffGreenCrystal, EffBlueCrystal).
constexpr int red_crystal = 1328;
// Its romance's love (131, EffInLove), on the pair a script names: strength 1
// on a member whose proposal the party refused, 2 on a lover in a frenzy.
constexpr int in_love = 1331, refused_love = 1, frenzied_love = 2;
constexpr int halves_elements = 1333, stone_to_flesh_kills = 1334, slug_acid = 1337,
              hell_hound_breath = 1338, quarter_from_edges = 1341, effects_proof = 1342,
              manticore_spines = 1343, crushes = 1344, rat_disease = 1345, quick_start = 1346,
              stone_golem = 1347, stone_breath = 1348, grabs_2d8 = 1349, grabs_4d5 = 1350,
              grabs_6d4 = 1351, frightens_on_surprise = 1352, fire_harms_half_more = 1353,
              ice_hound_breath = 1354, swallows_on_twenty = 1355;
// A stone golem's effect's strength while it is stone (EffSlowAura); flesh,
// it counts down to it.
constexpr int golem_stone = 255;
// OpenGoldBox's own mark on a Treasures creature crushing or squeezing its
// victim (144, 149 to 151), whose combat number its strength keeps: past
// Treasures' last effect (160), so no record of its names it. Its victim is
// held still (Curse's 136, Treasures' 156).
constexpr int squeezing = vga_effect_offset + 201, held_still = 336;
// Pools of Darkness's paralyzing gaze (its 114, plus darkness_effect_offset)
// and charming gaze (its 124, plus secret_effect_offset), which its original
// registers but no fight runs.
constexpr int darkness_paralyzing_gaze = 584, darkness_charming_gaze = 524;
constexpr int aurak_energy = champions_effect_offset + 4, maya_breath = champions_effect_offset + 6,
              death_knight_dispel = champions_effect_offset + 74,
              death_knight_fireball = champions_effect_offset + 78,
              death_knight_gate = champions_effect_offset + 76,
              dragon_slayer = champions_effect_offset + 75,
              dragonlance = champions_effect_offset + 114,
              reptile_bane = champions_effect_offset + 115,
              mace_disruption = champions_effect_offset + 117,
              aurak_madness = champions_effect_offset + 13,
              aurak_transformation = champions_effect_offset + 32,
              aurak_immolation = champions_effect_offset + 48,
              aurak_combat_bonus = champions_effect_offset + 57,
              aurak_transformation_timer = champions_effect_offset + 58,
              aurak_invulnerable = champions_effect_offset + 59,
              aurak_explosion = champions_effect_offset + 60,
              aurak_zap = champions_effect_offset + 66,
              bozak_explosion = champions_effect_offset + 68,
              baaz_disarm = champions_effect_offset + 67,
              kapak_paralysis = champions_effect_offset + 69,
              kapak_acid = champions_effect_offset + 70,
              death_knight_reflects = champions_effect_offset + 71,
              death_knight_fear = champions_effect_offset + 72,
              ghast_stench = champions_effect_offset + 79,
              acid_pool_timer = champions_effect_offset + 80,
              paralyzing_strike = champions_effect_offset + 81,
              champions_dragon_fear = champions_effect_offset + 82,
              slight_poison = champions_effect_offset + 87,
              champions_ghoul_touch = champions_effect_offset + 88,
              champions_sickened = champions_effect_offset + 118,
              champions_afraid = champions_effect_offset + 119,
              kender_taunted = champions_effect_offset + 91,
              kender_yelled = champions_effect_offset + 253,
              baaz_banked_weapon = champions_effect_offset + 254;
} // namespace effect

// The effects an outlook counts as leaving a creature unable to fight for
// its side (Session::Forecast): the helpless ones (nausea, snake charm,
// paralysis, sleep), a charm, confusion, fear, being held still or slowed,
// Secret's stun and Pools of Darkness's dance.
constexpr std::array<int, 11> disabling_effects{
    effect::nausea, effect::snake_charm, effect::paralysis, effect::sleep,   effect::charm,  235,
    effect::fear,   effect::held_still,  effect::slowed,    effect::stunned, effect::dancing};
// The spells' effects that help the creature they are on, and that hinder
// it, which Dispel Magic may take (Session::Forecast).
constexpr std::array<int, 37> helping_effects{
    effect::bless, effect::protection_from_evil, effect::protection_from_good, effect::resist_cold,
    effect::enlarged, effect::shield, effect::resist_fire, effect::spiritual_hammer,
    effect::invisible, effect::mirror_image, effect::animated, effect::blink, effect::strengthened,
    effect::hasted, effect::stinking_cloud, effect::missile_protection,
    effect::protection_from_evil_aura, effect::protection_from_good_aura, effect::prayer,
    effect::dust, effect::displacement,
    // Curse's: Dispel Evil, the fire shields, Minor Globe of
    // Invulnerability, the cloudkill a caster keeps.
    204, 345, 250, 254, 335, 263, effect::cloudkill, effect::retaliating_shield,
    // Secret's and Pools of Darkness's.
    effect::globe_of_invulnerability, effect::delayed_blast, effect::druid_fire_protection,
    effect::fire_touch, effect::iron_skin, effect::mind_blank, effect::blade_barrier,
    effect::see_invisible};
constexpr std::array<int, 10> hindering_effects{effect::curse, effect::silence, effect::enfeebled,
                                                effect::coughing, effect::blind,
                                                // Cause Disease, Bestow Curse, Sticks to Snakes,
                                                // Faerie Fire and Feeblemind.
                                                34, effect::bestow_curse, 203, 207, 268};

// What a hit carries, by the attacker's effects, in the original's order: a
// primary attack's hit and a secondary one's (effhndlr.pas, HIT_PHASE_1 and
// HIT_PHASE_2).
constexpr std::array<int, 46> primary_hit_effects{
    dq_effect::spore_touch, dq_effect::poison_bite, dq_effect::constricting_touch,
    dq_effect::mummy_touch, dq_effect::enchanted_paralysis, dq_effect::venom_touch,
    dkk_effect::stunning_hit, dkk_effect::fire_touch, 85, 86, 87, 68, 79, 80, 76, 73, 77,
    // Curse's (canonical_effect).
    257, 296, 322, 323, 345,
    // Secret's (fxengine.pas, its conditions 2 and 3, in its reconstruction).
    effect::poison_touch, effect::stone_touch, effect::strong_poison_touch, effect::ghoul_touch,
    // The kit's vorpal blade, ghoul and mummy (its fxcore.c, fx_trigger_group 2).
    effect::vorpal, effect::ghoul_paralysis, effect::rot_touch,
    // Gateway's: its poison and paralysis ride the first attack too (its
    // effhndlr.pas, condition 2), and its own holds, charm and disease.
    64, 67, effect::drinks_blood, effect::charming_touch, effect::constricts, effect::disease_touch,
    // Treasures' (its condition 2).
    effect::crushes, effect::rat_disease, effect::grabs_2d8, effect::grabs_4d5, effect::grabs_6d4,
    effect::swallows_on_twenty, effect::kapak_paralysis, effect::paralyzing_strike,
    effect::slight_poison, effect::champions_ghoul_touch};
constexpr std::array<int, 33> secondary_hit_effects{
    dq_effect::spore_touch, dq_effect::poison_bite, dq_effect::constricting_touch,
    dq_effect::mummy_touch, dq_effect::enchanted_paralysis, dq_effect::venom_touch,
    dkk_effect::stunning_hit, dkk_effect::fire_touch, 64, 65, 66, 67, 68, 69, 70, 79, 85, 86, 87,
    // Secret's.
    effect::poison_touch, effect::stone_touch, effect::strong_poison_touch, effect::ghoul_touch,
    // The kit's (fx_trigger_group 3).
    effect::adjacent_paralysis, effect::fire_blow, effect::rot_touch,
    // Treasures' (its condition 3).
    effect::grabs_2d8, effect::grabs_4d5, effect::grabs_6d4, effect::swallows_on_twenty,
    effect::kapak_paralysis, effect::paralyzing_strike, effect::slight_poison};

// The first element of a range that `matches`, none when there is none: one
// body for a lookup's const and non-const forms, deducing the constness of
// the range instead of casting it away (ES.50).
template <class Range, class Matches> auto *first_matching(Range &range, const Matches &matches)
{
    const auto found = std::find_if(range.begin(), range.end(), matches);
    return found == range.end() ? nullptr : &*found;
}

// An item a creature carries, which it may ready or put away in a fight
// (viewchar.pas, ToggleReadyItem): its record, readied or not, with what is
// left of it, and its type; its inventory entry (0 for a monster's own) and
// name; its count as the fight began (held_count); and whether it is spent.
struct Held
{
    CarriedItem carried;
    std::uint64_t inventory_id{};
    std::string name;
    int start_count{};
    bool gone{};
    // Its place among the creature's readied items, which keeps their order.
    int readied_order{-1};
    // Spiritual Hammer's hammer, which is no item of the party's and goes
    // with its spell (effhndlr.pas, EffSpiritualHammer).
    bool conjured{};
};

// A Buck Rogers creature in the fight (buck_rogers_combat.h): its record,
// items and ITEMS rows as the fight changes them, the effects on it, how
// long it has bled, its initiative's tie breaker, what the difficulty adds
// to its rolls and its Armor Class, whether it flies on a jetpack, to land
// where it chooses, and whether the computer plays it: a companion no
// Leadership has taken over (cbtacts.pas UseLeadership).
struct GroundFighter
{
    GroundRecord gear;
    std::vector<GroundEffect> effects;
    int dying_count{}, tie_break{}, to_hit{}, armor_class_bonus{};
    // It has attacked this round: readying a weapon can then only lower its
    // attacks (cbtacts.pas RecalcAttacksLeft).
    bool flying{}, computer_plays{}, attacked{};

    [[nodiscard]] bool has(int code) const noexcept
    {
        return has_ground_effect(effects, code);
    }

    [[nodiscard]] buck_rogers::Creature &creature() noexcept
    {
        return gear.creature;
    }

    [[nodiscard]] const buck_rogers::Creature &creature() const noexcept
    {
        return gear.creature;
    }
};

struct Fighter
{
    EntityId id{};
    std::string name, definition;
    // What its numbers are worked out from again as it readies or puts away
    // an item (charhlp.pas, RecalcStats): its record and the items it carries,
    // and the items its numbers were last worked out from, in their order
    // (UsableItem::ordinal), by their places among the held. A creature with
    // no record readies nothing.
    std::optional<por::CharacterRecord> record;
    std::vector<Held> held;
    std::vector<int> worked_from;
    // Its numbers count the items it has not readied too, as a monster's
    // record loads them.
    bool weighs_unreadied{};
    // The fight changed which items it has readied.
    bool readied_changed{};
    // A party member's items as its inventory knows them: each usable item's
    // inventory ID by its place (UsableItem::ordinal), and the item its
    // missile shoots, whose counts left the snapshot reports.
    std::vector<std::pair<int, std::uint64_t>> item_ids;
    std::uint64_t shot_item{};
    Side side{Side::party};
    Cell cell;
    rules::Footprint footprint;
    bool facing_left{};
    Combatant numbers;
    int hit_points{};
    Condition condition{Condition::okay};
    // How far below 0 a dying creature's Hit Points are; past 9 it is dead.
    int overkill{};
    // 0 means it does not act (again) this round.
    int initiative{};
    int half_moves{};
    std::array<int, 2> attacks_left{};
    // The attack form it strikes with next: the second (1) before the first (0).
    int attack_phase{1};
    // How many level-0 enemies it may still sweep through: its attack level,
    // until its first attack of the round.
    int sweep_budget{};
    bool surprised{};
    // Facing, 0 north and clockwise to 7 north-west.
    int direction{};
    // The way it starts the fight facing (rules::Participant::facing), which
    // the session turns it to as the fight begins.
    std::optional<int> starting_facing;
    // Attacks on it since its own turn began, and the sum of their angles from
    // its facing (0 to 4), mod 8 (cbtacts.pas, NoteAttackDirection).
    int flank_count{}, flank_spread{};
    bool guarding{};
    // Its morale broke at its turn's start: it runs for the edge.
    bool fleeing{};
    // Forced to flee for the rest of the fight: a stirge that has drunk its
    // fill, or undead a cleric turned.
    bool turned{};
    // It has tried to turn undead in this fight: once a fight; in Treasures
    // how many times it has, as many as the undead the fight began with, and
    // in Pools of Darkness as many as the fight loaded undead
    // (Session::turning_tries_).
    bool tried_turning{};
    int turn_attempts{};
    // Casting: whether it may still start a spell this round (damage and
    // silence stop it), the spell it has begun that goes off at its next
    // turn, and the spell it is aiming now with the creatures picked so far
    // and the picks it has left.
    bool cast_allowed{true};
    int pending_spell{}, aiming{}, picks_left{};
    // The rounds a Delayed Blast Fireball it is aiming waits before it goes
    // off, -1 before the caster has chosen (Secret's SpellDelayedBlastFireball).
    int blast_delay{-1};
    std::vector<EntityId> picked;
    // Its side before a charm took it over.
    Side own_side{Side::party};
    // The round it joins the fight as it ends, kept off the field (fled)
    // until then; 0 once it is there (rules::Participant::arrival_round).
    int arrival_round{};
    // Whether it may still use an item this round (silence and coughing stop
    // it), and the item it is using now, by its ordinal, -1 for none.
    bool item_use_allowed{true};
    int using_item{-1};
    // The original's AI: the approach pattern it walks by (1 to 6, 0 before
    // its first turn) and the enemy it has locked onto (combatai.pas).
    int stance{};
    EntityId lock{};
    // It has attacked this turn.
    bool fought{};
    // A creature the computer plays whose turn left it no way to fight or get
    // away walks the shortest way to an enemy (route_to_enemy) until it
    // fights again.
    bool routing{};
    // What it was before Animate Dead raised it, which it is again as it collapses.
    std::optional<Combatant> before_animation;
    // A Buck Rogers creature's own rules' state; none in the other games.
    std::optional<GroundFighter> ground;

    [[nodiscard]] bool able() const noexcept
    {
        return condition == Condition::okay;
    }

    // Down but not dead: out of the fight, where it may still be stood on.
    [[nodiscard]] bool downed() const noexcept
    {
        return condition == Condition::unconscious || condition == Condition::dying;
    }

    [[nodiscard]] int attacks() const noexcept
    {
        return attacks_left[0] + attacks_left[1];
    }

    [[nodiscard]] bool has(int code) const noexcept
    {
        return std::any_of(numbers.effects.begin(), numbers.effects.end(),
                           [code](const Effect &e)
                           {
                               return e.code == code;
                           });
    }

    [[nodiscard]] bool helpless() const noexcept
    {
        if (ground)
        {
            return ground->has(ground_effect::helpless);
        }
        return std::any_of(helpless_effects.begin(), helpless_effects.end(),
                           [this](int code)
                           {
                               return has(code);
                           });
    }

    // Removes the first effect of the code, if any.
    void remove(int code)
    {
        auto &effects = numbers.effects;
        const auto found = std::find_if(effects.begin(), effects.end(),
                                        [code](const Effect &e)
                                        {
                                            return e.code == code;
                                        });
        if (found != effects.end())
        {
            effects.erase(found);
        }
    }

    [[nodiscard]] static auto has_code(int code)
    {
        return [code](const Effect &e)
        {
            return e.code == code;
        };
    }

    [[nodiscard]] const Effect *find_effect(int code) const
    {
        return first_matching(numbers.effects, has_code(code));
    }

    [[nodiscard]] Effect *find_effect(int code)
    {
        return first_matching(numbers.effects, has_code(code));
    }
};

// The fight's helpers, defined in combat_session.cpp.
bool adjacent(const Fighter &a, const Fighter &b);
std::vector<Cell> squares_of(const Fighter &f);
int distance(const Fighter &a, const Fighter &b);
bool sees(const rules::Battlefield &board, const Fighter &a, const Fighter &b);
int attack_modifier(const Fighter &attacker, const Fighter &defender, GoldBoxGame game);
bool dispel_evil_takes(const Fighter &f, GoldBoxGame game);
bool giant_bane_strikes(const Fighter &attacker, const Fighter &defender, GoldBoxGame game);
int paced(const Fighter &f, int rate);
int caster_level(const Fighter &caster, const Spell &spell, GoldBoxGame game);
int spell_range(const Spell &spell, int level);
int doubled_distance(Cell a, Cell b);
bool underwater_spell(int number);
bool spell_built(int number);
bool magic_resists(const Fighter &target, int caster_level, int roll_100);
bool resists_magic_at_all(const Fighter &target);
bool absorbs_harm(const Fighter &target, int element);
// A Buck Rogers creature as the fight takes it, defined in ground_combat.cpp.
GroundFighter ground_fighter(const rules::Participant &p, const std::string &name);
Combatant ground_combatant(const GroundFighter &fighter, const GroundNumbers &numbers);

// Curse's Fire Shield, whose caster chooses its flames as it goes off, and
// the choice: the AI's (and Quick's) at random (SpellFireShield).
constexpr int fire_shield = 185, delayed_blast_spell = 315;
enum class Flames : unsigned
{
    random,
    hot,
    cold
};

// A cloud on the field: the caster's, its number among the caster's clouds of
// its kind, and the squares it covers where the ground can be walked. A
// stinking cloud covers its corner and the squares east, south-east and south
// of it (magic.pas, SpellStinkingCloud); Curse's Cloudkill the square aimed
// at, kept as its corner, and the eight around (SpellCloudkill).
enum class CloudKind
{
    stinking,
    deadly,
    // Pools of Darkness's Blade Barrier, on a stinking cloud's four squares
    // (its SpellStinkingCloud).
    blades,
    // A kapak leaves acid on its single square when it dissolves.
    acid
};

struct Cloud
{
    EntityId caster{};
    int index{};
    Cell corner;
    std::array<bool, 9> covers{};
    CloudKind kind{CloudKind::stinking};

    [[nodiscard]] std::vector<Cell> squares() const
    {
        if (kind == CloudKind::acid)
        {
            return {corner};
        }
        if (kind != CloudKind::deadly)
        {
            return {corner, Cell{corner.x + 1, corner.y}, Cell{corner.x + 1, corner.y + 1},
                    Cell{corner.x, corner.y + 1}};
        }
        std::vector<Cell> around{corner};
        for (int dy = -1; dy <= 1; ++dy)
        {
            for (int dx = -1; dx <= 1; ++dx)
            {
                if (dx != 0 || dy != 0)
                {
                    around.push_back({corner.x + dx, corner.y + dy});
                }
            }
        }
        return around;
    }
};

// A spell's damage dice: so many dice of so many sides, plus a bonus.
struct SpellDice
{
    int count{}, sides{}, bonus{};
};

// Meteor Swarm's meteor, which strikes each creature for its own 10d4
// (cast_meteor_swarm), and Blade Barrier's cut of 8d8 (cut_by_blades).
constexpr SpellDice meteor_dice{10, 4, 0};
constexpr SpellDice blade_dice{8, 8, 0};
// Where Meteor Swarm's four meteors fall, from its aim (meteor_fall).
constexpr std::array<Cell, 4> meteor_landings{{{-1, -1}, {1, -1}, {-1, 1}, {1, 1}}};

// A creature's most Hit Points as the rules know them, for a rule that may
// weigh them as a caster can know them instead (Session::Forecast).
constexpr auto true_most_hit_points = [](const Fighter &f)
{
    return f.numbers.max_hit_points;
};

class Session final : public rules::CombatSession
{
  public:
    Session(rules::Identity identity, rules::Battlefield board, std::vector<Fighter> fighters,
            std::uint64_t seed, unsigned morale, bool outdoors, bool escape_ends_fight,
            int party_facing, int party_move_modifier, bool crystals, bool frightening_surprise,
            int turning_tries, int turning_penalty, std::uint64_t time_minutes, bool zero_gravity,
            bool underwater)
        : identity_(std::move(identity)), board_(std::move(board)), fighters_(std::move(fighters)),
          random_{seed}, morale_threshold_(std::min(morale, 100U)), outdoors_(outdoors),
          underwater_(underwater), escape_ends_fight_(escape_ends_fight),
          party_facing_(party_facing), party_move_modifier_(party_move_modifier),
          crystals_(crystals), frightening_surprise_(frightening_surprise),
          turning_tries_(turning_tries), turning_penalty_(turning_penalty),
          starting_time_minutes_(time_minutes), zero_gravity_(zero_gravity)
    {
        rules::validate_battlefield(board_);
        // Each creature starts turned along the party's facing, the party's
        // enemies back toward it (combatst.pas, InitCreatureCombatData), or
        // toward the party from the side it was dealt to (PlaceAllCreatures).
        for (auto &f : fighters_)
        {
            if (underwater_ && f.side == Side::party)
            {
                f.numbers.effects.push_back({dq_effect::underwater_weapons, 0, 0, false});
            }

            f.direction = f.starting_facing
                              ? field_direction(*f.starting_facing)
                              : (along_party_facing() + (f.side == Side::party ? 0 : 4)) % 8;
        }
        log({"Combat begins.", {}});
        native_leadership();
        combat_start();
        update_hostile_hit_points();
        // A fight ends only as a round ends, unless a side has no one able to
        // fight from the start (combat.pas, CombatMain and EndOfRound).
        settle_outcome();
        if (outcome_ == rules::Outcome::ongoing)
        {
            begin_round();
            next_actor();
        }
    }

    static std::unique_ptr<Session> restored(rules::Identity identity, std::string_view checkpoint,
                                             const Content &content);

    // The game's item types, which a hammer the fight conjures is made of.
    void know_item_types(std::vector<por::ItemTemplate> types)
    {
        item_types_ = std::move(types);
    }

    // A Buck Rogers game's item words, which name what the fight offers to ready.
    void know_item_words(std::vector<std::string> words)
    {
        item_words_ = std::move(words);
    }

    [[nodiscard]] rules::Snapshot snapshot() const override;
    [[nodiscard]] std::vector<rules::Command> legal_commands() const override;
    [[nodiscard]] std::vector<Cell> movement_reach(EntityId actor) const override;
    bool submit(const rules::Command &command) override;
    void turn_against_party(const std::vector<EntityId> &turning) override;
    [[nodiscard]] std::string save() const override;
    [[nodiscard]] std::optional<rules::AreaOutlook> area_outlook(EntityId caster, unsigned spell,
                                                                 Cell aim) const override;

  private:
    Session() = default;

    rules::Identity identity_;
    rules::Battlefield board_;
    std::vector<Fighter> fighters_;
    RandomState random_;
    unsigned round_{};
    std::uint64_t revision_{1};
    EntityId actor_{};
    rules::Outcome outcome_{rules::Outcome::ongoing};
    std::vector<rules::LogEntry> log_;
    // The encounter's morale, 0 to 100, which the opposition's whole strength
    // is weighed against (the script's MORALE_THRESHOLD).
    unsigned morale_threshold_{100};
    // Out of doors (rules::Encounter::outdoors).
    bool outdoors_{};
    bool underwater_{};
    // Gateway's maze fights: a member's escape ends them (rules::Encounter).
    bool escape_ends_fight_{};
    // Pools of Darkness's gazes are played (play_gazes).
    bool gazes_played_{};
    bool original_movement_{true};
    // The player's limits on a party member's area spells
    // (Content::party_spell_limits).
    rules::AreaSpellLimits party_spell_limits_;
    // The side the member who ended such a fight ran off.
    std::optional<unsigned> escape_side_;
    // The party's facing and the script's change to its movement
    // (rules::Encounter).
    int party_facing_{};
    int party_move_modifier_{};
    // A crystal battle (rules::Encounter::crystals), and the crystal given
    // up in it, after which the others are smashed (combat.pas, its
    // cbTailFlag2 and gameState's lastCrystal).
    bool crystals_{};
    std::optional<unsigned> crystal_;
    // Treasures' romance (pair_lovers): its loves as they move, and the party
    // member and the NPC in love, 0 for one not in the fight.
    std::optional<rules::Romance> romance_;
    EntityId lover_{}, beloved_{};
    // The creature whose weapon strikes now, which a foe it fells counts
    // against; 0 outside a weapon's attack, where the one acting does.
    EntityId striker_{};
    // The round as which the fight ends unless someone attacks before it
    // (Pool of Radiance's successors, has_round_limit), and whether it ended
    // so.
    unsigned fight_end_round_{rounds_without_attacks};
    bool round_limit_reached_{};
    // Treasures' surprise that frightens (rules::Encounter), played only as
    // the fight begins.
    bool frightening_surprise_{};
    // Pools of Darkness's tries at turning undead each member has in this
    // fight, and what its area takes off the turning column
    // (rules::Encounter::turning_tries, turning_penalty).
    int turning_tries_{};
    int turning_penalty_{};
    std::uint64_t starting_time_minutes_{};
    std::vector<por::ItemTemplate> item_types_;
    std::vector<std::string> item_words_;

    // The direction along the party's facing on the field: north gives
    // north-west and south south-east, as the original's table turns them
    // (combatst.pas, quadDir; combatai.pas, AiMoveStep).
    [[nodiscard]] int along_party_facing() const
    {
        return field_direction(party_facing_);
    }

    // The direction on the field along a map facing (0 north, 2 east, 4
    // south, 6 west).
    [[nodiscard]] static int field_direction(int facing)
    {
        static constexpr std::array<int, 4> turned{7, 2, 3, 6};
        return turned[static_cast<std::size_t>(facing / 2 % 4)];
    }

    [[nodiscard]] int squares_of_movement(const Fighter &f) const;
    [[nodiscard]] int half_moves_of(const Fighter &f) const;
    [[nodiscard]] int movement_of(const Fighter &f) const;
    std::vector<Cloud> clouds_;

    // Delayed Blast Fireballs set and not yet gone off: the caster's, its
    // number among the caster's, the square, and the caster's level.
    struct DelayedBlast
    {
        EntityId caster{};
        int index{};
        Cell cell;
        int level{};
    };

    std::vector<DelayedBlast> blasts_;
    // The opposition's Hit Points left of its whole, in steps of 5, as of the
    // round's start (cbtacts.pas, UpdateHostileHpPercent).
    int hostile_hit_points_percent_{100};
    // Secret of the Silver Blades and Pools of Darkness order the round once
    // as it begins: those yet to act this round, first first (combat.pas,
    // OrderCreatures, in their reconstructions). Empty in the earlier games,
    // which pick the highest initiative left each time.
    std::vector<EntityId> turn_order_;
    // The dice the damage of the spell going off was rolled with (the kit's
    // damage_dice_count), which the kit's effects weigh fire by; 0 for any
    // other damage, which has no dice to weigh.
    int damage_dice_{};
    // A Buck Rogers fight (buck_rogers_combat.h): in zero gravity
    // (rules::Encounter::zero_gravity), its clouds of chaff, mist and gas,
    // and the sides whose tactics roll made this round.
    bool zero_gravity_{};
    std::vector<GroundCloud> ground_clouds_;
    std::array<bool, 2> tactics_{};

  public:
    // Pools of Darkness's paralyzing and charming gazes (its 114 and 124)
    // work when `played`, as their handlers describe; otherwise they do
    // nothing, as in the original, whose fights never run them.
    void play_gazes(bool played)
    {
        gazes_played_ = played;
    }

    void move_as_the_original(bool on)
    {
        original_movement_ = on;
    }

    void limit_party_area_spells(const rules::AreaSpellLimits &limits)
    {
        party_spell_limits_ = limits;
    }

    // Treasures' romance as the fight starts (its romance.pas,
    // TagRelationshipPair): the creatures of the pair's names, the last of
    // each, love each other, and no one else does.
    void pair_lovers(std::optional<rules::Romance> romance);

  private:
    [[nodiscard]] bool curse() const
    {
        return game_of(identity_) == GoldBoxGame::curse_of_the_azure_bonds;
    }

    [[nodiscard]] bool secret() const
    {
        return game_of(identity_) == GoldBoxGame::secret_of_the_silver_blades;
    }

    [[nodiscard]] bool kit() const
    {
        return game_of(identity_) == GoldBoxGame::unlimited_adventures;
    }

    [[nodiscard]] bool darkness() const
    {
        return game_of(identity_) == GoldBoxGame::pools_of_darkness;
    }

    [[nodiscard]] bool champions() const
    {
        return krynn_game(game_of(identity_));
    }

    [[nodiscard]] bool treasures() const
    {
        return game_of(identity_) == GoldBoxGame::treasures_of_the_savage_frontier;
    }

    // Countdown to Doomsday and Matrix Cubed, whose creatures fight by their
    // own rules (GroundFighter).
    [[nodiscard]] bool buck_rogers() const
    {
        return is_buck_rogers(game_of(identity_));
    }

    // Whether the fight moves as the original does (Content::original_movement,
    // the game's option): rules::StepRules::original's steps, the computer
    // stepping only while it keeps some movement, and a last half-move lost
    // as the move menu closes.
    [[nodiscard]] bool original_movement() const
    {
        return original_movement_;
    }

    // The 256-color engine's computer (Pools of Darkness's and Treasures'
    // combatai.pas and cbtacts.pas, the same in both): it casts the spell of
    // highest AI weight that has a target and never uses an item.
    [[nodiscard]] bool weighs_spells() const
    {
        return darkness() || treasures();
    }

    // Dark Queen's computer (its aiturn.c and cbaction.c) weighs its spells
    // the same way, by its own ratings, and never uses an item either.
    [[nodiscard]] bool native_weighs_spells() const
    {
        return game_of(identity_) == GoldBoxGame::dark_queen_of_krynn;
    }

    [[nodiscard]] bool gateway() const
    {
        return game_of(identity_) == GoldBoxGame::gateway_to_the_savage_frontier;
    }

    // Whether the fight plays Secret's rules where Pools of Darkness's
    // reconstruction does as Secret's does (docs/ORIGINAL-RULES.md, Pools of
    // Darkness); Treasures', on the same engine, is not checked yet.
    [[nodiscard]] bool secret_engine() const
    {
        return secret() || darkness();
    }

    // Secret, Pools of Darkness and Unlimited Adventures order each round
    // once as it begins, ties broken by a d2 (Secret's and Darkness's
    // combat.pas, OrderCreatures; the kit's combat.c and cbaction.c).
    [[nodiscard]] bool ordered_rounds() const
    {
        return secret_engine() || kit() ||
               game_of(identity_) == GoldBoxGame::death_knights_of_krynn || buck_rogers();
    }

    // Every game after Pool of Radiance ends a fight in which no one has
    // attacked for 15 rounds (Curse's, Secret's, Gateway's, Pools of
    // Darkness's and Treasures' combatst.pas, CombatStart, and cbtacts.pas,
    // MakeAttack; the kit's cmbsetup.c and cbaction.c, combat_attack).
    [[nodiscard]] bool has_round_limit() const
    {
        return game_of(identity_) != GoldBoxGame::pool_of_radiance;
    }

    void win_fight();
    void end_quiet_fight();

    // A spell as this fight's game has it (Gateway changes four rows).
    [[nodiscard]] const Spell *game_spell(int number) const
    {
        return spell(number, game_of(identity_));
    }

    int roll(int sides)
    {
        return roll_die(random_, sides);
    }

    // The fight's own dice, for a rule written to draw from any dice (each_way).
    [[nodiscard]] auto fight_dice()
    {
        return [this](int sides)
        {
            return roll(sides);
        };
    }

    [[nodiscard]] Fighter *find(EntityId id);
    [[nodiscard]] const Fighter *find(EntityId id) const;

    [[nodiscard]] static auto has_id(EntityId id)
    {
        return [id](const Fighter &f)
        {
            return f.id == id;
        };
    }

    [[nodiscard]] rules::MovementGrid grid_for(const Fighter &mover) const;
    void log(Message message);
    void begin_round();
    void age_the_hasted();
    void make_younger(Fighter &user);
    [[nodiscard]] std::vector<Held> held_now(const Fighter &f) const;
    [[nodiscard]] Combatant numbers_with(const Fighter &f, const std::vector<Held> &held,
                                         std::vector<int> &order) const;
    void rework_gear(Fighter &f, const std::vector<Held> &before_held,
                     std::vector<Held> after_held) const;
    void toggle_readied(Fighter &f, std::size_t index);
    void conjure_hammer(Fighter &caster);
    void settle_hammers();
    void equip_best(Fighter &f);
    [[nodiscard]] int weapon_priority(const Fighter &f, const Held &weapon, int other_hands) const;
    void next_actor();
    void end_round();
    void join_arrivals();
    [[nodiscard]] bool place_near(Fighter &f, Cell wanted);
    void end_actions(Fighter &fighter);
    void note_attack(const Fighter &attacker, Fighter &defender);
    void make_attack(Fighter &attacker, Fighter &defender, bool from_behind);
    [[nodiscard]] std::optional<std::string> attack_verb(const Fighter &attacker,
                                                         const Fighter &target) const;
    [[nodiscard]] bool enemy_next_to(const Fighter &f) const;
    void shoot(Fighter &attacker, Fighter &target);
    bool sweep(Fighter &attacker, Fighter &defender);
    void step_to(Fighter &mover, Cell next);
    void free_attacks(Fighter &mover, Cell next);
    void guards_strike(Fighter &mover);
    [[nodiscard]] bool can_bandage(const Fighter &bandager, const Fighter &dying) const;
    void inflict(Fighter &target, int damage);
    void slay_helpless(Fighter &attacker, Fighter &defender);
    void end_effects_at_zero(Fighter &f);
    void give_up_crystal(Fighter &f);
    void love_at_fall(Fighter &fallen);
    [[nodiscard]] int love_modifier(const Fighter &attacker, const Fighter &defender) const;
    [[nodiscard]] bool frenzied(const Fighter &f) const;
    void release(Fighter &holder, int code);
    void on_hit(Fighter &attacker, Fighter &defender, int phase, int primary_hits, int natural,
                int attack_roll);
    void savage_frontier_specials(Fighter &f);
    // A disease that takes hold at once: Strength and Hit Points waste away
    // (effhndlr.pas, EffDiseased), the disease itself kept too when `lasting`.
    void diseased_now(Fighter &victim, int level, bool lasting);
    // Whether the creature's readied weapon, or its readied armor, has this
    // name word in any of its parts.
    [[nodiscard]] bool weapon_named(const Fighter &f, int word) const;
    [[nodiscard]] bool armor_named(const Fighter &f, int word) const;
    // A hold of a Gateway or Treasures creature begins: the holder keeps its
    // victim's number in `hold`, and the victim cannot move.
    void hold_fast(Fighter &holder, Fighter &victim, int hold, int victim_effect);
    void poison(Fighter &victim, int modifier);
    void paralyze(Fighter &victim, int minutes, int modifier);
    void kill(Fighter &victim, std::string_view message = "{name} is killed.");
    void drain_levels(Fighter &victim, int levels);
    void rake(Fighter &attacker, Fighter &victim);
    void attached_turn(Fighter &f);
    void curse_turn_start(Fighter &f);
    void turn_undead(Fighter &cleric);
    void treasures_turn_undead(Fighter &turner);
    [[nodiscard]] bool may_turn(const Fighter &f) const;
    [[nodiscard]] Fighter *strongest_undead(const Fighter &turner, int highest_level);
    void darkness_turn_undead(Fighter &cleric);
    void begin_cast(Fighter &caster, int spell_number);
    void start_aiming(Fighter &caster, int spell_number);
    void resolve_spell(Fighter &caster, std::vector<EntityId> targets, Cell aim,
                       Flames flames = Flames::random);
    void lose_spell(Fighter &caster);
    void spend_item(Fighter &user, int ordinal, int number);
    [[nodiscard]] std::vector<rules::Command> aiming_commands(const Fighter &caster) const;
    [[nodiscard]] bool reachable_by_spell(const Fighter &caster, const Fighter &target,
                                          int spell) const;
    [[nodiscard]] std::vector<EntityId> within(Cell aim, int radius) const;
    [[nodiscard]] int burst_gap(Cell aim, const Fighter &f) const;

    // Whether a burst of this radius reaches a creature so far from its
    // square (burst_gap).
    [[nodiscard]] static bool in_burst(int gap, int radius) noexcept
    {
        return gap <= 2 * radius + 1;
    }

    void cast_generic(Fighter &caster, int number, int level, const std::vector<EntityId> &targets,
                      int damage, int element, int save_modifier = 0, int minutes = -1);
    [[nodiscard]] bool silenced(const Fighter &f) const;
    [[nodiscard]] int prayer_bonus(const Fighter &f) const;
    [[nodiscard]] bool protected_from(const Fighter &f, int code, int aura) const;
    bool mirror_absorbs(Fighter &target);
    void lose_image(Fighter &target);

    // Whether one of a creature's mirror images takes a harm: on 1d(images +
    // 1) above 1 (effhndlr.pas, EffMirrorImage).
    template <typename Dice>
    [[nodiscard]] bool image_takes_it(const Effect &images, Dice &dice) const
    {
        const int count = game_of(identity_) == GoldBoxGame::dark_queen_of_krynn
                              ? images.strength >> 5
                              : images.strength;
        return dice(count + 1) > 1;
    }

    [[nodiscard]] bool aura_protects(const Fighter &attacker, const Fighter &defender) const;
    bool add_with_message(Fighter &target, Effect added, bool negates, bool saved, int element,
                          int caster_level, const Spell *known);
    template <typename Dice>
    [[nodiscard]] bool refuses_effect(const Fighter &target, int code, int element,
                                      int caster_level, const Spell *known, Dice &dice) const;
    void nonweapon_damage(Fighter &target, int damage, int save_result, bool saved, int element,
                          int caster_level, int spell_number, bool single_target);

    // What a target's defenses make of a non-weapon harm (defended_harm): the
    // damage it comes to, an image taking it, a protection turning it, a
    // shambling mound growing by so many Hit Points, the druid's Protection
    // from Fire spending so many of its levels or ending, and a troll's
    // regeneration about to begin.
    struct Defended
    {
        int damage{};
        bool image_took_it{}, unaffected{};
        int grown{}, soak_spent{};
        bool soak_ends{}, regenerates_soon{};
    };

    template <typename Dice>
    [[nodiscard]] Defended defended_harm(const Fighter &target, int damage, int save_result,
                                         bool saved, int element, int caster_level,
                                         int spell_number, bool single_target, int damage_dice,
                                         Dice &dice) const;
    void absorb_harm(Fighter &target, int damage, int element);
    void charm(Fighter &target, Side charmer);
    void cast_sleep(int level, Cell aim);
    [[nodiscard]] std::vector<EntityId> sleep_takes(Cell aim, int pool) const;
    [[nodiscard]] std::vector<EntityId> path_area(const Fighter &caster, Cell aim, int max_range,
                                                  int width) const;
    [[nodiscard]] std::vector<Cell> path_squares(const Fighter &caster, Cell aim, int max_range,
                                                 int width) const;
    void lay_cloud(Fighter &caster, int level, Cell aim, CloudKind kind);
    [[nodiscard]] Cloud cloud_over(EntityId caster, int index, Cell aim, CloudKind kind) const;
    [[nodiscard]] std::vector<EntityId>
    haste_slow_takes(const Fighter &caster, int number, int level,
                     const std::vector<EntityId> &targets) const;
    [[nodiscard]] bool dispellable(const Effect &e) const;
    [[nodiscard]] int dispel_chance(const Effect &e, int caster_level) const;
    [[nodiscard]] static SpellDice shared_dice(int number, int level, int necklace_missile = 1);

    // Lightning Bolt's length in squares past its aim: 8 in Pool of
    // Radiance, 9 in Treasures and 7 in the other games (resolve_spell).
    [[nodiscard]] int bolt_length() const
    {
        const auto game = game_of(identity_);
        return game == GoldBoxGame::pool_of_radiance                   ? 8
               : game == GoldBoxGame::treasures_of_the_savage_frontier ? 9
                                                                       : 7;
    }

    // Cone of Cold's length: half the caster's level, at least a square.
    [[nodiscard]] static int cone_length(int level) noexcept
    {
        return std::max((level + 1) / 2, 1);
    }

    // Ice Storm's element: cold and magic, magic alone in Pools of Darkness
    // and the kit (resolve_spell).
    [[nodiscard]] int ice_storm_element() const
    {
        return darkness() || kit() || game_of(identity_) == GoldBoxGame::dark_queen_of_krynn ? 8
                                                                                             : 10;
    }

    void remove_with_handler(Fighter &f, std::size_t node);
    void dispel(Fighter &target, int level);
    void animate_dead(Fighter &caster, int level);
    void slow_poison(Fighter &target, int level);
    void revive(Fighter &f);
    [[nodiscard]] bool in_cloud(const Fighter &f, CloudKind kind) const;
    void breathe_cloud(Fighter &f, bool round_ends = false);
    void breathe_stinking_cloud(Fighter &f);
    void breathe_darkness_stench(Fighter &f);
    void breathe_cloudkill(Fighter &f);
    void breathe_darkness_cloudkill(Fighter &f);
    [[nodiscard]] int cloud_caster_level(CloudKind kind, int spell_number) const;
    bool lightning_at(Cell cell, int damage, int level, int save = 4, int element = 12,
                      int spell_number = 51);
    void lightning_path(Cell origin, Cell aim, int length, int damage, int level, bool bounce,
                        int save = 4, int element = 12, int spell_number = 51);
    [[nodiscard]] bool turns_bolt(Cell cell) const;
    template <typename Strike>
    void bolt_walk(Cell origin, Cell aim, int length, bool bounce, const Strike &strike) const;
    template <typename Strike>
    void treasures_bolt_walk(Cell origin, Cell aim, int length, bool near_bounce_doubles,
                             const Strike &strike) const;
    void monster_specials(Fighter &f);
    void darkness_gazes(Fighter &f);
    void curse_specials(Fighter &f);
    void secret_specials(Fighter &f);
    void kit_specials(Fighter &f);
    void dark_queen_specials(Fighter &f);
    void darkness_specials(Fighter &f);
    bool lightning_breath(Fighter &dragon);
    [[nodiscard]] const Fighter *lightning_bolt_aim(Fighter &caster);
    void kit_turn_start(Fighter &f);
    void petrifying_gaze(Fighter &gazer);
    bool secret_breath(Fighter &dragon, int code, int length, int element, int width = 3,
                       std::optional<int> damage = std::nullopt);
    [[nodiscard]] Fighter *secret_target(Fighter &f, int range, bool skip_helpless);
    void turn_to_stone(Fighter &victim);
    void cast_secret_spell(Fighter &caster, int number, int level,
                           const std::vector<EntityId> &targets, Cell aim);
    void heal_all_but_1d4(Fighter &target);
    void cast_delayed_blast(Fighter &caster, int level, const std::vector<EntityId> &targets,
                            Cell aim);
    void delayed_blast_goes_off(Fighter &caster, int index);
    [[nodiscard]] std::vector<EntityId> fireball_reach(const std::vector<EntityId> &targets,
                                                       Cell aim) const;
    void recount_strength(Fighter &f);
    void cast_strength_spell(Fighter &caster, int number, int level,
                             const std::vector<EntityId> &targets);
    void cast_death_spell(const std::vector<EntityId> &targets);
    template <typename Most>
    [[nodiscard]] std::vector<EntityId> death_spell_takes(const std::vector<EntityId> &targets,
                                                          int budget,
                                                          const Most &most_hit_points) const;
    void cast_darkness_spell(Fighter &caster, int number, int level,
                             const std::vector<EntityId> &targets, Cell aim);
    void cast_mass_charm(Fighter &caster, int level, const std::vector<EntityId> &targets);

    // What Mass Charm does with a target: passes over one it cannot take,
    // saying so, passes over one whose Hit Dice it has no room left for, or
    // tries to charm it.
    enum class MassCharmTake
    {
        unaffected,
        passed_over,
        tried
    };

    [[nodiscard]] std::vector<std::pair<EntityId, MassCharmTake>>
    mass_charm_takes(const Fighter &caster, int level, const std::vector<EntityId> &targets) const;
    void cast_power_word_blind(const std::vector<EntityId> &targets);
    template <typename Most>
    [[nodiscard]] std::vector<EntityId> blinded_by_word(const std::vector<EntityId> &targets,
                                                        const Most &most_hit_points) const;
    void cast_meteor_swarm(Fighter &caster, int level, Cell aim);

    struct MeteorFall
    {
        Cell landing;
        std::vector<EntityId> struck;
    };

    [[nodiscard]] MeteorFall meteor_fall(const Fighter &caster, Cell aim, Cell offset) const;
    void cast_power_word_kill(const std::vector<EntityId> &targets);
    template <typename Most>
    [[nodiscard]] std::vector<EntityId> slain_by_word(const std::vector<EntityId> &targets,
                                                      const Most &most_hit_points) const;
    void cut_by_blades(Fighter &f, bool may_save);
    [[nodiscard]] bool turns_aside_slaying(Fighter &target);
    void secret_poison(Fighter &victim, int modifier);
    void swallow_whole(Fighter &swallower, Fighter &victim);
    void iron_golem_absorbs(Fighter &golem, int damage, int element);
    void dragon_fear(const Fighter &dragon);
    void darkness_dragon_fear(const Fighter &dragon);
    void frighten_on_surprise(const Fighter &yeti);
    void stone_golem_turn(Fighter &golem);
    void order_turns();
    void reorder_after_cast(Fighter &caster);
    [[nodiscard]] bool breathes_clouds_freely(const Fighter &f) const;
    [[nodiscard]] bool refuses_spell(const Fighter &target, int number) const;
    [[nodiscard]] bool treasures_refuses_spell(const Fighter &target, int number) const;
    void beholder_rays(Fighter &f);
    [[nodiscard]] int uses(Fighter &f, int code);
    void spend_use(Fighter &f, int code);
    [[nodiscard]] EntityId standing_at(Cell cell) const;
    void combat_start();
    void round_end_effects();
    void expire(Fighter &f, const Effect &ended);
    void went_down(Fighter &f);
    void aurak_final_form(Fighter &f, bool revived);
    void draconian_explosion(Fighter &f, int dice, bool stuns);
    void champions_turn_start(Fighter &f);
    void kender_yell(Fighter &f);
    [[nodiscard]] bool cell_free(const Fighter &f) const;
    void stand_up(Fighter &f, int hit_points, std::string_view message);
    [[nodiscard]] bool can_target(const Fighter &attacker, const Fighter &target) const;
    [[nodiscard]] bool turned_aside(Fighter &defender);
    [[nodiscard]] int weapon_damage_defenses(Fighter &attacker, Fighter &defender, int damage,
                                             int phase, int primary_attacks, int &primary_hits);
    [[nodiscard]] int six_weapons_blow(const Fighter &attacker, int primary_attacks, int damage);
    bool eat_armor(const Fighter &pudding, Fighter &target);
    void age_effects();
    [[nodiscard]] bool saving_throw(const Fighter &f, int save, int modifier,
                                    const Fighter *caster = nullptr, int element = 0);
    [[nodiscard]] bool saves_with(const Fighter &f, int natural, int save, int modifier,
                                  const Fighter *caster = nullptr, int element = 0) const;
    [[nodiscard]] double save_chance(const Fighter &f, int save, int modifier,
                                     const Fighter *caster = nullptr, int element = 0) const;

    // A saving throw's natural roll: a dancer's is always a 1 (fxengine.pas,
    // RollSavingThrow).
    template <typename Dice> [[nodiscard]] static int save_die(const Fighter &f, Dice &dice)
    {
        return f.has(effect::dancing) ? 1 : dice(20);
    }

    void update_hostile_hit_points();
    [[nodiscard]] int highest_enemy_movement(const Fighter &f) const;
    bool check_morale(Fighter &f);
    void attempt_flee(Fighter &f);
    void end_fight_by_escape(const Fighter &escaped);
    // The original's AI (combatai.pas).
    // A party member on Quick casts only with Quick magic on (`magic`).
    void ai_turn(Fighter &f, bool magic);
    bool ai_use_item(Fighter &f);
    bool ai_cast(Fighter &f, bool magic);
    // The best aims found so far for area spells a party-side caster weighs
    // in one turn, by spell (party_area_aim).
    using AreaAims = std::map<int, std::optional<Cell>>;
    [[nodiscard]] bool ai_decides(const Fighter &f, int number, int least_priority,
                                  AreaAims &area_aims) const;
    bool ai_aim(Fighter &f);
    // A party member's area spells (docs/ORIGINAL-RULES.md, Party area spells).
    class Forecast;
    [[nodiscard]] bool weighs_area_spell(const Fighter &f, int number) const;
    [[nodiscard]] std::optional<Cell> party_area_aim(const Fighter &f, int number) const;
    bool aim_party_area_spell(Fighter &f);
    // Treasures' own (its combatai.pas and cbtacts.pas): the spell of highest
    // AI weight that has a target, its targets rated, and an area spell
    // aimed where it takes the most.
    bool treasures_ai_cast(Fighter &f, bool magic);
    [[nodiscard]] int treasures_spell_rating(const Fighter &caster, const Fighter &target,
                                             int number) const;
    [[nodiscard]] bool treasures_spell_has_target(const Fighter &f, int number) const;
    [[nodiscard]] Fighter *treasures_spell_target(Fighter &f, int number);
    [[nodiscard]] std::optional<Cell> treasures_area_aim(const Fighter &f, int number);
    bool treasures_ai_aim(Fighter &f);
    // Dark Queen's ratings and area aim (its cbaction.c).
    [[nodiscard]] int native_rank_target(const Fighter &caster, const Fighter &target,
                                         int number) const;
    [[nodiscard]] std::optional<Cell> native_area_aim(const Fighter &f, int number);
    [[nodiscard]] const Fighter *cure_search(const Fighter &f) const;
    bool acquire_target(Fighter &f, int range, bool relaxed, bool force_new);
    [[nodiscard]] std::vector<EntityId> enemies_within(const Fighter &f, int range,
                                                       bool sight) const;
    bool ai_move_attack(Fighter &f);
    bool ai_step(Fighter &f, int &last_direction, int &stuck);
    bool take_ai_step(Fighter &f, int direction, int &last_direction);
    bool can_step(const Fighter &f, int direction, bool &edge) const;
    [[nodiscard]] bool darkness_balks(const Fighter &f, const Fighter &moved) const;
    // Whether Secret's AI balks at a step into a cloud: not at all, unless it
    // saves against paralysis, or always.
    enum class CloudBalk
    {
        none,
        unless_saved,
        always
    };
    [[nodiscard]] CloudBalk cloud_balk(const Fighter &f, const Fighter &moved) const;

    struct Route
    {
        int direction{};
        EntityId target{};
    };

    [[nodiscard]] std::optional<Route> route_to_enemy(const Fighter &f) const;
    void finish_round(Fighter &f);
    [[nodiscard]] bool missile_in_hand(const Fighter &f) const;
    void put_out_of_play(Fighter &f, Condition condition);
    [[nodiscard]] std::vector<Cell> off_field_steps(const Fighter &f) const;
    void settle_outcome();

    // Buck Rogers ground combat (buck_rogers_combat.h), for GroundFighters.
    struct GroundWalk
    {
        bool reached{};
        Cell stop;
        int distance{};
        bool chaff{}, mist{}, blocked{};
    };

    [[nodiscard]] GroundNumbers ground_numbers_of(const Fighter &f) const;
    void ground_refresh(Fighter &f);
    void ground_begin_round();
    void native_leadership();
    [[nodiscard]] bool ground_turn_starts(Fighter &f);
    void ground_attack(Fighter &attacker, Fighter &defender);
    void ground_launch(Fighter &attacker, Cell aim);
    void ground_inflict(Fighter &target, int damage);
    void ground_down(Fighter &f);
    void ground_end_round();
    [[nodiscard]] bool ground_cloud_at(Cell cell, CloudMatter matter) const;
    [[nodiscard]] GroundWalk ground_walk(Cell from, Cell to, int range, bool stops_at_chaff) const;
    [[nodiscard]] GroundLine ground_line(const Fighter &attacker, const Fighter &defender) const;
    [[nodiscard]] bool ground_reaches(const Fighter &attacker, const Fighter &target) const;
    [[nodiscard]] std::vector<rules::Command> ground_commands(const Fighter &actor) const;
    bool ground_submit(Fighter &actor, const rules::Command &command);
    void ground_guards_strike(Fighter &mover);
    void ground_after_step(Fighter &mover);
    void ground_intimidate(Fighter &f);
    void ground_aid(Fighter &medic);
    [[nodiscard]] bool ground_has_patient(const Fighter &medic) const;
    void ground_jetpack(Fighter &f);
    void ground_land(Fighter &f, Cell landing);
    void ground_tactics();
    void write_ground(Writer &out) const;
    void read_ground(Reader &in);
};

// Whom a Death Spell of this budget slays among its targets, by their
// levels and each one's most Hit Points as `most_hit_points` gives them
// (cast_death_spell).
template <typename Most>
std::vector<EntityId> Session::death_spell_takes(const std::vector<EntityId> &targets, int budget,
                                                 const Most &most_hit_points) const
{
    std::vector<EntityId> taken;
    for (const auto id : targets)
    {
        const auto &target = *find(id);
        const int level = target.numbers.level;
        const int cost = level == 0   ? 1
                         : level == 1 ? 2
                         : level <= 3 ? 8
                         : level <= 5 ? 16
                         : level == 6 ? (most_hit_points(target) <= 51 ? 16 : 20)
                         : level <= 8 ? 20
                                      : 162;
        if (budget < cost)
        {
            continue;
        }
        budget -= cost;
        taken.push_back(id);
    }
    return taken;
}

// Whom Power Word Blind blinds among its targets, the nearest first, by each
// one's most Hit Points as `most_hit_points` gives them (cast_power_word_blind).
template <typename Most>
std::vector<EntityId> Session::blinded_by_word(const std::vector<EntityId> &targets,
                                               const Most &most_hit_points) const
{
    std::vector<EntityId> blinded;
    int total = 0;
    for (const auto id : targets)
    {
        const int hit_points = most_hit_points(*find(id));
        if (total > 100)
        {
            break;
        }
        if (total + hit_points <= 100)
        {
            total += hit_points;
            blinded.push_back(id);
        }
    }
    return blinded;
}

// Whom Power Word Kill slays among its targets, by each one's most Hit
// Points as `most_hit_points` gives them (cast_power_word_kill).
template <typename Most>
std::vector<EntityId> Session::slain_by_word(const std::vector<EntityId> &targets,
                                             const Most &most_hit_points) const
{
    if (targets.empty())
    {
        return {};
    }
    const int first_most = most_hit_points(*find(targets.front()));
    if (first_most > 10 && first_most <= 60)
    {
        return {targets.front()};
    }
    std::vector<EntityId> slain;
    for (const auto id : targets)
    {
        if (most_hit_points(*find(id)) <= 10)
        {
            slain.push_back(id);
        }
    }
    return slain;
}

// Whether the target's immunities nullify an effect a spell would add
// (add_with_message); `dice` draws the chances some of them take. It changes
// nothing, so an outlook may weigh each way it can go (each_way).
template <typename Dice>
bool Session::refuses_effect(const Fighter &target, int code, int element, int caster_level,
                             const Spell *known, Dice &dice) const
{
    const auto is = [code](std::initializer_list<int> codes)
    {
        return std::find(codes.begin(), codes.end(), code) != codes.end();
    };
    bool immune = target.has(effect::aurak_invulnerable) ||
                  (resists_magic_at_all(target) && magic_resists(target, caster_level, dice(100)));
    immune = immune || (target.has(107) && is({effect::sleep, effect::charm}) && dice(100) <= 90);
    immune = immune || (target.has(124) && is({effect::sleep, effect::charm}) && dice(100) <= 30);
    immune = immune || (target.has(108) && is({effect::charm, effect::sleep}));
    immune = immune || (target.has(109) && is({effect::paralysis}));
    immune = immune || (target.has(110) && (element & 2));
    immune = immune || (target.has(111) && is({effect::poisoned, effect::paralysis}));
    immune = immune || (target.has(112) && (element & 1));
    immune = immune || (target.has(125) &&
                        is({effect::charm, effect::sleep, effect::paralysis, effect::poisoned}));
    // Curse's protections (ovr013): from magic, every spell's effect; Minor
    // Globe of Invulnerability, a spell's below the fourth level; from cold
    // and electricity, theirs.
    const bool spell_effect = known && known->caster_class != SpellClass::item;
    immune = immune || (target.has(329) && known);
    // A dracolich shrugs off fear, enfeeblement and feeblemind (Curse's 133,
    // which it checked too late to work, docs/ORIGINAL-RULES.md, Bugs).
    immune = immune || (target.has(333) && is({342, effect::enfeebled, 268}));
    immune = immune || (target.has(263) && spell_effect && known->level < 4);
    immune = immune || (target.has(310) && (element & 2));
    immune = immune || (target.has(335) && (element & 4));
    // Secret's (effhndlr.pas, its condition 9): the Globe of Invulnerability
    // stops a spell's to the fourth level, the fearless are never afraid, and
    // an iron golem takes no effect at all.
    immune = immune ||
             (target.has(effect::globe_of_invulnerability) && spell_effect && known->level <= 4);
    immune = immune || (target.has(effect::fearless) && is({effect::fear}));
    // Pools of Darkness's Mind Blank turns charm, confusion, holding,
    // feeblemind and fear away (effhndlr.pas, EffMindBlank).
    immune = immune || (target.has(effect::mind_blank) &&
                        is({effect::charm, 235, effect::paralysis, 268, effect::fear}));
    immune = immune || target.has(effect::iron_golem);
    immune = immune || (target.has(dkk_effect::confusion_proof) && code == 235);
    immune = immune || target.has(dkk_effect::flesh_golem);
    // The kit's (effects.c): a spell below the eighth level takes no hold
    // (40; Pools of Darkness's Rakshasa too, effhndlr.pas, EffRakshasaProt),
    // nor does disease on an otyugh (76).
    immune = immune || (target.has(effect::spell_proof) && spell_effect && known->level < 8);
    constexpr int diseased = 34;
    immune = immune || (target.has(effect::disease_proof) && code == diseased);
    // A Treasures stone guardian takes no effect at all (its 142, NullifyAttack),
    // and a stone golem none from a spell but Stone to Flesh (EffSlowAura).
    immune = immune || target.has(effect::effects_proof);
    immune = immune || (target.has(effect::stone_golem) && known);
    return immune;
}

// The damage a non-weapon harm comes to once the target's defenses weigh it,
// and what they do on the way, which nonweapon_damage then does; `dice`
// draws what the defenses roll, and `damage_dice` is how many dice the harm
// was rolled with, which the kit's defenses weigh fire by. It changes
// nothing, so an outlook may weigh each way it can go (each_way).
template <typename Dice>
Session::Defended Session::defended_harm(const Fighter &target, int damage, int save_result,
                                         bool saved, int element, int caster_level,
                                         int spell_number, bool single_target, int damage_dice,
                                         Dice &dice) const
{
    Defended defended;
    if (target.has(dkk_effect::cold_vulnerable) && (element & 2))
    {
        damage *= 2;
    }
    if (target.has(dq_effect::cold_harms_more) && (element & 2))
    {
        damage += damage / 5;
    }
    if (target.has(dq_effect::resists_fire) && (element & 1))
    {
        damage = damage / 5 * 4;
    }
    if (target.has(dkk_effect::cold_electricity_resistance) && (element & 6))
    {
        damage /= 2;
    }
    // Mirror images take a single target's spell (effhndlr.pas, EffMirrorImage).
    if (const auto *images = target.find_effect(effect::mirror_image);
        single_target && images && image_takes_it(*images, dice))
    {
        defended.image_took_it = true;
        damage = 0;
    }
    if (target.has(91) && (spell_number == 15 || (element & 4)))
    {
        damage = 0;
    }
    // A Treasures stone golem takes no harm from a spell (EffSlowAura).
    if (target.has(effect::stone_golem) && spell_number != 0)
    {
        damage = 0;
    }
    // Gateway's fire harms more by 1d6, and 1d6 again unsaved; Treasures'
    // by half (EffFireVulnerable); a Treasures stone guardian takes half from
    // the elements and magic (EffHalfElemental).
    if ((element & 1) && target.has(effect::fire_harms_more))
    {
        damage += dice(6);
        damage += saved ? 0 : dice(6);
    }
    if ((element & 1) && target.has(effect::fire_harms_half_more))
    {
        damage += damage / 2;
    }
    if ((element & 0x0F) && target.has(effect::halves_elements))
    {
        damage /= 2;
    }
    if ((element & 2) && target.has(effect::resist_cold))
    {
        damage /= 2;
    }
    if ((element & 1) && target.has(effect::resist_fire))
    {
        damage /= 2;
    }
    // The druid's Protection from Fire takes 12 points of fire a level of its
    // strength, and ends once it has taken them all (effhndlr.pas,
    // EffResistFireDruid).
    if (const auto *soak = target.find_effect(effect::druid_fire_protection); soak && (element & 1))
    {
        const int taken = std::min(damage, 12 * soak->strength);
        damage -= taken;
        defended.soak_spent = (taken + 11) / 12;
        defended.soak_ends = soak->strength - defended.soak_spent <= 0 || damage > 0;
    }
    // Secret's resistance turns a spell's damage only when the spell carries
    // an effect too, so a Fireball's goes through (fxengine.pas,
    // CheckSpellResistance, in its reconstruction).
    const auto *cast = game_spell(spell_number);
    const bool resistible = game_of(identity_) == GoldBoxGame::dark_queen_of_krynn ||
                            target.numbers.magic_resistance == 0 || (cast && cast->effect != 0);
    if ((element & 8) && resistible && resists_magic_at_all(target) &&
        magic_resists(target, caster_level, dice(100)))
    {
        damage = 0;
    }
    // Curse's protections (ovr013): cold for one immune to it (310), dragon
    // breath (273), magic (329), electricity (335), and a spell below the
    // fourth level for a Minor Globe of Invulnerability (263), Secret's
    // Globe of Invulnerability, to the fourth, and the kit's rakshasa (its
    // 40), below the eighth.
    const bool spell_cast = cast && cast->caster_class != SpellClass::item;
    if (((element & 2) && target.has(310)) || ((element & 0x20) && target.has(273)) ||
        ((element & 8) && target.has(329)) || ((element & 4) && target.has(335)) ||
        (target.has(263) && spell_cast && cast->level < 4) ||
        (target.has(effect::globe_of_invulnerability) && spell_cast && cast->level <= 4) ||
        (target.has(effect::spell_proof) && spell_cast && cast->level < 8))
    {
        defended.unaffected = true;
        damage = 0;
    }
    // A shambling mound takes in electricity and grows: 1d8 Hit Points, past
    // its maximum (Curse's added 8 and threw its 1d8 away, docs/ORIGINAL-RULES.md, Bugs).
    if ((element & 4) && target.has(284) && damage > 0)
    {
        damage = 0;
        const int growth = dice(8);
        defended.grown = game_of(identity_) == GoldBoxGame::dark_queen_of_krynn ? 8 : growth;
    }
    // Resisting fire and cold (282): a save takes none of a spell that allows
    // one, else half.
    if ((element & 3) && target.has(282) && damage > 0)
    {
        const bool saved_whole = saves_with(target, save_die(target, dice), 4, 0);
        damage = saved_whole && save_result != 0 ? 0 : damage / 2;
    }
    if ((element & 1) && target.has(112))
    {
        damage = 0;
    }
    if ((element & 4) && target.has(114))
    {
        damage /= 2;
    }
    if ((element & 2) && target.has(118))
    {
        damage /= 2;
    }
    if (spell_number == 15 && target.has(effect::shield))
    {
        damage = 0;
    }
    if ((element & 1) && target.has(93))
    {
        damage /= 2;
    }
    defended.regenerates_soon = target.has(effect::troll_heal) &&
                                !target.has(effect::regeneration) &&
                                !target.has(effect::regenerating_soon);
    // The kit's (effects.c), by the dice the damage was rolled with: a mummy
    // takes 1 more a die of magical fire (168); an efreeti 1 less a die of
    // fire, never under 1 a die (219).
    if ((element & 9) == 9 && target.has(effect::hurt_more_by_magical_fire) && damage > 0)
    {
        damage += damage_dice;
    }
    if ((element & 1) && target.has(effect::fire_less_by_the_die) && damage > 0)
    {
        damage = std::max(damage - damage_dice, damage_dice);
    }
    // The kit's ring of fire resistance: no harm from ordinary fire, and 2
    // less a die of magical fire, never under 1 a die (effects.c, fx_fire_ring).
    if ((element & 1) && target.has(effect::fire_resistance_ring) && damage > 0)
    {
        damage = (element & 8) ? std::max(damage - 2 * damage_dice, damage_dice) : 0;
    }
    if (saved && save_result == 1)
    {
        damage = 0;
    }
    if (saved && save_result == 2)
    {
        damage /= 2;
    }
    if (game_of(identity_) == GoldBoxGame::dark_queen_of_krynn &&
        (((element & 1) && target.has(254)) || ((element & 2) && target.has(250))))
    {
        damage = saved ? 0 : damage / 2;
    }
    // A fire shield doubles what it is made of, unless saved against.
    if (!saved && (((element & 1) && target.has(250)) || ((element & 2) && target.has(254))))
    {
        damage *= 2;
    }
    defended.damage = damage;
    return defended;
}

// The way lightning_path's bolt runs, telling `strike` of each square it
// strikes, which answers whether the bolt turns there; Treasures' bolt runs
// its own way (treasures_bolt_walk). The squares it walks take whoever
// stands on them as it goes.
template <typename Strike>
void Session::bolt_walk(Cell origin, Cell aim, int length, bool bounce, const Strike &strike) const
{
    if (origin == aim)
    {
        return;
    }
    if (treasures())
    {
        treasures_bolt_walk(origin, aim, length, bounce, strike);
        return;
    }
    Cell target = aim;
    EntityId last = standing_at(aim);
    int sign = 1, budget = length * 2;
    while (budget > 0)
    {
        LineWalk walk(target, {target.x + (target.x - origin.x) * sign * budget,
                               target.y + (target.y - origin.y) * sign * budget});
        bool blocked = false;
        do
        {
            bool off_field = false;
            EntityId here = 0;
            if (walk.start != walk.end)
            {
                bool more = true;
                do
                {
                    more = walk.next();
                    here = standing_at(walk.at);
                    off_field = !board_.contains(walk.at);
                } while (more && !(here && here != last) && !off_field &&
                         board_.at(walk.at) == rules::Terrain::open && walk.distance < budget);
            }
            last = here;
            if (off_field)
            {
                budget = 0;
            }
            blocked = strike(walk.at);
            if (blocked)
            {
                target = walk.at;
                LineWalk back(target, origin);
                while (back.next())
                {
                }
                if (bounce && back.distance <= 8)
                {
                    walk.distance += 8;
                }
                sign = -sign;
                bounce = false;
                last = 0;
            }
            budget = walk.distance < budget ? budget - walk.distance : 0;
        } while (!blocked && budget > 0);
    }
}

// Treasures' bolt (its magic.pas, LightningPath) walks its length counting
// each step once: a leg costs the distance it walked, at least 2. It strikes
// each new creature it enters, never the one it struck last; out of doors
// (its overMap) it runs on through walls and rough ground, and indoors such a
// square turns it back toward the caster, a first turn within 8 of the
// caster costing the distance walked so far again. The field's edge ends it.
// The original kept the cost of the legs before a turn and took it from the
// first leg after, its byte wrapping, so the turned bolt most often ended at
// the first creature it met; and at the field's edge it struck the square
// before again, hitting the creature it had just hit a second time (obvious
// bugs, fixed: docs/ORIGINAL-RULES.md, Bugs).
template <typename Strike>
void Session::treasures_bolt_walk(Cell origin, Cell aim, int length, bool near_bounce_doubles,
                                  const Strike &strike) const
{
    Cell target = aim;
    EntityId last = standing_at(aim);
    int sign = 1, budget = length * 2;
    while (budget > 0)
    {
        LineWalk walk(target, {target.x + (target.x - origin.x) * sign * budget,
                               target.y + (target.y - origin.y) * sign * budget});
        int spent = 0;
        bool turned = false;
        do
        {
            EntityId here = 0;
            bool off_field = false, rough = false;
            if (walk.start != walk.end)
            {
                bool more = true;
                do
                {
                    more = walk.next();
                    off_field = !board_.contains(walk.at);
                    here = off_field ? 0 : standing_at(walk.at);
                    rough = !off_field && board_.at(walk.at) != rules::Terrain::open;
                } while (more && !(here && here != last) && !off_field && !rough &&
                         walk.distance < budget);
            }
            if (off_field)
            {
                return;
            }
            if (here != last && here)
            {
                (void)strike(walk.at);
            }
            last = here;
            int walked = walk.distance;
            if (rough && !outdoors_)
            {
                target = walk.at;
                LineWalk back(target, origin);
                while (back.next())
                {
                }
                if (near_bounce_doubles && back.distance <= 8)
                {
                    walked *= 2;
                }
                near_bounce_doubles = false;
                sign = -1;
                turned = true;
            }
            const int leg = std::max(walked - spent, 2);
            budget = leg < budget ? budget - leg : 0;
            spent = walked;
            if (walk.start == walk.end && !turned)
            {
                return;
            }
        } while (!turned && budget > 0);
    }
}

} // namespace opengold::original::detail

#endif
