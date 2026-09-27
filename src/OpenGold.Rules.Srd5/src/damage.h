#ifndef OPENGOLD_SRD5_DAMAGE_H
#define OPENGOLD_SRD5_DAMAGE_H
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace opengold::srd5::detail
{
enum class DamageType : unsigned
{
    acid,
    bludgeoning,
    cold,
    fire,
    force,
    lightning,
    necrotic,
    piercing,
    poison,
    psychic,
    radiant,
    slashing,
    thunder,
    count
};
enum class AffinityKind
{
    resistance,
    vulnerability,
    immunity
};

struct DamageAffinity
{
    AffinityKind kind{};
    std::optional<DamageType> type; // No type means all damage types.
    std::string source_id;
};

struct DamagePart
{
    DamageType type{};
    int amount{};
};

struct ResolvedDamage
{
    DamageType type{};
    int before{}, after{};
};

struct DamageResult
{
    int total{};
    std::vector<ResolvedDamage> parts;
};

[[nodiscard]] DamageType damage_type(std::string_view name);
[[nodiscard]] std::string_view damage_name(DamageType type);
// One instance, possibly with multiple damage types. Bonuses, penalties and
// save multipliers are already applied to these amounts. Aggregate each type,
// then apply resistance (round down), vulnerability and immunity. Separate
// instances (e.g. Magic Missile darts) must be resolved separately.
[[nodiscard]] DamageResult resolve_damage(std::span<const DamagePart> parts,
        std::span<const DamageAffinity> affinities);
} // namespace opengold::srd5::detail
#endif
