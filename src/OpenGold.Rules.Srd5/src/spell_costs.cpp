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

unsigned slot_level_of(std::string_view verb, const detail::SpellDef &spell)
{
    return std::max(spell.level, detail::cast_slot_level(verb));
}

unsigned upcast_steps(std::string_view verb, const detail::SpellDef &spell)
{
    return spell.level ? slot_level_of(verb, spell) - spell.level : 0;
}

bool upcast_matters(const detail::SpellDef &spell, unsigned level)
{
    return spell.upcast.extra_dice || spell.upcast.extra_instances || spell.upcast.extra_radius ||
           spell.rider == detail::Rider::aid || spell.rider == detail::Rider::false_life ||
           spell.rider == detail::Rider::ice_knife || spell.rider == detail::Rider::dispel_magic ||
           spell.rider == detail::Rider::magic_circle ||
           (spell.rider == detail::Rider::magic_weapon && level >= 4) ||
           // Healing, later damage, summoned stat blocks and the Globe's warded
           // levels grow with the slot; Bestow Curse drops Concentration from a
           // level-5 slot.
           spell.id == "heal" || spell.rider == detail::Rider::flame_blade ||
           spell.rider == detail::Rider::dragons_breath ||
           spell.rider == detail::Rider::giant_insect ||
           spell.rider == detail::Rider::summon_dragon ||
           spell.rider == detail::Rider::animate_objects ||
           spell.rider == detail::Rider::globe_of_invulnerability ||
           (spell.rider == detail::Rider::bestow_curse && level >= 5);
}

std::vector<unsigned> cast_levels(const Actor &a, const detail::SpellDef &spell)
{
    std::vector<unsigned> result;
    const unsigned lowest = lowest_slot(a, spell.level);
    for (unsigned level = spell.level; level <= max_slot_level; ++level)
    {
        if (a.slots[level] > 0 &&
            (level == lowest || (level > spell.level && upcast_matters(spell, level))))
        {
            result.push_back(level);
        }
    }
    return result;
}

std::pair<std::string, std::string> cast_at(const detail::SpellDef &spell, unsigned level,
                                            std::string verb, std::string label)
{
    if (level == spell.level)
    {
        return {std::move(verb), std::move(label)};
    }
    const auto slot = std::to_string(level);
    return {std::move(verb) + "_" + slot, std::move(label) + " (level " + slot + " slot)"};
}

bool arcanum_spent(const Actor &a, unsigned level)
{
    return std::any_of(a.effects.active.begin(), a.effects.active.end(),
                       [&](const auto &e)
                       {
                           return e.kind == detail::EffectKind::arcanum_used &&
                                  (e.dc & (1 << (level - 6)));
                       });
}

const detail::MagicItem *usable_item(const Actor &a, const detail::SpellDef &spell)
{
    // No item casts most spells: they are answered without reading every
    // carried item's key, which a well-stocked party makes slow.
    if (std::none_of(detail::magic_items.begin(), detail::magic_items.end(),
                     [&](const auto &item)
                     {
                         return item.spell == spell.id;
                     }))
    {
        return nullptr;
    }
    for (const auto &carried : a.source.inventory)
    {
        const auto *item = detail::magic_item(carried.definition);
        if (!item || item->spell != spell.id)
        {
            continue;
        }
        // A monster, which has no character profile, has its wand in hand
        // (DIFF-1).
        const bool held =
            a.source.character_profile.empty() ||
            std::any_of(a.definition.equipment_keys.begin(), a.definition.equipment_keys.end(),
                        [&](const auto &key)
                        {
                            return detail::magic_item(key) == item;
                        });
        const auto uses = std::find_if(a.item_uses.begin(), a.item_uses.end(),
                                       [&](const auto &entry)
                                       {
                                           return entry.first == carried.inventory_id;
                                       });
        if ((held || item->use != detail::ItemUse::wand) && uses != a.item_uses.end() &&
            uses->second)
        {
            return item;
        }
    }
    return nullptr;
}

const unsigned *basic_poison_left(const Actor &a)
{
    for (const auto &carried : a.source.inventory)
    {
        if (carried.definition != basic_poison)
        {
            continue;
        }
        for (const auto &[id, left] : a.item_uses)
        {
            if (id == carried.inventory_id && left)
            {
                return &left;
            }
        }
    }
    return nullptr;
}

unsigned *basic_poison_left(Actor &a)
{
    return const_cast<unsigned *>(basic_poison_left(std::as_const(a)));
}

void spend_item_use(Actor &a, const detail::MagicItem &kind)
{
    for (const auto &carried : a.source.inventory)
    {
        if (detail::magic_item(carried.definition) != &kind)
        {
            continue;
        }
        for (auto &[id, left] : a.item_uses)
        {
            if (id == carried.inventory_id && left)
            {
                --left;
                return;
            }
        }
    }
    throw std::runtime_error("No use of the item is left");
}

std::optional<ScrollEntry> readable_scroll(const Actor &a, const detail::SpellDef &spell,
                                           unsigned level)
{
    const auto &d = a.definition;
    // One who reads no scroll is answered without reading every carried key.
    if (d.str_dex_disadvantage ||
        (!d.reads_arcane_scrolls && !d.reads_divine_scrolls && !d.use_magic_device))
    {
        return std::nullopt;
    }
    for (const auto &carried : a.source.inventory)
    {
        const auto scroll = rules::parse_scroll_key(carried.definition);
        if (!scroll || scroll->unidentified ||
            (!(scroll->arcane ? d.reads_arcane_scrolls : d.reads_divine_scrolls) &&
             !d.use_magic_device))
        {
            continue;
        }
        const auto left = std::find_if(a.item_uses.begin(), a.item_uses.end(),
                                       [&](const auto &entry)
                                       {
                                           return entry.first == carried.inventory_id;
                                       });
        for (unsigned n = 0; n < scroll->spells.size(); ++n)
        {
            const auto &entry = scroll->spells[n];
            const unsigned cast = std::max(entry.level, spell.level);
            if (entry.id == spell.id && (!level || level == cast) && left != a.item_uses.end() &&
                (left->second & (1U << n)))
            {
                return ScrollEntry{carried.inventory_id, n, cast};
            }
        }
    }
    return std::nullopt;
}

bool needs_arcana_check(const Definition &d, unsigned level)
{
    return d.use_magic_device && !d.reads_arcane_scrolls && !d.reads_divine_scrolls && level > 1;
}

int scroll_dc(unsigned level)
{
    return level <= 2 ? 13 : level <= 4 ? 15 : level <= 6 ? 17 : level <= 8 ? 18 : 19;
}

bool item_effect(std::string_view verb)
{
    if (!detail::item_verb(verb))
    {
        return false;
    }
    const auto id = detail::without_cast_suffix(verb);
    return std::none_of(detail::magic_items.begin(), detail::magic_items.end(),
                        [&](const auto &item)
                        {
                            return item.spell == id && item.use == detail::ItemUse::wand;
                        });
}

std::optional<EntityId> attached_host(const Actor &a)
{
    for (const auto &e : a.effects.active)
    {
        if (e.kind == detail::EffectKind::attached)
        {
            return e.source_actor;
        }
    }
    return std::nullopt;
}

int innate_left(const Actor &a, std::size_t pool)
{
    return a.definition.innate[pool].uses - ((a.innate_spent >> (4 * pool)) & 15);
}

std::optional<std::size_t> innate_pool(const Actor &a, const detail::SpellDef &spell,
                                       bool bonus_action)
{
    const auto &pools = a.definition.innate;
    for (std::size_t n = 0; n < pools.size(); ++n)
    {
        if (pools[n].bonus_action == bonus_action && innate_left(a, n) > 0 &&
            std::find(pools[n].spells.begin(), pools[n].spells.end(), spell.id) !=
                pools[n].spells.end())
        {
            return n;
        }
    }
    return std::nullopt;
}

bool valid_innate_spent(const Definition &d, int spent)
{
    if (spent < 0 || spent >= 1 << (4 * d.innate.size()))
    {
        return false;
    }
    for (std::size_t pool = 0; pool < d.innate.size(); ++pool)
    {
        if (((spent >> (4 * pool)) & 15) > d.innate[pool].uses)
        {
            return false;
        }
    }
    return true;
}

bool bonus_action_verb(std::string_view verb)
{
    return verb.ends_with(detail::item_bonus_suffix) || verb.ends_with(detail::innate_bonus_suffix);
}

void pay_for_spell(Actor &a, std::string_view verb, const detail::SpellDef &spell)
{
    // A Legendary Action's spell spends the Legendary Action alone.
    if (detail::legendary_verb(verb))
    {
        return;
    }
    // An innate spell spends a use of its pool.
    if (verb.ends_with(detail::innate_suffix) || verb.ends_with(detail::innate_bonus_suffix))
    {
        const auto pool = innate_pool(a, spell, verb.ends_with(detail::innate_bonus_suffix));
        if (!pool)
        {
            throw std::runtime_error("No innate use of the spell is left");
        }
        a.innate_spent += 1 << (4 * *pool);
        return;
    }
    // A scroll's spell is erased from it (MAGIC-6).
    if (detail::scroll_verb(verb))
    {
        const auto entry = readable_scroll(a, spell, slot_level_of(verb, spell));
        if (!entry)
        {
            throw std::runtime_error("That spell is not on a scroll");
        }
        for (auto &[id, left] : a.item_uses)
        {
            if (id == entry->item)
            {
                left &= ~(1U << entry->index);
            }
        }
        return;
    }
    // A carried item's spell spends a charge or use of the item (MAGIC-3).
    if (detail::item_verb(verb))
    {
        const auto *item = usable_item(a, spell);
        if (!item)
        {
            throw std::runtime_error("No use of the item is left");
        }
        if (!std::exchange(a.keeps_charge, false))
        {
            spend_item_use(a, *item);
        }
        return;
    }
    if (verb.ends_with(detail::initiate_suffix))
    {
        a.initiate_casts &= ~initiate_bit(a.definition, spell.id);
        return;
    }
    // Divine Intervention: its once-per-Long-Rest use instead of a slot.
    if (verb.ends_with(detail::intervention_suffix))
    {
        --a.arcane[CharacterClass::cleric];
        return;
    }
    // Natural Recovery: its free Circle spell, once per Long Rest.
    if (verb.ends_with(detail::natural_suffix))
    {
        // Effects that last past the fight carry scope 1, as camp effects do.
        detail::apply_spell_benefit(a.effects, detail::EffectSource{.scope = 1, .actor = 1},
                                    a.source.name, detail::EffectKind::natural_cast, 0);
        return;
    }
    // Mystic Arcanum: its spell's once-per-Long-Rest use.
    if (verb.ends_with(detail::arcanum_suffix))
    {
        const int bit = 1 << (spell.level - 6);
        const auto used = std::find_if(a.effects.active.begin(), a.effects.active.end(),
                                       [](const auto &e)
                                       {
                                           return e.kind == detail::EffectKind::arcanum_used;
                                       });
        if (used != a.effects.active.end())
        {
            used->dc |= bit;
        }
        else
        {
            // Effects that last past the fight carry scope 1, as camp effects do.
            detail::apply_spell_benefit(a.effects, detail::EffectSource{.scope = 1, .actor = 1},
                                        a.source.name, detail::EffectKind::arcanum_used, bit);
        }
        return;
    }
    --a.slots[slot_level_of(verb, spell)];
    a.spent_slot = true;
}
} // namespace opengold::srd5::detail
