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
                     std::vector<std::string> item_words, GoldBoxGame game)
    : directory_(std::move(directory)), templates_(std::move(templates)),
      item_words_(std::move(item_words)), game_(game)
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

std::string AreaFiles::disk_archive(std::string_view family, unsigned disk) const
{
    const bool one_for_every_disk = game_ == GoldBoxGame::secret_of_the_silver_blades &&
                                    (family == "SPRIT" || family == "CPIC");
    return std::string(family) + std::to_string(one_for_every_disk ? 1 : disk) + ".DAX";
}

bool AreaFiles::has_archive(const std::string &name) const
{
    try
    {
        static_cast<void>(resolve_archive(directory_, name));
        return true;
    }
    catch (const EclError &)
    {
        return false;
    }
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
        // The later games' records carry no name text (decode_items).
        const auto items = decode_items(record.bytes, game_);
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

constexpr std::size_t wall_block_size = 780, slot_tiles = 70, common_tiles = 45;

// A LOAD PIECES bank of several blocks fills that many slots, block n's tiles
// in record id * 10 + n (the original's LoadWallSet): Valhingen Graveyard's
// LOAD PIECES 21,22,20, whose script sets the cells that keep the original
// from loading slot 2's own bank.
std::size_t bank_blocks(const AreaFiles &files, const std::string &disk, unsigned id)
{
    const auto blocks = files.record("WALLDEF" + disk + ".DAX", id).size() / wall_block_size;
    if (blocks == 0)
    {
        throw EclError("Invalid wall definition bank " + std::to_string(id));
    }
    return blocks;
}

WallTiles slot_tile_bank(const AreaFiles &files, const std::string &disk, unsigned id,
                         std::size_t blocks, std::size_t block)
{
    const auto tile_record = blocks == 1 ? id : static_cast<unsigned>(id * 10 + block + 1);
    auto tiles = decode_wall_tiles(files.record("8X8D" + disk + ".DAX", tile_record));
    if (!tiles || tiles->size() != slot_tiles)
    {
        throw EclError("Invalid wall tile bank " + std::to_string(tile_record));
    }
    return std::move(*tiles);
}

// A bank's walls in the view's space of symbols: the original adds 70 for
// each slot past the first to every symbol from the shared set's last (45)
// up. One past the space is drawn blank.
WallArtSet wall_bank(const AreaFiles &files, const std::string &disk, unsigned id, std::size_t slot,
                     const WallTiles &symbols, const WallArtReplacements &new_wall_art)
{
    auto definition = files.record("WALLDEF" + disk + ".DAX", id);
    const auto blocks = definition.size() / wall_block_size;
    for (auto &symbol : definition)
    {
        if (symbol >= common_tiles)
        {
            const auto shifted = symbol + slot_tiles * slot;
            symbol = shifted < symbols.size() ? static_cast<std::uint8_t>(shifted) : 0;
        }
    }
    auto decoded = decode_wall_art(definition, symbols);
    if (!decoded || decoded->appearances.size() != 5 * blocks)
    {
        throw EclError("Invalid wall definition bank " + std::to_string(id));
    }
    return with_replacements(std::move(*decoded),
                             wall_appearance_keys("WALLDEF" + disk + ".DAX", id, 5 * blocks),
                             new_wall_art);
}

} // namespace

WallArtSet load_wall_art(const AreaFiles &files, unsigned disk,
                         const std::array<unsigned, 3> &pieces,
                         const WallArtReplacements &new_wall_art)
{
    const auto bank = std::to_string(disk);
    // The view draws every slot's walls from one space of symbols: blank, the
    // shared 45, then each slot's 70. A slot left empty is blank, and a wall
    // may draw on the slots beside its own (docs/games/secret.md).
    WallTiles symbols(1 + common_tiles + slot_tiles * pieces.size());
    auto common = decode_wall_tiles(files.record("8X8D1.DAX", 203));
    if (!common || common->size() != common_tiles)
    {
        throw EclError("Invalid common wall tiles");
    }
    std::copy(common->begin(), common->end(), symbols.begin() + 1);
    std::vector<std::pair<unsigned, std::size_t>> loaded; // Each bank and its first slot.
    for (std::size_t slot = 0; slot < pieces.size();)
    {
        const auto id = pieces[slot];
        if (id == no_wall_bank)
        {
            ++slot;
            continue;
        }
        const auto blocks = bank_blocks(files, bank, id);
        if (slot + blocks > pieces.size())
        {
            throw EclError("Invalid wall definition bank " + std::to_string(id));
        }
        for (std::size_t block = 0; block < blocks; ++block)
        {
            const auto tiles = slot_tile_bank(files, bank, id, blocks, block);
            std::copy(tiles.begin(), tiles.end(),
                      symbols.begin() + static_cast<std::ptrdiff_t>(1 + common_tiles +
                                                                    slot_tiles * (slot + block)));
        }
        loaded.emplace_back(id, slot);
        slot += blocks;
    }
    WallArtSet walls;
    // Three five-appearance slots, each from its LOAD PIECES bank.
    std::size_t next_slot = 0;
    for (const auto &[id, slot] : loaded)
    {
        for (; next_slot < slot; ++next_slot)
        {
            walls.appearances.resize(walls.appearances.size() + 5);
            walls.originals.resize(walls.appearances.size());
        }
        auto with_new_art = wall_bank(files, bank, id, slot, symbols, new_wall_art);
        walls.scale = with_new_art.scale;
        walls.appearances.insert(walls.appearances.end(), with_new_art.appearances.begin(),
                                 with_new_art.appearances.end());
        with_new_art.originals.resize(with_new_art.appearances.size());
        walls.originals.insert(walls.originals.end(), with_new_art.originals.begin(),
                               with_new_art.originals.end());
        next_slot = slot + with_new_art.appearances.size() / 5;
    }
    for (; next_slot < pieces.size(); ++next_slot)
    {
        walls.appearances.resize(walls.appearances.size() + 5);
        walls.originals.resize(walls.appearances.size());
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
        read_archive(resolve_archive(files.directory(), files.disk_archive("SPRIT", area.bank)));
    district->combat_archive =
        read_archive(resolve_archive(files.directory(), files.disk_archive("CPIC", area.bank)));
    district->terrain_art = terrain_art;
    // Secret of the Silver Blades has no portraits for its scripts.
    for (auto [family, destination] :
         {std::pair{"HEAD", &district->heads}, std::pair{"BODY", &district->bodies}})
    {
        if (const auto name = family + bank + ".DAX"; files.has_archive(name))
        {
            files.pictures(name, *destination);
        }
    }
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
