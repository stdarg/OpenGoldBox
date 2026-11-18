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
#include "original_creation_support.h"
#include "test_support.h"

#include <cstdio>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace
{
// The spells a player picks first where creation offers a choice
// (Treasures' magic-user's first three levels).
constexpr std::array<std::string_view, 10> learned_first{
    "magic_missile", "sleep",     "shield", "stinking_cloud", "invisibility",
    "fireball",      "lightning", "haste",  "hold_person",    "slow"};
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

// Curse's explosive runes use selected-member DAMAGE 90h/84h. Its original
// CMD_Damage keeps the full damage on a save with bit 10h; without that bit,
// value's low three bits minus one select the saving throw category.
void curse_selected_damage()
{
    using namespace opengold;
    using namespace opengold::por;
    constexpr auto game = GoldBoxGame::curse_of_the_azure_bonds;
    const std::shared_ptr<const rules::CharacterRules> creation =
        original::character_rules(original::default_aging, game);
    original::Content content;
    content.game = game;
    auto party = std::make_shared<CampaignParty>(original::rules_module(content), creation);
    for (unsigned n = 0; n < 2; ++n)
    {
        CharacterCreator creator(creation, 40 + n);
        creator.select(rules::CreationField::race, "human");
        creator.select(rules::CreationField::character_class, "fighter");
        creator.name("Rune target " + std::to_string(n));
        test::walk_original_creation(creator);
        test::modify_to_the_highest(*party, party->add_pc(creator.create_character()));
    }
    const auto initial = party->checkpoint();
    bool saved = false, failed = false;
    for (const auto attack : {0x80, 0x90, 0xA0, 0xC0, 0xD0, 0xE0})
    {
        for (unsigned seed = 1; seed <= 20; ++seed)
        {
            auto state = initial;
            state.random_state = RandomState{seed};
            party->restore(state);
            const auto target = state.slots[1];
            rules::HazardAttack harm{0, 0, 4, 7, {}};
            harm.automatic = true;
            const bool whole_party = (attack & 0x40) != 0;
            if (attack == 0x80 || attack == 0xC0)
            {
                harm.original_save = whole_party ? 0 : 3; // Chosen 84h means 4 minus one.
            }
            if (whole_party)
            {
                (void)party->hazard_attack(state.slots[0], harm);
            }
            const auto expected = party->hazard_attack(target, harm);
            check(expected.has_value(), "The runes have a healthy selected target");
            const auto expected_hp = party->member(target).vitals.hit_points;
            const auto expected_other_hp = party->member(state.slots[0]).vitals.hit_points;
            if (attack == 0x80)
            {
                saved = saved || expected->saved;
                failed = failed || !expected->saved;
            }
            party->restore(state);
            std::vector<std::uint8_t> record{0, 0};
            // Keep the load entry empty; look runs the synthetic DAMAGE event.
            record.insert(record.end(), {1, 1, 0x14, 0x80});
            for (int entry = 1; entry < 5; ++entry)
            {
                record.insert(record.end(), {1, 1, 0x15, 0x80});
            }
            record.insert(record.end(), {0, 10, 0, 1, 46, 0, static_cast<std::uint8_t>(attack), 0,
                                         0, 0, 4, 0, 7, 0,
                                         static_cast<std::uint8_t>(whole_party      ? 0
                                                                   : attack == 0xA0 ? 0x80
                                                                                    : 0x84),
                                         0});
            auto program = std::make_shared<const EclProgram>(
                EclProgram::decode(record, "Curse explosive runes", game));
            RolfTourSession session({}, program, {}, 0x8014, {},
                                    std::make_shared<PhlanResources>());
            session.campaign_party(party);
            const auto settle = [&]
            {
                for (int n = 0; n < 50 && session.snapshot().phase == TourPhase::running; ++n)
                {
                    session.advance(1.0);
                }
            };
            settle();
            check(session.explore(ExplorationCommand::look), "The rune event starts");
            settle();
            check(session.snapshot().phase == TourPhase::awaiting_continue &&
                      session.snapshot().diagnostic.empty() &&
                      session.snapshot().dialogue.find("rolled back") == std::string::npos,
                  "Selected DAMAGE awaits acknowledgement: " + session.snapshot().dialogue +
                      session.snapshot().diagnostic);
            check(session.continue_dialogue(session.snapshot().continue_ticket),
                  "Acknowledge the damage");
            settle();
            check(session.snapshot().phase == TourPhase::completed &&
                      party->member(target).vitals.hit_points == expected_hp &&
                      party->member(state.slots[0]).vitals.hit_points == expected_other_hp,
                  "DAMAGE matches original saves and selected/whole-party targets");
        }
    }
    check(saved && failed, "The regression exercises successful and failed original saves");
}

// A camp cure changes the live party while the script retains its selected
// record. The next LOAD CHARACTER must not undo a spell already consumed.
void curse_camp_healing_survives_script_reads()
{
    using namespace opengold;
    using namespace opengold::por;
    constexpr auto game = GoldBoxGame::curse_of_the_azure_bonds;
    const std::shared_ptr<const rules::CharacterRules> creation =
        original::character_rules(original::default_aging, game);
    original::Content content;
    content.game = game;
    auto party = std::make_shared<CampaignParty>(original::rules_module(content), creation);
    CharacterCreator creator(creation, 42);
    creator.select(rules::CreationField::race, "human");
    creator.select(rules::CreationField::character_class, "cleric");
    creator.name("Camp healer");
    test::walk_original_creation(creator);
    const auto id = party->add_pc(creator.create_character());
    test::modify_to_the_highest(*party, id);
    check(static_cast<bool>(party->rest()), "The cleric rests to prepare a cure");
    party->choose_spells(id, {{}, std::vector<std::string>{"cleric:cure_light_wounds"}});
    rules::HazardAttack hurt{0, 0, 1, 12, {}};
    hurt.automatic = true;
    (void)party->hazard_attack(id, hurt);
    const auto damaged = party->member(id).vitals.hit_points;
    std::vector<std::uint8_t> record{0, 0, 1, 1, 0x14, 0x80};
    for (int entry = 1; entry < 5; ++entry)
    {
        record.insert(record.end(), {1, 1, 0x15, 0x80});
    }
    // EXIT for loading; LOAD 0, one-choice menu, LOAD 0, EXIT for looking.
    record.insert(record.end(),
                  {0, 10, 0, 0, 43, 1, 0, 0x40, 0, 1, 128, 3, 4, 32, 192, 10, 0, 0, 0});
    auto program =
        std::make_shared<const EclProgram>(EclProgram::decode(record, "Curse camp cure", game));
    RolfTourSession session({}, program, {}, 0x8014, {}, std::make_shared<PhlanResources>());
    session.campaign_party(party);
    const auto settle = [&]
    {
        for (int n = 0; n < 50 && session.snapshot().phase == TourPhase::running; ++n)
        {
            session.advance(1.0);
        }
    };
    settle();
    test::TourAccess::set_variable(session, EclAddress{0x4000}, 0);
    check(session.explore(ExplorationCommand::look), "The script loads its selected character");
    settle();
    check(session.snapshot().phase == TourPhase::awaiting_continue &&
              session.snapshot().choices.size() == 1,
          "The character record is retained across a paused journey-style menu");
    session.cast_outside_combat(id, id, "cast:cleric:cure_light_wounds");
    const auto healed = party->member(id).vitals.hit_points;
    check(healed > damaged && party->memorized_spells(id).empty(),
          "The actual cure heals and is consumed");
    check(session.choose(session.snapshot().continue_ticket, 0),
          "Resume the menu and read the character again");
    settle();
    check(session.snapshot().phase == TourPhase::completed &&
              party->member(id).vitals.hit_points == healed && party->memorized_spells(id).empty(),
          "Cast healing survives the next script character read");
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

void shared_map_saves_reload(GoldBoxGame game)
{
    using namespace opengold;
    const auto *games = std::getenv("OPENGOLD_GOLDBOX_DIR");
    if (!games)
    {
        std::cout << game_title(game) << " files are not here; shared-map saves are not checked.\n";
        return;
    }
    const bool curse = game == GoldBoxGame::curse_of_the_azure_bonds;
    const auto folder = std::filesystem::path(games) / (curse ? "CURSE" : "SECRET");
    const auto content = load_original_content(folder, game);
    const std::shared_ptr<const rules::CharacterRules> creation =
        original::character_rules(original::default_aging, game);
    const auto module = original::rules_module(content);
    auto party = std::make_shared<CampaignParty>(original::rules_module(content), creation);
    CharacterCreator creator(creation, 42);
    creator.select(rules::CreationField::race, "human");
    creator.select(rules::CreationField::character_class, "fighter");
    creator.name("Shared-map explorer");
    test::walk_original_creation(creator);
    party->add_pc(creator.create_character());
    auto session = load_campaign(folder, game);
    session.campaign_party(party);
    for (int n = 0; n < 200 && !session.can_leave(); ++n)
    {
        if (session.snapshot().phase == por::TourPhase::running)
        {
            session.advance(1.0);
        }
        else if (session.snapshot().phase == por::TourPhase::awaiting_continue)
        {
            check(session.choose(session.snapshot().continue_ticket, 0),
                  "Finish the shared-map fixture opening");
        }
        else if (session.snapshot().phase == por::TourPhase::treasure)
        {
            check(session.leave_treasure(session.snapshot().continue_ticket),
                  "Leave the opening treasure in this save fixture");
        }
    }
    check(session.can_leave(), "The opening finishes before entering the save fixture");
    const auto layouts = curse ? std::array<std::pair<unsigned, unsigned>, 2>{{{18, 17}, {33, 32}}}
                               : std::array<std::pair<unsigned, unsigned>, 2>{{{49, 48}, {50, 48}}};
    for (const auto &[script, map] : layouts)
    {
        if (!curse)
        {
            test::TourAccess::set_variable(session, por::EclAddress{0x4C69}, script == 49 ? 1 : 5);
            test::TourAccess::set_variable(session, por::EclAddress{0x4C6E}, 1);
        }
        test::TourAccess::enter_script(session, script, map,
                                       {script == 18 ? 15U : 7U, 14, por::MapDirection::south});
        for (int n = 0; n < 100 && !session.can_leave(); ++n)
        {
            if (session.snapshot().phase == por::TourPhase::running)
            {
                session.advance(1.0);
            }
            else if (session.snapshot().phase == por::TourPhase::awaiting_continue)
            {
                check(session.choose(session.snapshot().continue_ticket, 0),
                      "Finish the shared-map entry");
            }
        }
        check(session.can_leave() && session.snapshot().script_id == script &&
                  session.snapshot().area_id == map,
              "The shared script retains its original geometry");
        const auto bytes = encode_campaign(*party, &session, "shared-map-save", game);
        const auto restored =
            decode_campaign(bytes, *creation, *module, "shared-map-save", &session);
        check(restored.town && restored.town->snapshot().script_id == script &&
                  restored.town->snapshot().area_id == map &&
                  restored.town->snapshot().pose == session.snapshot().pose &&
                  restored.party.slots == party->state().slots,
              "A saved party reloads with its shared script, map and position");
    }
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
    // Reload before the whole event, so a chained fight can start again
    // with a different result from its first battle.
    bool reload_event = false;
    bool keep_preparation = false;
    std::string cast_after_by;
    // Physical walking already resolves arrival. Do not add a ten-minute
    // Look when a cleared-room waypoint needs no further command.
    bool arrive_only = false;
    std::vector<std::pair<std::string, std::string>> ready_names;
};

// The dice a beat starts with, for a route that gives each beat its own (Pools
// of Darkness's): from where the beat plays, what it answers and how many like
// it came before. One stream ran the whole route, so an engine change that
// rolled a die more or less anywhere reshuffled every beat after it; with a
// seed a beat, such a change reshuffles only the beats it touches, as a player
// reloading a save before each step gets fresh dice (reseed_after_load). The
// dice stay as fair as before.
std::uint64_t beat_dice_seed(const RouteBeat &beat, unsigned occurrence)
{
    // FNV-1a over the beat's words, each ended by a separator.
    std::uint64_t hash = 0xcbf29ce484222325ULL;
    const auto mix = [&hash](std::string_view text)
    {
        for (const char letter : text)
        {
            hash = (hash ^ static_cast<unsigned char>(letter)) * 0x100000001b3ULL;
        }
        hash = (hash ^ 0xFFU) * 0x100000001b3ULL;
    };
    mix(std::to_string(beat.map));
    mix(std::to_string(beat.x));
    mix(std::to_string(beat.y));
    mix(std::to_string(static_cast<int>(beat.facing)));
    for (const auto &answer : beat.answers)
    {
        mix(answer);
    }
    for (const auto step : beat.area_steps)
    {
        mix(std::to_string(static_cast<int>(step)));
    }
    mix(std::to_string(occurrence));
    if (beat.reloads != 0)
    {
        mix("reload " + std::to_string(beat.reloads));
    }
    return hash;
}

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
        // them) the barn holds two of each kind, not six. The party rests
        // first: a camp in Hap draws the patrols it hunts anyway.
        {50,
         0,
         0,
         D::north,
         {"Fight"},
         true,
         false,
         false,
         std::nullopt,
         0,
         0,
         std::pair<std::uint16_t, std::uint16_t>{0x4C47, 4}},
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
        {50, 7, 12, D::north, {"PARLAY WITH THE DRAGONS", "NICE"}, true},
        // Dracandros in the tower's courtyard; among his treasures, the Helm
        // of Dragons (cell 0x4C59): the second bond is gone. The party takes
        // the magic he leaves: Bracers AC 2 and a Ring of Wizardry for the
        // magic-user, a Ring of Protection +3, and his wands (Ice Storm and
        // Fireballs).
        []
        {
            RouteBeat dracandros{51, 4, 13, D::north, {}, true};
            dracandros.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x4C59, 1};
            dracandros.take_items = {{"BRACERS", "magic_user 6"},
                                     {"RING (", "paladin 1"},
                                     {"RING (", "magic_user 6"},
                                     {"WAND (", "magic_user 6"},
                                     {"WAND (", "magic_user 6"}};
            return dracandros;
        }(),
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
        // spies; the commander thinks well of a party that fights them. The
        // party takes their magic broad swords, splint mail and mace.
        []
        {
            RouteBeat spies{16, 0, 3, D::east, {"FIGHT", "HAUGHTY"}};
            spies.step = true;
            spies.take_items = {{"BROAD SWORD (", "paladin 1"},
                                {"BROAD SWORD (", "paladin 2"},
                                {"BROAD SWORD (", "ranger 3"},
                                {"SPLINT", "cleric 4"},
                                {"MACE (", "cleric 5"}};
            return spies;
        }(),
        // The Red Plumes' barracks (event 25), where they point out a spot
        // the party can rest: elsewhere in their quarters a camp is refused
        // ("NO SLEEPING HERE"), and in the streets wandering monsters break
        // it (ECL3.DAX 16's camp entries).
        {16, 5, 2, D::north, {}},
        // The shambling mounds over a dead cleric, who holds the Wand of
        // Defoliation the Pit of Moander calls for (readied), and a Wand of
        // Lightning, rested at the barracks.
        []
        {
            RouteBeat mounds{16, 11, 8, D::north, {"Fight"}, true};
            mounds.take_wand = true;
            mounds.take_items = {{"WAND (", "magic_user 6"}};
            return mounds;
        }(),
        // Into the Pit of Moander, rested at the barracks again.
        {16, 5, 2, D::north, {}},
        {16,
         11,
         0,
         D::north,
         {},
         true,
         false,
         false,
         std::nullopt,
         0,
         0,
         std::nullopt,
         false,
         true},
        // Alias and Dragonbait, who join a party that tells its story.
        {17, 1, 4, D::north, {"PARLAY", "NICE", "TELL HER YOUR STORY", "YES"}},
        // Down the stairs to the Pit's second level.
        {17, 15, 11, D::north, {"YES"}},
        // Mogion before her altar (ECL3.DAX 18, event 12): the bonds hold the
        // party while her cultists draw Moander through the rift, until the
        // bond of Moander fades and Alias and Dragonbait hack free. The party
        // attacks Mogion, six cultists and five shambling mounds; as the rift
        // snaps shut three pseudopods of Moander (140 Hit Points each) attack
        // at once, and in their remains the party finds the Gauntlet of
        // Moander (cell 0x4C5B). The magic-user readies the Wand of
        // Defoliation for the mounds first; the party takes the magic the
        // cult leaves: plate armor +3, a shield +2, a mace +3 and a Cloak of
        // Displacement. The pseudopods win most plays (8% of the party's
        // win), so a player reloads until it wins.
        []
        {
            RouteBeat mogion{17, 7, 1, D::north, {"ATTACK"}};
            mogion.reloads_until_won = 240;
            mogion.ready_types = {{79, "magic_user 6"}};
            mogion.take_items = {{"PLATE", "paladin 1"},
                                 {"SHIELD (", "paladin 2"},
                                 {"MACE (", "cleric 4"},
                                 {"CLOAK", "ranger 3"}};
            return mogion;
        }(),
        // Back up the stairs to the Pit's first level (not down again).
        {17, 15, 14, D::north, {"YES", "NO"}},
        // The cultists' last stand by the Pit's way out (ten cultists, five
        // large vegepygmies, five shambling mounds), and out of the Pit by its
        // door to the west.
        {17, 12, 4, D::west, {}, true},
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
        // Dexam's men fight Fzoul's on the way, and the party takes the
        // plate armor +1 they leave.
        []
        {
            RouteBeat woman{32, 2, 3, D::north, {}};
            woman.take_items = {
                {"PLATE", "paladin 2"}, {"PLATE", "ranger 3"}, {"PLATE", "cleric 5"}};
            return woman;
        }(),
        {32, 2, 3, D::north, {"YES"}},
        // Dexam the Beholder and his minions, for the Amulet of Lathander
        // (map 33's events 15 and 16; the "beholder" at 2,1 is a gas spore).
        // His lair meets the party only while cell 0x4C03 is 0: the hooded
        // woman set it to 1 (ECL4.DAX 33 0x8832), and the caves' new script
        // cleared it again, as the original's EclInit clears 4C00h to 4C1Fh
        // at each NEW ECL.
        {33, 2, 11, D::north, {}, true},
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
         false,
         true},
        // With the three artifacts, rested in camp at the edge of Zhentil Keep
        // (a camp in the caves draws their wandering monsters night after
        // night), back by Yulash and Hillsfar to the Standing Stones, where
        // the grey man reveals himself as Tyranthraxus and bids the party
        // meet him at Myth Drannor, and on there.
        {0, 0, 0, D::north, {"JOURNEY", "YULASH", "TRAIL", "stop:JOURNEY"}, true, false, true},
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
        // Rested in the ruins first (the parley keeps the rakshasas' patrols
        // away, as the cluebook says): no camp rests in the temple ("YOUR
        // BANDS WILL NOT LET YOU REST", ECL6.DAX 67).
        {66, 5, 6, D::north, {"YES", "YES"}, true},
        // In the temple's chapel Tyranthraxus takes the bonded party, Nameless
        // frees it and dies, and Tyranthraxus leaves his minions to it; then
        // up the main hall's stairs to the second floor. The party takes the
        // plate armor +2 the minions leave.
        []
        {
            RouteBeat chapel{67, 5, 11, D::north, {}};
            chapel.take_items = {{"PLATE", "paladin 2"}, {"PLATE", "ranger 3"}};
            return chapel;
        }(),
        {67, 10, 7, D::north, {"YES"}},
        // Tyranthraxus, with high priests and margoyles, on the second floor:
        // the final battle, the clerics' cures cast first on the hurt.
        []
        {
            RouteBeat finale{67, 6, 1, D::north, {}};
            finale.cure_first = true;
            finale.cure_to_full = true;
            finale.reloads_until_won = 240;
            return finale;
        }(),
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

// Steps through a map as walk_passages does, but without stopping to rest
// and taking the first choice where no answer is given: Secret's castle,
// whose guards break every camp, which a hurt party crosses to a gateway to
// the Well, where it can rest.
void walk_unrested(
    std::vector<RouteBeat> &beats, unsigned map,
    std::initializer_list<std::tuple<unsigned, unsigned, opengold::por::MapDirection>> passages,
    const std::vector<std::string> &answers)
{
    for (const auto &[x, y, facing] : passages)
    {
        beats.push_back(step_beat(map, x, y, facing, answers));
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

// From (10,4) of Pools of Darkness's Temple of Tyr (map 50) the party steps
// west into the scribe's office (9,4), rests there until whole, and steps
// back: the one room whose camps the temple's patrols leave alone.
void rest_in_scribes_office(std::vector<RouteBeat> &beats)
{
    using D = opengold::por::MapDirection;
    beats.push_back(step_beat(50, 10, 4, D::west));
    RouteBeat office{50, 9, 4, D::west, {}};
    office.rest_first = true;
    beats.push_back(office);
    beats.push_back(step_beat(50, 9, 4, D::east));
}

// Secret's route plays to the game's end, the soul gem shattered (since
// 2026-11-14; docs/GAMES.md, phase 4 step 4); were this false, it would stop
// on this map.
constexpr bool secret_route_ends_the_game = true;
constexpr unsigned secret_route_stops_on_map = 98;

// The beat on this square of the map (its `occurrence`th, counting from 1)
// takes these items from the treasures it wins (RouteBeat::take_items).
void take_at(std::vector<RouteBeat> &route, std::tuple<unsigned, unsigned, unsigned> square,
             unsigned occurrence, std::vector<std::pair<std::string, std::string>> items)
{
    for (auto &beat : route)
    {
        if (std::tuple{beat.map, beat.x, beat.y} == square && --occurrence == 0)
        {
            beat.take_items = std::move(items);
            return;
        }
    }
    throw std::logic_error("No beat takes the items on its square");
}

// Secret's mirror (its item type 48), which turns gazes back, and its
// shield (37).
constexpr int mirror_type = 48, shield_type = 37;

// What the route's party takes from the mayor's pile (item list 18), as a
// player shares it out: each item by its name and the member who readies it.
std::vector<std::pair<std::string, std::string>> secret_town_gear()
{
    return {{"Shield +2", "paladin 1"},
            {"Gauntlets", "paladin 2"},
            {"Wand of Ice Storm", "magic_user 6"},
            {"Bracers", "magic_user 6"}};
}

enum class SecretProgress
{
    none,
    opening,
    well,
    eldamar,
    headquarters,
    first_staff,
    staff
};

std::vector<RouteBeat> secret_earned_opening()
{
    using D = opengold::por::MapDirection;
    std::vector<RouteBeat> beats;
    RouteBeat shop{16,
                   14,
                   7,
                   D::north,
                   {"YES", "sell:LEATHER ARMOR@ranger 3", "sell:SCALE MAIL@cleric 5",
                    "buy:SCALE ARMOR@paladin 2,ranger 3,cleric 4,cleric 5",
                    "buy:BROAD SWORD@ranger 3", "buy:MORNING STAR@cleric 5", "buy:MIRROR"}};
    shop.step = true;
    shop.searching = false;
    beats.push_back(shop);
    RouteBeat prepare{16, 3, 3, D::south, {}, true};
    prepare.arrive_only = true;
    prepare.rest_for_spells = true;
    prepare.cure_first = true;
    prepare.cure_to_full = true;
    prepare.memorize_first_of = {"magic_missile", "fireball",          "bless",
                                 "prayer",        "cure_light_wounds", "cure_serious_wounds"};
    prepare.cast_after = {"bless"};
    beats.push_back(prepare);
    RouteBeat prayer{16, 3, 3, D::south, {}};
    prayer.waiting = true;
    prayer.cast_after = {"prayer"};
    beats.push_back(prayer);
    // This fixed encounter exposes the Black Circle and permits staying
    // in Marcus's guarded corridor. It requires no dragon or mine quest.
    RouteBeat portal{16, 3, 0, D::north, {"YES"}};
    portal.arrive_only = true;
    portal.searching = false;
    beats.push_back(portal);
    RouteBeat exposed{33, 4, 1, D::east, {"ATTACK", "NO"}};
    exposed.arrive_only = true;
    exposed.searching = false;
    exposed.cure_first = true;
    exposed.cure_to_full = true;
    exposed.reloads_until_won = 600;
    exposed.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x4C4D, 128};
    beats.push_back(exposed);
    RouteBeat return_home{33, 1, 1, D::north, {"YES"}};
    return_home.arrive_only = true;
    return_home.searching = false;
    beats.push_back(return_home);
    auto merchant = step_beat(16, 13, 11, D::east, {"YES", "STAY", "NO"});
    merchant.searching = false;
    merchant.reloads_until_won = 600;
    beats.push_back(merchant);
    RouteBeat pets{16, 15, 11, D::north, {}};
    pets.step = true;
    pets.searching = false;
    pets.cure_first = true;
    pets.cure_to_full = true;
    pets.reloads_until_won = 600;
    pets.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x4C2D, 255};
    beats.push_back(pets);
    beats.push_back(
        {16, 10, 8, D::south, {"YES",  "raise:dead",   "stone:flesh", "Heal", "paladin 1", "Pay",
                               "Heal", "paladin 2",    "Pay",         "Heal", "ranger 3",  "Pay",
                               "Heal", "cleric 4",     "Pay",         "Heal", "cleric 5",  "Pay",
                               "Heal", "magic_user 6", "Pay",         "Leave"}});
    beats.back().step = true;
    RouteBeat home{16, 3, 3, D::south, {}, true};
    home.arrive_only = true;
    home.rest_for_spells = true;
    home.cure_first = true;
    home.cure_to_full = true;
    home.memorize_first_of = {"magic_missile", "fireball", "cure_light_wounds",
                              "cure_serious_wounds"};
    beats.push_back(home);
    return beats;
}

const std::vector<RouteBeat> &secret_route();

std::vector<RouteBeat> secret_earned_route(SecretProgress progress)
{
    using D = opengold::por::MapDirection;
    auto beats = secret_earned_opening();
    if (progress < SecretProgress::well)
    {
        return beats;
    }
    RouteBeat prepare{16, 3, 3, D::south, {}, true};
    prepare.arrive_only = true;
    prepare.rest_for_spells = true;
    prepare.cure_first = true;
    prepare.cure_to_full = true;
    prepare.ward_each_with = "resist_fire";
    prepare.memorize_first_of = {"magic_missile", "hold_monster", "bless", "prayer"};
    prepare.cast_after = {"bless"};
    beats.push_back(prepare);
    RouteBeat prayer{16, 3, 3, D::south, {}};
    prayer.waiting = true;
    prayer.cast_after = {"prayer"};
    beats.push_back(prayer);
    RouteBeat portal{16, 3, 0, D::north, {"YES"}};
    portal.arrive_only = true;
    portal.searching = false;
    beats.push_back(portal);
    RouteBeat dragon{33, 7, 6, D::south, {"NO"}};
    dragon.step = true;
    dragon.searching = false;
    dragon.cure_first = true;
    dragon.cure_to_full = true;
    dragon.reloads_until_won = 600;
    dragon.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x4C63, 255};
    beats.push_back(dragon);
    RouteBeat return_home{33, 1, 1, D::north, {"YES"}};
    return_home.arrive_only = true;
    return_home.searching = false;
    beats.push_back(return_home);
    // Reuse the actual temple visit and town camp from the opening.
    const auto opening = secret_earned_opening();
    beats.insert(beats.end(), opening.end() - 2, opening.end());
    if (progress >= SecretProgress::eldamar)
    {
        auto hall = step_beat(16, 7, 7, D::north, train_everyone());
        hall.searching = false;
        beats.push_back(hall);
        beats.push_back(step_beat(16, 15, 8, D::east, {"YES"}));
        walk_ruins(beats,
                   {{14, 2, D::east},  {15, 5, D::east},  {10, 2, D::south}, {5, 0, D::north},
                    {10, 4, D::north}, {0, 11, D::south}, {2, 10, D::west},  {0, 6, D::south},
                    {14, 2, D::east},  {13, 4, D::west},  {6, 0, D::west},   {1, 14, D::south},
                    {3, 0, D::north},  {0, 4, D::west},   {3, 5, D::south},  {7, 11, D::south},
                    {8, 12, D::north}, {1, 9, D::south},  {2, 10, D::west},  {0, 6, D::south},
                    {0, 5, D::east},   {13, 4, D::west},  {1, 1, D::west},   {5, 0, D::north}});
        auto lair = step_beat(32, 0, 3, D::north, {"ATTACK", "YES"});
        lair.rest_first = true;
        lair.rest_for_spells = true;
        lair.cure_first = true;
        lair.cure_to_full = true;
        lair.ward_each_with = "resist_fire";
        lair.memorize_first_of = {"magic_missile", "hold_monster", "bless", "prayer"};
        // Select the visible, unidentified treasure by its ordinary labels.
        lair.take_items = {{"Cloak", "magic_user 6"},
                           {"Ring", "cleric 5"},
                           {"Plate Mail", "paladin 2"},
                           {"Long Sword", "ranger 3"}};
        lair.reloads_until_won = 600;
        lair.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x4C53, 255};
        beats.push_back(lair);
        RouteBeat recover{33, 1, 1, D::north, {"NO"}, true};
        recover.arrive_only = true;
        recover.rest_for_spells = true;
        recover.cure_first = true;
        recover.cure_to_full = true;
        recover.memorize_first_of = {"magic_missile", "fireball", "cure_light_wounds",
                                     "cure_serious_wounds"};
        beats.push_back(recover);
    }
    if (progress >= SecretProgress::headquarters)
    {
        const auto first_hq_beat = beats.size();
        RouteBeat prepare{33, 1, 1, D::north, {"NO"}, true};
        prepare.arrive_only = true;
        prepare.rest_for_spells = true;
        prepare.cure_first = true;
        prepare.cure_to_full = true;
        prepare.memorize_first_of = {"magic_missile", "fireball", "hold_monster", "bless",
                                     "prayer"};
        prepare.cast_after = {"bless"};
        beats.push_back(prepare);
        RouteBeat prayer{33, 1, 1, D::north, {}};
        prayer.waiting = true;
        prayer.cast_after = {"prayer"};
        beats.push_back(prayer);
        auto leave_well = step_beat(33, 0, 0, D::north);
        leave_well.searching = false;
        beats.push_back(leave_well);
        walk_ruins(beats, {{2, 0, D::north},
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
        beats.push_back(step_beat(32, 0, 2, D::south, {"ATTACK"}));
        beats.push_back({34, 12, 6, D::north, {}});
        beats.back().arrive_only = true;
        beats.push_back({34, 10, 5, D::north, {}});
        beats.back().arrive_only = true;
        beats.push_back({34, 10, 6, D::north, {"YES"}});
        beats.back().arrive_only = true;
        // The commander fights can earn another mage level. Use the real
        // town portal and free hall before the final headquarters approach.
        beats.push_back({33, 1, 1, D::north, {"YES"}, true});
        beats.back().arrive_only = true;
        beats.back().rest_for_spells = true;
        beats.push_back(step_beat(16, 7, 7, D::north, train_everyone()));
        beats.push_back({16, 3, 0, D::north, {"YES"}});
        beats.back().arrive_only = true;
        RouteBeat return_hq{33, 14, 14, D::north, {"YES"}, true};
        return_hq.rest_for_spells = true;
        return_hq.cure_first = true;
        return_hq.cure_to_full = true;
        return_hq.ward_memorized = "resist_fire";
        return_hq.memorize_first_of = prepare.memorize_first_of;
        beats.push_back(return_hq);
        // Look searches this square; do not search on every approach step.
        beats.push_back({34, 8, 4, D::south, {}});
        beats.push_back(step_beat(34, 8, 4, D::south));
        beats.push_back(step_beat(34, 8, 7, D::south, {"Bash", "Bash", "Bash", "Bash"}));
        RouteBeat late_wards{34, 8, 8, D::south, {}};
        late_wards.waiting = true;
        late_wards.ward_each_with = "resist_fire";
        late_wards.cast_after = {"bless"};
        beats.push_back(late_wards);
        auto renewed_prayer = prayer;
        renewed_prayer.map = 34;
        renewed_prayer.x = 8;
        renewed_prayer.y = 8;
        beats.push_back(renewed_prayer);
        const auto town_recovery = [&](bool identify = false)
        {
            beats.push_back({33, 1, 1, D::north, {"YES"}});
            beats.back().arrive_only = true;
            beats.push_back(opening[opening.size() - 2]);
            beats.push_back(opening.back());
            beats.push_back(step_beat(16, 7, 7, D::north, train_everyone()));
            if (identify)
            {
                beats.push_back(
                    step_beat(16, 14, 7, D::north,
                              {"YES", "identify:GIRDLE@paladin 1", "identify:BRACERS@magic_user 6",
                               "identify:WAND@magic_user 6"}));
            }
            beats.push_back({16, 3, 0, D::north, {"YES"}});
            beats.back().arrive_only = true;
        };
        for (const auto &[x, y] : {std::pair{1U, 13U}, std::pair{7U, 9U}, std::pair{9U, 15U}})
        {
            beats.push_back({34, x, y, D::south, {"CONTINUE"}});
            beats.back().arrive_only = true;
            beats.push_back({34, 10, 6, D::north, {"YES"}});
            beats.back().arrive_only = true;
            town_recovery();
            beats.push_back(return_hq);
            beats.push_back({34, 8, 4, D::south, {}});
            beats.push_back(step_beat(34, 8, 4, D::south));
            beats.push_back(step_beat(34, 8, 7, D::south, {"Bash", "Bash", "Bash", "Bash"}));
            beats.push_back(late_wards);
            beats.push_back(renewed_prayer);
        }
        beats.push_back({34, 13, 13, D::south, {"GO FORWARD"}});
        beats.back().arrive_only = true;
        auto sanctum = RouteBeat{34, 13, 14, D::south, {"ATTACK", "YES"}};
        sanctum.arrive_only = true;
        sanctum.cure_first = true;
        sanctum.cure_to_full = true;
        sanctum.reloads_until_won = 1200;
        sanctum.take_items = {
            {"Girdle", "paladin 1"}, {"Bracers", "magic_user 6"}, {"Wand", "magic_user 6"}};
        sanctum.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x4C98, 255};
        beats.push_back(sanctum);
        town_recovery(true);
        auto recover = prepare;
        recover.memorize_first_of = {"magic_missile", "fireball", "cure_light_wounds",
                                     "cure_serious_wounds"};
        recover.cast_after.clear();
        recover.ready_names = {{"Girdle of Giant Strength", "paladin 1"},
                               {"Bracers AC 3", "magic_user 6"},
                               {"Wand of Lightning", "magic_user 6"}};
        beats.push_back(recover);
        for (auto at = first_hq_beat; at < beats.size(); ++at)
        {
            auto &beat = beats[at];
            if (!beat.searching)
            {
                beat.searching = false;
            }
            beat.cure_first = true;
            beat.cure_to_full = true;
            beat.reloads_until_won = std::max(beat.reloads_until_won, 600U);
        }
    }
    if (progress >= SecretProgress::first_staff)
    {
        const auto first_mine_beat = beats.size();
        auto leave_well = step_beat(33, 0, 0, D::north);
        leave_well.rest_first = true;
        leave_well.rest_for_spells = true;
        beats.push_back(leave_well);
        walk_ruins(beats, {{2, 0, D::north},
                           {6, 0, D::west},
                           {1, 2, D::west},
                           {1, 14, D::south},
                           {7, 11, D::south},
                           {2, 10, D::west},
                           {1, 14, D::south}});
        beats.push_back({32, 3, 12, D::north, {"YES"}});
        beats.back().arrive_only = true;
        beats.push_back(step_beat(49, 3, 6, D::south, {"ATTACK"}));
        beats.push_back(step_beat(49, 5, 15, D::south, {"ENTER"}));
        auto derf = step_beat(50, 14, 3, D::north, {"TALK", "NO"});
        derf.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x4C85, 1};
        beats.push_back(derf);
        // Derf shoos the party south. Leave by the outer temple aisle;
        // the shorter geometric path repeatedly reenters his chamber.
        beats.push_back({50, 11, 11, D::north, {}});
        beats.back().arrive_only = true;
        // Reuse the existing contiguous passage/digging plan after Derf,
        // but walk every square rather than placing the party on it.
        const auto &story = secret_route();
        const auto ring =
            std::ranges::find_if(story,
                                 [](const RouteBeat &beat)
                                 {
                                     return beat.expect_cell && beat.expect_cell->first == 0x4C85;
                                 });
        check(ring != story.end(), "The established mine route includes Derf's quest");
        const auto first_piece = std::find_if(ring + 1, story.end(),
                                              [](const RouteBeat &beat)
                                              {
                                                  return beat.map == 48 && beat.x == 0 &&
                                                         beat.y == 5 && beat.facing == D::north &&
                                                         beat.step;
                                              });
        check(first_piece != story.end(), "The established route reaches the first staff piece");
        beats.insert(beats.end(), ring + 1, first_piece + 1);
        beats.back().expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x4C84, 1};
        if (progress >= SecretProgress::staff)
        {
            const auto restored =
                std::find_if(first_piece + 1, story.end(),
                             [](const RouteBeat &beat)
                             {
                                 return beat.expect_cell && beat.expect_cell->first == 0x4C86;
                             });
            check(restored != story.end(), "The established route restores Oswulf's staff");
            beats.insert(beats.end(), first_piece + 1, restored + 1);
        }
        for (auto at = first_mine_beat; at < beats.size(); ++at)
        {
            auto &beat = beats[at];
            if (!beat.searching)
            {
                beat.searching = false;
            }
            beat.cure_first = true;
            beat.cure_to_full = true;
            beat.reloads_until_won = std::max(beat.reloads_until_won, 600U);
        }
    }
    return beats;
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
        // The armoury (location 9): a mirror for each member, readied where a
        // hand is free, which turns the gazes of basilisks and medusae back
        // (the cluebook's advice).
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
        // Below, basilisks, medusae and cockatrices: each member readies its
        // mirror, which turns their gazes back (the cluebook's advice), in
        // place of its shield.
        for (const char *member :
             {"paladin 1", "paladin 2", "ranger 3", "cleric 4", "cleric 5", "magic_user 6"})
        {
            route.back().ready_types.emplace_back(mirror_type, member);
        }
        // Level 10: through the hole the miners broke into the castle's dungeon
        // (script 64). Its patrols of cockatrices, medusae and basilisks turn the
        // party to stone: each member readies its mirror, and the party waits
        // for them to go where it may.
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
                      true, false, true);
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
        // Spend the experience earned in the ruins and mines before taking
        // on the crevasses. The hall is free; these visits award nothing.
        for (int level = 0; level < 4; ++level)
        {
            route.push_back({16, 7, 6, D::north, train_everyone()});
        }
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
        // and 4C6B) to the Frost Giant Village (script 81). Against the
        // Dreadlord's legion's patrols the party strikes their wizards first,
        // as the cluebook bids (treasures_turn).
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
            true);
        // The Frost Giant Village (script 81): its blockaded door bashed open, its king won over by
        // fighting the Black Circle beside him, his guards lead the party east, toward the castle,
        // into the Eastern Crevasses (script 82).
        walk_passages(route, 81,
                      {{0, 8, D::east},   {1, 8, D::east},   {2, 8, D::east},   {3, 8, D::east},
                       {4, 8, D::east},   {5, 8, D::east},   {6, 8, D::east},   {7, 8, D::east},
                       {8, 8, D::east},   {9, 8, D::south},  {9, 9, D::south},  {9, 10, D::south},
                       {9, 11, D::south}, {9, 12, D::south}, {9, 13, D::south}, {9, 14, D::south},
                       {9, 15, D::east},  {10, 15, D::east}, {11, 15, D::east}, {12, 15, D::north},
                       {2, 15, D::west}},
                      true, false, false, {"BASH", "BASH", "BASH", "CASTLE"});
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
            true);
        route.push_back(step_beat(82, 15, 11, D::east, {"YES"}));
        // The Castle Entrance (script 96), where the castle's guards break
        // every camp: hurt from the crevasses, the party does not rest there
        // but walks along the great hall, through the rotating alcove at 5,9
        // into the room south of it, to the gateway at 6,9 (location 22),
        // which takes it to the Well's gate 13. Sargatha, the castellan, may
        // stop the party on the way with a scroll to sign: it attacks her at
        // once, as the cluebook advises (signing sends it robbed to the
        // Dungeon's tenth level).
        walk_unrested(route, 96,
                      {{0, 8, D::east},
                       {1, 8, D::east},
                       {2, 8, D::east},
                       {3, 8, D::east},
                       {4, 8, D::east},
                       {5, 8, D::south},
                       {5, 10, D::south},
                       {5, 11, D::east},
                       {6, 11, D::north}},
                      {"ATTACK MEDUSA"});
        route.push_back(step_beat(96, 6, 10, D::north, {"YES"}));
        // The portal makes the town's temple reachable again. A living but
        // petrified magic-user cannot be recovered by resting at the Well.
        route.push_back({33, 1, 1, D::north, {"YES"}, true});
        route.push_back({16, 10, 9, D::north, {"YES", "raise:dead", "stone:flesh", "Leave"}});
        for (int level = 0; level < 4; ++level)
        {
            route.push_back({16, 7, 6, D::north, train_everyone()});
        }
        route.push_back({16, 3, 0, D::north, {"YES"}});
        // Rested at the Well, back by gate 13 to the castle's gateway.
        route.push_back({33, 1, 14, D::north, {"YES"}, true});
        // The alcove turns back only once the room's lever is pulled (at
        // 5,15; cell 4C05): back to the great hall, along it into the
        // rotating alcove at 8,9, which turns the party into the room south
        // of it, and east from 8,15 onto the flagstone that sends it to 13,0,
        // by the stairway up.
        walk_passages(route, 96,
                      {{6, 9, D::south},  {6, 10, D::south}, {6, 11, D::south}, {6, 12, D::west},
                       {5, 12, D::south}, {5, 13, D::south}, {5, 14, D::south}, {5, 15, D::north},
                       {5, 14, D::north}, {5, 13, D::north}, {5, 12, D::north}, {5, 11, D::north},
                       {5, 9, D::north},  {5, 8, D::east},   {6, 8, D::east},   {7, 8, D::east},
                       {8, 8, D::south},  {8, 10, D::south}, {8, 11, D::south}, {8, 12, D::south},
                       {8, 13, D::west},  {7, 13, D::south}, {7, 14, D::south}, {7, 15, D::east},
                       {8, 15, D::east},  {13, 0, D::east},  {14, 0, D::east}},
                      false, false, false, {"YES", "ATTACK MEDUSA"});
        // The stairway up to the Middle Level of the Castle (script 97).
        route.push_back(step_beat(96, 15, 0, D::east, {"YES"}));
        // The Middle Level's maze (script 97), from 15,0 to the stairway up
        // at 0,15; its iron golems are given the password STEELEYE.
        walk_passages(route, 97,
                      {{15, 0, D::west},  {14, 0, D::west},  {13, 0, D::west}, {12, 0, D::west},
                       {11, 0, D::south}, {11, 1, D::south}, {11, 2, D::west}, {10, 2, D::west},
                       {9, 2, D::south},  {9, 3, D::south},  {9, 4, D::west},  {8, 4, D::west},
                       {7, 4, D::west},   {6, 4, D::west},   {5, 4, D::west},  {4, 4, D::south},
                       {4, 5, D::east},   {5, 5, D::east},   {6, 5, D::east},  {7, 5, D::east},
                       {8, 5, D::south},  {8, 6, D::south},  {8, 7, D::west},  {7, 7, D::north},
                       {7, 6, D::west},   {6, 6, D::south},  {6, 7, D::south}, {6, 8, D::west},
                       {5, 8, D::north},  {5, 7, D::north},  {5, 6, D::west},  {4, 6, D::west},
                       {3, 6, D::north},  {3, 5, D::west},   {2, 5, D::west},  {1, 5, D::west},
                       {0, 5, D::south},  {0, 6, D::south},  {0, 7, D::south}, {0, 8, D::south},
                       {0, 9, D::south},  {0, 10, D::south}, {0, 11, D::east}, {1, 11, D::south},
                       {1, 12, D::west},  {0, 12, D::south}, {0, 13, D::east}, {1, 13, D::south},
                       {1, 14, D::west},  {0, 14, D::south}},
                      false, false);
        // The stairway up to the Dreadlord's Sanctum (script 98).
        route.push_back(step_beat(97, 0, 15, D::west, {"YES"}));
        // The Dreadlord's Sanctum (script 98), as the cluebook tells it: the
        // illusion of the Dreadlord at 9,15 is let to attack first, which
        // ends it. Hurt from the maze, the party walks on to the gateway at
        // 12,11 (location 7), to rest at the Well and come back by its gate 6.
        walk_unrested(route, 98,
                      {{0, 15, D::east},   {1, 15, D::east},   {2, 15, D::east},  {3, 15, D::north},
                       {3, 14, D::north},  {3, 13, D::north},  {3, 12, D::east},  {4, 12, D::east},
                       {5, 12, D::east},   {6, 12, D::east},   {7, 12, D::east},  {8, 12, D::south},
                       {8, 13, D::south},  {8, 14, D::south},  {8, 15, D::east},  {9, 15, D::east},
                       {10, 15, D::north}, {10, 14, D::east},  {11, 14, D::east}, {12, 14, D::east},
                       {13, 14, D::north}, {13, 13, D::north}, {13, 12, D::north}},
                      {"LET LICH ATTACK FIRST"});
        route.push_back(step_beat(98, 13, 11, D::west, {"YES"}));
        RouteBeat sanctum_rest{33, 12, 3, D::north, {"YES"}, true};
        sanctum_rest.rest_for_spells = true;
        sanctum_rest.ward_memorized = "globe_of_invulnerability";
        sanctum_rest.ward_members = {"magic_user 6"};
        sanctum_rest.memorize_first_of = {"mass_invisibility", "mirror_image", "haste"};
        sanctum_rest.keep_preparation = true;
        route.push_back(sanctum_rest);
        // The lightning across the door at 13,9 is an illusion a member
        // bashes through, and the door at 12,0 takes the Silver Key.
        walk_passages(route, 98,
                      {{12, 11, D::east},
                       {13, 11, D::north},
                       {13, 10, D::north},
                       {13, 9, D::north},
                       {13, 8, D::north},
                       {13, 7, D::west},
                       {12, 7, D::north},
                       {12, 6, D::north},
                       {12, 5, D::north},
                       {12, 4, D::north},
                       {12, 3, D::north},
                       {12, 2, D::east},
                       {13, 2, D::east},
                       {14, 2, D::east},
                       {15, 2, D::north},
                       {15, 1, D::north},
                       {15, 0, D::west},
                       {14, 0, D::west},
                       {13, 0, D::west}},
                      false, false, false, {"BASH", "YES", "SILVER"});
        // Down the corridor beyond to the door that asks for the Dreadlord's
        // brother (8,9: OSWULF), past Sargatha's ambush at 2,10, to the door
        // at 2,9, which takes the Gold Key.
        walk_passages(route, 98,
                      {{12, 0, D::west},  {11, 0, D::south}, {11, 1, D::south}, {11, 2, D::south},
                       {11, 3, D::south}, {11, 4, D::south}, {11, 5, D::south}, {11, 6, D::south},
                       {11, 7, D::south}, {11, 8, D::south}, {11, 9, D::west},  {10, 9, D::west},
                       {9, 9, D::west},   {8, 9, D::west},   {7, 9, D::south},  {7, 10, D::west},
                       {6, 10, D::west},  {5, 10, D::west},  {4, 10, D::west},  {3, 10, D::west},
                       {2, 10, D::north}},
                      false, false, false, {"YES", "GOLD"});
        // North to the warrior in golden armor at 7,1, who bars the door at
        // 6,1; it takes the Brass Key and opens west onto the Dreadlord's
        // own rooms (script 99, on the Sanctum's map): the hydras at 5,1,
        // the Dreadlord himself at 2,4, and at 1,3 a secret door, which the
        // party searches for until it finds it (cell 4CDC's bit 128).
        walk_passages(route, 98,
                      {{2, 9, D::north}, {2, 8, D::north}, {2, 7, D::north}, {2, 6, D::east},
                       {3, 6, D::east},  {4, 6, D::east},  {5, 6, D::east},  {6, 6, D::east},
                       {7, 6, D::east},  {8, 6, D::north}, {8, 5, D::north}, {8, 4, D::north},
                       {8, 3, D::north}, {8, 2, D::north}, {8, 1, D::west},  {7, 1, D::west},
                       {6, 1, D::west},  {5, 1, D::south}, {5, 2, D::west},  {4, 2, D::south},
                       {4, 3, D::south}, {4, 4, D::west},  {3, 4, D::west},  {2, 4, D::north},
                       {2, 3, D::west}},
                      true, false, false, {"YES", "BRASS"});
        // The Dreadlord, awakened as the party steps west from 4,4 to 3,4,
        // with ten storm giants and six Banite priests dealt about the party,
        // their bolts and holds from the first round. Prepare the globe and
        // party buffs at the Well and cast them on the final approach.
        auto &dreadlord =
            *std::ranges::find_if(route,
                                  [](const RouteBeat &beat)
                                  {
                                      return beat.map == 98 && beat.x == 3 && beat.y == 4;
                                  });
        dreadlord.ward_each_with = "globe_of_invulnerability";
        dreadlord.ward_members = {"magic_user 6"};
        for (auto &beat : route)
        {
            if (beat.map == 98 && beat.x == 4 && beat.y == 2)
            {
                beat.cast_after = {"haste"};
            }
            if (beat.map == 98 && beat.x == 4 && beat.y == 3)
            {
                beat.cast_after = {"mass_invisibility"};
            }
            if (beat.map == 98 && beat.x == 4 && beat.y == 4)
            {
                beat.cast_after = {"mirror_image"};
            }
        }
        RouteBeat secret_door{98, 1, 3, D::north, {}};
        secret_door.repeat_in_place = true;
        secret_door.repeat_until_at_least = std::pair<std::uint16_t, std::uint16_t>{0x4CDC, 128};
        route.push_back(secret_door);
        // Through it, the chamber of the Dreadlord's soul: the party explores
        // it, beats the iron golems and medusae about the gem, and the gem is
        // shattered (cell 4CD9 set to 1): Eldamar's soul is free, and the
        // game is won. Twelve iron golems, whom only the weapons +3 of two
        // paladins and Vala harm, and six medusae guard the gem.
        RouteBeat soul_gem = step_beat(98, 1, 3, D::north, {"EXPLORE ROOM"});
        soul_gem.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x4CD9, 1};
        route.push_back(soul_gem);
        // The treasures on the way that arm the party better, as the
        // cluebook's list of items finds them (its Items), each taken where
        // the route's beat wins it: the Old Red Dragon's lair (the ruins'
        // 13), the dead adventurer on the mine's first level (the mines' 11),
        // the mines' 71 and 81, the frost giant king's thanks (the village's
        // 13) and the Eastern Crevasses' 16.
        take_at(route, {32, 0, 3}, 1, {{"Cloak", "magic_user 6"}, {"Ring", "magic_user 6"}});
        take_at(route, {48, 0, 5}, 1,
                {{"Plate Mail", "paladin 1"},
                 {"Long Sword", "paladin 1"},
                 {"Shield", "paladin 1"},
                 {"Ring", "paladin 1"}});
        take_at(route, {48, 14, 2}, 1, {{"Ring", "magic_user 6"}});
        take_at(route, {48, 6, 10}, 3,
                {{"Long Sword", "ranger 3"}, {"Banded", "paladin 2"}, {"Cloak", "cleric 4"}});
        take_at(route, {81, 2, 15}, 1,
                {{"Long Sword", "paladin 2"}, {"Shield", "paladin 2"}, {"Plate Mail", "ranger 3"}});
        take_at(route, {82, 15, 14}, 1,
                {{"Silver Long Sword", "paladin 1"}, {"Silver Shield", "paladin 1"}});
        // From the crevasses on, a fight its player loses with each of its
        // 120 tries is played again with new dice, up to 240 more, until one
        // wins, as a player reloads the Black Circle's patrols, whose
        // wizards hold and disintegrate the party when they act first.
        for (auto &beat : route)
        {
            if (beat.map >= 80)
            {
                beat.reloads_until_won = 240;
            }
        }
        // Oswulf's chained battles are played, never forced won. Training
        // on the town visit, keeping Dispel Magic, and targeting the enemy
        // casters let the route win them; the temple then recovers its dead
        // and petrified members through the castle's portal.
        const auto oswulf = std::find_if(route.rbegin(), route.rend(),
                                         [](const RouteBeat &beat)
                                         {
                                             return beat.map == 82 && beat.x == 15 && beat.y == 15;
                                         });
        check(oswulf != route.rend(), "The route contains Oswulf's chained battles");
        oswulf->reload_event = true;
        // The crevasses hold no gazes: their shields again, out of the
        // Dungeon; the castle's medusae and basilisks: the mirrors again, but
        // for the Silver Shield's bearer, whose shield turns gazes back.
        const auto first_on = [&route](unsigned map)
        {
            return std::ranges::find(route, map, &RouteBeat::map);
        };
        for (const char *member : {"paladin 1", "paladin 2", "ranger 3", "cleric 4", "cleric 5"})
        {
            first_on(80)->ready_types.emplace_back(shield_type, member);
        }
        for (const char *member : {"paladin 2", "ranger 3", "cleric 4", "cleric 5"})
        {
            first_on(96)->ready_types.emplace_back(mirror_type, member);
        }
        return route;
    }();
    return beats;
}

// Answers that train each of Pools of Darkness's route's members at a hall, as
// many levels as its experience allows up to `levels` (a member trains a
// level a choice; one without the experience is told so), then Leave; with
// `before` in front and `after` behind (Elminster's menu in Limbo).
std::vector<std::string> train_darkness_party(int levels, std::vector<std::string> before = {},
                                              const std::vector<std::string> &after = {})
{
    for (const char *member :
         {"paladin 1", "paladin 2", "ranger 3", "cleric 4", "cleric 5", "magic_user 6"})
    {
        before.insert(before.end(), static_cast<std::size_t>(levels), member);
    }
    before.emplace_back("Leave");
    before.insert(before.end(), after.begin(), after.end());
    return before;
}

// Pools of Darkness's route plays to the game's end, Gothmenes fallen and
// Bane's crystal dust (since 2026-11-16; docs/GAMES.md, Pools of Darkness,
// step 5); were this false, it would stop on this map.
constexpr bool darkness_route_ends_the_game = true;
constexpr unsigned darkness_route_stops_on_map = 52;

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
        // who got through (cell 1ABh 128), seven mages who cast Hold Monster
        // and twelve warriors; the route's player has won it since
        // 2026-11-15 (until then a stand-in the test won).
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
        // worship area. The temple's patrols break every camp until it is
        // freed (its rest entry, cell 181h): until the bells ring, a party
        // the temple's fights hurt walks on without resting.
        const auto temple_unfreed = route.size();
        walk_passages(route, 50,
                      {{11, 1, D::west}, {10, 1, D::south}, {10, 2, D::south}, {10, 3, D::south}});
        // Only the temple scribe's office (9,4, facing west: "IT MIGHT BE A
        // GOOD PLACE TO REST") lets the party rest before the temple is freed
        // (the cluebook's location 5): it rests there after the vampire priest.
        rest_in_scribes_office(route);
        walk_passages(route, 50,
                      {{10, 4, D::south},
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
        // East and south to the stairs down (6,5), the wall of type 7 east of
        // it, and down into the catacombs (script 54, the map's lower half),
        // where the party arrives at (7,13).
        walk_passages(route, 50,
                      {{3, 1, D::east},
                       {4, 1, D::east},
                       {5, 1, D::south},
                       {5, 2, D::south},
                       {5, 3, D::south},
                       {5, 4, D::south},
                       {5, 5, D::east}});
        route.push_back(step_beat(50, 6, 5, D::east, {"YES"}));
        // West through the cells and north to the burial preparation room,
        // where the party falls on the marauders fleeing Brimwulf (3,8), to
        // the stairs down at (0,8), which lead to the catacombs' east half
        // (8,9).
        walk_passages(route, 50,
                      {{7, 13, D::south}, {7, 14, D::west},  {6, 14, D::west},  {5, 14, D::west},
                       {4, 14, D::west},  {3, 14, D::west},  {2, 14, D::west},  {1, 14, D::north},
                       {1, 13, D::north}, {1, 12, D::north}, {1, 11, D::north}, {1, 10, D::east},
                       {2, 10, D::east},  {3, 10, D::east},  {4, 10, D::north}, {4, 9, D::north},
                       {4, 8, D::west},   {3, 8, D::west},   {2, 8, D::west},   {1, 8, D::west}});
        route.push_back(step_beat(50, 0, 8, D::south, {"YES"}));
        // South and east to Brimwulf's tomb (13,13), entered facing north:
        // the party opens the coffin, stands away from the spectre's wrath
        // and agrees to be tested (cell ABh 1), so he lets it pass.
        walk_passages(route, 50,
                      {{8, 10, D::south},
                       {8, 11, D::south},
                       {8, 12, D::south},
                       {8, 13, D::south},
                       {8, 14, D::east},
                       {9, 14, D::east},
                       {10, 14, D::north},
                       {10, 13, D::east},
                       {11, 13, D::east},
                       {12, 13, D::south},
                       {12, 14, D::east}});
        auto brimwulf = step_beat(50, 13, 14, D::north, {"OPEN COFFIN", "STAND AWAY", "YES"});
        brimwulf.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0xAB, 1};
        route.push_back(brimwulf);
        // Through the secret door beyond him to the small altar (12,10): the
        // ritual journal entry 11 gives, its third way (cell 17Fh bit 0),
        // so fewer elementals guard the tests.
        walk_passages(route, 50, {{13, 13, D::north}, {13, 12, D::north}, {13, 11, D::west}});
        auto ritual = step_beat(50, 12, 11, D::north, {"LIGHT INCENSE"});
        ritual.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x17F, 3};
        route.push_back(ritual);
        // The earth rises at (10,10); beaten, it leaves the Vessel of Purity,
        // which the party takes (cell 17Fh bit 3).
        walk_passages(route, 50, {{12, 10, D::south}, {12, 11, D::west}, {11, 11, D::west}});
        auto vessel = step_beat(50, 10, 11, D::north, {"TAKE BOWL"});
        vessel.rest_first = true;
        vessel.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x17F, 11};
        route.push_back(vessel);
        // Ilmater's symbol (12,8) ignites; the fire elemental's dying flame
        // passes into the bowl, the Spirit Flame (cell 17Fh bit 4).
        walk_passages(route, 50,
                      {{10, 10, D::north},
                       {10, 9, D::west},
                       {9, 9, D::north},
                       {9, 8, D::east},
                       {10, 8, D::east}});
        auto flame = step_beat(50, 11, 8, D::east);
        flame.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x17F, 27};
        route.push_back(flame);
        // The armory (14,10): the flame lights the Lamp of Vigilance,
        // Brimwulf tells the party to rekindle the four braziers and ring the
        // bells, and the party takes the lamp (cell 17Fh bit 5); a step on,
        // the shelves of arms Priam sought.
        walk_passages(route, 50, {{12, 8, D::east}, {13, 8, D::south}, {13, 9, D::south}});
        auto lamp = step_beat(50, 13, 10, D::east, {"YES"});
        lamp.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x17F, 59};
        route.push_back(lamp);
        route.push_back(step_beat(50, 14, 10, D::east));
        // Back west and up the stairs at (8,9) to the catacombs' west half
        // (0,8), across it to the stairs at (7,13), and up into the temple.
        walk_passages(route, 50,
                      {{15, 10, D::west},
                       {14, 10, D::west},
                       {13, 10, D::north},
                       {13, 9, D::north},
                       {13, 8, D::west},
                       {12, 8, D::west},
                       {11, 8, D::west},
                       {10, 8, D::west},
                       {9, 8, D::south},
                       {9, 9, D::west}});
        route.push_back(step_beat(50, 8, 9, D::north, {"YES"}));
        walk_passages(route, 50,
                      {{0, 8, D::east},   {1, 8, D::east},   {2, 8, D::east},   {3, 8, D::east},
                       {4, 8, D::south},  {4, 9, D::south},  {4, 10, D::west},  {3, 10, D::west},
                       {2, 10, D::west},  {1, 10, D::south}, {1, 11, D::south}, {1, 12, D::south},
                       {1, 13, D::south}, {1, 14, D::east},  {2, 14, D::east},  {3, 14, D::east},
                       {4, 14, D::east},  {5, 14, D::east},  {6, 14, D::east},  {7, 14, D::north}});
        route.push_back(step_beat(50, 7, 13, D::west, {"YES"}));
        // West across the worship area to the stairs up (1,5), climbed to the
        // temple's upper floor (8,5).
        walk_passages(
            route, 50,
            {{6, 5, D::west}, {5, 5, D::west}, {4, 5, D::west}, {3, 5, D::west}, {2, 5, D::west}});
        route.push_back(step_beat(50, 1, 5, D::west, {"YES"}));
        // The four braziers in the corner towers, each lit from the Lamp of
        // Vigilance (cell 17Dh gets a bit each). The north-west tower (8,0):
        // Up there the party rests in the scribe's office again, the catacombs
        // behind it, before it lights the braziers.
        walk_passages(route, 50,
                      {{8, 6, D::east}, {9, 6, D::east}, {10, 6, D::north}, {10, 5, D::north}});
        rest_in_scribes_office(route);
        walk_passages(route, 50,
                      {{10, 4, D::north},
                       {10, 3, D::north},
                       {10, 2, D::north},
                       {10, 1, D::west},
                       {9, 1, D::north}});
        auto first_brazier = step_beat(50, 9, 0, D::west, {"LIGHT BRAZIER"});
        first_brazier.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x17D, 77};
        route.push_back(first_brazier);
        // The north-east tower (15,0).
        walk_passages(route, 50,
                      {{8, 0, D::east},
                       {9, 0, D::south},
                       {9, 1, D::east},
                       {10, 1, D::south},
                       {10, 2, D::east},
                       {11, 2, D::east},
                       {12, 2, D::east},
                       {13, 2, D::north},
                       {13, 1, D::east},
                       {14, 1, D::north}});
        auto second_brazier = step_beat(50, 14, 0, D::east, {"LIGHT BRAZIER"});
        second_brazier.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x17D, 205};
        route.push_back(second_brazier);
        // The south-east tower (15,7).
        walk_passages(route, 50,
                      {{15, 0, D::south},
                       {15, 1, D::south},
                       {15, 2, D::south},
                       {15, 3, D::south},
                       {15, 4, D::south},
                       {15, 5, D::south}});
        auto third_brazier = step_beat(50, 15, 6, D::south, {"LIGHT BRAZIER"});
        third_brazier.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x17D, 237};
        route.push_back(third_brazier);
        // The south-west tower (8,7): the darkness is swept from the temple.
        walk_passages(route, 50,
                      {{15, 7, D::north},
                       {15, 6, D::west},
                       {14, 6, D::west},
                       {13, 6, D::north},
                       {13, 5, D::west},
                       {12, 5, D::west},
                       {11, 5, D::west},
                       {10, 5, D::south},
                       {10, 6, D::west},
                       {9, 6, D::south}});
        auto fourth_brazier = step_beat(50, 9, 7, D::west, {"LIGHT BRAZIER"});
        fourth_brazier.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x17D, 253};
        route.push_back(fourth_brazier);
        // The bell tower (12,3): with every brazier lit the bells ring clear,
        // and the shades of Laurellin and Brimwulf, neither slain, are reunited
        // (journal entry 74's lovers; cell 17Eh bit 1).
        walk_passages(route, 50,
                      {{8, 7, D::east},
                       {9, 7, D::north},
                       {9, 6, D::east},
                       {10, 6, D::north},
                       {10, 5, D::north},
                       {10, 4, D::north},
                       {10, 3, D::east}});
        auto bells = step_beat(50, 11, 3, D::east, {"PULL ROPES", "STEP AWAY"});
        bells.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x181, 1};
        route.push_back(bells);
        for (auto beat = route.begin() + static_cast<std::ptrdiff_t>(temple_unfreed);
             beat != route.end(); ++beat)
        {
            beat->camp_broken = true;
        }
        // Back down the stairs (8,5) to the ground floor.
        walk_passages(route, 50,
                      {{11, 3, D::west},
                       {10, 3, D::south},
                       {10, 4, D::south},
                       {10, 5, D::south},
                       {10, 6, D::west},
                       {9, 6, D::west},
                       {8, 6, D::north}});
        route.push_back(step_beat(50, 8, 5, D::east, {"YES"}));
        // Out through the meeting room and the entrance hall, leaving the
        // plunder alone, and out of the temple: Priam stays with his army.
        walk_passages(route, 50, {{2, 5, D::east}, {3, 5, D::south}, {3, 6, D::south}});
        route.push_back(step_beat(50, 3, 7, D::south, {"YES", "NO"}));
        // West across the Moonsea's north shore, by the crater of Phlan, to
        // the Hill Giant Steading near Dragonhorn Gap (9,2): the party talks
        // its way past the gate's giants as would-be mercenaries (map 21).
        // It flees the vampires that roam the shore, whose bites drain two
        // levels (since 2026-11-15, the 256-color games' effect numbers).
        // The gate's guards are answered on the last step only, so a
        // wandering band met on the way, which may ask the same, does not
        // take the answer (TALK).
        RouteBeat to_steading{17, 0, 0, D::north, {}};
        to_steading.passing = true;
        to_steading.fleeing = true;
        to_steading.area_steps = {C::northwest, C::west, C::west, C::west, C::west,     C::west,
                                  C::west,      C::west, C::west, C::west, C::northwest};
        route.push_back(to_steading);
        RouteBeat into_steading{17, 0, 0, D::north, {"YES", "TALK"}};
        into_steading.fleeing = true;
        into_steading.area_steps = {C::northwest};
        route.push_back(into_steading);
        // North through the courtyard to the Twisted Horn (9,8), where Kardal,
        // Sasha's spy, tells the party to kill the cloud giants in the private
        // room south of it for promotion to the inner steading (cell 114h
        // bit 1).
        walk_passages(route, 21,
                      {{7, 15, D::north},
                       {7, 14, D::north},
                       {7, 13, D::north},
                       {7, 12, D::north},
                       {7, 11, D::north},
                       {7, 10, D::north},
                       {7, 9, D::north},
                       {7, 8, D::east}});
        auto kardal = step_beat(21, 8, 8, D::east, {"YES", "YES"});
        kardal.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x114, 3};
        route.push_back(kardal);
        // The cloud giants at their gaming table (9,10), rested first: beaten,
        // they earn the party its notoriety (bit 2).
        walk_passages(route, 21, {{9, 8, D::south}});
        auto cloud_giants = step_beat(21, 9, 9, D::south);
        cloud_giants.rest_first = true;
        cloud_giants.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x114, 7};
        route.push_back(cloud_giants);
        // Back past the tavern to the inner steading's barred door (7,8): the
        // hill giants pull the party through, and the shaman sends for it
        // (bit 3).
        walk_passages(route, 21,
                      {{9, 10, D::north},
                       {9, 9, D::north},
                       {9, 8, D::west},
                       {8, 8, D::west},
                       {7, 8, D::north}});
        // North and round to Kardal's jeweled dagger in the hall (8,3), its
        // trail of blood followed (bit 4).
        walk_passages(route, 21,
                      {{7, 7, D::north},
                       {7, 6, D::east},
                       {8, 6, D::east},
                       {9, 6, D::east},
                       {10, 6, D::east},
                       {11, 6, D::north},
                       {11, 5, D::west},
                       {10, 5, D::west},
                       {9, 5, D::north},
                       {9, 4, D::north}});
        auto dagger = step_beat(21, 9, 3, D::west);
        dagger.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x114, 31};
        route.push_back(dagger);
        // Down the trail to Kardal (6,4), whom the party rescues from his
        // captors, rested first; he tells it of the Fire Giants (journal entry
        // 49, bit 5).
        walk_passages(route, 21, {{8, 3, D::west}, {7, 3, D::west}});
        auto rescue = step_beat(21, 6, 3, D::south, {"RESCUE HIM"});
        rescue.rest_first = true;
        rescue.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x114, 63};
        route.push_back(rescue);
        // A door slams in the distance (3,4), and a rear guard of hill giants
        // and ogres blocks the way south (3,6). A paladin challenges their
        // authority, the puzzled giants turn on the ogres, and the party beats
        // the ogres who win (bit 7).
        walk_passages(route, 21,
                      {{6, 4, D::west}, {5, 4, D::west}, {4, 4, D::west}, {3, 4, D::south}});
        auto rear_guard = step_beat(21, 3, 5, D::south, {"CHALLENGE", "paladin 1"});
        rear_guard.rest_first = true;
        route.push_back(rear_guard);
        // West and north round the dragons' hoard (3,2) to the hill giants,
        // ettins and skeletons making a stand (5,0), whom the party attacks:
        // the giants would see through a second challenge.
        walk_passages(route, 21,
                      {{3, 6, D::south},
                       {3, 7, D::west},
                       {2, 7, D::west},
                       {1, 7, D::west},
                       {0, 7, D::north},
                       {0, 6, D::north},
                       {0, 5, D::north},
                       {0, 4, D::north},
                       {0, 3, D::north},
                       {0, 2, D::east},
                       {1, 2, D::east},
                       {2, 2, D::north},
                       {2, 1, D::west},
                       {1, 1, D::north},
                       {1, 0, D::east},
                       {2, 0, D::east},
                       {3, 0, D::east}});
        auto last_stand = step_beat(21, 4, 0, D::east, {"ATTACK"});
        last_stand.rest_first = true;
        route.push_back(last_stand);
        // East to the antechamber (11,1), where a white and a black dragon
        // guard the shaman, and on to the Hill Giant Shaman himself (13,1),
        // each fight rested for.
        walk_passages(route, 21,
                      {{5, 0, D::east},
                       {6, 0, D::east},
                       {7, 0, D::east},
                       {8, 0, D::east},
                       {9, 0, D::east},
                       {10, 0, D::south}});
        auto antechamber = step_beat(21, 10, 1, D::east);
        antechamber.rest_first = true;
        route.push_back(antechamber);
        walk_passages(route, 21, {{11, 1, D::east}});
        auto shaman = step_beat(21, 12, 1, D::east);
        shaman.rest_first = true;
        shaman.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x116, 255};
        route.push_back(shaman);
        // His vault (12,0) is locked, and no one forces its door; past it,
        // the door in the steading's north wall (15,0) leads to the Fire Giant
        // Cave (script 32).
        walk_passages(route, 21, {{13, 1, D::north}, {13, 0, D::east}, {14, 0, D::east}});
        route.push_back(step_beat(21, 15, 0, D::north, {"YES"}));
        // In the Fire Giant Cave (map 32) Ungleow, the fire giant mage, meets
        // the party with a bribe (5,12), which it leaves, and it goes on to
        // her endless corridor along the south wall.
        walk_passages(route, 32,
                      {{3, 15, D::north},
                       {3, 14, D::north},
                       {3, 13, D::north},
                       {3, 12, D::north},
                       {3, 11, D::east},
                       {4, 11, D::east},
                       {5, 11, D::south},
                       {5, 12, D::south},
                       {5, 13, D::west},
                       {4, 13, D::south},
                       {4, 14, D::south},
                       {4, 15, D::east},
                       {5, 15, D::east},
                       {6, 15, D::east},
                       {7, 15, D::east},
                       {8, 15, D::east},
                       {9, 15, D::east},
                       {10, 15, D::east}});
        // Each step east from its end (11,15) takes the party back to (9,15)
        // and counts (cell 9Eh); at twelve a crushed skull lies there, and the
        // party sits down to collect its thoughts, picks up the ring in it and
        // throws it down the corridor, which breaks the spell (cell 132h bit 0).
        auto corridor = step_beat(32, 11, 15, D::east, {"YES", "YES", "THROW IT AWAY"});
        corridor.passing = true;
        corridor.repeat_in_place = true;
        corridor.repeat_until_bits = std::pair<std::uint16_t, std::uint16_t>{0x132, 1};
        route.push_back(corridor);
        // Out of the corridor's east end and round by the fire giants who
        // block the way (13,12), north through the room that teleports the
        // party (7,6) and over the rotating floor (12,7), past the mage's
        // guards (10,4) and the assassin who would treat on her behalf (2,3),
        // to Ungleow's room (2,5).
        walk_passages(
            route, 32,
            {{11, 15, D::east},  {12, 15, D::east},  {13, 15, D::east},  {14, 15, D::east},
             {15, 15, D::north}, {15, 14, D::north}, {15, 13, D::north}, {15, 12, D::west},
             {14, 12, D::west},  {13, 12, D::west},  {12, 12, D::west},  {11, 12, D::north},
             {11, 11, D::west},  {10, 11, D::west},  {9, 11, D::west},   {8, 11, D::west},
             {7, 11, D::north},  {7, 10, D::west},   {6, 10, D::north},  {6, 9, D::north},
             {6, 8, D::north},   {6, 7, D::north},   {6, 6, D::east},    {7, 6, D::south},
             {11, 6, D::north},  {11, 5, D::east},   {12, 5, D::south},  {12, 6, D::south},
             {12, 7, D::east},   {13, 7, D::east},   {14, 7, D::east},   {15, 7, D::north},
             {15, 6, D::north},  {15, 5, D::north},  {15, 4, D::west},   {14, 4, D::west},
             {13, 4, D::west},   {12, 4, D::west},   {11, 4, D::west},   {10, 4, D::south},
             {10, 5, D::west},   {9, 5, D::west},    {8, 5, D::west},    {7, 5, D::west},
             {6, 5, D::north},   {6, 4, D::north},   {6, 3, D::east},    {7, 3, D::east},
             {8, 3, D::east},    {9, 3, D::east},    {10, 3, D::east},   {11, 3, D::east}});
        // Past the giants who pour out behind it, the party rests in the
        // room north of the passage (13,2): the cave's camps rest only in its
        // quiet rooms (events 34 to 36; the cluebook's location 13).
        walk_passages(route, 32, {{12, 3, D::north}, {12, 2, D::east}});
        auto cave_rest = step_beat(32, 13, 2, D::west);
        cave_rest.rest_first = true;
        route.push_back(cave_rest);
        walk_passages(route, 32,
                      {{12, 2, D::south}, {12, 3, D::east},  {13, 3, D::east},  {14, 3, D::east},
                       {15, 3, D::north}, {15, 2, D::north}, {15, 1, D::north}, {15, 0, D::west},
                       {14, 0, D::west},  {13, 0, D::west},  {12, 0, D::west},  {11, 0, D::west},
                       {10, 0, D::west},  {9, 0, D::west},   {8, 0, D::west},   {7, 0, D::west},
                       {6, 0, D::west},   {5, 0, D::west},   {4, 0, D::south},  {4, 1, D::west},
                       {3, 1, D::west},   {2, 1, D::south},  {2, 2, D::south},  {2, 3, D::south}});
        // Ungleow pulls away from her giants and asks the party to sit down:
        // it attacks, and beats her and her guards (cell 134h bit 0).
        auto ungleow = step_beat(32, 2, 4, D::south, {"ATTACK"});
        ungleow.rest_first = true;
        route.push_back(ungleow);
        // Through her room and north up the passage to the dragons guarding
        // the way to the Aerie (0,2): the red, black and green dragons are
        // beaten (cell 133h bit 1), rested for first.
        walk_passages(route, 32,
                      {{2, 5, D::west}, {1, 5, D::west}, {0, 5, D::north}, {0, 4, D::north}});
        auto cave_dragons = step_beat(32, 0, 3, D::north);
        cave_dragons.rest_first = true;
        cave_dragons.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x133, 3};
        route.push_back(cave_dragons);
        // West from (0,1) the party sees the dragons flying off to the north-
        // west and follows them; the tunnel seals behind it, and it comes out
        // on the area map below the Dragons' Aerie (7,1).
        walk_passages(route, 32, {{0, 2, D::north}});
        auto follow_dragons = step_beat(32, 0, 1, D::west, {"YES"});
        follow_dragons.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x25, 7};
        route.push_back(follow_dragons);
        // North-west, a dragon flies into an opening in the mountains: the
        // party enters the Dragons' Aerie.
        RouteBeat to_aerie{17, 0, 0, D::north, {"YES"}};
        to_aerie.area_steps = {C::northwest};
        route.push_back(to_aerie);
        // The Aerie's camps rest only in its disused rooms (events 20 and 21;
        // the cluebook's location 9): from (14,4) the party steps into the
        // north-east one (13,5), rests and steps back, as a player rests
        // between the nests' fights.
        const auto rest_in_aerie_room = [&route]
        {
            walk_passages(route, 33, {{14, 4, D::west}, {13, 4, D::south}});
            auto room = step_beat(33, 13, 5, D::north);
            room.rest_first = true;
            route.push_back(room);
            walk_passages(route, 33, {{13, 4, D::east}});
        };
        // In the Aerie (map 33) the party beats the white and green dragons at
        // its doors, and takes the four keys to the dimensional gate from the
        // dragons' nests, in the order the planning aid finds them.
        walk_passages(route, 33, {{8, 15, D::north},  {8, 14, D::north},  {8, 13, D::north},
                                  {8, 12, D::north},  {8, 11, D::north},  {8, 10, D::north},
                                  {8, 9, D::north},   {8, 8, D::east},    {9, 8, D::east},
                                  {10, 8, D::east},   {11, 8, D::east},   {12, 8, D::south},
                                  {12, 9, D::south},  {12, 10, D::east},  {13, 10, D::south},
                                  {13, 11, D::east},  {14, 11, D::east},  {15, 11, D::south},
                                  {15, 12, D::south}, {15, 13, D::south}, {15, 14, D::south},
                                  {15, 15, D::west},  {14, 15, D::west},  {13, 15, D::west}});
        // The green dragons give Modthryth the blood for another dracolich; beaten, they leave the
        // copper key (cell 144h bit 3).
        auto copper_key = step_beat(33, 12, 15, D::west);
        copper_key.rest_first = true;
        copper_key.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x144, 8};
        route.push_back(copper_key);
        walk_passages(
            route, 33,
            {{11, 15, D::north}, {11, 14, D::north}, {11, 13, D::east},  {12, 13, D::east},
             {13, 13, D::east},  {14, 13, D::east},  {15, 13, D::north}, {15, 12, D::north},
             {15, 11, D::west},  {14, 11, D::west},  {13, 11, D::north}, {13, 10, D::west},
             {12, 10, D::north}, {12, 9, D::north},  {12, 8, D::west},   {11, 8, D::west},
             {10, 8, D::west},   {9, 8, D::north},   {9, 7, D::east},    {10, 7, D::east},
             {11, 7, D::east},   {12, 7, D::east},   {13, 7, D::east},   {14, 7, D::east},
             {15, 7, D::north},  {15, 6, D::north},  {15, 5, D::north},  {15, 4, D::west}});
        rest_in_aerie_room();
        walk_passages(route, 33,
                      {{14, 4, D::north},
                       {14, 3, D::north},
                       {14, 2, D::north},
                       {14, 1, D::north},
                       {14, 0, D::west},
                       {13, 0, D::south},
                       {13, 1, D::west}});
        // The black dragons pay homage to the duel's victor: the party attacks, and finds the iron
        // key in their treasure (bit 2).
        auto iron_key = step_beat(33, 12, 1, D::west, {"YES"});
        iron_key.rest_first = true;
        iron_key.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x144, 12};
        route.push_back(iron_key);
        walk_passages(route, 33,
                      {{11, 1, D::east},
                       {12, 1, D::north},
                       {12, 0, D::east},
                       {13, 0, D::east},
                       {14, 0, D::south},
                       {14, 1, D::south},
                       {14, 2, D::south},
                       {14, 3, D::south}});
        rest_in_aerie_room();
        walk_passages(route, 33,
                      {{14, 4, D::east}, {15, 4, D::south}, {15, 5, D::south}, {15, 6, D::south},
                       {15, 7, D::west}, {14, 7, D::west},  {13, 7, D::west},  {12, 7, D::west},
                       {11, 7, D::west}, {10, 7, D::west},  {9, 7, D::west},   {8, 7, D::west},
                       {7, 7, D::west},  {6, 7, D::west},   {5, 7, D::west},   {4, 7, D::west},
                       {3, 7, D::west},  {2, 7, D::west},   {1, 7, D::north},  {1, 6, D::north},
                       {1, 5, D::west},  {0, 5, D::north},  {0, 4, D::north},  {0, 3, D::north},
                       {0, 2, D::north}, {0, 1, D::east},   {1, 1, D::east},   {2, 1, D::east}});
        // The Modthryth's followers recruit the white dragons for the next dracolich; one wears the
        // silver key (bit 0).
        auto silver_key = step_beat(33, 3, 1, D::east);
        silver_key.rest_first = true;
        silver_key.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x144, 13};
        route.push_back(silver_key);
        // The party rests in the north-west disused room (3,2) on its way.
        walk_passages(route, 33, {{4, 1, D::south}, {4, 2, D::west}});
        auto north_west_room = step_beat(33, 3, 2, D::south);
        north_west_room.rest_first = true;
        route.push_back(north_west_room);
        walk_passages(route, 33,
                      {{3, 3, D::south},  {3, 4, D::south}, {3, 5, D::west},  {2, 5, D::south},
                       {2, 6, D::south},  {2, 7, D::east},  {3, 7, D::east},  {4, 7, D::east},
                       {5, 7, D::east},   {6, 7, D::south}, {6, 8, D::west},  {5, 8, D::west},
                       {4, 8, D::west},   {3, 8, D::south}, {3, 9, D::west},  {2, 9, D::south},
                       {2, 10, D::south}, {2, 11, D::west}, {1, 11, D::west}, {0, 11, D::south}});
        // And in the south-west one (2,12) before the blue dragons, whose
        // lightning the party meets whole.
        const auto rest_in_south_west_room = [&route]
        {
            walk_passages(route, 33, {{0, 12, D::east}, {1, 12, D::east}});
            auto south_west_room = step_beat(33, 2, 12, D::west);
            south_west_room.rest_first = true;
            route.push_back(south_west_room);
            walk_passages(route, 33, {{1, 12, D::west}});
        };
        rest_in_south_west_room();
        walk_passages(route, 33,
                      {{0, 12, D::south},
                       {0, 13, D::south},
                       {0, 14, D::south},
                       {0, 15, D::east},
                       {1, 15, D::north},
                       {1, 14, D::east},
                       {2, 14, D::east}});
        // The blue dragons' nest, the steel key hidden behind a rock (bit 1).
        // Its five blue dragons' lightning, three breaths each of their 80
        // Hit Points, beats the route's party (levels 12 to 14) in most plays
        // (4 of 120 win, two members dead, 2026-11-16), so the player reloads
        // until one wins and raises its dead after.
        auto steel_key = step_beat(33, 3, 14, D::east);
        steel_key.rest_first = true;
        steel_key.reloads_until_won = 240;
        steel_key.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x144, 15};
        route.push_back(steel_key);
        // Back west and north to the doors before the dimensional gate (7,2):
        // the four keys unlock them (cell 144h FFh).
        walk_passages(route, 33,
                      {{4, 14, D::west},
                       {3, 14, D::south},
                       {3, 15, D::west},
                       {2, 15, D::west},
                       {1, 15, D::west},
                       {0, 15, D::north},
                       {0, 14, D::north},
                       {0, 13, D::north}});
        // On the way it rests in the south-west disused room (2,12) again,
        // before Modthryth's dracoliches.
        rest_in_south_west_room();
        walk_passages(route, 33,
                      {{0, 12, D::north}, {0, 11, D::north}, {0, 10, D::north}, {0, 9, D::north},
                       {0, 8, D::east},   {1, 8, D::east},   {2, 8, D::east},   {3, 8, D::east},
                       {4, 8, D::east},   {5, 8, D::east},   {6, 8, D::north},  {6, 7, D::north},
                       {6, 6, D::east},   {7, 6, D::north},  {7, 5, D::north},  {7, 4, D::west},
                       {6, 4, D::west},   {5, 4, D::north},  {5, 3, D::east},   {6, 3, D::east},
                       {7, 3, D::north}});
        auto locks = step_beat(33, 7, 2, D::north, {"USE KEYS"});
        locks.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x144, 255};
        route.push_back(locks);
        // Through them Modthryth stands beside his dracoliches, which the
        // party beats (cell 141h FFh), rested first.
        auto dracolich = step_beat(33, 7, 2, D::north);
        dracolich.rest_first = true;
        dracolich.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x141, 255};
        route.push_back(dracolich);
        // Round to the dimensional gate (7,0), entered facing west: the party
        // steps through to Thorne's cave.
        walk_passages(route, 33, {{7, 1, D::east}, {8, 1, D::north}});
        // It comes out in Limbo, where Elminster offers his help: the party
        // rests, trains, and moves on to Thorne's cave. The route walks
        // straight to its goals, where a player meets wandering monsters on
        // the way (the cluebook's "Wandering Monsters" of every area): the
        // test gives each member experience for those of the Lands of Thar
        // to the Aerie, 300,000 (a stand-in), and the party trains as far as
        // it goes.
        route.push_back(step_beat(33, 8, 0, D::west, {"YES", "stop:REST"}));
        // Its clerics and paladins memorize Resist Fire for every member, as
        // a player readies the ward against Thorne's breath: no camp in his
        // cave rests until he falls (script 34's camp entry).
        RouteBeat limbo_rest{18, 0, 0, D::north, {}};
        limbo_rest.rest_choice = "REST";
        limbo_rest.ward_memorized = "resist_fire";
        route.push_back(limbo_rest);
        RouteBeat limbo{18, 0, 0, D::north,
                        train_darkness_party(2, {"TRAIN"}, {"MOVE ON", "THORNE"})};
        limbo.waiting = true;
        limbo.test_experience = 300'000;
        route.push_back(limbo);
        // In Thorne's cave (map 34) the party beats the red dragons at the
        // gate and goes straight for Thorne's chamber, by the way the
        // planning aid finds, before the Horn of Doom's third blast: past the
        // Ki-rin's spirit, who asks why it has come (DUTY), to Thorne (3,7),
        // whom it will not leave; beaten, he leaves the Horn of Doom (cell
        // 152h FFh).
        walk_passages(route, 34, {{1, 13, D::south}, {1, 14, D::south},  {1, 15, D::east},
                                  {2, 15, D::east},  {3, 15, D::east},   {4, 15, D::north},
                                  {4, 14, D::east},  {5, 14, D::east},   {6, 14, D::east},
                                  {7, 14, D::north}, {7, 13, D::north},  {7, 12, D::east},
                                  {8, 12, D::east},  {9, 12, D::east},   {10, 12, D::east},
                                  {11, 12, D::east}, {12, 12, D::north}, {12, 11, D::east},
                                  {13, 11, D::east}, {14, 11, D::east}});
        // South of the corridor (15,12) a red dragon stuffs its treasure into
        // a hole (the cluebook's room 4): the party attacks it, rested first,
        // and the paladin who leads takes the Ring of Fire Resistance from its
        // hoard (its one ring, unidentified) and wears it against Thorne's
        // breath.
        auto hoard = step_beat(34, 15, 11, D::south, {"ATTACK"});
        hoard.rest_first = true;
        hoard.take_items = {{"RING", "paladin 1"}};
        route.push_back(hoard);
        walk_passages(route, 34,
                      {{15, 12, D::north},
                       {15, 11, D::north},
                       {15, 10, D::north},
                       {15, 9, D::north},
                       {15, 8, D::north},
                       {15, 7, D::west},
                       {14, 7, D::west},
                       {13, 7, D::west}});
        // South through the doors to the room the Ki-rin has made safe to
        // camp in (10,9, event 39; the cluebook's 14), the one camp of the
        // cave that rests while Thorne lives (script 34's camp entry): the
        // party rests there, its clerics and paladins memorizing Resist Fire
        // for every member, and goes back north.
        walk_passages(route, 34,
                      {{12, 7, D::south}, {12, 8, D::south}, {12, 9, D::west}, {11, 9, D::west}});
        auto safe_room = step_beat(34, 10, 9, D::east);
        safe_room.rest_first = true;
        safe_room.rest_for_spells = true;
        safe_room.ward_memorized = "resist_fire";
        route.push_back(safe_room);
        walk_passages(route, 34, {{11, 9, D::east}, {12, 9, D::north}, {12, 8, D::north}});
        walk_passages(route, 34,
                      {{12, 7, D::north}, {12, 6, D::north}, {12, 5, D::west},  {11, 5, D::west},
                       {10, 5, D::south}, {10, 6, D::south}, {10, 7, D::south}, {10, 8, D::west},
                       {9, 8, D::west},   {8, 8, D::west},   {7, 8, D::north},  {7, 7, D::north},
                       {7, 6, D::north},  {7, 5, D::west},   {6, 5, D::north},  {6, 4, D::north},
                       {6, 3, D::north},  {6, 2, D::north},  {6, 1, D::west},   {5, 1, D::west},
                       {4, 1, D::west},   {3, 1, D::west},   {2, 1, D::south},  {2, 2, D::south},
                       {2, 3, D::south},  {2, 4, D::west},   {1, 4, D::south},  {1, 5, D::east},
                       {2, 5, D::east},   {3, 5, D::south}});
        // Thorne, an ancient red dragon, and his seven red dragons breathe
        // fire (his 24d10+12, three times): the clerics and the first paladin
        // cast the Resist Fire memorized in the safe room on each member
        // first, as a player wards the party. Their fireballs and breath beat
        // the party in most plays (6 of 120 win, 2026-11-16; since the dragons
        // come from the east, south and west as his SETUP MONSTER names, 9 of
        // 1,200, most of them at the cost of both clerics), so the player
        // reloads until one wins, and reloads a win that costs two members.
        auto thorne = step_beat(34, 3, 6, D::south, {"YES"});
        thorne.reloads_until_won = 240;
        thorne.rest_first = true;
        // The cave's camps do not rest: the clerics cure every hurt member
        // first (the dragons met on arrival leave some hurt).
        thorne.cure_first = true;
        thorne.cure_to_full = true;
        thorne.ward_each_with = "resist_fire";
        thorne.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x152, 255};
        route.push_back(thorne);
        // Back the way it came to the dimensional gate (0,13), stepped
        // through westward, and from Limbo on to the Realms.
        walk_passages(route, 34,
                      {{3, 7, D::north},  {3, 6, D::north},   {3, 5, D::west},   {2, 5, D::west},
                       {1, 5, D::north},  {1, 4, D::north},   {1, 3, D::east},   {2, 3, D::north},
                       {2, 2, D::north},  {2, 1, D::east},    {3, 1, D::east},   {4, 1, D::east},
                       {5, 1, D::east},   {6, 1, D::south},   {6, 2, D::south},  {6, 3, D::south},
                       {6, 4, D::south},  {6, 5, D::east},    {7, 5, D::south},  {7, 6, D::south},
                       {7, 7, D::south},  {7, 8, D::east},    {8, 8, D::north},  {8, 7, D::north},
                       {8, 6, D::east},   {9, 6, D::east},    {10, 6, D::north}, {10, 5, D::east},
                       {11, 5, D::east},  {12, 5, D::south},  {12, 6, D::south}, {12, 7, D::east},
                       {13, 7, D::east},  {14, 7, D::east},   {15, 7, D::south}, {15, 8, D::south},
                       {15, 9, D::south}, {15, 10, D::south}, {15, 11, D::west}, {14, 11, D::west},
                       {13, 11, D::west}, {12, 11, D::south}, {12, 12, D::west}, {11, 12, D::west},
                       {10, 12, D::west}, {9, 12, D::west},   {8, 12, D::west},  {7, 12, D::south},
                       {7, 13, D::south}, {7, 14, D::south},  {7, 15, D::west},  {6, 15, D::west},
                       {5, 15, D::west},  {4, 15, D::west},   {3, 15, D::west},  {2, 15, D::west},
                       {1, 15, D::north}, {1, 14, D::north}});
        route.push_back(step_beat(34, 1, 13, D::west, {"YES", "MOVE ON", "REALM"}));
        // Elminster sends the party back to the Aerie's gate (7,0): it walks
        // out by the Aerie's entrance (8,15) onto the area map (6,0).
        walk_passages(route, 33,
                      {{7, 0, D::east},   {8, 0, D::south},  {8, 1, D::south},  {8, 2, D::south},
                       {8, 3, D::west},   {7, 3, D::west},   {6, 3, D::west},   {5, 3, D::south},
                       {5, 4, D::east},   {6, 4, D::east},   {7, 4, D::east},   {8, 4, D::south},
                       {8, 5, D::south},  {8, 6, D::south},  {8, 7, D::south},  {8, 8, D::south},
                       {8, 9, D::south},  {8, 10, D::south}, {8, 11, D::south}, {8, 12, D::south},
                       {8, 13, D::south}, {8, 14, D::south}});
        route.push_back(step_beat(33, 8, 15, D::south, {"YES", "NO"}));
        // South-west round the area map to Zhentil Keep's gate (0,4), fleeing
        // the basilisks that roam the west where they let it, and resting
        // whole before the last step, as a player would before the city,
        // whose camps rouse its guards: a basilisk's gaze turned two
        // members to stone on the way, which Stone to Flesh undoes in camp.
        RouteBeat to_zhentil_keep{17, 0, 0, D::north, {}};
        to_zhentil_keep.area_steps = {C::southwest, C::southwest, C::southwest, C::southwest};
        to_zhentil_keep.fleeing = true;
        route.push_back(to_zhentil_keep);
        RouteBeat into_zhentil_keep{17, 0, 0, D::north, {}};
        into_zhentil_keep.area_steps = {C::west};
        into_zhentil_keep.fleeing = true;
        into_zhentil_keep.rest_first = true;
        route.push_back(into_zhentil_keep);
        // Paid in at the gate, the party walks west through the city's south
        // half (map 19's north half is Mulmaster's, script 26) to the training
        // hall's sign (3,11). North through the door the clerk guards (3,10,
        // facing north) it pays the 10 gems he asks of those not on the
        // approved list, and in the hall (3,9) each member with the experience
        // trains (event 5, PROGRAM 0).
        walk_passages(route, 19,
                      {{8, 15, D::north},
                       {8, 14, D::north},
                       {8, 13, D::north},
                       {8, 12, D::north},
                       {8, 11, D::west},
                       {7, 11, D::west},
                       {6, 11, D::west},
                       {5, 11, D::west},
                       {4, 11, D::west}});
        route.push_back(step_beat(19, 3, 11, D::north, {"YES"}));
        // The wandering monsters of Thorne's cave and the Moonsea's shore
        // a player meets on the way are a stand-in: 300,000 experience each.
        auto zhentil_hall = step_beat(19, 3, 10, D::north, train_darkness_party(2));
        zhentil_hall.test_experience = 300'000;
        route.push_back(zhentil_hall);
        // East along the streets to the Purple Plume Tavern (12,14), entered
        // from its sign (13,14) facing west: the party stays and rests there,
        // where no patrol breaks a camp while the guards have not been roused
        // (cell A7h; the cluebook's "mostly safe").
        walk_passages(route, 19,
                      {{3, 10, D::south},
                       {3, 11, D::east},
                       {4, 11, D::east},
                       {5, 11, D::east},
                       {6, 11, D::east},
                       {7, 11, D::east},
                       {8, 11, D::east},
                       {9, 11, D::east},
                       {10, 11, D::east},
                       {11, 11, D::east},
                       {12, 11, D::east},
                       {13, 11, D::east},
                       {14, 11, D::south},
                       {14, 12, D::south},
                       {14, 13, D::south},
                       {14, 14, D::west}});
        route.push_back(step_beat(19, 13, 14, D::west, {"YES"}));
        RouteBeat tavern{19, 12, 14, D::west, {"YES"}};
        tavern.rest_first = true;
        route.push_back(tavern);
        // Out of the tavern and north through the city to the gate of the
        // courtyard below Manshoon's tower (8,7), where the party goes on into
        // the tower's halls (script 48).
        walk_passages(route, 19,
                      {{12, 14, D::east},
                       {13, 14, D::east},
                       {14, 14, D::north},
                       {14, 13, D::north},
                       {14, 12, D::north},
                       {14, 11, D::north},
                       {14, 10, D::west},
                       {13, 10, D::west},
                       {12, 10, D::west},
                       {11, 10, D::west},
                       {10, 10, D::west},
                       {9, 10, D::west},
                       {8, 10, D::north},
                       {8, 9, D::north}});
        route.push_back(step_beat(19, 8, 8, D::north, {"YES"}));
        // Through the tower's entry offices (map 48) to the passage down to
        // the drow stronghold (3,8). Seven clerics question the party (13,12):
        // it goes on, has entered Zhentil Keep once, and bears Thorne's gift,
        // the Horn of Doom, so it may pass.
        walk_passages(route, 48,
                      {{15, 15, D::north},
                       {15, 14, D::west},
                       {14, 14, D::north},
                       {14, 13, D::north},
                       {14, 12, D::west}});
        route.push_back(step_beat(48, 13, 12, D::west, {"YES", "YES", "1", "YES"}));
        walk_passages(route, 48,
                      {{12, 12, D::south},
                       {12, 13, D::west},
                       {11, 13, D::west},
                       {10, 13, D::south},
                       {10, 14, D::east},
                       {11, 14, D::east},
                       {12, 14, D::south},
                       {12, 15, D::west},
                       {11, 15, D::west},
                       {10, 15, D::west},
                       {9, 15, D::west},
                       {8, 15, D::west},
                       {7, 15, D::north},
                       {7, 14, D::north},
                       {7, 13, D::north},
                       {7, 12, D::north},
                       {7, 11, D::north},
                       {7, 10, D::west},
                       {6, 10, D::west}});
        route.push_back(step_beat(48, 5, 10, D::north));
        walk_passages(route, 48, {{5, 8, D::west}, {4, 8, D::west}});
        route.push_back(step_beat(48, 3, 8, D::west, {"YES"}));
        // Down in the drow stronghold under the tower (map 35, the Descent to the
        // Depths) the party makes its way, as the planning aid finds it, to the
        // ladders into the long hall (7,14) and the drow temple's halls (map 68).
        walk_passages(route, 35,
                      {{0, 2, D::east},
                       {1, 2, D::south},
                       {1, 3, D::east},
                       {2, 3, D::south},
                       {2, 4, D::south},
                       {2, 5, D::east}});
        route.push_back(step_beat(35, 3, 5, D::east, {"YES"}));
        walk_passages(
            route, 35,
            {{2, 11, D::south},  {2, 12, D::east},   {3, 12, D::north},  {3, 11, D::east},
             {4, 11, D::north},  {4, 10, D::east},   {5, 10, D::east},   {6, 10, D::south},
             {6, 11, D::south},  {6, 12, D::east},   {7, 12, D::south},  {7, 13, D::east},
             {8, 13, D::north},  {8, 12, D::east},   {9, 12, D::east},   {10, 12, D::east},
             {11, 12, D::south}, {11, 13, D::south}, {11, 14, D::west},  {10, 14, D::west},
             {9, 14, D::south},  {9, 15, D::east},   {10, 15, D::east},  {11, 15, D::east},
             {12, 15, D::north}, {12, 14, D::north}, {12, 13, D::north}, {12, 12, D::north},
             {12, 11, D::north}, {12, 10, D::east},  {13, 10, D::east},  {14, 10, D::east},
             {15, 10, D::south}, {15, 11, D::south}, {15, 12, D::south}, {15, 13, D::west},
             {14, 13, D::west},  {13, 13, D::south}, {13, 14, D::south}, {13, 15, D::east},
             {14, 15, D::east}});
        route.push_back(step_beat(35, 15, 15, D::east, {"YES"}));
        walk_passages(route, 35,
                      {{0, 15, D::north},
                       {0, 14, D::north},
                       {0, 13, D::east},
                       {1, 13, D::east},
                       {2, 13, D::east},
                       {3, 13, D::south},
                       {3, 14, D::east},
                       {4, 14, D::east},
                       {5, 14, D::east},
                       {6, 14, D::east}});
        route.push_back(step_beat(35, 7, 14, D::east, {"YES"}));
        // In the slave pens below (map 68, Silk and the Slaves of the Drow)
        // the party makes its way as the planning aid finds it. Silk (7,13,
        // entered facing west) joins it when it agrees to help her (cell 25Eh
        // 1), and her room is safe to rest in (cell 9Ah). In the prison wing
        // (script 71, east of 12,13) it attacks the prison guards (14,12) for
        // their keys and frees her Swanmays from the cell at (10,11) (cell 25Eh
        // 3), as the Eyes of Kalistes will ask; it rests in her room again.
        // Then north through the barracks and west under the arches to the
        // quiet room (1,8, entered facing west), where it rests, over the
        // spiked pit's bridge and through the teleporting room, past the iron
        // golem and the fountain, to the secret door into Kalistes' Temple
        // (15,1; script 69).
        const auto slave_pens = route.size();
        walk_passages(route, 68,
                      {{7, 15, D::north},
                       {7, 14, D::east},
                       {8, 14, D::east},
                       {9, 14, D::north},
                       {9, 13, D::north},
                       {9, 12, D::west},
                       {8, 12, D::south}});
        route.push_back(step_beat(68, 8, 13, D::west, {"APPROACH", "YES"}));
        RouteBeat silk{68, 7, 13, D::west, {}};
        silk.rest_first = true;
        route.push_back(silk);
        walk_passages(route, 68,
                      {{7, 13, D::east},
                       {8, 13, D::north},
                       {8, 12, D::east},
                       {9, 12, D::east},
                       {10, 12, D::south},
                       {10, 13, D::east},
                       {11, 13, D::east},
                       {12, 13, D::east},
                       {13, 13, D::east}});
        route.push_back(step_beat(68, 14, 13, D::north, {"YES"}));
        walk_passages(
            route, 68,
            {{14, 12, D::north}, {14, 11, D::west}, {13, 11, D::west}, {12, 11, D::west}});
        auto swanmays = step_beat(68, 11, 11, D::west);
        swanmays.expect_cell = std::pair<std::uint16_t, std::uint16_t>{606, 3};
        route.push_back(swanmays);
        walk_passages(route, 68,
                      {{10, 11, D::east},
                       {11, 11, D::east},
                       {12, 11, D::east},
                       {13, 11, D::east},
                       {14, 11, D::south},
                       {14, 12, D::south},
                       {14, 13, D::west},
                       {13, 13, D::west},
                       {12, 13, D::west},
                       {11, 13, D::west},
                       {10, 13, D::north},
                       {10, 12, D::west},
                       {9, 12, D::west},
                       {8, 12, D::south},
                       {8, 13, D::west}});
        route.push_back(silk);
        walk_passages(route, 68,
                      {{7, 13, D::east},  {8, 13, D::north}, {8, 12, D::east}, {9, 12, D::north},
                       {9, 11, D::north}, {9, 10, D::west},  {8, 10, D::west}, {7, 10, D::north},
                       {7, 9, D::north},  {7, 8, D::west},   {6, 8, D::north}, {6, 7, D::east},
                       {7, 7, D::east},   {8, 7, D::east},   {9, 7, D::north}, {9, 6, D::north},
                       {9, 5, D::west},   {8, 5, D::west},   {7, 5, D::west},  {6, 5, D::west},
                       {5, 5, D::west},   {4, 5, D::west},   {3, 5, D::west},  {2, 5, D::south},
                       {2, 6, D::south},  {2, 7, D::south},  {2, 8, D::west}});
        RouteBeat quiet_room{68, 1, 8, D::west, {}};
        quiet_room.rest_first = true;
        route.push_back(quiet_room);
        walk_passages(route, 68,
                      {{1, 8, D::east},
                       {2, 8, D::north},
                       {2, 7, D::north},
                       {2, 6, D::north},
                       {2, 5, D::east},
                       {3, 5, D::east},
                       {4, 5, D::east},
                       {5, 5, D::north},
                       {5, 4, D::north},
                       {5, 3, D::west},
                       {4, 3, D::west},
                       {3, 3, D::west},
                       {2, 3, D::north},
                       {2, 2, D::west},
                       {1, 2, D::west}});
        route.push_back(step_beat(68, 0, 2, D::north, {"CROSS BRIDGE"}));
        walk_passages(route, 68,
                      {{2, 1, D::east},
                       {3, 1, D::east},
                       {8, 2, D::west},
                       {7, 2, D::west},
                       {9, 1, D::east},
                       {10, 1, D::east}});
        route.push_back(step_beat(68, 11, 1, D::east, {"YES"}));
        walk_passages(route, 68, {{12, 1, D::east}});
        route.push_back(step_beat(68, 13, 1, D::east, {"YES"}));
        walk_passages(route, 68, {{14, 1, D::east}});
        route.push_back(step_beat(68, 15, 1, D::east, {"YES"}));
        // The drow patrols fall on any camp outside the cluebook's two quiet
        // rooms: a hurt party walks on.
        for (auto beat = route.begin() + static_cast<std::ptrdiff_t>(slave_pens);
             beat != route.end(); ++beat)
        {
            beat->camp_broken = true;
        }
        // Kalistes' Temple (map 69): in through the secret entry (0,0) east
        // and south to the priests' quarters (8,2), where two priests leave
        // a map of where the monsters will be sent, which the party takes.
        // Its camps are broken (cell A2h) outside the barracks: a hurt party
        // walks on.
        const auto temple = route.size();
        walk_passages(route, 69,
                      {{0, 0, D::east},
                       {1, 0, D::east},
                       {2, 0, D::east},
                       {3, 0, D::east},
                       {4, 0, D::east},
                       {5, 0, D::east},
                       {6, 0, D::south},
                       {6, 1, D::south},
                       {6, 2, D::south},
                       {6, 3, D::east},
                       {7, 3, D::east}});
        route.push_back(step_beat(69, 8, 3, D::north, {"YES"}));
        // East past the guard who locks the party in and the priest who
        // takes it for assassins, to the barracks (13,1), whose drow squad
        // the party attacks; beaten, it rests there, the one room of the
        // temple whose camps no one breaks (cell A2h; the cluebook's 16).
        walk_passages(route, 69,
                      {{8, 2, D::north},
                       {8, 1, D::east},
                       {9, 1, D::east},
                       {10, 1, D::east},
                       {11, 1, D::east}});
        route.push_back(step_beat(69, 12, 1, D::east, {"ATTACK"}));
        RouteBeat barracks{69, 13, 1, D::east, {}};
        barracks.rest_first = true;
        route.push_back(barracks);
        for (auto beat = route.begin() + static_cast<std::ptrdiff_t>(temple);
             beat != route.end() - 1; ++beat)
        {
            beat->camp_broken = true;
        }
        // South along the temple's east corridor to the oracle (9,10),
        // entered facing north, which tells the drow to guard their lady
        // and what of hers the party must destroy (journal entry 93).
        walk_passages(route, 69,
                      {{13, 1, D::west},
                       {12, 1, D::south},
                       {12, 2, D::west},
                       {11, 2, D::south},
                       {11, 3, D::south},
                       {11, 4, D::south},
                       {11, 5, D::south},
                       {11, 6, D::south},
                       {11, 7, D::south},
                       {11, 8, D::south},
                       {11, 9, D::south},
                       {11, 10, D::south},
                       {11, 11, D::west},
                       {10, 11, D::west}});
        route.push_back(step_beat(69, 9, 11, D::north));
        // North up the middle aisle into the inner chamber (8,6), entered
        // facing north: the party disrupts the ceremony and beats its drow,
        // Pets of Kalistes and giant and phase spiders, then destroys what
        // the oracle named, the statue's HANDS (cell 278h), closing the
        // portals to the Realms. Through the last portal (8,4) it steps into
        // Limbo, on its way to Kalistes' dimension.
        walk_passages(route, 69,
                      {{9, 10, D::south},
                       {9, 11, D::west},
                       {8, 11, D::north},
                       {8, 10, D::north},
                       {8, 9, D::north},
                       {8, 8, D::north}});
        auto statue = step_beat(69, 8, 7, D::north, {"type:HANDS"});
        statue.expect_cell = std::pair<std::uint16_t, std::uint16_t>{632, 1};
        route.push_back(statue);
        walk_passages(route, 69, {{8, 6, D::north}});
        route.push_back(step_beat(69, 8, 5, D::north, {"YES", "stop:REST"}));
        // In Limbo Elminster's camp heals the party night after night, its
        // dead raised; it trains and moves on to Kalistes' land, the Web
        // Dimension (map 83, script 83). There the Eyes of Kalistes, Cynthia,
        // asks who helped the party at the temple: SILK, her sister, whose
        // Swanmays it freed (cell 25Eh 3), and she asks three favors, spiders
        // from the Cavern of Fireflies and drow prisoners first.
        RouteBeat web_rest{18, 0, 0, D::north, {}};
        web_rest.rest_choice = "REST";
        route.push_back(web_rest);
        // The drow stronghold's and the temple's wandering drow, and the Web
        // Dimension's minis, where the cluebook bids the party "gain as much
        // experience as you can" before Kalistes, are a stand-in: 400,000
        // experience each, trained here.
        RouteBeat to_web{
            18, 0, 0, D::north,
            train_darkness_party(2, {"TRAIN"},
                                 {"MOVE ON", "KALISTES LAND", "YES", "YES", "type:SILK"})};
        to_web.waiting = true;
        to_web.test_experience = 400'000;
        route.push_back(to_web);
        // Cynthia's first favor: drow prisoners. North round the temple's
        // ruins and west to the drow patrol at (4,4), which the party
        // captures (CAPTURE; cell 221h bit 5).
        walk_passages(route, 83,
                      {{8, 4, D::north},
                       {8, 3, D::east},
                       {9, 3, D::north},
                       {9, 2, D::north},
                       {9, 1, D::north},
                       {9, 0, D::west},
                       {8, 0, D::west},
                       {7, 0, D::west},
                       {6, 0, D::west},
                       {5, 0, D::south},
                       {5, 1, D::south},
                       {5, 2, D::south},
                       {5, 3, D::south}});
        route.push_back(step_beat(83, 5, 4, D::west, {"CAPTURE"}));
        // The second: spiders. Back north to the cave (6,1), the Cavern of
        // Fireflies (script 85), where the party takes a nest of pale spiders.
        walk_passages(route, 83,
                      {{4, 4, D::east}, {5, 4, D::north}, {5, 3, D::north}, {5, 2, D::north}});
        route.push_back(step_beat(83, 5, 1, D::east, {"YES", "YES"}));
        // Back to Cynthia (8,4) with both: her spiders disguise the drow as
        // the party, whose deaths Kalistes sees, and she asks the third favor,
        // that the party kill her, which the cluebook says to grant (BEHEAD).
        walk_passages(route, 83,
                      {{5, 1, D::north},
                       {5, 0, D::east},
                       {6, 0, D::east},
                       {7, 0, D::east},
                       {8, 0, D::east},
                       {9, 0, D::south},
                       {9, 1, D::south},
                       {9, 2, D::south},
                       {9, 3, D::west}});
        route.push_back(step_beat(83, 8, 3, D::south, {"BEHEAD"}));
        // Out of the ruins by the south way (7,9) onto the web's paths
        // (script 80, an area map as script 17 is), unrested: a camp in the
        // ruins drew Kalistes' pets, iron golems and giant spiders, which
        // beat the party a drow patrol had hurt, so the clerics cure the
        // badly hurt instead.
        walk_passages(route, 83,
                      {{8, 3, D::east},
                       {9, 3, D::south},
                       {9, 4, D::south},
                       {9, 5, D::south},
                       {9, 6, D::south},
                       {9, 7, D::west},
                       {8, 7, D::south},
                       {8, 8, D::south},
                       {8, 9, D::west}});
        for (auto beat = route.end() - 9; beat != route.end(); ++beat)
        {
            beat->camp_broken = true;
            beat->cure_first = true;
        }
        route.push_back(step_beat(83, 7, 9, D::south, {"YES", "NO"}));
        // North-east along the paths to the Drow Testing Ground's gates
        // (13,7), which swing open for the party (script 81).
        RouteBeat to_testing_ground{83, 0, 0, D::north, {"YES"}};
        to_testing_ground.area_steps = {C::northeast, C::northeast, C::northeast,
                                        C::northeast, C::northeast, C::northeast,
                                        C::east,      C::east,      C::east};
        route.push_back(to_testing_ground);
        // In the Testing Ground (map 81), past the gate's guards: south
        // through the compound, ambushing the two drow at (12,10), to the
        // barracks (13,14, entered facing east), whose drow the party beats
        // and where it rests (the cluebook's 19). The compound's drow priests
        // break the camps outside the barracks: a hurt party walks on.
        const auto testing_compound = route.size();
        walk_passages(route, 81,
                      {{15, 7, D::south},
                       {15, 8, D::west},
                       {14, 8, D::west},
                       {13, 8, D::south},
                       {13, 9, D::west}});
        walk_passages(route, 81, {{12, 9, D::south}}, false, false, false, {"YES"});
        walk_passages(route, 81,
                      {{12, 10, D::south},
                       {12, 11, D::south},
                       {12, 12, D::south},
                       {12, 13, D::south},
                       {12, 14, D::east}});
        for (auto beat = route.begin() + static_cast<std::ptrdiff_t>(testing_compound);
             beat != route.end(); ++beat)
        {
            beat->camp_broken = true;
        }
        RouteBeat testing_barracks{81, 13, 14, D::east, {}};
        testing_barracks.rest_first = true;
        route.push_back(testing_barracks);
        // West along the south wall to the east wall's secret door (8,14),
        // through it (YES, YES) into the Test's first chamber (9,14).
        walk_passages(route, 81,
                      {{13, 14, D::west},
                       {12, 14, D::south},
                       {12, 15, D::west},
                       {11, 15, D::west},
                       {10, 15, D::west},
                       {9, 15, D::west}});
        route.push_back(step_beat(81, 8, 15, D::north, {"YES", "YES"}));
        // The Test's first trial, Strength (10,14): a member strong enough
        // breaks the wall down.
        route.push_back(step_beat(81, 9, 14, D::east,
                                  {"paladin 1", "YES", "paladin 2", "YES", "ranger 3", "YES",
                                   "cleric 4", "YES", "cleric 5"}));
        // Courage (11,11): the party pushes on through its dread. Battle
        // Readiness (11,7): it beats the driders. Magical Ability (8,4): it
        // beats the drow mages. Intelligence (7,4): it answers the riddle
        // with the name of its last foe, BANE.
        walk_passages(route, 81, {{10, 14, D::north}, {10, 13, D::east}, {11, 13, D::north}});
        route.push_back(step_beat(81, 11, 12, D::north, {"PUSH FORWARD"}));
        walk_passages(route, 81,
                      {{11, 11, D::north},
                       {11, 10, D::north},
                       {11, 9, D::north},
                       {11, 8, D::north},
                       {11, 7, D::north},
                       {11, 6, D::north},
                       {11, 5, D::west},
                       {10, 5, D::west},
                       {9, 5, D::north},
                       {9, 4, D::west}});
        route.push_back(step_beat(81, 8, 4, D::west, {"type:BANE"}));
        // Seeing through Illusion (4,6): the party walks on through the wall
        // of flame (YES, YES). Temptation (4,12): it approaches the young elf
        // holding the Ward, a drider, and beats him and his kind.
        walk_passages(route, 81,
                      {{7, 4, D::south}, {7, 5, D::west}, {6, 5, D::west}, {5, 5, D::south}});
        route.push_back(step_beat(81, 5, 6, D::west, {"YES", "YES"}));
        // Burnt by the flames, the party does not camp in the Test, whose
        // camps draw swarms of spiders until it holds the amulets that repel
        // them: its clerics cure the badly hurt before Hem.
        walk_passages(route, 81,
                      {{4, 6, D::south},
                       {4, 7, D::south},
                       {4, 8, D::south},
                       {4, 9, D::south},
                       {4, 10, D::south}});
        for (auto beat = route.end() - 5; beat != route.end(); ++beat)
        {
            beat->camp_broken = true;
        }
        auto hem = step_beat(81, 4, 11, D::south, {"APPROACH"});
        hem.cure_first = true;
        route.push_back(hem);
        // The Test passed, the table beyond (6,14) holds magic daggers and
        // the amulets that repel spiders, which the party takes (cell 1FFh).
        walk_passages(route, 81, {{4, 12, D::south}, {4, 13, D::south}, {4, 14, D::east}});
        auto amulets = step_beat(81, 5, 14, D::east);
        amulets.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x1FF, 1};
        route.push_back(amulets);
        // North to the altar room (8,9), where Hem's fate is decided and Daris
        // flees with the Ward; the party follows.
        walk_passages(route, 81,
                      {{6, 14, D::north},
                       {6, 13, D::east},
                       {7, 13, D::north},
                       {7, 12, D::north},
                       {7, 11, D::east},
                       {8, 11, D::north},
                       {8, 10, D::north}});
        // After Daris, round by (6,9) to his guards (7,8), whom the party
        // beats; it picks up the Ward of Safe Passage (YES) and beats Shest,
        // Kalistes' Pet, and his enormous spiders (cell 1F7h).
        walk_passages(route, 81, {{8, 9, D::west}, {7, 9, D::west}, {6, 9, D::north}});
        auto ward = step_beat(81, 6, 8, D::east, {"YES"});
        ward.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x1F7, 1};
        route.push_back(ward);
        // Back through the Test's chambers, resting where it is hurt (the
        // cluebook: in any section of the Test it has completed), and out by
        // the compound's south way (5,15) to the web's paths.
        walk_passages(
            route, 81,
            {{7, 8, D::west},    {6, 8, D::south},   {6, 9, D::east},    {7, 9, D::east},
             {8, 9, D::south},   {8, 10, D::south},  {8, 11, D::west},   {7, 11, D::south},
             {7, 12, D::south},  {7, 13, D::south},  {7, 14, D::west},   {6, 14, D::west},
             {5, 14, D::west},   {4, 14, D::north},  {4, 13, D::north},  {4, 12, D::north},
             {4, 11, D::north},  {4, 10, D::north},  {4, 9, D::north},   {4, 8, D::north},
             {4, 7, D::north},   {4, 6, D::north},   {4, 5, D::east},    {5, 5, D::east},
             {6, 5, D::north},   {6, 4, D::east},    {7, 4, D::east},    {8, 4, D::east},
             {9, 4, D::south},   {9, 5, D::east},    {10, 5, D::east},   {11, 5, D::south},
             {11, 6, D::south},  {11, 7, D::south},  {11, 8, D::south},  {11, 9, D::south},
             {11, 10, D::south}, {11, 11, D::south}, {11, 12, D::south}, {11, 13, D::south}});
        // On the way out it rests in the barracks (13,14) again, its spells
        // chosen anew, for the web's paths have no safe place to rest.
        walk_passages(route, 81, {{11, 14, D::east}, {12, 14, D::east}});
        auto barracks_again = step_beat(81, 13, 14, D::west);
        barracks_again.rest_first = true;
        barracks_again.rest_for_spells = true;
        route.push_back(barracks_again);
        walk_passages(route, 81,
                      {{12, 14, D::west},
                       {11, 14, D::west},
                       {10, 14, D::west},
                       {9, 14, D::west},
                       {8, 14, D::south},
                       {8, 15, D::west},
                       {7, 15, D::west},
                       {6, 15, D::west}});
        route.push_back(step_beat(81, 5, 15, D::south, {"YES", "NO"}));
        // North-east and east along the paths, passing the other sites by,
        // to the village beside the road (33,1), by Kalistes' Parlor.
        // The party flees what roams the paths (the Pets of Kalistes), as a
        // player bound for the Parlor does.
        RouteBeat to_roadside_village{83, 0, 0, D::north, {}};
        to_roadside_village.passing = true;
        to_roadside_village.fleeing = true;
        to_roadside_village.area_steps = {C::northeast, C::northeast, C::northeast,
                                          C::northeast, C::northeast, C::northeast};
        to_roadside_village.area_steps.insert(to_roadside_village.area_steps.end(), 13, C::east);
        route.push_back(to_roadside_village);
        RouteBeat into_roadside_village{83, 0, 0, D::north, {"YES"}};
        into_roadside_village.area_steps = {C::east};
        into_roadside_village.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x25, 33};
        route.push_back(into_roadside_village);
        // Round the village to the Parlor's wall of gossamer (1,4), where
        // Locaste marches in by her own Ward; the party tries to enter as
        // well, and the spiders shy from its Ward (script 85): it is in
        // Kalistes' Parlor (script 82).
        walk_passages(route, 83,
                      {{7, 9, D::north}, {7, 8, D::east},  {8, 8, D::north}, {8, 7, D::east},
                       {9, 7, D::north}, {9, 6, D::north}, {9, 5, D::north}, {9, 4, D::north},
                       {9, 3, D::north}, {9, 2, D::north}, {9, 1, D::north}, {9, 0, D::west},
                       {8, 0, D::west},  {7, 0, D::west},  {6, 0, D::west},  {5, 0, D::south},
                       {5, 1, D::south}, {5, 2, D::south}, {5, 3, D::south}, {5, 4, D::west},
                       {4, 4, D::west},  {3, 4, D::west}});
        route.push_back(step_beat(83, 2, 4, D::west, {"TRY TO ENTER"}));
        // The cluebook's way through the Parlor: from the foyer west round
        // to the guards' quarters beside the main doors (2,14), where the
        // party beats the drow guards and rests (the only rooms its camps
        // rest in).
        walk_passages(route, 82,
                      {{7, 15, D::north},
                       {7, 14, D::north},
                       {7, 13, D::north},
                       {7, 12, D::east},
                       {8, 12, D::north},
                       {8, 11, D::west},
                       {7, 11, D::west},
                       {6, 11, D::west},
                       {5, 11, D::west},
                       {4, 11, D::south},
                       {4, 12, D::south},
                       {4, 13, D::south},
                       {4, 14, D::west}});
        route.push_back(step_beat(82, 3, 14, D::west));
        auto guard_quarters = step_beat(82, 2, 14, D::east);
        guard_quarters.rest_first = true;
        route.push_back(guard_quarters);
        // North and round to the kitchen (2,1), where the ogre cook is about
        // to kill Arta, Storm's sister: the party rescues her (RESCUE) and
        // beats the guards, pets and gaze spiders the cook calls; she shows
        // it the trapdoor to the larder below, which it leaves for now (NO),
        // going back to rest in the guards' quarters first.
        const auto guard_room_to_kitchen = [&route]
        {
            walk_passages(route, 82,
                          {{3, 14, D::north},
                           {3, 13, D::north},
                           {3, 12, D::north},
                           {3, 11, D::west},
                           {2, 11, D::north},
                           {2, 10, D::north},
                           {2, 9, D::north},
                           {2, 8, D::east},
                           {3, 8, D::east},
                           {4, 8, D::north},
                           {4, 7, D::north},
                           {4, 6, D::north},
                           {4, 5, D::north},
                           {4, 4, D::north},
                           {4, 3, D::north},
                           {4, 2, D::north},
                           {4, 1, D::west}});
        };
        guard_room_to_kitchen();
        route.push_back(step_beat(82, 3, 1, D::west, {"RESCUE", "NO"}));
        walk_passages(route, 82,
                      {{2, 1, D::east},
                       {3, 1, D::east},
                       {4, 1, D::south},
                       {4, 2, D::south},
                       {4, 3, D::south},
                       {4, 4, D::south},
                       {4, 5, D::south},
                       {4, 6, D::south},
                       {4, 7, D::south},
                       {4, 8, D::west},
                       {3, 8, D::west},
                       {2, 8, D::south},
                       {2, 9, D::south},
                       {2, 10, D::south},
                       {2, 11, D::east},
                       {3, 11, D::south},
                       {3, 12, D::south},
                       {3, 13, D::south},
                       {3, 14, D::west}});
        // The Parlor's camps rest only in the guards' quarters: the party
        // walks back hurt, without stopping.
        for (auto beat = route.end() - 19; beat != route.end(); ++beat)
        {
            beat->camp_broken = true;
        }
        auto rested_for_the_larder = step_beat(82, 2, 14, D::east);
        rested_for_the_larder.rest_first = true;
        // The lower level has no camp that rests, so this rest readies the
        // egg room's wards: a Protection from Evil for each member and the
        // magic-user's Hastes.
        rested_for_the_larder.ward_memorized = "protection_from_evil";
        rested_for_the_larder.memorize_first_of = {"haste", "haste"};
        route.push_back(rested_for_the_larder);
        // Rested, back to the kitchen and down through the trapdoor (YES) to
        // the larder (map 84 at 2,2), where the party leaves the slaves bound
        // (IGNORE), as they ask.
        guard_room_to_kitchen();
        route.push_back(step_beat(82, 3, 1, D::west, {"YES", "IGNORE"}));
        // From the larder east and round through the lower level, past the
        // driders' rooms, to the egg rooms (2,6), where the party destroys
        // the eggs of Kalistes' children to draw her out.
        walk_passages(route, 84,
                      {{2, 1, D::south},  {2, 2, D::east},   {3, 2, D::east},  {4, 2, D::south},
                       {4, 3, D::east},   {5, 3, D::east},   {6, 3, D::east},  {7, 3, D::east},
                       {8, 3, D::east},   {9, 3, D::east},   {10, 3, D::east}, {11, 3, D::east},
                       {12, 3, D::south}, {12, 4, D::south}, {12, 5, D::east}, {13, 5, D::south},
                       {13, 6, D::south}, {13, 7, D::south}, {13, 8, D::west}, {12, 8, D::west},
                       {11, 8, D::west},  {10, 8, D::west},  {9, 8, D::west},  {8, 8, D::north},
                       {8, 7, D::north},  {8, 6, D::north},  {8, 5, D::west},  {7, 5, D::west},
                       {6, 5, D::west},   {5, 5, D::south},  {5, 6, D::west},  {4, 6, D::west}},
                      false, false, false, {"IGNORE"});
        // Kalistes comes with seven drow champions, eight drow wizards and
        // five of her pets, and once she falls the slave who shows himself
        // Tanetal's spy calls salamanders, an earth elemental, bits of
        // Moander and poisonous snakes. Her script sets the break point to
        // 100 (cell 1Bh), so neither band ever breaks: the wizards' Ice
        // Storms, Hold Monster and Feeblemind, the pets' Disintegrate and
        // Kalistes' six blows beat the party in most plays. Before the eggs
        // the party wards itself as a player does: a Protection from Evil
        // on each member (Kalistes, her drow and her pets are all evil),
        // then the magic-user's Haste, which takes the whole party. So warded it wins Kalistes in
        // about a play in four, a member lost in each (6 of 360 unwarded),
        // and the spy's band after the kept play in nine of ten
        // (2026-11-18), so the player reloads her fight until a play wins.
        // Beaten, she leaves the Crystal Ring (cell 208h).
        route.back().ward_each_with = "protection_from_evil";
        auto eggs = step_beat(84, 3, 6, D::west, {"DESTROY"});
        eggs.ward_each_with = "haste";
        eggs.ward_members = {"paladin 1"};
        eggs.reloads_until_won = 240;
        eggs.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x208, 1};
        route.push_back(eggs);
        // Out with the ring: east through the lower level to the stairs up
        // (13,4, YES), and through the upper level to the foyer and out by
        // the main doors (7,15) to the web's paths.
        walk_passages(route, 84,
                      {{2, 6, D::east},   {3, 6, D::east},   {4, 6, D::east},  {5, 6, D::north},
                       {5, 5, D::east},   {6, 5, D::east},   {7, 5, D::east},  {8, 5, D::south},
                       {8, 6, D::south},  {8, 7, D::south},  {8, 8, D::east},  {9, 8, D::east},
                       {10, 8, D::east},  {11, 8, D::east},  {12, 8, D::east}, {13, 8, D::north},
                       {13, 7, D::north}, {13, 6, D::north}, {13, 5, D::east}, {14, 5, D::east},
                       {15, 5, D::north}, {15, 4, D::west}});
        route.push_back(step_beat(84, 14, 4, D::west, {"YES"}));
        walk_passages(route, 82,
                      {{13, 3, D::north}, {13, 2, D::west},  {12, 2, D::south}, {12, 3, D::south},
                       {12, 4, D::south}, {12, 5, D::south}, {12, 6, D::south}, {12, 7, D::south},
                       {12, 8, D::west},  {11, 8, D::west},  {10, 8, D::west},  {9, 8, D::south},
                       {9, 9, D::south},  {9, 10, D::south}, {9, 11, D::west},  {8, 11, D::south},
                       {8, 12, D::south}, {8, 13, D::south}, {8, 14, D::south}, {8, 15, D::west}});
        route.push_back(step_beat(82, 7, 15, D::south, {"YES", "NO"}));
        // South-west and west along the paths back to the ruined temple
        // (4,13), and in it to the black pool where Cynthia stood (8,4),
        // which returns the party to Limbo (YES).
        RouteBeat to_ruined_temple{83, 0, 0, D::north, {}};
        to_ruined_temple.passing = true;
        to_ruined_temple.area_steps.assign(12, C::southwest);
        to_ruined_temple.area_steps.insert(to_ruined_temple.area_steps.end(), 17, C::west);
        route.push_back(to_ruined_temple);
        RouteBeat into_ruined_temple{83, 0, 0, D::north, {"YES"}};
        into_ruined_temple.area_steps = {C::west};
        into_ruined_temple.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x25, 4};
        route.push_back(into_ruined_temple);
        walk_passages(route, 83,
                      {{7, 9, D::east},
                       {8, 9, D::north},
                       {8, 8, D::north},
                       {8, 7, D::east},
                       {9, 7, D::north},
                       {9, 6, D::north},
                       {9, 5, D::north},
                       {9, 4, D::north},
                       {9, 3, D::west}});
        route.push_back(step_beat(83, 8, 3, D::south, {"YES", "stop:REST"}));
        // Elminster rejoices at the Crystal Ring (journal entry 82); his camp
        // rests the party, it trains, and he returns it to the Realms by the
        // way it left them, Kalistes' Temple's portal (69: 8,5).
        RouteBeat ring_rest{18, 0, 0, D::north, {}};
        ring_rest.rest_choice = "REST";
        route.push_back(ring_rest);
        RouteBeat to_realms{18, 0, 0, D::north,
                            train_darkness_party(2, {"TRAIN"}, {"MOVE ON", "REALMS", "YES", "NO"})};
        to_realms.waiting = true;
        route.push_back(to_realms);
        // Back in the Realms, down the temple's middle aisle and out by its
        // main doors (8,15) to the Descent to the Depths (map 35 at 2,7).
        walk_passages(route, 69,
                      {{8, 5, D::south},
                       {8, 6, D::south},
                       {8, 7, D::south},
                       {8, 8, D::south},
                       {8, 9, D::south},
                       {8, 10, D::south},
                       {8, 11, D::south},
                       {8, 12, D::south},
                       {8, 13, D::south},
                       {8, 14, D::south}});
        route.push_back(step_beat(69, 8, 15, D::south, {"YES"}));
        // West round the stronghold's edge, which wraps to its east side,
        // and north up the eastern way to the path up to Manshoon's tower
        // (12,6, the path's walls north: "THE PATH LEADS UP").
        walk_passages(route, 35, {{2, 7, D::west},    {1, 7, D::west},    {0, 7, D::west},
                                  {15, 7, D::west},   {14, 7, D::south},  {14, 8, D::east},
                                  {15, 8, D::south},  {15, 9, D::south},  {15, 10, D::west},
                                  {14, 10, D::north}, {14, 9, D::west},   {13, 9, D::west},
                                  {12, 9, D::north},  {12, 8, D::north},  {12, 7, D::north},
                                  {12, 6, D::south},  {12, 7, D::south},  {12, 8, D::south},
                                  {12, 9, D::south},  {12, 10, D::south}, {12, 11, D::south},
                                  {12, 12, D::south}, {12, 13, D::south}, {12, 14, D::south}});
        route.push_back(step_beat(35, 12, 15, D::west, {"YES"}));
        // Through the skirmishers' rooms (the cluebook's 14), whose archers
        // shoot from behind: the party goes on each time (GO ON? YES).
        walk_passages(route, 35,
                      {{11, 15, D::west},
                       {10, 15, D::west},
                       {9, 15, D::north},
                       {9, 14, D::east},
                       {10, 14, D::east},
                       {11, 14, D::north}});
        walk_passages(route, 35,
                      {{11, 13, D::north},
                       {11, 12, D::west},
                       {10, 12, D::south},
                       {10, 13, D::west},
                       {9, 13, D::west},
                       {8, 13, D::west}},
                      false, false, false, {"YES"});
        // West to the ladder up (7,12, YES) and on to the wagons that take
        // slaves up to the keep (2,5), the cluebook's 3.
        route.push_back(step_beat(35, 7, 13, D::north));
        route.push_back(step_beat(35, 7, 12, D::west, {"YES"}));
        walk_passages(route, 35,
                      {{6, 12, D::north}, {6, 11, D::north}, {6, 10, D::west},  {5, 10, D::west},
                       {4, 10, D::west},  {3, 10, D::west},  {2, 10, D::west},  {1, 10, D::south},
                       {1, 11, D::west},  {0, 11, D::north}, {0, 10, D::north}, {0, 9, D::north},
                       {0, 8, D::east},   {1, 8, D::east},   {2, 8, D::east},   {3, 8, D::east},
                       {4, 8, D::north},  {4, 7, D::north},  {4, 6, D::north},  {4, 5, D::west},
                       {3, 5, D::west}});
        route.push_back(step_beat(35, 2, 5, D::north, {"YES"}));
        // Out of the wagon inside Zhentil Keep's gate (map 19 at 7,8),
        // unseen, and south through the city's south half to its training
        // hall (3,9), as on the way in: the wandering monsters of the Web
        // Dimension's paths and the Descent a player meets on the way are a
        // stand-in, 500,000 experience each, and the party trains up to two
        // levels for Myth Drannor's rakshasas. Then off the map by the city's
        // gate (8,15) to the area map.
        walk_passages(route, 19,
                      {{7, 8, D::east},
                       {8, 8, D::south},
                       {8, 9, D::south},
                       {8, 10, D::south},
                       {8, 11, D::west},
                       {7, 11, D::west},
                       {6, 11, D::west},
                       {5, 11, D::west},
                       {4, 11, D::west}});
        route.push_back(step_beat(19, 3, 11, D::north, {"YES"}));
        auto zhentil_hall_again = step_beat(19, 3, 10, D::north, train_darkness_party(2));
        zhentil_hall_again.test_experience = 500'000;
        route.push_back(zhentil_hall_again);
        walk_passages(route, 19,
                      {{3, 9, D::south},
                       {3, 10, D::south},
                       {3, 11, D::east},
                       {4, 11, D::east},
                       {5, 11, D::east},
                       {6, 11, D::east},
                       {7, 11, D::east},
                       {8, 11, D::south},
                       {8, 12, D::south},
                       {8, 13, D::south},
                       {8, 14, D::south}});
        route.push_back(step_beat(19, 8, 15, D::south, {"YES", "NO"}));
        // South down the area map's west edge to the ruins of Myth Drannor
        // (0,13), fleeing the basilisks and dracolisks that roam there.
        RouteBeat to_myth_drannor{17, 0, 0, D::north, {}};
        to_myth_drannor.passing = true;
        to_myth_drannor.fleeing = true;
        to_myth_drannor.area_steps.assign(8, C::south);
        route.push_back(to_myth_drannor);
        // Princess Nacacia asks the party into the ruins with her knights
        // (YES), joins it and offers six items against the rakshasas: the
        // ranger takes and readies her light crossbow and blessed bolts, which
        // slay a rakshasa they hit (effect 82), and keeps her quarrels +4. It passes through the
        // broken gate into Myth Drannor (map 66, script 66).
        RouteBeat into_myth_drannor{17, 0, 0, D::north, {"YES", "YES"}};
        into_myth_drannor.area_steps = {C::south};
        // The gifts are unidentified: her quarrels +4 come before her
        // blessed bolts, which the ranger readies last in their place.
        into_myth_drannor.take_items = {{"Crossbow", "ranger 3"}, {"Bolt", "ranger 3"},
                                        {"Bolt", "ranger 3"},     {"Bolt", "ranger 3"},
                                        {"Bolt", "ranger 3"},     {"Bolt", "ranger 3"}};
        route.push_back(into_myth_drannor);
        // Myth Drannor's walks make no camp but in the ruined houses (the
        // script's events 16 and up; the cluebook's locations 17): elsewhere
        // its camps draw the basilisks and dracolisks that roam the ruins
        // nine times in ten, so the party walks on hurt.
        const auto walk_ruins_unrested =
            [&route](std::initializer_list<std::tuple<unsigned, unsigned, D>> steps)
        {
            walk_passages(route, 66, steps, false, false, false, {"YES"});
            for (auto beat = route.end() - static_cast<std::ptrdiff_t>(steps.size());
                 beat != route.end(); ++beat)
            {
                beat->camp_broken = true;
            }
        };
        // In the ruins (map 66) the party walks south and east to the room of
        // smashed artifacts (15,9, the cluebook's 6), entered by its door
        // facing east in search mode, where it finds a stash of archery
        // supplies: a light crossbow +3, quarrels +4 and four stacks of
        // blessed bolts, which the second paladin takes and readies (the
        // quarrels first, the blessed bolts in their place), so that two
        // members shoot the rakshasas the bolts slay.
        walk_ruins_unrested({{15, 4, D::south},
                             {15, 5, D::south},
                             {15, 6, D::west},
                             {14, 6, D::west},
                             {13, 6, D::south},
                             {13, 7, D::south},
                             {13, 8, D::south},
                             {13, 9, D::east}});
        auto archery = step_beat(66, 14, 9, D::east);
        archery.camp_broken = true;
        archery.searching = true;
        archery.take_items = {{"Crossbow", "paladin 2"}, {"Bolt", "paladin 2"},
                              {"Bolt", "paladin 2"},     {"Bolt", "paladin 2"},
                              {"Bolt", "paladin 2"},     {"Bolt", "paladin 2"}};
        route.push_back(archery);
        auto searched = step_beat(66, 15, 9, D::west);
        searched.camp_broken = true;
        searched.passing = true;
        searched.searching = false;
        route.push_back(searched);
        // Back west, and along the street north of the knights the
        // rakshasas drain (the cluebook's 9, left alone: its fight would
        // spend the bolts the temple needs), to the spectres' room (3,10, the
        // cluebook's 11), entered by its door facing west. Beaten, they leave
        // an elf's remains, whose armor the party takes (YES), and with it a
        // light crossbow +2, quarrels +4 and two stacks of blessed bolts,
        // which the first paladin takes and readies, so three members shoot
        // the bolts.
        walk_ruins_unrested({{14, 9, D::west},
                             {13, 9, D::west},
                             {12, 9, D::west},
                             {11, 9, D::south},
                             {11, 10, D::west},
                             {10, 10, D::west},
                             {9, 10, D::west},
                             {8, 10, D::west},
                             {7, 10, D::west},
                             {6, 10, D::west},
                             {5, 10, D::west}});
        auto grave = step_beat(66, 4, 10, D::west, {"YES"});
        grave.camp_broken = true;
        grave.take_items = {{"Crossbow", "paladin 1"},
                            {"Bolt", "paladin 1"},
                            {"Bolt", "paladin 1"},
                            {"Bolt", "paladin 1"}};
        route.push_back(grave);
        // South to the ruined houses (6,13), where it rests, the spectres'
        // drains given back.
        walk_ruins_unrested({{3, 10, D::east},
                             {4, 10, D::east},
                             {5, 10, D::south},
                             {5, 11, D::south},
                             {5, 12, D::south},
                             {5, 13, D::south},
                             {5, 14, D::east},
                             {6, 14, D::north}});
        RouteBeat ruin_rest{66, 6, 13, D::north, {}};
        ruin_rest.rest_first = true;
        // A night's rest even when whole, so the casters have their spells
        // for the rakshasas, Dispel Magic first, which frees a member their
        // Hold Person holds.
        ruin_rest.rest_for_spells = true;
        ruin_rest.memorize_first_of = {"dispel_magic"};
        route.push_back(ruin_rest);
        // The bolts' bearers ready their crossbows (item type 27) and their
        // quarrels +4 (12), and, where `blessed`, their blessed bolts (74) in
        // the quarrels' place, as a player saves the bolts for the
        // rakshasas no +3 weapon harms.
        const auto ready_the_bolts = [](RouteBeat &beat, bool blessed)
        {
            for (const char *bearer : {"paladin 1", "paladin 2", "ranger 3"})
            {
                beat.ready_types.emplace_back(27, bearer);
                beat.ready_types.emplace_back(12, bearer);
                if (blessed)
                {
                    beat.ready_types.emplace_back(74, bearer);
                }
            }
        };
        // West to the rakshasas and Banites at their altar (3,14, the
        // cluebook's 10), the only way to the ruins' west side, and back to
        // the houses to rest. The altar's nine Banite clerics and seven
        // rakshasas, which take half from weapons below +3 and no spell below
        // the eighth level, fall to the quarrels +4 and the swords +3.
        walk_ruins_unrested({{6, 13, D::south},
                             {6, 14, D::west},
                             {5, 14, D::north},
                             {5, 13, D::west},
                             {4, 13, D::west}});
        auto banites = step_beat(66, 3, 13, D::south, {"YES"});
        banites.camp_broken = true;
        ready_the_bolts(banites, true);
        route.push_back(banites);
        walk_ruins_unrested({{3, 14, D::north},
                             {3, 13, D::east},
                             {4, 13, D::east},
                             {5, 13, D::south},
                             {5, 14, D::east},
                             {6, 14, D::north}});
        route.push_back(ruin_rest);
        // Then round the ruins' west side to the door that will not budge
        // (4,4).
        walk_ruins_unrested(
            {{6, 13, D::south}, {6, 14, D::west},  {5, 14, D::north}, {5, 13, D::west},
             {4, 13, D::west},  {3, 13, D::south}, {3, 14, D::west},  {2, 14, D::west},
             {1, 14, D::north}, {1, 13, D::west},  {0, 13, D::north}, {0, 12, D::north},
             {0, 11, D::north}, {0, 10, D::east},  {1, 10, D::north}, {1, 9, D::north},
             {1, 8, D::north},  {1, 7, D::west},   {0, 7, D::north},  {0, 6, D::east},
             {1, 6, D::east},   {2, 6, D::east},   {3, 6, D::east},   {4, 6, D::north},
             {4, 5, D::north}});
        // The door will not budge: the magic-user, of Dexterity 18 (17 will
        // do), climbs over and secures a rope (CLIMB OVER, WHO; cell 1D7h
        // bit 1).
        route.push_back(step_beat(66, 4, 4, D::north, {"CLIMB OVER", "magic_user 6"}));
        // Beyond it, round to the house west of the temple (3,0), where the
        // party rests, to the temple's door (5,0).
        walk_ruins_unrested({{4, 3, D::east},
                             {5, 3, D::south},
                             {5, 4, D::east},
                             {6, 4, D::north},
                             {6, 3, D::east},
                             {7, 3, D::north},
                             {7, 2, D::north},
                             {7, 1, D::north},
                             {7, 0, D::west},
                             {6, 0, D::west},
                             {5, 0, D::west},
                             {4, 0, D::west}});
        auto temple_rest = step_beat(66, 3, 0, D::east);
        temple_rest.rest_first = true;
        temple_rest.rest_for_spells = true;
        temple_rest.memorize_first_of = {"dispel_magic"};
        route.push_back(temple_rest);
        walk_ruins_unrested({{4, 0, D::east}});
        // Into the temple (5,1, the cluebook's 16), where the Rakshasa
        // Maharajah raises Tyranthraxus: the party smashes the knights'
        // globes and beats the Maharajah, three Rukhs, two Rajahs and seven
        // rakshasas who rush in (cell 1D8h FFh).
        auto maharajah = step_beat(66, 5, 0, D::south, {"SMASH GLOBES"});
        maharajah.camp_broken = true;
        ready_the_bolts(maharajah, true);
        maharajah.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x1D8, 255};
        route.push_back(maharajah);
        // Nacacia smiles and bids the party back to the glen: it walks back
        // over the door it climbed, past the altar and round to the broken
        // gate (15,4), the way the planning aid finds, and leaves Myth
        // Drannor stepping east from it (YES; script 66's step entry asks
        // on event 1 facing east).
        walk_ruins_unrested(
            {{5, 1, D::north},  {5, 0, D::east},  {6, 0, D::east},   {7, 0, D::south},
             {7, 1, D::south},  {7, 2, D::south}, {7, 3, D::south},  {7, 4, D::west},
             {6, 4, D::west},   {5, 4, D::north}, {5, 3, D::west},   {4, 3, D::south},
             {4, 4, D::south},  {4, 5, D::south}, {4, 6, D::west},   {3, 6, D::west},
             {2, 6, D::west},   {1, 6, D::west},  {0, 6, D::south},  {0, 7, D::east},
             {1, 7, D::south},  {1, 8, D::south}, {1, 9, D::south},  {1, 10, D::south},
             {1, 11, D::south}, {1, 12, D::west}, {0, 12, D::south}, {0, 13, D::east},
             {1, 13, D::south}, {1, 14, D::east}, {2, 14, D::east},  {3, 14, D::north},
             {3, 13, D::north}, {3, 12, D::east}, {4, 12, D::north}, {4, 11, D::north},
             {4, 10, D::north}, {4, 9, D::north}, {4, 8, D::north},  {4, 7, D::east},
             {5, 7, D::east},   {6, 7, D::east},  {7, 7, D::north},  {7, 6, D::north},
             {7, 5, D::east},   {8, 5, D::north}, {8, 4, D::east},   {9, 4, D::east},
             {10, 4, D::east},  {11, 4, D::east}, {12, 4, D::east},  {13, 4, D::east},
             {14, 4, D::east}});
        // Nacacia fell in the temple, so the knights carry her body away and
        // see the party out (ECL1:66 0x8668); outside it declines to enter
        // again (NO).
        route.push_back(step_beat(66, 15, 4, D::east, {"YES", "NO"}));
        // North-east across the area map to the red tower (5,10), fleeing
        // what roams there, and in (YES): the garden of the Red Tower (map 70,
        // script 70).
        RouteBeat to_red_tower{17, 0, 0, D::north, {}};
        to_red_tower.passing = true;
        to_red_tower.fleeing = true;
        to_red_tower.area_steps = {C::northeast, C::northeast, C::northeast};
        route.push_back(to_red_tower);
        RouteBeat into_red_tower{17, 0, 0, D::north, {"YES"}};
        into_red_tower.area_steps = {C::east};
        route.push_back(into_red_tower);
        // The Tower of Marcus (map 70, script 70; the cluebook's 51 to 56):
        // its levels are parts of one map, the level in cell 17Ah. Through
        // the garden of walking trees to the path down at 7,10 (THE PATH
        // GOES DOWN, YES) to the dungeons of level 0.
        walk_passages(route, 70,
                      {{4, 15, D::north},
                       {4, 14, D::north},
                       {4, 13, D::east},
                       {5, 13, D::east},
                       {6, 13, D::east},
                       {7, 13, D::east},
                       {8, 13, D::north},
                       {8, 12, D::west},
                       {7, 12, D::north},
                       {7, 11, D::west},
                       {6, 11, D::north},
                       {6, 10, D::east}});
        route.push_back(step_beat(70, 7, 10, D::east, {"YES"}));
        // North through the dungeons (it wraps to the map's south edge) and
        // past the guards to Shal's cell (10,9, the cluebook's 11): the
        // party agrees to take her to the top of the tower (YES).
        walk_passages(route, 70,
                      {{9, 0, D::north},
                       {9, 15, D::north},
                       {9, 14, D::north},
                       {9, 13, D::north},
                       {9, 12, D::north},
                       {9, 11, D::east},
                       {10, 11, D::east},
                       {11, 11, D::east},
                       {12, 11, D::north},
                       {12, 10, D::east},
                       {13, 10, D::north},
                       {13, 9, D::west},
                       {12, 9, D::west}});
        route.push_back(step_beat(70, 11, 9, D::west, {"YES"}));
        // With Shal, back past the guards' room and up the dungeons' path
        // (9,0, THE PATH GOES UP) to the garden, and up the lower tower's
        // levels 1 to 8 by their paths, the way the planning aid finds
        // (each path a YES; level 6 up on the tower's second map, 71). The
        // guards' reinforcements meet the party on the way out of the
        // dungeons (cell 176h, the row their runners left by), and every
        // camp there drew the tower's wandering mages and monsters, so it
        // walks on hurt, its clerics curing the badly hurt.
        walk_passages(route, 70,
                      {{10, 9, D::east},
                       {11, 9, D::east},
                       {12, 9, D::east},
                       {13, 9, D::south},
                       {13, 10, D::west},
                       {12, 10, D::south},
                       {12, 11, D::south},
                       {12, 12, D::south},
                       {12, 13, D::south},
                       {12, 14, D::south},
                       {12, 15, D::west},
                       {11, 15, D::west},
                       {10, 15, D::west},
                       {9, 15, D::south}});
        for (auto beat = route.end() - 14; beat != route.end(); ++beat)
        {
            beat->camp_broken = true;
            beat->cure_first = true;
        }
        route.push_back(step_beat(70, 9, 0, D::west, {"YES"}));
        // Through the garden, whose camps drew the tower's monsters too.
        walk_passages(route, 70, {{7, 10, D::west}, {6, 10, D::west}, {5, 10, D::west}});
        for (auto beat = route.end() - 3; beat != route.end(); ++beat)
        {
            beat->camp_broken = true;
            beat->cure_first = true;
        }
        route.push_back(step_beat(70, 4, 10, D::west, {"YES"}));
        walk_passages(route, 70,
                      {{13, 0, D::south},
                       {13, 1, D::south},
                       {13, 2, D::south},
                       {13, 3, D::west},
                       {12, 3, D::west},
                       {11, 3, D::west},
                       {10, 3, D::north},
                       {10, 2, D::west},
                       {9, 2, D::north},
                       {9, 1, D::west},
                       {8, 1, D::north}});
        // The quiet rooms of the lower tower's levels (map 70's event 30:
        // IT SEEMS SAFE TO REST IN; elsewhere its camps drew the tower's
        // mages and monsters): the party steps in, rests until whole, its
        // spells memorized, and steps out by the way it came.
        const auto rest_in_quiet_room = [&route](unsigned map, unsigned x, unsigned y, D out)
        {
            auto quiet_room = step_beat(map, x, y, out);
            quiet_room.rest_first = true;
            quiet_room.rest_for_spells = true;
            quiet_room.rest_through_period = true;
            route.push_back(quiet_room);
        };
        // Level 1's (8,0).
        rest_in_quiet_room(70, 8, 0, D::south);
        walk_passages(route, 70, {{8, 1, D::east}, {9, 1, D::east}, {10, 1, D::north}});
        route.push_back(step_beat(70, 10, 0, D::west, {"YES"}));
        walk_passages(route, 70,
                      {{1, 4, D::west},
                       {0, 4, D::south},
                       {0, 5, D::south},
                       {0, 6, D::south},
                       {0, 7, D::east},
                       {1, 7, D::east},
                       {2, 7, D::north},
                       {2, 6, D::east},
                       {3, 6, D::east},
                       {4, 6, D::east},
                       {5, 6, D::east},
                       {6, 6, D::east},
                       {7, 6, D::south}});
        // Level 2's quiet room (7,7), before the drow and the cleric's troops.
        rest_in_quiet_room(70, 7, 7, D::north);
        walk_passages(route, 70,
                      {{7, 6, D::west},
                       {6, 6, D::west},
                       {5, 6, D::west},
                       {4, 6, D::west},
                       {3, 6, D::west},
                       {2, 6, D::north},
                       {2, 5, D::east},
                       {3, 5, D::east},
                       {4, 5, D::east},
                       {5, 5, D::north},
                       {5, 4, D::west},
                       {4, 4, D::west},
                       {3, 4, D::west}});
        route.push_back(step_beat(70, 2, 4, D::west, {"YES"}));
        walk_passages(route, 70,
                      {{1, 0, D::west},
                       {0, 0, D::south},
                       {0, 1, D::south},
                       {0, 2, D::south},
                       {0, 3, D::east},
                       {1, 3, D::east},
                       {2, 3, D::north},
                       {2, 2, D::east},
                       {3, 2, D::east},
                       {4, 2, D::east},
                       {5, 2, D::east},
                       {6, 2, D::south},
                       {6, 3, D::east},
                       {7, 3, D::north},
                       {7, 2, D::north},
                       {7, 1, D::north},
                       {7, 0, D::west}});
        route.push_back(step_beat(70, 6, 0, D::west, {"YES"}));
        walk_passages(route, 70,
                      {{13, 4, D::south},
                       {13, 5, D::south},
                       {13, 6, D::south},
                       {13, 7, D::west},
                       {12, 7, D::west},
                       {11, 7, D::west},
                       {10, 7, D::west},
                       {9, 7, D::west},
                       {8, 7, D::north},
                       {8, 6, D::north},
                       {8, 5, D::east},
                       {9, 5, D::east},
                       {10, 5, D::south},
                       {10, 6, D::east},
                       {11, 6, D::north},
                       {11, 5, D::north},
                       {11, 4, D::west}});
        route.push_back(step_beat(70, 10, 4, D::west, {"YES"}));
        // Level 6: round by the quiet room north of the arrival (2,0).
        const auto rest_in_level_six_room = [&route, &rest_in_quiet_room]
        {
            walk_passages(route, 71,
                          {{1, 3, D::north}, {1, 2, D::north}, {1, 1, D::east}, {2, 1, D::north}});
            rest_in_quiet_room(71, 2, 0, D::south);
            walk_passages(route, 71, {{2, 1, D::west}, {1, 1, D::south}, {1, 2, D::south}});
        };
        walk_passages(route, 71,
                      {{1, 0, D::west},
                       {0, 0, D::south},
                       {0, 1, D::south},
                       {0, 2, D::south},
                       {0, 3, D::east}});
        rest_in_level_six_room();
        walk_passages(route, 71,
                      {{1, 3, D::east},
                       {2, 3, D::east},
                       {3, 3, D::south},
                       {3, 4, D::east},
                       {4, 4, D::east},
                       {5, 4, D::east},
                       {6, 4, D::east},
                       {7, 4, D::north},
                       {7, 3, D::east},
                       {8, 3, D::north},
                       {8, 2, D::east},
                       {9, 2, D::south},
                       {9, 3, D::south}});
        // Rested and its spells memorized, it walks into the bits of Moander,
        // the cultists who follow them and the Black Circle's mages (10,4,
        // the cluebook's 12), and back to the quiet room to rest again.
        auto moanders_monsters = step_beat(71, 9, 4, D::east);
        moanders_monsters.passing = true;
        route.push_back(moanders_monsters);
        walk_passages(route, 71,
                      {{10, 4, D::west},
                       {9, 4, D::north},
                       {9, 3, D::north},
                       {9, 2, D::west},
                       {8, 2, D::south},
                       {8, 3, D::west},
                       {7, 3, D::south},
                       {7, 4, D::west},
                       {6, 4, D::west},
                       {5, 4, D::west},
                       {4, 4, D::west},
                       {3, 4, D::north},
                       {3, 3, D::west},
                       {2, 3, D::west}});
        rest_in_level_six_room();
        walk_passages(route, 71,
                      {{1, 3, D::east},
                       {2, 3, D::east},
                       {3, 3, D::south},
                       {3, 4, D::east},
                       {4, 4, D::east},
                       {5, 4, D::east},
                       {6, 4, D::east},
                       {7, 4, D::north},
                       {7, 3, D::east},
                       {8, 3, D::north},
                       {8, 2, D::east},
                       {9, 2, D::south},
                       {9, 3, D::south},
                       {9, 4, D::east}});
        walk_passages(route, 71,
                      {{10, 4, D::east},
                       {11, 4, D::north},
                       {11, 3, D::north},
                       {11, 2, D::north},
                       {11, 1, D::north},
                       {11, 0, D::west}});
        route.push_back(step_beat(71, 10, 0, D::west, {"YES"}));
        walk_passages(route, 71,
                      {{10, 5, D::west}, {9, 5, D::west},  {8, 5, D::south}, {8, 6, D::west},
                       {7, 6, D::west},  {6, 6, D::west},  {5, 6, D::north}, {5, 5, D::west},
                       {4, 5, D::west},  {3, 5, D::west},  {2, 5, D::west},  {1, 5, D::west},
                       {0, 5, D::south}, {0, 6, D::south}, {0, 7, D::east},  {1, 7, D::south},
                       {1, 8, D::west},  {0, 8, D::south}, {0, 9, D::east},  {1, 9, D::east},
                       {2, 9, D::east},  {3, 9, D::east},  {4, 9, D::east}});
        // Level 7's pincer (the cluebook's 16 and 17): through the door at
        // (6,9) the party hears wizards ahead and others behind; it taunts
        // the group to the west (TAUNT) and ducks into the closet north
        // (6,8, WAIT HERE? YES), where the two factions destroy each other
        // (cell 177h), and walks on past their bodies.
        route.push_back(step_beat(71, 5, 9, D::east, {"TAUNT"}));
        route.push_back(step_beat(71, 6, 9, D::north, {"YES"}));
        walk_passages(route, 71,
                      {{6, 8, D::south},
                       {6, 9, D::east},
                       {7, 9, D::east},
                       {8, 9, D::east},
                       {9, 9, D::east},
                       {10, 9, D::north},
                       {10, 8, D::north},
                       {10, 7, D::north},
                       {10, 6, D::east},
                       {11, 6, D::east},
                       {12, 6, D::north},
                       {12, 5, D::west}});
        route.push_back(step_beat(71, 11, 5, D::west, {"YES"}));
        // Level 8: west round the guard rooms to the sortie against Phlan
        // (the cluebook's 22), where Shal bluffs the many guards before the
        // wizard gate by impersonating Petra (TRY IT? YES), and the party
        // hurries past through the gate (23) into the upper tower (script
        // 67), which locks behind it.
        walk_passages(
            route, 71,
            {{11, 10, D::west},  {10, 10, D::south}, {10, 11, D::east},  {11, 11, D::east},
             {12, 11, D::south}, {12, 12, D::south}, {12, 13, D::south}, {12, 14, D::west},
             {11, 14, D::west},  {10, 14, D::west},  {9, 14, D::west},   {8, 14, D::north},
             {8, 13, D::north},  {8, 12, D::north},  {8, 11, D::north},  {8, 10, D::west},
             {7, 10, D::south},  {7, 11, D::west},   {6, 11, D::south},  {6, 12, D::south},
             {6, 13, D::south},  {6, 14, D::west},   {5, 14, D::west},   {4, 14, D::west},
             {3, 14, D::west},   {2, 14, D::west},   {1, 14, D::west},   {0, 14, D::north},
             {0, 13, D::north},  {0, 12, D::north},  {0, 11, D::east},   {1, 11, D::south},
             {1, 12, D::east},   {2, 12, D::east}});
        // The guards' event (3,12) runs as a step through their door is
        // held back: the party hears the troops mustering and does not go on
        // (NO), and Shal offers her bluff (YES).
        route.push_back(step_beat(71, 3, 12, D::north, {"NO", "YES"}));
        // The upper tower (map 67, script 67): level 9 is its rows 0 to 4,
        // level 10 rows 5 to 9 and level 11 rows 10 to 15. Each faction the
        // party breaks up sets a bit of cell 251h; with all eight Marcus
        // stands in his chambers. Its camps are broken by the tower's
        // servants but in its quiet side rooms (script 67's camp entry: no
        // interruption on event 27), so the party walks on hurt between them,
        // its clerics' cures cast where a member is below half.
        const auto walk_tower =
            [&route](std::initializer_list<std::tuple<unsigned, unsigned, D>> steps)
        {
            walk_passages(route, 67, steps);
            for (auto beat = route.end() - static_cast<std::ptrdiff_t>(steps.size());
                 beat != route.end(); ++beat)
            {
                beat->camp_broken = true;
                beat->cure_first = true;
            }
        };
        const auto tower_step =
            [&route](unsigned x, unsigned y, D facing, std::vector<std::string> answers)
        {
            auto beat = step_beat(67, x, y, facing, std::move(answers));
            beat.camp_broken = true;
            beat.cure_first = true;
            route.push_back(beat);
        };
        // First round to the side room at 2,0 (IT SEEMS SAFE TO REST IN),
        // where the party rests.
        walk_tower({{1, 0, D::west},
                    {0, 0, D::south},
                    {0, 1, D::south},
                    {0, 2, D::south},
                    {0, 3, D::east},
                    {1, 3, D::north},
                    {1, 2, D::north},
                    {1, 1, D::east},
                    {2, 1, D::north}});
        auto upper_rest = step_beat(67, 2, 0, D::south);
        upper_rest.rest_first = true;
        upper_rest.rest_for_spells = true;
        upper_rest.rest_through_period = true;
        route.push_back(upper_rest);
        // East along level 9 to the altar of the Cult of Moander (15,2, the
        // cluebook's 3): the party cuts down the cultists coiled in the vines
        // (YES), and the cultists charge (cell 251h bit 0).
        walk_tower({{2, 1, D::east},
                    {3, 1, D::north},
                    {3, 0, D::east},
                    {4, 0, D::east},
                    {5, 0, D::east},
                    {6, 0, D::east},
                    {7, 0, D::east},
                    {8, 0, D::south},
                    {8, 1, D::south},
                    {8, 2, D::south},
                    {8, 3, D::east},
                    {9, 3, D::east},
                    {10, 3, D::east},
                    {11, 3, D::east},
                    {12, 3, D::east},
                    {13, 3, D::north},
                    {13, 2, D::east}});
        tower_step(14, 2, D::east, {"YES"});
        // West to the path down to level 10 (11,2, THE PATH GOES DOWN).
        walk_tower({{15, 2, D::west}, {14, 2, D::west}, {13, 2, D::west}, {12, 2, D::west}});
        tower_step(11, 2, D::west, {"YES"});
        // Level 10: west to its quiet room (5,9), where the party rests.
        walk_tower({{10, 7, D::north},
                    {10, 6, D::north},
                    {10, 5, D::west},
                    {9, 5, D::west},
                    {8, 5, D::west},
                    {7, 5, D::west},
                    {6, 5, D::west},
                    {5, 5, D::south},
                    {5, 6, D::south},
                    {5, 7, D::west},
                    {4, 7, D::west},
                    {3, 7, D::west},
                    {2, 7, D::south},
                    {2, 8, D::south},
                    {2, 9, D::east},
                    {3, 9, D::east},
                    {4, 9, D::east}});
        auto quiet_room_10 = step_beat(67, 5, 9, D::west);
        quiet_room_10.rest_first = true;
        quiet_room_10.rest_for_spells = true;
        quiet_room_10.rest_through_period = true;
        route.push_back(quiet_room_10);
        // East to the antechamber of the Red Wizards (13,5, the cluebook's
        // 5): the party waits there (YES) and overhears the code word, RED.
        // Through the door south it surprises two Red Wizards (13,6; cell
        // B0h 1), and at the loft's door (14,7, the cluebook's 6) it raps
        // (YES) and speaks the word: the coven inside falls on another, and
        // the party beats the survivors (cell 251h bit 1). It does not step
        // into the antechamber again, which would spoil the word (cell B0h).
        walk_tower({{4, 9, D::west},
                    {3, 9, D::west},
                    {2, 9, D::north},
                    {2, 8, D::north},
                    {2, 7, D::north},
                    {2, 6, D::east},
                    {3, 6, D::east},
                    {4, 6, D::east},
                    {5, 6, D::north},
                    {5, 5, D::east},
                    {6, 5, D::east},
                    {7, 5, D::east},
                    {8, 5, D::east},
                    {9, 5, D::east},
                    {10, 5, D::east},
                    {11, 5, D::east}});
        tower_step(12, 5, D::east, {"YES"});
        tower_step(13, 5, D::south, {});
        tower_step(13, 6, D::south, {});
        tower_step(13, 7, D::east, {"YES", "type:RED"});
        // West to the lofts of the Black Circle's mages (the cluebook's 9),
        // each attacked through its door in turn as small groups (cell 251h
        // bit 2), then the Fountains of Moander (1,8, the cluebook's 10),
        // whose clerics and monsters fall (bit 3), and the Black Circle's
        // last, formed outside it (1,7).
        walk_tower({{14, 7, D::north},
                    {14, 6, D::west},
                    {13, 6, D::north},
                    {13, 5, D::west},
                    {12, 5, D::west},
                    {11, 5, D::west},
                    {10, 5, D::west},
                    {9, 5, D::west},
                    {8, 5, D::west},
                    {7, 5, D::west},
                    {6, 5, D::west},
                    {5, 5, D::south},
                    {5, 6, D::west}});
        for (unsigned x = 4; x >= 1; --x)
        {
            tower_step(x, 6, D::north, {});
            tower_step(x, 5, D::south, {});
            if (x > 1)
            {
                tower_step(x, 6, D::west, {});
            }
        }
        tower_step(1, 6, D::west, {});
        tower_step(0, 6, D::east, {});
        // The fountain's room opens only by the doors where the Black Circle
        // forms, so the party rests in the quiet room (5,9) first and
        // goes in whole, round by the corridor west of it (1,7, no door).
        walk_tower({{1, 6, D::east},
                    {2, 6, D::south},
                    {2, 7, D::south},
                    {2, 8, D::south},
                    {2, 9, D::east},
                    {3, 9, D::east},
                    {4, 9, D::east}});
        route.push_back(quiet_room_10);
        walk_tower({{4, 9, D::west},
                    {3, 9, D::west},
                    {2, 9, D::north},
                    {2, 8, D::north},
                    {2, 7, D::west}});
        tower_step(1, 7, D::south, {});
        tower_step(1, 8, D::north, {});
        // Back to the quiet room to rest.
        walk_tower({{1, 7, D::east},
                    {2, 7, D::south},
                    {2, 8, D::south},
                    {2, 9, D::east},
                    {3, 9, D::east},
                    {4, 9, D::east}});
        route.push_back(quiet_room_10);
        // Down the path at 6,9 (THE PATH GOES DOWN, YES) to level 11 and
        // north to the Manufactory of Moander's Red Wizards (7,11, the
        // cluebook's 14), entered by its door: the party beats them and
        // their walking disasters (cell 251h bit 4).
        walk_tower({{4, 9, D::west},
                    {3, 9, D::west},
                    {2, 9, D::north},
                    {2, 8, D::north},
                    {2, 7, D::east},
                    {3, 7, D::east},
                    {4, 7, D::east},
                    {5, 7, D::south},
                    {5, 8, D::east},
                    {6, 8, D::south}});
        tower_step(6, 9, D::west, {"YES"});
        walk_tower({{5, 15, D::north},
                    {5, 14, D::north},
                    {5, 13, D::north},
                    {5, 12, D::north},
                    {5, 11, D::north},
                    {5, 10, D::east},
                    {6, 10, D::east},
                    {7, 10, D::east},
                    {8, 10, D::south}});
        tower_step(8, 11, D::west, {});
        // Round by the west corridor to the Black Circle's manufactory (7,14,
        // the cluebook's 12), whose wearied mages it attacks at once
        // (ATTACK; bit 5), through it to the Cult of Moander's (9,13, the
        // cluebook's 13; cultists, then the bits of Moander, bit 6), and on
        // to the level's quiet room (11,15), where it rests.
        walk_tower({{7, 11, D::east},
                    {8, 11, D::north},
                    {8, 10, D::west},
                    {7, 10, D::west},
                    {6, 10, D::west},
                    {5, 10, D::south},
                    {5, 11, D::south},
                    {5, 12, D::south},
                    {5, 13, D::south},
                    {5, 14, D::east}});
        tower_step(6, 14, D::east, {"ATTACK"});
        walk_tower({{7, 14, D::east}, {8, 14, D::south}, {8, 15, D::east}, {9, 15, D::north}});
        tower_step(9, 14, D::north, {});
        walk_tower({{9, 13, D::south},
                    {9, 14, D::east},
                    {10, 14, D::east},
                    {11, 14, D::east},
                    {12, 14, D::south}});
        auto quiet_room_11 = step_beat(67, 12, 15, D::west);
        quiet_room_11.camp_broken = true;
        route.push_back(quiet_room_11);
        auto rest_11 = step_beat(67, 11, 15, D::east);
        rest_11.rest_first = true;
        rest_11.rest_for_spells = true;
        rest_11.rest_through_period = true;
        route.push_back(rest_11);
        // East through the door into the Dispersion Diagram (14,15, the
        // cluebook's 15): seeing it disrupts the last of Marcus's operations
        // (bit 7), and the party moves on rather than break the beasts'
        // circle (MOVE).
        walk_tower({{12, 15, D::east}});
        tower_step(13, 15, D::east, {"MOVE"});
        // North through the edge of the Temple of Transformation, walking
        // warily among the rapt devotees (MOVE; two squares, before anyone
        // steps on the cat), to the secret underpass (11,10 west, the
        // cluebook's 19) and round to the Chambers of Marcus (1,13, the
        // cluebook's 18): his spectres, the hidden foes and his minions come
        // in waves, then Marcus himself, defending Petra.
        walk_tower({{14, 15, D::north},
                    {14, 14, D::north},
                    {14, 13, D::north},
                    {14, 12, D::north},
                    {14, 11, D::north},
                    {14, 10, D::west},
                    {13, 10, D::west},
                    {12, 10, D::west},
                    {11, 10, D::west},
                    {3, 10, D::west},
                    {2, 10, D::west},
                    {1, 10, D::west},
                    {0, 10, D::south},
                    {0, 11, D::south}});
        // The waves are fought back to back: the spectres, the Minions of
        // Bane breathing for 27 with the Banite clerics holding members for
        // them to slay, the human minions, and Marcus with his Black Circle
        // mages and minions (88 of 120 plays win the last, 2026-11-16).
        auto chambers = step_beat(67, 0, 12, D::south);
        chambers.camp_broken = true;
        route.push_back(chambers);
        // Shal and Petra meet and the woman, now whole, bids the party to
        // the temple: back by the underpass (3,10 east) and north to the
        // Vessel of Moander (13,9, the cluebook's 20), which reels as the
        // tower falls; the party leaps aboard (script 75).
        walk_tower({{1, 14, D::north},
                    {1, 13, D::west},
                    {0, 13, D::north},
                    {0, 12, D::north},
                    {0, 11, D::north},
                    {0, 10, D::east},
                    {1, 10, D::east},
                    {2, 10, D::east},
                    {3, 10, D::east},
                    {11, 10, D::east},
                    {12, 10, D::east}});
        tower_step(13, 10, D::north, {"stop:REST"});
        // In Limbo Shal leaves the party, and Elminster's camp rests it. The
        // tower's wandering mages, cultists and bits of Moander a player
        // meets on its levels are a stand-in: 500,000 experience each,
        // trained here (up to two levels). Then on to Moander (MOVE ON,
        // MOANDER).
        RouteBeat tower_rest{18, 0, 0, D::north, {}};
        tower_rest.rest_choice = "REST";
        route.push_back(tower_rest);
        RouteBeat to_moander{18, 0, 0, D::north,
                             train_darkness_party(2, {"TRAIN"}, {"MOVE ON", "MOANDER"})};
        to_moander.waiting = true;
        to_moander.test_experience = 500'000;
        route.push_back(to_moander);
        // Moander (scripts 51 to 53; the cluebook's 57 to 60): the party
        // arrives by the gate in the depression (map 52) and climbs out by
        // its footholds (YES) onto Moander's body, an area map (script 51).
        route.push_back(step_beat(52, 7, 10, D::north, {"YES", "NO"}));
        // Over the body (script 51: an area map of 38 by 15 places, cells 25h
        // and 26h) south-east and south-west to the great wound in Moander's
        // arm (15,13; the cluebook's Large Wound), fleeing the bits of
        // Moander, cultists and Bane's minions that roam it, and in (DO YOU
        // ENTER? YES): script 52 places the party in the wound's part of map
        // 52.
        RouteBeat to_the_wound{52, 0, 0, D::north, {"YES"}};
        to_the_wound.area_steps = {C::southeast, C::southeast, C::southeast,
                                   C::southeast, C::southeast, C::southeast,
                                   C::southwest, C::southwest, C::southwest};
        to_the_wound.fleeing = true;
        route.push_back(to_the_wound);
        // In the wound (map 52 from 12,15) camps rest only with Aeghwaet once
        // he is helped (the cluebook), so the party walks hurt, its clerics
        // curing the badly hurt.
        const auto walk_wound =
            [&route](std::initializer_list<std::tuple<unsigned, unsigned, D>> steps)
        {
            walk_passages(route, 52, steps);
            for (auto beat = route.end() - static_cast<std::ptrdiff_t>(steps.size());
                 beat != route.end(); ++beat)
            {
                beat->camp_broken = true;
                beat->cure_first = true;
            }
        };
        // North to Aeghwaet (12,12, event 11; the cluebook's 7), who asks the
        // party to rid his home of Tanetal's minions (YES; cell 232h bit 7).
        walk_wound({{12, 15, D::north}, {12, 14, D::north}});
        route.push_back(step_beat(52, 12, 13, D::north, {"YES"}));
        // The minions mining the flesh (events 14 to 17; the cluebook's 8),
        // fought in the order cell 233h keeps wherever they are met: (6,11),
        // (11,10), (10,15) and (14,13), each a bit of cell 239h.
        walk_wound({{12, 12, D::north},
                    {12, 11, D::north},
                    {12, 10, D::north},
                    {12, 9, D::north},
                    {12, 8, D::west},
                    {11, 8, D::west},
                    {10, 8, D::west},
                    {9, 8, D::south},
                    {9, 9, D::south},
                    {9, 10, D::south},
                    {9, 11, D::west},
                    {8, 11, D::west},
                    {7, 11, D::west}});
        walk_wound({{6, 11, D::east},
                    {7, 11, D::east},
                    {8, 11, D::east},
                    {9, 11, D::south},
                    {9, 12, D::south},
                    {9, 13, D::east},
                    {10, 13, D::east},
                    {11, 13, D::north},
                    {11, 12, D::north},
                    {11, 11, D::north}});
        // The second, a swarm of six bits of Moander (81 Hit Points, two
        // blows of up to 23, a poisoning one), the party beats since the
        // computer casts as the original's does.
        walk_wound({{11, 10, D::south},
                    {11, 11, D::south},
                    {11, 12, D::south},
                    {11, 13, D::west},
                    {10, 13, D::west},
                    {9, 13, D::south},
                    {9, 14, D::south},
                    {9, 15, D::east}});
        walk_wound({{10, 15, D::west},  {9, 15, D::north},  {9, 14, D::north}, {9, 13, D::north},
                    {9, 12, D::north},  {9, 11, D::north},  {9, 10, D::north}, {9, 9, D::north},
                    {9, 8, D::east},    {10, 8, D::east},   {11, 8, D::east},  {12, 8, D::east},
                    {13, 8, D::east},   {14, 8, D::east},   {15, 8, D::south}, {15, 9, D::south},
                    {15, 10, D::south}, {15, 11, D::south}, {15, 12, D::west}, {14, 12, D::south}});
        // Back to Aeghwaet's reward (12,8, event 13; the cluebook's 9): the
        // two potions the heart needs (cell 23Bh 1), and his healing (YES).
        walk_wound({{14, 13, D::north},
                    {14, 12, D::east},
                    {15, 12, D::north},
                    {15, 11, D::north},
                    {15, 10, D::north},
                    {15, 9, D::north},
                    {15, 8, D::west},
                    {14, 8, D::west}});
        auto reward = step_beat(52, 13, 8, D::west, {"YES"});
        reward.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x23B, 1};
        route.push_back(reward);
        // Aeghwaet helped, the wound's camps rest (the cluebook): the party
        // rests whole, walks back south to the wound's opening (12,15) and
        // leaves (DO YOU WANT TO LEAVE? YES) for Moander's body.
        auto rest_wound = step_beat(52, 12, 8, D::south);
        rest_wound.passing = true;
        rest_wound.rest_first = true;
        rest_wound.rest_for_spells = true;
        route.push_back(rest_wound);
        walk_passages(route, 52,
                      {{12, 9, D::south},
                       {12, 10, D::south},
                       {12, 11, D::south},
                       {12, 12, D::south},
                       {12, 13, D::south},
                       {12, 14, D::south}});
        route.push_back(step_beat(52, 12, 15, D::south, {"YES", "NO"}));
        // Over the body north and north-east to the wound in Moander's side
        // (21,3; the cluebook's Entrance to Heart), fleeing what roams it,
        // and in (DO YOU ENTER? YES).
        RouteBeat to_the_side{52, 0, 0, D::north, {"YES"}};
        to_the_side.area_steps = {C::north,     C::north,     C::north,     C::north,
                                  C::northeast, C::northeast, C::northeast, C::northeast,
                                  C::northeast, C::northeast};
        to_the_side.fleeing = true;
        route.push_back(to_the_side);
        // The Entrance to Heart (map 52 from 8,0), whose camps rest (the
        // cluebook): rested, the party walks south to the lung's muscle wall
        // (8,5, event 29; the cluebook's 25), spreads Aeghwaet's potion on it
        // (USE POTIONS) and cuts through (YES) into the heart (script 53).
        auto rest_entrance = step_beat(52, 8, 0, D::south);
        rest_entrance.passing = true;
        rest_entrance.rest_first = true;
        rest_entrance.rest_for_spells = true;
        // The magic-user takes Hastes for the heart's two great fights, its
        // iron golems and Tanetal's last stand (the camp before the heart; no
        // camp in it).
        rest_entrance.memorize_first_of = {"haste"};
        route.push_back(rest_entrance);
        walk_passages(route, 52, {{8, 1, D::south}, {8, 2, D::south}, {8, 3, D::south}});
        route.push_back(step_beat(52, 8, 4, D::south, {"USE POTIONS", "YES"}));
        // Moander's heart (map 53, script 53; its words in script 55): a
        // camp there draws Tanetal (the cluebook), so the party walks hurt,
        // its clerics curing. The heart's story comes to a party wandering
        // its muscle tissue (event 0: 1 step in 20, then 1 in 10), each part
        // a bit of cell 24Ah: the army's remains (32), the Watcher's first
        // scolding (4), Tanetal slaying two soldiers (1). The party paces
        // the tissue east of where it swam in (4,14 to 4,15) until each has
        // come, as a player wanders it.
        route.push_back(step_beat(53, 2, 15, D::east));
        for (const std::uint16_t bit : {32, 4, 1})
        {
            auto pace = step_beat(53, 4, 14, D::south);
            pace.repeat_in_place = true;
            pace.cure_first = true;
            pace.camp_broken = true;
            pace.repeat_until_bits = std::pair<std::uint16_t, std::uint16_t>{0x24A, bit};
            route.push_back(pace);
        }
        // The heart's walks: the clerics cure the badly hurt, no camp.
        // The last step from (x, y) plays with other dice, as a player
        // reloads where agents of Tanetal no play beats came on the party.
        const auto reload_at = [&route](unsigned x, unsigned y, unsigned times)
        {
            const auto beat = std::find_if(route.rbegin(), route.rend(),
                                           [&](const RouteBeat &b)
                                           {
                                               return b.map == 53 && b.x == x && b.y == y;
                                           });
            beat->reloads = times;
        };
        const auto walk_heart =
            [&route](std::initializer_list<std::tuple<unsigned, unsigned, D>> steps)
        {
            walk_passages(route, 53, steps);
            for (auto beat = route.end() - static_cast<std::ptrdiff_t>(steps.size());
                 beat != route.end(); ++beat)
            {
                beat->camp_broken = true;
                beat->cure_first = true;
            }
        };
        // West across the vena cava to the Watcher's spot at 1,13 (event
        // 26, the cluebook's 13; her spots serve her story in turn, cell
        // 242h): she tells the party to mix Aeghwaet's potions, which frees
        // it of the slime (YES; cell 241h 1).
        walk_heart({{4, 15, D::north}, {4, 14, D::north}, {4, 13, D::west}, {3, 13, D::west}});
        auto potions = step_beat(53, 2, 13, D::west, {"YES"});
        potions.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x241, 1};
        route.push_back(potions);
        // The heart beats every 60 steps (cell 9Bh counts them; 243h holds
        // the beat): DUB at 30, when one in an atrium or its vessels (events
        // 20 to 23) is drained into a ventricle, and LUB at 60, when one in a
        // ventricle or the vessels it drains (events 10 to 18) is flushed
        // from the heart. A player times the vessels by the beat, as the
        // mouth's warnings let one: the party paces muscle tissue until the
        // beat it waits for has passed (cell 9Ah: 1 after a LUB, 2 after a
        // DUB).
        const auto pace_heart = [&route](unsigned x, unsigned y, D facing, std::uint16_t beat)
        {
            auto pace = step_beat(53, x, y, facing);
            pace.repeat_in_place = true;
            pace.cure_first = true;
            pace.camp_broken = true;
            pace.repeat_until_bits = std::pair<std::uint16_t, std::uint16_t>{0x9A, beat};
            route.push_back(pace);
        };
        constexpr std::uint16_t after_lub = 1, after_dub = 2;
        // North up the vena cava (2,13 to 2,1) between two DUBs, and east
        // along the pulmonary trunk's loop (6,1 to 10,1) before the LUB, to
        // Tanetal's cache (11,1, event 9; the cluebook's 8): the
        // weapon-making tools (YES; cell 24Ah bit 128).
        pace_heart(0, 13, D::south, after_dub);
        walk_heart({{0, 14, D::north}, {0, 13, D::east},  {1, 13, D::east},  {2, 13, D::north},
                    {2, 12, D::north}, {2, 11, D::north}, {2, 10, D::north}, {2, 9, D::north},
                    {2, 8, D::north},  {2, 7, D::north},  {2, 6, D::north},  {2, 5, D::north},
                    {2, 4, D::north},  {2, 3, D::north},  {2, 2, D::north},  {2, 1, D::east},
                    {3, 1, D::east},   {4, 1, D::east},   {5, 1, D::east},   {6, 1, D::east},
                    {7, 1, D::east},   {8, 1, D::east},   {9, 1, D::east}});
        auto tools = step_beat(53, 10, 1, D::east, {"YES"});
        tools.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x24A, 173};
        route.push_back(tools);
        // After the LUB, back west to the vena cava and south to the arrow
        // in the old wound (4,5, event 5; the cluebook's 7): the tools fashion
        // a magic weapon four times (YES), the fifth try would sting (NO);
        // Moander's pulse quickens (cell 243h 14: a beat every 14 steps) and
        // his dreams trouble the heart (cell 241h 2). The party leaves the
        // weapons: its own are better.
        pace_heart(11, 0, D::south, after_lub);
        walk_heart({{11, 1, D::west},
                    {10, 1, D::west},
                    {9, 1, D::west},
                    {8, 1, D::west},
                    {7, 1, D::west},
                    {6, 1, D::west},
                    {5, 1, D::west},
                    {4, 1, D::west},
                    {3, 1, D::west},
                    {2, 1, D::south},
                    {2, 2, D::south},
                    {2, 3, D::south},
                    {2, 4, D::east},
                    {3, 4, D::east}});
        route.push_back(step_beat(53, 4, 4, D::south, {"YES", "YES", "YES", "YES", "NO"}));
        // Pacing the tissue west of the arrow until a Tanetal comes and the
        // party, with its new weapons, attacks him (ATTACK): a fake, of bits
        // of Moander (cell 24Ch bits 2 and 32).
        {
            auto pace = step_beat(53, 3, 5, D::south, {"ATTACK"});
            pace.repeat_in_place = true;
            pace.cure_first = true;
            pace.camp_broken = true;
            pace.repeat_until_bits = std::pair<std::uint16_t, std::uint16_t>{0x24C, 34};
            route.push_back(pace);
        }
        // East to the Watcher's second spot (7,6, event 30; the cluebook's
        // 11), between LUBs across the vessel (6,6): bits of Moander make off
        // with her unguent, and she asks for the golden cornucopia. Then
        // back west, up the vena cava between DUBs (one step reloaded, where
        // agents of Tanetal no play beats came on the party), along the
        // pulmonary trunk's loop after a LUB to the cornucopia (8,0, event 7;
        // the cluebook's 2: YES, the party carries it together), and down
        // to her third spot (9,4, event 29): she and the party lull Moander
        // to sleep (YES; a beat every 80 steps), and she gives it the tuning
        // fork that unmasks the fakes (cell 241h 3).
        walk_heart({{3, 6, D::north}, {3, 5, D::east}, {4, 5, D::east}});
        pace_heart(5, 5, D::south, after_lub);
        walk_heart({{5, 6, D::east}});
        route.push_back(step_beat(53, 6, 6, D::east, {"YES"}));
        pace_heart(7, 5, D::south, after_lub);
        walk_heart({{7, 6, D::west},
                    {6, 6, D::west},
                    {5, 6, D::west},
                    {4, 6, D::west},
                    {3, 6, D::north},
                    {3, 5, D::north}});
        pace_heart(3, 5, D::north, after_dub);
        walk_heart({{3, 4, D::west}, {2, 4, D::north}, {2, 3, D::north}, {2, 2, D::north}});
        reload_at(2, 2, 1);
        walk_heart({{2, 1, D::east}, {3, 1, D::east}, {4, 1, D::east}});
        pace_heart(4, 1, D::east, after_lub);
        walk_heart({{5, 1, D::east}, {6, 1, D::east}, {7, 1, D::east}});
        route.push_back(step_beat(53, 8, 1, D::north, {"YES"}));
        walk_heart({{8, 0, D::south},
                    {8, 1, D::east},
                    {9, 1, D::east},
                    {10, 1, D::south},
                    {10, 2, D::south}});
        route.push_back(step_beat(53, 10, 4, D::west, {"YES"}));
        // Round by the right atrium and ventricle and up the pulmonary artery
        // (the cluebook's way) to its iron golems (9,3, event 3; the
        // cluebook's 3), which tear the party's weapons away and magnetize
        // them. Six iron golems and four large ones, whom fire heals and no
        // other spell touches but lightning, which slows them: the members
        // fight with their weapons (wasted_on_golems), hasted as they step
        // up (Haste, cast out of a fight, hastes the whole party for 3 rounds
        // and 1 a level; most plays win, 87 of 120, 2026-11-16, where 3 did
        // unhasted). East along the artery, its current carries the party to
        // the unguent (15,3; the cluebook's 4).
        walk_heart({{9, 4, D::east},  {10, 4, D::north}, {10, 2, D::west}, {9, 2, D::west},
                    {8, 2, D::west},  {7, 2, D::west},   {6, 2, D::south}, {6, 4, D::south},
                    {6, 5, D::west},  {5, 5, D::west},   {4, 5, D::west},  {3, 5, D::west},
                    {2, 5, D::south}, {2, 6, D::south},  {2, 7, D::east},  {3, 7, D::east},
                    {4, 7, D::south}, {4, 8, D::south},  {4, 9, D::south}, {4, 10, D::east},
                    {5, 10, D::east}, {6, 10, D::east},  {7, 10, D::east}, {8, 10, D::north},
                    {8, 9, D::north}, {8, 8, D::north},  {8, 7, D::north}, {8, 6, D::north},
                    {8, 5, D::north}, {8, 4, D::north}});
        auto golems = step_beat(53, 8, 3, D::east);
        golems.cure_first = true;
        golems.camp_broken = true;
        // One Haste takes the whole party: it is cast on the first member.
        golems.ward_each_with = "haste";
        golems.ward_members = {"paladin 1"};
        // The clerics take up their maces +3 and shields again, as a player
        // does where only a weapon of +3 or better harms the foe: their
        // staff slings +2 do nothing to an iron golem.
        for (const char *cleric : {"cleric 4", "cleric 5"})
        {
            golems.ready_types.emplace_back(8, cleric);
            golems.ready_types.emplace_back(37, cleric);
        }
        route.push_back(golems);
        walk_heart({{9, 3, D::east}, {10, 3, D::east}});
        route.push_back(step_beat(53, 11, 3, D::east, {"YES"}));
        // Back west and round by the ventricles to the Watcher's fourth spot
        // (10,10, event 27; the cluebook's 14): the unguent given, she finds
        // the party's weapons magnetic. After a LUB, up the artery and round
        // by the aorta into the left ventricle (12,8; the cluebook's 6),
        // where Tanetal has not yet seen it: shocking the wall (YES) flushes
        // him and the party from the heart, and the Talisman of Bane slips
        // free; the party wedges itself in the pulmonary trunk (9,0).
        walk_heart({{15, 3, D::west},  {11, 3, D::west},  {10, 3, D::west},  {9, 3, D::west},
                    {8, 3, D::south},  {8, 4, D::south},  {8, 5, D::south},  {8, 6, D::south},
                    {8, 7, D::south},  {8, 8, D::south},  {8, 9, D::south},  {8, 10, D::west},
                    {7, 10, D::west},  {6, 10, D::west},  {5, 10, D::west},  {4, 10, D::west},
                    {3, 10, D::west},  {2, 10, D::south}, {2, 11, D::south}, {2, 12, D::east},
                    {3, 12, D::east},  {4, 12, D::south}, {4, 13, D::south}, {4, 14, D::east},
                    {5, 14, D::east},  {6, 14, D::east},  {7, 14, D::east},  {8, 14, D::east},
                    {9, 14, D::north}, {9, 13, D::north}, {9, 12, D::north}, {9, 11, D::north}});
        route.push_back(step_beat(53, 9, 10, D::east));
        walk_heart({{10, 10, D::west}, {9, 10, D::south}, {9, 11, D::south}, {9, 12, D::south}});
        pace_heart(9, 13, D::south, after_lub);
        walk_heart({{9, 14, D::north}, {9, 13, D::north},  {9, 12, D::north}, {9, 11, D::north},
                    {9, 10, D::east},  {10, 10, D::north}, {10, 9, D::north}, {10, 7, D::north},
                    {10, 6, D::east},  {11, 6, D::north},  {11, 5, D::north}, {11, 4, D::west},
                    {10, 4, D::north}, {10, 2, D::west},   {9, 2, D::west},   {8, 2, D::west},
                    {7, 2, D::west},   {6, 2, D::south},   {6, 4, D::south},  {6, 5, D::south},
                    {6, 6, D::south},  {6, 7, D::south},   {6, 8, D::east},   {7, 8, D::east},
                    {9, 8, D::east},   {10, 8, D::east}});
        route.push_back(step_beat(53, 11, 8, D::east, {"YES"}));
        // The Watcher's fifth business, at her last spot (15,7, event 28; the
        // cluebook's 15): her trap in the right ventricle (YES, help her).
        // The chase (events 32 to 38, cell A2h counting them; the cluebook's
        // 16) starts at 15,4 and follows her calls: south to 15,15, west to
        // 12,15, north to 12,12, west to 9,12, south to 9,15 and west to
        // 2,15, where Tanetal is in the artery to the north: shocking the
        // wall (YES) drains the party after him into the right ventricle.
        walk_heart({{9, 0, D::south},   {9, 1, D::east},   {10, 1, D::south},  {10, 2, D::south},
                    {10, 4, D::east},   {11, 4, D::south}, {11, 5, D::west},   {10, 5, D::south},
                    {10, 6, D::south},  {10, 7, D::south}, {10, 9, D::south},  {10, 10, D::south},
                    {10, 11, D::south}, {10, 12, D::east}, {11, 12, D::east},  {12, 12, D::east},
                    {13, 12, D::east},  {14, 12, D::east}, {15, 12, D::north}, {15, 11, D::north},
                    {15, 10, D::north}, {15, 9, D::north}});
        reload_at(10, 1, 1);
        reload_at(10, 12, 1);
        route.push_back(step_beat(53, 15, 8, D::north, {"YES"}));
        walk_heart({{15, 7, D::north}, {15, 6, D::north}});
        route.push_back(step_beat(53, 15, 5, D::north, {}));
        walk_heart({{15, 4, D::south},  {15, 5, D::south},  {15, 6, D::south},  {15, 7, D::south},
                    {15, 8, D::south},  {15, 9, D::south},  {15, 10, D::south}, {15, 11, D::south},
                    {15, 12, D::south}, {15, 13, D::south}, {15, 14, D::south}, {15, 15, D::west},
                    {14, 15, D::west},  {13, 15, D::west},  {12, 15, D::north}, {12, 14, D::north},
                    {12, 13, D::north}, {12, 12, D::west},  {11, 12, D::west},  {10, 12, D::west},
                    {9, 12, D::south},  {9, 13, D::south},  {9, 14, D::south},  {9, 15, D::west},
                    {8, 15, D::west},   {7, 15, D::west},   {6, 15, D::west},   {5, 15, D::west},
                    {4, 15, D::west}});
        route.push_back(step_beat(53, 3, 15, D::west, {"YES"}));
        // Tanetal's last stand in the right ventricle (5,12, event 8; the
        // cluebook's 10): he slays the Watcher and his minions rise.
        // Tanetal, his golem, cultists and bits of Moander: the cultists raise
        // Blade Barriers, which neither side's computer steps into
        // (combatai.pas, AiCanStep), so the bits of Moander, whose poison
        // slays, are not cut down by them. The magic-user hastes the party as
        // it steps up (23 of 120 plays win, members dead, 2026-11-16; none
        // of 360 unhasted), and the party raises its dead after. The party
        // takes the Talisman of Bane (cell 245h 1).
        walk_heart({{7, 10, D::west}, {6, 10, D::south}, {6, 11, D::south}});
        auto showdown = step_beat(53, 6, 12, D::west, {"YES"});
        showdown.cure_first = true;
        showdown.cure_to_full = true;
        showdown.camp_broken = true;
        showdown.ward_each_with = "haste";
        showdown.ward_members = {"paladin 1"};
        showdown.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x245, 1};
        route.push_back(showdown);
        // Out of the heart by the vena cava's end (2,15, south: ARE YOU SURE
        // YOU WANT TO LEAVE THE HEART? YES), onto Moander's body by the wound
        // in its side (20,3).
        walk_heart({{5, 12, D::north},
                    {5, 11, D::north},
                    {5, 10, D::west},
                    {4, 10, D::west},
                    {3, 10, D::west},
                    {2, 10, D::south},
                    {2, 11, D::south},
                    {2, 12, D::south},
                    {2, 13, D::south},
                    {2, 14, D::south}});
        route.push_back(step_beat(53, 2, 15, D::south, {"YES"}));
        // Over the body to the great wound in its arm (15,13: DO YOU ENTER?
        // YES), where Aeghwaet heals the party (YES; his temple raises any
        // dead).
        RouteBeat back_to_wound{53, 0, 0, D::north, {"YES", "YES", "raise:dead", "Leave"}};
        back_to_wound.area_steps = {C::southeast, C::southeast, C::south,     C::southwest,
                                    C::southwest, C::southwest, C::southwest, C::southwest,
                                    C::southwest, C::southwest};
        back_to_wound.fleeing = true;
        route.push_back(back_to_wound);
        // Out of the wound (YES, then NO to entering again) and over the body
        // north-east and north-west to the depression (12,4), where the party
        // climbs down (DO YOU ENTER? YES; map 52).
        route.push_back(step_beat(52, 12, 15, D::south, {"YES", "NO"}));
        RouteBeat to_the_depression{52, 0, 0, D::north, {"YES"}};
        to_the_depression.area_steps = {C::northeast, C::northeast, C::northeast,
                                        C::northwest, C::northwest, C::northwest,
                                        C::northwest, C::northwest, C::northwest};
        to_the_depression.fleeing = true;
        route.push_back(to_the_depression);
        // In the depression the gate (7,10, event 10, the wall south of it
        // ahead; the cluebook's Depression 1): the party steps through (YES)
        // to Limbo, where Elminster greets the Talisman of Bane, his camp rests
        // the party and it trains. REALMS takes it down his rope to the area
        // map below the Red Tower's pool (5,12), its stored goods left (NO).
        route.push_back(step_beat(52, 7, 9, D::south, {"YES", "stop:REST"}));
        RouteBeat talisman_rest{18, 0, 0, D::north, {}};
        talisman_rest.rest_choice = "REST";
        route.push_back(talisman_rest);
        RouteBeat down_rope{18, 0, 0, D::north,
                            train_darkness_party(2, {"TRAIN"}, {"MOVE ON", "REALMS", "YES", "NO"})};
        down_rope.waiting = true;
        route.push_back(down_rope);
        // East across the area map to the black city of Mulmaster (31,11),
        // fleeing what roams it, and in (YES; script 26, map 19's north half).
        // The sites on the way (a wayhouse, villages) are passed by (NO).
        RouteBeat to_mulmaster{18, 0, 0, D::north, {}};
        to_mulmaster.area_steps.assign(25, C::east);
        to_mulmaster.fleeing = true;
        to_mulmaster.passing = true;
        route.push_back(to_mulmaster);
        RouteBeat into_mulmaster{18, 0, 0, D::north, {"YES", "MOVE ON"}};
        into_mulmaster.area_steps = {C::northeast};
        into_mulmaster.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x16, 26};
        route.push_back(into_mulmaster);
        // MOVE ON past the gate's guards (0,3; the cluebook's 1), and through
        // the alleys, where Banites chant the labyrinth's way (journal entry
        // 24), to the arena's grating (5,6).
        walk_passages(route, 19,
                      {{0, 3, D::east},
                       {1, 3, D::east},
                       {2, 3, D::east},
                       {3, 3, D::south},
                       {3, 4, D::south},
                       {3, 5, D::south},
                       {3, 6, D::east},
                       {4, 6, D::east}});
        // Into the arena (6,6, event 21; the cluebook's 4): the party has
        // offended no one, so it fights a round or two (cell A6h) of the
        // pens' monsters, and a beholder offers it Arcam's interview (YES):
        // it is led down into the Cave of the Beholder (script 36).
        route.push_back(step_beat(19, 5, 6, D::east, {"YES"}));
        // The Cave of the Beholder (map 36, script 36): south from the
        // ladder past the mustering troops (5,4) to the spectacle (5,5,
        // journal entry 39), where the Chancellor offers a task against
        // Arcam's rivals (script 42) and the party refuses it at every price
        // (NO four times): his mercenaries attack and fall. Rested, it beats
        // the Chancellor covering Arcam's retreat (5,8).
        walk_passages(route, 36,
                      {{0, 0, D::east},
                       {1, 0, D::east},
                       {2, 0, D::east},
                       {3, 0, D::east},
                       {4, 0, D::south},
                       {4, 1, D::south},
                       {4, 2, D::east},
                       {5, 2, D::south},
                       {5, 3, D::south},
                       {5, 4, D::south}});
        route.push_back(step_beat(36, 5, 5, D::south, {"NO", "NO", "NO", "NO"}));
        {
            auto chancellor = step_beat(36, 5, 7, D::south);
            chancellor.rest_first = true;
            route.push_back(chancellor);
        }
        const auto cave_way = [&route](unsigned x, unsigned y, D facing)
        {
            route.push_back(step_beat(36, x, y, facing, {"YES"}));
        };
        // South, an escaped prisoner (5,9) leads the party out by the tunnel
        // to the surface (0,3, GO UP? YES), and the tunnel on the area map
        // brings it back in at 14,5: the planning aid's way round the
        // labyrinth, west and south through Arcam's empty chamber, by the
        // ramps (12,8; 1,7) and the passage (15,8), to the Sentinel Halls
        // (8,11, script 38; the cluebook's 26).
        cave_way(5, 8, D::south);
        cave_way(5, 9, D::south);
        cave_way(0, 3, D::west);
        walk_passages(route, 36,
                      {{14, 5, D::north},
                       {14, 4, D::west},
                       {13, 4, D::west},
                       {12, 4, D::west},
                       {11, 4, D::south},
                       {11, 5, D::east},
                       {12, 5, D::south},
                       {12, 6, D::south},
                       {12, 7, D::south}});
        cave_way(12, 8, D::north);
        walk_passages(route, 36, {{14, 9, D::north}, {14, 8, D::east}});
        cave_way(15, 8, D::east);
        walk_passages(route, 36, {{0, 8, D::east}});
        walk_passages(route, 36, {{1, 8, D::north}});
        cave_way(1, 7, D::east);
        walk_passages(route, 36, {{6, 9, D::east}, {7, 9, D::south}, {7, 10, D::south}});
        cave_way(7, 11, D::east);
        // The Sentinel Halls rest once Bane's minions have chanted past
        // three times (script 38's camp entry, cell A4h), so the party camps
        // again where they only pass by. East along row 11 the dragon
        // sentinel hears the Horn of Doom and the spider sentinel sees the
        // Crystal Ring (their dragons and spiders beaten), and at 2,11 the
        // fist senses the Talisman of Bane and opens (its minions beaten);
        // stepped east again it lets the party through, and at 4,12 the Pool
        // of Darkness (YES) leads to Limbo.
        {
            auto rest_sentinels = step_beat(36, 8, 11, D::east);
            rest_sentinels.passing = true;
            rest_sentinels.rest_first = true;
            rest_sentinels.rest_for_spells = true;
            rest_sentinels.rest_through_period = true;
            route.push_back(rest_sentinels);
        }
        walk_passages(route, 36,
                      {{9, 11, D::east},
                       {10, 11, D::east},
                       {11, 11, D::east},
                       {12, 11, D::east},
                       {13, 11, D::east},
                       {14, 11, D::east},
                       {15, 11, D::east},
                       {0, 11, D::east},
                       {1, 11, D::east}});
        cave_way(2, 11, D::east);
        cave_way(2, 11, D::east);
        walk_passages(route, 36, {{3, 11, D::south}, {3, 12, D::east}});
        route.push_back(step_beat(36, 4, 12, D::east, {"YES", "stop:REST"}));
        // Limbo: Elminster finds the party ready for the Dark Dimension. His
        // camp rests it, it trains, and it goes on to Bane's land (MOVE ON,
        // BANE'S LAND, YES; script 64, Dark Phlan).
        RouteBeat dark_rest{18, 0, 0, D::north, {}};
        dark_rest.rest_choice = "REST";
        route.push_back(dark_rest);
        RouteBeat to_dark_phlan{
            18, 0, 0, D::north,
            train_darkness_party(2, {"TRAIN"}, {"MOVE ON", "BANE'S LAND", "YES"})};
        to_dark_phlan.waiting = true;
        route.push_back(to_dark_phlan);
        // Dark Phlan (map 64, script 64): the drow who carry Sasha off are
        // beaten where the party arrives (13,0; the cluebook's 1). The crowd
        // carries it to Cadorna's announcement (11,1, LISTEN; the cluebook's
        // 16), and it goes south to the inn (12,11, YES; the cluebook's 2),
        // whose rooms are the only ones its camps rest in (script 64's camp
        // entry: events 2 and 3), and rests (13,11).
        walk_passages(route, 64,
                      {{13, 0, D::south},
                       {13, 1, D::west},
                       {12, 1, D::west},
                       {11, 1, D::south},
                       {12, 5, D::south},
                       {12, 6, D::south},
                       {12, 7, D::south},
                       {12, 8, D::south},
                       {12, 9, D::south},
                       {12, 10, D::west},
                       {11, 10, D::south}});
        route.push_back(step_beat(64, 11, 11, D::east, {"YES"}));
        {
            auto inn = step_beat(64, 12, 11, D::east);
            inn.passing = true;
            route.push_back(inn);
            auto inn_rest = step_beat(64, 13, 11, D::west);
            inn_rest.passing = true;
            inn_rest.rest_first = true;
            inn_rest.rest_for_spells = true;
            // Dracoliches and Bane minions breathe lightning. Save the
            // protection scroll for the last battle and enlarge the five
            // melee fighters before the finale.
            inn_rest.ward_memorized = "enlarge";
            inn_rest.ward_members = {"paladin 1", "paladin 2", "ranger 3", "cleric 4", "cleric 5"};
            inn_rest.memorize_first_of = {
                "mind_blank",   "mind_blank", "mind_blank", "globe_of_invulnerability",
                "haste",        "haste",      "haste",      "dispel_magic",
                "mirror_image", "bless",      "prayer"};
            inn_rest.keep_preparation = true;
            route.push_back(inn_rest);
        }
        // West through the city to its west gate (the cluebook's 12), where
        // Gothmenes sends a welcoming party of minions (1,5; the cluebook's
        // 18), and out to the Iron Wall (column 14, met facing west).
        walk_passages(route, 64,
                      {{12, 11, D::west}, {11, 11, D::west}, {10, 11, D::south}, {10, 12, D::west},
                       {9, 12, D::west},  {8, 12, D::west},  {7, 12, D::north},  {7, 11, D::north},
                       {7, 10, D::west},  {6, 10, D::west},  {5, 10, D::north},  {5, 9, D::north},
                       {5, 8, D::north},  {5, 7, D::north},  {5, 6, D::north},   {5, 5, D::west},
                       {4, 5, D::west},   {3, 5, D::west},   {2, 5, D::west},    {1, 5, D::west},
                       {0, 5, D::west}});
        // The Horn of Doom (USE HORN OF DOOM): Gothmenes swoops down and his
        // minions fall on the party; beaten, it blows the horn once for each
        // member (6; the cluebook's 12), the wall shatters, and through it
        // (YES) lies the Palace of Gothmenes (script 65, the Crystal Ring
        // held). Fourteen Minions of Bane, each casting Ice Storm and Hold
        // Monster and breathing for 22, fall to the party in 63 of 120 plays
        // (2026-11-16).
        {
            auto iron_wall =
                step_beat(64, 15, 5, D::west, {"USE HORN OF DOOM", "YES", "type:6", "YES"});
            route.push_back(iron_wall);
        }
        // The outer palace (map 65, script 65): its secret doors (walls of
        // type 5) open only to a party searching (cell 19h at 1; the
        // cluebook's 2, "use LOOK"), and a step taken searching runs the
        // objects of power instead of the square's event: the party names
        // who bears each object (WHO) and walks the palace's winding way in
        // search mode, EXIT leaving the objects as they are.
        const auto walk_palace =
            [&route](std::initializer_list<std::tuple<unsigned, unsigned, D>> steps)
        {
            walk_passages(route, 65, steps, true, false, false, {"EXIT"});
        };
        const auto palace_entry = route.size();
        walk_palace({{0, 0, D::east},  {1, 0, D::south}, {1, 1, D::east},  {2, 1, D::east},
                     {3, 1, D::north}, {3, 0, D::east},  {4, 0, D::east},  {5, 0, D::east},
                     {6, 0, D::east},  {7, 0, D::south}, {7, 1, D::south}, {7, 2, D::south},
                     {7, 3, D::west},  {6, 3, D::west},  {5, 3, D::north}, {5, 2, D::east},
                     {6, 2, D::north}, {6, 1, D::west},  {5, 1, D::west},  {4, 1, D::south},
                     {4, 2, D::south}});
        // The mage carries the crystal; the paladin keeps the protection
        // scroll packed until the last battle.
        route[palace_entry].answers = {"paladin 1", "magic_user 6", "paladin 2", "EXIT"};
        // "Sasha" and the Phlanerians (4,4, event 0; the cluebook's 4), met
        // with searching off, the clerics curing first (the palace's
        // wandering monsters break a camp): the party will not tell where
        // Elminster is (NO, NO), which gives Gothmenes' search its longest
        // time (cell 1CDh 77, the Tantalus Clock's strikes), and the
        // rakshasas are revealed. The bolts' bearers ready their crossbows
        // and quarrels (item types 27 and 12). Two Maharajahs and seven
        // Rajahs, whom no spell below the eighth level touches, cast an Ice
        // Storm each a round and beat the party in most plays (9 of 120 win,
        // 2026-11-16, with the magic-user's eighth- and ninth-level spells),
        // so the player reloads until one wins.
        {
            auto sasha = step_beat(65, 4, 3, D::south, {"NO", "NO"});
            sasha.searching = false;
            sasha.cure_first = true;
            sasha.cure_to_full = true;
            sasha.camp_broken = true;
            sasha.reloads_until_won = 240;
            for (const char *bearer : {"paladin 1", "paladin 2", "ranger 3"})
            {
                sasha.ready_types.emplace_back(27, bearer);
                sasha.ready_types.emplace_back(12, bearer);
            }
            route.push_back(sasha);
        }
        // On in search mode to the inner palace's entrance (8,8, event 11;
        // the cluebook's 9), stepped onto with searching off: Gothmenes
        // winds the Tantalus Clock and leaves (script 74).
        walk_palace({{4, 4, D::east},
                     {5, 4, D::east},
                     {6, 4, D::south},
                     {6, 5, D::east},
                     {7, 5, D::north},
                     {7, 4, D::east},
                     {8, 4, D::east},
                     {9, 4, D::east},
                     {10, 4, D::south},
                     {10, 5, D::south},
                     {10, 6, D::west},
                     {9, 6, D::west},
                     {8, 6, D::south}});
        {
            auto inner_palace = step_beat(65, 8, 7, D::south);
            // The outer palace permits rest (with wandering monsters).
            // Refill the magic-user's high-level spells before entering
            // the inner palace, where resting is no longer possible.
            inner_palace.rest_first = true;
            inner_palace.rest_for_spells = true;
            inner_palace.rest_through_period = true;
            inner_palace.searching = false;
            route.push_back(inner_palace);
        }
        // The inner palace (script 73): the Tantalus Clock strikes every
        // second step (cell 1CEh), no wandering monster comes while it ticks
        // (the cluebook), and Bane's Crystal shows in the Pool of Death only
        // once it has struck more than 50 times. The party paces the
        // entrance hall (8,8 to 8,7) until then, as a player waits, and
        // Gothmenes returns at the 77th strike.
        for (const std::uint16_t strikes : {18, 36, 52})
        {
            auto pace = step_beat(65, 8, 8, D::north);
            pace.repeat_in_place = true;
            pace.camp_broken = true;
            pace.repeat_until_at_least = std::pair<std::uint16_t, std::uint16_t>{0x1CE, strikes};
            route.push_back(pace);
        }
        // Round to the pool's north edge (9,8; the cluebook's 16), where a
        // step south into it (script 73's entry before a step, the wall of
        // type 1 ahead) asks who tries: the Crystal Ring's bearer (YES)
        // puts the ring on rather than reach in with it in a pocket (NO,
        // YES) and takes Bane's Crystal (script 75).
        const auto last_approach = route.size();
        walk_passages(route, 65, {{8, 8, D::north}, {8, 7, D::east}, {9, 7, D::south}});
        route[last_approach].cast_after = {"bless"};
        route[last_approach + 1].cast_after = {"prayer"};
        route[last_approach + 1].cast_after_by = "cleric 4";
        route.back().ward_each_with = "mind_blank";
        route.back().ward_members = {"cleric 4", "cleric 5", "magic_user 6"};
        route.back().cast_after = {"haste"};
        auto crystal = step_beat(65, 9, 8, D::south, {"YES", "magic_user 6", "NO", "YES"});
        crystal.ward_each_with = "globe_of_invulnerability";
        crystal.ward_members = {"magic_user 6"};
        crystal.cast_after = {"mirror_image"};
        crystal.cast_after_by = "magic_user 6";
        route.push_back(crystal);
        // Six strikes later Gothmenes returns with Elminster in his whip and
        // summons the party to the pool (script 65); it will not trade the
        // crystal for the wizard (NO, NO), Elminster breaks free to duel the
        // Balor, and Gothmenes' servants rise from the pool (script 75). The
        // crystal's bearer countermands the summons (YES; the cluebook's 17:
        // it drives back many of the servants and leaves its bearer near
        // death); the dracoliches, bits of Moander, Bane minions and Pets of
        // Kalistes, then Xoham and his beholders, are beaten, Elminster is
        // swept into the pool, and the party will not bargain for the
        // crystal's word (AGREE? NO): Gothmenes, with the few minions who
        // still answer him, fights to the death, and the crystal crumbles.
        // The player wins all three battles. It saves Haste copies for
        // Xoham and reads the collected protection scroll before the last
        // battle bars magic; that battle must put Gothmenes down, rather
        // than accepting a quiet-fight victory with him still standing.
        {
            auto finale = step_beat(65, 9, 7, D::south, {"NO", "NO", "YES", "NO"});
            finale.repeat_in_place = true;
            finale.camp_broken = true;
            // The clerics cure what the pool's waters took first, the inner
            // palace's camps resting nowhere.
            finale.cure_first = true;
            finale.cure_to_full = true;
            finale.ward_each_with = "enlarge";
            finale.ward_members = {"paladin 1", "paladin 2", "ranger 3", "cleric 4", "cleric 5"};
            // The blessed bolts serve the rakshasas. Restore the fighters'
            // swords and shields for the servants and Gothmenes.
            for (const char *member : {"paladin 1", "paladin 2", "ranger 3"})
            {
                finale.ready_types.emplace_back(18, member);
                finale.ready_types.emplace_back(37, member);
            }
            for (const char *member : {"cleric 4", "cleric 5"})
            {
                finale.ready_types.emplace_back(8, member);
                finale.ready_types.emplace_back(37, member);
            }
            finale.reload_event = true;
            finale.repeat_until_at_least = std::pair<std::uint16_t, std::uint16_t>{0x16, 75};
            route.push_back(finale);
        }
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
constexpr bool gateway_route_ends_the_game = true;
constexpr unsigned gateway_route_stops_on_map = 20;

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
                      {{0, 9, D::south},
                       {0, 10, D::east},
                       {1, 10, D::east},
                       {2, 10, D::south},
                       {2, 11, D::east},
                       {3, 11, D::east},
                       {4, 11, D::east},
                       {5, 11, D::east},
                       {6, 11, D::east},
                       {7, 11, D::east},
                       {8, 11, D::north},
                       {8, 10, D::east},
                       {9, 10, D::south}},
                      true);
        // The captain's lodge south of the street (event 118, ECL4:8
        // 0x92CF), whose pirates carry their gift, a Wand of Ice Storm: the
        // cluebook's answer to the Star Mounts' shambling mounds.
        {
            auto lodge = step_beat(8, 9, 11, D::south);
            lodge.rest_first = true;
            lodge.rest_for_spells = true;
            lodge.take_wand = true;
            lodge.pack_wand = true;
            route.push_back(lodge);
        }
        walk_passages(route, 8,
                      {{9, 12, D::north},
                       {9, 11, D::north},
                       {9, 10, D::west},
                       {8, 10, D::north},
                       {8, 9, D::north},
                       {8, 8, D::north},
                       {8, 7, D::north},
                       {8, 6, D::north},
                       {8, 5, D::west},
                       {7, 5, D::west},
                       {6, 5, D::west},
                       {5, 5, D::west},
                       {4, 5, D::west},
                       {3, 5, D::north},
                       {3, 4, D::north},
                       {3, 3, D::north},
                       {3, 2, D::west},
                       {2, 2, D::west},
                       {1, 2, D::west}},
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
        // The Kraken headquarters: south from the beach through the southern
        // fortress, out of search mode (a step searching takes ten minutes),
        // its guards held from fleeing, to the room where the Pearl Chest
        // passes 30 minutes after the landing (the cluebook's encounter 16,
        // event 125).
        const auto fortress_start = route.size();
        walk_passages(route, 13, {{12, 7, D::south},  {12, 8, D::east},   {13, 8, D::south},
                                  {13, 9, D::west},   {12, 9, D::south},  {12, 10, D::west},
                                  {11, 10, D::south}, {11, 11, D::west},  {10, 11, D::south},
                                  {10, 12, D::west},  {9, 12, D::north},  {9, 11, D::north},
                                  {9, 10, D::east},   {10, 10, D::north}, {10, 9, D::east},
                                  {11, 9, D::north},  {11, 8, D::west},   {10, 8, D::west},
                                  {9, 8, D::south},   {9, 9, D::west},    {8, 9, D::north},
                                  {8, 8, D::west},    {7, 8, D::west},    {6, 8, D::west},
                                  {5, 8, D::south},   {5, 9, D::west},    {4, 9, D::west}});
        for (auto beat = route.begin() + static_cast<std::ptrdiff_t>(fortress_start);
             beat != route.end(); ++beat)
        {
            beat->holding_foes = true;
        }
        // There the party looks about (ten minutes a Look), letting time pass
        // as the cluebook bids, until the chest's guards come; beaten, they
        // leave the statuette of the West (bit 2 of cell 4C27).
        {
            RouteBeat chest{13, 3, 9, D::west, {}};
            chest.repeat_until_at_least = {{0x4C27, 3}};
            chest.repeat_in_place = true;
            chest.holding_foes = true;
            route.push_back(chest);
        }
        // Back through the fortress to the beach, where the hidden boat takes
        // the party, rested, on the current to Neverwinter and its ambush.
        walk_passages(route, 13,
                      {{3, 9, D::east},    {4, 9, D::north},  {4, 8, D::east},    {5, 8, D::east},
                       {6, 8, D::east},    {7, 8, D::east},   {8, 8, D::south},   {8, 9, D::east},
                       {9, 9, D::north},   {9, 8, D::east},   {10, 8, D::east},   {11, 8, D::south},
                       {11, 9, D::west},   {10, 9, D::south}, {10, 10, D::west},  {9, 10, D::south},
                       {9, 11, D::south},  {9, 12, D::east},  {10, 12, D::north}, {10, 11, D::east},
                       {11, 11, D::north}, {11, 10, D::east}, {12, 10, D::north}, {12, 9, D::east},
                       {13, 9, D::north},  {13, 8, D::west}},
                      true);
        {
            auto boat = step_beat(13, 12, 8, D::north, {"YES"});
            boat.rest_first = true;
            route.push_back(boat);
        }
        // Neverwinter: up from the landing, rested, to the Zhentarim and their
        // manticores who wait for the statuette (cell 4C2A set).
        walk_passages(route, 6,
                      {{2, 5, D::north}, {2, 4, D::north}, {2, 3, D::west}, {1, 3, D::north}},
                      true);
        {
            auto ambush = step_beat(6, 1, 2, D::north);
            ambush.rest_first = true;
            ambush.expect_cell = {{0x4C2A, 1}};
            route.push_back(ambush);
        }
        // Out of Neverwinter's south gate and overland east and south, past
        // Yartar, to Secomber, where Amanitas lives.
        walk_passages(route, 6,
                      {{1, 1, D::east},
                       {2, 1, D::east},
                       {3, 1, D::east},
                       {4, 1, D::east},
                       {5, 1, D::east},
                       {6, 1, D::north},
                       {6, 14, D::east},
                       {7, 14, D::south},
                       {7, 15, D::south}},
                      true);
        walk_overland(route, 21,
                      {{4, 11, D::north},
                       {4, 9, D::east},
                       {5, 7, D::east},
                       {6, 5, D::east},
                       {7, 4, D::east},
                       {8, 3, D::east},
                       {9, 1, D::east},
                       {10, 0, D::east},
                       {11, 14, D::east},
                       {12, 12, D::east},
                       {13, 10, D::east},
                       {14, 8, D::east},
                       {15, 6, D::east}});
        walk_overland(route, 22,
                      {{0, 6, D::east},
                       {1, 4, D::east},
                       {2, 2, D::east},
                       {3, 0, D::east},
                       {4, 14, D::east},
                       {5, 13, D::east},
                       {6, 11, D::east},
                       {7, 9, D::east},
                       {8, 7, D::east},
                       {9, 6, D::east},
                       {10, 4, D::east},
                       {11, 2, D::east},
                       {12, 1, D::east},
                       {13, 0, D::east},
                       {14, 15, D::south}});
        walk_overland(route, 25, {{10, 0, D::east}});
        route.push_back(step_beat(25, 11, 14, D::east, {"YES"}));
        // Out of Secomber and overland east up the Delimbiyr Vale to Llorkh,
        // the long way round the Great Desert, as Amanitas bids.
        walk_passages(route, 14, {{0, 13, D::west}}, true);
        walk_overland(route, 25,
                      {{11, 14, D::north},
                       {11, 11, D::east},
                       {12, 10, D::north},
                       {12, 7, D::east},
                       {13, 5, D::east},
                       {14, 3, D::east},
                       {15, 1, D::east}});
        walk_overland(route, 26,
                      {{0, 1, D::east},
                       {1, 0, D::east},
                       {2, 15, D::east},
                       {3, 13, D::east},
                       {4, 12, D::east},
                       {5, 11, D::east},
                       {6, 10, D::east},
                       {7, 9, D::east},
                       {8, 8, D::east},
                       {9, 6, D::east},
                       {10, 5, D::east},
                       {11, 3, D::east},
                       {12, 1, D::north},
                       {12, 14, D::east},
                       {13, 13, D::east}});
        {
            // Rested before Llorkh, whose shops trap the party into the arena.
            auto llorkh = step_beat(26, 14, 12, D::north, {"YES"});
            llorkh.rest_first = true;
            llorkh.waiting = false;
            route.push_back(llorkh);
        }
        // Llorkh: into the first tavern, whose floor drops the party down the
        // ramp to the arena beneath the town.
        walk_passages(route, 16, {{0, 11, D::north}, {0, 10, D::east}, {1, 10, D::north}}, true);
        // The arena: out of the tunnel to its floor, where the statuettes are
        // taken and three fights follow with no rest between them.
        walk_passages(route, 17,
                      {{7, 11, D::north},
                       {7, 10, D::north},
                       {7, 9, D::north},
                       {7, 8, D::north},
                       {7, 7, D::north},
                       {7, 6, D::north}},
                      true);
        // Thrown into a cell, the party rests, then waits (an hour, a Look
        // ten minutes) for the jailer, whose keys Krevish, the deftest hand,
        // snatches (cell 4C06 set to 2).
        {
            RouteBeat cell{17, 12, 4, D::south, {"Krevish"}};
            cell.rest_first = true;
            cell.repeat_until_at_least = {{0x4C06, 2}};
            cell.repeat_in_place = true;
            route.push_back(cell);
        }
        // Out of the cells and through the barracks to the Zhentil commander's
        // room, where the statuette of the North and the others taken wait, and up
        // to Llorkh.
        walk_passages(route, 17,
                      {{11, 3, D::west},
                       {10, 3, D::west},
                       {9, 3, D::north},
                       {9, 2, D::west},
                       {8, 2, D::north},
                       {8, 1, D::west},
                       {7, 1, D::north},
                       {7, 0, D::west},
                       {6, 0, D::west},
                       {5, 0, D::west},
                       {4, 0, D::south},
                       {4, 1, D::west},
                       {3, 1, D::north},
                       {3, 0, D::west},
                       {2, 0, D::south},
                       {2, 1, D::south},
                       {2, 2, D::west}},
                      true);
        // The statuette of the North (bit 3 of cell 4C27) joins the other two.
        route.back().expect_cell = {{0x4C27, 7}};
        // Out of Llorkh and overland west to the foot of the Star Mounts, where
        // the aarakocra fly the party up to Ceptienne's aerie.
        {
            // Rested in Llorkh, whose inns and houses are safe again.
            auto rested = step_beat(16, 4, 6, D::south);
            rested.passing = true;
            rested.searching = true;
            rested.rest_first = true;
            route.push_back(rested);
        }
        walk_passages(route, 16,
                      {{4, 7, D::east},
                       {5, 7, D::east},
                       {6, 7, D::east},
                       {7, 7, D::south},
                       {7, 8, D::south},
                       {7, 9, D::south},
                       {7, 10, D::west},
                       {6, 10, D::west},
                       {5, 10, D::west},
                       {4, 10, D::west},
                       {3, 10, D::west},
                       {2, 10, D::west},
                       {1, 10, D::west},
                       {0, 10, D::west}},
                      true);
        walk_overland(route, 26,
                      {{13, 11, D::north},
                       {13, 9, D::west},
                       {12, 8, D::west},
                       {11, 6, D::north},
                       {11, 3, D::west},
                       {10, 2, D::west},
                       {9, 1, D::west},
                       {8, 0, D::west},
                       {7, 15, D::north},
                       {7, 12, D::west},
                       {6, 10, D::west},
                       {5, 9, D::west},
                       {4, 7, D::west},
                       {3, 5, D::west}});
        {
            // The party helps the aarakocra beset by giant snakes and
            // shambling mounds. The mounds (armor class -4, engulfing on 1
            // to 4 of a d10 at each hit) are struck only on 18 or more by
            // the party's swords +2 and smother it, so the party fights as
            // the cluebook bids: it lets the enemy come to it while the
            // magic-user readies the Wand of Ice Storm from Luskan's lodge,
            // which still harms the mounds by half, and burns the snakes
            // with Fireball (mounds_turn).
            constexpr int wand_type = 79;
            auto bird = step_beat(26, 2, 3, D::north, {"HELP BIRD", "YES"});
            bird.ready_types.emplace_back(wand_type, "magic_user 6");
            route.push_back(bird);
        }
        // Ceptienne's aerie: through the maze of one-way doors and the secret
        // door to her chamber, where she falls with the statuette of the South
        // (bit 4 of cell 4C27).
        walk_passages(route, 18,
                      {{15, 2, D::west},  {14, 2, D::west},  {13, 2, D::west},  {12, 2, D::north},
                       {12, 1, D::west},  {11, 1, D::north}, {11, 0, D::west},  {10, 0, D::west},
                       {9, 0, D::west},   {8, 0, D::west},   {7, 0, D::west},   {6, 0, D::south},
                       {6, 1, D::west},   {5, 1, D::south},  {5, 2, D::west},   {4, 2, D::south},
                       {4, 3, D::east},   {5, 3, D::east},   {6, 3, D::east},   {7, 3, D::east},
                       {8, 3, D::east},   {9, 3, D::south},  {9, 4, D::south},  {9, 5, D::south},
                       {9, 6, D::east},   {10, 6, D::north}, {10, 5, D::north}, {10, 4, D::east},
                       {11, 4, D::east},  {12, 4, D::south}, {12, 5, D::south}, {12, 6, D::south},
                       {12, 7, D::west},  {11, 7, D::west},  {10, 7, D::west},  {9, 7, D::west},
                       {8, 7, D::west},   {7, 7, D::south},  {7, 8, D::south},  {7, 9, D::south},
                       {7, 10, D::south}, {7, 11, D::west},  {6, 11, D::north}, {6, 10, D::north},
                       {6, 9, D::north},  {6, 8, D::west},   {5, 8, D::south},  {5, 9, D::south},
                       {5, 10, D::west},  {4, 10, D::north}, {4, 9, D::north},  {4, 8, D::west}},
                      true);
        route.back().expect_cell = {{0x4C27, 15}};
        // Rested in her chamber, back to the alcove where the aarakocra wait to
        // fly the party down, and overland north-east to Sundabar.
        {
            auto rested = step_beat(18, 3, 8, D::west);
            rested.passing = true;
            rested.searching = true;
            rested.rest_first = true;
            route.push_back(rested);
        }
        walk_passages(route, 18,
                      {{2, 8, D::north}, {2, 7, D::north},  {2, 6, D::north}, {2, 5, D::east},
                       {3, 5, D::north}, {3, 4, D::north},  {3, 3, D::east},  {4, 3, D::north},
                       {4, 2, D::east},  {5, 2, D::north},  {5, 1, D::east},  {6, 1, D::north},
                       {6, 0, D::east},  {7, 0, D::east},   {8, 0, D::east},  {9, 0, D::east},
                       {10, 0, D::east}, {11, 0, D::south}, {11, 1, D::east}, {12, 1, D::south},
                       {12, 2, D::east}, {13, 2, D::east}},
                      true);
        route.push_back(step_beat(18, 14, 2, D::east, {"YES"}));
        walk_overland(route, 26, {{2, 3, D::east}, {3, 2, D::east}, {4, 0, D::north}});
        walk_overland(route, 23,
                      {{8, 15, D::east},
                       {9, 14, D::east},
                       {10, 12, D::east},
                       {11, 10, D::east},
                       {12, 8, D::east},
                       {13, 6, D::east}});
        route.push_back(step_beat(23, 14, 4, D::east, {"YES"}));
        // Sundabar's Temple of Tyr (2,2) raises any the aarakocra's fight
        // cost: its shambling mounds engulf and smother their victims.
        route.push_back({19, 2, 2, D::north, {"raise:dead", "Leave"}});
        // Out of Sundabar by its west gate, and overland round the mountains
        // east to Ascore.
        walk_passages(route, 19, {{0, 10, D::west}}, true);
        walk_overland(route, 23,
                      {{14, 4, D::north}, {14, 1, D::north}, {14, 14, D::east}, {15, 12, D::east}});
        walk_overland(route, 24,
                      {{0, 12, D::east},
                       {1, 10, D::east},
                       {2, 8, D::east},
                       {3, 6, D::east},
                       {4, 4, D::east},
                       {5, 3, D::north},
                       {5, 0, D::east},
                       {6, 14, D::east},
                       {7, 12, D::east},
                       {8, 10, D::east},
                       {9, 8, D::east},
                       {10, 6, D::north},
                       {10, 4, D::east},
                       {11, 3, D::east}});
        // The way down into Ascore is taken by day: at night the cliff's edge is
        // too dark to explore (script 21), and a player camps until morning,
        // the test passing the time. The fights before it run long or short
        // as the original's monsters ready their weapons and the party's
        // conditions run down on the road.
        {
            auto descent = step_beat(24, 12, 1, D::east, {"YES"});
            descent.wait_until_minute = 8 * 60;
            route.push_back(descent);
        }
        // Ascore: east through the ruins to Vaalgamon's gate, which the four
        // statuettes open past its giant spiders. The party rests on the
        // gate's square, where Ascore's camp script lets it sleep (its events
        // from 107 on, ECL6:20 0x81B2); the rooms east of the gate break every
        // camp with undead, as the original's do. Then on to the passage where
        // Vaalgamon waits.
        walk_passages(route, 20,
                      {{0, 7, D::east},
                       {1, 7, D::east},
                       {2, 7, D::north},
                       {2, 6, D::east},
                       {3, 6, D::south},
                       {3, 7, D::east},
                       {4, 7, D::north},
                       {4, 6, D::east},
                       {5, 6, D::east},
                       {6, 6, D::east},
                       {7, 6, D::south},
                       {7, 7, D::east},
                       {8, 7, D::east}},
                      true);
        {
            auto rested = step_beat(20, 9, 7, D::east);
            rested.passing = true;
            rested.searching = true;
            rested.rest_first = true;
            route.push_back(rested);
        }
        walk_passages(route, 20,
                      {{10, 7, D::east},
                       {11, 7, D::south},
                       {11, 8, D::south},
                       {11, 9, D::south},
                       {11, 10, D::south},
                       {11, 11, D::west},
                       {10, 11, D::south},
                       {10, 12, D::south},
                       {10, 13, D::south},
                       {10, 14, D::south},
                       {10, 15, D::east}},
                      true);
        {
            // The party takes Vaalgamon's challenge, through the maze of
            // fights, each left as soon as a member runs off its north
            // side (or won outright, which leaves the party facing north as it
            // came, so the same way), the fourth past
            // Vaalgamon's own room, out to the plaza of the pyramids, where the
            // statuettes are set in the altar and the game is won (script 28):
            // the route stops on its closing words.
            auto finale = step_beat(20, 11, 15, D::east, {"MOVE AHEAD", "stop:Continue"});
            finale.maze_exits = {0, 0, 0, 0};
            route.push_back(finale);
        }
        return route;
    }();
    return beats;
}

// Treasures' route plays to the game's end once this is true; until then it
// stops on this map (docs/GAMES.md, Treasures, step 6).
constexpr bool treasure_route_ends_the_game = true;
constexpr unsigned treasure_route_stops_on_map = 30;

// Treasures of the Savage Frontier's story, beat by beat, as its cluebook
// tells it: from the dwarves' fortress outside Llorkh, where the opening
// leaves the party.
const std::vector<RouteBeat> &treasure_route()
{
    static const std::vector<RouteBeat> beats = []
    {
        using D = opengold::por::MapDirection;
        std::vector<RouteBeat> route;
        // Before each fight the party has its dead raised at the temple
        // (15,3) and rests at the inn (12,1): the streets' camps draw the
        // guards.
        const auto heal_in_llorkh = [&route]
        {
            route.push_back({16, 15, 3, D::north, {"YES", "raise:dead", "Leave"}});
            route.push_back({16, 12, 1, D::north, {"YES"}});
        };
        // A party brought from Gateway carries the experience and money of
        // that game's end, which the test gives (a stand-in, as Secret's
        // route stands in for a party from Curse), given at each visit since
        // an award stops short of the level after next; it trains at
        // Llorkh's hall (1,3, facing its door to the west), a level a visit.
        for (int visit = 0; visit < 4; ++visit)
        {
            RouteBeat hall{16, 1, 3, D::west, train_everyone()};
            hall.test_experience = 100'000;
            hall.test_gold = 10'000;
            route.push_back(hall);
        }
        // Llorkh (map 16): the Zhentil troops and Lord's men about the city
        // first, so that the Keep's courtyard loses guards to them (the
        // cluebook's Llorkh).
        for (const auto &[x, y, answers] :
             std::initializer_list<std::tuple<unsigned, unsigned, std::vector<std::string>>>{
                 {9, 14, {}},
                 {13, 13, {}},
                 {11, 12, {"ATTACK"}},
                 {8, 13, {}},
                 {8, 8, {}},
                 {1, 8, {"HELP DWARVES"}},
                 {6, 8, {"HELP DWARVES"}},
                 {14, 1, {}},
                 {4, 1, {}},
                 {1, 1, {}},
                 {0, 6, {}},
                 {5, 5, {}},
                 {8, 0, {}},
             })
        {
            heal_in_llorkh();
            route.push_back({16, x, y, D::north, answers});
        }
        // The Keep's courtyard, its guards fewer now.
        heal_in_llorkh();
        route.push_back({16, 8, 10, D::north, {}});
        // Down the stairs east of the courtyard into Geildarr's Keep (map 17),
        // rested first.
        heal_in_llorkh();
        route.push_back(step_beat(16, 9, 10, D::east));
        // The Keep: the black-caped man giving orders (11,5); Cortarra, who
        // takes the shape of her prisoner Siulajia (11,1): the party casts
        // Detect Magic, sees through her and fights her; then Lord
        // Geildarr's chamber (15,4), each rested for.
        RouteBeat orders{17, 11, 5, D::north, {}};
        orders.rest_first = true;
        route.push_back(orders);
        RouteBeat twins{17, 11, 1, D::north, {"CAST"}};
        twins.rest_first = true;
        twins.cast_after = {"detect_magic", "detect_evil", "dispel_magic"};
        twins.memorize_first = true;
        route.push_back(twins);
        RouteBeat geildarr{17, 15, 4, D::north, {}};
        geildarr.rest_first = true;
        route.push_back(geildarr);
        // Back up the stairs (11,10 west) to Llorkh, free now, and down the
        // river by the boat rentals (3,15), open once Geildarr is dead.
        route.push_back(step_beat(17, 11, 10, D::west));
        route.push_back({16, 3, 15, D::north, {"YES"}});
        // Overland west to Secomber (map 19), each step as OPENGOLD_SECRET_FIND
        // finds it, fleeing what may be fled, and into the town.
        walk_overland(route, 57, {{15, 3, D::east}, {0, 3, D::south}});
        walk_overland(route, 56,
                      {{14, 4, D::south},
                       {14, 5, D::south},
                       {14, 6, D::west},
                       {13, 6, D::south},
                       {13, 7, D::west}});
        route.push_back(step_beat(56, 12, 7, D::west, {"YES"}));
        // Secomber: the first crystal battle (13,15), the Hosttower, Kraken
        // and Zhentil agents over their Lucky Papers; then Amanitas's house
        // (7,10), where Erek leads the party in and Amanitas gives it his
        // crown and sends it to Leilon.
        // Before each the party rests at the Inn of the Eccentric Owl (0,11,
        // facing south), for the town's guards break a camp in the streets
        // (the cluebook's Resting: inside any building). The crystal
        // battle's four Hosttower sorceresses open with Lightning Bolts: the
        // magic-user memorizes its Fireballs first and burns them.
        RouteBeat owl{19, 0, 11, D::south, {"YES"}};
        owl.memorize_first_of = {"fireball"};
        route.push_back(owl);
        route.push_back({19, 13, 15, D::north, {}});
        route.push_back({19, 0, 11, D::south, {"YES"}});
        route.push_back({19, 7, 10, D::north, {"YES"}});
        // Out of Secomber's west gate and overland, by the steps
        // OPENGOLD_SECRET_FIND finds, north and west to Leilon (map 20).
        walk_passages(route, 19,
                      {{7, 11, D::west},
                       {6, 11, D::west},
                       {5, 11, D::west},
                       {4, 11, D::west},
                       {3, 11, D::north},
                       {3, 10, D::west},
                       {2, 10, D::west},
                       {1, 10, D::west},
                       {0, 10, D::west}});
        walk_overland(route, 56,
                      {{10, 7, D::north},
                       {10, 6, D::north},
                       {10, 5, D::north},
                       {10, 4, D::north},
                       {10, 3, D::north},
                       {10, 2, D::north},
                       {10, 1, D::north}});
        walk_overland(route, 54,
                      {{15, 14, D::north},
                       {15, 13, D::north},
                       {15, 12, D::north},
                       {15, 11, D::north},
                       {15, 10, D::north},
                       {15, 9, D::east},
                       {0, 9, D::north}});
        walk_overland(route, 53,
                      {{14, 8, D::west},
                       {13, 8, D::west},
                       {12, 8, D::west},
                       {11, 8, D::west},
                       {10, 8, D::west},
                       {9, 8, D::west},
                       {8, 8, D::west},
                       {7, 8, D::west},
                       {6, 8, D::west}});
        route.push_back(step_beat(53, 5, 8, D::west, {"YES"}));
        // Leilon: the Tavern of the Rutting Buck (9,7), rested for, where the
        // party is drugged and wakes in a guarded barn; it escapes by a trick
        // through the door (12,5).
        RouteBeat tavern{20, 9, 7, D::north, {}};
        tavern.rest_first = true;
        // The clerics memorize Resist Fire for every member against the
        // Zhentil captain's hell hounds, past the caverns where no camp rests.
        tavern.ward_memorized = "resist_fire";
        route.push_back(tavern);
        route.push_back({20, 12, 5, D::north, {"SICK TRICK"}});
        // Out of the barn by the stone stairway and the farmhouse, out of
        // Leilon and overland, where the Waterdeep 'guards' catch the party,
        // Fell Hatchet sentences it and the sea elves' caverns (map 22) take
        // it in (the cluebook's Waterdeep and the Caverns).
        walk_passages(route, 20,
                      {{12, 5, D::east},
                       {13, 5, D::north},
                       {13, 4, D::east},
                       {14, 4, D::south},
                       {14, 5, D::south},
                       {14, 6, D::east},
                       {15, 6, D::east},
                       {5, 3, D::east},
                       {6, 3, D::east},
                       {7, 3, D::north},
                       {7, 2, D::north},
                       {7, 1, D::west},
                       {6, 1, D::west},
                       {5, 1, D::north},
                       {5, 0, D::north}});
        walk_overland(route, 53,
                      {{4, 7, D::north},
                       {4, 6, D::north},
                       {4, 5, D::north},
                       {4, 4, D::north},
                       {4, 3, D::north},
                       {4, 2, D::north},
                       {4, 1, D::east},
                       {5, 1, D::east},
                       {6, 1, D::east},
                       {7, 1, D::north}});
        walk_overland(route, 52, {{4, 14, D::north}});
        // Through the caverns past King Mandoul's gateway and its stone
        // guardians to the Zhentil captain's office (12,0), whose fall shows
        // who raids the farms (the second Lucky Paper mission), and up the
        // rope ladder (15,0) to the Smuggler's Dock.
        walk_passages(route, 22,
                      {{0, 15, D::east},  {1, 15, D::east},  {2, 15, D::north}, {2, 14, D::north},
                       {2, 13, D::north}, {2, 12, D::east},  {3, 12, D::east},  {4, 12, D::south},
                       {4, 13, D::east},  {5, 13, D::east},  {6, 13, D::east},  {7, 13, D::east},
                       {8, 13, D::north}, {8, 12, D::north}, {8, 11, D::north}, {8, 10, D::north},
                       {8, 9, D::north},  {8, 8, D::north},  {8, 7, D::north},  {8, 6, D::east},
                       {9, 6, D::north},  {9, 5, D::north},  {9, 4, D::north},  {9, 3, D::north},
                       {9, 2, D::east},   {10, 2, D::east},  {11, 2, D::east},  {12, 2, D::north},
                       {12, 1, D::east},  {13, 1, D::north}});
        auto captain = step_beat(22, 13, 0, D::west);
        // His hell hounds breathe fire at whoever stands beside them, every
        // turn: the clerics ward each member with the Resist Fire memorized
        // in Leilon and cure the badly hurt, for the hounds and guards
        // breaking a camp here leave the party worn.
        captain.cure_first = true;
        captain.ward_each_with = "resist_fire";
        captain.expect_cell = {{0x128, 1}};
        route.push_back(captain);
        walk_passages(route, 22,
                      {{12, 0, D::east},
                       {13, 0, D::south},
                       {13, 1, D::west},
                       {12, 1, D::south},
                       {12, 2, D::east},
                       {13, 2, D::east},
                       {14, 2, D::east},
                       {15, 2, D::north}});
        route.push_back(step_beat(22, 15, 1, D::north, {"YES"}));
        // Out of the dock's east gate and overland, as OPENGOLD_SECRET_FIND
        // finds the way, south and east to Daggerford (map 39).
        route.push_back(step_beat(21, 15, 9, D::east));
        walk_overland(route, 55,
                      {{9, 7, D::south},
                       {9, 8, D::east},
                       {10, 8, D::east},
                       {11, 8, D::south},
                       {11, 9, D::east},
                       {12, 9, D::east},
                       {13, 9, D::east},
                       {14, 9, D::east}});
        walk_overland(route, 56, {{1, 9, D::south}});
        route.push_back(step_beat(56, 1, 10, D::south, {"YES"}));
        // Daggerford (map 39), the cluebook's mission 3: the alliance's men
        // at the door (12,13), the mages at their pot (11,14), the troops
        // resting (6,7) and the fire giants (0,0), then their headquarters
        // (6,9), whose fall frees the men of the town; before each the party
        // rests at the Inn of the Safe Crossing (14,7), for the streets'
        // camps draw the guards.
        for (const auto &[x, y] : std::initializer_list<std::pair<unsigned, unsigned>>{
                 {12, 13}, {11, 14}, {6, 7}, {0, 0}, {6, 9}})
        {
            route.push_back({39, 14, 7, D::north, {"YES"}});
            route.push_back({39, x, y, D::north, {}});
        }
        // At once to the town's south edge (8,15), where the party catches
        // the Zhentarim slipping away to warn the Way Inn.
        walk_passages(route, 39,
                      {{6, 9, D::south},
                       {6, 10, D::south},
                       {6, 11, D::east},
                       {7, 11, D::south},
                       {7, 12, D::south},
                       {7, 13, D::south},
                       {7, 14, D::east},
                       {8, 14, D::south}});
        // Rested at the inn again before leaving by the edge (8,15), where
        // the streets' patrols find a worn party.
        route.push_back({39, 14, 7, D::north, {"YES"}});
        walk_passages(route, 39, {{8, 15, D::south}});
        // Overland east and south to the Way Inn (map 40).
        walk_overland(route, 56,
                      {{1, 12, D::east},
                       {2, 12, D::east},
                       {3, 12, D::east},
                       {4, 12, D::east},
                       {5, 12, D::south},
                       {5, 13, D::south}});
        route.push_back(step_beat(56, 5, 14, D::south, {"YES"}));
        // The Way Inn (map 40): the storeroom (12,2), the barracks (4,4), the
        // inn's door (7,6) and its hall (2,7); up the stairs, the alliance's
        // meeting (14,13), the room facing the stairs (4,15), and the Zhentil
        // lord holding Dauravyn Redbeard (7,15), whose freeing ends the third
        // Lucky Paper mission (cell 129h). Before each the party rests in a
        // room beside the storeroom (13,1) or, upstairs, the meeting's
        // (14,14): a camp in the yard draws the guards.
        for (const auto &[x, y] : std::initializer_list<std::pair<unsigned, unsigned>>{
                 {12, 2}, {4, 4}, {7, 6}, {2, 7}, {14, 13}, {4, 15}, {7, 15}})
        {
            const bool upstairs = y >= 9;
            route.push_back({40, 14 - (upstairs ? 0U : 1U), upstairs ? 14U : 1U, D::north, {}});
            RouteBeat fight{40, x, y, D::north, {}};
            fight.rest_first = true;
            fight.rest_for_spells = true;
            route.push_back(fight);
        }
        // Redbeard's captors (a Zhentil lord, neo-otyughs, greater margoyles
        // and six Hosttower sorceresses, each with a Lightning Bolt and a
        // Fireball, then Charm Monsters and Charm Person): the magic-user
        // memorizes its Fireballs and Charm Person, which frees a charmed
        // member, the clerics Dispel Magic. The sorceresses most often bolt
        // the party before its magic-user acts. Its SETUP MONSTER deals them
        // ahead and to the right (sides 3, or 9 facing west), so half of them
        // bolt from beside the party; the party made on Modify Character wins
        // 69 plays of 600 (2026-11-16), so it reloads until it wins.
        route.back().memorize_first_of = {"fireball", "charm_person", "dispel_magic"};
        route.back().reloads_until_won = 600;
        route.back().expect_cell = {{0x129, 1}};
        // Down the stairs (9,13) and out of the Way Inn, then overland north
        // by maps 56 and 54 to Yartar (map 23).
        walk_passages(route, 40,
                      {{7, 15, D::east},
                       {8, 15, D::east},
                       {9, 15, D::east},
                       {10, 15, D::east},
                       {11, 15, D::north},
                       {11, 14, D::north},
                       {11, 13, D::west}});
        route.push_back(step_beat(40, 10, 13, D::west, {"YES"}));
        walk_passages(route, 40,
                      {{9, 5, D::south},
                       {9, 6, D::west},
                       {8, 6, D::north},
                       {8, 5, D::north},
                       {8, 4, D::north},
                       {8, 3, D::north},
                       {8, 2, D::west},
                       {7, 2, D::north},
                       {7, 1, D::north},
                       {7, 0, D::north}});
        walk_overland(route, 56,
                      {{5, 14, D::north},
                       {5, 13, D::north},
                       {5, 12, D::west},
                       {4, 12, D::west},
                       {3, 12, D::west},
                       {2, 12, D::west},
                       {1, 12, D::north},
                       {1, 11, D::north},
                       {1, 10, D::north},
                       {1, 9, D::north},
                       {1, 8, D::north},
                       {1, 7, D::north},
                       {1, 6, D::north},
                       {1, 5, D::north},
                       {1, 4, D::north},
                       {1, 3, D::north},
                       {1, 2, D::north},
                       {1, 1, D::north}});
        walk_overland(route, 54,
                      {{6, 14, D::north},
                       {6, 13, D::north},
                       {6, 12, D::north},
                       {6, 11, D::north},
                       {6, 10, D::north},
                       {6, 9, D::north},
                       {6, 8, D::west},
                       {5, 8, D::west},
                       {4, 8, D::west},
                       {3, 8, D::north},
                       {3, 7, D::north},
                       {3, 6, D::north},
                       {3, 5, D::north},
                       {3, 4, D::north},
                       {3, 3, D::east},
                       {4, 3, D::east}});
        route.push_back(step_beat(54, 5, 3, D::east, {"YES"}));
        // Yartar (map 23), the fourth mission's start: the robed Krakens at
        // the secret door (11,14) and the Kraken base (14,15), whose papers
        // blame Triboar for the Waterbaron's kidnapping; before each the party
        // rests in a house off the streets (11,9), where the guards leave it
        // alone.
        for (const auto &[x, y] :
             std::initializer_list<std::pair<unsigned, unsigned>>{{11, 14}, {14, 15}})
        {
            route.push_back({23, 11, 9, D::north, {}});
            RouteBeat fight{23, x, y, D::north, {}};
            fight.rest_first = true;
            fight.rest_for_spells = true;
            route.push_back(fight);
        }
        // Out of Yartar's west gate and overland (map 54) to Triboar (map
        // 24), telling its guards the party has not come from Yartar.
        walk_passages(route, 23, {{14, 15, D::west},  {13, 15, D::west},  {12, 15, D::west},
                                  {11, 15, D::north}, {11, 14, D::north}, {11, 13, D::north},
                                  {11, 12, D::west},  {10, 12, D::west},  {9, 12, D::west},
                                  {8, 12, D::north},  {8, 11, D::north},  {8, 10, D::north},
                                  {8, 9, D::north},   {8, 8, D::west},    {7, 8, D::west},
                                  {6, 8, D::west},    {5, 8, D::west},    {4, 8, D::west},
                                  {3, 8, D::west},    {2, 8, D::west},    {1, 8, D::west},
                                  {0, 8, D::west}});
        walk_overland(route, 54, {{5, 3, D::west}, {4, 3, D::west}});
        route.push_back(step_beat(54, 3, 3, D::west, {"YES", "NO"}));
        // Triboar (map 24): into the Kraken complex by its secret door (4,0),
        // past the offal heap (0,1) and the ogres (3,4), to the Waterbaron
        // (1,7), whose freeing ends the fourth Lucky Paper mission (cell
        // 12Ah); before each the party rests in a house (13,7), off the
        // streets and out of the complex.
        for (const auto &[x, y] :
             std::initializer_list<std::pair<unsigned, unsigned>>{{4, 0}, {0, 1}, {3, 4}, {1, 7}})
        {
            route.push_back({24, 13, 7, D::north, {}});
            RouteBeat fight{24, x, y, D::north, {}};
            fight.rest_first = true;
            fight.rest_for_spells = true;
            route.push_back(fight);
        }
        route.back().expect_cell = {{0x12A, 1}};
        // The Lord Protector of Triboar hears the Waterbaron (12,2); the party
        // rests first.
        route.push_back({24, 13, 7, D::north, {}});
        RouteBeat protector{24, 12, 2, D::north, {}};
        protector.rest_first = true;
        route.push_back(protector);
        // Out of Triboar and overland north and west (maps 54 and 52) to
        // Longsaddle (map 25).
        walk_passages(route, 24,
                      {{12, 3, D::south},
                       {12, 4, D::south},
                       {12, 5, D::south},
                       {12, 6, D::south},
                       {12, 7, D::south},
                       {12, 8, D::south},
                       {12, 9, D::east},
                       {13, 9, D::east},
                       {14, 9, D::east},
                       {15, 9, D::east}});
        walk_overland(route, 54,
                      {{3, 3, D::north}, {3, 2, D::west}, {2, 2, D::north}, {2, 1, D::north}});
        walk_overland(route, 52,
                      {{13, 14, D::north},
                       {13, 13, D::north},
                       {13, 12, D::north},
                       {13, 11, D::west},
                       {12, 11, D::west}});
        route.push_back(step_beat(52, 11, 11, D::west, {"YES"}));
        // Longsaddle (map 25), the fifth mission: in market hours the
        // Krakens threatening a rancher (7,12); then the ranches the Krakens
        // hold, Legson's (12,0) and its cellar (10,1), the Big Star (2,2) and
        // its barn (3,1), the ettins' (2,9) and their pit (3,8), the ranch
        // whose men ask the party's business (14,10; 13,11) and its griffons'
        // trap-door (15,12), and the Harpells' mansion (10,4), taken by spies;
        // last the ranchers who point to the barn (3,13), and the barn of
        // statues (1,15), where Malchor Harpell thanks the
        // party, ending the mission (cell 12Bh). Before each the party rests
        // indoors (8,7), away from the market, and goes on in the morning.
        route.push_back({25, 8, 7, D::north, {}});
        RouteBeat market{25, 7, 12, D::north, {}};
        market.rest_first = true;
        market.wait_until_minute = 9 * 60;
        route.push_back(market);
        for (const auto &[x, y, answers] :
             std::initializer_list<std::tuple<unsigned, unsigned, std::vector<std::string>>>{
                 {12, 0, {}},
                 {10, 1, {}},
                 {2, 2, {}},
                 {3, 1, {}},
                 {2, 9, {}},
                 {3, 8, {}},
                 {14, 10, {"YES"}},
                 {13, 11, {"YES"}},
                 {15, 12, {}},
                 {10, 4, {"INVESTIGATE"}},
                 {3, 13, {}},
                 {1, 15, {}}})
        {
            route.push_back({25, 8, 7, D::north, {}});
            RouteBeat ranch{25, x, y, D::north, answers};
            ranch.rest_first = true;
            ranch.rest_for_spells = true;
            ranch.wait_until_minute = 9 * 60;
            route.push_back(ranch);
        }
        route.back().expect_cell = {{0x12B, 1}};
        // Back overland south (maps 52, 53 and 55) to the Smuggler's Dock,
        // paying its gatekeeper's fee, for the ship to Mintarn.
        walk_passages(route, 25,
                      {{1, 15, D::north},
                       {1, 14, D::east},
                       {2, 14, D::east},
                       {3, 14, D::north},
                       {3, 13, D::east},
                       {4, 13, D::east},
                       {5, 13, D::south},
                       {5, 14, D::south},
                       {5, 15, D::south}});
        walk_overland(route, 52, {{10, 12, D::south}, {10, 13, D::south}, {10, 14, D::south}});
        walk_overland(route, 53,
                      {{13, 1, D::south},
                       {13, 2, D::south},
                       {13, 3, D::south},
                       {13, 4, D::south},
                       {13, 5, D::south},
                       {13, 6, D::south},
                       {13, 7, D::south},
                       {13, 8, D::south},
                       {13, 9, D::south},
                       {13, 10, D::south},
                       {13, 11, D::south},
                       {13, 12, D::south},
                       {13, 13, D::south},
                       {13, 14, D::south}});
        walk_overland(route, 55,
                      {{8, 1, D::south},
                       {8, 2, D::south},
                       {8, 3, D::south},
                       {8, 4, D::south},
                       {8, 5, D::south}});
        route.push_back(step_beat(55, 8, 6, D::south, {"TRAVELLING", "ACCEPT"}));
        // At sunrise the ship at the Mintarn pier (2,10, stepped onto from
        // the east) takes the party to Mintarn (map 26) for 80 platinum.
        auto to_mintarn = step_beat(21, 3, 10, D::west, {"ACCEPT"});
        to_mintarn.wait_until_minute = 6 * 60;
        route.push_back(to_mintarn);
        // Mintarn (map 26), the sixth mission: the Tyrant hears the party has
        // come to help (12,5); Southclaw Fort, where Princess Jagaerda joins
        // the party (8,11), its trash heap (11,11), its giants' master
        // (11,12) and its efreets (13,13); then the warehouses the invaders
        // hold (8,8; 7,7; 4,8; 2,11) and their headquarters (0,8), whose map
        // the Tyrant reads, ending the mission (cell 12Ch). First the party
        // trains at the Magical Arts hall (1,5, facing west); before each
        // fight it has its dead raised at the Temple of Waukeen (9,5) and
        // rests in a house by the docks (6,6).
        for (int visit = 0; visit < 2; ++visit)
        {
            route.push_back({26, 1, 5, D::west, train_everyone()});
        }
        for (const auto &[x, y, answers] :
             std::initializer_list<std::tuple<unsigned, unsigned, std::vector<std::string>>>{
                 {12, 5, {"CAME TO HELP"}},
                 {8, 11, {"YES"}},
                 {11, 11, {}},
                 {11, 12, {}},
                 {13, 13, {}},
                 {8, 8, {}},
                 {7, 7, {}},
                 {4, 8, {}},
                 {2, 11, {}},
                 {0, 8, {}}})
        {
            route.push_back({26, 9, 5, D::north, {"YES", "raise:dead", "Leave"}});
            route.push_back({26, 6, 6, D::north, {}});
            RouteBeat fight{26, x, y, D::north, answers};
            fight.rest_first = true;
            fight.rest_for_spells = true;
            route.push_back(fight);
        }
        route.back().expect_cell = {{0x12C, 1}};
        // At 0600 the sailor on the passenger dock (7,0, facing the sea) books
        // the party's passage to Orlumbor (map 27); as it boards, pirates
        // carry Siulajia off to Luskan, and the party sails on.
        route.push_back({26, 6, 6, D::north, {}});
        RouteBeat to_orlumbor{26, 7, 0, D::north, {"ORLUMBOR", "LEAVE"}};
        to_orlumbor.rest_first = true;
        to_orlumbor.wait_until_minute = 6 * 60;
        route.push_back(to_orlumbor);
        // Orlumbor (map 27), the seventh mission: the alliance's meeting in
        // the hidden Temple of Talos (10,15), whose plans show the warships
        // off the coast are the pirates', and Tulgar Wrighttson told of them
        // at his castle (11,3), ending the mission (cell 12Dh). The party
        // rests first in the harbor office (3,3).
        route.push_back({27, 3, 3, D::north, {}});
        RouteBeat talos{27, 10, 15, D::north, {}};
        talos.rest_first = true;
        talos.rest_for_spells = true;
        route.push_back(talos);
        route.push_back({27, 3, 3, D::north, {}});
        RouteBeat wrighttson{27, 11, 3, D::north, {}};
        wrighttson.rest_first = true;
        wrighttson.expect_cell = {{0x12D, 1}};
        route.push_back(wrighttson);
        // At 0600 a ship at the dock where the party came in (13,11, facing
        // the sea) sails it to Neverwinter (map 28).
        route.push_back({27, 3, 3, D::north, {}});
        RouteBeat to_neverwinter{27, 13, 11, D::east, {"NEVERWINTER"}};
        to_neverwinter.rest_first = true;
        to_neverwinter.wait_until_minute = 6 * 60;
        route.push_back(to_neverwinter);
        // Neverwinter (map 28), the eighth mission: the gardener whose
        // gardens the Krakens hold (6,13); their bases behind the secret door
        // (10,1) and in the east (15,11), with the ambassadors they hide, Froal
        // of Neverwinter (10,0) and Wimeg of Mirabar (15,15), whose freeing
        // ends the mission (cell 12Eh); then Lord Nasher (5,7). The party
        // keeps away from the city's guards, who would search it and find the
        // ambassadors' orders the Krakens planted. Before each the party has
        // its dead raised at the temple (9,10) and rests there: the Krakens
        // break camps in the gardens until the ambassadors are free (the
        // cluebook's Resting). First it trains at the Hall of Training
        // (6,11, facing west), a level a visit, on the experience earned
        // since Llorkh.
        for (int visit = 0; visit < 2; ++visit)
        {
            route.push_back({28, 6, 11, D::west, train_everyone()});
        }
        for (const auto &[x, y] : std::initializer_list<std::pair<unsigned, unsigned>>{
                 {6, 13}, {10, 1}, {10, 0}, {15, 11}, {15, 15}})
        {
            route.push_back({28, 9, 10, D::north, {"YES", "raise:dead", "Leave"}});
            RouteBeat fight{28, x, y, D::north, {}};
            fight.rest_first = true;
            fight.rest_for_spells = true;
            // The eastern base's fire giant mages shield themselves in fire
            // and fireball the party: the clerics ward each member with
            // Resist Fire.
            if (x == 15 && y == 11)
            {
                fight.ward_each_with = "resist_fire";
            }
            route.push_back(fight);
        }
        route.back().expect_cell = {{0x12E, 1}};
        route.push_back({28, 5, 7, D::north, {}});
        // Out of Neverwinter's north gate and overland (map 51) to Luskan
        // (map 31).
        walk_passages(route, 28,
                      {{3, 4, D::north},
                       {3, 3, D::west},
                       {2, 3, D::west},
                       {1, 3, D::north},
                       {1, 2, D::north},
                       {1, 1, D::east},
                       {2, 1, D::east},
                       {3, 1, D::east},
                       {4, 1, D::east},
                       {5, 1, D::east},
                       {6, 1, D::east},
                       {7, 1, D::north},
                       {7, 0, D::north}});
        walk_overland(route, 51,
                      {{12, 14, D::north},
                       {12, 13, D::north},
                       {12, 12, D::north},
                       {12, 11, D::north},
                       {12, 10, D::north},
                       {12, 9, D::north},
                       {12, 8, D::north},
                       {12, 7, D::west},
                       {11, 7, D::west},
                       {10, 7, D::west}});
        route.push_back(step_beat(51, 9, 7, D::north, {"YES"}));
        // Luskan (map 31), first visit: Siulajia freed from her guards
        // (11,1); then Jagaerda (6,6), whose boat takes the party to Ruathym
        // to rescue Captain Redleg. The party rests first in a house (11,6),
        // off the streets.
        route.push_back({31, 11, 6, D::north, {}});
        RouteBeat siulajia{31, 11, 1, D::north, {}};
        siulajia.rest_first = true;
        siulajia.rest_for_spells = true;
        // Her Banites' Hold Person takes the party's members, and a held one
        // is slain at the next blow: the clerics ward each member with
        // Protection from Evil (2 better saves against the evil) and keep
        // Dispel Magic to free the held, as a player readies a known fight.
        siulajia.ward_each_with = "protection_from_evil";
        siulajia.memorize_first_of = {"dispel_magic"};
        route.push_back(siulajia);
        route.push_back({31, 11, 6, D::north, {}});
        RouteBeat jagaerda{31, 6, 6, D::north, {"YES"}};
        jagaerda.rest_first = true;
        route.push_back(jagaerda);
        // Ruathym (map 32), the ninth mission: the Luskan men at their table
        // (8,2), the cloaked men's blockade orders (10,5), the two captains
        // (3,7) and the chamber beside Redleg's cell (4,9); then Captain
        // Redleg himself (3,8), freed of his charm. Before each the party
        // rests at the inn (9,8).
        for (const auto &[x, y] : std::initializer_list<std::pair<unsigned, unsigned>>{
                 {8, 2}, {10, 5}, {3, 7}, {4, 9}, {3, 8}})
        {
            route.push_back({32, 9, 8, D::north, {"YES"}});
            RouteBeat fight{32, x, y, D::north, {}};
            fight.rest_first = true;
            fight.rest_for_spells = true;
            route.push_back(fight);
        }
        // Redleg, freed, is not taken into the party: alone he finds his
        // boat before the guards can gather, and the party leaves unscathed
        // (the cluebook's Flint at Redleg's cell), where with him it meets
        // the island's largest fight at the pier.
        route.back().answers = {"RELEASE HIM", "NO"};
        // Redleg's boat under the north-east pier (15,4) carries the party off
        // Ruathym, ending the ninth mission (cell 12Fh).
        RouteBeat redleg_boat{32, 15, 4, D::north, {"GET IN BOAT"}};
        redleg_boat.expect_cell = {{0x12F, 1}};
        route.push_back(redleg_boat);
        // Luskan again: Redleg's farewell (8,3) and Jagaerda, captured for her
        // help, freed to join the party once more (7,4). The room where a
        // captain argues with a sorceress (11,3), whose guards and mages
        // leave the route's party all but dead, is left alone: no mission
        // asks for it.
        route.push_back({31, 8, 3, D::north, {}});
        route.push_back({31, 11, 6, D::north, {}});
        RouteBeat jagaerda_freed{31, 7, 4, D::north, {"ACCEPT"}};
        jagaerda_freed.rest_first = true;
        route.push_back(jagaerda_freed);
        // Out by Luskan's gate (15,15), where Jagaerda stays behind, by the
        // east road, rested first in the house (11,6), for the road's camps
        // draw wandering monsters.
        route.push_back({31, 11, 6, D::north, {}});
        auto to_mirabar = step_beat(31, 15, 15, D::south, {"EAST TO MIRABAR"});
        to_mirabar.rest_first = true;
        route.push_back(to_mirabar);
        // Overland east (maps 51 and 52) to Mirabar (map 30), for its
        // council.
        walk_overland(route, 51,
                      {{9, 5, D::north},
                       {9, 4, D::east},
                       {10, 4, D::east},
                       {11, 4, D::east},
                       {12, 4, D::east},
                       {13, 4, D::east},
                       {14, 4, D::east}});
        walk_overland(route, 52,
                      {{1, 4, D::north},
                       {1, 3, D::north},
                       {1, 2, D::east},
                       {2, 2, D::east},
                       {3, 2, D::east},
                       {4, 2, D::east}});
        route.push_back(step_beat(52, 5, 2, D::east, {"YES"}));
        // Mirabar (map 30): the council (7,9), shown that Luskan and the
        // Hosttower mislead it, and the Krakens who burst in (the tenth
        // Lucky Paper mission);
        // then King Steelfist's help for the last quest (15,4) and the
        // Marchion's hammer, Kriiador, taken along (1,7).
        // The party, hurt on the road, rests first at the Dig Inn (4,11).
        route.push_back({30, 4, 11, D::north, {"YES"}});
        RouteBeat council{30, 7, 9, D::north, {}};
        council.rest_first = true;
        council.rest_for_spells = true;
        // Its Hosttower sorceresses charm: Charm Person and Dispel Magic free
        // the charmed. The Krakens and sorceresses burst in a round or two
        // apart, as the dwarves come to help (cell 44h), and the party beats
        // them with Treasures' own spell table.
        council.memorize_first_of = {"charm_person", "dispel_magic"};
        route.push_back(council);
        route.push_back({30, 15, 4, D::north, {"YES"}});
        route.push_back({30, 1, 7, D::north, {"YES"}});
        // The council done, the tenth mission is told done by cell 1F8h at
        // FFh (no script sets cell 130h): the luck reading is now 12, and
        // Neverwinter's north pier opens for Fireshear. The party has its
        // dead raised at the Temple of Vergadain (2,13) and rests at the Dig
        // Inn (4,11).
        RouteBeat council_done{30, 2, 13, D::north, {"YES", "raise:dead", "Leave"}};
        council_done.expect_cell = {{0x1F8, 0xFF}};
        route.push_back(council_done);
        route.push_back({30, 4, 11, D::north, {"YES"}});
        // Out of Mirabar's south gate (7,15) and overland west and south
        // (maps 52, 51 and 53) to Neverwinter's east gate (map 28).
        walk_passages(route, 30,
                      {{4, 11, D::east},
                       {5, 11, D::east},
                       {6, 11, D::south},
                       {6, 12, D::south},
                       {6, 13, D::east},
                       {7, 13, D::south},
                       {7, 14, D::south},
                       {7, 15, D::south}});
        walk_overland(route, 52,
                      {{6, 3, D::south},
                       {6, 4, D::south},
                       {6, 5, D::south},
                       {6, 6, D::south},
                       {6, 7, D::south},
                       {6, 8, D::south},
                       {6, 9, D::south},
                       {6, 10, D::west},
                       {5, 10, D::west},
                       {4, 10, D::west},
                       {3, 10, D::west},
                       {2, 10, D::west},
                       {1, 10, D::west},
                       {0, 10, D::south}});
        walk_overland(route, 51,
                      {{14, 11, D::south},
                       {14, 12, D::south},
                       {14, 13, D::west},
                       {13, 13, D::south},
                       {13, 14, D::south}});
        route.push_back(step_beat(53, 2, 1, D::west, {"YES"}));
        // Neverwinter (map 28): Northpier Shipping (4,12) books the passage
        // to Fireshear, 250 platinum, and at 0700 the ship sails from the new
        // north pier (2,11, facing west). The party rests first in the
        // gardener's house (6,13).
        route.push_back({28, 4, 12, D::north, {"YES"}});
        route.push_back({28, 6, 13, D::north, {}});
        auto to_fireshear = step_beat(28, 2, 11, D::west, {"YES"});
        to_fireshear.rest_first = true;
        to_fireshear.wait_until_minute = 7 * 60;
        route.push_back(to_fireshear);
        // Fireshear (map 33), its leaders taken by the yeti: from the landing
        // (6,15) west through the streets to the mine shaft at (0,14), down
        // into the Firemines (map 34) at (5,15), where the yeti leave the
        // party to rest (the cluebook's Firemines).
        walk_passages(route, 33,
                      {{6, 15, D::north}, {6, 14, D::east},  {7, 14, D::north}, {7, 13, D::east},
                       {8, 13, D::north}, {8, 12, D::north}, {8, 11, D::west},  {7, 11, D::north},
                       {7, 10, D::west},  {6, 10, D::north}, {6, 9, D::west},   {5, 9, D::west},
                       {4, 9, D::west},   {3, 9, D::south},  {3, 10, D::south}, {3, 11, D::south},
                       {3, 12, D::west},  {2, 12, D::south}, {2, 13, D::west},  {1, 13, D::west}});
        route.push_back(step_beat(33, 0, 13, D::south, {"DESCEND", "AVOID"}));
        RouteBeat mines_rest{34, 5, 15, D::north, {"AVOID"}};
        mines_rest.rest_first = true;
        mines_rest.rest_for_spells = true;
        route.push_back(mines_rest);
        // That refuge leads nowhere else: rested, the party climbs back up
        // (to 0,14), goes east to the shaft at (5,12) and down to the mines
        // (9,12), and on to their great chamber (7,7), where the yeti and
        // their ice hounds charge in two waves, the grand yeti with the
        // second. His fall frees the Shearmaster and Fireshear's other
        // leaders (cell 22Dh).
        route.push_back(step_beat(34, 5, 15, D::east, {"CLIMB UP", "AVOID"}));
        walk_passages(route, 33,
                      {{0, 14, D::north},
                       {0, 13, D::east},
                       {1, 13, D::east},
                       {2, 13, D::east},
                       {3, 13, D::east},
                       {4, 13, D::north}});
        route.push_back(step_beat(33, 4, 12, D::east, {"DESCEND", "AVOID"}));
        walk_passages(route, 34,
                      {{9, 12, D::south},
                       {9, 13, D::west},
                       {8, 13, D::north},
                       {8, 12, D::north},
                       {8, 11, D::north},
                       {8, 10, D::north},
                       {8, 9, D::north},
                       {8, 8, D::west}});
        RouteBeat grand_yeti = step_beat(34, 7, 8, D::north);
        grand_yeti.expect_cell = {{0x22D, 1}};
        route.push_back(grand_yeti);
        // West through the mines and up the shaft at (1,11) to Fireshear
        // (2,5), whose streets are safe now; resting first, the party steps
        // west into the cavern on the sea (1,4), where the yeti and then the
        // beholder make their last stand, freeing the dock (cell 231h).
        walk_passages(route, 34,
                      {{7, 7, D::west},
                       {6, 7, D::south},
                       {6, 8, D::west},
                       {5, 8, D::west},
                       {4, 8, D::south},
                       {4, 9, D::west},
                       {3, 9, D::west},
                       {2, 9, D::west},
                       {1, 9, D::south},
                       {1, 10, D::south}});
        route.push_back(step_beat(34, 1, 11, D::west, {"CLIMB UP", "AVOID"}));
        walk_passages(
            route, 33,
            {{2, 5, D::east}, {3, 5, D::east}, {4, 5, D::north}, {4, 4, D::west}, {3, 4, D::west}});
        auto beholder = step_beat(33, 2, 4, D::west);
        beholder.rest_first = true;
        beholder.rest_for_spells = true;
        beholder.expect_cell = {{0x231, 1}};
        route.push_back(beholder);
        // From the dock (0,4, facing the sea) the Fireshear dwarves take the
        // party free to the Ice Peak's Aurilssbaarg (map 35), rested first.
        auto to_ice_peak = step_beat(33, 1, 4, D::west, {"ICE PEAK"});
        to_ice_peak.rest_first = true;
        route.push_back(to_ice_peak);
        // Aurilssbaarg (map 35), the Ice Peak's town: from the dock (2,15)
        // north and east by the streets to the path east (15,10) to Bjorn's
        // Hold (map 36), refusing the guards' escort to Tranjer Rolsk's trap
        // and telling the Northmen the party fights Luskan, as the cluebook
        // bids.
        walk_passages(route, 35,
                      {{2, 15, D::north},  {2, 14, D::north}, {2, 13, D::north},  {2, 12, D::north},
                       {2, 11, D::east},   {3, 11, D::east},  {4, 11, D::east},   {5, 11, D::south},
                       {5, 12, D::south},  {5, 13, D::east},  {6, 13, D::east},   {7, 13, D::east},
                       {8, 13, D::north},  {8, 12, D::east},  {9, 12, D::east},   {10, 12, D::east},
                       {11, 12, D::east},  {12, 12, D::east}, {13, 12, D::north}, {13, 11, D::east},
                       {14, 11, D::north}, {14, 10, D::east}, {15, 10, D::east}},
                      false, false, false, {"TELL STORY", "REFUSE"});
        // Bjorn's Hold (map 36), where the Northmen are friends: rested
        // indoors by the feasters' campfire (11,5), off the mountain paths,
        // the party goes to the pier (15,3), where Northmen and Ice Hunters
        // take it to Icewolf (map 37). The two parties of black-robed men
        // (7,10 and beyond the hot archway at 10,8) are left alone: no later
        // script asks whether they fell (cells 581h to 585h are Bjorn's
        // Hold's own), and the first beats the route's party with every dice.
        route.push_back({36, 11, 5, D::north, {}});
        RouteBeat to_icewolf{36, 15, 3, D::north, {"READY"}};
        to_icewolf.rest_first = true;
        route.push_back(to_icewolf);
        // Icewolf (map 37): the Northmen's gift, the Northhelm, worn by the
        // first paladin (10,0); training with the Northman (15,6), a level a
        // visit; then Bleak Sky at Morning (10,8, stepped into from the
        // north), who asks whether it is time and lets the party through the
        // one-way door (8,9), the last safe place, to rest and climb west
        // and north to the Ice Peak's lair (map 38).
        RouteBeat northhelm{37, 10, 0, D::north, {}};
        northhelm.take_items = {{"HELM", "paladin 1"}};
        route.push_back(northhelm);
        for (int visit = 0; visit < 2; ++visit)
        {
            route.push_back({37, 15, 6, D::north, train_everyone()});
        }
        route.push_back(step_beat(37, 10, 7, D::south, {"IT IS TIME"}));
        auto climb = step_beat(37, 8, 9, D::north);
        climb.rest_first = true;
        climb.rest_for_spells = true;
        route.push_back(climb);
        walk_passages(route, 37,
                      {{8, 8, D::north}, {8, 7, D::north}, {8, 6, D::east},  {9, 6, D::north},
                       {9, 5, D::west},  {8, 5, D::west},  {7, 5, D::north}, {7, 4, D::north},
                       {7, 3, D::west},  {6, 3, D::west},  {5, 3, D::north}, {5, 2, D::west},
                       {4, 2, D::north}, {4, 1, D::west},  {3, 1, D::west},  {2, 1, D::north},
                       {2, 0, D::west},  {1, 0, D::west},  {0, 0, D::south}, {0, 1, D::west}});
        // The Ice Peak (map 38): rested by the warm spring (3,13), where no
        // monster breaks a camp, the party goes past the avalanche (9,10),
        // whose stones it moves to find the tunnel, into the great cavern
        // (7,8), and west and north into Freezefire's lair (3,6), where the
        // black-robed men combing the hoard and then Freezefire himself,
        // risen as the Gem is pried from his claw, fall with the dwarves'
        // help (cell 25Ch set to 1), and the game is won.
        // The wand of fireballs among the dead adventurers' remains (12,7)
        // goes to the magic-user, readied for the lair.
        RouteBeat remains{38, 12, 7, D::north, {}};
        remains.take_wand = true;
        route.push_back(remains);
        route.push_back({38, 9, 10, D::north, {"ENTER TUNNEL"}});
        auto cavern = step_beat(38, 7, 8, D::west);
        cavern.rest_first = true;
        cavern.rest_for_spells = true;
        // Dispel Magic frees the members the lair's black-robed men hold.
        cavern.memorize_first_of = {"dispel_magic"};
        route.push_back(cavern);
        walk_passages(route, 38,
                      {{6, 8, D::west}, {5, 8, D::north}, {5, 7, D::west}, {4, 7, D::north}});
        auto freezefire = step_beat(38, 4, 6, D::west);
        freezefire.expect_cell = {{0x25C, 1}};
        // The black-robed masters, mages and spies hold, slay and charm the
        // party's members (Hold Monster, Hold Person, Slay Living and Charm
        // Person) from the first round; the route's player frees them with
        // Dispel Magic (memorized in the cavern). About one play in twenty is
        // won, most of the party dead (2026-11-15): the party reloads until
        // it wins, and with the dwarves beats Freezefire, risen at once after
        // them. The ice hunters' care (PROGRAM 5) makes the party whole as
        // the game is won, as the cluebook has it ("One member surviving is
        // all the party needs to win the battle").
        freezefire.reloads_until_won = 600;
        route.push_back(freezefire);
        // A camp on the overland (maps 51 to 57) draws its wandering
        // monsters night after night: the party walks on between towns
        // instead, as a player hurries to the next inn. So does one in the
        // sea elves' caverns (map 22), whose ghouls, wights, otyughs and
        // carrion crawlers break every night's rest: the party walks on to
        // the Zhentil captain's office, resting only before it. So does a
        // camp in Aurilssbaarg's streets, which the black-robed men and
        // Luskan's guards hunting the party fall on (the cluebook bids it
        // hurry through): the party walks on to Bjorn's Hold's campfire.
        constexpr unsigned first_overland = 51, last_overland = 57, caverns = 22, aurilssbaarg = 35;
        for (auto &beat : route)
        {
            beat.camp_broken = (beat.map >= first_overland && beat.map <= last_overland) ||
                               beat.map == caverns || beat.map == aurilssbaarg;
        }
        return route;
    }();
    return beats;
}

const std::vector<RouteBeat> &route_for(GoldBoxGame game)
{
    switch (game)
    {
    case GoldBoxGame::treasures_of_the_savage_frontier:
        return treasure_route();
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
        opengold::test::walk_original_creation(creator, learned_first);
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

// Secret's camp Fix (its camp.pas, CampFixMenu): the party rests only while
// its cleric's cures heal it, rolled from its spell slots, waking the
// unconscious, and in a second rest its Raise Dead slots raise the dead; no
// memorized spell is spent, and the rests take hours, not days.
void secret_camp_fixes_the_party()
{
    using opengold::por::TourPhase;
    const char *games = std::getenv("OPENGOLD_GOLDBOX_DIR");
    const auto folder = games ? std::filesystem::path(games) / "SECRET" : std::filesystem::path();
    if (!games || opengold::identify_game(folder) != GoldBoxGame::secret_of_the_silver_blades)
    {
        std::cout << "Secret's files are not here; its Fix is not played.\n";
        return;
    }
    auto session = opengold::load_campaign(folder, GoldBoxGame::secret_of_the_silver_blades);
    std::shared_ptr<const opengold::rules::CharacterRules> creation =
        opengold::original::character_rules(opengold::original::default_aging,
                                            GoldBoxGame::secret_of_the_silver_blades);
    auto party = std::make_shared<opengold::CampaignParty>(
        opengold::original::rules_module(
            opengold::load_original_content(folder, GoldBoxGame::secret_of_the_silver_blades)),
        creation);
    std::vector<opengold::MemberId> members;
    for (const char *klass : {"cleric", "fighter", "fighter"})
    {
        opengold::CharacterCreator creator(creation, 60 + static_cast<unsigned>(members.size()));
        creator.select(opengold::rules::CreationField::race, "human");
        creator.select(opengold::rules::CreationField::character_class, klass);
        creator.name(std::string(klass) + " " + std::to_string(members.size() + 1));
        opengold::test::walk_original_creation(creator, learned_first);
        members.push_back(party->add_pc(creator.create_character()));
        opengold::test::modify_to_the_highest(*party, members.back());
    }
    const auto cleric = members[0], fallen = members[1], dead = members[2];
    // The cleric trained until it has fifth-level slots, for Raise Dead.
    for (int level = 0;
         level < 6 && party->member(cleric).character.sheet().original->cleric_slots[4] == 0;
         ++level)
    {
        party->award_experience(1'000'000, "test:fix-" + std::to_string(level), cleric);
        party->train_at_hall_level(cleric, 0xFFU, {});
    }
    check(party->member(cleric).character.sheet().original->cleric_slots[4] > 0,
          "The cleric has fifth-level slots");
    session.campaign_party(party);
    for (int step = 0; step < 400 && !session.can_leave(); ++step)
    {
        const auto &s = session.snapshot();
        if (s.phase == TourPhase::running)
        {
            session.advance(1.0);
        }
        else if (s.phase == TourPhase::awaiting_continue)
        {
            check(session.choose(s.continue_ticket, 0), "The opening goes on");
        }
        else if (s.phase == TourPhase::treasure)
        {
            check(session.leave_treasure(s.continue_ticket), "The party leaves the pile");
        }
        else
        {
            break;
        }
    }
    check(session.can_leave(), "The opening ends in New Verdigris");
    auto hurt = party->checkpoint();
    for (auto &member : hurt.roster)
    {
        if (member.id == fallen)
        {
            member.vitals.hit_points = 0;
        }
        if (member.id == dead)
        {
            member.vitals.hit_points = 0;
            member.vitals.dead = true;
        }
    }
    party->restore(std::move(hurt));
    // No interruption: the rests are not rolled for.
    opengold::test::TourAccess::set_variable(
        session,
        opengold::por::game_address(opengold::por::ecl_rest_interruption_chance,
                                    GoldBoxGame::secret_of_the_silver_blades),
        0);
    const auto spells_before = party->memorized_spells(cleric);
    const auto minutes_before = party->state().time_minutes;
    check(session.camp_offers_fix() && session.camp_fix(), "The camp's Fix begins");
    for (int step = 0; step < 50 && session.snapshot().phase == TourPhase::running; ++step)
    {
        session.advance(1.0);
    }
    check(party->member(fallen).vitals.hit_points > 0,
          "The cure slots wake the fallen fighter: " + session.snapshot().dialogue);
    check(!party->member(dead).vitals.dead,
          "The second rest raises the dead fighter: " + session.snapshot().dialogue);
    check(session.snapshot().dialogue.find("has been raised from the dead") != std::string::npos,
          "The Fix tells the raising: " + session.snapshot().dialogue);
    check(party->state().time_minutes > minutes_before &&
              party->state().time_minutes < minutes_before + 24 * 60,
          "The Fix takes hours, not a night");
    check(party->memorized_spells(cleric) == spells_before, "The Fix spends no memorized spell");
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

enum class CurseProgress
{
    none,
    sewers,
    first_bond,
    hap,
    second_bond,
    third_bond,
    moander_recovery,
    fourth_bond,
    ending
};

// Earned milestones follow the physical opening, sewer lairs, first bond,
// Hap and Dracandros. Keep these checks separate from the assisted story
// route; none of these milestones alone establishes a campaign win.
std::vector<RouteBeat> curse_earned_route(CurseProgress progress)
{
    using D = opengold::por::MapDirection;
    const auto &story = curse_route();
    std::vector<RouteBeat> beats(story.begin(), story.begin() + 6);
    const auto world = [&](std::vector<std::string> choices, bool rest = false)
    {
        beats.push_back({0, 0, 0, D::north, std::move(choices), rest, false, true});
        beats.back().rest_for_spells = rest;
    };
    // The same shop event appears in disconnected geometry. Use the town
    // shop a party can actually reach from the inn, not the assisted route's.
    beats.front().x = 3;
    beats.front().y = 12;
    beats.front().facing = D::west;
    beats.front().step = true;
    beats.front().answers = {"YES",
                             "buy:LONG SWORD@paladin 1,paladin 2,ranger 3",
                             "buy:MACE@cleric 4,cleric 5",
                             "buy:DAGGER@magic_user 6",
                             "buy:PLATE ARMOR@paladin 1,paladin 2,ranger 3,cleric 4,cleric 5",
                             "buy:SHIELD@paladin 1,paladin 2,ranger 3,cleric 4,cleric 5"};
    beats[1].x = 5;
    beats[1].y = 5;
    beats[1].facing = D::east;
    beats[1].step = true;
    beats[2].y = 1;
    beats[2].step = true;
    // The guards' refusal, carriage and surrender lead into the guild.
    beats.erase(beats.begin() + 3);
    beats.back() = {1, 10, 15, D::south, {}, true};
    beats.back().step = true;
    // Clear the western lairs first, resting in each cleared room before
    // crossing to the next one. The folded corridors are player-accessible.
    for (const auto &[x, y] : std::array<std::pair<unsigned, unsigned>, 2>{{{1, 14}, {8, 14}}})
    {
        beats.push_back({3, x, y, D::north, {}, true});
    }
    beats.push_back({3, 14, 6, D::north, train_everyone(), true});
    // The hidden guild chambers and cleared lairs are safe camps (cluebook).
    for (const auto &[x, y] : std::array<std::pair<unsigned, unsigned>, 2>{{{11, 2}, {13, 2}}})
    {
        beats.push_back({3, x, y, D::north, {}, true});
        beats.back().take_items = {{"BRACERS", "magic_user 6"}, {"FLAIL", "cleric 5"}};
        beats.push_back({3, 14, 6, D::north, train_everyone(), true});
    }
    if (progress >= CurseProgress::first_bond)
    {
        beats.push_back({3, 13, 15, D::south, {}, true});
        beats.back().step = true;
        beats.push_back({4, 3, 13, D::north, {"stop:JOURNEY"}, true});
    }
    if (progress >= CurseProgress::hap)
    {
        beats.insert(beats.end(), story.begin() + 16, story.begin() + 31);
        // Hunt by looking from the real village arrival, replenishing spells
        // through its actual (possibly interrupted) camps.
        beats[27].x = 3;
        beats[27].y = 0;
        beats[27].repeat_in_place = true;
        beats[27].rest_for_spells = true;
        beats[27].take_items = {{"DROW LONG SWORD", "paladin 1"},
                                {"DROW LONG SWORD", "paladin 2"},
                                {"DROW LONG SWORD", "ranger 3"},
                                {"DROW MACE", "cleric 4"},
                                {"DROW MACE", "cleric 5"}};
        // Visit the barn in the village, not its disconnected geometry copy.
        beats[28].x = 3;
        beats[28].y = 13;
    }
    if (progress >= CurseProgress::second_bond)
    {
        // Train before clearing the barn: leaving Hap destroys Drow weapons.
        // At four patrols a final patrol can still supply replacements on return.
        auto barn = beats.back();
        beats.pop_back();
        barn.take_items = beats[27].take_items;
        barn.rest_for_spells = true;
        beats.push_back({50, 3, 0, D::north, {"YES", "stop:JOURNEY"}});
        beats.back().step = true;
        world({"stop:JOURNEY"}, true);
        world({"JOURNEY", "ESSEMBRA", "stop:JOURNEY"});
        world({"stop:JOURNEY"}, true);
        world({"JOURNEY", "STANDING STONE", "stop:JOURNEY"});
        world({"stop:JOURNEY"}, true);
        world({"JOURNEY", "ASHABENFORD", "stop:JOURNEY"});
        world({"stop:JOURNEY"}, true);
        world({"JOURNEY", "SHADOWDALE", "BY BOAT", "stop:JOURNEY"});
        world({"stop:JOURNEY"}, true);
        std::vector<std::string> hall{"ENTER CITY", "HALL"};
        const auto trainees = train_everyone();
        hall.insert(hall.end(), trainees.begin() + 1, trainees.end());
        hall.insert(hall.end(), {"LEAVE", "stop:JOURNEY"});
        world(std::move(hall));
        world({"stop:JOURNEY"}, true);
        // Retrace the known roads, then use the actual gate to the caves.
        beats.insert(beats.end(), story.begin() + 20, story.begin() + 29);
        beats.push_back(std::move(barn));
        beats.insert(beats.end(), story.begin() + 31, story.begin() + 33);
        // The five stories occupy separate parts of GEO51. Use their actual
        // stair events and the facing directions read from ECL5:51.
        beats.push_back({51, 3, 1, D::north, {"YES"}, true});
        beats.back().step = true;
        beats.push_back({51, 5, 4, D::west, {"YES", "NO"}, true});
        beats.back().step = true;
        beats.push_back({51, 14, 1, D::north, {"YES", "YES", "magic_user 6"}, true});
        beats.back().step = true;
        beats.push_back({51, 9, 12, D::west, {"YES"}, true});
        beats.back().step = true;
        // Prepare outside the courtyard's north door. Fire wards, enlarged
        // front-line fighters and Haste are actual memorized camp spells.
        beats.push_back({51, 4, 12, D::south, {}, true});
        RouteBeat prepare{51, 4, 12, D::south, {}, true};
        prepare.keep_preparation = true;
        prepare.rest_for_spells = true;
        prepare.ward_each_with = "resist_fire";
        prepare.memorize_first_of = {"enlarge", "enlarge", "enlarge", "haste", "bless", "prayer"};
        prepare.cast_after = {"bless"};
        beats.push_back(prepare);
        RouteBeat enlarge{51, 4, 12, D::south, {}};
        enlarge.keep_preparation = true;
        enlarge.ward_each_with = "enlarge";
        enlarge.ward_members = {"paladin 1", "paladin 2", "ranger 3"};
        enlarge.cure_first = true;
        enlarge.cure_to_full = true;
        enlarge.cast_after = {"prayer"};
        beats.push_back(enlarge);
        RouteBeat haste{51, 4, 12, D::south, {}};
        haste.keep_preparation = true;
        haste.cast_after = {"haste"};
        beats.push_back(haste);
        beats.push_back(story[33]);
        beats.back().x = 4;
        beats.back().y = 12;
        beats.back().facing = D::south;
        beats.back().step = true;
        beats.back().rest_first = false;
        beats.back().keep_preparation = true;
        beats.back().reloads_until_won = 1200;
        // Recover casualties through the actual village temple. A winning
        // fight can leave dead PCs; all six must be alive at this endpoint.
        beats.push_back(story[34]);
        // Recovery must not wait for dead clerics to memorize the boss's
        // Bless/Prayer plan. The surviving mage prepares these for travel.
        beats.back().memorize_first_of = {"magic_missile", "fireball"};
        beats.back().rest_first = true;
        beats.back().rest_for_spells = true;
        beats.back().step = true;
        // The obsolete troll-lair bracers fund Sune's actual resurrection
        // fees after Dracandros supplies the mage's stronger pair.
        beats.push_back({50, 2, 5, D::west, {"BUY SOMETHING", "sell:BRACERS@magic_user 6"}});
        beats.back().step = true;
        beats.push_back(story[35]);
        beats.back().step = true;
    }
    if (progress >= CurseProgress::third_bond)
    {
        RouteBeat depart{50, 3, 0, D::north, {"YES", "WILDERNESS", "stop:JOURNEY"}, true};
        depart.step = true;
        depart.rest_for_spells = true;
        beats.push_back(depart);
        world({"stop:JOURNEY"}, true);
        for (const char *name : {"paladin 1", "paladin 2", "ranger 3"})
        {
            beats.back().ready_types.emplace_back(36, name);
        }
        beats.back().ready_types.emplace_back(23, "cleric 4");
        beats.back().ready_types.emplace_back(12, "cleric 5");
        world({"JOURNEY", "ESSEMBRA", "stop:JOURNEY"});
        beats.back().reloads_until_won = 360;
        world({"stop:JOURNEY"}, true);
        world({"JOURNEY", "STANDING STONE", "stop:JOURNEY"});
        world({"stop:JOURNEY"}, true);
        world({"JOURNEY", "ASHABENFORD", "stop:JOURNEY"});
        world({"stop:JOURNEY"}, true);
        world({"JOURNEY", "SHADOWDALE", "BY BOAT", "stop:JOURNEY"});
        world({"stop:JOURNEY"}, true);
        // Spend the earned purse on the fourth-level arcane spells and
        // clerical advancement first; the remaining fighters train when funded.
        world({"ENTER CITY", "HALL", "magic_user 6", "magic_user 6", "cleric 4", "cleric 5",
               "paladin 1", "paladin 2", "ranger 3", "Leave", "LEAVE", "stop:JOURNEY"});
        world({"stop:JOURNEY"}, true);
        world({"JOURNEY", "ASHABENFORD", "BY BOAT", "stop:JOURNEY"});
        world({"stop:JOURNEY"}, true);
        world({"JOURNEY", "STANDING STONE", "stop:JOURNEY"});
        world({"stop:JOURNEY"}, true);
        world({"JOURNEY", "HILLSFAR", "stop:JOURNEY"});
        world({"stop:JOURNEY"}, true);
        world({"JOURNEY", "YULASH", "TRAIL", "stop:JOURNEY"});
        world({"ENTER CITY", "ASK PERMISSION", "PARLAY"}, true);
        const auto spies =
            std::ranges::find_if(story,
                                 [](const RouteBeat &beat)
                                 {
                                     return beat.map == 16 && beat.x == 0 && beat.y == 3;
                                 });
        const auto mogion =
            std::ranges::find_if(story,
                                 [](const RouteBeat &beat)
                                 {
                                     return beat.map == 17 && beat.x == 7 && beat.y == 1;
                                 });
        check(spies != story.end() && mogion != story.end(),
              "The existing Moander story is present");
        beats.insert(beats.end(), spies, mogion + 1);
        const auto mounds =
            std::ranges::find_if(beats,
                                 [](const RouteBeat &beat)
                                 {
                                     return beat.map == 16 && beat.x == 11 && beat.y == 8;
                                 });
        check(mounds != beats.end(), "The mound encounter is in the earned route");
        const auto mound_index = static_cast<std::size_t>(mounds - beats.begin());
        // Take the open streets around the houses, not the shorter path
        // through eight locked doors and their interior encounters.
        std::vector<RouteBeat> streets;
        for (const auto &[x, y] :
             std::array<std::pair<unsigned, unsigned>, 4>{{{3, 9}, {5, 15}, {13, 15}, {13, 8}}})
        {
            streets.push_back({16, x, y, D::north, {"Parley", "Nice", "NO", "Bash"}});
        }
        streets.front().rest_first = true;
        streets.front().rest_for_spells = true;
        streets.front().cure_first = true;
        streets.front().cure_to_full = true;
        beats[mound_index].rest_first = false;
        beats.insert(beats.begin() + static_cast<std::ptrdiff_t>(mound_index), streets.begin(),
                     streets.end());
        // The pit is entered by actually stepping off Yulash's north edge.
        for (auto &beat : beats)
        {
            if (beat.map == 17 && beat.x == 15 && beat.y == 11)
            {
                // The eastern stair is sealed off from the entrance. The
                // southern stair reaches the same lower temple by foot.
                beat.x = 7;
                beat.y = 14;
                beat.ready_types.emplace_back(79, "magic_user 6");
            }
            else if (beat.map == 17 && beat.x == 7 && beat.y == 1)
            {
                // Script 18 uses the eastern half of the shared geometry;
                // its altar is event 12 at (11,3), not the upper floor's.
                beat.x = 11;
                beat.y = 3;
                beat.step = true;
                beat.cure_first = true;
                beat.cure_to_full = true;
            }
            if (beat.map == 16 && beat.x == 11 && beat.y == 0)
            {
                beat.step = true;
            }
            if (beat.map == 16 && beat.x == 5 && beat.y == 2)
            {
                beat.cure_first = true;
                beat.cure_to_full = true;
                beat.answers = {"Parley", "Nice", "NO", "Bash"};
            }
        }
        auto boss = beats.back();
        beats.pop_back();
        // The corridor outside the altar has a guaranteed camp attack.
        // Prepare in the side room, then walk back to its north door.
        beats.push_back({17, 13, 6, D::north, {"WAIT", "NO", "Bash"}});
        beats.back().ready_types.emplace_back(79, "magic_user 6");
        RouteBeat prepare{17, 13, 6, D::north, {}, true};
        prepare.keep_preparation = true;
        prepare.rest_for_spells = true;
        prepare.ward_memorized = "enlarge";
        prepare.ward_members = {"paladin 1", "paladin 2"};
        prepare.memorize_first_of = {"enlarge", "enlarge", "haste", "fireball", "bless", "prayer"};
        beats.push_back(prepare);
        RouteBeat approach{17, 12, 4, D::north, {}};
        approach.keep_preparation = true;
        approach.cure_first = true;
        approach.cure_to_full = true;
        approach.cast_after = {"bless"};
        beats.push_back(approach);
        // Look costs ten minutes. Cast the short wards without another
        // look or walk between them and the single step into the altar.
        RouteBeat enlarge{17, 12, 4, D::north, {}};
        enlarge.waiting = true;
        enlarge.keep_preparation = true;
        enlarge.ward_each_with = "enlarge";
        enlarge.ward_members = {"paladin 1", "paladin 2"};
        enlarge.cure_first = true;
        enlarge.cure_to_full = true;
        enlarge.cast_after = {"prayer"};
        beats.push_back(enlarge);
        RouteBeat haste{17, 12, 4, D::north, {}};
        haste.waiting = true;
        haste.keep_preparation = true;
        haste.cast_after = {"haste"};
        beats.push_back(haste);
        boss.x = 12;
        boss.y = 4;
        boss.keep_preparation = true;
        // Search adds ten minutes even to this one step, outlasting the
        // short Bless, Prayer and Haste spells just cast at the altar door.
        boss.searching = false;
        beats.push_back(boss);
    }
    if (progress >= CurseProgress::moander_recovery)
    {
        // Dragonbait's real paladin ability stands a cleric up. The other
        // surviving PCs are unconscious after the pseudopods, not dead.
        RouteBeat hands{17, 12, 3, D::north, {}};
        hands.waiting = true;
        hands.ward_each_with = "lay_on_hands";
        hands.ward_members = {"cleric 4"};
        beats.push_back(hands);
        // The altar is a room too. Heal here before crossing hostile
        // corridors; a newly conscious cleric can prepare its cures here.
        RouteBeat room{17, 12, 3, D::north, {"NO", "WAIT", "Bash"}, true};
        room.rest_for_spells = true;
        room.arrive_only = true;
        room.cure_first = true;
        room.cure_to_full = true;
        room.ward_memorized = "cure_light_wounds";
        room.ward_members = {"cleric 4"};
        room.memorize_first_of = {"cure_light_wounds", "cure_serious_wounds", "magic_missile",
                                  "fireball"};
        beats.push_back(room);
        RouteBeat rest{17, 12, 3, D::north, {}, true};
        rest.rest_for_spells = true;
        rest.arrive_only = true;
        rest.cure_first = true;
        rest.cure_to_full = true;
        beats.push_back(rest);
        // The lower stair returns to the western upper half of map 17.
        beats.push_back({17, 15, 14, D::north, {"YES", "NO"}});
        beats.back().cure_first = true;
        beats.back().cure_to_full = true;
        beats.back().searching = false;
        // Rest in the upper stair room, then in each cleared room on the
        // way out. The slug and mound rooms lie across the only exit path.
        const auto recover_at = [&](unsigned x, unsigned y)
        {
            RouteBeat camp{17, x, y, D::north, {}, true};
            camp.rest_for_spells = true;
            camp.cure_first = true;
            camp.cure_to_full = true;
            camp.arrive_only = true;
            camp.ward_memorized = "cure_light_wounds";
            camp.ward_members = {"cleric 4"};
            camp.memorize_first_of = {"cure_light_wounds", "cure_serious_wounds", "magic_missile",
                                      "fireball"};
            beats.push_back(camp);
        };
        const auto prepare_at = [&](unsigned x, unsigned y)
        {
            RouteBeat prepare{17, x, y, D::north, {}, true};
            prepare.rest_for_spells = true;
            prepare.arrive_only = true;
            prepare.ward_memorized = "enlarge";
            prepare.ward_members = {"paladin 1", "Dragonbait"};
            prepare.memorize_first_of = {"enlarge",  "enlarge", "haste",
                                         "fireball", "bless",   "prayer"};
            prepare.cast_after = {"bless"};
            beats.push_back(prepare);
            RouteBeat enlarge{17, x, y, D::north, {}};
            enlarge.waiting = true;
            enlarge.ward_each_with = "enlarge";
            enlarge.ward_members = {"paladin 1", "Dragonbait"};
            enlarge.cast_after = {"prayer"};
            beats.push_back(enlarge);
            RouteBeat haste{17, x, y, D::north, {}};
            haste.waiting = true;
            haste.cast_after = {"haste"};
            beats.push_back(haste);
        };
        recover_at(7, 14);
        prepare_at(7, 14);
        // Pause on each corridor square so real remaining cures can be
        // cast after a door's wandering encounter, before the room's fight.
        for (unsigned y : {13U, 12U, 11U})
        {
            RouteBeat corridor{17, 7, y, D::north, {"NO", "WAIT", "Bash"}};
            corridor.cure_first = true;
            corridor.cure_to_full = true;
            corridor.searching = false;
            corridor.arrive_only = true;
            corridor.reloads_until_won = 360;
            beats.push_back(corridor);
        }
        for (const auto &[x, y] :
             std::array<std::pair<unsigned, unsigned>, 3>{{{6, 11}, {2, 13}, {0, 13}}})
        {
            RouteBeat fight{17, x, y, D::north, {"NO", "WAIT", "Bash"}};
            fight.cure_first = true;
            fight.cure_to_full = true;
            fight.ready_types.emplace_back(79, "magic_user 6");
            fight.reloads_until_won = 360;
            fight.searching = false;
            fight.arrive_only = true;
            beats.push_back(fight);
            recover_at(x, y);
            prepare_at(x, y);
        }
        RouteBeat exit_guard{17, 0, 12, D::west, {"NO", "WAIT", "Bash"}};
        exit_guard.cure_first = true;
        exit_guard.cure_to_full = true;
        exit_guard.ready_types.emplace_back(79, "magic_user 6");
        exit_guard.reloads_until_won = 360;
        exit_guard.searching = false;
        exit_guard.arrive_only = true;
        exit_guard.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x4C2D, 255};
        beats.push_back(exit_guard);
        beats.push_back({17, 0, 11, D::west, {"stop:JOURNEY"}});
        beats.back().step = true;
        beats.back().searching = false;
        beats.back().expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x4C5B, 255};
        // Sell Dracandros's ring through the real shop to pay for the
        // fallen PCs' resurrection and the next eligible hall training.
        world({"JOURNEY", "VOONLAR", "TRAIL", "ENTER CITY", "STORE", "sell:RING@magic_user 6",
               "TEMPLE", "raise:dead", "cure:unconscious", "Leave", "stop:INN"});
        beats.back().ward_memorized = "cure_light_wounds";
        beats.back().ward_members = {"cleric 4"};
        beats.back().memorize_first_of = {"cure_light_wounds", "cure_serious_wounds",
                                          "magic_missile", "fireball"};
        world({"HALL", "magic_user 6", "cleric 4", "cleric 5", "ranger 3", "paladin 1", "paladin 2",
               "Leave", "stop:INN"});
        // Save from idle exploration rather than a pending city menu.
        // The invasion now closes Yulash: its original entry redirects to
        // the visited upper Pit entrance instead.
        world({"LEAVE", "JOURNEY", "YULASH", "TRAIL", "stop:JOURNEY"});
        world({"ENTER CITY", "ASK PERMISSION", "PARLAY"});
    }
    if (progress >= CurseProgress::fourth_bond)
    {
        // The recovery save stands at the Pit's back entrance. Leave by
        // the same physical doorway, then recover in wilderness camp.
        RouteBeat depart{17, 0, 11, D::west, {"stop:JOURNEY"}};
        depart.step = true;
        depart.searching = false;
        depart.cure_first = true;
        depart.cure_to_full = true;
        beats.push_back(depart);
        world({"stop:JOURNEY"}, true);
        world({"JOURNEY", "ZHENTIL KEEP", "TRAIL", "stop:JOURNEY"});
        world({"ENTER CITY"});
        const std::vector<std::string> cures{"cure_light_wounds", "cure_serious_wounds",
                                             "magic_missile", "fireball"};
        const auto prepare_at = [&](unsigned map, unsigned x, unsigned y)
        {
            RouteBeat recover{map, x, y, D::north, {}, true};
            recover.arrive_only = true;
            recover.rest_for_spells = true;
            recover.cure_first = true;
            recover.cure_to_full = true;
            recover.ready_types.emplace_back(8, "magic_user 6");
            recover.ward_memorized = "cure_light_wounds";
            recover.ward_members = {"cleric 4"};
            recover.memorize_first_of = cures;
            beats.push_back(recover);
            auto prepare = recover;
            prepare.ward_memorized = "enlarge";
            prepare.ward_members = {"paladin 1", "paladin 2", "ranger 3"};
            prepare.memorize_first_of = {"enlarge",  "enlarge", "enlarge", "haste",
                                         "fireball", "bless",   "prayer"};
            prepare.cast_after = {"bless"};
            beats.push_back(prepare);
            RouteBeat enlarge{map, x, y, D::north, {}};
            enlarge.waiting = true;
            enlarge.ward_each_with = "enlarge";
            enlarge.ward_members = prepare.ward_members;
            enlarge.cast_after = {"prayer"};
            beats.push_back(enlarge);
            RouteBeat haste{map, x, y, D::north, {}};
            haste.waiting = true;
            haste.cast_after = {"haste"};
            beats.push_back(haste);
        };
        // Olive's secret entrance, Dimswart and the hooded woman's actual
        // dimension door. These events own the transfers between scripts.
        for (std::size_t index = 61; index <= 63; ++index)
        {
            beats.push_back(story[index]);
            beats.back().searching = false;
            beats.back().reloads_until_won = 360;
            beats.back().cure_first = true;
            beats.back().cure_to_full = true;
            if (index == 62)
            {
                // Olive's scripted entry is (7,14). The reachable cell is
                // just south of it; the assisted route used a disconnected
                // instance of the same event in the western temple rooms.
                beats.back().x = 7;
                beats.back().y = 15;
                beats.back().facing = D::south;
                beats.back().arrive_only = true;
                prepare_at(32, 7, 15);
            }
            if (index == 63)
            {
                beats.back().x = 4;
                beats.back().y = 10;
                beats.back().answers = {"YES"};
                beats.back().arrive_only = true;
            }
        }
        const auto approach_room = [&](unsigned x, unsigned y)
        {
            RouteBeat approach{33, x, y, D::north, {"NO", "WAIT", "Bash"}};
            approach.searching = false;
            approach.arrive_only = true;
            approach.cure_first = true;
            approach.cure_to_full = true;
            approach.reloads_until_won = 360;
            approach.ready_types.emplace_back(8, "magic_user 6");
            beats.push_back(approach);
            prepare_at(33, x, y);
        };
        // The altar's camp interruption is guaranteed. Leave its bounds,
        // clear the ogres, then approach the lair from the north. The old
        // rest target (4,12) lies beyond Dexam and cannot prepare that fight.
        approach_room(7, 5);
        approach_room(9, 5);
        approach_room(2, 10);
        RouteBeat dexam{33, 2, 11, D::south, {}};
        dexam.arrive_only = true;
        dexam.searching = false;
        dexam.reloads_until_won = 1200;
        dexam.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x4C5A, 1};
        beats.push_back(dexam);
        approach_room(4, 12);
        beats.push_back(story[66]);
        beats.back().searching = false;
        beats.back().reloads_until_won = 360;
        beats.back().cure_first = true;
        beats.back().cure_to_full = true;
        // The exit dismisses Dimswart and Olive and removes the bond.
        world({"stop:JOURNEY"}, true);
        beats.back().ward_memorized = "cure_light_wounds";
        beats.back().ward_members = {"cleric 4"};
        beats.back().memorize_first_of = cures;
        world({"JOURNEY", "YULASH", "TRAIL", "stop:JOURNEY"});
        world({"ENTER CITY"});
    }
    if (progress >= CurseProgress::ending)
    {
        RouteBeat depart{17, 0, 11, D::west, {"stop:JOURNEY"}};
        depart.step = true;
        depart.searching = false;
        beats.push_back(depart);
        world({"JOURNEY", "VOONLAR", "TRAIL", "ENTER CITY", "STORE", "identify:WAND@magic_user 6",
               "stop:INN"});
        world({"HALL", "magic_user 6", "paladin 1", "paladin 2", "Leave", "stop:INN"});
        beats.back().ready_names.emplace_back("Wand of Fireballs", "magic_user 6");
        world({"LEAVE", "JOURNEY", "YULASH", "TRAIL", "stop:JOURNEY"});
        world({"stop:JOURNEY"}, true);
        beats.back().ward_memorized = "cure_light_wounds";
        beats.back().ward_members = {"cleric 4"};
        beats.back().memorize_first_of = {"cure_light_wounds", "cure_serious_wounds",
                                          "magic_missile", "fireball"};
        for (std::size_t index = 68; index <= 72; ++index)
        {
            beats.push_back(story[index]);
            beats.back().searching = false;
            beats.back().reloads_until_won = 600;
            beats.back().cure_first = true;
            beats.back().cure_to_full = true;
        }
        // The rakshasa parley makes this room a safe preparation camp.
        // No camp rests inside the temple.
        RouteBeat recover{66, 9, 6, D::north, {}, true};
        recover.rest_for_spells = true;
        recover.cure_first = true;
        recover.cure_to_full = true;
        recover.arrive_only = true;
        recover.ward_memorized = "cure_light_wounds";
        recover.ward_members = {"cleric 4"};
        recover.memorize_first_of = {"cure_light_wounds", "cure_serious_wounds", "magic_missile",
                                     "fireball"};
        beats.push_back(recover);
        auto prepare = recover;
        prepare.ward_memorized = "enlarge";
        prepare.ward_members = {"paladin 1", "paladin 2", "ranger 3"};
        prepare.memorize_first_of = {"enlarge",  "enlarge", "enlarge", "haste",
                                     "fireball", "bless",   "prayer"};
        prepare.cast_after = {"bless"};
        beats.push_back(prepare);
        RouteBeat enlarge{66, 9, 6, D::north, {}};
        enlarge.waiting = true;
        enlarge.ward_each_with = "enlarge";
        enlarge.ward_members = prepare.ward_members;
        enlarge.cast_after = {"prayer"};
        beats.push_back(enlarge);
        RouteBeat haste{66, 9, 6, D::north, {}};
        haste.waiting = true;
        haste.cast_after = {"haste"};
        beats.push_back(haste);
        for (std::size_t index = 73; index <= 76; ++index)
        {
            beats.push_back(story[index]);
            beats.back().rest_first = false;
            beats.back().searching = false;
            beats.back().arrive_only = true;
            beats.back().cure_first = true;
            beats.back().cure_to_full = true;
            beats.back().reloads_until_won = 2400;
            if (index == 75)
            {
                // Step onto the stairs from the hall; their original warp
                // stays in the same map/script, so it ends this beat.
                beats.back().y = 8;
                beats.back().step = true;
                beats.back().arrive_only = false;
            }
        }
    }
    for (auto &beat : beats)
    {
        if (beat.map == 3)
        {
            beat.take_items = {{"BRACERS", "magic_user 6"}, {"FLAIL", "cleric 5"}};
        }
    }
    return beats;
}

// Replan after each actual step: encounters, doors and scripts may interrupt it.
// This route never places the party on a target square; scripts own transfers.
void walk_earned_campaign(opengold::por::RolfTourSession &session, unsigned tx, unsigned ty,
                          const std::function<void()> &settle, bool prefer_open = false)
{
    using C = opengold::por::ExplorationCommand;
    constexpr std::array<int, 4> dx{0, 1, 0, -1}, dy{-1, 0, 1, 0};
    const auto area = session.snapshot().area_id;
    const auto script = session.snapshot().script_id;
    for (unsigned moves = 0; moves < 400; ++moves)
    {
        const auto before = session.snapshot();
        if (before.script_id != script)
        {
            return;
        }
        check(before.area_id == area, "Earned walking stays in its current area");
        if (before.pose.x == tx && before.pose.y == ty)
        {
            return;
        }
        std::array<int, 256> previous;
        std::array<unsigned, 256> direction{};
        const int origin = before.pose.y * 16 + before.pose.x;
        // Yulash's open streets are safer than the shorter path through
        // locked houses. Fall back to locks only when the destination needs one.
        for (unsigned door_limit = prefer_open ? 1 : 2; door_limit <= 2; ++door_limit)
        {
            previous.fill(-1);
            std::deque<int> pending;
            previous[origin] = origin;
            pending.push_back(origin);
            while (!pending.empty())
            {
                const int at = pending.front();
                pending.pop_front();
                for (unsigned d = 0; d < 4; ++d)
                {
                    int x = at % 16 + dx[d], y = at / 16 + dy[d];
                    // ECL2:3 folds the sewer corridors across its north/south
                    // edges. These are actual scripted steps, not map wrapping.
                    if (area == 3 && y == 16 && (x == 2 || x == 5 || x == 9))
                    {
                        x = x == 2 ? 9 : x == 5 ? 11 : 15;
                        y = 0;
                    }
                    else if (area == 3 && y == -1 && (x == 9 || x == 11 || x == 15))
                    {
                        x = x == 9 ? 2 : x - 6;
                        y = 15;
                    }
                    if (x < 0 || y < 0 || x >= 16 || y >= 16)
                    {
                        continue;
                    }
                    const auto &edge = session.map().at(at % 16, at / 16);
                    if (edge.doors[d] > door_limit || (edge.walls[d] && !edge.doors[d]))
                    {
                        continue;
                    }
                    const int next = y * 16 + x;
                    if (previous[next] < 0)
                    {
                        previous[next] = at;
                        direction[next] = d;
                        pending.push_back(next);
                    }
                }
            }
            if (previous[ty * 16 + tx] >= 0)
            {
                break;
            }
        }
        int next = ty * 16 + tx;
        check(previous[next] >= 0, "The earned destination has a walkable route");
        while (previous[next] != origin)
        {
            next = previous[next];
        }
        const unsigned facing = direction[next];
        for (unsigned turns = 0; unsigned(session.snapshot().pose.facing) != facing && turns < 4;
             ++turns)
        {
            check(session.explore(C::turn_right), "The earned party can turn");
            settle();
        }
        check(session.explore(C::forward), "The earned party can attempt its next step");
        settle();
    }
    throw std::runtime_error("Earned walking did not arrive");
}

bool walk_census(GoldBoxGame game, bool route, CurseProgress progress = CurseProgress::none,
                 SecretProgress secret_progress = SecretProgress::none)
{
    const bool sewers = progress != CurseProgress::none;
    const bool earned_secret = secret_progress != SecretProgress::none;
    const bool earned = sewers || earned_secret;
    const bool first_bond = progress >= CurseProgress::first_bond;
    const bool hap = progress >= CurseProgress::hap;
    const bool second_bond = progress >= CurseProgress::second_bond;
    const bool third_bond = progress >= CurseProgress::third_bond;
    const bool moander_recovery = progress >= CurseProgress::moander_recovery;
    const bool fourth_bond = progress >= CurseProgress::fourth_bond;
    const bool earned_ending = progress >= CurseProgress::ending;
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
    // Spells the next beat casts outside combat (RouteBeat::cast_after),
    // which each caster takes first.
    std::vector<std::string> wanted_spells;
    // The spell the next beat casts on each member (RouteBeat::ward_each_with),
    // of which the casters memorize a copy for each member between them.
    std::string ward_spell;
    std::vector<std::string> ward_targets;
    bool keeps_preparation = false;
    const auto ward_target_count = [&]
    {
        return static_cast<unsigned>(std::ranges::count_if(
            party->state().slots,
            [&](opengold::MemberId id)
            {
                return id && !party->member(id).vitals.dead &&
                       (ward_targets.empty() ||
                        std::ranges::find(ward_targets, party->member(id).character.sheet().name) !=
                            ward_targets.end());
            }));
    };
    const auto spell_named = [](std::string_view id, std::string_view name)
    {
        id.remove_prefix(id.find_last_of(':') + 1);
        return id.substr(0, id.find('#')) == name;
    };
    // Pools of Darkness's members turned to stone.
    const auto statues = [&]
    {
        std::vector<opengold::MemberId> members;
        for (const auto id : party->state().slots)
        {
            if (id && !party->member(id).vitals.dead &&
                party->member(id).vitals.description == "Turned to stone.")
            {
                members.push_back(id);
            }
        }
        return members;
    };
    // A magic-user with Stone to Flesh memorized brings a statue back, as a
    // player would in camp (magic.pas, SpellStoneToFlesh).
    const auto unstone_the_statues = [&]
    {
        if (game != GoldBoxGame::pools_of_darkness)
        {
            return;
        }
        for (const auto statue : statues())
        {
            for (const auto caster : party->state().slots)
            {
                if (!caster || party->member(caster).vitals.dead)
                {
                    continue;
                }
                const auto actions = party->camp_actions(caster);
                const auto spell =
                    std::ranges::find_if(actions,
                                         [](const opengold::rules::CampAction &action)
                                         {
                                             return action.id.ends_with("stone_to_flesh");
                                         });
                if (spell != actions.end())
                {
                    std::cout << "  " << party->member(caster).character.sheet().name << " casts "
                              << spell->id << " on " << party->member(statue).character.sheet().name
                              << "\n";
                    party->use_camp_action(caster, statue, spell->id);
                    break;
                }
            }
        }
    };
    // Each day's spells: the strongest a caster knows at each of its levels.
    const auto memorize = [&]
    {
        // Pools of Darkness's spells of levels 6 to 9 come first at theirs,
        // which only its route's casters memorize.
        constexpr std::array<std::string_view, 27> favorites{"meteor_swarm",
                                                             "power_word_kill",
                                                             "mass_charm",
                                                             "power_word_blind",
                                                             "delayed_blast_fireball",
                                                             "power_word_stun",
                                                             "death_spell",
                                                             "disintegrate",
                                                             "heal",
                                                             "blade_barrier",
                                                             "destruction",
                                                             "resurrection",
                                                             "cone_of_cold",
                                                             "ice_storm",
                                                             "confusion",
                                                             "hold_monster",
                                                             "fireball",
                                                             "stinking_cloud",
                                                             "magic_missile",
                                                             "sleep",
                                                             "lightning_bolt",
                                                             "flame_strike",
                                                             "hold_person",
                                                             "cure_serious_wounds",
                                                             "cure_light_wounds",
                                                             "prayer",
                                                             "bless"};
        // The route's Pools of Darkness and Secret casters take spells of
        // every level they have slots of (Secret's to the seventh); the
        // other routes, levels 1 to 5.
        const int highest_level = !route                                             ? 5
                                  : game == GoldBoxGame::pools_of_darkness           ? 9
                                  : game == GoldBoxGame::secret_of_the_silver_blades ? 7
                                                                                     : 5;
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
        // Members with levels drained and not given back (Pools of
        // Darkness's Restoration gives one back a cast).
        const auto drained = [&]
        {
            std::vector<opengold::MemberId> members;
            for (const auto id : party->state().slots)
            {
                if (!id || party->member(id).vitals.dead)
                {
                    continue;
                }
                const auto &sheet = party->member(id).character.sheet();
                if (sheet.original && sheet.original->drained_levels > 0)
                {
                    members.push_back(id);
                }
            }
            return members;
        };
        const bool anyone_drained = game == GoldBoxGame::pools_of_darkness && !drained().empty();
        // A cleric with Restoration memorized gives a drained member a level
        // back, as a player would in camp, each cast one level.
        const auto restore_the_drained = [&]
        {
            if (game != GoldBoxGame::pools_of_darkness)
            {
                return;
            }
            for (const auto lost : drained())
            {
                for (const auto cleric : party->state().slots)
                {
                    if (!cleric || party->member(cleric).vitals.dead)
                    {
                        continue;
                    }
                    const auto actions = party->camp_actions(cleric);
                    const auto restore = std::ranges::find_if(
                        actions,
                        [](const opengold::rules::CampAction &action)
                        {
                            return action.id.find("restoration") != std::string::npos;
                        });
                    if (restore == actions.end())
                    {
                        continue;
                    }
                    try
                    {
                        party->use_camp_action(cleric, lost, restore->id);
                        break;
                    }
                    catch (const std::exception &)
                    {
                    }
                }
            }
        };
        // A cleric with Raise Dead (or Resurrection) memorized raises the dead, as a player would
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
                            return action.id.find("raise_dead") != std::string::npos ||
                                   action.id.find("resurrection") != std::string::npos;
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
        // A member knocked out (0 Hit Points) heals nothing as the party
        // rests (the original's resting heals only those okay): in Pools of
        // Darkness's route a member with a cure memorized casts it on one, as
        // a player would in camp.
        const auto wake_the_fallen = [&]
        {
            if (game != GoldBoxGame::pools_of_darkness)
            {
                return;
            }
            for (const auto fallen : party->state().slots)
            {
                if (!fallen || party->member(fallen).vitals.dead ||
                    party->member(fallen).vitals.hit_points > 0)
                {
                    continue;
                }
                for (const auto healer : party->state().slots)
                {
                    if (!healer || party->member(healer).vitals.dead ||
                        party->member(healer).vitals.hit_points <= 0)
                    {
                        continue;
                    }
                    const auto actions = party->camp_actions(healer);
                    const auto cure = std::ranges::find_if(
                        actions,
                        [](const opengold::rules::CampAction &action)
                        {
                            return action.id.find("cure_") != std::string::npos &&
                                   action.id.find("wounds") != std::string::npos;
                        });
                    if (cure == actions.end())
                    {
                        continue;
                    }
                    try
                    {
                        party->use_camp_action(healer, fallen, cure->id);
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
            restore_the_drained();
            wake_the_fallen();
            unstone_the_statues();
            return;
        }
        // Those the last Long Rest left choosing; choose_spells takes each off.
        const auto choosing = std::vector(party->state().spell_rest->members);
        // Each caster who can memorize the ward takes a copy for every
        // member, as far as its slots go, so that between them there is
        // one for each whatever their slots.
        const auto ward_copies = ward_target_count();
        for (const auto id : choosing)
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
                for (int level = 1; level <= highest_level; ++level)
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
                        // With someone dead, a cleric takes Raise Dead (or Resurrection)
                        // first; Pools of Darkness's route keeps them ready always, for
                        // its maps that let no camp rest (cure_the_hurt).
                        const bool keeps_raising =
                            anyone_dead || (game == GoldBoxGame::pools_of_darkness && route);
                        if (keeps_raising && (id.find("raise_dead") != std::string::npos ||
                                              id.find("resurrection") != std::string::npos))
                        {
                            return -1;
                        }
                        // With someone drained, Restoration first too.
                        if (anyone_drained && id.find("restoration") != std::string::npos)
                        {
                            return -1;
                        }
                        // With someone turned to stone, Stone to Flesh first.
                        if (!statues().empty() && game == GoldBoxGame::pools_of_darkness &&
                            id.find("stone_to_flesh") != std::string::npos)
                        {
                            return -1;
                        }
                        const auto copy_at = id.find('#');
                        const auto copy =
                            copy_at == std::string::npos
                                ? 1U
                                : static_cast<unsigned>(std::stoul(id.substr(copy_at + 1)));
                        const auto wanted_copies =
                            std::ranges::count_if(wanted_spells,
                                                  [&](const std::string &wanted)
                                                  {
                                                      return spell_named(id, wanted);
                                                  });
                        if ((!keeps_preparation && wanted_copies > 0) ||
                            copy <= static_cast<unsigned>(wanted_copies))
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
                    const auto level_slots =
                        static_cast<std::size_t>(slots[static_cast<std::size_t>(level - 1)]);
                    // The ward's copies ("#2", "#3" after the first) first,
                    // then the other spells by rank.
                    std::size_t taken = 0;
                    // The crevasses' wizards can hold the whole party. Keep
                    // one dispel per cleric instead of filling every third-
                    // level slot with Prayer, so a free cleric can answer it.
                    if (route && game == GoldBoxGame::secret_of_the_silver_blades &&
                        prefix == "cleric:" && level == 3 && level_slots > 0)
                    {
                        const auto dispel = std::ranges::find(candidates, "cleric:dispel_magic");
                        if (dispel != candidates.end())
                        {
                            prepared.push_back(*dispel);
                            candidates.erase(dispel);
                            ++taken;
                        }
                    }
                    std::vector<std::string> others;
                    for (const auto &candidate : candidates)
                    {
                        if (ward_spell.empty() || !spell_named(candidate, ward_spell))
                        {
                            others.push_back(candidate);
                        }
                        else if (taken < ward_copies && taken < level_slots)
                        {
                            prepared.push_back(candidate);
                            ++taken;
                        }
                    }
                    for (std::size_t n = 0; taken < level_slots && n < others.size(); ++n, ++taken)
                    {
                        prepared.push_back(others[n]);
                    }
                }
            }
            party->choose_spells(id, {{}, prepared});
        }
        raise_the_dead();
        restore_the_drained();
        wake_the_fallen();
        unstone_the_statues();
    };
    const auto item_definitions = game == GoldBoxGame::pools_of_darkness
                                      ? opengold::load_item_definitions(folder, game)
                                      : std::map<unsigned, opengold::por::Equipment>{};
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
            opengold::test::walk_original_creation(creator, learned_first);
            const auto id = party->add_pc(creator.create_character());
            if (earned)
            {
                party->add_coins(opengold::Coin::gold, creator.starting_gold());
                party->add_coins(opengold::Coin::platinum, creator.starting_platinum());
            }
            // The route's player raises each member's scores and Hit Points
            // to their highest on Modify Character, as a player makes a party
            // meant to finish the game: Pools of Darkness's magic-user's
            // eighth- and ninth-level spells need Intelligence 16 and 18
            // (TrimHighSpellSlots).
            if (route)
            {
                opengold::test::modify_to_the_highest(*party, id);
            }
            // OPENGOLD_FIGHT_GEAR prints each member's scores too.
            if (std::getenv("OPENGOLD_FIGHT_GEAR"))
            {
                const auto &sheet = party->member(id).character.sheet();
                std::cout << "  scores: " << sheet.name;
                for (const auto ability : opengold::rules::all_abilities)
                {
                    std::cout << " " << sheet.scores[ability];
                }
                std::cout << " hp " << sheet.hit_points << "\n";
            }
            // Pools of Darkness's route party carries the game's own starting
            // equipment, as a party made in the original does.
            if (route && game == GoldBoxGame::pools_of_darkness)
            {
                opengold::outfit_original_character(*party, id, item_definitions);
                continue;
            }
            // A long sword, plate mail and a shield of the game's own item
            // types; a cleric's mace; a magic-user's dagger. Secret, Pools of
            // Darkness and Treasures number their types as Pool of Radiance
            // does, not as Curse.
            const bool secret = game == GoldBoxGame::secret_of_the_silver_blades ||
                                game == GoldBoxGame::pools_of_darkness ||
                                game == GoldBoxGame::treasures_of_the_savage_frontier;
            const std::uint8_t sword = secret ? 18 : 36, plate = secret ? 36 : 58,
                               shield = secret ? 37 : 59, mace = secret ? 8 : 23,
                               dagger = secret ? 4 : 8;
            const std::vector<std::uint8_t> gear_types =
                klass == "magic_user" ? std::vector<std::uint8_t>{dagger}
                : klass == "cleric"   ? std::vector<std::uint8_t>{mace, plate, shield}
                                      : std::vector<std::uint8_t>{sword, plate, shield};
            for (const std::uint8_t type : earned ? std::vector<std::uint8_t>{} : gear_types)
            {
                std::array<std::uint8_t, 63> raw{};
                raw[46] = type;
                // The route's party carries the magic arms of one brought from
                // Pool of Radiance: weapons +2, armor and shields +1 (the
                // record's magic bonus, byte 50). Curse's party starts with
                // what its 300 platinum buys and takes the magic arms its
                // fights leave (RouteBeat::take_items).
                if (route && game != GoldBoxGame::curse_of_the_azure_bonds)
                {
                    raw[50] = type == plate || type == shield ? 1 : 2;
                }
                opengold::por::Equipment gear;
                gear.stored = opengold::por::decode_items(raw)->front();
                party->purchase(id, gear);
                party->equip(id, party->member(id).character.inventory().items().back().id);
            }
        }
        if (!earned)
        {
            party->add_coins(opengold::Coin::gold, 5000);
        }
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
    // A Treasures player's choice against casters, as its cluebook bids (hit
    // every enemy with a spell or a missile each round; one hit while it
    // casts loses its spell): the magic-user's Fireball, cast or from a wand,
    // where its burst takes the most enemies, sorceresses and mages counted
    // thrice, and spares every member and ally; else a blow or a shot at an
    // enemy who has begun casting, then at a caster. Secret's player plays
    // its crevasses' and castle's casters so (its cluebook: "Always be sure
    // to injure enemy spellcasters every round to prevent them from
    // casting"): the Black Circle's wizards and necromancers, the Legion's
    // wizards and the Banite priests.
    const auto treasures_turn = [game, &party](const opengold::rules::CombatSession &session)
        -> std::optional<opengold::rules::Command>
    {
        constexpr unsigned fireball = 47;
        constexpr int burst_reach = 7; // doubled steps: Fireball's 3 squares
        const auto state = session.snapshot();
        const auto commands = session.legal_commands();
        const auto actor =
            std::ranges::find(state.combatants, state.actor, &opengold::rules::CombatantView::id);
        int caster_level = 0;
        for (const auto id : party->state().slots)
        {
            if (id && actor != state.combatants.end() &&
                party->member(id).character.sheet().name == actor->name)
            {
                const auto &levels = party->member(id).character.sheet().original->class_levels;
                caster_level = std::max(
                    opengold::original::level_in(levels, opengold::original::ClassSlot::magic_user),
                    opengold::original::level_in(levels, opengold::original::ClassSlot::ranger) -
                        8);
            }
        }
        const auto *spell = opengold::original::spell(fireball, game);
        const auto reachable = [&](opengold::rules::Cell aim, bool item)
        {
            if (actor == state.combatants.end())
            {
                return false;
            }
            const int dx = std::abs(aim.x - actor->cell.x);
            const int dy = std::abs(aim.y - actor->cell.y);
            const int range =
                spell->range_fixed + spell->range_per_level * (item ? 6 : caster_level);
            return 2 * std::max(dx, dy) + std::min(dx, dy) <= 2 * range &&
                   opengold::rules::has_line_of_sight(state.battlefield, actor->cell, aim);
        };
        if (std::getenv("OPENGOLD_DEBUG_TURN"))
        {
            std::cout << "  turn of " << state.actor << ":";
            for (const auto &c : commands)
            {
                std::cout << " " << c.verb << ":" << c.item;
            }
            std::cout << "\n";
        }
        const auto caster = [game](const opengold::rules::CombatantView &unit)
        {
            if (game == GoldBoxGame::secret_of_the_silver_blades)
            {
                return unit.name.find("WIZARD") != std::string::npos ||
                       unit.name.find("NECROMANCER") != std::string::npos ||
                       unit.name.find("PRIEST") != std::string::npos;
            }
            return unit.name.find("SORC") != std::string::npos ||
                   unit.name.find("MAGE") != std::string::npos;
        };
        // Begun casting: its latest word in the log is that it begins.
        const auto casting = [&](const opengold::rules::CombatantView &unit)
        {
            const auto own = unit.name + " ";
            const auto latest = std::find_if(state.log_entries.rbegin(), state.log_entries.rend(),
                                             [&](const opengold::rules::LogEntry &entry)
                                             {
                                                 return entry.english.starts_with(own);
                                             });
            return latest != state.log_entries.rend() &&
                   latest->english.find(" begins casting ") != std::string::npos;
        };
        const auto burst_worth = [&](opengold::rules::Cell aim)
        {
            int worth = 0;
            for (const auto &unit : state.combatants)
            {
                const int dx = std::abs(unit.cell.x - aim.x), dy = std::abs(unit.cell.y - aim.y);
                if (!unit.conscious || 2 * std::max(dx, dy) + std::min(dx, dy) > burst_reach)
                {
                    continue;
                }
                worth += unit.side == opengold::rules::Side::party ? -100 : caster(unit) ? 3 : 1;
            }
            return worth;
        };
        const auto spell_of = [](const opengold::rules::Command &command)
        {
            return command.verb == "use" ? command.item % 256 : command.item;
        };
        const opengold::rules::Command *aim = nullptr;
        for (const auto &command : commands)
        {
            if (command.verb == "aim" && command.item == fireball &&
                (!aim || burst_worth(command.destination) > burst_worth(aim->destination)))
            {
                aim = &command;
            }
        }
        if (aim && burst_worth(aim->destination) > 0)
        {
            return *aim;
        }
        if (aim)
        {
            const auto abort = std::ranges::find(commands, std::string_view("abort"),
                                                 &opengold::rules::Command::verb);
            if (abort != commands.end())
            {
                return *abort;
            }
        }
        // Fireball, where a square of the field would take three enemies'
        // worth and no friend.
        const auto &field = state.battlefield;
        const auto offered = [&](std::string_view verb)
        {
            return std::ranges::any_of(commands,
                                       [&](const opengold::rules::Command &command)
                                       {
                                           return command.verb == verb &&
                                                  spell_of(command) == fireball;
                                       });
        };
        const bool may_cast = offered("cast"), may_use = offered("use");
        bool worth_a_fireball = false, worth_an_item = false;
        const auto searching = [&]
        {
            return (may_cast && !worth_a_fireball) || (may_use && !worth_an_item);
        };
        for (int y = 0; y < field.height && searching(); ++y)
        {
            for (int x = 0; x < field.width && searching(); ++x)
            {
                // A tempting burst behind a wall or beyond range wastes the
                // caster's spell and turn when its aim has to be aborted.
                const bool cast_here = may_cast && !worth_a_fireball && reachable({x, y}, false);
                const bool use_here = may_use && !worth_an_item && reachable({x, y}, true);
                if ((cast_here || use_here) && burst_worth({x, y}) >= 3)
                {
                    worth_a_fireball = worth_a_fireball || cast_here;
                    worth_an_item = worth_an_item || use_here;
                }
            }
        }
        for (const auto &command : commands)
        {
            if (((command.verb == "cast" && worth_a_fireball) ||
                 (command.verb == "use" && worth_an_item)) &&
                spell_of(command) == fireball)
            {
                return command;
            }
        }
        const auto unit_of = [&](opengold::rules::EntityId id)
        {
            return std::ranges::find(state.combatants, id, &opengold::rules::CombatantView::id);
        };
        const opengold::rules::Command *blow = nullptr;
        const auto rank = [&](const opengold::rules::Command &command)
        {
            const auto target = unit_of(command.target);
            return casting(*target) ? 0 : caster(*target) ? 1 : 2;
        };
        for (const auto &command : commands)
        {
            if ((command.verb == "melee" || command.verb == "ranged") &&
                unit_of(command.target) != state.combatants.end() && rank(command) < 2 &&
                (!blow || rank(command) < rank(*blow)))
            {
                blow = &command;
            }
        }
        return blow ? std::optional{*blow} : std::nullopt;
    };
    // A Gateway player's choice where shambling mounds fight, as its cluebook
    // bids at the foot of the Star Mounts: the Wand of Ice Storm (or the
    // spell) where its burst takes the most mounds, which still take half,
    // and Fireball where its burst takes the most giant snakes, each sparing
    // every member and ally. The caster steps nearer the mounds, out of their
    // reach, to bring them into range, and where no burst spares the party
    // casts Magic Missile at a mound or guards; the others let the enemy come
    // to them for the first rounds. Quick and the player's other choices for
    // the rest.
    const auto mounds_turn =
        [](const opengold::rules::CombatSession &session) -> std::optional<opengold::rules::Command>
    {
        constexpr unsigned magic_missile = 15, fireball = 47, ice_storm = 187;
        const auto state = session.snapshot();
        const bool dracolich =
            std::ranges::any_of(state.combatants,
                                [](const opengold::rules::CombatantView &unit)
                                {
                                    return unit.side == opengold::rules::Side::opposition &&
                                           unit.conscious && unit.name.starts_with("DRACOLICH");
                                });
        const bool mounds =
            std::ranges::any_of(state.combatants,
                                [](const opengold::rules::CombatantView &unit)
                                {
                                    return unit.conscious && (unit.name.starts_with("SHAMBLING") ||
                                                              unit.name.starts_with("DRACOLICH"));
                                });
        if (!mounds)
        {
            return std::nullopt;
        }
        // The doubled steps of each burst's reach: Ice Storm's 2 squares,
        // Fireball's 3 (Session::within).
        const auto reach = [](unsigned spell)
        {
            return spell == ice_storm ? 5 : 7;
        };
        const auto wanted = [dracolich](unsigned spell, const opengold::rules::CombatantView &unit)
        {
            if (dracolich)
            {
                return spell == fireball && unit.name.starts_with("DRACOLICH");
            }
            return spell == ice_storm ? unit.name.starts_with("SHAMBLING")
                                      : unit.name.starts_with("GIANT SNAKE");
        };
        const auto burst_worth = [&](unsigned spell, opengold::rules::Cell aim)
        {
            int worth = 0;
            for (const auto &unit : state.combatants)
            {
                const int dx = std::abs(unit.cell.x - aim.x), dy = std::abs(unit.cell.y - aim.y);
                if (!unit.conscious || 2 * std::max(dx, dy) + std::min(dx, dy) > reach(spell))
                {
                    continue;
                }
                worth += unit.side == opengold::rules::Side::party ? -100
                         : wanted(spell, unit)                     ? 1
                                                                   : 0;
            }
            return worth;
        };
        const auto spell_of = [](const opengold::rules::Command &command)
        {
            return command.verb == "use" ? command.item % 256 : command.item;
        };
        const auto commands = session.legal_commands();
        const opengold::rules::Command *aim = nullptr;
        for (const auto &command : commands)
        {
            if (command.verb == "aim" && (command.item == ice_storm || command.item == fireball) &&
                (!aim || burst_worth(command.item, command.destination) >
                             burst_worth(aim->item, aim->destination)))
            {
                aim = &command;
            }
        }
        if (aim && burst_worth(aim->item, aim->destination) > 0)
        {
            return *aim;
        }
        // Magic Missile at the mound with the fewest Hit Points left.
        const auto unit_of = [&](opengold::rules::EntityId id)
        {
            return std::ranges::find(state.combatants, id, &opengold::rules::CombatantView::id);
        };
        const opengold::rules::Command *missile = nullptr;
        for (const auto &command : commands)
        {
            if (command.verb == "aim" && command.item == magic_missile &&
                unit_of(command.target) != state.combatants.end() &&
                unit_of(command.target)->conscious &&
                (unit_of(command.target)->name.starts_with("SHAMBLING") ||
                 unit_of(command.target)->name.starts_with("DRACOLICH")) &&
                (!missile ||
                 unit_of(command.target)->hit_points < unit_of(missile->target)->hit_points))
            {
                missile = &command;
            }
        }
        if (missile)
        {
            return *missile;
        }
        if (aim)
        {
            const auto abort = std::ranges::find(commands, std::string_view("abort"),
                                                 &opengold::rules::Command::verb);
            if (abort != commands.end())
            {
                return *abort;
            }
        }
        const auto actor =
            std::ranges::find(state.combatants, state.actor, &opengold::rules::CombatantView::id);
        if (actor == state.combatants.end())
        {
            return std::nullopt;
        }
        const auto doubled = [](opengold::rules::Cell a, opengold::rules::Cell b)
        {
            const int dx = std::abs(a.x - b.x), dy = std::abs(a.y - b.y);
            return 2 * std::max(dx, dy) + std::min(dx, dy);
        };
        // A burst worth casting: a square in its range (Ice Storm from the
        // wand at level 6, 6 squares; Fireball 10 and 1 a level) takes a
        // wanted enemy (two snakes for a Fireball) and no friend.
        const auto &field = state.battlefield;
        const auto worth_casting = [&](unsigned spell, opengold::rules::Cell from)
        {
            const int least = spell == fireball && !dracolich ? 2 : 1,
                      range = spell == fireball ? 16 : 6;
            for (int y = 0; y < field.height; ++y)
            {
                for (int x = 0; x < field.width; ++x)
                {
                    if (doubled(from, {x, y}) / 2 <= range && burst_worth(spell, {x, y}) >= least)
                    {
                        return true;
                    }
                }
            }
            return false;
        };
        // Crimdrac is immune to cold; his cluebook recommends the immediate
        // Magic Missile first, then Fireball. All are real offered commands.
        if (dracolich)
        {
            for (const auto &command : commands)
            {
                if (command.verb == "cast" && command.item == magic_missile)
                {
                    return command;
                }
            }
        }
        bool blasting = false;
        for (const unsigned spell : {ice_storm, fireball})
        {
            if (dracolich && spell == ice_storm)
            {
                continue;
            }
            for (const auto &command : commands)
            {
                if ((command.verb == "use" || command.verb == "cast") && spell_of(command) == spell)
                {
                    blasting = true;
                    if (worth_casting(spell, actor->cell))
                    {
                        return command;
                    }
                }
            }
        }
        const auto nearest_enemy = [&](opengold::rules::Cell cell, bool mounds_only)
        {
            int nearest = 999;
            for (const auto &unit : state.combatants)
            {
                if (unit.side == opengold::rules::Side::opposition && unit.conscious &&
                    (!mounds_only ||
                     (unit.name.starts_with("SHAMBLING") || unit.name.starts_with("DRACOLICH"))))
                {
                    nearest = std::min(nearest, doubled(cell, unit.cell));
                }
            }
            return nearest;
        };
        // The caster with a burst left steps nearer the mounds, out of their
        // reach and its own burst's, to bring them into its range.
        if (blasting)
        {
            const opengold::rules::Command *step = nullptr;
            for (const auto &command : commands)
            {
                if (command.verb == "move" && nearest_enemy(command.destination, false) > 5 &&
                    nearest_enemy(command.destination, true) < nearest_enemy(actor->cell, true) &&
                    (!step || nearest_enemy(command.destination, true) <
                                  nearest_enemy(step->destination, true)))
                {
                    step = &command;
                }
            }
            if (step)
            {
                return *step;
            }
            // Where no burst spares the party, Magic Missile at a mound, else
            // a guard: Quick would aim the wand into the melee.
            for (const auto &command : commands)
            {
                if (command.verb == "cast" && command.item == magic_missile)
                {
                    return command;
                }
            }
            const auto guard = std::ranges::find(commands, std::string_view("guard"),
                                                 &opengold::rules::Command::verb);
            if (guard != commands.end())
            {
                return *guard;
            }
            return std::nullopt;
        }
        // The rest let the enemy come to them for the first rounds, so the
        // bursts find the mounds clear of the party.
        constexpr unsigned rounds_held = 6;
        if (state.round <= rounds_held && nearest_enemy(actor->cell, false) > 3)
        {
            const auto guard = std::ranges::find(commands, std::string_view("guard"),
                                                 &opengold::rules::Command::verb);
            if (guard != commands.end())
            {
                return *guard;
            }
        }
        return std::nullopt;
    };
    // One play of the pending fight with these dice; the party ends as it left it.
    // A player's own choice for a member's turn, where it has one, as the
    // cluebook fights the Pit of Moander: the Wand of Defoliation (its power,
    // Blast) at the nearest enemy within its three squares, Hold Monster at
    // the strongest enemy not yet helpless, Hold Person at the strongest
    // person (not a shambling mound nor a bit of Moander, plants it does not
    // hold); Quick for the rest.
    // A Curse player's choices against Tyranthraxus's guard, as the cluebook
    // bids (the final battle): fireballs into the margoyles and high priests
    // before they act, the burst where it takes the most and spares every
    // member; a cleric's Prayer once; then Hold Person on the high priests,
    // who are people, before they cast. Anything else the member's Quick
    // chooses.
    const auto finale_turn = [third_bond](const opengold::rules::CombatSession &session)
        -> std::optional<opengold::rules::Command>
    {
        constexpr unsigned fireball = 47, prayer = 42, hold_person = 23;
        constexpr int burst_reach = 7; // doubled steps: Fireball's 3 squares
        const auto state = session.snapshot();
        const bool mogion =
            third_bond &&
            std::ranges::any_of(state.combatants,
                                [](const opengold::rules::CombatantView &unit)
                                {
                                    return unit.side == opengold::rules::Side::opposition &&
                                           unit.name.starts_with("MOGION");
                                });
        const auto foe = [](const opengold::rules::CombatantView &unit)
        {
            return unit.side == opengold::rules::Side::opposition && unit.conscious;
        };
        if (std::ranges::none_of(state.combatants,
                                 [&](const opengold::rules::CombatantView &unit)
                                 {
                                     return foe(unit) &&
                                            (unit.name.starts_with("TYRANTHRAXUS") ||
                                             (mogion && unit.name.starts_with("MOGION")));
                                 }))
        {
            return std::nullopt;
        }
        const auto priest = [&](const opengold::rules::CombatantView &unit)
        {
            return foe(unit) && unit.status.empty() &&
                   (unit.name.starts_with("HIGH PRIEST") ||
                    (mogion &&
                     (unit.name.starts_with("MOGION") || unit.name.starts_with("CULTIST"))));
        };
        const auto burst_worth = [&](opengold::rules::Cell aim)
        {
            int worth = 0;
            for (const auto &unit : state.combatants)
            {
                const int dx = std::abs(unit.cell.x - aim.x), dy = std::abs(unit.cell.y - aim.y);
                if (!unit.conscious || 2 * std::max(dx, dy) + std::min(dx, dy) > burst_reach)
                {
                    continue;
                }
                // Fireball interrupts Mogion's human casters. Her mounds
                // resist fire and are kept for the Defoliation wand.
                worth += unit.side == opengold::rules::Side::party              ? -100
                         : priest(unit) || unit.name.starts_with("HIGH PRIEST") ? 2
                         : mogion && unit.name.starts_with("SHAMBLING")         ? 0
                                                                                : 1;
            }
            return worth;
        };
        const auto unit_of = [&](opengold::rules::EntityId id)
        {
            return std::ranges::find(state.combatants, id, &opengold::rules::CombatantView::id);
        };
        const auto spell_of = [](const opengold::rules::Command &command)
        {
            return command.verb == "use" ? command.item % 256 : command.item;
        };
        const auto commands = session.legal_commands();
        // Aiming a burst or a hold already begun.
        const opengold::rules::Command *aim = nullptr;
        for (const auto &command : commands)
        {
            if (command.verb == "aim" && command.item == fireball &&
                (!aim || burst_worth(command.destination) > burst_worth(aim->destination)))
            {
                aim = &command;
            }
            if (command.verb == "aim" && command.item == hold_person &&
                unit_of(command.target) != state.combatants.end() &&
                priest(*unit_of(command.target)))
            {
                return command;
            }
        }
        if (aim)
        {
            return *aim;
        }
        for (const auto &command : commands)
        {
            if (command.verb == "aim_done" && command.item == hold_person)
            {
                return command;
            }
        }
        // A burst worth casting: one square in range takes three or more
        // foes and no member.
        const bool burst_worth_casting = [&]
        {
            for (int y = 0; y < state.battlefield.height; ++y)
            {
                for (int x = 0; x < state.battlefield.width; ++x)
                {
                    if (burst_worth({x, y}) >= 3)
                    {
                        return true;
                    }
                }
            }
            return false;
        }();
        const bool prayed = std::ranges::any_of(
            state.log_entries,
            [](const opengold::rules::LogEntry &entry)
            {
                return entry.english.find(" casts Prayer.") != std::string::npos &&
                       !entry.english.starts_with("HIGH PRIEST");
            });
        const bool priests_free = std::ranges::any_of(state.combatants, priest);
        for (const auto &command : commands)
        {
            if (command.verb != "cast" && command.verb != "use")
            {
                continue;
            }
            const auto spell = spell_of(command);
            if ((spell == fireball && burst_worth_casting) || (spell == prayer && !prayed) ||
                (spell == hold_person && priests_free && command.verb == "cast"))
            {
                return command;
            }
        }
        return std::nullopt;
    };
    // Fight past the minions' retaliating shields to dragons and casters.
    // Distant shots can still hit a shielded creature safely.
    const auto darkness_finale_turn =
        [&](const opengold::rules::CombatSession &combat,
            std::set<opengold::rules::EntityId> &spaced) -> std::optional<opengold::rules::Command>
    {
        const auto state = combat.snapshot();
        const auto actor =
            std::ranges::find(state.combatants, state.actor, &opengold::rules::CombatantView::id);
        if (actor == state.combatants.end())
        {
            return std::nullopt;
        }
        const auto commands = combat.legal_commands();
        const auto enemies_left =
            std::ranges::count_if(state.combatants,
                                  [&](const opengold::rules::CombatantView &unit)
                                  {
                                      return unit.side != actor->side && unit.conscious;
                                  });
        const int healing_threshold = enemies_left <= 2 ? 9 : 4;
        // The crystal leaves its bearer near death. Heal a badly wounded
        // neighbour before attacking or starting another support spell.
        const opengold::rules::CombatantView *patient = nullptr;
        for (const auto &unit : state.combatants)
        {
            if (unit.side == actor->side && !unit.dead &&
                unit.hit_points * 12 < unit.max_hit_points * healing_threshold &&
                (!patient || unit.hit_points < patient->hit_points))
            {
                patient = &unit;
            }
        }
        if (patient)
        {
            for (const auto &command : commands)
            {
                if (command.verb == "aim" && command.item == 236 && command.target == patient->id)
                {
                    return command;
                }
            }
            for (const auto &command : commands)
            {
                if (command.verb == "cast" && command.item == 236)
                {
                    const auto range = [&](opengold::rules::Cell cell)
                    {
                        return std::max(std::abs(patient->cell.x - cell.x),
                                        std::abs(patient->cell.y - cell.y));
                    };
                    if (range(actor->cell) <= 1)
                    {
                        return command;
                    }
                    const opengold::rules::Command *nearer = nullptr;
                    for (const auto &step : commands)
                    {
                        if (step.verb == "move" && range(step.destination) < range(actor->cell) &&
                            (!nearer || range(step.destination) < range(nearer->destination)))
                        {
                            nearer = &step;
                        }
                    }
                    if (nearer)
                    {
                        return *nearer;
                    }
                }
            }
        }
        // Refresh Haste while a chained battle is still in progress;
        // the next one may bar magic and the first battle's Slow removes it.
        const bool beholders = std::ranges::any_of(
            state.combatants,
            [&](const opengold::rules::CombatantView &unit)
            {
                return unit.side != actor->side &&
                       (unit.name.starts_with("BEHOLDER") || unit.name.starts_with("ELDER ORB") ||
                        unit.name.starts_with("DEATH TYRANT"));
            });
        if (beholders && (state.round == 1 || enemies_left <= 3))
        {
            const auto has_haste = [](const opengold::rules::CombatantView &unit)
            {
                const auto numbers =
                    opengold::original::Combatant::decode(unit.persistent.resources);
                return numbers && std::ranges::any_of(numbers->effects,
                                                      [](const opengold::original::Effect &effect)
                                                      {
                                                          return effect.code == 39;
                                                      });
            };
            const bool needs_haste = std::ranges::any_of(
                state.combatants,
                [&](const opengold::rules::CombatantView &unit)
                {
                    return unit.side == actor->side && unit.conscious &&
                           (unit.name.starts_with("paladin") || unit.name.starts_with("ranger")) &&
                           !has_haste(unit);
                });
            if (needs_haste)
            {
                const auto cast =
                    std::ranges::find_if(commands,
                                         [](const opengold::rules::Command &command)
                                         {
                                             return command.verb == "cast" && command.item == 48;
                                         });
                if (cast != commands.end())
                {
                    return *cast;
                }
            }
        }
        const auto contains_enemy = [&](std::string_view prefix)
        {
            return std::ranges::any_of(state.combatants,
                                       [&](const opengold::rules::CombatantView &unit)
                                       {
                                           return unit.side != actor->side &&
                                                  unit.name.starts_with(prefix);
                                       });
        };
        const bool servants = contains_enemy("DRACOLICH") && contains_enemy("MINION OF BANE") &&
                              contains_enemy("PET OF KALISTES") && contains_enemy("BIT O MOANDER");
        if (servants && actor->name == "magic_user 6")
        {
            // Keep the extra Haste copies for Xoham. Blast the susceptible
            // servants; once only fireproof creatures remain, the fighters
            // finish them while the clerics recover the party.
            const bool fire_target = std::ranges::any_of(
                state.combatants,
                [&](const opengold::rules::CombatantView &unit)
                {
                    if (unit.side == actor->side || !unit.conscious)
                    {
                        return false;
                    }
                    const auto numbers =
                        opengold::original::Combatant::decode(unit.persistent.resources);
                    return numbers &&
                           std::ranges::none_of(numbers->effects,
                                                [](const opengold::original::Effect &effect)
                                                {
                                                    return effect.code == 112;
                                                });
                });
            if (fire_target)
            {
                const auto blast = std::ranges::find_if(
                    commands,
                    [](const opengold::rules::Command &command)
                    {
                        return command.verb == "cast" && command.label == "Delayed Blast Fireball";
                    });
                if (blast != commands.end())
                {
                    return *blast;
                }
            }
            const auto end =
                std::ranges::find(commands, std::string("end"), &opengold::rules::Command::verb);
            if (end != commands.end())
            {
                return *end;
            }
            return std::nullopt;
        }
        const auto own = opengold::original::Combatant::decode(actor->persistent.resources);
        const auto warrior = [&](opengold::original::ClassSlot slot)
        {
            return own && opengold::original::level_in(own->class_levels, slot) > 0;
        };
        const bool magic_barred =
            own && std::ranges::any_of(own->effects,
                                       [](const opengold::original::Effect &effect)
                                       {
                                           return effect.code == 600;
                                       });
        if (!warrior(opengold::original::ClassSlot::fighter) &&
            !warrior(opengold::original::ClassSlot::paladin) &&
            !warrior(opengold::original::ClassSlot::ranger) &&
            !warrior(opengold::original::ClassSlot::cleric) &&
            !(magic_barred && warrior(opengold::original::ClassSlot::magic_user)))
        {
            return std::nullopt;
        }
        std::set<opengold::rules::EntityId> shielded;
        for (const auto &unit : state.combatants)
        {
            if (unit.side == actor->side || !unit.conscious)
            {
                continue;
            }
            const auto numbers = opengold::original::Combatant::decode(unit.persistent.resources);
            if (numbers && std::ranges::any_of(numbers->effects,
                                               [](const opengold::original::Effect &effect)
                                               {
                                                   return effect.code == 343;
                                               }))
            {
                shielded.insert(unit.id);
            }
        }
        if (shielded.empty())
        {
            return std::nullopt;
        }
        // The last battle bars spells. Shoot the shielded minions with
        // missiles collected from actual treasure rather than trading twice
        // every melee hit back into the party. Ready uses the same free
        // inventory commands as the combat view.
        if (magic_barred)
        {
            const auto owner = std::ranges::find_if(
                party->state().slots,
                [&](opengold::MemberId id)
                {
                    return id && party->member(id).character.sheet().name == actor->name;
                });
            if (owner != party->state().slots.end())
            {
                const auto &member = party->member(*owner);
                const auto &inventory = member.character.inventory().items();
                const opengold::InventoryItem *ammunition = nullptr;
                const opengold::InventoryItem *launcher = nullptr;
                for (const auto &item : inventory)
                {
                    const bool spent =
                        std::ranges::any_of(actor->item_uses_left,
                                            [&](const auto &entry)
                                            {
                                                return entry.first == item.id && entry.second == 0;
                                            });
                    if ((item.original_type == 30 || item.original_type == 12) && !spent &&
                        item.quantity > 0)
                    {
                        const auto plus = [&](const opengold::InventoryItem &entry)
                        {
                            const auto source = member.item_sources.find(entry.id);
                            return source == member.item_sources.end()
                                       ? 0
                                       : source->second.stored.magic_bonus;
                        };
                        if (!ammunition || plus(item) > plus(*ammunition) ||
                            (plus(item) == plus(*ammunition) && item.original_type == 30 &&
                             ammunition->original_type != 30))
                        {
                            ammunition = &item;
                        }
                    }
                }
                if (ammunition)
                {
                    const auto type = ammunition->original_type == 30 ? 22 : 27;
                    const auto found =
                        std::ranges::find(inventory, type, &opengold::InventoryItem::original_type);
                    if (found != inventory.end())
                    {
                        launcher = &*found;
                    }
                }
                if (launcher)
                {
                    // Commands address the carried row, not its displayed
                    // name: different unidentified stacks have identical
                    // labels. Preserve the module's order of original items.
                    const auto ordinal_of =
                        [&](const opengold::InventoryItem &wanted) -> std::optional<unsigned>
                    {
                        unsigned ordinal = 0;
                        for (const auto &item : inventory)
                        {
                            const auto record = opengold::por::item_of_original_key(
                                opengold::rules_definition(member, item));
                            if (!record || record->type >= content.item_types.size())
                            {
                                continue;
                            }
                            if (item.id == wanted.id)
                            {
                                return ordinal;
                            }
                            ++ordinal;
                        }
                        return std::nullopt;
                    };
                    const auto &readied =
                        actor->readied_items ? *actor->readied_items : member.equipped;
                    const auto wearing = [&](const opengold::InventoryItem &item)
                    {
                        return std::ranges::find(readied, item.id) != readied.end();
                    };
                    for (const auto *item : {ammunition, launcher})
                    {
                        if (wearing(*item))
                        {
                            continue;
                        }
                        const auto ordinal = ordinal_of(*item);
                        const auto ready =
                            std::ranges::find_if(commands,
                                                 [&](const opengold::rules::Command &command)
                                                 {
                                                     return ordinal && command.verb == "ready" &&
                                                            command.item == *ordinal;
                                                 });
                        if (ready != commands.end())
                        {
                            return *ready;
                        }
                        // Clear the old weapon or shield occupying its hands.
                        for (const auto &command : commands)
                        {
                            if (command.verb != "unready")
                            {
                                continue;
                            }
                            const auto obstructs = std::ranges::find_if(
                                inventory,
                                [&](const opengold::InventoryItem &held)
                                {
                                    const auto ordinal = ordinal_of(held);
                                    return held.id != ammunition->id && held.id != launcher->id &&
                                           ordinal && command.item == *ordinal &&
                                           (held.original_type == 18 || held.original_type == 22 ||
                                            held.original_type == 27 || held.original_type == 37 ||
                                            held.original_type == 30 || held.original_type == 12);
                                });
                            if (obstructs != inventory.end())
                            {
                                return command;
                            }
                        }
                    }
                }
            }
        }
        if (magic_barred && state.round == 1 && !spaced.contains(actor->id))
        {
            const auto spacing = [&](opengold::rules::Cell cell)
            {
                int nearest = 99, total = 0;
                for (const auto &unit : state.combatants)
                {
                    if (unit.id != actor->id && unit.side == actor->side && unit.conscious)
                    {
                        const int apart = std::max(std::abs(cell.x - unit.cell.x),
                                                   std::abs(cell.y - unit.cell.y));
                        nearest = std::min(nearest, apart);
                        total += apart;
                    }
                }
                return std::pair{nearest, total};
            };
            if (spacing(actor->cell).first < 3)
            {
                const opengold::rules::Command *spread = nullptr;
                for (const auto &command : commands)
                {
                    if (command.verb == "move" &&
                        spacing(command.destination) > spacing(actor->cell) &&
                        (!spread || spacing(command.destination) > spacing(spread->destination)))
                    {
                        spread = &command;
                    }
                }
                if (spread)
                {
                    return *spread;
                }
            }
            spaced.insert(actor->id);
        }
        const auto priority = [magic_barred](const opengold::rules::CombatantView &unit)
        {
            // In the no-magic battle the minions' repeated lightning is
            // the main threat. Missile users can kill them without retaliation.
            if (magic_barred && unit.name.starts_with("MINION OF BANE"))
            {
                return -1;
            }
            return unit.name.starts_with("DRACOLICH") || unit.name.starts_with("GOTHMENES") ? 0
                   : unit.name.starts_with("PET OF KALISTES")                               ? 1
                                                                                            : 2;
        };
        const opengold::rules::Command *attack = nullptr;
        int rank = 100;
        for (const auto &command : commands)
        {
            if (command.verb != "melee" && command.verb != "ranged")
            {
                continue;
            }
            const auto target = std::ranges::find(state.combatants, command.target,
                                                  &opengold::rules::CombatantView::id);
            if (target == state.combatants.end() || target->side == actor->side ||
                (command.verb == "melee" && shielded.contains(target->id)) ||
                (command.verb == "ranged" && target->name.starts_with("GOTHMENES") &&
                 (!own || !own->striker || own->striker->plus < 3)))
            {
                continue;
            }
            if (!attack || priority(*target) < rank)
            {
                attack = &command;
                rank = priority(*target);
            }
        }
        if (attack)
        {
            return *attack;
        }
        if (own && own->missile && !own->missile->throwable)
        {
            const auto nearest = [&](opengold::rules::Cell cell)
            {
                int close = 99;
                for (const auto &unit : state.combatants)
                {
                    if (unit.side != actor->side && unit.conscious)
                    {
                        const int dx =
                            std::max({unit.cell.x - cell.x,
                                      cell.x - (unit.cell.x + unit.footprint.width - 1), 0});
                        const int dy =
                            std::max({unit.cell.y - cell.y,
                                      cell.y - (unit.cell.y + unit.footprint.height - 1), 0});
                        close = std::min(close, std::max(dx, dy));
                    }
                }
                return close;
            };
            const opengold::rules::Command *clear = nullptr;
            for (const auto &command : commands)
            {
                if (command.verb == "move" && nearest(command.destination) > 1 &&
                    nearest(command.destination) > nearest(actor->cell) &&
                    (!clear || nearest(command.destination) > nearest(clear->destination)))
                {
                    clear = &command;
                }
            }
            if (clear)
            {
                return *clear;
            }
        }
        // A mage without spells keeps clear instead of charging a balor
        // or guarding into a minion's retaliating shield.
        if (magic_barred && warrior(opengold::original::ClassSlot::magic_user))
        {
            const auto end =
                std::ranges::find(commands, std::string("end"), &opengold::rules::Command::verb);
            if (end != commands.end())
            {
                return *end;
            }
        }
        const auto distance = [](opengold::rules::Cell a, opengold::rules::Cell b)
        {
            return 2 * std::max(std::abs(a.x - b.x), std::abs(a.y - b.y)) +
                   std::min(std::abs(a.x - b.x), std::abs(a.y - b.y));
        };
        const opengold::rules::CombatantView *foe = nullptr;
        for (const auto &unit : state.combatants)
        {
            if (unit.side != actor->side && unit.conscious && !shielded.contains(unit.id) &&
                (!foe || priority(unit) < priority(*foe) ||
                 (priority(unit) == priority(*foe) &&
                  distance(actor->cell, unit.cell) < distance(actor->cell, foe->cell))))
            {
                foe = &unit;
            }
        }
        const opengold::rules::Command *step = nullptr;
        if (foe)
        {
            for (const auto &command : commands)
            {
                if (command.verb == "move" &&
                    distance(command.destination, foe->cell) < distance(actor->cell, foe->cell) &&
                    (!step || distance(command.destination, foe->cell) <
                                  distance(step->destination, foe->cell)))
                {
                    step = &command;
                }
            }
        }
        if (step)
        {
            return *step;
        }
        // When another body blocks the way, wait for the mage's blasts to
        // clear it rather than handing the turn to Quick to hit a shield.
        const bool mage_stands = std::ranges::any_of(state.combatants,
                                                     [&](const opengold::rules::CombatantView &unit)
                                                     {
                                                         return unit.side == actor->side &&
                                                                unit.conscious &&
                                                                unit.name == "magic_user 6";
                                                     });
        if ((mage_stands && !magic_barred && state.round <= 10) ||
            (magic_barred && warrior(opengold::original::ClassSlot::magic_user)))
        {
            const auto end =
                std::ranges::find(commands, std::string("end"), &opengold::rules::Command::verb);
            if (end != commands.end())
            {
                return *end;
            }
        }
        return std::nullopt;
    };
    const auto player_turn =
        [game, &keeps_preparation](
            const opengold::rules::CombatSession &session,
            std::vector<opengold::rules::EntityId> &picked,
            const std::set<std::string> &bolt_bearers) -> std::optional<opengold::rules::Command>
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
        // The magic-user's spells of the eighth and ninth levels, the only
        // ones that touch Pools of Darkness's rakshasas: Meteor Swarm, Power
        // Word Kill, Mass Charm and Power Word Blind.
        constexpr std::array<unsigned, 4> rakshasa_spells{324, 325, 320, 323};
        const auto state = session.snapshot();
        const bool rakshasas =
            std::ranges::any_of(state.combatants,
                                [](const opengold::rules::CombatantView &unit)
                                {
                                    return unit.side == opengold::rules::Side::opposition &&
                                           unit.name.starts_with("RAKSHASA");
                                });
        const auto ours = [&](unsigned spell)
        {
            return spell == blast || spell == hold_monster || holds_people(spell) ||
                   (rakshasas &&
                    std::ranges::find(rakshasa_spells, spell) != rakshasa_spells.end());
        };
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
            if (unit != state.combatants.end() && holds_people(spell) &&
                game == GoldBoxGame::pools_of_darkness && keeps_preparation)
            {
                const auto numbers =
                    opengold::original::Combatant::decode(unit->persistent.resources);
                if (!numbers || numbers->race == 0)
                {
                    return false;
                }
            }
            return unit != state.combatants.end() &&
                   unit->side == opengold::rules::Side::opposition && unit->conscious &&
                   unit->status.empty() && std::ranges::find(picked, id) == picked.end() &&
                   (!holds_people(spell) || !(unit->name.starts_with("SHAMBLING") ||
                                              unit->name.starts_with("BIT O MOANDER"))) &&
                   (spell != blast || distance(unit->cell) <= 3);
        };
        const auto better =
            [&](const opengold::rules::Command &a, const opengold::rules::Command &b)
        {
            if (game == GoldBoxGame::secret_of_the_silver_blades &&
                (holds_people(spell_of(a)) || spell_of(a) == hold_monster))
            {
                const auto caster = [](const opengold::rules::CombatantView &unit)
                {
                    return unit.name.find("WIZARD") != std::string::npos ||
                           unit.name.find("NECROMANCER") != std::string::npos ||
                           unit.name.find("PRIEST") != std::string::npos;
                };
                const bool a_casts = caster(*unit_of(a.target));
                const bool b_casts = caster(*unit_of(b.target));
                if (a_casts != b_casts)
                {
                    return a_casts;
                }
            }
            return spell_of(a) == blast
                       ? distance(a.destination) < distance(b.destination)
                       : unit_of(a.target)->hit_points > unit_of(b.target)->hit_points;
        };
        const auto commands = session.legal_commands();
        // Pools of Darkness's rakshasas are shot from afar, where a blessed
        // bolt slays one (its effect 82), as Nacacia bids: the Rajahs, whose
        // Lightning Bolts and Ice Storms harm the most, the Maharajah, the
        // Rukhs, whom no weapon below +4 harms whole, and last the
        // rakshasas, whom the swords +3 harm.
        const auto shooting_rank = [](const std::string &name)
        {
            return name.starts_with("RAKSHASA RAJAH")  ? 0
                   : name.starts_with("RAKSHASA MAH")  ? 1
                   : name.starts_with("RAKSHASA RUKH") ? 2
                                                       : 3;
        };
        const opengold::rules::Command *shot = nullptr;
        for (const auto &command : commands)
        {
            if (command.verb == "ranged" && unit_of(command.target) != state.combatants.end() &&
                unit_of(command.target)->name.starts_with("RAKSHASA") &&
                (!shot || shooting_rank(unit_of(command.target)->name) <
                              shooting_rank(unit_of(shot->target)->name)))
            {
                shot = &command;
            }
        }
        if (shot)
        {
            return *shot;
        }
        // A member with a crossbow readied cannot shoot with an enemy next to
        // it, nor beyond its range: against the rakshasas it steps where no
        // enemy stands next to it, nearer the nearest rakshasa, as a player
        // keeps the bolts' bearers clear to shoot.
        const auto rakshasa_near = [&](opengold::rules::Cell cell)
        {
            int nearest = 99;
            for (const auto &unit : state.combatants)
            {
                if (unit.side == opengold::rules::Side::opposition && unit.conscious &&
                    unit.name.starts_with("RAKSHASA"))
                {
                    nearest = std::min(nearest, std::max(std::abs(cell.x - unit.cell.x),
                                                         std::abs(cell.y - unit.cell.y)));
                }
            }
            return nearest;
        };
        const auto enemy_beside = [&](opengold::rules::Cell cell)
        {
            return std::ranges::any_of(state.combatants,
                                       [&](const opengold::rules::CombatantView &unit)
                                       {
                                           return unit.side == opengold::rules::Side::opposition &&
                                                  unit.conscious && unit.id != state.actor &&
                                                  std::max(std::abs(cell.x - unit.cell.x),
                                                           std::abs(cell.y - unit.cell.y)) <= 1;
                                       });
        };
        if (actor != state.combatants.end() && bolt_bearers.contains(actor->name) &&
            rakshasa_near(actor->cell) < 99)
        {
            const opengold::rules::Command *step = nullptr;
            for (const auto &command : commands)
            {
                if (command.verb != "move" || enemy_beside(command.destination))
                {
                    continue;
                }
                const bool clears = enemy_beside(actor->cell);
                const bool nears = rakshasa_near(command.destination) < rakshasa_near(actor->cell);
                if ((clears || nears) && (!step || rakshasa_near(command.destination) <
                                                       rakshasa_near(step->destination)))
                {
                    step = &command;
                }
            }
            if (step)
            {
                return *step;
            }
        }
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
        for (const unsigned wanted :
             {324U, 325U, 320U, 323U, blast, hold_monster, hold_person, mage_hold_person})
        {
            if (wanted > 300 && !rakshasas)
            {
                continue;
            }
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
    // A player's answer to a member charmed onto the other side or held
    // helpless (Treasures' Way Inn sorceresses, Freezefire's black-robed
    // men, Pools of Darkness's rakshasas): Charm Person at a charmed one,
    // which frees it with no save as the original Treasures' does, or Dispel
    // Magic, which ends a charm or a hold as the original's dispel does,
    // where a caster has it memorized.
    const auto free_the_bound =
        [&](const opengold::rules::CombatSession &fight) -> std::optional<opengold::rules::Command>
    {
        constexpr unsigned charm_person = 10, cleric_dispel = 41, mage_dispel = 46;
        const auto dispel = [](const opengold::rules::Command &command)
        {
            return command.item == cleric_dispel || command.item == mage_dispel;
        };
        const auto ours = [&](const opengold::rules::CombatantView &unit)
        {
            return std::ranges::any_of(party->state().slots,
                                       [&](opengold::MemberId id)
                                       {
                                           return id && party->member(id).character.sheet().name ==
                                                            unit.name;
                                       });
        };
        const auto state = fight.snapshot();
        std::vector<opengold::rules::CombatantView> charmed, held;
        for (const auto &unit : state.combatants)
        {
            if (!ours(unit) || unit.dead)
            {
                continue;
            }
            if (unit.side == opengold::rules::Side::opposition)
            {
                charmed.push_back(unit);
            }
            else if (unit.status == "helpless")
            {
                held.push_back(unit);
            }
        }
        if (charmed.empty() && held.empty())
        {
            return std::nullopt;
        }
        const auto aims_at = [](const opengold::rules::Command &command,
                                const std::vector<opengold::rules::CombatantView> &units)
        {
            return std::ranges::any_of(units,
                                       [&](const opengold::rules::CombatantView &unit)
                                       {
                                           return command.target == unit.id ||
                                                  (command.destination.x == unit.cell.x &&
                                                   command.destination.y == unit.cell.y);
                                       });
        };
        const auto commands = fight.legal_commands();
        for (const auto &command : commands)
        {
            if (command.verb == "aim" &&
                ((command.item == charm_person && aims_at(command, charmed)) ||
                 (dispel(command) && (aims_at(command, charmed) || aims_at(command, held)))))
            {
                return command;
            }
        }
        for (const auto &command : commands)
        {
            if (command.verb == "cast" && command.item == charm_person && !charmed.empty())
            {
                return command;
            }
        }
        for (const auto &command : commands)
        {
            if (command.verb == "cast" && dispel(command))
            {
                return command;
            }
        }
        return std::nullopt;
    };
    // Steps over open ground from each square of the field to its edge, or
    // to one side of it (0 north, 1 east, 2 south, 3 west), and the side a
    // square at the edge leaves by, as the combat rules tell it.
    const auto edge_side = [](const opengold::rules::Battlefield &field,
                              opengold::rules::Cell cell) -> std::optional<unsigned>
    {
        return cell.y == 0                  ? std::optional<unsigned>{0}
               : cell.x == field.width - 1  ? std::optional<unsigned>{1}
               : cell.y == field.height - 1 ? std::optional<unsigned>{2}
               : cell.x == 0                ? std::optional<unsigned>{3}
                                            : std::nullopt;
    };
    // Steps from each square of the field to the nearest of the given
    // squares, over open ground, or with `rough` over any but obstacles.
    const auto steps_from = [](const opengold::rules::Battlefield &field,
                               std::vector<opengold::rules::Cell> frontier, bool rough = false)
    {
        const auto passable = [&field, rough](int x, int y)
        {
            const auto ground = field.at({x, y});
            return rough ? ground != opengold::rules::Terrain::obstacle
                         : ground == opengold::rules::Terrain::open;
        };
        const auto at = [&field](int x, int y)
        {
            return static_cast<std::size_t>(y * field.width + x);
        };
        std::vector<int> steps(static_cast<std::size_t>(field.width * field.height), 9999);
        for (const auto cell : frontier)
        {
            steps[at(cell.x, cell.y)] = 0;
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
                        !passable(x, y) || steps[at(x, y)] <= steps[at(from.x, from.y)] + 1)
                    {
                        continue;
                    }
                    steps[at(x, y)] = steps[at(from.x, from.y)] + 1;
                    frontier.push_back({x, y});
                }
            }
        }
        return [steps, at, &field](opengold::rules::Cell cell)
        {
            return cell.x < 0 || cell.y < 0 || cell.x >= field.width || cell.y >= field.height
                       ? 9999
                       : steps[at(cell.x, cell.y)];
        };
    };
    const auto edge_steps =
        [&](const opengold::rules::Battlefield &field, std::optional<unsigned> side)
    {
        std::vector<opengold::rules::Cell> edge;
        for (int y = 0; y < field.height; ++y)
        {
            for (int x = 0; x < field.width; ++x)
            {
                const auto leaves_by = edge_side(field, {x, y});
                if (leaves_by && (!side || leaves_by == side) &&
                    field.at({x, y}) == opengold::rules::Terrain::open)
                {
                    edge.push_back({x, y});
                }
            }
        }
        return steps_from(field, std::move(edge));
    };
    // The side the party means to leave the next maze fight by
    // (RouteBeat::maze_exits), as the cluebook leads through Ascore.
    std::optional<unsigned> wanted_exit;
    // The beat being played flees what it meets (RouteBeat::fleeing).
    bool fleeing = false;
    // Whether the command begins a spell a player would not cast at iron
    // golems: fire (Fireball, Flame Strike, Meteor Swarm and the like) while
    // one stands on the other side, for fire heals it (EffIronGolem); and,
    // while only iron golems stand there, any spell against them, for none
    // but lightning, which only slows them, touches them. Quick magic, whose
    // computer picks the spell itself, is such a spell too there.
    // The member's Lightning Bolt where only iron golems stand on the other
    // side: begun, or aimed at a golem it has not slowed yet (else any).
    const auto lightning_at_golems =
        [&](const opengold::rules::CombatSession &fight) -> std::optional<opengold::rules::Command>
    {
        const auto bolt = opengold::original::spell_number("magic_user:lightning_bolt", game);
        if (!bolt)
        {
            return std::nullopt;
        }
        const auto state = fight.snapshot();
        const auto golem = [&](opengold::rules::EntityId id, bool unslowed)
        {
            const auto unit =
                std::ranges::find(state.combatants, id, &opengold::rules::CombatantView::id);
            return unit != state.combatants.end() &&
                   unit->side == opengold::rules::Side::opposition && unit->conscious &&
                   unit->name.find("IRON GOLEM") != std::string::npos &&
                   (!unslowed || unit->status.find("slow") == std::string::npos);
        };
        const auto commands = fight.legal_commands();
        for (const bool unslowed : {true, false})
        {
            for (const auto &command : commands)
            {
                if (command.verb == "aim" && command.item == static_cast<unsigned>(*bolt) &&
                    golem(command.target, unslowed))
                {
                    return command;
                }
            }
        }
        for (const auto &command : commands)
        {
            if (command.verb == "cast" && command.item == static_cast<unsigned>(*bolt))
            {
                return command;
            }
        }
        return std::nullopt;
    };
    const auto wasted_on_golems =
        [&](const opengold::rules::Snapshot &state, const opengold::rules::Command &command)
    {
        bool golems = false, others = false;
        for (const auto &unit : state.combatants)
        {
            if (unit.side == opengold::rules::Side::opposition && unit.conscious)
            {
                (unit.name.find("IRON GOLEM") != std::string::npos ? golems : others) = true;
            }
        }
        if (!golems)
        {
            return false;
        }
        if (command.verb == "quick_magic")
        {
            return !others;
        }
        if (command.verb != "cast" && command.verb != "use")
        {
            return false;
        }
        const auto *known = opengold::original::spell(
            static_cast<int>(command.verb == "use" ? command.item % 256 : command.item), game);
        if (!known)
        {
            return false;
        }
        const std::string_view name = known->name;
        const bool fire = name.find("Fire") != std::string_view::npos ||
                          name.find("Flame") != std::string_view::npos ||
                          name.find("Burning") != std::string_view::npos ||
                          name.find("Meteor") != std::string_view::npos;
        return fire || (!others && known->offensive);
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
                // OPENGOLD_FIGHT_CELLS adds where each stands.
                if (std::getenv("OPENGOLD_FIGHT_CELLS"))
                {
                    std::cout << "@" << unit.cell.x << "," << unit.cell.y;
                }
                if (std::getenv("OPENGOLD_FIGHT_GEAR"))
                {
                    const auto numbers =
                        opengold::original::Combatant::decode(unit.persistent.resources);
                    check(numbers.has_value(), "A traced original combatant has its numbers");
                    std::cout << " AC=" << numbers->armor_class << " THAC0=" << numbers->thac0
                              << " effects[";
                    for (const auto &effect : numbers->effects)
                    {
                        std::cout << effect.code << ":" << effect.minutes << ",";
                    }
                    std::cout << "]";
                }
            }
            std::cout << "\n";
        }
        // The members with a light crossbow readied (Pools of Darkness's item
        // type 27), who shoot the blessed bolts at the rakshasas.
        std::set<std::string> bolt_bearers;
        for (const auto id : party->state().slots)
        {
            if (!id)
            {
                continue;
            }
            const auto &member = party->member(id);
            // OPENGOLD_FIGHT_GEAR prints each member's memorized spells and
            // items as a fight begins (a planning aid).
            const bool gear = std::getenv("OPENGOLD_FIGHT_GEAR") != nullptr;
            if (gear)
            {
                std::cout << "  gear: " << member.character.sheet().name << " spells";
                for (const auto &spell : party->memorized_spells(id))
                {
                    std::cout << " " << spell;
                }
                std::cout << "\n";
            }
            for (const auto &item : member.character.inventory().items())
            {
                if (gear)
                {
                    std::cout << "  gear: " << member.character.sheet().name << " " << item.name
                              << " type " << item.original_type << " x" << item.quantity
                              << (std::ranges::find(member.equipped, item.id) !=
                                          member.equipped.end()
                                      ? " readied"
                                      : "")
                              << "\n";
                }
                if (item.original_type == 27 &&
                    std::ranges::find(member.equipped, item.id) != member.equipped.end())
                {
                    bolt_bearers.insert(member.character.sheet().name);
                }
            }
        }
        // Each turn chosen as the game's combat view chooses it: the party all
        // on Quick with Quick magic on (or, as a player may set it, only the
        // magic-user's), everyone else at the default difficulty.
        std::vector<opengold::rules::EntityId> picked;
        std::set<opengold::rules::EntityId> spaced;
        // Treasures' party on its way past a band it could not flee in the
        // asking (the overland's) runs off the field with the first tactic.
        const bool running =
            game == GoldBoxGame::treasures_of_the_savage_frontier && fleeing && tactic == 0;
        const bool yulash_mounds = third_bond && session.snapshot().script_id == 16;
        for (int command = 0;
             !running && command < 3000 &&
             combat.combat().snapshot().outcome == opengold::rules::Outcome::ongoing;
             ++command)
        {
            const auto &session = combat.combat();
            const auto state = session.snapshot();
            const auto actor = std::ranges::find(state.combatants, state.actor,
                                                 &opengold::rules::CombatantView::id);
            const bool member = actor != state.combatants.end() &&
                                actor->side == opengold::rules::Side::party &&
                                !opengold::is_ally(state.actor);
            // Dispel Magic can free a held or charmed member in Secret too.
            auto held = member && (game == GoldBoxGame::treasures_of_the_savage_frontier ||
                                   game == GoldBoxGame::pools_of_darkness ||
                                   game == GoldBoxGame::secret_of_the_silver_blades)
                            ? free_the_bound(session)
                            : std::nullopt;
            if (!held && member && tactic == 2 &&
                (game == GoldBoxGame::treasures_of_the_savage_frontier ||
                 game == GoldBoxGame::secret_of_the_silver_blades))
            {
                held = treasures_turn(session);
            }
            if (!held && member && tactic == 2 && !wanted_exit &&
                (game == GoldBoxGame::gateway_to_the_savage_frontier || yulash_mounds ||
                 (third_bond &&
                  std::ranges::any_of(state.combatants,
                                      [](const opengold::rules::CombatantView &unit)
                                      {
                                          return unit.side == opengold::rules::Side::opposition &&
                                                 unit.conscious &&
                                                 unit.name.starts_with("DRACOLICH");
                                      }))))
            {
                // Crimdrac is fought with the real Fireball wand won from
                // Dracandros. These are ordinary targeting/movement commands.
                held = mounds_turn(session);
            }
            if (!held && member && tactic == 2 && game == GoldBoxGame::curse_of_the_azure_bonds)
            {
                held = finale_turn(session);
            }
            if (!held && member && tactic == 2 && game == GoldBoxGame::pools_of_darkness &&
                keeps_preparation)
            {
                held = darkness_finale_turn(session, spaced);
            }
            // Treasures' Hold Person takes no monster, and its Quick casts by
            // its own AI, which aims its holds where they take: the other
            // games' choices do not serve it.
            if (!held && member && tactic == 2 &&
                game != GoldBoxGame::treasures_of_the_savage_frontier)
            {
                held = player_turn(session, picked, bolt_bearers);
            }
            // In a maze fight each member makes for the side the party means to
            // leave by and runs off it there.
            if (member && wanted_exit && !held)
            {
                const auto &field = state.battlefield;
                const auto to_exit = edge_steps(field, wanted_exit);
                for (const auto &option : session.legal_commands())
                {
                    if (option.verb == "flee" && edge_side(field, actor->cell) == wanted_exit)
                    {
                        held = option;
                        break;
                    }
                    if (option.verb == "move" &&
                        to_exit(option.destination) < to_exit(actor->cell) &&
                        (!held || to_exit(option.destination) < to_exit(held->destination)))
                    {
                        held = option;
                    }
                }
            }
            auto chosen =
                held     ? *held
                : member ? opengold::choose_quick_command(
                               session, tactic != 1 || actor->name.starts_with("magic_user"),
                               opengold::Searching::everyone)
                         : opengold::choose_demo_command(session, opengold::Searching::everyone,
                                                         opengold::default_difficulty);
            // A Secret or Pools of Darkness player casts no spell on iron golems that
            // heals them or does nothing to them (wasted_on_golems), as the
            // Quick computer would, whatever its tactic: the member fights
            // with its weapon instead.
            const bool iron_golems = game == GoldBoxGame::pools_of_darkness ||
                                     game == GoldBoxGame::secret_of_the_silver_blades;
            if (member && iron_golems && wasted_on_golems(state, chosen))
            {
                chosen =
                    opengold::choose_quick_command(session, false, opengold::Searching::everyone);
                // Lightning slows an iron golem (EffIronGolem): a member with
                // Lightning Bolt casts it at one instead.
                if (const auto bolt = lightning_at_golems(session))
                {
                    chosen = *bolt;
                }
            }
            else if (member && iron_golems && chosen.verb == "aim")
            {
                if (const auto bolt = lightning_at_golems(session); bolt && bolt->verb == "aim")
                {
                    chosen = *bolt;
                }
            }
            if (std::getenv("OPENGOLD_FIGHT_CELLS") && member && command < 80)
            {
                const auto numbers =
                    opengold::original::Combatant::decode(actor->persistent.resources);
                std::cout << "  command: " << actor->name << " " << chosen.verb << " "
                          << chosen.label << " target=" << chosen.target
                          << " weapon=" << (numbers ? numbers->weapon_type : -1)
                          << " plus=" << (numbers && numbers->striker ? numbers->striker->plus : 0)
                          << "\n";
            }
            if (!combat.submit(chosen))
            {
                ++phases["fight stopped at " + chosen.verb];
                break;
            }
        }
        // A fight where every turn is a guard, an enemy left standing where
        // Quick finds no way to it (Treasures' sorceresses, who never step,
        // behind the council's walls), is played on as a player plays it:
        // each member walks the way round to the nearest enemy standing and
        // fights it there. It is counted.
        if (!running && combat.combat().snapshot().outcome == opengold::rules::Outcome::ongoing)
        {
            ++phases["stalemates played on by walking round"];
            // OPENGOLD_FIGHT_CELLS tells who stands where as it begins.
            if (std::getenv("OPENGOLD_FIGHT_CELLS"))
            {
                std::cout << "  stalemate:";
                for (const auto &unit : combat.combat().snapshot().combatants)
                {
                    if (unit.conscious)
                    {
                        std::cout << " " << unit.name << "@" << unit.cell.x << "," << unit.cell.y;
                    }
                }
                std::cout << "\n";
            }
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
                std::vector<opengold::rules::Cell> enemies;
                for (const auto &unit : state.combatants)
                {
                    if (unit.side == opengold::rules::Side::opposition && unit.conscious)
                    {
                        enemies.push_back(unit.cell);
                    }
                }
                std::optional<opengold::rules::Command> chosen;
                if (member)
                {
                    const auto to_enemy = steps_from(state.battlefield, enemies, true);
                    if (std::getenv("OPENGOLD_FIGHT_CELLS") && command < 40)
                    {
                        std::cout << "  hunting: " << actor->name << "@" << actor->cell.x << ","
                                  << actor->cell.y << " " << to_enemy(actor->cell) << " steps, "
                                  << actor->status << "\n";
                    }
                    const auto options = fighting.legal_commands();
                    chosen = [&]() -> std::optional<opengold::rules::Command>
                    {
                        for (const auto &option : options)
                        {
                            if (option.verb == "attack" || option.verb == "ranged")
                            {
                                return option;
                            }
                        }
                        std::optional<opengold::rules::Command> step;
                        for (const auto &option : options)
                        {
                            if (option.verb == "move" &&
                                to_enemy(option.destination) < to_enemy(actor->cell) &&
                                (!step ||
                                 to_enemy(option.destination) < to_enemy(step->destination)))
                            {
                                step = option;
                            }
                        }
                        return step;
                    }();
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
        // A fight no side can finish still (the monsters stand where no one
        // can reach) is left as a player leaves it: each member walks to the
        // field's edge and flees. It is counted.
        if (combat.combat().snapshot().outcome == opengold::rules::Outcome::ongoing)
        {
            ++phases["stalemates left by fleeing"];
            const auto &field = session.pending_encounter()->field.geometry;
            const auto to_edge = edge_steps(field, std::nullopt);
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
    // OPENGOLD_ROUTE_PLAY_STAND_INS plays a stand-in's fights as any other,
    // the test winning none (a planning aid).
    const bool playing_stand_ins = std::getenv("OPENGOLD_ROUTE_PLAY_STAND_INS") != nullptr;
    // The beat being played keeps every enemy from fleeing (RouteBeat::holding_foes).
    bool holding_foes = false;
    // The tries the beat being played reloads for while no try has won it
    // (RouteBeat::reloads_until_won).
    unsigned reloads_until_won = 0;
    // The reloads of the beat being played (RouteBeat::reloads): its fights'
    // tries play dice of their own, as a player's reload rolls them afresh.
    unsigned beat_reloads = 0;
    // A planning aid (OPENGOLD_SECRET_FIND) wins every fight at once.
    bool planning = false;
    // Fights played so far (a camp counts the ones that interrupt it), and
    // those whose kept play lost a member or the fight.
    unsigned fights_fought = 0, fights_lost = 0, fights_defeated = 0;
    bool stop_after_four_patrols = false;
    bool rejected_boss_win = false;
    // The maze exits the beat being played has yet to take (RouteBeat::maze_exits).
    std::deque<unsigned> maze_exits;
    const auto fight = [&]
    {
        ++fights_fought;
        const auto before = party->state();
        wanted_exit.reset();
        if (session.pending_encounter()->escape_ends_fight && !maze_exits.empty())
        {
            wanted_exit = maze_exits.front();
            maze_exits.pop_front();
        }
        const auto boss_still_stands = [&](const opengold::rules::Snapshot &result)
        {
            return game == GoldBoxGame::pools_of_darkness &&
                   std::ranges::any_of(result.combatants,
                                       [](const opengold::rules::CombatantView &unit)
                                       {
                                           return unit.side == opengold::rules::Side::opposition &&
                                                  unit.name.starts_with("GOTHMENES") &&
                                                  !unit.dead && unit.hit_points > 0;
                                       });
        };
        const auto score = [&](const opengold::rules::Snapshot &result)
        {
            int lost =
                result.outcome == opengold::rules::Outcome::victory && !boss_still_stands(result)
                    ? 0
                    : 1'000'000;
            for (const auto &unit : result.combatants)
            {
                if (unit.side == opengold::rules::Side::party)
                {
                    // A member turned to stone is as lost as a dead one, and
                    // so is one left on the field the party fled.
                    const bool left_behind =
                        result.outcome == opengold::rules::Outcome::fled && !unit.fled;
                    lost += (unit.dead || unit.status == "stoned" || left_behind ? 1000 : 0) +
                            unit.max_hit_points - std::max(unit.hit_points, 0);
                }
                // An enemy that got away is a fight lost where its flight
                // raises the alarm.
                if (holding_foes && unit.side == opengold::rules::Side::opposition && unit.fled)
                {
                    lost += 1'000'000;
                }
            }
            // A maze fight left by another side leads elsewhere.
            if (wanted_exit && result.escape_side != wanted_exit)
            {
                lost += 1'000'000;
            }
            return lost;
        };
        // Try t plays seed 7 + t / 3 with tactic t % 3: everyone casting, the
        // magic-user alone casting, or a player's own choices (player_turn).
        // OPENGOLD_FIGHT_TACTIC=N plays every try with tactic N, to see
        // how one tactic fares (a planning aid), each try with dice of its own.
        // A fight's dice are its seed alone, not the beat's (beat_dice_seed
        // reseeds only the scripts and the party), so a beat's reload r moves
        // its tries on to seeds of their own, 10,000 a reload (past any try
        // count): otherwise a fight the scripts' dice do not change (Gateway's
        // Star Mounts) would play the same dice at every reload.
        const char *forced_tactic = std::getenv("OPENGOLD_FIGHT_TACTIC");
        const bool darkness_player = route && game == GoldBoxGame::pools_of_darkness;
        const auto tactic_of = [&](unsigned t)
        {
            return forced_tactic     ? static_cast<unsigned>(std::stoul(forced_tactic))
                   : darkness_player ? 2U
                                     : t % 3;
        };
        const std::uint64_t first_seed = 7 + std::uint64_t{beat_reloads} * 10'000;
        const auto play_try = [&](unsigned t)
        {
            // Darkness uses its verified manual plan by default, with the
            // same reload dice as the explicit tactic-2 diagnostic.
            return fight_once(first_seed + (forced_tactic || darkness_player ? t : t / 3),
                              tactic_of(t));
        };
        // OPENGOLD_FIGHT_SCORES plays every try and prints each one's score
        // (a planning aid, to see how often a tactic wins).
        const bool scoring = std::getenv("OPENGOLD_FIGHT_SCORES") != nullptr;
        const auto print_score = [&](unsigned t, const opengold::rules::Snapshot &played)
        {
            if (scoring)
            {
                const auto tactic = tactic_of(t);
                std::cout << "  try " << t << " (tactic " << tactic << "): score " << score(played)
                          << "\n";
            }
        };
        unsigned last = 0, best_try = 0;
        auto result = planning ? opengold::rules::Snapshot{} : play_try(0);
        auto best = planning ? 1 : score(result);
        print_score(0, result);
        // OPENGOLD_FIGHT_TRIES=N plays N tries instead of 120, to measure a
        // tactic's chances over more dice (a planning aid). A beat that
        // reloads until won (RouteBeat::reloads_until_won) plays on past them, up to its
        // count more, while no try has won.
        const char *tries = std::getenv("OPENGOLD_FIGHT_TRIES");
        const unsigned try_count = tries ? static_cast<unsigned>(std::stoul(tries)) : 120;
        constexpr int lost_fight = 1'000'000, one_member_lost = 1000, two_members_lost = 2000;
        // Pools of Darkness's player also reloads a won fight that cost two
        // members or more, up to 240 more plays, as both clerics lost leave
        // no one to raise the dead: since its monsters come from the sides
        // their SETUP MONSTER names, the best of 120 plays of some fights
        // (Thorne's dragons, the drow temple's statue) cost both. Where the
        // dead cannot be raised before the beat ends (the finale's Xoham,
        // whose script runs on to the end), one member lost is reloaded too.
        // Secret's player also avoids losing one member: the only magic-user
        // turned to stone cannot cast Stone to Flesh, and the next hard fight
        // can be unwinnable before the party reaches the town's temple again.
        const unsigned costly_reloads = game == GoldBoxGame::pools_of_darkness ||
                                                game == GoldBoxGame::secret_of_the_silver_blades
                                            ? 240
                                            : 0;
        const bool cannot_raise_between =
            game == GoldBoxGame::pools_of_darkness && keeps_preparation;
        const int costly_score =
            test_winning || cannot_raise_between || game == GoldBoxGame::secret_of_the_silver_blades
                ? one_member_lost
                : two_members_lost;
        while (route && !planning && (best > 0 || scoring) &&
               (++last < try_count ||
                (best >= lost_fight && last < try_count + reloads_until_won) ||
                (best >= costly_score && best < lost_fight && last < try_count + costly_reloads)))
        {
            party->restore(before);
            result = play_try(last);
            print_score(last, result);
            if (score(result) < best)
            {
                best = score(result);
                best_try = last;
            }
        }
        // OPENGOLD_FIGHT_KEEP=N keeps try N instead, to see a lost try's log.
        if (const char *keep = std::getenv("OPENGOLD_FIGHT_KEEP"))
        {
            best_try = static_cast<unsigned>(std::stoul(keep));
        }
        if (last != best_try && route && !planning)
        {
            party->restore(before);
            result = play_try(best_try);
        }
        // OPENGOLD_FIGHT_LOG prints the kept play's log, to see why a fight goes
        // as it does (the play's, where the test then wins it).
        if (std::getenv("OPENGOLD_FIGHT_LOG"))
        {
            for (const auto &entry : result.log_entries)
            {
                std::cout << "    | " << entry.english << '\n';
            }
        }
        // Where the test wins fights, it also wins one the best play wins
        // only at the cost of a member dead or turned to stone: the party
        // would be lost on the way (Secret's and Pools of Darkness's routes'
        // stand-in; Darkness's Vaasans leave most of the party dead; in
        // Treasures' lair the dwarves win once the black-robed men have
        // killed every member).
        const bool costly =
            test_winning &&
            (game == GoldBoxGame::secret_of_the_silver_blades ||
             game == GoldBoxGame::pools_of_darkness ||
             game == GoldBoxGame::treasures_of_the_savage_frontier) &&
            std::ranges::any_of(result.combatants,
                                [](const opengold::rules::CombatantView &unit)
                                {
                                    return unit.side == opengold::rules::Side::party &&
                                           !opengold::is_ally(unit.id) &&
                                           (unit.dead || unit.status == "stoned");
                                });
        if ((test_winning || planning) && (result.outcome != opengold::rules::Outcome::victory ||
                                           costly || boss_still_stands(result)))
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
        if (result.outcome == opengold::rules::Outcome::victory && boss_still_stands(result))
        {
            // A quiet fight is a victory under the original rules, but it
            // does not establish that the finale's boss can be defeated.
            // Reject that proof and reload the whole chained event.
            rejected_boss_win = true;
            ++fights_lost;
            ++phases["living Gothmenes rejected"];
            std::cout << "  Gothmenes is still standing; reload the finale.\n";
            return false;
        }
        ++phases["fights ended " + std::to_string(static_cast<int>(result.outcome))];
        constexpr int a_member_lost = 1000, the_fight_lost = 1'000'000;
        if (result.outcome == opengold::rules::Outcome::defeat)
        {
            ++fights_defeated;
        }
        if (result.outcome == opengold::rules::Outcome::defeat ||
            score(result) % the_fight_lost >= a_member_lost)
        {
            ++fights_lost;
        }
        return session.resolve_combat(result);
    };
    std::size_t diagnosed = 0;
    std::vector<std::string> answers;
    // The survey declines every offer (the last choice), so it does not walk
    // into a trap or surrender.
    bool declining = false;
    // The beat being played takes the first wand a treasure holds (RouteBeat::take_wand).
    bool taking_wand = false;
    // The wand it takes stays packed (RouteBeat::pack_wand).
    bool packing_wand = false;
    // The items the beat being played takes from a treasure, and who takes
    // each (RouteBeat::take_items).
    std::vector<std::pair<std::string, std::string>> items_to_take;
    // Whether the closing words of a won game were shown (game_won).
    bool game_won_shown = false;
    // The pieces of music the scripts started (TourSnapshot::music), and the
    // starts already counted.
    std::vector<unsigned> music_heard;
    unsigned music_starts_seen = 0;
    // The beat being played only passes by (RouteBeat::passing).
    bool passing = false;
    // The beat being played avoids the monsters it meets (RouteBeat::avoiding).
    bool avoiding = false;
    // Readies the member's item, putting away the one item, or else the two,
    // whose place it takes (quarrels where bolts go, a sword and shield where
    // a crossbow goes), as a player readies gear.
    const auto ready_in_place = [&](opengold::MemberId taker, std::uint64_t taken)
    {
        try
        {
            party->equip(taker, taken);
        }
        catch (const std::exception &)
        {
            // Something readied is in its place (quarrels where bolts go,
            // a sword and shield where a crossbow goes): the member puts
            // away the one item, or else the two, whose place it takes.
            auto readied = std::vector(party->member(taker).equipped);
            // Secret's player puts away a shield or a mirror (its types 37
            // and 48) before a weapon, as a mirror or a shield takes the
            // other's hand.
            if (game == GoldBoxGame::secret_of_the_silver_blades)
            {
                const auto &inventory = party->member(taker).character.inventory();
                std::ranges::stable_partition(
                    readied,
                    [&](std::uint64_t item)
                    {
                        const auto entry = inventory.find(item);
                        return entry && (entry->get().original_type == shield_type ||
                                         entry->get().original_type == mirror_type);
                    });
            }
            const auto fits_without = [&](std::vector<std::uint64_t> away)
            {
                for (const auto item : away)
                {
                    party->unequip(taker, item);
                }
                try
                {
                    party->equip(taker, taken);
                    return true;
                }
                catch (const std::exception &)
                {
                    for (const auto item : away)
                    {
                        party->equip(taker, item);
                    }
                    return false;
                }
            };
            bool fitted = false;
            for (std::size_t a = 0; a < readied.size() && !fitted; ++a)
            {
                fitted = fits_without({readied[a]});
            }
            for (std::size_t a = 0; a < readied.size() && !fitted; ++a)
            {
                for (std::size_t b = a + 1; b < readied.size() && !fitted; ++b)
                {
                    fitted = fits_without({readied[a], readied[b]});
                }
            }
        }
    };
    // Takes the first of the beat's items still to take that the treasure
    // holds, for the member named with it, and readies it, as a player gears
    // the party: true when one was taken.
    const auto take_named_item = [&](const std::vector<opengold::por::Equipment> &pile)
    {
        const auto upper = [](std::string text)
        {
            std::ranges::transform(text, text.begin(), ::toupper);
            return text;
        };
        const auto slots = party->state().slots;
        for (auto wanted = items_to_take.begin(); wanted != items_to_take.end(); ++wanted)
        {
            const auto item = std::ranges::find_if(
                pile,
                [&](const opengold::por::Equipment &it)
                {
                    return upper(it.label()).find(upper(wanted->first)) != std::string::npos;
                });
            const auto taker = std::ranges::find_if(
                slots,
                [&](opengold::MemberId id)
                {
                    return id && party->member(id).character.sheet().name == wanted->second;
                });
            if (item == pile.end() || taker == slots.end())
            {
                continue;
            }
            const auto item_name = item->label();
            party->select(opengold::PartySlot{static_cast<unsigned>(taker - slots.begin())});
            if (!session.take_treasure(session.snapshot().continue_ticket,
                                       static_cast<std::size_t>(item - pile.begin())))
            {
                continue;
            }
            const auto taken = party->member(*taker).character.inventory().items().back().id;
            ready_in_place(*taker, taken);
            // Like stacks are joined into the one readied (the items menu's
            // JOIN), as a player joins the blessed bolts so that a fight
            // shoots them all.
            const auto &holder = party->member(*taker);
            const auto definition =
                holder.character.inventory().find(taken)
                    ? holder.character.inventory().find(taken)->get().definition_id
                    : std::string{};
            std::uint64_t joined = taken;
            for (const auto readied : holder.equipped)
            {
                const auto entry = holder.character.inventory().find(readied);
                if (entry && entry->get().definition_id == definition)
                {
                    joined = readied;
                }
            }
            try
            {
                party->join_like_items(*taker, joined);
            }
            catch (const std::exception &)
            {
            }
            std::cout << "  " << wanted->second << " takes and readies " << item_name << "\n";
            items_to_take.erase(wanted);
            return true;
        }
        return false;
    };
    // Pools of Darkness's route player wears the protection the treasures
    // offer, as a player identifies what the party finds and keeps the best:
    // a Ring (item type 59) or Cloak of Protection (75) goes to the member
    // whose protection it raises most, in place of a worse one of its kind
    // (two rings a member). Companions, who leave, take none. True when one
    // was taken.
    const auto take_protection = [&](const std::vector<opengold::por::Equipment> &pile)
    {
        // Keep the Aerie's protection scroll for the finale's lightning
        // breaths. It remains packed, so Quick cannot spend it earlier.
        const auto scroll =
            std::ranges::find_if(pile,
                                 [](const opengold::por::Equipment &item)
                                 {
                                     return (item.stored.effect_codes[1] & 0x7F) == 95 &&
                                            item.stored.effect_codes[2] < 0x80;
                                 });
        const auto &members = party->state().slots;
        const auto protector = std::ranges::find_if(
            members,
            [&](opengold::MemberId id)
            {
                return id && party->member(id).character.sheet().name == "paladin 1";
            });
        if (scroll != pile.end() && protector != members.end())
        {
            party->select(opengold::PartySlot{static_cast<unsigned>(protector - members.begin())});
            const auto label = scroll->label();
            if (session.take_treasure(session.snapshot().continue_ticket,
                                      static_cast<std::size_t>(scroll - pile.begin())))
            {
                std::cout << "  paladin 1 takes and packs " << label << " for the finale\n";
                return true;
            }
        }
        // Preserve found missiles for shielded enemies in the no-magic
        // finale. These are real treasure stacks, kept packed until needed.
        for (std::size_t at = 0; at < pile.size(); ++at)
        {
            const auto &offered = pile[at].stored;
            if ((offered.type != 30 && offered.type != 12) || offered.magic_bonus < 1)
            {
                continue;
            }
            std::optional<std::size_t> taker;
            unsigned least = 100;
            for (std::size_t slot = 0; slot < members.size(); ++slot)
            {
                const auto id = members[slot];
                if (!id || party->member(id).vitals.dead ||
                    !(party->member(id).character.sheet().name.starts_with("paladin") ||
                      party->member(id).character.sheet().name.starts_with("ranger")))
                {
                    continue;
                }
                unsigned count = 0;
                for (const auto &item : party->member(id).character.inventory().items())
                {
                    const auto source = party->member(id).item_sources.find(item.id);
                    if (item.original_type == offered.type &&
                        source != party->member(id).item_sources.end() &&
                        source->second.stored.magic_bonus >= offered.magic_bonus)
                    {
                        count += item.quantity;
                    }
                }
                if (count < least)
                {
                    least = count;
                    taker = slot;
                }
            }
            if (!taker)
            {
                continue;
            }
            const auto id = members[*taker];
            const auto label = pile[at].label();
            party->select(opengold::PartySlot{static_cast<unsigned>(*taker)});
            if (session.take_treasure(session.snapshot().continue_ticket, at))
            {
                const auto taken = party->member(id).character.inventory().items().back().id;
                party->join_like_items(id, taken);
                std::cout << "  " << party->member(id).character.sheet().name << " takes and packs "
                          << label << " for shielded enemies\n";
                return true;
            }
        }
        // Equip upgrades actually found along the route. Leaving the +4
        // swords and maces behind made the party fight the finale with its
        // original +3 equipment despite having earned better weapons.
        for (std::size_t at = 0; at < pile.size(); ++at)
        {
            const auto &offered = pile[at].stored;
            const int type = offered.type;
            if (type != 18 && type != 8 && type != 15 && type != 36 && type != 37)
            {
                continue;
            }
            std::optional<std::size_t> taker;
            int best_gain = 0;
            for (std::size_t slot = 0; slot < members.size(); ++slot)
            {
                const auto id = members[slot];
                if (!id || party->member(id).vitals.dead || !party->member(id).npc_source.empty())
                {
                    continue;
                }
                const auto &holder = party->member(id);
                int best = -1;
                for (const auto &item : holder.character.inventory().items())
                {
                    if (item.original_type == type)
                    {
                        const auto source = holder.item_sources.find(item.id);
                        if (source != holder.item_sources.end())
                        {
                            best =
                                std::max(best, static_cast<int>(source->second.stored.magic_bonus));
                        }
                    }
                }
                // An upgrade replaces a type this class already carries.
                // Do not give a mage plate or a cleric a sword.
                const int gain = best < 0 ? 0 : offered.magic_bonus - best;
                if (gain > best_gain)
                {
                    best_gain = gain;
                    taker = slot;
                }
            }
            if (taker)
            {
                const auto id = members[*taker];
                const auto label = pile[at].label();
                party->select(opengold::PartySlot{static_cast<unsigned>(*taker)});
                if (session.take_treasure(session.snapshot().continue_ticket, at))
                {
                    const auto taken = party->member(id).character.inventory().items().back().id;
                    ready_in_place(id, taken);
                    std::cout << "  " << party->member(id).character.sheet().name
                              << " takes and readies an upgrade: " << label << "\n";
                    return true;
                }
            }
        }
        constexpr std::uint8_t ring_of_protection = 59, cloak_of_protection = 75;
        constexpr std::size_t rings_worn = 2;
        const auto slots = party->state().slots;
        // What the member wears: the bonus and item of each of `type`, worst
        // first, and whether its ring and cloak places are taken.
        struct Worn
        {
            std::vector<std::pair<int, std::uint64_t>> of_type;
            std::size_t rings = 0;
            bool cloak = false;
        };
        const auto worn = [&](opengold::MemberId id, std::uint8_t type)
        {
            Worn found;
            const auto &member = party->member(id);
            for (const auto item : member.equipped)
            {
                const auto source = member.item_sources.find(item);
                if (source == member.item_sources.end())
                {
                    continue;
                }
                const auto &stored = source->second.stored;
                if (stored.type == type)
                {
                    found.of_type.emplace_back(stored.magic_bonus, item);
                }
                const auto label = source->second.label();
                found.rings += label.starts_with("Ring") && !label.starts_with("Ring Mail");
                found.cloak = found.cloak || label.starts_with("Cloak");
            }
            std::ranges::sort(found.of_type);
            return found;
        };
        for (std::size_t at = 0; at < pile.size(); ++at)
        {
            const auto &offered = pile[at].stored;
            const int bonus = offered.magic_bonus;
            if ((offered.type != ring_of_protection && offered.type != cloak_of_protection) ||
                bonus <= 0)
            {
                continue;
            }
            int best_gain = 0;
            std::size_t best_slot = slots.size();
            std::optional<std::uint64_t> replaced;
            for (std::size_t slot = 0; slot < slots.size(); ++slot)
            {
                const auto id = slots[slot];
                if (!id || party->member(id).vitals.dead || !party->member(id).npc_source.empty())
                {
                    continue;
                }
                const auto now = worn(id, offered.type);
                int gain = 0;
                std::optional<std::uint64_t> worse;
                if ((offered.type == ring_of_protection && now.rings < rings_worn) ||
                    (offered.type == cloak_of_protection && !now.cloak))
                {
                    gain = bonus;
                }
                else if (!now.of_type.empty())
                {
                    gain = bonus - now.of_type.front().first;
                    worse = now.of_type.front().second;
                }
                if (gain > best_gain)
                {
                    best_gain = gain;
                    best_slot = slot;
                    replaced = worse;
                }
            }
            if (best_slot == slots.size())
            {
                continue;
            }
            const auto taker = slots[best_slot];
            const auto item_name = pile[at].label();
            party->select(opengold::PartySlot{static_cast<unsigned>(best_slot)});
            if (!session.take_treasure(session.snapshot().continue_ticket, at))
            {
                continue;
            }
            const auto taken = party->member(taker).character.inventory().items().back().id;
            if (replaced)
            {
                party->unequip(taker, *replaced);
            }
            ready_in_place(taker, taken);
            std::cout << "  " << party->member(taker).character.sheet().name << " takes and wears "
                      << item_name << " +" << bonus << "\n";
            return true;
        }
        return false;
    };
    bool reload_camp_on_attack = false;
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
            if (s.music_starts != music_starts_seen)
            {
                music_starts_seen = s.music_starts;
                if (s.music)
                {
                    music_heard.push_back(*s.music);
                    std::cout << "  music: piece " << *s.music << " at " << where << "\n";
                }
            }
            switch (s.phase)
            {
            case TourPhase::running:
                // A camp can reach its spell choices only after a scene or
                // interruption has played. Finish those choices before
                // asking the waiting camp to advance again.
                if (party->state().spell_rest)
                {
                    memorize();
                }
                session.advance(1.0);
                continue;
            case TourPhase::awaiting_continue:
            {
                game_won_shown = game_won_shown || s.game_won;
                said = s.dialogue;
                if (game == GoldBoxGame::pools_of_darkness && keeps_preparation &&
                    s.dialogue.find("AGREE?") != std::string::npos && shown[s.dialogue] == 0)
                {
                    // The inventory's Use remains available during dialogue.
                    // Save the twelve-minute scroll for the no-magic battle,
                    // reading it before the Balor bars magic.
                    check(!session.magic_barred(), "The protection scroll precedes antiMagic");
                    bool used = false;
                    for (const auto id : party->state().slots)
                    {
                        if (!id || party->member(id).vitals.dead ||
                            party->member(id).vitals.hit_points <= 0 ||
                            party->member(id).character.sheet().name != "paladin 1")
                        {
                            continue;
                        }
                        const auto &member = party->member(id);
                        const auto &items = member.character.inventory().items();
                        const auto scroll = std::ranges::find_if(
                            items,
                            [&](const opengold::InventoryItem &item)
                            {
                                const auto source = member.item_sources.find(item.id);
                                return source != member.item_sources.end() &&
                                       (source->second.stored.effect_codes[1] & 0x7F) == 95 &&
                                       party->rule_module().usable_outside_combat(
                                           item.definition_id);
                            });
                        if (scroll != items.end())
                        {
                            party->select(opengold::PartySlot{0});
                            party->drink_potion(id, scroll->id);
                            std::cout
                                << "  paladin 1 uses the collected protection scroll before Gothmenes bars magic\n";
                            used = true;
                        }
                    }
                    check(used, "The paladin reads the actually collected protection scroll");
                }
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
                // Treasures' party walking the overland flees the bands that
                // fall on it (sixteen fire giants and their mages after
                // Mirabar), as a player hurries on to the next town.
                else if (game == GoldBoxGame::treasures_of_the_savage_frontier && fleeing &&
                         where != "camp" && flee != s.choices.end())
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
                    // Pools of Darkness's magic-user takes the strongest of
                    // its new levels first: only the eighth and ninth touch
                    // its rakshasas.
                    constexpr std::array<std::string_view, 7> darkness_first{
                        "Meteor Swarm",
                        "Mind Blank",
                        "Mass Charm",
                        "Power Word Kill",
                        "Power Word Blind",
                        "Delayed Blast Fireball",
                        "Globe of Invulnerability"};
                    // Secret's magic-user takes a sixth-level spell when it
                    // reaches them: the Globe against the storm giants'
                    // bolts (the cluebook's advice), Disintegrate against
                    // the Black Circle's lords.
                    constexpr std::array<std::string_view, 4> secret_first{
                        "Mass Invisibility", "Globe of Invulnerability", "Disintegrate",
                        "Stone to Flesh"};
                    std::vector<std::string_view> wanted(learned.begin(), learned.end());
                    if (game == GoldBoxGame::pools_of_darkness)
                    {
                        wanted.insert(wanted.begin(), darkness_first.begin(), darkness_first.end());
                    }
                    if (game == GoldBoxGame::secret_of_the_silver_blades)
                    {
                        wanted.insert(wanted.begin(), secret_first.begin(), secret_first.end());
                    }
                    for (const auto name : wanted)
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
                // Wake living casualties with paid temple cures before
                // requesting training, which requires consciousness.
                if (!answers.empty() && answers.front() == "cure:unconscious" &&
                    std::ranges::any_of(s.choices,
                                        [](const std::string &choice)
                                        {
                                            return choice.starts_with("Cure Light Wounds");
                                        }))
                {
                    answers.erase(answers.begin());
                    for (const auto id : party->state().slots)
                    {
                        if (id && !party->member(id).vitals.dead &&
                            party->member(id).vitals.hit_points <= 0)
                        {
                            answers.insert(answers.begin(),
                                           {"Cure Light Wounds",
                                            party->member(id).character.sheet().name, "Pay",
                                            "cure:unconscious"});
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
                constexpr std::array<std::pair<std::string_view, std::string_view>, 12> riddles{
                    {{"YOU FEEL IT, YOU NEVER SAW IT", "YOUR HEART"},
                     {"YOU MUST KEEP IT AFTER GIVING IT", "YOUR WORD"},
                     {"LIGHT AS A FEATHER", "YOUR BREATH"},
                     {"HAS A MOUTH BUT DOES NOT SPEAK", "RIVER"},
                     {"RUNS SMOOTHER THAN ANY RHYME", "WATER"},
                     {"YOU BREAK IT IF YOU EVEN NAME IT", "SILENCE"},
                     {"PASSES BEFORE THE SUN", "WIND"},
                     {"YOU FEED IT, IT LIVES", "FIRE"},
                     {"WHAT IS THE SECRET WORD?", "NACACIA"},
                     // The castle's: the iron golems' password (where a
                     // magic mouth names a false one too), and the
                     // Dreadlord's door, which asks for his brother.
                     {"IRON GOLEMS APPEARS", "STEELEYE"},
                     {"THE PASSWORD HERE IS FOOL", "STEELEYE"},
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
                if (trace)
                {
                    std::cout << where << ": the shop sells";
                    for (const auto &offered : session.shop_stock())
                    {
                        std::cout << " " << offered.label() << ";";
                    }
                    std::cout << " (the party has " << party->purse_gold() << " gold)\n";
                }
                // Identify carried items through the real paid shop service,
                // then choose the revealed wand in a later ready action.
                while (!answers.empty() && answers.front().starts_with("identify:"))
                {
                    const auto order = answers.front().substr(9);
                    answers.erase(answers.begin());
                    const auto at = order.find('@');
                    check(at != std::string::npos, "Identification names its holder");
                    const auto wanted = order.substr(0, at);
                    const auto holder = order.substr(at + 1);
                    const auto member = std::ranges::find_if(
                        party->state().slots,
                        [&](opengold::MemberId id)
                        {
                            return id && party->member(id).character.sheet().name == holder;
                        });
                    check(member != party->state().slots.end(), "The holder visits the real shop");
                    const auto carried = party->member(*member).character.inventory().items();
                    const std::vector<opengold::InventoryItem> items(carried.begin(),
                                                                     carried.end());
                    bool found = false;
                    for (const auto &item : items)
                    {
                        auto name = item.name;
                        std::ranges::transform(name, name.begin(), ::toupper);
                        if (name.find(wanted) == std::string::npos)
                        {
                            continue;
                        }
                        found = true;
                        if (!party->member(*member).item_sources.at(item.id).identified())
                        {
                            check(party->identify_at_shop(*member, item.id),
                                  "Pay the shop's identification fee");
                            std::cout << "  " << holder << " pays to identify "
                                      << party->member(*member).item_sources.at(item.id).label()
                                      << "\n";
                        }
                    }
                    check(found, "Identification uses actual carried items");
                }
                // Selling loot uses the same ID, quote and sale
                // services as the inventory's controls inside a real shop.
                while (!answers.empty() && answers.front().starts_with("sell:"))
                {
                    const auto order = answers.front().substr(5);
                    answers.erase(answers.begin());
                    const auto at = order.find('@');
                    check(at != std::string::npos, "A sale names its holder");
                    const auto wanted = order.substr(0, at);
                    const auto holder = order.substr(at + 1);
                    const auto member = std::ranges::find_if(
                        party->state().slots,
                        [&](opengold::MemberId id)
                        {
                            return id && party->member(id).character.sheet().name == holder;
                        });
                    check(member != party->state().slots.end(), "The seller is in the party");
                    const auto id = *member;
                    const auto &carried = party->member(id);
                    const auto &items = carried.character.inventory().items();
                    const auto item = std::ranges::find_if(
                        items,
                        [&](const opengold::InventoryItem &entry)
                        {
                            auto name = entry.name;
                            std::ranges::transform(name, name.begin(), ::toupper);
                            return name.find(wanted) != std::string::npos;
                        });
                    check(item != items.end(), "The sale uses an actual carried item");
                    const auto item_id = item->id;
                    const auto &source = carried.item_sources.at(item_id);
                    if (!source.identified())
                    {
                        check(party->identify_at_shop(id, item_id),
                              "Pay to identify the sale item");
                    }
                    const opengold::SaleMarket market{session.shop_stock(), session.town_stock()};
                    const auto quote = party->sale_offer(id, item_id, market);
                    check(quote > 0 && party->sell(id, item_id, market) == quote,
                          "The real shop pays its quoted sale price");
                    std::cout << "  " << holder << " sells " << wanted << " for " << quote
                              << " gp\n";
                }
                // "buy:NAME" buys each member an item whose name holds NAME
                // and readies it, as a player outfits the party (Secret's
                // armoury sells the mirrors that turn gazes back);
                // "buy:NAME@MEMBER,MEMBER" buys one only for the members named.
                // Answers that buy follow one another in one visit.
                while (!answers.empty() && answers.front().starts_with("buy:"))
                {
                    const auto order = answers.front().substr(4);
                    answers.erase(answers.begin());
                    const auto at = order.find('@');
                    const auto wanted = order.substr(0, at);
                    const auto buyers =
                        at == std::string::npos ? std::string{} : "," + order.substr(at + 1) + ",";
                    const auto slots = party->state().slots;
                    for (std::size_t slot = 0; slot < slots.size(); ++slot)
                    {
                        if (slots[slot] && !buyers.empty() &&
                            buyers.find("," + party->member(slots[slot]).character.sheet().name +
                                        ",") == std::string::npos)
                        {
                            continue;
                        }
                        const auto &stock = session.shop_stock();
                        const auto item = std::ranges::find_if(
                            stock,
                            [&](const opengold::por::Equipment &offered)
                            {
                                auto name = offered.label();
                                std::ranges::transform(name, name.begin(), ::toupper);
                                return name.find(wanted) != std::string::npos;
                            });
                        check(!sewers || !slots[slot] || item != stock.end(),
                              "The real shop stocks the sewer party's requested equipment");
                        if (!slots[slot] || item == stock.end())
                        {
                            continue;
                        }
                        party->select(opengold::PartySlot{static_cast<unsigned>(slot)});
                        const bool bought =
                            session.buy(session.snapshot().continue_ticket,
                                        static_cast<std::size_t>(item - stock.begin()));
                        check(!sewers || bought,
                              "Creation money pays for the sewer party's equipment");
                        if (bought)
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
                // OPENGOLD_ROUTE_PILES prints what each treasure holds (a
                // planning aid, to name the items a beat takes).
                if (std::getenv("OPENGOLD_ROUTE_PILES"))
                {
                    std::cout << "  pile:";
                    // An unidentified item shows its whole name and its magic
                    // bonus too.
                    for (const auto &item : pile)
                    {
                        std::cout << " [" << item.label();
                        if (!item.identified())
                        {
                            std::cout << " = " << item.decoded_name << " " << std::showpos
                                      << item.stored.magic_bonus << std::noshowpos;
                        }
                        std::cout << "]";
                    }
                    std::cout << "\n";
                }
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
                        // In place of a wand readied already (Curse's
                        // Dracandros leaves two).
                        if (!packing_wand)
                        {
                            ready_in_place(*mage, items.back().id);
                        }
                        std::cout << "  " << where << ": the magic-user takes "
                                  << (packing_wand ? "and packs " : "and readies ") << wand_name
                                  << "\n";
                        taking_wand = false;
                        continue;
                    }
                }
                if (route && take_named_item(pile))
                {
                    continue;
                }
                if (route && game == GoldBoxGame::pools_of_darkness && take_protection(pile))
                {
                    continue;
                }
                (void)session.leave_treasure(s.continue_ticket);
                continue;
            }
            case TourPhase::combat:
                if (reload_camp_on_attack)
                {
                    return;
                }
                if (session.monster_picture())
                {
                    (void)session.start_encounter();
                    continue;
                }
                // A fight that ends without a result fails the walk: a pass
                // must not hide one.
                if (!session.pending_encounter() || !fight())
                {
                    if (rejected_boss_win)
                    {
                        return;
                    }
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
        const auto &machine = opengold::test::TourAccess::machine(session);
        std::cout << "  machine state " << static_cast<int>(machine.state()) << " at "
                  << machine.address() << ", spell choices "
                  << (party->state().spell_rest ? party->state().spell_rest->members.size() : 0)
                  << "\n";
        if (game == GoldBoxGame::pools_of_darkness)
        {
            std::cout << "  camp flags " << machine.variable(opengold::por::EclAddress{0x9D}) << "/"
                      << machine.variable(opengold::por::EclAddress{0xA2}) << "\n";
        }
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
        // Secret's magic-user has cast half the spells it memorized: a
        // player passing through rests for them before the next fight.
        const auto spells_spent = [&]
        {
            if (game != GoldBoxGame::secret_of_the_silver_blades || !route)
            {
                return false;
            }
            return std::ranges::any_of(
                party->state().slots,
                [&](opengold::MemberId id)
                {
                    if (!id || party->member(id).vitals.dead ||
                        !party->member(id).character.sheet().name.starts_with("magic_user"))
                    {
                        return false;
                    }
                    const auto memorized =
                        party->member(id).character.sheet().prepared_spells.size();
                    return party->memorized_spells(id).size() * 2 < memorized;
                });
        };
        const auto party_hit_points = [&]
        {
            int total = 0;
            for (const auto id : party->state().slots)
            {
                total += id ? party->member(id).vitals.hit_points : 0;
            }
            return total;
        };
        // Where Pools of Darkness's script lets no camp rest (the Fire Giant
        // Cave, the Dragons' Aerie) or Curse's (its ruined temple), its
        // clerics raise the dead and cast their cures, the strongest first, on
        // each member below half its Hit Points, as a player would.
        const auto cure_the_hurt = [&](bool to_full = false)
        {
            if (game != GoldBoxGame::pools_of_darkness &&
                game != GoldBoxGame::curse_of_the_azure_bonds && !earned_secret)
            {
                return;
            }
            const auto cast_cure =
                [&](opengold::MemberId healer, opengold::MemberId target, const std::string &action)
            {
                if (sewers || earned_secret)
                {
                    session.cast_outside_combat(healer, target, action);
                }
                else
                {
                    party->use_camp_action(healer, target, action);
                }
            };
            for (const auto dead : party->state().slots)
            {
                for (const auto healer : party->state().slots)
                {
                    if (!dead || !healer || !party->member(dead).vitals.dead ||
                        party->member(healer).vitals.dead ||
                        party->member(healer).vitals.hit_points <= 0)
                    {
                        continue;
                    }
                    const auto actions = party->camp_actions(healer);
                    const auto raise =
                        std::ranges::find_if(actions,
                                             [](const opengold::rules::CampAction &action)
                                             {
                                                 return action.id.ends_with("resurrection") ||
                                                        action.id.ends_with("raise_dead");
                                             });
                    if (raise == actions.end())
                    {
                        continue;
                    }
                    try
                    {
                        std::cout << "  " << party->member(healer).character.sheet().name
                                  << " casts " << raise->id << " on "
                                  << party->member(dead).character.sheet().name << "\n";
                        cast_cure(healer, dead, raise->id);
                    }
                    catch (const std::exception &)
                    {
                    }
                }
            }
            // A member knocked out (0 Hit Points) is cured too, as
            // wake_the_fallen does in camp.
            const auto below_half = [&](opengold::MemberId id)
            {
                const auto &member = party->member(id);
                const int short_of =
                    to_full ? member.vitals.hit_points : member.vitals.hit_points * 2;
                return !member.vitals.dead && member.vitals.description != "Turned to stone." &&
                       short_of < member.character.sheet().hit_points;
            };
            for (const auto hurt : party->state().slots)
            {
                for (const char *cure :
                     {"heal", "cure_critical_wounds", "cure_serious_wounds", "cure_light_wounds"})
                {
                    for (const auto healer : party->state().slots)
                    {
                        while (hurt && healer && below_half(hurt) &&
                               party->member(healer).vitals.hit_points > 0)
                        {
                            // Heal leaves 1d4 damage. Spending every copy
                            // trying to repair that last scratch wastes the
                            // spells needed after the crystal drains its
                            // bearer in the finale; use smaller cures there.
                            const int missing = party->member(hurt).character.sheet().hit_points -
                                                party->member(hurt).vitals.hit_points;
                            const int worthwhile =
                                std::string_view(cure) == "heal"                   ? 28
                                : std::string_view(cure) == "cure_critical_wounds" ? 15
                                : std::string_view(cure) == "cure_serious_wounds"  ? 8
                                                                                   : 1;
                            if (missing < worthwhile)
                            {
                                break;
                            }
                            const auto actions = party->camp_actions(healer);
                            const auto found =
                                std::ranges::find_if(actions,
                                                     [&](const opengold::rules::CampAction &action)
                                                     {
                                                         return action.id.ends_with(cure);
                                                     });
                            if (found == actions.end())
                            {
                                break;
                            }
                            std::cout << "  " << party->member(healer).character.sheet().name
                                      << " casts " << found->id << " on "
                                      << party->member(hurt).character.sheet().name << "\n";
                            cast_cure(healer, hurt, found->id);
                        }
                    }
                }
            }
        };
        // A camp as the camp dialog makes it: Treasures' camp menu opens
        // first, running the map's camp entry and telling the romance's scene
        // (open_camp), and the party rests only once the menu is open.
        const auto camp_and_rest = [&]
        {
            if (session.open_camp())
            {
                play("camp menu");
                if (!session.snapshot().camp_menu_open)
                {
                    return false;
                }
            }
            return session.camp(opengold::RestKind::long_rest);
        };
        // Secret's player heals with the camp's Fix (camp.pas, CampFixMenu),
        // as the cluebook bids where rests are broken (the crevasses): it
        // rests only the hours its clerics' cures take, again while the
        // party is hurt and its clerics stand. A Fix a fight broke counts
        // towards `interrupted`; false once monsters broke six.
        const auto fix_party = [&](unsigned &interrupted)
        {
            for (int fix = 0; fix < 6 && !party_whole() && session.camp_offers_fix(); ++fix)
            {
                const auto fights_before = fights_fought;
                const auto hit_points_before = party_hit_points();
                const auto before_camp = session.detached_copy();
                const auto party_before = party->state();
                const auto reloads_before = beat_reloads;
                bool fixed = false;
                for (unsigned reload = 0; reload < 8; ++reload)
                {
                    if (reload != 0)
                    {
                        session = before_camp.detached_copy();
                        party->restore(party_before);
                        session.attach_restored_party(party);
                        session.reseed_dice(
                            static_cast<std::uint32_t>(party_before.time_minutes + reload));
                        beat_reloads = reloads_before + reload;
                        ++phases["camp attacks reloaded"];
                    }
                    if (!session.camp_fix())
                    {
                        break;
                    }
                    play("camp");
                    if (session.snapshot().phase != TourPhase::defeated)
                    {
                        fixed = true;
                        break;
                    }
                }
                beat_reloads = reloads_before;
                if (!fixed)
                {
                    return false;
                }
                std::cout << "  the party fixes: " << hit_points_before << " to "
                          << party_hit_points() << " Hit Points\n";
                if (fights_fought != fights_before && ++interrupted >= 6)
                {
                    return false;
                }
                if (fights_fought == fights_before && party_hit_points() == hit_points_before)
                {
                    break;
                }
            }
            return true;
        };
        // `a_night_first` rests a night even when the party is whole, so its
        // casters choose their spells anew (RouteBeat::rest_for_spells).
        const auto rest_until_healed = [&](bool a_night_first = false, bool through_period = false)
        {
            // Secret's party gives up resting where monsters break its camp
            // six times (its castle's guards come every night), and on that
            // map rests no more while it only passes by. A broken rest keeps
            // the days and spells its time gave (2026-11-15), so the party
            // rests on through more of them.
            unsigned interrupted = 0;
            unsigned idle_camps = 0;
            // Secret's party Fixes first, then rests a night for its spells.
            if (game == GoldBoxGame::secret_of_the_silver_blades && route && !party_whole())
            {
                if (!fix_party(interrupted))
                {
                    restless_map = session.snapshot().area_id;
                    return;
                }
                a_night_first = true;
            }
            for (int day = 0; day < 90 && session.can_leave(); ++day)
            {
                const auto copies_ready = [&](std::string_view wanted)
                {
                    unsigned count = 0;
                    for (const auto id : party->state().slots)
                    {
                        if (id && !party->member(id).vitals.dead)
                        {
                            count += static_cast<unsigned>(
                                std::ranges::count_if(party->memorized_spells(id),
                                                      [&](const std::string &spell)
                                                      {
                                                          return spell_named(spell, wanted);
                                                      }));
                        }
                    }
                    return count;
                };
                const bool missing_plan =
                    (game == GoldBoxGame::pools_of_darkness || second_bond) && keeps_preparation &&
                    a_night_first &&
                    (std::ranges::any_of(wanted_spells,
                                         [&](const std::string &wanted)
                                         {
                                             return copies_ready(wanted) <
                                                    static_cast<unsigned>(
                                                        std::ranges::count(wanted_spells, wanted));
                                         }) ||
                     (!ward_spell.empty() && copies_ready(ward_spell) < ward_target_count()));
                const bool resting = (a_night_first && day == 0) || !party_whole() || missing_plan;
                if (!resting)
                {
                    break;
                }
                const auto fights_before = fights_fought;
                const auto hit_points_before = party_hit_points();
                bool rested = false;
                for (unsigned reload = 0; reload <= 32; ++reload)
                {
                    const bool pit =
                        third_bond && (session.snapshot().script_id == 18 ||
                                       (moander_recovery && session.snapshot().script_id == 17));
                    const bool cave = fourth_bond && session.snapshot().script_id == 34;
                    const bool retry_camp = pit || cave;
                    const auto before_camp =
                        retry_camp ? std::optional(session.detached_copy()) : std::nullopt;
                    const auto party_before = party->state();
                    const auto answers_before = answers;
                    reload_camp_on_attack = retry_camp && reload < 32;
                    rested = camp_and_rest();
                    if (rested)
                    {
                        // Choose actual spells as the rest dialog does. A
                        // player reloads a camp attacked before preparation
                        // finishes rather than spending its wards in that fight.
                        memorize();
                        play("camp");
                    }
                    reload_camp_on_attack = false;
                    if (!rested || !retry_camp || session.snapshot().phase != TourPhase::combat)
                    {
                        break;
                    }
                    session = before_camp->detached_copy();
                    party->restore(party_before);
                    session.attach_restored_party(party);
                    answers = answers_before;
                    session.reseed_dice(
                        static_cast<std::uint32_t>(party_before.time_minutes + reload + 1));
                    ++phases[pit ? "Pit preparation camps reloaded"
                                 : "Cave preparation camps reloaded"];
                }
                if (!rested)
                {
                    break;
                }
                // A camp whose script speaks first rests only once its
                // words are answered; finish any choices left by it.
                if (party->state().spell_rest)
                {
                    memorize();
                }
                if (stop_after_four_patrols &&
                    session.script_variable(opengold::por::EclAddress{0x4C47}) >= 4)
                {
                    if (session.snapshot().camp_offered)
                    {
                        session.decline_camp();
                    }
                    return;
                }
                // At the final preparation camps, use the refreshed cures
                // instead of waiting a day per Hit Point while wandering
                // monsters repeatedly undo the natural healing.
                if ((game == GoldBoxGame::pools_of_darkness && keeps_preparation) ||
                    (third_bond && (session.snapshot().script_id == 18 ||
                                    (moander_recovery && session.snapshot().script_id == 17))) ||
                    (fourth_bond && session.snapshot().script_id == 34))
                {
                    cure_the_hurt(true);
                }
                if (fights_fought != fights_before &&
                    game == GoldBoxGame::secret_of_the_silver_blades && ++interrupted >= 6)
                {
                    restless_map = session.snapshot().area_id;
                    break;
                }
                // A camp its script did not let rest (Pools of Darkness's
                // camp entry sets cell 2Bh: the Fire Giant Cave outside its
                // quiet rooms): its cures are cast instead.
                const bool darkness = game == GoldBoxGame::pools_of_darkness;
                // With `through_period`, a camp that rested its day is no
                // refusal, though its script set a period (the Tower of
                // Marcus's 96 steps, broken but in its quiet rooms).
                const bool refused = darkness &&
                                     opengold::test::TourAccess::machine(session).variable(
                                         opengold::por::EclAddress{0x2B}) != 0 &&
                                     !(through_period && session.snapshot().dialogue.find(
                                                             "rest complete") != std::string::npos);
                // With `through_period` a camp whose wanderers only pass by
                // (the Cave of the Beholder's chanting minions) is made again,
                // as a player camps again; twenty such in a row are a refusal.
                const bool idle = refused && resting && fights_fought == fights_before &&
                                  party_hit_points() == hit_points_before;
                idle_camps = idle ? idle_camps + 1 : 0;
                if (idle && (!through_period || idle_camps >= 20))
                {
                    cure_the_hurt();
                    break;
                }
            }
        };
        // Days in camp from the wilderness's menu until every member is whole:
        // CAMP opens the camp, and the menu returns after each night or fight.
        const auto rest_in_wilderness = [&](bool a_night_first = false)
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
            for (int day = 0; day < 90 && (a_night_first || !party_whole()); ++day)
            {
                answers = {"CAMP"};
                play("wilderness camp");
                if (!session.snapshot().camp_offered ||
                    !session.camp(opengold::RestKind::long_rest))
                {
                    break;
                }
                a_night_first = false;
                // The spells are chosen as the rest ends, before the camp is
                // attacked or the menu returns, as the rest dialog has it.
                memorize();
                answers = {"stop:JOURNEY"};
                play("wilderness camp");
                if (sewers)
                {
                    cure_the_hurt(true);
                }
            }
        };
        if (game == GoldBoxGame::secret_of_the_silver_blades)
        {
            items_to_take =
                earned_secret
                    ? std::vector<std::pair<std::string,
                                            std::string>>{{"Plate Mail +1", "paladin 1"},
                                                          {"Shield +2", "paladin 1"},
                                                          {"Long Sword +1", "paladin 1"},
                                                          {"Gauntlets", "paladin 2"},
                                                          {"Halberd +2", "paladin 2"},
                                                          {"Mace +1", "cleric 4"},
                                                          {"Bracers", "magic_user 6"},
                                                          {"Wand of Ice Storm", "magic_user 6"},
                                                          {"Leather Armor +1", "ranger 3"},
                                                          {"Scale Mail +2", "cleric 5"}}
                    : secret_town_gear();
        }
        play("opening");
        // Planning aids, so a search can start where the route ends without
        // replaying it: OPENGOLD_ROUTE_BEATS=N plays only the route's first N
        // beats, OPENGOLD_ROUTE_SAVE=FILE saves the game where the beats end
        // (with their count), and OPENGOLD_ROUTE_RESUME=FILE loads such a save
        // and plays on from the beat after it.
        const char *beats_to_play = std::getenv("OPENGOLD_ROUTE_BEATS");
        const std::size_t beat_limit = beats_to_play ? std::stoul(beats_to_play) : SIZE_MAX;
        constexpr std::string_view assets = "route-checkpoint";
        std::size_t beats_resumed = 0;
        if (const char *resume = std::getenv("OPENGOLD_ROUTE_RESUME"))
        {
            std::ifstream file(resume, std::ios::binary);
            if (!file)
            {
                throw std::runtime_error(std::string("Cannot open route checkpoint: ") + resume);
            }
            file >> beats_resumed;
            file.get();
            const std::string bytes{std::istreambuf_iterator<char>(file), {}};
            auto saved = opengold::decode_campaign(
                bytes, *creation, *opengold::original::rules_module(content), assets, &session);
            party->restore(saved.party);
            session = std::move(*saved.town);
            session.attach_restored_party(party);
            // OPENGOLD_ROUTE_WHOLE makes the resumed party whole outside the
            // game's commands, its spells memorized, and keeps it from camping
            // on the map it stands on, to measure a fight at full strength (a
            // planning aid).
            if (std::getenv("OPENGOLD_ROUTE_WHOLE"))
            {
                for (int day = 0; day < 200 && !party_whole() && party->rest(); ++day)
                {
                    memorize();
                }
                restless_map = session.snapshot().area_id;
            }
        }
        std::size_t beat_number = 0;
        // Pools of Darkness's, Gateway's and Secret's routes give each beat
        // dice of its own (beat_dice_seed), so a resumed save plays on as the
        // whole run does and an engine change reshuffles only the beats it
        // touches; the other routes keep their one stream.
        const bool dice_per_beat = game == GoldBoxGame::pools_of_darkness ||
                                   game == GoldBoxGame::gateway_to_the_savage_frontier ||
                                   game == GoldBoxGame::secret_of_the_silver_blades;
        // How many beats like each (by beat_dice_seed's first seed) came before.
        std::map<std::uint64_t, unsigned> beats_seen;
        // A beat that throws ends the route, named with the beat it was in.
        std::string current_beat = "the route's start";
        // A member casts the first of these spells anyone has memorized,
        // outside combat.
        const auto cast_one_of =
            [&](const std::vector<std::string> &wanted, const std::string &caster_name)
        {
            for (const auto &spell : wanted)
            {
                for (const auto id : party->state().slots)
                {
                    if (!id || party->member(id).vitals.dead ||
                        (!caster_name.empty() &&
                         party->member(id).character.sheet().name != caster_name))
                    {
                        continue;
                    }
                    for (const auto &action : party->camp_actions(id))
                    {
                        if (action.id.find(spell) != std::string::npos)
                        {
                            std::cout << "  " << party->member(id).character.sheet().name
                                      << " casts " << action.id << "\n";
                            session.cast_outside_combat(id, id, action.id);
                            return;
                        }
                    }
                }
            }
            if (std::getenv("OPENGOLD_FIGHT_GEAR"))
            {
                for (const auto id : party->state().slots)
                {
                    if (!id)
                    {
                        continue;
                    }
                    std::cout << "  available after failed camp cast for "
                              << party->member(id).character.sheet().name << ":";
                    for (const auto &spell : party->memorized_spells(id))
                    {
                        std::cout << " " << spell;
                    }
                    std::cout << "\n";
                }
            }
            needs["No one can cast what the beat asks"].push_back(current_beat);
        };
        // The member named readies its largest stack of the item type,
        // preferring the strongest enchantment between equal stacks.
        const auto ready_type = [&](int type, const std::string &name)
        {
            for (const auto id : party->state().slots)
            {
                if (!id || party->member(id).character.sheet().name != name)
                {
                    continue;
                }
                const opengold::InventoryItem *largest = nullptr;
                const auto bonus = [&](const opengold::InventoryItem &item)
                {
                    const auto &sources = party->member(id).item_sources;
                    const auto found = sources.find(item.id);
                    return found == sources.end() ? 0 : found->second.stored.magic_bonus;
                };
                for (const auto &item : party->member(id).character.inventory().items())
                {
                    if (item.original_type == type &&
                        (!largest || item.quantity > largest->quantity ||
                         (item.quantity == largest->quantity && bonus(item) > bonus(*largest))))
                    {
                        largest = &item;
                    }
                }
                if (largest && std::ranges::find(party->member(id).equipped, largest->id) ==
                                   party->member(id).equipped.end())
                {
                    std::cout << "  " << name << " readies " << largest->name << "\n";
                    ready_in_place(id, largest->id);
                }
            }
        };
        // Each living member gets the spell from whoever has it memorized,
        // cast in camp (RouteBeat::ward_each_with).
        const auto ward_each_member =
            [&](const std::string &spell_id, const std::vector<std::string> &members)
        {
            const auto has_ward = [&](opengold::MemberId id)
            {
                return id && !party->member(id).vitals.dead &&
                       party->member(id).vitals.hit_points > 0 &&
                       std::ranges::any_of(party->camp_actions(id),
                                           [&](const opengold::rules::CampAction &action)
                                           {
                                               return spell_named(action.id, spell_id);
                                           });
            };
            for (const auto target : party->state().slots)
            {
                if (!target || party->member(target).vitals.dead ||
                    (!members.empty() &&
                     std::ranges::find(members, party->member(target).character.sheet().name) ==
                         members.end()))
                {
                    continue;
                }
                const auto caster = std::ranges::find_if(party->state().slots, has_ward);
                if (caster == party->state().slots.end())
                {
                    if (std::getenv("OPENGOLD_FIGHT_GEAR"))
                    {
                        for (const auto id : party->state().slots)
                        {
                            if (!id)
                            {
                                continue;
                            }
                            std::cout << "  available wards: "
                                      << party->member(id).character.sheet().name;
                            for (const auto &action : party->camp_actions(id))
                            {
                                std::cout << " " << action.id;
                            }
                            std::cout << "\n";
                        }
                    }
                    needs["No one has the ward memorized for every member"].push_back(current_beat);
                    return;
                }
                const auto actions = party->camp_actions(*caster);
                const auto action =
                    std::ranges::find_if(actions,
                                         [&](const opengold::rules::CampAction &camp_action)
                                         {
                                             return spell_named(camp_action.id, spell_id);
                                         });
                std::cout << "  " << party->member(*caster).character.sheet().name << " casts "
                          << action->id << " on " << party->member(target).character.sheet().name
                          << "\n";
                party->use_camp_action(*caster, target, action->id);
            }
        };
        try
        {
            const auto earned_beats = sewers          ? curse_earned_route(progress)
                                      : earned_secret ? secret_earned_route(secret_progress)
                                                      : std::vector<RouteBeat>{};
            for (const auto &beat : earned ? earned_beats : route_for(game))
            {
                if (beat_number == beat_limit)
                {
                    break;
                }
                ++beat_number;
                check(!earned || (beat.test_experience == 0 && beat.test_gold == 0 &&
                                  !beat.test_heal && !beat.test_wins && beat.test_cells.empty() &&
                                  !beat.wait_until_minute && !beat.enter_map),
                      "Earned progression uses no injected rewards, healing or victories");
                check(planning || !beat.test_wins,
                      "An ordinary campaign route never forces combat victories");
                const auto beat_seed = beat_dice_seed(beat, beats_seen[beat_dice_seed(beat, 0)]++);
                // Reconstruct the player's spell plan across skipped beats too,
                // so a resumed route prepares the same spells as a whole run.
                // Keep the finale's preparation through intervening camps.
                // Healing after a guard fight must not silently replace the
                // wards selected at the last safe inn with ordinary attacks.
                keeps_preparation = keeps_preparation || beat.keep_preparation;
                const auto &next_ward =
                    beat.ward_each_with.empty() ? beat.ward_memorized : beat.ward_each_with;
                if (!keeps_preparation || !next_ward.empty())
                {
                    ward_spell = next_ward;
                    ward_targets = beat.ward_members;
                }
                if (!keeps_preparation || !beat.memorize_first_of.empty())
                {
                    wanted_spells = beat.memorize_first_of;
                }
                wanted_spells.insert(wanted_spells.end(), beat.cast_after.begin(),
                                     beat.cast_after.end());
                if (beat_number <= beats_resumed)
                {
                    continue;
                }
                const auto where = "beat " + std::to_string(beat.map) + " " +
                                   std::to_string(beat.x) + "," + std::to_string(beat.y);
                current_beat = where + " (beat " + std::to_string(beat_number) + ")";
                stop_after_four_patrols = second_bond && beat_number == 28;
                if (session.snapshot().phase == TourPhase::defeated)
                {
                    break;
                }
                if (dice_per_beat)
                {
                    // A script waiting at a menu (Elminster's in Limbo) keeps
                    // its dice: the machine takes a seed only between scripts.
                    const auto machine_state = opengold::test::TourAccess::machine(session).state();
                    if (machine_state == opengold::por::EclState::idle ||
                        machine_state == opengold::por::EclState::completed)
                    {
                        session.reseed_dice(
                            static_cast<std::uint32_t>(beat_seed ^ (beat_seed >> 32)));
                    }
                    auto reseeded = party->state();
                    reseeded.random_state = opengold::RandomState{beat_seed};
                    party->restore(std::move(reseeded));
                }
                // A camp before the beat fights as the beat does.
                if (sewers)
                {
                    items_to_take = beat.take_items;
                }
                test_winning = beat.test_wins && !playing_stand_ins;
                reloads_until_won = beat.reloads_until_won;
                beat_reloads = beat.reloads;
                if (beat.rest_first ||
                    (beat.passing && !beat.camp_broken && (party_hurt() || spells_spent()) &&
                     session.snapshot().area_id != restless_map))
                {
                    // A camp attacked fights: the last beat's flight is over.
                    fleeing = false;
                    beat.waiting ? rest_in_wilderness(sewers && beat.rest_for_spells)
                                 : rest_until_healed(beat.rest_for_spells || spells_spent(),
                                                     beat.rest_through_period);
                }
                else if (game == GoldBoxGame::secret_of_the_silver_blades && route &&
                         beat.passing && !party_whole())
                {
                    // Secret's party Fixes whenever a member is hurt, as a
                    // player does after a fight, and where it gives up
                    // resting: its few hours are broken less often than a
                    // night.
                    fleeing = false;
                    unsigned interrupted = 0;
                    (void)fix_party(interrupted);
                }
                if (beat.cure_first)
                {
                    cure_the_hurt(beat.cure_to_full);
                }
                if (!beat.ward_each_with.empty())
                {
                    ward_each_member(beat.ward_each_with, beat.ward_members);
                }
                for (const auto &[type, name] : beat.ready_types)
                {
                    ready_type(type, name);
                }
                for (const auto &[name, holder] : beat.ready_names)
                {
                    const auto member = std::ranges::find_if(
                        party->state().slots,
                        [&](opengold::MemberId id)
                        {
                            return id && party->member(id).character.sheet().name == holder;
                        });
                    check(member != party->state().slots.end(),
                          "The named item's holder is in the party");
                    const auto &items = party->member(*member).character.inventory().items();
                    const auto item = std::ranges::find_if(
                        items,
                        [&](const opengold::InventoryItem &entry)
                        {
                            const auto &source = party->member(*member).item_sources.at(entry.id);
                            return source.identified() &&
                                   source.label().find(name) != std::string::npos;
                        });
                    if (item == items.end())
                    {
                        for (const auto &entry : items)
                        {
                            const auto &source = party->member(*member).item_sources.at(entry.id);
                            std::cout << "  carried: " << entry.name << " / " << source.label()
                                      << " x" << entry.quantity << " (identified "
                                      << source.identified() << ")\n";
                        }
                    }
                    check(item != items.end(), "Ready the actually identified carried item");
                    const auto id = item->id;
                    ready_in_place(*member, id);
                    std::cout << "  " << holder << " readies " << name << "\n";
                }
                if (beat.memorize_first && session.can_leave() && camp_and_rest())
                {
                    memorize();
                    play("camp");
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
                // A night's rest heals little under the original rules: a
                // resting beat may take months.
                const int rounds = beat.rest_choice ? 120 : 40;
                for (int round = 0; round < rounds; ++round)
                {
                    if (hap && beat_number == 28 && round > 0)
                    {
                        rest_until_healed(true);
                    }
                    if (stop_after_four_patrols &&
                        session.script_variable(opengold::por::EclAddress{0x4C47}) >= 4)
                    {
                        break;
                    }
                    answers = beat.answers;
                    taking_wand = beat.take_wand;
                    packing_wand = beat.pack_wand;
                    if (!sewers)
                    {
                        items_to_take = beat.take_items;
                    }
                    passing = beat.passing;
                    fleeing = beat.fleeing;
                    avoiding = beat.avoiding;
                    if (beat.searching)
                    {
                        session.set_searching(*beat.searching);
                    }
                    test_winning = beat.test_wins && !playing_stand_ins;
                    holding_foes = beat.holding_foes;
                    reloads_until_won = beat.reloads_until_won;
                    beat_reloads = beat.reloads;
                    maze_exits.assign(beat.maze_exits.begin(), beat.maze_exits.end());
                    if (beat.rest_choice)
                    {
                        if (party->state().spell_rest)
                        {
                            memorize();
                        }
                        answers = {*beat.rest_choice, "stop:" + *beat.rest_choice};
                        play(where);
                        if (party->state().spell_rest)
                        {
                            memorize();
                        }
                    }
                    else if (beat.waiting)
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
                        check(!earned, "The earned opening enters areas through their scripts");
                        opengold::test::TourAccess::enter(session, beat.map,
                                                          {beat.x, beat.y, beat.facing});
                        play(where);
                    }
                    else
                    {
                        const unsigned x = beat.repeat_in_place
                                               ? beat.x
                                               : (beat.x + static_cast<unsigned>(round)) % 16;
                        // OPENGOLD_ROUTE_JUMPS names each beat that puts the
                        // party on a square of its map other than the one it
                        // stands on, a step the beats before it did not walk.
                        const auto &stands = session.snapshot();
                        if (std::getenv("OPENGOLD_ROUTE_JUMPS") && round == 0 &&
                            stands.area_id == beat.map &&
                            (stands.pose.x != x || stands.pose.y != beat.y))
                        {
                            std::cout << "  jump to " << where << " from " << stands.pose.x << ","
                                      << stands.pose.y << "\n";
                        }
                        // Treasures' overland: a band that kills a member or
                        // beats the party is met as a player meets it after
                        // reloading, the step played again with the new dice
                        // a reload gives (reseed_dice), up to 20 times.
                        const bool reloading =
                            beat.reload_event ||
                            (game == GoldBoxGame::treasures_of_the_savage_frontier && beat.fleeing);
                        for (unsigned reload = 0;; ++reload)
                        {
                            beat_reloads = beat.reloads + reload;
                            auto before_step =
                                reloading ? std::optional(session.detached_copy()) : std::nullopt;
                            const auto party_before = party->state();
                            const auto answers_before = answers;
                            const unsigned lost_before = fights_lost;
                            rejected_boss_win = false;
                            if (earned)
                            {
                                check(
                                    !earned_secret || session.snapshot().area_id == beat.map,
                                    "Earned Secret walking begins in its actual destination area");
                                const auto destination_script = session.snapshot().script_id;
                                const auto destination_answers = answers;
                                const bool destination_passing = passing;
                                const bool story_walk =
                                    earned_secret ||
                                    (second_bond &&
                                     (session.snapshot().script_id == 50 || beat.map == 51 ||
                                      (third_bond && (beat.map == 16 || beat.map == 17)) ||
                                      (fourth_bond && (beat.map == 32 || beat.map == 33)) ||
                                      (earned_ending &&
                                       (beat.map == 64 || beat.map == 66 || beat.map == 67))));
                                passing = !story_walk;
                                session.set_searching(beat.searching.value_or(true));
                                walk_earned_campaign(
                                    session, x, beat.y,
                                    [&]
                                    {
                                        answers = story_walk ? destination_answers
                                                  : beat_number == 3
                                                      ? std::vector<std::string>{"YES", "Bash"}
                                                      : std::vector<std::string>{
                                                            "BARGAIN", "BURN THEM", "NO", "Bash"};
                                        if (earned_secret && beat.map == 33 &&
                                            (session.snapshot().pose.x != x ||
                                             session.snapshot().pose.y != beat.y))
                                        {
                                            // Pass other Well gateways on the way to the
                                            // chosen portal without entering them.
                                            answers = {"NO"};
                                        }
                                        if (first_bond && beat.map == 4)
                                        {
                                            answers.insert(answers.begin(), "stop:JOURNEY");
                                        }
                                        play(where);
                                        if (earned_secret && beat.cure_first && session.can_leave())
                                        {
                                            cure_the_hurt(beat.cure_to_full);
                                        }
                                    },
                                    third_bond && beat.map == 16);
                                passing = destination_passing;
                                answers = destination_answers;
                                if (session.snapshot().script_id != destination_script)
                                {
                                    break;
                                }
                                for (unsigned turns = 0;
                                     session.snapshot().pose.facing != beat.facing && turns < 4;
                                     ++turns)
                                {
                                    check(session.explore(
                                              opengold::por::ExplorationCommand::turn_right),
                                          "The earned party faces its destination");
                                    play(where);
                                }
                            }
                            else
                            {
                                check(!earned, "The earned opening never relocates the party");
                                opengold::test::TourAccess::place(session,
                                                                  {x, beat.y, beat.facing});
                            }
                            if (!beat.arrive_only &&
                                session.explore(beat.step
                                                    ? opengold::por::ExplorationCommand::forward
                                                    : opengold::por::ExplorationCommand::look))
                            {
                                play(where);
                            }
                            const bool failed =
                                beat.reload_event ? rejected_boss_win || session.snapshot().phase ==
                                                                             TourPhase::defeated
                                                  : fights_lost != lost_before;
                            if (!reloading || !failed || reload >= 20)
                            {
                                break;
                            }
                            ++phases["overland steps reloaded"];
                            session = std::move(*before_step);
                            party->restore(party_before);
                            // Reloading the event also replays its decisions.
                            // Otherwise a lost finale consumes NO/NO/YES/NO
                            // and the next attempt accepts the Balor's bargain.
                            answers = answers_before;
                            session.attach_restored_party(party);
                            session.reseed_dice(
                                static_cast<std::uint32_t>(beat_number * 64 + reload));
                        }
                    }
                    if (!beat.cast_after.empty())
                    {
                        cast_one_of(beat.cast_after, beat.cast_after_by);
                        play(where);
                    }
                    const auto &until = beat.repeat_until_at_least;
                    const auto &bits = beat.repeat_until_bits;
                    const bool repeating =
                        (beat.rest_choice && !party_whole()) ||
                        (until && cell(until->first) < until->second) ||
                        (bits && (cell(bits->first) & bits->second) != bits->second);
                    if (!repeating || session.snapshot().phase == TourPhase::defeated)
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
                std::cout << "  party after " << where << " (" << beat_number << "):";
                for (const auto id : party->state().slots)
                {
                    if (id)
                    {
                        const auto &member = party->member(id);
                        std::cout << " " << member.vitals.hit_points << "/"
                                  << member.character.sheet().hit_points
                                  << (member.vitals.dead ? " dead" : "");
                        // Why a member does not heal: its state's words.
                        if (!member.vitals.dead && !member.vitals.description.empty() &&
                            member.vitals.hit_points < member.character.sheet().hit_points)
                        {
                            std::cout << " [" << member.vitals.description << "]";
                        }
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
                // OPENGOLD_ROUTE_SAVES=PREFIX saves the game after each of the
                // beats OPENGOLD_ROUTE_SAVE_AT names (N,N,...), as PREFIXN.sav,
                // for OPENGOLD_ROUTE_RESUME.
                if (const char *prefix = std::getenv("OPENGOLD_ROUTE_SAVES");
                    prefix && session.can_leave())
                {
                    const auto at = std::string(",") +
                                    (std::getenv("OPENGOLD_ROUTE_SAVE_AT")
                                         ? std::getenv("OPENGOLD_ROUTE_SAVE_AT")
                                         : "") +
                                    ",";
                    if (at.find("," + std::to_string(beat_number) + ",") != std::string::npos)
                    {
                        std::ofstream file(prefix + std::to_string(beat_number) + ".sav",
                                           std::ios::binary);
                        file << beat_number << '\n'
                             << opengold::encode_campaign(*party, &session, assets, game);
                    }
                }
            }
        }
        catch (const std::exception &error)
        {
            needs[error.what()].push_back(current_beat);
        }
        if (const char *save = std::getenv("OPENGOLD_ROUTE_SAVE"); save && session.can_leave())
        {
            std::ofstream file(save, std::ios::binary);
            file << beat_number << '\n'
                 << opengold::encode_campaign(*party, &session, assets, game);
        }

        // The full route must hear Bane's arrival music. A late checkpoint
        // does not replay that earlier event; report failures before the
        // diagnostics summary rather than silently changing the exit status.
        if (game == GoldBoxGame::pools_of_darkness && beats_resumed == 0 &&
            std::ranges::find(music_heard, 2U) == music_heard.end())
        {
            needs["Bane's coming started no music"].push_back("the route");
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
        // OPENGOLD_WALLS prints each square's walls, doors and event on the
        // map the route ended on, or on map N with OPENGOLD_WALLS=N (N > 1),
        // to plan a route by.
        if (const char *walls_of = std::getenv("OPENGOLD_WALLS"))
        {
            if (std::strtoul(walls_of, nullptr, 10) > 1)
            {
                if (game == GoldBoxGame::treasures_of_the_savage_frontier)
                {
                    prepare_treasures_entry(
                        session, static_cast<unsigned>(std::strtoul(walls_of, nullptr, 10)));
                }
                opengold::test::TourAccess::enter(
                    session, static_cast<unsigned>(std::strtoul(walls_of, nullptr, 10)),
                    {0, 0, opengold::por::MapDirection::north});
                play("walls");
            }
            const auto &map = session.map();
            for (unsigned y = 0; y < 16; ++y)
            {
                for (unsigned x = 0; x < 16; ++x)
                {
                    const auto &c = map.at(x, y);
                    std::cout << "walls " << x << "," << y << " w" << int(c.walls[0]) << "."
                              << int(c.walls[1]) << "." << int(c.walls[2]) << "." << int(c.walls[3])
                              << " d" << int(c.doors[0]) << "." << int(c.doors[1]) << "."
                              << int(c.doors[2]) << "." << int(c.doors[3]) << " e"
                              << int(c.event_raw) << "\n";
                }
            }
        }
        // OPENGOLD_CURSE_SURVEY walks the map the route ended on, printing each
        // square's event, to plan the route's next beats by.
        if (const char *survey = std::getenv("OPENGOLD_CURSE_SURVEY");
            survey && end.phase != TourPhase::defeated)
        {
            // OPENGOLD_CURSE_SURVEY=N (N > 1) walks into map N first.
            if (std::strtoul(survey, nullptr, 10) > 1)
            {
                if (game == GoldBoxGame::treasures_of_the_savage_frontier)
                {
                    prepare_treasures_entry(
                        session, static_cast<unsigned>(std::strtoul(survey, nullptr, 10)));
                }
                opengold::test::TourAccess::enter(
                    session, static_cast<unsigned>(std::strtoul(survey, nullptr, 10)),
                    {0, 0, opengold::por::MapDirection::north});
                play("survey");
            }
            const auto map = session.snapshot().area_id;
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
                    // OPENGOLD_CURSE_SURVEY=E, S or W looks facing that way
                    // (an event that answers only one facing); else north.
                    const std::string_view facing_word = std::getenv("OPENGOLD_CURSE_SURVEY");
                    const auto facing = facing_word == "E"   ? opengold::por::MapDirection::east
                                        : facing_word == "S" ? opengold::por::MapDirection::south
                                        : facing_word == "W" ? opengold::por::MapDirection::west
                                                             : opengold::por::MapDirection::north;
                    opengold::test::TourAccess::place(session, {x, y, facing});
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
            // OPENGOLD_SECRET_AREA steps an area map's eight ways instead
            // (Pools of Darkness's Moander's body, script 51), with
            // OPENGOLD_SECRET_KEYS=25,26 keying each place by its cells.
            const bool area = std::getenv("OPENGOLD_SECRET_AREA") != nullptr;
            for (std::size_t i = 0; i < nodes.size() && i < 20000 && !found; ++i)
            {
                for (unsigned f = 0; f < (area ? 8U : 4U) && !found; ++f)
                {
                    session = nodes[i].session.detached_copy();
                    party->restore(nodes[i].party_state);
                    session.attach_restored_party(party);
                    const auto from = session.snapshot().pose;
                    if (area)
                    {
                        (void)session.explore(static_cast<opengold::por::ExplorationCommand>(
                            static_cast<unsigned>(opengold::por::ExplorationCommand::north) + f));
                    }
                    else
                    {
                        const auto facing = static_cast<opengold::por::MapDirection>(f);
                        opengold::test::TourAccess::place(session, {from.x, from.y, facing});
                        (void)session.explore(opengold::por::ExplorationCommand::forward);
                    }
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
        const bool someone_standing = std::ranges::any_of(
            party->state().slots,
            [&](opengold::MemberId id)
            {
                return id && !party->member(id).vitals.dead &&
                       party->member(id).vitals.hit_points > 0 &&
                       party->member(id).vitals.description != "Turned to stone.";
            });
        const bool played = needs.empty() && end.phase != TourPhase::defeated;
        if (earned_secret)
        {
            unsigned pcs = 0;
            bool healthy = true;
            for (const auto id : party->state().slots)
            {
                if (!id)
                {
                    continue;
                }
                const auto &member = party->member(id);
                healthy = healthy && !member.vitals.dead &&
                          member.vitals.description != "Turned to stone.";
                if (!member.npc_source.empty())
                {
                    continue;
                }
                ++pcs;
                healthy = healthy && member.experience >= 200000 &&
                          std::ranges::any_of(member.character.inventory().items(),
                                              [](const opengold::InventoryItem &item)
                                              {
                                                  return item.original_type == mirror_type;
                                              });
            }
            std::cout << (secret_progress >= SecretProgress::staff
                              ? "Earned Secret restored staff: "
                          : secret_progress >= SecretProgress::first_staff
                              ? "Earned Secret first staff piece: "
                          : secret_progress >= SecretProgress::headquarters
                              ? "Earned Secret headquarters: "
                          : secret_progress >= SecretProgress::eldamar ? "Earned Secret Eldamar: "
                          : secret_progress >= SecretProgress::well    ? "Earned Secret Well: "
                                                                       : "Earned Secret opening: ")
                      << fights_fought << " fights, " << fights_defeated
                      << " defeats; this is not a campaign win.\n";
            return played && pcs == 6 && healthy && someone_standing && session.can_leave() &&
                   end.area_id == (secret_progress >= SecretProgress::staff         ? 50U
                                   : secret_progress >= SecretProgress::first_staff ? 48U
                                   : secret_progress >= SecretProgress::eldamar     ? 33U
                                                                                    : 16U) &&
                   session.script_variable(opengold::por::EclAddress{0x4C2D}) == 255 &&
                   fights_fought >= 1 && fights_defeated == 0 &&
                   (secret_progress < SecretProgress::well ||
                    session.script_variable(opengold::por::EclAddress{0x4C63}) == 255) &&
                   (secret_progress < SecretProgress::eldamar ||
                    session.script_variable(opengold::por::EclAddress{0x4C53}) == 255) &&
                   (secret_progress < SecretProgress::headquarters ||
                    session.script_variable(opengold::por::EclAddress{0x4C98}) == 255) &&
                   (secret_progress < SecretProgress::first_staff ||
                    (session.script_variable(opengold::por::EclAddress{0x4C85}) == 1 &&
                     (session.script_variable(opengold::por::EclAddress{0x4C84}) & 1) != 0)) &&
                   (secret_progress < SecretProgress::staff ||
                    (session.script_variable(opengold::por::EclAddress{0x4C84}) == 255 &&
                     session.script_variable(opengold::por::EclAddress{0x4C86}) == 1));
        }
        if (sewers)
        {
            std::cout << (earned_ending      ? "Earned Curse ending route: "
                          : fourth_bond      ? "Earned fourth-bond progression: "
                          : moander_recovery ? "Earned Moander recovery: "
                          : third_bond       ? "Earned third-bond progression: "
                          : second_bond      ? "Earned second-bond progression: "
                          : hap              ? "Earned Hap progression: "
                          : first_bond       ? "Earned first-bond progression: "
                                             : "Earned sewer progression: ")
                      << fights_fought << " fights, " << fights_defeated << " defeats, "
                      << fights_lost << " fights with casualties or defeat; "
                      << (earned_ending ? "the actual winning event is required.\n"
                                        : "this is not a campaign win.\n");
            const auto player_count =
                std::ranges::count_if(party->state().slots,
                                      [&](opengold::MemberId id)
                                      {
                                          return id && party->member(id).npc_source.empty();
                                      });
            const bool earned_training =
                player_count == 6 &&
                std::ranges::all_of(party->state().slots,
                                    [&](opengold::MemberId id)
                                    {
                                        if (!id)
                                        {
                                            return true;
                                        }
                                        const auto &member = party->member(id);
                                        if (!member.npc_source.empty())
                                        {
                                            return true;
                                        }
                                        const auto &sheet = member.character.sheet();
                                        return (third_bond || !member.vitals.dead) &&
                                               member.experience > 25000 &&
                                               (!sheet.name.starts_with("cleric") ||
                                                std::ranges::max(sheet.original->class_levels) >=
                                                    6);
                                    });
            if (earned_ending)
            {
                const bool pc_standing = std::ranges::any_of(
                    party->state().slots,
                    [&](opengold::MemberId id)
                    {
                        return id && party->member(id).npc_source.empty() &&
                               !party->member(id).vitals.dead &&
                               party->member(id).vitals.hit_points > 0 &&
                               party->member(id).vitals.description != "Turned to stone.";
                    });
                const bool won = played && earned_training && pc_standing && game_won_shown &&
                                 end.dialogue.find("You have won.") != std::string::npos &&
                                 fights_defeated == 0;
                std::cout << "Earned Curse campaign " << (won ? "won" : "not won") << ".\n";
                return won;
            }
            if (fourth_bond)
            {
                return played && someone_standing && earned_training && session.can_leave() &&
                       end.script_id == 17 &&
                       session.script_variable(opengold::por::EclAddress{0x4C59}) == 1 &&
                       session.script_variable(opengold::por::EclAddress{0x4C5B}) == 255 &&
                       session.script_variable(opengold::por::EclAddress{0x4C5A}) == 1 &&
                       fights_defeated == 0;
            }
            if (moander_recovery)
            {
                const bool recovered = std::ranges::all_of(
                    party->state().slots,
                    [&](opengold::MemberId id)
                    {
                        if (!id)
                        {
                            return true;
                        }
                        const auto &member = party->member(id);
                        const auto &sheet = member.character.sheet();
                        return member.npc_source.empty() && !member.vitals.dead &&
                               ((!sheet.name.starts_with("cleric") &&
                                 !sheet.name.starts_with("magic_user")) ||
                                std::ranges::max(sheet.original->class_levels) >= 8);
                    });
                // The guard and departure scripts advance these cells to
                // 255. Reentering the cleared Pit resets its visit cell to
                // 128 while retaining the completed Gauntlet state.
                return played && someone_standing && earned_training && recovered &&
                       session.can_leave() && end.script_id == 17 &&
                       session.script_variable(opengold::por::EclAddress{0x4C5B}) == 255 &&
                       session.script_variable(opengold::por::EclAddress{0x4C2D}) == 128 &&
                       fights_defeated == 0;
            }
            if (third_bond)
            {
                const bool pc_standing = std::ranges::any_of(
                    party->state().slots,
                    [&](opengold::MemberId id)
                    {
                        return id && party->member(id).npc_source.empty() &&
                               !party->member(id).vitals.dead &&
                               party->member(id).vitals.hit_points > 0 &&
                               party->member(id).vitals.description != "Turned to stone.";
                    });
                return played && pc_standing && earned_training &&
                       session.script_variable(opengold::por::EclAddress{0x4C5B}) == 1 &&
                       session.script_variable(opengold::por::EclAddress{0x4C2D}) == 1 &&
                       fights_defeated == 0;
            }
            if (second_bond)
            {
                const bool advanced = std::ranges::all_of(
                    party->state().slots,
                    [&](opengold::MemberId id)
                    {
                        return !id || !party->member(id).npc_source.empty() ||
                               std::ranges::max(
                                   party->member(id).character.sheet().original->class_levels) >= 6;
                    });
                return played && someone_standing && earned_training && advanced &&
                       session.script_variable(opengold::por::EclAddress{0x4C59}) == 1 &&
                       session.script_variable(opengold::por::EclAddress{0x4C60}) == 1 &&
                       fights_defeated == 0;
            }
            if (hap)
            {
                return played && end.area_id == 50 && someone_standing && earned_training &&
                       session.script_variable(opengold::por::EclAddress{0x4C47}) >= 4 &&
                       session.script_variable(opengold::por::EclAddress{0x4C5E}) == 1 &&
                       fights_fought >= 6 && fights_lost == 0;
            }
            if (first_bond)
            {
                return played && end.area_id == 0 && someone_standing && earned_training &&
                       session.script_variable(opengold::por::EclAddress{0x4C2A}) == 1 &&
                       fights_fought >= 6 && fights_lost == 0;
            }
            return played && end.area_id == 3 && someone_standing && earned_training &&
                   fights_fought >= 6 && fights_lost == 0;
        }
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
        if (game == GoldBoxGame::treasures_of_the_savage_frontier && !treasure_route_ends_the_game)
        {
            std::cout << "Treasures' route stops on map " << end.area_id
                      << ", short of the game's end (docs/GAMES.md, Treasures, step 6).\n";
            return played && end.area_id == treasure_route_stops_on_map;
        }
        // Pools of Darkness's script 75 goes on past the win to Elminster's
        // last words and the party's return to Phlan, so its route ends
        // there, the win shown on the way.
        const bool ends_on_the_win = game != GoldBoxGame::pools_of_darkness;
        // The original counts the final fight won with one member standing
        // (postcbt.pas, DetermineCombatResults). Reaching the actual ending
        // with casualties is a win; a complete party loss is not.
        return played &&
               (!ends_on_the_win || end.dialogue.find("You have won.") != std::string::npos) &&
               game_won_shown && someone_standing;
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
    check(curse_save.starts_with("OPENGOLD-CAMPAIGN 53\n"), "Saves are format 53");
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
        if (argc == 2 && std::string_view(argv[1]) == "--curse-earned-ending")
        {
            return walk_census(GoldBoxGame::curse_of_the_azure_bonds, true, CurseProgress::ending)
                       ? 0
                       : 1;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--curse-fourth-bond")
        {
            return walk_census(GoldBoxGame::curse_of_the_azure_bonds, true,
                               CurseProgress::fourth_bond)
                       ? 0
                       : 1;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--curse-moander-recovery")
        {
            return walk_census(GoldBoxGame::curse_of_the_azure_bonds, true,
                               CurseProgress::moander_recovery)
                       ? 0
                       : 1;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--curse-third-bond")
        {
            return walk_census(GoldBoxGame::curse_of_the_azure_bonds, true,
                               CurseProgress::third_bond)
                       ? 0
                       : 1;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--curse-second-bond")
        {
            return walk_census(GoldBoxGame::curse_of_the_azure_bonds, true,
                               CurseProgress::second_bond)
                       ? 0
                       : 1;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--curse-hap")
        {
            return walk_census(GoldBoxGame::curse_of_the_azure_bonds, true, CurseProgress::hap) ? 0
                                                                                                : 1;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--curse-first-bond")
        {
            return walk_census(GoldBoxGame::curse_of_the_azure_bonds, true,
                               CurseProgress::first_bond)
                       ? 0
                       : 1;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--curse-sewers")
        {
            return walk_census(GoldBoxGame::curse_of_the_azure_bonds, true, CurseProgress::sewers)
                       ? 0
                       : 1;
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
        if (argc == 2 && std::string_view(argv[1]) == "--secret-earned-opening")
        {
            return walk_census(GoldBoxGame::secret_of_the_silver_blades, true, CurseProgress::none,
                               SecretProgress::opening)
                       ? 0
                       : 1;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--secret-earned-well")
        {
            return walk_census(GoldBoxGame::secret_of_the_silver_blades, true, CurseProgress::none,
                               SecretProgress::well)
                       ? 0
                       : 1;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--secret-earned-eldamar")
        {
            return walk_census(GoldBoxGame::secret_of_the_silver_blades, true, CurseProgress::none,
                               SecretProgress::eldamar)
                       ? 0
                       : 1;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--secret-earned-headquarters")
        {
            return walk_census(GoldBoxGame::secret_of_the_silver_blades, true, CurseProgress::none,
                               SecretProgress::headquarters)
                       ? 0
                       : 1;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--secret-earned-first-staff")
        {
            return walk_census(GoldBoxGame::secret_of_the_silver_blades, true, CurseProgress::none,
                               SecretProgress::first_staff)
                       ? 0
                       : 1;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--secret-earned-staff")
        {
            return walk_census(GoldBoxGame::secret_of_the_silver_blades, true, CurseProgress::none,
                               SecretProgress::staff)
                       ? 0
                       : 1;
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
        if (argc == 2 && std::string_view(argv[1]) == "--treasure-route")
        {
            return walk_census(GoldBoxGame::treasures_of_the_savage_frontier, true) ? 0 : 1;
        }
        each_game_is_told_apart_by_its_files();
        curse_creatures_are_read_from_their_layout();
        curse_scripts_run_in_their_own_bank();
        the_machine_runs_curse_scripts();
        curse_script_memory_is_pool_of_radiance_moved();
        engine_routines_are_the_games();
        curse_selected_damage();
        curse_camp_healing_survives_script_reads();
        the_loaders_read_curse();
        a_save_names_its_game();
        item_types_carry_their_properties();
        curse_opening_reaches_the_first_map();
        shared_map_saves_reload(GoldBoxGame::curse_of_the_azure_bonds);
        shared_map_saves_reload(GoldBoxGame::secret_of_the_silver_blades);
        secret_opening_reaches_the_first_map();
        secret_camp_fixes_the_party();
        the_font_has_eight_bytes_a_glyph();
        std::cout << "Gold Box games passed.\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
