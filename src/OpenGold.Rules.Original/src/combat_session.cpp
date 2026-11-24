#include "combat_session.h"
#include "champions_rules.h"
#include "dark_queen_effects.h"
#include "dark_queen_progression.h"
#include "dark_queen_rules.h"
#include "death_knights_effects.h"
#include "death_knights_rules.h"

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

// A fight under the original rules, as Pool of Radiance's combat units play it
// (docs/ORIGINAL-RULES.md, "Combat"): melee and missiles.
namespace opengold::original::detail
{
namespace
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

    [[nodiscard]] const Effect *find_effect(int code) const
    {
        return const_cast<Fighter *>(this)->find_effect(code);
    }

    [[nodiscard]] Effect *find_effect(int code)
    {
        const auto found = std::find_if(numbers.effects.begin(), numbers.effects.end(),
                                        [code](const Effect &e)
                                        {
                                            return e.code == code;
                                        });
        return found == numbers.effects.end() ? nullptr : &*found;
    }
};

// Whether two creatures stand side by side, any square of one next to any of the other's.
bool adjacent(const Fighter &a, const Fighter &b)
{
    const auto gap = [](int first, int size, int other_first, int other_size)
    {
        return std::max(
            {0, other_first - (first + size - 1), first - (other_first + other_size - 1)});
    };
    const int dx = gap(a.cell.x, a.footprint.width, b.cell.x, b.footprint.width);
    const int dy = gap(a.cell.y, a.footprint.height, b.cell.y, b.footprint.height);
    return std::max(dx, dy) == 1;
}

// The squares a creature covers.
std::vector<Cell> squares_of(const Fighter &f)
{
    std::vector<Cell> squares;
    for (int dy = 0; dy < f.footprint.height; ++dy)
    {
        for (int dx = 0; dx < f.footprint.width; ++dx)
        {
            squares.push_back({f.cell.x + dx, f.cell.y + dy});
        }
    }
    return squares;
}

// The original's distance between two creatures: a straight line's steps
// counted 2 straight and 3 diagonal, halved, between their nearest squares
// (charhlp.pas, CalcDistance; pathing.pas, NextPathStep).
int distance(const Fighter &a, const Fighter &b)
{
    int nearest = 1000;
    for (const auto from : squares_of(a))
    {
        for (const auto to : squares_of(b))
        {
            const int dx = std::abs(to.x - from.x), dy = std::abs(to.y - from.y);
            nearest = std::min(nearest, (2 * std::max(dx, dy) + std::min(dx, dy)) / 2);
        }
    }
    return nearest;
}

// A straight line walked a square at a time, its doubled distance growing by
// 2 a straight step and 3 a diagonal one (pathing.pas, PathFind and NextPathStep).
struct LineWalk
{
    Cell start, end, at;
    int across{}, down{}, step_x{}, step_y{}, error{}, diagonal_step{}, straight_step{}, distance{};

    LineWalk(Cell from, Cell to) : start(from), end(to), at(from)
    {
        across = std::abs(to.x - from.x);
        down = std::abs(to.y - from.y);
        step_x = (to.x > from.x) - (to.x < from.x);
        step_y = (to.y > from.y) - (to.y < from.y);
        const int major = std::max(across, down), minor = std::min(across, down);
        error = 2 * minor - major;
        diagonal_step = 2 * (minor - major);
        straight_step = 2 * minor;
    }

    // One step on; false at the line's end.
    bool next()
    {
        const bool x_major = across > down;
        if ((x_major && at.x == end.x) || (!x_major && at.y == end.y))
        {
            return false;
        }
        if (error >= 0)
        {
            (x_major ? at.y : at.x) += x_major ? step_y : step_x;
            error += diagonal_step;
            distance += 3;
        }
        else
        {
            error += straight_step;
            distance += 2;
        }
        (x_major ? at.x : at.y) += x_major ? step_x : step_y;
        return true;
    }
};

// Whether the line from one square to another is clear (pathing.pas,
// WalkLine): a square on it, both ends too, blocks it when the square's
// blocking height is above the clearance of the square looked from. Without
// heights an obstacle blocks and nothing else does.
bool clear_line(const rules::Battlefield &board, Cell from, Cell to)
{
    const auto at = [&board](Cell cell)
    {
        return static_cast<std::size_t>(cell.y * board.width + cell.x);
    };
    const auto blocking = [&](Cell cell) -> int
    {
        if (!board.contains(cell))
        {
            return 255;
        }
        if (board.blocking.empty())
        {
            return board.at(cell) == rules::Terrain::obstacle ? 2 : 0;
        }
        return board.blocking[at(cell)];
    };
    const int eye =
        board.clearance.empty() || !board.contains(from) ? 0 : board.clearance[at(from)];
    LineWalk line(from, to);
    do
    {
        if (blocking(line.at) > eye)
        {
            return false;
        }
    } while (line.next());
    return true;
}

// Whether a line runs from a square of one creature to a square of the other
// that the ground does not block; creatures do not (pathing.pas, BuildTargetList).
bool sees(const rules::Battlefield &board, const Fighter &a, const Fighter &b)
{
    for (const auto from : squares_of(a))
    {
        for (const auto to : squares_of(b))
        {
            if (clear_line(board, from, to))
            {
                return true;
            }
        }
    }
    return false;
}

// The original's eight directions: 0 north and clockwise to 7 north-west.
constexpr std::array<int, 8> step_x{0, 1, 1, 1, 0, -1, -1, -1};
constexpr std::array<int, 8> step_y{-1, -1, 0, 1, 1, 1, 0, -1};

// Whether a direction leads from one cell toward another: the other cell lies
// in the 90-degree cone around the cell one step that way (pathing.pas, IsDirToward).
bool leads_toward(Cell from, Cell to, int direction)
{
    const int nx = from.x + step_x[static_cast<std::size_t>(direction)];
    const int ny = from.y + step_y[static_cast<std::size_t>(direction)];
    const int tx = to.x, ty = to.y;
    if (from == to || (nx == tx && ny == ty))
    {
        return true;
    }
    switch (direction)
    {
    case 0:
        return (tx >= nx && ty <= nx - tx + ny) || (tx <= nx && ty <= tx - nx + ny);
    case 1:
        return (tx >= nx && ty <= nx - tx + ny) || (tx >= nx + ny - ty && ty <= ny);
    case 2:
        return (tx >= nx + ny - ty && ty <= ny) || (tx >= nx + ty - ny && ty >= ny);
    case 3:
        return (tx >= nx + ty - ny && ty >= ny) || (tx >= nx && ty >= tx - nx + ny);
    case 4:
        return (tx >= nx && ty >= tx - nx + ny) || (tx <= nx && ty >= nx - tx + ny);
    case 5:
        return (tx <= nx && ty >= nx - tx + ny) || (tx <= nx + ny - ty && ty >= ny);
    case 6:
        return (tx <= nx + ny - ty && ty >= ny) || (tx <= nx + ty - ny && ty <= ny);
    default:
        return (tx <= nx + ty - ny && ty <= ny) || (tx <= nx && ty <= tx - nx + ny);
    }
}

// The first direction that leads from one cell toward another (cbtacts.pas, DirTowardTarget).
int direction_toward(Cell from, Cell to)
{
    for (int direction = 0; direction < 8; ++direction)
    {
        if (leads_toward(from, to, direction))
        {
            return direction;
        }
    }
    return 0;
}

// The English of a message: its template with each {name} replaced.
std::string english(const Message &message)
{
    auto text = message.source;
    for (const auto &argument : message.arguments)
    {
        const auto key = "{" + argument.name + "}";
        for (auto at = text.find(key); at != std::string::npos; at = text.find(key))
        {
            text.replace(at, key.size(), argument.value);
        }
    }
    return text;
}

// A checkpoint's text: numbers and length-prefixed strings, space-separated.
class Writer
{
  public:
    template <class... T> Writer &operator()(const T &...values)
    {
        (put(values), ...);
        return *this;
    }

    [[nodiscard]] std::string text() const
    {
        return out_.str();
    }

  private:
    std::ostringstream out_;

    void put(const std::string &text)
    {
        out_ << text.size() << ':' << text << ' ';
    }

    void put(bool value)
    {
        out_ << (value ? 1 : 0) << ' ';
    }

    template <class T>
        requires std::is_integral_v<T>
    void put(T value)
    {
        out_ << +value << ' ';
    }
};

class Reader
{
  public:
    explicit Reader(std::string_view text) : text_(text)
    {
    }

    // Unsigned numbers read as unsigned: the random state uses all 64 bits.
    template <class Number> Number number()
    {
        skip();
        Number value{};
        const auto [end, error] =
            std::from_chars(text_.data() + at_, text_.data() + text_.size(), value);
        if (error != std::errc{})
        {
            throw std::runtime_error("Invalid original-rules combat checkpoint");
        }
        at_ = static_cast<std::size_t>(end - text_.data());
        return value;
    }

    template <class T> T as()
    {
        if constexpr (std::is_same_v<T, std::uint64_t>)
        {
            return number<std::uint64_t>();
        }
        return static_cast<T>(number<long long>());
    }

    std::string text()
    {
        const auto size = static_cast<std::size_t>(number<long long>());
        if (at_ >= text_.size() || text_[at_] != ':' || size > text_.size() - at_ - 1)
        {
            throw std::runtime_error("Invalid original-rules combat checkpoint");
        }
        std::string value(text_.substr(at_ + 1, size));
        at_ += size + 1;
        return value;
    }

    [[nodiscard]] bool finished()
    {
        skip();
        return at_ == text_.size();
    }

  private:
    std::string_view text_;
    std::size_t at_{};

    void skip()
    {
        while (at_ < text_.size() && text_[at_] == ' ')
        {
            ++at_;
        }
    }
};

int attack_modifier(const Fighter &attacker, const Fighter &defender, GoldBoxGame game);

// Whether Dispel Evil takes a creature for evil: by its alignment in Curse,
// by its record's flag in Secret (effhndlr.pas, EffDispelEvil and
// EffDispelEvilTouch, in its reconstruction).
bool dispel_evil_takes(const Fighter &f, GoldBoxGame game)
{
    if (game == GoldBoxGame::secret_of_the_silver_blades)
    {
        return (f.numbers.traits & trait::dispel_evil) != 0;
    }
    const int alignment = f.numbers.alignment;
    return alignment == 2 || alignment == 5 || alignment == 8;
}

// Whether a giant-bane weapon bites on the defender, one its record flags
// for it (target flag bit 0): Secret's (its effect 75, EffHolyWater) or the
// kit's giant-slaying sword (fxcombat.c, fx_damage_vs_immunity1), each 6d6
// and the wielder's Strength bonus.
bool giant_bane_strikes(const Fighter &attacker, const Fighter &defender, GoldBoxGame game)
{
    const bool secret_bane =
        game == GoldBoxGame::secret_of_the_silver_blades && attacker.has(effect::giant_bane);
    return (secret_bane || attacker.has(effect::kit_giant_bane)) &&
           (defender.numbers.traits & trait::giant_bane) != 0;
}

// Haste doubles a creature's rates of movement and attacks and Slow halves
// them; together they cancel (effhndlr.pas, EffHasted, EffSlowed).
int paced(const Fighter &f, int rate)
{
    const bool hasted = f.has(effect::hasted),
               slowed = f.has(effect::slowed) && !f.has(dq_effect::free_action);
    if (hasted && !slowed)
    {
        return rate * 2;
    }
    return slowed && !hasted ? rate / 2 : rate;
}

// A caster's level for a spell: its cleric or magic-user level, 12 for an
// item's own power (magic.pas, GetCasterLevel). Curse and the games after it
// count a paladin's cleric spells at its level less 8 (Secret at its level),
// a ranger's druid spells at its level less 7 and its magic-user spells at
// its level less 8, and cast at level 6 for one with no cleric or magic-user
// levels that is no paladin above 8 nor ranger above 7.
int caster_level(const Fighter &caster, const Spell &spell, GoldBoxGame game)
{
    // From an item a spell is cast at level 6, an item's own power at 12
    // (magic.pas, GetCasterLevel).
    if (caster.using_item >= 0)
    {
        return spell.caster_class == SpellClass::item ? 12 : 6;
    }
    const auto level = [&caster](ClassSlot slot)
    {
        return caster.numbers.class_levels[static_cast<std::size_t>(slot)];
    };
    if (spell.caster_class == SpellClass::item)
    {
        return 12;
    }
    if (game == GoldBoxGame::death_knights_of_krynn || game == GoldBoxGame::dark_queen_of_krynn)
    {
        const auto *moon = caster.find_effect(dkk_effect::casting_moon);
        const int encoded = moon ? moon->strength : 3;
        return game == GoldBoxGame::dark_queen_of_krynn
                   ? dark_queen_caster_level(caster.numbers.class_levels, spell.caster_class,
                                             encoded / 4, encoded % 4)
                   : death_knights_caster_level(caster.numbers.class_levels, spell.caster_class,
                                                encoded / 4, encoded % 4);
    }
    if (game == GoldBoxGame::pool_of_radiance)
    {
        return level(spell.caster_class == SpellClass::cleric ? ClassSlot::cleric
                                                              : ClassSlot::magic_user);
    }
    const int paladin = level(ClassSlot::paladin), ranger = level(ClassSlot::ranger);
    if (level(ClassSlot::cleric) == 0 && level(ClassSlot::magic_user) == 0 && paladin < 9 &&
        ranger < 8)
    {
        return 6;
    }
    const int paladin_less = game == GoldBoxGame::secret_of_the_silver_blades ? 0 : 8;
    switch (spell.caster_class)
    {
    case SpellClass::cleric:
        return std::max(level(ClassSlot::cleric), paladin - paladin_less);
    case SpellClass::druid:
        return std::max(ranger - 7, 0);
    default:
        return std::max(level(ClassSlot::magic_user), ranger - 8);
    }
}

// A spell's range in squares at a level: a touch is 1 (magic.pas).
int spell_range(const Spell &spell, int level)
{
    if (spell.range_fixed == 255)
    {
        return 1;
    }
    const int range = spell.range_fixed + spell.range_per_level * level;
    return range == 0 && spell.area != 0 ? 1 : range;
}

// The original's doubled distance between two squares along a line: 2 a
// straight step, 3 a diagonal one (pathing.pas, NextPathStep).
int doubled_distance(Cell a, Cell b)
{
    const int dx = std::abs(a.x - b.x), dy = std::abs(a.y - b.y);
    return 2 * std::max(dx, dy) + std::min(dx, dy);
}

// Curse's Fire Shield, whose caster chooses its flames as it goes off, and
// the choice: the AI's (and Quick's) at random (SpellFireShield).
constexpr int fire_shield = 185, delayed_blast_spell = 315;
enum class Flames : unsigned
{
    random,
    hot,
    cold
};

// The spells built so far; the rest are not offered yet.
bool underwater_spell(int number)
{
    static constexpr std::array blocked{9, 47, 174, 315, 318, 324};
    return std::ranges::find(blocked, number) == blocked.end();
}

bool spell_built(int number)
{
    static constexpr std::array built{
        26, 27, 36, 41, 46, 57, 58, 59, 60, 61, 62, 63, 64, 65, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
        12, 13, 15, 16, 17, 19, 20, 21, 23, 24, 25, 28, 29, 30, 32, 33, 35, 37, 38, 40, 42, 34, 43,
        44, 45, 47, 48, 49, 50, 51, 52, 53, 54, 55,
        // Curse's (spells.cpp, curse_spells).
        158, 166, 168, 169, 170, 171, 172, 173, 174, 176, 177, 178, 179, 180, 181, 182, 183, 184,
        185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200,
        native_elixir_youth,
        // Secret's (spells.cpp, secret_spells).
        236, 256, 290, 296, 298, 310, 311, 312, 313, 314, 315, 316, 317,
        // Pools of Darkness's that are cast in fights (spells.cpp, darkness_spells).
        301, 303, 304, 308, 309, 318, 319, 320, 321, 322, 323, 324, 325, 326, 501, 502, 503, 504,
        505, 506, 507};
    return std::find(built.begin(), built.end(), number) != built.end();
}

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

// The dice a run of a rule draws, served from a script of outcomes, so that
// each_way can try every way the run can go. The first die the script does
// not cover is noted, and any it does not cover falls as a 1 for a run whose
// result is set aside.
class ScriptedDice
{
  public:
    explicit ScriptedDice(const std::vector<int> &script) : script_(script)
    {
    }

    int operator()(int sides)
    {
        if (drawn_ < script_.size())
        {
            return script_[drawn_++];
        }
        if (unscripted_sides_ == 0)
        {
            unscripted_sides_ = sides;
        }
        ++drawn_;
        return 1;
    }

    // The sides of the first die the script did not cover, 0 for none.
    [[nodiscard]] int unscripted_sides() const noexcept
    {
        return unscripted_sides_;
    }

  private:
    const std::vector<int> &script_;
    std::size_t drawn_{};
    int unscripted_sides_{};
};

// Every way a run of a rule that draws dice can go, each with its chance:
// `run` (given the dice to draw from) is run again for each outcome of each
// die it draws, and `take` given each way's chance and the run's result.
// The run must change nothing but its dice, as the rule's const parts do.
template <typename Result, typename Run, typename Take> void each_way(Run &&run, Take &&take)
{
    std::vector<std::pair<std::vector<int>, double>> pending{{{}, 1.0}};
    while (!pending.empty())
    {
        auto [script, chance] = std::move(pending.back());
        pending.pop_back();
        ScriptedDice dice(script);
        const Result result = run(dice);
        const int sides = dice.unscripted_sides();
        if (sides == 0)
        {
            take(chance, result);
            continue;
        }
        for (int outcome = 1; outcome <= sides; ++outcome)
        {
            auto next = script;
            next.push_back(outcome);
            pending.emplace_back(std::move(next), chance / sides);
        }
    }
}

// The chance of each total of `count` dice of `sides`, plus `bonus`, by the total.
std::vector<double> dice_chances(int count, int sides, int bonus)
{
    std::vector<double> chances{1.0};
    for (int die = 0; die < count; ++die)
    {
        std::vector<double> next(chances.size() + static_cast<std::size_t>(sides), 0.0);
        for (std::size_t total = 0; total < chances.size(); ++total)
        {
            for (int face = 1; face <= sides; ++face)
            {
                next[total + static_cast<std::size_t>(face)] += chances[total] / sides;
            }
        }
        chances = std::move(next);
    }
    std::vector<double> shifted(static_cast<std::size_t>(std::max(bonus, 0)), 0.0);
    shifted.insert(shifted.end(), chances.begin(), chances.end());
    return shifted;
}

// The chance of each sum of two amounts that fall apart from each other.
std::vector<double> sum_chances(const std::vector<double> &first, const std::vector<double> &second)
{
    std::vector<double> sum(first.size() + second.size() - 1, 0.0);
    for (std::size_t a = 0; a < first.size(); ++a)
    {
        for (std::size_t b = 0; b < second.size(); ++b)
        {
            sum[a + b] += first[a] * second[b];
        }
    }
    return sum;
}

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
    [[nodiscard]] rules::MovementGrid grid_for(const Fighter &mover) const;
    void log(Message message);
    void begin_round();
    void age_the_hasted();
    void make_younger(Fighter &user);
    [[nodiscard]] std::vector<Held> held_now(const Fighter &f) const;
    [[nodiscard]] Combatant numbers_with(const Fighter &f, const std::vector<Held> &held,
                                         std::vector<int> &order) const;
    void rework_gear(Fighter &f, const std::vector<Held> &before_held,
                     std::vector<Held> after_held);
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
    [[nodiscard]] bool image_takes_it(const Effect &images, Dice &&dice) const
    {
        const int count = game_of(identity_) == GoldBoxGame::dark_queen_of_krynn
                              ? images.strength >> 5
                              : images.strength;
        return dice(count + 1) > 1;
    }

    [[nodiscard]] bool aura_protects(const Fighter &attacker, const Fighter &defender) const;
    bool add_with_message(Fighter &target, Effect added, bool negates, bool saved, int element,
                          int caster_level, const Spell *spell);
    template <typename Dice>
    [[nodiscard]] bool refuses_effect(const Fighter &target, int code, int element,
                                      int caster_level, const Spell *known, Dice &&dice) const;
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
                                         Dice &&dice) const;
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
    void bolt_walk(Cell origin, Cell aim, int length, bool bounce, Strike &&strike) const;
    template <typename Strike>
    void treasures_bolt_walk(Cell origin, Cell aim, int length, bool near_bounce_doubles,
                             Strike &&strike) const;
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
                                                          int budget, Most &&most_hit_points) const;
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
                                                        Most &&most_hit_points) const;
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
                                                      Most &&most_hit_points) const;
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
    template <typename Dice> [[nodiscard]] static int save_die(const Fighter &f, Dice &&dice)
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
    [[nodiscard]] Fighter *cure_search(const Fighter &f);
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

Fighter *Session::find(EntityId id)
{
    const auto found = std::find_if(fighters_.begin(), fighters_.end(),
                                    [id](const Fighter &f)
                                    {
                                        return f.id == id;
                                    });
    return found == fighters_.end() ? nullptr : &*found;
}

const Fighter *Session::find(EntityId id) const
{
    return const_cast<Session *>(this)->find(id);
}

void Session::log(Message message)
{
    log_.push_back({english(message), std::move(message)});
}

// A creature's movement in squares before Haste and Slow (cbtacts.pas,
// CalcCreatureMovement): the party's side moves by the script's modifier
// too, a byte that wraps, and a movement outside 1 to 96 is 1. Secret's
// boots of speed double it (charhlp.pas, RecalcMovement), as the kit's do
// (hero.c, recalc_stats).
int Session::squares_of_movement(const Fighter &f) const
{
    int squares = f.numbers.movement;
    if ((secret() && f.has(effect::boots_of_speed)) || f.has(effect::speed_boots))
    {
        squares *= 2;
    }
    if (f.side == Side::party)
    {
        squares = (squares + party_move_modifier_) % 256;
    }
    return squares < 1 || squares > 96 ? 1 : squares;
}

// The half-moves it has this round: twice its movement, doubled by Haste and
// halved by Slow. Pools of Darkness adds its script's modifier (cell 1Ch, a
// signed byte) to a party member's movement after Haste and Slow, and a sum
// below 1 is 1 (charhlp.pas, RecalcMovement).
int Session::half_moves_of(const Fighter &f) const
{
    if (darkness() && f.side == Side::party && party_move_modifier_ != 0)
    {
        const int modifier = static_cast<std::int8_t>(party_move_modifier_ & 0xFF);
        return 2 * std::max(paced(f, f.numbers.movement) + modifier, 1);
    }
    const int moves = paced(f, 2 * squares_of_movement(f));
    return underwater_ && !f.has(2150) ? moves / 3 : moves;
}

// Its movement in squares this round, as the original weighs flight and
// morale with it.
int Session::movement_of(const Fighter &f) const
{
    return half_moves_of(f) / 2;
}

// Every creature able to act rolls initiative: d6 with its Dexterity's
// missile adjustment, at least 1; a surprised one loses 6 in the first round,
// and a result outside 0 to 20 lets it not act (combat.pas, BeginRound).
void Session::begin_round()
{
    ++round_;
    if (buck_rogers())
    {
        ground_begin_round();
        return;
    }
    rules::CharacterSheet::Original calendar;
    if (game_of(identity_) == GoldBoxGame::death_knights_of_krynn)
    {
        const auto passed = static_cast<std::uint64_t>(round_ - 1);
        if (starting_time_minutes_ > std::numeric_limits<std::uint64_t>::max() - passed)
        {
            throw std::overflow_error("Combat calendar overflow");
        }
        death_knights_calendar(calendar,
                               death_knights_calendar_day(starting_time_minutes_ + passed));
    }
    for (auto &f : fighters_)
    {
        if (game_of(identity_) == GoldBoxGame::death_knights_of_krynn)
        {
            if (auto *moon = f.find_effect(dkk_effect::casting_moon))
            {
                const int robe = moon->strength / 4;
                if (robe >= 1 && robe <= 3)
                {
                    moon->strength =
                        robe * 4 + calendar.krynn_moon_phases[static_cast<std::size_t>(robe - 1)];
                }
            }
        }
    }
    for (auto &f : fighters_)
    {
        f.initiative = 0;
        if (!f.able())
        {
            continue;
        }
        f.initiative = std::max(roll(6) + f.numbers.initiative_bonus, 1);
        if (round_ == 1 && f.surprised)
        {
            f.initiative -= 6;
            if (f.initiative < 0 || f.initiative > 20)
            {
                f.initiative = 0;
            }
        }
        // A Treasures vilstrak moves twice as far in the first round
        // (its 146, at condition 18).
        f.half_moves = f.has(effect::immobile)                     ? 0
                       : f.has(effect::quick_start) && round_ <= 1 ? 2 * half_moves_of(f)
                                                                   : half_moves_of(f);
        f.cast_allowed = true;
        f.item_use_allowed = true;
        f.using_item = -1;
        f.pending_spell = f.aiming = f.picks_left = 0;
        f.picked.clear();
        f.attack_phase = 1;
        f.sweep_budget = f.numbers.attack_level;
        for (std::size_t form = 0; form < f.attacks_left.size(); ++form)
        {
            f.attacks_left[form] = attacks_this_round(
                paced(f, f.numbers.attacks[form].rate_per_two_rounds), static_cast<int>(round_));
        }
        // A loaded missile weapon attacks at its own rate, no more often than
        // it has shots (cbtacts.pas, RecalcAttacksLeft).
        if (const auto &missile = f.numbers.missile; missile && missile->loaded)
        {
            f.attacks_left[0] = attacks_this_round(paced(f, missile->rate_per_two_rounds),
                                                   static_cast<int>(round_));
            if (!missile->needs_no_ammunition)
            {
                f.attacks_left[0] = std::min(f.attacks_left[0], std::max(missile->quantity, 1));
            }
        }
    }
    age_the_hasted();
    if (ordered_rounds())
    {
        order_turns();
    }
}

// Secret's order for the round: each creature in turn goes before the first
// already placed whose initiative is lower, or the same on a d2 of 1
// (combat.pas, OrderCreatures, in its reconstruction).
void Session::order_turns()
{
    turn_order_.clear();
    for (const auto &f : fighters_)
    {
        auto place = turn_order_.begin();
        while (place != turn_order_.end())
        {
            const int other = find(*place)->initiative;
            if (f.initiative > other || (f.initiative == other && roll(2) == 1))
            {
                break;
            }
            ++place;
        }
        turn_order_.insert(place, f.id);
    }
}

// A Secret caster who begins a spell moves back among those yet to act, past
// each whose initiative is higher than its own, or the same on a d2 of 1
// (cbtacts.pas, CastSpellCombat, in its reconstruction).
void Session::reorder_after_cast(Fighter &caster)
{
    auto place = turn_order_.begin();
    while (place != turn_order_.end())
    {
        const int other = find(*place)->initiative;
        if (other < caster.initiative || (other == caster.initiative && roll(2) != 1))
        {
            break;
        }
        ++place;
    }
    turn_order_.insert(place, caster.id);
}

// The highest initiative left acts next; a tie goes to the higher of fresh
// d100 rolls (combat.pas, NextCreature). In Secret the next in the round's
// order still able to act does. With none left the round ends.
void Session::next_actor()
{
    actor_ = 0;
    while (outcome_ == rules::Outcome::ongoing)
    {
        Fighter *best = nullptr;
        int best_tiebreak = 0;
        while (ordered_rounds() && !best && !turn_order_.empty())
        {
            auto *first = find(turn_order_.front());
            turn_order_.erase(turn_order_.begin());
            if (first->able() && first->initiative > 0)
            {
                best = first;
            }
        }
        for (auto &f : fighters_)
        {
            // Secret's, Darkness's and the kit's order has picked already.
            if (ordered_rounds() || !f.able() || f.initiative <= 0)
            {
                continue;
            }
            if (!best || f.initiative > best->initiative)
            {
                best = &f;
                best_tiebreak = roll(100);
            }
            else if (f.initiative == best->initiative)
            {
                const auto tiebreak = roll(100);
                // A full tie goes to the later creature (combat.pas, NextCreature).
                if (tiebreak >= best_tiebreak)
                {
                    best = &f;
                    best_tiebreak = tiebreak;
                }
            }
        }
        if (best)
        {
            // A turn starts afresh: no flanking counted and no guard kept
            // (combat.pas, the creature's turn).
            best->flank_count = best->flank_spread = 0;
            best->guarding = false;
            if (best->ground)
            {
                if (!ground_turn_starts(*best) || check_morale(*best))
                {
                    continue;
                }
                actor_ = best->id;
                return;
            }
            // A helpless creature's turn ends as it starts (effhndlr.pas, the
            // helpless effects at TURN_START).
            // So does a stunned one's in Secret (Power Word Stun's 106, EffHelpless
            // at its ACTION_START), though it is not helpless.
            // And a dancing one's in Pools of Darkness (Otto's Irresistible
            // Dance's lost turn, EffClearGuard at TURN_START).
            if (best->helpless() || best->has(effect::stunned) || best->has(effect::dancing))
            {
                end_actions(*best);
                continue;
            }
            // A holding bite or a drinking stirge spends its turn on its victim
            // (effhndlr.pas, ACTION_START).
            attached_turn(*best);
            curse_turn_start(*best);
            kit_turn_start(*best);
            champions_turn_start(*best);
            if (best->initiative <= 0 || !best->able() || check_morale(*best))
            {
                continue;
            }
            // Silence within a square stops its casting for the round
            // (effhndlr.pas, EffSilence at ACTION_START).
            if (silenced(*best) && best->cast_allowed)
            {
                best->cast_allowed = false;
                best->item_use_allowed = false;
                log({"{name} is silenced.", {{"name", best->name}}});
            }
            // Coughing stops casting too (effhndlr.pas, EffCoughing).
            if (best->has(effect::coughing) && best->cast_allowed)
            {
                best->cast_allowed = false;
                best->item_use_allowed = false;
                log({"{name} is coughing.", {{"name", best->name}}});
            }
            // A spell begun earlier in the round goes off now (combat.pas,
            // ManualControl): its targets are chosen as it does.
            if (best->pending_spell)
            {
                const int pending = best->pending_spell;
                best->pending_spell = 0;
                start_aiming(*best, pending);
                if (best->initiative <= 0)
                {
                    continue;
                }
            }
            actor_ = best->id;
            return;
        }
        end_round();
    }
}

// The round ends: each dying creature bleeds a point and dies past 9 below 0
// (combat.pas, EndOfRound).
void Session::end_round()
{
    age_effects();
    settle_hammers();
    update_hostile_hit_points();
    round_end_effects();
    for (auto &f : fighters_)
    {
        breathe_cloud(f, true);
    }
    if (buck_rogers())
    {
        ground_end_round();
    }
    for (auto &f : fighters_)
    {
        if (!f.ground && f.condition == Condition::dying && ++f.overkill > 9)
        {
            f.condition = Condition::dead;
            log({"{name} dies of its wounds.", {{"name", f.name}}});
        }
    }
    settle_outcome();
    if (outcome_ == rules::Outcome::ongoing && has_round_limit() && round_ >= fight_end_round_)
    {
        end_quiet_fight();
    }
    if (outcome_ == rules::Outcome::ongoing)
    {
        join_arrivals();
        begin_round();
    }
}

// Those held back whose round has come join the fight where they would have
// stood, or on the free square nearest it (combat.pas,
// JoinDelayedCombatants; combatst.pas, JoinBattle). One with no room stays
// away.
void Session::join_arrivals()
{
    for (auto &f : fighters_)
    {
        if (f.arrival_round == 0 || static_cast<unsigned>(f.arrival_round) > round_)
        {
            continue;
        }
        f.arrival_round = 0;
        if (place_near(f, f.cell))
        {
            f.condition = Condition::okay;
            log({"{name} joins the battle.", {{"name", f.name}}});
        }
    }
}

// Puts the fighter on the open, free square nearest the wanted one, ring by
// ring; false, leaving it where it was, when there is none.
bool Session::place_near(Fighter &f, Cell wanted)
{
    // The squares its enemies can walk to: it is not shut away from them
    // (the original places it as the fight places its side, which never
    // leaves it walled in).
    const auto at = [this](Cell c)
    {
        return static_cast<std::size_t>(c.y * board_.width + c.x);
    };
    std::vector<bool> reached(static_cast<std::size_t>(board_.width * board_.height));
    std::vector<Cell> frontier;
    for (const auto &o : fighters_)
    {
        if (o.side != f.side && o.able() && board_.contains(o.cell))
        {
            reached[at(o.cell)] = true;
            frontier.push_back(o.cell);
        }
    }
    const bool enemies_stand = !frontier.empty();
    for (std::size_t n = 0; n < frontier.size(); ++n)
    {
        for (int dy = -1; dy <= 1; ++dy)
        {
            for (int dx = -1; dx <= 1; ++dx)
            {
                const Cell next{frontier[n].x + dx, frontier[n].y + dy};
                if (board_.contains(next) && !reached[at(next)] &&
                    !rules::blocks_movement(board_.at(next)))
                {
                    reached[at(next)] = true;
                    frontier.push_back(next);
                }
            }
        }
    }
    const auto fits = [&]
    {
        const auto squares = squares_of(f);
        return cell_free(f) &&
               std::all_of(squares.begin(), squares.end(),
                           [this](Cell c)
                           {
                               return board_.contains(c) && !rules::blocks_movement(board_.at(c));
                           }) &&
               (!enemies_stand || reached[at(f.cell)]);
    };
    const int reach = std::max(board_.width, board_.height);
    for (int ring = 0; ring <= reach; ++ring)
    {
        for (int dy = -ring; dy <= ring; ++dy)
        {
            for (int dx = -ring; dx <= ring; ++dx)
            {
                if (std::max(std::abs(dx), std::abs(dy)) != ring)
                {
                    continue;
                }
                f.cell = {wanted.x + dx, wanted.y + dy};
                if (fits())
                {
                    return true;
                }
            }
        }
    }
    f.cell = wanted;
    return false;
}

// Its actions end: no initiative, guard or movement left, though the attacks
// it has not made stay for a guard's strike or a free attack (charhlp.pas, EndActions).
void Session::end_actions(Fighter &fighter)
{
    fighter.initiative = 0;
    fighter.pending_spell = 0;
    fighter.guarding = false;
    fighter.half_moves = 0;
}

// Counts an attack on the defender for flanking: one more attacker, and the
// angle between its facing and the attack (0 to 4) added to its spread, mod 8
// (cbtacts.pas, NoteAttackDirection).
void Session::note_attack(const Fighter &attacker, Fighter &defender)
{
    ++defender.flank_count;
    int angle = (direction_toward(defender.cell, attacker.cell) - defender.direction + 8) % 8;
    if (angle > 4)
    {
        angle = 8 - angle;
    }
    defender.flank_spread = (defender.flank_spread + angle) % 8;
}

// An attack (cbtacts.pas, MakeAttack and RollAttacks). The defender turns to
// face the attacker unless it is struck from behind or already attacked three
// times. A thief backstabs a defender attacked more than once from the side it
// faces away from, against its Armor Class from behind less 2, for its level
// div 4 + 2 times the damage; others strike from behind there once the attacks
// on it have spread wide. Attacks run from the attacker's current form down to
// the first until the defender goes down; with none left its actions end.
void Session::make_attack(Fighter &attacker, Fighter &defender, bool from_behind)
{
    // The original counts its rounds from 0, so this round's is one less.
    fight_end_round_ = round_ - 1 + rounds_without_attacks;
    // Buck Rogers attacks are never made as from behind: the rear is found
    // from the defender's facing (cbtacts.pas RollAttacks).
    if (attacker.ground)
    {
        ground_attack(attacker, defender);
        return;
    }
    if (defender.flank_count < 3 && !from_behind)
    {
        defender.direction = direction_toward(defender.cell, attacker.cell);
    }
    attacker.direction = direction_toward(attacker.cell, defender.cell);
    attacker.lock = defender.id;
    attacker.facing_left = defender.cell.x < attacker.cell.x;
    const bool behind_its_back =
        defender.flank_count > 1 &&
        direction_toward(attacker.cell, defender.cell) == defender.direction;
    // Secret's thief backstabs only a defender of one square next to it
    // (cbtacts.pas, IsBackstab, in its reconstruction).
    const bool one_square = defender.footprint.width == 1 && defender.footprint.height == 1;
    const bool backstab = attacker.numbers.backstabs && behind_its_back &&
                          (!secret_engine() || (one_square && adjacent(attacker, defender)));
    const bool behind = from_behind || (behind_its_back && defender.flank_spread > 4);
    // Shield keeps the front Armor Class at 3 or better (effhndlr.pas, EffShield).
    const int front = defender.has(effect::shield) ? std::min(defender.numbers.armor_class, 3)
                                                   : defender.numbers.armor_class;
    // Blindness leaves both Armor Classes 4 worse (effhndlr.pas, EffBlind);
    // coughing leaves the rear one 2 worse, no worse than 10, and the front as
    // bad (EffCoughing).
    // Faerie Fire leaves both 2 worse (Curse made them better, docs/ORIGINAL-RULES.md, Bugs).
    // Pools of Darkness's dance leaves both 4 worse and Iron Skin 4 better
    // (effhndlr.pas, EffDancing, EffIronSkin).
    const int blind = (defender.has(effect::blind) ? 4 : 0) + (defender.has(207) ? 2 : 0) +
                      (defender.has(effect::dancing) ? 4 : 0) -
                      (defender.has(effect::iron_skin) ? 4 : 0);
    const bool coughs = defender.has(effect::coughing);
    const int rear = coughs ? std::max(std::min(defender.numbers.rear_armor_class + 2, 10),
                                       defender.numbers.rear_armor_class)
                            : defender.numbers.rear_armor_class;
    // Secret takes 4 from the Armor Class from behind for a backstab (RollAttacks).
    const int backstab_armor_class = rear + (secret_engine() ? 4 : 2);
    const int armor_class = (backstab ? backstab_armor_class
                             : behind ? rear
                             : coughs ? rear
                                      : front) +
                            blind;
    if (defender.helpless())
    {
        slay_helpless(attacker, defender);
        return;
    }
    if (backstab)
    {
        log({"{attacker} backstabs {name}.",
             {{"attacker", attacker.name}, {"name", defender.name}}});
    }
    int primary_hits = 0;
    // The first attacks made at this defender, hit or miss (cbtacts.pas,
    // RollAttacks' attackCounts), which pick Kalistes' weapon.
    int primary_attacks = 0;
    for (int phase = attacker.attack_phase; phase >= 0 && defender.able(); --phase)
    {
        auto &left = attacker.attacks_left[static_cast<std::size_t>(phase)];
        auto attack = attacker.numbers.attacks[static_cast<std::size_t>(phase)];
        if (phase == 0 && defender.numbers.large && attacker.numbers.large_attack)
        {
            attack = *attacker.numbers.large_attack;
        }
        // Each attack is spent before it is rolled, so that what a hit carries
        // (a rake) may take over the rest.
        while (left > 0 && defender.able())
        {
            --left;
            attacker.attack_phase = phase;
            primary_attacks += phase == 0 ? 1 : 0;
            const auto natural = roll(20);
            // Attacking makes it visible (fxengine.pas, AttackRoll).
            while (attacker.has(effect::invisible))
            {
                attacker.remove(effect::invisible);
            }
            const int modifier = attack_modifier(attacker, defender, game_of(identity_)) +
                                 prayer_bonus(attacker) + love_modifier(attacker, defender) -
                                 (aura_protects(attacker, defender) ? 2 : 0);
            // One made helpless partway through is hit by the rest (cbtacts.pas, RollAttacks).
            const bool aside = natural > 1 && turned_aside(defender);
            if ((aside || !attack_hits(natural, attacker.numbers.thac0, armor_class, modifier)) &&
                !defender.helpless())
            {
                log({"{attacker} misses {name}.",
                     {{"attacker", attacker.name}, {"name", defender.name}}});
                continue;
            }
            // A Secret creature that swallows whole does so on a natural 20,
            // before the blow lands (EffSwallowWhole, at its damage).
            if (attacker.has(effect::swallows_whole) && natural == 20)
            {
                swallow_whole(attacker, defender);
                continue;
            }
            int dice = 0;
            for (int die = 0; die < attack.dice; ++die)
            {
                dice += roll(std::max(attack.sides, 1));
            }
            auto damage = hit_damage(dice, attack.modifier);
            // A giant-bane weapon's blow is 6d6 and the Strength bonus in
            // place of the weapon's own (EffHolyWater, at its damage).
            if (giant_bane_strikes(attacker, defender, game_of(identity_)))
            {
                damage = roll(6) + roll(6) + roll(6) + roll(6) + roll(6) + roll(6) +
                         strength_damage_bonus(attacker.numbers.strength,
                                               attacker.numbers.exceptional_strength);
            }
            // Secret multiplies a backstab after the effects on the blow, so the
            // multiple covers a ranger's bonus too (CalcWeaponDamage), with
            // (level - 1) div 4 + 2; Pools of Darkness by 5 at most.
            if (backstab && !secret_engine())
            {
                damage *= attacker.numbers.thief_level / 4 + 2;
            }
            damage = weapon_damage_defenses(attacker, defender, damage, phase, primary_attacks,
                                            primary_hits);
            if (backstab && secret_engine())
            {
                const int multiple = (attacker.numbers.thief_level - 1) / 4 + 2;
                damage *= darkness() ? std::min(multiple, 5) : multiple;
            }
            log({"{attacker} hits {name} for {damage}.",
                 {{"attacker", attacker.name},
                  {"name", defender.name},
                  {"damage", std::to_string(damage)}}});
            primary_hits += phase == 0 ? 1 : 0;
            if (attacker.has(effect::paralyzing_strike))
            {
                damage = 0;
            }
            striker_ = attacker.id;
            inflict(defender, damage);
            striker_ = 0;
            // In Secret only a blow that did damage carries anything (RollAttacks).
            if (defender.able() && (damage > 0 || !secret_engine()))
            {
                // The original's attack roll, its bonuses in, a natural
                // 20 counting as 100 (cbtacts.pas, RollAttacks).
                on_hit(attacker, defender, phase, primary_hits, natural,
                       natural == 20 ? 100 : natural + modifier);
            }
            // Fire Touch burns what its bearer hits for 2d6, halved by a save
            // (effhndlr.pas, EffFireTouch); Pools of Darkness's fire with a hit
            // for 4d6 (EffFireDmg4d6).
            if (const auto *touch = attacker.find_effect(effect::fire_touch);
                touch && defender.able())
            {
                const bool saved = saving_throw(defender, 4, 0, &attacker, 9);
                nonweapon_damage(defender, roll(6) + roll(6), 2, saved, 9, touch->strength, 318,
                                 false);
            }
            if (attacker.has(effect::fire_hit) && defender.able())
            {
                const bool saved = saving_throw(defender, 4, 0, &attacker, 9);
                const int fire = roll(6) + roll(6) + roll(6) + roll(6);
                nonweapon_damage(defender, fire, 2, saved, 9, attacker.numbers.level, 0, false);
            }

            // Native MakeAttack stops the remaining blows of a disabled
            // attacker. The triggering hit still lands after retaliation;
            // neither this form nor a later one can attack from death.
            if (game_of(identity_) == GoldBoxGame::dark_queen_of_krynn && !attacker.able())
            {
                attacker.attacks_left = {};
                break;
            }
        }
    }
    attacker.sweep_budget = 0;
    if (attacker.attacks() == 0)
    {
        end_actions(attacker);
    }
}

// What an attacker's and a defender's effects do to its attack roll
// (fxengine.pas, AttackRoll): Bless +1, Curse -1, an invisible defender 4
// harder, and a protected one 2 harder for an attacker of the alignment it is
// protected from. The original lost that 2 to a roll made after it (an
// obvious bug, fixed).
int attack_modifier(const Fighter &attacker, const Fighter &defender, GoldBoxGame game)
{
    int modifier = (attacker.has(effect::bless) ? 1 : 0) - (attacker.has(effect::curse) ? 1 : 0);
    modifier += attacker.has(effect::aurak_combat_bonus) ? 2 : 0;
    modifier -= defender.has(effect::aurak_combat_bonus) ? 2 : 0;
    // Blindness and Bestow Curse take 4 from the attacker's rolls (effhndlr.pas, 33, 36).
    modifier -= attacker.has(effect::blind) ? 4 : 0;
    modifier -= attacker.has(dkk_effect::smoked) ? 2 : 0;
    if (defender.has(dkk_effect::dispel_evil_ward) &&
        (attacker.numbers.traits & trait::dispel_evil))
    {
        modifier -= 7;
    }
    modifier -= attacker.has(effect::bestow_curse) ? 4 : 0;
    // The kit's 148, which a ghast's stench gives, takes 2 (effects.c); so
    // does Pools of Darkness's dragon fear (effhndlr.pas, EffDragonFeared).
    modifier -= attacker.has(effect::sickened) ? 2 : 0;
    modifier -= attacker.has(dq_effect::venomed) ? 1 : 0;
    modifier -= attacker.has(effect::dragon_feared) ? 2 : 0;
    modifier -= attacker.has(effect::champions_sickened) ? 2 : 0;
    modifier -= attacker.has(effect::champions_afraid) ? 1 : 0;
    if (defender.has(effect::invisible) || defender.has(effect::dust))
    {
        modifier -= 4;
    }
    // A dwarf or gnome strikes its foes 1 better, and giants, bugbears and
    // gnolls strike it 4 worse (effhndlr.pas, 18, 26, 47, 48). The original
    // judged the giant by the defender's own target instead of the attacker,
    // reading eight names from its list of five (obvious bugs, fixed).
    const int foe = defender.numbers.kin, kind = attacker.numbers.kin;
    modifier += attacker.has(26) && (foe & kin::dwarf_foe) ? 1 : 0;
    modifier += attacker.has(18) && (foe & kin::gnome_foe) ? 1 : 0;
    modifier -= defender.has(47) && (kind & kin::giant) ? 4 : 0;
    modifier -= defender.has(48) && (kind & kin::bugbear) ? 4 : 0;
    const int alignment = attacker.numbers.alignment;
    const bool evil = alignment == 2 || alignment == 5 || alignment == 8;
    const bool good = alignment == 0 || alignment == 3 || alignment == 6;
    if ((evil && defender.has(effect::protection_from_evil)) ||
        (good && defender.has(effect::protection_from_good)))
    {
        modifier -= 2;
    }
    // Dispel Evil keeps the evil 7 worse off at its bearer (Curse, DispelEvil).
    modifier -= dispel_evil_takes(attacker, game) && defender.has(204) ? 7 : 0;
    // A giant-bane weapon strikes its quarry 1 better. The kit added the 1
    // after the roll, where it changed nothing (an obvious bug, fixed;
    // fxcombat.c, fx_damage_vs_immunity1).
    modifier += giant_bane_strikes(attacker, defender, game) ? 1 : 0;
    modifier +=
        attacker.has(effect::dragon_slayer) && (defender.numbers.traits & champions_dragon_trait)
            ? 2
            : 0;
    return modifier;
}

// The attack the attacker can make on the target, if any (cbtacts.pas,
// AimTargetMenu): melee next to it, without a missile weapon or with one it
// can throw; otherwise a loaded missile weapon shoots a target it sees within
// range, unless an enemy stands next to it and the weapon is not throwable.
std::optional<std::string> Session::attack_verb(const Fighter &attacker,
                                                const Fighter &target) const
{
    if (!can_target(attacker, target))
    {
        return std::nullopt;
    }
    const auto &missile = attacker.numbers.missile;
    const bool next_to = adjacent(attacker, target);
    if (!missile)
    {
        return next_to ? std::optional<std::string>("melee") : std::nullopt;
    }
    if (!missile->loaded || (!missile->throwable && enemy_next_to(attacker)))
    {
        return std::nullopt;
    }
    if (next_to)
    {
        return "melee";
    }
    if (underwater_ || distance(attacker, target) > missile->range ||
        !sees(board_, attacker, target))
    {
        return std::nullopt;
    }
    return missile->thrown ? "throw" : "ranged";
}

bool Session::enemy_next_to(const Fighter &f) const
{
    return std::any_of(fighters_.begin(), fighters_.end(),
                       [&f](const Fighter &other)
                       {
                           return other.side != f.side && other.able() && adjacent(f, other);
                       });
}

// A missile attack, which spends a shot for each attack made: a thrown
// weapon's last throw leaves its thrower empty-handed, and a launcher's last
// arrow or quarrel leaves it nothing to shoot (cbtacts.pas, MakeAttack).
void Session::shoot(Fighter &attacker, Fighter &target)
{
    // A Buck Rogers weapon spends its own rounds (ground_attack).
    if (attacker.ground)
    {
        make_attack(attacker, target, false);
        return;
    }
    const int before = attacker.attacks_left[0];
    make_attack(attacker, target, false);
    auto &missile = *attacker.numbers.missile;
    if (missile.needs_no_ammunition)
    {
        return;
    }
    if (missile.quantity > 0)
    {
        missile.quantity = std::max(missile.quantity - (before - attacker.attacks_left[0]), 0);
    }
    if (missile.quantity > 0)
    {
        return;
    }
    if (!missile.thrown)
    {
        missile.loaded = false;
        return;
    }
    auto &numbers = attacker.numbers;
    numbers.thac0 = missile.empty_handed_thac0;
    numbers.attacks[0] = missile.empty_handed_attack;
    numbers.backstabs = missile.empty_handed_backstabs;
    numbers.large_attack.reset();
    numbers.missile.reset();
    numbers.striker.reset();
    numbers.weapon_type = -1;
}

// A fighter sweeps through level-0 enemies: when the defender is one, next to
// it, and more of them stand around it than it has attacks this round, it
// strikes each once, the defender first, up to its attack level (cbtacts.pas,
// SweepAttack).
bool Session::sweep(Fighter &attacker, Fighter &defender)
{
    if (attacker.attacks_left[0] >= attacker.sweep_budget || defender.numbers.level != 0 ||
        !adjacent(attacker, defender))
    {
        return false;
    }
    std::vector<Fighter *> weak{&defender};
    for (auto &f : fighters_)
    {
        if (&f != &defender && f.side != attacker.side && f.able() && f.numbers.level == 0 &&
            adjacent(attacker, f))
        {
            weak.push_back(&f);
        }
    }
    if (static_cast<int>(weak.size()) <= attacker.attacks_left[0])
    {
        return false;
    }
    weak.resize(
        std::min<std::size_t>(weak.size(), static_cast<std::size_t>(attacker.sweep_budget)));
    log({"{attacker} sweeps.", {{"attacker", attacker.name}}});
    for (auto *target : weak)
    {
        note_attack(attacker, *target);
        attacker.attacks_left = {1, 0};
        attacker.attack_phase = 0;
        make_attack(attacker, *target, false);
    }
    end_actions(attacker);
    return true;
}

// Each enemy next to the mover now but not after its step strikes once, as
// from behind, while the mover stays up: unless it no longer acts this round,
// has already been attacked and has the mover out of the five directions in
// front of it (cbtacts.pas, AttacksOfOpportunity).
void Session::free_attacks(Fighter &mover, Cell next)
{
    auto stepped = mover;
    stepped.cell = next;
    for (auto &enemy : fighters_)
    {
        if (!mover.able() || enemy.side == mover.side || !enemy.able() || enemy.helpless() ||
            enemy.has(effect::bite_hold) || enemy.has(effect::blood_drain) ||
            !can_target(enemy, mover) || !adjacent(enemy, mover) || adjacent(enemy, stepped))
        {
            continue;
        }
        bool facing = false;
        for (int scan = enemy.direction + 6; scan <= enemy.direction + 10; ++scan)
        {
            facing = facing || leads_toward(enemy.cell, mover.cell, scan % 8);
        }
        if (enemy.initiative <= 0 && enemy.flank_count > 0 && !facing)
        {
            continue;
        }
        int phase = enemy.numbers.attacks[0].rate_per_two_rounds == 0 ? 1 : 0;
        for (int form = 0; form < 2; ++form)
        {
            if (enemy.attacks_left[static_cast<std::size_t>(form)] > 0)
            {
                phase = form;
            }
        }
        auto &left = enemy.attacks_left[static_cast<std::size_t>(phase)];
        left = std::max(left, 1);
        enemy.attack_phase = phase;
        // Native's final Aurak forms (its raw 210 and 213, both carrying 165)
        // strike no one leaving, after their attack is readied (cbaction.c).
        if (game_of(identity_) == GoldBoxGame::dark_queen_of_krynn &&
            enemy.has(effect::aurak_invulnerable))
        {
            continue;
        }
        log({"{attacker} strikes {name} leaving.",
             {{"attacker", enemy.name}, {"name", mover.name}}});
        make_attack(enemy, mover, true);
    }
}

// A guarding enemy the mover comes next to strikes it once and stops guarding
// (cbtacts.pas, GuardersAttack).
void Session::guards_strike(Fighter &mover)
{
    for (auto &guard : fighters_)
    {
        if (!mover.able() || !guard.guarding || guard.side == mover.side || !guard.able() ||
            guard.helpless() || !adjacent(guard, mover))
        {
            continue;
        }
        guard.guarding = false;
        if (guard.attacks() == 0)
        {
            continue;
        }
        note_attack(guard, mover);
        log({"{attacker} strikes from guard.", {{"attacker", guard.name}}});
        make_attack(guard, mover, false);
    }
}

// One step: the enemies left behind strike first, then the mover pays 2
// half-moves, or 3 on a diagonal, times the ground's cost, its flanking counts
// clear, and guards it comes next to strike (cbtacts.pas, CombatMoveCreature).
void Session::step_to(Fighter &mover, Cell next)
{
    // A Buck Rogers creature leaving an enemy is not struck for it; guards
    // strike it as it steps (cbtacts.pas CombatMoveCreature).
    if (mover.ground)
    {
        const auto price = grid_for(mover).step_price(mover.cell, next);
        if (!price || price->need > mover.half_moves)
        {
            mover.half_moves = 0;
            return;
        }
        mover.half_moves -= price->charge;
        mover.direction = direction_toward(mover.cell, next);
        mover.facing_left = next.x < mover.cell.x || (next.x == mover.cell.x && mover.facing_left);
        mover.cell = next;
        mover.flank_count = mover.flank_spread = 0;
        ground_after_step(mover);
        return;
    }
    free_attacks(mover, next);
    if (!mover.able())
    {
        mover.half_moves = 0;
        return;
    }
    const auto price = grid_for(mover).step_price(mover.cell, next);
    if (!price || price->need > mover.half_moves)
    {
        mover.half_moves = 0;
        return;
    }
    mover.half_moves -= price->charge;
    mover.direction = direction_toward(mover.cell, next);
    mover.facing_left = next.x < mover.cell.x || (next.x == mover.cell.x && mover.facing_left);
    mover.cell = next;
    mover.flank_count = mover.flank_spread = 0;
    breathe_cloud(mover);
    guards_strike(mover);
    if (!mover.able())
    {
        mover.half_moves = 0;
    }
}

// Bandaging saves a dying party member next to the one bandaging (the user,
// 2026-11-12; the original reached any on the field).
bool Session::can_bandage(const Fighter &bandager, const Fighter &dying) const
{
    return bandager.side == Side::party && dying.side == Side::party &&
           dying.condition == Condition::dying && adjacent(bandager, dying);
}

// Exactly 0 Hit Points leaves a creature unconscious, below 0 dying, and
// more than 9 below 0 dead (charhlp.pas, InflictDamage).
void Session::inflict(Fighter &target, int damage)
{
    if (target.ground)
    {
        ground_inflict(target, damage);
        return;
    }
    if (target.has(effect::aurak_invulnerable))
    {
        return;
    }
    // Any damage stops casting this round and loses a spell being cast
    // (cbtacts.pas, ProcessAttackResult).
    if (damage > 0)
    {
        target.cast_allowed = false;
        if (target.pending_spell || target.aiming)
        {
            lose_spell(target);
        }
    }
    if (target.hit_points >= damage)
    {
        target.hit_points -= damage;
        if (target.hit_points > 0)
        {
            return;
        }
        // The animated dead die at exactly 0 (charhlp.pas, InflictDamage).
        target.condition = target.has(effect::animated) ? Condition::dead : Condition::unconscious;
        log({target.condition == Condition::dead ? "{name} is killed."
                                                 : "{name} falls unconscious.",
             {{"name", target.name}}});
    }
    else
    {
        target.overkill = damage - target.hit_points;
        target.hit_points = 0;
        target.condition = target.overkill > 9 ? Condition::dead : Condition::dying;
        log({target.condition == Condition::dead ? "{name} is killed." : "{name} is dying.",
             {{"name", target.name}}});
    }
    target.initiative = 0;
    end_effects_at_zero(target);
    went_down(target);
}

// It loses the effects that end at 0 Hit Points; a hold or a drain it had
// lets its victim go (fxengine.pas, RemoveEffectsHp0).
void Session::end_effects_at_zero(Fighter &f)
{
    for (const auto code : effects_ending_at_zero)
    {
        if (code == effect::bite_hold || code == effect::blood_drain)
        {
            release(f, code);
            release(f, effect::engulfing);
            release(f, effect::hugging);
        }
        else if ((code == effect::charm || code == effect::animated) && f.has(code))
        {
            const auto *node = f.find_effect(code);
            remove_with_handler(f, static_cast<std::size_t>(node - f.numbers.effects.data()));
        }
        else
        {
            f.remove(code);
        }
    }
    // Its effects at 0 Hit Points then play (fxengine.pas, condition
    // ZERO_HIT_POINTS); one gone off the field has none.
    if (f.condition != Condition::fled && f.condition != Condition::surrendered)
    {
        give_up_crystal(f);
        love_at_fall(f);
    }
}

// In a crystal battle the first creature carrying a crystal to fall gives
// it to the party, and every other one still fighting smashes its own, so
// that one battle yields one crystal (effhndlr.pas, EffRedCrystal and
// ShowCrystalSmash; the cluebook's Crystals).
void Session::give_up_crystal(Fighter &f)
{
    if (!crystals_ || crystal_)
    {
        return;
    }
    const auto carried = [](const Fighter &bearer) -> std::optional<unsigned>
    {
        for (unsigned colour = 0; colour < 3; ++colour)
        {
            if (bearer.has(effect::red_crystal + static_cast<int>(colour)))
            {
                return colour;
            }
        }
        return std::nullopt;
    };
    const auto colour = carried(f);
    if (!colour)
    {
        return;
    }
    crystal_ = colour;
    f.remove(effect::red_crystal + static_cast<int>(*colour));
    for (const auto &other : fighters_)
    {
        if (other.able() && carried(other))
        {
            log({"{name} smashes a crystal.", {{"name", other.name}}});
        }
    }
}

void Session::pair_lovers(std::optional<rules::Romance> romance)
{
    romance_ = std::move(romance);
    lover_ = beloved_ = 0;
    if (!romance_)
    {
        return;
    }
    const auto named = [](std::string_view name, std::string_view wanted)
    {
        return !wanted.empty() && std::ranges::equal(name, wanted,
                                                     [](unsigned char a, unsigned char b)
                                                     {
                                                         return std::toupper(a) == std::toupper(b);
                                                     });
    };
    for (const auto &f : fighters_)
    {
        if (named(f.name, romance_->npc))
        {
            beloved_ = f.id;
        }
        if (named(f.name, romance_->member))
        {
            lover_ = f.id;
        }
    }
    for (auto &f : fighters_)
    {
        const bool paired = f.id == lover_ || f.id == beloved_;
        if (paired && !f.has(effect::in_love))
        {
            f.numbers.effects.push_back({effect::in_love, 0, 0, false});
        }
        else if (!paired)
        {
            f.remove(effect::in_love);
        }
    }
}

// A lover falls (EffInLove at ZERO_HIT_POINTS): the other, still fighting
// and loving it 100 or more, goes into a frenzy, the computer playing it to
// the fight's end. A foe that falls to a lover (EffInLove's condition 25,
// for the one whose weapon felled it or who acts) moves the other's love: 2
// up for an evil foe, to 255 at most, 20 down for a good one, to 0 at least.
void Session::love_at_fall(Fighter &fallen)
{
    if (!romance_)
    {
        return;
    }
    const auto other_of = [this](const Fighter &one) -> Fighter *
    {
        const EntityId other = one.id == beloved_ ? lover_ : beloved_;
        return other ? find(other) : nullptr;
    };
    // The love of `one` for the other.
    const auto love_of = [this](const Fighter &one) -> int &
    {
        return one.id == beloved_ ? romance_->npc_love : romance_->member_love;
    };
    if (fallen.has(effect::in_love))
    {
        if (auto *ally = other_of(fallen); ally && ally->able() && love_of(*ally) >= 100)
        {
            if (auto *love = ally->find_effect(effect::in_love);
                love && love->strength != effect::frenzied_love)
            {
                log({"{name} goes into a frenzy.", {{"name", ally->name}}});
                love->strength = effect::frenzied_love;
                if (ally->numbers.morale < non_player)
                {
                    ally->numbers.morale = animated_player;
                }
            }
        }
    }
    if (fallen.able())
    {
        return;
    }
    auto *killer = find(striker_ ? striker_ : actor_);
    if (!killer || killer->id == fallen.id || !killer->has(effect::in_love))
    {
        return;
    }
    // The killer's beloved's love: the member's when the NPC kills.
    int &love = killer->id == beloved_ ? romance_->member_love : romance_->npc_love;
    const int alignment = fallen.numbers.alignment;
    if (alignment == 2 || alignment == 5 || alignment == 8)
    {
        love += love < 0xFE ? 2 : 0;
    }
    else if (alignment == 0 || alignment == 3 || alignment == 6)
    {
        love = love >= 20 ? love - 20 : 0;
    }
}

// Love on an attack roll (EffInLove at ATTACKER_ATTACK_ROLL and at the
// defender's): a lover in a frenzy strikes 2 better, one whose proposal was
// refused 2 worse; one who loves and is loved 100 or more 1 better, one whose
// beloved is not fighting, or who loves 100 or more and is loved below 50, 1
// worse. A lover in a frenzy, or refused, is struck 2 better.
int Session::love_modifier(const Fighter &attacker, const Fighter &defender) const
{
    int modifier = 0;
    if (const auto *love = attacker.find_effect(effect::in_love))
    {
        if (love->strength == effect::frenzied_love)
        {
            modifier += 2;
        }
        else if (love->strength == effect::refused_love)
        {
            modifier -= 2;
        }
        else if (romance_)
        {
            const bool npc = attacker.id == beloved_;
            const int own = npc ? romance_->npc_love : romance_->member_love;
            const int other = npc ? romance_->member_love : romance_->npc_love;
            const EntityId ally_id = npc ? lover_ : beloved_;
            const auto *ally = ally_id ? find(ally_id) : nullptr;
            if (own >= 100 && other >= 100)
            {
                modifier += 1;
            }
            else if (!((own < 100 || other >= 50) && ally && ally->able()))
            {
                modifier -= 1;
            }
        }
    }
    if (const auto *love = defender.find_effect(effect::in_love);
        love && (love->strength == effect::frenzied_love || love->strength == effect::refused_love))
    {
        modifier += 2;
    }
    return modifier;
}

bool Session::frenzied(const Fighter &f) const
{
    const auto *love = f.find_effect(effect::in_love);
    return love && love->strength == effect::frenzied_love && f.side == Side::party;
}

// A hold or a drain ends: its victim, whose combat number its strength keeps,
// may move again.
void Session::release(Fighter &holder, int code)
{
    const auto *hold = holder.find_effect(code);
    if (!hold)
    {
        return;
    }
    if (auto *victim = find(static_cast<EntityId>(hold->strength)))
    {
        victim->remove(effect::immobile);
        victim->remove(effect::suffocating);
        if (code == effect::squeezing)
        {
            victim->remove(effect::held_still);
        }
    }
    holder.remove(code);
}

void Session::hold_fast(Fighter &holder, Fighter &victim, int hold, int victim_effect)
{
    holder.numbers.effects.push_back({hold, 0, static_cast<int>(victim.id), true});
    victim.numbers.effects.push_back({victim_effect, 0, 0, false});
    victim.half_moves = 0;
}

bool Session::weapon_named(const Fighter &f, int word) const
{
    constexpr std::uint8_t weapon_slot = 0;
    return std::ranges::any_of(held_now(f),
                               [word](const Held &h)
                               {
                                   return !h.gone && h.carried.item.readied() &&
                                          h.carried.type.worn_location == weapon_slot &&
                                          std::ranges::find(h.carried.item.name_components, word) !=
                                              h.carried.item.name_components.end();
                               });
}

bool Session::armor_named(const Fighter &f, int word) const
{
    constexpr std::uint8_t armor_slot = 2;
    return std::ranges::any_of(held_now(f),
                               [word](const Held &h)
                               {
                                   return !h.gone && h.carried.item.readied() &&
                                          h.carried.type.worn_location == armor_slot &&
                                          std::ranges::find(h.carried.item.name_components, word) !=
                                              h.carried.item.name_components.end();
                               });
}

// What a hit that leaves its target standing carries (cbtacts.pas, RollAttacks;
// effhndlr.pas). The original stacked a new hold at each hit of a holding
// bite; one hold at a time here (an obvious bug, fixed).
void Session::on_hit(Fighter &attacker, Fighter &defender, int phase, int primary_hits, int natural,
                     int attack_roll)
{
    if (champions() && phase == 0 && attacker.has(65) && defender.able())
    {
        poison(defender, 4);
    }
    const auto effects = phase == 0 ? std::span<const int>(primary_hit_effects)
                                    : std::span<const int>(secondary_hit_effects);
    for (const auto code : effects)
    {
        if (code == 0 || !attacker.has(code) || !defender.able())
        {
            continue;
        }
        // Gateway's poison rides only the first attack, its paralysis both;
        // the other games' the second only (effhndlr.pas, conditions 2 and 3).
        if (!champions() &&
            ((code == 64 && (phase == 0) != gateway()) || (code == 67 && phase == 0 && !gateway())))
        {
            continue;
        }
        switch (code)
        {
        case dkk_effect::stunning_hit:
            if (!saving_throw(defender, 1, 0, &attacker))
            {
                (void)add_with_message(defender, {336, 3, 0, false}, false, false, 0,
                                       attacker.numbers.level, nullptr);
            }
            break;
        case dkk_effect::fire_touch:
            nonweapon_damage(defender, roll(6) + roll(6), 2,
                             saving_throw(defender, 4, 0, &attacker), 0, 0, 0, false);
            break;
        case effect::kapak_paralysis:
        case effect::paralyzing_strike:
            paralyze(defender, game_of(identity_) == GoldBoxGame::dark_queen_of_krynn ? 10 : 100,
                     0);
            break;
        case effect::champions_ghoul_touch:
            if (defender.numbers.race != elf_race)
            {
                paralyze(defender, 100, 0);
            }
            break;
        case effect::slight_poison:
            poison(defender, 1);
            break;
        case 64:
            poison(defender, 0);
            break;
        case dq_effect::spore_touch:
            if (!defender.has(effect::disease_proof))
            {
                defender.numbers.effects.push_back({dq_effect::spore_infection, 1440, 255, true});
                log({"{name} is diseased.", {{"name", defender.name}}});
            }
            attacker.numbers.effects.push_back({2194, 0, 255, false});
            kill(attacker);
            break;
        case dq_effect::poison_bite:
            if (!saving_throw(defender, 0, 0, &attacker))
            {
                // This shipped post-hit handler rolls the extra d6 after
                // damage has already been applied; it does not strike again.
                (void)roll(6);
                (void)add_with_message(defender,
                                       {effect::paralysis, roll(6) + roll(6) + roll(6), 12, false},
                                       false, false, 0, 0, nullptr);
            }
            break;
        case dq_effect::mummy_touch:
            if (!defender.has(250) && !defender.has(dq_effect::rot_recovery))
            {
                defender.numbers.effects.push_back({dq_effect::rot_recovery, 14400, 255, true});
                defender.numbers.effects.push_back(
                    {effect::kit_rotting, 43200, roll(6) * 32 + 31, true});
                log({"{name} is diseased.", {{"name", defender.name}}});
            }
            break;
        case dq_effect::enchanted_paralysis:
            if (!saving_throw(defender, 4, -4, &attacker))
            {
                (void)roll(6);
                (void)roll(6);
                (void)roll(6);
                paralyze(defender, 10, 0);
            }
            break;
        case dq_effect::venom_touch:
            if (!saving_throw(defender, 0, 0, &attacker))
            {
                defender.remove(dq_effect::venomed);
                defender.numbers.effects.push_back({dq_effect::venomed, roll(4), 255, false});
                log({"{name} is crippled by venom.", {{"name", defender.name}}});
            }
            break;
        case effect::drinks_blood:
            // A Gateway stirge takes hold on 1 to 4 of a d10 (cbtacts.pas,
            // AttackDrainBlood).
            if (roll(10) <= 4 && !attacker.has(effect::drinking_blood) &&
                !defender.has(effect::immobile))
            {
                hold_fast(attacker, defender, effect::drinking_blood, effect::immobile);
                log({"{attacker} attaches to {name}.",
                     {{"attacker", attacker.name}, {"name", defender.name}}});
            }
            break;
        case effect::constricts:
            // A Gateway squid's arms take hold on 1 to 4 of a d10
            // (cbtacts.pas, AttackConstrict).
            if (roll(10) <= 4 && !attacker.has(effect::constricting) &&
                !defender.has(effect::immobile))
            {
                hold_fast(attacker, defender, effect::constricting, effect::immobile);
                log({"{attacker} constricts {name}.",
                     {{"attacker", attacker.name}, {"name", defender.name}}});
            }
            break;
        case effect::charming_touch:
        {
            // A harpy's touch charms to its side unless saved against as a
            // spell (effhndlr.pas, EffCharmGaze; Treasures' EffHarpyTouch).
            if (defender.has(effect::charm))
            {
                break;
            }
            if (!saving_throw(defender, 4, 0, &attacker))
            {
                defender.numbers.effects.push_back(
                    {effect::charm, 0, (attacker.side == Side::opposition ? 128 : 0) + 3, true});
                charm(defender, attacker.side);
                log({"{name} is charmed.", {{"name", defender.name}}});
            }
            break;
        }
        case effect::disease_touch:
            // Gateway's mummy's touch diseases unless saved against: Strength
            // and Hit Points waste away at once (EffDiseaseTouch).
            if (!saving_throw(defender, 0, 0, &attacker))
            {
                diseased_now(defender, attacker.numbers.level, false);
            }
            break;
        case effect::rat_disease:
            // A Treasures rat's bite diseases one time in 20 unless saved
            // against (EffRatDisease).
            if (roll(100) <= 5 && !defender.has(34) && !saving_throw(defender, 0, 0, &attacker))
            {
                diseased_now(defender, attacker.numbers.level, true);
            }
            break;
        case effect::crushes:
            // A kampfult's hit holds its victim to be crushed (EffKampfultCrush).
            if (!attacker.has(effect::squeezing))
            {
                hold_fast(attacker, defender, effect::squeezing, effect::held_still);
                log({"{name} is being crushed.", {{"name", defender.name}}});
            }
            break;
        case effect::grabs_2d8:
        case effect::grabs_4d5:
        case effect::grabs_6d4:
            // A yeti grabs on an attack roll of 20 or more (T20GrabHelper).
            if (attack_roll >= 20 && !attacker.has(effect::squeezing))
            {
                hold_fast(attacker, defender, effect::squeezing, effect::held_still);
                log({"{attacker} grabs {name}.",
                     {{"attacker", attacker.name}, {"name", defender.name}}});
            }
            break;
        case effect::swallows_on_twenty:
            // A Treasures remorhaz swallows on an attack roll of 20 or more,
            // after the blow (EffSwallowWhole).
            if (attack_roll >= 20)
            {
                swallow_whole(attacker, defender);
            }
            break;
        case effect::poison_touch:
            // Secret's poison rides the second attack form. A creature with
            // none (a centipede, a giant snake or spider, a medusa) would never
            // poison, so its first form carries it (an obvious bug, fixed).
            if (phase == 0 && attacker.numbers.attacks[1].rate_per_two_rounds > 0)
            {
                break;
            }
            secret_poison(defender, 0);
            break;
        case effect::strong_poison_touch:
            secret_poison(defender, -2);
            break;
        case effect::vorpal:
            // A primary blow on a natural 17 or more is the kit's critical
            // hit, which beheads but a creature proof against it (fxcore.c,
            // fx_attack_roll; fxcombat.c, fx_vorpal).
            if (natural >= 17 && (defender.numbers.traits & trait::beheading_proof) == 0)
            {
                kill(defender, "{name} is beheaded.");
            }
            break;
        case effect::ghoul_paralysis:
            // The kit's ghoul holds 10 rounds unless saved against, an elf
            // never (effects.c, fx_ghoul_paralysis).
            if (defender.numbers.race != elf_race)
            {
                paralyze(defender, 10, 0);
            }
            break;
        case effect::adjacent_paralysis:
            // The kit's second blow that holds one beside it 10 rounds
            // unless saved against (fx_adjacent_paralysis).
            if (distance(attacker, defender) < 2)
            {
                paralyze(defender, 10, 0);
            }
            break;
        case effect::fire_blow:
        {
            // The kit's burning second blow: 2d6 of magical fire, halved by
            // a save against spells (fxcombat.c, fx_fire_damage_2d6).
            const bool saved = saving_throw(defender, 4, 0, &attacker, 9);
            nonweapon_damage(defender, roll(6) + roll(6), 2, saved, 9, attacker.numbers.level, 0,
                             false);
            break;
        }
        case effect::stone_touch:
            // A cockatrice's touch, saved against as petrification (EffFleshToStone).
            if (!saving_throw(defender, 1, 0, &attacker))
            {
                turn_to_stone(defender);
            }
            break;
        case effect::ghoul_touch:
            // Held 10 rounds unless saved against (EffGhoulPara).
            paralyze(defender, 10, 0);
            break;
        case 65:
            poison(defender, 4);
            break;
        case 66:
            poison(defender, 2);
            break;
        case 70:
            poison(defender, -2);
            break;
        case 67:
            paralyze(defender, roll(8) + roll(8), 0);
            break;
        case 68:
            // Race 2 is the elf (half-elves are not spared).
            if (defender.numbers.race != 2)
            {
                paralyze(defender, 63, 0);
            }
            break;
        case 69:
            // The original passed -2 and its paralysis ignored it (an obvious bug, fixed).
            paralyze(defender, roll(9) + 10, -2);
            break;
        case 79:
            // Fire on a hit, 2d10 and magical (effhndlr.pas, EffFireTouch).
            nonweapon_damage(defender, roll(10) + roll(10), 0, false, 9, 0, 0, false);
            break;
        case 80:
            // Acid on a hit, 1d4 (EffAcidTouch).
            nonweapon_damage(defender, roll(4), 0, false, 16, 0, 0, false);
            break;
        case 85:
            drain_levels(defender, 1);
            break;
        case 86:
            drain_levels(defender, 2);
            break;
        case 87:
            // Mummy rot: no healing for 10 days, and death in 1d6 months
            // unless cured (effhndlr.pas, EffRotTouch).
            if (!defender.has(effect::rot))
            {
                defender.numbers.effects.push_back({effect::rot, 14400, 0, true});
                defender.numbers.effects.push_back(
                    {effect::mummy_rot, 43200, roll(6) * 16 + 15, true});
                log({"{name} is diseased.", {{"name", defender.name}}});
            }
            break;
        case effect::rot_touch:
            // The kit's mummy rot (effects.c, fx_disease_touch): no healing
            // for 10 days (its 215, the fight's 50), and rotting a month at a
            // time for 1d6 months, its strength the months by 32s, which
            // takes Charisma and at last the victim's life as time passes
            // (OriginalModule::pass_time). The kit also spared one that held
            // its effect 50, Pool of Radiance's rot's number, which is a hot
            // Fire Shield in the kit's (Bugs).
            if (!defender.has(effect::rot))
            {
                defender.numbers.effects.push_back({effect::rot, 14400, 255, true});
                defender.numbers.effects.push_back(
                    {effect::kit_rotting, 43200, roll(6) * 32 + 31, true});
                log({"{name} is diseased.", {{"name", defender.name}}});
            }
            break;
        case 76:
            // A stirge drinks 12 to 15 Hit Points' worth, its strength the
            // victim's, which cannot move meanwhile (effhndlr.pas, EffBloodAttach).
            attacker.numbers.effects.push_back(
                {effect::blood_drain, 12 * 16 + 15, static_cast<int>(defender.id), true});
            defender.numbers.effects.push_back({effect::immobile, 0, 0, false});
            defender.half_moves = 0;
            break;
        case 73:
            if (primary_hits == 2)
            {
                rake(attacker, defender);
            }
            break;
        case 345:
        {
            // Dispel Evil: an evil creature it strikes that fails its save is
            // sent away, and the spell ends (Curse, sp_dispel_evil).
            if (!dispel_evil_takes(defender, game_of(identity_)))
            {
                break;
            }
            if (saving_throw(defender, 4, 0, &attacker))
            {
                log({"{name} resists dispel evil.", {{"name", defender.name}}});
                break;
            }
            log({"{name} is dispelled.", {{"name", defender.name}}});
            defender.condition = Condition::gone;
            defender.hit_points = 0;
            defender.initiative = 0;
            end_effects_at_zero(defender);
            went_down(defender);
            attacker.remove(204);
            attacker.remove(345);
            break;
        }
        case 322:
            // A dracolich's touch paralyzes for the fight (Curse, ovr013).
            paralyze(defender, 0, 0);
            break;
        case 323:
            // A cold touch, 2d8 of magical cold (Curse, ovr013).
            nonweapon_damage(defender, roll(8) + roll(8), 0, false, 10, 0, 0, false);
            break;
        case 296:
            // An owlbear hugs on an attack roll of 18 or more and holds its
            // victim (Curse, AffectOwlbearHugAttackCheck).
            // Gateway's owlbear never hugs one held already (AttackHug).
            if (natural >= 18 && !attacker.has(effect::hugging) &&
                (!gateway() || !defender.has(effect::immobile)))
            {
                attacker.numbers.effects.push_back(
                    {effect::hugging, 0, static_cast<int>(defender.id), true});
                defender.numbers.effects.push_back({effect::immobile, 0, 0, false});
                defender.half_moves = 0;
                log({"{attacker} hugs {name}.",
                     {{"attacker", attacker.name}, {"name", defender.name}}});
            }
            break;
        case dq_effect::constricting_touch:
            if (roll(10) > 4)
            {
                break;
            }
            [[fallthrough]];
        case 257:
            // A shambling mound whose two blows both hit engulfs its victim,
            // which suffocates in 2d4 of its turns (Curse, engulfs).
            // Gateway's on 1 to 4 of a d10 at each hit (its AttackEngulf).
            if ((gateway() ? roll(10) <= 4 : primary_hits == 2) &&
                !defender.has(effect::immobile) && !defender.has(effect::suffocating) &&
                !attacker.has(effect::engulfing) &&
                !(game_of(identity_) == GoldBoxGame::dark_queen_of_krynn && defender.has(2150)))
            {
                attacker.numbers.effects.push_back(
                    {effect::engulfing, 0, static_cast<int>(defender.id), true});
                defender.numbers.effects.push_back({effect::immobile, 0, 0, false});
                defender.numbers.effects.push_back(
                    {effect::suffocating, 0, roll(4) + roll(4), false});
                defender.half_moves = 0;
                log({code == dq_effect::constricting_touch ? "{attacker} constricts {name}."
                                                           : "{attacker} engulfs {name}.",
                     {{"attacker", attacker.name}, {"name", defender.name}}});
            }
            break;
        case 77:
            if (defender.numbers.strength < 19 && !attacker.has(effect::bite_hold))
            {
                attacker.numbers.effects.push_back(
                    {effect::bite_hold, 0, static_cast<int>(defender.id), true});
                defender.numbers.effects.push_back({effect::immobile, 0, 0, false});
                defender.half_moves = 0;
                log({"{name} is held fast.", {{"name", defender.name}}});
            }
            break;
        default:
            break;
        }
    }
}

void Session::diseased_now(Fighter &victim, int level, bool lasting)
{
    constexpr int diseased = 34, wasting_strength = 43, wasting_hit_points = 44;
    if (lasting)
    {
        victim.numbers.effects.push_back({diseased, 0, level, false});
    }
    for (const int wasting : {wasting_strength, wasting_hit_points})
    {
        if (!victim.has(wasting))
        {
            victim.numbers.effects.push_back({wasting, 0, level, false});
        }
    }
    log({"{name} is diseased.", {{"name", victim.name}}});
}

// A poisoned touch kills unless saved against (save 0, paralysis and poison).
void Session::poison(Fighter &victim, int modifier)
{
    if (saving_throw(victim, 0, modifier))
    {
        return;
    }
    log({"{name} is poisoned.", {{"name", victim.name}}});
    victim.numbers.effects.push_back({effect::poisoned, 0, 0, false});
    kill(victim);
}

void Session::paralyze(Fighter &victim, int minutes, int modifier)
{
    if (saving_throw(victim, 0, modifier))
    {
        return;
    }
    // Native natural touches dispatch the resulting paralysis through
    // effect immunity too, after the save. Paralysis-proof creatures remain
    // immune to an enchanted draconian's touch as well as magical holds.
    if (game_of(identity_) == GoldBoxGame::dark_queen_of_krynn &&
        (victim.has(109) || victim.has(111) || victim.has(125) ||
         victim.has(effect::aurak_invulnerable)))
    {
        return;
    }
    log({"{name} is paralyzed.", {{"name", victim.name}}});
    victim.numbers.effects.push_back({effect::paralysis, minutes, 12, false});
}

// Secret's poisoned blow (effhndlr.pas, PoisonTarget, in its reconstruction):
// a victim poisonous itself saves better by its own poison's strength
// against plain poison, and with no penalty against strong poison.
void Session::secret_poison(Fighter &victim, int modifier)
{
    if (const auto *own = victim.find_effect(effect::poison_touch))
    {
        modifier = modifier < 0 ? 0 : modifier + own->strength;
    }
    poison(victim, modifier);
}

// Turned to stone: out of the fight for good.
void Session::turn_to_stone(Fighter &victim)
{
    victim.condition = Condition::stoned;
    victim.hit_points = 0;
    victim.initiative = 0;
    log({"{name} is stoned.", {{"name", victim.name}}});
    end_effects_at_zero(victim);
    went_down(victim);
}

// A Secret remorhaz swallows its victim whole: dead at once (effhndlr.pas,
// EffSwallowWhole, in its reconstruction).
void Session::swallow_whole(Fighter &swallower, Fighter &victim)
{
    log({"{name} is swallowed whole.", {{"name", victim.name}}});
    victim.condition = Condition::dead;
    victim.hit_points = 0;
    victim.initiative = 0;
    end_effects_at_zero(victim);
    went_down(victim);
    log({roll(100) <= 50 ? "{name} belches in satisfaction." : "{victim} is digested instantly.",
         {{"name", swallower.name}, {"victim", victim.name}}});
}

// A level drain, no save (effhndlr.pas, EffDrainOne and EffDrainTwo). Each
// level takes the Hit Points of a level (its maximum over its levels) from
// its maximum and its current; one below level 2 is dead forever. Otherwise
// the class whose level took the most experience (the later on a tie) loses
// a level, its THAC0 and saving throws following the class tables, and one
// left with no Hit Points is killed. The original divided by zero for a
// creature without class levels, which loses its highest level's share here;
// and it kept the drained class's level as the highest, where the highest of
// its classes is kept here (both obvious bugs, fixed).
void Session::drain_levels(Fighter &victim, int levels)
{
    log({levels == 1 ? "{name} is drained a level." : "{name} is drained 2 levels.",
         {{"name", victim.name}}});
    auto &numbers = victim.numbers;
    if (game_of(identity_) == GoldBoxGame::dark_queen_of_krynn && numbers.native_progression)
    {
        for (int n = 0; n < levels && victim.condition != Condition::gone; ++n)
        {
            auto &progression = *numbers.native_progression;
            const auto loss =
                roll_native_drain(progression, numbers.class_levels, numbers.max_hit_points,
                                  numbers.constitution, progression.race, random_);
            if (!loss)
            {
                break;
            }
            const auto before = numbers.class_levels;
            const int missing = std::max(0, numbers.max_hit_points - victim.hit_points);
            progression.peak_class_levels[loss->slot] =
                std::max(progression.peak_class_levels[loss->slot], loss->before_level);
            --numbers.class_levels[loss->slot];
            progression.rolled_hit_points =
                std::max(0, progression.rolled_hit_points - loss->rolled_hit_points);
            progression.experience = loss->experience;
            progression.previous_experience = loss->previous_experience;
            progression.previous_max_hit_points = loss->previous_max_hit_points;
            progression.drains.push_back(*loss);
            // The campaign's public dead-character maximum remains at least
            // one; the exact receipt still retains the source's full HP loss.
            numbers.max_hit_points = std::max(1, numbers.max_hit_points - loss->hit_points);
            victim.hit_points = loss->dead ? 0 : std::max(0, numbers.max_hit_points - missing);
            numbers.thac0 +=
                thac0(numbers.class_levels, game_of(identity_)) - thac0(before, game_of(identity_));
            const auto old_saves = saving_throws(before, game_of(identity_));
            const auto new_saves = saving_throws(numbers.class_levels, game_of(identity_));
            for (std::size_t save = 0; save < numbers.saving_throws.size(); ++save)
            {
                numbers.saving_throws[save] += new_saves[save] - old_saves[save];
            }
            numbers.level =
                *std::max_element(numbers.class_levels.begin(), numbers.class_levels.end());
            const auto attack_rate = [](const ClassLevels &classes)
            {
                const int warrior = std::max({level_in(classes, ClassSlot::fighter),
                                              level_in(classes, ClassSlot::paladin),
                                              level_in(classes, ClassSlot::monk)});
                const int ranger = level_in(classes, ClassSlot::ranger);
                return std::max(warrior > 12  ? 4
                                : warrior > 6 ? 3
                                              : 2,
                                ranger > 14  ? 4
                                : ranger > 7 ? 3
                                             : 2);
            };
            const int attack_change = attack_rate(numbers.class_levels) - attack_rate(before);
            numbers.attacks[0].rate_per_two_rounds =
                std::max(0, numbers.attacks[0].rate_per_two_rounds + attack_change);
            victim.attacks_left[0] =
                std::min(victim.attacks_left[0],
                         attacks_this_round(paced(victim, numbers.attacks[0].rate_per_two_rounds),
                                            static_cast<int>(round_)));
            trim_native_spells(numbers);
            if (loss->dead || victim.has(effect::animated))
            {
                victim.condition = Condition::gone;
                victim.hit_points = 0;
                victim.initiative = 0;
                log({"{name} is dead forever.", {{"name", victim.name}}});
                end_effects_at_zero(victim);
                return;
            }
            if (victim.hit_points == 0)
            {
                victim.condition = Condition::unconscious;
                victim.initiative = 0;
                end_effects_at_zero(victim);
                log({"{name} falls unconscious.", {{"name", victim.name}}});
            }
        }
        return;
    }
    for (int n = 0; n < levels && victim.condition != Condition::gone; ++n)
    {
        auto &classes = numbers.class_levels;
        const int total = std::accumulate(classes.begin(), classes.end(), 0);
        const int per_level = numbers.max_hit_points / std::max({total, numbers.level, 1});
        // One dead forever keeps the maximum it had, which the game weighs
        // as it takes its member back.
        if (numbers.level >= 2 && !victim.has(effect::animated))
        {
            numbers.max_hit_points = std::max(numbers.max_hit_points - per_level, 1);
        }
        victim.hit_points = std::max(victim.hit_points - per_level, 0);
        if (numbers.level < 2 || victim.has(effect::animated))
        {
            victim.condition = Condition::gone;
            victim.hit_points = 0;
            victim.initiative = 0;
            log({"{name} is dead forever.", {{"name", victim.name}}});
            end_effects_at_zero(victim);
            return;
        }
        std::size_t drained = classes.size();
        int most = -1;
        for (std::size_t slot = 0; slot < classes.size(); ++slot)
        {
            if (classes[slot] <= 0)
            {
                continue;
            }
            const int experience = experience_for_level(static_cast<ClassSlot>(slot), classes[slot],
                                                        game_of(identity_))
                                       .value_or(0);
            if (experience >= most)
            {
                most = experience;
                drained = slot;
            }
        }
        if (drained < classes.size())
        {
            const auto before = classes;
            --classes[drained];
            const auto game = game_of(identity_);
            numbers.thac0 += thac0(classes, game) - thac0(before, game);
            const auto saves_before = saving_throws(before, game);
            const auto saves_after = saving_throws(classes, game);
            for (std::size_t save = 0; save < numbers.saving_throws.size(); ++save)
            {
                numbers.saving_throws[save] += saves_after[save] - saves_before[save];
            }
            numbers.level = *std::max_element(classes.begin(), classes.end());
        }
        else
        {
            --numbers.level;
        }
        if (victim.hit_points == 0 && victim.condition != Condition::dead)
        {
            kill(victim);
            return;
        }
    }
}

// Casting (combat.pas, CastSpellCombat): a spell with a casting time takes it
// off the caster's initiative and goes off at its next turn this round; one
// without goes off now.
void Session::begin_cast(Fighter &caster, int spell_number)
{
    const auto &known = *game_spell(spell_number);
    const int delay = known.casting_time / 3;
    if (delay == 0)
    {
        start_aiming(caster, spell_number);
        return;
    }
    log({"{name} begins casting {spell}.",
         {{"name", caster.name}, {"spell", std::string(known.name), true}}});
    caster.pending_spell = spell_number;
    caster.initiative = std::max(caster.initiative - delay, 1);
    if (ordered_rounds())
    {
        reorder_after_cast(caster);
    }
    actor_ = 0;
}

// Choosing targets as the spell goes off (cbtacts.pas, SelectCombatTargets):
// a spell on the caster goes off at once; others wait for their aim.
void Session::start_aiming(Fighter &caster, int spell_number)
{
    const auto &known = *game_spell(spell_number);
    const int area = known.area & 0x0F;
    caster.aiming = spell_number;
    caster.picked.clear();
    caster.blast_delay = -1;
    // Fire Shield waits for its flames to be chosen (SpellFireShield).
    if (spell_number == fire_shield)
    {
        caster.picks_left = 0;
        return;
    }
    if (area == 0)
    {
        resolve_spell(caster, {caster.id}, caster.cell);
        return;
    }
    caster.picks_left = area <= 7 ? (area & 3) + 1 : 1;
}

// Whether the caster can single out a creature for its spell: within range,
// in sight, and no veto (cbtacts.pas, AimTargetMenu). Healing reaches the
// downed but not the dead, Secret's Stone to Flesh a statue.
bool Session::reachable_by_spell(const Fighter &caster, const Fighter &target, int spell) const
{
    constexpr int heal = 236, druid_cure = 298, stone_to_flesh = 313;
    const auto &known = *game_spell(spell);
    const bool heals = spell == 3 || spell == heal || spell == druid_cure;
    const bool slows_poison =
        spell == 26 && target.has(effect::poisoned) && target.condition == Condition::dead;
    const bool unstones = spell == stone_to_flesh && target.condition == Condition::stoned;
    if (!(target.able() || (heals && target.downed()) || slows_poison || unstones) ||
        std::find(caster.picked.begin(), caster.picked.end(), target.id) != caster.picked.end())
    {
        return false;
    }
    return distance(caster, target) <=
               spell_range(known, caster_level(caster, known, game_of(identity_))) &&
           sees(board_, caster, target) && can_target(caster, target);
}

std::vector<rules::Command> Session::aiming_commands(const Fighter &caster) const
{
    std::vector<rules::Command> commands;
    const auto &known = *game_spell(caster.aiming);
    const int area = known.area & 0x0F;
    const auto command = [&](std::string verb)
    {
        rules::Command c;
        c.revision = revision_;
        c.actor = caster.id;
        c.verb = std::move(verb);
        c.label = std::string(known.name);
        c.item = static_cast<unsigned>(caster.aiming);
        return c;
    };
    if (caster.aiming == delayed_blast_spell && caster.blast_delay < 0)
    {
        // How many rounds it waits (the original's 1 to 50 segments, in tens).
        static constexpr std::array<const char *, 6> delays{"No delay",       "Delay 1 round",
                                                            "Delay 2 rounds", "Delay 3 rounds",
                                                            "Delay 4 rounds", "Delay 5 rounds"};
        for (std::size_t rounds = 0; rounds < delays.size(); ++rounds)
        {
            auto choice = command("blast_delay");
            choice.label = delays[rounds];
            choice.item = static_cast<unsigned>(rounds);
            commands.push_back(choice);
        }
    }
    else if (caster.aiming == fire_shield)
    {
        // The flames, hot or cold (SpellFireShield's menu).
        for (const auto flames : {Flames::hot, Flames::cold})
        {
            if (underwater_ && flames == Flames::hot)
            {
                continue;
            }
            auto choice = command("flames");
            choice.label = flames == Flames::hot ? "Hot flames" : "Cold flames";
            choice.item = static_cast<unsigned>(flames);
            commands.push_back(choice);
        }
    }
    else if (area >= 8 && area <= 14)
    {
        // A square in range and in sight, open ground (cbtacts.pas, ManualAimCursor).
        const int range = spell_range(known, caster_level(caster, known, game_of(identity_)));
        for (int y = 0; y < board_.height; ++y)
        {
            for (int x = 0; x < board_.width; ++x)
            {
                const Cell cell{x, y};
                if (rules::blocks_movement(board_.at(cell)) ||
                    doubled_distance(caster.cell, cell) / 2 > range ||
                    !clear_line(board_, caster.cell, cell))
                {
                    continue;
                }
                auto aim = command("aim");
                aim.destination = cell;
                commands.push_back(aim);
            }
        }
    }
    else
    {
        if (game_of(identity_) == GoldBoxGame::dark_queen_of_krynn && area == 15)
        {
            // Native case15 singles out a creature, or uses the high nibble
            // as a radius around empty ground (cbaction.c, combat_spell_targets).
            const int range = spell_range(known, caster_level(caster, known, game_of(identity_)));
            for (int y = 0; y < board_.height; ++y)
            {
                for (int x = 0; x < board_.width; ++x)
                {
                    const Cell cell{x, y};
                    if (standing_at(cell) || board_.at(cell) == rules::Terrain::obstacle ||
                        doubled_distance(caster.cell, cell) / 2 > range ||
                        !clear_line(board_, caster.cell, cell))
                    {
                        continue;
                    }
                    auto aim = command("aim");
                    aim.destination = cell;
                    commands.push_back(aim);
                }
            }
        }
        for (const auto &f : fighters_)
        {
            if (reachable_by_spell(caster, f, caster.aiming))
            {
                auto aim = command("aim");
                aim.target = f.id;
                aim.destination = f.cell;
                aim.intent = known.offensive ? rules::Intent::hostile : rules::Intent::friendly;
                commands.push_back(aim);
            }
        }
        if (!caster.picked.empty())
        {
            commands.push_back(command("aim_done"));
        }
    }
    commands.push_back(command("abort"));
    // The AI aims a spell that comes due on its turn (combat.pas, ManualControl).
    commands.push_back(command("quick"));
    return commands;
}

// Everyone standing within a radius of a square that a line from it reaches,
// allies and the caster too; the nearest first (cbtacts.pas, SelectCombatTargets).
std::vector<EntityId> Session::within(Cell aim, int radius) const
{
    std::vector<std::pair<int, EntityId>> found;
    for (const auto &f : fighters_)
    {
        if (!f.able())
        {
            continue;
        }
        if (const int gap = burst_gap(aim, f); in_burst(gap, radius))
        {
            found.emplace_back(gap, f.id);
        }
    }
    std::stable_sort(found.begin(), found.end(),
                     [](const auto &a, const auto &b)
                     {
                         return a.first < b.first;
                     });
    std::vector<EntityId> ids;
    for (const auto &[gap, id] : found)
    {
        ids.push_back(id);
    }
    return ids;
}

// How far a burst at a square reaches to a creature: the doubled distance to
// the nearest of its squares a line from the square reaches, 1000 for none.
int Session::burst_gap(Cell aim, const Fighter &f) const
{
    int nearest = 1000;
    for (const auto square : squares_of(f))
    {
        if (clear_line(board_, aim, square))
        {
            nearest = std::min(nearest, doubled_distance(aim, square));
        }
    }
    return nearest;
}

// The damage dice an area spell rolls once for all it reaches, by the
// caster's level (magic.pas): Fireball's and Lightning Bolt's level d6, Cone
// of Cold's level d4 and the level, Ice Storm's 3d10, a blast's 6d6, Delayed
// Blast Fireball's level d6 and the level, and a Necklace of Missiles' 3, 5
// or 7 d6 as its missile (1d3) falls; none for any other spell.
SpellDice Session::shared_dice(int number, int level, int necklace_missile)
{
    switch (number)
    {
    case 47:
    case 51:
        return {level, 6, 0};
    case 64:
        return {necklace_missile * 2 + 1, 6, 0};
    case 187:
        return {3, 10, 0};
    case 192:
        return {level, 4, level};
    case 198:
        return {6, 6, 0};
    case delayed_blast_spell:
        return {level, 6, level};
    default:
        return {};
    }
}

// A spell lost: its memorized copy is gone (cbtacts.pas, ProcessAttackResult).
void Session::lose_spell(Fighter &caster)
{
    const int lost = caster.aiming ? caster.aiming : caster.pending_spell;
    auto &spells = caster.numbers.spells;
    if (const auto copy = std::find(spells.begin(), spells.end(), lost); copy != spells.end())
    {
        spells.erase(copy);
    }
    if (caster.pending_spell || caster.aiming)
    {
        log({"{name} loses a spell.", {{"name", caster.name}}});
    }
    caster.pending_spell = caster.aiming = caster.picks_left = 0;
    caster.picked.clear();
}

// The spell goes off at its targets (magic.pas, CastSpell): the caster turns
// to its aim and shows itself, the memorized copy is spent, and its actions end.
bool magic_resists(const Fighter &target, int caster_level, int roll_100);

void Session::resolve_spell(Fighter &caster, std::vector<EntityId> targets, Cell aim, Flames flames)
{
    const int number = caster.aiming;
    const auto &known = *game_spell(number);
    const int level = caster_level(caster, known, game_of(identity_));
    caster.aiming = caster.picks_left = 0;
    caster.picked.clear();
    if (aim != caster.cell)
    {
        caster.direction = direction_toward(caster.cell, aim);
        caster.facing_left = aim.x < caster.cell.x;
    }
    while (caster.has(effect::invisible))
    {
        caster.remove(effect::invisible);
    }
    const int item = caster.using_item;
    if (item >= 0)
    {
        spend_item(caster, item, number);
    }
    else
    {
        auto &spells = caster.numbers.spells;
        if (const auto copy = std::find(spells.begin(), spells.end(), number); copy != spells.end())
        {
            spells.erase(copy);
        }
    }
    log({"{name} casts {spell}.",
         {{"name", caster.name}, {"spell", std::string(known.name), true}}});
    if (underwater_ && !underwater_spell(number))
    {
        log({"That has no effect underwater!", {}});
        end_actions(caster);
        return;
    }
    const auto roll_dice = [this](int count, int sides)
    {
        damage_dice_ = count;
        int total = 0;
        for (int n = 0; n < count; ++n)
        {
            total += roll(sides);
        }
        return total;
    };
    if (champions())
    {
        for (auto &id : targets)
        {
            const auto *target = find(id);
            if (target && target->has(effect::death_knight_reflects) &&
                ((known.area < 8 && roll(10) == 1) || known.area > 14))
            {
                log({"{name} reflects the spell.", {{"name", target->name}}});
                id = caster.id;
            }
        }
    }
    std::erase_if(targets,
                  [&](EntityId id)
                  {
                      const auto &target = *find(id);
                      if (!refuses_spell(target, number))
                      {
                          return false;
                      }
                      log({"{name} is unaffected.", {{"name", target.name}}});
                      return true;
                  });
    std::vector<EntityId> kept;
    switch (number)
    {
    case 505:
    case 1:
        // Bless: the caster's side, but not one with an enemy next to it.
        for (const auto id : targets)
        {
            const auto &f = *find(id);
            if (f.side == caster.side && !enemy_next_to(f))
            {
                kept.push_back(id);
            }
        }
        cast_generic(caster, number, level, kept, 0, 0);
        break;
    case 2:
        for (const auto id : targets)
        {
            if (find(id)->side != caster.side)
            {
                kept.push_back(id);
            }
        }
        cast_generic(caster, number, level, kept, 0, 0);
        break;
    case 3:
    case 298:
        // Cure Light Wounds (the cleric's, and Secret's druid's): 1d8, not
        // through rot; the dying stop bleeding but stay down (magic.pas, the cure).
        for (const auto id : targets)
        {
            auto &f = *find(id);
            if (f.has(effect::rot) || !(f.able() || f.downed()))
            {
                continue;
            }
            f.hit_points = std::min(f.hit_points + roll(8), f.numbers.max_hit_points);
            if (f.condition == Condition::dying)
            {
                f.condition = Condition::unconscious;
                f.overkill = 0;
            }
            log({"{name} is healed.", {{"name", f.name}}});
        }
        break;
    case 4:
        cast_generic(caster, number, level, targets, roll(8), 8);
        break;
    case 507:
    case 9:
        // Burning Hands: the caster's level in fire.
        cast_generic(caster, number, level, targets, level, 9);
        break;
    case 506:
    case 10:
        // Charm Person cannot take a creature of type above 1 or a large one
        // (magic.pas, the charm); Secret and Pools of Darkness judge by race
        // and flags instead (refuses_spell).
        for (const auto id : targets)
        {
            auto &f = *find(id);
            // Treasures' takes any but a monster its flags do not call a
            // person or one of more than a square (its SpellCharmPerson);
            // every character stands on one square (its chargen.pas), though
            // a tall one counts as large here.
            const bool refused =
                treasures() ? f.numbers.race == 0 &&
                                  ((f.numbers.traits & trait::person) == 0 || f.numbers.large)
                            : f.numbers.creature_type > 1 || f.numbers.large;
            if (!secret_engine() && !champions() && refused)
            {
                log({"{name} is unaffected.", {{"name", f.name}}});
                continue;
            }
            // Treasures' frees one charmed already, with no save (its
            // SpellCharmPerson).
            if (treasures() && f.has(effect::charm))
            {
                const auto *node = f.find_effect(effect::charm);
                remove_with_handler(f, static_cast<std::size_t>(node - f.numbers.effects.data()));
                log({"{name} is uncharmed.", {{"name", f.name}}});
                continue;
            }
            kept.push_back(id);
        }
        cast_generic(caster, number, level, kept, 0, 0);
        break;
    case 15:
    {
        // Magic Missile: a missile of 1d4+1 for every two levels, the first at
        // level 1. The original's level div 2 gave none at level 1, against
        // its manual (an obvious bug, fixed).
        const int missiles = (level + 1) / 2;
        cast_generic(caster, number, level, targets, roll_dice(missiles, 4) + missiles, 8);
        break;
    }
    case 20:
        cast_generic(caster, number, level, targets, roll(8) + level, 12);
        break;
    case 21:
        cast_sleep(level, aim);
        break;
    case 23:
    case 49:
    {
        // Hold Person: the fewer held, the harder to save, each in turn from
        // the last picked; a creature of type above 1 or a large one is not
        // held (magic.pas, the hold). In Secret and Pools of Darkness a
        // monster is never held, whatever its size (magic.pas, their
        // multi-target resolvers).
        const int count = static_cast<int>(targets.size());
        const int modifier = count == 1 ? (number == 23 ? -2 : -3) : count == 2 ? -1 : 0;
        for (auto id = targets.rbegin(); id != targets.rend(); ++id)
        {
            auto &f = *find(*id);
            // Treasures' holds no monster either, and its size is no matter
            // (treasures_refuses_spell).
            const bool held_off = secret_engine() || treasures()
                                      ? f.numbers.race == 0
                                      : f.numbers.creature_type > 1 || f.numbers.large;
            if (held_off)
            {
                log({"{name} is unaffected.", {{"name", f.name}}});
                continue;
            }
            cast_generic(caster, number, level, {*id}, 0, 0, modifier);
        }
        break;
    }
    case 32:
    {
        // Native packs 1d4 images above its five-bit caster level.
        const int minutes = known.minutes_fixed + known.minutes_per_level * level;
        const int images = roll(4);
        const int strength =
            game_of(identity_) == GoldBoxGame::dark_queen_of_krynn ? (images << 5) | level : images;
        add_with_message(caster, {effect::mirror_image, minutes, strength, false}, false, false, 0,
                         level, &known);
        break;
    }
    case 37:
        for (const auto id : targets)
        {
            auto &f = *find(id);
            if (f.has(effect::blind))
            {
                f.remove(effect::blind);
                log({"{name} can see.", {{"name", f.name}}});
            }
        }
        break;
    case 40:
        // Cause Disease lasts 1d6 x 10 minutes before the disease takes hold.
        cast_generic(caster, number, level, targets, 0, 0, 0, roll(6) * 10);
        break;
    case 42:
        // Prayer on the caster, or in Gateway on those it touches: its
        // strength keeps the caster's side in bit 4.
        for (const auto id : targets)
        {
            (void)add_with_message(*find(id),
                                   {effect::prayer, level,
                                    (caster.side == Side::opposition ? 0x10 : 0) + level, false},
                                   false, false, 0, level, &known);
        }
        break;
    case 504:
    case 43:
    case 189:
        for (const auto id : targets)
        {
            auto &f = *find(id);
            if (f.has(effect::bestow_curse))
            {
                f.remove(effect::bestow_curse);
                log({"{name} is un-cursed.", {{"name", f.name}}});
            }
        }
        break;
    case 34:
        lay_cloud(caster, level, aim, CloudKind::stinking);
        break;
    case 191:
        lay_cloud(caster, level, aim, CloudKind::deadly);
        break;
    case 26:
        for (const auto id : targets)
        {
            slow_poison(*find(id), level);
        }
        break;
    case 27:
    {
        // Snake Charm: every snake on the field, either side, whose Hit Points
        // the caster's own can pay for (magic.pas, the snake charm). Secret
        // and Pools of Darkness know a snake by its flags and pass over one
        // charmed already (SpellSnakeCharm).
        int pool = caster.hit_points;
        for (auto &f : fighters_)
        {
            const bool snake =
                (secret_engine() || champions())
                    ? (f.numbers.traits & trait::snake) && !f.has(effect::snake_charm)
                    : f.numbers.creature_type == 14;
            if (!f.able() || !snake || f.hit_points > pool)
            {
                continue;
            }
            pool -= f.hit_points;
            add_with_message(f, {effect::snake_charm, 0, level, false}, false, false, 0, level,
                             &known);
        }
        break;
    }
    case 36:
    case 190:
        animate_dead(caster, level);
        break;
    case 57:
        // A Potion of Speed cures Slow, or hastes for 5d4 rounds.
        if (caster.has(effect::slowed))
        {
            caster.remove(effect::slowed);
            log({"{name} is cured.", {{"name", caster.name}}});
        }
        else
        {
            add_with_message(caster, {effect::hasted, roll_dice(5, 4), 12, false}, false, false, 0,
                             level, &known);
        }
        break;
    case 58:
        // Keoghtum's Ointment: poison out (the dead stay dead), else disease,
        // else 1d4+8 healed.
        for (const auto id : targets)
        {
            auto &f = *find(id);
            if (f.has(effect::poisoned))
            {
                f.remove(effect::poisoned);
                f.remove(effect::slow_poison);
            }
            else if (f.has(effect::rot) || f.has(34) || f.has(43))
            {
                for (const int code :
                     {34, 43, 44, 31, effect::rot, effect::mummy_rot, effect::kit_rotting})
                {
                    f.remove(code);
                }
                log({"{name} is cured.", {{"name", f.name}}});
            }
            else if (f.able())
            {
                f.hit_points = std::min(f.hit_points + roll(4) + 8, f.numbers.max_hit_points);
                log({"{name} is healed.", {{"name", f.name}}});
            }
        }
        break;
    case 60:
        // A Javelin of Lightning: 1d6+20 at its mark, then a bolt of 3 squares
        // of 20 beyond it (magic.pas, SpellJavelinOfLightning).
        lightning_at(aim, roll(6) + 20, level);
        lightning_path(caster.cell, aim, 3, 20, level, false);
        break;
    case 61:
        cast_generic(caster, number, level, targets, 0, 0, 0, roll_dice(5, 4));
        break;
    case 62:
        caster.hit_points =
            std::min(caster.hit_points + roll_dice(2, 4) + 2, caster.numbers.max_hit_points);
        log({"{name} is healed.", {{"name", caster.name}}});
        break;
    case 63:
        // Dust of Disappearance for 2d10 x 10 rounds in a fight.
        cast_generic(caster, number, level, targets, 0, 0, 0, roll_dice(2, 10) * 10);
        break;
    case 64:
    {
        // A Necklace of Missiles: a fireball of 3, 5 or 7 dice.
        const auto dice = shared_dice(number, level, roll(3));
        cast_generic(caster, number, level, fireball_reach(targets, aim),
                     roll_dice(dice.count, dice.sides) + dice.bonus, 9);
        break;
    }
    case 65:
        // A Wand of Magic Missiles: 2d4+2, which Shield does not stop.
        cast_generic(caster, number, level, targets, roll_dice(2, 4) + 2, 8);
        break;
    case 41:
    case 46:
        // Dispel Magic on everyone within a square of the aim. The original
        // dispelled the first of them once for each (an obvious bug, fixed).
        for (const auto id : targets)
        {
            dispel(*find(id), level);
        }
        break;
    case 51:
    {
        // Lightning Bolt: the caster's level in d6, one roll, at the aim and
        // then along a bolt of 8 squares in Pool of Radiance, 9 in Treasures
        // and 7 in the other games (magic.pas, SpellLightningBolt; the kit's
        // spellfx.c, spell_lightning_bolt). OpenGoldBox ran Pool of
        // Radiance's bolt in every game (Bugs).
        const auto dice = shared_dice(number, level);
        const int damage = roll_dice(dice.count, dice.sides) + dice.bonus;
        if (underwater_)
        {
            cast_generic(caster, number, level, within(aim, 2), damage, 9);
            break;
        }
        lightning_at(aim, damage, level);
        lightning_path(caster.cell, aim, bolt_length(), damage, level, true);
        break;
    }
    case 47:
    {
        // Fireball: the caster's level in d6, one roll for all, saved for half.
        const auto dice = shared_dice(number, level);
        cast_generic(caster, number, level, fireball_reach(targets, aim),
                     roll_dice(dice.count, dice.sides) + dice.bonus, 9);
        break;
    }
    case 178:
        // Entangle: out of doors, each that fails its save is held by the
        // plants (Curse's SpellEntangle); in a dungeon nothing grows.
        if (!outdoors_)
        {
            log({"Nothing happens.", {}});
            break;
        }
        for (const auto id : targets)
        {
            auto &target = *find(id);
            const bool saved = saving_throw(target, 4, 0, &caster);
            (void)add_with_message(target, {336, known.minutes_fixed, level, false}, true, saved, 0,
                                   level, &known);
        }
        break;
    case 48:
    case 55:
    {
        // Haste the caster's side, or Slow its enemies, up to its level of
        // them, the nearest first; one under the opposite spell is only freed
        // of it (magic.pas, Haste and Slow).
        const bool haste = number == 48;
        for (const auto id : haste_slow_takes(caster, number, level, targets))
        {
            auto &f = *find(id);
            if (!haste && game_of(identity_) == GoldBoxGame::dark_queen_of_krynn &&
                f.has(dq_effect::free_action))
            {
                continue;
            }
            const int opposite = haste ? effect::slowed : effect::hasted;
            if (f.has(opposite))
            {
                f.remove(opposite);
                log({"{name} is cured.", {{"name", f.name}}});
                continue;
            }
            cast_generic(caster, number, level, {id}, 0, 0);
        }
        break;
    }
    case 181:
        // Charm Monsters: Charm Person on any creature (Curse let it charm
        // only what Charm Person can, as it does Hold Monster).
        cast_generic(caster, number, level, targets, 0, 0);
        break;
    case 182:
    {
        // Confusion: at most 2d8 of those in its area.
        auto confused = targets;
        confused.resize(
            std::min<std::size_t>(confused.size(), static_cast<std::size_t>(roll(8) + roll(8))));
        cast_generic(caster, number, level, confused, 0, 0);
        break;
    }
    case 184:
        // Fear: those on a path 6 squares long and 3 wide that fail to save
        // flee (Curse, SpellFear). Pools of Darkness's are forced to flee
        // while the fright lasts (its SpellFear sets their turned flag, which
        // its EffFear clears as the fright ends).
        for (const auto id : path_area(caster, aim, 6, 3))
        {
            auto &f = *find(id);
            if (!saving_throw(f, 4, 0, &caster) &&
                add_with_message(f, {342, known.minutes_per_level * level, level, true}, false,
                                 false, 0, level, &known))
            {
                f.fleeing = true;
                f.turned = f.turned || darkness();
            }
        }
        break;
    case 186:
        // Fumble: a failed save leaves it helpless, a made one slowed
        // (Curse, SpellFumble).
        for (const auto id : targets)
        {
            auto &f = *find(id);
            if (game_of(identity_) == GoldBoxGame::dark_queen_of_krynn &&
                f.has(dq_effect::free_action))
            {
                continue;
            }
            const int minutes = known.minutes_per_level * level;
            const bool saved = saving_throw(f, 4, 0, &caster);
            // Native Fumble uses its own end-actions effect rather than
            // the earlier game's inherited Fumble effect.
            const int fumbled =
                game_of(identity_) == GoldBoxGame::dark_queen_of_krynn ? effect::stunned : 31;
            (void)add_with_message(f, {saved ? effect::slowed : fumbled, minutes, level, false},
                                   false, false, 0, level, &known);
        }
        break;
    case 193:
        // Feeblemind: a cleric saves 1 better, a magic-user 4 worse, any
        // other 2 worse; one feebleminded casts nothing (Curse, AffectFeebleMind).
        for (const auto id : targets)
        {
            auto &f = *find(id);
            const auto &l = f.numbers.class_levels;
            const int modifier = l[static_cast<std::size_t>(ClassSlot::cleric)] > 0       ? 1
                                 : l[static_cast<std::size_t>(ClassSlot::magic_user)] > 0 ? -4
                                                                                          : -2;
            cast_generic(caster, number, level, {id}, 0, 0, modifier);
            if (f.has(268))
            {
                f.numbers.spells.clear();
                f.cast_allowed = false;
            }
        }
        break;
    case 170:
        // Sticks to Snakes: those under 6 Hit Dice fight snakes for a while;
        // stronger ones smash them flat (Curse, SpellSticksToSnakes).
        for (const auto id : targets)
        {
            auto &f = *find(id);
            if (f.numbers.level >= 6)
            {
                log({"{name} smashes them flat.", {{"name", f.name}}});
                continue;
            }
            (void)add_with_message(f, {203, 0, level, false}, false, false, 0, level, &known);
        }
        break;
    case 173:
        // Dispel Evil: on the caster, whose blows send the evil away.
        (void)add_with_message(caster, {204, known.minutes_fixed, level, false}, false, false, 0,
                               level, &known);
        caster.numbers.effects.push_back({345, known.minutes_fixed, level, false});
        break;
    case 185:
    {
        // Fire Shield, hot or cold as its caster chose (the AI and Quick at
        // random, hot on a d10 above 5). Darkness pairs them with its
        // retaliating shield (magic.pas, SpellFireShield).
        const int minutes = known.minutes_fixed + known.minutes_per_level * level;
        const bool hot =
            !underwater_ && (flames == Flames::random ? roll(10) > 5 : flames == Flames::hot);
        (void)add_with_message(caster, {hot ? 250 : 254, minutes, level, false}, false, false, 0,
                               level, &known);
        caster.numbers.effects.push_back(
            {darkness() || game_of(identity_) == GoldBoxGame::dark_queen_of_krynn
                 ? effect::retaliating_shield
                 : 335,
             minutes, level, false});
        break;
    }
    case 183:
    {
        // Dimension Door: to the square aimed at, free of any hold.
        if (standing_at(aim) || !board_.contains(aim) || rules::blocks_movement(board_.at(aim)))
        {
            break;
        }
        for (auto &other : fighters_)
        {
            for (const int hold : {effect::hugging, effect::engulfing, effect::bite_hold})
            {
                if (const auto *held = other.find_effect(hold);
                    held && held->strength == static_cast<int>(caster.id))
                {
                    release(other, hold);
                }
            }
        }
        caster.remove(effect::immobile);
        caster.cell = aim;
        log({"{name} teleports.", {{"name", caster.name}}});
        break;
    }
    case 158:
    case 171:
    case 199:
    {
        // Curse's cures (ovr023): Cure Serious Wounds 2d8+1, Cure Critical
        // Wounds 3d8+3, a healing power 2d4+2.
        const int healed = number == 158   ? roll_dice(2, 8) + 1
                           : number == 171 ? roll_dice(3, 8) + 3
                                           : roll_dice(2, 4) + 2;
        for (const auto id : targets)
        {
            auto &f = *find(id);
            if (f.has(effect::rot) || !(f.able() || f.downed()))
            {
                continue;
            }
            f.hit_points = std::min(f.hit_points + healed, f.numbers.max_hit_points);
            if (f.condition == Condition::dying)
            {
                f.condition = Condition::unconscious;
                f.overkill = 0;
            }
            log({"{name} is healed.", {{"name", f.name}}});
        }
        break;
    }
    case 166:
        cast_generic(caster, number, level, targets, roll_dice(2, 8) + 1, 8);
        break;
    case 172:
        cast_generic(caster, number, level, targets, roll_dice(3, 8) + 3, 8);
        break;
    case 174:
        // Flame Strike: 6d8 of fire.
        cast_generic(caster, number, level, targets, roll_dice(6, 8), 9);
        break;
    case 187:
        // Ice Storm: 3d10 of cold and magic, so magic resistance can turn it
        // (Curse's, Secret's, Gateway's and Treasures' magic.pas,
        // SpellIceStorm), of magic alone in Pools of Darkness and the kit
        // (its spellfx.c, spell_ice_storm).
        {
            auto dice = shared_dice(number, level);
            if (underwater_)
            {
                dice.count = 1;
            }
            cast_generic(caster, number, level, underwater_ ? within(aim, 3) : targets,
                         roll_dice(dice.count, dice.sides) + dice.bonus, ice_storm_element());
            break;
        }
    case 192:
    {
        // Cone of Cold: the caster's level and as many d4 of cold and magic,
        // along a path two wide to half its level (magic.pas,
        // SpellConeOfCold; the kit's spellfx.c, spell_cone_of_cold).
        const auto dice = shared_dice(number, level);
        cast_generic(caster, number, level, path_area(caster, aim, cone_length(level), 2),
                     roll_dice(dice.count, dice.sides) + dice.bonus, 10);
        break;
    }
    case 198:
    {
        // A blast (Curse's Wand of Defoliation, a monster's): 6d6 along a path
        // to 3 squares; a plant takes it whole, with no save (ovr023, sub_61727).
        constexpr int plant = 18;
        const auto dice = shared_dice(number, level);
        const int damage = roll_dice(dice.count, dice.sides) + dice.bonus;
        std::vector<EntityId> others;
        for (const auto id : path_area(caster, aim, 3, 1))
        {
            auto &target = *find(id);
            if (target.numbers.creature_type == plant)
            {
                nonweapon_damage(target, damage, game_spell(number)->save_result, false, 8, level,
                                 number, false);
            }
            else
            {
                others.push_back(id);
            }
        }
        cast_generic(caster, number, level, others, damage, 8);
        break;
    }
    case 194:
    {
        // Hold Monster: Hold Person's saves, on any creature (Curse let it
        // hold only what Hold Person can, docs/ORIGINAL-RULES.md, Bugs).
        const int count = static_cast<int>(targets.size());
        const int modifier = count == 1 ? -3 : count == 2 ? -1 : 0;
        for (auto id = targets.rbegin(); id != targets.rend(); ++id)
        {
            cast_generic(caster, number, level, {*id}, 0, 0, modifier);
        }
        break;
    }
    case 176:
    {
        // Slay Living: unless magic resistance turns it, a failed save
        // kills; a saved one takes 2d8+1 (ovr023, SpellSlayLiving).
        for (const auto id : targets)
        {
            auto &target = *find(id);
            if (magic_resists(target, level, roll(100)))
            {
                log({"{name} is unaffected.", {{"name", target.name}}});
                continue;
            }
            if (!saving_throw(target, 4, 0, &caster))
            {
                log({"{name} is slain.", {{"name", target.name}}});
                kill(target);
                continue;
            }
            nonweapon_damage(target, roll_dice(2, 8) + 1, 0, false, 8, level, number, true);
        }
        break;
    }
    case 168:
        // Poison: the caster's touch carries poison (ovr023, SpellPoison).
        add_with_message(caster, {64, 0, level, false}, false, false, 0, level, &known);
        break;
    case 12:
    case 13:
    case 35:
    case 59:
        cast_strength_spell(caster, number, level, targets);
        break;
    case 236:
    case 256:
    case 290:
    case 310:
    case 311:
    case 313:
    case 314:
    case 315:
    case 316:
    case 317:
        cast_secret_spell(caster, number, level, targets, aim);
        break;
    case native_elixir_youth:
    case 263:
        make_younger(caster);
        break;
    case 301:
    case 303:
    case 304:
    case 308:
    case 309:
    case 320:
    case 321:
    case 323:
    case 324:
    case 325:
    case 326:
        cast_darkness_spell(caster, number, level, targets, aim);
        break;
    case 322:
        // Mind Blank lasts a day (magic.pas, CalcSpellDuration).
        cast_generic(caster, number, level, targets, 0, 0, 0, 1440);
        break;
    case 28:
        cast_generic(caster, number, level, targets, 0, 0);
        conjure_hammer(caster);
        break;
    default:
        cast_generic(caster, number, level, targets, 0, 0);
        break;
    }
    damage_dice_ = 0;
    settle_hammers();
    caster.using_item = -1;
    end_actions(caster);
}

// Haste ages its creature a year the first time it works on it, which a bit
// of its strength marks (effhndlr.pas, EffHasted): as the round's numbers are
// worked out (RecalcStats). A member's age is its marker's.
void Session::age_the_hasted()
{
    for (auto &f : fighters_)
    {
        auto *haste = f.find_effect(effect::hasted);
        if (!haste || (haste->strength & effect::hasted_aged) != 0)
        {
            continue;
        }
        haste->strength |= effect::hasted_aged;
        if (auto *age = f.find_effect(effect::age_in_years))
        {
            ++age->strength;
        }
        log({"{name} ages.", {{"name", f.name}}});
    }
}

namespace
{
// An item that holds spells to read: its type sits in slots 11 to 13
// (magic.pas, ItemReadable).
bool readable_item(const CarriedItem &c)
{
    return c.type.worn_location > 10 && c.type.worn_location < 14;
}

// What the inventory counts of an item: a scroll the bits of its spells left,
// a charged item its charges, a stack its size, anything else one.
int held_count(const CarriedItem &c)
{
    const auto &bytes = c.item.effect_codes;
    if (readable_item(c))
    {
        int bits = 0;
        for (std::size_t at = 0; at < bytes.size(); ++at)
        {
            bits |= bytes[at] != 0 ? 1 << at : 0;
        }
        return bits;
    }
    if (bytes[1] > 0 && bytes[2] < 0x80 && bytes[0] > 0)
    {
        return bytes[0];
    }
    return std::max<int>(c.item.stack_size, 1);
}

void set_readied(Held &held, bool readied)
{
    held.carried.item.readied_raw = readied ? 1 : 0;
    held.carried.item.raw[52] = held.carried.item.readied_raw;
}

// Readies an item last among the readied, or puts it away.
void place_readied(std::vector<Held> &held, std::size_t at, bool readied)
{
    int last = -1;
    for (const auto &other : held)
    {
        last = std::max(last, other.readied_order);
    }
    set_readied(held[at], readied);
    held[at].readied_order = readied ? last + 1 : -1;
}
} // namespace

// The items as the fight has left them: what each usable one has left, the
// shots its missile has left, and those used up (a spent charge's last, a
// thrown weapon gone from its hand).
std::vector<Held> Session::held_now(const Fighter &f) const
{
    auto held = f.held;
    if (!f.record)
    {
        return held;
    }
    std::vector<CarriedItem> worked;
    for (const int at : f.worked_from)
    {
        worked.push_back(held[static_cast<std::size_t>(at)].carried);
    }
    const auto &n = f.numbers;
    for (const auto &usable : usable_items(worked, game_of(identity_)))
    {
        auto &item =
            held[static_cast<std::size_t>(f.worked_from[static_cast<std::size_t>(usable.ordinal)])];
        const auto now = std::ranges::find(n.items, usable.ordinal, &UsableItem::ordinal);
        if (now == n.items.end())
        {
            item.gone = true;
            continue;
        }
        auto &raw = item.carried.item;
        for (std::size_t at = 0; now->scroll && at < now->spells.size(); ++at)
        {
            if (now->spells[at] == 0)
            {
                raw.effect_codes[at] = 0;
                raw.raw[60 + at] = 0;
            }
        }
        if (!now->scroll && usable.charges > 0)
        {
            raw.effect_codes[0] = static_cast<std::uint8_t>(now->charges);
            raw.raw[60] = raw.effect_codes[0];
        }
        if (now->quantity > 0)
        {
            raw.stack_size = static_cast<std::uint8_t>(now->quantity);
            raw.raw[57] = raw.stack_size;
        }
    }
    // The missile's shots: a thrown weapon, or the arrows or quarrels its
    // launcher fires; a readied weapon gone from its hand was thrown.
    namespace bit = por::item_property;
    const auto ammunition = ammunition_types(game_of(identity_));
    for (const int at : f.worked_from)
    {
        auto &weapon = held[static_cast<std::size_t>(at)];
        const auto &c = weapon.carried;
        if (!c.item.readied() || c.type.worn_location != 0)
        {
            continue;
        }
        if (n.weapon_type != c.item.type)
        {
            weapon.gone = true;
            continue;
        }
        const auto &missile = n.missile;
        if (!missile || missile->needs_no_ammunition)
        {
            continue;
        }
        for (const int shot_at : f.worked_from)
        {
            auto &shot = held[static_cast<std::size_t>(shot_at)];
            const auto type = shot.carried.item.type;
            const bool arrows = c.type.has(bit::fires_arrows) && type == ammunition.arrows;
            const bool quarrels =
                c.type.has(bit::fires_quarrels) && ammunition.shot_as_quarrel(type);
            const bool shoots = missile->thrown
                                    ? shot_at == at
                                    : shot.carried.item.readied() && (arrows || quarrels);
            if (!shoots)
            {
                continue;
            }
            if (!missile->loaded)
            {
                shot.gone = true;
            }
            else if (missile->quantity > 0)
            {
                shot.carried.item.stack_size = static_cast<std::uint8_t>(missile->quantity);
                shot.carried.item.raw[57] = shot.carried.item.stack_size;
            }
        }
    }
    return held;
}

// A creature's numbers with these items, as RecalcStats works them out from
// its record, at the Strength it has now: a monster's with all it carries, a
// character's with those it has readied, in the order it readied them.
// `order` gets their places among the held.
Combatant Session::numbers_with(const Fighter &f, const std::vector<Held> &held,
                                std::vector<int> &order) const
{
    order.clear();
    for (std::size_t at = 0; at < held.size(); ++at)
    {
        const bool counted = f.weighs_unreadied || held[at].carried.item.readied();
        if (!held[at].gone && counted)
        {
            order.push_back(static_cast<int>(at));
        }
    }
    if (!f.weighs_unreadied)
    {
        std::ranges::stable_sort(order,
                                 [&held](int a, int b)
                                 {
                                     return held[static_cast<std::size_t>(a)].readied_order <
                                            held[static_cast<std::size_t>(b)].readied_order;
                                 });
    }
    std::vector<CarriedItem> items;
    for (const int at : order)
    {
        items.push_back(held[static_cast<std::size_t>(at)].carried);
    }
    auto record = *f.record;
    record.current_abilities.strength = static_cast<std::uint8_t>(f.numbers.strength);
    record.current_abilities.exceptional_strength =
        static_cast<std::uint8_t>(f.numbers.exceptional_strength);
    return combatant_from_record(record, items);
}

// The creature's numbers as its items change (charhlp.pas, RecalcStats;
// cbtacts.pas, RecalcAttacksLeft): what its items give changes by the
// difference, and what the fight has done to it (drained levels, effects,
// Strength) stays. One that has not attacked yet this round gets the new
// weapon's attacks; one that has keeps no more than that.
void Session::rework_gear(Fighter &f, const std::vector<Held> &before_held,
                          std::vector<Held> after_held)
{
    std::vector<int> before_order, after_order;
    const auto before = numbers_with(f, before_held, before_order);
    const auto after = numbers_with(f, after_held, after_order);
    const auto attacks_with = [&](const Combatant &c)
    {
        const auto &missile = c.missile;
        if (missile && missile->loaded)
        {
            const int attacks = attacks_this_round(paced(f, missile->rate_per_two_rounds),
                                                   static_cast<int>(round_));
            return missile->needs_no_ammunition ? attacks
                                                : std::min(attacks, std::max(missile->quantity, 1));
        }
        return attacks_this_round(paced(f, c.attacks[0].rate_per_two_rounds),
                                  static_cast<int>(round_));
    };
    auto &n = f.numbers;
    n.thac0 += after.thac0 - before.thac0;
    n.armor_class += after.armor_class - before.armor_class;
    n.rear_armor_class += after.rear_armor_class - before.rear_armor_class;
    n.save_bonus += after.save_bonus - before.save_bonus;
    n.movement += after.movement - before.movement;
    n.attacks[0] = after.attacks[0];
    n.large_attack = after.large_attack;
    n.missile = after.missile;
    n.striker = after.striker;
    n.weapon_type = after.weapon_type;
    n.backstabs = after.backstabs;
    n.strength_counts = after.strength_counts;
    // What an effect gives (Secret's reflection) stays.
    n.wards_gaze = after.wards_gaze || (n.wards_gaze && !before.wards_gaze);
    n.mirror = after.mirror || (n.mirror && !before.mirror);
    n.items = after.items;
    // An item put away takes the effect it gave, one readied lays its own
    // (ToggleItemEffect); the numbers worked from the items hold only theirs.
    for (const auto &given : before.effects)
    {
        if (std::ranges::find(after.effects, given.code, &Effect::code) == after.effects.end())
        {
            f.remove(given.code);
        }
    }
    for (const auto &given : after.effects)
    {
        if (!f.has(given.code))
        {
            n.effects.push_back(given);
        }
    }
    const int had = attacks_with(before), now = attacks_with(after);
    f.attacks_left[0] = f.attacks_left[0] >= had ? now : std::min(f.attacks_left[0], now);
    f.held = std::move(after_held);
    f.worked_from = std::move(after_order);
}

// Readying an item, or putting one away, as the View menu does in a fight
// (viewchar.pas, ToggleReadyItem): it takes no action.
void Session::toggle_readied(Fighter &f, std::size_t index)
{
    const auto before = held_now(f);
    auto after = before;
    const bool putting_away = after[index].carried.item.readied();
    place_readied(after, index, !putting_away);
    rework_gear(f, before, std::move(after));
    f.readied_changed = true;
    // Pool of Radiance's hammer put away ends its spell (its special byte's
    // handler 137, EffHammerGone), as Secret's and the later games' do by
    // taking the effect the hammer gives; Curse's and Gateway's stay.
    const auto game = game_of(identity_);
    const bool curse_engine = game == GoldBoxGame::curse_of_the_azure_bonds ||
                              game == GoldBoxGame::gateway_to_the_savage_frontier ||
                              game == GoldBoxGame::unlimited_adventures;
    if (putting_away && f.held[index].conjured && !curse_engine)
    {
        f.remove(effect::spiritual_hammer);
    }
    settle_hammers();
}

// The caster of Spiritual Hammer gains its hammer while the spell lasts,
// unless it has one or carries 16 items (effhndlr.pas, EffSpiritualHammer).
void Session::conjure_hammer(Fighter &caster)
{
    const auto making = hammer_making(game_of(identity_));
    if (!caster.record || !caster.has(effect::spiritual_hammer) ||
        making.type >= item_types_.size())
    {
        return;
    }
    const auto before = held_now(caster);
    const auto living = std::ranges::count_if(before,
                                              [](const Held &h)
                                              {
                                                  return !h.gone;
                                              });
    const bool has_one = std::ranges::any_of(before,
                                             [](const Held &h)
                                             {
                                                 return h.conjured && !h.gone;
                                             });
    if (has_one || living >= 16)
    {
        return;
    }
    Held hammer;
    auto &item = hammer.carried.item;
    item.type = making.type;
    item.name_components = {0, making.type, making.name_word};
    item.magic_bonus = 1;
    item.effect_codes = {0, effect::spiritual_hammer, making.special};
    item.raw[46] = item.type;
    item.raw[47 + 1] = item.name_components[1];
    item.raw[47 + 2] = item.name_components[2];
    item.raw[50] = 1;
    item.raw[61] = item.effect_codes[1];
    item.raw[62] = item.effect_codes[2];
    hammer.carried.type = item_types_[making.type];
    hammer.name = "Spiritual Hammer";
    hammer.conjured = true;
    hammer.start_count = 1;
    auto after = before;
    after.push_back(std::move(hammer));
    const std::size_t at = after.size() - 1;
    std::vector<CarriedItem> readied;
    bool armed = false;
    for (const auto &h : before)
    {
        if (!h.gone && h.carried.item.readied())
        {
            readied.push_back(h.carried);
            armed = armed || h.carried.type.worn_location == 0;
        }
    }
    const bool ready = making.readied_always || (making.readied_unarmed && !armed);
    if (ready && !readying_refusal(after[at].carried, readied, caster.record->item_limit,
                                   game_of(identity_)))
    {
        place_readied(after, at, true);
        caster.readied_changed = true;
    }
    rework_gear(caster, before, std::move(after));
    log({"{name} gains an item.", {{"name", caster.name}}});
}

// A hammer whose spell has ended is gone (EffSpiritualHammer, as its effect
// is removed).
void Session::settle_hammers()
{
    for (auto &f : fighters_)
    {
        const auto conjured = std::ranges::find_if(f.held,
                                                   [](const Held &h)
                                                   {
                                                       return h.conjured && !h.gone;
                                                   });
        if (conjured == f.held.end() || f.has(effect::spiritual_hammer))
        {
            continue;
        }
        const auto at = static_cast<std::size_t>(conjured - f.held.begin());
        const auto before = held_now(f);
        auto after = before;
        after[at].gone = true;
        if (after[at].carried.item.readied())
        {
            place_readied(after, at, false);
            f.readied_changed = true;
        }
        rework_gear(f, before, std::move(after));
    }
}

// What the original's AI makes of a weapon (combatai.pas, CalcWeaponPriority):
// its dice, 8 a plus, 2 a point of its damage bonus, 2 a shot beyond one for a
// launcher and 3 for one hand; holy water against undead is worth 8; none
// for one needing more hands than are free, an aligned weapon of another
// alignment, the Javelin of Lightning or a cursed weapon.
int Session::weapon_priority(const Fighter &f, const Held &weapon, int other_hands) const
{
    namespace bit = por::item_property;
    constexpr int holy_water = 85, aligned_weapon = 132, javelin_of_lightning = 83;
    const auto &type = weapon.carried.type;
    const auto &item = weapon.carried.item;
    const auto &damage = type.small_medium_damage;
    int priority = damage.count * damage.sides;
    priority += item.magic_bonus > 0 ? item.magic_bonus * 8 : 0;
    priority += damage.modifier > 0 ? damage.modifier * 2 : 0;
    const auto target = std::ranges::find(fighters_, f.lock, &Fighter::id);
    const bool pool_numbers = !secret() && !vga_engine(game_of(identity_));
    if (pool_numbers && item.type == holy_water && f.lock && target != fighters_.end() &&
        target->numbers.undead_level > 0)
    {
        priority = 8;
    }
    if (type.has(bit::fires_ammunition))
    {
        priority += (type.rate_of_fire - 1) * 2;
    }
    priority += type.hands <= 1 ? 3 : 0;
    const bool other_alignment = item.effect_codes[2] == aligned_weapon &&
                                 (item.effect_codes[1] & 0xF) != f.numbers.alignment;
    if (type.hands + other_hands > 3 || other_alignment ||
        item.effect_codes[1] == javelin_of_lightning || item.cursed_raw != 0)
    {
        priority = 0;
    }
    return priority;
}

// The original's AI readies its best weapon and shield (combatai.pas,
// EquipWeaponAndArmor): its best missile weapon when it scores above half the
// best melee weapon's (its own blows set the bar), has something to shoot and
// is thrown with Strength or has no enemy next to it; else its best melee
// weapon; and the shield of the best plus its hands leave room for. A cursed
// item is never put away.
void Session::equip_best(Fighter &f)
{
    if (!f.record)
    {
        return;
    }
    namespace bit = por::item_property;
    constexpr std::uint8_t weapon_place = 0, shield_place = 1;
    const auto before = held_now(f);
    auto after = before;
    int other_hands = 0;
    std::optional<std::size_t> weapon, shield;
    for (std::size_t at = 0; at < after.size(); ++at)
    {
        const auto &c = after[at].carried;
        if (after[at].gone || !c.item.readied())
        {
            continue;
        }
        if (c.type.worn_location == weapon_place)
        {
            weapon = at;
        }
        else if (c.type.worn_location == shield_place)
        {
            shield = at;
        }
        else
        {
            other_hands += c.type.hands;
        }
    }
    const auto &unarmed = f.record->base_attacks[0].damage;
    int best_melee_score =
        unarmed.count * unarmed.sides + (unarmed.modifier > 0 ? unarmed.modifier * 2 : 0);
    int best_ranged_score = 1, best_shield_score = 0;
    std::optional<std::size_t> best_melee, best_ranged, best_shield;
    for (std::size_t at = 0; at < after.size(); ++at)
    {
        const auto &held = after[at];
        const auto &type = held.carried.type;
        const bool banked =
            std::ranges::any_of(f.numbers.effects,
                                [at](const Effect &power)
                                {
                                    return power.code == effect::baaz_banked_weapon &&
                                           power.strength == static_cast<int>(at + 1);
                                });
        const bool wrong_order =
            champions() &&
            champions_ready_refusal(held.carried.item, f.record->race, f.record->robe_order)
                .has_value();
        if (held.gone || banked || wrong_order ||
            (type.class_restrictions & f.record->item_limit) == 0)
        {
            continue;
        }
        if (type.worn_location == weapon_place)
        {
            const int score = weapon_priority(f, held, other_hands);
            const bool launcher = type.has(bit::fires_ammunition);
            if ((launcher || type.has(bit::thrown)) && score > best_ranged_score)
            {
                best_ranged = at;
                best_ranged_score = score;
            }
            if (!launcher && score > best_melee_score)
            {
                best_melee = at;
                best_melee_score = score;
            }
        }
        else if (type.worn_location == shield_place)
        {
            const int plus = held.carried.item.magic_bonus;
            const int score = plus >= 0 ? plus + 1 : 0;
            if (score > best_shield_score)
            {
                best_shield = at;
                best_shield_score = score;
            }
        }
    }
    bool supplied = false, thrown_with_strength = false;
    if (best_ranged)
    {
        const auto &type = after[*best_ranged].carried.type;
        constexpr std::uint8_t sling = bit::fires_ammunition | bit::dexterity_missile;
        const auto ammunition = ammunition_types(game_of(identity_));
        const auto readied_of = [&after](auto wanted)
        {
            return std::ranges::any_of(after,
                                       [wanted](const Held &h)
                                       {
                                           return !h.gone && h.carried.item.readied() &&
                                                  wanted(h.carried.item.type);
                                       });
        };
        const auto arrows = [&ammunition](std::uint8_t t)
        {
            return t == ammunition.arrows;
        };
        const auto quarrels = [&ammunition](std::uint8_t t)
        {
            return ammunition.shot_as_quarrel(t);
        };
        supplied = type.has(bit::thrown) || type.properties == sling ||
                   (type.has(bit::fires_arrows) && readied_of(arrows)) ||
                   (type.has(bit::fires_quarrels) && readied_of(quarrels));
        thrown_with_strength =
            type.range > 1 && type.has(bit::thrown) && type.has(bit::strength_bonus);
    }
    const bool shoots = best_ranged && best_ranged_score > best_melee_score / 2 && supplied &&
                        (thrown_with_strength || !enemy_next_to(f));
    const auto chosen = shoots ? best_ranged : best_melee;
    const auto cursed = [&after](std::optional<std::size_t> at)
    {
        return at && after[*at].carried.item.cursed_raw != 0;
    };
    const auto ready = [&after](std::optional<std::size_t> at, bool readied)
    {
        if (at)
        {
            place_readied(after, *at, readied);
        }
    };
    if (weapon != chosen && !cursed(weapon))
    {
        ready(weapon, false);
        ready(chosen, true);
        weapon = chosen;
    }
    const int weapon_hands = weapon ? after[*weapon].carried.type.hands : 0;
    const int shield_hands = shield ? after[*shield].carried.type.hands : 0;
    if (other_hands + weapon_hands + shield_hands > 2)
    {
        // Too many hands: the shield goes, or the weapon when the shield is
        // cursed or there is none.
        ready(shield && !cursed(shield) ? shield : weapon, false);
    }
    else if (other_hands + weapon_hands < 2 && shield != best_shield && !cursed(shield))
    {
        ready(shield, false);
        ready(best_shield, true);
    }
    bool changed = false;
    for (std::size_t at = 0; at < after.size(); ++at)
    {
        changed = changed || after[at].carried.item.readied() != before[at].carried.item.readied();
    }
    if (changed)
    {
        rework_gear(f, before, std::move(after));
        f.readied_changed = true;
    }
}

// Youth (Secret's item power 63, SpellYouth): its user becomes 1d4+1 years
// younger, but not below 18.
void Session::make_younger(Fighter &user)
{
    constexpr int youngest = 18;
    const int years = roll(4) + 1;
    auto *age = user.find_effect(effect::age_in_years);
    if (age && age->strength - years < youngest)
    {
        age->strength = youngest;
        log({"{name} is now 18 years old.", {{"name", user.name}}});
        return;
    }
    if (age)
    {
        age->strength -= years;
    }
    log({"{name} becomes younger.", {{"name", user.name}}});
}

// What using an item spends (viewchar.pas, UseItem): a scroll the spell read,
// gone with its last; a stack one of it; another item a charge, gone at 0.
// An item of no charges never runs out.
void Session::spend_item(Fighter &user, int ordinal, int number)
{
    auto &items = user.numbers.items;
    const auto item = std::find_if(items.begin(), items.end(),
                                   [ordinal](const UsableItem &i)
                                   {
                                       return i.ordinal == ordinal;
                                   });
    if (item == items.end())
    {
        return;
    }
    if (item->scroll)
    {
        if (const auto read = std::find(item->spells.begin(), item->spells.end(), number);
            read != item->spells.end())
        {
            *read = 0;
        }
        if (item->spells == std::array<int, 3>{})
        {
            items.erase(item);
        }
        return;
    }
    if (item->charges <= 0)
    {
        return;
    }
    if (item->quantity > 1)
    {
        --item->quantity;
        return;
    }
    if (--item->charges == 0)
    {
        items.erase(item);
    }
}

// The creatures on a path from a caster through its aim, as Curse's area
// spells take them (ovr023, BuildAreaDamageTargets in its reconstruction): the
// aim is carried on the caster's way to max_range squares, kept where the
// caster can see, and every creature but the caster on the line back to it,
// and on one or two lines beside it when the path is that wide, is taken.
std::vector<EntityId> Session::path_area(const Fighter &caster, Cell aim, int max_range,
                                         int width) const
{
    std::vector<EntityId> found;
    for (const auto square : path_squares(caster, aim, max_range, width))
    {
        if (const auto id = standing_at(square);
            id && id != caster.id && std::ranges::find(found, id) == found.end())
        {
            found.push_back(id);
        }
    }
    return found;
}

// The squares of path_area's path, in the order it takes them: each line
// from its end back to the caster.
std::vector<Cell> Session::path_squares(const Fighter &caster, Cell aim, int max_range,
                                        int width) const
{
    static constexpr std::array<Cell, 8> beside{
        {{1, 0}, {1, 0}, {0, 1}, {0, 1}, {-1, 0}, {-1, 0}, {0, -1}, {0, -1}}};
    static constexpr std::array<Cell, 8> other_side{
        {{-1, 0}, {0, -1}, {0, -1}, {1, 0}, {1, 0}, {0, 1}, {0, 1}, {-1, 0}}};
    Cell end = aim;
    if (aim != caster.cell)
    {
        LineWalk out(caster.cell, {caster.cell.x + (aim.x - caster.cell.x) * max_range * 2,
                                   caster.cell.y + (aim.y - caster.cell.y) * max_range * 2});
        while (out.next() && board_.contains(out.at) &&
               board_.at(out.at) != rules::Terrain::obstacle)
        {
            if (out.distance <= LineWalk(caster.cell, aim).distance)
            {
                continue;
            }
            if (out.distance > max_range * 2)
            {
                break;
            }
            end = out.at;
        }
    }
    std::vector<Cell> squares;
    const auto gather = [&](Cell from)
    {
        if (!board_.contains(from))
        {
            return;
        }
        LineWalk back(from, caster.cell);
        do
        {
            squares.push_back(back.at);
        } while (back.next());
    };
    gather(end);
    const auto toward_caster = static_cast<std::size_t>(direction_toward(end, caster.cell));
    if (width > 1)
    {
        gather({end.x + beside[toward_caster].x, end.y + beside[toward_caster].y});
    }
    if (width > 2)
    {
        gather({end.x + other_side[toward_caster].x, end.y + other_side[toward_caster].y});
    }
    return squares;
}

// Whom Haste (48) or Slow takes among its targets: up to the caster's level of
// the caster's side or of its enemies, the nearest first (resolve_spell).
std::vector<EntityId> Session::haste_slow_takes(const Fighter &caster, int number, int level,
                                                const std::vector<EntityId> &targets) const
{
    const bool haste = number == 48;
    std::vector<EntityId> taken;
    for (const auto id : targets)
    {
        if (static_cast<int>(taken.size()) < level && (find(id)->side == caster.side) == haste)
        {
            taken.push_back(id);
        }
    }
    return taken;
}

// The original's resolver (magic.pas, ResolveSpell): each target saves if the
// spell allows, a touch must hit first, the one damage roll is shared, and the
// spell's effect is added through the immunities.
void Session::cast_generic(Fighter &caster, int number, int level,
                           const std::vector<EntityId> &targets, int damage, int element,
                           int save_modifier, int minutes)
{
    if (damage <= 0)
    {
        element = 0;
    }
    const auto &known = *game_spell(number);
    for (const auto id : targets)
    {
        auto &target = *find(id);
        bool saved = false;
        if (known.save_result != 0)
        {
            saved = saving_throw(target, known.save_type, save_modifier, &caster, element);
        }
        if (game_of(identity_) == GoldBoxGame::dark_queen_of_krynn &&
            target.has(dq_effect::free_action) && (number == 23 || number == 49 || number == 194))
        {
            saved = true;
        }
        if (known.range_fixed == 255)
        {
            const int natural = roll(20);
            if (!attack_hits(natural, caster.numbers.thac0, target.numbers.armor_class,
                             attack_modifier(caster, target, game_of(identity_)) +
                                 love_modifier(caster, target)))
            {
                log({"{attacker} misses {name}.",
                     {{"attacker", caster.name}, {"name", target.name}}});
                saved = true;
                damage = 0;
            }
        }
        if (damage > 0)
        {
            nonweapon_damage(target, damage, known.save_result, saved, element, level, number,
                             targets.size() == 1 && (known.area & 0x0F) < 8);
        }
        if (known.effect > 0 && target.condition != Condition::dead)
        {
            const int lasting =
                minutes >= 0 ? minutes : known.minutes_fixed + known.minutes_per_level * level;
            const int strength = known.effect == effect::charm
                                     ? (caster.side == Side::opposition ? 128 : 0) + level
                                     : level;
            if (add_with_message(target,
                                 {known.effect, lasting, strength, known.effect == effect::charm},
                                 known.save_result == 1, saved, element, level, &known) &&
                known.effect == effect::charm)
            {
                charm(target, caster.side);
            }
        }
    }
}

// The chance magic resistance resists a caster of this level: 15% at level
// 11 and 5 more a level above (50% for the stronger kind, 105); the original's
// byte wraps a chance below 0 to one that always resists (effhndlr.pas, 105, 106).
bool magic_resists(const Fighter &target, int caster_level, int roll_100)
{
    for (const auto [code, base] : {std::pair{105, 50}, std::pair{106, 15}})
    {
        if (!target.has(code))
        {
            continue;
        }
        const int chance = base - (11 - caster_level) * 5;
        if (chance < 0 || roll_100 <= chance)
        {
            return true;
        }
    }
    // Curse's (305, 306) lessen with a higher caster's level, as AD&D's do
    // (ResistMagicPercent in its reconstruction).
    for (const auto [code, base] : {std::pair{305, 50}, std::pair{306, 15}})
    {
        if (target.has(code) && roll_100 <= base + (11 - caster_level) * 5)
        {
            return true;
        }
    }
    // Secret's lessens as Curse's do, from the percent its record holds
    // (fxengine.pas, CheckSpellResistance, in its reconstruction).
    const int percent = target.numbers.magic_resistance;
    return percent > 0 && roll_100 <= percent + (11 - caster_level) * 5;
}

// Secret's spells a creature's race or flags refuse (magic.pas, in its
// reconstruction): Charm Person takes one of the player's races, or a monster
// its flags call a person; Charm Person or Mammal one they call a mammal; Slay
// Living and Death Spell cannot slay the unliving. Secret's own spells are
// numbered 200 higher in the rules (canonical_spell).
bool Session::refuses_spell(const Fighter &target, int number) const
{
    constexpr int charm_person = 10, slay_living = 176, charm_person_or_mammal = 296,
                  death_spell = 310;
    if (champions() && (number == charm_person || number == 506))
    {
        // The native person flag admits humanoid monsters; draconians and
        // undead carry different type flags, and cannot be charmed as people.
        return target.numbers.large ||
               (target.numbers.race == 0 && (target.numbers.traits & trait::person) == 0);
    }
    if (treasures())
    {
        return treasures_refuses_spell(target, number);
    }
    if (!secret_engine())
    {
        return false;
    }
    const bool monster = target.numbers.race == 0;
    const int traits = target.numbers.traits;
    switch (number)
    {
    case charm_person:
        // Pools of Darkness's takes a monster its flags call a mammal, of
        // one square (its SpellCharmPerson).
        if (darkness())
        {
            return (monster && (traits & trait::mammal) == 0) || target.numbers.large;
        }
        return monster && (traits & trait::person) == 0;
    case charm_person_or_mammal:
        return monster && (traits & trait::mammal) == 0;
    case slay_living:
    case death_spell:
        return (traits & trait::unliving) != 0;
    default:
        return false;
    }
}

// Treasures' spells that take only a creature its flags call a person (bit 2
// of its "immunities", which every character has): Hold Person, Charm
// Monsters, Hold Monster and Mass Charm (its magic.pas, ApplyEffectToTargets
// and SpellMassCharm). Hold Person never takes a monster, flagged or not.
// Charm Person judges its targets itself (resolve_spell).
bool Session::treasures_refuses_spell(const Fighter &target, int number) const
{
    constexpr int cleric_hold_person = 23, mage_hold_person = 49, charm_monsters = 181,
                  hold_monster = 194, mass_charm = 320;
    const bool monster = target.numbers.race == 0;
    const bool person = (target.numbers.traits & trait::person) != 0;
    switch (number)
    {
    case cleric_hold_person:
    case mage_hold_person:
        return monster;
    case charm_monsters:
    case hold_monster:
    case mass_charm:
        return monster && !person;
    default:
        return false;
    }
}

// Whether a creature resists magic, by Pool of Radiance's or Curse's numbers
// or by its record in Secret.
bool resists_magic_at_all(const Fighter &target)
{
    return target.has(105) || target.has(106) || target.has(305) || target.has(306) ||
           target.numbers.magic_resistance > 0;
}

// Secret of the Silver Blades' own spells (magic.pas, in its reconstruction),
// by the rules' numbers (canonical_spell).
void Session::cast_secret_spell(Fighter &caster, int number, int level,
                                const std::vector<EntityId> &targets, Cell aim)
{
    constexpr int heal = 236, harm = 256, barkskin = 290, death_spell = 310, disintegrate = 311,
                  stone_to_flesh = 313, flesh_to_stone = 314, delayed_blast_fireball = 315,
                  mass_invisibility = 316, power_word_stun = 317;
    constexpr int diseased = 34;
    const auto &known = *game_spell(number);
    const auto roll_dice = [this](int count, int sides)
    {
        damage_dice_ = count;
        int total = 0;
        for (int n = 0; n < count; ++n)
        {
            total += roll(sides);
        }
        return total;
    };
    switch (number)
    {
    case heal:
    case barkskin:
        // Secret's table casts Barkskin as Heal (spellFns[90]).
        for (const auto id : targets)
        {
            heal_all_but_1d4(*find(id));
        }
        break;
    case harm:
        // Harm diseases its target and leaves it 1d4 Hit Points.
        for (const auto id : targets)
        {
            auto &target = *find(id);
            (void)add_with_message(target, {diseased, 0, level, false}, false, false, 8, level,
                                   &known);
            nonweapon_damage(target, target.hit_points - roll(4), 0, false, 8, level, number,
                             false);
        }
        break;
    case death_spell:
        cast_death_spell(targets);
        break;
    case disintegrate:
    case flesh_to_stone:
        // A failed save against spells slays outright, or turns to stone.
        for (const auto id : targets)
        {
            auto &target = *find(id);
            if (turns_aside_slaying(target) || saving_throw(target, 4, 0, &caster))
            {
                continue;
            }
            if (number == disintegrate)
            {
                kill(target, "{name} disintegrates.");
            }
            else
            {
                turn_to_stone(target);
            }
        }
        break;
    case stone_to_flesh:
        // A statue lives again with 1 Hit Point, where it stood.
        for (const auto id : targets)
        {
            auto &target = *find(id);
            // A Treasures stone guardian dies by it, and a stone golem
            // becomes flesh for two of its turns (SpellStoneToFlesh).
            if (target.has(effect::stone_to_flesh_kills) && target.able())
            {
                kill(target);
                continue;
            }
            if (auto *golem = target.find_effect(effect::stone_golem); golem && target.able())
            {
                const bool stone = golem->strength == effect::golem_stone;
                golem->strength = stone ? 1 : golem->strength;
                log({stone ? "{name} becomes flesh." : "{name} is still flesh.",
                     {{"name", target.name}}});
                continue;
            }
            if (target.condition != Condition::stoned || !cell_free(target))
            {
                log({"{name} is unaffected.", {{"name", target.name}}});
                continue;
            }
            stand_up(target, 1, "{name} is no longer a statue.");
        }
        break;
    case delayed_blast_fireball:
        cast_delayed_blast(caster, level, targets, aim);
        break;
    case mass_invisibility:
    {
        // The caster's side, but not one with an enemy next to it.
        std::vector<EntityId> hidden;
        for (const auto id : targets)
        {
            const auto &target = *find(id);
            if (target.side == caster.side && !enemy_next_to(target))
            {
                hidden.push_back(id);
            }
        }
        cast_generic(caster, number, level, hidden, 0, 0);
        break;
    }
    case power_word_stun:
        // Stunned for 4d4 rounds at up to 30 Hit Points, 2d4 to 60 and 1d4
        // to 90; above that it is unaffected.
        for (const auto id : targets)
        {
            auto &target = *find(id);
            const int hit_points = target.hit_points;
            const int rounds = hit_points <= 30   ? roll_dice(4, 4)
                               : hit_points <= 60 ? roll_dice(2, 4)
                               : hit_points <= 90 ? roll(4)
                                                  : 0;
            if (rounds == 0)
            {
                log({"{name} is unaffected.", {{"name", target.name}}});
                continue;
            }
            (void)add_with_message(target, {effect::stunned, rounds, level, false}, false, false, 0,
                                   level, &known);
        }
        break;
    default:
        break;
    }
}

// The Strength spells (magic.pas, SpellEnlarge, SpellReduce, SpellStrength,
// SpellGiantStrength): each effect keeps the Strength it gives in its
// strength, by the original's encoding (strength_code).
void Session::cast_strength_spell(Fighter &caster, int number, int level,
                                  const std::vector<EntityId> &targets)
{
    constexpr int enlarge = 12, reduce = 13, giant_strength = 59;
    const auto &known = *game_spell(number);
    const int minutes = known.minutes_fixed + known.minutes_per_level * level;
    const auto game = game_of(identity_);
    for (const auto id : targets)
    {
        auto &target = *find(id);
        const auto &n = target.numbers;
        if (number == reduce)
        {
            if (!target.has(effect::enlarged) || saving_throw(target, 4, 0, &caster))
            {
                log({"{name} is unaffected.", {{"name", target.name}}});
                continue;
            }
            target.remove(effect::enlarged);
            log({"{name} has been reduced.", {{"name", target.name}}});
            recount_strength(target);
            continue;
        }
        const auto [base, exceptional] =
            number == enlarge ? enlarged_strength(level, game)
            : number == giant_strength
                ? std::pair{21, 0}
                : strengthened(n.strength, n.exceptional_strength, n.class_levels, game,
                               roll(strength_spell_die(n.class_levels, game)));
        const bool native_frost =
            number == giant_strength && game == GoldBoxGame::dark_queen_of_krynn;
        const int code = number == enlarge ? effect::enlarged
                         : native_frost    ? 113
                                           : effect::strengthened;
        const bool stronger = stronger_than(base, exceptional, n.strength, n.exceptional_strength);
        // A Potion of Giant Strength lasts the fight (its table's no time).
        const int lasting = native_frost               ? 40 + roll(4) * 10
                            : number == giant_strength ? 0
                                                       : minutes;
        if (add_with_message(target, {code, lasting, strength_code(base, exceptional), false},
                             false, false, 0, level, &known))
        {
            recount_strength(target);
            if (stronger)
            {
                log({"{name} is stronger.", {{"name", target.name}}});
            }
        }
    }
}

// A creature's Strength again (fxengine.pas, RecalcAbility): its own, less
// what a disease has wasted, or the best its Enlarge and Strength effects
// give; a change moves its blows' to-hit and damage when they take Strength.
void Session::recount_strength(Fighter &f)
{
    auto &n = f.numbers;
    int base = n.natural_strength, exceptional = n.natural_exceptional_strength;
    if (const auto *wasted = f.find_effect(effect::strength_wasted); wasted && wasted->strength > 0)
    {
        base = std::max(3, base - wasted->strength);
        exceptional = 0;
    }
    for (const auto &e : n.effects)
    {
        if (e.code != effect::enlarged && e.code != effect::strengthened &&
            !(game_of(identity_) == GoldBoxGame::dark_queen_of_krynn && e.code == 113))
        {
            continue;
        }
        const auto [given, given_exceptional] = strength_of_code(e.strength);
        if (stronger_than(given, given_exceptional, base, exceptional))
        {
            base = given;
            exceptional = given_exceptional;
        }
    }
    if (n.strength_counts)
    {
        n.thac0 += strength_to_hit_bonus(n.strength, n.exceptional_strength) -
                   strength_to_hit_bonus(base, exceptional);
        const int more_damage = strength_damage_bonus(base, exceptional) -
                                strength_damage_bonus(n.strength, n.exceptional_strength);
        n.attacks[0].modifier += more_damage;
        if (n.large_attack)
        {
            n.large_attack->modifier += more_damage;
        }
    }
    n.strength = base;
    n.exceptional_strength = exceptional;
}

// Out of doors a fireball catches only those within 2 squares of its aim,
// not 3 (magic.pas, SpellFireball, outside 3D maps).
std::vector<EntityId> Session::fireball_reach(const std::vector<EntityId> &targets, Cell aim) const
{
    return outdoors_ ? within(aim, 2) : targets;
}

// Secret's Delayed Blast Fireball (SpellDelayedBlastFireball): with no delay
// it bursts at once, the caster's level and as many d6 of fire on all within
// 3 of the aim; with one it waits on its square for those rounds (effect 100,
// EffDelayedBlastFireball). The AI never delays it: the original asked anyone
// casting it, the computer's creatures too, for the delay.
void Session::cast_delayed_blast(Fighter &caster, int level, const std::vector<EntityId> &targets,
                                 Cell aim)
{
    constexpr int fire = 9, delayed_blast_fireball = 315;
    const int delay = std::max(caster.blast_delay, 0);
    caster.blast_delay = -1;
    if (delay == 0)
    {
        if (game_of(identity_) == GoldBoxGame::dark_queen_of_krynn)
        {
            damage_dice_ = level;
        }
        const auto dice = shared_dice(delayed_blast_fireball, level);
        int total = dice.bonus;
        for (int die = 0; die < dice.count; ++die)
        {
            total += roll(dice.sides);
        }
        cast_generic(caster, delayed_blast_fireball, level, targets, total, fire);
        return;
    }
    const int index =
        static_cast<int>(std::ranges::count(blasts_, caster.id, &DelayedBlast::caster));
    blasts_.push_back({caster.id, index, aim, level});
    caster.numbers.effects.push_back({effect::delayed_blast, delay, index, true});
    log({"{name} sets a delayed blast fireball.", {{"name", caster.name}}});
}

// The blast goes off on its square: everyone within 3 takes the caster's
// level and as many d6 of fire, half on a save against spells.
void Session::delayed_blast_goes_off(Fighter &caster, int index)
{
    constexpr int fire = 9, delayed_blast_fireball = 315;
    const auto blast = std::ranges::find_if(blasts_,
                                            [&](const DelayedBlast &b)
                                            {
                                                return b.caster == caster.id && b.index == index;
                                            });
    if (blast == blasts_.end())
    {
        return;
    }
    const auto set = *blast;
    blasts_.erase(blast);
    const int previous_dice = damage_dice_;
    if (game_of(identity_) == GoldBoxGame::dark_queen_of_krynn)
    {
        damage_dice_ = set.level;
    }
    log({"Delayed Blast Fireball goes off.", {}});
    const auto dice = shared_dice(delayed_blast_fireball, set.level);
    int total = dice.bonus;
    for (int die = 0; die < dice.count; ++die)
    {
        total += roll(dice.sides);
    }
    for (const auto id : within(set.cell, 3))
    {
        auto &target = *find(id);
        const bool saved = saving_throw(target, 4, 0, &caster);
        nonweapon_damage(target, total, 2, saved, fire, set.level, delayed_blast_fireball, false);
    }
    if (game_of(identity_) == GoldBoxGame::dark_queen_of_krynn)
    {
        damage_dice_ = previous_dice;
    }
}

// Heal (SpellHeal): disease, blindness and feeblemind cured, then every Hit
// Point but 1d4; one already that close to its maximum gains none.
void Session::heal_all_but_1d4(Fighter &target)
{
    constexpr int feebleminded = 268;
    bool cured = false;
    for (const int disease : {31, 34, 43, 44})
    {
        while (target.has(disease))
        {
            target.remove(disease);
            cured = true;
        }
    }
    if (cured)
    {
        log({"{name} is cured.", {{"name", target.name}}});
    }
    if (target.has(effect::blind))
    {
        target.remove(effect::blind);
        log({"{name} can see.", {{"name", target.name}}});
    }
    if (target.has(feebleminded))
    {
        target.remove(feebleminded);
        log({"{name} regains intellect.", {{"name", target.name}}});
    }
    const int short_of = roll(4);
    const int maximum = target.numbers.max_hit_points;
    if (target.hit_points + short_of <= maximum)
    {
        target.hit_points = maximum - short_of;
    }
    if (target.condition == Condition::dying)
    {
        target.condition = Condition::unconscious;
        target.overkill = 0;
    }
    log({"{name} is healed.", {{"name", target.name}}});
}

// Death Spell (SpellDeathSpell): 4d20 x 2 to spend on those in its area in
// turn, each costing by its level (1 at level 0, 2 at 1, 8 at 2 and 3, 16 at
// 4 and 5, 16 or 20 at 6 by whether its most Hit Points pass 51, 20 at 7 and
// 8, 162 above); each paid for drops dead. The unliving refuse it
// (refuses_spell).
void Session::cast_death_spell(const std::vector<EntityId> &targets)
{
    const int budget = (roll(20) + roll(20) + roll(20) + roll(20)) * 2;
    for (const auto id : death_spell_takes(targets, budget, true_most_hit_points))
    {
        kill(*find(id), "{name} drops dead.");
    }
}

// Whom a Death Spell of this budget slays among its targets, by their
// levels and each one's most Hit Points as `most_hit_points` gives them
// (cast_death_spell).
template <typename Most>
std::vector<EntityId> Session::death_spell_takes(const std::vector<EntityId> &targets, int budget,
                                                 Most &&most_hit_points) const
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

// Pools of Darkness's own spells in a fight (magic.pas, in its
// reconstruction), by the rules' numbers (canonical_spell).
void Session::cast_darkness_spell(Fighter &caster, int number, int level,
                                  const std::vector<EntityId> &targets, Cell aim)
{
    constexpr int blade_barrier = 301, energy_drain = 303, destruction = 304, hold_animal = 308,
                  protection_from_fire = 309, mass_charm = 320, irresistible_dance = 321,
                  power_word_blind = 323, meteor_swarm = 324, power_word_kill = 325,
                  monster_summoning = 326;
    const auto &known = *game_spell(number);
    switch (number)
    {
    case blade_barrier:
        lay_cloud(caster, level, aim, CloudKind::blades);
        break;
    case energy_drain:
        for (const auto id : targets)
        {
            drain_levels(*find(id), 1);
        }
        break;
    case destruction:
        // Gone for good, unless magic resistance turns it away (SpellDestruction).
        for (const auto id : targets)
        {
            auto &target = *find(id);
            if (turns_aside_slaying(target) ||
                (resists_magic_at_all(target) && magic_resists(target, level, roll(100))))
            {
                log({"{name} is unaffected.", {{"name", target.name}}});
                continue;
            }
            target.condition = Condition::gone;
            target.hit_points = 0;
            target.initiative = 0;
            log({"{name} is destroyed.", {{"name", target.name}}});
            end_effects_at_zero(target);
            went_down(target);
        }
        break;
    case hold_animal:
    {
        // The fewer it holds, the harder they save: -4 for one, -2 for two,
        // -1 for three (SpellHoldAnimal).
        const auto count = targets.size();
        const int modifier = count == 1 ? -4 : count == 2 ? -2 : count == 3 ? -1 : 0;
        cast_generic(caster, number, level, targets, 0, 0, modifier);
        break;
    }
    case protection_from_fire:
        // A ranger gets the druid's protection, until it has taken 12 points
        // of fire a caster level; anyone else Resist Fire, with no end
        // (SpellProtectionFromFire).
        for (const auto id : targets)
        {
            auto &target = *find(id);
            const bool ranger =
                target.numbers.class_levels[static_cast<std::size_t>(ClassSlot::ranger)] > 0;
            (void)add_with_message(target,
                                   ranger ? Effect{effect::druid_fire_protection, 0, level, false}
                                          : Effect{effect::resist_fire, 0, 0, false},
                                   false, false, 0, level, &known);
        }
        break;
    case mass_charm:
        cast_mass_charm(caster, level, targets);
        break;
    case irresistible_dance:
        // A touch that must hit; the dance lasts 1d4+1 rounds
        // (SpellIrresistibleDance, CalcSpellDuration).
        for (const auto id : targets)
        {
            auto &target = *find(id);
            if (game_of(identity_) == GoldBoxGame::dark_queen_of_krynn &&
                target.has(dq_effect::free_action))
            {
                continue;
            }
            if (!attack_hits(roll(20), caster.numbers.thac0, target.numbers.armor_class,
                             attack_modifier(caster, target, game_of(identity_)) +
                                 love_modifier(caster, target)))
            {
                log({"{attacker} misses {name}.",
                     {{"attacker", caster.name}, {"name", target.name}}});
                continue;
            }
            (void)add_with_message(target, {effect::dancing, roll(4) + 1, level, false}, false,
                                   false, 0, level, &known);
        }
        break;
    case power_word_blind:
        cast_power_word_blind(targets);
        break;
    case meteor_swarm:
        cast_meteor_swarm(caster, level, aim);
        break;
    case power_word_kill:
        cast_power_word_kill(targets);
        break;
    case monster_summoning:
        // It gates in creatures that wait off the field for it, which no
        // fight of OpenGoldBox's keeps (SpellMonsterSummoning).
        log({"{name}'s summons is not answered.", {{"name", caster.name}}});
        break;
    default:
        break;
    }
}

// Mass Charm charms enemies of up to twice the caster's level in Hit Dice,
// each saving at -2; it passes over a large creature, the caster's side and,
// in Pools of Darkness, a monster its record does not call a person (bit 2 of
// its "immunities", which every character has; SpellMassCharm).
void Session::cast_mass_charm(Fighter &caster, int level, const std::vector<EntityId> &targets)
{
    const auto &known = *game_spell(320);
    for (const auto [id, take] : mass_charm_takes(caster, level, targets))
    {
        auto &target = *find(id);
        if (take == MassCharmTake::unaffected)
        {
            log({"{name} is unaffected.", {{"name", target.name}}});
            continue;
        }
        if (take == MassCharmTake::passed_over)
        {
            continue;
        }
        const bool saved = saving_throw(target, 4, -2, &caster);
        const int strength = (caster.side == Side::opposition ? 128 : 0) + level;
        if (add_with_message(target, {effect::charm, 0, strength, true}, true, saved, 0, level,
                             &known))
        {
            charm(target, caster.side);
        }
    }
}

// What Mass Charm does with each target in turn, until its Hit Dice are spent
// (cast_mass_charm).
std::vector<std::pair<EntityId, Session::MassCharmTake>>
Session::mass_charm_takes(const Fighter &caster, int level,
                          const std::vector<EntityId> &targets) const
{
    std::vector<std::pair<EntityId, MassCharmTake>> takes;
    const int limit = 2 * level;
    int total = 0;
    for (const auto id : targets)
    {
        if (total == limit)
        {
            break;
        }
        const auto &target = *find(id);
        const bool person =
            target.numbers.race != 0 || (target.numbers.traits & trait::person) != 0;
        if ((darkness() && !person) || target.numbers.large || target.side == caster.side)
        {
            takes.emplace_back(id, MassCharmTake::unaffected);
            continue;
        }
        const int cost = target.numbers.level;
        if (total + cost > limit)
        {
            takes.emplace_back(id, MassCharmTake::passed_over);
            continue;
        }
        total += cost;
        takes.emplace_back(id, MassCharmTake::tried);
    }
    return takes;
}

// Power Word Blind blinds those within reach, the nearest first, up to 100
// Hit Points of them in all: for 1d4+1 rounds, ten times as long when they
// have fewer than 51 between them (SpellPowerWordBlind, which worked the time
// out and then blinded them for good, docs/ORIGINAL-RULES.md, Bugs).
void Session::cast_power_word_blind(const std::vector<EntityId> &targets)
{
    const auto &known = *game_spell(323);
    const auto blinded = blinded_by_word(targets, true_most_hit_points);
    int total = 0;
    for (const auto id : blinded)
    {
        total += find(id)->numbers.max_hit_points;
    }
    const int rounds = (roll(4) + 1) * (total < 51 ? 10 : 1);
    for (const auto id : blinded)
    {
        (void)add_with_message(*find(id), {effect::blind, rounds, 0, false}, false, false, 0, 0,
                               &known);
    }
}

// Whom Power Word Blind blinds among its targets, the nearest first, by each
// one's most Hit Points as `most_hit_points` gives them (cast_power_word_blind).
template <typename Most>
std::vector<EntityId> Session::blinded_by_word(const std::vector<EntityId> &targets,
                                               Most &&most_hit_points) const
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

// Meteor Swarm: four meteors land on the squares diagonal to the aim, or on
// it where the caster cannot see them; each strikes all on its way for 10d4
// and bursts on all within a square of where it lands for 10d4, halved by a
// save (SpellMeteorSwarm).
void Session::cast_meteor_swarm(Fighter &caster, int level, Cell aim)
{
    const int previous_dice = damage_dice_;
    if (game_of(identity_) == GoldBoxGame::dark_queen_of_krynn)
    {
        damage_dice_ = 10;
    }
    constexpr int fire_and_magic = 9;
    const auto ten_d4 = [this]
    {
        int total = meteor_dice.bonus;
        for (int die = 0; die < meteor_dice.count; ++die)
        {
            total += roll(meteor_dice.sides);
        }
        return total;
    };
    for (const auto offset : meteor_landings)
    {
        const auto [landing, struck] = meteor_fall(caster, aim, offset);
        for (const auto id : struck)
        {
            auto &target = *find(id);
            if (target.able())
            {
                nonweapon_damage(target, ten_d4(), 0, false, fire_and_magic, level, 324, false);
            }
        }
        for (const auto id : within(landing, 1))
        {
            auto &target = *find(id);
            if (target.able())
            {
                const bool saved = saving_throw(target, 4, 0, &caster, fire_and_magic);
                nonweapon_damage(target, ten_d4(), 2, saved, fire_and_magic, level, 324, false);
            }
        }
    }
    if (game_of(identity_) == GoldBoxGame::dark_queen_of_krynn)
    {
        damage_dice_ = previous_dice;
    }
}

// Where a meteor of Meteor Swarm lands, by its offset from the aim (on the
// aim where the caster cannot see the square), and whom it strikes on its
// way there: everyone standing on its path but the caster and the one where
// it lands (cast_meteor_swarm).
Session::MeteorFall Session::meteor_fall(const Fighter &caster, Cell aim, Cell offset) const
{
    MeteorFall fall{{aim.x + offset.x, aim.y + offset.y}, {}};
    if (!board_.contains(fall.landing) ||
        !rules::has_line_of_sight(board_, caster.cell, fall.landing))
    {
        fall.landing = aim;
    }
    LineWalk path(caster.cell, fall.landing);
    while (path.next())
    {
        const auto id = standing_at(path.at);
        if (id && id != caster.id && path.at != fall.landing &&
            std::ranges::find(fall.struck, id) == fall.struck.end())
        {
            fall.struck.push_back(id);
        }
    }
    return fall;
}

// Power Word Kill slays its first target of 11 to 60 most Hit Points, or
// else every one of 10 or fewer (SpellPowerWordKill).
void Session::cast_power_word_kill(const std::vector<EntityId> &targets)
{
    for (const auto id : slain_by_word(targets, true_most_hit_points))
    {
        if (auto &target = *find(id); !turns_aside_slaying(target))
        {
            kill(target, "{name} is slain.");
        }
    }
}

// Whom Power Word Kill slays among its targets, by each one's most Hit
// Points as `most_hit_points` gives them (cast_power_word_kill).
template <typename Most>
std::vector<EntityId> Session::slain_by_word(const std::vector<EntityId> &targets,
                                             Most &&most_hit_points) const
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

// Whether a spell that slays or petrifies outright is turned aside before
// its save (Secret's condition 9): an iron golem takes no spell, and an
// image takes it for its caster. Magic resistance does not turn it, as the
// original weighs resistance only against an effect or magical damage
// (CheckSpellResistance).
bool Session::turns_aside_slaying(Fighter &target)
{
    if (target.has(effect::iron_golem))
    {
        log({"{name} is unaffected.", {{"name", target.name}}});
        return true;
    }
    return mirror_absorbs(target);
}

// An effect added as a spell adds it (fxengine.pas, AddEffectWithMessage): the
// target's immunities may nullify it, a save that negates refuses it, and an
// effect it has already is replaced by a longer or lasting one.
bool Session::add_with_message(Fighter &target, Effect added, bool negates, bool saved, int element,
                               int caster_level, const Spell *known)
{
    const int code = added.code;
    if (refuses_effect(target, code, element, caster_level, known, fight_dice()) ||
        (negates && saved))
    {
        log({"{name} is unaffected.", {{"name", target.name}}});
        return false;
    }
    if (auto *old = target.find_effect(code);
        old && ((old->minutes != 0 && added.minutes > old->minutes) || added.minutes == 0))
    {
        target.remove(code);
    }
    target.numbers.effects.push_back(added);
    if (code == effect::sleep && target.condition == Condition::okay)
    {
        target.initiative = 0;
    }
    log({"{name} is affected by {spell}.",
         {{"name", target.name}, {"spell", std::string(known ? known->name : ""), true}}});
    return true;
}

// Whether the target's immunities nullify an effect a spell would add
// (add_with_message); `dice` draws the chances some of them take. It changes
// nothing, so an outlook may weigh each way it can go (each_way).
template <typename Dice>
bool Session::refuses_effect(const Fighter &target, int code, int element, int caster_level,
                             const Spell *known, Dice &&dice) const
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

// Whether a non-weapon harm does something else to the target than damage:
// an iron golem's, a flesh golem's and fire on one that fire heals
// (absorb_harm).
bool absorbs_harm(const Fighter &target, int element)
{
    return target.has(effect::iron_golem) || target.has(dkk_effect::flesh_golem) ||
           (target.has(dkk_effect::fire_heals) && (element & 1));
}

// Damage that is not a weapon's (fxengine.pas, InflictNonWeaponDamage): the
// target's resistances weigh it by its element (1 fire, 2 cold, 4
// electricity, 8 magic, 16 acid), then a save halves or negates it
// (defended_harm).
void Session::nonweapon_damage(Fighter &target, int damage, int save_result, bool saved,
                               int element, int caster_level, int spell_number, bool single_target)
{
    if (absorbs_harm(target, element))
    {
        absorb_harm(target, damage, element);
        return;
    }
    const auto defended = defended_harm(target, damage, save_result, saved, element, caster_level,
                                        spell_number, single_target, damage_dice_, fight_dice());
    if (defended.image_took_it)
    {
        lose_image(target);
    }
    if (auto *soak = target.find_effect(effect::druid_fire_protection);
        soak && (defended.soak_spent > 0 || defended.soak_ends))
    {
        soak->strength -= defended.soak_spent;
        if (defended.soak_ends)
        {
            target.remove(effect::druid_fire_protection);
        }
    }
    if (defended.unaffected)
    {
        log({"{name} is unaffected.", {{"name", target.name}}});
    }
    if (defended.grown > 0)
    {
        target.hit_points += defended.grown;
        log({"{name} grows.", {{"name", target.name}}});
    }
    if (defended.regenerates_soon)
    {
        target.numbers.effects.push_back({effect::regenerating_soon, 3, 0, true});
    }
    if (defended.damage <= 0)
    {
        return;
    }
    log({"{name} takes {damage}.",
         {{"name", target.name}, {"damage", std::to_string(defended.damage)}}});
    inflict(target, defended.damage);
}

// What an iron golem, a flesh golem or one that fire heals does with a
// non-weapon harm (absorbs_harm).
void Session::absorb_harm(Fighter &target, int damage, int element)
{
    if (target.has(effect::iron_golem))
    {
        iron_golem_absorbs(target, damage, element);
        return;
    }
    if (target.has(dkk_effect::flesh_golem))
    {
        if ((element & 4) && (element & 8))
        {
            target.hit_points = std::min(target.hit_points + std::max(damage_dice_, 0),
                                         target.numbers.max_hit_points);
            log({"{name} is healed.", {{"name", target.name}}});
        }
        else if ((element & 3) && (element & 8))
        {
            target.numbers.effects.push_back({effect::slowed, roll(6) + roll(6), 255, false});
            log({"{name} is slowed.", {{"name", target.name}}});
        }
        return;
    }
    if (element & 8)
    {
        target.hit_points = std::min(target.hit_points + damage, target.numbers.max_hit_points);
        log({"{name} is healed.", {{"name", target.name}}});
    }
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
                                         Dice &&dice) const
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

// Secret's iron golem takes no damage but a weapon's (effhndlr.pas,
// EffIronGolem, in its reconstruction): magical fire heals it by the damage,
// up to its maximum, before any save; electricity slows it for 3 rounds.
void Session::iron_golem_absorbs(Fighter &golem, int damage, int element)
{
    if ((element & 1) && (element & 8))
    {
        golem.hit_points = std::min(golem.hit_points + damage, golem.numbers.max_hit_points);
        log({"{name} is healed.", {{"name", golem.name}}});
    }
    else if (element & 4)
    {
        golem.numbers.effects.push_back({effect::slowed, 3, 255, false});
        log({"{name} is slowed.", {{"name", golem.name}}});
    }
    else
    {
        log({"{name} is unaffected.", {{"name", golem.name}}});
    }
}

// A charmed creature fights for its charmer (effhndlr.pas, the charm).
void Session::charm(Fighter &target, Side charmer)
{
    target.own_side = target.side;
    target.side = charmer;
    target.fleeing = false;
}

// Sleep (magic.pas, the sleep): 4d4 levels' worth around the aim, the nearest
// first, allies too; a creature costs 1 up to level 1, 2 at 2, 4 at 3, 6 at 4,
// 10 at 5 for a monster and 20 otherwise, 20 from 6 up, spent before its
// immunities are tried. Those already asleep and the animated dead are
// passed over.
void Session::cast_sleep(int level, Cell aim)
{
    const int pool = roll(4) + roll(4) + roll(4) + roll(4);
    const auto &known = *game_spell(21);
    for (const auto id : sleep_takes(aim, pool))
    {
        add_with_message(*find(id), {effect::sleep, 5 * level, level, false}, false, false, 0,
                         level, &known);
    }
}

// Whom a Sleep of this pool of levels at a square puts to sleep, the
// nearest first (cast_sleep).
std::vector<EntityId> Session::sleep_takes(Cell aim, int pool) const
{
    std::vector<EntityId> taken;
    for (const auto id : within(aim, game_spell(21)->area & 7))
    {
        const auto &f = *find(id);
        if (f.has(effect::sleep) || f.has(effect::animated))
        {
            continue;
        }
        const int highest = f.numbers.level;
        const int cost = highest <= 1   ? 1
                         : highest == 2 ? 2
                         : highest == 3 ? 4
                         : highest == 4 ? 6
                         : highest == 5 ? (f.numbers.race == 0 ? 10 : 20)
                                        : 20;
        if (cost > pool)
        {
            continue;
        }
        pool -= cost;
        taken.push_back(id);
    }
    return taken;
}

// Whether a carrier of Silence stands within a square of it, itself
// included (effhndlr.pas, EffSilence's aura).
bool Session::silenced(const Fighter &f) const
{
    return std::any_of(fighters_.begin(), fighters_.end(),
                       [&](const Fighter &carrier)
                       {
                           return carrier.able() && carrier.has(effect::silence) &&
                                  (carrier.id == f.id || distance(carrier, f) <= 1);
                       });
}

// Prayer: within 6 squares of the first one praying, in its sight, its own
// side gets +1 to attacks and saves and the other side -1 (effhndlr.pas,
// EffPrayer, EffAlliedPrayer).
int Session::prayer_bonus(const Fighter &f) const
{
    for (const auto &carrier : fighters_)
    {
        const auto *node = carrier.able() ? carrier.find_effect(effect::prayer) : nullptr;
        if (!node ||
            (carrier.id != f.id && (distance(carrier, f) > 6 || !sees(board_, carrier, f))))
        {
            continue;
        }
        const Side praying = (node->strength & 0x10) ? Side::opposition : Side::party;
        return f.side == praying ? 1 : -1;
    }
    return 0;
}

// Protected by its own spell, or by a 10' radius one within a square of it
// (effhndlr.pas, 45 and 46).
bool Session::protected_from(const Fighter &f, int code, int aura) const
{
    if (f.has(code))
    {
        return true;
    }
    return std::any_of(fighters_.begin(), fighters_.end(),
                       [&](const Fighter &carrier)
                       {
                           return carrier.able() && carrier.has(aura) &&
                                  (carrier.id == f.id || distance(carrier, f) <= 1);
                       });
}

// An evil or good attacker 2 worse against one only a 10' radius protects
// (its own spell already counts in attack_modifier).
bool Session::aura_protects(const Fighter &attacker, const Fighter &defender) const
{
    const int alignment = attacker.numbers.alignment;
    const bool evil = alignment == 2 || alignment == 5 || alignment == 8;
    const bool good = alignment == 0 || alignment == 3 || alignment == 6;
    return (evil && !defender.has(effect::protection_from_evil) &&
            protected_from(defender, effect::protection_from_evil,
                           effect::protection_from_evil_aura)) ||
           (good && !defender.has(effect::protection_from_good) &&
            protected_from(defender, effect::protection_from_good,
                           effect::protection_from_good_aura));
}

// A mirror image takes the harm on 1d(images + 1) above 1, and is gone
// (effhndlr.pas, EffMirrorImage).
bool Session::mirror_absorbs(Fighter &target)
{
    const auto *images = target.find_effect(effect::mirror_image);
    if (!images || !image_takes_it(*images, fight_dice()))
    {
        return false;
    }
    lose_image(target);
    return true;
}

// The image that took the harm is gone.
void Session::lose_image(Fighter &target)
{
    log({"{name} loses an image.", {{"name", target.name}}});
    auto *images = target.find_effect(effect::mirror_image);
    const bool packed = game_of(identity_) == GoldBoxGame::dark_queen_of_krynn;
    const int count = packed ? images->strength >> 5 : images->strength;
    if (count <= 1)
    {
        target.remove(effect::mirror_image);
    }
    else
    {
        images->strength = packed ? ((count - 1) << 5) | (images->strength & 0x1F) : count - 1;
    }
}

// The creature standing on a square, 0 for none.
EntityId Session::standing_at(Cell cell) const
{
    for (const auto &f : fighters_)
    {
        const auto mine = squares_of(f);
        if (f.able() && std::find(mine.begin(), mine.end(), cell) != mine.end())
        {
            return f.id;
        }
    }
    return 0;
}

// Lightning strikes a square: one standing there saves against spells for
// half; a wall turns the bolt (magic.pas, LightningAttackAt). Lightning Bolt's
// is electricity and magic (element 12); a breath's is not a spell.
bool Session::lightning_at(Cell cell, int damage, int level, int save, int element,
                           int spell_number)
{
    if (const auto id = standing_at(cell))
    {
        auto &target = *find(id);
        const bool saved = saving_throw(target, save, 0, nullptr, element);
        nonweapon_damage(target, damage, 2, saved, element, level, spell_number, false);
    }
    return turns_bolt(cell);
}

// Whether a bolt striking a square turns there: a wall stands on it.
bool Session::turns_bolt(Cell cell) const
{
    return board_.contains(cell) && board_.at(cell) == rules::Terrain::obstacle;
}

// The bolt runs on from the aim, away from the caster (magic.pas,
// LightningPath): each leg goes to the next creature, the field's edge (where
// the bolt ends), rough ground, or the end of its length; a wall turns it back
// past the caster, costing 8 more when the wall is within 4 squares of it.
// Each leg takes the whole distance walked so far from what is left, as the
// original counts it.
void Session::lightning_path(Cell origin, Cell aim, int length, int damage, int level, bool bounce,
                             int save, int element, int spell_number)
{
    const auto strike = [&](Cell cell)
    {
        return lightning_at(cell, damage, level, save, element, spell_number);
    };
    bolt_walk(origin, aim, length, bounce, strike);
}

// The way lightning_path's bolt runs, telling `strike` of each square it
// strikes, which answers whether the bolt turns there; Treasures' bolt runs
// its own way (treasures_bolt_walk). The squares it walks take whoever
// stands on them as it goes.
template <typename Strike>
void Session::bolt_walk(Cell origin, Cell aim, int length, bool bounce, Strike &&strike) const
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
                                  Strike &&strike) const
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

// Stinking Cloud (magic.pas, SpellStinkingCloud): a cloud on the aim and the
// squares east, south-east and south of it, lasting the caster's level in
// rounds; those in it breathe it at once. Curse's Cloudkill (SpellCloudkill)
// lays its cloud on the aim and the eight squares around it.
void Session::lay_cloud(Fighter &caster, int level, Cell aim, CloudKind kind)
{
    // The caster's clouds of the kind before the last one laid, as the
    // original counts them.
    int index = 0;
    for (std::size_t n = 0; n + 1 < clouds_.size(); ++n)
    {
        index += clouds_[n].caster == caster.id && clouds_[n].kind == kind ? 1 : 0;
    }
    const auto cloud = cloud_over(caster.id, index, aim, kind);
    const auto squares = cloud.squares();
    clouds_.push_back(cloud);
    if (kind == CloudKind::blades)
    {
        // The blades whirl three rounds a caster level, and those in them as
        // they rise may save (fxengine.pas, ConsiderTerrainEffects).
        caster.numbers.effects.push_back(
            {effect::blade_barrier, 3 * level, level + index * 16, true});
        log({"{name} raises a barrier of whirling blades.", {{"name", caster.name}}});
        for (const auto square : squares)
        {
            if (const auto id = standing_at(square))
            {
                cut_by_blades(*find(id), true);
            }
        }
        return;
    }
    const bool stinking = kind == CloudKind::stinking;
    caster.numbers.effects.push_back(
        {stinking ? effect::stinking_cloud : effect::cloudkill, level, level + index * 16, true});
    log({stinking ? "{name} creates a noxious cloud." : "{name} creates a poisonous cloud.",
         {{"name", caster.name}}});
    for (const auto square : squares)
    {
        if (const auto id = standing_at(square))
        {
            breathe_cloud(*find(id));
        }
    }
}

// A cloud of the kind laid at a square, covering its squares on the field
// that no wall stands on.
Cloud Session::cloud_over(EntityId caster, int index, Cell aim, CloudKind kind) const
{
    Cloud cloud{caster, index, aim, {}, kind};
    const auto squares = cloud.squares();
    for (std::size_t n = 0; n < squares.size(); ++n)
    {
        cloud.covers[n] =
            board_.contains(squares[n]) && board_.at(squares[n]) != rules::Terrain::obstacle;
    }
    return cloud;
}

bool Session::in_cloud(const Fighter &f, CloudKind kind) const
{
    const auto mine = squares_of(f);
    return std::any_of(clouds_.begin(), clouds_.end(),
                       [&mine, kind](const Cloud &cloud)
                       {
                           if (cloud.kind != kind)
                           {
                               return false;
                           }
                           const auto squares = cloud.squares();
                           for (std::size_t n = 0; n < squares.size(); ++n)
                           {
                               if (cloud.covers[n] &&
                                   std::find(mine.begin(), mine.end(), squares[n]) != mine.end())
                               {
                                   return true;
                               }
                           }
                           return false;
                       });
}

// In Secret and Pools of Darkness the undead and the unliving (the record's
// flag) breathe a stinking cloud unharmed (fxengine.pas,
// ConsiderTerrainEffects, in their reconstructions).
bool Session::breathes_clouds_freely(const Fighter &f) const
{
    return secret_engine() &&
           (f.numbers.undead_level > 0 || (f.numbers.traits & trait::unliving) != 0);
}

// What the clouds do to one in them (fxengine.pas, ConsiderTerrainEffects).
// Pools of Darkness's has its own stench and cloudkill, and passes over the
// stench as the round ends (combat.pas, EndOfRound, skipCloud).
void Session::breathe_cloud(Fighter &f, bool round_ends)
{
    if (darkness())
    {
        if (!round_ends)
        {
            breathe_darkness_stench(f);
        }
        breathe_darkness_cloudkill(f);
    }
    else
    {
        breathe_stinking_cloud(f);
        breathe_cloudkill(f);
    }
    cut_by_blades(f, false);
    if (f.able() && in_cloud(f, CloudKind::acid))
    {
        log({"{name} is burned by acid.", {{"name", f.name}}});
        nonweapon_damage(f, roll(6), 0, false, 16, 0, 0, false);
    }
}

// The level of the caster of the first cloud of the kind, for the magic
// resistance its harm meets; 12 once the caster is gone.
int Session::cloud_caster_level(CloudKind kind, int spell_number) const
{
    const auto cloud = std::find_if(clouds_.begin(), clouds_.end(),
                                    [kind](const Cloud &c)
                                    {
                                        return c.kind == kind;
                                    });
    const auto *caster = cloud != clouds_.end() ? find(cloud->caster) : nullptr;
    return caster ? caster_level(*caster, *game_spell(spell_number), game_of(identity_)) : 12;
}

// Pools of Darkness's Blade Barrier cuts one in it for 8d8, which magic
// resistance may turn away; only as the blades rise may it save
// (fxengine.pas, ConsiderTerrainEffects).
void Session::cut_by_blades(Fighter &f, bool may_save)
{
    if (!f.able() || !in_cloud(f, CloudKind::blades))
    {
        return;
    }
    const auto blades = std::find_if(clouds_.begin(), clouds_.end(),
                                     [](const Cloud &c)
                                     {
                                         return c.kind == CloudKind::blades;
                                     });
    const auto *caster = find(blades->caster);
    const int level = cloud_caster_level(CloudKind::blades, 301);
    if (resists_magic_at_all(f) && magic_resists(f, level, roll(100)))
    {
        log({"{name} is unaffected.", {{"name", f.name}}});
        return;
    }
    if (may_save && saving_throw(f, 4, 0, caster))
    {
        return;
    }
    log({"{name} is cut by the blades.", {{"name", f.name}}});
    int damage = blade_dice.bonus;
    for (int die = 0; die < blade_dice.count; ++die)
    {
        damage += roll(blade_dice.sides);
    }
    nonweapon_damage(f, damage, 0, false, 0, level, 301, false);
}

// Cloudkill kills one of level 0 to 4 outright with its poison; one of level
// 5 saves against it at -4, one of level 6 at 0; one of a higher level breathes
// it unharmed.
void Session::breathe_cloudkill(Fighter &f)
{
    const int level = f.numbers.level;
    if (!f.able() || !in_cloud(f, CloudKind::deadly) || level > 6 ||
        (level >= 5 && saving_throw(f, 0, level == 5 ? -4 : 0)))
    {
        return;
    }
    log({"{name} is poisoned.", {{"name", f.name}}});
    f.numbers.effects.push_back({effect::poisoned, 0, 255, false});
    kill(f);
}

// Pools of Darkness's cloudkill (fxengine.pas, ConsiderTerrainEffects): one
// its record calls proof against poison breathes it unharmed; one of 0 to 4
// Hit Dice dies of its poison, one of 5 saves at -4, one of 6 at 0, and one
// of more takes 1d10, magical and poisonous.
void Session::breathe_darkness_cloudkill(Fighter &f)
{
    if (!f.able() || !in_cloud(f, CloudKind::deadly) ||
        (f.numbers.traits & trait::poison_proof) != 0)
    {
        return;
    }
    const int hit_dice = f.numbers.level;
    if (hit_dice > 6)
    {
        constexpr int magical_poison = 0x88;
        nonweapon_damage(f, roll(10), 0, false, magical_poison,
                         cloud_caster_level(CloudKind::deadly, 191), 191, false);
        return;
    }
    if (hit_dice >= 5 && saving_throw(f, 0, hit_dice == 5 ? -4 : 0))
    {
        return;
    }
    log({"{name} is poisoned.", {{"name", f.name}}});
    f.numbers.effects.push_back({effect::poisoned, 0, 255, false});
    kill(f);
}

// Pools of Darkness's stinking cloud (fxengine.pas, ConsiderTerrainEffects):
// the undead, the nauseous and one its record calls unliving or proof against
// poison breathe it unharmed; the rest save against paralysis, one that saves
// coughing for 1d4+1 rounds and losing the spell it was casting, one that
// fails helpless with nausea for 1d4+1.
void Session::breathe_darkness_stench(Fighter &f)
{
    constexpr int spared = trait::unliving | trait::poison_proof;
    if (!f.able() || !in_cloud(f, CloudKind::stinking) || f.numbers.undead_level > 0 ||
        f.has(effect::nausea) || (f.numbers.traits & spared) != 0)
    {
        return;
    }
    const auto &known = *game_spell(34);
    if (!saving_throw(f, 0, 0))
    {
        add_with_message(f, {effect::nausea, roll(4) + 1, 255, false}, false, false, 0, 0, &known);
        return;
    }
    add_with_message(f, {effect::coughing, roll(4) + 1, 255, false}, false, true, 0, 0, &known);
    if (f.pending_spell || f.aiming)
    {
        lose_spell(f);
        end_actions(f);
    }
}

// One in a stinking cloud saves against paralysis: saved, it coughs for the
// round; failed, nausea leaves it helpless for 1d4+1 rounds. One coughing,
// nauseous, animated or immune to poison breathes it unharmed.
void Session::breathe_stinking_cloud(Fighter &f)
{
    if (!f.able() || !in_cloud(f, CloudKind::stinking) || f.has(effect::coughing) ||
        f.has(effect::nausea) || f.has(effect::animated) || f.has(111) || f.has(125) ||
        breathes_clouds_freely(f))
    {
        return;
    }
    const auto &known = *game_spell(34);
    const bool native = game_of(identity_) == GoldBoxGame::dark_queen_of_krynn;
    // Native terrain dispatch uses the current actor's casting level:
    // the caster on initial placement, the creature on its later turn.
    const auto *acting = find(actor_);
    const int level = native ? caster_level(acting ? *acting : f, known, game_of(identity_)) : 0;
    if (saving_throw(f, 0, 0))
    {
        add_with_message(f, {effect::coughing, native ? roll(4) + 1 : 1, 255, false}, false, true,
                         0, level, &known);
        return;
    }
    add_with_message(f, {effect::nausea, roll(4) + 1, 255, false}, false, false, 0, level, &known);
}

// An effect ends before its time, its handler running as it goes: a charm
// lets go, the animated collapse, and the rest end as when their time runs out.
void Session::remove_with_handler(Fighter &f, std::size_t node)
{
    const auto ended = f.numbers.effects[node];
    f.numbers.effects.erase(f.numbers.effects.begin() + static_cast<std::ptrdiff_t>(node));
    if (ended.code == effect::charm)
    {
        f.side = f.own_side;
        return;
    }
    if (ended.code == effect::animated)
    {
        // It collapses, dead again, what it was before (effhndlr.pas, EffAnimateDead).
        if (f.before_animation)
        {
            auto numbers = *f.before_animation;
            numbers.spells.clear();
            numbers.effects = f.numbers.effects;
            f.numbers = numbers;
            f.before_animation.reset();
        }
        f.side = f.own_side;
        f.condition = Condition::dead;
        f.hit_points = 0;
        f.initiative = 0;
        log({"{name} collapses.", {{"name", f.name}}});
        return;
    }
    if (ended.code == effect::enlarged || ended.code == effect::strengthened)
    {
        recount_strength(f);
    }
    if (ended.on_remove)
    {
        expire(f, ended);
    }
}

// Dispel Magic (magic.pas, the dispel): each effect of a strength below 255
// has its level in the strength's low 4 bits, and goes on a d100 within 50%,
// 5% more for each level the caster has above it or 2% less for each below.
void Session::dispel(Fighter &target, int level)
{
    bool any = false;
    for (std::size_t node = 0; node < target.numbers.effects.size();)
    {
        const auto &e = target.numbers.effects[node];
        if (!dispellable(e))
        {
            ++node;
            continue;
        }
        if (roll(100) <= dispel_chance(e, level))
        {
            remove_with_handler(target, node);
            any = true;
            continue;
        }
        ++node;
    }
    if (any)
    {
        log({"{name} is dispelled.", {{"name", target.name}}});
    }
}

// Whether Dispel Magic may take an effect: one of a strength below 255, but
// never a creature's age, nor in Treasures love, a kampfult's crushing, a
// stone golem's or a yeti's grab (its magic.pas, SpellDispelMagic).
bool Session::dispellable(const Effect &e) const
{
    const bool treasures_kept =
        treasures() && (e.code == effect::in_love || e.code == effect::crushes ||
                        (e.code >= effect::stone_golem && e.code <= effect::grabs_6d4));
    return e.code != effect::age_in_years && !treasures_kept && e.strength < 255;
}

// The percent chance Dispel Magic of a caster's level takes an effect: 50,
// 5 more for each level the caster has above the effect's (its strength's
// low 4 bits, or Native's low 5) or 2 less for each below.
int Session::dispel_chance(const Effect &e, int caster_level) const
{
    const int effect_level =
        e.strength & (game_of(identity_) == GoldBoxGame::dark_queen_of_krynn ? 0x1F : 0x0F);
    return caster_level >= effect_level ? 50 + 5 * (caster_level - effect_level)
                                        : 50 - 2 * (effect_level - caster_level);
}

// Animate Dead (magic.pas, the animation): up to the caster's level of the
// dead of creature type 0, either side, whose squares are free, rise whole
// for the caster as undead (level 2) moving 6, sweeping no one, their spells
// forgotten; the animated die at exactly 0 Hit Points.
void Session::animate_dead(Fighter &caster, int level)
{
    int left = level;
    for (auto &f : fighters_)
    {
        if (left <= 0 || f.condition != Condition::dead || f.numbers.creature_type != 0 ||
            f.has(effect::animated) || !cell_free(f))
        {
            continue;
        }
        --left;
        f.before_animation = f.numbers;
        f.own_side = f.side;
        f.side = caster.side;
        f.numbers.undead_level = 2;
        f.numbers.movement = 6;
        f.numbers.attack_level = 0;
        f.numbers.spells.clear();
        f.numbers.creature_type = 4;
        f.condition = Condition::okay;
        f.hit_points = f.numbers.max_hit_points;
        f.overkill = 0;
        f.initiative = 0;
        f.numbers.effects.push_back(
            {effect::animated, 0, (f.own_side == Side::opposition ? 16 : 0) + level, true});
        log({"{name} is animated.", {{"name", f.name}}});
    }
}

// Slow Poison (magic.pas, the slow poison): one poisoned, even killed by it,
// stands again with at least 1 Hit Point while the spell lasts, losing a point
// every 10 minutes but never the last; when it wears off still poisoned, it dies.
void Session::slow_poison(Fighter &target, int level)
{
    if (!target.has(effect::poisoned) || target.has(effect::animated))
    {
        return;
    }
    target.hit_points = std::max(target.hit_points, 1);
    target.numbers.effects.push_back({effect::slow_poison, 60 * level, 255, true});
    log({"{name} is affected by {spell}.",
         {{"name", target.name}, {"spell", std::string(game_spell(26)->name), true}}});
    revive(target);
    target.numbers.effects.push_back({effect::slow_poison_tick, 10, 255, true});
}

// The poisoned dead stand when their square is free, or try again a minute
// later (effhndlr.pas, EffDelayedRevival).
void Session::revive(Fighter &f)
{
    if (f.condition == Condition::okay)
    {
        return;
    }
    if (cell_free(f))
    {
        stand_up(f, std::max(f.hit_points, 1), "{name} gets back up.");
        return;
    }
    f.numbers.effects.push_back({effect::revival, 1, 255, true});
}

// The original's AI, a turn (combatai.pas, CombatAiControl): the approach
// pattern, then an item, a spell begun earlier, turning undead, a spell, and
// else the fight: lock onto an enemy and move and attack until the turn is
// done. Morale is checked as every turn starts (next_actor).
void Session::ai_turn(Fighter &f, bool magic)
{
    f.fought = false;
    // Native's final Aurak form its frenzy's timer made (raw 213: the blazing
    // shape without the slain form's zap) ends its turn at once (aiturn.c).
    if (game_of(identity_) == GoldBoxGame::dark_queen_of_krynn &&
        f.has(effect::aurak_invulnerable) && !f.has(effect::aurak_zap))
    {
        end_actions(f);
        return;
    }
    // The pattern: kept 3 times in 4 while it is 1 to 4; rerolled 1 to 4,
    // or 5 to 6 one time in 8 (CombatAiControl).
    if (f.stance == 0 || f.stance > 4 || roll(4) == 1)
    {
        f.stance = roll(8) == 8 ? roll(2) + 4 : roll(4);
    }
    if (f.aiming)
    {
        if (!ai_aim(f))
        {
            lose_spell(f);
            end_actions(f);
        }
        return;
    }
    if (ai_use_item(f))
    {
        end_actions(f);
        return;
    }
    const auto undead_in_sight = std::any_of(fighters_.begin(), fighters_.end(),
                                             [&](const Fighter &o)
                                             {
                                                 return o.side != f.side && o.able() && !o.turned &&
                                                        o.numbers.undead_level > 0 &&
                                                        sees(board_, f, o);
                                             });
    // Pools of Darkness's AI turns undead only while it does not flee
    // (combatai.pas, TurnUndeadAi).
    if (may_turn(f) && undead_in_sight &&
        !((darkness() || game_of(identity_) == GoldBoxGame::dark_queen_of_krynn) && f.fleeing))
    {
        turn_undead(f);
        return;
    }
    if (ai_cast(f, magic))
    {
        return;
    }
    equip_best(f);
    while (f.able() && f.initiative > 0)
    {
        if (!acquire_target(f, 255, true, false) || f.initiative <= 0)
        {
            finish_round(f);
            return;
        }
        if (ai_move_attack(f))
        {
            return;
        }
    }
}

// The enemies standing within a range, in sight or through walls.
std::vector<EntityId> Session::enemies_within(const Fighter &f, int range, bool sight) const
{
    std::vector<EntityId> found;
    for (const auto &o : fighters_)
    {
        if (o.side != f.side && o.able() && distance(f, o) <= range &&
            (!sight || sees(board_, f, o)))
        {
            found.push_back(o.id);
        }
    }
    return found;
}

// Locking onto an enemy (cbtacts.pas, AcquireTarget): a lock still on an
// enemy that may be singled out stays; else up to 20 random picks among the
// enemies in range and sight that may be singled out; else, unless a new
// lock was forced, among those in range through walls, any of them when
// relaxed.
bool Session::acquire_target(Fighter &f, int range, bool relaxed, bool force_new)
{
    if (const auto *locked = find(f.lock);
        locked && !force_new && locked->side != f.side && locked->able() && can_target(f, *locked))
    {
        return true;
    }
    f.lock = 0;
    for (int pass = 1; pass <= 2; ++pass)
    {
        const bool through_walls = pass == 2 && !force_new;
        auto candidates = enemies_within(f, range, !through_walls);
        const auto *taunt = f.find_effect(effect::kender_taunted);
        const auto yeller = taunt ? static_cast<EntityId>(taunt->strength) : EntityId{};
        if (const auto target = std::ranges::find(candidates, yeller); target != candidates.end())
        {
            std::iter_swap(candidates.begin(), target);
        }
        for (int tries = 0; tries < 20 && !candidates.empty(); ++tries)
        {
            auto &pick = candidates[tries == 0 && yeller != 0 && candidates.front() == yeller
                                        ? 0
                                        : static_cast<std::size_t>(
                                              roll(static_cast<int>(candidates.size())) - 1)];
            if (pick == 0)
            {
                continue;
            }
            if (can_target(f, *find(pick)) || (through_walls && relaxed))
            {
                f.lock = pick;
                return true;
            }
            pick = 0;
            if (std::all_of(candidates.begin(), candidates.end(),
                            [](EntityId id)
                            {
                                return id == 0;
                            }))
            {
                break;
            }
        }
    }
    return false;
}

// Whether its readied weapon reaches past the next square: a missile weapon,
// throwables too.
bool Session::missile_in_hand(const Fighter &f) const
{
    return f.numbers.missile.has_value();
}

// Nothing more to do (combatai.pas, FinishRound): a helpless creature, one
// with a missile weapon or one without a turn left ends its actions; another
// guards.
void Session::finish_round(Fighter &f)
{
    if (f.helpless() || missile_in_hand(f) || f.initiative <= 0)
    {
        end_actions(f);
        return;
    }
    end_actions(f);
    f.guarding = true;
    log({"{name} guards.", {{"name", f.name}}});
}

// Moving and attacking (combatai.pas, AiMoveAttack): up to 20 passes, each a
// step or an attack on an enemy within its weapon's reach. A fleeing creature
// spends its movement running first. True when its turn is done; false when
// it downed its target with attacks left, and locks onto another.
bool Session::ai_move_attack(Fighter &f)
{
    monster_specials(f);
    if (f.initiative <= 0 || !f.able())
    {
        return true;
    }
    int last_direction = 8, stuck = 0;
    for (int pass = 1; pass <= 20; ++pass)
    {
        if (f.fleeing)
        {
            while (f.half_moves > 0 && f.initiative > 0 && f.able())
            {
                if (!ai_step(f, last_direction, stuck))
                {
                    break;
                }
            }
        }
        if (f.initiative <= 0 || !f.able())
        {
            return true;
        }
        const int reach = missile_in_hand(f) ? f.numbers.missile->range : 1;
        // The lock, when it is an enemy within reach and in sight. The
        // original skipped any lock on the party's side, so monsters never
        // struck theirs first (an obvious bug, fixed).
        Fighter *target = find(f.lock);
        if (target && (!target->able() || target->side == f.side || !can_target(f, *target) ||
                       distance(f, *target) > reach || !sees(board_, f, *target)))
        {
            target = nullptr;
        }
        if (!target)
        {
            const auto in_reach = enemies_within(f, reach, true);
            if (in_reach.empty())
            {
                if (!acquire_target(f, 255, false, false))
                {
                    finish_round(f);
                    return true;
                }
                if (!ai_step(f, last_direction, stuck))
                {
                    return true;
                }
                continue;
            }
            auto &pick = *find(
                in_reach[static_cast<std::size_t>(roll(static_cast<int>(in_reach.size())) - 1)]);
            // A missile weapon with an enemy next to it: it readies another
            // weapon, and its turn ends (AiMoveAttack).
            if (missile_in_hand(f) && !f.numbers.missile->throwable && enemy_next_to(f))
            {
                equip_best(f);
                finish_round(f);
                return true;
            }
            // The original struck a pick next to it even one it may not single
            // out (an obvious bug, fixed).
            if (!can_target(f, pick))
            {
                continue;
            }
            target = &pick;
        }
        const auto verb = attack_verb(f, *target);
        if (!verb)
        {
            finish_round(f);
            return true;
        }
        note_attack(f, *target);
        f.fought = true;
        f.routing = false;
        if (*verb == "melee")
        {
            if (!sweep(f, *target))
            {
                make_attack(f, *target, false);
            }
        }
        else
        {
            shoot(f, *target);
        }
        if (f.attacks() <= 0 || f.initiative <= 0)
        {
            end_actions(f);
            return true;
        }
        if (!target->able())
        {
            return false;
        }
    }
    // The original ran on through the 21st pass (an obvious bug, fixed).
    finish_round(f);
    return true;
}

// Pools of Darkness's gazes, played only where the game's option asks
// (docs/CONFIGURATION.md, darkness_gazes): the paralyzing gaze holds its
// lock for 10 rounds unless it saves against paralysis at +3 or meets gazes
// unharmed, a mirror turning a reflectable gaze back on the gazer
// (effhndlr.pas, EffParalyzeGaze); the charming gaze casts Charm Person at
// it, saved against as the spell is (magic.pas, EffGazeCharm).
void Session::darkness_gazes(Fighter &f)
{
    auto *lock = find(f.lock);
    const auto in_sight = [&]
    {
        return f.able() && lock && lock->able() && lock->side != f.side && can_target(f, *lock) &&
               sees(board_, f, *lock);
    };
    if (f.has(effect::darkness_paralyzing_gaze) && in_sight())
    {
        log({"{name} gazes.", {{"name", f.name}}});
        Fighter *victim = lock;
        if (f.has(effect::reflectable_gaze) && lock->numbers.mirror)
        {
            log({"{name} reflects it.", {{"name", lock->name}}});
            victim = &f;
        }
        if (!saving_throw(*victim, 1, 3, &f) && !victim->has(effect::gaze_proof))
        {
            log({"{name} is paralyzed.", {{"name", victim->name}}});
            victim->numbers.effects.push_back({effect::paralysis, 10, 12, false});
        }
    }
    lock = find(f.lock);
    if (f.has(effect::darkness_charming_gaze) && in_sight())
    {
        log({"{name} gazes.", {{"name", f.name}}});
        const bool saved = saving_throw(*lock, 4, 0, &f) || lock->has(effect::gaze_proof);
        if (add_with_message(*lock,
                             {effect::charm, 0, (f.side == Side::opposition ? 128 : 0) + 12, true},
                             true, saved, 0, 12, game_spell(10)))
        {
            charm(*lock, f.side);
        }
    }
}

// A monster's own specials as its AI moves and attacks (combatai.pas,
// MONSTER_SPECIAL): the petrifying gaze, the charming gaze, the lightning
// breath and the ankheg's acid, at its lock.
void Session::monster_specials(Fighter &f)
{
    if (game_of(identity_) == GoldBoxGame::death_knights_of_krynn)
    {
        if (f.has(dkk_effect::death_breath) && f.able() && f.initiative > 0)
        {
            (void)secret_breath(f, dkk_effect::death_breath, 8, 0x60, 1);
        }
        if (f.has(effect::darkness_charming_gaze) && f.able() && f.initiative > 0)
        {
            if (auto *target = secret_target(f, 6, false))
            {
                log({"{name} gazes.", {{"name", f.name}}});
                if (!saving_throw(*target, 4, 0, &f) &&
                    add_with_message(*target, {effect::charm, 0, 255, true}, false, false, 0,
                                     f.numbers.level, game_spell(10)))
                {
                    charm(*target, f.side);
                }
            }
        }
        if (f.has(dkk_effect::smoke) && f.able() && f.initiative > 0)
        {
            Fighter *best = nullptr; // Borrowed from fighters_ for this query only.
            int rating = 0;
            for (auto &target : fighters_)
            {
                if (target.side == f.side || !target.able() || distance(f, target) > 1 ||
                    target.has(dkk_effect::smoked))
                {
                    continue;
                }
                const int score = 60 - target.numbers.thac0 +
                                  target.attacks_left[0] * (6 * target.numbers.attacks[0].dice +
                                                            target.numbers.attacks[0].modifier);
                if (score > rating)
                {
                    best = &target;
                    rating = score;
                }
            }
            if (best && !saving_throw(*best, 3, 0, &f))
            {
                (void)add_with_message(*best, {dkk_effect::smoked, 0, 0, false}, false, false, 0,
                                       f.numbers.level, nullptr);
                log({"{name} is blinded by smoke.", {{"name", best->name}}});
            }
        }
    }
    auto *lock = find(f.lock);
    // Gateway's gazes only at one it sees within 20 squares, not one hidden
    // (its EffPetrifyingGaze).
    const bool gaze_reaches =
        !gateway() || (lock && !lock->has(effect::invisible) && !lock->has(effect::dust) &&
                       distance(f, *lock) <= 20 && sees(board_, f, *lock));
    if (f.has(83) && lock && lock->able() && gaze_reaches)
    {
        // The petrifying gaze, at any range; a mirror turns a gaze that can be
        // reflected back on the gazer (effhndlr.pas, EffPetrifyingGaze).
        log({"{name} gazes.", {{"name", f.name}}});
        Fighter *victim = lock;
        if (f.has(127) && lock->numbers.mirror)
        {
            log({"{name} reflects it.", {{"name", lock->name}}});
            victim = &f;
        }
        if (!saving_throw(*victim, 1, 0, &f))
        {
            victim->condition = Condition::stoned;
            victim->hit_points = 0;
            victim->initiative = 0;
            log({"{name} is stoned.", {{"name", victim->name}}});
            end_effects_at_zero(*victim);
            went_down(*victim);
        }
        if (!f.able())
        {
            return;
        }
    }
    lock = find(f.lock);
    // Effect 84 is the charming gaze in Pool of Radiance, and the kit's
    // vampires' (its 124), only: Curse's and Gateway's shambling mounds carry
    // it as their absorbing of electricity (their effhndlr.pas), which charms
    // nothing.
    const bool charming = game_of(identity_) == GoldBoxGame::pool_of_radiance || kit();
    if (f.has(84) && charming && lock && lock->able() && can_target(f, *lock) &&
        sees(board_, f, *lock))
    {
        // The charming gaze: saved against at -2, and always by one that
        // meets gazes unharmed. The original's reach was an unset value; it
        // reaches what the gazer sees here (an obvious bug, fixed).
        log({"{name} gazes.", {{"name", f.name}}});
        // The kit's gaze casts Charm Person, saved against as the spell is
        // (fxcombat.c, fx_spell_gaze).
        const int gaze_modifier = kit() ? 0 : -2;
        const bool saved =
            saving_throw(*lock, 4, gaze_modifier, &f) || lock->has(effect::gaze_proof);
        if (add_with_message(*lock,
                             {effect::charm, 0, (f.side == Side::opposition ? 128 : 0) + 12, true},
                             true, saved, 0, 12, game_spell(10)))
        {
            charm(*lock, f.side);
        }
    }
    if (darkness() && gazes_played_)
    {
        darkness_gazes(f);
    }
    if (const auto *breath = f.find_effect(88); breath && (round_ == 1 || roll(100) > 50))
    {
        // The lightning breath: always in the first round, then half the time;
        // at an enemy within 4 squares and the dragon's magic-user level, for
        // its maximum Hit Points, saved against for half; 3, 2 or 1 breaths by
        // its strength (effhndlr.pas and magic.pas, EffBreatheLightning). With
        // no enemy in reach the original breathed on itself and spent the
        // breath (an obvious bug, fixed: it holds its breath).
        const int strength = breath->strength;
        const int reach =
            4 + f.numbers.class_levels[static_cast<std::size_t>(ClassSlot::magic_user)];
        if (acquire_target(f, reach, false, true))
        {
            log({"{name} breathes.", {{"name", f.name}}});
            const Cell aim = find(f.lock)->cell;
            const int sx = (aim.x > f.cell.x) - (aim.x < f.cell.x);
            const int sy = (aim.y > f.cell.y) - (aim.y < f.cell.y);
            const Cell first{f.cell.x + sx + (sx > 0 ? 1 : 0), f.cell.y + sy + (sy > 0 ? 1 : 0)};
            while (f.has(effect::invisible))
            {
                f.remove(effect::invisible);
            }
            const int damage = f.numbers.max_hit_points;
            lightning_at(first, damage, 12, 3);
            lightning_path(f.cell, first, 10, damage, 12, false, 3);
            // A breath that came back off a wall may have downed the dragon,
            // which loses the effect with its life.
            if (auto *left = f.find_effect(88); left && strength > 253)
            {
                left->strength = strength - 1;
            }
            else if (left)
            {
                f.remove(88);
            }
            end_actions(f);
            return;
        }
    }
    lock = find(f.lock);
    if (f.has(121) && roll(100) <= 25 && lock && lock->able() && distance(f, *lock) < 4)
    {
        // The ankheg's acid, once a fight: 8d4 acid, saved against for half
        // (effhndlr.pas, EffSpitAcid). The original left it untyped (an
        // obvious bug, fixed).
        end_actions(f);
        log({"{name} spits acid.", {{"name", f.name}}});
        int damage = 0;
        for (int die = 0; die < 8; ++die)
        {
            damage += roll(4);
        }
        nonweapon_damage(*lock, damage, 2, saving_throw(*lock, 3, 0, &f, 16), 16, 12, 0, false);
        f.remove(121);
        f.remove(80);
    }
    dark_queen_specials(f);
    curse_specials(f);
    secret_specials(f);
    kit_specials(f);
    darkness_specials(f);
    savage_frontier_specials(f);
    stone_golem_turn(f);
}

// A Treasures stone golem at its turn (effhndlr.pas, EffSlowAura,
// MONSTER_SPECIAL): stone, it slows an enemy it picks for 6 rounds, one not
// slowed already, and fights on; flesh, it counts down to stone again.
void Session::stone_golem_turn(Fighter &golem)
{
    auto *stone = golem.find_effect(effect::stone_golem);
    if (!stone || !golem.able())
    {
        return;
    }
    if (stone->strength != effect::golem_stone)
    {
        stone->strength = (stone->strength + 255) % 256;
        if (stone->strength == effect::golem_stone)
        {
            log({"{name} returns to stone.", {{"name", golem.name}}});
        }
        return;
    }
    if (!acquire_target(golem, 255, false, true))
    {
        return;
    }
    auto &target = *find(golem.lock);
    if (target.has(effect::slowed))
    {
        return;
    }
    log({"{name} slows {target}.", {{"name", golem.name}, {"target", target.name}}});
    target.numbers.effects.push_back({effect::slowed, 6, 255, false});
}

// Gateway's and Treasures' monsters' own powers at their turn (their
// effhndlr.pas, MONSTER_SPECIAL): Gateway's paralyzing gaze; Treasures' hell
// hound's fire and ice hound's cold, a manticore's spines and a gorgimera's
// stone breath.
void Session::savage_frontier_specials(Fighter &f)
{
    if (!f.able())
    {
        return;
    }
    auto *lock = find(f.lock);
    // At its lock, at any range: held for good unless saved against as
    // petrification (Gateway's EffParalyzingGaze).
    if (f.has(effect::gateway_paralyzing_gaze) && lock && lock->able())
    {
        log({"{name} gazes.", {{"name", f.name}}});
        if (!saving_throw(*lock, 1, 0, &f))
        {
            log({"{name} is paralyzed.", {{"name", lock->name}}});
            lock->numbers.effects.push_back({effect::paralysis, 0, 12, false});
        }
    }
    // 7 of fire or cold at one beside it, saved against for half
    // (EffHellHoundBreath, EffIceHoundBreath); the turn goes on.
    for (const auto [code, element] :
         {std::pair{effect::hell_hound_breath, 0x21}, std::pair{effect::ice_hound_breath, 0x22}})
    {
        if (f.has(code) && acquire_target(f, 12, false, false) && (lock = find(f.lock)) &&
            distance(f, *lock) <= 1)
        {
            log({element == 0x21 ? "{name} breathes fire." : "{name} breathes ice.",
                 {{"name", f.name}}});
            nonweapon_damage(*lock, 7, 2, saving_throw(*lock, 3, 0, &f, element & 3), element, 12,
                             0, false);
        }
    }
    // Six spines of 1d6 at one it sees within 10 squares (EffManticoreSpikes).
    if (f.has(effect::manticore_spines) && acquire_target(f, 10, false, true) &&
        (lock = find(f.lock)) && sees(board_, f, *lock))
    {
        log({"{name} fires its spines.", {{"name", f.name}}});
        auto &blow = f.numbers.attacks[0];
        const auto own = blow;
        blow = {own.rate_per_two_rounds, 1, 6, 0};
        f.attacks_left = {6, 0};
        f.attack_phase = 0;
        make_attack(f, *lock, false);
        blow = own;
        end_actions(f);
        return;
    }
    // Twice a fight, at one within 2 squares: stoned unless saved against as
    // petrification or proof against gazes (EffStoneBreath).
    if (f.has(effect::stone_breath) && uses(f, effect::stone_breath) < 2 &&
        acquire_target(f, 2, false, true) && (lock = find(f.lock)))
    {
        spend_use(f, effect::stone_breath);
        log({"{name} breathes.", {{"name", f.name}}});
        if (!lock->has(effect::gaze_proof) && !saving_throw(*lock, 1, 0, &f))
        {
            turn_to_stone(*lock);
        }
        end_actions(f);
    }
}

// The kit's monsters' breath as their AI attacks (aiturn.c, ai_attack;
// fxcombat.c and effects.c): a blue dragon breathes lightning as Pools of
// Darkness's does (its 91, the fight's 561); a green dragon breathes poison three times a
// fight, its maximum Hit Points along 3 squares at an enemy within them, half
// on a save against breath (90); a gorgon four times, along 6 squares two
// wide at an enemy within them but never through its own side, turning to
// stone each that fails a save against petrification (195). Each breath ends
// its turn.
void Session::kit_specials(Fighter &f)
{
    if (kit() && f.has(effect::blue_breath) && f.able() && f.initiative > 0 && lightning_breath(f))
    {
        return;
    }
    if (f.has(effect::eye_rays))
    {
        beholder_rays(f);
    }
    auto *poison_or_steam = f.find_effect(effect::poison_breath);
    if (!poison_or_steam)
    {
        poison_or_steam = f.find_effect(dq_effect::steam_breath);
    }
    if (auto *breath = poison_or_steam; breath && f.able() && f.initiative > 0)
    {
        breath->strength = std::min(breath->strength, 3);
        const auto targets = breath->strength > 0 && acquire_target(f, 3, false, true)
                                 ? path_area(f, find(f.lock)->cell, 3, 1)
                                 : std::vector<EntityId>{};
        if (!targets.empty())
        {
            --breath->strength;
            log({breath->code == dq_effect::steam_breath ? "{name} breathes steam."
                                                         : "{name} breathes poison.",
                 {{"name", f.name}}});
            for (const auto id : targets)
            {
                auto &victim = *find(id);
                nonweapon_damage(victim, f.numbers.max_hit_points, 2,
                                 saving_throw(victim, 3, 0, &f, 0x20), 0x20, 12, 0, false);
            }
            end_actions(f);
            return;
        }
    }
    if (auto *breath = f.find_effect(effect::gorgon_breath); breath && f.able() && f.initiative > 0)
    {
        breath->strength = std::min(breath->strength, 4);
        const auto targets = breath->strength > 0 && acquire_target(f, 6, false, true)
                                 ? path_area(f, find(f.lock)->cell, 6, 2)
                                 : std::vector<EntityId>{};
        const bool spares_friends = std::ranges::none_of(targets,
                                                         [&](EntityId id)
                                                         {
                                                             return find(id)->side == f.side;
                                                         });
        if (!targets.empty() && spares_friends)
        {
            --breath->strength;
            log({"{name} breathes.", {{"name", f.name}}});
            for (const auto id : targets)
            {
                auto &victim = *find(id);
                if (victim.able() && !saving_throw(victim, 1, 0, &f))
                {
                    turn_to_stone(victim);
                }
            }
            end_actions(f);
        }
    }
}

// Native ray facts: a fireshadow uses a narrow 13-square ray at most once
// per six rounds; the Eye of the Deep stuns along a three-square, two-wide path.
void Session::dark_queen_specials(Fighter &f)
{
    if (!f.able() || f.initiative <= 0)
    {
        return;
    }
    if (auto *breath = f.find_effect(dq_effect::baldric_breath))
    {
        if (round_ == 1 || breath->strength > 3)
        {
            breath->strength = 3;
        }
        if (breath->strength > 0)
        {
            const bool paralysis = roll(2) == 2;
            const auto *target = secret_target(f, 6, true);
            if (target)
            {
                const auto targets =
                    path_area(f, target->cell, paralysis ? 5 : 7, paralysis ? 4 : 3);
                if (!targets.empty() && std::ranges::none_of(targets,
                                                             [&](EntityId id)
                                                             {
                                                                 return find(id)->side == f.side;
                                                             }))
                {
                    log({paralysis ? "{name} breathes paralysis." : "{name} breathes cold.",
                         {{"name", f.name}}});
                    for (const auto id : targets)
                    {
                        auto &victim = *find(id);
                        const bool saved = saving_throw(victim, 3, 0, &f);
                        (void)add_with_message(victim, {effect::paralysis, 10, 12, false}, true,
                                               saved, paralysis ? 0 : 0x22, 12, nullptr);
                    }
                    --breath->strength;
                    end_actions(f);
                    return;
                }
            }
        }
    }
    const bool death_ray = f.has(dq_effect::disintegrating_ray);
    auto *ray = f.find_effect(death_ray ? dq_effect::disintegrating_ray : dq_effect::blinding_ray);
    if (!ray)
    {
        return;
    }
    if (death_ray && (round_ % 6 == 0 || ray->strength > 1))
    {
        ray->strength = 1;
    }
    if (death_ray && ray->strength <= 0)
    {
        return;
    }
    const auto *aim = secret_target(f, 6, true);
    if (!aim)
    {
        return;
    }
    const auto targets = path_area(f, aim->cell, death_ray ? 13 : 3, death_ray ? 1 : 2);
    if (targets.empty() || std::ranges::any_of(targets,
                                               [&](EntityId id)
                                               {
                                                   return find(id)->side == f.side;
                                               }))
    {
        return;
    }
    log({death_ray ? "{name} flashes a ray." : "{name} gazes.", {{"name", f.name}}});
    for (const auto id : targets)
    {
        auto &target = *find(id);
        const bool saved = saving_throw(target, death_ray ? 3 : 0, 0, &f);
        if (death_ray)
        {
            nonweapon_damage(target, roll(4) + roll(4) + roll(4) + roll(4), 1, saved, 0, 0, 0,
                             false);
            if (target.hit_points == 0)
            {
                put_out_of_play(target, Condition::gone);
                log({"{name} is disintegrated.", {{"name", target.name}}});
            }
        }
        else
        {
            (void)add_with_message(target, {effect::paralysis, roll(4) + roll(4), 255, false}, true,
                                   saved, 0, 0, nullptr);
        }
    }
    if (death_ray)
    {
        --ray->strength;
    }
    end_actions(f);
}

// The kit's effects as a creature's turn starts (combat.c, its turn;
// effects.c): a ghast's stench leaves each enemy beside it that fails a save
// against spells 2 worse at its attacks for the round (137, the kit's 148 on
// it), and a bulette strikes 4 times with its second attack, 8 one time in 4
// (99, as its AI readies its weapons).
void Session::kit_turn_start(Fighter &f)
{
    if (!f.able())
    {
        return;
    }
    if (f.has(effect::stench))
    {
        log({"{name} emits an evil stench.", {{"name", f.name}}});
        for (auto &other : fighters_)
        {
            if (other.side != f.side && other.able() && adjacent(f, other) &&
                !saving_throw(other, 4, 0, &f))
            {
                log({"{name} is sickened.", {{"name", other.name}}});
                other.numbers.effects.push_back({effect::sickened, 1, 0, false});
            }
        }
    }
    if (f.has(effect::bulette_attacks))
    {
        f.attacks_left[1] = roll(4) == 1 ? 8 : 4;
    }
}

// The target a Secret monster's power picks, as its AI aims a spell
// (cbtacts.pas, SelectSpellTarget, in its reconstruction): a new lock on an
// enemy it sees within range, in one try. A power aimed as a paralyzing spell
// is (its picker's spell 61) passes a helpless one over.
Fighter *Session::secret_target(Fighter &f, int range, bool skip_helpless)
{
    if (!acquire_target(f, range, false, true))
    {
        return nullptr;
    }
    auto *target = find(f.lock);
    return skip_helpless && target->helpless() ? nullptr : target;
}

// Secret's monsters' powers as each of their AI's turns starts (fxengine.pas,
// its condition 14, in its reconstruction), in its order: the petrifying gaze,
// a pyrohydra's fire, the umber hulk's confusing gaze, a hell hound's fire, a
// giant slug's acid and a dragon's breath. Only the breath ends its turn.
void Session::secret_specials(Fighter &f)
{
    if (f.has(effect::petrifying_gaze) && f.able())
    {
        petrifying_gaze(f);
    }
    // A pyrohydra breathes 8 of fire at its lock beside it, from the second
    // round for as many rounds as its first attack's rate (EffBreatheFireAdjacent).
    auto *lock = find(f.lock);
    const bool fire_round =
        round_ > 1 && static_cast<int>(round_) - 1 <= f.numbers.attacks[0].rate_per_two_rounds;
    if (f.has(effect::fire_beside) && f.able() && fire_round && lock && lock->able() &&
        distance(f, *lock) < 2)
    {
        log({"{name} breathes fire.", {{"name", f.name}}});
        nonweapon_damage(*lock, 8, 2, saving_throw(*lock, 3, 0, &f, 0x21), 0x21, 12, 0, false);
    }
    // The umber hulk's gaze confuses one beside it for 1d10+2 rounds unless
    // it saves against spells (EffReflectSpell) or meets gazes unharmed.
    if (f.has(effect::confusing_gaze) && f.able())
    {
        if (auto *target = secret_target(f, 1, false))
        {
            log({"{name} gazes.", {{"name", f.name}}});
            if (!saving_throw(*target, 4, 0, &f) && !target->has(effect::gaze_proof))
            {
                (void)add_with_message(*target, {235, roll(10) + 2, f.numbers.level, true}, true,
                                       false, 0, f.numbers.level, game_spell(182));
            }
        }
    }
    // A hell hound breathes 7 of fire at one beside it, every turn
    // (EffBreatheFireAtTarget).
    if (f.has(effect::fire_at_target) && f.able())
    {
        if (auto *target = secret_target(f, 1, false))
        {
            log({"{name} breathes fire.", {{"name", f.name}}});
            nonweapon_damage(*target, 7, 2, saving_throw(*target, 3, 0, &f, 0x21), 0x21, 12, 0,
                             false);
        }
    }
    // A giant slug spits 4d8 of acid at one within 6 squares: 1 time in 10 in
    // the first round, then 1 in 10 for each square nearer than 11 (EffSpitAtTarget).
    if (f.has(effect::spits_acid) && f.able())
    {
        if (auto *target = secret_target(f, 6, true))
        {
            const int chance = round_ == 1 ? 10 : (11 - distance(f, *target)) * 10;
            if (roll(100) <= chance)
            {
                log({"{name} spits acid.", {{"name", f.name}}});
                int damage = 0;
                for (int die = 0; die < 4; ++die)
                {
                    damage += roll(8);
                }
                nonweapon_damage(*target, damage, 2, saving_throw(*target, 3, 0, &f, 16), 16, 12, 0,
                                 false);
            }
            else
            {
                log({"{name} spits acid and misses.", {{"name", f.name}}});
            }
        }
    }
    // Maya's native cold cone reaches eight squares and is used once.
    if (champions() && f.has(effect::maya_breath) && f.able() && f.initiative > 0)
    {
        (void)secret_breath(f, effect::maya_breath, 8, 0x22);
    }
    // A white dragon breathes cold 7 squares long, a red one fire 9 long
    // (magic.pas, EffDragonBreath).
    if (f.has(effect::cold_breath) && f.able() && f.initiative > 0)
    {
        (void)secret_breath(f, effect::cold_breath, 7, 0x22);
    }
    if (f.has(effect::fire_breath) && f.able() && f.initiative > 0)
    {
        (void)secret_breath(f, effect::fire_breath, 9, 0x21);
    }
}

// A Secret gaze that turns to stone (effhndlr.pas, EffPetrifyingGaze, in its
// reconstruction): at an enemy within 6 squares that is not helpless, saved
// against as petrification. A target with a mirror turns a gaze that can be
// reflected back on the gazer. Unlimited Adventures' vampires and dracolisk
// meet every gaze unharmed (the kit's 126).
void Session::petrifying_gaze(Fighter &gazer)
{
    auto *target = secret_target(gazer, 6, true);
    if (!target)
    {
        return;
    }
    log({"{name} gazes.", {{"name", gazer.name}}});
    Fighter *victim = target;
    if (gazer.has(effect::reflectable_gaze) && target->numbers.mirror)
    {
        log({"{name} reflects it.", {{"name", target->name}}});
        victim = &gazer;
    }
    // The kit's and Pools of Darkness's immunity to gazes spares it
    // (effhndlr.pas, EffImmuneGaze).
    // The kit spares an iron golem too (its 79; effects.c, fx_stone_gaze).
    const bool spared =
        victim->has(effect::gaze_proof) || (kit() && victim->has(effect::iron_golem));
    if (!saving_throw(*victim, 1, 0, &gazer, 16) && !spared)
    {
        turn_to_stone(*victim);
    }
}

// A Secret dragon's breath (magic.pas, EffDragonBreath, in its reconstruction):
// three a fight, the effect's strength counting them down from the first
// round; aimed at an enemy within 6 squares that is not helpless, along a
// path of the length, three wide (or as wide as given), never while any
// creature of its own side stands in it; its maximum Hit Points to each (or
// the damage given), half on a save against breath. A breath ends its turn.
bool Session::secret_breath(Fighter &dragon, int code, int length, int element, int width,
                            std::optional<int> damage)
{
    auto *breath = dragon.find_effect(code);
    if (round_ == 1 || breath->strength > 3)
    {
        breath->strength = champions() && code == effect::maya_breath ? 1 : 3;
    }
    if (breath->strength <= 0)
    {
        return false;
    }
    const auto *target = secret_target(dragon, 6, true);
    if (!target)
    {
        return false;
    }
    const auto targets = path_area(dragon, target->cell, length, width);
    const bool spares_friends = std::ranges::none_of(targets,
                                                     [&](EntityId id)
                                                     {
                                                         return find(id)->side == dragon.side;
                                                     });
    if (targets.empty() || !spares_friends)
    {
        return false;
    }
    --breath->strength;
    log({"{name} breathes.", {{"name", dragon.name}}});
    const int harm = damage.value_or(dragon.numbers.max_hit_points);
    for (const auto id : targets)
    {
        auto &victim = *find(id);
        nonweapon_damage(victim, harm, 2, saving_throw(victim, 3, 0, &dragon, element), element, 12,
                         0, false);
    }
    end_actions(dragon);
    return true;
}

// Pools of Darkness's monsters' own powers as their AI attacks (fxengine.pas,
// MONSTER_SPECIAL, in its order, and magic.pas): the biggest red dragon
// breathes fire 9 squares long and three wide for 24d10+12, a blue one
// lightning, a black one acid 6 squares long and one wide, each three times a
// fight as Secret's dragons do (EffDragonBreath, EffBreatheLightning); its
// green dragon breathes as the kit's (poison_breath). Its paralyzing and
// charming gazes (114, 124) have handlers the original registers but never
// runs, so the gaze spider and the vampires never gaze, as there.
void Session::darkness_specials(Fighter &f)
{
    if ((!darkness() && !champions()) || !f.able() || f.initiative <= 0)
    {
        return;
    }
    if (f.has(effect::big_red_breath))
    {
        int fire = 12;
        for (int die = 0; die < 24; ++die)
        {
            fire += roll(10);
        }
        if (secret_breath(f, effect::big_red_breath, 9, 0x21, 3, fire))
        {
            return;
        }
    }
    if (f.has(effect::blue_breath) && lightning_breath(f))
    {
        return;
    }
    if (f.has(effect::black_breath) && secret_breath(f, effect::black_breath, 6, 0x20, 1))
    {
        return;
    }
}

// A blue dragon's lightning, Pools of Darkness's and the kit's alike
// (magic.pas, EffBreatheLightning; fxcombat.c, fx_lightning_breath): three a
// fight from the first round, aimed as the dragon would aim a Lightning Bolt,
// its maximum Hit Points to the aim and to each creature along a bolt of 9
// squares that does not bounce, half on a save against spells. The breath is
// electricity, not magic (element 24h), and no spell. With no enemy to aim at
// it holds its breath. It ends its turn.
bool Session::lightning_breath(Fighter &dragon)
{
    auto *breath = dragon.find_effect(effect::blue_breath);
    if (round_ == 1 || breath->strength > 3)
    {
        breath->strength = 3;
    }
    if (breath->strength <= 0)
    {
        return false;
    }
    const auto *target = lightning_bolt_aim(dragon);
    if (!target)
    {
        return false;
    }
    --breath->strength;
    log({"{name} breathes.", {{"name", dragon.name}}});
    const Cell aim = target->cell;
    constexpr int electric_breath = 0x24;
    const int damage = dragon.numbers.max_hit_points, level = dragon.numbers.level;
    lightning_at(aim, damage, level, 4, electric_breath, 0);
    lightning_path(dragon.cell, aim, 9, damage, level, false, 4, electric_breath, 0);
    end_actions(dragon);
    return true;
}

// The enemy a monster's Lightning Bolt is aimed at, as Pools of Darkness's and
// the kit's AI aim it (cbtacts.pas and cbaction.c, aim_target and
// rank_target): within the spell's range there, 4 squares and 4 a caster
// level (their spell tables' row; the spell table here keeps Pool of
// Radiance's 1 a level), and none immune to electricity or within a Globe of
// Invulnerability or a Minor Globe, nor one of magic resistance above 130.
const Fighter *Session::lightning_bolt_aim(Fighter &caster)
{
    constexpr int lightning_bolt = 51;
    const int level = caster_level(caster, *game_spell(lightning_bolt), game_of(identity_));
    const auto *target = secret_target(caster, 4 + 4 * level, false);
    const bool proof = target && (target->has(335) || target->has(263) ||
                                  target->has(effect::globe_of_invulnerability) ||
                                  target->numbers.magic_resistance > 130);
    return proof ? nullptr : target;
}

// A Secret dragon's fear as a fight starts (effhndlr.pas, EffDragonFear, in
// its reconstruction): each enemy below level 5 that is not fearless and not
// afraid already is terrified and runs for the rest of the fight.
void Session::dragon_fear(const Fighter &dragon)
{
    // The kit's terror (effects.c, fx_terrify) reaches one below 6 Hit Dice,
    // saved against as a spell, for 4d6 rounds.
    const int fearless_from = kit() ? 6 : 5;
    for (auto &f : fighters_)
    {
        if (f.side == dragon.side || !f.able() || f.has(effect::fearless) || f.has(effect::fear) ||
            f.numbers.level >= fearless_from)
        {
            continue;
        }
        const bool saved = kit() && saving_throw(f, 4, 0, &dragon);
        const int rounds = kit() ? roll(6) + roll(6) + roll(6) + roll(6) : 0;
        if (add_with_message(f, {effect::fear, rounds, 255, true}, true, saved, 0,
                             dragon.numbers.level, game_spell(184)))
        {
            f.turned = true;
        }
    }
}

// A Pools of Darkness dragon's fear (effhndlr.pas, EffDragonFear, in its
// reconstruction): each able enemy not terrified already that fails a save
// against petrification at -4 is terrified for the fight, 2 worse to hit and
// 2 less harmful a blow (EffDragonFeared).
void Session::darkness_dragon_fear(const Fighter &dragon)
{
    for (auto &f : fighters_)
    {
        if (f.side == dragon.side || !f.able() || f.has(effect::dragon_feared) ||
            saving_throw(f, 1, -4, &dragon))
        {
            continue;
        }
        f.numbers.effects.push_back({effect::dragon_feared, 0, 0, false});
        log({"{name} is terrified.", {{"name", f.name}}});
    }
}

// How many times a creature has used a power this fight, kept on it as an
// effect of the power's code plus 100.
int Session::uses(Fighter &f, int code)
{
    if (const auto *count = f.find_effect(code + 100))
    {
        return count->strength;
    }
    return 0;
}

void Session::spend_use(Fighter &f, int code)
{
    if (auto *count = f.find_effect(code + 100))
    {
        ++count->strength;
        return;
    }
    f.numbers.effects.push_back({code + 100, 0, 1, false});
}

// Curse's monsters' own powers at their turn (its reconstruction's ovr023):
// a breath of acid or fire along a path, a hell hound's fire, Tyranthraxus's
// lightning, a slug's acid, a paralyzing gaze and a beholder's rays.
void Session::curse_specials(Fighter &f)
{
    const auto breathe = [&](int code, int length, int width, int element, bool spare_friends)
    {
        if (!f.has(code) || uses(f, code) >= 3 || !acquire_target(f, 6, false, false))
        {
            return false;
        }
        const auto targets = path_area(f, find(f.lock)->cell, length, width);
        if (spare_friends && std::ranges::any_of(targets,
                                                 [&](EntityId id)
                                                 {
                                                     return find(id)->side == f.side;
                                                 }))
        {
            return false;
        }
        log({"{name} breathes.", {{"name", f.name}}});
        spend_use(f, code);
        for (const auto id : targets)
        {
            auto &target = *find(id);
            nonweapon_damage(target, f.numbers.max_hit_points, 2,
                             saving_throw(target, 3, 0, &f, element), element, 12, 0, false);
        }
        end_actions(f);
        return true;
    };
    // A black dragon breathes acid along 6 squares, never through its own;
    // a red one fire along 9, three wide (DragonBreathAcid, DragonBreathFire).
    if (breathe(290, 6, 1, 0x30, true) || breathe(328, 9, 3, 0x21, false))
    {
        return;
    }
    auto *lock = find(f.lock);
    // A hell hound breathes 7 of fire at one beside it, half the time.
    if (f.has(331) && acquire_target(f, 12, false, false) && (lock = find(f.lock)) &&
        distance(f, *lock) <= 1 && roll(100) <= 50)
    {
        log({"{name} breathes fire.", {{"name", f.name}}});
        nonweapon_damage(*lock, 7, 2, saving_throw(*lock, 3, 0, &f, 1), 0x21, 12, 0, false);
        end_actions(f);
        return;
    }
    // Tyranthraxus throws lightning in the first four rounds: 16d6 along
    // the bolt and again on its way back (cast_throw_lightening).
    if (f.has(332) && round_ <= 4 && acquire_target(f, 12, false, false))
    {
        const Cell aim = find(f.lock)->cell;
        while (f.has(effect::invisible))
        {
            f.remove(effect::invisible);
        }
        log({"{name} throws lightning.", {{"name", f.name}}});
        const auto bolt = [this]
        {
            int total = 0;
            for (int die = 0; die < 16; ++die)
            {
                total += roll(6);
            }
            return total;
        };
        lightning_at(aim, bolt(), 12);
        lightning_path(f.cell, aim, 8, bolt(), 12, true);
        end_actions(f);
        return;
    }
    // A giant slug spits acid as far as 6 squares, 3 times in 10, for its
    // Hit Points, saved against for half (AffectSpitAcid); Curse left it
    // untyped, as Pool of Radiance's ankheg (an obvious bug, fixed: acid).
    // Gateway's for 1d12 (its EffSpitAcidBreath), Treasures' for 1d13+3
    // (its EffSpitAcid).
    if ((f.has(286) || f.has(effect::slug_acid)) && acquire_target(f, 12, false, false) &&
        (lock = find(f.lock)) && distance(f, *lock) <= 6)
    {
        if (roll(100) <= 30)
        {
            log({"{name} spits acid.", {{"name", f.name}}});
            const int damage = gateway()                  ? roll(12)
                               : f.has(effect::slug_acid) ? roll(13) + 3
                                                          : f.numbers.max_hit_points;
            nonweapon_damage(*lock, damage, 2, saving_throw(*lock, 3, 0, &f, 16), 16, 12, 0, false);
        }
        else
        {
            log({"{name} spits acid and misses.", {{"name", f.name}}});
        }
    }
    // A paralyzing gaze: saved against as petrification, or held for an hour
    // (cast_gaze_paralyze).
    if (f.has(326) && acquire_target(f, 12, false, false) && (lock = find(f.lock)) &&
        sees(board_, f, *lock))
    {
        log({"{name} gazes.", {{"name", f.name}}});
        if (!saving_throw(*lock, 1, 0, &f))
        {
            log({"{name} is paralyzed.", {{"name", lock->name}}});
            lock->numbers.effects.push_back({effect::paralysis, 60, 12, false});
        }
    }
    if (f.has(287))
    {
        beholder_rays(f);
    }
}

// A beholder's eyes (Curse's attack_or_kill): up to four tries, each power
// once a fight, by the target's distance: a disintegrating ray at exactly 2
// squares, a petrifying one at 3, death at 4, a 2d8+1 wound at 5; then Fear,
// Slow and Sleep. The kit's (cbaction.c, eye_rays; its 117) and Pools of
// Darkness's (cbtacts.pas, AttackEyeRays; its 117, as its AI's move begins)
// lock onto an enemy anew anywhere on the field as they open, and use each
// power once a turn, a ray within its distance; the wound is Cause Serious
// Wounds.
void Session::beholder_rays(Fighter &f)
{
    const bool kit_eyes = kit() || darkness();
    int used_this_turn = 0;
    for (int tries = 0; tries < 4 && f.able(); ++tries)
    {
        const bool fresh_lock = kit_eyes && tries == 0;
        if (!acquire_target(f, kit_eyes ? 255 : 12, false, fresh_lock))
        {
            return;
        }
        auto &target = *find(f.lock);
        const int range = distance(f, target);
        const int used = kit_eyes ? used_this_turn : uses(f, 287);
        const auto power = [&](int bit)
        {
            if (used & bit)
            {
                return false;
            }
            if (kit_eyes)
            {
                used_this_turn |= bit;
            }
            else if (auto *count = f.find_effect(387))
            {
                count->strength |= bit;
            }
            else
            {
                f.numbers.effects.push_back({387, 0, bit, false});
            }
            return true;
        };
        const auto reaches = [&](int squares)
        {
            return kit_eyes ? range <= squares : range == squares;
        };
        if (reaches(2) && power(1))
        {
            log({"{name} fires a disintegrating ray.", {{"name", f.name}}});
            if (!saving_throw(target, 3, 0, &f))
            {
                target.condition = Condition::gone;
                target.hit_points = 0;
                target.initiative = 0;
                log({"{name} is disintegrated.", {{"name", target.name}}});
                end_effects_at_zero(target);
                went_down(target);
            }
        }
        else if (reaches(3) && power(2))
        {
            log({"{name} fires a petrifying ray.", {{"name", f.name}}});
            if (!saving_throw(target, 1, 0, &f))
            {
                turn_to_stone(target);
            }
        }
        else if (reaches(4) && power(4))
        {
            log({"{name} fires a death ray.", {{"name", f.name}}});
            if (!saving_throw(target, 0, 0, &f))
            {
                log({"{name} is slain.", {{"name", target.name}}});
                kill(target);
            }
        }
        else if (reaches(5) && power(8))
        {
            log({"{name} fires a wounding ray.", {{"name", f.name}}});
            constexpr int cause_serious_wounds = 66;
            const int wound =
                kit_eyes ? canonical_spell(cause_serious_wounds, game_of(identity_)) : 0;
            nonweapon_damage(target, roll(8) + roll(8) + 1, 0, false, 8, 12, wound, false);
        }
        else
        {
            for (const auto [bit, number] :
                 {std::pair{16, 184}, std::pair{32, 55}, std::pair{64, 21}})
            {
                if (power(bit))
                {
                    f.aiming = number;
                    f.using_item = -1;
                    resolve_spell(f, {target.id}, target.cell);
                    break;
                }
            }
        }
    }
}

// The original's approach patterns: what each pattern adds to the direction,
// in the order it tries them (combatai.pas, aiDirOffsets).
constexpr std::array<std::array<int, 5>, 6> approach{{
    {8, 7, 6, 1, 2},
    {8, 1, 2, 7, 6},
    {7, 1, 8, 6, 2},
    {1, 7, 8, 2, 6},
    {8, 7, 6, 5, 4},
    {8, 1, 2, 3, 4},
}};

// Whether it can step that way (combatai.pas, AiCanStep): onto the field,
// free of creatures, on ground it can pay for; a stinking cloud only for one
// that breathes it unharmed. Off the field, edge is set.
bool Session::can_step(const Fighter &f, int direction, bool &edge) const
{
    edge = false;
    const auto dx = step_x[static_cast<std::size_t>(direction)];
    const auto dy = step_y[static_cast<std::size_t>(direction)];
    auto moved = f;
    moved.cell = {f.cell.x + dx, f.cell.y + dy};
    for (const auto square : squares_of(moved))
    {
        if (!board_.contains(square))
        {
            edge = true;
            return false;
        }
    }
    // Each step is a stop: it may not end on another creature's square.
    // Moving as the original does, it must leave some movement, except in Pool
    // of Radiance, whose computer spends its last half-moves.
    const auto grid = grid_for(f);
    const auto price = grid.step_price(f.cell, moved.cell);
    const bool keeps_some =
        original_movement() && game_of(identity_) != GoldBoxGame::pool_of_radiance;
    const bool affordable =
        price && (keeps_some ? price->need < f.half_moves : price->need <= f.half_moves);
    if (!affordable || !grid.can_stop_at(moved.cell))
    {
        return false;
    }
    if (secret())
    {
        // A stinking cloud's save is rolled as the step is taken (ai_step).
        return cloud_balk(f, moved) != CloudBalk::always;
    }
    if (darkness())
    {
        return !darkness_balks(f, moved);
    }
    const bool breathes_freely = f.has(effect::animated) || f.has(effect::coughing) || f.has(111) ||
                                 f.has(125) || breathes_clouds_freely(f);
    return breathes_freely || !in_cloud(moved, CloudKind::stinking);
}

// Secret's AI balks at a step into a cloud (combatai.pas, AiCanStep): into a
// cloudkill when its movement is below 7, unless proof against poison and
// paralysis (its effect 99) or turned; into a stinking cloud unless it coughs
// already, a globe guards it (its effects 57 and 63), it is proof or turned,
// or it saves against paralysis.
Session::CloudBalk Session::cloud_balk(const Fighter &f, const Fighter &moved) const
{
    const auto game = game_of(identity_);
    const bool proof = f.has(canonical_effect(99, game));
    if (in_cloud(moved, CloudKind::deadly) && f.numbers.movement < 7 && !proof && !f.turned)
    {
        return CloudBalk::always;
    }
    const bool guarded = f.has(effect::coughing) || f.has(canonical_effect(57, game)) ||
                         f.has(canonical_effect(63, game)) || proof || f.turned;
    return in_cloud(moved, CloudKind::stinking) && !guarded ? CloudBalk::unless_saved
                                                            : CloudBalk::none;
}

// Pools of Darkness's AI never steps into a cloud that would harm it, with no
// save and whatever its movement (combatai.pas, AiCanStep): a stinking cloud
// unless it coughs already, a Minor Globe guards it (its effect 63) or its
// record calls it unliving or proof against poison; a cloudkill unless a
// Globe of Invulnerability guards it (57) or it is proof against poison; and
// Blade Barrier's blades always. One turned to flee steps anywhere.
bool Session::darkness_balks(const Fighter &f, const Fighter &moved) const
{
    if (f.turned)
    {
        return false;
    }
    const auto game = game_of(identity_);
    const int traits = f.numbers.traits;
    const bool poison_proof = (traits & trait::poison_proof) != 0;
    const bool breathes_stench = f.has(effect::coughing) || f.has(canonical_effect(63, game)) ||
                                 poison_proof || (traits & trait::unliving) != 0;
    const bool breathes_poison = f.has(canonical_effect(57, game)) || poison_proof;
    return (in_cloud(moved, CloudKind::stinking) && !breathes_stench) ||
           (in_cloud(moved, CloudKind::deadly) && !breathes_poison) ||
           in_cloud(moved, CloudKind::blades);
}

// The first step of the shortest way, round walls and creatures, to a square
// beside the nearest enemy it may single out, when that step can be taken. A
// departure from the original, whose AI a wall could leave with no way to
// fight or get away for good, so a fight no side could end went on forever
// (docs/DEVIATIONS.md).
std::optional<Session::Route> Session::route_to_enemy(const Fighter &f) const
{
    constexpr int unlimited = 1'000'000;
    const auto reach = grid_for(f).reachable(unlimited);
    std::optional<int> best_cost;
    rules::Cell best{};
    EntityId target{};
    auto moved = f;
    for (int y = 0; y < board_.height; ++y)
    {
        for (int x = 0; x < board_.width; ++x)
        {
            moved.cell = {x, y};
            const auto cost = reach.cost_to(moved.cell);
            if (!cost || (best_cost && *cost >= *best_cost))
            {
                continue;
            }
            for (const auto &o : fighters_)
            {
                if (o.side != f.side && o.able() && can_target(f, o) && distance(moved, o) <= 1)
                {
                    best_cost = cost;
                    best = moved.cell;
                    target = o.id;
                    break;
                }
            }
        }
    }
    if (!best_cost)
    {
        return std::nullopt;
    }
    const auto path = reach.path_to(best);
    for (int direction = 0; direction < 8 && !path.empty(); ++direction)
    {
        const rules::Cell step{f.cell.x + step_x[static_cast<std::size_t>(direction)],
                               f.cell.y + step_y[static_cast<std::size_t>(direction)]};
        bool edge = false;
        if (step == path.front() && can_step(f, direction, edge))
        {
            return Route{direction, target};
        }
    }
    return std::nullopt;
}

// One step (combatai.pas, AiMoveStep): toward its lock, or away when
// fleeing, trying the pattern's five directions; a fleeing creature that
// finds the edge tries to get away. Stuck (no way, or turning back), it
// changes pattern, then drops its lock, then stops; a creature the computer
// plays that has not fought this turn takes the shortest way to an enemy
// instead of stopping (route_to_enemy). False when it cannot go on.
bool Session::ai_step(Fighter &f, int &last_direction, int &stuck)
{
    const bool lone_mage =
        f.numbers.class_levels[static_cast<std::size_t>(ClassSlot::magic_user)] > 0 &&
        std::count_if(f.numbers.class_levels.begin(), f.numbers.class_levels.end(),
                      [](int level)
                      {
                          return level > 0;
                      }) == 1;
    if (f.half_moves / 2 <= 0 || f.initiative <= 0 || (lone_mage && !f.fleeing))
    {
        finish_round(f);
        return false;
    }
    if (f.routing)
    {
        if (const auto route = route_to_enemy(f))
        {
            f.lock = route->target;
            return take_ai_step(f, route->direction, last_direction);
        }
        f.routing = false;
    }
    const Fighter *locked = f.fleeing ? nullptr : find(f.lock);
    int base = 0;
    if (f.fleeing)
    {
        // The party's enemies run along its facing, its own side the other
        // way (combatai.pas, AiMoveStep); the 256-color engine's turn one
        // step clockwise of the earlier games', north giving north and
        // south south.
        f.stance = roll(2);
        const int turned = vga_engine(game_of(identity_)) ? 1 : 0;
        base = (along_party_facing() + turned + (f.side == Side::party ? 4 : 0)) % 8;
    }
    else if (locked)
    {
        base = direction_toward(f.cell, locked->cell);
    }
    int chosen = -1;
    const auto &pattern = approach[static_cast<std::size_t>(std::clamp(f.stance, 1, 6) - 1)];
    for (const int offset : pattern)
    {
        const int direction = (base + offset) % 8;
        bool edge = false;
        auto moved = f;
        moved.cell = {f.cell.x + step_x[static_cast<std::size_t>(direction)],
                      f.cell.y + step_y[static_cast<std::size_t>(direction)]};
        if (can_step(f, direction, edge) &&
            (!secret() || cloud_balk(f, moved) != CloudBalk::unless_saved || saving_throw(f, 0, 0)))
        {
            chosen = direction;
            break;
        }
        if (edge && f.fleeing)
        {
            attempt_flee(f);
            f.half_moves = 0;
            f.fleeing = false;
            return false;
        }
    }
    if (chosen < 0 || chosen == (last_direction + 4) % 8)
    {
        ++stuck;
        f.stance = f.stance % 6 + 1;
        if (stuck > 1)
        {
            f.lock = 0;
            if (stuck > 2)
            {
                // Its turn left it no way to fight or get away.
                const auto route =
                    f.numbers.morale >= non_player && !f.fought ? route_to_enemy(f) : std::nullopt;
                if (route)
                {
                    f.routing = true;
                    f.fleeing = false;
                    f.lock = route->target;
                    return take_ai_step(f, route->direction, last_direction);
                }
                f.half_moves = 0;
                return false;
            }
            if (!acquire_target(f, 255, true, false))
            {
                finish_round(f);
                return false;
            }
        }
    }
    if (chosen < 0)
    {
        return true;
    }
    return take_ai_step(f, chosen, last_direction);
}

// The step an AI chose; false when the step left it unable to go on.
bool Session::take_ai_step(Fighter &f, int direction, int &last_direction)
{
    last_direction = direction;
    step_to(f, {f.cell.x + step_x[static_cast<std::size_t>(direction)],
                f.cell.y + step_y[static_cast<std::size_t>(direction)]});
    if (!f.able())
    {
        end_actions(f);
        return false;
    }
    return true;
}

// An AI uses an item (combatai.pas, SelectItemToUse): 1d7 passes, the k-th
// taking powers of priority 8 - k or more; the first readied item (not a
// scroll) whose spell it decides on is used.
bool Session::ai_use_item(Fighter &f)
{
    // The 256-color engine's computer never uses an item (its SelectItemToUse).
    if (weighs_spells())
    {
        return false;
    }
    const int passes = roll(7);
    const bool enemy_left = std::any_of(fighters_.begin(), fighters_.end(),
                                        [&f](const Fighter &o)
                                        {
                                            return o.side != f.side && o.able();
                                        });
    if (!f.item_use_allowed || !enemy_left)
    {
        return false;
    }
    AreaAims area_aims;
    for (int pass = 1; pass <= passes; ++pass)
    {
        for (const auto &item : f.numbers.items)
        {
            const int number = item.spells[0];
            if (item.scroll || !item.readied || !spell_built(number))
            {
                continue;
            }
            // A party member weighs an item's area spell as the item casts
            // it, at the item's level (caster_level), as its aim will; the
            // original's AI decides at the creature's own level.
            f.using_item = weighs_area_spell(f, number) ? item.ordinal : -1;
            if (!ai_decides(f, number, 8 - pass, area_aims))
            {
                f.using_item = -1;
                continue;
            }
            f.using_item = item.ordinal;
            log({"{name} uses an item.", {{"name", f.name}}});
            f.aiming = number;
            f.picked.clear();
            const auto &known = *game_spell(number);
            f.picks_left = (known.area & 0x0F) <= 7 ? (known.area & 3) + 1 : 1;
            if (!ai_aim(f))
            {
                f.aiming = f.picks_left = 0;
                f.using_item = -1;
            }
            return true;
        }
    }
    return false;
}

// An AI casts (combatai.pas, SelectSpellToCast): one whose morale counts (a
// monster or NPC); 1d7 passes, three random picks each among its memorized
// spells, the first it decides on cast as anyone casts it.
bool Session::ai_cast(Fighter &f, bool magic)
{
    if (weighs_spells())
    {
        return treasures_ai_cast(f, magic);
    }
    const int passes = roll(7);
    const bool enemy_left = std::any_of(fighters_.begin(), fighters_.end(),
                                        [&f](const Fighter &o)
                                        {
                                            return o.side != f.side && o.able();
                                        });
    const auto &spells = f.numbers.spells;
    // A party member casts only with the party's Quick magic on (Curse's
    // reference card: ALT M); monsters and NPCs always may.
    if (!f.cast_allowed || silenced(f) || f.has(effect::no_magic) || !enemy_left ||
        (f.numbers.morale < non_player && !magic) || spells.empty())
    {
        return false;
    }
    AreaAims area_aims;
    for (int pass = 1; pass <= passes; ++pass)
    {
        for (int pick = 0; pick < 3; ++pick)
        {
            const int number =
                spells[static_cast<std::size_t>(roll(static_cast<int>(spells.size())) - 1)];
            if (game_spell(number) && game_spell(number)->when != 0 && spell_built(number) &&
                (!underwater_ || underwater_spell(number)) &&
                ai_decides(f, number, 8 - pass, area_aims))
            {
                begin_cast(f, number);
                if (f.aiming && !ai_aim(f))
                {
                    lose_spell(f);
                    end_actions(f);
                }
                return true;
            }
        }
    }
    return false;
}

// Treasures' computer casts (its combatai.pas, SelectSpellToCast and
// PickCastableSpell): never while fleeing; of its memorized spells that may
// be cast in a fight, the one of highest AI weight (a later copy first among
// equals) that has a target worth it, and that one only if 1d100 is above
// four times its fighter, paladin and ranger levels and 1d5 no more than the
// spell's weight. The original's sort passes over the first memorized spell
// (docs/ORIGINAL-RULES.md, Bugs); OpenGoldBox weighs them all.
bool Session::treasures_ai_cast(Fighter &f, bool magic)
{
    if (f.fleeing || !f.cast_allowed || silenced(f) || f.has(effect::no_magic) ||
        (f.numbers.morale < non_player && !magic))
    {
        return false;
    }
    std::vector<int> ranked;
    for (auto copy = f.numbers.spells.rbegin(); copy != f.numbers.spells.rend(); ++copy)
    {
        const auto *known = game_spell(*copy);
        if (known && known->when != 0 && spell_built(*copy))
        {
            ranked.push_back(*copy);
        }
    }
    std::ranges::stable_sort(ranked, std::greater{},
                             [this](int number)
                             {
                                 return game_spell(number)->ai_priority;
                             });
    const auto chosen = std::ranges::find_if(ranked,
                                             [&](int number)
                                             {
                                                 return treasures_spell_has_target(f, number);
                                             });
    if (chosen == ranked.end())
    {
        return false;
    }
    const auto &levels = f.numbers.class_levels;
    const int fighting = 4 * (levels[static_cast<std::size_t>(ClassSlot::fighter)] +
                              levels[static_cast<std::size_t>(ClassSlot::paladin)] +
                              levels[static_cast<std::size_t>(ClassSlot::ranger)]);
    if (roll(100) <= fighting || game_spell(*chosen)->ai_priority < roll(5))
    {
        return false;
    }
    begin_cast(f, *chosen);
    if (f.aiming && !ai_aim(f))
    {
        lose_spell(f);
        end_actions(f);
    }
    return true;
}

// How Treasures' computer rates a creature as its spell's target (its
// cbtacts.pas, RateSpellTarget): -10 one of the side the spell would spare,
// 1 one the spell may take, 0 one it would do nothing to. A globe keeps off
// the spells it stops, and magic resistance above 50 every spell.
int Session::treasures_spell_rating(const Fighter &caster, const Fighter &target, int number) const
{
    // Pools of Darkness numbers these effects as Treasures does.
    const auto game = game_of(identity_);
    const auto &known = *game_spell(number);
    const Side enemies = caster.side == Side::party ? Side::opposition : Side::party;
    if (target.side != (known.offensive ? enemies : caster.side))
    {
        return -10;
    }
    // Its own effect numbers (canonical_effect).
    const auto has = [&target, game](int code)
    {
        return target.has(canonical_effect(code, game));
    };
    constexpr int bless = 1, immune_electricity = 6, shield = 17, blind = 33, bestow_curse = 36,
                  globe = 57, prayer = 49, held = 52, asleep = 53, minor_globe = 63,
                  immune_fear = 92, immune_fire = 93;
    const bool person = target.numbers.race != 0 || (target.numbers.traits & trait::person) != 0;
    const auto fighting_beside = [&]
    {
        return std::any_of(fighters_.begin(), fighters_.end(),
                           [&](const Fighter &o)
                           {
                               return o.side != target.side && o.able() && distance(o, target) <= 1;
                           });
    };
    bool takes = false;
    // The spells by the rules' numbers (canonical_spell).
    switch (number)
    {
    case 1: // Bless, on one no enemy stands beside
        takes = !has(bless) && !fighting_beside();
        break;
    case 42: // Prayer
        takes = !caster.has(canonical_effect(prayer, game));
        break;
    case 3:   // Cure Light Wounds
    case 158: // Cure Serious Wounds
    case 171: // Cure Critical Wounds
    case 236: // Heal
        takes = target.hit_points < target.numbers.max_hit_points;
        break;
    case 296: // Charm Person or Mammal
        takes = target.numbers.race != 0 || (target.numbers.traits & trait::mammal) != 0;
        break;
    case 10: // Charm Person, a person on one square
        takes = person && !target.numbers.large;
        break;
    case 15:  // Magic Missile
    case 165: // the wand of magic missiles
        takes = !has(shield);
        break;
    case 21: // Sleep
        takes = target.numbers.level < 5 && !has(asleep);
        break;
    case 23:
    case 49: // Hold Person, a character or person not held
        takes = target.numbers.race != 0 && person && !has(held);
        break;
    case 37: // Cure Blindness
        takes = has(blind);
        break;
    case 43: // Remove Curse
        takes = has(bestow_curse);
        break;
    case 47:  // Fireball
    case 64:  // the necklace of missiles
    case 174: // Flame Strike
    case 315: // Delayed Blast Fireball
        takes = !has(immune_fire);
        break;
    case 51: // Lightning Bolt
        takes = !has(immune_electricity);
        break;
    case 176: // Slay Living
    case 310: // Death Spell
        takes = (target.numbers.traits & trait::unliving) == 0;
        break;
    case 61:  // the wand of paralyzation
    case 182: // Confusion
        takes = true;
        break;
    case 181: // Charm Monsters
    case 194: // Hold Monster
    case 320: // Mass Charm
        takes = person;
        break;
    case 184: // Fear
        takes = !has(immune_fear);
        break;
    case 191: // Cloudkill
        takes = target.numbers.level < 6;
        break;
    default:
        takes = known.effect == 0 || !target.has(known.effect);
        break;
    }
    if (takes && ((known.level < 5 && has(globe)) || (known.level < 4 && has(minor_globe)) ||
                  target.numbers.magic_resistance > 50))
    {
        takes = false;
    }
    return takes ? 1 : 0;
}

// Whether Treasures' spell has a target worth casting at (its RateSpell):
// the caster itself for a spell on the caster, else someone in range and
// sight it rates above 0.
bool Session::treasures_spell_has_target(const Fighter &f, int number) const
{
    if (weighs_area_spell(f, number))
    {
        return party_area_aim(f, number).has_value();
    }
    const auto &known = *game_spell(number);
    if (known.area == 0)
    {
        return treasures_spell_rating(f, f, number) > 0;
    }
    const int range = spell_range(known, caster_level(f, known, game_of(identity_)));
    return std::any_of(fighters_.begin(), fighters_.end(),
                       [&](const Fighter &o)
                       {
                           return o.able() && distance(f, o) <= range &&
                                  (o.id == f.id || sees(board_, f, o)) &&
                                  treasures_spell_rating(f, o, number) > 0;
                       });
}

// The one creature Treasures' computer singles out for its spell (its
// SelectSpellTarget, PickSpellTarget): an enemy it locks onto anew, or for a
// spell on its own side one of them at random, within range and in sight;
// nobody when it rates that one at 0 or below.
Fighter *Session::treasures_spell_target(Fighter &f, int number)
{
    const auto &known = *game_spell(number);
    const int range = spell_range(known, caster_level(f, known, game_of(identity_)));
    Fighter *pick = nullptr;
    if (known.offensive)
    {
        pick = acquire_target(f, range, false, true) ? find(f.lock) : nullptr;
    }
    else
    {
        std::vector<Fighter *> friends;
        for (auto &o : fighters_)
        {
            if (o.side == f.side && o.able() && distance(f, o) <= range &&
                (o.id == f.id || sees(board_, f, o)))
            {
                friends.push_back(&o);
            }
        }
        if (!friends.empty())
        {
            pick = friends[static_cast<std::size_t>(roll(static_cast<int>(friends.size())) - 1)];
        }
    }
    return pick && treasures_spell_rating(f, *pick, number) > 0 ? pick : nullptr;
}

// Where Treasures' computer aims a spell that bursts around a square (its
// AimAreaSpell): the square in range and sight where the creatures the
// burst would take rate highest together, each it may take 1 and each of
// the side it would spare -10, a tie taking the later square one time in
// four; nowhere when no square rates above 0. The burst is the one the
// spell has as it goes off (within).
std::optional<Cell> Session::treasures_area_aim(const Fighter &f, int number)
{
    const auto &known = *game_spell(number);
    const int range = spell_range(known, caster_level(f, known, game_of(identity_)));
    const int radius = known.area & 7;
    // Only a square near one the spell may take can rate above 0.
    std::vector<Cell> takeable;
    for (const auto &o : fighters_)
    {
        if (o.able() && treasures_spell_rating(f, o, number) > 0)
        {
            takeable.push_back(o.cell);
        }
    }
    std::optional<Cell> best;
    int best_score = 0;
    for (int x = f.cell.x - range; x <= f.cell.x + range; ++x)
    {
        for (int y = f.cell.y - range; y <= f.cell.y + range; ++y)
        {
            const Cell cell{x, y};
            const bool near_one =
                std::any_of(takeable.begin(), takeable.end(),
                            [&](Cell at)
                            {
                                return doubled_distance(at, cell) <= 2 * radius + 4;
                            });
            if (!near_one || !board_.contains(cell) || rules::blocks_movement(board_.at(cell)) ||
                doubled_distance(f.cell, cell) / 2 > range || !clear_line(board_, f.cell, cell))
            {
                continue;
            }
            int score = 0;
            for (const auto id : within(cell, radius))
            {
                score += treasures_spell_rating(f, *find(id), number);
            }
            if (score > best_score || (score == best_score && score > 0 && roll(100) <= 25))
            {
                best = cell;
                best_score = score;
            }
        }
    }
    return best_score > 0 ? best : std::nullopt;
}

// Treasures' computer aims its spell (its cbtacts.pas, SelectCombatTargets):
// a burst at the square AimAreaSpell finds; a bolt or a spell on one
// creature at the one it singles out; a spell on several at as many picks
// as it takes (the same one picked again spends a pick), or for Charm
// Monsters and Faerie Fire picks until their weight passes a budget (2d4,
// or the caster's level for Faerie Fire; a creature weighs 1 to level 1, 2
// at 2, 4 at 3 and 8 above, or for Faerie Fire 1, and 2 a large one). False
// when it finds no one, and the spell is aborted.
bool Session::treasures_ai_aim(Fighter &f)
{
    constexpr int faerie_fire = 179;
    const int number = f.aiming;
    const auto &known = *game_spell(number);
    const int area = known.area & 0x0F;
    const auto abort = [&]
    {
        log({"{name} aborts the spell.", {{"name", f.name}}});
        return false;
    };
    if (area == 0)
    {
        resolve_spell(f, {f.id}, f.cell);
        return true;
    }
    if (area >= 9 && area <= 14)
    {
        const auto aim = treasures_area_aim(f, number);
        if (!aim)
        {
            return abort();
        }
        resolve_spell(f, within(*aim, area & 7), *aim);
        return true;
    }
    if (area == 8 || area == 15)
    {
        const auto *target = treasures_spell_target(f, number);
        if (!target)
        {
            return abort();
        }
        const auto aim = target->cell;
        resolve_spell(f, area == 8 ? within(aim, 0) : std::vector<EntityId>{target->id}, aim);
        return true;
    }
    std::vector<EntityId> picked;
    const auto picked_already = [&](const Fighter &target)
    {
        return std::find(picked.begin(), picked.end(), target.id) != picked.end();
    };
    if (area == 5)
    {
        int budget =
            number == faerie_fire ? caster_level(f, known, game_of(identity_)) : roll(4) + roll(4);
        int weight = 0;
        while (budget > 0)
        {
            const auto *target = treasures_spell_target(f, number);
            if (!target)
            {
                break;
            }
            if (picked_already(*target))
            {
                --budget;
                continue;
            }
            picked.push_back(target->id);
            const int level = target->numbers.level;
            weight += number == faerie_fire ? (target->numbers.large ? 2 : 1)
                      : level <= 1          ? 1
                      : level == 2          ? 2
                      : level == 3          ? 4
                                            : 8;
            if (picked.size() > 1 && weight > budget)
            {
                break;
            }
        }
    }
    else
    {
        for (int picks = (area & 3) + 1; picks > 0; --picks)
        {
            const auto *target = treasures_spell_target(f, number);
            if (!target)
            {
                break;
            }
            if (!picked_already(*target))
            {
                picked.push_back(target->id);
            }
        }
    }
    if (picked.empty())
    {
        return abort();
    }
    resolve_spell(f, picked, find(picked.back())->cell);
    return true;
}

// Whether the AI takes this spell (combatai.pas, DecideCastSpell): of high
// enough priority; one on itself always, Cure Light Wounds when someone needs
// it; one at an enemy when an enemy is in range and sight, and none of those
// has a friend within the spell's blast. A party-side caster takes an area
// spell only where its outlook finds an aim the player's limits allow
// (party_area_aim), kept for the turn in `area_aims`.
bool Session::ai_decides(const Fighter &f, int number, int least_priority,
                         AreaAims &area_aims) const
{
    const auto &known = *game_spell(number);
    if (known.ai_priority == 0 || known.ai_priority < least_priority)
    {
        return false;
    }
    if (number == 3)
    {
        return const_cast<Session *>(this)->cure_search(f) != nullptr;
    }
    if (!known.offensive)
    {
        return true;
    }
    if (weighs_area_spell(f, number))
    {
        auto [aim, unweighed] = area_aims.try_emplace(number);
        if (unweighed)
        {
            aim->second = party_area_aim(f, number);
        }
        return aim->second.has_value();
    }
    const int range = spell_range(known, caster_level(f, known, game_of(identity_)));
    const auto targets = enemies_within(f, range, true);
    if (targets.empty())
    {
        return false;
    }
    if (known.ai_radius == 0)
    {
        return true;
    }
    return std::none_of(targets.begin(), targets.end(),
                        [&](EntityId id)
                        {
                            const auto &t = *find(id);
                            return std::any_of(fighters_.begin(), fighters_.end(),
                                               [&](const Fighter &o)
                                               {
                                                   return o.side == f.side && o.able() &&
                                                          distance(o, t) <= known.ai_radius;
                                               });
                        });
}

// Whom an AI heals (combatai.pas, the cure search): within a square, the
// wounded of its side with the fewest Hit Points (itself below half), else,
// unless that one has fewer than 8, one downed beside it.
Fighter *Session::cure_search(const Fighter &f)
{
    Fighter *best = nullptr;
    for (auto &o : fighters_)
    {
        if (o.side != f.side || !o.able() || distance(f, o) > 1 ||
            o.hit_points >= o.numbers.max_hit_points ||
            (o.id == f.id && o.hit_points * 2 >= o.numbers.max_hit_points))
        {
            continue;
        }
        if (!best || o.hit_points < best->hit_points)
        {
            best = &o;
        }
    }
    if (best && best->hit_points < 8)
    {
        return best;
    }
    for (auto &o : fighters_)
    {
        if (o.side == f.side && o.downed() && distance(f, o) <= 1)
        {
            return &o;
        }
    }
    return best;
}

// The AI aims what it casts or uses (cbtacts.pas, SelectCombatTargets): on
// itself, or Cure Light Wounds on whom the cure search finds, or at random
// enemies in range and sight, not one with a friend within the blast nor a
// helpless one for a spell that makes helpless; a party-side caster an area
// spell where its outlook is best (aim_party_area_spell). False when it
// finds none.
bool Session::ai_aim(Fighter &f)
{
    if (weighs_area_spell(f, f.aiming))
    {
        return aim_party_area_spell(f);
    }
    if (weighs_spells())
    {
        return treasures_ai_aim(f);
    }
    const auto &known = *game_spell(f.aiming);
    const int area = known.area & 0x0F;
    if (f.aiming == 3)
    {
        auto *patient = cure_search(f);
        if (!patient)
        {
            return false;
        }
        resolve_spell(f, {patient->id}, patient->cell);
        return true;
    }
    if (!known.offensive)
    {
        resolve_spell(
            f, area >= 8 && area <= 14 ? within(f.cell, area & 7) : std::vector<EntityId>{f.id},
            f.cell);
        return true;
    }
    const int range = spell_range(known, caster_level(f, known, game_of(identity_)));
    const bool makes_helpless = std::find(helpless_effects.begin(), helpless_effects.end(),
                                          known.effect) != helpless_effects.end();
    std::vector<EntityId> picked;
    const int picks = area <= 7 ? (area & 3) + 1 : 1;
    for (int n = 0; n < picks; ++n)
    {
        for (int tries = 0; tries < 20; ++tries)
        {
            if (!acquire_target(f, range, false, true))
            {
                break;
            }
            const auto &pick = *find(f.lock);
            const bool crowded =
                known.ai_radius > 0 && std::any_of(fighters_.begin(), fighters_.end(),
                                                   [&](const Fighter &o)
                                                   {
                                                       return o.side == f.side && o.able() &&
                                                              distance(o, pick) <= known.ai_radius;
                                                   });
            if (crowded || (makes_helpless && pick.helpless()))
            {
                continue;
            }
            if (std::find(picked.begin(), picked.end(), pick.id) == picked.end())
            {
                picked.push_back(pick.id);
            }
            break;
        }
    }
    if (picked.empty())
    {
        log({"{name} aborts the spell.", {{"name", f.name}}});
        return false;
    }
    const auto aim = find(picked.front())->cell;
    resolve_spell(f, area >= 8 && area <= 14 ? within(aim, area & 7) : picked, aim);
    return true;
}

// A party member's area spells (docs/ORIGINAL-RULES.md, Party area spells;
// the user's decision, 2026-11-21): a computer-run member of the party, or a
// creature the computer plays on the party's side, weighs what an area spell
// would likely do to its own side against what it would likely do to the
// enemies, and aims it where that is best (rules::judge_area_spell).
// Monsters and the creatures against the party keep the original's AI.
class Session::Forecast
{
  public:
    Forecast(const Session &session, const Fighter &caster, int number, int level)
        : session_(session), caster_(caster), number_(number), level_(level),
          spell_(*session.game_spell(number))
    {
        int enemies = 0, levels = 0;
        for (const auto &f : session_.fighters_)
        {
            if (f.able() && f.side != caster_.side && !is_ally(f))
            {
                enemy_ids_.push_back(f.id);
                ++enemies;
                levels += f.numbers.level + 1;
            }
        }
        average_enemy_level_ = enemies > 0 ? static_cast<double>(levels) / enemies : 1.0;
    }

    // The spells the forecast weighs: those that reach an area around their
    // aim or along a path, whoever stands there.
    [[nodiscard]] static bool weighs(int number) noexcept
    {
        static constexpr std::array<int, 23> area_spells{21,  34,  41,  46,  47,  51,  55,  60,
                                                         64,  178, 182, 184, 187, 191, 192, 198,
                                                         301, 310, 315, 320, 323, 324, 325};
        return std::ranges::find(area_spells, number) != area_spells.end();
    }

    [[nodiscard]] rules::AreaOutlook outlook(Cell aim)
    {
        const auto reach = reach_at(aim);
        return summed(reach, scenarios(reach));
    }

    // The best of the aims the rules let the caster give the spell, among
    // those the limits let it cast at; none when no aim passes.
    [[nodiscard]] std::optional<Cell> best_aim(const rules::AreaSpellLimits &limits)
    {
        std::optional<Cell> best;
        rules::AreaSpellJudgement best_judgement;
        std::map<std::vector<std::int64_t>, rules::AreaSpellJudgement> judged;
        for (const auto aim : aims())
        {
            const auto reach = reach_at(aim);
            if (reach.hits.empty() && reach.downed.empty())
            {
                continue;
            }
            auto [found, unjudged] = judged.try_emplace(reach.key());
            if (unjudged)
            {
                found->second = rules::judge_area_spell(summed(reach, scenarios(reach)), limits);
            }
            const auto &judgement = found->second;
            if (judgement.cast && (!best || rules::better_area_aim(judgement, best_judgement)))
            {
                best = aim;
                best_judgement = judgement;
            }
        }
        return best;
    }

  private:
    // How the caster knows a creature: of its own side or not; its Hit
    // Points, an enemy's estimated from its Hit Dice (4.5 a die, 2 below one)
    // less what it has visibly lost, at least 1; an enemy's worth by its
    // level against the fight's enemies' average (each level plus 1); and
    // whether an able enemy of the caster's stands next to it.
    struct Known
    {
        bool ally{};
        int hit_points{}, max_hit_points{};
        double weight{1.0};
        bool enemy_beside{};
    };

    // One way the spell reaches a creature: a burst or path takes it (0), a
    // bolt strikes it once (1, Javelin of Lightning's mark 2), or a meteor
    // strikes it on its way (3) or bursts on it (4). A creature may be
    // reached several ways.
    struct Hit
    {
        EntityId id{};
        int way{};
    };

    // Whom the spell reaches at an aim, in the rules' order, and the members
    // already down lying in its area.
    struct Reach
    {
        Cell aim;
        std::vector<Hit> hits;
        std::vector<EntityId> downed;

        // Aims that reach alike fare alike.
        [[nodiscard]] std::vector<std::int64_t> key() const
        {
            std::vector<std::int64_t> key;
            for (const auto &hit : hits)
            {
                key.push_back(static_cast<std::int64_t>(hit.id) * 8 + hit.way);
            }
            key.push_back(-1);
            key.insert(key.end(), downed.begin(), downed.end());
            return key;
        }
    };

    // What one creature faces in one way the spell's shared dice fall.
    struct Fate
    {
        std::vector<double> loss{1.0};
        double disabled{}, helps_lost{}, hindrances_lost{}, hindrances_gained{}, freed{};
    };

    // One way the spell's shared dice fall, its chance, and the fate of each
    // creature it reaches then, each apart from the others'.
    struct Scenario
    {
        double chance{1.0};
        std::map<EntityId, Fate> fates;
    };

    static constexpr int burst_way = 0, bolt_way = 1, mark_way = 2, meteor_path_way = 3,
                         meteor_burst_way = 4;

    const Session &session_;
    const Fighter &caster_;
    int number_, level_;
    const Spell &spell_;
    std::vector<EntityId> enemy_ids_;
    double average_enemy_level_{1.0};
    std::map<EntityId, Known> known_;
    std::map<std::tuple<EntityId, int, bool, int, int, int, int>, std::vector<double>> harm_;

    // The side a creature fights for at heart: a charmed one its own.
    [[nodiscard]] bool is_ally(const Fighter &f) const
    {
        return (f.has(effect::charm) ? f.own_side : f.side) == caster_.side;
    }

    [[nodiscard]] const Known &known(const Fighter &f)
    {
        if (const auto found = known_.find(f.id); found != known_.end())
        {
            return found->second;
        }
        Known known;
        known.ally = is_ally(f);
        known.hit_points = f.hit_points;
        known.max_hit_points = f.numbers.max_hit_points;
        if (!known.ally)
        {
            const int level = f.numbers.level;
            known.max_hit_points = level >= 1 ? (9 * level + 1) / 2 : 2;
            const int lost = std::max(f.numbers.max_hit_points - f.hit_points, 0);
            known.hit_points = std::max(known.max_hit_points - lost, 1);
            known.weight = (level + 1) / average_enemy_level_;
        }
        known.enemy_beside = known.ally && std::ranges::any_of(session_.fighters_,
                                                               [&](const Fighter &o)
                                                               {
                                                                   return o.able() &&
                                                                          o.side != caster_.side &&
                                                                          adjacent(o, f);
                                                               });
        return known_.emplace(f.id, known).first->second;
    }

    // A creature's most Hit Points as the caster knows them.
    [[nodiscard]] auto known_most_hit_points()
    {
        return [this](const Fighter &f)
        {
            return known(f).max_hit_points;
        };
    }

    // The largest loss a creature's outlook tells apart: all it has.
    [[nodiscard]] int cap_of(const Fighter &f)
    {
        const auto &k = known(f);
        return std::max({k.hit_points, k.max_hit_points, 1});
    }

    // A loss of `chances` kept to a creature's cap, more lumped with it.
    [[nodiscard]] std::vector<double> capped(std::vector<double> chances, int cap) const
    {
        const auto size = static_cast<std::size_t>(cap) + 1;
        if (chances.size() > size)
        {
            for (std::size_t lost = size; lost < chances.size(); ++lost)
            {
                chances[size - 1] += chances[lost];
            }
            chances.resize(size);
        }
        return chances;
    }

    // The loss a non-weapon harm of `damage` does a creature once its
    // defenses weigh it, each way they can go (nonweapon_damage).
    [[nodiscard]] const std::vector<double> &harm(const Fighter &target, int damage,
                                                  int save_result, bool saved, int element,
                                                  int spell, int damage_dice)
    {
        const auto key =
            std::tuple{target.id, damage, saved, save_result, element, spell, damage_dice};
        if (const auto found = harm_.find(key); found != harm_.end())
        {
            return found->second;
        }
        const int cap = cap_of(target);
        std::vector<double> chances(static_cast<std::size_t>(cap) + 1, 0.0);
        if (absorbs_harm(target, element))
        {
            chances[0] = 1;
        }
        else
        {
            each_way<int>(
                [&](auto &&dice)
                {
                    return session_
                        .defended_harm(target, damage, save_result, saved, element, level_, spell,
                                       false, damage_dice, dice)
                        .damage;
                },
                [&](double way, int taken)
                {
                    chances[static_cast<std::size_t>(std::clamp(taken, 0, cap))] += way;
                });
        }
        return harm_.emplace(key, std::move(chances)).first->second;
    }

    // The loss a harm of `damage` does a creature that saves with this
    // chance, which halves or negates it as `save_result` says.
    [[nodiscard]] std::vector<double> struck(const Fighter &target, int damage, double save,
                                             int save_result, int element, int spell,
                                             int damage_dice)
    {
        auto loss = harm(target, damage, save_result, false, element, spell, damage_dice);
        if (save_result == 0 || save <= 0)
        {
            return loss;
        }
        const auto &saved = harm(target, damage, save_result, true, element, spell, damage_dice);
        for (std::size_t lost = 0; lost < loss.size(); ++lost)
        {
            loss[lost] = (1 - save) * loss[lost] + save * saved[lost];
        }
        return loss;
    }

    // The same for a harm of its own dice.
    [[nodiscard]] std::vector<double> struck_by_dice(const Fighter &target, const SpellDice &dice,
                                                     double save, int save_result, int element,
                                                     int spell)
    {
        std::vector<double> loss(static_cast<std::size_t>(cap_of(target)) + 1, 0.0);
        const auto totals = dice_chances(dice.count, dice.sides, dice.bonus);
        for (std::size_t total = 0; total < totals.size(); ++total)
        {
            if (totals[total] <= 0)
            {
                continue;
            }
            const auto one =
                struck(target, static_cast<int>(total), save, save_result, element, spell, 0);
            for (std::size_t lost = 0; lost < loss.size(); ++lost)
            {
                loss[lost] += totals[total] * one[lost];
            }
        }
        return loss;
    }

    // The chance the target's immunities nullify an effect the spell adds
    // (refuses_effect), at a caster level.
    [[nodiscard]] double refusal(const Fighter &target, int code, int caster_level,
                                 const Spell &known) const
    {
        double chance = 0;
        each_way<bool>(
            [&](auto &&dice)
            {
                return session_.refuses_effect(target, code, 0, caster_level, &known, dice);
            },
            [&](double way, bool refused)
            {
                chance += refused ? way : 0;
            });
        return chance;
    }

    // The chance an effect the spell adds takes the target (cast_generic): a
    // save of this modifier, where the spell allows one, refuses it when the
    // save negates; then its immunities may.
    [[nodiscard]] double effect_takes(const Fighter &target, int code, int save_modifier) const
    {
        const double save = spell_.save_result == 1 ? session_.save_chance(target, spell_.save_type,
                                                                           save_modifier, &caster_)
                                                    : 0.0;
        return (1 - save) * (1 - refusal(target, code, level_, spell_));
    }

    // A fate that slays with this chance.
    [[nodiscard]] Fate slain(const Fighter &target, double chance)
    {
        Fate fate;
        fate.loss.assign(static_cast<std::size_t>(cap_of(target)) + 1, 0.0);
        fate.loss.front() = 1 - chance;
        fate.loss.back() += chance;
        return fate;
    }

    [[nodiscard]] static Fate disabled_by(double chance)
    {
        Fate fate;
        fate.disabled = chance;
        return fate;
    }

    // Two fates of one creature, falling apart from each other.
    [[nodiscard]] Fate joined(const Fate &first, const Fate &second, int cap) const
    {
        Fate fate;
        fate.loss = capped(sum_chances(first.loss, second.loss), cap);
        fate.disabled = 1 - (1 - first.disabled) * (1 - second.disabled);
        fate.helps_lost = first.helps_lost + second.helps_lost;
        fate.hindrances_lost = first.hindrances_lost + second.hindrances_lost;
        fate.hindrances_gained = first.hindrances_gained + second.hindrances_gained;
        fate.freed = std::max(first.freed, second.freed);
        return fate;
    }

    void add_fate(Scenario &scenario, const Fighter &target, Fate fate)
    {
        if (auto found = scenario.fates.find(target.id); found != scenario.fates.end())
        {
            found->second = joined(found->second, fate, cap_of(target));
            return;
        }
        scenario.fates.emplace(target.id, std::move(fate));
    }

    // The aims the rules let the caster give the spell (aiming_commands):
    // for one that reaches around a square, each open square in range and
    // sight; for Javelin of Lightning, each creature it may single out.
    [[nodiscard]] std::vector<Cell> aims() const
    {
        std::vector<Cell> aims;
        const int area = spell_.area & 0x0F;
        if (area < 8 || area > 14)
        {
            for (const auto &f : session_.fighters_)
            {
                if (session_.reachable_by_spell(caster_, f, number_))
                {
                    aims.push_back(f.cell);
                }
            }
            return aims;
        }
        const auto &board = session_.board_;
        const int range = spell_range(spell_, level_);
        for (int y = 0; y < board.height; ++y)
        {
            for (int x = 0; x < board.width; ++x)
            {
                const Cell cell{x, y};
                if (!rules::blocks_movement(board.at(cell)) &&
                    doubled_distance(caster_.cell, cell) / 2 <= range &&
                    clear_line(board, caster_.cell, cell))
                {
                    aims.push_back(cell);
                }
            }
        }
        return aims;
    }

    // The targets a spell reaching around its aim is given (the aim
    // command's), less those that refuse it (resolve_spell).
    [[nodiscard]] std::vector<EntityId> burst_targets(Cell aim) const
    {
        auto targets = session_.within(aim, spell_.area & 7);
        std::erase_if(targets,
                      [&](EntityId id)
                      {
                          return session_.refuses_spell(*session_.find(id), number_);
                      });
        return targets;
    }

    [[nodiscard]] Reach reach_at(Cell aim) const
    {
        Reach reach{aim, {}, {}};
        const auto &session = session_;
        const auto take = [&](const std::vector<EntityId> &ids, int way)
        {
            for (const auto id : ids)
            {
                reach.hits.push_back({id, way});
            }
        };
        const auto standing_on = [&](const std::vector<Cell> &cells, int way)
        {
            std::vector<EntityId> found;
            for (const auto cell : cells)
            {
                if (const auto id = session.standing_at(cell);
                    id && std::ranges::find(found, id) == found.end())
                {
                    found.push_back(id);
                }
            }
            take(found, way);
        };
        const auto downed = [&](const auto &lies_in_area)
        {
            for (const auto &f : session.fighters_)
            {
                if (f.downed() && is_ally(f) && lies_in_area(f) &&
                    std::ranges::find(reach.downed, f.id) == reach.downed.end())
                {
                    reach.downed.push_back(f.id);
                }
            }
        };
        const auto downed_in_burst = [&](Cell at, int radius)
        {
            downed(
                [&](const Fighter &f)
                {
                    return in_burst(session.burst_gap(at, f), radius);
                });
        };
        const auto downed_on = [&](const std::vector<Cell> &cells)
        {
            downed(
                [&](const Fighter &f)
                {
                    return std::ranges::any_of(squares_of(f),
                                               [&](Cell square)
                                               {
                                                   return std::ranges::find(cells, square) !=
                                                          cells.end();
                                               });
                });
        };
        const int radius = spell_.area & 7;
        switch (number_)
        {
        case 47:
        case 64:
            take(session.fireball_reach(burst_targets(aim), aim), burst_way);
            downed_in_burst(aim, session.outdoors_ ? 2 : radius);
            break;
        case 21:
            take(session.within(aim, radius), burst_way);
            downed_in_burst(aim, radius);
            break;
        case 184:
        case 192:
        case 198:
        {
            const int length = number_ == 184 ? 6 : number_ == 192 ? cone_length(level_) : 3;
            const int width = number_ == 184 ? 3 : number_ == 192 ? 2 : 1;
            take(session.path_area(caster_, aim, length, width), burst_way);
            downed_on(session.path_squares(caster_, aim, length, width));
            break;
        }
        case 34:
        case 191:
        case 301:
        {
            const auto kind = number_ == 34    ? CloudKind::stinking
                              : number_ == 191 ? CloudKind::deadly
                                               : CloudKind::blades;
            const auto cloud = session.cloud_over(caster_.id, 0, aim, kind);
            const auto squares = cloud.squares();
            std::vector<Cell> covered;
            for (std::size_t n = 0; n < squares.size(); ++n)
            {
                if (cloud.covers[n])
                {
                    covered.push_back(squares[n]);
                }
            }
            standing_on(covered, burst_way);
            downed_on(covered);
            break;
        }
        case 51:
        case 60:
        {
            const bool javelin = number_ == 60;
            if (const auto id = session.standing_at(aim))
            {
                reach.hits.push_back({id, javelin ? mark_way : bolt_way});
            }
            std::vector<Cell> line;
            Cell from = aim;
            line.push_back(aim);
            session.bolt_walk(caster_.cell, aim, javelin ? 3 : session.bolt_length(), !javelin,
                              [&](Cell cell)
                              {
                                  if (const auto id = session.standing_at(cell))
                                  {
                                      reach.hits.push_back({id, bolt_way});
                                  }
                                  LineWalk walk(from, cell);
                                  while (walk.next())
                                  {
                                      line.push_back(walk.at);
                                  }
                                  from = cell;
                                  return session.turns_bolt(cell);
                              });
            downed_on(line);
            break;
        }
        case 324:
            for (const auto offset : meteor_landings)
            {
                const auto fall = session.meteor_fall(caster_, aim, offset);
                take(fall.struck, meteor_path_way);
                take(session.within(fall.landing, 1), meteor_burst_way);
                std::vector<Cell> path;
                LineWalk walk(caster_.cell, fall.landing);
                while (walk.next())
                {
                    path.push_back(walk.at);
                }
                downed_on(path);
                downed_in_burst(fall.landing, 1);
            }
            break;
        default:
            take(burst_targets(aim), burst_way);
            downed_in_burst(aim, radius);
            break;
        }
        return reach;
    }

    [[nodiscard]] const Fighter &fighter(EntityId id) const
    {
        return *session_.find(id);
    }

    // The ways the spell's shared dice fall at an aim, and each creature's
    // fate in each, as its resolver plays it.
    [[nodiscard]] std::vector<Scenario> scenarios(const Reach &reach)
    {
        switch (number_)
        {
        case 47:
        case 64:
        case 187:
        case 192:
        case 198:
        case 315:
            return damage_scenarios(reach);
        case 51:
        case 60:
            return bolt_scenarios(reach);
        case 324:
            return meteor_scenarios(reach);
        case 21:
            return sleep_scenarios(reach);
        case 182:
            return confusion_scenarios(reach);
        case 310:
            return death_spell_scenarios(reach);
        default:
            return {fixed_scenario(reach)};
        }
    }

    // Spells of damage rolled once for all they reach (resolve_spell).
    [[nodiscard]] std::vector<Scenario> damage_scenarios(const Reach &reach)
    {
        struct Roll
        {
            double chance{};
            int total{}, dice{};
        };

        std::vector<Roll> rolls;
        // A necklace's missile (1d3) sets its dice; Delayed Blast
        // Fireball's are rolled one at a time, which the kit does not
        // weigh by.
        for (int missile = 1; missile <= (number_ == 64 ? 3 : 1); ++missile)
        {
            const auto dice = shared_dice(number_, level_, missile);
            const auto totals = dice_chances(dice.count, dice.sides, dice.bonus);
            const double weight = number_ == 64 ? 1.0 / 3 : 1.0;
            for (std::size_t total = 0; total < totals.size(); ++total)
            {
                if (totals[total] > 0)
                {
                    rolls.push_back({weight * totals[total], static_cast<int>(total),
                                     number_ == delayed_blast_spell ? 0 : dice.count});
                }
            }
        }
        const int element = number_ == 187   ? session_.ice_storm_element()
                            : number_ == 192 ? 10
                            : number_ == 198 ? 8
                                             : 9;
        // A blast takes a plant whole, with no save.
        constexpr int plant = 18;
        std::vector<int> save_results;
        std::vector<double> saves;
        for (const auto &hit : reach.hits)
        {
            const auto &target = fighter(hit.id);
            const bool whole = number_ == 198 && target.numbers.creature_type == plant;
            save_results.push_back(whole ? 0 : spell_.save_result);
            saves.push_back(
                save_results.back() != 0
                    ? session_.save_chance(target, spell_.save_type, 0, &caster_, element)
                    : 0.0);
        }
        std::vector<Scenario> scenarios;
        for (const auto &roll : rolls)
        {
            Scenario scenario{roll.chance, {}};
            for (std::size_t n = 0; n < reach.hits.size(); ++n)
            {
                const auto &target = fighter(reach.hits[n].id);
                Fate fate;
                fate.loss = struck(target, roll.total, saves[n], save_results[n], element, number_,
                                   roll.dice);
                add_fate(scenario, target, std::move(fate));
            }
            scenarios.push_back(std::move(scenario));
        }
        return scenarios;
    }

    // Lightning Bolt's one roll at its aim and each square its bolt
    // strikes, or Javelin of Lightning's 1d6+20 at its mark and 20 along its
    // bolt; each strike saved for half against spells (lightning_at).
    [[nodiscard]] std::vector<Scenario> bolt_scenarios(const Reach &reach)
    {
        constexpr int bolt_element = 12, bolt_spell = 51;
        std::vector<std::pair<double, int>> rolls;
        int damage_dice = 0;
        if (number_ == 60)
        {
            rolls.emplace_back(1.0, 20);
        }
        else
        {
            const auto dice = shared_dice(number_, level_);
            damage_dice = dice.count;
            const auto totals = dice_chances(dice.count, dice.sides, dice.bonus);
            for (std::size_t total = 0; total < totals.size(); ++total)
            {
                if (totals[total] > 0)
                {
                    rolls.emplace_back(totals[total], static_cast<int>(total));
                }
            }
        }
        std::vector<double> saves;
        for (const auto &hit : reach.hits)
        {
            saves.push_back(session_.save_chance(fighter(hit.id), 4, 0, nullptr, bolt_element));
        }
        std::vector<Scenario> scenarios;
        for (const auto [chance, total] : rolls)
        {
            Scenario scenario{chance, {}};
            for (std::size_t n = 0; n < reach.hits.size(); ++n)
            {
                const auto &target = fighter(reach.hits[n].id);
                Fate fate;
                fate.loss =
                    reach.hits[n].way == mark_way
                        ? struck_by_dice(target, {1, 6, 20}, saves[n], 2, bolt_element, bolt_spell)
                        : struck(target, total, saves[n], 2, bolt_element, bolt_spell, damage_dice);
                add_fate(scenario, target, std::move(fate));
            }
            scenarios.push_back(std::move(scenario));
        }
        return scenarios;
    }

    // Meteor Swarm's meteors, each striking for its own 10d4 on its way and
    // bursting on those around where it lands, saved for half
    // (cast_meteor_swarm).
    [[nodiscard]] std::vector<Scenario> meteor_scenarios(const Reach &reach)
    {
        constexpr int fire_and_magic = 9;
        Scenario scenario;
        for (const auto &hit : reach.hits)
        {
            const auto &target = fighter(hit.id);
            const bool burst = hit.way == meteor_burst_way;
            const double save =
                burst ? session_.save_chance(target, 4, 0, &caster_, fire_and_magic) : 0.0;
            Fate fate;
            fate.loss =
                struck_by_dice(target, meteor_dice, save, burst ? 2 : 0, fire_and_magic, number_);
            add_fate(scenario, target, std::move(fate));
        }
        return {scenario};
    }

    // Sleep's 4d4 levels, spent on the nearest first (sleep_takes), each
    // asleep unless its immunities refuse it.
    [[nodiscard]] std::vector<Scenario> sleep_scenarios(const Reach &reach)
    {
        const auto pools = dice_chances(4, 4, 0);
        std::vector<Scenario> scenarios;
        for (std::size_t pool = 0; pool < pools.size(); ++pool)
        {
            if (pools[pool] <= 0)
            {
                continue;
            }
            Scenario scenario{pools[pool], {}};
            for (const auto id : session_.sleep_takes(reach.aim, static_cast<int>(pool)))
            {
                const auto &target = fighter(id);
                add_fate(scenario, target,
                         disabled_by(1 - refusal(target, effect::sleep, level_, spell_)));
            }
            scenarios.push_back(std::move(scenario));
        }
        return scenarios;
    }

    // Confusion takes at most 2d8 of its targets, the nearest first.
    [[nodiscard]] std::vector<Scenario> confusion_scenarios(const Reach &reach)
    {
        constexpr int confused = 235;
        const auto counts = dice_chances(2, 8, 0);
        std::vector<Scenario> scenarios;
        for (std::size_t count = 0; count < counts.size(); ++count)
        {
            if (counts[count] <= 0)
            {
                continue;
            }
            Scenario scenario{counts[count], {}};
            for (std::size_t n = 0; n < reach.hits.size() && n < count; ++n)
            {
                const auto &target = fighter(reach.hits[n].id);
                add_fate(scenario, target, disabled_by(effect_takes(target, confused, 0)));
            }
            scenarios.push_back(std::move(scenario));
        }
        return scenarios;
    }

    // Death Spell's 4d20 x 2 to spend on its targets by their levels
    // (death_spell_takes), weighing an enemy's most Hit Points as the
    // caster knows them.
    [[nodiscard]] std::vector<Scenario> death_spell_scenarios(const Reach &reach)
    {
        std::vector<EntityId> targets;
        for (const auto &hit : reach.hits)
        {
            targets.push_back(hit.id);
        }
        const auto budgets = dice_chances(4, 20, 0);
        std::vector<Scenario> scenarios;
        for (std::size_t budget = 0; budget < budgets.size(); ++budget)
        {
            if (budgets[budget] <= 0)
            {
                continue;
            }
            Scenario scenario{budgets[budget], {}};
            for (const auto id : session_.death_spell_takes(targets, static_cast<int>(budget) * 2,
                                                            known_most_hit_points()))
            {
                add_fate(scenario, fighter(id), slain(fighter(id), 1.0));
            }
            scenarios.push_back(std::move(scenario));
        }
        return scenarios;
    }

    // The spells with no dice shared among those they reach.
    [[nodiscard]] Scenario fixed_scenario(const Reach &reach)
    {
        Scenario scenario;
        std::vector<EntityId> targets;
        for (const auto &hit : reach.hits)
        {
            targets.push_back(hit.id);
        }
        const auto fate_of = [&](EntityId id, Fate fate)
        {
            add_fate(scenario, fighter(id), std::move(fate));
        };
        switch (number_)
        {
        case 34:
            for (const auto id : targets)
            {
                fate_of(id, disabled_by(sickening(fighter(id))));
            }
            break;
        case 191:
            for (const auto id : targets)
            {
                fate_of(id, poisoning(fighter(id)));
            }
            break;
        case 301:
            for (const auto id : targets)
            {
                fate_of(id, cutting(fighter(id)));
            }
            break;
        case 55:
            // Slow: up to the caster's level of its enemies, a hasted one
            // only losing its Haste (resolve_spell).
            for (const auto id : session_.haste_slow_takes(caster_, number_, level_, targets))
            {
                const auto &target = fighter(id);
                Fate fate;
                if (target.has(effect::hasted))
                {
                    fate.helps_lost = 1;
                }
                else
                {
                    fate.disabled = effect_takes(target, effect::slowed, 0);
                }
                fate_of(id, std::move(fate));
            }
            break;
        case 178:
            // Entangle holds those who fail their saves, out of doors only.
            for (const auto id : session_.outdoors_ ? targets : std::vector<EntityId>{})
            {
                const auto &target = fighter(id);
                const double save = session_.save_chance(target, 4, 0, &caster_);
                fate_of(id, disabled_by((1 - save) *
                                        (1 - refusal(target, effect::held_still, level_, spell_))));
            }
            break;
        case 184:
            // Fear: those who fail their saves flee.
            for (const auto id : targets)
            {
                const auto &target = fighter(id);
                const double save = session_.save_chance(target, 4, 0, &caster_);
                fate_of(id, disabled_by((1 - save) *
                                        (1 - refusal(target, effect::fear, level_, spell_))));
            }
            break;
        case 320:
            for (const auto [id, take] : session_.mass_charm_takes(caster_, level_, targets))
            {
                if (take == MassCharmTake::tried)
                {
                    const auto &target = fighter(id);
                    const double save = session_.save_chance(target, 4, -2, &caster_);
                    fate_of(id, disabled_by((1 - save) *
                                            (1 - refusal(target, effect::charm, level_, spell_))));
                }
            }
            break;
        case 323:
            for (const auto id : session_.blinded_by_word(targets, known_most_hit_points()))
            {
                Fate fate;
                fate.hindrances_gained = 1 - refusal(fighter(id), effect::blind, 0, spell_);
                fate_of(id, std::move(fate));
            }
            break;
        case 325:
            for (const auto id : session_.slain_by_word(targets, known_most_hit_points()))
            {
                const auto &target = fighter(id);
                const auto *images = target.find_effect(effect::mirror_image);
                const double turned =
                    target.has(effect::iron_golem) ? 1.0
                    : images ? static_cast<double>(images->strength) / (images->strength + 1)
                             : 0.0;
                fate_of(id, slain(target, 1 - turned));
            }
            break;
        case 41:
        case 46:
            for (const auto id : targets)
            {
                fate_of(id, dispelling(fighter(id)));
            }
            break;
        default:
            break;
        }
        return scenario;
    }

    // The chance a stinking cloud laid on a creature leaves it helpless with
    // nausea (breathe_stinking_cloud, breathe_darkness_stench).
    [[nodiscard]] double sickening(const Fighter &f) const
    {
        const bool spared =
            session_.darkness()
                ? f.numbers.undead_level > 0 || f.has(effect::nausea) ||
                      (f.numbers.traits & (trait::unliving | trait::poison_proof)) != 0
                : f.has(effect::coughing) || f.has(effect::nausea) || f.has(effect::animated) ||
                      f.has(111) || f.has(125) || session_.breathes_clouds_freely(f);
        if (spared)
        {
            return 0;
        }
        const double save = session_.save_chance(f, 0, 0);
        return (1 - save) * (1 - refusal(f, effect::nausea, 0, *session_.game_spell(34)));
    }

    // What a cloudkill laid on a creature does (breathe_cloudkill,
    // breathe_darkness_cloudkill).
    [[nodiscard]] Fate poisoning(const Fighter &f)
    {
        const int level = f.numbers.level;
        if (session_.darkness())
        {
            if ((f.numbers.traits & trait::poison_proof) != 0)
            {
                return {};
            }
            if (level > 6)
            {
                constexpr int magical_poison = 0x88;
                Fate fate;
                fate.loss = struck_by_dice(f, {1, 10, 0}, 0, 0, magical_poison, number_);
                return fate;
            }
        }
        else if (level > 6)
        {
            return {};
        }
        const double save = level >= 5 ? session_.save_chance(f, 0, level == 5 ? -4 : 0) : 0.0;
        return slain(f, 1 - save);
    }

    // What a Blade Barrier rising on a creature does: unless magic
    // resistance turns it or a save, 8d8 (cut_by_blades).
    [[nodiscard]] Fate cutting(const Fighter &f)
    {
        double resisted = 0;
        if (resists_magic_at_all(f))
        {
            each_way<bool>(
                [&](auto &&dice)
                {
                    return magic_resists(f, level_, dice(100));
                },
                [&](double way, bool resists)
                {
                    resisted += resists ? way : 0;
                });
        }
        const double cut = (1 - resisted) * (1 - session_.save_chance(f, 4, 0, &caster_));
        Fate fate;
        fate.loss = struck_by_dice(f, blade_dice, 0, 0, 0, number_);
        for (auto &chance : fate.loss)
        {
            chance *= cut;
        }
        fate.loss.front() += 1 - cut;
        return fate;
    }

    // What Dispel Magic likely takes from a creature (dispel): each effect
    // it may take goes by its own chance, counted as helping or hindering
    // the creature, and the creature is freed once every effect that
    // disables it goes.
    [[nodiscard]] Fate dispelling(const Fighter &f) const
    {
        Fate fate;
        bool disabled = false;
        double freed = 1;
        for (const auto &e : f.numbers.effects)
        {
            const bool disables =
                std::ranges::find(disabling_effects, e.code) != disabling_effects.end();
            const double chance =
                session_.dispellable(e)
                    ? std::clamp(session_.dispel_chance(e, level_), 0, 100) / 100.0
                    : 0.0;
            if (disables)
            {
                disabled = true;
                freed *= chance;
            }
            else if (std::ranges::find(helping_effects, e.code) != helping_effects.end())
            {
                fate.helps_lost += chance;
            }
            else if (std::ranges::find(hindering_effects, e.code) != hindering_effects.end())
            {
                fate.hindrances_lost += chance;
            }
        }
        fate.freed = disabled ? freed : 0.0;
        return fate;
    }

    // Each creature's outlook from the spell's scenarios: the chances of each
    // fate weighed together, and the chance every able enemy goes down in
    // the same fall of the dice. A member already down in the area counts as
    // slain.
    [[nodiscard]] rules::AreaOutlook summed(const Reach &reach,
                                            const std::vector<Scenario> &scenarios)
    {
        rules::AreaOutlook outlook;
        std::vector<EntityId> ids;
        for (const auto &hit : reach.hits)
        {
            if (std::ranges::find(ids, hit.id) == ids.end())
            {
                ids.push_back(hit.id);
            }
        }
        for (const auto id : ids)
        {
            const auto &target = fighter(id);
            const auto &k = known(target);
            rules::CreatureOutlook creature;
            creature.id = id;
            creature.ally = k.ally;
            creature.hit_points = k.hit_points;
            creature.max_hit_points = k.max_hit_points;
            creature.weight = k.weight;
            creature.enemy_beside = k.enemy_beside;
            creature.loss.assign(static_cast<std::size_t>(cap_of(target)) + 1, 0.0);
            for (const auto &scenario : scenarios)
            {
                const auto found = scenario.fates.find(id);
                if (found == scenario.fates.end())
                {
                    creature.loss.front() += scenario.chance;
                    continue;
                }
                const auto &fate = found->second;
                for (std::size_t lost = 0; lost < fate.loss.size(); ++lost)
                {
                    creature.loss[lost] += scenario.chance * fate.loss[lost];
                }
                creature.disabled += scenario.chance * fate.disabled;
                creature.helps_lost += scenario.chance * fate.helps_lost;
                creature.hindrances_lost += scenario.chance * fate.hindrances_lost;
                creature.hindrances_gained += scenario.chance * fate.hindrances_gained;
                creature.freed += scenario.chance * fate.freed;
            }
            outlook.creatures.push_back(std::move(creature));
        }
        for (const auto id : reach.downed)
        {
            const auto &target = fighter(id);
            rules::CreatureOutlook creature;
            creature.id = id;
            creature.ally = true;
            creature.hit_points = std::min(target.hit_points, 0);
            creature.max_hit_points = target.numbers.max_hit_points;
            outlook.creatures.push_back(std::move(creature));
        }
        for (const auto &scenario : scenarios)
        {
            double all_down = scenario.chance;
            for (const auto id : enemy_ids_)
            {
                const auto found = scenario.fates.find(id);
                if (found == scenario.fates.end())
                {
                    all_down = 0;
                    break;
                }
                const int hit_points = known(fighter(id)).hit_points;
                double down = found->second.disabled;
                for (auto lost = static_cast<std::size_t>(hit_points);
                     lost < found->second.loss.size(); ++lost)
                {
                    down += found->second.loss[lost];
                }
                all_down *= std::min(down, 1.0);
            }
            outlook.every_enemy_down += all_down;
        }
        return outlook;
    }
};

// Whether the computer weighs this spell's outlook for its caster: an area
// spell of a creature on the party's side.
bool Session::weighs_area_spell(const Fighter &f, int number) const
{
    return f.side == Side::party && Forecast::weighs(number);
}

// The best aim a party-side caster's outlook allows for an area spell
// (Forecast::best_aim), none when no aim passes the player's limits.
std::optional<Cell> Session::party_area_aim(const Fighter &f, int number) const
{
    const auto &known = *game_spell(number);
    Forecast forecast(*this, f, number, caster_level(f, known, game_of(identity_)));
    return forecast.best_aim(party_spell_limits_);
}

// A party-side caster's area spell goes off at its best aim; false, the
// spell given up, when no aim passes.
bool Session::aim_party_area_spell(Fighter &f)
{
    const auto aim = party_area_aim(f, f.aiming);
    if (!aim)
    {
        log({"{name} aborts the spell.", {{"name", f.name}}});
        return false;
    }
    const int area = game_spell(f.aiming)->area & 0x0F;
    resolve_spell(f,
                  area >= 8 && area <= 14 ? within(*aim, area & 7)
                                          : std::vector<EntityId>{standing_at(*aim)},
                  *aim);
    return true;
}

std::optional<rules::AreaOutlook> Session::area_outlook(EntityId caster, unsigned spell,
                                                        Cell aim) const
{
    const auto *f = find(caster);
    const int number = static_cast<int>(spell);
    const auto *known = game_spell(number);
    if (!f || !known || !Forecast::weighs(number))
    {
        return std::nullopt;
    }
    Forecast forecast(*this, *f, number, caster_level(*f, *known, game_of(identity_)));
    return forecast.outlook(aim);
}

// The roll a cleric needs to turn undead, by the undead's level (1 skeleton to
// 10 vampire) and the cleric's level (1 to 8, 9 to 13, 14 up): above 0 it
// turns, 0 destroys, below 0 destroys on that much or more, 99 never
// (cbtacts.pas, CombatTurnUndead).
// Pools of Darkness's goes on to undead of levels 11 to 13 (its
// turnUndeadTable); the earlier games turn none above 10.
constexpr std::array<std::array<int, 10>, 13> turning_table{{
    {10, 7, 4, 1, 1, 0, 0, -1, -1, -1},
    {13, 10, 7, 1, 1, 0, 0, 0, -1, -1},
    {16, 13, 10, 4, 1, 1, 0, 0, 0, -1},
    {19, 16, 13, 7, 4, 1, 1, 0, 0, -1},
    {20, 19, 16, 10, 7, 4, 1, 1, 0, 0},
    {99, 20, 19, 13, 10, 7, 4, 1, 1, 0},
    {99, 99, 20, 16, 13, 10, 7, 4, 1, 0},
    {99, 99, 99, 20, 16, 13, 10, 7, 4, 1},
    {99, 99, 99, 99, 20, 16, 13, 10, 7, 1},
    {99, 99, 99, 99, 99, 20, 16, 13, 10, 4},
    {99, 99, 99, 99, 99, 99, 20, 16, 13, 10},
    {99, 99, 99, 99, 99, 99, 99, 19, 16, 10},
    {99, 99, 99, 99, 99, 99, 99, 20, 19, 13},
}};

// Turning undead, once a fight (cbtacts.pas, CombatTurnUndead): one d20 for
// the whole attempt against up to 1d12 undead enemies in sight, the lowest
// first and the nearest of those; each it beats is turned, or destroyed, and
// the first it fails ends the attempt. Destruction goes on to at least six.
// The cleric's actions end.
void Session::turn_undead(Fighter &cleric)
{
    if (treasures())
    {
        treasures_turn_undead(cleric);
        return;
    }
    if (darkness() || game_of(identity_) == GoldBoxGame::dark_queen_of_krynn)
    {
        darkness_turn_undead(cleric);
        return;
    }
    cleric.tried_turning = true;
    log({"{name} turns undead.", {{"name", cleric.name}}});
    const int level = cleric.numbers.class_levels[static_cast<std::size_t>(ClassSlot::cleric)];
    const auto column = static_cast<std::size_t>(level <= 8 ? level - 1 : level <= 13 ? 8 : 9);
    int budget = roll(12);
    int at_least = 6;
    const int natural = roll(20);
    bool any = false;
    while (budget > 0)
    {
        Fighter *target = nullptr;
        for (auto &f : fighters_)
        {
            if (f.side == cleric.side || !f.able() || f.turned || f.numbers.undead_level < 1 ||
                f.numbers.undead_level > 10 || !sees(board_, cleric, f))
            {
                continue;
            }
            if (!target || f.numbers.undead_level < target->numbers.undead_level ||
                (f.numbers.undead_level == target->numbers.undead_level &&
                 distance(cleric, f) < distance(cleric, *target)))
            {
                target = &f;
            }
        }
        if (!target)
        {
            break;
        }
        const int need =
            turning_table[static_cast<std::size_t>(target->numbers.undead_level - 1)][column];
        if (natural < std::abs(need))
        {
            break;
        }
        any = true;
        if (need > 0)
        {
            target->turned = true;
            log({"{name} is turned.", {{"name", target->name}}});
        }
        else
        {
            target->condition = Condition::gone;
            target->hit_points = 0;
            target->initiative = 0;
            log({"{name} is destroyed.", {{"name", target->name}}});
            end_effects_at_zero(*target);
        }
        at_least = std::max(at_least - 1, 0);
        --budget;
        if (budget == 0 && at_least > 0 && need < 0)
        {
            budget = 1;
        }
    }
    if (!any)
    {
        log({"Nothing happens.", {}});
    }
    end_actions(cleric);
}

// Pools of Darkness's turning (cbtacts.pas, CombatTurnUndead and
// FindTurnUndeadTarget): its column is the better of the cleric's level and
// the paladin's less 2 (to 8, 9 to 13, 14 up), less the area's penalty
// (rules::Encounter::turning_penalty), which may leave none to turn with.
// One d20 for the attempt. The undead enemy in sight of the highest level not
// above the last one tried comes first, the nearest of those; one the roll
// beats is turned off the field or destroyed, and one it fails sends the
// attempt on to the levels below. The first one tried sets how many it may
// affect: 1d6+6 where it would be destroyed past doubt, else 1d12.
// Destruction goes on to at least six. The turner's actions end.
void Session::darkness_turn_undead(Fighter &cleric)
{
    ++cleric.turn_attempts;
    log({"{name} turns undead.", {{"name", cleric.name}}});
    const auto level = [&cleric](ClassSlot slot)
    {
        return cleric.numbers.class_levels[static_cast<std::size_t>(slot)];
    };
    const bool native = game_of(identity_) == GoldBoxGame::dark_queen_of_krynn;
    const int deity_bonus = native && cleric.record && cleric.record->deity == 2 ? 2 : 0;
    const int best =
        std::max(level(ClassSlot::cleric), level(ClassSlot::paladin) - 2) + deity_bonus;
    const int full_column = best <= 8 ? best : best <= 13 ? 9 : 10;
    const int column = turning_penalty_ > full_column ? 0 : full_column - turning_penalty_;
    const int natural = roll(20);
    int highest = static_cast<int>(turning_table.size());
    std::optional<int> budget;
    int at_least = 6;
    bool any = false;
    while (!budget || *budget > 0)
    {
        Fighter *target = nullptr;
        for (auto &f : fighters_)
        {
            const int undead = f.numbers.undead_level;
            if (f.side == cleric.side || !f.able() || f.turned || undead < 1 || undead > highest ||
                !sees(board_, cleric, f))
            {
                continue;
            }
            if (!target || undead > target->numbers.undead_level ||
                (undead == target->numbers.undead_level &&
                 distance(cleric, f) < distance(cleric, *target)))
            {
                target = &f;
            }
        }
        if (!target)
        {
            break;
        }
        highest = target->numbers.undead_level;
        constexpr int never = 99;
        const int need = column == 0 ? never
                                     : turning_table[static_cast<std::size_t>(highest - 1)]
                                                    [static_cast<std::size_t>(column - 1)];
        if (!budget)
        {
            budget = need < 0 ? roll(6) + 6 : roll(12);
        }
        if (natural < std::abs(need))
        {
            --highest;
            continue;
        }
        any = true;
        if (need > 0)
        {
            log({"{name} is turned.", {{"name", target->name}}});
            put_out_of_play(*target, Condition::fled);
        }
        else
        {
            target->condition = Condition::gone;
            target->hit_points = 0;
            target->initiative = 0;
            log({"{name} is destroyed.", {{"name", target->name}}});
            end_effects_at_zero(*target);
        }
        at_least = std::max(at_least - 1, 0);
        --*budget;
        if (*budget == 0 && at_least > 0 && need < 0)
        {
            budget = 1;
        }
    }
    if (!any)
    {
        log({"Nothing happens.", {}});
    }
    end_actions(cleric);
}

// Whether the creature may turn undead now: one with cleric levels, once a
// fight (combat.pas, CombatMenu), but in Secret, which has no turning. In
// Treasures a cleric or a paladin past level 2, as many times a fight as
// the undead the fight began with (its combat.pas, CombatMenu; eclvm.pas,
// LOAD MONSTER's undeadCount). In Pools of Darkness a cleric or a paladin past
// level 2, as often as the fight's LOAD MONSTERs brought undead
// (turnUndeadLimit).
bool Session::may_turn(const Fighter &f) const
{
    const auto &levels = f.numbers.class_levels;
    if (darkness() || game_of(identity_) == GoldBoxGame::dark_queen_of_krynn)
    {
        return (levels[static_cast<std::size_t>(ClassSlot::cleric)] > 0 ||
                levels[static_cast<std::size_t>(ClassSlot::paladin)] > 2) &&
               f.turn_attempts < turning_tries_;
    }
    if (!treasures())
    {
        return levels[static_cast<std::size_t>(ClassSlot::cleric)] > 0 && !f.tried_turning &&
               !secret();
    }
    const auto undead = std::ranges::count_if(fighters_,
                                              [](const Fighter &o)
                                              {
                                                  return o.numbers.undead_level > 0;
                                              });
    return (levels[static_cast<std::size_t>(ClassSlot::cleric)] > 0 ||
            levels[static_cast<std::size_t>(ClassSlot::paladin)] > 2) &&
           f.turn_attempts < undead;
}

// Treasures' table (its gdconsts.pas, turnUndeadTable): undead levels 1 to
// 13, by the turning column 0 (none) to 10, read as turning_table.
constexpr std::array<std::array<int, 11>, 13> treasures_turning_table{{
    {99, 10, 7, 4, 1, 1, 0, 0, -1, -1, -1},
    {99, 13, 10, 7, 1, 1, 0, 0, 0, -1, -1},
    {99, 16, 13, 10, 4, 1, 1, 0, 0, 0, -1},
    {99, 19, 16, 13, 7, 4, 1, 1, 0, 0, -1},
    {99, 20, 19, 16, 10, 7, 4, 1, 1, 0, 0},
    {99, 99, 20, 19, 13, 10, 7, 4, 1, 1, 0},
    {99, 99, 99, 20, 16, 13, 10, 7, 4, 1, 0},
    {99, 99, 99, 99, 20, 16, 13, 10, 7, 4, 1},
    {99, 99, 99, 99, 99, 20, 16, 13, 10, 7, 1},
    {99, 99, 99, 99, 99, 99, 20, 16, 13, 10, 4},
    {99, 99, 99, 99, 99, 99, 99, 20, 16, 13, 10},
    {99, 99, 99, 99, 99, 99, 99, 99, 19, 16, 10},
    {99, 99, 99, 99, 99, 99, 99, 99, 20, 19, 13},
}};

// The strongest undead enemy in sight, not turned, of a level no higher
// than given, the nearest of equals (its cbtacts.pas, FindTurnUndeadTarget).
Fighter *Session::strongest_undead(const Fighter &turner, int highest_level)
{
    Fighter *target = nullptr;
    for (auto &f : fighters_)
    {
        const int level = f.numbers.undead_level;
        if (f.side == turner.side || !f.able() || f.turned || level < 1 || level > highest_level ||
            !sees(board_, turner, f))
        {
            continue;
        }
        if (!target || level > target->numbers.undead_level ||
            (level == target->numbers.undead_level &&
             distance(turner, f) < distance(turner, *target)))
        {
            target = &f;
        }
    }
    return target;
}

// Treasures' turning (its cbtacts.pas, CombatTurnUndead): its column is the
// cleric level or the paladin level less 2, whichever is more, 9 from 9 to
// 13 and 10 from 14. One d20 is rolled against the strongest undead in sight
// first; each it beats is turned or destroyed, up to 1d12, or 1d6+6 when
// the first is destroyed, destruction going on to at least six; a roll it
// fails sends it to the weaker undead. The turner's actions end. (The area's
// turning penalty, its cell 0Fh, no Treasures script sets.)
void Session::treasures_turn_undead(Fighter &turner)
{
    ++turner.turn_attempts;
    log({"{name} turns undead.", {{"name", turner.name}}});
    const auto &levels = turner.numbers.class_levels;
    int column = std::max(levels[static_cast<std::size_t>(ClassSlot::cleric)],
                          levels[static_cast<std::size_t>(ClassSlot::paladin)] - 2);
    if (column > 8)
    {
        column = column < 14 ? 9 : 10;
    }
    column = std::max(column, 0);
    constexpr int strongest = 13;
    int highest_level = strongest, at_least = 6, left = -1;
    const int natural = roll(20);
    bool any = false;
    while (left != 0)
    {
        auto *target = strongest_undead(turner, highest_level);
        if (!target)
        {
            break;
        }
        const int need =
            treasures_turning_table[static_cast<std::size_t>(target->numbers.undead_level - 1)]
                                   [static_cast<std::size_t>(column)];
        if (left < 0)
        {
            left = need < 0 ? roll(6) + 6 : roll(12);
        }
        if (natural < std::abs(need))
        {
            --highest_level;
            continue;
        }
        any = true;
        if (need > 0)
        {
            target->turned = true;
            log({"{name} is turned.", {{"name", target->name}}});
        }
        else
        {
            target->condition = Condition::gone;
            target->hit_points = 0;
            target->initiative = 0;
            log({"{name} is destroyed.", {{"name", target->name}}});
            end_effects_at_zero(*target);
        }
        at_least = std::max(at_least - 1, 0);
        --left;
        if (left == 0 && at_least > 0 && need < 0)
        {
            left = 1;
        }
    }
    if (!any)
    {
        log({"Nothing happens.", {}});
    }
    end_actions(turner);
}

// Killed outright, whatever its Hit Points.
void Session::kill(Fighter &victim, std::string_view message)
{
    if (victim.has(effect::aurak_invulnerable))
    {
        return;
    }
    victim.condition = Condition::dead;
    victim.hit_points = 0;
    victim.initiative = 0;
    log({std::string(message), {{"name", victim.name}}});
    end_effects_at_zero(victim);
    went_down(victim);
}

// Both claws hit: two rakes of 2d4 with the rear claws, then its actions end
// (cbtacts.pas, RearClawRake).
void Session::rake(Fighter &attacker, Fighter &victim)
{
    auto &rear = attacker.numbers.attacks[1];
    const auto own = rear;
    rear = {own.rate_per_two_rounds, 2, 4, own.modifier};
    attacker.attacks_left = {0, 2};
    attacker.attack_phase = 1;
    log({"{attacker} rakes {name}.", {{"attacker", attacker.name}, {"name", victim.name}}});
    make_attack(attacker, victim, false);
    rear = own;
    attacker.attacks_left = {0, 0};
    end_actions(attacker);
}

// Curse's conditions at a turn's start (its reconstruction's ovr013): one
// entangled cannot move; a feebleminded one cannot cast; one fighting snakes
// spends its turn on them until its attacks wear them out; a confused one
// rolls what it does, then may shake it off.
void Session::curse_turn_start(Fighter &f)
{
    if (!f.able())
    {
        return;
    }
    if (f.has(336))
    {
        f.half_moves = 0;
    }
    // Gateway's held, engulfed or hugged cannot cast (its EffImmobile, at
    // ACTION_START).
    if (gateway() && f.has(effect::immobile))
    {
        f.cast_allowed = false;
    }
    if (f.has(268))
    {
        f.cast_allowed = false;
        f.item_use_allowed = false;
    }
    if (auto *snakes = f.find_effect(203))
    {
        log({"{name} is fighting with snakes.", {{"name", f.name}}});
        snakes->strength -= f.attacks_left[0] + f.attacks_left[1];
        if (snakes->strength <= 0)
        {
            f.remove(203);
        }
        end_actions(f);
        return;
    }
    if (f.has(235))
    {
        const int roll_100 = roll(100);
        if (roll_100 <= 10)
        {
            // It breaks and runs, afraid for 10 rounds.
            f.remove(235);
            f.fleeing = true;
            f.numbers.effects.push_back({342, 10, 0, true});
            log({"{name} flees.", {{"name", f.name}}});
            return;
        }
        if (roll_100 <= 60)
        {
            log({"{name} is confused.", {{"name", f.name}}});
            end_actions(f);
        }
        else if (roll_100 <= 80)
        {
            // Berserk for a round: against the nearest creature, of either side.
            const Fighter *nearest = nullptr;
            for (const auto &other : fighters_)
            {
                if (other.id != f.id && other.able() &&
                    (!nearest || distance(f, other) < distance(f, *nearest)))
                {
                    nearest = &other;
                }
            }
            if (nearest)
            {
                log({"{name} goes berserk.", {{"name", f.name}}});
                f.own_side = f.has(effect::charm) ? f.own_side : f.side;
                f.side = nearest->side == Side::party ? Side::opposition : Side::party;
                f.lock = nearest->id;
                f.numbers.effects.push_back({337, 1, 0, true});
            }
        }
        else
        {
            log({"{name} is enraged.", {{"name", f.name}}});
        }
        if (saving_throw(f, 4, -2))
        {
            f.remove(235);
        }
    }
}

// At its turn's start a creature holding a victim bites it from behind with
// 3d4, and a stirge drinks 1d4 from its victim; either spends its turn. A
// stirge that has drunk its fill lets go and flies off (effhndlr.pas, 74 and 75).
void Session::attached_turn(Fighter &f)
{
    // One engulfed suffocates when its count runs out (Curse, Suffocates).
    if (auto *air = f.find_effect(effect::suffocating); air && f.able() && --air->strength <= 0)
    {
        log({"{name} suffocates.", {{"name", f.name}}});
        f.remove(effect::suffocating);
        kill(f);
        return;
    }

    // A shambling mound crushes its engulfed victim with two blows of 2d8;
    // an owlbear squeezes its hugged one with one (Curse; its owlbear's
    // squeeze was never called, docs/ORIGINAL-RULES.md, Bugs).
    // Gateway's owlbear squeezes with two (its BiteAndHold), its stirge
    // drinks with two blows of 1d4, and its squid squeezes with two of 1d2
    // (its 156 was never called, docs/ORIGINAL-RULES.md, Bugs).
    struct Squeeze
    {
        int code, blows, sides;
    };

    for (const auto [code, blows, sides] :
         {Squeeze{effect::engulfing, 2, 8}, Squeeze{effect::hugging, gateway() ? 2 : 1, 8},
          Squeeze{effect::drinking_blood, 2, 4}, Squeeze{effect::constricting, 2, 2}})
    {
        const auto *hold = f.find_effect(code);
        if (!hold)
        {
            continue;
        }
        auto *victim = find(static_cast<EntityId>(hold->strength));
        if (!f.able() || !victim || !victim->able())
        {
            release(f, code);
            continue;
        }
        auto &blow = f.numbers.attacks[0];
        const auto own = blow;
        blow = {own.rate_per_two_rounds, sides == 8 ? 2 : 1, sides, 0};
        f.attacks_left = {blows, 0};
        f.attack_phase = 0;
        make_attack(f, *victim, false);
        blow = own;
        end_actions(f);
        if (!victim->able())
        {
            release(f, code);
        }
        return;
    }
    // A Treasures kampfult crushes its victim for 5, a yeti squeezes for
    // 2d8, 4d5 or 6d4 (EffKampfultCrush, T20GrabHelper).
    if (const auto *hold = f.find_effect(effect::squeezing))
    {
        auto *victim = find(static_cast<EntityId>(hold->strength));
        if (!f.able() || !victim || !victim->able())
        {
            release(f, effect::squeezing);
            return;
        }
        const auto dice = [this](int count, int sides)
        {
            int total = 0;
            for (int die = 0; die < count; ++die)
            {
                total += roll(sides);
            }
            return total;
        };
        const int damage = f.has(effect::crushes)     ? 5
                           : f.has(effect::grabs_6d4) ? dice(6, 4)
                           : f.has(effect::grabs_4d5) ? dice(4, 5)
                                                      : dice(2, 8);
        log({"{attacker} squeezes {name} for {damage}.",
             {{"attacker", f.name}, {"name", victim->name}, {"damage", std::to_string(damage)}}});
        inflict(*victim, damage);
        end_actions(f);
        if (!victim->able())
        {
            release(f, effect::squeezing);
        }
        return;
    }
    if (const auto *hold = f.find_effect(effect::bite_hold))
    {
        auto *victim = find(static_cast<EntityId>(hold->strength));
        if (!f.able() || !victim || !victim->able())
        {
            release(f, effect::bite_hold);
        }
        else
        {
            auto &bite = f.numbers.attacks[0];
            const auto own = bite;
            bite = {own.rate_per_two_rounds, 3, 4, own.modifier};
            f.attacks_left = {1, 0};
            f.attack_phase = 0;
            make_attack(f, *victim, true);
            bite = own;
            end_actions(f);
            if (!victim->able())
            {
                release(f, effect::bite_hold);
            }
        }
    }
    if (const auto *drain = f.find_effect(effect::blood_drain))
    {
        auto *victim = find(static_cast<EntityId>(drain->strength));
        if (!f.able() || !victim || !victim->able())
        {
            release(f, effect::blood_drain);
            return;
        }
        const int damage = roll(4);
        log({"{attacker} drinks {name}'s blood for {damage}.",
             {{"attacker", f.name}, {"name", victim->name}, {"damage", std::to_string(damage)}}});
        inflict(*victim, damage);
        end_actions(f);
        auto *left = f.find_effect(effect::blood_drain);
        if (left && victim->able() && left->minutes / 16 > damage)
        {
            left->minutes -= damage * 16;
            return;
        }
        release(f, effect::blood_drain);
        if (f.able() && victim->able())
        {
            f.turned = true;
        }
    }
}

// An attack on a helpless creature is one blow of its Hit Points and 5 more,
// which leaves it dying 5 below 0; the attacker's attacks are spent
// (cbtacts.pas, RollAttacks).
void Session::slay_helpless(Fighter &attacker, Fighter &defender)
{
    log({"{attacker} slays the helpless {name}.",
         {{"attacker", attacker.name}, {"name", defender.name}}});
    inflict(defender, defender.hit_points + 5);
    attacker.attacks_left = {0, 0};
    attacker.sweep_budget = 0;
    end_actions(attacker);
}

// A round is a minute: every creature's timed effects age by one, and those
// with a minute or less left end (fxengine.pas, the clock's advance).
void Session::age_effects()
{
    for (auto &f : fighters_)
    {
        auto &effects = f.numbers.effects;
        std::vector<Effect> ended;
        std::erase_if(effects,
                      [&ended](const Effect &e)
                      {
                          if (e.minutes == 1)
                          {
                              ended.push_back(e);
                          }
                          return e.minutes == 1;
                      });
        for (auto &e : effects)
        {
            e.minutes = e.minutes > 1 ? e.minutes - 1 : e.minutes;
        }
        for (const auto &e : ended)
        {
            expire(f, e);
        }
    }
}

// An effect whose time ran out, by its handler (effhndlr.pas): a troll's
// regeneration starts 3 rounds after its first wound; one fighting on past
// its death falls dead; a troll comes back with all its Hit Points when its
// square is free, or tries again a minute later.
void Session::expire(Fighter &f, const Effect &ended)
{
    switch (ended.code)
    {
    case dq_effect::green_fire:
        if (f.downed())
        {
            kill(f, "{name} is engulfed by green fire!");
        }
        else if (f.able() && f.hit_points < f.numbers.max_hit_points)
        {
            const int damage = dark_queen_green_fire_damage(roll(8), f.numbers.effects, random_);
            inflict(f, damage);
            if (f.hit_points <= 0)
            {
                kill(f, "{name} is engulfed by green fire!");
            }
            else
            {
                f.numbers.effects.push_back({dq_effect::green_fire, 1, 255, true});
            }
        }
        break;
    case dq_effect::spore_infection:
        kill(f, "{name} dies from disease.");
        break;
    case dq_effect::rot_recovery:
        if (!f.able() && !f.downed())
        {
            break;
        }
        f.numbers.effects.push_back({dq_effect::rot_recovery, 14400, ended.strength, true});
        f.hit_points = std::min(f.hit_points + 1, f.numbers.max_hit_points);
        break;
    case effect::acid_pool_timer:
        std::erase_if(clouds_,
                      [&f](const Cloud &cloud)
                      {
                          return cloud.caster == f.id && cloud.kind == CloudKind::acid;
                      });
        log({"The acid drains away.", {}});
        break;
    case effect::aurak_transformation_timer:
        aurak_final_form(f, false);
        break;
    case effect::aurak_explosion:
        draconian_explosion(f, 3, true);
        break;
    case effect::delayed_blast:
        delayed_blast_goes_off(f, ended.strength);
        break;
    case effect::enlarged:
    case effect::strengthened:
        recount_strength(f);
        break;
    case 113:
        if (game_of(identity_) == GoldBoxGame::dark_queen_of_krynn)
        {
            recount_strength(f);
        }
        break;
    case 342:
        // Fear ends: it stops fleeing and is the player's again (Curse, AffectFear).
        f.fleeing = false;
        // Treasures' fear also ends the flight its yetis' surprise forced, and
        // Pools of Darkness's the flight its Fear forced (their effhndlr.pas,
        // EffFear, as the effect is removed).
        if (treasures() || darkness())
        {
            f.turned = false;
        }
        break;
    case 337:
        // Berserk ends: it is back on its own side.
        f.side = f.own_side;
        break;
    case effect::regenerating_soon:
        f.numbers.effects.push_back({effect::regeneration, 0, 0, false});
        break;
    case effect::fighting_on:
        if (f.able())
        {
            f.condition = Condition::dead;
            f.hit_points = 0;
            f.initiative = 0;
            log({"{name} falls dead.", {{"name", f.name}}});
        }
        break;
    case effect::slow_poison_tick:
        // Slow Poison costs a point every 10 minutes, never the last
        // (effhndlr.pas, EffSlowPoisonHit).
        if (f.hit_points > 1)
        {
            --f.hit_points;
        }
        f.numbers.effects.push_back({effect::slow_poison_tick, 10, 255, true});
        break;
    case effect::slow_poison:
        // Still poisoned when it wears off, it dies (EffSlowPoisonEnd).
        f.remove(effect::slow_poison_tick);
        if (f.has(effect::poisoned) && f.condition != Condition::dead)
        {
            log({"{name} dies from poison.", {{"name", f.name}}});
            kill(f);
        }
        break;
    case effect::revival:
        revive(f);
        break;
    case effect::stinking_cloud:
    case effect::cloudkill:
    case effect::blade_barrier:
    {
        // The caster's cloud of this kind and number clears (effhndlr.pas,
        // EffClearStinkingCloud; Curse's EffClearCloudkill).
        const auto kind = ended.code == effect::cloudkill       ? CloudKind::deadly
                          : ended.code == effect::blade_barrier ? CloudKind::blades
                                                                : CloudKind::stinking;
        const auto cloud = std::find_if(clouds_.begin(), clouds_.end(),
                                        [&](const Cloud &c)
                                        {
                                            return c.caster == f.id && c.kind == kind &&
                                                   c.index == ended.strength / 16;
                                        });
        if (cloud != clouds_.end())
        {
            clouds_.erase(cloud);
            log({"The air clears a little.", {}});
        }
        break;
    }
    case effect::troll_return:
        if (f.condition == Condition::okay)
        {
            break;
        }
        if (cell_free(f))
        {
            stand_up(f, f.numbers.max_hit_points, "{name} rises again.");
        }
        else
        {
            f.numbers.effects.push_back({effect::troll_return, 1, 0, true});
        }
        break;
    default:
        break;
    }
}

// Native knights may take living NPC allies off Quick at combat start.
void Session::native_leadership()
{
    if (game_of(identity_) != GoldBoxGame::dark_queen_of_krynn)
    {
        return;
    }
    // Native combat_leadership_apply resets Quick at each new fight, then
    // lets each living PC knight try NPCs not already following a leader.
    for (auto &f : fighters_)
    {
        f.remove(dq_effect::led_by_knight);
    }
    static constexpr std::array odds{20, 40, 80};
    for (const auto &leader : fighters_)
    {
        if (leader.side != Side::party || !leader.able() || !leader.record ||
            leader.record->status != 0 || (leader.record->npc_flag & 0x80) != 0 ||
            leader.numbers.class_levels[static_cast<std::size_t>(ClassSlot::monk)] <= 0 ||
            leader.record->knight_order < 1 || leader.record->knight_order > 3)
        {
            continue;
        }
        const int threshold =
            odds[leader.record->knight_order - 1] +
            std::max(static_cast<int>(leader.record->current_abilities.charisma) - 13, 0) * 5;
        for (auto &follower : fighters_)
        {
            if (follower.side != Side::party || !follower.able() || !follower.record ||
                follower.record->status != 0 || (follower.record->npc_flag & 0x80) == 0 ||
                follower.record->current_abilities.intelligence <= 3 ||
                follower.has(dq_effect::led_by_knight) || threshold <= roll(100))
            {
                continue;
            }
            follower.numbers.effects.push_back({dq_effect::led_by_knight, 0, 255, false});
            log({"{leader} leads {name}.", {{"leader", leader.name}, {"name", follower.name}}});
        }
    }
}

// At a fight's start (combatst.pas, COMBAT_START): one that fights on while
// down may stand; a mummy's fear grips its enemies once; displacement is
// ready; camouflage and a ring of invisibility hide; a dragon frightens.
void Session::combat_start()
{
    bool feared = false;
    for (auto &f : fighters_)
    {
        // Native raw151 has a registered handler but no resident trigger;
        // keep its inherent metadata instead of starting the older game's counter.
        if (champions() && game_of(identity_) != GoldBoxGame::dark_queen_of_krynn)
        {
            if (auto *power = f.find_effect(effect::aurak_energy))
            {
                power->strength = 3;
            }
        }
        // What its lasting effects make of its Strength (a Strength spell cast
        // in camp, a disease's wasting).
        if (f.has(effect::enlarged) || f.has(effect::strengthened) ||
            f.has(effect::strength_wasted))
        {
            recount_strength(f);
        }
        // Curse's alias starts every fight invisible (add_affect_19).
        if (f.has(338) && !f.has(effect::invisible))
        {
            f.numbers.effects.push_back({effect::invisible, 255, 255, false});
        }
        // Gateway's fight start fires neither (its condition 8).
        if (f.has(effect::fights_while_down) && !gateway())
        {
            went_down(f);
        }
        if (f.has(effect::mummy_fear) && !feared)
        {
            feared = true;
            // The first mummy's fear is the only one: every mummy loses its own.
            for (auto &other : fighters_)
            {
                other.remove(effect::mummy_fear);
            }
            const auto enemies = std::count_if(fighters_.begin(), fighters_.end(),
                                               [&f](const Fighter &other)
                                               {
                                                   return other.side != f.side;
                                               });
            for (auto &other : fighters_)
            {
                if (other.side == f.side || !other.able())
                {
                    continue;
                }
                // Paralysis by fear: saved against with +1 for a side of 6
                // or more and +2 for a human; immunity to paralysis spares
                // (effhndlr.pas, EffMummyFear).
                const int modifier = (enemies >= 6 ? 1 : 0) + (other.numbers.race == 7 ? 2 : 0);
                if (other.has(109) || other.has(111) || other.has(125) ||
                    saving_throw(other, 0, modifier))
                {
                    log({"{name} is unaffected.", {{"name", other.name}}});
                    continue;
                }
                other.numbers.effects.push_back({effect::paralysis, roll(4), 12, false});
                log({"{name} is paralyzed with fear.", {{"name", other.name}}});
            }
        }
        if (auto *displacement = f.find_effect(effect::displacement))
        {
            displacement->strength &= ~effect::displaced;
        }
        if (f.has(effect::camouflage) && roll(100) <= 95)
        {
            f.numbers.effects.push_back({effect::invisible, 1, 12, false});
        }
        // A ring of invisibility hides its wearer for the round (effhndlr.pas,
        // EffRenewInvisibility): Secret's Dreadlord starts every fight unseen.
        // Gateway's ring first hides at the round's end.
        if (f.has(effect::renew_invisibility) && !gateway())
        {
            f.numbers.effects.push_back({effect::invisible, 1, 12, false});
        }
        if (champions() && f.has(effect::champions_dragon_fear) && f.able())
        {
            for (auto &other : fighters_)
            {
                if (other.side == f.side || !other.able() || other.has(effect::fearless) ||
                    other.has(effect::fear) || other.has(effect::champions_afraid))
                {
                    continue;
                }
                if (other.numbers.level <= 3)
                {
                    if (add_with_message(other, {effect::fear, 0, 255, true}, false, false, 0,
                                         f.numbers.level, game_spell(184)))
                    {
                        other.turned = true;
                    }
                }
                else if (!saving_throw(other, 4, 0, &f))
                {
                    other.numbers.effects.push_back({effect::champions_afraid, 0, 255, false});
                    log({"{name} is afraid.", {{"name", other.name}}});
                }
            }
        }
        // A Secret dragon's fear grips the weaker of its enemies (EffDragonFear).
        if (f.has(effect::dragon_fear) && f.able())
        {
            dragon_fear(f);
        }
        // A Pools of Darkness dragon's, tried by both of the original's
        // conditions of a fight's start (combatst.pas, CombatStart, and
        // combat.pas, CombatMain), so one that saves once faces it again.
        if (f.has(effect::darkness_dragon_fear) && f.able())
        {
            darkness_dragon_fear(f);
            darkness_dragon_fear(f);
        }
        if (frightening_surprise_ && f.has(effect::frightens_on_surprise))
        {
            frighten_on_surprise(f);
        }
    }
}

// A Treasures yeti's fear when it surprises (effhndlr.pas, EffFrightenAura,
// as a fight starts with the script's cell 53h set): each able enemy that
// fails a save against petrification, unless afraid already, is frightened
// and flees.
void Session::frighten_on_surprise(const Fighter &yeti)
{
    for (auto &f : fighters_)
    {
        if (f.side == yeti.side || !f.able() || saving_throw(f, 1, 0, &yeti) || f.has(effect::fear))
        {
            continue;
        }
        f.numbers.effects.push_back({effect::fear, 2, 255, true});
        f.turned = true;
        log({"{name} is frightened.", {{"name", f.name}}});
    }
}

// At a round's end, after effects age (combat.pas, EndOfRound): regeneration
// heals 3; camouflage hides again, 95 times in 100, and a ring of invisibility
// every time; displacement is ready for the next round.
void Session::round_end_effects()
{
    for (auto &f : fighters_)
    {
        if (f.has(effect::regeneration))
        {
            f.hit_points = std::min(f.hit_points + 3, f.numbers.max_hit_points);
        }
        // Gateway's Vaalgamon regenerates 1 (EffRegenerateOne).
        const bool native_regeneration =
            game_of(identity_) != GoldBoxGame::dark_queen_of_krynn ||
            std::ranges::any_of(fighters_,
                                [](const Fighter &other)
                                {
                                    return other.side == rules::Side::opposition && other.able();
                                });
        if (f.has(effect::regenerates_one) && native_regeneration &&
            f.hit_points < f.numbers.max_hit_points)
        {
            f.hit_points = std::min(f.hit_points + 1, f.numbers.max_hit_points);
        }
        if (f.has(effect::camouflage) && roll(100) <= 95)
        {
            f.numbers.effects.push_back({effect::invisible, 1, 12, false});
        }
        if (f.has(effect::renew_invisibility))
        {
            f.numbers.effects.push_back({effect::invisible, 1, 12, false});
        }
        if (auto *displacement = f.find_effect(effect::displacement))
        {
            displacement->strength &= ~effect::displaced;
        }
    }
}

// A creature just gone down (fxengine.pas, HIT_POINTS_ZERO): one that fights
// on while down stands again, dying less than 6 below 0 or unconscious, with
// 6 less its wounds, for 1d4+1 rounds; a troll downed by anything but fire or
// acid (any weapon) comes back in 3d6 rounds.
void Session::went_down(Fighter &f)
{
    if (f.has(dq_effect::spore_burst))
    {
        f.remove(dq_effect::spore_burst);
        if (!f.has(2194))
        {
            log({"{name} explodes.", {{"name", f.name}}});
            for (const auto id : within(f.cell, 2))
            {
                auto &other = *find(id);
                if (other.id == f.id || !other.able() || other.has(2183))
                {
                    continue;
                }
                const bool saved = saving_throw(other, 2, 0, &f);
                int damage = 0;
                for (int die = 0; die < 6; ++die)
                {
                    damage += roll(6);
                }
                nonweapon_damage(other, damage, 2, saved, 0, 0, 0, false);
            }
        }
    }
    if (f.has(dq_effect::ice_explosion))
    {
        f.remove(dq_effect::ice_explosion);
        log({"{name} explodes.", {{"name", f.name}}});
        const int damage = roll(10) + roll(10) + roll(10);
        for (const auto id : within(f.cell, 2))
        {
            auto &other = *find(id);
            if (other.id != f.id && other.able())
            {
                nonweapon_damage(other, damage, 0, false, 8, 12, 187, false);
            }
        }
    }
    if (f.has(dq_effect::meteor_explosion))
    {
        f.remove(dq_effect::meteor_explosion);
        log({"{name} explodes.", {{"name", f.name}}});
        cast_meteor_swarm(f, 12, f.cell);
    }

    if (champions())
    {
        if (f.has(effect::baaz_disarm))
        {
            auto *attacker = find(striker_);
            if (attacker && attacker->record && distance(*attacker, f) <= 1 &&
                roll(20) >= attacker->record->current_abilities.dexterity - 3)
            {
                const auto held = held_now(*attacker);
                const auto weapon =
                    std::ranges::find_if(held,
                                         [](const Held &item)
                                         {
                                             return !item.gone && !item.conjured &&
                                                    item.carried.item.readied() &&
                                                    item.carried.item.cursed_raw == 0 &&
                                                    item.carried.type.worn_location == 0;
                                         });
                if (weapon != held.end())
                {
                    const auto at = static_cast<std::size_t>(weapon - held.begin());
                    toggle_readied(*attacker, at);
                    attacker->numbers.effects.push_back(
                        {effect::baaz_banked_weapon, 0, static_cast<int>(at + 1), false});
                    log({"{name} loses a weapon in the Baaz's stone body.",
                         {{"name", attacker->name}}});
                }
            }
        }
        if (f.has(effect::kapak_acid))
        {
            f.remove(effect::kapak_acid);
            Cloud pool;
            pool.caster = f.id;
            pool.corner = f.cell;
            pool.covers[0] = true;
            pool.kind = CloudKind::acid;
            clouds_.push_back(pool);
            put_out_of_play(f, Condition::gone);
            f.numbers.effects.push_back({effect::acid_pool_timer, roll(6), 0, true});
            log({"{name} dissolves into acid.", {{"name", f.name}}});
            return;
        }
        if (f.has(effect::aurak_madness))
        {
            f.remove(effect::aurak_madness);
            stand_up(f, 20, "{name} rises in a frenzy.");
            f.numbers.spells.clear();
            f.numbers.effects.push_back({effect::aurak_transformation, 0, 255, false});
            f.numbers.effects.push_back({effect::aurak_immolation, 0, 255, false});
            f.numbers.effects.push_back({effect::aurak_combat_bonus, 0, 255, false});
            f.numbers.effects.push_back({effect::aurak_transformation_timer, 6, 255, true});
            return;
        }
        if (f.has(effect::aurak_transformation))
        {
            aurak_final_form(f, true);
            return;
        }
        if (f.has(effect::bozak_explosion))
        {
            draconian_explosion(f, 1, false);
            return;
        }
    }
    if (f.has(effect::fights_while_down) &&
        ((f.condition == Condition::dying && f.overkill < 6) ||
         f.condition == Condition::unconscious) &&
        cell_free(f))
    {
        const int hit_points = f.condition == Condition::dying ? 6 - f.overkill : 6;
        f.remove(effect::fights_while_down);
        stand_up(f, hit_points, "{name} gets back up.");
        f.numbers.effects.push_back({effect::fighting_on, roll(4) + 1, 0, true});
        return;
    }
    if (f.has(effect::troll_burn) && !f.able() && !f.has(effect::troll_return))
    {
        f.numbers.effects.push_back({effect::troll_return, roll(6) + roll(6) + roll(6), 0, true});
    }
}

void Session::aurak_final_form(Fighter &f, bool revived)
{
    if (game_of(identity_) == GoldBoxGame::dark_queen_of_krynn)
    {
        f.remove(effect::aurak_energy);
    }
    for (const auto code :
         {effect::aurak_madness, effect::aurak_transformation, effect::aurak_immolation,
          effect::aurak_combat_bonus, effect::aurak_transformation_timer})
    {
        f.remove(code);
    }
    if (revived)
    {
        stand_up(f, 20, "{name} becomes a blazing shape.");
    }
    else
    {
        log({"{name} becomes a blazing shape.", {{"name", f.name}}});
    }
    f.numbers.effects.push_back({effect::aurak_invulnerable, 0, 255, false});
    f.numbers.effects.push_back({effect::aurak_explosion, 3, 255, true});
    if (revived || game_of(identity_) != GoldBoxGame::dark_queen_of_krynn)
    {
        f.numbers.effects.push_back({effect::aurak_zap, 0, 255, false});
    }
}

void Session::draconian_explosion(Fighter &f, int dice, bool stuns)
{
    const bool native_enchanted = game_of(identity_) == GoldBoxGame::dark_queen_of_krynn &&
                                  f.has(dq_effect::enchanted_draconian) && stuns;
    const int die_faces = native_enchanted ? 8 : 6;
    if (native_enchanted)
    {
        dice = 16;
    }
    for (const auto code : {effect::bozak_explosion, effect::aurak_invulnerable,
                            effect::aurak_explosion, effect::aurak_zap})
    {
        f.remove(code);
    }
    log({"{name} explodes.", {{"name", f.name}}});
    put_out_of_play(f, Condition::gone);
    for (auto &other : fighters_)
    {
        if (other.id == f.id || !other.able() || distance(f, other) > 1)
        {
            continue;
        }
        int damage = 0;
        for (int count = 0; count < dice; ++count)
        {
            damage += roll(die_faces);
        }
        nonweapon_damage(other, damage, 0, false, 0, 0, 0, false);
        if (stuns && other.able() && !saving_throw(other, 4, 0) &&
            !(game_of(identity_) == GoldBoxGame::dark_queen_of_krynn &&
              (other.has(effect::aurak_madness) || other.has(effect::aurak_transformation) ||
               other.has(effect::aurak_explosion))))
        {
            other.numbers.effects.push_back({effect::stunned, 0, 255, false});
            log({"{name} is stunned.", {{"name", other.name}}});
        }
    }
}

void Session::kender_yell(Fighter &f)
{
    // Once a fight, even when every opponent saves; the mark is checkpointed
    // with ordinary combat effects and does not carry into another fight.
    f.numbers.effects.push_back({effect::kender_yelled, 0, 0, false});
    log({"{name} yells a taunt.", {{"name", f.name}}});
    for (auto &other : fighters_)
    {
        if (other.side != f.side && other.able() && !other.helpless() &&
            other.numbers.intelligence > 3 && !saving_throw(other, 4, 0, &f))
        {
            other.numbers.effects.push_back(
                {effect::kender_taunted, 0, static_cast<int>(f.id), false});
            log({"{name} wants to kill {yeller}.", {{"name", other.name}, {"yeller", f.name}}});
        }
    }
    end_actions(f);
}

void Session::champions_turn_start(Fighter &f)
{
    if (!champions() || !f.able())
    {
        return;
    }
    if (f.has(dkk_effect::regenerate_three))
    {
        f.hit_points = std::min(f.hit_points + 3, f.numbers.max_hit_points);
    }
    if (f.has(dkk_effect::immolation))
    {
        for (auto &other : fighters_)
        {
            if (other.id != f.id && other.able() && distance(f, other) <= 1)
            {
                nonweapon_damage(other, roll(6), 0, false, 1, 0, 0, false);
            }
        }
    }
    if (f.has(effect::death_knight_gate) && f.hit_points < f.numbers.max_hit_points / 2)
    {
        f.remove(effect::death_knight_gate);
        for (auto &reserve : fighters_)
        {
            if (reserve.side != f.side || reserve.arrival_round != gated_reserve_round)
            {
                continue;
            }
            const auto old_cell = reserve.cell;
            bool placed = false;
            for (const auto &enemy : fighters_)
            {
                if (enemy.side == f.side || !enemy.able())
                {
                    continue;
                }
                for (std::size_t direction = 0; direction < step_x.size(); ++direction)
                {
                    reserve.cell = {enemy.cell.x + step_x[direction],
                                    enemy.cell.y + step_y[direction]};
                    const auto squares = squares_of(reserve);
                    if (cell_free(reserve) &&
                        std::ranges::all_of(squares,
                                            [&](Cell cell)
                                            {
                                                return board_.contains(cell) &&
                                                       board_.at(cell) == rules::Terrain::open;
                                            }))
                    {
                        reserve.condition = Condition::okay;
                        reserve.arrival_round = 0;
                        reserve.hit_points = reserve.numbers.max_hit_points;
                        reserve.initiative = 0;
                        log({"{name} gates in.", {{"name", reserve.name}}});
                        placed = true;
                        break;
                    }
                }
                if (placed)
                {
                    break;
                }
            }
            if (!placed)
            {
                reserve.cell = old_cell;
            }
        }
    }
    if (f.has(effect::death_knight_fireball))
    {
        const auto &fireball = *game_spell(47);
        const int level = caster_level(f, fireball, game_of(identity_));
        const int range = spell_range(fireball, level);
        for (unsigned attempt = 0; attempt < 20; ++attempt)
        {
            const auto *target = secret_target(f, range, false);
            if (!target)
            {
                break;
            }
            const auto aim = target->cell;
            if (std::ranges::any_of(fighters_,
                                    [&](const Fighter &other)
                                    {
                                        return other.side == f.side && other.able() &&
                                               distance(other, *target) <= fireball.ai_radius;
                                    }))
            {
                continue;
            }
            log({"{name} throws a fireball.", {{"name", f.name}}});
            f.remove(effect::death_knight_fireball);
            const auto targets = fireball_reach(within(aim, fireball.area & 7), aim);
            int damage = 0;
            for (unsigned die = 0; die < 15; ++die)
            {
                damage += roll(6);
            }
            cast_generic(f, 47, level, targets, damage, 9);
            break;
        }
    }
    if (f.has(effect::death_knight_dispel) && roll(20) == 1)
    {
        const auto &spell = *game_spell(41);
        const int level = caster_level(f, spell, game_of(identity_));
        if (const auto *target = secret_target(f, spell_range(spell, level), false))
        {
            log({"{name} invokes Dispel Magic.", {{"name", f.name}}});
            for (const auto id : within(target->cell, spell.area & 7))
            {
                dispel(*find(id), level);
            }
        }
    }
    if (f.has(effect::ghast_stench) || f.has(effect::death_knight_fear))
    {
        for (auto &other : fighters_)
        {
            if (other.side == f.side || !other.able() || distance(f, other) > 1)
            {
                continue;
            }
            if (f.has(effect::ghast_stench) && !other.has(effect::champions_sickened) &&
                !saving_throw(other, 4, 0, &f))
            {
                other.numbers.effects.push_back({effect::champions_sickened, 0, 255, false});
                log({"{name} is affected by the evil stench.", {{"name", other.name}}});
            }
            if (f.has(effect::death_knight_fear) && !other.has(effect::fearless) &&
                !other.has(effect::fear) && !saving_throw(other, 4, 0, &f))
            {
                if (add_with_message(other, {effect::fear, 0, 255, true}, false, false, 0,
                                     f.numbers.level, game_spell(184)))
                {
                    other.turned = true;
                }
            }
        }
    }
    if (f.has(effect::aurak_immolation))
    {
        // Native's frenzy burns only the other side, with fire (effects.c,
        // fx_immolate).
        const bool native = game_of(identity_) == GoldBoxGame::dark_queen_of_krynn;
        for (auto &other : fighters_)
        {
            if (other.id != f.id && other.able() && distance(f, other) <= 1 &&
                !(native && other.side == f.side))
            {
                nonweapon_damage(other, roll(6), 0, false, native ? 1 : 0, 0, 0, false);
            }
        }
    }
    if (f.has(effect::aurak_zap))
    {
        for (auto &other : fighters_)
        {
            if (other.side != f.side && other.able() && distance(f, other) <= 1)
            {
                log({"{name} is struck by an energy bolt.", {{"name", other.name}}});
                const bool native_enchanted =
                    game_of(identity_) == GoldBoxGame::dark_queen_of_krynn &&
                    f.has(dq_effect::enchanted_draconian);
                int damage = 0;
                for (int die = 0; die < (native_enchanted ? 8 : 2); ++die)
                {
                    damage += roll(native_enchanted ? 8 : 6);
                }
                nonweapon_damage(other, damage, 0, false, 0, 0, 0, false);
                end_actions(f);
                break;
            }
        }
    }
    if (game_of(identity_) != GoldBoxGame::dark_queen_of_krynn && f.able() && f.initiative > 0 &&
        f.has(effect::aurak_energy) && roll(100) <= 75)
    {
        auto *power = f.find_effect(effect::aurak_energy);
        if (power->strength > 0 && roll(100) <= 25)
        {
            Fighter *target = nullptr;
            for (auto &other : fighters_)
            {
                if (other.side != f.side && other.able() && distance(f, other) <= 1)
                {
                    target = &other;
                }
            }
            if (target)
            {
                --power->strength;
                log({"{name} breathes a blinding cloud.", {{"name", f.name}}});
                const bool saved = saving_throw(*target, 3, 0, &f);
                nonweapon_damage(*target, saved ? 10 : 20, 0, false, 0, 0, 0, false);
                if (!saved)
                {
                    (void)add_with_message(*target, {effect::blind, 0, 0, false}, false, false, 0,
                                           0, nullptr);
                    end_actions(f);
                }
                return;
            }
        }
        if (auto *target = secret_target(f, 6, true))
        {
            log({"{name} fires an energy bolt.", {{"name", f.name}}});
            const bool targeted = game_of(identity_) == GoldBoxGame::dark_queen_of_krynn;
            nonweapon_damage(*target, roll(8) + 2, 0, false, 0, 0, 0, targeted);
            if (target->able())
            {
                nonweapon_damage(*target, roll(8) + 2, 0, false, 0, 0, 0, targeted);
            }
            end_actions(f);
        }
    }
}

bool Session::cell_free(const Fighter &f) const
{
    const auto mine = squares_of(f);
    return std::none_of(fighters_.begin(), fighters_.end(),
                        [&](const Fighter &other)
                        {
                            if (other.id == f.id || other.condition == Condition::dead ||
                                other.condition == Condition::fled ||
                                other.condition == Condition::surrendered ||
                                other.condition == Condition::gone)
                            {
                                return false;
                            }
                            const auto theirs = squares_of(other);
                            return std::any_of(mine.begin(), mine.end(),
                                               [&theirs](Cell c)
                                               {
                                                   return std::find(theirs.begin(), theirs.end(),
                                                                    c) != theirs.end();
                                               });
                        });
}

// Back on its feet, without turns this round (its initiative is spent).
void Session::stand_up(Fighter &f, int hit_points, std::string_view message)
{
    f.condition = Condition::okay;
    f.hit_points = hit_points;
    f.overkill = 0;
    f.initiative = 0;
    log({std::string(message), {{"name", f.name}}});
}

// Whether the attacker may single out the target (cbtacts.pas, CanTarget): not
// an invisible one unless it sees the invisible, not a blinking one before it
// has acted this round, and nothing that holds up a mirror or holy symbol to a
// gazing monster.
bool Session::can_target(const Fighter &attacker, const Fighter &target) const
{
    if (target.has(effect::invisible) && !attacker.has(effect::see_invisible))
    {
        return false;
    }
    if (target.has(effect::blink) && target.initiative > 0)
    {
        return false;
    }
    // Curse's blink hides it once it has acted (AffectBlink in its reconstruction).
    if (target.has(237) && target.initiative <= 0)
    {
        return false;
    }
    // Invisibility to animals: an animal that cannot see the invisible
    // cannot single it out (Curse, AffectInvisToAnimals).
    if (target.has(269) && attacker.numbers.creature_type == 19 &&
        !attacker.has(effect::see_invisible))
    {
        return false;
    }
    // Dust of Disappearance hides it even from one that sees the invisible
    // (effhndlr.pas, EffDustDisappear).
    if (target.has(effect::dust))
    {
        return false;
    }
    return !(attacker.has(effect::gaze_block) && target.numbers.wards_gaze);
}

// An attack roll a defender turns aside, even a natural 20 (fxengine.pas,
// ATTACK_ROLL_DEF): a blinking one before it has acted this round, and a
// displaced one the first attack each round. The original readied
// displacement only at the fight's start, so it turned aside one attack a
// fight; once a round here, as its own notes say (an obvious bug, fixed).
bool Session::turned_aside(Fighter &defender)
{
    if ((defender.has(effect::blink) && defender.initiative > 0) ||
        (defender.has(237) && defender.initiative <= 0))
    {
        return true;
    }
    auto *displacement = defender.find_effect(effect::displacement);
    if (displacement && (displacement->strength & effect::displaced) == 0)
    {
        displacement->strength |= effect::displaced;
        return true;
    }
    return false;
}

// Pools of Darkness's Kalistes strikes the first six attacks of each attack
// she makes with six weapons in turn (effhndlr.pas, EffMultiAttackNewWeapons): each
// blow does its weapon's dice, 4 more and her Strength's bonus, in place of
// what her record's dice did, and names the weapon. Her further attacks, and
// any of her second form, keep their damage, as the original's count of first
// attacks picks no weapon for them.
int Session::six_weapons_blow(const Fighter &attacker, int primary_attacks, int damage)
{
    struct Weapon
    {
        int dice, sides;
        std::string_view name;
    };

    static constexpr std::array<Weapon, 6> weapons{{{1, 10, "Two-Handed Sword +4"},
                                                    {2, 4, "Bastard Sword +4"},
                                                    {1, 8, "Long Sword +4"},
                                                    {1, 8, "Scimitar +4"},
                                                    {1, 4, "Dagger +4"},
                                                    {1, 6, "Spear +4"}}};
    if (primary_attacks < 1 || primary_attacks > static_cast<int>(weapons.size()))
    {
        return damage;
    }
    const auto &weapon = weapons[static_cast<std::size_t>(primary_attacks - 1)];
    int blow =
        4 + strength_damage_bonus(attacker.numbers.strength, attacker.numbers.exceptional_strength);
    for (int die = 0; die < weapon.dice; ++die)
    {
        blow += roll(weapon.sides);
    }
    log({"{attacker} strikes with a {weapon}.",
         {{"attacker", attacker.name}, {"weapon", std::string(weapon.name)}}});
    return std::max(blow, 0);
}

// What the defender's effects make of a weapon's damage, in the original's
// order (effhndlr.pas, WEAPON_DAMAGE_DEFENDER): evading missiles and boulders
// from afar, a troll's regeneration beginning, halving by weapon class or
// magic, weapons that cannot harm it, and oil or holy water.
int Session::weapon_damage_defenses(Fighter &attacker, Fighter &defender, int damage, int phase,
                                    int primary_attacks, int &primary_hits)
{
    if (attacker.has(dq_effect::green_fire_touch) && !saving_throw(defender, 4, 0, &attacker) &&
        !defender.has(dq_effect::green_fire))
    {
        defender.numbers.effects.push_back({dq_effect::green_fire, 1, 255, true});
        log({"{name} is turning to green fire!", {{"name", defender.name}}});
    }
    if (defender.has(dq_effect::anemone_acid) && distance(attacker, defender) <= 1 && damage <= 15)
    {
        log({"{name} is squirted by anemone's acid!", {{"name", attacker.name}}});
        nonweapon_damage(attacker, roll(4) + roll(4) + roll(4), 0, false, 256, 0, 0, false);
    }
    const auto &striker = attacker.numbers.striker;
    const int plus = striker ? striker->plus : 0;
    const bool silver = striker && striker->silver;
    const int weapon_class = striker ? striker->weapon_class : 0;
    const bool piercing = (weapon_class & 0x01) != 0, bludgeoning = (weapon_class & 0x80) != 0;
    const auto evades = [&](int chance)
    {
        if (distance(attacker, defender) <= 1 || roll(100) > chance)
        {
            return false;
        }
        log({"{name} avoids it.", {{"name", defender.name}}});
        // The original took the hit from the primary attacks' count whatever
        // attack it was, wrapping below 0 (an obvious bug, fixed).
        if (phase == 0 && primary_hits > 0)
        {
            --primary_hits;
        }
        return true;
    };
    if (attacker.has(dq_effect::underwater_weapons) && !attacker.has(2150) && striker)
    {
        damage = attacker.numbers.missile ? 0
                 : weapon_class != 0 ? std::min(damage, attacker.numbers.attacks[0].modifier + 1)
                                     : damage;
    }
    // A black pudding's acid spends the blow on the armor it eats (the kit's
    // 186), as the attacker's effects come before the defender's.
    if (attacker.has(effect::eats_armor) && eat_armor(attacker, defender))
    {
        damage = 0;
    }
    // Curse's rakshasa (its effect 130) struck by a banishing quarrel is gone
    // for good (effhndlr.pas, EffBanishByWeapon).
    constexpr int banished_by_quarrel = 330;
    if (defender.has(banished_by_quarrel) && striker && striker->banishing)
    {
        log({"{name} is banished.", {{"name", defender.name}}});
        put_out_of_play(defender, Condition::gone);
        return 0;
    }
    // Pools of Darkness's rakshasa struck by a blessed bolt is slain
    // (effhndlr.pas, EffBlessedBoltKills), as the kit's by its quarrel of
    // slaying (effects.c, fx_slain_by_type).
    if (defender.has(effect::slain_by_blessed_bolt) && striker && striker->blessed_bolt)
    {
        kill(defender, "{name} is slain.");
        return 0;
    }
    // Ray of Enfeeblement takes a quarter of the attacker's damage (effhndlr.pas, EffEnfeeble).
    if (attacker.has(effect::enfeebled))
    {
        damage -= damage / 4;
    }
    if (attacker.has(dq_effect::disruption))
    {
        if (defender.numbers.traits & dq_effect::disruption_double_trait)
        {
            damage *= 2;
        }
        else if ((defender.has(dq_effect::disintegrating_ray) && roll(2) == 1) ||
                 defender.numbers.undead_level > 0)
        {
            log({"{name} is disrupted.", {{"name", defender.name}}});
            put_out_of_play(defender, Condition::gone);
            return 0;
        }
    }
    if (attacker.has(dq_effect::dragon_slayer) &&
        (defender.numbers.traits & champions_dragon_trait))
    {
        damage = defender.has(504)
                     ? damage + 6
                     : roll(12) + roll(12) + roll(12) + 6 +
                           strength_damage_bonus(attacker.numbers.strength,
                                                 attacker.numbers.exceptional_strength);
    }
    if (champions())
    {
        const bool dragon = (defender.numbers.traits & champions_dragon_trait) != 0;
        if (dragon && attacker.has(effect::dragon_slayer))
        {
            // The native handler triples one d12, then adds 4 and Strength.
            damage = 3 * roll(12) + 4 +
                     strength_damage_bonus(attacker.numbers.strength,
                                           attacker.numbers.exceptional_strength);
        }
        if (dragon && attacker.has(effect::dragonlance))
        {
            damage = attacker.hit_points;
        }
        if (attacker.has(effect::reptile_bane) &&
            (defender.numbers.traits & champions_reptile_trait))
        {
            damage += 3;
        }
        if (attacker.has(effect::mace_disruption) && (defender.numbers.traits & trait::unliving))
        {
            if (defender.numbers.undead_level > 0)
            {
                log({"{name} is disrupted.", {{"name", defender.name}}});
                put_out_of_play(defender, Condition::gone);
                return 0;
            }
            damage *= 2;
        }
    }
    // Pools of Darkness's dragon fear takes 2 from a blow (EffDragonFeared).
    if (attacker.has(effect::dragon_feared))
    {
        damage = std::max(damage - 2, 0);
    }
    if (attacker.has(dkk_effect::smoked))
    {
        damage = std::max(0, damage - 2);
    }
    if (attacker.has(effect::six_weapons))
    {
        damage = six_weapons_blow(attacker, primary_attacks, damage);
    }
    // Mirror images take blows too, as the manual says; the original let them
    // take only a spell's (an obvious bug, fixed).
    if (mirror_absorbs(defender))
    {
        damage = 0;
    }
    if (defender.has(104) && attacker.numbers.weapon_type >= 0 && evades(60))
    {
        damage = 0;
    }
    // Protection from Normal Missiles: no harm from a mundane weapon from afar
    // (effhndlr.pas, EffProtNormalMissiles).
    if (defender.has(effect::missile_protection) && striker && plus == 0 &&
        attacker.numbers.weapon_type >= 0 && evades(100))
    {
        damage = 0;
    }
    const int boulder = attacker.numbers.weapon_type;
    if (defender.has(120) && (boulder == 87 || boulder == 88) && evades(50))
    {
        damage = 0;
    }
    if (defender.has(effect::troll_heal) && !defender.has(effect::regeneration) &&
        !defender.has(effect::regenerating_soon))
    {
        defender.numbers.effects.push_back({effect::regenerating_soon, 3, 0, true});
    }
    if (defender.has(115) && striker && (weapon_class == 0 || piercing))
    {
        damage /= 2;
    }
    if (defender.has(116) && plus > 0)
    {
        damage /= 2;
    }
    if (defender.has(119) && plus == 0 && (attacker.numbers.race > 0 || attacker.numbers.level < 4))
    {
        damage = 0;
    }
    // Secret's: an iron golem takes harm only from a weapon of +3 or better,
    // and a giant slug none from a bludgeoning one (effhndlr.pas,
    // EffNeedMagicWeapon and EffHalfBash, in its reconstruction).
    if (defender.has(dkk_effect::needs_plus_three) && (!striker || plus < 3) &&
        (attacker.numbers.race > 0 || attacker.numbers.level < 4))
    {
        damage = 0;
    }
    if (defender.has(effect::needs_plus_three) && (!striker || plus < 3))
    {
        damage = 0;
    }
    if (defender.has(effect::bashing_proof) && striker && bludgeoning)
    {
        damage = 0;
    }
    // Pools of Darkness's crushing weapon does half (effhndlr.pas,
    // EffHalfDmgCrush); its weapons below +3 and +4 and its need of a +2
    // weapon are the kit's, below (EffLuckStone, EffHalfDmgWeaponLt3,
    // EffNeedPlus2).
    if (defender.has(effect::halves_crushing) && striker && bludgeoning)
    {
        damage /= 2;
    }
    // The kit's (effects.c): a weapon below +3 or +4 does half (80, 81); one
    // below +2 nothing, nor a creature's bare blows unless a monster's of 7
    // Hit Dice or more (125); and a black pudding takes nothing from any
    // blow (193).
    if (defender.has(effect::halved_below_plus_three) && striker && plus < 3)
    {
        damage /= 2;
    }
    if (defender.has(effect::halved_below_plus_four) && striker && plus < 4)
    {
        damage /= 2;
    }
    // Darkness's shield returns twice the damage of an adjacent weapon
    // as magic, before the blow lands (effhndlr.pas, EffZapAttacker).
    // A returned magical hit is not another weapon attack, so shields
    // cannot recursively return one another's retaliation.
    if ((darkness() || game_of(identity_) == GoldBoxGame::dark_queen_of_krynn) &&
        defender.has(effect::retaliating_shield) && damage > 0 && distance(attacker, defender) < 2)
    {
        const int returned = damage > std::numeric_limits<int>::max() / 2
                                 ? std::numeric_limits<int>::max()
                                 : damage * 2;
        log({"{name}'s Fire Shield strikes {attacker}.",
             {{"name", defender.name}, {"attacker", attacker.name}}});
        nonweapon_damage(attacker, returned, 0, false, 8, 0, 0, false);
    }
    // The kit's need of a weapon of +n (fx_need_plus): one below it does
    // nothing, nor bare blows but a monster's of (n - 1) * 2 + 5 Hit Dice or
    // more: +1 (103), +2 (125), +3 (60).
    const auto turns_aside = [&](int needed)
    {
        const bool strong_monster =
            attacker.numbers.race == 0 && attacker.numbers.level >= (needed - 1) * 2 + 5;
        return striker ? plus < needed : !strong_monster;
    };
    if ((defender.has(effect::needs_plus_one) && turns_aside(1)) ||
        (defender.has(effect::needs_plus_two) && turns_aside(2)) ||
        (defender.has(effect::needs_plus_three_kit) && turns_aside(3)))
    {
        damage = 0;
    }
    if (defender.has(effect::blow_proof))
    {
        damage = 0;
    }
    if (defender.has(dq_effect::mundane_blows_fail) && distance(attacker, defender) <= 1 &&
        (!striker || plus == 0))
    {
        damage = 0;
    }
    // A Treasures stone golem, while stone, takes nothing from a weapon
    // below +2 (EffSlowAura, LimitDamageByResistance).
    if (const auto *golem = defender.find_effect(effect::stone_golem);
        golem && golem->strength == effect::golem_stone && turns_aside(2))
    {
        damage = 0;
    }
    if (defender.has(123))
    {
        damage = plus > 0 ? damage : silver ? damage / 2 : 0;
    }
    if (defender.has(96) && plus == 0 && !silver)
    {
        damage = 0;
    }
    if (defender.has(94) && striker && (piercing || bludgeoning))
    {
        damage /= 2;
    }
    // Curse's defenses (ovr013): normal weapons do nothing and +1 or +2
    // half (260); every blow does half (281); a piercing one 1 (285).
    // Gateway's holy water harms such a mummy in full (its
    // EffRequirePlusThreeWeapon).
    const bool holy_water_blow = gateway() && attacker.numbers.weapon_type == 85;
    if (defender.has(260) && !holy_water_blow)
    {
        damage = plus == 0 ? 0 : plus < 3 ? damage / 2 : damage;
    }
    if (defender.has(281))
    {
        damage /= 2;
    }
    // Gateway's and Treasures' weapons of Icewind Dale and of stonecutting do
    // double to the creatures they bane, and their glacier armor turns half a
    // fire creature's blow (EffVulnerableToIcewindDale,
    // EffVulnerableToStonecutting, EffGlacierArmor): name words 144, 250 and
    // 137 in Gateway, 127, 129 and 128 in Treasures.
    const bool frontier = gateway() || treasures();
    if (frontier && defender.has(effect::icewind_dale_harms) &&
        weapon_named(attacker, gateway() ? 144 : 127))
    {
        damage *= 2;
    }
    if (frontier && defender.has(effect::stonecutting_harms) &&
        weapon_named(attacker, gateway() ? 250 : 129))
    {
        damage *= 2;
    }
    if (frontier && attacker.has(effect::glacier_halves) &&
        armor_named(defender, gateway() ? 137 : 128))
    {
        damage /= 2;
    }
    // A Treasures stone guardian takes a quarter from an edge or a point,
    // and nothing from a weapon whose type may be thrown, even one swung
    // (EffResistItemTypes). The original reads that type of no item for a
    // bare blow (Bugs).
    if (defender.has(effect::quarter_from_edges) && striker && weapon_class <= 1)
    {
        damage /= 4;
    }
    if (defender.has(effect::quarter_from_edges) && striker && striker->thrown)
    {
        damage = 0;
    }
    if (defender.has(285) && weapon_class == 1)
    {
        damage = std::min(damage, 1);
    }
    // A ranger adds its level against giants (Curse, AffectRangerVsGiant); in
    // Secret and Pools of Darkness against the creatures its flags name, and
    // in the kit those its monster types do, from next to them
    // (EffRangerDamageBonus; the kit's effects.c, 105).
    const bool ranger_foe =
        secret_engine() || kit() || game_of(identity_) == GoldBoxGame::dark_queen_of_krynn
            ? (defender.numbers.traits & trait::ranger_foe) && adjacent(attacker, defender)
            : (defender.numbers.kin & kin::giant) != 0;
    if (attacker.has(334) && ranger_foe && damage > 0)
    {
        damage += attacker.numbers.class_levels[static_cast<std::size_t>(ClassSlot::ranger)];
    }
    // A salamander's heat adds 1d6 unless the target resists fire (303).
    if (attacker.has(303) && damage > 0 && !defender.has(effect::resist_fire) &&
        !defender.has(254) && !defender.has(61) && !defender.has(effect::fire_resistance_ring))
    {
        damage += roll(6);
    }
    // A flask of oil burns a mummy for 3d8; holy water burns the undead for 1d6+1.
    if (defender.has(122) && attacker.numbers.weapon_type == 86)
    {
        damage = roll(8) + roll(8) + roll(8);
    }
    // The kit's holy water is its item type 36h (effects.c,
    // fx_weapon_type36_damage).
    const int holy_water = kit() ? 0x36 : 85;
    if (defender.has(117) && attacker.numbers.weapon_type == holy_water)
    {
        damage = roll(6) + 1;
    }
    if (defender.has(dq_effect::disarming_body) && attacker.record &&
        distance(attacker, defender) <= 1 && damage > 0 && damage % 3 == 0 &&
        roll(20) >= attacker.record->current_abilities.dexterity - 3)
    {
        const auto held = held_now(attacker);
        const auto weapon = std::ranges::find_if(held,
                                                 [](const Held &item)
                                                 {
                                                     return !item.gone && !item.conjured &&
                                                            item.carried.item.readied() &&
                                                            item.carried.item.cursed_raw == 0 &&
                                                            item.carried.type.worn_location == 0;
                                                 });
        if (weapon != held.end() &&
            !(weapon->carried.item.type == 6 && weapon->carried.item.name_components[2] == 121))
        {
            const auto at = static_cast<std::size_t>(weapon - held.begin());
            toggle_readied(attacker, at);
            attacker.numbers.effects.push_back(
                {effect::baaz_banked_weapon, 0, static_cast<int>(at + 1), false});
            log({"{name} loses a weapon.", {{"name", attacker.name}}});
        }
    }
    return damage;
}

// A black pudding's blow eats into the armor its target has readied (the
// kit's effects.c, 186): a plus at a time, banded and plate mail (the kit's
// item types 23h and 24h) to -1 and other armor to 0, then dissolves it.
// Whether there was armor to eat, which takes the blow's harm.
bool Session::eat_armor(const Fighter &pudding, Fighter &target)
{
    constexpr std::uint8_t armor_slot = 2, banded_mail = 0x23, plate_mail = 0x24;
    if (!target.record)
    {
        return false;
    }
    const auto before = held_now(target);
    auto after = before;
    bool eaten = false;
    for (auto &held : after)
    {
        auto &armor = held.carried.item;
        if (held.gone || !armor.readied() || held.carried.type.worn_location != armor_slot)
        {
            continue;
        }
        eaten = true;
        const int lowest = armor.type == banded_mail || armor.type == plate_mail ? -1 : 0;
        if (armor.magic_bonus > lowest)
        {
            --armor.magic_bonus;
            armor.raw[50] = static_cast<std::uint8_t>(armor.magic_bonus);
            log({"{name} is dissolving {target}'s armor!",
                 {{"name", pudding.name}, {"target", target.name}}});
        }
        else
        {
            held.gone = true;
            log({"{name} dissolves {target}'s armor!",
                 {{"name", pudding.name}, {"target", target.name}}});
        }
    }
    if (eaten)
    {
        rework_gear(target, before, std::move(after));
        if (!target.has(effect::armor_eaten))
        {
            target.numbers.effects.push_back({effect::armor_eaten, 0, 0, false});
        }
    }
    return eaten;
}

// A saving throw (fxengine.pas, RollSavingThrow): its natural roll
// (save_die), judged by saves_with.
bool Session::saving_throw(const Fighter &f, int save, int modifier, const Fighter *caster,
                           int element)
{
    return saves_with(f, save_die(f, fight_dice()), save, modifier, caster, element);
}

// The chance a saving throw saves, each way its natural roll falls weighed.
double Session::save_chance(const Fighter &f, int save, int modifier, const Fighter *caster,
                            int element) const
{
    double chance = 0;
    each_way<bool>(
        [&](auto &&draw)
        {
            return saves_with(f, save_die(f, draw), save, modifier, caster, element);
        },
        [&](double way, bool saved)
        {
            chance += saved ? way : 0;
        });
    return chance;
}

// Whether a saving throw of this natural roll saves: a natural 1 fails and a
// natural 20 saves; otherwise the roll with the creature's save bonus and the
// modifier must reach its number for the save. A total below 0 fails, where
// the original's byte wrapped it to a success (an obvious bug, fixed).
bool Session::saves_with(const Fighter &f, int natural, int save, int modifier,
                         const Fighter *caster, int element) const
{
    if (natural == 1 || natural == 20)
    {
        return natural == 20;
    }
    // Immunity to paralysis and poison makes those saves (effhndlr.pas, 111 and 125).
    if (save == 0 && (f.has(111) || f.has(125)))
    {
        return true;
    }
    // The saver's effects (effhndlr.pas, SAVING_THROW): Constitution for a
    // dwarf, gnome or halfling against poison, spells and wands; Shield;
    // protection against a caster of that alignment; and resistance to the
    // cold or fire the spell carries.
    const int constitution = f.numbers.constitution;
    const int hardiness = constitution < 4     ? 0
                          : constitution <= 6  ? 1
                          : constitution <= 10 ? 2
                          : constitution <= 13 ? 3
                          : constitution <= 17 ? 4
                          : constitution <= 20 ? 5
                                               : 0;
    // Secret's 94 counts Constitution against all three (EffConPsnSave).
    const bool poison_spell_or_wand = save == 0 || save == 2 || save == 4;
    if ((f.has(90) && save == 0) || (f.has(97) && (save == 4 || save == 2)) ||
        (f.has(effect::hardy) && poison_spell_or_wand))
    {
        modifier += hardiness;
    }
    // Secret's luckstone saves 1 better (EffSaveBonus).
    modifier += f.has(effect::luck) ? 1 : 0;
    modifier += f.has(effect::shield) ? 1 : 0;
    modifier -= f.has(effect::blind) ? 4 : 0;
    modifier -= f.has(effect::bestow_curse) ? 4 : 0;
    modifier += prayer_bonus(f);
    if (caster)
    {
        const int alignment = caster->numbers.alignment;
        const bool evil = alignment == 2 || alignment == 5 || alignment == 8;
        const bool good = alignment == 0 || alignment == 3 || alignment == 6;
        if ((evil &&
             protected_from(f, effect::protection_from_evil, effect::protection_from_evil_aura)) ||
            (good &&
             protected_from(f, effect::protection_from_good, effect::protection_from_good_aura)))
        {
            modifier += 2;
        }
    }
    if (((element & 2) && f.has(effect::resist_cold)) ||
        ((element & 1) && f.has(effect::resist_fire)))
    {
        modifier += 3;
    }
    modifier += (element & 1) && f.has(effect::fire_resistance_ring) ? 4 : 0;
    modifier += (element & 1) && f.has(dq_effect::resists_fire) ? 4 : 0;
    if ((element & 16) && f.has(dq_effect::mask_16_save_bonus))
    {
        modifier = 20;
    }
    // Curse's fire shields: the hot one saves 2 better against cold, the
    // cold one against fire (HotFireShield, ColdFireShield).
    if (((element & 2) && f.has(250)) || ((element & 1) && f.has(254)))
    {
        modifier += 2;
    }
    const int total = natural + f.numbers.save_bonus + modifier;
    return total >= 0 && total >= f.numbers.saving_throws[static_cast<std::size_t>(save)];
}

// The opposition's Hit Points left, those of the creatures still fighting
// against the whole of every one's, rounded down to a step of 5.
void Session::update_hostile_hit_points()
{
    int left = 0, whole = 0;
    for (const auto &f : fighters_)
    {
        if (f.side != Side::opposition)
        {
            continue;
        }
        left += f.able() ? f.hit_points : 0;
        whole += f.numbers.max_hit_points;
    }
    if (whole > 0)
    {
        hostile_hit_points_percent_ = left * 20 / whole * 5;
    }
}

int Session::highest_enemy_movement(const Fighter &f) const
{
    int highest = 0;
    for (const auto &other : fighters_)
    {
        if (other.side != f.side && other.able())
        {
            highest = std::max(highest, movement_of(other));
        }
    }
    return highest;
}

// A creature whose morale counts checks it as its turn starts (combatai.pas,
// CheckMorale): it holds while its morale (twice the byte's low 7 bits, 0
// past 102) is at least the share of its Hit Points it has lost; an enemy of
// the party also holds while the opposition's Hit Points left are at least
// 100 less the encounter's morale. Broken, it runs when no enemy is faster,
// gives up when its Intelligence is 6 or more, and otherwise fights on. It
// may rally at its next turn. True when it surrendered.
bool Session::check_morale(Fighter &f)
{
    f.fleeing = false;
    // Gateway's black dragon gives up below 20 Hit Points when the last of
    // its side (effhndlr.pas, EffSurrender, at MORALE_CHECK).
    if (f.has(effect::surrenders_when_last) && f.hit_points < 20 &&
        std::ranges::none_of(
            fighters_,
            [&f](const Fighter &other)
            {
                return other.id != f.id && other.side == f.side && other.able();
            }))
    {
        put_out_of_play(f, Condition::surrendered);
        log({"{name} surrenders.", {{"name", f.name}}});
        return true;
    }
    release(f, effect::bite_hold);
    release(f, effect::blood_drain);
    release(f, effect::drinking_blood);
    release(f, effect::constricting);
    release(f, effect::squeezing);
    if (f.turned)
    {
        f.fleeing = true;
        log({"{name} is forced to flee.", {{"name", f.name}}});
        return false;
    }
    if (f.numbers.morale < non_player)
    {
        return false;
    }
    // A charmed creature never breaks (effhndlr.pas, the charm at MORALE_CHECK).
    if (f.has(effect::charm))
    {
        return false;
    }
    int morale = static_cast<int>(f.numbers.morale & 0x7F) * 2;
    if (morale > 102)
    {
        morale = 0;
    }
    // Bless adds 5 and Curse takes 5, never below 0, which fails (EffBless, EffCurse).
    const auto blessed = [&f](int value)
    {
        value += f.has(effect::bless) ? 5 : 0;
        return f.has(effect::curse) ? std::max(value - 5, 0) : value;
    };
    morale = morale == 0 ? 0 : blessed(morale);
    if (morale != 0 && morale >= 100 - f.hit_points * 100 / f.numbers.max_hit_points)
    {
        return false;
    }
    const int group = hostile_hit_points_percent_ == 0 ? 0 : blessed(hostile_hit_points_percent_);
    if (group != 0 && group >= 100 - static_cast<int>(morale_threshold_) &&
        f.side == Side::opposition)
    {
        return false;
    }
    if (highest_enemy_movement(f) <= movement_of(f))
    {
        f.fleeing = true;
        log({"{name} flees in panic.", {{"name", f.name}}});
        return false;
    }
    if (f.numbers.intelligence > 5)
    {
        put_out_of_play(f, Condition::surrendered);
        log({"{name} surrenders.", {{"name", f.name}}});
        return true;
    }
    return false;
}

// Off the field (fxengine.pas, PutOutOfPlayWithStatus): one that gave up has
// no Hit Points left; one that got away keeps its own. Its actions end.
void Session::put_out_of_play(Fighter &f, Condition condition)
{
    f.condition = condition;
    if (condition != Condition::fled)
    {
        f.hit_points = 0;
    }
    f.fleeing = false;
    end_actions(f);
    end_effects_at_zero(f);
}

// Stepping off the field (cbtacts.pas, AttemptFlee): it gets away with no
// enemy in sight, or with more movement than the fastest enemy, or on a 1 in
// 2 when they tie. Its actions end either way.
void Session::attempt_flee(Fighter &f)
{
    const bool watched =
        std::any_of(fighters_.begin(), fighters_.end(),
                    [this, &f](const Fighter &other)
                    {
                        return other.side != f.side && other.able() && sees(board_, f, other);
                    });
    const int fastest = highest_enemy_movement(f);
    const int own = movement_of(f);
    const bool away = !watched || fastest < own || (fastest == own && roll(2) == 1);
    if (away)
    {
        put_out_of_play(f, Condition::fled);
        log({"{name} got away.", {{"name", f.name}}});
        if (escape_ends_fight_ && f.side == Side::party && f.numbers.morale < non_player)
        {
            end_fight_by_escape(f);
        }
        return;
    }
    log({"{name} cannot get away.", {{"name", f.name}}});
    end_actions(f);
}

// One member's escape ends Gateway's maze fights (cbtacts.pas, AttemptFlee in
// combat mode 1): the party leaves by the side it ran off, and the monsters
// left stay behind, worth nothing.
void Session::end_fight_by_escape(const Fighter &escaped)
{
    escape_side_ = escaped.cell.y <= 0                   ? 0U
                   : escaped.cell.x >= board_.width - 1  ? 1U
                   : escaped.cell.y >= board_.height - 1 ? 2U
                                                         : 3U;
    for (auto &f : fighters_)
    {
        if (f.side == Side::opposition && f.condition == Condition::okay)
        {
            put_out_of_play(f, Condition::fled);
        }
    }
    settle_outcome();
}

// The squares just off the field next to a creature on its edge.
std::vector<Cell> Session::off_field_steps(const Fighter &f) const
{
    std::vector<Cell> steps;
    for (const auto square : squares_of(f))
    {
        for (int direction = 0; direction < 8; ++direction)
        {
            const Cell step{square.x + step_x[static_cast<std::size_t>(direction)],
                            square.y + step_y[static_cast<std::size_t>(direction)]};
            if (!board_.contains(step) &&
                std::find(steps.begin(), steps.end(), step) == steps.end())
            {
                steps.push_back(step);
            }
        }
    }
    return steps;
}

// A side with no one left able to fight has lost. When the party wins, its
// dying members wake unconscious (postcbt.pas).
void Session::settle_outcome()
{
    const auto side_standing = [this](Side side)
    {
        return std::any_of(fighters_.begin(), fighters_.end(),
                           [side](const Fighter &f)
                           {
                               return f.side == side && f.able();
                           });
    };
    // With none of the party left fighting, it fled when one of its own
    // characters got away, and is defeated otherwise (postcbt.pas,
    // DetermineCombatResults).
    if (!side_standing(Side::party))
    {
        const bool got_away =
            std::any_of(fighters_.begin(), fighters_.end(),
                        [](const Fighter &f)
                        {
                            return f.side == Side::party && f.condition == Condition::fled &&
                                   f.arrival_round == 0 && f.numbers.morale < non_player;
                        });
        outcome_ = got_away ? rules::Outcome::fled : rules::Outcome::defeat;
    }
    else if (!side_standing(Side::opposition))
    {
        win_fight();
    }
    if (outcome_ != rules::Outcome::ongoing)
    {
        // A frenzy ends with the fight, and the party plays the lover again
        // (postcbt.pas, DetermineCombatResults).
        for (auto &f : fighters_)
        {
            if (auto *love = f.find_effect(effect::in_love);
                love && love->strength == effect::frenzied_love)
            {
                love->strength = 0;
                if (f.numbers.morale == animated_player)
                {
                    f.numbers.morale = 0;
                }
            }
        }
        actor_ = 0;
        // A Buck Rogers fight ends with the new commands' statuses back to
        // okay, and no one blind, helpless or stunned (combat.pas
        // CombatDoneCleanup).
        for (auto &f : fighters_)
        {
            if (f.ground)
            {
                auto &creature = f.ground->creature();
                if (status_of(creature) >= BuckRogersStatus::dodging)
                {
                    set_status(creature, BuckRogersStatus::okay);
                }
                std::erase_if(f.ground->effects,
                              [](const GroundEffect &e)
                              {
                                  return e.code == ground_effect::blind ||
                                         e.code == ground_effect::helpless ||
                                         e.code == ground_effect::stunned;
                              });
            }
        }
        log({outcome_ == rules::Outcome::victory ? "The party wins the fight."
             : outcome_ == rules::Outcome::fled  ? "The party has fled."
                                                 : "The party is defeated.",
             {}});
    }
}

// The party wins, and its dying members wake unconscious (postcbt.pas,
// DetermineCombatResults).
void Session::win_fight()
{
    outcome_ = rules::Outcome::victory;
    for (auto &f : fighters_)
    {
        if (f.side == Side::party && f.condition == Condition::dying)
        {
            f.condition = Condition::unconscious;
            f.overkill = 0;
        }
    }
}

// A fight in which no one has attacked for 15 rounds ends (combat.pas,
// EndOfRound). With some of the party still on its feet the original counts
// it won (postcbt.pas, DetermineCombatResults: a member okay wins the
// fight); the monsters still standing leave the field, worth neither
// experience nor treasure, as the original pays only for those put down
// (DetermineLootExp).
void Session::end_quiet_fight()
{
    round_limit_reached_ = true;
    log({"No one has attacked for 15 rounds, and the fight ends.", {}});
    win_fight();
    actor_ = 0;
    log({"The party wins the fight.", {}});
}

rules::MovementGrid Session::grid_for(const Fighter &mover) const
{
    std::vector<rules::Occupant> occupants;
    for (const auto &f : fighters_)
    {
        if (f.id == mover.id || f.condition == Condition::dead || f.condition == Condition::fled ||
            f.condition == Condition::surrendered || f.condition == Condition::gone ||
            f.condition == Condition::stoned)
        {
            continue;
        }
        occupants.push_back({f.cell, f.side != mover.side, !f.able(), f.downed(), f.footprint});
    }
    return rules::MovementGrid(
        board_, mover.cell, occupants, false, false, false, mover.footprint, half_moves,
        original_movement() ? rules::StepRules::original : rules::StepRules::standard);
}

namespace
{
// A scroll's spells not yet read, a bit for each of its record's spell bytes.
int unread_spells(const UsableItem &scroll)
{
    int bits = 0;
    for (std::size_t at = 0; at < scroll.spells.size(); ++at)
    {
        if (scroll.spells[at] != 0)
        {
            bits |= 1 << at;
        }
    }
    return bits;
}
} // namespace

rules::Snapshot Session::snapshot() const
{
    rules::Snapshot state;
    state.identity = identity_;
    state.revision = revision_;
    state.round = round_;
    if (game_of(identity_) == GoldBoxGame::death_knights_of_krynn)
    {
        // EndOfRound advances one minute, including a fight's final round.
        const auto finished =
            round_ - (round_ > 0 && outcome_ == rules::Outcome::ongoing ? 1U : 0U);
        state.elapsed_milliseconds = static_cast<std::uint64_t>(finished) * 60000;
    }
    state.actor = actor_;
    state.outcome = outcome_;
    state.escape_side = escape_side_;
    state.crystal = crystal_;
    state.romance = romance_;
    state.round_limit_reached = round_limit_reached_;
    state.battlefield = board_;
    state.log_entries = log_;
    // Buck Rogers chaff and aerosol mist are shown where they lie, as the
    // original draws its clouds: the player must see what stops a launched
    // missile or grenade (ground_launch) and some shots (CalcWeaponDamage).
    for (const auto &cloud : ground_clouds_)
    {
        if (cloud.matter == CloudMatter::chaff || cloud.matter == CloudMatter::mist)
        {
            std::ranges::copy(cloud.squares(), std::back_inserter(state.obscured));
        }
    }
    std::vector<const Fighter *> order;
    for (const auto &f : fighters_)
    {
        order.push_back(&f);
    }
    std::stable_sort(order.begin(), order.end(),
                     [](const Fighter *a, const Fighter *b)
                     {
                         return a->initiative > b->initiative;
                     });
    for (const auto *f : order)
    {
        rules::CombatantView view;
        view.id = f->id;
        view.name = f->name;
        view.definition = f->definition;
        view.side = f->side;
        view.cell = f->cell;
        view.footprint = f->footprint;
        view.facing_left = f->facing_left;
        view.hit_points = f->condition == Condition::dying ? -f->overkill : f->hit_points;
        view.max_hit_points = f->numbers.max_hit_points;
        view.armor_class = f->numbers.armor_class;
        view.initiative = f->initiative;
        // Half-moves are 5 feet each on the original's 10-foot squares.
        view.movement_feet = f->half_moves * 5 / 2;
        view.action = f->id == actor_;
        view.conscious = f->able();
        view.dead = f->condition == Condition::dead || f->condition == Condition::gone;
        view.status = f->condition == Condition::dead          ? "dead"
                      : f->condition == Condition::dying       ? "dying"
                      : f->condition == Condition::unconscious ? "unconscious"
                      : f->condition == Condition::fled
                          ? (f->arrival_round == gated_reserve_round ? "waiting" : "fled")
                      : f->condition == Condition::surrendered ? "surrendered"
                      : f->condition == Condition::gone        ? "gone"
                      : f->condition == Condition::stoned      ? "stoned"
                      : f->helpless()                          ? "helpless"
                                                               : "";
        view.fled = f->condition == Condition::fled;
        view.surrendered = f->condition == Condition::surrendered;
        view.panicked = f->fleeing;
        // What outlasts the fight: its Hit Points (one that got away keeps
        // them), death, and its numbers as the fight left them (levels
        // drained, effects still running), for the game to keep.
        const bool keeps = f->condition == Condition::okay || f->condition == Condition::fled;
        // A statue stays one until Stone to Flesh (status PETRIFIED).
        const bool native_npc = game_of(identity_) == GoldBoxGame::dark_queen_of_krynn &&
                                f->side == Side::party && f->record &&
                                (f->record->npc_flag & 0x80) != 0;
        // Leadership takes a Buck Rogers companion off the computer's control
        // for the fight (ground_begin_round); nothing else does.
        const bool led_companion = f->ground && f->side == Side::party &&
                                   f->ground->creature().companion != 0 &&
                                   !f->ground->computer_plays;
        view.player_controls_ally =
            (native_npc && f->has(dq_effect::led_by_knight)) || led_companion;
        view.computer_plays = frenzied(*f) || (f->ground && f->ground->computer_plays) ||
                              (native_npc && !view.player_controls_ally);
        view.persistent = {keeps ? f->hit_points : 0, view.dead, f->numbers.encode(),
                           f->condition == Condition::stoned ? std::string(stoned_description)
                                                             : std::string()};
        // A Buck Rogers creature shows its ascending Armor Class and its
        // record's status, and leaves its record and items as the fight left
        // them: Hit Points, injury, rounds and readied gear
        // (decode_ground_record).
        if (f->ground)
        {
            const auto status = status_of(f->ground->creature());
            view.armor_class = ground_numbers_of(*f).armor_class[0];
            view.max_hit_points = f->ground->creature().hit_point_maximum;
            view.hit_points = f->hit_points;
            view.dead = status == BuckRogersStatus::dead || status == BuckRogersStatus::gone;
            view.status = status == BuckRogersStatus::comatose   ? "comatose"
                          : status == BuckRogersStatus::poisoned ? "poisoned"
                                                                 : view.status;
            view.persistent = {f->hit_points, view.dead, encode_ground_record(f->ground->gear),
                               std::string()};
            state.combatants.push_back(std::move(view));
            continue;
        }
        // What is left of a party member's items, for the game to trim its
        // inventory to: an item gone is 0, a scroll the bits of its spells
        // not read.
        if (f->record)
        {
            // What it carries as the fight has left it: each item whose
            // count changed, and the items readied when the fight changed
            // which.
            const auto held = held_now(*f);
            std::vector<std::pair<int, std::uint64_t>> readied;
            for (const auto &item : held)
            {
                // A hammer the fight conjured outlasts it with its spell
                // (EffSpiritualHammer), as an item the member carries.
                if (!item.inventory_id && item.conjured && !item.gone)
                {
                    view.items_gained.push_back({por::original_item_key(item.carried.item),
                                                 item.name, item.carried.item.readied()});
                    continue;
                }
                if (!item.inventory_id)
                {
                    continue;
                }
                const int left = item.gone ? 0 : held_count(item.carried);
                if (left != item.start_count)
                {
                    view.item_uses_left.emplace_back(item.inventory_id,
                                                     static_cast<unsigned>(left));
                }
                if (!item.gone && item.carried.item.readied())
                {
                    readied.emplace_back(item.readied_order, item.inventory_id);
                }
                // Armor a black pudding has eaten into keeps the plus it has left.
                if (!item.gone && f->has(effect::armor_eaten) &&
                    item.carried.type.worn_location == 2)
                {
                    view.item_pluses.emplace_back(item.inventory_id, item.carried.item.magic_bonus);
                }
            }
            if (f->readied_changed)
            {
                std::ranges::sort(readied);
                auto &ids = view.readied_items.emplace();
                for (const auto &[readied_order, id] : readied)
                {
                    ids.push_back(id);
                }
            }
            state.combatants.push_back(std::move(view));
            continue;
        }
        for (const auto &[ordinal, id] : f->item_ids)
        {
            const auto &items = f->numbers.items;
            const auto usable = std::ranges::find(items, ordinal, &UsableItem::ordinal);
            const int left = usable == items.end() ? 0
                             : usable->scroll      ? unread_spells(*usable)
                             : usable->charges > 0 ? usable->charges
                                                   : std::max(usable->quantity, 1);
            view.item_uses_left.emplace_back(id, static_cast<unsigned>(left));
        }
        if (f->shot_item)
        {
            const auto &missile = f->numbers.missile;
            const int left = missile && missile->loaded ? std::max(missile->quantity, 1) : 0;
            view.item_uses_left.emplace_back(f->shot_item, static_cast<unsigned>(left));
        }
        state.combatants.push_back(std::move(view));
    }
    return state;
}

std::vector<rules::Command> Session::legal_commands() const
{
    std::vector<rules::Command> commands;
    const auto *actor = find(actor_);
    if (outcome_ != rules::Outcome::ongoing || !actor)
    {
        return commands;
    }
    if (actor->aiming)
    {
        return aiming_commands(*actor);
    }
    if (actor->ground)
    {
        return ground_commands(*actor);
    }
    // A lover in a frenzy fights under the computer's control (EffInLove).
    if (frenzied(*actor))
    {
        rules::Command quick;
        quick.revision = revision_;
        quick.actor = actor_;
        quick.verb = "quick";
        commands.push_back(quick);
        return commands;
    }
    for (const auto &cell : movement_reach(actor_))
    {
        rules::Command move;
        move.revision = revision_;
        move.actor = actor_;
        move.verb = "move";
        move.destination = cell;
        commands.push_back(move);
    }
    if (actor->attacks() > 0)
    {
        for (const auto &f : fighters_)
        {
            // A creature down is out of the fight (charhlp.pas, InflictDamage
            // clears its enabled flag) and is no target.
            if (f.side == actor->side || !f.able())
            {
                continue;
            }
            if (auto verb = attack_verb(*actor, f))
            {
                rules::Command attack;
                attack.revision = revision_;
                attack.actor = actor_;
                attack.target = f.id;
                attack.verb = std::move(*verb);
                attack.intent = rules::Intent::hostile;
                commands.push_back(attack);
            }
        }
    }
    // The Done menu (combat.pas, DoneMenu): Guard, Delay, Bandage and ending its actions.
    const auto offer = [&commands, actor, this](std::string verb, EntityId target = 0)
    {
        rules::Command command;
        command.revision = revision_;
        command.actor = actor->id;
        command.verb = std::move(verb);
        command.target = target;
        commands.push_back(command);
    };
    // Cast, while it may and has a spell ready for a fight (combat.pas, CombatMenu).
    if (actor->cast_allowed && !silenced(*actor) && !actor->has(effect::no_magic))
    {
        std::vector<int> offered_spells;
        for (const int number : actor->numbers.spells)
        {
            const auto *known = game_spell(number);
            if (known && known->when != 0 && spell_built(number) &&
                (!underwater_ || underwater_spell(number)) &&
                std::find(offered_spells.begin(), offered_spells.end(), number) ==
                    offered_spells.end())
            {
                offered_spells.push_back(number);
                rules::Command cast;
                cast.revision = revision_;
                cast.actor = actor->id;
                cast.verb = "cast";
                cast.item = static_cast<unsigned>(number);
                cast.label = std::string(known->name);
                commands.push_back(cast);
            }
        }
    }
    // Use, for a readied item with a spell or power for a fight; a scroll's
    // spells once it is known, or by Read Magic, or a cleric's scroll for a
    // cleric (viewchar.pas, UseItemMenu; magic.pas, the scroll).
    if (actor->item_use_allowed && !silenced(*actor) &&
        !((darkness() || game_of(identity_) == GoldBoxGame::dark_queen_of_krynn) &&
          actor->has(effect::no_magic)))
    {
        const bool cleric =
            actor->numbers.class_levels[static_cast<std::size_t>(ClassSlot::cleric)] > 0;
        for (const auto &item : actor->numbers.items)
        {
            const bool readable = !item.scroll || item.identified ||
                                  actor->has(effect::read_magic) || (cleric && item.slot == 12);
            if (!item.readied || !readable)
            {
                continue;
            }
            for (const int number : item.spells)
            {
                const auto *known = game_spell(number);
                if (!known || known->when == 0 || !spell_built(number))
                {
                    continue;
                }
                rules::Command use;
                use.revision = revision_;
                use.actor = actor->id;
                use.verb = "use";
                use.item = static_cast<unsigned>(item.ordinal * 256 + number);
                use.label = std::string(known->name);
                commands.push_back(use);
                if (!item.scroll)
                {
                    break;
                }
            }
        }
    }
    // Ready, for an item a party member carries and may ready, and put away,
    // for one readied that is not cursed (viewchar.pas, ToggleReadyItem).
    if (actor->record)
    {
        const auto held = held_now(*actor);
        std::vector<CarriedItem> readied;
        for (const auto &item : held)
        {
            if (!item.gone && item.carried.item.readied())
            {
                readied.push_back(item.carried);
            }
        }
        for (std::size_t at = 0; at < held.size(); ++at)
        {
            const auto &item = held[at];
            const bool is_readied = item.carried.item.readied();
            const bool banked =
                std::ranges::any_of(actor->numbers.effects,
                                    [at](const Effect &power)
                                    {
                                        return power.code == effect::baaz_banked_weapon &&
                                               power.strength == static_cast<int>(at + 1);
                                    });
            const bool wrong_order =
                champions() && champions_ready_refusal(item.carried.item, actor->record->race,
                                                       actor->record->robe_order)
                                   .has_value();
            if (item.gone || banked || (!is_readied && wrong_order) ||
                (!item.inventory_id && !item.conjured) ||
                (is_readied ? item.carried.item.cursed_raw != 0
                            : readying_refusal(item.carried, readied, actor->record->item_limit,
                                               game_of(identity_))
                                  .has_value()))
            {
                continue;
            }
            rules::Command toggle;
            toggle.revision = revision_;
            toggle.actor = actor_;
            toggle.verb = is_readied ? "unready" : "ready";
            toggle.item = static_cast<unsigned>(at);
            toggle.label = item.name;
            commands.push_back(toggle);
        }
    }
    // Guard needs a weapon that strikes next to it (combat.pas, DoneMenu).
    if (!actor->numbers.missile || actor->numbers.missile->throwable)
    {
        offer("guard");
    }
    offer("delay");
    // Quick: the original's AI plays its turn (combat.pas, TurnOnAuto); a party
    // member's also with its spells, as with the party's Quick magic on.
    offer("quick");
    if (actor->numbers.morale < non_player)
    {
        offer("quick_magic");
    }
    // Turn, once a fight for one with cleric levels (combat.pas, CombatMenu).
    // Secret has no turning (its CombatMenu offers none).
    if (may_turn(*actor))
    {
        offer("turn");
    }
    if (champions() && actor->numbers.race == 5 &&
        std::ranges::none_of(fighters_,
                             [](const Fighter &fighter)
                             {
                                 return fighter.has(effect::kender_yelled);
                             }))
    {
        offer("yell");
    }
    for (const auto &f : fighters_)
    {
        if (can_bandage(*actor, f))
        {
            offer("bandage", f.id);
        }
    }
    // Stepping off the field offers to flee, with a step left (combat.pas, MoveMenu).
    if (actor->half_moves >= 2)
    {
        for (const auto step : off_field_steps(*actor))
        {
            rules::Command flee;
            flee.revision = revision_;
            flee.actor = actor->id;
            flee.verb = "flee";
            flee.destination = step;
            commands.push_back(flee);
        }
    }
    offer("end");
    return commands;
}

std::vector<Cell> Session::movement_reach(EntityId id) const
{
    std::vector<Cell> cells;
    const auto *mover = find(id);
    if (!mover || id != actor_ || mover->half_moves <= 0)
    {
        return cells;
    }
    const auto reach = grid_for(*mover).reachable(mover->half_moves);
    for (int y = 0; y < board_.height; ++y)
    {
        for (int x = 0; x < board_.width; ++x)
        {
            if (reach.cost_to({x, y}))
            {
                cells.push_back({x, y});
            }
        }
    }
    return cells;
}

bool Session::submit(const rules::Command &command)
{
    const auto legal = legal_commands();
    const bool offered =
        std::any_of(legal.begin(), legal.end(),
                    [&command](const rules::Command &c)
                    {
                        return c.revision == command.revision && c.actor == command.actor &&
                               c.verb == command.verb && c.target == command.target &&
                               c.item == command.item &&
                               ((c.verb != "move" && c.verb != "flee" && c.verb != "aim" &&
                                 c.verb != "launch" && c.verb != "land") ||
                                c.destination == command.destination);
                    });
    if (!offered)
    {
        return false;
    }
    auto &actor = *find(actor_);
    if (actor.ground && ground_submit(actor, command))
    {
    }
    else if (command.verb == "move")
    {
        // Step by step along the cheapest path, which free attacks and guards may cut short.
        for (const auto cell :
             grid_for(actor).reachable(actor.half_moves).path_to(command.destination))
        {
            if (!actor.able() || actor.half_moves <= 0)
            {
                break;
            }
            step_to(actor, cell);
        }
        // As the move menu closes, less than a straight step is nothing
        // (combat.pas, the move menu).
        if (original_movement() && actor.half_moves < 2)
        {
            actor.half_moves = 0;
        }
    }
    else if (command.verb == "melee")
    {
        auto &target = *find(command.target);
        note_attack(actor, target);
        if (!sweep(actor, target))
        {
            make_attack(actor, target, false);
        }
    }
    else if (command.verb == "ranged" || command.verb == "throw")
    {
        auto &target = *find(command.target);
        note_attack(actor, target);
        shoot(actor, target);
    }
    else if (command.verb == "yell")
    {
        kender_yell(actor);
    }
    else if (command.verb == "turn")
    {
        turn_undead(actor);
    }
    else if (command.verb == "cast")
    {
        begin_cast(actor, static_cast<int>(command.item));
    }
    else if (command.verb == "use")
    {
        // An item's spell goes off at once (viewchar.pas, UseItem).
        actor.using_item = static_cast<int>(command.item / 256);
        log({"{name} uses an item.", {{"name", actor.name}}});
        start_aiming(actor, static_cast<int>(command.item % 256));
    }
    else if (command.verb == "aim")
    {
        const auto &known = *game_spell(actor.aiming);
        const int area = known.area & 0x0F;
        const bool native_ground = game_of(identity_) == GoldBoxGame::dark_queen_of_krynn &&
                                   area == 15 && command.target == 0;
        if ((area >= 8 && area <= 14) || native_ground)
        {
            const int radius = native_ground ? known.area >> 4 : area & 7;
            resolve_spell(actor, within(command.destination, radius), command.destination);
        }
        else
        {
            actor.picked.push_back(command.target);
            if (--actor.picks_left <= 0)
            {
                resolve_spell(actor, actor.picked, find(command.target)->cell);
            }
        }
    }
    else if (command.verb == "blast_delay")
    {
        actor.blast_delay = static_cast<int>(command.item);
    }
    else if (command.verb == "flames")
    {
        resolve_spell(actor, {actor.id}, actor.cell, static_cast<Flames>(command.item));
    }
    else if (command.verb == "aim_done")
    {
        resolve_spell(actor, actor.picked, find(actor.picked.back())->cell);
    }
    else if (command.verb == "abort")
    {
        log({"{name} aborts the spell.", {{"name", actor.name}}});
        // An item's aim given up ends the turn but spends nothing; the
        // original spent the charge (an obvious bug, fixed).
        if (actor.using_item >= 0)
        {
            actor.aiming = actor.picks_left = 0;
            actor.picked.clear();
            actor.using_item = -1;
        }
        else
        {
            lose_spell(actor);
        }
        end_actions(actor);
    }
    else if (command.verb == "flee")
    {
        attempt_flee(actor);
    }
    else if (command.verb == "quick" || command.verb == "quick_magic")
    {
        ai_turn(actor, command.verb == "quick_magic");
    }
    else if (command.verb == "ready" || command.verb == "unready")
    {
        toggle_readied(actor, command.item);
    }
    else if (command.verb == "guard")
    {
        end_actions(actor);
        actor.guarding = true;
        log({"{name} guards.", {{"name", actor.name}}});
    }
    else if (command.verb == "delay")
    {
        // It acts again later this round, at initiative 1; in Secret and Pools
        // of Darkness, last of all (combat.pas, ManualControl, in their
        // reconstructions).
        actor.initiative = 1;
        if (ordered_rounds())
        {
            turn_order_.push_back(actor.id);
        }
        actor_ = 0;
    }
    else if (command.verb == "bandage")
    {
        auto &dying = *find(command.target);
        dying.condition = Condition::unconscious;
        dying.overkill = 0;
        log({"{name} is bandaged.", {{"name", dying.name}}});
        end_actions(actor);
    }
    else
    {
        end_actions(actor);
    }
    ++revision_;
    // A creature in a jetpack's flight lands before its turn goes on.
    const bool flying = actor_ != 0 && actor.ground && actor.ground->flying && actor.able();
    if (outcome_ == rules::Outcome::ongoing && (actor_ == 0 || !actor.aiming) && !flying &&
        (actor_ == 0 || actor.initiative == 0 || !actor.able() ||
         (actor.half_moves == 0 && actor.attacks() == 0)))
    {
        next_actor();
    }
    return true;
}

void Session::turn_against_party(const std::vector<EntityId> &turning)
{
    for (const auto id : turning)
    {
        if (auto *f = find(id))
        {
            f->side = Side::opposition;
        }
    }
    ++revision_;
}

// Buck Rogers ground combat (buck_rogers_combat.h; docs/games/countdown.md,
// its ground combat). The fight keeps its grid, turns and computer play; a
// GroundFighter's rolls, damage, injuries, area weapons, clouds and commands
// are its game's.

// A participant of a Buck Rogers fight: its record and gear from its
// profile, a party member's Hit Points as the game kept them, and the
// difficulty (rules::OriginalStrength): a hostile creature's Hit Points as
// LOAD MONSTER scales them, which become its maximum (eclvm.pas
// EclLoadMonster), and what its side's rolls and its Armor Class gain.
GroundFighter ground_fighter(const rules::Participant &p, const std::string &name)
{
    auto gear = decode_ground_record(p.character_profile);
    if (!gear)
    {
        throw std::runtime_error("A Buck Rogers creature has no record: " + name);
    }
    GroundFighter fighter{std::move(*gear)};
    auto &creature = fighter.creature();
    const auto &strength = p.original_strength;
    if (strength.hit_points_percent != 100)
    {
        const int scaled =
            std::max(creature.hit_points * static_cast<int>(strength.hit_points_percent) / 100, 1);
        creature.hit_points = static_cast<std::uint16_t>(scaled);
        creature.hit_point_maximum = creature.hit_points;
    }
    if (p.state)
    {
        creature.hit_points = static_cast<std::uint16_t>(
            std::clamp(p.state->hit_points, 0, static_cast<int>(creature.hit_point_maximum)));
        if (p.state->dead)
        {
            set_status(creature, BuckRogersStatus::dead);
        }
    }
    fighter.to_hit = strength.to_hit;
    fighter.armor_class_bonus = strength.armor_class_bonus;
    fighter.computer_plays = p.side == Side::party && creature.companion != 0;
    return fighter;
}

// What the fight's frame reads of a Buck Rogers creature (Combatant): its Hit
// Points, movement, attacks and missile, level, Intelligence and morale, and
// its attack roll and Armor Class read descending, as the computer weighs
// them: a combat value b and an Armor Class a hit when b + d20 reaches a, so
// a THAC0 of 60 - b against an Armor Class of 60 - a (viewchar.pas
// PrintCombatStats shows the THAC0 so).
Combatant ground_combatant(const GroundFighter &fighter, const GroundNumbers &numbers)
{
    const auto &creature = fighter.creature();
    const auto &items = fighter.gear.items;
    const auto &types = fighter.gear.types;
    Combatant c;
    c.thac0 = 60 - numbers.attack_bonus;
    c.armor_class = 60 - numbers.armor_class[0];
    c.rear_armor_class = 60 - numbers.armor_class[1];
    c.hit_points = creature.hit_points;
    c.max_hit_points = creature.hit_point_maximum;
    c.movement = numbers.movement;
    const auto weapon = readied_weapon(items, types);
    const int rate =
        weapon ? type_of(items[*weapon], types)->attacks_doubled : creature.unarmed_attacks_doubled;
    for (std::size_t slot = 0; slot < c.attacks.size(); ++slot)
    {
        const auto &dice = numbers.damage[slot];
        c.attacks[slot] = {slot == 0 ? rate : creature.attacks_doubled, dice.count, dice.sides,
                           dice.bonus};
    }
    c.level = creature.level;
    c.intelligence = creature.abilities[static_cast<std::size_t>(BuckRogersAbility::intelligence)];
    c.morale = creature.npc;
    c.initiative_bonus = buck_rogers_missile_adjustment(
        creature.abilities[static_cast<std::size_t>(BuckRogersAbility::dexterity)]);
    if (const int range = readied_weapon_range(items, types); weapon && range > 1)
    {
        Missile missile;
        missile.range = range;
        missile.rate_per_two_rounds = rate;
        missile.quantity = items[*weapon].quantity;
        missile.loaded = missile.quantity > 0;
        c.missile = missile;
    }
    return c;
}

// Its numbers this moment (charhlp.pas RecalcStats): its side's tactics, the
// difficulty's Armor Class, and blindness 4 worse on both (EffBlind).
GroundNumbers Session::ground_numbers_of(const Fighter &f) const
{
    const auto &g = *f.ground;
    auto numbers = ground_numbers(g.creature(), g.gear.items, g.gear.types,
                                  tactics_[static_cast<std::size_t>(f.side)]);
    const int better = g.armor_class_bonus - (g.has(ground_effect::blind) ? 4 : 0);
    numbers.armor_class[0] += better;
    numbers.armor_class[1] += better;
    return numbers;
}

// Works its numbers out again into its record, as the original does
// (RecalcStats), and into what the fight's frame reads.
void Session::ground_refresh(Fighter &f)
{
    auto &creature = f.ground->creature();
    const auto numbers = ground_numbers_of(f);
    const auto byte = [](int value)
    {
        return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
    };
    creature.attack_bonus = byte(numbers.attack_bonus);
    creature.armor_class = {byte(numbers.armor_class[0]), byte(numbers.armor_class[1])};
    creature.armor_flags = numbers.armor_flags;
    creature.current_movement = byte(numbers.movement);
    creature.encumbrance = static_cast<std::uint16_t>(numbers.encumbrance);
    f.numbers = ground_combatant(*f.ground, numbers);
}

// The tactics rolls (cbtacts.pas RollTactics): in the party's order, each
// side's creatures may roll until one makes it.
void Session::ground_tactics()
{
    tactics_ = {};
    for (auto &f : fighters_)
    {
        auto &made = tactics_[static_cast<std::size_t>(f.side)];
        if (!f.ground || made ||
            !rolls_tactics(f.ground->creature(), f.ground->has(ground_effect::stunned)) ||
            !ground_skill_succeeds(f.ground->creature(), Skill::battle_tactics, random_))
        {
            continue;
        }
        made = true;
        log({"{name} makes the tactics roll.", {{"name", f.name}}});
    }
}

// A round begins (combat.pas CombatMain; cbtacts.pas BeginRound). As the
// fight begins, Leadership takes companions off the computer's control
// (UseLeadership). Then the tactics rolls; each creature's attacks, its
// initiative (d10, its Dexterity's missile adjustment and 1, at least 2, 6
// less when surprised in the first round, 0 outside 0 to 20) with a d127 to
// break ties, and its movement; and the round's order, highest first
// (combat.pas OrderCreatures).
void Session::ground_begin_round()
{
    if (round_ == 1)
    {
        for (auto &leader : fighters_)
        {
            if (!leader.ground || leader.side != Side::party ||
                !may_lead(leader.ground->creature()))
            {
                continue;
            }
            for (auto &follower : fighters_)
            {
                if (!follower.ground || follower.side != Side::party ||
                    !follower.ground->computer_plays ||
                    !may_follow(follower.ground->creature(),
                                follower.ground->has(ground_effect::unkillable)) ||
                    !ground_skill_succeeds(leader.ground->creature(), Skill::leadership, random_))
                {
                    continue;
                }
                follower.ground->computer_plays = false;
                log({"{leader} leads {name}.", {{"leader", leader.name}, {"name", follower.name}}});
            }
        }
    }
    ground_tactics();
    for (auto &f : fighters_)
    {
        f.initiative = 0;
        if (!f.ground)
        {
            continue;
        }
        auto &g = *f.ground;
        const auto &creature = g.creature();
        const auto attacks =
            ground_attacks(creature, g.gear.items, g.gear.types, static_cast<int>(round_) - 1,
                           g.has(ground_effect::dizzy));
        f.attacks_left = attacks;
        f.attack_phase = 1;
        g.attacked = false;
        f.cast_allowed = f.item_use_allowed = false;
        if (f.able())
        {
            const int dexterity =
                creature.abilities[static_cast<std::size_t>(BuckRogersAbility::dexterity)];
            f.initiative = std::max(roll(10) + buck_rogers_missile_adjustment(dexterity) + 1, 2);
            if (round_ == 1 && f.surprised)
            {
                f.initiative -= 6;
            }
            if (f.initiative < 0 || f.initiative > 20)
            {
                f.initiative = 0;
            }
            g.tie_break = roll(127);
        }
        ground_refresh(f);
        const int bonus = f.side == Side::party ? party_move_modifier_ : 0;
        f.half_moves = 2 * (f.numbers.movement + bonus);
    }
    turn_order_.clear();
    for (const auto &f : fighters_)
    {
        auto place = turn_order_.begin();
        while (place != turn_order_.end())
        {
            const auto &other = *find(*place);
            if (f.initiative > other.initiative ||
                (f.initiative == other.initiative && f.ground && other.ground &&
                 other.ground->tie_break < f.ground->tie_break))
            {
                break;
            }
            ++place;
        }
        turn_order_.insert(place, f.id);
    }
}

// Its turn starts (combat.pas CreatureAction): dodging and a failed Zero-G
// roll end; helplessness, fear and a stun each cost it the turn, and the
// fear goes (effhndlr.pas EffHelpless, EffScared, EffStunned); in zero
// gravity it rolls Maneuver in Zero-G, and a failure leaves it 2 worse to
// hit with a quarter of its movement (cbtacts.pas RollZeroG). False when it
// does not act.
bool Session::ground_turn_starts(Fighter &f)
{
    auto &g = *f.ground;
    auto &creature = g.creature();
    const auto status = status_of(creature);
    if (status == BuckRogersStatus::dodging || status == BuckRogersStatus::failed_zero_g)
    {
        set_status(creature, BuckRogersStatus::okay);
    }
    for (const auto [code, message] : {std::pair{ground_effect::helpless, "{name} is helpless."},
                                       std::pair{ground_effect::scared, "{name} is intimidated."},
                                       std::pair{ground_effect::stunned, "{name} is stunned."}})
    {
        if (g.has(code) && f.initiative > 0)
        {
            end_actions(f);
            log({message, {{"name", f.name}}});
        }
    }
    const auto scared = std::ranges::find(g.effects, ground_effect::scared, &GroundEffect::code);
    if (scared != g.effects.end())
    {
        g.effects.erase(scared);
    }
    if (f.initiative <= 0)
    {
        return false;
    }
    ground_refresh(f);
    if (rolls_zero_g(zero_gravity_, creature.enabled != 0, f.initiative))
    {
        if (ground_skill_succeeds(creature, Skill::maneuver_in_zero_g, random_))
        {
            log({"{name} makes the Zero-G roll.", {{"name", f.name}}});
        }
        else
        {
            log({"{name} fails the Zero-G roll.", {{"name", f.name}}});
            set_command_status(creature, BuckRogersStatus::failed_zero_g);
            f.half_moves = zero_g_movement(f.half_moves);
            ground_refresh(f);
        }
    }
    return true;
}

bool Session::ground_cloud_at(Cell cell, CloudMatter matter) const
{
    return std::ranges::any_of(ground_clouds_,
                               [cell, matter](const GroundCloud &cloud)
                               {
                                   return cloud.matter == matter && cloud.covers_square(cell);
                               });
}

// A line walked as the original walks it (pathing.pas WalkLine): it sees
// chaff and mist on any square, cover (line class 2 ground, which may be
// impassable) past its first two squares where no cloud lies, and stops on a
// wall, on chaff when asked to, or past `range` straight steps (its doubled
// distance past 2 * range + 1).
Session::GroundWalk Session::ground_walk(Cell from, Cell to, int range, bool stops_at_chaff) const
{
    GroundWalk walk;
    LineWalk line(from, to);
    const int limit = range * 2 + 1;
    int grace = 2;
    do
    {
        const Cell at = line.at;
        bool stop = false;
        if (ground_cloud_at(at, CloudMatter::chaff))
        {
            walk.chaff = true;
            stop = stops_at_chaff;
        }
        else if (ground_cloud_at(at, CloudMatter::mist))
        {
            walk.mist = true;
        }
        else if (grace > 0)
        {
            --grace;
        }
        else if (!ground_cloud_at(at, CloudMatter::gas) &&
                 (board_.at(at) == rules::Terrain::cover ||
                  board_.at(at) == rules::Terrain::impassable_cover))
        {
            walk.blocked = true;
        }
        const bool wall = board_.at(at) == rules::Terrain::obstacle && from != to;
        if (wall || stop || line.distance > limit)
        {
            walk.stop = at;
            walk.distance = line.distance;
            return walk;
        }
    } while (line.next());
    walk.reached = true;
    walk.stop = to;
    walk.distance = line.distance;
    return walk;
}

// What lies on the line between an attacker and its defender: chaff and mist
// walking from the defender (cbtacts.pas CalcWeaponDamage), cover walking
// from the attacker (ApplyRangePenalty).
GroundLine Session::ground_line(const Fighter &attacker, const Fighter &defender) const
{
    const auto toward = ground_walk(defender.cell, attacker.cell, 255, false);
    const auto from = ground_walk(attacker.cell, defender.cell, 255, false);
    return {distance(attacker, defender), from.blocked, toward.chaff, toward.mist};
}

// Whether its readied weapon may strike a target (cbtacts.pas AimTargetMenu):
// within its range and in a line no wall stops, and with rounds left in a
// weapon that is not a melee one.
bool Session::ground_reaches(const Fighter &attacker, const Fighter &target) const
{
    const auto &g = *attacker.ground;
    const auto &items = g.gear.items;
    const auto &types = g.gear.types;
    if (distance(attacker, target) > readied_weapon_range(items, types) ||
        !sees(board_, attacker, target))
    {
        return false;
    }
    const auto weapon = readied_weapon(items, types);
    return !weapon || weapon_class_of(items, types) == WeaponClass::melee ||
           items[*weapon].quantity > 0;
}

// A Buck Rogers attack (cbtacts.pas MakeAttack and RollAttacks). The
// defender turns to face it unless it has drawn two attacks; an area weapon
// launches at the defender's square. Otherwise each attack left, the second
// slot's first, rolls against the defender's Armor Class from the front or
// the rear (target_number); a helpless defender is always hit. A backstab
// multiplies the damage; what stops a hit takes it away; a Sonic Stunner
// stuns instead. A poisonous creature's hits from its second slot poison. A
// ranged weapon spends a round for each first-slot attack.
void Session::ground_attack(Fighter &attacker, Fighter &defender)
{
    auto &a = *attacker.ground;
    auto &items = a.gear.items;
    const auto &types = a.gear.types;
    if (defender.flank_count < 2)
    {
        defender.direction = direction_toward(defender.cell, attacker.cell);
    }
    const int blow = direction_toward(attacker.cell, defender.cell);
    attacker.direction = blow;
    attacker.lock = defender.id;
    attacker.facing_left = defender.cell.x < attacker.cell.x;
    const auto weapon = readied_weapon(items, types);
    if (weapon && projectile_kind(items[*weapon]) != 0)
    {
        if (items[*weapon].quantity > 0)
        {
            ground_launch(attacker, defender.cell);
        }
        else
        {
            end_actions(attacker);
        }
        return;
    }
    if (attacker.attacks() == 0 || !defender.ground)
    {
        return;
    }
    auto &d = *defender.ground;
    ground_refresh(defender);
    const auto defending = ground_numbers_of(defender);
    const auto attacking = ground_numbers_of(attacker);
    const bool one_square = defender.footprint.width == 1 && defender.footprint.height == 1;
    const bool rear = rear_attack(defender.direction, blow, defender.flank_count, one_square);
    const int range = readied_weapon_range(items, types);
    const bool backstab = rear && backstab_position(defender.direction, blow, range) &&
                          backstab_check(a.creature(), random_);
    const auto line = ground_line(attacker, defender);
    const AttackSetting setting{rear, a.creature().career == 5, range > 1, attacker.guarding, range,
                                line};
    const int target = target_number(defending, setting);
    const std::optional<WeaponClass> weapon_class =
        weapon ? std::optional{weapon_class_of(items, types)} : std::nullopt;
    const bool stunner = weapon && items[*weapon].type == ground_item::sonic_stunner;
    if (backstab)
    {
        log({"{attacker} backstabs {name}.",
             {{"attacker", attacker.name}, {"name", defender.name}}});
    }
    bool hit_any = false;
    int first_slot_attacks = 0;
    a.attacked = true;
    for (int phase = attacker.attack_phase; phase >= 0 && defender.able(); --phase)
    {
        auto &left = attacker.attacks_left[static_cast<std::size_t>(phase)];
        while (left > 0 && defender.able() && attacker.able())
        {
            --left;
            attacker.attack_phase = phase;
            first_slot_attacks += phase == 0 ? 1 : 0;
            const int natural = roll(20);
            const int modifier = a.to_hit - (a.has(ground_effect::blind) ? 4 : 0);
            if (!ground_attack_hits(natural, attacking.attack_bonus, modifier, target) &&
                !defender.helpless())
            {
                continue;
            }
            hit_any = true;
            if (stunner)
            {
                const bool saved = roll_ground_save(d.creature(), ground_save::stun, 0,
                                                    d.has(ground_effect::blind), random_);
                const bool stunned =
                    give_ground_effect(d.effects, {ground_effect::stunned, stun_rounds(random_), 0},
                                       saved, false, true);
                log({stunned ? "{name} is stunned." : "{name} is unaffected.",
                     {{"name", defender.name}}});
                continue;
            }
            const auto &dice = attacking.damage[static_cast<std::size_t>(phase)];
            int rolled = 0;
            for (int die = 0; die < dice.count; ++die)
            {
                rolled += roll(std::max(dice.sides, 1));
            }
            int damage = std::max(rolled + dice.bonus, 0);
            if (backstab)
            {
                damage = backstab_damage(damage, a.creature());
            }
            const HitDefense defense{weapon_class, line.distance, line, defending.armor_flags,
                                     defender.hit_points};
            damage = damage_after_defenses(damage, defense, d.effects, random_);
            log({"{attacker} hits {name} for {damage}.",
                 {{"attacker", attacker.name},
                  {"name", defender.name},
                  {"damage", std::to_string(damage)}}});
            ground_inflict(defender, damage);
            if (phase == 1 && damage > 0 && defender.able())
            {
                if (const auto dizzying = std::ranges::find(
                        a.effects, ground_effect::dizzying_poison, &GroundEffect::code);
                    dizzying != a.effects.end())
                {
                    const bool saved =
                        roll_ground_save(d.creature(), ground_save::gas, dizzying->strength,
                                         d.has(ground_effect::blind), random_);
                    if (give_ground_effect(d.effects, {ground_effect::dizzy, 0, 0}, saved, false,
                                           false))
                    {
                        log({"{name} is poisoned and dizzy.", {{"name", defender.name}}});
                    }
                }
                if (const auto lethal = std::ranges::find(a.effects, ground_effect::lethal_poison,
                                                          &GroundEffect::code);
                    lethal != a.effects.end() &&
                    !roll_ground_save(d.creature(), ground_save::gas, lethal->strength,
                                      d.has(ground_effect::blind), random_))
                {
                    poison_kills(d.creature());
                    ground_down(defender);
                    log({"{name} is poisoned.", {{"name", defender.name}}});
                }
            }
        }
    }
    if (!hit_any)
    {
        log({"{attacker} misses {name}.", {{"attacker", attacker.name}, {"name", defender.name}}});
    }
    if (weapon && weapon_class != WeaponClass::melee &&
        spend_rounds(items[*weapon], *type_of(items[*weapon], types), first_slot_attacks))
    {
        items.erase(items.begin() + static_cast<std::ptrdiff_t>(*weapon));
    }
    ground_refresh(attacker);
    if (attacker.attacks() == 0 || !attacker.able())
    {
        end_actions(attacker);
    }
}

// A missile or grenade fired at a square (cbtacts.pas
// CombatLaunchProjectile). Its damage is rolled once; the rocket launcher
// and plasma thrower, and a grenade launcher, must be readied again. A shot
// whose combat value and d20, less the range's penalty, come to 50 or less
// goes astray, up to four tries at a square near the shooter; chaff on its
// way sets it off there. Every creature its blast reaches saves or takes
// the damage and the effect; a cloud stays. It ends the shooter's turn and
// spends a round, the last of a grenade bundle with it. The original put the
// launcher away before it rolled, so the shot went as if unarmed, without
// the weapon's range; and it threw away a stray shot's fourth landing; here
// the shot is the weapon's and every landing counts (docs/games/countdown.md,
// Bugs).
void Session::ground_launch(Fighter &attacker, Cell aim)
{
    auto &g = *attacker.ground;
    auto &items = g.gear.items;
    const auto &types = g.gear.types;
    const auto weapon = *readied_weapon(items, types);
    const auto &type = *type_of(items[weapon], types);
    const int kind = projectile_kind(items[weapon]);
    const auto &shot = projectile(kind);
    const auto weapon_class = static_cast<WeaponClass>(type.weapon_class);
    const int range = readied_weapon_range(items, types);
    const int attack_bonus = ground_numbers_of(attacker).attack_bonus;
    const auto blast = roll_blast(kind, type, items[weapon].plus, random_);
    g.attacked = true;
    const bool launched =
        put_away_launcher(items, types, weapon) || weapon_class != WeaponClass::grenade;
    log({launched ? "{name} launches a missile." : "{name} throws a grenade.",
         {{"name", attacker.name}}});
    const Cell from = attacker.cell;
    const int across = std::abs(aim.x - from.x), down = std::abs(aim.y - from.y);
    const int far = across > down ? across + down / 2 : down + across / 2;
    if (projectile_strays(attack_bonus, roll(20), far, range))
    {
        int limit = 4;
        for (int tries = 0; tries < 4; ++tries)
        {
            const Cell landing{from.x + scatter_offset(aim.x - from.x, random_),
                               from.y + scatter_offset(aim.y - from.y, random_)};
            if (!board_.contains(landing))
            {
                continue;
            }
            const auto walk = ground_walk(from, landing, limit, false);
            limit = walk.distance;
            if (walk.reached)
            {
                aim = landing;
                log({"It goes astray.", {}});
                break;
            }
        }
    }
    if (const auto way = ground_walk(from, aim, 255, true); !way.reached && way.chaff)
    {
        aim = way.stop;
        log({"It hits chaff.", {}});
    }
    std::vector<std::pair<int, Fighter *>> reached;
    for (auto &f : fighters_)
    {
        if (!f.able() || !f.ground)
        {
            continue;
        }
        int nearest = std::numeric_limits<int>::max();
        for (const auto square : squares_of(f))
        {
            if (const auto walk = ground_walk(aim, square, shot.radius, false); walk.reached)
            {
                nearest = std::min(nearest, walk.distance);
            }
        }
        if (nearest != std::numeric_limits<int>::max())
        {
            reached.emplace_back(nearest, &f);
        }
    }
    std::ranges::stable_sort(reached, {}, &std::pair<int, Fighter *>::first);
    if (shot.lingers)
    {
        if (const auto matter = cloud_of(kind))
        {
            ground_clouds_.insert(ground_clouds_.begin(),
                                  lay_ground_cloud(aim, *matter, zero_gravity_,
                                                   [this](Cell cell)
                                                   {
                                                       return board_.contains(cell) &&
                                                              !rules::blocks_movement(
                                                                  board_.at(cell));
                                                   }));
        }
    }
    const bool dazzle = items[weapon].type == ground_item::dazzle_grenade;
    const bool stun_grenade = items[weapon].type == ground_item::stun_grenade;
    for (const auto &[gap, target] : reached)
    {
        auto &t = *target->ground;
        const bool saved =
            spared_by_gear(kind, ground_numbers_of(*target).armor_flags) ||
            roll_ground_save(t.creature(), shot.save, 0, t.has(ground_effect::blind), random_);
        if (blast.damage > 0)
        {
            int damage = blast.damage;
            // Non-weapon damage meets two effects (fxengine.pas
            // InflictNonWeaponDamage): an immunity to heat and plasma, and
            // the unkillable one's luck.
            if (t.has(ground_effect::immune_to_heat_and_plasma) &&
                (weapon_class == WeaponClass::plasma || weapon_class == WeaponClass::heat))
            {
                damage = 0;
            }
            if (t.has(ground_effect::unkillable))
            {
                damage = std::min(damage, target->hit_points);
            }
            damage = blast_damage(damage, shot.saved, saved);
            if (damage > 0)
            {
                log({"{name} takes {damage}.",
                     {{"name", target->name}, {"damage", std::to_string(damage)}}});
                ground_inflict(*target, damage);
            }
        }
        if (target->able() && shot.effect > 0)
        {
            const bool given = give_ground_effect(t.effects, {shot.effect, blast.minutes, 0},
                                                  saved && shot.saved == SaveEffect::cancels,
                                                  dazzle, stun_grenade);
            log({given ? "{name} is affected." : "{name} is unaffected.",
                 {{"name", target->name}}});
        }
    }
    end_actions(attacker);
    if (spend_rounds(items[weapon], type, 1))
    {
        items.erase(items.begin() + static_cast<std::ptrdiff_t>(weapon));
    }
    ground_refresh(attacker);
}

// Damage (charhlp.pas InflictDamage): down at 0 and below (ground_down).
void Session::ground_inflict(Fighter &target, int damage)
{
    if (!target.able())
    {
        return;
    }
    auto &g = *target.ground;
    inflict_ground_damage(g.creature(), damage, g.dying_count);
    target.hit_points = g.creature().hit_points;
    const auto status = status_of(g.creature());
    if (can_act(status))
    {
        return;
    }
    ground_down(target);
    log({status == BuckRogersStatus::unconscious ? "{name} goes down."
         : status == BuckRogersStatus::dying     ? "{name} goes down, and is dying."
         : status == BuckRogersStatus::comatose  ? "{name} goes down, comatose."
                                                 : "{name} is killed.",
         {{"name", target.name}}});
}

// It goes down: out of the fight's turns, without the effects that disable
// (fxengine.pas RemoveIncapacitatingEffects). A bleeding one is the fight's
// dying; the dead and gone its dead; the rest, unconscious, comatose or
// poisoned, lie as the unconscious do.
void Session::ground_down(Fighter &f)
{
    auto &creature = f.ground->creature();
    remove_incapacitating(f.ground->effects);
    creature.enabled = 0;
    f.hit_points = creature.hit_points;
    const auto status = status_of(creature);
    f.condition = status == BuckRogersStatus::dying ? Condition::dying
                  : status == BuckRogersStatus::dead || status == BuckRogersStatus::gone
                      ? Condition::dead
                      : Condition::unconscious;
    end_actions(f);
}

// The round ends (combat.pas EndOfRound): sprinters and jetpack fliers are
// okay again; a minute passes for every effect; the clouds count down, and
// those that clear go (fxengine.pas CountDownAreaEffects); the bleeding
// count on, slipping into a coma past 9 (charhlp.pas ProcessDyingCharacters).
void Session::ground_end_round()
{
    for (auto &f : fighters_)
    {
        if (!f.ground)
        {
            continue;
        }
        auto &creature = f.ground->creature();
        const auto status = status_of(creature);
        if (status == BuckRogersStatus::sprinting || status == BuckRogersStatus::used_jetpack)
        {
            set_status(creature, BuckRogersStatus::okay);
        }
        age_ground_effects(f.ground->effects);
    }
    const auto before = ground_clouds_.size();
    std::erase_if(ground_clouds_,
                  [](GroundCloud &cloud)
                  {
                      return !cloud_lingers(cloud);
                  });
    if (ground_clouds_.size() != before)
    {
        log({"The air clears a little.", {}});
    }
    for (auto &f : fighters_)
    {
        if (f.ground && bleed(f.ground->creature(), f.ground->dying_count))
        {
            f.condition = Condition::unconscious;
            log({"{name} slips into a coma.", {{"name", f.name}}});
        }
    }
}

// The guards strike a creature that moves or lands near them (cbtacts.pas
// GuardersAttack): each enemy guard not helpless, with a ranged weapon or
// next to it, whose line reaches it within the weapon's range (any line, for
// one flying on a jetpack), strikes once and stops guarding.
void Session::ground_guards_strike(Fighter &mover)
{
    const bool flying = status_of(mover.ground->creature()) == BuckRogersStatus::using_jetpack;
    for (auto &guard : fighters_)
    {
        if (!mover.able() || !guard.ground || !guard.guarding || guard.side == mover.side ||
            !guard.able() || guard.helpless() || !sees(board_, guard, mover))
        {
            continue;
        }
        const int range = readied_weapon_range(guard.ground->gear.items, guard.ground->gear.types);
        if (range <= 1 && distance(guard, mover) > 1)
        {
            continue;
        }
        if (!flying && !ground_walk(guard.cell, mover.cell, range, false).reached)
        {
            continue;
        }
        note_attack(guard, mover);
        log({"{attacker} strikes from guard.", {{"attacker", guard.name}}});
        make_attack(guard, mover, false);
        guard.guarding = false;
    }
}

// After a step (combat.pas MoveMenu): the guards strike, and gas on its new
// square may leave it helpless (fxengine.pas ConsiderTerrainEffects), which
// ends its turn.
void Session::ground_after_step(Fighter &mover)
{
    ground_guards_strike(mover);
    auto &g = *mover.ground;
    if (mover.able() && ground_cloud_at(mover.cell, CloudMatter::gas))
    {
        if (const auto minutes = gassed(g.creature(), ground_numbers_of(mover).armor_flags,
                                        mover.helpless(), g.has(ground_effect::blind), random_);
            minutes && give_ground_effect(g.effects, {ground_effect::helpless, *minutes, 0}, false,
                                          false, false))
        {
            log({"{name} is helpless.", {{"name", mover.name}}});
        }
    }
    if (!mover.able() || mover.helpless())
    {
        end_actions(mover);
    }
}

// Intimidate (cbtacts.pas IntimidateTarget).
void Session::ground_intimidate(Fighter &f)
{
    if (!ground_skill_succeeds(f.ground->creature(), Skill::intimidate, random_))
    {
        log({"{name} fails to intimidate.", {{"name", f.name}}});
        return;
    }
    log({"{name} looks real menacing.", {{"name", f.name}}});
    for (auto &enemy : fighters_)
    {
        if (!enemy.ground || enemy.side == f.side || !enemy.able() || enemy.helpless() ||
            distance(f, enemy) > 1 || !sees(board_, f, enemy))
        {
            continue;
        }
        const bool saved = roll_ground_save(enemy.ground->creature(), ground_save::stun, 0,
                                            enemy.ground->has(ground_effect::blind), random_);
        const bool scared =
            give_ground_effect(enemy.ground->effects, scared_effect(), saved, false, false);
        log({scared ? "{name} is intimidated." : "{name} is unaffected.", {{"name", enemy.name}}});
    }
}

// AID's patients (viewchar.pas FindPatients, by the after-fight rules'
// find_patients): of the medic and its side's creatures on its square or next
// to it, those its skills can treat. The candidates, the medic first, as the
// after-fight rules take a team, and the patients among them.
struct GroundPatients
{
    std::vector<std::size_t> fighters;
    std::vector<TeamMember> team;
    std::vector<std::size_t> patients;
};

GroundPatients ground_patients(const std::vector<Fighter> &fighters, const Fighter &medic)
{
    GroundPatients found;
    found.fighters.push_back(0);
    found.team.push_back({});
    for (std::size_t index = 0; index < fighters.size(); ++index)
    {
        const auto &f = fighters[index];
        if (!f.ground || f.side != medic.side || f.condition == Condition::fled ||
            std::abs(f.cell.x - medic.cell.x) > 1 || std::abs(f.cell.y - medic.cell.y) > 1)
        {
            continue;
        }
        TeamMember member;
        member.character = f.ground->creature();
        // The after-fight rules know the team by its record's side: the
        // fight's side counts.
        member.character.side = f.side == Side::party ? 0 : 1;
        member.items = f.ground->gear.items;
        member.stunned =
            f.ground->has(ground_effect::stunned) || f.ground->has(ground_effect::dizzy);
        const std::size_t at = f.id == medic.id ? 0 : found.team.size();
        if (at == 0)
        {
            found.fighters[0] = index;
            found.team[0] = std::move(member);
        }
        else
        {
            found.fighters.push_back(index);
            found.team.push_back(std::move(member));
        }
    }
    found.patients = find_patients(0, found.team);
    return found;
}

bool Session::ground_has_patient(const Fighter &medic) const
{
    return !ground_patients(fighters_, medic).patients.empty();
}

// Aid (viewchar.pas TreatPatients, by the after-fight rules'
// treat_patients): each bleeding patient is bandaged, unconscious and no
// longer bleeding, and each stunned or dizzy one may be treated on a Treat
// Stun/Paralysis check.
void Session::ground_aid(Fighter &medic)
{
    auto found = ground_patients(fighters_, medic);
    for (const auto &done : treat_patients(0, found.team, found.patients, random_))
    {
        auto &patient = fighters_[found.fighters[done.patient]];
        auto &p = *patient.ground;
        if (done.kind == PatientTreatment::Kind::bandaged)
        {
            p.creature().status = found.team[done.patient].character.status;
            p.creature().enabled = 0;
            p.dying_count = 0;
            patient.condition = Condition::unconscious;
            log({"{name} is bandaged.", {{"name", patient.name}}});
            continue;
        }
        std::erase_if(p.effects,
                      [](const GroundEffect &e)
                      {
                          return e.code == ground_effect::dizzy || e.code == ground_effect::stunned;
                      });
        log({"{name} is treated.", {{"name", patient.name}}});
    }
}

// Jetpack (cbtacts.pas CombatUseJetpack): on a Use Jetpack check it takes
// off, to land where it chooses (ground_land); on a failure it lurches
// through eleven random squares near it, lands on the last it may stand on,
// meets the guards there and ends its turn.
void Session::ground_jetpack(Fighter &f)
{
    auto &g = *f.ground;
    if (ground_skill_succeeds(g.creature(), Skill::use_jetpack, random_))
    {
        set_command_status(g.creature(), BuckRogersStatus::using_jetpack);
        g.flying = true;
        log({"{name} takes off.", {{"name", f.name}}});
        return;
    }
    log({"{name} fails at use jetpack.", {{"name", f.name}}});
    set_command_status(g.creature(), BuckRogersStatus::using_jetpack);
    f.half_moves = 0;
    Cell at = f.cell, landing = f.cell;
    for (int step = 0; step < lurch_steps; ++step)
    {
        do
        {
            at = lurch_step(at, random_);
        } while (!board_.contains(at));
        auto moved = f;
        moved.cell = at;
        if (!rules::blocks_movement(board_.at(at)) && cell_free(moved) && at != f.cell)
        {
            landing = at;
        }
    }
    f.cell = landing;
    ground_guards_strike(f);
    if (f.able())
    {
        set_command_status(g.creature(), BuckRogersStatus::used_jetpack);
    }
    end_actions(f);
}

// A jetpack flight lands: the guards strike as it leaves and as it lands; it
// may still attack, but moves no more this round.
void Session::ground_land(Fighter &f, Cell landing)
{
    auto &g = *f.ground;
    g.flying = false;
    ground_guards_strike(f);
    f.half_moves = 0;
    if (!f.able())
    {
        return;
    }
    f.facing_left = landing.x < f.cell.x || (landing.x == f.cell.x && f.facing_left);
    f.cell = landing;
    ground_guards_strike(f);
    if (f.able())
    {
        set_command_status(g.creature(), BuckRogersStatus::used_jetpack);
    }
}

// The commands a Buck Rogers creature has (combat.pas CombatMenu): Move,
// Target (a blow or a shot at an enemy, or a launch at a square), the new
// commands (ground_menu), Wait, Quick, End and fleeing off the field; while
// it flies, only landing; and only Quick for a companion the computer plays.
std::vector<rules::Command> Session::ground_commands(const Fighter &actor) const
{
    std::vector<rules::Command> commands;
    const auto add = [&](std::string verb, EntityId target = 0, Cell destination = {})
    {
        rules::Command command;
        command.revision = revision_;
        command.actor = actor.id;
        command.verb = std::move(verb);
        command.target = target;
        command.destination = destination;
        command.intent = target != 0 ? rules::Intent::hostile : rules::Intent{};
        commands.push_back(std::move(command));
    };
    const auto &g = *actor.ground;
    const auto &creature = g.creature();
    const auto &items = g.gear.items;
    const auto &types = g.gear.types;
    if (g.computer_plays)
    {
        add("quick");
        return commands;
    }
    const auto open = [this, &actor](Cell cell)
    {
        auto moved = actor;
        moved.cell = cell;
        return board_.contains(cell) && !rules::blocks_movement(board_.at(cell)) &&
               cell_free(moved);
    };
    if (g.flying)
    {
        for (int y = 0; y < board_.height; ++y)
        {
            for (int x = 0; x < board_.width; ++x)
            {
                if (open({x, y}))
                {
                    add("land", 0, {x, y});
                }
            }
        }
        return commands;
    }
    for (const auto &cell : movement_reach(actor.id))
    {
        add("move", 0, cell);
    }
    const auto weapon = readied_weapon(items, types);
    if (actor.attacks() > 0 && may_strike(creature, items, types))
    {
        const bool area = weapon && projectile_kind(items[*weapon]) != 0;
        if (area && items[*weapon].quantity > 0)
        {
            const int range = readied_weapon_range(items, types);
            for (int y = 0; y < board_.height; ++y)
            {
                for (int x = 0; x < board_.width; ++x)
                {
                    const Cell cell{x, y};
                    const int dx = std::abs(x - actor.cell.x), dy = std::abs(y - actor.cell.y);
                    if (cell != actor.cell &&
                        (2 * std::max(dx, dy) + std::min(dx, dy)) / 2 <= range &&
                        !rules::blocks_movement(board_.at(cell)) &&
                        ground_walk(actor.cell, cell, 255, false).reached)
                    {
                        add("launch", 0, cell);
                    }
                }
            }
        }
        for (const auto &f : fighters_)
        {
            if (!area && f.side != actor.side && f.able() && ground_reaches(actor, f))
            {
                add(distance(actor, f) > 1 ? "ranged" : "melee", f.id);
            }
        }
    }
    const bool enemy_next_to =
        std::ranges::any_of(fighters_,
                            [&actor](const Fighter &f)
                            {
                                return f.side != actor.side && f.able() && distance(actor, f) <= 1;
                            });
    const bool patient = ground_has_patient(actor);
    const auto menu = ground_menu(creature, items, types,
                                  {actor.half_moves > 0, enemy_next_to, patient, outdoors_});
    for (const auto &[offered, verb] :
         {std::pair{menu.intimidate, "intimidate"}, std::pair{menu.aid, "aid"},
          std::pair{menu.sprint, "sprint"}, std::pair{menu.dodge, "dodge"},
          std::pair{menu.jetpack, "jetpack"}, std::pair{menu.guard, "guard"}})
    {
        if (offered)
        {
            add(verb);
        }
    }
    for (std::size_t at = 0; at < items.size(); ++at)
    {
        if (may_ready(items, types, at))
        {
            add(readied(items[at]) ? "unready" : "ready");
            commands.back().item = static_cast<unsigned>(at);
            commands.back().label = item_name(items[at], item_words_);
        }
    }
    add("delay");
    add("quick");
    if (actor.half_moves >= 2)
    {
        for (const auto step : off_field_steps(actor))
        {
            add("flee", 0, step);
        }
    }
    add("end");
    return commands;
}

// The Buck Rogers commands; false for those the fight plays as every game's
// (move, delay, quick, flee, end).
bool Session::ground_submit(Fighter &actor, const rules::Command &command)
{
    auto &creature = actor.ground->creature();
    const auto &verb = command.verb;
    if (verb == "melee" || verb == "ranged")
    {
        auto &target = *find(command.target);
        note_attack(actor, target);
        make_attack(actor, target, false);
    }
    else if (verb == "launch")
    {
        ground_launch(actor, command.destination);
    }
    else if (verb == "intimidate")
    {
        ground_intimidate(actor);
        end_actions(actor);
    }
    else if (verb == "aid")
    {
        ground_aid(actor);
        end_actions(actor);
    }
    else if (verb == "sprint")
    {
        if (status_of(creature) != BuckRogersStatus::sprinting)
        {
            set_command_status(creature, BuckRogersStatus::sprinting);
            actor.half_moves = sprint_movement(actor.half_moves);
        }
        log({"{name} sprints.", {{"name", actor.name}}});
    }
    else if (verb == "dodge")
    {
        set_command_status(creature, BuckRogersStatus::dodging);
        end_actions(actor);
        ground_refresh(actor);
        log({"{name} dodges.", {{"name", actor.name}}});
    }
    else if (verb == "jetpack")
    {
        ground_jetpack(actor);
    }
    else if (verb == "land")
    {
        ground_land(actor, command.destination);
    }
    else if (verb == "ready" || verb == "unready")
    {
        // Readying an item (viewchar.pas ToggleReadyItem): a heavy one ends
        // the turn; the attacks left follow the weapon readied, and only fall
        // once it has attacked (cbtacts.pas RecalcAttacksLeft).
        auto &g = *actor.ground;
        auto &item = g.gear.items[command.item];
        item.flags ^= buck_rogers::Item::readied;
        if (verb == "ready" && readying_ends_turn(*type_of(item, g.gear.types)))
        {
            end_actions(actor);
        }
        const int first =
            ground_attacks(creature, g.gear.items, g.gear.types, static_cast<int>(round_) - 1,
                           g.has(ground_effect::dizzy))[0];
        auto &left = actor.attacks_left[0];
        left = g.attacked ? std::min(left, first) : first;
        ground_refresh(actor);
    }
    else if (verb == "guard")
    {
        end_actions(actor);
        if (may_guard_with(actor.ground->gear.items, actor.ground->gear.types))
        {
            actor.guarding = true;
            log({"{name} guards.", {{"name", actor.name}}});
        }
    }
    else
    {
        return false;
    }
    return true;
}

// A Buck Rogers fight's own state in its checkpoint: zero gravity, the
// sides' tactics, the clouds, and each creature's record and gear, effects
// and counts.
void Session::write_ground(Writer &out) const
{
    out(zero_gravity_, tactics_[0], tactics_[1], ground_clouds_.size());
    for (const auto &cloud : ground_clouds_)
    {
        out(cloud.center.x, cloud.center.y, cloud.rounds_left, static_cast<unsigned>(cloud.matter));
        for (const bool covers : cloud.covers)
        {
            out(covers);
        }
    }
    for (const auto &f : fighters_)
    {
        out(f.ground.has_value());
        if (!f.ground)
        {
            continue;
        }
        const auto &g = *f.ground;
        out(encode_ground_record(g.gear), g.effects.size());
        for (const auto &e : g.effects)
        {
            out(e.code, e.minutes, e.strength);
        }
        out(g.dying_count, g.tie_break, g.to_hit, g.armor_class_bonus, g.flying, g.computer_plays,
            g.attacked);
    }
}

void Session::read_ground(Reader &in)
{
    const auto invalid = []
    {
        throw std::runtime_error("Invalid original-rules combat checkpoint");
    };
    zero_gravity_ = in.as<bool>();
    tactics_ = {in.as<bool>(), in.as<bool>()};
    const auto clouds = in.as<std::size_t>();
    if (clouds > 64)
    {
        invalid();
    }
    for (std::size_t n = 0; n < clouds; ++n)
    {
        GroundCloud cloud;
        cloud.center = {in.as<int>(), in.as<int>()};
        cloud.rounds_left = in.as<int>();
        const auto matter = in.as<unsigned>();
        if (matter > static_cast<unsigned>(CloudMatter::mist))
        {
            invalid();
        }
        cloud.matter = static_cast<CloudMatter>(matter);
        for (auto &covers : cloud.covers)
        {
            covers = in.as<bool>();
        }
        ground_clouds_.push_back(cloud);
    }
    for (auto &f : fighters_)
    {
        if (!in.as<bool>())
        {
            continue;
        }
        auto gear = decode_ground_record(in.text());
        const auto effects = in.as<std::size_t>();
        if (!gear || effects > 64)
        {
            invalid();
        }
        GroundFighter g{std::move(*gear)};
        for (std::size_t n = 0; n < effects; ++n)
        {
            const int code = in.as<int>(), minutes = in.as<int>();
            g.effects.push_back({code, minutes, in.as<int>()});
        }
        g.dying_count = in.as<int>();
        g.tie_break = in.as<int>();
        g.to_hit = in.as<int>();
        g.armor_class_bonus = in.as<int>();
        g.flying = in.as<bool>();
        g.computer_plays = in.as<bool>();
        g.attacked = in.as<bool>();
        f.ground = std::move(g);
    }
}

std::string Session::save() const
{
    Writer out;
    const bool dark_queen = game_of(identity_) == GoldBoxGame::dark_queen_of_krynn;
    const bool native_calendar =
        dark_queen || game_of(identity_) == GoldBoxGame::death_knights_of_krynn;
    out(std::string(dark_queen ? dark_queen_checkpoint_format : checkpoint_format), board_.width,
        board_.height);
    if (native_calendar)
    {
        out(starting_time_minutes_);
    }
    if (dark_queen)
    {
        out(underwater_);
    }
    for (const auto tile : board_.terrain)
    {
        out(static_cast<unsigned>(tile));
    }
    out(board_.plants.size());
    for (const bool plant : board_.plants)
    {
        out(plant);
    }
    out(board_.blocking.size());
    for (std::size_t n = 0; n < board_.blocking.size(); ++n)
    {
        out(static_cast<unsigned>(board_.clearance[n]), static_cast<unsigned>(board_.blocking[n]));
    }
    out(board_.move_cost.size());
    for (const auto cost : board_.move_cost)
    {
        out(static_cast<unsigned>(cost));
    }
    out(random_.value, round_, revision_, actor_, static_cast<unsigned>(outcome_),
        morale_threshold_, outdoors_, escape_ends_fight_, escape_side_.value_or(4), party_facing_,
        party_move_modifier_, crystals_, crystal_.value_or(3), hostile_hit_points_percent_,
        turning_tries_, turning_penalty_, fight_end_round_, round_limit_reached_,
        romance_.has_value());
    if (romance_)
    {
        out(romance_->member, romance_->npc, romance_->member_love, romance_->npc_love, lover_,
            beloved_);
    }
    out(clouds_.size());
    for (const auto &cloud : clouds_)
    {
        out(cloud.caster, cloud.index, cloud.corner.x, cloud.corner.y,
            static_cast<unsigned>(cloud.kind));
        for (const bool covers : cloud.covers)
        {
            out(covers);
        }
    }
    out(blasts_.size());
    for (const auto &blast : blasts_)
    {
        out(blast.caster, blast.index, blast.cell.x, blast.cell.y, blast.level);
    }
    out(turn_order_.size());
    for (const auto id : turn_order_)
    {
        out(id);
    }
    out(fighters_.size());
    for (const auto &f : fighters_)
    {
        out(f.id, f.name, f.definition, static_cast<unsigned>(f.side), f.cell.x, f.cell.y,
            f.footprint.width, f.footprint.height, f.facing_left, f.numbers.encode(), f.hit_points,
            static_cast<unsigned>(f.condition), f.overkill, f.initiative, f.half_moves,
            f.attacks_left[0], f.attacks_left[1], f.surprised, f.attack_phase, f.sweep_budget,
            f.direction, f.flank_count, f.flank_spread, f.guarding, f.fleeing, f.turned,
            f.tried_turning, f.turn_attempts, f.cast_allowed, f.pending_spell, f.aiming,
            f.picks_left, f.blast_delay, static_cast<unsigned>(f.own_side), f.item_use_allowed,
            f.using_item, f.stance, f.lock, f.arrival_round,
            f.before_animation ? f.before_animation->encode() : std::string(), f.picked.size());
        for (const auto id : f.picked)
        {
            out(id);
        }
        out(f.shot_item, f.item_ids.size());
        for (const auto &[ordinal, id] : f.item_ids)
        {
            out(ordinal, id);
        }
        out(f.record ? encode_fight_record(*f.record) : std::string(), f.weighs_unreadied,
            f.readied_changed, f.held.size());
        for (const auto &held : f.held)
        {
            const auto &item = held.carried.item;
            out(por::original_item_key(item), item.readied_raw, item.stack_size, held.inventory_id,
                held.name, held.start_count, held.gone, held.readied_order, held.conjured);
        }
        out(f.worked_from.size());
        for (const int at : f.worked_from)
        {
            out(at);
        }
    }
    out(log_.size());
    for (const auto &entry : log_)
    {
        out(entry.message.source, entry.message.arguments.size());
        for (const auto &argument : entry.message.arguments)
        {
            out(argument.name, argument.value, argument.translate);
        }
    }
    // The Buck Rogers games' own state follows; no other game's fight has it.
    if (buck_rogers())
    {
        write_ground(out);
    }
    return out.text();
}

namespace
{
// A fighter's record and held items as a checkpoint keeps them, their types
// from the game's.
void read_gear(Reader &in, Fighter &f, const Content &content)
{
    const auto invalid = []
    {
        throw std::runtime_error("Invalid original-rules combat checkpoint");
    };
    if (const auto record = in.text(); !record.empty())
    {
        f.record = decode_fight_record(record);
        if (!f.record)
        {
            invalid();
        }
    }
    f.weighs_unreadied = in.as<bool>();
    f.readied_changed = in.as<bool>();
    const auto count = in.as<std::size_t>();
    if (count > 255)
    {
        invalid();
    }
    for (std::size_t n = 0; n < count; ++n)
    {
        auto item = por::item_of_original_key(in.text());
        if (!item || item->type >= content.item_types.size())
        {
            invalid();
        }
        Held held{{*item, content.item_types[item->type]}};
        auto &raw = held.carried.item;
        raw.readied_raw = in.as<std::uint8_t>();
        raw.stack_size = in.as<std::uint8_t>();
        raw.raw[52] = raw.readied_raw;
        raw.raw[57] = raw.stack_size;
        held.inventory_id = in.as<std::uint64_t>();
        held.name = in.text();
        held.start_count = in.as<int>();
        held.gone = in.as<bool>();
        held.readied_order = in.as<int>();
        held.conjured = in.as<bool>();
        f.held.push_back(std::move(held));
    }
    const auto worked = in.as<std::size_t>();
    for (std::size_t n = 0; n < worked; ++n)
    {
        const auto at = in.as<int>();
        if (at < 0 || static_cast<std::size_t>(at) >= f.held.size())
        {
            invalid();
        }
        f.worked_from.push_back(at);
    }
}
} // namespace

std::unique_ptr<Session> Session::restored(rules::Identity identity, std::string_view checkpoint,
                                           const Content &content)
{
    Reader in(checkpoint);
    const auto format = in.text();
    const bool has_costs = format == checkpoint_format || format == dark_queen_checkpoint_format;
    const bool dark_queen =
        format == dark_queen_checkpoint_format || format == costless_dark_queen_checkpoint_format;
    if (!has_costs && format != costless_checkpoint_format &&
        format != costless_death_knights_checkpoint_format && !dark_queen)
    {
        throw std::runtime_error(rules::older_save_message);
    }
    const bool native_calendar =
        dark_queen || format == costless_death_knights_checkpoint_format ||
        (format == checkpoint_format && game_of(identity) == GoldBoxGame::death_knights_of_krynn);
    std::unique_ptr<Session> session(new Session);
    session->identity_ = std::move(identity);
    session->item_types_ = content.item_types;
    auto &board = session->board_;
    board.width = in.as<int>();
    board.height = in.as<int>();
    if (native_calendar)
    {
        session->starting_time_minutes_ = in.as<std::uint64_t>();
    }
    if (dark_queen)
    {
        session->underwater_ = in.as<bool>();
    }
    if (board.width < 2 || board.height < 2 || board.width > 64 || board.height > 64)
    {
        throw std::runtime_error("Invalid original-rules combat checkpoint");
    }
    board.terrain.resize(static_cast<std::size_t>(board.width * board.height));
    for (auto &tile : board.terrain)
    {
        tile = in.as<rules::Terrain>();
    }
    const auto plants = in.as<std::size_t>();
    if (plants != 0 && plants != board.terrain.size())
    {
        throw std::runtime_error("Invalid original-rules combat checkpoint");
    }
    for (std::size_t n = 0; n < plants; ++n)
    {
        board.plants.push_back(in.as<bool>());
    }
    const auto heights = in.as<std::size_t>();
    if (heights != 0 && heights != board.terrain.size())
    {
        throw std::runtime_error("Invalid original-rules combat checkpoint");
    }
    for (std::size_t n = 0; n < heights; ++n)
    {
        board.clearance.push_back(static_cast<std::uint8_t>(in.as<unsigned>()));
        board.blocking.push_back(static_cast<std::uint8_t>(in.as<unsigned>()));
    }
    const auto costs = has_costs ? in.as<std::size_t>() : 0;
    if (costs != 0 && costs != board.terrain.size())
    {
        throw std::runtime_error("Invalid original-rules combat checkpoint");
    }
    for (std::size_t n = 0; n < costs; ++n)
    {
        const auto cost = in.as<std::uint64_t>();
        if (cost == 0 || cost > std::numeric_limits<std::uint8_t>::max())
        {
            throw std::runtime_error("Invalid original-rules combat checkpoint");
        }
        board.move_cost.push_back(static_cast<std::uint8_t>(cost));
    }
    rules::validate_battlefield(board);
    session->random_.value = in.as<std::uint64_t>();
    session->round_ = in.as<unsigned>();
    session->revision_ = in.as<std::uint64_t>();
    session->actor_ = in.as<EntityId>();
    session->outcome_ = in.as<rules::Outcome>();
    session->morale_threshold_ = in.as<unsigned>();
    session->outdoors_ = in.as<bool>();
    session->escape_ends_fight_ = in.as<bool>();
    if (const auto side = in.as<unsigned>(); side < 4)
    {
        session->escape_side_ = side;
    }
    session->party_facing_ = in.as<int>();
    session->party_move_modifier_ = in.as<int>();
    session->crystals_ = in.as<bool>();
    if (const auto colour = in.as<unsigned>(); colour < 3)
    {
        session->crystal_ = colour;
    }
    session->hostile_hit_points_percent_ = in.as<int>();
    session->turning_tries_ = in.as<int>();
    session->turning_penalty_ = in.as<int>();
    session->fight_end_round_ = in.as<unsigned>();
    session->round_limit_reached_ = in.as<bool>();
    if (in.as<bool>())
    {
        rules::Romance romance;
        romance.member = in.text();
        romance.npc = in.text();
        romance.member_love = in.as<int>();
        romance.npc_love = in.as<int>();
        session->romance_ = std::move(romance);
        session->lover_ = in.as<EntityId>();
        session->beloved_ = in.as<EntityId>();
    }
    const auto clouds = in.as<std::size_t>();
    if (clouds > 64)
    {
        throw std::runtime_error("Invalid original-rules combat checkpoint");
    }
    for (std::size_t n = 0; n < clouds; ++n)
    {
        Cloud cloud;
        cloud.caster = in.as<EntityId>();
        cloud.index = in.as<int>();
        cloud.corner = {in.as<int>(), in.as<int>()};
        const auto kind = in.as<unsigned>();
        if (kind > static_cast<unsigned>(CloudKind::acid))
        {
            throw std::runtime_error("Invalid original-rules combat checkpoint");
        }
        cloud.kind = static_cast<CloudKind>(kind);
        for (auto &covers : cloud.covers)
        {
            covers = in.as<bool>();
        }
        session->clouds_.push_back(cloud);
    }
    const auto blasts = in.as<std::size_t>();
    if (blasts > 64)
    {
        throw std::runtime_error("Invalid original-rules combat checkpoint");
    }
    for (std::size_t n = 0; n < blasts; ++n)
    {
        DelayedBlast blast;
        blast.caster = in.as<EntityId>();
        blast.index = in.as<int>();
        blast.cell = {in.as<int>(), in.as<int>()};
        blast.level = in.as<int>();
        session->blasts_.push_back(blast);
    }
    const auto ordered = in.as<std::size_t>();
    if (ordered > 256)
    {
        throw std::runtime_error("Invalid original-rules combat checkpoint");
    }
    for (std::size_t n = 0; n < ordered; ++n)
    {
        session->turn_order_.push_back(in.as<EntityId>());
    }
    if (session->morale_threshold_ > 100 || session->hostile_hit_points_percent_ < 0 ||
        session->hostile_hit_points_percent_ > 100)
    {
        throw std::runtime_error("Invalid original-rules combat checkpoint");
    }
    const auto count = in.as<std::size_t>();
    if (count > 256)
    {
        throw std::runtime_error("Invalid original-rules combat checkpoint");
    }
    for (std::size_t fighter_index = 0; fighter_index < count; ++fighter_index)
    {
        Fighter f;
        f.id = in.as<EntityId>();
        f.name = in.text();
        f.definition = in.text();
        f.side = in.as<Side>();
        f.cell = {in.as<int>(), in.as<int>()};
        f.footprint = {in.as<int>(), in.as<int>()};
        f.facing_left = in.as<bool>();
        const auto numbers = Combatant::decode(in.text());
        if (!numbers)
        {
            throw std::runtime_error("Invalid original-rules combat checkpoint");
        }
        f.numbers = *numbers;
        f.hit_points = in.as<int>();
        f.condition = in.as<Condition>();
        f.overkill = in.as<int>();
        f.initiative = in.as<int>();
        f.half_moves = in.as<int>();
        f.attacks_left = {in.as<int>(), in.as<int>()};
        f.surprised = in.as<bool>();
        f.attack_phase = in.as<int>();
        f.sweep_budget = in.as<int>();
        f.direction = in.as<int>();
        f.flank_count = in.as<int>();
        f.flank_spread = in.as<int>();
        f.guarding = in.as<bool>();
        f.fleeing = in.as<bool>();
        f.turned = in.as<bool>();
        f.tried_turning = in.as<bool>();
        f.turn_attempts = in.as<int>();
        f.cast_allowed = in.as<bool>();
        f.pending_spell = in.as<int>();
        f.aiming = in.as<int>();
        f.picks_left = in.as<int>();
        f.blast_delay = in.as<int>();
        f.own_side = in.as<Side>();
        f.item_use_allowed = in.as<bool>();
        f.using_item = in.as<int>();
        f.stance = in.as<int>();
        f.lock = in.as<EntityId>();
        f.arrival_round = in.as<int>();
        if (const auto before = in.text(); !before.empty())
        {
            f.before_animation = Combatant::decode(before);
            if (!f.before_animation)
            {
                throw std::runtime_error("Invalid original-rules combat checkpoint");
            }
        }
        const auto picked = in.as<std::size_t>();
        if (picked > 8 || (f.pending_spell && !spell(f.pending_spell)) ||
            (f.aiming && !spell(f.aiming)))
        {
            throw std::runtime_error("Invalid original-rules combat checkpoint");
        }
        for (std::size_t n = 0; n < picked; ++n)
        {
            f.picked.push_back(in.as<EntityId>());
        }
        f.shot_item = in.as<std::uint64_t>();
        const auto counted = in.as<std::size_t>();
        if (counted > 255)
        {
            throw std::runtime_error("Invalid original-rules combat checkpoint");
        }
        for (std::size_t n = 0; n < counted; ++n)
        {
            const auto ordinal = in.as<int>();
            f.item_ids.emplace_back(ordinal, in.as<std::uint64_t>());
        }
        read_gear(in, f, content);
        if (f.condition > Condition::stoned || f.attack_phase < 0 || f.attack_phase > 1 ||
            f.direction < 0 || f.direction > 7)
        {
            throw std::runtime_error("Invalid original-rules combat checkpoint");
        }
        session->fighters_.push_back(std::move(f));
    }
    const auto entries = in.as<std::size_t>();
    for (std::size_t n = 0; n < entries; ++n)
    {
        Message message;
        message.source = in.text();
        const auto arguments = in.as<std::size_t>();
        for (std::size_t a = 0; a < arguments; ++a)
        {
            rules::MessageArgument argument;
            argument.name = in.text();
            argument.value = in.text();
            argument.translate = in.as<bool>();
            message.arguments.push_back(std::move(argument));
        }
        session->log_.push_back({english(message), std::move(message)});
    }
    if (session->buck_rogers())
    {
        session->read_ground(in);
    }
    const bool order_known = std::ranges::all_of(session->turn_order_,
                                                 [&session](EntityId id)
                                                 {
                                                     return session->find(id) != nullptr;
                                                 });
    if (!in.finished() || !order_known)
    {
        throw std::runtime_error("Invalid original-rules combat checkpoint");
    }
    return session;
}

} // namespace

namespace
{
// The counts a party member's inventory keeps, which its profile's items
// leave out: its usable items' charges or stacks, and how many of what its
// missile shoots it has.
void take_counts(Fighter &f, const std::vector<rules::CarriedEquipment> &inventory,
                 const Content &content)
{
    namespace bit = por::item_property;
    const auto ammunition = ammunition_types(content.game);
    for (const auto &carried : inventory)
    {
        const auto record = por::item_of_original_key(carried.definition);
        if (carried.equipment_index < 0 || !record)
        {
            continue;
        }
        const int count = static_cast<int>(carried.quantity);
        for (auto &usable : f.numbers.items)
        {
            if (usable.ordinal != carried.equipment_index)
            {
                continue;
            }
            f.item_ids.emplace_back(usable.ordinal, carried.inventory_id);
            if (usable.charges > 0)
            {
                usable.charges = count;
            }
            else
            {
                usable.quantity = count > 1 ? count : 0;
            }
        }
        auto &missile = f.numbers.missile;
        if (!missile || f.shot_item || f.numbers.weapon_type < 0 ||
            static_cast<std::size_t>(f.numbers.weapon_type) >= content.item_types.size())
        {
            continue;
        }
        const auto &launcher = content.item_types[static_cast<std::size_t>(f.numbers.weapon_type)];
        const bool shot = missile->thrown ? record->type == f.numbers.weapon_type
                                          : (launcher.has(bit::fires_arrows) &&
                                             record->type == ammunition.arrows) ||
                                                (launcher.has(bit::fires_quarrels) &&
                                                 ammunition.shot_as_quarrel(record->type));
        if (shot)
        {
            f.shot_item = carried.inventory_id;
            missile->quantity = count > 1 ? count : 0;
        }
    }
}

// What a party member readies from in the fight: its record and the items it
// carries, each with its count, the readied in the order its profile has
// them. One with a readied item of no original record readies nothing.
void take_gear(Fighter &f, const rules::Participant &p, const Content &content)
{
    auto record = decode_fight_record(p.character_record);
    if (!record)
    {
        return;
    }
    std::vector<Held> held;
    for (const auto &carried : p.inventory)
    {
        const auto item = por::item_of_original_key(carried.definition);
        if (!item || item->type >= content.item_types.size())
        {
            if (carried.equipment_index >= 0)
            {
                return;
            }
            continue;
        }
        Held h{{*item, content.item_types[item->type]}, carried.inventory_id, carried.name};
        // A hammer an earlier fight conjured goes as its spell does.
        h.conjured = is_spiritual_hammer(*item, content.game);
        const auto count = static_cast<int>(carried.quantity);
        if (count > 1 && held_count(h.carried) != count)
        {
            h.carried.item.stack_size = static_cast<std::uint8_t>(count);
            h.carried.item.raw[57] = h.carried.item.stack_size;
        }
        set_readied(h, carried.equipment_index >= 0);
        h.readied_order = carried.equipment_index;
        h.start_count = held_count(h.carried);
        held.push_back(std::move(h));
    }
    std::vector<int> order;
    for (std::size_t at = 0; at < held.size(); ++at)
    {
        if (held[at].readied_order >= 0)
        {
            order.push_back(static_cast<int>(at));
        }
    }
    std::ranges::sort(order,
                      [&held](int a, int b)
                      {
                          return held[static_cast<std::size_t>(a)].readied_order <
                                 held[static_cast<std::size_t>(b)].readied_order;
                      });
    f.record = std::move(*record);
    f.held = std::move(held);
    f.worked_from = std::move(order);
}

// What a monster readies from: its record and all it carries, as its
// numbers were worked out from them.
void take_monster_gear(Fighter &f, const MonsterGear &gear)
{
    f.record = gear.record;
    f.weighs_unreadied = true;
    for (std::size_t at = 0; at < gear.carried.size(); ++at)
    {
        Held h{gear.carried[at]};
        h.readied_order = h.carried.item.readied() ? static_cast<int>(at) : -1;
        h.start_count = held_count(h.carried);
        f.held.push_back(std::move(h));
        f.worked_from.push_back(static_cast<int>(at));
    }
}

// What the difficulty does to a creature as its fight begins
// (rules::OriginalStrength). A thrown weapon's empty-handed THAC0 moves with
// its THAC0, for when the last one is thrown.
void strengthen(Combatant &numbers, const rules::OriginalStrength &strength)
{
    if (strength.hit_points_percent != 100)
    {
        const auto scaled = [percent = static_cast<int>(strength.hit_points_percent)](int points)
        {
            return std::max(points * percent / 100, 1);
        };
        numbers.hit_points = scaled(numbers.hit_points);
        numbers.max_hit_points = scaled(numbers.max_hit_points);
    }
    numbers.thac0 -= strength.to_hit;
    if (numbers.missile)
    {
        numbers.missile->empty_handed_thac0 -= strength.to_hit;
    }
    numbers.armor_class -= strength.armor_class_bonus;
    numbers.rear_armor_class -= strength.armor_class_bonus;
}
} // namespace

std::unique_ptr<rules::CombatSession> create_session(rules::Identity identity,
                                                     rules::Encounter encounter, std::uint64_t seed,
                                                     const Content &content, bool gazes_played)
{
    std::vector<Fighter> fighters;
    for (auto &p : encounter.participants)
    {
        Fighter f;
        f.id = p.id;
        f.name = std::move(p.name);
        f.definition = p.definition;
        f.side = p.side;
        f.cell = p.cell;
        f.footprint = p.footprint;
        f.facing_left = p.facing_left;
        f.starting_facing = p.facing;
        f.surprised = p.surprised;
        if (is_buck_rogers(game_of(identity)))
        {
            f.ground = ground_fighter(p, f.name);
            f.hit_points = f.ground->creature().hit_points;
            f.numbers = ground_combatant(*f.ground,
                                         ground_numbers(f.ground->creature(), f.ground->gear.items,
                                                        f.ground->gear.types, false));
            const auto status = status_of(f.ground->creature());
            f.condition = status == BuckRogersStatus::dead || status == BuckRogersStatus::gone
                              ? Condition::dead
                          : f.hit_points <= 0 || !can_act(status) ? Condition::unconscious
                                                                  : Condition::okay;
            fighters.push_back(std::move(f));
            continue;
        }
        if (!p.character_profile.empty())
        {
            const auto numbers = Combatant::decode(p.character_profile);
            if (!numbers)
            {
                throw std::runtime_error("A party member has no original-rules profile: " + f.name);
            }
            f.numbers = *numbers;
            take_counts(f, p.inventory, content);
            take_gear(f, p, content);
            // A member's last fight left it the spells it had not cast, which
            // it keeps until a day's rest memorizes them again, and the
            // conditions that outlast fights and rests (lasting_condition).
            if (p.state && !p.state->resources.empty())
            {
                const auto carried = carried_state(p.state->resources, content.game);
                if (carried.last_fight)
                {
                    f.numbers.spells = carried.last_fight->spells;
                }
                for (const auto &kept : carried.conditions)
                {
                    if (!f.has(kept.code))
                    {
                        f.numbers.effects.push_back(kept);
                    }
                }
            }
        }
        else if (const auto found = content.bestiary.find(p.definition);
                 found != content.bestiary.end())
        {
            f.numbers = found->second;
            if (const auto gear = content.gear.find(p.definition); gear != content.gear.end())
            {
                take_monster_gear(f, gear->second);
            }
        }
        else
        {
            throw std::runtime_error("No original-rules record for " + p.definition);
        }
        if (content.game == GoldBoxGame::dark_queen_of_krynn)
        {
            for (const auto &effect : f.numbers.effects)
            {
                if (dark_queen_effect::unimplemented(effect.code))
                {
                    throw std::runtime_error(
                        "Dark Queen effect " +
                        std::to_string(effect.code - dark_queen_effect::offset) +
                        " is not implemented yet");
                }
            }
        }
        strengthen(f.numbers, p.original_strength);
        f.hit_points = p.state ? p.state->hit_points : f.numbers.hit_points;
        if (p.state && p.state->dead)
        {
            f.condition = Condition::dead;
        }
        else if (p.state && p.state->description == stoned_description)
        {
            f.condition = Condition::stoned;
            f.hit_points = 0;
        }
        else if (f.hit_points <= 0)
        {
            f.condition = Condition::unconscious;
            f.hit_points = 0;
        }
        else if (p.arrival_round > 0)
        {
            f.condition = Condition::fled;
            f.arrival_round = static_cast<int>(p.arrival_round);
        }
        else if (game_of(identity) == GoldBoxGame::champions_of_krynn && f.record &&
                 f.record->status == 9)
        {
            // The native REMOVED status is a gated reserve. Sir Lebaum's
            // four ghasts wait off the arena until his power brings them in.
            f.condition = Condition::fled;
            f.arrival_round = gated_reserve_round;
        }
        fighters.push_back(std::move(f));
    }
    // In a "no magic" fight every fighter holds the bar, so it lasts as the
    // fight's effects are kept.
    if (encounter.no_magic)
    {
        for (auto &f : fighters)
        {
            f.numbers.effects.push_back({effect::no_magic, 0, 0, false});
        }
    }
    // A party NPC without a morale of its own takes the encounter's (combatst.pas,
    // InitCreatureCombatData).
    const auto threshold = std::min(encounter.morale, 100U);
    for (auto &f : fighters)
    {
        const auto own = f.numbers.morale & 0x7F;
        if (f.side == Side::party && f.numbers.morale >= non_player && (own == 0 || own > 102))
        {
            // The original stored the threshold unhalved, so an ally's morale
            // was twice the encounter's and failed every check from 52 up (an
            // obvious bug, fixed).
            f.numbers.morale = non_player + threshold / 2;
        }
    }
    auto session = std::make_unique<Session>(
        std::move(identity), std::move(encounter.battlefield), std::move(fighters), seed, threshold,
        encounter.outdoors, encounter.escape_ends_fight, encounter.party_facing,
        encounter.party_move_modifier, encounter.crystals, encounter.frightening_surprise,
        encounter.turning_tries, encounter.turning_penalty, encounter.time_minutes,
        encounter.zero_gravity,
        content.game == GoldBoxGame::dark_queen_of_krynn && encounter.underwater);
    session->know_item_types(content.item_types);
    session->know_item_words(content.buck_rogers_item_words);
    session->play_gazes(gazes_played);
    session->move_as_the_original(content.original_movement);
    session->limit_party_area_spells(content.party_spell_limits);
    session->pair_lovers(std::move(encounter.romance));
    return session;
}

std::unique_ptr<rules::CombatSession> restore_session(rules::Identity identity,
                                                      std::string_view checkpoint,
                                                      const Content &content, bool gazes_played)
{
    auto session = Session::restored(std::move(identity), checkpoint, content);
    session->know_item_words(content.buck_rogers_item_words);
    session->play_gazes(gazes_played);
    session->move_as_the_original(content.original_movement);
    session->limit_party_area_spells(content.party_spell_limits);
    return session;
}

} // namespace opengold::original::detail
