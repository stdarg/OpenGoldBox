#ifndef OPENGOLDBOX_CACHED_COMBAT_H
#define OPENGOLDBOX_CACHED_COMBAT_H

#include "opengold/combat_demo.h"
#include <memory>
#include <optional>
#include <utility>

namespace presentation
{
// The combat view's demo and a snapshot of its current combat. A snapshot
// copies the whole battlefield, roster and log, and the view asks for one many
// times a frame, so it is built once per change and handed out by reference;
// building it in a const query is what mutable is for (Effective C++ Items 3
// and 20). Reads see the demo as const, and every change goes through
// change(), which drops the snapshot, so a stale one cannot be served (Item 18).
class CachedCombat
{
  public:
    CachedCombat &operator=(std::unique_ptr<opengold::CombatDemo> demo) noexcept
    {
        demo_ = std::move(demo);
        snapshot_.reset();
        return *this;
    }

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return demo_ != nullptr;
    }

    [[nodiscard]] const opengold::CombatDemo *operator->() const noexcept
    {
        return demo_.get();
    }

    // Call the change at once, as change().submit(command): a reference kept
    // and used after a later snapshot() would leave that snapshot stale.
    [[nodiscard]] opengold::CombatDemo &change() noexcept
    {
        snapshot_.reset();
        return *demo_;
    }

    // Requires a combat (has_combat()). The reference lasts until the next
    // change(); copy the snapshot to compare it with a later one.
    [[nodiscard]] const opengold::rules::Snapshot &snapshot() const
    {
        if (!snapshot_)
            snapshot_ = demo_->combat().snapshot();
        return *snapshot_;
    }

  private:
    std::unique_ptr<opengold::CombatDemo> demo_;
    mutable std::optional<opengold::rules::Snapshot> snapshot_;
};
} // namespace presentation
#endif
