#include "opengold/rolf_tour.h"
#include "opengold/treasure_service.h"
#include "opengold/authored_items.h"
#include "opengold/creature_art.h"
#include "opengold/random_treasure.h"

#include <algorithm>
#include <utility>

namespace opengold::por
{
bool TreasureService::awaits(const std::string &reward_id) const
{
    return std::any_of(pending_loot_.begin(), pending_loot_.end(),
                       [&](const auto &pending)
                       {
                           return pending.reward_id == reward_id;
                       });
}

void TreasureService::restore_loot(std::vector<PendingLoot> loot)
{
    pending_loot_ = std::move(loot);
    staged_treasure_.reset();
}

PendingLoot TreasureService::take_staged()
{
    auto loot = std::move(*staged_treasure_);
    staged_treasure_.reset();
    return loot;
}

void TreasureService::claim_loot(TownServiceHost &host)
{
    auto *campaign = host.campaign();
    if (!campaign)
    {
        return;
    }
    for (auto it = pending_loot_.begin(); it != pending_loot_.end();)
    {
        const auto before = host.held_items();
        if (campaign->award_loot(it->wealth, it->items, it->reward_id))
        {
            host.equip_gained(before);
            LootTaken taken{.wealth = it->wealth};
            for (const auto &item : it->items)
            {
                taken.items.push_back(item.label());
            }
            host.screen().loot_taken.push_back(std::move(taken));
            it = pending_loot_.erase(it);
            ++host.screen().revision;
        }
        else
        {
            ++it;
        }
    }
}

void TreasureService::pile_loot(TownServiceHost &host)
{
    auto *campaign = host.campaign();
    if (!campaign)
    {
        return;
    }
    for (auto it = pending_loot_.begin(); it != pending_loot_.end();)
    {
        const auto &claimed = campaign->state().claimed_rewards;
        const bool repeat =
            std::find(claimed.begin(), claimed.end(), it->reward_id) != claimed.end();
        if (!campaign->award_loot(it->wealth, {}, it->reward_id))
        {
            ++it;
            continue;
        }
        if (!repeat)
        {
            host.screen().loot_taken.push_back({.wealth = it->wealth});
            for (auto &item : it->items)
            {
                treasure_pile_.push_back(std::move(item));
            }
        }
        it = pending_loot_.erase(it);
        ++host.screen().revision;
    }
}

void TreasureService::open_treasure(TownServiceHost &host, std::uint64_t request,
                                    std::vector<EclMemoryWrite> writes,
                                    std::optional<EclConditions> conditions)
{
    host.log_player("finds a treasure of " + std::to_string(treasure_pile_.size()) + " items");
    treasure_request_ = request;
    treasure_writes_ = std::move(writes);
    host.screen().phase = TourPhase::treasure;
    host.screen().continue_ticket = host.new_ticket();
    host.screen().choices.clear();
    host.screen().diagnostic.clear();
    ++host.screen().revision;
    treasure_conditions_ = conditions;
}

bool TreasureService::take_treasure(TownServiceHost &host, std::uint64_t ticket, std::size_t item)
{
    if (host.screen().phase != TourPhase::treasure || !ticket ||
        ticket != host.screen().continue_ticket || item >= treasure_pile_.size() ||
        !host.campaign() || !host.campaign()->selected())
    {
        return false;
    }
    auto *campaign = host.campaign();
    const auto taker = campaign->selected();
    host.log_player("takes " + treasure_pile_[item].label() + " (item " + std::to_string(item + 1) +
                    " of " + std::to_string(treasure_pile_.size()) + ")");
    host.screen().gear_changes.clear();
    const auto before = host.held_items();
    try
    {
        campaign->take_item(taker, treasure_pile_[item]);
    }
    catch (const std::exception &e)
    {
        host.screen().diagnostic = e.what();
        ++host.screen().revision;
        return false;
    }
    host.equip_gained(before);
    treasure_pile_.erase(treasure_pile_.begin() + static_cast<std::ptrdiff_t>(item));
    host.screen().diagnostic.clear();
    // A new ticket: the list the view shows has changed.
    host.screen().continue_ticket = host.new_ticket();
    ++host.screen().revision;
    return true;
}

bool TreasureService::leave_treasure(TownServiceHost &host, std::uint64_t ticket)
{
    if (host.screen().phase != TourPhase::treasure || !ticket ||
        ticket != host.screen().continue_ticket)
    {
        return false;
    }
    host.log_player(treasure_pile_.empty()
                        ? "leaves the treasure"
                        : "leaves " + std::to_string(treasure_pile_.size()) + " items behind");
    // The script reads its character afresh: the member may have taken items.
    auto reply = host.character_reply(host.selected_character());
    reply.writes.insert(reply.writes.end(), treasure_writes_.begin(), treasure_writes_.end());
    reply.conditions = std::exchange(treasure_conditions_, std::nullopt);
    if (!host.script_machine().resume_host(treasure_request_, reply))
    {
        host.fail("The script did not take the treasure's end");
        return true;
    }
    treasure_pile_.clear();
    treasure_writes_.clear();
    treasure_request_ = 0;
    host.screen().continue_ticket = 0;
    host.screen().diagnostic.clear();
    host.screen().phase = TourPhase::running;
    ++host.screen().revision;
    host.advance(0);
    return true;
}

namespace
{
// A creature's gear by MONnITM place. With an export, an original item the
// Combat Sprite Catalog left out is not carried (#274).
// Gear the catalog added to a creature (#274), each as a plain item of its
// original type from New Phlan's shop lists. A type no list sells is left out.
std::vector<Equipment> added_gear(const ExportedMonster *exported, const PhlanResources &town)
{
    std::vector<Equipment> gear;
    for (const auto &added : exported ? exported->equipment : std::vector<MonsterItem>{})
    {
        if (added.source_index >= 0 || added.source_type < 0)
        {
            continue;
        }
        const auto plain = [&]() -> const Equipment *
        {
            for (const auto &[record, list] : town.treasure)
            {
                for (const auto &item : list)
                {
                    if (item.stored.type == added.source_type && !item.stored.magic_bonus &&
                        !item.stored.cursed_raw)
                    {
                        return &item;
                    }
                }
            }
            return nullptr;
        }();
        if (plain)
        {
            gear.push_back(*plain);
        }
    }
    return gear;
}

// The item key of the item a creature fights with: its original item, or a
// plain item of the type the catalog added. Empty for none.
} // namespace

bool RolfTourSession::take_treasure(std::uint64_t ticket, std::size_t item)
{
    return treasure_service_.take_treasure(*this, ticket, item);
}

bool RolfTourSession::leave_treasure(std::uint64_t ticket)
{
    return treasure_service_.leave_treasure(*this, ticket);
}

PendingLoot RolfTourSession::encounter_loot(unsigned area, std::vector<unsigned> records,
                                            std::string reward, bool items,
                                            std::vector<SpentGear> spent) const
{
    // Dark Queen shares one creature table across every map. The earlier
    // games keep New Phlan's fights on the town and the rest on districts.
    const auto district = town_->districts.find(area);
    const PhlanResources *resources =
        area == 0 || game() == GoldBoxGame::dark_queen_of_krynn
            ? town_.get()
            : (district == town_->districts.end() ? nullptr : district->second.get());
    if (!resources || records.empty() || records.size() > 56 || reward.empty() ||
        reward.size() > 160 ||
        std::any_of(records.begin(), records.end(),
                    [&](auto id)
                    {
                        return !resources->encounter_creatures.contains(id);
                    }))
    {
        throw EclError("Invalid pending encounter loot");
    }
    PendingLoot loot;
    loot.reward_id = std::move(reward);
    loot.records = std::move(records);
    loot.include_items = items;
    loot.area = area;
    for (auto id : loot.records)
    {
        const auto &creature = resources->encounter_creatures.at(id);
        for (unsigned coin = 0; coin < 7; ++coin)
        {
            loot.wealth[coin] += creature.stored.wealth[coin];
        }
        if (!items)
        {
            continue;
        }
        auto gear = carried_gear(creature, EncounterHost::exported_creature(*this, *resources, id));
        // One monster of this record used its gear in the fight (DIFF-1).
        if (const auto used = std::find_if(spent.begin(), spent.end(),
                                           [&](const auto &s)
                                           {
                                               return s.record == id;
                                           });
            used != spent.end())
        {
            for (const auto &[index, left] : used->uses_left)
            {
                if (index >= gear.size() || !gear[index])
                {
                    throw EclError("Invalid spent monster gear");
                }
                gear[index] = item_after_uses(std::move(*gear[index]), left);
            }
            spent.erase(used);
        }
        for (auto &item : gear)
        {
            // There is no ammunition: a monster's arrows are not left behind.
            if (item && !opengold::original_item::is_ammunition(item->stored.type))
            {
                loot.items.push_back(std::move(*item));
            }
        }
        for (auto &item :
             added_gear(EncounterHost::exported_creature(*this, *resources, id), *town_))
        {
            item.index = loot.items.size();
            loot.items.push_back(std::move(item));
        }
    }
    return loot;
}

std::string RolfTourSession::treasure_identity(std::uint16_t address) const
{
    // A TREASURE instruction is identified by its script and original address.
    constexpr std::string_view digits = "0123456789abcdef";
    std::string hex;
    for (int shift = 12; shift >= 0; shift -= 4)
    {
        hex += digits[(address >> shift) & 15];
    }
    const std::string archive = "ECL" + std::to_string(current_area_ ? area_resources().bank : 3);
    return "por:" + archive + ":" + std::to_string(current_script_) + ":treasure:" + hex + ":v1";
}

void RolfTourSession::stage_treasure(const EclRequest &request)
{
    // Operands: copper, silver, electrum, gold, platinum, gems, jewelry, then an
    // item code: an ITEMn list below 128, 128 + n random items, or 255 for none.
    const auto items = request.arguments.at(7).number();
    if (!campaign_)
    {
        throw EclError("Treasure awards require a campaign party");
    }
    PendingLoot loot;
    for (unsigned coin = 0; coin < 7; ++coin)
    {
        loot.wealth[coin] = request.arguments.at(coin).number();
    }
    loot.reward_id = treasure_identity(request.instruction->address);
    // An instruction whose list comes from a script cell gives each list once:
    // the Temple of Bane's three trapdoors share one (ECL1:24 0xAA34).
    if (request.instruction->operands.at(7).tag != 0)
    {
        loot.reward_id += ":list:" + std::to_string(items);
    }
    loot.include_items = false;
    if (items < 128)
    {
        // A fixed list from the area's item archive.
        const auto &lists = area_resources().treasure;
        const auto found = lists.find(items);
        if (found == lists.end())
        {
            throw EclError("Unknown treasure list " + std::to_string(items));
        }
        for (auto item : found->second)
        {
            // There is no ammunition, so a list's arrows and quarrels are not found.
            if (opengold::original_item::is_ammunition(item.stored.type))
            {
                continue;
            }
            item.index = loot.items.size();
            loot.items.push_back(std::move(item));
        }
    }
    else if (items != 255)
    {
        // Rolled on the script's own saved random stream, as the original did here.
        const auto roll = [&](unsigned sides)
        {
            return machine_.host_random(request.id, sides) + 1;
        };
        for (auto &record : random_treasure_items(items - 128, roll))
        {
            Equipment item;
            item.index = loot.items.size();
            const auto &templates = town_->item_templates;
            if (!templates.empty())
            {
                if (record.type >= templates.size())
                {
                    throw EclError("Missing item template for generated treasure");
                }
                item.base = templates[record.type];
            }
            item.bonuses = equipment_bonuses(record, item.base);
            item.stored = std::move(record);
            loot.items.push_back(std::move(item));
        }
    }
    treasure_service_.stage(std::move(loot));
}

void RolfTourSession::award_staged_treasure(bool with_experience)
{
    auto loot = treasure_service_.take_staged();
    const auto &claimed = campaign_->state().claimed_rewards;
    const auto awarded = [&](const std::string &id)
    {
        return std::find(claimed.begin(), claimed.end(), id) != claimed.end() ||
               treasure_service_.awaits(id);
    };
    // The script decides whether a treasure repeats, as the original's flags
    // did: City Hall pays every reward through one instruction. Each award
    // from an instruction already used gets an identity of its own.
    const auto first_award = loot.reward_id;
    for (unsigned award = 2; awarded(loot.reward_id); ++award)
    {
        loot.reward_id = first_award + ":award:" + std::to_string(award);
    }
    // Pool's COMBAT runs its post-combat award even with no monsters. The
    // script's treasure therefore earns experience too, whether it stands
    // alone or accompanies a fight (EclCombat, DetermineLootExp). Keep its
    // claim beside this particular script-authorized treasure award.
    if (with_experience && game() == GoldBoxGame::pool_of_radiance && plays_original_records())
    {
        const auto experience = encounters_.treasure_experience(*this, loot);
        if (experience)
        {
            campaign_->award_experience(encounters_.experience_share(*this, experience),
                                        loot.reward_id + ":experience");
        }
    }
    treasure_service_.add_loot(std::move(loot));
    treasure_service_.pile_loot(*this);
    if (!treasure_service_.pending_loot().empty())
    {
        snapshot_.dialogue += "\nLoot is retained until your party has room in its purses.";
    }
}

} // namespace opengold::por
