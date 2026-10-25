#include "archive_io.h"
#include "opengold/authored_items.h"
#include "opengold/magic_item_catalog.h"
#include "opengold/por_sound.h"
#include "opengold/rolf_tour.h"
#include <utility>
#include "opengold/exploration_view.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <optional>

namespace opengold::por
{
namespace
{
std::filesystem::path resolve_archive(const std::filesystem::path &directory, std::string_view name)
{
    return detail::find_archive<EclError>(directory, name, "Ambiguous archive " + std::string(name),
                                          "Missing " + std::string(name));
}

std::vector<std::uint8_t> read_archive(const std::filesystem::path &path)
{
    return detail::read_archive_bytes<EclError>(path, "Archive exceeds size limit",
                                                "Cannot open " + path.string(),
                                                "Incomplete archive read");
}

} // namespace

RolfTourSession RolfTourSession::load(const std::filesystem::path &directory,
                                      const WallArtReplacements &new_wall_art,
                                      std::shared_ptr<const MonsterBundle> monsters)
{
    const auto catalog = EclCatalog::load(directory);
    const auto program = catalog.find({"ECL3.DAX", 0});
    if (!program)
    {
        throw EclError("Rolf tour requires ECL3.DAX record 0");
    }
    // Guard this explicit entry profile. No pattern search through embedded data,
    // no modification of original bytes, and no assumption that an event ID is PC.
    const auto first = program->instruction(0xB071);
    const auto intro = program->instruction(0xB0B9);
    const auto setup = program->instruction(0xB0AD);
    if (first.opcode != 9 || first.operands[0].value != 1 || first.operands[1].value != 0x4AC5 ||
        intro.opcode != 18 || intro.operands[0].tag != 128 || setup.opcode != 12 ||
        setup.operands[0].value != 12)
    {
        throw EclError("This ECL3:0 does not match the supported Rolf entry profile");
    }
    // Explicit, verified Phlan profile. This does not implement general 127
    // operand semantics or assume that every map shares its GEO/art record ID.
    // Checked before the maps are loaded: it needs only the program
    // (Effective C++ Item 26).
    const auto pieces = program->instruction(0x9B11);
    if (pieces.opcode != 55 || pieces.operands.size() != 3 ||
        std::any_of(pieces.operands.begin(), pieces.operands.end(),
                    [](const auto &a)
                    {
                        return a.tag != 0 || a.value != 127;
                    }))
    {
        throw EclError("Unsupported Phlan wall resource profile");
    }
    const auto maps = MapCatalog::load(directory);
    const auto map = maps.find({"GEO3.DAX", 0});
    if (!map)
    {
        throw EclError("Rolf tour requires GEO3.DAX record 0");
    }
    const auto archive = [&](const char *name)
    {
        auto records = decode_dax_archive(read_archive(resolve_archive(directory, name)));
        if (!records)
        {
            throw EclError(std::string("Invalid wall archive ") + name);
        }
        return records;
    };
    const auto find_record = [](const DaxDecodeResult &records,
                                unsigned id) -> const std::vector<std::uint8_t> &
    {
        for (const auto &record : records.records)
        {
            if (record.id == id)
            {
                return record.bytes;
            }
        }
        throw EclError("Missing required Phlan wall/tile record " + std::to_string(id));
    };
    const auto definitions = archive("WALLDEF3.DAX");
    const auto shared = archive("8X8D1.DAX");
    const auto local = archive("8X8D3.DAX");
    WallTiles tiles(1); // Blank tile slot zero.
    const auto append = [&](const std::vector<std::uint8_t> &record, unsigned expected_count)
    {
        auto decoded = decode_wall_tiles(record);
        if (!decoded || decoded->size() != expected_count)
        {
            throw EclError("Unexpected Phlan wall tile layout");
        }
        tiles.insert(tiles.end(), decoded->begin(), decoded->end());
    };
    append(find_record(shared, 203), 45);
    for (unsigned id : {101, 102, 103})
    {
        append(find_record(local, id), 70);
    }
    auto wall_art = decode_wall_art(find_record(definitions, 0), tiles);
    if (!wall_art || wall_art->appearances.size() != 15)
    {
        throw EclError("Invalid Phlan wall definitions");
    }
    wall_art = with_replacements(std::move(*wall_art), wall_appearance_keys("WALLDEF3.DAX", 0, 15),
                                 new_wall_art);
    auto bytes = read_archive(resolve_archive(directory, "SPRIT3.DAX"));
    std::array<opengold::Image, 3> sprites;
    for (std::uint8_t i = 0; i < sprites.size(); ++i)
    {
        auto result = decode_ega_sprite(bytes, 12, i);
        if (!result)
        {
            throw EclError("Cannot decode Rolf encounter distance image " + std::to_string(i));
        }
        sprites[i] = std::move(result.image);
    }
    auto town = std::make_shared<PhlanResources>();
    for (unsigned id : {0, 8, 11})
    {
        auto p = catalog.find({"ECL3.DAX", static_cast<std::uint8_t>(id)});
        if (!p)
        {
            throw EclError("Missing New Phlan building script");
        }
        town->programs.emplace(id, std::move(p));
    }
    const auto templates = decode_item_templates(read_archive(resolve_archive(directory, "ITEMS")));
    if (!templates)
    {
        throw EclError("Invalid item templates");
    }
    // Fixed lists name their magic items as the original does ("Long Sword +2
    // Flame Tongue"). START.EXE holds the words; without it they keep the
    // generic label and bonus.
    std::vector<std::string> item_words;
    std::optional<OverlandWorld> overland;
    std::optional<std::filesystem::path> start;
    try
    {
        start = resolve_archive(directory, "START.EXE");
    }
    catch (const EclError &)
    {
    }
    if (start)
    {
        const auto image = unpack_sound_executable(read_archive(*start));
        auto words = decode_item_words(image);
        if (!words)
        {
            throw EclError("START.EXE has no item name table");
        }
        item_words = std::move(*words);
        overland = read_overland_world(image);
    }
    // An archive's fixed item lists (shop stock in town, script treasure in areas).
    const auto item_lists = [&](const std::string &name)
    {
        std::map<unsigned, std::vector<Equipment>> lists;
        for (const auto &record : archive(name.c_str()).records)
        {
            const auto items = decode_items(record.bytes);
            if (!items)
            {
                throw EclError("Invalid treasure record in " + name);
            }
            auto &stock = lists[record.id];
            for (const auto &item : *items)
            {
                if (item.type >= templates->size())
                {
                    throw EclError("Invalid item type in " + name);
                }
                Equipment equipment;
                equipment.index = stock.size();
                equipment.stored = item;
                equipment.base = (*templates)[item.type];
                equipment.bonuses = equipment_bonuses(item, equipment.base);
                if (is_magic(item) && !item_words.empty())
                {
                    equipment.decoded_name = item_name(item, item_words);
                    equipment.unidentified_name =
                        item_name(item, item_words, item.hidden_name_words);
                }
                stock.push_back(std::move(equipment));
            }
        }
        return lists;
    };
    town->treasure = item_lists("ITEM3.DAX");

    town->item_templates = *templates;
    town->monsters = std::move(monsters);
    town->sprite_archive = std::move(bytes);
    const auto pictures = [&](const char *name, auto &destination)
    {
        for (const auto &record : archive(name).records)
        {
            auto image = decode_ega_picture(record.bytes);
            if (image)
            {
                destination.emplace(record.id, std::move(image.image));
            }
        }
    };
    // The wilderness map (docs/audits/central-wilderness.md): START.EXE's grid
    // drawn with SQRPACI's two sets of 128 tiles, the party as ICON 0, and its
    // fights on WILDCOM's ground with RANDCOM's debris.
    if (overland)
    {
        const auto tiles = read_archive(resolve_archive(directory, "SQRPACI.DAX"));
        for (std::uint8_t set = 1; set <= 2; ++set)
        {
            for (unsigned frame = 0; frame < 128; ++frame)
            {
                auto tile = decode_ega_combat_icon(tiles, set, frame);
                if (!tile)
                {
                    throw EclError("Invalid overland tile " + std::to_string(frame));
                }
                town->overland_tiles.push_back(std::move(tile.image));
            }
        }
        auto icon = decode_ega_combat_icon(read_archive(resolve_archive(directory, "ICON.DAX")), 0, 0);
        if (!icon)
        {
            throw EclError("Invalid overland party icon");
        }
        town->party_icon = std::move(icon.image);
        const auto wild = read_archive(resolve_archive(directory, "WILDCOM.DAX")),
                   debris = read_archive(resolve_archive(directory, "RANDCOM.DAX"));
        for (const auto &[set, frames] : {std::pair{&wild, 34u}, std::pair{&debris, 6u}})
        {
            for (unsigned frame = 0; frame < frames; ++frame)
            {
                auto art = decode_ega_combat_icon(*set, 1, frame);
                if (!art || art.image.width != 24 || art.image.height != 24)
                {
                    throw EclError("Invalid wilderness battlefield tile " + std::to_string(frame));
                }
                town->wilderness_art.push_back(std::move(art.image));
            }
        }
        town->overland = std::make_shared<const OverlandWorld>(std::move(*overland));
    }
    pictures("HEAD3.DAX", town->heads);
    pictures("BODY3.DAX", town->bodies);
    pictures("PIC3.DAX", town->pictures);
    // The harbor's boat (PIC3 41) is an animation; its first frame stands for it.
    for (const auto &record : archive("PIC3.DAX").records)
    {
        if (auto animation = decode_ega_animation(record.bytes))
        {
            town->animations.emplace(record.id, std::move(animation.frames));
        }
    }
    town->map = map->get();
    town->wall_art = *wall_art;

    constexpr unsigned no_wall_bank = 255;
    // The districts beyond New Phlan, one row per area: its script, its archive
    // bank (ECLn, GEOn, WALLDEFn, MONnCHA and so on), the maps its script loads
    // with each map's three wall banks (`LOAD PIECES`), and the original
    // creature records its encounters convert.
    struct DistrictMap
    {
        unsigned id;
        std::array<unsigned, 3> pieces;
    };

    struct Conversion
    {
        std::uint8_t record;
        const char *definition;
        unsigned fit_xp, award_xp;
    };

    struct AreaSource
    {
        unsigned script, bank;
        std::vector<DistrictMap> maps;
        std::vector<Conversion> creatures;
    };

    // The original Slums creatures' experience follows docs/EXPEDITION.md; the
    // set-encounter creatures added later and Kuto's Well award each stat
    // block's XP, a leader double (docs/audits/kutos-well.md).
    const std::array areas{
        AreaSource{20,
                   2,
                   {{20, {2, 4, 1}}},
                   {{0, "slums-kobold", 25, 25},
                    {1, "slums-kobold-leader", 25, 50},
                    {11, "slums-kobold-leader-sword", 25, 50},
                    {2, "slums-goblin", 50, 50},
                    {3, "slums-goblin-leader", 50, 100},
                    {12, "slums-goblin-leader", 50, 100},
                    {4, "slums-orc", 100, 75},
                    {13, "slums-orc", 100, 75},
                    {5, "slums-orc-leader", 100, 150},
                    {14, "slums-orc-leader", 100, 150},
                    {15, "slums-orc-leader", 100, 150},
                    {63, "slums-bugbear", 200, 200},
                    {6, "slums-hobgoblin", 100, 100},
                    {7, "slums-hobgoblin", 100, 200},
                    {8, "ogre", 450, 450},
                    {31, "troll", 1800, 1800},
                    {73, "gnoll-warrior", 100, 100},
                    {94, "slums-magic-user", 200, 200}}},
        AreaSource{29,
                   8,
                   {{29, {3, 20, 1}}, {32, {18, 17, 1}}},
                   {{0, "slums-kobold", 25, 25},
                    {1, "slums-kobold-leader", 25, 50},
                    {57, "lizardfolk", 100, 100},
                    {59, "giant-lizard", 50, 50},
                    {73, "gnoll-warrior", 100, 100},
                    {32, "norris-the-gray", 450, 450}}},
        // Podol Plaza (docs/audits/podol-plaza.md): its
        // MON1CHA records 2-8 are the Slums' MON2CHA ones.
        AreaSource{18,
                   1,
                   {{18, {1, 3, 5}}},
                   {{2, "slums-goblin", 50, 50},
                    {3, "slums-goblin-leader", 50, 100},
                    {4, "slums-orc", 100, 75},
                    {5, "slums-orc-leader", 100, 150},
                    {6, "slums-hobgoblin", 100, 100},
                    {7, "slums-hobgoblin", 100, 200},
                    {8, "ogre", 450, 450},
                    {41, "buccaneer", 200, 200}}},
        // Cadorna Textile House
        // (docs/audits/cadorna-textile-house.md, CADORNA-1):
        // its MON4CHA records 6-8 are the Slums' MON2CHA
        // ones; the Large Scorpion and the Wight keep their
        // stat blocks' XP with the original's Hit Points.
        AreaSource{2,
                   4,
                   {{2, {1, 3, 20}}},
                   {{6, "slums-hobgoblin", 100, 100},
                    {7, "slums-hobgoblin", 100, 200},
                    {8, "ogre", 450, 450},
                    {18, "large-scorpion", 700, 700},
                    {20, "wight", 700, 700},
                    {27, "skullcrusher", 100, 100},
                    {28, "grishnak", 450, 450},
                    {34, "skeleton", 50, 50},
                    {35, "zombie", 50, 50},
                    {38, "giant-frog", 50, 50},
                    {39, "giant-scorpion", 700, 700},
                    {72, "ghoul", 200, 200}}},
        // Mendor's Library (docs/audits/mendors-library.md,
        // MENDOR-1): its MON2CHA kobold is the Slums' one.
        AreaSource{15,
                   2,
                   {{15, {1, 19, 23}}},
                   {{0, "slums-kobold", 25, 25},
                    {17, "specter", 200, 200},
                    {25, "mad-man", 25, 25},
                    {26, "basilisk", 700, 700}}},
        // Sokal Keep (docs/audits/sokal-keep.md, SOKAL-1),
        // by boat from New Phlan's docks: its MON4CHA orcs
        // and hobgoblins are the Slums' MON2CHA ones, and
        // Ferran Martinez has the Spectre's record.
        AreaSource{21,
                   4,
                   {{21, {1, 5, 9}}},
                   {{4, "slums-orc", 100, 75},
                    {5, "slums-orc-leader", 100, 150},
                    {6, "slums-hobgoblin", 100, 100},
                    {19, "specter", 200, 200},
                    {34, "skeleton", 50, 50},
                    {35, "zombie", 50, 50},
                    {38, "giant-frog", 50, 50},
                    {39, "giant-scorpion", 700, 700}}},
        // The Central Wilderness (docs/audits/central-wilderness.md,
        // OVERLAND-1): the wilderness map, and the grove and
        // ruined huts on GEO7:26.
        AreaSource{26,
                   7,
                   {{26, {7, 8, 23}}},
                   {{0, "slums-kobold", 25, 25},
                    {1, "slums-kobold-leader", 25, 50},
                    {123, "slums-kobold", 25, 25},
                    {40, "wilderness-bandit", 25, 25},
                    {53, "bandit-captain", 450, 450},
                    {57, "lizardfolk", 100, 100},
                    {58, "mutant-lizardfolk", 100, 100},
                    {61, "stirge", 25, 25},
                    {68, "displacer-beast", 700, 700},
                    {69, "drider", 2300, 2300},
                    {97, "wilderness-bandit", 25, 25},
                    {98, "wilderness-bandit", 25, 25}}},
        // Kovel Mansion (docs/audits/kovel-mansion.md,
        // KOVEL-1), the thieves' guild north of the city.
        AreaSource{14,
                   3,
                   {{14, {1, 3, 5}}},
                   {{45, "kovel-thief", 25, 25}, {51, "kovel-master-thief", 200, 200}}},
        // The Temple of Bane (docs/audits/temple-of-bane.md,
        // BANE-1): the street east of the temple (map 31)
        // and the temple's block (map 24). Its MON1CHA
        // records 2-8 are Podol Plaza's.
        AreaSource{24,
                   1,
                   {{24, {1, 3, 5}}, {31, {1, 3, 5}}},
                   {{2, "slums-goblin", 50, 50},
                    {3, "slums-goblin-leader", 50, 100},
                    {4, "slums-orc", 100, 75},
                    {5, "slums-orc-leader", 100, 150},
                    {6, "slums-hobgoblin", 100, 100},
                    {7, "slums-hobgoblin", 100, 200},
                    {8, "ogre", 450, 450},
                    {9, "ogre", 450, 900},
                    {33, "bane-cleric", 450, 450},
                    {44, "bane-orc-archer", 100, 100}}},
        // The nomad camp (docs/audits/nomad-camp.md, NOMAD-1), north of the
        // city on the wilderness map.
        AreaSource{17,
                   7,
                   {{17, {7, 8, 23}}},
                   {{0, "slums-kobold", 25, 25},
                    {123, "slums-kobold", 25, 25},
                    {40, "wilderness-bandit", 25, 25},
                    {41, "nomad-guard", 450, 450},
                    {42, "hassad", 700, 700},
                    {43, "nomad-shaman", 200, 200}}},
        // The Pyramid (docs/audits/pyramid.md, PYRAMID-1): Yarash's prison of
        // souls (map 22) and his laboratories (map 23).
        AreaSource{
            22,
            7,
            {{22, {1, 3, no_wall_bank}}},
            {{57, "lizardfolk", 100, 100},
             {61, "stirge", 25, 25},
             {62, "minotaur", 700, 700},
             {68, "displacer-beast", 700, 700},
             {69, "drider", 2300, 2300},
             {75, "trapped-fighter", 700, 700},
             {76, "trapped-veteran", 700, 700},
             {77, "kovel-master-thief", 200, 200},
             {78, "trapped-acolyte", 50, 50},
             {79, "nomad-shaman", 200, 200},
             {80, "trapped-veteran", 700, 700},
             {123, "slums-kobold", 25, 25}}},
        AreaSource{
            23,
            7,
            {{23, {1, 3, no_wall_bank}}},
            {{57, "lizardfolk", 100, 100},
             {58, "mutant-lizardfolk", 100, 100},
             {81, "yarash", 2300, 2300}}},
        // The western wilderness (docs/audits/western-wilderness.md, WEST-1):
        // its window of the wilderness map, and the dark cave on GEO6:25.
        AreaSource{
            25,
            6,
            {{25, {17, 17, 17}}},
            {{10, "quickling", 100, 100},
             {45, "kovel-thief", 25, 25},
             {51, "kovel-master-thief", 200, 200},
             {54, "zhent-corporal", 25, 25},
             {60, "venomous-snake", 50, 50},
             {65, "ankheg", 450, 450},
             {67, "centaur", 450, 450},
             {74, "giant-mantis", 700, 700},
             {99, "kovel-thief", 25, 25},
             {100, "bandit-captain", 450, 450},
             {118, "thri-kreen", 700, 700},
             {119, "tiger", 200, 200},
             {120, "wild-boar", 50, 50}}},
        // The eastern wilderness (docs/audits/eastern-wilderness.md, EAST-1):
        // its window of the wilderness map, and the wyverns' cave on GEO8:27.
        AreaSource{
            27,
            8,
            {{27, {17, 17, 17}}},
            {{0, "slums-kobold", 25, 25},
             {1, "slums-kobold-leader", 25, 50},
             {31, "troll", 1800, 1800},
             {57, "lizardfolk", 100, 100},
             {59, "giant-lizard", 50, 50},
             {62, "minotaur", 700, 700},
             {73, "gnoll-warrior", 100, 100},
             {113, "hippogriff", 200, 200},
             {116, "phase-spider", 700, 700},
             {121, "wyvern", 2300, 2300}}},
        // The kobold caves (docs/audits/kobold-caves.md, KOBOLD-1), by the
        // eastern wilderness.
        AreaSource{
            13,
            8,
            {{13, {17, 18, no_wall_bank}}},
            {{0, "slums-kobold", 25, 25},
             {1, "slums-kobold-leader", 25, 50},
             {31, "troll", 1800, 1800},
             {87, "trapped-fighter", 700, 700},
             {104, "trapped-fighter", 700, 700},
             {120, "wild-boar", 50, 50},
             {121, "wyvern", 2300, 2300},
             {124, "efreeti", 2300, 2300}}},
        // The lizard men's keep (docs/audits/lizard-keep.md, LIZARD-1): the
        // ruined castle in the eastern wilderness's quicksand (map 16) and
        // its lower level (map 30).
        AreaSource{
            16,
            8,
            {{16, {9, 20, 23}}, {30, {9, 17, 23}}},
            {{57, "lizardfolk", 100, 100},
             {59, "giant-lizard", 50, 50},
             {95, "mutant-lizardfolk", 100, 100}}}};
    const auto creatures = CreatureCatalog::load(directory);
    const auto dungeon = read_archive(resolve_archive(directory, "DUNGCOM.DAX"));
    std::vector<Image> terrain_art;
    for (unsigned tile = 0; tile < 25; ++tile)
    {
        auto image = decode_ega_combat_icon(dungeon, 1, tile);
        if (!image || image.image.width != 24 || image.image.height != 24)
        {
            throw EclError("Invalid dungeon tactical tile");
        }
        terrain_art.push_back(std::move(image.image));
    }
    for (const auto &area : areas)
    {
        const auto bank = std::to_string(area.bank);
        const auto program =
            catalog.find({"ECL" + bank + ".DAX", static_cast<std::uint8_t>(area.script)});
        if (!program)
        {
            throw EclError("Missing area script ECL" + bank + ":" + std::to_string(area.script));
        }
        town->programs.emplace(area.script, program);
        const auto definitions = archive(("WALLDEF" + bank + ".DAX").c_str()),
                   tiles = archive(("8X8D" + bank + ".DAX").c_str());
        for (const auto &geo : area.maps)
        {
            auto district = std::make_shared<PhlanResources>();
            const auto map = maps.find({"GEO" + bank + ".DAX", static_cast<std::uint8_t>(geo.id)});
            if (!map)
            {
                throw EclError("Missing area map GEO" + bank + ":" + std::to_string(geo.id));
            }
            district->map = map->get();
            district->script = area.script;
            district->bank = area.bank;
            district->pieces = geo.pieces;
            district->treasure = item_lists("ITEM" + bank + ".DAX");
            // Three five-appearance banks, each with the common tiles followed
            // by its own 70 tiles. Decode independently before concatenating.
            for (const auto id : geo.pieces)
            {
                // Bank 255 loads nothing (the Pyramid's LOAD PIECES 1,3,255):
                // its five appearances stay blank.
                if (id == no_wall_bank)
                {
                    district->wall_art.appearances.resize(district->wall_art.appearances.size() +
                                                          5);
                    continue;
                }
                WallTiles walls(1);
                auto common = decode_wall_tiles(find_record(shared, 203));
                auto local_bank = decode_wall_tiles(find_record(tiles, id));
                if (!common || common->size() != 45 || !local_bank || local_bank->size() != 70)
                {
                    throw EclError("Invalid wall tile bank " + std::to_string(id));
                }
                walls.insert(walls.end(), common->begin(), common->end());
                walls.insert(walls.end(), local_bank->begin(), local_bank->end());
                auto decoded = decode_wall_art(find_record(definitions, id), walls);
                if (!decoded || decoded->appearances.size() != 5)
                {
                    throw EclError("Invalid wall definition bank " + std::to_string(id));
                }
                const auto with_new_art = with_replacements(
                    std::move(*decoded), wall_appearance_keys("WALLDEF" + bank + ".DAX", id, 5),
                    new_wall_art);
                district->wall_art.scale = with_new_art.scale;
                district->wall_art.appearances.insert(district->wall_art.appearances.end(),
                                                      with_new_art.appearances.begin(),
                                                      with_new_art.appearances.end());
            }
            district->sprite_archive =
                read_archive(resolve_archive(directory, "SPRIT" + bank + ".DAX"));
            for (const auto &conversion : area.creatures)
            {
                const auto id = conversion.record;
                const auto creature = creatures.find({static_cast<std::uint8_t>(area.bank), id});
                if (!creature)
                {
                    throw EclError("Missing original creature MON" + bank +
                                   "CHA:" + std::to_string(id));
                }
                district->encounter_creatures.emplace(id, creature->get());
                district->conversions.emplace(id, EncounterConversion{conversion.definition,
                                                                      conversion.fit_xp,
                                                                      conversion.award_xp});
            }
            district->combat_archive =
                read_archive(resolve_archive(directory, "CPIC" + bank + ".DAX"));
            district->terrain_art = terrain_art;
            pictures(("HEAD" + bank + ".DAX").c_str(), district->heads);
            pictures(("BODY" + bank + ".DAX").c_str(), district->bodies);
            pictures(("PIC" + bank + ".DAX").c_str(), district->pictures);
            for (const auto &record : archive(("PIC" + bank + ".DAX").c_str()).records)
            {
                if (auto animation = decode_ega_animation(record.bytes))
                {
                    district->animations.emplace(record.id, std::move(animation.frames));
                }
            }
            town->districts.emplace(geo.id, std::move(district));
        }
    }
    return RolfTourSession(map->get(), program, std::move(sprites), 0xB071, std::move(*wall_art),
                           std::move(town));
}

RolfTourSession::RolfTourSession(GeoMap map, std::shared_ptr<const EclProgram> program,
                                 std::array<opengold::Image, 3> sprites, std::uint32_t entry,
                                 WallArtSet wall_art, std::shared_ptr<const PhlanResources> town)
    // A town supplies its own map and wall art, as restart() reloads them.
    : map_(town && town->map ? *town->map : std::move(map)), program_(std::move(program)),
      sprites_(std::move(sprites)),
      wall_art_(town && town->map ? town->wall_art : std::move(wall_art)), machine_(program_),
      entry_(entry), town_(std::move(town))
{
    // The members above are initialized once; only the per-run state is reset
    // here (Effective C++ Item 4).
    reset_run();
}

RolfTourSession RolfTourSession::detached_copy() const
{
    RolfTourSession copy(*this);
    copy.campaign_.reset();
    return copy;
}

namespace
{
// The original item a shop's record names, at the record's price. Its type
// must still match the list's, so a record made from other files fails here.
Equipment original_offering(const PhlanResources &town, const opengold::ShopOffering &offering)
{
    const std::map<unsigned, std::vector<Equipment>> *lists = nullptr;
    if (offering.archive == "ITEM3.DAX")
    {
        lists = &town.treasure;
    }
    for (const auto &[id, district] : town.districts)
    {
        if (offering.archive == "ITEM" + std::to_string(district->bank) + ".DAX")
        {
            lists = &district->treasure;
        }
    }
    if (lists)
    {
        const auto list = lists->find(offering.record);
        if (list != lists->end() && offering.index < list->second.size() &&
            list->second[offering.index].stored.type == offering.item_type)
        {
            auto item = list->second[offering.index];
            item.stored.value = static_cast<std::uint16_t>(offering.price_gp);
            return item;
        }
    }
    throw EclError("A shop sells an item the game's lists lack: " + offering.name);
}

// Gear OpenGoldBox adds (authored_items.h), sold by the record's bundle.
Equipment authored_offering(const opengold::ShopOffering &offering)
{
    Equipment item;
    item.stored.type = static_cast<std::uint8_t>(offering.item_type);
    item.stored.stored_name = offering.name;
    item.stored.value = static_cast<std::uint16_t>(offering.price_gp);
    item.stored.stack_size = static_cast<std::uint8_t>(offering.bundle_quantity);
    return item;
}
} // namespace

void RolfTourSession::shops(std::vector<opengold::ShopSite> sites)
{
    std::vector<std::vector<Equipment>> stocks;
    for (const auto &site : sites)
    {
        auto &stock = stocks.emplace_back();
        for (const auto &offering : site.offerings)
        {
            // A record may still list arrows or quarrels; there is no ammunition.
            if (offering.item_type < 128 &&
                opengold::original_item::is_ammunition(static_cast<std::uint8_t>(offering.item_type)))
            {
                continue;
            }
            stock.push_back(offering.archive.empty() ? authored_offering(offering)
                                                     : original_offering(*town_, offering));
            stock.back().index = stock.size() - 1;
        }
    }
    shops_ = std::move(sites);
    shop_stocks_ = std::move(stocks);
}

void RolfTourSession::restart()
{
    if (town_ && town_->map)
    {
        map_ = *town_->map;
        wall_art_ = town_->wall_art;
    }
    machine_ = EclMachine(program_);
    reset_run();
}

void RolfTourSession::reset_run()
{
    current_area_ = 0;
    visited_areas_.clear();
    seen_areas_.clear();
    rest_checks_ = 0;
    const auto revision = snapshot_.revision + 1;
    snapshot_ = {};
    snapshot_.revision = revision;
    menu_request_ = delayed_request_ = 0;
    remaining_delay_ = 0;
    party_ = {};
    treasure_.clear();
    picture_.reset();
    event_checkpoint_.reset();
    diagnostics_.clear();
    current_script_ = 0;
    event_stage_ = EventStage::none;
    selected_character_ = {};
    camp_kind_ = RestKind::long_rest;
    staged_enemies_.clear();
    staged_art_.clear();
    staged_records_.clear();
    encounter_menu_.reset();
    encounter_.reset();
    monster_picture_id_.reset();
    showing_monster_picture_ = false;
    combat_request_ = 0;
    pending_loot_.clear();
    staged_treasure_.reset();
    door_menu_ = pick_tried_ = false;
    door_choices_.clear();
    who_request_ = damage_request_ = 0;
    duel_ = false;
    who_slots_.clear();
    temple_visit_.reset();
    hall_visit_.reset();
    pending_movement_.reset();
    transition_ = message_only_ = false;
    shop_request_ = 0;
    if (town_ && !town_->sprite_archive.empty())
    {
        for (unsigned n = 0; n < 3; ++n)
        {
            auto decoded = decode_ega_sprite(town_->sprite_archive, 12, n);
            if (!decoded)
            {
                throw EclError("Cannot restore Rolf sprite");
            }
            sprites_[n] = std::move(decoded.image);
        }
    }
    // Minimal explicit isolated state, not a fabricated whole party/world model.
    for (const std::uint16_t address :
         {0x03DE, 0x49C9, 0x49FD, 0x4AC5, 0x4A07, 0x4A0F, 0x4A10, 0x4A11, 0x6DE1, 0x6E79,
          0x6E7A, 0x6E7B, 0x6E7C, 0x6E7D, 0x9801, 0xC04B, 0xC04C, 0xC04D, 0xC04E, 0xC04F})
    {
        machine_.bind_variable(EclAddress{address}, 0);
    }
    machine_.bind_variable(ecl_clock_hour, 12); // Research fixture: midday.
    for (const std::uint8_t opcode : {12, 13, 14, 45, 49, 58})
    {
        machine_.enable_host(opcode);
    }
    if (town_)
    {
        configure_town();
    }
    if (!machine_.start_at_for_inspection(entry_))
    {
        fail("Invalid isolated tour entry");
    }
}

void RolfTourSession::fail(std::string diagnostic)
{
    if (snapshot_.tour_finished && event_checkpoint_ && !closed_way_.empty())
    {
        auto message = std::exchange(closed_way_, {});
        roll_back_event();
        notice(std::move(message));
        return;
    }
    closed_way_.clear();
    if (snapshot_.tour_finished && event_checkpoint_)
    {
        diagnostics_.push_back(diagnostic);
        try
        {
            roll_back_event();
            notice("This event is not supported yet: " + diagnostic +
                   "\nThe event's changes were rolled back. You can continue exploring.");
            return;
        }
        catch (const std::exception &error)
        {
            // A rollback that cannot finish leaves the tour faulted rather than
            // half restored.
            diagnostic += "\nRolling the event back failed: " + std::string(error.what());
        }
    }
    snapshot_.phase = TourPhase::faulted;
    snapshot_.diagnostic = std::move(diagnostic);
    snapshot_.continue_ticket = 0;
    ++snapshot_.revision;
}

void RolfTourSession::roll_back_event()
{
    auto saved = std::move(*event_checkpoint_);
    event_checkpoint_.reset();
    // The steps that can throw come first: the party's restore checks before
    // it changes anything, and the area change needs the district's map.
    if (campaign_ && saved.campaign)
    {
        campaign_->restore(std::move(*saved.campaign));
    }
    change_area(saved.area);
    machine_ = std::move(saved.machine);
    party_ = std::move(saved.party);
    current_script_ = saved.script;
    map_ = std::move(saved.map);
    pending_loot_ = std::move(saved.pending_loot);
    staged_treasure_.reset();
    door_menu_ = false;
    door_choices_.clear();
    visited_areas_ = std::move(saved.visited_areas);
    seen_areas_ = std::move(saved.seen_areas);
    rest_checks_ = saved.rest_checks;
    travel_mode_ = saved.travel_mode;
    const auto revision = snapshot_.revision + 1;
    snapshot_ = std::move(saved.snapshot);
    snapshot_.revision = revision;
    who_request_ = damage_request_ = 0;
    duel_ = false;
    who_slots_.clear();
    temple_visit_.reset();
    hall_visit_.reset();
    selected_character_ = saved.selected_character;
    pending_movement_.reset();
    transition_ = false;
    delayed_request_ = shop_request_ = 0;
    treasure_.clear();
    staged_enemies_.clear();
    staged_art_.clear();
    staged_records_.clear();
    encounter_menu_.reset();
    encounter_.reset();
    monster_picture_id_.reset();
    showing_monster_picture_ = false;
    combat_request_ = 0;
    snapshot_.sprite_frame = -1;
    picture_.reset();
    ++snapshot_.picture_revision;
    publish_pose();
    snapshot_.script_id = current_script_;
}

void RolfTourSession::publish_pose()
{
    const auto facing = map_direction(machine_.variable(ecl_party_facing));
    const PartyPose next{machine_.variable(ecl_party_x), machine_.variable(ecl_party_y),
                         facing.value_or(MapDirection::north)};
    if (next.x >= 16 || next.y >= 16 || !facing)
    {
        throw EclError("Tour wrote an invalid party pose");
    }
    snapshot_.pose = next;
    snapshot_.overland = overland();
    if (overland())
    {
        snapshot_.overland_x = machine_.variable(ecl_overland_x);
        snapshot_.overland_y = machine_.variable(ecl_overland_y);
        snapshot_.overland_direction = machine_.variable(ecl_overland_direction) % 8;
    }
    else
    {
        snapshot_.visited.set(next.y * 16 + next.x);
        snapshot_.seen.set(next.y * 16 + next.x);
    }
    ++snapshot_.revision;
}

Image RolfTourSession::observe_view()
{
    if (overland() && town_ && town_->overland)
    {
        // The landmarks the story has revealed or changed (the original's
        // DrawOverlandWindow): each shows its own tile while its condition holds.
        const auto variable = [&](std::uint16_t address)
        {
            return machine_.variable(EclAddress{address});
        };
        const OverlandView view{
            town_->overland->window_start[travel_mode_ - 2] + snapshot_.overland_x,
            snapshot_.overland_y, snapshot_.overland_direction,
            [&](unsigned landmark)
            {
                switch (landmark)
                {
                case 3:
                    return variable(0x4A8C) != 0xFF;
                case 4:
                    return variable(0x4AD2) != 0 || (variable(0x4A7C) & 8) != 0 ||
                           variable(0x4A9F) == 0;
                case 5:
                    return (variable(0x4AA0) & 2) == 0;
                case 6:
                    return true;
                case 7:
                    return (variable(0x4AA0) & 1) == 0;
                case 8:
                case 9:
                    return variable(0x4AB3) > 0xFD;
                default:
                    return false;
                }
            }};
        return render_overland_window(*town_->overland, town_->overland_tiles, town_->party_icon,
                                      view);
    }
    const auto pose = snapshot_.pose;
    auto view = render_exploration_view(map_, wall_art_, pose.x, pose.y, pose.facing);
    // Before the opening script publishes a pose, (0,0) is only a default
    // value. An early UI refresh must not record it as explored territory.
    if (snapshot_.visited.none())
    {
        return std::move(view.image);
    }
    const auto known = snapshot_.seen | view.visible;
    if (known != snapshot_.seen)
    {
        snapshot_.seen = known;
        ++snapshot_.revision;
    }
    return std::move(view.image);
}

void RolfTourSession::handle_host(const EclRequest &request)
{
    if (town_ && snapshot_.tour_finished && handle_town_host(request))
    {
        return;
    }
    EclHostReply reply;
    const auto opcode = request.instruction->opcode;
    switch (opcode)
    {
    case 12:
        if (request.arguments[0].number() != 12 || request.arguments[1].number() != 2 ||
            request.arguments[2].number() != 9)
        {
            throw EclError("Unsupported encounter setup in Rolf tour");
        }
        snapshot_.sprite_frame = 2;
        break;
    case 13:
        if (snapshot_.sprite_frame > 0)
        {
            --snapshot_.sprite_frame;
        }
        if (encounter_distance_ > 0)
        {
            --encounter_distance_;
        }
        break;
    case 14:
        if (request.arguments[0].number() != 255)
        {
            throw EclError("Tour requested an unsupported picture");
        }
        snapshot_.sprite_frame = -1;
        break;
    case 49:
        snapshot_.sprite_frame = -1;
        break;
    case 58:
        remaining_delay_ = 0.22;
        delayed_request_ = request.id;
        ++snapshot_.revision;
        return;
    case 45:
    {
        // The address of the original engine routine the script calls.
        const auto service = request.arguments[0].address();
        if (service == EclAddress{0xC01E})
        {
            const auto facing = map_direction(machine_.variable(ecl_party_facing));
            PartyPose pose{machine_.variable(ecl_party_x), machine_.variable(ecl_party_y),
                           facing.value_or(MapDirection::north)};
            if (pose.x >= 16 || pose.y >= 16 || !facing)
            {
                throw EclError("Invalid scripted movement pose");
            }
            pose.x = (static_cast<int>(pose.x) + step_x(pose.facing) + 16) % 16;
            pose.y = (static_cast<int>(pose.y) + step_y(pose.facing) + 16) % 16;
            const auto &cell = map_.at(pose.x, pose.y);
            reply.writes = {{ecl_party_x, static_cast<std::uint16_t>(pose.x)},
                            {ecl_party_y, static_cast<std::uint16_t>(pose.y)},
                            {ecl_wall_ahead, cell.walls[index(pose.facing)]},
                            {ecl_cell_event, cell.event_raw}};
        }
        else if (service == EclAddress{0x2C90} || service == EclAddress{0xC018})
        {
            const auto x = machine_.variable(ecl_party_x), y = machine_.variable(ecl_party_y),
                       f = machine_.variable(ecl_party_facing);
            if (x >= 16 || y >= 16 || f >= 4)
            {
                throw EclError("Invalid redraw pose");
            }
            const auto &cell = map_.at(x, y);
            reply.writes = {{ecl_wall_ahead, cell.walls[f]}, {ecl_cell_event, cell.event_raw}};
            ++snapshot_.redraws;
        }
        else if (service == EclAddress{0x8001})
        {
            // The original's one-on-one duel: the next COMBAT is the
            // selected member's alone (coab's isDuel).
            duel_ = true;
        }
        else if (service == EclAddress{0xBA03})
        {
            if (machine_.variable(EclAddress{0x03DE}) != 8)
            {
                throw EclError("Unsupported tour sound selector");
            }
            ++snapshot_.footsteps; // Presentation plays an original OpenGoldBox footstep cue.
        }
        else if (service == EclAddress{0x0806} || service == EclAddress{0x2C4E} ||
                 service == EclAddress{0x2C51} || service == EclAddress{0xC003} ||
                 service == EclAddress{0xC009} || service == EclAddress{0xC01B})
        {
            // The original engine runs only its six known routines and does
            // nothing for any other address, such as the wilderness scripts'
            // map redraws and recolourings.
        }
        else
        {
            throw EclError("Unsupported native service in tour: " +
                           std::to_string(service.location));
        }
        break;
    }
    default:
        throw EclError("Unsupported tour host request");
    }
    if (!machine_.resume_host(request.id, reply))
    {
        throw EclError("Tour host reply rejected");
    }
    if (opcode == 45 && (request.arguments[0].address() == EclAddress{0x2C90} ||
                         request.arguments[0].address() == EclAddress{0xC01E}))
    {
        publish_pose();
    }
    ++snapshot_.revision;
}

void RolfTourSession::advance(double seconds)
{
    if (snapshot_.phase != TourPhase::running)
    {
        return;
    }
    if (!std::isfinite(seconds) || seconds < 0)
    {
        return;
    }
    try
    {
        if (delayed_request_)
        {
            remaining_delay_ -= std::min(seconds, 1.0);
            if (remaining_delay_ > 0)
            {
                return;
            }
            if (!machine_.resume_host(delayed_request_, {}))
            {
                throw EclError("Delayed reply rejected");
            }
            delayed_request_ = 0;
        }
        for (unsigned requests = 0; requests < 64; ++requests)
        {
            auto result = machine_.run(256);
            if (result.state == EclState::faulted)
            {
                fail(std::move(result.diagnostic));
                return;
            }
            if (result.state == EclState::completed)
            {
                if (town_ && snapshot_.tour_finished)
                {
                    finish_event();
                    return;
                }
                snapshot_.phase = TourPhase::completed;
                snapshot_.tour_finished = true;
                publish_pose();
                return;
            }
            if (!result.request)
            {
                return; // Instruction budget yield.
            }
            auto &request = *result.request;
            if (request.kind == EclRequestKind::text)
            {
                // Cleared text keeps a rune scroll on screen until the next
                // pause, as the original drew its runes above the text.
                if (request.clear)
                {
                    snapshot_.dialogue = rune_scroll_;
                }
                snapshot_.dialogue += request.text;
                if (snapshot_.dialogue.size() > 8192)
                {
                    throw EclError("Tour dialogue exceeds limit");
                }
                if (!machine_.resume(request.id))
                {
                    throw EclError("Text acknowledgement rejected");
                }
                ++snapshot_.revision;
            }
            else if (request.kind == EclRequestKind::menu)
            {
                if (request.vertical && !request.text.empty())
                {
                    snapshot_.dialogue += "\n" + request.text;
                }
                if (request.instruction && request.instruction->opcode == 44)
                {
                    announce_spokesman();
                }
                snapshot_.choices = std::move(request.choices);
                rune_scroll_.clear();
                menu_request_ = request.id;
                snapshot_.continue_ticket = ++next_ticket_;
                snapshot_.phase = TourPhase::awaiting_continue;
                ++snapshot_.prompts;
                ++snapshot_.revision;
                return;
            }
            else if (request.kind == EclRequestKind::host)
            {
                handle_host(request);
                if (delayed_request_ || snapshot_.phase != TourPhase::running)
                {
                    return;
                }
            }
            else if (request.kind == EclRequestKind::input_number ||
                     request.kind == EclRequestKind::input_string)
            {
                menu_request_ = request.id;
                snapshot_.continue_ticket = ++next_ticket_;
                snapshot_.number_input = request.kind == EclRequestKind::input_number;
                snapshot_.phase = TourPhase::awaiting_input;
                ++snapshot_.revision;
                return;
            }
            else
            {
                throw EclError("Unexpected input request");
            }
        }
    }
    catch (const std::exception &error)
    {
        fail(error.what());
    }
}

bool RolfTourSession::continue_dialogue(std::uint64_t ticket)
{
    return choose(ticket, 0);
}

void RolfTourSession::encounter_challenge(unsigned challenge)
{
    encounter_challenge_ = std::min(challenge, max_encounter_challenge);
}

bool RolfTourSession::camp(RestKind kind)
{
    if (kind != RestKind::short_rest && kind != RestKind::long_rest)
    {
        return false;
    }
    if (!town_ || !campaign_ || snapshot_.phase != TourPhase::completed || campaign_->in_combat() ||
        campaign_->state().short_rest || campaign_->state().spell_rest ||
        campaign_->state().training_rest)
    {
        return false;
    }
    camp_kind_ = kind;
    begin_event(EventTrigger::camp);
    advance(0);
    return true;
}

bool RolfTourSession::explore(ExplorationCommand command)
{
    if (command == ExplorationCommand::camp)
    {
        return camp(RestKind::long_rest);
    }
    if (snapshot_.phase != TourPhase::completed ||
        (campaign_ && (campaign_->in_combat() || campaign_->state().short_rest ||
                       campaign_->state().spell_rest || campaign_->state().training_rest)))
    {
        return false;
    }
    if (town_)
    {
        // On the wilderness map the party steps in eight directions; in a 3D
        // area it walks forward and turns.
        const bool step = command >= ExplorationCommand::north;
        if (step != overland() && command != ExplorationCommand::look)
        {
            return false;
        }
        if (step)
        {
            const auto direction = static_cast<unsigned>(command) -
                                   static_cast<unsigned>(ExplorationCommand::north);
            machine_.bind_variable(ecl_overland_direction, static_cast<std::uint16_t>(direction));
            snapshot_.overland_direction = direction;
            pending_movement_ = command;
            begin_event(EventTrigger::step);
        }
        else if (command == ExplorationCommand::forward)
        {
            pending_movement_ = command;
            begin_event(EventTrigger::step);
        }
        else if (command == ExplorationCommand::look)
        {
            begin_event(EventTrigger::look);
        }
        else
        {
            move_party(command);
            return true;
        } // Original map menu turns without running search_location.
        advance(0);
        return true;
    }
    auto pose = snapshot_.pose;
    if (command == ExplorationCommand::turn_left)
    {
        pose.facing = turned_left(pose.facing);
    }
    else if (command == ExplorationCommand::turn_right)
    {
        pose.facing = turned_right(pose.facing);
    }
    else if (command == ExplorationCommand::turn_around)
    {
        pose.facing = reversed(pose.facing);
    }
    else
    {
        const auto &edge = map_.at(pose.x, pose.y);
        const int x = static_cast<int>(pose.x) + step_x(pose.facing),
                  y = static_cast<int>(pose.y) + step_y(pose.facing);
        if (x < 0 || y < 0 || x >= 16 || y >= 16 || edge.walls[index(pose.facing)] ||
            edge.doors[index(pose.facing)])
        {
            return false;
        }
        const auto &other = map_.at(x, y);
        const auto reverse = reversed(pose.facing);
        if (other.walls[index(reverse)] || other.doors[index(reverse)])
        {
            return false;
        }
        pose.x = x;
        pose.y = y;
    }
    machine_.bind_variable(ecl_party_x, pose.x);
    machine_.bind_variable(ecl_party_y, pose.y);
    machine_.bind_variable(ecl_party_facing, static_cast<std::uint16_t>(index(pose.facing)));
    const auto &cell = map_.at(pose.x, pose.y);
    machine_.bind_variable(ecl_wall_ahead, cell.walls[index(pose.facing)]);
    machine_.bind_variable(ecl_cell_event, cell.event_raw);
    publish_pose();
    return true;
}
} // namespace opengold::por
