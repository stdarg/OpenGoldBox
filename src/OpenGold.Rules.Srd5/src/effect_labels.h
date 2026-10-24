#ifndef OPENGOLD_SRD5_EFFECT_LABELS_H
#define OPENGOLD_SRD5_EFFECT_LABELS_H
#include "status_effects.h"
#include <algorithm>
#include <array>
#include <string_view>

namespace opengold::srd5::detail
{
// What "{value}" in a description stands for: the effect's `dc` as a number,
// as a DamageType or as a Beast form (one past its index in beast_forms).
enum class EffectValue
{
    none,
    number,
    damage_type,
    beast_form
};

// What a status effect is called and what it does, for the character sheet
// and the combat panel.
struct EffectLabel
{
    EffectKind kind;
    std::string_view name, description;
    EffectValue value{EffectValue::none};
};

inline constexpr std::array effect_labels{
    EffectLabel{
        EffectKind::blindness, "Blinded (Blindness/Deafness)",
        "Cannot see: its attack rolls have Disadvantage and attack rolls against it have Advantage. A Constitution save at the end of each of its turns ends it."},
    EffectLabel{EffectKind::ray_of_frost, "Ray of Frost", "Speed reduced by 10 feet."},
    EffectLabel{EffectKind::shocking_grasp, "Shocking Grasp", "Cannot make Opportunity Attacks."},
    EffectLabel{EffectKind::chill_touch, "Chill Touch", "Cannot regain Hit Points."},
    EffectLabel{EffectKind::sap, "Sap", "Its next attack roll has Disadvantage."},
    EffectLabel{EffectKind::vex, "Vex",
                "The next attack roll against it by the creature that caused this has Advantage."},
    EffectLabel{EffectKind::slow, "Slowed", "Speed reduced by 10 feet."},
    EffectLabel{
        EffectKind::searing_smite, "Searing Smite",
        "Takes Fire damage at the start of each of its turns until a Constitution save puts the flames out."},
    EffectLabel{EffectKind::shield_of_faith, "Shield of Faith", "+2 to Armor Class."},
    EffectLabel{EffectKind::heroism, "Heroism",
                "Gains {value} Temporary Hit Points at the start of each of its turns.",
                EffectValue::number},
    EffectLabel{EffectKind::divine_favor, "Divine Favor",
                "Weapon hits deal an extra 1d4 Radiant damage."},
    EffectLabel{EffectKind::bless, "Bless", "Adds 1d4 to attack rolls and saving throws."},
    EffectLabel{
        EffectKind::protection_from_evil_and_good, "Protection from Evil and Good",
        "Aberrations, Celestials, Elementals, Fey, Fiends and Undead have Disadvantage on attack rolls against it."},
    EffectLabel{EffectKind::command, "Commanded", "Must obey a one-word command on its next turn."},
    EffectLabel{EffectKind::sacred_weapon, "Sacred Weapon",
                "+{value} to attack rolls with Melee weapons, which may deal Radiant damage.",
                EffectValue::number},
    EffectLabel{EffectKind::hunters_mark, "Hunter's Mark",
                "Hits by the creature that marked it deal an extra 1d6 Force damage."},
    EffectLabel{EffectKind::longstrider, "Longstrider", "Speed increased by 10 feet."},
    EffectLabel{
        EffectKind::ensnaring_strike, "Restrained (Ensnaring Strike)",
        "Takes 1d6 Piercing damage at the start of each of its turns; a Strength (Athletics) check escapes."},
    EffectLabel{EffectKind::entangle, "Restrained (Entangle)",
                "A Strength (Athletics) check breaks free."},
    EffectLabel{EffectKind::turned, "Turned",
                "Frightened and Incapacitated, fleeing the Cleric, until it takes damage."},
    EffectLabel{EffectKind::aid, "Aid", "Hit Point maximum increased by {value}.",
                EffectValue::number},
    EffectLabel{EffectKind::guiding_bolt, "Guiding Bolt",
                "The next attack roll against it has Advantage."},
    EffectLabel{EffectKind::bane, "Bane", "Subtracts 1d4 from attack rolls and saving throws."},
    EffectLabel{
        EffectKind::hold_person, "Paralyzed (Hold Person)",
        "Cannot move or act; fails Strength and Dexterity saves; attack rolls against it have Advantage, and hits from within 5 feet are critical. A Wisdom save at the end of each of its turns ends it."},
    EffectLabel{
        EffectKind::sanctuary, "Sanctuary",
        "A creature that attacks it or harms it with a spell must make a Wisdom save or lose the attack."},
    EffectLabel{
        EffectKind::warding_bond, "Warding Bond",
        "+1 to Armor Class and saving throws and Resistance to all damage; the caster takes the same damage."},
    EffectLabel{EffectKind::protection_from_poison, "Protection from Poison",
                "Resistance to Poison damage."},
    EffectLabel{EffectKind::resistance, "Resistance",
                "Takes 1d4 less {value} damage once each turn.", EffectValue::damage_type},
    EffectLabel{EffectKind::prayer_of_healing, "Prayer of Healing received",
                "Prayer of Healing cannot heal it again until it finishes a Long Rest."},
    EffectLabel{EffectKind::expeditious_retreat, "Expeditious Retreat",
                "Can Dash as a Bonus Action each turn."},
    EffectLabel{EffectKind::mage_armor, "Mage Armor",
                "Without armor, its base Armor Class is 13 + its Dexterity modifier."},
    EffectLabel{EffectKind::poisoned, "Poisoned",
                "Disadvantage on attack rolls and ability checks."},
    EffectLabel{EffectKind::drowsy, "Incapacitated (Sleep)",
                "Falls Unconscious if it fails a Wisdom save at the end of its next turn."},
    EffectLabel{EffectKind::asleep, "Unconscious (Sleep)",
                "Incapacitated and Prone; attack rolls against it have Advantage."},
    EffectLabel{
        EffectKind::laughing, "Prone and Incapacitated (laughing)",
        "A Wisdom save at the end of each of its turns ends it, and so may one with Advantage whenever it takes damage."},
    EffectLabel{
        EffectKind::dazzled, "Blinded (Color Spray)",
        "Cannot see: its attack rolls have Disadvantage and attack rolls against it have Advantage."},
    EffectLabel{EffectKind::webbed, "Restrained (Web)",
                "A Strength (Athletics) check breaks free."},
    EffectLabel{EffectKind::shield, "Shield",
                "+5 to Armor Class, and Magic Missile deals it no damage."},
    EffectLabel{EffectKind::acid_arrow, "Burning acid (Acid Arrow)",
                "Takes 2d4 Acid damage at the end of its next turn."},
    EffectLabel{
        EffectKind::enfeebled, "Enfeebled (Ray of Enfeeblement)",
        "Disadvantage on Strength-based D20 Tests and 1d8 less damage. A Constitution save at the end of each of its turns ends it."},
    EffectLabel{EffectKind::blur, "Blur", "Attack rolls against it have Disadvantage."},
    EffectLabel{EffectKind::mirror_image, "Mirror Image",
                "{value} duplicates left that may take a hit in its place.", EffectValue::number},
    EffectLabel{EffectKind::magic_weapon, "Magic Weapon",
                "+{value} to weapon attack and damage rolls.", EffectValue::number},
    EffectLabel{EffectKind::invisible, "Invisible", "Unseen until it attacks or casts a spell."},
    EffectLabel{EffectKind::see_invisibility, "See Invisibility",
                "Sees Invisible creatures as if they were visible."},
    EffectLabel{EffectKind::enlarged, "Enlarged",
                "Advantage on Strength saves; weapon hits deal 1d4 more damage."},
    EffectLabel{EffectKind::reduced, "Reduced",
                "Disadvantage on Strength saves; weapon hits deal 1d4 less damage."},
    EffectLabel{EffectKind::dragons_breath, "Dragon's Breath",
                "Can exhale {value} damage as a Magic action.", EffectValue::damage_type},
    EffectLabel{EffectKind::charmed, "Charmed",
                "Cannot attack or target the creature that charmed it."},
    EffectLabel{
        EffectKind::raging, "Raging",
        "Extra damage with Strength attacks, Advantage on Strength checks and saves, and Resistance to Bludgeoning, Piercing and Slashing damage."},
    EffectLabel{
        EffectKind::reckless, "Reckless",
        "Advantage on Strength attack rolls this turn; attack rolls against it have Advantage."},
    EffectLabel{EffectKind::addled, "Addled", "Cannot make Opportunity Attacks."},
    EffectLabel{EffectKind::innate_sorcery, "Innate Sorcery",
                "+1 to its spell save DC and Advantage on spell attack rolls."},
    EffectLabel{EffectKind::metamagic, "Metamagic readied",
                "Its next spell this turn is changed by Metamagic."},
    EffectLabel{EffectKind::extended, "Extended Spell",
                "Advantage on saves to keep its Concentration."},
    EffectLabel{EffectKind::hex, "Hexed", "Hits by the caster deal an extra 1d6 Necrotic damage."},
    EffectLabel{EffectKind::outlined, "Outlined (Faerie Fire)",
                "Attack rolls against it have Advantage, and it cannot be Invisible."},
    EffectLabel{EffectKind::lit, "Lit (Starry Wisp)", "Cannot be Invisible."},
    EffectLabel{EffectKind::inspired, "Inspired",
                "Can add a d{value} to a failed attack roll, save or check.", EffectValue::number},
    EffectLabel{EffectKind::shillelagh, "Shillelagh",
                "Its Club or Quarterstaff uses its spellcasting ability and deals a d8."},
    EffectLabel{EffectKind::produce_flame, "Produce Flame",
                "Holds a flame it can hurl as an action."},
    EffectLabel{EffectKind::barkskin, "Barkskin", "Armor Class is at least 17."},
    EffectLabel{EffectKind::flame_blade, "Flame Blade",
                "Holds a fiery blade it attacks with as an action."},
    EffectLabel{EffectKind::heated, "Heated (Heat Metal)",
                "Its armor glows red-hot, and the caster can burn it again each turn."},
    EffectLabel{EffectKind::scorched, "Scorched (Heat Metal)", "Disadvantage on attack rolls."},
    EffectLabel{EffectKind::wild_shape, "Wild Shape", "In the form of a {value}.",
                EffectValue::beast_form},
    EffectLabel{EffectKind::guidance, "Guidance", "Adds 1d4 to ability checks with one skill."},
    EffectLabel{EffectKind::relentless_used, "Relentless Endurance used",
                "Its next drop to 0 Hit Points is not stopped at 1."},
    EffectLabel{EffectKind::trait_uses, "Species trait used",
                "{value} uses spent since the last Long Rest.", EffectValue::number},
    EffectLabel{EffectKind::hidden, "Hidden", "Unseen by enemies until one finds it."},
    EffectLabel{EffectKind::heroic_inspiration, "Heroic Inspiration", "Can roll a d20 again once."},
    EffectLabel{EffectKind::frosts_chill, "Frost's Chill", "Speed reduced by 10 feet."},
    EffectLabel{EffectKind::heightened, "Heightened Spell",
                "Disadvantage on every save against one spell."},
    EffectLabel{EffectKind::hamstrung, "Hamstring Blow", "Speed reduced by 15 feet."},
    EffectLabel{
        EffectKind::stunned, "Stunned (Stunning Strike)",
        "Incapacitated; fails Strength and Dexterity saves, and attack rolls against it have Advantage."},
    EffectLabel{EffectKind::staggered, "Staggered (Stunning Strike)",
                "Speed halved, and the next attack roll against it has Advantage."},
    EffectLabel{EffectKind::studied, "Studied",
                "The Fighter's next attack roll against it has Advantage."},
    EffectLabel{
        EffectKind::frightened, "Frightened",
        "Disadvantage on attack rolls and ability checks while it can see the source. A Wisdom save at the end of each of its turns ends it."},
    EffectLabel{EffectKind::staggering, "Staggered (Staggering Blow)",
                "Cannot make Opportunity Attacks, and its next saving throw has Disadvantage."},
    EffectLabel{EffectKind::sundered, "Sundered (Sundering Blow)",
                "The next attack roll another creature makes against it gains +5."},
    EffectLabel{
        EffectKind::knocked_out, "Unconscious (Knock Out)",
        "Wakes if damaged or shaken awake. A Constitution save at the end of each of its turns ends it."},
    EffectLabel{EffectKind::dazed, "Dazed",
                "On its next turn it can only move, take an action or take a Bonus Action."},
    EffectLabel{EffectKind::shining, "Shining (Shining Smite)",
                "Attack rolls against it have Advantage, and it cannot be Invisible."},
    EffectLabel{
        EffectKind::abjured, "Frightened (Abjure Foes)",
        "On its turns it can only move, take an action or take a Bonus Action; taking damage ends it."},
    EffectLabel{EffectKind::hopeful, "Beacon of Hope",
                "Advantage on Wisdom saves and death saves, and healing restores the most it can."},
    EffectLabel{EffectKind::smite_protected, "Smite of Protection",
                "Creatures in its Aura of Protection have +2 to Armor Class and Dexterity saves."},
    EffectLabel{EffectKind::death_ward, "Death Ward",
                "Its first drop to 0 Hit Points leaves it at 1 instead."},
    EffectLabel{
        EffectKind::aura_of_life, "Aura of Life",
        "Allies within 30 feet have Resistance to Necrotic damage, and one at 0 Hit Points regains 1 as its turn starts."},
    EffectLabel{
        EffectKind::free_moving, "Freedom of Movement",
        "Difficult Terrain costs it nothing extra, and magic cannot slow, paralyze or restrain it."},
    EffectLabel{EffectKind::banished, "Banished",
                "Gone to another plane, Incapacitated, while the caster concentrates."},
    EffectLabel{EffectKind::circling, "Casting Magic Circle",
                "The circle stands when the minute of casting ends."},
    EffectLabel{EffectKind::circle_ward, "Magic Circle",
                "Creatures of one kind cannot interrupt the party's rest."},
    EffectLabel{EffectKind::fallen, "Fallen",
                "Died less than a minute ago; Revivify can bring it back."},
    EffectLabel{EffectKind::defended, "Multiattack Defense",
                "The creature that hit it this turn attacks it with Disadvantage."},
    EffectLabel{EffectKind::energy_ward, "Protection from Energy", "Resistance to {value} damage.",
                EffectValue::damage_type},
    EffectLabel{EffectKind::stoneskin, "Stoneskin",
                "Resistance to Bludgeoning, Piercing and Slashing damage."},
    EffectLabel{
        EffectKind::dominated, "Dominated",
        "Charmed and fighting for the creature that dominated it; it saves again whenever it takes damage."},
    EffectLabel{EffectKind::veiled, "Invisible (Nature's Veil)",
                "Invisible until the end of its next turn."},
    EffectLabel{EffectKind::defending, "Superior Hunter's Defense",
                "Resistance to {value} damage this turn.", EffectValue::damage_type},
    EffectLabel{EffectKind::cursed, "Cursed", "Bestow Curse hinders its checks, saves or attacks."},
    EffectLabel{EffectKind::animating, "Casting Animate Dead",
                "The dead rise when the minute of casting ends."},
    EffectLabel{EffectKind::raised, "Raised (Zombie)",
                "A Zombie that obeys the creature that raised it."},
    EffectLabel{EffectKind::contagion, "Poisoned (Contagion)",
                "Disadvantage on attack rolls, ability checks and one ability's saving throws."},
    EffectLabel{EffectKind::dispel_evil, "Dispel Evil and Good",
                "Celestials, Elementals, Fey, Fiends and Undead attack it with Disadvantage."},
    EffectLabel{EffectKind::deceased, "Recently dead",
                "Raise Dead can bring it back within ten days of its death."},
    EffectLabel{EffectKind::returned, "Returned from death",
                "-{value} to D20 Tests; the penalty drops by 1 after each Long Rest.",
                EffectValue::number},
    EffectLabel{EffectKind::drained, "Hit Point maximum lowered (Harm)",
                "Hit Point maximum lowered by {value} until Greater Restoration.",
                EffectValue::number},
    EffectLabel{
        EffectKind::petrifying, "Restrained (Petrifying Gaze)",
        "Speed 0; turning to stone. A Constitution save at the end of its next turn ends it; a failure leaves it Petrified."},
    EffectLabel{EffectKind::life_drained, "Hit Point maximum lowered (Life Drain)",
                "Hit Point maximum lowered by {value} until a Long Rest.", EffectValue::number},
    EffectLabel{
        EffectKind::feasted, "Heroes' Feast",
        "Hit Point maximum increased by {value}, Resistance to Poison damage, and immune to Frightened and Poisoned.",
        EffectValue::number},
    EffectLabel{EffectKind::true_seeing, "True Seeing",
                "Sees Invisible creatures and through magical Darkness."},
    EffectLabel{EffectKind::worded, "Divine Word",
                "Blinded, and Stunned too if the word struck it hardest."},
    EffectLabel{EffectKind::regenerating, "Regenerate",
                "Regains 1 Hit Point at the start of each of its turns."},
    EffectLabel{EffectKind::knitting, "Casting Regenerate",
                "The spell takes hold when the minute of casting ends."},
    EffectLabel{
        EffectKind::hasted, "Haste",
        "Speed doubled, +2 to Armor Class, Advantage on Dexterity saves, and a second, limited action each turn."},
    EffectLabel{EffectKind::lethargic, "Incapacitated (lethargy)",
                "Cannot move or act until the end of its next turn, as Haste wears off."},
    EffectLabel{EffectKind::entranced, "Charmed (Hypnotic Pattern)",
                "Incapacitated with a Speed of 0 until damaged or shaken awake."},
    EffectLabel{EffectKind::terrified, "Frightened (Fear)",
                "Flees the source with the Dash action."},
    EffectLabel{EffectKind::retching, "Poisoned (Stinking Cloud)",
                "Cannot take an action or a Bonus Action this turn."},
    EffectLabel{EffectKind::vampiric, "Vampiric Touch", "Can repeat the touch as a Magic action."},
    EffectLabel{EffectKind::blinking, "Blink",
                "May vanish to the Ethereal Plane at the end of each of its turns."},
    EffectLabel{EffectKind::ethereal, "Ethereal",
                "On the Ethereal Plane, out of reach, until the start of its next turn."},
    EffectLabel{EffectKind::sheltered, "Tiny Hut", "Nothing can interrupt the party's rest."},
    EffectLabel{EffectKind::tentacled, "Restrained (Black Tentacles)",
                "Restrained until it escapes or the spell ends."},
    EffectLabel{
        EffectKind::confused, "Confused",
        "No Bonus Actions or Reactions, and a d10 decides what it does each turn. A Wisdom save at the end of each of its turns ends it."},
    EffectLabel{
        EffectKind::fire_shield, "Fire Shield",
        "Resistance to Cold damage (warm shield) or Fire damage (chill shield); a creature that hits it in melee takes damage."},
    EffectLabel{EffectKind::greater_invisible, "Invisible (Greater Invisibility)",
                "Unseen even when it attacks or casts a spell."},
    EffectLabel{
        EffectKind::haunted, "Phantasmal Killer",
        "Disadvantage on attack rolls; each failed save at the end of its turns deals Psychic damage."},
    EffectLabel{EffectKind::polymorphed, "Polymorphed", "Turned into a {value}.",
                EffectValue::beast_form},
    EffectLabel{EffectKind::sphered, "Resilient Sphere",
                "Sealed in a sphere that nothing passes in or out of."},
    EffectLabel{EffectKind::vitriol, "Vitriolic Sphere",
                "Takes 5d4 Acid damage at the end of its next turn."},
    EffectLabel{EffectKind::minor_elementals, "Conjure Minor Elementals",
                "Its attacks deal extra {value} damage.", EffectValue::damage_type},
    EffectLabel{EffectKind::summoned, "Summoned", "Lasts while its summoner keeps Concentration."},
    EffectLabel{EffectKind::shared_resistance, "Summon Dragon", "Resistance to {value} damage.",
                EffectValue::damage_type},
    EffectLabel{EffectKind::engulfed, "Restrained (Conjure Elemental)",
                "Engulfed by an elemental spirit."},
    EffectLabel{EffectKind::lifted, "Restrained (Telekinesis)",
                "Held aloft until the end of the caster's next turn."},
    EffectLabel{EffectKind::telekinetic, "Telekinesis",
                "Can move a creature again as a Magic action."},
    EffectLabel{EffectKind::dust, "Turned to dust", "Only Resurrection can bring it back."},
    EffectLabel{EffectKind::contingency, "Contingency",
                "A stored spell takes effect when its trigger happens."},
    EffectLabel{EffectKind::eye_panic, "Frightened (Eyebite)",
                "Flees the caster with the Dash action until it is 60 feet away and out of sight."},
    EffectLabel{EffectKind::sickened, "Poisoned (Eyebite)",
                "Disadvantage on attack rolls and ability checks."},
    EffectLabel{EffectKind::eyebiting, "Eyebite", "Can target another creature as a Magic action."},
    EffectLabel{EffectKind::stoning, "Restrained (Flesh to Stone)",
                "Turning to stone: three failed Constitution saves petrify it."},
    EffectLabel{EffectKind::petrified, "Petrified",
                "Turned to stone: Incapacitated and unable to move."},
    EffectLabel{EffectKind::rooted, "Rooted (Flesh to Stone)",
                "Speed 0 until the start of the caster's next turn."},
    EffectLabel{EffectKind::dancing, "Charmed (Irresistible Dance)",
                "Charmed and dancing, using its movement to dance."},
    EffectLabel{EffectKind::jigging, "Dancing", "Dances in place until the end of its next turn."},
    EffectLabel{EffectKind::jarred, "Body left behind (Magic Jar)",
                "Lies helpless while its soul possesses another creature."},
    EffectLabel{EffectKind::aloft, "Fallen upward (Reverse Gravity)",
                "Out of melee reach until the spell ends."},
    EffectLabel{EffectKind::rising, "Rising as a Zombie",
                "Rises at the start of its slayer's next turn."},
    EffectLabel{EffectKind::indigo, "Restrained (Prismatic Spray)",
                "Turning to stone: three failed Constitution saves petrify it."},
    EffectLabel{
        EffectKind::violet, "Blinded (Prismatic Spray)",
        "Blinded until a Wisdom save at the start of the caster's next turn; a failure sends it to another plane."},
    EffectLabel{EffectKind::befuddled, "Befuddled", "Cannot cast spells or take the Magic action."},
    EffectLabel{EffectKind::mind_blank, "Mind Blank",
                "Immune to Psychic damage and the Charmed condition."},
    EffectLabel{EffectKind::mazed, "Lost in a maze",
                "Escapes with a DC 20 Intelligence (Investigation) check or when the spell ends."},
    EffectLabel{EffectKind::word_stunned, "Stunned (Power Word Stun)",
                "Stunned until it makes a Constitution save at the end of one of its turns."},
    EffectLabel{EffectKind::overchannel, "Overchannel",
                "Its next Wizard spell deals maximum damage."},
    EffectLabel{EffectKind::overchanneled, "Overchannel used",
                "Used {value} times since the last Long Rest; using it again harms the Wizard.",
                EffectValue::number},
    EffectLabel{EffectKind::simulacrum, "Simulacrum",
                "A Construct copy with half the original's Hit Points, healed only by repair."},
    EffectLabel{EffectKind::dragon_wings, "Dragon Wings",
                "Ignores Difficult Terrain and passes through enemies' spaces."},
    EffectLabel{EffectKind::wings_used, "Dragon Wings used",
                "Its free use returns with a Long Rest."},
    EffectLabel{EffectKind::luck_used, "Dark One's Own Luck used",
                "{value} uses spent since the last Long Rest.", EffectValue::number},
    EffectLabel{EffectKind::hurled, "Hurled through the Lower Planes",
                "Incapacitated until the end of the Warlock's next turn."},
    EffectLabel{EffectKind::hurl_used, "Hurl Through Hell used",
                "Its free use returns with a Long Rest."},
    EffectLabel{EffectKind::arcanum_used, "Mystic Arcanum used",
                "Its spells return with a Long Rest."},
    EffectLabel{EffectKind::resurged, "Wild Resurgence used",
                "The spell slot it took from Wild Shape returns with a Long Rest."},
    EffectLabel{EffectKind::natural_cast, "Natural Recovery used",
                "The free Circle spell returns with a Long Rest."},
    EffectLabel{EffectKind::fey_fright, "Frightened (Conjure Fey)",
                "Frightened of the caster until the start of its next turn."},
    EffectLabel{EffectKind::animal_shaped, "Animal Shapes", "In the form of a {value}.",
                EffectValue::beast_form},
    EffectLabel{
        EffectKind::time_slowed, "Slow",
        "Half Speed, -2 to Armor Class and Dexterity saves, no Reactions, and only an action or a Bonus Action and one attack each turn. A Wisdom save at the end of each of its turns ends it."},
    EffectLabel{EffectKind::protectors_used, "Gift of the Protectors used",
                "It can keep someone up again after a Long Rest."},
    EffectLabel{EffectKind::paralysis_ray, "Paralyzed (Wand of Paralysis)",
                "Cannot move or act. A Constitution save at the end of each of its turns ends it."},
    EffectLabel{EffectKind::giant_strength, "Giant Strength", "Strength is 21."},
    EffectLabel{EffectKind::javelin_spent, "Javelin of Lightning spent",
                "Its bolt returns with a Long Rest."},
    EffectLabel{EffectKind::enhance_ability, "Enhance Ability (Strength)",
                "Advantage on Strength checks."},
    EffectLabel{EffectKind::missile_ward, "Protection From Normal Missiles",
                "Missiles without magic from more than 5 feet away miss it."},
    EffectLabel{EffectKind::snake_charmed, "Incapacitated (Snake Charm)",
                "Charmed until the fight ends; a hit slays it."},
    EffectLabel{
        EffectKind::envenomed, "Poisoned (Cunning Strike)",
        "Disadvantage on attack rolls and ability checks. A Constitution save at the end of each of its turns ends it."},
    EffectLabel{EffectKind::enthralled, "Charmed (Enthralling Panache)",
                "Cannot attack the pirate until the start of the pirate's next turn."}};

// Bookkeeping no player sees: a save already made against an area this turn,
// a summoned spirit's breath, a casting's dice, or a double's secret. Every
// other kind has a label.
inline constexpr std::array unlabeled_effects{
    EffectKind::moonlit,     EffectKind::faith_tested, EffectKind::ensnaring,
    EffectKind::pack_tested, EffectKind::pack_moved,   EffectKind::beings_tested,
    EffectKind::guarded,     EffectKind::swarmed,      EffectKind::bladed,
    EffectKind::bathed,      EffectKind::vitality,     EffectKind::sleeted,
    EffectKind::groped,      EffectKind::wall_burned,  EffectKind::spirit_breath,
    EffectKind::breathed,    EffectKind::illusory,     EffectKind::elemental_spirit,
    EffectKind::hand_ready,  EffectKind::clouded,      EffectKind::eye_resisted,
    EffectKind::thorned,     EffectKind::swum};

// The kinds a Long Rest ends, which the sheet shows without a countdown.
inline constexpr std::array long_rest_effects{
    EffectKind::prayer_of_healing, EffectKind::javelin_spent, EffectKind::relentless_used,
    EffectKind::protectors_used,   EffectKind::overchanneled, EffectKind::wings_used,
    EffectKind::luck_used,         EffectKind::hurl_used,     EffectKind::arcanum_used,
    EffectKind::resurged,          EffectKind::natural_cast,  EffectKind::trait_uses,
    EffectKind::heroic_inspiration, EffectKind::life_drained};

// The label of a kind a player sees; nullptr for bookkeeping.
inline constexpr const EffectLabel *effect_label(EffectKind kind)
{
    const auto found = std::find_if(effect_labels.begin(), effect_labels.end(),
                                    [kind](const EffectLabel &label)
                                    {
                                        return label.kind == kind;
                                    });
    return found == effect_labels.end() ? nullptr : &*found;
}

// The SRD conditions a creature can be immune to, as effect labels name them.
inline constexpr std::array<std::string_view, 14> condition_labels{
    "Blinded",       "Charmed",   "Deafened",  "Exhaustion", "Frightened", "Grappled",  "Incapacitated",
    "Paralyzed",     "Petrified", "Poisoned",  "Prone",      "Restrained", "Stunned",   "Unconscious"};

// The label of a condition named in lower case ("restrained"); empty when it
// is no SRD condition.
inline constexpr std::string_view condition_label(std::string_view condition)
{
    for (const auto label : condition_labels)
    {
        if (label.size() == condition.size() &&
            std::equal(label.begin() + 1, label.end(), condition.begin() + 1) &&
            label.front() - 'A' + 'a' == condition.front())
        {
            return label;
        }
    }
    return {};
}

// Whether an effect of the kind imposes the condition: its label names it
// before any parenthesis ("Prone and Incapacitated (laughing)"), or it charms
// or frightens under another name.
inline constexpr bool imposes_condition(EffectKind kind, std::string_view condition)
{
    if (kind == EffectKind::dominated)
    {
        return condition == "Charmed";
    }
    if (kind == EffectKind::haunted)
    {
        return condition == "Frightened";
    }
    const auto *label = effect_label(kind);
    if (!label)
    {
        return false;
    }
    auto named = label->name.substr(0, label->name.find(" ("));
    while (!named.empty())
    {
        const auto end = named.find(" and ");
        if (named.substr(0, end) == condition)
        {
            return true;
        }
        named = end == std::string_view::npos ? std::string_view{} : named.substr(end + 5);
    }
    return false;
}

inline constexpr bool ends_with_long_rest(EffectKind kind)
{
    return std::find(long_rest_effects.begin(), long_rest_effects.end(), kind) !=
           long_rest_effects.end();
}
} // namespace opengold::srd5::detail
#endif
