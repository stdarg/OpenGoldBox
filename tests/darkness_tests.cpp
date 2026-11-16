// Pools of Darkness's campaign (docs/games/darkness.md): its script commands,
// and its opening played on the real files when OPENGOLD_GOLDBOX_DIR names the
// folder holding DARKNESS.
#include "opengold/campaign_party.h"
#include "opengold/campaign_save.h"
#include "opengold/campaign_session.h"
#include "opengold/character_creator.h"
#include "opengold/creature_catalog.h"
#include "opengold/darkness_import.h"
#include "opengold/ecl.h"
#include "opengold/ecl_machine.h"
#include "opengold/game_census.h"
#include "opengold/original.h"
#include "opengold/original_content.h"
#include "opengold/pool_radiance.h"
#include "opengold/por_sound.h"
#include "opengold/map_catalog.h"
#include "original_creation_support.h"
#include "test_support.h"
#include "tour_access.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace
{
using opengold::GoldBoxGame;
using opengold::por::EclAddress;
using opengold::por::EclMachine;
using opengold::por::EclState;
using opengold::por::ExplorationCommand;
using opengold::por::TourPhase;
using opengold::test::check;

constexpr auto darkness_game = GoldBoxGame::pools_of_darkness;

std::filesystem::path darkness_folder()
{
    const char *games = std::getenv("OPENGOLD_GOLDBOX_DIR");
    const auto folder = games ? std::filesystem::path(games) / "DARKNESS" : std::filesystem::path();
    return games && opengold::identify_game(folder) == darkness_game ? folder
                                                                     : std::filesystem::path();
}

// A script of Pools of Darkness whose five entries run body, which is
// followed by EXIT.
std::shared_ptr<const opengold::por::EclProgram> script(std::vector<std::uint8_t> body)
{
    const auto start =
        static_cast<std::uint16_t>(opengold::por::ecl_memory(darkness_game).origin + 5 * 4);
    std::vector<std::uint8_t> record{0, 0};
    for (int entry = 0; entry < 5; ++entry)
    {
        record.insert(record.end(), {0x01, 0x01, static_cast<std::uint8_t>(start & 0xff),
                                     static_cast<std::uint8_t>(start >> 8)});
    }
    record.insert(record.end(), body.begin(), body.end());
    record.push_back(0x00);
    return std::make_shared<const opengold::por::EclProgram>(
        opengold::por::EclProgram::decode(record, "commands", darkness_game));
}

// YES NO (22h) asks and sets only the comparison: yes the equal flag. WAIT KEY
// (1Dh) waits for Enter. CLEAR AND EXIT (23h) clears the text and ends the
// script, though data follow it.
void the_new_commands_run()
{
    const EclAddress said_yes{0x0100};
    // YES NO; IF =; SAVE 7 to 100h.
    const std::vector<std::uint8_t> ask{0x22, 0x16, 0x09, 0x00, 0x07, 0x01, 0x00, 0x01};
    for (const std::size_t choice : {0U, 1U})
    {
        EclMachine machine(script(ask));
        machine.bind_variable(said_yes, 0);
        check(machine.start(4), "The script starts");
        const auto asked = machine.run();
        check(asked.state == EclState::waiting && asked.request &&
                  asked.request->choices == std::vector<std::string>{"YES", "NO"},
              "YES NO asks");
        check(machine.resume(asked.request->id, choice), "The player answers");
        check(machine.run().state == EclState::completed, "The script goes on to its end");
        check(machine.variable(said_yes) == (choice == 0 ? 7 : 0), "Yes compares equal, no not");
    }
    // WAIT KEY; CLEAR AND EXIT; then a byte no command starts with.
    EclMachine machine(script({0x1D, 0x23, 0xFF}));
    check(machine.start(4), "The script starts");
    const auto waiting = machine.run();
    check(waiting.state == EclState::waiting && waiting.request &&
              waiting.request->choices.size() == 1,
          "WAIT KEY waits for Enter");
    check(machine.resume(waiting.request->id, 0), "Enter goes on");
    const auto cleared = machine.run();
    check(cleared.state == EclState::waiting && cleared.request && cleared.request->clear,
          "CLEAR AND EXIT clears the text");
    check(machine.resume(cleared.request->id) && machine.run().state == EclState::completed,
          "and ends the script there");
}

// SUBTRACT stores its result in as many bytes as its destination's operand
// names, as the original's WriteGbvm does: 1 from a byte cell at 0 leaves
// 255 there, from a word cell 65535.
void arithmetic_stores_by_the_destinations_width()
{
    const EclAddress counter{0x0100};
    for (const std::uint8_t tag : {std::uint8_t{1}, std::uint8_t{3}})
    {
        // SUBTRACT 1 from the counter, into the counter.
        EclMachine machine(script({0x05, 0x00, 0x01, tag, 0x00, 0x01, tag, 0x00, 0x01}));
        machine.bind_variable(counter, 0);
        check(machine.start(4) && machine.run().state == EclState::completed,
              "The script subtracts and ends");
        check(machine.variable(counter) == (tag == 1 ? 255 : 65535),
              "A byte cell keeps a byte and a word cell a word");
    }
}

// The item words are in GAME.EXE: 125 slots of 21 bytes.
void the_item_words_are_in_the_game_program()
{
    const auto folder = darkness_folder();
    if (folder.empty())
    {
        std::cout << "Pools of Darkness's files are not here; its item words are not read.\n";
        return;
    }
    std::ifstream program(folder / "GAME.EXE", std::ios::binary);
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(program), {}};
    const auto words = opengold::por::decode_item_words(
        opengold::por::unpack_sound_executable(bytes), darkness_game);
    check(words && words->size() == 126 && (*words)[1] == "Battle Axe" && (*words)[2] == "Hand Axe",
          "GAME.EXE names the item words");
}

// Its item types are numbered as Treasures' are (a long sword 18, the earlier
// games' halberd), so its monsters' items take their names from its words,
// plain ones too.
void the_monsters_items_are_named_by_their_words()
{
    const auto folder = darkness_folder();
    if (folder.empty())
    {
        std::cout << "Pools of Darkness's files are not here; its monsters' items are not named.\n";
        return;
    }
    const auto catalog = opengold::por::CreatureCatalog::load(folder, darkness_game);
    unsigned long_swords = 0;
    std::string misnamed;
    for (const auto &[id, creature] : catalog.all())
    {
        for (const auto &item : creature.equipment)
        {
            if (item.stored.type == 18)
            {
                ++long_swords;
                if (item.label().find("Long Sword") == std::string::npos)
                {
                    misnamed = item.label();
                }
            }
        }
    }
    check(long_swords > 0 && misnamed.empty(),
          "The monsters' long swords are named long swords: " + std::to_string(long_swords) +
              " found, " + misnamed);
}

std::size_t colours(const opengold::Image &image)
{
    std::set<std::uint32_t> seen;
    for (std::size_t p = 0; p + 3 < image.rgba.size(); p += 4)
    {
        seen.insert(image.rgba[p] << 16 | image.rgba[p + 1] << 8 | image.rgba[p + 2]);
    }
    return seen.size();
}

void the_area_map_is_drawn(opengold::por::RolfTourSession &session);
void the_scripts_start_the_music(opengold::por::RolfTourSession &session);

// A new game runs script 16: map 16, New Phlan's docks, with wall set 1, the
// party at (15,0) facing west over picture 148, where Rolf, the portmaster,
// offers a tour. Declining it, he walks the party out, and the docks' walls are
// drawn in the wall set's own colors.
void the_opening_reaches_new_phlan()
{
    const auto folder = darkness_folder();
    if (folder.empty())
    {
        std::cout << "Pools of Darkness's files are not here; its opening is not played.\n";
        return;
    }
    auto session = opengold::load_campaign(folder, darkness_game);
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
    const auto &snapshot = session.snapshot();
    check(snapshot.area_id == 16, "New Phlan's docks are loaded: " + diagnostics());
    check(snapshot.dialogue.find("I AM ROLF, PORTMASTER") != std::string::npos &&
              snapshot.choices == std::vector<std::string>{"YES", "NO"},
          "Rolf offers a tour: " + diagnostics());
    check(session.picture().has_value() && session.picture()->width == 88 &&
              colours(*session.picture()) > 16,
          "Picture 148 shows Rolf in 256 colors");
    check(session.choose(snapshot.continue_ticket, 1), "The party declines");
    settle();
    check(session.snapshot().dialogue.find("THEN LET ME ESCORT YOU OUT") != std::string::npos,
          "Rolf walks the party out: " + diagnostics());
    for (int screen = 0; screen < 8 && session.snapshot().phase == TourPhase::awaiting_continue;
         ++screen)
    {
        check(session.choose(session.snapshot().continue_ticket, 0), "The words go on");
        settle();
    }
    check(session.snapshot().phase == TourPhase::completed, "The opening ends: " + diagnostics());
    // Two steps west from (15,0), then the steps of his table, to the gate's
    // street at (11,2) facing south.
    const auto pose = session.snapshot().pose;
    check(pose.x == 11 && pose.y == 2 && pose.facing == opengold::por::MapDirection::south,
          "Rolf leaves the party at (11,2) facing south");
    check(session.snapshot().footsteps > 0, "His steps are heard");
    check(session.script_diagnostics().empty(), "The opening needs nothing unsupported");
    const auto view = session.observe_view();
    // Behind the walls the daytime sky and ground (SKYGRND 1), not the other
    // games' flat sky: its top row holds more than one color.
    std::set<std::uint32_t> sky;
    for (std::size_t p = 0; p < std::size_t{view.width} * 4; p += 4)
    {
        sky.insert(view.rgba[p] << 16 | view.rgba[p + 1] << 8 | view.rgba[p + 2]);
    }
    check(sky.size() > 1, "The sky is Darkness's picture");
    check(colours(view) > 8, "The 3D view shows the docks' walls in 256 colors");
    check(session.explore(ExplorationCommand::turn_left), "The party turns");
    check(session.observe_view().rgba != view.rgba, "Turning shows another view");
    the_scripts_start_the_music(session);
    the_area_map_is_drawn(session);
}

// A script's PLAY SOUND from 80h starts MUSIC.DAX's piece of that number less
// 80h, again from its start when it is the one playing (eclvm.pas,
// EclPlaySound; music.pas, StartMusic); any sound effect stops it, a script's
// or the party's footstep (music.pas, SendSoundCommand; explore.pas, StepForward).
void the_scripts_start_the_music(opengold::por::RolfTourSession &session)
{
    using opengold::test::TourAccess;
    const auto &snapshot = session.snapshot();
    check(!snapshot.music && snapshot.music_starts == 0, "No music plays on New Phlan's streets");
    TourAccess::play_sound(session, 0x82);
    check(snapshot.music == 2U && snapshot.music_starts == 1, "PLAY SOUND 82h starts piece 2");
    TourAccess::play_sound(session, 0x82);
    check(snapshot.music == 2U && snapshot.music_starts == 2, "The same piece starts again");
    TourAccess::play_sound(session, 0x88);
    check(snapshot.music == 8U, "PLAY SOUND 88h starts piece 8 in its place");
    TourAccess::play_sound(session, 1);
    check(snapshot.music == 8U, "Sound 1 is no effect and leaves the music");
    TourAccess::play_sound(session, 5);
    check(!snapshot.music, "A sound effect stops the music");
    TourAccess::play_sound(session, 0x81);
    const auto footsteps = snapshot.footsteps;
    for (int turn = 0; turn < 4 && snapshot.footsteps == footsteps; ++turn)
    {
        (void)session.explore(ExplorationCommand::forward);
        for (int step = 0; step < 50 && snapshot.phase == TourPhase::running; ++step)
        {
            session.advance(1.0);
        }
        (void)session.explore(ExplorationCommand::turn_left);
    }
    check(snapshot.footsteps != footsteps && !snapshot.music, "The party's footstep stops it");
}

// Outside a dungeon (cell 22h at 0) the view is the area map as the original
// draws it: the tile set's big picture (240 for set 0, cell 3Ah) with the
// party's marker over its square (cells 25h and 26h), one 8-pixel cell each.
void the_area_map_is_drawn(opengold::por::RolfTourSession &session)
{
    using opengold::test::TourAccess;
    TourAccess::set_variable(session, EclAddress{0x22}, 0);
    TourAccess::set_variable(session, EclAddress{0x3A}, 0);
    TourAccess::set_variable(session, EclAddress{0x25}, 26);
    TourAccess::set_variable(session, EclAddress{0x26}, 6);
    const auto map = session.observe_view();
    check(map.width == 304 && map.height == 120 && colours(map) > 16,
          "The area map is picture 240, 38 by 15 squares, in its own colors");
    TourAccess::set_variable(session, EclAddress{0x25}, 25);
    const auto moved = session.observe_view();
    const auto square_differs = [&](unsigned column, unsigned row)
    {
        for (unsigned y = row * 8; y < row * 8 + 8; ++y)
        {
            for (unsigned x = column * 8; x < column * 8 + 8; ++x)
            {
                const auto at = (std::size_t{y} * map.width + x) * 4;
                if (!std::equal(map.rgba.begin() + at, map.rgba.begin() + at + 4,
                                moved.rgba.begin() + at))
                {
                    return true;
                }
            }
        }
        return false;
    };
    check(square_differs(26, 6) && square_differs(25, 6) && !square_differs(10, 10),
          "The marker moves with the party, the rest of the map stays");
}

// Every map of GEO1 is held by a script that loads it (LOAD FILES), so the
// walk (opengold_darkness_walk) walks them all; map 49 is in the file, but no
// script loads it.
void every_map_has_its_script()
{
    const auto folder = darkness_folder();
    if (folder.empty())
    {
        std::cout << "Pools of Darkness's files are not here; its maps are not counted.\n";
        return;
    }
    const auto session = opengold::load_campaign(folder, darkness_game);
    const auto scripts = opengold::test::TourAccess::area_scripts(session);
    const auto maps = opengold::por::MapCatalog::load(folder);
    std::vector<unsigned> without;
    for (const auto &[id, map] : maps.all())
    {
        if (!scripts.contains(id.record))
        {
            without.push_back(id.record);
        }
    }
    std::string missing;
    for (const auto map : without)
    {
        missing += " " + std::to_string(map);
    }
    check(without == std::vector<unsigned>{49}, "Every map has its script; without one:" + missing);
}

} // namespace

// A Secret of the Silver Blades character comes to Pools of Darkness as its
// LoadChar takes one in (saveload.pas, its legacy record): its levels, Hit
// Points, rolled Hit Points, experience and the spells it knew, under Pools
// of Darkness's rules; its items as they were, readied or
// not, but a bundle (type 73) left behind, boots of type 45 made +2 and a
// cloak of type 58 given displacement (effect 89); and its own platinum,
// gems and jewelry, not Pools of Darkness's 200 platinum.
void a_secret_character_comes_across()
{
    const auto darkness = darkness_folder();
    const char *games = std::getenv("OPENGOLD_GOLDBOX_DIR");
    const auto secret = games ? std::filesystem::path(games) / "SECRET" : std::filesystem::path();
    constexpr auto secret_game = GoldBoxGame::secret_of_the_silver_blades;
    if (darkness.empty() || opengold::identify_game(secret) != secret_game)
    {
        std::cout << "Secret's or Pools of Darkness's files are not here; no one comes across.\n";
        return;
    }
    const auto secret_content = opengold::load_original_content(secret, secret_game);
    std::shared_ptr<const opengold::rules::CharacterRules> secret_creation =
        opengold::original::character_rules(opengold::original::default_aging, secret_game);
    opengold::CampaignParty first(opengold::original::rules_module(secret_content),
                                  secret_creation);
    opengold::CharacterCreator creator(secret_creation, 5);
    creator.select(opengold::rules::CreationField::race, "elf");
    creator.select(opengold::rules::CreationField::character_class, "fighter_magic_user");
    creator.name("Veteran");
    opengold::test::walk_original_creation(creator);
    const auto id = first.add_pc(creator.create_character());
    const auto item = [&](std::uint8_t type)
    {
        opengold::por::Equipment equipment;
        equipment.stored.type = type;
        equipment.stored.name_components = {type, 0, 0};
        // Its record's bytes, which its inventory key keeps: type and word.
        equipment.stored.raw[46] = equipment.stored.raw[47] = type;
        equipment.base = secret_content.item_types.at(type);
        return equipment;
    };
    constexpr std::uint8_t dagger = 4, boots = 45, cloak = 58, bundle = 73;
    for (const auto type : {dagger, boots, cloak, bundle})
    {
        first.take_item(id, item(type));
    }
    (void)first.equip(id, first.member(id).character.inventory().items().front().id);
    const auto &veteran = first.member(id);
    // OPENGOLD_SECRET_SAVE_FIXTURE writes the veteran's party, with money, as
    // a Secret save for the party screen's check (darkness_from_secret.cmake).
    if (const char *fixture = std::getenv("OPENGOLD_SECRET_SAVE_FIXTURE"))
    {
        auto wealth = first.state().wealth;
        wealth[static_cast<std::size_t>(opengold::Coin::platinum)] = 1234;
        wealth[static_cast<std::size_t>(opengold::Coin::gems)] = 6;
        wealth[static_cast<std::size_t>(opengold::Coin::jewelry)] = 2;
        first.set_wealth(wealth);
        opengold::write_campaign_file(
            fixture,
            opengold::encode_campaign(first, nullptr, opengold::campaign_asset_identity(secret)));
    }

    std::shared_ptr<const opengold::rules::CharacterRules> creation =
        opengold::original::character_rules(opengold::original::default_aging, darkness_game);
    opengold::CampaignParty second(
        opengold::original::rules_module(opengold::load_original_content(darkness, darkness_game)),
        creation);
    const auto arrived =
        second.bring_across(veteran, opengold::secret_items_for_darkness(darkness),
                            opengold::CampaignParty::BroughtMoney{1234, 6, 2},
                            opengold::CampaignParty::ConvertedItems::readied_as_they_were);
    const auto &member = second.member(arrived);
    const auto &sheet = member.character.sheet();
    const auto &before = *veteran.character.sheet().original;
    check(sheet.original->class_levels == before.class_levels &&
              sheet.hit_points == veteran.character.sheet().hit_points &&
              sheet.original->rolled_hit_points == before.rolled_hit_points &&
              member.experience == veteran.experience &&
              opengold::original::game_of(sheet.identity) == darkness_game,
          "Its levels, Hit Points, rolled Hit Points and experience come along");
    const auto knows = [](const opengold::rules::CharacterSheet &s, int number)
    {
        return std::ranges::find(s.original->known_spells, number) !=
               s.original->known_spells.end();
    };
    for (const int number : before.known_spells)
    {
        check(knows(sheet, number), "It knows every spell it knew: " + std::to_string(number));
    }
    check(!before.known_spells.empty(), "The veteran knew spells");
    // The original's LoadChar copies Secret's attack level (byte 230) into
    // the known spells' place of Fire Touch (118, 318 here), so every one
    // brought across knew it (Bugs).
    constexpr int fire_touch = 318;
    check(!knows(sheet, fire_touch), "It does not learn Fire Touch from its attack level");
    const auto &items = member.character.inventory().items();
    check(items.size() == 3 && member.equipped.size() == 1,
          "Its dagger stays readied; its bundle stays behind");
    for (const auto &kept : items)
    {
        const auto &stored = member.item_sources.at(kept.id).stored;
        check(stored.type != boots || stored.magic_bonus == 2, "Its boots become +2");
        check(stored.type != cloak || stored.effect_codes[1] == 89, "Its cloak gives displacement");
    }
    const auto &wealth = second.state().wealth;
    check(wealth[static_cast<std::size_t>(opengold::Coin::platinum)] == 1234 &&
              wealth[static_cast<std::size_t>(opengold::Coin::gems)] == 6 &&
              wealth[static_cast<std::size_t>(opengold::Coin::jewelry)] == 2,
          "It keeps its own platinum, gems and jewelry");
}

int main()
{
    try
    {
        the_new_commands_run();
        arithmetic_stores_by_the_destinations_width();
        the_item_words_are_in_the_game_program();
        the_monsters_items_are_named_by_their_words();
        the_opening_reaches_new_phlan();
        every_map_has_its_script();
        a_secret_character_comes_across();
        std::cout << "Pools of Darkness passed.\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
