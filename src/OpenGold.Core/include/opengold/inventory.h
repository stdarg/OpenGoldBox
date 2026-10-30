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
template <bool Reading> struct SaveCodec;
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

// An item to add. The stable content key and the display name are both
// strings, so they are named fields rather than adjacent parameters that a
// call could swap (Effective C++ Item 18).
struct NewItem
{
    std::string definition_id, name;
    std::uint32_t quantity{1};
    int original_type{-1};
};

class Inventory
{
  public:
    [[nodiscard]] std::span<const InventoryItem> items() const noexcept
    {
        return items_;
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return items_.empty();
    }

    [[nodiscard]] std::optional<std::reference_wrapper<const InventoryItem>>
    find(std::uint64_t id) const;
    // Each addition creates a separate stack with an inventory-local ID.
    // References from items()/find() must be reacquired after mutation.
    std::uint64_t add(NewItem item);
    void remove(std::uint64_t id, std::uint32_t quantity = 1);
    // An item that becomes known under a new name, such as an identified sword.
    void rename(std::uint64_t id, std::string name);
    // An item whose definition changes, such as a scroll a spell was read from.
    void redefine(std::uint64_t id, std::string definition_id);
    // A new count for an item, such as a recharged wand's charges.
    void set_quantity(std::uint64_t id, std::uint32_t quantity);

  private:
    template <bool Reading> friend struct opengold::SaveCodec;
    std::vector<InventoryItem> items_;
    std::uint64_t next_id_{1};
};
} // namespace opengold
#endif
