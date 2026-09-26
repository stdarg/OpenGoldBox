#ifndef OPENGOLD_CREATURE_FACTORY_H
#define OPENGOLD_CREATURE_FACTORY_H

#include "opengold/creature_catalog.h"
#include <memory>

namespace opengold::por
{

// Supplied by combat rules after resolving attack rates, movement and conditions.
// Counts refer to the two stored base attack slots, not equipped weapon attacks.
struct CreatureTurnBudget
{
    std::array<unsigned, 2> attacks{};
    unsigned movement{};
};

enum class CreatureActionKind
{
    base_attack,
    move,
    end_turn
};
struct CreatureAction
{
    CreatureActionKind kind{};
    std::size_t attack_slot{}; // Used only for base_attack.
    unsigned remaining{};
};

class CreatureFactory;

// Value-owned mutable state; copies are independent snapshots. Immutable source
// data is shared with the factory and remains alive after the factory is destroyed.
class CreatureInstance
{
  public:
    [[nodiscard]] const Creature &definition() const;
    [[nodiscard]] int hit_points() const noexcept
    {
        return hp_;
    }
    [[nodiscard]] int max_hit_points() const noexcept
    {
        return max_hp_;
    }
    [[nodiscard]] bool can_act() const noexcept
    {
        return hp_ > 0 && !incapacitated_ && turn_active_;
    }
    // Nonnegative amounts only; returns actual HP lost/restored. Zero HP is
    // inactive, not a declaration of death. Healing/revival eligibility is external.
    int take_damage(int amount);
    int heal(int amount);
    void set_incapacitated(bool value) noexcept;

    // Starts inactive. Turn numbers must strictly increase, preventing a repeated
    // call from replenishing a spent budget. Returns false for stale/active turns.
    bool begin_turn(std::uint64_t turn_number, CreatureTurnBudget budget);
    void end_turn() noexcept;
    [[nodiscard]] std::vector<CreatureAction> available_actions() const;
    // Consume allowances only after the caller validates target/range/path etc.
    // These operations do not roll damage, move a token, or execute effects.
    bool try_attack(std::size_t slot) noexcept;
    bool try_move(unsigned distance) noexcept;

  private:
    friend class CreatureFactory;
    CreatureInstance(std::shared_ptr<const CreatureCatalog> catalog, CreatureId id, int hp);
    std::shared_ptr<const CreatureCatalog> catalog_;
    CreatureId id_;
    int hp_{}, max_hp_{};
    bool incapacitated_{}, turn_active_{};
    std::optional<std::uint64_t> last_turn_;
    CreatureTurnBudget remaining_;
};

class CreatureFactory
{
  public:
    [[nodiscard]] static CreatureFactory load(const std::filesystem::path &game_directory);
    explicit CreatureFactory(CreatureCatalog catalog);
    [[nodiscard]] const CreatureCatalog &catalog() const noexcept
    {
        return *catalog_;
    }
    // Defaults to stored max HP, never the potentially uninitialized current HP.
    // Explicit positive HP supports externally rolled/scaled encounter HP.
    // Missing IDs or a zero-HP template without an override throw CatalogError.
    [[nodiscard]] CreatureInstance create(CreatureId id,
                                          std::optional<int> max_hp = std::nullopt) const;

  private:
    std::shared_ptr<const CreatureCatalog> catalog_;
};

} // namespace opengold::por
#endif
