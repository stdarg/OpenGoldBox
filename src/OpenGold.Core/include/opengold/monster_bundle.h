#ifndef OPENGOLD_MONSTER_BUNDLE_H
#define OPENGOLD_MONSTER_BUNDLE_H

#include "opengold/formats.h"
#include "opengold/png_decoder.h"
#include <array>
#include <cstdint>
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

// The weapons a creature fights with: a readied ranged weapon, else its first;
// a readied melee weapon, else its first. Null where it has none.
struct WieldedItems
{
    const MonsterItem *melee{}, *ranged{};
};

[[nodiscard]] WieldedItems wielded_items(const ExportedMonster &monster);

// The weapon sets a creature shows as it attacks: a melee attack its melee
// weapon's, with the readied shield where a set has it, and a shot its ranged
// weapon's. Each falls back to the first set of its kind; null without one.
struct AttackVariants
{
    const MonsterVariant *melee{}, *ranged{};
};

[[nodiscard]] AttackVariants attack_variants(const ExportedMonster &monster);

// Whether the catalog draws the creature human-shaped, at a humanoid height.
[[nodiscard]] bool human_shaped(const ExportedMonster &monster);

// How many 128-pixel art squares fill one combat square, as the catalog
// previews it: by humanoid height, else by the footprint.
[[nodiscard]] double art_scale(const ExportedMonster &monster);

// The row below a pose's lowest opaque pixel, where its feet stand.
[[nodiscard]] unsigned planted_ground(const Image &pose);

// The original combat icon CPIC<bank>.DAX `icon` a Wild Shape or summoned form
// wears (docs/monster-art-mapping.md). The creature MON<bank>CHA record `icon`
// is drawn with the same icon, so the form wears that creature's new art.
struct FormIcon
{
    std::string_view form;
    unsigned bank;
    std::uint8_t icon;
};

// Summon Dragon's spirit wears Tyranthraxus's icon, Animate Objects' objects
// the Skeleton's (LEVELS-4), Giant Insect's insects the Large Scorpion's.
inline constexpr std::array form_icons{
    FormIcon{"wolf", 4, 106},           FormIcon{"boar", 6, 120},
    FormIcon{"giant_lizard", 8, 59},    FormIcon{"giant_snake", 5, 60},
    FormIcon{"tiger", 6, 119},          FormIcon{"giant_insect", 4, 18},
    FormIcon{"draconic_spirit", 5, 66}, FormIcon{"animated_object", 4, 34}};
} // namespace opengold::por
#endif
