#include "opengold/creature_factory.h"

#include <algorithm>
#include <utility>

namespace opengold::por
{

CreatureFactory CreatureFactory::load(const std::filesystem::path &directory)
{
    return CreatureFactory(CreatureCatalog::load(directory));
}

CreatureFactory::CreatureFactory(CreatureCatalog catalog)
    : catalog_(std::make_shared<const CreatureCatalog>(std::move(catalog)))
{
}

CreatureInstance CreatureFactory::create(CreatureId id, std::optional<int> max_hp) const
{
    const auto found = catalog_->find(id);
    if (!found)
        throw CatalogError("Unknown creature " + id.key());
    const int hp = max_hp.value_or(found->get().stored.max_hit_points);
    if (hp <= 0)
        throw CatalogError("Positive instance HP required for " + id.key());
    return CreatureInstance(catalog_, id, hp);
}

CreatureInstance::CreatureInstance(std::shared_ptr<const CreatureCatalog> catalog, CreatureId id,
                                   int hp)
    : catalog_(std::move(catalog)), id_(id), hp_(hp), max_hp_(hp)
{
}

const Creature &CreatureInstance::definition() const
{
    return catalog_->find(id_)->get();
}

int CreatureInstance::take_damage(int amount)
{
    if (amount < 0)
        throw std::invalid_argument("Damage must be nonnegative");
    const auto lost = std::min(hp_, amount);
    hp_ -= lost;
    if (hp_ == 0)
        end_turn();
    return lost;
}

int CreatureInstance::heal(int amount)
{
    if (amount < 0)
        throw std::invalid_argument("Healing must be nonnegative");
    const auto restored = std::min(max_hp_ - hp_, amount);
    hp_ += restored;
    return restored;
}

void CreatureInstance::set_incapacitated(bool value) noexcept
{
    incapacitated_ = value;
    if (value)
        end_turn();
}

bool CreatureInstance::begin_turn(std::uint64_t number, CreatureTurnBudget budget)
{
    if (turn_active_ || (last_turn_ && number <= *last_turn_))
        return false;
    // Reject allowances for absent/undecoded attacks. The rules layer owns the
    // count, including fractional rates and modifiers, but cannot invent a slot.
    const auto &attacks = definition().stored.base_attacks;
    for (std::size_t i = 0; i < attacks.size(); ++i)
    {
        if (budget.attacks[i] && (!attacks[i].attacks_per_two_rounds || !attacks[i].damage.count ||
                                  !attacks[i].damage.sides))
            throw std::invalid_argument("Turn budget refers to an unavailable base attack");
    }
    last_turn_ = number;
    remaining_ = budget;
    turn_active_ = hp_ > 0 && !incapacitated_;
    if (!turn_active_)
        remaining_ = {};
    return turn_active_;
}

void CreatureInstance::end_turn() noexcept
{
    turn_active_ = false;
    remaining_ = {};
}

std::vector<CreatureAction> CreatureInstance::available_actions() const
{
    std::vector<CreatureAction> actions;
    if (!can_act())
        return actions;
    for (std::size_t i = 0; i < remaining_.attacks.size(); ++i)
        if (remaining_.attacks[i])
            actions.push_back({CreatureActionKind::base_attack, i, remaining_.attacks[i]});
    if (remaining_.movement)
        actions.push_back({CreatureActionKind::move, 0, remaining_.movement});
    actions.push_back({CreatureActionKind::end_turn, 0, 1});
    return actions;
}

bool CreatureInstance::try_attack(std::size_t slot) noexcept
{
    if (!can_act() || slot >= remaining_.attacks.size() || !remaining_.attacks[slot])
        return false;
    --remaining_.attacks[slot];
    return true;
}

bool CreatureInstance::try_move(unsigned distance) noexcept
{
    if (!can_act() || distance == 0 || distance > remaining_.movement)
        return false;
    remaining_.movement -= distance;
    return true;
}

} // namespace opengold::por
