#include "spell_access.h"
#include <algorithm>
#include <array>
#include <optional>
#include <set>
#include <stdexcept>

namespace opengold::srd5::detail
{
using namespace rules;

namespace
{
constexpr std::string_view source = "class:wizard:spellcasting";

// Class spell lists a row belongs to, as bits of Spell::lists. A per-row
// property means a new spell describes itself in one place.
constexpr unsigned wizard_list = 1, cleric_list = 2, paladin_list = 4, ranger_list = 8,
                   druid_list = 16, sorcerer_list = 32, warlock_list = 64, bard_list = 128;

struct Spell
{
    std::string_view id, label;
    unsigned level, mask;
    unsigned lists{wizard_list};
};

// Existing spell implementations only. This is not any class's complete list.
constexpr std::array spells{
    Spell{"chill_touch", "Chill Touch", 0, 2048, wizard_list | sorcerer_list | warlock_list},
    Spell{"shocking_grasp", "Shocking Grasp", 0, 1024, wizard_list | sorcerer_list},
    Spell{"eldritch_blast", "Eldritch Blast", 0, 512, warlock_list},
    Spell{"ray_of_frost", "Ray of Frost", 0, 256, wizard_list | sorcerer_list},
    Spell{"sacred_flame", "Sacred Flame", 0, 128, cleric_list},
    Spell{"fire_bolt", "Fire Bolt", 0, 1, wizard_list | sorcerer_list},
    Spell{"poison_spray", "Poison Spray", 0, 64,
          wizard_list | druid_list | sorcerer_list | warlock_list},
    Spell{"spare_the_dying", "Spare the Dying", 0, 0, cleric_list | druid_list},
    Spell{"magic_missile", "Magic Missile", 1, 4, wizard_list | sorcerer_list},
    Spell{"scorching_ray", "Scorching Ray", 2, 16, wizard_list | sorcerer_list},
    Spell{"blindness", "Blindness", 2, 32, wizard_list | cleric_list | sorcerer_list | bard_list},
    Spell{"inflict_wounds", "Inflict Wounds", 1, 0, cleric_list},
    Spell{"cure_wounds", "Cure Wounds", 1, 0,
          cleric_list | paladin_list | ranger_list | bard_list | druid_list},
    Spell{"healing_word", "Healing Word", 1, 0, cleric_list | bard_list | druid_list},
    Spell{"divine_smite", "Divine Smite", 1, 0, paladin_list},
    Spell{"searing_smite", "Searing Smite", 1, 0, paladin_list},
    Spell{"shining_smite", "Shining Smite", 2, 0, paladin_list},
    Spell{"shield_of_faith", "Shield of Faith", 1, 0, cleric_list | paladin_list},
    Spell{"heroism", "Heroism", 1, 0, paladin_list | bard_list},
    Spell{"divine_favor", "Divine Favor", 1, 0, paladin_list},
    Spell{"bless", "Bless", 1, 0, cleric_list | paladin_list},
    // Also on the Druid and Warlock lists; added with those classes.
    Spell{"protection_from_evil_and_good", "Protection from Evil and Good", 1, 0,
          cleric_list | paladin_list | wizard_list | warlock_list | druid_list},
    Spell{"command", "Command", 1, 0, cleric_list | paladin_list | bard_list},
    Spell{"hunters_mark", "Hunter's Mark", 1, 0, ranger_list},
    // Also on the Bard list; added with that class.
    Spell{"longstrider", "Longstrider", 1, 0, ranger_list | druid_list | wizard_list | bard_list},
    Spell{"goodberry", "Goodberry", 1, 0, ranger_list | druid_list},
    Spell{"ensnaring_strike", "Ensnaring Strike", 1, 0, ranger_list},
    Spell{"entangle", "Entangle", 1, 0, ranger_list | druid_list},
    // Also on the Sorcerer list; added with that class.
    Spell{"fog_cloud", "Fog Cloud", 1, 0, ranger_list | druid_list | wizard_list | sorcerer_list},
    Spell{"lesser_restoration", "Lesser Restoration", 2, 0,
          cleric_list | bard_list | druid_list | paladin_list | ranger_list},
    Spell{"aid", "Aid", 2, 0, cleric_list | bard_list | druid_list | paladin_list | ranger_list},
    Spell{"guiding_bolt", "Guiding Bolt", 1, 0, cleric_list},
    // Also on the Bard and Warlock lists; added with those classes.
    Spell{"bane", "Bane", 1, 0, cleric_list | warlock_list | bard_list},
    // Also on the Bard, Druid, Sorcerer and Warlock lists.
    Spell{"hold_person", "Hold Person", 2, 0,
          cleric_list | druid_list | wizard_list | sorcerer_list | warlock_list | bard_list},
    Spell{"sanctuary", "Sanctuary", 1, 0, cleric_list},
    Spell{"warding_bond", "Warding Bond", 2, 0, cleric_list | paladin_list},
    Spell{"protection_from_poison", "Protection from Poison", 2, 0,
          cleric_list | druid_list | paladin_list | ranger_list},
    Spell{"resistance", "Resistance", 0, 0, cleric_list | druid_list},
    Spell{"guidance", "Guidance", 0, 0, cleric_list | druid_list},
    // Also on the Bard list; added with that class.
    Spell{"silence", "Silence", 2, 0, cleric_list | bard_list | ranger_list},
    Spell{"spiritual_weapon", "Spiritual Weapon", 2, 0, cleric_list},
    Spell{"prayer_of_healing", "Prayer of Healing", 2, 0, cleric_list | paladin_list},
    // Also on the Sorcerer list (and Thunderwave and Shatter on the Bard's and
    // Druid's); added with those classes.
    Spell{"burning_hands", "Burning Hands", 1, 0, wizard_list | sorcerer_list},
    Spell{"thunderwave", "Thunderwave", 1, 0, wizard_list | druid_list | sorcerer_list | bard_list},
    Spell{"shatter", "Shatter", 2, 0, wizard_list | sorcerer_list | bard_list},
    // Also on the Sorcerer list (and Expeditious Retreat on the Warlock's).
    Spell{"mage_armor", "Mage Armor", 1, 0, wizard_list | sorcerer_list},
    Spell{"false_life", "False Life", 1, 0, wizard_list | sorcerer_list},
    Spell{"expeditious_retreat", "Expeditious Retreat", 1, 0,
          wizard_list | sorcerer_list | warlock_list},
    // Also on the Sorcerer list (Ice Knife on the Druid's); added with those classes.
    Spell{"ray_of_sickness", "Ray of Sickness", 1, 0, wizard_list | sorcerer_list},
    Spell{"ice_knife", "Ice Knife", 1, 0, wizard_list | druid_list | sorcerer_list},
    Spell{"chromatic_orb", "Chromatic Orb", 1, 0, wizard_list | sorcerer_list},
    Spell{"acid_splash", "Acid Splash", 0, 0, wizard_list | sorcerer_list},
    // Also on the Bard and Sorcerer lists; added with those classes.
    Spell{"sleep", "Sleep", 1, 0, wizard_list | sorcerer_list | bard_list},
    Spell{"hideous_laughter", "Hideous Laughter", 1, 0, wizard_list | warlock_list | bard_list},
    Spell{"color_spray", "Color Spray", 1, 0, wizard_list | sorcerer_list | bard_list},
    Spell{"grease", "Grease", 1, 0, wizard_list | sorcerer_list},
    Spell{"web", "Web", 2, 0, wizard_list | sorcerer_list},
    Spell{"shield", "Shield", 1, 0, wizard_list | sorcerer_list},
    Spell{"misty_step", "Misty Step", 2, 0, wizard_list | sorcerer_list | warlock_list},
    Spell{"acid_arrow", "Acid Arrow", 2, 0, wizard_list},
    // Mind Spike is also on the Sorcerer and Warlock lists, Ray of
    // Enfeeblement on the Warlock list; added with those classes.
    Spell{"mind_spike", "Mind Spike", 2, 0, wizard_list | sorcerer_list | warlock_list},
    Spell{"ray_of_enfeeblement", "Ray of Enfeeblement", 2, 0, wizard_list | warlock_list},
    // Blur and Mirror Image are also on the Sorcerer (and Bard and Warlock)
    // lists, Magic Weapon on the Paladin, Ranger and Sorcerer lists.
    Spell{"blur", "Blur", 2, 0, wizard_list | sorcerer_list},
    Spell{"mirror_image", "Mirror Image", 2, 0,
          wizard_list | sorcerer_list | warlock_list | bard_list},
    Spell{"magic_weapon", "Magic Weapon", 2, 0,
          wizard_list | sorcerer_list | paladin_list | ranger_list},
    // Also on the Bard, Sorcerer and Warlock lists; added with those classes.
    Spell{"invisibility", "Invisibility", 2, 0,
          wizard_list | sorcerer_list | warlock_list | bard_list},
    Spell{"see_invisibility", "See Invisibility", 2, 0, wizard_list | sorcerer_list | bard_list},
    Spell{"darkness", "Darkness", 2, 0, wizard_list | sorcerer_list | warlock_list},
    // Also on the Druid and Sorcerer lists; added with those classes.
    Spell{"flaming_sphere", "Flaming Sphere", 2, 0, wizard_list | sorcerer_list | druid_list},
    // Also on the Bard and Sorcerer lists; added with those classes.
    Spell{"knock", "Knock", 2, 0, wizard_list | sorcerer_list | bard_list},
    // Also on the Bard, Druid and Sorcerer lists; added with those classes.
    Spell{"enlarge_reduce", "Enlarge/Reduce", 2, 0,
          wizard_list | sorcerer_list | bard_list | druid_list},
    // Also on the Bard and Warlock lists; added with those classes.
    Spell{"true_strike", "True Strike", 0, 0,
          wizard_list | sorcerer_list | warlock_list | bard_list},
    // Also on the Sorcerer list; added with that class.
    Spell{"dragons_breath", "Dragon's Breath", 2, 0, wizard_list | sorcerer_list},
    // Also on the Bard, Druid, Sorcerer and Warlock lists; added with those classes.
    Spell{"charm_person", "Charm Person", 1, 0,
          wizard_list | sorcerer_list | warlock_list | bard_list | druid_list},
    // Also on the Druid, Ranger and Sorcerer lists; added with those classes.
    Spell{"gust_of_wind", "Gust of Wind", 2, 0,
          wizard_list | sorcerer_list | druid_list | ranger_list},
    Spell{"sorcerous_burst", "Sorcerous Burst", 0, 0, sorcerer_list},
    Spell{"hex", "Hex", 1, 0, warlock_list},
    Spell{"hellish_rebuke", "Hellish Rebuke", 1, 0, warlock_list},
    Spell{"vicious_mockery", "Vicious Mockery", 0, 0, bard_list},
    Spell{"starry_wisp", "Starry Wisp", 0, 0, bard_list | druid_list},
    Spell{"dissonant_whispers", "Dissonant Whispers", 1, 0, bard_list},
    Spell{"faerie_fire", "Faerie Fire", 1, 0, bard_list | druid_list},
    Spell{"produce_flame", "Produce Flame", 0, 0, druid_list},
    Spell{"shillelagh", "Shillelagh", 0, 0, druid_list},
    Spell{"barkskin", "Barkskin", 2, 0, druid_list | ranger_list},
    Spell{"flame_blade", "Flame Blade", 2, 0, druid_list},
    Spell{"moonbeam", "Moonbeam", 2, 0, druid_list},
    Spell{"spike_growth", "Spike Growth", 2, 0, druid_list | ranger_list},
    Spell{"heat_metal", "Heat Metal", 2, 0, druid_list | bard_list},
    Spell{"beacon_of_hope", "Beacon of Hope", 3, 0, cleric_list},
    Spell{"revivify", "Revivify", 3, 0, cleric_list | druid_list | paladin_list | ranger_list},
    Spell{"magic_circle", "Magic Circle", 3, 0,
          cleric_list | paladin_list | warlock_list | wizard_list},
    Spell{"remove_curse", "Remove Curse", 3, 0,
          cleric_list | paladin_list | warlock_list | wizard_list},
    Spell{"death_ward", "Death Ward", 4, 0, cleric_list | paladin_list},
    Spell{"aura_of_life", "Aura of Life", 4, 0, cleric_list | paladin_list},
    Spell{"freedom_of_movement", "Freedom of Movement", 4, 0,
          bard_list | cleric_list | druid_list | ranger_list},
    Spell{"guardian_of_faith", "Guardian of Faith", 4, 0, cleric_list},
    Spell{"banishment", "Banishment", 4, 0,
          cleric_list | paladin_list | sorcerer_list | warlock_list | wizard_list},
    Spell{"protection_from_energy", "Protection from Energy", 3, 0,
          cleric_list | druid_list | ranger_list | sorcerer_list | wizard_list},
    Spell{"conjure_animals", "Conjure Animals", 3, 0, druid_list | ranger_list},
    Spell{"wind_wall", "Wind Wall", 3, 0, druid_list | ranger_list},
    Spell{"plant_growth", "Plant Growth", 3, 0, bard_list | druid_list | ranger_list},
    Spell{"stoneskin", "Stoneskin", 4, 0, druid_list | ranger_list | sorcerer_list | wizard_list},
    Spell{"conjure_woodland_beings", "Conjure Woodland Beings", 4, 0, druid_list | ranger_list},
    Spell{"dominate_beast", "Dominate Beast", 4, 0, druid_list | ranger_list | sorcerer_list},
    Spell{"mass_healing_word", "Mass Healing Word", 3, 0, bard_list | cleric_list},
    Spell{"spirit_guardians", "Spirit Guardians", 3, 0, cleric_list},
    Spell{"bestow_curse", "Bestow Curse", 3, 0, bard_list | cleric_list | wizard_list},
    Spell{"animate_dead", "Animate Dead", 3, 0, cleric_list | wizard_list},
    Spell{"contagion", "Contagion", 5, 0, cleric_list | druid_list},
    Spell{"dispel_evil_and_good", "Dispel Evil and Good", 5, 0, cleric_list | paladin_list},
    Spell{"flame_strike", "Flame Strike", 5, 0, cleric_list},
    Spell{"greater_restoration", "Greater Restoration", 5, 0,
          bard_list | cleric_list | druid_list | paladin_list | ranger_list},
    Spell{"insect_plague", "Insect Plague", 5, 0, cleric_list | druid_list | sorcerer_list},
    Spell{"mass_cure_wounds", "Mass Cure Wounds", 5, 0, bard_list | cleric_list | druid_list},
    Spell{"raise_dead", "Raise Dead", 5, 0, bard_list | cleric_list | paladin_list},
    Spell{"blade_barrier", "Blade Barrier", 6, 0, cleric_list},
    Spell{"create_undead", "Create Undead", 6, 0, cleric_list | warlock_list | wizard_list},
    Spell{"harm", "Harm", 6, 0, cleric_list}, Spell{"heal", "Heal", 6, 0, cleric_list | druid_list},
    Spell{"heroes_feast", "Heroes' Feast", 6, 0, bard_list | cleric_list | druid_list},
    Spell{"sunbeam", "Sunbeam", 6, 0, cleric_list | druid_list | sorcerer_list | wizard_list},
    Spell{"true_seeing", "True Seeing", 6, 0,
          bard_list | cleric_list | sorcerer_list | warlock_list | wizard_list},
    Spell{"word_of_recall", "Word of Recall", 6, 0, cleric_list},
    Spell{"conjure_celestial", "Conjure Celestial", 7, 0, cleric_list},
    Spell{"divine_word", "Divine Word", 7, 0, cleric_list},
    Spell{"fire_storm", "Fire Storm", 7, 0, cleric_list | druid_list | sorcerer_list},
    Spell{"regenerate", "Regenerate", 7, 0, bard_list | cleric_list | druid_list},
    Spell{"resurrection", "Resurrection", 7, 0, bard_list | cleric_list},
    Spell{"antimagic_field", "Antimagic Field", 8, 0, cleric_list | wizard_list},
    Spell{"earthquake", "Earthquake", 8, 0, cleric_list | druid_list | sorcerer_list},
    Spell{"holy_aura", "Holy Aura", 8, 0, cleric_list},
    Spell{"sunburst", "Sunburst", 8, 0, cleric_list | druid_list | sorcerer_list | wizard_list},
    Spell{"fireball", "Fireball", 3, 0, sorcerer_list | wizard_list},
    Spell{"lightning_bolt", "Lightning Bolt", 3, 0, sorcerer_list | wizard_list},
    Spell{"counterspell", "Counterspell", 3, 0, sorcerer_list | warlock_list | wizard_list},
    Spell{"haste", "Haste", 3, 0, sorcerer_list | wizard_list},
    Spell{"hypnotic_pattern", "Hypnotic Pattern", 3, 0,
          bard_list | sorcerer_list | warlock_list | wizard_list},
    Spell{"fear", "Fear", 3, 0, bard_list | sorcerer_list | warlock_list | wizard_list},
    Spell{"sleet_storm", "Sleet Storm", 3, 0, druid_list | sorcerer_list | wizard_list},
    Spell{"stinking_cloud", "Stinking Cloud", 3, 0, bard_list | sorcerer_list | wizard_list},
    Spell{"vampiric_touch", "Vampiric Touch", 3, 0, sorcerer_list | warlock_list | wizard_list},
    Spell{"blink", "Blink", 3, 0, sorcerer_list | wizard_list},
    Spell{"tiny_hut", "Tiny Hut", 3, 0, bard_list | wizard_list},
    Spell{"black_tentacles", "Black Tentacles", 4, 0, wizard_list},
    Spell{"blight", "Blight", 4, 0, druid_list | sorcerer_list | warlock_list | wizard_list},
    Spell{"charm_monster", "Charm Monster", 4, 0,
          bard_list | druid_list | sorcerer_list | warlock_list | wizard_list},
    Spell{"confusion", "Confusion", 4, 0, bard_list | druid_list | sorcerer_list | wizard_list},
    Spell{"dimension_door", "Dimension Door", 4, 0,
          bard_list | sorcerer_list | warlock_list | wizard_list},
    Spell{"fire_shield", "Fire Shield", 4, 0, druid_list | sorcerer_list | wizard_list},
    Spell{"greater_invisibility", "Greater Invisibility", 4, 0,
          bard_list | sorcerer_list | wizard_list},
    Spell{"ice_storm", "Ice Storm", 4, 0, druid_list | sorcerer_list | wizard_list},
    Spell{"phantasmal_killer", "Phantasmal Killer", 4, 0, bard_list | wizard_list},
    Spell{"polymorph", "Polymorph", 4, 0, bard_list | druid_list | sorcerer_list | wizard_list},
    Spell{"resilient_sphere", "Resilient Sphere", 4, 0, wizard_list},
    Spell{"vitriolic_sphere", "Vitriolic Sphere", 4, 0, sorcerer_list | wizard_list},
    Spell{"wall_of_fire", "Wall of Fire", 4, 0, druid_list | sorcerer_list | wizard_list},
    Spell{"faithful_hound", "Faithful Hound", 4, 0, wizard_list},
    Spell{"conjure_minor_elementals", "Conjure Minor Elementals", 4, 0, druid_list | wizard_list},
    Spell{"arcane_hand", "Arcane Hand", 5, 0, sorcerer_list | wizard_list},
    Spell{"cloudkill", "Cloudkill", 5, 0, sorcerer_list | wizard_list},
    Spell{"cone_of_cold", "Cone of Cold", 5, 0, druid_list | sorcerer_list | wizard_list},
    Spell{"conjure_elemental", "Conjure Elemental", 5, 0, druid_list | wizard_list},
    Spell{"dominate_person", "Dominate Person", 5, 0, bard_list | sorcerer_list | wizard_list},
    Spell{"hold_monster", "Hold Monster", 5, 0,
          bard_list | sorcerer_list | warlock_list | wizard_list},
    Spell{"summon_dragon", "Summon Dragon", 5, 0, wizard_list},
    Spell{"wall_of_force", "Wall of Force", 5, 0, wizard_list},
    Spell{"wall_of_stone", "Wall of Stone", 5, 0, druid_list | sorcerer_list | wizard_list},
    Spell{"animate_objects", "Animate Objects", 5, 0, bard_list | sorcerer_list | wizard_list},
    Spell{"mislead", "Mislead", 5, 0, bard_list | warlock_list | wizard_list},
    Spell{"telekinesis", "Telekinesis", 5, 0, sorcerer_list | wizard_list},
    Spell{"chain_lightning", "Chain Lightning", 6, 0, sorcerer_list | wizard_list},
    Spell{"circle_of_death", "Circle of Death", 6, 0, sorcerer_list | warlock_list | wizard_list},
    Spell{"contingency", "Contingency", 6, 0, wizard_list},
    Spell{"disintegrate", "Disintegrate", 6, 0, sorcerer_list | wizard_list},
    Spell{"eyebite", "Eyebite", 6, 0, bard_list | sorcerer_list | warlock_list | wizard_list},
    Spell{"flesh_to_stone", "Flesh to Stone", 6, 0, druid_list | sorcerer_list | wizard_list},
    Spell{"freezing_sphere", "Freezing Sphere", 6, 0, sorcerer_list | wizard_list},
    Spell{"globe_of_invulnerability", "Globe of Invulnerability", 6, 0,
          sorcerer_list | wizard_list},
    Spell{"irresistible_dance", "Irresistible Dance", 6, 0, bard_list | wizard_list},
    Spell{"magic_jar", "Magic Jar", 6, 0, wizard_list},
    Spell{"wall_of_ice", "Wall of Ice", 6, 0, wizard_list},
    Spell{"arcane_sword", "Arcane Sword", 7, 0, bard_list | wizard_list},
    Spell{"delayed_blast_fireball", "Delayed Blast Fireball", 7, 0, sorcerer_list | wizard_list},
    Spell{"finger_of_death", "Finger of Death", 7, 0, sorcerer_list | warlock_list | wizard_list},
    Spell{"forcecage", "Forcecage", 7, 0, bard_list | warlock_list | wizard_list},
    Spell{"prismatic_spray", "Prismatic Spray", 7, 0, bard_list | sorcerer_list | wizard_list},
    Spell{"reverse_gravity", "Reverse Gravity", 7, 0, druid_list | sorcerer_list | wizard_list},
    Spell{"simulacrum", "Simulacrum", 7, 0, wizard_list},
    Spell{"call_lightning", "Call Lightning", 3, 0, druid_list},
    Spell{"giant_insect", "Giant Insect", 4, 0, druid_list},
    Spell{"antilife_shell", "Antilife Shell", 5, 0, druid_list},
    Spell{"conjure_fey", "Conjure Fey", 6, 0, druid_list},
    Spell{"wall_of_thorns", "Wall of Thorns", 6, 0, druid_list},
    Spell{"animal_shapes", "Animal Shapes", 8, 0, druid_list},
    Spell{"slow", "Slow", 3, 0, bard_list | sorcerer_list | wizard_list},
    Spell{"tsunami", "Tsunami", 8, 0, druid_list},
    Spell{"reincarnate", "Reincarnate", 5, 0, druid_list},
    Spell{"befuddlement", "Befuddlement", 8, 0,
          bard_list | druid_list | warlock_list | wizard_list},
    Spell{"dominate_monster", "Dominate Monster", 8, 0,
          bard_list | sorcerer_list | warlock_list | wizard_list},
    Spell{"incendiary_cloud", "Incendiary Cloud", 8, 0, druid_list | sorcerer_list | wizard_list},
    Spell{"maze", "Maze", 8, 0, wizard_list},
    Spell{"mind_blank", "Mind Blank", 8, 0, bard_list | wizard_list},
    Spell{"power_word_stun", "Power Word Stun", 8, 0,
          bard_list | sorcerer_list | warlock_list | wizard_list},
    Spell{"dispel_magic", "Dispel Magic", 3, 0,
          bard_list | cleric_list | druid_list | paladin_list | ranger_list | sorcerer_list |
              warlock_list | wizard_list}};

// A class that prepares spells from its whole class list instead of a
// spellbook. Arrays are indexed by class level minus one (levels 1-4).
struct PreparedCaster
{
    CharacterClass klass;
    std::string_view source, cantrip_label;
    // The feature that grants the spells: Spellcasting, or a Warlock's Pact Magic.
    std::string_view feature;
    unsigned list, cantrip_list;
    // By class level, 1 to 15 (SRD 5.2.1 class tables).
    std::array<unsigned, 15> cantrips, prepared, highest_slot;
    // What a Long Rest may change: a Cleric any prepared spell, a Paladin or
    // Ranger one, a Sorcerer none (it changes spells only on gaining a level).
    enum class RestChange
    {
        any,
        one,
        none
    } rest_change;
};

constexpr std::array prepared_casters{
    PreparedCaster{CharacterClass::cleric,
                   "class:cleric:spellcasting",
                   "Cleric cantrips",
                   "feature:spellcasting",
                   cleric_list,
                   cleric_list,
                   {3, 3, 3, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5},
                   {4, 5, 6, 7, 9, 10, 11, 12, 14, 15, 16, 16, 17, 17, 18},
                   {1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8},
                   PreparedCaster::RestChange::any},
    PreparedCaster{// Paladin cantrips come only from Blessed Warrior, from the Cleric list.
                   CharacterClass::paladin,
                   "class:paladin:spellcasting",
                   "Blessed Warrior cantrips",
                   "feature:spellcasting",
                   paladin_list,
                   cleric_list,
                   {},
                   {2, 3, 4, 5, 6, 6, 7, 7, 9, 9, 10, 10, 11, 11, 12},
                   {1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4},
                   PreparedCaster::RestChange::one},
    // Ranger cantrips come only from Druidic Warrior, from the Druid list.
    PreparedCaster{CharacterClass::ranger,
                   "class:ranger:spellcasting",
                   "Druidic Warrior cantrips",
                   "feature:spellcasting",
                   ranger_list,
                   druid_list,
                   {},
                   {2, 3, 4, 5, 6, 6, 7, 7, 9, 9, 10, 10, 11, 11, 12},
                   {1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4},
                   PreparedCaster::RestChange::one},
    PreparedCaster{CharacterClass::sorcerer,
                   "class:sorcerer:spellcasting",
                   "Sorcerer cantrips",
                   "feature:spellcasting",
                   sorcerer_list,
                   sorcerer_list,
                   {4, 4, 4, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6},
                   {2, 4, 6, 7, 9, 10, 11, 12, 14, 15, 16, 16, 17, 17, 18},
                   {1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8},
                   PreparedCaster::RestChange::none},
    // Pact Magic: a Warlock prepares from its list; its slots are all one level.
    PreparedCaster{CharacterClass::warlock,
                   "class:warlock:pact_magic",
                   "Warlock cantrips",
                   "feature:pact_magic",
                   warlock_list,
                   warlock_list,
                   {2, 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4},
                   {2, 3, 4, 5, 6, 7, 8, 9, 10, 10, 11, 11, 12, 12, 13},
                   {1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 5, 5, 5, 5, 5},
                   PreparedCaster::RestChange::none},
    PreparedCaster{CharacterClass::bard,
                   "class:bard:spellcasting",
                   "Bard cantrips",
                   "feature:spellcasting",
                   bard_list,
                   bard_list,
                   {2, 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4},
                   {4, 5, 6, 7, 9, 10, 11, 12, 14, 15, 16, 16, 17, 17, 18},
                   {1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8},
                   PreparedCaster::RestChange::none},
    PreparedCaster{CharacterClass::druid,
                   "class:druid:spellcasting",
                   "Druid cantrips",
                   "feature:spellcasting",
                   druid_list,
                   druid_list,
                   {2, 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4},
                   {4, 5, 6, 7, 9, 10, 11, 12, 14, 15, 16, 16, 17, 17, 18},
                   {1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8},
                   PreparedCaster::RestChange::any}};

// The lists a caster prepares from: its own, and from Bard 10 (Magical
// Secrets) the Cleric, Druid and Wizard lists too.
unsigned prepared_list(const PreparedCaster &caster, unsigned level)
{
    return caster.list | (caster.klass == CharacterClass::bard && level >= 10
                              ? cleric_list | druid_list | wizard_list
                              : 0u);
}

// Spells a class always has prepared from a class level on. `land` names the
// Circle of the Land type whose Circle Spells these are.
struct AlwaysPrepared
{
    CharacterClass klass;
    std::string_view spell;
    unsigned level;
    std::string_view land{};
};

// Paladin's Smite, SRD 5.2.1 p. 54, the Oath of Devotion spells, p. 56, the
// Ranger's Favored Enemy and the Cleric's Life Domain.
constexpr std::array always_prepared_table{
    AlwaysPrepared{CharacterClass::paladin, "divine_smite", 2},
    // Circle of the Land spells, p. 46, for the land chosen.
    AlwaysPrepared{CharacterClass::druid, "blur", 3, "arid"},
    AlwaysPrepared{CharacterClass::druid, "burning_hands", 3, "arid"},
    AlwaysPrepared{CharacterClass::druid, "fire_bolt", 3, "arid"},
    AlwaysPrepared{CharacterClass::druid, "fog_cloud", 3, "polar"},
    AlwaysPrepared{CharacterClass::druid, "hold_person", 3, "polar"},
    AlwaysPrepared{CharacterClass::druid, "ray_of_frost", 3, "polar"},
    AlwaysPrepared{CharacterClass::druid, "misty_step", 3, "temperate"},
    AlwaysPrepared{CharacterClass::druid, "shocking_grasp", 3, "temperate"},
    AlwaysPrepared{CharacterClass::druid, "sleep", 3, "temperate"},
    AlwaysPrepared{CharacterClass::druid, "acid_splash", 3, "tropical"},
    AlwaysPrepared{CharacterClass::druid, "ray_of_sickness", 3, "tropical"},
    AlwaysPrepared{CharacterClass::druid, "web", 3, "tropical"},
    // The lands' later Circle Spells (SRD 5.2.1 p. 46); temperate's Tree
    // Stride is cut.
    AlwaysPrepared{CharacterClass::druid, "fireball", 5, "arid"},
    AlwaysPrepared{CharacterClass::druid, "blight", 7, "arid"},
    AlwaysPrepared{CharacterClass::druid, "wall_of_stone", 9, "arid"},
    AlwaysPrepared{CharacterClass::druid, "sleet_storm", 5, "polar"},
    AlwaysPrepared{CharacterClass::druid, "ice_storm", 7, "polar"},
    AlwaysPrepared{CharacterClass::druid, "cone_of_cold", 9, "polar"},
    AlwaysPrepared{CharacterClass::druid, "lightning_bolt", 5, "temperate"},
    AlwaysPrepared{CharacterClass::druid, "freedom_of_movement", 7, "temperate"},
    AlwaysPrepared{CharacterClass::druid, "stinking_cloud", 5, "tropical"},
    AlwaysPrepared{CharacterClass::druid, "polymorph", 7, "tropical"},
    AlwaysPrepared{CharacterClass::druid, "insect_plague", 9, "tropical"},
    // Fiend Spells, p. 76 (Suggestion removed by DM-3).
    AlwaysPrepared{CharacterClass::warlock, "burning_hands", 3},
    AlwaysPrepared{CharacterClass::warlock, "command", 3},
    AlwaysPrepared{CharacterClass::warlock, "scorching_ray", 3},
    // Draconic Spells, p. 70 (Alter Self removed by DM-3).
    AlwaysPrepared{CharacterClass::sorcerer, "chromatic_orb", 3},
    AlwaysPrepared{CharacterClass::sorcerer, "command", 3},
    AlwaysPrepared{CharacterClass::sorcerer, "dragons_breath", 3},
    // Fiend Spells (SRD 5.2.1 p. 76): Geas is cut.
    AlwaysPrepared{CharacterClass::warlock, "fireball", 5},
    AlwaysPrepared{CharacterClass::warlock, "stinking_cloud", 5},
    AlwaysPrepared{CharacterClass::warlock, "fire_shield", 7},
    AlwaysPrepared{CharacterClass::warlock, "wall_of_fire", 7},
    AlwaysPrepared{CharacterClass::warlock, "insect_plague", 9},
    // Draconic Spells (SRD 5.2.1 p. 70): Fly, Arcane Eye and Legend Lore are cut.
    AlwaysPrepared{CharacterClass::sorcerer, "fear", 5},
    AlwaysPrepared{CharacterClass::sorcerer, "charm_monster", 7},
    AlwaysPrepared{CharacterClass::sorcerer, "summon_dragon", 9},
    AlwaysPrepared{CharacterClass::paladin, "protection_from_evil_and_good", 3},
    AlwaysPrepared{CharacterClass::paladin, "shield_of_faith", 3},
    // Zone of Truth, the other level-5 oath spell, is cut (DM-1).
    AlwaysPrepared{CharacterClass::paladin, "aid", 5},
    AlwaysPrepared{CharacterClass::paladin, "beacon_of_hope", 9},
    AlwaysPrepared{CharacterClass::paladin, "dispel_magic", 9},
    AlwaysPrepared{CharacterClass::paladin, "freedom_of_movement", 13},
    AlwaysPrepared{CharacterClass::paladin, "guardian_of_faith", 13},
    // Favored Enemy, SRD 5.2.1 p. 57.
    AlwaysPrepared{CharacterClass::ranger, "hunters_mark", 1},
    // Life Domain spells, p. 37.
    AlwaysPrepared{CharacterClass::cleric, "aid", 3},
    AlwaysPrepared{CharacterClass::cleric, "bless", 3},
    AlwaysPrepared{CharacterClass::cleric, "cure_wounds", 3},
    AlwaysPrepared{CharacterClass::cleric, "lesser_restoration", 3},
    AlwaysPrepared{CharacterClass::cleric, "mass_healing_word", 5},
    AlwaysPrepared{CharacterClass::cleric, "revivify", 5},
    AlwaysPrepared{CharacterClass::cleric, "aura_of_life", 7},
    AlwaysPrepared{CharacterClass::cleric, "death_ward", 7},
    AlwaysPrepared{CharacterClass::cleric, "greater_restoration", 9},
    AlwaysPrepared{CharacterClass::cleric, "mass_cure_wounds", 9}};

const PreparedCaster *prepared_caster(CharacterClass klass)
{
    for (const auto &caster : prepared_casters)
    {
        if (caster.klass == klass)
        {
            return &caster;
        }
    }
    return nullptr;
}

void require(bool ok)
{
    if (!ok)
    {
        throw std::runtime_error("Invalid spell grant, spellbook entry or preparation");
    }
}

const Spell &find(std::string_view id)
{
    for (const auto &spell : spells)
    {
        if (spell.id == id)
        {
            return spell;
        }
    }
    throw std::runtime_error("Unsupported spell knowledge");
}

FeatureGrant grant(std::string_view id, unsigned level, std::string_view origin = source)
{
    return {"spell:" + std::string(id),
            std::string(origin),
            level,
            {{"access", find(id).level ? "spellbook" : "cantrip"}}};
}

std::string_view class_source(CharacterClass klass)
{
    const auto *caster = prepared_caster(klass);
    return caster ? caster->source : source;
}

// The Druid's Magician Primal Order adds one Druid cantrip.
bool magician(std::span<const FeatureGrant> grants)
{
    return std::find(grants.begin(), grants.end(),
                     FeatureGrant{"order:magician", "class:druid:primal_order", 1, {}}) !=
           grants.end();
}

// The Thaumaturge Divine Order adds one Cleric cantrip.
bool thaumaturge(std::span<const FeatureGrant> grants)
{
    return std::find(grants.begin(), grants.end(),
                     FeatureGrant{"order:thaumaturge", "class:cleric:divine_order", 1, {}}) !=
           grants.end();
}

// Paladin's Blessed Warrior Fighting Style adds two Cleric cantrips at level two.
bool blessed_warrior(std::span<const FeatureGrant> grants)
{
    return std::find(
               grants.begin(), grants.end(),
               FeatureGrant{"feature:blessed_warrior", "class:paladin:fighting_style", 2, {}}) !=
           grants.end();
}

// The Ranger's Druidic Warrior Fighting Style adds two Druid cantrips at level two.
bool druidic_warrior(std::span<const FeatureGrant> grants)
{
    return std::find(
               grants.begin(), grants.end(),
               FeatureGrant{"feature:druidic_warrior", "class:ranger:fighting_style", 2, {}}) !=
           grants.end();
}

// Magic Initiate, SRD 5.2.1 p. 87: two cantrips and one level-one spell from
// the Cleric, Druid or Wizard list, cast with the ability chosen for the feat.
// Taken again it must name another list, so each list is its own feat id and
// the grants it makes are sourced from that id.
struct InitiateList
{
    std::string_view id, label;
    unsigned list;
    unsigned cantrips{2}, spells{1};
    // Fixed for the Pact of the Tome; Magic Initiate's is chosen.
    std::optional<Ability> ability;
    bool free_cast{true}; // the level-one spell once per Long Rest without a slot
};

constexpr std::array initiate_lists{InitiateList{"cleric", "Cleric", cleric_list},
                                    InitiateList{"druid", "Druid", druid_list},
                                    InitiateList{"wizard", "Wizard", wizard_list}};

// The Warlock's Pact of the Tome, SRD 5.2.1 p. 73: three cantrips and two
// level-one spells from any class's list, prepared as Warlock spells. Its
// ritual requirement is waived (TOME-1), since the game has no ritual spells.
constexpr std::string_view tome_id = "invocation:pact_of_the_tome";
constexpr unsigned any_list = wizard_list | cleric_list | paladin_list | ranger_list | druid_list |
                              sorcerer_list | warlock_list | bard_list;
constexpr InitiateList tome{
    "pact_of_the_tome", "Pact of the Tome", any_list, 3, 2, Ability::charisma, false};

constexpr std::string_view initiate_prefix = "feat:magic_initiate_";
constexpr std::string_view ability_prefix = "spellcasting_ability:";
constexpr std::array<std::pair<std::string_view, std::string_view>, 3> initiate_abilities{
    {{"intelligence", "Intelligence"}, {"wisdom", "Wisdom"}, {"charisma", "Charisma"}}};

const InitiateList *initiate_list(std::string_view feat)
{
    if (feat == tome_id)
    {
        return &tome;
    }
    if (!feat.starts_with(initiate_prefix))
    {
        return nullptr;
    }
    feat.remove_prefix(initiate_prefix.size());
    for (const auto &list : initiate_lists)
    {
        if (list.id == feat)
        {
            return &list;
        }
    }
    return nullptr;
}

// A class's own spell grant; Magic Initiate's spells answer to the feat instead.
bool class_spell_grant(const FeatureGrant &grant)
{
    return is_spell_grant(grant) && !is_initiate_grant(grant);
}

// The Magic Initiate feats held, as (feat id, acquisition level), in grant
// order: from a background or level four, or the Warlock's Lessons of the
// First Ones, which grants the feat its id names. The Pact of the Tome counts
// as one, under its invocation's id.
std::vector<std::pair<std::string, unsigned>> initiate_feats(std::span<const FeatureGrant> grants)
{
    constexpr std::string_view lessons = "invocation:lessons_";
    std::vector<std::pair<std::string, unsigned>> result;
    for (const auto &g : grants)
    {
        if (initiate_list(g.id))
        {
            result.emplace_back(g.id, g.level);
        }
        else if (g.id.starts_with(lessons) && initiate_list("feat:" + g.id.substr(lessons.size())))
        {
            result.emplace_back("feat:" + g.id.substr(lessons.size()), g.level);
        }
    }
    return result;
}

FeatureGrant initiate_spell_grant(std::string_view feat, std::string_view spell, unsigned level)
{
    return {"spell:" + std::string(spell),
            std::string(feat),
            level,
            {{"access", find(spell).level ? "always_prepared" : "cantrip"}}};
}

// Checks each feat's grants and adds its cantrips and always-prepared spell.
void add_initiate_access(SpellAccess &result, std::span<const FeatureGrant> grants, unsigned level)
{
    std::set<std::string> known;
    for (const auto &g : grants)
    {
        if (is_spell_grant(g))
        {
            require(known.insert(g.id).second);
        }
    }
    const auto feats = initiate_feats(grants);
    std::set<std::string> lists;
    for (const auto &feat : feats)
    {
        require(lists.insert(feat.first).second);
    }
    for (const auto &g : grants)
    {
        if (!is_initiate_grant(g))
        {
            continue;
        }
        const auto feat = std::find_if(feats.begin(), feats.end(),
                                       [&](const auto &f)
                                       {
                                           return f.first == g.source_id;
                                       });
        require(feat != feats.end() && g.level == feat->second && g.level <= level);
        if (g.id.starts_with(ability_prefix))
        {
            const auto ability = std::string_view(g.id).substr(ability_prefix.size());
            require(g.choices.empty() &&
                    std::any_of(initiate_abilities.begin(), initiate_abilities.end(),
                                [&](const auto &a)
                                {
                                    return a.first == ability;
                                }));
            continue;
        }
        require(is_spell_grant(g));
        const auto &spell = find(std::string_view(g.id).substr(6));
        require((spell.lists & initiate_list(g.source_id)->list) && spell.level <= 1 &&
                g == initiate_spell_grant(g.source_id, spell.id, g.level));
        result.initiate.push_back(
            {std::string(spell.id), std::string(spell.label), g.source_id, g.level});
        if (spell.level && std::find(result.always_prepared.begin(), result.always_prepared.end(),
                                     spell.id) == result.always_prepared.end())
        {
            result.always_prepared.emplace_back(spell.id);
        }
    }
    for (const auto &[feat, acquired] : feats)
    {
        unsigned abilities = 0, cantrips = 0, leveled = 0;
        for (const auto &g : grants)
        {
            if (g.source_id == feat)
            {
                if (g.id.starts_with(ability_prefix))
                {
                    ++abilities;
                }
                else if (find(std::string_view(g.id).substr(6)).level)
                {
                    ++leveled;
                }
                else
                {
                    ++cantrips;
                }
            }
        }
        const auto &list = *initiate_list(feat);
        require(abilities <= (list.ability ? 0u : 1u) && cantrips <= list.cantrips &&
                leveled <= list.spells);
    }
}

// What each Magic Initiate feat acquired at the sheet's level still has to choose.
void add_initiate_learning(SpellChoiceOptions &result, const CharacterSheet &sheet)
{
    for (const auto &[feat, acquired] : initiate_feats(sheet.grants))
    {
        if (acquired != static_cast<unsigned>(sheet.level))
        {
            continue;
        }
        const auto &list = *initiate_list(feat);
        const auto name = list.ability ? std::string(list.label)
                                       : "Magic Initiate (" + std::string(list.label) + ")";
        const auto mine = [&](auto predicate)
        {
            return static_cast<unsigned>(std::count_if(sheet.grants.begin(), sheet.grants.end(),
                                                       [&](const auto &g)
                                                       {
                                                           return g.source_id == feat &&
                                                                  predicate(g);
                                                       }));
        };
        const auto known = [&](std::string_view id)
        {
            return std::any_of(sheet.grants.begin(), sheet.grants.end(),
                               [&](const auto &g)
                               {
                                   return g.id == "spell:" + std::string(id);
                               });
        };
        const bool ability_chosen = mine(
                                        [](const auto &g)
                                        {
                                            return g.id.starts_with(ability_prefix);
                                        }) > 0;
        if (!ability_chosen && !list.ability)
        {
            TrainingChoiceGroup group{feat + ":ability", name + ": spellcasting ability", 1, {}};
            group.acquired_level = acquired;
            // The class's own spellcasting ability first, so a ready-made
            // character's first pick is the one its class already raises.
            using enum CharacterClass;
            const auto klass = class_of(sheet);
            const std::string_view first =
                klass == cleric || klass == druid || klass == ranger ? "wisdom"
                : klass == bard || klass == paladin || klass == sorcerer || klass == warlock
                    ? "charisma"
                    : "intelligence";
            auto abilities = initiate_abilities;
            std::stable_partition(abilities.begin(), abilities.end(),
                                  [&](const auto &a)
                                  {
                                      return a.first == first;
                                  });
            for (const auto &[id, label] : abilities)
            {
                group.options.push_back({std::string(id), std::string(label),
                                         "Spell attacks and save DCs for these spells."});
            }
            result.learning.push_back(std::move(group));
        }
        for (const unsigned spell_level : {0u, 1u})
        {
            const unsigned owed = spell_level ? list.spells : list.cantrips;
            const auto have = mine(
                [&](const auto &g)
                {
                    return is_spell_grant(g) &&
                           find(std::string_view(g.id).substr(6)).level == spell_level;
                });
            if (have >= owed)
            {
                continue;
            }
            TrainingChoiceGroup group{
                feat + (spell_level ? ":spell" : ":cantrips"),
                name + (spell_level ? list.spells > 1 ? " level 1 spells" : " level 1 spell"
                                    : " cantrips"),
                owed - have,
                {}};
            group.acquired_level = acquired;
            for (const auto &spell : spells)
            {
                if ((spell.lists & list.list) && spell.level == spell_level && !known(spell.id))
                {
                    group.options.push_back({std::string(spell.id), std::string(spell.label), {}});
                }
            }
            // A character who knows all but one of the list's cantrips in the
            // game learns only that one (INITIATE-1).
            group.count = std::min(group.count, static_cast<unsigned>(group.options.size()));
            if (group.count)
            {
                result.learning.push_back(std::move(group));
            }
        }
    }
}

// The grant a Magic Initiate learning group's pick makes, or none for another group.
std::optional<FeatureGrant> initiate_choice_grant(const TrainingChoiceGroup &group,
                                                  std::string_view value)
{
    const auto split = group.id.rfind(':');
    if (split == std::string::npos || !initiate_list(std::string_view(group.id).substr(0, split)))
    {
        return std::nullopt;
    }
    auto feat = group.id.substr(0, split);
    if (group.id.ends_with(":ability"))
    {
        return FeatureGrant{std::string(ability_prefix) + std::string(value),
                            std::move(feat),
                            group.acquired_level,
                            {}};
    }
    return initiate_spell_grant(feat, value, group.acquired_level);
}

// The cantrips a caster knows at a class level, its features included.
unsigned cantrips_at(const PreparedCaster &caster, std::span<const FeatureGrant> grants,
                     unsigned level)
{
    return caster.cantrips[level - 1] +
           (caster.klass == CharacterClass::cleric && thaumaturge(grants) ? 1 : 0) +
           (caster.klass == CharacterClass::druid && magician(grants) ? 1 : 0) +
           (caster.klass == CharacterClass::paladin && level >= 2 && blessed_warrior(grants) ? 2
                                                                                             : 0) +
           (caster.klass == CharacterClass::ranger && level >= 2 && druidic_warrior(grants) ? 2
                                                                                            : 0);
}

SpellAccess prepared_access(const PreparedCaster &caster, std::span<const FeatureGrant> grants,
                            unsigned level, std::span<const std::string> prepared)
{
    require(level >= 1 && level <= level_cap(caster.klass));
    // "class:cleric:spellcasting" is granted by the "class:cleric" Spellcasting feature.
    const std::string feature_source(caster.source.substr(0, caster.source.rfind(':')));
    require(std::find(grants.begin(), grants.end(),
                      FeatureGrant{std::string(caster.feature), feature_source, 1, {}}) !=
            grants.end());
    SpellAccess result;
    result.cantrip_choices = cantrips_at(caster, grants, level);
    result.prepared_choices = caster.prepared[level - 1];
    result.always_prepared = always_prepared_spells(caster.klass, level, grants);
    std::set<std::string> known;
    std::array<unsigned, 16> learned{};
    for (const auto &g : grants)
    {
        if (class_spell_grant(g))
        {
            const auto &spell = find(std::string_view(g.id).substr(6));
            // Cantrips are learned at level one or at a level whose count grows.
            const bool learning_level =
                g.level == 1 ||
                (g.level >= 2 && g.level <= level &&
                 cantrips_at(caster, grants, g.level) > cantrips_at(caster, grants, g.level - 1));
            require((spell.lists & caster.cantrip_list) && spell.level == 0 && learning_level &&
                    g.level <= level && known.insert(g.id).second &&
                    g == grant(spell.id, g.level, caster.source));
            result.cantrips.push_back(
                {std::string(spell.id), std::string(spell.label), g.source_id, g.level});
            ++learned[g.level];
        }
    }
    require(learned[1] <= cantrips_at(caster, grants, 1));
    for (unsigned n = 2; n <= level; ++n)
    {
        require(learned[n] <= cantrips_at(caster, grants, n) - cantrips_at(caster, grants, n - 1));
    }
    std::set<std::string> selected;
    for (const auto &id : prepared)
    {
        const auto &spell = find(id);
        require((spell.lists & prepared_list(caster, level)) && spell.level >= 1 &&
                spell.level <= caster.highest_slot[level - 1] && selected.insert(id).second);
        result.prepared.push_back(id);
    }
    require(result.prepared.size() <= result.prepared_choices);
    return result;
}

// Gaining a level whose cantrip count grows learns cantrips; a Long Rest
// changes prepared spells. Creation is level one. Replacing a cantrip on
// gaining a level waits for a second implemented cantrip on the class list.
SpellChoiceOptions prepared_choice_options(const PreparedCaster &caster,
                                           const CharacterSheet &sheet, SpellChoiceContext context)
{
    SpellChoiceOptions result;
    const auto access =
        spell_access(sheet.grants, caster.klass, sheet.level, sheet.prepared_spells);
    const auto known = [&](std::string_view id)
    {
        const auto named = [&](const auto &s)
        {
            return s.id == id;
        };
        return std::any_of(access.cantrips.begin(), access.cantrips.end(), named) ||
               std::any_of(access.initiate.begin(), access.initiate.end(), named);
    };
    const unsigned level = sheet.level;
    const unsigned capacity = level > 1 ? cantrips_at(caster, sheet.grants, level) -
                                              cantrips_at(caster, sheet.grants, level - 1)
                                        : 0;
    if (context == SpellChoiceContext::advancement && capacity)
    {
        const auto used =
            static_cast<unsigned>(std::count_if(sheet.grants.begin(), sheet.grants.end(),
                                                [&](const auto &g)
                                                {
                                                    return class_spell_grant(g) && g.level == level;
                                                }));
        if (used < capacity)
        {
            TrainingChoiceGroup group{"cantrips:" + std::to_string(level),
                                      std::string(caster.cantrip_label),
                                      capacity - used,
                                      {}};
            group.acquired_level = level;
            for (const auto &spell : spells)
            {
                if ((spell.lists & caster.cantrip_list) && spell.level == 0 && !known(spell.id))
                {
                    group.options.push_back({std::string(spell.id), std::string(spell.label), {}});
                }
            }
            result.learning.push_back(std::move(group));
        }
    }
    result.prepared_count = access.prepared_choices;
    // Only a Wizard's Memorize Spell changes prepared spells on a Short Rest.
    result.may_prepare = context == SpellChoiceContext::advancement ||
                         (context == SpellChoiceContext::long_rest &&
                          caster.rest_change != PreparedCaster::RestChange::none);
    for (const auto &spell : spells)
    {
        if ((spell.lists & prepared_list(caster, level)) && spell.level >= 1 &&
            spell.level <= caster.highest_slot[level - 1] &&
            std::find(access.always_prepared.begin(), access.always_prepared.end(), spell.id) ==
                access.always_prepared.end())
        {
            result.preparation.push_back({std::string(spell.id), std::string(spell.label), {}});
        }
    }
    // A spell that becomes always prepared at this level frees its place.
    if (context == SpellChoiceContext::advancement)
    {
        for (const auto &id : sheet.prepared_spells)
        {
            if (std::find(access.always_prepared.begin(), access.always_prepared.end(), id) ==
                access.always_prepared.end())
            {
                result.locked_prepared.push_back(id);
            }
        }
    }
    return result;
}
} // namespace

bool is_spell_grant(const FeatureGrant &grant)
{
    return grant.id.starts_with("spell:");
}

bool is_initiate_grant(const FeatureGrant &grant)
{
    return grant.source_id.starts_with(initiate_prefix) || grant.source_id == tome_id;
}

std::vector<FeatureGrant> without_spell_grants(std::span<const FeatureGrant> grants)
{
    std::vector<FeatureGrant> result;
    for (const auto &g : grants)
    {
        if (!is_spell_grant(g) && !is_initiate_grant(g))
        {
            result.push_back(g);
        }
    }
    return result;
}

TrainingChoiceGroup starting_cantrip_options(CharacterClass klass)
{
    if (klass == CharacterClass::warlock)
    {
        return {
            "class:warlock:pact_magic",
            "Warlock cantrips",
            2,
            {{"eldritch_blast", "Eldritch Blast",
              "Ranged spell attack: 1d10 Force damage, 120 feet; creature targets currently supported."},
             {"poison_spray", "Poison Spray", "Ranged spell attack: 1d12 Poison damage, 30 feet."},
             {"chill_touch", "Chill Touch",
              "Melee spell attack: 1d10 Necrotic damage, Touch; prevents healing until the end of your next turn."},
             {"true_strike", "True Strike",
              "Melee weapon attack using your spellcasting ability; Radiant or the weapon's damage type."}}};
    }
    if (klass == CharacterClass::druid)
    {
        return {
            "class:druid:spellcasting",
            "Druid cantrips",
            2,
            {{"poison_spray", "Poison Spray", "Ranged spell attack: 1d12 Poison damage, 30 feet."},
             {"produce_flame", "Produce Flame",
              "Bonus Action: a flame in your hand; an Action hurls it, a ranged spell attack for 1d8 Fire, 60 feet."},
             {"resistance", "Resistance",
              "Touch: once per turn the creature takes 1d4 less damage of a chosen type; Concentration."},
             {"shillelagh", "Shillelagh",
              "Bonus Action: your Club or Quarterstaff uses Wisdom and a d8 for a minute."},
             {"spare_the_dying", "Spare the Dying",
              "A creature at 0 Hit Points within 15 feet becomes Stable."},
             {"starry_wisp", "Starry Wisp",
              "Ranged spell attack: 1d8 Radiant damage, 60 feet; the target cannot be Invisible until the end of your next turn."},
             // Last, so ready-made Druids keep the cantrips they had before it (GUIDE-1).
             {"guidance", "Guidance",
              "Touch: the creature adds 1d4 to Athletics or Medicine checks, as chosen; Concentration."}}};
    }
    if (klass == CharacterClass::bard)
    {
        return {
            "class:bard:spellcasting",
            "Bard cantrips",
            2,
            {{"vicious_mockery", "Vicious Mockery",
              "Wisdom save: 1d6 Psychic damage and Disadvantage on the target's next attack roll, 60 feet."},
             {"starry_wisp", "Starry Wisp",
              "Ranged spell attack: 1d8 Radiant damage, 60 feet; the target cannot be Invisible until the end of your next turn."},
             {"true_strike", "True Strike",
              "Melee weapon attack using your spellcasting ability; Radiant or the weapon's damage type."}}};
    }
    if (klass == CharacterClass::cleric)
    {
        return {
            "class:cleric:spellcasting",
            "Cleric cantrips",
            3,
            {{"sacred_flame", "Sacred Flame",
              "Dexterity save: 1d8 Radiant damage, visible creature within 60 feet."},
             {"spare_the_dying", "Spare the Dying",
              "A creature at 0 Hit Points within 15 feet becomes Stable."},
             {"resistance", "Resistance",
              "Touch: once per turn the creature takes 1d4 less damage of a chosen type; Concentration."},
             {"guidance", "Guidance",
              "Touch: the creature adds 1d4 to Athletics or Medicine checks, as chosen; Concentration."}}};
    }
    if (klass != CharacterClass::wizard && klass != CharacterClass::sorcerer)
    {
        return {};
    }
    TrainingChoiceGroup group{
        klass == CharacterClass::sorcerer ? "class:sorcerer:spellcasting" : std::string(source),
        klass == CharacterClass::sorcerer ? "Sorcerer cantrips" : "Wizard cantrips",
        klass == CharacterClass::sorcerer ? 4u : 3u,
        {{"fire_bolt", "Fire Bolt", "Ranged spell attack: 1d10 Fire damage, 120 feet."},
         {"poison_spray", "Poison Spray", "Ranged spell attack: 1d12 Poison damage, 30 feet."},
         {"ray_of_frost", "Ray of Frost",
          "Ranged spell attack: 1d8 Cold damage, 60 feet; Speed reduced by 10 feet until your next turn."},
         {"shocking_grasp", "Shocking Grasp",
          "Melee spell attack: 1d8 Lightning damage, Touch; prevents Opportunity Attacks until the target’s next turn."},
         {"chill_touch", "Chill Touch",
          "Melee spell attack: 1d10 Necrotic damage, Touch; prevents healing until the end of your next turn."},
         {"acid_splash", "Acid Splash",
          "Dexterity save: 1d6 Acid damage to each creature in a 5-foot-radius sphere within 60 feet."},
         {"true_strike", "True Strike",
          "Melee weapon attack using your spellcasting ability; Radiant or the weapon's damage type."}}};
    if (klass == CharacterClass::sorcerer)
    {
        group.options.push_back(
            {"sorcerous_burst", "Sorcerous Burst",
             "Ranged spell attack: 1d8 damage of a chosen type, 120 feet; an 8 rolls another d8."});
    }
    return group;
}

std::vector<FeatureGrant>
starting_spell_grants(CharacterClass klass, const std::optional<std::vector<std::string>> &cantrips)
{
    if (klass == CharacterClass::sorcerer)
    {
        std::vector<FeatureGrant> result;
        std::set<std::string> unique;
        const auto offered = starting_cantrip_options(CharacterClass::sorcerer).options;
        const auto chosen = cantrips.value_or(std::vector<std::string>{});
        require(chosen.size() <= 4);
        for (const auto &id : chosen)
        {
            require(std::any_of(offered.begin(), offered.end(),
                                [&](const auto &option)
                                {
                                    return option.id == id;
                                }) &&
                    unique.insert(id).second);
            result.push_back(grant(id, 1, "class:sorcerer:spellcasting"));
        }
        return result;
    }
    if (klass == CharacterClass::warlock)
    {
        std::vector<FeatureGrant> result;
        std::set<std::string> unique;
        const auto offered = starting_cantrip_options(CharacterClass::warlock).options;
        require(!cantrips || cantrips->size() <= 2);
        for (const auto &id : cantrips.value_or(std::vector<std::string>{}))
        {
            require(std::any_of(offered.begin(), offered.end(),
                                [&](const auto &option)
                                {
                                    return option.id == id;
                                }) &&
                    unique.insert(id).second);
            result.push_back(grant(id, 1, "class:warlock:pact_magic"));
        }
        return result;
    }
    if (klass == CharacterClass::druid)
    {
        std::vector<FeatureGrant> result;
        std::set<std::string> unique;
        const auto offered = starting_cantrip_options(CharacterClass::druid).options;
        for (const auto &id : cantrips.value_or(std::vector<std::string>{}))
        {
            require(std::any_of(offered.begin(), offered.end(),
                                [&](const auto &option)
                                {
                                    return option.id == id;
                                }) &&
                    unique.insert(id).second);
            result.push_back(grant(id, 1, "class:druid:spellcasting"));
        }
        return result;
    }
    if (klass == CharacterClass::bard)
    {
        std::vector<FeatureGrant> result;
        std::set<std::string> unique;
        const auto offered = starting_cantrip_options(CharacterClass::bard).options;
        for (const auto &id : cantrips.value_or(std::vector<std::string>{}))
        {
            require(std::any_of(offered.begin(), offered.end(),
                                [&](const auto &option)
                                {
                                    return option.id == id;
                                }) &&
                    unique.insert(id).second);
            result.push_back(grant(id, 1, "class:bard:spellcasting"));
        }
        return result;
    }
    if (klass == CharacterClass::cleric)
    {
        std::vector<FeatureGrant> result;
        std::set<std::string> unique;
        const auto offered = starting_cantrip_options(CharacterClass::cleric).options;
        for (const auto &id : cantrips.value_or(std::vector<std::string>{}))
        {
            require(std::any_of(offered.begin(), offered.end(),
                                [&](const auto &option)
                                {
                                    return option.id == id;
                                }) &&
                    unique.insert(id).second);
            result.push_back(grant(id, 1, "class:cleric:spellcasting"));
        }
        return result;
    }
    if (klass != CharacterClass::wizard)
    {
        require(!cantrips || cantrips->empty());
        return {};
    }
    const auto chosen = cantrips.value_or(std::vector<std::string>{"fire_bolt"});
    require(chosen.size() <= 3);
    std::set<std::string> unique;
    std::vector<FeatureGrant> result;
    for (const auto &id : chosen)
    {
        require((id == "fire_bolt" || id == "poison_spray" || id == "ray_of_frost" ||
                 id == "shocking_grasp" || id == "chill_touch" || id == "acid_splash" ||
                 id == "true_strike") &&
                unique.insert(id).second);
        result.push_back(grant(id, 1));
    }
    result.push_back(grant("magic_missile", 1));
    return result;
}

// A class's own spells, without Magic Initiate's.
SpellAccess class_spell_access(std::span<const FeatureGrant> grants, CharacterClass klass,
                               unsigned level, std::span<const std::string> prepared);
SpellChoiceOptions class_spell_choice_options(const CharacterSheet &sheet,
                                              SpellChoiceContext context);

SpellAccess spell_access(std::span<const FeatureGrant> grants, CharacterClass klass, unsigned level,
                         std::span<const std::string> prepared)
{
    auto result = class_spell_access(grants, klass, level, prepared);
    add_initiate_access(result, grants, level);
    // A lineage's spells, unless the character already knows them otherwise.
    const auto known = [&](std::string_view id)
    {
        const auto named = [&](const auto &s)
        {
            return s.id == id;
        };
        return std::any_of(result.cantrips.begin(), result.cantrips.end(), named) ||
               std::any_of(result.initiate.begin(), result.initiate.end(), named) ||
               std::find(result.prepared.begin(), result.prepared.end(), id) !=
                   result.prepared.end();
    };
    for (const auto &lineage : lineage_spells(grants, level))
    {
        for (const auto &id : lineage.spells)
        {
            if (known(id))
            {
                continue;
            }
            const auto &spell = find(id);
            result.initiate.push_back({std::string(spell.id), std::string(spell.label),
                                       lineage.feat, spell.level ? 3u : 1u});
            if (spell.level &&
                std::find(result.always_prepared.begin(), result.always_prepared.end(), id) ==
                    result.always_prepared.end())
            {
                result.always_prepared.emplace_back(spell.id);
            }
        }
    }
    return result;
}

SpellAccess class_spell_access(std::span<const FeatureGrant> grants, CharacterClass klass,
                               unsigned level, std::span<const std::string> prepared)
{
    SpellAccess result;
    if (const auto *caster = prepared_caster(klass))
    {
        return prepared_access(*caster, grants, level, prepared);
    }
    if (klass != CharacterClass::wizard)
    {
        require(std::none_of(grants.begin(), grants.end(), class_spell_grant));
        return result; // Each other class's preparation policy has its own issue.
    }
    require(level >= 1 && level <= level_cap(klass));
    require(std::find(grants.begin(), grants.end(),
                      FeatureGrant{"feature:spellcasting", "class:wizard", 1, {}}) != grants.end());
    // SRD 5.2.1 p. 77: cantrips and prepared spells by Wizard level.
    constexpr std::array<unsigned, 15> prepared_by_level{4,  5,  6,  7,  9,  10, 11, 12,
                                                         14, 15, 16, 16, 17, 18, 19};
    result.cantrip_choices = level >= 10 ? 5 : level >= 4 ? 4 : 3;
    result.spellbook_choices = 6 + 2 * (level - 1);
    result.prepared_choices = prepared_by_level[level - 1];
    std::set<std::string> known;
    std::array<unsigned, 16> books{}, cantrips{};
    for (const auto &g : grants)
    {
        if (class_spell_grant(g))
        {
            require(g.source_id == source && g.level >= 1 && g.level <= level &&
                    known.insert(g.id).second);
            const auto &spell = find(std::string_view(g.id).substr(6));
            require(spell.lists & wizard_list);
            auto expected = grant(spell.id, g.level);
            unsigned learned = g.level;
            if (const auto replacement = g.choices.find("learned_at");
                replacement != g.choices.end())
            {
                require(spell.level == 0);
                bool valid = false;
                for (unsigned n = g.level; n <= level; ++n)
                {
                    if (replacement->second == std::to_string(n))
                    {
                        learned = n;
                        valid = true;
                    }
                }
                require(valid);
                expected.choices.emplace("learned_at", replacement->second);
            }
            require(g == expected);
            // A spell of a level the Wizard had slots for: half its level, rounded up.
            require(spell.level == 0 || spell.level <= (g.level + 1) / 2);
            auto &list = spell.level ? result.spellbook : result.cantrips;
            list.push_back({std::string(spell.id), std::string(spell.label), g.source_id, learned});
            if (spell.level)
            {
                ++books[g.level];
            }
            else
            {
                ++cantrips[g.level];
            }
        }
    }
    require(books[1] <= 6 && cantrips[1] <= 3);
    for (unsigned n = 2; n <= level; ++n)
    {
        require(books[n] <= 2 && cantrips[n] <= (n == 4 || n == 10 ? 1u : 0u));
    }
    std::set<std::string> selected;
    for (const auto &id : prepared)
    {
        require(selected.insert(id).second &&
                std::any_of(result.spellbook.begin(), result.spellbook.end(),
                            [&](const auto &s)
                            {
                                return s.id == id;
                            }));
        result.prepared.push_back(id);
    }
    require(result.prepared.size() <= result.prepared_choices);
    return result;
}

SpellChoiceOptions spell_choice_options(const CharacterSheet &sheet, SpellChoiceContext context)
{
    auto result = class_spell_choice_options(sheet, context);
    if (context == SpellChoiceContext::advancement)
    {
        add_initiate_learning(result, sheet);
    }
    return result;
}

SpellChoiceOptions class_spell_choice_options(const CharacterSheet &sheet,
                                              SpellChoiceContext context)
{
    SpellChoiceOptions result;
    const auto klass = class_of(sheet);
    if (const auto *caster = prepared_caster(klass))
    {
        return prepared_choice_options(*caster, sheet, context);
    }
    if (klass != CharacterClass::wizard)
    {
        return result;
    }
    // Memorize Spell: from Wizard 5, a Short Rest changes one prepared spell.
    if (context == SpellChoiceContext::short_rest && sheet.level < 5)
    {
        return result;
    }
    const auto access = spell_access(sheet.grants, klass, sheet.level, sheet.prepared_spells);
    auto known = [&](std::string_view id)
    {
        return std::any_of(sheet.grants.begin(), sheet.grants.end(),
                           [&](const auto &g)
                           {
                               return g.id == "spell:" + std::string(id);
                           });
    };
    // Advancement (including creation at level one) learns the current level's
    // spells; a Long Rest only prepares or replaces.
    if (context == SpellChoiceContext::advancement)
    {
        const unsigned level = sheet.level;
        for (bool cantrip : {true, false})
        {
            const unsigned capacity = cantrip ? (level == 1                  ? 3
                                                 : level == 4 || level == 10 ? 1
                                                                             : 0)
                                              : (level == 1 ? 6 : 2);
            const auto used = static_cast<unsigned>(std::count_if(
                sheet.grants.begin(), sheet.grants.end(),
                [&](const auto &g)
                {
                    return class_spell_grant(g) && g.level == level &&
                           (find(std::string_view(g.id).substr(6)).level == 0) == cantrip;
                }));
            if (capacity == used)
            {
                continue;
            }
            TrainingChoiceGroup group;
            group.id = std::string(cantrip ? "cantrips:" : "spellbook:") + std::to_string(level);
            group.label = cantrip ? "Wizard cantrips" : "Spellbook";
            group.count = capacity - used;
            group.acquired_level = level;
            for (const auto &spell : spells)
            {
                // A spell of a level the Wizard has slots for: half its level,
                // rounded up.
                if ((spell.level == 0) == cantrip && (spell.lists & wizard_list) &&
                    spell.level <= (level + 1) / 2 && !known(spell.id))
                {
                    group.options.push_back({std::string(spell.id), std::string(spell.label), {}});
                }
            }
            result.learning.push_back(std::move(group));
        }
    }
    result.prepared_count = access.prepared_choices;
    result.may_prepare = true;
    for (const auto &spell : access.spellbook)
    {
        result.preparation.push_back({spell.id, spell.label, {}});
    }
    if (context == SpellChoiceContext::advancement)
    {
        result.locked_prepared = sheet.prepared_spells;
    }
    result.may_replace = context == SpellChoiceContext::long_rest;
    if (result.may_replace)
    {
        for (const auto &spell : access.cantrips)
        {
            result.replaceable.push_back({spell.id, spell.label, {}});
        }
        for (const auto &choice : starting_cantrip_options(CharacterClass::wizard).options)
        {
            if (!known(choice.id))
            {
                result.replacements.push_back(choice);
            }
        }
    }
    return result;
}

void apply_spell_choices(CharacterSheet &sheet, const SpellChoices &choices,
                         SpellChoiceContext context, ChoiceCompleteness completeness)
{
    const bool complete = completeness == ChoiceCompleteness::complete;
    const auto klass = class_of(sheet);
    const auto *caster = prepared_caster(klass);
    const auto origin = class_source(klass);
    auto candidate = sheet;
    const auto options = spell_choice_options(sheet, context);
    for (const auto &[id, values] : choices.learning)
    {
        const auto group = std::find_if(options.learning.begin(), options.learning.end(),
                                        [&](const auto &g)
                                        {
                                            return g.id == id;
                                        });
        require(group != options.learning.end() && values.size() <= group->count);
        for (const auto &value : values)
        {
            require(std::any_of(group->options.begin(), group->options.end(),
                                [&](const auto &o)
                                {
                                    return o.id == value;
                                }));
            if (auto initiate = initiate_choice_grant(*group, value))
            {
                candidate.grants.push_back(std::move(*initiate));
            }
            else
            {
                candidate.grants.push_back(grant(value, group->acquired_level, origin));
            }
        }
    }
    require(choices.replace_cantrip.empty() == choices.replacement.empty());
    if (!choices.replace_cantrip.empty())
    {
        require(options.may_replace &&
                std::any_of(options.replaceable.begin(), options.replaceable.end(),
                            [&](const auto &o)
                            {
                                return o.id == choices.replace_cantrip;
                            }) &&
                std::any_of(options.replacements.begin(), options.replacements.end(),
                            [&](const auto &o)
                            {
                                return o.id == choices.replacement;
                            }));
        auto existing = std::find_if(candidate.grants.begin(), candidate.grants.end(),
                                     [&](const auto &g)
                                     {
                                         return g.id == "spell:" + choices.replace_cantrip;
                                     });
        *existing = grant(choices.replacement, existing->level, origin);
        existing->choices.emplace("learned_at", std::to_string(sheet.level));
    }
    if (choices.prepared)
    {
        require(options.may_prepare);
        for (const auto &id : options.locked_prepared)
        {
            require(std::find(choices.prepared->begin(), choices.prepared->end(), id) !=
                    choices.prepared->end());
        }
        if ((caster && caster->rest_change == PreparedCaster::RestChange::one &&
             context == SpellChoiceContext::long_rest) ||
            context == SpellChoiceContext::short_rest)
        {
            require(std::count_if(sheet.prepared_spells.begin(), sheet.prepared_spells.end(),
                                  [&](const auto &id)
                                  {
                                      return std::find(choices.prepared->begin(),
                                                       choices.prepared->end(),
                                                       id) == choices.prepared->end();
                                  }) <= 1);
        }
        const auto always = always_prepared_spells(klass, sheet.level, sheet.grants);
        for (const auto &id : *choices.prepared)
        {
            require(std::find(always.begin(), always.end(), id) == always.end());
        }
        candidate.prepared_spells = *choices.prepared;
    }
    const auto access =
        spell_access(candidate.grants, klass, candidate.level, candidate.prepared_spells);
    if (complete)
    {
        const auto remaining = spell_choice_options(candidate, context);
        for (const auto &group : remaining.learning)
        {
            if (!group.options.empty() && group.count)
            {
                throw std::runtime_error("Complete the available spell choices.");
            }
        }
        // A place left open, such as one a swap freed, is a choice to finish.
        if (options.may_prepare &&
            candidate.prepared_spells.size() !=
                std::min<std::size_t>(access.prepared_choices, remaining.preparation.size()))
        {
            throw std::runtime_error("Complete the available spell choices.");
        }
    }
    sheet = std::move(candidate);
}

std::vector<std::string> circle_spells(unsigned level, std::string_view land)
{
    std::vector<std::string> result;
    for (const auto &entry : always_prepared_table)
    {
        if (entry.klass == CharacterClass::druid && level >= entry.level && entry.land == land)
        {
            result.emplace_back(entry.spell);
        }
    }
    return result;
}

std::vector<std::string> always_prepared_spells(CharacterClass klass, unsigned level,
                                                std::span<const FeatureGrant> grants)
{
    std::vector<std::string> result;
    for (const auto &entry : always_prepared_table)
    {
        if (entry.klass == klass && level >= entry.level &&
            (entry.land.empty() || std::any_of(grants.begin(), grants.end(),
                                               [&](const auto &g)
                                               {
                                                   return g.id == "land:" + std::string(entry.land);
                                               })))
        {
            result.emplace_back(entry.spell);
        }
    }
    // Magic Initiate's level-one spell is always prepared too.
    for (const auto &g : grants)
    {
        if (is_initiate_grant(g) && is_spell_grant(g))
        {
            const auto id = std::string_view(g.id).substr(6);
            if (find(id).level && std::find(result.begin(), result.end(), id) == result.end())
            {
                result.emplace_back(id);
            }
        }
    }
    // So are a Lore Bard's Magical Discoveries.
    for (const auto &g : grants)
    {
        if (g.id.starts_with("discovery:"))
        {
            result.emplace_back(std::string_view(g.id).substr(10));
        }
    }
    // So is a lineage's, from level 3.
    for (const auto &lineage : lineage_spells(grants, level))
    {
        if (!lineage.free_spell.empty() &&
            std::find(result.begin(), result.end(), lineage.free_spell) == result.end())
        {
            result.push_back(lineage.free_spell);
        }
    }
    return result;
}

std::vector<InitiateSpells> magic_initiate_spells(std::span<const FeatureGrant> grants)
{
    std::vector<InitiateSpells> result;
    for (const auto &[feat, acquired] : initiate_feats(grants))
    {
        InitiateSpells entry{feat};
        const auto &list = *initiate_list(feat);
        if (list.ability)
        {
            entry.ability = *list.ability;
        }
        for (const auto &g : grants)
        {
            if (g.source_id != feat)
            {
                continue;
            }
            if (g.id.starts_with(ability_prefix))
            {
                const auto ability = std::string_view(g.id).substr(ability_prefix.size());
                entry.ability = ability == "wisdom"     ? Ability::wisdom
                                : ability == "charisma" ? Ability::charisma
                                                        : Ability::intelligence;
                continue;
            }
            const auto id = std::string_view(g.id).substr(6);
            entry.spells.emplace_back(id);
            if (find(id).level && list.free_cast)
            {
                entry.free_spell = id;
            }
        }
        result.push_back(std::move(entry));
    }
    return result;
}

std::vector<AdvancementOption> magic_initiate_feats(std::span<const FeatureGrant> grants)
{
    const auto held = initiate_feats(grants);
    std::vector<AdvancementOption> result;
    for (const auto &list : initiate_lists)
    {
        const auto feat = std::string(initiate_prefix) + std::string(list.id);
        result.push_back(
            {feat.substr(5), "Magic Initiate (" + std::string(list.label) + ")",
             "Two " + std::string(list.label) +
                 " cantrips and one level-one spell, cast once per Long Rest without a slot; Intelligence, Wisdom or Charisma for them.",
             std::none_of(held.begin(), held.end(),
                          [&](const auto &f)
                          {
                              return f.first == feat;
                          })});
    }
    return result;
}

bool prepares_spells(CharacterClass klass)
{
    return klass == CharacterClass::wizard || prepared_caster(klass);
}

bool on_class_list(CharacterClass klass, std::string_view spell)
{
    const auto *caster = prepared_caster(klass);
    const auto found = std::find_if(spells.begin(), spells.end(),
                                    [&](const auto &s)
                                    {
                                        return s.id == spell;
                                    });
    const unsigned list = klass == CharacterClass::wizard ? wizard_list
                          : caster                        ? caster->list
                                                          : 0u;
    return found != spells.end() && (found->lists & list);
}

std::vector<std::string> known_cantrip_ids(const SpellAccess &access)
{
    std::vector<std::string> result;
    // find() still rejects an unsupported id, as the mask lookup used to.
    for (const auto &s : access.cantrips)
    {
        (void)find(s.id);
        result.push_back(s.id);
    }
    for (const auto &s : access.initiate)
    {
        if (!find(s.id).level)
        {
            result.push_back(s.id);
        }
    }
    return result;
}

std::vector<std::string> casting_ids(const SpellAccess &access)
{
    auto result = known_cantrip_ids(access);
    for (const auto &id : access.prepared)
    {
        (void)find(id);
        result.push_back(id);
    }
    // A spell prepared before a feature made it always prepared appears once.
    for (const auto &id : access.always_prepared)
    {
        if (std::find(result.begin(), result.end(), id) == result.end())
        {
            result.push_back(id);
        }
    }
    return result;
}

namespace
{
bool offers(std::span<const AdvancementOption> options, std::string_view id)
{
    return std::any_of(options.begin(), options.end(),
                       [&](const auto &o)
                       {
                           return o.id == id;
                       });
}

// A swap is legal when both ids come from the offer, or neither is given.
bool swap_allowed(const LevelSwap &swap, const SwapOffer &offer)
{
    if (swap.replaced.empty() && swap.replacement.empty())
    {
        return false;
    }
    if (!offers(offer.replaceable, swap.replaced) || !offers(offer.replacements, swap.replacement))
    {
        throw std::runtime_error("Choose a listed replacement");
    }
    return true;
}
} // namespace

SwapOffer cantrip_swap_offer(const CharacterSheet &sheet)
{
    SwapOffer offer{"Cantrip", {}, {}};
    const auto *caster = prepared_caster(class_of(sheet));
    if (!caster || sheet.level >= static_cast<int>(level_cap(class_of(sheet))))
    {
        return offer;
    }
    const auto known = [&](std::string_view id)
    {
        return std::any_of(sheet.grants.begin(), sheet.grants.end(),
                           [&](const auto &g)
                           {
                               return g.id == "spell:" + std::string(id);
                           });
    };
    for (const auto &g : sheet.grants)
    {
        if (g.source_id == caster->source && is_spell_grant(g))
        {
            const auto &spell = find(std::string_view(g.id).substr(6));
            if (!spell.level)
            {
                offer.replaceable.push_back({std::string(spell.id), std::string(spell.label), {}});
            }
        }
    }
    if (offer.replaceable.empty())
    {
        return offer;
    }
    for (const auto &spell : spells)
    {
        if (!spell.level && (spell.lists & caster->cantrip_list) && !known(spell.id))
        {
            offer.replacements.push_back({std::string(spell.id), std::string(spell.label), {}});
        }
    }
    return offer;
}

SwapOffer spell_swap_offer(const CharacterSheet &sheet)
{
    SwapOffer offer{"Spell", {}, {}};
    const auto klass = class_of(sheet);
    const auto *caster = prepared_caster(klass);
    if (!caster || caster->rest_change != PreparedCaster::RestChange::none ||
        sheet.level >= static_cast<int>(level_cap(klass)))
    {
        return offer;
    }
    const auto next = sheet.level + 1u;
    const auto always = always_prepared_spells(klass, next, sheet.grants);
    const auto taken = [&](std::string_view id)
    {
        return std::find(sheet.prepared_spells.begin(), sheet.prepared_spells.end(), id) !=
                   sheet.prepared_spells.end() ||
               std::find(always.begin(), always.end(), id) != always.end() ||
               std::any_of(sheet.grants.begin(), sheet.grants.end(),
                           [&](const auto &g)
                           {
                               return g.id == "spell:" + std::string(id);
                           });
    };
    for (const auto &id : sheet.prepared_spells)
    {
        if (std::find(always.begin(), always.end(), id) == always.end())
        {
            offer.replaceable.push_back({id, std::string(find(id).label), {}});
        }
    }
    if (offer.replaceable.empty())
    {
        return offer;
    }
    for (const auto &spell : spells)
    {
        if (spell.level >= 1 && spell.level <= caster->highest_slot[next - 1] &&
            (spell.lists & prepared_list(*caster, next)) && !taken(spell.id))
        {
            offer.replacements.push_back({std::string(spell.id), std::string(spell.label), {}});
        }
    }
    return offer;
}

void apply_cantrip_swap(CharacterSheet &sheet, const LevelSwap &swap, const SwapOffer &offer)
{
    if (!swap_allowed(swap, offer))
    {
        return;
    }
    // The replacement takes the replaced cantrip's place and acquisition level.
    for (auto &g : sheet.grants)
    {
        if (g.id == "spell:" + swap.replaced && is_spell_grant(g) && !is_initiate_grant(g))
        {
            g.id = "spell:" + swap.replacement;
            return;
        }
    }
    throw std::runtime_error("No such cantrip to replace");
}

void apply_spell_swap(CharacterSheet &sheet, const LevelSwap &swap, const SwapOffer &offer)
{
    if (!swap_allowed(swap, offer))
    {
        return;
    }
    const auto found =
        std::find(sheet.prepared_spells.begin(), sheet.prepared_spells.end(), swap.replaced);
    if (found == sheet.prepared_spells.end())
    {
        throw std::runtime_error("No such spell to replace");
    }
    *found = swap.replacement;
}

std::vector<InitiateSpells> lineage_spells(std::span<const FeatureGrant> grants, unsigned level)
{
    const auto chose = [&](std::string_view id) -> const FeatureGrant *
    {
        const auto found = std::find_if(grants.begin(), grants.end(),
                                        [&](const auto &g)
                                        {
                                            return g.id == id;
                                        });
        return found == grants.end() ? nullptr : &*found;
    };
    const auto ability_of = [&](std::string_view species, Ability fallback)
    {
        for (const auto &[id, value] :
             {std::pair{"intelligence", Ability::intelligence},
              std::pair{"wisdom", Ability::wisdom}, std::pair{"charisma", Ability::charisma}})
        {
            if (const auto *g = chose("spellcasting_ability:" + std::string(id));
                g && g->source_id == "species:" + std::string(species) + ":ability")
            {
                return value;
            }
        }
        return fallback;
    };

    struct Legacy
    {
        std::string_view choice, cantrip, third;
    };

    constexpr std::array legacies{Legacy{"legacy:abyssal", "poison_spray", "ray_of_sickness"},
                                  Legacy{"legacy:chthonic", "chill_touch", "false_life"},
                                  Legacy{"legacy:infernal", "fire_bolt", "hellish_rebuke"}};
    std::vector<InitiateSpells> result;
    for (const auto &legacy : legacies)
    {
        if (chose(legacy.choice))
        {
            InitiateSpells entry{"species:tiefling",
                                 ability_of("tiefling", Ability::charisma),
                                 {std::string(legacy.cantrip)},
                                 {}};
            if (level >= 3)
            {
                entry.spells.emplace_back(legacy.third);
                entry.free_spell = legacy.third;
            }
            result.push_back(std::move(entry));
        }
    }
    if (chose("lineage:high_elf"))
    {
        for (const auto &g : grants)
        {
            if (g.source_id == "species:elf:cantrip" && g.id.starts_with("cantrip:"))
            {
                result.push_back({"species:elf",
                                  ability_of("elf", Ability::intelligence),
                                  {g.id.substr(8)},
                                  {}});
            }
        }
    }
    if (chose("lineage:wood_elf") && level >= 3)
    {
        result.push_back({"species:elf",
                          ability_of("elf", Ability::intelligence),
                          {"longstrider"},
                          "longstrider"});
    }
    return result;
}

} // namespace opengold::srd5::detail
