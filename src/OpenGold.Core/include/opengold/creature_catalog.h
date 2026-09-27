#ifndef OPENGOLD_CREATURE_CATALOG_H
#define OPENGOLD_CREATURE_CATALOG_H

#include "opengold/pool_radiance.h"

#include <compare>
#include <filesystem>
#include <functional>
#include <map>
#include <stdexcept>
#include <string_view>

namespace opengold::por
{

struct CreatureId
{
    std::uint8_t bank{}, record{};
    auto operator<=>(const CreatureId &) const = default;
    [[nodiscard]] std::string archive() const;
    [[nodiscard]] std::string key() const;
};

// Reference AD&D adjustments, not automatically added to the stored MON statistics.
// nullopt means outside the supported table/applicability, NOT a zero adjustment.
struct AbilityModifiers
{
    std::optional<int> strength_to_hit, strength_damage;
    std::optional<int> dexterity_missile_to_hit, dexterity_ac_adjustment;
    std::optional<int> constitution_hp_per_hit_die;
    // Cleric, druid, fighter, paladin, ranger, mage, thief, monk. Multiclass HP
    // must be computed per class and averaged; the scalar above is unset for multiclass.
    std::array<std::optional<int>, 8> constitution_hp_per_hit_die_by_class;
    // GBE interprets CHA[170] as strength-bonus permission; GBC leaves it unknown.
    std::optional<bool> strength_bonus_allowed_hint;
};

[[nodiscard]] AbilityModifiers ability_modifiers(const CharacterRecord &character);

enum class EffectKind
{
    none,
    unknown,
    spell,
    condition,
    racial_bonus,
    special_attack,
    regeneration,
    resistance,
    immunity,
    vulnerability,
    other
};

struct Regeneration
{
    int hp_per_round{};
    std::optional<DamageDice> revival_delay_rounds;
};

struct EffectDefinition
{
    std::uint8_t code{};
    std::string_view name{"Unknown effect"};
    EffectKind kind{EffectKind::unknown};
    std::string_view subject;
    std::optional<int> resistance_percent;
    std::optional<double> incoming_damage_multiplier;
    std::optional<int> target_save_adjustment, levels_drained;
    std::optional<Regeneration> regeneration;
};

// Pool of Radiance's SPC namespace, not a later Gold Box game's effect table.
[[nodiscard]] EffectDefinition describe_effect(std::uint8_t code);

struct SpecialEffect
{
    EffectRecord stored;
    EffectDefinition definition;
};

struct EquipmentBonuses
{
    // Per-item contributions. Positive to-hit/damage/save bonuses help the wearer;
    // negative AC adjustments help. No stacking or equipped-state inference.
    std::optional<int> weapon_to_hit, weapon_damage;
    std::optional<int> armor_base_ac, ac_adjustment;
    int save_bonus{};
};

struct Equipment
{
    std::size_t index{}; // Position in this creature's MONnITM record.
    ItemRecord stored;
    ItemTemplate base;
    EquipmentBonuses bonuses;
    [[nodiscard]] std::string label() const;
};

struct Creature
{
    CreatureId id;
    CharacterRecord stored;
    AbilityModifiers abilities;
    std::vector<Equipment> equipment;   // Full inventory; consult stored.readied().
    std::vector<SpecialEffect> effects; // MONnSPC records; item activation codes stay with items.
    std::vector<std::string> interpretation_notes;
};

class CatalogError : public std::runtime_error
{
  public:
    using std::runtime_error::runtime_error;
};

// Immutable after loading. Owns its data; no file handles or borrowed asset buffers survive load().
class CreatureCatalog
{
  public:
    // Reads all eight MONnCHA / MONnITM banks, SPC banks where supplied, and ITEMS.
    // Throws CatalogError on incomplete/corrupt required input; never returns a partial catalog.
    // Stock PoR has no MON1SPC or MON3SPC; the other six SPC banks are required.
    [[nodiscard]] static CreatureCatalog load(const std::filesystem::path &game_directory);

    [[nodiscard]] const std::map<CreatureId, Creature> &all() const noexcept
    {
        return creatures_;
    }

    // Returned references are non-owning and valid for this catalog's lifetime.
    [[nodiscard]] std::optional<std::reference_wrapper<const Creature>>
    find(CreatureId id) const noexcept;
    // Case-insensitive exact-name lookup returns EVERY matching bank/variant.
    [[nodiscard]] std::vector<std::reference_wrapper<const Creature>>
    find_by_name(std::string_view name) const;

  private:
    CreatureCatalog() = default;
    std::map<CreatureId, Creature> creatures_;
};

} // namespace opengold::por
#endif
