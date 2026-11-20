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
        town.won_cell_at_event_start_ = 0;
    }

    // Sets a script string, such as the training hall's class word.
    static void set_string(por::RolfTourSession &town, por::EclAddress address,
                           std::string_view value)
    {
        town.machine_.bind_string(address, value);
    }

    // A script's PLAY SOUND in the 256-color games (eclvm.pas, EclPlaySound),
    // for a sound no script near a test's party plays.
    static void play_sound(por::RolfTourSession &town, unsigned sound)
    {
        town.vga_play_sound(sound);
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

    // A Buck Rogers ground-combat tile set's tiles, by its file.
    static std::size_t buck_rogers_tiles(const por::RolfTourSession &town, std::string_view file)
    {
        return town.town_->buck_rogers_tile_sets.at(std::string(file)).size();
    }

    // A district's map as the campaign loaded it.
    static const por::GeoMap &district_map(const por::RolfTourSession &town, unsigned map)
    {
        return *town.town_->districts.at(map)->map;
    }

    // Every script of the campaign, by its number.
    static const std::map<unsigned, std::shared_ptr<const por::EclProgram>> &
    programs(const por::RolfTourSession &town)
    {
        return town.town_->programs;
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

    // The idle party runs `script` from `address`, an instruction boundary,
    // as an event: a test reaches a command no walk of the maps reaches at
    // the instruction where its setup begins (the ship fights of the solar
    // map's script and of the escape from the Mariposa). The script's entry
    // does not run; the party stays where it stands.
    static void run_from(por::RolfTourSession &town, unsigned script, std::uint32_t address)
    {
        const auto program = town.town_->programs.find(script);
        if (!town.can_leave() || program == town.town_->programs.end())
        {
            throw std::runtime_error("The party cannot run that script now");
        }
        town.machine_.switch_program(program->second);
        town.current_script_ = script;
        town.snapshot_.script_id = script;
        const auto bank = town.town_->script_banks.find(script);
        if (bank != town.town_->script_banks.end())
        {
            town.machine_.bind_variable(town.game_cell(por::ecl_script_bank),
                                        static_cast<std::uint16_t>(bank->second));
        }
        if (!town.machine_.start_at_for_inspection(address))
        {
            throw std::runtime_error("The script has no instruction there");
        }
        town.event_stage_ = por::RolfTourSession::EventStage::search;
        town.snapshot_.phase = por::TourPhase::running;
        ++town.snapshot_.revision;
        town.advance(0);
    }

    // Reseeds the script's dice, so a test can try an event on several rolls.
    static void seed_dice(por::RolfTourSession &town, std::uint32_t seed)
    {
        town.machine_.seed_random(seed);
    }
};
} // namespace opengold::test
#endif
