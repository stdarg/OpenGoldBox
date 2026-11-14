// Telling the Gold Box games apart and reading what Curse of the Azure Bonds
// changed (docs/GAMES.md, docs/games/curse.md). The census of the real files
// runs in tests/game_census.cmake.
#include "opengold/campaign_party.h"
#include "opengold/campaign_save.h"
#include "opengold/campaign_session.h"
#include "opengold/character_art.h"
#include "opengold/creature_catalog.h"
#include "opengold/curse_records.h"
#include "opengold/ecl.h"
#include "opengold/ecl_address.h"
#include "opengold/ecl_machine.h"
#include "opengold/formats.h"
#include "opengold/coin_purse.h"
#include "opengold/overland.h"
#include "opengold/por_sound.h"
#include "opengold/original_combat.h"
#include "opengold/original.h"
#include "opengold/original_spells.h"
#include "opengold/original_content.h"
#include "opengold/character_creator.h"
#include "opengold/combat_demo.h"
#include "tour_access.h"
#include <map>
#include "opengold/game_census.h"
#include "opengold/magic_item_catalog.h"
#include "opengold/pool_radiance.h"
#include "opengold/sound_bank.h"
#include "opengold/srd5.h"
#include "test_support.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace
{
using opengold::GoldBoxGame;
using opengold::test::check;
using opengold::test::rejects;

std::filesystem::path empty_folder(const std::string &name)
{
    const auto folder = std::filesystem::temp_directory_path() / ("opengold-games-" + name);
    std::filesystem::remove_all(folder);
    std::filesystem::create_directories(folder);
    return folder;
}

void each_game_is_told_apart_by_its_files()
{
    const std::vector<std::pair<std::string, GoldBoxGame>> markers{
        {"SQRPACI.DAX", GoldBoxGame::pool_of_radiance},
        {"tiles.dax", GoldBoxGame::curse_of_the_azure_bonds},
        {"SOUND.DAX", GoldBoxGame::secret_of_the_silver_blades},
        {"ITEMS.1", GoldBoxGame::gateway_to_the_savage_frontier},
        {"INTRO.BMA", GoldBoxGame::pools_of_darkness},
        {"LUCK.DAX", GoldBoxGame::treasures_of_the_savage_frontier},
        {"CKIT.EXE", GoldBoxGame::unlimited_adventures}};
    for (const auto &[marker, game] : markers)
    {
        const auto folder = empty_folder(marker);
        std::ofstream(folder / "GEO1.DAX").put('\0');
        std::ofstream(folder / marker).put('\0');
        check(opengold::identify_game(folder) == game,
              marker + " must identify " + std::string(opengold::game_title(game)));
        std::filesystem::remove_all(folder);
    }
    const auto folder = empty_folder("none");
    std::ofstream(folder / "GEO1.DAX").put('\0');
    check(!opengold::identify_game(folder), "A folder without a game's files is no game");
    std::filesystem::remove_all(folder);
}

// Curse's 422-byte creature record, each field at its place.
void curse_creatures_are_read_from_their_layout()
{
    std::vector<std::uint8_t> bytes(422);
    const std::string name = "ETTIN";
    bytes[0] = static_cast<std::uint8_t>(name.size());
    std::copy(name.begin(), name.end(), bytes.begin() + 1);
    bytes[16] = 18;  // Strength, permanent
    bytes[17] = 17;  // and current.
    bytes[27] = 9;   // Charisma, current.
    bytes[29] = 50;  // Exceptional Strength 18/50.
    bytes[115] = 50; // THAC0 10.
    bytes[116] = 7;
    bytes[117] = 2;
    bytes[120] = 70;
    bytes[121] = 47; // First known spell.
    bytes[228] = 12;
    bytes[251] = 0xd2; // Copper 1234.
    bytes[252] = 0x04;
    bytes[267] = 10;   // Fighter level.
    bytes[284] = 4;    // Four attacks in two rounds,
    bytes[286] = 2;    // two
    bytes[288] = 8;    // eight-sided dice
    bytes[290] = 0xff; // minus one.
    bytes[292] = 57;   // Armor Class 3.
    bytes[295] = 0x50; // 1950 experience.
    bytes[296] = 0x07;
    bytes[316] = 0x9e; // 1950 experience for killing it.
    bytes[317] = 0x07;
    bytes[332] = 3;
    bytes[407] = 1;
    bytes[420] = 66;
    bytes[421] = 9;

    const auto creature = opengold::curse::decode_creature(bytes);
    check(creature.has_value(), "A 422-byte record must decode");
    check(creature->game == GoldBoxGame::curse_of_the_azure_bonds && creature->raw == bytes,
          "It keeps its game and bytes");
    check(creature->name == name, "The name");
    check(creature->abilities.strength == 18 && creature->current_abilities.strength == 17,
          "Strength's permanent and current scores");
    check(creature->current_abilities.charisma == 9, "Charisma's current score");
    check(creature->abilities.exceptional_strength == 50, "Exceptional Strength");
    check(creature->base_thac0 == 10 && creature->base_armor_class == 3,
          "THAC0 and Armor Class are 60 minus the stored byte");
    check(creature->race == 7 && creature->character_class == 2, "Race and class");
    check(creature->max_hit_points == 70 && creature->current.hit_points_raw == 66, "Hit points");
    check(creature->spell_book.size() == 100 && creature->spell_book[0] == 47, "Known spells");
    check(creature->memorized_spells.size() == 84, "84 memorized spells");
    check(creature->base_movement == 12 && creature->current.movement == 9, "Movement");
    check(creature->wealth[0] == 1234, "Coins");
    check(creature->levels.fighter == 10, "Fighter level");
    const auto &attack = creature->base_attacks[0];
    check(attack.attacks_per_two_rounds == 4 && attack.damage.count == 2 &&
              attack.damage.sides == 8 && attack.damage.modifier == -1,
          "The first attack's rate and dice");
    check(creature->experience == 1872 && creature->base_xp_award == 1950,
          "Experience and its award");
    check(creature->item_count == 3 && creature->hostile == 1, "Items and hostility");
    check(opengold::por::decode_character(bytes, GoldBoxGame::curse_of_the_azure_bonds).has_value(),
          "decode_character reads each game's layout");

    bytes.push_back(0);
    check(!opengold::curse::decode_creature(bytes), "Pool of Radiance's sizes are not Curse's");
    bytes.pop_back();
    bytes[0] = 16;
    check(!opengold::curse::decode_creature(bytes), "A name holds at most 15 letters");
}

// Curse runs its scripts at 8000h, gives ECL CLOCK a second operand and adds three commands.
void curse_scripts_run_in_their_own_bank()
{
    std::vector<std::uint8_t> record{0, 0};
    constexpr std::uint16_t body = 0x8000 + 5 * 4;
    for (int entry = 0; entry < 5; ++entry)
    {
        record.insert(record.end(), {0x01, 0x02, body & 0xff, body >> 8});
    }
    record.insert(record.end(), {0x40, 0x00, 0x05});             // DESTROY ITEMS 5
    record.insert(record.end(), {0x34, 0x00, 0x01, 0x00, 0x02}); // ECL CLOCK 1, 2
    record.push_back(0x00);                                      // EXIT

    const auto program =
        opengold::por::EclProgram::decode(record, "test", GoldBoxGame::curse_of_the_azure_bonds);
    check(program.entries()[0] == body, "The entries point into the body at 8014h");
    const auto destroy = program.instruction(body);
    check(opengold::por::ecl_opcode(destroy.opcode, program.game()).name == "DESTROY ITEMS" &&
              destroy.operands.size() == 1 && destroy.operands[0].value == 5,
          "DESTROY ITEMS takes one operand");
    const auto clock = program.instruction(destroy.next);
    check(clock.operands.size() == 2, "Curse's ECL CLOCK takes two operands");
    check(opengold::por::walk_reachable(program).errors.empty(), "The script walks cleanly");
    check(opengold::por::ecl_opcode(0x34).operands == 1,
          "Pool of Radiance's ECL CLOCK keeps one operand");
    rejects(
        [&]
        {
            static_cast<void>(opengold::por::EclProgram::decode(record, "test"));
        },
        "Pool of Radiance's scripts start at 9900h");
    rejects(
        []
        {
            static_cast<void>(opengold::por::ecl_opcode(0x40));
        },
        "Pool of Radiance has no command 40h");
}

// The script machine runs a Curse script in Curse's bank: its entries, its
// commands and a write into its own code.
void the_machine_runs_curse_scripts()
{
    std::vector<std::uint8_t> record{0, 0};
    constexpr std::uint16_t body = 0x8000 + 5 * 4;
    for (int entry = 0; entry < 5; ++entry)
    {
        record.insert(record.end(), {0x01, 0x01, body & 0xff, body >> 8});
    }
    constexpr std::uint16_t own_byte = body + 12;                      // The EXIT below.
    record.insert(record.end(), {0x09, 0x00, 0x07, 0x01, 0x00, 0x40}); // SAVE 7 at 4000h
    record.insert(record.end(), {0x09, 0x00, 0x00, 0x01, own_byte & 0xff, own_byte >> 8});
    record.push_back(0x00); // EXIT, overwritten with EXIT
    opengold::por::EclMachine machine(
        std::make_shared<const opengold::por::EclProgram>(opengold::por::EclProgram::decode(
            record, "CURSE:1", GoldBoxGame::curse_of_the_azure_bonds)));
    machine.bind_variable(opengold::por::EclAddress{0x4000}, 0);
    check(machine.start(0), "A Curse script starts at its first entry");
    const auto result = machine.run();
    check(result.state == opengold::por::EclState::completed,
          "It runs to its EXIT: " + result.diagnostic);
    check(machine.variable(opengold::por::EclAddress{0x4000}) == 7, "SAVE wrote the variable");
}

// Every cell Curse's source names lies where moving Pool of Radiance's zones puts it.
void curse_script_memory_is_pool_of_radiance_moved()
{
    using opengold::por::EclAddress;
    using opengold::por::game_address;
    constexpr auto curse = GoldBoxGame::curse_of_the_azure_bonds;
    const std::vector<std::pair<std::uint16_t, std::uint16_t>> cells{
        {0x49C5, 0x4BC5}, // The 3D map loaded.
        {0x49C9, 0x4BC9}, // The clock's hour.
        {0x49E6, 0x4BE6}, // A 3D map is loaded.
        {0x49F2, 0x4BF2}, // The script before the last NEW ECL.
        {0x6DC1, 0x7EC1}, // The monsters' distance.
        {0x6DC6, 0x7EC6}, // Morale.
        {0x6DC9, 0x7EC9}, // The party's next step blocked.
        {0x6DCA, 0x7ECA}, // Search flags.
        {0x6DCB, 0x7ECB}, // Surprise.
        {0x6DD2, 0x7ED2}, // Rest checks.
        {0x6DD5, 0x7ED5}, // Leaving the map.
        {0x6DE1, 0x7EE1}, // The portrait's head.
        {0x6DE6, 0x7EE6}, // The mirror duel.
        {0x6E33, 0x7F33}, // An ally attacked.
        {0x6E3E, 0x7F3E}, // The party's size.
        {0x6E70, 0x7F70}, // Monsters' to-hit.
        {0x6E72, 0x7F72}, // The party's movement in combat.
        {0x9700, 0x7A00}, // The strings.
        {0xC04B, 0xC04B}, // The party's position stays.
    };
    for (const auto &[pool, expected] : cells)
    {
        check(game_address(EclAddress{pool}, curse).location == expected,
              "Cell " + std::to_string(pool) + " in Curse");
        check(game_address(EclAddress{pool}, GoldBoxGame::pool_of_radiance).location == pool,
              "Pool of Radiance keeps its own cells");
    }
    // The current character's cells keep their offsets in zone 1 (eclmenu.pas SpecialReadZ1).
    check(game_address(opengold::por::ecl_character_hit_points, curse).location == 0x7D19,
          "The character's hit points move with zone 1");
}

// A script's CALL runs an engine routine by the address its game gives it:
// Curse's view redraw, 2E10h, means nothing to Pool of Radiance.
void engine_routines_are_the_games()
{
    using namespace opengold::por;
    const auto run_call = [](GoldBoxGame game)
    {
        const auto origin = ecl_memory(game).origin;
        const auto body = static_cast<std::uint16_t>(origin + 5 * 4);
        std::vector<std::uint8_t> record{0, 0};
        for (int entry = 0; entry < 5; ++entry)
        {
            record.insert(record.end(), {0x01, 0x01, static_cast<std::uint8_t>(body & 0xff),
                                         static_cast<std::uint8_t>(body >> 8)});
        }
        record.insert(record.end(), {0x2D, 0x02, 0x10, 0x2E, 0x00}); // CALL 2E10h, EXIT
        EclMachine machine(
            std::make_shared<const EclProgram>(EclProgram::decode(record, "CALL", game)));
        machine.enable_host(0x2D);
        check(machine.start(0), "The script starts");
        return machine.run();
    };
    const auto curse = run_call(GoldBoxGame::curse_of_the_azure_bonds);
    check(curse.state == EclState::waiting && curse.request.has_value(),
          "Curse's redraw reaches the host");
    check(run_call(GoldBoxGame::pool_of_radiance).state == EclState::completed,
          "In Pool of Radiance 2E10h is no routine, and the CALL does nothing");
    const auto routines = ecl_engine_routines(GoldBoxGame::curse_of_the_azure_bonds);
    check(routines.redraw_view == 0x2E10 && routines.sound == 0xB200 &&
              routines.move_forward ==
                  ecl_engine_routines(GoldBoxGame::pool_of_radiance).move_forward,
          "Curse's routines: its own redraw and sound, Pool of Radiance's step forward");
}

void the_font_has_eight_bytes_a_glyph()
{
    std::vector<std::uint8_t> font(64 * 8);
    font[8] = 0x0f; // Glyph 1's top row.
    const auto glyphs = opengold::decode_game_font(font);
    check(glyphs && glyphs->size() == 64 && (*glyphs)[1][0] == 0x0f, "64 glyphs, in order");
    font.pop_back();
    check(!opengold::decode_game_font(font), "Whole glyphs only");
    check(!opengold::decode_game_font(std::vector<std::uint8_t>(63 * 8)),
          "Upper-case letters need the first 64 glyphs");
}

// The game's own loaders read Curse's files, when the developer has them
// (OPENGOLD_GOLDBOX_DIR, docs/GAMES.md).
void the_loaders_read_curse()
{
    const char *games = std::getenv("OPENGOLD_GOLDBOX_DIR");
    const auto folder = games ? std::filesystem::path(games) / "CURSE" : std::filesystem::path();
    if (!games || opengold::identify_game(folder) != GoldBoxGame::curse_of_the_azure_bonds)
    {
        std::cout << "Curse's files are not here; its loaders are not checked.\n";
        return;
    }
    constexpr auto curse = GoldBoxGame::curse_of_the_azure_bonds;
    const auto creatures = opengold::por::CreatureCatalog::load(folder, curse);
    check(creatures.all().size() == 81, "Curse has 81 creature records");
    bool found_ettin = false;
    for (const auto &[id, creature] : creatures.all())
    {
        if (creature.stored.name == "ETTIN")
        {
            found_ettin =
                creature.stored.base_armor_class == 3 && creature.stored.max_hit_points == 70;
        }
    }
    check(found_ettin, "The ettin has Armor Class 3 and 70 hit points");
    const auto items = opengold::por::MagicItemCatalog::load(folder, curse);
    check(!items.lists.empty(), "Curse's magic items list");
    const auto art = opengold::por::CharacterArt::load(folder, curse);
    check(art.heads().empty() && art.combat_heads().size() == 56,
          "Curse's player characters have combat icons but no portraits to choose");
    const auto sounds = opengold::por::SoundBank::load(folder, curse);
    check(sounds.at(10).audible, "Curse's footstep sounds");
    // Its fights out of doors: open ground from its own battlefield tiles,
    // trees where a place's terrain has them (ovr011, SetupWildernessFloor).
    std::ifstream start(folder / "START.EXE", std::ios::binary);
    const std::vector<std::uint8_t> start_bytes{std::istreambuf_iterator<char>(start), {}};
    const auto image = opengold::por::unpack_sound_executable(start_bytes);
    const auto wilderness = opengold::por::curse_wilderness(image);
    check(wilderness && wilderness->combat_tiles.size() == 0x42 &&
              wilderness->place_flags.size() == 33,
          "Curse's START.EXE has its battlefield tiles and its places' terrain");
    const auto field_with = [&](unsigned flags)
    {
        std::uint32_t seed = 11;
        return opengold::por::wilderness_battlefield(
            wilderness->combat_tiles, flags,
            [&](int count, int sides)
            {
                int total = 0;
                for (int n = 0; n < count; ++n)
                {
                    seed = seed * 1103515245 + 12345;
                    total += static_cast<int>((seed >> 16) % static_cast<unsigned>(sides)) + 1;
                }
                return total;
            });
    };
    const auto blocked = [](const opengold::por::DungeonBattlefield &field)
    {
        return std::ranges::count(field.geometry.terrain, opengold::rules::Terrain::obstacle);
    };
    const auto open_ground = field_with(0x80), forest = field_with(0x08);
    check(open_ground.geometry.width == 50 && open_ground.geometry.height == 25 &&
              blocked(open_ground) < blocked(forest),
          "A wilderness field is open ground, with more trees in a forest");
}

// Curse's opening, as the game plays it: its first script wakes the party in a
// small room, shows picture 10, then leaves it on its first map, square (7,13)
// facing east, with the view drawn (docs/GAMES.md, phase 3 step 1).
void curse_opening_reaches_the_first_map()
{
    using opengold::por::TourPhase;
    const char *games = std::getenv("OPENGOLD_GOLDBOX_DIR");
    const auto folder = games ? std::filesystem::path(games) / "CURSE" : std::filesystem::path();
    if (!games || opengold::identify_game(folder) != GoldBoxGame::curse_of_the_azure_bonds)
    {
        std::cout << "Curse's files are not here; its opening is not played.\n";
        return;
    }
    auto session = opengold::load_campaign(folder, GoldBoxGame::curse_of_the_azure_bonds);
    const auto settle = [&session]
    {
        for (int step = 0; step < 50 && session.snapshot().phase == TourPhase::running; ++step)
        {
            session.advance(1.0);
        }
    };
    const auto diagnostics = [&session]
    {
        return session.snapshot().diagnostic +
               (session.script_diagnostics().empty() ? "" : session.script_diagnostics().front());
    };
    settle();
    check(session.snapshot().phase == TourPhase::awaiting_continue &&
              session.snapshot().dialogue.find("YOU AWAKEN") != std::string::npos,
          "The opening wakes the party: " + diagnostics());
    check(session.snapshot().area_id == 1, "Its first map is loaded");
    check(session.choose(session.snapshot().continue_ticket, 0), "The party goes on");
    settle();
    check(session.picture().has_value(), "Picture 10 shows: " + diagnostics());
    check(session.choose(session.snapshot().continue_ticket, 0), "The party goes on again");
    settle();
    const auto &snapshot = session.snapshot();
    check(snapshot.phase == TourPhase::completed, "The opening ends: " + diagnostics());
    check(snapshot.pose.x == 7 && snapshot.pose.y == 13 &&
              snapshot.pose.facing == opengold::por::MapDirection::east,
          "The party stands on square (7,13) facing east");
    check(!session.picture().has_value(), "The redraw puts the view back over the picture");
    check(session.script_diagnostics().empty(), "The opening needs nothing unsupported");
    const auto colours = [](const opengold::Image &image)
    {
        std::set<std::uint32_t> seen;
        for (std::size_t p = 0; p + 3 < image.rgba.size(); p += 4)
        {
            seen.insert(image.rgba[p] << 16 | image.rgba[p + 1] << 8 | image.rgba[p + 2]);
        }
        return seen.size();
    };
    const auto view = session.observe_view();
    check(colours(view) > 2, "The 3D view shows the room's walls");
    check(session.explore(opengold::por::ExplorationCommand::turn_left), "The party turns");
    check(session.observe_view().rgba != view.rgba, "Turning shows another view");
    // Through the room's west door the innkeeper welcomes the party to Tilverton.
    check(session.explore(opengold::por::ExplorationCommand::turn_left), "The party faces west");
    check(session.explore(opengold::por::ExplorationCommand::forward), "The party steps");
    settle();
    check(session.snapshot().pose.x == 6 && session.snapshot().pose.y == 13 &&
              session.snapshot().dialogue.find("TILVERTON") != std::string::npos,
          "The innkeeper speaks: " + diagnostics());
}

// Curse's step 2 (docs/GAMES.md): every square of the map the party stands on,
// each of its events run to its end, answering the first choice; prints what
// the scripts still need. Run with --curse-walk.
// True when every event it reached ran: no script needs a command or form
// the session lacks.
// A square of a story's route: stood on, facing, its event played with these
// answers (a choice whose label holds the answer, or input to type), in turn.
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
    // The rounds of repeat_until_at_least stay on the beat's square, as a
    // player tries a climb again.
    bool repeat_in_place = false;
};

// Answers that train each of the route's members a level at a hall: its
// name, then the hall's own Train (the first choice).
std::vector<std::string> train_everyone()
{
    return {"YES",      "paladin 1", "paladin 2",    "ranger 3",
            "cleric 4", "cleric 5",  "magic_user 6", "Leave"};
}

// Curse's story, beat by beat, as its cluebook tells it (the route test, plan
// step 7): Tilverton, until the party is exiled to the wilderness.
const std::vector<RouteBeat> &curse_route()
{
    using D = opengold::por::MapDirection;
    static const std::vector<RouteBeat> beats{
        {1, 12, 7, D::north, {}},      // The armor shop.
        {1, 6, 5, D::north, {}},       // Julynn the sage.
        {1, 1, 0, D::north, {}},       // The guards turn the party back once,
        {1, 1, 1, D::north, {}, true}, // twice, and the king's carriage comes:
                                       // the party surrenders, is freed from jail
                                       // and led to the Thieves' Guild, rested.
        {1, 9, 3, D::north, {}, true}, // The guildmaster's news, rested; the Fire Knives burst in.
        {3, 0, 0, D::south, {}, true, true}, // Out through the guild's back to the sewers.
        // The guild's secret training hall, eight levels over (an award stops
        // short of the level after next), to levels 10 and 11: Dracandros's
        // guards, whom the original's AI left behind his courtyard's walls,
        // all reach the party (docs/DEVIATIONS.md, the computer's way round
        // walls).
        {3, 14, 6, D::north, train_everyone(), false, false, false, std::nullopt, 60000, 7000},
        {3, 14, 6, D::north, train_everyone(), false, false, false, std::nullopt, 60000, 7000},
        {3, 14, 6, D::north, train_everyone(), false, false, false, std::nullopt, 60000, 7000},
        {3, 14, 6, D::north, train_everyone(), false, false, false, std::nullopt, 60000, 7000},
        {3, 14, 6, D::north, train_everyone(), false, false, false, std::nullopt, 60000, 7000},
        {3, 14, 6, D::north, train_everyone(), false, false, false, std::nullopt, 60000, 7000},
        {3, 14, 6, D::north, train_everyone(), false, false, false, std::nullopt, 400000, 9000},
        {3, 14, 6, D::north, train_everyone(), false, false, false, std::nullopt, 400000, 9000},
        {4, 0, 0, D::south, {}, true, true}, // Through the sewers to the Fire Knives' hideout.
        {4, 3, 13, D::north, {"stop:JOURNEY"}, true},             // The Fire Knives' leader and his
                                                                  // band; freed of the first bond,
                                                                  // the party is exiled to the
                                                                  // wilderness's menu.
        {0, 0, 0, D::north, {"stop:JOURNEY"}, true, false, true}, // Rested in camp.
        // Shadowdale is the journey's destination (cell 0x4C9C), though the
        // sage's flag is still set from Tilverton.
        {0,
         0,
         0,
         D::north,
         {"JOURNEY", "SHADOWDALE", "stop:TRAIL"},
         false,
         false,
         true,
         std::pair<std::uint16_t, std::uint16_t>{0x4C9C, 1}},
        {0, 0, 0, D::north, {"TRAIL", "stop:JOURNEY"}, false, false, true},
        {0, 0, 0, D::north, {"stop:JOURNEY"}, true, false, true},
        // On to Hap, resting at each stop: by boat on the Ashaben (kept clear of
        // pirates), then by the menu's first way.
        {0,
         0,
         0,
         D::north,
         {"JOURNEY", "ASHABENFORD", "BY BOAT", "stop:JOURNEY"},
         false,
         false,
         true},
        {0, 0, 0, D::north, {"stop:JOURNEY"}, true, false, true},
        {0, 0, 0, D::north, {"JOURNEY", "STANDING STONE", "stop:JOURNEY"}, false, false, true},
        {0, 0, 0, D::north, {"stop:JOURNEY"}, true, false, true},
        {0, 0, 0, D::north, {"JOURNEY", "ESSEMBRA", "stop:JOURNEY"}, false, false, true},
        {0, 0, 0, D::north, {"stop:JOURNEY"}, true, false, true},
        {0, 0, 0, D::north, {"JOURNEY", "HAP", "stop:JOURNEY"}, false, false, true},
        {0, 0, 0, D::north, {"stop:JOURNEY"}, true, false, true},
        {0, 0, 0, D::north, {"ENTER CITY"}, false, false, true}, // Hap.
        // Hap's dark elf patrols: once four have fallen (cell 0x4C47 counts
        // them) the barn holds two of each kind, not six. A camp in Hap draws
        // them on the party, so it does not rest there.
        {50,
         0,
         0,
         D::north,
         {"Fight"},
         false,
         false,
         false,
         std::nullopt,
         0,
         0,
         std::pair<std::uint16_t, std::uint16_t>{0x4C47, 4},
         true},
        {50, 8, 4, D::north, {}}, // The efreeti and the dark elves in the barn.
        // Out through Hap's gate, following the efreeti's map to the caves.
        {50,
         3,
         0,
         D::north,
         {"YES", "CAVES"},
         false,
         false,
         false,
         std::nullopt,
         0,
         0,
         std::nullopt,
         false,
         true},
        // Crimdrac the dracolich lets the party live and takes it up to the
        // Wizard's Tower; on its roof the party, freed of the bond, parleys
        // with the dragons and is nice to them, as the cluebook advises.
        {50,
         7,
         12,
         D::north,
         {"PARLAY WITH THE DRAGONS", "NICE"},
         false,
         false,
         false,
         std::nullopt,
         0,
         0,
         std::nullopt,
         true},
        // Dracandros in the tower's courtyard; among his treasures, the Helm
        // of Dragons (cell 0x4C59): the second bond is gone.
        {51,
         4,
         13,
         D::north,
         {},
         false,
         false,
         false,
         std::pair<std::uint16_t, std::uint16_t>{0x4C59, 1},
         0,
         0,
         std::nullopt,
         true},
        // Out of the tower by its secret passage, to Hap's village, and the
        // dead raised at its Temple of Sune.
        {51,
         7,
         15,
         D::east,
         {"WILDERNESS", "VILLAGE"},
         false,
         false,
         false,
         std::nullopt,
         0,
         0,
         std::nullopt,
         false,
         true},
        // (Stepping in from the street, as a building is entered only from
        // outside: each sets cell 0x4C02 until the party is out again.)
        {50,
         3,
         8,
         D::west,
         {"YES", "raise:dead", "Leave"},
         false,
         false,
         false,
         std::nullopt,
         0,
         0,
         std::nullopt,
         false,
         true},
        // Rested at Hap, out of its gate to the wilderness, toward Yulash.
        {50,
         3,
         0,
         D::north,
         {"YES", "WILDERNESS", "stop:JOURNEY"},
         true,
         false,
         false,
         std::nullopt,
         0,
         0,
         std::nullopt,
         false,
         true},
        {0, 0, 0, D::north, {"JOURNEY", "ESSEMBRA", "stop:JOURNEY"}, false, false, true},
        {0, 0, 0, D::north, {"stop:JOURNEY"}, true, false, true},
        {0, 0, 0, D::north, {"JOURNEY", "STANDING STONE", "stop:JOURNEY"}, false, false, true},
        {0, 0, 0, D::north, {"stop:JOURNEY"}, true, false, true},
        {0, 0, 0, D::north, {"JOURNEY", "HILLSFAR", "stop:JOURNEY"}, false, false, true},
        {0, 0, 0, D::north, {"stop:JOURNEY"}, true, false, true},
        {0, 0, 0, D::north, {"JOURNEY", "YULASH", "TRAIL", "stop:JOURNEY"}, false, false, true},
        // Into Yulash, asking the Red Plumes' leave (sneaking in earns their
        // commander's disapproval, as the cluebook says).
        {0, 0, 0, D::north, {"ENTER CITY", "ASK PERMISSION", "PARLAY"}, true, false, true},
        // In the commander's waiting room, a step east brings out Zhentil Keep
        // spies; the commander thinks well of a party that fights them.
        {16,
         0,
         3,
         D::east,
         {"FIGHT", "HAUGHTY"},
         false,
         false,
         false,
         std::nullopt,
         0,
         0,
         std::nullopt,
         false,
         true},
        // The shambling mounds over a dead cleric, who holds the Wand of
        // Defoliation the Pit of Moander calls for.
        {16,
         11,
         8,
         D::north,
         {"Fight"},
         false,
         false,
         false,
         std::nullopt,
         0,
         0,
         std::nullopt,
         true,
         false,
         true},
        // Into the Pit of Moander.
        {16,
         11,
         0,
         D::north,
         {},
         false,
         false,
         false,
         std::nullopt,
         0,
         0,
         std::nullopt,
         true,
         true},
        // Alias and Dragonbait, who join a party that tells its story.
        {17, 1, 4, D::north, {"PARLAY", "NICE", "TELL HER YOUR STORY", "YES"}},
        // Down the stairs to the Pit's second level.
        {17, 15, 11, D::north, {"YES"}},
        // Mogion and Moander's pseudopods are not yet beaten by the route's
        // player: the test sets what their fall sets (ECL3.DAX 18: the
        // Gauntlet of Moander, cell 0x4C5B, and 0x4C2D), and the party climbs
        // back to the Pit's first level.
        {17,
         15,
         14,
         D::north,
         {"YES"},
         false,
         false,
         false,
         std::nullopt,
         0,
         0,
         std::nullopt,
         false,
         false,
         false,
         {{0x4C5B, 1}, {0x4C2D, 1}}},
        // The cultists' last stand by the Pit's way out (ten cultists, five
        // large vegepygmies, five shambling mounds), which the test wins for
        // now, and out of the Pit by its door to the west.
        {17,
         12,
         4,
         D::west,
         {},
         false,
         false,
         false,
         std::nullopt,
         0,
         0,
         std::nullopt,
         true,
         false,
         false,
         {},
         true},
        // The way out, facing west at 0,11 (ECL3.DAX 17), back to the
        // wilderness.
        {17,
         0,
         11,
         D::west,
         {"stop:JOURNEY"},
         false,
         false,
         false,
         std::nullopt,
         0,
         0,
         std::nullopt,
         false,
         true},
        // On from Yulash by trail to Voonlar, its dead raised at its temple.
        {0,
         0,
         0,
         D::north,
         {"JOURNEY", "VOONLAR", "TRAIL", "ENTER CITY", "TEMPLE", "raise:dead", "Leave", "stop:INN"},
         false,
         false,
         true},
        // Out of Voonlar back to Yulash's edge, camped there until healed, and
        // on to the edge of Zhentil Keep.
        {0,
         0,
         0,
         D::north,
         {"LEAVE", "JOURNEY", "YULASH", "TRAIL", "stop:JOURNEY"},
         false,
         false,
         true},
        {0, 0, 0, D::north, {"stop:JOURNEY"}, true, false, true},
        {0,
         0,
         0,
         D::north,
         {"JOURNEY", "ZHENTIL KEEP", "TRAIL", "stop:JOURNEY"},
         false,
         false,
         true},
        // Into Zhentil Keep.
        {0, 0, 0, D::north, {"ENTER CITY"}, false, false, true},
        // Olive Ruskettle leads the party by a secret way into the Shrine of
        // Bane.
        {32, 10, 11, D::north, {"YES"}},
        // Dimswart the sage, held in a cell, comes along.
        {32, 1, 7, D::north, {"YES"}},
        // The hooded woman (Dexam's medusa) takes the party to the Caves of
        // the Beholder, the only way out of the shrine. She shows herself
        // every other time the party passes (cell 0x4C1B), so it passes twice.
        {32, 2, 3, D::north, {}},
        {32, 2, 3, D::north, {"YES"}},
        // Dexam the Beholder and his minions, for the Amulet of Lathander
        // (map 33's events 15 and 16; the "beholder" at 2,1 is a gas spore).
        // His lair meets the party only while cell 0x4C03 is 0, which the
        // hooded woman set to 1 (ECL4.DAX 33 0x8832) and nothing on the way
        // clears; until it is known what clears it in the original, the test
        // does.
        {33,
         2,
         11,
         D::north,
         {},
         false,
         false,
         false,
         std::nullopt,
         0,
         0,
         std::nullopt,
         true,
         false,
         false,
         {{0x4C03, 0}},
         true},
        // Out of the caves, stepping off the map south of 11,15 (event 19 and
        // LEAVE_MAP), where Olive takes Dimswart away, back to the wilderness.
        {33,
         11,
         15,
         D::south,
         {"stop:JOURNEY"},
         false,
         false,
         false,
         std::nullopt,
         0,
         0,
         std::nullopt,
         true,
         true},
        // With the three artifacts, back by Yulash and Hillsfar to the
        // Standing Stones, where the grey man reveals himself as Tyranthraxus
        // and bids the party meet him at Myth Drannor, and on there.
        {0, 0, 0, D::north, {"JOURNEY", "YULASH", "TRAIL", "stop:JOURNEY"}, false, false, true},
        {0, 0, 0, D::north, {"JOURNEY", "HILLSFAR", "TRAIL", "stop:JOURNEY"}, false, false, true},
        {0, 0, 0, D::north, {"JOURNEY", "STANDING", "TRAIL", "stop:JOURNEY"}, false, false, true},
        {0,
         0,
         0,
         D::north,
         {"JOURNEY", "MYTH DRANNOR", "WILDERNESS", "ENTER CITY"},
         false,
         false,
         true},
        // Through the Burial Glen, east off its map by the path to the ruins of
        // Myth Drannor (ECL6.DAX 64, LEAVE_MAP facing east).
        {64,
         15,
         3,
         D::east,
         {"PATH"},
         false,
         false,
         false,
         std::nullopt,
         0,
         0,
         std::nullopt,
         false,
         true},
        // In the ruins, the head Rakshasa, answered haughtily, helps against
        // Tyranthraxus; a sewer grate leads into the ruined temple.
        {66, 9, 6, D::north, {"Parley", "HAUGHTY"}},
        {66,
         5,
         6,
         D::north,
         {"YES", "YES"},
         false,
         false,
         false,
         std::nullopt,
         0,
         0,
         std::nullopt,
         true},
        // In the temple's chapel Tyranthraxus takes the bonded party, Nameless
        // frees it and dies, and Tyranthraxus leaves his minions to it; then
        // up the main hall's stairs to the second floor.
        {67, 5, 11, D::north, {}, false, false, false, std::nullopt, 0, 0, std::nullopt, true},
        {67, 10, 7, D::north, {"YES"}},
        // Tyranthraxus, with high priests and margoyles, on the second floor:
        // the final battle.
        {67,
         6,
         1,
         D::north,
         {},
         false,
         false,
         false,
         std::nullopt,
         0,
         0,
         std::nullopt,
         true,
         false,
         false,
         {},
         true},
    };
    return beats;
}

// A step forward from a square of `map`: a way out of a map, or one of the
// ruins' passages, which its script turns into a move to another room
// (Secret's step entry), with these answers.
RouteBeat step_beat(unsigned map, unsigned x, unsigned y, opengold::por::MapDirection facing,
                    std::vector<std::string> answers = {})
{
    RouteBeat beat{map, x, y, facing, std::move(answers)};
    beat.step = true;
    return beat;
}

// Steps through the passages of Secret's ruins (map 32), each from a square
// the party walks to inside its room: the ruins are rooms of one map, each
// passage moving the party on to another room or sector (cell 4C4B).
void walk_ruins(
    std::vector<RouteBeat> &beats,
    std::initializer_list<std::tuple<unsigned, unsigned, opengold::por::MapDirection>> passages)
{
    for (const auto &[x, y, facing] : passages)
    {
        beats.push_back(step_beat(32, x, y, facing));
    }
}

// Steps through the passages of a map whose rooms its script joins (the
// mine's levels, a level's two halves told apart by cell 4C6B), each from a
// square the party walks to inside its room, as OPENGOLD_SECRET_FIND finds
// them. The party fights what it meets on the way and answers NO to what it
// is asked, so it only passes by.
void walk_passages(
    std::vector<RouteBeat> &beats, unsigned map,
    std::initializer_list<std::tuple<unsigned, unsigned, opengold::por::MapDirection>> passages,
    bool searching = false, bool test_wins = false, bool avoiding = false,
    const std::vector<std::string> &answers = {})
{
    for (const auto &[x, y, facing] : passages)
    {
        auto beat = step_beat(map, x, y, facing, answers);
        beat.passing = true;
        beat.searching = searching;
        beat.test_wins = test_wins;
        beat.avoiding = avoiding;
        beats.push_back(beat);
    }
}

// Steps across Gateway's overland (maps 21 to 26), as walk_passages walks a
// map, fleeing the wandering monsters it meets where it can.
void walk_overland(
    std::vector<RouteBeat> &beats, unsigned map,
    std::initializer_list<std::tuple<unsigned, unsigned, opengold::por::MapDirection>> steps)
{
    walk_passages(beats, map, steps);
    for (auto beat = beats.end() - static_cast<std::ptrdiff_t>(steps.size()); beat != beats.end();
         ++beat)
    {
        beat->fleeing = true;
    }
}

// Secret's route plays to the game's end once this is true; until then it
// stops on this map, the Castle Entrance (docs/GAMES.md, phase 4 step 4).
constexpr bool secret_route_ends_the_game = false;
constexpr unsigned secret_route_stops_on_map = 96;

// Secret's story, beat by beat, as its cluebook tells it (plan phase 4, step
// 4): from New Verdigris on.
const std::vector<RouteBeat> &secret_route()
{
    using D = opengold::por::MapDirection;
    static const std::vector<RouteBeat> beats = []
    {
        std::vector<RouteBeat> route;
        // The training hall (free): a party brought from Curse of the Azure
        // Bonds carries the experience of that game's end, which the test gives
        // (a stand-in, as Curse's route stands in for a party from Pool of
        // Radiance); an award stops short of the level after next, so the
        // party trains a level a visit, four times.
        for (int level = 0; level < 4; ++level)
        {
            RouteBeat experienced{16, 7, 6, D::north, train_everyone()};
            experienced.test_experience = 1'000'000;
            route.push_back(experienced);
        }
        // The armoury (location 9): a mirror for each member, readied, which
        // turns the gazes of basilisks and medusae back (the cluebook's advice).
        route.push_back({16, 14, 6, D::north, {"YES", "buy:MIRROR"}});
        route.insert(
            route.end(),
            {
                // The teleporter in the mayor's house's northmost room, to the Well
                // of Knowledge.
                {16, 3, 0, D::north, {"YES"}},
                // The Ancient Red Dragon coiled about the Well; with it dead the
                // Well is safe and asks for gems, which the party keeps.
                {33, 7, 7, D::north, {"NO"}, true},
                // The portal home, and Marcus's house: his welcome mat and his pets.
                {33, 1, 1, D::north, {"YES"}},
                {16, 15, 10, D::north, {}, true},
                // The Temple of Gond heals the party for nothing.
                {16, 10, 9, D::north, {"YES", "Heal", "paladin 1", "Pay", "Heal", "paladin 2",
                                       "Pay", "Heal", "ranger 3",  "Pay", "Heal", "cleric 4",
                                       "Pay", "Heal", "cleric 5",  "Pay", "Heal", "magic_user 6",
                                       "Pay", "Leave"}},
            });
        // Out by the town's east gate, into the ruins (sector 5).
        route.push_back(step_beat(16, 15, 8, D::east, {"YES"}));
        // Through the ruins to the lair of the Old Red Dragon (location 13,
        // sector 9): west through sector 5, south through sector 7.
        walk_ruins(route,
                   {{14, 2, D::east},  {15, 5, D::east},  {10, 2, D::south}, {5, 0, D::north},
                    {10, 4, D::north}, {0, 11, D::south}, {2, 10, D::west},  {0, 6, D::south},
                    {14, 2, D::east},  {13, 4, D::west},  {6, 0, D::west},   {1, 14, D::south},
                    {3, 0, D::north},  {0, 4, D::west},   {3, 5, D::south},  {7, 11, D::south},
                    {8, 12, D::north}, {1, 9, D::south},  {2, 10, D::west},  {0, 6, D::south},
                    {0, 5, D::east},   {13, 4, D::west},  {1, 1, D::west},   {5, 0, D::north}});
        // The lair: the Old Red Dragon, rested for, and the Amulet of
        // Eldamar (cell 4C53); its gateway takes the party to the Well.
        auto lair = step_beat(32, 0, 3, D::north, {"ATTACK", "YES"});
        lair.rest_first = true;
        lair.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x4C53, 255};
        route.push_back(lair);
        // From the Well's north-west corner into the ruins (sector 6), and
        // through sector 8 to the Black Circle Headquarters' main entrance;
        // rested at the Well, safe since its dragon fell.
        auto leave_well = step_beat(33, 0, 0, D::north);
        leave_well.rest_first = true;
        route.push_back(leave_well);
        walk_ruins(route, {{2, 0, D::north},
                           {6, 0, D::west},
                           {1, 2, D::west},
                           {1, 14, D::south},
                           {7, 11, D::south},
                           {2, 10, D::west},
                           {1, 14, D::south},
                           {2, 14, D::south},
                           {11, 2, D::west},
                           {3, 5, D::south},
                           {7, 13, D::west},
                           {1, 1, D::west},
                           {0, 6, D::south},
                           {1, 0, D::south},
                           {0, 3, D::west}});
        // The Headquarters (map 34): its captain bars the way and is fought.
        route.push_back(step_beat(32, 0, 2, D::south, {"ATTACK"}));
        // The commander and his troops, and on his desk the key to the
        // secret door. The corridors allow no rest (guards come).
        route.push_back({34, 12, 6, D::north, {}});
        route.push_back({34, 10, 5, D::north, {}});
        // The office's gateway to the Well (building 16), to rest where it is
        // safe, and back.
        route.push_back({34, 10, 6, D::north, {"YES"}});
        route.push_back({33, 14, 14, D::north, {"YES"}, true});
        // The key finds the secret door south of the corridor (a search); the
        // locked door beyond is bashed.
        route.push_back({34, 8, 4, D::south, {}});
        route.push_back(step_beat(34, 8, 4, D::south));
        route.push_back(step_beat(34, 8, 7, D::south, {"Bash", "Bash", "Bash", "Bash"}));
        // The hatchling pool and the Inner Sanctum beyond: its gateway takes
        // the party to the Well.
        route.push_back({34, 13, 13, D::south, {"GO FORWARD"}});
        route.push_back({34, 13, 14, D::south, {"ATTACK", "YES"}});
        // Rested at the Well, out by its north-west corner again and through
        // sectors 6 and 8 to the mine's entrance (location 12), unguarded now
        // the Headquarters has fallen.
        route.push_back(leave_well);
        walk_ruins(route, {{2, 0, D::north},
                           {6, 0, D::west},
                           {1, 2, D::west},
                           {1, 14, D::south},
                           {7, 11, D::south},
                           {2, 10, D::west},
                           {1, 14, D::south}});
        route.push_back({32, 3, 12, D::north, {"YES"}});
        // The mine's temple level (script 51 on map 49): the passage south of
        // the wheel lift, the minotaurs guarding the temple's doors, and the
        // Temple of Tyr (map 50), where Derf Strongarm sets the test of
        // Oswulf's staff and gives the Ring of Tyr (cell 4C85).
        route.push_back(step_beat(49, 3, 6, D::south, {"ATTACK"}));
        route.push_back(step_beat(49, 5, 15, D::south, {"ENTER"}));
        auto derf = step_beat(50, 14, 3, D::north, {"TALK", "NO"});
        derf.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x4C85, 1};
        route.push_back(derf);
        // Back out of the temple and through the passage to the wheel lift.
        route.push_back(step_beat(50, 8, 0, D::north));
        route.push_back(step_beat(49, 5, 12, D::north));
        route.push_back({49, 3, 3, D::north, {"YES", "1"}});
        // Level 1 (script 49 on map 48): west to the dead adventurer, whose
        // piece of the staff the Ring of Tyr finds (cell 4C84).
        walk_passages(
            route, 48,
            {{3, 3, D::west},    {2, 3, D::west},   {1, 3, D::west},    {4, 12, D::west},
             {3, 12, D::south},  {3, 13, D::west},  {2, 13, D::south},  {2, 14, D::west},
             {1, 14, D::south},  {1, 15, D::south}, {15, 8, D::south},  {15, 9, D::south},
             {15, 10, D::west},  {14, 10, D::west}, {13, 10, D::north}, {13, 9, D::west},
             {12, 9, D::west},   {11, 9, D::west},  {10, 0, D::west},   {9, 0, D::west},
             {8, 0, D::west},    {7, 0, D::west},   {6, 0, D::west},    {5, 0, D::west},
             {9, 7, D::west},    {8, 7, D::west},   {7, 7, D::west},    {6, 7, D::north},
             {6, 6, D::north},   {1, 13, D::north}, {1, 12, D::north},  {1, 11, D::north},
             {1, 10, D::north},  {1, 9, D::east},   {2, 9, D::east},    {3, 9, D::east},
             {4, 9, D::east},    {2, 15, D::east},  {3, 15, D::north},  {3, 14, D::east},
             {4, 14, D::north},  {4, 13, D::north}, {11, 8, D::east},   {12, 8, D::east},
             {13, 8, D::north},  {13, 7, D::north}, {13, 6, D::west},   {12, 6, D::west},
             {11, 6, D::west},   {10, 6, D::west},  {9, 6, D::west},    {8, 6, D::west},
             {7, 6, D::west},    {15, 0, D::west},  {14, 0, D::west},   {13, 0, D::west},
             {12, 0, D::west},   {11, 0, D::south}, {11, 1, D::west},   {10, 1, D::west},
             {9, 1, D::west},    {8, 1, D::west},   {7, 1, D::west},    {6, 1, D::west},
             {5, 1, D::west},    {7, 15, D::west},  {6, 15, D::north},  {6, 14, D::west},
             {5, 14, D::north},  {5, 13, D::north}, {11, 13, D::north}, {11, 12, D::north},
             {11, 11, D::north}, {0, 10, D::north}, {0, 9, D::north},   {0, 8, D::north},
             {0, 7, D::north},   {0, 6, D::north},  {0, 5, D::north}});
        // Back to the lift, and down to level 2.
        walk_passages(
            route, 48,
            {{1, 1, D::south},   {0, 5, D::south},   {0, 6, D::south},  {0, 7, D::south},
             {0, 8, D::south},   {0, 9, D::south},   {0, 10, D::south}, {11, 11, D::south},
             {11, 12, D::south}, {11, 13, D::south}, {5, 13, D::south}, {5, 14, D::east},
             {6, 14, D::south},  {6, 15, D::east},   {7, 15, D::east},  {5, 1, D::east},
             {6, 1, D::east},    {7, 1, D::east},    {8, 1, D::east},   {9, 1, D::east},
             {10, 1, D::east},   {11, 1, D::north},  {11, 0, D::east},  {12, 0, D::east},
             {13, 0, D::east},   {14, 0, D::east},   {15, 0, D::east},  {7, 6, D::east},
             {8, 6, D::east},    {9, 6, D::east},    {10, 6, D::east},  {11, 6, D::east},
             {12, 6, D::east},   {13, 6, D::south},  {13, 7, D::south}, {13, 8, D::west},
             {12, 8, D::west},   {11, 8, D::south},  {4, 13, D::south}, {4, 14, D::south},
             {4, 15, D::west},   {3, 15, D::west},   {2, 15, D::west},  {4, 9, D::west},
             {3, 9, D::west},    {2, 9, D::west},    {1, 9, D::south},  {1, 10, D::south},
             {1, 11, D::south},  {1, 12, D::south},  {1, 13, D::south}, {6, 6, D::south},
             {6, 7, D::east},    {7, 7, D::east},    {8, 7, D::east},   {9, 7, D::east},
             {5, 0, D::east},    {6, 0, D::east},    {7, 0, D::east},   {8, 0, D::east},
             {9, 0, D::east},    {10, 0, D::east},   {11, 9, D::east},  {12, 9, D::east},
             {13, 9, D::south},  {13, 10, D::east},  {14, 10, D::east}, {15, 10, D::north},
             {15, 9, D::north},  {15, 8, D::north},  {1, 15, D::north}, {1, 14, D::east},
             {2, 14, D::north},  {2, 13, D::east},   {3, 13, D::north}, {3, 12, D::east},
             {4, 12, D::east},   {1, 3, D::east},    {2, 3, D::east}});
        route.push_back({48, 3, 3, D::north, {"YES", "2"}, true});
        // Level 2, searching (a trapdoor is in the way): to the miners' barracks, whose trapped
        // chest (disarmed or not) holds the second piece.
        walk_passages(route, 48,
                      {{3, 3, D::west},   {2, 3, D::west},   {1, 3, D::west},   {15, 0, D::west},
                       {14, 0, D::west},  {13, 0, D::west},  {12, 0, D::west},  {11, 0, D::south},
                       {11, 1, D::west},  {10, 1, D::west},  {9, 1, D::west},   {8, 1, D::west},
                       {7, 1, D::west},   {6, 1, D::west},   {5, 1, D::west},   {14, 6, D::west},
                       {13, 6, D::north}, {13, 5, D::west},  {12, 5, D::north}, {8, 15, D::north},
                       {8, 14, D::west},  {7, 14, D::north}, {7, 13, D::west},  {6, 13, D::north},
                       {6, 12, D::west},  {5, 12, D::west},  {5, 15, D::west},  {4, 15, D::north},
                       {4, 14, D::north}, {4, 13, D::north}, {14, 9, D::north}, {14, 8, D::north},
                       {14, 7, D::east},  {15, 7, D::north}, {15, 6, D::north}, {15, 5, D::west},
                       {14, 5, D::north}, {14, 4, D::north}, {14, 3, D::north}, {0, 3, D::north},
                       {0, 2, D::east},   {1, 2, D::east},   {2, 2, D::north},  {2, 1, D::north},
                       {2, 0, D::east},   {3, 0, D::east},   {4, 0, D::south},  {4, 1, D::south},
                       {4, 2, D::east},   {5, 2, D::east},   {6, 2, D::east},   {7, 2, D::east},
                       {8, 2, D::east},   {9, 2, D::east}},
                      true);
        route.push_back({48, 10, 2, D::north, {"SEARCH", "YES"}});
        // Back to the lift, and down to level 3.
        walk_passages(route, 48,
                      {{10, 2, D::west},  {9, 2, D::west},   {8, 2, D::west},   {7, 2, D::west},
                       {6, 2, D::west},   {5, 2, D::west},   {4, 2, D::north},  {4, 1, D::north},
                       {4, 0, D::west},   {3, 0, D::west},   {2, 0, D::south},  {2, 1, D::south},
                       {2, 2, D::west},   {1, 2, D::west},   {0, 2, D::south},  {0, 3, D::south},
                       {14, 3, D::south}, {14, 4, D::south}, {14, 5, D::east},  {15, 5, D::south},
                       {15, 6, D::south}, {15, 7, D::west},  {14, 7, D::south}, {14, 8, D::south},
                       {14, 9, D::south}, {4, 13, D::south}, {4, 14, D::south}, {4, 15, D::east},
                       {5, 15, D::east},  {5, 12, D::east},  {6, 12, D::south}, {6, 13, D::east},
                       {7, 13, D::south}, {7, 14, D::east},  {8, 14, D::south}, {8, 15, D::south},
                       {12, 5, D::east},  {13, 5, D::south}, {13, 6, D::east},  {14, 6, D::east},
                       {5, 1, D::east},   {6, 1, D::east},   {7, 1, D::east},   {8, 1, D::east},
                       {9, 1, D::east},   {10, 1, D::east},  {11, 1, D::north}, {11, 0, D::east},
                       {12, 0, D::east},  {13, 0, D::east},  {14, 0, D::east},  {15, 0, D::east},
                       {1, 3, D::east},   {2, 3, D::east}},
                      true);
        route.push_back({48, 3, 3, D::north, {"YES", "3"}, true});
        // Level 3: the wyverns' lair, whose cache holds the third piece.
        walk_passages(route, 48,
                      {{3, 3, D::north},   {3, 2, D::north},   {3, 1, D::north},  {14, 9, D::north},
                       {14, 8, D::north},  {14, 7, D::east},   {15, 7, D::north}, {15, 6, D::north},
                       {15, 5, D::west},   {14, 5, D::north},  {14, 4, D::north}, {14, 3, D::north},
                       {11, 13, D::north}, {11, 12, D::north}, {11, 11, D::east}, {12, 11, D::east},
                       {13, 11, D::east},  {5, 1, D::east},    {6, 1, D::east},   {7, 1, D::east},
                       {8, 1, D::east},    {9, 1, D::east},    {10, 1, D::east},  {11, 1, D::north},
                       {11, 0, D::east},   {12, 0, D::east},   {13, 0, D::east},  {14, 0, D::east},
                       {15, 0, D::east},   {7, 6, D::east},    {8, 6, D::east},   {9, 6, D::east},
                       {10, 6, D::east},   {11, 6, D::east},   {12, 6, D::east},  {13, 6, D::east},
                       {14, 6, D::east},   {2, 10, D::east},   {3, 10, D::east},  {4, 10, D::east},
                       {5, 10, D::east},   {6, 10, D::east}},
                      true);
        // Back to the lift, and down to level 4.
        walk_passages(
            route, 48,
            {{7, 10, D::west},  {6, 10, D::west},   {5, 10, D::west},   {4, 10, D::west},
             {3, 10, D::west},  {2, 10, D::west},   {14, 6, D::west},   {13, 6, D::west},
             {12, 6, D::west},  {11, 6, D::west},   {10, 6, D::west},   {9, 6, D::west},
             {8, 6, D::west},   {7, 6, D::west},    {15, 0, D::west},   {14, 0, D::west},
             {13, 0, D::west},  {12, 0, D::west},   {11, 0, D::south},  {11, 1, D::west},
             {10, 1, D::west},  {9, 1, D::west},    {8, 1, D::west},    {7, 1, D::west},
             {6, 1, D::west},   {5, 1, D::west},    {13, 11, D::west},  {12, 11, D::south},
             {12, 12, D::west}, {11, 12, D::south}, {11, 13, D::south}, {14, 3, D::south},
             {14, 4, D::south}, {14, 5, D::east},   {15, 5, D::south},  {15, 6, D::south},
             {15, 7, D::west},  {14, 7, D::south},  {14, 8, D::south},  {14, 9, D::south},
             {3, 1, D::south},  {3, 2, D::south}},
            true);
        route.push_back({48, 3, 3, D::north, {"YES", "4"}, true});
        // Level 4: a pouch on the ground holds the fourth piece.
        walk_passages(route, 48,
                      {{3, 3, D::south},  {3, 4, D::south},  {3, 5, D::south},  {1, 5, D::south},
                       {1, 6, D::south},  {1, 7, D::south},  {1, 8, D::east},   {2, 8, D::east},
                       {3, 8, D::east},   {4, 8, D::east},   {5, 8, D::south},  {5, 9, D::south},
                       {10, 7, D::south}, {10, 8, D::west},  {9, 8, D::west},   {8, 8, D::west},
                       {7, 8, D::west},   {6, 8, D::south},  {6, 9, D::south},  {1, 5, D::south},
                       {1, 6, D::south},  {1, 7, D::south},  {1, 8, D::east},   {2, 8, D::east},
                       {3, 8, D::east},   {4, 8, D::east},   {5, 8, D::south},  {5, 9, D::south},
                       {11, 11, D::east}, {12, 11, D::east}, {13, 11, D::east}, {5, 0, D::east},
                       {6, 0, D::east},   {7, 0, D::east},   {8, 0, D::east},   {9, 0, D::east},
                       {10, 0, D::east}},
                      true);
        // Back to the lift, and down to level 5.
        walk_passages(route, 48,
                      {{8, 11, D::west},  {10, 0, D::west},   {9, 0, D::west},  {8, 0, D::west},
                       {7, 0, D::west},   {6, 0, D::west},    {5, 0, D::west},  {13, 11, D::west},
                       {12, 11, D::west}, {11, 11, D::north}, {5, 9, D::north}, {5, 8, D::west},
                       {4, 8, D::west},   {3, 8, D::west},    {2, 8, D::west},  {1, 8, D::north},
                       {1, 7, D::north},  {1, 6, D::north},   {1, 5, D::north}, {6, 9, D::north},
                       {6, 8, D::east},   {7, 8, D::east},    {8, 8, D::east},  {9, 8, D::east},
                       {10, 8, D::north}, {10, 7, D::north},  {5, 9, D::north}, {5, 8, D::west},
                       {4, 8, D::west},   {3, 8, D::west},    {2, 8, D::west},  {1, 8, D::north},
                       {1, 7, D::north},  {1, 6, D::north},   {1, 5, D::north}, {3, 5, D::north},
                       {3, 4, D::north}},
                      true);
        route.push_back({48, 3, 3, D::north, {"YES", "5"}, true});
        // Level 5 (script 50): the fifth piece.
        walk_passages(
            route, 48,
            {{3, 3, D::north},   {3, 2, D::north},   {3, 1, D::north},   {8, 15, D::north},
             {8, 14, D::west},   {7, 14, D::north},  {7, 13, D::west},   {6, 13, D::north},
             {6, 12, D::west},   {5, 12, D::west},   {10, 5, D::west},   {9, 5, D::west},
             {8, 5, D::west},    {9, 7, D::west},    {8, 7, D::west},    {7, 7, D::west},
             {6, 7, D::north},   {6, 6, D::north},   {10, 13, D::north}, {10, 12, D::north},
             {10, 11, D::west},  {9, 11, D::west},   {8, 11, D::west},   {15, 0, D::west},
             {14, 0, D::west},   {13, 0, D::west},   {12, 0, D::west},   {11, 0, D::south},
             {11, 1, D::west},   {10, 1, D::west},   {9, 1, D::west},    {8, 1, D::west},
             {7, 1, D::west},    {6, 1, D::west},    {5, 1, D::west},    {9, 7, D::west},
             {8, 7, D::west},    {7, 7, D::west},    {6, 7, D::north},   {6, 6, D::north},
             {11, 13, D::north}, {11, 12, D::north}, {11, 11, D::north}, {1, 15, D::north},
             {1, 14, D::east},   {2, 14, D::north},  {2, 13, D::east},   {3, 13, D::north},
             {3, 12, D::east},   {4, 12, D::east},   {11, 14, D::east},  {12, 14, D::north},
             {12, 13, D::east},  {13, 13, D::north}, {13, 12, D::east}},
            true);
        // Back to the lift, and down to level 6.
        walk_passages(
            route, 48,
            {{14, 12, D::west},  {13, 12, D::south}, {13, 13, D::west},  {12, 13, D::south},
             {12, 14, D::west},  {11, 14, D::west},  {4, 12, D::west},   {3, 12, D::south},
             {3, 13, D::west},   {2, 13, D::south},  {2, 14, D::west},   {1, 14, D::south},
             {1, 15, D::south},  {11, 11, D::south}, {11, 12, D::south}, {11, 13, D::south},
             {6, 6, D::south},   {6, 7, D::east},    {7, 7, D::east},    {8, 7, D::east},
             {9, 7, D::east},    {5, 1, D::east},    {6, 1, D::east},    {7, 1, D::east},
             {8, 1, D::east},    {9, 1, D::east},    {10, 1, D::east},   {11, 1, D::north},
             {11, 0, D::east},   {12, 0, D::east},   {13, 0, D::east},   {14, 0, D::east},
             {15, 0, D::east},   {8, 11, D::east},   {9, 11, D::east},   {10, 11, D::south},
             {10, 12, D::south}, {10, 13, D::south}, {6, 6, D::south},   {6, 7, D::east},
             {7, 7, D::east},    {8, 7, D::east},    {9, 7, D::east},    {8, 5, D::east},
             {9, 5, D::east},    {10, 5, D::east},   {5, 12, D::east},   {6, 12, D::south},
             {6, 13, D::east},   {7, 13, D::south},  {7, 14, D::east},   {8, 14, D::south},
             {8, 15, D::south},  {3, 1, D::south},   {3, 2, D::south}},
            true);
        route.push_back({48, 3, 3, D::north, {"YES", "6"}, true});
        // Level 6: the sixth piece.
        walk_passages(route, 48,
                      {{3, 3, D::west},   {2, 3, D::west},   {1, 3, D::west},   {10, 0, D::west},
                       {9, 0, D::west},   {8, 0, D::west},   {7, 0, D::west},   {6, 0, D::west},
                       {5, 0, D::west},   {5, 15, D::west},  {4, 15, D::west},  {3, 15, D::west},
                       {2, 15, D::west},  {15, 0, D::west},  {14, 0, D::west},  {13, 0, D::west},
                       {12, 0, D::west},  {11, 0, D::south}, {11, 1, D::west},  {10, 1, D::west},
                       {9, 1, D::west},   {8, 1, D::west},   {7, 1, D::west},   {6, 1, D::west},
                       {12, 7, D::west},  {11, 7, D::west},  {10, 7, D::south}, {10, 8, D::west},
                       {9, 8, D::west},   {8, 8, D::west},   {7, 8, D::west},   {6, 8, D::south},
                       {6, 9, D::south},  {14, 3, D::south}, {14, 4, D::south}, {14, 5, D::east},
                       {15, 5, D::south}, {15, 6, D::south}, {15, 7, D::west},  {14, 7, D::south},
                       {14, 8, D::south}, {14, 9, D::south}, {5, 13, D::south}, {5, 14, D::east},
                       {6, 14, D::south}, {6, 15, D::east},  {7, 15, D::east},  {9, 15, D::east},
                       {10, 15, D::east}, {11, 15, D::east}, {12, 15, D::east}, {13, 15, D::north},
                       {13, 14, D::east}},
                      true);
        // Back to the lift, and down to level 7.
        walk_passages(route, 48,
                      {{14, 14, D::west}, {13, 14, D::south}, {13, 15, D::west}, {12, 15, D::west},
                       {11, 15, D::west}, {10, 15, D::west},  {9, 15, D::west},  {7, 15, D::west},
                       {6, 15, D::north}, {6, 14, D::west},   {5, 14, D::north}, {5, 13, D::north},
                       {14, 9, D::north}, {14, 8, D::north},  {14, 7, D::east},  {15, 7, D::north},
                       {15, 6, D::north}, {15, 5, D::west},   {14, 5, D::north}, {14, 4, D::north},
                       {14, 3, D::north}, {6, 9, D::north},   {6, 8, D::east},   {7, 8, D::east},
                       {8, 8, D::east},   {9, 8, D::east},    {10, 8, D::north}, {10, 7, D::east},
                       {11, 7, D::east},  {12, 7, D::east},   {5, 1, D::east},   {6, 1, D::east},
                       {7, 1, D::east},   {8, 1, D::east},    {9, 1, D::east},   {10, 1, D::east},
                       {11, 1, D::north}, {11, 0, D::east},   {12, 0, D::east},  {13, 0, D::east},
                       {14, 0, D::east},  {15, 0, D::east},   {2, 15, D::east},  {3, 15, D::east},
                       {4, 15, D::east},  {5, 15, D::east},   {5, 0, D::east},   {6, 0, D::east},
                       {7, 0, D::east},   {8, 0, D::east},    {9, 0, D::east},   {10, 0, D::east},
                       {1, 3, D::east},   {2, 3, D::east}},
                      true);
        route.push_back({48, 3, 3, D::north, {"YES", "7"}, true});
        // Level 7: the seventh piece.
        walk_passages(
            route, 48,
            {{3, 3, D::south},   {3, 4, D::south},   {3, 5, D::south},   {10, 7, D::south},
             {10, 8, D::south},  {10, 9, D::south},  {10, 10, D::east},  {11, 10, D::east},
             {12, 10, D::east},  {5, 12, D::east},   {6, 12, D::south},  {6, 13, D::east},
             {7, 13, D::south},  {7, 14, D::east},   {8, 14, D::south},  {8, 15, D::south},
             {11, 11, D::south}, {11, 12, D::south}, {11, 13, D::south}, {0, 5, D::south},
             {0, 6, D::south},   {0, 7, D::south},   {0, 8, D::south},   {0, 9, D::south},
             {0, 10, D::south},  {9, 3, D::south},   {9, 4, D::south},   {9, 5, D::west},
             {8, 5, D::west},    {4, 12, D::west},   {3, 12, D::south},  {3, 13, D::west},
             {2, 13, D::south},  {2, 14, D::west},   {1, 14, D::south},  {1, 15, D::south},
             {15, 8, D::south},  {15, 9, D::south},  {15, 10, D::west},  {14, 10, D::west},
             {13, 10, D::north}, {13, 9, D::west},   {12, 9, D::west},   {11, 9, D::west},
             {9, 7, D::west},    {8, 7, D::west},    {7, 7, D::west},    {6, 7, D::north},
             {6, 6, D::north},   {15, 4, D::north},  {15, 3, D::north},  {15, 2, D::west},
             {14, 2, D::west}},
            true);
        // Back to the lift, and down to level 8.
        walk_passages(
            route, 48,
            {{13, 2, D::east},   {14, 2, D::east},   {15, 2, D::south}, {15, 3, D::south},
             {15, 4, D::south},  {6, 6, D::south},   {6, 7, D::east},   {7, 7, D::east},
             {8, 7, D::east},    {9, 7, D::east},    {11, 9, D::east},  {12, 9, D::east},
             {13, 9, D::south},  {13, 10, D::east},  {14, 10, D::east}, {15, 10, D::north},
             {15, 9, D::north},  {15, 8, D::north},  {1, 15, D::north}, {1, 14, D::east},
             {2, 14, D::north},  {2, 13, D::east},   {3, 13, D::north}, {3, 12, D::east},
             {4, 12, D::east},   {8, 5, D::east},    {9, 5, D::north},  {9, 4, D::north},
             {9, 3, D::north},   {0, 10, D::north},  {0, 9, D::north},  {0, 8, D::north},
             {0, 7, D::north},   {0, 6, D::north},   {0, 5, D::north},  {11, 13, D::north},
             {11, 12, D::north}, {11, 11, D::north}, {8, 15, D::north}, {8, 14, D::west},
             {7, 14, D::north},  {7, 13, D::west},   {6, 13, D::north}, {6, 12, D::west},
             {5, 12, D::west},   {12, 10, D::west},  {11, 10, D::west}, {10, 10, D::north},
             {10, 9, D::north},  {10, 8, D::north},  {10, 7, D::north}, {3, 5, D::north},
             {3, 4, D::north}},
            true);
        route.push_back({48, 3, 3, D::north, {"YES", "8"}, true});
        // Level 8: the Tomb of Oswulf, which the Ring of Tyr opens, holds the last piece.
        walk_passages(route, 48,
                      {{3, 3, D::south},  {3, 4, D::south},  {3, 5, D::south},  {14, 3, D::south},
                       {14, 4, D::south}, {14, 5, D::east},  {15, 5, D::south}, {15, 6, D::south},
                       {15, 7, D::west},  {14, 7, D::south}, {14, 8, D::south}, {14, 9, D::south},
                       {9, 3, D::south},  {9, 4, D::south},  {9, 5, D::west},   {8, 5, D::west},
                       {4, 12, D::west},  {3, 12, D::south}, {3, 13, D::west},  {2, 13, D::south},
                       {2, 14, D::west},  {1, 14, D::south}, {1, 15, D::south}, {0, 11, D::south},
                       {0, 12, D::south}, {0, 13, D::south}, {0, 14, D::south}, {0, 15, D::south},
                       {10, 7, D::south}, {10, 8, D::south}, {10, 9, D::south}, {10, 10, D::east},
                       {11, 10, D::east}, {12, 10, D::east}, {12, 1, D::east},  {13, 1, D::east},
                       {14, 1, D::east},  {15, 1, D::east},  {12, 1, D::east},  {13, 1, D::east},
                       {14, 1, D::east},  {15, 1, D::east},  {2, 10, D::east},  {3, 10, D::east},
                       {4, 10, D::east},  {5, 10, D::east},  {6, 10, D::east}},
                      true);
        // Back to the lift, and up to the temple level.
        walk_passages(route, 48,
                      {{7, 10, D::west},   {6, 10, D::west},  {5, 10, D::west},  {4, 10, D::west},
                       {3, 10, D::west},   {2, 10, D::west},  {15, 1, D::west},  {14, 1, D::west},
                       {13, 1, D::west},   {12, 1, D::west},  {15, 1, D::west},  {14, 1, D::west},
                       {13, 1, D::west},   {12, 1, D::west},  {12, 10, D::west}, {11, 10, D::west},
                       {10, 10, D::north}, {10, 9, D::north}, {10, 8, D::north}, {10, 7, D::north},
                       {0, 15, D::north},  {0, 14, D::north}, {0, 13, D::north}, {0, 12, D::north},
                       {0, 11, D::north},  {1, 15, D::north}, {1, 14, D::east},  {2, 14, D::north},
                       {2, 13, D::east},   {3, 13, D::north}, {3, 12, D::east},  {4, 12, D::east},
                       {8, 5, D::east},    {9, 5, D::north},  {9, 4, D::north},  {9, 3, D::north},
                       {14, 9, D::north},  {14, 8, D::north}, {14, 7, D::east},  {15, 7, D::north},
                       {15, 6, D::north},  {15, 5, D::west},  {14, 5, D::north}, {14, 4, D::north},
                       {14, 3, D::north},  {3, 5, D::north},  {3, 4, D::north}},
                      true);
        route.push_back({48, 3, 3, D::north, {"YES", "TEMPLE"}, true});
        // The temple level again: Derf mends the staff from its eight pieces
        // (cell 4C86); then back to the lift and down to level 8.
        route.push_back(step_beat(49, 3, 6, D::south));
        route.push_back(step_beat(49, 5, 15, D::south, {"ENTER"}));
        auto staff = step_beat(50, 14, 3, D::north);
        staff.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x4C86, 1};
        route.push_back(staff);
        route.push_back(step_beat(50, 8, 0, D::north));
        route.push_back(step_beat(49, 5, 12, D::north));
        route.push_back({49, 3, 3, D::north, {"YES", "8"}, true});
        // Level 8's malfunctioning teleporter, which hurts the party and drops it on level 9 among
        // umber hulks.
        walk_passages(route, 48,
                      {{3, 3, D::north},   {3, 2, D::north},  {3, 1, D::north},  {11, 8, D::east},
                       {12, 8, D::east},   {13, 8, D::north}, {13, 7, D::north}, {13, 6, D::west},
                       {12, 6, D::west},   {11, 6, D::west},  {10, 6, D::west},  {9, 6, D::west},
                       {8, 6, D::west},    {7, 6, D::west},   {12, 10, D::west}, {11, 10, D::west},
                       {10, 10, D::north}, {10, 9, D::north}, {10, 8, D::west},  {9, 8, D::west},
                       {8, 8, D::west},    {7, 8, D::west},   {6, 8, D::south},  {6, 9, D::south},
                       {6, 3, D::south},   {6, 4, D::west},   {5, 4, D::west},   {4, 4, D::south},
                       {4, 5, D::south},   {4, 6, D::west},   {3, 6, D::west},   {2, 6, D::north},
                       {2, 5, D::north},   {2, 4, D::west},   {1, 4, D::west},   {0, 4, D::west},
                       {10, 0, D::west},   {9, 0, D::west},   {8, 0, D::west},   {7, 0, D::west},
                       {6, 0, D::west},    {5, 0, D::west},   {7, 15, D::west},  {6, 15, D::north},
                       {6, 14, D::west},   {5, 14, D::north}, {5, 13, D::north}, {0, 10, D::north},
                       {0, 9, D::north},   {0, 8, D::north},  {0, 7, D::north},  {0, 6, D::north},
                       {0, 5, D::north},   {0, 15, D::north}, {0, 14, D::north}, {0, 13, D::north},
                       {0, 12, D::north},  {0, 11, D::north}, {0, 3, D::north},  {0, 2, D::east},
                       {1, 2, D::east},    {2, 2, D::north},  {2, 1, D::north},  {2, 0, D::east},
                       {3, 0, D::east},    {4, 0, D::south},  {4, 1, D::south},  {4, 2, D::east},
                       {5, 2, D::east},    {6, 2, D::east},   {7, 2, D::east},   {8, 2, D::east},
                       {9, 2, D::east}},
                      true);
        // Level 9 (script 51): the Black Circle's force wall across the lift's shaft is taken down
        // (cell 4C70). Rested before the teleporter, whose hulks give no time to recover.
        route.back().rest_first = true;
        // Rested first, its dead raised: the teleporter and the hulks hurt.
        const auto level_9 = route.size();
        walk_passages(
            route, 49,
            {{13, 3, D::south},  {13, 4, D::west},  {12, 4, D::west},   {11, 4, D::west},
             {10, 4, D::west},   {6, 4, D::west},   {5, 4, D::west},    {4, 4, D::south},
             {4, 5, D::south},   {4, 6, D::south},  {13, 12, D::south}, {13, 13, D::west},
             {12, 13, D::south}, {12, 14, D::west}, {11, 14, D::west},  {10, 0, D::west},
             {9, 0, D::west},    {8, 0, D::west},   {7, 0, D::west},    {6, 0, D::west},
             {5, 0, D::west},    {4, 0, D::south},  {4, 1, D::south},   {4, 2, D::east},
             {5, 2, D::east},    {6, 2, D::east},   {7, 9, D::east},    {8, 9, D::east},
             {9, 9, D::east},    {10, 9, D::east},  {11, 9, D::south},  {11, 10, D::south},
             {11, 11, D::south}, {0, 5, D::south},  {0, 6, D::south},   {0, 7, D::south},
             {0, 8, D::south},   {0, 9, D::south},  {0, 10, D::south}},
            true);
        route[level_9].rest_first = true;
        // To the lift, and down to level 10.
        walk_passages(route, 49, {{3, 0, D::south}, {3, 1, D::south}, {3, 2, D::south}}, true);
        // Up to the temple level, where the Temple of Tyr is safe to rest in
        // (the lift is not: its rests are interrupted), and down to level 10.
        route.push_back({49, 3, 3, D::north, {"YES", "TEMPLE"}});
        route.push_back(step_beat(49, 3, 6, D::south));
        route.push_back(step_beat(49, 5, 15, D::south, {"ENTER"}));
        route.push_back({50, 8, 0, D::north, {}, true});
        route.push_back(step_beat(50, 8, 0, D::north));
        route.push_back(step_beat(49, 5, 12, D::north));
        route.push_back({49, 3, 3, D::north, {"YES", "B"}});
        // Level 10: through the hole the miners broke into the castle's dungeon
        // (script 64). Its patrols of cockatrices, medusae and basilisks turn the
        // party to stone: the party waits for them to go where it may, and where
        // they attack with every dice the route tries the test wins the fight
        // its player loses (a stand-in).
        walk_passages(route, 49,
                      {{3, 3, D::east},   {4, 3, D::east},   {5, 3, D::east},    {6, 3, D::east},
                       {11, 0, D::east},  {12, 0, D::east},  {13, 0, D::east},   {14, 0, D::east},
                       {15, 0, D::east},  {11, 0, D::east},  {12, 0, D::east},   {13, 0, D::east},
                       {14, 0, D::east},  {15, 0, D::east},  {7, 2, D::east},    {8, 2, D::east},
                       {9, 2, D::east},   {10, 2, D::east},  {11, 2, D::east},   {12, 2, D::east},
                       {13, 2, D::east},  {9, 1, D::east},   {10, 1, D::east},   {11, 1, D::east},
                       {12, 1, D::east},  {13, 1, D::east},  {3, 11, D::east},   {4, 11, D::east},
                       {5, 11, D::east},  {6, 11, D::east},  {7, 11, D::south},  {7, 12, D::south},
                       {0, 11, D::south}, {0, 12, D::south}, {0, 13, D::south},  {0, 14, D::south},
                       {0, 15, D::south}, {1, 5, D::south},  {1, 6, D::south},   {1, 7, D::south},
                       {1, 8, D::east},   {2, 8, D::east},   {3, 8, D::east},    {4, 8, D::east},
                       {5, 8, D::south},  {5, 9, D::south},  {1, 5, D::south},   {1, 6, D::south},
                       {1, 7, D::south},  {1, 8, D::east},   {2, 8, D::east},    {3, 8, D::east},
                       {4, 8, D::east},   {5, 8, D::south},  {5, 9, D::south},   {0, 11, D::south},
                       {0, 12, D::south}, {0, 13, D::south}, {0, 14, D::south},  {0, 15, D::south},
                       {6, 12, D::south}, {6, 13, D::east},  {7, 13, D::south},  {7, 14, D::east},
                       {8, 14, D::south}, {8, 15, D::east},  {9, 15, D::east},   {10, 15, D::east},
                       {11, 15, D::east}, {12, 15, D::east}, {13, 15, D::north}, {13, 14, D::east},
                       {14, 14, D::east}, {15, 14, D::east}},
                      true, true, true);
        // The Dungeon (map 64; levels 10 to 7 on one map, the level in cell
        // 4C87). Level 10's patrols leave members turned to stone, which only
        // Stone to Flesh cures: back by the Dungeon's teleporter (location 7)
        // to the Well and the Temple of Gond.
        // To the teleporter.
        walk_passages(route, 64,
                      {{0, 15, D::north}, {0, 14, D::north}, {0, 13, D::north}, {0, 12, D::east},
                       {1, 12, D::east},  {2, 12, D::east},  {3, 12, D::north}, {3, 11, D::north},
                       {3, 10, D::north}, {3, 9, D::north},  {3, 8, D::north},  {3, 7, D::west},
                       {2, 7, D::west},   {1, 7, D::west},   {0, 7, D::north},  {0, 6, D::north},
                       {0, 5, D::east},   {1, 5, D::east},   {2, 5, D::south},  {2, 6, D::east}},
                      true);
        route.push_back({64, 3, 6, D::north, {"YES"}});
        // At the Well (building 15): rested, then by its gate to the mayor's
        // house, where the Temple of Gond raises the dead and turns statues
        // back to flesh, and back by the mayor's teleporter and the Well's
        // gate 15 to the Dungeon (3,6), rested there.
        route.push_back({33, 1, 1, D::north, {"YES"}, true});
        route.push_back({16, 10, 9, D::north, {"YES", "raise:dead", "stone:flesh", "Leave"}});
        route.push_back({16, 3, 0, D::north, {"YES"}});
        route.push_back({33, 8, 14, D::north, {"YES"}});
        route.push_back({64, 3, 6, D::north, {"NO"}, true});
        // The Brass Key, on level 7, which the Amulet of Eldamar reveals (cell 4CDE); the spirit's
        // riddles are answered as the cluebook answers them.
        walk_passages(
            route, 64,
            {{3, 6, D::west},    {2, 6, D::north},   {2, 5, D::west},    {1, 5, D::west},
             {0, 5, D::north},   {0, 4, D::north},   {0, 3, D::north},   {0, 2, D::north},
             {0, 1, D::north},   {0, 0, D::east},    {1, 0, D::east},    {2, 0, D::east},
             {3, 0, D::north},   {4, 0, D::east},    {5, 0, D::south},   {5, 1, D::south},
             {5, 2, D::south},   {5, 3, D::south},   {5, 4, D::south},   {5, 5, D::south},
             {5, 6, D::south},   {5, 7, D::west},    {4, 7, D::south},   {4, 8, D::south},
             {4, 9, D::east},    {5, 9, D::north},   {5, 8, D::east},    {6, 8, D::east},
             {7, 8, D::south},   {7, 9, D::south},   {7, 10, D::south},  {7, 11, D::south},
             {7, 12, D::west},   {6, 12, D::west},   {5, 12, D::north},  {5, 11, D::west},
             {4, 11, D::south},  {4, 12, D::south},  {4, 13, D::south},  {4, 14, D::south},
             {4, 15, D::east},   {5, 15, D::east},   {6, 15, D::north},  {6, 14, D::north},
             {6, 13, D::east},   {7, 13, D::south},  {7, 14, D::south},  {7, 15, D::south},
             {8, 15, D::east},   {9, 15, D::north},  {9, 14, D::north},  {9, 13, D::east},
             {10, 13, D::north}, {10, 12, D::north}, {10, 11, D::north}, {10, 10, D::east},
             {11, 10, D::north}, {11, 9, D::north},  {11, 8, D::north},  {11, 7, D::north},
             {11, 6, D::west},   {10, 6, D::south},  {10, 7, D::west},   {9, 7, D::north},
             {9, 6, D::north},   {9, 5, D::east},    {10, 5, D::east},   {11, 5, D::north},
             {11, 4, D::north},  {11, 3, D::north},  {11, 2, D::north},  {11, 1, D::north},
             {11, 0, D::north},  {15, 7, D::west},   {14, 7, D::south},  {14, 8, D::south},
             {14, 9, D::south},  {14, 10, D::south}, {14, 11, D::south}, {14, 12, D::south},
             {14, 13, D::south}, {14, 14, D::south}, {14, 15, D::west},  {13, 15, D::north},
             {13, 14, D::west},  {12, 14, D::south}},
            true);
        // Up to levels 6 to 3 (script 65): the pyro-hydras guard the Silver Key (cell 4CDF).
        walk_passages(
            route, 64,
            {{12, 15, D::north}, {12, 14, D::east},  {13, 14, D::south}, {13, 15, D::east},
             {14, 15, D::north}, {14, 14, D::north}, {14, 13, D::north}, {14, 12, D::north},
             {14, 11, D::north}, {14, 10, D::north}, {14, 9, D::north},  {14, 8, D::west},
             {13, 8, D::north},  {13, 7, D::north},  {13, 6, D::east},   {14, 6, D::east},
             {15, 6, D::north},  {15, 5, D::north},  {15, 4, D::north},  {15, 3, D::north},
             {15, 2, D::west},   {14, 2, D::north},  {14, 1, D::north},  {14, 0, D::west},
             {13, 0, D::south},  {13, 1, D::south},  {13, 2, D::south},  {13, 3, D::south},
             {13, 4, D::south},  {13, 5, D::west},   {12, 5, D::north},  {12, 4, D::north},
             {12, 3, D::north},  {12, 2, D::north},  {12, 1, D::north},  {12, 0, D::north}},
            true);
        walk_passages(
            route, 65,
            {{1, 10, D::west},   {0, 10, D::south},  {0, 11, D::south}, {0, 12, D::south},
             {0, 13, D::south},  {0, 14, D::south},  {0, 15, D::east},  {1, 15, D::north},
             {1, 14, D::east},   {2, 14, D::south},  {2, 15, D::east},  {3, 15, D::north},
             {3, 14, D::north},  {3, 13, D::east},   {4, 13, D::east},  {5, 13, D::north},
             {5, 12, D::north},  {5, 11, D::north},  {5, 10, D::east},  {6, 10, D::east},
             {7, 10, D::east},   {15, 10, D::south}, {15, 11, D::west}, {14, 11, D::north},
             {14, 10, D::west},  {13, 10, D::south}, {13, 11, D::west}, {12, 11, D::south},
             {12, 12, D::south}, {12, 13, D::west},  {11, 13, D::west}, {10, 13, D::west},
             {9, 13, D::west},   {8, 13, D::south},  {8, 14, D::south}, {8, 15, D::south},
             {4, 4, D::west},    {3, 4, D::north},   {3, 3, D::east},   {4, 3, D::east},
             {5, 3, D::south},   {5, 4, D::east},    {6, 4, D::east},   {7, 4, D::north},
             {7, 3, D::north},   {7, 2, D::north},   {7, 1, D::north},  {7, 0, D::west},
             {6, 0, D::west},    {5, 0, D::west},    {4, 0, D::west},   {3, 0, D::west},
             {2, 0, D::south},   {2, 1, D::south},   {2, 2, D::west},   {1, 2, D::west},
             {0, 2, D::south},   {0, 3, D::south},   {0, 4, D::south},  {0, 5, D::east},
             {1, 5, D::east},    {2, 5, D::south},   {2, 6, D::south},  {2, 7, D::south},
             {2, 8, D::west}},
            true);
        // Up to levels 2 and 1 (script 66): the purple worms guard the Gold Key (cell 4CE0).
        walk_passages(route, 65,
                      {{1, 8, D::east},   {2, 8, D::south},  {2, 9, D::east},   {3, 9, D::east},
                       {5, 6, D::south},  {5, 7, D::west},   {4, 7, D::north},  {4, 6, D::north},
                       {4, 5, D::east},   {5, 5, D::east},   {6, 5, D::east},   {7, 5, D::south},
                       {7, 6, D::east},   {8, 6, D::east},   {9, 6, D::east},   {10, 6, D::north},
                       {10, 5, D::north}, {10, 4, D::north}, {10, 3, D::east},  {11, 3, D::north},
                       {11, 2, D::north}, {11, 1, D::west},  {10, 1, D::north}, {10, 0, D::east},
                       {11, 0, D::east},  {12, 0, D::east},  {13, 0, D::south}, {13, 1, D::south},
                       {13, 2, D::south}, {13, 3, D::east},  {14, 3, D::north}, {14, 2, D::north},
                       {14, 1, D::east},  {15, 1, D::south}, {15, 2, D::south}, {15, 3, D::south},
                       {15, 4, D::south}, {15, 5, D::south}, {15, 6, D::south}, {15, 7, D::south},
                       {15, 8, D::west},  {14, 8, D::south}, {14, 9, D::west},  {13, 9, D::west}},
                      true);
        walk_passages(route, 66,
                      {{0, 7, D::north},   {0, 6, D::east},   {1, 6, D::north},  {1, 5, D::west},
                       {0, 5, D::north},   {0, 4, D::east},   {1, 4, D::east},   {2, 4, D::east},
                       {3, 4, D::east},    {4, 4, D::north},  {4, 3, D::east},   {5, 3, D::east},
                       {6, 3, D::south},   {6, 4, D::east},   {7, 4, D::east},   {8, 4, D::east},
                       {9, 4, D::east},    {10, 4, D::east},  {11, 4, D::east},  {12, 4, D::east},
                       {13, 4, D::east},   {14, 4, D::south}, {14, 5, D::south}, {14, 6, D::south},
                       {14, 7, D::east},   {15, 7, D::north}, {15, 6, D::north}, {15, 5, D::north},
                       {15, 4, D::north},  {15, 3, D::north}, {15, 2, D::north}, {15, 1, D::north},
                       {15, 0, D::north},  {15, 8, D::west},  {14, 8, D::south}, {14, 9, D::west},
                       {13, 9, D::west},   {12, 9, D::west},  {11, 9, D::west},  {10, 9, D::south},
                       {10, 10, D::south}, {10, 11, D::west}, {9, 11, D::west},  {8, 11, D::west},
                       {7, 11, D::south},  {7, 12, D::south}, {7, 13, D::west},  {6, 13, D::west},
                       {5, 13, D::west},   {4, 13, D::south}, {4, 14, D::south}, {4, 15, D::west},
                       {3, 15, D::north},  {3, 14, D::north}, {3, 13, D::west},  {2, 13, D::west},
                       {1, 13, D::north},  {1, 12, D::north}, {1, 11, D::north}, {1, 10, D::north},
                       {1, 9, D::north},   {1, 8, D::east},   {2, 8, D::east},   {3, 8, D::south},
                       {3, 9, D::south},   {3, 10, D::west},  {2, 10, D::south}},
                      true);
        // Out of the Dungeon's top into the Western Crevasses (script 80).
        walk_passages(route, 66,
                      {{2, 11, D::north},
                       {2, 10, D::north},
                       {2, 9, D::east},
                       {3, 9, D::north},
                       {3, 8, D::west},
                       {2, 8, D::west},
                       {1, 8, D::west},
                       {0, 8, D::south},
                       {0, 9, D::south},
                       {0, 10, D::south},
                       {0, 11, D::south},
                       {0, 12, D::south},
                       {0, 13, D::south},
                       {0, 14, D::east},
                       {1, 14, D::east},
                       {2, 14, D::south},
                       {2, 15, D::south}},
                      true);
        // The Western Crevasses (script 80; its places told apart by cells 4C05
        // and 4C6B) to the Frost Giant Village (script 81). Its patrols of the
        // Dreadlord's legion beat the route's party with every dice it tries:
        // there the test wins the fight its player loses (a stand-in).
        walk_passages(
            route, 80,
            {{1, 11, D::east},   {2, 11, D::east},   {3, 11, D::north},  {3, 10, D::east},
             {4, 10, D::north},  {4, 9, D::east},    {5, 9, D::east},    {1, 7, D::east},
             {2, 7, D::east},    {3, 7, D::east},    {4, 7, D::east},    {5, 7, D::east},
             {6, 7, D::east},    {7, 7, D::east},    {8, 7, D::east},    {9, 7, D::east},
             {10, 7, D::east},   {11, 7, D::east},   {12, 7, D::east},   {13, 7, D::east},
             {14, 7, D::east},   {1, 1, D::east},    {2, 1, D::south},   {2, 2, D::east},
             {3, 2, D::south},   {3, 3, D::east},    {4, 3, D::south},   {4, 4, D::east},
             {5, 4, D::south},   {5, 5, D::east},    {6, 5, D::south},   {6, 6, D::east},
             {7, 6, D::east},    {4, 15, D::east},   {5, 15, D::east},   {6, 15, D::east},
             {7, 15, D::east},   {8, 15, D::east},   {9, 15, D::east},   {1, 14, D::east},
             {2, 14, D::north},  {2, 13, D::east},   {3, 13, D::north},  {3, 12, D::east},
             {4, 12, D::north},  {4, 11, D::east},   {5, 11, D::north},  {5, 10, D::east},
             {6, 10, D::north},  {6, 9, D::east},    {7, 9, D::east},    {2, 15, D::east},
             {3, 15, D::north},  {3, 14, D::east},   {4, 14, D::east},   {5, 14, D::north},
             {5, 13, D::east},   {6, 13, D::east},   {7, 13, D::north},  {7, 12, D::east},
             {8, 12, D::east},   {9, 12, D::north},  {9, 11, D::east},   {10, 11, D::east},
             {11, 11, D::north}, {11, 10, D::east},  {12, 10, D::east},  {13, 10, D::north},
             {13, 9, D::east},   {14, 9, D::east},   {9, 14, D::east},   {10, 14, D::east},
             {11, 14, D::east},  {12, 14, D::east},  {13, 14, D::east},  {14, 14, D::east},
             {4, 0, D::east},    {5, 0, D::east},    {6, 0, D::east},    {7, 0, D::east},
             {8, 0, D::east},    {9, 0, D::east},    {10, 0, D::east},   {11, 0, D::east},
             {12, 0, D::east},   {13, 0, D::east},   {14, 0, D::east},   {12, 4, D::east},
             {13, 4, D::east},   {14, 4, D::east},   {15, 4, D::north},  {15, 3, D::north},
             {15, 2, D::west},   {14, 2, D::north},  {14, 1, D::west},   {13, 1, D::west},
             {12, 1, D::west},   {9, 15, D::west},   {8, 15, D::west},   {7, 15, D::west},
             {6, 15, D::west},   {5, 15, D::west},   {4, 15, D::west},   {7, 6, D::west},
             {6, 6, D::north},   {6, 5, D::west},    {5, 5, D::north},   {5, 4, D::west},
             {4, 4, D::north},   {4, 3, D::west},    {3, 3, D::north},   {3, 2, D::west},
             {2, 2, D::north},   {2, 1, D::west},    {1, 1, D::west},    {2, 6, D::west},
             {1, 6, D::west},    {0, 6, D::north},   {0, 5, D::north},   {0, 4, D::north},
             {0, 3, D::north},   {0, 2, D::north},   {0, 1, D::north},   {0, 0, D::east},
             {1, 0, D::east},    {2, 15, D::east},   {3, 15, D::north},  {3, 14, D::east},
             {4, 14, D::east},   {5, 14, D::north},  {5, 13, D::east},   {6, 13, D::east},
             {7, 13, D::north},  {7, 12, D::east},   {8, 12, D::east},   {9, 12, D::north},
             {9, 11, D::east},   {10, 11, D::east},  {11, 11, D::north}, {11, 10, D::east},
             {12, 10, D::east},  {13, 10, D::north}, {13, 9, D::east},   {14, 9, D::east},
             {4, 0, D::east},    {5, 0, D::east},    {6, 0, D::east},    {7, 0, D::east},
             {8, 0, D::east},    {9, 0, D::east},    {10, 0, D::east},   {11, 0, D::south},
             {11, 1, D::south},  {11, 2, D::east},   {12, 2, D::east},   {13, 2, D::south},
             {13, 3, D::east},   {14, 3, D::east},   {1, 7, D::east},    {2, 7, D::east},
             {3, 7, D::east},    {4, 7, D::east},    {5, 7, D::east},    {6, 7, D::east},
             {7, 7, D::east},    {8, 7, D::east},    {9, 7, D::east},    {10, 7, D::east},
             {11, 7, D::east},   {12, 7, D::east},   {13, 7, D::east},   {14, 7, D::east},
             {1, 8, D::east},    {2, 8, D::east},    {3, 8, D::east},    {4, 8, D::east},
             {5, 8, D::east},    {6, 8, D::east},    {7, 8, D::east},    {8, 8, D::east},
             {9, 8, D::east},    {10, 8, D::east},   {11, 8, D::east},   {12, 8, D::east},
             {13, 8, D::east},   {14, 8, D::east},   {12, 1, D::east},   {13, 1, D::east},
             {14, 1, D::south},  {14, 2, D::east},   {15, 2, D::south},  {15, 3, D::south},
             {15, 4, D::west},   {14, 4, D::west},   {13, 4, D::west},   {12, 4, D::west},
             {3, 9, D::west},    {2, 9, D::west},    {1, 9, D::west},    {0, 9, D::south},
             {0, 10, D::south},  {0, 11, D::south},  {0, 12, D::south},  {0, 13, D::south},
             {0, 14, D::south},  {0, 15, D::east},   {1, 15, D::east},   {5, 3, D::east},
             {6, 3, D::south},   {6, 4, D::east},    {7, 4, D::south},   {7, 5, D::east},
             {8, 5, D::east},    {9, 5, D::east},    {10, 5, D::east},   {12, 11, D::east},
             {13, 11, D::south}, {13, 12, D::south}, {13, 13, D::south}, {13, 14, D::east},
             {14, 14, D::east},  {6, 14, D::east},   {7, 14, D::east},   {8, 14, D::north},
             {8, 13, D::east},   {9, 13, D::east},   {10, 13, D::north}, {10, 12, D::east},
             {11, 12, D::east},  {12, 12, D::east},  {2, 0, D::east},    {3, 0, D::south},
             {3, 1, D::east},    {4, 1, D::east},    {5, 1, D::south},   {5, 2, D::east},
             {6, 2, D::east},    {7, 2, D::south},   {7, 3, D::east},    {8, 3, D::east},
             {9, 3, D::south},   {9, 4, D::east},    {10, 4, D::east},   {11, 4, D::south},
             {11, 5, D::east},   {12, 5, D::east},   {13, 5, D::south},  {13, 6, D::east},
             {14, 6, D::east},   {2, 0, D::east},    {3, 0, D::south},   {3, 1, D::east},
             {4, 1, D::east},    {5, 1, D::south},   {5, 2, D::east},    {6, 2, D::east},
             {7, 2, D::south},   {7, 3, D::east},    {8, 3, D::east},    {9, 3, D::south},
             {9, 4, D::east},    {10, 4, D::east},   {11, 4, D::south},  {11, 5, D::east},
             {12, 5, D::east},   {13, 5, D::south},  {13, 6, D::east},   {14, 6, D::east},
             {5, 12, D::east},   {6, 12, D::north},  {6, 11, D::east},   {7, 11, D::north},
             {7, 10, D::east},   {8, 10, D::east},   {9, 10, D::east},   {10, 10, D::east},
             {8, 6, D::east},    {9, 6, D::east},    {10, 6, D::east},   {11, 6, D::east},
             {12, 6, D::east},   {1, 5, D::east},    {2, 5, D::east},    {3, 5, D::east}},
            true, true);
        // The Frost Giant Village (script 81): its blockaded door bashed open, its king won over by
        // fighting the Black Circle beside him, his guards lead the party east, toward the castle,
        // into the Eastern Crevasses (script 82). Fights its player loses with every dice are won
        // by the test (a stand-in).
        walk_passages(route, 81,
                      {{0, 8, D::east},   {1, 8, D::east},   {2, 8, D::east},   {3, 8, D::east},
                       {4, 8, D::east},   {5, 8, D::east},   {6, 8, D::east},   {7, 8, D::east},
                       {8, 8, D::east},   {9, 8, D::south},  {9, 9, D::south},  {9, 10, D::south},
                       {9, 11, D::south}, {9, 12, D::south}, {9, 13, D::south}, {9, 14, D::south},
                       {9, 15, D::east},  {10, 15, D::east}, {11, 15, D::east}, {12, 15, D::north},
                       {2, 15, D::west}},
                      true, true, false, {"BASH", "BASH", "BASH", "CASTLE"});
        // From the mine on, a fight on the way (or in a camp on the way) that
        // the route's player loses with every dice it tries is won by the test
        // (a stand-in): Secret's mines, Dungeon and crevasses meet the party
        // with more and stronger foes than its player can beat each time.
        const auto mine = std::ranges::find_if(route,
                                               [](const RouteBeat &beat)
                                               {
                                                   return beat.map == 48;
                                               });
        for (auto beat = mine; beat != route.end(); ++beat)
        {
            beat->test_wins = beat->test_wins || beat->passing;
        }
        // The Eastern Crevasses (script 82) to the Castle of the Twins' gate, which Oswulf's spirit
        // opens to the bearer of his staff (script 96).
        walk_passages(
            route, 82,
            {{1, 11, D::east},   {2, 11, D::east},   {3, 11, D::north},  {3, 10, D::east},
             {4, 10, D::north},  {4, 9, D::east},    {5, 9, D::east},    {1, 7, D::east},
             {2, 7, D::east},    {3, 7, D::east},    {4, 7, D::east},    {5, 7, D::east},
             {6, 7, D::east},    {7, 7, D::east},    {8, 7, D::east},    {9, 7, D::east},
             {10, 7, D::east},   {11, 7, D::east},   {12, 7, D::east},   {13, 7, D::east},
             {14, 7, D::east},   {6, 1, D::east},    {7, 1, D::east},    {8, 1, D::south},
             {8, 2, D::east},    {9, 2, D::east},    {10, 2, D::south},  {10, 3, D::east},
             {11, 3, D::east},   {12, 3, D::east},   {5, 3, D::east},    {6, 3, D::south},
             {6, 4, D::east},    {7, 4, D::south},   {7, 5, D::east},    {8, 5, D::east},
             {9, 5, D::east},    {10, 5, D::east},   {5, 3, D::east},    {6, 3, D::south},
             {6, 4, D::east},    {7, 4, D::south},   {7, 5, D::east},    {8, 5, D::east},
             {9, 5, D::east},    {10, 5, D::east},   {8, 9, D::east},    {9, 9, D::east},
             {10, 9, D::east},   {11, 9, D::east},   {12, 9, D::east},   {8, 6, D::east},
             {9, 6, D::east},    {10, 6, D::east},   {11, 6, D::east},   {12, 6, D::east},
             {2, 0, D::east},    {3, 0, D::south},   {3, 1, D::east},    {4, 1, D::east},
             {5, 1, D::south},   {5, 2, D::east},    {6, 2, D::east},    {7, 2, D::south},
             {7, 3, D::east},    {8, 3, D::east},    {9, 3, D::south},   {9, 4, D::east},
             {10, 4, D::east},   {11, 4, D::south},  {11, 5, D::east},   {12, 5, D::east},
             {13, 5, D::south},  {13, 6, D::east},   {14, 6, D::east},   {5, 12, D::east},
             {6, 12, D::north},  {6, 11, D::east},   {7, 11, D::north},  {7, 10, D::east},
             {8, 10, D::east},   {9, 10, D::east},   {10, 10, D::east},  {5, 12, D::east},
             {6, 12, D::north},  {6, 11, D::east},   {7, 11, D::north},  {7, 10, D::east},
             {8, 10, D::east},   {9, 10, D::east},   {10, 10, D::east},  {15, 9, D::north},
             {15, 8, D::north},  {15, 7, D::north},  {15, 6, D::north},  {15, 5, D::west},
             {14, 5, D::west},   {2, 6, D::west},    {1, 6, D::west},    {0, 6, D::north},
             {0, 5, D::north},   {0, 4, D::north},   {0, 3, D::north},   {0, 2, D::north},
             {0, 1, D::north},   {0, 0, D::east},    {1, 0, D::east},    {12, 4, D::east},
             {13, 4, D::east},   {14, 4, D::east},   {15, 4, D::north},  {15, 3, D::north},
             {15, 2, D::west},   {14, 2, D::north},  {14, 1, D::west},   {13, 1, D::west},
             {12, 1, D::west},   {1, 15, D::west},   {0, 15, D::north},  {0, 14, D::north},
             {0, 13, D::north},  {0, 12, D::north},  {0, 11, D::north},  {0, 10, D::north},
             {0, 9, D::east},    {1, 9, D::east},    {2, 9, D::east},    {3, 9, D::east},
             {1, 7, D::east},    {2, 7, D::east},    {3, 7, D::east},    {4, 7, D::east},
             {5, 7, D::east},    {6, 7, D::east},    {7, 7, D::east},    {8, 7, D::east},
             {9, 7, D::east},    {10, 7, D::east},   {11, 7, D::east},   {12, 7, D::east},
             {13, 7, D::east},   {14, 7, D::east},   {9, 14, D::east},   {10, 14, D::east},
             {11, 14, D::east},  {12, 14, D::east},  {13, 14, D::east},  {14, 14, D::east},
             {12, 1, D::east},   {13, 1, D::east},   {14, 1, D::south},  {14, 2, D::east},
             {15, 2, D::south},  {15, 3, D::south},  {15, 4, D::west},   {14, 4, D::west},
             {13, 4, D::west},   {12, 4, D::west},   {14, 9, D::west},   {13, 9, D::south},
             {13, 10, D::west},  {12, 10, D::west},  {11, 10, D::south}, {11, 11, D::west},
             {10, 11, D::west},  {9, 11, D::south},  {9, 12, D::west},   {8, 12, D::west},
             {7, 12, D::south},  {7, 13, D::west},   {6, 13, D::west},   {5, 13, D::south},
             {5, 14, D::west},   {4, 14, D::west},   {3, 14, D::south},  {3, 15, D::west},
             {2, 15, D::west},   {1, 0, D::west},    {0, 0, D::south},   {0, 1, D::south},
             {0, 2, D::south},   {0, 3, D::south},   {0, 4, D::south},   {0, 5, D::south},
             {0, 6, D::east},    {1, 6, D::east},    {2, 6, D::east},    {6, 14, D::east},
             {7, 14, D::east},   {8, 14, D::north},  {8, 13, D::east},   {9, 13, D::east},
             {10, 13, D::north}, {10, 12, D::east},  {11, 12, D::east},  {12, 12, D::east},
             {4, 0, D::east},    {5, 0, D::east},    {6, 0, D::east},    {7, 0, D::east},
             {8, 0, D::east},    {9, 0, D::east},    {10, 0, D::east},   {11, 0, D::south},
             {11, 1, D::south},  {11, 2, D::east},   {12, 2, D::east},   {13, 2, D::south},
             {13, 3, D::east},   {14, 3, D::east},   {1, 8, D::east},    {2, 8, D::east},
             {3, 8, D::east},    {4, 8, D::east},    {5, 8, D::east},    {6, 8, D::east},
             {7, 8, D::east},    {8, 8, D::east},    {9, 8, D::east},    {10, 8, D::east},
             {11, 8, D::east},   {12, 8, D::east},   {13, 8, D::east},   {14, 8, D::east},
             {1, 14, D::east},   {2, 14, D::north},  {2, 13, D::east},   {3, 13, D::north},
             {3, 12, D::east},   {4, 12, D::north},  {4, 11, D::east},   {5, 11, D::north},
             {5, 10, D::east},   {6, 10, D::north},  {6, 9, D::east},    {7, 9, D::east},
             {4, 0, D::east},    {5, 0, D::east},    {6, 0, D::east},    {7, 0, D::east},
             {8, 0, D::east},    {9, 0, D::east},    {10, 0, D::east},   {11, 0, D::east},
             {12, 0, D::east},   {13, 0, D::east},   {14, 0, D::east},   {2, 15, D::east},
             {3, 15, D::north},  {3, 14, D::east},   {4, 14, D::east},   {5, 14, D::north},
             {5, 13, D::east},   {6, 13, D::east},   {7, 13, D::north},  {7, 12, D::east},
             {8, 12, D::east},   {9, 12, D::north},  {9, 11, D::east},   {10, 11, D::east},
             {11, 11, D::north}, {11, 10, D::east},  {12, 10, D::east},  {13, 10, D::north},
             {13, 9, D::east},   {14, 9, D::east},   {1, 8, D::east},    {2, 8, D::east},
             {3, 8, D::east},    {4, 8, D::east},    {5, 8, D::east},    {6, 8, D::east},
             {7, 8, D::east},    {8, 8, D::east},    {9, 8, D::east},    {10, 8, D::east},
             {11, 8, D::east},   {12, 8, D::east},   {13, 8, D::east},   {14, 8, D::east},
             {9, 14, D::east},   {10, 14, D::east},  {11, 14, D::east},  {12, 14, D::east},
             {13, 14, D::east},  {14, 14, D::east},  {1, 1, D::east},    {2, 1, D::south},
             {2, 2, D::east},    {3, 2, D::south},   {3, 3, D::east},    {4, 3, D::south},
             {4, 4, D::east},    {5, 4, D::south},   {5, 5, D::east},    {6, 5, D::south},
             {6, 6, D::east},    {7, 6, D::east},    {10, 15, D::east},  {11, 15, D::east},
             {12, 15, D::east},  {13, 15, D::east},  {14, 15, D::east},  {15, 15, D::north},
             {15, 14, D::north}, {15, 13, D::north}, {15, 12, D::north}},
            true, true);
        route.push_back(step_beat(82, 15, 11, D::east, {"YES"}));
        return route;
    }();
    return beats;
}

// Pools of Darkness's route plays to the game's end once this is true; until
// then it stops on this map (docs/GAMES.md, Pools of Darkness, step 5).
constexpr bool darkness_route_ends_the_game = false;
constexpr unsigned darkness_route_stops_on_map = 50;

// Pools of Darkness's story, beat by beat, as its cluebook tells it: from New
// Phlan's docks, where Rolf has walked the party out.
const std::vector<RouteBeat> &darkness_route()
{
    static const std::vector<RouteBeat> beats = []
    {
        using D = opengold::por::MapDirection;
        std::vector<RouteBeat> route;
        // Rolf's tour ends in the park; the party walks west off the docks'
        // map into West Phlan (map 17), which the script loads as the party
        // leaves the edge (cell 21h).
        route.push_back(step_beat(16, 0, 4, D::west));
        // Sasha waits at the west gate (17: 0,4): the party takes her escort
        // duty, and on the hill outside the storm takes Phlan; Elminster
        // meets the party in Limbo and sends it back through the portal.
        route.push_back(step_beat(17, 1, 4, D::west, {"YES"}));
        // Back from Limbo the party stands on the area map at (26,6), outside
        // any dungeon: a step west.
        RouteBeat west{17, 0, 0, D::north, {}};
        west.area_steps = {opengold::por::ExplorationCommand::west};
        west.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x25, 25};
        route.push_back(west);
        // North-west across the Lands of Thar, past Melvaunt's army and the
        // farmhouse, to the stockade of Taydome's Keep (19,1), where Sasha is
        // held (the cluebook's Sasha and the Lands of Thar).
        using C = opengold::por::ExplorationCommand;
        RouteBeat to_keep{17, 0, 0, D::north, {"YES", "WAIT"}};
        to_keep.area_steps = {C::northwest, C::northwest, C::northwest,
                              C::northwest, C::northwest, C::west};
        route.push_back(to_keep);
        // Inside, the party waits while the giants lock the gate, then meets
        // Kimarr and his monsters in the keep's hall (6,4), rested first.
        RouteBeat kimarr{22, 6, 4, D::north, {}};
        kimarr.rest_first = true;
        route.push_back(kimarr);
        // South through the waiting rooms to the meeting room (5,10), where
        // the party beats the ogres and breaks Bostell's chains (cell 128h),
        // so he aids it against Kimarr.
        walk_passages(route, 22,
                      {{6, 4, D::south},
                       {6, 5, D::south},
                       {6, 6, D::west},
                       {5, 6, D::west},
                       {4, 6, D::south},
                       {4, 7, D::south},
                       {4, 8, D::south},
                       {4, 9, D::south},
                       {4, 10, D::south},
                       {4, 11, D::east}});
        auto bostell = step_beat(22, 5, 11, D::north, {"BREAK CHAINS"});
        bostell.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x128, 1};
        route.push_back(bostell);
        // Back north and east through the halls, past the study where Kimarr
        // plots (11,6), to the room where his giants hold Quil (9,5): the
        // party stays, Bostell rushes in, and Kimarr, a blue dragon, falls
        // with his monsters (cell 123h). Quil has broken the great spell; the
        // party goes with him to Sasha, who sends it out to the area map.
        walk_passages(route, 22,
                      {{5, 10, D::south}, {5, 11, D::west},  {4, 11, D::north}, {4, 10, D::north},
                       {4, 9, D::north},  {4, 8, D::north},  {4, 7, D::north},  {4, 6, D::east},
                       {5, 6, D::east},   {6, 6, D::north},  {6, 5, D::north},  {6, 4, D::east},
                       {7, 4, D::east},   {8, 4, D::north},  {8, 3, D::east},   {9, 3, D::east},
                       {10, 3, D::east},  {11, 3, D::south}, {11, 4, D::south}, {11, 5, D::south},
                       {11, 6, D::west},  {10, 6, D::west}});
        auto quil = step_beat(22, 9, 6, D::north, {"NO", "YES", "NO"});
        quil.rest_first = true;
        quil.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x123, 1};
        route.push_back(quil);
        // East along the mountains to the Silver Blades' cave (27,2), where
        // Vala digs for the four items that rule the elementals.
        RouteBeat to_vala{17, 0, 0, D::north, {"YES"}};
        to_vala.area_steps = {C::east, C::east, C::east, C::east,
                              C::east, C::east, C::east, C::southeast};
        route.push_back(to_vala);
        // In the cave (map 37) the party tells the hill giants it has come to
        // see the Silver Lady (1,8), and through the north door Vala greets
        // it in her office (2,6) and joins it.
        route.push_back(step_beat(37, 0, 8, D::east, {"YES"}));
        route.push_back(step_beat(37, 1, 8, D::east));
        route.push_back(step_beat(37, 2, 8, D::north));
        route.push_back(step_beat(37, 2, 7, D::north, {"YES"}));
        // Rested in her office, the party goes with Vala for the four items
        // in the cluebook's order. The Worldstone (9,0): the giants fighting
        // over it will not hand it over, and are beaten (cell 1ADh bit 1).
        const auto worldstone_way = route.size();
        walk_passages(route, 37,
                      {{2, 6, D::north},
                       {2, 5, D::north},
                       {2, 4, D::north},
                       {2, 3, D::north},
                       {2, 2, D::east},
                       {3, 2, D::east},
                       {4, 2, D::east},
                       {5, 2, D::east},
                       {6, 2, D::east},
                       {7, 2, D::north},
                       {7, 1, D::east},
                       {8, 1, D::east},
                       {9, 1, D::north}});
        route[worldstone_way].rest_first = true;
        // The Crucible of Flame (11,4), past the room where Vala tells of it
        // (10,2): its fire elementals are beaten and the party takes it.
        walk_passages(route, 37,
                      {{9, 0, D::south},
                       {9, 1, D::east},
                       {10, 1, D::south},
                       {10, 2, D::south},
                       {10, 3, D::south}});
        route.push_back(step_beat(37, 10, 4, D::east, {"YES"}));
        // The Lindenwood Staff (4,12), past the Vaasans plotting with the drow
        // (5,3): the party searches the coffins, facing west, and beats the
        // dead that rise.
        walk_passages(route, 37,
                      {{11, 4, D::west},  {10, 4, D::north}, {10, 3, D::north}, {10, 2, D::north},
                       {10, 1, D::west},  {9, 1, D::west},   {8, 1, D::west},   {7, 1, D::south},
                       {7, 2, D::west},   {6, 2, D::west},   {5, 2, D::south},  {5, 3, D::east},
                       {6, 3, D::south},  {6, 4, D::south},  {6, 5, D::south},  {6, 6, D::south},
                       {6, 7, D::south},  {6, 8, D::south},  {6, 9, D::south},  {6, 10, D::south},
                       {6, 11, D::south}, {6, 12, D::west}});
        auto lindenwood = step_beat(37, 5, 12, D::west, {"YES"});
        lindenwood.rest_first = true;
        route.push_back(lindenwood);
        // The Oakroot Staff (7,14), found only by searching among the roots:
        // the party searches its way there, takes it and beats the earth
        // elementals the rocks become.
        walk_passages(route, 37,
                      {{4, 12, D::east}, {5, 12, D::east}, {6, 12, D::east}, {7, 12, D::south}},
                      true);
        auto oakroot = step_beat(37, 7, 13, D::south, {"YES"});
        oakroot.searching = true;
        route.push_back(oakroot);
        // West to the quiet room (1,13), the nearest where the digging lets
        // the party rest (the cluebook's 12).
        walk_passages(route, 37,
                      {{7, 14, D::north},
                       {7, 13, D::north},
                       {7, 12, D::west},
                       {6, 12, D::west},
                       {5, 12, D::west},
                       {4, 12, D::south},
                       {4, 13, D::west},
                       {3, 13, D::west},
                       {2, 13, D::west}},
                      false);
        // Rested, east with the four items to the chamber the giants fortify
        // (14,8): the party uses the staves, the elementals seal the breaches,
        // Vala charges after the Vaasan army, and the party beats the Vaasans
        // who got through (cell 1ABh 128). Their seven mages hold the party
        // with Hold Monster before it can act and twelve warriors slay the
        // held, with every dice the route tries: there the test wins the fight
        // (a stand-in, until the route's player gears and wards the party as a
        // player would, with the cave's treasure and its own protections).
        const auto to_breach = route.size();
        walk_passages(route, 37,
                      {{1, 13, D::east},
                       {2, 13, D::east},
                       {3, 13, D::east},
                       {4, 13, D::north},
                       {4, 12, D::east},
                       {5, 12, D::east},
                       {6, 12, D::north},
                       {6, 11, D::north},
                       {6, 10, D::north},
                       {6, 9, D::north},
                       {6, 8, D::east},
                       {7, 8, D::east},
                       {8, 8, D::east},
                       {9, 8, D::east},
                       {10, 8, D::east},
                       {11, 8, D::east},
                       {12, 8, D::east}});
        route[to_breach].rest_first = true;
        auto vaasans = step_beat(37, 13, 8, D::east, {"YES"});
        vaasans.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x1AB, 128};
        vaasans.test_wins = true;
        route.push_back(vaasans);
        // Back west along the passage and out of the cave (0,8), onto the
        // area map by its door (27,2).
        walk_passages(route, 37,
                      {{14, 8, D::west},
                       {13, 8, D::west},
                       {12, 8, D::west},
                       {11, 8, D::west},
                       {10, 8, D::west},
                       {9, 8, D::west},
                       {8, 8, D::west},
                       {7, 8, D::west},
                       {6, 8, D::west},
                       {5, 8, D::west},
                       {4, 8, D::west},
                       {3, 8, D::west},
                       {2, 8, D::west},
                       {1, 8, D::west}});
        route.push_back(step_beat(37, 0, 8, D::west, {"YES", "NO"}));
        // South-west to the army camped by Melvaunt's crater (21,5): Priam
        // joins the party and leads it to the Temple of Tyr (script 50).
        RouteBeat to_priam{17, 0, 0, D::north, {"YES"}};
        to_priam.area_steps = {C::southwest, C::southwest, C::southwest, C::west, C::west, C::west};
        route.push_back(to_priam);
        // In the entrance hall (3,7) a guard sprints upstairs as the party
        // comes into the meeting room (3,5): the party pursues at once, so it
        // surprises the vampire priest and his marauders in the office.
        route.push_back(step_beat(50, 3, 7, D::north));
        route.push_back(step_beat(50, 3, 6, D::north, {"PURSUE"}));
        // From the office (11,1) back to the stairs (8,5) and down to the
        // worship area.
        walk_passages(route, 50,
                      {{11, 1, D::west},
                       {10, 1, D::south},
                       {10, 2, D::south},
                       {10, 3, D::south},
                       {10, 4, D::south},
                       {10, 5, D::south},
                       {10, 6, D::west},
                       {9, 6, D::west},
                       {8, 6, D::north}});
        route.push_back(step_beat(50, 8, 5, D::east, {"YES"}));
        // North to Tyr's high altar (3,1): with the vampire priest dead the
        // party scrubs the black hand off it and earns Tyr's blessing.
        walk_passages(route, 50,
                      {{2, 5, D::north}, {2, 4, D::north}, {2, 3, D::north}, {2, 2, D::east}});
        auto altar = step_beat(50, 3, 2, D::north, {"SCRUB PAINT"});
        altar.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x17F, 2};
        route.push_back(altar);
        return route;
    }();
    return beats;
}

// Answers that train each of Gateway's route's members a level at a hall,
// Krevish too.
std::vector<std::string> train_gateway_party()
{
    auto answers = train_everyone();
    answers.insert(answers.end() - 1, "Krevish");
    return answers;
}

// Gateway's route plays to the game's end once this is true; until then it
// stops on this map (docs/GAMES.md, phase 5 step 4).
constexpr bool gateway_route_ends_the_game = false;
constexpr unsigned gateway_route_stops_on_map = 13;

// Gateway's story, beat by beat, as its cluebook tells it (plan phase 5, step
// 4): from the inn of Yartar on.
const std::vector<RouteBeat> &gateway_route()
{
    static const std::vector<RouteBeat> beats = []
    {
        using D = opengold::por::MapDirection;
        std::vector<RouteBeat> route{
            // Yartar: the thugs cornering Krevish.
            {1, 4, 0, D::north, {"YES"}},
            // The boat rentals, out of the city by the river.
            {1, 9, 3, D::north, {"YES"}},
        };
        // Up the River Dessarin by boat, each step as OPENGOLD_SECRET_FIND finds
        // it, to Nesme's dock.
        walk_overland(route, 22,
                      {{5, 12, D::north},
                       {5, 9, D::east},
                       {6, 8, D::east},
                       {7, 6, D::east},
                       {8, 5, D::east}});
        route.push_back(step_beat(22, 9, 3, D::south, {"YES"}));
        // Nesme: from the dock through the streets to the hidden temple of
        // Bane, its guards of lizard men and orcs, and the Banite cleric's
        // chamber (cell 4C93 set as he falls).
        walk_passages(route, 3,
                      {{3, 12, D::north}, {3, 11, D::north}, {3, 10, D::west},  {2, 10, D::west},
                       {1, 10, D::west},  {0, 10, D::north}, {0, 9, D::north},  {0, 8, D::north},
                       {0, 7, D::east},   {1, 7, D::east},   {2, 7, D::east},   {3, 7, D::east},
                       {4, 7, D::east},   {5, 7, D::east},   {6, 7, D::east},   {7, 7, D::east},
                       {8, 7, D::east},   {9, 7, D::east},   {10, 7, D::north}, {10, 6, D::north},
                       {10, 5, D::north}, {10, 4, D::north}, {10, 3, D::west},  {9, 3, D::south},
                       {9, 4, D::west},   {8, 4, D::north},  {8, 3, D::north},  {8, 2, D::west},
                       {7, 2, D::north},  {7, 1, D::north},  {7, 0, D::east},   {8, 0, D::east}},
                      true);
        {
            auto cleric = step_beat(3, 9, 0, D::south);
            cleric.rest_first = true;
            cleric.expect_cell = {{0x4C93, 1}};
            route.push_back(cleric);
        }
        // Out of the temple and the eastern gate, and overland east to
        // Silverymoon.
        walk_passages(route, 3,
                      {{9, 1, D::east},
                       {10, 1, D::north},
                       {10, 15, D::north},
                       {10, 14, D::north},
                       {10, 13, D::east},
                       {11, 12, D::east},
                       {12, 12, D::north},
                       {12, 11, D::east},
                       {13, 11, D::north},
                       {13, 10, D::north},
                       {13, 9, D::north},
                       {13, 8, D::east},
                       {14, 7, D::east},
                       {15, 7, D::east}});
        walk_overland(route, 22,
                      {{10, 4, D::east},
                       {11, 2, D::east},
                       {12, 1, D::east},
                       {13, 0, D::east},
                       {14, 15, D::east},
                       {15, 13, D::east}});
        walk_overland(route, 23,
                      {{0, 13, D::east},
                       {1, 11, D::east},
                       {2, 9, D::east},
                       {3, 7, D::east},
                       {4, 5, D::east},
                       {5, 4, D::east},
                       {6, 3, D::east}});
        route.push_back(step_beat(23, 7, 1, D::south, {"YES"}));
        // Silverymoon: by the secret door east of the gate to the hidden
        // rooms, the trolls the shouting man torments, and Erek, chained,
        // who joins to rescue Amanitas.
        walk_passages(route, 4,
                      {{0, 1, D::north},
                       {0, 0, D::east},
                       {1, 0, D::east},
                       {2, 0, D::east},
                       {3, 0, D::east},
                       {4, 0, D::east},
                       {5, 0, D::east},
                       {6, 0, D::east}},
                      true);
        {
            auto trolls = step_beat(4, 7, 0, D::east);
            trolls.rest_first = true;
            route.push_back(trolls);
        }
        route.push_back(step_beat(4, 8, 0, D::south));
        route.push_back(step_beat(4, 8, 1, D::west, {"ACCEPT HIM"}));
        // Back out of the gate and overland south to Everlund.
        walk_passages(route, 4,
                      {{7, 1, D::east},
                       {8, 1, D::north},
                       {8, 0, D::west},
                       {7, 0, D::west},
                       {6, 0, D::west},
                       {5, 0, D::west},
                       {4, 0, D::west},
                       {3, 0, D::west},
                       {2, 0, D::west},
                       {1, 0, D::west},
                       {0, 0, D::west}});
        walk_overland(route, 23,
                      {{7, 1, D::north}, {7, 14, D::north}, {7, 11, D::north}, {7, 8, D::north}});
        route.push_back(step_beat(23, 7, 5, D::west, {"YES"}));
        // Everlund: through the streets to the stable, where owlbears guard
        // the secret door to the passage beneath the River Rauvin.
        walk_passages(route, 5,
                      {{8, 15, D::north},
                       {8, 14, D::north},
                       {8, 13, D::north},
                       {8, 12, D::north},
                       {8, 11, D::north},
                       {8, 10, D::north},
                       {8, 9, D::west},
                       {7, 8, D::west},
                       {6, 8, D::west},
                       {5, 8, D::north},
                       {5, 7, D::west},
                       {4, 7, D::north},
                       {4, 6, D::west},
                       {3, 6, D::west},
                       {2, 6, D::west},
                       {1, 6, D::west}},
                      true);
        {
            auto owlbears = step_beat(5, 0, 6, D::south);
            owlbears.rest_first = true;
            owlbears.passing = true;
            owlbears.searching = true;
            route.push_back(owlbears);
        }
        walk_passages(route, 5, {{0, 7, D::south}, {0, 8, D::south}}, true);
        // The passage to the hidden Zhentarim complex, the man whipping his
        // owlbears, and Amanitas's room: his rescue (cell 4C32 set) gives the
        // Card of Counting and the Ring of Reversal.
        walk_passages(route, 5,
                      {{0, 9, D::south},
                       {0, 10, D::south},
                       {0, 11, D::south},
                       {0, 12, D::south},
                       {0, 13, D::east},
                       {1, 13, D::east},
                       {2, 13, D::east},
                       {3, 13, D::east}},
                      true);
        {
            auto amanitas = step_beat(5, 4, 13, D::east);
            amanitas.rest_first = true;
            amanitas.expect_cell = {{0x4C32, 1}};
            route.push_back(amanitas);
        }
        // Out of Everlund and overland west to Yartar, as Amanitas bids.
        walk_passages(route, 5,
                      {{5, 13, D::east},
                       {6, 13, D::south},
                       {6, 14, D::south},
                       {6, 15, D::east},
                       {7, 15, D::east},
                       {8, 15, D::south}});
        walk_overland(route, 23,
                      {{6, 6, D::west},
                       {5, 4, D::west},
                       {4, 2, D::west},
                       {3, 0, D::west},
                       {2, 15, D::west},
                       {1, 13, D::west},
                       {0, 11, D::west}});
        walk_overland(route, 22,
                      {{15, 11, D::west},
                       {14, 9, D::west},
                       {13, 7, D::west},
                       {12, 6, D::west},
                       {11, 4, D::west},
                       {10, 2, D::west},
                       {9, 1, D::west},
                       {8, 0, D::west},
                       {7, 14, D::west}});
        route.push_back(step_beat(22, 6, 12, D::west, {"YES"}));
        // Yartar's Hall of Training. A player would have hunted Nesme's trolls
        // and the wilds for experience by now; the test gives it (a stand-in,
        // as Curse's and Secret's routes give a party brought from the game
        // before) with the gold for the training, and the party trains a
        // level a visit, twice, then rests
        // at the Inn of the Glowing Gem.
        for (int level = 0; level < 2; ++level)
        {
            RouteBeat experienced{1, 3, 9, D::north, train_gateway_party()};
            experienced.test_experience = 6'000;
            experienced.test_gold = 8'000;
            route.push_back(experienced);
        }
        route.push_back({1, 0, 9, D::north, {"YES"}});
        // Leaving the city with the Ring of Reversal, the party is pulled
        // through a secret door into the Kraken complex beneath Yartar.
        walk_passages(route, 1,
                      {{3, 8, D::west}, {2, 8, D::west}, {1, 8, D::west}, {0, 8, D::west}});
        // Krevish climbs through the grating above the cell's door and
        // unlocks it (cell 4C08), tried again after a fall, as a player would.
        {
            auto grating = step_beat(2, 12, 15, D::west, {"YES", "Krevish"});
            grating.repeat_until_at_least = {{0x4C08, 1}};
            grating.repeat_in_place = true;
            route.push_back(grating);
        }
        // North through the complex to the lizard men at the captain's door,
        // and the Kraken captain and his spies.
        walk_passages(route, 2,
                      {{11, 15, D::west},
                       {10, 15, D::north},
                       {10, 14, D::north},
                       {10, 13, D::north},
                       {10, 12, D::east},
                       {11, 12, D::east},
                       {12, 12, D::north},
                       {12, 11, D::east},
                       {13, 11, D::north},
                       {13, 10, D::north},
                       {13, 9, D::north},
                       {13, 8, D::north},
                       {13, 7, D::north},
                       {13, 6, D::west},
                       {12, 6, D::north},
                       {12, 5, D::north},
                       {12, 4, D::north},
                       {12, 3, D::west}},
                      true);
        walk_passages(route, 2, {{11, 3, D::west}, {10, 3, D::south}});
        {
            // The captain's room is safe to rest in once he is beaten.
            auto rest = step_beat(2, 10, 4, D::north);
            rest.rest_first = true;
            route.push_back(rest);
        }
        // Out of Yartar's north gate and overland west to Neverwinter.
        walk_passages(route, 1,
                      {{13, 2, D::west},
                       {12, 2, D::west},
                       {11, 2, D::north},
                       {11, 1, D::north},
                       {11, 0, D::north}},
                      true);
        walk_overland(route, 22,
                      {{6, 12, D::north},
                       {6, 9, D::west},
                       {5, 8, D::west},
                       {4, 6, D::west},
                       {3, 4, D::west},
                       {2, 2, D::west},
                       {1, 0, D::west},
                       {0, 14, D::west}});
        walk_overland(route, 21,
                      {{15, 14, D::west},
                       {14, 12, D::west},
                       {13, 10, D::west},
                       {12, 8, D::west},
                       {11, 7, D::west},
                       {10, 6, D::north},
                       {10, 3, D::west},
                       {9, 1, D::west},
                       {8, 0, D::west},
                       {7, 15, D::west},
                       {6, 13, D::south},
                       {6, 12, D::west}});
        route.push_back(step_beat(21, 5, 10, D::west, {"YES"}));
        // Out of Neverwinter and north along the coast to Luskan.
        walk_passages(route, 6, {{7, 15, D::south}}, true);
        walk_overland(route, 21,
                      {{4, 11, D::north},
                       {4, 9, D::north},
                       {4, 6, D::south},
                       {4, 5, D::west},
                       {3, 3, D::west}});
        route.push_back(step_beat(21, 2, 1, D::west, {"YES"}));
        // Luskan: through the streets to the bridge to the Hosttower of the Arcane.
        walk_passages(
            route, 8,
            {{15, 15, D::north}, {15, 14, D::west},  {14, 14, D::west}, {13, 14, D::west},
             {12, 13, D::north}, {12, 12, D::north}, {12, 11, D::west}, {11, 11, D::north},
             {11, 10, D::west},  {10, 10, D::west},  {9, 10, D::west},  {8, 10, D::south},
             {8, 11, D::west},   {7, 11, D::west},   {6, 11, D::west},  {5, 11, D::west},
             {4, 11, D::west},   {3, 11, D::west},   {2, 11, D::north}, {2, 10, D::west},
             {1, 10, D::west},   {0, 10, D::north},  {0, 9, D::north}},
            true);
        // The Hosttower: through its owlbears' rooms to the tiny room whose trap door
        // leads down into the dungeon beneath.
        walk_passages(route, 9,
                      {{9, 11, D::north}, {9, 10, D::north}, {9, 9, D::west},   {8, 9, D::north},
                       {8, 8, D::west},   {7, 8, D::north},  {7, 7, D::west},   {6, 7, D::west},
                       {5, 7, D::west},   {4, 7, D::west},   {3, 7, D::south},  {3, 8, D::west},
                       {2, 8, D::south},  {2, 9, D::south},  {2, 10, D::south}, {2, 11, D::south},
                       {2, 12, D::east},  {3, 12, D::south}, {3, 13, D::east},  {4, 13, D::east},
                       {5, 13, D::east},  {6, 13, D::east},  {7, 13, D::north}, {7, 12, D::east},
                       {8, 12, D::south}, {8, 13, D::south}, {8, 14, D::south}, {8, 15, D::west},
                       {7, 15, D::west},  {1, 4, D::north},  {1, 3, D::north},  {1, 2, D::north},
                       {1, 1, D::north},  {1, 0, D::east},   {2, 0, D::east}},
                      true);
        // Brinshaar's cell.
        route.push_back(step_beat(9, 3, 0, D::south));
        // The dungeon's maze of one-way doors, as the planning aid finds it, to the
        // room where the statuette of the East is kept (bit 1 of cell 4C27).
        walk_passages(
            route, 9,
            {{3, 1, D::south},   {3, 2, D::south},   {3, 3, D::south},   {3, 4, D::west},
             {2, 4, D::south},   {2, 5, D::west},    {1, 5, D::south},   {1, 6, D::west},
             {0, 6, D::south},   {0, 7, D::east},    {1, 7, D::east},    {2, 7, D::north},
             {2, 6, D::east},    {3, 6, D::north},   {3, 5, D::east},    {4, 5, D::north},
             {4, 4, D::east},    {5, 4, D::north},   {5, 3, D::north},   {5, 2, D::north},
             {5, 1, D::north},   {5, 0, D::east},    {6, 0, D::east},    {7, 0, D::south},
             {7, 1, D::south},   {7, 2, D::south},   {7, 3, D::south},   {7, 4, D::west},
             {6, 4, D::south},   {6, 5, D::east},    {7, 5, D::south},   {7, 6, D::east},
             {8, 6, D::east},    {9, 6, D::south},   {9, 7, D::south},   {9, 8, D::east},
             {10, 8, D::east},   {11, 8, D::north},  {11, 7, D::north},  {11, 6, D::north},
             {11, 5, D::west},   {10, 5, D::west},   {9, 5, D::north},   {9, 4, D::west},
             {8, 4, D::north},   {8, 3, D::north},   {8, 2, D::north},   {8, 1, D::north},
             {8, 0, D::east},    {9, 0, D::south},   {9, 1, D::east},    {10, 1, D::east},
             {11, 1, D::south},  {11, 2, D::south},  {11, 3, D::east},   {12, 3, D::south},
             {12, 4, D::south},  {12, 5, D::south},  {12, 6, D::east},   {13, 6, D::east},
             {14, 6, D::south},  {14, 7, D::west},   {13, 7, D::west},   {12, 7, D::south},
             {12, 8, D::south},  {12, 9, D::south},  {12, 10, D::east},  {13, 10, D::south},
             {13, 11, D::south}, {13, 12, D::south}, {13, 13, D::south}, {13, 14, D::east},
             {14, 14, D::south}, {14, 15, D::east},  {15, 15, D::north}, {15, 14, D::north},
             {15, 13, D::west},  {14, 13, D::north}, {14, 12, D::east},  {15, 12, D::north},
             {15, 11, D::west},  {14, 11, D::north}, {14, 10, D::north}, {14, 9, D::west},
             {13, 9, D::north},  {13, 8, D::east},   {14, 8, D::east},   {15, 8, D::north},
             {15, 7, D::north},  {15, 6, D::north},  {15, 5, D::west},   {14, 5, D::west},
             {13, 5, D::north},  {13, 4, D::east},   {14, 4, D::east},   {15, 4, D::north},
             {15, 3, D::west},   {14, 3, D::west},   {13, 3, D::north},  {13, 2, D::north},
             {13, 1, D::east}},
            true);
        {
            auto east = step_beat(9, 14, 1, D::north);
            east.rest_first = true;
            east.expect_cell = {{0x4C27, 1}};
            route.push_back(east);
        }
        // Out of the Hosttower, where Brinshaar turns on the party, back through
        // Luskan to the pier of the Company of the Brazen Pennant, and its ship to
        // Tuern, which sails at 0800 (the party waits for it, a stand-in for a
        // player's rest in camp).
        walk_passages(route, 9,
                      {{14, 0, D::south}, {14, 1, D::south}, {14, 2, D::west},  {13, 2, D::south},
                       {13, 3, D::east},  {14, 3, D::east},  {15, 3, D::south}, {15, 4, D::west},
                       {14, 4, D::west},  {13, 4, D::south}, {13, 5, D::east},  {14, 5, D::east},
                       {15, 5, D::south}, {15, 6, D::south}, {15, 7, D::south}, {15, 8, D::west},
                       {14, 8, D::west},  {13, 8, D::south}, {13, 9, D::south}, {13, 10, D::south},
                       {13, 11, D::west}, {12, 11, D::west}, {11, 11, D::west}, {10, 11, D::west},
                       {9, 11, D::south}},
                      true);
        walk_passages(route, 8,
                      {{0, 9, D::south}, {0, 10, D::east}, {1, 10, D::east},  {2, 10, D::south},
                       {2, 11, D::east}, {3, 11, D::east}, {4, 11, D::east},  {5, 11, D::east},
                       {6, 11, D::east}, {7, 11, D::east}, {8, 11, D::north}, {8, 10, D::north},
                       {8, 9, D::north}, {8, 8, D::north}, {8, 7, D::north},  {8, 6, D::north},
                       {8, 5, D::west},  {7, 5, D::west},  {6, 5, D::west},   {5, 5, D::west},
                       {4, 5, D::west},  {3, 5, D::north}, {3, 4, D::north},  {3, 3, D::north},
                       {3, 2, D::west},  {2, 2, D::west},  {1, 2, D::west}},
                      true);
        {
            auto ship = step_beat(8, 0, 2, D::south, {"YES"});
            ship.wait_until_minute = 7 * 60 + 35;
            route.push_back(ship);
        }
        // Uttersea's pier, and the morning ship to Gundarlun.
        route.push_back(step_beat(10, 9, 12, D::south, {"GUNDARLUN"}));
        // Gundbarg's Hall of Training on the experience of the islands' fights
        // a player would have fought (the test gives it, as in Yartar), twice,
        // the Temple's raising of the dead and the Green Turtle Inn's rest.
        for (int level = 0; level < 2; ++level)
        {
            RouteBeat experienced{11, 9, 6, D::north, train_gateway_party()};
            experienced.test_experience = 25'000;
            experienced.test_gold = 8'000;
            route.push_back(experienced);
        }
        route.push_back({11, 10, 3, D::north, {"raise:dead", "Leave"}});
        route.push_back({11, 11, 7, D::north, {"YES"}});
        // Gundbarg: the party waits for the morning ship, which sails at 0900
        // (the test passes the time) and is wrecked by a giant kraken.
        {
            RouteBeat ship{11, 13, 10, D::south, {"NEVERWINTER", "PAY", "PAY"}};
            ship.wait_until_minute = 9 * 60 + 5;
            route.push_back(ship);
        }
        // The Purple Rocks: across the island to the small boat, and over the
        // straits to the Kraken headquarters.
        walk_passages(route, 12, {{13, 13, D::north}, {13, 12, D::north}, {13, 11, D::east},
                                  {14, 11, D::east},  {15, 11, D::north}, {15, 10, D::north},
                                  {15, 9, D::west},   {14, 9, D::south},  {14, 10, D::west},
                                  {13, 10, D::west},  {12, 10, D::north}, {12, 9, D::north},
                                  {12, 8, D::north},  {12, 7, D::east},   {13, 7, D::north},
                                  {13, 6, D::north},  {13, 5, D::west},   {12, 5, D::south},
                                  {12, 6, D::west},   {11, 6, D::south},  {11, 7, D::west}},
                      true);
        {
            auto boat = step_beat(12, 10, 7, D::west);
            boat.rest_first = true;
            boat.holding_foes = true;
            route.push_back(boat);
        }
        return route;
    }();
    return beats;
}

const std::vector<RouteBeat> &route_for(GoldBoxGame game)
{
    switch (game)
    {
    case GoldBoxGame::secret_of_the_silver_blades:
        return secret_route();
    case GoldBoxGame::pools_of_darkness:
        return darkness_route();
    case GoldBoxGame::gateway_to_the_savage_frontier:
        return gateway_route();
    default:
        return curse_route();
    }
}

// Secret of the Silver Blades starts with script 17 of disk 2: a vision of
// the valley over picture 119, then NEW ECL 16 on disk 1: New Verdigris
// (docs/games/secret.md).
void secret_opening_reaches_the_first_map()
{
    using opengold::por::TourPhase;
    const char *games = std::getenv("OPENGOLD_GOLDBOX_DIR");
    const auto folder = games ? std::filesystem::path(games) / "SECRET" : std::filesystem::path();
    if (!games || opengold::identify_game(folder) != GoldBoxGame::secret_of_the_silver_blades)
    {
        std::cout << "Secret's files are not here; its opening is not played.\n";
        return;
    }
    auto session = opengold::load_campaign(folder, GoldBoxGame::secret_of_the_silver_blades);
    // Six fighters made under Secret's original rules.
    std::shared_ptr<const opengold::rules::CharacterRules> creation =
        opengold::original::character_rules(opengold::original::default_aging,
                                            GoldBoxGame::secret_of_the_silver_blades);
    auto party = std::make_shared<opengold::CampaignParty>(
        opengold::original::rules_module(
            opengold::load_original_content(folder, GoldBoxGame::secret_of_the_silver_blades)),
        creation);
    for (unsigned n = 0; n < 6; ++n)
    {
        opengold::CharacterCreator creator(creation, 40 + n);
        creator.select(opengold::rules::CreationField::race, "human");
        creator.select(opengold::rules::CreationField::character_class, "fighter");
        creator.name("Fighter " + std::to_string(n + 1));
        for (const bool roll : {false, false, false, true, false, false})
        {
            if (roll)
            {
                creator.roll();
            }
            creator.next();
        }
        static_cast<void>(party->add_pc(creator.create_character()));
    }
    session.campaign_party(party);
    const auto settle = [&session]
    {
        for (int step = 0; step < 50 && session.snapshot().phase == TourPhase::running; ++step)
        {
            session.advance(1.0);
        }
    };
    const auto diagnostics = [&session]
    {
        return session.snapshot().diagnostic +
               (session.script_diagnostics().empty() ? "" : session.script_diagnostics().front());
    };
    settle();
    check(session.snapshot().dialogue.find("GROWN BORED WITH PEACE") != std::string::npos,
          "The vision begins: " + diagnostics());
    check(session.picture().has_value(), "Picture 119 shows the valley: " + diagnostics());
    for (int screen = 0; screen < 8 && session.snapshot().phase == TourPhase::awaiting_continue;
         ++screen)
    {
        check(session.choose(session.snapshot().continue_ticket, 0), "The vision goes on");
        settle();
    }
    // New Verdigris's script puts the party at (3,3) facing south (SAVE 2 to
    // C04Dh).
    const auto &snapshot = session.snapshot();
    // Priam, the mayor, welcomes the party and gives it what the town could
    // gather (TREASURE with item list 18), then tells it where to go.
    check(snapshot.phase == TourPhase::treasure && session.treasure_pile().size() == 12,
          "The mayor gives the party the town's equipment: " + diagnostics());
    check(session.leave_treasure(snapshot.continue_ticket), "The party leaves the pile");
    settle();
    std::string told;
    for (int screen = 0; screen < 8 && session.snapshot().phase == TourPhase::awaiting_continue;
         ++screen)
    {
        told += session.snapshot().dialogue;
        check(session.choose(session.snapshot().continue_ticket, 0), "The mayor goes on");
        settle();
    }
    check(told.find("TELEPORTER TO THE WELL") != std::string::npos &&
              session.snapshot().phase == TourPhase::completed,
          "The mayor tells of the teleporter to the Well, and the party may go: " + diagnostics());
    check(snapshot.area_id == 16, "NEW ECL 16 loads map 16 of disk 1: " + diagnostics());
    check(snapshot.pose.x == 3 && snapshot.pose.y == 3 &&
              snapshot.pose.facing == opengold::por::MapDirection::south,
          "The party lies on square (3,3) facing south");
    const auto view = session.observe_view();
    std::set<std::uint32_t> colours;
    for (std::size_t p = 0; p + 3 < view.rgba.size(); p += 4)
    {
        colours.insert(view.rgba[p] << 16 | view.rgba[p + 1] << 8 | view.rgba[p + 2]);
    }
    check(colours.size() > 2, "The town's walls are drawn");
    // Secret has no ending of its own: the event in which Eldamar's soul is
    // freed (script 99 sets cell 4CD9 to 1) wins the game as it ends. Here
    // the cell is set and the mayor's teleporter's question taken as the event that
    // set it.
    const auto ask_at_the_archway = [&]
    {
        opengold::test::TourAccess::place(session, {3, 0, opengold::por::MapDirection::north});
        check(session.explore(opengold::por::ExplorationCommand::look), "The archway is looked at");
        settle();
        check(session.snapshot().choices.size() == 2,
              "The archway asks: " + session.snapshot().dialogue + diagnostics());
    };
    opengold::test::TourAccess::set_variable(session, opengold::por::EclAddress{0x4CD9}, 1);
    ask_at_the_archway();
    opengold::test::TourAccess::soul_freed_in_event(session);
    check(session.choose(session.snapshot().continue_ticket, 1), "The party stays");
    settle();
    for (int screen = 0; screen < 4 && session.snapshot().phase == TourPhase::awaiting_continue &&
                         !session.snapshot().game_won;
         ++screen)
    {
        check(session.choose(session.snapshot().continue_ticket, 0), "The words go on");
        settle();
    }
    check(session.snapshot().game_won && session.snapshot().dialogue.ends_with("You have won.") &&
              session.snapshot().phase == TourPhase::awaiting_continue,
          "Freeing Eldamar's soul wins the game: " + session.snapshot().dialogue);
    check(session.continue_dialogue(session.snapshot().continue_ticket) &&
              !session.snapshot().game_won && session.snapshot().phase == TourPhase::completed,
          "The game goes on after its win");
    ask_at_the_archway();
    check(session.choose(session.snapshot().continue_ticket, 1), "The party declines again");
    settle();
    for (int screen = 0; screen < 4 && session.snapshot().phase == TourPhase::awaiting_continue;
         ++screen)
    {
        check(!session.snapshot().game_won, "The win is told once");
        check(session.choose(session.snapshot().continue_ticket, 0), "The words go on");
        settle();
    }
}

// Treasures' overland script (51) loads the map its cell 2EEh names and puts
// the party where cells 2EBh to 2EDh say (facing, x and y), which the way
// there sets; entering an overland map directly, the walk sets them.
void prepare_treasures_entry(opengold::por::RolfTourSession &session, unsigned map)
{
    constexpr unsigned overland_script = 51;
    constexpr std::uint16_t facing_cell = 0x2EB, x_cell = 0x2EC, y_cell = 0x2ED, map_cell = 0x2EE;
    if (opengold::test::TourAccess::area_scripts(session).at(map) != overland_script)
    {
        return;
    }
    for (const auto &[cell, value] :
         {std::pair{map_cell, static_cast<std::uint16_t>(map)},
          std::pair{facing_cell, std::uint16_t{0}}, std::pair{x_cell, std::uint16_t{8}},
          std::pair{y_cell, std::uint16_t{8}}})
    {
        opengold::test::TourAccess::set_variable(session, opengold::por::EclAddress{cell}, value);
    }
}

bool walk_census(GoldBoxGame game, bool route)
{
    using opengold::por::TourPhase;
    const char *games = std::getenv("OPENGOLD_GOLDBOX_DIR");
    const auto folder = games ? std::filesystem::path(games) / std::string(game_folder(game))
                              : std::filesystem::path();
    if (!games || opengold::identify_game(folder) != game)
    {
        std::cout << game_title(game) << "'s files are not here.\n";
        return true;
    }
    auto session = opengold::load_campaign(folder, game);
    // A party under the original rules, fighting Curse's own creatures.
    const auto content = opengold::load_original_content(folder, game);
    std::shared_ptr<const opengold::rules::CharacterRules> creation =
        opengold::original::character_rules(opengold::original::default_aging, game);
    std::shared_ptr<opengold::CampaignParty> party;
    // The walk's party is six fighters; the route's the cluebook's: two
    // paladins, a ranger, two clerics and a magic-user.
    const std::array<std::string, 6> walk_classes{"fighter", "fighter", "fighter",
                                                  "fighter", "fighter", "fighter"};
    const std::array<std::string, 6> route_classes{"paladin", "paladin", "ranger",
                                                   "cleric",  "cleric",  "magic_user"};
    // Each day's spells: the strongest a caster knows at each of its levels.
    const auto memorize = [&]
    {
        constexpr std::array<std::string_view, 15> favorites{
            "cone_of_cold",      "ice_storm",      "confusion",     "hold_monster",
            "fireball",          "stinking_cloud", "magic_missile", "sleep",
            "lightning_bolt",    "flame_strike",   "hold_person",   "cure_serious_wounds",
            "cure_light_wounds", "prayer",         "bless"};
        const auto dead = [&]
        {
            std::vector<opengold::MemberId> members;
            for (const auto id : party->state().slots)
            {
                if (id && party->member(id).vitals.dead)
                {
                    members.push_back(id);
                }
            }
            return members;
        };
        const bool anyone_dead = !dead().empty();
        // A cleric with Raise Dead memorized raises the dead, as a player would
        // in camp (Curse's Raise Dead; an elf cannot be raised).
        const auto raise_the_dead = [&]
        {
            for (const auto lost : dead())
            {
                for (const auto cleric : party->state().slots)
                {
                    if (!cleric || party->member(cleric).vitals.dead)
                    {
                        continue;
                    }
                    const auto actions = party->camp_actions(cleric);
                    const auto raise = std::ranges::find_if(
                        actions,
                        [](const opengold::rules::CampAction &action)
                        {
                            return action.id.find("raise_dead") != std::string::npos;
                        });
                    if (raise == actions.end())
                    {
                        continue;
                    }
                    try
                    {
                        party->use_camp_action(cleric, lost, raise->id);
                        break;
                    }
                    catch (const std::exception &)
                    {
                    }
                }
            }
        };
        if (!party->state().spell_rest)
        {
            raise_the_dead();
            return;
        }
        // Those the last Long Rest left choosing; choose_spells takes each off.
        for (const auto id : std::vector(party->state().spell_rest->members))
        {
            const auto options = party->spell_choice_options(id);
            const auto &numbers = *party->member(id).character.sheet().original;
            if (!options.may_prepare)
            {
                party->keep_rest_spells(id);
                continue;
            }
            std::vector<std::string> prepared;
            for (const auto &[prefix, slots] :
                 {std::pair{std::string("cleric:"), numbers.cleric_slots},
                  std::pair{std::string("magic_user:"), numbers.magic_user_slots}})
            {
                for (int level = 1; level <= 5; ++level)
                {
                    std::vector<std::string> candidates;
                    for (const auto &choice : options.preparation)
                    {
                        const auto number = opengold::original::spell_number(choice.id);
                        if (choice.id.starts_with(prefix) && number &&
                            opengold::original::spell(*number)->level == level)
                        {
                            candidates.push_back(choice.id);
                        }
                    }
                    const auto rank = [&](const std::string &id)
                    {
                        // With someone dead, a cleric takes Raise Dead first.
                        if (anyone_dead && id.find("raise_dead") != std::string::npos)
                        {
                            return -1;
                        }
                        const auto found =
                            std::ranges::find_if(favorites,
                                                 [&](std::string_view name)
                                                 {
                                                     return id.find(name) != std::string::npos;
                                                 });
                        return static_cast<int>(found - favorites.begin());
                    };
                    std::ranges::stable_sort(candidates, {}, rank);
                    for (int n = 0; n < slots[static_cast<std::size_t>(level - 1)] &&
                                    n < static_cast<int>(candidates.size());
                         ++n)
                    {
                        prepared.push_back(candidates[static_cast<std::size_t>(n)]);
                    }
                }
            }
            party->choose_spells(id, {{}, prepared});
        }
        raise_the_dead();
    };
    const auto new_party = [&]
    {
        party = std::make_shared<opengold::CampaignParty>(opengold::original::rules_module(content),
                                                          creation);
        const auto &classes = route ? route_classes : walk_classes;
        for (unsigned n = 0; n < 6; ++n)
        {
            const auto &klass = classes[n];
            opengold::CharacterCreator creator(creation, 90 + n);
            creator.select(opengold::rules::CreationField::race, "human");
            creator.select(opengold::rules::CreationField::character_class, klass);
            creator.name(klass + " " + std::to_string(n + 1));
            for (const bool roll : {false, false, false, true, false, false})
            {
                if (roll)
                {
                    creator.roll();
                }
                creator.next();
            }
            const auto id = party->add_pc(creator.create_character());
            // A long sword, plate mail and a shield of the game's own item
            // types; a cleric's mace; a magic-user's dagger. Secret and Pools
            // of Darkness number their types as Pool of Radiance does, not as
            // Curse.
            const bool secret = game == GoldBoxGame::secret_of_the_silver_blades ||
                                game == GoldBoxGame::pools_of_darkness;
            const std::uint8_t sword = secret ? 18 : 36, plate = secret ? 36 : 55,
                               shield = secret ? 37 : 59, mace = secret ? 8 : 23,
                               dagger = secret ? 4 : 8;
            const std::vector<std::uint8_t> gear_types =
                klass == "magic_user" ? std::vector<std::uint8_t>{dagger}
                : klass == "cleric"   ? std::vector<std::uint8_t>{mace, plate, shield}
                                      : std::vector<std::uint8_t>{sword, plate, shield};
            for (const std::uint8_t type : gear_types)
            {
                std::array<std::uint8_t, 63> raw{};
                raw[46] = type;
                // The route's party carries the magic arms of one brought from
                // Pool of Radiance: weapons +2, armor and shields +1 (the
                // record's magic bonus, byte 50).
                if (route)
                {
                    raw[50] = type == plate || type == shield ? 1 : 2;
                }
                opengold::por::Equipment gear;
                gear.stored = opengold::por::decode_items(raw)->front();
                party->purchase(id, gear);
                party->equip(id, party->member(id).character.inventory().items().back().id);
            }
        }
        party->add_coins(opengold::Coin::gold, 5000);
        if (route && party->rest())
        {
            memorize();
        }
    };
    new_party();
    session.campaign_party(party);
    std::map<std::string, std::vector<std::string>> needs;
    std::map<std::string, unsigned> phases;
    bool trace = route || std::getenv("OPENGOLD_CURSE_TRACE");
    // One play of the pending fight with these dice; the party ends as it left it.
    // A player's own choice for a member's turn, where it has one, as the
    // cluebook fights the Pit of Moander: the Wand of Defoliation (its power,
    // Blast) at the nearest enemy within its three squares, Hold Monster at
    // the strongest enemy not yet helpless, Hold Person at the strongest
    // person (not a shambling mound); Quick for the rest.
    const auto player_turn = [](const opengold::rules::CombatSession &session,
                                std::vector<opengold::rules::EntityId> &picked)
        -> std::optional<opengold::rules::Command>
    {
        constexpr unsigned blast = 198, hold_monster = 194, hold_person = 23, mage_hold_person = 49;
        const auto spell_of = [](const opengold::rules::Command &command)
        {
            return command.verb == "use" ? command.item % 256 : command.item;
        };
        const auto holds_people = [](unsigned spell)
        {
            return spell == hold_person || spell == mage_hold_person;
        };
        const auto ours = [&](unsigned spell)
        {
            return spell == blast || spell == hold_monster || holds_people(spell);
        };
        const auto state = session.snapshot();
        const auto unit_of = [&](opengold::rules::EntityId id)
        {
            return std::ranges::find(state.combatants, id, &opengold::rules::CombatantView::id);
        };
        const auto actor = unit_of(state.actor);
        const auto distance = [&](opengold::rules::Cell cell)
        {
            return actor == state.combatants.end() ? 99
                                                   : std::max(std::abs(cell.x - actor->cell.x),
                                                              std::abs(cell.y - actor->cell.y));
        };
        const auto worth_aiming = [&](opengold::rules::EntityId id, unsigned spell)
        {
            const auto unit = unit_of(id);
            return unit != state.combatants.end() &&
                   unit->side == opengold::rules::Side::opposition && unit->conscious &&
                   unit->status.empty() && std::ranges::find(picked, id) == picked.end() &&
                   (!holds_people(spell) || !unit->name.starts_with("SHAMBLING")) &&
                   (spell != blast || distance(unit->cell) <= 3);
        };
        const auto better =
            [&](const opengold::rules::Command &a, const opengold::rules::Command &b)
        {
            return spell_of(a) == blast
                       ? distance(a.destination) < distance(b.destination)
                       : unit_of(a.target)->hit_points > unit_of(b.target)->hit_points;
        };
        const auto commands = session.legal_commands();
        const opengold::rules::Command *best = nullptr;
        for (const auto &command : commands)
        {
            if (command.verb == "aim" && ours(command.item) &&
                worth_aiming(command.target, command.item) && (!best || better(command, *best)))
            {
                best = &command;
            }
        }
        if (best)
        {
            picked.push_back(best->target);
            return *best;
        }
        for (const auto &command : commands)
        {
            if (command.verb == "aim_done" && ours(command.item))
            {
                picked.clear();
                return command;
            }
        }
        picked.clear();
        for (const unsigned wanted : {blast, hold_monster, hold_person, mage_hold_person})
        {
            const bool target = std::ranges::any_of(state.combatants,
                                                    [&](const opengold::rules::CombatantView &unit)
                                                    {
                                                        return worth_aiming(unit.id, wanted);
                                                    });
            for (const auto &command : commands)
            {
                if (target && (command.verb == "use" || command.verb == "cast") &&
                    spell_of(command) == wanted)
                {
                    return command;
                }
            }
        }
        return std::nullopt;
    };
    const auto fight_once = [&](std::uint64_t seed, unsigned tactic)
    {
        opengold::CombatDemo combat(opengold::original::rules_module(content));
        combat.campaign_party(party);
        combat.encounter(*session.pending_encounter(), seed);
        if (trace)
        {
            std::cout << "  fight:";
            for (const auto &unit : combat.combat().snapshot().combatants)
            {
                std::cout << " " << unit.name << " " << unit.hit_points << "/"
                          << unit.max_hit_points;
            }
            std::cout << "\n";
        }
        // Each turn chosen as the game's combat view chooses it: the party all
        // on Quick with Quick magic on (or, as a player may set it, only the
        // magic-user's), everyone else at the default difficulty.
        std::vector<opengold::rules::EntityId> picked;
        for (int command = 0; command < 3000 && combat.combat().snapshot().outcome ==
                                                    opengold::rules::Outcome::ongoing;
             ++command)
        {
            const auto &session = combat.combat();
            const auto state = session.snapshot();
            const auto actor = std::ranges::find(state.combatants, state.actor,
                                                 &opengold::rules::CombatantView::id);
            const bool member = actor != state.combatants.end() &&
                                actor->side == opengold::rules::Side::party &&
                                !opengold::is_ally(state.actor);
            const auto held = member && tactic == 2 ? player_turn(session, picked) : std::nullopt;
            const auto chosen =
                held     ? *held
                : member ? opengold::choose_quick_command(
                               session, tactic != 1 || actor->name.starts_with("magic_user"),
                               opengold::Searching::everyone)
                         : opengold::choose_demo_command(session, opengold::Searching::everyone,
                                                         opengold::default_difficulty);
            if (!combat.submit(chosen))
            {
                ++phases["fight stopped at " + chosen.verb];
                break;
            }
        }
        // A fight no side can finish (the monsters stand where no one can
        // reach, and every turn is a guard) is left as a player leaves it:
        // each member walks to the field's edge and flees. It is counted.
        if (combat.combat().snapshot().outcome == opengold::rules::Outcome::ongoing)
        {
            ++phases["stalemates left by fleeing"];
            // Steps over open ground from each square to the field's edge.
            const auto &field = session.pending_encounter()->field.geometry;
            const auto at = [&field](int x, int y)
            {
                return static_cast<std::size_t>(y * field.width + x);
            };
            std::vector<int> steps(static_cast<std::size_t>(field.width * field.height), 9999);
            std::vector<opengold::rules::Cell> frontier;
            for (int y = 0; y < field.height; ++y)
            {
                for (int x = 0; x < field.width; ++x)
                {
                    const bool edge =
                        x == 0 || y == 0 || x == field.width - 1 || y == field.height - 1;
                    if (edge && field.terrain[at(x, y)] == opengold::rules::Terrain::open)
                    {
                        steps[at(x, y)] = 0;
                        frontier.push_back({x, y});
                    }
                }
            }
            for (std::size_t n = 0; n < frontier.size(); ++n)
            {
                const auto from = frontier[n];
                for (int dy = -1; dy <= 1; ++dy)
                {
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        const int x = from.x + dx, y = from.y + dy;
                        if (x < 0 || y < 0 || x >= field.width || y >= field.height ||
                            field.terrain[at(x, y)] != opengold::rules::Terrain::open ||
                            steps[at(x, y)] <= steps[at(from.x, from.y)] + 1)
                        {
                            continue;
                        }
                        steps[at(x, y)] = steps[at(from.x, from.y)] + 1;
                        frontier.push_back({x, y});
                    }
                }
            }
            const auto to_edge = [&](opengold::rules::Cell cell)
            {
                return cell.x < 0 || cell.y < 0 || cell.x >= field.width || cell.y >= field.height
                           ? 9999
                           : steps[at(cell.x, cell.y)];
            };
            for (int command = 0; command < 3000 && combat.combat().snapshot().outcome ==
                                                        opengold::rules::Outcome::ongoing;
                 ++command)
            {
                const auto &fighting = combat.combat();
                const auto state = fighting.snapshot();
                const auto actor = std::ranges::find(state.combatants, state.actor,
                                                     &opengold::rules::CombatantView::id);
                const bool member = actor != state.combatants.end() &&
                                    actor->side == opengold::rules::Side::party &&
                                    !opengold::is_ally(state.actor);
                std::optional<opengold::rules::Command> chosen;
                if (member)
                {
                    for (const auto &option : fighting.legal_commands())
                    {
                        if (option.verb == "flee")
                        {
                            chosen = option;
                            break;
                        }
                        if (option.verb == "move" &&
                            to_edge(option.destination) < to_edge(actor->cell) &&
                            (!chosen || to_edge(option.destination) < to_edge(chosen->destination)))
                        {
                            chosen = option;
                        }
                    }
                }
                const auto next =
                    chosen ? *chosen
                    : member
                        ? opengold::choose_quick_command(fighting, true,
                                                         opengold::Searching::everyone)
                        : opengold::choose_demo_command(fighting, opengold::Searching::everyone,
                                                        opengold::default_difficulty);
                if (!combat.submit(next))
                {
                    ++phases["fight stopped at " + next.verb];
                    break;
                }
            }
        }
        return combat.combat().snapshot();
    };
    // The pending fight. The route plays it with several dice from the party
    // as it stood, as a player reloads a save, each with three tactics, and
    // keeps the best play: a win,
    // then the fewest dead, then the fewest Hit Points lost. The walk plays it
    // once.
    // The beat being played lets the test win a fight its player loses
    // (RouteBeat::test_wins).
    bool test_winning = false;
    // The beat being played keeps every enemy from fleeing (RouteBeat::holding_foes).
    bool holding_foes = false;
    // A planning aid (OPENGOLD_SECRET_FIND) wins every fight at once.
    bool planning = false;
    // Fights played so far (a camp counts the ones that interrupt it).
    unsigned fights_fought = 0;
    const auto fight = [&]
    {
        ++fights_fought;
        const auto before = party->state();
        const auto score = [&](const opengold::rules::Snapshot &result)
        {
            int lost = result.outcome == opengold::rules::Outcome::victory ? 0 : 1'000'000;
            for (const auto &unit : result.combatants)
            {
                if (unit.side == opengold::rules::Side::party)
                {
                    // A member turned to stone is as lost as a dead one.
                    lost += (unit.dead || unit.status == "stoned" ? 1000 : 0) +
                            unit.max_hit_points - std::max(unit.hit_points, 0);
                }
                // An enemy that got away is a fight lost where its flight
                // raises the alarm.
                if (holding_foes && unit.side == opengold::rules::Side::opposition && unit.fled)
                {
                    lost += 1'000'000;
                }
            }
            return lost;
        };
        // Try t plays seed 7 + t / 3 with tactic t % 3: everyone casting, the
        // magic-user alone casting, or a player's own choices (player_turn).
        const auto play_try = [&](unsigned t)
        {
            return fight_once(7 + t / 3, t % 3);
        };
        unsigned last = 0, best_try = 0;
        auto result = planning ? opengold::rules::Snapshot{} : play_try(0);
        auto best = planning ? 1 : score(result);
        while (route && !planning && best > 0 && ++last < 120)
        {
            party->restore(before);
            result = play_try(last);
            if (score(result) < best)
            {
                best = score(result);
                best_try = last;
            }
        }
        if (last != best_try && route && !planning)
        {
            party->restore(before);
            result = play_try(best_try);
        }
        // Where the test wins fights, it also wins one the best play wins
        // only at the cost of a member dead or turned to stone: the party
        // would be lost on the way (Secret's route's stand-in).
        const bool costly =
            test_winning && game == GoldBoxGame::secret_of_the_silver_blades &&
            std::ranges::any_of(result.combatants,
                                [](const opengold::rules::CombatantView &unit)
                                {
                                    return unit.side == opengold::rules::Side::party &&
                                           !opengold::is_ally(unit.id) &&
                                           (unit.dead || unit.status == "stoned");
                                });
        if ((test_winning || planning) &&
            (result.outcome != opengold::rules::Outcome::victory || costly))
        {
            party->restore(before);
            opengold::CombatDemo won(opengold::original::rules_module(content));
            won.campaign_party(party);
            won.encounter(*session.pending_encounter(), 7);
            result = won.combat().snapshot();
            result.outcome = opengold::rules::Outcome::victory;
            for (auto &unit : result.combatants)
            {
                if (unit.side == opengold::rules::Side::opposition)
                {
                    unit.dead = true;
                    unit.conscious = false;
                    unit.hit_points = 0;
                    unit.status = "dead";
                }
            }
            if (!planning)
            {
                std::cout << "  the test wins the fight\n";
            }
        }
        // OPENGOLD_FIGHT_LOG prints the kept play's log, to see why a fight goes
        // as it does.
        if (std::getenv("OPENGOLD_FIGHT_LOG"))
        {
            for (const auto &entry : result.log_entries)
            {
                std::cout << "    | " << entry.english << '\n';
            }
        }
        ++phases["fights ended " + std::to_string(static_cast<int>(result.outcome))];
        return session.resolve_combat(result);
    };
    std::size_t diagnosed = 0;
    std::vector<std::string> answers;
    // The survey declines every offer (the last choice), so it does not walk
    // into a trap or surrender.
    bool declining = false;
    // The beat being played takes the first wand a treasure holds (RouteBeat::take_wand).
    bool taking_wand = false;
    // Whether the closing words of a won game were shown (game_won).
    bool game_won_shown = false;
    // The beat being played only passes by (RouteBeat::passing).
    bool passing = false;
    // The beat being played flees what it meets (RouteBeat::fleeing).
    bool fleeing = false;
    // The beat being played avoids the monsters it meets (RouteBeat::avoiding).
    bool avoiding = false;
    const auto play = [&](const std::string &where)
    {
        const auto attribute = [&]
        {
            const auto &found = session.script_diagnostics();
            for (; diagnosed < found.size(); ++diagnosed)
            {
                needs[found[diagnosed]].push_back(where);
            }
        };
        // A menu that comes back (a temple's services) is left by its last choice.
        std::map<std::string, unsigned> shown;
        // The words shown before an input, which a riddle's question may
        // have cleared away.
        std::string said;
        for (int step = 0; step < 400; ++step)
        {
            const auto &s = session.snapshot();
            if (!s.diagnostic.empty() && !s.diagnostic.starts_with("Bought "))
            {
                needs[s.diagnostic].push_back(where);
            }
            switch (s.phase)
            {
            case TourPhase::running:
                session.advance(1.0);
                continue;
            case TourPhase::awaiting_continue:
            {
                game_won_shown = game_won_shown || s.game_won;
                said = s.dialogue;
                // OPENGOLD_CURSE_TRACE prints each square's dialogue, to plan a route by.
                if (trace && shown[s.dialogue] == 0)
                {
                    std::cout << where << ": " << s.dialogue.substr(0, 160) << " [";
                    for (const auto &choice : s.choices)
                    {
                        std::cout << choice << "|";
                    }
                    std::cout << "]\n";
                }
                // An answer "stop:TEXT" ends the beat at the menu offering TEXT.
                if (!answers.empty() && answers.front().starts_with("stop:") &&
                    std::ranges::any_of(s.choices,
                                        [&](const std::string &choice)
                                        {
                                            return choice.find(answers.front().substr(5)) !=
                                                   std::string::npos;
                                        }))
                {
                    answers.erase(answers.begin());
                    return;
                }
                std::size_t pick = (++shown[s.dialogue] > 2 || declining) && !s.choices.empty()
                                       ? s.choices.size() - 1
                                       : 0;
                // OPENGOLD_SECRET_PREFER=WORD,... has the planning aid take a
                // choice holding one of those words where one is offered (the
                // way a walk is to go: the giants' guards ask which way; a
                // blocked door is bashed until it gives).
                if (const char *prefer = std::getenv("OPENGOLD_SECRET_PREFER"); planning && prefer)
                {
                    std::stringstream words(prefer);
                    std::string word;
                    while (std::getline(words, word, ','))
                    {
                        if (std::ranges::any_of(s.choices,
                                                [&](const std::string &choice)
                                                {
                                                    return choice.find(word) != std::string::npos;
                                                }))
                        {
                            answers = {word};
                            break;
                        }
                    }
                }
                // The planning aid takes on what it meets but declines a
                // question (NO), so it fights its way on but stays put.
                // Monsters that come on Secret's party in camp while a member
                // is below half its Hit Points: the party flees them, as a
                // player would, rather than fight on.
                const auto flee = std::ranges::find(s.choices, "FLEE");
                if (where == "camp" && game == GoldBoxGame::secret_of_the_silver_blades &&
                    flee != s.choices.end() &&
                    std::ranges::any_of(
                        party->state().slots,
                        [&](opengold::MemberId id)
                        {
                            return id && !party->member(id).vitals.dead &&
                                   party->member(id).vitals.description != "Turned to stone." &&
                                   party->member(id).vitals.hit_points * 2 <
                                       party->member(id).character.sheet().hit_points;
                        }))
                {
                    pick = static_cast<std::size_t>(flee - s.choices.begin());
                }
                else if (const auto wait = std::ranges::find(s.choices, "WAIT");
                         avoiding && wait != s.choices.end() && shown[s.dialogue] <= 2)
                {
                    pick = static_cast<std::size_t>(wait - s.choices.begin());
                }
                else if ((planning || (passing && where != "camp")) && shown[s.dialogue] <= 2)
                {
                    const auto no = std::ranges::find(s.choices, "NO");
                    pick = no == s.choices.end() ? 0
                                                 : static_cast<std::size_t>(no - s.choices.begin());
                    // OPENGOLD_FIND_YES=TEXT|TEXT... has the planning aid
                    // answer YES where the question holds a TEXT, as Gateway's
                    // "ENTER NESME?", so it can walk into the place it looks for.
                    std::stringstream yes(planning && std::getenv("OPENGOLD_FIND_YES")
                                              ? std::getenv("OPENGOLD_FIND_YES")
                                              : "");
                    for (std::string text; std::getline(yes, text, '|');)
                    {
                        if (s.dialogue.find(text) != std::string::npos)
                        {
                            pick = 0;
                        }
                    }
                    const auto flee = std::ranges::find(s.choices, "FLEE");
                    if (fleeing && flee != s.choices.end())
                    {
                        pick = static_cast<std::size_t>(flee - s.choices.begin());
                    }
                }
                // A magic-user learns the spell a player would take.
                if (s.dialogue.starts_with("Choose a spell for"))
                {
                    constexpr std::array<std::string_view, 12> learned{
                        "Hold Monster",  "Cone of Cold", "Ice Storm",      "Fireball",
                        "Haste",         "Slow",         "Lightning Bolt", "Stinking Cloud",
                        "Magic Missile", "Sleep",        "Enlarge",        "Shield"};
                    for (const auto name : learned)
                    {
                        const auto found = std::ranges::find(s.choices, name);
                        if (found != s.choices.end())
                        {
                            pick = static_cast<std::size_t>(found - s.choices.begin());
                            break;
                        }
                    }
                }
                // "raise:dead" at a temple's services raises each dead member,
                // whoever the fights have left dead, then goes on.
                if (!answers.empty() && answers.front() == "raise:dead" &&
                    std::ranges::any_of(s.choices,
                                        [](const std::string &choice)
                                        {
                                            return choice.starts_with("Raise Dead");
                                        }))
                {
                    answers.erase(answers.begin());
                    for (const auto id : party->state().slots)
                    {
                        if (id && party->member(id).vitals.dead)
                        {
                            answers.insert(answers.begin(),
                                           {"Raise Dead", party->member(id).character.sheet().name,
                                            "Pay", "raise:dead"});
                            break;
                        }
                    }
                }
                // "stone:flesh" likewise has the temple's Stone to Flesh bring
                // back each member turned to stone.
                if (!answers.empty() && answers.front() == "stone:flesh" &&
                    std::ranges::any_of(s.choices,
                                        [](const std::string &choice)
                                        {
                                            return choice.starts_with("Stone to Flesh");
                                        }))
                {
                    answers.erase(answers.begin());
                    for (const auto id : party->state().slots)
                    {
                        if (id && party->member(id).vitals.description == "Turned to stone.")
                        {
                            answers.insert(answers.begin(),
                                           {"Stone to Flesh",
                                            party->member(id).character.sheet().name, "Pay",
                                            "stone:flesh"});
                            break;
                        }
                    }
                }
                // In order; a passing beat's answers are each for whatever asks
                // it on the way, so the first one offered is taken.
                for (auto answer = answers.begin(); answer != answers.end(); ++answer)
                {
                    const auto found =
                        std::ranges::find_if(s.choices,
                                             [&](const std::string &choice)
                                             {
                                                 return choice.find(*answer) != std::string::npos;
                                             });
                    if (found != s.choices.end())
                    {
                        pick = static_cast<std::size_t>(found - s.choices.begin());
                        answers.erase(answer);
                        break;
                    }
                    if (!passing)
                    {
                        break;
                    }
                }
                if (!session.choose(s.continue_ticket, pick) &&
                    !session.continue_dialogue(s.continue_ticket))
                {
                    ++phases["stuck at a choice"];
                    attribute();
                    return;
                }
                continue;
            }
            case TourPhase::awaiting_input:
            {
                std::string typed = "0";
                // Secret's dungeon spirit's riddles and the secret word, as
                // its cluebook answers them.
                constexpr std::array<std::pair<std::string_view, std::string_view>, 11> riddles{
                    {{"YOU FEEL IT, YOU NEVER SAW IT", "YOUR HEART"},
                     {"YOU MUST KEEP IT AFTER GIVING IT", "YOUR WORD"},
                     {"LIGHT AS A FEATHER", "YOUR BREATH"},
                     {"HAS A MOUTH BUT DOES NOT SPEAK", "RIVER"},
                     {"RUNS SMOOTHER THAN ANY RHYME", "WATER"},
                     {"YOU BREAK IT IF YOU EVEN NAME IT", "SILENCE"},
                     {"PASSES BEFORE THE SUN", "WIND"},
                     {"YOU FEED IT, IT LIVES", "FIRE"},
                     {"WHAT IS THE SECRET WORD?", "NACACIA"},
                     // The castle's: the iron golems' password, and the
                     // Dreadlord's door, which asks for his brother.
                     {"IRON GOLEMS APPEARS", "STEELEYE"},
                     {"FRATERNAL BONDS", "OSWULF"}}};
                for (const auto &[riddle, answer] : riddles)
                {
                    if ((said + s.dialogue).find(riddle) != std::string::npos)
                    {
                        typed = answer;
                    }
                }
                if (!answers.empty() && answers.front().starts_with("type:"))
                {
                    typed = answers.front().substr(5);
                    answers.erase(answers.begin());
                }
                if (trace)
                {
                    std::cout << where << ": " << s.dialogue.substr(0, 160) << " <" << typed
                              << ">\n";
                }
                (void)session.input(s.continue_ticket, typed);
                continue;
            }
            case TourPhase::shopping:
                // Treasures' shop 12 trades gems and sells nothing.
                if (session.shop_stock().empty() &&
                    game != GoldBoxGame::treasures_of_the_savage_frontier)
                {
                    needs["A shop with nothing to sell"].push_back(where);
                }
                ++phases["shops with goods"];
                // "buy:NAME" buys each member an item whose name holds NAME
                // and readies it, as a player outfits the party (Secret's
                // armoury sells the mirrors that turn gazes back).
                if (!answers.empty() && answers.front().starts_with("buy:"))
                {
                    const auto wanted = answers.front().substr(4);
                    answers.erase(answers.begin());
                    std::cout << where << ": the shop sells";
                    for (const auto &offered : session.shop_stock())
                    {
                        std::cout << " " << offered.label() << ";";
                    }
                    std::cout << "\n";
                    const auto slots = party->state().slots;
                    for (std::size_t slot = 0; slot < slots.size(); ++slot)
                    {
                        const auto &stock = session.shop_stock();
                        const auto item = std::ranges::find_if(
                            stock,
                            [&](const opengold::por::Equipment &offered)
                            {
                                auto name = offered.label();
                                std::ranges::transform(name, name.begin(), ::toupper);
                                return name.find(wanted) != std::string::npos;
                            });
                        if (!slots[slot] || item == stock.end())
                        {
                            continue;
                        }
                        party->select(opengold::PartySlot{static_cast<unsigned>(slot)});
                        if (session.buy(session.snapshot().continue_ticket,
                                        static_cast<std::size_t>(item - stock.begin())))
                        {
                            const auto &items =
                                party->member(slots[slot]).character.inventory().items();
                            try
                            {
                                party->equip(slots[slot], items.back().id);
                            }
                            catch (const std::exception &)
                            {
                            }
                        }
                    }
                }
                (void)session.leave_shop(session.snapshot().continue_ticket);
                continue;
            case TourPhase::treasure:
            {
                // The route takes a wand for its magic-user, readied for a fight
                // (the original's AI uses only readied items), as a player would.
                const auto &pile = session.treasure_pile();
                const auto wand =
                    std::ranges::find_if(pile,
                                         [](const opengold::por::Equipment &item)
                                         {
                                             auto name = item.label();
                                             std::ranges::transform(name, name.begin(), ::toupper);
                                             return name.find("WAND") != std::string::npos;
                                         });
                const auto slots = party->state().slots;
                const auto mage = std::ranges::find_if(
                    slots,
                    [&](opengold::MemberId id)
                    {
                        return id &&
                               party->member(id).character.sheet().name.starts_with("magic_user");
                    });
                if (route && taking_wand && wand != pile.end() && mage != slots.end())
                {
                    const auto wand_name = wand->label();
                    party->select(opengold::PartySlot{static_cast<unsigned>(mage - slots.begin())});
                    const auto taken = session.take_treasure(
                        s.continue_ticket, static_cast<std::size_t>(wand - pile.begin()));
                    if (taken)
                    {
                        const auto &items = party->member(*mage).character.inventory().items();
                        party->equip(*mage, items.back().id);
                        std::cout << "  " << where << ": the magic-user takes and readies "
                                  << wand_name << "\n";
                        taking_wand = false;
                        continue;
                    }
                }
                (void)session.leave_treasure(s.continue_ticket);
                continue;
            }
            case TourPhase::combat:
                if (session.monster_picture())
                {
                    (void)session.start_encounter();
                    continue;
                }
                // A fight that ends without a result fails the walk: a pass
                // must not hide one.
                if (!session.pending_encounter() || !fight())
                {
                    needs[session.pending_encounter()
                              ? "A fight ended without a result: " + session.snapshot().diagnostic
                              : std::string("A fight began with no encounter")]
                        .push_back(where);
                    attribute();
                    return;
                }
                continue;
            default:
                ++phases[std::to_string(static_cast<int>(s.phase))];
                attribute();
                return;
            }
        }
        const auto &s = session.snapshot();
        std::cout << "Too long at " << where << ", phase " << static_cast<int>(s.phase) << ": "
                  << s.dialogue << "\n";
        attribute();
        ++phases["too long"];
    };
    // A new game with a new party, standing on `map` as its script's entry has it.
    const auto restart = [&](unsigned map)
    {
        session = opengold::load_campaign(folder, game);
        new_party();
        session.campaign_party(party);
        diagnosed = 0;
        play("opening");
        if (session.snapshot().area_id == map)
        {
            return true;
        }
        const auto where = "map " + std::to_string(map) + " entry";
        // Gateway's overland script (21) loads the map its cell 4C24h names,
        // which the way the party came set: the overland map entered, or else
        // Yartar's (22), where a new game's party first goes out (ECL2:1).
        if (game == GoldBoxGame::gateway_to_the_savage_frontier)
        {
            constexpr unsigned overland_script = 21, yartar_overland = 22;
            const bool overland =
                opengold::test::TourAccess::area_scripts(session).at(map) == overland_script;
            opengold::test::TourAccess::set_variable(
                session, opengold::por::EclAddress{0x4C24},
                static_cast<std::uint16_t>(overland ? map : yartar_overland));
        }
        if (game == GoldBoxGame::treasures_of_the_savage_frontier)
        {
            prepare_treasures_entry(session, map);
        }
        try
        {
            opengold::test::TourAccess::enter(session, map,
                                              {8, 8, opengold::por::MapDirection::north});
            play(where);
        }
        catch (const std::exception &error)
        {
            needs[error.what()].push_back(where);
            return false;
        }
        return session.snapshot().area_id == map && session.can_leave();
    };
    // Every square of the map, its event run where it has one; an event that
    // ends the game or takes the party away is played from a new game.
    const auto walk = [&](unsigned area)
    {
        for (unsigned y = 0; y < 16; ++y)
        {
            for (unsigned x = 0; x < 16; ++x)
            {
                const auto where = "map " + std::to_string(area) + " " + std::to_string(x) + "," +
                                   std::to_string(y);
                if ((session.snapshot().area_id != area || !session.can_leave()) && !restart(area))
                {
                    ++phases["map not reentered"];
                    return;
                }
                opengold::test::TourAccess::place(session,
                                                  {x, y, opengold::por::MapDirection::north});
                try
                {
                    if (session.explore(opengold::por::ExplorationCommand::look))
                    {
                        play(where);
                    }
                }
                catch (const std::exception &error)
                {
                    needs[error.what()].push_back(where);
                    return;
                }
            }
        }
    };
    if (route)
    {
        // Days of rest in camp until every member is whole, as a player rests
        // (or a script's PROGRAM 9 offers the camp).
        // A cleric of Secret's party who could raise the dead after a rest.
        const auto cleric_standing = [&]
        {
            return game == GoldBoxGame::secret_of_the_silver_blades &&
                   std::ranges::any_of(
                       party->state().slots,
                       [&](opengold::MemberId id)
                       {
                           return id && !party->member(id).vitals.dead &&
                                  party->member(id).vitals.description != "Turned to stone." &&
                                  party->member(id).character.sheet().name.starts_with("cleric");
                       });
        };
        // Every member whole: no one hurt, and in Secret's route no one dead
        // a standing cleric could raise.
        const auto party_whole = [&]
        {
            const bool raising = cleric_standing();
            return std::ranges::all_of(
                party->state().slots,
                [&](opengold::MemberId id)
                {
                    return !id || (party->member(id).vitals.dead && !raising) ||
                           party->member(id).vitals.description == "Turned to stone." ||
                           (!party->member(id).vitals.dead &&
                            party->member(id).vitals.hit_points >=
                                party->member(id).character.sheet().hit_points);
                });
        };
        // A member below half its Hit Points (not a statue), or one dead a
        // standing cleric could raise: a player passing through rests first.
        const auto party_hurt = [&]
        {
            return std::ranges::any_of(
                party->state().slots,
                [&](opengold::MemberId id)
                {
                    return id && ((party->member(id).vitals.dead && cleric_standing()) ||
                                  (!party->member(id).vitals.dead &&
                                   party->member(id).vitals.description != "Turned to stone." &&
                                   party->member(id).vitals.hit_points * 2 <
                                       party->member(id).character.sheet().hit_points));
                });
        };
        // The map where Secret's party last gave up resting.
        unsigned restless_map = 0xFFFF;
        const auto rest_until_healed = [&]
        {
            // Secret's party gives up resting where monsters break its camp
            // three times (its castle's guards come every night), and on that
            // map rests no more while it only passes by.
            unsigned interrupted = 0;
            for (int day = 0; day < 90 && session.can_leave(); ++day)
            {
                if (party_whole() || !session.camp(opengold::RestKind::long_rest))
                {
                    break;
                }
                // The spells are chosen as the rest ends, as the rest dialog
                // has it, before a camp's interruption or the map's script.
                memorize();
                const auto fights_before = fights_fought;
                play("camp");
                if (fights_fought != fights_before &&
                    game == GoldBoxGame::secret_of_the_silver_blades && ++interrupted >= 3)
                {
                    restless_map = session.snapshot().area_id;
                    break;
                }
            }
        };
        // Days in camp from the wilderness's menu until every member is whole:
        // CAMP opens the camp, and the menu returns after each night or fight.
        const auto rest_in_wilderness = [&]
        {
            // Leaving the camp without resting brings the menu back.
            answers = {"CAMP"};
            play("wilderness camp");
            session.decline_camp();
            answers = {"stop:JOURNEY"};
            play("wilderness camp");
            if (session.snapshot().phase != TourPhase::awaiting_continue)
            {
                needs["The wilderness's menu does not return after a declined camp"].push_back(
                    "wilderness camp");
            }
            for (int day = 0; day < 90 && !party_whole(); ++day)
            {
                answers = {"CAMP"};
                play("wilderness camp");
                if (!session.snapshot().camp_offered ||
                    !session.camp(opengold::RestKind::long_rest))
                {
                    break;
                }
                // The spells are chosen as the rest ends, before the camp is
                // attacked or the menu returns, as the rest dialog has it.
                memorize();
                answers = {"stop:JOURNEY"};
                play("wilderness camp");
            }
        };
        play("opening");
        std::size_t beat_number = 0;
        // A beat that throws ends the route, named with the beat it was in.
        std::string current_beat = "the route's start";
        try
        {
            // OPENGOLD_ROUTE_BEATS=N plays only the route's first N beats, so
            // the planning aids can look from a point short of its end.
            const char *beat_limit = std::getenv("OPENGOLD_ROUTE_BEATS");
            for (const auto &beat : route_for(game))
            {
                ++beat_number;
                if (beat_limit && beat_number > std::stoul(beat_limit))
                {
                    break;
                }
                const auto where = "beat " + std::to_string(beat.map) + " " +
                                   std::to_string(beat.x) + "," + std::to_string(beat.y);
                current_beat = where + " (beat " + std::to_string(beat_number) + ")";
                if (session.snapshot().phase == TourPhase::defeated)
                {
                    break;
                }
                // A camp before the beat fights as the beat does.
                test_winning = beat.test_wins;
                if (beat.rest_first ||
                    (beat.passing && party_hurt() && session.snapshot().area_id != restless_map))
                {
                    beat.waiting ? rest_in_wilderness() : rest_until_healed();
                }
                if (session.snapshot().phase == TourPhase::defeated)
                {
                    break;
                }
                for (int day = 0; beat.test_heal && day < 90 && !party_whole() && party->rest();
                     ++day)
                {
                    memorize();
                }
                if (beat.wait_until_minute)
                {
                    // The campaign's clock starts at noon.
                    constexpr std::uint64_t minutes_a_day = 24 * 60, noon = 12 * 60;
                    const auto now = (party->state().time_minutes + noon) % minutes_a_day;
                    party->advance_time(std::chrono::minutes{
                        (*beat.wait_until_minute + minutes_a_day - now) % minutes_a_day});
                }
                for (const auto &[address, value] : beat.test_cells)
                {
                    opengold::test::TourAccess::set_variable(
                        session, opengold::por::EclAddress{address}, value);
                }
                if (beat.test_experience)
                {
                    for (const auto id : party->state().slots)
                    {
                        if (id)
                        {
                            party->award_experience(beat.test_experience,
                                                    "test:curse-route-" +
                                                        std::to_string(beat_number) + "-" +
                                                        std::to_string(id),
                                                    id);
                        }
                    }
                    party->add_coins(opengold::Coin::gold, beat.test_gold);
                }
                const auto cell = [&](std::uint16_t address)
                {
                    return opengold::test::TourAccess::machine(session).variable(
                        opengold::por::EclAddress{address});
                };
                for (int round = 0; round < 40; ++round)
                {
                    answers = beat.answers;
                    taking_wand = beat.take_wand;
                    passing = beat.passing;
                    fleeing = beat.fleeing;
                    avoiding = beat.avoiding;
                    if (beat.searching)
                    {
                        session.set_searching(*beat.searching);
                    }
                    test_winning = beat.test_wins;
                    holding_foes = beat.holding_foes;
                    if (beat.waiting)
                    {
                        play(where);
                    }
                    else if (!beat.area_steps.empty())
                    {
                        for (const auto step : beat.area_steps)
                        {
                            if (session.explore(step))
                            {
                                play(where);
                            }
                        }
                    }
                    else if (beat.enter_map)
                    {
                        opengold::test::TourAccess::enter(session, beat.map,
                                                          {beat.x, beat.y, beat.facing});
                        play(where);
                    }
                    else
                    {
                        const unsigned x = beat.repeat_in_place
                                               ? beat.x
                                               : (beat.x + static_cast<unsigned>(round)) % 16;
                        opengold::test::TourAccess::place(session, {x, beat.y, beat.facing});
                        if (session.explore(beat.step ? opengold::por::ExplorationCommand::forward
                                                      : opengold::por::ExplorationCommand::look))
                        {
                            play(where);
                        }
                    }
                    const auto &until = beat.repeat_until_at_least;
                    if (!until || cell(until->first) >= until->second ||
                        session.snapshot().phase == TourPhase::defeated)
                    {
                        break;
                    }
                    for (int day = 0; beat.test_heal && day < 90 && !party_whole() && party->rest();
                         ++day)
                    {
                        memorize();
                    }
                }
                if (session.snapshot().camp_offered)
                {
                    rest_until_healed();
                }
                if (beat.expect_cell)
                {
                    const auto held = opengold::test::TourAccess::machine(session).variable(
                        opengold::por::EclAddress{beat.expect_cell->first});
                    if (held != beat.expect_cell->second)
                    {
                        needs["Cell " + std::to_string(beat.expect_cell->first) + " holds " +
                              std::to_string(held) + ", not " +
                              std::to_string(beat.expect_cell->second)]
                            .push_back(where);
                    }
                }
                // OPENGOLD_BEAT_WORDS prints the words a beat ends on, which a
                // trace leaves out when they ask nothing (an event that exits).
                if (std::getenv("OPENGOLD_BEAT_WORDS"))
                {
                    std::cout << "  words: " << session.snapshot().dialogue.substr(0, 150) << "\n";
                }
                std::cout << "  party after " << where << ":";
                for (const auto id : party->state().slots)
                {
                    if (id)
                    {
                        const auto &member = party->member(id);
                        std::cout << " " << member.vitals.hit_points << "/"
                                  << member.character.sheet().hit_points
                                  << (member.vitals.dead ? " dead" : "");
                        if (const auto &numbers = member.character.sheet().original)
                        {
                            std::cout << " (" << member.experience << " xp, level "
                                      << std::ranges::max(numbers->class_levels) << ")";
                        }
                    }
                }
                std::cout << (session.snapshot().camp_offered ? " (camp offered)" : "") << "\n";
                if (std::getenv("OPENGOLD_CURSE_CELL"))
                {
                    const auto cell =
                        static_cast<std::uint16_t>(std::stoul(std::getenv("OPENGOLD_CURSE_CELL")));
                    std::cout << "  cell " << cell << " = "
                              << opengold::test::TourAccess::machine(session).variable(
                                     opengold::por::EclAddress{cell})
                              << "\n";
                }
            }
        }
        catch (const std::exception &error)
        {
            needs[error.what()].push_back(current_beat);
        }

        for (const auto &[phase, count] : phases)
        {
            std::cout << "  ended " << count << "x in phase " << phase << "\n";
        }
        for (const auto &[message, places] : needs)
        {
            std::cout << "  " << message << " (at " << places.front() << ")\n";
        }
        const auto &end = session.snapshot();
        std::cout << "Route ends on map " << end.area_id << " (script " << end.script_id << ") at "
                  << end.pose.x << "," << end.pose.y << ", phase " << static_cast<int>(end.phase)
                  << "\n";
        // OPENGOLD_DARKNESS_SITES looks at every square of Pools of Darkness's
        // area map (cells 25h and 26h), each from where the route ended, and
        // prints what is there, to plan the route's journeys by.
        if (std::getenv("OPENGOLD_DARKNESS_SITES"))
        {
            constexpr std::uint16_t columns = 38, rows = 15;
            const auto base = session.detached_copy();
            const auto base_party = party->state();
            for (std::uint16_t row = 0; row < rows; ++row)
            {
                for (std::uint16_t column = 0; column < columns; ++column)
                {
                    session = base.detached_copy();
                    party->restore(base_party);
                    session.attach_restored_party(party);
                    opengold::test::TourAccess::set_variable(
                        session, opengold::por::EclAddress{0x25}, column);
                    opengold::test::TourAccess::set_variable(session,
                                                             opengold::por::EclAddress{0x26}, row);
                    if (!session.explore(opengold::por::ExplorationCommand::look))
                    {
                        continue;
                    }
                    for (int n = 0; n < 50 && session.snapshot().phase == TourPhase::running; ++n)
                    {
                        session.advance(1.0);
                    }
                    std::cout << "site " << column << "," << row << ": "
                              << session.snapshot().dialogue.substr(0, 90) << "\n";
                }
            }
        }
        // OPENGOLD_ROUTE_EVENTS prints the event number of each square of the
        // map the route ended on, row by row, to find where a script's event is.
        if (std::getenv("OPENGOLD_ROUTE_EVENTS"))
        {
            for (unsigned y = 0; y < 16; ++y)
            {
                std::cout << "events row " << y << ":";
                for (unsigned x = 0; x < 16; ++x)
                {
                    std::cout << " " << session.map().at(x, y).event_number();
                }
                // Each square's way out north, east, south and west: its
                // barrier code (1 open, 2 and 3 locked), with a wall marked.
                std::cout << "\nbarriers row " << y << ":";
                for (unsigned x = 0; x < 16; ++x)
                {
                    const auto &cell = session.map().at(x, y);
                    std::cout << " ";
                    for (unsigned side = 0; side < 4; ++side)
                    {
                        std::cout << static_cast<int>(cell.doors[side])
                                  << (cell.walls[side] ? "w" : "");
                    }
                }
                std::cout << "\n";
            }
        }
        // OPENGOLD_CURSE_SURVEY walks the map the route ended on, printing each
        // square's event, to plan the route's next beats by.
        if (std::getenv("OPENGOLD_CURSE_SURVEY") && end.phase != TourPhase::defeated)
        {
            const auto map = end.area_id;
            declining = true;
            for (unsigned y = 0; y < 16 && session.snapshot().area_id == map; ++y)
            {
                for (unsigned x = 0; x < 16 && session.snapshot().area_id == map; ++x)
                {
                    if (!session.can_leave())
                    {
                        break;
                    }
                    // A square that holds the party (a menu it loops in) is
                    // undone, and the survey goes on.
                    auto before_square = session.detached_copy();
                    const auto party_before = party->state();
                    opengold::test::TourAccess::place(session,
                                                      {x, y, opengold::por::MapDirection::north});
                    if (session.explore(opengold::por::ExplorationCommand::look))
                    {
                        const auto where = "survey " + std::to_string(map) + " " +
                                           std::to_string(x) + "," + std::to_string(y);
                        play(where);
                        // An event's words that ask nothing, such as a sign.
                        if (session.snapshot().phase == TourPhase::completed &&
                            !session.snapshot().dialogue.empty())
                        {
                            std::cout << where
                                      << " says: " << session.snapshot().dialogue.substr(0, 120)
                                      << "\n";
                        }
                    }
                    if (!session.can_leave() || session.snapshot().area_id != map)
                    {
                        session = std::move(before_square);
                        party->restore(party_before);
                        session.attach_restored_party(party);
                    }
                    // A planning aid, so the party rests outside the map, where
                    // no patrol interrupts it.
                    for (int day = 0; day < 90 && !party_whole() && party->rest(); ++day)
                    {
                        memorize();
                    }
                }
            }
            std::cout << "Survey ends on map " << session.snapshot().area_id << ", phase "
                      << static_cast<int>(session.snapshot().phase) << "\n";
        }
        // OPENGOLD_SECRET_LINKS prints, for Secret's ruins (one map, its
        // sectors told apart by cell 4C4B), every step that leaves a sector
        // or the map: sector, square and facing, and where the step lands.
        if (std::getenv("OPENGOLD_SECRET_LINKS") && end.area_id == 32 && session.can_leave())
        {
            const auto base = session.detached_copy();
            const auto party_before = party->state();
            std::cout << "sector now "
                      << opengold::test::TourAccess::machine(session).variable(
                             opengold::por::EclAddress{0x4C4B})
                      << "\n";
            for (std::uint16_t sector = 1; sector <= 10; ++sector)
            {
                for (unsigned y = 0; y < 16; ++y)
                {
                    for (unsigned x = 0; x < 16; ++x)
                    {
                        for (unsigned f = 0; f < 4; ++f)
                        {
                            auto trial = base.detached_copy();
                            trial.attach_restored_party(party);
                            opengold::test::TourAccess::set_variable(
                                trial, opengold::por::EclAddress{0x4C4B}, sector);
                            const auto facing = static_cast<opengold::por::MapDirection>(f);
                            opengold::test::TourAccess::place(trial, {x, y, facing});
                            (void)trial.explore(opengold::por::ExplorationCommand::forward);
                            for (int n = 0; n < 50 && trial.snapshot().phase == TourPhase::running;
                                 ++n)
                            {
                                trial.advance(1.0);
                            }
                            const auto machine = opengold::test::TourAccess::machine(trial);
                            const auto now = machine.variable(opengold::por::EclAddress{0x4C4B});
                            const auto &s = trial.snapshot();
                            const auto moved =
                                std::abs(static_cast<int>(s.pose.x) - static_cast<int>(x)) +
                                std::abs(static_cast<int>(s.pose.y) - static_cast<int>(y));
                            if (now != sector || s.area_id != 32 || moved > 1 ||
                                s.pose.facing != facing)
                            {
                                std::cout
                                    << "link " << sector << " " << x << "," << y << " f" << f
                                    << " -> map " << s.area_id << " sector " << now << " "
                                    << s.pose.x << "," << s.pose.y << " f"
                                    << static_cast<int>(s.pose.facing) << " big "
                                    << machine.variable(opengold::por::EclAddress{0x4CFD}) << ","
                                    << machine.variable(opengold::por::EclAddress{0x4CFE}) << " "
                                    << s.dialogue.substr(0, 60) << "\n";
                            }
                            party->restore(party_before);
                        }
                    }
                }
            }
        }
        // OPENGOLD_SECRET_FIND=X,Y[,MAP] searches, from where the route ends,
        // the steps that reach that square, each step played as the game
        // plays it (passages, their scripts' moves, arrivals' events with
        // every offer declined and every fight won at once): it prints the
        // steps, those that are not a plain step to the next square marked.
        // OPENGOLD_SECRET_KEYS=CELL,... (hex) tells squares apart by those
        // cells too (a mine level's half, the ruins' sector), and
        // OPENGOLD_SECRET_GOAL=CELL=VALUE,... asks the goal to hold them.
        if (const char *find = std::getenv("OPENGOLD_SECRET_FIND"); find && session.can_leave())
        {
            unsigned goal_x = 0, goal_y = 0, goal_map = end.area_id;
            (void)std::sscanf(find, "%u,%u,%u", &goal_x, &goal_y, &goal_map);
            // FIND=* asks for the goal's cells anywhere.
            const bool anywhere = find[0] == '*';
            // FIND=map:N asks for any square of map N.
            const bool any_square = std::string_view(find).starts_with("map:");
            if (any_square)
            {
                goal_map = static_cast<unsigned>(std::stoul(find + 4));
            }
            const auto hex_list = [](const char *text)
            {
                std::vector<std::pair<std::uint16_t, std::uint16_t>> cells;
                std::stringstream stream(text ? text : "");
                std::string item;
                while (std::getline(stream, item, ','))
                {
                    const auto equals = item.find('=');
                    cells.emplace_back(
                        static_cast<std::uint16_t>(std::stoul(item.substr(0, equals), nullptr, 16)),
                        equals == std::string::npos
                            ? 0
                            : static_cast<std::uint16_t>(std::stoul(item.substr(equals + 1))));
                }
                return cells;
            };
            const auto keys = hex_list(std::getenv("OPENGOLD_SECRET_KEYS"));
            const auto goal_cells = hex_list(std::getenv("OPENGOLD_SECRET_GOAL"));
            const auto cell_of = [](const opengold::por::RolfTourSession &at, std::uint16_t address)
            {
                return opengold::test::TourAccess::machine(at).variable(
                    opengold::por::EclAddress{address});
            };
            const auto cells_of = [&](const opengold::por::RolfTourSession &at)
            {
                std::string values;
                for (const auto &[address, value] : keys)
                {
                    values += " " + std::to_string(cell_of(at, address));
                }
                return values;
            };
            const auto key_of = [&](const opengold::por::RolfTourSession &at)
            {
                const auto &s = at.snapshot();
                return std::to_string(s.area_id) + " " + std::to_string(s.pose.x) + "," +
                       std::to_string(s.pose.y) + cells_of(at);
            };

            struct Node
            {
                opengold::por::RolfTourSession session;
                std::remove_cvref_t<decltype(party->state())> party_state;
                std::size_t parent;
                std::string move;
            };

            auto main_session = std::move(session);
            const auto main_party = party->state();
            std::vector<Node> nodes;
            nodes.push_back({main_session.detached_copy(), main_party, 0, "start"});
            // OPENGOLD_SECRET_SEARCHING walks in search mode.
            nodes.back().session.set_searching(std::getenv("OPENGOLD_SECRET_SEARCHING") != nullptr);
            std::set<std::string> seen{key_of(main_session)};
            planning = true;
            declining = true;
            trace = false;
            std::optional<std::size_t> found;
            for (std::size_t i = 0; i < nodes.size() && i < 20000 && !found; ++i)
            {
                for (unsigned f = 0; f < 4 && !found; ++f)
                {
                    session = nodes[i].session.detached_copy();
                    party->restore(nodes[i].party_state);
                    session.attach_restored_party(party);
                    const auto from = session.snapshot().pose;
                    const auto facing = static_cast<opengold::por::MapDirection>(f);
                    opengold::test::TourAccess::place(session, {from.x, from.y, facing});
                    (void)session.explore(opengold::por::ExplorationCommand::forward);
                    play("find");
                    if (!session.can_leave())
                    {
                        continue;
                    }
                    const auto key = key_of(session);
                    if (!seen.insert(key).second)
                    {
                        continue;
                    }
                    const auto &to = session.snapshot().pose;
                    const bool plain =
                        session.snapshot().area_id == nodes[i].session.snapshot().area_id &&
                        std::abs(static_cast<int>(to.x) - static_cast<int>(from.x)) +
                                std::abs(static_cast<int>(to.y) - static_cast<int>(from.y)) ==
                            1 &&
                        cells_of(session) == cells_of(nodes[i].session);
                    nodes.push_back({session.detached_copy(), party->state(), i,
                                     std::string(plain ? "  " : "* ") + std::to_string(from.x) +
                                         "," + std::to_string(from.y) + " f" + std::to_string(f) +
                                         " -> " + key});
                    const bool at_goal =
                        (anywhere || (session.snapshot().area_id == goal_map &&
                                      (any_square || (to.x == goal_x && to.y == goal_y)))) &&
                        std::ranges::all_of(goal_cells,
                                            [&](const auto &cell)
                                            {
                                                return cell_of(session, cell.first) == cell.second;
                                            });
                    if (at_goal)
                    {
                        found = nodes.size() - 1;
                    }
                }
            }
            if (found)
            {
                std::vector<std::string> path;
                for (auto at = *found; at != 0; at = nodes[at].parent)
                {
                    path.push_back(nodes[at].move);
                }
                std::cout << "find: from " << key_of(main_session) << "\n";
                for (auto step = path.rbegin(); step != path.rend(); ++step)
                {
                    std::cout << "find: " << *step << "\n";
                }
            }
            else
            {
                std::cout << "find: not reached in " << nodes.size() << " squares:\n";
                for (const auto &node : nodes)
                {
                    std::cout << "find: " << node.move << "\n";
                }
            }
            session = std::move(main_session);
            party->restore(main_party);
            session.attach_restored_party(party);
            planning = false;
            declining = false;
        }
        // The route's last beat wins the game, its closing words shown as a
        // win (game_won): Curse's Tyranthraxus falls and its ending is told;
        // Secret's soul gem is shattered (cell 4CD9 set to 1) and the win
        // follows the soul's freeing.
        const bool all_alive = std::ranges::none_of(party->state().slots,
                                                    [&](opengold::MemberId id)
                                                    {
                                                        return id && party->member(id).vitals.dead;
                                                    });
        const bool played = needs.empty() && end.phase != TourPhase::defeated;
        // Secret's route does not reach its end yet: until it does, it passes
        // when it plays every beat to the map of its last.
        if (game == GoldBoxGame::secret_of_the_silver_blades && !secret_route_ends_the_game)
        {
            std::cout << "Secret's route stops on map " << end.area_id
                      << ", short of the game's end (docs/GAMES.md, phase 4 step 4).\n";
            return played && end.area_id == secret_route_stops_on_map;
        }
        if (game == GoldBoxGame::pools_of_darkness && !darkness_route_ends_the_game)
        {
            std::cout << "Pools of Darkness's route stops on map " << end.area_id
                      << ", short of the game's end (docs/GAMES.md, Pools of Darkness).\n";
            return played && end.area_id == darkness_route_stops_on_map;
        }
        if (game == GoldBoxGame::gateway_to_the_savage_frontier && !gateway_route_ends_the_game)
        {
            std::cout << "Gateway's route stops on map " << end.area_id
                      << ", short of the game's end (docs/GAMES.md, phase 5 step 4).\n";
            return played && end.area_id == gateway_route_stops_on_map;
        }
        return played && end.dialogue.find("You have won.") != std::string::npos &&
               game_won_shown && all_alive;
    }
    // OPENGOLD_CURSE_MAP walks that map alone.
    const char *only = std::getenv("OPENGOLD_CURSE_MAP");
    play("opening");
    if (!only)
    {
        walk(session.snapshot().area_id);
    }
    for (const auto &[map, script] : opengold::test::TourAccess::area_scripts(session))
    {
        if ((!only || std::to_string(map) == only) && restart(map))
        {
            walk(map);
        }
    }
    std::cout << game_title(game) << " walk:\n";
    for (const auto &[message, places] : needs)
    {
        std::cout << "  " << places.size() << "x " << message << " (first at " << places.front()
                  << ")\n";
    }
    for (const auto &[phase, count] : phases)
    {
        std::cout << "  ended " << count << "x in phase " << phase << "\n";
    }
    return needs.empty();
}

// An item type's property bits, byte 14 of ITEMS, as Pool of Radiance's own
// files hold them (OPENGOLD_GAME_DIR).
void item_types_carry_their_properties()
{
    const char *game = std::getenv("OPENGOLD_GAME_DIR");
    if (!game)
    {
        return;
    }
    std::ifstream file(std::filesystem::path(game) / "ITEMS", std::ios::binary);
    const std::vector<std::uint8_t> bytes(std::istreambuf_iterator<char>(file), {});
    const auto types = opengold::por::decode_item_templates(bytes);
    check(types.has_value() && types->size() > 36, "ITEMS decodes");
    namespace bit = opengold::por::item_property;
    const auto &quarrels = (*types)[28], &long_sword = (*types)[36], &darts = (*types)[9];
    check(quarrels.has(bit::fires_quarrels) && quarrels.has(bit::fires_ammunition) &&
              quarrels.has(bit::dexterity_missile),
          "Quarrels: fired, as ammunition, with Dexterity");
    check(long_sword.has(bit::strength_bonus) && !long_sword.has(bit::thrown),
          "A long sword takes Strength");
    check(darts.has(bit::thrown) && darts.rate_of_fire == 6, "Darts are thrown, three a round");
}

// A save names its game; a save made before format 48 is Pool of Radiance's.
void a_save_names_its_game()
{
    const auto rules_file =
        std::filesystem::path(OPENGOLD_SOURCE_DIR) / "data/rules/srd-5.2.1/combat.rules";
    opengold::CampaignParty party(opengold::srd5::load(rules_file));
    const auto module = opengold::srd5::load(rules_file);
    const auto creation_rules = opengold::srd5::character_rules();
    const auto &creation = *creation_rules;
    const auto curse_save =
        opengold::encode_campaign(party, nullptr, "files", GoldBoxGame::curse_of_the_azure_bonds);
    check(curse_save.starts_with("OPENGOLD-CAMPAIGN 50\n"), "Saves are format 50");
    check(opengold::decode_campaign(curse_save, creation, *module, "files", nullptr).game ==
              GoldBoxGame::curse_of_the_azure_bonds,
          "The save keeps its game");
    const auto older = opengold::encode_campaign_in_format(party, nullptr, "files", 47);
    check(opengold::decode_campaign(older, creation, *module, "files", nullptr).game ==
              GoldBoxGame::pool_of_radiance,
          "A format 47 save is Pool of Radiance's");
    rejects(
        [&]
        {
            static_cast<void>(opengold::encode_campaign_in_format(
                party, nullptr, "files", 47, GoldBoxGame::curse_of_the_azure_bonds));
        },
        "Format 47 cannot record another game");
}

} // namespace

int main(int argc, char **argv)
{
    try
    {
        if (argc == 2 && std::string_view(argv[1]) == "--curse-walk")
        {
            return walk_census(GoldBoxGame::curse_of_the_azure_bonds, false) ? 0 : 1;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--curse-route")
        {
            return walk_census(GoldBoxGame::curse_of_the_azure_bonds, true) ? 0 : 1;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--secret-walk")
        {
            return walk_census(GoldBoxGame::secret_of_the_silver_blades, false) ? 0 : 1;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--gateway-walk")
        {
            return walk_census(GoldBoxGame::gateway_to_the_savage_frontier, false) ? 0 : 1;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--darkness-walk")
        {
            return walk_census(GoldBoxGame::pools_of_darkness, false) ? 0 : 1;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--treasure-walk")
        {
            return walk_census(GoldBoxGame::treasures_of_the_savage_frontier, false) ? 0 : 1;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--secret-route")
        {
            return walk_census(GoldBoxGame::secret_of_the_silver_blades, true) ? 0 : 1;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--darkness-route")
        {
            return walk_census(GoldBoxGame::pools_of_darkness, true) ? 0 : 1;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--gateway-route")
        {
            return walk_census(GoldBoxGame::gateway_to_the_savage_frontier, true) ? 0 : 1;
        }
        each_game_is_told_apart_by_its_files();
        curse_creatures_are_read_from_their_layout();
        curse_scripts_run_in_their_own_bank();
        the_machine_runs_curse_scripts();
        curse_script_memory_is_pool_of_radiance_moved();
        engine_routines_are_the_games();
        the_loaders_read_curse();
        a_save_names_its_game();
        item_types_carry_their_properties();
        curse_opening_reaches_the_first_map();
        secret_opening_reaches_the_first_map();
        the_font_has_eight_bytes_a_glyph();
        std::cout << "Gold Box games passed.\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
