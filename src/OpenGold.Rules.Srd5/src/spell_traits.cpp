#include "spell_traits.h"
#include <algorithm>

namespace opengold::srd5::detail
{
bool transmutable(DamageType type)
{
    return std::any_of(transmuted_types.begin(), transmuted_types.end(),
                       [&](auto id)
                       {
                           return damage_type(id) == type;
                       });
}

bool metamagic_applies(Metamagic option, const SpellDef &spell)
{
    const bool saves =
        spell.pattern == SpellPattern::save_damage || spell.pattern == SpellPattern::save_condition;
    const bool attacks =
        spell.pattern == SpellPattern::spell_attack || spell.pattern == SpellPattern::repeat_attack;
    switch (option)
    {
    case Metamagic::careful:
        return saves && spell.target == SpellTarget::area;
    case Metamagic::distant:
        return spell.target != SpellTarget::self;
    case Metamagic::empowered:
        return spell.dice.count && (attacks || spell.pattern == SpellPattern::save_damage);
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
        return (attacks || spell.pattern == SpellPattern::save_damage) &&
               transmutable(spell.damage);
    case Metamagic::twinned:
        return spell.upcast.extra_instances > 0;
    }
    return false;
}

std::string command_label(CommandOption option)
{
    const auto index = static_cast<std::size_t>(option) - 1;
    auto label = std::string(command_options.at(index));
    label[0] = static_cast<char>(label[0] - 'a' + 'A');
    return label;
}

bool splits_among_targets(const SpellDef &spell)
{
    return spell.id == "magic_missile" || spell.id == "scorching_ray" ||
           spell.id == "eldritch_blast";
}

bool selects_creatures(const SpellDef &spell)
{
    return splits_among_targets(spell) || spell.pattern == SpellPattern::buff ||
           spell.id == "chain_lightning" || spell.rider == Rider::command ||
           spell.rider == Rider::abjure_foes ||
           (spell.pattern == SpellPattern::heal && spell.instances > 1) ||
           (spell.pattern == SpellPattern::save_condition &&
            (spell.instances > 1 || spell.upcast.extra_instances));
}

bool contingent(const SpellDef &spell)
{
    return spell.level >= 1 && spell.level <= 5 && !spell.bonus_action &&
           (spell.pattern == SpellPattern::buff || spell.pattern == SpellPattern::heal) &&
           (spell.target == SpellTarget::self || spell.target == SpellTarget::ally ||
            spell.target == SpellTarget::wounded_ally) &&
           spell.instances == 1 && spell.rider != Rider::fire_shield &&
           spell.rider != Rider::protection_from_energy &&
           spell.rider != Rider::conjure_minor_elementals && spell.rider != Rider::enlarge_reduce &&
           spell.rider != Rider::regenerate && spell.rider != Rider::animate_objects &&
           spell.rider != Rider::mislead;
}

bool struck_by(std::string_view verb)
{
    static constexpr std::array<std::string_view, 12> verbs{"martial_arts",
                                                            "flurry_",
                                                            "evil_eye_",
                                                            "vampiric_touch_again",
                                                            "spiritual_weapon_strike",
                                                            "arcane_sword_strike",
                                                            "conjure_fey_strike",
                                                            "light_",
                                                            "nick_",
                                                            "divine_spark_radiant",
                                                            "superior_hunters_prey",
                                                            "horde_breaker"};
    return verb == "rampage" || std::any_of(verbs.begin(), verbs.end(),
                                            [&](std::string_view prefix)
                                            {
                                                return verb.starts_with(prefix);
                                            });
}

bool healed_by(std::string_view verb)
{
    return verb == "divine_spark" || verb.starts_with("lay_on_hands") || verb == "potent_vitality";
}

bool charms(const SpellDef &spell)
{
    return spell.rider == Rider::charm_person || spell.rider == Rider::dominate_beast ||
           spell.rider == Rider::dominate_person || spell.rider == Rider::dominate_monster ||
           spell.rider == Rider::magic_jar || spell.rider == Rider::irresistible_dance;
}

std::string_view contingency_label(std::size_t trigger)
{
    static constexpr std::array<std::string_view, 3> labels{
        "at half Hit Points", "when a fight starts", "at 0 Hit Points"};
    return labels.at(trigger);
}

bool magical_command(std::string_view verb)
{
    static constexpr std::array<std::string_view, 27> verbs{"dispel_break",
                                                            "compulsion_heading",
                                                            "dispel_dismissal",
                                                            "move_moonbeam",
                                                            "sunbeam_line",
                                                            "vampiric_touch_again",
                                                            "flame_blade_strike",
                                                            "hurl_flame",
                                                            "armor_of_shadows",
                                                            "fiendish_vigor",
                                                            "lands_aid",
                                                            "divine_spark",
                                                            "divine_spark_radiant",
                                                            "turn_undead",
                                                            "preserve_life",
                                                            "abjure_foes",
                                                            "tireless",
                                                            "spiritual_weapon_strike",
                                                            "move_spirit_pack",
                                                            "move_pillar",
                                                            "potent_vitality",
                                                            "sacred_weapon",
                                                            "sacred_weapon_radiant",
                                                            "move_hound",
                                                            "arcane_sword_strike",
                                                            "call_lightning_again",
                                                            "conjure_fey_strike"};
    return std::find(verbs.begin(), verbs.end(), verb) != verbs.end() ||
           verb.starts_with("lay_on_hands") || verb.starts_with("wild_shape") ||
           verb.starts_with("dragons_breath_exhale") || verb.starts_with("breath_weapon") ||
           verb.starts_with("divine_strike") || verb.starts_with("channel_");
}

bool is_smite(std::string_view verb)
{
    const auto spell = without_cast_suffix(verb);
    return spell == "divine_smite" || verb == "divine_smite_free" || spell == "searing_smite" ||
           spell == "shining_smite" || spell == "ensnaring_strike";
}
} // namespace opengold::srd5::detail
