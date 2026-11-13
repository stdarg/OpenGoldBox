#include "opengold/game_census.h"

#include "archive_io.h"
#include "opengold/character_art.h"
#include "opengold/curse_records.h"
#include "opengold/ecl.h"
#include "opengold/formats.h"
#include "opengold/gateway_formats.h"
#include "opengold/geo_map.h"
#include "opengold/music_bank.h"
#include "opengold/path_text.h"
#include "opengold/pool_radiance.h"
#include "opengold/por_sound.h"
#include "opengold/treasures_texts.h"
#include "opengold/vga_picture.h"
#include "opengold/vace_film.h"
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
    if (game == GoldBoxGame::unlimited_adventures)
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

// Gateway to the Savage Frontier's pictures carry a color header
// (docs/games/gateway.md); every frame is drawn to be sure its colors read.
void check_color_header_picture(const Bytes &bytes)
{
    if (const auto picture = gateway::decode_picture(bytes))
    {
        for (std::size_t frame = 0; frame < picture->frames.size(); ++frame)
        {
            static_cast<void>(gateway::picture_image(*picture, frame));
        }
        return;
    }
    const auto animation = gateway::decode_animation(bytes);
    require(animation.has_value(), "neither a picture nor an animation");
    for (const auto &frame : *animation)
    {
        static_cast<void>(gateway::picture_image(frame.picture));
    }
}

struct ColorHeaderFamily
{
    std::string_view name, description;
    std::function<void(const GameFile &, const DaxRecord &)> decode;
};

// Gateway's decoders in place of the earlier games' for the families whose
// format it changed: pictures and combat icons with a color header, and its
// digitized sound effects.
std::vector<ColorHeaderFamily> color_header_families()
{
    const auto picture = [](const GameFile &, const DaxRecord &record)
    {
        check_color_header_picture(record.bytes);
    };
    const auto combat_icon = [](const GameFile &, const DaxRecord &record)
    {
        require(gateway::decode_picture(record.bytes).has_value(), "not a combat icon");
    };
    std::vector<ColorHeaderFamily> families;
    for (const auto *name : {"HEAD", "BODY", "PIC", "TITLE", "BIGPIC", "SKY"})
    {
        families.push_back({name, "pictures", picture});
    }
    for (const auto *name : {"CPIC", "COMSPR", "DUNGCOM", "RANDCOM", "WILDCOM"})
    {
        families.push_back({name, "combat icons", combat_icon});
    }
    families.push_back({"ZOUNDS", "sound effects", [](const GameFile &, const DaxRecord &record)
                        {
                            require(
                                gateway::decode_sound_sample(record.id, record.bytes).has_value(),
                                "not a sound effect");
                        }});
    return families;
}

// A record of the music driver's archive: the driver program, or one of its
// parameter blocks of instruments and songs.
void check_music(GoldBoxGame game, const Bytes &bytes)
{
    if (is_music_driver(bytes))
    {
        return;
    }
    require(decode_music_bank(bytes, game).has_value(), "neither the music driver nor its songs");
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

// Pools of Darkness and Treasures draw in 256 colors: their pictures, tiles and
// icons share one layout, their PIC and SPRIT animations another
// (docs/games/darkness.md).
bool uses_vga_pictures(GoldBoxGame game)
{
    return game == GoldBoxGame::pools_of_darkness ||
           game == GoldBoxGame::treasures_of_the_savage_frontier;
}

void check_vga_picture(const Bytes &bytes)
{
    require(decode_vga_picture(bytes).has_value(), "not a 256-color picture");
}

void check_vga_tiles(const Bytes &bytes)
{
    const auto tiles = decode_vga_picture(bytes);
    require(tiles && tiles->frames.front().width == 8 && tiles->frames.front().height == 8,
            "not 8x8 tiles");
}

// A wall definition record is drawn with the 8X8D1.DAX record of its own id
// (the original's LoadWallSet and Load8x8Set).
void check_vga_wall_art(const std::map<std::string, GameFile> &by_name, const DaxRecord &record)
{
    const auto tile_file = by_name.find("8X8D1.DAX");
    require(tile_file != by_name.end(), "8X8D1.DAX is missing");
    const auto tile_records = archive_records(tile_file->second);
    const auto *tile_record = find_record(tile_records, record.id);
    require(tile_record != nullptr, "its tiles are missing");
    const auto tiles = decode_vga_picture(*tile_record);
    require(tiles.has_value(), "its tiles do not decode");
    require(por::decode_vga_wall_art(record.bytes, *tiles).has_value(), "not wall art");
}

// Treasures' BORD.DAX: the frames' tiles and pictures, the font (record 8, a
// picture of 64 glyphs), the frames' tile runs, and two name tables.
void check_border_record(const DaxRecord &record)
{
    switch (record.id)
    {
    case 7:
        require(treasures::decode_border_runs(record.bytes).has_value(),
                "not runs of border tiles");
        return;
    case 9:
        require(treasures::decode_name_table(record.bytes, 37).has_value(), "not the effect names");
        return;
    case 10:
        require(treasures::decode_name_table(record.bytes, 20).has_value(), "not the item words");
        return;
    default:
        check_vga_picture(record.bytes);
    }
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

bool is_film_file(const GameFile &file)
{
    const auto kind = extension(file.name);
    return kind == "ACF" || kind == "BMA";
}

// Pools of Darkness' opening and ending films: each a control file (.ACF) and
// its frames (.BMA), played by VACE.EXE (docs/games/darkness.md, "The opening and ending films"). One
// piece a film.
void add_film_row(GameCensus &census, const std::map<std::string, GameFile> &by_name,
                  const std::vector<GameFile> &film_files)
{
    if (film_files.empty())
    {
        return;
    }
    RowBuilder row("FILM: opening and ending films");
    std::set<std::string> films;
    for (const auto &file : film_files)
    {
        row.add_file(file.name);
        films.insert(file.name.substr(0, file.name.rfind('.')));
    }
    for (const auto &film : films)
    {
        row.check(film + " film",
                  [&]
                  {
                      const auto control = by_name.find(film + ".ACF");
                      const auto frames = by_name.find(film + ".BMA");
                      require(control != by_name.end() && frames != by_name.end(),
                              "needs both its .ACF and its .BMA");
                      require(decode_vace_film(read_bytes(control->second.path),
                                               read_bytes(frames->second.path))
                                  .has_value(),
                              "not a film");
                  });
    }
    census.rows.push_back(std::move(row).take());
}

using RecordDecoder = std::function<void(const GameFile &, const DaxRecord &)>;

struct FamilyDecoder
{
    std::string_view description;
    RecordDecoder decode;
};

// The 256-color games' pictures in place of the EGA games' decoders, and the
// files only those games have.
void add_vga_decoders(std::map<std::string, FamilyDecoder, std::less<>> &decoders,
                      const std::map<std::string, GameFile> &by_name)
{
    const auto picture = [](const GameFile &, const DaxRecord &record)
    {
        check_vga_picture(record.bytes);
    };
    const auto picture_or_animation = [](const GameFile &, const DaxRecord &record)
    {
        require(decode_vga_animation(record.bytes) || decode_vga_picture(record.bytes),
                "neither a 256-color picture nor an animation");
    };
    const auto sprite = [](const GameFile &, const DaxRecord &record)
    {
        require(decode_vga_animation(record.bytes).has_value(), "not a 256-color sprite");
    };
    const auto tiles_and_font = [](const GameFile &file, const DaxRecord &record)
    {
        if (record.id == 201 && file.name == "8X8D0.DAX")
        {
            require(decode_game_font(record.bytes).has_value(), "not the font");
            return;
        }
        check_vga_tiles(record.bytes);
    };
    const auto wall_art = [&by_name](const GameFile &, const DaxRecord &record)
    {
        check_vga_wall_art(by_name, record);
    };
    const auto border = [](const GameFile &, const DaxRecord &record)
    {
        check_border_record(record);
    };
    const auto luck_text = [](const GameFile &, const DaxRecord &record)
    {
        require(treasures::decode_luck_text(record.bytes).has_value(), "not a text");
    };
    const std::array<std::pair<std::string_view, FamilyDecoder>, 20> vga_families{{
        {"TITLE", {"pictures", picture}},
        {"BIGPIC", {"pictures", picture}},
        // Treasures' PIC1A.DAX continues PIC1.DAX. Both also hold records that
        // only set colors, in the still pictures' layout.
        {"PIC", {"pictures", picture_or_animation}},
        {"PIC1A", {"pictures", picture_or_animation}},
        {"SPRIT", {"sprites", sprite}},
        {"CHEAD", {"combat heads", picture}},
        {"CBODY", {"combat bodies", picture}},
        {"CPIC", {"combat icons", picture}},
        {"COMSPR", {"combat icons", picture}},
        {"DUNGCOM", {"combat icons", picture}},
        {"WILDCOM", {"combat icons", picture}},
        {"8X8D", {"wall tiles and the font", tiles_and_font}},
        {"WALLDEF", {"wall art", wall_art}},
        {"BACK", {"backdrops", picture}},
        {"SKYGRND", {"skies", picture}},
        {"BORDERS", {"border tiles", picture}},
        {"CURSOR", {"the cursor", picture}},
        {"OVER", {"overland maps", picture}},
        {"BORD", {"frames, the font and name tables", border}},
        {"LUCK", {"luck, lucky paper and romance texts", luck_text}},
    }};
    for (const auto &[name, decoder] : vga_families)
    {
        decoders.insert_or_assign(std::string(name), decoder);
    }
}

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
    if (game == GoldBoxGame::gateway_to_the_savage_frontier)
    {
        for (const auto &family : color_header_families())
        {
            decoders.insert_or_assign(std::string(family.name),
                                      FamilyDecoder{family.description, family.decode});
        }
    }

    const auto music = [game](const GameFile &, const DaxRecord &record)
    {
        check_music(game, record.bytes);
    };
    // Secret of the Silver Blades names the music driver's archive SOUND.DAX.
    decoders.emplace("MUSIC", FamilyDecoder{"music", music});
    decoders.emplace("SOUND", FamilyDecoder{"music", music});
    if (game == GoldBoxGame::pools_of_darkness ||
        game == GoldBoxGame::treasures_of_the_savage_frontier)
    {
        decoders.insert_or_assign(
            "ZOUNDS",
            FamilyDecoder{
                "sound effects", [](const GameFile &, const DaxRecord &record)
                {
                    require(gateway::decode_later_sound_sample(record.id, record.bytes).has_value(),
                            "not a sound effect");
                }});
    }

    // Curse's copy protection draws its symbols from TILES.DAX, in the combat icons' format.
    decoders.emplace("TILES", FamilyDecoder{"copy-protection symbols", check_combat_icon});
    if (uses_vga_pictures(game))
    {
        add_vga_decoders(decoders, by_name);
    }
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
    std::vector<GameFile> film_files;
    // Pools of Darkness' introduction and finale play songs of their own.
    RowBuilder song_files("SMS: the introduction's and finale's songs");
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
        else if (is_film_file(file))
        {
            film_files.push_back(file);
        }
        // Gateway to the Savage Frontier names the file ITEMS.1.
        else if (file.name == "ITEMS" || file.name == "ITEMS.1")
        {
            RowBuilder row("ITEMS: item types");
            row.add_file(file.name);
            row.check(file.name,
                      [&file, game]
                      {
                          require(
                              por::decode_item_templates(read_bytes(file.path), game).has_value(),
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
        else if (extension(file.name) == "SMS")
        {
            song_files.add_file(file.name);
            song_files.check(file.name,
                             [&file, game]
                             {
                                 require(decode_music_bank(read_bytes(file.path), game).has_value(),
                                         "not the music driver's songs");
                             });
        }
        else
        {
            census.unread_files.push_back(file.name);
        }
    }
    add_film_row(census, by_name, film_files);
    if (auto row = std::move(song_files).take(); row.pieces > 0)
    {
        census.rows.push_back(std::move(row));
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
