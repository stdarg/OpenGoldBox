#include "actor_model.h"
#include "armor.h"
#include "character_definition.h"
#include "combat_geometry.h"
#include "definition.h"
#include "resource_pools.h"
#include "spell_table.h"
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

int slot_total(const SpellSlots &slots)
{
    return std::accumulate(slots.begin(), slots.end(), 0);
}

SpellSlots spell_slots(CharacterClass klass, unsigned level)
{
    using Row = std::array<int, 9>;
    static constexpr std::array<Row, 15> full{{{2},
                                               {3},
                                               {4, 2},
                                               {4, 3},
                                               {4, 3, 2},
                                               {4, 3, 3},
                                               {4, 3, 3, 1},
                                               {4, 3, 3, 2},
                                               {4, 3, 3, 3, 1},
                                               {4, 3, 3, 3, 2},
                                               {4, 3, 3, 3, 2, 1},
                                               {4, 3, 3, 3, 2, 1},
                                               {4, 3, 3, 3, 2, 1, 1},
                                               {4, 3, 3, 3, 2, 1, 1},
                                               {4, 3, 3, 3, 2, 1, 1, 1}}};
    static constexpr std::array<Row, 15> half{{{2},
                                               {2},
                                               {3},
                                               {3},
                                               {4, 2},
                                               {4, 2},
                                               {4, 3},
                                               {4, 3},
                                               {4, 3, 2},
                                               {4, 3, 2},
                                               {4, 3, 3},
                                               {4, 3, 3},
                                               {4, 3, 3, 1},
                                               {4, 3, 3, 1},
                                               {4, 3, 3, 2}}};
    SpellSlots result{};
    if (level < 1 || level > full.size())
    {
        return result;
    }
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

rules::Missile weapon_missile(const detail::Weapon &weapon)
{
    if (weapon.key == "sling")
    {
        return rules::Missile::stone;
    }
    return weapon.thrown ? rules::Missile::axe : rules::Missile::arrow;
}

CombatDisplay combat_display(std::string_view definition)
{
    if (definition == "slums-kobold")
    {
        return {"Kobold", "Dagger", "Dagger"};
    }
    if (definition == "slums-kobold-leader" || definition == "slums-kobold-leader-sword")
    {
        return {"Kobold Leader", "Dagger", "Dagger"};
    }
    if (definition == "bandit")
    {
        return {"Bandit", "Scimitar", "Light crossbow"};
    }
    if (definition == "slums-goblin")
    {
        return {"Goblin Guard", "Scimitar", "Shortbow"};
    }
    if (definition == "slums-goblin-leader")
    {
        return {"Goblin Leader", "Scimitar", "Shortbow"};
    }
    if (definition == "slums-orc")
    {
        return {"Orc", "Greataxe", "Javelin"};
    }
    if (definition == "slums-orc-leader")
    {
        return {"Orc Leader", "Greataxe", "Javelin"};
    }
    if (definition == "slums-orc-leader-archer")
    {
        return {"Orc Leader", "Greataxe", "Longbow"};
    }
    if (definition == "slums-bugbear")
    {
        return {"Bugbear", "Grab", "Light hammer"};
    }
    // The Slums' set encounters and Kuto's Well: their SRD stat blocks' attacks.
    if (definition == "slums-hobgoblin")
    {
        return {"Hobgoblin", "Longsword", "Longbow"};
    }
    if (definition == "ogre")
    {
        return {"Ogre", "Greatclub", "Javelin"};
    }
    if (definition == "troll")
    {
        return {"Troll", "Rend", nullptr};
    }
    if (definition == "slums-magic-user")
    {
        return {"Magic-User", "Dagger", nullptr};
    }
    if (definition == "gnoll-warrior")
    {
        return {"Gnoll Warrior", "Rend", "Bone bow"};
    }
    if (definition == "lizardfolk")
    {
        return {"Lizardfolk", "Bite", "Javelin"};
    }
    if (definition == "giant-lizard")
    {
        return {"Giant Lizard", "Bite", nullptr};
    }
    if (definition == "norris-the-gray")
    {
        return {"Norris the Gray", "Scimitar", nullptr};
    }
    return {nullptr, nullptr, nullptr};
}

detail::EffectState all_effects(const Actor &a)
{
    auto effects = a.effects;
    effects.active.insert(effects.active.end(), a.suppressed.begin(), a.suppressed.end());
    std::sort(effects.active.begin(), effects.active.end(),
              [](const auto &x, const auto &y)
              {
                  return x.id < y.id;
              });
    return effects;
}

rules::MessageArgument effect_value(const detail::Effect &effect, detail::EffectValue value)
{
    if (value == detail::EffectValue::damage_type)
    {
        std::string name(detail::damage_name(static_cast<detail::DamageType>(effect.dc)));
        name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
        return {"value", name, true};
    }
    if (value == detail::EffectValue::beast_form)
    {
        return {"value",
                std::string(detail::beast_forms.at(static_cast<std::size_t>(effect.dc - 1)).label),
                true};
    }
    return {"value", std::to_string(effect.dc)};
}

std::vector<rules::StatusEffectView> status_effect_views(const Actor &a, bool oiled)
{
    std::vector<rules::StatusEffectView> views;
    for (const auto &effect : all_effects(a).active)
    {
        const auto *label = detail::effect_label(effect.kind);
        // Wild Resurgence's `dc` 0 only marks this turn's slot.
        if (!label || (effect.kind == detail::EffectKind::resurged && !effect.dc))
        {
            continue;
        }
        rules::StatusEffectView view{{std::string(label->name), {}},
                                     {std::string(label->description), {}}};
        if (label->value != detail::EffectValue::none)
        {
            view.description.arguments.push_back(effect_value(effect, label->value));
        }
        if (effect.source_name != a.source.name)
        {
            view.source = effect.source_name;
        }
        view.end = detail::ends_with_long_rest(effect.kind)      ? rules::EffectEnd::long_rest
                   : effect.kind == detail::EffectKind::returned ? rules::EffectEnd::untimed
                   : effect.remaining_ms > lasting_effect_ms     ? rules::EffectEnd::removed
                                                                 : rules::EffectEnd::timed;
        view.remaining = std::chrono::milliseconds(effect.remaining_ms);
        views.push_back(std::move(view));
    }
    const auto state = [&](bool shown, rules::Message name, rules::Message description)
    {
        if (shown)
        {
            views.push_back(
                {std::move(name), std::move(description), {}, rules::EffectEnd::untimed});
        }
    };
    state(
        a.effects.prone, {"Prone", {}},
        {"Attack rolls against it from within 5 feet have Advantage, others Disadvantage; standing up costs half its Speed.",
         {}});
    state(a.burning, {"Burning", {}},
          {"Takes 1d4 Fire damage at the start of each of its turns.", {}});
    state(oiled, {"Covered in oil", {}}, {"Fire damage deals it 5 more.", {}});
    state(a.panicked && !a.must_stay, {"Fleeing in panic", {}},
          {"Its morale broke; it runs for the edge of the field.", {}});
    state(
        a.panicked && a.must_stay, {"Charging in panic", {}},
        {"It could not get away. It runs at its enemies and strikes wildly at whoever is in reach, friend or foe, with Disadvantage; attack rolls against it have Advantage, and it has no Bonus Actions or Reactions.",
         {}});
    if (const auto &held = a.concentration.active())
    {
        views.push_back({{"Concentrating", {}},
                         {"Taking damage calls for a Constitution save to keep its spell.", {}},
                         {},
                         rules::EffectEnd::timed,
                         std::chrono::milliseconds(held->remaining_ms)});
    }
    return views;
}

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
           (detail::has_effect(target.effects, detail::EffectKind::hasted) ? 2 : 0) -
           (detail::has_effect(target.effects, detail::EffectKind::time_slowed) ? 2 : 0) +
           (detail::has_effect(target.effects, detail::EffectKind::shield) ? 5 : 0);
}

int remaining_of(const ResourceDescriptor &descriptor, const Actor &a)
{
    return descriptor.slot_level ? a.slots[descriptor.slot_level]
           : descriptor.pool     ? (a.*descriptor.pool)[descriptor.owner]
                                 : a.*descriptor.remaining;
}

int capacity_of(const ResourceDescriptor &descriptor, const Definition &d)
{
    return descriptor.slot_level ? d.slots[descriptor.slot_level] : d.*descriptor.capacity;
}

bool takes_lessons_skilled(const AdvancementChoice &choice)
{
    return choice.option_swap.replacement == "lessons_skilled" ||
           std::any_of(choice.training.begin(), choice.training.end(),
                       [](const auto &picks)
                       {
                           return picks.first.starts_with("class:warlock:invocations:") &&
                                  std::find(picks.second.begin(), picks.second.end(),
                                            "lessons_skilled") != picks.second.end();
                       });
}

int max_hp(const Actor &a)
{
    return std::max(1, a.definition.hp + detail::hit_point_bonus(a.effects));
}

ClassCounts lay_capacity(const Definition &d)
{
    ClassCounts c;
    c[CharacterClass::paladin] = d.lay_on_hands;
    c[CharacterClass::sorcerer] = d.sorcery_points;
    c[CharacterClass::barbarian] = d.relentless_rage;
    c[CharacterClass::ranger] = d.natures_veil;
    return c;
}

unsigned pact_level(const SpellSlots &slots)
{
    return static_cast<unsigned>(std::find_if(slots.begin() + 1, slots.end(),
                                              [](int count)
                                              {
                                                  return count > 0;
                                              }) -
                                 slots.begin());
}

int slot_room(const Definition &d, unsigned level)
{
    const auto &cost = font_of_magic_cost;
    return d.slots[level] + (level < cost.size() ? d.sorcery_points / cost[level] : 0);
}

bool slots_in_range(const SpellSlots &slots, const Definition &d)
{
    for (unsigned level = 1; level <= max_slot_level; ++level)
    {
        if (slots[level] < 0 || slots[level] > slot_room(d, level))
        {
            return false;
        }
    }
    return slots[0] == 0;
}

unsigned lowest_slot(const Actor &a, unsigned from)
{
    for (unsigned level = from; level <= max_slot_level; ++level)
    {
        if (a.slots[level] > 0)
        {
            return level;
        }
    }
    return 0;
}

ClassCounts free_cast_capacity(const Definition &d)
{
    ClassCounts c;
    c[CharacterClass::paladin] = d.free_smite;
    c[CharacterClass::ranger] = d.favored_enemy;
    c[CharacterClass::sorcerer] = d.innate_sorcery;
    c[CharacterClass::monk] = d.wholeness;
    c[CharacterClass::barbarian] = d.intimidating_presence;
    return c;
}

int initiate_capacity(const Definition &d)
{
    return (1 << d.initiate_spells.size()) - 1;
}

int initiate_bit(const Definition &d, std::string_view spell)
{
    const auto found = std::find(d.initiate_spells.begin(), d.initiate_spells.end(), spell);
    return found == d.initiate_spells.end() ? 0 : 1 << (found - d.initiate_spells.begin());
}

int casting_bonus(const Definition &d, std::string_view spell)
{
    const auto found = d.initiate_casting.find(spell);
    return found == d.initiate_casting.end() ? d.casting : found->second;
}

int casting_modifier(const Definition &d, std::string_view spell)
{
    return casting_bonus(d, spell) - (2 + (d.level - 1) / 4);
}

int cantrip_tier(const Definition &d)
{
    return d.level >= 17 ? 4 : d.level >= 11 ? 3 : d.level >= 5 ? 2 : 1;
}

ClassCounts surge_capacity(const Definition &d)
{
    ClassCounts c;
    c[CharacterClass::fighter] = d.surges;
    c[CharacterClass::monk] = d.focus;
    return c;
}

ClassCounts arcane_capacity(const Definition &d)
{
    ClassCounts c;
    c[CharacterClass::wizard] = d.arcane;
    c[CharacterClass::monk] = d.metabolism;
    c[CharacterClass::warlock] = d.magical_cunning;
    c[CharacterClass::bard] = d.bardic_inspiration;
    c[CharacterClass::fighter] = d.indomitable;
    c[CharacterClass::barbarian] = d.persistent_rage;
    c[CharacterClass::ranger] = d.tireless;
    c[CharacterClass::cleric] = d.divine_intervention;
    c[CharacterClass::sorcerer] = d.sorcerous_restoration;
    c[CharacterClass::druid] = d.natural_recovery;
    return c;
}

ClassCounts channel_capacity(const Definition &d)
{
    ClassCounts c;
    c[CharacterClass::cleric] = d.channel_divinity;
    c[CharacterClass::barbarian] = d.rages;
    c[CharacterClass::druid] = d.wild_shapes;
    return c;
}

bool within(const ClassCounts &counts, const ClassCounts &capacity)
{
    for (std::size_t n = 0; n < counts.by_class.size(); ++n)
    {
        if (counts.by_class[n] < 0 || counts.by_class[n] > capacity.by_class[n])
        {
            return false;
        }
    }
    return true;
}

void cap(ClassCounts &counts, const ClassCounts &capacity)
{
    for (std::size_t n = 0; n < counts.by_class.size(); ++n)
    {
        counts.by_class[n] = std::min(counts.by_class[n], capacity.by_class[n]);
    }
}

void grow(ClassCounts &counts, const ClassCounts &now, const ClassCounts &before)
{
    for (std::size_t n = 0; n < counts.by_class.size(); ++n)
    {
        counts.by_class[n] += now.by_class[n] - before.by_class[n];
    }
}

std::ostream &operator<<(std::ostream &out, const ClassCounts &counts)
{
    for (std::size_t n = 0; n < counts.by_class.size(); ++n)
    {
        out << (n ? " " : "") << counts.by_class[n];
    }
    return out;
}

std::istream &operator>>(std::istream &in, ClassCounts &counts)
{
    for (auto &count : counts.by_class)
    {
        in >> count;
    }
    return in;
}

void write_pool(std::ostream &out, const ClassCounts &counts, const ClassCounts &capacity)
{
    bool written = false;
    for (std::size_t n = 0; n < counts.by_class.size(); ++n)
    {
        if (capacity.by_class[n])
        {
            out << (written ? " " : "") << counts.by_class[n];
            written = true;
        }
    }
    if (!written)
    {
        out << 0;
    }
}

void read_pool(std::istream &in, ClassCounts &counts, const ClassCounts &capacity)
{
    counts = {};
    bool read = false;
    for (std::size_t n = 0; n < counts.by_class.size(); ++n)
    {
        if (capacity.by_class[n])
        {
            in >> counts.by_class[n];
            read = true;
        }
    }
    if (!read)
    {
        int none{};
        in >> none;
        if (none)
        {
            in.setstate(std::ios::failbit);
        }
    }
}

Definition strengthened(Definition d, int giant)
{
    const int gain = giant - d.strength;
    if (gain <= 0)
    {
        return d;
    }
    if (d.melee_ability == d.strength)
    {
        d.melee_bonus += gain;
        d.melee.bonus += gain;
        d.melee_ability = giant;
    }
    d.saves[static_cast<std::size_t>(Ability::strength)] += gain;
    d.athletics += gain;
    d.strength = giant;
    return d;
}

Definition shaped(Definition d, const detail::BeastForm &form)
{
    d.ac = form.armor_class;
    d.mage_armor_ac = 0;
    d.speed = form.speed;
    d.size = form.size;
    d.footprint = form.footprint;
    d.strength = form.strength;
    d.dexterity = form.dexterity;
    std::copy(form.saves.begin(), form.saves.end(), d.saves.begin());
    d.melee = form.attack;
    d.melee_bonus = form.attack_bonus;
    d.melee_ability = form.attack.bonus;
    d.melee_type = detail::DamageType::piercing;
    d.reach = 5;
    d.weapon_label = std::string(form.attack_label);
    d.finesse = d.ranged_weapon = d.ranged_loading = d.shield = d.other_weapon = false;
    d.ranged_missile.reset();
    d.weapon_hands = 0;
    d.versatile_sides = 0;
    d.melee_magic = d.ranged_magic = 0;
    d.bane_undead = d.flame_tongue = false;
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
    d.prone_bite = form.prone_bite || form.prone_large;
    d.prone_large = form.prone_large;
    if (form.slashing)
    {
        d.melee_type = detail::DamageType::slashing;
    }
    d.bloodied_fury = form.bloodied_fury;
    d.charge = form.charge;
    return d;
}

unsigned short_rest_recovery(const ResourceDescriptor &descriptor, const Definition &d,
                             unsigned capacity)
{
    const bool whole = descriptor.short_rest < 0 || (descriptor.slot_level && d.pact_magic) ||
                       (descriptor.id == "bardic_inspiration" && d.font_of_inspiration);
    // A Warlock with another Spellcasting class gets its Pact Magic slots
    // back among the others of their level (MULTI-1).
    if (!whole && descriptor.slot_level && d.pact_slots &&
        descriptor.slot_level == static_cast<unsigned>(d.pact_slot_level))
    {
        return static_cast<unsigned>(d.pact_slots);
    }
    return whole ? capacity : static_cast<unsigned>(descriptor.short_rest);
}

rules::ResourcePool resource_pool(const ResourceDescriptor &descriptor, const Actor &actor,
                                  const Definition &d)
{
    const auto capacity = static_cast<unsigned>(capacity_of(descriptor, d));
    return {std::string(descriptor.id),
            {std::string(descriptor.label), {}},
            static_cast<unsigned>(remaining_of(descriptor, actor)),
            capacity,
            short_rest_recovery(descriptor, d, capacity)};
}

void add_initiate_pools(std::vector<rules::ResourcePool> &pools, const Actor &actor,
                        const Definition &d)
{
    for (std::size_t n = 0; n < d.initiate_spells.size(); ++n)
    {
        pools.push_back(
            {"magic_initiate:" + d.initiate_spells[n],
             {d.lineage_spells.contains(d.initiate_spells[n]) ? "Lineage: {spell}"
                                                              : "Magic Initiate: {spell}",
              {{"spell", std::string(detail::find_spell(d.initiate_spells[n])->label), true}}},
             (actor.initiate_casts >> n) & 1 ? 1u : 0u,
             1u,
             0u});
    }
}

int stealth_total(const Actor &a)
{
    for (const auto &effect : a.effects.active)
    {
        if (effect.kind == detail::EffectKind::hidden)
        {
            return effect.dc;
        }
    }
    return 0;
}

int trait_uses_spent(const Actor &a)
{
    for (const auto &effect : a.effects.active)
    {
        if (effect.kind == detail::EffectKind::trait_uses)
        {
            return effect.dc;
        }
    }
    return 0;
}

int trait_uses_left(const Actor &a)
{
    return a.definition.trait_uses - trait_uses_spent(a);
}

void add_trait_pool(std::vector<rules::ResourcePool> &pools, const Actor &actor,
                    const Definition &d)
{
    if (d.breath)
    {
        pools.push_back({"breath_weapon",
                         {"Breath Weapon", {}},
                         static_cast<unsigned>(std::max(0, trait_uses_left(actor))),
                         static_cast<unsigned>(d.trait_uses),
                         0u});
    }
    if (d.giant != GiantBoon::none)
    {
        pools.push_back({"giant_ancestry",
                         {std::string(giant_boon_label(d.giant)), {}},
                         static_cast<unsigned>(std::max(0, trait_uses_left(actor))),
                         static_cast<unsigned>(d.trait_uses),
                         0u});
    }
    if (d.resourceful)
    {
        pools.push_back(
            {"heroic_inspiration",
             {"Heroic Inspiration", {}},
             detail::has_effect(actor.effects, detail::EffectKind::heroic_inspiration) ? 1u : 0u,
             1u,
             0u});
    }
}

Footprint footprint(const Actor &a)
{
    return a.form && a.form->footprint ? *a.form->footprint : a.source.footprint;
}

std::vector<Cell> squares_of(Cell anchor, Footprint footprint)
{
    std::vector<Cell> squares;
    for (int dy = 0; dy < footprint.height; ++dy)
    {
        for (int dx = 0; dx < footprint.width; ++dx)
        {
            squares.push_back({anchor.x + dx, anchor.y + dy});
        }
    }
    return squares;
}

std::vector<Cell> squares_of(const Actor &a)
{
    return squares_of(a.source.cell, footprint(a));
}

bool occupies(const Actor &a, Cell cell)
{
    return covers(a.source.cell, footprint(a), cell);
}

bool touches(const std::vector<Cell> &cells, const Actor &a)
{
    return std::any_of(cells.begin(), cells.end(),
                       [&](Cell cell)
                       {
                           return occupies(a, cell);
                       });
}

int distance(const Actor &a, const Actor &b)
{
    return distance(a.source.cell, footprint(a), b.source.cell, footprint(b));
}

int distance(const Actor &a, Cell cell)
{
    return distance(a.source.cell, footprint(a), cell, {});
}

int distance(Cell cell, const Actor &a)
{
    return distance(a, cell);
}

int distance(const Actor &a, Cell anchor, Footprint size)
{
    return distance(a.source.cell, footprint(a), anchor, size);
}

Cell origin_toward(const Actor &a, Cell toward)
{
    Cell nearest = a.source.cell;
    for (const auto square : squares_of(a))
    {
        if (distance(square, toward) < distance(nearest, toward))
        {
            nearest = square;
        }
    }
    return nearest;
}

Footprint summoned_footprint(std::string_view definition)
{
    if (definition.starts_with("giant-insect"))
    {
        return {2, 1};
    }
    if (definition.starts_with("draconic-spirit"))
    {
        return {2, 2};
    }
    return {};
}

Definition strengthened(Definition d, const rules::Participant &who)
{
    d.hp = std::max(1, d.hp * static_cast<int>(who.hit_points_percent) / 100);
    d.melee_bonus += who.attack_bonus;
    d.ranged_bonus += who.attack_bonus;
    return d;
}

Definition wielding(Definition d, std::string_view melee_key, std::string_view ranged_key)
{
    const auto *ranged = detail::weapon(ranged_key);
    // The SRD lists a javelin as a melee weapon with the Thrown property.
    const bool shoots = ranged && (ranged->ranged || ranged->thrown) && ranged->dice;
    if (!ranged_key.empty() && !shoots && !d.innate_ranged)
    {
        d.range = d.long_range = 0;
    }
    if (const auto *melee = detail::weapon(melee_key); melee && !melee->ranged && melee->dice)
    {
        const auto enchantment = rules::parse_item_key(melee_key);
        d.melee.count = melee->dice;
        d.melee.sides = melee->sides;
        d.melee_type = melee->type;
        d.melee_magic = enchantment.bonus;
        d.bane_undead = enchantment.power == rules::ItemPower::bane_undead;
    }
    if (shoots)
    {
        // A stat block without a ranged weapon shoots with its melee bonuses.
        if (!d.range)
        {
            d.ranged_bonus = d.melee_bonus;
            d.ranged.bonus = d.melee.bonus;
        }
        d.ranged.count = ranged->dice;
        d.ranged.sides = ranged->sides;
        d.ranged_type = ranged->type;
        d.ranged_magic = rules::parse_item_key(ranged_key).bonus;
        d.range = ranged->range;
        d.long_range = ranged->long_range;
        d.ranged_missile = weapon_missile(*ranged);
        d.ranged_two_handed = ranged->hands == 2;
    }
    return d;
}

Definition fielded(const Content &content, const rules::Participant &who)
{
    return strengthened(
        wielding(content.definitions.at(who.definition), who.melee_weapon, who.ranged_weapon), who);
}

bool same_command(const Command &a, const Command &b)
{
    return a.revision == b.revision && a.actor == b.actor && a.target == b.target &&
           a.verb == b.verb && a.destination == b.destination && a.item == b.item;
}

Definition without_shield(std::string_view profile, const Definition &d)
{
    std::vector<std::string> keys;
    for (const auto &key : d.equipment_keys)
    {
        if (!detail::is_shield(key))
        {
            keys.push_back(key);
        }
    }
    return character_definition(profile, std::span<const std::string>(keys));
}

Definition monster_without_shield(Definition d)
{
    d.ac -= 2;
    return d;
}

int melee_rating(const Definition &d)
{
    return (d.melee.count * d.melee.sides + 8 * d.melee_magic) *
           (1 + d.extra_attacks + (d.martial_arts ? 1 : 0));
}

bool wears_shield(const Definition &d)
{
    return std::any_of(d.equipment_keys.begin(), d.equipment_keys.end(),
                       [](const auto &key)
                       {
                           return detail::is_shield(key);
                       });
}

const CunningOption &cunning_option(CunningStrike kind)
{
    return cunning_strikes.at(static_cast<std::size_t>(kind));
}

bool reloaded(const Actor &a)
{
    return !a.actions.loading_fired || a.actions.available() || a.actions.haste;
}

bool attack_action(std::string_view verb)
{
    return verb == "melee" || verb == "ranged" || verb == "torch" || verb == "shoot" ||
           verb.starts_with("throw") || verb == "javelin_lightning";
}

bool turns_to_attack(std::string_view verb)
{
    // A caster turns toward a foe, not toward an ally being healed, so every
    // spell except the ally-targeted ones counts alongside weapon attacks.
    if (verb.starts_with("light_") || verb.starts_with("nick_") || verb.starts_with("throw") ||
        verb == "melee" || verb == "ranged" || verb == "torch" || verb == "shoot" ||
        verb == "javelin_lightning")
    {
        return true;
    }
    const auto *spell = detail::find_spell(verb);
    return spell && spell->target != detail::SpellTarget::wounded_ally;
}

std::string dice_detail(const std::vector<RolledDie> &dice, std::optional<int> total)
{
    if (dice.empty())
    {
        return {};
    }
    std::string text = " (";
    int sum = 0;
    for (std::size_t n = 0; n < dice.size();)
    {
        std::size_t end = n;
        while (end < dice.size() && dice[end].sides == dice[n].sides)
        {
            ++end;
        }
        text +=
            (n ? ", " : "") + std::to_string(end - n) + "d" + std::to_string(dice[n].sides) + ":";
        for (std::size_t k = n; k < end; ++k)
        {
            text += (k > n ? " + " : " ") + std::to_string(dice[k].result);
            sum += dice[k].result;
        }
        n = end;
    }
    if (total)
    {
        const int added = *total - sum;
        if (added)
        {
            text += std::string(added > 0 ? ", + " : ", - ") + std::to_string(std::abs(added));
        }
        text += " = " + std::to_string(*total);
    }
    return text + ")";
}
} // namespace opengold::srd5::detail
