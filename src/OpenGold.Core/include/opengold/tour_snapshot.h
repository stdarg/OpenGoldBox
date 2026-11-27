#ifndef OPENGOLD_TOUR_SNAPSHOT_H
#define OPENGOLD_TOUR_SNAPSHOT_H

// What a town session shows its view (RolfTourSession::snapshot) and the
// commands the view gives it.

#include "opengold/campaign_party.h"
#include "opengold/exploration_view.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace opengold::por
{
struct PartyPose
{
    unsigned x{}, y{}; // GEO coordinates.
    MapDirection facing{MapDirection::north};
    auto operator<=>(const PartyPose &) const = default;
};

enum class TourPhase
{
    running,
    awaiting_continue,
    awaiting_input,
    shopping,
    completed,
    faulted,
    combat,
    defeated,
    // After a won fight or a script's treasure: the items left lie in a pile
    // for the party to take; the script waits.
    treasure,
    // A Buck Rogers SHIP COMBAT waits for the ship fight (TourSnapshot's
    // ship_combat): the ship-combat screen plays it (ship_fight) and gives
    // the session its end (finish_ship_combat).
    ship_combat,
    // A Buck Rogers training waits for the trainee's choices
    // (buck_rogers_training): a Warrior's specialization and the skill
    // points; finish_buck_rogers_training keeps them and the team menu
    // returns.
    training
};

// A Buck Rogers script's SHIP COMBAT (1Fh; shipcbt.pas ShipCombat): the
// enemy ship's type, 1 to 5 (SHIPS.DAX record 39 + type), the operand the
// original never reads, the enemy pilot's skill and its to-hit number.
struct ShipCombatCall
{
    unsigned enemy_type{}, unused{}, pilot_skill{}, to_hit{};
    bool operator==(const ShipCombatCall &) const = default;
};

// A script took coins the payer lacked and the purse made change.
struct CoinPayment
{
    std::string payer;
    opengold::CoinExchange coins;
};

// The money and items a fight or a treasure gave the party, for the view to
// name.
struct LootTaken
{
    std::array<unsigned, 7> wealth{};
    std::vector<std::string> items; // Each item's name as the party knows it.
};

// One check made against a locked door, for the player's log.
struct DoorCheck
{
    std::string member;
    opengold::DoorMethod method{};
    int die{}, total{}, difficulty{};
    bool thieves_tools{}, magic_lock_picks{};
};

// One member's roll to find a secret door, for the player's log (SECRET-1).
struct SecretDoorSearch
{
    std::string member;
    int die{}, needed{};
};

struct TourSnapshot
{
    PartyPose pose;
    TourPhase phase{TourPhase::running};
    std::string dialogue, diagnostic;
    // The script's WHO waits for a party member, offered as the choices. The
    // dialogue keeps what the script said, such as the question being asked.
    bool choosing_member{};
    std::uint64_t revision{}, continue_ticket{};
    unsigned prompts{}, redraws{}, footsteps{};
    // The storm's thunder in Valhingen Graveyard (the original's fireball
    // sound), counted as footsteps are; not saved, as no sound is pending
    // once a save is made.
    unsigned thunder{};
    // The later games' scripts' sound effects other than the footstep: a
    // PLAY SOUND of 2 to 12, the sounds their driver plays (sounddrv.pas,
    // SoundCommand), the last one and the count, counted as footsteps are;
    // not saved, as thunder is not.
    unsigned script_sound{}, script_sounds{};
    int sprite_frame{-1}; // -1 hidden; native near/medium/far frames are 0/1/2.
    bool tour_finished{};
    // The party has just won the game: its closing words are shown until the
    // player goes on (the view plays the game's ending film first, if any).
    bool game_won{};
    // The script opened the camp menu (PROGRAM 9 outside an inn): the party
    // may camp now, and the view shows its camp dialog.
    bool camp_offered{};
    // Treasures' camp menu is open (open_camp): its map's camp entry and the
    // romance's scene are done, and the view shows the camp dialog.
    bool camp_menu_open{};
    // The later games' music, which a script's PLAY SOUND from 80h starts
    // (eclvm.pas, EclPlaySound): the piece (MUSIC.DAX's record), none once a
    // sound effect has stopped it (music.pas, SendSoundCommand), and the
    // starts, counted as footsteps are, a piece started again counting too.
    std::optional<unsigned> music;
    unsigned music_starts{};
    // Search mode is on (set_searching).
    bool searching{};
    unsigned script_id{}, event_runs{}, sprite_id{12};
    unsigned area_id{};
    std::vector<std::string> choices;
    // Dialogue and choices our campaign host writes (a training hall's
    // services), for the view to translate; empty while the script speaks.
    std::vector<opengold::rules::Message> host_dialogue, host_choices;
    bool number_input{};
    std::uint64_t picture_revision{};
    MapKnowledge visited;
    MapKnowledge seen; // Persistent map knowledge: visited or visible in a shown 3D view.
    std::vector<CoinPayment> payments; // This event's payments; cleared when the next begins.
    std::vector<LootTaken> loot_taken; // This event's loot; cleared when the next begins.
    // What auto-equip (EQUIP-1) put on from this event's loot, or from the
    // last purchase while shopping; cleared with the loot and at each purchase.
    std::vector<opengold::GearChange> gear_changes;
    // This step's checks against a locked door and whether it opened.
    std::vector<DoorCheck> door_checks;
    bool door_opened{};
    // This event's rolls to find a secret door, and whether one was found.
    std::vector<SecretDoorSearch> secret_door_searches;
    bool secret_door_found{};
    // On the wilderness map: the party's square in its window's coordinates
    // and the direction it last moved, 0 north and on clockwise to 7.
    bool overland{};
    unsigned overland_x{}, overland_y{}, overland_direction{};
    // The Buck Rogers games' solar map is on screen (PICTURE MODE 2, or the
    // area kind 70h): the screen that shows it drives the session from here.
    bool solar_map{};
    // The SHIP COMBAT the script waits on, while the phase is ship_combat.
    std::optional<ShipCombatCall> ship_combat;
};
enum class ExplorationCommand
{
    turn_left,
    turn_right,
    turn_around,
    forward,
    look,
    camp,
    // On the wilderness map, a step in one of eight directions.
    north,
    northeast,
    east,
    southeast,
    south,
    southwest,
    west,
    northwest
};
} // namespace opengold::por
#endif
