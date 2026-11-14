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

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
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
        return route;
    }();
    return beats;
}

const std::vector<RouteBeat> &route_for(GoldBoxGame game)
{
    return game == GoldBoxGame::secret_of_the_silver_blades ? secret_route() : curse_route();
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
}

bool walk_census(GoldBoxGame game, bool route)
{
    using opengold::por::TourPhase;
    const char *games = std::getenv("OPENGOLD_GOLDBOX_DIR");
    const auto folder = games
                            ? std::filesystem::path(games) /
                                  (game == GoldBoxGame::secret_of_the_silver_blades      ? "SECRET"
                                   : game == GoldBoxGame::gateway_to_the_savage_frontier ? "GATEWAY"
                                                                                         : "CURSE")
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
            // types; a cleric's mace; a magic-user's dagger. Secret numbers
            // its types as Pool of Radiance does, not as Curse.
            const bool secret = game == GoldBoxGame::secret_of_the_silver_blades;
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
    const bool trace = route || std::getenv("OPENGOLD_CURSE_TRACE");
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
    const auto fight = [&]
    {
        const auto before = party->state();
        const auto score = [](const opengold::rules::Snapshot &result)
        {
            int lost = result.outcome == opengold::rules::Outcome::victory ? 0 : 1'000'000;
            for (const auto &unit : result.combatants)
            {
                if (unit.side == opengold::rules::Side::party)
                {
                    lost +=
                        (unit.dead ? 1000 : 0) + unit.max_hit_points - std::max(unit.hit_points, 0);
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
        auto result = play_try(0);
        auto best = score(result);
        while (route && best > 0 && ++last < 120)
        {
            party->restore(before);
            result = play_try(last);
            if (score(result) < best)
            {
                best = score(result);
                best_try = last;
            }
        }
        if (last != best_try && route)
        {
            party->restore(before);
            result = play_try(best_try);
        }
        if (test_winning && result.outcome != opengold::rules::Outcome::victory)
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
            std::cout << "  the test wins the fight\n";
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
        for (int step = 0; step < 400; ++step)
        {
            const auto &s = session.snapshot();
            if (!s.diagnostic.empty())
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
                if (!answers.empty())
                {
                    const auto found = std::ranges::find_if(
                        s.choices,
                        [&](const std::string &choice)
                        {
                            return choice.find(answers.front()) != std::string::npos;
                        });
                    if (found != s.choices.end())
                    {
                        pick = static_cast<std::size_t>(found - s.choices.begin());
                        answers.erase(answers.begin());
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
                if (session.shop_stock().empty())
                {
                    needs["A shop with nothing to sell"].push_back(where);
                }
                ++phases["shops with goods"];
                (void)session.leave_shop(s.continue_ticket);
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
                if (!session.pending_encounter() || !fight())
                {
                    ++phases["fight not resolved"];
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
        const auto party_whole = [&]
        {
            return std::ranges::all_of(
                party->state().slots,
                [&](opengold::MemberId id)
                {
                    return !id || party->member(id).vitals.dead ||
                           party->member(id).vitals.hit_points >=
                               party->member(id).character.sheet().hit_points;
                });
        };
        const auto rest_until_healed = [&]
        {
            for (int day = 0; day < 90 && session.can_leave(); ++day)
            {
                if (party_whole() || !session.camp(opengold::RestKind::long_rest))
                {
                    break;
                }
                // The spells are chosen as the rest ends, as the rest dialog
                // has it, before a camp's interruption or the map's script.
                memorize();
                play("camp");
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
            for (const auto &beat : route_for(game))
            {
                ++beat_number;
                const auto where = "beat " + std::to_string(beat.map) + " " +
                                   std::to_string(beat.x) + "," + std::to_string(beat.y);
                current_beat = where + " (beat " + std::to_string(beat_number) + ")";
                if (session.snapshot().phase == TourPhase::defeated)
                {
                    break;
                }
                if (beat.rest_first)
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
                    test_winning = beat.test_wins;
                    if (beat.waiting)
                    {
                        play(where);
                    }
                    else if (beat.enter_map)
                    {
                        opengold::test::TourAccess::enter(session, beat.map,
                                                          {beat.x, beat.y, beat.facing});
                        play(where);
                    }
                    else
                    {
                        const unsigned x = (beat.x + static_cast<unsigned>(round)) % 16;
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
                        play("survey " + std::to_string(map) + " " + std::to_string(x) + "," +
                             std::to_string(y));
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
        // The route's last beat wins the game: Tyranthraxus falls, and the
        // ending is told.
        const bool all_alive = std::ranges::none_of(party->state().slots,
                                                    [&](opengold::MemberId id)
                                                    {
                                                        return id && party->member(id).vitals.dead;
                                                    });
        return needs.empty() && end.phase != TourPhase::defeated &&
               end.dialogue.find("You have won.") != std::string::npos && game_won_shown &&
               all_alive;
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
        if (argc == 2 && std::string_view(argv[1]) == "--secret-route")
        {
            return walk_census(GoldBoxGame::secret_of_the_silver_blades, true) ? 0 : 1;
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
