#include "opengold/magic_item_catalog.h"
#include "archive_io.h"
#include "opengold/ecl_machine.h"
#include "opengold/formats.h"
#include "opengold/por_sound.h"
#include <map>
#include <set>
#include <string_view>
#include <utility>

namespace opengold::por
{
namespace
{
constexpr std::uint8_t magic_user_scroll = 61, cleric_scroll = 62;

using ListKey = std::pair<std::string, std::uint8_t>;
using Records = std::map<std::uint8_t, std::vector<std::uint8_t>>;

std::vector<std::uint8_t> read_game_file(const std::filesystem::path &directory,
                                         std::string_view name)
{
    const auto path = detail::find_archive<MagicItemError>(
        directory, name, "Ambiguous " + std::string(name), "Missing " + std::string(name));
    return detail::read_archive_bytes<MagicItemError>(path, std::string(name) + " is too large",
                                                      "Cannot open " + path.string(),
                                                      "Incomplete read of " + path.string());
}

Records read_dax(const std::filesystem::path &directory, const std::string &name)
{
    auto decoded = decode_dax_archive(read_game_file(directory, name));

    if (!decoded)
    {
        throw MagicItemError("Invalid DAX archive " + name);
    }

    Records records;

    for (auto &record : decoded.records)
    {
        records.emplace(record.id, std::move(record.bytes));
    }

    return records;
}

// Lists named by a constant: TREASURE's item code below 128 picks an ITEMn list, and
// LOAD MONSTER's first operand a MONn creature. n is the script's bank.
std::map<ListKey, std::set<std::string>> script_references(const std::filesystem::path &directory)
{
    std::map<ListKey, std::set<std::string>> references;
    const auto catalog = EclCatalog::load(directory);

    for (const auto &[id, program] : catalog.all())
    {
        const auto bank = id.archive.substr(3, 1);
        const auto script = id.archive + ":" + std::to_string(id.record);

        for (const auto &i : walk_reachable(*program).instructions)
        {
            const auto name = ecl_opcode(i.opcode).name;

            if (name == "TREASURE" && i.operands.size() == 8 && i.operands[7].tag == 0 &&
                i.operands[7].value < 128)
            {
                references[{"ITEM" + bank + ".DAX", static_cast<std::uint8_t>(i.operands[7].value)}]
                    .insert(script);
            }
            else if (name == "LOAD MONSTER" && !i.operands.empty() && i.operands[0].tag == 0 &&
                     i.operands[0].value < 256)
            {
                references[{"MON" + bank + "ITM.DAX",
                            static_cast<std::uint8_t>(i.operands[0].value)}]
                    .insert(script);
            }
        }
    }

    return references;
}

std::vector<std::string> spells_of(const ItemRecord &item, const std::vector<std::string> &names)
{
    const auto spell = [&](std::uint8_t number)
    {
        return number < names.size() ? names[number] : "Spell " + std::to_string(number);
    };
    std::vector<std::string> spells;

    if (item.type == magic_user_scroll || item.type == cleric_scroll)
    {
        for (const auto number : item.effect_codes)
        {
            if (number != 0)
            {
                spells.push_back(spell(number));
            }
        }
    }

    // Byte 61 names a spell only on charged items; higher numbers are item-only powers.
    else if (item.effect_codes[2] == 0 && item.effect_codes[1] != 0 &&
             item.effect_codes[1] < names.size())
    {
        spells.push_back(spell(item.effect_codes[1]));
    }
    return spells;
}

void add_lists(MagicItemCatalog &catalog, const std::string &archive, const Records &records,
               const Records *carriers, const std::map<ListKey, std::set<std::string>> &references)
{
    for (const auto &[record, bytes] : records)
    {
        const auto items = decode_items(bytes);

        if (!items)
        {
            throw MagicItemError("Invalid item list " + archive + ":" + std::to_string(record));
        }

        MagicItemList list{archive, record, {}, {}, {}};

        for (const auto &item : *items)
        {
            if (is_magic(item))
            {
                list.items.push_back({item, item_name(item, catalog.item_words),
                                      spells_of(item, catalog.spell_names)});
            }
        }

        if (list.items.empty())
        {
            continue;
        }

        if (carriers)
        {
            const auto creature = carriers->find(record);
            const auto decoded =
                creature == carriers->end() ? std::nullopt : decode_character(creature->second);
            list.carrier = decoded ? decoded->name : "";
        }

        if (const auto found = references.find({archive, record}); found != references.end())
        {
            list.scripts.assign(found->second.begin(), found->second.end());
        }

        catalog.lists.push_back(std::move(list));
    }
}
} // namespace

bool is_magic(const ItemRecord &item) noexcept
{
    return item.magic_bonus != 0 || item.cursed_raw != 0 || item.effect_codes[0] != 0 ||
           item.effect_codes[1] != 0;
}

MagicItemCatalog MagicItemCatalog::load(const std::filesystem::path &directory)
{
    MagicItemCatalog catalog;
    const auto image = unpack_sound_executable(read_game_file(directory, "START.EXE"));
    auto words = decode_item_words(image);
    auto spells = decode_spell_names(image);

    if (!words || !spells)
    {
        throw MagicItemError("START.EXE has no item or spell name table");
    }

    catalog.item_words = std::move(*words);
    catalog.spell_names = std::move(*spells);
    const auto references = script_references(directory);

    for (int bank = 1; bank <= 8; ++bank)
    {
        const auto digit = std::to_string(bank);
        add_lists(catalog, "ITEM" + digit + ".DAX", read_dax(directory, "ITEM" + digit + ".DAX"),
                  nullptr, references);
    }

    for (int bank = 1; bank <= 8; ++bank)
    {
        const auto digit = std::to_string(bank);
        const auto carriers = read_dax(directory, "MON" + digit + "CHA.DAX");
        add_lists(catalog, "MON" + digit + "ITM.DAX",
                  read_dax(directory, "MON" + digit + "ITM.DAX"), &carriers, references);
    }

    return catalog;
}
} // namespace opengold::por
