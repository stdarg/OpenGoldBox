#include "combat_model.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <iterator>
#include <limits>
#include <numeric>
#include <set>
#include <sstream>
#include <stdexcept>

namespace opengold::srd5::detail
{
using namespace rules;

bool unconscious(const Actor &a)
{
    return a.life.hp == 0;
}

int return_penalty(const Actor &a)
{
    for (const auto &e : a.effects.active)
    {
        if (e.kind == detail::EffectKind::returned)
        {
            return e.dc;
        }
    }
    return 0;
}

bool conscious(const Actor &a)
{
    return !a.life.dead && !unconscious(a);
}

bool absent(const Actor &a)
{
    return detail::has_effect(a.effects, detail::EffectKind::banished) ||
           detail::has_effect(a.effects, detail::EffectKind::ethereal) ||
           detail::has_effect(a.effects, detail::EffectKind::jaunted) ||
           detail::has_effect(a.effects, detail::EffectKind::mazed) ||
           detail::has_effect(a.effects, detail::EffectKind::hurled);
}

bool aloft(const Actor &a)
{
    return detail::has_effect(a.effects, detail::EffectKind::aloft);
}

std::string recovered_levels(const std::array<unsigned, 5> &slots)
{
    std::string levels;
    for (unsigned level = 5; level >= 1; --level)
    {
        for (unsigned n = 0; n < slots[level - 1]; ++n)
        {
            levels += (levels.empty() ? "" : ", ") + std::to_string(level);
        }
    }
    return levels;
}

std::vector<ArcaneAllocation> arcane_allocations(unsigned budget)
{
    std::vector<ArcaneAllocation> result;
    std::array<unsigned, 5> slots{};
    const auto add = [&]
    {
        std::string id =
            "arcane_recovery:" + std::to_string(slots[0]) + ":" + std::to_string(slots[1]);
        unsigned last = 1;
        for (unsigned level = 3; level <= 5; ++level)
        {
            if (slots[level - 1])
            {
                last = level - 1;
            }
        }
        for (unsigned index = 2; index <= last; ++index)
        {
            id += ":" + std::to_string(slots[index]);
        }
        const auto levels = recovered_levels(slots);
        // Built in one place, not in a ?: chain: GCC 16 takes the arguments'
        // initializer list in a ?: branch for a dangling pointer.
        Message label{"Spell slots of levels {levels}", {{"levels", levels}}};
        if (levels == "1")
        {
            label = Message{"One level-one spell slot", {}};
        }
        else if (levels == "1, 1")
        {
            label = Message{"Two level-one spell slots", {}};
        }
        else if (levels == "2")
        {
            label = Message{"One level-two spell slot", {}};
        }

        result.push_back({std::move(id), std::move(label), slots});
    };
    const auto choose = [&](const auto &self, unsigned level, unsigned left) -> void
    {
        if (level == 0)
        {
            if (left < budget)
            {
                add();
            }
            return;
        }
        for (unsigned count = 0; count * level <= left; ++count)
        {
            slots[level - 1] = count;
            self(self, level - 1, left - count * level);
        }
        slots[level - 1] = 0;
    };
    choose(choose, 5, budget);
    return result;
}

bool gives_condition(detail::EffectKind kind, std::string_view condition)
{
    if (condition == "blinded")
    {
        return kind == detail::EffectKind::blindness || kind == detail::EffectKind::dazzled;
    }
    if (condition == "paralyzed")
    {
        return kind == detail::EffectKind::hold_person ||
               kind == detail::EffectKind::paralysis_ray || kind == detail::EffectKind::clawed ||
               kind == detail::EffectKind::envenomed_paralyzed;
    }
    return kind == detail::EffectKind::poisoned || kind == detail::EffectKind::contagion ||
           kind == detail::EffectKind::retching || kind == detail::EffectKind::sickened ||
           kind == detail::EffectKind::envenomed || kind == detail::EffectKind::envenomed_paralyzed;
}

std::string condition_name(std::string_view condition)
{
    return condition == "blinded" ? "Blinded" : condition == "paralyzed" ? "Paralyzed" : "Poisoned";
}

bool has_condition(const Actor &a, std::string_view condition)
{
    return std::any_of(a.effects.active.begin(), a.effects.active.end(),
                       [&](const auto &e)
                       {
                           return gives_condition(e.kind, condition);
                       });
}

bool suffers_poisoned(const Actor &a)
{
    return has_condition(a, "poisoned") &&
           !detail::has_effect(a.effects, detail::EffectKind::feasted);
}

bool touch_restorable(const Actor &a)
{
    return std::any_of(a.effects.active.begin(), a.effects.active.end(),
                       [](const auto &e)
                       {
                           return std::any_of(
                               restorable_conditions.begin(), restorable_conditions.end(),
                               [&](const auto &condition)
                               {
                                   return std::find(condition.second.begin(),
                                                    condition.second.end(),
                                                    e.kind) != condition.second.end();
                               });
                       });
}

bool curse(detail::EffectKind kind)
{
    return kind == detail::EffectKind::hex || kind == detail::EffectKind::cursed ||
           kind == detail::EffectKind::rotting_curse;
}

void bring_back(Actor &patient, int hit_points)
{
    std::erase_if(patient.effects.active,
                  [](const auto &e)
                  {
                      return e.kind == detail::EffectKind::fallen ||
                             e.kind == detail::EffectKind::deceased ||
                             e.kind == detail::EffectKind::poisoned ||
                             e.kind == detail::EffectKind::dust;
                  });
    patient.life.dead = false;
    patient.life.successes = patient.life.failures = 0;
    patient.life.stable = false;
    patient.life.recovery = {};
    patient.life.hp = hit_points;
}

bool poison(detail::EffectKind kind)
{
    return gives_condition(kind, "poisoned") && kind != detail::EffectKind::contagion;
}

bool temple_helps(const Actor &a, TempleService service)
{
    const auto any = [&](auto &&matches)
    {
        return std::any_of(a.effects.active.begin(), a.effects.active.end(),
                           [&](const auto &e)
                           {
                               return matches(e.kind);
                           });
    };
    // Raise Dead: dead up to ten days and not turned to dust; Resurrection
    // the rest.
    const bool raisable = detail::has_effect(a.effects, detail::EffectKind::deceased) &&
                          !detail::has_effect(a.effects, detail::EffectKind::dust);
    if (service == TempleService::raise_dead || service == TempleService::resurrection)
    {
        return a.life.dead && raisable == (service == TempleService::raise_dead);
    }
    if (a.life.dead)
    {
        return false;
    }
    switch (service)
    {
    case TempleService::cure_light_wounds:
    case TempleService::cure_serious_wounds:
    case TempleService::cure_critical_wounds:
        return a.life.hp < max_hp(a);
    case TempleService::cure_blindness:
        return has_condition(a, "blinded");
    case TempleService::cure_disease:
        return detail::has_effect(a.effects, detail::EffectKind::contagion);
    case TempleService::neutralize_poison:
        return any(poison);
    case TempleService::remove_curse:
        return any(curse);
    case TempleService::stone_to_flesh:
        return any(
            [](detail::EffectKind kind)
            {
                return kind == detail::EffectKind::petrified || kind == detail::EffectKind::stoning;
            });
    default:
        return false;
    }
}

bool bound_to_cursed_item(const Actor &a, bool weapons_only)
{
    const auto &keys = a.definition.equipment_keys;
    return !a.curse_lifted &&
           std::any_of(keys.begin(), keys.end(),
                       [&](const auto &key)
                       {
                           return rules::parse_item_key(key).power == rules::ItemPower::cursed &&
                                  (!weapons_only || detail::weapon(key));
                       });
}

bool charmable_snake(const Actor &c)
{
    const auto &id = c.source.definition;
    return id.find("snake") != std::string::npos || id.find("scorpion") != std::string::npos;
}

bool cursed(const Actor &a)
{
    return bound_to_cursed_item(a) || std::any_of(a.effects.active.begin(), a.effects.active.end(),
                                                  [](const auto &e)
                                                  {
                                                      return curse(e.kind);
                                                  });
}

std::string ward_label(std::size_t type)
{
    static constexpr std::array<std::string_view, 5> labels{"Celestials", "Elementals", "Fey",
                                                            "Fiends", "Undead"};
    return std::string(labels.at(type));
}

std::string curse_label(std::size_t curse)
{
    static constexpr std::array<std::string_view, 9> labels{"Strength",
                                                            "Dexterity",
                                                            "Constitution",
                                                            "Intelligence",
                                                            "Wisdom",
                                                            "Charisma",
                                                            "Disadvantage attacking you",
                                                            "forced Dodge",
                                                            "extra Necrotic damage"};
    return std::string(labels.at(curse));
}

std::string wall_direction_label(std::size_t direction)
{
    static constexpr std::array<std::string_view, 5> labels{
        "horizontal", "vertical", "rising diagonal", "falling diagonal", "ring"};
    return std::string(labels.at(direction));
}

std::size_t wave_heading(std::string_view verb, Cell origin, Cell center)
{
    // Across a row, a column, a rising or a falling diagonal.
    static constexpr std::array<std::size_t, 4> across{0, 2, 4, 6};
    const auto forward = across[detail::wall_direction(verb).value_or(0)];
    const auto step = wave_headings[forward];
    const bool backward = (center.x - origin.x) * step.x + (center.y - origin.y) * step.y < 0;
    return backward ? forward + 1 : forward;
}

std::size_t ward_of(std::string_view verb)
{
    const auto type =
        detail::without_cast_suffix(verb).substr(std::string_view("magic_circle_").size());
    return static_cast<std::size_t>(
        std::find(detail::ward_types.begin(), detail::ward_types.end(), type) -
        detail::ward_types.begin());
}

bool has_greater_harm(const Actor &a, std::size_t harm)
{
    return std::any_of(a.effects.active.begin(), a.effects.active.end(),
                       [&](const auto &e)
                       {
                           return harm == 4   ? e.kind == detail::EffectKind::befuddled
                                  : harm == 3 ? e.kind == detail::EffectKind::petrified
                                  : harm == 2 ? e.kind == detail::EffectKind::drained ||
                                                    e.kind == detail::EffectKind::life_drained
                                  : harm      ? curse(e.kind)
                                              : e.kind == detail::EffectKind::charmed ||
                                                    e.kind == detail::EffectKind::enthralled ||
                                                    e.kind == detail::EffectKind::turn_charmed ||
                                                    e.kind == detail::EffectKind::compelled ||
                                                    e.kind == detail::EffectKind::dominated;
                       });
}

bool restorable(const Actor &a)
{
    return std::any_of(detail::restoration_conditions.begin(), detail::restoration_conditions.end(),
                       [&](std::string_view condition)
                       {
                           return has_condition(a, condition);
                       });
}

int next_hit_die(const Definition &d, int remaining)
{
    if (d.level_dice.empty() || remaining < 1)
    {
        return d.hit_die;
    }
    return d.level_dice[static_cast<std::size_t>(
        std::min<int>(remaining, static_cast<int>(d.level_dice.size())) - 1)];
}

int recovery_class_level(const Definition &d)
{
    return std::max(d.arcane ? d.class_levels[CharacterClass::wizard] : 0,
                    d.natural_recovery ? d.class_levels[CharacterClass::druid] : 0);
}

bool can_recover(const Actor &actor, const ArcaneAllocation &choice)
{
    const auto &d = actor.definition;
    // Signed comparisons: an actor holding more slots than its definition grants
    // (the room Font of Magic allows) has a negative spent count, which must
    // refuse the choice rather than wrap to a huge unsigned room.
    unsigned total = 0;
    for (unsigned level = 1; level <= 5; ++level)
    {
        total += level * choice.slots[level - 1];
        if (std::cmp_greater(choice.slots[level - 1], d.slots[level] - actor.slots[level]))
        {
            return false;
        }
    }
    // Arcane Recovery (Wizard) or Natural Recovery (Land Druid 6).
    // Half the Wizard's or Land Druid's level, rounded up (MULTI-1).
    return conscious(actor) &&
           ((d.arcane && actor.arcane[CharacterClass::wizard] > 0) ||
            (d.natural_recovery && actor.arcane[CharacterClass::druid] > 0)) &&
           std::cmp_less_equal(total, (recovery_class_level(d) + 1) / 2);
}

int movement_left(const Actor &a)
{
    if (!conscious(a) || a.aim_used)
    {
        return 0;
    }
    // A resisted Stunning Strike halves the Speed, and so does Slow.
    const int staggered =
        (detail::has_effect(a.effects, detail::EffectKind::staggered) ? a.definition.speed / 2
                                                                      : 0) +
        (detail::has_effect(a.effects, detail::EffectKind::time_slowed) ? a.definition.speed / 2
                                                                        : 0);
    // Haste doubles the Speed: each Dash's share as well. A Resilient Sphere
    // rolls at half.
    const int hasted =
        detail::has_effect(a.effects, detail::EffectKind::hasted) ? a.definition.speed : 0;
    const int rolling =
        detail::has_effect(a.effects, detail::EffectKind::sphered) ? a.definition.speed / 2 : 0;
    const int penalty =
        std::min(a.definition.speed, detail::speed_penalty(a.effects) + staggered + rolling) -
        hasted;
    return std::max(0, a.movement - penalty * (1 + a.dashes));
}

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
        for (unsigned level = 1; level <= max_slot_level; ++level)
        {
            in >> a.slots[level];
        }
        in >> a.life.successes >> a.life.failures >> a.life.stable >> a.hit_dice >>
            a.life.recovery.death_save_in_ms >> a.life.recovery.stable_recovery_in_ms >>
            a.life.temporary_hp.amount >> std::quoted(a.life.temporary_hp.source_id) >> a.rushes;
        read_pool(in, a.surges, surge_capacity(a.definition));
        read_pool(in, a.arcane, arcane_capacity(a.definition));
        read_pool(in, a.lay_on_hands, lay_capacity(a.definition));
        read_pool(in, a.free_casts, free_cast_capacity(a.definition));
        read_pool(in, a.channel_divinity, channel_capacity(a.definition));
        in >> a.initiate_casts;
        if (!in || magic != vitals_magic)
        {
            throw std::runtime_error("Invalid character resource state");
        }
        detail::decode_stable_recovery(a.life.recovery);
        a.effects = detail::read_effects(in);
        in >> std::ws;
        if (!in.eof())
        {
            throw std::runtime_error("Trailing character resource state");
        }
    }
    // A simulacrum is a Construct.
    if (detail::has_effect(a.effects, detail::EffectKind::simulacrum))
    {
        a.definition.creature_type = "construct";
    }
    const auto &d = a.definition;
    if (!slots_in_range(a.slots, d))
    {
        throw std::runtime_error("Invalid character vitals");
    }
    if (a.life.hp < 0 || a.life.hp > max_hp(a) || (a.life.dead && a.life.hp != 0) || a.winds < 0 ||
        a.winds > d.winds || !within(a.arcane, arcane_capacity(d)) ||
        !within(a.surges, surge_capacity(d)) || a.rushes < 0 || a.rushes > d.rushes ||
        a.hit_dice < 0 || a.hit_dice > (d.hit_die ? d.level : 0) || a.life.successes < 0 ||
        a.life.successes > 3 || a.life.failures < 0 || a.life.failures > 4 ||
        !within(a.lay_on_hands, lay_capacity(d)) || !within(a.free_casts, free_cast_capacity(d)) ||
        !within(a.channel_divinity, channel_capacity(d)) || a.initiate_casts < 0 ||
        (a.initiate_casts & ~initiate_capacity(d)))
    {
        throw std::runtime_error("Invalid character vitals");
    }
    detail::validate_recovery(a.life);
    detail::validate_temporary_hp(a.life.temporary_hp);
    if (a.life.recovery.stable_recovery_due && !detail::healing_blocked(a.effects))
    {
        throw std::runtime_error("Earned recovery requires active healing prevention");
    }
}

Actor actor_from(const Definition &definition, const VitalState &state)
{
    Actor actor;
    actor.definition = definition;
    actor.winds = definition.winds;
    actor.slots = definition.slots;
    restore_vitals(actor, state);
    return actor;
}

void end_befuddlement(Actor &a)
{
    std::erase_if(a.effects.active,
                  [](const auto &e)
                  {
                      return e.kind == detail::EffectKind::befuddled;
                  });
}

VitalState vitals(const Actor &a)
{
    std::ostringstream out;
    out << vitals_magic << ' ' << a.winds << ' ';
    for (unsigned level = 1; level <= max_slot_level; ++level)
    {
        out << a.slots[level] << ' ';
    }
    out << a.life.successes << ' ' << a.life.failures << ' ' << a.life.stable << ' ' << a.hit_dice
        << ' ' << a.life.recovery.death_save_in_ms << ' '
        << detail::encode_stable_recovery(a.life.recovery) << ' ' << a.life.temporary_hp.amount
        << ' ' << std::quoted(a.life.temporary_hp.source_id) << ' ' << a.rushes << ' ';
    const auto &d = a.definition;
    write_pool(out, a.surges, surge_capacity(d));
    out << ' ';
    write_pool(out, a.arcane, arcane_capacity(d));
    out << ' ';
    write_pool(out, a.lay_on_hands, lay_capacity(d));
    out << ' ';
    write_pool(out, a.free_casts, free_cast_capacity(d));
    out << ' ';
    write_pool(out, a.channel_divinity, channel_capacity(d));
    out << ' ' << a.initiate_casts << ' ';
    // A creature hides from one fight's enemies; it is not hidden after it.
    auto effects = all_effects(a);
    std::erase_if(effects.active,
                  [](const auto &e)
                  {
                      return e.kind == detail::EffectKind::hidden;
                  });
    detail::write_effects(out, effects);
    std::string description;
    for (unsigned level = 1; level <= max_slot_level; ++level)
    {
        if (a.definition.slots[level])
        {
            description += (description.empty() ? "" : "\n") + std::string(slot_labels[level]) +
                           ": " + std::to_string(a.slots[level]) + " / " +
                           std::to_string(a.definition.slots[level]);
        }
    }
    if (a.definition.winds)
    {
        description = "Second Wind uses: " + std::to_string(a.winds) + " / " +
                      std::to_string(a.definition.winds);
    }
    if (a.definition.arcane)
    {
        description += (description.empty() ? "" : "\n") + std::string("Arcane Recovery uses: ") +
                       std::to_string(a.arcane[CharacterClass::wizard]) + " / 1";
    }
    if (a.definition.surges && a.surges[CharacterClass::fighter] < a.definition.surges)
    {
        description += (description.empty() ? "" : "\n") + std::string("Action Surge uses: ") +
                       std::to_string(a.surges[CharacterClass::fighter]) + " / " +
                       std::to_string(a.definition.surges);
    }
    if (a.life.hp == 0)
    {
        description +=
            (description.empty() ? "" : "\n") +
            std::string(a.life.dead     ? "Dead"
                        : a.life.stable ? "Stable, unconscious"
                                        : "Unconscious; death saves ") +
            (!a.life.dead && !a.life.stable ? std::to_string(a.life.successes) + " successes, " +
                                                  std::to_string(a.life.failures) + " failures"
                                            : "");
    }
    return {a.life.hp, a.life.dead, out.str(), description};
}
} // namespace opengold::srd5::detail
