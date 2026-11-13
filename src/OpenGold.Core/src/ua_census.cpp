#include "ua_census.h"

#include "archive_io.h"
#include "census_rows.h"
#include "opengold/path_text.h"
#include "opengold/pool_radiance.h"
#include "opengold/ua_formats.h"
#include "opengold/ua_media.h"

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

using census_detail::RowBuilder;
using Bytes = std::vector<std::uint8_t>;

struct GameFile
{
    std::string name; // Upper case, with its folder: "DISK2/PICA.TLB".
    std::filesystem::path path;
};

std::string base_name(std::string_view name)
{
    const auto slash = name.rfind('/');
    return std::string(slash == std::string_view::npos ? name : name.substr(slash + 1));
}

std::string stem(std::string_view name)
{
    const auto base = base_name(name);
    return base.substr(0, base.find('.'));
}

std::string extension(std::string_view name)
{
    const auto dot = name.rfind('.');
    return dot == std::string_view::npos ? std::string{} : std::string(name.substr(dot + 1));
}

// The folders the game's data sits in.
constexpr std::array<std::string_view, 4> data_folders{"ART", "DISK1", "DISK2", "DISK3"};

// Programs, sound drivers and their timbres, settings and the designer's
// printed supplement: OpenGoldBox never reads them.
bool is_never_read(const std::string &name)
{
    static const std::set<std::string, std::less<>> extensions{"EXE", "BAT", "ADV", "CFG"};
    static const std::set<std::string, std::less<>> names{"INSTR.AD", "UAHELP.TXT"};
    return extensions.contains(extension(name)) || names.contains(base_name(name));
}

void add_files(const std::filesystem::path &folder, const std::string &prefix,
               std::vector<GameFile> &files, std::vector<std::string> &directories)
{
    for (const auto &entry : std::filesystem::directory_iterator(folder))
    {
        auto name = por::detail::upper_ascii(path_text(entry.path().filename()));
        if (entry.is_regular_file())
        {
            files.push_back({prefix + name, entry.path()});
        }
        else if (entry.is_directory())
        {
            const bool data = prefix.empty() && std::find(data_folders.begin(), data_folders.end(),
                                                          name) != data_folders.end();
            if (data)
            {
                add_files(entry.path(), name + "/", files, directories);
            }
            else
            {
                directories.push_back(prefix + name + "/");
            }
        }
    }
}

Bytes read_bytes(const GameFile &file)
{
    const auto name = path_text(file.path);
    return por::detail::read_archive_bytes<std::runtime_error>(
        file.path, name + " is too large", "Cannot open " + name, "Cannot read " + name);
}

void require(bool decoded, const char *why)
{
    if (!decoded)
    {
        throw std::runtime_error(why);
    }
}

std::string item_name(const GameFile &file, std::size_t item)
{
    return file.name + " item " + std::to_string(item);
}

// A file whose library does not open is one piece that did not decode.
std::optional<ua::Library> open_library(RowBuilder &row, const GameFile &file)
{
    row.add_file(file.name);
    std::optional<ua::Library> library;
    try
    {
        library = ua::decode_library(read_bytes(file));
    }
    catch (const std::exception &error)
    {
        row.fail(file.name, error.what());
    }
    if (!library)
    {
        row.check(file.name,
                  []
                  {
                      throw std::runtime_error("not a resource library");
                  });
    }
    return library;
}

void check_index(RowBuilder &row, const std::string &where, const ua::Library &library)
{
    row.check(where,
              [&library]
              {
                  require(ua::decode_index(library).has_value(), "not an index");
              });
}

// Every glyph of a TILE library is a piece, and so is its index. A composite
// decodes when every glyph it names is an image.
void check_tile_library(RowBuilder &row, const std::string &where, const ua::Library &library)
{
    if (library.signature != "TILE")
    {
        row.check(where,
                  []
                  {
                      throw std::runtime_error("not a tile library");
                  });
        return;
    }
    std::vector<std::optional<ua::Glyph>> glyphs;
    for (std::size_t item = 0; item < library.items.size(); ++item)
    {
        const bool index = item == 0 && library.indexed;
        glyphs.push_back(index ? std::nullopt : ua::decode_glyph(library.items[item]));
    }
    auto is_image = [&glyphs](std::size_t glyph)
    {
        return glyph < glyphs.size() && glyphs[glyph] &&
               glyphs[glyph]->kind == ua::GlyphKind::image;
    };
    for (std::size_t item = 0; item < library.items.size(); ++item)
    {
        if (item == 0 && library.indexed)
        {
            check_index(row, where + " index", library);
            continue;
        }
        row.check(where + " glyph " + std::to_string(item),
                  [&]
                  {
                      const auto &glyph = glyphs[item];
                      require(glyph.has_value(), "not a glyph");
                      require(std::all_of(glyph->parts.begin(), glyph->parts.end(),
                                          [&is_image](const ua::GlyphPart &part)
                                          {
                                              return is_image(part.glyph);
                                          }),
                              "a part is not an image of the library");
                  });
    }
}

// A .TLB file is a TILE library, or a library of them behind an index.
void check_tiles(RowBuilder &row, const GameFile &file)
{
    const auto library = open_library(row, file);
    if (!library)
    {
        return;
    }
    if (library->signature != "HLIB")
    {
        check_tile_library(row, file.name, *library);
        return;
    }
    for (std::size_t item = 0; item < library->items.size(); ++item)
    {
        if (item == 0 && library->indexed)
        {
            check_index(row, file.name + " index", *library);
            continue;
        }
        const auto inner = ua::decode_library(library->items[item]);
        if (!inner)
        {
            row.check(item_name(file, item),
                      []
                      {
                          throw std::runtime_error("not a resource library");
                      });
            continue;
        }
        check_tile_library(row, item_name(file, item), *inner);
    }
}

using ItemDecoder = std::function<bool(std::span<const std::uint8_t>)>;

// Each item of a DATA or DIG4 library through one decoder; an index is a piece.
void check_records(RowBuilder &row, const GameFile &file, std::string_view signature,
                   const ItemDecoder &decode)
{
    const auto library = open_library(row, file);
    if (!library)
    {
        return;
    }
    if (library->signature != signature)
    {
        row.check(file.name,
                  []
                  {
                      throw std::runtime_error("a library of another kind");
                  });
        return;
    }
    for (std::size_t item = 0; item < library->items.size(); ++item)
    {
        if (item == 0 && library->indexed)
        {
            check_index(row, file.name + " index", *library);
            continue;
        }
        row.check(item_name(file, item),
                  [&]
                  {
                      require(decode(library->items[item]), "not a record of its kind");
                  });
    }
}

// A file decoded whole is one piece.
void check_file(RowBuilder &row, const GameFile &file, const ItemDecoder &decode)
{
    row.add_file(file.name);
    row.check(file.name,
              [&]
              {
                  require(decode(read_bytes(file)), "does not decode");
              });
}

// Each marker's sound is a piece.
void check_voice_file(RowBuilder &row, const GameFile &file)
{
    row.add_file(file.name);
    std::optional<std::vector<ua::VoiceSection>> sections;
    try
    {
        sections = ua::decode_voice_file(read_bytes(file));
    }
    catch (const std::exception &error)
    {
        row.fail(file.name, error.what());
    }
    if (!sections)
    {
        row.check(file.name,
                  []
                  {
                      throw std::runtime_error("not a Creative Voice file");
                  });
        return;
    }
    for (std::size_t section = 0; section < sections->size(); ++section)
    {
        row.check(file.name + " sound " + std::to_string(section),
                  [&]
                  {
                      const auto &sound = (*sections)[section];
                      require(sound.marker.has_value(), "sound before the first marker");
                      require(!sound.samples.empty() && sound.rate > 0, "no sound");
                  });
    }
}

// Each sequence is a piece.
void check_music(RowBuilder &row, const GameFile &file)
{
    row.add_file(file.name);
    std::optional<std::vector<ua::XmiSequence>> sequences;
    try
    {
        sequences = ua::decode_xmi(read_bytes(file));
    }
    catch (const std::exception &error)
    {
        row.fail(file.name, error.what());
    }
    if (!sequences)
    {
        row.check(file.name,
                  []
                  {
                      throw std::runtime_error("not XMI music");
                  });
        return;
    }
    for (std::size_t sequence = 0; sequence < sequences->size(); ++sequence)
    {
        row.check(file.name + " sequence " + std::to_string(sequence),
                  [&]
                  {
                      require(!(*sequences)[sequence].events.empty(), "no events");
                  });
    }
}

template <typename Decoded>
ItemDecoder decodes(std::optional<Decoded> (*decoder)(std::span<const std::uint8_t>))
{
    return [decoder](std::span<const std::uint8_t> bytes)
    {
        return decoder(bytes).has_value();
    };
}

// The kinds of .TLB file, by name.
const std::map<std::string, std::string, std::less<>> &tile_kinds()
{
    static const std::map<std::string, std::string, std::less<>> kinds{
        {"TITLE", "pictures"},       {"BIGPIC", "pictures"},     {"BIGPIX", "pictures"},
        {"PICA", "pictures"},        {"PICB", "pictures"},       {"PICC", "pictures"},
        {"PICD", "pictures"},        {"PICE", "pictures"},       {"PICF", "pictures"},
        {"BACK", "pictures"},        {"SPRIT", "sprites"},       {"CPIC", "combat icons"},
        {"COMSPR", "combat icons"},  {"CBODY", "combat icons"},  {"DUNGCOM", "combat icons"},
        {"WILDCOM", "combat icons"}, {"8X8DB", "wall tiles"},    {"8X8DC", "wall tiles"},
        {"ALWAYS", "screen parts"},  {"FRAME", "screen parts"},  {"GEN", "screen parts"},
        {"MENU", "screen parts"},    {"TOPVIEW", "screen parts"}};
    return kinds;
}

using FileCheck = std::function<void(RowBuilder &, const GameFile &)>;

struct Kind
{
    std::string row;
    FileCheck check;
};

FileCheck records(std::string_view signature, ItemDecoder decode)
{
    return [signature, decode = std::move(decode)](RowBuilder &row, const GameFile &file)
    {
        check_records(row, file, signature, decode);
    };
}

FileCheck whole(ItemDecoder decode)
{
    return [decode = std::move(decode)](RowBuilder &row, const GameFile &file)
    {
        check_file(row, file, decode);
    };
}

bool decodes_item_types(std::span<const std::uint8_t> bytes)
{
    return por::decode_item_templates(bytes, GoldBoxGame::unlimited_adventures).has_value();
}

// The files with a kind of their own, by name.
const std::map<std::string, Kind, std::less<>> &data_kinds()
{
    static const std::map<std::string, Kind, std::less<>> kinds{
        {"MONST.GLB", {"MONST.GLB: monsters", records("DATA", decodes(&ua::decode_monster))}},
        {"GEO.GLB", {"GEO.GLB: areas", records("DATA", decodes(&ua::decode_area))}},
        {"STRG.GLB", {"STRG.GLB: labels", records("DATA", decodes(&ua::decode_labels))}},
        {"GAME.GLB",
         {"GAME.GLB: game settings", records("DATA", decodes(&ua::decode_game_settings))}},
        {"SCRIPT.GLB",
         {"SCRIPT.GLB: the event editor's forms",
          records("DATA", decodes(&ua::decode_editor_form))}},
        {"SOUNDS.GLB",
         {"SOUNDS.GLB: speaker sounds", records("DIG4", decodes(&ua::decode_speaker_sound))}},
        {"GAME.FON", {"FON: the font", whole(decodes(&ua::decode_font))}},
        {"ITEMS.DAT", {"ITEMS.DAT: item types", whole(decodes_item_types)}},
        {"ITEM.DAT", {"ITEM.DAT: items", whole(decodes(&ua::decode_items))}},
    };
    return kinds;
}

// The row a file belongs to and how its pieces decode; nullopt for a file no
// decoder reads.
std::optional<Kind> kind_of(const GameFile &file)
{
    const auto name = base_name(file.name);
    const auto ext = extension(name);
    if (ext == "TLB")
    {
        const auto kind = tile_kinds().find(stem(name));
        if (kind == tile_kinds().end())
        {
            return std::nullopt;
        }
        return Kind{"TLB: " + kind->second, check_tiles};
    }
    if (const auto data = data_kinds().find(name); data != data_kinds().end())
    {
        return data->second;
    }
    if (ext == "VOC")
    {
        return Kind{"VOC: sound effects", check_voice_file};
    }
    if (ext == "XMI")
    {
        return Kind{"XMI: music", check_music};
    }
    if (ext == "LBM")
    {
        return Kind{"LBM: art templates", whole(decodes(&ua::decode_ilbm))};
    }
    if (ext == "PCX")
    {
        return Kind{"PCX: art templates", whole(decodes(&ua::decode_pcx))};
    }
    return std::nullopt;
}

} // namespace

GameCensus take_ua_census(const std::filesystem::path &folder)
{
    GameCensus census;
    census.game = GoldBoxGame::unlimited_adventures;
    std::vector<GameFile> files;
    std::vector<std::string> directories;
    add_files(folder, "", files, directories);
    std::sort(files.begin(), files.end(),
              [](const GameFile &a, const GameFile &b)
              {
                  return a.name < b.name;
              });
    std::map<std::string, RowBuilder> rows;
    for (const auto &file : files)
    {
        if (is_never_read(file.name))
        {
            census.program_files.push_back(file.name);
            continue;
        }
        auto kind = kind_of(file);
        if (!kind)
        {
            census.unread_files.push_back(file.name);
            continue;
        }
        auto row = rows.try_emplace(kind->row, kind->row).first;
        kind->check(row->second, file);
    }
    for (auto &[name, row] : rows)
    {
        census.rows.push_back(std::move(row).take());
    }
    std::sort(directories.begin(), directories.end());
    census.unread_files.insert(census.unread_files.end(), directories.begin(), directories.end());
    return census;
}

} // namespace opengold
