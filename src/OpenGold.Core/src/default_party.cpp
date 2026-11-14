#include "opengold/default_party.h"
#include "archive_io.h"
#include "opengold/creature_catalog.h"
#include "opengold/equipment.h"
#include "opengold/pool_radiance.h"
#include "opengold/por_sound.h"
#include <array>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace opengold
{
namespace
{
using por::detail::upper_ascii;

[[nodiscard]] std::filesystem::path saved_file(const std::filesystem::path &folder,
                                               const std::string &name)
{
    return por::detail::find_archive<std::runtime_error>(
        folder, upper_ascii(name), "More than one " + name + " in the saved game folder",
        "The saved game A has no " + name);
}

// The game folder's SAVE folder, whatever the case of its name.
[[nodiscard]] std::filesystem::path saved_game_folder(const std::filesystem::path &game_folder)
{
    for (const auto &entry : std::filesystem::directory_iterator(game_folder))
    {
        if (entry.is_directory() && upper_ascii(entry.path().filename().string()) == "SAVE")
        {
            return entry.path();
        }
    }
    throw std::runtime_error("The game folder has no SAVE folder with the saved game A");
}

[[nodiscard]] std::vector<std::uint8_t> read_saved_file(const std::filesystem::path &path)
{
    const auto name = path.filename().string();
    return por::detail::read_archive_bytes<std::runtime_error>(
        path, name + " is too large", "Cannot open " + name, "Cannot read " + name);
}

// The member files SAVGAMA.DAT names (CHRDATA1 ...): it ends with their count
// and eight names of up to 40 letters, each after its length.
[[nodiscard]] std::vector<std::string> member_names(std::span<const std::uint8_t> saved_game)
{
    constexpr std::size_t name_size = 41, names = 8;
    if (saved_game.size() < 1 + names * name_size)
    {
        throw std::runtime_error("SAVGAMA.DAT is too short");
    }
    const auto list = saved_game.last(1 + names * name_size);
    const std::size_t count = list[0];
    if (count == 0 || count > names)
    {
        throw std::runtime_error("SAVGAMA.DAT names no party");
    }
    std::vector<std::string> found;
    for (std::size_t n = 0; n < count; ++n)
    {
        const auto name = list.subspan(1 + n * name_size, name_size);
        if (name[0] == 0 || name[0] >= name_size)
        {
            throw std::runtime_error("SAVGAMA.DAT has an unreadable member name");
        }
        found.emplace_back(name.begin() + 1, name.begin() + 1 + name[0]);
    }
    return found;
}

// The portrait and combat icon a record keeps after the experience its defeat
// awards (portraitHead to iconColors): 0 for none, as in the later games' records.
[[nodiscard]] por::CharacterAppearance record_appearance(std::span<const std::uint8_t> record)
{
    const std::size_t at = record.size() == 285 ? 187 : record.size() == 422 ? 319 : 337;
    const auto head = record[at], body = record[at + 1];
    por::CharacterAppearance appearance;
    if (head != 0 && body != 0)
    {
        appearance.portrait_head = head;
        appearance.portrait_body = body;
    }
    appearance.combat_head = record[at + 2];
    appearance.combat_body = record[at + 3];
    appearance.icon_size = record[at + 5] == 1 ? por::IconSize::small : por::IconSize::regular;
    // Each color byte holds Color-1 in its low half and Color-2 in its high
    // half, for body, arm, leg, hair, shield and weapon; the appearance keeps
    // weapon, body, hair, shield, arms and legs.
    constexpr std::array<std::size_t, 6> original_part{5, 0, 3, 4, 1, 2};
    for (std::size_t part = 0; part < original_part.size(); ++part)
    {
        const auto colors = record[at + 6 + original_part[part]];
        appearance.colors[0][part] = colors & 0x0F;
        appearance.colors[1][part] = colors >> 4;
    }
    return appearance;
}

// What the original's item files need to be items: the game's item types and
// the words of their names.
struct ItemSources
{
    std::vector<por::ItemTemplate> templates;
    std::vector<std::string> words;
};

[[nodiscard]] ItemSources item_sources(const std::filesystem::path &game_folder, GoldBoxGame game)
{
    const std::string types_file(item_types_file(game));
    auto templates =
        por::decode_item_templates(read_saved_file(saved_file(game_folder, types_file)), game);
    if (!templates)
    {
        throw std::runtime_error("Unreadable " + types_file);
    }
    ItemSources sources{std::move(*templates), {}};
    const auto image = por::unpack_sound_executable(
        read_saved_file(saved_file(game_folder, std::string(item_words_program(game)))));
    if (auto words = por::decode_item_words(image, game))
    {
        sources.words = std::move(*words);
    }
    return sources;
}

[[nodiscard]] std::vector<por::Equipment> read_items(const std::filesystem::path &path,
                                                     GoldBoxGame game, const ItemSources &sources)
{
    const auto records = por::decode_items(read_saved_file(path), game);
    if (!records)
    {
        throw std::runtime_error("Unreadable " + path.filename().string());
    }
    std::vector<por::Equipment> items;
    for (const auto &record : *records)
    {
        if (record.type >= sources.templates.size())
        {
            throw std::runtime_error("Unknown item type in " + path.filename().string());
        }
        por::Equipment item;
        item.index = items.size();
        item.stored = record;
        // A saved item's name bytes hold the line the item screen last showed
        // (" Yes  Shield +1"), so it is named from its words, as a monster's is.
        item.stored.stored_name.clear();
        item.base = sources.templates[record.type];
        item.bonuses = por::equipment_bonuses(record, item.base);
        if (por::item_type_name(record.type).empty())
        {
            item.decoded_name = por::item_name(record, sources.words);
            item.unidentified_name =
                por::item_name(record, sources.words, record.hidden_name_words);
        }
        items.push_back(std::move(item));
    }
    return items;
}

// A member as the saved game keeps it, read whole before the party changes.
struct SavedMember
{
    Character character;
    unsigned experience{};
    std::array<std::uint16_t, 7> wealth{};
    std::vector<por::Equipment> items;
};

[[nodiscard]] SavedMember read_member(const std::filesystem::path &save_folder,
                                      const std::string &name, GoldBoxGame game,
                                      const rules::CharacterRules &creation,
                                      std::optional<ItemSources> &sources,
                                      const std::filesystem::path &game_folder)
{
    const auto bytes = read_saved_file(saved_file(save_folder, name + ".SAV"));
    const auto record = por::decode_character(bytes, game);
    if (!record)
    {
        throw std::runtime_error("Unreadable " + name + ".SAV");
    }
    auto draft = creation.record_draft(bytes);
    if (!draft)
    {
        throw std::runtime_error("The default party plays only under the original rules");
    }
    // Secret's "Guy de Valois " keeps a space after its name.
    while (draft->name.ends_with(' '))
    {
        draft->name.pop_back();
    }
    SavedMember member{Character(creation, *draft, record_appearance(bytes)),
                       record->experience,
                       record->wealth,
                       {}};
    // Pool of Radiance's item files; the later games' saved game A has none.
    const auto items_name = upper_ascii(name + ".ITM");
    for (const auto &file : std::filesystem::directory_iterator(save_folder))
    {
        if (upper_ascii(file.path().filename().string()) != items_name)
        {
            continue;
        }
        if (!sources)
        {
            sources = item_sources(game_folder, game);
        }
        member.items = read_items(file.path(), game, *sources);
    }
    return member;
}

void add_member(CampaignParty &party, SavedMember member)
{
    const auto id = party.add_pc(std::move(member.character));
    auto state = party.checkpoint();
    state.roster.back().experience = member.experience;
    party.restore(std::move(state));
    for (const auto &item : member.items)
    {
        const auto before = party.member(id).character.inventory().items().size();
        party.take_item(id, item);
        const auto &now = party.member(id).character.inventory().items();
        if (item.stored.readied() && now.size() > before)
        {
            (void)party.equip(id, now.back().id);
        }
    }
    for (std::size_t coin = 0; coin < member.wealth.size(); ++coin)
    {
        party.add_coins(static_cast<Coin>(coin), member.wealth[coin]);
    }
}
} // namespace

void add_default_party(CampaignParty &party, GoldBoxGame game,
                       const std::filesystem::path &game_folder)
{
    const auto save_folder = saved_game_folder(game_folder);
    std::optional<ItemSources> sources;
    std::vector<SavedMember> members;
    for (const auto &name : member_names(read_saved_file(saved_file(save_folder, "SAVGAMA.DAT"))))
    {
        members.push_back(
            read_member(save_folder, name, game, *party.character_rules(), sources, game_folder));
    }
    const auto before = party.checkpoint();
    try
    {
        for (auto &member : members)
        {
            add_member(party, std::move(member));
        }
    }
    catch (...)
    {
        party.restore(before);
        throw;
    }
}
} // namespace opengold
