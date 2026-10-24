// data/monsters as the game reads it (#274): the Combat Sprite Catalog's export
// of each creature's name, gear and new art, and which weapon set shows.
#include "opengold/monster_bundle.h"
#include "png_size_only.h"
#include "test_support.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

using namespace opengold;
using namespace opengold::por;
using opengold::test::check;
using opengold::test::png_size_only;

namespace
{
const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR);

const ExportedMonster &slums(const MonsterBundle &bundle, unsigned record)
{
    const auto *found = bundle.creature(2, record);
    check(found != nullptr, "The Slums creature " + std::to_string(record) + " is exported");
    return *found;
}

std::string text(const std::filesystem::path &path)
{
    std::ifstream in(path);
    return {std::istreambuf_iterator<char>(in), {}};
}

// The shipped export: names, readied weapon sets and the art's scale.
void shipped()
{
    const auto bundle = load_monster_bundle(root / "data/monsters", png_size_only);
    check(bundle.monsters.size() >= 160, "The catalog's creatures are exported");
    const auto &orc = slums(bundle, 4);
    check(orc.name == "ORC" && orc.creature_archive == "MON2CHA.DAX" && orc.creature_record == 4,
          "An exported creature names its original record");
    const auto *armed = readied_variant(orc, false);
    check(armed && armed == &orc.variants.front() && !armed->shield_id.empty() &&
              armed->ready.width == 128 && armed->action.height == 128,
          "The orc shows its readied sword and shield, in both poses");
    const auto &leader = slums(bundle, 5);
    const auto *bow = readied_variant(leader, true);
    check(bow && bow->id == "item_003__ranged" && readied_variant(leader, false)->id == "item_000__one_hand",
          "An orc leader shows its bow only as the archer");
    const auto &kobold = slums(bundle, 0);
    check(readied_variant(kobold, false) == &kobold.variants.front(),
          "With no weapon readied, the first weapon set shows");
    check(slums(bundle, 8).variants.size() == 1 && slums(bundle, 8).variants[0].id == "default",
          "A creature without weapons has one default pair");
    const auto archer = wielded_items(leader, true), fighter = wielded_items(leader, false);
    check(archer.melee && archer.melee->name == "Battle axe" && archer.ranged &&
              archer.ranged->name == "Long bow" && fighter.melee == archer.melee && !fighter.ranged,
          "An orc leader swings its battle axe and shoots its bow only as the archer");
    check(wielded_items(kobold, false).melee == &kobold.equipment.front(),
          "With no weapon readied, a creature fights with its first");
    check(human_shaped(kobold) && human_shaped(slums(bundle, 8)) &&
              !human_shaped(*bundle.creature(3, 106)),
          "Kobolds and ogres are human-shaped; wolves are not");
    check(art_scale(slums(bundle, 2)) == 1.0 && art_scale(orc) == 1.45,
          "Art scales by humanoid height, as the catalog previews it");
    for (const auto &entry : std::filesystem::directory_iterator(root / "data/monsters"))
    {
        if (entry.is_directory())
        {
            check(text(entry.path() / "monster.json").find("dev_tools") == std::string::npos,
                  "The export names nothing in dev_tools");
        }
    }
    check(load_monster_bundle(root / "no-such-folder", png_size_only).monsters.empty(),
          "No export, no new creatures");
}

struct Folder
{
    std::filesystem::path path =
        std::filesystem::temp_directory_path() /
        ("opengold-monsters-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

    Folder()
    {
        std::filesystem::create_directories(path);
    }

    Folder(const Folder &) = delete;
    Folder &operator=(const Folder &) = delete;

    ~Folder()
    {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
};

const std::string copy_record = R"({"format": "opengold-monster-v1", "key": "CUSTOM:1_0",
    "creature_archive": "MON2CHA.DAX", "creature_record": 4, "copied_from": "MON2CHA.DAX:4",
    "origin_key": "MON2CHA.DAX:4", "name": "ORC CAPTAIN", "sprite_size": null,
    "humanoid_size": "tall", "equipment": [
      {"id": "item_000", "kind": "weapon", "name": "Mace", "readied": true,
       "source_index": 0, "source_type": 32, "weapon_role": "one_hand"},
      {"id": "added_1", "kind": "weapon", "name": "short bow", "readied": false,
       "source_index": -1, "source_type": -1, "weapon_role": "ranged"}],
    "variants": [{"id": "item_000__one_hand", "weapon_id": "item_000", "shield_id": "",
      "ready": "item_000__one_hand/ready.png", "action": "item_000__one_hand/action.png"}]})";

// A copy the catalog made, and records the game refuses.
void handmade()
{
    const auto png = text(root / "data/monsters/mon2cha_dax_2/item_000__item_001/ready.png");
    const auto write = [&](const std::filesystem::path &folder, const std::string &record)
    {
        std::filesystem::create_directories(folder / "item_000__one_hand");
        std::ofstream(folder / "monster.json") << record;
        std::ofstream(folder / "item_000__one_hand/ready.png", std::ios::binary) << png;
        std::ofstream(folder / "item_000__one_hand/action.png", std::ios::binary) << png;
    };
    {
        Folder folder;
        write(folder.path / "custom_1_0", copy_record);
        const auto bundle = load_monster_bundle(folder.path, png_size_only);
        const auto &copy = bundle.monsters.at("CUSTOM:1_0");
        check(copy.origin_key == "MON2CHA.DAX:4" && copy.name == "ORC CAPTAIN" &&
                  copy.equipment[1].source_index == -1 && art_scale(copy) == 2.1 &&
                  !bundle.creature(2, 4),
              "A copy keeps its own name and gear, apart from its source");
    }
    const auto refused = [&](std::string record, std::string_view from, std::string_view to,
                             const std::string &message)
    {
        Folder folder;
        record.replace(record.find(from), from.size(), to);
        write(folder.path / "custom_1_0", record);
        test::rejects(
            [&]
            {
                (void)load_monster_bundle(folder.path, png_size_only);
            },
            message);
    };
    refused(copy_record, "item_000__one_hand/ready.png", "../ready.png",
            "Art outside the creature's folder is refused");
    refused(copy_record, "opengold-monster-v1", "opengold-monster-v9",
            "An unknown format is refused");
    refused(copy_record, "\"kind\": \"weapon\"", "\"kind\": \"wand\"",
            "An unknown kind of item is refused");
    Folder small;
    write(small.path / "custom_1_0", copy_record);
    std::ofstream(small.path / "custom_1_0/item_000__one_hand/ready.png", std::ios::binary)
        << text(root / "data/art/walls/WALLDEF1_DAX_1_1/far_front.png");
    test::rejects(
        [&]
        {
            (void)load_monster_bundle(small.path, png_size_only);
        },
        "Art that is not 128 x 128 is refused");
}

// Feet stand on the row below the lowest opaque pixel.
void planted()
{
    Image pose;
    pose.width = pose.height = 128;
    pose.rgba.assign(128 * 128 * 4, 0);
    pose.rgba[(100 * 128 + 64) * 4 + 3] = 255;
    check(planted_ground(pose) == 101, "The feet stand below the lowest opaque pixel");
}
} // namespace

int main()
{
    try
    {
        shipped();
        handmade();
        planted();
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Monster bundle tests passed\n";
}
