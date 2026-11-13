#include "opengold/game_census.h"

#include "archive_io.h"
#include "opengold/character_art.h"
#include "opengold/curse_records.h"
#include "opengold/ecl.h"
#include "opengold/formats.h"
#include "opengold/geo_map.h"
#include "opengold/path_text.h"
#include "opengold/pool_radiance.h"
#include "opengold/por_sound.h"
#include "opengold/wall_art.h"

#include <algorithm>
#include <array>
#include <functional>
#include <map>
#include <set>
#include <stdexcept>
#include <string_view>

namespace opengold
{
namespace
{

using Bytes = std::vector<std::uint8_t>;

struct GameFile
{
    std::string name; // Upper case, as the original names it.
    std::filesystem::path path;
};

// The file's name without its extension and without the disk number at its end:
// "ECL3.DAX" is "ECL", "8X8D1.DAX" is "8X8D".
std::string family(std::string_view name)
{
    auto stem = std::string(name.substr(0, name.find('.')));
    while (stem.size() > 1 && stem.back() >= '0' && stem.back() <= '9')
    {
        stem.pop_back();
    }
    return stem;
}

std::string extension(std::string_view name)
{
    const auto dot = name.rfind('.');
    return dot == std::string_view::npos ? std::string{} : std::string(name.substr(dot + 1));
}

bool is_program_file(const GameFile &file)
{
    static const std::set<std::string> programs{"EXE", "OVR", "COM", "BAT", "CFG", "DRV"};
    return file.name != "START.EXE" && programs.contains(extension(file.name));
}

Bytes read_bytes(const std::filesystem::path &path)
{
    const auto name = path_text(path);
    return por::detail::read_archive_bytes<std::runtime_error>(
        path, name + " is too large", "Cannot open " + name, "Cannot read " + name);
}

std::vector<DaxRecord> archive_records(const GameFile &file)
{
    auto archive = decode_dax_archive(read_bytes(file.path));
    if (!archive)
    {
        throw std::runtime_error("not a valid DAX archive");
    }
    return std::move(archive.records);
}

class RowBuilder
{
  public:
    explicit RowBuilder(std::string kind)
    {
        row_.kind = std::move(kind);
    }

    void add_file(const std::string &name)
    {
        row_.files.push_back(name);
    }

    // Counts one piece: decoded when the check returns without throwing.
    void check(const std::string &where, const std::function<void()> &decode)
    {
        ++row_.pieces;
        try
        {
            decode();
            ++row_.decoded;
        }
        catch (const std::exception &error)
        {
            fail(where, error.what());
        }
    }

    void fail(const std::string &where, const std::string &why)
    {
        if (row_.first_failure.empty())
        {
            row_.first_failure = where + ": " + why;
        }
    }

    [[nodiscard]] CensusRow take() &&
    {
        return std::move(row_);
    }

  private:
    CensusRow row_;
};

std::string record_name(const GameFile &file, const DaxRecord &record)
{
    return file.name + " record " + std::to_string(record.id);
}

void require(bool decoded, const char *why)
{
    if (!decoded)
    {
        throw std::runtime_error(why);
    }
}

// Each record of each file through one decoder.
void check_records(RowBuilder &row, const std::vector<GameFile> &files,
                   const std::function<void(const GameFile &, const DaxRecord &)> &decode)
{
    for (const auto &file : files)
    {
        row.add_file(file.name);
        std::vector<DaxRecord> records;
        try
        {
            records = archive_records(file);
        }
        catch (const std::exception &error)
        {
            row.check(file.name,
                      [&error]
                      {
                          throw std::runtime_error(error.what());
                      });
            continue;
        }
        for (const auto &record : records)
        {
            row.check(record_name(file, record),
                      [&]
                      {
                          decode(file, record);
                      });
        }
    }
}

void check_creature(GoldBoxGame game, const Bytes &bytes)
{
    if (game != GoldBoxGame::pool_of_radiance && game != GoldBoxGame::curse_of_the_azure_bonds &&
        game != GoldBoxGame::secret_of_the_silver_blades)
    {
        throw std::runtime_error("this game's creature records are not decoded yet");
    }
    require(por::decode_character(bytes, game).has_value(), "not a creature record");
}

void check_script(GoldBoxGame game, const GameFile &file, const DaxRecord &record)
{
    const auto program = por::EclProgram::decode(record.bytes, record_name(file, record), game);
    // A path reached only by falling through an ON GOTO is the walker's guess:
    // scripts keep the choice in range, and data may follow the ON GOTO.
    for (const auto &error : por::walk_reachable(program).errors)
    {
        if (!error.fallthrough_from)
        {
            throw std::runtime_error("instruction at " + std::to_string(error.address) + ": " +
                                     error.message);
        }
    }
}

// A picture, or an animation whose frames are pictures.
void check_picture(const Bytes &bytes)
{
    require(decode_ega_picture(bytes) || decode_ega_animation(bytes),
            "neither a picture nor an animation");
}

void check_combat_icon(const GameFile &file, const DaxRecord &record)
{
    const auto bytes = read_bytes(file.path);
    require(static_cast<bool>(decode_ega_combat_icon(bytes, record.id, 0)), "not a combat icon");
}

void check_sprite(const GameFile &file, const DaxRecord &record)
{
    require(!record.bytes.empty(), "empty sprite");
    const auto bytes = read_bytes(file.path);
    for (std::uint8_t frame = 0; frame < record.bytes[0]; ++frame)
    {
        require(static_cast<bool>(decode_ega_sprite(bytes, record.id, frame)),
                "a sprite frame does not decode");
    }
}

const Bytes *find_record(const std::vector<DaxRecord> &records, unsigned id)
{
    const auto found = std::find_if(records.begin(), records.end(),
                                    [id](const DaxRecord &record)
                                    {
                                        return record.id == id;
                                    });
    return found == records.end() ? nullptr : &found->bytes;
}

// A wall definition block is drawn with the shared tiles (8X8D1 record 203) and
// the tiles of its own disk's 8X8D file: record id, or id * 10 + n for block n
// of a bank of several blocks (the original's LoadWallSet).
void check_wall_art(const std::map<std::string, GameFile> &by_name, const GameFile &file,
                    const DaxRecord &record)
{
    constexpr std::size_t block_size = 780;
    const auto blocks = record.bytes.size() / block_size;
    require(blocks > 0 && record.bytes.size() % block_size == 0, "not whole wall blocks");
    const auto shared_file = by_name.find("8X8D1.DAX");
    require(shared_file != by_name.end(), "8X8D1.DAX is missing");
    const auto shared = archive_records(shared_file->second);
    const auto *common = find_record(shared, 203);
    require(common != nullptr, "8X8D1.DAX has no shared tiles");
    por::WallTiles tiles(1); // Tile zero is blank.
    const auto common_tiles = por::decode_wall_tiles(*common);
    require(common_tiles.has_value(), "the shared tiles do not decode");
    tiles.insert(tiles.end(), common_tiles->begin(), common_tiles->end());

    const auto disk = file.name.substr(std::string_view("WALLDEF").size());
    const auto local_file = by_name.find("8X8D" + disk);
    require(local_file != by_name.end(), "the disk's 8X8D file is missing");
    const auto local = archive_records(local_file->second);
    for (std::size_t block = 0; block < blocks; ++block)
    {
        const auto id = blocks == 1 ? record.id : record.id * 10 + block + 1;
        const auto *bank = find_record(local, static_cast<unsigned>(id));
        // Pool of Radiance's Phlan block 0 is drawn with tiles 101 to 103 (rolf_tour.cpp).
        if (bank == nullptr && record.id == 0)
        {
            bank = find_record(local, static_cast<unsigned>(100 + block + 1));
        }
        require(bank != nullptr, "its tiles are missing");
        const auto block_tiles = por::decode_wall_tiles(*bank);
        require(block_tiles.has_value(), "its tiles do not decode");
        tiles.insert(tiles.end(), block_tiles->begin(), block_tiles->end());
    }
    // The original draws every wall from one space of symbols: the shared ones,
    // then the three slots' tiles, 70 a slot. Secret of the Silver Blades'
    // walls also use the tiles of the sets loaded beside them (docs/games/secret.md),
    // which this record alone cannot name, so the slots it leaves free are blank.
    constexpr std::size_t symbol_space = 1 + 45 + 3 * 70;
    tiles.resize(std::max(tiles.size(), symbol_space));
    const auto art = por::decode_wall_art(record.bytes, tiles);
    require(art.has_value() && art->appearances.size() == 5 * blocks, "not wall art");
}

void check_names_and_sounds(RowBuilder &row, GoldBoxGame game, const GameFile &start,
                            const std::filesystem::path &folder)
{
    row.add_file(start.name);
    const auto image = por::unpack_sound_executable(read_bytes(start.path));
    row.check(start.name + " item names",
              [&]
              {
                  require(por::decode_item_words(image, game).has_value(), "no item name table");
              });
    row.check(start.name + " spell names",
              [&]
              {
                  require(por::decode_spell_names(image).has_value(), "no spell name table");
              });
    row.check(start.name + " sound effects",
              [&]
              {
                  const auto effects = por::load_sound_effects(folder, game);
                  require(std::any_of(effects.begin(), effects.end(),
                                      [](const por::SoundEffect &effect)
                                      {
                                          return effect.audible();
                                      }),
                          "no audible sound effect");
              });
}

using RecordDecoder = std::function<void(const GameFile &, const DaxRecord &)>;

struct FamilyDecoder
{
    std::string_view description;
    RecordDecoder decode;
};

std::map<std::string, FamilyDecoder, std::less<>>
family_decoders(GoldBoxGame game, const std::map<std::string, GameFile> &by_name)
{
    const auto picture = [](const GameFile &, const DaxRecord &record)
    {
        check_picture(record.bytes);
    };
    const auto icon_part = [](const GameFile &, const DaxRecord &record)
    {
        static_cast<void>(por::decode_character_icon(record.bytes));
    };
    const auto items = [game](const GameFile &, const DaxRecord &record)
    {
        require(por::decode_items(record.bytes, game).has_value(), "not an item list");
    };
    std::map<std::string, FamilyDecoder, std::less<>> decoders{
        {"GEO",
         {"maps",
          [](const GameFile &, const DaxRecord &record)
          {
              require(por::decode_geo_map(record.bytes).has_value(), "not a map");
          }}},
        {"ECL",
         {"scripts",
          [game](const GameFile &file, const DaxRecord &record)
          {
              check_script(game, file, record);
          }}},
        {"8X8D",
         {"wall tiles and the font",
          [](const GameFile &file, const DaxRecord &record)
          {
              // Secret of the Silver Blades keeps the font alone in 8X8D5.
              if (record.id == 201 && (file.name == "8X8D1.DAX" || file.name == "8X8D5.DAX"))
              {
                  require(decode_game_font(record.bytes).has_value(), "not the font");
                  return;
              }
              require(por::decode_wall_tiles(record.bytes).has_value(), "not 8x8 tiles");
          }}},
        {"WALLDEF",
         {"wall art",
          [&by_name](const GameFile &file, const DaxRecord &record)
          {
              check_wall_art(by_name, file, record);
          }}},
        {"MONCHA",
         {"creatures",
          [game](const GameFile &, const DaxRecord &record)
          {
              check_creature(game, record.bytes);
          }}},
        {"MONITM", {"creatures' items", items}},
        {"ITEM", {"item lists", items}},
        {"MONSPC",
         {"creatures' effects",
          [](const GameFile &, const DaxRecord &record)
          {
              require(por::decode_effects(record.bytes).has_value(), "not an effect list");
          }}},
        {"CHEAD", {"combat heads", icon_part}},
        {"CBODY", {"combat bodies", icon_part}},
        {"SPRIT",
         {"sprites", [](const GameFile &file, const DaxRecord &record)
          {
              check_sprite(file, record);
          }}}};
    for (const auto *name :
         {"HEAD", "BODY", "PIC", "TITLE", "BIGPIC", "BACPAC", "SQRPACI", "FINAL", "SKY"})
    {
        decoders.emplace(name, FamilyDecoder{"pictures", picture});
    }
    for (const auto *name : {"CPIC", "COMSPR", "ICON", "DUNGCOM", "RANDCOM", "WILDCOM"})
    {
        decoders.emplace(name, FamilyDecoder{"combat icons", check_combat_icon});
    }
    // Curse's copy protection draws its symbols from TILES.DAX, in the combat icons' format.
    decoders.emplace("TILES", FamilyDecoder{"copy-protection symbols", check_combat_icon});
    return decoders;
}

// MON3CHA.DAX belongs to the family MONCHA: the disk number sits in the middle.
std::string decoder_family(std::string_view name)
{
    auto stem = family(name);
    if (stem.starts_with("MON") && stem.size() > 3)
    {
        auto rest = stem.substr(3);
        while (!rest.empty() && rest.front() >= '0' && rest.front() <= '9')
        {
            rest.erase(rest.begin());
        }
        return "MON" + rest;
    }
    return stem;
}

std::vector<GameFile> folder_files(const std::filesystem::path &folder,
                                   std::vector<std::string> &directories)
{
    std::vector<GameFile> files;
    for (const auto &entry : std::filesystem::directory_iterator(folder))
    {
        auto name = por::detail::upper_ascii(path_text(entry.path().filename()));
        if (entry.is_directory())
        {
            directories.push_back(name + "/");
        }
        else if (entry.is_regular_file())
        {
            files.push_back({std::move(name), entry.path()});
        }
    }
    std::sort(files.begin(), files.end(),
              [](const GameFile &a, const GameFile &b)
              {
                  return a.name < b.name;
              });
    std::sort(directories.begin(), directories.end());
    return files;
}

} // namespace

bool GameCensus::complete() const noexcept
{
    return unread_files.empty() && std::all_of(rows.begin(), rows.end(),
                                               [](const CensusRow &row)
                                               {
                                                   return row.complete();
                                               });
}

std::optional<GoldBoxGame> identify_game(const std::filesystem::path &folder)
{
    // Each file below exists in only one of the games' folders.
    constexpr std::array<std::pair<std::string_view, GoldBoxGame>, 7> markers{{
        {"CKIT.EXE", GoldBoxGame::unlimited_adventures},
        {"SQRPACI.DAX", GoldBoxGame::pool_of_radiance},
        {"TILES.DAX", GoldBoxGame::curse_of_the_azure_bonds},
        {"SOUND.DAX", GoldBoxGame::secret_of_the_silver_blades},
        {"ITEMS.1", GoldBoxGame::gateway_to_the_savage_frontier},
        {"INTRO.BMA", GoldBoxGame::pools_of_darkness},
        {"LUCK.DAX", GoldBoxGame::treasures_of_the_savage_frontier},
    }};
    std::error_code error;
    if (!std::filesystem::is_directory(folder, error))
    {
        return std::nullopt;
    }
    std::set<std::string, std::less<>> names;
    for (const auto &entry : std::filesystem::directory_iterator(folder, error))
    {
        names.insert(por::detail::upper_ascii(path_text(entry.path().filename())));
    }
    for (const auto &[marker, game] : markers)
    {
        if (names.contains(marker))
        {
            return game;
        }
    }
    return std::nullopt;
}

GameCensus take_census(const std::filesystem::path &folder, GoldBoxGame game)
{
    GameCensus census;
    census.game = game;
    std::vector<std::string> directories;
    const auto files = folder_files(folder, directories);
    std::map<std::string, GameFile> by_name;
    for (const auto &file : files)
    {
        by_name.emplace(file.name, file);
    }
    const auto decoders = family_decoders(game, by_name);

    // The files of each family, in the order the decoders list them.
    std::map<std::string, std::vector<GameFile>> families;
    for (const auto &file : files)
    {
        if (is_program_file(file))
        {
            census.program_files.push_back(file.name);
            continue;
        }
        const auto name = decoder_family(file.name);
        if (extension(file.name) == "DAX" && decoders.contains(name))
        {
            families[name].push_back(file);
        }
        else if (file.name == "ITEMS")
        {
            RowBuilder row("ITEMS: item types");
            row.add_file(file.name);
            row.check(file.name,
                      [&file]
                      {
                          require(por::decode_item_templates(read_bytes(file.path)).has_value(),
                                  "not item types");
                      });
            census.rows.push_back(std::move(row).take());
        }
        else if (file.name == "START.EXE")
        {
            RowBuilder row("START.EXE: item names, spell names and sounds");
            try
            {
                check_names_and_sounds(row, game, file, folder);
            }
            catch (const std::exception &error)
            {
                row.check(file.name,
                          [&error]
                          {
                              throw std::runtime_error(error.what());
                          });
            }
            census.rows.push_back(std::move(row).take());
        }
        else
        {
            census.unread_files.push_back(file.name);
        }
    }
    for (const auto &[name, members] : families)
    {
        const auto &decoder = decoders.find(name)->second;
        RowBuilder row(name + ": " + std::string(decoder.description));
        check_records(row, members, decoder.decode);
        census.rows.push_back(std::move(row).take());
    }
    for (auto &directory : directories)
    {
        // The SAVE folder holds the player's saved games, not the game's data.
        if (directory == "SAVE/")
        {
            census.program_files.push_back(std::move(directory));
        }
        else
        {
            census.unread_files.push_back(std::move(directory));
        }
    }
    std::sort(census.rows.begin(), census.rows.end(),
              [](const CensusRow &a, const CensusRow &b)
              {
                  return a.kind < b.kind;
              });
    return census;
}

} // namespace opengold
