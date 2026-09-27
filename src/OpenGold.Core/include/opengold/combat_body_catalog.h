#ifndef OPENGOLD_COMBAT_BODY_CATALOG_H
#define OPENGOLD_COMBAT_BODY_CATALOG_H

#include "opengold/character_art.h"
#include <array>
#include <filesystem>
#include <span>
#include <set>
#include <string>
#include <vector>

namespace opengold
{
struct PartyMember;
}

namespace opengold::por
{
struct CombatLookOption
{
    std::string id, label;
    int original_type{};
};

struct CombatEquipment
{
    int original_type{-1};
    std::string name, definition_id;
};

struct CombatBodySelection
{
    unsigned body{};
    bool matched{};
    std::string combination, label;
};

// Body IDs and weapon labels are data, not compiled-in classifications of the art.
struct CombatBodyCatalog
{
    std::array<std::set<std::string>, 35> bodies{};
    std::vector<CombatLookOption> options;
    std::set<std::string> deleted;
    [[nodiscard]] static CombatBodyCatalog load(const std::filesystem::path &assignments,
            const std::filesystem::path &options_file);
    [[nodiscard]] CombatBodySelection choose(std::span<const CombatEquipment> equipped,
            unsigned fallback) const;
};

struct ResolvedCombatAppearance
{
    CharacterAppearance appearance;
    CombatBodySelection selection;
    [[nodiscard]] Image icon(const CharacterArt &art, bool action) const;
};

// Saved anatomy remains unchanged. The selection supplies wielding arms and
// equipment layers only; encounter creatures do not use this API.
[[nodiscard]] ResolvedCombatAppearance resolve_combat_appearance(const PartyMember &member,
        const CombatBodyCatalog &catalog);
} // namespace opengold::por
#endif
