#ifndef OPENGOLD_TEMPLE_CATALOG_H
#define OPENGOLD_TEMPLE_CATALOG_H
#include "opengold/rules.h"
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace opengold
{
// A service on a temple's Heal list and its price (TEMPLE-1).
struct TempleOffering
{
    rules::TempleService service{};
    std::uint32_t price_gp{};
    bool operator==(const TempleOffering &) const = default;
};

// A temple the game knows from data/shops: the map squares where its service
// opens, the services it offers there, and its priest's portrait (relative to
// the game's portraits folder; empty for none).
struct TempleSite
{
    std::string archive; // "GEO3.DAX"
    unsigned record{};
    std::vector<std::pair<unsigned, unsigned>> markers;
    std::vector<TempleOffering> offerings;
    std::string portrait;
};

struct TempleServiceInfo
{
    rules::TempleService service;
    std::string_view id, name;
    std::uint32_t original_price_gp;
};

// Every service, in the original's order, with the original's prices, which
// were the same at every temple; Resurrection is OpenGoldBox's, Heal
// Curse's (offered after Cure Critical Wounds), and Restoration Pools of
// Darkness's and Treasures', whose temples ask nothing for it (their
// temple.pas has no prices).
inline constexpr std::array<TempleServiceInfo, 12> temple_services{{
    {rules::TempleService::cure_blindness, "cure_blindness", "Cure Blindness", 1000},
    {rules::TempleService::cure_disease, "cure_disease", "Cure Disease", 1000},
    {rules::TempleService::cure_light_wounds, "cure_light_wounds", "Cure Light Wounds", 100},
    {rules::TempleService::cure_serious_wounds, "cure_serious_wounds", "Cure Serious Wounds", 350},
    {rules::TempleService::cure_critical_wounds, "cure_critical_wounds", "Cure Critical Wounds",
     600},
    {rules::TempleService::neutralize_poison, "neutralize_poison", "Neutralize Poison", 1000},
    {rules::TempleService::raise_dead, "raise_dead", "Raise Dead", 5500},
    {rules::TempleService::remove_curse, "remove_curse", "Remove Curse", 3500},
    {rules::TempleService::stone_to_flesh, "stone_to_flesh", "Stone to Flesh", 2000},
    {rules::TempleService::resurrection, "resurrection", "Resurrection", 11000},
    {rules::TempleService::heal, "heal", "Heal", 5000},
    {rules::TempleService::restoration, "restoration", "Restoration", 0},
}};

[[nodiscard]] constexpr const TempleServiceInfo &temple_service_info(rules::TempleService service)
{
    return temple_services.at(static_cast<std::size_t>(service));
}

[[nodiscard]] constexpr std::optional<rules::TempleService>
temple_service_named(std::string_view id)
{
    for (const auto &info : temple_services)
    {
        if (info.id == id)
        {
            return info.service;
        }
    }
    return std::nullopt;
}

// A temple the data does not name offers every service of the game at the
// original's price: Curse's adds Heal after Cure Critical Wounds (ovr005,
// temple_heal in its reconstruction), and Pools of Darkness's and
// Treasures' Restoration last (their HealMenu).
[[nodiscard]] inline std::vector<TempleOffering>
original_temple_offerings(bool offers_heal = false, bool offers_restoration = false)
{
    std::vector<TempleOffering> offerings;
    for (const auto &info : temple_services)
    {
        if (info.service == rules::TempleService::heal ||
            (info.service == rules::TempleService::restoration && !offers_restoration))
        {
            continue;
        }
        offerings.push_back({info.service, info.original_price_gp});
        if (offers_heal && info.service == rules::TempleService::cure_critical_wounds)
        {
            const auto &heal = temple_service_info(rules::TempleService::heal);
            offerings.push_back({heal.service, heal.original_price_gp});
        }
    }
    return offerings;
}
} // namespace opengold
#endif
