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
#include "beast_forms.h"
#include "combat_grid.h"
#include "status_effects.h"
#include "concentration.h"
#include "life_cycle.h"
#include "recovery_timeline.h"
#include "weapons.h"
#include "thrown_gear.h"
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
#include <numeric>
#include <set>
#include <sstream>
#include <stdexcept>

namespace opengold::srd5
{
using namespace rules;
using detail::CharacterClass;

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

bool trained(CharacterClass klass, std::span<const FeatureGrant> grants, std::string_view key)
{
    // Starting-class grants, SRD 5.2.1 pp. 49 and 61, plus the Protector Divine
    // Order's Martial weapon and Heavy armor training. Multiclass entry and other
    // optional feature grants are separate, not yet implemented capabilities.
    // The Druid's Warden Primal Order: Martial weapons and Medium armor.
    const bool protector = detail::has_grant(grants, "order:protector");
    const bool warden = detail::has_grant(grants, "order:warden");
    if (const auto *weapon = detail::weapon(key))
        return detail::weapon_proficient(klass, *weapon) ||
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
           kind == detail::EffectKind::guidance ||
           kind == detail::EffectKind::expeditious_retreat || kind == detail::EffectKind::drowsy ||
           kind == detail::EffectKind::asleep || kind == detail::EffectKind::laughing ||
           kind == detail::EffectKind::webbed || kind == detail::EffectKind::enfeebled ||
           kind == detail::EffectKind::blur || kind == detail::EffectKind::invisible ||
           kind == detail::EffectKind::enlarged || kind == detail::EffectKind::reduced ||
           kind == detail::EffectKind::dragons_breath || kind == detail::EffectKind::extended ||
           kind == detail::EffectKind::hex || kind == detail::EffectKind::outlined ||
           kind == detail::EffectKind::flame_blade || kind == detail::EffectKind::heated ||
           kind == detail::EffectKind::shining;
}

// Command's option as players read it, such as "Approach".
std::string command_label(detail::CommandOption option)
{
    const auto index = static_cast<std::size_t>(option) - 1;
    auto label = std::string(detail::command_options.at(index));
    label[0] = static_cast<char>(label[0] - 'a' + 'A');
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
// Magic Missile's darts and Scorching Ray's rays are each aimed at a creature,
// several at one if the caster likes (SRD 5.2.1 pp. 146, 159).
bool splits_among_targets(const detail::SpellDef &spell)
{
    return spell.id == "magic_missile" || spell.id == "scorching_ray";
}

bool selects_creatures(const detail::SpellDef &spell)
{
    return splits_among_targets(spell) || spell.pattern == detail::SpellPattern::buff ||
           spell.rider == detail::Rider::command ||
           (spell.pattern == detail::SpellPattern::save_condition &&
            (spell.instances > 1 || spell.upcast.extra_instances));
}

// A smite follows the caster's own melee hit; divine_smite_free is Paladin's
// Smite's slotless cast and divine_smite_2 a cast from a level-2 slot.
bool is_smite(std::string_view verb)
{
    return verb == "divine_smite" || verb == "divine_smite_free" || verb == "divine_smite_2" ||
           verb == "searing_smite" || verb == "shining_smite" || verb == "ensnaring_strike";
}

std::string equipment_note(const CharacterSheet &sheet, std::string_view item)
{
    std::string text;
    if (trained(detail::class_of(sheet), sheet.grants, item))
        text = "Class training: no untrained-use penalty.";
    else if (item == "shield")
        text = "Untrained shield: no AC bonus.";
    else if (detail::armor(item))
        text =
            "Untrained armor: disadvantage on Strength/Dexterity attacks, checks (including initiative), and saves; cannot cast spells.";
    else
        text = "Untrained weapon: no proficiency bonus on attack rolls.";
    if (item == "chain_mail" && sheet.scores[Ability::strength] < 13)
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
constexpr unsigned checkpoint_format = 52;
// MELEE-1: as in the original game, melee reaches adjacent squares only; the
// SRD's 10-foot reach (polearms, the whip, a troll's claws) is not used.
constexpr int melee_reach = 5;

// Which spells a class may legitimately have stored at a level. This replaces a
// packed allow-mask, which could not express a spell beyond the 31st bit.
struct SpellAccessRow
{
    CharacterClass klass;
    std::string_view spell;
    unsigned min_level;
};

constexpr std::array class_spell_access
{
    SpellAccessRow{CharacterClass::cleric, "cure_wounds", 1},
    SpellAccessRow{CharacterClass::cleric, "healing_word", 1},
    SpellAccessRow{CharacterClass::cleric, "blindness", 3},
    SpellAccessRow{CharacterClass::cleric, "sacred_flame", 1},
    SpellAccessRow{CharacterClass::cleric, "inflict_wounds", 1},
    SpellAccessRow{CharacterClass::paladin, "cure_wounds", 1},
    SpellAccessRow{CharacterClass::paladin, "divine_smite", 1},
    SpellAccessRow{CharacterClass::paladin, "searing_smite", 1},
    SpellAccessRow{CharacterClass::paladin, "shield_of_faith", 1},
    SpellAccessRow{CharacterClass::paladin, "heroism", 1},
    SpellAccessRow{CharacterClass::paladin, "divine_favor", 1},
    SpellAccessRow{CharacterClass::cleric, "shield_of_faith", 1},
    SpellAccessRow{CharacterClass::cleric, "bless", 1},
    SpellAccessRow{CharacterClass::paladin, "bless", 1},
    SpellAccessRow{CharacterClass::paladin, "protection_from_evil_and_good", 1},
    SpellAccessRow{CharacterClass::cleric, "protection_from_evil_and_good", 1},
    SpellAccessRow{CharacterClass::paladin, "command", 1},
    // Paladin level-2 spells, with the level-2 slots of Paladin level 5.
    SpellAccessRow{CharacterClass::paladin, "aid", 5},
    SpellAccessRow{CharacterClass::paladin, "lesser_restoration", 5},
    SpellAccessRow{CharacterClass::paladin, "magic_weapon", 5},
    SpellAccessRow{CharacterClass::paladin, "prayer_of_healing", 5},
    SpellAccessRow{CharacterClass::paladin, "protection_from_poison", 5},
    SpellAccessRow{CharacterClass::paladin, "warding_bond", 5},
    SpellAccessRow{CharacterClass::paladin, "shining_smite", 5},
    SpellAccessRow{CharacterClass::ranger, "cure_wounds", 1},
    SpellAccessRow{CharacterClass::ranger, "hunters_mark", 1},
    SpellAccessRow{CharacterClass::ranger, "longstrider", 1},
    SpellAccessRow{CharacterClass::ranger, "goodberry", 1},
    SpellAccessRow{CharacterClass::ranger, "ensnaring_strike", 1},
    SpellAccessRow{CharacterClass::ranger, "entangle", 1},
    SpellAccessRow{CharacterClass::ranger, "fog_cloud", 1},
    // Blessed and Druidic Warrior's cantrips; spell access checks the feature itself.
    SpellAccessRow{CharacterClass::ranger, "poison_spray", 2},
    SpellAccessRow{CharacterClass::paladin, "sacred_flame", 2},
    SpellAccessRow{CharacterClass::cleric, "command", 1},
    SpellAccessRow{CharacterClass::cleric, "lesser_restoration", 3},
    SpellAccessRow{CharacterClass::cleric, "aid", 3},
    SpellAccessRow{CharacterClass::cleric, "guiding_bolt", 1},
    SpellAccessRow{CharacterClass::cleric, "bane", 1},
    SpellAccessRow{CharacterClass::cleric, "spare_the_dying", 1},
    SpellAccessRow{CharacterClass::cleric, "hold_person", 3},
    SpellAccessRow{CharacterClass::cleric, "sanctuary", 1},
    SpellAccessRow{CharacterClass::cleric, "warding_bond", 3},
    SpellAccessRow{CharacterClass::cleric, "protection_from_poison", 3},
    SpellAccessRow{CharacterClass::cleric, "resistance", 1},
    SpellAccessRow{CharacterClass::cleric, "guidance", 1},
    SpellAccessRow{CharacterClass::cleric, "silence", 3},
    SpellAccessRow{CharacterClass::cleric, "spiritual_weapon", 3},
    SpellAccessRow{CharacterClass::cleric, "prayer_of_healing", 3},
    SpellAccessRow{CharacterClass::paladin, "resistance", 2},
    SpellAccessRow{CharacterClass::ranger, "resistance", 2},
    SpellAccessRow{CharacterClass::paladin, "guidance", 2},
    SpellAccessRow{CharacterClass::ranger, "guidance", 2},
    SpellAccessRow{CharacterClass::paladin, "spare_the_dying", 2},
    SpellAccessRow{CharacterClass::ranger, "spare_the_dying", 2},
    SpellAccessRow{CharacterClass::wizard, "fire_bolt", 1},
    SpellAccessRow{CharacterClass::wizard, "magic_missile", 1},
    SpellAccessRow{CharacterClass::wizard, "scorching_ray", 3},
    SpellAccessRow{CharacterClass::wizard, "protection_from_evil_and_good", 1},
    SpellAccessRow{CharacterClass::wizard, "longstrider", 1},
    SpellAccessRow{CharacterClass::wizard, "fog_cloud", 1},
    SpellAccessRow{CharacterClass::wizard, "hold_person", 3},
    SpellAccessRow{CharacterClass::wizard, "burning_hands", 1},
    SpellAccessRow{CharacterClass::wizard, "thunderwave", 1},
    SpellAccessRow{CharacterClass::wizard, "shatter", 3},
    SpellAccessRow{CharacterClass::wizard, "mage_armor", 1},
    SpellAccessRow{CharacterClass::wizard, "false_life", 1},
    SpellAccessRow{CharacterClass::wizard, "expeditious_retreat", 1},
    SpellAccessRow{CharacterClass::wizard, "ray_of_sickness", 1},
    SpellAccessRow{CharacterClass::wizard, "ice_knife", 1},
    SpellAccessRow{CharacterClass::wizard, "chromatic_orb", 1},
    SpellAccessRow{CharacterClass::wizard, "acid_splash", 1},
    SpellAccessRow{CharacterClass::wizard, "sleep", 1},
    SpellAccessRow{CharacterClass::wizard, "hideous_laughter", 1},
    SpellAccessRow{CharacterClass::wizard, "color_spray", 1},
    SpellAccessRow{CharacterClass::wizard, "grease", 1},
    SpellAccessRow{CharacterClass::wizard, "web", 3},
    SpellAccessRow{CharacterClass::wizard, "shield", 1},
    SpellAccessRow{CharacterClass::wizard, "misty_step", 3},
    SpellAccessRow{CharacterClass::wizard, "acid_arrow", 3},
    SpellAccessRow{CharacterClass::wizard, "mind_spike", 3},
    SpellAccessRow{CharacterClass::wizard, "ray_of_enfeeblement", 3},
    SpellAccessRow{CharacterClass::wizard, "blur", 3},
    SpellAccessRow{CharacterClass::wizard, "mirror_image", 3},
    SpellAccessRow{CharacterClass::wizard, "magic_weapon", 3},
    SpellAccessRow{CharacterClass::wizard, "invisibility", 3},
    SpellAccessRow{CharacterClass::wizard, "see_invisibility", 3},
    SpellAccessRow{CharacterClass::wizard, "darkness", 3},
    SpellAccessRow{CharacterClass::wizard, "flaming_sphere", 3},
    SpellAccessRow{CharacterClass::wizard, "knock", 3},
    SpellAccessRow{CharacterClass::wizard, "enlarge_reduce", 3},
    SpellAccessRow{CharacterClass::wizard, "true_strike", 1},
    SpellAccessRow{CharacterClass::wizard, "dragons_breath", 3},
    SpellAccessRow{CharacterClass::wizard, "charm_person", 1},
    SpellAccessRow{CharacterClass::wizard, "gust_of_wind", 3},
    SpellAccessRow{CharacterClass::wizard, "blindness", 3},
    SpellAccessRow{CharacterClass::wizard, "poison_spray", 1},
    SpellAccessRow{CharacterClass::wizard, "ray_of_frost", 1},
    SpellAccessRow{CharacterClass::wizard, "shocking_grasp", 1},
    SpellAccessRow{CharacterClass::wizard, "chill_touch", 1},
    SpellAccessRow{CharacterClass::sorcerer, "fire_bolt", 1},
    SpellAccessRow{CharacterClass::sorcerer, "poison_spray", 1},
    SpellAccessRow{CharacterClass::sorcerer, "ray_of_frost", 1},
    SpellAccessRow{CharacterClass::sorcerer, "shocking_grasp", 1},
    SpellAccessRow{CharacterClass::sorcerer, "chill_touch", 1},
    SpellAccessRow{CharacterClass::sorcerer, "acid_splash", 1},
    SpellAccessRow{CharacterClass::sorcerer, "true_strike", 1},
    // Prepared Sorcerer spells; Command comes only from Draconic Spells.
    SpellAccessRow{CharacterClass::sorcerer, "burning_hands", 1},
    SpellAccessRow{CharacterClass::sorcerer, "charm_person", 1},
    SpellAccessRow{CharacterClass::sorcerer, "chromatic_orb", 1},
    SpellAccessRow{CharacterClass::sorcerer, "color_spray", 1},
    SpellAccessRow{CharacterClass::sorcerer, "expeditious_retreat", 1},
    SpellAccessRow{CharacterClass::sorcerer, "false_life", 1},
    SpellAccessRow{CharacterClass::sorcerer, "fog_cloud", 1},
    SpellAccessRow{CharacterClass::sorcerer, "grease", 1},
    SpellAccessRow{CharacterClass::sorcerer, "ice_knife", 1},
    SpellAccessRow{CharacterClass::sorcerer, "mage_armor", 1},
    SpellAccessRow{CharacterClass::sorcerer, "magic_missile", 1},
    SpellAccessRow{CharacterClass::sorcerer, "ray_of_sickness", 1},
    SpellAccessRow{CharacterClass::sorcerer, "shield", 1},
    SpellAccessRow{CharacterClass::sorcerer, "sleep", 1},
    SpellAccessRow{CharacterClass::sorcerer, "thunderwave", 1},
    SpellAccessRow{CharacterClass::sorcerer, "sorcerous_burst", 1},
    SpellAccessRow{CharacterClass::sorcerer, "blindness", 3},
    SpellAccessRow{CharacterClass::sorcerer, "blur", 3},
    SpellAccessRow{CharacterClass::sorcerer, "darkness", 3},
    SpellAccessRow{CharacterClass::sorcerer, "dragons_breath", 3},
    SpellAccessRow{CharacterClass::sorcerer, "enlarge_reduce", 3},
    SpellAccessRow{CharacterClass::sorcerer, "flaming_sphere", 3},
    SpellAccessRow{CharacterClass::sorcerer, "gust_of_wind", 3},
    SpellAccessRow{CharacterClass::sorcerer, "hold_person", 3},
    SpellAccessRow{CharacterClass::sorcerer, "invisibility", 3},
    SpellAccessRow{CharacterClass::sorcerer, "knock", 3},
    SpellAccessRow{CharacterClass::sorcerer, "magic_weapon", 3},
    SpellAccessRow{CharacterClass::sorcerer, "mind_spike", 3},
    SpellAccessRow{CharacterClass::sorcerer, "mirror_image", 3},
    SpellAccessRow{CharacterClass::sorcerer, "misty_step", 3},
    SpellAccessRow{CharacterClass::sorcerer, "scorching_ray", 3},
    SpellAccessRow{CharacterClass::sorcerer, "see_invisibility", 3},
    SpellAccessRow{CharacterClass::sorcerer, "shatter", 3},
    SpellAccessRow{CharacterClass::sorcerer, "web", 3},
    SpellAccessRow{CharacterClass::sorcerer, "command", 3},
    SpellAccessRow{CharacterClass::bard, "true_strike", 1},
    SpellAccessRow{CharacterClass::bard, "vicious_mockery", 1},
    SpellAccessRow{CharacterClass::bard, "starry_wisp", 1},
    SpellAccessRow{CharacterClass::bard, "bane", 1},
    SpellAccessRow{CharacterClass::bard, "charm_person", 1},
    SpellAccessRow{CharacterClass::bard, "color_spray", 1},
    SpellAccessRow{CharacterClass::bard, "command", 1},
    SpellAccessRow{CharacterClass::bard, "cure_wounds", 1},
    SpellAccessRow{CharacterClass::bard, "dissonant_whispers", 1},
    SpellAccessRow{CharacterClass::bard, "faerie_fire", 1},
    SpellAccessRow{CharacterClass::bard, "healing_word", 1},
    SpellAccessRow{CharacterClass::bard, "heroism", 1},
    SpellAccessRow{CharacterClass::bard, "hideous_laughter", 1},
    SpellAccessRow{CharacterClass::bard, "longstrider", 1},
    SpellAccessRow{CharacterClass::bard, "sleep", 1},
    SpellAccessRow{CharacterClass::bard, "thunderwave", 1},
    SpellAccessRow{CharacterClass::bard, "aid", 3},
    SpellAccessRow{CharacterClass::bard, "blindness", 3},
    SpellAccessRow{CharacterClass::bard, "enlarge_reduce", 3},
    SpellAccessRow{CharacterClass::bard, "hold_person", 3},
    SpellAccessRow{CharacterClass::bard, "invisibility", 3},
    SpellAccessRow{CharacterClass::bard, "knock", 3},
    SpellAccessRow{CharacterClass::bard, "lesser_restoration", 3},
    SpellAccessRow{CharacterClass::bard, "mirror_image", 3},
    SpellAccessRow{CharacterClass::bard, "see_invisibility", 3},
    SpellAccessRow{CharacterClass::bard, "shatter", 3},
    SpellAccessRow{CharacterClass::bard, "silence", 3},
    SpellAccessRow{CharacterClass::druid, "produce_flame", 1},
    SpellAccessRow{CharacterClass::druid, "shillelagh", 1},
    SpellAccessRow{CharacterClass::druid, "poison_spray", 1},
    SpellAccessRow{CharacterClass::druid, "resistance", 1},
    SpellAccessRow{CharacterClass::druid, "guidance", 1},
    SpellAccessRow{CharacterClass::druid, "spare_the_dying", 1},
    SpellAccessRow{CharacterClass::druid, "starry_wisp", 1},
    SpellAccessRow{CharacterClass::druid, "charm_person", 1},
    SpellAccessRow{CharacterClass::druid, "cure_wounds", 1},
    SpellAccessRow{CharacterClass::druid, "entangle", 1},
    SpellAccessRow{CharacterClass::druid, "faerie_fire", 1},
    SpellAccessRow{CharacterClass::druid, "fog_cloud", 1},
    SpellAccessRow{CharacterClass::druid, "goodberry", 1},
    SpellAccessRow{CharacterClass::druid, "healing_word", 1},
    SpellAccessRow{CharacterClass::druid, "ice_knife", 1},
    SpellAccessRow{CharacterClass::druid, "longstrider", 1},
    SpellAccessRow{CharacterClass::druid, "protection_from_evil_and_good", 1},
    SpellAccessRow{CharacterClass::druid, "thunderwave", 1},
    SpellAccessRow{CharacterClass::druid, "aid", 3},
    SpellAccessRow{CharacterClass::druid, "barkskin", 3},
    SpellAccessRow{CharacterClass::druid, "flame_blade", 3},
    SpellAccessRow{CharacterClass::druid, "moonbeam", 3},
    SpellAccessRow{CharacterClass::druid, "spike_growth", 3},
    SpellAccessRow{CharacterClass::druid, "heat_metal", 3},
    SpellAccessRow{CharacterClass::bard, "heat_metal", 3},
    SpellAccessRow{CharacterClass::druid, "enlarge_reduce", 3},
    SpellAccessRow{CharacterClass::druid, "flaming_sphere", 3},
    SpellAccessRow{CharacterClass::druid, "gust_of_wind", 3},
    SpellAccessRow{CharacterClass::druid, "hold_person", 3},
    SpellAccessRow{CharacterClass::druid, "lesser_restoration", 3},
    SpellAccessRow{CharacterClass::druid, "protection_from_poison", 3},
    // Circle Spells come only from the land chosen at level three.
    SpellAccessRow{CharacterClass::druid, "blur", 3},
    SpellAccessRow{CharacterClass::druid, "burning_hands", 3},
    SpellAccessRow{CharacterClass::druid, "fire_bolt", 3},
    SpellAccessRow{CharacterClass::druid, "ray_of_frost", 3},
    SpellAccessRow{CharacterClass::druid, "misty_step", 3},
    SpellAccessRow{CharacterClass::druid, "shocking_grasp", 3},
    SpellAccessRow{CharacterClass::druid, "sleep", 3},
    SpellAccessRow{CharacterClass::druid, "acid_splash", 3},
    SpellAccessRow{CharacterClass::druid, "ray_of_sickness", 3},
    SpellAccessRow{CharacterClass::druid, "web", 3},
    // Fiend Spells come only from the Fiend Patron.
    SpellAccessRow{CharacterClass::warlock, "burning_hands", 3},
    SpellAccessRow{CharacterClass::warlock, "command", 3},
    SpellAccessRow{CharacterClass::warlock, "scorching_ray", 3},
    SpellAccessRow{CharacterClass::warlock, "chill_touch", 1},
    SpellAccessRow{CharacterClass::warlock, "eldritch_blast", 1},
    SpellAccessRow{CharacterClass::warlock, "poison_spray", 1},
    SpellAccessRow{CharacterClass::warlock, "true_strike", 1},
    SpellAccessRow{CharacterClass::warlock, "bane", 1},
    SpellAccessRow{CharacterClass::warlock, "charm_person", 1},
    SpellAccessRow{CharacterClass::warlock, "expeditious_retreat", 1},
    SpellAccessRow{CharacterClass::warlock, "hellish_rebuke", 1},
    SpellAccessRow{CharacterClass::warlock, "hex", 1},
    SpellAccessRow{CharacterClass::warlock, "hideous_laughter", 1},
    SpellAccessRow{CharacterClass::warlock, "protection_from_evil_and_good", 1},
    SpellAccessRow{CharacterClass::warlock, "darkness", 3},
    SpellAccessRow{CharacterClass::warlock, "hold_person", 3},
    SpellAccessRow{CharacterClass::warlock, "invisibility", 3},
    SpellAccessRow{CharacterClass::warlock, "mind_spike", 3},
    SpellAccessRow{CharacterClass::warlock, "mirror_image", 3},
    SpellAccessRow{CharacterClass::warlock, "misty_step", 3},
    SpellAccessRow{CharacterClass::warlock, "ray_of_enfeeblement", 3}};

std::vector<std::string> allowed_spells(CharacterClass klass, unsigned level)
{
    std::vector<std::string> result;
    for (const auto &row : class_spell_access)
        if (row.klass == klass && level >= row.min_level)
            result.emplace_back(row.spell);
    return result;
}

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

std::string_view giant_boon_label(GiantBoon boon)
{
    switch (boon)
    {
    case GiantBoon::cloud:
        return "Cloud's Jaunt";
    case GiantBoon::fire:
        return "Fire's Burn";
    case GiantBoon::frost:
        return "Frost's Chill";
    case GiantBoon::hill:
        return "Hill's Tumble";
    case GiantBoon::stone:
        return "Stone's Endurance";
    case GiantBoon::storm:
        return "Storm's Thunder";
    case GiantBoon::none:
        break;
    }
    return {};
}

// Spell slots by spell level, 1 to 9; index 0 is unused.
using SpellSlots = std::array<int, 10>;
inline constexpr unsigned highest_slot_level = 9;

// "Level-one spell slots" to "Level-nine spell slots", by slot level.
constexpr std::array<std::string_view, 10> slot_labels{
    "", "Level-one spell slots", "Level-two spell slots", "Level-three spell slots",
    "Level-four spell slots", "Level-five spell slots", "Level-six spell slots",
    "Level-seven spell slots", "Level-eight spell slots", "Level-nine spell slots"};

int slot_total(const SpellSlots &slots)
{
    return std::accumulate(slots.begin(), slots.end(), 0);
}

// Spell slots by class level, SRD 5.2.1's class tables: the full casters, the
// Paladin and Ranger, and the Warlock's Pact Magic, whose slots share one level.
SpellSlots spell_slots(CharacterClass klass, unsigned level)
{
    using Row = std::array<int, 9>;
    static constexpr std::array<Row, 15> full{{
        {2}, {3}, {4, 2}, {4, 3}, {4, 3, 2}, {4, 3, 3}, {4, 3, 3, 1}, {4, 3, 3, 2},
        {4, 3, 3, 3, 1}, {4, 3, 3, 3, 2}, {4, 3, 3, 3, 2, 1}, {4, 3, 3, 3, 2, 1},
        {4, 3, 3, 3, 2, 1, 1}, {4, 3, 3, 3, 2, 1, 1}, {4, 3, 3, 3, 2, 1, 1, 1}}};
    static constexpr std::array<Row, 15> half{{
        {2}, {2}, {3}, {3}, {4, 2}, {4, 2}, {4, 3}, {4, 3}, {4, 3, 2}, {4, 3, 2},
        {4, 3, 3}, {4, 3, 3}, {4, 3, 3, 1}, {4, 3, 3, 1}, {4, 3, 3, 2}}};
    SpellSlots result{};
    if (level < 1 || level > full.size())
        return result;
    if (klass == CharacterClass::warlock)
    {
        const unsigned pact_level = std::min(5u, (level + 1) / 2);
        result[pact_level] = level == 1 ? 1 : level >= 11 ? 3 : 2;
        return result;
    }
    const bool full_caster = klass == CharacterClass::cleric || klass == CharacterClass::wizard ||
                             klass == CharacterClass::sorcerer || klass == CharacterClass::bard ||
                             klass == CharacterClass::druid;
    const bool half_caster = klass == CharacterClass::paladin || klass == CharacterClass::ranger;
    if (full_caster || half_caster)
    {
        const auto &row = (full_caster ? full : half)[level - 1];
        std::copy(row.begin(), row.end(), result.begin() + 1);
    }
    return result;
}

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
    int range{}, long_range{}, winds{}, casting{}, level{};
    SpellSlots slots{};
    int extra_attacks{}; // Extra Attack: attacks after the Attack action's first
    // The lowest d20 that is a Critical Hit with a weapon or Unarmed Strike:
    // the Champion's Improved Critical (19) and Superior Critical (18).
    int critical_from{20};
    int indomitable{};   // Indomitable's uses per Long Rest
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
    // Magic Initiate: the spell attack bonus (the feat's ability modifier plus
    // Proficiency Bonus) of each spell it teaches, and each feat's level-one
    // spell in feat order, cast once per Long Rest without a slot.
    std::map<std::string, int, std::less<>> initiate_casting;
    // The free casts that come from a lineage rather than Magic Initiate.
    std::set<std::string, std::less<>> lineage_spells;
    std::vector<std::string> initiate_spells;
    int sorcery_points{}; // Sorcerer level 2: Font of Magic, in Actor::lay_on_hands
    std::vector<std::string> metamagic; // the Sorcerer's Metamagic options
    bool pact_magic{};    // Warlock: slots of one level, back on a Short Rest
    // Eldritch Invocations.
    bool armor_of_shadows{}, devils_sight{}, eldritch_mind{}, fiendish_vigor{},
         pact_of_the_blade{};
    // The cantrips Agonizing Blast, Eldritch Spear and Repelling Blast improve.
    std::set<std::string, std::less<>> agonizing, eldritch_spear, repelling;
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
    bool prone_bite{}, bloodied_fury{}, charge{}; // a Beast form's attack traits
    bool deflect{};       // Monk level 3: Deflect Attacks
    bool open_hand{};     // Warrior of the Open Hand, Monk level 3
    int focus_dc{};       // 8 + Wisdom + Proficiency, for Focus features' saves
    int dexterity{};      // Dexterity modifier
    bool frenzy{};                   // Berserker, Barbarian level 3
    bool heavy_armor{};   // wearing Heavy armor, which prevents Rage
    // Hunter, Ranger level 3: Hunter's Lore and one Hunter's Prey option.
    bool hunters_lore{}, colossus_slayer{}, horde_breaker{};
    bool gnomish_cunning{}; // Gnome: Advantage on Intelligence, Wisdom and Charisma saves
    bool relentless{}; // Orc: Relentless Endurance, once per Long Rest
    // Dragonborn: the Breath Weapon's damage type, from the Draconic Ancestry.
    std::optional<detail::DamageType> breath;
    GiantBoon giant{GiantBoon::none}; // Goliath: the Giant Ancestry's boon
    int trait_uses{}; // uses of a species trait per Long Rest (Breath Weapon, Giant Ancestry)
    bool resourceful{}; // Human: Heroic Inspiration, back after each Long Rest
    bool lucky{}; // Halfling: a d20 test's 1 is rolled again
    bool nimble{}; // Halfling Nimbleness: moves through larger creatures' spaces
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
    bool brave{}; // Halfling: Advantage on saves to avoid or end Frightened
    // Rogue levels 5-10.
    bool cunning_strike{}, uncanny_dodge{}, evasion{}, supreme_sneak{};
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
    // Reliable Talent (Rogue 7): the proficient skills whose d20 counts as at least 10.
    std::set<std::string, std::less<>> reliable;
    bool dwarf{}, cunning{}, tactical_mind{}, champion{}, great_weapon_fighting{},
         two_weapon_fighting{};
    unsigned sneak_level{};
    bool finesse{}, ranged_weapon{};
    int medicine{};
    int athletics{}; // a character's Strength (Athletics) check bonus
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
    // A second damage type a ranged hit adds (the Hobgoblin Warrior's poisoned arrows).
    Dice ranged_extra;
    detail::DamageType ranged_extra_type{detail::DamageType::poison};
    // Hit Points regained at the start of each turn; Acid or Fire damage stops it
    // for the next turn, and the creature dies only at 0 HP without regenerating.
    int regeneration{};
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
    // The Slums' set encounters and Kuto's Well: their SRD stat blocks' attacks.
    if (definition == "slums-hobgoblin")
        return {"Hobgoblin", "Longsword", "Longbow"};
    if (definition == "ogre")
        return {"Ogre", "Greatclub", "Javelin"};
    if (definition == "troll")
        return {"Troll", "Rend", nullptr};
    if (definition == "slums-magic-user")
        return {"Magic-User", "Dagger", nullptr};
    if (definition == "gnoll-warrior")
        return {"Gnoll Warrior", "Rend", "Bone bow"};
    if (definition == "lizardfolk")
        return {"Lizardfolk", "Bite", "Javelin"};
    if (definition == "giant-lizard")
        return {"Giant Lizard", "Bite", nullptr};
    if (definition == "norris-the-gray")
        return {"Norris the Gray", "Scimitar", nullptr};
    return {nullptr, nullptr, nullptr};
}

struct Content
{
    Identity identity;
    std::map<std::string, Definition> definitions;
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
    int hit_dice{}, rushes{}, surges{}, dashes{}, arcane{}, lay_on_hands{},
        // Remaining free casts of Paladin's Smite or Favored Enemy: a character has at
        // most one of them while multiclassing is deferred (SCOPE-1).
        free_casts{},
        channel_divinity{};
    // Magic Initiate's unspent free casts: bit n is Definition::initiate_spells[n].
    int initiate_casts{};
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
    bool attacked{}; // made an attack roll on its own turn (Reckless Attack's first)
    int pounce{};    // Instinctive Pounce's feet, usable only right after entering a Rage
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
    bool resistance_used{}; // Resistance (the cantrip) reduces damage once per turn
    bool regeneration_blocked{}; // Took Acid or Fire damage since its last turn began
    // Downed with Regeneration: the round it may get back up, 3d6 rounds after it
    // fell (the original game's troll); 0 otherwise.
    int rises_on_round{};
    // Thrown gear (Acid, Alchemist's Fire, Oil, by ThrownGearEffect) still carried:
    // counted from the inventory when combat begins and spent by each throw.
    std::array<unsigned, 3> thrown_gear_left{};
    bool burning{};          // Burning: 1d4 Fire damage at the start of each of its turns
    int oiled_until_round{}; // Covered in Oil through this round: Fire damage deals 5 more
    bool shield_off{};       // Took its shield off this fight (for a two-handed bow)
    // Tried to run off the field as fast as the fastest enemy and failed: it
    // stays until the fight ends (the original's rule).
    bool must_stay{};
    // Ran off the field this turn; it leaves the fight when the turn ends.
    bool fled{};
    // Its morale broke: it runs for the edge until it rallies.
    bool panicked{};
    // Gave up: it leaves the fight, counted as defeated.
    bool surrendered{};
    EntityId horde_origin{};
    detail::ConcentrationState concentration; // the one Concentration spell this actor keeps
    // Wild Shape: the Beast form's statistics, derived from the wild_shape effect.
    std::optional<Definition> form;
    bool light_damage{};        // Transient attack copy only; pending hits carry their own flag.
    bool involuntary_overlap{}; // Interrupted in an occupied space; retained through recovery until
    // separated.
    detail::EffectState effects;
};

// The actor's AC with its lasting effects. Mage Armor replaces an unarmored
// base AC. Barkskin: an AC of 17 if it was lower. Shield of Faith: +2. Warding
// Bond: +1. Shield: +5.
int armor_class_of(const Actor &target)
{
    const auto &d = target.form ? *target.form : target.definition;
    const int base = detail::has_effect(target.effects, detail::EffectKind::mage_armor)
                     ? std::max(d.ac, d.mage_armor_ac)
                     : d.ac;
    return (detail::has_effect(target.effects, detail::EffectKind::barkskin) ? std::max(base, 17)
            : base) +
           (detail::has_effect(target.effects, detail::EffectKind::shield_of_faith) ? 2 : 0) +
           (detail::has_effect(target.effects, detail::EffectKind::warding_bond) ? 1 : 0) +
           (detail::has_effect(target.effects, detail::EffectKind::shield) ? 5 : 0);
}

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
    unsigned slot_level{}; // a spell slot row: its level, in place of the members
};

int remaining_of(const ResourceDescriptor &descriptor, const Actor &a)
{
    return descriptor.slot_level ? a.slots[descriptor.slot_level] : a.*descriptor.remaining;
}

int capacity_of(const ResourceDescriptor &descriptor, const Definition &d)
{
    return descriptor.slot_level ? d.slots[descriptor.slot_level] : d.*descriptor.capacity;
}

constexpr std::array resource_descriptors
{
    ResourceDescriptor{"action_surge", "Action Surge", &Actor::surges, &Definition::surges, -1,
        true},
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
        &Definition::metabolism, 0, false},
    ResourceDescriptor{"indomitable", "Indomitable", &Actor::arcane, &Definition::indomitable, 0,
        true},
    ResourceDescriptor{"wholeness_of_body", "Wholeness of Body", &Actor::free_casts,
        &Definition::wholeness, 0, true},
    ResourceDescriptor{"relentless_rage", "Relentless Rage", &Actor::lay_on_hands,
        &Definition::relentless_rage, -1, true},
    ResourceDescriptor{"intimidating_presence", "Intimidating Presence", &Actor::free_casts,
        &Definition::intimidating_presence, 0, true},
    ResourceDescriptor{"persistent_rage", "Persistent Rage", &Actor::arcane,
        &Definition::persistent_rage, 0, true}};

// Whether a level-up takes Lessons of the First Ones' Skilled, as a new
// invocation or one swapped in.
bool takes_lessons_skilled(const AdvancementChoice &choice)
{
    const auto picks = choice.training.find("class:warlock:invocations:2");
    return choice.option_swap.replacement == "lessons_skilled" ||
           (picks != choice.training.end() &&
            std::find(picks->second.begin(), picks->second.end(), "lessons_skilled") !=
            picks->second.end());
}

// The Hit Point maximum with Aid's increase.
int max_hp(const Actor &a)
{
    return a.definition.hp + detail::hit_point_bonus(a.effects);
}

// Lay On Hands and Sorcery Points share one store, restored by a Long Rest; no
// class has both.
int lay_capacity(const Definition &d)
{
    return d.lay_on_hands + d.sorcery_points + d.relentless_rage;
}

// Spell slots of a level, with room for the ones Font of Magic creates from
// Sorcery Points (2, 3, 5, 6 and 7 points for levels 1 to 5); they vanish on a
// Long Rest.
int slot_room(const Definition &d, unsigned level)
{
    constexpr std::array<int, 6> cost{0, 2, 3, 5, 6, 7};
    return d.slots[level] + (level < cost.size() ? d.sorcery_points / cost[level] : 0);
}

// Every slot level holds between none and the room for it.
bool slots_in_range(const SpellSlots &slots, const Definition &d)
{
    for (unsigned level = 1; level <= highest_slot_level; ++level)
        if (slots[level] < 0 || slots[level] > slot_room(d, level))
            return false;
    return slots[0] == 0;
}

// The lowest slot level, `from` or above, with a slot left; 0 if none.
unsigned lowest_slot(const Actor &a, unsigned from)
{
    for (unsigned level = from; level <= highest_slot_level; ++level)
        if (a.slots[level] > 0)
            return level;
    return 0;
}

// Paladin's Smite, Favored Enemy and Innate Sorcery share one store, restored
// by a Long Rest; no class has two of them.
int free_cast_capacity(const Definition &d)
{
    return d.free_smite + d.favored_enemy + d.innate_sorcery + d.wholeness +
           d.intimidating_presence;
}

// One bit per Magic Initiate feat, each its spell's free cast.
int initiate_capacity(const Definition &d)
{
    return (1 << d.initiate_spells.size()) - 1;
}

// The bit of a spell's Magic Initiate free cast, or 0 when no feat casts it free.
int initiate_bit(const Definition &d, std::string_view spell)
{
    const auto found = std::find(d.initiate_spells.begin(), d.initiate_spells.end(), spell);
    return found == d.initiate_spells.end()
           ? 0 : 1 << (found - d.initiate_spells.begin());
}

// The spell attack bonus a spell uses: the ability Magic Initiate chose for
// the spells it teaches, the class's spellcasting ability for every other.
int casting_bonus(const Definition &d, std::string_view spell)
{
    const auto found = d.initiate_casting.find(spell);
    return found == d.initiate_casting.end() ? d.casting : found->second;
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
    return d.arcane + d.metabolism + d.magical_cunning + d.bardic_inspiration + d.indomitable +
           d.persistent_rage;
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
    d.charge = form.charge;
    return d;
}

rules::ResourcePool resource_pool(const ResourceDescriptor &descriptor, const Actor &actor,
                                  const Definition &d)
{
    const auto capacity = static_cast<unsigned>(capacity_of(descriptor, d));
    return {std::string(descriptor.id),
        {std::string(descriptor.label), {}},
        static_cast<unsigned>(remaining_of(descriptor, actor)),
        capacity,
        descriptor.short_rest < 0 ? capacity : static_cast<unsigned>(descriptor.short_rest)};
}

// Magic Initiate's free casts: one pool per feat, named for its spell.
void add_initiate_pools(std::vector<rules::ResourcePool> &pools, const Actor &actor,
                        const Definition &d)
{
    for (std::size_t n = 0; n < d.initiate_spells.size(); ++n)
        pools.push_back({"magic_initiate:" + d.initiate_spells[n],
            {   d.lineage_spells.contains(d.initiate_spells[n]) ? "Lineage: {spell}"
                : "Magic Initiate: {spell}",
                {{"spell", std::string(detail::find_spell(d.initiate_spells[n])->label), true}}
            },
            (actor.initiate_casts >> n) & 1 ? 1u : 0u, 1u, 0u});
}

// A hidden creature's Stealth total: the DC to find it.
int stealth_total(const Actor &a)
{
    for (const auto &effect : a.effects.active)
        if (effect.kind == detail::EffectKind::hidden)
            return effect.dc;
    return 0;
}

// Uses of a species trait spent since the last Long Rest.
int trait_uses_spent(const Actor &a)
{
    for (const auto &effect : a.effects.active)
        if (effect.kind == detail::EffectKind::trait_uses)
            return effect.dc;
    return 0;
}

int trait_uses_left(const Actor &a)
{
    return a.definition.trait_uses - trait_uses_spent(a);
}

// A species trait's uses, shown with the class resources.
void add_trait_pool(std::vector<rules::ResourcePool> &pools, const Actor &actor,
                    const Definition &d)
{
    if (d.breath)
        pools.push_back({"breath_weapon", {"Breath Weapon", {}},
                         static_cast<unsigned>(std::max(0, trait_uses_left(actor))),
                         static_cast<unsigned>(d.trait_uses), 0u});
    if (d.giant != GiantBoon::none)
        pools.push_back({"giant_ancestry", {std::string(giant_boon_label(d.giant)), {}},
                         static_cast<unsigned>(std::max(0, trait_uses_left(actor))),
                         static_cast<unsigned>(d.trait_uses), 0u});
    if (d.resourceful)
        pools.push_back({"heroic_inspiration", {"Heroic Inspiration", {}},
                         detail::has_effect(actor.effects, detail::EffectKind::heroic_inspiration)
                         ? 1u : 0u, 1u, 0u});
}

// The slot level a spell verb casts at: a "_2" verb casts a level-one spell
// from a level-two slot; otherwise the spell's own level.
unsigned slot_level_of(std::string_view verb, const detail::SpellDef &spell)
{
    return verb.ends_with("_2") ? 2u : spell.level;
}

// Pays for a leveled spell: with Magic Initiate's free cast, or from a slot of
// the level it is cast at, the turn's one slot.
void pay_for_spell(Actor &a, std::string_view verb, const detail::SpellDef &spell)
{
    if (verb.ends_with(detail::initiate_suffix))
    {
        a.initiate_casts &= ~initiate_bit(a.definition, spell.id);
        return;
    }
    --a.slots[slot_level_of(verb, spell)];
    a.spent_slot = true;
}

bool unconscious(const Actor &a)
{
    return a.life.hp == 0;
}

bool conscious(const Actor &a)
{
    return !a.life.dead && !unconscious(a);
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

// Whether an effect gives the named Lesser Restoration condition.
bool gives_condition(detail::EffectKind kind, std::string_view condition)
{
    if (condition == "blinded")
        return kind == detail::EffectKind::blindness || kind == detail::EffectKind::dazzled;
    if (condition == "paralyzed")
        return kind == detail::EffectKind::hold_person;
    return kind == detail::EffectKind::poisoned;
}

std::string condition_name(std::string_view condition)
{
    return condition == "blinded" ? "Blinded" : condition == "paralyzed" ? "Paralyzed" : "Poisoned";
}

bool has_condition(const Actor &a, std::string_view condition)
{
    return std::any_of(a.effects.active.begin(), a.effects.active.end(), [&](const auto & e)
    {
        return gives_condition(e.kind, condition);
    });
}

// Whether Lesser Restoration has a condition to end on the creature.
bool restorable(const Actor &a)
{
    return std::any_of(detail::restoration_conditions.begin(), detail::restoration_conditions.end(),
                       [&](std::string_view condition)
    {
        return has_condition(a, condition);
    });
}

bool can_recover(const Actor &actor, const ArcaneAllocation &choice)
{
    const auto &d = actor.definition;
    // Signed comparisons: an actor holding more slots than its definition grants
    // (the room Font of Magic allows) has a negative spent count, which must
    // refuse the choice rather than wrap to a huge unsigned room.
    return conscious(actor) && d.arcane && actor.arcane > 0 &&
           std::cmp_less_equal(choice.first + 2 * choice.second, (d.level + 1) / 2) &&
           std::cmp_less_equal(choice.first, d.slots[1] - actor.slots[1]) &&
           std::cmp_less_equal(choice.second, d.slots[2] - actor.slots[2]);
}

int movement_left(const Actor &a)
{
    if (!conscious(a) || a.aim_used)
        return 0;
    // A resisted Stunning Strike halves the Speed.
    const int staggered = detail::has_effect(a.effects, detail::EffectKind::staggered)
                          ? a.definition.speed / 2 : 0;
    const int penalty =
        std::min(a.definition.speed, detail::speed_penalty(a.effects) + staggered);
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
int maximum_hit_points(CharacterClass klass, int die, bool dwarf, std::span<const int> modifiers,
                       bool draconic)
{
    if (modifiers.empty() || modifiers.size() > detail::level_cap(klass) ||
            std::any_of(modifiers.begin(), modifiers.end(),
                        [](int n)
{
    return n < -4 || n > 5;
}))
    throw std::runtime_error("Invalid HP advancement history");
    int hp = die + modifiers.front() + (dwarf ? static_cast<int>(modifiers.size()) : 0) +
             (draconic && modifiers.size() >= 3 ? static_cast<int>(modifiers.size()) : 0);
    for (std::size_t i = 1; i < modifiers.size(); ++i)
    {
        const int increase = modifiers[i] - modifiers[i - 1];
        // Constitution rises only with a feat level's ability points.
        if (increase < 0 || increase > 1 ||
                (increase && !detail::gains_feat(klass, static_cast<unsigned>(i + 1))))
            throw std::runtime_error("Invalid Constitution advancement history");
        // SRD p. 23: gain HP first, then apply a new modifier per attained level.
        hp += std::max(1, die / 2 + 1 + modifiers[i - 1]) + increase * static_cast<int>(i + 1);
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
    std::string magic, class_label, race;
    AbilityArray<int> scores{};
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
    in >> std::quoted(class_label) >> std::quoted(race);
    for (auto &score : scores)
        in >> score;
    if (!in || level < 1 || level > 20 || features > 63 ||
            std::any_of(scores.begin(), scores.end(),
                        [](int n)
{
    return n < 3 || n > 20;
}))
    throw std::runtime_error("Invalid character profile");
    const auto character_class = detail::class_from_label(class_label);
    if (!character_class)
        throw std::runtime_error("Unknown class");
    const CharacterClass klass = *character_class;
    if (level > detail::level_cap(klass))
        throw std::runtime_error("Invalid character profile");
    const auto races = character_rules()->choices(CreationField::race);
    if (std::none_of(races.begin(), races.end(),
                     [&](const auto & r)
{
    return r.label == race;
}))
    throw std::runtime_error("Unknown species");
    const int str = ability_modifier(scores[Ability::strength]),
              dex = ability_modifier(scores[Ability::dexterity]),
              con = ability_modifier(scores[Ability::constitution]);
    // The Proficiency Bonus: +2 through level 4, +3 from 5, +4 from 9.
    const int proficiency = 2 + (static_cast<int>(level) - 1) / 4;
    std::vector<int> hp_modifiers(level, con);
    for (auto &modifier : hp_modifiers)
        in >> modifier;
    in >> count;
    if (!in || count > 3 || hp_modifiers.back() != con)
        throw std::runtime_error("Invalid character HP history or equipment count");
    const int die = klass == CharacterClass::barbarian ? 12
                    : (klass == CharacterClass::fighter || klass == CharacterClass::paladin ||
                       klass == CharacterClass::ranger) ? 10
                    : (klass == CharacterClass::wizard || klass == CharacterClass::sorcerer) ? 6
                    : 8;
    Definition d;
    d.hp = maximum_hit_points(klass, die, race == "Dwarf", hp_modifiers,
                              klass == CharacterClass::sorcerer);
    d.hit_die = die;
    d.constitution = con;
    d.dwarf = race == "Dwarf";
    d.gnomish_cunning = race == "Gnome";
    d.relentless = race == "Orc";
    d.resourceful = race == "Human";
    d.lucky = d.brave = race == "Halfling";
    d.nimble = race == "Halfling";
    d.naturally_stealthy = race == "Halfling";
    // Gnomes and Halflings are Small.
    if (race == "Gnome" || race == "Halfling")
        d.size = 1;
    // Elves do not sleep, so Sleep cannot touch them.
    d.sleepless = race == "Elf";
    d.rages = klass == CharacterClass::barbarian
              ? (level >= 12 ? 5 : level >= 6 ? 4 : level >= 3 ? 3 : 2) : 0;
    d.wild_shapes = klass == CharacterClass::druid && level >= 2 ? 2 : 0;
    d.lands_aid = klass == CharacterClass::druid && level >= 3;
    d.rage_damage = klass == CharacterClass::barbarian ? (level >= 9 ? 3 : 2) : 0;
    d.strength = str;
    d.danger_sense = d.reckless = klass == CharacterClass::barbarian && level >= 2;
    d.frenzy = klass == CharacterClass::barbarian && level >= 3;
    d.focus = klass == CharacterClass::monk && level >= 2 ? static_cast<int>(level) : 0;
    d.metabolism = klass == CharacterClass::monk && level >= 2 ? 1 : 0;
    d.innate_sorcery = klass == CharacterClass::sorcerer ? 2 : 0;
    d.sorcery_points =
        klass == CharacterClass::sorcerer && level >= 2 ? static_cast<int>(level) : 0;
    d.pact_magic = klass == CharacterClass::warlock;
    d.magical_cunning = klass == CharacterClass::warlock && level >= 2 ? 1 : 0;
    d.bardic_inspiration =
        klass == CharacterClass::bard ? std::max(1, ability_modifier(scores[Ability::charisma]))
        : 0;
    d.cutting_words = klass == CharacterClass::bard && level >= 3;
    d.dark_ones_blessing =
        klass == CharacterClass::warlock && level >= 3
        ? std::max(1, ability_modifier(scores[Ability::charisma]) + static_cast<int>(level))
        : 0;
    d.deflect = d.open_hand = klass == CharacterClass::monk && level >= 3;
    d.focus_dc = 8 + proficiency + ability_modifier(scores[Ability::wisdom]);
    d.dexterity = dex;
    d.rushes = race == "Orc" ? proficiency : 0;
    for (const auto ability : all_abilities)
        d.saves[ability_index(ability)] =
            ability_modifier(scores[ability]) +
            (detail::save_proficient(klass, level, ability) ? proficiency : 0);
    d.alert = (features & 32) != 0;
    d.ac = 10 + dex;
    // Jack of All Trades adds half the Proficiency Bonus to Initiative, as
    // SRD-DECISIONS keeps it, unless Alert already adds the whole bonus.
    d.initiative = dex + (d.alert ? proficiency
                          : klass == CharacterClass::bard && level >= 2 ? proficiency / 2 : 0);
    d.speed = race == "Goliath" ? 35 : 30;
    d.level = level;
    d.melee_bonus = proficiency + str;
    d.melee = {0, 0, std::max(0, 1 + str)};
    d.champion = klass == CharacterClass::fighter && level >= 3;
    d.critical_from = !d.champion ? 20 : level >= 15 ? 18 : 19;
    d.extra_attacks = (klass == CharacterClass::fighter || klass == CharacterClass::barbarian ||
                       klass == CharacterClass::monk || klass == CharacterClass::paladin) &&
                      level >= 5 ? 1 : 0;
    // Two Extra Attacks (Fighter 11): three attacks with each Attack action.
    if (klass == CharacterClass::fighter && level >= 11)
        d.extra_attacks = 2;
    d.studied_attacks = klass == CharacterClass::fighter && level >= 13;
    const bool barbarian = klass == CharacterClass::barbarian;
    d.mindless_rage = barbarian && level >= 6;
    d.feral_instinct = d.instinctive_pounce = barbarian && level >= 7;
    d.brutal_strike = barbarian && level >= 9;
    // Relentless Rage's DC climbs 10, 15, 20, 25, 30; past that no save passes.
    d.relentless_rage = barbarian && level >= 11 ? 5 : 0;
    d.improved_brutal = barbarian && level >= 13;
    d.intimidating_presence = barbarian && level >= 14 ? 1 : 0;
    d.persistent_rage = barbarian && level >= 15 ? 1 : 0;
    d.retaliation = barbarian && level >= 10;
    const bool rogue = klass == CharacterClass::rogue;
    d.cunning_strike = d.uncanny_dodge = rogue && level >= 5;
    d.evasion = (rogue || klass == CharacterClass::monk) && level >= 7;
    d.martial_die = klass != CharacterClass::monk ? 6 : level >= 11 ? 10 : level >= 5 ? 8 : 6;
    d.wisdom = ability_modifier(scores[Ability::wisdom]);
    d.stunning_strike = klass == CharacterClass::monk && level >= 5;
    d.empowered_strikes = klass == CharacterClass::monk && level >= 6;
    d.heightened_focus = d.self_restoration = klass == CharacterClass::monk && level >= 10;
    d.fleet_step = klass == CharacterClass::monk && level >= 11;
    d.deflect_energy = klass == CharacterClass::monk && level >= 13;
    d.disciplined_survivor = klass == CharacterClass::monk && level >= 14;
    d.perfect_focus = klass == CharacterClass::monk && level >= 15;
    d.aura_of_protection = klass == CharacterClass::paladin && level >= 6
                           ? std::max(1, ability_modifier(scores[Ability::charisma])) : 0;
    d.aura_of_devotion = klass == CharacterClass::paladin && level >= 7;
    d.wholeness = klass == CharacterClass::monk && level >= 6
                  ? std::max(1, ability_modifier(scores[Ability::wisdom])) : 0;
    d.supreme_sneak = rogue && level >= 9;
    d.improved_cunning = rogue && level >= 11;
    d.devious_strikes = rogue && level >= 14;
    d.tactical_shift = klass == CharacterClass::fighter && level >= 5;
    d.tactical_master = klass == CharacterClass::fighter && level >= 9;
    d.heroic_warrior = klass == CharacterClass::fighter && level >= 10;
    d.indomitable = klass != CharacterClass::fighter ? 0 : level >= 13 ? 2 : level >= 9 ? 1 : 0;
    d.arcane = klass == CharacterClass::wizard ? 1 : 0;
    d.lay_on_hands = klass == CharacterClass::paladin ? 5 * level : 0;
    d.channel_divinity = (klass == CharacterClass::paladin && level >= 3) ||
                         (klass == CharacterClass::cleric && level >= 2) ? 2
                         : 0;
    d.sacred_weapon = klass == CharacterClass::paladin && level >= 3;
    d.divine_spark = klass == CharacterClass::cleric && level >= 2;
    d.life_domain = klass == CharacterClass::cleric && level >= 3;
    d.evoker = klass == CharacterClass::wizard && level >= 3;
    d.free_smite = klass == CharacterClass::paladin && level >= 2 ? 1 : 0;
    d.favored_enemy = klass == CharacterClass::ranger ? 2 : 0;
    d.medicine = ability_modifier(scores[Ability::wisdom]);
    d.tactical_mind = klass == CharacterClass::fighter && level >= 2;
    d.cunning = klass == CharacterClass::rogue && level >= 2;
    d.sneak_level = klass == CharacterClass::rogue ? level : 0;
    d.great_weapon_fighting = (features & 8) != 0;
    d.two_weapon_fighting = (features & 16) != 0;
    d.surges = klass == CharacterClass::fighter && level >= 2 ? 1 : 0;
    // Second Wind: 2 uses, 3 from level 4, 4 from level 10.
    d.winds = klass == CharacterClass::fighter ? (level >= 10 ? 4 : level >= 4 ? 3 : 2) : 0;
    d.slots = spell_slots(klass, level);
    const Ability casting_ability =
        (klass == CharacterClass::cleric || klass == CharacterClass::ranger ||
         klass == CharacterClass::druid) ? Ability::wisdom
        : (klass == CharacterClass::warlock || klass == CharacterClass::sorcerer ||
           klass == CharacterClass::paladin || klass == CharacterClass::bard) ? Ability::charisma
        : Ability::intelligence;
    d.casting = proficiency + ability_modifier(scores[casting_ability]);
    if ((features & 1) && klass != CharacterClass::fighter &&
            klass != CharacterClass::paladin && klass != CharacterClass::ranger)
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
    // A species' creation choices: size, a Wood Elf's speed and a Tiefling's
    // or Dragonborn's resistance.
    const auto chose = [&](std::string_view id)
    {
        return std::any_of(grants.begin(), grants.end(), [&](const auto & g)
        {
            return g.id == id;
        });
    };
    if (chose("size:small"))
        d.size = 1;
    if (chose("lineage:wood_elf"))
        d.speed = 35;
    using detail::DamageType;
    for (const auto &[choice, type] : std::initializer_list<std::pair<std::string_view, DamageType>>
{
    {"legacy:abyssal", DamageType::poison}, {"legacy:chthonic", DamageType::necrotic},
        {"legacy:infernal", DamageType::fire}, {"dragon:black", DamageType::acid},
        {"dragon:copper", DamageType::acid}, {"dragon:blue", DamageType::lightning},
        {"dragon:bronze", DamageType::lightning}, {"dragon:brass", DamageType::fire},
        {"dragon:gold", DamageType::fire}, {"dragon:red", DamageType::fire},
        {"dragon:green", DamageType::poison}, {"dragon:silver", DamageType::cold},
        {"dragon:white", DamageType::cold}
    })
    if (chose(choice))
    {
        d.affinities.push_back({detail::AffinityKind::resistance, type,
                                "species/" + std::string(choice)});
        // Breath Weapon: as many uses as the Proficiency Bonus.
        if (choice.starts_with("dragon:"))
        {
            d.breath = type;
            d.trait_uses = proficiency;
        }
    }
    // Giant Ancestry: as many uses as the Proficiency Bonus.
    for (const auto &[choice, boon] : std::initializer_list<std::pair<std::string_view, GiantBoon>>
{
    {"giant:cloud", GiantBoon::cloud}, {"giant:fire", GiantBoon::fire},
        {"giant:frost", GiantBoon::frost}, {"giant:hill", GiantBoon::hill},
        {"giant:stone", GiantBoon::stone}, {"giant:storm", GiantBoon::storm}
    })
    if (chose(choice))
    {
        d.giant = boon;
        d.trait_uses = proficiency;
    }
    // Magic Initiate's and a lineage's spells: their own ability, and a free
    // cast of the leveled one each Long Rest.
    auto gifts = detail::magic_initiate_spells(grants);
    for (auto &lineage : detail::lineage_spells(grants, level))
        gifts.push_back(std::move(lineage));
    for (const auto &feat : gifts)
    {
        for (const auto &spell : feat.spells)
            d.initiate_casting.emplace(spell, proficiency + ability_modifier(scores[feat.ability]));
        if (!feat.free_spell.empty())
            d.initiate_spells.push_back(feat.free_spell);
        if (feat.feat.starts_with("species:"))
            d.lineage_spells.insert(feat.free_spell);
    }
    // A stored spell is the class's at this level, or one Magic Initiate teaches.
    const auto allowed = allowed_spells(klass, level);
    const auto storable = [&](const std::string & id)
    {
        return detail::knows_spell(allowed, id) || d.initiate_casting.contains(id);
    };
    if (!std::all_of(stored_spells.begin(), stored_spells.end(), storable))
        throw std::runtime_error("Invalid prepared spells or feat prerequisites");
    const auto invoked = [&](std::string_view id)
    {
        return std::any_of(grants.begin(), grants.end(), [&](const auto & g)
        {
            return g.id == "invocation:" + std::string(id);
        });
    };
    for (const auto &grant : grants)
        for (auto &[prefix, cantrips] :
                std::initializer_list<std::pair<std::string_view, std::set<std::string, std::less<>> *>>
    {
        {"invocation:agonizing_blast:", &d.agonizing},
        {"invocation:eldritch_spear:", &d.eldritch_spear},
        {"invocation:repelling_blast:", &d.repelling}
    })
    if (grant.id.starts_with(prefix))
        cantrips->insert(grant.id.substr(prefix.size()));
    d.armor_of_shadows = invoked("armor_of_shadows");
    d.devils_sight = invoked("devils_sight");
    d.eldritch_mind = invoked("eldritch_mind");
    d.fiendish_vigor = invoked("fiendish_vigor");
    d.pact_of_the_blade = invoked("pact_of_the_blade");
    // Lessons of the First Ones: an Origin feat.
    d.savage |= invoked("lessons_savage_attacker");
    if (invoked("lessons_alert"))
    {
        d.alert = true;
        d.initiative = dex + proficiency;
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
                                          pact ? ability_modifier(scores[Ability::charisma]) : -5);
            const int bonus = (trained(klass, grants, key) || pact ? proficiency : 0) + modifier;
            if ((item->dice || item->fixed_damage) && !item->ranged)
            {
                d.melee_ability = modifier;
                d.melee_bonus = bonus;
                d.melee = {item->dice, item->sides,
                           item->fixed_damage ? item->fixed_damage : modifier
                          };
                d.melee_type = item->type;
                d.reach = melee_reach;
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
            if (scores[Ability::strength] < item->strength)
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
    const auto training = detail::training_profile(grants, klass, race,
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
    d.perception = std::find_if(training.skills.begin(), training.skills.end(),
                                [](const auto & skill)
    {
        return skill.id == "perception";
    })
    ->bonus;
    d.stealth = std::find_if(training.skills.begin(), training.skills.end(),
                             [](const auto & skill)
    {
        return skill.id == "stealth";
    })
    ->bonus;
    if (klass == CharacterClass::rogue && level >= 7)
        for (const auto &skill : training.skills)
            if (skill.proficient)
                d.reliable.insert(skill.id);
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
    if (!same_spells(detail::casting_ids(access), stored_spells))
        throw std::runtime_error("Character casting access disagrees with spell grants");
    const auto features_only = detail::without_spell_grants(detail::without_training(grants));
    const auto effects = detail::validate_grants(features_only, klass,
                         detail::race_id(race), background, level);
    if (effects.feats != features)
        throw std::runtime_error("Character effects disagree with acquired grants");
    // Each level's Constitution modifier: the score before any feat, plus
    // what the Ability Score Improvements gained by that level gave it.
    for (unsigned i = 0; i < level; ++i)
    {
        int score = scores[Ability::constitution] - effects.abilities[Ability::constitution];
        for (const auto &g : grants)
            if (g.id == "feat:ability_score_improvement" && g.level <= i + 1)
                if (const auto raised = g.choices.find("constitution"); raised != g.choices.end())
                    score += raised->second == "2" ? 2 : 1;
        if (hp_modifiers[i] != ability_modifier(score))
            throw std::runtime_error("HP history disagrees with acquired ability choices");
    }
    if (race == "Dwarf")
        d.affinities.push_back({detail::AffinityKind::resistance, detail::DamageType::poison,
                                "species:dwarf/trait:dwarven_resilience"});
    if (hands > 2)
        throw std::runtime_error(
            "Not enough free hands. Unequip the shield or two-handed weapon first.");
    if (!armor && klass == CharacterClass::barbarian)
        d.ac = std::max(d.ac, 10 + dex + con);
    // Draconic Resilience: dragon-like scales.
    if (!armor && klass == CharacterClass::sorcerer && level >= 3)
        d.ac = std::max(d.ac, 10 + dex + ability_modifier(scores[Ability::charisma]));
    if (!armor && !shield && klass == CharacterClass::monk)
        d.ac = std::max(d.ac, 10 + dex + ability_modifier(scores[Ability::wisdom]));
    // Martial Arts: the better of Strength and Dexterity for attack and damage
    // rolls, and the Martial Arts die (a d6 through level four) when larger.
    d.martial_arts = klass == CharacterClass::monk && !armor && !shield && monk_weapons;
    // Unarmored Movement: 10 feet more without armor or a Shield, 15 from level
    // 6, 20 from 10 and 25 from 14.
    if (klass == CharacterClass::monk && level >= 2 && !armor && !shield)
        d.speed += level >= 14 ? 25 : level >= 10 ? 20 : level >= 6 ? 15 : 10;
    // Fast Movement: 10 feet more without Heavy armor.
    if (klass == CharacterClass::barbarian && level >= 5 && !d.heavy_armor)
        d.speed += 10;
    if (d.martial_arts)
    {
        d.martial_modifier = std::max(str, dex);
        if (!weapon)
        {
            d.melee = {1, d.martial_die, d.martial_modifier};
            d.melee_bonus = proficiency + d.martial_modifier;
        }
        else
        {
            d.melee_bonus += d.martial_modifier - d.melee_ability;
            d.melee.bonus = d.martial_modifier;
            d.melee.sides = std::max(d.melee.sides, d.martial_die);
        }
        d.melee_ability = d.martial_modifier;
    }
    // Empowered Strikes: Unarmed Strikes deal Force damage, which almost
    // nothing resists, so it is always chosen.
    if (d.empowered_strikes && !weapon)
        d.melee_type = detail::DamageType::force;
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
constexpr std::string_view vitals_magic = "SRD13";

void restore_vitals(Actor &a, const VitalState &state)
{
    a.life.hp = state.hit_points;
    a.life.dead = state.dead;
    // Empty resources describe a fresh character: every pool and Hit Die is
    // full. Callers supply the full Second Wind and spell-slot values.
    a.hit_dice = a.definition.hit_die ? a.definition.level : 0;
    a.rushes = a.definition.rushes;
    a.surges = surge_capacity(a.definition);
    a.arcane = arcane_capacity(a.definition);
    a.lay_on_hands = lay_capacity(a.definition);
    a.free_casts = free_cast_capacity(a.definition);
    a.channel_divinity = channel_capacity(a.definition);
    a.initiate_casts = initiate_capacity(a.definition);
    if (!state.resources.empty())
    {
        std::istringstream in(state.resources);
        std::string magic;
        in >> magic >> a.winds;
        for (unsigned level = 1; level <= highest_slot_level; ++level)
            in >> a.slots[level];
        in >> a.life.successes >> a.life.failures >> a.life.stable >>
           a.hit_dice >> a.life.recovery.death_save_in_ms >> a.life.recovery.stable_recovery_in_ms >>
           a.life.temporary_hp.amount >> std::quoted(a.life.temporary_hp.source_id) >> a.rushes >>
           a.surges >> a.arcane >> a.lay_on_hands >> a.free_casts >> a.channel_divinity >>
           a.initiate_casts;
        if (!in || magic != vitals_magic)
            throw std::runtime_error("Invalid character resource state");
        detail::decode_stable_recovery(a.life.recovery);
        a.effects = detail::read_effects(in);
        in >> std::ws;
        if (!in.eof())
            throw std::runtime_error("Trailing character resource state");
    }
    const auto &d = a.definition;
    if (!slots_in_range(a.slots, d))
        throw std::runtime_error("Invalid character vitals");
    if (a.life.hp < 0 || a.life.hp > max_hp(a) || (a.life.dead && a.life.hp != 0) || a.winds < 0 || a.winds > d.winds ||
            a.arcane < 0 || a.arcane > arcane_capacity(d) || a.surges < 0 ||
            a.surges > surge_capacity(d) || a.rushes < 0 ||
            a.rushes > d.rushes || a.hit_dice < 0 || a.hit_dice > (d.hit_die ? d.level : 0) ||
            a.life.successes < 0 || a.life.successes > 3 || a.life.failures < 0 || a.life.failures > 4 ||
            a.lay_on_hands < 0 || a.lay_on_hands > lay_capacity(d) || a.free_casts < 0 ||
            a.free_casts > free_cast_capacity(d) || a.channel_divinity < 0 ||
            a.channel_divinity > channel_capacity(d) || a.initiate_casts < 0 ||
            (a.initiate_casts & ~initiate_capacity(d)))
        throw std::runtime_error("Invalid character vitals");
    detail::validate_recovery(a.life);
    detail::validate_temporary_hp(a.life.temporary_hp);
    if (a.life.recovery.stable_recovery_due && !detail::healing_blocked(a.effects))
        throw std::runtime_error("Earned recovery requires active healing prevention");
}

// An actor for a rules call outside combat: full Second Winds and slots from
// its definition, then its saved vitals. One place, so a new resource cannot be
// missed in one of the many callers.
Actor actor_from(const Definition &definition, const VitalState &state)
{
    Actor actor;
    actor.definition = definition;
    actor.winds = definition.winds;
    actor.slots = definition.slots;
    restore_vitals(actor, state);
    return actor;
}

VitalState vitals(const Actor &a)
{
    std::ostringstream out;
    out << vitals_magic << ' ' << a.winds << ' ';
    for (unsigned level = 1; level <= highest_slot_level; ++level)
        out << a.slots[level] << ' ';
    out << a.life.successes << ' ' << a.life.failures << ' ' << a.life.stable << ' ' << a.hit_dice << ' '
        << a.life.recovery.death_save_in_ms << ' ' << detail::encode_stable_recovery(a.life.recovery) << ' '
        << a.life.temporary_hp.amount << ' ' << std::quoted(a.life.temporary_hp.source_id) << ' '
        << a.rushes << ' ' << a.surges << ' ' << a.arcane << ' ' << a.lay_on_hands << ' '
        << a.free_casts << ' ' << a.channel_divinity << ' ' << a.initiate_casts << ' ';
    // A creature hides from one fight's enemies; it is not hidden after it.
    auto effects = a.effects;
    std::erase_if(effects.active, [](const auto & e)
    {
        return e.kind == detail::EffectKind::hidden;
    });
    detail::write_effects(out, effects);
    std::string description;
    for (unsigned level = 1; level <= highest_slot_level; ++level)
        if (a.definition.slots[level])
            description += (description.empty() ? "" : "\n") + std::string(slot_labels[level]) +
                           ": " + std::to_string(a.slots[level]) + " / " +
                           std::to_string(a.definition.slots[level]);
    if (a.definition.winds)
        description = "Second Wind uses: " + std::to_string(a.winds) + " / " +
                      std::to_string(a.definition.winds);
    if (a.definition.arcane)
        description += (description.empty() ? "" : "\n") + std::string("Arcane Recovery uses: ") +
                       std::to_string(a.arcane) + " / 1";
    if (a.definition.surges && a.surges < a.definition.surges)
        description += (description.empty() ? "" : "\n") + std::string("Action Surge uses: ") +
                       std::to_string(a.surges) + " / " + std::to_string(a.definition.surges);
    if (a.life.hp == 0)
        description += (description.empty() ? "" : "\n") +
                       std::string(a.life.dead     ? "Dead"
                                   : a.life.stable ? "Stable, unconscious"
                                   : "Unconscious; death saves ") +
                       (!a.life.dead && !a.life.stable ? std::to_string(a.life.successes) + " successes, " +
                        std::to_string(a.life.failures) + " failures"
                        : "");
    if (detail::healing_blocked(a.effects))
        description += "\nChill Touch: cannot regain HP.";
    if (detail::has_effect(a.effects, detail::EffectKind::shocking_grasp))
        description += "\nShocking Grasp: cannot make Opportunity Attacks.";
    if (detail::frosted(a.effects))
        description += "\nRay of Frost: Speed reduced by 10 feet.";
    if (detail::slowed(a.effects))
        description += "\nSlow: Speed reduced by 10 feet.";
    if (detail::has_effect(a.effects, detail::EffectKind::frosts_chill))
        description += "\nFrost's Chill: Speed reduced by 10 feet.";
    if (detail::has_effect(a.effects, detail::EffectKind::hamstrung))
        description += "\nHamstring Blow: Speed reduced by 15 feet.";
    if (detail::blinded(a.effects))
        description += "\nBlinded";
    if (a.effects.prone)
        description += "\nProne";
    return {a.life.hp, a.life.dead, out.str(), description};
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

// A character's statistics after taking off its shield: the same equipment
// without it.
Definition without_shield(std::string_view profile, const Definition &d)
{
    std::vector<std::string> keys;
    for (const auto &key : d.equipment_keys)
        if (key != "shield")
            keys.push_back(key);
    return character_definition(profile, std::span<const std::string>(keys));
}

bool wears_shield(const Definition &d)
{
    return std::find(d.equipment_keys.begin(), d.equipment_keys.end(), "shield") !=
           d.equipment_keys.end();
}

// Tactical Master's three masteries, by the attack verbs' suffixes, with
// their commands' labels.
struct TacticalOption
{
    std::string_view suffix;
    detail::Mastery kind;
    std::string_view melee_label, ranged_label;
};
constexpr std::array<TacticalOption, 3> tactical_masteries{
    {   {"push", detail::Mastery::push, "Melee attack (Push)", "Ranged attack (Push)"},
        {"sap", detail::Mastery::sap, "Melee attack (Sap)", "Ranged attack (Sap)"},
        {"slow", detail::Mastery::slow, "Melee attack (Slow)", "Ranged attack (Slow)"}
    }};

// Brutal Strike's blows (Barbarian 9; Staggering and Sundering from 13).
enum class BrutalBlow : unsigned
{
    forceful,
    hamstring,
    staggering,
    sundering
};

// Brutal Strike's blows by verb, with their commands' labels.
constexpr std::array<std::tuple<std::string_view, std::string_view, BrutalBlow>, 4>
brutal_blows{{
        {"brutal_forceful", "Brutal Strike (Forceful Blow)", BrutalBlow::forceful},
        {"brutal_hamstring", "Brutal Strike (Hamstring Blow)", BrutalBlow::hamstring},
        {"brutal_staggering", "Brutal Strike (Staggering Blow)", BrutalBlow::staggering},
        {"brutal_sundering", "Brutal Strike (Sundering Blow)", BrutalBlow::sundering}
    }};

// Cunning Strike's effects (Rogue 5).
enum class CunningStrike : unsigned
{
    trip,
    withdraw,
    stealth,   // Supreme Sneak's Stealth Attack (Thief 9)
    daze,      // Devious Strikes (Rogue 14)
    knock_out,
    obscure
};

// Cunning Strike's options by verb suffix, with their names and the Sneak
// Attack dice they cost; the Poison option waits for a Poisoner's Kit (#233).
// A command's verb is "melee_" or "ranged_" and one suffix, or two joined by
// "+" with Improved Cunning Strike (Rogue 11).
struct CunningOption
{
    std::string_view suffix;
    CunningStrike kind;
    std::string_view name;
    int cost;
};
constexpr std::array<CunningOption, 6> cunning_strikes{
    {   {"trip", CunningStrike::trip, "Trip", 1},
        {"withdraw", CunningStrike::withdraw, "Withdraw", 1},
        {"stealth", CunningStrike::stealth, "Stealth Attack", 1},
        {"daze", CunningStrike::daze, "Daze", 2},
        {"knock_out", CunningStrike::knock_out, "Knock Out", 6},
        {"obscure", CunningStrike::obscure, "Obscure", 3}
    }};

const CunningOption &cunning_option(CunningStrike kind)
{
    return cunning_strikes.at(static_cast<std::size_t>(kind));
}

// An attack the Attack action makes, which Extra Attack repeats; Reckless
// Attack and the pact weapon's damage types arrive here as "melee".
bool attack_action(std::string_view verb)
{
    return verb == "melee" || verb == "ranged" || verb == "torch" || verb == "shoot" ||
           verb.starts_with("throw");
}

bool turns_to_attack(std::string_view verb)
{
    // A caster turns toward a foe, not toward an ally being healed, so every
    // spell except the ally-targeted ones counts alongside weapon attacks.
    if (verb.starts_with("light_") || verb.starts_with("nick_") || verb.starts_with("throw") ||
            verb == "melee" || verb == "ranged" || verb == "torch" || verb == "shoot")
        return true;
    const auto *spell = detail::find_spell(verb);
    return spell && spell->target != detail::SpellTarget::wounded_ally;
}

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
    int bursts{}; // how many extra d8s an 8 may add (Sorcerous Burst)
    std::string_view spell{}; // its attack bonus follows Magic Initiate's ability
};

// What attack() settled before apply_hit reports and applies it. Four ints and
// three flags in a row were easy to pass out of order, so callers name each one.
struct ResolvedAttack
{
    int natural{}; // the d20 as rolled
    int bonus{};   // everything added to the d20
    int mode{};    // below zero with Disadvantage, above zero with Advantage
    int damage{};  // after every reduction; zero on a miss
    detail::DamageType type{};
    AttackRange range{AttackRange::melee};
    bool spell{};
    bool savage{}; // Savage Attacker rerolled the damage
};

class Session final : public CombatSession
{
  public:
    Session(std::shared_ptr<const Content> content, Encounter encounter, std::uint64_t seed,
            bool restoring = false)
        : content_(std::move(content)), board_(std::move(encounter.battlefield)), rng_(seed),
          scope_(encounter.scope), morale_(encounter.morale)
    {
        if (!scope_)
            throw std::runtime_error("Invalid encounter scope");
        if (morale_ > 100 || std::any_of(encounter.participants.begin(),
                                         encounter.participants.end(), [](const auto & p)
    {
        return p.morale > 100 || p.intelligence > 30;
    }))
        throw std::runtime_error("Invalid encounter morale");
        detail::validate_battlefield(board_);
        // A restored fight may have one creature left on the field, the rest
        // having run off it.
        if (encounter.participants.size() < (restoring ? 1u : 2u) ||
                encounter.participants.size() > 64)
            throw std::runtime_error("Invalid encounter size");
        std::set<EntityId> ids;
        std::set<Cell> cells;
        std::set<Side> sides;
        for (auto &p : encounter.participants)
        {
            if (!p.id || !ids.insert(p.id).second || (!cells.insert(p.cell).second && !restoring) ||
                    board_.at(p.cell) == Terrain::obstacle || p.side > Side::opposition ||
                    p.name.empty() || p.name.size() > 160 ||
                    (p.character_profile.empty() && !content_->definitions.contains(p.definition)))
                throw std::runtime_error("Invalid participant or unsupported rules definition: " +
                                         p.definition);
            sides.insert(p.side);
            const auto d = p.character_profile.empty() ? content_->definitions.at(p.definition)
                           : character_definition(p.character_profile);
            Actor a;
            a.definition = d;
            a.source = std::move(p);
            a.life.hp = d.hp;
            a.winds = d.winds;
            a.slots = d.slots;
            a.hit_dice = d.hit_die ? d.level : 0;
            a.rushes = d.rushes;
            a.surges = surge_capacity(d);
            a.arcane = arcane_capacity(d);
            a.lay_on_hands = lay_capacity(d);
            a.free_casts = free_cast_capacity(d);
            a.channel_divinity = channel_capacity(d);
            a.initiate_casts = initiate_capacity(d);
            a.facing_left = a.source.facing_left;
            if (a.source.state)
                restore_vitals(a, *a.source.state);
            // A Druid still in a Beast form from earlier fights in that form.
            refresh_form(a);
            a.initiative =
                detail::d20({d.champion || d.feral_instinct,
                             d.str_dex_disadvantage || a.source.surprised, d.lucky},
                            rng_) +
                d.initiative;
            a.movement = a.form ? a.form->speed : d.speed;
            actors_.push_back(std::move(a));
        }
        if (sides.size() != 2 && !restoring)
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
            // Thrown gear is counted once, from what each character carries in.
            for (auto &a : actors_)
                for (const auto &item : a.source.inventory)
                    if (const auto *gear = detail::thrown_gear(item.definition))
                        a.thrown_gear_left[static_cast<std::size_t>(gear->effect)] += item.quantity;
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
                    if (a.source.side == Side::party &&
                            (def(a).alert || metabolism_ready(a) || persistent_ready(a)) &&
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
    // The damage type a Pact of the Blade attack being resolved deals instead
    // of its weapon's; set only during that attack.
    std::optional<detail::DamageType> pact_damage_;
    // Tactical Master (Fighter 9): the mastery the attacker uses for one attack
    // instead of its weapon's own.
    struct TacticalMastery
    {
        EntityId actor{};
        detail::Mastery kind{};
    };
    std::optional<TacticalMastery> tactical_mastery_;
    // Brutal Strike (Barbarian 9): the attack it is chosen for, and its effect.
    struct BrutalStrike
    {
        EntityId actor{};
        BrutalBlow blow{};
    };
    std::optional<BrutalStrike> brutal_;
    // Cunning Strike (Rogue 5): the effect chosen for one attack, paid with a
    // Sneak Attack die when the attack deals Sneak Attack damage.
    struct PendingCunning
    {
        EntityId actor{};
        CunningStrike kind{};
        std::optional<CunningStrike> second; // Improved Cunning Strike

        [[nodiscard]] bool has(CunningStrike wanted) const
        {
            return kind == wanted || second == wanted;
        }

        [[nodiscard]] int cost() const
        {
            return cunning_option(kind).cost + (second ? cunning_option(*second).cost : 0);
        }
    };
    std::optional<PendingCunning> cunning_;
    // Stunning Strike (Monk 5): the attack it is tried with, if it hits.
    EntityId stunning_{};
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
        // Grease outlasts a caster who runs off the field, so a zone keeps the
        // DC its creatures save against rather than looking the caster up.
        int save_dc{};
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
        rebuke,   // an attack damaged a Warlock: Hellish Rebuke
        boon,     // a Goliath's attack roll hit and dealt damage: Fire's Burn and so on
        endure,   // an attack is about to damage a Goliath: Stone's Endurance
        thunder,  // an attack damaged a Goliath: Storm's Thunder
        retaliate // a creature within 5 feet damaged a Berserker: Retaliation
    };
    struct ReactionQuestion
    {
        EntityId target{};
        Asked asked{};
        bool deflectable{}; // the hit deals Bludgeoning, Piercing or Slashing damage
        bool critical{};    // a hit Shield cannot turn aside
    };
    // A creature's answer during the command being replayed: "shield",
    // "deflect", "redirect", "rebuke", "giant_boon", "stones_endurance",
    // "storms_thunder" or "decline".
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
    // Creatures damaged during the command, each with the creature acting then
    // (the one whose turn it is, or one making an Opportunity Attack). Hellish
    // Rebuke and Storm's Thunder answer them after the attack or the command.
    struct DamageTaken
    {
        EntityId target{}, source{};
    };
    std::vector<DamageTaken> damage_taken_;
    void answer_damage();
    // True while perform() can undo the command to ask a question; damage
    // from turns ended outside it (a creature fled or fell) asks nothing.
    bool askable_{};
    [[nodiscard]] const ReactionAnswer *answer_of(EntityId target, Asked asked) const;
    [[nodiscard]] bool can_shield(const Actor &target) const;
    [[nodiscard]] bool can_deflect(const Actor &target, bool deflectable) const;
    [[nodiscard]] bool can_dodge(const Actor &target) const;
    // Uncanny Dodge (Rogue 5): the hit's damage halved, when the Rogue chose it.
    int dodged(Actor &target, int amount);
    void ask_reaction(const Actor &target, Asked asked, bool deflectable = false,
                      bool critical = false) const;
    int deflected(const Actor &attacker, Actor &target, int amount, detail::DamageType type,
                  bool ranged);
    [[nodiscard]] bool can_rebuke(const Actor &warlock, const Actor &attacker) const;
    // Giant Ancestry's Reactions and on-hit boons, each asked before use.
    [[nodiscard]] bool can_use_giant_reaction(const Actor &goliath, GiantBoon boon) const;
    void giant_boon(const Actor &attacker, Actor &target, bool critical);
    [[nodiscard]] int endured(Actor &goliath, int amount);
    void storms_thunder(const Actor &attacker, Actor &goliath);
    // Bardic Inspiration's die added to a roll that failed by `shortfall`.
    int inspiration_roll(const Actor &creature, int shortfall);
    void log_inspiration(const Actor &inspired, int die, bool success);
    // Cutting Words' die taken from an attack roll that hit, or an ability
    // check that succeeded, by `margin`.
    enum class CutRoll : unsigned
    {
        attack,
        check
    };
    int cutting_words(const Actor &roller, int margin, CutRoll roll = CutRoll::attack);
    // Cutting Words' die taken from a damage roll of `amount` that would drop
    // `target`, on the Bard's side, to 0 Hit Points (CUT-1).
    int cut_damage(const Actor &roller, const Actor &target, int amount, detail::DamageType type);
    // A Lore Bard who may use Cutting Words against the roller, or none.
    Actor *cutting_bard(const Actor &roller);
    void bless_fiends(const Actor &fallen);
    void rebuke(const Actor &attacker, Actor &warlock);
    void clouds_jaunt(Actor &goliath, Cell destination);
    void retaliate(const Actor &attacker, Actor &berserker);
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
    // Clears the rest of the mover's route when a reaction left it illegal: a
    // creature now stands on it, or a bite knocked the mover Prone and it must
    // crawl farther than its movement allows. The mover stops where it is,
    // keeping the movement it has left. True when it stopped.
    bool stop_illegal_route();
    void validate_mastery_state() const;
    void offer_mastery(const Actor &, const Actor &, int natural, AttackRange range, int damage,
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
    void use_focus_movement(Actor &a, std::string_view verb, EntityId carried = 0);
    void use_font_of_magic(Actor &a, std::string_view verb);
    void ready_metamagic(Actor &a, std::string_view choice);
    void open_hand(const Actor &monk, Actor &target, std::string_view verb);
    bool weapon_reaction(const Actor &, const Definition &) const;
    bool has_weapon_reaction(const Actor &, Cell, Cell) const;
    void validate_light() const;
    Actor thrown_actor(const Actor &, std::string_view weapon) const;
    void throw_weapon(Actor &, Actor &, unsigned item, bool light = false);
    // Whether a character carries an item it does not already wield.
    bool carries(const Actor &, std::string_view key) const;
    void torch_attack(Actor &, Actor &);
    // A ranged weapon a character carries with its ammunition while wielding no
    // ranged weapon, and has the hands for; empty when there is none.
    std::string_view carried_ranged_weapon(const Actor &) const;
    // Whether taking off its shield would free a character to draw a carried
    // two-handed bow or crossbow.
    bool shield_blocks_bow(const Actor &) const;
    // Fleeing the fight (the original's rule): a party member runs off the edge
    // of the field. Faster than every conscious enemy, it gets away; as fast as
    // the fastest, it has an even chance; slower than any, it cannot leave.
    [[nodiscard]] int current_speed(const Actor &) const;
    [[nodiscard]] bool can_run_off(const Actor &) const;
    [[nodiscard]] std::vector<Cell> off_field_steps(const Actor &) const;
    void run_off_field(Actor &);
    void remove_fled();
    // A member who got away: out of the fight, back with the party after it.
    struct Departed
    {
        Participant source;
        int hp{}, max_hp{}, armor_class{};
        VitalState vitals;
        std::vector<std::pair<std::string, unsigned>> thrown_gear_left;
        bool surrendered{};
    };
    std::vector<Departed> fled_;
    // Morale (the original's rule): checked at the start of each turn of a
    // creature the rules control. The encounter's morale is set by its script.
    unsigned morale_{100};
    void check_morale(Actor &);
    void throw_gear(Actor &, Actor &, const detail::ThrownGear &);
    std::vector<HeldItemView> items_;
    void initialize_items();
    Definition equipped_definition(const Actor &a, const std::vector<HeldItemView> &items) const;
    std::vector<LogEntry> log_;
    std::vector<Cell> path_;
    std::size_t path_index_{};
    std::vector<EntityId> reactors_;
    std::size_t reactor_index_{};
    // A creature moving outside its own turn (Dissonant Whispers' flight),
    // with the feet it has left; path_ and reactors_ then follow it.
    struct Flight
    {
        EntityId actor{};
        int remaining{};
    };
    std::optional<Flight> flight_;

    // The creature path_ moves: the fleeing one, or the one whose turn it is.
    Actor &mover()
    {
        return flight_ ? actor(flight_->actor) : actors_[turn_];
    }

    const Actor &mover() const
    {
        return flight_ ? actor(flight_->actor) : actors_[turn_];
    }

    int mover_budget(const Actor &m) const
    {
        return flight_ ? flight_->remaining : movement_left(m);
    }

    const Definition &def(const Actor &a) const
    {
        return a.form ? *a.form : a.definition;
    }

    const Actor &actor(EntityId id) const
    {
        const auto found = std::find_if(actors_.begin(), actors_.end(), [&](const auto & a)
        {
            return a.source.id == id;
        });
        if (found == actors_.end())
            throw std::logic_error("No combatant has id " + std::to_string(id));
        return *found;
    }

    Actor &actor(EntityId id)
    {
        return const_cast<Actor &>(std::as_const(*this).actor(id));
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
            log_.erase(log_.begin());
        log_.push_back({std::move(english), std::move(message)});
    }

    bool line_of_sight(Cell a, Cell b) const
    {
        return detail::has_line_of_sight(board_, a, b);
    }

    // Heavily Obscured squares block sight into and out of them.
    bool clear_view(const Actor &a, const Actor &b) const
    {
        return conscious(a) && !detail::blinded(a.effects) && !obscured(a, a.source.cell) &&
               !obscured(a, b.source.cell) && line_of_sight(a.source.cell, b.source.cell);
    }

    bool can_see(const Actor &a, const Actor &b) const
    {
        return clear_view(a, b) && !unseen(a, b);
    }

    // An Invisible creature hides from all but those with See Invisibility; a
    // hidden one has the Invisible condition until it is found.
    static bool unseen(const Actor &viewer, const Actor &target)
    {
        // Faerie Fire, Starry Wisp and Shining Smite keep a creature from
        // being Invisible.
        return (detail::has_effect(target.effects, detail::EffectKind::invisible) ||
                hidden(target)) &&
               !detail::has_effect(viewer.effects, detail::EffectKind::see_invisibility) &&
               !detail::has_effect(target.effects, detail::EffectKind::outlined) &&
               !detail::has_effect(target.effects, detail::EffectKind::lit) &&
               !detail::has_effect(target.effects, detail::EffectKind::shining);
    }

    [[nodiscard]] static bool hidden(const Actor &a)
    {
        return detail::has_effect(a.effects, detail::EffectKind::hidden);
    }

    // Hide (SRD 5.2.1, HIDE-1): out of every enemy's line of sight, or Heavily
    // Obscured from it, a DC 15 Dexterity (Stealth) check hides the creature;
    // the total is the DC to find it.
    [[nodiscard]] bool can_hide(const Actor &a) const;
    void hide(Actor &a);
    // The Search action: a Wisdom (Perception) check finds each hidden enemy
    // in clear view whose Stealth total it reaches.
    void search(Actor &searcher);
    // An enemy in clear view whose Passive Perception reaches a hidden
    // creature's Stealth total finds it (HIDE-1).
    void notice_hidden();
    void find_hidden(Actor &hider, const Actor &finder);
    // Hiding ends after its creature makes an attack roll, casts any spell
    // (HIDE-1) or makes a loud noise, such as a Breath Weapon.
    void end_hiding(EntityId id);

    // Invisibility ends right after its creature casts a spell, once any attack
    // roll the spell makes has had the benefit; casting Invisibility on oneself
    // starts it rather than ending it, so callers check before the spell. The
    // end is an ordinary call after the action rather than a destructor's work,
    // so an exception (a reaction question undoing the action) skips it and
    // nothing can throw from a destructor (Effective C++ Item 8).
    [[nodiscard]] static bool invisible(const Actor &a);
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

    // An actor's place in the turn order, found by id: a reference may be to a
    // copy (thrown weapons attack with one), so its address says nothing
    // (Effective C++ Item 27).
    std::size_t index_of(EntityId id) const
    {
        const auto found = std::find_if(actors_.begin(), actors_.end(), [&](const auto & a)
        {
            return a.source.id == id;
        });
        if (found == actors_.end())
            throw std::logic_error("No combatant has id " + std::to_string(id));
        return static_cast<std::size_t>(found - actors_.begin());
    }

    unsigned turn_end_ms(std::size_t index) const
    {
        return static_cast<unsigned>((index + 1) * detail::round_ms / actors_.size());
    }

    bool shares_occupied_space(const Actor &who) const
    {
        return std::any_of(actors_.begin(), actors_.end(),
                           [&](const auto & other)
        {
            return other.source.id != who.source.id && !other.life.dead &&
                   other.source.cell == who.source.cell;
        });
    }

    void clear_departed_overlaps()
    {
        for (auto &a : actors_)
            if (a.life.dead || !shares_occupied_space(a))
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
    // Darts striking one creature together, as one hit (Magic Missile).
    // `split`: other creatures take darts too, so the log names this one.
    void missile_hits(Actor &a, Actor &target, const detail::SpellDef &spell,
                      const Dice &rolled, unsigned darts, bool split);
    // `targets` holds one creature, or for a split spell one per dart or ray.
    void cast_resolved_spell(const detail::SpellDef &spell, std::string_view verb, Actor &a,
                             std::span<const EntityId> targets);
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
    // The area's squares on an open field, before walls block any of them.
    [[nodiscard]] std::vector<Cell> area_shape(const detail::SpellDef &spell,
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
    // A pushed creature stopped by a wall, an obstacle or another creature with
    // `short_feet` of its push left takes 1d6 Bludgeoning for each 10 feet or
    // part of it, up to 3d6 (PUSH-1).
    void slam(Actor &target, int short_feet);
    // Whether a wall, an obstacle or another creature stands in `cell` (on the board).
    bool blocks_push(Cell cell, EntityId pushed) const;
    // Whether `other` may make an Opportunity Attack on `moving` stepping from
    // `from` to `to`.
    bool provokes(const Actor &other, const Actor &moving, Cell from, Cell to) const;
    // Dissonant Whispers' flight from `from` within `budget` feet: the
    // farthest square, by the route that provokes fewest Opportunity Attacks.
    std::vector<Cell> flight_route(const Actor &from, const Actor &target, int budget) const;
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
    void change_speed(Actor &a, int old_speed);
    // Land's Aid: thorns in a 10-foot Sphere and flowers that heal one ally.
    void lands_aid(Actor &druid, Cell center);
    void breath_weapon(Actor &dragonborn, const PendingArea &aimed);
    void spend_trait_use(Actor &a);
    // Heroic Inspiration (a Human's Resourceful, HEROIC-1): the d20 of the
    // first failed test that a reroll could save is rolled again. A Long Rest
    // gives it; a new character has none yet. The new roll, if it was used.
    std::optional<int> heroic_reroll(Actor &a);
    // Heat Metal targets only metal armor a creature wears.
    [[nodiscard]] bool wears_metal(const Actor &creature) const;
    // Heat Metal's 2d8 Fire and Constitution save.
    void heat_metal(const Actor &caster, Actor &creature);
    void potent_cantrip(Actor &target, const detail::SpellDef &spell, detail::DamageDice rolled);
    [[nodiscard]] std::vector<EntityId> sculpted(const Actor &caster, const detail::SpellDef &spell,
            unsigned slot_level, const std::vector<Cell> &cells) const;
    void cast_area();
    void cast_aimed_area(Actor &a, const PendingArea &aimed);
    void squeeze_ensnared(Actor &a);
    void escape_ensnaring(Actor &a);
    // Guidance's 1d4 for an ability check with `skill`, logged; 0 without it.
    [[nodiscard]] int guidance_bonus(const Actor &a, std::string_view skill);
    [[nodiscard]] bool mark_can_move(const Actor &caster) const;
    [[nodiscard]] bool hexed_by(const Actor &target, const Actor &caster) const;
    [[nodiscard]] bool hex_can_move(const Actor &caster) const;
    // Sacred Weapon's attack bonus, 0 without it.
    [[nodiscard]] int sacred_weapon_bonus(const Actor &a) const;
    [[nodiscard]] detail::DamageType sacred_damage_type(const Actor &a) const;
    unsigned next_save_ms(EntityId target) const;
    unsigned next_turn_ms(const Actor &target) const;
    void advance_turn_time();
    // Halfling Luck, logged when it rerolled a 1 (automatic, as AUTO-1).
    void log_luck(const Actor &a, bool rerolled);
    void log_save(const Actor &target, const detail::SaveResult &result);
    // `advantage` is a feature's own Advantage, such as a Large creature against
    // Ensnaring Strike, and `disadvantage` a lasting spell's Heightened Spell;
    // conditions and armor are applied here.
    bool saving_throw_succeeds(const Actor &target, detail::Ability ability, int dc,
                               bool advantage = false, bool disadvantage = false);
    // Heightened Spell: the creature saves against the caster's spell with
    // Disadvantage for as long as it lasts.
    void mark_heightened(const Actor &caster, const detail::SpellDef &spell);
    [[nodiscard]] bool heightened_against(const Actor &target, const Actor &caster,
                                          std::string_view spell) const
    {
        return detail::heightened_against(target.effects, scope_, caster.source.id, spell);
    }
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
        return natural == 20 || (!spell && natural >= def(a).critical_from) ||
               (helpless(target) && distance(a.source.cell, target.source.cell) <= 5);
    }

    // Unconscious (or asleep) or Paralyzed: attacked with Advantage, critically hit within
    // 5 feet, failing Strength and Dexterity saves.
    static bool helpless(const Actor &target)
    {
        return unconscious(target) || detail::paralyzed(target.effects) ||
               detail::has_effect(target.effects, detail::EffectKind::asleep) ||
               detail::has_effect(target.effects, detail::EffectKind::knocked_out);
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
    // Innate Sorcery. A spell Magic Initiate teaches uses the feat's ability;
    // a class feature names no spell and uses the class's.
    [[nodiscard]] int spell_dc(const Actor &caster, std::string_view spell = {}) const
    {
        return 8 + casting_bonus(def(caster), spell) +
               (detail::has_effect(caster.effects, detail::EffectKind::innate_sorcery) ? 1 : 0);
    }
    // The source recorded on an effect that `a` imposes.
    [[nodiscard]] detail::EffectSource effect_source(const Actor &a) const
    {
        return {.scope = scope_, .actor = a.source.id};
    }

    [[nodiscard]] bool strength_attack(const Actor &a, AttackRange range) const;
    void attack_recklessly(Actor &a);
    // Brutal Strike's blow after its hit: Forceful Blow pushes the target 15
    // feet and lets the Barbarian follow; Hamstring Blow slows it; Staggering
    // and Sundering Blows mark it until the Barbarian's next turn.
    void brutal_blow(Actor &barbarian, Actor &target, BrutalBlow blow);
    // Reliable Talent (Rogue 7): a proficient skill check's d20 counts as at least 10.
    int reliable(const Actor &a, std::string_view skill, int natural);
    // Evasion (Rogue 7): the damage a creature takes from a save-for-half
    // effect, given whether it saved.
    int evaded(const Actor &target, detail::Ability save, bool halves, bool saved, int total) const;
    // Mindless Rage (Berserker 6): raging, immune to Charmed and Frightened.
    bool mindless(const Actor &a) const;
    int frenzy_damage(Actor &berserker, bool critical);
    // Enlarge or Reduce changes a weapon hit's damage.
    int resized_damage(const Actor &a, int amount);
    int burst_damage(const SpellAttack &spell, bool critical);
    [[nodiscard]] static bool fought_advantage(const detail::SpellDef &spell);
    // Whether `a` is Charmed by `other`, which it then cannot attack or target.
    [[nodiscard]] bool charmed_by(const Actor &a, EntityId other) const;
    [[nodiscard]] bool aura_covers(const Actor &paladin, const Actor &a) const;
    [[nodiscard]] int aura_of_protection(const Actor &a) const;
    [[nodiscard]] bool aura_of_devotion(const Actor &a) const;
    // Frightened: a living creature that frightens `a` is in its line of sight.
    [[nodiscard]] std::vector<const Actor *> fears_in_sight(const Actor &a) const;
    // Intimidating Presence (Berserker 14): each enemy within 30 feet makes a
    // Wisdom save or is Frightened for up to a minute.
    void intimidate(const Actor &barbarian);
    // Persistent Rage (Barbarian 15) is offered at Initiative while a Rage use is spent.
    [[nodiscard]] bool persistent_ready(const Actor &a) const
    {
        return def(a).persistent_rage && a.arcane > 0 && a.channel_divinity < def(a).rages;
    }
    // True Strike deals Radiant damage or the weapon's own type, as the caster chooses.
    enum class TrueStrikeDamage
    {
        weapon,
        radiant
    };

    void strike_true(Actor &a, Actor &target, TrueStrikeDamage choice);
    bool strikes_duplicate(const Actor &attacker, Actor &target);
    // A weapon attack, or a spell attack when `spell` is given.
    bool attack(Actor &a, Actor &target, AttackRange range,
                const std::optional<SpellAttack> &spell = std::nullopt);
    detail::Mastery weapon_mastery(const Actor &, AttackRange range) const;
    bool mastery_capacity(const Actor &, const Actor &, AttackRange range) const;
    detail::RollModifiers attack_modifiers(const Actor &a, const Actor &target, AttackRange range,
                                           AttackSource source) const;
    Dice weapon_dice(const Actor &a, AttackRange range) const;
    detail::DamageDieRule weapon_die_rule(const Actor &a, AttackRange range) const;
    void apply_hit(Actor &a, Actor &target, const ResolvedAttack &resolved);
    static void extend_run(Actor &a, Cell from, Cell to);
    [[nodiscard]] bool charges(const Actor &a, const Actor &target) const;
    bool sneak_eligible(const Actor &a, const Actor &target, AttackRange range, int mode) const;
    bool ally_beside(const Actor &a, const Actor &target) const;
    int roll_advantage_damage(const Actor &a, const Actor &target, int natural);
    // `forgo` dice are given up for a Cunning Strike.
    [[nodiscard]] int roll_sneak_attack(Actor &a, const Actor &target, int natural, int forgo = 0);
    // Cunning Strike's effect after a hit that dealt Sneak Attack damage.
    void cunning_effect(Actor &rogue, Actor &target, CunningStrike kind);
    // The Cunning Strike effects a command's verb names, if it names any.
    [[nodiscard]] static std::optional<std::pair<CunningStrike, std::optional<CunningStrike>>>
            cunning_choice(std::string_view verb);
    // Stunning Strike's Constitution save after a hit.
    void stunning_strike(Actor &monk, Actor &target);
    // Studied Attacks: a miss gives the Fighter's next attack on the target Advantage.
    void study(const Actor &fighter, Actor &target);
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
    // An Incapacitated creature loses Concentration and ends its Turn Undead.
    void end_incapacitated_effects(Actor &a);
    unsigned selection_maximum(const PendingSelection &) const;
    void choose_target(const Command &command);
    void cast_on_selection();
    void cast_selected(Actor &a, const PendingSelection &selection);
    // Bless adds 1d4 to attack rolls and saving throws; Bane subtracts 1d4.
    int blessing_die(const Actor &a);
    void burn_searing_smites(Actor &a);
    void update_outcome();
    void wake_resting_participants();
    void bandage_after_victory();
    // A creature with Regeneration that has not died: down at 0 HP, it may rise.
    bool may_rise(const Actor &a) const
    {
        return def(a).regeneration && !a.life.dead;
    }
    // As in the original game, a downed troll's square can be stood on, and a
    // creature standing there keeps it from getting up.
    bool downed_riser(const Actor &a) const
    {
        return may_rise(a) && a.life.hp == 0;
    }
    void regenerate(Actor &a);
    void get_up(Actor &a);
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
            if (actors_[i].life.hp == 0 && !actors_[i].life.dead && !actors_[i].life.stable)
                actors_[i].life.recovery.death_save_in_ms = i ? turn_end_ms(i - 1) : 0;
        }
        // Perfect Focus (Monk 15): at least 4 Focus Points when the fight starts.
        for (auto &a : actors_)
            if (def(a).perfect_focus && a.surges < 4 && conscious(a))
            {
                a.surges = 4;
                log(a.source.name + " has 4 Focus Points (Perfect Focus).",
                {"{name} has 4 Focus Points (Perfect Focus).", {{"name", a.source.name}}});
            }
        if (outcome_ == Outcome::ongoing && !begin_turn())
            end_turn();
    }

    // Uncanny Metabolism is offered at Initiative only while it would restore
    // something: spent Focus or missing Hit Points.
    bool metabolism_ready(const Actor &a) const
    {
        return def(a).metabolism && a.arcane > 0 &&
               (a.surges < surge_capacity(def(a)) || a.life.hp < max_hp(a));
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
        if (command.verb == "persistent_rage")
        {
            auto &barbarian = actor(command.actor);
            --barbarian.arcane;
            barbarian.channel_divinity = def(barbarian).rages;
            log(barbarian.source.name + " regains every Rage use (Persistent Rage).",
            {"{name} regains every Rage use (Persistent Rage).", {{"name", barbarian.source.name}}});
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
    void restore_self(Actor &a);
    void dispatch_tracking_turn(const Command &command);
    // Heightened Focus: the carried ally steps into the square the Monk left,
    // provoking nothing.
    void follow_carrier(const Actor &monk, Cell departed);
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
            carried.id = static_cast<unsigned>(items_.size() + 1);
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
        detail::apply_poisoned(target.effects, effect_source(monk), monk.source.name,
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
        value = 10 + static_cast<int>(std::find(transmuted_types.begin(),
                                                transmuted_types.end(), type) -
                                      transmuted_types.begin());
        label = "Transmuted Spell";
    }
    else
    {
        value = static_cast<int>(std::find(metamagic_ids.begin(), metamagic_ids.end(), choice) -
                                 metamagic_ids.begin());
        label = metamagic_labels[static_cast<std::size_t>(value)];
    }
    detail::apply_spell_benefit(a.effects, effect_source(a), a.source.name,
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
        ++a.slots[second ? 2 : 1];
    }
    else
    {
        --a.slots[second ? 2 : 1];
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

void Session::use_focus_movement(Actor &a, std::string_view verb, EntityId carried)
{
    // Disengage with Patient Defense, Dash with Step of the Wind; a Focus Point
    // adds Dodge or Disengage.
    const bool focus = verb.ends_with("_focus") || verb == "step_of_the_wind_carry";
    const bool patient = verb.starts_with("patient_defense");
    a.bonus = false;
    if (focus)
        --a.surges;
    if (patient || focus)
        a.disengaged = true;
    if (patient && focus)
        a.dodge = true;
    // Heightened Focus: Patient Defense with Focus gives two Martial Arts dice
    // of Temporary Hit Points.
    if (patient && focus && def(a).heightened_focus)
        if (const int amount = roll(def(a).martial_die) + roll(def(a).martial_die);
                amount > a.life.temporary_hp.amount)
            detail::grant_temporary_hp(a.life, {amount, "feature:heightened_focus"},
                                       TemporaryHpChoice::use_new);
    if (!patient)
        a.fleet_step = false;
    if (!patient)
    {
        a.movement += def(a).speed;
        ++a.dashes;
    }
    const std::string name = patient ? "Patient Defense" : "Step of the Wind";
    log(a.source.name + " uses " + name + ".",
    {"{name} uses {feature}.", {{"name", a.source.name}, {"feature", name, true}}});
    // Heightened Focus: the ally moves with the Monk for the rest of the turn.
    if (carried)
    {
        a.carried = carried;
        log(a.source.name + " carries " + actor(carried).source.name + " (Step of the Wind).",
        {
            "{name} carries {target} (Step of the Wind).",
            {{"name", a.source.name}, {"target", actor(carried).source.name}}
        });
    }
}

Actor Session::unarmed_actor(const Actor &a) const
{
    // A Monk's Unarmed Strike: the Martial Arts die plus its modifier, with no
    // weapon's mastery, grip or feat riders.
    auto result = a;
    auto &d = result.definition;
    d.melee = {1, d.martial_die, d.martial_modifier};
    d.melee_bonus = 2 + (d.level - 1) / 4 + d.martial_modifier;
    d.melee_ability = d.martial_modifier;
    d.melee_type = d.empowered_strikes ? detail::DamageType::force
                                       : detail::DamageType::bludgeoning;
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
                    w && reaches(melee_reach))
                return true;
    return false;
}

bool Session::weapon_reaction(const Actor &a, const Definition &weapon) const
{
    if (!pending() || path_index_ >= path_.size())
        return false;
    return distance(a.source.cell, mover().source.cell) <= weapon.reach &&
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
                static_cast<unsigned>(items_.size() + 1),
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
                    items_.push_back({static_cast<unsigned>(items_.size() + 1),
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
        if (item.holder == a.source.id && !item.stowed && item.id != a.selected_weapon &&
                !(a.shield_off && item.definition == "shield"))
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

bool Session::carries(const Actor &a, std::string_view key) const
{
    const auto &keys = def(a).equipment_keys;
    return !a.source.character_profile.empty() &&
           std::find(keys.begin(), keys.end(), key) == keys.end() &&
           std::any_of(a.source.inventory.begin(), a.source.inventory.end(),
                       [&](const auto & item)
    {
        return item.definition == key && item.quantity;
    });
}

std::string_view Session::carried_ranged_weapon(const Actor &a) const
{
    if (a.source.character_profile.empty() || def(a).range)
        return {};
    const auto has_ammunition = [&](detail::Ammunition type)
    {
        return type == detail::Ammunition::none ||
               std::any_of(a.source.inventory.begin(), a.source.inventory.end(),
                           [&](const auto & item)
        {
            const auto *ammunition = detail::ammunition(item.definition);
            return ammunition && ammunition->type == type && item.quantity;
        });
    };
    for (const auto &item : a.source.inventory)
        if (const auto *w = detail::weapon(item.definition);
                w && w->ranged && item.quantity && has_ammunition(w->ammunition) &&
                (w->hands == 1 || !wears_shield(def(a))))
            return w->key;
    return {};
}

bool Session::shield_blocks_bow(const Actor &a) const
{
    if (a.shield_off || !wears_shield(def(a)) || !carried_ranged_weapon(a).empty())
        return false;
    auto unshielded = a;
    unshielded.definition = without_shield(a.source.character_profile, a.definition);
    return !carried_ranged_weapon(unshielded).empty();
}

// The original's morale test. A creature with morale of its own holds while
// it has lost no more of its Hit Points (in percent) than that morale; one
// without, or one that loses it, holds while its side keeps at least 100 less
// the encounter's morale percent of its Hit Points (in steps of 5). Broken, it
// flees in panic when no opponent is faster; otherwise one with Intelligence
// above 5 surrenders, and the rest fight on. It is tested again every turn.
void Session::check_morale(Actor &a)
{
    const int lost = 100 - 100 * a.life.hp / std::max(1, max_hp(a));
    bool holds = a.source.morale > 0 && static_cast<int>(a.source.morale) >= lost;
    if (!holds)
    {
        int remaining = 0, total = 0;
        for (const auto &other : actors_)
            if (other.source.side == a.source.side)
            {
                total += max_hp(other);
                if (!other.life.dead && !other.fled)
                    remaining += other.life.hp;
            }
        for (const auto &gone : fled_)
            if (gone.source.side == a.source.side)
                total += gone.max_hp;
        const int percent = total ? remaining * 100 / total / 5 * 5 : 0;
        holds = percent > 0 && percent >= 100 - static_cast<int>(morale_);
    }
    if (holds)
    {
        a.panicked = false;
        return;
    }
    const bool outpaced = std::any_of(actors_.begin(), actors_.end(), [&](const auto & other)
    {
        return other.source.side != a.source.side && other.life.hp > 0 && !other.life.dead && !other.fled &&
               current_speed(other) > current_speed(a);
    });
    // One that already failed to get away is cornered and fights on.
    if (!outpaced && a.must_stay)
    {
        a.panicked = false;
        return;
    }
    if (!outpaced)
    {
        if (!a.panicked)
            log(a.source.name + " flees in panic.",
            {"{name} flees in panic.", {{"name", a.source.name}}});
        a.panicked = true;
        return;
    }
    a.panicked = false;
    if (a.source.intelligence > 5)
    {
        end_concentration(a);
        a.surrendered = true;
        a.fled = true;
        log(a.source.name + " surrenders.", {"{name} surrenders.", {{"name", a.source.name}}});
    }
}

int Session::current_speed(const Actor &a) const
{
    return std::max(0, def(a).speed - detail::speed_penalty(a.effects));
}

bool Session::can_run_off(const Actor &a) const
{
    if ((a.source.side != Side::party && !a.panicked) || !conscious(a) || a.must_stay || a.fled ||
            actors_[turn_].source.id != a.source.id || outcome_ != Outcome::ongoing || pending() ||
            temporary_offer_ || check_choice_ || graze_ || effect_waiting() || champion_move_ ||
            movement_left(a) < 5 || off_field_steps(a).empty())
        return false;
    for (const auto &other : actors_)
        if (other.source.side != a.source.side && conscious(other) &&
                current_speed(other) > current_speed(a))
            return false;
    return true;
}

std::vector<Cell> Session::off_field_steps(const Actor &a) const
{
    std::vector<Cell> steps;
    for (const auto step : {Cell{0, -1}, Cell{1, 0}, Cell{0, 1}, Cell{-1, 0}})
        if (const Cell next{a.source.cell.x + step.x, a.source.cell.y + step.y};
                !board_.contains(next))
            steps.push_back(next);
    return steps;
}

void Session::run_off_field(Actor &a)
{
    int fastest = 0;
    for (const auto &other : actors_)
        if (other.source.side != a.source.side && conscious(other))
            fastest = std::max(fastest, current_speed(other));
    if (current_speed(a) < fastest || (current_speed(a) == fastest && roll(2) == 1))
    {
        // Cornered, a broken monster fights on from now, not from its next turn.
        a.must_stay = true;
        a.panicked = false;
        log(a.source.name + " cannot get away and must stay.",
        {"{name} cannot get away and must stay.", {{"name", a.source.name}}});
        return;
    }
    end_concentration(a);
    // Wild Shape lasts until the fight ends (WILD-1); for the one who flees, it has.
    end_wild_shape(a);
    a.fled = true;
    log(a.source.name + " flees the battle.",
    {"{name} flees the battle.", {{"name", a.source.name}}});
}

// Takes those who ran off the field out of the turn order; the turn index
// keeps pointing at the same creature.
void Session::remove_fled()
{
    for (std::size_t i = actors_.size(); i-- > 0;)
    {
        const auto &a = actors_[i];
        if (!a.fled)
            continue;
        Departed gone{a.source, a.surrendered ? 0 : a.life.hp, max_hp(a), armor_class(a), vitals(a)};
        gone.surrendered = a.surrendered;
        if (!a.source.character_profile.empty())
            for (const auto &gear : detail::thrown_gear_items)
                gone.thrown_gear_left.emplace_back(
                    std::string(gear.key),
                    a.thrown_gear_left[static_cast<std::size_t>(gear.effect)]);
        fled_.push_back(std::move(gone));
        actors_.erase(actors_.begin() + static_cast<std::ptrdiff_t>(i));
        if (i < turn_)
            --turn_;
        else if (turn_ >= actors_.size())
            turn_ = 0;
    }
}

// A carried Torch strikes as a Simple Melee weapon for 1 Fire damage.
void Session::torch_attack(Actor &a, Actor &target)
{
    auto striker = thrown_actor(a, "torch");
    attack(striker, target, AttackRange::melee);
    a.aim_ready = striker.aim_ready;
}

// Acid, Alchemist's Fire or Oil thrown in place of an attack: the target makes
// a Dexterity save against 8 + the thrower's Dexterity modifier and Proficiency
// Bonus (SRD 5.2.1 adventuring gear).
void Session::throw_gear(Actor &a, Actor &target, const detail::ThrownGear &gear)
{
    --a.thrown_gear_left[static_cast<std::size_t>(gear.effect)];
    const auto &d = def(a);
    const int dc = 8 + d.dexterity + 2 + (d.level - 1) / 4;
    const auto label = std::string(gear.label);
    log(a.source.name + " throws " + label + " at " + target.source.name + ".",
    {
        "{name} throws {item} at {target}.",
        {{"name", a.source.name}, {"item", label, true}, {"target", target.source.name}}
    });
    // Forcing an enemy's save on the thrower's own turn extends a Rage.
    if (target.source.side != a.source.side && actors_[turn_].source.id == a.source.id)
        extend_rage(a);
    if (saving_throw_succeeds(target, detail::Ability::dexterity, dc))
        return;
    if (gear.effect == detail::ThrownGearEffect::oil)
    {
        target.oiled_until_round = static_cast<int>(round_) + 10;
        log(target.source.name + " is covered in oil.",
        {"{name} is covered in oil.", {{"name", target.source.name}}});
        return;
    }
    const bool acid = gear.effect == detail::ThrownGearEffect::acid;
    const auto type = acid ? detail::DamageType::acid : detail::DamageType::fire;
    const int amount = resolved_damage(target, type, acid ? roll(6) + roll(6) : roll(4));
    log(target.source.name + " takes " + std::to_string(amount) + " " +
        std::string(detail::damage_name(type)) + " damage.",
    {
        "{name} takes {damage} {type} damage.",
        {   {"name", target.source.name}, {"damage", std::to_string(amount)},
            {"type", std::string(detail::damage_name(type)), true}
        }
    });
    damage(target, amount, false);
    if (!acid && !target.life.dead)
    {
        target.burning = true;
        log(target.source.name + " starts burning.",
        {"{name} starts burning.", {{"name", target.source.name}}});
    }
}

// Thrown weapons work like ammunition: the weapon stays where it was, held or
// carried, so throwing never changes what the thrower holds.
void Session::throw_weapon(Actor &a, Actor &target, unsigned token, bool light)
{
    auto attacker = thrown_actor(a, items_.at(token - 1).definition);
    attacker.light_damage = light;
    attack(attacker, target, AttackRange::ranged);
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
                if (board.at(cell) == Terrain::open)
                    board.terrain[static_cast<std::size_t>(cell.y * board.width + cell.x)] =
                        Terrain::difficult;
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
            if ((!dx && !dy) || !board_.contains(cell) || board_.at(cell) == Terrain::obstacle ||
                    std::any_of(actors_.begin(), actors_.end(), [&](const auto & other)
            {
                return !other.life.dead && other.source.cell == cell;
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
        // current cell is the path origin, not an obstacle. As in the original
        // game, a creature may stop on the downed (dying heroes walling in the
        // last one could otherwise leave a fight no one can end). Halfling
        // Nimbleness passes through a larger enemy as through an ally: never
        // stopping there.
        if (other.life.dead || downed_riser(other) || other.source.id == mover.source.id)
            continue;
        const bool nimble_pass = def(mover).nimble && def(other).size > def(mover).size;
        occupants.push_back({other.source.cell,
                             other.source.side != mover.source.side && !nimble_pass,
                             unconscious(other), other.life.hp == 0});
    }
    // Frightened: no square closer to a creature it fears and can see.
    auto board = zoned_board();
    for (const auto *fear : fears_in_sight(mover))
        for (int y = 0; y < board.height; ++y)
            for (int x = 0; x < board.width; ++x)
                if (distance({x, y}, fear->source.cell) <
                        distance(mover.source.cell, fear->source.cell))
                    board.terrain[static_cast<std::size_t>(y * board.width + x)] =
                        Terrain::obstacle;
    return {board, mover.source.cell, occupants, mover.effects.prone};
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
    s.log_entries = log_;
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
    // A Shield prompt during the cast comes first; the choice resumes after it.
    if (selection_ && !s.reaction_pending)
    {
        s.actor = selection_->caster;
        s.spell_targeting = SpellTargeting{selection_->caster, selection_->verb,
                                           selection_->chosen, selection_maximum(*selection_),
                                           splits_among_targets(*detail::find_spell(selection_->verb))};
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
            if (s.optional_effect_choice->options.size() > 1 &&
                    actors_[turn_].source.side == Side::party)
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
                                                    def(actor(c.actor)).medicine + c.guidance,
                                                    c.natural + def(actor(c.actor)).medicine +
                                                    c.guidance,
                                                    10,
                                                    actor(c.actor).winds};
    }
    if (temporary_offer_)
        s.temporary_hp_offer = TemporaryHpOffer{actors_[turn_].source.id,
                                                actors_[turn_].life.temporary_hp, *temporary_offer_};
    for (const auto &a : actors_)
    {
        std::string status = a.life.dead      ? "Dead"
                             : a.life.hp == 0 ? (a.life.stable ? "Stable, unconscious" : "Unconscious")
                             : a.effects.prone    ? "Prone"
                             : a.dodge            ? "Dodging"
                             : "Ready";
        if (def(a).slots[1])
            status += " | slots " + std::to_string(a.slots[1]);
        for (unsigned level = 2; level <= highest_slot_level; ++level)
            if (def(a).slots[level])
                status += " | L" + std::to_string(level) + " slots " + std::to_string(a.slots[level]);
        if (def(a).winds)
            status += " | Second Wind " + std::to_string(a.winds);
        s.combatants.push_back(
        {
            a.source.id, a.source.name, a.source.definition, a.source.side, a.source.cell, a.life.hp,
            max_hp(a), armor_class(a), a.initiative,
            champion_move_ && champion_move_->actor == a.source.id ? champion_move_->remaining
            : movement_left(a),
            a.actions.available() && conscious(a), a.bonus && conscious(a),
            a.reaction && conscious(a), conscious(a), a.life.dead, a.facing_left, status, vitals(a)});
        const auto display = combat_display(a.source.definition);
        auto &view = s.combatants.back();
        view.regenerates = def(a).regeneration > 0;
        view.regeneration_stopped = a.regeneration_blocked;
        view.has_torch = carries(a, "torch") ||
                         std::find(def(a).equipment_keys.begin(), def(a).equipment_keys.end(),
                                   "torch") != def(a).equipment_keys.end();
        view.burning = a.burning;
        view.oiled = a.oiled_until_round >= static_cast<int>(round_);
        view.hidden = hidden(a);
        view.panicked = a.panicked;
        view.spells = def(a).spells;
        view.can_flee = a.source.side == Side::party && conscious(a) && !a.must_stay &&
                        std::none_of(actors_.begin(), actors_.end(), [&](const auto & other)
        {
            return other.source.side != a.source.side && conscious(other) &&
                   current_speed(other) > current_speed(a);
        });
        for (const auto &gear : detail::thrown_gear_items)
            if (a.source.side == Side::party && !a.source.character_profile.empty())
                view.thrown_gear_left.emplace_back(
                    std::string(gear.key),
                    a.thrown_gear_left[static_cast<std::size_t>(gear.effect)]);
        view.temporary_hp = a.life.temporary_hp;
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
        // Its morale broke: it runs for the edge (MORALE-1).
        if (a.panicked)
            view.conditions.push_back({"Fleeing in panic", {}});
        if (def(a).cunning)
            view.bonus_actions = {"cunning_dash", "cunning_disengage", "cunning_hide"};
        if (def(a).sneak_level >= 3)
            view.bonus_actions.push_back("steady_aim");
        if (def(a).lay_on_hands)
        {
            view.bonus_actions.push_back("lay_on_hands");
            view.bonus_actions.push_back("lay_on_hands_cure");
        }
        if (def(a).rages)
            view.bonus_actions.insert(view.bonus_actions.end(), {"rage", "extend_rage"});
        if (def(a).intimidating_presence)
            view.bonus_actions.insert(view.bonus_actions.end(),
                                      {"intimidating_presence", "intimidating_presence_rage"});
        if (def(a).martial_arts)
            view.bonus_actions.push_back("martial_arts");
        if (def(a).innate_sorcery)
            view.bonus_actions.push_back("innate_sorcery");
        if (def(a).bardic_inspiration)
            view.bonus_actions.push_back("bardic_inspiration");
        if (def(a).wild_shapes)
        {
            for (const auto &form : detail::beast_forms)
                view.bonus_actions.push_back("wild_shape_" + std::string(form.key));
            view.bonus_actions.push_back("leave_wild_shape");
        }
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
            "flurry_of_blows", "flurry_addle", "flurry_push", "flurry_topple", "flurry_stun",
            "patient_defense", "patient_defense_focus", "step_of_the_wind",
            "step_of_the_wind_focus", "step_of_the_wind_carry"
        });
        if (detail::knows_spell(def(a).spells, "divine_smite") && def(a).free_smite)
            view.bonus_actions.push_back("divine_smite_free");
        for (const auto *smite :
                {"divine_smite", "searing_smite", "ensnaring_strike"
                })
            if (detail::knows_spell(def(a).spells, smite))
                view.bonus_actions.push_back(smite);
        if (detail::knows_spell(def(a).spells, "divine_smite") && def(a).slots[2])
            view.bonus_actions.push_back("divine_smite_2");
        if (detail::knows_spell(def(a).spells, "shining_smite"))
            view.bonus_actions.push_back("shining_smite");
        for (const auto &descriptor : resource_descriptors)
            if (descriptor.combat_view && capacity_of(descriptor, def(a)))
                view.resources.push_back(resource_pool(descriptor, a, def(a)));
        add_initiate_pools(view.resources, a, def(a));
        add_trait_pool(view.resources, a, a.definition);
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
        messages.push_back({a.life.dead      ? "Dead"
                            : a.life.hp == 0 ? (a.life.stable ? "Stable, unconscious" : "Unconscious")
                            : a.effects.prone    ? "Prone"
                            : a.dodge            ? "Dodging"
                            : "Ready",
                            {}});
        if (def(a).slots[1])
            messages.push_back({"Spell slots: {count}", {{"count", std::to_string(a.slots[1])}}});
        if (def(a).slots[2])
            messages.push_back({"L2 slots: {count}", {{"count", std::to_string(a.slots[2])}}});
        for (unsigned level = 3; level <= highest_slot_level; ++level)
            if (def(a).slots[level])
                messages.push_back({"L{level} slots: {count}",
                    {{"level", std::to_string(level)}, {"count", std::to_string(a.slots[level])}}});
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
        if (detail::has_effect(a.effects, detail::EffectKind::shocking_grasp))
        {
            messages.push_back({"Shocking Grasp: cannot make Opportunity Attacks.", {}});
            view.conditions.push_back({"Shocking Grasp: cannot make Opportunity Attacks.", {}});
        }
        if (detail::frosted(a.effects))
        {
            messages.push_back({"Ray of Frost: Speed reduced by 10 feet.", {}});
            view.conditions.push_back({"Ray of Frost: Speed reduced by 10 feet.", {}});
        }
        if (detail::has_effect(a.effects, detail::EffectKind::hamstrung))
        {
            messages.push_back({"Hamstring Blow: Speed reduced by 15 feet.", {}});
            view.conditions.push_back({"Hamstring Blow: Speed reduced by 15 feet.", {}});
        }
        if (detail::has_effect(a.effects, detail::EffectKind::frosts_chill))
        {
            messages.push_back({"Frost's Chill: Speed reduced by 10 feet.", {}});
            view.conditions.push_back({"Frost's Chill: Speed reduced by 10 feet.", {}});
        }
        for (const auto &effect : a.effects.active)
            if (effect.kind == detail::EffectKind::studied)
            {
                Message message
                {
                    "Studied Attacks ({source}): source's next attack against this creature has Advantage.",
                    {{"source", effect.source_name}}
                };
                messages.push_back(message);
                view.conditions.push_back(std::move(message));
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
                    std::pair{detail::EffectKind::knocked_out, "Unconscious (Knock Out)"},
                    std::pair{detail::EffectKind::dazed, "Dazed"},
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
                    std::pair{detail::EffectKind::frightened, "Frightened"},
                    std::pair{detail::EffectKind::staggering, "Staggered (Staggering Blow)"},
                    std::pair{detail::EffectKind::sundered, "Sundered (Sundering Blow)"},
                    std::pair{detail::EffectKind::innate_sorcery, "Innate Sorcery"},
                    std::pair{detail::EffectKind::metamagic, "Metamagic readied"},
                    std::pair{detail::EffectKind::hex, "Hexed"},
                    std::pair{detail::EffectKind::outlined, "Outlined (Faerie Fire)"},
                    std::pair{detail::EffectKind::shining, "Shining (Shining Smite)"},
                    std::pair{detail::EffectKind::lit, "Lit (Starry Wisp)"},
                    std::pair{detail::EffectKind::inspired, "Inspired"},
                    std::pair{detail::EffectKind::shillelagh, "Shillelagh"},
                    std::pair{detail::EffectKind::produce_flame, "Produce Flame"},
                    std::pair{detail::EffectKind::barkskin, "Barkskin"},
                    std::pair{detail::EffectKind::flame_blade, "Flame Blade"},
                    std::pair{detail::EffectKind::heated, "Heated (Heat Metal)"},
                    std::pair{detail::EffectKind::scorched, "Scorched (Heat Metal)"},
                    std::pair{detail::EffectKind::hidden, "Hidden"}
                })
            if (detail::has_effect(a.effects, kind))
            {
                messages.push_back({label, {}});
                s.combatants.back().status += std::string(" | ") + label;
                s.combatants.back().conditions.push_back({label, {}});
            }
        for (const auto &[shown, label] :
                {
                    std::pair{a.burning, "Burning"},
                    std::pair{a.oiled_until_round >= static_cast<int>(round_), "Covered in oil"}
                })
            if (shown)
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
                const auto &form = detail::beast_forms.at(static_cast<std::size_t>(effect.dc - 1));
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
        if (detail::has_effect(a.effects, detail::EffectKind::stunned))
        {
            messages.push_back({"Stunned", {}});
            s.combatants.back().status += " | Stunned";
            s.combatants.back().conditions.push_back({"Stunned", {}});
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
    // Members who ran off the field are reported with the vitals they left with.
    for (const auto &gone : fled_)
    {
        CombatantView view{gone.source.id, gone.source.name, gone.source.definition,
                           gone.source.side, gone.source.cell, gone.hp, gone.max_hp,
                           gone.armor_class};
        view.status = gone.surrendered ? "Surrendered" : "Fled";
        view.status_messages = {{view.status, {}}};
        view.persistent = gone.vitals;
        view.thrown_gear_left = gone.thrown_gear_left;
        view.fled = !gone.surrendered;
        view.surrendered = gone.surrendered;
        s.combatants.push_back(std::move(view));
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
        const auto index = index_of(a.source.id);
        const unsigned slot = turn_end_ms(index) - (index ? turn_end_ms(index - 1) : 0);
        detail::apply_poisoned(target.effects, effect_source(a), a.source.name,
                               next_turn_ms(a) + slot);
        log(target.source.name + " is Poisoned.",
        {"{name} is Poisoned.", {{"name", target.source.name}}});
        return;
    }
    case detail::Rider::resistance:
    {
        const auto type = *detail::resistance_type(verb);
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::resistance, static_cast<int>(type));
        const auto name = std::string(detail::damage_name(type));
        log(target.source.name + " gains Resistance against " + name + " damage.",
        {
            "{name} gains Resistance against {type} damage.",
            {{"name", target.source.name}, {"type", name, true}}
        });
        return;
    }
    case detail::Rider::guidance:
    {
        const int skill = *detail::guidance_skill(verb);
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::guidance, skill);
        const auto name = std::string(skill == 0 ? "Athletics" : "Medicine");
        log(target.source.name + " gains Guidance for " + name + " checks.",
        {
            "{name} gains Guidance for {skill} checks.",
            {{"name", target.source.name}, {"skill", name, true}}
        });
        return;
    }
    case detail::Rider::sanctuary:
        detail::apply_sanctuary(target.effects, effect_source(a), a.source.name, dc);
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
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
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
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::protection_from_poison, 0);
        log(target.source.name + " gains Protection from Poison.",
        {
            "{name} gains {spell}.",
            {{"name", target.source.name}, {"spell", "Protection from Poison", true}}
        });
        return;
    case detail::Rider::mage_armor:
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
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
        if (target.life.temporary_hp.amount)
            temporary_offer_ = std::move(offered);
        else
            detail::grant_temporary_hp(target.life, offered, TemporaryHpChoice::use_new);
        return;
    }
    case detail::Rider::expeditious_retreat:
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
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
        detail::apply_repeating_condition(
            target.effects, detail::EffectKind::laughing, effect_source(a), a.source.name,
            detail::RepeatingSave{.dc = dc, .first_save_ms = next_save_ms(target.source.id)});
        target.effects.prone = true;
        log(target.source.name + " falls Prone, laughing.",
        {"{name} falls Prone, laughing.", {{"name", target.source.name}}});
        end_incapacitated_effects(target);
        return;
    case detail::Rider::sleep:
    case detail::Rider::color_spray:
    case detail::Rider::grease:
    case detail::Rider::web:
    case detail::Rider::misty_step:
        return; // Aimed areas, resolved by cast_area().
    case detail::Rider::invisibility:
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::invisible, 0);
        log(target.source.name + " turns Invisible.",
        {"{name} turns Invisible.", {{"name", target.source.name}}});
        return;
    case detail::Rider::see_invisibility:
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
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
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::flame_blade, 0);
        log(target.source.name + " gains Flame Blade.",
        {"{name} gains {spell}.", {{"name", target.source.name}, {"spell", "Flame Blade", true}}});
        return;
    case detail::Rider::vicious_mockery:
        // Disadvantage on its next attack roll before the end of its next turn.
        detail::apply_attack_mastery(target.effects, detail::EffectKind::sap, effect_source(a),
                                     a.source.name, next_save_ms(target.source.id));
        log(target.source.name + " has Disadvantage on its next attack roll.",
        {"{name} has Disadvantage on its next attack roll.", {{"name", target.source.name}}});
        return;
    case detail::Rider::dissonant_whispers:
        // Its Reaction carries it as far from the caster as it can.
        if (!target.reaction || detail::incapacitated(target.effects))
            return;
    {
        target.reaction = false;
        log(target.source.name + " flees from " + a.source.name + ".",
        {"{name} flees from {caster}.", {{"name", target.source.name}, {"caster", a.source.name}}});
        // It moves with its Reaction, so leaving a reach provokes as any move.
        const int budget = std::max(0, def(target).speed - detail::speed_penalty(target.effects));
        auto route = flight_route(a, target, budget);
        if (route.empty() || !path_.empty())
            return;
        flight_ = Flight{target.source.id, budget};
        path_ = std::move(route);
        path_index_ = 0;
        progress_movement();
        return;
    }
    case detail::Rider::starry_wisp:
    {
        const auto index = index_of(a.source.id);
        const unsigned slot = turn_end_ms(index) - (index ? turn_end_ms(index - 1) : 0);
        detail::apply_poisoned(target.effects, effect_source(a), a.source.name,
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
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name, kind, 0);
        log(target.source.name + " gains " + std::string(spell.label) + ".",
        {"{name} gains {spell}.", {{"name", target.source.name}, {"spell", std::string(spell.label), true}}});
        return;
    }
    case detail::Rider::barkskin:
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::barkskin, 0);
        log(target.source.name + " gains Barkskin.",
        {"{name} gains {spell}.", {{"name", target.source.name}, {"spell", "Barkskin", true}}});
        return;
    case detail::Rider::hex:
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::hex, 0);
        log(target.source.name + " is hexed.", {"{name} is hexed.", {{"name", target.source.name}}});
        return;
    case detail::Rider::charm_person:
        // Mindless Rage: a raging Berserker cannot be Charmed.
        if (mindless(target))
        {
            log(target.source.name + "'s Mindless Rage shrugs off the charm.",
            {"{name}'s Mindless Rage shrugs off the charm.", {{"name", target.source.name}}});
            return;
        }
        if (aura_of_devotion(target))
        {
            log(target.source.name + " is immune to the charm (Aura of Devotion).",
            {"{name} is immune to the charm (Aura of Devotion).", {{"name", target.source.name}}});
            return;
        }
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::charmed, 0);
        log(target.source.name + " is Charmed by " + a.source.name + ".",
        {
            "{name} is Charmed by {caster}.",
            {{"name", target.source.name}, {"caster", a.source.name}}
        });
        return;
    case detail::Rider::dragons_breath:
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::dragons_breath,
                                    static_cast<int>(*detail::dragon_type(verb)));
        log(target.source.name + " gains Dragon's Breath.",
        {"{name} gains {spell}.", {{"name", target.source.name}, {"spell", "Dragon's Breath", true}}});
        return;
    case detail::Rider::enlarge_reduce:
    {
        const bool enlarge = verb == "enlarge";
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    enlarge ? detail::EffectKind::enlarged
                                    : detail::EffectKind::reduced, 0);
        log(target.source.name + (enlarge ? " is enlarged." : " is reduced."),
        {enlarge ? "{name} is enlarged." : "{name} is reduced.", {{"name", target.source.name}}});
        return;
    }
    case detail::Rider::blur:
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::blur, 0);
        log(target.source.name + " gains Blur.",
        {"{name} gains {spell}.", {{"name", target.source.name}, {"spell", "Blur", true}}});
        return;
    case detail::Rider::mirror_image:
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
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
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::magic_weapon, 1);
        log(target.source.name + " gains Magic Weapon.",
        {"{name} gains {spell}.", {{"name", target.source.name}, {"spell", "Magic Weapon", true}}});
        return;
    case detail::Rider::acid_arrow:
        detail::apply_poisoned(target.effects, effect_source(a), a.source.name,
                               next_save_ms(target.source.id), detail::EffectKind::acid_arrow);
        return;
    case detail::Rider::ray_of_enfeeblement:
        detail::apply_repeating_condition(
            target.effects, detail::EffectKind::enfeebled, effect_source(a), a.source.name,
            detail::RepeatingSave{.dc = dc, .first_save_ms = next_save_ms(target.source.id)});
        log(target.source.name + " is enfeebled.",
        {"{name} is enfeebled.", {{"name", target.source.name}}});
        return;
    case detail::Rider::hold_person:
        detail::apply_hold_person(
            target.effects, effect_source(a), a.source.name,
            detail::RepeatingSave{.dc = dc, .first_save_ms = next_save_ms(target.source.id)});
        log(target.source.name + " is Paralyzed.",
        {"{name} is Paralyzed.", {{"name", target.source.name}}});
        end_incapacitated_effects(target);
        return;
    case detail::Rider::guiding_bolt:
    {
        // Until the end of the caster's next turn.
        const auto index = index_of(a.source.id);
        const unsigned slot = turn_end_ms(index) - (index ? turn_end_ms(index - 1) : 0);
        detail::apply_guiding_bolt(target.effects, effect_source(a), a.source.name,
                                   next_turn_ms(a) + slot);
        log(target.source.name + " is lit by Guiding Bolt.",
        {"{name} is lit by Guiding Bolt.", {{"name", target.source.name}}});
        return;
    }
    case detail::Rider::bane:
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::bane, 0);
        log(target.source.name + " is weakened by Bane.",
        {"{name} is weakened by Bane.", {{"name", target.source.name}}});
        return;
    case detail::Rider::aid:
    {
        // Aid does not stack with itself; a creature already aided keeps its own.
        if (detail::hit_point_bonus(target.effects) >= 5 || target.life.dead ||
                !detail::can_apply(target.effects))
            return;
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::aid, 5);
        log(target.source.name + " gains Aid.",
        {"{name} gains {spell}.", {{"name", target.source.name}, {"spell", "Aid", true}}});
        heal(target, 5);
        return;
    }
    case detail::Rider::lesser_restoration:
    {
        const auto condition = *detail::restoration_condition(verb);
        std::erase_if(target.effects.active, [&](const auto & e)
        {
            return gives_condition(e.kind, condition);
        });
        const std::string message = condition == "blinded"     ? "{name} is no longer Blinded."
                                     : condition == "paralyzed" ? "{name} is no longer Paralyzed."
                                     : "{name} is no longer Poisoned.";
        log(target.source.name + " is no longer " + condition_name(condition) + ".",
        {message, {{"name", target.source.name}}});
        return;
    }
    case detail::Rider::chill_touch:
    {
        const unsigned slot = turn_end_ms(turn_) - (turn_ ? turn_end_ms(turn_ - 1) : 0);
        detail::apply_chill_touch(target.effects, effect_source(a), a.source.name,
                                  detail::round_ms + slot);
        log(target.source.name + " cannot regain HP until the end of the caster's next turn.",
        {
            "{name} cannot regain HP until the end of the caster's next turn.",
            {{"name", target.source.name}}
        });
        return;
    }
    case detail::Rider::shocking_grasp:
        detail::apply_shocking_grasp(target.effects, effect_source(a), a.source.name,
                                     next_turn_ms(target));
        log(target.source.name + " cannot make Opportunity Attacks until its next turn.",
        {
            "{name} cannot make Opportunity Attacks until its next turn.",
            {{"name", target.source.name}}
        });
        return;
    case detail::Rider::ray_of_frost:
        detail::apply_ray_of_frost(target.effects, effect_source(a), a.source.name,
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
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    rider_effect(spell.rider), value);
        log(target.source.name + " gains " + std::string(spell.label) + ".",
        {"{name} gains {spell}.", {{"name", target.source.name}, {"spell", std::string(spell.label), true}}});
        if (spell.rider == detail::Rider::hunters_mark)
            reveal_lore(a, target);
        return;
    }
    case detail::Rider::command:
    {
        const auto option = detail::command_option(verb).value();
        const auto index = index_of(target.source.id);
        const unsigned slot = turn_end_ms(index) - (index ? turn_end_ms(index - 1) : 0);
        detail::apply_command(target.effects, effect_source(a), a.source.name, option,
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
        detail::apply_blindness(
            target.effects, effect_source(a), a.source.name,
            detail::RepeatingSave{.dc = dc, .first_save_ms = next_save_ms(target.source.id)});
        log(target.source.name + " is Blinded.",
        {"{name} is Blinded.", {{"name", target.source.name}}});
        return;
    }
}

void Session::resolve_spell(const detail::SpellDef &spell, std::string_view verb, Actor &a,
                            EntityId target_id)
{
    end_sanctuary(a);
    const bool was_invisible = invisible(a);
    const auto id = a.source.id;
    cast_resolved_spell(spell, verb, a, std::span<const EntityId>(&target_id, 1));
    if (was_invisible)
        end_invisibility(id);
    end_hiding(id);
}

void Session::missile_hits(Actor &a, Actor &target, const detail::SpellDef &spell,
                           const Dice &rolled, unsigned darts, bool split)
{
    if (sanctuary_stops(a, target))
        return;
    const auto name = std::string(spell.label);
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
    for (unsigned n = 0; n < darts; ++n)
        total += resolved_damage(target, spell.damage, dice(rolled));
    // The plain English keeps its original lower-case damage word so stored
    // combat logs stay byte identical. The template uses the translatable
    // capitalised name, matching the convention in resolved_damage().
    auto lowered = std::string(detail::damage_name(spell.damage));
    for (auto &c : lowered)
        if (c >= 'A' && c <= 'Z')
            c += 32;
    if (split)
        log(a.source.name + " casts " + name + " at " + target.source.name + " for " +
            std::to_string(total) + " " + lowered + " damage.",
        {
            "{name} casts {spell} at {target} for {damage} {type} damage.",
            {   {"name", a.source.name},
                {"spell", name, true},
                {"target", target.source.name},
                {"damage", std::to_string(total)},
                {"type", std::string(detail::damage_name(spell.damage)), true}
            }
        });
    else
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
}

void Session::cast_resolved_spell(const detail::SpellDef &spell, std::string_view verb, Actor &a,
                                  std::span<const EntityId> targets)
{
    const auto target_id = targets.front();
    const auto &d = def(a);
    const bool upcast = verb.ends_with("_2");
    take_metamagic(a, spell);
    if (casting_with(Metamagic::heightened))
        heightened_target_ = target_id;
    mark_heightened(a, spell);
    if (spell.level)
        pay_for_spell(a, verb, spell);
    auto rolled = spell.dice;
    rolled.count += static_cast<int>(upcast ? spell.upcast.extra_dice : 0u);
    if (spell.add_casting_modifier)
        rolled.bonus = casting_bonus(d, spell.id) - 2;
    // Agonizing Blast adds the Charisma modifier to its cantrip's damage.
    if (d.agonizing.contains(spell.id))
        rolled.bonus += d.casting - 2;
    const unsigned instances = spell.instances + (upcast ? spell.upcast.extra_instances : 0u);
    const int dc = spell_dc(a, spell.id);
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
        detail::stabilize(target.life, rng_);
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
        const bool struck =
            attack(a, target, spell.melee ? AttackRange::melee : AttackRange::ranged,
                   SpellAttack{.dice = rolled, .type = type, .bursts = bursts,
                               .spell = spell.id});
        // Repelling Blast pushes a Large or smaller creature 10 feet away.
        if (struck && d.repelling.contains(spell.id) && !target.life.dead &&
                def(target).size <= 3)
            push_away(a, target, 2);
        if (struck)
            apply_rider(spell, verb, a, target, dc);
        else if (spell.rider == detail::Rider::acid_arrow && target.life.hp > 0)
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
        else if (d.evoker && !spell.level && target.life.hp > 0)
            potent_cantrip(target, spell, rolled);
        // Ice Knife's shard explodes, hit or miss.
        if (spell.rider == detail::Rider::ice_knife)
            ice_burst(a, target, upcast);
        return;
    }
    case detail::SpellPattern::repeat_attack:
    {
        // One ray per aimed target, in the order aimed; a single target takes
        // them all. A ray whose target has died turns to the next aimed foe
        // still alive, else to the nearest foe it could have been aimed at,
        // standing ones first (SPLIT-1); with none left it is lost. A downed
        // party member stays a target: each hit is a failed death save.
        std::vector<EntityId> rays(targets.begin(), targets.end());
        if (rays.size() == 1)
            rays.assign(instances, target_id);
        const auto alive = [&](EntityId id)
        {
            return !actor(id).life.dead;
        };
        const auto nearest = [&]() -> EntityId
        {
            const Actor *best = nullptr;
            const auto rank = [&](const Actor & who)
            {
                return std::pair{who.life.hp <= 0, distance(a.source.cell, who.source.cell)};
            };
            for (const auto &other : actors_)
            {
                if (other.life.dead || other.source.side == a.source.side ||
                        distance(a.source.cell, other.source.cell) > spell.range ||
                        (spell.requires_sight ? !can_see(a, other)
                         : !line_of_sight(a.source.cell, other.source.cell)))
                    continue;
                if (!best || rank(other) < rank(*best))
                    best = &other;
            }
            return best ? best->source.id : 0;
        };
        for (std::size_t n = 0; n < rays.size(); ++n)
        {
            auto ray = rays[n];
            if (!alive(ray))
            {
                const auto next = std::find_if(rays.begin() + static_cast<std::ptrdiff_t>(n) + 1,
                                               rays.end(), alive);
                ray = next != rays.end() ? *next : nearest();
                if (!ray)
                    continue;
                log("The ray turns to " + actor(ray).source.name + ".",
                {"The ray turns to {target}.", {{"target", actor(ray).source.name}}});
            }
            attack(a, actor(ray), spell.melee ? AttackRange::melee : AttackRange::ranged,
                   SpellAttack{.dice = rolled, .type = cast_damage_type(spell.damage),
                               .spell = spell.id});
        }
        return;
    }
    case detail::SpellPattern::auto_damage:
    {
        // The darts strike together: each creature takes its darts as one hit.
        std::vector<std::pair<EntityId, unsigned>> struck;
        for (const auto id : targets)
        {
            const auto found = std::find_if(struck.begin(), struck.end(), [&](const auto & entry)
            {
                return entry.first == id;
            });
            if (found != struck.end())
                ++found->second;
            else
                struck.push_back({id, 1});
        }
        if (targets.size() == 1)
            struck.front().second = instances;
        for (const auto &[id, darts] : struck)
            missile_hits(a, actor(id), spell, rolled, darts, struck.size() > 1);
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
        const bool halves = spell.half_on_success || (d.evoker && !spell.level);
        if (saved && !halves)
            return;
        // Halving comes before resistance, which resolved_damage applies.
        const int rolled_damage = spell_dice(a, rolled);
        const auto damage_type = cast_damage_type(spell.damage);
        const auto type = std::string(detail::damage_name(damage_type));
        const int amount = resolved_damage(
                               target, damage_type,
                               evaded(target, spell.save, halves, saved, rolled_damage));
        log(target.source.name + " takes " + std::to_string(amount) + " " + type + " damage.",
        {
            "{name} takes {damage} {type} damage.",
            {   {"name", target.source.name},
                {"damage", std::to_string(amount)},
                {"type", type, true}
            }
        });
        damage(target, amount);
        if (!saved && spell.rider != detail::Rider::none && !target.life.dead)
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
                detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
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
            detail::apply_attack_mastery(target.effects, detail::EffectKind::sap, effect_source(a),
                                         a.source.name, next_turn_ms(a));
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
    if (other.life.dead || rage_of(a))
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
            // Scorching Ray may strike a downed but living foe (the user's call).
            if (other.source.side == a.source.side ||
                    (other.life.hp <= 0 && spell.pattern != detail::SpellPattern::repeat_attack))
                continue;
            break;
        case detail::SpellTarget::wounded_ally:
            if (other.source.side != a.source.side || other.life.hp >= max_hp(other))
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
            if (other.source.side != a.source.side || other.life.hp != 0 || other.life.stable)
                continue;
            break;
        }
        if (feet > spell_range(a, spell))
            continue;
        if (spell.requires_sight && !can_see(a, other))
            continue;
        if (spell.requires_effect_capacity && !detail::can_apply(other.effects))
            continue;
        if (spell.rider == detail::Rider::lesser_restoration && !restorable(other))
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
        // A level-one spell from a level-one or level-two slot, one slot a
        // turn, or with Magic Initiate's free cast, which spends no slot.
        const auto offer_level_one = [&](const std::string & verb, const std::string & label)
        {
            if (!a.spent_slot && a.slots[1] > 0)
                offer(verb, label);
            if (!a.spent_slot && a.slots[2] > 0)
                offer(verb + "_2", label + " (level 2 slot)");
            if (a.initiate_casts & initiate_bit(d, spell.id))
                offer(verb + std::string(detail::initiate_suffix), label + " (Magic Initiate)");
        };
        if (spell.id == "chromatic_orb")
        {
            for (const auto type : detail::chromatic_types)
                offer_level_one("chromatic_orb_" + std::string(type),
                                "Chromatic Orb: " +
                                std::string(detail::damage_name(detail::damage_type(type))));
            continue;
        }
        if (spell.rider == detail::Rider::guidance)
        {
            offer("guidance_athletics", "Guidance: Athletics");
            offer("guidance_medicine", "Guidance: Medicine");
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
        // A level-two spell is only ever cast from a level-two slot, so it has
        // no separate upcast verb.
        if (spell.level >= 2)
        {
            if (a.spent_slot || a.slots[2] <= 0)
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
            // The caster chooses which one condition to end.
            if (spell.rider == detail::Rider::lesser_restoration)
            {
                for (const auto condition : detail::restoration_conditions)
                    if (has_condition(other, condition))
                        offer("lesser_restoration_" + std::string(condition),
                              "Lesser Restoration: " + condition_name(condition));
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
                offer_level_one(verb,
                                "Command: " + command_label(detail::command_option(verb).value()));
            }
            continue;
        }
        offer_level_one(std::string(spell.id), std::string(spell.label));
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
        else if (question.asked == Asked::boon)
            add(who.source.id, "giant_boon", std::string(giant_boon_label(def(who).giant)));
        else if (question.asked == Asked::endure)
            add(who.source.id, "stones_endurance", "Stone's Endurance");
        else if (question.asked == Asked::thunder)
            add(who.source.id, "storms_thunder", "Storm's Thunder");
        else if (question.asked == Asked::retaliate)
            add(who.source.id, "retaliate", "Retaliation attack");
        else
        {
            if (can_shield(who) && !question.critical)
                add(who.source.id, "shield",
                    question.asked == Asked::missile ? "Cast Shield against Magic Missile"
                    : "Cast Shield against the hit");
            if (question.asked == Asked::hit && can_deflect(who, question.deflectable))
                add(who.source.id, "deflect", "Deflect Attacks");
            if (question.asked == Asked::hit && can_dodge(who))
                add(who.source.id, "uncanny_dodge", "Uncanny Dodge");
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
        // An area centered on a square needs a clear line to that square; a
        // cone, line or cube spreads from the caster and is only pointed.
        const bool centered = !spell.cone && !spell.line && !spell.cube;
        const auto aimable = [&](Cell cell)
        {
            return (!open_square || teleport_open(caster, cell)) &&
                   (!centered || line_of_sight(caster.source.cell, cell));
        };
        for (int y = 0; y < board_.height; ++y)
            for (int x = 0; x < board_.width; ++x)
                if (distance(caster.source.cell, Cell{x, y}) <= spell_range(caster, spell) &&
                        aimable(Cell{x, y}))
                    add(caster.source.id, "area_move", std::string(spell.label), 0, Cell{x, y});
        if (aimable(area_->center))
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
        const bool split = splits_among_targets(spell);
        for (const auto &other : actors_)
        {
            if (other.life.dead || distance(caster.source.cell, other.source.cell) > spell.range)
                continue;
            // A dart or ray can only be aimed at a creature the spell could
            // strike alone; another one may be aimed at it again. A ray, not a
            // dart, may be aimed at a downed but living creature.
            if (split && (other.source.side == caster.source.side ||
                          (other.life.hp <= 0 &&
                           spell.pattern != detail::SpellPattern::repeat_attack) ||
                          (spell.requires_sight ? !can_see(caster, other)
                           : !line_of_sight(caster.source.cell, other.source.cell))))
                continue;
            add(caster.source.id, selection_->verb, std::string(spell.label), other.source.id);
        }
        // Every dart or ray is aimed before the spell is cast.
        if (!selection_->chosen.empty() && !split)
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
            if (persistent_ready(a))
                add(id, "persistent_rage", "Persistent Rage: regain all Rage uses");
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
            const auto range =
                command.verb == "ranged" || command.verb == "throw" ||
                command.verb == "light_ranged" || command.verb == "light_throw" ||
                command.verb == "nick_ranged" || command.verb == "nick_throw"
                ? AttackRange::ranged
                : AttackRange::melee;
            if (range == AttackRange::melee && command.verb != "melee" &&
                    command.verb != "opportunity" && command.verb != "light_melee" &&
                    command.verb != "nick_melee")
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
            return !mastery_capacity(attacking, actor(command.target), range);
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
        const auto &mover_cell = actor(champion_move_->actor).source.cell;
        for (const auto cell : movement_reach(champion_move_->actor))
            // Forceful Blow's move closes on its target the whole way.
            if (!champion_move_->toward ||
                    distance(cell, actor(champion_move_->toward).source.cell) ==
                    distance(mover_cell, actor(champion_move_->toward).source.cell) -
                    distance(mover_cell, cell))
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
            add(pending(), "opportunity", "Opportunity attack", mover().source.id);
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
        if (can_hide(a))
            add(id, "cunning_hide", "Cunning Action: Hide");
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
            if (other.source.id == a.source.id || other.life.dead || feet > 30)
                continue;
            if (other.source.side != a.source.side && other.life.hp > 0 &&
                    def(other).creature_type == "undead")
                undead = true;
            const bool wounded_ally = other.source.side == a.source.side &&
                                      other.life.hp < max_hp(other) &&
                                      !detail::healing_blocked(other.effects);
            const bool enemy = other.source.side != a.source.side && other.life.hp > 0;
            if ((wounded_ally || enemy) && can_see(a, other))
                add(id, "divine_spark", "Divine Spark", other.source.id);
        }
        if (undead)
            add(id, "turn_undead", "Turn Undead", id);
        // Preserve Life (Life Domain) needs a Bloodied ally within 30 feet.
        if (d.life_domain && std::any_of(actors_.begin(), actors_.end(), [&](const auto & other)
    {
        return other.source.side == a.source.side && !other.life.dead &&
                   other.life.hp * 2 <= max_hp(other) &&
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
                     detail::has_effect(other.effects, detail::EffectKind::asleep) ||
                     detail::has_effect(other.effects, detail::EffectKind::knocked_out)))
                add(id, "shake_awake", "Shake awake", other.source.id);
    // Martial Arts: an Unarmed Strike as a Bonus Action.
    if (a.bonus && d.martial_arts)
        for (const auto &other : actors_)
            if (other.source.side != a.source.side && other.life.hp > 0 && !other.life.dead &&
                    distance(a.source.cell, other.source.cell) <= 5)
                add(id, "martial_arts", "Unarmed Strike", other.source.id);
    // Font of Magic: a Bonus Action turns Sorcery Points into a slot; a slot
    // becomes Sorcery Points without an action.
    if (d.sorcery_points)
    {
        if (a.bonus && a.lay_on_hands >= 2 && a.slots[1] < slot_room(d, 1))
            add(id, "create_slot_1", "Font of Magic: create a level-1 slot (2 Sorcery Points)");
        if (a.bonus && d.level >= 3 && a.lay_on_hands >= 3 && a.slots[2] < slot_room(d, 2))
            add(id, "create_slot_2", "Font of Magic: create a level-2 slot (3 Sorcery Points)");
        if (a.slots[1] > 0 && a.lay_on_hands < d.sorcery_points)
            add(id, "convert_slot_1", "Font of Magic: a level-1 slot into 1 Sorcery Point");
        if (a.slots[2] > 0 && a.lay_on_hands < d.sorcery_points)
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
                const auto index = static_cast<std::size_t>(
                    std::find(metamagic_ids.begin(), metamagic_ids.end(), known) -
                    metamagic_ids.begin());
                const auto option = static_cast<Metamagic>(index);
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
            if (other.source.side == a.source.side && other.source.id != id && !other.life.dead &&
                    distance(a.source.cell, other.source.cell) <= 60 &&
                    !detail::has_effect(other.effects, detail::EffectKind::inspired))
                add(id, "bardic_inspiration", "Bardic Inspiration", other.source.id);
    // Innate Sorcery: a Bonus Action, twice per Long Rest.
    if (a.bonus && d.innate_sorcery && a.free_casts > 0 &&
            !detail::has_effect(a.effects, detail::EffectKind::innate_sorcery))
        add(id, "innate_sorcery", "Innate Sorcery");
    // Monk's Focus: Patient Defense and Step of the Wind, free or with a Focus
    // Point, and Flurry of Blows for a Focus Point.
    // Heightened Focus: Step of the Wind with Focus may carry an ally beside
    // the Monk, Large or smaller, standing or not.
    const auto offer_carry = [&]
    {
        if (!d.heightened_focus || a.surges <= 0)
            return;
        for (const auto &ally : actors_)
            if (ally.source.side == a.source.side && ally.source.id != id && !ally.life.dead &&
                    def(ally).size <= 3 && distance(a.source.cell, ally.source.cell) <= 5)
                add(id, "step_of_the_wind_carry",
                    "Step of the Wind: Disengage and Dash, carrying an ally (1 Focus)",
                    ally.source.id);
    };
    // Fleet Step: Step of the Wind right after another Bonus Action.
    if (!a.bonus && a.fleet_step)
    {
        add(id, "step_of_the_wind", "Step of the Wind: Dash");
        if (a.surges > 0)
            add(id, "step_of_the_wind_focus", "Step of the Wind: Disengage and Dash (1 Focus)");
        offer_carry();
    }
    if (a.bonus && d.focus)
    {
        add(id, "patient_defense", "Patient Defense: Disengage");
        add(id, "step_of_the_wind", "Step of the Wind: Dash");
        offer_carry();
        if (a.surges > 0)
        {
            add(id, "patient_defense_focus", "Patient Defense: Disengage and Dodge (1 Focus)");
            add(id, "step_of_the_wind_focus", "Step of the Wind: Disengage and Dash (1 Focus)");
            for (const auto &other : actors_)
                if (other.source.side != a.source.side && other.life.hp > 0 && !other.life.dead &&
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
                    // Stunning Strike on a Flurry hit costs one more Focus Point.
                    if (d.stunning_strike && d.martial_arts && !a.stun_used && a.surges > 1)
                        add(id, "flurry_stun", "Flurry of Blows: Stunning Strike", other.source.id);
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
        else if (rage && !d.persistent_rage && rage->remaining_ms <= detail::round_ms)
            add(id, "extend_rage", "Extend Rage");
    }
    if (a.bonus && d.intimidating_presence)
    {
        if (a.free_casts > 0)
            add(id, "intimidating_presence", "Intimidating Presence");
        else if (a.channel_divinity > 0)
            add(id, "intimidating_presence_rage", "Intimidating Presence (1 Rage)");
    }
    // Expeditious Retreat: Dash as a Bonus Action while it lasts.
    if (a.bonus && detail::has_effect(a.effects, detail::EffectKind::expeditious_retreat))
        add(id, "retreat_dash", "Expeditious Retreat: Dash");
    // Flaming Sphere: a Bonus Action rolls it up to 30 feet into a creature's
    // space, where it stops beside the creature and burns it.
    for (const auto &zone : zones_)
        if (zone.kind == ZoneKind::flaming_sphere && zone.caster == id && a.bonus)
            for (const auto &other : actors_)
                if (other.life.hp > 0 && !other.life.dead && other.source.id != id &&
                        distance(zone.cells.front(), other.source.cell) <= 30)
                    add(id, "roll_flaming_sphere", "Roll Flaming Sphere", other.source.id);
    // Heat Metal: a Bonus Action on later turns heats the metal again.
    if (a.bonus)
        for (const auto &other : actors_)
            if (!other.life.dead && other.life.hp > 0 &&
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
                if (other.life.hp > 0 && !other.life.dead && !in_zone(zone, other.source.cell) &&
                        distance(zone.cells.front(), other.source.cell) <= 60)
                    add(id, "move_moonbeam", "Move Moonbeam", other.source.id);
    // Spiritual Weapon: on later turns a Bonus Action moves the force up to 20
    // feet and attacks a creature within 5 feet of it.
    if (const auto *force = spiritual_weapon(a); force && a.bonus)
        for (const auto &other : actors_)
            if (other.source.side != a.source.side && other.life.hp > 0 && !other.life.dead &&
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
                    other.life.hp > 0 && distance(origin.source.cell, other.source.cell) <= 5 &&
                    (feet <= d.reach || (d.range > 0 && feet <= d.long_range)))
                add(id, "horde_breaker", "Horde Breaker", other.source.id);
        }
    }
    // Favored Enemy casts Hunter's Mark without a slot, while no living quarry
    // carries the mark already; a dropped quarry's mark moves for a Bonus Action.
    const bool marking = std::any_of(actors_.begin(), actors_.end(), [&](const auto & other)
    {
        return other.life.hp > 0 && !other.life.dead && marked_by(other, a);
    });
    const bool free_mark = a.bonus && a.free_casts > 0 && d.favored_enemy && !marking &&
                           detail::knows_spell(d.spells, "hunters_mark") && !d.str_dex_disadvantage;
    const bool moving_mark = a.bonus && mark_can_move(a);
    // Hex moves to a new creature as a Bonus Action once its first drops.
    if (a.bonus && hex_can_move(a))
        for (const auto &other : actors_)
            if (other.source.side != a.source.side && other.life.hp > 0 && !other.life.dead &&
                    distance(a.source.cell, other.source.cell) <= 90 && can_see(a, other))
                add(id, "hex_move", "Move Hex", other.source.id);
    if (free_mark || moving_mark)
        for (const auto &other : actors_)
            if (other.source.side != a.source.side && other.life.hp > 0 && !other.life.dead &&
                    distance(a.source.cell, other.source.cell) <= 90 && can_see(a, other) &&
                    !marked_by(other, a))
            {
                if (free_mark)
                    add(id, "hunters_mark_free", "Hunter's Mark (Favored Enemy)", other.source.id);
                if (moving_mark)
                    add(id, "hunters_mark_move", "Move Hunter's Mark", other.source.id);
            }
    // Sacred Weapon comes with the Attack action, so it is offered while the
    // Action is unspent and costs only a Channel Divinity use. The player picks
    // the weapon's own damage or Radiant when imbuing it (SACRED-1).
    if (d.sacred_weapon && a.channel_divinity > 0 && a.actions.available() && d.melee.count &&
            !detail::has_effect(a.effects, detail::EffectKind::sacred_weapon))
    {
        add(id, "sacred_weapon", "Sacred Weapon", id);
        add(id, "sacred_weapon_radiant", "Sacred Weapon (Radiant)", id);
    }
    if (a.bonus && d.aggressive && speed > 0 && enemy_in_sight(a))
        add(id, "aggressive", "Aggressive");
    if (a.bonus && a.winds > 0 && a.life.hp < max_hp(a))
        add(id, "second_wind", "Second Wind", id);
    // Wholeness of Body: a Bonus Action heals the Martial Arts die + Wisdom.
    if (a.bonus && d.wholeness && a.free_casts > 0 && a.life.hp < max_hp(a))
        add(id, "wholeness_of_body", "Wholeness of Body", id);
    // Lay On Hands: touch yourself or an adjacent wounded ally whose healing
    // can take effect, including one at 0 Hit Points.
    if (a.bonus && d.lay_on_hands && a.lay_on_hands > 0)
        for (const auto &other : actors_)
            if (other.source.side == a.source.side && !other.life.dead &&
                    other.life.hp < max_hp(other) && !detail::healing_blocked(other.effects) &&
                    distance(a.source.cell, other.source.cell) <= 5)
                add(id, "lay_on_hands", "Lay On Hands", other.source.id);
    // Lay On Hands can instead spend 5 points to end Poisoned (SRD 5.2.1 p. 61).
    if (a.bonus && d.lay_on_hands && a.lay_on_hands >= 5)
        for (const auto &other : actors_)
            if (other.source.side == a.source.side && !other.life.dead &&
                    has_condition(other, "poisoned") &&
                    distance(a.source.cell, other.source.cell) <= 5)
                add(id, "lay_on_hands_cure", "Lay On Hands: cure Poisoned", other.source.id);
    // Cloud's Jaunt: a Bonus Action teleport, aimed like Misty Step.
    if (a.bonus && d.giant == GiantBoon::cloud && trait_uses_left(a) > 0)
        commands.push_back({revision_, id, 0, "clouds_jaunt", "Cloud's Jaunt", Cell{}, 0, true});
    // Smites, right after this actor's own melee hit on a creature still standing.
    if (a.bonus && a.smite_target && actor(a.smite_target).life.hp > 0 && !d.spells.empty())
    {
        const bool slot = a.slots[1] > 0 && !a.spent_slot;
        const bool slot2 = a.slots[2] > 0 && !a.spent_slot;
        const bool melee = a.smite_melee;
        if (melee && detail::knows_spell(d.spells, "divine_smite") && a.free_casts > 0)
            add(id, "divine_smite_free", "Divine Smite (Paladin's Smite)", a.smite_target);
        if (melee && detail::knows_spell(d.spells, "divine_smite") && slot)
            add(id, "divine_smite", "Divine Smite", a.smite_target);
        if (melee && detail::knows_spell(d.spells, "divine_smite") && slot2)
            add(id, "divine_smite_2", "Divine Smite (level 2 slot)", a.smite_target);
        if (melee && detail::knows_spell(d.spells, "searing_smite") && slot)
            add(id, "searing_smite", "Searing Smite", a.smite_target);
        if (melee && detail::knows_spell(d.spells, "shining_smite") && slot2)
            add(id, "shining_smite", "Shining Smite", a.smite_target);
        if (detail::knows_spell(d.spells, "ensnaring_strike") && slot)
            add(id, "ensnaring_strike", "Ensnaring Strike", a.smite_target);
    }
    for (const auto &other : actors_)
    {
        const int feet = distance(a.source.cell, other.source.cell);
        for (const auto scope :
                {
                    detail::SpellTarget::wounded_ally, detail::SpellTarget::ally,
                    detail::SpellTarget::self, detail::SpellTarget::enemy
                })
            offer_spells(commands, a, other, feet, scope, true);
        // A spell aimed at any creature, such as a Quickened Poison Spray, needs
        // the same clear line as in the Action pass.
        if (line_of_sight(a.source.cell, other.source.cell))
            offer_spells(commands, a, other, feet, detail::SpellTarget::any_creature, true);

    }
    offer_spells(commands, a, a, 0, detail::SpellTarget::area, true);
    if (!a.light_extra && !a.light_origins.empty())
        for (const auto &item : items_)
            if (item.holder == id && light_eligible(a, item.id))
            {
                const auto *weapon = detail::weapon(item.definition);
                for (const auto &other : actors_)
                    if (other.source.side != a.source.side && !other.life.dead && other.life.hp > 0 &&
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
                        if (!item.stowed && !weapon->ranged && feet <= melee_reach)
                            offer("light_melee", "nick_melee");
                        if (!item.stowed && weapon->ranged && feet <= weapon->long_range)
                            offer("light_ranged", "nick_ranged");
                        if (weapon->thrown && feet <= weapon->long_range)
                            offer("light_throw", "nick_throw");
                    }
            }
    // Cunning Strike: offered when the attack would deal Sneak Attack damage.
    const auto offer_cunning = [&](AttackRange range, bool in_reach, const Actor & other)
    {
        if (!d.cunning_strike || a.sneak_used || !in_reach ||
                !sneak_eligible(a, other, range,
                                attack_modifiers(a, other, range, AttackSource::weapon).mode()))
            return;
        const std::string base = range == AttackRange::melee ? "melee" : "ranged";
        const std::string label = range == AttackRange::melee ? "Melee attack (" : "Ranged attack (";
        const int dice = detail::sneak_attack_dice(d.sneak_level).count;
        const auto usable = [&](const CunningOption & option)
        {
            const bool devious = option.kind == CunningStrike::daze ||
                                 option.kind == CunningStrike::knock_out ||
                                 option.kind == CunningStrike::obscure;
            return option.cost <= dice && (!devious || d.devious_strikes) &&
                   (option.kind != CunningStrike::stealth || (d.supreme_sneak && hidden(a)));
        };
        for (std::size_t i = 0; i < cunning_strikes.size(); ++i)
        {
            const auto &first = cunning_strikes[i];
            if (!usable(first))
                continue;
            add(id, base + "_" + std::string(first.suffix),
                label + std::string(first.name) + ")", other.source.id);
            // Improved Cunning Strike: a second effect the dice can also pay for.
            for (std::size_t j = i + 1; d.improved_cunning && j < cunning_strikes.size(); ++j)
                if (const auto &second = cunning_strikes[j];
                        usable(second) && first.cost + second.cost <= dice)
                    add(id, base + "_" + std::string(first.suffix) + "+" + std::string(second.suffix),
                        label + std::string(first.name) + ", " + std::string(second.name) + ")",
                        other.source.id);
        }
    };
    // Tactical Master: the attack with Push, Sap or Slow, other than the
    // weapon's own mastery.
    const auto offer_tactical = [&](AttackRange range, bool in_reach, const Actor & other)
    {
        const auto own = weapon_mastery(a, range);
        if (!d.tactical_master || !in_reach || own == detail::Mastery::none)
            return;
        const bool melee = range == AttackRange::melee;
        for (const auto &option : tactical_masteries)
            if (option.kind != own)
                add(id, std::string(melee ? "melee_" : "ranged_") + std::string(option.suffix),
                    std::string(melee ? option.melee_label : option.ranged_label), other.source.id);
    };
    // The Attack action's attacks on one foe, with the Magic actions that
    // attack (which need the Action itself); Extra Attack offers the weapon
    // attacks again after the action's first attack.
    const auto offer_attacks = [&](const Actor & other, int feet)
    {
        // A Wild Shape form cannot wield its equipment (SRD 5.2.1), so it
        // draws no Torch or bow and throws no flask.
        const bool wields_equipment = !a.form;
        // A carried Torch is drawn as part of the attack (SRD 5.2.1: a weapon
        // can be equipped with each attack of the Attack action).
        if (wields_equipment && feet <= 5 && carries(a, "torch"))
            add(id, "torch", "Torch attack", other.source.id);
        // A carried bow or crossbow is drawn as part of the attack, too.
        if (const auto bow = wields_equipment ? carried_ranged_weapon(a) : std::string_view{};
                !bow.empty() && feet <= detail::weapon(bow)->long_range)
            add(id, "shoot", "Shoot", other.source.id);
        for (const auto &gear : detail::thrown_gear_items)
            if (wields_equipment && feet <= detail::thrown_gear_range &&
                    a.thrown_gear_left[static_cast<std::size_t>(gear.effect)])
                add(id, "throw_" + std::string(gear.key), "Throw " + std::string(gear.label),
                    other.source.id);
        if (wields_equipment && physical_inventory_)
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
        // Pact of the Blade: the pact weapon may deal Necrotic, Psychic
        // or Radiant damage instead of its own type.
        if (feet <= d.reach && d.pact_of_the_blade && d.melee.count && !d.ranged_weapon &&
                !d.weapon_label.empty())
        {
            add(id, "melee_necrotic", "Melee attack (Necrotic)", other.source.id);
            add(id, "melee_psychic", "Melee attack (Psychic)", other.source.id);
            add(id, "melee_radiant", "Melee attack (Radiant)", other.source.id);
        }
        offer_tactical(AttackRange::melee, feet <= d.reach, other);
        offer_cunning(AttackRange::melee, feet <= d.reach, other);
        // Stunning Strike: a Monk weapon or Unarmed Strike, once a turn, 1 Focus on a hit.
        if (feet <= d.reach && d.stunning_strike && d.martial_arts && !a.stun_used && a.surges > 0)
            add(id, "melee_stun", "Melee attack (Stunning Strike)", other.source.id);
        // Flame Blade: a melee spell attack with the blade as an Action.
        if (feet <= 5 && a.actions.available(true) &&
                detail::has_effect(a.effects, detail::EffectKind::flame_blade))
            add(id, "flame_blade_strike", "Flame Blade attack", other.source.id);
        // Produce Flame: the flame in hand is hurled as an Action.
        if (feet <= 60 && a.actions.available(true) &&
                detail::has_effect(a.effects, detail::EffectKind::produce_flame))
            add(id, "hurl_flame", "Hurl flame (Produce Flame)", other.source.id);
        // Reckless Attack: chosen with the turn's first attack roll, which
        // is the Attack action's first attack.
        if (feet <= d.reach && d.reckless && !a.attacked && strength_attack(a, AttackRange::melee))
            add(id, "reckless", "Reckless attack", other.source.id);
        // Brutal Strike: after Reckless Attack, once a turn, one Strength attack
        // gives up its Advantage for 1d10 more damage and a blow.
        if (feet <= d.reach && d.brutal_strike && !a.brutal_used &&
                detail::has_effect(a.effects, detail::EffectKind::reckless) &&
                strength_attack(a, AttackRange::melee))
        {
            for (const auto &[verb, label, blow] : brutal_blows)
                if (d.improved_brutal || blow == BrutalBlow::forceful ||
                        blow == BrutalBlow::hamstring)
                    add(id, std::string(verb), std::string(label), other.source.id);
        }
        // True Strike: the cantrip's attack with the melee weapon in hand.
        if (feet <= d.reach && d.melee.count && !d.ranged_weapon &&
                detail::knows_spell(d.spells, "true_strike") && a.actions.available(true))
        {
            add(id, "true_strike", "True Strike", other.source.id);
            add(id, "true_strike_radiant", "True Strike (Radiant)", other.source.id);
        }
        if (d.range > 0 && feet <= d.long_range)
        {
            add(id, "ranged", "Ranged attack", other.source.id);
            offer_tactical(AttackRange::ranged, true, other);
            offer_cunning(AttackRange::ranged, true, other);
        }
    };
    // Extra Attack: the Attack action's later attacks, once its first is made.
    if (!a.actions.available() && a.actions.extra_attacks > 0)
        for (const auto &other : actors_)
            if (!other.life.dead && other.source.side != a.source.side &&
                    (other.life.hp > 0 || may_rise(other)) &&
                    line_of_sight(a.source.cell, other.source.cell))
                offer_attacks(other, distance(a.source.cell, other.source.cell));
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
                    if (detail::damage_type(type) == static_cast<detail::DamageType>(effect.dc))
                        commands.push_back({revision_, id, 0,
                                            "dragons_breath_exhale_" + std::string(type),
                                            "Exhale (Dragon's Breath)", Cell{}, 0, true});
        // Breath Weapon takes the place of the Attack action's one attack.
        if (def(a).breath && trait_uses_left(a) > 0 && a.actions.available())
        {
            commands.push_back({revision_, id, 0, "breath_weapon_cone", "Breath Weapon (cone)",
                                Cell{}, 0, true});
            commands.push_back({revision_, id, 0, "breath_weapon_line", "Breath Weapon (line)",
                                Cell{}, 0, true});
        }
        add(id, "dash", "Dash");
        add(id, "dodge", "Dodge");
        if (can_hide(a))
            add(id, "hide", "Hide");
        if (std::any_of(actors_.begin(), actors_.end(), [&](const auto & other)
    {
        return other.source.side != a.source.side && hidden(other);
        }))
        add(id, "search", "Search");
        if (shield_blocks_bow(a))
            add(id, "doff_shield", "Take off shield");
        if (a.burning)
            add(id, "extinguish", "Put out the fire");
        add(id, "disengage", "Disengage");
        for (const auto &other : actors_)
        {
            if (other.life.dead || !line_of_sight(a.source.cell, other.source.cell))
                continue;
            const int feet = distance(a.source.cell, other.source.cell);
            if (!a.source.character_profile.empty() && other.life.hp == 0 && !other.life.stable &&
                    !may_rise(other) && feet <= 5)
                add(id, "stabilize", "Stabilize", other.source.id);
            offer_spells(commands, a, other, feet, detail::SpellTarget::any_creature, false);
            if (other.source.side != a.source.side && (other.life.hp > 0 || may_rise(other)))
            {
                offer_attacks(other, feet);
                offer_spells(commands, a, other, feet, detail::SpellTarget::enemy, false);
            }
            // A downed foe is struck only by Scorching Ray (the user's call);
            // the spell offers filter out everything else.
            else if (other.source.side != a.source.side)
                offer_spells(commands, a, other, feet, detail::SpellTarget::enemy, false);
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
    if (can_run_off(a))
        for (const auto step : off_field_steps(a))
            add(id, "flee", "Flee", 0, step);
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
    if (caster == actors_.end() || caster->life.dead)
        return;
    const bool approach = command->dc == static_cast<int>(detail::CommandOption::approach);
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
        return e.kind == detail::EffectKind::resistance && e.dc == static_cast<int>(type);
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
    // Oil: Fire damage on an oiled creature deals 5 more, once, from the burning oil.
    if (type == detail::DamageType::fire && amount > 0 &&
            target.oiled_until_round >= static_cast<int>(round_))
    {
        target.oiled_until_round = 0;
        amount += 5;
        log("The oil on " + target.source.name + " burns for 5 more damage.",
        {"The oil on {name} burns for 5 more damage.", {{"name", target.source.name}}});
    }
    const std::array parts{detail::DamagePart{type, amount}};
    const auto result = detail::resolve_damage(parts, affinities(target));
    if (def(target).regeneration && result.total > 0 &&
            (type == detail::DamageType::acid || type == detail::DamageType::fire))
        target.regeneration_blocked = true;
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
    if (!amount || target.life.dead)
        return;
    amount = endured(target, amount);
    if (!amount)
        return;
    const bool standing = target.life.hp > 0;
    const auto source = pending() ? pending() : actors_[turn_].source.id;
    if (standing && source != target.source.id)
        damage_taken_.push_back({target.source.id, source});
    const detail::DamageOptions options{
        .critical = critical,
        .dies_at_zero = target.source.side == Side::opposition && !def(target).regeneration};
    detail::damage_life(target.life, amount, max_hp(target), options);
    // Regeneration: damage never kills it and it makes no death saves; the start
    // of each of its turns decides (regenerate). As in the original game, it may
    // get back up 3d6 rounds after it falls.
    if (def(target).regeneration && target.life.hp == 0)
    {
        target.life.dead = false;
        target.life.successes = target.life.failures = 0;
        target.life.recovery = {detail::death_turn_ms, 0};
        if (standing)
            target.rises_on_round = static_cast<int>(round_) + dice({3, 6, 0});
    }
    // Relentless Rage: a raging Barbarian dropped to 0 Hit Points but not
    // killed outright makes a Constitution save, DC 10 and 5 more for each
    // earlier try since its last rest; success leaves twice its level in Hit
    // Points. It is always tried, as Relentless Endurance is.
    if (standing && target.life.hp == 0 && !target.life.dead && def(target).relentless_rage &&
            target.lay_on_hands > 0 && rage_of(target))
    {
        const int dc = 10 + 5 * (def(target).relentless_rage - target.lay_on_hands);
        --target.lay_on_hands;
        log(target.source.name + " tries Relentless Rage.",
        {"{name} tries Relentless Rage.", {{"name", target.source.name}}});
        if (saving_throw_succeeds(target, detail::Ability::constitution, dc))
        {
            (void)detail::heal_life(target.life, 2 * def(target).level, max_hp(target), true);
            log(target.source.name + " fights on with " + std::to_string(target.life.hp) +
                " Hit Points (Relentless Rage).",
            {
                "{name} fights on with {hp} Hit Points (Relentless Rage).",
                {{"name", target.source.name}, {"hp", std::to_string(target.life.hp)}}
            });
        }
    }
    // Relentless Endurance: an Orc dropped to 0 Hit Points but not killed
    // outright drops to 1 instead, once per Long Rest (always taken, AUTO-1).
    if (standing && target.life.hp == 0 && !target.life.dead && def(target).relentless &&
            !detail::has_effect(target.effects, detail::EffectKind::relentless_used) &&
            detail::can_apply(target.effects))
    {
        // Not healing (Chill Touch cannot stop it); the dying clock is cleared.
        (void)detail::heal_life(target.life, 1, max_hp(target), true);
        detail::apply_spell_benefit(target.effects, effect_source(target), target.source.name,
                                    detail::EffectKind::relentless_used, 0);
        log(target.source.name + " endures: Relentless Endurance leaves 1 Hit Point.",
        {
            "{name} endures: Relentless Endurance leaves 1 Hit Point.",
            {{"name", target.source.name}}
        });
    }
    if (standing && target.life.hp == 0)
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
    if (target.life.hp == 0)
        for (auto &other : actors_)
            std::erase_if(other.effects.active, [&](const auto & e)
        {
            return e.kind == detail::EffectKind::warding_bond && e.source_scope == scope_ &&
                   e.source_actor == target.source.id;
        });
    // Damage tests Concentration; dropping to 0 Hit Points ends it.
    if (target.concentration.active())
    {
        const bool rolls = target.life.hp > 0 && !target.life.dead;
        auto modifiers = detail::saving_modifiers(detail::Ability::constitution,
                         def(target).str_dex_disadvantage, target.dodge);
        modifiers.advantage |= detail::has_effect(target.effects, detail::EffectKind::extended) ||
                               def(target).eldritch_mind;
        const auto result = target.concentration.damage(
                                amount,
                                def(target).saves[2] + aura_of_protection(target) +
                                (rolls ? blessing_die(target) : 0),
                                modifiers, target.life.hp == 0 || target.life.dead, rng_,
                                target.effects.active);
        if (result.save)
            log_save(target, *result.save);
        if (result.inspiration)
        {
            detail::spend_inspiration(target.effects);
            log_inspiration(target, result.inspiration, result.save->success);
        }
        if (result.ended)
            drop_concentration_effects(target);
    }
    if (target.life.hp == 0)
    {
        target.effects.prone = true;
        end_wild_shape(target);
        end_incapacitated_effects(target);
    }
    // Damage ends Turn Undead, Sleep and Charm Person on the creature.
    // Adaptation: any damage ends the charm, not only the charmer's side's.
    std::erase_if(target.effects.active, [](const auto & e)
    {
        return e.kind == detail::EffectKind::turned || e.kind == detail::EffectKind::drowsy ||
               e.kind == detail::EffectKind::asleep || e.kind == detail::EffectKind::charmed ||
               e.kind == detail::EffectKind::knocked_out;
    });
    // Hideous Laughter: damage calls for a new Wisdom save, with Advantage.
    for (std::size_t n = 0; n < target.effects.active.size() && target.life.hp > 0; ++n)
        if (target.effects.active[n].kind == detail::EffectKind::laughing &&
                saving_throw_succeeds(target, detail::Ability::wisdom, target.effects.active[n].dc,
                                      true,
                                      detail::heightened_against(
                                          target.effects, target.effects.active[n].source_scope,
                                          target.effects.active[n].source_actor,
                                          "hideous_laughter")))
        {
            target.effects.active.erase(target.effects.active.begin() +
                                        static_cast<std::ptrdiff_t>(n));
            log(target.source.name + " stops laughing.",
            {"{name} stops laughing.", {{"name", target.source.name}}});
            break;
        }
    // Sacred Weapon ends when its wielder is Incapacitated.
    if (target.life.hp == 0)
        std::erase_if(target.effects.active, [](const auto & e)
    {
        return e.kind == detail::EffectKind::sacred_weapon;
    });
    if (target.life.hp == 0 && !target.life.dead && !target.life.stable)
        target.life.recovery.death_save_in_ms = next_turn_ms(target);
    if (target.life.hp == 0)
    {
        target.dodge = false;
        if (!target.life.dead && shares_occupied_space(target))
            target.involuntary_overlap = true;
        log(target.source.name + (target.life.dead ? " is defeated." : " falls unconscious."),
        {
            target.life.dead ? "{name} is defeated." : "{name} falls unconscious.",
            {{"name", target.source.name}}
        });
    }
    clear_departed_overlaps();
}

int Session::heal(Actor &target, int amount)
{
    if (target.life.hp == 0 && shares_occupied_space(target))
        target.involuntary_overlap = true;
    const bool was_unconscious = target.life.hp == 0;
    const int restored =
        detail::heal_life(target.life, amount, max_hp(target), !detail::healing_blocked(target.effects));
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
    const bool searing = verb == "searing_smite", shining = verb == "shining_smite";
    if (sanctuary_stops(a, actor(a.smite_target)))
    {
        a.smite_target = 0;
        a.bonus = false;
        return;
    }
    const auto &spell = *detail::find_spell(searing ? "searing_smite"
                                            : shining ? "shining_smite"
                                            : "divine_smite");
    auto &target = actor(a.smite_target);
    a.smite_target = 0;
    a.bonus = false;
    const bool level_two = shining || verb == "divine_smite_2";
    if (verb == "divine_smite_free")
        --a.free_casts;
    else
    {
        --a.slots[level_two ? 2 : 1];
        a.spent_slot = true;
    }
    auto rolled = spell.dice;
    // A level-2 slot adds a die to Divine Smite.
    if (verb == "divine_smite_2")
        rolled.count += spell.upcast.extra_dice;
    // Divine Smite deals 1d8 more to a Fiend or an Undead.
    const auto &type = def(target).creature_type;
    if (!searing && !shining && (type == "fiend" || type == "undead"))
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
    if (searing && target.life.hp > 0 && detail::can_apply(target.effects))
        detail::apply_searing_smite(target.effects, effect_source(a), a.source.name,
                                    spell_dc(a));
    if (shining && target.life.hp > 0 && detail::can_apply(target.effects))
    {
        begin_concentration(a, spell);
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::shining, 0);
        log(target.source.name + " shines.", {"{name} shines.", {{"name", target.source.name}}});
    }
}

std::vector<Cell> Session::area_cells(const detail::SpellDef &spell, std::string_view verb,
                                     Cell origin, Cell center) const
{
    // An area reaches only the squares with a clear line from its point of
    // origin: the caster for a cone, line or cube, the aimed square otherwise.
    const Cell point = spell.cone || spell.line || spell.cube ? origin : center;
    auto cells = area_shape(spell, verb, origin, center);
    std::erase_if(cells, [&](Cell cell)
    {
        return !line_of_sight(point, cell);
    });
    return cells;
}

std::vector<Cell> Session::area_shape(const detail::SpellDef &spell, std::string_view verb,
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
    // A line two squares wide, or one, from the caster toward the aim.
    if (spell.line)
    {
        const double length = std::sqrt(ax * ax + ay * ay);
        const double right_edge = spell.narrow_line ? 0.5 : 1.0;
        for (int y = 0; y < board_.height; ++y)
            for (int x = 0; x < board_.width; ++x)
            {
                const double vx = x - origin.x, vy = y - origin.y;
                const double along = (vx * ax + vy * ay) / length;
                const double across = (vx * ay - vy * ax) / length;
                if (along > 0 && along * 5 <= spell.line && across > -0.5 && across <= right_edge)
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
    return cell != caster.source.cell && board_.at(cell) != Terrain::obstacle &&
           line_of_sight(caster.source.cell, cell) && !obscured(caster, cell) &&
           std::none_of(actors_.begin(), actors_.end(), [&](const auto & other)
    {
        return !other.life.dead && other.source.cell == cell;
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
            if (other.source.side != caster.source.side && other.life.hp > 0 && !other.life.dead &&
                    (!nearest || distance(caster.source.cell, other.source.cell) <
                     distance(caster.source.cell, nearest->source.cell)))
                nearest = &other;
        return nearest ? spectral_cell(*nearest, caster.source.cell) : caster.source.cell;
    }
    // The preview starts on the nearest living enemy in range and in clear
    // line, else the caster.
    Cell best = caster.source.cell;
    int best_feet = spell.range + 1;
    for (const auto &other : actors_)
    {
        const int feet = distance(caster.source.cell, other.source.cell);
        if (other.source.side != caster.source.side && other.life.hp > 0 && feet < best_feet &&
                line_of_sight(caster.source.cell, other.source.cell))
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
                other.source.id != caster.source.id && !other.life.dead && can_see(caster, other) &&
                std::find(cells.begin(), cells.end(), other.source.cell) != cells.end())
            spared.push_back(other.source.id);
    return spared;
}

int Session::area_dc(const Actor &caster, const detail::SpellDef &spell) const
{
    // Breath Weapon: 8 + the Constitution modifier + the Proficiency Bonus.
    if (spell.id.starts_with("breath_weapon_"))
        return 8 + def(caster).constitution + 2 + (def(caster).level - 1) / 4;
    // A breath given by Dragon's Breath uses the spell caster's save DC.
    if (spell.id == "dragons_breath_exhale")
        for (const auto &effect : caster.effects.active)
            if (effect.kind == detail::EffectKind::dragons_breath)
                return spell_dc(actor(effect.source_actor));
    return spell_dc(caster, spell.id);
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
    if (def(caster).agonizing.contains(spell.id))
        rolled.bonus += def(caster).casting - 2;
    const int total = spell_dice(caster, rolled);
    const auto damage_type = spell.id.starts_with("breath_weapon_")
                             ? *def(caster).breath
                             : cast_damage_type(detail::dragon_type(verb).value_or(spell.damage));
    const auto type = std::string(detail::damage_name(damage_type));
    // Careful Spell: the chosen allies succeed and so take no half damage.
    const auto careful = careful_allies(caster, cells);
    for (auto &other : actors_)
    {
        if (other.life.dead || other.source.id == caster.source.id ||
                std::find(cells.begin(), cells.end(), other.source.cell) == cells.end() ||
                std::find(spared.begin(), spared.end(), other.source.id) != spared.end() ||
                std::find(careful.begin(), careful.end(), other.source.id) != careful.end())
            continue;
        const bool saved = saving_throw_succeeds(other, spell.save, dc);
        // Potent Cantrip: an Evoker's saved-against cantrip still deals half.
        const bool halved = spell.half_on_success || (def(caster).evoker && !spell.level);
        if (saved && !halved)
            continue;
        const int amount =
            resolved_damage(other, damage_type, evaded(other, spell.save, halved, saved, total));
        log(other.source.name + " takes " + std::to_string(amount) + " " + type + " damage.",
        {
            "{name} takes {damage} {type} damage.",
            {{"name", other.source.name}, {"damage", std::to_string(amount)}, {"type", type, true}}
        });
        damage(other, amount);
        if (!saved && spell.rider == detail::Rider::thunderwave && !other.life.dead)
            push_away(caster, other, 2);
    }
}

void Session::condition_area(Actor &caster, const detail::SpellDef &spell,
                             const std::vector<Cell> &cells)
{
    // Sleep chooses the enemies in its sphere; Color Spray's cone takes every
    // creature in it.
    const bool sleep = spell.rider == detail::Rider::sleep;
    const int dc = spell_dc(caster, spell.id);
    const auto index = index_of(caster.source.id);
    const unsigned slot = turn_end_ms(index) - (index ? turn_end_ms(index - 1) : 0);
    const auto careful = careful_allies(caster, cells);
    for (auto &other : actors_)
    {
        if (other.life.dead || other.life.hp == 0 || other.source.id == caster.source.id ||
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
            detail::apply_spell_benefit(other.effects, effect_source(caster), caster.source.name,
                                        detail::EffectKind::outlined, 0);
            log(other.source.name + " is outlined.", {"{name} is outlined.", {{"name", other.source.name}}});
        }
        else if (sleep)
        {
            detail::apply_repeating_condition(
                other.effects, detail::EffectKind::drowsy, effect_source(caster),
                caster.source.name,
                detail::RepeatingSave{.dc = dc, .first_save_ms = next_save_ms(other.source.id)});
            log(other.source.name + " grows drowsy.",
            {"{name} grows drowsy.", {{"name", other.source.name}}});
            end_incapacitated_effects(other);
        }
        else
        {
            detail::apply_poisoned(other.effects, effect_source(caster), caster.source.name,
                                   next_turn_ms(caster) + slot, detail::EffectKind::dazzled);
            log(other.source.name + " is Blinded.",
            {"{name} is Blinded.", {{"name", other.source.name}}});
        }
    }
}

void Session::blow(const Zone &line, Actor &creature)
{
    // Gust of Wind: a Strength save or 15 feet away from the caster.
    if (creature.life.dead || creature.life.hp == 0 || creature.source.id == line.caster)
        return;
    const auto &caster = actor(line.caster);
    if (!saving_throw_succeeds(creature, detail::Ability::strength, spell_dc(caster), false,
                               heightened_against(creature, caster, "gust_of_wind")))
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
    if (creature.life.dead || creature.life.hp == 0)
        return;
    const auto &spell = *detail::find_spell("flaming_sphere");
    const bool saved = saving_throw_succeeds(creature, spell.save, spell_dc(caster), false,
                       heightened_against(creature, caster, spell.id));
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
        a.form = shaped(a.definition,
                        detail::beast_forms.at(static_cast<std::size_t>(shape->dc - 1)));
}

void Session::end_wild_shape(Actor &a)
{
    if (!a.form)
        return;
    std::erase_if(a.effects.active, [](const auto & e)
    {
        return e.kind == detail::EffectKind::wild_shape;
    });
    const int form_speed = def(a).speed;
    a.form.reset();
    change_speed(a, form_speed);
    log(a.source.name + " leaves Wild Shape.", {"{name} leaves Wild Shape.", {{"name", a.source.name}}});
}

// A Speed change mid-turn changes the movement left by the difference; movement
// already spent stays spent (SRD 5.2.1 p. 14).
void Session::change_speed(Actor &a, int old_speed)
{
    a.movement = std::max(0, a.movement + def(a).speed - old_speed);
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
        return !other.life.dead && std::find(cells.begin(), cells.end(), other.source.cell) != cells.end();
    };
    // The Druid chooses its enemies for the thorns and its most wounded ally
    // for the flowers.
    const int dc = spell_dc(druid), rolled = dice(aid.dice);
    for (auto &other : actors_)
    {
        if (!inside(other) || other.source.side == druid.source.side || other.life.hp == 0)
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
        if (inside(other) && other.source.side == druid.source.side && other.life.hp < max_hp(other) &&
                (!wounded || max_hp(other) - other.life.hp > max_hp(*wounded) - wounded->life.hp))
            wounded = &other;
    if (wounded)
        (void)heal(*wounded, dice(aid.dice));
}

bool Session::wears_metal(const Actor &creature) const
{
    if (creature.source.character_profile.empty())
        return def(creature).metal_armor;
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
    if (creature.life.dead || creature.life.hp == 0 || !detail::can_apply(creature.effects) ||
            saving_throw_succeeds(creature, spell.save, spell_dc(caster), false,
                                  heightened_against(creature, caster, spell.id)))
        return;
    detail::apply_poisoned(creature.effects, effect_source(caster), caster.source.name,
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
    if (!shining || creature.life.dead || creature.life.hp == 0 ||
            detail::has_effect(creature.effects, detail::EffectKind::moonlit))
        return;
    const auto &caster = actor(beam.caster);
    const unsigned rest_of_turn = turn_end_ms(turn_) - (turn_ ? turn_end_ms(turn_ - 1) : 0);
    if (detail::can_apply(creature.effects))
        detail::apply_poisoned(creature.effects, effect_source(caster), caster.source.name,
                               rest_of_turn, detail::EffectKind::moonlit);
    const auto &spell = *detail::find_spell("moonbeam");
    const bool saved = saving_throw_succeeds(creature, spell.save, spell_dc(caster), false,
                       heightened_against(creature, caster, spell.id));
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
    if (creature.life.dead || creature.life.hp == 0 ||
            (grease ? creature.effects.prone
             : detail::has_effect(creature.effects, detail::EffectKind::webbed) ||
             !detail::can_apply(creature.effects)))
        return false;
    // Grease outlasts its caster, so its mark is found by the caster's id.
    const bool heightened = detail::heightened_against(creature.effects, scope_, zone.caster,
                            grease ? "grease" : "web");
    if (saving_throw_succeeds(creature, detail::Ability::dexterity, zone.save_dc, false, heightened))
        return false;
    if (grease)
    {
        creature.effects.prone = true;
        log(creature.source.name + " slips and falls Prone.",
        {"{name} slips and falls Prone.", {{"name", creature.source.name}}});
        return true;
    }
    // Web needs Concentration, so its caster is still on the field.
    const auto &caster = actor(zone.caster);
    detail::apply_entangle(creature.effects, effect_source(caster), caster.source.name,
                           zone.save_dc, detail::EffectKind::webbed);
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
    const int dc = spell_dc(caster, "ice_knife");
    const auto centre = target.source.cell;
    for (auto &other : actors_)
    {
        if (other.life.dead || distance(centre, other.source.cell) > 5)
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
        if (!board_.contains(next) || board_.at(next) == Terrain::obstacle ||
                std::any_of(actors_.begin(), actors_.end(), [&](const auto & other)
        {
            return !other.life.dead && other.source.cell == next;
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
    // The battlefield's edge is open ground beyond, not a wall.
    if (const Cell next{target.source.cell.x + sx, target.source.cell.y + sy};
            moved < squares && (sx || sy) && board_.contains(next))
        slam(target, 5 * (squares - moved));
}

bool Session::blocks_push(Cell cell, EntityId pushed) const
{
    return board_.contains(cell) &&
           (board_.at(cell) == Terrain::obstacle ||
            std::any_of(actors_.begin(), actors_.end(), [&](const auto & other)
    {
        return !other.life.dead && other.source.id != pushed && other.source.cell == cell;
    }));
}

void Session::slam(Actor &target, int short_feet)
{
    if (short_feet <= 0 || target.life.dead)
        return;
    const int dice = std::min(3, (short_feet + 9) / 10);
    int rolled = 0;
    for (int n = 0; n < dice; ++n)
        rolled += roll(6);
    const int amount = resolved_damage(target, detail::DamageType::bludgeoning, rolled);
    log(target.source.name + " slams into an obstacle and takes " + std::to_string(amount) +
        " Bludgeoning damage.",
    {
        "{name} slams into an obstacle and takes {damage} Bludgeoning damage.",
        {{"name", target.source.name}, {"damage", std::to_string(amount)}}
    });
    damage(target, amount);
}

bool Session::provokes(const Actor &other, const Actor &moving, Cell from, Cell to) const
{
    return other.source.side != moving.source.side && conscious(other) && other.reaction &&
           !detail::has_effect(other.effects, detail::EffectKind::turned) &&
           !detail::incapacitated(other.effects) && !detail::opportunity_blocked(other.effects) &&
           !charmed_by(other, moving.source.id) && has_weapon_reaction(other, from, to) &&
           can_see(other, moving);
}

std::vector<Cell> Session::flight_route(const Actor &from, const Actor &target, int budget) const
{
    // Squares are searched by feet spent, keeping for each square and spend
    // the route with the fewest Opportunity Attacks provoked.
    const auto grid = movement_grid(target);
    const int width = board_.width, cells = board_.width * board_.height, spends = budget / 5 + 1;
    constexpr int unreached = std::numeric_limits<int>::max();
    struct Visit
    {
        int provoked = unreached;
        int previous = -1;
    };
    std::vector<Visit> visits(static_cast<std::size_t>(cells * spends));
    const auto at = [&](Cell c, int feet)
    {
        return static_cast<std::size_t>((c.y * width + c.x) * spends + feet / 5);
    };
    const auto cell_of = [&](std::size_t visit)
    {
        const int square = static_cast<int>(visit) / spends;
        return Cell{square % width, square / width};
    };
    visits[at(target.source.cell, 0)].provoked = 0;
    for (int feet = 0; feet < budget; feet += 5)
        for (int square = 0; square < cells; ++square)
        {
            const Cell here{square % width, square / width};
            const auto &visit = visits[at(here, feet)];
            if (visit.provoked == unreached)
                continue;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx)
                {
                    const Cell next{here.x + dx, here.y + dy};
                    const auto cost = (dx || dy) && board_.contains(next)
                                      ? grid.step_cost(here, next) : std::nullopt;
                    if (!cost || feet + *cost > budget)
                        continue;
                    const auto provoked = static_cast<int>(std::count_if(
                                              actors_.begin(), actors_.end(), [&](const auto & other)
                    {
                        return provokes(other, target, here, next);
                    }));
                    auto &reached = visits[at(next, feet + *cost)];
                    if (visit.provoked + provoked < reached.provoked)
                        reached = {visit.provoked + provoked, static_cast<int>(at(here, feet))};
                }
        }
    // The farthest square it may stop on, then the safest route, then the shortest.
    std::size_t best = at(target.source.cell, 0);
    const auto better = [&](std::size_t candidate)
    {
        const auto a = cell_of(candidate), b = cell_of(best);
        const int far = distance(from.source.cell, a), best_far = distance(from.source.cell, b);
        if (far != best_far)
            return far > best_far;
        return visits[candidate].provoked < visits[best].provoked;
    };
    for (std::size_t visit = 0; visit < visits.size(); ++visit)
        if (visits[visit].provoked != unreached && grid.can_stop_at(cell_of(visit)) && better(visit))
            best = visit;
    std::vector<Cell> route;
    for (auto visit = best; visits[visit].previous >= 0;
            visit = static_cast<std::size_t>(visits[visit].previous))
        route.push_back(cell_of(visit));
    std::reverse(route.begin(), route.end());
    return route;
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
    // Not a spell: the teleport leaves Sanctuary, Invisibility and hiding be.
    if (aimed.verb == "clouds_jaunt")
    {
        clouds_jaunt(a, aimed.center);
        return;
    }
    end_sanctuary(a);
    const bool was_invisible = invisible(a);
    const auto id = a.source.id;
    if (aimed.verb.starts_with("breath_weapon_"))
        breath_weapon(a, aimed);
    else
        cast_aimed_area(a, aimed);
    if (was_invisible)
        end_invisibility(id);
    end_hiding(id);
}

void Session::breath_weapon(Actor &dragonborn, const PendingArea &aimed)
{
    // The exhalation replaces the Attack action's one attack; Extra Attack,
    // which would leave others, comes after the level cap.
    dragonborn.nick_origin = 0;
    (void)dragonborn.actions.spend();
    spend_trait_use(dragonborn);
    log(dragonborn.source.name + " uses Breath Weapon.",
    {"{name} uses Breath Weapon.", {{"name", dragonborn.source.name}}});
    const auto &breath = *detail::find_spell(aimed.verb);
    damage_area(dragonborn, breath, aimed.verb,
                area_cells(breath, aimed.verb, dragonborn.source.cell, aimed.center),
                area_dc(dragonborn, breath));
}

std::optional<int> Session::heroic_reroll(Actor &a)
{
    if (!conscious(a) || !std::erase_if(a.effects.active, [](const auto & e)
{
    return e.kind == detail::EffectKind::heroic_inspiration;
}))
    return std::nullopt;
    const int natural = roll(20);
    log(a.source.name + " uses Heroic Inspiration and rolls the d20 again: " +
        std::to_string(natural) + ".",
    {
        "{name} uses Heroic Inspiration and rolls the d20 again: {roll}.",
        {{"name", a.source.name}, {"roll", std::to_string(natural)}}
    });
    return natural;
}

void Session::spend_trait_use(Actor &a)
{
    for (auto &effect : a.effects.active)
        if (effect.kind == detail::EffectKind::trait_uses)
        {
            ++effect.dc;
            return;
        }
    detail::apply_spell_benefit(a.effects, effect_source(a), a.source.name,
                                detail::EffectKind::trait_uses, 1);
}

void Session::cast_aimed_area(Actor &a, const PendingArea &aimed)
{
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
        pay_for_spell(a, aimed.verb, spell);
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
                if (!heightened_target_ && !other.life.dead && other.source.cell == cell &&
                        other.source.side != a.source.side)
                    heightened_target_ = other.source.id;
    mark_heightened(a, spell);
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
    zones_.push_back({a.source.id, kind, cells, kind == ZoneKind::grease ? elapsed_ms_ + 60000 : 0,
                      spell_dc(a, spell.id)});
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
    const int dc = spell_dc(a, spell.id);
    for (auto &other : actors_)
    {
        if (other.life.dead || other.source.id == a.source.id ||
                std::find(cells.begin(), cells.end(), other.source.cell) == cells.end() ||
                !detail::can_apply(other.effects) ||
                saving_throw_succeeds(other, spell.save, dc))
            continue;
        detail::apply_entangle(other.effects, effect_source(a), a.source.name, dc);
        log(other.source.name + " is Restrained.",
        {"{name} is Restrained.", {{"name", other.source.name}}});
    }
}

int Session::disciple_of_life(const Actor &caster, unsigned slot_level) const
{
    // Disciple of Life: healing from a spell slot restores 2 + the slot's level more.
    return def(caster).life_domain ? 2 + static_cast<int>(slot_level) : 0;
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
        if (other.source.side == cleric.source.side && !other.life.dead &&
                other.life.hp * 2 <= max_hp(other) &&
                distance(cleric.source.cell, other.source.cell) <= 30 &&
                def(other).creature_type != "undead" && def(other).creature_type != "construct")
            bloodied.push_back(&other);
    std::sort(bloodied.begin(), bloodied.end(), [&](const Actor * x, const Actor * y)
    {
        return x->life.hp * max_hp(*y) < y->life.hp * max_hp(*x);
    });
    for (auto *other : bloodied)
    {
        const int room = max_hp(*other) / 2 - other->life.hp;
        if (pool <= 0 || room <= 0)
            continue;
        pool -= heal(*other, std::min(pool, room));
    }
}

void Session::divine_spark(Actor &cleric, Actor &target)
{
    // 1d8 + Wisdom modifier: restored to an ally, or Radiant to an enemy (the
    // SRD's Necrotic is not offered, SPARK-1), halved on a Constitution save.
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
    const auto type = detail::DamageType::radiant;
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
        if (other.source.side != cleric.source.side && other.life.hp > 0 &&
                def(other).creature_type == "undead" &&
                distance(cleric.source.cell, other.source.cell) <= 30 &&
                detail::can_apply(other.effects) &&
                !saving_throw_succeeds(other, detail::Ability::wisdom, spell_dc(cleric)))
        {
            detail::apply_spell_benefit(other.effects, effect_source(cleric),
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
    --a.slots[1];
    a.spent_slot = true;
    log(a.source.name + " casts Ensnaring Strike on " + target.source.name + ".",
    {
        "{name} casts Ensnaring Strike on {target}.",
        {{"name", a.source.name}, {"target", target.source.name}}
    });
    const int dc = spell_dc(a);
    if (target.life.hp == 0 || !detail::can_apply(target.effects) ||
            saving_throw_succeeds(target, detail::Ability::strength, dc, def(target).size >= 3))
        return;
    begin_concentration(a, spell);
    detail::apply_ensnaring_strike(target.effects, effect_source(a), a.source.name, dc);
    log(target.source.name + " is Restrained.",
    {"{name} is Restrained.", {{"name", target.source.name}}});
}

void Session::squeeze_ensnared(Actor &a)
{
    // Ensnaring Strike's vines deal 1d6 Piercing at the start of each turn.
    if (!detail::has_effect(a.effects, detail::EffectKind::ensnaring_strike) || a.life.hp == 0)
        return;
    const int amount = resolved_damage(a, detail::DamageType::piercing, roll(6));
    log(a.source.name + " takes " + std::to_string(amount) + " Piercing damage from the vines.",
    {
        "{name} takes {damage} Piercing damage from the vines.",
        {{"name", a.source.name}, {"damage", std::to_string(amount)}}
    });
    damage(a, amount, false);
}

int Session::guidance_bonus(const Actor &a, std::string_view skill)
{
    const bool guided = std::any_of(a.effects.active.begin(), a.effects.active.end(),
                                    [&](const auto & e)
    {
        return e.kind == detail::EffectKind::guidance &&
               detail::guidance_skills[static_cast<std::size_t>(e.dc)] == skill;
    });
    if (!guided)
        return 0;
    const int bonus = roll(4);
    log(a.source.name + "'s Guidance adds " + std::to_string(bonus) + ".",
    {
        "{name}'s Guidance adds {amount}.",
        {{"name", a.source.name}, {"amount", std::to_string(bonus)}}
    });
    return bonus;
}

void Session::escape_ensnaring(Actor &a)
{
    // A Strength (Athletics) check against the spell's DC ends Ensnaring
    // Strike. A creature's Strength save bonus stands in for Athletics. Rage
    // and the Champion's Remarkable Athlete give the check Advantage.
    const auto found = std::find_if(a.effects.active.begin(), a.effects.active.end(),
                                    [](const auto & e)
    {
        return e.kind == detail::EffectKind::ensnaring_strike ||
               e.kind == detail::EffectKind::entangle || e.kind == detail::EffectKind::webbed;
    });
    const auto vines = *found;
    const bool advantage = rage_of(a) || def(a).champion;
    const int natural = reliable(
        a, "athletics", detail::d20({advantage, !fears_in_sight(a).empty(), def(a).lucky}, rng_));
    int bonus = (a.source.character_profile.empty() ? def(a).saves[0] : def(a).athletics) +
                guidance_bonus(a, "athletics");
    bool escaped = natural + bonus >= vines.dc;
    // An enemy Lore Bard's Cutting Words may spoil the escape; the check's
    // line shows the lowered bonus, as an attack's does.
    if (escaped)
    {
        bonus -= cutting_words(a, natural + bonus - vines.dc, CutRoll::check);
        escaped = natural + bonus >= vines.dc;
    }
    if (!escaped && bonus + 20 >= vines.dc)
        if (const auto again = heroic_reroll(a))
            escaped = *again + bonus >= vines.dc;
    const std::string advantage_label = advantage ? " (advantage)" : "";
    log(a.source.name + " Athletics check: d20 " + std::to_string(natural) + advantage_label +
        " + " + std::to_string(bonus) + " vs DC " + std::to_string(vines.dc) +
        (escaped ? " (escapes)." : " (still Restrained)."),
    {
        escaped ? "{name} Athletics check: d20 {roll}{advantage} + {bonus} vs DC {dc} (escapes)."
        : "{name} Athletics check: d20 {roll}{advantage} + {bonus} vs DC {dc} (still Restrained).",
        {   {"name", a.source.name},
            {"roll", std::to_string(natural)},
            {"advantage", advantage_label, true},
            {"bonus", std::to_string(bonus)},
            {"dc", std::to_string(vines.dc)}
        }
    });
    if (!escaped)
        escaped = inspiration_roll(a, vines.dc - natural - bonus) >= vines.dc - natural - bonus;
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
    const auto found = std::find(chosen.begin(), chosen.end(), command.target);
    if (found != chosen.end() && !splits_among_targets(*detail::find_spell(selection_->verb)))
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
    const bool was_invisible = invisible(a);
    const auto id = a.source.id;
    cast_selected(a, selection);
    if (was_invisible)
        end_invisibility(id);
    end_hiding(id);
}

void Session::cast_selected(Actor &a, const PendingSelection &selection)
{
    const auto &spell = *detail::find_spell(selection.verb);
    const bool quickened = readies(a, Metamagic::quickened, spell);
    if (splits_among_targets(spell))
    {
        if (quickened)
            a.bonus = false;
        else
        {
            a.nick_origin = 0;
            (void)a.actions.spend(true);
        }
        cast_resolved_spell(spell, selection.verb, a, selection.chosen);
        return;
    }
    take_metamagic(a, spell);
    if (casting_with(Metamagic::heightened))
        heightened_target_ = selection.chosen.front();
    mark_heightened(a, spell);
    if (quickened)
        a.bonus = false;
    else
    {
        a.nick_origin = 0;
        (void)a.actions.spend(true);
    }
    pay_for_spell(a, selection.verb, spell);
    log(a.source.name + " casts " + std::string(spell.label) + ".",
    {"{name} casts {spell}.", {{"name", a.source.name}, {"spell", std::string(spell.label), true}}});
    if (spell.concentration)
        begin_concentration(a, spell);
    const int dc = spell_dc(a, spell.id);
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

std::vector<const Actor *> Session::fears_in_sight(const Actor &a) const
{
    std::vector<const Actor *> result;
    for (const auto &e : a.effects.active)
        if (e.kind == detail::EffectKind::frightened && e.source_scope == scope_)
            for (const auto &other : actors_)
                if (other.source.id == e.source_actor && !other.life.dead &&
                        line_of_sight(a.source.cell, other.source.cell))
                    result.push_back(&other);
    return result;
}

void Session::intimidate(const Actor &barbarian)
{
    const auto &d = def(barbarian);
    const int dc = 8 + d.strength + 2 + (d.level - 1) / 4;
    log(barbarian.source.name + " uses Intimidating Presence.",
    {"{name} uses Intimidating Presence.", {{"name", barbarian.source.name}}});
    for (auto &other : actors_)
    {
        if (other.source.side == barbarian.source.side || !conscious(other) ||
                distance(barbarian.source.cell, other.source.cell) > 30 ||
                !line_of_sight(barbarian.source.cell, other.source.cell) || mindless(other) ||
                detail::has_effect(other.effects, detail::EffectKind::frightened) ||
                !detail::can_apply(other.effects))
            continue;
        if (saving_throw_succeeds(other, detail::Ability::wisdom, dc, def(other).brave))
            continue;
        detail::apply_repeating_condition(
            other.effects, detail::EffectKind::frightened, effect_source(barbarian),
            barbarian.source.name,
            detail::RepeatingSave{.dc = dc, .first_save_ms = next_save_ms(other.source.id)});
        log(other.source.name + " is Frightened.",
        {"{name} is Frightened.", {{"name", other.source.name}}});
    }
}

bool Session::charmed_by(const Actor &a, EntityId other) const
{
    // Aura of Devotion: a Charm has no effect while its target is in the aura.
    if (aura_of_devotion(a))
        return false;
    return std::any_of(a.effects.active.begin(), a.effects.active.end(), [&](const auto & e)
    {
        return e.kind == detail::EffectKind::charmed && e.source_actor == other &&
               e.source_scope == scope_;
    });
}

// A Paladin's 10-foot Emanation reaches itself and its allies in a clear line,
// while it is conscious and not Incapacitated.
bool Session::aura_covers(const Actor &paladin, const Actor &a) const
{
    return def(paladin).aura_of_protection && paladin.source.side == a.source.side &&
           conscious(paladin) && !detail::incapacitated(paladin.effects) &&
           distance(paladin.source.cell, a.source.cell) <= 10 &&
           line_of_sight(paladin.source.cell, a.source.cell);
}

int Session::aura_of_protection(const Actor &a) const
{
    // A creature in two auras benefits from one; the game takes the larger.
    int bonus = 0;
    for (const auto &paladin : actors_)
        if (aura_covers(paladin, a))
            bonus = std::max(bonus, def(paladin).aura_of_protection);
    return bonus;
}

bool Session::aura_of_devotion(const Actor &a) const
{
    return std::any_of(actors_.begin(), actors_.end(), [&](const auto & paladin)
    {
        return def(paladin).aura_of_devotion && aura_covers(paladin, a);
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
    return armor_class_of(target);
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
                if (caster.source.id == e.source_actor && caster.life.hp > 0 && !caster.life.dead &&
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

bool Session::invisible(const Actor &a)
{
    return detail::has_effect(a.effects, detail::EffectKind::invisible);
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

bool Session::can_hide(const Actor &a) const
{
    if (hidden(a) || a.source.character_profile.empty())
        return false;
    // Naturally Stealthy: a creature at least one size larger also blocks
    // the view (the viewer itself aside).
    return std::none_of(actors_.begin(), actors_.end(), [&](const auto & enemy)
    {
        if (enemy.source.side == a.source.side || !conscious(enemy) ||
                obscured(enemy, enemy.source.cell) || obscured(enemy, a.source.cell))
            return false;
        auto board = board_;
        if (def(a).naturally_stealthy)
            for (const auto &other : actors_)
                if (!other.life.dead && other.source.id != a.source.id &&
                        other.source.id != enemy.source.id && def(other).size > def(a).size)
                    board.terrain[other.source.cell.y * board_.width + other.source.cell.x] =
                        Terrain::obstacle;
        return detail::has_line_of_sight(board, enemy.source.cell, a.source.cell);
    });
}

void Session::hide(Actor &a)
{
    // Armor that hinders stealth gives the check Disadvantage.
    constexpr int dc = 15;
    const auto &d = def(a);
    bool lucky_reroll = false;
    int natural = detail::d20({false, d.stealth_disadvantage || !fears_in_sight(a).empty(), d.lucky},
                              rng_, &lucky_reroll);
    log_luck(a, lucky_reroll);
    natural = reliable(a, "stealth", natural);
    int total = natural + d.stealth;
    const std::string disadvantage = d.stealth_disadvantage ? " (disadvantage)" : "";
    log(a.source.name + " Stealth check: d20 " + std::to_string(natural) + disadvantage + " + " +
        std::to_string(d.stealth) + " vs DC 15.",
    {
        "{name} Stealth check: d20 {roll}{disadvantage} + {bonus} vs DC {dc}.",
        {   {"name", a.source.name},
            {"roll", std::to_string(natural)},
            {"disadvantage", disadvantage, true},
            {"bonus", std::to_string(d.stealth)},
            {"dc", std::to_string(dc)}
        }
    });
    if (total < dc && d.stealth + 20 >= dc)
        if (const auto again = heroic_reroll(a))
            total = *again + d.stealth;
    if (total < dc)
        total += inspiration_roll(a, dc - total);
    if (total < dc)
    {
        log(a.source.name + " fails to hide.", {"{name} fails to hide.", {{"name", a.source.name}}});
        return;
    }
    detail::apply_spell_benefit(a.effects, effect_source(a), a.source.name,
                                detail::EffectKind::hidden, std::min(total, 40));
    log(a.source.name + " is hidden (Stealth " + std::to_string(total) + ").",
    {"{name} is hidden (Stealth {total}).", {{"name", a.source.name}, {"total", std::to_string(total)}}});
}

void Session::search(Actor &searcher)
{
    const auto &d = def(searcher);
    bool lucky_reroll = false;
    const int natural =
        detail::d20({false, !fears_in_sight(searcher).empty(), d.lucky}, rng_, &lucky_reroll);
    log_luck(searcher, lucky_reroll);
    int bonus = d.perception;
    // An enemy Lore Bard's Cutting Words may keep the best-hidden creature the
    // search would find hidden.
    int margin = 6;
    for (const auto &other : actors_)
        if (other.source.side != searcher.source.side && hidden(other) &&
                clear_view(searcher, other) && natural + bonus >= stealth_total(other))
            margin = std::min(margin, natural + bonus - stealth_total(other));
    bonus -= cutting_words(searcher, margin, CutRoll::check);
    const int total = natural + bonus;
    log(searcher.source.name + " searches: Perception d20 " + std::to_string(natural) + " + " +
        std::to_string(bonus) + " = " + std::to_string(total) + ".",
    {
        "{name} searches: Perception d20 {roll} + {bonus} = {total}.",
        {   {"name", searcher.source.name},
            {"roll", std::to_string(natural)},
            {"bonus", std::to_string(bonus)},
            {"total", std::to_string(total)}
        }
    });
    for (auto &other : actors_)
        if (other.source.side != searcher.source.side && hidden(other) &&
                clear_view(searcher, other) && total >= stealth_total(other))
            find_hidden(other, searcher);
}

void Session::notice_hidden()
{
    for (auto &hider : actors_)
        for (const auto &enemy : actors_)
            if (hidden(hider) && enemy.source.side != hider.source.side &&
                    clear_view(enemy, hider) && 10 + def(enemy).perception >= stealth_total(hider))
                find_hidden(hider, enemy);
}

void Session::find_hidden(Actor &hider, const Actor &finder)
{
    std::erase_if(hider.effects.active, [](const auto & e)
    {
        return e.kind == detail::EffectKind::hidden;
    });
    log(finder.source.name + " finds " + hider.source.name + ".",
    {"{finder} finds {name}.", {{"finder", finder.source.name}, {"name", hider.source.name}}});
}

void Session::end_hiding(EntityId id)
{
    auto &a = actor(id);
    if (std::erase_if(a.effects.active, [](const auto & e)
{
    return e.kind == detail::EffectKind::hidden;
}))
    log(a.source.name + " is no longer hidden.",
    {"{name} is no longer hidden.", {{"name", a.source.name}}});
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
    caster.concentration.start(
    {   {scope_, 1, caster.source.id},
        detail::benefit_duration_ms(rider_effect(spell.rider)) * (extended ? 2 : 1)
    });
    if (extended)
        detail::apply_spell_benefit(caster.effects, effect_source(caster), caster.source.name,
                                    detail::EffectKind::extended, 0);
}

void Session::end_concentration(Actor &caster)
{
    if (caster.concentration.stop())
        drop_concentration_effects(caster);
}

void Session::end_incapacitated_effects(Actor &a)
{
    end_concentration(a);
    for (auto &other : actors_)
        std::erase_if(other.effects.active, [&](const auto & e)
    {
        return e.kind == detail::EffectKind::turned && e.source_actor == a.source.id &&
               e.source_scope == scope_;
    });
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
    for (std::size_t n = 0; n < a.effects.active.size() && a.life.hp > 0; ++n)
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
        if (a.life.hp > 0 && saving_throw_succeeds(a, detail::Ability::constitution, dc))
            a.effects.active[n].remaining_ms = 0;
    }
    std::erase_if(a.effects.active, [](const auto & e)
    {
        return e.kind == detail::EffectKind::searing_smite && !e.remaining_ms;
    });
}

detail::Mastery Session::weapon_mastery(const Actor &a, AttackRange range) const
{
    const auto &d = def(a);
    if (d.masteries.empty() || (range == AttackRange::melee && d.ranged_weapon))
        return detail::Mastery::none;
    for (const auto &key : d.equipment_keys)
        if (const auto *weapon = detail::weapon(key))
        {
            if (std::find(d.masteries.begin(), d.masteries.end(), key) == d.masteries.end())
                return detail::Mastery::none;
            return tactical_mastery_ && tactical_mastery_->actor == a.source.id
                   ? tactical_mastery_->kind
                   : weapon->mastery;
        }
    return detail::Mastery::none;
}

bool Session::mastery_capacity(const Actor &a, const Actor &target, AttackRange range) const
{
    const auto kind = weapon_mastery(a, range);
    if (kind != detail::Mastery::sap && kind != detail::Mastery::vex)
        return true;
    return detail::can_apply_attack_mastery(target.effects,
                                            kind == detail::Mastery::sap ? detail::EffectKind::sap
                                            : detail::EffectKind::vex,
                                            effect_source(a));
}

detail::RollModifiers Session::attack_modifiers(const Actor &a, const Actor &target,
        AttackRange range, AttackSource source) const
{
    const bool ranged = range == AttackRange::ranged;
    const bool spell = source == AttackSource::spell;
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
    auto result = detail::attack_modifiers(detail::AttackConditions{
        .attacker_blind = detail::blinded(a.effects) || attacker_fogged || unseen(a, target),
        .target_blind = detail::blinded(target.effects) || target_fogged || unseen(target, a),
        .target_dodging = target.dodge,
        .other_disadvantage = disadvantaged || a.effects.prone});
    // At zero HP the creature is Unconscious and Prone (SRD pp.187,191).
    // At longer range their opposing attack modifiers cancel, not stack.
    const bool pack_tactics = !spell && d.pack_tactics && ally_beside(a, target);
    // A Boar's Bloodied Fury: Advantage at half its Hit Points or fewer.
    if (!spell && d.bloodied_fury && a.life.hp * 2 <= max_hp(a))
        result.advantage = true;
    if (helpless(target) || a.aim_ready || pack_tactics ||
            detail::vexed_by(target.effects, effect_source(a)) ||
            detail::studied_by(target.effects, effect_source(a)))
        result.advantage = true;
    if (detail::sapped(a.effects) ||
            detail::has_effect(a.effects, detail::EffectKind::poisoned) ||
            detail::has_effect(a.effects, detail::EffectKind::scorched) ||
            !fears_in_sight(a).empty())
        result.disadvantage = true;
    // Ray of Enfeeblement: Disadvantage on Strength-based attack rolls, here
    // every melee weapon attack.
    if (!spell && !ranged && detail::has_effect(a.effects, detail::EffectKind::enfeebled))
        result.disadvantage = true;
    // Reckless Attack: Advantage on the attacker's Strength attack rolls and on
    // attack rolls against it.
    if ((!spell && detail::has_effect(a.effects, detail::EffectKind::reckless) &&
            strength_attack(a, range)) ||
            detail::has_effect(target.effects, detail::EffectKind::reckless))
        result.advantage = true;
    // Faerie Fire: Advantage against an outlined creature the attacker sees.
    if (detail::has_effect(target.effects, detail::EffectKind::outlined) && can_see(a, target))
        result.advantage = true;
    // Shining Smite: Advantage against the shining creature.
    if (detail::has_effect(target.effects, detail::EffectKind::shining))
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
    // Stunned: attack rolls against it have Advantage.
    if (detail::has_effect(target.effects, detail::EffectKind::stunned))
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
    if (target.effects.prone || target.life.hp == 0)
    {
        if (distance(a.source.cell, target.source.cell) <= 5)
            result.advantage = true;
        else
            result.disadvantage = true;
    }
    result.lucky = d.lucky;
    return result;
}

detail::DamageDieRule Session::weapon_die_rule(const Actor &a, AttackRange range) const
{
    const auto &d = def(a);
    return d.great_weapon_fighting && range == AttackRange::melee && !d.ranged_weapon &&
           !d.weapon_label.empty() && d.weapon_hands == 2
           ? detail::DamageDieRule::great_weapon_fighting
           : detail::DamageDieRule::normal;
}

Dice Session::weapon_dice(const Actor &a, AttackRange range) const
{
    const bool ranged = range == AttackRange::ranged;
    const auto &d = def(a);
    auto result = ranged ? d.ranged : d.melee;
    if (!ranged && d.versatile_sides && d.weapon_hands == 2)
        result.sides = d.versatile_sides;
    // Shillelagh: a d8 plus the spellcasting modifier.
    if (!ranged && shillelagh(a))
        result = {1, 8, casting_bonus(d, "shillelagh") - 2};
    if (a.cleave_damage || (a.light_damage && !d.two_weapon_fighting))
        result.bonus = std::min(0, result.bonus);
    return result;
}

void Session::extend_run(Actor &a, Cell from, Cell to)
{
    const Cell step{to.x - from.x, to.y - from.y};
    if (a.run_feet && a.run_end == from && a.run_step == step)
        a.run_feet += 5;
    else
    {
        a.run_start = from;
        a.run_step = step;
        a.run_feet = 5;
    }
    a.run_end = to;
}

void Session::study(const Actor &fighter, Actor &target)
{
    // Studied Attacks lasts until the end of the Fighter's next turn, as Vex does.
    const auto index = index_of(fighter.source.id);
    const unsigned slot = turn_end_ms(index) - (index ? turn_end_ms(index - 1) : 0);
    detail::apply_studied(target.effects, effect_source(fighter), fighter.source.name,
                          next_turn_ms(actor(fighter.source.id)) + slot);
    log(fighter.source.name + " studies " + target.source.name + ".",
    {"{name} studies {target}.", {{"name", fighter.source.name}, {"target", target.source.name}}});
}

bool Session::charges(const Actor &a, const Actor &target) const
{
    // 20 feet or more in one straight line, each step 5 feet closer to the
    // target, ending where the Boar stands.
    return def(a).charge && def(target).size <= 2 && a.run_feet >= 20 &&
           a.run_end == a.source.cell &&
           distance(a.run_start, target.source.cell) - distance(a.run_end, target.source.cell) ==
           a.run_feet;
}

void Session::apply_hit(Actor &a, Actor &target, const ResolvedAttack &resolved)
{
    const int natural = resolved.natural, bonus = resolved.bonus, mode = resolved.mode;
    const bool spell = resolved.spell, savage = resolved.savage;
    const auto range = resolved.range;
    const bool ranged = range == AttackRange::ranged;
    detail::consume_attack_masteries(actor(a.source.id).effects, target.effects, effect_source(a));
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
    if (!(!spell && natural >= def(a).critical_from) &&
            !attack_hits(natural, bonus, armor_class(target)))
    {
        log(message + " misses.",
        {
            "{actor} -> {target}: d20 {roll} + {bonus} vs AC {ac}{disadvantage} misses.",
            arguments
        });
        if (!spell && !ranged &&
                weapon_mastery(a, AttackRange::melee) == detail::Mastery::graze &&
                graze_damage(a, target) > 0)
            graze_ = PendingGraze{a.source.id, target.source.id, natural};
        if (def(a).studied_attacks)
            study(a, target);
        return;
    }
    const bool critical = critical_hit(a, target, natural, spell);
    const bool helpless = unconscious(target) && distance(a.source.cell, target.source.cell) <= 5;
    if (savage)
        message += " (Savage Attacker)";
    const int amount = resolved_damage(target, resolved.type, resolved.damage);
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
    if (!spell && !ranged && def(a).prone_bite && def(target).size <= 2 && target.life.hp > 0 &&
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
        const auto property = weapon_mastery(a, range);
        if (property == detail::Mastery::sap || (property == detail::Mastery::vex && amount > 0))
        {
            const auto &source = actor(a.source.id);
            const auto index = index_of(source.source.id);
            const unsigned slot = turn_end_ms(index) - (index ? turn_end_ms(index - 1) : 0);
            const auto kind = property == detail::Mastery::sap ? detail::EffectKind::sap
                              : detail::EffectKind::vex;
            detail::apply_attack_mastery(target.effects, kind, effect_source(a), a.source.name,
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
    if (!spell)
        offer_mastery(a, target, natural, range, amount, critical);
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
    if (pending() == a.source.id && mover().life.hp == 0 && (effect_waiting() || champion_move_))
    {
        path_.clear();
        path_index_ = 0;
        reactors_.clear();
        reactor_index_ = 0;
        flight_.reset();
    }
}

bool Session::sneak_eligible(const Actor &a, const Actor &target, AttackRange range,
                             int mode) const
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
        range == AttackRange::ranged ? d.range > 0 : !d.weapon_label.empty() && !d.ranged_weapon,
        d.finesse,
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

int Session::roll_sneak_attack(Actor &a, const Actor &target, int natural, int forgo)
{
    a.sneak_used = actor(a.source.id).sneak_used = true;
    const bool critical = critical_hit(a, target, natural);
    auto sneak_dice = detail::sneak_attack_dice(def(a).sneak_level);
    sneak_dice.count = std::max(0, sneak_dice.count - forgo);
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

bool Session::attack(Actor &a, Actor &target, AttackRange range,
                     const std::optional<SpellAttack> &spell_attack)
{
    const bool ranged = range == AttackRange::ranged;
    const bool spell = spell_attack.has_value();
    if (target.source.cell.x != a.source.cell.x)
        a.facing_left = target.source.cell.x < a.source.cell.x;
    if (sanctuary_stops(a, target))
        return false;
    const auto &d = def(a);
    auto modifiers =
        attack_modifiers(a, target, range, spell ? AttackSource::spell : AttackSource::weapon);
    // Brutal Strike gives up the roll's Advantage; a roll with Disadvantage
    // cannot be chosen, so it stays an ordinary attack.
    const bool brutal = brutal_ && brutal_->actor == a.source.id && !spell &&
                        range == AttackRange::melee && !modifiers.disadvantage;
    if (brutal)
        modifiers.advantage = false;
    a.aim_ready = false;
    if (actors_[turn_].source.id == a.source.id)
        actor(a.source.id).attacked = true;
    // Guiding Bolt's and Studied Attacks' Advantage is spent on this attack roll.
    const auto attacker = effect_source(a);
    std::erase_if(target.effects.active, [&](const auto & e)
    {
        return e.kind == detail::EffectKind::guiding_bolt ||
               (e.kind == detail::EffectKind::studied && e.source_scope == attacker.scope &&
                e.source_actor == attacker.actor);
    });
    const int weapon_magic = spell ? 0 : magic_weapon_bonus(a);
    const bool charging = !spell && !ranged && charges(a, target);
    a.run_feet = 0;
    bool lucky_reroll = false;
    int natural = detail::d20(modifiers, rng_, &lucky_reroll);
    log_luck(a, lucky_reroll);
    end_invisibility(a.source.id);
    // Supreme Sneak's Stealth Attack keeps a hidden Rogue hidden when it deals
    // Sneak Attack damage; that is known after the roll.
    const bool stealthy = cunning_ && cunning_->actor == a.source.id &&
                          cunning_->has(CunningStrike::stealth);
    if (!stealthy)
        end_hiding(a.source.id);
    // An attack roll against an enemy on its own turn extends a Rage.
    if (target.source.side != a.source.side && actors_[turn_].source.id == a.source.id)
        extend_rage(actor(a.source.id));
    int bonus = (spell ? casting_bonus(d, spell_attack->spell)
                 : !ranged && shillelagh(a) ? casting_bonus(d, "shillelagh")
                 : ranged ? d.ranged_bonus
                 : d.melee_bonus) + blessing_die(a) +
                (spell || ranged ? 0 : sacred_weapon_bonus(a)) + weapon_magic;
    // Sundering Blow: +5 to the next attack roll another creature makes against it.
    if (detail::spend_sundering(target.effects, effect_source(a)))
    {
        bonus += 5;
        log("Sundering Blow adds 5 to the attack roll.",
        {"Sundering Blow adds 5 to the attack roll.", {}});
    }
    const auto damage_dice = spell ? spell_attack->dice : weapon_dice(a, range);
    // Seeking Spell: a missed spell attack rolls its d20 again, once.
    if (spell && casting_with(Metamagic::seeking) && natural != 20 &&
            !attack_hits(natural, bonus, armor_class(target)))
    {
        casting_metamagic_.reset();
        natural = detail::d20(modifiers, rng_);
        log("Seeking Spell rerolls the missed attack.", {"Seeking Spell rerolls the missed attack.", {}});
    }
    // Heroic Inspiration rolls a missed attack again.
    if (natural != 20 && !(!spell && natural >= d.critical_from) &&
            !attack_hits(natural, bonus, armor_class(target)))
        if (const auto again = heroic_reroll(actor(a.source.id)))
            natural = *again;
    const bool automatic = natural == 20 || (!spell && natural >= d.critical_from);
    bool hit = automatic || attack_hits(natural, bonus, armor_class(target));
    // Bardic Inspiration may rescue a miss; Cutting Words may spoil a hit.
    if (!hit && natural != 1)
    {
        bonus += inspiration_roll(a, armor_class(target) - natural - bonus);
        hit = attack_hits(natural, bonus, armor_class(target));
    }
    else if (hit && !automatic)
    {
        bonus -= cutting_words(a, natural + bonus - armor_class(target));
        hit = attack_hits(natural, bonus, armor_class(target));
    }
    const auto damage_type = spell    ? spell_attack->type
                             : ranged ? d.ranged_type
                             : sacred_damage_type(a);
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
    const bool sneak = hit && !spell && sneak_eligible(a, target, range, modifiers.mode());
    const auto roll_damage = [&]
    {
        return !spell && (d.sneak_level || d.great_weapon_fighting || a.light_damage ||
                          a.cleave_damage)
               ? detail::roll_damage_component(rng_, damage_dice,
                                               critical_hit(a, target, natural),
                                               weapon_die_rule(a, range))
               : spell ? this->spell_dice(a, damage_dice, critical_hit(a, target, natural, spell))
               : dice(damage_dice, critical_hit(a, target, natural, spell));
    };
    const bool bursting = spell && spell_attack->bursts;
    int weapon_damage =
        hit ? (bursting ? burst_damage(*spell_attack, critical_hit(a, target, natural, spell))
               : roll_damage())
        : 0;
    // Sneak Attack and Savage Attacker have one right answer, so they apply
    // automatically and the log records what they added.
    const bool cunning = sneak && cunning_ && cunning_->actor == a.source.id;
    const int sneak_damage = sneak ? roll_sneak_attack(a, target, natural,
                                 cunning ? cunning_->cost() : 0) : 0;
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
    if (hit && !spell && rage_of(a) && strength_attack(a, range))
    {
        weapon_damage += d.rage_damage;
        log("Rage adds " + std::to_string(d.rage_damage) + " damage.",
        {"Rage adds {damage} damage.", {{"damage", std::to_string(d.rage_damage)}}});
        weapon_damage += frenzy_damage(actor(a.source.id), critical_hit(a, target, natural));
    }
    if (hit && !spell)
        weapon_damage = resized_damage(a, weapon_damage);
    if (hit && brutal)
    {
        const int extra = dice({1, 10, 0}, critical_hit(a, target, natural));
        weapon_damage += extra;
        log("Brutal Strike adds " + std::to_string(extra) + " damage.",
        {"Brutal Strike adds {damage} damage.", {{"damage", std::to_string(extra)}}});
    }
    // Colossus Slayer: once per turn, 1d8 more on a creature already missing HP.
    if (hit && !spell && d.colossus_slayer && !a.colossus_used && target.life.hp < max_hp(target))
    {
        a.colossus_used = true;
        const int extra = dice({1, 8, 0}, critical_hit(a, target, natural));
        weapon_damage += extra;
        log("Colossus Slayer adds " + std::to_string(extra) + " damage.",
        {"Colossus Slayer adds {damage} damage.", {{"damage", std::to_string(extra)}}});
    }
    if (hit && charging)
    {
        const int extra = dice({1, 6, 0}, critical_hit(a, target, natural));
        weapon_damage += extra;
        log("The charge adds " + std::to_string(extra) + " damage.",
        {"The charge adds {damage} damage.", {{"damage", std::to_string(extra)}}});
    }
    // Ray of Enfeeblement: 1d8 less on each of the creature's damage rolls.
    const int enfeebled = hit && detail::has_effect(a.effects, detail::EffectKind::enfeebled)
                          ? roll(8)
                          : 0;
    if (enfeebled)
        log("Ray of Enfeeblement subtracts " + std::to_string(enfeebled) + " damage.",
        {"Ray of Enfeeblement subtracts {damage} damage.", {{"damage", std::to_string(enfeebled)}}});
    int rolled = std::max(0, weapon_damage + sneak_damage + advantage_damage - enfeebled);
    if (hit)
        rolled = std::max(0, rolled - cut_damage(a, target, rolled, damage_type));
    const int amount = hit ? dodged(target, deflected(a, target, rolled, damage_type, ranged)) : 0;
    apply_hit(a, target,
              ResolvedAttack{.natural = natural,
                             .bonus = bonus,
                             .mode = modifiers.mode(),
                             .damage = amount,
                             .type = damage_type,
                             .range = range,
                             .spell = spell,
                             .savage = savage});
    if (hit && charging && target.life.hp > 0 && !target.effects.prone)
    {
        target.effects.prone = true;
        log(target.source.name + " is knocked Prone.",
        {"{name} is knocked Prone.", {{"name", target.source.name}}});
    }
    // A ranged hit's added damage of a second type (the Hobgoblin Warrior's poison).
    if (hit && !spell && ranged && d.ranged_extra.count && !target.life.dead)
    {
        const auto type = d.ranged_extra_type;
        const int extra_roll = dice(d.ranged_extra, critical_hit(a, target, natural));
        const int extra = resolved_damage(target, type, std::max(0, extra_roll -
                                          cut_damage(a, target, extra_roll, type)));
        const auto name = std::string(detail::damage_name(type));
        log(target.source.name + " takes " + std::to_string(extra) + " " + name + " damage.",
        {
            "{name} takes {damage} {type} damage.",
            {{"name", target.source.name}, {"damage", std::to_string(extra)}, {"type", name, true}}
        });
        damage(target, extra, false);
    }
    // Hunter's Mark: any attack-roll hit on the caster's quarry deals 1d6 Force.
    if (hit && marked_by(target, a) && !target.life.dead)
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
    if (hit && hexed_by(target, a) && !target.life.dead)
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
            !target.life.dead)
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
    if (hit && amount > 0 && target.life.hp > 0)
        giant_boon(a, target, critical_hit(a, target, natural, spell));
    if (hit && brutal && !target.life.dead)
        brutal_blow(actor(a.source.id), target, brutal_->blow);
    if (cunning)
    {
        // Withdraw's move comes after the other effect.
        auto kinds = std::vector{cunning_->kind};
        if (cunning_->second)
            kinds.push_back(*cunning_->second);
        std::stable_partition(kinds.begin(), kinds.end(), [](auto kind)
        {
            return kind != CunningStrike::withdraw;
        });
        for (const auto kind : kinds)
            cunning_effect(actor(a.source.id), target, kind);
    }
    else if (stealthy)
        end_hiding(a.source.id);
    if (hit && stunning_ == a.source.id && !spell && !ranged && d.martial_arts &&
            !actor(a.source.id).stun_used && actor(a.source.id).surges > 0 && !target.life.dead &&
            target.life.hp > 0)
        stunning_strike(actor(a.source.id), target);
    // Hellish Rebuke and Storm's Thunder answer the damage once the attack is done.
    answer_damage();
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
            if (other.life.hp > 0 && !other.life.dead)
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
            if (other.life.hp > 0 && !other.life.dead)
                return false;
            marked = true;
        }
    return marked;
}

void Session::strike_true(Actor &a, Actor &target, TrueStrikeDamage choice)
{
    // The attack uses the spellcasting modifier for its attack and damage
    // rolls; `casting` is that modifier plus the +2 Proficiency Bonus.
    auto striker = a;
    auto &d = striker.definition;
    const int modifier = casting_bonus(d, "true_strike") - 2;
    d.melee_bonus += modifier - d.melee_ability;
    d.melee_ability = modifier;
    d.melee.bonus = modifier;
    if (choice == TrueStrikeDamage::radiant)
        d.melee_type = detail::DamageType::radiant;
    log(a.source.name + " casts True Strike.",
    {"{name} casts {spell}.", {{"name", a.source.name}, {"spell", "True Strike", true}}});
    attack(striker, target, AttackRange::melee);
    a.aim_ready = striker.aim_ready;
}

std::optional<Session::ReadyMetamagic> Session::readied_metamagic(const Actor &a)
{
    for (const auto &e : a.effects.active)
        if (e.kind == detail::EffectKind::metamagic)
        {
            if (e.dc < 10)
                return ReadyMetamagic{static_cast<Metamagic>(e.dc), std::nullopt};
            return ReadyMetamagic{Metamagic::transmuted,
                                  detail::damage_type(transmuted_types.at(
                                      static_cast<std::size_t>(e.dc - 10)))};
        }
    return std::nullopt;
}

bool Session::readies(const Actor &a, Metamagic option, const detail::SpellDef &spell)
{
    const auto ready = readied_metamagic(a);
    return ready && ready->option == option && metamagic_applies(option, spell) &&
           a.lay_on_hands >= metamagic_cost(option);
}

void Session::mark_heightened(const Actor &caster, const detail::SpellDef &spell)
{
    // A new cast of the spell replaces the old one's mark.
    const int index = detail::spell_index(spell.id);
    for (auto &other : actors_)
        std::erase_if(other.effects.active, [&](const auto & e)
    {
        return e.kind == detail::EffectKind::heightened && e.source_scope == scope_ &&
               e.source_actor == caster.source.id && e.dc == index;
    });
    if (!heightened_target_)
        return;
    auto &target = actor(heightened_target_);
    if (detail::can_apply(target.effects))
        detail::apply_spell_benefit(target.effects, effect_source(caster), caster.source.name,
                                    detail::EffectKind::heightened, index);
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
    const std::string label(metamagic_labels[static_cast<std::size_t>(ready->option)]);
    log(caster.source.name + " uses " + label + ".",
    {"{name} uses {feature}.", {{"name", caster.source.name}, {"feature", label, true}}});
}

int Session::spell_range(const Actor &caster, const detail::SpellDef &spell) const
{
    // Eldritch Spear: 30 feet more per Warlock level for its cantrip.
    const int range = spell.range + (def(caster).eldritch_spear.contains(spell.id)
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
    for (int n = 0;
            n < rerolls && n < static_cast<int>(rolls.size()) && rolls[n] * 2 < dice.sides + 1;
            ++n)
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
    const auto most = static_cast<std::size_t>(std::max(1, def(caster).casting - 2));
    for (const auto &other : actors_)
        if (spared.size() < most && other.source.side == caster.source.side &&
                other.source.id != caster.source.id && !other.life.dead &&
                std::find(cells.begin(), cells.end(), other.source.cell) != cells.end())
            spared.push_back(other.source.id);
    return spared;
}

int Session::burst_damage(const SpellAttack &spell, bool critical)
{
    // Each die showing its maximum adds another, up to `bursts` extra dice.
    const auto &dice = spell.dice;
    const int count = dice.count * (critical ? 2 : 1);
    int total = dice.bonus, extra = 0;
    for (int n = 0; n < count + extra; ++n)
    {
        const int rolled = roll(dice.sides);
        total += rolled;
        if (rolled == dice.sides && extra < spell.bursts)
            ++extra;
    }
    return total;
}

int Session::resized_damage(const Actor &a, int amount)
{
    // Enlarge: 1d4 more weapon damage. Reduce: 1d4 less, but not below 1.
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
    detail::apply_poisoned(a.effects, effect_source(a), a.source.name, next_turn_ms(a),
                           detail::EffectKind::reckless);
    log(a.source.name + " attacks recklessly.",
    {"{name} attacks recklessly.", {{"name", a.source.name}}});
}

void Session::stunning_strike(Actor &monk, Actor &target)
{
    monk.stun_used = true;
    --monk.surges;
    log(monk.source.name + " attempts a Stunning Strike.",
    {"{name} attempts a Stunning Strike.", {{"name", monk.source.name}}});
    if (!saving_throw_succeeds(target, detail::Ability::constitution, def(monk).focus_dc))
    {
        detail::apply_poisoned(target.effects, effect_source(monk), monk.source.name,
                               next_turn_ms(monk), detail::EffectKind::stunned);
        log(target.source.name + " is Stunned.", {"{name} is Stunned.", {{"name", target.source.name}}});
        return;
    }
    // Resisted: Speed halved, and the next attack roll against it has Advantage.
    detail::apply_poisoned(target.effects, effect_source(monk), monk.source.name,
                           next_turn_ms(monk), detail::EffectKind::staggered);
    detail::apply_guiding_bolt(target.effects, effect_source(monk), monk.source.name,
                               next_turn_ms(monk));
    log(target.source.name + " is staggered: Speed halved, and the next attack against it has Advantage.",
    {
        "{name} is staggered: Speed halved, and the next attack against it has Advantage.",
        {{"name", target.source.name}}
    });
}

std::optional<std::pair<CunningStrike, std::optional<CunningStrike>>>
Session::cunning_choice(std::string_view verb)
{
    for (const std::string_view base : {"melee_", "ranged_"})
    {
        if (!verb.starts_with(base))
            continue;
        const auto rest = verb.substr(base.size());
        const auto plus = rest.find('+');
        const auto named = [](std::string_view suffix) -> std::optional<CunningStrike>
        {
            for (const auto &option : cunning_strikes)
                if (option.suffix == suffix)
                    return option.kind;
            return std::nullopt;
        };
        const auto first = named(rest.substr(0, plus));
        if (!first)
            return std::nullopt;
        if (plus == std::string_view::npos)
            return std::pair{*first, std::optional<CunningStrike>{}};
        const auto second = named(rest.substr(plus + 1));
        if (!second)
            return std::nullopt;
        return std::pair{*first, second};
    }
    return std::nullopt;
}

void Session::cunning_effect(Actor &rogue, Actor &target, CunningStrike kind)
{
    const auto &d = def(rogue);
    if (kind == CunningStrike::stealth)
    {
        log(rogue.source.name + " stays hidden (Stealth Attack).",
        {"{name} stays hidden (Stealth Attack).", {{"name", rogue.source.name}}});
        return;
    }
    // The saves are against 8 + Dexterity + Proficiency Bonus.
    const int dc = 8 + d.dexterity + 2 + (d.level - 1) / 4;
    const bool standing = !target.life.dead && target.life.hp > 0;
    if (kind == CunningStrike::daze)
    {
        if (!standing || saving_throw_succeeds(target, detail::Ability::constitution, dc))
            return;
        detail::apply_poisoned(target.effects, effect_source(rogue), rogue.source.name,
                               next_save_ms(target.source.id), detail::EffectKind::dazed);
        log(target.source.name + " is dazed.", {"{name} is dazed.", {{"name", target.source.name}}});
        return;
    }
    if (kind == CunningStrike::knock_out)
    {
        if (!standing || saving_throw_succeeds(target, detail::Ability::constitution, dc))
            return;
        detail::apply_repeating_condition(
            target.effects, detail::EffectKind::knocked_out, effect_source(rogue),
            rogue.source.name,
            detail::RepeatingSave{.dc = dc, .first_save_ms = next_save_ms(target.source.id)});
        target.effects.prone = true;
        log(target.source.name + " is knocked out.",
        {"{name} is knocked out.", {{"name", target.source.name}}});
        end_incapacitated_effects(target);
        return;
    }
    if (kind == CunningStrike::obscure)
    {
        if (!standing || saving_throw_succeeds(target, detail::Ability::dexterity, dc))
            return;
        detail::apply_poisoned(target.effects, effect_source(rogue), rogue.source.name,
                               next_save_ms(target.source.id), detail::EffectKind::dazzled);
        log(target.source.name + " is Blinded.", {"{name} is Blinded.", {{"name", target.source.name}}});
        return;
    }
    if (kind == CunningStrike::trip)
    {
        // A Large or smaller creature saves or falls Prone.
        if (!standing || def(target).size > 3 || target.effects.prone)
            return;
        if (saving_throw_succeeds(target, detail::Ability::dexterity, dc))
            return;
        target.effects.prone = true;
        log(target.source.name + " is tripped and falls Prone.",
        {"{name} is tripped and falls Prone.", {{"name", target.source.name}}});
        return;
    }
    // Withdraw: up to half Speed without provoking Opportunity Attacks.
    if (const int feet = std::max(0, d.speed - detail::speed_penalty(rogue.effects)) / 2;
            feet > 0 && conscious(rogue) && !champion_move_)
    {
        champion_move_ = ChampionMove{.actor = rogue.source.id, .target = rogue.source.id,
                                      .remaining = feet, .origin = rogue.source.cell,
                                      .withdraw = true};
        log(rogue.source.name + " may withdraw " + std::to_string(feet) +
            " feet without provoking Opportunity Attacks.",
        {
            "{name} may withdraw {feet} feet without provoking Opportunity Attacks.",
            {{"name", rogue.source.name}, {"feet", std::to_string(feet)}}
        });
    }
}

void Session::brutal_blow(Actor &barbarian, Actor &target, BrutalBlow blow)
{
    if (blow == BrutalBlow::staggering || blow == BrutalBlow::sundering)
    {
        const bool staggering = blow == BrutalBlow::staggering;
        detail::apply_brutal_blow(target.effects,
                                  staggering ? detail::EffectKind::staggering
                                  : detail::EffectKind::sundered,
                                  effect_source(barbarian), barbarian.source.name,
                                  next_turn_ms(barbarian));
        if (staggering)
            log(target.source.name + " staggers (Staggering Blow).",
            {"{name} staggers (Staggering Blow).", {{"name", target.source.name}}});
        else
            log(target.source.name + " is sundered (Sundering Blow).",
            {"{name} is sundered (Sundering Blow).", {{"name", target.source.name}}});
        return;
    }
    if (blow == BrutalBlow::hamstring)
    {
        detail::apply_hamstring(target.effects, effect_source(barbarian), barbarian.source.name,
                                next_turn_ms(barbarian));
        log(target.source.name + "'s Speed drops by 15 feet (Hamstring Blow).",
        {"{name}'s Speed drops by 15 feet (Hamstring Blow).", {{"name", target.source.name}}});
        return;
    }
    push_away(barbarian, target, 3);
    // Then up to half Speed straight toward the target, provoking nothing.
    if (const int feet = std::max(0, def(barbarian).speed -
                                  detail::speed_penalty(barbarian.effects)) / 2;
            feet > 0 && conscious(barbarian) && !champion_move_ && !target.life.dead)
        champion_move_ = ChampionMove{.actor = barbarian.source.id,
                                      .target = target.source.id,
                                      .remaining = feet,
                                      .origin = barbarian.source.cell,
                                      .toward = target.source.id};
}

int Session::reliable(const Actor &a, std::string_view skill, int natural)
{
    if (natural >= 10 || !def(a).reliable.contains(skill))
        return natural;
    log("Reliable Talent: " + a.source.name + "'s d20 of " + std::to_string(natural) +
        " counts as 10.",
    {
        "Reliable Talent: {name}'s d20 of {roll} counts as 10.",
        {{"name", a.source.name}, {"roll", std::to_string(natural)}}
    });
    return 10;
}

int Session::evaded(const Actor &target, detail::Ability save, bool halves, bool saved,
                    int total) const
{
    if (def(target).evasion && save == detail::Ability::dexterity && halves &&
            !detail::incapacitated(target.effects))
        return saved ? 0 : total / 2;
    return saved ? total / 2 : total;
}

bool Session::mindless(const Actor &a) const
{
    return def(a).mindless_rage && rage_of(a);
}

bool Session::strength_attack(const Actor &a, AttackRange range) const
{
    // A melee attack without a weapon is an Unarmed Strike, always Strength;
    // a ranged one is Strength only when thrown.
    const auto &d = def(a);
    if (range == AttackRange::ranged)
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
    // The Charisma modifier, at least +1, while the weapon is imbued.
    return detail::has_effect(a.effects, detail::EffectKind::sacred_weapon)
           ? std::max(1, def(a).casting - 2) : 0;
}

detail::DamageType Session::sacred_damage_type(const Actor &a) const
{
    if (pact_damage_)
        return *pact_damage_;
    // The damage the Paladin chose when imbuing the weapon.
    for (const auto &effect : a.effects.active)
        if (effect.kind == detail::EffectKind::sacred_weapon && effect.dc == 1)
            return detail::DamageType::radiant;
    return def(a).melee_type;
}

void Session::finish_reaction()
{
    ++reactor_index_;
    update_outcome();
    if (outcome_ == Outcome::ongoing)
    {
        if (mover().life.hp == 0)
        {
            path_.clear();
            path_index_ = 0;
            reactors_.clear();
            reactor_index_ = 0;
            flight_.reset();
        }
        else if (!pending() && !stop_illegal_route())
            progress_movement();
    }
}

void Session::update_outcome()
{
    // Decided once: a later call must not announce the outcome again.
    if (outcome_ != Outcome::ongoing)
        return;
    bool party = false, enemies = false;
    for (const auto &a : actors_)
        if (a.life.hp > 0 && !a.life.dead && !a.fled)
            (a.source.side == Side::party ? party : enemies) = true;
    if (!party || !enemies)
    {
        // No member left on the field: it fled if anyone got away.
        const bool party_fled =
            std::any_of(fled_.begin(), fled_.end(), [](const auto & gone)
        {
            return gone.source.side == Side::party;
        }) || std::any_of(actors_.begin(), actors_.end(), [](const auto & a)
        {
            return a.fled && a.source.side == Side::party;
        });
        outcome_ = party ? Outcome::victory : party_fled ? Outcome::fled : Outcome::defeat;
        // Concentration is tracked in combat only; it ends with the combat, and
        // so does Wild Shape (WILD-1).
        for (auto &a : actors_)
        {
            end_concentration(a);
            end_wild_shape(a);
        }
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
        {
            // The original game ends the fight once every enemy is down; a
            // downed troll does not get up afterwards.
            for (auto &a : actors_)
                if (a.source.side == Side::opposition && downed_riser(a))
                {
                    a.life.dead = true;
                    a.life.recovery = {};
                    a.rises_on_round = 0;
                    log(a.source.name + " stays down.",
                    {"{name} stays down.", {{"name", a.source.name}}});
                }
            bandage_after_victory();
        }
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

// As in the original game, whoever is still dying when the fight is won has
// its wounds bound afterward: it is Stable and recovers on the usual clock.
// Prone does not outlast combat: the party is standing (or lying unconscious)
// when exploration resumes.
void Session::bandage_after_victory()
{
    for (auto &a : actors_)
    {
        if (a.life.hp != 0 || a.life.dead || a.life.stable)
            continue;
        detail::stabilize(a.life, rng_);
        log(a.source.name + " is bandaged and stable.",
        {"{name} is bandaged and stable.", {{"name", a.source.name}}});
    }
    for (auto &a : actors_)
        a.effects.prone = false;
}

// Regeneration at the start of the creature's turn: Acid or Fire damage since its
// last turn stops it, and a creature at 0 HP that cannot regenerate dies.
void Session::regenerate(Actor &a)
{
    const bool blocked = a.regeneration_blocked;
    a.regeneration_blocked = false;
    if (blocked)
    {
        if (a.life.hp > 0)
        {
            log(a.source.name + " cannot regenerate this turn.",
            {"{name} cannot regenerate this turn.", {{"name", a.source.name}}});
            return;
        }
        a.life.dead = true;
        a.life.stable = false;
        a.life.recovery = {};
        a.rises_on_round = 0;
        log(a.source.name + " cannot regenerate and dies.",
        {"{name} cannot regenerate and dies.", {{"name", a.source.name}}});
        clear_departed_overlaps();
        return;
    }
    if (a.life.hp == 0)
    {
        get_up(a);
        return;
    }
    const int healed = detail::heal_life(a.life, def(a).regeneration, max_hp(a),
                                         !detail::healing_blocked(a.effects));
    if (!healed)
        return;
    log(a.source.name + " regenerates " + std::to_string(healed) + " Hit Points.",
    {
        "{name} regenerates {amount} Hit Points.",
        {{"name", a.source.name}, {"amount", std::to_string(healed)}}
    });
}

// As in the original game, a downed creature with Regeneration gets back up with
// all its Hit Points once its 3d6 rounds have passed, but not while a creature
// stands on its square; it tries again on each of its turns.
void Session::get_up(Actor &a)
{
    const bool stood_on = std::any_of(actors_.begin(), actors_.end(), [&](const auto & other)
    {
        return other.source.id != a.source.id && conscious(other) &&
               other.source.cell == a.source.cell;
    });
    if (static_cast<int>(round_) < a.rises_on_round || stood_on)
        return;
    if (!detail::heal_life(a.life, max_hp(a), max_hp(a), !detail::healing_blocked(a.effects)))
        return;
    a.rises_on_round = 0;
    a.effects.prone = true;
    if (shares_occupied_space(a))
        a.involuntary_overlap = true;
    log(a.source.name + " gets back up.", {"{name} gets back up.", {{"name", a.source.name}}});
}

bool Session::begin_turn()
{
    auto &a = actors_[turn_];
    if (a.fled)
        return false;
    if (a.source.side != Side::party && conscious(a))
    {
        check_morale(a);
        if (a.fled)
            return false;
    }
    std::erase_if(a.effects.active,
                  [](const auto & effect)
    {
        return effect.kind == detail::EffectKind::shocking_grasp;
    });
    if (a.life.dead)
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
    if (a.life.dead)
        return false;
    spring_zones(a, ZoneKind::web);
    // Heroism: Temporary HP at the start of each of the target's turns, kept
    // only when higher than what the target already has.
    for (const auto &effect : a.effects.active)
        if (effect.kind == detail::EffectKind::heroism && a.life.hp > 0 &&
                effect.dc > a.life.temporary_hp.amount)
            detail::grant_temporary_hp(a.life, {effect.dc, "spell:heroism"},
                                       TemporaryHpChoice::use_new);
    // Persistent Rage (Barbarian 15): the Rage needs no extending.
    if (def(a).persistent_rage && rage_of(a))
        extend_rage(a);
    // Heroic Warrior (Champion 10): Heroic Inspiration at the start of each
    // turn in a fight, when the Fighter lacks it.
    if (def(a).heroic_warrior && conscious(a) &&
            !detail::has_effect(a.effects, detail::EffectKind::heroic_inspiration) &&
            detail::can_apply(a.effects))
    {
        detail::apply_spell_benefit(a.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::heroic_inspiration, 0);
        log(a.source.name + " gains Heroic Inspiration (Heroic Warrior).",
        {"{name} gains Heroic Inspiration (Heroic Warrior).", {{"name", a.source.name}}});
    }
    // Burning: 1d4 Fire damage at the start of each turn until put out.
    if (a.burning && !a.life.dead)
    {
        const int amount = resolved_damage(a, detail::DamageType::fire, roll(4));
        log(a.source.name + " burns for " + std::to_string(amount) + " Fire damage.",
        {"{name} burns for {damage} Fire damage.", {{"name", a.source.name}, {"damage", std::to_string(amount)}}});
        damage(a, amount, false);
        if (a.life.dead)
            a.burning = false;
    }
    if (def(a).regeneration)
    {
        regenerate(a);
        if (a.life.hp == 0)
            return false;
    }
    if (a.life.hp == 0)
    {
        if (!a.life.stable)
        {
            const int aura = aura_of_protection(a);
            const int result = detail::death_save(a.life, rng_, !detail::healing_blocked(a.effects),
                                                  def(a).lucky, aura);
            if (aura)
                log(a.source.name + " death save: " + std::to_string(result) + " + " +
                    std::to_string(aura) + " (Aura of Protection)",
            {
                "{name} death save: {roll} + {bonus} (Aura of Protection)",
                {   {"name", a.source.name},
                    {"roll", std::to_string(result)},
                    {"bonus", std::to_string(aura)}
                }
            });
            else
                log(a.source.name + " death save: " + std::to_string(result),
            {
                "{name} death save: {roll}",
                {{"name", a.source.name}, {"roll", std::to_string(result)}}
            });
            if (a.life.hp > 0)
            {
                a.effects.prone = true;
                if (shares_occupied_space(a))
                    a.involuntary_overlap = true;
            }
            if (a.life.dead)
                clear_departed_overlaps();
        }
        if (a.life.hp == 0)
            return false;
    }
    // One spell slot per turn, whoever's turn it is: a Reaction spell on
    // another creature's turn may spend a slot again.
    for (auto &actor : actors_)
    {
        actor.spent_slot = false;
        actor.cleave_used = false;
        actor.colossus_used = actor.horde_used = actor.resistance_used = false;
        actor.horde_origin = 0;
        actor.savage_used = false;
        actor.sneak_used = false;
        actor.aim_used = actor.aim_ready = false;
        actor.moved = false;
        actor.attacked = false;
        actor.pounce = 0;
        actor.brutal_used = false;
        actor.stun_used = false;
        actor.fleet_step = false;
        actor.carried = 0;
        actor.run_feet = 0;
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
    a.rush_used = false;
    // Command's Grovel and Halt take the whole turn; Approach and Flee limit
    // the turn's commands instead (obey_command).
    if (const auto *command = detail::command_effect(a.effects))
    {
        if (command->dc == static_cast<int>(detail::CommandOption::grovel))
        {
            a.effects.prone = true;
            log(a.source.name + " grovels.", {"{name} grovels.", {{"name", a.source.name}}});
            return false;
        }
        if (command->dc == static_cast<int>(detail::CommandOption::halt))
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
    const auto index = index_of(target.source.id);
    const unsigned start = index ? turn_end_ms(index - 1) : 0,
                   current = turn_ ? turn_end_ms(turn_ - 1) : 0;
    return start > current ? start - current : detail::round_ms - current + start;
}

unsigned Session::next_save_ms(EntityId target) const
{
    const auto end = turn_end_ms(index_of(target));
    const auto start = turn_ ? turn_end_ms(turn_ - 1) : 0;
    return end > start ? end - start : detail::round_ms - start + end;
}

void Session::log_luck(const Actor &a, bool rerolled)
{
    if (rerolled)
        log(a.source.name + " rolls a 1 and rerolls it with Luck.",
        {"{name} rolls a 1 and rerolls it with Luck.", {{"name", a.source.name}}});
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
                                    bool advantage, bool disadvantage)
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
    if (detail::has_effect(target.effects, detail::EffectKind::stunned) &&
            (ability == detail::Ability::strength || ability == detail::Ability::dexterity))
    {
        const std::string name = ability == detail::Ability::strength ? "Strength" : "Dexterity";
        log(target.source.name + " automatically fails the " + name + " save while Stunned.",
        {
            "{name} automatically fails the {ability} save while Stunned.",
            {{"name", target.source.name}, {"ability", name, true}}
        });
        return false;
    }
    if ((detail::has_effect(target.effects, detail::EffectKind::asleep) ||
            detail::has_effect(target.effects, detail::EffectKind::knocked_out)) &&
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
                      (detail::has_effect(target.effects, detail::EffectKind::warding_bond) ? 1 : 0) +
                      aura_of_protection(target);
    auto modifiers =
        detail::saving_modifiers(ability, def(target).str_dex_disadvantage, target.dodge);
    modifiers.advantage |= advantage;
    // Heightened Spell: Disadvantage on the save against the spell.
    modifiers.disadvantage |= heightened_target_ == target.source.id || disadvantage;
    // Staggering Blow: Disadvantage on the next saving throw.
    modifiers.disadvantage |= detail::spend_staggering(actor(target.source.id).effects);
    // Gnomish Cunning: Advantage on Intelligence, Wisdom and Charisma saves.
    modifiers.advantage |= def(target).gnomish_cunning && detail::mental(ability);
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
    modifiers.lucky = def(target).lucky;
    bool lucky_reroll = false;
    auto result = detail::saving_throw(
                      ability, def(target).saves[static_cast<unsigned>(ability)] + bless, dc,
                      modifiers, rng_, &lucky_reroll);
    log_luck(target, lucky_reroll);
    log_save(target, result);
    // Heroic Inspiration rolls a failed save again, if a 20 could pass it.
    if (!result.success && result.bonus + 20 >= dc)
        if (const auto again = heroic_reroll(actor(target.source.id)))
        {
            result.natural = *again;
            result.success = result.natural + result.bonus >= dc;
            log_save(target, result);
        }
    // Indomitable: the Fighter rerolls a failed save it could still pass,
    // adding its Fighter level, without asking, as Heroic Inspiration does.
    if (auto &fighter = actor(target.source.id);
            !result.success && def(fighter).indomitable && fighter.arcane > 0 &&
            conscious(fighter) && result.bonus + def(fighter).level + 20 >= dc)
    {
        --fighter.arcane;
        result.natural = roll(20);
        result.bonus += def(fighter).level;
        result.success = result.natural + result.bonus >= dc;
        log(fighter.source.name + " uses Indomitable and rolls the save again.",
        {"{name} uses Indomitable and rolls the save again.", {{"name", fighter.source.name}}});
        log_save(target, result);
    }
    // Disciplined Survivor: the Monk spends a Focus Point to reroll a failed
    // save a 20 could pass, without asking (LEVELS-2).
    if (auto &monk = actor(target.source.id);
            !result.success && def(monk).disciplined_survivor && monk.surges > 0 &&
            conscious(monk) && result.bonus + 20 >= dc)
    {
        --monk.surges;
        result.natural = roll(20);
        result.success = result.natural + result.bonus >= dc;
        log(monk.source.name + " spends 1 Focus Point and rolls the save again (Disciplined Survivor).",
        {
            "{name} spends 1 Focus Point and rolls the save again (Disciplined Survivor).",
            {{"name", monk.source.name}}
        });
        log_save(target, result);
    }
    // Bardic Inspiration may turn the failure into a success.
    if (!result.success)
        if (const int die = inspiration_roll(target, dc - result.natural - result.bonus))
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
        detail::start_stable_recovery(a.life, rng_);
    std::vector<detail::RecoverySubject> subjects;
    std::vector<EntityId> unconscious;
    // Acid Arrow burns once its effect runs out, after the effects are walked.
    std::vector<EntityId> burned;
    for (auto &a : actors_)
    {
        auto saves = def(a).saves;
        for (auto &save : saves)
            save += aura_of_protection(a);
        subjects.push_back(
        {
            {   a.source.id, a.effects, saves, a.life.dead, def(a).str_dex_disadvantage, a.dodge,
                def(a).gnomish_cunning, def(a).lucky, def(a).brave
            },
            a.life});
        if (a.life.hp == 0)
            unconscious.push_back(a.source.id);
    }
    detail::elapse_recovery(
        subjects, delta, rng_, detail::RecoveryMode::combat,
        [&](const detail::EffectEvent & event)
    {
        const auto &target = actor(event.target);
        if (event.save)
            log_save(target, *event.save);
        if (event.inspiration)
            log_inspiration(target, event.inspiration, event.save->success);
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
        if (target.life.dead)
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
        if (rage_of(a) && (detail::incapacitated(a.effects) || a.life.hp == 0))
            std::erase_if(a.effects.active, [](const auto & e)
        {
            return e.kind == detail::EffectKind::raging;
        });
    // An ended Aid lowers the maximum, and Hit Points above it with it.
    for (auto &a : actors_)
        a.life.hp = std::min(a.life.hp, max_hp(a));
    for (auto &a : actors_)
        if (a.life.hp > 0 &&
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

void Session::restore_self(Actor &a)
{
    // Self-Restoration (Monk 10): one of Charmed, Frightened or Poisoned ends.
    if (!def(a).self_restoration || !conscious(a))
        return;
    for (const auto &[kind, name] : {std::pair{detail::EffectKind::charmed, "Charmed"},
                                     std::pair{detail::EffectKind::frightened, "Frightened"},
                                     std::pair{detail::EffectKind::poisoned, "Poisoned"}})
    {
        const auto found = std::find_if(a.effects.active.begin(), a.effects.active.end(),
                                        [&](const auto & e)
        {
            return e.kind == kind;
        });
        if (found == a.effects.active.end())
            continue;
        a.effects.active.erase(found);
        log(a.source.name + " is no longer " + name + " (Self-Restoration).",
        {
            "{name} is no longer {condition} (Self-Restoration).",
            {{"name", a.source.name}, {"condition", name, true}}
        });
        return;
    }
}

void Session::end_turn()
{
    actors_[turn_].actions.surge = false;
    // A readied Metamagic option lapses with the turn.
    std::erase_if(actors_[turn_].effects.active, [](const auto & e)
    {
        return e.kind == detail::EffectKind::metamagic;
    });
    restore_self(actors_[turn_]);
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
    auto &a = mover();
    const auto grid = movement_grid(a);
    while (path_index_ < path_.size() && a.life.hp > 0)
    {
        const auto destination = path_[path_index_];
        if (reactors_.empty() && !a.disengaged)
            for (const auto &other : actors_)
                if (provokes(other, a, a.source.cell, destination))
                    reactors_.push_back(other.source.id);
        if (pending())
            return;
        if (!board_.contains(destination))
        {
            a.movement -= 5;
            ++path_index_;
            reactors_.clear();
            reactor_index_ = 0;
            run_off_field(a);
            break;
        }
        const auto cost = grid.step_cost(a.source.cell, destination);
        if (!cost || *cost > mover_budget(a))
            throw std::logic_error("Invalid accepted movement path");
        const auto departed = a.source.cell;
        a.source.cell = destination;
        follow_carrier(a, departed);
        // A flight spends the Reaction, not the creature's own movement.
        if (flight_)
            flight_->remaining -= *cost;
        else
        {
            a.movement -= *cost;
            a.pounce = std::max(0, a.pounce - *cost);
            a.moved = true;
            extend_run(a, departed, destination);
        }
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
    flight_.reset();
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
    // A command applies completely or not at all: an exception part way
    // through restores the session as it was (Effective C++ Item 29). The
    // restore moves, so it cannot itself fail part way.
    static_assert(std::is_nothrow_move_assignable_v<Session>);
    Session before = *this;
    try
    {
        if (reaction_prompt_)
            answer_reaction(command);
        else
            perform(command);
        notice_hidden();
        // Revisions are command tickets; zero is reserved for invalid commands.
        // Unsigned wrap is defined, but must skip that reserved value.
        if (++revision_ == 0)
            revision_ = 1;
        update_outcome();
        // One who ran off the field ends its turn and leaves the fight.
        if (outcome_ == Outcome::ongoing && !pending() && !reaction_prompt_ && actors_[turn_].fled)
            end_turn();
        remove_fled();
        update_outcome();
        if (initiative_choices_.empty() && outcome_ == Outcome::ongoing && !pending() &&
                !reaction_prompt_ && !champion_move_ && !effect_waiting() && actors_[turn_].life.hp == 0)
        {
            end_turn();
            // A creature whose morale broke as its turn began leaves now too.
            remove_fled();
            update_outcome();
        }
        if (outcome_ != Outcome::ongoing)
            advance_turn_time();
    }
    catch (...)
    {
        *this = std::move(before);
        throw;
    }
    return true;
}

const Session::ReactionAnswer *Session::answer_of(EntityId target, Asked asked) const
{
    for (const auto &answer : reaction_answers_)
        if (answer.target == target && answer.asked == asked)
            return &answer;
    return nullptr;
}

// Shield takes a slot, unless Magic Initiate's free cast pays for it.
bool Session::can_shield(const Actor &target) const
{
    const bool slot = lowest_slot(target, 1) && !target.spent_slot;
    const bool free = (target.initiate_casts & initiate_bit(def(target), "shield")) != 0;
    return conscious(target) && target.reaction && (slot || free) &&
           detail::knows_spell(def(target).spells, "shield") &&
           !detail::incapacitated(target.effects) &&
           !detail::has_effect(target.effects, detail::EffectKind::shield);
}

bool Session::can_dodge(const Actor &target) const
{
    // Uncanny Dodge: the attacker must be seen; a Blinded Rogue sees no one.
    return def(target).uncanny_dodge && conscious(target) && target.reaction &&
           !detail::incapacitated(target.effects) && !detail::blinded(target.effects);
}

bool Session::can_deflect(const Actor &target, bool deflectable) const
{
    return (deflectable || def(target).deflect_energy) && def(target).deflect &&
           conscious(target) && target.reaction &&
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
        asked == Asked::redirect || asked == Asked::rebuke || asked == Asked::boon ||
        asked == Asked::endure || asked == Asked::thunder || asked == Asked::retaliate ||
        (can_shield(target) && !critical) ||
        (asked == Asked::hit && (can_deflect(target, deflectable) || can_dodge(target)));
    if (askable)
        throw ReactionQuestion{target.source.id, asked, deflectable, critical};
}

int Session::dodged(Actor &target, int amount)
{
    const auto found = std::find_if(reaction_answers_.begin(), reaction_answers_.end(),
                                    [&](const auto & answer)
    {
        return answer.target == target.source.id && answer.asked == Asked::hit &&
               answer.verb == "uncanny_dodge";
    });
    if (found == reaction_answers_.end())
        return amount;
    // Spent once: a later hit in the same command is taken in full.
    found->verb = "dodged";
    log(target.source.name + " uses Uncanny Dodge and takes half the damage.",
    {"{name} uses Uncanny Dodge and takes half the damage.", {{"name", target.source.name}}});
    return amount / 2;
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
    if (left || target.surges <= 0 || attacker.life.hp <= 0 ||
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
        if (!amount || warlock.life.hp <= 0 || warlock.life.dead ||
                warlock.source.side == fallen.source.side ||
                (by != warlock.source.id && distance(warlock.source.cell, fallen.source.cell) > 10) ||
                amount <= warlock.life.temporary_hp.amount)
            continue;
        detail::grant_temporary_hp(warlock.life, {amount, "feature:dark_ones_blessing"},
                                   TemporaryHpChoice::use_new);
        log("Dark One's Blessing: " + warlock.source.name + " gains " + std::to_string(amount) +
            " Temporary Hit Points.",
        {
            "Dark One's Blessing: {name} gains {amount} Temporary Hit Points.",
            {{"name", warlock.source.name}, {"amount", std::to_string(amount)}}
        });
    }
}

int Session::inspiration_roll(const Actor &creature, int shortfall)
{
    // Used without asking, and only when the die could make up the shortfall:
    // the roll and its target number are in the log for the player too.
    auto &inspired = actor(creature.source.id);
    const auto found = std::find_if(inspired.effects.active.begin(), inspired.effects.active.end(),
                                    [](const auto & e)
    {
        return e.kind == detail::EffectKind::inspired;
    });
    if (found == inspired.effects.active.end() || !conscious(inspired) || found->dc < shortfall)
        return 0;
    const int sides = found->dc;
    inspired.effects.active.erase(found);
    const int die = roll(sides);
    log_inspiration(inspired, die, die >= shortfall);
    return die;
}

void Session::log_inspiration(const Actor &inspired, int die, bool success)
{
    const auto amount = std::to_string(die);
    log("Bardic Inspiration adds " + amount + " to " + inspired.source.name + "'s roll" +
        (success ? ", turning it into a success." : ", not enough to succeed."),
    {
        success ? "Bardic Inspiration adds {amount} to {name}'s roll, turning it into a success."
        : "Bardic Inspiration adds {amount} to {name}'s roll, not enough to succeed.",
        {{"amount", amount}, {"name", inspired.source.name}}
    });
}

Actor *Session::cutting_bard(const Actor &roller)
{
    // A Lore Bard who sees the roller within 60 feet spends a Reaction and a
    // Bardic Inspiration use to subtract the die from the roll.
    for (auto &bard : actors_)
        if (def(bard).cutting_words && bard.source.side != roller.source.side &&
                conscious(bard) && bard.reaction && !detail::incapacitated(bard.effects) &&
                bard.arcane > 0 && distance(bard.source.cell, roller.source.cell) <= 60 &&
                can_see(bard, roller))
            return &bard;
    return nullptr;
}

int Session::cutting_words(const Actor &roller, int margin, CutRoll cut)
{
    // It is used without asking, and only when the d6 could turn the success
    // into a failure.
    if (margin >= 6)
        return 0;
    auto *const bard = cutting_bard(roller);
    if (!bard)
        return 0;
    bard->reaction = false;
    --bard->arcane;
    const int die = roll(6);
    const bool failed = die > margin;
    const auto amount = std::to_string(die);
    const bool attack = cut == CutRoll::attack;
    const std::string outcome =
        attack ? (failed ? ", turning the hit into a miss." : "; the attack still hits.")
        : (failed ? ", turning the success into a failure." : "; the check still succeeds.");
    log(bard->source.name + " uses Cutting Words: -" + amount + outcome,
    {
        attack ? (failed ? "{name} uses Cutting Words: -{amount}, turning the hit into a miss."
                  : "{name} uses Cutting Words: -{amount}; the attack still hits.")
        : (failed ? "{name} uses Cutting Words: -{amount}, turning the success into a failure."
           : "{name} uses Cutting Words: -{amount}; the check still succeeds."),
        {{"name", bard->source.name}, {"amount", amount}}
    });
    return die;
}

int Session::cut_damage(const Actor &roller, const Actor &target, int amount,
                        detail::DamageType type)
{
    // Used without asking, and only when the roll would drop the target to 0
    // Hit Points and the d6 could keep it standing (CUT-1). The estimate
    // counts resistances and Temporary Hit Points, nothing rolled later.
    const auto taken = [&](int rolled)
    {
        const std::array parts{detail::DamagePart{type, std::max(0, rolled)}};
        return detail::resolve_damage(parts, affinities(target)).total;
    };
    const int standing = target.life.hp + target.life.temporary_hp.amount;
    if (target.life.hp <= 0 || taken(amount) < standing || taken(amount - 6) >= standing)
        return 0;
    auto *const bard = cutting_bard(roller);
    if (!bard || bard->source.side != target.source.side)
        return 0;
    bard->reaction = false;
    --bard->arcane;
    const int die = roll(6);
    const bool saved = taken(amount - die) < standing;
    const auto cut = std::to_string(die);
    log(bard->source.name + " uses Cutting Words: -" + cut + " damage" +
        (saved ? ", keeping " + target.source.name + " standing." : ", not enough to save " +
         target.source.name + "."),
    {
        saved ? "{name} uses Cutting Words: -{amount} damage, keeping {target} standing."
        : "{name} uses Cutting Words: -{amount} damage, not enough to save {target}.",
        {{"name", bard->source.name}, {"amount", cut}, {"target", target.source.name}}
    });
    return die;
}

bool Session::can_rebuke(const Actor &warlock, const Actor &attacker) const
{
    // A slot, or an Infernal Tiefling's free cast.
    const bool free = (warlock.initiate_casts & initiate_bit(def(warlock), "hellish_rebuke")) != 0;
    return warlock.source.side != attacker.source.side && conscious(warlock) &&
           warlock.reaction && !detail::incapacitated(warlock.effects) &&
           (free || (lowest_slot(warlock, 1) && !warlock.spent_slot)) &&
           detail::knows_spell(def(warlock).spells, "hellish_rebuke") &&
           distance(warlock.source.cell, attacker.source.cell) <= 60 && can_see(warlock, attacker) &&
           !attacker.life.dead;
}

void Session::rebuke(const Actor &attacker, Actor &warlock)
{
    // Hellish Rebuke: the attacker saves against 2d10 Fire, 1d10 more for
    // each slot level above 1, taking half on a success.
    if (!can_rebuke(warlock, attacker))
        return;
    ask_reaction(warlock, Asked::rebuke);
    if (answer_of(warlock.source.id, Asked::rebuke)->verb != "rebuke")
        return;
    warlock.reaction = false;
    // The free cast first: it can pay for nothing else.
    const auto bit = initiate_bit(def(warlock), "hellish_rebuke");
    const bool free = (warlock.initiate_casts & bit) != 0;
    const unsigned level = free ? 1 : lowest_slot(warlock, 1);
    if (free)
        warlock.initiate_casts &= ~bit;
    else
    {
        warlock.spent_slot = true;
        --warlock.slots[level];
    }
    auto &foe = actor(attacker.source.id);
    end_hiding(warlock.source.id);
    log(warlock.source.name + " casts Hellish Rebuke at " + foe.source.name + ".",
    {
        "{name} casts Hellish Rebuke at {target}.",
        {{"name", warlock.source.name}, {"target", foe.source.name}}
    });
    const bool saved = saving_throw_succeeds(foe, detail::Ability::dexterity,
                                          spell_dc(warlock, "hellish_rebuke"));
    const int rolled = dice({static_cast<int>(level) + 1, 10, 0});
    const int amount = resolved_damage(foe, detail::DamageType::fire, saved ? rolled / 2 : rolled);
    log(foe.source.name + " takes " + std::to_string(amount) + " Fire damage.",
    {
        "{name} takes {damage} {type} damage.",
        {{"name", foe.source.name}, {"damage", std::to_string(amount)}, {"type", "Fire", true}}
    });
    damage(foe, amount);
}

bool Session::can_use_giant_reaction(const Actor &goliath, GiantBoon boon) const
{
    return def(goliath).giant == boon && conscious(goliath) && goliath.reaction &&
           !detail::incapacitated(goliath.effects) && trait_uses_left(goliath) > 0;
}

void Session::giant_boon(const Actor &attacker, Actor &target, bool critical)
{
    // Fire's Burn, Frost's Chill and Hill's Tumble follow a damaging hit with
    // an attack roll, at most once in a command (Scorching Ray's rays).
    const auto boon = def(attacker).giant;
    if ((boon != GiantBoon::fire && boon != GiantBoon::frost && boon != GiantBoon::hill) ||
            trait_uses_left(attacker) <= 0 ||
            (boon == GiantBoon::hill && (def(target).size > 3 || target.effects.prone)))
        return;
    ask_reaction(attacker, Asked::boon);
    const auto used = std::find_if(reaction_answers_.begin(), reaction_answers_.end(),
                                   [&](const auto & answer)
    {
        return answer.target == attacker.source.id && answer.asked == Asked::boon &&
               answer.verb == "giant_boon";
    });
    if (used == reaction_answers_.end())
        return;
    used->verb = "spent";
    auto &goliath = actor(attacker.source.id);
    spend_trait_use(goliath);
    const auto label = std::string(giant_boon_label(boon));
    log(goliath.source.name + " uses " + label + ".",
    {"{name} uses {boon}.", {{"name", goliath.source.name}, {"boon", label, true}}});
    if (boon == GiantBoon::hill)
    {
        target.effects.prone = true;
        log(target.source.name + " is knocked Prone.",
        {"{name} is knocked Prone.", {{"name", target.source.name}}});
        return;
    }
    // The boon's die is part of the hit's damage, so a Critical Hit doubles it.
    const auto type = boon == GiantBoon::fire ? detail::DamageType::fire : detail::DamageType::cold;
    const int amount = resolved_damage(target, type,
                                       dice({1, boon == GiantBoon::fire ? 10 : 6, 0}, critical));
    const auto name = std::string(detail::damage_name(type));
    log(target.source.name + " takes " + std::to_string(amount) + " " + name + " damage.",
    {
        "{name} takes {damage} {type} damage.",
        {{"name", target.source.name}, {"damage", std::to_string(amount)}, {"type", name, true}}
    });
    if (boon == GiantBoon::frost && detail::can_apply(target.effects))
    {
        detail::apply_frosts_chill(target.effects, effect_source(goliath), goliath.source.name,
                                   next_turn_ms(goliath));
        log(target.source.name + "'s Speed drops by 10 feet.",
        {"{name}'s Speed drops by 10 feet.", {{"name", target.source.name}}});
    }
    damage(target, amount, false);
}

int Session::endured(Actor &goliath, int amount)
{
    // Stone's Endurance: 1d12 + Constitution modifier less damage.
    if (amount <= 0 || !askable_ || !can_use_giant_reaction(goliath, GiantBoon::stone))
        return amount;
    ask_reaction(goliath, Asked::endure);
    if (answer_of(goliath.source.id, Asked::endure)->verb != "stones_endurance")
        return amount;
    goliath.reaction = false;
    spend_trait_use(goliath);
    const int left = std::max(0, amount - roll(12) - def(goliath).constitution);
    log(goliath.source.name + " uses Stone's Endurance: " + std::to_string(amount - left) +
        " less damage.",
    {
        "{name} uses Stone's Endurance: {damage} less damage.",
        {{"name", goliath.source.name}, {"damage", std::to_string(amount - left)}}
    });
    return left;
}

void Session::storms_thunder(const Actor &attacker, Actor &goliath)
{
    // Storm's Thunder: 1d8 Thunder damage to a creature within 60 feet that
    // damaged the Goliath.
    if (attacker.source.side == goliath.source.side || attacker.life.dead ||
            attacker.life.hp <= 0 || distance(goliath.source.cell, attacker.source.cell) > 60 ||
            !can_use_giant_reaction(goliath, GiantBoon::storm))
        return;
    ask_reaction(goliath, Asked::thunder);
    if (answer_of(goliath.source.id, Asked::thunder)->verb != "storms_thunder")
        return;
    goliath.reaction = false;
    spend_trait_use(goliath);
    auto &foe = actor(attacker.source.id);
    const int amount = resolved_damage(foe, detail::DamageType::thunder, roll(8));
    log(goliath.source.name + " uses Storm's Thunder: " + foe.source.name + " takes " +
        std::to_string(amount) + " Thunder damage.",
    {
        "{name} uses Storm's Thunder: {target} takes {damage} Thunder damage.",
        {{"name", goliath.source.name}, {"target", foe.source.name}, {"damage", std::to_string(amount)}}
    });
    damage(foe, amount);
}

void Session::retaliate(const Actor &attacker, Actor &berserker)
{
    // Retaliation: a melee attack, with the Reaction, against a creature
    // within 5 feet that damaged the Berserker.
    if (!def(berserker).retaliation || attacker.source.side == berserker.source.side ||
            !conscious(berserker) || !berserker.reaction ||
            detail::incapacitated(berserker.effects) || attacker.life.dead ||
            attacker.life.hp <= 0 || distance(berserker.source.cell, attacker.source.cell) > 5)
        return;
    ask_reaction(berserker, Asked::retaliate);
    if (answer_of(berserker.source.id, Asked::retaliate)->verb != "retaliate")
        return;
    berserker.reaction = false;
    log(berserker.source.name + " retaliates.", {"{name} retaliates.", {{"name", berserker.source.name}}});
    attack(berserker, actor(attacker.source.id), AttackRange::melee);
}

void Session::clouds_jaunt(Actor &goliath, Cell destination)
{
    // Teleporting provokes no Opportunity Attacks.
    goliath.bonus = false;
    spend_trait_use(goliath);
    goliath.source.cell = destination;
    clear_departed_overlaps();
    log(goliath.source.name + " uses Cloud's Jaunt.",
    {"{name} uses Cloud's Jaunt.", {{"name", goliath.source.name}}});
}

void Session::answer_damage()
{
    // Indexed: a Reaction's own damage is recorded too, though only against
    // the creature acting, which never answers itself.
    for (std::size_t n = 0; n < damage_taken_.size(); ++n)
    {
        const auto [target, source] = damage_taken_[n];
        rebuke(actor(source), actor(target));
        storms_thunder(actor(source), actor(target));
        retaliate(actor(source), actor(target));
    }
    damage_taken_.clear();
}

void Session::perform(const Command &command)
{
    casting_metamagic_.reset();
    heightened_target_ = 0;
    const bool askable = std::any_of(actors_.begin(), actors_.end(), [&](const auto & other)
    {
        return can_shield(other) || def(other).deflect ||
               detail::knows_spell(def(other).spells, "hellish_rebuke") ||
               def(other).giant != GiantBoon::none || def(other).retaliation ||
               def(other).uncanny_dodge;
    });
    damage_taken_.clear();
    if (!askable)
    {
        dispatch_tracking_turn(command);
        damage_taken_.clear();
        reaction_answers_.clear();
        return;
    }
    // Undo everything the command did before the hit: the replay rolls the
    // same dice from the same state.
    const Session before = *this;
    try
    {
        askable_ = true;
        dispatch_tracking_turn(command);
        answer_damage();
        askable_ = false;
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
        // Magic Initiate's free cast first: it can pay for nothing else, while
        // a slot could.
        const auto &shield = *detail::find_spell("shield");
        const bool free = (defender.initiate_casts & initiate_bit(def(defender), "shield")) != 0;
        pay_for_spell(defender,
                      free ? "shield" + std::string(detail::initiate_suffix)
                      : defender.slots[1] > 0 ? "shield" : "shield_2",
                      shield);
        detail::apply_poisoned(defender.effects, effect_source(defender), defender.source.name,
                               next_turn_ms(defender), detail::EffectKind::shield);
        end_hiding(defender.source.id);
        log(defender.source.name + " casts Shield.",
        {"{name} casts {spell}.", {{"name", defender.source.name}, {"spell", "Shield", true}}});
    }
    else if (command.verb == "deflect" || command.verb == "uncanny_dodge")
        defender.reaction = false;
    perform(prompt.command);
}

void Session::follow_carrier(const Actor &monk, Cell departed)
{
    if (!monk.carried || flight_)
        return;
    auto &ally = actor(monk.carried);
    if (!ally.life.dead && distance(ally.source.cell, monk.source.cell) <= 10)
        ally.source.cell = departed;
}

void Session::dispatch_tracking_turn(const Command &command)
{
    const auto acting = std::find_if(actors_.begin(), actors_.end(), [&](const auto & a)
    {
        return a.source.id == command.actor;
    });
    if (acting == actors_.end())
    {
        dispatch(command);
        return;
    }
    const bool had_bonus = acting->bonus, had_action = acting->actions.normal,
               had_moved = acting->moved;
    dispatch(command);
    // Daze (Rogue 14): on its turn the creature does only one of moving, an
    // action or a Bonus Action; the first it does takes away the others.
    if (auto &dazed = actor(command.actor);
            detail::has_effect(dazed.effects, detail::EffectKind::dazed) &&
            actors_[turn_].source.id == dazed.source.id)
    {
        if (!had_moved && dazed.moved)
        {
            dazed.actions = {.normal = false};
            dazed.bonus = false;
        }
        else if (had_action && !dazed.actions.normal)
        {
            dazed.movement = 0;
            dazed.bonus = false;
        }
        else if (had_bonus && !dazed.bonus)
        {
            dazed.movement = 0;
            dazed.actions = {.normal = false};
        }
    }
    if (!had_bonus)
        return;
    // Fleet Step (Monk 11): a Bonus Action other than Step of the Wind leaves
    // Step of the Wind for right after it.
    auto &monk = actor(command.actor);
    if (!monk.bonus && def(monk).fleet_step && !command.verb.starts_with("step_of_the_wind"))
        monk.fleet_step = true;
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
    if (const auto chosen = cunning_choice(command.verb))
    {
        cunning_ = PendingCunning{command.actor, chosen->first, chosen->second};
        auto plain = command;
        plain.verb = command.verb.starts_with("melee_") ? "melee" : "ranged";
        dispatch(plain);
        cunning_.reset();
        return;
    }
    if (command.verb == "melee_stun")
    {
        stunning_ = command.actor;
        auto melee = command;
        melee.verb = "melee";
        dispatch(melee);
        stunning_ = 0;
        return;
    }
    if (const auto found = std::find_if(brutal_blows.begin(), brutal_blows.end(),
                                        [&](const auto & option)
{
    return std::get<0>(option) == command.verb;
});
    found != brutal_blows.end())
    {
        actor(command.actor).brutal_used = true;
        brutal_ = BrutalStrike{command.actor, std::get<2>(*found)};
        auto melee = command;
        melee.verb = "melee";
        dispatch(melee);
        brutal_.reset();
        return;
    }
    // Tactical Master: Push, Sap or Slow in place of the weapon's mastery.
    for (const auto &option : tactical_masteries)
        for (const std::string_view base : {"melee", "ranged"})
            if (command.verb == std::string(base) + "_" + std::string(option.suffix))
            {
                tactical_mastery_ = TacticalMastery{command.actor, option.kind};
                auto plain = command;
                plain.verb = base;
                dispatch(plain);
                tactical_mastery_.reset();
                return;
            }
    if (command.verb == "melee_necrotic" || command.verb == "melee_psychic" ||
            command.verb == "melee_radiant")
    {
        pact_damage_ = detail::damage_type(std::string_view(command.verb).substr(6));
        auto melee = command;
        melee.verb = "melee";
        dispatch(melee);
        pact_damage_.reset();
        return;
    }
    auto &a = actor(command.actor);
    const auto &d = def(a);
    if (command.verb != "move" && command.verb != "melee")
        a.run_feet = 0;
    // Instinctive Pounce's movement belongs to entering the Rage; anything
    // else first gives up what is left of it.
    if (command.verb != "move" && a.pounce > 0)
    {
        a.movement = std::max(0, a.movement - a.pounce);
        a.pounce = 0;
    }
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
        if (boost && check.natural + d.medicine + check.guidance + boost >= 10)
            --a.winds;
        finish_check(check, boost);
    }
    else if (command.verb == "stabilize")
    {
        PendingCheck check{a.source.id, command.target,
                           reliable(a, "medicine",
                                    detail::d20({false, !fears_in_sight(a).empty(), d.lucky}, rng_)),
                           a.actions.surge};
        check.guidance = guidance_bonus(a, "medicine");
        a.nick_origin = 0;
        a.actions.spend();
        if (check.natural + d.medicine + check.guidance < 10 && d.tactical_mind && a.winds > 0)
            check_choice_ = check;
        else
            finish_check(check, 0);
    }
    else if (command.verb == "temp_hp_keep" || command.verb == "temp_hp_use")
    {
        detail::grant_temporary_hp(a.life, *temporary_offer_,
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
                   command.verb == "light_ranged" || command.verb == "nick_ranged"
                   ? AttackRange::ranged
                   : AttackRange::melee);
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
    else if (command.verb == "cunning_hide")
    {
        a.bonus = false;
        hide(a);
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
        if (command.verb == "flurry_stun")
            stunning_ = a.source.id;
        // Heightened Focus (Monk 10): three strikes.
        const int strikes = d.heightened_focus ? 3 : 2;
        for (int strike = 0; strike < strikes && actor(command.target).life.hp > 0; ++strike)
        {
            auto striker = unarmed_actor(a);
            const bool hit = attack(striker, actor(command.target), AttackRange::melee);
            a.aim_ready = striker.aim_ready;
            if (hit && actor(command.target).life.hp > 0)
                open_hand(a, actor(command.target), command.verb);
        }
        stunning_ = 0;
    }
    else if (command.verb.starts_with("patient_defense") ||
             command.verb.starts_with("step_of_the_wind"))
        use_focus_movement(a, command.verb,
                           command.verb == "step_of_the_wind_carry" ? command.target : 0);
    else if (command.verb.starts_with("create_slot_") || command.verb.starts_with("convert_slot_"))
        use_font_of_magic(a, command.verb);
    else if (command.verb.starts_with("metamagic_"))
        ready_metamagic(a, command.verb.substr(10));
    else if (command.verb == "armor_of_shadows" || command.verb == "fiendish_vigor")
    {
        a.nick_origin = 0;
        (void)a.actions.spend(true);
        end_sanctuary(a);
        const bool was_invisible = invisible(a);
        if (command.verb == "armor_of_shadows")
        {
            detail::apply_spell_benefit(a.effects, effect_source(a), a.source.name,
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
            if (a.life.temporary_hp.amount)
                temporary_offer_ = std::move(offered);
            else
                detail::grant_temporary_hp(a.life, offered, TemporaryHpChoice::use_new);
        }
        if (was_invisible)
            end_invisibility(a.source.id);
        end_hiding(a.source.id);
    }
    else if (command.verb == "flame_blade_strike")
    {
        // A Magic action: 3d6 + the spellcasting modifier Fire on a hit.
        a.nick_origin = 0;
        (void)a.actions.spend(true);
        end_sanctuary(a);
        (void)attack(a, actor(command.target), AttackRange::melee,
                     SpellAttack{.dice = {3, 6, def(a).casting - 2},
                                 .type = detail::DamageType::fire});
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
        detail::apply_spell_benefit(a.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::wild_shape,
                                    static_cast<int>(form - detail::beast_forms.data()) + 1);
        const int druid_speed = def(a).speed;
        refresh_form(a);
        change_speed(a, druid_speed);
        log(a.source.name + " takes the shape of a " + std::string(form->label) + ".",
        {"{name} takes the shape of a {form}.", {{"name", a.source.name}, {"form", std::string(form->label), true}}});
        // Temporary Hit Points equal to the Druid level.
        TemporaryHitPoints offered{def(a).level, "feature:wild_shape"};
        if (a.life.temporary_hp.amount)
            temporary_offer_ = std::move(offered);
        else
            detail::grant_temporary_hp(a.life, offered, TemporaryHpChoice::use_new);
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
        (void)attack(a, actor(command.target), AttackRange::ranged,
                     SpellAttack{.dice = {1, 8, 0}, .type = detail::DamageType::fire,
                                 .spell = "produce_flame"});
    }
    else if (command.verb == "bardic_inspiration")
    {
        a.bonus = false;
        --a.arcane;
        auto &ally = actor(command.target);
        detail::apply_spell_benefit(ally.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::inspired, 6);
        log(a.source.name + " inspires " + ally.source.name + ".",
        {"{name} inspires {target}.", {{"name", a.source.name}, {"target", ally.source.name}}});
    }
    else if (command.verb == "innate_sorcery")
    {
        a.bonus = false;
        --a.free_casts;
        detail::apply_spell_benefit(a.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::innate_sorcery, 0);
        log(a.source.name + " unleashes Innate Sorcery.",
        {"{name} unleashes Innate Sorcery.", {{"name", a.source.name}}});
    }
    else if (command.verb == "martial_arts")
    {
        a.bonus = false;
        auto striker = unarmed_actor(a);
        attack(striker, actor(command.target), AttackRange::melee);
        a.aim_ready = striker.aim_ready;
    }
    else if (command.verb == "rage")
    {
        a.bonus = false;
        --a.channel_divinity;
        // A raging creature cannot keep Concentration.
        end_concentration(a);
        detail::apply_poisoned(a.effects, effect_source(a), a.source.name, rage_duration(a),
                               detail::EffectKind::raging);
        log(a.source.name + " enters a Rage.", {"{name} enters a Rage.", {{"name", a.source.name}}});
        // Instinctive Pounce: up to half Speed, as part of entering the Rage.
        if (const int feet = std::max(0, d.speed - detail::speed_penalty(a.effects)) / 2;
                d.instinctive_pounce && feet > 0)
        {
            a.movement += feet;
            a.pounce = feet;
            log(a.source.name + " may move " + std::to_string(feet) +
                " feet as part of entering the Rage (Instinctive Pounce).",
            {
                "{name} may move {feet} feet as part of entering the Rage (Instinctive Pounce).",
                {{"name", a.source.name}, {"feet", std::to_string(feet)}}
            });
        }
        // Mindless Rage: entering the Rage ends Charmed and Frightened.
        if (d.mindless_rage)
            std::erase_if(a.effects.active, [](const auto & e)
        {
            return e.kind == detail::EffectKind::charmed || e.kind == detail::EffectKind::turned ||
                   e.kind == detail::EffectKind::frightened;
        });
    }
    else if (command.verb == "intimidating_presence" ||
             command.verb == "intimidating_presence_rage")
    {
        // A Rage use restores the once-per-Long-Rest use.
        a.bonus = false;
        if (command.verb == "intimidating_presence")
            --a.free_casts;
        else
            --a.channel_divinity;
        intimidate(a);
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
            return e.kind == detail::EffectKind::drowsy || e.kind == detail::EffectKind::asleep ||
                   e.kind == detail::EffectKind::knocked_out;
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
        attack(a, target, AttackRange::melee,
               SpellAttack{.dice = {1, 8, d.casting - 2}, .type = detail::DamageType::force});
    }
    else if (command.verb == "horde_breaker")
    {
        a.horde_used = true;
        auto &target = actor(command.target);
        log(a.source.name + " uses Horde Breaker.",
        {"{name} uses Horde Breaker.", {{"name", a.source.name}}});
        const bool beyond_reach = distance(a.source.cell, target.source.cell) > d.reach;
        attack(a, target, beyond_reach ? AttackRange::ranged : AttackRange::melee);
    }
    else if (command.verb == "hunters_mark_free")
    {
        const auto &spell = *detail::find_spell("hunters_mark");
        a.bonus = false;
        --a.free_casts;
        end_hiding(a.source.id);
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
        detail::apply_spell_benefit(cursed.effects, effect_source(a), a.source.name,
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
        detail::apply_spell_benefit(quarry.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::hunters_mark, 0);
        log(a.source.name + " moves Hunter's Mark to " + quarry.source.name + ".",
        {
            "{name} moves Hunter's Mark to {target}.",
            {{"name", a.source.name}, {"target", quarry.source.name}}
        });
        reveal_lore(a, quarry);
    }
    else if (command.verb == "sacred_weapon" || command.verb == "sacred_weapon_radiant")
    {
        // The effect records the chosen damage: 1 for Radiant, 0 for the weapon's own.
        const bool radiant = command.verb == "sacred_weapon_radiant";
        --a.channel_divinity;
        detail::apply_spell_benefit(a.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::sacred_weapon, radiant ? 1 : 0);
        if (radiant)
            log(a.source.name + " uses Sacred Weapon, dealing Radiant damage.",
            {"{name} uses Sacred Weapon, dealing Radiant damage.", {{"name", a.source.name}}});
        else
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
        if (a.life.temporary_hp.amount)
            temporary_offer_ = std::move(offered);
        else
            detail::grant_temporary_hp(a.life, offered, TemporaryHpChoice::use_new);
        log(a.source.name + " uses Adrenaline Rush.",
        {"{name} uses Adrenaline Rush.", {{"name", a.source.name}}});
    }
    else if (command.verb == "opportunity" || command.verb == "decline")
    {
        if (command.verb == "opportunity")
        {
            a.reaction = false;
            attack(a, actor(command.target), AttackRange::melee);
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
    else if (command.verb == "flee")
    {
        // One step off the field; leaving an enemy's reach provokes as any move.
        path_ = {command.destination};
        path_index_ = 0;
        progress_movement();
    }
    else if (command.verb == "end")
        end_turn();
    else if (command.verb == "wholeness_of_body")
    {
        a.bonus = false;
        --a.free_casts;
        log(a.source.name + " uses Wholeness of Body.",
        {"{name} uses Wholeness of Body.", {{"name", a.source.name}}});
        heal(a, roll(d.martial_die) + std::max(1, d.wisdom));
    }
    else if (command.verb == "second_wind")
    {
        a.bonus = false;
        --a.winds;
        heal(a, roll(10) + d.level);
        // Tactical Shift: up to half Speed without provoking Opportunity Attacks.
        if (const int feet = std::max(0, d.speed - detail::speed_penalty(a.effects)) / 2;
                d.tactical_shift && feet > 0 && conscious(a))
        {
            champion_move_ = ChampionMove{.actor = a.source.id, .target = a.source.id,
                                          .remaining = feet, .origin = a.source.cell,
                                          .shift = true};
            log("Tactical Shift: " + a.source.name + " may move " + std::to_string(feet) +
                " feet without provoking Opportunity Attacks.",
            {
                "Tactical Shift: {name} may move {feet} feet without provoking Opportunity Attacks.",
                {{"name", a.source.name}, {"feet", std::to_string(feet)}}
            });
        }
    }
    else if (command.verb == "lay_on_hands_cure")
    {
        auto &target = actor(command.target);
        a.bonus = false;
        a.lay_on_hands -= 5;
        std::erase_if(target.effects.active, [](const auto & e)
        {
            return gives_condition(e.kind, "poisoned");
        });
        log(a.source.name + " uses Lay On Hands on " + target.source.name + ".",
        {
            "{name} uses Lay On Hands on {target}.",
            {{"name", a.source.name}, {"target", target.source.name}}
        });
        log(target.source.name + " is no longer Poisoned.",
        {"{name} is no longer Poisoned.", {{"name", target.source.name}}});
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
        a.lay_on_hands -= heal(target, std::min(a.lay_on_hands, max_hp(target) - target.life.hp));
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
        // Extra Attack: a later attack of the same Attack action spends nothing.
        const bool attacking = attack_action(command.verb);
        if (!attacking || !a.actions.attack_again())
        {
            a.nick_origin = 0;
            (void)a.actions.spend(detail::find_spell(command.verb) != nullptr);
            if (attacking)
                a.actions.extra_attacks = static_cast<unsigned>(d.extra_attacks);
        }
        if (command.verb == "dash")
        {
            a.movement += d.speed;
            ++a.dashes;
            log(a.source.name + " dashes.", {"{name} dashes.", {{"name", a.source.name}}});
        }
        else if (command.verb == "doff_shield")
        {
            // Taking off a shield takes the Utilize action (SRD 5.2.1); it stays
            // off for the rest of the fight.
            a.shield_off = true;
            a.definition = without_shield(a.source.character_profile, a.definition);
            log(a.source.name + " takes off a shield to use a bow.",
            {"{name} takes off a shield to use a bow.", {{"name", a.source.name}}});
        }
        else if (command.verb == "extinguish")
        {
            // Rolling on the ground puts out the fire and leaves the creature Prone.
            a.burning = false;
            a.effects.prone = true;
            log(a.source.name + " rolls on the ground and puts out the fire.",
            {"{name} rolls on the ground and puts out the fire.", {{"name", a.source.name}}});
        }
        else if (command.verb == "torch")
            torch_attack(a, actor(command.target));
        else if (command.verb == "shoot")
        {
            auto shooter = thrown_actor(a, carried_ranged_weapon(a));
            attack(shooter, actor(command.target), AttackRange::ranged);
            a.aim_ready = shooter.aim_ready;
        }
        else if (const auto *gear = command.verb.starts_with("throw_")
                                    ? detail::thrown_gear(std::string_view(command.verb).substr(6))
                                    : nullptr)
            throw_gear(a, actor(command.target), *gear);
        else if (command.verb == "dodge")
        {
            a.dodge = true;
            log(a.source.name + " dodges.", {"{name} dodges.", {{"name", a.source.name}}});
        }
        else if (command.verb == "hide")
            hide(a);
        else if (command.verb == "search")
            search(a);
        else if (command.verb == "disengage")
        {
            a.disengaged = true;
            log(a.source.name + " disengages.", {"{name} disengages.", {{"name", a.source.name}}});
        }
        else if (command.verb == "true_strike" || command.verb == "true_strike_radiant")
            strike_true(a, actor(command.target), command.verb == "true_strike_radiant"
                        ? TrueStrikeDamage::radiant
                        : TrueStrikeDamage::weapon);
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
                // Multiattack: a monster's melee attacks follow on the same
                // target while it stands.
                const int attacks = command.verb == "melee" ? d.multiattack : 1;
                for (int n = 0; n < attacks && actor(command.target).life.hp > 0; ++n)
                    attack(a, actor(command.target),
                           command.verb == "melee" ? AttackRange::melee : AttackRange::ranged);
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
        out << static_cast<unsigned>(cell) << ' ';
    out << '\n';
    out << rng_ << ' ' << revision_ << ' ' << turn_ << ' ' << round_ << ' '
        << static_cast<int>(outcome_) << ' ' << actors_.size() << '\n';
    for (const auto &a : actors_)
    {
        out << a.source.id << ' ' << std::quoted(a.source.definition) << ' '
            << std::quoted(a.source.name) << ' ' << static_cast<unsigned>(a.source.side) << ' '
            << a.source.cell.x << ' '
            << a.source.cell.y << ' ' << a.life.hp << ' ' << a.initiative << ' ' << a.movement << ' '
            << a.winds << ' ';
        for (unsigned level = 1; level <= highest_slot_level; ++level)
            out << a.slots[level] << ' ';
        out << a.life.successes << ' ' << a.life.failures << ' '
            << a.actions.normal << ' ' << a.bonus << ' ' << a.reaction << ' ' << a.dodge << ' '
            << a.disengaged << ' ' << a.life.stable << ' ' << a.life.dead << ' '
            << std::quoted(a.source.character_profile) << ' ' << a.spent_slot
            << ' ' << a.savage_used << ' ' << a.facing_left << ' ' << a.involuntary_overlap << ' '
            << a.hit_dice << ' ' << a.life.recovery.death_save_in_ms << ' '
            << detail::encode_stable_recovery(a.life.recovery) << ' ' << a.life.temporary_hp.amount << ' '
            << std::quoted(a.life.temporary_hp.source_id) << ' ' << a.rushes << ' ' << a.rush_used
            << ' ' << a.surges << ' ' << a.surge_used << ' ' << a.actions.surge << ' '
            << a.actions.extra_attacks << ' ' << a.dashes
            << ' ' << a.arcane << ' ' << a.lay_on_hands << ' ' << a.free_casts << ' '
            << a.channel_divinity << ' ' << a.initiate_casts << ' ' << a.smite_target << ' '
            << a.smite_critical << ' '
            << a.sneak_used << ' ' << a.aim_used
            << ' ' << a.aim_ready
            << ' ' << a.moved << ' ' << a.attacked << ' ' << a.pounce << ' ' << a.brutal_used
            << ' ' << a.stun_used << ' ' << a.fleet_step << ' ' << a.carried
            << ' ' << a.selected_weapon
            << ' ' << a.light_origins.size();
        for (auto id : a.light_origins)
            out << ' ' << id;
        out << ' ' << a.light_extra << ' ' << a.nick_origin << ' ' << a.cleave_used << ' '
            << a.colossus_used << ' ' << a.horde_used << ' ' << a.horde_origin << ' '
            << a.smite_melee << ' ' << a.resistance_used << ' ' << a.regeneration_blocked << ' '
            << a.rises_on_round << ' ' << a.thrown_gear_left[0] << ' ' << a.thrown_gear_left[1]
            << ' ' << a.thrown_gear_left[2] << ' ' << a.burning << ' ' << a.oiled_until_round
            << ' ' << a.shield_off << ' ' << a.must_stay << ' ' << a.panicked << ' '
            << a.source.morale << ' ' << a.source.intelligence << ' ' << a.run_start.x << ' '
            << a.run_start.y << ' ' << a.run_end.x << ' ' << a.run_end.y << ' ' << a.run_step.x
            << ' ' << a.run_step.y << ' ' << a.run_feet << ' ';
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
    out << (flight_ ? flight_->actor : 0) << ' ' << (flight_ ? flight_->remaining : 0) << '\n';
    out << log_.size() << '\n';
    for (const auto &entry : log_)
        out << std::quoted(entry.english) << '\n';
    out << scope_ << ' ' << elapsed_ms_ << ' ' << morale_ << ' ' << actors_.size() << '\n';
    for (const auto &a : actors_)
    {
        detail::write_effects(out, a.effects);
        out << '\n';
    }
    out << static_cast<bool>(temporary_offer_) << '\n';
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
    out << static_cast<bool>(check_choice_) << '\n';
    if (check_choice_)
    {
        const auto &c = *check_choice_;
        out << c.actor << ' ' << c.target << ' ' << c.natural << ' ' << c.surge_spent << ' '
            << c.guidance << '\n';
    }
    out << static_cast<bool>(champion_move_) << '\n';
    if (champion_move_)
    {
        const auto &c = *champion_move_;
        out << c.actor << ' ' << c.target << ' ' << c.natural << ' ' << c.remaining << ' '
            << c.spell << ' ' << c.origin.x << ' ' << c.origin.y << ' ' << c.shift << ' ' << c.toward
            << ' ' << c.withdraw << '\n';
    }
    out << static_cast<bool>(graze_) << '\n';
    if (graze_)
    {
        const auto &g = *graze_;
        out << g.actor << ' ' << g.target << ' ' << g.natural << ' ';
    }
    out << light_active_ << ' ' << nick_active_ << '\n';
    out << static_cast<bool>(mastery_) << '\n';
    if (mastery_)
    {
        const auto &m = *mastery_;
        out << m.actor << ' ' << m.target << ' ' << static_cast<unsigned>(m.kind) << ' '
            << m.natural << ' '
            << m.ranged << ' ' << m.targeting << ' ' << std::quoted(m.weapon) << ' '
            << m.origin.x << ' ' << m.origin.y << ' ' << m.thrown_item << ' ' << m.tactical << '\n';
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
    out << static_cast<bool>(effect_reaction_origin_) << ' ';
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
    out << static_cast<bool>(selection_);
    if (selection_)
    {
        out << ' ' << selection_->caster << ' ' << selection_->verb << ' '
            << selection_->chosen.size();
        for (const auto id : selection_->chosen)
            out << ' ' << id;
    }
    out << '\n';
    out << static_cast<bool>(area_);
    if (area_)
        out << ' ' << area_->caster << ' ' << area_->verb << ' ' << area_->center.x << ' '
            << area_->center.y;
    out << '\n' << zones_.size();
    for (const auto &zone : zones_)
    {
        out << ' ' << zone.caster << ' ' << static_cast<unsigned>(zone.kind) << ' '
            << zone.ends_ms << ' '
            << zone.save_dc << ' ' << zone.cells.size();
        for (const auto cell : zone.cells)
            out << ' ' << cell.x << ' ' << cell.y;
    }
    out << '\n' << static_cast<bool>(reaction_prompt_);
    if (reaction_prompt_)
    {
        const auto &prompt = *reaction_prompt_;
        const auto &question = prompt.question;
        const auto &command = prompt.command;
        out << ' ' << question.target << ' ' << static_cast<unsigned>(question.asked) << ' '
            << question.deflectable << ' ' << question.critical << ' ' << command.actor << ' '
            << command.target << ' ' << std::quoted(command.verb) << ' ' << command.destination.x
            << ' ' << command.destination.y << ' ' << command.item << ' ' << prompt.answers.size();
        for (const auto &answer : prompt.answers)
            out << ' ' << answer.target << ' ' << static_cast<unsigned>(answer.asked) << ' '
                << std::quoted(answer.verb);
    }
    out << '\n';
    // What each actor carries in (a Torch or bow to draw, thrown gear), one line
    // per actor in turn order.
    for (const auto &a : actors_)
    {
        out << a.source.inventory.size();
        for (const auto &item : a.source.inventory)
            out << ' ' << item.inventory_id << ' ' << std::quoted(item.definition) << ' '
                << item.quantity << ' ' << item.equipment_index;
        out << '\n';
    }
    // Members who ran off the field, with the vitals and flasks they left with.
    out << fled_.size() << '\n';
    for (const auto &gone : fled_)
    {
        out << gone.source.id << ' ' << std::quoted(gone.source.definition) << ' '
            << std::quoted(gone.source.name) << ' '
            << static_cast<unsigned>(gone.source.side) << ' '
            << gone.source.cell.x << ' ' << gone.source.cell.y << ' '
            << std::quoted(gone.source.character_profile) << ' ' << gone.hp << ' ' << gone.max_hp
            << ' ' << gone.armor_class << ' ' << gone.vitals.hit_points << ' ' << gone.vitals.dead
            << ' ' << std::quoted(gone.vitals.resources) << ' '
            << std::quoted(gone.vitals.description) << ' ' << gone.surrendered << ' '
            << gone.thrown_gear_left.size();
        for (const auto &[gear, left] : gone.thrown_gear_left)
            out << ' ' << std::quoted(gear) << ' ' << left;
        out << '\n';
    }
    return out.str();
}

// Parse one actor independently of session mutation.
Actor read_checkpoint_actor(std::istream &input, const Content &content)
{
    Actor actor;
    auto &source = actor.source;
    unsigned side{}, light_count{};
    input >> source.id >> std::quoted(source.definition) >> std::quoted(source.name) >>
          side >> source.cell.x >> source.cell.y >> actor.life.hp >> actor.initiative >>
          actor.movement >> actor.winds;
    for (unsigned level = 1; level <= highest_slot_level; ++level)
        input >> actor.slots[level];
    input >> actor.life.successes >> actor.life.failures >>
          actor.actions.normal >> actor.bonus >> actor.reaction >> actor.dodge >> actor.disengaged >>
          actor.life.stable >> actor.life.dead >> std::quoted(source.character_profile) >>
          actor.spent_slot >> actor.savage_used >> actor.facing_left >> actor.involuntary_overlap >>
          actor.hit_dice >> actor.life.recovery.death_save_in_ms >>
          actor.life.recovery.stable_recovery_in_ms >> actor.life.temporary_hp.amount >>
          std::quoted(actor.life.temporary_hp.source_id) >> actor.rushes >> actor.rush_used >>
          actor.surges >> actor.surge_used >> actor.actions.surge >> actor.actions.extra_attacks >>
          actor.dashes >> actor.arcane >>
          actor.lay_on_hands >> actor.free_casts >> actor.channel_divinity >>
          actor.initiate_casts >> actor.smite_target >>
          actor.smite_critical >> actor.sneak_used >> actor.aim_used >> actor.aim_ready >> actor.moved >>
          actor.attacked >> actor.pounce >> actor.brutal_used >> actor.stun_used >>
          actor.fleet_step >> actor.carried >> actor.selected_weapon >> light_count;
    if (!input || light_count > 2)
        throw std::runtime_error("Invalid Light attack count");
    // The checkpoint stores a side as its enumerator's value.
    if (side > static_cast<unsigned>(Side::opposition))
        throw std::runtime_error("Invalid checkpoint side");
    source.side = static_cast<Side>(side);
    for (unsigned i = 0; i < light_count; ++i)
    {
        unsigned id{};
        input >> id;
        actor.light_origins.push_back(id);
    }
    input >> actor.light_extra >> actor.nick_origin >> actor.cleave_used >> actor.colossus_used >>
          actor.horde_used >> actor.horde_origin >> actor.smite_melee >> actor.resistance_used >>
          actor.regeneration_blocked >> actor.rises_on_round >> actor.thrown_gear_left[0] >>
          actor.thrown_gear_left[1] >> actor.thrown_gear_left[2] >> actor.burning >>
          actor.oiled_until_round >> actor.shield_off >> actor.must_stay >> actor.panicked >>
          actor.source.morale >> actor.source.intelligence >> actor.run_start.x >>
          actor.run_start.y >> actor.run_end.x >> actor.run_end.y >> actor.run_step.x >>
          actor.run_step.y >> actor.run_feet;
    if (actor.source.morale > 100 || actor.source.intelligence > 30)
        throw std::runtime_error("Invalid checkpoint morale");
    if (actor.run_feet < 0 || actor.run_feet % 5 || actor.run_feet > 5 * 64 * 64 ||
            std::abs(actor.run_step.x) > 1 || std::abs(actor.run_step.y) > 1)
        throw std::runtime_error("Invalid checkpoint charge run");
    actor.concentration = detail::read_concentration(input);
    // Hunter's Mark, 1 hour, is the longest Concentration spell in the game.
    if (const auto &held = actor.concentration.active();
            held && (held->source.caster != source.id || held->remaining_ms > 3600000))
        throw std::runtime_error("Invalid checkpoint concentration");
    detail::decode_stable_recovery(actor.life.recovery);
    if (!input ||
            (source.character_profile.empty() && !content.definitions.contains(source.definition)))
        throw std::runtime_error("Invalid checkpoint actor");
    actor.definition = source.character_profile.empty()
                       ? content.definitions.at(source.definition)
                       : character_definition(source.character_profile);
    if (actor.shield_off)
    {
        if (source.character_profile.empty() || !wears_shield(actor.definition))
            throw std::runtime_error("Invalid shield removal");
        actor.definition = without_shield(source.character_profile, actor.definition);
    }
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
    // Cunning Action, Aggressive, Step of the Wind and Expeditious Retreat
    // spend the bonus action on a Dash. Effects are read later, so knowing the
    // spell is enough here.
    const bool bonus_dash = (definition.cunning || definition.aggressive || definition.focus ||
                             detail::knows_spell(definition.spells, "expeditious_retreat")) &&
                            !actor.bonus;
    if (actor.dashes < 0 ||
            actor.dashes > static_cast<int>(!actor.actions.normal) +
            static_cast<int>(actor.rush_used || bonus_dash) +
            static_cast<int>(actor.surge_used && !actor.actions.surge))
        throw std::runtime_error("Invalid Dash allowance count");
    if (actor.pounce < 0 || actor.pounce > definition.speed || actor.pounce > actor.movement)
        throw std::runtime_error("Invalid Instinctive Pounce movement");
    // Extra Attack's attacks follow an Attack action already taken.
    if (actor.actions.extra_attacks > static_cast<unsigned>(definition.extra_attacks) ||
            (actor.actions.extra_attacks && actor.actions.normal && !actor.surge_used))
        throw std::runtime_error("Invalid Extra Attack count");
    if (actor.fleet_step && (!definition.fleet_step || actor.bonus))
        throw std::runtime_error("Invalid Fleet Step");
    if (actor.carried && (!definition.heightened_focus || actor.carried == actor.source.id))
        throw std::runtime_error("Invalid carried ally");
    // The upper bound waits for the effects, read later: Aid raises it.
    if (actor.life.hp < 0 || (actor.life.dead && actor.life.hp > 0) ||
            // Dash spends the action before adding a second movement allowance.
            // Accepting both extra movement and an unused action lets a later Dash
            // create a state outside the checkpoint's own movement bounds.
            actor.movement < 0 ||
            actor.arcane < 0 || actor.arcane > arcane_capacity(definition) || actor.surges < 0 ||
            actor.surges > surge_capacity(definition) ||
            (actor.surge_used && (!definition.surges || actor.surges == definition.surges)) ||
            (actor.actions.surge && !actor.surge_used) || actor.rushes < 0 ||
            actor.rushes > definition.rushes ||
            (actor.rush_used &&
             (actor.bonus || !definition.rushes || actor.rushes == definition.rushes)) ||
            actor.winds < 0 || actor.winds > definition.winds ||
            !slots_in_range(actor.slots, definition) ||
            actor.hit_dice < 0 || actor.hit_dice > (definition.hit_die ? definition.level : 0) ||
            actor.life.successes < 0 || actor.life.successes > 3 || actor.life.failures < 0 || actor.life.failures > 4 ||
            actor.lay_on_hands < 0 || actor.lay_on_hands > lay_capacity(definition) ||
            actor.free_casts < 0 || actor.free_casts > free_cast_capacity(definition) ||
            actor.channel_divinity < 0 || actor.channel_divinity > channel_capacity(definition) ||
            actor.initiate_casts < 0 || (actor.initiate_casts & ~initiate_capacity(definition)))
        throw std::runtime_error("Invalid checkpoint actor state");
    detail::validate_recovery(actor.life);
    detail::validate_temporary_hp(actor.life.temporary_hp);
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
        board.terrain.push_back(static_cast<Terrain>(terrain));
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
        if (!input || board_.at(cell) == Terrain::obstacle)
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
    // A fleeing creature, paused on an Opportunity Attack.
    Flight flight;
    input >> flight.actor >> flight.remaining;
    const bool fleeing = std::any_of(actors_.begin(), actors_.end(), [&](const auto & actor)
    {
        return actor.source.id == flight.actor && conscious(actor);
    });
    if (!input || (flight.actor && (!fleeing || flight.remaining < 0 || flight.remaining > 120 ||
                                    path_.empty())) || (!flight.actor && flight.remaining))
        throw std::runtime_error("Invalid checkpoint flight");
    if (flight.actor)
        flight_ = flight;
}

void Session::finish_check(const PendingCheck &check, int boost)
{
    auto &a = actor(check.actor);
    auto &target = actor(check.target);
    const int total = check.natural + def(a).medicine + check.guidance + boost;
    bool success = total >= 10;
    Message message
    {
        "{actor} stabilizes {target}: d20 {roll} + {modifier} + {boost} = {total} vs DC {dc}: {result}.",
        {   {"actor", a.source.name},
            {"target", target.source.name},
            {"roll", std::to_string(check.natural)},
            {"modifier", std::to_string(def(a).medicine + check.guidance)},
            {"boost", std::to_string(boost)},
            {"total", std::to_string(total)},
            {"dc", "10"},
            {"result", success ? "Success" : "Failure", true}
        }};
    log(a.source.name + " Medicine: " + std::to_string(check.natural) + " + " +
        std::to_string(def(a).medicine + check.guidance) + " + " + std::to_string(boost) + " = " +
        std::to_string(total) + " vs DC 10: " + (success ? "success" : "failure"),
        message);
    // Bardic Inspiration follows the failed check in the log.
    if (!success)
        success = inspiration_roll(a, 10 - total) >= 10 - total;
    if (success)
        detail::stabilize(target.life, rng_);
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
    // The critical mover can now occupy the interrupted route; never walk
    // through the newly occupied cell.
    if (stop_illegal_route())
        return;
    finish_reaction();
}

bool Session::stop_illegal_route()
{
    if (path_index_ >= path_.size())
        return false;
    const auto &moving = mover();
    const auto grid = movement_grid(moving);
    auto cell = moving.source.cell;
    int budget = mover_budget(moving);
    bool valid = true, leaves_field = false;
    for (auto i = path_index_; i < path_.size(); ++i)
    {
        // A fleeing route ends with a 5-foot step off the field.
        if (!board_.contains(path_[i]))
        {
            valid = budget >= 5;
            leaves_field = true;
            break;
        }
        const auto cost = grid.step_cost(cell, path_[i]);
        if (!cost || *cost > budget)
        {
            valid = false;
            break;
        }
        budget -= *cost;
        cell = path_[i];
    }
    if (valid && (leaves_field || grid.can_stop_at(cell)))
        return false;
    path_.clear();
    path_index_ = 0;
    reactors_.clear();
    reactor_index_ = 0;
    flight_.reset();
    return true;
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
    if (c.toward)
    {
        // Forceful Blow: the turn's Barbarian follows the creature it struck.
        const int budget = who == actors_.end() ? 0
                           : std::max(0, def(*who).speed - detail::speed_penalty(who->effects)) / 2;
        if (who == actors_.end() || !def(*who).brutal_strike || !conscious(*who) ||
                c.actor != actors_[turn_].source.id || c.target != c.toward || c.natural ||
                c.spell || c.triggered || c.shift || !who->brutal_used || pending() ||
                c.remaining < 0 || c.remaining > budget || !board_.contains(c.origin) ||
                distance(c.origin, who->source.cell) > budget - c.remaining ||
                std::none_of(actors_.begin(), actors_.end(), [&](const auto & a)
        {
            return a.source.id == c.toward && !a.life.dead;
        }))
        throw std::runtime_error("Invalid Forceful Blow movement");
        return;
    }
    if (c.withdraw)
    {
        // Withdraw: the turn's Rogue, after a Cunning Strike.
        const int budget = who == actors_.end() ? 0
                           : std::max(0, def(*who).speed - detail::speed_penalty(who->effects)) / 2;
        if (who == actors_.end() || !def(*who).cunning_strike || !conscious(*who) ||
                c.actor != actors_[turn_].source.id || c.target != c.actor || c.natural ||
                c.spell || c.triggered || c.shift || c.toward || !who->sneak_used || pending() ||
                c.remaining < 0 || c.remaining > budget || !board_.contains(c.origin) ||
                distance(c.origin, who->source.cell) > budget - c.remaining)
            throw std::runtime_error("Invalid Withdraw movement");
        return;
    }
    if (c.shift)
    {
        // Tactical Shift: the turn's Fighter, after its Second Wind.
        const int budget = who == actors_.end() ? 0
                           : std::max(0, def(*who).speed - detail::speed_penalty(who->effects)) / 2;
        if (who == actors_.end() || !def(*who).tactical_shift || !conscious(*who) ||
                c.actor != actors_[turn_].source.id || c.target != c.actor || c.natural ||
                c.spell || c.triggered || who->bonus || pending() || c.remaining < 0 ||
                c.remaining > budget || !board_.contains(c.origin) ||
                distance(c.origin, who->source.cell) > budget - c.remaining)
            throw std::runtime_error("Invalid Tactical Shift movement");
        return;
    }
    const auto target = std::find_if(actors_.begin(), actors_.end(),
                                     [&](const auto & a)
    {
        return a.source.id == c.target;
    });
    if (who == actors_.end() || target == actors_.end() || c.actor == c.target ||
            !def(*who).champion || !conscious(*who) || c.natural < 2 || c.natural > 20 ||
            c.origin.x < 0 || c.origin.y < 0 || c.origin.x >= board_.width ||
            c.origin.y >= board_.height || board_.at(c.origin) == Terrain::obstacle ||
            check_choice_ ||
            temporary_offer_ || outcome_ != Outcome::ongoing)
        throw std::runtime_error("Invalid Champion movement source");
    if (c.triggered)
    {
        if (!board_.contains(c.trigger_origin) ||
                board_.at(c.trigger_origin) == Terrain::obstacle ||
                !board_.contains(c.target_origin) ||
                board_.at(c.target_origin) == Terrain::obstacle ||
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
            !(c.natural == 20 || (!c.spell && c.natural >= def(*who).critical_from) ||
              ((c.helpless || target->life.hp == 0) &&
               distance(c.triggered ? c.trigger_origin : c.origin,
                        c.triggered ? c.target_origin : target->source.cell) <= 5)) ||
            (pending() ? (pending() != c.actor || who->reaction ||
                          (!c.cleave && c.target != mover().source.id))
             : (c.actor != actors_[turn_].source.id &&
                !((c.cleave ? actors_[turn_].life.hp == 0
                   : c.target == actors_[turn_].source.id && target->life.hp == 0) &&
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
            c.natural < 1 || c.natural > 20 || c.natural + def(a).medicine + c.guidance >= 10 ||
            target == actors_.end() || target->life.hp != 0 || target->life.dead || target->life.stable ||
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
    if (a == actors_.end() || t == actors_.end() || a == t || !conscious(*a) || t->life.dead ||
            weapon_mastery(*a, AttackRange::melee) != detail::Mastery::graze ||
            g.natural < 1 || g.natural > 20 ||
            attack_hits(g.natural, def(*a).melee_bonus, armor_class(*t)) ||
            g.natural >= def(*a).critical_from ||
            distance(a->source.cell, t->source.cell) > def(*a).reach ||
            !line_of_sight(a->source.cell, t->source.cell) || check_choice_ ||
            temporary_offer_ || champion_move_ || outcome_ != Outcome::ongoing ||
            (pending() ? (pending() != g.actor || a->reaction || g.target != mover().source.id)
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
        if (!seen.insert(id).second || a.source.side != Side::party ||
                (!def(a).alert && !metabolism_ready(a) && !persistent_ready(a)) ||
                !conscious(a))
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
        if (a.life.hp > max_hp(a))
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
                             !mover.rush_used || mover.bonus || mover.life.temporary_hp.amount <= 0 ||
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
        if (actor.involuntary_overlap && (actor.life.dead || !shares_occupied_space(actor)))
            throw std::runtime_error("Invalid involuntary checkpoint overlap");
        if (actor.regeneration_blocked && !def(actor).regeneration)
            throw std::runtime_error("Regeneration blocked without Regeneration");
        // 3d6 rounds at most.
        if (actor.rises_on_round < 0 || actor.rises_on_round > static_cast<int>(round_) + 18 ||
                (actor.rises_on_round && !def(actor).regeneration))
            throw std::runtime_error("Invalid getting-up round");
        if ((actor.burning && actor.life.dead) || actor.oiled_until_round < 0 ||
                actor.oiled_until_round > static_cast<int>(round_) + 10 ||
                (actor.source.character_profile.empty() &&
                 std::any_of(actor.thrown_gear_left.begin(), actor.thrown_gear_left.end(),
                             [](auto left)
    {
        return left > 0;
    })))
        throw std::runtime_error("Invalid fire, oil or thrown gear state");
        if (actor.life.hp > 0 && !actor.life.dead)
            (actor.source.side == Side::party ? party : enemies) = true;
        if (actor.life.dead)
            continue;
        for (const auto &other : actors_)
        {
            if (other.source.id <= actor.source.id || other.life.dead ||
                    actor.source.cell != other.source.cell)
                continue;
            const auto &moving = this->mover();
            const bool in_transit =
                pending() && path_index_ > 0 &&
                path_[path_index_ - 1] == moving.source.cell &&
                (actor.source.id == moving.source.id || other.source.id == moving.source.id);
            if ((in_transit && (actor.source.side == other.source.side || unconscious(actor) ||
                                unconscious(other))) ||
                    actor.involuntary_overlap || other.involuntary_overlap ||
                    actor.life.hp == 0 || other.life.hp == 0)
                continue;
            throw std::runtime_error("Invalid overlapping checkpoint actors");
        }
    }
    const bool party_fled = std::any_of(fled_.begin(), fled_.end(), [](const auto & gone)
    {
        return gone.source.side == Side::party;
    });
    const auto expected = !party ? (party_fled ? Outcome::fled : Outcome::defeat)
                          : !enemies ? Outcome::victory : Outcome::ongoing;
    if (outcome_ != expected || (initiative_choices_.empty() && expected == Outcome::ongoing &&
                                 mover.life.hp == 0 && !champion_move_ && !effect_waiting()))
        throw std::runtime_error("Invalid checkpoint outcome/turn");
    if (!pending() && (!path_.empty() || !reactors_.empty()))
        throw std::runtime_error("Unpaused checkpoint movement");
    if (pending())
        validate_pending_movement();
}

void Session::validate_pending_movement() const
{
    auto moving = mover();
    if (effect_reaction_origin_)
    {
        moving.source.cell = effect_reaction_origin_->mover;
        moving.effects.prone = effect_reaction_origin_->prone;
    }
    if (outcome_ != Outcome::ongoing || moving.disengaged)
        throw std::runtime_error("Invalid pending movement");
    std::vector<detail::Occupant> occupants;
    for (const auto &other : actors_)
        if (!other.life.dead && !downed_riser(other) && other.source.id != moving.source.id)
            occupants.push_back({effect_reaction_origin_ && other.source.id == pending()
                                 ? effect_reaction_origin_->source
                                 : champion_move_ && other.source.id == champion_move_->actor
                                 ? champion_move_->origin
                                 : other.source.cell,
                                 other.source.side != moving.source.side, unconscious(other),
                                 other.life.hp == 0});
    const detail::MovementGrid grid{board_, moving.source.cell, occupants, moving.effects.prone};
    auto cell = moving.source.cell;
    int remaining =
        effect_reaction_origin_ ? effect_reaction_origin_->movement : mover_budget(moving);
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
            if (actor.source.side == moving.source.side || actor.reaction ||
                    !has_weapon_reaction(original, moving.source.cell, path_[path_index_]))
                throw std::runtime_error("Invalid mastery reaction trigger");
            continue;
        }
        if (champion_move_ && i == reactor_index_ && actor.source.id == champion_move_->actor)
        {
            if (actor.source.side == moving.source.side ||
                    distance(champion_move_->origin, moving.source.cell) > def(actor).reach ||
                    distance(champion_move_->origin, path_[path_index_]) <= def(actor).reach)
                throw std::runtime_error("Invalid Champion reaction origin");
            continue;
        }
        if (detail::opportunity_blocked(actor.effects) || actor.life.hp == 0 ||
                (!actor.reaction && !(graze_ && graze_->actor == actor.source.id &&
                                      i == reactor_index_)) ||
                actor.source.side == moving.source.side ||
                !has_weapon_reaction(actor, moving.source.cell, path_[path_index_]) ||
                !can_see(actor, moving))
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
    for (std::size_t i = 0; i < count; ++i)
    {
        std::string line;
        input >> std::quoted(line);
        if (!input || line.size() > 1000)
            throw std::runtime_error("Invalid checkpoint log line");
        Message message{line, {}};
        log_.push_back({std::move(line), std::move(message)});
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
    if (!input || count < 1 || count > 64 || turn >= count || round == 0 || outcome > 3 ||
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
    input >> session->scope_ >> session->elapsed_ms_ >> session->morale_ >> effects_count;
    if (session->morale_ > 100)
        throw std::runtime_error("Invalid checkpoint morale");
    if (!input || !session->scope_ || effects_count != session->actors_.size())
        throw std::runtime_error("Invalid checkpoint effect header");
    for (auto &a : session->actors_)
    {
        a.effects = detail::read_effects(input);
        if (a.life.recovery.stable_recovery_due && !detail::healing_blocked(a.effects))
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
        input >> c.actor >> c.target >> c.natural >> c.surge_spent >> c.guidance;
        if (!input || c.guidance < 0 || c.guidance > 4)
            throw std::runtime_error("Invalid ability-check checkpoint");
        session->check_choice_ = c;
    }
    bool pending_champion{};
    input >> pending_champion;
    if (pending_champion)
    {
        ChampionMove c;
        input >> c.actor >> c.target >> c.natural >> c.remaining >> c.spell >> c.origin.x >>
              c.origin.y >> c.shift >> c.toward >> c.withdraw;
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
              std::quoted(m.weapon) >> m.origin.x >> m.origin.y >> m.thrown_item >> m.tactical;
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
        input >> zone.caster >> kind >> zone.ends_ms >> zone.save_dc >> cells;
        // Only Grease keeps time; every other zone ends with Concentration.
        // Grease outlasts a caster who has since run off the field.
        if (!input || (!zone.ends_ms && !known_actor(zone.caster)) ||
                kind > static_cast<unsigned>(ZoneKind::spikes) ||
                (kind == static_cast<unsigned>(ZoneKind::grease)) != (zone.ends_ms != 0) ||
                !cells ||
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
        if (!input || !known_actor(question.target) ||
                asked > static_cast<unsigned>(Asked::retaliate) ||
                !known_actor(command.actor) || (command.target && !known_actor(command.target)) ||
                command.verb.empty() || answers > 3 * session->actors_.size())
            throw std::runtime_error("Invalid reaction prompt");
        question.asked = static_cast<Asked>(asked);
        for (std::size_t n = 0; n < answers; ++n)
        {
            ReactionAnswer answer;
            unsigned kind{};
            input >> answer.target >> kind >> std::quoted(answer.verb);
            if (!input || !known_actor(answer.target) ||
                    kind > static_cast<unsigned>(Asked::retaliate) ||
                    (answer.verb != "shield" && answer.verb != "deflect" &&
                     answer.verb != "redirect" && answer.verb != "rebuke" &&
                     answer.verb != "giant_boon" && answer.verb != "stones_endurance" &&
                     answer.verb != "storms_thunder" && answer.verb != "retaliate" &&
                     answer.verb != "uncanny_dodge" &&
                     answer.verb != "decline"))
                throw std::runtime_error("Invalid reaction prompt");
            answer.asked = static_cast<Asked>(kind);
            prompt.answers.push_back(std::move(answer));
        }
        session->reaction_prompt_ = std::move(prompt);
    }
    for (auto &a : session->actors_)
    {
        std::size_t carried{};
        input >> carried;
        if (!input || carried > 64 || (carried && a.source.character_profile.empty()))
            throw std::runtime_error("Invalid checkpoint inventory");
        a.source.inventory.resize(carried);
        for (auto &item : a.source.inventory)
            input >> item.inventory_id >> std::quoted(item.definition) >> item.quantity >>
                  item.equipment_index;
    }
    std::size_t departed{};
    input >> departed;
    if (!input || departed > 8)
        throw std::runtime_error("Invalid checkpoint flight");
    for (std::size_t n = 0; n < departed; ++n)
    {
        Departed gone;
        unsigned side{};
        input >> gone.source.id >> std::quoted(gone.source.definition) >>
              std::quoted(gone.source.name) >> side >> gone.source.cell.x >>
              gone.source.cell.y >> std::quoted(gone.source.character_profile) >> gone.hp >>
              gone.max_hp >> gone.armor_class >> gone.vitals.hit_points >> gone.vitals.dead >>
              std::quoted(gone.vitals.resources) >> std::quoted(gone.vitals.description) >>
              gone.surrendered;
        std::size_t gear_count{};
        input >> gear_count;
        if (!input || side > static_cast<unsigned>(Side::opposition))
            throw std::runtime_error("Invalid checkpoint flight");
        gone.source.side = static_cast<Side>(side);
        if (gone.hp < (gone.surrendered ? 0 : 1) || gone.hp > gone.max_hp ||
                (gone.surrendered && (gone.hp || gone.source.side == Side::party)) ||
                gone.vitals.dead || gear_count > detail::thrown_gear_items.size())
            throw std::runtime_error("Invalid checkpoint flight");
        gone.thrown_gear_left.resize(gear_count);
        for (auto &[gear, left] : gone.thrown_gear_left)
            input >> std::quoted(gear) >> left;
        session->fled_.push_back(std::move(gone));
    }
    if (session->actors_.size() + session->fled_.size() < 2)
        throw std::runtime_error("Invalid checkpoint flight");
    if (!input)
        throw std::runtime_error("Invalid checkpoint continuation");
    for (auto &a : session->actors_)
    {
        refresh_form(a);
        // Movement is bounded by the current Speed, a Wild Shape form's included,
        // once for each movement allowance the turn has had, and what is left of
        // Instinctive Pounce.
        if (a.movement > session->def(a).speed * (1 + a.dashes) + a.pounce)
            throw std::runtime_error("Invalid Dash allowance count");
    }
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
        // SRD 5.2.1 p. 23, through level 15.
        static constexpr unsigned thresholds[] {0, 0, 300, 900, 2700, 6500, 14000, 23000, 34000,
                                                48000, 64000, 85000, 100000, 120000, 140000,
                                                165000};
        if (level < 1 || level > 15)
            throw std::runtime_error("Unsupported character level");
        return thresholds[level];
    }

    std::vector<TrainingChoiceGroup> training_options(const CharacterSheet &sheet) const override
    {
        const auto klass = detail::class_of(sheet);
        if (klass == CharacterClass::wizard && sheet.level >= 2)
            return {detail::scholar_options(sheet.grants)};
        if (klass == CharacterClass::fighter && sheet.level >= 4)
            return later_masteries(klass, sheet);
        if (klass == CharacterClass::sorcerer && sheet.level >= 2)
            return {detail::metamagic_options()};
        if (klass == CharacterClass::rogue && sheet.level >= 6)
            return {detail::rogue_expertise_options(sheet.grants)};
        if (klass == CharacterClass::bard && sheet.level >= 3)
            return {detail::lore_options(sheet.grants)};
        if (klass == CharacterClass::druid && sheet.level >= 3)
            return {detail::land_options()};
        if (klass == CharacterClass::barbarian && sheet.level >= 3)
        {
            std::vector<TrainingChoiceGroup> groups{detail::primal_knowledge_options(sheet.grants)};
            for (auto &group : later_masteries(klass, sheet))
                groups.push_back(std::move(group));
            return groups;
        }
        return {};
    }

    std::vector<TrainingReplacementOptions>
    rest_training_options(const CharacterSheet &sheet, RestKind kind) const override
    {
        // Weapon Mastery and the Druid's land change after a Long Rest; the
        // Hunter's Prey after any rest (SRD 5.2.1 pp. 46, 59).
        std::vector<TrainingReplacementOptions> result;
        if (kind == RestKind::long_rest)
            if (auto mastery = mastery_rest_choice(sheet))
                result.push_back(std::move(*mastery));
        if (auto prey = subclass_rest_choice(sheet, detail::hunters_prey_options()))
            result.push_back(std::move(*prey));
        if (kind == RestKind::long_rest)
            if (auto land = subclass_rest_choice(sheet, detail::land_options()))
                result.push_back(std::move(*land));
        return result;
    }

    TrainingChoiceGroup hall_training_options(const CharacterSheet &sheet,
            std::span<const std::string> class_ids) const override
    {
        std::vector<detail::CharacterClass> classes;
        for (const auto &id : class_ids)
            classes.push_back(detail::class_from_id(id));
        return detail::hall_training_options(sheet.grants, classes);
    }

    void train_at_hall(CharacterSheet &sheet, std::string_view grant) const override
    {
        const auto klass = detail::class_of(sheet);
        auto candidate = sheet;
        // After the last skill or Expertise grant, so a save's replay, which
        // applies a level's spell choices first, rebuilds the same grant order.
        const auto last_skill =
            std::find_if(candidate.grants.rbegin(), candidate.grants.rend(), [](const auto & g)
        {
            return g.id.starts_with("skill:") || g.id.starts_with("expertise:");
        });
        candidate.grants.insert(last_skill.base(),
                                FeatureGrant{std::string(grant), std::string(detail::hall_training),
                                             static_cast<unsigned>(sheet.level), {}});
        candidate.training = detail::training_profile(candidate.grants, klass, candidate.race,
            detail::background_id(candidate.background),
            candidate.level, candidate.scores);
        (void)character_profile(candidate, {});
        sheet = std::move(candidate);
    }

    TrainingChoices replace_rest_training(CharacterSheet &sheet,
                                          std::span<const std::string> selected) const override
    {
        // A Long Rest offers every choice a rest can change.
        const auto offered = rest_training_options(sheet, RestKind::long_rest);
        if (offered.empty())
            throw std::runtime_error("No completed training choice to replace");
        const auto klass = detail::class_of(sheet);
        auto candidate = sheet;
        std::vector<std::string> unclaimed(selected.begin(), selected.end());
        TrainingChoices changed;
        for (const auto &choice : offered)
        {
            std::vector<std::string> picks;
            std::erase_if(unclaimed, [&](const auto & id)
            {
                const bool mine = std::any_of(choice.group.options.begin(),
                                              choice.group.options.end(),
                                              [&](const auto & option)
                {
                    return option.id == id;
                });
                if (mine)
                    picks.push_back(id);
                return mine;
            });
            if (picks.empty())
                continue;
            if (choice.group.id == "weapon_mastery")
                candidate.grants = detail::replace_masteries(candidate.grants, klass,
                                   candidate.level, picks);
            else
                replace_subclass_choice(candidate, choice.group.id, picks);
            changed[choice.group.id] = std::move(picks);
        }
        if (!unclaimed.empty())
            throw std::runtime_error("Not a choice this rest can change");
        // A new land's Circle Spells are always prepared, so they leave the list.
        const auto always = detail::always_prepared_spells(klass, candidate.level, candidate.grants);
        std::erase_if(candidate.prepared_spells, [&](const auto & id)
        {
            return std::find(always.begin(), always.end(), id) != always.end();
        });
        candidate.training = detail::training_profile(candidate.grants, klass, candidate.race,
            detail::background_id(candidate.background),
            candidate.level, candidate.scores);
        (void)character_profile(candidate, {});
        sheet = std::move(candidate);
        return changed;
    }

    AdvancementOptions advancement_options(const CharacterSheet &sheet) const override
    {
        // Each class advances to its cap (LEVELS-1); class_of rejects a label
        // no class has.
        const auto klass = detail::class_of(sheet);
        if (static_cast<unsigned>(sheet.level) >= detail::level_cap(klass))
            return {};
        AdvancementOptions result;
        result.level = sheet.level + 1;
        if (klass == CharacterClass::fighter ||
                ((klass == CharacterClass::paladin || klass == CharacterClass::ranger) &&
                 result.level == 2))
        {
            if (klass == CharacterClass::fighter)
                result.fighting_styles.push_back(
            {"keep", "Keep current", "Retain the class-granted Fighting Style."});
            for (auto style : detail::fighting_styles())
            {
                style.available =
                    (klass != CharacterClass::fighter ||
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
            if (klass == CharacterClass::paladin)
                result.fighting_styles.push_back(
            {
                "blessed_warrior", "Blessed Warrior",
                "Learn two Cleric cantrips; Charisma is your spellcasting ability for them."
            });
            // SRD 5.2.1 p. 59: the Ranger's alternative.
            if (klass == CharacterClass::ranger)
                result.fighting_styles.push_back(
            {
                "druidic_warrior", "Druidic Warrior",
                "Learn two Druid cantrips; Wisdom is your spellcasting ability for them."
            });
        }
        if (klass == CharacterClass::wizard && result.level == 2)
            result.training = {detail::scholar_options(sheet.grants)};
        if (klass == CharacterClass::fighter && (result.level == 4 || result.level == 10))
            result.training = {
                detail::mastery_options(CharacterClass::fighter, result.level, sheet.grants)};
        if (klass == CharacterClass::rogue && result.level == 6)
            result.training = {detail::rogue_expertise_options(sheet.grants)};
        // The Champion's Additional Fighting Style: a Fighting Style feat it
        // does not have yet.
        if (klass == CharacterClass::fighter && result.level == 7)
        {
            TrainingChoiceGroup group{std::string(detail::champion_style), "Additional Fighting Style",
                                      1, {}, TrainingChoiceControl::single_selection};
            for (const auto &style : detail::fighting_styles())
                if (!detail::has_grant(sheet.grants, "feat:" + style.id))
                    group.options.push_back({style.id, style.label, style.description});
            result.training = {std::move(group)};
        }
        if (klass == CharacterClass::barbarian && result.level == 3)
            result.training = {detail::primal_knowledge_options(sheet.grants)};
        if (klass == CharacterClass::sorcerer && result.level == 2)
            result.training = {detail::metamagic_options()};
        if (klass == CharacterClass::bard && result.level == 3)
            result.training = {detail::lore_options(sheet.grants)};
        if (klass == CharacterClass::druid && result.level == 3)
            result.training = {detail::land_options()};
        if (klass == CharacterClass::warlock && result.level == 2)
            result.training = {detail::invocation_options(2, sheet.grants,
                                                          "class:warlock:invocations:2")};
        if (klass == CharacterClass::barbarian && (result.level == 4 || result.level == 10))
            result.training = {
                detail::mastery_options(CharacterClass::barbarian, result.level, sheet.grants)};
        // The Hunter is the SRD's only Ranger subclass; Hunter's Prey is its choice.
        if (klass == CharacterClass::ranger && result.level == 3)
            result.training = {{
                "subclass:ranger:hunter", "Hunter's Prey", 1,
                detail::hunters_prey_options().options,
                TrainingChoiceControl::single_selection
            }
        };
        result.description =
            "Fixed-average HP growth. Resources gain only their new capacity;\nexisting expenditure remains.";
        if (klass == CharacterClass::cleric && result.level == 2)
            result.description =
                "Channel Divinity: two uses, one back on a Short Rest, for Divine Spark (heal or harm 1d8 + Wisdom within 30 feet) or Turn Undead.";
        if (klass == CharacterClass::wizard && result.level == 3)
            result.description =
                "Evoker: Potent Cantrip deals half damage when a cantrip misses or is saved against; Sculpt Spells spares up to 1 + the spell's level allies in your Evocation areas.";
        if (klass == CharacterClass::cleric && result.level == 3)
            result.description =
                "Life Domain: Disciple of Life adds 2 + the slot level to healing spells; Preserve Life (Channel Divinity) restores five times your level among Bloodied allies within 30 feet; Bless, Cure Wounds and Lesser Restoration are always prepared.";
        if (klass == CharacterClass::fighter && result.level == 3)
            result.description =
                "Champion: weapon/unarmed criticals on 19–20.\nAdvantage on Initiative and Strength (Athletics).\nCritical hit: optional half-Speed move, no opportunity attacks.";
        if (klass == CharacterClass::barbarian && result.level == 5)
            result.description =
                "Extra Attack: two attacks with each Attack action.\nFast Movement: Speed +10 feet without Heavy armor.\nProficiency Bonus +3.";
        if (klass == CharacterClass::barbarian && result.level == 6)
            result.description =
                "Berserker: Mindless Rage. While raging you cannot be Charmed or Frightened, and entering a Rage ends both.\nA fourth Rage.";
        if (klass == CharacterClass::barbarian && result.level == 7)
            result.description =
                "Feral Instinct: Advantage on Initiative.\nInstinctive Pounce: move up to half your Speed as part of entering a Rage.";
        if (klass == CharacterClass::barbarian && result.level == 9)
            result.description =
                "Brutal Strike: after Reckless Attack, one Strength attack a turn may give up its Advantage; on a hit it deals 1d10 more damage and a Forceful Blow (a 15-foot push you may follow) or a Hamstring Blow (Speed 15 feet lower).\nRage Damage +3; Proficiency Bonus +4.";
        if (klass == CharacterClass::barbarian && result.level == 10)
            result.description =
                "Berserker: Retaliation. When a creature within 5 feet damages you, your Reaction makes a melee attack against it.\nA fourth Weapon Mastery.";
        if (klass == CharacterClass::barbarian && result.level == 11)
            result.description =
                "Relentless Rage: dropping to 0 Hit Points while raging without dying, a Constitution save (DC 10, 5 higher for each try since your last rest) leaves you twice your Barbarian level in Hit Points.";
        if (klass == CharacterClass::barbarian && result.level == 12)
            result.description = "A feat or ability points.\nA fifth Rage.";
        if (klass == CharacterClass::barbarian && result.level == 13)
            result.description =
                "Improved Brutal Strike: two more blows. Staggering Blow: Disadvantage on the target's next saving throw, and no Opportunity Attacks. Sundering Blow: +5 to the next attack roll another creature makes against it.\nProficiency Bonus +5.";
        if (klass == CharacterClass::barbarian && result.level == 14)
            result.description =
                "Berserker: Intimidating Presence. A Bonus Action: each enemy within 30 feet makes a Wisdom save or is Frightened for up to a minute. Once per Long Rest, or for a Rage use.";
        if (klass == CharacterClass::barbarian && result.level == 15)
            result.description =
                "Persistent Rage: when Initiative is rolled, regain every Rage use, once per Long Rest. Your Rage needs no extending.";
        if (klass == CharacterClass::rogue && result.level == 5)
            result.description =
                "Cunning Strike: when an attack would deal Sneak Attack damage, give up a die to Trip the target (Dexterity save or Prone) or Withdraw (half your Speed without provoking).\nUncanny Dodge: your Reaction halves the damage of a hit.\nSneak Attack 3d6; Proficiency Bonus +3.";
        if (klass == CharacterClass::rogue && result.level == 6)
            result.description =
                "Expertise in two more skills.";
        if (klass == CharacterClass::rogue && result.level == 7)
            result.description =
                "Evasion: a Dexterity save against half damage takes none on a success and half on a failure.\nReliable Talent: a d20 below 10 on a proficient skill check counts as 10.\nSneak Attack 4d6.";
        if (klass == CharacterClass::rogue && result.level == 9)
            result.description =
                "Thief: Supreme Sneak. A hidden Rogue's Stealth Attack (a Cunning Strike) leaves it hidden.\nSneak Attack 5d6; Proficiency Bonus +4.";
        if (klass == CharacterClass::rogue && (result.level == 10 || result.level == 12))
            result.description =
                "A feat or ability points.";
        if (klass == CharacterClass::rogue && result.level == 11)
            result.description =
                "Improved Cunning Strike: one Sneak Attack may carry two Cunning Strike effects, paying for both.\nSneak Attack 6d6.";
        if (klass == CharacterClass::rogue && result.level == 13)
            result.description =
                "Thief: Use Magic Device, which waits for magic items.\nSneak Attack 7d6; Proficiency Bonus +5.";
        if (klass == CharacterClass::rogue && result.level == 14)
            result.description =
                "Devious Strikes: three more Cunning Strike effects. Daze (2 dice): a Constitution save or only one of moving, an action or a Bonus Action on its next turn. Knock Out (6 dice): a Constitution save or Unconscious for up to a minute, until damaged or shaken awake. Obscure (3 dice): a Dexterity save or Blinded until the end of its next turn.";
        if (klass == CharacterClass::rogue && result.level == 15)
            result.description =
                "Slippery Mind: proficiency in Wisdom and Charisma saving throws.\nSneak Attack 8d6.";
        if (klass == CharacterClass::monk && result.level == 5)
            result.description =
                "Extra Attack: two attacks with each Attack action.\nStunning Strike: once a turn, a hit with a Monk weapon or an Unarmed Strike may spend 1 Focus: Constitution save or Stunned until your next turn.\nMartial Arts d8; Proficiency Bonus +3.";
        if (klass == CharacterClass::monk && result.level == 6)
            result.description =
                "Empowered Strikes: Unarmed Strikes deal Force damage.\nOpen Hand: Wholeness of Body, a Bonus Action that heals the Martial Arts die + Wisdom, Wisdom modifier times per Long Rest.\nUnarmored Movement +15 feet.";
        if (klass == CharacterClass::monk && result.level == 7)
            result.description =
                "Evasion: a Dexterity save against half damage takes none on a success and half on a failure.";
        if (klass == CharacterClass::monk && result.level == 9)
            result.description =
                "Proficiency Bonus +4.";
        if (klass == CharacterClass::monk && result.level == 10)
            result.description =
                "Heightened Focus: Flurry of Blows makes three strikes, and Patient Defense with Focus gives two Martial Arts dice of Temporary Hit Points.\nSelf-Restoration: at the end of each of your turns, Charmed, Frightened or Poisoned ends.\nUnarmored Movement +20 feet.";
        if (klass == CharacterClass::monk && result.level == 11)
            result.description =
                "Open Hand: Fleet Step. After another Bonus Action, you may also use Step of the Wind.\nMartial Arts d10.";
        if (klass == CharacterClass::monk && result.level == 13)
            result.description =
                "Deflect Energy: Deflect Attacks works against any damage type.\nProficiency Bonus +5.";
        if (klass == CharacterClass::monk && result.level == 14)
            result.description =
                "Disciplined Survivor: proficiency in every saving throw.\nUnarmored Movement +25 feet.";
        if (klass == CharacterClass::monk && result.level == 15)
            result.description =
                "Perfect Focus: you start each fight with at least 4 Focus Points.";
        if (klass == CharacterClass::fighter && result.level == 5)
            result.description =
                "Extra Attack: two attacks with each Attack action.\nTactical Shift: after Second Wind, move up to half Speed without provoking Opportunity Attacks.\nProficiency Bonus +3.";
        if (klass == CharacterClass::fighter && result.level == 7)
            result.description = "Champion: a second Fighting Style feat.";
        if (klass == CharacterClass::fighter && result.level == 9)
            result.description =
                "Indomitable: once per Long Rest, reroll a failed saving throw, adding your Fighter level.\nTactical Master: an attack may use Push, Sap or Slow instead of its weapon's mastery.\nProficiency Bonus +4.";
        if (klass == CharacterClass::fighter && result.level == 10)
            result.description =
                "Champion: Heroic Inspiration at the start of each of your turns in a fight, when you lack it.\nA fifth Weapon Mastery, and a fourth Second Wind.";
        if (klass == CharacterClass::fighter && result.level == 11)
            result.description = "Two Extra Attacks: three attacks with each Attack action.";
        if (klass == CharacterClass::fighter && result.level == 13)
            result.description =
                "Indomitable: two uses per Long Rest.\nStudied Attacks: after you miss a creature, your next attack roll against it before the end of your next turn has Advantage.\nProficiency Bonus +5.";
        if (klass == CharacterClass::fighter && result.level == 15)
            result.description = "Champion: Superior Critical. Weapon and Unarmed Strike criticals on 18–20.";
        if (klass == CharacterClass::rogue && (result.level == 3 || result.level == 4))
            result.description =
                "Sneak Attack: 2d6. Steady Aim: Bonus Action; next attack roll has Advantage, Speed becomes 0.\nThief: Fast Hands has no use until magic items arrive. Hide remains unavailable.";
        if (klass == CharacterClass::paladin && result.level <= 4)
            result.description =
                "Prepared spells, Lay On Hands and fixed HP advancement; Fighting Style or Blessed Warrior and Paladin's Smite at level two; Channel Divinity, the Oath of Devotion and Sacred Weapon at level three. Level four grants an available feat or ability points.";
        if (klass == CharacterClass::paladin && result.level == 5)
            result.description =
                "Extra Attack: two attacks with each Attack action.\nLevel 2 spell slots and Paladin spells, among them Shining Smite; Divine Smite from a level 2 slot deals 3d8. Aid is always prepared.\nProficiency Bonus +3.";
        if (klass == CharacterClass::paladin && result.level == 6)
            result.description =
                "Aura of Protection: you and allies within 10 feet add your Charisma modifier (at least +1) to saving throws, unless you are Incapacitated.";
        if (klass == CharacterClass::paladin && result.level == 7)
            result.description =
                "Oath of Devotion: Aura of Devotion. You and allies in your Aura of Protection cannot be Charmed.\nA third level 2 spell slot.";
        if (klass == CharacterClass::paladin && result.level == 8)
            result.description = "A feat or ability points.";
        if (klass == CharacterClass::druid)
            result.description = "Prepared Druid spells and a Primal Order: Magician (an extra cantrip) or Warden (Martial weapons and Medium armor); the Circle of the Land at level three with its land's Circle Spells always prepared. Level four grants a third cantrip and an available feat or ability points.";
        if (klass == CharacterClass::bard)
            result.description = "Bardic spellcasting with prepared Bard spells; Jack of All Trades at level two; the College of Lore at level three with three more skills. Level four grants a third cantrip and an available feat or ability points.";
        if (klass == CharacterClass::warlock)
            result.description = "Pact Magic: prepared Warlock spells cast from slots of one level that return on a Short Rest; Magical Cunning at level two; the Fiend Patron at level three: Dark One's Blessing and Burning Hands, Command and Scorching Ray always prepared. Level four grants a third cantrip and an available feat or ability points.";
        if (klass == CharacterClass::sorcerer)
            result.description = "Prepared Sorcerer spells and Innate Sorcery; Font of Magic's Sorcery Points at level two; Draconic Sorcery at level three: Draconic Resilience (Hit Points and unarmored AC 10 + Dexterity + Charisma) and Chromatic Orb, Command and Dragon's Breath always prepared. Level four grants a fifth cantrip and an available feat or ability points.";
        if (klass == CharacterClass::monk && result.level <= 4)
            result.description = "Martial Arts and Unarmored Defense; Monk's Focus (Flurry of Blows, Patient Defense, Step of the Wind), Unarmored Movement and Uncanny Metabolism at level two; Deflect Attacks and the Warrior of the Open Hand at level three. Level four grants an available feat or ability points.";
        if (klass == CharacterClass::barbarian && result.level <= 4)
            result.description = "Rage, Unarmored Defense and Weapon Mastery; Danger Sense and Reckless Attack at level two; the Berserker with Frenzy, Primal Knowledge and a third Rage at level three. Level four grants an available feat or ability points and a third Weapon Mastery.";
        if (klass == CharacterClass::ranger)
            result.description =
                "Prepared spells with Favored Enemy and fixed HP advancement; Fighting Style or Druidic Warrior at level two; the Hunter with Hunter's Lore and Hunter's Prey at level three. Level four grants an available feat or ability points.";
        if (detail::gains_feat(klass, result.level))
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
            // Repeatable in the SRD; the game takes it once from a feat
            // level (a Human's Versatile Skilled aside), since its skills are
            // recorded as one group.
            {
                "skilled", "Skilled",
                "Gain proficiency in any three skills of your choice.",
                std::none_of(sheet.grants.begin(), sheet.grants.end(), [](const auto & g)
                {
                    return g.id == "feat:skilled" &&
                           g.source_id.ends_with(":ability_score_improvement");
                })
            }
        };
        if (detail::gains_feat(klass, result.level))
        {
            // After Savage Attacker, where the single Magic Initiate entry stood.
            const auto initiate = detail::magic_initiate_feats(sheet.grants);
            result.feats.insert(result.feats.begin() + 3, initiate.begin(), initiate.end());
        }
        if (klass == CharacterClass::paladin)
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
        if (klass == CharacterClass::cleric)
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
        if (klass == CharacterClass::wizard)
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
        if (options.level)
        {
            options.cantrip_swap = detail::cantrip_swap_offer(sheet);
            options.spell_swap = detail::spell_swap_offer(sheet);
            options.option_swap = detail::option_swap_offer(sheet);
        }
        // Skilled's proficiency page exists only while Skilled is the selection,
        // so switching away from it drops the group and its unconfirmed picks.
        if (choice.feat == "skilled" && options.level &&
                detail::gains_feat(detail::class_of(sheet), options.level))
            options.training.push_back(detail::skilled_options(sheet.grants));
        // Lessons of the First Ones' Skilled asks for its skills when it is
        // taken, as a new invocation or a replacement.
        if (options.level && takes_lessons_skilled(choice))
            options.training.push_back(detail::lessons_skilled_options(sheet.grants, options.level));
        if (choice.fighting_style && detail::class_of(sheet) == CharacterClass::fighter)
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
        const auto klass = detail::class_of(sheet);
        AdvancementChoice choice;
        const auto options = advancement_options(sheet);
        if (!options.level)
            return choice;
        if (options.level == 2 &&
                (klass == CharacterClass::paladin || klass == CharacterClass::ranger))
            choice.fighting_style = "defense";
        choice.spells = sheet.prepared_spells;
        for (const auto &group : options.training)
            for (const auto &option : group.options)
                if (choice.training[group.id].size() < group.count)
                    choice.training[group.id].push_back(option.id);
        if (choice.spells.empty() && klass == CharacterClass::wizard)
            choice.spells = {"magic_missile"};
        if (detail::prepares_spells(klass))
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
                                        SpellChoiceContext::advancement,
                                        ChoiceCompleteness::partial);
            const auto preparation =
                detail::spell_choice_options(next, SpellChoiceContext::advancement);
            choice.spells = preparation.locked_prepared;
            for (const auto &spell : preparation.preparation)
                if (choice.spells.size() < preparation.prepared_count &&
                        std::find(choice.spells.begin(), choice.spells.end(), spell.id) ==
                        choice.spells.end())
                    choice.spells.push_back(spell.id);
        }
        if (detail::gains_feat(klass, options.level))
        {
            choice.feat = "ability_score_improvement";
            const Ability primary =
                (klass == CharacterClass::fighter || klass == CharacterClass::paladin)
                ? Ability::strength
                : (klass == CharacterClass::cleric || klass == CharacterClass::druid)
                ? Ability::wisdom
                : (klass == CharacterClass::rogue || klass == CharacterClass::ranger)
                ? Ability::dexterity
                : (klass == CharacterClass::sorcerer || klass == CharacterClass::warlock ||
                   klass == CharacterClass::bard)
                ? Ability::charisma
                : Ability::intelligence;
            // Two points, from the primary ability onwards in sheet order,
            // wrapping past Charisma, never above 20.
            unsigned remaining = 2;
            for (std::size_t n = 0; n < all_abilities.size() && remaining; ++n)
            {
                const auto ability =
                    all_abilities[(ability_index(primary) + n) % all_abilities.size()];
                choice.abilities[ability] =
                    std::min(remaining,
                             static_cast<unsigned>(std::max(0, 20 - sheet.scores[ability])));
                remaining -= choice.abilities[ability];
            }
        }
        return choice;
    }

    CharacterSheet spell_choice_sheet(const CharacterSheet &sheet,
                                      const AdvancementChoice &choice) const override
    {
        const auto klass = detail::class_of(sheet);
        auto next = sheet;
        ++next.level;
        if ((klass == CharacterClass::paladin || klass == CharacterClass::ranger) &&
                next.level == 2)
            next.grants.push_back({"feature:fighting_style",
                                   "class:" + std::string(detail::class_id(klass)),
                                   2,
                                   {}});
        if (choice.fighting_style)
        {
            const auto source =
                "class:" + std::string(detail::class_id(klass)) + ":fighting_style";
            std::erase_if(next.grants,
                          [&](const auto & g)
            {
                return g.source_id == source;
            });
            next.grants.push_back(
            {fighting_style_grant(*choice.fighting_style), source,
             static_cast<unsigned>(next.level), {}});
        }
        // A cantrip, spell, invocation or Metamagic option replaced on gaining
        // the level changes what the level's spell choices offer.
        detail::apply_cantrip_swap(next, choice.cantrip_swap, detail::cantrip_swap_offer(sheet));
        detail::apply_spell_swap(next, choice.spell_swap, detail::spell_swap_offer(sheet));
        detail::apply_option_swap(next, choice.option_swap, detail::option_swap_offer(sheet));
        const auto held = [&](std::string_view id)
        {
            return std::any_of(next.grants.begin(), next.grants.end(), [&](const auto & g)
            {
                return g.id == id;
            });
        };
        // A blast invocation needs the cantrip it improves.
        for (const auto &g : next.grants)
            for (const std::string_view blast :
                    {"invocation:agonizing_blast:", "invocation:eldritch_spear:",
                     "invocation:repelling_blast:"
                    })
                if (g.id.starts_with(blast) && !held("spell:" + g.id.substr(blast.size())))
                    throw std::runtime_error("A blast invocation needs the cantrip it improves.");
        // Magic Initiate's spells are chosen with the level's other spells,
        // whether the feat comes at level four or from Lessons of the First Ones.
        if (choice.feat.starts_with("magic_initiate_") &&
                detail::gains_feat(klass, static_cast<unsigned>(next.level)))
            next.grants.push_back(detail::advancement_grant(klass, next.level, choice));
        const auto invocations = "class:warlock:invocations:2";
        if (const auto lessons = choice.training.find(invocations);
                lessons != choice.training.end())
            for (const auto &id : lessons->second)
                if (id.starts_with("lessons_magic_initiate_"))
                    next.grants.push_back(
                {"invocation:" + id, invocations, static_cast<unsigned>(next.level), {}});
        return next;
    }

    bool advance_character(CharacterSheet &sheet, VitalState &state,
                           const AdvancementChoice &choice) const override
    {
        const auto klass = detail::class_of(sheet);
        const auto options = advancement_options(sheet, choice);
        if (!options.level)
            return false;
        const bool half_style =
            (klass == CharacterClass::paladin || klass == CharacterClass::ranger) &&
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
        if (detail::gains_feat(klass, options.level))
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
            throw std::runtime_error("Feats and ability points come only at the class's feat levels");
        // Classes that prepare spells validate their choices through
        // apply_spell_choices below, against the whole class list.
        std::set<std::string> selected;
        for (const auto &spell : choice.spells)
        {
            if (!selected.insert(spell).second ||
                    (!detail::prepares_spells(klass) &&
                     std::none_of(options.spells.begin(), options.spells.end(),
                                  [&](const auto & s)
        {
            return s.id == spell && s.available;
        })))
            throw std::runtime_error("Choose only available, distinct spells");
        }
        if (!options.spells.empty() && choice.spells.empty())
            throw std::runtime_error("Choose at least one supported spell");
        if (takes_lessons_skilled(choice) && !choice.training.contains("invocation:lessons_skilled"))
            throw std::runtime_error("Choose exactly three Skilled proficiencies");
        // A swapped-in invocation cannot also be one of the level's new picks.
        if (const auto picks = choice.training.find("class:warlock:invocations:2");
                picks != choice.training.end() && !choice.option_swap.replacement.empty() &&
                std::find(picks->second.begin(), picks->second.end(),
                          choice.option_swap.replacement) != picks->second.end())
            throw std::runtime_error("An invocation cannot be both swapped in and chosen");
        auto actor = actor_from(old, state);
        auto next = spell_choice_sheet(sheet, choice);
        // The list after a spell swap; the level's choices then add to it.
        const auto prepared_before = next.prepared_spells;
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
            {
                const FeatureGrant grant
                {
                    // Skilled spans both catalogs, so its options carry the prefixed id.
                    id == "feat:skilled"             ? value
                    : id == "class:wizard:scholar" ? "expertise:" + value
                    : id == "subclass:ranger:hunter" ? "prey:" + value
                    : id == "class:barbarian:primal_knowledge" ? "skill:" + value
                    : id == "class:sorcerer:metamagic" ? "metamagic:" + value
                    : id == "class:warlock:invocations:2" ? "invocation:" + value
                    : id == "invocation:lessons_skilled" ? value
                    : id == "subclass:bard:lore" ? "skill:" + value
                    : id == "subclass:druid:land" ? "land:" + value
                    : id == detail::champion_style ? "feat:" + value
                    : id == detail::rogue_expertise_6 ? "expertise:" + value
                    : "mastery:" + value,
                    id,
                    static_cast<unsigned>(next.level),
                    {}};
                // spell_choice_sheet already granted Lessons' Magic Initiate.
                if (std::find(next.grants.begin(), next.grants.end(), grant) == next.grants.end())
                    next.grants.push_back(grant);
            }
        }
        if (klass == CharacterClass::warlock && next.level == 2)
            next.grants.push_back({"feature:magical_cunning", "class:warlock", 2, {}});
        if (klass == CharacterClass::bard && next.level == 2)
            next.grants.push_back({"feature:jack_of_all_trades", "class:bard", 2, {}});
        // The Circle of the Land is the SRD's only Druid subclass.
        if (klass == CharacterClass::druid && next.level == 2)
            next.grants.push_back({"feature:wild_shape", "class:druid", 2, {}});
        if (klass == CharacterClass::druid && next.level == 3)
        {
            next.grants.push_back({"subclass:land", "class:druid", 3, {}});
            next.grants.push_back({"feature:circle_spells", "subclass:druid:land", 3, {}});
            next.grants.push_back({"feature:lands_aid", "subclass:druid:land", 3, {}});
        }
        // The College of Lore is the SRD's only Bard subclass.
        if (klass == CharacterClass::bard && next.level == 3)
        {
            next.grants.push_back({"subclass:lore", "class:bard", 3, {}});
            next.grants.push_back({"feature:bonus_proficiencies", "subclass:bard:lore", 3, {}});
            next.grants.push_back({"feature:cutting_words", "subclass:bard:lore", 3, {}});
        }
        // The Fiend Patron is the SRD's only Warlock subclass.
        if (klass == CharacterClass::warlock && next.level == 3)
        {
            next.grants.push_back({"subclass:fiend", "class:warlock", 3, {}});
            next.grants.push_back({"feature:dark_ones_blessing", "subclass:warlock:fiend", 3, {}});
            next.grants.push_back({"feature:fiend_spells", "subclass:warlock:fiend", 3, {}});
        }
        if (klass == CharacterClass::sorcerer && next.level == 2)
        {
            next.grants.push_back({"feature:font_of_magic", "class:sorcerer", 2, {}});
            next.grants.push_back({"feature:metamagic", "class:sorcerer", 2, {}});
        }
        // Draconic Sorcery is the SRD's only Sorcerer subclass.
        if (klass == CharacterClass::sorcerer && next.level == 3)
        {
            next.grants.push_back({"subclass:draconic", "class:sorcerer", 3, {}});
            next.grants.push_back({"feature:draconic_resilience", "subclass:sorcerer:draconic", 3, {}});
            next.grants.push_back({"feature:draconic_spells", "subclass:sorcerer:draconic", 3, {}});
        }
        if (klass == CharacterClass::monk && next.level == 2)
        {
            next.grants.push_back({"feature:monks_focus", "class:monk", 2, {}});
            next.grants.push_back({"feature:unarmored_movement", "class:monk", 2, {}});
            next.grants.push_back({"feature:uncanny_metabolism", "class:monk", 2, {}});
        }
        // The Warrior of the Open Hand is the SRD's only Monk subclass.
        if (klass == CharacterClass::monk && next.level == 3)
        {
            next.grants.push_back({"feature:deflect_attacks", "class:monk", 3, {}});
            next.grants.push_back({"subclass:open_hand", "class:monk", 3, {}});
            next.grants.push_back({"feature:open_hand_technique", "subclass:monk:open_hand", 3, {}});
        }
        if (klass == CharacterClass::barbarian && next.level == 2)
        {
            next.grants.push_back({"feature:danger_sense", "class:barbarian", 2, {}});
            next.grants.push_back({"feature:reckless_attack", "class:barbarian", 2, {}});
        }
        // The Berserker is the SRD's only Barbarian subclass.
        if (klass == CharacterClass::barbarian && next.level == 3)
        {
            next.grants.push_back({"subclass:berserker", "class:barbarian", 3, {}});
            next.grants.push_back({"feature:frenzy", "subclass:barbarian:berserker", 3, {}});
            next.grants.push_back({"feature:primal_knowledge", "class:barbarian", 3, {}});
        }
        // The Thief is the SRD's only Rogue subclass. Fast Hands waits for magic
        // items to have a use; Second-Story Work is cut (SRD-DECISIONS).
        if (klass == CharacterClass::rogue && next.level == 3)
        {
            next.grants.push_back({"feature:steady_aim", "class:rogue", 3, {}});
            next.grants.push_back({"subclass:thief", "class:rogue", 3, {}});
            next.grants.push_back({"feature:fast_hands", "subclass:rogue:thief", 3, {}});
        }
        if (klass == CharacterClass::rogue && next.level == 2)
            next.grants.push_back({"feature:cunning_action", "class:rogue", 2, {}});
        for (auto &grant : detail::higher_level_features(klass, static_cast<unsigned>(next.level)))
            next.grants.push_back(std::move(grant));
        if (klass == CharacterClass::fighter && next.level == 2)
        {
            next.grants.push_back({"feature:action_surge", "class:fighter", 2, {}});
            next.grants.push_back({"feature:tactical_mind", "class:fighter", 2, {}});
        }
        if (klass == CharacterClass::cleric && next.level == 2)
            next.grants.push_back({"feature:channel_divinity", "class:cleric", 2, {}});
        // The Life Domain is the SRD's only Cleric subclass.
        if (klass == CharacterClass::cleric && next.level == 3)
        {
            next.grants.push_back({"subclass:life", "class:cleric", 3, {}});
            next.grants.push_back({"feature:disciple_of_life", "subclass:cleric:life", 3, {}});
            next.grants.push_back({"feature:preserve_life", "subclass:cleric:life", 3, {}});
        }
        // The Evoker is the SRD's only Wizard subclass.
        if (klass == CharacterClass::wizard && next.level == 3)
        {
            next.grants.push_back({"subclass:evoker", "class:wizard", 3, {}});
            next.grants.push_back({"feature:potent_cantrip", "subclass:wizard:evoker", 3, {}});
            next.grants.push_back({"feature:sculpt_spells", "subclass:wizard:evoker", 3, {}});
        }
        if (klass == CharacterClass::ranger && next.level == 3)
        {
            next.grants.push_back({"subclass:hunter", "class:ranger", 3, {}});
            next.grants.push_back({"feature:hunters_lore", "subclass:ranger:hunter", 3, {}});
        }
        // The Oath of Devotion is the SRD's only Paladin subclass.
        if (klass == CharacterClass::paladin && next.level == 3)
        {
            next.grants.push_back({"feature:channel_divinity", "class:paladin", 3, {}});
            next.grants.push_back({"subclass:devotion", "class:paladin", 3, {}});
            next.grants.push_back(
            {"feature:sacred_weapon", "subclass:paladin:devotion", 3, {}});
        }
        if (klass == CharacterClass::fighter && next.level == 3)
        {
            next.grants.push_back({"subclass:champion", "class:fighter", 3, {}});
            next.grants.push_back(
            {"feature:improved_critical", "subclass:fighter:champion", 3, {}});
            next.grants.push_back(
            {"feature:remarkable_athlete", "subclass:fighter:champion", 3, {}});
        }
        for (const auto ability : all_abilities)
            next.save_proficiencies[ability] =
                detail::save_proficient(klass, static_cast<unsigned>(next.level), ability);
        for (const auto ability : all_abilities)
        {
            next.scores[ability] += choice.abilities[ability];
            next.bonuses[ability] += choice.abilities[ability];
            if (next.scores[ability] > 20)
                throw std::runtime_error("Ability scores cannot exceed 20");
            next.modifiers[ability] = ability_modifier(next.scores[ability]);
            next.saving_throws[ability] =
                next.modifiers[ability] +
                (next.save_proficiencies[ability] ? 2 + (static_cast<int>(next.level) - 1) / 4 : 0);
        }
        if (points)
        {
            AbilityAdjustment adjustment{"feat:" + choice.feat,
                                         "Level " + std::to_string(next.level) +
                                         " Ability Score Improvement",
                                         static_cast<unsigned>(next.level)};
            for (const auto ability : all_abilities)
                adjustment.bonuses[ability] = static_cast<int>(choice.abilities[ability]);
            adjustment.label_message = {"Level {level} Ability Score Improvement",
                {{"level", std::to_string(next.level)}}
            };
            next.ability_adjustments.push_back(std::move(adjustment));
        }
        // spell_choice_sheet already granted Magic Initiate, whose spells it
        // offers. The same feat from another source (a Human's Versatile
        // Skilled) is a second grant.
        if (const auto feat = detail::advancement_grant(klass, next.level, choice);
                !choice.feat.empty() &&
                std::find(next.grants.begin(), next.grants.end(), feat) == next.grants.end())
            next.grants.push_back(feat);
        next.prepared_spells = choice.spells;
        if (choice.spell_learning)
        {
            next.prepared_spells = prepared_before;
            // A class without spells of its own learns only Magic Initiate's.
            const auto prepared = detail::prepares_spells(klass)
                                  ? std::optional(choice.spells) : std::nullopt;
            detail::apply_spell_choices(next,
                                        SpellChoices{*choice.spell_learning, prepared, {}, {}},
                                        SpellChoiceContext::advancement,
                                        ChoiceCompleteness::complete);
        }
        else if (detail::prepares_spells(klass) ||
                 !detail::spell_choice_options(next, SpellChoiceContext::advancement)
                 .learning.empty())
            throw std::runtime_error("Independent spell learning choices are required");
        next.training = detail::training_profile(
                            next.grants, klass, next.race,
                            detail::background_id(next.background), next.level, next.scores);
        next.hit_point_modifiers.push_back(next.modifiers[Ability::constitution]);
        next.hit_points =
            maximum_hit_points(klass, next.hit_die, next.race == "Dwarf", next.hit_point_modifiers,
                               klass == CharacterClass::sorcerer);
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
        if (klass == CharacterClass::rogue && next.level == 2)
        {
            next.class_modifiers +=
                "\nCunning Action: Dash or Disengage as a Bonus Action on your turn. Hide remains unavailable.";
            next.class_messages.push_back(
            {
                "Cunning Action: Dash or Disengage as a Bonus Action on your turn. Hide remains unavailable.",
                {}});
        }
        if (klass == CharacterClass::rogue && next.level == 3)
        {
            next.class_modifiers +=
                "\nSneak Attack: 2d6. Steady Aim: Bonus Action; next attack roll has Advantage, Speed becomes 0.";
            next.class_messages.push_back(
            {
                "Sneak Attack: 2d6. Steady Aim: Bonus Action; next attack roll has Advantage, Speed becomes 0.",
                {}});
        }
        if (klass == CharacterClass::paladin || klass == CharacterClass::ranger ||
                klass == CharacterClass::barbarian || klass == CharacterClass::monk ||
                klass == CharacterClass::sorcerer || klass == CharacterClass::warlock ||
                klass == CharacterClass::bard || klass == CharacterClass::druid)
        {
            const std::string note =
                klass == CharacterClass::druid
                ? "Prepared Druid spells and a Primal Order: Magician (an extra cantrip) or Warden (Martial weapons and Medium armor); the Circle of the Land at level three with its land's Circle Spells always prepared. Level four grants a third cantrip and an available feat or ability points."
                : klass == CharacterClass::bard
                ? "Bardic spellcasting with prepared Bard spells; Jack of All Trades at level two; the College of Lore at level three with three more skills. Level four grants a third cantrip and an available feat or ability points."
                : klass == CharacterClass::warlock
                ? "Pact Magic: prepared Warlock spells cast from slots of one level that return on a Short Rest; Magical Cunning at level two; the Fiend Patron at level three: Dark One's Blessing and Burning Hands, Command and Scorching Ray always prepared. Level four grants a third cantrip and an available feat or ability points."
                : klass == CharacterClass::sorcerer
                ? "Prepared Sorcerer spells and Innate Sorcery; Font of Magic's Sorcery Points at level two; Draconic Sorcery at level three: Draconic Resilience (Hit Points and unarmored AC 10 + Dexterity + Charisma) and Chromatic Orb, Command and Dragon's Breath always prepared. Level four grants a fifth cantrip and an available feat or ability points."
                : klass == CharacterClass::monk
                ? "Martial Arts and Unarmored Defense; Monk's Focus (Flurry of Blows, Patient Defense, Step of the Wind), Unarmored Movement and Uncanny Metabolism at level two; Deflect Attacks and the Warrior of the Open Hand at level three. Level four grants an available feat or ability points."
                : klass == CharacterClass::barbarian
                ? "Rage, Unarmored Defense and Weapon Mastery; Danger Sense and Reckless Attack at level two; the Berserker with Frenzy, Primal Knowledge and a third Rage at level three. Level four grants an available feat or ability points and a third Weapon Mastery."
                : klass == CharacterClass::paladin
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
        if (klass == CharacterClass::fighter && next.level == 2)
        {
            next.class_modifiers +=
                "\nAction Surge: one additional action, except Magic, on your turn. One use per Short or Long Rest.";
            next.class_messages.push_back(
            {
                "Action Surge: one additional action, except Magic, on your turn. One use per Short or Long Rest.",
                {}});
        }
        actor.definition = character_definition(character_profile(next, {}).data);
        if (actor.life.hp > 0)
            actor.life.hp += growth;
        // Pact Magic's slots rise a level together (level 2 at Warlock level
        // 3); the slots already spent stay spent.
        const int spent_slots = slot_total(old.slots) - slot_total(actor.slots);
        const auto pact_level = [](const SpellSlots &slots)
        {
            return std::find_if(slots.begin() + 1, slots.end(), [](int count)
            {
                return count > 0;
            }) - slots.begin();
        };
        if (actor.definition.pact_magic && pact_level(actor.definition.slots) != pact_level(old.slots))
        {
            const auto level = pact_level(actor.definition.slots);
            actor.slots = {};
            actor.slots[level] = std::max(0, actor.definition.slots[level] - spent_slots);
        }
        else
            for (unsigned level = 1; level <= highest_slot_level; ++level)
                actor.slots[level] += actor.definition.slots[level] - old.slots[level];
        actor.winds += actor.definition.winds - old.winds;
        actor.rushes += actor.definition.rushes - old.rushes;
        actor.surges += surge_capacity(actor.definition) - surge_capacity(old);
        actor.arcane += arcane_capacity(actor.definition) - arcane_capacity(old);
        actor.lay_on_hands += lay_capacity(actor.definition) - lay_capacity(old);
        actor.free_casts += free_cast_capacity(actor.definition) - free_cast_capacity(old);
        // A feat taken now arrives with its free cast; earlier ones keep their bits.
        actor.initiate_casts |= initiate_capacity(actor.definition) & ~initiate_capacity(old);
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
        auto actor = camp_actor(sheet, state);
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

    void elapse(std::span<Participant> participants, std::chrono::milliseconds elapsed,
                RandomState &random_state) const override
    {
        if (elapsed < std::chrono::milliseconds::zero())
            throw std::invalid_argument("Time cannot run backwards");
        const auto milliseconds = static_cast<std::uint64_t>(elapsed.count());
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
            a.life.hp = a.definition.hp;
            a.winds = a.definition.winds;
            a.slots = a.definition.slots;
            a.hit_dice = a.definition.hit_die ? a.definition.level : 0;
            if (p.state)
                restore_vitals(a, *p.state);
            actors.push_back(std::move(a));
        }
        std::vector<detail::RecoverySubject> subjects;
        for (auto &a : actors)
            subjects.push_back({{
                a.source.id, a.effects, a.definition.saves, a.life.dead,
                a.definition.str_dex_disadvantage, false, a.definition.gnomish_cunning,
                a.definition.lucky, a.definition.brave
            },
            a.life});
        auto rng = random_state.value;
        detail::elapse_recovery(subjects, milliseconds, rng);
        // An ended Aid lowers the maximum, and Hit Points above it with it.
        for (auto &a : actors)
            a.life.hp = std::min(a.life.hp, max_hp(a));
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
        random_state.value = rng;
    }

    void recover(VitalState &state, const CharacterSheet &sheet) const override
    {
        const auto d = character_definition(character_profile(sheet, {}).data);
        auto actor = actor_from(d, state);
        if (actor.life.dead || actor.life.hp < 1)
            throw std::runtime_error("Long rest requires at least one HP at its start");
        (void)detail::heal_life(actor.life, max_hp(actor), max_hp(actor),
                                !detail::healing_blocked(actor.effects));
        // A Long Rest makes Prayer of Healing able to help the creature again,
        // and gives back Relentless Endurance and a species trait's uses.
        std::erase_if(actor.effects.active, [](const auto & e)
        {
            return e.kind == detail::EffectKind::prayer_of_healing ||
                   e.kind == detail::EffectKind::relentless_used ||
                   e.kind == detail::EffectKind::trait_uses ||
                   e.kind == detail::EffectKind::heroic_inspiration;
        });
        // Resourceful: a Human finishes the Long Rest with Heroic Inspiration.
        if (d.resourceful && detail::can_apply(actor.effects))
            // Camp effects carry scope 1: there is no encounter to source them.
            detail::apply_spell_benefit(actor.effects, detail::EffectSource{.scope = 1, .actor = 1},
                                        sheet.name, detail::EffectKind::heroic_inspiration, 0);
        actor.winds = d.winds;
        actor.slots = d.slots;
        actor.hit_dice = d.hit_die ? d.level : 0;
        actor.life.successes = actor.life.failures = 0;
        actor.life.stable = false;
        actor.life.recovery = {};
        actor.life.temporary_hp = {};
        actor.rushes = d.rushes;
        actor.surges = surge_capacity(d);
        actor.arcane = arcane_capacity(d);
        actor.lay_on_hands = lay_capacity(d);
        actor.free_casts = free_cast_capacity(d);
        actor.channel_divinity = channel_capacity(d);
        actor.initiate_casts = initiate_capacity(d);
        state = vitals(actor);
    }

    RecoveryInfo recovery_info(const CharacterSheet &sheet, const VitalState &state) const override
    {
        const auto d = character_definition(character_profile(sheet, {}).data);
        auto actor = actor_from(d, state);
        RecoveryInfo result{static_cast<unsigned>(d.hit_die),
                            static_cast<unsigned>(actor.hit_dice),
                            static_cast<unsigned>(d.level),
                            !actor.life.dead && actor.life.hp > 0,
                            {},
                            actor.life.temporary_hp};
        for (const auto &descriptor : resource_descriptors)
            if (capacity_of(descriptor, d))
                result.resources.push_back(resource_pool(descriptor, actor, d));
        add_initiate_pools(result.resources, actor, d);
        add_trait_pool(result.resources, actor, d);
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
        auto actor = actor_from(d, state);
        detail::grant_temporary_hp(actor.life, offered, choice);
        auto next = vitals(actor);
        state = std::move(next);
    }

    void recover_short_rest(VitalState &state, const CharacterSheet &sheet) const override
    {
        const auto d = character_definition(character_profile(sheet, {}).data);
        auto actor = actor_from(d, state);
        if (actor.life.dead || actor.life.hp < 1)
            throw std::runtime_error("Short rest requires at least one HP at its start");
        actor.winds = std::min(d.winds, actor.winds + 1);
        actor.rushes = d.rushes;
        actor.surges = surge_capacity(d);
        actor.channel_divinity = std::min(channel_capacity(d), actor.channel_divinity + 1);
        // Pact Magic slots all return on a Short Rest.
        if (d.pact_magic)
            actor.slots = d.slots;
        state = vitals(actor);
    }

    Message recover_rest_choice(VitalState &state, const CharacterSheet &sheet,
                                std::string_view choice_id) const override
    {
        const auto d = character_definition(character_profile(sheet, {}).data);
        auto actor = actor_from(d, state);
        const auto choice = std::find_if(arcane_allocations.begin(), arcane_allocations.end(),
                                         [&](const auto & value)
        {
            return value.id == choice_id;
        });
        if (choice == arcane_allocations.end() || !can_recover(actor, *choice))
            throw std::runtime_error("This rest recovery choice is unavailable");
        actor.slots[1] += static_cast<int>(choice->first);
        actor.slots[2] += static_cast<int>(choice->second);
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
                               RandomState &random_state) const override
    {
        const auto d = character_definition(character_profile(sheet, {}).data);
        auto actor = actor_from(d, state);
        if (actor.life.dead || actor.life.hp < 1 || actor.hit_dice < 1)
            throw std::runtime_error("No Hit Die can be spent by this character");
        auto rng = random_state.value;
        const int rolled = roll_die(rng, d.hit_die);
        const int healing = detail::heal_life(actor.life, std::max(1, rolled + d.constitution), max_hp(actor),
                                              !detail::healing_blocked(actor.effects));
        --actor.hit_dice;
        HitDieResult result{static_cast<unsigned>(d.hit_die), rolled, d.constitution, healing,
                            static_cast<unsigned>(actor.hit_dice)};
        auto next = vitals(actor);
        state = std::move(next);
        random_state.value = rng;
        return result;
    }

    void set_hit_points(VitalState &state, const CharacterSheet &sheet, int hp) const override
    {
        if (hp < 0 || hp > sheet.hit_points || (state.dead && hp))
            throw std::runtime_error("Unsupported script HP change");
        auto actor = camp_actor(sheet, state);
        if (hp == state.hit_points ||
                (hp > state.hit_points && detail::healing_blocked(actor.effects)))
            return;
        detail::set_life_hit_points(actor.life, hp, max_hp(actor));
        state = vitals(actor);
    }

    HazardAttackResult hazard_attack(VitalState &state, const CharacterSheet &sheet,
                                     std::span<const std::string> gear,
                                     const HazardAttack &attack,
                                     RandomState &random_state) const override
    {
        auto actor = actor_from(character_definition(character_profile(sheet, gear).data), state);
        if (actor.life.dead || actor.life.hp < 1)
            throw std::runtime_error("A hazard attacks a conscious member");
        auto rng = random_state.value;
        HazardAttackResult result;
        result.natural = roll_die(rng, 20);
        result.total = result.natural + attack.attack_bonus;
        result.armor_class = armor_class_of(actor);
        result.critical = result.natural == 20;
        result.hit = result.critical || (result.natural != 1 && result.total >= result.armor_class);
        if (result.hit)
        {
            int rolled = attack.damage_bonus;
            for (unsigned n = 0; n < attack.dice * (result.critical ? 2 : 1); ++n)
                rolled += roll_die(rng, static_cast<int>(attack.sides));
            const detail::DamagePart part{detail::damage_type(attack.damage_type),
                                          std::max(0, rolled)};
            const auto &d = actor.form ? *actor.form : actor.definition;
            result.damage = detail::resolve_damage({&part, 1}, d.affinities).total;
            detail::damage_life(actor.life, result.damage, max_hp(actor));
            // As after a victory, the dying member's saves are rolled at once.
            while (actor.life.hp == 0 && !actor.life.dead && !actor.life.stable)
                result.death_saves.push_back(
                    detail::death_save(actor.life, rng, !detail::healing_blocked(actor.effects),
                                       actor.definition.lucky));
        }
        state = vitals(actor);
        random_state.value = rng;
        return result;
    }

    // A character's actor outside combat, its resources read from `state`.
    Actor camp_actor(const CharacterSheet &sheet, const VitalState &state) const
    {
        return actor_from(character_definition(character_profile(sheet, {}).data), state);
    }

    void temple_heal(VitalState &state, const CharacterSheet &sheet,
                     RandomState &random_state) const override
    {
        const auto d = character_definition(character_profile(sheet, {}).data);
        auto actor = actor_from(d, state);
        if (actor.life.dead || actor.life.hp >= max_hp(actor))
            throw std::runtime_error("Cure Wounds requires a wounded living member");
        // Authored temple caster: Cure Wounds, Wisdom +3. Same SplitMix64 as combat.
        auto rng = random_state.value;
        int amount = 3;
        for (int i = 0; i < 2; ++i)
            amount += roll_die(rng, 8);
        (void)detail::heal_life(actor.life, amount, max_hp(actor), !detail::healing_blocked(actor.effects));
        auto next = vitals(actor);
        state = std::move(next);
        random_state.value = rng;
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
        if (actor.life.dead || actor.life.hp == 0)
            return actions;
        if (d.lay_on_hands && actor.lay_on_hands > 0)
            actions.push_back({"lay_on_hands", {"Lay On Hands", {}}});
        if (d.lay_on_hands && actor.lay_on_hands >= 5)
            actions.push_back({"lay_on_hands_cure", {"Lay On Hands: cure Poisoned (5)", {}}});
        // Magical Cunning: a one-minute rite, so a camp action; it needs no
        // target and so takes the whole-party path.
        if (d.magical_cunning && actor.arcane > 0 &&
                slot_total(actor.slots) < slot_total(d.slots))
            actions.push_back({"magical_cunning", {"Magical Cunning", {}}, true});
        if (d.str_dex_disadvantage)
            return actions;
        for (const auto &spell : detail::spell_table)
        {
            // Aid's eight hours outlast a fight, so it is cast in camp too.
            if ((spell.pattern != detail::SpellPattern::heal &&
                    spell.pattern != detail::SpellPattern::camp &&
                    spell.rider != detail::Rider::aid) ||
                    !detail::knows_spell(d.spells, spell.id))
                continue;
            const std::string label(spell.label);
            const bool party = spell.instances > 1;
            if (spell.level >= 2)
            {
                if (actor.slots[2] > 0)
                    actions.push_back({std::string(spell.id), {label, {}}, party});
                continue;
            }
            if (actor.slots[1] > 0)
                actions.push_back({std::string(spell.id), {label, {}}, party});
            // Goodberry gains nothing from a higher slot.
            if (actor.slots[2] > 0 && spell.pattern == detail::SpellPattern::heal)
                actions.push_back({std::string(spell.id) + "_2", {label + " (level 2 slot)", {}}});
            if (actor.initiate_casts & initiate_bit(d, spell.id))
                actions.push_back({std::string(spell.id) + std::string(detail::initiate_suffix),
                                   {label + " (Magic Initiate)", {}}, party});
        }
        return actions;
    }

    void use_camp_action(const CharacterSheet &user, VitalState &user_state,
                         const CharacterSheet &target, VitalState &target_state,
                         std::string_view action, RandomState &random_state) const override
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
        if (action == "lay_on_hands_cure")
        {
            // 5 points end Poisoned on a living member (SRD 5.2.1 p. 61).
            if (patient.life.dead || !has_condition(patient, "poisoned"))
                throw std::runtime_error("Lay On Hands cures only a Poisoned member");
            caster.lay_on_hands -= 5;
            std::erase_if(patient.effects.active, [](const auto & e)
            {
                return gives_condition(e.kind, "poisoned");
            });
            user_state = vitals(caster);
            if (!self)
                target_state = vitals(healed);
            return;
        }
        const int maximum = max_hp(patient);
        if (patient.life.dead || patient.life.hp >= maximum)
            throw std::runtime_error("Healing requires a wounded living member");
        const bool can_heal = !detail::healing_blocked(patient.effects);
        auto rng = random_state.value;
        if (action == "lay_on_hands")
        {
            // Only the Hit Points actually restored are spent from the pool.
            caster.lay_on_hands -= detail::heal_life(
                                       patient.life, std::min(caster.lay_on_hands, maximum - patient.life.hp),
                                       maximum, can_heal);
        }
        else if (const auto &spell = *detail::find_spell(action); spell.id == "goodberry")
        {
            pay_for_spell(caster, action, spell);
            (void)detail::heal_life(patient.life, std::min(10, maximum - patient.life.hp), maximum, can_heal);
        }
        else
        {
            const bool upcast = action.ends_with("_2");
            pay_for_spell(caster, action, spell);
            // Disciple of Life adds 2 + the slot's level.
            int amount = casting_bonus(caster.definition, spell.id) - 2 +
                         (caster.definition.life_domain
                          ? 2 + (upcast ? 2 : static_cast<int>(spell.level))
                          : 0);
            const int count =
                spell.dice.count + (upcast ? static_cast<int>(spell.upcast.extra_dice) : 0);
            for (int n = 0; n < count; ++n)
                amount += roll_die(rng, spell.dice.sides);
            (void)detail::heal_life(patient.life, std::max(0, amount), maximum, can_heal);
        }
        user_state = vitals(caster);
        if (!self)
            target_state = vitals(healed);
        random_state.value = rng;
    }

    bool can_cast_exploration_spell(const CharacterSheet &sheet, const VitalState &state,
                                    std::string_view id) const override
    {
        const auto actor = camp_actor(sheet, state);
        const auto *spell = detail::find_spell(id);
        return spell && spell->pattern == detail::SpellPattern::exploration && !actor.life.dead &&
               actor.life.hp > 0 && !actor.definition.str_dex_disadvantage &&
               detail::knows_spell(actor.definition.spells, id) &&
               actor.slots[spell->level >= 2 ? 2 : 1] > 0;
    }

    void cast_exploration_spell(const CharacterSheet &sheet, VitalState &state,
                                std::string_view id) const override
    {
        if (!can_cast_exploration_spell(sheet, state, id))
            throw std::runtime_error("That spell cannot be cast now");
        auto caster = camp_actor(sheet, state);
        --caster.slots[detail::find_spell(id)->level >= 2 ? 2 : 1];
        state = vitals(caster);
    }

    // Aid in camp: the game chooses, as Pool of Radiance's group spells do,
    // the three members with the lowest Hit Point maximum not already aided
    // (AID-1).
    static void camp_aid(const std::string &caster_name, std::vector<Actor *> members)
    {
        std::erase_if(members, [](const Actor * a)
        {
            return a->life.dead || detail::hit_point_bonus(a->effects) >= 5 ||
                   !detail::can_apply(a->effects);
        });
        if (members.empty())
            throw std::runtime_error("No member can be helped by it now");
        std::stable_sort(members.begin(), members.end(), [](const Actor * x, const Actor * y)
        {
            return max_hp(*x) < max_hp(*y);
        });
        if (members.size() > 3)
            members.resize(3);
        for (auto *member : members)
        {
            // Camp effects carry scope 1: there is no encounter to source them.
            detail::apply_spell_benefit(member->effects, detail::EffectSource{.scope = 1, .actor = 1},
                                        caster_name, detail::EffectKind::aid, 5);
            (void)detail::heal_life(member->life, 5, max_hp(*member),
                                    !detail::healing_blocked(member->effects));
        }
    }

    // Prayer of Healing: the five most hurt members it has not healed since
    // their last Long Rest each regain 2d8 + the spellcasting modifier.
    void use_party_camp_action(const CharacterSheet &user, VitalState &user_state,
                               std::span<CampTarget> party, std::string_view action,
                               RandomState &random_state) const override
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
            auto &slots = caster.slots[d.slots[2] ? 2 : 1];
            const int maximum = d.slots[d.slots[2] ? 2 : 1];
            slots = std::min(maximum, slots + (maximum + 1) / 2);
            --caster.arcane;
            user_state = vitals(caster);
            return;
        }
        std::vector<std::pair<Actor, VitalState *>> members;
        for (const auto &member : party)
            if (&member.state.get() != &user_state)
                members.push_back({camp_actor(member.sheet, member.state), &member.state.get()});
        std::vector<Actor *> chosen{&caster};
        for (auto &[actor, state] : members)
            chosen.push_back(&actor);
        if (std::none_of(party.begin(), party.end(), [&](const auto & member)
    {
        return &member.state.get() == &user_state;
    }))
        chosen.erase(chosen.begin());
        if (action == "aid")
        {
            camp_aid(user.name, chosen);
            --caster.slots[2];
            user_state = vitals(caster);
            for (auto &[actor, state] : members)
                *state = vitals(actor);
            return;
        }
        std::erase_if(chosen, [](const Actor * a)
        {
            return a->life.dead || a->life.hp >= max_hp(*a) ||
                   detail::has_effect(a->effects, detail::EffectKind::prayer_of_healing);
        });
        if (chosen.empty())
            throw std::runtime_error("No member can be healed by it now");
        std::sort(chosen.begin(), chosen.end(), [](const Actor * x, const Actor * y)
        {
            return x->life.hp * max_hp(*y) < y->life.hp * max_hp(*x);
        });
        if (chosen.size() > 5)
            chosen.resize(5);
        const auto &spell = *detail::find_spell(action);
        --caster.slots[2];
        auto rng = random_state.value;
        for (auto *patient : chosen)
        {
            // Disciple of Life adds 2 + the slot's level.
            int amount = caster.definition.casting - 2 +
                         (caster.definition.life_domain ? 2 + static_cast<int>(spell.level) : 0);
            for (int n = 0; n < spell.dice.count; ++n)
                amount += roll_die(rng, spell.dice.sides);
            (void)detail::heal_life(patient->life, std::max(0, amount), max_hp(*patient),
                                    !detail::healing_blocked(patient->effects));
            // Camp effects carry scope 1: there is no encounter to source them.
            detail::apply_spell_benefit(patient->effects,
                                        detail::EffectSource{.scope = 1, .actor = 1}, user.name,
                                        detail::EffectKind::prayer_of_healing, 0);
        }
        user_state = vitals(caster);
        for (auto &[actor, state] : members)
            *state = vitals(actor);
        random_state.value = rng;
    }

    SpellChoiceOptions spell_choice_options(const CharacterSheet &sheet,
                                            SpellChoiceContext context) const override
    {
        return detail::spell_choice_options(sheet, context);
    }

    void apply_spell_choices(CharacterSheet &sheet, const SpellChoices &choice,
                             SpellChoiceContext context,
                             ChoiceCompleteness completeness) const override
    {
        detail::apply_spell_choices(sheet, choice, context, completeness);
    }

    SpellAccess spell_access(const CharacterSheet &sheet) const override
    {
        return detail::spell_access(sheet.grants, detail::class_of(sheet), sheet.level,
                                    sheet.prepared_spells);
    }

    EquipmentInfo equipment_info(std::string_view key) const override
    {
        if (detail::ammunition(key) || detail::thrown_gear(key))
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
                                       std::span<const std::string> gear, Ability ability,
                                       std::string_view skill) const override
    {
        const auto d = character_definition(character_profile(sheet, gear).data);
        auto result = character_rules()->ability_check(sheet, ability, skill);
        const bool strength_or_dexterity =
            ability == Ability::strength || ability == Ability::dexterity;
        result.disadvantage =
            (strength_or_dexterity && d.str_dex_disadvantage) ||
            (ability == Ability::dexterity && skill == "stealth" && d.stealth_disadvantage);
        return result;
    }

    AbilityCheckRoll roll_ability_check(const CharacterSheet &sheet,
                                        std::span<const std::string> gear, Ability ability,
                                        std::string_view skill,
                                        RandomState &random_state) const override
    {
        const auto modifier = ability_check(sheet, gear, ability, skill);
        const bool lucky = character_definition(character_profile(sheet, gear).data).lucky;
        auto rng = random_state.value;
        // Advantage and Disadvantage cancel; either alone rolls a second d20.
        int die = detail::d20({modifier.advantage, modifier.disadvantage, lucky}, rng);
        // Reliable Talent: a proficient check's d20 counts as at least 10.
        if (detail::class_of(sheet) == CharacterClass::rogue && sheet.level >= 7 &&
                modifier.proficiency > 0)
            die = std::max(die, 10);
        random_state.value = rng;
        return {die, die + modifier.total};
    }

    CharacterProfile character_profile(const CharacterSheet &sheet,
                                       std::span<const std::string> gear) const override
    {
        if (sheet.identity != character_rules()->identity() || sheet.level < 1)
            throw std::runtime_error("Unsupported character rules identity or level");
        const auto klass = detail::class_of(sheet);
        if (static_cast<unsigned>(sheet.level) > detail::level_cap(klass))
            throw std::runtime_error("Unsupported character rules identity or level");
        (void)detail::training_profile(sheet.grants, klass, sheet.race,
                                       detail::background_id(sheet.background), sheet.level,
                                       sheet.scores);
        const auto features_only =
            detail::without_spell_grants(detail::without_training(sheet.grants));
        const auto access = spell_access(sheet);
        const auto effects =
            detail::validate_grants(features_only, klass,
                                    detail::race_id(sheet.race),
                                    detail::background_id(sheet.background), sheet.level);
        const unsigned features = effects.feats;
        if (std::cmp_not_equal(sheet.hit_point_modifiers.size(), sheet.level))
            throw std::runtime_error("HP history does not match character advancement");
        // After the background's, one adjustment for each Ability Score
        // Improvement, in level order, together adding what the grants record.
        std::vector<unsigned> asi_levels;
        for (const auto &g : sheet.grants)
            if (g.id == "feat:ability_score_improvement")
                asi_levels.push_back(g.level);
        std::sort(asi_levels.begin(), asi_levels.end());
        if (sheet.ability_adjustments.size() != 1 + asi_levels.size())
            throw std::runtime_error("Ability sources disagree with acquired grants");
        AbilityArray<int> raised{};
        for (std::size_t i = 0; i < asi_levels.size(); ++i)
        {
            const auto &adjustment = sheet.ability_adjustments[i + 1];
            if (adjustment.source_id != "feat:ability_score_improvement" ||
                    adjustment.level != asi_levels[i])
                throw std::runtime_error("Ability sources disagree with acquired choices");
            for (const auto ability : all_abilities)
                raised[ability] += adjustment.bonuses[ability];
        }
        if (raised != effects.abilities)
            throw std::runtime_error("Ability sources disagree with acquired choices");
        for (const auto ability : all_abilities)
        {
            const auto bonuses =
                sheet.ability_adjustments.front().bonuses[ability] + effects.abilities[ability];
            if (sheet.bonuses[ability] != bonuses ||
                    sheet.scores[ability] != sheet.base[ability] + bonuses)
                throw std::runtime_error("Ability totals disagree with acquired choices");
        }
        auto spells = detail::casting_ids(access);
        std::set<std::string> selected;
        const auto record = [&](std::string id)
        {
            if (!detail::knows_spell(spells, id))
                spells.push_back(std::move(id));
        };
        // Preparable spells come from the eligibility table. Cantrips are
        // filtered out: they are known, never prepared.
        const auto preparable =
            detail::spells_of_level(allowed_spells(klass, sheet.level), false);
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
        out << ' ' << std::quoted(detail::background_id(sheet.background));
        detail::write_grants(out, sheet.grants);
        const auto data = out.str();
        const auto d = character_definition(data);
        const int proficiency = 2 + (static_cast<int>(sheet.level) - 1) / 4;
        const auto bonus = std::to_string(proficiency);
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
                result.item_modifiers += trained(klass, sheet.grants, key)
                                         ? "Source: equipped Shield: +2 AC.\n"
                                         : "Source: equipped Shield: +0 AC (untrained).\n";
            else if (key == "leather")
                result.item_modifiers += "Source: equipped Leather armor and Dexterity score " +
                                         std::to_string(sheet.scores[Ability::dexterity]) +
                                         ". AC becomes 11 + Dexterity modifier (" +
                                         std::to_string(sheet.modifiers[Ability::dexterity]) +
                                         ").\n";
            else if (key == "chain_mail")
                result.item_modifiers +=
                    "Source: equipped Chain mail. AC becomes 16; speed -10 feet below Strength 13 (current Strength " +
                    std::to_string(sheet.scores[Ability::strength]) + ").\n";
            else if (const auto *item = detail::armor(key))
                result.item_modifiers +=
                    "Source: equipped " + std::string(item->label) + " (" +
                    std::string(detail::armor_category_label(item->category)) + "). Base AC " +
                    std::to_string(item->base_ac) + "; applied Dexterity modifier " +
                    std::to_string(item->dexterity_contribution(
                                       sheet.modifiers[Ability::dexterity])) +
                    "; armor AC " +
                    std::to_string(item->base_ac + item->dexterity_contribution(
                                       sheet.modifiers[Ability::dexterity])) +
                    ".\n";
            else if (key == "wand")
                result.item_modifiers +=
                    "Source: equipped Wand. Held focus; melee uses unarmed strike.\n";
            else if (const auto *item = detail::weapon(key); item && item->fixed_damage)
                result.item_modifiers +=
                    "Source: equipped " + weapon_label(key) + ". Attack uses " +
                    attack_ability(key) + " modifier" +
                    (trained(klass, sheet.grants, key) ? " +" + bonus + " class proficiency"
                     : std::string(" without proficiency")) +
                                                       "; damage is fixed at " + std::to_string(item->fixed_damage) +
                                                       " without an ability modifier.\n";
            else
                result.item_modifiers +=
                    "Source: equipped " + weapon_label(key) + " and " + sheet.character_class +
                    " weapon proficiency. Weapon attack uses " + attack_ability(key) + " modifier" +
                    (trained(klass, sheet.grants, key) ? " +" + bonus + " class proficiency"
                     : std::string(" without proficiency")) +
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
                std::to_string(sheet.scores[Ability::strength]) +
                ". Attack uses Strength modifier +" + bonus +
                " proficiency; damage is 1 + Strength modifier (minimum 0).";
        result.spell_modifiers = "Active conditions are shown in the character status.";
        if (klass == CharacterClass::wizard)
            result.spell_modifiers =
                "Source: Fire Bolt and Wizard spellcasting, Intelligence score " +
                std::to_string(sheet.scores[Ability::intelligence]) +
                ". Attack: Intelligence modifier +" + bonus + " proficiency = " +
                std::to_string(d.casting) + ". Magic Missile has no ability modifier to damage.\n" +
                result.spell_modifiers;
        if (klass == CharacterClass::cleric)
            result.spell_modifiers =
                "Source: Cure Wounds and Cleric spellcasting, Wisdom score " +
                std::to_string(sheet.scores[Ability::wisdom]) +
                ". Healing: 2d8 + Wisdom modifier (" +
                std::to_string(d.casting - proficiency) + ").\n" + result.spell_modifiers;
        if (d.str_dex_disadvantage)
            result.spell_modifiers =
                "Cannot cast spells while wearing untrained armor.\n" + result.spell_modifiers;
        // Language-independent presentation of the same computed rule results.
        for (const auto &key : gear)
        {
            if (key == "shield")
                result.item_messages.push_back({trained(klass, sheet.grants, key)
                                                ? "Source: equipped Shield: +2 AC."
                                                : "Source: equipped Shield: +0 AC (untrained).",
                                                {}});
            else if (key == "leather")
                result.item_messages.push_back(
            {
                "Source: equipped Leather armor and Dexterity score {score}. AC becomes 11 + Dexterity modifier ({modifier}).",
                {   {"score", std::to_string(sheet.scores[Ability::dexterity])},
                    {"modifier", std::to_string(sheet.modifiers[Ability::dexterity])}
                }});
            else if (key == "chain_mail")
                result.item_messages.push_back(
            {
                "Source: equipped Chain mail. AC becomes 16; speed -10 feet below Strength 13 (current Strength {score}).",
                {{"score", std::to_string(sheet.scores[Ability::strength])}}});
            else if (const auto *item = detail::armor(key))
                result.item_messages.push_back(
            {
                "Source: equipped {item} ({category}). Base AC {base}; applied Dexterity modifier {dexterity}; armor AC {ac}.",
                {   {"item", std::string(item->label), true},
                    {"category", std::string(detail::armor_category_label(item->category)), true},
                    {"base", std::to_string(item->base_ac)},
                    {
                        "dexterity",
                        std::to_string(
                            item->dexterity_contribution(sheet.modifiers[Ability::dexterity]))
                    },
                    {
                        "ac", std::to_string(item->base_ac + item->dexterity_contribution(
                                                 sheet.modifiers[Ability::dexterity]))
                    }
                }});
            else if (key == "wand")
                result.item_messages.push_back(
            {"Source: equipped Wand. Held focus; melee uses unarmed strike.", {}});
            else if (const auto *item = detail::weapon(key); item && item->fixed_damage)
                result.item_messages.push_back(
            {
                trained(klass, sheet.grants, key)
                ? "Source: equipped {item}. Attack uses {ability} modifier +{bonus} class proficiency; damage is fixed at {damage} without an ability modifier."
                : "Source: equipped {item}. Attack uses {ability} modifier without proficiency; damage is fixed at {damage} without an ability modifier.",
                {   {"item", weapon_label(key), true},
                    {"ability", attack_ability(key), true},
                    {"bonus", bonus},
                    {"damage", std::to_string(item->fixed_damage)}
                }});
            else
                result.item_messages.push_back(
            {
                trained(klass, sheet.grants, key)
                ? "Source: equipped {item} and {class} weapon proficiency. Weapon attack uses {ability} modifier +{bonus} class proficiency; damage adds that ability modifier."
                : "Source: equipped {item} and {class} weapon proficiency. Weapon attack uses {ability} modifier without proficiency; damage adds that ability modifier.",
                {   {"item", weapon_label(key), true},
                    {"class", sheet.character_class, true},
                    {"ability", attack_ability(key), true},
                    {"bonus", bonus}
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
                    const bool penalty = sheet.scores[Ability::strength] < item->strength;
                    result.item_modifiers +=
                        "Source: equipped " + std::string(item->label) + ". Requires Strength " +
                        std::to_string(item->strength) + "; current score " +
                        std::to_string(sheet.scores[Ability::strength]) + ". " +
                        (penalty ? "Speed reduced by 10 feet." : "Requirement met.") + "\n";
                    result.item_messages.push_back(
                    {
                        penalty
                        ? "Source: equipped {item}. Requires Strength {required}; current score {score}. Speed reduced by 10 feet."
                        : "Source: equipped {item}. Requires Strength {required}; current score {score}. Requirement met.",
                        {   {"item", std::string(item->label), true},
                            {"required", std::to_string(item->strength)},
                            {"score", std::to_string(sheet.scores[Ability::strength])}
                        }});
                }
            }
            if (const auto *item = detail::weapon(key); item && item->heavy)
            {
                const std::string ability = item->ranged ? "Dexterity" : "Strength";
                const auto governing = item->ranged ? Ability::dexterity : Ability::strength;
                const auto score = std::to_string(sheet.scores[governing]);
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
            "No equipment modifiers. Source: unarmed strike rules and Strength score {score}. Attack uses Strength modifier +{bonus} proficiency; damage is 1 + Strength modifier (minimum 0).",
            {{"score", std::to_string(sheet.scores[Ability::strength])}, {"bonus", bonus}}});
        if (d.str_dex_disadvantage)
            result.spell_messages.push_back(
        {"Cannot cast spells while wearing untrained armor.", {}});
        if (klass == CharacterClass::wizard)
            result.spell_messages.push_back(
        {
            "Source: Fire Bolt and Wizard spellcasting, Intelligence score {score}. Attack: Intelligence modifier +{bonus} proficiency = {attack}. Magic Missile has no ability modifier to damage.",
            {   {"score", std::to_string(sheet.scores[Ability::intelligence])},
                {"bonus", bonus},
                {"attack", std::to_string(d.casting)}
            }});
        if (klass == CharacterClass::cleric)
            result.spell_messages.push_back(
        {
            "Source: Cure Wounds and Cleric spellcasting, Wisdom score {score}. Healing: 2d8 + Wisdom modifier ({modifier}).",
            {   {"score", std::to_string(sheet.scores[Ability::wisdom])},
                {"modifier", std::to_string(d.casting - proficiency)}
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
        if (klass == CharacterClass::sorcerer)
        {
            result.spell_modifiers +=
                "\nSorcerer cantrips: Charisma score " +
                    std::to_string(sheet.scores[Ability::charisma]) +
                "; spell attack bonus " + std::to_string(d.casting) + "; pending choices " +
                std::to_string(access.cantrip_choices - access.cantrips.size()) + ".";
            result.spell_messages.push_back(
            {
                "Sorcerer cantrips: Charisma score {score}; spell attack bonus {attack}; pending choices {cantrips}.",
                {   {"score", std::to_string(sheet.scores[Ability::charisma])},
                    {"attack", std::to_string(d.casting)},
                    {"cantrips", std::to_string(access.cantrip_choices - access.cantrips.size())}
                }});
        }
        if (klass == CharacterClass::warlock)
        {
            result.spell_modifiers +=
                "\nPact Magic cantrips: Charisma score " +
                    std::to_string(sheet.scores[Ability::charisma]) +
                "; spell attack bonus " + std::to_string(d.casting) + "; pending choices " +
                std::to_string(access.cantrip_choices - access.cantrips.size()) + ".";
            result.spell_messages.push_back(
            {
                "Pact Magic cantrips: Charisma score {score}; spell attack bonus {attack}; pending choices {cantrips}.",
                {   {"score", std::to_string(sheet.scores[Ability::charisma])},
                    {"attack", std::to_string(d.casting)},
                    {"cantrips", std::to_string(access.cantrip_choices - access.cantrips.size())}
                }});
        }
        if (klass == CharacterClass::cleric)
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
        if (klass == CharacterClass::ranger)
        {
            result.spell_modifiers +=
                "\nRanger spellcasting: Wisdom score " +
                    std::to_string(sheet.scores[Ability::wisdom]) +
                "; pending prepared spells: " +
                std::to_string(access.prepared_choices - access.prepared.size()) + ".";
            result.spell_messages.push_back(
            {
                "Ranger spellcasting: Wisdom score {score}; pending prepared spells: {prepared}.",
                {   {"score", std::to_string(sheet.scores[Ability::wisdom])},
                    {"prepared", std::to_string(access.prepared_choices - access.prepared.size())}
                }});
        }
        if (klass == CharacterClass::paladin)
        {
            result.spell_modifiers +=
                "\nPaladin spellcasting: Charisma score " +
                    std::to_string(sheet.scores[Ability::charisma]) +
                "; pending prepared spells: " +
                std::to_string(access.prepared_choices - access.prepared.size()) + ".";
            result.spell_messages.push_back(
            {
                "Paladin spellcasting: Charisma score {score}; pending prepared spells: {prepared}.",
                {   {"score", std::to_string(sheet.scores[Ability::charisma])},
                    {"prepared", std::to_string(access.prepared_choices - access.prepared.size())}
                }});
        }
        if (klass == CharacterClass::wizard)
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
        // Magic Initiate: each spell it teaches, with its feat's spell attack bonus.
        for (const auto &spell : access.initiate)
        {
            const auto attack = std::to_string(casting_bonus(d, spell.id));
            const auto list = spell.source_id.substr(spell.source_id.rfind('_') + 1);
            const std::string feat = list == "cleric" ? "Magic Initiate (Cleric)"
                                     : list == "druid" ? "Magic Initiate (Druid)"
                                     : "Magic Initiate (Wizard)";
            result.spell_modifiers +=
                "\n" + feat + ": " + spell.label + "; spell attack bonus " + attack + ".";
            result.spell_messages.push_back(
            {
                "{feat}: {spell}; spell attack bonus {attack}.",
                {{"feat", feat, true}, {"spell", spell.label, true}, {"attack", attack}}});
        }
        result.spell_messages.push_back(
        {"Active conditions are shown in the character status.", {}});
        return result;
    }

  private:
    std::shared_ptr<const Content> content_;

    // The Weapon Mastery kinds learned after level 1, through the sheet's level.
    static std::vector<TrainingChoiceGroup> later_masteries(CharacterClass klass,
            const CharacterSheet &sheet)
    {
        std::vector<TrainingChoiceGroup> groups;
        for (const auto acquired : detail::mastery_levels)
            if (acquired > 1 && acquired <= static_cast<unsigned>(sheet.level))
                if (auto group = detail::mastery_options(klass, acquired, sheet.grants); group.count)
                    groups.push_back(std::move(group));
        return groups;
    }

    // Weapon Mastery's picks, once every one is chosen.
    static std::optional<TrainingReplacementOptions> mastery_rest_choice(const CharacterSheet &sheet)
    {
        const auto klass = detail::class_of(sheet);
        auto group = detail::mastery_options(klass, 1);
        if (!group.count)
            return {};
        for (const auto acquired : detail::mastery_levels)
            if (acquired > 1 && acquired <= static_cast<unsigned>(sheet.level))
                group.count += detail::mastery_options(klass, acquired).count;
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

    // A subclass's single choice, stored as a grant sourced from its group
    // ("prey:colossus_slayer" from "subclass:ranger:hunter"), once it is made.
    static std::string_view subclass_choice_prefix(std::string_view group)
    {
        return group == "subclass:ranger:hunter" ? "prey:" : "land:";
    }

    static std::optional<TrainingReplacementOptions>
    subclass_rest_choice(const CharacterSheet &sheet, TrainingChoiceGroup group)
    {
        const auto prefix = subclass_choice_prefix(group.id);
        const auto made = std::find_if(sheet.grants.begin(), sheet.grants.end(), [&](const auto & g)
        {
            return g.source_id == group.id && g.id.starts_with(prefix);
        });
        if (made == sheet.grants.end())
            return {};
        std::vector<std::string> selected{made->id.substr(prefix.size())};
        return TrainingReplacementOptions{std::move(group), std::move(selected), 1};
    }

    static void replace_subclass_choice(CharacterSheet &sheet, const std::string &group,
                                        const std::vector<std::string> &picks)
    {
        if (picks.size() != 1)
            throw std::runtime_error("Choose one option");
        const auto prefix = subclass_choice_prefix(group);
        for (auto &g : sheet.grants)
            if (g.source_id == group && g.id.starts_with(prefix))
                g.id = std::string(prefix) + picks.front();
    }
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
    content.identity = {"opengold.srd5", "0.6.204", revision + "/" + std::to_string(hash)};
    std::set<std::string> save_rows, casting_rows, damage_rows, size_rows, trait_rows, type_rows,
        equipment_rows;
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
            content.definitions.at(key).size = static_cast<int>(found - names.begin());
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
        // "equipment <creature> metal_armor <source record> <armor>": the source
        // and armor document where the fact comes from (#231).
        if (tag == "equipment")
        {
            std::string fact, source, armor;
            row >> fact >> source >> armor;
            const auto found = content.definitions.find(key);
            if (!row || found == content.definitions.end() || fact != "metal_armor" ||
                    !equipment_rows.insert(key).second)
                throw std::runtime_error("Invalid creature equipment: " + key);
            found->second.metal_armor = true;
            row >> std::ws;
            if (!row.eof())
                throw std::runtime_error("Unknown creature equipment fields: " + key);
            continue;
        }
        // regeneration: Hit Points the creature regains at the start of each turn.
        if (tag == "regeneration")
        {
            const auto found = content.definitions.find(key);
            int value{};
            row >> value;
            if (!row || found == content.definitions.end() || value <= 0 || value > 50 ||
                    !trait_rows.insert(tag + " " + key).second)
                throw std::runtime_error("Invalid regeneration: " + key);
            found->second.regeneration = value;
            row >> std::ws;
            if (!row.eof())
                throw std::runtime_error("Unknown regeneration fields: " + key);
            continue;
        }
        // ranged_extra_damage creature count sides type: a ranged hit's added damage.
        if (tag == "ranged_extra_damage")
        {
            const auto found = content.definitions.find(key);
            Dice extra;
            std::string type;
            row >> extra.count >> extra.sides >> type;
            if (!row || found == content.definitions.end() || !found->second.ranged.count ||
                    extra.count < 1 || extra.count > 10 || extra.sides < 2 || extra.sides > 20 ||
                    !trait_rows.insert(tag + " " + key).second)
                throw std::runtime_error("Invalid ranged extra damage: " + key);
            found->second.ranged_extra = extra;
            found->second.ranged_extra_type = detail::damage_type(type);
            row >> std::ws;
            if (!row.eof())
                throw std::runtime_error("Unknown ranged extra damage fields: " + key);
            continue;
        }
        if (tag == "perception")
        {
            const auto found = content.definitions.find(key);
            int bonus{};
            row >> bonus;
            if (!row || found == content.definitions.end() || bonus < -5 || bonus > 20 ||
                    !trait_rows.insert(tag + " " + key).second)
                throw std::runtime_error("Invalid Perception: " + key);
            found->second.perception = bonus;
            row >> std::ws;
            if (!row.eof())
                throw std::runtime_error("Unknown Perception fields: " + key);
            continue;
        }
        if (tag == "multiattack")
        {
            const auto found = content.definitions.find(key);
            int attacks{};
            row >> attacks;
            if (!row || found == content.definitions.end() || attacks < 2 || attacks > 4 ||
                    !trait_rows.insert(tag + " " + key).second)
                throw std::runtime_error("Invalid Multiattack: " + key);
            found->second.multiattack = attacks;
            row >> std::ws;
            if (!row.eof())
                throw std::runtime_error("Unknown Multiattack fields: " + key);
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
                row >> definition.slots[2] >> casting_spells;
                if (definition.level < 3 || definition.slots[2] < 0 || definition.slots[2] > 20 ||
                        casting_spells < 0 || casting_spells > 63)
                    throw std::runtime_error("Invalid supplemental spellcasting");
                definition.spells = detail::spells_from_mask(static_cast<unsigned>(casting_spells));
                definition.known_cantrips = detail::spells_of_level(definition.spells, true);
                if (detail::mask_from_spells(definition.spells) !=
                        static_cast<unsigned>(casting_spells))
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
            d.ranged.bonus >> d.range >> d.long_range >> d.winds >> d.slots[1] >> d.casting >>
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
                (!ranged_none && !ranged_weapon) || d.winds < 0 || d.winds > 10 || d.slots[1] < 0 ||
                d.slots[1] > 20 || d.level < 1 || d.level > 4 || creature_spells < 0 ||
                creature_spells > 7 || d.melee_bonus < -10 || d.melee_bonus > 30 || d.casting < -10 ||
                d.casting > 30 || d.melee.bonus < -10 || d.melee.bonus > 30)
            throw std::runtime_error("Invalid or unsupported creature definition: " + key);
        d.spells = detail::spells_from_mask(static_cast<unsigned>(creature_spells));
        d.known_cantrips = detail::spells_of_level(d.spells, true);
        if (detail::mask_from_spells(d.spells) != static_cast<unsigned>(creature_spells))
            throw std::runtime_error("Invalid or unsupported creature definition: " + key);
        row >> std::ws;
        if (!row.eof())
            throw std::runtime_error("Unknown creature fields: " + key);
        content.definitions.emplace(std::move(key), d);
    }
    if (content.definitions.empty())
        throw std::runtime_error("Empty rules content");
    // A creature without the Perception skill uses its Wisdom save, which is
    // its Wisdom modifier unless it is proficient.
    for (auto &[key, definition] : content.definitions)
        if (!trait_rows.contains("perception " + key))
            definition.perception = definition.saves[4];
    return std::make_unique<Module>(std::move(content));
}
} // namespace opengold::srd5
