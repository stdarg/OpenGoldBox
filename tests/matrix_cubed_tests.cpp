// Buck Rogers: Matrix Cubed, plan step B1 (docs/BUCK-ROGERS.md): its music
// files through the Audio Interface Library, first on made-up bytes, then the
// game's own files when OPENGOLD_GOLDBOX_DIR names the folder holding them
// (docs/games/matrix-cubed.md). Countdown to Doomsday's tests cover the
// script commands and records both games share; the whole census of the real
// files also runs in tests/game_census.cmake.
#include "opengold/ail_files.h"
#include "opengold/buck_rogers_records.h"
#include "opengold/buck_rogers_scripts.h"
#include "opengold/formats.h"
#include "opengold/game_census.h"
#include "opengold/pool_radiance.h"
#include "opengold/por_sound.h"
#include "opengold/ua_media.h"
#include "test_support.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using opengold::GoldBoxGame;
using opengold::test::check;
using Bytes = std::vector<std::uint8_t>;

constexpr auto matrix = GoldBoxGame::matrix_cubed;

void put_word(Bytes &bytes, std::size_t at, unsigned value)
{
    bytes[at] = static_cast<std::uint8_t>(value);
    bytes[at + 1] = static_cast<std::uint8_t>(value >> 8);
}

void drivers_are_recognized()
{
    constexpr std::string_view notice = "Copyright (C) 1991 John Miles";
    Bytes driver(64);
    put_word(driver, 0, 0x20);
    std::copy(notice.begin(), notice.end(), driver.begin() + 2);
    put_word(driver, 0x20, 100); // Function 100 at 30h,
    put_word(driver, 0x22, 0x30);
    put_word(driver, 0x24, 0xFFFF); // then the table's end.
    const auto read = opengold::ail::decode_driver(driver);
    check(read && read->functions.size() == 1 && read->functions[0][0] == 100,
          "A driver lists its functions after John Miles' notice");
    driver[2] = 'c';
    check(!opengold::ail::decode_driver(driver), "Without the notice it is not a driver");
}

void the_timbre_library_is_indexed()
{
    // Two timbres, each a size word and two bytes, after a 14-byte index.
    Bytes library(14 + 8);
    library[0] = 3; // Patch 3, bank 0, at 14.
    put_word(library, 2, 14);
    library[6] = 5; // Patch 5, bank 1, at 18.
    library[7] = 1;
    put_word(library, 8, 18);
    library[13] = 0xFF; // The index ends.
    put_word(library, 14, 4);
    put_word(library, 18, 4);
    const auto timbres = opengold::ail::decode_global_timbre_library(library);
    check(timbres && timbres->size() == 2 && (*timbres)[1].patch == 5 && (*timbres)[1].bank == 1 &&
              (*timbres)[1].bytes.size() == 4,
          "Each index entry names a patch, a bank and where the timbre starts");
    put_word(library, 18, 9);
    check(!opengold::ail::decode_global_timbre_library(library),
          "A timbre may not run past the end");
}

void the_bank_and_catalog_are_read()
{
    constexpr std::string_view signature = "ADLIB-";
    Bytes bank(28 + 2 * 12 + 30);
    bank[0] = 1;
    std::copy(signature.begin(), signature.end(), bank.begin() + 2);
    put_word(bank, 8, 1);  // One instrument used,
    put_word(bank, 10, 2); // two names,
    put_word(bank, 12, 28);
    put_word(bank, 16, 28 + 24);
    bank[28 + 2] = 1; // the first used, named PIANO.
    const std::string_view piano = "PIANO";
    std::copy(piano.begin(), piano.end(), bank.begin() + 28 + 3);
    const auto instruments = opengold::ail::decode_instrument_bank(bank);
    check(instruments && instruments->size() == 1 && instruments->front().name == "PIANO",
          "An AdLib bank names its instruments");

    const std::string text = ";a catalog\r\noutfile(\"x.ad\")\r\n"
                             "timbre(1,7) = BNK_inst(\"piano\",0)   ;a piano\r\n";
    const auto catalog = opengold::ail::decode_timbre_catalog(Bytes(text.begin(), text.end()));
    check(catalog && catalog->size() == 1 && catalog->front().bank == 1 &&
              catalog->front().patch == 7 && catalog->front().instrument == "piano",
          "The catalog assigns a bank's instrument to each bank and patch");
    const std::string broken = "timbre(1 = BNK_inst(\"piano\",0)\n";
    check(!opengold::ail::decode_timbre_catalog(Bytes(broken.begin(), broken.end())),
          "A timbre line that cannot be read is refused");
}

std::optional<std::filesystem::path> files_of(GoldBoxGame game)
{
    const char *games = std::getenv("OPENGOLD_GOLDBOX_DIR");
    const auto folder = games ? std::filesystem::path(games) / opengold::game_folder(game)
                              : std::filesystem::path();
    if (!games || !std::filesystem::exists(folder / "SHIPS.DAX"))
    {
        std::cout << opengold::game_title(game) << "'s files are not here; they are not read.\n";
        return std::nullopt;
    }
    return folder;
}

Bytes file_bytes(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), {}};
}

Bytes record_of(const std::filesystem::path &path, unsigned id)
{
    const auto archive = opengold::decode_dax_archive(file_bytes(path));
    check(static_cast<bool>(archive), path.filename().string() + " is an archive");
    const auto found = std::find_if(archive.records.begin(), archive.records.end(),
                                    [id](const opengold::DaxRecord &record)
                                    {
                                        return record.id == id;
                                    });
    check(found != archive.records.end(), path.filename().string() + " holds the record");
    return found->bytes;
}

void the_census_is_complete(const std::filesystem::path &folder)
{
    check(opengold::identify_game(folder) == matrix, "The folder is Matrix Cubed's");
    const auto census = opengold::take_census(folder, matrix);
    check(census.complete(), "Every file of Matrix Cubed decodes");
    check(census.foreign_files ==
              std::vector<std::string>{"SEG00036.001", "SEG00036.002", "JOURNAL/"},
          "Two text fragments and an empty folder are not the game's");
}

void the_records_decode(const std::filesystem::path &folder)
{
    const auto refuge =
        opengold::buck_rogers::decode_creature(record_of(folder / "MON0CHA.DAX", 1));
    check(refuge && refuge->name == "SID REFUGE" && refuge->level == 12 &&
              refuge->hit_point_maximum == 84 && refuge->raw.size() == 259,
          "Creature 1 is a level 12 Sid refuge in a 259-byte record");
    const auto image = opengold::por::unpack_sound_executable(file_bytes(folder / "START.EXE"));
    const auto words = opengold::por::decode_item_words(image, matrix);
    check(words && words->size() == 46 && words->at(1) == "Knife",
          "START.EXE names items with 45 words, one more than Countdown's");
    const auto sounds = opengold::por::load_sound_effects(folder, matrix);
    check(sounds.size() == 21 && sounds[9].audible(), "START.EXE's driver plays the footstep");
}

void the_music_needs_only_its_library(const std::filesystem::path &folder)
{
    const auto library =
        opengold::ail::decode_global_timbre_library(file_bytes(folder / "FATV9.AD"));
    check(library && library->size() == 152, "FATV9.AD holds 152 timbres");
    for (const auto *name : {"BUCKA.XMI", "BUCKB.XMI", "BUCKC.XMI"})
    {
        const auto songs = opengold::ua::decode_xmi(file_bytes(folder / name));
        check(songs && !songs->empty(), std::string(name) + " holds songs");
        for (const auto &song : *songs)
        {
            for (const auto &[patch, bank] : song.timbres)
            {
                check(std::any_of(library->begin(), library->end(),
                                  [&](const opengold::ail::GlobalTimbre &timbre)
                                  {
                                      return timbre.patch == patch && timbre.bank == bank;
                                  }),
                      std::string(name) + " asks only for timbres FATV9.AD holds");
            }
        }
    }
    for (const auto *name : {"ADLIB.ADV", "SBFM.ADV", "MT32MPU.ADV", "PCSPKR.ADV", "TANDY.ADV"})
    {
        check(opengold::ail::decode_driver(file_bytes(folder / name)).has_value(),
              std::string(name) + " is an AIL driver");
    }
}

void the_scripts_are_listed(const std::filesystem::path &folder)
{
    const auto listing = opengold::list_buck_rogers_scripts(folder, matrix);
    check(listing.stalls.empty(), "No script reaches 3Bh or 3Ch");
    check(listing.uneven_skips.empty(), "No IF skips a command the original skips unevenly");
    check(listing.skill_checks.size() == 120, "The scripts check skills 120 times");
    check(listing.ship_combats.size() == 4, "Four ship combats");
    const auto end = std::find_if(listing.programs.begin(), listing.programs.end(),
                                  [](const opengold::ScriptCommandUse &use)
                                  {
                                      return use.operands == std::vector<std::string>{"2"};
                                  });
    check(end != listing.programs.end() && end->place.script == 114 &&
              end->place.address == 0x9E08 &&
              std::count_if(listing.programs.begin(), listing.programs.end(),
                            [](const opengold::ScriptCommandUse &use)
                            {
                                return use.operands == std::vector<std::string>{"2"};
                            }) == 1,
          "Only script 114 ends the game, with PROGRAM 2 after the heroes' welcome");
}

void the_files_decode()
{
    const auto folder = files_of(matrix);
    if (!folder)
    {
        return;
    }
    the_census_is_complete(*folder);
    the_records_decode(*folder);
    the_music_needs_only_its_library(*folder);
    the_scripts_are_listed(*folder);
}

} // namespace

int main()
{
    try
    {
        drivers_are_recognized();
        the_timbre_library_is_indexed();
        the_bank_and_catalog_are_read();
        the_files_decode();
        std::cout << "Matrix Cubed passed.\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
