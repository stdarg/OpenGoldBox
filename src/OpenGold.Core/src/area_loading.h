#ifndef OPENGOLD_AREA_LOADING_H
#define OPENGOLD_AREA_LOADING_H

// What every Gold Box campaign reads for one map of an area: the map, its
// walls, sprites, pictures, treasure lists and combat art. A campaign lists
// its areas; Pool of Radiance's are in rolf_tour.cpp.

#include "opengold/formats.h"
#include "opengold/gold_box_game.h"
#include "opengold/map_catalog.h"
#include "opengold/rolf_tour.h"
#include "opengold/wall_art_replacements.h"

#include <array>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace opengold::por::detail
{

// A game folder's file, whatever the case of its name.
[[nodiscard]] std::filesystem::path resolve_archive(const std::filesystem::path &directory,
                                                    std::string_view name);
[[nodiscard]] std::vector<std::uint8_t> read_archive(const std::filesystem::path &path);

// LOAD PIECES's bank 255 loads nothing: its five wall appearances stay blank.
inline constexpr unsigned no_wall_bank = 255;

// The banks a LOAD PIECES loads: Secret of the Silver Blades' original skips
// a second or third slot of 127 as of 255 (its EclLoadFiles).
[[nodiscard]] inline std::array<unsigned, 3> load_pieces_banks(std::array<unsigned, 3> operands)
{
    constexpr unsigned none_later = 127;
    for (std::size_t slot = 1; slot < operands.size(); ++slot)
    {
        if (operands[slot] == none_later)
        {
            operands[slot] = no_wall_bank;
        }
    }
    return operands;
}

// A map an area's script loads, with its three wall banks (LOAD PIECES).
struct DistrictMap
{
    unsigned id;
    std::array<unsigned, 3> pieces;
    // Other scripts that load this map, whose creatures it also holds.
    std::vector<unsigned> shared_with{};
};

// An area: its script, its archive bank (ECLn, GEOn, WALLDEFn and the rest)
// and the maps its script loads.
struct AreaSource
{
    unsigned script, bank;
    std::vector<DistrictMap> maps;
};

// The game folder's files an area is read from, each archive decoded once.
class AreaFiles
{
  public:
    // The game's item lists are read its way: after Pool of Radiance, the
    // original names an item only by its words, and the records' name bytes
    // are scratch (decode_items).
    AreaFiles(std::filesystem::path directory, std::vector<ItemTemplate> templates,
              std::vector<std::string> item_words,
              GoldBoxGame game = GoldBoxGame::pool_of_radiance);

    [[nodiscard]] const std::filesystem::path &directory() const noexcept
    {
        return directory_;
    }

    [[nodiscard]] GoldBoxGame game() const noexcept
    {
        return game_;
    }

    [[nodiscard]] const DaxDecodeResult &archive(const std::string &name) const;
    // A family's file for a disk ("SPRIT", 2 is SPRIT2.DAX): Secret of the
    // Silver Blades keeps one sprite and one combat icon file, numbered 1, for
    // every disk.
    [[nodiscard]] std::string disk_archive(std::string_view family, unsigned disk) const;
    [[nodiscard]] bool has_archive(const std::string &name) const;
    // A family's files for a disk, in the order the original looks for a
    // record: Gateway to the Savage Frontier first looks in the file without a
    // disk number (8X8D.DAX), when there is one, then in the disk's (its LoadDax).
    [[nodiscard]] std::vector<std::string> disk_archives(std::string_view family,
                                                         unsigned disk) const;
    // The first of disk_archives that exists and holds the record; throws
    // EclError when none does.
    [[nodiscard]] std::string disk_record_archive(std::string_view family, unsigned disk,
                                                  unsigned id) const;
    // Throws EclError naming the archive when the record is missing.
    [[nodiscard]] const std::vector<std::uint8_t> &record(const std::string &archive_name,
                                                          unsigned id) const;
    // An archive's fixed item lists (shop stock in town, script treasure in areas).
    [[nodiscard]] std::map<unsigned, std::vector<Equipment>>
    item_lists(const std::string &name) const;
    // A record of item definitions numbered from 1 (Pools of Darkness's
    // ITEM0), without the empty ones (type FFh).
    [[nodiscard]] std::map<unsigned, Equipment> item_definitions(const std::string &name,
                                                                 unsigned record) const;
    // The archive's pictures (not its animations), by record, in the colors
    // the game shows them: Gateway to the Savage Frontier's in its VGA mode's.
    // A record already in destination is kept.
    void pictures(const std::string &name, std::map<unsigned, Image> &destination) const;
    // The archive's animations, by record, as pictures does.
    void animations(const std::string &name,
                    std::map<unsigned, std::vector<AnimationFrame>> &destination) const;

  private:
    std::filesystem::path directory_;
    std::vector<ItemTemplate> templates_;
    std::vector<std::string> item_words_;
    GoldBoxGame game_{GoldBoxGame::pool_of_radiance};
    mutable std::map<std::string, DaxDecodeResult> archives_;
};

// The walls LOAD PIECES loads into the view's three slots from the disk's
// WALLDEF and 8X8D files: bank 255 leaves a slot blank, and a bank of several
// blocks fills that many slots. A slot's tiles number 70, or 210 for Gateway
// to the Savage Frontier's large sets.
[[nodiscard]] WallArtSet load_wall_art(const AreaFiles &files, unsigned disk,
                                       const std::array<unsigned, 3> &pieces,
                                       const WallArtReplacements &new_wall_art);

// Pools of Darkness's walls: one set at a time, WALLDEF1 record `set` drawn
// from the tiles of 8X8D1 record `set` in the colors they set; the maps name
// its appearances from 1 (docs/games/darkness.md, Walls).
[[nodiscard]] WallArtSet load_vga_wall_art(const AreaFiles &files, unsigned set);

// One map of an area, without its creatures: the campaign adds those.
[[nodiscard]] std::shared_ptr<PhlanResources>
load_district(const AreaFiles &files, const MapCatalog &maps, const AreaSource &area,
              const DistrictMap &geo, const WallArtReplacements &new_wall_art,
              const std::vector<Image> &terrain_art);

} // namespace opengold::por::detail

#endif
