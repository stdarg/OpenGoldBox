#ifndef OPENGOLD_TREASURE_SERVICE_H
#define OPENGOLD_TREASURE_SERVICE_H

#include "opengold/ecl_machine.h"
#include "opengold/equipment.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace opengold::por
{
class TownServiceHost;

// Uncollected rewards. Encounter loot comes from defeated creature records;
// script treasure (TREASURE) has no records and carries only its money.
struct PendingLoot
{
    std::array<unsigned, 7> wealth{};
    std::vector<Equipment> items;
    std::string reward_id;
    std::vector<unsigned> records;
    bool include_items{true};
    unsigned area{}; // the district whose original creature records these are
};

// What a monster used of its gear in a fight (DIFF-1): its creature record and,
// by each item's place in the record's gear, the uses it has left.
struct SpentGear
{
    unsigned record{};
    std::vector<std::pair<std::size_t, unsigned>> uses_left;
};

// The treasure a fight or a script gives the party (host_treasure.cpp): the
// loot waiting for room in its purses, the script's TREASURE waiting for the
// COMBAT or shop that hands it out, and the pile of items left for the party
// to take.
class TreasureService
{
  public:
    [[nodiscard]] const std::vector<PendingLoot> &pending_loot() const noexcept
    {
        return pending_loot_;
    }

    void add_loot(PendingLoot loot)
    {
        pending_loot_.push_back(std::move(loot));
    }

    // Whether loot of this reward still waits.
    [[nodiscard]] bool awaits(const std::string &reward_id) const;
    // The loot an event found, back as a rolled-back event began; nothing is
    // staged.
    void restore_loot(std::vector<PendingLoot> loot);
    // The party takes what it has room for: the items to the members, or with
    // pile_loot to the treasure pile.
    void claim_loot(TownServiceHost &host);
    void pile_loot(TownServiceHost &host);

    // Money from a non-shop TREASURE, awarded by the COMBAT that follows it.
    [[nodiscard]] const std::optional<PendingLoot> &staged() const noexcept
    {
        return staged_treasure_;
    }

    void stage(PendingLoot loot)
    {
        staged_treasure_ = std::move(loot);
    }

    // The staged treasure, which is then no longer staged.
    [[nodiscard]] PendingLoot take_staged();

    void forget_staged() noexcept
    {
        staged_treasure_.reset();
    }

    // The items of the treasure the party stands at (TourPhase::treasure).
    [[nodiscard]] const std::vector<Equipment> &treasure_pile() const noexcept
    {
        return treasure_pile_;
    }

    void add_to_pile(Equipment item)
    {
        treasure_pile_.push_back(std::move(item));
    }

    // Shows the treasure pile; `request` resumes with `writes` and the compare
    // flags `conditions` once it closes.
    void open_treasure(TownServiceHost &host, std::uint64_t request,
                       std::vector<EclMemoryWrite> writes, std::optional<EclConditions> conditions);
    // At a treasure: the selected member takes the pile's `item`. False, with
    // the reason in the diagnostic, when the member can carry no more.
    bool take_treasure(TownServiceHost &host, std::uint64_t ticket, std::size_t item);
    // Leaves what is still in the pile behind for good; the script goes on.
    bool leave_treasure(TownServiceHost &host, std::uint64_t ticket);

    // Reads `treasure`'s pending loot from a campaign save, or writes a const
    // one (campaign_save.cpp). Loot from creature records is read back as
    // `encounter_loot` makes it from them.
    template <class Codec, class Treasure, class EncounterLoot>
    static void save(Codec &codec, Treasure &treasure, const EncounterLoot &encounter_loot);

  private:
    std::vector<PendingLoot> pending_loot_;
    std::optional<PendingLoot> staged_treasure_;
    std::vector<Equipment> treasure_pile_;
    std::uint64_t treasure_request_{};
    std::vector<EclMemoryWrite> treasure_writes_;
    // The compare flags the script hears with them (Pools of Darkness's COMBAT).
    std::optional<EclConditions> treasure_conditions_;
};
} // namespace opengold::por
#endif
