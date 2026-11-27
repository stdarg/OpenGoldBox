#ifndef OPENGOLD_VAULT_SERVICE_H
#define OPENGOLD_VAULT_SERVICE_H

#include "opengold/campaign_party.h"
#include "opengold/equipment.h"
#include "opengold/message.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace opengold::por
{
class TownServiceHost;

// Dark Queen of Krynn's vault (dark_queen_vault.cpp). Its contents belong to
// this campaign save, shared by its vault entrances, never to the user's
// original VAULT*.DAT files.
class VaultService
{
  public:
    // The vault opens; the script's `request` is answered when the party
    // leaves.
    void open_dark_queen_vault(TownServiceHost &host, std::uint64_t request);
    // The party chose the menu's `choice`.
    bool choose_dark_queen_vault(TownServiceHost &host, std::size_t choice);

    [[nodiscard]] bool visiting() const noexcept
    {
        return dark_queen_vault_visit_.has_value();
    }

    // Forgets the visit, as a new run of the tour or a loaded save does.
    void close() noexcept
    {
        dark_queen_vault_visit_.reset();
    }

    // Reads `vault`'s contents from a campaign save, or writes a const one
    // (campaign_save.cpp); `recorded` is false in a format before the
    // vault's, which keeps none.
    template <class Codec, class Vault> static void save(Codec &codec, Vault &vault, bool recorded);

  private:
    enum class VaultMenu
    {
        home,
        deposit,
        withdraw,
        recipient
    };
    enum class VaultAction
    {
        money,
        deposit,
        withdraw,
        leave,
        deposit_item,
        withdraw_money,
        withdraw_item,
        recipient,
        back
    };

    struct VaultChoice
    {
        VaultAction action{};
        opengold::MemberId member{};
        std::uint64_t item{};
    };

    struct VaultVisit
    {
        std::uint64_t request{};
        std::size_t item{};
        std::vector<VaultChoice> choices;
    };

    // The vault's steel, gems and jewelry, and its items, at most 50.
    std::array<unsigned, 3> dark_queen_vault_money_{};
    std::vector<Equipment> dark_queen_vault_items_;
    std::optional<VaultVisit> dark_queen_vault_visit_;
    void show_dark_queen_vault(TownServiceHost &host, VaultMenu menu,
                               std::vector<opengold::rules::Message> notice = {});
};
} // namespace opengold::por
#endif
