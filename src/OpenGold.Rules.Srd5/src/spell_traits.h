#ifndef OPENGOLD_SRD5_SPELL_TRAITS_H
#define OPENGOLD_SRD5_SPELL_TRAITS_H
#include "spell_table.h"
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

// What a spell, an effect or a feature's command is, read off the spell table
// and the verb: the Session's combat resolution asks these; none keeps state.
namespace opengold::srd5::detail
{
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

inline constexpr std::array<std::string_view, 10> metamagic_ids{
    "careful",   "distant", "empowered", "extended",   "heightened",
    "quickened", "seeking", "subtle",    "transmuted", "twinned"};
inline constexpr std::array<std::string_view, 10> metamagic_labels{
    "Careful Spell",   "Distant Spell", "Empowered Spell", "Extended Spell",   "Heightened Spell",
    "Quickened Spell", "Seeking Spell", "Subtle Spell",    "Transmuted Spell", "Twinned Spell"};
// Transmuted Spell changes one of these damage types into another.
inline constexpr std::array<std::string_view, 6> transmuted_types{"acid",      "cold",   "fire",
                                                                  "lightning", "poison", "thunder"};

constexpr int metamagic_cost(Metamagic option)
{
    return option == Metamagic::heightened || option == Metamagic::quickened ? 2 : 1;
}

// The lasting benefit a buff spell's rider applies.
constexpr EffectKind rider_effect(Rider rider)
{
    return rider == Rider::shield_of_faith ? EffectKind::shield_of_faith
           : rider == Rider::heroism       ? EffectKind::heroism
           : rider == Rider::bless         ? EffectKind::bless
           : rider == Rider::protection_from_evil_and_good
               ? EffectKind::protection_from_evil_and_good
           : rider == Rider::hunters_mark ? EffectKind::hunters_mark
           : rider == Rider::longstrider  ? EffectKind::longstrider
           // A minute of casting ends before its Concentration.
           : rider == Rider::animate_dead || rider == Rider::create_undead ||
                   rider == Rider::regenerate || rider == Rider::antimagic_field
               ? EffectKind::hunters_mark
           // Conjure Celestial and Conjure Minor Elementals last 10 minutes.
           : rider == Rider::conjure_celestial || rider == Rider::conjure_minor_elementals
               ? EffectKind::shield_of_faith
               : EffectKind::divine_favor;
}

// Effects that end with their caster's Concentration.
constexpr bool concentration_effect(EffectKind kind)
{
    return kind == EffectKind::shield_of_faith || kind == EffectKind::heroism ||
           kind == EffectKind::bless || kind == EffectKind::protection_from_evil_and_good ||
           kind == EffectKind::hunters_mark || kind == EffectKind::ensnaring_strike ||
           kind == EffectKind::entangle || kind == EffectKind::bane ||
           kind == EffectKind::hold_person || kind == EffectKind::resistance ||
           kind == EffectKind::guidance || kind == EffectKind::expeditious_retreat ||
           kind == EffectKind::drowsy || kind == EffectKind::asleep ||
           kind == EffectKind::laughing || kind == EffectKind::webbed ||
           kind == EffectKind::enfeebled || kind == EffectKind::blur ||
           kind == EffectKind::invisible || kind == EffectKind::enlarged ||
           kind == EffectKind::reduced || kind == EffectKind::dragons_breath ||
           kind == EffectKind::extended || kind == EffectKind::hex ||
           kind == EffectKind::outlined || kind == EffectKind::flame_blade ||
           kind == EffectKind::heated || kind == EffectKind::shining ||
           kind == EffectKind::hopeful || kind == EffectKind::aura_of_life ||
           kind == EffectKind::banished || kind == EffectKind::circling ||
           kind == EffectKind::ensnaring || kind == EffectKind::energy_ward ||
           kind == EffectKind::stoneskin || kind == EffectKind::dominated ||
           kind == EffectKind::cursed || kind == EffectKind::animating ||
           kind == EffectKind::dispel_evil || kind == EffectKind::knitting ||
           kind == EffectKind::hasted || kind == EffectKind::entranced ||
           kind == EffectKind::terrified || kind == EffectKind::vampiric ||
           kind == EffectKind::tentacled || kind == EffectKind::confused ||
           kind == EffectKind::greater_invisible || kind == EffectKind::time_slowed ||
           kind == EffectKind::haunted || kind == EffectKind::polymorphed ||
           kind == EffectKind::sphered || kind == EffectKind::minor_elementals ||
           kind == EffectKind::summoned || kind == EffectKind::spirit_breath ||
           kind == EffectKind::shared_resistance || kind == EffectKind::illusory ||
           kind == EffectKind::engulfed || kind == EffectKind::elemental_spirit ||
           kind == EffectKind::telekinetic || kind == EffectKind::eye_panic ||
           kind == EffectKind::sickened || kind == EffectKind::eye_resisted ||
           kind == EffectKind::eyebiting || kind == EffectKind::stoning ||
           kind == EffectKind::petrified || kind == EffectKind::dancing ||
           kind == EffectKind::aloft || kind == EffectKind::mazed || kind == EffectKind::compelled;
}

// The level of the spell an effect comes from (0 for a cantrip), or none for
// a feature's, a weapon's or a marker's, which Dispel Magic leaves be.
constexpr std::optional<unsigned> spell_level_of(EffectKind kind)
{
    using Kind = EffectKind;
    switch (kind)
    {
    case Kind::ray_of_frost:
    case Kind::shocking_grasp:
    case Kind::chill_touch:
    case Kind::resistance:
    case Kind::lit:
    case Kind::shillelagh:
    case Kind::produce_flame:
    case Kind::guidance:
        return 0u;
    case Kind::searing_smite:
    case Kind::shield_of_faith:
    case Kind::heroism:
    case Kind::divine_favor:
    case Kind::bless:
    case Kind::protection_from_evil_and_good:
    case Kind::command:
    case Kind::hunters_mark:
    case Kind::longstrider:
    case Kind::ensnaring_strike:
    case Kind::entangle:
    case Kind::guiding_bolt:
    case Kind::bane:
    case Kind::sanctuary:
    case Kind::expeditious_retreat:
    case Kind::mage_armor:
    case Kind::drowsy:
    case Kind::asleep:
    case Kind::laughing:
    case Kind::shield:
    case Kind::charmed:
    case Kind::hex:
    case Kind::outlined:
        return 1u;
    case Kind::blindness:
    case Kind::aid:
    case Kind::hold_person:
    case Kind::warding_bond:
    case Kind::protection_from_poison:
    case Kind::webbed:
    case Kind::acid_arrow:
    case Kind::enfeebled:
    case Kind::blur:
    case Kind::mirror_image:
    case Kind::magic_weapon:
    case Kind::invisible:
    case Kind::see_invisibility:
    case Kind::enlarged:
    case Kind::reduced:
    case Kind::dragons_breath:
    case Kind::barkskin:
    case Kind::flame_blade:
    case Kind::heated:
    case Kind::scorched:
    case Kind::shining:
        return 2u;
    case Kind::hopeful:
    case Kind::energy_ward:
    case Kind::cursed:
    case Kind::hasted:
    case Kind::time_slowed:
    case Kind::entranced:
    case Kind::terrified:
    case Kind::retching:
    case Kind::vampiric:
    case Kind::blinking:
        return 3u;
    case Kind::death_ward:
    case Kind::aura_of_life:
    case Kind::compelled:
    case Kind::free_moving:
    case Kind::banished:
    case Kind::stoneskin:
    case Kind::dominated:
    case Kind::tentacled:
    case Kind::confused:
    case Kind::fire_shield:
    case Kind::greater_invisible:
    case Kind::haunted:
    case Kind::polymorphed:
    case Kind::sphered:
    case Kind::vitriol:
    case Kind::minor_elementals:
        return 4u;
    case Kind::contagion:
    case Kind::dispel_evil:
    case Kind::shared_resistance:
    case Kind::engulfed:
    case Kind::lifted:
        return 5u;
    case Kind::true_seeing:
    case Kind::contingency:
    case Kind::eye_panic:
    case Kind::sickened:
    case Kind::stoning:
    case Kind::petrified:
    case Kind::dancing:
        return 6u;
    case Kind::regenerating:
    case Kind::aloft:
        return 7u;
    case Kind::mind_blank:
    case Kind::mazed:
        return 8u;
    default:
        return std::nullopt;
    }
}

[[nodiscard]] bool transmutable(DamageType type);
// Whether a readied Metamagic option can change this spell.
[[nodiscard]] bool metamagic_applies(Metamagic option, const SpellDef &spell);
// Command's option as players read it, such as "Approach".
[[nodiscard]] std::string command_label(CommandOption option);
// Spells whose creatures are chosen one click at a time (CLASS-2) when they may
// affect more than one.
// Magic Missile's darts and Scorching Ray's rays are each aimed at a creature,
// several at one if the caster likes (SRD 5.2.1 pp. 146, 159).
[[nodiscard]] bool splits_among_targets(const SpellDef &spell);
[[nodiscard]] bool selects_creatures(const SpellDef &spell);
// Contingency's spell (LEVELS-3): a spell of level 5 or lower cast with an
// action that can take effect on its caster, without a choice of its own.
[[nodiscard]] bool contingent(const SpellDef &spell);
// A feature's command that strikes the creature it targets (CLOSE-1).
[[nodiscard]] bool struck_by(std::string_view verb);
// And one that heals it.
[[nodiscard]] bool healed_by(std::string_view verb);
// A spell that charms its creature or controls its mind, which Mind Blank stops.
[[nodiscard]] bool charms(const SpellDef &spell);
// A Contingency trigger as players read it, by its index in contingency_triggers.
[[nodiscard]] std::string_view contingency_label(std::size_t trigger);
// A command that is a Magic action or another magical effect (Antimagic
// Field, LEVELS-4): Channel Divinity's uses, a spell's later actions, Lay On
// Hands, Wild Shape and a Dragonborn's Breath Weapon. Spells are kept out of
// the field where they are offered.
[[nodiscard]] bool magical_command(std::string_view verb);
// A smite follows the caster's own melee hit; divine_smite_free is Paladin's
// Smite's slotless cast, and a "_N" verb a cast from a higher slot.
[[nodiscard]] bool is_smite(std::string_view verb);
} // namespace opengold::srd5::detail
#endif
