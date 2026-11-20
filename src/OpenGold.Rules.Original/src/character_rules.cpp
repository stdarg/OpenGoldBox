#include "opengold/original.h"
#include "opengold/buck_rogers_characters.h"
#include "opengold/curse_records.h"
#include "opengold/secret_records.h"
#include "opengold/late_records.h"
#include "opengold/champions_records.h"
#include "opengold/death_knights_records.h"
#include "death_knights_rules.h"
#include "dark_queen_rules.h"
#include "opengold/dark_queen_records.h"
#include "opengold/pool_radiance.h"
#include "opengold/original_spells.h"
#include <algorithm>
#include <span>
#include <cctype>
#include <optional>
#include <sstream>
#include <stdexcept>

namespace opengold::original
{
namespace
{
using rules::Ability;
using rules::CharacterDraft;
using rules::CharacterSheet;
using rules::CreationChoice;
using rules::CreationField;

template <class Enum> struct Named
{
    Enum value;
    std::string_view id, label, description;
};

// The six races the original's creation offers; it skips the half-orc.
constexpr std::array<Named<Race>, 13> races{{
    {Race::dwarf, "dwarf", "Dwarf",
     "Stout, hardy folk who resist poison and magic and fight giants and goblins well. A dwarf "
     "may be a fighter, a thief or both."},
    {Race::elf, "elf", "Elf",
     "Graceful, long-lived folk who shrug off sleep and charm magic and are deadly with bows and "
     "swords. An elf may combine fighter, magic-user and thief."},
    {Race::gnome, "gnome", "Gnome",
     "Small, clever folk who resist magic and fight kobolds and goblins well. A gnome may be a "
     "fighter, a thief or both."},
    {Race::half_elf, "half_elf", "Half-Elf",
     "Folk of both human and elven blood, partly proof against sleep and charm magic, with the "
     "widest choice of classes."},
    {Race::halfling, "halfling", "Halfling",
     "Small, nimble folk who resist poison and magic. A halfling may be a fighter, a thief or "
     "both."},
    {Race::human, "human", "Human",
     "The most common folk of the Realms, who may follow any one class with no limit on their "
     "levels."},
    {Race::silvanesti_elf, "silvanesti_elf", "Silvanesti Elf", "An elf of Silvanesti."},
    {Race::qualinesti_elf, "qualinesti_elf", "Qualinesti Elf", "An elf of Qualinesti."},
    {Race::krynn_half_elf, "krynn_half_elf", "Half-Elf", "A half-elf of Krynn."},
    {Race::mountain_dwarf, "mountain_dwarf", "Mountain Dwarf", "A dwarf of the mountain clans."},
    {Race::hill_dwarf, "hill_dwarf", "Hill Dwarf", "A dwarf of the hill clans."},
    {Race::kender, "kender", "Kender", "Nimble, fearless folk whose taunts provoke enemies."},
    {Race::krynn_human, "krynn_human", "Human", "A human of Krynn."},
}};

constexpr std::array<Named<Gender>, 2> genders{{
    {Gender::male, "male", "Male", "A female's Strength may be lower than a male's."},
    {Gender::female, "female", "Female", "A female's Strength may be lower than a male's."},
}};

constexpr std::array<Named<Profession>, 16> professions{{
    {Profession::cleric, "cleric", "Cleric",
     "A priest who fights in armor and casts healing and protective spells. Hit Die: d8."},
    {Profession::fighter, "fighter", "Fighter",
     "A warrior who uses every weapon and armor and gains more attacks with experience. Hit Die: "
     "d10."},
    {Profession::magic_user, "magic_user", "Magic-User",
     "A wizard who learns powerful spells from a spellbook but fights poorly and wears no armor. "
     "Hit Die: d4."},
    {Profession::thief, "thief", "Thief",
     "A rogue who opens locks, finds traps and strikes from behind. Hit Die: d6."},
    {Profession::paladin, "paladin", "Paladin",
     "A lawful good warrior who heals by touch, cures disease and is shielded from evil. Hit Die: "
     "d10."},
    {Profession::ranger, "ranger", "Ranger",
     "A warrior of the wilds who strikes giant kind hard. Hit Dice: 2d8 at first level, then d8."},
    {Profession::cleric_fighter, "cleric_fighter", "Cleric/Fighter",
     "A cleric and a fighter at once, sharing experience between the two."},
    {Profession::cleric_fighter_magic_user, "cleric_fighter_magic_user",
     "Cleric/Fighter/Magic-User",
     "A cleric, fighter and magic-user at once, sharing experience "
     "among the three."},
    {Profession::cleric_ranger, "cleric_ranger", "Cleric/Ranger",
     "A cleric and a ranger at once, sharing experience between the two."},
    {Profession::cleric_magic_user, "cleric_magic_user", "Cleric/Magic-User",
     "A cleric and a magic-user at once, sharing experience between the two."},
    {Profession::cleric_thief, "cleric_thief", "Cleric/Thief",
     "A cleric and a thief at once, sharing experience between the two."},
    {Profession::fighter_magic_user, "fighter_magic_user", "Fighter/Magic-User",
     "A fighter and a magic-user at once, sharing experience between the two."},
    {Profession::fighter_thief, "fighter_thief", "Fighter/Thief",
     "A fighter and a thief at once, sharing experience between the two."},
    {Profession::fighter_magic_user_thief, "fighter_magic_user_thief", "Fighter/Magic-User/Thief",
     "A fighter, magic-user and thief at once, sharing experience among the three."},
    {Profession::magic_user_thief, "magic_user_thief", "Magic-User/Thief",
     "A magic-user and a thief at once, sharing experience between the two."},
    {Profession::knight, "knight", "Knight of Solamnia",
     "A lawful good warrior of the Crown, who may petition the Sword and Rose. First Hit Dice: 2d10."},
}};

constexpr std::array<Named<unsigned>, 7> deities{
    {{1, "paladine", "Paladine", "Good; grants Protection from Evil in a radius."},
     {2, "majere", "Majere", "Good; grants Silence."},
     {3, "kiri_jolith", "Kiri-Jolith", "Good; improves attacks and grants Detect Magic."},
     {4, "mishakal", "Mishakal", "Good; grants Remove Curse, Bless and Charm Person."},
     {5, "sirrion", "Sirrion", "Neutral; grants Burning Hands."},
     {6, "reorx", "Reorx", "Neutral; improves a dwarf's attacks."},
     {7, "shinare", "Shinare", "Neutral; grants Charm Person."}}};

bool dwarf_race(Race race)
{
    return race == Race::mountain_dwarf || race == Race::hill_dwarf;
}

// The SRD module's alignment ids, which the scripts' host already reads.
constexpr std::array<Named<Alignment>, 9> alignments{{
    {Alignment::lawful_good, "lawful_good", "Lawful Good", "Guided by compassion, duty and order."},
    {Alignment::lawful_neutral, "lawful_neutral", "Lawful Neutral",
     "Guided by rules, tradition or a personal code."},
    {Alignment::lawful_evil, "lawful_evil", "Lawful Evil",
     "Pursues selfish ends through structure and rules."},
    {Alignment::neutral_good, "neutral_good", "Neutral Good",
     "Seeks to help others, with flexibility about rules."},
    {Alignment::true_neutral, "neutral", "True Neutral",
     "Balances competing principles, or acts as circumstances demand."},
    {Alignment::neutral_evil, "neutral_evil", "Neutral Evil",
     "Pursues selfish ends without loyalty to order or freedom."},
    {Alignment::chaotic_good, "chaotic_good", "Chaotic Good",
     "Values kindness and individual freedom."},
    {Alignment::chaotic_neutral, "chaotic_neutral", "Chaotic Neutral",
     "Values personal freedom and independence."},
    {Alignment::chaotic_evil, "chaotic_evil", "Chaotic Evil",
     "Acts destructively, with little regard for rules or others."},
}};

template <class Enum, std::size_t N>
[[nodiscard]] std::optional<Enum> find_id(const std::array<Named<Enum>, N> &table,
                                          std::string_view id)
{
    const auto found = std::ranges::find(table, id, &Named<Enum>::id);
    return found == table.end() ? std::nullopt : std::optional<Enum>{found->value};
}

template <class Enum, std::size_t N>
[[nodiscard]] const Named<Enum> &named(const std::array<Named<Enum>, N> &table, Enum value)
{
    const auto found = std::ranges::find(table, value, &Named<Enum>::value);
    if (found == table.end())
    {
        throw std::invalid_argument("A choice the original rules do not offer");
    }
    return *found;
}

template <class Enum, std::size_t N>
[[nodiscard]] Enum require_id(const std::array<Named<Enum>, N> &table, std::string_view id,
                              std::string_view what)
{
    const auto found = find_id(table, id);
    if (!found)
    {
        throw std::invalid_argument("Unknown " + std::string(what) + ": " + std::string(id));
    }
    return *found;
}

template <class Enum, std::size_t N>
[[nodiscard]] std::vector<CreationChoice> listed(const std::array<Named<Enum>, N> &table)
{
    std::vector<CreationChoice> result;
    for (const auto &entry : table)
    {
        result.push_back(
            {std::string(entry.id), std::string(entry.label), std::string(entry.description)});
    }
    return result;
}

constexpr int rules_data_version = 1;

// The draft's rules data: a version, the six rolls from Strength to Charisma,
// the Strength percentile, the starting age, three Hit Dice and the gold, and
// in Curse the count of the starting training's Hit Dice and the dice.
[[nodiscard]] std::string encode(const RolledCharacter &rolled)
{
    std::ostringstream out;
    out << rules_data_version;
    for (const int roll : rolled.rolls)
    {
        out << ' ' << roll;
    }
    out << ' ' << rolled.strength_percentile << ' ' << rolled.starting_age;
    for (const int die : rolled.first_level_hit_dice)
    {
        out << ' ' << die;
    }
    out << ' ' << rolled.gold;
    if (!rolled.training_hit_dice.empty())
    {
        out << ' ' << rolled.training_hit_dice.size();
        for (const int die : rolled.training_hit_dice)
        {
            out << ' ' << die;
        }
    }
    return out.str();
}

// Modify's numbers, kept after the roll (or after an imported character's
// numbers): a marker, a version, the six scores from Strength to Charisma,
// exceptional Strength and the Hit Points. A draft never modified has none,
// so its rules data is what it was before Modify.
constexpr std::string_view modified_marker = " modified ";
constexpr int modified_data_version = 1;

// The rules data before Modify's numbers.
[[nodiscard]] std::string_view before_modified(std::string_view data)
{
    return data.substr(0, data.find(modified_marker));
}

[[nodiscard]] std::optional<ModifiedNumbers> modified_of(std::string_view data)
{
    const auto found = data.find(modified_marker);
    if (found == std::string_view::npos)
    {
        return std::nullopt;
    }
    std::istringstream in{std::string(data.substr(found + modified_marker.size()))};
    int version{};
    ModifiedNumbers numbers;
    in >> version;
    for (const auto ability : rules::all_abilities)
    {
        in >> numbers.scores[ability];
    }
    in >> numbers.exceptional_strength >> numbers.hit_points;
    std::string rest;
    if (!in || version != modified_data_version || (in >> rest) || numbers.hit_points < 1 ||
        numbers.exceptional_strength < 0 || numbers.exceptional_strength > 100)
    {
        throw std::invalid_argument("Unreadable modified character");
    }
    return numbers;
}

// The rules data with Modify's numbers in place of any before.
[[nodiscard]] std::string with_modified(std::string_view data, const ModifiedNumbers &numbers)
{
    std::ostringstream out;
    out << before_modified(data) << modified_marker << modified_data_version;
    for (const auto ability : rules::all_abilities)
    {
        out << ' ' << numbers.scores[ability];
    }
    out << ' ' << numbers.exceptional_strength << ' ' << numbers.hit_points;
    return out.str();
}

[[nodiscard]] RolledCharacter decode(std::string_view data)
{
    std::istringstream in{std::string(before_modified(data))};
    int version{};
    RolledCharacter rolled;
    in >> version;
    for (int &roll : rolled.rolls)
    {
        in >> roll;
    }
    in >> rolled.strength_percentile >> rolled.starting_age;
    for (int &die : rolled.first_level_hit_dice)
    {
        in >> die;
    }
    in >> rolled.gold;
    const bool read = !in.fail();
    // Curse's starting training's dice, when the draft has them.
    std::size_t training_dice = 0;
    bool dice_read = true;
    if (read && in >> training_dice && training_dice <= 64)
    {
        rolled.training_hit_dice.resize(training_dice);
        for (int &die : rolled.training_hit_dice)
        {
            in >> die;
        }
        dice_read = !in.fail();
    }
    else
    {
        dice_read = in.eof() && training_dice == 0;
    }
    std::string rest;
    if (!read || !dice_read || version != rules_data_version || (in >> rest) ||
        std::ranges::any_of(rolled.rolls,
                            [](int roll)
                            {
                                // Champions' +2 Dexterity plus the original
                                // creation's +1 can leave a raw roll of 21.
                                return roll < 2 || roll > 21;
                            }) ||
        rolled.strength_percentile < 1 || rolled.strength_percentile > 100 ||
        rolled.starting_age < 1 || rolled.gold < 0)
    {
        throw std::invalid_argument("Unreadable original-rules creation rolls");
    }
    return rolled;
}

// A draft's choices, read and checked.
struct Choices
{
    Race race;
    Gender gender;
    Profession profession;
};

[[nodiscard]] Choices read_choices(const CharacterDraft &draft, GoldBoxGame game)
{
    const Choices choices{require_id(races, draft.race, "race"),
                          require_id(genders, draft.gender, "gender"),
                          require_id(professions, draft.character_class, "class")};
    const auto offered = professions_for(choices.race, game);
    if (std::ranges::find(offered, choices.profession) == offered.end())
    {
        throw std::invalid_argument("The original rules do not offer this race that class");
    }
    return choices;
}

// A creature record of any game, told by its size: Pool of Radiance's 285
// bytes, Curse's 422, Secret's 439 or the 256-color engine's 510, read as the
// game's whose rules play it (Pools of Darkness's or Treasures').
std::optional<por::CharacterRecord> decode_record(std::span<const std::uint8_t> bytes,
                                                  GoldBoxGame game)
{
    if (game == GoldBoxGame::dark_queen_of_krynn)
    {
        return dark_queen::decode_character(bytes);
    }
    if (game == GoldBoxGame::death_knights_of_krynn && (bytes.size() == 216 || bytes.size() == 436))
    {
        auto record = death_knights::decode_creature(bytes.first(216));
        if (record && bytes.size() == 436 &&
            !death_knights::attach_spells(*record, bytes.subspan(216)))
        {
            return std::nullopt;
        }
        return record;
    }
    switch (bytes.size())
    {
    case 409:
        return champions::decode_creature(bytes);
    case 422:
        return curse::decode_creature(bytes);
    case 439:
        return secret::decode_creature(bytes);
    case 510:
        return late::decode_creature(bytes,
                                     vga_engine(game) ? game : GoldBoxGame::pools_of_darkness);
    default:
        return por::decode_character(bytes);
    }
}

constexpr std::string_view record_prefix = "record:";
constexpr std::string_view imported_prefix = "imported:";

// A character brought from another game (Curse's ConvertPoolRadPlayer): its
// scores, levels, Hit Points, age, experience, drained levels and the
// magic-user spells it learned, as the draft keeps them; and the Hit Points it
// rolled where the game keeps them (Pools of Darkness's LoadChar), else
// nothing (version 1 drafts have none).
struct Imported
{
    rules::AbilityArray<int> scores{};
    int exceptional_strength{};
    ClassLevels levels{};
    int hit_points{}, age{}, experience{}, drained_levels{}, drained_hit_points{};
    int race{}, profession{}, alignment{};
    int deity{}, robe{}, knight{};
    std::vector<int> learned;
    std::optional<int> rolled_hit_points;
    std::optional<std::vector<std::string>> prepared;
};

std::string encode_imported(const Imported &from)
{
    std::ostringstream out;
    const bool krynn = from.deity || from.robe || from.knight || from.prepared.has_value();
    out << imported_prefix << (from.prepared ? 4 : krynn ? 3 : from.rolled_hit_points ? 2 : 1);
    for (const auto ability : rules::all_abilities)
    {
        out << ' ' << from.scores[ability];
    }
    out << ' ' << from.exceptional_strength;
    for (const int level : from.levels)
    {
        out << ' ' << level;
    }
    out << ' ' << from.hit_points << ' ' << from.age << ' ' << from.experience << ' '
        << from.drained_levels << ' ' << from.drained_hit_points << ' ' << from.race << ' '
        << from.profession << ' ' << from.alignment << ' ' << from.learned.size();
    for (const int number : from.learned)
    {
        out << ' ' << number;
    }
    if (from.rolled_hit_points || krynn)
    {
        out << ' ' << from.rolled_hit_points.value_or(0);
    }
    if (krynn)
    {
        out << ' ' << from.deity << ' ' << from.robe << ' ' << from.knight;
    }
    if (from.prepared)
    {
        out << ' ' << from.prepared->size();
        for (const auto &id : *from.prepared)
        {
            out << ' ' << id;
        }
    }
    return out.str();
}

std::optional<Imported> imported_of(const CharacterDraft &draft)
{
    std::string_view data = draft.rules_data;
    if (!data.starts_with(imported_prefix))
    {
        return std::nullopt;
    }
    std::istringstream in{std::string(before_modified(data).substr(imported_prefix.size()))};
    int version{};
    Imported from;
    in >> version;
    for (const auto ability : rules::all_abilities)
    {
        in >> from.scores[ability];
    }
    in >> from.exceptional_strength;
    for (int &level : from.levels)
    {
        in >> level;
    }
    std::size_t learned = 0;
    in >> from.hit_points >> from.age >> from.experience >> from.drained_levels >>
        from.drained_hit_points >> from.race >> from.profession >> from.alignment >> learned;
    from.learned.resize(std::min<std::size_t>(learned, 128));
    for (int &number : from.learned)
    {
        in >> number;
    }
    if (version >= 2)
    {
        int rolled = 0;
        in >> rolled;
        from.rolled_hit_points = rolled;
    }
    if (version >= 3)
    {
        in >> from.deity >> from.robe >> from.knight;
    }
    if (version == 4)
    {
        std::size_t count = 0;
        in >> count;
        if (count > 74)
        {
            throw std::invalid_argument("Too many imported prepared spells");
        }
        from.prepared = std::vector<std::string>(count);
        for (auto &id : *from.prepared)
        {
            in >> id;
            if (id.empty() || id.size() > 128)
            {
                throw std::invalid_argument("Invalid imported prepared spell");
            }
        }
    }
    std::string rest;
    if (!in || version < 1 || version > 4 || (in >> rest) || from.hit_points < 1 ||
        from.deity < 0 || from.deity > 7 || from.robe < 0 || from.robe > 3 || from.knight < 0 ||
        from.knight > 3)
    {
        throw std::invalid_argument("Unreadable imported character");
    }
    return from;
}

constexpr std::string_view hex = "0123456789abcdef";

// A record's bytes as a draft keeps them.
std::string record_data(std::span<const std::uint8_t> bytes)
{
    std::string data(record_prefix);
    for (const auto byte : bytes)
    {
        data += hex[byte >> 4];
        data += hex[byte & 0xF];
    }
    return data;
}

// The record a draft holds, if it is an NPC's.
std::optional<por::CharacterRecord> record_of(const CharacterDraft &draft, GoldBoxGame game)
{
    std::string_view data = draft.rules_data;
    if (!data.starts_with(record_prefix))
    {
        return std::nullopt;
    }
    data.remove_prefix(record_prefix.size());
    std::vector<std::uint8_t> bytes;
    for (std::size_t n = 0; n + 1 < data.size(); n += 2)
    {
        bytes.push_back(static_cast<std::uint8_t>(hex.find(data[n]) * 16 + hex.find(data[n + 1])));
    }
    auto record = decode_record(bytes, game);
    if (!record)
    {
        throw std::invalid_argument("Unreadable original record");
    }
    return record;
}

// "DIRTEN" as a sheet names it: "Dirten".
std::string title_case(std::string_view name)
{
    std::string titled;
    bool start = true;
    for (const char c : name)
    {
        const auto byte = static_cast<unsigned char>(c);
        titled += static_cast<char>(start ? std::toupper(byte) : std::tolower(byte));
        start = c == ' ';
    }
    return titled;
}

[[nodiscard]] ClassLevels record_levels(const por::CharacterRecord &record)
{
    const auto &l = record.levels;
    return {l.cleric, l.druid, l.fighter, l.paladin, l.ranger, l.mage, l.thief, l.monk};
}

// The profession whose classes are the record's: a fighter's for one with
// none the original's professions name.
[[nodiscard]] Profession record_profession(const por::CharacterRecord &record)
{
    const auto levels = record_levels(record);
    for (const auto &entry : professions)
    {
        const auto classes = classes_of(entry.value);
        bool same = true;
        for (std::size_t at = 0; at < class_slot_count; ++at)
        {
            const bool held = levels[at] > 0;
            same = same && held == (std::ranges::find(classes, static_cast<ClassSlot>(at)) !=
                                    classes.end());
        }
        if (same)
        {
            return entry.value;
        }
    }
    return Profession::fighter;
}

[[nodiscard]] rules::AbilityArray<int> record_scores(const por::CharacterRecord &record)
{
    const auto &a = record.current_abilities;
    return {a.strength, a.dexterity, a.constitution, a.intelligence, a.wisdom, a.charisma};
}

// Treasures' creation: the magic-user's chosen spells of levels 1 to 3, in
// training choice groups named by this prefix and the level.
constexpr int chosen_spell_levels = 3;
constexpr std::string_view spell_group_prefix = "treasures_spells:";
constexpr std::string_view read_magic_id = "magic_user:read_magic";

// The spells a character starts with: those its training taught, but where
// Treasures' creation offers a choice (`groups`), Read Magic and the spells
// chosen in each group in place of the taught spells of levels 1 to 3. Until
// the choice is made (a sheet shown before its page) only Read Magic.
[[nodiscard]] std::vector<int> chosen_spells(const CharacterDraft &draft, std::vector<int> taught,
                                             std::span<const rules::TrainingChoiceGroup> groups)
{
    if (groups.empty())
    {
        return taught;
    }
    std::erase_if(taught,
                  [](int number)
                  {
                      const auto *known = spell(number);
                      return known && known->caster_class == SpellClass::magic_user &&
                             known->level <= chosen_spell_levels;
                  });
    taught.push_back(*spell_number(read_magic_id));
    for (const auto &group : groups)
    {
        const auto found = draft.training.find(group.id);
        if (found == draft.training.end())
        {
            continue;
        }
        for (std::size_t n = 0; n < found->second.size() && n < group.count; ++n)
        {
            const auto &id = found->second[n];
            const bool offered =
                std::ranges::find(group.options, id, &CreationChoice::id) != group.options.end();
            if (const auto number = spell_number(id); offered && number)
            {
                taught.push_back(*number);
            }
        }
    }
    return taught;
}

class OriginalCharacterRules final : public rules::CharacterRules
{
  public:
    OriginalCharacterRules(Aging aging, GoldBoxGame game, std::optional<StartingStock> start = {})
        : aging_(aging), game_(game), start_(start)
    {
    }

    rules::Identity identity() const override
    {
        return original::identity(game_);
    }

    std::string game_setting() const override
    {
        return std::string(aging_name(aging_));
    }

    // An NPC who joins: its record, whole, in the draft (record_data), so the
    // character is the record's scores, levels and Hit Points.
    std::optional<CharacterDraft> record_draft(std::span<const std::uint8_t> bytes) const override
    {
        // A Buck Rogers creature joins as its record (buck_rogers_characters.h).
        if (is_buck_rogers(game_))
        {
            auto creature = buck_rogers::decode_creature(bytes);
            if (!creature)
            {
                throw std::invalid_argument("Unreadable original record");
            }
            return buck_rogers_draft({std::move(*creature), {}, {}});
        }
        const auto record = decode_record(bytes, game_);
        if (!record)
        {
            throw std::invalid_argument("Unreadable original record");
        }
        CharacterDraft draft;
        draft.name = title_case(record->name);
        draft.race = std::string(named(races, race_from_record(record->race, record->game)).id);
        draft.gender = record->gender ? "female" : "male";
        draft.character_class = std::string(named(professions, record_profession(*record)).id);
        draft.alignment = std::string(
            named(alignments, static_cast<Alignment>(std::min<int>(record->alignment, 8))).id);
        draft.rules_data = record_data(bytes);
        draft.rolled = true;
        return draft;
    }

    // A character brought from the game before (Curse's ConvertPoolRadPlayer):
    // its scores kept within this game's limits for its race and gender, its
    // levels, Hit Points, age, experience and drained levels, and the
    // magic-user spells it learned but Animate Dead; it starts with this
    // game's platinum (Curse's 300). Pools of Darkness's LoadChar takes a
    // Secret character's record nearly whole, its rolled Hit Points too, but
    // not the levels a drain took, which its record has no place for.
    std::optional<CharacterDraft> imported_draft(const CharacterSheet &from,
                                                 unsigned experience) const override
    {
        if (!from.original)
        {
            return std::nullopt;
        }
        const auto &o = *from.original;
        const auto race = static_cast<Race>(o.race);
        const auto gender = from.gender == "Female" ? Gender::female : Gender::male;
        const auto limits = ability_limits(race, gender, game_);
        Imported in;
        for (const auto ability : rules::all_abilities)
        {
            in.scores[ability] = (game_ == GoldBoxGame::death_knights_of_krynn ||
                                  game_ == GoldBoxGame::dark_queen_of_krynn)
                                     ? from.scores[ability]
                                     : std::clamp(from.scores[ability], limits.minimum[ability],
                                                  limits.maximum[ability]);
        }
        in.exceptional_strength =
            in.scores[Ability::strength] == 18
                ? std::min(o.exceptional_strength, limits.exceptional_strength_maximum)
                : 0;
        in.levels = o.class_levels;
        in.hit_points = std::max(from.hit_points, 1);
        in.age = o.age;
        if (game_ == GoldBoxGame::death_knights_of_krynn)
        {
            switch (race)
            {
            case Race::qualinesti_elf:
            case Race::silvanesti_elf:
                in.age = std::min(in.age, 1200);
                break;
            case Race::krynn_half_elf:
                in.age = std::min(in.age, 200);
                break;
            case Race::mountain_dwarf:
            case Race::hill_dwarf:
                in.age = std::min(in.age, 300);
                break;
            case Race::kender:
                in.age = in.age > 199 ? 70 : in.age;
                break;
            case Race::krynn_human:
                in.age = std::min(in.age, 50);
                break;
            default:
                break;
            }
        }
        in.experience = static_cast<int>(std::min<unsigned>(experience, 100000000));
        const bool darkness = game_ == GoldBoxGame::pools_of_darkness;
        in.drained_levels = darkness ? 0 : o.drained_levels;
        in.drained_hit_points = darkness ? 0 : o.drained_hit_points;
        in.race = o.race;
        in.profession = o.profession;
        in.alignment = o.alignment;
        in.deity = o.deity;
        in.robe = o.robe_order;
        in.knight = o.knight_order;
        if (in.deity || in.robe || in.knight)
        {
            in.rolled_hit_points = o.rolled_hit_points;
        }
        if (darkness || game_ == GoldBoxGame::death_knights_of_krynn ||
            game_ == GoldBoxGame::dark_queen_of_krynn)
        {
            in.rolled_hit_points = o.rolled_hit_points;
        }
        if (game_ == GoldBoxGame::death_knights_of_krynn)
        {
            in.prepared = from.prepared_spells;
        }
        if (game_ == GoldBoxGame::dark_queen_of_krynn)
        {
            in.prepared = std::vector<std::string>{};
            for (const auto &id : from.prepared_spells)
            {
                if (const auto number = spell_number(id, game_of(from.identity)))
                {
                    const auto mapped = dark_queen_import_spell(*number);
                    const auto suffix = id.find('#');
                    auto converted = spell_id(mapped);
                    if (!converted.empty())
                    {
                        if (suffix != std::string::npos)
                        {
                            converted += id.substr(suffix);
                        }
                        in.prepared->push_back(std::move(converted));
                    }
                }
            }
            const ModifiedCharacter character{race,
                                              gender,
                                              static_cast<Profession>(o.profession),
                                              in.levels,
                                              {in.deity, in.robe, in.knight}};
            const auto cap =
                dark_queen_hit_point_maximum(character, in.scores[Ability::constitution]);
            in.hit_points = std::min(in.hit_points, cap);
            in.rolled_hit_points = std::min(in.rolled_hit_points.value_or(in.hit_points), cap);
        }
        constexpr int animate_dead = 36;
        for (const int previous_number : o.known_spells)
        {
            const int number = game_ == GoldBoxGame::dark_queen_of_krynn
                                   ? dark_queen_import_spell(previous_number)
                                   : previous_number;
            const auto *known = spell(number, game_);
            if (known && known->caster_class == SpellClass::magic_user &&
                (game_ == GoldBoxGame::death_knights_of_krynn ||
                 game_ == GoldBoxGame::dark_queen_of_krynn || number != animate_dead))
            {
                in.learned.push_back(number);
            }
        }
        CharacterDraft draft;
        draft.name = from.name;
        draft.race = std::string(named(races, race).id);
        draft.gender = std::string(named(genders, gender).id);
        draft.character_class =
            std::string(named(professions, static_cast<Profession>(o.profession)).id);
        draft.alignment = std::string(named(alignments, static_cast<Alignment>(o.alignment)).id);
        draft.rules_data = encode_imported(in);
        draft.rolled = true;
        return draft;
    }

    // Modify Character: a character its game's creation made, or one brought
    // from the game before, while its experience is what its game's creation
    // gives (modify_allowed); an NPC's record never.
    bool modifiable(const CharacterDraft &draft, unsigned experience) const override
    {
        // A Buck Rogers character's Modify is its creation screen's
        // (buck_rogers_rules.h), not this one's.
        if (!draft.rolled || draft.rules_data.starts_with(record_prefix) ||
            holds_buck_rogers_record(draft.rules_data))
        {
            return false;
        }
        // An Unlimited Adventures design's start, or the kit's own.
        const int design_start =
            start_ ? start_->experience : starting_experience(Profession::fighter, game_);
        return modify_allowed(static_cast<int>(std::min<unsigned>(experience, 100000000)), game_,
                              design_start);
    }

    CharacterDraft modified(const CharacterDraft &draft, rules::ModifyRow row,
                            rules::ModifyStep step) const override
    {
        if (draft.rules_data.starts_with(record_prefix))
        {
            throw std::invalid_argument("An NPC cannot be modified");
        }
        const auto sheet = evaluate(draft, rules::NameRequirement::optional);
        const auto &numbers = *sheet.original;
        const ModifiedNumbers before{sheet.scores, numbers.exceptional_strength, sheet.hit_points};
        const auto after = modify_step(before, modified_character(draft, sheet), row, step, game_);
        auto changed = draft;
        if (after != before)
        {
            changed.rules_data = with_modified(draft.rules_data, after);
        }
        return changed;
    }

    // Pool of Radiance's Modify changes only the numbers; from Curse on it
    // changes the name too.
    bool modify_renames() const override
    {
        return game_ != GoldBoxGame::pool_of_radiance;
    }

    // The kit's roll offers Modify beside Reroll (create.c); the Pascal
    // games' Modify is on their party menu only.
    bool modify_while_made() const override
    {
        return game_ == GoldBoxGame::unlimited_adventures;
    }

    std::vector<CreationChoice> choices(CreationField field) const override
    {
        switch (field)
        {
        case CreationField::race:
        {
            // The races that may take some class in this game.
            auto offered = listed(races);
            std::erase_if(offered,
                          [this](const CreationChoice &choice)
                          {
                              return professions_for(*find_id(races, choice.id), game_).empty();
                          });
            return offered;
        }
        case CreationField::gender:
            return listed(genders);
        case CreationField::character_class:
        {
            // The classes some race may take in this game.
            auto offered = listed(professions);
            std::erase_if(offered,
                          [this](const CreationChoice &choice)
                          {
                              const auto profession = *find_id(professions, choice.id);
                              return std::ranges::none_of(
                                  races,
                                  [&](const auto &race)
                                  {
                                      const auto list = professions_for(race.value, game_);
                                      return std::ranges::find(list, profession) != list.end();
                                  });
                          });
            return offered;
        }
        case CreationField::alignment:
        {
            auto choices = listed(alignments);
            if (krynn_game(game_))
            {
                std::erase_if(
                    choices,
                    [](const auto &choice)
                    {
                        return static_cast<unsigned>(*find_id(alignments, choice.id)) % 3 == 2;
                    });
            }
            return choices;
        }
        case CreationField::background:
            return krynn_game(game_) ? listed(deities) : std::vector<CreationChoice>{};
        }
        throw std::invalid_argument("Unknown creation field");
    }

    // The original has no backgrounds: one adjustment, which adds nothing.
    std::vector<rules::ScoreAdjustment> adjustments(std::string_view) const override
    {
        return {rules::ScoreAdjustment{}};
    }

    std::array<rules::AbilityRoll, 6> roll(RandomState &) const override
    {
        throw std::logic_error("The original rules roll each ability in place");
    }

    rules::AbilityGeneration ability_generation() const override
    {
        return rules::AbilityGeneration::rolled_in_place;
    }

    bool choices_require_reroll(const CharacterDraft &before,
                                const CharacterDraft &after) const override
    {
        return CharacterRules::choices_require_reroll(before, after) ||
               (krynn_game(game_) &&
                (before.background != after.background || before.alignment != after.alignment));
    }

    void roll_in_place(CharacterDraft &draft, RandomState &random) const override
    {
        const auto chosen = read_choices(draft, game_);
        draft.rules_data = encode(roll_character(
            chosen.race, chosen.profession, game_,
            experience_of(chosen.profession, draft.alignment), random,
            {static_cast<int>(find_id(deities, draft.background).value_or(0)), 0, 0}));
        draft.rolled = true;
    }

    void set_rolled_scores(CharacterDraft &draft,
                           const rules::AbilityArray<int> &rolls) const override
    {
        auto rolled = decode(draft.rules_data);
        rolled.rolls = rolls;
        draft.rules_data = encode(rolled);
    }

    bool allows(const CharacterDraft &draft, CreationField field,
                std::string_view id) const override
    {
        switch (field)
        {
        case CreationField::race:
        {
            const auto race = find_id(races, id);
            return race && !professions_for(*race, game_).empty();
        }
        case CreationField::gender:
            return find_id(genders, id).has_value();
        case CreationField::character_class:
        {
            const auto race = find_id(races, draft.race);
            const auto profession = find_id(professions, id);
            if (!race || !profession)
            {
                return false;
            }
            const auto offered = professions_for(*race, game_);
            return std::ranges::find(offered, *profession) != offered.end();
        }
        case CreationField::alignment:
        {
            const auto profession = find_id(professions, draft.character_class);
            const auto alignment = find_id(alignments, id);
            if (!profession || !alignment)
            {
                return false;
            }
            if (game_ == GoldBoxGame::dark_queen_of_krynn &&
                !dark_queen_alignment_allowed(*profession, *alignment))
            {
                return false;
            }
            if (game_ == GoldBoxGame::death_knights_of_krynn &&
                !death_knights_alignment_allowed(*profession, *alignment))
            {
                return false;
            }
            if (krynn_game(game_))
            {
                const auto number = static_cast<unsigned>(*alignment);
                if (number % 3 == 2)
                {
                    return false;
                }
                if (*profession == Profession::knight)
                {
                    return number == 0;
                }
                const bool thief = std::ranges::find(classes_of(*profession), ClassSlot::thief) !=
                                   classes_of(*profession).end();
                if (game_ == GoldBoxGame::champions_of_krynn && thief && number != 1 &&
                    number != 3 && number != 4)
                {
                    return false;
                }
                const auto deity = find_id(deities, draft.background);
                if (deity &&
                    ((*deity <= 4 && number % 3 != 0) ||
                     (*deity > 4 && number != 1 && number != 3 && number != 4 && number != 7)))
                {
                    return false;
                }
            }
            if (game_ == GoldBoxGame::death_knights_of_krynn ||
                game_ == GoldBoxGame::dark_queen_of_krynn)
            {
                return true;
            }
            const auto open = alignments_for(*profession);
            return std::ranges::find(open, *alignment) != open.end();
        }
        case CreationField::background:
        {
            if (!krynn_game(game_))
            {
                return false;
            }
            const auto profession = find_id(professions, draft.character_class);
            const auto deity = find_id(deities, id);
            const auto race = find_id(races, draft.race);
            if (!profession || !deity || !race)
            {
                return false;
            }
            const auto classes = classes_of(*profession);
            return std::ranges::find(classes, ClassSlot::cleric) != classes.end() &&
                   (!dwarf_race(*race) || *deity == 6);
        }
        }
        return false;
    }

    std::optional<int> ability_score(const CharacterDraft &draft, Ability ability) const override
    {
        if (const auto record = record_of(draft, game_))
        {
            return record_scores(*record)[ability];
        }
        if (const auto imported = imported_of(draft))
        {
            return modified_imported(*imported, draft).scores[ability];
        }
        if (!draft.rolled)
        {
            return std::nullopt;
        }
        return scores(draft)[ability];
    }

    std::array<Ability, 6> preset_ability_priority(std::string_view, unsigned) const override
    {
        return {Ability::strength,     Ability::dexterity, Ability::constitution,
                Ability::intelligence, Ability::wisdom,    Ability::charisma};
    }

    // Creation raises each score to the class's minimums, so no class waits on
    // the rolls.
    rules::ClassRequirements class_requirements(std::string_view) const override
    {
        return {{}, false, 0, {}};
    }

    CharacterSheet evaluate(const CharacterDraft &draft, rules::NameRequirement name) const override
    {
        if (name == rules::NameRequirement::required && draft.name.empty())
        {
            throw std::invalid_argument("A character needs a name");
        }
        if (const auto record = buck_rogers_record_of(draft.rules_data))
        {
            return buck_rogers_sheet(draft, *record, identity());
        }
        if (const auto record = record_of(draft, game_))
        {
            return record_sheet(draft, *record);
        }
        if (const auto imported = imported_of(draft))
        {
            auto sheet = imported_sheet(draft, modified_imported(*imported, draft));
            if (modified_of(draft.rules_data))
            {
                auto &numbers = *sheet.original;
                numbers.rolled_hit_points = modified_rolled_hit_points(
                    modified_character(draft, sheet), sheet.scores[Ability::constitution],
                    sheet.hit_points, numbers.rolled_hit_points, game_);
            }
            return sheet;
        }
        const auto chosen = read_choices(draft, game_);
        const auto alignment = require_id(alignments, draft.alignment, "alignment");
        if (!allows(draft, CreationField::alignment, draft.alignment))
        {
            throw std::invalid_argument("The class does not allow that alignment");
        }
        if (!draft.rolled)
        {
            throw std::invalid_argument("The character's abilities are not rolled");
        }
        const auto rolled = decode(draft.rules_data);
        const auto rolled_ability = ability_scores(chosen.race, chosen.gender, chosen.profession,
                                                   rolled, aging_, rolled.starting_age, game_);
        // Modify changes the scores and Hit Points after the original's
        // creation trained the character, so its levels stay the roll's.
        const auto modified = modified_of(draft.rules_data);
        const auto ability = modified ? modified->scores : rolled_ability;
        CharacterSheet sheet;
        sheet.identity = identity();
        sheet.name = draft.name;
        sheet.race = std::string(named(races, chosen.race).label);
        sheet.gender = std::string(named(genders, chosen.gender).label);
        sheet.character_class = std::string(named(professions, chosen.profession).label);
        sheet.alignment = std::string(named(alignments, alignment).label);
        sheet.base = ability;
        sheet.scores = ability;
        // Curse's character starts trained to its starting experience.
        const auto trained =
            starting_training(chosen.race, chosen.profession, rolled_ability, rolled, game_,
                              experience_of(chosen.profession, draft.alignment),
                              {static_cast<int>(find_id(deities, draft.background).value_or(0)),
                               static_cast<int>(alignment) % 3 == 0 ? 1 : 2,
                               chosen.profession == Profession::knight ? 1 : 0});
        const auto &levels = trained.levels;
        sheet.level = *std::max_element(levels.begin(), levels.end());
        sheet.hit_points =
            modified
                ? modified->hit_points
                : first_level_hit_points(
                      chosen.profession, rolled, ability[Ability::constitution], game_,
                      {static_cast<int>(find_id(deities, draft.background).value_or(0)), 0, 0}) +
                      trained.hit_points;
        for (const auto slot : classes_of(chosen.profession))
        {
            sheet.level_classes.emplace_back(class_slot_id(slot, game_));
        }
        CharacterSheet::Original original;
        original.class_levels = levels;
        original.thac0 = thac0(levels, game_);
        original.saving_throws = saving_throws(levels, game_);
        original.experience = experience_of(chosen.profession, draft.alignment);
        original.platinum = platinum();
        original.exceptional_strength =
            modified ? modified->exceptional_strength
                     : exceptional_strength(chosen.race, chosen.gender, chosen.profession, rolled,
                                            ability[Ability::strength], game_);
        original.rolled_hit_points =
            first_level_rolled_hit_points(chosen.profession, rolled) + trained.rolled_hit_points;
        if (modified)
        {
            original.rolled_hit_points = modified_rolled_hit_points(
                {chosen.race,
                 chosen.gender,
                 chosen.profession,
                 levels,
                 {static_cast<int>(find_id(deities, draft.background).value_or(0)), 0, 0}},
                ability[Ability::constitution], modified->hit_points, original.rolled_hit_points,
                game_);
        }
        original.age = rolled.starting_age;
        original.aging_changes_scores = aging_ == Aging::add;
        original.gold = rolled.gold;
        if (game_ == GoldBoxGame::champions_of_krynn)
        {
            original.platinum = rolled.gold;
            original.gold = 0;
        }
        original.race = static_cast<int>(chosen.race);
        original.profession = static_cast<int>(chosen.profession);
        original.alignment = static_cast<int>(alignment);
        if (krynn_game(game_))
        {
            if (level_in(levels, ClassSlot::cleric))
            {
                if (!allows(draft, CreationField::background, draft.background))
                {
                    throw std::invalid_argument("A cleric must choose an eligible deity");
                }
                original.deity = static_cast<int>(*find_id(deities, draft.background));
            }
            original.robe_order = level_in(levels, ClassSlot::magic_user)
                                      ? (static_cast<int>(alignment) % 3 == 0 ? 1 : 2)
                                      : 0;
            original.knight_order = chosen.profession == Profession::knight ? 1 : 0;
            sheet.background =
                original.deity
                    ? std::string(named(deities, static_cast<unsigned>(original.deity)).label)
                    : "";
        }
        original.thief_skills = thief_skills(chosen.race, level_in(levels, ClassSlot::thief),
                                             ability[Ability::dexterity], game_);
        const auto slots =
            caster_slots(levels, ability[Ability::wisdom], ability[Ability::intelligence], game_);
        original.cleric_slots = slots.cleric;
        original.druid_slots = slots.druid;
        original.magic_user_slots = slots.magic_user;
        original.known_spells = known_spells(
            levels, ability[Ability::wisdom],
            chosen_spells(draft, trained.learned_spells, training_options(draft)), game_);
        if (krynn_game(game_))
        {
            original.known_spells = level_in(levels, ClassSlot::magic_user)
                                        ? std::vector<int>{11, 18, 12, 15, 21}
                                        : std::vector<int>{};
            if (game_ == GoldBoxGame::dark_queen_of_krynn)
            {
                recalculate_dark_queen_numbers(original, ability);
                original.known_spells =
                    dark_queen_starting_spells(original.robe_order, original.magic_user_slots);
                recalculate_dark_queen_numbers(original, ability);
            }
            else if (game_ == GoldBoxGame::death_knights_of_krynn)
            {
                original.known_spells = death_knights_starting_spells(
                    original.robe_order, level_in(levels, ClassSlot::magic_user));
                recalculate_death_knights_numbers(original, ability);
            }
            else
            {
                recalculate_champions_numbers(original, ability);
            }
        }
        sheet.original = original;
        return sheet;
    }

    // The original has no skills or cantrips at creation, and most games no
    // spell choices: a cleric knows every first-level spell, and a
    // magic-user starts with the spells its game teaches. Treasures' magic-
    // user chooses its first- to third-level spells, a third more of each
    // level than its slots (its chargen.pas), and knows Read Magic besides.
    std::vector<rules::TrainingChoiceGroup>
    training_options(const CharacterDraft &draft) const override
    {
        std::vector<rules::TrainingChoiceGroup> groups;
        if (game_ != GoldBoxGame::treasures_of_the_savage_frontier || !draft.rolled)
        {
            return groups;
        }
        const auto chosen = read_choices(draft, game_);
        const auto classes = classes_of(chosen.profession);
        if (std::ranges::find(classes, ClassSlot::magic_user) == classes.end())
        {
            return groups;
        }
        const auto rolled = decode(draft.rules_data);
        const auto ability = ability_scores(chosen.race, chosen.gender, chosen.profession, rolled,
                                            aging_, rolled.starting_age, game_);
        const auto trained =
            starting_training(chosen.race, chosen.profession, ability, rolled, game_);
        const auto now = scores(draft);
        const auto slots =
            spell_slots(ClassSlot::magic_user, level_in(trained.levels, ClassSlot::magic_user),
                        now[Ability::wisdom], now[Ability::intelligence], game_);
        for (int level = 1; level <= chosen_spell_levels; ++level)
        {
            const int slot_count = slots[static_cast<std::size_t>(level - 1)];
            if (slot_count <= 0)
            {
                continue;
            }
            rules::TrainingChoiceGroup group;
            group.id = std::string(spell_group_prefix) + std::to_string(level);
            group.label = "Magic-user spells of level " + std::to_string(level);
            for (const int number : castable_spells(game_))
            {
                const auto *known = spell(number);
                if (known->caster_class == SpellClass::magic_user && known->level == level &&
                    spell_id(number) != read_magic_id)
                {
                    group.options.push_back({spell_id(number), std::string(known->name), {}});
                }
            }
            group.count = std::min(static_cast<unsigned>(slot_count * 4 / 3),
                                   static_cast<unsigned>(group.options.size()));
            groups.push_back(std::move(group));
        }
        return groups;
    }

    rules::SpellChoiceOptions spell_choice_options(const CharacterDraft &) const override
    {
        return {};
    }

    rules::TrainingChoiceGroup cantrip_options(const CharacterDraft &) const override
    {
        return {};
    }

    rules::AbilityCheckModifier ability_check(const CharacterSheet &, Ability,
                                              std::string_view) const override
    {
        return {};
    }

  private:
    // The character Modify changes, by its sheet.
    [[nodiscard]] static ModifiedCharacter modified_character(const CharacterDraft &draft,
                                                              const CharacterSheet &sheet)
    {
        const auto &numbers = *sheet.original;
        return {static_cast<Race>(numbers.race),
                draft.gender == "female" ? Gender::female : Gender::male,
                static_cast<Profession>(numbers.profession),
                numbers.class_levels,
                {numbers.deity, numbers.robe_order, numbers.knight_order}};
    }

    // A character brought from another game with Modify's numbers, when the
    // player kept some.
    [[nodiscard]] static Imported modified_imported(Imported from, const CharacterDraft &draft)
    {
        if (const auto modified = modified_of(draft.rules_data))
        {
            from.scores = modified->scores;
            from.exceptional_strength = modified->exceptional_strength;
            from.hit_points = modified->hit_points;
        }
        return from;
    }

    // A character brought from another game, as its draft keeps it.
    [[nodiscard]] CharacterSheet imported_sheet(const CharacterDraft &draft,
                                                const Imported &from) const
    {
        const auto race = static_cast<Race>(from.race);
        const auto profession = static_cast<Profession>(from.profession);
        CharacterSheet sheet;
        sheet.identity = identity();
        sheet.name = draft.name;
        sheet.race = std::string(named(races, race).label);
        sheet.gender = draft.gender == "female" ? "Female" : "Male";
        sheet.character_class = std::string(named(professions, profession).label);
        sheet.alignment =
            std::string(named(alignments, static_cast<Alignment>(from.alignment)).label);
        sheet.base = from.scores;
        sheet.scores = from.scores;
        sheet.level = std::max(1, *std::max_element(from.levels.begin(), from.levels.end()));
        sheet.hit_points = from.hit_points;
        if ((game_ == GoldBoxGame::death_knights_of_krynn ||
             game_ == GoldBoxGame::dark_queen_of_krynn) &&
            from.prepared)
        {
            sheet.prepared_spells = *from.prepared;
        }
        for (std::size_t at = 0; at < class_slot_count; ++at)
        {
            if (from.levels[at] > 0)
            {
                sheet.level_classes.emplace_back(class_slot_id(static_cast<ClassSlot>(at), game_));
            }
        }
        CharacterSheet::Original numbers;
        numbers.class_levels = from.levels;
        numbers.thac0 = thac0(from.levels, game_);
        numbers.saving_throws = saving_throws(from.levels, game_);
        numbers.exceptional_strength = from.exceptional_strength;
        numbers.age = from.age;
        numbers.aging_changes_scores = aging_ == Aging::add;
        numbers.race = from.race;
        numbers.profession = from.profession;
        numbers.alignment = from.alignment;
        numbers.deity = from.deity;
        numbers.robe_order = from.robe;
        numbers.knight_order = from.knight;
        numbers.experience = from.experience;
        numbers.platinum = (game_ == GoldBoxGame::death_knights_of_krynn ||
                            game_ == GoldBoxGame::dark_queen_of_krynn)
                               ? 0
                               : platinum();
        numbers.drained_levels = from.drained_levels;
        numbers.drained_hit_points = from.drained_hit_points;
        // Its Hit Points less a level's Constitution bonus, as Curse's
        // ConvertPoolRadPlayer reckons them, or those it rolled where the
        // game kept them.
        numbers.rolled_hit_points =
            from.rolled_hit_points
                ? *from.rolled_hit_points
                : std::max(0, from.hit_points -
                                  constitution_hit_point_bonus(profession,
                                                               from.scores[Ability::constitution]));
        numbers.thief_skills = thief_skills(race, level_in(from.levels, ClassSlot::thief),
                                            from.scores[Ability::dexterity], game_);
        const auto slots = caster_slots(from.levels, from.scores[Ability::wisdom],
                                        from.scores[Ability::intelligence], game_);
        numbers.cleric_slots = slots.cleric;
        numbers.druid_slots = slots.druid;
        numbers.magic_user_slots = slots.magic_user;
        // Its spell book as it was: no starting spells of this game's it lacked.
        numbers.known_spells =
            known_spells(from.levels, from.scores[Ability::wisdom], from.learned, game_);
        std::erase_if(numbers.known_spells,
                      [&from](int number)
                      {
                          return spell(number)->caster_class == SpellClass::magic_user &&
                                 std::ranges::find(from.learned, number) == from.learned.end();
                      });
        if (krynn_game(game_))
        {
            if (game_ == GoldBoxGame::dark_queen_of_krynn)
            {
                numbers.known_spells = from.learned;
                recalculate_dark_queen_numbers(numbers, sheet.scores);
            }
            else if (game_ == GoldBoxGame::death_knights_of_krynn)
            {
                numbers.known_spells = from.learned;
                recalculate_death_knights_numbers(numbers, sheet.scores);
            }
            else
            {
                recalculate_champions_numbers(numbers, sheet.scores);
            }
        }
        sheet.original = numbers;
        return sheet;
    }

    // An NPC's sheet: its record's scores, levels, Hit Points, spells known
    // and memorized, and its race, class and alignment.
    [[nodiscard]] CharacterSheet record_sheet(const CharacterDraft &draft,
                                              const por::CharacterRecord &record) const
    {
        const auto ability = record_scores(record);
        const auto levels = record_levels(record);
        const auto race = find_id(races, draft.race).value_or(Race::human);
        const auto profession = record_profession(record);
        const auto alignment =
            find_id(alignments, draft.alignment).value_or(Alignment::true_neutral);
        CharacterSheet sheet;
        sheet.identity = identity();
        sheet.name = draft.name;
        sheet.race = std::string(named(races, race).label);
        sheet.gender = draft.gender == "female" ? "Female" : "Male";
        sheet.character_class = std::string(named(professions, profession).label);
        sheet.alignment = std::string(named(alignments, alignment).label);
        sheet.base = ability;
        sheet.scores = ability;
        sheet.level = std::max(1, *std::max_element(levels.begin(), levels.end()));
        sheet.hit_points = std::max<int>(record.max_hit_points, 1);
        for (std::size_t at = 0; at < class_slot_count; ++at)
        {
            if (levels[at] > 0)
            {
                sheet.level_classes.emplace_back(class_slot_id(static_cast<ClassSlot>(at), game_));
            }
        }
        CharacterSheet::Original numbers;
        numbers.class_levels = levels;
        numbers.thac0 = thac0(levels, game_);
        numbers.saving_throws = saving_throws(levels, game_);
        numbers.exceptional_strength = record.current_abilities.exceptional_strength;
        numbers.rolled_hit_points = record.rolled_hit_points;
        numbers.age = record.age;
        numbers.aging_changes_scores = aging_ == Aging::add;
        numbers.race = static_cast<int>(race);
        numbers.profession = static_cast<int>(profession);
        numbers.alignment = static_cast<int>(alignment);
        numbers.deity = record.deity;
        numbers.robe_order = record.robe_order;
        numbers.knight_order = record.knight_order;
        std::copy(record.deity_spell_slots.begin(), record.deity_spell_slots.end(),
                  numbers.deity_spell_slots.begin());
        numbers.thief_skills = thief_skills(race, level_in(levels, ClassSlot::thief),
                                            ability[Ability::dexterity], game_);
        const auto slots =
            caster_slots(levels, ability[Ability::wisdom], ability[Ability::intelligence], game_);
        numbers.cleric_slots = slots.cleric;
        numbers.druid_slots = slots.druid;
        numbers.magic_user_slots = slots.magic_user;
        // A magic-user's or ranger's book: the record's known-spell flags, by
        // number.
        const bool keeps_book =
            level_in(levels, ClassSlot::magic_user) > 0 || level_in(levels, ClassSlot::ranger) > 0;
        std::vector<int> learned;
        for (std::size_t n = 0; keeps_book && n < record.spell_book.size(); ++n)
        {
            if (record.spell_book[n] != 0)
            {
                learned.push_back(canonical_spell(static_cast<int>(n) + 1, record.game));
            }
        }
        numbers.known_spells = known_spells(levels, ability[Ability::wisdom], learned, game_);
        if (krynn_game(game_))
        {
            numbers.experience = record.experience;
            numbers.known_spells = learned;
            if (game_ == GoldBoxGame::dark_queen_of_krynn)
            {
                recalculate_dark_queen_numbers(numbers, ability);
            }
            else if (game_ == GoldBoxGame::death_knights_of_krynn)
            {
                recalculate_death_knights_numbers(numbers, ability);
            }
            else
            {
                recalculate_champions_numbers(numbers, ability);
            }
        }
        sheet.original = numbers;
        for (const auto memorized : record.memorized_spells)
        {
            if (memorized != 0 && memorized < 0x80)
            {
                if (const auto id = spell_id(canonical_spell(memorized, record.game)); !id.empty())
                {
                    sheet.prepared_spells.push_back(id);
                }
            }
        }
        return sheet;
    }

    // Modify's scores, or else the roll's.
    [[nodiscard]] rules::AbilityArray<int> scores(const CharacterDraft &draft) const
    {
        if (const auto modified = modified_of(draft.rules_data))
        {
            return modified->scores;
        }
        const auto chosen = read_choices(draft, game_);
        const auto rolled = decode(draft.rules_data);
        return ability_scores(chosen.race, chosen.gender, chosen.profession, rolled, aging_,
                              rolled.starting_age, game_);
    }

    // A new character's experience for each class and its platinum: the
    // game's, or what a design's game record sets.
    [[nodiscard]] int experience_of(Profession profession, std::string_view alignment = {}) const
    {
        if (game_ == GoldBoxGame::champions_of_krynn &&
            (profession == Profession::cleric || profession == Profession::cleric_thief))
        {
            const auto chosen = find_id(alignments, alignment);
            return chosen && static_cast<unsigned>(*chosen) % 3 == 1 ? 1500 : 2000;
        }
        if (!start_)
        {
            return starting_experience(profession, game_);
        }
        return start_->experience / static_cast<int>(classes_of(profession).size());
    }

    [[nodiscard]] int platinum() const
    {
        return start_ ? start_->platinum : starting_platinum(game_);
    }

    Aging aging_;
    GoldBoxGame game_;
    std::optional<StartingStock> start_;
};
} // namespace

namespace
{
constexpr std::string_view pool_edition = "pool-of-radiance-1.3";
constexpr std::string_view curse_edition = "curse-of-the-azure-bonds";
constexpr std::string_view secret_edition = "secret-of-the-silver-blades";
constexpr std::string_view gateway_edition = "gateway-to-the-savage-frontier";
constexpr std::string_view champions_edition = "champions-of-krynn";
constexpr std::string_view dark_queen_edition = "dark-queen-of-krynn";
constexpr std::string_view death_knights_edition = "death-knights-of-krynn";
constexpr std::string_view ua_edition = "unlimited-adventures";
constexpr std::string_view darkness_edition = "pools-of-darkness";
constexpr std::string_view treasures_edition = "treasures-of-the-savage-frontier";
constexpr std::string_view countdown_edition = "countdown-to-doomsday";
constexpr std::string_view matrix_edition = "matrix-cubed";
} // namespace

rules::Identity identity(GoldBoxGame game)
{
    const auto edition = game == GoldBoxGame::dark_queen_of_krynn           ? dark_queen_edition
                         : game == GoldBoxGame::death_knights_of_krynn      ? death_knights_edition
                         : game == GoldBoxGame::champions_of_krynn          ? champions_edition
                         : game == GoldBoxGame::curse_of_the_azure_bonds    ? curse_edition
                         : game == GoldBoxGame::secret_of_the_silver_blades ? secret_edition
                         : game == GoldBoxGame::gateway_to_the_savage_frontier   ? gateway_edition
                         : game == GoldBoxGame::unlimited_adventures             ? ua_edition
                         : game == GoldBoxGame::pools_of_darkness                ? darkness_edition
                         : game == GoldBoxGame::treasures_of_the_savage_frontier ? treasures_edition
                         : game == GoldBoxGame::countdown_to_doomsday            ? countdown_edition
                         : game == GoldBoxGame::matrix_cubed                     ? matrix_edition
                                                                                 : pool_edition;
    return {"opengold.original", "0.1.0", std::string(edition)};
}

GoldBoxGame game_of(const rules::Identity &identity)
{
    if (identity.content == dark_queen_edition)
    {
        return GoldBoxGame::dark_queen_of_krynn;
    }
    if (identity.content == death_knights_edition)
    {
        return GoldBoxGame::death_knights_of_krynn;
    }
    if (identity.content == champions_edition)
    {
        return GoldBoxGame::champions_of_krynn;
    }
    if (identity.content == curse_edition)
    {
        return GoldBoxGame::curse_of_the_azure_bonds;
    }
    if (identity.content == gateway_edition)
    {
        return GoldBoxGame::gateway_to_the_savage_frontier;
    }
    if (identity.content == ua_edition)
    {
        return GoldBoxGame::unlimited_adventures;
    }
    if (identity.content == darkness_edition)
    {
        return GoldBoxGame::pools_of_darkness;
    }
    if (identity.content == treasures_edition)
    {
        return GoldBoxGame::treasures_of_the_savage_frontier;
    }
    if (identity.content == countdown_edition)
    {
        return GoldBoxGame::countdown_to_doomsday;
    }
    if (identity.content == matrix_edition)
    {
        return GoldBoxGame::matrix_cubed;
    }
    return identity.content == secret_edition ? GoldBoxGame::secret_of_the_silver_blades
                                              : GoldBoxGame::pool_of_radiance;
}

std::unique_ptr<rules::CharacterRules> character_rules(Aging aging, GoldBoxGame game)
{
    return std::make_unique<OriginalCharacterRules>(aging, game);
}

std::unique_ptr<rules::CharacterRules> design_character_rules(Aging aging, StartingStock start)
{
    return std::make_unique<OriginalCharacterRules>(aging, GoldBoxGame::unlimited_adventures,
                                                    start);
}
} // namespace opengold::original
