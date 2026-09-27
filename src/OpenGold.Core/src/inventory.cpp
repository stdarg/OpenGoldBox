#include "opengold/inventory.h"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace opengold
{
std::optional<std::reference_wrapper<const InventoryItem>> Inventory::find(std::uint64_t id) const
{
    const auto it = std::find_if(items_.begin(), items_.end(),
                                 [&](const auto &item)
                                 {
                                     return item.id == id;
                                 });
    if (it == items_.end())
        return std::nullopt;
    return std::cref(*it);
}

std::uint64_t Inventory::add(std::string definition_id, std::string name, std::uint32_t quantity,
                             int original_type)
{
    if (!quantity || definition_id.find_first_not_of(" \t\r\n") == std::string::npos ||
        name.find_first_not_of(" \t\r\n") == std::string::npos)
        throw std::runtime_error(
            "An inventory item needs a definition, name and positive quantity");
    if (next_id_ == std::numeric_limits<std::uint64_t>::max())
        throw std::runtime_error("Inventory item IDs exhausted");
    items_.push_back(
        {next_id_, std::move(definition_id), std::move(name), quantity, original_type});
    return next_id_++;
}

void Inventory::remove(std::uint64_t id, std::uint32_t quantity)
{
    const auto it = std::find_if(items_.begin(), items_.end(),
                                 [&](const auto &item)
                                 {
                                     return item.id == id;
                                 });
    if (!quantity || it == items_.end() || quantity > it->quantity)
        throw std::runtime_error("Invalid inventory removal");
    if (quantity == it->quantity)
        items_.erase(it);
    else
        it->quantity -= quantity;
}
} // namespace opengold
