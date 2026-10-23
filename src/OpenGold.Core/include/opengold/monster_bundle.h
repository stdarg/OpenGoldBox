#ifndef OPENGOLD_MONSTER_BUNDLE_H
#define OPENGOLD_MONSTER_BUNDLE_H

#include "opengold/formats.h"
#include "opengold/png_decoder.h"
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace opengold::por
{
// An item a monster carries, as the Combat Sprite Catalog exported it (#274).
// An original item names its MONnITM slot; gear added in the catalog has
// source_index -1 and is known only by name.
struct MonsterItem
{
    std::string id, name;
    std::string kind;        // "weapon", "shield" or "gear"
    std::string weapon_role; // "one_hand", "two_hand", "ranged", or empty
    bool readied{};
    int source_type{-1};
    int source_index{-1};
};

// One weapon set's new art: a 128 x 128 ready pose and action pose.
struct MonsterVariant
{
    std::string id, weapon_id, shield_id;
    Image ready, action;
};

// A creature from data/monsters: an original MONnCHA record, or a copy of one
// the catalog made, with its name, gear and new art.
struct ExportedMonster
{
    std::string key; // "MON2CHA.DAX:4", or "CUSTOM:..." for a copy
    std::string creature_archive;
    unsigned creature_record{};
    std::string copied_from, origin_key; // empty unless a copy
    std::string name;
    unsigned width_squares{1}, height_squares{1};
    std::string humanoid_size{"na"}; // "small", "normal", "tall" or "na"
    std::vector<MonsterItem> equipment;
    std::vector<MonsterVariant> variants; // none while art awaits regeneration
};

// Every exported creature, by key.
struct MonsterBundle
{
    std::map<std::string, ExportedMonster> monsters;

    // The original creature MON<bank>CHA.DAX record `record`, if exported.
    [[nodiscard]] const ExportedMonster *creature(unsigned bank, unsigned record) const;
};

// Reads data/monsters: one folder per creature holding monster.json and each
// weapon set's ready.png and action.png. A missing folder means no export.
// Throws on a record the game cannot use.
[[nodiscard]] MonsterBundle load_monster_bundle(const std::filesystem::path &folder,
                                                const PngDecoder &decode);

// The weapon set to show, with the readied shield where a set has it: an
// archer's bow, else a readied melee weapon, else any readied weapon, else
// the first set. Null without art.
[[nodiscard]] const MonsterVariant *readied_variant(const ExportedMonster &monster, bool archer);

// How many 128-pixel art squares fill one combat square, as the catalog
// previews it: by humanoid height, else by the footprint.
[[nodiscard]] double art_scale(const ExportedMonster &monster);

// The row below a pose's lowest opaque pixel, where its feet stand.
[[nodiscard]] unsigned planted_ground(const Image &pose);
} // namespace opengold::por
#endif
