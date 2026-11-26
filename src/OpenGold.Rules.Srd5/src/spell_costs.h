#ifndef OPENGOLD_SRD5_SPELL_COSTS_H
#define OPENGOLD_SRD5_SPELL_COSTS_H
#include "actor_model.h"
#include "definition.h"
#include "magic_items.h"
#include "spell_table.h"
#include "opengold/rules.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// What a spell costs and what pays for it: the slot level and upcasting, a
// charged item, a scroll, an innate pool or Magic Initiate's free cast.
namespace opengold::srd5::detail
{
using rules::EntityId;

// The slot level a spell verb casts at: a "_N" verb names a slot above the
// spell's level; otherwise the spell's own level.
[[nodiscard]] unsigned slot_level_of(std::string_view verb, const detail::SpellDef &spell);

// Slot levels above the spell's own: each adds its upcast once more.
[[nodiscard]] unsigned upcast_steps(std::string_view verb, const detail::SpellDef &spell);

// Whether a slot of `level`, above the spell's own, changes the spell: more
// dice, creatures or area, or a rider that grows with the slot.
[[nodiscard]] bool upcast_matters(const detail::SpellDef &spell, unsigned level);

// The slot levels a spell is offered at: each level that changes it, and the
// lowest with a slot left, its own level while those last.
[[nodiscard]] std::vector<unsigned> cast_levels(const Actor &a, const detail::SpellDef &spell);

// The verb and label for a cast at `level`; above the spell's own level, the
// verb gains "_3" and the label " (level 3 slot)" for a level-3 slot.
[[nodiscard]] std::pair<std::string, std::string>
cast_at(const detail::SpellDef &spell, unsigned level, std::string verb, std::string label);

// Whether a Mystic Arcanum spell of this level was cast since the last Long Rest.
[[nodiscard]] bool arcanum_spent(const Actor &a, unsigned level);

// The charged item `a` can use for `spell` now (MAGIC-3): one with uses left,
// and a wand only while one of its kind is held. Null when there is none.
[[nodiscard]] const detail::MagicItem *usable_item(const Actor &a, const detail::SpellDef &spell);

// The uses left of the first carried vial of Basic Poison with any, or null.
[[nodiscard]] const unsigned *basic_poison_left(const Actor &a);

[[nodiscard]] unsigned *basic_poison_left(Actor &a);

// Spends a use of the first carried item of `kind` with uses left.
void spend_item_use(Actor &a, const detail::MagicItem &kind);

// A spell still on a carried scroll (MAGIC-6): the scroll's inventory ID, the
// spell's place on it and the slot level it is cast at.
struct ScrollEntry
{
    std::uint64_t item{};
    unsigned index{};
    unsigned level{};
};

// The first spell `spell` that `a` can read from a carried scroll this fight,
// at `level` when it is not 0: an identified scroll of its kind of magic, with
// the spell not yet read. Wearing armor without training, it reads none.
[[nodiscard]] std::optional<ScrollEntry>
readable_scroll(const Actor &a, const detail::SpellDef &spell, unsigned level = 0);

// SRD 5.2.1's Spell Scroll: the save DC by the spell's level; its attack
// bonus is the DC less 8.
// Use Magic Device (MAGIC-6, SRD 5.2.1): a Thief who reads scrolls only
// through it first makes an Intelligence (Arcana) check, DC 10 + the level,
// for a spell above level 1. A failure loses the spell.
[[nodiscard]] bool needs_arcana_check(const Definition &d, unsigned level);

[[nodiscard]] int scroll_dc(unsigned level);

// Whether a verb uses a carried item, and the use is not the casting of a
// spell: a potion, a missile or dust. A wand casts its spell.
[[nodiscard]] bool item_effect(std::string_view verb);

// The creature a Stirge clings to, if it clings to one.
[[nodiscard]] std::optional<EntityId> attached_host(const Actor &a);

// The uses a pool of the actor's innate spells has left.
[[nodiscard]] int innate_left(const Actor &a, std::size_t pool);

// The pool of the actor's innate spells, cast with a Bonus Action or with the
// Action, that can cast the spell now; none when no such pool has a use left.
[[nodiscard]] std::optional<std::size_t> innate_pool(const Actor &a, const detail::SpellDef &spell,
                                                     bool bonus_action);

// Whether `spent` is a count of uses each innate pool of `d` could have spent.
[[nodiscard]] bool valid_innate_spent(const Definition &d, int spent);

// Whether a verb casts a spell with a Bonus Action that is not the spell's own
// casting time: an item used with Fast Hands, or an innate Bonus Action spell.
[[nodiscard]] bool bonus_action_verb(std::string_view verb);

// Pays for a leveled spell: with Magic Initiate's free cast, or from a slot of
// the level it is cast at, the turn's one slot.
void pay_for_spell(Actor &a, std::string_view verb, const detail::SpellDef &spell);
} // namespace opengold::srd5::detail
#endif
