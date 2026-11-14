#ifndef OPENGOLD_TESTS_TOUR_ACCESS_H
#define OPENGOLD_TESTS_TOUR_ACCESS_H

#include "opengold/overland.h"
#include "opengold/rolf_tour.h"
#include <cstdint>
#include <map>
#include <string_view>
#include <vector>

namespace opengold::test
{
// Test-only access to a town session: sets a script variable, such as a
// council commission's flag, that the test route cannot reach through the
// game, or reseeds the script's dice. The party must be idle between events.
struct TourAccess
{
    static void set_variable(por::RolfTourSession &town, por::EclAddress address,
                             std::uint16_t value)
    {
        town.machine_.bind_variable(address, value);
    }

    // Takes the running event as the one that freed Secret's soul (cell 4CD9
    // set to 1 by script 99's SAVE, which a test cannot reach): the cell is
    // set while the party is idle, and this forgets it was set before.
    static void soul_freed_in_event(por::RolfTourSession &town)
    {
        town.soul_at_event_start_ = 0;
    }

    // Sets a script string, such as the training hall's class word.
    static void set_string(por::RolfTourSession &town, por::EclAddress address,
                           std::string_view value)
    {
        town.machine_.bind_string(address, value);
    }

    // A copy of the script machine, to try dice on without touching the session.
    static por::EclMachine machine(const por::RolfTourSession &town)
    {
        return town.machine_;
    }

    // An original item list of the area that loads `map`, as its TREASURE gives it.
    static const std::vector<por::Equipment> &item_list(const por::RolfTourSession &town,
                                                        unsigned map, unsigned list)
    {
        return town.town_->districts.at(map)->treasure.at(list);
    }

    // The wilderness map the session travels, to plan a journey on.
    static const por::OverlandWorld &overland_world(const por::RolfTourSession &town)
    {
        return *town.town_->overland;
    }

    // The wilderness tiles, plain or as the healed valley shows them.
    static const std::vector<Image> &overland_tiles(const por::RolfTourSession &town, bool healed)
    {
        return healed ? town.town_->healed_overland_tiles : town.town_->overland_tiles;
    }

    // Everything the creatures of New Phlan and its districts carry into a fight.
    static std::vector<por::Equipment> creature_equipment(const por::RolfTourSession &town)
    {
        std::vector<por::Equipment> gear;
        const auto add = [&](const por::PhlanResources &area)
        {
            for (const auto &[record, creature] : area.encounter_creatures)
            {
                gear.insert(gear.end(), creature.equipment.begin(), creature.equipment.end());
            }
        };
        add(*town.town_);
        for (const auto &[map, district] : town.town_->districts)
        {
            add(*district);
        }
        return gear;
    }

    // The original's closing story, as read from the player's GAME.OVR.
    static const std::vector<std::string> &closing_story(const por::RolfTourSession &town)
    {
        return town.town_->closing_story;
    }

    // Records a square of `map` as one the party has stood on.
    static void mark_visited(por::RolfTourSession &town, unsigned map, unsigned x, unsigned y)
    {
        (map == town.current_area_ ? town.snapshot_.visited
                                   : town.visited_areas_[map])[y * 16 + x] = true;
    }

    // An area script as the session runs it, with data/encounters' changes.
    static std::shared_ptr<const por::EclProgram> program(const por::RolfTourSession &town,
                                                          unsigned script)
    {
        return town.town_->programs.at(script);
    }

    // Stands the idle party on a square of its map, as a save made there would
    // have it: the area event saves start one step from squares that no walk
    // from the map's arrival reaches (behind secret or magically locked doors).
    static void place(por::RolfTourSession &town, por::PartyPose pose)
    {
        town.bind_pose(pose);
    }

    // The maps of the session's areas, by the script that loads each first.
    static std::map<unsigned, unsigned> area_scripts(const por::RolfTourSession &town)
    {
        std::map<unsigned, unsigned> scripts;
        for (const auto &[map, district] : town.town_->districts)
        {
            scripts.emplace(map, district->script);
        }
        return scripts;
    }

    // The party walks into `map` at `pose` as its script's entry has it.
    static void enter(por::RolfTourSession &town, unsigned map, por::PartyPose pose)
    {
        town.enter_script(town.town_->districts.at(map)->script, {map, pose});
    }

    // The party is taken into `script` on `map` at `pose`, as a NEW ECL that
    // no map of its own names would take it (Gateway's ending, script 28).
    static void enter_script(por::RolfTourSession &town, unsigned script, unsigned map,
                             por::PartyPose pose)
    {
        town.enter_script(script, {map, pose});
    }

    // Reseeds the script's dice, so a test can try an event on several rolls.
    static void seed_dice(por::RolfTourSession &town, std::uint32_t seed)
    {
        town.machine_.seed_random(seed);
    }
};
} // namespace opengold::test
#endif
