#ifndef OPENGOLD_PARTY_TEST_BRIDGE_H
#define OPENGOLD_PARTY_TEST_BRIDGE_H
#include "opengold/campaign_party.h"
#include "opengold/character.h"
#include "opengold/rolf_tour.h"
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <memory>
#include <vector>

// The Party tester's link to the game's code: a party of up to eight Character
// Pool members, levelled with the game's level-up dialog or its defaults, and
// fights on an area's battlefield with the creatures the area's scripts load,
// on the game's combat screen. Its parent hosts the dialog and the fight.
class PartyTestBridge : public godot::Node
{
    GDCLASS(PartyTestBridge, godot::Node)

  public:
    // The Character Pool, the areas' maps, their creatures and those left out
    // ("left_out"). "error" on failure.
    [[nodiscard]] godot::Dictionary catalog();
    // A new level-one party of the pool members at `pool_indices` (1 to 8).
    [[nodiscard]] godot::String build_party(const godot::Array &pool_indices);
    // Each member: name, classes, level, hit points, experience, can_advance;
    // and for checks the level's classes by ID ("class_ids"), species ID,
    // ability scores and saving throws in Strength-to-Charisma order, which
    // saves are proficient, Hit Dice and each resource's capacity by ID.
    [[nodiscard]] godot::Array members() const;
    // Each class: its ID, its name as the level-up dialog shows it, and the
    // abilities (0-5, Strength to Charisma) another class needs 13 in, all of
    // them or, when "any", one.
    [[nodiscard]] godot::Array classes() const;
    // Gives every member below `level` the experience for it.
    [[nodiscard]] godot::String set_level(int level);
    // Opens the game's level-up dialog for the first member with a level to
    // take; the member's name, or empty when nobody has one.
    [[nodiscard]] godot::String open_level_up();
    // Every remaining level taken with the game's default choices.
    [[nodiscard]] godot::String advance_with_defaults();
    // The game's character sheet text for member `index`.
    [[nodiscard]] godot::String sheet(int index) const;
    // A fight: {"area": map key, "seed", "difficulty", "groups": [{"creature",
    // "count"}]}. Empty on success, else why not.
    [[nodiscard]] godot::String start_fight(const godot::Dictionary &configuration);
    // "Victory", "Defeat" or "Finished" once the fight is over; empty before.
    [[nodiscard]] godot::String outcome() const;
    // Closes the fight. The party is as it was before it.
    void leave();

  protected:
    static void _bind_methods();

  private:
    struct AreaMap
    {
        unsigned district{};
        godot::String label;
    };

    // A creature an area's script loads, drawn with the icon a script names.
    struct CreatureChoice
    {
        unsigned district{}, record{}, icon{};
        godot::String label;
    };

    void load_game_data();
    [[nodiscard]] opengold::CampaignParty &party() const;
    void member_advanced();

    std::shared_ptr<const opengold::por::PhlanResources> town_;
    std::vector<opengold::Character> pool_;
    std::vector<AreaMap> areas_;
    std::vector<CreatureChoice> creatures_;
    // Creatures the scripts load only from a variable, with no icon to draw
    // them by: the game knows their icon only while a script runs.
    std::vector<godot::String> left_out_;
    // The levelled party. Each fight is fought by a copy, so it starts every
    // fight as it is now.
    std::shared_ptr<opengold::CampaignParty> party_;
};

#endif
