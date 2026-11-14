#include "opengold/default_party.h"
#include "json_document.h"
#include "opengold/diagnostic_log.h"
#include "opengold/dice.h"
#include "opengold/pool_radiance.h"
#include <algorithm>
#include <array>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace opengold
{
namespace
{
using detail::Json;

// Reads one file, naming it, the member and the field in each error.
class PartyReader
{
  public:
    explicit PartyReader(std::filesystem::path file) : file_(std::move(file))
    {
    }

    [[noreturn]] void reject(std::string_view where, std::string_view problem) const
    {
        throw std::runtime_error(file_.filename().string() + ": " + std::string(where) + ": " +
                                 std::string(problem));
    }

    template <typename T>
    const T &field(const Json &object, std::string_view key, std::string_view where) const
    {
        const auto *found = object.find(key);
        const auto *value = found ? std::get_if<T>(&found->value) : nullptr;
        if (!value)
        {
            reject(where, "missing or invalid " + std::string(key));
        }
        return *value;
    }

    const std::string &text(const Json &object, std::string_view key, std::string_view where) const
    {
        const auto &value = field<std::string>(object, key, where);
        if (value.empty())
        {
            reject(where, "empty " + std::string(key));
        }
        return value;
    }

    int whole(const Json &value, std::string_view where, int low, int high) const
    {
        const auto *number = std::get_if<double>(&value.value);
        if (!number || *number != static_cast<int>(*number) || *number < low || *number > high)
        {
            reject(where, "a whole number from " + std::to_string(low) + " to " +
                              std::to_string(high) + " is needed");
        }
        return static_cast<int>(*number);
    }

    int whole_field(const Json &object, std::string_view key, std::string_view where, int low,
                    int high) const
    {
        const auto *found = object.find(key);
        if (!found)
        {
            reject(where, "missing " + std::string(key));
        }
        return whole(*found, std::string(where) + ", " + std::string(key), low, high);
    }

    // An object member, kept as the Json that holds it.
    const Json &object(const Json &parent, std::string_view key, std::string_view where) const
    {
        const auto *found = parent.find(key);
        if (!found || !std::holds_alternative<Json::Object>(found->value))
        {
            reject(where, "missing or invalid " + std::string(key));
        }
        return *found;
    }

    std::array<int, 6> six(const Json &values, std::string_view where, int low, int high) const
    {
        const auto *array = std::get_if<Json::Array>(&values.value);
        if (!array || array->size() != 6)
        {
            reject(where, "six numbers are needed");
        }
        std::array<int, 6> result{};
        for (std::size_t n = 0; n < array->size(); ++n)
        {
            result[n] = whole((*array)[n], where, low, high);
        }
        return result;
    }

    std::array<int, 6> six_field(const Json &object, std::string_view key, std::string_view where,
                                 int low, int high) const
    {
        const auto *found = object.find(key);
        if (!found)
        {
            reject(where, "missing " + std::string(key));
        }
        return six(*found, std::string(where) + ", " + std::string(key), low, high);
    }

    rules::Ability ability(const Json &object, std::string_view key, std::string_view where) const
    {
        constexpr std::array<std::string_view, 6> names{"strength",     "dexterity", "constitution",
                                                        "intelligence", "wisdom",    "charisma"};
        const auto &name = text(object, key, where);
        const auto found = std::ranges::find(names, name);
        if (found == names.end())
        {
            reject(where, std::string(key) + " names no ability: " + name);
        }
        return rules::all_abilities[static_cast<std::size_t>(found - names.begin())];
    }

    std::vector<DefaultItem> items(const Json &object, std::string_view where, bool typed) const
    {
        std::vector<DefaultItem> result;
        for (const auto &entry : field<Json::Array>(object, "items", where))
        {
            DefaultItem item;
            item.name = text(entry, "name", where);
            const auto item_where = std::string(where) + ", item " + item.name;
            if (typed)
            {
                item.type = whole_field(entry, "type", item_where, 0, 255);
            }
            else
            {
                item.key = text(entry, "key", item_where);
            }
            if (entry.find("quantity"))
            {
                item.quantity =
                    static_cast<unsigned>(whole_field(entry, "quantity", item_where, 1, 255));
            }
            if (const auto *readied = entry.find("readied"))
            {
                const auto *flag = std::get_if<bool>(&readied->value);
                if (!flag)
                {
                    reject(item_where, "readied is true or false");
                }
                item.readied = *flag;
            }
            result.push_back(std::move(item));
        }
        return result;
    }

    std::vector<std::string> spells(const Json &entry, std::string_view where) const
    {
        std::vector<std::string> result;
        for (const auto &spell : field<Json::Array>(entry, "spells", where))
        {
            const auto *id = std::get_if<std::string>(&spell.value);
            if (!id || id->empty())
            {
                reject(where, "a spell is named by its id");
            }
            result.push_back(*id);
        }
        return result;
    }

    por::CharacterAppearance appearance(const Json &member, std::string_view where) const
    {
        por::CharacterAppearance look;
        const auto &portrait = object(member, "portrait", where);
        look.portrait_head = static_cast<unsigned>(whole_field(portrait, "head", where, 0, 255));
        look.portrait_body = static_cast<unsigned>(whole_field(portrait, "body", where, 1, 255));
        const auto &icon = object(member, "combat_icon", where);
        look.combat_head = static_cast<unsigned>(whole_field(icon, "head", where, 0, 13));
        look.combat_body = static_cast<unsigned>(whole_field(icon, "body", where, 0, 31));
        const auto &colors = field<Json::Array>(icon, "colors", where);
        if (colors.size() != 2)
        {
            reject(where, "colors needs Color-1 and Color-2");
        }
        for (std::size_t bank = 0; bank < 2; ++bank)
        {
            const auto shades = six(colors[bank], std::string(where) + ", colors", 0, 15);
            std::ranges::copy(shades, look.colors[bank].begin());
        }
        return look;
    }

    DefaultPartyMember::Original original(const Json &entry, std::string_view where) const
    {
        DefaultPartyMember::Original result;
        result.race = text(entry, "race", where);
        result.character_class = text(entry, "class", where);
        result.alignment = text(entry, "alignment", where);
        const auto scores = six_field(entry, "scores", where, 3, 19);
        std::ranges::copy(scores, result.scores.begin());
        result.items = items(entry, where, true);
        result.spells = spells(entry, where);
        return result;
    }

    DefaultPartyMember::Srd srd(const Json &entry, const std::string &name,
                                std::string_view where) const
    {
        DefaultPartyMember::Srd result;
        auto &character = result.character;
        character.name = name;
        character.race = text(entry, "race", where);
        character.character_class = text(entry, "class", where);
        character.alignment = text(entry, "alignment", where);
        character.background = text(entry, "background", where);
        character.rolled = six_field(entry, "rolled", where, 3, 18);
        const auto &increase = object(entry, "background_increase", where);
        character.plus_two = ability(increase, "plus_two", where);
        character.plus_one = ability(increase, "plus_one", where);
        if (character.plus_two == character.plus_one)
        {
            reject(where, "the background raises two different abilities");
        }
        result.items = items(entry, where, false);
        character.spells = spells(entry, where);
        return result;
    }

  private:
    std::filesystem::path file_;
};

// Rules that roll in place are the original's; the SRD's assign totals.
[[nodiscard]] bool rolls_in_place(const rules::CharacterRules &creation)
{
    return creation.ability_generation() == rules::AbilityGeneration::rolled_in_place;
}

// The look a member has under these rules: short folk stand short, as the
// character pool's do (character_pool.cpp).
[[nodiscard]] por::CharacterAppearance look_under(const por::CharacterAppearance &look,
                                                  std::string_view race, bool original)
{
    auto result = look;
    const bool short_folk =
        race == "gnome" || (original && (race == "dwarf" || race == "halfling"));
    result.icon_size = short_folk ? por::IconSize::small : por::IconSize::regular;
    return result;
}

// An item of the game's own types, given for nothing, with the name it shows.
[[nodiscard]] por::Equipment original_item(const DefaultItem &item)
{
    std::array<std::uint8_t, 63> raw{};
    raw[46] = static_cast<std::uint8_t>(item.type);
    por::Equipment equipment;
    equipment.stored = por::decode_items(raw)->front();
    equipment.stored.stack_size = static_cast<std::uint8_t>(item.quantity);
    equipment.stored.stored_name = item.name;
    return equipment;
}

// The original's member: chosen as creation chooses, rolled with its place in
// the party as the seed, and its rolls made the file's.
MemberId add_original_member(CampaignParty &party, const DefaultPartyMember &member,
                             std::uint64_t seed)
{
    const auto &creation = *party.character_rules();
    const auto &chosen = member.original;
    rules::CharacterDraft draft;
    draft.name = member.name;
    draft.race = chosen.race;
    draft.gender = member.gender;
    draft.character_class = chosen.character_class;
    draft.alignment = chosen.alignment;
    for (const auto field :
         {rules::CreationField::character_class, rules::CreationField::alignment})
    {
        const auto &id = field == rules::CreationField::character_class ? draft.character_class
                                                                        : draft.alignment;
        if (!creation.allows(draft, field, id))
        {
            throw std::runtime_error("these rules do not allow " + id);
        }
    }
    RandomState random{seed};
    creation.roll_in_place(draft, random);
    creation.set_rolled_scores(draft, chosen.scores);
    const auto id =
        party.add_pc(Character(creation, draft, look_under(member.appearance, chosen.race, true)));
    // What a created character joins with (CharacterCreator::starting_gold).
    const auto &numbers = *party.member(id).character.sheet().original;
    party.add_coins(Coin::gold, static_cast<std::uint32_t>(numbers.gold));
    party.add_coins(Coin::platinum, static_cast<std::uint32_t>(numbers.platinum));
    for (const auto &item : chosen.items)
    {
        party.purchase(id, original_item(item)); // It has no value: it costs nothing.
        if (item.readied)
        {
            (void)party.equip(id, party.member(id).character.inventory().items().back().id);
        }
    }
    return id;
}

// The SRD's member: a level-1 companion with its items, and the 3d6 x 10 gold
// pieces a created character brings.
MemberId add_srd_member(CampaignParty &party, const DefaultPartyMember &member)
{
    auto character =
        build_companion(member.srd.character, *party.character_rules(), party.rule_module());
    character.appearance(look_under(member.appearance, member.srd.character.race, false));
    std::vector<std::uint64_t> readied;
    for (const auto &item : member.srd.items)
    {
        const auto added = character.add_item({item.key, item.name, item.quantity});
        if (item.readied)
        {
            readied.push_back(added);
        }
    }
    const auto id = party.add_pc(std::move(character));
    for (const auto item : readied)
    {
        (void)party.equip(id, item);
    }
    auto state = party.checkpoint();
    std::uint32_t gold = 0;
    const RollPurpose purpose("starting gold");
    for (int die = 0; die < 3; ++die)
    {
        gold += static_cast<std::uint32_t>(roll_die(state.random_state, 6) * 10);
    }
    party.restore(std::move(state));
    party.add_coins(Coin::gold, gold);
    return id;
}

// The ids a rest's choice takes: a spell memorized again is its id with "#2",
// "#3" and so on after it ("cleric:cure_light_wounds#2").
[[nodiscard]] std::vector<std::string> numbered(const std::vector<std::string> &spells)
{
    std::vector<std::string> ids;
    for (std::size_t n = 0; n < spells.size(); ++n)
    {
        const auto copies =
            std::count(spells.begin(), spells.begin() + static_cast<std::ptrdiff_t>(n), spells[n]);
        ids.push_back(copies ? spells[n] + "#" + std::to_string(copies + 1) : spells[n]);
    }
    return ids;
}

// The original's casters memorize their spells at a Long Rest, as a new
// party does at its first camp.
void memorize(CampaignParty &party, std::span<const DefaultPartyMember> members,
              std::span<const MemberId> ids)
{
    const bool casters = std::ranges::any_of(members,
                                             [](const DefaultPartyMember &member)
                                             {
                                                 return !member.original.spells.empty();
                                             });
    if (!casters || !party.rest() || !party.state().spell_rest)
    {
        return;
    }
    for (std::size_t n = 0; n < members.size(); ++n)
    {
        const auto &waiting = party.state().spell_rest->members;
        if (std::ranges::find(waiting, ids[n]) == waiting.end())
        {
            continue;
        }
        if (members[n].original.spells.empty())
        {
            party.keep_rest_spells(ids[n]);
            continue;
        }
        party.choose_spells(ids[n], {{}, numbered(members[n].original.spells)});
    }
}
} // namespace

std::vector<DefaultPartyMember> load_default_party(const std::filesystem::path &file)
{
    const PartyReader read(file);
    const auto document = detail::read_json_file(file, "default party");
    const auto &format = read.field<std::string>(document, "format", "file");
    if (format != "opengold-default-party-v1")
    {
        read.reject("format", "must be opengold-default-party-v1");
    }
    std::vector<DefaultPartyMember> party;
    for (const auto &entry : read.field<Json::Array>(document, "members", "file"))
    {
        DefaultPartyMember member;
        member.name = read.text(entry, "name", "member " + std::to_string(party.size() + 1));
        const auto where = member.name;
        member.gender = read.text(entry, "gender", where);
        member.appearance = read.appearance(entry, where);
        member.original =
            read.original(read.object(entry, "original", where), where + ", original");
        member.srd = read.srd(read.object(entry, "srd", where), member.name, where + ", srd");
        member.srd.character.gender = member.gender;
        party.push_back(std::move(member));
    }
    if (party.size() != 6)
    {
        read.reject("members", "a default party has six members");
    }
    return party;
}

void add_default_party(CampaignParty &party, std::span<const DefaultPartyMember> members)
{
    const auto before = party.checkpoint();
    try
    {
        const bool original = rolls_in_place(*party.character_rules());
        std::vector<MemberId> ids;
        for (std::size_t n = 0; n < members.size(); ++n)
        {
            try
            {
                ids.push_back(original ? add_original_member(party, members[n], 1 + n)
                                       : add_srd_member(party, members[n]));
            }
            catch (const std::exception &failure)
            {
                // Says whom, for whoever edits the file.
                throw std::runtime_error(members[n].name + ": " + failure.what());
            }
        }
        if (original)
        {
            memorize(party, members, ids);
        }
    }
    catch (...)
    {
        party.restore(before);
        throw;
    }
}
} // namespace opengold
