#ifndef OPENGOLD_ITEM_CONVERSION_H
#define OPENGOLD_ITEM_CONVERSION_H
#include "opengold/equipment.h"
#include "opengold/rules.h"
#include <optional>
#include <string>

// An original item record as the rules see it: its key in the rules' catalog
// or its original record, how many it counts for in a pack, and what is left
// of it after use. These read the record alone; the party (campaign_party.h)
// applies them to its members.
namespace opengold
{
// The record's key in the SRD catalog, or "por:unsupported:<type>" for an
// item the catalog has no conversion for.
[[nodiscard]] std::string equipment_conversion(const por::Equipment &item);
// The item's definition under these rules: its conversion to the SRD's
// catalog, or its original record for rules that keep the original's items.
[[nodiscard]] std::string item_definition(const rules::RulesModule &rules,
                                          const por::Equipment &item);
// The key the rules see for an original item converted to `definition`: it
// says so when the item's magic is not yet identified. A charged item's uses
// are known either way.
[[nodiscard]] std::string rules_key(std::string definition, const por::Equipment &source);
// How many an original item counts for in an inventory: a stack's items, or a
// charged item's charges or doses (MAGIC-3), otherwise one.
[[nodiscard]] unsigned carried_quantity(const por::Equipment &item);
// The same under these rules: under rules that play the original's records,
// every charged item of the original's counts its charges.
[[nodiscard]] unsigned carried_quantity(const rules::RulesModule &rules,
                                        const por::Equipment &item);
// The original item as a fight left it: `left` of its stacked items, charges
// or doses, or for a scroll the spells whose bits are set in `left` (MAGIC-6).
// Nothing when none is left.
[[nodiscard]] std::optional<por::Equipment> item_after_uses(por::Equipment item, unsigned left);
} // namespace opengold
#endif
