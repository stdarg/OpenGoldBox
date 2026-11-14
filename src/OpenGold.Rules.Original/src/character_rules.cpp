#include "opengold/original.h"
#include "opengold/curse_records.h"
#include "opengold/secret_records.h"
#include "opengold/pool_radiance.h"
#include "opengold/original_spells.h"
#include <algorithm>
#include <cctype>
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
constexpr std::array<Named<Race>, 6> races{{
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
}};

constexpr std::array<Named<Gender>, 2> genders{{
    {Gender::male, "male", "Male", "A female's Strength may be lower than a male's."},
    {Gender::female, "female", "Female", "A female's Strength may be lower than a male's."},
}};

constexpr std::array<Named<Profession>, 15> professions{{
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
}};

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

[[nodiscard]] RolledCharacter decode(std::string_view data)
{
    std::istringstream in{std::string(data)};
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
                                return roll < 2 || roll > 20;
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
// bytes, Curse's 422 or Secret's 439.
std::optional<por::CharacterRecord> decode_record(std::span<const std::uint8_t> bytes)
{
    switch (bytes.size())
    {
    case 422:
        return curse::decode_creature(bytes);
    case 439:
        return secret::decode_creature(bytes);
    default:
        return por::decode_character(bytes);
    }
}

constexpr std::string_view record_prefix = "record:";
constexpr std::string_view imported_prefix = "imported:";

// A character brought from another game (Curse's ConvertPoolRadPlayer): its
// scores, levels, Hit Points, age, experience, drained levels and the
// magic-user spells it learned, as the draft keeps them.
struct Imported
{
    rules::AbilityArray<int> scores{};
    int exceptional_strength{};
    ClassLevels levels{};
    int hit_points{}, age{}, experience{}, drained_levels{}, drained_hit_points{};
    int race{}, profession{}, alignment{};
    std::vector<int> learned;
};

std::string encode_imported(const Imported &from)
{
    std::ostringstream out;
    out << imported_prefix << 1;
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
    return out.str();
}

std::optional<Imported> imported_of(const CharacterDraft &draft)
{
    std::string_view data = draft.rules_data;
    if (!data.starts_with(imported_prefix))
    {
        return std::nullopt;
    }
    std::istringstream in{std::string(data.substr(imported_prefix.size()))};
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
    std::string rest;
    if (!in || version != 1 || (in >> rest) || from.hit_points < 1)
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
std::optional<por::CharacterRecord> record_of(const CharacterDraft &draft)
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
    auto record = decode_record(bytes);
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

class OriginalCharacterRules final : public rules::CharacterRules
{
  public:
    OriginalCharacterRules(Aging aging, GoldBoxGame game) : aging_(aging), game_(game)
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
        const auto record = decode_record(bytes);
        if (!record)
        {
            throw std::invalid_argument("Unreadable original record");
        }
        CharacterDraft draft;
        draft.name = title_case(record->name);
        draft.race =
            std::string(named(races, record->race >= 1 && record->race <= 7 && record->race != 6
                                         ? static_cast<Race>(record->race - 1)
                                         : Race::human)
                            .id);
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
    // game's platinum (Curse's 300).
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
        const auto limits = ability_limits(race, gender);
        Imported in;
        for (const auto ability : rules::all_abilities)
        {
            in.scores[ability] =
                std::clamp(from.scores[ability], limits.minimum[ability], limits.maximum[ability]);
        }
        in.exceptional_strength =
            in.scores[Ability::strength] == 18
                ? std::min(o.exceptional_strength, limits.exceptional_strength_maximum)
                : 0;
        in.levels = o.class_levels;
        in.hit_points = std::max(from.hit_points, 1);
        in.age = o.age;
        in.experience = static_cast<int>(std::min<unsigned>(experience, 100000000));
        in.drained_levels = o.drained_levels;
        in.drained_hit_points = o.drained_hit_points;
        in.race = o.race;
        in.profession = o.profession;
        in.alignment = o.alignment;
        constexpr int animate_dead = 36;
        for (const int number : o.known_spells)
        {
            const auto *known = spell(number);
            if (known && known->caster_class == SpellClass::magic_user && number != animate_dead)
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
            return listed(alignments);
        case CreationField::background:
            return {};
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

    void roll_in_place(CharacterDraft &draft, RandomState &random) const override
    {
        const auto chosen = read_choices(draft, game_);
        draft.rules_data = encode(roll_character(chosen.race, chosen.profession, game_, random));
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
            const auto open = alignments_for(*profession);
            return std::ranges::find(open, *alignment) != open.end();
        }
        case CreationField::background:
            return false;
        }
        return false;
    }

    std::optional<int> ability_score(const CharacterDraft &draft, Ability ability) const override
    {
        if (const auto record = record_of(draft))
        {
            return record_scores(*record)[ability];
        }
        if (const auto imported = imported_of(draft))
        {
            return imported->scores[ability];
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
        if (const auto record = record_of(draft))
        {
            return record_sheet(draft, *record);
        }
        if (const auto imported = imported_of(draft))
        {
            return imported_sheet(draft, *imported);
        }
        const auto chosen = read_choices(draft, game_);
        const auto alignment = require_id(alignments, draft.alignment, "alignment");
        const auto open = alignments_for(chosen.profession);
        if (std::ranges::find(open, alignment) == open.end())
        {
            throw std::invalid_argument("The class does not allow that alignment");
        }
        if (!draft.rolled)
        {
            throw std::invalid_argument("The character's abilities are not rolled");
        }
        const auto rolled = decode(draft.rules_data);
        const auto ability = ability_scores(chosen.race, chosen.gender, chosen.profession, rolled,
                                            aging_, rolled.starting_age);
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
            starting_training(chosen.race, chosen.profession, ability, rolled, game_);
        const auto &levels = trained.levels;
        sheet.level = *std::max_element(levels.begin(), levels.end());
        sheet.hit_points = first_level_hit_points(chosen.profession, rolled,
                                                  ability[Ability::constitution], game_) +
                           trained.hit_points;
        for (const auto slot : classes_of(chosen.profession))
        {
            sheet.level_classes.emplace_back(class_slot_id(slot));
        }
        CharacterSheet::Original original;
        original.class_levels = levels;
        original.thac0 = thac0(levels, game_);
        original.saving_throws = saving_throws(levels, game_);
        original.experience = starting_experience(chosen.profession, game_);
        original.platinum = starting_platinum(game_);
        original.exceptional_strength = exceptional_strength(
            chosen.race, chosen.gender, chosen.profession, rolled, ability[Ability::strength]);
        original.rolled_hit_points =
            first_level_rolled_hit_points(chosen.profession, rolled) + trained.rolled_hit_points;
        original.age = rolled.starting_age;
        original.gold = rolled.gold;
        original.race = static_cast<int>(chosen.race);
        original.profession = static_cast<int>(chosen.profession);
        original.alignment = static_cast<int>(alignment);
        original.thief_skills = thief_skills(chosen.race, level_in(levels, ClassSlot::thief),
                                             ability[Ability::dexterity], game_);
        original.cleric_slots =
            spell_slots(ClassSlot::cleric, level_in(levels, ClassSlot::cleric),
                        ability[Ability::wisdom], ability[Ability::intelligence], game_);
        original.magic_user_slots =
            spell_slots(ClassSlot::magic_user, level_in(levels, ClassSlot::magic_user),
                        ability[Ability::wisdom], ability[Ability::intelligence], game_);
        original.known_spells =
            known_spells(levels, ability[Ability::wisdom], trained.learned_spells, game_);
        sheet.original = original;
        return sheet;
    }

    // The original has no skills, cantrips or spell choices at creation: a
    // cleric knows every first-level spell and a magic-user starts with four.
    std::vector<rules::TrainingChoiceGroup> training_options(const CharacterDraft &) const override
    {
        return {};
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
        for (std::size_t at = 0; at < class_slot_count; ++at)
        {
            if (from.levels[at] > 0)
            {
                sheet.level_classes.emplace_back(class_slot_id(static_cast<ClassSlot>(at)));
            }
        }
        CharacterSheet::Original numbers;
        numbers.class_levels = from.levels;
        numbers.thac0 = thac0(from.levels, game_);
        numbers.saving_throws = saving_throws(from.levels, game_);
        numbers.exceptional_strength = from.exceptional_strength;
        numbers.age = from.age;
        numbers.race = from.race;
        numbers.profession = from.profession;
        numbers.alignment = from.alignment;
        numbers.experience = from.experience;
        numbers.platinum = starting_platinum(game_);
        numbers.drained_levels = from.drained_levels;
        numbers.drained_hit_points = from.drained_hit_points;
        // Its Hit Points less a level's Constitution bonus, as Curse's
        // ConvertPoolRadPlayer reckons them.
        numbers.rolled_hit_points =
            std::max(0, from.hit_points - constitution_hit_point_bonus(
                                              profession, from.scores[Ability::constitution]));
        numbers.thief_skills = thief_skills(race, level_in(from.levels, ClassSlot::thief),
                                            from.scores[Ability::dexterity], game_);
        numbers.cleric_slots =
            spell_slots(ClassSlot::cleric, level_in(from.levels, ClassSlot::cleric),
                        from.scores[Ability::wisdom], from.scores[Ability::intelligence], game_);
        numbers.magic_user_slots =
            spell_slots(ClassSlot::magic_user, level_in(from.levels, ClassSlot::magic_user),
                        from.scores[Ability::wisdom], from.scores[Ability::intelligence], game_);
        // Its spell book as it was: no starting spells of this game's it lacked.
        numbers.known_spells =
            known_spells(from.levels, from.scores[Ability::wisdom], from.learned, game_);
        std::erase_if(numbers.known_spells,
                      [&from](int number)
                      {
                          return spell(number)->caster_class == SpellClass::magic_user &&
                                 std::ranges::find(from.learned, number) == from.learned.end();
                      });
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
                sheet.level_classes.emplace_back(class_slot_id(static_cast<ClassSlot>(at)));
            }
        }
        CharacterSheet::Original numbers;
        numbers.class_levels = levels;
        numbers.thac0 = thac0(levels, game_);
        numbers.saving_throws = saving_throws(levels, game_);
        numbers.exceptional_strength = record.current_abilities.exceptional_strength;
        numbers.rolled_hit_points = record.rolled_hit_points;
        numbers.age = record.age;
        numbers.race = static_cast<int>(race);
        numbers.profession = static_cast<int>(profession);
        numbers.alignment = static_cast<int>(alignment);
        numbers.thief_skills = thief_skills(race, level_in(levels, ClassSlot::thief),
                                            ability[Ability::dexterity], game_);
        numbers.cleric_slots =
            spell_slots(ClassSlot::cleric, level_in(levels, ClassSlot::cleric),
                        ability[Ability::wisdom], ability[Ability::intelligence], game_);
        numbers.magic_user_slots =
            spell_slots(ClassSlot::magic_user, level_in(levels, ClassSlot::magic_user),
                        ability[Ability::wisdom], ability[Ability::intelligence], game_);
        // A magic-user's book: the record's known-spell flags, by number.
        std::vector<int> learned;
        for (std::size_t n = 0; n < record.spell_book.size(); ++n)
        {
            if (record.spell_book[n] != 0)
            {
                learned.push_back(canonical_spell(static_cast<int>(n) + 1, record.game));
            }
        }
        numbers.known_spells = known_spells(levels, ability[Ability::wisdom], learned, game_);
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

    [[nodiscard]] rules::AbilityArray<int> scores(const CharacterDraft &draft) const
    {
        const auto chosen = read_choices(draft, game_);
        const auto rolled = decode(draft.rules_data);
        return ability_scores(chosen.race, chosen.gender, chosen.profession, rolled, aging_,
                              rolled.starting_age);
    }

    Aging aging_;
    GoldBoxGame game_;
};
} // namespace

namespace
{
constexpr std::string_view pool_edition = "pool-of-radiance-1.3";
constexpr std::string_view curse_edition = "curse-of-the-azure-bonds";
constexpr std::string_view secret_edition = "secret-of-the-silver-blades";
constexpr std::string_view gateway_edition = "gateway-to-the-savage-frontier";
constexpr std::string_view ua_edition = "unlimited-adventures";
constexpr std::string_view darkness_edition = "pools-of-darkness";
} // namespace

rules::Identity identity(GoldBoxGame game)
{
    const auto edition = game == GoldBoxGame::curse_of_the_azure_bonds         ? curse_edition
                         : game == GoldBoxGame::secret_of_the_silver_blades    ? secret_edition
                         : game == GoldBoxGame::gateway_to_the_savage_frontier ? gateway_edition
                         : game == GoldBoxGame::unlimited_adventures           ? ua_edition
                         : game == GoldBoxGame::pools_of_darkness              ? darkness_edition
                                                                               : pool_edition;
    return {"opengold.original", "0.1.0", std::string(edition)};
}

GoldBoxGame game_of(const rules::Identity &identity)
{
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
    return identity.content == secret_edition ? GoldBoxGame::secret_of_the_silver_blades
                                              : GoldBoxGame::pool_of_radiance;
}

std::unique_ptr<rules::CharacterRules> character_rules(Aging aging, GoldBoxGame game)
{
    return std::make_unique<OriginalCharacterRules>(aging, game);
}
} // namespace opengold::original
