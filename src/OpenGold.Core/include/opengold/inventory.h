#ifndef OPENGOLD_INVENTORY_H
#define OPENGOLD_INVENTORY_H
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace opengold
{
struct SaveCodec;
}
namespace opengold
{
struct InventoryItem
{
    std::uint64_t id{};
    // Stable content key, resolved by the game's item catalog/rules adapter.
    std::string definition_id, name;
    std::uint32_t quantity{};
    int original_type{-1};
    bool operator==(const InventoryItem &) const = default;
};
class Inventory
{
  public:
    [[nodiscard]] std::span<const InventoryItem> items() const
    {
        return items_;
    }
    [[nodiscard]] bool empty() const
    {
        return items_.empty();
    }
    [[nodiscard]] std::optional<std::reference_wrapper<const InventoryItem>>
    find(std::uint64_t id) const;
    // Each addition creates a separate stack with an inventory-local ID.
    // References from items()/find() must be reacquired after mutation.
    std::uint64_t add(std::string definition_id, std::string name, std::uint32_t quantity = 1,
                      int original_type = -1);
    void remove(std::uint64_t id, std::uint32_t quantity = 1);

  private:
    friend struct opengold::SaveCodec;
    std::vector<InventoryItem> items_;
    std::uint64_t next_id_{1};
};
} // namespace opengold
#endif
