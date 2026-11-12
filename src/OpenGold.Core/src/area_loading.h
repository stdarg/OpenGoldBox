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
    AreaFiles(std::filesystem::path directory, std::vector<ItemTemplate> templates,
              std::vector<std::string> item_words);

    [[nodiscard]] const std::filesystem::path &directory() const noexcept
    {
        return directory_;
    }

    [[nodiscard]] const DaxDecodeResult &archive(const std::string &name) const;
    // Throws EclError naming the archive when the record is missing.
    [[nodiscard]] const std::vector<std::uint8_t> &record(const std::string &archive_name,
                                                          unsigned id) const;
    // An archive's fixed item lists (shop stock in town, script treasure in areas).
    [[nodiscard]] std::map<unsigned, std::vector<Equipment>>
    item_lists(const std::string &name) const;
    // The archive's pictures (not its animations), by record.
    void pictures(const std::string &name, std::map<unsigned, Image> &destination) const;

  private:
    std::filesystem::path directory_;
    std::vector<ItemTemplate> templates_;
    std::vector<std::string> item_words_;
    mutable std::map<std::string, DaxDecodeResult> archives_;
};

// One map of an area, without its creatures: the campaign adds those.
[[nodiscard]] std::shared_ptr<PhlanResources>
load_district(const AreaFiles &files, const MapCatalog &maps, const AreaSource &area,
              const DistrictMap &geo, const WallArtReplacements &new_wall_art,
              const std::vector<Image> &terrain_art);

} // namespace opengold::por::detail

#endif
