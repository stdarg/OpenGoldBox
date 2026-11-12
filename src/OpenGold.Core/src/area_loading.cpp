#include "area_loading.h"

#include "archive_io.h"
#include "opengold/creature_catalog.h"
#include "opengold/magic_item_catalog.h"
#include "opengold/path_text.h"

#include <utility>

namespace opengold::por::detail
{

std::filesystem::path resolve_archive(const std::filesystem::path &directory, std::string_view name)
{
    return find_archive<EclError>(directory, name, "Ambiguous archive " + std::string(name),
                                  "Missing " + std::string(name));
}

std::vector<std::uint8_t> read_archive(const std::filesystem::path &path)
{
    return read_archive_bytes<EclError>(path, "Archive exceeds size limit",
                                        "Cannot open " + path_text(path),
                                        "Incomplete archive read");
}

AreaFiles::AreaFiles(std::filesystem::path directory, std::vector<ItemTemplate> templates,
                     std::vector<std::string> item_words)
    : directory_(std::move(directory)), templates_(std::move(templates)),
      item_words_(std::move(item_words))
{
}

const DaxDecodeResult &AreaFiles::archive(const std::string &name) const
{
    if (const auto found = archives_.find(name); found != archives_.end())
    {
        return found->second;
    }
    auto records = decode_dax_archive(read_archive(resolve_archive(directory_, name)));
    if (!records)
    {
        throw EclError("Invalid wall archive " + name);
    }
    return archives_.emplace(name, std::move(records)).first->second;
}

const std::vector<std::uint8_t> &AreaFiles::record(const std::string &archive_name,
                                                   unsigned id) const
{
    for (const auto &record : archive(archive_name).records)
    {
        if (record.id == id)
        {
            return record.bytes;
        }
    }
    throw EclError("Missing required Phlan wall/tile record " + std::to_string(id));
}

std::map<unsigned, std::vector<Equipment>> AreaFiles::item_lists(const std::string &name) const
{
    std::map<unsigned, std::vector<Equipment>> lists;
    for (const auto &record : archive(name).records)
    {
        const auto items = decode_items(record.bytes);
        if (!items)
        {
            throw EclError("Invalid treasure record in " + name);
        }
        auto &stock = lists[record.id];
        for (const auto &item : *items)
        {
            if (item.type >= templates_.size())
            {
                throw EclError("Invalid item type in " + name);
            }
            Equipment equipment;
            equipment.index = stock.size();
            equipment.stored = item;
            equipment.base = templates_[item.type];
            equipment.bonuses = equipment_bonuses(item, equipment.base);
            // Fixed lists name their magic items as the original does ("Long
            // Sword +2 Flame Tongue"); without START.EXE's words they keep the
            // generic label and bonus.
            if (is_magic(item) && !item_words_.empty())
            {
                equipment.decoded_name = item_name(item, item_words_);
                equipment.unidentified_name = item_name(item, item_words_, item.hidden_name_words);
            }
            stock.push_back(std::move(equipment));
        }
    }
    return lists;
}

void AreaFiles::pictures(const std::string &name, std::map<unsigned, Image> &destination) const
{
    for (const auto &record : archive(name).records)
    {
        auto image = decode_ega_picture(record.bytes);
        if (image)
        {
            destination.emplace(record.id, std::move(image.image));
        }
    }
}

namespace
{

// A LOAD PIECES bank's walls: the common tiles, then its own, decoded before
// they are put together. A bank of several blocks fills that many slots, its
// tiles those of each block in turn, block n's in record id * 10 + n (coab
// LoadWalldef): Valhingen Graveyard's LOAD PIECES 21,22,20, whose script sets
// the cells that keep the original from loading slot 2's own bank.
WallArtSet wall_bank(const AreaFiles &files, const std::string &bank, unsigned id,
                     const WallArtReplacements &new_wall_art, std::size_t &blocks)
{
    const auto &definition = files.record("WALLDEF" + bank + ".DAX", id);
    constexpr std::size_t block_size = 780;
    blocks = definition.size() / block_size;
    if (blocks == 0)
    {
        throw EclError("Invalid wall definition bank " + std::to_string(id));
    }
    WallTiles walls(1);
    auto common = decode_wall_tiles(files.record("8X8D1.DAX", 203));
    if (!common || common->size() != 45)
    {
        throw EclError("Invalid common wall tiles");
    }
    walls.insert(walls.end(), common->begin(), common->end());
    for (std::size_t block = 0; block < blocks; ++block)
    {
        const auto tile_record = blocks == 1 ? id : static_cast<unsigned>(id * 10 + block + 1);
        auto local_bank = decode_wall_tiles(files.record("8X8D" + bank + ".DAX", tile_record));
        if (!local_bank || local_bank->size() != 70)
        {
            throw EclError("Invalid wall tile bank " + std::to_string(tile_record));
        }
        walls.insert(walls.end(), local_bank->begin(), local_bank->end());
    }
    auto decoded = decode_wall_art(definition, walls);
    if (!decoded || decoded->appearances.size() != 5 * blocks)
    {
        throw EclError("Invalid wall definition bank " + std::to_string(id));
    }
    return with_replacements(std::move(*decoded),
                             wall_appearance_keys("WALLDEF" + bank + ".DAX", id, 5 * blocks),
                             new_wall_art);
}

} // namespace

WallArtSet load_wall_art(const AreaFiles &files, unsigned disk,
                         const std::array<unsigned, 3> &pieces,
                         const WallArtReplacements &new_wall_art)
{
    const auto bank = std::to_string(disk);
    WallArtSet walls;
    // Three five-appearance slots, each from its LOAD PIECES bank.
    for (std::size_t slot = 0; slot < pieces.size();)
    {
        const auto id = pieces[slot];
        if (id == no_wall_bank)
        {
            walls.appearances.resize(walls.appearances.size() + 5);
            walls.originals.resize(walls.appearances.size());
            ++slot;
            continue;
        }
        std::size_t blocks{};
        auto with_new_art = wall_bank(files, bank, id, new_wall_art, blocks);
        if (slot + blocks > pieces.size())
        {
            throw EclError("Invalid wall definition bank " + std::to_string(id));
        }
        walls.scale = with_new_art.scale;
        walls.appearances.insert(walls.appearances.end(), with_new_art.appearances.begin(),
                                 with_new_art.appearances.end());
        with_new_art.originals.resize(with_new_art.appearances.size());
        walls.originals.insert(walls.originals.end(), with_new_art.originals.begin(),
                               with_new_art.originals.end());
        slot += blocks;
    }
    return walls;
}

std::shared_ptr<PhlanResources> load_district(const AreaFiles &files, const MapCatalog &maps,
                                              const AreaSource &area, const DistrictMap &geo,
                                              const WallArtReplacements &new_wall_art,
                                              const std::vector<Image> &terrain_art)
{
    const auto bank = std::to_string(area.bank);
    auto district = std::make_shared<PhlanResources>();
    const auto area_map = maps.find({"GEO" + bank + ".DAX", static_cast<std::uint8_t>(geo.id)});
    if (!area_map)
    {
        throw EclError("Missing area map GEO" + bank + ":" + std::to_string(geo.id));
    }
    district->map = area_map->get();
    district->script = area.script;
    district->shared_with = geo.shared_with;
    district->bank = area.bank;
    district->pieces = geo.pieces;
    district->treasure = files.item_lists("ITEM" + bank + ".DAX");
    district->wall_art = load_wall_art(files, area.bank, geo.pieces, new_wall_art);
    district->sprite_archive =
        read_archive(resolve_archive(files.directory(), "SPRIT" + bank + ".DAX"));
    district->combat_archive =
        read_archive(resolve_archive(files.directory(), "CPIC" + bank + ".DAX"));
    district->terrain_art = terrain_art;
    files.pictures("HEAD" + bank + ".DAX", district->heads);
    files.pictures("BODY" + bank + ".DAX", district->bodies);
    files.pictures("PIC" + bank + ".DAX", district->pictures);
    for (const auto &record : files.archive("PIC" + bank + ".DAX").records)
    {
        if (auto animation = decode_ega_animation(record.bytes))
        {
            district->animations.emplace(record.id, std::move(animation.frames));
        }
    }
    return district;
}

} // namespace opengold::por::detail
