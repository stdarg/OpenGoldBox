#ifndef OPENGOLD_TEST_GOLD_BOX_ROUTE_H
#define OPENGOLD_TEST_GOLD_BOX_ROUTE_H

#include "opengold/rolf_tour.h"
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace opengold::test
{
struct RouteBeat
{
    unsigned map, x, y;
    opengold::por::MapDirection facing;
    std::vector<std::string> answers;
    // Rest until every member is whole first, as a player would before a
    // known fight.
    bool rest_first = false;
    // Walk into the map, its script taking over, as its way in from the last
    // map does, instead of looking at a square of the map the party is on.
    bool enter_map = false;
    // Answer the menu the last beat stopped at (the wilderness's), where the
    // party stands, instead of going anywhere.
    bool waiting = false;
    // A script cell that must hold this value after the beat.
    std::optional<std::pair<std::uint16_t, std::uint16_t>> expect_cell;
    // Experience and gold the test gives each member before the beat, as the
    // Pool of Radiance expedition does before its harder areas, for a party
    // as strong as one brought from Pool of Radiance.
    unsigned test_experience = 0, test_gold = 0;
    // Play the beat again (up to 40 times), a square further east each time
    // along the row, until this cell is at least the value: Hap's patrols
    // meet the party as it goes about.
    std::optional<std::pair<std::uint16_t, std::uint16_t>> repeat_until_at_least;
    // Before the beat and between those rounds the test heals the party
    // outside the game's commands (rests where no patrol interrupts it), as
    // the Pool of Radiance expedition does where the game allows no rest.
    bool test_heal = false;
    // Step forward from the square instead of looking at it: a map's ways
    // out run as the party steps (a script's entry 0).
    bool step = false;
    // Take the first wand a treasure there holds, for the magic-user, readied.
    bool take_wand = false;
    // Script cells the test sets before the beat, standing for what the route
    // cannot yet do itself (as the cells a fight it does not win would set).
    std::vector<std::pair<std::uint16_t, std::uint16_t>> test_cells;
    // A fight the route's player loses with every dice it tries is won by the
    // test (its enemies dead, the party as it began), so the route goes on
    // through the story: a stand-in until its player wins it.
    bool test_wins = false;
    // The party only passes by: it answers NO where it is asked a question
    // (and the first choice elsewhere, fighting what it meets), and rests
    // first when a member is below half its Hit Points.
    bool passing = false;
    // The party waits for the monsters it meets to go where it is offered
    // (level 10's stoning patrols).
    bool avoiding = false;
    // Search mode on or off from this beat on, as a player's (S)earch, so
    // a hidden trap is noticed on the way.
    std::optional<bool> searching;
    // Steps on Pools of Darkness's area map, each in one of the eight
    // directions, instead of looking at or stepping from a square of a map.
    std::vector<opengold::por::ExplorationCommand> area_steps;
    // The party flees what it meets where it may (ATTACK or FLEE), as a
    // player crossing Gateway's wilds flees wandering monsters too strong
    // for it; it fights when it cannot get away.
    bool fleeing = false;
    // The minute of the day the party waits for before the beat, as a player
    // waits in camp for a ship's sailing: the test passes the time, for
    // OpenGoldBox's camp rests only a night.
    std::optional<unsigned> wait_until_minute;
    // Every enemy of the beat's fights must fall, none flee: at the Kraken
    // headquarters a guard who gets away sounds the alarm (the cluebook's
    // Alarms), as a player keeps one from escaping.
    bool holding_foes = false;
    // A fight none of the 120 tries wins is played again with new dice, up
    // to this many more times, until one does, as a player reloads a fight
    // the enemy most often wins in its first round (Treasures' Way Inn
    // sorceresses and the lair's black-robed men).
    unsigned reloads_until_won = 0;
    // The rounds of repeat_until_at_least stay on the beat's square, as a
    // player tries a climb again.
    bool repeat_in_place = false;
    // The sides (0 north, 1 east, 2 south, 3 west) the party leaves the
    // beat's maze fights by, one a fight, as Gateway's cluebook leads through
    // Ascore: a member runs off that side (rules::Encounter::escape_ends_fight).
    std::vector<unsigned> maze_exits;
    // The map's monsters break every camp (Pools of Darkness's Temple of Tyr
    // until it is freed): a passing party walks on hurt instead of resting.
    bool camp_broken = false;
    // A day's rest before the beat even when the party is whole, as a player
    // rests to memorize a spell the beat needs.
    bool memorize_first = false;
    // Play the beat again (up to 40 times) until this cell holds these bits:
    // the Fire Giant Cave's endless corridor until its spell is broken.
    std::optional<std::pair<std::uint16_t, std::uint16_t>> repeat_until_bits;
    // Answer the waiting menu with this choice, night after night, until the
    // party is whole, its spells chosen after each: Elminster's REST in Limbo.
    std::optional<std::string> rest_choice;
    // After the beat's event a member casts the first of these spells (camp
    // actions holding the text) that anyone has memorized, as a player
    // answers Treasures' Cortarra, whose script waits for a spell.
    std::vector<std::string> cast_after;
    // Items to take from a treasure there, each by its name (any case) and
    // the name of the member who takes and readies it, as a player gears the
    // party for a fight to come (Thorne's Ring of Fire Resistance).
    std::vector<std::pair<std::string, std::string>> take_items;
    // With rest_first, a night's rest even when every member is whole, so the
    // casters have their spells again, as a player rests before a known
    // fight (Treasures' Way Inn, whose fights follow one another).
    bool rest_for_spells = false;
    // Pools of Darkness: rest until whole where the camp's script sets a
    // rest period but lets the rest finish (the Tower of Marcus's quiet
    // rooms), instead of taking that period for a refused camp.
    bool rest_through_period = false;
    // Pools of Darkness: the clerics cast their cures on the badly hurt
    // before the beat instead of a camp, where a camp draws a swarm (the
    // Drow Testing Ground's Test, before its spiders' amulets are found).
    bool cure_first = false;
    // With cure_first, every member short of its Hit Points is cured, not
    // only the badly hurt, as a player heals up before a known fight where
    // no camp rests (Thorne's cave).
    bool cure_to_full = false;
    // Spells the casters memorize first as they rest before the beat, as a
    // player readies the counters a known fight needs (Dispel Magic against
    // Treasures' charms and holds).
    std::vector<std::string> memorize_first_of;
    // Pools of Darkness: a spell the casters memorize a copy of for each
    // member as they rest before the beat, and cast on each member then, as
    // a player wards the party before a known fight (Resist Fire before
    // Thorne's breath).
    std::string ward_each_with;
    // The members the ward is cast on, by name; every member when empty.
    std::vector<std::string> ward_members;
    // Pools of Darkness: a spell the casters memorize a copy of for each
    // member as they rest in the beat, for a later beat's ward where no camp
    // rests (Thorne's cave, whose camps fail until he falls).
    std::string ward_memorized;
    // Pools of Darkness: items each member readies again before the beat, by
    // their original item type, the largest stack first, as a player readies
    // a crossbow and the blessed bolts left after a fight shot the last of
    // the readied ones and took up the sword again.
    std::vector<std::pair<int, std::string>> ready_types;
    // A route with dice a beat (Pools of Darkness's, Gateway's): the beat
    // plays with other dice, its scripts' and its fights', as a player reloads
    // and steps again where a wandering monster no play of the party beats
    // came on it (the heart's agents of Tanetal); 0 keeps the beat's own.
    unsigned reloads = 0;
    // With take_wand, the wand goes in the pack, not readied, so Quick spends
    // none of its charges until a later beat readies it (ready_types), as a
    // player keeps Gateway's Wand of Ice Storm for the Star Mounts' mounds.
    bool pack_wand = false;
    // Keep purchased supplies packed until a later beat readies them.
    bool pack_purchases = false;
    // Reload before the whole event, so a chained fight can start again
    // with a different result from its first battle.
    bool reload_event = false;
    bool keep_preparation = false;
    std::string cast_after_by;
    // Physical walking already resolves arrival. Do not add a ten-minute
    // Look when a cleared-room waypoint needs no further command.
    bool arrive_only = false;
    std::vector<std::pair<std::string, std::string>> ready_names;
    // Drink actual carried potions through the ordinary campaign service
    // immediately before this beat, preserving the next fight's first action.
    std::vector<std::pair<std::string, std::string>> drink_names;
    // Wait at the actual destination with the player's ten-minute Look command.
    std::optional<unsigned> look_until_minute;
    // Match a particular question without spending an unrelated answer on
    // an earlier door or a repeated breathing reminder.
    std::vector<std::pair<std::string, std::string>> dialogue_answers;
    // Mark these actually carried scroll spells for the next ordinary rest.
    // Read Magic is cast first; neither the spell nor the scroll is supplied
    // by the test.
    std::vector<std::string> scribe_after;
};
} // namespace opengold::test

#endif
