#ifndef OPENGOLD_SRD5_STATUS_EFFECTS_H
#define OPENGOLD_SRD5_STATUS_EFFECTS_H

#include "character_class.h"
#include "opengold/rules.h"
#include <array>
#include <functional>
#include <iosfwd>
#include <string_view>

namespace opengold::srd5::detail
{
using rules::Ability;
enum class EffectKind : unsigned
{
    blindness = 1,
    ray_of_frost = 2,
    shocking_grasp = 3,
    chill_touch = 4,
    sap = 5,
    vex = 6,
    slow = 7,
    // Searing Smite: Fire damage and a Constitution save at the start of each
    // of the target's turns, for up to 1 minute.
    searing_smite = 8,
    // Spell benefits with no save, SRD 5.2.1. Shield of Faith: +2 AC.
    shield_of_faith = 9,
    // Heroism: Temporary HP equal to `dc` (the caster's spellcasting modifier)
    // at the start of each of the target's turns.
    heroism = 10,
    // Divine Favor: +1d4 Radiant damage on weapon hits.
    divine_favor = 11,
    // Bless: +1d4 to attack rolls and saving throws.
    bless = 12,
    // Protection from Evil and Good: Aberrations, Celestials, Elementals, Fey,
    // Fiends and Undead have Disadvantage on attack rolls against the target.
    protection_from_evil_and_good = 13,
    // Command: the target obeys on its next turn; `dc` holds its CommandOption.
    command = 14,
    // Sacred Weapon: `dc` (the Charisma modifier, at least 1) is added to
    // attack rolls with Melee weapons; they may deal Radiant damage.
    sacred_weapon = 15,
    // Hunter's Mark: the caster's attack-roll hits deal an extra 1d6 Force damage.
    hunters_mark = 16,
    // Longstrider: Speed increases by 10 feet.
    longstrider = 17,
    // Ensnaring Strike: Restrained, 1d6 Piercing damage at the start of each of
    // the target's turns; `dc` is the Strength (Athletics) check that escapes it.
    ensnaring_strike = 18,
    // Entangle: Restrained by the plants; `dc` is the Strength (Athletics)
    // check that frees the creature without ending the spell.
    entangle = 19,
    // Turn Undead: Frightened and Incapacitated, fleeing the Cleric, until it
    // takes damage or the minute ends.
    turned = 20,
    // Aid: the Hit Point maximum (and current Hit Points when cast) rise by `dc`.
    aid = 21,
    // Guiding Bolt: the next attack roll against the target has Advantage,
    // until the end of the caster's next turn.
    guiding_bolt = 22,
    // Bane: the target subtracts 1d4 from attack rolls and saving throws.
    bane = 23,
    // Hold Person: Paralyzed, with a Wisdom save at the end of each of its turns.
    hold_person = 24,
    // Sanctuary: a creature that attacks the target or harms it with a spell
    // makes a Wisdom save against `dc` or loses the attack (CLASS-9).
    sanctuary = 25,
    // Warding Bond: +1 AC and saves, Resistance to all damage; the caster takes
    // the same damage.
    warding_bond = 26,
    // Protection from Poison: Resistance to Poison damage.
    protection_from_poison = 27,
    // Resistance: 1d4 less damage of type `dc` (a DamageType), once per turn.
    resistance = 28,
    // Prayer of Healing: the creature cannot be healed by it again until it
    // finishes a Long Rest.
    prayer_of_healing = 29,
    // Expeditious Retreat: Dash as a Bonus Action each turn.
    expeditious_retreat = 30,
    // Mage Armor: an unarmored creature's base AC becomes 13 + Dexterity.
    mage_armor = 31,
    // Poisoned (Ray of Sickness): Disadvantage on attack rolls and ability checks.
    poisoned = 32,
    // Sleep, first stage: Incapacitated; a failed Wisdom save at the end of its
    // next turn turns it into `asleep`.
    drowsy = 33,
    // Sleep, second stage: Unconscious (Incapacitated, Prone, helpless).
    asleep = 34,
    // Hideous Laughter: Prone and Incapacitated, a Wisdom save at the end of
    // each of its turns and, with Advantage, whenever it takes damage.
    laughing = 35,
    // Color Spray: Blinded until the end of the caster's next turn.
    dazzled = 36,
    // Web: Restrained until an Athletics check breaks free, like Entangle.
    webbed = 37,
    // Shield: +5 AC and no Magic Missile damage until the caster's next turn.
    shield = 38,
    // Acid Arrow: 2d4 Acid damage when it runs out, at the end of the
    // target's next turn.
    acid_arrow = 39,
    // Ray of Enfeeblement: Disadvantage on Strength-based D20 Tests and 1d8
    // less damage, with a Constitution save at the end of each of its turns.
    enfeebled = 40,
    // Blur: attack rolls against the creature have Disadvantage.
    blur = 41,
    // Mirror Image: `dc` counts the duplicates left that may take a hit.
    mirror_image = 42,
    // Magic Weapon: `dc` is the bonus to weapon attack and damage rolls.
    magic_weapon = 43,
    // Invisibility: the Invisible condition until the creature attacks or casts.
    invisible = 44,
    // See Invisibility: Invisible creatures are seen as if visible.
    see_invisibility = 45,
    // Enlarge/Reduce: Advantage (Enlarge) or Disadvantage (Reduce) on Strength
    // saves, and 1d4 more or less weapon damage.
    enlarged = 46,
    reduced = 47,
    // Dragon's Breath: `dc` is the damage type the creature may exhale.
    dragons_breath = 48,
    // Charm Person: Charmed by the caster, so it cannot attack or target it.
    charmed = 49,
    // Rage: until the end of the Barbarian's next turn, unless extended.
    raging = 50,
    // Reckless Attack: until the start of the Barbarian's next turn; `dc` is 1
    // once this turn's Frenzy has struck.
    reckless = 51,
    // Open Hand Technique's Addle: no Opportunity Attacks until the start of
    // the creature's next turn.
    addled = 52,
    // Innate Sorcery: +1 spell save DC and Advantage on spell attack rolls.
    innate_sorcery = 53,
    // A Metamagic option readied for the Sorcerer's next spell this turn; `dc`
    // is the option, or 10 + the type for Transmuted Spell.
    metamagic = 54,
    // Extended Spell: Advantage on saves to keep this Concentration.
    extended = 55,
    // Hex: the caster's attack-roll hits deal 1d6 more Necrotic damage.
    hex = 56,
    // Faerie Fire: attack rolls against the creature have Advantage if the
    // attacker sees it, and it cannot be Invisible.
    outlined = 57,
    // Starry Wisp: it cannot be Invisible until the end of the caster's next turn.
    lit = 58,
    // Bardic Inspiration: `dc` is the die added to the creature's next failed
    // attack roll, save or check that the die could turn into a success.
    inspired = 59,
    // Shillelagh: the caster's Club or Quarterstaff uses the spellcasting
    // ability and a d8.
    shillelagh = 60,
    // Produce Flame: a flame in hand the caster may hurl as an Action.
    produce_flame = 61,
    // Barkskin: the creature's AC is at least 17.
    barkskin = 62,
    // Flame Blade: a fiery blade in hand the caster attacks with as an Action.
    flame_blade = 63,
    // Moonbeam: the creature has made this turn's save against a Moonbeam.
    moonlit = 64,
    // Heat Metal: the creature's armor glows red-hot; `dc` is 1 during the
    // turn it was cast, before the caster may heat it again.
    heated = 65,
    // Heat Metal: Disadvantage on attack rolls until the caster's next turn.
    scorched = 66,
    // Wild Shape: `dc` is the Beast form, one past its index in beast_forms.
    wild_shape = 67,
    // Guidance: 1d4 on ability checks with one skill; `dc` indexes guidance_skills.
    guidance = 68,
    // Relentless Endurance has been used; it returns with a Long Rest.
    relentless_used = 69,
    // A species trait used per Long Rest (a Dragonborn's Breath Weapon): `dc`
    // counts the uses spent since the last one.
    trait_uses = 70,
    // Hidden by the Hide action: Invisible until found; `dc` is the Stealth
    // total an enemy's Perception must reach.
    hidden = 71,
    // Heroic Inspiration, which a Human gains on finishing a Long Rest.
    heroic_inspiration = 72,
    // Frost's Chill: Speed 10 feet lower until the Goliath's next turn.
    frosts_chill = 73,
    // Heightened Spell: Disadvantage on every save against one spell of the
    // source's; `dc` is the spell's spell_index().
    heightened = 74,
    // Brutal Strike's Hamstring Blow: Speed 15 feet lower until the start of
    // the Barbarian's next turn.
    hamstrung = 75,
    // Stunning Strike (Monk 5): Stunned until the start of the Monk's next
    // turn; or, on a successful save, Speed halved (staggered) while the next
    // attack roll against it has Advantage (a Guiding Bolt effect).
    stunned = 76,
    staggered = 77,
    // Studied Attacks (Fighter 13): after a miss, the Fighter's next attack
    // roll against the creature has Advantage, until the end of its next turn.
    studied = 78,
    // Frightened for up to a minute, with a Wisdom save at the end of each of
    // the creature's turns (Intimidating Presence).
    frightened = 79,
    // Brutal Strike's Staggering Blow, until the start of the Barbarian's next
    // turn: no Opportunity Attacks, and while `dc` is 1, Disadvantage on the
    // next saving throw.
    staggering = 80,
    // Brutal Strike's Sundering Blow: the next attack roll another creature
    // makes against it, before the start of the Barbarian's next turn, has +5.
    sundered = 81,
    // Cunning Strike's Knock Out (Rogue 14): Unconscious for up to a minute,
    // until damaged or shaken awake, with a Constitution save at the end of
    // each of its turns.
    knocked_out = 82,
    // Cunning Strike's Daze (Rogue 14): on its next turn, only one of moving,
    // an action or a Bonus Action.
    dazed = 83,
    // Shining Smite: attack rolls against the creature have Advantage, and it
    // cannot be Invisible.
    shining = 84,
    // Abjure Foes (Paladin 9): Frightened of the Paladin for 1 minute or until
    // it takes damage; on its turns only one of moving, an action or a Bonus Action.
    abjured = 85,
    // Beacon of Hope: Advantage on Wisdom saves and death saves, and healing
    // restores its maximum.
    hopeful = 86,
    // Smite of Protection (Devotion 15), on the Paladin until the start of its
    // next turn: creatures in its Aura of Protection have +2 AC and +2 to
    // Dexterity saves.
    smite_protected = 87,
    // Death Ward: the first drop to 0 Hit Points leaves 1 instead.
    death_ward = 88,
    // Aura of Life, on its caster: allies within 30 feet resist Necrotic damage
    // and one at 0 Hit Points regains 1 as its turn starts.
    aura_of_life = 89,
    // Freedom of Movement: Difficult Terrain costs nothing extra, and magic
    // neither slows the creature nor leaves it Paralyzed or Restrained.
    free_moving = 90,
    // Banishment: gone to a demiplane, Incapacitated, while its caster
    // concentrates; after a full minute an extraplanar creature never returns.
    banished = 91,
    // Guardian of Faith: the creature has saved against a guardian this turn.
    faith_tested = 92,
    // Magic Circle's minute of casting, on its caster: `dc` is the slot level,
    // spent only once the circle stands.
    circling = 93,
    // A Magic Circle drawn in camp, on its caster: `dc` indexes ward_types; no
    // creature of that type interrupts the rest while it lasts.
    circle_ward = 94,
    // A party member who died less than a minute ago, whom Revivify can raise.
    fallen = 95,
    // Multiattack Defense (Hunter Ranger 7), on the Ranger: the creature that
    // hit it this turn, its source, attacks it with Disadvantage until the
    // turn ends.
    defended = 96,
    // Ensnaring Strike's vines, on its caster: `dc` is how many d6 they deal.
    ensnaring = 97,
    // Protection from Energy: Resistance to the DamageType in `dc`.
    energy_ward = 98,
    // Conjure Animals: the creature has saved against a spirit pack this turn.
    pack_tested = 99,
    // Conjure Animals, on its caster: the pack has moved this turn.
    pack_moved = 100,
    // Stoneskin: Resistance to Bludgeoning, Piercing and Slashing damage.
    stoneskin = 101,
    // Conjure Woodland Beings: the creature has saved against its spirits this turn.
    beings_tested = 102,
    // Dominate Beast: Charmed and fighting for the source (LEVELS-1); `dc` is
    // the save it repeats when damaged.
    dominated = 103,
    // Nature's Veil (Ranger 14): Invisible until the end of its next turn.
    veiled = 104,
    // Superior Hunter's Defense (Hunter 15): Resistance to the DamageType in
    // `dc` until the end of the turn.
    defending = 105,
    // Spirit Guardians: the creature has saved against them this turn.
    guarded = 106,
    // Bestow Curse: `dc` indexes curses; 10 more when no Concentration holds it.
    cursed = 107,
    // Animate Dead's minute of casting, on its caster: `dc` is the slot level,
    // spent only once the corpses rise.
    animating = 108,
    // Animate Dead: a Zombie its source commands (LEVELS-1).
    raised = 109,
    // Contagion: Poisoned, with Disadvantage on one ability's saves; `dc` is
    // the ability, 10 for each end-of-turn success and 100 for each failure.
    contagion = 110,
    // Dispel Evil and Good, on its caster: Celestials, Elementals, Fey, Fiends
    // and Undead attack it with Disadvantage.
    dispel_evil = 111,
    // Insect Plague: the creature has saved against a swarm this turn.
    swarmed = 112,
    // A dead party member within Raise Dead's ten days.
    deceased = 113,
    // Back from the dead (Raise Dead): `dc` is the penalty to D20 Tests, one
    // less after each Long Rest.
    returned = 114,
    // Harm: the Hit Point maximum is lower by `dc` until Greater Restoration.
    drained = 115,
    // Heroes' Feast: `dc` more Hit Points at most, Poison Resistance and
    // Immunity to Frightened and Poisoned, for a day.
    feasted = 116,
    // True Seeing: Truesight, seeing the Invisible and through magical Darkness.
    true_seeing = 117,
    // Blade Barrier: the creature has saved against the blades this turn.
    bladed = 118,
    // Conjure Celestial: the pillar's light has touched the creature this turn.
    bathed = 119,
    // Divine Word: `dc` 1 is Blinded and Stunned for an hour, 2 Blinded for
    // 10 minutes.
    worded = 120,
    // Regenerate: 1 Hit Point regained at the start of each of its turns.
    regenerating = 121,
    // Regenerate's minute of casting, on the creature it is cast on: `dc` is
    // the slot level, given back if the casting fails.
    knitting = 122,
    // Improved Blessed Strikes (Cleric 14): a Cleric cantrip has just dealt
    // damage, so the caster may grant Temporary Hit Points this turn.
    vitality = 123,
    // Haste: doubled Speed, +2 AC, Advantage on Dexterity saves and a limited
    // second action each turn.
    hasted = 124,
    // Haste's end: Incapacitated with a Speed of 0 until the end of its next turn.
    lethargic = 125,
    // Hypnotic Pattern: Charmed, Incapacitated and Speed 0 until damaged or
    // shaken awake.
    entranced = 126,
    // Fear: Frightened of the source, fleeing it with the Dash action.
    terrified = 127,
    // Stinking Cloud: Poisoned, with no action or Bonus Action, this turn.
    retching = 128,
    // Vampiric Touch, on its caster: a Magic action repeats the touch.
    vampiric = 129,
    // Blink, on its caster: a d6 at the end of each of its turns.
    blinking = 130,
    // Blink: on the Ethereal Plane until the start of its next turn.
    ethereal = 131,
    // Tiny Hut, in camp: a rest that cannot be interrupted.
    sheltered = 132,
    // Sleet Storm: the creature has saved against the sleet this turn.
    sleeted = 133,
    // Black Tentacles: Restrained until it escapes or the spell ends; `dc` is
    // the escape's DC.
    tentacled = 134,
    // Black Tentacles: the creature has saved against them this turn.
    groped = 135,
    // Confusion: no Bonus Actions or Reactions and a d10 for each turn's
    // behavior; a Wisdom save at the end of each of its turns.
    confused = 136,
    // Fire Shield: `dc` 0 is the warm shield (Cold Resistance, Fire back), 1
    // the chill one (Fire Resistance, Cold back).
    fire_shield = 137,
    // Greater Invisibility: Invisible, whether or not it attacks.
    greater_invisible = 138,
    // Phantasmal Killer: Disadvantage on attack rolls; `dc` is the dice of
    // Psychic damage each failed end-of-turn save deals.
    haunted = 139,
    // Polymorph: a Beast form; `dc` is its index in beast_forms, plus 1.
    polymorphed = 140,
    // Resilient Sphere: sealed in, nothing passing in or out.
    sphered = 141,
    // Vitriolic Sphere: 5d4 Acid at the end of its next turn.
    vitriol = 142,
    // Wall of Fire: the creature has been burned entering the wall this turn.
    wall_burned = 143,
    // Conjure Minor Elementals, on its caster: `dc` is the extra damage's
    // DamageType.
    minor_elementals = 144,
    // A summoned creature (Summon Dragon, Animate Objects, Mislead's double),
    // held by its source's Concentration; `dc` is its attack bonus.
    summoned = 145,
    // Summon Dragon's spirit: `dc` is its breath's DamageType.
    spirit_breath = 146,
    // The spirit has breathed this turn.
    breathed = 147,
    // Summon Dragon, on its caster: Resistance to the DamageType in `dc`.
    shared_resistance = 148,
    // Mislead's double: `dc` 1 once an attack has passed through it.
    illusory = 149,
    // Conjure Elemental: Restrained by the spirit; `dc` is the DamageType.
    engulfed = 150,
    // Conjure Elemental, on its caster: `dc` is the spirit's DamageType.
    elemental_spirit = 151,
    // Telekinesis: Restrained until the end of the caster's next turn.
    lifted = 152,
    // Telekinesis, on its caster: a Magic action moves a creature again.
    telekinetic = 153,
    // Arcane Hand: the hand acts without a Bonus Action the turn it appears.
    hand_ready = 154,
    // Cloudkill: the creature has saved against the fog this turn.
    clouded = 155,
    // Disintegrate: a body turned to dust, which only Resurrection revives.
    dust = 156,
    // Contingency, on its caster: `dc` is the stored spell's index in
    // spell_table times 4 plus the trigger (contingency_triggers).
    contingency = 157,
    // Eyebite: Frightened, fleeing the caster with the Dash action, until it
    // stands 60 feet away out of the caster's sight. (Its sleep is Sleep's.)
    eye_panic = 158,
    // Eyebite: Poisoned.
    sickened = 159,
    // Eyebite: the creature saved, so this casting cannot target it again.
    eye_resisted = 160,
    // Eyebite, on its caster: a Magic action targets another creature.
    eyebiting = 161,
    // Flesh to Stone: Restrained; `dc` is the successes plus 10 for each failure.
    stoning = 162,
    // Flesh to Stone: Petrified; `dc` 1 once its full minute made it last.
    petrified = 163,
    // Flesh to Stone, saved against: Speed 0 until the start of the caster's
    // next turn.
    rooted = 164,
    // Irresistible Dance: Charmed and dancing.
    dancing = 165,
    // Irresistible Dance, saved against: dancing in place until the end of
    // its next turn.
    jigging = 166,
    // Magic Jar, on its caster: the body lies helpless while its soul
    // possesses another.
    jarred = 167,
    // Reverse Gravity: fallen upward, out of melee reach, until it ends.
    aloft = 168,
    // Finger of Death: a slain Humanoid that rises as a Zombie at the start
    // of its slayer's next turn.
    rising = 169,
    // Prismatic Spray's indigo ray: Restrained; `dc` is the save DC times 100
    // plus the successes plus 10 for each failure.
    indigo = 170,
    // Prismatic Spray's violet ray: Blinded until a Wisdom save at the start
    // of the caster's next turn; `dc` is the save DC.
    violet = 171,
    // Befuddlement: no spells or Magic actions; `dc` is the save DC.
    befuddled = 172,
    // Mind Blank: Immunity to Psychic damage and the Charmed condition.
    mind_blank = 173,
    // Maze: lost in a demiplane until a DC 20 Intelligence (Investigation)
    // check or the end of the spell.
    mazed = 174,
    // Power Word Stun: Stunned until a Constitution save at the end of one of
    // its turns; `dc` is the save DC.
    word_stunned = 175,
    // Overchannel (Evoker 14), readied: the next Wizard spell deals maximum damage.
    overchannel = 176,
    // Overchannel's uses since the last Long Rest, in `dc`.
    overchanneled = 177,
    // Simulacrum, on the copy: a Construct whose Hit Point maximum is lowered
    // by `dc` to half the original's, healed only by repair; it never wears off.
    simulacrum = 178,
    // Dragon Wings (Sorcerer 14), for an hour: no Difficult Terrain, and
    // enemies' squares passed as an ally's (LEVELS-3).
    dragon_wings = 179,
    // Dragon Wings' free use is spent until a Long Rest.
    wings_used = 180,
    // Dark One's Own Luck's uses since the last Long Rest, in `dc`.
    luck_used = 181,
    // Hurl Through Hell (Fiend 14): gone to the Lower Planes, Incapacitated,
    // until the end of the Warlock's next turn.
    hurled = 182,
    // Hurl Through Hell's free use is spent until a Long Rest.
    hurl_used = 183,
    // Mystic Arcanum's spells cast since the last Long Rest: `dc` has a bit
    // for each, 1 for level 6, 2 for 7 and 4 for 8.
    arcanum_used = 184,
    // Wild Resurgence (Druid 5): a slot spent for Wild Shape this turn, or
    // (`dc` 1) a Wild Shape use spent for a slot until a Long Rest.
    resurged = 185,
    // Natural Recovery (Land 6): the free Circle spell cast until a Long Rest.
    natural_cast = 186,
    // Conjure Fey: Frightened of the caster until the start of its next turn.
    fey_fright = 187,
    // Wall of Thorns: the thorns have tested the creature this turn.
    thorned = 188,
    // Animal Shapes: a Beast form, `dc` its index in beast_forms plus one,
    // until the fight ends or the creature leaves it.
    animal_shaped = 189,
    // Slow: half Speed, -2 to AC and Dexterity saves, no Reactions, an action
    // or a Bonus Action and one attack each turn, and a Wisdom save, `dc`,
    // to cast a spell; a Wisdom save at the end of each of its turns.
    time_slowed = 190,
    // Gift of the Protectors has kept someone up; it returns with the
    // Warlock's Long Rest.
    protectors_used = 191,
    // Tsunami: the creature has swum this turn, against the wave.
    swum = 192,
    // The Wand of Paralysis (MAGIC-3): Paralyzed for up to a minute, a
    // Constitution save at the end of each of its turns.
    paralysis_ray = 193,
    // A Potion of Giant Strength (MAGIC-3): Strength 21 for an hour.
    giant_strength = 194,
    // The Javelin of Lightning's bolt is spent until a Long Rest (MAGIC-3).
    javelin_spent = 195,
    // Enhance Ability from a scroll (MAGIC-6): Advantage on Strength checks.
    enhance_ability = 196,
    // Protection From Normal Missiles (MAGIC-6): a missile without magic from
    // more than 5 feet away misses.
    missile_ward = 197,
    // Snake Charm (MAGIC-6): Incapacitated until the fight ends; a hit slays it.
    snake_charmed = 198,
    // Cunning Strike's Poison (Rogue 5): Poisoned for up to a minute, a
    // Constitution save at the end of each of its turns.
    envenomed = 199,
    // A Pirate's Enthralling Panache: Charmed by it until the start of its next
    // turn. Unlike Charm Person's charm, damage does not end it.
    enthralled = 200,
    // A Wight's Life Drain: the Hit Point maximum lowered by `dc` until a Long
    // Rest or Greater Restoration.
    life_drained = 201,
    // The last kind: a new one goes after it and updates saved_effect_kind().
    // Plant Growth's Overgrowth has no effect kind; it is a zone.
    last = life_drained
};

// Magic Circle's creature types, by `dc`.
inline constexpr std::array<std::string_view, 5> ward_types{"celestial", "elemental", "fey",
                                                            "fiend", "undead"};

// The longest a spell benefit lasts, in milliseconds.
unsigned benefit_duration_ms(EffectKind kind);
inline constexpr unsigned round_ms = 6000;
inline constexpr std::size_t effect_limit = 128;

struct RollModifiers
{
    bool advantage{}, disadvantage{};
    // Halfling Luck: a d20 that shows 1 is rolled again, and the new roll used.
    bool lucky{};

    // Multiple sources never add extra dice. Opposing sources cancel.
    [[nodiscard]] int mode() const
    {
        return static_cast<int>(advantage) - static_cast<int>(disadvantage);
    }
};

struct SaveResult
{
    Ability ability{};
    int natural{}, bonus{}, dc{}, mode{};
    bool success{};
};

// `lucky_reroll`, when given, reports whether Luck rerolled a 1.
[[nodiscard]] SaveResult saving_throw(Ability ability, int bonus, int dc, RollModifiers modifiers,
                                      std::uint64_t &rng, bool *lucky_reroll = nullptr);
[[nodiscard]] int d20(RollModifiers modifiers, std::uint64_t &rng, bool *lucky_reroll = nullptr);
[[nodiscard]] std::array<Ability, 2> class_save_proficiencies(CharacterClass character_class);
// Whether a character of the class and level adds its Proficiency Bonus to the save.
[[nodiscard]] bool save_proficient(CharacterClass character_class, unsigned level, Ability ability);

// A source is provenance, never a borrowed Actor pointer. Scope distinguishes
// encounter-local monster IDs when a lasting effect reaches another encounter.
struct Effect
{
    std::uint64_t id{}, source_scope{};
    rules::EntityId source_actor{};
    std::string source_name;
    EffectKind kind{EffectKind::blindness};
    int dc{};
    unsigned remaining_ms{}, save_in_ms{};
    bool operator==(const Effect &) const = default;
};

struct EffectState
{
    std::uint64_t next_id{1};
    std::vector<Effect> active;
    bool prone{};
    bool operator==(const EffectState &) const = default;
};

struct EffectSubject
{
    rules::EntityId id{};
    std::reference_wrapper<EffectState> effects;
    std::array<int, 6> saves{};
    bool dead{}, str_dex_disadvantage{}, dodge{};
    // Gnomish Cunning: Advantage on Intelligence, Wisdom and Charisma saves.
    bool mental_advantage{};
    bool lucky{}; // Halfling Luck, for repeated saves and death saves
    bool brave{}; // Halfling Brave: Advantage on saves to end Frightened
};

struct EffectEvent
{
    rules::EntityId target{};
    Effect effect;
    std::optional<SaveResult> save;
    int inspiration{}; // a Bardic Inspiration die added to the failed save
    bool removed{};
};

using EffectObserver = std::function<void(const EffectEvent &)>;

// Bardic Inspiration's die rolled for a failed save, or 0 when the creature has
// none or the die could not make up the shortfall.
[[nodiscard]] int inspiration_die(const std::vector<Effect> &effects, const SaveResult &failed,
                                  std::uint64_t &rng);
// Ends the creature's Bardic Inspiration once its die is used.
void spend_inspiration(EffectState &effects);

// Command's options, numbered as a Command effect stores them in its `dc`.
enum class CommandOption : int
{
    approach = 1,
    flee,
    grovel,
    halt
};

// Who imposes an effect: the encounter's scope and the creature's ID in it.
// Passed apart, the two integers converted silently into each other's place
// (Effective C++ Item 18).
struct EffectSource
{
    std::uint64_t scope{};
    rules::EntityId actor{};
};

// A condition's save DC and how far away its first repeated save is; an int
// and an unsigned side by side were just as easy to swap.
struct RepeatingSave
{
    int dc{};
    unsigned first_save_ms{};
};

// At a future boundary, effects expiring at that boundary no longer prevent healing.
[[nodiscard]] bool healing_blocked(const EffectState &effects, std::uint64_t after_ms = 0) noexcept;
void apply_chill_touch(EffectState &effects, EffectSource source, std::string name,
                       unsigned duration_ms);
void apply_guiding_bolt(EffectState &effects, EffectSource source, std::string name,
                        unsigned duration_ms);
// Poisoned or Color Spray's Blinded, lasting `duration_ms` (to the end of the
// caster's next turn).
void apply_poisoned(EffectState &effects, EffectSource source, std::string name,
                    unsigned duration_ms, EffectKind kind = EffectKind::poisoned);
[[nodiscard]] bool opportunity_blocked(const EffectState &effects) noexcept;
void apply_shocking_grasp(EffectState &effects, EffectSource source, std::string name,
                          unsigned duration_ms);
// The net Speed reduction from effects; negative when Longstrider raises Speed,
// and enough to bring any Speed to 0 while Restrained.
[[nodiscard]] int speed_penalty(const EffectState &effects) noexcept;
[[nodiscard]] bool slowed(const EffectState &effects) noexcept;
[[nodiscard]] bool frosted(const EffectState &effects) noexcept;
void apply_ray_of_frost(EffectState &effects, EffectSource source, std::string name,
                        unsigned duration_ms);
void apply_frosts_chill(EffectState &effects, EffectSource source, std::string name,
                        unsigned duration_ms);
// Hamstring Blow: a newer blow replaces an older one, whoever struck it.
void apply_hamstring(EffectState &effects, EffectSource source, std::string name,
                     unsigned duration_ms);
// Whether Heightened Spell marked the creature for the source's spell.
[[nodiscard]] bool heightened_against(const EffectState &effects, std::uint64_t source_scope,
                                      rules::EntityId source_actor, std::string_view spell);
[[nodiscard]] bool sapped(const EffectState &effects) noexcept;
[[nodiscard]] bool vexed_by(const EffectState &effects, EffectSource source) noexcept;
[[nodiscard]] bool studied_by(const EffectState &effects, EffectSource source) noexcept;
// Brutal Strike's Staggering Blow or Sundering Blow, until the start of the
// Barbarian's next turn, `duration_ms` from now.
void apply_brutal_blow(EffectState &effects, EffectKind kind, EffectSource source, std::string name,
                       unsigned duration_ms);
// Spends a Staggering Blow's Disadvantage on this saving throw, if one is left.
[[nodiscard]] bool spend_staggering(EffectState &effects);
// Spends a Sundering Blow's +5 on an attack roll by anyone but its Barbarian.
[[nodiscard]] bool spend_sundering(EffectState &effects, EffectSource attacker);
// Replaces the source's earlier Studied Attacks mark on the creature.
void apply_studied(EffectState &effects, EffectSource source, std::string name,
                   unsigned duration_ms);
[[nodiscard]] bool has_attack_mastery(const EffectState &effects) noexcept;
[[nodiscard]] bool can_apply_attack_mastery(const EffectState &, EffectKind, EffectSource source);
void apply_attack_mastery(EffectState &, EffectKind, EffectSource source, std::string name,
                          unsigned duration_ms);
void consume_attack_masteries(EffectState &attacker, EffectState &target, EffectSource source);
[[nodiscard]] bool blinded(const EffectState &effects) noexcept;
[[nodiscard]] bool can_apply(const EffectState &effects) noexcept;
void apply_blindness(EffectState &effects, EffectSource source, std::string name,
                     RepeatingSave save);
void apply_ensnaring_strike(EffectState &, EffectSource source, std::string name, int dc);
void apply_entangle(EffectState &, EffectSource source, std::string name, int dc,
                    EffectKind kind = EffectKind::entangle);
// Restrained: Speed 0, attacks against it have Advantage, its attacks and
// Dexterity saves have Disadvantage.
[[nodiscard]] bool restrained(const EffectState &effects) noexcept;
// Paralyzed: Incapacitated and Speed 0; it fails Strength and Dexterity saves,
// attacks against it have Advantage and hits within 5 feet are critical.
[[nodiscard]] bool paralyzed(const EffectState &effects) noexcept;
// Stunned: by a Stunning Strike, or for an hour by Divine Word.
[[nodiscard]] bool stunned(const EffectState &effects) noexcept;
// Eyebite's sleep: Sleep's Unconscious, held by the source's Concentration.
void apply_asleep(EffectState &effects, EffectSource source, std::string name);
// Incapacitated by a spell: Paralyzed, Sleep or Hideous Laughter.
[[nodiscard]] bool incapacitated(const EffectState &effects) noexcept;
// A repeated save condition with its first save `first_save_ms` away:
// Sleep's drowsiness, Hideous Laughter, Ray of Enfeeblement or Frightened.
void apply_repeating_condition(EffectState &effects, EffectKind kind, EffectSource source,
                               std::string name, RepeatingSave save);
void apply_hold_person(EffectState &effects, EffectSource source, std::string name,
                       RepeatingSave save);
void apply_sanctuary(EffectState &effects, EffectSource source, std::string name, int dc);
void apply_searing_smite(EffectState &, EffectSource source, std::string name, int dc);
// Applies a spell benefit that has no save. `value` is kind-specific.
void apply_spell_benefit(EffectState &, EffectSource source, std::string name, EffectKind kind,
                         int value);
// Lasts until the end of the target's next turn, `duration_ms` from now.
void apply_command(EffectState &, EffectSource source, std::string name, CommandOption option,
                   unsigned duration_ms);
// The Command the target must obey, if any.
[[nodiscard]] const Effect *command_effect(const EffectState &);
bool has_effect(const EffectState &, EffectKind) noexcept;
// Aid's increase to the Hit Point maximum; Aid does not stack with itself.
[[nodiscard]] int hit_point_bonus(const EffectState &);
[[nodiscard]] RollModifiers saving_modifiers(Ability ability, bool untrained_armor, bool dodge);

// What decides an attack roll's Advantage and Disadvantage. Four flags in a row
// are easy to pass in the wrong order, so callers name each one.
struct AttackConditions
{
    bool attacker_blind{};
    bool target_blind{};
    bool target_dodging{};
    bool other_disadvantage{};
};

[[nodiscard]] RollModifiers attack_modifiers(const AttackConditions &conditions);

// All subjects share one chronological event queue. Advancing 12 seconds once
// must consume exactly the same rolls as advancing 1 second twelve times.
// Intelligence, Wisdom or Charisma: the saves Gnomish Cunning favors.
[[nodiscard]] constexpr bool mental(Ability ability)
{
    return ability == Ability::intelligence || ability == Ability::wisdom ||
           ability == Ability::charisma;
}

// A repeated save's modifiers for the subject, Gnomish Cunning included.
[[nodiscard]] RollModifiers mental_save_modifiers(Ability ability, const EffectSubject &subject);
void elapse_effects(std::span<EffectSubject> subjects, std::uint64_t milliseconds,
                    std::uint64_t &rng, const EffectObserver &observe = {});
void write_effects(std::ostream &out, const EffectState &effects);
[[nodiscard]] EffectState read_effects(std::istream &in);
} // namespace opengold::srd5::detail
#endif
