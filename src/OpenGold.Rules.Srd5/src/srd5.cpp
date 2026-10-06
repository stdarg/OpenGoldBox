#include "dice.h"
#include "damage_roll.h"
#include "sneak_attack.h"
#include "action_budget.h"
#include "feature_grants.h"
#include "training.h"
#include "weapon_mastery.h"
#include "spell_access.h"
#include "spell_table.h"
#include "creature_equipment.h"
#include "beast_forms.h"
#include "combat_grid.h"
#include "status_effects.h"
#include "concentration.h"
#include "life_cycle.h"
#include "recovery_timeline.h"
#include "weapons.h"
#include "armor.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <initializer_list>
#include <utility>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace opengold::srd5
{
using namespace rules;

int ability_modifier(int score) noexcept
{
    return static_cast<int>(std::floor((static_cast<double>(score) - 10) / 2.0));
}

int minimum_save_roll(int dc, int bonus) noexcept
{
    return static_cast<int>(std::clamp(static_cast<long long>(dc) - bonus, 1LL, 21LL));
}

bool attack_hits(int natural, int bonus, int ac) noexcept
{
    return natural == 20 || (natural != 1 && static_cast<std::int64_t>(natural) + bonus >= ac);
}

namespace
{
std::string weapon_label(std::string_view key)
{
    if (const auto *item = detail::weapon(key))
        return std::string(item->label);
    if (const auto *item = detail::armor(key))
        return std::string(item->label);
    return std::string(key);
}

std::string attack_ability(std::string_view key)
{
    const auto *item = detail::weapon(key);
    return item && item->finesse  ? "higher of Strength or Dexterity"
           : item && item->ranged ? "Dexterity"
           : "Strength";
}

bool trained(std::string_view klass, std::span<const FeatureGrant> grants, std::string_view key)
{
    // Starting-class grants, SRD 5.2.1 pp. 49 and 61, plus the Protector Divine
    // Order's Martial weapon and Heavy armor training. Multiclass entry and other
    // optional feature grants are separate, not yet implemented capabilities.
    // The Druid's Warden Primal Order: Martial weapons and Medium armor.
    const bool protector = detail::has_grant(grants, "order:protector");
    const bool warden = detail::has_grant(grants, "order:warden");
    if (const auto *weapon = detail::weapon(key))
        return detail::weapon_proficient(detail::grant_source_id(klass), *weapon) ||
               ((protector || warden) && weapon->martial);
    if (const auto *armor = detail::armor(key))
        return detail::armor_trained(klass, armor->category) ||
               (protector && armor->category == detail::ArmorCategory::heavy) ||
               (warden && armor->category == detail::ArmorCategory::medium);
    throw std::runtime_error("Unsupported equipment conversion: " + std::string(key));
}
} // namespace

// Metamagic options, in the order of metamagic_options().
enum class Metamagic : int
{
    careful,
    distant,
    empowered,
    extended,
    heightened,
    quickened,
    seeking,
    subtle,
    transmuted,
    twinned
};

constexpr std::array<std::string_view, 10> metamagic_ids{"careful", "distant", "empowered",
    "extended", "heightened", "quickened", "seeking", "subtle", "transmuted", "twinned"};
constexpr std::array<std::string_view, 10> metamagic_labels{"Careful Spell", "Distant Spell",
    "Empowered Spell", "Extended Spell", "Heightened Spell", "Quickened Spell", "Seeking Spell",
    "Subtle Spell", "Transmuted Spell", "Twinned Spell"};
// Transmuted Spell changes one of these damage types into another.
constexpr std::array<std::string_view, 6> transmuted_types{"acid", "cold", "fire", "lightning",
    "poison", "thunder"};

int metamagic_cost(Metamagic option)
{
    return option == Metamagic::heightened || option == Metamagic::quickened ? 2 : 1;
}

bool transmutable(detail::DamageType type)
{
    return std::any_of(transmuted_types.begin(), transmuted_types.end(), [&](auto id)
    {
        return detail::damage_type(id) == type;
    });
}

// Whether a readied Metamagic option can change this spell.
bool metamagic_applies(Metamagic option, const detail::SpellDef &spell)
{
    const bool saves = spell.pattern == detail::SpellPattern::save_damage ||
                       spell.pattern == detail::SpellPattern::save_condition;
    const bool attacks = spell.pattern == detail::SpellPattern::spell_attack ||
                         spell.pattern == detail::SpellPattern::repeat_attack;
    switch (option)
    {
    case Metamagic::careful:
        return saves && spell.target == detail::SpellTarget::area;
    case Metamagic::distant:
        return spell.target != detail::SpellTarget::self;
    case Metamagic::empowered:
        return spell.dice.count && (attacks || spell.pattern == detail::SpellPattern::save_damage);
    case Metamagic::extended:
        return spell.concentration;
    case Metamagic::heightened:
        return saves;
    case Metamagic::quickened:
        return !spell.bonus_action;
    case Metamagic::seeking:
        return attacks;
    case Metamagic::subtle:
        return false; // Retired with spell components (CLASS-11).
    case Metamagic::transmuted:
        return (attacks || spell.pattern == detail::SpellPattern::save_damage) &&
               transmutable(spell.damage);
    case Metamagic::twinned:
        return spell.upcast.extra_instances > 0;
    }
    return false;
}

// The lasting benefit a buff spell's rider applies.
detail::EffectKind rider_effect(detail::Rider rider)
{
    return rider == detail::Rider::shield_of_faith ? detail::EffectKind::shield_of_faith
           : rider == detail::Rider::heroism       ? detail::EffectKind::heroism
           : rider == detail::Rider::bless         ? detail::EffectKind::bless
           : rider == detail::Rider::protection_from_evil_and_good
           ? detail::EffectKind::protection_from_evil_and_good
           : rider == detail::Rider::hunters_mark ? detail::EffectKind::hunters_mark
           : rider == detail::Rider::longstrider  ? detail::EffectKind::longstrider
           : detail::EffectKind::divine_favor;
}

// Effects that end with their caster's Concentration.
bool concentration_effect(detail::EffectKind kind)
{
    return kind == detail::EffectKind::shield_of_faith || kind == detail::EffectKind::heroism ||
           kind == detail::EffectKind::bless ||
           kind == detail::EffectKind::protection_from_evil_and_good ||
           kind == detail::EffectKind::hunters_mark || kind == detail::EffectKind::ensnaring_strike ||
           kind == detail::EffectKind::entangle || kind == detail::EffectKind::bane ||
           kind == detail::EffectKind::hold_person || kind == detail::EffectKind::resistance ||
           kind == detail::EffectKind::expeditious_retreat || kind == detail::EffectKind::drowsy ||
           kind == detail::EffectKind::asleep || kind == detail::EffectKind::laughing ||
           kind == detail::EffectKind::webbed || kind == detail::EffectKind::enfeebled ||
           kind == detail::EffectKind::blur || kind == detail::EffectKind::invisible ||
           kind == detail::EffectKind::enlarged || kind == detail::EffectKind::reduced ||
           kind == detail::EffectKind::dragons_breath || kind == detail::EffectKind::extended ||
           kind == detail::EffectKind::hex || kind == detail::EffectKind::outlined ||
           kind == detail::EffectKind::flame_blade || kind == detail::EffectKind::heated;
}

// Command's option as players read it, "Approach" for 1.
std::string command_label(int option)
{
    auto label = std::string(detail::command_options.at(std::size_t(option - 1)));
    label[0] = char(label[0] - 'a' + 'A');
    return label;
}

// The grant a Fighting Style choice adds: a feat, or the Paladin's Blessed
// Warrior or the Ranger's Druidic Warrior.
std::string fighting_style_grant(std::string_view style)
{
    return style == "blessed_warrior" || style == "druidic_warrior" ? "feature:" + std::string(style)
           : "feat:" + std::string(style);
}

// Spells whose creatures are chosen one click at a time (CLASS-2) when they may
// affect more than one.
bool selects_creatures(const detail::SpellDef &spell)
{
    return spell.pattern == detail::SpellPattern::buff || spell.rider == detail::Rider::command ||
           (spell.pattern == detail::SpellPattern::save_condition &&
            (spell.instances > 1 || spell.upcast.extra_instances));
}

// A smite follows the caster's own melee hit; divine_smite_free is Paladin's
// Smite's slotless cast.
bool is_smite(std::string_view verb)
{
    return verb == "divine_smite" || verb == "divine_smite_free" || verb == "searing_smite" ||
           verb == "ensnaring_strike";
}

std::string equipment_note(const CharacterSheet &sheet, std::string_view item)
{
    std::string text;
    if (trained(sheet.character_class, sheet.grants, item))
        text = "Class training: no untrained-use penalty.";
    else if (item == "shield")
        text = "Untrained shield: no AC bonus.";
    else if (detail::armor(item))
        text =
            "Untrained armor: disadvantage on Strength/Dexterity attacks, checks (including initiative), and saves; cannot cast spells.";
    else
        text = "Untrained weapon: no proficiency bonus on attack rolls (no +2 at level 1).";
    if (item == "chain_mail" && sheet.scores[0] < 13)
        text += " Chain mail requires Strength 13: speed reduced by 10 feet.";
    if (item == "wand")
        text += " Plain focus only; no charged wand spell is granted.";
    return text;
}

namespace
{
constexpr std::string_view rush_source = "species:orc/trait:adrenaline_rush";

// The only character profile tag this module reads or writes. Profiles live
// inside campaign saves and combat checkpoints, whose own format numbers reject
// anything older; change this recipe in place until 1.0.
constexpr std::string_view profile_magic = "PC42";

// The only combat checkpoint format this module reads or writes. Older
// checkpoints are rejected rather than migrated; change it in place until 1.0.
constexpr unsigned checkpoint_format = 40;

// Which spells a class may legitimately have stored at a level. This replaces a
// packed allow-mask, which could not express a spell beyond the 31st bit.
struct SpellAccessRow
{
    std::string_view klass, spell;
    unsigned min_level;
};

constexpr std::array class_spell_access
{
    SpellAccessRow{"Cleric", "cure_wounds", 1},
    SpellAccessRow{"Cleric", "healing_word", 1},
    SpellAccessRow{"Cleric", "blindness", 3},
    SpellAccessRow{"Cleric", "sacred_flame", 1},
    SpellAccessRow{"Cleric", "inflict_wounds", 1},
    SpellAccessRow{"Paladin", "cure_wounds", 1},
    SpellAccessRow{"Paladin", "divine_smite", 1},
    SpellAccessRow{"Paladin", "searing_smite", 1},
    SpellAccessRow{"Paladin", "shield_of_faith", 1},
    SpellAccessRow{"Paladin", "heroism", 1},
    SpellAccessRow{"Paladin", "divine_favor", 1},
    SpellAccessRow{"Cleric", "shield_of_faith", 1},
    SpellAccessRow{"Cleric", "bless", 1},
    SpellAccessRow{"Paladin", "bless", 1},
    SpellAccessRow{"Paladin", "protection_from_evil_and_good", 1},
    SpellAccessRow{"Cleric", "protection_from_evil_and_good", 1},
    SpellAccessRow{"Paladin", "command", 1},
    SpellAccessRow{"Ranger", "cure_wounds", 1},
    SpellAccessRow{"Ranger", "hunters_mark", 1},
    SpellAccessRow{"Ranger", "longstrider", 1},
    SpellAccessRow{"Ranger", "goodberry", 1},
    SpellAccessRow{"Ranger", "ensnaring_strike", 1},
    SpellAccessRow{"Ranger", "entangle", 1},
    SpellAccessRow{"Ranger", "fog_cloud", 1},
    // Blessed and Druidic Warrior's cantrips; spell access checks the feature itself.
    SpellAccessRow{"Ranger", "poison_spray", 2},
    SpellAccessRow{"Paladin", "sacred_flame", 2},
    SpellAccessRow{"Cleric", "command", 1},
    SpellAccessRow{"Cleric", "lesser_restoration", 3},
    SpellAccessRow{"Cleric", "aid", 3},
    SpellAccessRow{"Cleric", "guiding_bolt", 1},
    SpellAccessRow{"Cleric", "bane", 1},
    SpellAccessRow{"Cleric", "spare_the_dying", 1},
    SpellAccessRow{"Cleric", "hold_person", 3},
    SpellAccessRow{"Cleric", "sanctuary", 1},
    SpellAccessRow{"Cleric", "warding_bond", 3},
    SpellAccessRow{"Cleric", "protection_from_poison", 3},
    SpellAccessRow{"Cleric", "resistance", 1},
    SpellAccessRow{"Cleric", "silence", 3},
    SpellAccessRow{"Cleric", "spiritual_weapon", 3},
    SpellAccessRow{"Cleric", "prayer_of_healing", 3},
    SpellAccessRow{"Paladin", "resistance", 2},
    SpellAccessRow{"Ranger", "resistance", 2},
    SpellAccessRow{"Paladin", "spare_the_dying", 2},
    SpellAccessRow{"Ranger", "spare_the_dying", 2},
    SpellAccessRow{"Wizard", "fire_bolt", 1},
    SpellAccessRow{"Wizard", "magic_missile", 1},
    SpellAccessRow{"Wizard", "scorching_ray", 3},
    SpellAccessRow{"Wizard", "protection_from_evil_and_good", 1},
    SpellAccessRow{"Wizard", "longstrider", 1},
    SpellAccessRow{"Wizard", "fog_cloud", 1},
    SpellAccessRow{"Wizard", "hold_person", 3},
    SpellAccessRow{"Wizard", "burning_hands", 1},
    SpellAccessRow{"Wizard", "thunderwave", 1},
    SpellAccessRow{"Wizard", "shatter", 3},
    SpellAccessRow{"Wizard", "mage_armor", 1},
    SpellAccessRow{"Wizard", "false_life", 1},
    SpellAccessRow{"Wizard", "expeditious_retreat", 1},
    SpellAccessRow{"Wizard", "ray_of_sickness", 1},
    SpellAccessRow{"Wizard", "ice_knife", 1},
    SpellAccessRow{"Wizard", "chromatic_orb", 1},
    SpellAccessRow{"Wizard", "acid_splash", 1},
    SpellAccessRow{"Wizard", "sleep", 1},
    SpellAccessRow{"Wizard", "hideous_laughter", 1},
    SpellAccessRow{"Wizard", "color_spray", 1},
    SpellAccessRow{"Wizard", "grease", 1},
    SpellAccessRow{"Wizard", "web", 3},
    SpellAccessRow{"Wizard", "shield", 1},
    SpellAccessRow{"Wizard", "misty_step", 3},
    SpellAccessRow{"Wizard", "acid_arrow", 3},
    SpellAccessRow{"Wizard", "mind_spike", 3},
    SpellAccessRow{"Wizard", "ray_of_enfeeblement", 3},
    SpellAccessRow{"Wizard", "blur", 3},
    SpellAccessRow{"Wizard", "mirror_image", 3},
    SpellAccessRow{"Wizard", "magic_weapon", 3},
    SpellAccessRow{"Wizard", "invisibility", 3},
    SpellAccessRow{"Wizard", "see_invisibility", 3},
    SpellAccessRow{"Wizard", "darkness", 3},
    SpellAccessRow{"Wizard", "flaming_sphere", 3},
    SpellAccessRow{"Wizard", "knock", 3},
    SpellAccessRow{"Wizard", "enlarge_reduce", 3},
    SpellAccessRow{"Wizard", "true_strike", 1},
    SpellAccessRow{"Wizard", "dragons_breath", 3},
    SpellAccessRow{"Wizard", "charm_person", 1},
    SpellAccessRow{"Wizard", "gust_of_wind", 3},
    SpellAccessRow{"Wizard", "blindness", 3},
    SpellAccessRow{"Wizard", "poison_spray", 1},
    SpellAccessRow{"Wizard", "ray_of_frost", 1},
    SpellAccessRow{"Wizard", "shocking_grasp", 1},
    SpellAccessRow{"Wizard", "chill_touch", 1},
    SpellAccessRow{"Sorcerer", "fire_bolt", 1},
    SpellAccessRow{"Sorcerer", "poison_spray", 1},
    SpellAccessRow{"Sorcerer", "ray_of_frost", 1},
    SpellAccessRow{"Sorcerer", "shocking_grasp", 1},
    SpellAccessRow{"Sorcerer", "chill_touch", 1},
    SpellAccessRow{"Sorcerer", "acid_splash", 1},
    SpellAccessRow{"Sorcerer", "true_strike", 1},
    // Prepared Sorcerer spells; Command comes only from Draconic Spells.
    SpellAccessRow{"Sorcerer", "burning_hands", 1},
    SpellAccessRow{"Sorcerer", "charm_person", 1},
    SpellAccessRow{"Sorcerer", "chromatic_orb", 1},
    SpellAccessRow{"Sorcerer", "color_spray", 1},
    SpellAccessRow{"Sorcerer", "expeditious_retreat", 1},
    SpellAccessRow{"Sorcerer", "false_life", 1},
    SpellAccessRow{"Sorcerer", "fog_cloud", 1},
    SpellAccessRow{"Sorcerer", "grease", 1},
    SpellAccessRow{"Sorcerer", "ice_knife", 1},
    SpellAccessRow{"Sorcerer", "mage_armor", 1},
    SpellAccessRow{"Sorcerer", "magic_missile", 1},
    SpellAccessRow{"Sorcerer", "ray_of_sickness", 1},
    SpellAccessRow{"Sorcerer", "shield", 1},
    SpellAccessRow{"Sorcerer", "sleep", 1},
    SpellAccessRow{"Sorcerer", "thunderwave", 1},
    SpellAccessRow{"Sorcerer", "sorcerous_burst", 1},
    SpellAccessRow{"Sorcerer", "blindness", 3},
    SpellAccessRow{"Sorcerer", "blur", 3},
    SpellAccessRow{"Sorcerer", "darkness", 3},
    SpellAccessRow{"Sorcerer", "dragons_breath", 3},
    SpellAccessRow{"Sorcerer", "enlarge_reduce", 3},
    SpellAccessRow{"Sorcerer", "flaming_sphere", 3},
    SpellAccessRow{"Sorcerer", "gust_of_wind", 3},
    SpellAccessRow{"Sorcerer", "hold_person", 3},
    SpellAccessRow{"Sorcerer", "invisibility", 3},
    SpellAccessRow{"Sorcerer", "knock", 3},
    SpellAccessRow{"Sorcerer", "magic_weapon", 3},
    SpellAccessRow{"Sorcerer", "mind_spike", 3},
    SpellAccessRow{"Sorcerer", "mirror_image", 3},
    SpellAccessRow{"Sorcerer", "misty_step", 3},
    SpellAccessRow{"Sorcerer", "scorching_ray", 3},
    SpellAccessRow{"Sorcerer", "see_invisibility", 3},
    SpellAccessRow{"Sorcerer", "shatter", 3},
    SpellAccessRow{"Sorcerer", "web", 3},
    SpellAccessRow{"Sorcerer", "command", 3},
    SpellAccessRow{"Bard", "true_strike", 1},
    SpellAccessRow{"Bard", "vicious_mockery", 1},
    SpellAccessRow{"Bard", "starry_wisp", 1},
    SpellAccessRow{"Bard", "bane", 1},
    SpellAccessRow{"Bard", "charm_person", 1},
    SpellAccessRow{"Bard", "color_spray", 1},
    SpellAccessRow{"Bard", "command", 1},
    SpellAccessRow{"Bard", "cure_wounds", 1},
    SpellAccessRow{"Bard", "dissonant_whispers", 1},
    SpellAccessRow{"Bard", "faerie_fire", 1},
    SpellAccessRow{"Bard", "healing_word", 1},
    SpellAccessRow{"Bard", "heroism", 1},
    SpellAccessRow{"Bard", "hideous_laughter", 1},
    SpellAccessRow{"Bard", "longstrider", 1},
    SpellAccessRow{"Bard", "sleep", 1},
    SpellAccessRow{"Bard", "thunderwave", 1},
    SpellAccessRow{"Bard", "aid", 3},
    SpellAccessRow{"Bard", "blindness", 3},
    SpellAccessRow{"Bard", "enlarge_reduce", 3},
    SpellAccessRow{"Bard", "hold_person", 3},
    SpellAccessRow{"Bard", "invisibility", 3},
    SpellAccessRow{"Bard", "knock", 3},
    SpellAccessRow{"Bard", "lesser_restoration", 3},
    SpellAccessRow{"Bard", "mirror_image", 3},
    SpellAccessRow{"Bard", "see_invisibility", 3},
    SpellAccessRow{"Bard", "shatter", 3},
    SpellAccessRow{"Bard", "silence", 3},
    SpellAccessRow{"Druid", "produce_flame", 1},
    SpellAccessRow{"Druid", "shillelagh", 1},
    SpellAccessRow{"Druid", "poison_spray", 1},
    SpellAccessRow{"Druid", "resistance", 1},
    SpellAccessRow{"Druid", "spare_the_dying", 1},
    SpellAccessRow{"Druid", "starry_wisp", 1},
    SpellAccessRow{"Druid", "charm_person", 1},
    SpellAccessRow{"Druid", "cure_wounds", 1},
    SpellAccessRow{"Druid", "entangle", 1},
    SpellAccessRow{"Druid", "faerie_fire", 1},
    SpellAccessRow{"Druid", "fog_cloud", 1},
    SpellAccessRow{"Druid", "goodberry", 1},
    SpellAccessRow{"Druid", "healing_word", 1},
    SpellAccessRow{"Druid", "ice_knife", 1},
    SpellAccessRow{"Druid", "longstrider", 1},
    SpellAccessRow{"Druid", "protection_from_evil_and_good", 1},
    SpellAccessRow{"Druid", "thunderwave", 1},
    SpellAccessRow{"Druid", "aid", 3},
    SpellAccessRow{"Druid", "barkskin", 3},
    SpellAccessRow{"Druid", "flame_blade", 3},
    SpellAccessRow{"Druid", "moonbeam", 3},
    SpellAccessRow{"Druid", "spike_growth", 3},
    SpellAccessRow{"Druid", "heat_metal", 3},
    SpellAccessRow{"Bard", "heat_metal", 3},
    SpellAccessRow{"Druid", "enlarge_reduce", 3},
    SpellAccessRow{"Druid", "flaming_sphere", 3},
    SpellAccessRow{"Druid", "gust_of_wind", 3},
    SpellAccessRow{"Druid", "hold_person", 3},
    SpellAccessRow{"Druid", "lesser_restoration", 3},
    SpellAccessRow{"Druid", "protection_from_poison", 3},
    // Circle Spells come only from the land chosen at level three.
    SpellAccessRow{"Druid", "blur", 3},
    SpellAccessRow{"Druid", "burning_hands", 3},
    SpellAccessRow{"Druid", "fire_bolt", 3},
    SpellAccessRow{"Druid", "ray_of_frost", 3},
    SpellAccessRow{"Druid", "misty_step", 3},
    SpellAccessRow{"Druid", "shocking_grasp", 3},
    SpellAccessRow{"Druid", "sleep", 3},
    SpellAccessRow{"Druid", "acid_splash", 3},
    SpellAccessRow{"Druid", "ray_of_sickness", 3},
    SpellAccessRow{"Druid", "web", 3},
    // Fiend Spells come only from the Fiend Patron.
    SpellAccessRow{"Warlock", "burning_hands", 3},
    SpellAccessRow{"Warlock", "command", 3},
    SpellAccessRow{"Warlock", "scorching_ray", 3},
    SpellAccessRow{"Warlock", "chill_touch", 1},
    SpellAccessRow{"Warlock", "eldritch_blast", 1},
    SpellAccessRow{"Warlock", "poison_spray", 1},
    SpellAccessRow{"Warlock", "true_strike", 1},
    SpellAccessRow{"Warlock", "bane", 1},
    SpellAccessRow{"Warlock", "charm_person", 1},
    SpellAccessRow{"Warlock", "expeditious_retreat", 1},
    SpellAccessRow{"Warlock", "hellish_rebuke", 1},
    SpellAccessRow{"Warlock", "hex", 1},
    SpellAccessRow{"Warlock", "hideous_laughter", 1},
    SpellAccessRow{"Warlock", "protection_from_evil_and_good", 1},
    SpellAccessRow{"Warlock", "darkness", 3},
    SpellAccessRow{"Warlock", "hold_person", 3},
    SpellAccessRow{"Warlock", "invisibility", 3},
    SpellAccessRow{"Warlock", "mind_spike", 3},
    SpellAccessRow{"Warlock", "mirror_image", 3},
    SpellAccessRow{"Warlock", "misty_step", 3},
    SpellAccessRow{"Warlock", "ray_of_enfeeblement", 3}};

std::vector<std::string> allowed_spells(std::string_view klass, unsigned level)
{
    std::vector<std::string> result;
    for (const auto &row : class_spell_access)
        if (row.klass == klass && level >= row.min_level)
            result.emplace_back(row.spell);
    return result;
}

using Dice = detail::DamageDice;

struct Definition
{
    int ac{}, hp{}, initiative{}, speed{}, melee_bonus{};
    bool alert{};
    int melee_ability{}, ranged_ability{}, size{2};
    std::string creature_type{"humanoid"}; // SRD creature type; player characters are Humanoid
    Dice melee;
    int ranged_bonus{};
    int reach{5};
    Dice ranged;
    int range{}, long_range{}, winds{}, slots{}, casting{}, level{}, slots2{};
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
    int hit_die{}, constitution{}, rushes{}, surges{}, arcane{};
    int lay_on_hands{}; // Paladin healing pool: five times Paladin level
    int free_smite{};   // Paladin's Smite: one Divine Smite without a slot per Long Rest
    int favored_enemy{}; // Favored Enemy: two Hunter's Marks without a slot per Long Rest
    int channel_divinity{};
    bool sacred_weapon{}; // Oath of Devotion, Paladin level 3
    bool divine_spark{};  // Cleric Channel Divinity: Divine Spark and Turn Undead
    bool life_domain{};   // Cleric level 3: Disciple of Life and Preserve Life
    bool evoker{};        // Wizard level 3: Potent Cantrip and Sculpt Spells
    int mage_armor_ac{};  // the AC Mage Armor gives; 0 while wearing armor
    bool sleepless{};     // an Elf: Sleep has no effect
    // Barbarian Rage: uses (kept in Actor::channel_divinity, which no Barbarian
    // has) and the bonus to Strength-based weapon damage.
    int rages{}, rage_damage{};
    int strength{};       // Strength modifier, to tell Strength-based attacks
    bool danger_sense{}, reckless{}; // Barbarian level 2
    // Monk Martial Arts: unarmored, unarmed or with only Monk weapons. The
    // modifier is the better of Strength and Dexterity.
    bool martial_arts{};
    int martial_modifier{};
    // Monk level 2: Focus Points (kept in Actor::surges, which no Monk has) and
    // Uncanny Metabolism's once-per-Long-Rest use (kept in Actor::arcane).
    int focus{}, metabolism{};
    int innate_sorcery{}; // Sorcerer: Innate Sorcery uses, in Actor::free_casts
    int sorcery_points{}; // Sorcerer level 2: Font of Magic, in Actor::lay_on_hands
    std::vector<std::string> metamagic; // the Sorcerer's Metamagic options
    bool pact_magic{};    // Warlock: slots of one level, back on a Short Rest
    // Eldritch Invocations.
    bool agonizing_blast{}, armor_of_shadows{}, devils_sight{}, eldritch_mind{},
         eldritch_spear{}, fiendish_vigor{}, pact_of_the_blade{}, repelling_blast{};
    int magical_cunning{}; // Warlock level 2: once per Long Rest, in Actor::arcane
    // Fiend Patron, Warlock level 3: Temporary Hit Points when an enemy drops.
    int dark_ones_blessing{};
    // Bard: Bardic Inspiration uses (Charisma modifier, at least one), kept in
    // Actor::arcane, which no Bard has; Cutting Words from level 3.
    int bardic_inspiration{};
    bool cutting_words{};
    bool shillelagh_weapon{}; // the melee weapon is a Club or Quarterstaff
    // Druid: Wild Shape uses, kept in Actor::channel_divinity, which no Druid
    // has; Land's Aid spends them from level 3.
    int wild_shapes{};
    bool lands_aid{};
    bool prone_bite{}, bloodied_fury{}; // a Beast form's attack traits
    bool deflect{};       // Monk level 3: Deflect Attacks
    bool open_hand{};     // Warrior of the Open Hand, Monk level 3
    int focus_dc{};       // 8 + Wisdom + Proficiency, for Focus features' saves
    int dexterity{};      // Dexterity modifier
    bool frenzy{};                   // Berserker, Barbarian level 3
    bool heavy_armor{};   // wearing Heavy armor, which prevents Rage
    // Hunter, Ranger level 3: Hunter's Lore and one Hunter's Prey option.
    bool hunters_lore{}, colossus_slayer{}, horde_breaker{};
    bool dwarf{}, cunning{}, tactical_mind{}, champion{}, great_weapon_fighting{},
         two_weapon_fighting{};
    unsigned sneak_level{};
    bool finesse{}, ranged_weapon{};
    int medicine{};
    int athletics{}; // a character's Strength (Athletics) check bonus
    detail::DamageType melee_type{detail::DamageType::bludgeoning},
           ranged_type{detail::DamageType::bludgeoning};
    std::vector<detail::DamageAffinity> affinities;
    std::vector<std::string> equipment_keys, masteries;
    // Monster traits from supplemental content rows.
    bool pack_tactics{}, aggressive{};
    Dice advantage_damage; // Extra weapon damage when the attack roll had Advantage.
};

struct CombatDisplay
{
    const char *type;
    const char *melee;
    const char *ranged;
};

CombatDisplay combat_display(std::string_view definition)
{
    if (definition == "slums-kobold")
        return {"Kobold", "Dagger", "Dagger"};
    if (definition == "slums-kobold-leader" || definition == "slums-kobold-leader-sword")
        return {"Kobold Leader", "Dagger", "Dagger"};
    if (definition == "bandit")
        return {"Bandit", "Scimitar", "Light crossbow"};
    if (definition == "slums-goblin")
        return {"Goblin Guard", "Scimitar", "Shortbow"};
    if (definition == "slums-goblin-leader")
        return {"Goblin Leader", "Scimitar", "Shortbow"};
    if (definition == "slums-orc")
        return {"Orc", "Greataxe", "Javelin"};
    if (definition == "slums-orc-leader")
        return {"Orc Leader", "Greataxe", "Javelin"};
    if (definition == "slums-orc-leader-archer")
        return {"Orc Leader", "Greataxe", "Longbow"};
    if (definition == "slums-bugbear")
        return {"Bugbear", "Grab", "Light hammer"};
    return {nullptr, nullptr, nullptr};
}

struct Content
{
    Identity identity;
    std::map<std::string, Definition> definitions;
};

struct Actor : detail::LifeState
{
    Participant source;
    Definition definition;
    int initiative{}, movement{}, winds{}, slots{}, slots2{};
    int hit_dice{}, rushes{}, surges{}, dashes{}, arcane{}, lay_on_hands{},
        // Remaining free casts of Paladin's Smite or Favored Enemy: a character has at
        // most one of them while multiclassing is deferred (SCOPE-1).
        free_casts{},
        channel_divinity{};
    // The creature this actor just hit with a melee weapon on its own turn, which
    // a smite may follow; cleared by any other command or a new turn.
    EntityId smite_target{};
    bool smite_critical{};
    bool smite_melee{}; // Divine and Searing Smite need a Melee hit; Ensnaring Strike any weapon hit
    bool rush_used{}, surge_used{};
    detail::ActionBudget actions;
    bool bonus{true}, reaction{true}, dodge{}, disengaged{};
    bool spent_slot{}, savage_used{}, facing_left{};
    bool sneak_used{}, aim_used{}, aim_ready{}, moved{};
    unsigned selected_weapon{};
    std::vector<unsigned> light_origins;
    unsigned nick_origin{}; // Light weapon used by the current Attack action.
    unsigned light_extra{}; // 0: unused, 1: Bonus Action, 2: Nick; shared once per turn.
    bool cleave_used{}, cleave_damage{};
    // Hunter's Prey, once per turn. horde_origin is the first creature this
    // actor attacked with a weapon this turn, which Horde Breaker attacks beside.
    bool colossus_used{}, horde_used{};
    bool resistance_used{}; // Resistance (the cantrip) reduces damage once per turn
    EntityId horde_origin{};
    detail::ConcentrationState concentration; // the one Concentration spell this actor keeps
    // Wild Shape: the Beast form's statistics, derived from the wild_shape effect.
    std::optional<Definition> form;
    bool light_damage{};        // Transient attack copy only; pending hits carry their own flag.
    bool involuntary_overlap{}; // Interrupted in an occupied space; retained through recovery until
    // separated.
    detail::EffectState effects;
};

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
    int short_rest;   // uses restored by a Short Rest; -1 means the full capacity
    bool combat_view; // also listed during combat, not only on the rest screen
};

constexpr std::array resource_descriptors
{
    ResourceDescriptor{"action_surge", "Action Surge", &Actor::surges, &Definition::surges, -1,
        true},
    ResourceDescriptor{"adrenaline_rush", "Adrenaline Rush", &Actor::rushes, &Definition::rushes,
        -1, true},
    ResourceDescriptor{"second_wind", "Second Wind", &Actor::winds, &Definition::winds, 1, false},
    ResourceDescriptor{"spell_slot:1", "Level-one spell slots", &Actor::slots, &Definition::slots,
        0, false},
    ResourceDescriptor{"spell_slot:2", "Level-two spell slots", &Actor::slots2, &Definition::slots2,
        0, false},
    ResourceDescriptor{"arcane_recovery", "Arcane Recovery", &Actor::arcane, &Definition::arcane, 0,
        false},
    ResourceDescriptor{"lay_on_hands", "Lay On Hands", &Actor::lay_on_hands,
        &Definition::lay_on_hands, 0, true},
    ResourceDescriptor{"paladins_smite", "Paladin's Smite", &Actor::free_casts,
        &Definition::free_smite, 0, true},
    ResourceDescriptor{"favored_enemy", "Favored Enemy", &Actor::free_casts,
        &Definition::favored_enemy, 0, true},
    ResourceDescriptor{"channel_divinity", "Channel Divinity", &Actor::channel_divinity,
        &Definition::channel_divinity, 1, true},
    ResourceDescriptor{"rage", "Rage", &Actor::channel_divinity, &Definition::rages, 1, true},
    ResourceDescriptor{"wild_shape", "Wild Shape", &Actor::channel_divinity,
        &Definition::wild_shapes, 1, true},
    ResourceDescriptor{"focus", "Focus Points", &Actor::surges, &Definition::focus, -1, true},
    ResourceDescriptor{"innate_sorcery", "Innate Sorcery", &Actor::free_casts,
        &Definition::innate_sorcery, 0, true},
    ResourceDescriptor{"sorcery_points", "Sorcery Points", &Actor::lay_on_hands,
        &Definition::sorcery_points, 0, true},
    ResourceDescriptor{"magical_cunning", "Magical Cunning", &Actor::arcane,
        &Definition::magical_cunning, 0, false},
    ResourceDescriptor{"bardic_inspiration", "Bardic Inspiration", &Actor::arcane,
        &Definition::bardic_inspiration, 0, true},
    ResourceDescriptor{"uncanny_metabolism", "Uncanny Metabolism", &Actor::arcane,
        &Definition::metabolism, 0, false}};

// The Hit Point maximum with Aid's increase.
int max_hp(const Actor &a)
{
    return a.definition.hp + detail::hit_point_bonus(a.effects);
}

// Lay On Hands and Sorcery Points share one store, restored by a Long Rest; no
// class has both.
int lay_capacity(const Definition &d)
{
    return d.lay_on_hands + d.sorcery_points;
}

// Spell slots, with room for the ones Font of Magic creates from Sorcery
// Points (2 for a level-1 slot, 3 for a level-2 one); they vanish on a Long Rest.
int slot_room(const Definition &d)
{
    return d.slots + d.sorcery_points / 2;
}

int slot2_room(const Definition &d)
{
    return d.slots2 + d.sorcery_points / 3;
}

// Paladin's Smite, Favored Enemy and Innate Sorcery share one store, restored
// by a Long Rest; no class has two of them.
int free_cast_capacity(const Definition &d)
{
    return d.free_smite + d.favored_enemy + d.innate_sorcery;
}

// Action Surge and Focus Points share one store; no class has both, and a
// Short Rest restores each fully.
int surge_capacity(const Definition &d)
{
    return d.surges + d.focus;
}

// Arcane Recovery, Uncanny Metabolism and Magical Cunning share one store,
// once per Long Rest.
int arcane_capacity(const Definition &d)
{
    return d.arcane + d.metabolism + d.magical_cunning + d.bardic_inspiration;
}

// Channel Divinity and Rage share one store; no class has both, and each
// regains one use on a Short Rest.
int channel_capacity(const Definition &d)
{
    return d.channel_divinity + d.rages + d.wild_shapes;
}

// Wild Shape: the Beast's AC, Speed, size, Strength, Dexterity, physical saves
// and attack replace the Druid's; Hit Points, mental scores, proficiencies
// and features stay. Equipment merges into the form, and no spells are cast.
Definition shaped(Definition d, const detail::BeastForm &form)
{
    d.ac = form.armor_class;
    d.mage_armor_ac = 0;
    d.speed = form.speed;
    d.size = form.size;
    d.strength = form.strength;
    d.dexterity = form.dexterity;
    std::copy(form.saves.begin(), form.saves.end(), d.saves.begin());
    d.melee = form.attack;
    d.melee_bonus = form.attack_bonus;
    d.melee_ability = form.attack.bonus;
    d.melee_type = detail::DamageType::piercing;
    d.reach = 5;
    d.weapon_label = std::string(form.attack_label);
    d.finesse = d.ranged_weapon = d.shield = d.other_weapon = false;
    d.weapon_hands = 0;
    d.versatile_sides = 0;
    d.ranged = {};
    d.ranged_bonus = d.range = d.long_range = 0;
    d.masteries.clear();
    d.savage = d.great_weapon_fighting = d.two_weapon_fighting = false;
    d.melee_heavy_disadvantage = d.ranged_heavy_disadvantage = false;
    d.str_dex_disadvantage = d.stealth_disadvantage = false;
    d.shillelagh_weapon = false;
    d.equipment_keys.clear();
    d.spells.clear();
    d.pack_tactics = form.pack_tactics;
    d.prone_bite = form.prone_bite;
    d.bloodied_fury = form.bloodied_fury;
    return d;
}

rules::ResourcePool resource_pool(const ResourceDescriptor &descriptor, const Actor &actor,
                                  const Definition &d)
{
    const auto capacity = unsigned(d.*descriptor.capacity);
    return {std::string(descriptor.id),
        {std::string(descriptor.label), {}},
        unsigned(actor.*descriptor.remaining),
        capacity,
        descriptor.short_rest < 0 ? capacity : unsigned(descriptor.short_rest)};
}

bool unconscious(const Actor &a)
{
    return a.hp == 0;
}

bool conscious(const Actor &a)
{
    return !a.dead && !unconscious(a);
}

struct ArcaneAllocation
{
    std::string_view id, label;
    unsigned first, second;
};

constexpr std::array arcane_allocations
{
    ArcaneAllocation{"arcane_recovery:1:0", "One level-one spell slot", 1, 0},
    ArcaneAllocation{"arcane_recovery:2:0", "Two level-one spell slots", 2, 0},
    ArcaneAllocation{"arcane_recovery:0:1", "One level-two spell slot", 0, 1}};

bool can_recover(const Actor &actor, const ArcaneAllocation &choice)
{
    const auto &d = actor.definition;
    return conscious(actor) && d.arcane && actor.arcane > 0 &&
           choice.first + 2 * choice.second <= unsigned((d.level + 1) / 2) &&
           choice.first <= unsigned(d.slots - actor.slots) &&
           choice.second <= unsigned(d.slots2 - actor.slots2);
}

int movement_left(const Actor &a)
{
    if (!conscious(a) || a.aim_used)
        return 0;
    const int penalty = std::min(a.definition.speed, detail::speed_penalty(a.effects));
    return std::max(0, a.movement - penalty * (1 + a.dashes));
}

struct ChampionMove
{
    EntityId actor{}, target{};
    int natural{}, remaining{};
    bool spell{};
    Cell origin;
    bool triggered{}, cleave{}, helpless{};
    Cell trigger_origin{}, target_origin{};
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
};

// `draconic`: a Draconic Sorcerer gains its level in Hit Points from level 3.
int maximum_hit_points(int die, bool dwarf, std::span<const int> modifiers, bool draconic)
{
    if (modifiers.empty() || modifiers.size() > 4 ||
            std::any_of(modifiers.begin(), modifiers.end(),
                        [](int n)
{
    return n < -4 || n > 5;
}))
    throw std::runtime_error("Invalid HP advancement history");
    int hp = die + modifiers.front() + (dwarf ? int(modifiers.size()) : 0) +
             (draconic && modifiers.size() >= 3 ? int(modifiers.size()) : 0);
    for (std::size_t i = 1; i < modifiers.size(); ++i)
    {
        const int increase = modifiers[i] - modifiers[i - 1];
        if (increase < 0 || increase > 1 || (i != 3 && increase))
            throw std::runtime_error("Invalid Constitution advancement history");
        // SRD p. 23: gain HP first, then apply a new modifier per attained level.
        hp += std::max(1, die / 2 + 1 + modifiers[i - 1]) + increase * int(i + 1);
    }
    return hp;
}

// Module-owned character recipe. Original item IDs never enter this layer.
Definition
character_definition(std::string_view bytes,
                     std::optional<std::span<const std::string>> equipment_override = std::nullopt)
{
    if (bytes.size() > 8192)
        throw std::runtime_error("Character profile exceeds limit");
    std::istringstream in{std::string(bytes)};
    std::string magic, klass, race;
    std::array<int, 6> scores{};
    unsigned count{}, level{}, features{}, listed{};
    in >> magic >> level >> features >> listed;
    // Untrusted input: bound the list and require every id to be a supported
    // spell before anything else looks at it.
    if (!in || magic != profile_magic || listed > detail::spell_table.size())
        throw std::runtime_error("Invalid character profile");
    std::vector<std::string> stored_spells;
    for (unsigned n = 0; n < listed; ++n)
    {
        std::string id;
        in >> id;
        if (!in || !detail::find_spell(id) || detail::knows_spell(stored_spells, id))
            throw std::runtime_error("Invalid character profile");
        stored_spells.push_back(std::move(id));
    }
    in >> std::quoted(klass) >> std::quoted(race);
    for (auto &score : scores)
        in >> score;
    if (!in || level < 1 || level > 4 || features > 63 ||
            std::any_of(scores.begin(), scores.end(),
                        [](int n)
{
    return n < 3 || n > 20;
}))
    throw std::runtime_error("Invalid character profile");
    if (level > 1 && klass != "Fighter" && klass != "Cleric" && klass != "Wizard" &&
            klass != "Rogue" && klass != "Paladin" && klass != "Ranger" && klass != "Barbarian" &&
            klass != "Monk" && klass != "Sorcerer" && klass != "Warlock" && klass != "Bard" &&
            klass != "Druid")
        throw std::runtime_error("Advancement is unsupported for this class");
    const auto races = character_rules()->choices(CreationField::race);
    if (std::none_of(races.begin(), races.end(),
                     [&](const auto & r)
{
    return r.label == race;
}))
    throw std::runtime_error("Unknown species");
    const int str = ability_modifier(scores[0]), dex = ability_modifier(scores[1]),
              con = ability_modifier(scores[2]);
    std::vector<int> hp_modifiers(level, con);
    for (auto &modifier : hp_modifiers)
        in >> modifier;
    in >> count;
    if (!in || count > 3 || hp_modifiers.back() != con)
        throw std::runtime_error("Invalid character HP history or equipment count");
    const auto classes = character_rules()->choices(CreationField::character_class);
    if (std::none_of(classes.begin(), classes.end(),
                     [&](const auto & c)
{
    return c.label == klass;
}))
    throw std::runtime_error("Unknown class");
    const int die = klass == "Barbarian"                                              ? 12
                    : (klass == "Fighter" || klass == "Paladin" || klass == "Ranger") ? 10
                    : (klass == "Wizard" || klass == "Sorcerer")                      ? 6
                    : 8;
    Definition d;
    d.hp = maximum_hit_points(die, race == "Dwarf", hp_modifiers, klass == "Sorcerer");
    d.hit_die = die;
    d.constitution = con;
    d.dwarf = race == "Dwarf";
    // Elves do not sleep, so Sleep cannot touch them.
    d.sleepless = race == "Elf";
    d.rages = klass == "Barbarian" ? (level >= 3 ? 3 : 2) : 0;
    d.wild_shapes = klass == "Druid" && level >= 2 ? 2 : 0;
    d.lands_aid = klass == "Druid" && level >= 3;
    d.rage_damage = klass == "Barbarian" ? 2 : 0;
    d.strength = str;
    d.danger_sense = d.reckless = klass == "Barbarian" && level >= 2;
    d.frenzy = klass == "Barbarian" && level >= 3;
    d.focus = klass == "Monk" && level >= 2 ? int(level) : 0;
    d.metabolism = klass == "Monk" && level >= 2 ? 1 : 0;
    d.innate_sorcery = klass == "Sorcerer" ? 2 : 0;
    d.sorcery_points = klass == "Sorcerer" && level >= 2 ? int(level) : 0;
    d.pact_magic = klass == "Warlock";
    d.magical_cunning = klass == "Warlock" && level >= 2 ? 1 : 0;
    d.bardic_inspiration = klass == "Bard" ? std::max(1, ability_modifier(scores[5])) : 0;
    d.cutting_words = klass == "Bard" && level >= 3;
    d.dark_ones_blessing =
        klass == "Warlock" && level >= 3 ? std::max(1, ability_modifier(scores[5]) + int(level)) : 0;
    d.deflect = d.open_hand = klass == "Monk" && level >= 3;
    d.focus_dc = 8 + 2 + ability_modifier(scores[4]);
    d.dexterity = dex;
    d.rushes = race == "Orc" ? 2 + (level - 1) / 4 : 0;
    const auto trained_saves = detail::class_save_proficiencies(klass);
    for (unsigned i = 0; i < 6; ++i)
        d.saves[i] = ability_modifier(scores[i]) +
                     ((i == trained_saves[0] || i == trained_saves[1]) ? 2 : 0);
    d.alert = (features & 32) != 0;
    d.ac = 10 + dex;
    // Jack of All Trades adds half the Proficiency Bonus to Initiative, as
    // SRD-DECISIONS keeps it, unless Alert already adds the whole bonus.
    d.initiative = dex + (d.alert ? int(2 + (level - 1) / 4)
                          : klass == "Bard" && level >= 2 ? 1 : 0);
    d.speed = race == "Goliath" ? 35 : 30;
    d.level = level;
    d.melee_bonus = 2 + str;
    d.melee = {0, 0, std::max(0, 1 + str)};
    d.champion = klass == "Fighter" && level >= 3;
    d.arcane = klass == "Wizard" ? 1 : 0;
    d.lay_on_hands = klass == "Paladin" ? 5 * level : 0;
    d.channel_divinity = (klass == "Paladin" && level >= 3) || (klass == "Cleric" && level >= 2) ? 2
                         : 0;
    d.sacred_weapon = klass == "Paladin" && level >= 3;
    d.divine_spark = klass == "Cleric" && level >= 2;
    d.life_domain = klass == "Cleric" && level >= 3;
    d.evoker = klass == "Wizard" && level >= 3;
    d.free_smite = klass == "Paladin" && level >= 2 ? 1 : 0;
    d.favored_enemy = klass == "Ranger" ? 2 : 0;
    d.medicine = ability_modifier(scores[4]);
    d.tactical_mind = klass == "Fighter" && level >= 2;
    d.cunning = klass == "Rogue" && level >= 2;
    d.sneak_level = klass == "Rogue" ? level : 0;
    d.great_weapon_fighting = (features & 8) != 0;
    d.two_weapon_fighting = (features & 16) != 0;
    d.surges = klass == "Fighter" && level >= 2 ? 1 : 0;
    d.winds = klass == "Fighter" ? (level == 4 ? 3 : 2) : 0;
    // Pact Magic: one level-1 slot, then two; level-2 slots from level 3.
    d.slots = klass == "Warlock" ? (level <= 2 ? int(level) : 0)
              : (klass == "Cleric" || klass == "Wizard" || klass == "Sorcerer" || klass == "Bard" ||
                 klass == "Druid")
              ? (level == 1 ? 2 : level == 2 ? 3 : 4)
              : (klass == "Paladin" || klass == "Ranger") ? (level <= 2 ? 2 : 3)
              : 0;
    d.slots2 = klass == "Warlock" && level >= 3 ? 2
               : (klass == "Cleric" || klass == "Wizard" || klass == "Sorcerer" || klass == "Bard" ||
                 klass == "Druid") &&
               level >= 3
               ? (level == 3 ? 2 : 3)
               : 0;
    d.casting = 2 + ability_modifier(scores[(klass == "Cleric" || klass == "Ranger" ||
                                             klass == "Druid") ? 4
                                            : (klass == "Warlock" || klass == "Sorcerer" ||
                                               klass == "Paladin") ? 5
                                            : 3]);
    const auto allowed = allowed_spells(klass, level);
    const bool eligible = std::all_of(stored_spells.begin(), stored_spells.end(),
                                      [&](const auto & id)
    {
        return detail::knows_spell(allowed, id);
    });
    if (!eligible || ((features & 1) && klass != "Fighter" && klass != "Paladin" &&
                      klass != "Ranger"))
        throw std::runtime_error("Invalid prepared spells or feat prerequisites");
    d.spells = stored_spells;
    d.savage = (features & 2) != 0;
    d.known_cantrips = detail::spells_of_level(d.spells, true);

    bool weapon = false, armor = false, shield = false;
    bool monk_weapons = true; // every wielded weapon is a Simple Melee or Light Martial one
    unsigned hands = 0;
    for (unsigned i = 0; i < count; ++i)
    {
        std::string key;
        in >> std::quoted(key);
        d.equipment_keys.push_back(std::move(key));
    }
    if (equipment_override)
        d.equipment_keys.assign(equipment_override->begin(), equipment_override->end());
    // Grants come before equipment because a Divine Order can add training.
    std::string background;
    in >> std::quoted(background);
    const auto grants = detail::read_grants(in);
    const auto invoked = [&](std::string_view id)
    {
        return std::any_of(grants.begin(), grants.end(), [&](const auto & g)
        {
            return g.id == "invocation:" + std::string(id);
        });
    };
    d.agonizing_blast = invoked("agonizing_blast");
    d.armor_of_shadows = invoked("armor_of_shadows");
    d.devils_sight = invoked("devils_sight");
    d.eldritch_mind = invoked("eldritch_mind");
    d.eldritch_spear = invoked("eldritch_spear");
    d.fiendish_vigor = invoked("fiendish_vigor");
    d.pact_of_the_blade = invoked("pact_of_the_blade");
    d.repelling_blast = invoked("repelling_blast");
    // Lessons of the First Ones: an Origin feat.
    d.savage |= invoked("lessons_savage_attacker");
    if (invoked("lessons_alert"))
    {
        d.alert = true;
        d.initiative = dex + int(2 + (level - 1) / 4);
    }
    for (const auto &key : d.equipment_keys)
    {
        if (const auto *item = detail::weapon(key))
        {
            monk_weapons &= item->dice && !item->ranged && (!item->martial || item->light);
            if (weapon)
            {
                if (d.other_weapon)
                    throw std::runtime_error("Only two weapons may be equipped");
                d.other_weapon = true;
                hands += item->hands;
                continue;
            }
            weapon = true;
            d.weapon_label = item->label;
            d.shillelagh_weapon = key == "club" || key == "quarterstaff";
            d.finesse = item->finesse;
            d.ranged_weapon = item->ranged;
            d.weapon_hands = item->hands;
            d.versatile_sides = item->versatile_sides;
            hands += d.weapon_hands;
            // Pact of the Blade: a melee pact weapon uses Charisma if better,
            // with proficiency.
            const bool pact = d.pact_of_the_blade && !item->ranged;
            const int modifier = std::max(item->finesse ? std::max(str, dex) : item->ranged ? dex : str,
                                          pact ? ability_modifier(scores[5]) : -5);
            const int bonus = (trained(klass, grants, key) || pact ? 2 : 0) + modifier;
            if (item->dice && !item->ranged)
            {
                d.melee_ability = modifier;
                d.melee_bonus = bonus;
                d.melee = {item->dice, item->sides, modifier};
                d.melee_type = item->type;
                d.reach = item->reach;
                d.melee_heavy_disadvantage = item->heavy_disadvantage(scores);
            }
            if (item->range)
            {
                d.ranged_ability = modifier;
                d.ranged_bonus = bonus + ((item->ranged && (features & 4)) ? 2 : 0);
                d.ranged = {item->dice, item->sides,
                            item->fixed_damage ? item->fixed_damage : modifier
                           };
                d.ranged_type = item->type;
                d.range = item->range;
                d.long_range = item->long_range;
                d.ranged_heavy_disadvantage = item->heavy_disadvantage(scores);
            }
        }
        else if (const auto *item = detail::armor(key);
                 item && item->category != detail::ArmorCategory::shield)
        {
            if (armor)
                throw std::runtime_error("Only one armor may be equipped");
            if (!trained(klass, grants, key))
            {
                d.str_dex_disadvantage = true;
                d.spells.clear();
            }
            armor = true;
            d.heavy_armor = item->category == detail::ArmorCategory::heavy;
            d.ac = item->base_ac + item->dexterity_contribution(dex);
            d.stealth_disadvantage = item->stealth_disadvantage;
            if (scores[0] < item->strength)
                d.speed -= 10;
        }
        else if (key == "shield")
        {
            if (shield)
                throw std::runtime_error("Only one shield may be equipped");
            shield = true;
            ++hands;
        }
        else
            throw std::runtime_error("Unsupported equipment conversion: " + key);
    }
    d.shield = shield;
    // The grip follows the other hand at attack time: a Versatile weapon is
    // wielded two-handed exactly when no shield or second weapon is held.
    if (d.versatile_sides && !shield && !d.other_weapon)
    {
        hands = hands - d.weapon_hands + 2;
        d.weapon_hands = 2;
    }
    const auto training = detail::training_profile(grants, detail::grant_source_id(klass),
                          background, level, scores);
    d.medicine = std::find_if(training.skills.begin(), training.skills.end(),
                              [](const auto & skill)
    {
        return skill.id == "medicine";
    })
    ->bonus;
    d.athletics = std::find_if(training.skills.begin(), training.skills.end(),
                               [](const auto & skill)
    {
        return skill.id == "athletics";
    })
    ->bonus;
    for (const auto &grant : grants)
    {
        if (detail::is_mastery_grant(grant))
            d.masteries.push_back(grant.id.substr(8));
        if (grant.id.starts_with("metamagic:"))
            d.metamagic.push_back(grant.id.substr(10));
        d.hunters_lore |= grant.id == "feature:hunters_lore";
        d.colossus_slayer |= grant.id == "prey:colossus_slayer";
        d.horde_breaker |= grant.id == "prey:horde_breaker";
    }
    std::vector<std::string> prepared;
    if (detail::prepares_spells(klass))
    {
        // Always-prepared spells are stored with the others but never prepared.
        prepared = detail::spells_of_level(stored_spells, false);
        for (const auto &id : detail::always_prepared_spells(klass, level, grants))
            std::erase(prepared, id);
    }
    const auto access = detail::spell_access(grants, klass, level, prepared);
    // Compare as sets: the grants and the stored list must describe the same
    // spells, independent of the order either was written in.
    const auto same_spells = [](std::vector<std::string> left, std::vector<std::string> right)
    {
        std::sort(left.begin(), left.end());
        std::sort(right.begin(), right.end());
        return left == right;
    };
    if (detail::prepares_spells(klass) &&
            !same_spells(detail::casting_ids(access), stored_spells))
        throw std::runtime_error("Character casting access disagrees with spell grants");
    const auto features_only = detail::without_spell_grants(detail::without_training(grants));
    const auto effects = detail::validate_grants(features_only, detail::grant_source_id(klass),
                         detail::grant_source_id(race), background, level);
    if (effects.feats != features)
        throw std::runtime_error("Character effects disagree with acquired grants");
    const int initial_con = ability_modifier(scores[2] - effects.abilities[2]);
    for (unsigned i = 0; i < level; ++i)
        if (hp_modifiers[i] != (i == 3 ? con : initial_con))
            throw std::runtime_error("HP history disagrees with acquired ability choices");
    if (race == "Dwarf")
        d.affinities.push_back({detail::AffinityKind::resistance, detail::DamageType::poison,
                                "species:dwarf/trait:dwarven_resilience"});
    if (hands > 2)
        throw std::runtime_error(
            "Not enough free hands. Unequip the shield or two-handed weapon first.");
    if (!armor && klass == "Barbarian")
        d.ac = std::max(d.ac, 10 + dex + con);
    // Draconic Resilience: dragon-like scales.
    if (!armor && klass == "Sorcerer" && level >= 3)
        d.ac = std::max(d.ac, 10 + dex + ability_modifier(scores[5]));
    if (!armor && !shield && klass == "Monk")
        d.ac = std::max(d.ac, 10 + dex + ability_modifier(scores[4]));
    // Martial Arts: the better of Strength and Dexterity for attack and damage
    // rolls, and the Martial Arts die (a d6 through level four) when larger.
    d.martial_arts = klass == "Monk" && !armor && !shield && monk_weapons;
    // Unarmored Movement: 10 feet more without armor or a Shield.
    if (klass == "Monk" && level >= 2 && !armor && !shield)
        d.speed += 10;
    if (d.martial_arts)
    {
        d.martial_modifier = std::max(str, dex);
        if (!weapon)
        {
            d.melee = {1, 6, d.martial_modifier};
            d.melee_bonus = 2 + d.martial_modifier;
        }
        else
        {
            d.melee_bonus += d.martial_modifier - d.melee_ability;
            d.melee.bonus = d.martial_modifier;
            d.melee.sides = std::max(d.melee.sides, 6);
        }
        d.melee_ability = d.martial_modifier;
    }
    if (shield && trained(klass, grants, "shield"))
        d.ac += 2;
    // Mage Armor's AC for a creature wearing no armor.
    if (!armor)
        d.mage_armor_ac = 13 + dex + (shield && trained(klass, grants, "shield") ? 2 : 0);
    if (armor && (features & 1))
        ++d.ac;
    in >> std::ws;
    if (!in.eof())
        throw std::runtime_error("Invalid character profile fields");
    return d;
}

// The only vital-state tag this module reads or writes; see profile_magic.
constexpr std::string_view vitals_magic = "SRD11";

void restore_vitals(Actor &a, const VitalState &state)
{
    a.hp = state.hit_points;
    a.dead = state.dead;
    // Empty resources describe a fresh character: every pool and Hit Die is
    // full. Callers supply the full Second Wind and spell-slot values.
    a.hit_dice = a.definition.hit_die ? a.definition.level : 0;
    a.rushes = a.definition.rushes;
    a.surges = surge_capacity(a.definition);
    a.arcane = arcane_capacity(a.definition);
    a.lay_on_hands = lay_capacity(a.definition);
    a.free_casts = free_cast_capacity(a.definition);
    a.channel_divinity = channel_capacity(a.definition);
    if (!state.resources.empty())
    {
        std::istringstream in(state.resources);
        std::string magic;
        in >> magic >> a.winds >> a.slots >> a.slots2 >> a.successes >> a.failures >> a.stable >>
           a.hit_dice >> a.recovery.death_save_in_ms >> a.recovery.stable_recovery_in_ms >>
           a.temporary_hp.amount >> std::quoted(a.temporary_hp.source_id) >> a.rushes >>
           a.surges >> a.arcane >> a.lay_on_hands >> a.free_casts >> a.channel_divinity;
        if (!in || magic != vitals_magic)
            throw std::runtime_error("Invalid character resource state");
        detail::decode_stable_recovery(a.recovery);
        a.effects = detail::read_effects(in);
        in >> std::ws;
        if (!in.eof())
            throw std::runtime_error("Trailing character resource state");
    }
    const auto &d = a.definition;
    if (a.hp < 0 || a.hp > max_hp(a) || (a.dead && a.hp != 0) || a.winds < 0 || a.winds > d.winds ||
            a.slots < 0 || a.slots > slot_room(d) || a.arcane < 0 || a.arcane > arcane_capacity(d) ||
            a.slots2 < 0 || a.slots2 > slot2_room(d) || a.surges < 0 ||
            a.surges > surge_capacity(d) || a.rushes < 0 ||
            a.rushes > d.rushes || a.hit_dice < 0 || a.hit_dice > (d.hit_die ? d.level : 0) ||
            a.successes < 0 || a.successes > 3 || a.failures < 0 || a.failures > 4 ||
            a.lay_on_hands < 0 || a.lay_on_hands > lay_capacity(d) || a.free_casts < 0 ||
            a.free_casts > free_cast_capacity(d) || a.channel_divinity < 0 ||
            a.channel_divinity > channel_capacity(d))
        throw std::runtime_error("Invalid character vitals");
    detail::validate_recovery(a);
    detail::validate_temporary_hp(a.temporary_hp);
    if (a.recovery.stable_recovery_due && !detail::healing_blocked(a.effects))
        throw std::runtime_error("Earned recovery requires active healing prevention");
}

VitalState vitals(const Actor &a)
{
    std::ostringstream out;
    out << vitals_magic << ' ' << a.winds << ' ' << a.slots << ' ' << a.slots2 << ' '
        << a.successes << ' ' << a.failures << ' ' << a.stable << ' ' << a.hit_dice << ' '
        << a.recovery.death_save_in_ms << ' ' << detail::encode_stable_recovery(a.recovery) << ' '
        << a.temporary_hp.amount << ' ' << std::quoted(a.temporary_hp.source_id) << ' '
        << a.rushes << ' ' << a.surges << ' ' << a.arcane << ' ' << a.lay_on_hands << ' '
        << a.free_casts << ' ' << a.channel_divinity << ' ';
    detail::write_effects(out, a.effects);
    std::string description;
    if (a.definition.slots)
        description = "Level-one spell slots: " + std::to_string(a.slots) + " / " +
                      std::to_string(a.definition.slots);
    if (a.definition.slots2)
        description += "\nLevel-two spell slots: " + std::to_string(a.slots2) + " / " +
                       std::to_string(a.definition.slots2);
    if (a.definition.winds)
        description = "Second Wind uses: " + std::to_string(a.winds) + " / " +
                      std::to_string(a.definition.winds);
    if (a.definition.arcane)
        description += (description.empty() ? "" : "\n") + std::string("Arcane Recovery uses: ") +
                       std::to_string(a.arcane) + " / 1";
    if (a.definition.surges && a.surges < a.definition.surges)
        description += (description.empty() ? "" : "\n") + std::string("Action Surge uses: ") +
                       std::to_string(a.surges) + " / " + std::to_string(a.definition.surges);
    if (a.hp == 0)
        description += (description.empty() ? "" : "\n") +
                       std::string(a.dead     ? "Dead"
                                   : a.stable ? "Stable, unconscious"
                                   : "Unconscious; death saves ") +
                       (!a.dead && !a.stable ? std::to_string(a.successes) + " successes, " +
                        std::to_string(a.failures) + " failures"
                        : "");
    if (detail::healing_blocked(a.effects))
        description += "\nChill Touch: cannot regain HP.";
    if (detail::opportunity_blocked(a.effects))
        description += "\nShocking Grasp: cannot make Opportunity Attacks.";
    if (detail::frosted(a.effects))
        description += "\nRay of Frost: Speed reduced by 10 feet.";
    if (detail::slowed(a.effects))
        description += "\nSlow: Speed reduced by 10 feet.";
    if (detail::blinded(a.effects))
        description += "\nBlinded";
    if (a.effects.prone)
        description += "\nProne";
    return {a.hp, a.dead, out.str(), description};
}

int distance(Cell a, Cell b)
{
    return std::max(std::abs(a.x - b.x), std::abs(a.y - b.y)) * 5;
}

bool same_command(const Command &a, const Command &b)
{
    return a.revision == b.revision && a.actor == b.actor && a.target == b.target &&
           a.verb == b.verb && a.destination == b.destination && a.item == b.item;
}

bool turns_to_attack(std::string_view verb)
{
    // A caster turns toward a foe, not toward an ally being healed, so every
    // spell except the ally-targeted ones counts alongside weapon attacks.
    if (verb.starts_with("light_") || verb.starts_with("nick_") || verb == "throw" ||
            verb == "melee" || verb == "ranged")
        return true;
    const auto *spell = detail::find_spell(verb);
    return spell && spell->target != detail::SpellTarget::wounded_ally;
}

class Session final : public CombatSession
{
  public:
    Session(std::shared_ptr<const Content> content, Encounter encounter, std::uint64_t seed,
            bool restoring = false)
        : content_(std::move(content)), board_(std::move(encounter.battlefield)), rng_(seed),
          scope_(encounter.scope)
    {
        if (!scope_)
            throw std::runtime_error("Invalid encounter scope");
        detail::validate_battlefield(board_);
        if (encounter.participants.size() < 2 || encounter.participants.size() > 64)
            throw std::runtime_error("Invalid encounter size");
        std::set<EntityId> ids;
        std::set<Cell> cells;
        std::set<unsigned> sides;
        for (auto &p : encounter.participants)
        {
            if (!p.id || !ids.insert(p.id).second || (!cells.insert(p.cell).second && !restoring) ||
                    board_.at(p.cell) == 1 || p.side > 1 || p.name.empty() || p.name.size() > 160 ||
                    (p.character_profile.empty() && !content_->definitions.contains(p.definition)))
                throw std::runtime_error("Invalid participant or unsupported rules definition: " +
                                         p.definition);
            sides.insert(p.side);
            const auto d = p.character_profile.empty() ? content_->definitions.at(p.definition)
                           : character_definition(p.character_profile);
            Actor a;
            a.definition = d;
            a.source = std::move(p);
            a.hp = d.hp;
            a.winds = d.winds;
            a.slots = d.slots;
            a.slots2 = d.slots2;
            a.hit_dice = d.hit_die ? d.level : 0;
            a.rushes = d.rushes;
            a.surges = surge_capacity(d);
            a.arcane = arcane_capacity(d);
            a.lay_on_hands = lay_capacity(d);
            a.free_casts = free_cast_capacity(d);
            a.channel_divinity = channel_capacity(d);
            a.facing_left = a.source.facing_left;
            if (a.source.state)
                restore_vitals(a, *a.source.state);
            a.initiative =
                detail::d20({d.champion, d.str_dex_disadvantage || a.source.surprised}, rng_) +
                d.initiative;
            a.movement = d.speed;
            actors_.push_back(std::move(a));
        }
        if (sides.size() != 2)
            throw std::runtime_error("Encounter needs both sides");
        // Fixed tie adjudication: descending initiative, then stable entity ID.
        std::stable_sort(actors_.begin(), actors_.end(),
                         [](const Actor & a, const Actor & b)
        {
            return a.initiative != b.initiative ? a.initiative > b.initiative
                   : a.source.id < b.source.id;
        });
        log("Combat begins. Each square is 5 feet.");
        if (!restoring)
            wake_resting_participants();
        update_outcome();
        frost_movement_ = std::any_of(
                              actors_.begin(), actors_.end(),
                              [](const auto & a)
        {
            return a.definition.cunning ||
                   (detail::knows_spell(a.definition.known_cantrips, "ray_of_frost")) ||
                   detail::speed_penalty(a.effects);
        });
        if (!restoring)
        {
            for (const auto &a : actors_)
            {
                for (const auto &key : a.definition.equipment_keys)
                    if (const auto *w = detail::weapon(key); w && (w->thrown || w->light))
                        physical_inventory_ = true;
                for (const auto &item : a.source.inventory)
                    if (const auto *w = detail::weapon(item.definition);
                            w && (w->thrown || w->light))
                        physical_inventory_ = true;
            }
            for (const auto &a : actors_)
                if (a.definition.other_weapon)
                    physical_inventory_ = true;
            if (physical_inventory_)
                initialize_items();
        }
        if (!restoring && std::any_of(actors_.begin(), actors_.end(),
                                      [](const auto & a)
    {
        return a.definition.two_weapon_fighting;
    }))
        activate_light();
        if (!restoring)
        {
            if (outcome_ == Outcome::ongoing)
                for (const auto &a : actors_)
                    if (a.source.side == 0 && (def(a).alert || metabolism_ready(a)) &&
                            conscious(a))
                        initiative_choices_.push_back(a.source.id);
            if (initiative_choices_.empty())
                start_encounter_turns();
        }
    }

    Snapshot snapshot() const override;
    std::vector<Command> legal_commands() const override;
    std::vector<Cell> movement_reach(EntityId actor) const override;
    bool submit(const Command &command) override;
    std::string save() const override;
    static std::unique_ptr<Session> restore(std::shared_ptr<const Content> content,
                                            std::string_view bytes);

  private:
    std::shared_ptr<const Content> content_;
    Battlefield board_;
    std::vector<Actor> actors_;
    std::uint64_t rng_{}, revision_{1};
    std::uint64_t scope_{1}, elapsed_ms_{};
    unsigned turn_{}, round_{1};
    Outcome outcome_{Outcome::ongoing};
    std::optional<TemporaryHitPoints> temporary_offer_;
    std::optional<PendingCheck> check_choice_;
    std::optional<ChampionMove> champion_move_;
    std::optional<PendingGraze> graze_;
    std::optional<PendingMastery> mastery_;
    // Creatures chosen so far for a spell that affects several (CLASS-2).
    struct PendingSelection
    {
        EntityId caster{};
        std::string verb;
        std::vector<EntityId> chosen;
    };
    std::optional<PendingSelection> selection_;
    // An area spell being aimed (CLASS-5); nothing is spent until it is cast.
    struct PendingArea
    {
        EntityId caster{};
        std::string verb;
        Cell center;
    };
    std::optional<PendingArea> area_;
    // A spell's lasting area, which lasts as long as its caster's Concentration
    // or, for Grease, a minute: Entangle's plants, Grease and Web are Difficult
    // Terrain, Fog Cloud is Heavily Obscured.
    enum class ZoneKind : unsigned
    {
        plants,
        fog,
        silence,
        spiritual_weapon, // one square: where the spectral force floats
        grease,
        web,
        flaming_sphere, // one square: where the fire burns
        gust,           // Gust of Wind's line
        darkness,       // magical Darkness, Heavily Obscured except to Devil's Sight
        moonbeam,       // Moonbeam's beam; cells.front() is its centre
        spikes          // Spike Growth's Difficult Terrain
    };
    struct Zone
    {
        EntityId caster{};
        ZoneKind kind{};
        std::vector<Cell> cells;
        std::uint64_t ends_ms{}; // combat time it vanishes; 0 while held by Concentration
    };
    std::vector<Zone> zones_;
    std::vector<ChampionMove> champion_offers_;
    std::optional<EffectReaction> effect_reaction_origin_;
    // Reactions to a hit (Shield, Deflect Attacks) land deep inside a command.
    // The command is undone at that moment and the creature asked; its answer
    // replays the command with the same dice.
    enum class Asked : unsigned
    {
        hit,      // an attack roll hit: Shield or Deflect Attacks
        missile,  // targeted by Magic Missile: Shield
        redirect, // Deflect Attacks stopped all the damage: redirect it for 1 Focus
        rebuke,      // an attack damaged a Warlock: Hellish Rebuke
        inspiration, // an inspired creature failed an attack roll or save
        cutting      // an enemy's attack roll hit: a Lore Bard's Cutting Words
    };
    struct ReactionQuestion
    {
        EntityId target{};
        Asked asked{};
        bool deflectable{}; // the hit deals Bludgeoning, Piercing or Slashing damage
        bool critical{};    // a hit Shield cannot turn aside
    };
    // A creature's answer during the command being replayed: "shield",
    // "deflect", "redirect" or "decline".
    struct ReactionAnswer
    {
        EntityId target{};
        Asked asked{};
        std::string verb;
    };
    struct PendingReaction
    {
        ReactionQuestion question;
        Command command;
        std::vector<ReactionAnswer> answers;
    };
    std::optional<PendingReaction> reaction_prompt_;
    std::vector<ReactionAnswer> reaction_answers_;
    [[nodiscard]] const ReactionAnswer *answer_of(EntityId target, Asked asked) const;
    [[nodiscard]] bool can_shield(const Actor &target) const;
    [[nodiscard]] bool can_deflect(const Actor &target, bool deflectable) const;
    void ask_reaction(const Actor &target, Asked asked, bool deflectable = false,
                      bool critical = false) const;
    int deflected(const Actor &attacker, Actor &target, int amount, detail::DamageType type,
                  bool ranged);
    [[nodiscard]] bool can_rebuke(const Actor &warlock, const Actor &attacker) const;
    // Bardic Inspiration's die added to a failed roll, if the creature uses it.
    int inspiration_roll(const Actor &creature);
    // Cutting Words' die taken from an attack roll, if a Bard uses it.
    int cutting_words(const Actor &attacker);
    void bless_fiends(const Actor &fallen);
    void rebuke(const Actor &attacker, Actor &warlock);
    void perform(const Command &command);
    void dispatch(const Command &command);
    void answer_reaction(const Command &command);

    bool effect_waiting() const
    {
        return mastery_.has_value() || !champion_offers_.empty();
    }

    Actor mastery_actor(const PendingMastery &) const;
    std::vector<EntityId> cleave_targets(const PendingMastery &,
                                         std::optional<Cell> from = {}) const;
    std::vector<Cell> push_cells(const PendingMastery &, std::optional<Cell> from = {}) const;
    bool mastery_available(const PendingMastery &) const;
    OptionalEffectChoice effect_choices() const;
    bool use_effect(const Command &);
    void finish_effects();
    void validate_mastery_state() const;
    void offer_mastery(const Actor &, const Actor &, int natural, bool ranged, int damage,
                       bool critical);
    void validate_graze() const;

    int graze_damage(const Actor &a, const Actor &target) const
    {
        const int cap = std::max(0, def(a).melee_ability);
        const std::array parts{detail::DamagePart{def(a).melee_type, cap}};
        auto defenses = affinities(target);
        // The approved Graze policy permits reductions, never vulnerability increases.
        std::erase_if(defenses,
                      [](const auto & affinity)
        {
            return affinity.kind == detail::AffinityKind::vulnerability;
        });
        return std::min(cap, detail::resolve_damage(parts, defenses).total);
    }

    void finish_champion_move();
    void validate_champion_move() const;
    void finish_check(const PendingCheck &check, int boost);
    void validate_check() const;
    bool frost_movement_{}, items_active_{}, physical_inventory_{}, light_active_{}, nick_active_{};
    unsigned held_weapon(const Actor &) const;
    void activate_light();
    bool light_eligible(const Actor &, unsigned item) const;
    bool has_nick(const Actor &) const;
    bool nick_weapon(const Actor &, unsigned item) const;
    void qualify_light(Actor &, unsigned item);
    Actor item_actor(const Actor &, unsigned item) const;
    Actor unarmed_actor(const Actor &a) const;
    void use_focus_movement(Actor &a, std::string_view verb);
    void use_font_of_magic(Actor &a, std::string_view verb);
    void ready_metamagic(Actor &a, std::string_view choice);
    void open_hand(const Actor &monk, Actor &target, std::string_view verb);
    bool weapon_reaction(const Actor &, const Definition &) const;
    bool has_weapon_reaction(const Actor &, Cell, Cell) const;
    void validate_light() const;
    Actor thrown_actor(const Actor &, std::string_view weapon) const;
    void throw_weapon(Actor &, Actor &, unsigned item, bool light = false);
    std::vector<HeldItemView> items_;
    void initialize_items();
    Definition equipped_definition(const Actor &a, const std::vector<HeldItemView> &items) const;
    std::vector<std::string> log_;
    std::vector<Message> log_messages_;
    std::vector<Cell> path_;
    std::size_t path_index_{};
    std::vector<EntityId> reactors_;
    std::size_t reactor_index_{};

    const Definition &def(const Actor &a) const
    {
        return a.form ? *a.form : a.definition;
    }

    const Actor &actor(EntityId id) const
    {
        return *std::find_if(actors_.begin(), actors_.end(),
                             [&](const auto & a)
        {
            return a.source.id == id;
        });
    }

    Actor &actor(EntityId id)
    {
        return *std::find_if(actors_.begin(), actors_.end(),
                             [&](const auto & a)
        {
            return a.source.id == id;
        });
    }

    int roll(int sides)
    {
        return roll_die(rng_, sides);
    }

    int dice(Dice d, bool critical = false)
    {
        return detail::roll_damage(rng_, d, critical);
    }

    void log(std::string english, Message message = {})
    {
        if (message.source.empty())
            message.source = english;
        if (log_.size() == 80)
        {
            log_.erase(log_.begin());
            log_messages_.erase(log_messages_.begin());
        }
        log_.push_back(std::move(english));
        log_messages_.push_back(std::move(message));
    }

    bool line_of_sight(Cell a, Cell b) const
    {
        return detail::has_line_of_sight(board_, a, b);
    }

    // Heavily Obscured squares block sight into and out of them.
    bool can_see(const Actor &a, const Actor &b) const
    {
        return conscious(a) && !detail::blinded(a.effects) && !obscured(a, a.source.cell) &&
               !obscured(a, b.source.cell) && line_of_sight(a.source.cell, b.source.cell) &&
               !unseen(a, b);
    }

    // An Invisible creature hides from all but those with See Invisibility.
    static bool unseen(const Actor &viewer, const Actor &target)
    {
        // Faerie Fire and Starry Wisp keep a creature from being Invisible.
        return detail::has_effect(target.effects, detail::EffectKind::invisible) &&
               !detail::has_effect(viewer.effects, detail::EffectKind::see_invisibility) &&
               !detail::has_effect(target.effects, detail::EffectKind::outlined) &&
               !detail::has_effect(target.effects, detail::EffectKind::lit);
    }

    // Invisibility ends right after its creature casts a spell, once any
    // attack roll the spell makes has had the benefit. Casting Invisibility on
    // oneself starts it rather than ending it.
    class InvisibilityEnds
    {
      public:
        InvisibilityEnds(Session &session, const Actor &caster)
            : session_(session), id_(caster.source.id),
              invisible_(detail::has_effect(caster.effects, detail::EffectKind::invisible))
        {
        }

        InvisibilityEnds(const InvisibilityEnds &) = delete;
        InvisibilityEnds &operator=(const InvisibilityEnds &) = delete;

        ~InvisibilityEnds()
        {
            if (invisible_)
                session_.end_invisibility(id_);
        }

      private:
        Session &session_;
        EntityId id_;
        bool invisible_;
    };
    void end_invisibility(EntityId id);
    // The Barbarian's Rage effect, if it is raging; borrowed from its effects.
    [[nodiscard]] static const detail::Effect *rage_of(const Actor &a);
    [[nodiscard]] unsigned rage_duration(const Actor &a) const;
    void extend_rage(Actor &a);

    bool enemy_in_sight(const Actor &a) const
    {
        return std::any_of(actors_.begin(), actors_.end(), [&](const auto & other)
        {
            return other.source.side != a.source.side && conscious(other) && can_see(a, other);
        });
    }

    unsigned turn_end_ms(std::size_t index) const
    {
        return unsigned((index + 1) * detail::round_ms / actors_.size());
    }

    bool shares_occupied_space(const Actor &who) const
    {
        return std::any_of(actors_.begin(), actors_.end(),
                           [&](const auto & other)
        {
            return other.source.id != who.source.id && !other.dead &&
                   other.source.cell == who.source.cell;
        });
    }

    void clear_departed_overlaps()
    {
        for (auto &a : actors_)
            if (a.dead || !shares_occupied_space(a))
                a.involuntary_overlap = false;
    }

    // Emits every table spell whose row matches `scope` and pass. Called once
    // per original offer position so the observable order is unchanged.
    void offer_spells(std::vector<Command> &commands, const Actor &a, const Actor &other, int feet,
                      detail::SpellTarget scope, bool bonus_pass) const;
    // Resolves one table spell. `verb` is the offered command: it names the
    // "_2" level-two slot form and Command's option.
    void resolve_spell(const detail::SpellDef &spell, std::string_view verb, Actor &a,
                       EntityId target_id);
    void apply_rider(const detail::SpellDef &spell, std::string_view verb, Actor &a, Actor &target,
                     int dc);
    // Command's Approach and Flee: the commanded creature only moves toward or
    // away from the caster, then ends its turn.
    void obey_command(std::vector<Command> &commands, const Actor &a) const;
    [[nodiscard]] bool marked_by(const Actor &target, const Actor &caster) const;
    void reveal_lore(const Actor &caster, const Actor &target);
    void resolve_ensnaring_strike(Actor &a);
    void divine_spark(Actor &cleric, Actor &target);
    [[nodiscard]] int disciple_of_life(const Actor &caster, unsigned slot_level) const;
    void preserve_life(Actor &cleric);
    void turn_undead(Actor &cleric);
    void flee_turning(std::vector<Command> &commands, const Actor &a) const;
    void hold_still(std::vector<Command> &commands, const Actor &a) const;
    // `verb` names the spell's "_2" form, which a sphere grows with.
    // `origin` is the caster's square, from which cones and cubes extend.
    [[nodiscard]] std::vector<Cell> area_cells(const detail::SpellDef &spell,
            std::string_view verb, Cell origin, Cell center) const;
    // Whether `viewer` cannot see into the square.
    [[nodiscard]] bool obscured(const Actor &viewer, Cell cell) const;
    // Inside Silence: no Thunder damage.
    [[nodiscard]] bool silenced(Cell cell) const;
    [[nodiscard]] Cell spectral_cell(const Actor &target, Cell from) const;
    [[nodiscard]] const Zone *spiritual_weapon(const Actor &caster) const;
    [[nodiscard]] Cell default_area_center(const Actor &caster, const detail::SpellDef &spell) const;
    void aim_area(const Command &command);
    void damage_area(Actor &caster, const detail::SpellDef &spell, std::string_view verb,
                     const std::vector<Cell> &cells, int dc);
    [[nodiscard]] int area_dc(const Actor &caster, const detail::SpellDef &spell) const;
    void push_away(const Actor &from, Actor &target, int squares);
    void ice_burst(Actor &caster, const Actor &target, bool upcast);
    void condition_area(Actor &caster, const detail::SpellDef &spell,
                        const std::vector<Cell> &cells);
    [[nodiscard]] bool teleport_open(const Actor &caster, Cell cell) const;
    [[nodiscard]] static bool in_zone(const Zone &zone, Cell cell);
    // Makes a creature in a Grease or Web zone save; true when it is caught.
    bool spring_zone(const Zone &zone, Actor &creature);
    void spring_zones(Actor &creature, ZoneKind kind);
    void blow(const Zone &line, Actor &creature);
    // Flaming Sphere burns a creature ending its turn within 5 feet of it.
    void burn_beside_spheres(Actor &creature);
    void sphere_burns(const Actor &caster, Actor &creature);
    [[nodiscard]] std::vector<Cell> beam_cells(Cell center) const;
    // A save against the beam's Radiant damage, once per turn.
    void moonbeam_burns(const Zone &beam, Actor &creature);
    // Moonbeam's beam moves onto a creature, burning those it reaches.
    void move_moonbeam(Actor &caster, const Actor &onto);
    void spikes_pierce(Actor &creature);
    // Wild Shape's Beast form, kept in step with the wild_shape effect.
    static void refresh_form(Actor &a);
    void end_wild_shape(Actor &a);
    // Land's Aid: thorns in a 10-foot Sphere and flowers that heal one ally.
    void lands_aid(Actor &druid, Cell center);
    // Heat Metal targets only metal armor a creature wears.
    [[nodiscard]] bool wears_metal(const Actor &creature) const;
    // Heat Metal's 2d8 Fire and Constitution save.
    void heat_metal(const Actor &caster, Actor &creature);
    void potent_cantrip(Actor &target, const detail::SpellDef &spell, detail::DamageDice rolled);
    [[nodiscard]] std::vector<EntityId> sculpted(const Actor &caster, const detail::SpellDef &spell,
            unsigned slot_level, const std::vector<Cell> &cells) const;
    void cast_area();
    void squeeze_ensnared(Actor &a);
    void escape_ensnaring(Actor &a);
    [[nodiscard]] bool mark_can_move(const Actor &caster) const;
    [[nodiscard]] bool hexed_by(const Actor &target, const Actor &caster) const;
    [[nodiscard]] bool hex_can_move(const Actor &caster) const;
    // Sacred Weapon's attack bonus, 0 without it.
    [[nodiscard]] int sacred_weapon_bonus(const Actor &a) const;
    [[nodiscard]] detail::DamageType sacred_damage_type(const Actor &a, const Actor &target) const;
    unsigned next_save_ms(EntityId target) const;
    unsigned next_turn_ms(const Actor &target) const;
    void advance_turn_time();
    void log_save(const Actor &target, const detail::SaveResult &result);
    // `advantage` is a feature's own Advantage, such as a Large creature against
    // Ensnaring Strike; conditions and armor are applied here.
    bool saving_throw_succeeds(const Actor &target, detail::Ability ability, int dc,
                               bool advantage = false);
    detail::MovementGrid movement_grid(const Actor &mover) const;
    // The board with spell zones applied, as movement and the view see it.
    [[nodiscard]] Battlefield zoned_board() const;
    std::vector<Cell> path_to(const Actor &a, Cell destination) const;

    EntityId pending() const
    {
        return reactor_index_ < reactors_.size() ? reactors_[reactor_index_] : 0;
    }

    bool critical_hit(const Actor &a, const Actor &target, int natural, bool spell = false) const
    {
        return natural == 20 || (!spell && def(a).champion && natural == 19) ||
               (helpless(target) && distance(a.source.cell, target.source.cell) <= 5);
    }

    // Unconscious (or asleep) or Paralyzed: attacked with Advantage, critically hit within
    // 5 feet, failing Strength and Dexterity saves.
    static bool helpless(const Actor &target)
    {
        return unconscious(target) || detail::paralyzed(target.effects) ||
               detail::has_effect(target.effects, detail::EffectKind::asleep);
    }

    [[nodiscard]] int magic_weapon_bonus(const Actor &a) const;
    // Shillelagh is on the Club or Quarterstaff the creature holds.
    [[nodiscard]] bool shillelagh(const Actor &a) const;
    // A readied Metamagic option and, for Transmuted Spell, its new type.
    struct ReadyMetamagic
    {
        Metamagic option{};
        std::optional<detail::DamageType> type;
    };
    [[nodiscard]] static std::optional<ReadyMetamagic> readied_metamagic(const Actor &a);
    // The readied option, if it can change this spell and is affordable.
    [[nodiscard]] static bool readies(const Actor &a, Metamagic option,
                                      const detail::SpellDef &spell);
    // Spends the readied option on the spell being cast, if it applies.
    void take_metamagic(Actor &caster, const detail::SpellDef &spell);
    [[nodiscard]] bool casting_with(Metamagic option) const
    {
        return casting_metamagic_ && casting_metamagic_->option == option;
    }
    [[nodiscard]] int spell_range(const Actor &caster, const detail::SpellDef &spell) const;
    [[nodiscard]] detail::DamageType cast_damage_type(detail::DamageType type) const;
    int spell_dice(const Actor &caster, Dice dice, bool critical = false);
    // Careful Spell: up to the Charisma modifier (at least one) of the caster's
    // allies in the area, who then succeed on their saves.
    [[nodiscard]] std::vector<EntityId> careful_allies(const Actor &caster,
            const std::vector<Cell> &cells) const;
    // Metamagic spent on the spell now resolving, and Heightened Spell's target.
    std::optional<ReadyMetamagic> casting_metamagic_;
    EntityId heightened_target_{};
    // 8 + the caster's spellcasting modifier and Proficiency Bonus, +1 during
    // Innate Sorcery.
    [[nodiscard]] int spell_dc(const Actor &caster) const
    {
        return 8 + def(caster).casting +
               (detail::has_effect(caster.effects, detail::EffectKind::innate_sorcery) ? 1 : 0);
    }
    [[nodiscard]] bool strength_attack(const Actor &a, bool ranged) const;
    void attack_recklessly(Actor &a);
    int frenzy_damage(Actor &berserker, bool critical);
    int resized_damage(const Actor &a, bool weapon_hit, int amount);
    int burst_damage(Dice dice, bool critical, int bursts);
    [[nodiscard]] static bool fought_advantage(const detail::SpellDef &spell);
    // Whether `a` is Charmed by `other`, which it then cannot attack or target.
    [[nodiscard]] bool charmed_by(const Actor &a, EntityId other) const;
    void strike_true(Actor &a, Actor &target, bool radiant);
    bool strikes_duplicate(const Actor &attacker, Actor &target);
    // `bursts` is how many extra d8s an 8 may add (Sorcerous Burst).
    bool attack(Actor &a, Actor &target, bool ranged, bool spell = false,
                Dice spell_dice = {1, 10, 0},
                detail::DamageType spell_type = detail::DamageType::fire, int bursts = 0);
    detail::Mastery weapon_mastery(const Actor &, bool ranged) const;
    bool mastery_capacity(const Actor &, const Actor &, bool ranged) const;
    detail::RollModifiers attack_modifiers(const Actor &a, const Actor &target, bool ranged,
                                           bool spell) const;
    Dice weapon_dice(const Actor &a, bool ranged) const;
    detail::DamageDieRule weapon_die_rule(const Actor &a, bool ranged) const;
    void apply_hit(Actor &a, Actor &target, int natural, int bonus, int mode, int amount,
                   bool savage, detail::DamageType type, bool ranged, bool spell = false,
                   bool optional_mastery = true);
    bool sneak_eligible(const Actor &a, const Actor &target, bool ranged, int mode) const;
    bool ally_beside(const Actor &a, const Actor &target) const;
    int roll_advantage_damage(const Actor &a, const Actor &target, int natural);
    [[nodiscard]] int roll_sneak_attack(Actor &a, const Actor &target, int natural);
    [[nodiscard]] int keep_higher_savage_roll(Actor &a, int first, int second);
    void finish_reaction();
    int resolved_damage(Actor &target, detail::DamageType type, int amount);
    // The creature's damage affinities with those its effects grant: Warding
    // Bond resists all damage, Protection from Poison resists Poison.
    [[nodiscard]] std::vector<detail::DamageAffinity> affinities(const Actor &target) const;
    // Sanctuary: whether the warded target stops this attack or harmful spell,
    // after the attacker's Wisdom save. Attacking or casting ends the
    // attacker's own Sanctuary.
    bool sanctuary_stops(Actor &attacker, const Actor &target);
    void end_sanctuary(Actor &a);
    // The living casters bonded to `target` by Warding Bond within 60 feet.
    [[nodiscard]] std::vector<EntityId> bonds_on(const Actor &target) const;
    void damage(Actor &target, int amount, bool critical = false);
    int heal(Actor &target, int amount); // Returns the Hit Points restored.
    void resolve_smite(Actor &a, std::string_view verb);
    int armor_class(const Actor &target) const;
    void begin_concentration(Actor &caster, const detail::SpellDef &spell);
    void end_concentration(Actor &caster);
    void drop_concentration_effects(const Actor &caster);
    unsigned selection_maximum(const PendingSelection &) const;
    void choose_target(const Command &command);
    void cast_on_selection();
    // Bless adds 1d4 to attack rolls and saving throws; Bane subtracts 1d4.
    int blessing_die(const Actor &a);
    void burn_searing_smites(Actor &a);
    void update_outcome();
    void wake_resting_participants();
    void resolve_death_saves_after_victory();
    std::vector<EntityId> initiative_choices_;

    void start_encounter_turns()
    {
        // Recovery checks follow the target's new initiative in a new encounter.
        // Exact timers in a restored encounter are installed after construction.
        for (std::size_t i = 0; i < actors_.size(); ++i)
        {
            for (auto &effect : actors_[i].effects.active)
                if (effect.kind == detail::EffectKind::blindness)
                    effect.save_in_ms = turn_end_ms(i);
            if (actors_[i].hp == 0 && !actors_[i].dead && !actors_[i].stable)
                actors_[i].recovery.death_save_in_ms = i ? turn_end_ms(i - 1) : 0;
        }
        if (outcome_ == Outcome::ongoing && !begin_turn())
            end_turn();
    }

    // Uncanny Metabolism is offered at Initiative only while it would restore
    // something: spent Focus or missing Hit Points.
    bool metabolism_ready(const Actor &a) const
    {
        return def(a).metabolism && a.arcane > 0 &&
               (a.surges < surge_capacity(def(a)) || a.hp < max_hp(a));
    }

    void resolve_initiative(const Command &command)
    {
        if (command.verb == "initiative_swap")
            std::swap(actor(command.actor).initiative, actor(command.target).initiative);
        if (command.verb == "uncanny_metabolism")
        {
            // All Focus Points back, and the Martial Arts die plus the Monk
            // level in Hit Points.
            auto &monk = actor(command.actor);
            --monk.arcane;
            monk.surges = surge_capacity(def(monk));
            log(monk.source.name + " uses Uncanny Metabolism.",
            {"{name} uses Uncanny Metabolism.", {{"name", monk.source.name}}});
            heal(monk, roll(6) + def(monk).level);
        }
        std::erase(initiative_choices_, command.actor);
        std::stable_sort(actors_.begin(), actors_.end(),
                         [](const Actor & a, const Actor & b)
        {
            return a.initiative != b.initiative ? a.initiative > b.initiative
                   : a.source.id < b.source.id;
        });
        if (initiative_choices_.empty())
            start_encounter_turns();
    }

    void validate_initiative() const;
    bool begin_turn();
    void end_turn();
    void progress_movement();
    void restore_movement(std::istream &input);
    void validate_restored_state() const;
    void validate_pending_movement() const;
    void restore_log(std::istream &input);
};

unsigned Session::held_weapon(const Actor &a) const
{
    const auto eligible = [&](const auto & item)
    {
        return item.holder == a.source.id && !item.stowed && detail::weapon(item.definition);
    };
    if (a.selected_weapon)
        for (const auto &item : items_)
            if (item.id == a.selected_weapon && eligible(item))
                return item.id;
    for (const auto &item : items_)
        if (eligible(item))
            return item.id;
    return 0;
}

void Session::activate_light()
{
    if (light_active_)
        return;
    physical_inventory_ = true;
    initialize_items();
    const auto count = items_.size();
    for (unsigned i = 0; i < count; ++i)
        if (items_[i].holder && !items_[i].stowed && items_[i].quantity > 1)
        {
            auto carried = items_[i];
            carried.id = unsigned(items_.size() + 1);
            --carried.quantity;
            carried.stowed = true;
            items_[i].quantity = 1;
            items_.push_back(std::move(carried));
        }
    light_active_ = true;
}

void Session::open_hand(const Actor &monk, Actor &target, std::string_view verb)
{
    // Open Hand Technique on a Flurry hit: Addle stops Opportunity Attacks
    // until the target's next turn; Push (Strength save) shoves it 15 feet;
    // Topple (Dexterity save) knocks it Prone.
    if (verb == "flurry_addle")
    {
        detail::apply_poisoned(target.effects, scope_, monk.source.id, monk.source.name,
                               next_turn_ms(target), detail::EffectKind::addled);
        log(target.source.name + " is addled.", {"{name} is addled.", {{"name", target.source.name}}});
    }
    else if (verb == "flurry_push" &&
             !saving_throw_succeeds(target, detail::Ability::strength, def(monk).focus_dc))
        push_away(monk, target, 3);
    else if (verb == "flurry_topple" && !target.effects.prone &&
             !saving_throw_succeeds(target, detail::Ability::dexterity, def(monk).focus_dc))
    {
        target.effects.prone = true;
        log(target.source.name + " is knocked Prone.",
        {"{name} is knocked Prone.", {{"name", target.source.name}}});
    }
}

void Session::ready_metamagic(Actor &a, std::string_view choice)
{
    std::erase_if(a.effects.active, [](const auto & e)
    {
        return e.kind == detail::EffectKind::metamagic;
    });
    if (choice == "cancel")
        return;
    int value = 0;
    std::string label;
    if (choice.starts_with("transmuted_"))
    {
        const auto type = choice.substr(11);
        value = 10 + int(std::find(transmuted_types.begin(), transmuted_types.end(), type) -
                         transmuted_types.begin());
        label = "Transmuted Spell";
    }
    else
    {
        value = int(std::find(metamagic_ids.begin(), metamagic_ids.end(), choice) -
                    metamagic_ids.begin());
        label = metamagic_labels[std::size_t(value)];
    }
    detail::apply_spell_benefit(a.effects, scope_, a.source.id, a.source.name,
                                detail::EffectKind::metamagic, value);
    log(a.source.name + " readies " + label + ".",
    {"{name} readies {feature}.", {{"name", a.source.name}, {"feature", label, true}}});
}

void Session::use_font_of_magic(Actor &a, std::string_view verb)
{
    const bool second = verb.ends_with("_2");
    if (verb.starts_with("create_slot_"))
    {
        a.bonus = false;
        a.lay_on_hands -= second ? 3 : 2;
        ++(second ? a.slots2 : a.slots);
    }
    else
    {
        --(second ? a.slots2 : a.slots);
        a.lay_on_hands = std::min(def(a).sorcery_points, a.lay_on_hands + (second ? 2 : 1));
    }
    const std::string level = second ? "2" : "1";
    log(a.source.name + (verb.starts_with("create_slot_")
                         ? " creates a level-" + level + " spell slot."
                         : " turns a level-" + level + " spell slot into Sorcery Points."),
    {
        verb.starts_with("create_slot_") ? "{name} creates a level-{level} spell slot."
        : "{name} turns a level-{level} spell slot into Sorcery Points.",
        {{"name", a.source.name}, {"level", level}}
    });
}

void Session::use_focus_movement(Actor &a, std::string_view verb)
{
    // Disengage with Patient Defense, Dash with Step of the Wind; a Focus Point
    // adds Dodge or Disengage.
    const bool focus = verb.ends_with("_focus");
    const bool patient = verb.starts_with("patient_defense");
    a.bonus = false;
    if (focus)
        --a.surges;
    if (patient || focus)
        a.disengaged = true;
    if (patient && focus)
        a.dodge = true;
    if (!patient)
    {
        a.movement += def(a).speed;
        ++a.dashes;
    }
    const std::string name = patient ? "Patient Defense" : "Step of the Wind";
    log(a.source.name + " uses " + name + ".",
    {"{name} uses {feature}.", {{"name", a.source.name}, {"feature", name, true}}});
}

Actor Session::unarmed_actor(const Actor &a) const
{
    // A Monk's Unarmed Strike: the Martial Arts die plus its modifier, with no
    // weapon's mastery, grip or feat riders.
    auto result = a;
    auto &d = result.definition;
    d.melee = {1, 6, d.martial_modifier};
    d.melee_bonus = 2 + d.martial_modifier;
    d.melee_ability = d.martial_modifier;
    d.melee_type = detail::DamageType::bludgeoning;
    d.reach = 5;
    d.versatile_sides = 0;
    d.masteries.clear();
    d.savage = d.great_weapon_fighting = false;
    return result;
}

Actor Session::item_actor(const Actor &a, unsigned token) const
{
    const auto &item = items_.at(token - 1);
    if (item.holder != a.source.id || item.stowed || !detail::weapon(item.definition))
        throw std::runtime_error("Weapon is not held");
    auto result = a;
    result.selected_weapon = token;
    result.definition = equipped_definition(result, items_);
    return result;
}

bool Session::has_nick(const Actor &a) const
{
    return std::any_of(def(a).masteries.begin(), def(a).masteries.end(),
                       [](const auto & key)
    {
        const auto *w = detail::weapon(key);
        return w && w->mastery == detail::Mastery::nick;
    });
}

bool Session::nick_weapon(const Actor &a, unsigned token) const
{
    if (!token || token > items_.size())
        return false;
    const auto &key = items_[token - 1].definition;
    const auto *w = detail::weapon(key);
    return w && w->mastery == detail::Mastery::nick &&
           std::find(def(a).masteries.begin(), def(a).masteries.end(), key) !=
           def(a).masteries.end();
}

bool Session::light_eligible(const Actor &a, unsigned token) const
{
    if (!token || token > items_.size())
        return false;
    const auto *weapon = detail::weapon(items_[token - 1].definition);
    return weapon && weapon->light &&
           std::any_of(a.light_origins.begin(), a.light_origins.end(),
                       [&](auto prior)
    {
        return prior != token;
    });
}

void Session::qualify_light(Actor &a, unsigned token)
{
    if (!token)
        return;
    const auto *weapon = detail::weapon(items_.at(token - 1).definition);
    if (nick_active_ && weapon && weapon->light)
        a.nick_origin = token;
    if (weapon && weapon->light &&
            std::find(a.light_origins.begin(), a.light_origins.end(), token) == a.light_origins.end())
        a.light_origins.push_back(token);
}

bool Session::has_weapon_reaction(const Actor &a, Cell from, Cell to) const
{
    const auto reaches = [&](int reach)
    {
        return distance(a.source.cell, from) <= reach && distance(a.source.cell, to) > reach;
    };
    if (reaches(def(a).reach))
        return true;
    for (const auto &item : items_)
        if (item.holder == a.source.id && !item.stowed)
            if (const auto *w = detail::weapon(item.definition);
                    w && reaches(w->ranged ? 5 : w->reach))
                return true;
    return false;
}

bool Session::weapon_reaction(const Actor &a, const Definition &weapon) const
{
    if (!pending() || path_index_ >= path_.size())
        return false;
    return distance(a.source.cell, actors_[turn_].source.cell) <= weapon.reach &&
           distance(a.source.cell, path_[path_index_]) > weapon.reach;
}

void Session::validate_light() const
{
    if (nick_active_ && (!light_active_ || std::none_of(actors_.begin(), actors_.end(),
            [&](const auto & a)
{
    return has_nick(a);
    })))
    throw std::runtime_error("Nick requires an actual mastery entitlement");
    if (light_active_ && (!physical_inventory_ || !items_active_))
        throw std::runtime_error("Light attacks require physical weapon identities");
    for (const auto &a : actors_)
    {
        if ((!nick_active_ && a.nick_origin) ||
                (a.nick_origin && std::find(a.light_origins.begin(), a.light_origins.end(),
                                            a.nick_origin) == a.light_origins.end()))
            throw std::runtime_error("Invalid Nick Attack action origin");
        if (a.light_extra > 2 || (!nick_active_ && a.light_extra) ||
                (a.light_extra &&
                 (a.light_origins.empty() || a.source.id != actors_[turn_].source.id)) ||
                (a.light_extra == 1 && a.bonus) || (a.light_extra == 2 && !has_nick(a)))
            throw std::runtime_error("Invalid shared Light/Nick expenditure");
        if (!light_active_ &&
                (a.selected_weapon || !a.light_origins.empty() || a.definition.two_weapon_fighting))
            throw std::runtime_error("Missing Light attack checkpoint state");
        if (a.selected_weapon &&
                (a.selected_weapon > items_.size() || held_weapon(a) != a.selected_weapon))
            throw std::runtime_error("Invalid selected weapon");
        if (!a.light_origins.empty() && (a.source.id != actors_[turn_].source.id ||
                                         (a.actions.normal && (!a.surge_used || a.actions.surge))))
            throw std::runtime_error("Light attack lacks an Attack action");
        if (a.light_origins.size() > 1 && (!a.surge_used || a.actions.normal || a.actions.surge))
            throw std::runtime_error("Too many qualifying Attack actions");
        std::set<unsigned> seen;
        for (auto id : a.light_origins)
        {
            if (!id || id > items_.size() || !seen.insert(id).second)
                throw std::runtime_error("Invalid qualifying weapon identity");
            const auto *w = detail::weapon(items_[id - 1].definition);
            if (!w || !w->light)
                throw std::runtime_error("Qualifying weapon is not Light");
        }
    }
}

void Session::initialize_items()
{
    if (items_active_)
        return;
    std::vector<const Actor *> ordered;
    for (const auto &a : actors_)
        ordered.push_back(&a);
    std::sort(ordered.begin(), ordered.end(),
              [](auto a, auto b)
    {
        return a->source.id < b->source.id;
    });
    for (const auto *a : ordered)
    {
        std::set<std::uint64_t> ids;
        for (const auto &source : a->source.inventory)
            if (!source.inventory_id || !source.quantity || !ids.insert(source.inventory_id).second)
                throw std::runtime_error("Invalid carried item source");
        for (unsigned i = 0; i < a->definition.equipment_keys.size(); ++i)
        {
            const auto &key = a->definition.equipment_keys[i];
            if (key != "shield" && !detail::weapon(key))
                continue;
            HeldItemView item
            {
                unsigned(items_.size() + 1),
                a->source.id,
                a->source.id,
                i,
                key,
                {key == "shield" ? "Shield" : std::string(detail::weapon(key)->label), {}}};
            if (physical_inventory_)
                for (const auto &source : a->source.inventory)
                    if (source.equipment_index == static_cast<int>(i))
                    {
                        if (source.definition != key || item.inventory_id)
                            throw std::runtime_error("Carried item disagrees with equipped source");
                        item.inventory_id = source.inventory_id;
                        item.quantity = source.quantity;
                    }
            items_.push_back(std::move(item));
        }
        if (physical_inventory_)
            for (const auto &source : a->source.inventory)
                if (source.equipment_index < 0 && detail::weapon(source.definition))
                    items_.push_back({unsigned(items_.size() + 1),
                                      a->source.id,
                                      a->source.id,
                                      0,
                                      source.definition,
                {std::string(detail::weapon(source.definition)->label), {}},
        source.inventory_id,
        source.quantity,
        true});
    }
    items_active_ = true;
}

Definition Session::equipped_definition(const Actor &a,
                                        const std::vector<HeldItemView> &items) const
{
    if (a.source.character_profile.empty())
        throw std::runtime_error("Equipment exchange requires a character equipment profile");
    const auto original = character_definition(a.source.character_profile);
    std::vector<std::string> keys;
    for (const auto &key : original.equipment_keys)
        if (key != "shield" && !detail::weapon(key))
            keys.push_back(key);
    if (a.selected_weapon)
        for (const auto &item : items)
            if (item.id == a.selected_weapon && item.holder == a.source.id && !item.stowed)
                keys.push_back(item.definition);
    for (const auto &item : items)
        if (item.holder == a.source.id && !item.stowed && item.id != a.selected_weapon)
            keys.push_back(item.definition);
    return character_definition(a.source.character_profile, std::span<const std::string>(keys));
}

Actor Session::thrown_actor(const Actor &a, std::string_view weapon) const
{
    auto result = a;
    std::vector<std::string> keys;
    for (const auto &key : a.definition.equipment_keys)
        if (!detail::weapon(key))
            keys.push_back(key);
    keys.emplace_back(weapon);
    result.definition =
        character_definition(a.source.character_profile, std::span<const std::string>(keys));
    return result;
}

// Thrown weapons work like ammunition: the weapon stays where it was, held or
// carried, so throwing never changes what the thrower holds.
void Session::throw_weapon(Actor &a, Actor &target, unsigned token, bool light)
{
    auto attacker = thrown_actor(a, items_.at(token - 1).definition);
    attacker.light_damage = light;
    attack(attacker, target, true, false);
    a.aim_ready = attacker.aim_ready;
    if (mastery_)
        mastery_->thrown_item = token;
}

Battlefield Session::zoned_board() const
{
    // Entangle's plants, Grease and Web make their open squares Difficult
    // Terrain. Adaptation: so does Gust of Wind's line, whose wind only slows
    // movement toward its caster.
    auto board = board_;
    for (const auto &zone : zones_)
        if (zone.kind == ZoneKind::plants || zone.kind == ZoneKind::grease ||
                zone.kind == ZoneKind::web || zone.kind == ZoneKind::gust ||
                zone.kind == ZoneKind::spikes)
            for (const auto cell : zone.cells)
                if (board.at(cell) == 0)
                    board.terrain[std::size_t(cell.y * board.width + cell.x)] = 2;
    return board;
}

Cell Session::spectral_cell(const Actor &target, Cell from) const
{
    // The open, unoccupied square beside the target nearest `from`.
    std::optional<Cell> best;
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx)
        {
            const Cell cell{target.source.cell.x + dx, target.source.cell.y + dy};
            if ((!dx && !dy) || !board_.contains(cell) || board_.at(cell) == 1 ||
                    std::any_of(actors_.begin(), actors_.end(), [&](const auto & other)
            {
                return !other.dead && other.source.cell == cell;
            }))
            continue;
            if (!best || distance(cell, from) < distance(*best, from))
                best = cell;
        }
    return best.value_or(target.source.cell);
}

const Session::Zone *Session::spiritual_weapon(const Actor &caster) const
{
    const auto found = std::find_if(zones_.begin(), zones_.end(), [&](const auto & zone)
    {
        return zone.kind == ZoneKind::spiritual_weapon && zone.caster == caster.source.id;
    });
    return found == zones_.end() ? nullptr : &*found;
}

bool Session::silenced(Cell cell) const
{
    return std::any_of(zones_.begin(), zones_.end(), [&](const auto & zone)
    {
        return zone.kind == ZoneKind::silence &&
               std::find(zone.cells.begin(), zone.cells.end(), cell) != zone.cells.end();
    });
}

bool Session::obscured(const Actor &viewer, Cell cell) const
{
    // Fog Cloud hides from everyone; magical Darkness from all but Devil's Sight.
    return std::any_of(zones_.begin(), zones_.end(), [&](const auto & zone)
    {
        return (zone.kind == ZoneKind::fog ||
                (zone.kind == ZoneKind::darkness && !def(viewer).devils_sight)) &&
               std::find(zone.cells.begin(), zone.cells.end(), cell) != zone.cells.end();
    });
}

detail::MovementGrid Session::movement_grid(const Actor &mover) const
{
    std::vector<detail::Occupant> occupants;
    for (const auto &other : actors_)
    {
        // Unconscious actors still occupy space; corpses do not. The mover's
        // current cell is the path origin, not an obstacle.
        if (!other.dead && other.source.id != mover.source.id)
            occupants.push_back(
        {other.source.cell, other.source.side != mover.source.side, unconscious(other)});
    }
    return {zoned_board(), mover.source.cell, occupants, mover.effects.prone};
}

std::vector<Cell> Session::path_to(const Actor &actor, Cell destination) const
{
    return movement_grid(actor).reachable(movement_left(actor)).path_to(destination);
}

Snapshot Session::snapshot() const
{
    Snapshot s;
    s.identity = content_->identity;
    s.revision = revision_;
    s.round = round_;
    s.outcome = outcome_;
    s.initiative_choices = initiative_choices_;
    s.elapsed_milliseconds = elapsed_ms_;
    s.held_items = items_;
    s.physical_inventory = physical_inventory_;
    s.actor = champion_move_ ? champion_move_->actor
              : pending()      ? pending()
              : actors_[turn_].source.id;
    s.reaction_pending = !champion_move_ && pending() != 0;
    if (reaction_prompt_)
    {
        s.actor = reaction_prompt_->question.target;
        s.reaction_pending = true;
    }
    s.battlefield = zoned_board();
    for (const auto &zone : zones_)
        if (zone.kind == ZoneKind::fog || zone.kind == ZoneKind::darkness)
            s.obscured.insert(s.obscured.end(), zone.cells.begin(), zone.cells.end());
        else if (zone.kind == ZoneKind::silence)
            s.silenced.insert(s.silenced.end(), zone.cells.begin(), zone.cells.end());
        else if (zone.kind == ZoneKind::spiritual_weapon)
            s.spiritual_weapons.push_back(zone.cells.front());
        else if (zone.kind == ZoneKind::flaming_sphere)
            s.flaming_spheres.push_back(zone.cells.front());
        else if (zone.kind == ZoneKind::moonbeam)
            s.moonbeams.insert(s.moonbeams.end(), zone.cells.begin(), zone.cells.end());
    s.log = log_;
    s.log_messages = log_messages_;
    if (!initiative_choices_.empty())
        s.actor = initiative_choices_.front();
    if (graze_)
    {
        const auto &g = *graze_;
        const auto &a = actor(g.actor);
        const auto &t = actor(g.target);
        const int amount = graze_damage(a, t);
        s.optional_effect_choice = OptionalEffectChoice
        {
            g.actor,
            g.target,
            {"Graze", {}},
            {
                "Target: {target}\nGraze damage: {damage}\n\nUse Graze after this miss, or skip it.\nThe attack's Action or Reaction is already spent.",
                {{"target", t.source.name}, {"damage", std::to_string(amount)}}
            }};
    }
    if (selection_)
    {
        s.actor = selection_->caster;
        s.spell_targeting = SpellTargeting{selection_->caster, selection_->verb,
                                           selection_->chosen, selection_maximum(*selection_)};
    }
    if (area_)
    {
        s.actor = area_->caster;
        s.area_targeting = AreaTargeting{area_->caster, area_->verb, area_->center,
                                         area_cells(*detail::find_spell(area_->verb), area_->verb,
                                                    actor(area_->caster).source.cell, area_->center)};
    }
    if (!champion_move_ && effect_waiting())
    {
        if (mastery_ && mastery_->targeting)
        {
            const auto &m = *mastery_;
            s.actor = m.actor;
            s.effect_targeting = EffectTargeting
            {
                m.actor, m.kind == detail::Mastery::cleave ? "effect_attack" : "effect_push",
                m.kind == detail::Mastery::cleave
                ? Message{"Choose a Cleave target, or skip.", {}}
:
                Message{"Choose a Push destination, or skip.", {}},
                m.kind == detail::Mastery::push};
        }
        else
        {
            s.optional_effect_choice = effect_choices();
            s.actor = s.optional_effect_choice->actor;
            if (s.optional_effect_choice->options.size() > 1 && actors_[turn_].source.side == 0)
                s.actor = actors_[turn_].source.id;
        }
    }
    if (champion_move_)
        s.free_movement = FreeMovement{champion_move_->actor, champion_move_->remaining};
    if (check_choice_)
    {
        const auto &c = *check_choice_;
        s.ability_check_choice = AbilityCheckChoice{c.actor,
                                                    c.target,
                                                    c.natural,
                                                    def(actor(c.actor)).medicine,
                                                    c.natural + def(actor(c.actor)).medicine,
                                                    10,
                                                    actor(c.actor).winds};
    }
    if (temporary_offer_)
        s.temporary_hp_offer = TemporaryHpOffer{actors_[turn_].source.id,
                                                actors_[turn_].temporary_hp, *temporary_offer_};
    for (const auto &a : actors_)
    {
        std::string status = a.dead      ? "Dead"
                             : a.hp == 0 ? (a.stable ? "Stable, unconscious" : "Unconscious")
                             : a.effects.prone    ? "Prone"
                             : a.dodge            ? "Dodging"
                             : "Ready";
        if (def(a).slots)
            status += " | slots " + std::to_string(a.slots);
        if (def(a).slots2)
            status += " | L2 slots " + std::to_string(a.slots2);
        if (def(a).winds)
            status += " | Second Wind " + std::to_string(a.winds);
        s.combatants.push_back(
        {
            a.source.id, a.source.name, a.source.definition, a.source.side, a.source.cell, a.hp,
            max_hp(a), armor_class(a), a.initiative,
            champion_move_ && champion_move_->actor == a.source.id ? champion_move_->remaining
            : movement_left(a),
            a.actions.available() && conscious(a), a.bonus && conscious(a),
            a.reaction && conscious(a), conscious(a), a.dead, a.facing_left, status, vitals(a)});
        const auto display = combat_display(a.source.definition);
        auto &view = s.combatants.back();
        view.temporary_hp = a.temporary_hp;
        view.prone = a.effects.prone;
        if (physical_inventory_)
            for (const auto &item : items_)
                if (item.holder == a.source.id)
                    if (const auto *w = detail::weapon(item.definition); w && w->thrown)
                    {
                        const auto offered = legal_commands();
                        view.thrown_weapons.push_back({item.id, item.label,
                                                       std::any_of(offered.begin(), offered.end(),
                                                               [&](const auto & c)
                        {
                            return c.verb == "throw" &&
                                   c.item == item.id;
                        })});
                    }
        view.selected_weapon = held_weapon(a);
        view.nick_mastery = has_nick(a);
        const auto offered = legal_commands();
        unsigned hand_index = 0;
        for (const auto &item : items_)
            if (item.holder == a.source.id)
                if (const auto *w = detail::weapon(item.definition))
                {
                    const std::string hand = item.stowed    ? "Carried"
                                             : hand_index++ ? "Other hand"
                                             : "Main hand";
                    if (!item.stowed)
                        view.weapons.push_back(
                    {
                        item.id,
                        {
                            "{hand} — {weapon}",
                            {{"hand", hand, true}, {"weapon", item.label.source, true}}
                        },
                        item.id == view.selected_weapon ||
                        std::any_of(offered.begin(), offered.end(),
                                    [&](const auto & c)
                        {
                            return c.actor == a.source.id &&
                            c.verb == "weapon_select" &&
                            c.item == item.id;
                        })});
                    if (nick_weapon(a, item.id))
                    {
                        const auto option = [&](const char *verb, const char *label)
                        {
                            view.nick_attacks.push_back(
                            {
                                item.id,
                                verb,
                                {
                                    label,
                                    {{"weapon", item.label.source, true}, {"hand", hand, true}}
                                },
                                std::any_of(offered.begin(), offered.end(),
                                            [&](const auto & c)
                                {
                                    return c.actor == a.source.id && c.verb == verb &&
                                    c.item == item.id;
                                })});
                        };
                        if (!item.stowed && !w->ranged)
                            option("nick_melee", "Nick attack — {weapon} ({hand})");
                        if (!item.stowed && w->ranged)
                            option("nick_ranged", "Nick ranged attack — {weapon} ({hand})");
                        if (w->thrown)
                            option("nick_throw", "Nick throw — {weapon} ({hand})");
                    }
                    if (w->light && !a.light_origins.empty())
                    {
                        const auto option = [&](const char *verb, const char *label)
                        {
                            view.light_attacks.push_back(
                            {
                                item.id,
                                verb,
                                {
                                    label,
                                    {{"weapon", item.label.source, true}, {"hand", hand, true}}
                                },
                                std::any_of(offered.begin(), offered.end(),
                                            [&](const auto & c)
                                {
                                    return c.actor == a.source.id && c.verb == verb &&
                                    c.item == item.id;
                                })});
                        };
                        if (!item.stowed && !w->ranged)
                            option("light_melee", "Light attack — {weapon} ({hand})");
                        if (!item.stowed && w->ranged)
                            option("light_ranged", "Light ranged attack — {weapon} ({hand})");
                        if (w->thrown)
                            option("light_throw", "Light throw — {weapon} ({hand})");
                    }
                }
        if (a.effects.prone)
            view.conditions.push_back({"Prone", {}});
        if (def(a).cunning)
            view.bonus_actions = {"cunning_dash", "cunning_disengage"};
        if (def(a).sneak_level >= 3)
            view.bonus_actions.push_back("steady_aim");
        if (def(a).lay_on_hands)
            view.bonus_actions.push_back("lay_on_hands");
        if (def(a).rages)
            view.bonus_actions.insert(view.bonus_actions.end(), {"rage", "extend_rage"});
        if (def(a).martial_arts)
            view.bonus_actions.push_back("martial_arts");
        if (def(a).innate_sorcery)
            view.bonus_actions.push_back("innate_sorcery");
        if (def(a).bardic_inspiration)
            view.bonus_actions.push_back("bardic_inspiration");
        if (def(a).sorcery_points)
            view.bonus_actions.insert(view.bonus_actions.end(),
        {"create_slot_1", "create_slot_2", "convert_slot_1", "convert_slot_2"});
        for (const auto &known : def(a).metamagic)
            if (known == "transmuted")
                for (const auto type : transmuted_types)
                    view.bonus_actions.push_back("metamagic_transmuted_" + std::string(type));
            else
                view.bonus_actions.push_back("metamagic_" + known);
        if (!def(a).metamagic.empty())
            view.bonus_actions.push_back("metamagic_cancel");
        if (def(a).focus)
            view.bonus_actions.insert(view.bonus_actions.end(),
        {
            "flurry_of_blows", "flurry_addle", "flurry_push", "flurry_topple", "patient_defense",
            "patient_defense_focus", "step_of_the_wind", "step_of_the_wind_focus"
        });
        if (detail::knows_spell(def(a).spells, "divine_smite") && def(a).free_smite)
            view.bonus_actions.push_back("divine_smite_free");
        for (const auto *smite :
                {"divine_smite", "searing_smite", "ensnaring_strike"
                })
            if (detail::knows_spell(def(a).spells, smite))
                view.bonus_actions.push_back(smite);
        for (const auto &descriptor : resource_descriptors)
            if (descriptor.combat_view && def(a).*descriptor.capacity)
                view.resources.push_back(resource_pool(descriptor, a, def(a)));
        // Explicit display order: this list feeds the combat cantrip dropdown,
        // so its order is observable in the UI and is not the table's order.
        for (const auto *id :
                {"fire_bolt", "chill_touch", "shocking_grasp", "eldritch_blast",
                 "poison_spray", "ray_of_frost", "sacred_flame"
                })
            if (detail::knows_spell(def(a).known_cantrips, id))
                view.known_cantrips.push_back(id);

        if (def(a).hit_die)
        {
            view.hp_messages.push_back(
            {
                "Maximum HP includes level {level}, d{die} Hit Die and Constitution modifier {modifier}.",
                {   {"level", std::to_string(def(a).level)},
                    {"die", std::to_string(def(a).hit_die)},
                    {"modifier", std::to_string(def(a).constitution)}
                }});
            if (def(a).dwarf)
                view.hp_messages.push_back({"Dwarven Toughness: +{hp} maximum HP.",
                {{"hp", std::to_string(def(a).level)}}});
        }
        if (display.type)
            view.type_name = display.type;
        if (display.melee)
            view.melee_weapon = display.melee;
        if (display.ranged)
            view.ranged_weapon = display.ranged;
        view.ranged_attack_available = def(a).range > 0;
        auto &messages = s.combatants.back().status_messages;
        messages.push_back({a.dead      ? "Dead"
                            : a.hp == 0 ? (a.stable ? "Stable, unconscious" : "Unconscious")
                            : a.effects.prone    ? "Prone"
                            : a.dodge            ? "Dodging"
                            : "Ready",
                            {}});
        if (def(a).slots)
            messages.push_back({"Spell slots: {count}", {{"count", std::to_string(a.slots)}}});
        if (def(a).slots2)
            messages.push_back({"L2 slots: {count}", {{"count", std::to_string(a.slots2)}}});
        if (a.actions.surge)
            messages.push_back({"Action Surge action ready (no Magic).", {}});
        if (def(a).winds)
            messages.push_back({"Second Wind: {count}", {{"count", std::to_string(a.winds)}}});
        for (const auto &effect : a.effects.active)
            if (effect.kind == detail::EffectKind::chill_touch)
            {
                Message message{"Chill Touch ({source}): cannot regain HP.",
                    {{"source", effect.source_name}}};
                messages.push_back(message);
                view.conditions.push_back(std::move(message));
            }
        for (const auto &effect : a.effects.active)
            if (effect.kind == detail::EffectKind::sap || effect.kind == detail::EffectKind::vex ||
                    effect.kind == detail::EffectKind::slow)
            {
                Message message
                {
                    effect.kind == detail::EffectKind::sap
                    ? "Sap ({source}): next attack roll has Disadvantage."
                    : effect.kind == detail::EffectKind::slow
                    ? "Slow ({source}): Speed reduced by 10 feet."
                    : "Vex ({source}): source's next attack against this creature has Advantage.",
                    {{"source", effect.source_name}}};
                messages.push_back(message);
                view.conditions.push_back(std::move(message));
            }
        if (detail::opportunity_blocked(a.effects))
        {
            messages.push_back({"Shocking Grasp: cannot make Opportunity Attacks.", {}});
            view.conditions.push_back({"Shocking Grasp: cannot make Opportunity Attacks.", {}});
        }
        if (detail::frosted(a.effects))
        {
            messages.push_back({"Ray of Frost: Speed reduced by 10 feet.", {}});
            view.conditions.push_back({"Ray of Frost: Speed reduced by 10 feet.", {}});
        }
        if (detail::has_effect(a.effects, detail::EffectKind::guiding_bolt))
        {
            messages.push_back({"Guiding Bolt: the next attack roll against it has Advantage.", {}});
            view.conditions.push_back({"Guiding Bolt: the next attack roll against it has Advantage.", {}});
        }
        if (detail::has_effect(a.effects, detail::EffectKind::bane))
        {
            messages.push_back({"Bane: -1d4 to attack rolls and saving throws.", {}});
            view.conditions.push_back({"Bane: -1d4 to attack rolls and saving throws.", {}});
        }
        for (const auto &[kind, label] :
                {
                    std::pair{detail::EffectKind::drowsy, "Incapacitated (Sleep)"},
                    std::pair{detail::EffectKind::asleep, "Unconscious (Sleep)"},
                    std::pair{detail::EffectKind::laughing, "Prone and Incapacitated (laughing)"},
                    std::pair{detail::EffectKind::enfeebled, "Enfeebled (Ray of Enfeeblement)"},
                    std::pair{detail::EffectKind::acid_arrow, "Burning acid (Acid Arrow)"},
                    std::pair{detail::EffectKind::invisible, "Invisible"},
                    std::pair{detail::EffectKind::enlarged, "Enlarged"},
                    std::pair{detail::EffectKind::reduced, "Reduced"},
                    std::pair{detail::EffectKind::charmed, "Charmed"},
                    std::pair{detail::EffectKind::raging, "Raging"},
                    std::pair{detail::EffectKind::reckless, "Reckless"},
                    std::pair{detail::EffectKind::addled, "Addled"},
                    std::pair{detail::EffectKind::innate_sorcery, "Innate Sorcery"},
                    std::pair{detail::EffectKind::metamagic, "Metamagic readied"},
                    std::pair{detail::EffectKind::hex, "Hexed"},
                    std::pair{detail::EffectKind::outlined, "Outlined (Faerie Fire)"},
                    std::pair{detail::EffectKind::lit, "Lit (Starry Wisp)"},
                    std::pair{detail::EffectKind::inspired, "Inspired"},
                    std::pair{detail::EffectKind::shillelagh, "Shillelagh"},
                    std::pair{detail::EffectKind::produce_flame, "Produce Flame"},
                    std::pair{detail::EffectKind::barkskin, "Barkskin"},
                    std::pair{detail::EffectKind::flame_blade, "Flame Blade"},
                    std::pair{detail::EffectKind::heated, "Heated (Heat Metal)"},
                    std::pair{detail::EffectKind::scorched, "Scorched (Heat Metal)"}
                })
            if (detail::has_effect(a.effects, kind))
            {
                messages.push_back({label, {}});
                s.combatants.back().status += std::string(" | ") + label;
                s.combatants.back().conditions.push_back({label, {}});
            }
        if (a.concentration.active())
        {
            messages.push_back({"Concentrating", {}});
            s.combatants.back().status += " | Concentrating";
            s.combatants.back().conditions.push_back({"Concentrating", {}});
        }
        for (const auto &effect : a.effects.active)
            if (effect.kind == detail::EffectKind::wild_shape)
            {
                const auto &form = detail::beast_forms.at(std::size_t(effect.dc - 1));
                s.combatants.back().form = std::string(form.key);
                const Message shape{"Wild Shape ({form})", {{"form", std::string(form.label), true}}};
                messages.push_back(shape);
                s.combatants.back().status += " | Wild Shape (" + std::string(form.label) + ")";
                s.combatants.back().conditions.push_back(shape);
            }
        for (const auto &effect : a.effects.active)
            if (effect.kind == detail::EffectKind::mirror_image)
            {
                const Message duplicates{"Mirror Image ({count} duplicates)",
                    {{"count", std::to_string(effect.dc)}}};
                messages.push_back(duplicates);
                s.combatants.back().status += " | Mirror Image (" + std::to_string(effect.dc) +
                                              " duplicates)";
                s.combatants.back().conditions.push_back(duplicates);
            }
        if (detail::has_effect(a.effects, detail::EffectKind::poisoned))
        {
            messages.push_back({"Poisoned", {}});
            s.combatants.back().status += " | Poisoned";
            s.combatants.back().conditions.push_back({"Poisoned", {}});
        }
        if (detail::paralyzed(a.effects))
        {
            messages.push_back({"Paralyzed", {}});
            s.combatants.back().status += " | Paralyzed";
            s.combatants.back().conditions.push_back({"Paralyzed", {}});
        }
        if (detail::restrained(a.effects))
        {
            messages.push_back({"Restrained", {}});
            s.combatants.back().status += " | Restrained";
            s.combatants.back().conditions.push_back({"Restrained", {}});
        }
        if (detail::blinded(a.effects))
        {
            messages.push_back({"Blinded", {}});
            s.combatants.back().status += " | Blinded";
            s.combatants.back().conditions.push_back({"Blinded", {}});
        }
    }
    return s;
}

void Session::apply_rider(const detail::SpellDef &spell, std::string_view verb, Actor &a,
                          Actor &target, int dc)
{
    // Each rider keeps its own duration rule and its own log line; the wording
    // is unchanged so the message catalogue does not move.
    switch (spell.rider)
    {
    case detail::Rider::none:
    case detail::Rider::entangle:
    case detail::Rider::fog_cloud:
    case detail::Rider::silence:
    case detail::Rider::thunderwave:
        return; // Area spells resolve through cast_area().
    case detail::Rider::spiritual_weapon:
    case detail::Rider::ice_knife:
        return; // Its force is placed, or its shard bursts, with the attack.
    case detail::Rider::ray_of_sickness:
    {
        // Poisoned until the end of the caster's next turn.
        const auto index = static_cast<std::size_t>(&a - actors_.data());
        const unsigned slot = turn_end_ms(index) - (index ? turn_end_ms(index - 1) : 0);
        detail::apply_poisoned(target.effects, scope_, a.source.id, a.source.name,
                               next_turn_ms(a) + slot);
        log(target.source.name + " is Poisoned.",
        {"{name} is Poisoned.", {{"name", target.source.name}}});
        return;
    }
    case detail::Rider::resistance:
    {
        const auto type = *detail::resistance_type(verb);
        detail::apply_spell_benefit(target.effects, scope_, a.source.id, a.source.name,
                                    detail::EffectKind::resistance, int(type));
        const auto name = std::string(detail::damage_name(type));
        log(target.source.name + " gains Resistance against " + name + " damage.",
        {
            "{name} gains Resistance against {type} damage.",
            {{"name", target.source.name}, {"type", name, true}}
        });
        return;
    }
    case detail::Rider::sanctuary:
        detail::apply_sanctuary(target.effects, scope_, a.source.id, a.source.name, dc);
        log(target.source.name + " gains Sanctuary.",
        {"{name} gains {spell}.", {{"name", target.source.name}, {"spell", "Sanctuary", true}}});
        return;
    case detail::Rider::warding_bond:
    {
        // A new bond ends any other on either creature.
        for (auto &other : actors_)
            std::erase_if(other.effects.active, [&](const auto & e)
        {
            return e.kind == detail::EffectKind::warding_bond &&
                   ((e.source_scope == scope_ && e.source_actor == a.source.id) ||
                    other.source.id == target.source.id);
        });
        detail::apply_spell_benefit(target.effects, scope_, a.source.id, a.source.name,
                                    detail::EffectKind::warding_bond, 0);
        log(target.source.name + " gains Warding Bond.",
        {"{name} gains {spell}.", {{"name", target.source.name}, {"spell", "Warding Bond", true}}});
        return;
    }
    case detail::Rider::protection_from_poison:
        std::erase_if(target.effects.active, [](const auto & e)
        {
            return e.kind == detail::EffectKind::poisoned;
        });
        detail::apply_spell_benefit(target.effects, scope_, a.source.id, a.source.name,
                                    detail::EffectKind::protection_from_poison, 0);
        log(target.source.name + " gains Protection from Poison.",
        {
            "{name} gains {spell}.",
            {{"name", target.source.name}, {"spell", "Protection from Poison", true}}
        });
        return;
    case detail::Rider::mage_armor:
        detail::apply_spell_benefit(target.effects, scope_, a.source.id, a.source.name,
                                    detail::EffectKind::mage_armor, 0);
        log(target.source.name + " gains Mage Armor.",
        {"{name} gains {spell}.", {{"name", target.source.name}, {"spell", "Mage Armor", true}}});
        return;
    case detail::Rider::false_life:
    {
        // 2d4 + 4, and 5 more from a level-two slot. Temporary Hit Points do
        // not stack: the creature keeps the better or is asked, as for Adrenaline Rush.
        TemporaryHitPoints offered{dice({2, 4, 4}) + (verb.ends_with("_2") ? 5 : 0),
                                   "spell:false_life"};
        log(target.source.name + " gains False Life.",
        {"{name} gains {spell}.", {{"name", target.source.name}, {"spell", "False Life", true}}});
        if (target.temporary_hp.amount)
            temporary_offer_ = std::move(offered);
        else
            detail::grant_temporary_hp(target, offered, TemporaryHpChoice::use_new);
        return;
    }
    case detail::Rider::expeditious_retreat:
        detail::apply_spell_benefit(target.effects, scope_, a.source.id, a.source.name,
                                    detail::EffectKind::expeditious_retreat, 0);
        log(target.source.name + " gains Expeditious Retreat.",
        {
            "{name} gains {spell}.",
            {{"name", target.source.name}, {"spell", "Expeditious Retreat", true}}
        });
        target.movement += def(target).speed;
        ++target.dashes;
        return;
    case detail::Rider::hideous_laughter:
        detail::apply_repeating_condition(target.effects, detail::EffectKind::laughing, scope_,
                                          a.source.id, a.source.name, dc,
                                          next_save_ms(target.source.id));
        target.effects.prone = true;
        log(target.source.name + " falls Prone, laughing.",
        {"{name} falls Prone, laughing.", {{"name", target.source.name}}});
        return;
    case detail::Rider::sleep:
    case detail::Rider::color_spray:
    case detail::Rider::grease:
    case detail::Rider::web:
    case detail::Rider::misty_step:
        return; // Aimed areas, resolved by cast_area().
    case detail::Rider::invisibility:
        detail::apply_spell_benefit(target.effects, scope_, a.source.id, a.source.name,
                                    detail::EffectKind::invisible, 0);
        log(target.source.name + " turns Invisible.",
        {"{name} turns Invisible.", {{"name", target.source.name}}});
        return;
    case detail::Rider::see_invisibility:
        detail::apply_spell_benefit(target.effects, scope_, a.source.id, a.source.name,
                                    detail::EffectKind::see_invisibility, 0);
        log(target.source.name + " gains See Invisibility.",
        {"{name} gains {spell}.", {{"name", target.source.name}, {"spell", "See Invisibility", true}}});
        return;
    case detail::Rider::darkness:
    case detail::Rider::gust_of_wind:
    case detail::Rider::flaming_sphere:
    case detail::Rider::moonbeam:
    case detail::Rider::spike_growth:
        return; // Aimed areas, resolved by cast_area().
    case detail::Rider::heat_metal:
        return; // Resolved by heat_metal().
    case detail::Rider::flame_blade:
        detail::apply_spell_benefit(target.effects, scope_, a.source.id, a.source.name,
                                    detail::EffectKind::flame_blade, 0);
        log(target.source.name + " gains Flame Blade.",
        {"{name} gains {spell}.", {{"name", target.source.name}, {"spell", "Flame Blade", true}}});
        return;
    case detail::Rider::vicious_mockery:
        // Disadvantage on its next attack roll before the end of its next turn.
        detail::apply_attack_mastery(target.effects, detail::EffectKind::sap, scope_, a.source.id,
                                     a.source.name, next_save_ms(target.source.id));
        log(target.source.name + " has Disadvantage on its next attack roll.",
        {"{name} has Disadvantage on its next attack roll.", {{"name", target.source.name}}});
        return;
    case detail::Rider::dissonant_whispers:
        // Its Reaction carries it as far from the caster as it can.
        if (!target.reaction || detail::incapacitated(target.effects))
            return;
        target.reaction = false;
        log(target.source.name + " flees from " + a.source.name + ".",
        {"{name} flees from {caster}.", {{"name", target.source.name}, {"caster", a.source.name}}});
        push_away(a, target, std::max(0, def(target).speed - detail::speed_penalty(target.effects)) / 5);
        return;
    case detail::Rider::starry_wisp:
    {
        const auto index = static_cast<std::size_t>(&a - actors_.data());
        const unsigned slot = turn_end_ms(index) - (index ? turn_end_ms(index - 1) : 0);
        detail::apply_poisoned(target.effects, scope_, a.source.id, a.source.name,
                               next_turn_ms(a) + slot, detail::EffectKind::lit);
        return;
    }
    case detail::Rider::faerie_fire:
        return; // An aimed area, resolved by cast_area().
    case detail::Rider::shillelagh:
    case detail::Rider::produce_flame:
    {
        // Casting it again ends the earlier one.
        const auto kind = spell.rider == detail::Rider::shillelagh ? detail::EffectKind::shillelagh
                          : detail::EffectKind::produce_flame;
        std::erase_if(target.effects.active, [&](const auto & e)
        {
            return e.kind == kind;
        });
        detail::apply_spell_benefit(target.effects, scope_, a.source.id, a.source.name, kind, 0);
        log(target.source.name + " gains " + std::string(spell.label) + ".",
        {"{name} gains {spell}.", {{"name", target.source.name}, {"spell", std::string(spell.label), true}}});
        return;
    }
    case detail::Rider::barkskin:
        detail::apply_spell_benefit(target.effects, scope_, a.source.id, a.source.name,
                                    detail::EffectKind::barkskin, 0);
        log(target.source.name + " gains Barkskin.",
        {"{name} gains {spell}.", {{"name", target.source.name}, {"spell", "Barkskin", true}}});
        return;
    case detail::Rider::hex:
        detail::apply_spell_benefit(target.effects, scope_, a.source.id, a.source.name,
                                    detail::EffectKind::hex, 0);
        log(target.source.name + " is hexed.", {"{name} is hexed.", {{"name", target.source.name}}});
        return;
    case detail::Rider::charm_person:
        detail::apply_spell_benefit(target.effects, scope_, a.source.id, a.source.name,
                                    detail::EffectKind::charmed, 0);
        log(target.source.name + " is Charmed by " + a.source.name + ".",
        {
            "{name} is Charmed by {caster}.",
            {{"name", target.source.name}, {"caster", a.source.name}}
        });
        return;
    case detail::Rider::dragons_breath:
        detail::apply_spell_benefit(target.effects, scope_, a.source.id, a.source.name,
                                    detail::EffectKind::dragons_breath,
                                    int(*detail::dragon_type(verb)));
        log(target.source.name + " gains Dragon's Breath.",
        {"{name} gains {spell}.", {{"name", target.source.name}, {"spell", "Dragon's Breath", true}}});
        return;
    case detail::Rider::enlarge_reduce:
    {
        const bool enlarge = verb == "enlarge";
        detail::apply_spell_benefit(target.effects, scope_, a.source.id, a.source.name,
                                    enlarge ? detail::EffectKind::enlarged
                                    : detail::EffectKind::reduced, 0);
        log(target.source.name + (enlarge ? " is enlarged." : " is reduced."),
        {enlarge ? "{name} is enlarged." : "{name} is reduced.", {{"name", target.source.name}}});
        return;
    }
    case detail::Rider::blur:
        detail::apply_spell_benefit(target.effects, scope_, a.source.id, a.source.name,
                                    detail::EffectKind::blur, 0);
        log(target.source.name + " gains Blur.",
        {"{name} gains {spell}.", {{"name", target.source.name}, {"spell", "Blur", true}}});
        return;
    case detail::Rider::mirror_image:
        detail::apply_spell_benefit(target.effects, scope_, a.source.id, a.source.name,
                                    detail::EffectKind::mirror_image, 3);
        log(target.source.name + " gains Mirror Image.",
        {"{name} gains {spell}.", {{"name", target.source.name}, {"spell", "Mirror Image", true}}});
        return;
    case detail::Rider::magic_weapon:
        // Casting it again ends the earlier one.
        for (auto &other : actors_)
            std::erase_if(other.effects.active, [&](const auto & e)
        {
            return e.kind == detail::EffectKind::magic_weapon && e.source_actor == a.source.id &&
                   e.source_scope == scope_;
        });
        detail::apply_spell_benefit(target.effects, scope_, a.source.id, a.source.name,
                                    detail::EffectKind::magic_weapon, 1);
        log(target.source.name + " gains Magic Weapon.",
        {"{name} gains {spell}.", {{"name", target.source.name}, {"spell", "Magic Weapon", true}}});
        return;
    case detail::Rider::acid_arrow:
        detail::apply_poisoned(target.effects, scope_, a.source.id, a.source.name,
                               next_save_ms(target.source.id), detail::EffectKind::acid_arrow);
        return;
    case detail::Rider::ray_of_enfeeblement:
        detail::apply_repeating_condition(target.effects, detail::EffectKind::enfeebled, scope_,
                                          a.source.id, a.source.name, dc,
                                          next_save_ms(target.source.id));
        log(target.source.name + " is enfeebled.",
        {"{name} is enfeebled.", {{"name", target.source.name}}});
        return;
    case detail::Rider::hold_person:
        detail::apply_hold_person(target.effects, scope_, a.source.id, a.source.name, dc,
                                  next_save_ms(target.source.id));
        log(target.source.name + " is Paralyzed.",
        {"{name} is Paralyzed.", {{"name", target.source.name}}});
        return;
    case detail::Rider::guiding_bolt:
    {
        // Until the end of the caster's next turn.
        const auto index = static_cast<std::size_t>(&a - actors_.data());
        const unsigned slot = turn_end_ms(index) - (index ? turn_end_ms(index - 1) : 0);
        detail::apply_guiding_bolt(target.effects, scope_, a.source.id, a.source.name,
                                   next_turn_ms(a) + slot);
        log(target.source.name + " is lit by Guiding Bolt.",
        {"{name} is lit by Guiding Bolt.", {{"name", target.source.name}}});
        return;
    }
    case detail::Rider::bane:
        detail::apply_spell_benefit(target.effects, scope_, a.source.id, a.source.name,
                                    detail::EffectKind::bane, 0);
        log(target.source.name + " is weakened by Bane.",
        {"{name} is weakened by Bane.", {{"name", target.source.name}}});
        return;
    case detail::Rider::aid:
    {
        // Aid does not stack with itself; a creature already aided keeps its own.
        if (detail::hit_point_bonus(target.effects) >= 5 || target.dead ||
                !detail::can_apply(target.effects))
            return;
        detail::apply_spell_benefit(target.effects, scope_, a.source.id, a.source.name,
                                    detail::EffectKind::aid, 5);
        log(target.source.name + " gains Aid.",
        {"{name} gains {spell}.", {{"name", target.source.name}, {"spell", "Aid", true}}});
        heal(target, 5);
        return;
    }
    case detail::Rider::lesser_restoration:
        std::erase_if(target.effects.active, [](const auto & e)
        {
            return e.kind == detail::EffectKind::blindness;
        });
        log(target.source.name + " is no longer Blinded.",
        {"{name} is no longer Blinded.", {{"name", target.source.name}}});
        return;
    case detail::Rider::chill_touch:
    {
        const unsigned slot = turn_end_ms(turn_) - (turn_ ? turn_end_ms(turn_ - 1) : 0);
        detail::apply_chill_touch(target.effects, scope_, a.source.id, a.source.name,
                                  detail::round_ms + slot);
        log(target.source.name + " cannot regain HP until the end of the caster's next turn.",
        {
            "{name} cannot regain HP until the end of the caster's next turn.",
            {{"name", target.source.name}}
        });
        return;
    }
    case detail::Rider::shocking_grasp:
        detail::apply_shocking_grasp(target.effects, scope_, a.source.id, a.source.name,
                                     next_turn_ms(target));
        log(target.source.name + " cannot make Opportunity Attacks until its next turn.",
        {
            "{name} cannot make Opportunity Attacks until its next turn.",
            {{"name", target.source.name}}
        });
        return;
    case detail::Rider::ray_of_frost:
        detail::apply_ray_of_frost(target.effects, scope_, a.source.id, a.source.name,
                                   next_turn_ms(a));
        log(target.source.name + " is slowed by Ray of Frost.",
        {"{name} is slowed by Ray of Frost.", {{"name", target.source.name}}});
        return;
    case detail::Rider::shield_of_faith:
    case detail::Rider::heroism:
    case detail::Rider::divine_favor:
    case detail::Rider::bless:
    case detail::Rider::protection_from_evil_and_good:
    case detail::Rider::hunters_mark:
    case detail::Rider::longstrider:
    {
        // Heroism's Temporary HP equal the caster's spellcasting modifier.
        const int value =
            spell.rider == detail::Rider::heroism ? std::max(0, def(a).casting - 2) : 0;
        detail::apply_spell_benefit(target.effects, scope_, a.source.id, a.source.name,
                                    rider_effect(spell.rider), value);
        log(target.source.name + " gains " + std::string(spell.label) + ".",
        {"{name} gains {spell}.", {{"name", target.source.name}, {"spell", std::string(spell.label), true}}});
        if (spell.rider == detail::Rider::hunters_mark)
            reveal_lore(a, target);
        return;
    }
    case detail::Rider::command:
    {
        const int option = detail::command_option(verb);
        const auto index = static_cast<std::size_t>(&target - actors_.data());
        const unsigned slot = turn_end_ms(index) - (index ? turn_end_ms(index - 1) : 0);
        detail::apply_command(target.effects, scope_, a.source.id, a.source.name, option,
                              next_turn_ms(target) + slot);
        log(target.source.name + " must obey " + a.source.name + "'s Command: " +
            command_label(option) + ".",
        {
            "{name} must obey {caster}'s Command: {option}.",
            {   {"name", target.source.name},
                {"caster", a.source.name},
                {"option", command_label(option), true}
            }
        });
        return;
    }
    case detail::Rider::blindness:
        detail::apply_blindness(target.effects, scope_, a.source.id, a.source.name, dc,
                                next_save_ms(target.source.id));
        log(target.source.name + " is Blinded.",
        {"{name} is Blinded.", {{"name", target.source.name}}});
        return;
    }
}

void Session::resolve_spell(const detail::SpellDef &spell, std::string_view verb, Actor &a,
                            EntityId target_id)
{
    const auto &d = def(a);
    const bool upcast = verb.ends_with("_2");
    end_sanctuary(a);
    const InvisibilityEnds ends{*this, a};
    take_metamagic(a, spell);
    if (casting_with(Metamagic::heightened))
        heightened_target_ = target_id;
    // A level-two spell always draws a level-two slot; a level-one spell draws
    // one only in its upcast form.
    if (spell.level)
    {
        if (upcast || spell.level >= 2)
            --a.slots2;
        else
            --a.slots;
        a.spent_slot = true;
    }
    auto rolled = spell.dice;
    rolled.count += static_cast<int>(upcast ? spell.upcast.extra_dice : 0u);
    if (spell.add_casting_modifier)
        rolled.bonus = d.casting - 2;
    const unsigned instances = spell.instances + (upcast ? spell.upcast.extra_instances : 0u);
    const int dc = spell_dc(a);
    const auto name = std::string(spell.label);
    switch (spell.pattern)
    {
    case detail::SpellPattern::smite:
        return; // Smites resolve through resolve_smite, after the caster's own hit.
    case detail::SpellPattern::camp:
    case detail::SpellPattern::exploration:
        return; // Never offered in combat.
    case detail::SpellPattern::weapon_strike:
        return; // Resolved by strike_true().
    case detail::SpellPattern::reaction:
        return; // Cast through the Shield prompt.
    case detail::SpellPattern::stabilize:
    {
        auto &target = actor(target_id);
        detail::stabilize(target, rng_);
        log(a.source.name + " casts " + name + ": " + target.source.name + " is Stable.",
        {
            "{name} casts {spell}: {target} is Stable.",
            {{"name", a.source.name}, {"spell", name, true}, {"target", target.source.name}}
        });
        return;
    }
    case detail::SpellPattern::buff:
        if (spell.concentration)
            begin_concentration(a, spell);
        apply_rider(spell, verb, a, actor(target_id), dc);
        return;
    case detail::SpellPattern::heal:
        heal(actor(target_id), dice(rolled) + disciple_of_life(a, upcast ? 2 : spell.level));
        return;
    case detail::SpellPattern::spell_attack:
    {
        auto &target = actor(target_id);
        // Spiritual Weapon's force appears beside the target and lasts with the
        // caster's Concentration, whether or not this first attack hits.
        if (spell.rider == detail::Rider::spiritual_weapon)
        {
            begin_concentration(a, spell);
            zones_.push_back({a.source.id, ZoneKind::spiritual_weapon,
                              {spectral_cell(target, a.source.cell)}});
        }
        const auto type = cast_damage_type(
                              detail::chromatic_type(verb)
                              .value_or(detail::burst_type(verb).value_or(spell.damage)));
        // Sorcerous Burst: each 8 adds a d8, at most the spellcasting modifier.
        const int bursts = spell.id == "sorcerous_burst" ? std::max(1, d.casting - 2) : 0;
        const bool blast = spell.id == "eldritch_blast";
        // Agonizing Blast adds the Charisma modifier to Eldritch Blast's damage.
        if (blast && d.agonizing_blast)
            rolled.bonus += d.casting - 2;
        const bool struck = attack(a, target, !spell.melee, true, rolled, type, bursts);
        // Repelling Blast pushes a Large or smaller creature 10 feet away.
        if (struck && blast && d.repelling_blast && !target.dead && def(target).size <= 3)
            push_away(a, target, 2);
        if (struck)
            apply_rider(spell, verb, a, target, dc);
        else if (spell.rider == detail::Rider::acid_arrow && target.hp > 0)
        {
            // A miss splashes half the initial damage, without the later burn.
            const int amount = resolved_damage(target, spell.damage, dice(rolled) / 2);
            log(target.source.name + " takes " + std::to_string(amount) +
                " Acid damage from the splash.",
            {
                "{name} takes {damage} Acid damage from the splash.",
                {{"name", target.source.name}, {"damage", std::to_string(amount)}}
            });
            damage(target, amount, false);
        }
        else if (d.evoker && !spell.level && target.hp > 0)
            potent_cantrip(target, spell, rolled);
        // Ice Knife's shard explodes, hit or miss.
        if (spell.rider == detail::Rider::ice_knife)
            ice_burst(a, target, upcast);
        return;
    }
    case detail::SpellPattern::repeat_attack:
        // Re-read the target each pass: it may drop before the later rays.
        for (unsigned ray = 0; ray < instances && actor(target_id).hp > 0; ++ray)
            attack(a, actor(target_id), !spell.melee, true, rolled, cast_damage_type(spell.damage));
        return;
    case detail::SpellPattern::auto_damage:
    {
        auto &target = actor(target_id);
        if (sanctuary_stops(a, target))
            return;
        if (spell.id == "magic_missile")
            ask_reaction(target, Asked::missile);
        if (spell.id == "magic_missile" &&
                detail::has_effect(target.effects, detail::EffectKind::shield))
        {
            log(target.source.name + "'s Shield blocks Magic Missile.",
            {"{name}'s Shield blocks Magic Missile.", {{"name", target.source.name}}});
            return;
        }
        int total = 0;
        // Instances resolve separately so resistance applies per instance.
        for (unsigned n = 0; n < instances; ++n)
            total += resolved_damage(target, spell.damage, dice(rolled));
        // The plain English keeps its original lower-case damage word so stored
        // combat logs stay byte identical. The template uses the translatable
        // capitalised name, matching the convention in resolved_damage().
        auto lowered = std::string(detail::damage_name(spell.damage));
        for (auto &c : lowered)
            if (c >= 'A' && c <= 'Z')
                c += 32;
        log(a.source.name + " casts " + name + " for " + std::to_string(total) + " " + lowered +
            " damage.",
        {
            "{name} casts {spell} for {damage} {type} damage.",
            {   {"name", a.source.name},
                {"spell", name, true},
                {"damage", std::to_string(total)},
                {"type", std::string(detail::damage_name(spell.damage)), true}
            }
        });
        damage(target, total);
        return;
    }
    case detail::SpellPattern::save_damage:
    {
        auto &target = actor(target_id);
        if (sanctuary_stops(a, target))
            return;
        if (spell.concentration)
            begin_concentration(a, spell);
        log(a.source.name + " casts " + name + " at " + target.source.name + ".",
        {
            "{name} casts {spell} at {target}.",
            {{"name", a.source.name}, {"spell", name, true}, {"target", target.source.name}}
        });
        const bool saved = saving_throw_succeeds(target, spell.save, dc);
        // Potent Cantrip: a saved-against cantrip still deals half damage.
        if (saved && !spell.half_on_success && !(d.evoker && !spell.level))
            return;
        // Halving comes before resistance, which resolved_damage applies.
        const int rolled_damage = spell_dice(a, rolled);
        const auto damage_type = cast_damage_type(spell.damage);
        const auto type = std::string(detail::damage_name(damage_type));
        const int amount =
            resolved_damage(target, damage_type, saved ? rolled_damage / 2 : rolled_damage);
        log(target.source.name + " takes " + std::to_string(amount) + " " + type + " damage.",
        {
            "{name} takes {damage} {type} damage.",
            {   {"name", target.source.name},
                {"damage", std::to_string(amount)},
                {"type", type, true}
            }
        });
        damage(target, amount);
        if (!saved && spell.rider != detail::Rider::none && !target.dead)
            apply_rider(spell, verb, a, target, dc);
        return;
    }
    case detail::SpellPattern::save_condition:
    {
        auto &target = actor(target_id);
        if (spell.concentration)
            begin_concentration(a, spell);
        if (spell.rider == detail::Rider::heat_metal)
        {
            std::erase_if(target.effects.active, [&](const auto & e)
            {
                return e.kind == detail::EffectKind::heated && e.source_actor == a.source.id;
            });
            if (detail::can_apply(target.effects))
                detail::apply_spell_benefit(target.effects, scope_, a.source.id, a.source.name,
                                            detail::EffectKind::heated, 1);
            heat_metal(a, target);
            return;
        }
        // A willing creature forgoes the save.
        if (verb == "enlarge" ||
                !saving_throw_succeeds(target, spell.save, dc, fought_advantage(spell)))
            apply_rider(spell, verb, a, target, dc);
        else if (spell.rider == detail::Rider::ray_of_enfeeblement)
        {
            // A success still leaves Disadvantage on its next attack roll until
            // the start of the caster's next turn, as Sap does.
            detail::apply_attack_mastery(target.effects, detail::EffectKind::sap, scope_,
                                         a.source.id, a.source.name, next_turn_ms(a));
            log(target.source.name + " has Disadvantage on its next attack roll.",
            {"{name} has Disadvantage on its next attack roll.", {{"name", target.source.name}}});
        }
        return;
    }
    }
}

void Session::offer_spells(std::vector<Command> &commands, const Actor &a, const Actor &other,
                           int feet, detail::SpellTarget scope, bool bonus_pass) const
{
    // The Bonus Action pass runs before the loop that skips corpses, so filter
    // them here too; the Action pass has already done it and is unaffected.
    // A raging creature cannot cast spells.
    if (other.dead || rage_of(a))
        return;
    const auto &d = def(a);
    for (const auto &spell : detail::spell_table)
    {
        // Smites follow the caster's own melee hit; the smite window offers them.
        if (spell.pattern == detail::SpellPattern::smite ||
                spell.pattern == detail::SpellPattern::camp ||
                spell.pattern == detail::SpellPattern::exploration ||
                spell.pattern == detail::SpellPattern::weapon_strike ||
                spell.pattern == detail::SpellPattern::reaction || spell.target != scope)
            continue;
        // Quickened Spell offers an Action spell in the Bonus Action pass.
        const bool as_bonus = spell.bonus_action || readies(a, Metamagic::quickened, spell);
        if (as_bonus != bonus_pass)
            continue;
        if (!detail::knows_spell(d.spells, spell.id))
            continue;
        switch (spell.target)
        {
        case detail::SpellTarget::enemy:
            if (other.source.side == a.source.side || other.hp <= 0)
                continue;
            break;
        case detail::SpellTarget::wounded_ally:
            if (other.source.side != a.source.side || other.hp >= max_hp(other))
                continue;
            break;
        case detail::SpellTarget::any_creature:
            break;
        case detail::SpellTarget::ally:
            if (other.source.side != a.source.side)
                continue;
            break;
        case detail::SpellTarget::self:
            if (other.source.id != a.source.id)
                continue;
            break;
        case detail::SpellTarget::area:
            break; // Aimed at a point after it is chosen.
        case detail::SpellTarget::dying_ally:
            if (other.source.side != a.source.side || other.hp != 0 || other.stable)
                continue;
            break;
        }
        if (feet > spell_range(a, spell))
            continue;
        if (spell.requires_sight && !can_see(a, other))
            continue;
        if (spell.requires_effect_capacity && !detail::can_apply(other.effects))
            continue;
        // Lesser Restoration has a condition to end only on a Blinded creature.
        if (spell.rider == detail::Rider::lesser_restoration && !detail::blinded(other.effects))
            continue;
        if (spell.rider == detail::Rider::heat_metal && !wears_metal(other))
            continue;
        if (spell.humanoid_only && def(other).creature_type != "humanoid")
            continue;
        if (spell.not_self && other.source.id == a.source.id)
            continue;
        // Mage Armor needs a creature wearing no armor, and lasts only once.
        if (spell.rider == detail::Rider::mage_armor &&
                (!def(other).mage_armor_ac ||
                 detail::has_effect(other.effects, detail::EffectKind::mage_armor)))
            continue;
        if (spell.rider == detail::Rider::expeditious_retreat &&
                detail::has_effect(other.effects, detail::EffectKind::expeditious_retreat))
            continue;
        if ((spell.rider == detail::Rider::blur &&
                detail::has_effect(other.effects, detail::EffectKind::blur)) ||
                (spell.rider == detail::Rider::invisibility &&
                 detail::has_effect(other.effects, detail::EffectKind::invisible)) ||
                (spell.rider == detail::Rider::see_invisibility &&
                 detail::has_effect(other.effects, detail::EffectKind::see_invisibility)) ||
                (spell.rider == detail::Rider::mirror_image &&
                 detail::has_effect(other.effects, detail::EffectKind::mirror_image)) ||
                (spell.rider == detail::Rider::magic_weapon &&
                 detail::has_effect(other.effects, detail::EffectKind::magic_weapon)) ||
                (spell.rider == detail::Rider::barkskin &&
                 detail::has_effect(other.effects, detail::EffectKind::barkskin)) ||
                (spell.rider == detail::Rider::shillelagh && !d.shillelagh_weapon))
            continue;
        if (as_bonus ? !a.bonus : !a.actions.available(true))
            continue;
        const bool aimed = spell.target == detail::SpellTarget::area;
        const auto offer = [&](std::string verb, std::string label)
        {
            commands.push_back({revision_, a.source.id, aimed ? 0 : other.source.id,
                                std::move(verb), std::move(label), Cell{}, 0, aimed});
        };
        if (spell.id == "chromatic_orb")
        {
            if (a.spent_slot)
                continue;
            for (const auto type : detail::chromatic_types)
            {
                const auto verb = "chromatic_orb_" + std::string(type);
                const auto label = "Chromatic Orb: " +
                                   std::string(detail::damage_name(detail::damage_type(type)));
                if (a.slots > 0)
                    offer(verb, label);
                if (a.slots2 > 0)
                    offer(verb + "_2", label + " (level 2 slot)");
            }
            continue;
        }
        if (spell.rider == detail::Rider::resistance)
        {
            for (const auto type : detail::resistance_types)
                offer("resistance_" + std::string(type),
                      "Resistance: " + std::string(detail::damage_name(detail::damage_type(type))));
            continue;
        }
        if (spell.id == "sorcerous_burst")
        {
            for (const auto type : detail::burst_types)
                offer("sorcerous_burst_" + std::string(type),
                      "Sorcerous Burst: " +
                      std::string(detail::damage_name(detail::damage_type(type))));
            continue;
        }
        if (!spell.level)
        {
            offer(std::string(spell.id), std::string(spell.label));
            continue;
        }
        if (a.spent_slot)
            continue;
        // A level-two spell is only ever cast from a level-two slot, so it has
        // no separate upcast verb.
        if (spell.level >= 2)
        {
            if (a.slots2 <= 0)
                continue;
            if (spell.rider == detail::Rider::dragons_breath)
            {
                if (!detail::has_effect(other.effects, detail::EffectKind::dragons_breath))
                    for (const auto type : detail::dragon_types)
                        offer("dragons_breath_" + std::string(type),
                              "Dragon's Breath: " +
                              std::string(detail::damage_name(detail::damage_type(type))));
                continue;
            }
            if (spell.rider == detail::Rider::enlarge_reduce)
            {
                if (other.source.side == a.source.side)
                    offer("enlarge", "Enlarge");
                else
                    offer("reduce", "Reduce");
                continue;
            }
            offer(std::string(spell.id), std::string(spell.label));
            continue;
        }
        if (spell.rider == detail::Rider::command)
        {
            for (const auto option : detail::command_options)
            {
                const auto verb = "command_" + std::string(option);
                const auto label = "Command: " + command_label(detail::command_option(verb));
                if (a.slots > 0)
                    offer(verb, label);
                if (a.slots2 > 0)
                    offer(verb + "_2", label + " (level 2 slot)");
            }
            continue;
        }
        if (a.slots > 0)
            offer(std::string(spell.id), std::string(spell.label));
        if (a.slots2 > 0)
            offer(std::string(spell.id) + "_2", std::string(spell.label) + " (level 2 slot)");
    }
}

std::vector<Command> Session::legal_commands() const
{
    std::vector<Command> commands;
    if (outcome_ != Outcome::ongoing)
        return commands;
    const auto add = [&](EntityId who, std::string verb, std::string label, EntityId target = 0,
                         Cell destination = Cell{})
    {
        commands.push_back(
        {revision_, who, target, std::move(verb), std::move(label), destination});
    };
    if (reaction_prompt_)
    {
        const auto &question = reaction_prompt_->question;
        const auto &who = actor(question.target);
        if (question.asked == Asked::redirect)
            add(who.source.id, "redirect", "Redirect the attack (1 Focus)");
        else if (question.asked == Asked::rebuke)
            add(who.source.id, "rebuke", "Cast Hellish Rebuke");
        else if (question.asked == Asked::inspiration)
            add(who.source.id, "inspire", "Use Bardic Inspiration");
        else if (question.asked == Asked::cutting)
            add(who.source.id, "cutting", "Use Cutting Words");
        else
        {
            if (can_shield(who) && !question.critical)
                add(who.source.id, "shield",
                    question.asked == Asked::missile ? "Cast Shield against Magic Missile"
                    : "Cast Shield against the hit");
            if (question.asked == Asked::hit && can_deflect(who, question.deflectable))
                add(who.source.id, "deflect", "Deflect Attacks");
        }
        add(who.source.id, "decline", "Decline reaction");
        return commands;
    }
    // While aiming an area spell, only moving the preview, casting or cancelling.
    if (area_)
    {
        const auto &caster = actor(area_->caster);
        const auto &spell = *detail::find_spell(area_->verb);
        // Misty Step and Flaming Sphere need an unoccupied square.
        const bool open_square = spell.rider == detail::Rider::misty_step ||
                                 spell.rider == detail::Rider::flaming_sphere;
        for (int y = 0; y < board_.height; ++y)
            for (int x = 0; x < board_.width; ++x)
                if (distance(caster.source.cell, Cell{x, y}) <= spell_range(caster, spell) &&
                        (!open_square || teleport_open(caster, Cell{x, y})))
                    add(caster.source.id, "area_move", std::string(spell.label), 0, Cell{x, y});
        if (!open_square || teleport_open(caster, area_->center))
            add(caster.source.id, "area_cast", "Cast spell");
        add(caster.source.id, "spell_cancel", "Cancel");
        return commands;
    }
    // While choosing a spell's creatures, only the choice itself is open:
    // any living creature in range toggles, then cast or cancel.
    if (selection_)
    {
        const auto &caster = actor(selection_->caster);
        const auto &spell = *detail::find_spell(selection_->verb);
        for (const auto &other : actors_)
            if (!other.dead && distance(caster.source.cell, other.source.cell) <= spell.range)
                add(caster.source.id, selection_->verb, std::string(spell.label), other.source.id);
        if (!selection_->chosen.empty())
            add(caster.source.id, "spell_cast", "Cast spell");
        add(caster.source.id, "spell_cancel", "Cancel");
        return commands;
    }
    if (!initiative_choices_.empty())
    {
        for (const auto id : initiative_choices_)
        {
            const auto &a = actor(id);
            add(id, "initiative_keep", "Keep initiative");
            if (def(a).alert)
                for (const auto &ally : actors_)
                    if (ally.source.id != id && ally.source.side == a.source.side && conscious(ally))
                        add(id, "initiative_swap", "Swap initiative", ally.source.id);
            if (metabolism_ready(a))
                add(id, "uncanny_metabolism", "Uncanny Metabolism");
        }
        return commands;
    }
    const auto add_weapons = [&](const Actor & a, bool reacting)
    {
        for (const auto &item : items_)
            if (item.holder == a.source.id && !item.stowed && detail::weapon(item.definition) &&
                    item.id != held_weapon(a))
            {
                const auto choice = item_actor(a, item.id);
                if (!reacting || weapon_reaction(choice, def(choice)))
                {
                    add(a.source.id, "weapon_select", "Select weapon");
                    commands.back().item = item.id;
                }
            }
    };
    const auto filtered = [&]
    {
        // A Charmed creature cannot attack or target its charmer.
        std::erase_if(commands, [&](const auto & command)
        {
            return command.target && command.target != command.actor &&
                   charmed_by(actor(command.actor), command.target);
        });
        std::erase_if(commands,
        [&](const auto & command)
        {
            const bool ranged =
            command.verb == "ranged" || command.verb == "throw" ||
            command.verb == "light_ranged" || command.verb == "light_throw" ||
            command.verb == "nick_ranged" || command.verb == "nick_throw";
            if (!ranged && command.verb != "melee" && command.verb != "opportunity" &&
                    command.verb != "light_melee" && command.verb != "nick_melee")
                return false;
            auto attacking = actor(command.actor);
            if (command.item)
            {
                if (command.verb == "throw" || command.verb == "light_throw" ||
                        command.verb == "nick_throw")
                    attacking = thrown_actor(attacking,
                                             items_.at(command.item - 1).definition);
                else if ((command.verb.starts_with("light_") ||
                          command.verb.starts_with("nick_")))
                    attacking = item_actor(attacking, command.item);
            }
            return !mastery_capacity(attacking, actor(command.target), ranged);
        });
        return commands;
    };
    if (graze_)
    {
        add(graze_->actor, "effect_use", "Use", graze_->target);
        add(graze_->actor, "effect_skip", "Skip", graze_->target);
        return commands;
    }
    if (!champion_move_ && effect_waiting())
    {
        if (mastery_ && mastery_->targeting)
        {
            const auto &m = *mastery_;
            if (m.kind == detail::Mastery::cleave)
            {
                for (auto target : cleave_targets(m))
                    add(m.actor, "effect_attack", "Cleave attack", target);
            }
            else
                for (auto cell : push_cells(m))
                    add(m.actor, "effect_push", "Push", m.target, cell);
            add(m.actor, "effect_skip", "Skip", m.target);
            commands.back().item = 1;
            if (m.kind == detail::Mastery::cleave &&
                    std::none_of(commands.begin(), commands.end(),
                                 [&](const auto & c)
        {
            return c.verb == "effect_attack" &&
                   actor(c.target).source.side != actor(m.actor).source.side;
            }))
            std::rotate(commands.begin(), commands.end() - 1, commands.end());
        }
        else
        {
            const auto choice = effect_choices();
            for (const auto &option : choice.options)
            {
                if (option.available)
                {
                    add(choice.actor, "effect_use", "Use", choice.target);
                    commands.back().item = option.id;
                }
                add(choice.actor, "effect_skip", "Skip", choice.target);
                commands.back().item = option.id;
            }
        }
        return commands;
    }
    if (champion_move_)
    {
        add(champion_move_->actor, "end", "Finish free move");
        for (const auto cell : movement_reach(champion_move_->actor))
            add(champion_move_->actor, "move", "Free move", 0, cell);
        return filtered();
    }
    if (check_choice_)
    {
        add(check_choice_->actor, "mind_use", "Use Tactical Mind", check_choice_->target);
        add(check_choice_->actor, "mind_skip", "Keep failed check", check_choice_->target);
        return filtered();
    }
    if (temporary_offer_)
    {
        add(actors_[turn_].source.id, "temp_hp_keep", "Keep current");
        add(actors_[turn_].source.id, "temp_hp_use", "Use new");
        return filtered();
    }
    if (pending())
    {
        const auto reactor = std::find_if(actors_.begin(), actors_.end(),
                                          [&](const auto & a)
        {
            return a.source.id == pending();
        });
        add_weapons(*reactor, true);
        if (weapon_reaction(*reactor, def(*reactor)))
            add(pending(), "opportunity", "Opportunity attack", actors_[turn_].source.id);
        add(pending(), "decline", "Decline reaction");
        return filtered();
    }
    const auto &a = actors_[turn_];
    if (!conscious(a))
        return filtered();
    const auto &d = def(a);
    const auto id = a.source.id;
    add(id, "end", "End turn");
    add_weapons(a, false);
    const int speed = a.aim_used ? 0 : std::max(0, d.speed - detail::speed_penalty(a.effects));
    if (a.effects.prone && speed > 0 && movement_left(a) >= speed / 2)
        add(id, "stand_up", "Stand up");
    if (d.surges && a.surges > 0 && !a.surge_used)
        add(id, "action_surge", "Action Surge", id);
    if (a.bonus && d.sneak_level >= 3 && !a.moved)
        add(id, "steady_aim", "Steady Aim");
    if (a.bonus && d.cunning)
    {
        add(id, "cunning_dash", "Cunning Action: Dash");
        add(id, "cunning_disengage", "Cunning Action: Disengage");
    }
    if (a.bonus && a.rushes > 0)
        add(id, "adrenaline_rush", "Adrenaline Rush", id);
    // The Cleric's Channel Divinity: Divine Spark heals or harms another creature
    // within 30 feet; Turn Undead needs an Undead enemy within 30 feet.
    if (a.channel_divinity > 0 && a.actions.available() && d.divine_spark)
    {
        bool undead = false;
        for (const auto &other : actors_)
        {
            const int feet = distance(a.source.cell, other.source.cell);
            if (other.source.id == a.source.id || other.dead || feet > 30)
                continue;
            if (other.source.side != a.source.side && other.hp > 0 &&
                    def(other).creature_type == "undead")
                undead = true;
            const bool wounded_ally = other.source.side == a.source.side &&
                                      other.hp < max_hp(other) &&
                                      !detail::healing_blocked(other.effects);
            const bool enemy = other.source.side != a.source.side && other.hp > 0;
            if ((wounded_ally || enemy) && can_see(a, other))
                add(id, "divine_spark", "Divine Spark", other.source.id);
        }
        if (undead)
            add(id, "turn_undead", "Turn Undead", id);
        // Preserve Life (Life Domain) needs a Bloodied ally within 30 feet.
        if (d.life_domain && std::any_of(actors_.begin(), actors_.end(), [&](const auto & other)
    {
        return other.source.side == a.source.side && !other.dead &&
                   other.hp * 2 <= max_hp(other) &&
                   distance(a.source.cell, other.source.cell) <= 30;
        }))
        add(id, "preserve_life", "Preserve Life", id);
    }
    // A creature caught by Ensnaring Strike or Entangle may spend its Action to
    // break free.
    if (a.actions.available() && detail::restrained(a.effects))
        add(id, "escape",
            detail::has_effect(a.effects, detail::EffectKind::webbed) ? "Escape the webs"
            : "Escape the vines",
            id);
    // Sleep ends when someone within 5 feet spends an Action to shake the sleeper.
    if (a.actions.available())
        for (const auto &other : actors_)
            if (other.source.side == a.source.side && other.source.id != a.source.id &&
                    distance(a.source.cell, other.source.cell) <= 5 &&
                    (detail::has_effect(other.effects, detail::EffectKind::drowsy) ||
                     detail::has_effect(other.effects, detail::EffectKind::asleep)))
                add(id, "shake_awake", "Shake awake", other.source.id);
    // Martial Arts: an Unarmed Strike as a Bonus Action.
    if (a.bonus && d.martial_arts)
        for (const auto &other : actors_)
            if (other.source.side != a.source.side && other.hp > 0 && !other.dead &&
                    distance(a.source.cell, other.source.cell) <= 5)
                add(id, "martial_arts", "Unarmed Strike", other.source.id);
    // Font of Magic: a Bonus Action turns Sorcery Points into a slot; a slot
    // becomes Sorcery Points without an action.
    if (d.sorcery_points)
    {
        if (a.bonus && a.lay_on_hands >= 2 && a.slots < slot_room(d))
            add(id, "create_slot_1", "Font of Magic: create a level-1 slot (2 Sorcery Points)");
        if (a.bonus && d.level >= 3 && a.lay_on_hands >= 3 && a.slots2 < slot2_room(d))
            add(id, "create_slot_2", "Font of Magic: create a level-2 slot (3 Sorcery Points)");
        if (a.slots > 0 && a.lay_on_hands < d.sorcery_points)
            add(id, "convert_slot_1", "Font of Magic: a level-1 slot into 1 Sorcery Point");
        if (a.slots2 > 0 && a.lay_on_hands < d.sorcery_points)
            add(id, "convert_slot_2", "Font of Magic: a level-2 slot into 2 Sorcery Points");
    }
    // Metamagic: ready an affordable option for the next spell this turn, no
    // action; the Sorcery Points are spent when a spell it changes is cast.
    if (!d.metamagic.empty())
    {
        if (readied_metamagic(a))
            add(id, "metamagic_cancel", "Metamagic: cancel");
        else
            for (const auto &known : d.metamagic)
            {
                const auto index = std::size_t(std::find(metamagic_ids.begin(), metamagic_ids.end(),
                                               known) - metamagic_ids.begin());
                const auto option = Metamagic(index);
                const int cost = metamagic_cost(option);
                // Subtle Spell retired with spell components (CLASS-11).
                if (a.lay_on_hands < cost || option == Metamagic::subtle)
                    continue;
                const std::string points = " (" + std::to_string(cost) +
                                           (cost == 1 ? " Sorcery Point)" : " Sorcery Points)");
                const std::string label(metamagic_labels[index]);
                if (option != Metamagic::transmuted)
                    add(id, "metamagic_" + known, "Metamagic: " + label + points);
                else
                    for (const auto type : transmuted_types)
                        add(id, "metamagic_transmuted_" + std::string(type),
                            "Metamagic: " + label + " to " +
                            std::string(detail::damage_name(detail::damage_type(type))) + points);
            }
    }
    // Bardic Inspiration: a Bonus Action gives an ally within 60 feet the die.
    if (a.bonus && d.bardic_inspiration && a.arcane > 0)
        for (const auto &other : actors_)
            if (other.source.side == a.source.side && other.source.id != id && !other.dead &&
                    distance(a.source.cell, other.source.cell) <= 60 &&
                    !detail::has_effect(other.effects, detail::EffectKind::inspired))
                add(id, "bardic_inspiration", "Bardic Inspiration", other.source.id);
    // Innate Sorcery: a Bonus Action, twice per Long Rest.
    if (a.bonus && d.innate_sorcery && a.free_casts > 0 &&
            !detail::has_effect(a.effects, detail::EffectKind::innate_sorcery))
        add(id, "innate_sorcery", "Innate Sorcery");
    // Monk's Focus: Patient Defense and Step of the Wind, free or with a Focus
    // Point, and Flurry of Blows for a Focus Point.
    if (a.bonus && d.focus)
    {
        add(id, "patient_defense", "Patient Defense: Disengage");
        add(id, "step_of_the_wind", "Step of the Wind: Dash");
        if (a.surges > 0)
        {
            add(id, "patient_defense_focus", "Patient Defense: Disengage and Dodge (1 Focus)");
            add(id, "step_of_the_wind_focus", "Step of the Wind: Disengage and Dash (1 Focus)");
            for (const auto &other : actors_)
                if (other.source.side != a.source.side && other.hp > 0 && !other.dead &&
                        distance(a.source.cell, other.source.cell) <= 5)
                {
                    add(id, "flurry_of_blows", "Flurry of Blows", other.source.id);
                    // Open Hand Technique, chosen for the whole Flurry.
                    if (d.open_hand)
                    {
                        add(id, "flurry_addle", "Flurry of Blows: Addle", other.source.id);
                        add(id, "flurry_push", "Flurry of Blows: Push", other.source.id);
                        add(id, "flurry_topple", "Flurry of Blows: Topple", other.source.id);
                    }
                }
        }
    }
    // Wild Shape: a Bonus Action takes a Beast form or leaves it.
    if (a.bonus && d.wild_shapes)
    {
        if (a.channel_divinity > 0)
            for (const auto &form : detail::beast_forms)
                add(id, "wild_shape_" + std::string(form.key),
                    "Wild Shape: " + std::string(form.label));
        if (a.form)
            add(id, "leave_wild_shape", "Leave Wild Shape");
    }
    // Rage: a Bonus Action outside Heavy armor; on a later turn a Bonus Action
    // extends it.
    if (a.bonus && d.rages)
    {
        const auto *rage = rage_of(a);
        if (!rage && a.channel_divinity > 0 && !d.heavy_armor)
            add(id, "rage", "Rage");
        else if (rage && rage->remaining_ms <= detail::round_ms)
            add(id, "extend_rage", "Extend Rage");
    }
    // Expeditious Retreat: Dash as a Bonus Action while it lasts.
    if (a.bonus && detail::has_effect(a.effects, detail::EffectKind::expeditious_retreat))
        add(id, "retreat_dash", "Expeditious Retreat: Dash");
    // Flaming Sphere: a Bonus Action rolls it up to 30 feet into a creature's
    // space, where it stops beside the creature and burns it.
    for (const auto &zone : zones_)
        if (zone.kind == ZoneKind::flaming_sphere && zone.caster == id && a.bonus)
            for (const auto &other : actors_)
                if (other.hp > 0 && !other.dead && other.source.id != id &&
                        distance(zone.cells.front(), other.source.cell) <= 30)
                    add(id, "roll_flaming_sphere", "Roll Flaming Sphere", other.source.id);
    // Heat Metal: a Bonus Action on later turns heats the metal again.
    if (a.bonus)
        for (const auto &other : actors_)
            if (!other.dead && other.hp > 0 &&
                    distance(a.source.cell, other.source.cell) <= 60 &&
                    std::any_of(other.effects.active.begin(), other.effects.active.end(),
                                [&](const auto & e)
        {
            return e.kind == detail::EffectKind::heated && e.source_actor == id && !e.dc;
        }))
        add(id, "heat_metal_again", "Heat Metal again", other.source.id);
    // Moonbeam: a Magic action moves the beam up to 60 feet, here onto a creature.
    for (const auto &zone : zones_)
        if (zone.kind == ZoneKind::moonbeam && zone.caster == id && a.actions.available(true))
            for (const auto &other : actors_)
                if (other.hp > 0 && !other.dead && !in_zone(zone, other.source.cell) &&
                        distance(zone.cells.front(), other.source.cell) <= 60)
                    add(id, "move_moonbeam", "Move Moonbeam", other.source.id);
    // Spiritual Weapon: on later turns a Bonus Action moves the force up to 20
    // feet and attacks a creature within 5 feet of it.
    if (const auto *force = spiritual_weapon(a); force && a.bonus)
        for (const auto &other : actors_)
            if (other.source.side != a.source.side && other.hp > 0 && !other.dead &&
                    distance(force->cells.front(), other.source.cell) <= 25)
                add(id, "spiritual_weapon_strike", "Spiritual Weapon attack", other.source.id);
    // Horde Breaker: once per turn, after a weapon attack, another creature
    // within 5 feet of the first target and within the weapon's reach or range.
    if (d.horde_breaker && a.horde_origin && !a.horde_used)
    {
        const auto &origin = actor(a.horde_origin);
        for (const auto &other : actors_)
        {
            const int feet = distance(a.source.cell, other.source.cell);
            if (other.source.id != origin.source.id && other.source.side != a.source.side &&
                    other.hp > 0 && distance(origin.source.cell, other.source.cell) <= 5 &&
                    (feet <= d.reach || (d.range > 0 && feet <= d.long_range)))
                add(id, "horde_breaker", "Horde Breaker", other.source.id);
        }
    }
    // Favored Enemy casts Hunter's Mark without a slot, while no living quarry
    // carries the mark already; a dropped quarry's mark moves for a Bonus Action.
    const bool marking = std::any_of(actors_.begin(), actors_.end(), [&](const auto & other)
    {
        return other.hp > 0 && !other.dead && marked_by(other, a);
    });
    const bool free_mark = a.bonus && a.free_casts > 0 && d.favored_enemy && !marking &&
                           detail::knows_spell(d.spells, "hunters_mark") && !d.str_dex_disadvantage;
    const bool moving_mark = a.bonus && mark_can_move(a);
    // Hex moves to a new creature as a Bonus Action once its first drops.
    if (a.bonus && hex_can_move(a))
        for (const auto &other : actors_)
            if (other.source.side != a.source.side && other.hp > 0 && !other.dead &&
                    distance(a.source.cell, other.source.cell) <= 90 && can_see(a, other))
                add(id, "hex_move", "Move Hex", other.source.id);
    if (free_mark || moving_mark)
        for (const auto &other : actors_)
            if (other.source.side != a.source.side && other.hp > 0 && !other.dead &&
                    distance(a.source.cell, other.source.cell) <= 90 && can_see(a, other) &&
                    !marked_by(other, a))
            {
                if (free_mark)
                    add(id, "hunters_mark_free", "Hunter's Mark (Favored Enemy)", other.source.id);
                if (moving_mark)
                    add(id, "hunters_mark_move", "Move Hunter's Mark", other.source.id);
            }
    // Sacred Weapon comes with the Attack action, so it is offered while the
    // Action is unspent and costs only a Channel Divinity use.
    if (d.sacred_weapon && a.channel_divinity > 0 && a.actions.available() && d.melee.count &&
            !detail::has_effect(a.effects, detail::EffectKind::sacred_weapon))
        add(id, "sacred_weapon", "Sacred Weapon", id);
    if (a.bonus && d.aggressive && speed > 0 && enemy_in_sight(a))
        add(id, "aggressive", "Aggressive");
    if (a.bonus && a.winds > 0 && a.hp < max_hp(a))
        add(id, "second_wind", "Second Wind", id);
    // Lay On Hands: touch yourself or an adjacent wounded ally whose healing
    // can take effect, including one at 0 Hit Points.
    if (a.bonus && d.lay_on_hands && a.lay_on_hands > 0)
        for (const auto &other : actors_)
            if (other.source.side == a.source.side && !other.dead &&
                    other.hp < max_hp(other) && !detail::healing_blocked(other.effects) &&
                    distance(a.source.cell, other.source.cell) <= 5)
                add(id, "lay_on_hands", "Lay On Hands", other.source.id);
    // Smites, right after this actor's own melee hit on a creature still standing.
    if (a.bonus && a.smite_target && actor(a.smite_target).hp > 0 && !d.spells.empty())
    {
        const bool slot = a.slots > 0 && !a.spent_slot;
        const bool melee = a.smite_melee;
        if (melee && detail::knows_spell(d.spells, "divine_smite") && a.free_casts > 0)
            add(id, "divine_smite_free", "Divine Smite (Paladin's Smite)", a.smite_target);
        if (melee && detail::knows_spell(d.spells, "divine_smite") && slot)
            add(id, "divine_smite", "Divine Smite", a.smite_target);
        if (melee && detail::knows_spell(d.spells, "searing_smite") && slot)
            add(id, "searing_smite", "Searing Smite", a.smite_target);
        if (detail::knows_spell(d.spells, "ensnaring_strike") && slot)
            add(id, "ensnaring_strike", "Ensnaring Strike", a.smite_target);
    }
    for (const auto &other : actors_)
        for (const auto scope :
                {
                    detail::SpellTarget::wounded_ally, detail::SpellTarget::ally,
                    detail::SpellTarget::self, detail::SpellTarget::enemy
                })
            offer_spells(commands, a, other, distance(a.source.cell, other.source.cell), scope,
                         true);
    offer_spells(commands, a, a, 0, detail::SpellTarget::area, true);
    if (!a.light_extra && !a.light_origins.empty())
        for (const auto &item : items_)
            if (item.holder == id && light_eligible(a, item.id))
            {
                const auto *weapon = detail::weapon(item.definition);
                for (const auto &other : actors_)
                    if (other.source.side != a.source.side && !other.dead && other.hp > 0 &&
                            line_of_sight(a.source.cell, other.source.cell))
                    {
                        const auto offer = [&](const char *light, const char *nick)
                        {
                            if (a.bonus)
                            {
                                add(id, light, "Light extra attack", other.source.id);
                                commands.back().item = item.id;
                            }
                            if (nick_active_ && a.nick_origin && a.nick_origin != item.id &&
                                    nick_weapon(a, item.id))
                            {
                                add(id, nick, "Nick attack", other.source.id);
                                commands.back().item = item.id;
                            }
                        };
                        const int feet = distance(a.source.cell, other.source.cell);
                        if (!item.stowed && !weapon->ranged && feet <= weapon->reach)
                            offer("light_melee", "nick_melee");
                        if (!item.stowed && weapon->ranged && feet <= weapon->long_range)
                            offer("light_ranged", "nick_ranged");
                        if (weapon->thrown && feet <= weapon->long_range)
                            offer("light_throw", "nick_throw");
                    }
            }
    if (a.actions.available())
    {
        offer_spells(commands, a, a, 0, detail::SpellTarget::area, false);
        // Armor of Shadows and Fiendish Vigor: Mage Armor and False Life on the
        // Warlock, cast without a slot.
        if (a.actions.available(true) && !rage_of(a))
        {
            if (d.armor_of_shadows && d.mage_armor_ac &&
                    !detail::has_effect(a.effects, detail::EffectKind::mage_armor))
                add(id, "armor_of_shadows", "Armor of Shadows (Mage Armor)", id);
            if (d.fiendish_vigor)
                add(id, "fiendish_vigor", "Fiendish Vigor (False Life)", id);
        }
        // Land's Aid: a Magic action spending a use of Wild Shape, aimed at a point.
        if (d.lands_aid && a.channel_divinity > 0 && a.actions.available(true))
            commands.push_back({revision_, id, 0, "lands_aid", "Land's Aid", Cell{}, 0, true});
        // Dragon's Breath: its holder exhales the chosen cone as an Action.
        for (const auto &effect : a.effects.active)
            if (effect.kind == detail::EffectKind::dragons_breath)
                for (const auto type : detail::dragon_types)
                    if (detail::damage_type(type) == detail::DamageType(effect.dc))
                        commands.push_back({revision_, id, 0,
                                            "dragons_breath_exhale_" + std::string(type),
                                            "Exhale (Dragon's Breath)", Cell{}, 0, true});
        add(id, "dash", "Dash");
        add(id, "dodge", "Dodge");
        add(id, "disengage", "Disengage");
        for (const auto &other : actors_)
        {
            if (other.dead || !line_of_sight(a.source.cell, other.source.cell))
                continue;
            const int feet = distance(a.source.cell, other.source.cell);
            if (!a.source.character_profile.empty() && other.hp == 0 && !other.stable && feet <= 5)
                add(id, "stabilize", "Stabilize", other.source.id);
            offer_spells(commands, a, other, feet, detail::SpellTarget::any_creature, false);
            if (other.source.side != a.source.side && other.hp > 0)
            {
                if (physical_inventory_)
                    for (const auto &item : items_)
                        if (item.holder == id)
                            if (const auto *w = detail::weapon(item.definition);
                                    w && w->thrown && feet <= w->long_range)
                            {
                                add(id, "throw", "Throw", other.source.id);
                                commands.back().item = item.id;
                            }
                if (feet <= d.reach)
                    add(id, "melee",
                        a.source.definition.starts_with("slums-kobold") ? "Dagger attack"
                        : "Melee attack",
                        other.source.id);
                // Flame Blade: a melee spell attack with the blade as an Action.
                if (feet <= 5 && a.actions.available(true) &&
                        detail::has_effect(a.effects, detail::EffectKind::flame_blade))
                    add(id, "flame_blade_strike", "Flame Blade attack", other.source.id);
                // Produce Flame: the flame in hand is hurled as an Action.
                if (feet <= 60 && a.actions.available(true) &&
                        detail::has_effect(a.effects, detail::EffectKind::produce_flame))
                    add(id, "hurl_flame", "Hurl flame (Produce Flame)", other.source.id);
                // Reckless Attack: chosen with the turn's first attack roll, which
                // through level four is the Attack action's one attack.
                if (feet <= d.reach && d.reckless && strength_attack(a, false))
                    add(id, "reckless", "Reckless attack", other.source.id);
                // True Strike: the cantrip's attack with the melee weapon in hand.
                if (feet <= d.reach && d.melee.count && !d.ranged_weapon &&
                        detail::knows_spell(d.spells, "true_strike") && a.actions.available(true))
                {
                    add(id, "true_strike", "True Strike", other.source.id);
                    add(id, "true_strike_radiant", "True Strike (Radiant)", other.source.id);
                }
                if (d.range > 0 && feet <= d.long_range)
                    add(id, "ranged", "Ranged attack", other.source.id);
                offer_spells(commands, a, other, feet, detail::SpellTarget::enemy, false);
            }
            else
                for (const auto scope :
                        {
                            detail::SpellTarget::wounded_ally, detail::SpellTarget::ally,
                            detail::SpellTarget::self, detail::SpellTarget::dying_ally
                        })
                    offer_spells(commands, a, other, feet, scope, false);
        }
    }
    for (const auto cell : movement_reach(id))
        add(id, "move", "Move", 0, cell);
    auto offered = filtered();
    obey_command(offered, a);
    flee_turning(offered, a);
    hold_still(offered, a);
    return offered;
}

void Session::obey_command(std::vector<Command> &commands, const Actor &a) const
{
    const auto *command = detail::command_effect(a.effects);
    if (!command)
        return;
    const auto caster = std::find_if(actors_.begin(), actors_.end(), [&](const auto & other)
    {
        return command->source_scope == scope_ && other.source.id == command->source_actor;
    });
    // With no caster left there is nothing to approach or flee.
    if (caster == actors_.end() || caster->dead)
        return;
    const bool approach = command->dc == int(detail::CommandOption::approach);
    const int start = distance(a.source.cell, caster->source.cell);
    // Approach ends the turn within 5 feet of the caster.
    const Command *best = nullptr;
    int best_feet = start;
    if (!approach || start > 5)
        for (const auto &offer : commands)
            if (offer.verb == "move")
            {
                const int feet = distance(offer.destination, caster->source.cell);
                if (approach ? feet < best_feet : feet > best_feet)
                {
                    best = &offer;
                    best_feet = feet;
                }
            }
    const auto offered = [&](std::string_view verb)
    {
        return std::find_if(commands.begin(), commands.end(), [&](const auto & offer)
        {
            return offer.verb == verb;
        });
    };
    // Flee uses the fastest means, so it Dashes once its movement runs out.
    auto kept = best                                                      ? *best
                : !approach && offered("dash") != commands.end() ? *offered("dash")
                : *offered("end");
    commands = {std::move(kept)};
}

std::vector<Cell> Session::movement_reach(EntityId id) const
{
    std::vector<Cell> cells;
    if (outcome_ != Outcome::ongoing || (!champion_move_ && pending()) || temporary_offer_ ||
            check_choice_ || graze_ || (!champion_move_ && effect_waiting()) ||
            (champion_move_ && id != champion_move_->actor))
        return cells;
    const auto actor = std::find_if(actors_.begin(), actors_.end(),
                                    [&](const auto & a)
    {
        return a.source.id == id;
    });
    if (actor == actors_.end() || !conscious(*actor))
        return cells;
    const auto budget = champion_move_ ? champion_move_->remaining : movement_left(*actor);
    if (budget <= 0)
        return cells;
    const auto reachable = movement_grid(*actor).reachable(budget);
    for (int y = 0; y < board_.height; ++y)
        for (int x = 0; x < board_.width; ++x)
            if (reachable.cost_to({x, y}))
                cells.push_back({x, y});
    return cells;
}

int Session::resolved_damage(Actor &target, detail::DamageType type, int amount)
{
    // Resistance (the cantrip): 1d4 less of its type, once per turn, before
    // resistances halve the rest.
    const auto ward = std::find_if(target.effects.active.begin(), target.effects.active.end(),
                                   [&](const auto & e)
    {
        return e.kind == detail::EffectKind::resistance && e.dc == int(type);
    });
    if (ward != target.effects.active.end() && !target.resistance_used && amount > 0)
    {
        target.resistance_used = true;
        const int reduced = std::min(amount, roll(4));
        amount -= reduced;
        log(target.source.name + "'s Resistance reduces the damage by " + std::to_string(reduced) +
            ".",
        {
            "{name}'s Resistance reduces the damage by {amount}.",
            {{"name", target.source.name}, {"amount", std::to_string(reduced)}}
        });
    }
    const std::array parts{detail::DamagePart{type, amount}};
    const auto result = detail::resolve_damage(parts, affinities(target));
    if (result.total != amount)
    {
        const auto name = std::string(detail::damage_name(type));
        log(target.source.name + ": " + name + " damage " + std::to_string(amount) + " -> " +
            std::to_string(result.total) + ".",
        {
            "{name}: {type} damage {before} -> {after}.",
            {   {"name", target.source.name},
                {"type", name, true},
                {"before", std::to_string(amount)},
                {"after", std::to_string(result.total)}
            }
        });
    }
    return result.total;
}

void Session::damage(Actor &target, int amount, bool critical)
{
    if (!amount || target.dead)
        return;
    const bool standing = target.hp > 0;
    detail::damage_life(target, amount, max_hp(target), critical, target.source.side == 1);
    if (standing && target.hp == 0)
        bless_fiends(target);
    // Warding Bond: the caster takes the same damage; the bond ends when the
    // caster drops to 0 Hit Points or is more than 60 feet away.
    for (const auto bond : bonds_on(target))
    {
        auto &caster = actor(bond);
        log(caster.source.name + " shares " + std::to_string(amount) +
            " damage through Warding Bond.",
        {
            "{name} shares {damage} damage through Warding Bond.",
            {{"name", caster.source.name}, {"damage", std::to_string(amount)}}
        });
        damage(caster, amount, false);
    }
    if (target.hp == 0)
        for (auto &other : actors_)
            std::erase_if(other.effects.active, [&](const auto & e)
        {
            return e.kind == detail::EffectKind::warding_bond && e.source_scope == scope_ &&
                   e.source_actor == target.source.id;
        });
    // Damage tests Concentration; dropping to 0 Hit Points ends it.
    if (target.concentration.active())
    {
        const bool rolls = target.hp > 0 && !target.dead;
        auto modifiers = detail::saving_modifiers(detail::Ability::constitution,
                         def(target).str_dex_disadvantage, target.dodge);
        modifiers.advantage |= detail::has_effect(target.effects, detail::EffectKind::extended) ||
                               def(target).eldritch_mind;
        const auto result = target.concentration.damage(
                                amount, def(target).saves[2] + (rolls ? blessing_die(target) : 0),
                                modifiers, target.hp == 0 || target.dead, rng_);
        if (result.save)
            log_save(target, *result.save);
        if (result.ended)
            drop_concentration_effects(target);
    }
    if (target.hp == 0)
    {
        target.effects.prone = true;
        end_wild_shape(target);
    }
    // Damage ends Turn Undead, Sleep and Charm Person on the creature.
    // Adaptation: any damage ends the charm, not only the charmer's side's.
    std::erase_if(target.effects.active, [](const auto & e)
    {
        return e.kind == detail::EffectKind::turned || e.kind == detail::EffectKind::drowsy ||
               e.kind == detail::EffectKind::asleep || e.kind == detail::EffectKind::charmed;
    });
    // Hideous Laughter: damage calls for a new Wisdom save, with Advantage.
    for (std::size_t n = 0; n < target.effects.active.size() && target.hp > 0; ++n)
        if (target.effects.active[n].kind == detail::EffectKind::laughing &&
                saving_throw_succeeds(target, detail::Ability::wisdom, target.effects.active[n].dc,
                                      true))
        {
            target.effects.active.erase(target.effects.active.begin() + std::ptrdiff_t(n));
            log(target.source.name + " stops laughing.",
            {"{name} stops laughing.", {{"name", target.source.name}}});
            break;
        }
    // Sacred Weapon ends when its wielder is Incapacitated.
    if (target.hp == 0)
        std::erase_if(target.effects.active, [](const auto & e)
    {
        return e.kind == detail::EffectKind::sacred_weapon;
    });
    if (target.hp == 0 && !target.dead && !target.stable)
        target.recovery.death_save_in_ms = next_turn_ms(target);
    if (target.hp == 0)
    {
        target.dodge = false;
        if (!target.dead && shares_occupied_space(target))
            target.involuntary_overlap = true;
        log(target.source.name + (target.dead ? " is defeated." : " falls unconscious."),
        {
            target.dead ? "{name} is defeated." : "{name} falls unconscious.",
            {{"name", target.source.name}}
        });
    }
    clear_departed_overlaps();
}

int Session::heal(Actor &target, int amount)
{
    if (target.hp == 0 && shares_occupied_space(target))
        target.involuntary_overlap = true;
    const bool was_unconscious = target.hp == 0;
    const int restored =
        detail::heal_life(target, amount, max_hp(target), !detail::healing_blocked(target.effects));
    if (was_unconscious && restored)
        target.effects.prone = true;
    log(target.source.name + " recovers " + std::to_string(restored) + " HP.",
    {
        "{name} recovers {hp} HP.",
        {{"name", target.source.name}, {"hp", std::to_string(restored)}}
    });
    return restored;
}

void Session::resolve_smite(Actor &a, std::string_view verb)
{
    if (verb == "ensnaring_strike")
    {
        resolve_ensnaring_strike(a);
        return;
    }
    const bool searing = verb == "searing_smite";
    if (sanctuary_stops(a, actor(a.smite_target)))
    {
        a.smite_target = 0;
        a.bonus = false;
        return;
    }
    const auto &spell = *detail::find_spell(searing ? "searing_smite" : "divine_smite");
    auto &target = actor(a.smite_target);
    a.smite_target = 0;
    a.bonus = false;
    if (verb == "divine_smite_free")
        --a.free_casts;
    else
    {
        --a.slots;
        a.spent_slot = true;
    }
    auto rolled = spell.dice;
    // Divine Smite deals 1d8 more to a Fiend or an Undead.
    const auto &type = def(target).creature_type;
    if (!searing && (type == "fiend" || type == "undead"))
        ++rolled.count;
    // The damage is part of the attack, so a critical hit doubles its dice.
    if (a.smite_critical)
        rolled.count *= 2;
    const auto damage_type = std::string(detail::damage_name(spell.damage));
    const int amount = resolved_damage(target, spell.damage, dice(rolled));
    log(a.source.name + " casts " + std::string(spell.label) + ": " + target.source.name +
        " takes " + std::to_string(amount) + " " + damage_type + " damage.",
    {
        "{name} casts {spell}: {target} takes {damage} {type} damage.",
        {   {"name", a.source.name},
            {"spell", std::string(spell.label), true},
            {"target", target.source.name},
            {"damage", std::to_string(amount)},
            {"type", damage_type, true}
        }
    });
    damage(target, amount, false);
    if (searing && target.hp > 0 && detail::can_apply(target.effects))
        detail::apply_searing_smite(target.effects, scope_, a.source.id, a.source.name,
                                    spell_dc(a));
}

std::vector<Cell> Session::area_cells(const detail::SpellDef &spell, std::string_view verb,
                                     Cell origin, Cell center) const
{
    std::vector<Cell> cells;
    // The aimed direction from the caster; aiming at the caster faces east.
    const int dx = center.x - origin.x, dy = center.y - origin.y;
    const double ax = dx || dy ? dx : 1, ay = dy;
    // A cone: squares within its length and about 30 degrees of the aim.
    if (spell.cone)
    {
        for (int y = 0; y < board_.height; ++y)
            for (int x = 0; x < board_.width; ++x)
            {
                const double vx = x - origin.x, vy = y - origin.y;
                if ((!vx && !vy) || distance(origin, Cell{x, y}) > spell.cone)
                    continue;
                const double cosine = (vx * ax + vy * ay) /
                                      (std::sqrt(vx * vx + vy * vy) * std::sqrt(ax * ax + ay * ay));
                if (cosine >= 0.866)
                    cells.push_back({x, y});
            }
        return cells;
    }
    // A line two squares wide from the caster toward the aim.
    if (spell.line)
    {
        const double length = std::sqrt(ax * ax + ay * ay);
        for (int y = 0; y < board_.height; ++y)
            for (int x = 0; x < board_.width; ++x)
            {
                const double vx = x - origin.x, vy = y - origin.y;
                const double along = (vx * ax + vy * ay) / length;
                const double across = (vx * ay - vy * ax) / length;
                if (along > 0 && along * 5 <= spell.line && across > -0.5 && across <= 1.0)
                    cells.push_back({x, y});
            }
        return cells;
    }
    // A cube with a face against the caster, on the aimed side.
    if (spell.cube)
    {
        const int side = spell.cube / 5;
        const auto step = [&](double along, double across)
        {
            return std::abs(along) * 2 < std::abs(across) ? 0 : along > 0 ? 1 : along < 0 ? -1 : 0;
        };
        const int sx = step(ax, ay), sy = step(ay, ax);
        const auto first = [&](int start, int sign)
        {
            return sign > 0 ? start + 1 : sign < 0 ? start - side : start - (side - 1) / 2;
        };
        for (int y = first(origin.y, sy); y < first(origin.y, sy) + side; ++y)
            for (int x = first(origin.x, sx); x < first(origin.x, sx) + side; ++x)
                if (board_.contains(Cell{x, y}))
                    cells.push_back({x, y});
        return cells;
    }
    // A sphere: every square within its radius of the aimed one.
    if (spell.radius)
    {
        const int radius = spell.radius + (verb.ends_with("_2") ? spell.upcast.extra_radius : 0);
        for (int y = 0; y < board_.height; ++y)
            for (int x = 0; x < board_.width; ++x)
                if (distance(center, Cell{x, y}) <= radius)
                    cells.push_back({x, y});
        return cells;
    }
    // A square of `area` feet around the aimed cell, clipped to the board.
    const int side = spell.area / 5, first = (side - 1) / 2;
    for (int y = center.y - first; y < center.y - first + side; ++y)
        for (int x = center.x - first; x < center.x - first + side; ++x)
            if (board_.contains(Cell{x, y}))
                cells.push_back({x, y});
    return cells;
}

bool Session::teleport_open(const Actor &caster, Cell cell) const
{
    // Misty Step: an unoccupied space the caster can see.
    return cell != caster.source.cell && board_.at(cell) != 1 &&
           line_of_sight(caster.source.cell, cell) && !obscured(caster, cell) &&
           std::none_of(actors_.begin(), actors_.end(), [&](const auto & other)
    {
        return !other.dead && other.source.cell == cell;
    });
}

Cell Session::default_area_center(const Actor &caster, const detail::SpellDef &spell) const
{
    // A teleport's preview starts on the caster; it must be moved before casting.
    if (spell.rider == detail::Rider::misty_step)
        return caster.source.cell;
    // Flaming Sphere starts in the open square beside the nearest enemy.
    if (spell.rider == detail::Rider::flaming_sphere)
    {
        const Actor *nearest = nullptr; // Borrowed from actors_.
        for (const auto &other : actors_)
            if (other.source.side != caster.source.side && other.hp > 0 && !other.dead &&
                    (!nearest || distance(caster.source.cell, other.source.cell) <
                     distance(caster.source.cell, nearest->source.cell)))
                nearest = &other;
        return nearest ? spectral_cell(*nearest, caster.source.cell) : caster.source.cell;
    }
    // The preview starts on the nearest living enemy in range, else the caster.
    Cell best = caster.source.cell;
    int best_feet = spell.range + 1;
    for (const auto &other : actors_)
    {
        const int feet = distance(caster.source.cell, other.source.cell);
        if (other.source.side != caster.source.side && other.hp > 0 && feet < best_feet)
        {
            best = other.source.cell;
            best_feet = feet;
        }
    }
    return best;
}

void Session::aim_area(const Command &command)
{
    if (command.verb == "spell_cancel")
        area_.reset();
    else if (command.verb == "area_cast")
        cast_area();
    else
        area_->center = command.destination;
}

void Session::potent_cantrip(Actor &target, const detail::SpellDef &spell,
                             detail::DamageDice rolled)
{
    // A missed cantrip still deals half its damage, without its other effects.
    const int amount = resolved_damage(target, spell.damage, dice(rolled) / 2);
    log("Potent Cantrip: " + target.source.name + " takes " + std::to_string(amount) + " damage.",
    {
        "Potent Cantrip: {name} takes {damage} damage.",
        {{"name", target.source.name}, {"damage", std::to_string(amount)}}
    });
    damage(target, amount, false);
}

std::vector<EntityId> Session::sculpted(const Actor &caster, const detail::SpellDef &spell,
                                        unsigned slot_level, const std::vector<Cell> &cells) const
{
    // Sculpt Spells: up to 1 + the spell's level allies the Evoker can see in
    // an Evocation area are spared, chosen automatically.
    std::vector<EntityId> spared;
    if (!def(caster).evoker || !spell.evocation)
        return spared;
    for (const auto &other : actors_)
        if (spared.size() < 1 + slot_level && other.source.side == caster.source.side &&
                other.source.id != caster.source.id && !other.dead && can_see(caster, other) &&
                std::find(cells.begin(), cells.end(), other.source.cell) != cells.end())
            spared.push_back(other.source.id);
    return spared;
}

int Session::area_dc(const Actor &caster, const detail::SpellDef &spell) const
{
    // A breath given by Dragon's Breath uses the spell caster's save DC.
    if (spell.id == "dragons_breath_exhale")
        for (const auto &effect : caster.effects.active)
            if (effect.kind == detail::EffectKind::dragons_breath)
                return spell_dc(actor(effect.source_actor));
    return spell_dc(caster);
}

void Session::damage_area(Actor &caster, const detail::SpellDef &spell, std::string_view verb,
                          const std::vector<Cell> &cells, int dc)
{
    // Each creature in the area, the caster aside, saves for half.
    const auto spared = sculpted(caster, spell, verb.ends_with("_2") ? 2 : spell.level, cells);
    for (const auto id : spared)
        log(actor(id).source.name + " is spared by Sculpt Spells.",
        {"{name} is spared by Sculpt Spells.", {{"name", actor(id).source.name}}});
    auto rolled = spell.dice;
    rolled.count += static_cast<int>(verb.ends_with("_2") ? spell.upcast.extra_dice : 0u);
    const int total = spell_dice(caster, rolled);
    const auto damage_type = cast_damage_type(detail::dragon_type(verb).value_or(spell.damage));
    const auto type = std::string(detail::damage_name(damage_type));
    // Careful Spell: the chosen allies succeed and so take no half damage.
    const auto careful = careful_allies(caster, cells);
    for (auto &other : actors_)
    {
        if (other.dead || other.source.id == caster.source.id ||
                std::find(cells.begin(), cells.end(), other.source.cell) == cells.end() ||
                std::find(spared.begin(), spared.end(), other.source.id) != spared.end() ||
                std::find(careful.begin(), careful.end(), other.source.id) != careful.end())
            continue;
        const bool saved = saving_throw_succeeds(other, spell.save, dc);
        // Potent Cantrip: an Evoker's saved-against cantrip still deals half.
        const bool halved = spell.half_on_success || (def(caster).evoker && !spell.level);
        if (saved && !halved)
            continue;
        const int amount = resolved_damage(other, damage_type, saved ? total / 2 : total);
        log(other.source.name + " takes " + std::to_string(amount) + " " + type + " damage.",
        {
            "{name} takes {damage} {type} damage.",
            {{"name", other.source.name}, {"damage", std::to_string(amount)}, {"type", type, true}}
        });
        damage(other, amount);
        if (!saved && spell.rider == detail::Rider::thunderwave && !other.dead)
            push_away(caster, other, 2);
    }
}

void Session::condition_area(Actor &caster, const detail::SpellDef &spell,
                             const std::vector<Cell> &cells)
{
    // Sleep chooses the enemies in its sphere; Color Spray's cone takes every
    // creature in it.
    const bool sleep = spell.rider == detail::Rider::sleep;
    const int dc = spell_dc(caster);
    const auto index = static_cast<std::size_t>(&caster - actors_.data());
    const unsigned slot = turn_end_ms(index) - (index ? turn_end_ms(index - 1) : 0);
    const auto careful = careful_allies(caster, cells);
    for (auto &other : actors_)
    {
        if (other.dead || other.hp == 0 || other.source.id == caster.source.id ||
                std::find(cells.begin(), cells.end(), other.source.cell) == cells.end() ||
                std::find(careful.begin(), careful.end(), other.source.id) != careful.end() ||
                (sleep && other.source.side == caster.source.side) || !detail::can_apply(other.effects))
            continue;
        if (sleep && (def(other).sleepless || def(other).creature_type == "undead" ||
                      def(other).creature_type == "construct"))
        {
            log(other.source.name + " does not sleep.",
            {"{name} does not sleep.", {{"name", other.source.name}}});
            continue;
        }
        if (saving_throw_succeeds(other, spell.save, dc))
            continue;
        if (spell.rider == detail::Rider::faerie_fire)
        {
            detail::apply_spell_benefit(other.effects, scope_, caster.source.id, caster.source.name,
                                        detail::EffectKind::outlined, 0);
            log(other.source.name + " is outlined.", {"{name} is outlined.", {{"name", other.source.name}}});
        }
        else if (sleep)
        {
            detail::apply_repeating_condition(other.effects, detail::EffectKind::drowsy, scope_,
                                              caster.source.id, caster.source.name, dc,
                                              next_save_ms(other.source.id));
            log(other.source.name + " grows drowsy.",
            {"{name} grows drowsy.", {{"name", other.source.name}}});
        }
        else
        {
            detail::apply_poisoned(other.effects, scope_, caster.source.id, caster.source.name,
                                   next_turn_ms(caster) + slot, detail::EffectKind::dazzled);
            log(other.source.name + " is Blinded.",
            {"{name} is Blinded.", {{"name", other.source.name}}});
        }
    }
}

void Session::blow(const Zone &line, Actor &creature)
{
    // Gust of Wind: a Strength save or 15 feet away from the caster.
    if (creature.dead || creature.hp == 0 || creature.source.id == line.caster)
        return;
    const auto &caster = actor(line.caster);
    if (!saving_throw_succeeds(creature, detail::Ability::strength, spell_dc(caster)))
        push_away(caster, creature, 3);
}

void Session::burn_beside_spheres(Actor &creature)
{
    // Copies: the burn can end a caster's Concentration and with it the sphere.
    const auto zones = zones_;
    for (const auto &zone : zones)
        if (zone.kind == ZoneKind::flaming_sphere &&
                distance(zone.cells.front(), creature.source.cell) <= 5)
            sphere_burns(actor(zone.caster), creature);
}

void Session::sphere_burns(const Actor &caster, Actor &creature)
{
    if (creature.dead || creature.hp == 0)
        return;
    const auto &spell = *detail::find_spell("flaming_sphere");
    const bool saved = saving_throw_succeeds(creature, spell.save, spell_dc(caster));
    const int rolled = dice({spell.dice.count, spell.dice.sides, 0});
    const int amount = resolved_damage(creature, spell.damage, saved ? rolled / 2 : rolled);
    log(creature.source.name + " takes " + std::to_string(amount) +
        " Fire damage from the Flaming Sphere.",
    {
        "{name} takes {damage} Fire damage from the Flaming Sphere.",
        {{"name", creature.source.name}, {"damage", std::to_string(amount)}}
    });
    damage(creature, amount);
}

void Session::refresh_form(Actor &a)
{
    const auto shape = std::find_if(a.effects.active.begin(), a.effects.active.end(),
                                    [](const auto & e)
    {
        return e.kind == detail::EffectKind::wild_shape;
    });
    if (shape == a.effects.active.end())
        a.form.reset();
    else
        a.form = shaped(a.definition, detail::beast_forms.at(std::size_t(shape->dc - 1)));
}

void Session::end_wild_shape(Actor &a)
{
    if (!a.form)
        return;
    std::erase_if(a.effects.active, [](const auto & e)
    {
        return e.kind == detail::EffectKind::wild_shape;
    });
    a.form.reset();
    log(a.source.name + " leaves Wild Shape.", {"{name} leaves Wild Shape.", {{"name", a.source.name}}});
}

void Session::lands_aid(Actor &druid, Cell center)
{
    druid.nick_origin = 0;
    (void)druid.actions.spend(true);
    --druid.channel_divinity;
    log(druid.source.name + " uses Land's Aid.", {"{name} uses Land's Aid.", {{"name", druid.source.name}}});
    const auto &aid = *detail::find_spell("lands_aid");
    const auto cells = area_cells(aid, aid.id, druid.source.cell, center);
    const auto inside = [&](const Actor & other)
    {
        return !other.dead && std::find(cells.begin(), cells.end(), other.source.cell) != cells.end();
    };
    // The Druid chooses its enemies for the thorns and its most wounded ally
    // for the flowers.
    const int dc = spell_dc(druid), rolled = dice(aid.dice);
    for (auto &other : actors_)
    {
        if (!inside(other) || other.source.side == druid.source.side || other.hp == 0)
            continue;
        const bool saved = saving_throw_succeeds(other, aid.save, dc);
        const int amount = resolved_damage(other, aid.damage, saved ? rolled / 2 : rolled);
        log(other.source.name + " takes " + std::to_string(amount) + " Necrotic damage from the thorns.",
        {
            "{name} takes {damage} Necrotic damage from the thorns.",
            {{"name", other.source.name}, {"damage", std::to_string(amount)}}
        });
        damage(other, amount);
    }
    Actor *wounded = nullptr; // Borrowed from actors_.
    for (auto &other : actors_)
        if (inside(other) && other.source.side == druid.source.side && other.hp < max_hp(other) &&
                (!wounded || max_hp(other) - other.hp > max_hp(*wounded) - wounded->hp))
            wounded = &other;
    if (wounded)
        (void)heal(*wounded, dice(aid.dice));
}

bool Session::wears_metal(const Actor &creature) const
{
    if (creature.source.character_profile.empty())
        return detail::creature_wears_metal(creature.source.definition);
    const auto &keys = def(creature).equipment_keys;
    return std::any_of(keys.begin(), keys.end(), [](const auto & key)
    {
        const auto *worn = detail::armor(key);
        return worn && detail::metal_armor(*worn);
    });
}

void Session::heat_metal(const Actor &caster, Actor &creature)
{
    const auto &spell = *detail::find_spell("heat_metal");
    const int amount = resolved_damage(creature, spell.damage, dice(spell.dice));
    log(creature.source.name + " takes " + std::to_string(amount) + " Fire damage from the hot metal.",
    {
        "{name} takes {damage} Fire damage from the hot metal.",
        {{"name", creature.source.name}, {"damage", std::to_string(amount)}}
    });
    damage(creature, amount);
    if (creature.dead || creature.hp == 0 || !detail::can_apply(creature.effects) ||
            saving_throw_succeeds(creature, spell.save, spell_dc(caster)))
        return;
    detail::apply_poisoned(creature.effects, scope_, caster.source.id, caster.source.name,
                           next_turn_ms(caster), detail::EffectKind::scorched);
    log(creature.source.name + " has Disadvantage on attack rolls.",
    {"{name} has Disadvantage on attack rolls.", {{"name", creature.source.name}}});
}

void Session::move_moonbeam(Actor &caster, const Actor &onto)
{
    auto &beam = *std::find_if(zones_.begin(), zones_.end(), [&](const auto & zone)
    {
        return zone.kind == ZoneKind::moonbeam && zone.caster == caster.source.id;
    });
    const auto before = beam;
    beam.cells = beam_cells(onto.source.cell);
    const auto after = beam;
    log(caster.source.name + " moves the Moonbeam.",
    {"{name} moves the Moonbeam.", {{"name", caster.source.name}}});
    for (auto &other : actors_)
        if (in_zone(after, other.source.cell) && !in_zone(before, other.source.cell))
            moonbeam_burns(after, other);
}

std::vector<Cell> Session::beam_cells(Cell center) const
{
    // Moonbeam's 5-foot radius, its centre first so a moved beam keeps it.
    std::vector<Cell> cells{center};
    for (const auto cell : area_cells(*detail::find_spell("moonbeam"), "moonbeam", center, center))
        if (cell != center)
            cells.push_back(cell);
    return cells;
}

void Session::moonbeam_burns(const Zone &beam, Actor &creature)
{
    // A creature saves against a Moonbeam only once per turn.
    // The beam may have ended with its caster's Concentration mid-burn.
    const bool shining = std::any_of(zones_.begin(), zones_.end(), [&](const auto & zone)
    {
        return zone.kind == ZoneKind::moonbeam && zone.caster == beam.caster;
    });
    if (!shining || creature.dead || creature.hp == 0 ||
            detail::has_effect(creature.effects, detail::EffectKind::moonlit))
        return;
    const auto &caster = actor(beam.caster);
    const unsigned rest_of_turn = turn_end_ms(turn_) - (turn_ ? turn_end_ms(turn_ - 1) : 0);
    if (detail::can_apply(creature.effects))
        detail::apply_poisoned(creature.effects, scope_, caster.source.id, caster.source.name,
                               rest_of_turn, detail::EffectKind::moonlit);
    const auto &spell = *detail::find_spell("moonbeam");
    const bool saved = saving_throw_succeeds(creature, spell.save, spell_dc(caster));
    const int rolled = dice(spell.dice);
    const int amount = resolved_damage(creature, spell.damage, saved ? rolled / 2 : rolled);
    log(creature.source.name + " takes " + std::to_string(amount) + " Radiant damage from the Moonbeam.",
    {
        "{name} takes {damage} Radiant damage from the Moonbeam.",
        {{"name", creature.source.name}, {"damage", std::to_string(amount)}}
    });
    damage(creature, amount);
}

void Session::spikes_pierce(Actor &creature)
{
    const int amount = resolved_damage(creature, detail::DamageType::piercing, dice({2, 4, 0}));
    log(creature.source.name + " takes " + std::to_string(amount) + " Piercing damage from the spikes.",
    {
        "{name} takes {damage} Piercing damage from the spikes.",
        {{"name", creature.source.name}, {"damage", std::to_string(amount)}}
    });
    damage(creature, amount);
}

bool Session::in_zone(const Zone &zone, Cell cell)
{
    return std::find(zone.cells.begin(), zone.cells.end(), cell) != zone.cells.end();
}

bool Session::spring_zone(const Zone &zone, Actor &creature)
{
    // Grease knocks a creature Prone and Web Restrains it, each on a failed
    // Dexterity save against the caster's spell DC.
    const bool grease = zone.kind == ZoneKind::grease;
    if (creature.dead || creature.hp == 0 ||
            (grease ? creature.effects.prone
             : detail::has_effect(creature.effects, detail::EffectKind::webbed) ||
             !detail::can_apply(creature.effects)))
        return false;
    const auto &caster = actor(zone.caster);
    const int dc = spell_dc(caster);
    if (saving_throw_succeeds(creature, detail::Ability::dexterity, dc))
        return false;
    if (grease)
    {
        creature.effects.prone = true;
        log(creature.source.name + " slips and falls Prone.",
        {"{name} slips and falls Prone.", {{"name", creature.source.name}}});
        return true;
    }
    detail::apply_entangle(creature.effects, scope_, caster.source.id, caster.source.name, dc,
                           detail::EffectKind::webbed);
    log(creature.source.name + " is Restrained by the webs.",
    {"{name} is Restrained by the webs.", {{"name", creature.source.name}}});
    return true;
}

void Session::spring_zones(Actor &creature, ZoneKind kind)
{
    // Copies: a save can end a caster's Concentration and with it the zone.
    const auto zones = zones_;
    for (const auto &zone : zones)
        if (zone.kind == kind && in_zone(zone, creature.source.cell))
            (void)spring_zone(zone, creature);
}

void Session::ice_burst(Actor &caster, const Actor &target, bool upcast)
{
    // The target and each creature within 5 feet of it save against 2d6 Cold
    // (3d6 from a level-two slot).
    const int total = dice({upcast ? 3 : 2, 6, 0});
    const int dc = spell_dc(caster);
    const auto centre = target.source.cell;
    for (auto &other : actors_)
    {
        if (other.dead || distance(centre, other.source.cell) > 5)
            continue;
        if (saving_throw_succeeds(other, detail::Ability::dexterity, dc))
            continue;
        const int amount = resolved_damage(other, detail::DamageType::cold, total);
        log(other.source.name + " takes " + std::to_string(amount) + " Cold damage.",
        {
            "{name} takes {damage} {type} damage.",
            {{"name", other.source.name}, {"damage", std::to_string(amount)}, {"type", "Cold", true}}
        });
        damage(other, amount);
    }
}

void Session::push_away(const Actor &from, Actor &target, int squares)
{
    // Straight away from `from`, square by square, while the way is open.
    const int sx = (target.source.cell.x > from.source.cell.x) - (target.source.cell.x < from.source.cell.x);
    const int sy = (target.source.cell.y > from.source.cell.y) - (target.source.cell.y < from.source.cell.y);
    int moved = 0;
    for (; moved < squares && (sx || sy); ++moved)
    {
        const Cell next{target.source.cell.x + sx, target.source.cell.y + sy};
        if (!board_.contains(next) || board_.at(next) == 1 ||
                std::any_of(actors_.begin(), actors_.end(), [&](const auto & other)
        {
            return !other.dead && other.source.cell == next;
        }))
        break;
        target.source.cell = next;
    }
    if (moved)
        log(target.source.name + " is pushed " + std::to_string(5 * moved) + " feet.",
        {
            "{name} is pushed {feet} feet.",
            {{"name", target.source.name}, {"feet", std::to_string(5 * moved)}}
        });
}

void Session::cast_area()
{
    const auto aimed = *area_;
    area_.reset();
    auto &a = actor(aimed.caster);
    if (aimed.verb == "lands_aid")
    {
        lands_aid(a, aimed.center);
        return;
    }
    end_sanctuary(a);
    const InvisibilityEnds ends{*this, a};
    const auto &spell = *detail::find_spell(aimed.verb);
    // Quickened Spell casts an Action spell as a Bonus Action.
    const bool quickened = !spell.bonus_action && readies(a, Metamagic::quickened, spell);
    take_metamagic(a, spell);
    if (spell.bonus_action || quickened)
        a.bonus = false;
    else
    {
        a.nick_origin = 0;
        (void)a.actions.spend(true);
    }
    if (spell.level)
    {
        if (aimed.verb.ends_with("_2") || spell.level >= 2)
            --a.slots2;
        else
            --a.slots;
        a.spent_slot = true;
    }
    if (spell.id == "dragons_breath_exhale")
        log(a.source.name + " exhales Dragon's Breath.",
        {"{name} exhales Dragon's Breath.", {{"name", a.source.name}}});
    else
        log(a.source.name + " casts " + std::string(spell.label) + ".",
        {"{name} casts {spell}.", {{"name", a.source.name}, {"spell", std::string(spell.label), true}}});
    // Heightened Spell takes the first enemy in the area.
    if (casting_with(Metamagic::heightened))
        for (const auto cell : area_cells(spell, aimed.verb, a.source.cell, aimed.center))
            for (const auto &other : actors_)
                if (!heightened_target_ && !other.dead && other.source.cell == cell &&
                        other.source.side != a.source.side)
                    heightened_target_ = other.source.id;
    if (spell.rider == detail::Rider::flaming_sphere)
    {
        begin_concentration(a, spell);
        zones_.push_back({a.source.id, ZoneKind::flaming_sphere, {aimed.center}});
        return;
    }
    if (spell.rider == detail::Rider::moonbeam)
    {
        begin_concentration(a, spell);
        zones_.push_back({a.source.id, ZoneKind::moonbeam, beam_cells(aimed.center)});
        const auto beam = zones_.back();
        for (auto &other : actors_)
            if (in_zone(beam, other.source.cell))
                moonbeam_burns(beam, other);
        return;
    }
    if (spell.rider == detail::Rider::gust_of_wind)
    {
        begin_concentration(a, spell);
        const auto cells = area_cells(spell, aimed.verb, a.source.cell, aimed.center);
        zones_.push_back({a.source.id, ZoneKind::gust, cells});
        const auto line = zones_.back();
        for (auto &other : actors_)
            if (other.source.id != a.source.id && in_zone(line, other.source.cell))
                blow(line, other);
        return;
    }
    if (spell.rider == detail::Rider::misty_step)
    {
        // Teleporting provokes no Opportunity Attacks.
        a.source.cell = aimed.center;
        clear_departed_overlaps();
        log(a.source.name + " steps through the mist.",
        {"{name} steps through the mist.", {{"name", a.source.name}}});
        return;
    }
    const auto cells = area_cells(spell, aimed.verb, a.source.cell, aimed.center);
    if (spell.pattern == detail::SpellPattern::save_damage)
    {
        damage_area(a, spell, aimed.verb, cells, area_dc(a, spell));
        return;
    }
    if (spell.rider == detail::Rider::sleep || spell.rider == detail::Rider::color_spray ||
            spell.rider == detail::Rider::faerie_fire)
    {
        if (spell.concentration)
            begin_concentration(a, spell);
        condition_area(a, spell, cells);
        return;
    }
    if (spell.concentration)
        begin_concentration(a, spell);
    const auto kind = spell.rider == detail::Rider::fog_cloud ? ZoneKind::fog
                      : spell.rider == detail::Rider::darkness ? ZoneKind::darkness
                      : spell.rider == detail::Rider::silence ? ZoneKind::silence
                      : spell.rider == detail::Rider::grease  ? ZoneKind::grease
                      : spell.rider == detail::Rider::web     ? ZoneKind::web
                      : spell.rider == detail::Rider::spike_growth ? ZoneKind::spikes
                      : ZoneKind::plants;
    zones_.push_back({a.source.id, kind, cells, kind == ZoneKind::grease ? elapsed_ms_ + 60000 : 0});
    if (kind == ZoneKind::grease || kind == ZoneKind::web)
    {
        const auto zone = zones_.back();
        for (auto &other : actors_)
            if (other.source.id != a.source.id && in_zone(zone, other.source.cell))
                (void)spring_zone(zone, other);
        return;
    }
    if (kind != ZoneKind::plants)
        return;
    const int dc = spell_dc(a);
    for (auto &other : actors_)
    {
        if (other.dead || other.source.id == a.source.id ||
                std::find(cells.begin(), cells.end(), other.source.cell) == cells.end() ||
                !detail::can_apply(other.effects) ||
                saving_throw_succeeds(other, spell.save, dc))
            continue;
        detail::apply_entangle(other.effects, scope_, a.source.id, a.source.name, dc);
        log(other.source.name + " is Restrained.",
        {"{name} is Restrained.", {{"name", other.source.name}}});
    }
}

int Session::disciple_of_life(const Actor &caster, unsigned slot_level) const
{
    // Disciple of Life: healing from a spell slot restores 2 + the slot's level more.
    return def(caster).life_domain ? 2 + int(slot_level) : 0;
}

void Session::preserve_life(Actor &cleric)
{
    // Five times the Cleric level, divided among Bloodied allies within 30
    // feet, none raised above half its Hit Point maximum. The most hurt first.
    log(cleric.source.name + " uses Preserve Life.",
    {"{name} uses Preserve Life.", {{"name", cleric.source.name}}});
    int pool = 5 * def(cleric).level;
    std::vector<Actor *> bloodied;
    for (auto &other : actors_)
        if (other.source.side == cleric.source.side && !other.dead &&
                other.hp * 2 <= max_hp(other) &&
                distance(cleric.source.cell, other.source.cell) <= 30 &&
                def(other).creature_type != "undead" && def(other).creature_type != "construct")
            bloodied.push_back(&other);
    std::sort(bloodied.begin(), bloodied.end(), [&](const Actor * x, const Actor * y)
    {
        return x->hp * max_hp(*y) < y->hp * max_hp(*x);
    });
    for (auto *other : bloodied)
    {
        const int room = max_hp(*other) / 2 - other->hp;
        if (pool <= 0 || room <= 0)
            continue;
        pool -= heal(*other, std::min(pool, room));
    }
}

void Session::divine_spark(Actor &cleric, Actor &target)
{
    // 1d8 + Wisdom modifier: restored to an ally, or Radiant (Necrotic if the
    // target resists Radiant more) to an enemy, halved on a Constitution save.
    const int total = std::max(0, roll(8) + def(cleric).casting - 2);
    log(cleric.source.name + " uses Divine Spark on " + target.source.name + ".",
    {
        "{name} uses Divine Spark on {target}.",
        {{"name", cleric.source.name}, {"target", target.source.name}}
    });
    if (target.source.side == cleric.source.side)
    {
        heal(target, total);
        return;
    }
    const auto taken = [&](detail::DamageType type)
    {
        const std::array parts{detail::DamagePart{type, 100}};
        return detail::resolve_damage(parts, affinities(target)).total;
    };
    const auto type = taken(detail::DamageType::necrotic) > taken(detail::DamageType::radiant)
                      ? detail::DamageType::necrotic
                      : detail::DamageType::radiant;
    const bool saved = saving_throw_succeeds(target, detail::Ability::constitution,
                       spell_dc(cleric));
    const int amount = resolved_damage(target, type, saved ? total / 2 : total);
    const auto name = std::string(detail::damage_name(type));
    log(target.source.name + " takes " + std::to_string(amount) + " " + name + " damage.",
    {
        "{name} takes {damage} {type} damage.",
        {{"name", target.source.name}, {"damage", std::to_string(amount)}, {"type", name, true}}
    });
    damage(target, amount, false);
}

void Session::turn_undead(Actor &cleric)
{
    log(cleric.source.name + " uses Turn Undead.",
    {"{name} uses Turn Undead.", {{"name", cleric.source.name}}});
    for (auto &other : actors_)
        if (other.source.side != cleric.source.side && other.hp > 0 &&
                def(other).creature_type == "undead" &&
                distance(cleric.source.cell, other.source.cell) <= 30 &&
                detail::can_apply(other.effects) &&
                !saving_throw_succeeds(other, detail::Ability::wisdom, spell_dc(cleric)))
        {
            detail::apply_spell_benefit(other.effects, scope_, cleric.source.id,
                                        cleric.source.name, detail::EffectKind::turned, 0);
            log(other.source.name + " is turned.",
            {"{name} is turned.", {{"name", other.source.name}}});
        }
}

void Session::hold_still(std::vector<Command> &commands, const Actor &a) const
{
    // Incapacitated (Paralyzed, Sleep, Hideous Laughter): it can only end its
    // turn. Adaptation: a Prone, laughing creature could otherwise crawl.
    if (!detail::incapacitated(a.effects))
        return;
    const auto end = std::find_if(commands.begin(), commands.end(), [](const auto & offer)
    {
        return offer.verb == "end";
    });
    auto kept = *end;
    commands = {std::move(kept)};
}

void Session::flee_turning(std::vector<Command> &commands, const Actor &a) const
{
    // Turned: Incapacitated, it can only move as far from the Cleric as it can.
    const auto turned = std::find_if(a.effects.active.begin(), a.effects.active.end(),
                                     [](const auto & e)
    {
        return e.kind == detail::EffectKind::turned;
    });
    if (turned == a.effects.active.end())
        return;
    const auto cleric = std::find_if(actors_.begin(), actors_.end(), [&](const auto & other)
    {
        return turned->source_scope == scope_ && other.source.id == turned->source_actor;
    });
    if (cleric == actors_.end() || !conscious(*cleric))
        return;
    const Command *farthest = nullptr;
    int best = distance(a.source.cell, cleric->source.cell);
    for (const auto &offer : commands)
        if (offer.verb == "move" && distance(offer.destination, cleric->source.cell) > best)
        {
            farthest = &offer;
            best = distance(offer.destination, cleric->source.cell);
        }
    const auto end = std::find_if(commands.begin(), commands.end(), [](const auto & offer)
    {
        return offer.verb == "end";
    });
    auto kept = farthest ? *farthest : *end;
    commands = {std::move(kept)};
}

void Session::resolve_ensnaring_strike(Actor &a)
{
    // Vines grasp the creature just hit; a Large or larger one has Advantage.
    const auto &spell = *detail::find_spell("ensnaring_strike");
    auto &target = actor(a.smite_target);
    a.smite_target = 0;
    a.bonus = false;
    --a.slots;
    a.spent_slot = true;
    log(a.source.name + " casts Ensnaring Strike on " + target.source.name + ".",
    {
        "{name} casts Ensnaring Strike on {target}.",
        {{"name", a.source.name}, {"target", target.source.name}}
    });
    const int dc = spell_dc(a);
    if (target.hp == 0 || !detail::can_apply(target.effects) ||
            saving_throw_succeeds(target, detail::Ability::strength, dc, def(target).size >= 3))
        return;
    begin_concentration(a, spell);
    detail::apply_ensnaring_strike(target.effects, scope_, a.source.id, a.source.name, dc);
    log(target.source.name + " is Restrained.",
    {"{name} is Restrained.", {{"name", target.source.name}}});
}

void Session::squeeze_ensnared(Actor &a)
{
    // Ensnaring Strike's vines deal 1d6 Piercing at the start of each turn.
    if (!detail::has_effect(a.effects, detail::EffectKind::ensnaring_strike) || a.hp == 0)
        return;
    const int amount = resolved_damage(a, detail::DamageType::piercing, roll(6));
    log(a.source.name + " takes " + std::to_string(amount) + " Piercing damage from the vines.",
    {
        "{name} takes {damage} Piercing damage from the vines.",
        {{"name", a.source.name}, {"damage", std::to_string(amount)}}
    });
    damage(a, amount, false);
}

void Session::escape_ensnaring(Actor &a)
{
    // A Strength (Athletics) check against the spell's DC ends Ensnaring
    // Strike. A creature's Strength save bonus stands in for Athletics.
    const auto found = std::find_if(a.effects.active.begin(), a.effects.active.end(),
                                    [](const auto & e)
    {
        return e.kind == detail::EffectKind::ensnaring_strike ||
               e.kind == detail::EffectKind::entangle || e.kind == detail::EffectKind::webbed;
    });
    const auto vines = *found;
    const int bonus = a.source.character_profile.empty() ? def(a).saves[0] : def(a).athletics;
    const int natural = roll(20);
    const bool escaped = natural + bonus >= vines.dc;
    log(a.source.name + " Athletics check: d20 " + std::to_string(natural) + " + " +
        std::to_string(bonus) + " vs DC " + std::to_string(vines.dc) +
        (escaped ? " (escapes)." : " (still Restrained)."),
    {
        escaped ? "{name} Athletics check: d20 {roll} + {bonus} vs DC {dc} (escapes)."
        : "{name} Athletics check: d20 {roll} + {bonus} vs DC {dc} (still Restrained).",
        {   {"name", a.source.name},
            {"roll", std::to_string(natural)},
            {"bonus", std::to_string(bonus)},
            {"dc", std::to_string(vines.dc)}
        }
    });
    if (!escaped)
        return;
    // Ensnaring Strike ends, and with it the caster's Concentration; Entangle
    // only lets this creature go.
    if (vines.kind == detail::EffectKind::ensnaring_strike)
        for (auto &caster : actors_)
            if (vines.source_scope == scope_ && caster.source.id == vines.source_actor)
                end_concentration(caster);
    std::erase_if(a.effects.active, [&](const auto & e)
    {
        return e.id == vines.id;
    });
}

int Session::blessing_die(const Actor &a)
{
    return (detail::has_effect(a.effects, detail::EffectKind::bless) ? roll(4) : 0) -
           (detail::has_effect(a.effects, detail::EffectKind::bane) ? roll(4) : 0);
}

unsigned Session::selection_maximum(const PendingSelection &selection) const
{
    const auto &spell = *detail::find_spell(selection.verb);
    // Twinned Spell raises the spell's effective level by one.
    const bool twinned = readies(actor(selection.caster), Metamagic::twinned, spell);
    return spell.instances +
           (selection.verb.ends_with("_2") || twinned ? spell.upcast.extra_instances : 0u) +
           (selection.verb.ends_with("_2") && twinned ? spell.upcast.extra_instances : 0u);
}

void Session::choose_target(const Command &command)
{
    if (command.verb == "spell_cancel")
    {
        selection_.reset();
        return;
    }
    if (command.verb == "spell_cast")
    {
        cast_on_selection();
        return;
    }
    auto &chosen = selection_->chosen;
    if (const auto found = std::find(chosen.begin(), chosen.end(), command.target);
            found != chosen.end())
        chosen.erase(found);
    else
        chosen.push_back(command.target);
    if (chosen.size() == selection_maximum(*selection_))
        cast_on_selection();
}

void Session::cast_on_selection()
{
    const auto selection = *selection_;
    selection_.reset();
    auto &a = actor(selection.caster);
    end_sanctuary(a);
    const InvisibilityEnds ends{*this, a};
    const auto &spell = *detail::find_spell(selection.verb);
    const bool quickened = readies(a, Metamagic::quickened, spell);
    take_metamagic(a, spell);
    if (casting_with(Metamagic::heightened))
        heightened_target_ = selection.chosen.front();
    if (quickened)
        a.bonus = false;
    else
    {
        a.nick_origin = 0;
        (void)a.actions.spend(true);
    }
    if (selection.verb.ends_with("_2") || spell.level >= 2)
        --a.slots2;
    else
        --a.slots;
    a.spent_slot = true;
    log(a.source.name + " casts " + std::string(spell.label) + ".",
    {"{name} casts {spell}.", {{"name", a.source.name}, {"spell", std::string(spell.label), true}}});
    if (spell.concentration)
        begin_concentration(a, spell);
    const int dc = spell_dc(a);
    for (const auto id : selection.chosen)
    {
        auto &target = actor(id);
        if (spell.humanoid_only && def(target).creature_type != "humanoid")
            continue;
        if (spell.pattern == detail::SpellPattern::save_condition &&
                saving_throw_succeeds(target, spell.save, dc, fought_advantage(spell)))
            continue;
        apply_rider(spell, selection.verb, a, target, dc);
    }
}

bool Session::charmed_by(const Actor &a, EntityId other) const
{
    return std::any_of(a.effects.active.begin(), a.effects.active.end(), [&](const auto & e)
    {
        return e.kind == detail::EffectKind::charmed && e.source_actor == other &&
               e.source_scope == scope_;
    });
}

bool Session::fought_advantage(const detail::SpellDef &spell)
{
    // Charm Person: Advantage on the save while the caster's side fights the
    // creature, which in combat is always.
    return spell.rider == detail::Rider::charm_person;
}

int Session::armor_class(const Actor &target) const
{
    // Mage Armor replaces an unarmored base AC. Shield of Faith: +2. Warding Bond: +1.
    const int base = detail::has_effect(target.effects, detail::EffectKind::mage_armor)
                     ? std::max(def(target).ac, def(target).mage_armor_ac)
                     : def(target).ac;
    // Barkskin: an AC of 17 if it was lower.
    return (detail::has_effect(target.effects, detail::EffectKind::barkskin) ? std::max(base, 17)
            : base) +
           (detail::has_effect(target.effects, detail::EffectKind::shield_of_faith) ? 2 : 0) +
           (detail::has_effect(target.effects, detail::EffectKind::warding_bond) ? 1 : 0) +
           (detail::has_effect(target.effects, detail::EffectKind::shield) ? 5 : 0);
}

std::vector<detail::DamageAffinity> Session::affinities(const Actor &target) const
{
    auto result = def(target).affinities;
    if (detail::has_effect(target.effects, detail::EffectKind::warding_bond))
        result.push_back({detail::AffinityKind::resistance, std::nullopt, "spell:warding_bond"});
    if (detail::has_effect(target.effects, detail::EffectKind::protection_from_poison))
        result.push_back({detail::AffinityKind::resistance, detail::DamageType::poison,
                          "spell:protection_from_poison"});
    if (rage_of(target))
        for (const auto type : {detail::DamageType::bludgeoning, detail::DamageType::piercing,
                                detail::DamageType::slashing})
            result.push_back({detail::AffinityKind::resistance, type, "feature:rage"});
    if (silenced(target.source.cell))
        result.push_back({detail::AffinityKind::immunity, detail::DamageType::thunder,
                          "spell:silence"});
    return result;
}

std::vector<EntityId> Session::bonds_on(const Actor &target) const
{
    std::vector<EntityId> casters;
    for (const auto &e : target.effects.active)
        if (e.kind == detail::EffectKind::warding_bond && e.source_scope == scope_)
            for (const auto &caster : actors_)
                if (caster.source.id == e.source_actor && caster.hp > 0 && !caster.dead &&
                        caster.source.id != target.source.id &&
                        distance(caster.source.cell, target.source.cell) <= 60)
                    casters.push_back(caster.source.id);
    return casters;
}

const detail::Effect *Session::rage_of(const Actor &a)
{
    const auto found = std::find_if(a.effects.active.begin(), a.effects.active.end(),
                                    [](const auto & e)
    {
        return e.kind == detail::EffectKind::raging;
    });
    return found == a.effects.active.end() ? nullptr : &*found;
}

unsigned Session::rage_duration(const Actor &a) const
{
    // Until the end of the Barbarian's next turn, counted from its current one.
    return next_save_ms(a.source.id) + detail::round_ms;
}

void Session::extend_rage(Actor &a)
{
    for (auto &e : a.effects.active)
        if (e.kind == detail::EffectKind::raging)
            e.remaining_ms = rage_duration(a);
}

void Session::end_invisibility(EntityId id)
{
    auto &a = actor(id);
    if (std::erase_if(a.effects.active, [](const auto & e)
{
    return e.kind == detail::EffectKind::invisible;
}))
    log(a.source.name + " is no longer Invisible.",
    {"{name} is no longer Invisible.", {{"name", a.source.name}}});
}

void Session::end_sanctuary(Actor &a)
{
    std::erase_if(a.effects.active, [](const auto & e)
    {
        return e.kind == detail::EffectKind::sanctuary;
    });
}

bool Session::sanctuary_stops(Actor &attacker, const Actor &target)
{
    end_sanctuary(attacker);
    const auto ward = std::find_if(target.effects.active.begin(), target.effects.active.end(),
                                   [](const auto & e)
    {
        return e.kind == detail::EffectKind::sanctuary;
    });
    if (ward == target.effects.active.end() || attacker.source.side == target.source.side ||
            saving_throw_succeeds(attacker, detail::Ability::wisdom, ward->dc))
        return false;
    log(attacker.source.name + "'s attack on " + target.source.name + " is lost to Sanctuary.",
    {
        "{name}'s attack on {target} is lost to Sanctuary.",
        {{"name", attacker.source.name}, {"target", target.source.name}}
    });
    return true;
}

void Session::begin_concentration(Actor &caster, const detail::SpellDef &spell)
{
    // A new Concentration spell ends the old one first, removing its effects.
    // Extended Spell doubles it and gives Advantage on the saves to keep it.
    end_concentration(caster);
    const bool extended = casting_with(Metamagic::extended);
    caster.concentration.begin(
    {   {scope_, 1, caster.source.id},
        detail::benefit_duration_ms(rider_effect(spell.rider)) * (extended ? 2 : 1)
    });
    if (extended)
        detail::apply_spell_benefit(caster.effects, scope_, caster.source.id, caster.source.name,
                                    detail::EffectKind::extended, 0);
}

void Session::end_concentration(Actor &caster)
{
    if (caster.concentration.end())
        drop_concentration_effects(caster);
}

void Session::drop_concentration_effects(const Actor &caster)
{
    std::erase_if(zones_, [&](const auto & zone)
    {
        return zone.caster == caster.source.id && !zone.ends_ms;
    });
    for (auto &other : actors_)
        std::erase_if(other.effects.active, [&](const auto & e)
    {
        return concentration_effect(e.kind) && e.source_actor == caster.source.id &&
               e.source_scope == scope_;
    });
    log(caster.source.name + " loses Concentration.",
    {"{name} loses Concentration.", {{"name", caster.source.name}}});
}

void Session::burn_searing_smites(Actor &a)
{
    // At the start of each of its turns the target burns, then saves to end it.
    for (std::size_t n = 0; n < a.effects.active.size() && a.hp > 0; ++n)
    {
        if (a.effects.active[n].kind != detail::EffectKind::searing_smite)
            continue;
        const int dc = a.effects.active[n].dc;
        const int amount = resolved_damage(a, detail::DamageType::fire, roll(6));
        log(a.source.name + " burns for " + std::to_string(amount) + " Fire damage.",
        {
            "{name} burns for {damage} Fire damage.",
            {{"name", a.source.name}, {"damage", std::to_string(amount)}}
        });
        damage(a, amount, false);
        if (a.hp > 0 && saving_throw_succeeds(a, detail::Ability::constitution, dc))
            a.effects.active[n].remaining_ms = 0;
    }
    std::erase_if(a.effects.active, [](const auto & e)
    {
        return e.kind == detail::EffectKind::searing_smite && !e.remaining_ms;
    });
}

detail::Mastery Session::weapon_mastery(const Actor &a, bool ranged) const
{
    const auto &d = def(a);
    if (d.masteries.empty() || (!ranged && d.ranged_weapon))
        return detail::Mastery::none;
    for (const auto &key : d.equipment_keys)
        if (const auto *weapon = detail::weapon(key))
            return std::find(d.masteries.begin(), d.masteries.end(), key) != d.masteries.end()
                   ? weapon->mastery
                   : detail::Mastery::none;
    return detail::Mastery::none;
}

bool Session::mastery_capacity(const Actor &a, const Actor &target, bool ranged) const
{
    const auto kind = weapon_mastery(a, ranged);
    if (kind != detail::Mastery::sap && kind != detail::Mastery::vex)
        return true;
    return detail::can_apply_attack_mastery(target.effects,
                                            kind == detail::Mastery::sap ? detail::EffectKind::sap
                                            : detail::EffectKind::vex,
                                            scope_, a.source.id);
}

detail::RollModifiers Session::attack_modifiers(const Actor &a, const Actor &target, bool ranged,
        bool spell) const
{
    const auto &d = def(a);
    bool disadvantaged =
        !spell && (d.str_dex_disadvantage ||
                   (ranged ? d.ranged_heavy_disadvantage : d.melee_heavy_disadvantage));
    if (ranged)
    {
        if (!spell && distance(a.source.cell, target.source.cell) > d.range)
            disadvantaged = true;
        for (const auto &other : actors_)
            if (other.source.side != a.source.side && conscious(other) &&
                    distance(a.source.cell, other.source.cell) <= 5 && can_see(other, a))
                disadvantaged = true;
    }
    // Fog hides either creature from the other, as Blinded would.
    const bool attacker_fogged = obscured(a, a.source.cell) || obscured(a, target.source.cell);
    const bool target_fogged = obscured(target, a.source.cell) || obscured(target, target.source.cell);
    auto result = detail::attack_modifiers(detail::blinded(a.effects) || attacker_fogged ||
                                           unseen(a, target),
                                           detail::blinded(target.effects) || target_fogged ||
                                           unseen(target, a),
                                           target.dodge, disadvantaged || a.effects.prone);
    // At zero HP the creature is Unconscious and Prone (SRD pp.187,191).
    // At longer range their opposing attack modifiers cancel, not stack.
    const bool pack_tactics = !spell && d.pack_tactics && ally_beside(a, target);
    // A Boar's Bloodied Fury: Advantage at half its Hit Points or fewer.
    if (!spell && d.bloodied_fury && a.hp * 2 <= max_hp(a))
        result.advantage = true;
    if (helpless(target) || a.aim_ready || pack_tactics ||
            detail::vexed_by(target.effects, scope_, a.source.id))
        result.advantage = true;
    if (detail::sapped(a.effects) ||
            detail::has_effect(a.effects, detail::EffectKind::poisoned) ||
            detail::has_effect(a.effects, detail::EffectKind::scorched))
        result.disadvantage = true;
    // Ray of Enfeeblement: Disadvantage on Strength-based attack rolls, here
    // every melee weapon attack.
    if (!spell && !ranged && detail::has_effect(a.effects, detail::EffectKind::enfeebled))
        result.disadvantage = true;
    // Reckless Attack: Advantage on the attacker's Strength attack rolls and on
    // attack rolls against it.
    if ((!spell && detail::has_effect(a.effects, detail::EffectKind::reckless) &&
            strength_attack(a, ranged)) ||
            detail::has_effect(target.effects, detail::EffectKind::reckless))
        result.advantage = true;
    // Faerie Fire: Advantage against an outlined creature the attacker sees.
    if (detail::has_effect(target.effects, detail::EffectKind::outlined) && can_see(a, target))
        result.advantage = true;
    // Innate Sorcery: Advantage on spell attack rolls.
    if (spell && detail::has_effect(a.effects, detail::EffectKind::innate_sorcery))
        result.advantage = true;
    // Blur: attack rolls against the creature have Disadvantage.
    if (detail::has_effect(target.effects, detail::EffectKind::blur))
        result.disadvantage = true;
    // Guiding Bolt: the next attack roll against the target has Advantage.
    if (detail::has_effect(target.effects, detail::EffectKind::guiding_bolt))
        result.advantage = true;
    // Restrained: attacks against it have Advantage, its own have Disadvantage.
    if (detail::restrained(target.effects))
        result.advantage = true;
    if (detail::restrained(a.effects))
        result.disadvantage = true;
    // Protection from Evil and Good guards against these creature types.
    const auto &type = def(a).creature_type;
    if (detail::has_effect(target.effects, detail::EffectKind::protection_from_evil_and_good) &&
            (type == "aberration" || type == "celestial" || type == "elemental" ||
             type == "fey" || type == "fiend" || type == "undead"))
        result.disadvantage = true;
    if (target.effects.prone || target.hp == 0)
    {
        if (distance(a.source.cell, target.source.cell) <= 5)
            result.advantage = true;
        else
            result.disadvantage = true;
    }
    return result;
}

detail::DamageDieRule Session::weapon_die_rule(const Actor &a, bool ranged) const
{
    const auto &d = def(a);
    return d.great_weapon_fighting && !ranged && !d.ranged_weapon && !d.weapon_label.empty() &&
           d.weapon_hands == 2 ? detail::DamageDieRule::great_weapon_fighting
           : detail::DamageDieRule::normal;
}

Dice Session::weapon_dice(const Actor &a, bool ranged) const
{
    const auto &d = def(a);
    auto result = ranged ? d.ranged : d.melee;
    if (!ranged && d.versatile_sides && d.weapon_hands == 2)
        result.sides = d.versatile_sides;
    // Shillelagh: a d8 plus the spellcasting modifier.
    if (!ranged && shillelagh(a))
        result = {1, 8, d.casting - 2};
    if (a.cleave_damage || (a.light_damage && !d.two_weapon_fighting))
        result.bonus = std::min(0, result.bonus);
    return result;
}

void Session::apply_hit(Actor &a, Actor &target, int natural, int bonus, int mode, int amount,
                        bool savage, detail::DamageType type, bool ranged, bool spell,
                        bool optional_mastery)
{
    detail::consume_attack_masteries(actor(a.source.id).effects, target.effects, scope_,
                                     a.source.id);
    const std::string modifier_label = mode < 0   ? " (disadvantage)"
                                       : mode > 0 ? " (advantage)"
                                       : "";
    std::string message = a.source.name + " -> " + target.source.name + ": d20 " +
                          std::to_string(natural) + " + " + std::to_string(bonus) + " vs AC " +
                          std::to_string(armor_class(target)) + modifier_label;
    std::vector<MessageArgument> arguments{{"actor", a.source.name},
        {"target", target.source.name},
        {"roll", std::to_string(natural)},
        {"bonus", std::to_string(bonus)},
        {"ac", std::to_string(armor_class(target))},
        {"disadvantage", modifier_label, true}};
    if (!(!spell && def(a).champion && natural == 19) &&
            !attack_hits(natural, bonus, armor_class(target)))
    {
        log(message + " misses.",
        {
            "{actor} -> {target}: d20 {roll} + {bonus} vs AC {ac}{disadvantage} misses.",
            arguments
        });
        if (!spell && !ranged && weapon_mastery(a, false) == detail::Mastery::graze &&
                graze_damage(a, target) > 0)
            graze_ = PendingGraze{a.source.id, target.source.id, natural};
        return;
    }
    const bool critical = critical_hit(a, target, natural, spell);
    const bool helpless = unconscious(target) && distance(a.source.cell, target.source.cell) <= 5;
    if (savage)
        message += " (Savage Attacker)";
    amount = resolved_damage(target, type, amount);
    // The Versatile grip is chosen automatically, so the damage line names it.
    const std::string grip = spell || ranged || !def(a).versatile_sides ? ""
                             : def(a).weapon_hands == 2                 ? " (two-handed)"
                             : " (one-handed)";
    arguments.push_back({"savage", savage ? " (Savage Attacker)" : "", true});
    arguments.push_back({"hit", critical ? "CRITICAL" : "hits", true});
    arguments.push_back({"damage", std::to_string(amount)});
    arguments.push_back({"grip", grip, true});
    log(message + (critical ? " CRITICAL" : " hits") + " for " + std::to_string(amount) +
        " damage" + grip + ".",
    {
        "{actor} -> {target}: d20 {roll} + {bonus} vs AC {ac}{disadvantage}{savage} {hit} for {damage} damage{grip}.",
        arguments
    });
damage(target, amount, critical);
    // A Wolf's bite knocks a Medium or smaller target Prone.
    if (!spell && !ranged && def(a).prone_bite && def(target).size <= 2 && target.hp > 0 &&
            !target.effects.prone)
    {
        target.effects.prone = true;
        log(target.source.name + " is knocked Prone.",
        {"{name} is knocked Prone.", {{"name", target.source.name}}});
    }
    // A smite may follow a weapon hit or an Unarmed Strike on the attacker's own
    // turn; Divine and Searing Smite need it to be a Melee hit.
    if (!spell && actors_[turn_].source.id == a.source.id)
    {
        auto &attacker = actor(a.source.id);
        attacker.smite_target = target.source.id;
        attacker.smite_critical = critical;
        attacker.smite_melee = !ranged;
    }
    if (!spell)
    {
        const auto property = weapon_mastery(a, ranged);
        if (property == detail::Mastery::sap || (property == detail::Mastery::vex && amount > 0))
        {
            const auto &source = actor(a.source.id);
            const auto index = static_cast<std::size_t>(&source - actors_.data());
            const unsigned slot = turn_end_ms(index) - (index ? turn_end_ms(index - 1) : 0);
            const auto kind = property == detail::Mastery::sap ? detail::EffectKind::sap
                              : detail::EffectKind::vex;
            detail::apply_attack_mastery(target.effects, kind, scope_, a.source.id, a.source.name,
                                         next_turn_ms(source) +
                                         (kind == detail::EffectKind::vex ? slot : 0));
            const auto label = std::string(detail::mastery_name(property));
            log(target.source.name + " gains " + label + " from " + a.source.name + ".",
            {
                "{name} gains {mastery} from {source}.",
                {   {"name", target.source.name},
                    {"mastery", label, true},
                    {"source", a.source.name}
                }
            });
        }
    }
    if (!spell && optional_mastery)
        offer_mastery(a, target, natural, ranged, amount, critical);
    if (critical && def(a).champion && conscious(a))
    {
        ChampionMove move
        {
            a.source.id, target.source.id,
            natural,     std::max(0, def(a).speed - detail::speed_penalty(a.effects)) / 2,
            spell,       a.source.cell};
        if (effect_waiting() || a.cleave_damage)
        {
            move.triggered = true;
            move.cleave = a.cleave_damage;
            move.helpless = helpless;
            move.trigger_origin = a.source.cell;
            move.target_origin = target.source.cell;
        }
        if (effect_waiting())
            champion_offers_.push_back(move);
        else
            champion_move_ = move;
    }
    if (pending() == a.source.id && actors_[turn_].hp == 0 && (effect_waiting() || champion_move_))
    {
        path_.clear();
        path_index_ = 0;
        reactors_.clear();
        reactor_index_ = 0;
    }
}

bool Session::sneak_eligible(const Actor &a, const Actor &target, bool ranged, int mode) const
{
    const auto &d = def(a);
    if (!d.sneak_level || a.sneak_used)
        return false;
    const bool ally = std::any_of(actors_.begin(), actors_.end(),
                                  [&](const auto & other)
    {
        return other.source.id != a.source.id &&
               other.source.side == a.source.side &&
               conscious(other) &&
               distance(other.source.cell, target.source.cell) <= 5;
    });
    // A ranged weapon's fallback melee attack is unarmed, not that weapon.
    return detail::sneak_attack_eligible(
    {
        ranged ? d.range > 0 : !d.weapon_label.empty() && !d.ranged_weapon, d.finesse,
        d.ranged_weapon, mode, ally});
}

// Pack Tactics: a conscious ally of the attacker stands within 5 feet of the target.
bool Session::ally_beside(const Actor &a, const Actor &target) const
{
    return std::any_of(actors_.begin(), actors_.end(), [&](const auto & other)
    {
        return other.source.id != a.source.id && other.source.side == a.source.side &&
               conscious(other) && distance(other.source.cell, target.source.cell) <= 5;
    });
}

int Session::roll_advantage_damage(const Actor &a, const Actor &target, int natural)
{
    const auto &extra_dice = def(a).advantage_damage;
    const bool critical = critical_hit(a, target, natural);
    const int extra = dice(extra_dice, critical);
    const auto count = std::to_string(extra_dice.count * (critical ? 2 : 1));
    const auto sides = std::to_string(extra_dice.sides);
    log(a.source.name + " adds " + count + "d" + sides + " for " + std::to_string(extra) +
        " extra damage (Advantage).",
    {
        "{name} adds {count}d{sides} for {damage} extra damage (Advantage).",
        {   {"name", a.source.name},
            {"count", count},
            {"sides", sides},
            {"damage", std::to_string(extra)}
        }
    });
    return extra;
}

int Session::roll_sneak_attack(Actor &a, const Actor &target, int natural)
{
    a.sneak_used = actor(a.source.id).sneak_used = true;
    const bool critical = critical_hit(a, target, natural);
    const auto sneak_dice = detail::sneak_attack_dice(def(a).sneak_level);
    const int extra = dice(sneak_dice, critical);
    const auto count = std::to_string(sneak_dice.count * (critical ? 2 : 1));
    log(a.source.name + " adds Sneak Attack: " + count + "d6 for " + std::to_string(extra) +
        " extra damage.",
    {
        "{name} adds Sneak Attack: {count}d6 for {damage} extra damage.",
        {{"name", a.source.name}, {"count", count}, {"damage", std::to_string(extra)}}
    });
    return extra;
}

int Session::keep_higher_savage_roll(Actor &a, int first, int second)
{
    a.savage_used = actor(a.source.id).savage_used = true;
    const int kept = std::max(first, second);
    log(a.source.name + " rerolls weapon damage (Savage Attacker): " + std::to_string(first) +
        " and " + std::to_string(second) + ", keeps " + std::to_string(kept) + ".",
    {
        "{name} rerolls weapon damage (Savage Attacker): {first} and {second}, keeps {kept}.",
        {   {"name", a.source.name},
            {"first", std::to_string(first)},
            {"second", std::to_string(second)},
            {"kept", std::to_string(kept)}
        }
    });
    return kept;
}

bool Session::attack(Actor &a, Actor &target, bool ranged, bool spell, Dice spell_dice,
                     detail::DamageType spell_type, int bursts)
{
    if (target.source.cell.x != a.source.cell.x)
        a.facing_left = target.source.cell.x < a.source.cell.x;
    if (sanctuary_stops(a, target))
        return false;
    const auto &d = def(a);
    const auto modifiers = attack_modifiers(a, target, ranged, spell);
    a.aim_ready = false;
    // Guiding Bolt's Advantage is spent on this attack roll.
    std::erase_if(target.effects.active, [](const auto & e)
    {
        return e.kind == detail::EffectKind::guiding_bolt;
    });
    const int weapon_magic = spell ? 0 : magic_weapon_bonus(a);
    int natural = detail::d20(modifiers, rng_);
    end_invisibility(a.source.id);
    // An attack roll against an enemy on its own turn extends a Rage.
    if (target.source.side != a.source.side && actors_[turn_].source.id == a.source.id)
        extend_rage(actor(a.source.id));
    int bonus = (spell || (!ranged && shillelagh(a)) ? d.casting
                 : ranged ? d.ranged_bonus
                 : d.melee_bonus) + blessing_die(a) +
                (spell || ranged ? 0 : sacred_weapon_bonus(a)) + weapon_magic;
    const auto damage_dice = spell ? spell_dice : weapon_dice(a, ranged);
    // Seeking Spell: a missed spell attack rolls its d20 again, once.
    if (spell && casting_with(Metamagic::seeking) && natural != 20 &&
            !attack_hits(natural, bonus, armor_class(target)))
    {
        casting_metamagic_.reset();
        natural = detail::d20(modifiers, rng_);
        log("Seeking Spell rerolls the missed attack.", {"Seeking Spell rerolls the missed attack.", {}});
    }
    const bool automatic = natural == 20 || (!spell && d.champion && natural == 19);
    bool hit = automatic || attack_hits(natural, bonus, armor_class(target));
    // Bardic Inspiration may rescue a miss; Cutting Words may spoil a hit.
    if (!hit)
    {
        bonus += inspiration_roll(a);
        hit = attack_hits(natural, bonus, armor_class(target));
    }
    else if (!automatic)
    {
        bonus -= cutting_words(a);
        hit = attack_hits(natural, bonus, armor_class(target));
    }
    const auto damage_type = spell    ? spell_type
                             : ranged ? d.ranged_type
                             : sacred_damage_type(a, target);
    // Shield cannot turn a critical hit into a miss, but Deflect Attacks can
    // still lessen one.
    if (hit)
        ask_reaction(target, Asked::hit,
                     damage_type == detail::DamageType::bludgeoning ||
                     damage_type == detail::DamageType::piercing ||
                     damage_type == detail::DamageType::slashing,
                     automatic);
    if (hit && strikes_duplicate(a, target))
        return false;
    const bool sneak = hit && !spell && sneak_eligible(a, target, ranged, modifiers.mode());
    const auto roll_damage = [&]
    {
        return !spell && (d.sneak_level || d.great_weapon_fighting || a.light_damage ||
                          a.cleave_damage)
               ? detail::roll_damage_component(rng_, damage_dice,
                                               critical_hit(a, target, natural),
                                               weapon_die_rule(a, ranged))
               : spell ? this->spell_dice(a, damage_dice, critical_hit(a, target, natural, spell))
               : dice(damage_dice, critical_hit(a, target, natural, spell));
    };
    int weapon_damage = hit ? (bursts ? burst_damage(spell_dice, critical_hit(a, target, natural,
                                spell), bursts)
                               : roll_damage())
                        : 0;
    // Sneak Attack and Savage Attacker have one right answer, so they apply
    // automatically and the log records what they added.
    const int sneak_damage = sneak ? roll_sneak_attack(a, target, natural) : 0;
    const int advantage_damage = hit && !spell && d.advantage_damage.count &&
                                 modifiers.mode() > 0
                                 ? roll_advantage_damage(a, target, natural)
                                 : 0;
    const bool savage = hit && !spell && damage_dice.count && d.savage &&
                        !actor(a.source.id).savage_used;
    if (savage)
        weapon_damage = keep_higher_savage_roll(a, weapon_damage, roll_damage());
    if (hit)
        weapon_damage += weapon_magic;
    // Rage Damage: Strength-based weapon and unarmed hits.
    if (hit && !spell && rage_of(a) && strength_attack(a, ranged))
    {
        weapon_damage += d.rage_damage;
        log("Rage adds " + std::to_string(d.rage_damage) + " damage.",
        {"Rage adds {damage} damage.", {{"damage", std::to_string(d.rage_damage)}}});
        weapon_damage += frenzy_damage(actor(a.source.id), critical_hit(a, target, natural));
    }
    weapon_damage = resized_damage(a, hit && !spell, weapon_damage);
    // Colossus Slayer: once per turn, 1d8 more on a creature already missing HP.
    if (hit && !spell && d.colossus_slayer && !a.colossus_used && target.hp < max_hp(target))
    {
        a.colossus_used = true;
        const int extra = dice({1, 8, 0}, critical_hit(a, target, natural));
        weapon_damage += extra;
        log("Colossus Slayer adds " + std::to_string(extra) + " damage.",
        {"Colossus Slayer adds {damage} damage.", {{"damage", std::to_string(extra)}}});
    }
    // Ray of Enfeeblement: 1d8 less on each of the creature's damage rolls.
    const int enfeebled = hit && detail::has_effect(a.effects, detail::EffectKind::enfeebled)
                          ? roll(8)
                          : 0;
    if (enfeebled)
        log("Ray of Enfeeblement subtracts " + std::to_string(enfeebled) + " damage.",
        {"Ray of Enfeeblement subtracts {damage} damage.", {{"damage", std::to_string(enfeebled)}}});
    const int amount =
        hit ? deflected(a, target,
                        std::max(0, weapon_damage + sneak_damage + advantage_damage - enfeebled),
                        damage_type, ranged)
        : 0;
    apply_hit(a, target, natural, bonus, modifiers.mode(), amount, savage, damage_type, ranged,
              spell);
    // Hunter's Mark: any attack-roll hit on the caster's quarry deals 1d6 Force.
    if (hit && marked_by(target, a) && !target.dead)
    {
        const int extra = resolved_damage(target, detail::DamageType::force,
                                          dice({1, 6, 0}, critical_hit(a, target, natural, spell)));
        log(target.source.name + " takes " + std::to_string(extra) +
            " Force damage from Hunter's Mark.",
        {
            "{name} takes {damage} Force damage from Hunter's Mark.",
            {{"name", target.source.name}, {"damage", std::to_string(extra)}}
        });
        damage(target, extra, false);
    }
    // Hex: the caster's attack-roll hits deal 1d6 more Necrotic damage.
    if (hit && hexed_by(target, a) && !target.dead)
    {
        const int extra = resolved_damage(target, detail::DamageType::necrotic,
                                          dice({1, 6, 0}, critical_hit(a, target, natural, spell)));
        log(target.source.name + " takes " + std::to_string(extra) + " Necrotic damage from Hex.",
        {
            "{name} takes {damage} Necrotic damage from Hex.",
            {{"name", target.source.name}, {"damage", std::to_string(extra)}}
        });
        damage(target, extra, false);
    }
    // Divine Favor: a weapon hit deals an extra 1d4 Radiant damage.
    if (hit && !spell && detail::has_effect(a.effects, detail::EffectKind::divine_favor) &&
            !target.dead)
    {
        const int extra = resolved_damage(target, detail::DamageType::radiant,
                                          dice({1, 4, 0}, critical_hit(a, target, natural)));
        log(target.source.name + " takes " + std::to_string(extra) +
            " Radiant damage from Divine Favor.",
        {
            "{name} takes {damage} Radiant damage from Divine Favor.",
            {{"name", target.source.name}, {"damage", std::to_string(extra)}}
        });
        damage(target, extra, false);
    }
    // Hellish Rebuke answers the damage once the attack is done.
    if (hit && amount > 0 && target.hp > 0)
        rebuke(a, target);
    return hit;
}

void Session::reveal_lore(const Actor &caster, const Actor &target)
{
    // Hunter's Lore: the marked creature's damage Immunities, Resistances and
    // Vulnerabilities become known.
    if (!def(caster).hunters_lore)
        return;
    const auto &affinities = def(target).affinities;
    if (affinities.empty())
        log("Hunter's Lore: " + target.source.name +
            " has no damage Immunities, Resistances or Vulnerabilities.",
    {
        "Hunter's Lore: {name} has no damage Immunities, Resistances or Vulnerabilities.",
        {{"name", target.source.name}}
    });
    for (const auto &affinity : affinities)
    {
        const std::string kind = affinity.kind == detail::AffinityKind::resistance ? "Resistance"
                                 : affinity.kind == detail::AffinityKind::immunity ? "Immunity"
                                 : "Vulnerability";
        const std::string type =
            affinity.type ? std::string(detail::damage_name(*affinity.type)) : "all damage";
        log("Hunter's Lore: " + target.source.name + " has " + kind + " to " + type + ".",
        {
            "Hunter's Lore: {name} has {kind} to {type}.",
            {{"name", target.source.name}, {"kind", kind, true}, {"type", type, true}}
        });
    }
}

bool Session::marked_by(const Actor &target, const Actor &caster) const
{
    return std::any_of(target.effects.active.begin(), target.effects.active.end(),
                       [&](const auto & e)
    {
        return e.kind == detail::EffectKind::hunters_mark && e.source_scope == scope_ &&
               e.source_actor == caster.source.id;
    });
}

bool Session::hexed_by(const Actor &target, const Actor &caster) const
{
    return std::any_of(target.effects.active.begin(), target.effects.active.end(),
                       [&](const auto & e)
    {
        return e.kind == detail::EffectKind::hex && e.source_scope == scope_ &&
               e.source_actor == caster.source.id;
    });
}

bool Session::hex_can_move(const Actor &caster) const
{
    // Hex moves once its creature drops, while the caster concentrates on it.
    bool hexed = false;
    for (const auto &other : actors_)
        if (hexed_by(other, caster))
        {
            if (other.hp > 0 && !other.dead)
                return false;
            hexed = true;
        }
    return hexed;
}

bool Session::mark_can_move(const Actor &caster) const
{
    // The mark moves once its creature drops to 0 Hit Points, while the
    // caster still concentrates on it.
    bool marked = false;
    for (const auto &other : actors_)
        if (marked_by(other, caster))
        {
            if (other.hp > 0 && !other.dead)
                return false;
            marked = true;
        }
    return marked;
}

void Session::strike_true(Actor &a, Actor &target, bool radiant)
{
    // The attack uses the spellcasting modifier for its attack and damage
    // rolls; `casting` is that modifier plus the +2 Proficiency Bonus.
    auto striker = a;
    auto &d = striker.definition;
    const int modifier = d.casting - 2;
    d.melee_bonus += modifier - d.melee_ability;
    d.melee_ability = modifier;
    d.melee.bonus = modifier;
    if (radiant)
        d.melee_type = detail::DamageType::radiant;
    log(a.source.name + " casts True Strike.",
    {"{name} casts {spell}.", {{"name", a.source.name}, {"spell", "True Strike", true}}});
    attack(striker, target, false);
    a.aim_ready = striker.aim_ready;
}

std::optional<Session::ReadyMetamagic> Session::readied_metamagic(const Actor &a)
{
    for (const auto &e : a.effects.active)
        if (e.kind == detail::EffectKind::metamagic)
        {
            if (e.dc < 10)
                return ReadyMetamagic{Metamagic(e.dc), std::nullopt};
            return ReadyMetamagic{Metamagic::transmuted,
                                  detail::damage_type(transmuted_types.at(std::size_t(e.dc - 10)))};
        }
    return std::nullopt;
}

bool Session::readies(const Actor &a, Metamagic option, const detail::SpellDef &spell)
{
    const auto ready = readied_metamagic(a);
    return ready && ready->option == option && metamagic_applies(option, spell) &&
           a.lay_on_hands >= metamagic_cost(option);
}

void Session::take_metamagic(Actor &caster, const detail::SpellDef &spell)
{
    casting_metamagic_.reset();
    heightened_target_ = 0;
    const auto ready = readied_metamagic(caster);
    if (!ready || !readies(caster, ready->option, spell))
        return;
    caster.lay_on_hands -= metamagic_cost(ready->option);
    std::erase_if(caster.effects.active, [](const auto & e)
    {
        return e.kind == detail::EffectKind::metamagic;
    });
    casting_metamagic_ = ready;
    const std::string label(metamagic_labels[std::size_t(ready->option)]);
    log(caster.source.name + " uses " + label + ".",
    {"{name} uses {feature}.", {{"name", caster.source.name}, {"feature", label, true}}});
}

int Session::spell_range(const Actor &caster, const detail::SpellDef &spell) const
{
    // Eldritch Spear: 30 feet more per Warlock level for Eldritch Blast.
    const int range = spell.range + (spell.id == "eldritch_blast" && def(caster).eldritch_spear
                                     ? 30 * def(caster).level
                                     : 0);
    // Distant Spell: double the range, or 30 feet for Touch.
    if (!readies(caster, Metamagic::distant, spell))
        return range;
    return range <= 5 ? 30 : 2 * range;
}

detail::DamageType Session::cast_damage_type(detail::DamageType type) const
{
    if (casting_with(Metamagic::transmuted) && transmutable(type))
        return *casting_metamagic_->type;
    return type;
}

int Session::spell_dice(const Actor &caster, Dice dice, bool critical)
{
    // Empowered Spell rerolls the lowest dice below their average, up to the
    // Charisma modifier (at least one), and keeps the new rolls.
    if (!casting_with(Metamagic::empowered))
        return this->dice(dice, critical);
    std::vector<int> rolls;
    for (int n = 0; n < dice.count * (critical ? 2 : 1); ++n)
        rolls.push_back(roll(dice.sides));
    std::sort(rolls.begin(), rolls.end());
    const int rerolls = std::max(1, def(caster).casting - 2);
    for (int n = 0; n < rerolls && n < int(rolls.size()) && rolls[n] * 2 < dice.sides + 1; ++n)
        rolls[n] = roll(dice.sides);
    int total = dice.bonus;
    for (const int value : rolls)
        total += value;
    return total;
}

std::vector<EntityId> Session::careful_allies(const Actor &caster, const std::vector<Cell> &cells) const
{
    std::vector<EntityId> spared;
    if (!casting_with(Metamagic::careful))
        return spared;
    const auto most = std::size_t(std::max(1, def(caster).casting - 2));
    for (const auto &other : actors_)
        if (spared.size() < most && other.source.side == caster.source.side &&
                other.source.id != caster.source.id && !other.dead &&
                std::find(cells.begin(), cells.end(), other.source.cell) != cells.end())
            spared.push_back(other.source.id);
    return spared;
}

int Session::burst_damage(Dice dice, bool critical, int bursts)
{
    // Each die showing its maximum adds another, up to `bursts` extra dice.
    const int count = dice.count * (critical ? 2 : 1);
    int total = dice.bonus, extra = 0;
    for (int n = 0; n < count + extra; ++n)
    {
        const int rolled = roll(dice.sides);
        total += rolled;
        if (rolled == dice.sides && extra < bursts)
            ++extra;
    }
    return total;
}

int Session::resized_damage(const Actor &a, bool weapon_hit, int amount)
{
    // Enlarge: 1d4 more weapon damage. Reduce: 1d4 less, but not below 1.
    if (!weapon_hit)
        return amount;
    if (detail::has_effect(a.effects, detail::EffectKind::enlarged))
    {
        const int extra = roll(4);
        log("Enlarge adds " + std::to_string(extra) + " damage.",
        {"Enlarge adds {damage} damage.", {{"damage", std::to_string(extra)}}});
        return amount + extra;
    }
    if (detail::has_effect(a.effects, detail::EffectKind::reduced))
    {
        const int less = roll(4);
        log("Reduce subtracts " + std::to_string(less) + " damage.",
        {"Reduce subtracts {damage} damage.", {{"damage", std::to_string(less)}}});
        return std::max(1, amount - less);
    }
    return amount;
}

int Session::frenzy_damage(Actor &berserker, bool critical)
{
    // Frenzy: raging and reckless, the turn's first Strength-based hit deals a
    // d6 per point of Rage Damage. The Reckless effect records that it struck.
    if (!def(berserker).frenzy || actors_[turn_].source.id != berserker.source.id)
        return 0;
    for (auto &e : berserker.effects.active)
        if (e.kind == detail::EffectKind::reckless && !e.dc)
        {
            e.dc = 1;
            const int extra = dice({def(berserker).rage_damage, 6, 0}, critical);
            log("Frenzy adds " + std::to_string(extra) + " damage.",
            {"Frenzy adds {damage} damage.", {{"damage", std::to_string(extra)}}});
            return extra;
        }
    return 0;
}

void Session::attack_recklessly(Actor &a)
{
    // Until the start of its next turn: Advantage on its Strength attack rolls,
    // and attack rolls against it have Advantage.
    detail::apply_poisoned(a.effects, scope_, a.source.id, a.source.name, next_turn_ms(a),
                           detail::EffectKind::reckless);
    log(a.source.name + " attacks recklessly.",
    {"{name} attacks recklessly.", {{"name", a.source.name}}});
}

bool Session::strength_attack(const Actor &a, bool ranged) const
{
    // A melee attack without a weapon is an Unarmed Strike, always Strength;
    // a ranged one is Strength only when thrown.
    const auto &d = def(a);
    if (ranged)
        return !d.ranged_weapon && d.ranged_ability == d.strength;
    return !d.melee.count || d.melee_ability == d.strength;
}

bool Session::shillelagh(const Actor &a) const
{
    return def(a).shillelagh_weapon && detail::has_effect(a.effects, detail::EffectKind::shillelagh);
}

int Session::magic_weapon_bonus(const Actor &a) const
{
    for (const auto &effect : a.effects.active)
        if (effect.kind == detail::EffectKind::magic_weapon)
            return effect.dc;
    return 0;
}

bool Session::strikes_duplicate(const Actor &attacker, Actor &target)
{
    // Mirror Image: a d6 per duplicate left; any 3 or higher and a duplicate
    // takes the hit and vanishes. A Blinded attacker is not fooled.
    const auto image = std::find_if(target.effects.active.begin(), target.effects.active.end(),
                                    [](const auto & e)
    {
        return e.kind == detail::EffectKind::mirror_image;
    });
    if (image == target.effects.active.end() || detail::blinded(attacker.effects))
        return false;
    bool diverted = false;
    for (int duplicate = 0; duplicate < image->dc; ++duplicate)
        diverted = roll(6) >= 3 || diverted;
    if (!diverted)
        return false;
    log(attacker.source.name + " hits one of " + target.source.name + "'s duplicates, which vanishes.",
    {
        "{attacker} hits one of {name}'s duplicates, which vanishes.",
        {{"attacker", attacker.source.name}, {"name", target.source.name}}
    });
    if (--image->dc == 0)
        target.effects.active.erase(image);
    return true;
}

int Session::sacred_weapon_bonus(const Actor &a) const
{
    for (const auto &effect : a.effects.active)
        if (effect.kind == detail::EffectKind::sacred_weapon)
            return effect.dc;
    return 0;
}

detail::DamageType Session::sacred_damage_type(const Actor &a, const Actor &target) const
{
    // Sacred Weapon's Radiant damage is optional; it is chosen whenever the
    // target resists the weapon's own type more than Radiant.
    const auto usual = def(a).melee_type;
    if (!sacred_weapon_bonus(a))
        return usual;
    const auto taken = [&](detail::DamageType type)
    {
        const std::array parts{detail::DamagePart{type, 100}};
        return detail::resolve_damage(parts, affinities(target)).total;
    };
    return taken(detail::DamageType::radiant) > taken(usual) ? detail::DamageType::radiant : usual;
}

void Session::finish_reaction()
{
    ++reactor_index_;
    update_outcome();
    if (outcome_ == Outcome::ongoing)
    {
        if (actors_[turn_].hp == 0)
        {
            path_.clear();
            path_index_ = 0;
            reactors_.clear();
            reactor_index_ = 0;
        }
        else if (!pending())
            progress_movement();
    }
}

void Session::update_outcome()
{
    bool party = false, enemies = false;
    for (const auto &a : actors_)
        if (a.hp > 0 && !a.dead)
            (a.source.side == 0 ? party : enemies) = true;
    if (!party || !enemies)
    {
        outcome_ = !party ? Outcome::defeat : Outcome::victory;
        // Concentration is tracked in combat only; it ends with the combat.
        for (auto &a : actors_)
            end_concentration(a);
        champion_move_.reset();
        mastery_.reset();
        champion_offers_.clear();
        effect_reaction_origin_.reset();
        path_.clear();
        path_index_ = 0;
        reactors_.clear();
        reactor_index_ = 0;
        log(outcome_ == Outcome::victory ? "Victory." : "The party is incapacitated. Defeat.");
        if (outcome_ == Outcome::victory)
            resolve_death_saves_after_victory();
    }
}

// SIMPLIFY-1: nobody sleeps through a fight. A character whose rest the
// encounter interrupted starts awake but still lying down.
void Session::wake_resting_participants()
{
    for (auto &a : actors_)
        if (a.source.resting && conscious(a))
        {
            a.effects.prone = true;
            log(a.source.name + " wakes up prone.",
            {"{name} wakes up prone.", {{"name", a.source.name}}});
        }
}

// Nothing is left to fight, so the remaining death saves are rolled at once
// instead of on a six-second clock. Prone does not outlast combat: the party
// is standing (or lying unconscious) when exploration resumes.
void Session::resolve_death_saves_after_victory()
{
    for (auto &a : actors_)
    {
        if (a.hp != 0 || a.dead || a.stable)
            continue;
        do
        {
            const int roll = detail::death_save(a, rng_, !detail::healing_blocked(a.effects));
            log(a.source.name + " death save: " + std::to_string(roll),
            {
                "{name} death save: {roll}",
                {{"name", a.source.name}, {"roll", std::to_string(roll)}}
            });
        }
        while (a.hp == 0 && !a.dead && !a.stable);
        if (a.dead)
            log(a.source.name + " dies.", {"{name} dies.", {{"name", a.source.name}}});
        else if (a.hp > 0)
            log(a.source.name + " regains 1 HP.",
            {"{name} regains 1 HP.", {{"name", a.source.name}}});
        else
            log(a.source.name + " is stable.", {"{name} is stable.", {{"name", a.source.name}}});
    }
    for (auto &a : actors_)
        a.effects.prone = false;
}

bool Session::begin_turn()
{
    auto &a = actors_[turn_];
    std::erase_if(a.effects.active,
                  [](const auto & effect)
    {
        return effect.kind == detail::EffectKind::shocking_grasp;
    });
    if (a.dead)
        return false;
    // Warding Bond ends once its caster is down or more than 60 feet away.
    for (auto &other : actors_)
    {
        const auto kept = bonds_on(other);
        std::erase_if(other.effects.active, [&](const auto & e)
        {
            return e.kind == detail::EffectKind::warding_bond &&
                   std::find(kept.begin(), kept.end(), e.source_actor) == kept.end();
        });
    }
    burn_searing_smites(a);
    squeeze_ensnared(a);
    // Wild Shape ends when it lapses or its Druid is Incapacitated.
    if (a.form && detail::incapacitated(a.effects))
        end_wild_shape(a);
    refresh_form(a);
    // Heat Metal may be repeated from the caster's next turn on.
    for (auto &other : actors_)
        for (auto &effect : other.effects.active)
            if (effect.kind == detail::EffectKind::heated && effect.source_actor == a.source.id)
                effect.dc = 0;
    if (a.dead)
        return false;
    spring_zones(a, ZoneKind::web);
    // Heroism: Temporary HP at the start of each of the target's turns, kept
    // only when higher than what the target already has.
    for (const auto &effect : a.effects.active)
        if (effect.kind == detail::EffectKind::heroism && a.hp > 0 &&
                effect.dc > a.temporary_hp.amount)
            detail::grant_temporary_hp(a, {effect.dc, "spell:heroism"},
                                       TemporaryHpChoice::use_new);
    if (a.hp == 0)
    {
        if (!a.stable)
        {
            const int result = detail::death_save(a, rng_, !detail::healing_blocked(a.effects));
            log(a.source.name + " death save: " + std::to_string(result),
            {
                "{name} death save: {roll}",
                {{"name", a.source.name}, {"roll", std::to_string(result)}}
            });
            if (a.hp > 0)
            {
                a.effects.prone = true;
                if (shares_occupied_space(a))
                    a.involuntary_overlap = true;
            }
            if (a.dead)
                clear_departed_overlaps();
        }
        if (a.hp == 0)
            return false;
    }
    for (auto &actor : actors_)
    {
        actor.cleave_used = false;
        actor.colossus_used = actor.horde_used = actor.resistance_used = false;
        actor.horde_origin = 0;
        actor.savage_used = false;
        actor.sneak_used = false;
        actor.aim_used = actor.aim_ready = false;
        actor.moved = false;
        actor.light_origins.clear();
        actor.light_extra = 0;
        actor.nick_origin = 0;
        actor.smite_target = 0;
        actor.smite_critical = actor.smite_melee = false;
    }
    // Historical checkpoints do not distinguish a spent Light attack from any
    // other Bonus Action. Keep their current turn exact; enable Nick at the first
    // fresh turn boundary, where the new shared allowance is unambiguous.
    if (!nick_active_ && std::any_of(actors_.begin(), actors_.end(),
                                     [&](const auto & actor)
{
    return has_nick(actor);
    }))
    {
        activate_light();
        nick_active_ = true;
    }
    a.actions = {};
    a.surge_used = false;
    a.bonus = a.reaction = true;
    a.dodge = a.disengaged = false;
    a.movement = def(a).speed;
    a.dashes = 0;
    a.spent_slot = false;
    a.rush_used = false;
    // Command's Grovel and Halt take the whole turn; Approach and Flee limit
    // the turn's commands instead (obey_command).
    if (const auto *command = detail::command_effect(a.effects))
    {
        if (command->dc == int(detail::CommandOption::grovel))
        {
            a.effects.prone = true;
            log(a.source.name + " grovels.", {"{name} grovels.", {{"name", a.source.name}}});
            return false;
        }
        if (command->dc == int(detail::CommandOption::halt))
        {
            log(a.source.name + " halts.", {"{name} halts.", {{"name", a.source.name}}});
            return false;
        }
    }
    log("Round " + std::to_string(round_) + ": " + a.source.name + " acts.",
    {
        "Round {round}: {name} acts.",
        {{"round", std::to_string(round_)}, {"name", a.source.name}}
    });
    return true;
}

unsigned Session::next_turn_ms(const Actor &target) const
{
    const auto index = static_cast<std::size_t>(&target - actors_.data());
    const unsigned start = index ? turn_end_ms(index - 1) : 0,
                   current = turn_ ? turn_end_ms(turn_ - 1) : 0;
    return start > current ? start - current : detail::round_ms - current + start;
}

unsigned Session::next_save_ms(EntityId target) const
{
    const auto found = std::find_if(actors_.begin(), actors_.end(),
                                    [&](const auto & a)
    {
        return a.source.id == target;
    });
    const auto end = turn_end_ms(found - actors_.begin());
    const auto start = turn_ ? turn_end_ms(turn_ - 1) : 0;
    return end > start ? end - start : detail::round_ms - start + end;
}

void Session::log_save(const Actor &target, const detail::SaveResult &result)
{
    constexpr std::array names{"Strength",     "Dexterity", "Constitution",
                               "Intelligence", "Wisdom",    "Charisma"};
    constexpr std::array messages
    {
        "{name} Strength save: d20 {roll} + {bonus} vs DC {dc} ({result}).",
        "{name} Dexterity save: d20 {roll} + {bonus} vs DC {dc} ({result}).",
        "{name} Constitution save: d20 {roll} + {bonus} vs DC {dc} ({result}).",
        "{name} Intelligence save: d20 {roll} + {bonus} vs DC {dc} ({result}).",
        "{name} Wisdom save: d20 {roll} + {bonus} vs DC {dc} ({result}).",
        "{name} Charisma save: d20 {roll} + {bonus} vs DC {dc} ({result})."};
    const auto ability = static_cast<unsigned>(result.ability);
    const std::string outcome = result.success ? "success" : "failure";
    log(target.source.name + " " + names[ability] + " save: d20 " + std::to_string(result.natural) +
        " + " + std::to_string(result.bonus) + " vs DC " + std::to_string(result.dc) + " (" +
        outcome + ").",
    {
        messages[ability],
        {   {"name", target.source.name},
            {"roll", std::to_string(result.natural)},
            {"bonus", std::to_string(result.bonus)},
            {"dc", std::to_string(result.dc)},
            {"result", outcome, true}
        }
    });
}

bool Session::saving_throw_succeeds(const Actor &target, detail::Ability ability, int dc,
                                    bool advantage)
{
    if (detail::paralyzed(target.effects) && !unconscious(target) &&
            (ability == detail::Ability::strength || ability == detail::Ability::dexterity))
    {
        const std::string name = ability == detail::Ability::strength ? "Strength" : "Dexterity";
        log(target.source.name + " automatically fails the " + name + " save while Paralyzed.",
        {
            "{name} automatically fails the {ability} save while Paralyzed.",
            {{"name", target.source.name}, {"ability", name, true}}
        });
        return false;
    }
    if (detail::has_effect(target.effects, detail::EffectKind::asleep) &&
            (ability == detail::Ability::strength || ability == detail::Ability::dexterity))
    {
        const std::string name = ability == detail::Ability::strength ? "Strength" : "Dexterity";
        log(target.source.name + " automatically fails the " + name + " save while asleep.",
        {
            "{name} automatically fails the {ability} save while asleep.",
            {{"name", target.source.name}, {"ability", name, true}}
        });
        return false;
    }
    if (unconscious(target) &&
            (ability == detail::Ability::strength || ability == detail::Ability::dexterity))
    {
        const std::string name = ability == detail::Ability::strength ? "Strength" : "Dexterity";
        log(target.source.name + " automatically fails the " + name + " save while Unconscious.",
        {
            "{name} automatically fails the {ability} save while Unconscious.",
            {{"name", target.source.name}, {"ability", name, true}}
        });
        return false;
    }
    // Warding Bond: +1 to saving throws.
    const int bless = blessing_die(target) +
                      (detail::has_effect(target.effects, detail::EffectKind::warding_bond) ? 1 : 0);
    auto modifiers =
        detail::saving_modifiers(ability, def(target).str_dex_disadvantage, target.dodge);
    modifiers.advantage |= advantage;
    // Heightened Spell: Disadvantage on the save against the spell.
    modifiers.disadvantage |= heightened_target_ == target.source.id;
    // Danger Sense: Advantage on Dexterity saves unless Incapacitated.
    modifiers.advantage |= ability == detail::Ability::dexterity && def(target).danger_sense &&
                           !detail::incapacitated(target.effects);
    // Restrained: Disadvantage on Dexterity saves.
    modifiers.disadvantage |= ability == detail::Ability::dexterity && detail::restrained(target.effects);
    // Ray of Enfeeblement and Reduce: Disadvantage on Strength saves; Enlarge:
    // Advantage.
    if (ability == detail::Ability::strength)
    {
        modifiers.disadvantage |= detail::has_effect(target.effects, detail::EffectKind::enfeebled) ||
                                  detail::has_effect(target.effects, detail::EffectKind::reduced);
        modifiers.advantage |= detail::has_effect(target.effects, detail::EffectKind::enlarged) ||
                               rage_of(target);
    }
    auto result = detail::saving_throw(
                      ability, def(target).saves[static_cast<unsigned>(ability)] + bless, dc,
                      modifiers, rng_);
    log_save(target, result);
    // Bardic Inspiration may turn the failure into a success.
    if (!result.success)
        if (const int die = inspiration_roll(target))
            result.success = result.natural + result.bonus + die >= dc;
    return result.success;
}

void Session::advance_turn_time()
{
    // Partition one six-second round across its fixed initiative slots. Integer
    // boundaries telescope to exactly 6000 ms, even with 7 or 64 participants.
    // Dead/unconscious slots still pass time; menus and repeated snapshots don't.
    const unsigned delta = turn_end_ms(turn_) - (turn_ ? turn_end_ms(turn_ - 1) : 0);
    for (auto &a : actors_)
        detail::start_stable_recovery(a, rng_);
    std::vector<detail::RecoverySubject> subjects;
    std::vector<EntityId> unconscious;
    // Acid Arrow burns once its effect runs out, after the effects are walked.
    std::vector<EntityId> burned;
    for (auto &a : actors_)
    {
        subjects.push_back(
        {
            {a.source.id, a.effects, def(a).saves, a.dead, def(a).str_dex_disadvantage, a.dodge},
            a});
        if (a.hp == 0)
            unconscious.push_back(a.source.id);
    }
    detail::elapse_recovery(
        subjects, delta, rng_, detail::RecoveryMode::combat,
        [&](const detail::EffectEvent & event)
    {
        const auto &target = actor(event.target);
        if (event.save)
            log_save(target, *event.save);
        if (event.removed && (event.effect.kind == detail::EffectKind::sap ||
                              event.effect.kind == detail::EffectKind::vex ||
                              event.effect.kind == detail::EffectKind::slow))
            log(target.source.name + " loses a mastery effect.",
        {
            "{name} loses {mastery} from {source}.",
            {   {"name", target.source.name},
                {
                    "mastery",
                    event.effect.kind == detail::EffectKind::sap    ? "Sap"
                    : event.effect.kind == detail::EffectKind::slow ? "Slow"
                    : "Vex",
                    true
                },
                {"source", event.effect.source_name}
            }
        });
        if (event.removed && event.effect.kind == detail::EffectKind::chill_touch)
            log(target.source.name + " loses a Chill Touch effect.",
        {"{name} loses a Chill Touch effect.", {{"name", target.source.name}}});
        if (event.removed && event.effect.kind == detail::EffectKind::shocking_grasp)
            log(target.source.name + " loses a Shocking Grasp effect.",
        {"{name} loses a Shocking Grasp effect.", {{"name", target.source.name}}});
        if (event.removed && event.effect.kind == detail::EffectKind::ray_of_frost)
            log(target.source.name + " loses a Ray of Frost effect.",
        {"{name} loses a Ray of Frost effect.", {{"name", target.source.name}}});
        if (event.removed && event.effect.kind == detail::EffectKind::blindness)
            log(target.source.name + " recovers from a blindness effect.",
        {"{name} recovers from a blindness effect.", {{"name", target.source.name}}});
        if (event.removed && event.effect.kind == detail::EffectKind::hold_person)
            log(target.source.name + " is no longer Paralyzed.",
        {"{name} is no longer Paralyzed.", {{"name", target.source.name}}});
        if (event.removed && event.effect.kind == detail::EffectKind::acid_arrow)
            burned.push_back(event.target);
    });
    for (const auto id : burned)
    {
        auto &target = actor(id);
        if (target.dead)
            continue;
        const int amount = resolved_damage(target, detail::DamageType::acid, dice({2, 4, 0}));
        log(target.source.name + " takes " + std::to_string(amount) + " Acid damage from Acid Arrow.",
        {
            "{name} takes {damage} Acid damage from Acid Arrow.",
            {{"name", target.source.name}, {"damage", std::to_string(amount)}}
        });
        damage(target, amount, false);
    }
    for (auto &a : actors_)
        if (a.concentration.elapse(delta))
            drop_concentration_effects(a);
    // Rage ends early when the Barbarian is Incapacitated or falls.
    for (auto &a : actors_)
        if (rage_of(a) && (detail::incapacitated(a.effects) || a.hp == 0))
            std::erase_if(a.effects.active, [](const auto & e)
        {
            return e.kind == detail::EffectKind::raging;
        });
    // An ended Aid lowers the maximum, and Hit Points above it with it.
    for (auto &a : actors_)
        a.hp = std::min(a.hp, max_hp(a));
    for (auto &a : actors_)
        if (a.hp > 0 &&
                std::find(unconscious.begin(), unconscious.end(), a.source.id) != unconscious.end())
        {
            a.effects.prone = true;
            if (shares_occupied_space(a))
                a.involuntary_overlap = true;
            log(a.source.name + " recovers 1 HP naturally.",
            {"{name} recovers 1 HP naturally.", {{"name", a.source.name}}});
        }
    elapsed_ms_ +=
        std::min<std::uint64_t>(delta, std::numeric_limits<std::uint64_t>::max() - elapsed_ms_);
    std::erase_if(zones_, [&](const auto & zone)
    {
        return zone.ends_ms && zone.ends_ms <= elapsed_ms_;
    });
}

void Session::end_turn()
{
    actors_[turn_].actions.surge = false;
    // A readied Metamagic option lapses with the turn.
    std::erase_if(actors_[turn_].effects.active, [](const auto & e)
    {
        return e.kind == detail::EffectKind::metamagic;
    });
    spring_zones(actors_[turn_], ZoneKind::grease);
    burn_beside_spheres(actors_[turn_]);
    for (const auto &zone : std::vector<Zone>(zones_))
        if (zone.kind == ZoneKind::moonbeam && in_zone(zone, actors_[turn_].source.cell))
            moonbeam_burns(zone, actors_[turn_]);
    for (const auto &zone : std::vector<Zone>(zones_))
        if (zone.kind == ZoneKind::gust && in_zone(zone, actors_[turn_].source.cell))
            blow(zone, actors_[turn_]);
    for (std::size_t checked = 0; checked <= actors_.size() * 2; ++checked)
    {
        advance_turn_time();
        turn_ = (turn_ + 1) % actors_.size();
        // The round is a display counter. Saturation avoids wrapping it to
        // zero while keeping an extremely long (or edited) combat playable.
        if (turn_ == 0 && round_ < std::numeric_limits<unsigned>::max())
            ++round_;
        if (begin_turn())
            return;
    }
    update_outcome();
}

void Session::progress_movement()
{
    auto &a = actors_[turn_];
    const auto grid = movement_grid(a);
    while (path_index_ < path_.size() && a.hp > 0)
    {
        const auto destination = path_[path_index_];
        if (reactors_.empty() && !a.disengaged)
            for (const auto &other : actors_)
                if (other.source.side != a.source.side && conscious(other) && other.reaction &&
                        !detail::has_effect(other.effects, detail::EffectKind::turned) &&
                        !detail::incapacitated(other.effects) &&
                        !detail::opportunity_blocked(other.effects) &&
                        !charmed_by(other, a.source.id) &&
                        has_weapon_reaction(other, a.source.cell, destination) && can_see(other, a))
                    reactors_.push_back(other.source.id);
        if (pending())
            return;
        const auto cost = grid.step_cost(a.source.cell, destination);
        if (!cost || *cost > movement_left(a))
            throw std::logic_error("Invalid accepted movement path");
        a.movement -= *cost;
        const auto departed = a.source.cell;
        a.source.cell = destination;
        a.moved = true;
        clear_departed_overlaps();
        ++path_index_;
        reactors_.clear();
        reactor_index_ = 0;
        // Entering Grease or Web calls for a save; a creature caught stops.
        // Each square moved into or within Spike Growth pierces; entering a
        // Moonbeam burns.
        const auto zones = zones_;
        bool caught = false;
        for (const auto &zone : zones)
        {
            if ((zone.kind == ZoneKind::grease || zone.kind == ZoneKind::web) &&
                    in_zone(zone, destination) && !in_zone(zone, departed))
                caught = spring_zone(zone, a) || caught;
            if (zone.kind == ZoneKind::spikes && in_zone(zone, destination))
                spikes_pierce(a);
            if (zone.kind == ZoneKind::moonbeam && in_zone(zone, destination) &&
                    !in_zone(zone, departed))
                moonbeam_burns(zone, a);
        }
        if (caught)
            break;
    }
    path_.clear();
    path_index_ = 0;
    reactors_.clear();
    reactor_index_ = 0;
}

bool Session::submit(const Command &command)
{
    const auto offered = legal_commands();
    if (std::none_of(offered.begin(), offered.end(),
                     [&](const auto & c)
{
    return same_command(c, command);
    }))
    return false;
    if (reaction_prompt_)
        answer_reaction(command);
    else
        perform(command);
    // Revisions are command tickets; zero is reserved for invalid commands.
    // Unsigned wrap is defined, but must skip that reserved value.
    if (++revision_ == 0)
        revision_ = 1;
    update_outcome();
    if (initiative_choices_.empty() && outcome_ == Outcome::ongoing && !pending() &&
            !reaction_prompt_ && !champion_move_ && !effect_waiting() && actors_[turn_].hp == 0)
        end_turn();
    if (outcome_ != Outcome::ongoing)
        advance_turn_time();
    return true;
}

const Session::ReactionAnswer *Session::answer_of(EntityId target, Asked asked) const
{
    for (const auto &answer : reaction_answers_)
        if (answer.target == target && answer.asked == asked)
            return &answer;
    return nullptr;
}

bool Session::can_shield(const Actor &target) const
{
    return conscious(target) && target.reaction && (target.slots > 0 || target.slots2 > 0) &&
           detail::knows_spell(def(target).spells, "shield") &&
           !detail::incapacitated(target.effects) &&
           !detail::has_effect(target.effects, detail::EffectKind::shield);
}

bool Session::can_deflect(const Actor &target, bool deflectable) const
{
    return deflectable && def(target).deflect && conscious(target) && target.reaction &&
           !detail::incapacitated(target.effects);
}

void Session::ask_reaction(const Actor &target, Asked asked, bool deflectable,
                           bool critical) const
{
    // Unwinds the command to perform(), which asks the creature. A creature
    // asks once per command about the same kind of moment.
    if (answer_of(target.source.id, asked))
        return;
    const bool askable =
        asked == Asked::redirect || asked == Asked::rebuke || asked == Asked::inspiration ||
        asked == Asked::cutting ||
        (can_shield(target) && !critical) ||
        (asked == Asked::hit && can_deflect(target, deflectable));
    if (askable)
        throw ReactionQuestion{target.source.id, asked, deflectable, critical};
}

int Session::deflected(const Actor &attacker, Actor &target, int amount, detail::DamageType type,
                       bool ranged)
{
    // Deflect Attacks: 1d10 + Dexterity + Monk level less damage. Brought to 0,
    // a Focus Point redirects the force at the attacker, if it is near enough.
    const auto found = std::find_if(reaction_answers_.begin(), reaction_answers_.end(),
                                    [&](const auto & answer)
    {
        return answer.target == target.source.id && answer.asked == Asked::hit &&
               answer.verb == "deflect";
    });
    if (found == reaction_answers_.end())
        return amount;
    // Spent once: a later hit in the same command is not deflected.
    found->verb = "deflected";
    const auto &d = def(target);
    const int reduction = roll(10) + d.dexterity + d.level;
    const int left = std::max(0, amount - reduction);
    log(target.source.name + " deflects " + std::to_string(amount - left) + " damage.",
    {
        "{name} deflects {damage} damage.",
        {{"name", target.source.name}, {"damage", std::to_string(amount - left)}}
    });
    const int reach = ranged ? 60 : 5;
    if (left || target.surges <= 0 || attacker.hp <= 0 ||
            distance(target.source.cell, attacker.source.cell) > reach ||
            !can_see(target, attacker))
        return left;
    ask_reaction(target, Asked::redirect);
    if (answer_of(target.source.id, Asked::redirect)->verb != "redirect")
        return left;
    --target.surges;
    auto &foe = actor(attacker.source.id);
    log(target.source.name + " redirects the attack at " + foe.source.name + ".",
    {
        "{name} redirects the attack at {target}.",
        {{"name", target.source.name}, {"target", foe.source.name}}
    });
    if (!saving_throw_succeeds(foe, detail::Ability::dexterity, d.focus_dc))
    {
        const int force = resolved_damage(foe, type, dice({2, 6, d.dexterity}));
        log(foe.source.name + " takes " + std::to_string(force) + " damage.",
        {"{name} takes {damage} damage.", {{"name", foe.source.name}, {"damage", std::to_string(force)}}});
        damage(foe, force);
    }
    return left;
}

void Session::bless_fiends(const Actor &fallen)
{
    // Dark One's Blessing: a Fiend Warlock gains Temporary Hit Points when it
    // drops an enemy, or someone drops one within 10 feet of it. The creature
    // acting (or reacting) now is taken as the one who dropped it, and the
    // better Temporary Hit Points are kept, as with Heroism.
    const auto by = pending() ? pending() : actors_[turn_].source.id;
    for (auto &warlock : actors_)
    {
        const int amount = def(warlock).dark_ones_blessing;
        if (!amount || warlock.hp <= 0 || warlock.dead ||
                warlock.source.side == fallen.source.side ||
                (by != warlock.source.id && distance(warlock.source.cell, fallen.source.cell) > 10) ||
                amount <= warlock.temporary_hp.amount)
            continue;
        detail::grant_temporary_hp(warlock, {amount, "feature:dark_ones_blessing"},
                                   TemporaryHpChoice::use_new);
        log("Dark One's Blessing: " + warlock.source.name + " gains " + std::to_string(amount) +
            " Temporary Hit Points.",
        {
            "Dark One's Blessing: {name} gains {amount} Temporary Hit Points.",
            {{"name", warlock.source.name}, {"amount", std::to_string(amount)}}
        });
    }
}

int Session::inspiration_roll(const Actor &creature)
{
    auto &inspired = actor(creature.source.id);
    const auto found = std::find_if(inspired.effects.active.begin(), inspired.effects.active.end(),
                                    [](const auto & e)
    {
        return e.kind == detail::EffectKind::inspired;
    });
    if (found == inspired.effects.active.end() || !conscious(inspired))
        return 0;
    ask_reaction(inspired, Asked::inspiration);
    if (answer_of(inspired.source.id, Asked::inspiration)->verb != "inspire")
        return 0;
    const int sides = found->dc;
    inspired.effects.active.erase(found);
    const int die = roll(sides);
    log("Bardic Inspiration adds " + std::to_string(die) + " to " + inspired.source.name + "'s roll.",
    {
        "Bardic Inspiration adds {amount} to {name}'s roll.",
        {{"amount", std::to_string(die)}, {"name", inspired.source.name}}
    });
    return die;
}

int Session::cutting_words(const Actor &attacker)
{
    // A Lore Bard who sees the attacker within 60 feet may spend a Reaction and
    // a Bardic Inspiration use to subtract the die from the attack roll.
    for (auto &bard : actors_)
    {
        if (!def(bard).cutting_words || bard.source.side == attacker.source.side ||
                !conscious(bard) || !bard.reaction || detail::incapacitated(bard.effects) ||
                bard.arcane <= 0 || distance(bard.source.cell, attacker.source.cell) > 60 ||
                !can_see(bard, attacker))
            continue;
        ask_reaction(bard, Asked::cutting);
        if (answer_of(bard.source.id, Asked::cutting)->verb != "cutting")
            continue;
        bard.reaction = false;
        --bard.arcane;
        const int die = roll(6);
        log(bard.source.name + " uses Cutting Words: -" + std::to_string(die) + ".",
        {
            "{name} uses Cutting Words: -{amount}.",
            {{"name", bard.source.name}, {"amount", std::to_string(die)}}
        });
        return die;
    }
    return 0;
}

bool Session::can_rebuke(const Actor &warlock, const Actor &attacker) const
{
    return warlock.source.side != attacker.source.side && conscious(warlock) &&
           warlock.reaction && !detail::incapacitated(warlock.effects) &&
           (warlock.slots > 0 || warlock.slots2 > 0) &&
           detail::knows_spell(def(warlock).spells, "hellish_rebuke") &&
           distance(warlock.source.cell, attacker.source.cell) <= 60 && can_see(warlock, attacker) &&
           !attacker.dead;
}

void Session::rebuke(const Actor &attacker, Actor &warlock)
{
    // Hellish Rebuke: the attacker saves against 2d10 Fire, 3d10 from a
    // level-2 slot, taking half on a success.
    if (!can_rebuke(warlock, attacker))
        return;
    ask_reaction(warlock, Asked::rebuke);
    if (answer_of(warlock.source.id, Asked::rebuke)->verb != "rebuke")
        return;
    warlock.reaction = false;
    const bool second = warlock.slots <= 0;
    --(second ? warlock.slots2 : warlock.slots);
    auto &foe = actor(attacker.source.id);
    log(warlock.source.name + " casts Hellish Rebuke at " + foe.source.name + ".",
    {
        "{name} casts Hellish Rebuke at {target}.",
        {{"name", warlock.source.name}, {"target", foe.source.name}}
    });
    const bool saved = saving_throw_succeeds(foe, detail::Ability::dexterity, spell_dc(warlock));
    const int rolled = dice({second ? 3 : 2, 10, 0});
    const int amount = resolved_damage(foe, detail::DamageType::fire, saved ? rolled / 2 : rolled);
    log(foe.source.name + " takes " + std::to_string(amount) + " Fire damage.",
    {
        "{name} takes {damage} {type} damage.",
        {{"name", foe.source.name}, {"damage", std::to_string(amount)}, {"type", "Fire", true}}
    });
    damage(foe, amount);
}

void Session::perform(const Command &command)
{
    casting_metamagic_.reset();
    heightened_target_ = 0;
    const bool askable = std::any_of(actors_.begin(), actors_.end(), [&](const auto & other)
    {
        return can_shield(other) || def(other).deflect ||
               detail::knows_spell(def(other).spells, "hellish_rebuke") ||
               def(other).cutting_words ||
               detail::has_effect(other.effects, detail::EffectKind::inspired);
    });
    if (!askable)
    {
        dispatch(command);
        reaction_answers_.clear();
        return;
    }
    // Undo everything the command did before the hit: the replay rolls the
    // same dice from the same state.
    const Session before = *this;
    try
    {
        dispatch(command);
        reaction_answers_.clear();
    }
    catch (const ReactionQuestion &question)
    {
        *this = before;
        reaction_prompt_ = PendingReaction{question, command, reaction_answers_};
        reaction_answers_.clear();
    }
}

void Session::answer_reaction(const Command &command)
{
    const auto prompt = *reaction_prompt_;
    reaction_prompt_.reset();
    reaction_answers_ = prompt.answers;
    reaction_answers_.push_back({prompt.question.target, prompt.question.asked, command.verb});
    auto &defender = actor(prompt.question.target);
    if (command.verb == "shield")
    {
        defender.reaction = false;
        if (defender.slots > 0)
            --defender.slots;
        else
            --defender.slots2;
        detail::apply_poisoned(defender.effects, scope_, defender.source.id, defender.source.name,
                               next_turn_ms(defender), detail::EffectKind::shield);
        log(defender.source.name + " casts Shield.",
        {"{name} casts {spell}.", {{"name", defender.source.name}, {"spell", "Shield", true}}});
    }
    else if (command.verb == "deflect")
        defender.reaction = false;
    perform(prompt.command);
}

void Session::dispatch(const Command &command)
{
    if (command.verb == "reckless")
    {
        attack_recklessly(actor(command.actor));
        auto melee = command;
        melee.verb = "melee";
        dispatch(melee);
        return;
    }
    auto &a = actor(command.actor);
    const auto &d = def(a);
    // A smite must follow the hit at once. Resolving that hit's own weapon
    // mastery choice keeps the chance open; any other command ends it.
    if (!is_smite(command.verb) && !command.verb.starts_with("effect_"))
        a.smite_target = 0;
    if (turns_to_attack(command.verb) && command.target)
    {
        const auto &target = actor(command.target);
        if (target.source.cell.x != a.source.cell.x)
        {
            const bool new_left = target.source.cell.x < a.source.cell.x;
            if (new_left != a.facing_left)
            {
                a.facing_left = new_left;
                log(a.source.name + (new_left ? " turns left." : " turns right."));
            }
        }
    }
    if (!initiative_choices_.empty())
    {
        resolve_initiative(command);
    }
    else if (area_)
        aim_area(command);
    else if (selection_)
        choose_target(command);
    else if (!graze_ && !champion_move_ && effect_waiting())
    {
        use_effect(command);
    }
    else if (graze_)
    {
        const auto g = *graze_;
        graze_.reset();
        if (command.verb == "effect_use")
        {
            auto &target = actor(g.target);
            const int amount = graze_damage(a, target);
            log(a.source.name + " grazes " + target.source.name + " for " + std::to_string(amount) +
                " damage.",
            {
                "{actor} grazes {target} for {damage} damage.",
                {   {"actor", a.source.name},
                    {"target", target.source.name},
                    {"damage", std::to_string(amount)}
                }
            });
            damage(target, amount, false);
        }
        if (pending())
            finish_reaction();
    }
    else if (champion_move_)
    {
        if (command.verb == "end")
            finish_champion_move();
        else
        {
            const auto grid = movement_grid(a);
            const auto path =
                grid.reachable(champion_move_->remaining).path_to(command.destination);
            for (const auto cell : path)
            {
                champion_move_->remaining -= *grid.step_cost(a.source.cell, cell);
                a.source.cell = cell;
                a.moved = true;
            }
            clear_departed_overlaps();
            if (champion_move_->remaining == 0)
                finish_champion_move();
        }
    }
    else if (command.verb == "mind_use" || command.verb == "mind_skip")
    {
        const auto check = *check_choice_;
        check_choice_.reset();
        const int boost = command.verb == "mind_use" ? roll(10) : 0;
        if (boost && check.natural + d.medicine + boost >= 10)
            --a.winds;
        finish_check(check, boost);
    }
    else if (command.verb == "stabilize")
    {
        PendingCheck check{a.source.id, command.target, detail::d20({}, rng_), a.actions.surge};
        a.nick_origin = 0;
        a.actions.spend();
        if (check.natural + d.medicine < 10 && d.tactical_mind && a.winds > 0)
            check_choice_ = check;
        else
            finish_check(check, 0);
    }
    else if (command.verb == "temp_hp_keep" || command.verb == "temp_hp_use")
    {
        detail::grant_temporary_hp(a, *temporary_offer_,
                                   command.verb == "temp_hp_keep" ? TemporaryHpChoice::keep_current
                                   : TemporaryHpChoice::use_new);
        temporary_offer_.reset();
    }
    else if (command.verb == "weapon_select")
    {
        activate_light();
        a.selected_weapon = command.item;
        a.definition = equipped_definition(a, items_);
    }
    else if (command.verb.starts_with("light_") || command.verb.starts_with("nick_"))
    {
        const bool nick = command.verb.starts_with("nick_");
        if (!nick)
            a.bonus = false;
        if (nick_active_)
            a.light_extra = nick ? 2 : 1;
        if (command.verb == "light_throw" || command.verb == "nick_throw")
            throw_weapon(a, actor(command.target), command.item, true);
        else
        {
            auto attacker = item_actor(a, command.item);
            attacker.light_damage = true;
            attack(attacker, actor(command.target),
                   command.verb == "light_ranged" || command.verb == "nick_ranged");
            a.aim_ready = attacker.aim_ready;
        }
    }
    else if (command.verb == "stand_up")
    {
        a.movement -= std::max(0, d.speed - detail::speed_penalty(a.effects)) / 2;
        a.effects.prone = false;
    }
    else if (command.verb == "action_surge")
    {
        --a.surges;
        a.surge_used = true;
        a.actions.surge = true;
        log(a.source.name + " uses Action Surge.",
        {"{name} uses Action Surge.", {{"name", a.source.name}}});
    }
    else if (command.verb == "steady_aim")
    {
        a.bonus = false;
        a.aim_used = a.aim_ready = true;
        log(a.source.name + " uses Steady Aim.",
        {"{name} uses Steady Aim.", {{"name", a.source.name}}});
    }
    else if (command.verb == "cunning_dash" || command.verb == "cunning_disengage")
    {
        a.bonus = false;
        if (command.verb == "cunning_dash")
        {
            a.movement += d.speed;
            ++a.dashes;
            log(a.source.name + " dashes.", {"{name} dashes.", {{"name", a.source.name}}});
        }
        else
        {
            a.disengaged = true;
            log(a.source.name + " disengages.", {"{name} disengages.", {{"name", a.source.name}}});
        }
    }
    else if (command.verb == "aggressive")
    {
        // Only monsters have Aggressive and the policy spends it closing in, so
        // "toward a hostile creature" is not enforced on the extra movement.
        a.bonus = false;
        a.movement += d.speed;
        ++a.dashes;
        log(a.source.name + " moves aggressively.",
        {"{name} moves aggressively.", {{"name", a.source.name}}});
    }
    else if (command.verb == "divine_spark")
    {
        a.nick_origin = 0;
        (void)a.actions.spend(false);
        --a.channel_divinity;
        divine_spark(a, actor(command.target));
    }
    else if (command.verb == "preserve_life")
    {
        a.nick_origin = 0;
        (void)a.actions.spend(false);
        --a.channel_divinity;
        preserve_life(a);
    }
    else if (command.verb == "turn_undead")
    {
        a.nick_origin = 0;
        (void)a.actions.spend(false);
        --a.channel_divinity;
        turn_undead(a);
    }
    else if (command.verb == "escape")
    {
        a.nick_origin = 0;
        (void)a.actions.spend(false);
        escape_ensnaring(a);
    }
    else if (command.verb == "flurry_of_blows" || command.verb.starts_with("flurry_"))
    {
        // Two Unarmed Strikes; the second only if the first leaves the target up.
        a.bonus = false;
        --a.surges;
        log(a.source.name + " uses Flurry of Blows.",
        {"{name} uses Flurry of Blows.", {{"name", a.source.name}}});
        for (int strike = 0; strike < 2 && actor(command.target).hp > 0; ++strike)
        {
            auto striker = unarmed_actor(a);
            const bool hit = attack(striker, actor(command.target), false);
            a.aim_ready = striker.aim_ready;
            if (hit && actor(command.target).hp > 0)
                open_hand(a, actor(command.target), command.verb);
        }
    }
    else if (command.verb.starts_with("patient_defense") ||
             command.verb.starts_with("step_of_the_wind"))
        use_focus_movement(a, command.verb);
    else if (command.verb.starts_with("create_slot_") || command.verb.starts_with("convert_slot_"))
        use_font_of_magic(a, command.verb);
    else if (command.verb.starts_with("metamagic_"))
        ready_metamagic(a, command.verb.substr(10));
    else if (command.verb == "armor_of_shadows" || command.verb == "fiendish_vigor")
    {
        a.nick_origin = 0;
        (void)a.actions.spend(true);
        end_sanctuary(a);
        const InvisibilityEnds ends{*this, a};
        if (command.verb == "armor_of_shadows")
        {
            detail::apply_spell_benefit(a.effects, scope_, a.source.id, a.source.name,
                                        detail::EffectKind::mage_armor, 0);
            log(a.source.name + " gains Mage Armor.",
            {"{name} gains {spell}.", {{"name", a.source.name}, {"spell", "Mage Armor", true}}});
        }
        else
        {
            // Fiendish Vigor takes False Life's highest result: 2d4 + 4 = 12.
            TemporaryHitPoints offered{12, "spell:false_life"};
            log(a.source.name + " gains False Life.",
            {"{name} gains {spell}.", {{"name", a.source.name}, {"spell", "False Life", true}}});
            if (a.temporary_hp.amount)
                temporary_offer_ = std::move(offered);
            else
                detail::grant_temporary_hp(a, offered, TemporaryHpChoice::use_new);
        }
    }
    else if (command.verb == "flame_blade_strike")
    {
        // A Magic action: 3d6 + the spellcasting modifier Fire on a hit.
        a.nick_origin = 0;
        (void)a.actions.spend(true);
        end_sanctuary(a);
        (void)attack(a, actor(command.target), false, true, {3, 6, def(a).casting - 2},
                     detail::DamageType::fire);
    }
    else if (command.verb.starts_with("wild_shape_"))
    {
        a.bonus = false;
        --a.channel_divinity;
        const auto *form = detail::beast_form(std::string_view(command.verb).substr(11));
        std::erase_if(a.effects.active, [](const auto & e)
        {
            return e.kind == detail::EffectKind::wild_shape;
        });
        detail::apply_spell_benefit(a.effects, scope_, a.source.id, a.source.name,
                                    detail::EffectKind::wild_shape,
                                    int(form - detail::beast_forms.data()) + 1);
        refresh_form(a);
        log(a.source.name + " takes the shape of a " + std::string(form->label) + ".",
        {"{name} takes the shape of a {form}.", {{"name", a.source.name}, {"form", std::string(form->label), true}}});
        // Temporary Hit Points equal to the Druid level.
        TemporaryHitPoints offered{def(a).level, "feature:wild_shape"};
        if (a.temporary_hp.amount)
            temporary_offer_ = std::move(offered);
        else
            detail::grant_temporary_hp(a, offered, TemporaryHpChoice::use_new);
    }
    else if (command.verb == "leave_wild_shape")
    {
        a.bonus = false;
        end_wild_shape(a);
    }
    else if (command.verb == "heat_metal_again")
    {
        a.bonus = false;
        log(a.source.name + " heats the metal again.",
        {"{name} heats the metal again.", {{"name", a.source.name}}});
        heat_metal(a, actor(command.target));
    }
    else if (command.verb == "move_moonbeam")
    {
        a.nick_origin = 0;
        (void)a.actions.spend(true);
        move_moonbeam(a, actor(command.target));
    }
    else if (command.verb == "hurl_flame")
    {
        // A Magic action, not a casting: a ranged spell attack for 1d8 Fire.
        a.nick_origin = 0;
        (void)a.actions.spend(true);
        end_sanctuary(a);
        log(a.source.name + " hurls Produce Flame.",
        {"{name} hurls {spell}.", {{"name", a.source.name}, {"spell", "Produce Flame", true}}});
        (void)attack(a, actor(command.target), true, true, {1, 8, 0}, detail::DamageType::fire);
    }
    else if (command.verb == "bardic_inspiration")
    {
        a.bonus = false;
        --a.arcane;
        auto &ally = actor(command.target);
        detail::apply_spell_benefit(ally.effects, scope_, a.source.id, a.source.name,
                                    detail::EffectKind::inspired, 6);
        log(a.source.name + " inspires " + ally.source.name + ".",
        {"{name} inspires {target}.", {{"name", a.source.name}, {"target", ally.source.name}}});
    }
    else if (command.verb == "innate_sorcery")
    {
        a.bonus = false;
        --a.free_casts;
        detail::apply_spell_benefit(a.effects, scope_, a.source.id, a.source.name,
                                    detail::EffectKind::innate_sorcery, 0);
        log(a.source.name + " unleashes Innate Sorcery.",
        {"{name} unleashes Innate Sorcery.", {{"name", a.source.name}}});
    }
    else if (command.verb == "martial_arts")
    {
        a.bonus = false;
        auto striker = unarmed_actor(a);
        attack(striker, actor(command.target), false);
        a.aim_ready = striker.aim_ready;
    }
    else if (command.verb == "rage")
    {
        a.bonus = false;
        --a.channel_divinity;
        // A raging creature cannot keep Concentration.
        end_concentration(a);
        detail::apply_poisoned(a.effects, scope_, a.source.id, a.source.name, rage_duration(a),
                               detail::EffectKind::raging);
        log(a.source.name + " enters a Rage.", {"{name} enters a Rage.", {{"name", a.source.name}}});
    }
    else if (command.verb == "extend_rage")
    {
        a.bonus = false;
        extend_rage(a);
    }
    else if (command.verb == "shake_awake")
    {
        a.nick_origin = 0;
        (void)a.actions.spend(false);
        auto &sleeper = actor(command.target);
        std::erase_if(sleeper.effects.active, [](const auto & e)
        {
            return e.kind == detail::EffectKind::drowsy || e.kind == detail::EffectKind::asleep;
        });
        log(a.source.name + " shakes " + sleeper.source.name + " awake.",
        {
            "{name} shakes {target} awake.",
            {{"name", a.source.name}, {"target", sleeper.source.name}}
        });
    }
    else if (command.verb == "retreat_dash")
    {
        a.bonus = false;
        a.movement += d.speed;
        ++a.dashes;
        log(a.source.name + " dashes.", {"{name} dashes.", {{"name", a.source.name}}});
    }
    else if (command.verb == "roll_flaming_sphere")
    {
        a.bonus = false;
        auto &target = actor(command.target);
        auto &sphere = *std::find_if(zones_.begin(), zones_.end(), [&](const auto & zone)
        {
            return zone.kind == ZoneKind::flaming_sphere && zone.caster == a.source.id;
        });
        sphere.cells.front() = spectral_cell(target, sphere.cells.front());
        log(a.source.name + " rolls the Flaming Sphere into " + target.source.name + ".",
        {
            "{name} rolls the Flaming Sphere into {target}.",
            {{"name", a.source.name}, {"target", target.source.name}}
        });
        sphere_burns(a, target);
    }
    else if (command.verb == "spiritual_weapon_strike")
    {
        a.bonus = false;
        end_sanctuary(a);
        auto &target = actor(command.target);
        auto &force = *std::find_if(zones_.begin(), zones_.end(), [&](const auto & zone)
        {
            return zone.kind == ZoneKind::spiritual_weapon && zone.caster == a.source.id;
        });
        force.cells.front() = spectral_cell(target, force.cells.front());
        log(a.source.name + "'s Spiritual Weapon strikes.",
        {"{name}'s Spiritual Weapon strikes.", {{"name", a.source.name}}});
        attack(a, target, false, true, {1, 8, d.casting - 2}, detail::DamageType::force);
    }
    else if (command.verb == "horde_breaker")
    {
        a.horde_used = true;
        auto &target = actor(command.target);
        log(a.source.name + " uses Horde Breaker.",
        {"{name} uses Horde Breaker.", {{"name", a.source.name}}});
        attack(a, target, distance(a.source.cell, target.source.cell) > d.reach);
    }
    else if (command.verb == "hunters_mark_free")
    {
        const auto &spell = *detail::find_spell("hunters_mark");
        a.bonus = false;
        --a.free_casts;
        log(a.source.name + " casts Hunter's Mark (Favored Enemy).",
        {"{name} casts Hunter's Mark (Favored Enemy).", {{"name", a.source.name}}});
        begin_concentration(a, spell);
        apply_rider(spell, command.verb, a, actor(command.target), spell_dc(a));
    }
    else if (command.verb == "hex_move")
    {
        a.bonus = false;
        for (auto &other : actors_)
            std::erase_if(other.effects.active, [&](const auto & e)
        {
            return e.kind == detail::EffectKind::hex && e.source_scope == scope_ &&
                   e.source_actor == a.source.id;
        });
        auto &cursed = actor(command.target);
        detail::apply_spell_benefit(cursed.effects, scope_, a.source.id, a.source.name,
                                    detail::EffectKind::hex, 0);
        log(a.source.name + " moves Hex to " + cursed.source.name + ".",
        {
            "{name} moves Hex to {target}.",
            {{"name", a.source.name}, {"target", cursed.source.name}}
        });
    }
    else if (command.verb == "hunters_mark_move")
    {
        a.bonus = false;
        for (auto &other : actors_)
            std::erase_if(other.effects.active, [&](const auto & e)
        {
            return e.kind == detail::EffectKind::hunters_mark && e.source_scope == scope_ &&
                   e.source_actor == a.source.id;
        });
        auto &quarry = actor(command.target);
        detail::apply_spell_benefit(quarry.effects, scope_, a.source.id, a.source.name,
                                    detail::EffectKind::hunters_mark, 0);
        log(a.source.name + " moves Hunter's Mark to " + quarry.source.name + ".",
        {
            "{name} moves Hunter's Mark to {target}.",
            {{"name", a.source.name}, {"target", quarry.source.name}}
        });
        reveal_lore(a, quarry);
    }
    else if (command.verb == "sacred_weapon")
    {
        --a.channel_divinity;
        detail::apply_spell_benefit(a.effects, scope_, a.source.id, a.source.name,
                                    detail::EffectKind::sacred_weapon,
                                    std::max(1, d.casting - 2));
        log(a.source.name + " uses Sacred Weapon.",
        {"{name} uses Sacred Weapon.", {{"name", a.source.name}}});
    }
    else if (command.verb == "adrenaline_rush")
    {
        a.bonus = false;
        --a.rushes;
        a.rush_used = true;
        a.movement += d.speed;
        ++a.dashes;
        TemporaryHitPoints offered{d.rushes, std::string(rush_source)};
        if (a.temporary_hp.amount)
            temporary_offer_ = std::move(offered);
        else
            detail::grant_temporary_hp(a, offered, TemporaryHpChoice::use_new);
        log(a.source.name + " uses Adrenaline Rush.",
        {"{name} uses Adrenaline Rush.", {{"name", a.source.name}}});
    }
    else if (command.verb == "opportunity" || command.verb == "decline")
    {
        if (command.verb == "opportunity")
        {
            a.reaction = false;
            attack(a, actor(command.target), false);
        }
        if (!champion_move_ && !graze_ && !effect_waiting())
            finish_reaction();
    }
    else if (command.verb == "move")
    {
        path_ = path_to(a, command.destination);
        path_index_ = 0;
        progress_movement();
    }
    else if (command.verb == "end")
        end_turn();
    else if (command.verb == "second_wind")
    {
        a.bonus = false;
        --a.winds;
        heal(a, roll(10) + d.level);
    }
    else if (command.verb == "lay_on_hands")
    {
        // Restores what the target is missing, up to the pool; only the
        // Hit Points actually restored are spent.
        auto &target = actor(command.target);
        a.bonus = false;
        log(a.source.name + " uses Lay On Hands on " + target.source.name + ".",
        {
            "{name} uses Lay On Hands on {target}.",
            {{"name", a.source.name}, {"target", target.source.name}}
        });
        a.lay_on_hands -= heal(target, std::min(a.lay_on_hands, max_hp(target) - target.hp));
    }
    else if (const auto *aimed = detail::find_spell(command.verb);
             aimed && aimed->target == detail::SpellTarget::area)
        area_ = PendingArea{a.source.id, command.verb, default_area_center(a, *aimed)};
    else if (const auto *multi = detail::find_spell(command.verb);
             multi && selects_creatures(*multi) &&
             selection_maximum({a.source.id, command.verb, {}}) > 1)
    {
        // The first creature starts the choice; nothing is spent until the cast.
        selection_ = PendingSelection{a.source.id, command.verb, {command.target}};
        if (selection_->chosen.size() == selection_maximum(*selection_))
            cast_on_selection();
    }
    else if (is_smite(command.verb))
        resolve_smite(a, command.verb);
    else if (const auto *bonus_spell = detail::find_spell(command.verb);
             bonus_spell && (bonus_spell->bonus_action ||
                             readies(a, Metamagic::quickened, *bonus_spell)))
    {
        // A Bonus Action spell spends no Action, so it resolves outside the
        // Action block below.
        a.bonus = false;
        resolve_spell(*bonus_spell, command.verb, a, command.target);
    }
    else
    {
        a.nick_origin = 0;
        (void)a.actions.spend(detail::find_spell(command.verb) != nullptr);
        if (command.verb == "dash")
        {
            a.movement += d.speed;
            ++a.dashes;
            log(a.source.name + " dashes.", {"{name} dashes.", {{"name", a.source.name}}});
        }
        else if (command.verb == "dodge")
        {
            a.dodge = true;
            log(a.source.name + " dodges.", {"{name} dodges.", {{"name", a.source.name}}});
        }
        else if (command.verb == "disengage")
        {
            a.disengaged = true;
            log(a.source.name + " disengages.", {"{name} disengages.", {{"name", a.source.name}}});
        }
        else if (command.verb == "true_strike" || command.verb == "true_strike_radiant")
            strike_true(a, actor(command.target), command.verb == "true_strike_radiant");
        else if (const auto *spell = detail::find_spell(command.verb))
            resolve_spell(*spell, command.verb, a, command.target);
        else if (command.verb == "throw")
        {
            const auto *w = detail::weapon(items_.at(command.item - 1).definition);
            if (w && w->light)
                activate_light();
            throw_weapon(a, actor(command.target), command.item);
            if (light_active_)
                qualify_light(a, command.item);
        }
        else
        {
            // Only a weapon Attack action earns Light; spells and unarmed fallback do not.
            const auto selected_key = std::find_if(d.equipment_keys.begin(), d.equipment_keys.end(),
                                                   [](const auto & key)
            {
                return detail::weapon(key) != nullptr;
            });
            const auto *weapon =
                selected_key == d.equipment_keys.end() ? nullptr : detail::weapon(*selected_key);
            const bool qualifies =
                weapon && weapon->light &&
                (command.verb == "ranged" || (command.verb == "melee" && !weapon->ranged));
            if (qualifies)
                activate_light();
            const auto token = held_weapon(a);
            if (physical_inventory_ && command.verb == "ranged" && weapon && weapon->thrown &&
                    token)
            {
                throw_weapon(a, actor(command.target), token);
                if (qualifies)
                    qualify_light(a, token);
            }
            else
            {
                // Fire Bolt used to reach this fallthrough and borrow attack()'s
                // default 1d10 fire arguments; it is now an explicit table row.
                if (d.horde_breaker && !a.horde_origin)
                    a.horde_origin = command.target;
                attack(a, actor(command.target), command.verb != "melee");
                if (qualifies)
                    qualify_light(a, token);
            }
        }
    }
}

std::string Session::save() const
{
    // The module owns the checkpoint format, including RNG and pending reactions.
    std::ostringstream out;
    out << "OGCOMBAT " << checkpoint_format << ' ' << std::quoted(content_->identity.module) << ' '
        << std::quoted(content_->identity.version) << ' ' << std::quoted(content_->identity.content)
        << '\n';
    out << board_.width << ' ' << board_.height << '\n';
    for (auto cell : board_.terrain)
        out << unsigned(cell) << ' ';
    out << '\n';
    out << rng_ << ' ' << revision_ << ' ' << turn_ << ' ' << round_ << ' '
        << static_cast<int>(outcome_) << ' ' << actors_.size() << '\n';
    for (const auto &a : actors_)
    {
        out << a.source.id << ' ' << std::quoted(a.source.definition) << ' '
            << std::quoted(a.source.name) << ' ' << a.source.side << ' ' << a.source.cell.x << ' '
            << a.source.cell.y << ' ' << a.hp << ' ' << a.initiative << ' ' << a.movement << ' '
            << a.winds << ' ' << a.slots << ' ' << a.successes << ' ' << a.failures << ' '
            << a.actions.normal << ' ' << a.bonus << ' ' << a.reaction << ' ' << a.dodge << ' '
            << a.disengaged << ' ' << a.stable << ' ' << a.dead << ' '
            << std::quoted(a.source.character_profile) << ' ' << a.slots2 << ' ' << a.spent_slot
            << ' ' << a.savage_used << ' ' << a.facing_left << ' ' << a.involuntary_overlap << ' '
            << a.hit_dice << ' ' << a.recovery.death_save_in_ms << ' '
            << detail::encode_stable_recovery(a.recovery) << ' ' << a.temporary_hp.amount << ' '
            << std::quoted(a.temporary_hp.source_id) << ' ' << a.rushes << ' ' << a.rush_used
            << ' ' << a.surges << ' ' << a.surge_used << ' ' << a.actions.surge << ' ' << a.dashes
            << ' ' << a.arcane << ' ' << a.lay_on_hands << ' ' << a.free_casts << ' '
            << a.channel_divinity << ' ' << a.smite_target << ' ' << a.smite_critical << ' '
            << a.sneak_used << ' ' << a.aim_used
            << ' ' << a.aim_ready
            << ' ' << a.moved << ' ' << a.selected_weapon << ' ' << a.light_origins.size();
        for (auto id : a.light_origins)
            out << ' ' << id;
        out << ' ' << a.light_extra << ' ' << a.nick_origin << ' ' << a.cleave_used << ' '
            << a.colossus_used << ' ' << a.horde_used << ' ' << a.horde_origin << ' '
            << a.smite_melee << ' ' << a.resistance_used << ' ';
        detail::write_concentration(out, a.concentration);
        out << '\n';
    }
    out << path_.size() << ' ' << path_index_ << '\n';
    for (auto p : path_)
        out << p.x << ' ' << p.y << ' ';
    out << '\n';
    out << reactors_.size() << ' ' << reactor_index_ << '\n';
    for (auto id : reactors_)
        out << id << ' ';
    out << '\n';
    out << log_.size() << '\n';
    for (const auto &line : log_)
        out << std::quoted(line) << '\n';
    out << scope_ << ' ' << elapsed_ms_ << ' ' << actors_.size() << '\n';
    for (const auto &a : actors_)
    {
        detail::write_effects(out, a.effects);
        out << '\n';
    }
    out << bool(temporary_offer_) << '\n';
    if (temporary_offer_)
        out << temporary_offer_->amount << ' ' << std::quoted(temporary_offer_->source_id) << '\n';
    out << frost_movement_ << ' ' << items_active_ << '\n';
    out << physical_inventory_ << '\n';
    if (items_active_)
    {
        out << items_.size() << '\n';
        for (const auto &item : items_)
            out << item.id << ' ' << item.origin << ' ' << item.equipment_index << ' '
                << item.inventory_id << ' ' << std::quoted(item.definition) << ' ' << item.quantity
                << ' ' << item.stowed << ' ' << item.holder << '\n';
    }
    out << bool(check_choice_) << '\n';
    if (check_choice_)
    {
        const auto &c = *check_choice_;
        out << c.actor << ' ' << c.target << ' ' << c.natural << ' ' << c.surge_spent << '\n';
    }
    out << bool(champion_move_) << '\n';
    if (champion_move_)
    {
        const auto &c = *champion_move_;
        out << c.actor << ' ' << c.target << ' ' << c.natural << ' ' << c.remaining << ' '
            << c.spell << ' ' << c.origin.x << ' ' << c.origin.y << '\n';
    }
    out << bool(graze_) << '\n';
    if (graze_)
    {
        const auto &g = *graze_;
        out << g.actor << ' ' << g.target << ' ' << g.natural << ' ';
    }
    out << light_active_ << ' ' << nick_active_ << '\n';
    out << bool(mastery_) << '\n';
    if (mastery_)
    {
        const auto &m = *mastery_;
        out << m.actor << ' ' << m.target << ' ' << unsigned(m.kind) << ' ' << m.natural << ' '
            << m.ranged << ' ' << m.targeting << ' ' << std::quoted(m.weapon) << ' '
            << m.origin.x << ' ' << m.origin.y << ' ' << m.thrown_item << '\n';
    }
    const auto extra = [&](const ChampionMove & c)
    {
        out << c.triggered << ' ' << c.cleave << ' ' << c.helpless << ' ' << c.trigger_origin.x
            << ' ' << c.trigger_origin.y << ' ' << c.target_origin.x << ' ' << c.target_origin.y
            << '\n';
    };
    out << champion_offers_.size() << '\n';
    for (const auto &c : champion_offers_)
    {
        out << c.actor << ' ' << c.target << ' ' << c.natural << ' ' << c.remaining << ' '
            << c.spell << ' ' << c.origin.x << ' ' << c.origin.y << ' ';
        extra(c);
    }
    if (champion_move_)
        extra(*champion_move_);
    out << bool(effect_reaction_origin_) << ' ';
    if (effect_reaction_origin_)
        out << effect_reaction_origin_->actor << ' ' << effect_reaction_origin_->source.x << ' '
            << effect_reaction_origin_->source.y << ' ' << effect_reaction_origin_->mover.x
            << ' ' << effect_reaction_origin_->mover.y << ' '
            << effect_reaction_origin_->movement << ' ' << effect_reaction_origin_->prone;
    out << '\n';
    out << initiative_choices_.size();
    for (const auto id : initiative_choices_)
        out << ' ' << id;
    out << '\n';
    out << bool(selection_);
    if (selection_)
    {
        out << ' ' << selection_->caster << ' ' << selection_->verb << ' '
            << selection_->chosen.size();
        for (const auto id : selection_->chosen)
            out << ' ' << id;
    }
    out << '\n';
    out << bool(area_);
    if (area_)
        out << ' ' << area_->caster << ' ' << area_->verb << ' ' << area_->center.x << ' '
            << area_->center.y;
    out << '\n' << zones_.size();
    for (const auto &zone : zones_)
    {
        out << ' ' << zone.caster << ' ' << unsigned(zone.kind) << ' ' << zone.ends_ms << ' '
            << zone.cells.size();
        for (const auto cell : zone.cells)
            out << ' ' << cell.x << ' ' << cell.y;
    }
    out << '\n' << bool(reaction_prompt_);
    if (reaction_prompt_)
    {
        const auto &prompt = *reaction_prompt_;
        const auto &question = prompt.question;
        const auto &command = prompt.command;
        out << ' ' << question.target << ' ' << unsigned(question.asked) << ' '
            << question.deflectable << ' ' << question.critical << ' ' << command.actor << ' '
            << command.target << ' ' << std::quoted(command.verb) << ' ' << command.destination.x
            << ' ' << command.destination.y << ' ' << command.item << ' ' << prompt.answers.size();
        for (const auto &answer : prompt.answers)
            out << ' ' << answer.target << ' ' << unsigned(answer.asked) << ' '
                << std::quoted(answer.verb);
    }
    out << '\n';
    return out.str();
}

// Parse one actor independently of session mutation.
Actor read_checkpoint_actor(std::istream &input, const Content &content)
{
    Actor actor;
    auto &source = actor.source;
    unsigned light_count{};
    input >> source.id >> std::quoted(source.definition) >> std::quoted(source.name) >>
          source.side >> source.cell.x >> source.cell.y >> actor.hp >> actor.initiative >>
          actor.movement >> actor.winds >> actor.slots >> actor.successes >> actor.failures >>
          actor.actions.normal >> actor.bonus >> actor.reaction >> actor.dodge >> actor.disengaged >>
          actor.stable >> actor.dead >> std::quoted(source.character_profile) >> actor.slots2 >>
          actor.spent_slot >> actor.savage_used >> actor.facing_left >> actor.involuntary_overlap >>
          actor.hit_dice >> actor.recovery.death_save_in_ms >>
          actor.recovery.stable_recovery_in_ms >> actor.temporary_hp.amount >>
          std::quoted(actor.temporary_hp.source_id) >> actor.rushes >> actor.rush_used >>
          actor.surges >> actor.surge_used >> actor.actions.surge >> actor.dashes >> actor.arcane >>
          actor.lay_on_hands >> actor.free_casts >> actor.channel_divinity >> actor.smite_target >>
          actor.smite_critical >> actor.sneak_used >> actor.aim_used >> actor.aim_ready >> actor.moved >>
          actor.selected_weapon >> light_count;
    if (!input || light_count > 2)
        throw std::runtime_error("Invalid Light attack count");
    for (unsigned i = 0; i < light_count; ++i)
    {
        unsigned id{};
        input >> id;
        actor.light_origins.push_back(id);
    }
    input >> actor.light_extra >> actor.nick_origin >> actor.cleave_used >> actor.colossus_used >>
          actor.horde_used >> actor.horde_origin >> actor.smite_melee >> actor.resistance_used;
    actor.concentration = detail::read_concentration(input);
    // Hunter's Mark, 1 hour, is the longest Concentration spell in the game.
    if (const auto &held = actor.concentration.active();
            held && (held->source.caster != source.id || held->remaining_ms > 3600000))
        throw std::runtime_error("Invalid checkpoint concentration");
    detail::decode_stable_recovery(actor.recovery);
    if (!input ||
            (source.character_profile.empty() && !content.definitions.contains(source.definition)))
        throw std::runtime_error("Invalid checkpoint actor");
    actor.definition = source.character_profile.empty()
                       ? content.definitions.at(source.definition)
                       : character_definition(source.character_profile);
    const auto &definition = actor.definition;
    if (actor.cleave_used && std::none_of(definition.masteries.begin(), definition.masteries.end(),
                                          [](const auto & key)
{
    const auto *weapon = detail::weapon(key);
        return weapon &&
               weapon->mastery == detail::Mastery::cleave;
    }))
    throw std::runtime_error("Invalid Cleave expenditure source");
    if ((actor.colossus_used && !definition.colossus_slayer) ||
            ((actor.horde_used || actor.horde_origin) && !definition.horde_breaker))
        throw std::runtime_error("Invalid Hunter's Prey state");
    if ((actor.sneak_used && !definition.sneak_level) ||
            (actor.aim_used && (definition.sneak_level < 3 || actor.bonus || actor.moved)) ||
            (actor.aim_ready && !actor.aim_used))
        throw std::runtime_error("Invalid Rogue attack expenditure");
    // Cunning Action, Aggressive and Expeditious Retreat spend the bonus action
    // on a Dash. Effects are read later, so knowing the spell is enough here.
    const bool bonus_dash = (definition.cunning || definition.aggressive ||
                             detail::knows_spell(definition.spells, "expeditious_retreat")) &&
                            !actor.bonus;
    if (actor.dashes < 0 ||
            actor.dashes > int(!actor.actions.normal) +
            int(actor.rush_used || bonus_dash) +
            int(actor.surge_used && !actor.actions.surge) ||
            actor.movement > definition.speed * (1 + actor.dashes))
        throw std::runtime_error("Invalid Dash allowance count");
    // The upper bound waits for the effects, read later: Aid raises it.
    if (actor.hp < 0 || (actor.dead && actor.hp > 0) ||
            // Dash spends the action before adding a second movement allowance.
            // Accepting both extra movement and an unused action lets a later Dash
            // create a state outside the checkpoint's own movement bounds.
            actor.movement < 0 ||
            actor.movement >
            definition.speed * (1 + !actor.actions.normal +
                                int(actor.rush_used || bonus_dash) +
                                (actor.surge_used && !actor.actions.surge)) ||
            actor.arcane < 0 || actor.arcane > arcane_capacity(definition) || actor.surges < 0 ||
            actor.surges > surge_capacity(definition) ||
            (actor.surge_used && (!definition.surges || actor.surges == definition.surges)) ||
            (actor.actions.surge && !actor.surge_used) || actor.rushes < 0 ||
            actor.rushes > definition.rushes ||
            (actor.rush_used &&
             (actor.bonus || !definition.rushes || actor.rushes == definition.rushes)) ||
            actor.winds < 0 || actor.winds > definition.winds || actor.slots < 0 ||
            actor.slots > slot_room(definition) || actor.slots2 < 0 ||
            actor.slots2 > slot2_room(definition) ||
            actor.hit_dice < 0 || actor.hit_dice > (definition.hit_die ? definition.level : 0) ||
            actor.successes < 0 || actor.successes > 3 || actor.failures < 0 || actor.failures > 4 ||
            actor.lay_on_hands < 0 || actor.lay_on_hands > lay_capacity(definition) ||
            actor.free_casts < 0 || actor.free_casts > free_cast_capacity(definition) ||
            actor.channel_divinity < 0 || actor.channel_divinity > channel_capacity(definition))
        throw std::runtime_error("Invalid checkpoint actor state");
    detail::validate_recovery(actor);
    detail::validate_temporary_hp(actor.temporary_hp);
    return actor;
}

Battlefield read_checkpoint_board(std::istream &input)
{
    Battlefield board;
    input >> board.width >> board.height;
    // Bound dimensions before multiplication or allocation, even for truncated
    // input. No serialized count is allowed to control an unbounded allocation.
    if (!input || board.width < 2 || board.height < 2 || board.width > 64 || board.height > 64)
        throw std::runtime_error("Invalid checkpoint board");
    for (int i = 0; i < board.width * board.height; ++i)
    {
        unsigned terrain{};
        input >> terrain;
        if (!input || terrain > 2)
            throw std::runtime_error("Invalid checkpoint terrain");
        board.terrain.push_back(static_cast<std::uint8_t>(terrain));
    }
    return board;
}

void Session::restore_movement(std::istream &input)
{
    std::size_t count{};
    input >> count >> path_index_;
    if (!input || count > 1024 || path_index_ > count)
        throw std::runtime_error("Invalid checkpoint path");
    for (std::size_t i = 0; i < count; ++i)
    {
        Cell cell;
        input >> cell.x >> cell.y;
        if (!input || board_.at(cell) == 1)
            throw std::runtime_error("Invalid checkpoint path cell");
        path_.push_back(cell);
    }
    input >> count >> reactor_index_;
    if (!input || count > 64 || reactor_index_ > count)
        throw std::runtime_error("Invalid checkpoint reactions");
    std::set<EntityId> seen;
    for (std::size_t i = 0; i < count; ++i)
    {
        EntityId id{};
        input >> id;
        const auto exists = std::any_of(actors_.begin(), actors_.end(),
                                        [id](const auto & actor)
        {
            return actor.source.id == id;
        });
        if (!input || !seen.insert(id).second || !exists)
            throw std::runtime_error("Invalid checkpoint reactor");
        reactors_.push_back(id);
    }
}

void Session::finish_check(const PendingCheck &check, int boost)
{
    auto &a = actor(check.actor);
    auto &target = actor(check.target);
    const int total = check.natural + def(a).medicine + boost;
    const bool success = total >= 10;
    if (success)
        detail::stabilize(target, rng_);
    Message message
    {
        "{actor} stabilizes {target}: d20 {roll} + {modifier} + {boost} = {total} vs DC {dc}: {result}.",
        {   {"actor", a.source.name},
            {"target", target.source.name},
            {"roll", std::to_string(check.natural)},
            {"modifier", std::to_string(def(a).medicine)},
            {"boost", std::to_string(boost)},
            {"total", std::to_string(total)},
            {"dc", "10"},
            {"result", success ? "Success" : "Failure", true}
        }};
    log(a.source.name + " Medicine: " + std::to_string(check.natural) + " + " +
        std::to_string(def(a).medicine) + " + " + std::to_string(boost) + " = " +
        std::to_string(total) + " vs DC 10: " + (success ? "success" : "failure"),
        message);
}

void Session::finish_champion_move()
{
    champion_move_.reset();
    if (mastery_ && !mastery_available(*mastery_) && champion_offers_.empty())
        mastery_.reset();
    if (effect_waiting())
        return;
    finish_effects();
}

void Session::finish_effects()
{
    effect_reaction_origin_.reset();
    if (!pending())
        return;
    const auto &mover = actors_[turn_];
    // The critical mover can now occupy the interrupted route. Stop a route
    // that is no longer legal; never walk through the newly occupied cell.
    const auto grid = movement_grid(mover);
    auto cell = mover.source.cell;
    int budget = movement_left(mover);
    bool valid = true;
    for (auto i = path_index_; i < path_.size(); ++i)
    {
        const auto cost = grid.step_cost(cell, path_[i]);
        if (!cost || *cost > budget)
        {
            valid = false;
            break;
        }
        budget -= *cost;
        cell = path_[i];
    }
    if (!valid || !grid.can_stop_at(cell))
    {
        path_.clear();
        path_index_ = 0;
        reactors_.clear();
        reactor_index_ = 0;
        return;
    }
    finish_reaction();
}

void Session::validate_champion_move() const
{
    if (!champion_move_)
        return;
    const auto &c = *champion_move_;
    const auto who = std::find_if(actors_.begin(), actors_.end(),
                                  [&](const auto & a)
    {
        return a.source.id == c.actor;
    });
    const auto target = std::find_if(actors_.begin(), actors_.end(),
                                     [&](const auto & a)
    {
        return a.source.id == c.target;
    });
    if (who == actors_.end() || target == actors_.end() || c.actor == c.target ||
            !def(*who).champion || !conscious(*who) || c.natural < 2 || c.natural > 20 ||
            c.origin.x < 0 || c.origin.y < 0 || c.origin.x >= board_.width ||
            c.origin.y >= board_.height || board_.at(c.origin) == 1 || check_choice_ ||
            temporary_offer_ || outcome_ != Outcome::ongoing)
        throw std::runtime_error("Invalid Champion movement source");
    if (c.triggered)
    {
        if (!board_.contains(c.trigger_origin) || board_.at(c.trigger_origin) == 1 ||
                !board_.contains(c.target_origin) || board_.at(c.target_origin) == 1 ||
                (c.cleave && !who->cleave_used) ||
                (c.helpless && distance(c.trigger_origin, c.target_origin) > 5))
            throw std::runtime_error("Invalid Champion trigger positions");
    }
    else if (c.cleave || c.helpless || c.trigger_origin != Cell{} || c.target_origin != Cell{})
        throw std::runtime_error("Unexpected Champion trigger metadata");
    const int budget = std::max(0, def(*who).speed - detail::speed_penalty(who->effects)) / 2;
    if ((c.actor == actors_[turn_].source.id &&
            (who->actions.surge || (who->actions.normal && !who->surge_used))) ||
            c.remaining < 0 || c.remaining > budget ||
            distance(c.origin, who->source.cell) > budget - c.remaining ||
            !(c.natural == 20 || (!c.spell && c.natural == 19) ||
              ((c.helpless || target->hp == 0) &&
               distance(c.triggered ? c.trigger_origin : c.origin,
                        c.triggered ? c.target_origin : target->source.cell) <= 5)) ||
            (pending() ? (pending() != c.actor || who->reaction ||
                          (!c.cleave && c.target != actors_[turn_].source.id))
             : (c.actor != actors_[turn_].source.id &&
                !((c.cleave ? actors_[turn_].hp == 0
                   : c.target == actors_[turn_].source.id && target->hp == 0) &&
                  !who->reaction))))
        throw std::runtime_error("Invalid Champion movement allowance/trigger");
}

void Session::validate_check() const
{
    if (!check_choice_)
        return;
    const auto &c = *check_choice_;
    const auto &a = actors_[turn_];
    const auto target = std::find_if(actors_.begin(), actors_.end(),
                                     [&](const auto & t)
    {
        return t.source.id == c.target;
    });
    if (c.actor != a.source.id || !conscious(a) || !def(a).tactical_mind || a.winds <= 0 ||
            c.natural < 1 || c.natural > 20 || c.natural + def(a).medicine >= 10 ||
            target == actors_.end() || target->hp != 0 || target->dead || target->stable ||
            distance(a.source.cell, target->source.cell) > 5 ||
            !line_of_sight(a.source.cell, target->source.cell) || pending() || temporary_offer_ ||
            outcome_ != Outcome::ongoing || a.actions.surge ||
            (c.surge_spent ? !a.surge_used : a.actions.normal))
        throw std::runtime_error("Invalid pending ability check");
}

#include "mastery_resolution_impl.h"

void Session::validate_graze() const
{
    if (!graze_)
        return;
    const auto &g = *graze_;
    const auto a = std::find_if(actors_.begin(), actors_.end(),
                                [&](const auto & a)
    {
        return a.source.id == g.actor;
    });
    const auto t = std::find_if(actors_.begin(), actors_.end(),
                                [&](const auto & a)
    {
        return a.source.id == g.target;
    });
    if (a == actors_.end() || t == actors_.end() || a == t || !conscious(*a) || t->dead ||
            weapon_mastery(*a, false) != detail::Mastery::graze || g.natural < 1 || g.natural > 20 ||
            attack_hits(g.natural, def(*a).melee_bonus, armor_class(*t)) ||
            (def(*a).champion && g.natural == 19) ||
            distance(a->source.cell, t->source.cell) > def(*a).reach ||
            !line_of_sight(a->source.cell, t->source.cell) || check_choice_ ||
            temporary_offer_ || champion_move_ || outcome_ != Outcome::ongoing ||
            (pending() ? (pending() != g.actor || a->reaction || g.target != actors_[turn_].source.id)
             : (g.actor != actors_[turn_].source.id ||
                (a->actions.normal && (!a->surge_used || a->actions.surge)))))
        throw std::runtime_error("Invalid pending Graze");
}

void Session::validate_initiative() const
{
    if (initiative_choices_.empty())
        return;
    if (round_ != 1 || turn_ != 0 || elapsed_ms_ || outcome_ != Outcome::ongoing || pending() ||
            !path_.empty() || temporary_offer_ || check_choice_ || champion_move_ ||
            graze_ || effect_waiting())
        throw std::runtime_error("Invalid pre-turn Initiative phase");
    std::set<EntityId> seen;
    for (const auto id : initiative_choices_)
    {
        const auto &a = actor(id);
        if (!seen.insert(id).second || a.source.side != 0 ||
                (!def(a).alert && !metabolism_ready(a)) || !conscious(a))
            throw std::runtime_error("Invalid Initiative holder");
    }
    for (std::size_t i = 0; i < actors_.size(); ++i)
    {
        const auto &a = actors_[i];
        if (!a.actions.normal || a.actions.surge || !a.bonus || !a.reaction || a.dodge ||
                a.surge_used || a.rush_used || a.savage_used || a.sneak_used || a.aim_used || a.moved ||
                a.cleave_used || a.movement != def(a).speed)
            throw std::runtime_error("Spent action before Initiative decisions");
        if (i &&
                (actors_[i - 1].initiative < a.initiative ||
                 (actors_[i - 1].initiative == a.initiative && actors_[i - 1].source.id > a.source.id)))
            throw std::runtime_error("Unsorted Initiative phase");
    }
}

void Session::validate_restored_state() const
{
    for (const auto &a : actors_)
        if (a.hp > max_hp(a))
            throw std::runtime_error("Invalid checkpoint Hit Points");
    for (const auto &a : actors_)
        if (a.horde_origin && std::none_of(actors_.begin(), actors_.end(), [&](const auto & other)
    {
        return other.source.id == a.horde_origin;
    }))
        throw std::runtime_error("Invalid Horde Breaker target");
    validate_initiative();
    validate_check();
    validate_champion_move();
    validate_graze();
    validate_mastery_state();
    if (pending() && path_index_ >= path_.size())
        throw std::runtime_error("Reaction without movement");
    const auto &mover = actors_[turn_];
    if (temporary_offer_ && (outcome_ != Outcome::ongoing || pending() || !conscious(mover) ||
                             !mover.rush_used || mover.bonus || mover.temporary_hp.amount <= 0 ||
                             temporary_offer_->amount != mover.definition.rushes ||
                             temporary_offer_->source_id != rush_source))
        throw std::runtime_error("Invalid pending Temporary HP replacement");
    bool party = false, enemies = false;
    for (const auto &actor : actors_)
    {
        if (actor.cleave_used &&
                (actor.source.id == mover.source.id
                 ? (actor.actions.normal && (!actor.surge_used || actor.actions.surge))
                 : actor.reaction))
            throw std::runtime_error("Cleave without a spent triggering attack");
        if (actor.aim_used && actor.source.id != mover.source.id)
            throw std::runtime_error("Steady Aim outside current turn");
        if (actor.actions.surge && actor.source.id != mover.source.id)
            throw std::runtime_error("Action Surge allowance outside its turn");
        if (actor.involuntary_overlap && (actor.dead || !shares_occupied_space(actor)))
            throw std::runtime_error("Invalid involuntary checkpoint overlap");
        if (actor.hp > 0 && !actor.dead)
            (actor.source.side == 0 ? party : enemies) = true;
        if (actor.dead)
            continue;
        for (const auto &other : actors_)
        {
            if (other.source.id <= actor.source.id || other.dead ||
                    actor.source.cell != other.source.cell)
                continue;
            const bool in_transit =
                pending() && path_index_ > 0 &&
                path_[path_index_ - 1] == mover.source.cell &&
                (actor.source.id == mover.source.id || other.source.id == mover.source.id);
            if ((in_transit && (actor.source.side == other.source.side || unconscious(actor) ||
                                unconscious(other))) ||
                    actor.involuntary_overlap || other.involuntary_overlap)
                continue;
            throw std::runtime_error("Invalid overlapping checkpoint actors");
        }
    }
    const auto expected = !party ? Outcome::defeat : !enemies ? Outcome::victory : Outcome::ongoing;
    if (outcome_ != expected || (initiative_choices_.empty() && expected == Outcome::ongoing &&
                                 mover.hp == 0 && !champion_move_ && !effect_waiting()))
        throw std::runtime_error("Invalid checkpoint outcome/turn");
    if (!pending() && (!path_.empty() || !reactors_.empty()))
        throw std::runtime_error("Unpaused checkpoint movement");
    if (pending())
        validate_pending_movement();
}

void Session::validate_pending_movement() const
{
    auto mover = actors_[turn_];
    if (effect_reaction_origin_)
    {
        mover.source.cell = effect_reaction_origin_->mover;
        mover.effects.prone = effect_reaction_origin_->prone;
    }
    if (outcome_ != Outcome::ongoing || mover.disengaged)
        throw std::runtime_error("Invalid pending movement");
    std::vector<detail::Occupant> occupants;
    for (const auto &other : actors_)
        if (!other.dead && other.source.id != mover.source.id)
            occupants.push_back({effect_reaction_origin_ && other.source.id == pending()
                                 ? effect_reaction_origin_->source
                                 : champion_move_ && other.source.id == champion_move_->actor
                                 ? champion_move_->origin
                                 : other.source.cell,
                                 other.source.side != mover.source.side, unconscious(other)});
    const detail::MovementGrid grid{board_, mover.source.cell, occupants, mover.effects.prone};
    auto cell = mover.source.cell;
    int remaining =
        effect_reaction_origin_ ? effect_reaction_origin_->movement : movement_left(mover);
    // Only the suffix remains to be travelled. The prefix is history and has
    // already spent its movement budget; charging for it again breaks restores.
    for (auto i = path_index_; i < path_.size(); ++i)
    {
        const auto next = path_[i];
        const auto cost = grid.step_cost(cell, next);
        if (!cost || *cost > remaining)
            throw std::runtime_error("Invalid checkpoint movement step/budget");
        remaining -= *cost;
        cell = next;
    }
    if (!grid.can_stop_at(cell))
        throw std::runtime_error("Invalid checkpoint movement destination");
    for (auto i = reactor_index_; i < reactors_.size(); ++i)
    {
        const auto reactor = std::find_if(actors_.begin(), actors_.end(),
                                          [&](const auto & actor)
        {
            return actor.source.id == reactors_[i];
        });
        // restore_movement already established that every reactor ID exists.
        const auto &actor = *reactor;
        if (effect_reaction_origin_ && i == reactor_index_)
        {
            auto original = actor;
            original.source.cell = effect_reaction_origin_->source;
            if (actor.source.side == mover.source.side || actor.reaction ||
                    !has_weapon_reaction(original, mover.source.cell, path_[path_index_]))
                throw std::runtime_error("Invalid mastery reaction trigger");
            continue;
        }
        if (champion_move_ && i == reactor_index_ && actor.source.id == champion_move_->actor)
        {
            if (actor.source.side == mover.source.side ||
                    distance(champion_move_->origin, mover.source.cell) > def(actor).reach ||
                    distance(champion_move_->origin, path_[path_index_]) <= def(actor).reach)
                throw std::runtime_error("Invalid Champion reaction origin");
            continue;
        }
        if (detail::opportunity_blocked(actor.effects) || actor.hp == 0 ||
                (!actor.reaction && !(graze_ && graze_->actor == actor.source.id &&
                                      i == reactor_index_)) ||
                actor.source.side == mover.source.side ||
                !has_weapon_reaction(actor, mover.source.cell, path_[path_index_]) ||
                !can_see(actor, mover))
            throw std::runtime_error("Invalid checkpoint opportunity attack");
    }
}

void Session::restore_log(std::istream &input)
{
    std::size_t count{};
    input >> count;
    if (!input || count > 80)
        throw std::runtime_error("Invalid checkpoint log");
    log_.clear();
    log_messages_.clear();
    for (std::size_t i = 0; i < count; ++i)
    {
        std::string line;
        input >> std::quoted(line);
        if (!input || line.size() > 1000)
            throw std::runtime_error("Invalid checkpoint log line");
        log_messages_.push_back({line, {}});
        log_.push_back(std::move(line));
    }
}

std::unique_ptr<Session> Session::restore(std::shared_ptr<const Content> content,
        std::string_view bytes)
{
    if (bytes.size() > 4 * 1024 * 1024)
        throw std::runtime_error("Combat checkpoint exceeds limit");
    std::istringstream input{std::string(bytes)};
    std::string magic;
    unsigned version{};
    Identity identity;
    input >> magic >> version >> std::quoted(identity.module) >> std::quoted(identity.version) >>
          std::quoted(identity.content);
    if (!input || magic != "OGCOMBAT" || version > checkpoint_format)
        throw std::runtime_error("Combat checkpoint rules/content version mismatch");
    if (version < checkpoint_format || identity != content->identity)
        throw std::runtime_error(older_save_message);
    Encounter encounter;
    encounter.battlefield = read_checkpoint_board(input);
    std::uint64_t rng{}, revision{};
    unsigned turn{}, round{}, outcome{}, count{};
    input >> rng >> revision >> turn >> round >> outcome >> count;
    if (!input || count < 2 || count > 64 || turn >= count || round == 0 || outcome > 2 ||
            !revision)
        throw std::runtime_error("Invalid checkpoint header");
    std::vector<Actor> actors;
    for (unsigned i = 0; i < count; ++i)
    {
        auto actor = read_checkpoint_actor(input, *content);
        encounter.participants.push_back(actor.source);
        actors.push_back(std::move(actor));
    }
    // Build and validate a separate owned candidate. Any failure destroys it;
    // callers never receive a partially restored session or lose a live one.
    // The constructor checks identities/geometry; saved order and RNG then
    // replace its fresh initiative state before relational checks run.
    auto session = std::make_unique<Session>(content, std::move(encounter), 0, true);
    session->actors_ = std::move(actors);
    session->rng_ = rng;
    session->revision_ = revision;
    session->turn_ = turn;
    session->round_ = round;
    session->outcome_ = static_cast<Outcome>(outcome);
    session->restore_movement(input);
    session->restore_log(input);
    unsigned effects_count{};
    input >> session->scope_ >> session->elapsed_ms_ >> effects_count;
    if (!input || !session->scope_ || effects_count != session->actors_.size())
        throw std::runtime_error("Invalid checkpoint effect header");
    for (auto &a : session->actors_)
    {
        a.effects = detail::read_effects(input);
        if (a.recovery.stable_recovery_due && !detail::healing_blocked(a.effects))
            throw std::runtime_error("Invalid earned recovery checkpoint");
    }
    bool pending_offer{};
    input >> pending_offer;
    if (pending_offer)
    {
        TemporaryHitPoints offer;
        input >> offer.amount >> std::quoted(offer.source_id);
        detail::validate_temporary_hp(offer);
        session->temporary_offer_ = std::move(offer);
    }
    if (!input)
        throw std::runtime_error("Invalid Temporary HP choice checkpoint");
    bool has_items{};
    input >> session->frost_movement_ >> has_items >> session->physical_inventory_;
    if (!input || (session->physical_inventory_ && !has_items))
        throw std::runtime_error("Invalid recovery checkpoint flags");
    session->items_active_ = has_items;
    if (has_items)
    {
        std::size_t count{};
        input >> count;
        if (!input || count > 100000)
            throw std::runtime_error("Invalid held item count");
        session->items_.resize(count);
        for (unsigned index = 0; index < count; ++index)
        {
            auto &item = session->items_[index];
            unsigned id{};
            item.id = index + 1;
            input >> id >> item.origin >> item.equipment_index >> item.inventory_id >>
                  std::quoted(item.definition) >> item.quantity >> item.stowed;
            const auto source = std::find_if(session->actors_.begin(), session->actors_.end(),
                                             [&](const auto & a)
            {
                return a.source.id == item.origin;
            });
            const auto *weapon = detail::weapon(item.definition);
            if (!input || source == session->actors_.end() ||
                    source->source.character_profile.empty() ||
                    (!weapon && item.definition != "shield") || !item.quantity)
                throw std::runtime_error("Invalid physical inventory source");
            const auto original = character_definition(source->source.character_profile);
            if (!item.inventory_id &&
                    (item.equipment_index >= original.equipment_keys.size() ||
                     original.equipment_keys[item.equipment_index] != item.definition))
                throw std::runtime_error("Invalid physical equipment source");
            item.label = {weapon ? std::string(weapon->label) : "Shield", {}};
            input >> item.holder;
            if (!input || id != item.id)
                throw std::runtime_error("Invalid held item identity");
            const auto holder = std::find_if(session->actors_.begin(), session->actors_.end(),
                                             [&](const auto & a)
            {
                return a.source.id == item.holder;
            });
            if (holder == session->actors_.end() || holder->source.character_profile.empty())
                throw std::runtime_error("Invalid held item holder");
        }
        for (auto &a : session->actors_)
            if (!a.source.character_profile.empty())
                a.definition = session->equipped_definition(a, session->items_);
    }
    bool pending_check{};
    input >> pending_check;
    if (pending_check)
    {
        PendingCheck c;
        input >> c.actor >> c.target >> c.natural >> c.surge_spent;
        if (!input)
            throw std::runtime_error("Invalid ability-check checkpoint");
        session->check_choice_ = c;
    }
    bool pending_champion{};
    input >> pending_champion;
    if (pending_champion)
    {
        ChampionMove c;
        input >> c.actor >> c.target >> c.natural >> c.remaining >> c.spell >> c.origin.x >>
              c.origin.y;
        if (!input)
            throw std::runtime_error("Invalid Champion movement checkpoint");
        session->champion_move_ = c;
    }
    bool saved_graze{}, saved_light{}, saved_nick{};
    input >> saved_graze;
    if (saved_graze)
    {
        PendingGraze g;
        input >> g.actor >> g.target >> g.natural;
        session->graze_ = g;
    }
    input >> saved_light >> saved_nick;
    bool has_mastery{};
    input >> has_mastery;
    if (has_mastery)
    {
        PendingMastery m;
        unsigned kind{};
        input >> m.actor >> m.target >> kind >> m.natural >> m.ranged >> m.targeting >>
              std::quoted(m.weapon) >> m.origin.x >> m.origin.y >> m.thrown_item;
        m.kind = static_cast<detail::Mastery>(kind);
        session->mastery_ = std::move(m);
    }
    const auto extra = [&](ChampionMove & c)
    {
        input >> c.triggered >> c.cleave >> c.helpless >> c.trigger_origin.x >>
              c.trigger_origin.y >> c.target_origin.x >> c.target_origin.y;
    };
    unsigned offers{};
    input >> offers;
    if (!input || offers > 2)
        throw std::runtime_error("Invalid queued effect count");
    for (unsigned i = 0; i < offers; ++i)
    {
        ChampionMove c;
        input >> c.actor >> c.target >> c.natural >> c.remaining >> c.spell >> c.origin.x >>
              c.origin.y;
        extra(c);
        session->champion_offers_.push_back(c);
    }
    if (session->champion_move_)
        extra(*session->champion_move_);
    bool origin{};
    input >> origin;
    if (origin)
    {
        EffectReaction r;
        input >> r.actor >> r.source.x >> r.source.y >> r.mover.x >> r.mover.y >> r.movement >>
              r.prone;
        session->effect_reaction_origin_ = r;
    }
    unsigned choices{};
    input >> choices;
    if (!input || choices > session->actors_.size())
        throw std::runtime_error("Invalid Initiative choice count");
    for (unsigned i = 0; i < choices; ++i)
    {
        EntityId id{};
        input >> id;
        session->initiative_choices_.push_back(id);
    }
    bool selecting{};
    input >> selecting;
    if (selecting)
    {
        PendingSelection selection;
        std::size_t count{};
        input >> selection.caster >> selection.verb >> count;
        const auto *spell = detail::find_spell(selection.verb);
        if (!input || !spell || !selects_creatures(*spell) || !count || count > 8)
            throw std::runtime_error("Invalid spell target choice");
        for (std::size_t n = 0; n < count; ++n)
        {
            EntityId id{};
            input >> id;
            selection.chosen.push_back(id);
        }
        const auto known = [&](EntityId id)
        {
            return std::any_of(session->actors_.begin(), session->actors_.end(),
                               [&](const auto & a)
            {
                return a.source.id == id;
            });
        };
        if (!input || !known(selection.caster) ||
                !std::all_of(selection.chosen.begin(), selection.chosen.end(), known) ||
                count >= session->selection_maximum(selection))
            throw std::runtime_error("Invalid spell target choice");
        session->selection_ = std::move(selection);
    }
    const auto known_actor = [&](EntityId id)
    {
        return std::any_of(session->actors_.begin(), session->actors_.end(),
                           [&](const auto & a)
        {
            return a.source.id == id;
        });
    };
    bool aiming{};
    input >> aiming;
    if (aiming)
    {
        PendingArea area;
        input >> area.caster >> area.verb >> area.center.x >> area.center.y;
        const auto *spell = detail::find_spell(area.verb);
        if (!input || !spell || spell->target != detail::SpellTarget::area ||
                !known_actor(area.caster) || !session->board_.contains(area.center) ||
                session->selection_)
            throw std::runtime_error("Invalid area aim");
        session->area_ = std::move(area);
    }
    std::size_t zones{};
    input >> zones;
    if (!input || zones > session->actors_.size())
        throw std::runtime_error("Invalid spell zones");
    for (std::size_t n = 0; n < zones; ++n)
    {
        Zone zone;
        std::size_t cells{};
        unsigned kind{};
        input >> zone.caster >> kind >> zone.ends_ms >> cells;
        // Only Grease keeps time; every other zone ends with Concentration.
        if (!input || !known_actor(zone.caster) || kind > unsigned(ZoneKind::spikes) ||
                (kind == unsigned(ZoneKind::grease)) != (zone.ends_ms != 0) || !cells ||
                cells > session->board_.terrain.size())
            throw std::runtime_error("Invalid spell zone");
        zone.kind = static_cast<ZoneKind>(kind);
        for (std::size_t c = 0; c < cells; ++c)
        {
            Cell cell;
            input >> cell.x >> cell.y;
            if (!input || !session->board_.contains(cell))
                throw std::runtime_error("Invalid spell zone");
            zone.cells.push_back(cell);
        }
        session->zones_.push_back(std::move(zone));
    }
    bool asking{};
    input >> asking;
    if (asking)
    {
        PendingReaction prompt;
        auto &question = prompt.question;
        auto &command = prompt.command;
        unsigned asked{};
        std::size_t answers{};
        input >> question.target >> asked >> question.deflectable >> question.critical >>
              command.actor >> command.target >> std::quoted(command.verb) >>
              command.destination.x >> command.destination.y >> command.item >> answers;
        if (!input || !known_actor(question.target) || asked > unsigned(Asked::cutting) ||
                !known_actor(command.actor) || (command.target && !known_actor(command.target)) ||
                command.verb.empty() || answers > 3 * session->actors_.size())
            throw std::runtime_error("Invalid reaction prompt");
        question.asked = Asked(asked);
        for (std::size_t n = 0; n < answers; ++n)
        {
            ReactionAnswer answer;
            unsigned kind{};
            input >> answer.target >> kind >> std::quoted(answer.verb);
            if (!input || !known_actor(answer.target) || kind > unsigned(Asked::cutting) ||
                    (answer.verb != "shield" && answer.verb != "deflect" &&
                     answer.verb != "redirect" && answer.verb != "rebuke" &&
                     answer.verb != "inspire" && answer.verb != "cutting" &&
                     answer.verb != "decline"))
                throw std::runtime_error("Invalid reaction prompt");
            answer.asked = Asked(kind);
            prompt.answers.push_back(std::move(answer));
        }
        session->reaction_prompt_ = std::move(prompt);
    }
    if (!input)
        throw std::runtime_error("Invalid checkpoint continuation");
    for (auto &a : session->actors_)
        refresh_form(a);
    session->light_active_ = saved_light;
    session->nick_active_ = saved_nick;
    session->validate_light();
    session->validate_restored_state();
    input >> std::ws;
    if (!input.eof())
        throw std::runtime_error("Trailing checkpoint data");
    return session;
}

class Module final : public RulesModule
{
  public:
    explicit Module(Content content) : content_(std::make_shared<const Content>(std::move(content)))
    {
    }

    Identity identity() const override
    {
        return content_->identity;
    }

    std::vector<std::string> supported_features() const override
    {
        return {"alert",
                "skilled",
                "light",
                "two_weapon_fighting",
                "loading",
                "scholar",
                "arcane_recovery",
                "champion",
                "stabilize",
                "tactical_mind",
                "chill_touch",
                "shocking_grasp",
                "eldritch_blast",
                "initiative",
                "movement",
                "unconscious_enemy_transit",
                "melee",
                "ranged",
                "critical_hits",
                "dodge",
                "dash",
                "disengage",
                "opportunity_attacks",
                "facing",
                "turn_opportunity_attacks",
                "death_saves",
                "second_wind",
                "action_surge",
                "cunning_dash",
                "cunning_disengage",
                "pack_tactics",
                "aggressive",
                "advantage_damage",
                "sneak_attack",
                "steady_aim",
                "great_weapon_fighting",
                "ray_of_frost",
                "fire_bolt",
                "poison_spray",
                "sacred_flame",
                "cure_wounds",
                "magic_missile",
                "healing_word",
                "scorching_ray",
                "level_two_slots",
                "manual_advancement",
                "ability_score_improvement",
                "defense",
                "archery",
                "savage_attacker",
                "saving_throws",
                "blinded",
                "blindness",
                "timed_effects",
                "versatile",
                "feature_grants",
                "training_grants",
                "rest_resources",
                "hit_dice",
                "recovery_clocks",
                "campaign_recovery",
                "typed_damage",
                "damage_affinities",
                "dwarven_poison_resistance",
                "temporary_hp",
                "adrenaline_rush",
                "heavy_weapons",
                "weapon_catalog",
                "armor_catalog",
                "wizard_spellbook",
                "checkpoint"};
    }

    std::unique_ptr<CombatSession> create(Encounter e, std::uint64_t seed) const override
    {
        return std::make_unique<Session>(content_, std::move(e), seed);
    }

    std::unique_ptr<CombatSession> restore(std::string_view checkpoint) const override
    {
        return Session::restore(content_, checkpoint);
    }

    unsigned experience_for_level(unsigned level) const override
    {
        static constexpr unsigned thresholds[] {0, 0, 300, 900, 2700};
        if (level < 1 || level > 4)
            throw std::runtime_error("Unsupported character level");
        return thresholds[level];
    }

    bool advance_character(CharacterSheet &sheet, VitalState &state) const override
    {
        return advance_character(sheet, state, default_advancement(sheet));
    }

    std::vector<TrainingChoiceGroup> training_options(const CharacterSheet &sheet) const override
    {
        if (sheet.character_class == "Wizard" && sheet.level >= 2)
            return {detail::scholar_options(sheet.grants)};
        if (sheet.character_class == "Fighter" && sheet.level >= 4)
            return {detail::mastery_options("fighter", 4, sheet.grants)};
        if (sheet.character_class == "Sorcerer" && sheet.level >= 2)
            return {detail::metamagic_options()};
        if (sheet.character_class == "Bard" && sheet.level >= 3)
            return {detail::lore_options(sheet.grants)};
        if (sheet.character_class == "Druid" && sheet.level >= 3)
            return {detail::land_options()};
        if (sheet.character_class == "Barbarian" && sheet.level >= 3)
        {
            std::vector<TrainingChoiceGroup> groups{detail::primal_knowledge_options(sheet.grants)};
            if (sheet.level >= 4)
                groups.push_back(detail::mastery_options("barbarian", 4, sheet.grants));
            return groups;
        }
        return {};
    }

    std::optional<TrainingReplacementOptions>
    rest_training_options(const CharacterSheet &sheet) const override
    {
        const auto klass = detail::grant_source_id(sheet.character_class);
        auto group = detail::mastery_options(klass, 1);
        if (!group.count)
            return {};
        if (sheet.level >= 4)
            group.count += detail::mastery_options(klass, 4).count;
        const auto choices = detail::mastery_choices(sheet.grants, klass, sheet.level);
        std::vector<std::string> selected;
        for (const auto &[source, values] : choices)
            selected.insert(selected.end(), values.begin(), values.end());
        if (selected.size() != group.count)
            return {}; // Nothing to replace until every mastery is chosen.
        group.id = "weapon_mastery";
        return TrainingReplacementOptions{std::move(group), std::move(selected),
                                          detail::mastery_replacements(klass)};
    }

    TrainingChoices replace_rest_training(CharacterSheet &sheet,
                                          std::span<const std::string> selected) const override
    {
        if (!rest_training_options(sheet))
            throw std::runtime_error("No completed Weapon Mastery training to replace");
        const auto klass = detail::grant_source_id(sheet.character_class);
        auto candidate = sheet;
        candidate.grants = detail::replace_masteries(sheet.grants, klass, sheet.level, selected);
        candidate.training = detail::training_profile(candidate.grants, klass,
            detail::grant_source_id(candidate.background),
            candidate.level, candidate.scores);
        auto choices = detail::mastery_choices(candidate.grants, klass, candidate.level);
        (void)character_profile(candidate, {});
        sheet = std::move(candidate);
        return choices;
    }

    AdvancementOptions advancement_options(const CharacterSheet &sheet) const override
    {
        if (sheet.level >= 4 ||
                (sheet.character_class != "Fighter" && sheet.character_class != "Cleric" &&
                 sheet.character_class != "Wizard" && sheet.character_class != "Rogue" &&
                 sheet.character_class != "Paladin" && sheet.character_class != "Ranger" &&
                 sheet.character_class != "Barbarian" && sheet.character_class != "Monk" &&
                 sheet.character_class != "Sorcerer" && sheet.character_class != "Warlock" &&
                 sheet.character_class != "Bard" && sheet.character_class != "Druid"))
            return {};
        AdvancementOptions result;
        result.level = sheet.level + 1;
        if (sheet.character_class == "Fighter" ||
                ((sheet.character_class == "Paladin" || sheet.character_class == "Ranger") &&
                 result.level == 2))
        {
            if (sheet.character_class == "Fighter")
                result.fighting_styles.push_back(
            {"keep", "Keep current", "Retain the class-granted Fighting Style."});
            for (auto style : detail::fighting_styles())
            {
                style.available =
                    (sheet.character_class != "Fighter" ||
                     std::any_of(sheet.grants.begin(), sheet.grants.end(),
                                 [](const auto & g)
                {
                    return g.source_id == "class:fighter:fighting_style";
                })) &&
                std::none_of(sheet.grants.begin(), sheet.grants.end(),
                             [&](const auto & g)
                {
                    return g.id == "feat:" + style.id;
                });
                result.fighting_styles.push_back(std::move(style));
            }
            // SRD 5.2.1 p. 54: the Paladin's alternative to a Fighting Style feat.
            if (sheet.character_class == "Paladin")
                result.fighting_styles.push_back(
            {
                "blessed_warrior", "Blessed Warrior",
                "Learn two Cleric cantrips; Charisma is your spellcasting ability for them."
            });
            // SRD 5.2.1 p. 59: the Ranger's alternative.
            if (sheet.character_class == "Ranger")
                result.fighting_styles.push_back(
            {
                "druidic_warrior", "Druidic Warrior",
                "Learn two Druid cantrips; Wisdom is your spellcasting ability for them."
            });
        }
        if (sheet.character_class == "Wizard" && result.level == 2)
            result.training = {detail::scholar_options(sheet.grants)};
        if (sheet.character_class == "Fighter" && result.level == 4)
            result.training = {detail::mastery_options("fighter", 4, sheet.grants)};
        if (sheet.character_class == "Barbarian" && result.level == 3)
            result.training = {detail::primal_knowledge_options(sheet.grants)};
        if (sheet.character_class == "Sorcerer" && result.level == 2)
            result.training = {detail::metamagic_options()};
        if (sheet.character_class == "Bard" && result.level == 3)
            result.training = {detail::lore_options(sheet.grants)};
        if (sheet.character_class == "Druid" && result.level == 3)
            result.training = {detail::land_options()};
        if (sheet.character_class == "Warlock" && result.level == 2)
            result.training = {detail::invocation_options(2, sheet.grants,
                                                          "class:warlock:invocations:2")};
        if (sheet.character_class == "Barbarian" && result.level == 4)
            result.training = {detail::mastery_options("barbarian", 4, sheet.grants)};
        // The Hunter is the SRD's only Ranger subclass; Hunter's Prey is its choice.
        if (sheet.character_class == "Ranger" && result.level == 3)
            result.training = {{
                "subclass:ranger:hunter", "Hunter's Prey", 1,
                {   {
                        "colossus_slayer", "Colossus Slayer",
                        "Once per turn, a weapon hit deals 1d8 extra damage to a creature missing any Hit Points."
                    },
                    {
                        "horde_breaker", "Horde Breaker",
                        "Once per turn, after a weapon attack, attack another creature within 5 feet of the first target."
                    }
                },
                TrainingChoiceControl::single_selection
            }
        };
        result.description =
            "Fixed-average HP growth. Resources gain only their new capacity;\nexisting expenditure remains.";
        if (sheet.character_class == "Cleric" && result.level == 2)
            result.description =
                "Channel Divinity: two uses, one back on a Short Rest, for Divine Spark (heal or harm 1d8 + Wisdom within 30 feet) or Turn Undead.";
        if (sheet.character_class == "Wizard" && result.level == 3)
            result.description =
                "Evoker: Potent Cantrip deals half damage when a cantrip misses or is saved against; Sculpt Spells spares up to 1 + the spell's level allies in your Evocation areas.";
        if (sheet.character_class == "Cleric" && result.level == 3)
            result.description =
                "Life Domain: Disciple of Life adds 2 + the slot level to healing spells; Preserve Life (Channel Divinity) restores five times your level among Bloodied allies within 30 feet; Bless, Cure Wounds and Lesser Restoration are always prepared.";
        if (sheet.character_class == "Fighter" && result.level == 3)
            result.description =
                "Champion: weapon/unarmed criticals on 19–20.\nAdvantage on Initiative and Strength (Athletics).\nCritical hit: optional half-Speed move, no opportunity attacks.";
        if (sheet.character_class == "Rogue" && result.level >= 3)
            result.description =
                "Sneak Attack: 2d6. Steady Aim: Bonus Action; next attack roll has Advantage, Speed becomes 0.\nThief: Fast Hands has no use until magic items arrive. Hide and weapon mastery remain unavailable.";
        if (sheet.character_class == "Paladin")
            result.description =
                "Prepared spells, Lay On Hands and fixed HP advancement; Fighting Style or Blessed Warrior and Paladin's Smite at level two; Channel Divinity, the Oath of Devotion and Sacred Weapon at level three. Level four grants an available feat or ability points.";
        if (sheet.character_class == "Druid")
            result.description = "Prepared Druid spells and a Primal Order: Magician (an extra cantrip) or Warden (Martial weapons and Medium armor); the Circle of the Land at level three with its land's Circle Spells always prepared. Level four grants a third cantrip and an available feat or ability points.";
        if (sheet.character_class == "Bard")
            result.description = "Bardic spellcasting with prepared Bard spells; Jack of All Trades at level two; the College of Lore at level three with three more skills. Level four grants a third cantrip and an available feat or ability points.";
        if (sheet.character_class == "Warlock")
            result.description = "Pact Magic: prepared Warlock spells cast from slots of one level that return on a Short Rest; Magical Cunning at level two; the Fiend Patron at level three: Dark One's Blessing and Burning Hands, Command and Scorching Ray always prepared. Level four grants a third cantrip and an available feat or ability points.";
        if (sheet.character_class == "Sorcerer")
            result.description = "Prepared Sorcerer spells and Innate Sorcery; Font of Magic's Sorcery Points at level two; Draconic Sorcery at level three: Draconic Resilience (Hit Points and unarmored AC 10 + Dexterity + Charisma) and Chromatic Orb, Command and Dragon's Breath always prepared. Level four grants a fifth cantrip and an available feat or ability points.";
        if (sheet.character_class == "Monk")
            result.description = "Martial Arts and Unarmored Defense; Monk's Focus (Flurry of Blows, Patient Defense, Step of the Wind), Unarmored Movement and Uncanny Metabolism at level two; Deflect Attacks and the Warrior of the Open Hand at level three. Level four grants an available feat or ability points.";
        if (sheet.character_class == "Barbarian")
            result.description = "Rage, Unarmored Defense and Weapon Mastery; Danger Sense and Reckless Attack at level two; the Berserker with Frenzy, Primal Knowledge and a third Rage at level three. Level four grants an available feat or ability points and a third Weapon Mastery.";
        if (sheet.character_class == "Ranger")
            result.description =
                "Prepared spells with Favored Enemy and fixed HP advancement; Fighting Style or Druidic Warrior at level two; the Hunter with Hunter's Lore and Hunter's Prey at level three. Level four grants an available feat or ability points.";
        if (result.level == 4)
            result.feats =
        {
            {
                "ability_score_improvement", "Ability points",
                "Add 2 to one ability or 1 to two abilities; maximum 20."
            },
            {
                "defense", "Defense",
                "+1 AC while wearing armor. Requires the Fighting Style feature.",
                detail::has_grant(sheet.grants, "feature:fighting_style") &&
                !detail::has_grant(sheet.grants, "feat:defense")
            },
            {
                "savage_attacker", "Savage Attacker",
                "Once per turn on a weapon hit, roll weapon damage twice and keep the higher roll.",
                !detail::has_grant(sheet.grants, "feat:savage_attacker")
            },
            {
                "magic_initiate", "Magic Initiate",
                "Unavailable: its complete spell-selection feature is not implemented.", false
            },
            {
                "archery", "Archery",
                "+2 to attack rolls with Ranged weapons. Requires Fighting Style.",
                detail::has_grant(sheet.grants, "feature:fighting_style") &&
                !detail::has_grant(sheet.grants, "feat:archery")
            },
            {
                "great_weapon_fighting", "Great Weapon Fighting",
                "Treat damage dice showing 1 or 2 as 3 with an eligible Melee weapon held in two hands.",
                detail::has_grant(sheet.grants, "feature:fighting_style") &&
                !detail::has_grant(sheet.grants, "feat:great_weapon_fighting")
            },
            {
                "two_weapon_fighting", "Two-Weapon Fighting",
                "Add your ability modifier to the extra attack granted by the Light property.",
                detail::has_grant(sheet.grants, "feature:fighting_style") &&
                !detail::has_grant(sheet.grants, "feat:two_weapon_fighting")
            },
            {
                "alert", "Alert",
                "Add proficiency to Initiative; optionally swap Initiative with an eligible ally before the first turn.",
                !detail::has_grant(sheet.grants, "feat:alert")
            },
            // Repeatable: availability does not exclude already holding it. Only
            // one level-four entitlement exists, so a second acquisition is not
            // reachable yet; the entitlement check rejects reusing this one.
            {
                "skilled", "Skilled",
                "Gain proficiency in any three skills of your choice."
            }
        };
        if (sheet.character_class == "Paladin")
            result.spells =
        {
            {"cure_wounds", "Cure Wounds", "Action; touch; heals 2d8 + Charisma modifier."},
            {
                "divine_smite", "Divine Smite",
                "Bonus Action right after a melee hit: 2d8 Radiant damage, 3d8 against a Fiend or an Undead."
            },
            {
                "searing_smite", "Searing Smite",
                "Bonus Action right after a melee hit: 1d6 Fire damage, then 1d6 at the start of each of the target's turns until it succeeds on a Constitution save."
            }
        };
        if (sheet.character_class == "Cleric")
            result.spells =
        {
            {"cure_wounds", "Cure Wounds", "Action; touch; heals 2d8 + Wisdom modifier."},
            {
                "healing_word", "Healing Word",
                "Bonus action; 60 feet; heals 2d4 + Wisdom modifier."
            },
            {
                "blindness", "Blindness",
                "Blindness/Deafness (blindness option): Constitution save; repeat at end of turn; up to 1 minute. Level 2 slot.",
                result.level >= 3
            },
            {
                "inflict_wounds", "Inflict Wounds",
                "Action; touch; Constitution save; 2d10 Necrotic damage, half on a success, +1d10 from a level 2 slot."
            },
            {"bless", "Bless", "Action; 30 feet; up to three creatures add 1d4 to attack rolls and saving throws. Concentration, up to 1 minute."},
            {
                "protection_from_evil_and_good", "Protection from Evil and Good",
                "Action; touch; Aberrations, Celestials, Elementals, Fey, Fiends and Undead attack the creature with Disadvantage. Concentration, up to 10 minutes."
            }
        };
        if (sheet.character_class == "Wizard")
            result.spells =
        {
            {"magic_missile", "Magic Missile", "Action; 120 feet; three darts at one target."},
            {
                "scorching_ray", "Scorching Ray",
                "Action; 120 feet; three spell attacks at one target. Requires level 3.",
                result.level >= 3
            },
            {
                "blindness", "Blindness",
                "Blindness/Deafness (blindness option): Constitution save; repeat at end of turn; up to 1 minute. Level 2 slot.",
                result.level >= 3
            },
            {"shield", "Shield", "Unavailable: spell reactions are not implemented.", false}
        };
        return result;
    }

    AdvancementOptions advancement_options(const CharacterSheet &sheet,
                                           const AdvancementChoice &choice) const override
    {
        auto options = advancement_options(sheet);
        // Skilled's proficiency page exists only while Skilled is the selection,
        // so switching away from it drops the group and its unconfirmed picks.
        if (choice.feat == "skilled" && options.level == 4)
            options.training.push_back(detail::skilled_options(sheet.grants));
        if (choice.fighting_style && sheet.character_class == "Fighter")
        {
            const auto source = "class:fighter:fighting_style";
            for (auto &feat : options.feats)
                if (feat.id == "defense" || feat.id == "archery" ||
                        feat.id == "great_weapon_fighting" || feat.id == "two_weapon_fighting")
                    feat.available =
                        feat.id != *choice.fighting_style &&
                        std::none_of(sheet.grants.begin(), sheet.grants.end(),
                                     [&](const auto & g)
                {
                    return g.id == "feat:" + feat.id && g.source_id != source;
                });
        }
        return options;
    }

    AdvancementChoice default_advancement(const CharacterSheet &sheet) const override
    {
        AdvancementChoice choice;
        const auto options = advancement_options(sheet);
        if (!options.level)
            return choice;
        if (options.level == 2 &&
                (sheet.character_class == "Paladin" || sheet.character_class == "Ranger"))
            choice.fighting_style = "defense";
        choice.spells = sheet.prepared_spells;
        for (const auto &group : options.training)
            for (const auto &option : group.options)
                if (choice.training[group.id].size() < group.count)
                    choice.training[group.id].push_back(option.id);
        if (choice.spells.empty() && sheet.character_class == "Wizard")
            choice.spells = {"magic_missile"};
        if (detail::prepares_spells(sheet.character_class))
        {
            auto next = sheet;
            next.level = options.level;
            choice.spell_learning.emplace();
            for (const auto &group :
                    detail::spell_choice_options(next, SpellChoiceContext::advancement).learning)
            {
                auto &values = (*choice.spell_learning)[group.id];
                for (const auto &option : group.options)
                {
                    if (values.size() == group.count)
                        break;
                    values.push_back(option.id);
                }
            }
            detail::apply_spell_choices(next, SpellChoices{*choice.spell_learning, {}, {}, {}},
                                        SpellChoiceContext::advancement, false);
            const auto preparation =
                detail::spell_choice_options(next, SpellChoiceContext::advancement);
            choice.spells = preparation.locked_prepared;
            for (const auto &spell : preparation.preparation)
                if (choice.spells.size() < preparation.prepared_count &&
                        std::find(choice.spells.begin(), choice.spells.end(), spell.id) ==
                        choice.spells.end())
                    choice.spells.push_back(spell.id);
        }
        if (options.level == 4)
        {
            choice.feat = "ability_score_improvement";
            const unsigned primary =
                (sheet.character_class == "Fighter" || sheet.character_class == "Paladin") ? 0
                : (sheet.character_class == "Cleric" || sheet.character_class == "Druid") ? 4
                : (sheet.character_class == "Rogue" || sheet.character_class == "Ranger")  ? 1
                : (sheet.character_class == "Sorcerer" || sheet.character_class == "Warlock" ||
                   sheet.character_class == "Bard") ? 5
                : 3;
            unsigned remaining = 2;
            for (unsigned n = 0; n < 6 && remaining; ++n)
            {
                const auto index = (primary + n) % 6;
                choice.abilities[index] =
                    std::min(remaining, unsigned(std::max(0, 20 - sheet.scores[index])));
                remaining -= choice.abilities[index];
            }
        }
        return choice;
    }

    CharacterSheet spell_choice_sheet(const CharacterSheet &sheet,
                                      const AdvancementChoice &choice) const override
    {
        auto next = sheet;
        ++next.level;
        if ((sheet.character_class == "Paladin" || sheet.character_class == "Ranger") &&
                next.level == 2)
            next.grants.push_back({"feature:fighting_style",
                                   "class:" + detail::grant_source_id(sheet.character_class),
                                   2,
                                   {}});
        if (choice.fighting_style)
        {
            const auto source =
                "class:" + detail::grant_source_id(sheet.character_class) + ":fighting_style";
            std::erase_if(next.grants,
                          [&](const auto & g)
            {
                return g.source_id == source;
            });
            next.grants.push_back(
            {fighting_style_grant(*choice.fighting_style), source, unsigned(next.level), {}});
        }
        return next;
    }

    bool advance_character(CharacterSheet &sheet, VitalState &state,
                           const AdvancementChoice &choice) const override
    {
        const auto options = advancement_options(sheet, choice);
        if (!options.level)
            return false;
        const bool half_style =
            (sheet.character_class == "Paladin" || sheet.character_class == "Ranger") &&
            options.level == 2;
        if (half_style && !choice.fighting_style)
            throw std::runtime_error("Choose a Fighting Style");
        if (choice.fighting_style &&
                std::none_of(options.fighting_styles.begin(), options.fighting_styles.end(),
                             [&](const auto & f)
    {
        return f.id == *choice.fighting_style && f.id != "keep" && f.available;
    }))
        throw std::runtime_error("Choose an eligible Fighting Style");
        const auto old = character_definition(character_profile(sheet, {}).data);
        unsigned points = 0;
        for (auto n : choice.abilities)
        {
            if (n > 2)
                throw std::runtime_error("An ability increase cannot exceed 2");
            points += n;
        }
        if (options.level == 4)
        {
            const auto feat = std::find_if(options.feats.begin(), options.feats.end(),
                                           [&](const auto & f)
            {
                return f.id == choice.feat && f.available;
            });
            if (feat == options.feats.end())
                throw std::runtime_error("Choose an available feat or ability points");
            if (points != (choice.feat == "ability_score_improvement" ? 2u : 0u))
                throw std::runtime_error("Assign exactly two ability points, or choose a feat");
            // The training loop below checks the count only when the group is
            // present, so require the picks rather than granting Skilled empty.
            if (choice.feat == "skilled" && !choice.training.contains("feat:skilled"))
                throw std::runtime_error("Choose exactly three Skilled proficiencies");
        }
        else if (!choice.feat.empty() || points)
            throw std::runtime_error("Feats and ability points are available at level 4");
        // Classes that prepare spells validate their choices through
        // apply_spell_choices below, against the whole class list.
        std::set<std::string> selected;
        for (const auto &spell : choice.spells)
        {
            if (!selected.insert(spell).second ||
                    (!detail::prepares_spells(sheet.character_class) &&
                     std::none_of(options.spells.begin(), options.spells.end(),
                                  [&](const auto & s)
        {
            return s.id == spell && s.available;
        })))
            throw std::runtime_error("Choose only available, distinct spells");
        }
        if (!options.spells.empty() && choice.spells.empty())
            throw std::runtime_error("Choose at least one supported spell");
        Actor actor;
        actor.definition = old;
        actor.winds = old.winds;
        actor.slots = old.slots;
        actor.slots2 = old.slots2;
        restore_vitals(actor, state);
        auto next = spell_choice_sheet(sheet, choice);
        for (const auto &[id, values] : choice.training)
        {
            const auto group = std::find_if(options.training.begin(), options.training.end(),
                                            [&](const auto & g)
            {
                return g.id == id;
            });
            if (group == options.training.end() || values.size() != group->count ||
                    std::set<std::string>(values.begin(), values.end()).size() != values.size() ||
                    std::any_of(values.begin(), values.end(),
                                [&](const auto & value)
        {
            return std::none_of(group->options.begin(), group->options.end(),
                                [&](const auto & o)
            {
                return o.id == value;
            });
            }))
            throw std::runtime_error("Invalid advancement training choice");
            for (const auto &value : values)
                next.grants.push_back(
            {
                // Skilled spans both catalogs, so its options carry the prefixed id.
                id == "feat:skilled"             ? value
                : id == "class:wizard:scholar" ? "expertise:" + value
                : id == "subclass:ranger:hunter" ? "prey:" + value
                : id == "class:barbarian:primal_knowledge" ? "skill:" + value
                : id == "class:sorcerer:metamagic" ? "metamagic:" + value
                : id == "class:warlock:invocations:2" ? "invocation:" + value
                : id == "subclass:bard:lore" ? "skill:" + value
                : id == "subclass:druid:land" ? "land:" + value
                : "mastery:" + value,
                id,
                unsigned(next.level),
                {}});
        }
        if (next.character_class == "Warlock" && next.level == 2)
            next.grants.push_back({"feature:magical_cunning", "class:warlock", 2, {}});
        if (next.character_class == "Bard" && next.level == 2)
            next.grants.push_back({"feature:jack_of_all_trades", "class:bard", 2, {}});
        // The Circle of the Land is the SRD's only Druid subclass.
        if (next.character_class == "Druid" && next.level == 2)
            next.grants.push_back({"feature:wild_shape", "class:druid", 2, {}});
        if (next.character_class == "Druid" && next.level == 3)
        {
            next.grants.push_back({"subclass:land", "class:druid", 3, {}});
            next.grants.push_back({"feature:circle_spells", "subclass:druid:land", 3, {}});
            next.grants.push_back({"feature:lands_aid", "subclass:druid:land", 3, {}});
        }
        // The College of Lore is the SRD's only Bard subclass.
        if (next.character_class == "Bard" && next.level == 3)
        {
            next.grants.push_back({"subclass:lore", "class:bard", 3, {}});
            next.grants.push_back({"feature:bonus_proficiencies", "subclass:bard:lore", 3, {}});
            next.grants.push_back({"feature:cutting_words", "subclass:bard:lore", 3, {}});
        }
        // The Fiend Patron is the SRD's only Warlock subclass.
        if (next.character_class == "Warlock" && next.level == 3)
        {
            next.grants.push_back({"subclass:fiend", "class:warlock", 3, {}});
            next.grants.push_back({"feature:dark_ones_blessing", "subclass:warlock:fiend", 3, {}});
            next.grants.push_back({"feature:fiend_spells", "subclass:warlock:fiend", 3, {}});
        }
        if (next.character_class == "Sorcerer" && next.level == 2)
        {
            next.grants.push_back({"feature:font_of_magic", "class:sorcerer", 2, {}});
            next.grants.push_back({"feature:metamagic", "class:sorcerer", 2, {}});
        }
        // Draconic Sorcery is the SRD's only Sorcerer subclass.
        if (next.character_class == "Sorcerer" && next.level == 3)
        {
            next.grants.push_back({"subclass:draconic", "class:sorcerer", 3, {}});
            next.grants.push_back({"feature:draconic_resilience", "subclass:sorcerer:draconic", 3, {}});
            next.grants.push_back({"feature:draconic_spells", "subclass:sorcerer:draconic", 3, {}});
        }
        if (next.character_class == "Monk" && next.level == 2)
        {
            next.grants.push_back({"feature:monks_focus", "class:monk", 2, {}});
            next.grants.push_back({"feature:unarmored_movement", "class:monk", 2, {}});
            next.grants.push_back({"feature:uncanny_metabolism", "class:monk", 2, {}});
        }
        // The Warrior of the Open Hand is the SRD's only Monk subclass.
        if (next.character_class == "Monk" && next.level == 3)
        {
            next.grants.push_back({"feature:deflect_attacks", "class:monk", 3, {}});
            next.grants.push_back({"subclass:open_hand", "class:monk", 3, {}});
            next.grants.push_back({"feature:open_hand_technique", "subclass:monk:open_hand", 3, {}});
        }
        if (next.character_class == "Barbarian" && next.level == 2)
        {
            next.grants.push_back({"feature:danger_sense", "class:barbarian", 2, {}});
            next.grants.push_back({"feature:reckless_attack", "class:barbarian", 2, {}});
        }
        // The Berserker is the SRD's only Barbarian subclass.
        if (next.character_class == "Barbarian" && next.level == 3)
        {
            next.grants.push_back({"subclass:berserker", "class:barbarian", 3, {}});
            next.grants.push_back({"feature:frenzy", "subclass:barbarian:berserker", 3, {}});
            next.grants.push_back({"feature:primal_knowledge", "class:barbarian", 3, {}});
        }
        // The Thief is the SRD's only Rogue subclass. Fast Hands waits for magic
        // items to have a use; Second-Story Work is cut (SRD-DECISIONS).
        if (next.character_class == "Rogue" && next.level == 3)
        {
            next.grants.push_back({"feature:steady_aim", "class:rogue", 3, {}});
            next.grants.push_back({"subclass:thief", "class:rogue", 3, {}});
            next.grants.push_back({"feature:fast_hands", "subclass:rogue:thief", 3, {}});
        }
        if (next.character_class == "Rogue" && next.level == 2)
            next.grants.push_back({"feature:cunning_action", "class:rogue", 2, {}});
        if (next.character_class == "Fighter" && next.level == 2)
        {
            next.grants.push_back({"feature:action_surge", "class:fighter", 2, {}});
            next.grants.push_back({"feature:tactical_mind", "class:fighter", 2, {}});
        }
        if (next.character_class == "Cleric" && next.level == 2)
            next.grants.push_back({"feature:channel_divinity", "class:cleric", 2, {}});
        // The Life Domain is the SRD's only Cleric subclass.
        if (next.character_class == "Cleric" && next.level == 3)
        {
            next.grants.push_back({"subclass:life", "class:cleric", 3, {}});
            next.grants.push_back({"feature:disciple_of_life", "subclass:cleric:life", 3, {}});
            next.grants.push_back({"feature:preserve_life", "subclass:cleric:life", 3, {}});
        }
        // The Evoker is the SRD's only Wizard subclass.
        if (next.character_class == "Wizard" && next.level == 3)
        {
            next.grants.push_back({"subclass:evoker", "class:wizard", 3, {}});
            next.grants.push_back({"feature:potent_cantrip", "subclass:wizard:evoker", 3, {}});
            next.grants.push_back({"feature:sculpt_spells", "subclass:wizard:evoker", 3, {}});
        }
        if (next.character_class == "Ranger" && next.level == 3)
        {
            next.grants.push_back({"subclass:hunter", "class:ranger", 3, {}});
            next.grants.push_back({"feature:hunters_lore", "subclass:ranger:hunter", 3, {}});
        }
        // The Oath of Devotion is the SRD's only Paladin subclass.
        if (next.character_class == "Paladin" && next.level == 3)
        {
            next.grants.push_back({"feature:channel_divinity", "class:paladin", 3, {}});
            next.grants.push_back({"subclass:devotion", "class:paladin", 3, {}});
            next.grants.push_back(
            {"feature:sacred_weapon", "subclass:paladin:devotion", 3, {}});
        }
        if (next.character_class == "Fighter" && next.level == 3)
        {
            next.grants.push_back({"subclass:champion", "class:fighter", 3, {}});
            next.grants.push_back(
            {"feature:improved_critical", "subclass:fighter:champion", 3, {}});
            next.grants.push_back(
            {"feature:remarkable_athlete", "subclass:fighter:champion", 3, {}});
        }
        for (unsigned n = 0; n < 6; ++n)
        {
            next.scores[n] += choice.abilities[n];
            next.bonuses[n] += choice.abilities[n];
            if (next.scores[n] > 20)
                throw std::runtime_error("Ability scores cannot exceed 20");
            next.modifiers[n] = ability_modifier(next.scores[n]);
            next.saving_throws[n] = next.modifiers[n] + (next.save_proficiencies[n] ? 2 : 0);
        }
        if (points)
        {
            AbilityAdjustment adjustment{"feat:" + choice.feat,
                                         "Level " + std::to_string(next.level) +
                                         " Ability Score Improvement",
                                         unsigned(next.level)};
            for (unsigned n = 0; n < 6; ++n)
                adjustment.bonuses[n] = int(choice.abilities[n]);
            adjustment.label_message = {"Level {level} Ability Score Improvement",
                {{"level", std::to_string(next.level)}}
            };
            next.ability_adjustments.push_back(std::move(adjustment));
        }
        if (!choice.feat.empty())
            next.grants.push_back(detail::advancement_grant(
                                      detail::grant_source_id(sheet.character_class), next.level, choice));
        next.prepared_spells = choice.spells;
        if (choice.spell_learning)
        {
            next.prepared_spells = sheet.prepared_spells;
            detail::apply_spell_choices(next,
                                        SpellChoices{*choice.spell_learning, choice.spells, {}, {}},
                                        SpellChoiceContext::advancement);
        }
        else if (detail::prepares_spells(sheet.character_class))
            throw std::runtime_error("Independent spell learning choices are required");
        next.training = detail::training_profile(
                            next.grants, detail::grant_source_id(next.character_class),
                            detail::grant_source_id(next.background), next.level, next.scores);
        next.hit_point_modifiers.push_back(next.modifiers[2]);
        next.hit_points =
            maximum_hit_points(next.hit_die, next.race == "Dwarf", next.hit_point_modifiers,
                               next.character_class == "Sorcerer");
        if (next.race == "Dwarf")
        {
            next.racial_modifiers.replace(0, next.racial_modifiers.find('\n'),
                                          "Dwarven Toughness: +" + std::to_string(next.level) +
                                          " maximum HP.");
            for (auto &message : next.racial_messages)
                if (message.source == "Dwarven Toughness: +{hp} maximum HP.")
                    message.arguments = {{"hp", std::to_string(next.level)}};
        }
        const int growth = next.hit_points - sheet.hit_points;
        next.hp_explanation =
            "Level " + std::to_string(next.level) + ": " + std::to_string(next.hit_points) +
            " maximum HP; gain " + std::to_string(growth) +
            ". Fixed-average Hit Die growth includes Constitution and any retroactive Constitution increase.";
        next.hp_messages =
        {
            {
                "Level {level}: {hp} maximum HP; gain {growth}. Fixed-average Hit Die growth includes Constitution and any retroactive Constitution increase.",
                {   {"level", std::to_string(next.level)},
                    {"hp", std::to_string(next.hit_points)},
                    {"growth", std::to_string(growth)}
                }
            }
        };
        if (next.character_class == "Rogue" && next.level == 2)
        {
            next.class_modifiers +=
                "\nCunning Action: Dash or Disengage as a Bonus Action on your turn. Hide remains unavailable.";
            next.class_messages.push_back(
            {
                "Cunning Action: Dash or Disengage as a Bonus Action on your turn. Hide remains unavailable.",
                {}});
        }
        if (next.character_class == "Rogue" && next.level == 3)
        {
            next.class_modifiers +=
                "\nSneak Attack: 2d6. Steady Aim: Bonus Action; next attack roll has Advantage, Speed becomes 0.";
            next.class_messages.push_back(
            {
                "Sneak Attack: 2d6. Steady Aim: Bonus Action; next attack roll has Advantage, Speed becomes 0.",
                {}});
        }
        if (next.character_class == "Paladin" || next.character_class == "Ranger" ||
                next.character_class == "Barbarian" || next.character_class == "Monk" ||
                next.character_class == "Sorcerer" || next.character_class == "Warlock" ||
                next.character_class == "Bard" || next.character_class == "Druid")
        {
            const std::string note =
                next.character_class == "Druid"
                ? "Prepared Druid spells and a Primal Order: Magician (an extra cantrip) or Warden (Martial weapons and Medium armor); the Circle of the Land at level three with its land's Circle Spells always prepared. Level four grants a third cantrip and an available feat or ability points."
                : next.character_class == "Bard"
                ? "Bardic spellcasting with prepared Bard spells; Jack of All Trades at level two; the College of Lore at level three with three more skills. Level four grants a third cantrip and an available feat or ability points."
                : next.character_class == "Warlock"
                ? "Pact Magic: prepared Warlock spells cast from slots of one level that return on a Short Rest; Magical Cunning at level two; the Fiend Patron at level three: Dark One's Blessing and Burning Hands, Command and Scorching Ray always prepared. Level four grants a third cantrip and an available feat or ability points."
                : next.character_class == "Sorcerer"
                ? "Prepared Sorcerer spells and Innate Sorcery; Font of Magic's Sorcery Points at level two; Draconic Sorcery at level three: Draconic Resilience (Hit Points and unarmored AC 10 + Dexterity + Charisma) and Chromatic Orb, Command and Dragon's Breath always prepared. Level four grants a fifth cantrip and an available feat or ability points."
                : next.character_class == "Monk"
                ? "Martial Arts and Unarmored Defense; Monk's Focus (Flurry of Blows, Patient Defense, Step of the Wind), Unarmored Movement and Uncanny Metabolism at level two; Deflect Attacks and the Warrior of the Open Hand at level three. Level four grants an available feat or ability points."
                : next.character_class == "Barbarian"
                ? "Rage, Unarmored Defense and Weapon Mastery; Danger Sense and Reckless Attack at level two; the Berserker with Frenzy, Primal Knowledge and a third Rage at level three. Level four grants an available feat or ability points and a third Weapon Mastery."
                : next.character_class == "Paladin"
                ? "Prepared spells, Lay On Hands and fixed HP advancement; Fighting Style or Blessed Warrior and Paladin's Smite at level two; Channel Divinity, the Oath of Devotion and Sacred Weapon at level three. Level four grants an available feat or ability points."
                : "Prepared spells with Favored Enemy and fixed HP advancement; Fighting Style or Druidic Warrior at level two; the Hunter with Hunter's Lore and Hunter's Prey at level three. Level four grants an available feat or ability points.";
            next.class_modifiers += "\n" + note;
            next.class_messages.push_back({note, {}});
        }
        else
        {
            next.class_modifiers +=
                "\nLevel " + std::to_string(next.level) +
                ": HP and spell-slot advancement applied. Additional class and subclass features remain unavailable.";
            next.class_messages.push_back(
            {
                "Level {level}: HP and spell-slot advancement applied. Additional class and subclass features remain unavailable.",
                {{"level", std::to_string(next.level)}}});
        }
        if (next.character_class == "Fighter" && next.level == 2)
        {
            next.class_modifiers +=
                "\nAction Surge: one additional action, except Magic, on your turn. One use per Short or Long Rest.";
            next.class_messages.push_back(
            {
                "Action Surge: one additional action, except Magic, on your turn. One use per Short or Long Rest.",
                {}});
        }
        actor.definition = character_definition(character_profile(next, {}).data);
        if (actor.hp > 0)
            actor.hp += growth;
        // Pact Magic's slots become level-two slots at Warlock level 3; the
        // slots already spent stay spent.
        const int spent_slots = old.slots - actor.slots + old.slots2 - actor.slots2;
        if (actor.definition.pact_magic && !actor.definition.slots && old.slots)
        {
            actor.slots = 0;
            actor.slots2 = std::max(0, actor.definition.slots2 - spent_slots);
        }
        else
        {
            actor.slots += actor.definition.slots - old.slots;
            actor.slots2 += actor.definition.slots2 - old.slots2;
        }
        actor.winds += actor.definition.winds - old.winds;
        actor.rushes += actor.definition.rushes - old.rushes;
        actor.surges += surge_capacity(actor.definition) - surge_capacity(old);
        actor.arcane += arcane_capacity(actor.definition) - arcane_capacity(old);
        actor.lay_on_hands += lay_capacity(actor.definition) - lay_capacity(old);
        actor.free_casts += free_cast_capacity(actor.definition) - free_cast_capacity(old);
        actor.channel_divinity += channel_capacity(actor.definition) - channel_capacity(old);
        actor.hit_dice += actor.definition.level - old.level;
        auto continuation = vitals(actor);
        sheet = std::move(next);
        state = std::move(continuation);
        return true;
    }

    void validate_character_state(const CharacterSheet &sheet,
                                  const VitalState &state) const override
    {
        Actor actor;
        actor.definition = character_definition(character_profile(sheet, {}).data);
        actor.winds = actor.definition.winds;
        actor.slots = actor.definition.slots;
        actor.slots2 = actor.definition.slots2;
        restore_vitals(actor, state);
    }

    // SRD Long Rest: eight hours, then sixteen hours before the next one begins.
    RestPolicy long_rest_policy() const override
    {
        return {480, 960};
    }

    RestPolicy short_rest_policy() const override
    {
        return {60, 0};
    }

    void elapse(std::span<Participant> participants, std::uint64_t milliseconds,
                std::uint64_t &random_state) const override
    {
        // Work on owned candidates so malformed state cannot partly advance a
        // party or consume its RNG. No Godot or campaign data enters the rules.
        std::vector<Actor> actors;
        actors.reserve(participants.size());
        for (const auto &p : participants)
        {
            Actor a;
            a.source = p;
            a.definition = p.character_profile.empty() ? content_->definitions.at(p.definition)
                           : character_definition(p.character_profile);
            a.hp = a.definition.hp;
            a.winds = a.definition.winds;
            a.slots = a.definition.slots;
            a.slots2 = a.definition.slots2;
            a.hit_dice = a.definition.hit_die ? a.definition.level : 0;
            if (p.state)
                restore_vitals(a, *p.state);
            actors.push_back(std::move(a));
        }
        std::vector<detail::RecoverySubject> subjects;
        for (auto &a : actors)
            subjects.push_back({{
                a.source.id, a.effects, a.definition.saves, a.dead,
                a.definition.str_dex_disadvantage, false
            },
            a});
        auto rng = random_state;
        detail::elapse_recovery(subjects, milliseconds, rng);
        // An ended Aid lowers the maximum, and Hit Points above it with it.
        for (auto &a : actors)
            a.hp = std::min(a.hp, max_hp(a));
        if (!milliseconds)
            return;
        std::vector<VitalState> next;
        next.reserve(actors.size());
        for (const auto &a : actors)
            next.push_back(vitals(a));
        for (std::size_t i = 0; i < participants.size(); ++i)
            if (participants[i].state &&
                    // A fresh character's empty state stays empty unless dying.
                    ((participants[i].state->hit_points == 0 && !participants[i].state->dead) ||
                     !participants[i].state->resources.empty()))
                participants[i].state = std::move(next[i]);
        random_state = rng;
    }

    void recover(VitalState &state, const CharacterSheet &sheet) const override
    {
        const auto d = character_definition(character_profile(sheet, {}).data);
        Actor actor;
        actor.definition = d;
        actor.winds = d.winds;
        actor.slots = d.slots;
        actor.slots2 = d.slots2;
        restore_vitals(actor, state);
        if (actor.dead || actor.hp < 1)
            throw std::runtime_error("Long rest requires at least one HP at its start");
        (void)detail::heal_life(actor, max_hp(actor), max_hp(actor),
                                !detail::healing_blocked(actor.effects));
        // A Long Rest makes Prayer of Healing able to help the creature again.
        std::erase_if(actor.effects.active, [](const auto & e)
        {
            return e.kind == detail::EffectKind::prayer_of_healing;
        });
        actor.winds = d.winds;
        actor.slots = d.slots;
        actor.slots2 = d.slots2;
        actor.hit_dice = d.hit_die ? d.level : 0;
        actor.successes = actor.failures = 0;
        actor.stable = false;
        actor.recovery = {};
        actor.temporary_hp = {};
        actor.rushes = d.rushes;
        actor.surges = surge_capacity(d);
        actor.arcane = arcane_capacity(d);
        actor.lay_on_hands = lay_capacity(d);
        actor.free_casts = free_cast_capacity(d);
        actor.channel_divinity = channel_capacity(d);
        state = vitals(actor);
    }

    RecoveryInfo recovery_info(const CharacterSheet &sheet, const VitalState &state) const override
    {
        const auto d = character_definition(character_profile(sheet, {}).data);
        Actor actor;
        actor.definition = d;
        actor.winds = d.winds;
        actor.slots = d.slots;
        actor.slots2 = d.slots2;
        restore_vitals(actor, state);
        RecoveryInfo result{unsigned(d.hit_die),
                            unsigned(actor.hit_dice),
                            unsigned(d.level),
                            !actor.dead && actor.hp > 0,
                            {},
                            actor.temporary_hp};
        for (const auto &descriptor : resource_descriptors)
            if (d.*descriptor.capacity)
                result.resources.push_back(resource_pool(descriptor, actor, d));
        for (const auto &choice : arcane_allocations)
            if (can_recover(actor, choice))
                result.choices.push_back({std::string(choice.id), {std::string(choice.label), {}}});
        return result;
    }

    void grant_temporary_hit_points(VitalState &state, const CharacterSheet &sheet,
                                    const TemporaryHitPoints &offered,
                                    TemporaryHpChoice choice) const override
    {
        const auto d = character_definition(character_profile(sheet, {}).data);
        Actor actor;
        actor.definition = d;
        actor.winds = d.winds;
        actor.slots = d.slots;
        actor.slots2 = d.slots2;
        restore_vitals(actor, state);
        detail::grant_temporary_hp(actor, offered, choice);
        auto next = vitals(actor);
        state = std::move(next);
    }

    void recover_short_rest(VitalState &state, const CharacterSheet &sheet) const override
    {
        const auto d = character_definition(character_profile(sheet, {}).data);
        Actor actor;
        actor.definition = d;
        actor.winds = d.winds;
        actor.slots = d.slots;
        actor.slots2 = d.slots2;
        restore_vitals(actor, state);
        if (actor.dead || actor.hp < 1)
            throw std::runtime_error("Short rest requires at least one HP at its start");
        actor.winds = std::min(d.winds, actor.winds + 1);
        actor.rushes = d.rushes;
        actor.surges = surge_capacity(d);
        actor.channel_divinity = std::min(channel_capacity(d), actor.channel_divinity + 1);
        // Pact Magic slots all return on a Short Rest.
        if (d.pact_magic)
        {
            actor.slots = d.slots;
            actor.slots2 = d.slots2;
        }
        state = vitals(actor);
    }

    Message recover_rest_choice(VitalState &state, const CharacterSheet &sheet,
                                std::string_view choice_id) const override
    {
        const auto d = character_definition(character_profile(sheet, {}).data);
        Actor actor;
        actor.definition = d;
        actor.winds = d.winds;
        actor.slots = d.slots;
        actor.slots2 = d.slots2;
        restore_vitals(actor, state);
        const auto choice = std::find_if(arcane_allocations.begin(), arcane_allocations.end(),
                                         [&](const auto & value)
        {
            return value.id == choice_id;
        });
        if (choice == arcane_allocations.end() || !can_recover(actor, *choice))
            throw std::runtime_error("This rest recovery choice is unavailable");
        actor.slots += int(choice->first);
        actor.slots2 += int(choice->second);
        --actor.arcane;
        auto next = vitals(actor);
        Message result
        {
            "Arcane Recovery restored {first} level-one and {second} level-two spell slots.",
            {{"first", std::to_string(choice->first)}, {"second", std::to_string(choice->second)}}};
        state = std::move(next);
        return result;
    }

    HitDieResult spend_hit_die(VitalState &state, const CharacterSheet &sheet,
                               std::uint64_t &random_state) const override
    {
        const auto d = character_definition(character_profile(sheet, {}).data);
        Actor actor;
        actor.definition = d;
        actor.winds = d.winds;
        actor.slots = d.slots;
        actor.slots2 = d.slots2;
        restore_vitals(actor, state);
        if (actor.dead || actor.hp < 1 || actor.hit_dice < 1)
            throw std::runtime_error("No Hit Die can be spent by this character");
        auto rng = random_state;
        const int rolled = roll_die(rng, d.hit_die);
        const int healing = detail::heal_life(actor, std::max(1, rolled + d.constitution), max_hp(actor),
                                              !detail::healing_blocked(actor.effects));
        --actor.hit_dice;
        HitDieResult result{unsigned(d.hit_die), rolled, d.constitution, healing,
                            unsigned(actor.hit_dice)};
        auto next = vitals(actor);
        state = std::move(next);
        random_state = rng;
        return result;
    }

    void set_hit_points(VitalState &state, const CharacterSheet &sheet, int hp) const override
    {
        if (hp < 0 || hp > sheet.hit_points || (state.dead && hp))
            throw std::runtime_error("Unsupported script HP change");
        Actor actor;
        actor.definition = character_definition(character_profile(sheet, {}).data);
        actor.winds = actor.definition.winds;
        actor.slots = actor.definition.slots;
        actor.slots2 = actor.definition.slots2;
        restore_vitals(actor, state);
        if (hp == state.hit_points ||
                (hp > state.hit_points && detail::healing_blocked(actor.effects)))
            return;
        detail::set_life_hit_points(actor, hp, max_hp(actor));
        state = vitals(actor);
    }

    // A character's actor outside combat, its resources read from `state`.
    Actor camp_actor(const CharacterSheet &sheet, const VitalState &state) const
    {
        Actor actor;
        actor.definition = character_definition(character_profile(sheet, {}).data);
        actor.winds = actor.definition.winds;
        actor.slots = actor.definition.slots;
        actor.slots2 = actor.definition.slots2;
        restore_vitals(actor, state);
        return actor;
    }

    void temple_heal(VitalState &state, const CharacterSheet &sheet,
                     std::uint64_t &random_state) const override
    {
        const auto d = character_definition(character_profile(sheet, {}).data);
        Actor actor;
        actor.definition = d;
        actor.winds = d.winds;
        actor.slots = d.slots;
        actor.slots2 = d.slots2;
        restore_vitals(actor, state);
        if (actor.dead || actor.hp >= max_hp(actor))
            throw std::runtime_error("Cure Wounds requires a wounded living member");
        // Authored temple caster: Cure Wounds, Wisdom +3. Same SplitMix64 as combat.
        auto rng = random_state;
        int amount = 3;
        for (int i = 0; i < 2; ++i)
            amount += roll_die(rng, 8);
        (void)detail::heal_life(actor, amount, max_hp(actor), !detail::healing_blocked(actor.effects));
        auto next = vitals(actor);
        state = std::move(next);
        random_state = rng;
    }

    int hit_point_maximum(const CharacterSheet &sheet, const VitalState &state) const override
    {
        // Unreadable vitals keep the sheet's maximum; validating them is left to
        // the rules that use them, which reject them there.
        try
        {
            return max_hp(camp_actor(sheet, state));
        }
        catch (const std::exception &)
        {
            return sheet.hit_points;
        }
    }

    // Healing outside combat (CLASS-3): Lay On Hands and the healing spells.
    // A free hand is not required; there is time to stow a shield.
    std::vector<CampAction> camp_actions(const CharacterSheet &sheet,
                                         const VitalState &state) const override
    {
        const auto actor = camp_actor(sheet, state);
        const auto &d = actor.definition;
        std::vector<CampAction> actions;
        if (actor.dead || actor.hp == 0)
            return actions;
        if (d.lay_on_hands && actor.lay_on_hands > 0)
            actions.push_back({"lay_on_hands", {"Lay On Hands", {}}});
        // Magical Cunning: a one-minute rite, so a camp action; it needs no
        // target and so takes the whole-party path.
        if (d.magical_cunning && actor.arcane > 0 &&
                actor.slots + actor.slots2 < d.slots + d.slots2)
            actions.push_back({"magical_cunning", {"Magical Cunning", {}}, true});
        if (d.str_dex_disadvantage)
            return actions;
        for (const auto &spell : detail::spell_table)
        {
            if ((spell.pattern != detail::SpellPattern::heal &&
                    spell.pattern != detail::SpellPattern::camp) ||
                    !detail::knows_spell(d.spells, spell.id))
                continue;
            const std::string label(spell.label);
            const bool party = spell.instances > 1;
            if (spell.level >= 2)
            {
                if (actor.slots2 > 0)
                    actions.push_back({std::string(spell.id), {label, {}}, party});
                continue;
            }
            if (actor.slots > 0)
                actions.push_back({std::string(spell.id), {label, {}}, party});
            // Goodberry gains nothing from a higher slot.
            if (actor.slots2 > 0 && spell.pattern == detail::SpellPattern::heal)
                actions.push_back({std::string(spell.id) + "_2", {label + " (level 2 slot)", {}}});
        }
        return actions;
    }

    void use_camp_action(const CharacterSheet &user, VitalState &user_state,
                         const CharacterSheet &target, VitalState &target_state,
                         std::string_view action, std::uint64_t &random_state) const override
    {
        const auto offered = camp_actions(user, user_state);
        if (std::none_of(offered.begin(), offered.end(), [&](const auto & a)
    {
        return a.id == action && !a.whole_party;
    }))
        throw std::runtime_error("That spell or feature cannot be used now");
        const bool self = &user_state == &target_state;
        auto caster = camp_actor(user, user_state);
        auto healed = self ? caster : camp_actor(target, target_state);
        auto &patient = self ? caster : healed;
        const int maximum = max_hp(patient);
        if (patient.dead || patient.hp >= maximum)
            throw std::runtime_error("Healing requires a wounded living member");
        const bool can_heal = !detail::healing_blocked(patient.effects);
        auto rng = random_state;
        if (action == "lay_on_hands")
        {
            // Only the Hit Points actually restored are spent from the pool.
            caster.lay_on_hands -= detail::heal_life(
                                       patient, std::min(caster.lay_on_hands, maximum - patient.hp),
                                       maximum, can_heal);
        }
        else if (action == "goodberry")
        {
            --caster.slots;
            (void)detail::heal_life(patient, std::min(10, maximum - patient.hp), maximum, can_heal);
        }
        else
        {
            const auto &spell = *detail::find_spell(action);
            const bool upcast = action.ends_with("_2");
            if (upcast)
                --caster.slots2;
            else
                --caster.slots;
            // Disciple of Life adds 2 + the slot's level.
            int amount = caster.definition.casting - 2 +
                         (caster.definition.life_domain ? 2 + (upcast ? 2 : int(spell.level)) : 0);
            const int count = spell.dice.count + (upcast ? int(spell.upcast.extra_dice) : 0);
            for (int n = 0; n < count; ++n)
                amount += roll_die(rng, spell.dice.sides);
            (void)detail::heal_life(patient, std::max(0, amount), maximum, can_heal);
        }
        user_state = vitals(caster);
        if (!self)
            target_state = vitals(healed);
        random_state = rng;
    }

    bool can_cast_exploration_spell(const CharacterSheet &sheet, const VitalState &state,
                                    std::string_view id) const override
    {
        const auto actor = camp_actor(sheet, state);
        const auto *spell = detail::find_spell(id);
        return spell && spell->pattern == detail::SpellPattern::exploration && !actor.dead &&
               actor.hp > 0 && !actor.definition.str_dex_disadvantage &&
               detail::knows_spell(actor.definition.spells, id) &&
               (spell->level >= 2 ? actor.slots2 : actor.slots) > 0;
    }

    void cast_exploration_spell(const CharacterSheet &sheet, VitalState &state,
                                std::string_view id) const override
    {
        if (!can_cast_exploration_spell(sheet, state, id))
            throw std::runtime_error("That spell cannot be cast now");
        auto caster = camp_actor(sheet, state);
        if (detail::find_spell(id)->level >= 2)
            --caster.slots2;
        else
            --caster.slots;
        state = vitals(caster);
    }

    // Prayer of Healing: the five most hurt members it has not healed since
    // their last Long Rest each regain 2d8 + the spellcasting modifier.
    void use_party_camp_action(const CharacterSheet &user, VitalState &user_state,
                               std::span<const CampTarget> party, std::string_view action,
                               std::uint64_t &random_state) const override
    {
        const auto offered = camp_actions(user, user_state);
        if (std::none_of(offered.begin(), offered.end(), [&](const auto & a)
    {
        return a.id == action && a.whole_party;
    }))
        throw std::runtime_error("That spell or feature cannot be used now");
        auto caster = camp_actor(user, user_state);
        if (action == "magical_cunning")
        {
            // Back come expended Pact Magic slots, up to half the maximum
            // (rounded up).
            const auto &d = caster.definition;
            auto &slots = d.slots2 ? caster.slots2 : caster.slots;
            const int maximum = d.slots2 ? d.slots2 : d.slots;
            slots = std::min(maximum, slots + (maximum + 1) / 2);
            --caster.arcane;
            user_state = vitals(caster);
            return;
        }
        std::vector<std::pair<Actor, VitalState *>> members;
        for (const auto &member : party)
            if (member.state != &user_state)
                members.push_back({camp_actor(*member.sheet, *member.state), member.state});
        std::vector<Actor *> chosen{&caster};
        for (auto &[actor, state] : members)
            chosen.push_back(&actor);
        if (std::none_of(party.begin(), party.end(), [&](const auto & member)
    {
        return member.state == &user_state;
    }))
        chosen.erase(chosen.begin());
        std::erase_if(chosen, [](const Actor * a)
        {
            return a->dead || a->hp >= max_hp(*a) ||
                   detail::has_effect(a->effects, detail::EffectKind::prayer_of_healing);
        });
        if (chosen.empty())
            throw std::runtime_error("No member can be healed by it now");
        std::sort(chosen.begin(), chosen.end(), [](const Actor * x, const Actor * y)
        {
            return x->hp * max_hp(*y) < y->hp * max_hp(*x);
        });
        if (chosen.size() > 5)
            chosen.resize(5);
        const auto &spell = *detail::find_spell(action);
        --caster.slots2;
        auto rng = random_state;
        for (auto *patient : chosen)
        {
            // Disciple of Life adds 2 + the slot's level.
            int amount = caster.definition.casting - 2 +
                         (caster.definition.life_domain ? 2 + int(spell.level) : 0);
            for (int n = 0; n < spell.dice.count; ++n)
                amount += roll_die(rng, spell.dice.sides);
            (void)detail::heal_life(*patient, std::max(0, amount), max_hp(*patient),
                                    !detail::healing_blocked(patient->effects));
            // Camp effects carry scope 1: there is no encounter to source them.
            detail::apply_spell_benefit(patient->effects, 1, 1, user.name,
                                        detail::EffectKind::prayer_of_healing, 0);
        }
        user_state = vitals(caster);
        for (auto &[actor, state] : members)
            *state = vitals(actor);
        random_state = rng;
    }

    SpellChoiceOptions spell_choice_options(const CharacterSheet &sheet,
                                            SpellChoiceContext context) const override
    {
        return detail::spell_choice_options(sheet, context);
    }

    void apply_spell_choices(CharacterSheet &sheet, const SpellChoices &choice,
                             SpellChoiceContext context, bool complete) const override
    {
        detail::apply_spell_choices(sheet, choice, context, complete);
    }

    SpellAccess spell_access(const CharacterSheet &sheet) const override
    {
        return detail::spell_access(sheet.grants, sheet.character_class, sheet.level,
                                    sheet.prepared_spells);
    }

    EquipmentInfo equipment_info(std::string_view key) const override
    {
        if (detail::ammunition(key))
            return {EquipmentSlot::carried, 0};
        if (const auto *item = detail::weapon(key))
            return {EquipmentSlot::weapon, item->hands};
        if (key == "shield")
            return {EquipmentSlot::shield, 1};
        if (detail::armor(key))
            return {EquipmentSlot::armor, 0};
        return {};
    }

    std::vector<EquipmentChoice> equipment_choices(const CharacterSheet &sheet,
            std::span<const std::string> candidates,
            unsigned selected) const override
    {
        if (selected >= candidates.size())
            throw std::runtime_error("Unknown equipment candidate");
        const auto *chosen = detail::weapon(candidates[selected]);
        if (!chosen || chosen->hands != 1)
            return {};
        std::vector<unsigned> held;
        for (unsigned i = 0; i < candidates.size(); ++i)
            if (i != selected && detail::weapon(candidates[i]))
                held.push_back(i);
        if (held.empty())
            return {};
        std::vector<EquipmentChoice> result;
        for (unsigned n = 0; n < 2; ++n)
        {
            const auto op = n ? EquipmentOperation::equip_other : EquipmentOperation::equip_main;
            EquipmentChoice choice
            {
                op,
                {
                    n ? "Other hand — {item}" : "Main hand — {item}",
                    {{"item", n < held.size() ? weapon_label(candidates[held[n]]) : "Empty", true}}
                },
                {},
                true};
            choice.explanation = {"Equip {item} in this hand; replace its current weapon.",
                {{"item", weapon_label(candidates[selected]), true}}
            };
            try
            {
                (void)equipment_change(sheet, candidates, selected, op);
            }
            catch (const std::runtime_error &e)
            {
                choice.available = false;
                choice.explanation = {e.what(), {}};
            }
            result.push_back(std::move(choice));
        }
        return result;
    }

    EquipmentChange equipment_change(const CharacterSheet &sheet,
                                     std::span<const std::string> candidates,
                                     unsigned selected,
                                     EquipmentOperation operation) const override
    {
        if (selected >= candidates.size())
            throw std::runtime_error("Unknown equipment candidate");
        const auto slot = equipment_info(candidates[selected]).slot;
        if (operation != EquipmentOperation::unequip && slot == EquipmentSlot::carried)
            throw std::runtime_error("This item is carried, not equipped");
        EquipmentChange result;
        const bool hand = operation == EquipmentOperation::equip_main ||
                          operation == EquipmentOperation::equip_other;
        if (hand)
        {
            const auto *weapon = detail::weapon(candidates[selected]);
            if (!weapon || weapon->hands != 1)
                throw std::runtime_error("Choose a one-handed weapon");
            std::vector<unsigned> held;
            for (unsigned i = 0; i < candidates.size(); ++i)
                if (i != selected)
                {
                    if (detail::weapon(candidates[i]))
                        held.push_back(i);
                    else
                        result.indices.push_back(i);
                }
            if (held.empty() || held.size() > 2)
                throw std::runtime_error("Invalid equipped hand selection");
            result.indices.push_back(operation == EquipmentOperation::equip_main ? selected
                                     : held[0]);
            if (operation == EquipmentOperation::equip_other)
                result.indices.push_back(selected);
            else if (held.size() > 1)
                result.indices.push_back(held[1]);
            result.separate_selected_unit = true;
        }
        else
            for (unsigned i = 0; i < candidates.size(); ++i)
            {
                if (operation == EquipmentOperation::unequip && i == selected)
                    continue;
                if (operation == EquipmentOperation::equip && slot == EquipmentSlot::weapon &&
                        i != selected && equipment_info(candidates[i]).slot == EquipmentSlot::weapon)
                    continue;
                result.indices.push_back(i);
            }
        std::vector<std::string> gear;
        for (auto i : result.indices)
            gear.push_back(candidates[i]);
        (void)character_profile(sheet, gear);
        return result;
    }

    AbilityCheckModifier ability_check(const CharacterSheet &sheet,
                                       std::span<const std::string> gear, unsigned ability,
                                       std::string_view skill) const override
    {
        const auto d = character_definition(character_profile(sheet, gear).data);
        auto result = character_rules()->ability_check(sheet, ability, skill);
        result.disadvantage = (ability < 2 && d.str_dex_disadvantage) ||
                              (ability == 1 && skill == "stealth" && d.stealth_disadvantage);
        return result;
    }

    AbilityCheckRoll roll_ability_check(const CharacterSheet &sheet,
                                        std::span<const std::string> gear, unsigned ability,
                                        std::string_view skill,
                                        std::uint64_t &random_state) const override
    {
        const auto modifier = ability_check(sheet, gear, ability, skill);
        auto rng = random_state;
        int die = roll_die(rng, 20);
        // Advantage and Disadvantage cancel; either alone rolls a second d20.
        if (modifier.advantage != modifier.disadvantage)
        {
            const int second = roll_die(rng, 20);
            die = modifier.advantage ? std::max(die, second) : std::min(die, second);
        }
        random_state = rng;
        return {die, die + modifier.total};
    }

    CharacterProfile character_profile(const CharacterSheet &sheet,
                                       std::span<const std::string> gear) const override
    {
        if (sheet.identity != character_rules()->identity() || sheet.level < 1 || sheet.level > 4)
            throw std::runtime_error("Unsupported character rules identity or level");
        (void)detail::training_profile(sheet.grants, detail::grant_source_id(sheet.character_class),
                                       detail::grant_source_id(sheet.background), sheet.level,
                                       sheet.scores);
        const auto features_only =
            detail::without_spell_grants(detail::without_training(sheet.grants));
        const auto access = spell_access(sheet);
        const auto effects =
            detail::validate_grants(features_only, detail::grant_source_id(sheet.character_class),
                                    detail::grant_source_id(sheet.race),
                                    detail::grant_source_id(sheet.background), sheet.level);
        const unsigned features = effects.feats;
        if (sheet.hit_point_modifiers.size() != sheet.level)
            throw std::runtime_error("HP history does not match character advancement");
        const bool asi = detail::has_grant(sheet.grants, "feat:ability_score_improvement");
        if (sheet.ability_adjustments.size() != (asi ? 2u : 1u))
            throw std::runtime_error("Ability sources disagree with acquired grants");
        if (asi)
        {
            const auto &adjustment = sheet.ability_adjustments.back();
            if (adjustment.source_id != "feat:ability_score_improvement" || adjustment.level != 4 ||
                    adjustment.bonuses != effects.abilities)
                throw std::runtime_error("Ability sources disagree with acquired choices");
        }
        for (unsigned i = 0; i < 6; ++i)
        {
            const auto bonuses =
                sheet.ability_adjustments.front().bonuses[i] + effects.abilities[i];
            if (sheet.bonuses[i] != bonuses || sheet.scores[i] != sheet.base[i] + bonuses)
                throw std::runtime_error("Ability totals disagree with acquired choices");
        }
        auto spells =
            detail::prepares_spells(sheet.character_class)
            ? detail::casting_ids(access)
            : detail::known_cantrip_ids(access);
        std::set<std::string> selected;
        const auto record = [&](std::string id)
        {
            if (!detail::knows_spell(spells, id))
                spells.push_back(std::move(id));
        };
        // Preparable spells come from the eligibility table. Cantrips are
        // filtered out: they are known, never prepared.
        const auto preparable =
            detail::spells_of_level(allowed_spells(sheet.character_class, sheet.level), false);
        for (const auto &spell : sheet.prepared_spells)
        {
            if (!selected.insert(spell).second)
                throw std::runtime_error("Duplicate prepared spell");
            if (!detail::knows_spell(preparable, spell))
                throw std::runtime_error("Unsupported prepared spell");
            record(spell);
        }
        std::ostringstream out;
        out << profile_magic << ' ' << sheet.level << ' ' << features << ' ' << spells.size();
        for (const auto &id : spells)
            out << ' ' << id;
        out << ' ' << std::quoted(sheet.character_class) << ' ' << std::quoted(sheet.race);
        for (auto score : sheet.scores)
            out << ' ' << score;
        for (auto modifier : sheet.hit_point_modifiers)
            out << ' ' << modifier;
        out << ' ' << gear.size();
        for (const auto &item : gear)
            out << ' ' << std::quoted(item);
        out << ' ' << std::quoted(detail::grant_source_id(sheet.background));
        detail::write_grants(out, sheet.grants);
        const auto data = out.str();
        const auto d = character_definition(data);
        if (d.hp != sheet.hit_points)
            throw std::runtime_error("Character HP does not match rules profile");
        CharacterProfile result
        {
            data,
            d.hp,
            d.ac,
            "Level 1-4 subset: HP, selected feats, supported prepared spells and level-one/two slots. Additional class/subclass and species features remain unavailable.",
            d.speed,
            d.melee_bonus};
        result.strength_dexterity_disadvantage = d.str_dex_disadvantage;
        result.weapon_hands = d.weapon_hands;
        unsigned hand_index = 0;
        for (const auto &key : gear)
            result.equipment_positions.push_back(
        {
            detail::weapon(key) ? (hand_index++ ? "Other hand" : "Main hand") : "Equipped",
            {}});
        if (d.versatile_sides)
        {
            const auto sides = d.weapon_hands == 2 ? d.versatile_sides : d.melee.sides;
            result.item_messages.push_back(
            {
                "Weapon grip: {hands} hands; melee damage 1d{sides} + ability modifier.",
                {{"hands", std::to_string(d.weapon_hands)}, {"sides", std::to_string(sides)}}});
            result.item_modifiers += "Weapon grip: " + std::to_string(d.weapon_hands) +
                                     " hands; melee damage 1d" + std::to_string(sides) +
                                     " + ability modifier.\n";
        }
        for (const auto &key : gear)
        {
            if (key == "shield")
                result.item_modifiers += trained(sheet.character_class, sheet.grants, key)
                                         ? "Source: equipped Shield: +2 AC.\n"
                                         : "Source: equipped Shield: +0 AC (untrained).\n";
            else if (key == "leather")
                result.item_modifiers += "Source: equipped Leather armor and Dexterity score " +
                                         std::to_string(sheet.scores[1]) +
                                         ". AC becomes 11 + Dexterity modifier (" +
                                         std::to_string(sheet.modifiers[1]) + ").\n";
            else if (key == "chain_mail")
                result.item_modifiers +=
                    "Source: equipped Chain mail. AC becomes 16; speed -10 feet below Strength 13 (current Strength " +
                    std::to_string(sheet.scores[0]) + ").\n";
            else if (const auto *item = detail::armor(key))
                result.item_modifiers +=
                    "Source: equipped " + std::string(item->label) + " (" +
                    std::string(detail::armor_category_label(item->category)) + "). Base AC " +
                    std::to_string(item->base_ac) + "; applied Dexterity modifier " +
                    std::to_string(item->dexterity_contribution(sheet.modifiers[1])) +
                    "; armor AC " +
                    std::to_string(item->base_ac +
                                   item->dexterity_contribution(sheet.modifiers[1])) +
                    ".\n";
            else if (key == "wand")
                result.item_modifiers +=
                    "Source: equipped Wand. Held focus; melee uses unarmed strike.\n";
            else if (const auto *item = detail::weapon(key); item && item->fixed_damage)
                result.item_modifiers +=
                    "Source: equipped " + weapon_label(key) + ". Attack uses " +
                    attack_ability(key) + " modifier" +
                    (trained(sheet.character_class, sheet.grants, key) ? " +2 class proficiency"
                     : " without proficiency") +
                                                       "; damage is fixed at " + std::to_string(item->fixed_damage) +
                                                       " without an ability modifier.\n";
            else
                result.item_modifiers +=
                    "Source: equipped " + weapon_label(key) + " and " + sheet.character_class +
                    " weapon proficiency. Weapon attack uses " + attack_ability(key) + " modifier" +
                    (trained(sheet.character_class, sheet.grants, key) ? " +2 class proficiency"
                     : " without proficiency") +
                                                       "; damage adds that ability modifier.\n";
        }
        for (const auto &key : gear)
            result.item_modifiers +=
                "Source: equipped " + weapon_label(key) + ". " + equipment_note(sheet, key) + "\n";
        if (features & 1)
            result.item_modifiers += "Defense feat: +1 AC while wearing armor.\n";
        if (features & 8)
            result.item_modifiers +=
                "Great Weapon Fighting: eligible Melee weapon damage dice showing 1 or 2 count as 3 while held in two hands.\n";
        if (features & 32)
            result.item_modifiers +=
                "Alert: add proficiency to Initiative; optionally swap Initiative with an eligible ally before the first turn.\n";
        if (features & 2)
            result.item_modifiers +=
                "Savage Attacker: once per turn on a weapon hit, weapon damage is rolled twice and the higher result is kept automatically.\n";
        if (gear.empty())
            result.item_modifiers =
                "No equipment modifiers. Source: unarmed strike rules and Strength score " +
                std::to_string(sheet.scores[0]) +
                ". Attack uses Strength modifier +2 level-one proficiency; damage is 1 + Strength modifier (minimum 0).";
        result.spell_modifiers = "Active conditions are shown in the character status.";
        if (sheet.character_class == "Wizard")
            result.spell_modifiers =
                "Source: Fire Bolt and Wizard spellcasting, Intelligence score " +
                std::to_string(sheet.scores[3]) +
                ". Attack: Intelligence modifier +2 level-one proficiency = " +
                std::to_string(d.casting) + ". Magic Missile has no ability modifier to damage.\n" +
                result.spell_modifiers;
        if (sheet.character_class == "Cleric")
            result.spell_modifiers =
                "Source: Cure Wounds and Cleric spellcasting, Wisdom score " +
                std::to_string(sheet.scores[4]) + ". Healing: 2d8 + Wisdom modifier (" +
                std::to_string(d.casting - 2) + ").\n" + result.spell_modifiers;
        if (d.str_dex_disadvantage)
            result.spell_modifiers =
                "Cannot cast spells while wearing untrained armor.\n" + result.spell_modifiers;
        // Language-independent presentation of the same computed rule results.
        for (const auto &key : gear)
        {
            if (key == "shield")
                result.item_messages.push_back({trained(sheet.character_class, sheet.grants, key)
                                                ? "Source: equipped Shield: +2 AC."
                                                : "Source: equipped Shield: +0 AC (untrained).",
                                                {}});
            else if (key == "leather")
                result.item_messages.push_back(
            {
                "Source: equipped Leather armor and Dexterity score {score}. AC becomes 11 + Dexterity modifier ({modifier}).",
                {   {"score", std::to_string(sheet.scores[1])},
                    {"modifier", std::to_string(sheet.modifiers[1])}
                }});
            else if (key == "chain_mail")
                result.item_messages.push_back(
            {
                "Source: equipped Chain mail. AC becomes 16; speed -10 feet below Strength 13 (current Strength {score}).",
                {{"score", std::to_string(sheet.scores[0])}}});
            else if (const auto *item = detail::armor(key))
                result.item_messages.push_back(
            {
                "Source: equipped {item} ({category}). Base AC {base}; applied Dexterity modifier {dexterity}; armor AC {ac}.",
                {   {"item", std::string(item->label), true},
                    {"category", std::string(detail::armor_category_label(item->category)), true},
                    {"base", std::to_string(item->base_ac)},
                    {
                        "dexterity",
                        std::to_string(item->dexterity_contribution(sheet.modifiers[1]))
                    },
                    {
                        "ac", std::to_string(item->base_ac +
                                             item->dexterity_contribution(sheet.modifiers[1]))
                    }
                }});
            else if (key == "wand")
                result.item_messages.push_back(
            {"Source: equipped Wand. Held focus; melee uses unarmed strike.", {}});
            else if (const auto *item = detail::weapon(key); item && item->fixed_damage)
                result.item_messages.push_back(
            {
                "Source: equipped {item}. Attack uses {ability} modifier {proficiency}; damage is fixed at {damage} without an ability modifier.",
                {   {"item", weapon_label(key), true},
                    {"ability", attack_ability(key), true},
                    {
                        "proficiency",
                        trained(sheet.character_class, sheet.grants, key) ? "+2 class proficiency"
                        : "without proficiency",
                        true
                    },
                    {"damage", std::to_string(item->fixed_damage)}
                }});
            else
                result.item_messages.push_back(
            {
                "Source: equipped {item} and {class} weapon proficiency. Weapon attack uses {ability} modifier {proficiency}; damage adds that ability modifier.",
                {   {"item", weapon_label(key), true},
                    {"class", sheet.character_class, true},
                    {"ability", attack_ability(key), true},
                    {
                        "proficiency",
                        trained(sheet.character_class, sheet.grants, key) ? "+2 class proficiency"
                        : "without proficiency",
                        true
                    }
                }});
            result.item_messages.push_back({equipment_note(sheet, key), {}});
            if (const auto *item = detail::armor(key))
            {
                const auto note = [&](std::string text)
                {
                    result.item_modifiers += text + "\n";
                    result.item_messages.push_back({std::move(text), {}});
                };
                if (item->category == detail::ArmorCategory::medium)
                    note(
                        "Medium armor limits the Dexterity modifier added to AC to +2; negative modifiers still apply.");
                if (item->category == detail::ArmorCategory::heavy)
                    note("Heavy armor ignores the Dexterity modifier when calculating AC.");
                if (item->stealth_disadvantage)
                    note("This armor imposes Disadvantage on Dexterity (Stealth) checks.");
                if (item->strength && key != "chain_mail")
                {
                    const bool penalty = sheet.scores[0] < item->strength;
                    result.item_modifiers +=
                        "Source: equipped " + std::string(item->label) + ". Requires Strength " +
                        std::to_string(item->strength) + "; current score " +
                        std::to_string(sheet.scores[0]) + ". " +
                        (penalty ? "Speed reduced by 10 feet." : "Requirement met.") + "\n";
                    result.item_messages.push_back(
                    {
                        penalty
                        ? "Source: equipped {item}. Requires Strength {required}; current score {score}. Speed reduced by 10 feet."
                        : "Source: equipped {item}. Requires Strength {required}; current score {score}. Requirement met.",
                        {   {"item", std::string(item->label), true},
                            {"required", std::to_string(item->strength)},
                            {"score", std::to_string(sheet.scores[0])}
                        }});
                }
            }
            if (const auto *item = detail::weapon(key); item && item->heavy)
            {
                const std::string ability = item->ranged ? "Dexterity" : "Strength";
                const auto score = std::to_string(sheet.scores[item->ranged ? 1 : 0]);
                const bool penalty = item->heavy_disadvantage(sheet.scores);
                result.item_modifiers +=
                    "Source: equipped " + weapon_label(key) + " (Heavy). Requires " + ability +
                    " 13; current score " + score + ". " +
                    (penalty ? "Attacks with this weapon have Disadvantage." : "Requirement met.") +
                    "\n";
                result.item_messages.push_back(
                {
                    penalty
                    ? "Source: equipped {item} (Heavy). Requires {ability} 13; current score {score}. Attacks with this weapon have Disadvantage."
                    : "Source: equipped {item} (Heavy). Requires {ability} 13; current score {score}. Requirement met.",
                    {   {"item", weapon_label(key), true},
                        {"ability", ability, true},
                        {"score", score}
                    }});
            }
        }
        if (features & 1)
            result.item_messages.push_back({"Defense feat: +1 AC while wearing armor.", {}});
        if (features & 8)
            result.item_messages.push_back(
        {
            "Great Weapon Fighting: eligible Melee weapon damage dice showing 1 or 2 count as 3 while held in two hands.",
            {}});
        if (features & 32)
            result.item_messages.push_back(
        {
            "Alert: add proficiency to Initiative; optionally swap Initiative with an eligible ally before the first turn.",
            {}});
        if (features & 2)
            result.item_messages.push_back(
        {
            "Savage Attacker: once per turn on a weapon hit, weapon damage is rolled twice and the higher result is kept automatically.",
            {}});
        if (gear.empty())
            result.item_messages.push_back(
        {
            "No equipment modifiers. Source: unarmed strike rules and Strength score {score}. Attack uses Strength modifier +2 level-one proficiency; damage is 1 + Strength modifier (minimum 0).",
            {{"score", std::to_string(sheet.scores[0])}}});
        if (d.str_dex_disadvantage)
            result.spell_messages.push_back(
        {"Cannot cast spells while wearing untrained armor.", {}});
        if (sheet.character_class == "Wizard")
            result.spell_messages.push_back(
        {
            "Source: Fire Bolt and Wizard spellcasting, Intelligence score {score}. Attack: Intelligence modifier +2 level-one proficiency = {attack}. Magic Missile has no ability modifier to damage.",
            {   {"score", std::to_string(sheet.scores[3])},
                {"attack", std::to_string(d.casting)}
            }});
        if (sheet.character_class == "Cleric")
            result.spell_messages.push_back(
        {
            "Source: Cure Wounds and Cleric spellcasting, Wisdom score {score}. Healing: 2d8 + Wisdom modifier ({modifier}).",
            {   {"score", std::to_string(sheet.scores[4])},
                {"modifier", std::to_string(d.casting - 2)}
            }});
        if (access.cantrip_choices)
        {
            for (const auto &spell : access.cantrips)
            {
                result.spell_modifiers += "\nKnown cantrip: " + spell.label + ".";
                result.spell_messages.push_back(
                {"Known cantrip: {spell}.", {{"spell", spell.label, true}}});
            }
        }
        if (sheet.character_class == "Sorcerer")
        {
            result.spell_modifiers +=
                "\nSorcerer cantrips: Charisma score " + std::to_string(sheet.scores[5]) +
                "; spell attack bonus " + std::to_string(d.casting) + "; pending choices " +
                std::to_string(access.cantrip_choices - access.cantrips.size()) + ".";
            result.spell_messages.push_back(
            {
                "Sorcerer cantrips: Charisma score {score}; spell attack bonus {attack}; pending choices {cantrips}.",
                {   {"score", std::to_string(sheet.scores[5])},
                    {"attack", std::to_string(d.casting)},
                    {"cantrips", std::to_string(access.cantrip_choices - access.cantrips.size())}
                }});
        }
        if (sheet.character_class == "Warlock")
        {
            result.spell_modifiers +=
                "\nPact Magic cantrips: Charisma score " + std::to_string(sheet.scores[5]) +
                "; spell attack bonus " + std::to_string(d.casting) + "; pending choices " +
                std::to_string(access.cantrip_choices - access.cantrips.size()) + ".";
            result.spell_messages.push_back(
            {
                "Pact Magic cantrips: Charisma score {score}; spell attack bonus {attack}; pending choices {cantrips}.",
                {   {"score", std::to_string(sheet.scores[5])},
                    {"attack", std::to_string(d.casting)},
                    {"cantrips", std::to_string(access.cantrip_choices - access.cantrips.size())}
                }});
        }
        if (sheet.character_class == "Cleric")
        {
            result.spell_modifiers +=
                "\nPending Cleric choices: " +
                std::to_string(access.cantrip_choices - access.cantrips.size()) + " cantrips, " +
                std::to_string(access.prepared_choices - access.prepared.size()) +
                " prepared spells.";
            result.spell_messages.push_back(
            {
                "Pending Cleric choices: {cantrips} cantrips, {prepared} prepared spells.",
                {   {"cantrips", std::to_string(access.cantrip_choices - access.cantrips.size())},
                    {"prepared", std::to_string(access.prepared_choices - access.prepared.size())}
                }});
        }
        if (sheet.character_class == "Ranger")
        {
            result.spell_modifiers +=
                "\nRanger spellcasting: Wisdom score " + std::to_string(sheet.scores[4]) +
                "; pending prepared spells: " +
                std::to_string(access.prepared_choices - access.prepared.size()) + ".";
            result.spell_messages.push_back(
            {
                "Ranger spellcasting: Wisdom score {score}; pending prepared spells: {prepared}.",
                {   {"score", std::to_string(sheet.scores[4])},
                    {"prepared", std::to_string(access.prepared_choices - access.prepared.size())}
                }});
        }
        if (sheet.character_class == "Paladin")
        {
            result.spell_modifiers +=
                "\nPaladin spellcasting: Charisma score " + std::to_string(sheet.scores[5]) +
                "; pending prepared spells: " +
                std::to_string(access.prepared_choices - access.prepared.size()) + ".";
            result.spell_messages.push_back(
            {
                "Paladin spellcasting: Charisma score {score}; pending prepared spells: {prepared}.",
                {   {"score", std::to_string(sheet.scores[5])},
                    {"prepared", std::to_string(access.prepared_choices - access.prepared.size())}
                }});
        }
        if (sheet.character_class == "Wizard")
        {
            for (const auto &spell : access.spellbook)
            {
                result.spell_modifiers += "\nSpellbook: " + spell.label +
                                          " (learned at Wizard level " +
                                          std::to_string(spell.acquired_level) + ").";
                result.spell_messages.push_back(
                {
                    "Spellbook: {spell} (learned at Wizard level {level}).",
                    {   {"spell", spell.label, true},
                        {"level", std::to_string(spell.acquired_level)}
                    }});
            }
            result.spell_modifiers +=
                "\nPending Wizard choices: " +
                std::to_string(access.cantrip_choices - access.cantrips.size()) + " cantrips, " +
                std::to_string(access.spellbook_choices - access.spellbook.size()) +
                " spellbook spells, " +
                std::to_string(access.prepared_choices - access.prepared.size()) +
                " prepared spells.";
            result.spell_messages.push_back(
            {
                "Pending Wizard choices: {cantrips} cantrips, {book} spellbook spells, {prepared} prepared spells.",
                {   {"cantrips", std::to_string(access.cantrip_choices - access.cantrips.size())},
                    {"book", std::to_string(access.spellbook_choices - access.spellbook.size())},
                    {"prepared", std::to_string(access.prepared_choices - access.prepared.size())}
                }});
        }
        result.spell_messages.push_back(
        {"Active conditions are shown in the character status.", {}});
        return result;
    }

  private:
    std::shared_ptr<const Content> content_;
};
} // namespace

std::unique_ptr<RulesModule> load(const std::filesystem::path &file)
{
    if (std::filesystem::file_size(file) > 65536)
        throw std::runtime_error("Rules content exceeds size limit");
    std::ifstream input(file, std::ios::binary);
    if (!input)
        throw std::runtime_error("Cannot open rules content: " + file.string());
    std::string bytes{std::istreambuf_iterator<char>(input), {}};
    if (bytes.size() > 65536 || input.bad())
        throw std::runtime_error("Invalid rules content size/read");
    return parse_content(bytes);
}

std::unique_ptr<RulesModule> parse_content(std::string_view content_bytes)
{
    if (content_bytes.size() > 65536)
        throw std::runtime_error("Rules content exceeds size limit");
    std::string bytes(content_bytes);
    // Git may translate line endings; identical content must keep its identity.
    bytes.erase(std::remove(bytes.begin(), bytes.end(), '\r'), bytes.end());
    std::uint64_t hash = 14695981039346656037ULL;
    for (unsigned char c : bytes)
    {
        hash ^= c;
        hash *= 1099511628211ULL;
    }
    std::istringstream lines(bytes);
    std::string line, magic, revision;
    unsigned version;
    std::getline(lines, line);
    std::istringstream header(line);
    header >> magic >> version >> revision;
    if (!header || magic != "OPENGOLD_SRD5" || version != 1)
        throw std::runtime_error("Unsupported rules content format");
    header >> std::ws;
    if (!header.eof() || revision.empty() || revision.size() > 80)
        throw std::runtime_error("Invalid rules content header");
    Content content;
    content.identity = {"opengold.srd5", "0.6.129", revision + "/" + std::to_string(hash)};
    std::set<std::string> save_rows, casting_rows, damage_rows, size_rows, trait_rows, type_rows;
    while (std::getline(lines, line))
    {
        if (line.empty() || line[0] == '#' || line == "\r")
            continue;
        std::istringstream row(line);
        std::string tag, key;
        Definition d;
        row >> tag >> key;
        if (tag == "damage_types" || tag == "affinity")
        {
            const auto found = content.definitions.find(key);
            if (found == content.definitions.end())
                throw std::runtime_error("Unknown damage profile: " + key);
            auto &definition = found->second;
            std::string first, second;
            row >> first >> second;
            if (tag == "damage_types")
            {
                if (!damage_rows.insert(key).second)
                    throw std::runtime_error("Duplicate damage type row");
                definition.melee_type = detail::damage_type(first);
                definition.ranged_type = detail::damage_type(second);
            }
            else
            {
                std::string type;
                row >> type;
                if (first.empty() || first.size() > 128 || definition.affinities.size() >= 128 ||
                        std::any_of(definition.affinities.begin(), definition.affinities.end(),
                                    [&](const auto & a)
            {
                return a.source_id == first;
            }))
                throw std::runtime_error("Invalid damage affinity source");
                detail::DamageAffinity affinity;
                affinity.source_id = first;
                if (second == "resistance")
                    affinity.kind = detail::AffinityKind::resistance;
                else if (second == "vulnerability")
                    affinity.kind = detail::AffinityKind::vulnerability;
                else if (second == "immunity")
                    affinity.kind = detail::AffinityKind::immunity;
                else
                    throw std::runtime_error("Unknown damage affinity");
                if (type != "all")
                    affinity.type = detail::damage_type(type);
                definition.affinities.push_back(std::move(affinity));
            }
            if (!row)
                throw std::runtime_error("Truncated damage profile");
            row >> std::ws;
            if (!row.eof())
                throw std::runtime_error("Unknown damage profile fields");
            continue;
        }
        if (tag == "size")
        {
            std::string size;
            row >> size;
            const std::array<std::string_view, 6> names{"tiny",  "small", "medium",
                    "large", "huge",  "gargantuan"};
            const auto found = std::find(names.begin(), names.end(), size);
            if (!row || !content.definitions.contains(key) || found == names.end() ||
                    !size_rows.insert(key).second)
                throw std::runtime_error("Invalid creature size");
            content.definitions.at(key).size = int(found - names.begin());
            row >> std::ws;
            if (!row.eof())
                throw std::runtime_error("Unknown creature size fields");
            continue;
        }
        if (tag == "type")
        {
            // SRD 5.2.1 creature types; a definition without a row is Humanoid.
            std::string type;
            row >> type;
            const std::array<std::string_view, 14> types{
                "aberration", "beast", "celestial", "construct", "dragon", "elemental", "fey",
                "fiend", "giant", "humanoid", "monstrosity", "ooze", "plant", "undead"};
            if (!row || !content.definitions.contains(key) ||
                    std::find(types.begin(), types.end(), type) == types.end() ||
                    !type_rows.insert(key).second)
                throw std::runtime_error("Invalid creature type");
            content.definitions.at(key).creature_type = type;
            row >> std::ws;
            if (!row.eof())
                throw std::runtime_error("Unknown creature type fields");
            continue;
        }
        if (tag == "pack_tactics" || tag == "aggressive" || tag == "advantage_damage")
        {
            const auto found = content.definitions.find(key);
            if (found == content.definitions.end() || !trait_rows.insert(tag + " " + key).second)
                throw std::runtime_error("Invalid creature trait: " + key);
            auto &definition = found->second;
            if (tag == "pack_tactics")
                definition.pack_tactics = true;
            else if (tag == "aggressive")
                definition.aggressive = true;
            else
            {
                auto &extra = definition.advantage_damage;
                row >> extra.count >> extra.sides;
                if (!row || extra.count < 1 || extra.count > 10 || extra.sides < 2 ||
                        extra.sides > 20)
                    throw std::runtime_error("Invalid Advantage damage: " + key);
            }
            row >> std::ws;
            if (!row.eof())
                throw std::runtime_error("Unknown creature trait fields: " + key);
            continue;
        }
        if (tag == "saves" || tag == "spellcasting")
        {
            const auto found = content.definitions.find(key);
            auto &seen = tag == "saves" ? save_rows : casting_rows;
            if (found == content.definitions.end() || !seen.insert(key).second)
                throw std::runtime_error("Invalid supplemental creature definition: " + key);
            auto &definition = found->second;
            if (tag == "saves")
            {
                for (auto &bonus : definition.saves)
                    row >> bonus;
                if (std::any_of(definition.saves.begin(), definition.saves.end(),
                                [](int n)
            {
                return n < -10 || n > 30;
            }))
                throw std::runtime_error("Invalid saving throw bonus");
            }
            else
            {
                // Authored creature rows keep the compact bitmask: they are a
                // handful of spells, so the 31-bit limit is no constraint there.
                // It is translated to ids at the boundary; nothing downstream
                // sees a mask.
                int casting_spells{};
                row >> definition.slots2 >> casting_spells;
                if (definition.level < 3 || definition.slots2 < 0 || definition.slots2 > 20 ||
                        casting_spells < 0 || casting_spells > 63)
                    throw std::runtime_error("Invalid supplemental spellcasting");
                definition.spells = detail::spells_from_mask(unsigned(casting_spells));
                definition.known_cantrips = detail::spells_of_level(definition.spells, true);
                if (detail::mask_from_spells(definition.spells) != unsigned(casting_spells))
                    throw std::runtime_error("Invalid supplemental spellcasting");
            }
            if (!row)
                throw std::runtime_error("Truncated supplemental creature definition");
            row >> std::ws;
            if (!row.eof())
                throw std::runtime_error("Unknown supplemental creature fields");
            continue;
        }
        int creature_spells{};
        row >> d.ac >> d.hp >> d.initiative >> d.speed >> d.melee_bonus >> d.melee.count >>
            d.melee.sides >> d.melee.bonus >> d.ranged_bonus >> d.ranged.count >> d.ranged.sides >>
            d.ranged.bonus >> d.range >> d.long_range >> d.winds >> d.slots >> d.casting >>
            d.level >> creature_spells;

        const bool ranged_none = d.range == 0 && d.long_range == 0 && d.ranged_bonus == 0 &&
                                 d.ranged.count == 0 && d.ranged.sides == 0 && d.ranged.bonus == 0;
        const bool ranged_weapon = d.range >= 5 && d.long_range >= d.range && d.long_range <= 600 &&
                                   d.ranged_bonus >= -10 && d.ranged_bonus <= 30 &&
                                   d.ranged.count >= 1 && d.ranged.count <= 10 &&
                                   d.ranged.sides >= 2 && d.ranged.sides <= 20 &&
                                   d.ranged.bonus >= -10 && d.ranged.bonus <= 30;
        if (!row || tag != "creature" || key.size() > 80 || content.definitions.contains(key) ||
                d.ac < 1 || d.ac > 40 || d.hp < 1 || d.hp > 1000 || d.initiative < -10 ||
                d.initiative > 20 || d.speed < 5 || d.speed > 120 || d.speed % 5 || d.melee.count < 1 ||
                d.melee.count > 10 || d.melee.sides < 2 || d.melee.sides > 20 ||
                (!ranged_none && !ranged_weapon) || d.winds < 0 || d.winds > 10 || d.slots < 0 ||
                d.slots > 20 || d.level < 1 || d.level > 4 || creature_spells < 0 ||
                creature_spells > 7 || d.melee_bonus < -10 || d.melee_bonus > 30 || d.casting < -10 ||
                d.casting > 30 || d.melee.bonus < -10 || d.melee.bonus > 30)
            throw std::runtime_error("Invalid or unsupported creature definition: " + key);
        d.spells = detail::spells_from_mask(unsigned(creature_spells));
        d.known_cantrips = detail::spells_of_level(d.spells, true);
        if (detail::mask_from_spells(d.spells) != unsigned(creature_spells))
            throw std::runtime_error("Invalid or unsupported creature definition: " + key);
        row >> std::ws;
        if (!row.eof())
            throw std::runtime_error("Unknown creature fields: " + key);
        content.definitions.emplace(std::move(key), d);
    }
    if (content.definitions.empty())
        throw std::runtime_error("Empty rules content");
    return std::make_unique<Module>(std::move(content));
}
} // namespace opengold::srd5
