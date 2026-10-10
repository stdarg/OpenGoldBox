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
class CombatBodyCatalog
{
  public:
    using BodyAssignments = std::array<std::set<std::string>, 35>;

    [[nodiscard]] static CombatBodyCatalog load(const std::filesystem::path &assignments,
            const std::filesystem::path &options_file);
    [[nodiscard]] CombatBodySelection choose(std::span<const CombatEquipment> equipped,
            unsigned fallback) const;

    // The data is private so it keeps what load() checked: every assigned or
    // deleted combination names a look option, and no body keeps a deleted
    // one (Effective C++ Item 22). These change it only within those rules.
    void assign_body(unsigned body, std::set<std::string> combinations);
    void delete_combination(const std::string &combination);
    void clear_assignments();

    [[nodiscard]] const BodyAssignments &bodies() const
    {
        return bodies_;
    }

    [[nodiscard]] const std::vector<CombatLookOption> &options() const
    {
        return options_;
    }

    [[nodiscard]] const std::set<std::string> &deleted() const
    {
        return deleted_;
    }

  private:
    [[nodiscard]] bool names_option(const std::string &combination) const;

    BodyAssignments bodies_{};
    std::vector<CombatLookOption> options_;
    std::set<std::string> deleted_;
};

struct ResolvedCombatAppearance
{
    CharacterAppearance appearance;
    CombatBodySelection selection;
    [[nodiscard]] Image icon(const CharacterArt &art, IconPose pose) const;
};

// Saved anatomy remains unchanged. The selection supplies wielding arms and
// equipment layers only; encounter creatures do not use this API.
[[nodiscard]] ResolvedCombatAppearance resolve_combat_appearance(const PartyMember &member,
        const CombatBodyCatalog &catalog);
} // namespace opengold::por
#endif
