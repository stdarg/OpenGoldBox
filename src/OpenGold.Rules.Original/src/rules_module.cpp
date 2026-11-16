#include "combat_session.h"
#include "opengold/original.h"
#include "opengold/original_combat.h"
#include "opengold/original_spells.h"
#include "opengold/dice.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <map>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>

// The original rules as the rules module the game's fights and party run
// through (docs/ORIGINAL-RULES.md, plan step 4). A fight and a character's
// numbers in it are built; camp, magic, training and the temple come with the
// plan's later steps, and say so until then.
namespace opengold::original
{
namespace
{

using namespace rules;

// Where an item type is readied (ITEMS byte 0, C_Equip): a weapon, a
// shield, armor; gauntlets to boots in their own places, rings two at most
// (readying_refusal).
constexpr std::uint8_t weapon_place = 0, shield_place = 1, armor_place = 2;

std::runtime_error unavailable(std::string_view what)
{
    return std::runtime_error(std::string(what) + " is not available under the original rules yet");
}

// A sheet's original numbers, which only a character made under the original
// rules has.
const CharacterSheet::Original &original_numbers(const CharacterSheet &sheet)
{
    if (!sheet.original)
    {
        throw std::invalid_argument("This character was not made under the original rules");
    }
    return *sheet.original;
}

// The classes the character would train in at a hall with this much
// experience. Curse, Gateway and Treasures train one a visit: the ready class whose next
// level needs the most experience (their TrainingMenu, which read every class
// at the last one's level; each is read at its own here, docs/ORIGINAL-RULES.md, Bugs).
std::vector<ClassSlot> ready_classes(const CharacterSheet &sheet, unsigned experience)
{
    const auto &numbers = original_numbers(sheet);
    const auto capped = std::min<unsigned>(experience, std::numeric_limits<int>::max());
    const auto game = game_of(sheet.identity);
    auto ready = classes_ready_to_train(static_cast<Race>(numbers.race), numbers.class_levels,
                                        sheet.scores, static_cast<int>(capped), game);
    if (ready.size() > 1 && (game == GoldBoxGame::curse_of_the_azure_bonds ||
                             game == GoldBoxGame::gateway_to_the_savage_frontier ||
                             game == GoldBoxGame::treasures_of_the_savage_frontier))
    {
        const auto needed = [&](ClassSlot slot)
        {
            return experience_for_level(slot, level_in(numbers.class_levels, slot) + 1, game)
                .value_or(0);
        };
        const auto most = *std::ranges::max_element(ready, {}, needed);
        ready = {most};
    }
    return ready;
}

// What a training rolled, as a level's rules data: a version, the trained
// classes as bits by class slot, and the Hit Dice's total.
struct Training
{
    unsigned classes{};
    int rolled{};
};

constexpr int training_data_version = 1;

std::string encode_training(const Training &training)
{
    return std::to_string(training_data_version) + ' ' + std::to_string(training.classes) + ' ' +
           std::to_string(training.rolled);
}

Training decode_training(std::string_view data)
{
    std::istringstream in{std::string(data)};
    int version{};
    Training training;
    std::string rest;
    in >> version >> training.classes >> training.rolled;
    if (!in || version != training_data_version || (in >> rest) || training.classes == 0 ||
        training.classes >= (1U << class_slot_count) || training.rolled < 1)
    {
        throw std::invalid_argument("Unreadable original-rules training");
    }
    return training;
}

// A level drain a fight left (effhndlr.pas, the level drain), as a level
// history entry: "drain", the levels lost in each class slot and the Hit
// Points lost with them.
struct Drain
{
    ClassLevels levels{};
    int hit_points{};
};

constexpr std::string_view drain_word = "drain";

std::string encode_drain(const Drain &drain)
{
    std::string data(drain_word);
    for (const int level : drain.levels)
    {
        data += ' ' + std::to_string(level);
    }
    return data + ' ' + std::to_string(drain.hit_points);
}

// What being raised cost (temple.pas, RaiseDead; Curse's spell, cast_raise),
// as a level history entry: "raised" and how, "temple" or "spell".
constexpr std::string_view raised_word = "raised";

std::string encode_raised(Revival by)
{
    return std::string(raised_word) + (by == Revival::temple ? " temple" : " spell");
}

std::optional<Revival> decode_raised(std::string_view data)
{
    if (data == encode_raised(Revival::temple))
    {
        return Revival::temple;
    }
    if (data == encode_raised(Revival::spell))
    {
        return Revival::spell;
    }
    if (data.starts_with(raised_word))
    {
        throw std::invalid_argument("Unreadable original-rules raising");
    }
    return std::nullopt;
}

// The years a character's age changed by after creation (Haste ages a year,
// Youth takes 1d4+1 off), as a level history entry: "age" and the years.
constexpr std::string_view age_word = "age";

std::string encode_age(int years)
{
    return std::string(age_word) + ' ' + std::to_string(years);
}

std::optional<int> decode_age(std::string_view data)
{
    if (!data.starts_with(age_word))
    {
        return std::nullopt;
    }
    std::istringstream in{std::string(data.substr(age_word.size()))};
    int years{};
    std::string rest;
    if (!(in >> years) || (in >> rest) || years == 0)
    {
        throw std::invalid_argument("Unreadable original-rules age");
    }
    return years;
}

// Treasures' refused proposal (its romance.pas, UpdateRelationship), as a
// level history entry.
constexpr std::string_view romance_refused_entry = "romance refused";

// The Charisma the kit's rot took (effects.c, fx_rotting), as a level
// history entry: "charisma" and the points lost.
constexpr std::string_view charisma_word = "charisma";

std::string encode_charisma_loss(int points)
{
    return std::string(charisma_word) + ' ' + std::to_string(points);
}

std::optional<int> decode_charisma_loss(std::string_view data)
{
    if (!data.starts_with(charisma_word))
    {
        return std::nullopt;
    }
    std::istringstream in{std::string(data.substr(charisma_word.size()))};
    int points{};
    std::string rest;
    if (!(in >> points) || (in >> rest) || points <= 0)
    {
        throw std::invalid_argument("Unreadable original-rules Charisma loss");
    }
    return points;
}

std::optional<Drain> decode_drain(std::string_view data)
{
    if (!data.starts_with(drain_word))
    {
        return std::nullopt;
    }
    std::istringstream in{std::string(data.substr(drain_word.size()))};
    Drain drain;
    for (int &level : drain.levels)
    {
        in >> level;
    }
    in >> drain.hit_points;
    std::string rest;
    if (!in || (in >> rest) || drain.hit_points < 0 ||
        std::ranges::any_of(
            drain.levels,
            [](int level)
            {
                return level < 0;
            }))
    {
        throw std::invalid_argument("Unreadable original-rules level drain");
    }
    return drain;
}

// A drained level Restoration gives back (magic.pas, SpellRestoration), as a
// level history entry: "restore", the class slot, and the Hit Points and
// Hit Points rolled given back with it.
struct Restoration
{
    ClassSlot slot{};
    int hit_points{}, rolled_hit_points{};
};

constexpr std::string_view restore_word = "restore";

std::string encode_restoration(const Restoration &restoration)
{
    return std::string(restore_word) + ' ' +
           std::to_string(static_cast<unsigned>(restoration.slot)) + ' ' +
           std::to_string(restoration.hit_points) + ' ' +
           std::to_string(restoration.rolled_hit_points);
}

std::optional<Restoration> decode_restoration(std::string_view data)
{
    if (!data.starts_with(restore_word))
    {
        return std::nullopt;
    }
    std::istringstream in{std::string(data.substr(restore_word.size()))};
    unsigned slot{};
    Restoration restoration;
    std::string rest;
    if (!(in >> slot >> restoration.hit_points >> restoration.rolled_hit_points) || (in >> rest) ||
        slot >= class_slot_count || restoration.hit_points < 0 || restoration.rolled_hit_points < 0)
    {
        throw std::invalid_argument("Unreadable original-rules restoration");
    }
    restoration.slot = static_cast<ClassSlot>(slot);
    return restoration;
}

// What Restoration would give back (magic.pas, SpellRestoration): a level of
// the class below its peak whose next level needs the least experience (the
// later class on a tie), with the drained Hit Points' share of each level
// lost; nothing when no class is below its peak.
std::optional<Restoration> restoration_of(const CharacterSheet &sheet)
{
    const auto &numbers = original_numbers(sheet);
    const auto game = game_of(sheet.identity);
    int levels_lost = 0, drained_classes = 0;
    int least_needed = std::numeric_limits<int>::max();
    std::optional<ClassSlot> chosen;
    for (std::size_t at = 0; at < class_slot_count; ++at)
    {
        const int level = numbers.class_levels[at], peak = numbers.peak_class_levels[at];
        drained_classes += peak > 0 ? 1 : 0;
        if (level >= peak)
        {
            continue;
        }
        levels_lost += peak - level;
        const auto slot = static_cast<ClassSlot>(at);
        const int needed =
            experience_for_level(slot, level + 1, game).value_or(std::numeric_limits<int>::max());
        if (needed <= least_needed)
        {
            least_needed = needed;
            chosen = slot;
        }
    }
    if (!chosen)
    {
        return std::nullopt;
    }
    const int hit_points = numbers.drained_hit_points / levels_lost;
    const int restored_level = level_in(numbers.class_levels, *chosen) + 1;
    return Restoration{*chosen, hit_points,
                       restored_rolled_hit_points(*chosen, restored_level, hit_points,
                                                  sheet.scores[Ability::constitution],
                                                  drained_classes, game)};
}

// The slots a character has for a spell's class and level.
int slots_for(const CharacterSheet::Original &numbers, const Spell &known)
{
    const auto &slots = known.caster_class == SpellClass::cleric  ? numbers.cleric_slots
                        : known.caster_class == SpellClass::druid ? numbers.druid_slots
                                                                  : numbers.magic_user_slots;
    return known.level >= 1 && known.level <= static_cast<int>(slots.size())
               ? slots[static_cast<std::size_t>(known.level - 1)]
               : 0;
}

// The sheet's numbers that follow from its class levels, worked out again.
void recount_levels(CharacterSheet &sheet)
{
    auto &numbers = *sheet.original;
    const auto &levels = numbers.class_levels;
    const auto game = game_of(sheet.identity);
    numbers.thac0 = thac0(levels, game);
    numbers.saving_throws = saving_throws(levels, game);
    numbers.thief_skills =
        thief_skills(static_cast<Race>(numbers.race), level_in(levels, ClassSlot::thief),
                     sheet.scores[Ability::dexterity], game);
    const auto slots = caster_slots(levels, sheet.scores[Ability::wisdom],
                                    sheet.scores[Ability::intelligence], game);
    numbers.cleric_slots = slots.cleric;
    numbers.druid_slots = slots.druid;
    numbers.magic_user_slots = slots.magic_user;
    std::vector<int> learned;
    for (const int number : numbers.known_spells)
    {
        if (spell(number)->caster_class == SpellClass::magic_user)
        {
            learned.push_back(number);
        }
    }
    numbers.known_spells = known_spells(levels, sheet.scores[Ability::wisdom], learned, game);
}

// A character's record as the original keeps one, from its sheet: what
// RecalcStats starts from before the items (chargen.pas sets these at
// creation). Its coins are the party's, which OpenGoldBox shares, so they
// weigh nothing here (docs/DEVIATIONS.md).
por::CharacterRecord character_record(const CharacterSheet &sheet)
{
    const auto &original = original_numbers(sheet);
    const auto score = [&sheet](Ability ability)
    {
        return static_cast<std::uint8_t>(std::clamp(sheet.scores[ability], 0, 255));
    };
    por::CharacterRecord record;
    // The record reads its spells and items by its game's numbers.
    record.game = game_of(sheet.identity);
    record.name = sheet.name;
    // The items its classes may use (chargen.pas, the item limits).
    record.item_limit = usability_of(original.class_levels, record.game);
    record.current_abilities = {score(Ability::strength),
                                score(Ability::intelligence),
                                score(Ability::wisdom),
                                score(Ability::dexterity),
                                score(Ability::constitution),
                                score(Ability::charisma),
                                static_cast<std::uint8_t>(original.exceptional_strength)};
    record.abilities = record.current_abilities;
    // Characters' blows take Strength; a fighter above level 6 strikes three
    // times in two rounds (chargen.pas, ClassCal). In Secret a fighter or
    // paladin strikes four times above level 12, a ranger above 14
    // (classcal.pas, RecalcClassStats).
    record.strength_bonus_flag_raw = 1;
    const auto &l = original.class_levels;
    const auto fighter = level_in(l, ClassSlot::fighter);
    // Curse and Gateway give a paladin above level 6 and a ranger above 7
    // the same (their RecalcClassStats).
    const auto game = game_of(sheet.identity);
    int attacks = fighter > 6 ? 3 : 2;
    if (game != GoldBoxGame::pool_of_radiance)
    {
        const bool warrior = std::max(fighter, level_in(l, ClassSlot::paladin)) > 6;
        attacks = warrior || level_in(l, ClassSlot::ranger) > 7 ? 3 : 2;
    }
    // Pools of Darkness's are Secret's.
    if (game == GoldBoxGame::secret_of_the_silver_blades || vga_engine(game))
    {
        const auto warrior = std::max(fighter, level_in(l, ClassSlot::paladin));
        const auto ranger = level_in(l, ClassSlot::ranger);
        attacks = std::max({2, warrior > 6 ? 3 : 2, warrior > 12 ? 4 : 2, ranger > 7 ? 3 : 2,
                            ranger > 14 ? 4 : 2});
    }
    record.base_attacks[0] = {static_cast<std::uint8_t>(attacks), {1, 2, 0}};
    record.base_thac0 = original.thac0;
    record.base_armor_class = 10;
    record.max_hit_points = static_cast<std::uint8_t>(std::clamp(sheet.hit_points, 1, 255));
    record.base_movement = 12;
    const auto &t = original.saving_throws;
    record.saves = {static_cast<std::uint8_t>(t[0]), static_cast<std::uint8_t>(t[1]),
                    static_cast<std::uint8_t>(t[2]), static_cast<std::uint8_t>(t[3]),
                    static_cast<std::uint8_t>(t[4])};
    const auto level = [&l](ClassSlot slot)
    {
        return static_cast<std::uint8_t>(level_in(l, slot));
    };
    record.levels = {level(ClassSlot::cleric),  level(ClassSlot::druid),
                     level(ClassSlot::fighter), level(ClassSlot::paladin),
                     level(ClassSlot::ranger),  level(ClassSlot::magic_user),
                     level(ClassSlot::thief),   level(ClassSlot::monk)};
    record.highest_level_raw = *std::max_element(l.begin(), l.end());
    // The original's race numbers run from 1 (dwarf) to 7 (human).
    record.race = static_cast<std::uint8_t>(original.race + 1);
    record.alignment = static_cast<std::uint8_t>(original.alignment);
    // Dwarves, gnomes and halflings stand one square short (chargen.pas).
    const auto race = static_cast<Race>(original.race);
    const bool short_folk = race == Race::dwarf || race == Race::gnome || race == Race::halfling;
    record.icon_size_raw = short_folk ? 1 : 2;
    // The 256-color engine's fights (Pools of Darkness's and Treasures')
    // stand every character on one square, a tall one too (their
    // chargen.pas: the icon's height is only drawn), so no weapon strikes
    // one as large and every size check passes it.
    if (vga_engine(record.game))
    {
        record.icon_size_raw = 1;
    }
    for (const auto &id : sheet.prepared_spells)
    {
        if (const auto number = spell_number(id, game_of(sheet.identity)))
        {
            // As the game's own record holds it (canonical_spell undone).
            const int own = *number > secret_spell_offset  ? *number - secret_spell_offset
                            : *number > curse_spell_offset ? *number - curse_spell_offset
                                                           : *number;
            record.memorized_spells.push_back(static_cast<std::uint8_t>(own));
        }
    }
    return record;
}

// A character's numbers with its readied items, as the original works them
// out at the start of every fight and turn (charhlp.pas, RecalcStats), and
// the innate effects each race gets at creation (chargen.pas).
Combatant character_combatant(const CharacterSheet &sheet,
                              std::span<const CarriedItem> carried = {})
{
    auto c = combatant_from_record(character_record(sheet), carried);
    // The Constitution saves against poison (90) and against spells and wands
    // (97), and the elf's and half-elf's resistance to sleep and charm (107,
    // 124). A dwarf's and a gnome's bonus against their foes (26, 18), their
    // defense against giants (47), and a gnome's against bugbears (48).
    // Secret gives no one the save against poison and a gnome no defense
    // against giants (its chargen.pas), nor does Unlimited Adventures (the
    // kit's create.c, race_start), whose gnome strikes its foes (its effect
    // 7) as the gnome's 18 does.
    constexpr int dwarf = 1, elf = 2, gnome = 3, half_elf = 4, halfling = 5;
    constexpr int dwarf_attack = 26, gnome_attack = 18, giant_defense = 47, bugbear_defense = 48;
    const auto innate = [&c](int code)
    {
        c.effects.push_back({code, 0, 255, false});
    };
    const bool secret_or_kit =
        game_of(sheet.identity) == GoldBoxGame::secret_of_the_silver_blades ||
        game_of(sheet.identity) == GoldBoxGame::unlimited_adventures;
    if (!secret_or_kit && (c.race == dwarf || c.race == halfling))
    {
        innate(90);
    }
    if (c.race == dwarf || c.race == halfling || c.race == gnome)
    {
        innate(97);
    }
    if (c.race == dwarf)
    {
        innate(dwarf_attack);
    }
    if (c.race == gnome)
    {
        innate(gnome_attack);
        innate(bugbear_defense);
    }
    if (c.race == dwarf || (c.race == gnome && !secret_or_kit))
    {
        innate(giant_defense);
    }
    if (c.race == elf)
    {
        innate(107);
    }
    if (c.race == half_elf)
    {
        innate(124);
    }
    // Curse's paladin is protected from evil, and its ranger strikes giants
    // harder (createPlayer in its reconstruction).
    if (c.class_levels[static_cast<std::size_t>(ClassSlot::paladin)] > 0)
    {
        innate(8);
    }
    if (c.class_levels[static_cast<std::size_t>(ClassSlot::ranger)] > 0)
    {
        innate(334);
    }
    // Its age, which Haste and Youth change in a fight (age_in_years).
    c.effects.push_back({age_in_years, 0, original_numbers(sheet).age, false});
    return c;
}

class OriginalModule final : public RulesModule
{
  public:
    explicit OriginalModule(Content content) : content_(std::move(content))
    {
    }

    [[nodiscard]] Identity identity() const override;
    [[nodiscard]] std::unique_ptr<CombatSession> create(Encounter encounter,
                                                        std::uint64_t seed) const override;
    [[nodiscard]] std::unique_ptr<CombatSession>
    restore(std::string_view checkpoint) const override;
    [[nodiscard]] CharacterProfile character_profile(const CharacterSheet &,
                                                     std::span<const std::string>) const override;
    [[nodiscard]] EquipmentInfo equipment_info(std::string_view) const override;
    [[nodiscard]] unsigned carrying_capacity_pounds(const CharacterSheet &) const override;
    [[nodiscard]] bool can_use_item(const CharacterSheet &, std::string_view) const override;
    [[nodiscard]] std::vector<EquipmentChoice> equipment_choices(const CharacterSheet &,
                                                                 std::span<const std::string>,
                                                                 unsigned) const override;
    [[nodiscard]] EquipmentChange equipment_change(const CharacterSheet &,
                                                   std::span<const std::string> candidates,
                                                   unsigned selected,
                                                   EquipmentOperation) const override;
    [[nodiscard]] GearRating gear_rating(const CharacterSheet &, std::span<const std::string> gear,
                                         int target_armor_class) const override;
    [[nodiscard]] std::optional<WeaponAttackView>
    weapon_attack(const CharacterSheet &, std::span<const std::string> gear) const override;
    [[nodiscard]] bool suits(const CharacterSheet &, std::string_view key) const override;
    [[nodiscard]] SpellAccess spell_access(const CharacterSheet &) const override;
    [[nodiscard]] SpellChoiceOptions spell_choice_options(const CharacterSheet &,
                                                          SpellChoiceContext) const override;
    void apply_spell_choices(CharacterSheet &, const SpellChoices &, SpellChoiceContext,
                             ChoiceCompleteness) const override;
    [[nodiscard]] AbilityCheckModifier ability_check(const CharacterSheet &,
                                                     std::span<const std::string> gear,
                                                     Ability ability,
                                                     std::string_view skill) const override;
    [[nodiscard]] unsigned climbing_chance(const CharacterSheet &, const VitalState &,
                                           std::span<const std::string> gear) const override;
    [[nodiscard]] unsigned check_chance(const CharacterSheet &, const VitalState &,
                                        std::span<const std::string> gear, Ability ability,
                                        std::string_view skill, int difficulty) const override;
    [[nodiscard]] AbilityCheckRoll roll_ability_check(const CharacterSheet &, const VitalState &,
                                                      std::span<const std::string> gear,
                                                      Ability ability, std::string_view skill,
                                                      RandomState &random_state) const override;
    [[nodiscard]] unsigned experience_for_level(unsigned level) const override;
    [[nodiscard]] CreatureStats creature_stats(const Participant &creature) const override;
    [[nodiscard]] std::vector<TrainingReplacementOptions>
    rest_training_options(const CharacterSheet &, RestKind) const override;
    TrainingChoices replace_rest_training(CharacterSheet &,
                                          std::span<const std::string>) const override;
    [[nodiscard]] std::vector<TrainingChoiceGroup>
    training_options(const CharacterSheet &) const override;
    [[nodiscard]] TrainingChoiceGroup
    hall_training_options(const CharacterSheet &,
                          std::span<const std::string> class_ids) const override;
    void train_at_hall(CharacterSheet &, std::string_view grant) const override;
    [[nodiscard]] AdvancementOptions advancement_options(const CharacterSheet &) const override;
    [[nodiscard]] std::vector<AdvancementOption>
    advancement_classes(const CharacterSheet &) const override;
    [[nodiscard]] AdvancementOptions advancement_options(const CharacterSheet &sheet,
                                                         const AdvancementChoice &) const override;
    [[nodiscard]] AdvancementChoice default_advancement(const CharacterSheet &) const override;
    [[nodiscard]] CharacterSheet spell_choice_sheet(const CharacterSheet &,
                                                    const AdvancementChoice &) const override;
    bool advance_character(CharacterSheet &sheet, VitalState &state,
                           const AdvancementChoice &) const override;
    [[nodiscard]] bool ready_to_advance(const CharacterSheet &sheet,
                                        unsigned experience) const override;

    [[nodiscard]] bool plays_original_records() const override
    {
        return true;
    }

    [[nodiscard]] bool trains_at_halls() const override
    {
        return true;
    }

    [[nodiscard]] bool usable_outside_combat(std::string_view item) const override;
    [[nodiscard]] bool cursed_item(std::string_view item) const override;

    [[nodiscard]] std::vector<std::string> classes_to_train(const CharacterSheet &sheet,
                                                            unsigned experience) const override;
    void roll_advancement(const CharacterSheet &sheet, unsigned experience,
                          AdvancementChoice &choice, RandomState &random_state) const override;
    [[nodiscard]] unsigned experience_after_advance(const CharacterSheet &before,
                                                    unsigned experience) const override;
    [[nodiscard]] std::vector<AdvancementChoice>
    lasting_changes(const CharacterSheet &sheet, const VitalState &after) const override;
    [[nodiscard]] std::vector<AdvancementChoice> revival_changes(const CharacterSheet &sheet,
                                                                 Revival by) const override;

    // The original's coins: 20 sp, 2 ep or 1/5 pp to the gold piece
    // (gdconsts.pas, coinValues). A design's prices are platinum, which the
    // party pays at 10 gold the piece (the user's answer, 2026-11-14).
    [[nodiscard]] std::array<std::uint64_t, 5> coin_worth() const override
    {
        const std::uint64_t platinum =
            content_.game == GoldBoxGame::unlimited_adventures ? 2000 : 1000;
        return {1, 10, 100, 200, platinum};
    }
    void drain_character(CharacterSheet &sheet, VitalState &state, const Drain &drain) const;
    void restore_character(CharacterSheet &sheet, VitalState &state,
                           const Restoration &given_back) const;
    [[nodiscard]] unsigned experience_after_lasting_change(const CharacterSheet &after,
                                                           const AdvancementChoice &change,
                                                           unsigned experience) const override;
    void raise_character(CharacterSheet &sheet, VitalState &state, Revival by) const;
    void age_character(CharacterSheet &sheet, int years) const;
    static void lose_charisma(CharacterSheet &sheet, VitalState &state, int points);
    static void refuse_romance(CharacterSheet &sheet, VitalState &state);
    [[nodiscard]] unsigned experience_share(const CharacterSheet &sheet,
                                            unsigned award) const override;
    void recover(VitalState &state, const CharacterSheet &sheet) const override;

    [[nodiscard]] bool rests_any_length() const override
    {
        return true;
    }

    void recover_days(VitalState &state, const CharacterSheet &sheet, unsigned days) const override;
    void end_rest(VitalState &state, const CharacterSheet &sheet) const override;
    [[nodiscard]] std::vector<std::string> memorized_spells(const CharacterSheet &sheet,
                                                            const VitalState &state) const override;
    [[nodiscard]] std::vector<MemorizedAnew>
    memorizing_anew(const CharacterSheet &sheet, const VitalState &state,
                    const SpellChoices &choices) const override;
    void pass_time(VitalState &state, const CharacterSheet &sheet, std::uint64_t from,
                   std::uint64_t to) const override;
    [[nodiscard]] bool item_lasts(const VitalState &state, const CharacterSheet &sheet,
                                  std::string_view definition) const override;
    [[nodiscard]] std::vector<AdvancementChoice> clock_changes(const CharacterSheet &sheet,
                                                               const VitalState &state,
                                                               std::uint64_t from,
                                                               std::uint64_t to) const override;
    [[nodiscard]] RecoveryInfo recovery_info(const CharacterSheet &,
                                             const VitalState &) const override;
    void grant_temporary_hit_points(VitalState &, const CharacterSheet &,
                                    const TemporaryHitPoints &, TemporaryHpChoice) const override;
    void recover_short_rest(VitalState &, const CharacterSheet &) const override;
    [[nodiscard]] Message recover_rest_choice(VitalState &, const CharacterSheet &,
                                              std::string_view) const override;
    HitDieResult spend_hit_die(VitalState &, const CharacterSheet &, RandomState &) const override;
    void elapse(std::span<Participant>, std::chrono::milliseconds, RandomState &) const override;
    void validate_character_state(const CharacterSheet &, const VitalState &) const override;
    [[nodiscard]] RestPolicy long_rest_policy() const override;
    [[nodiscard]] RestPolicy short_rest_policy() const override;
    void set_hit_points(VitalState &, const CharacterSheet &, int) const override;

    void play_unplayed_gazes(bool played) override
    {
        gazes_played_ = played;
    }

    [[nodiscard]] bool offers_temple_service(TempleService service) const override
    {
        return service != TempleService::resurrection && service != TempleService::cure_feeblemind;
    }
    [[nodiscard]] bool temple_service_helps(const CharacterSheet &sheet, const VitalState &state,
                                            TempleService service) const override;
    void temple_service(VitalState &state, const CharacterSheet &sheet, TempleService service,
                        RandomState &random_state) const override;
    [[nodiscard]] HazardAttackResult hazard_attack(VitalState &state, const CharacterSheet &sheet,
                                                   std::span<const std::string> gear,
                                                   const HazardAttack &attack,
                                                   RandomState &random_state) const override;
    [[nodiscard]] int hit_point_maximum(const CharacterSheet &, const VitalState &) const override;
    [[nodiscard]] std::vector<StatusEffectView> status_effects(const CharacterSheet &,
                                                               const VitalState &) const override;
    [[nodiscard]] std::vector<CampAction> camp_actions(const CharacterSheet &,
                                                       const VitalState &) const override;
    void use_camp_action(const CharacterSheet &user, VitalState &user_state,
                         const CharacterSheet &target, VitalState &target_state,
                         std::string_view action, RandomState &random_state) const override;
    void drink_potion(const CharacterSheet &, VitalState &, std::string_view item,
                      RandomState &random_state) const override;
    void use_party_camp_action(const CharacterSheet &user, VitalState &user_state,
                               std::span<CampTarget> party, std::string_view action,
                               RandomState &random_state) const override;
    [[nodiscard]] std::vector<CampAction> scroll_actions(const CharacterSheet &, const VitalState &,
                                                         std::string_view scroll) const override;
    [[nodiscard]] bool use_scroll(const CharacterSheet &user, VitalState &user_state,
                                  const CharacterSheet &target, VitalState &target_state,
                                  std::string_view scroll, std::string_view action,
                                  RandomState &random_state) const override;
    [[nodiscard]] bool use_party_scroll(const CharacterSheet &user, VitalState &user_state,
                                        std::span<CampTarget> party, std::string_view scroll,
                                        std::string_view action,
                                        RandomState &random_state) const override;
    [[nodiscard]] std::optional<unsigned>
    copyable_spell_level(const CharacterSheet &, std::string_view spell) const override;
    [[nodiscard]] std::vector<ScrollCopy> scroll_copies(const CharacterSheet &, const VitalState &,
                                                        std::string_view scroll) const override;
    [[nodiscard]] std::vector<ScrollCopy> pending_scribing(const CharacterSheet &,
                                                           std::string_view scroll) const override;
    [[nodiscard]] std::vector<std::chrono::minutes>
    preparation_finishes(std::span<const unsigned> levels) const override;
    [[nodiscard]] std::vector<ScrollWriting> writable_spells(const CharacterSheet &) const override;
    [[nodiscard]] bool can_cast_exploration_spell(const CharacterSheet &, const VitalState &,
                                                  std::string_view) const override;
    void cast_exploration_spell(const CharacterSheet &caster, VitalState &state,
                                std::string_view spell) const override;

  private:
    // An item of this game, by its key: its record and its type.
    [[nodiscard]] CarriedItem item_for(std::string_view key) const;
    // The gear, every item readied.
    [[nodiscard]] std::vector<CarriedItem> readied(std::span<const std::string> gear) const;

    Content content_;
    // Pools of Darkness's gazes are played in later fights (the game's option).
    bool gazes_played_{};
};

} // namespace

Identity OriginalModule::identity() const
{
    return original::identity(content_.game);
}

std::unique_ptr<CombatSession> OriginalModule::create(Encounter encounter, std::uint64_t seed) const
{
    return detail::create_session(identity(), std::move(encounter), seed, content_, gazes_played_);
}

std::unique_ptr<CombatSession> OriginalModule::restore(std::string_view checkpoint) const
{
    return detail::restore_session(identity(), checkpoint, content_, gazes_played_);
}

CharacterProfile OriginalModule::character_profile(const CharacterSheet &sheet,
                                                   std::span<const std::string> gear) const
{
    const auto carried = readied(gear);
    const auto combatant = character_combatant(sheet, carried);
    CharacterProfile profile;
    profile.data = combatant.encode();
    profile.record = encode_fight_record(character_record(sheet));
    profile.hit_points = combatant.max_hit_points;
    profile.armor_class = combatant.armor_class;
    profile.movement_feet = combatant.movement * 5;
    profile.item_save_bonus = combatant.save_bonus;
    for (const auto &item : carried)
    {
        const auto place = item.type.worn_location;
        profile.equipment_positions.push_back({place == weapon_place   ? "Weapon"
                                               : place == shield_place ? "Shield"
                                               : place == armor_place  ? "Armor"
                                                                       : "Readied",
                                               {}});
        profile.weapon_hands += place == weapon_place ? item.type.hands : 0U;
    }
    return profile;
}

unsigned OriginalModule::experience_for_level(unsigned level) const
{
    // The fighter's table, as the party screen shows a single figure.
    const auto needed =
        original::experience_for_level(ClassSlot::fighter, static_cast<int>(level), content_.game);
    if (!needed)
    {
        throw std::runtime_error("No such level under the original rules");
    }
    return static_cast<unsigned>(*needed);
}

CreatureStats OriginalModule::creature_stats(const Participant &creature) const
{
    Combatant c;
    if (!creature.character_profile.empty())
    {
        const auto decoded = Combatant::decode(creature.character_profile);
        if (!decoded)
        {
            throw std::runtime_error("Invalid original-rules profile");
        }
        c = *decoded;
    }
    else if (const auto found = content_.bestiary.find(creature.definition);
             found != content_.bestiary.end())
    {
        c = found->second;
    }
    else
    {
        throw std::runtime_error("No original-rules record for " + creature.definition);
    }
    CreatureStats stats;
    stats.armor_class = c.armor_class;
    stats.hit_points = c.max_hit_points;
    stats.initiative = c.initiative_bonus;
    stats.speed_feet = c.movement * 5;
    stats.attacks = std::max(c.attacks[0].rate_per_two_rounds / 2, 1);
    return stats;
}

void OriginalModule::set_hit_points(VitalState &state, const CharacterSheet &sheet,
                                    int hit_points) const
{
    if (hit_points < 0 || hit_points > std::max(sheet.hit_points, 1))
    {
        throw std::runtime_error("Hit Points outside the character's range");
    }
    state.hit_points = hit_points;
}

int OriginalModule::hit_point_maximum(const CharacterSheet &sheet, const VitalState &) const
{
    return std::max(sheet.hit_points, 1);
}

void OriginalModule::validate_character_state(const CharacterSheet &sheet,
                                              const VitalState &state) const
{
    if (state.hit_points < 0 || state.hit_points > std::max(sheet.hit_points, 1))
    {
        throw std::runtime_error("Hit Points outside the character's range");
    }
}

std::vector<StatusEffectView> OriginalModule::status_effects(const CharacterSheet &,
                                                             const VitalState &) const
{
    return {};
}

void OriginalModule::elapse(std::span<Participant>, std::chrono::milliseconds, RandomState &) const
{
    // No lasting effects run under the original rules yet.
}

CarriedItem OriginalModule::item_for(std::string_view key) const
{
    const auto record = por::item_of_original_key(key);
    if (!record || record->type >= content_.item_types.size())
    {
        throw std::runtime_error("Not an item of the original rules");
    }
    return {*record, content_.item_types[record->type]};
}

std::vector<CarriedItem> OriginalModule::readied(std::span<const std::string> gear) const
{
    std::vector<CarriedItem> carried;
    for (const auto &key : gear)
    {
        auto item = item_for(key);
        item.item.readied_raw = 1;
        carried.push_back(std::move(item));
    }
    return carried;
}

namespace
{
// The usability bits of the character's classes (usability_of).
std::uint8_t usability(const CharacterSheet &sheet)
{
    return usability_of(original_numbers(sheet).class_levels, game_of(sheet.identity));
}

// A d20 hit's chance against THAC0 and an Armor Class: a 1 misses, a 20 hits.
double hit_chance(int thac0, int armor_class)
{
    const int needed = std::clamp(thac0 - armor_class, 2, 20);
    return (21 - needed) / 20.0;
}
} // namespace

// Ten coins' weight is a pound.
EquipmentInfo OriginalModule::equipment_info(std::string_view key) const
{
    const auto item = item_for(key);
    EquipmentInfo info;
    switch (item.type.worn_location)
    {
    case weapon_place:
        info.slot = EquipmentSlot::weapon;
        break;
    case shield_place:
        info.slot = EquipmentSlot::shield;
        break;
    case armor_place:
        info.slot = EquipmentSlot::armor;
        break;
    default:
        info.slot = EquipmentSlot::worn;
    }
    info.hands = item.type.hands;
    info.weight_hundredths = item.item.weight * 10U;
    info.ranged = item.type.range > 1;
    return info;
}

// What the character may carry before it can take no more: its allowance and
// 1,500 coins' weight more (viewchar.pas, CannotCarryItem).
unsigned OriginalModule::carrying_capacity_pounds(const CharacterSheet &sheet) const
{
    const auto &numbers = original_numbers(sheet);
    const int coins =
        weight_allowance(sheet.scores[Ability::strength], numbers.exceptional_strength) + 1500;
    return static_cast<unsigned>(std::max(coins, 0) / 10);
}

bool OriginalModule::can_use_item(const CharacterSheet &sheet, std::string_view key) const
{
    return (item_for(key).type.class_restrictions & usability(sheet)) != 0;
}

// The original readies an item or refuses; it never chooses a hand.
std::vector<EquipmentChoice> OriginalModule::equipment_choices(const CharacterSheet &,
                                                               std::span<const std::string>,
                                                               unsigned) const
{
    return {};
}

// Readying as the original allows it (viewchar.pas, ToggleReadyItem): not a
// class's wrong item, no more than two hands' worth, one item in each place
// but two rings, one stack of arrows and one of quarrels; and a cursed item
// is never put away.
EquipmentChange OriginalModule::equipment_change(const CharacterSheet &sheet,
                                                 std::span<const std::string> candidates,
                                                 unsigned selected,
                                                 EquipmentOperation operation) const
{
    if (selected >= candidates.size())
    {
        throw std::runtime_error("Unknown equipment candidate");
    }
    const auto chosen = item_for(candidates[selected]);
    EquipmentChange change;
    if (operation == EquipmentOperation::unequip)
    {
        if (chosen.item.cursed_raw != 0)
        {
            throw std::runtime_error("It's cursed");
        }
        for (unsigned n = 0; n < candidates.size(); ++n)
        {
            if (n != selected)
            {
                change.indices.push_back(n);
            }
        }
        return change;
    }
    std::vector<CarriedItem> others;
    for (unsigned n = 0; n < candidates.size(); ++n)
    {
        if (n != selected)
        {
            others.push_back(item_for(candidates[n]));
            change.indices.push_back(n);
        }
    }
    if (const auto refusal =
            readying_refusal(chosen, others, usability(sheet), game_of(sheet.identity)))
    {
        throw std::runtime_error(*refusal == ReadyRefusal::wrong_class ? "Wrong class"
                                 : *refusal == ReadyRefusal::hands_full
                                     ? "Your hands are full!"
                                     : "Already using another item there");
    }
    change.indices.push_back(selected);
    return change;
}

// The gear's rating for auto-equip: its Armor Class, which falls as it gets
// better, and the damage its first attack is expected to deal in a round
// against the target's Armor Class, given as the SRD counts it (20 less ours).
GearRating OriginalModule::gear_rating(const CharacterSheet &sheet,
                                       std::span<const std::string> gear,
                                       int target_armor_class) const
{
    const auto c = character_combatant(sheet, readied(gear));
    const auto &attack = c.attacks[0];
    const double average = attack.dice * (attack.sides + 1) / 2.0 + attack.modifier;
    GearRating rating;
    rating.armor_class = c.armor_class;
    rating.descending_armor_class = true;
    rating.round_damage = attack.rate_per_two_rounds / 2.0 *
                          hit_chance(c.thac0, 20 - target_armor_class) * std::max(average, 0.0);
    return rating;
}

// The weapon's attack as a shop's hover shows it: what the character's
// THAC0 is better than 20 by, and the dice.
std::optional<WeaponAttackView>
OriginalModule::weapon_attack(const CharacterSheet &sheet, std::span<const std::string> gear) const
{
    const auto carried = readied(gear);
    if (std::ranges::none_of(carried,
                             [](const CarriedItem &item)
                             {
                                 return item.type.worn_location == weapon_place;
                             }))
    {
        return std::nullopt;
    }
    const auto c = character_combatant(sheet, carried);
    WeaponAttackView view;
    view.attack_bonus = 20 - c.thac0;
    view.dice = c.attacks[0].dice;
    view.sides = c.attacks[0].sides;
    view.damage_bonus = c.attacks[0].modifier;
    view.trained = true;
    return view;
}

bool OriginalModule::suits(const CharacterSheet &sheet, std::string_view key) const
{
    return can_use_item(sheet, key);
}

// Spells known and memorized come with magic (plan step 5).
SpellAccess OriginalModule::spell_access(const CharacterSheet &) const
{
    return {};
}

// Memorizing (camp.pas, CampMemorizeMenu): after a day's rest each slot takes
// a known spell of its class and level, the same spell in several slots if
// the player likes; a copy beyond the first is "#2", "#3". An hour's rest
// memorizes nothing: the original's preparation takes hours.
SpellChoiceOptions OriginalModule::spell_choice_options(const CharacterSheet &sheet,
                                                        SpellChoiceContext context) const
{
    SpellChoiceOptions options;
    if (context == SpellChoiceContext::short_rest || !sheet.original)
    {
        return options;
    }
    const auto &numbers = *sheet.original;
    for (const int number : numbers.known_spells)
    {
        const auto *known = spell(number);
        const int slots = slots_for(numbers, *known);
        for (int copy = 1; copy <= slots; ++copy)
        {
            options.preparation.push_back(
                {spell_id(number) + (copy > 1 ? "#" + std::to_string(copy) : std::string{}),
                 std::string(known->name),
                 {}});
        }
    }
    for (const auto &slots : {numbers.cleric_slots, numbers.druid_slots, numbers.magic_user_slots})
    {
        for (const int count : slots)
        {
            options.prepared_count += static_cast<unsigned>(count);
        }
    }
    options.may_prepare = options.prepared_count > 0;
    return options;
}

void OriginalModule::apply_spell_choices(CharacterSheet &sheet, const SpellChoices &choices,
                                         SpellChoiceContext context, ChoiceCompleteness) const
{
    if (!choices.prepared)
    {
        return;
    }
    const auto offered = spell_choice_options(sheet, context).preparation;
    std::map<std::pair<SpellClass, int>, int> taken;
    for (const auto &id : *choices.prepared)
    {
        if (std::ranges::none_of(offered,
                                 [&id](const CreationChoice &option)
                                 {
                                     return option.id == id;
                                 }) ||
            std::ranges::count(*choices.prepared, id) > 1)
        {
            throw std::runtime_error("That spell cannot be memorized");
        }
        const auto *chosen = spell(*spell_number(id, game_of(sheet.identity)));
        if (++taken[{chosen->caster_class, chosen->level}] > slots_for(*sheet.original, *chosen))
        {
            throw std::runtime_error("Too many spells of that level");
        }
    }
    sheet.prepared_spells = *choices.prepared;
}

// The original has no ability checks: its scripts and doors read thief
// skills and Strength. The game's checks (a d20 and a modifier against a
// difficulty) are answered with the original's chances.
AbilityCheckModifier OriginalModule::ability_check(const CharacterSheet &,
                                                   std::span<const std::string>, Ability,
                                                   std::string_view) const
{
    return {};
}

namespace
{
// A thief's skill in percent, 0 for a character with no thief levels.
unsigned thief_chance(const CharacterSheet &sheet, ThiefSkill skill)
{
    return static_cast<unsigned>(
        std::max(skill_chance(original_numbers(sheet).thief_skills, skill), 0));
}

// The chance in percent to force a door open by Strength, on the original's
// open-doors table (explore.pas, BashDoor). The original's table has no row
// for Strength 19 and above, which never opened a door; it opens as 18/00
// does (docs/ORIGINAL-RULES.md, Bugs).
unsigned bash_chance(const CharacterSheet &sheet)
{
    const int strength = sheet.scores[Ability::strength];
    const int exceptional = original_numbers(sheet).exceptional_strength;
    const int sixths = strength <= 7        ? 1
                       : strength <= 15     ? 2
                       : strength <= 17     ? 3
                       : strength > 18      ? 5
                       : exceptional == 100 ? 5
                       : exceptional > 50   ? 4
                                            : 3;
    return static_cast<unsigned>(sixths * 100 / 6);
}

// The chance a game's check stands for: picking a lock (Sleight of Hand) by
// Open Locks, climbing (Athletics with Dexterity) by Climb Walls, forcing a
// door (Athletics with Strength) by Strength; nothing else succeeds.
unsigned original_chance(const CharacterSheet &sheet, Ability ability, std::string_view skill)
{
    if (skill == "sleight_of_hand")
    {
        return thief_chance(sheet, ThiefSkill::open_locks);
    }
    if (skill == "athletics")
    {
        return ability == Ability::strength ? bash_chance(sheet)
                                            : thief_chance(sheet, ThiefSkill::climb_walls);
    }
    return 0;
}
} // namespace

unsigned OriginalModule::climbing_chance(const CharacterSheet &sheet, const VitalState &,
                                         std::span<const std::string>) const
{
    return thief_chance(sheet, ThiefSkill::climb_walls);
}

unsigned OriginalModule::check_chance(const CharacterSheet &sheet, const VitalState &,
                                      std::span<const std::string>, Ability ability,
                                      std::string_view skill, int) const
{
    return original_chance(sheet, ability, skill);
}

// A d20 whose modifier gives the original's chance against the game's usual
// difficulty of 15, to the nearest 5 percent.
AbilityCheckRoll OriginalModule::roll_ability_check(const CharacterSheet &sheet, const VitalState &,
                                                    std::span<const std::string>, Ability ability,
                                                    std::string_view skill,
                                                    RandomState &random_state) const
{
    constexpr int usual_difficulty = 15;
    const int winning_faces = (static_cast<int>(original_chance(sheet, ability, skill)) + 2) / 5;
    AbilityCheckRoll roll;
    roll.die = roll_die(random_state, 20);
    roll.total = roll.die + winning_faces - (21 - usual_difficulty);
    return roll;
}

// The original has no training a rest replaces.
std::vector<TrainingReplacementOptions>
OriginalModule::rest_training_options(const CharacterSheet &, RestKind) const
{
    return {};
}

TrainingChoices OriginalModule::replace_rest_training(CharacterSheet &,
                                                      std::span<const std::string>) const
{
    throw unavailable("replace_rest_training");
}

std::vector<TrainingChoiceGroup> OriginalModule::training_options(const CharacterSheet &) const
{
    return {};
}

TrainingChoiceGroup OriginalModule::hall_training_options(const CharacterSheet &,
                                                          std::span<const std::string>) const
{
    throw unavailable("hall_training_options");
}

// A spell scribed from a scroll into the book (camp.pas, CampScribeMenu):
// "boon:scroll:" and the spell's id, of a class and level the character has
// slots for. A spell already in the book stays, as a level the character
// comes back to after a drain replays it.
void OriginalModule::train_at_hall(CharacterSheet &sheet, std::string_view grant) const
{
    constexpr std::string_view scribed = "boon:scroll:";
    const auto number =
        grant.starts_with(scribed) ? spell_number(grant.substr(scribed.size())) : std::nullopt;
    const auto *known = number ? spell(*number, game_of(sheet.identity)) : nullptr;
    if (!known || !sheet.original || slots_for(*sheet.original, *known) == 0)
    {
        throw std::invalid_argument("That spell cannot be scribed");
    }
    auto next = sheet;
    auto &book = next.original->known_spells;
    if (std::ranges::find(book, *number) == book.end())
    {
        book.push_back(*number);
        recount_levels(next);
    }
    sheet = std::move(next);
}

// Training is the original's level-up: every class with the experience rises
// at once, so a sheet's level counts its trainings, while its class levels
// say what it is. Nothing is chosen; a magic-user's new spell comes with
// magic (plan step 5).
AdvancementOptions OriginalModule::advancement_options(const CharacterSheet &sheet) const
{
    AdvancementOptions options;
    if (ready_classes(sheet, std::numeric_limits<unsigned>::max()).empty())
    {
        return options;
    }
    options.level = static_cast<unsigned>(sheet.level) + 1;
    options.class_level = options.level;
    // A magic-user learns a spell of a level its next level casts.
    const auto &numbers = original_numbers(sheet);
    const int magic_user = level_in(numbers.class_levels, ClassSlot::magic_user);
    const auto game = game_of(sheet.identity);
    const int ranger = level_in(numbers.class_levels, ClassSlot::ranger);
    // Unlimited Adventures' ranger learns one too as its ranger level rises
    // past 8, of a level its next ranger level casts (train.c); its training
    // offers the spell only when the ranger level rises (docs/ORIGINAL-RULES.md,
    // Bugs).
    const bool kit_ranger =
        magic_user == 0 && game == GoldBoxGame::unlimited_adventures && ranger >= 8;
    if (magic_user == 0 && !kit_ranger)
    {
        return options;
    }
    ClassLevels next_ranger{};
    level_in(next_ranger, ClassSlot::ranger) = ranger + 1;
    const auto slots =
        kit_ranger
            ? caster_slots(next_ranger, sheet.scores[Ability::wisdom], 9, game).magic_user
            : spell_slots(ClassSlot::magic_user, magic_user + 1, sheet.scores[Ability::wisdom],
                          sheet.scores[Ability::intelligence], game);
    for (const int number : castable_spells(game_of(sheet.identity)))
    {
        const auto *offered = spell(number);
        if (offered->caster_class == SpellClass::magic_user &&
            slots[static_cast<std::size_t>(offered->level - 1)] > 0 &&
            std::ranges::find(numbers.known_spells, number) == numbers.known_spells.end())
        {
            options.spells.push_back({spell_id(number), std::string(offered->name), {}, true});
        }
    }
    return options;
}

std::vector<AdvancementOption> OriginalModule::advancement_classes(const CharacterSheet &) const
{
    return {};
}

AdvancementOptions OriginalModule::advancement_options(const CharacterSheet &sheet,
                                                       const AdvancementChoice &) const
{
    return advancement_options(sheet);
}

AdvancementChoice OriginalModule::default_advancement(const CharacterSheet &) const
{
    return {};
}

CharacterSheet OriginalModule::spell_choice_sheet(const CharacterSheet &sheet,
                                                  const AdvancementChoice &) const
{
    return sheet;
}

bool OriginalModule::ready_to_advance(const CharacterSheet &sheet, unsigned experience) const
{
    return !ready_classes(sheet, experience).empty();
}

std::vector<std::string> OriginalModule::classes_to_train(const CharacterSheet &sheet,
                                                          unsigned experience) const
{
    std::vector<std::string> classes;
    for (const auto slot : ready_classes(sheet, experience))
    {
        classes.emplace_back(class_slot_id(slot));
    }
    return classes;
}

// A hall trains only its classes (the choice's "hall" names them): the
// others wait for their own hall.
void OriginalModule::roll_advancement(const CharacterSheet &sheet, unsigned experience,
                                      AdvancementChoice &choice, RandomState &random_state) const
{
    auto trained = ready_classes(sheet, experience);
    if (const auto hall = choice.training.find("hall"); hall != choice.training.end())
    {
        std::erase_if(trained,
                      [&hall](ClassSlot slot)
                      {
                          return std::ranges::find(hall->second, class_slot_id(slot)) ==
                                 hall->second.end();
                      });
    }
    if (trained.empty())
    {
        throw std::runtime_error("This character is not ready to train");
    }
    Training training;
    auto after = original_numbers(sheet).class_levels;
    for (const auto slot : trained)
    {
        training.classes |= 1U << static_cast<unsigned>(slot);
        ++level_in(after, slot);
    }
    training.rolled = roll_training_hit_dice(after, trained, game_of(sheet.identity), random_state);
    choice.rules_data = encode_training(training);
}

namespace
{
constexpr int cure_light_wounds = 3, cure_serious_wounds = 158, cure_critical_wounds = 171,
              raise_dead = 175, heal = 236, barkskin = 290, druid_cure_light_wounds = 298,
              cure_blindness = 37, cure_disease = 39, remove_curse = 43,
              magic_user_remove_curse = 189, neutralize_poison = 167, enlarge = 12, reduce = 13,
              strength_spell = 35, slow_poison = 26, resurrection = 305, druid_cure_disease = 306,
              druid_neutralize_poison = 307, restoration = 302, stone_to_flesh = 313;
// The spells a member may cast in camp: the cures, Curse's Raise Dead and
// Neutralize Poison, Secret's Heal and Barkskin, which its table casts as
// Heal, Slow Poison, the Strength spells, whose effects last into the next
// fight, Pools of Darkness's Resurrection, Restoration and its ranger's
// Cure Disease and Neutralize Poison, and Stone to Flesh, which the later
// games' table lets be cast out of a fight.
constexpr std::array<int, 21> camp_spells{cure_light_wounds,
                                          cure_serious_wounds,
                                          cure_critical_wounds,
                                          raise_dead,
                                          heal,
                                          barkskin,
                                          druid_cure_light_wounds,
                                          cure_blindness,
                                          cure_disease,
                                          remove_curse,
                                          magic_user_remove_curse,
                                          neutralize_poison,
                                          enlarge,
                                          reduce,
                                          strength_spell,
                                          slow_poison,
                                          resurrection,
                                          restoration,
                                          druid_cure_disease,
                                          druid_neutralize_poison,
                                          stone_to_flesh};
constexpr std::string_view cast_prefix = "cast:";
// A paladin's laying on of hands, once between a day's rests; the member's
// state keeps that it was used (as the paladinDailyHealCast effect).
constexpr std::string_view lay_on_hands = "paladin:lay_on_hands";
constexpr int laid_on_hands = 340;
// A paladin's cure of disease, once a week and once more for each five
// levels past the first (Curse's viewchar.pas, PaladinCureDisease); the
// week's cures are counted in the strength of a week-long effect.
constexpr std::string_view cure_disease_action = "paladin:cure_disease";
constexpr int cured_this_week = 341, week_minutes = 7 * 24 * 60;

// The conditions, by the fight's numbers (lasting_condition).
constexpr int blind = 33, bestow_curse = 36, poisoned = 55, slow_poison_hit = 15,
              slow_poison_end = 22, feebleminded = 268;
constexpr std::array<int, 3> poison_codes{poisoned, slow_poison_hit, slow_poison_end};
// A revival that costs nothing, from poison (Secret's and Gateway's temples,
// Curse's Neutralize Poison) or by Pools of Darkness's Resurrection: the
// state keeps it until the raising's cost is waived (raise_character). No
// game's effect has its number.
constexpr int revived_from_poison = 600;

// Disease as its cures take it away (temple.pas, cureDiseaseEffects): the
// helplessness it brings, disease and its losses of Strength and Hit Points,
// and in Pool of Radiance rot and mummy rot.
std::vector<int> disease_codes(GoldBoxGame game)
{
    // And the Strength it wasted, which comes back (Secret's
    // RemoveDiseaseEffects, recounting Strength).
    std::vector<int> codes{31, 34, 43, 44, strength_wasted};
    if (game == GoldBoxGame::pool_of_radiance)
    {
        codes.insert(codes.end(), {50, 57});
    }
    // The kit's cures take its rot (spellfx.c, disease_cure; temple.c,
    // cure_disease: its 215 and 172), not the Charisma it took.
    if (game == GoldBoxGame::unlimited_adventures)
    {
        codes.insert(codes.end(), {50, kit_rotting});
    }
    return codes;
}

// What a temple's Heal cures besides wounds: in Secret blindness, disease and
// feeblemind, in Treasures blindness and feeblemind (their temple.pas,
// CureWounds), elsewhere nothing.
std::vector<int> heal_cures(GoldBoxGame game)
{
    if (game == GoldBoxGame::secret_of_the_silver_blades)
    {
        auto cured = disease_codes(game);
        cured.insert(cured.end(), {blind, feebleminded});
        return cured;
    }
    if (game == GoldBoxGame::treasures_of_the_savage_frontier)
    {
        return {blind, feebleminded};
    }
    return {};
}

// The spells a member has memorized and not cast: its last fight's, or after
// a day's rest its sheet's.
std::vector<int> spells_left(const CharacterSheet &sheet, const VitalState &state)
{
    if (const auto last = Combatant::decode(state.resources))
    {
        return last->spells;
    }
    std::vector<int> memorized;
    for (const auto &id : sheet.prepared_spells)
    {
        if (const auto number = spell_number(id, game_of(sheet.identity)))
        {
            memorized.push_back(*number);
        }
    }
    return memorized;
}

// The conditions a member carries out of its fights (lasting_condition).
std::vector<Effect> conditions_of(const CharacterSheet &sheet, const VitalState &state)
{
    return carried_state(state.resources, game_of(sheet.identity)).conditions;
}

const Effect *condition(const std::vector<Effect> &conditions, int code)
{
    const auto found = std::ranges::find(conditions, code, &Effect::code);
    return found == conditions.end() ? nullptr : &*found;
}

bool has_any(const CharacterSheet &sheet, const VitalState &state, std::span<const int> codes)
{
    const auto conditions = conditions_of(sheet, state);
    return std::ranges::any_of(codes,
                               [&conditions](int code)
                               {
                                   return condition(conditions, code) != nullptr;
                               });
}

// A disease that has taken hold, as hours and ten minutes pass
// (effhndlr.pas, EffDiseaseStrength and EffDiseaseHitPoints): a point of
// Strength an hour down to 3, and a Hit Point every ten minutes down to 1,
// and then the member is helpless until it is cured.
void waste_away(const CharacterSheet &sheet, VitalState &state, std::vector<Effect> &conditions,
                int hours, int ten_minutes)
{
    constexpr int wasting_strength = 43, wasting_hit_points = 44, helpless = 31;
    bool done_in = false;
    if (condition(conditions, wasting_strength))
    {
        const int own = sheet.scores[Ability::strength];
        auto wasted = std::ranges::find(conditions, strength_wasted, &Effect::code);
        if (wasted == conditions.end())
        {
            conditions.push_back({strength_wasted, 0, 0, false});
            wasted = std::prev(conditions.end());
        }
        wasted->strength = std::min(wasted->strength + hours, std::max(own - 3, 0));
        done_in = own - wasted->strength <= 3;
    }
    if (condition(conditions, wasting_hit_points) && !state.dead)
    {
        state.hit_points = std::max(std::min(state.hit_points, 1), state.hit_points - ten_minutes);
        done_in = done_in || state.hit_points <= 1;
    }
    const bool helpless_already =
        std::ranges::any_of(conditions,
                            [](const Effect &e)
                            {
                                return e.code == helpless && e.minutes == 0;
                            });
    if (done_in && !helpless_already)
    {
        conditions.push_back({helpless, 0, 255, false});
    }
}

// The member's state with its conditions changed: its last fight's numbers
// keep the change, or after a rest only its conditions do.
void change_conditions(const CharacterSheet &sheet, VitalState &state,
                       const std::function<void(std::vector<Effect> &)> &change)
{
    auto carried = carried_state(state.resources, game_of(sheet.identity));
    if (carried.last_fight)
    {
        change(carried.last_fight->effects);
        state.resources = carried.last_fight->encode();
        return;
    }
    change(carried.conditions);
    state.resources = rested_state(carried.conditions);
}

// Takes the conditions of these codes away; true when one was there.
bool cure(const CharacterSheet &sheet, VitalState &state, std::span<const int> codes)
{
    bool cured = false;
    change_conditions(sheet, state,
                      [&](std::vector<Effect> &effects)
                      {
                          cured = std::erase_if(effects,
                                                [codes](const Effect &e)
                                                {
                                                    return std::ranges::find(codes, e.code) !=
                                                           codes.end();
                                                }) > 0;
                      });
    return cured;
}

// The poisoned dead stand again with at least 1 Hit Point, their raising
// costing nothing (temple.pas, NeutralizePoison; Curse's SpellNeutralizePoison).
void stand_unpoisoned(const CharacterSheet &sheet, VitalState &state)
{
    state.dead = false;
    state.hit_points = std::max(state.hit_points, 1);
    change_conditions(sheet, state,
                      [](std::vector<Effect> &effects)
                      {
                          effects.push_back({revived_from_poison, 0, 0, false});
                      });
}

// A camp caster's level for a spell (charhlp.pas, GetCasterLevel): its cleric
// or magic-user level, or a paladin's or ranger's less 8.
int camp_caster_level(const CharacterSheet &caster, const Spell &known)
{
    const auto &levels = original_numbers(caster).class_levels;
    if (known.caster_class == SpellClass::druid)
    {
        return std::max(level_in(levels, ClassSlot::ranger) - 7, 0);
    }
    if (known.caster_class == SpellClass::cleric)
    {
        return std::max(level_in(levels, ClassSlot::cleric),
                        level_in(levels, ClassSlot::paladin) - 8);
    }
    return std::max(level_in(levels, ClassSlot::magic_user),
                    level_in(levels, ClassSlot::ranger) - 8);
}

// Slow Poison in camp (magic.pas, SpellSlowPoison): one poison killed stands
// with 1 Hit Point for an hour a caster level, and dies again if the poison
// outlasts it (recover).
void hold_poison_back(const CharacterSheet &caster, const CharacterSheet &target, VitalState &state)
{
    constexpr int slow_poison_number = 26;
    if (!has_any(target, state, std::array{poisoned}))
    {
        return;
    }
    const int hour_a_level = 60 * camp_caster_level(caster, *spell(slow_poison_number));
    if (state.dead)
    {
        stand_unpoisoned(target, state);
    }
    change_conditions(target, state,
                      [hour_a_level](std::vector<Effect> &effects)
                      {
                          std::erase_if(effects,
                                        [](const Effect &e)
                                        {
                                            return e.code == slow_poison_end;
                                        });
                          effects.push_back({slow_poison_end, hour_a_level, 255, true});
                      });
}

// Enlarge, Reduce and Strength cast in camp (magic.pas): their effects last
// into the next fight, which works out the Strength they give.
void strengthen_in_camp(const CharacterSheet &caster, const CharacterSheet &target,
                        VitalState &state, int number, RandomState &random_state)
{
    constexpr int enlarge_number = 12, reduce_number = 13, enlarged = 12, strength_effect = 38;
    const auto &known = *spell(number);
    const auto game = game_of(target.identity);
    const int level = camp_caster_level(caster, known);
    const int minutes = known.minutes_fixed + known.minutes_per_level * level;
    const auto &numbers = original_numbers(target);
    if (number == reduce_number)
    {
        // A failed save against spells loses the Enlarge (SpellReduce).
        const int save = numbers.saving_throws[4];
        if (roll_die(random_state, 20) < save)
        {
            (void)cure(target, state, std::array{enlarged});
        }
        return;
    }
    const int own = target.scores[Ability::strength];
    const int own_exceptional = numbers.exceptional_strength;
    const auto [base, exceptional] =
        number == enlarge_number
            ? enlarged_strength(level, game)
            : strengthened(own, own_exceptional, numbers.class_levels, game,
                           roll_die(random_state, strength_spell_die(numbers.class_levels, game)));
    const int code = number == enlarge_number ? enlarged : strength_effect;
    change_conditions(target, state,
                      [&](std::vector<Effect> &effects)
                      {
                          std::erase_if(effects,
                                        [code](const Effect &e)
                                        {
                                            return e.code == code;
                                        });
                          effects.push_back(
                              {code, minutes, strength_code(base, exceptional), false});
                      });
}

// The spells cast in camp for their effects, which carry into the next fight
// (lasting_condition); the party is camp target 4.
constexpr int protection_from_fire = 309, mind_blank = 322;
constexpr std::array<int, 24> camp_effect_spells{1,  6,  7,  8,   16,  17,  18,  19,
                                                 24, 29, 30, 32,  42,  45,  48,  50,
                                                 52, 53, 54, 169, 309, 318, 319, 322};
constexpr int party_target = 4;

// The spells a player casts outside combat to see what is hidden, spent with
// no effect laid: a script may answer one (Treasures' Cortarra is seen
// through when Detect Magic is cast before her twin).
[[nodiscard]] bool sight_spell(int number)
{
    constexpr std::array<std::string_view, 3> sight{
        "cleric:detect_magic", "magic_user:detect_magic", "druid:detect_magic"};
    return std::ranges::any_of(sight,
                               [number](std::string_view id)
                               {
                                   return spell_number(id) == number;
                               });
}

// A camp spell's effect on its target (magic.pas, ResolveSpell): its minutes
// by the caster's level, its strength the caster's level; Mirror Image's the
// 1d4 images, Prayer's the party's side (bit 4 clear) and the level.
void lay_camp_effect(const CharacterSheet &caster, const CharacterSheet &target, VitalState &state,
                     int number, RandomState &random_state)
{
    constexpr int mirror_image = 32;
    const auto &known = *spell(number, game_of(caster.identity));
    const int level = camp_caster_level(caster, known);
    int minutes = known.minutes_fixed + known.minutes_per_level * level;
    int strength = number == mirror_image ? roll_die(random_state, 4) : level;
    int code = known.effect;
    // Mind Blank lasts a day (magic.pas, CalcSpellDuration). Protection from
    // Fire gives a ranger the druid's protection, which lasts until it has
    // taken 12 points of fire a caster level, and anyone else Resist Fire,
    // with no end (SpellProtectionFromFire).
    if (number == mind_blank)
    {
        minutes = 1440;
    }
    if (number == protection_from_fire &&
        level_in(original_numbers(target).class_levels, ClassSlot::ranger) == 0)
    {
        constexpr int resist_fire = 20;
        code = resist_fire;
        strength = 0;
    }
    change_conditions(target, state,
                      [&](std::vector<Effect> &effects)
                      {
                          std::erase_if(effects,
                                        [code](const Effect &e)
                                        {
                                            return e.code == code;
                                        });
                          effects.push_back({code, minutes, strength, false});
                      });
}

// What a paladin may still do: lay on hands today, and cure disease this week.
bool may_lay_on_hands(const CharacterSheet &sheet, const VitalState &state)
{
    return level_in(original_numbers(sheet).class_levels, ClassSlot::paladin) > 0 &&
           !condition(conditions_of(sheet, state), laid_on_hands);
}

int cures_left(const CharacterSheet &sheet, const VitalState &state)
{
    const int paladin = level_in(original_numbers(sheet).class_levels, ClassSlot::paladin);
    if (paladin == 0)
    {
        return 0;
    }
    const auto conditions = conditions_of(sheet, state);
    const auto *spent = condition(conditions, cured_this_week);
    return (paladin - 1) / 5 + 1 - (spent ? spent->strength : 0);
}

// What casting a camp spell, or a paladin's gift, spends from the member's
// state: the spell's memorized copy, or the day's or week's use.
void spend_spell(const CharacterSheet &user, VitalState &state, int number)
{
    auto left = spells_left(user, state);
    left.erase(std::ranges::find(left, number));
    auto carried = carried_state(state.resources, game_of(user.identity));
    auto numbers = carried.last_fight.value_or(character_combatant(user));
    if (!carried.last_fight)
    {
        numbers.effects.insert(numbers.effects.end(), carried.conditions.begin(),
                               carried.conditions.end());
    }
    numbers.spells = std::move(left);
    state.resources = numbers.encode();
}
} // namespace

bool OriginalModule::advance_character(CharacterSheet &sheet, VitalState &state,
                                       const AdvancementChoice &choice) const
{
    const auto &numbers = original_numbers(sheet);
    if (const auto drain = decode_drain(choice.rules_data))
    {
        drain_character(sheet, state, *drain);
        return true;
    }
    if (const auto raised = decode_raised(choice.rules_data))
    {
        raise_character(sheet, state, *raised);
        return true;
    }
    if (const auto years = decode_age(choice.rules_data))
    {
        age_character(sheet, *years);
        return true;
    }
    if (const auto points = decode_charisma_loss(choice.rules_data))
    {
        lose_charisma(sheet, state, *points);
        return true;
    }
    if (choice.rules_data == romance_refused_entry)
    {
        refuse_romance(sheet, state);
        return true;
    }
    if (const auto given_back = decode_restoration(choice.rules_data))
    {
        restore_character(sheet, state, *given_back);
        return true;
    }
    const auto training = decode_training(choice.rules_data);
    // The spell a magic-user learns, when the level is one of its own; and
    // Unlimited Adventures' ranger past level 8, when its ranger level rises.
    const auto trains = [&training](ClassSlot slot)
    {
        return (training.classes & (1U << static_cast<unsigned>(slot))) != 0;
    };
    const bool magic_user_trained =
        trains(ClassSlot::magic_user) ||
        (game_of(sheet.identity) == GoldBoxGame::unlimited_adventures &&
         trains(ClassSlot::ranger) &&
         level_in(original_numbers(sheet).class_levels, ClassSlot::ranger) >= 8);
    if (choice.spells.size() > 1 || (!choice.spells.empty() && !magic_user_trained))
    {
        throw std::invalid_argument("Only a magic-user's training learns a spell");
    }
    const auto offered = advancement_options(sheet).spells;
    for (const auto &id : choice.spells)
    {
        if (std::ranges::none_of(offered,
                                 [&id](const AdvancementOption &option)
                                 {
                                     return option.id == id;
                                 }))
        {
            throw std::invalid_argument("That spell cannot be learned");
        }
    }
    auto next = sheet;
    for (const auto &id : choice.spells)
    {
        next.original->known_spells.push_back(*spell_number(id, game_of(sheet.identity)));
    }
    auto &levels = next.original->class_levels;
    std::vector<ClassSlot> trained;
    for (std::size_t at = 0; at < class_slot_count; ++at)
    {
        if ((training.classes & (1U << at)) == 0)
        {
            continue;
        }
        trained.push_back(static_cast<ClassSlot>(at));
        if (levels[at] == 0)
        {
            throw std::invalid_argument("The character has no levels in a class it trained in");
        }
        ++levels[at];
        next.level_classes.emplace_back(class_slot_id(static_cast<ClassSlot>(at)));
        // Each class trained gives back a drained level, whose Hit Points are
        // forgotten (chargen.pas, TrainingMenu).
        auto &drained = *next.original;
        if (drained.drained_levels > 0)
        {
            drained.drained_hit_points -= drained.drained_hit_points / drained.drained_levels;
            --drained.drained_levels;
        }
    }
    recount_levels(next);
    const int gain = training_hit_points(static_cast<Profession>(numbers.profession),
                                         training.rolled, next.scores[Ability::constitution],
                                         game_of(next.identity), levels, trained);
    next.hit_points += gain;
    next.original->rolled_hit_points +=
        training_rolled_hit_points(static_cast<Profession>(numbers.profession), training.rolled);
    ++next.level;
    sheet = std::move(next);
    state.hit_points += gain;
    return true;
}

// The drain is the character's: its classes' levels, its sheet's highest level
// and its Hit Points go down; the original keeps count for training and
// Restoration to give back.
void OriginalModule::drain_character(CharacterSheet &sheet, VitalState &state,
                                     const Drain &drain) const
{
    auto next = sheet;
    auto &numbers = *next.original;
    int total = 0;
    for (std::size_t at = 0; at < class_slot_count; ++at)
    {
        const int lost = drain.levels[at];
        if (lost > numbers.class_levels[at])
        {
            throw std::invalid_argument("A drain takes more levels than the character has");
        }
        if (lost > 0)
        {
            numbers.peak_class_levels[at] =
                std::max(numbers.peak_class_levels[at], numbers.class_levels[at]);
        }
        numbers.class_levels[at] -= lost;
        total += lost;
        const auto id = class_slot_id(static_cast<ClassSlot>(at));
        for (int n = 0; n < lost; ++n)
        {
            const auto last = std::ranges::find(next.level_classes | std::views::reverse, id);
            if (last != next.level_classes.rend() && next.level_classes.size() > 1)
            {
                next.level_classes.erase(std::next(last).base());
            }
        }
    }
    recount_levels(next);
    next.level = std::max(1, next.level - total);
    next.hit_points = std::max(1, next.hit_points - drain.hit_points);
    numbers.rolled_hit_points = std::max(0, numbers.rolled_hit_points - drain.hit_points);
    numbers.drained_levels += total;
    numbers.drained_hit_points += drain.hit_points;
    state.hit_points = std::min(state.hit_points, next.hit_points);
    sheet = std::move(next);
}

// Restoration gives a drained level back: the class rises a level, and the
// Hit Points come back on the maximum and the current ones; the level is no
// longer counted drained, and the member is no longer marked for it.
void OriginalModule::restore_character(CharacterSheet &sheet, VitalState &state,
                                       const Restoration &given_back) const
{
    auto next = sheet;
    auto &numbers = *next.original;
    ++level_in(numbers.class_levels, given_back.slot);
    next.level_classes.emplace_back(class_slot_id(given_back.slot));
    ++next.level;
    recount_levels(next);
    next.hit_points += given_back.hit_points;
    numbers.rolled_hit_points += given_back.rolled_hit_points;
    numbers.drained_levels = std::max(0, numbers.drained_levels - 1);
    numbers.drained_hit_points = std::max(0, numbers.drained_hit_points - given_back.hit_points);
    (void)cure(next, state, std::array{restoring});
    if (!state.dead)
    {
        state.hit_points += given_back.hit_points;
    }
    sheet = std::move(next);
}

// The Charisma the kit's rot took comes off the sheet, never below 3, and the
// member is no longer marked for it.
void OriginalModule::lose_charisma(CharacterSheet &sheet, VitalState &state, int points)
{
    constexpr int lowest = 3;
    const int score = std::max(sheet.scores[Ability::charisma] - points, lowest);
    sheet.scores[Ability::charisma] = sheet.base[Ability::charisma] = score;
    (void)cure(sheet, state, std::array{charisma_rotted});
}

// The member whose proposal the party refused loses 6 Charisma for good,
// to 0 at least, and its love (131) is a refused one from now on: its
// attacks 2 worse, attacks on it 2 better (its romance.pas,
// UpdateRelationship; effhndlr.pas, EffInLove).
void OriginalModule::refuse_romance(CharacterSheet &sheet, VitalState &state)
{
    static constexpr int lost = 6, in_love = 1331, refused = 1;
    sheet.scores[Ability::charisma] = std::max(sheet.scores[Ability::charisma] - lost, 0);
    sheet.base[Ability::charisma] = std::max(sheet.base[Ability::charisma] - lost, 0);
    change_conditions(sheet, state,
                      [](std::vector<Effect> &conditions)
                      {
                          const auto love = std::ranges::find(conditions, in_love, &Effect::code);
                          if (love != conditions.end())
                          {
                              love->strength = refused;
                          }
                          else
                          {
                              conditions.push_back({in_love, 0, refused, false});
                          }
                      });
}

// Restoration leaves the experience the member had when it lost only the
// level given back, else what the restored level needs (magic.pas,
// SpellRestoration). A drain here takes no experience, so the member keeps
// its own, raised to the restored level's when that is more.
unsigned OriginalModule::experience_after_lasting_change(const CharacterSheet &after,
                                                         const AdvancementChoice &change,
                                                         unsigned experience) const
{
    const auto given_back = decode_restoration(change.rules_data);
    if (!given_back)
    {
        return experience;
    }
    const auto needed = original::experience_for_level(
        given_back->slot, level_in(original_numbers(after).class_levels, given_back->slot),
        game_of(after.identity));
    return std::max(experience, static_cast<unsigned>(std::max(0, needed.value_or(0))));
}

// A character's age changes by `years`. Under AD&D aging its scores follow,
// each older age group's changes kept between 3 and the race's maximum, as
// at creation (ability_scores); under the original's only its starting age
// counts.
void OriginalModule::age_character(CharacterSheet &sheet, int years) const
{
    auto next = sheet;
    auto &numbers = *next.original;
    const int before = numbers.age;
    numbers.age = std::max(1, numbers.age + years);
    if (numbers.aging_changes_scores)
    {
        const auto race = static_cast<Race>(numbers.race);
        const auto gender = next.gender == "Female" ? Gender::female : Gender::male;
        const auto limits = ability_limits(race, gender);
        const auto then = age_changes(Aging::add, race, before);
        const auto now = age_changes(Aging::add, race, numbers.age);
        for (const auto ability : rules::all_abilities)
        {
            const int change = now[ability] - then[ability];
            if (change != 0)
            {
                const int score =
                    std::clamp(next.scores[ability] + change, 3, limits.maximum[ability]);
                next.scores[ability] = next.base[ability] = score;
            }
        }
        if (next.scores[Ability::strength] != 18)
        {
            numbers.exceptional_strength = 0;
        }
        recount_levels(next);
    }
    sheet = std::move(next);
}

// A raised character loses a point of Constitution while it has one
// (temple.pas, RaiseDead; Curse's cast_raise). A temple's raising also takes
// Hit Points off its maximum (raised_hit_point_loss); the spell's does not.
void OriginalModule::raise_character(CharacterSheet &sheet, VitalState &state, Revival by) const
{
    // One that poison killed and Neutralize Poison (or Gateway's Raise Dead)
    // stood up again loses nothing (temple.pas, NeutralizePoison and RaiseDead).
    if (cure(sheet, state, std::array{revived_from_poison}))
    {
        return;
    }
    auto next = sheet;
    auto &numbers = *next.original;
    auto &constitution = next.scores[Ability::constitution];
    if (constitution > 0)
    {
        --constitution;
        next.base[Ability::constitution] = constitution;
    }
    if (by == Revival::temple)
    {
        const int loss = raised_hit_point_loss(numbers.class_levels, constitution, next.hit_points,
                                               numbers.rolled_hit_points);
        next.hit_points = std::max(1, next.hit_points - loss);
    }
    state.hit_points = std::min(state.hit_points, next.hit_points);
    sheet = std::move(next);
}

std::vector<AdvancementChoice> OriginalModule::revival_changes(const CharacterSheet &sheet,
                                                               Revival by) const
{
    if (!sheet.original)
    {
        return {};
    }
    AdvancementChoice change;
    change.rules_data = encode_raised(by);
    return {change};
}

// What a fight, or an item used outside one, left that the character keeps:
// the levels drained, the level Restoration gives back (restoring), and the
// years its age changed by (age_in_years).
std::vector<AdvancementChoice> OriginalModule::lasting_changes(const CharacterSheet &sheet,
                                                               const VitalState &after) const
{
    if (after.resources.empty() || !sheet.original)
    {
        return {};
    }
    std::vector<AdvancementChoice> changes;
    const auto carried = carried_state(after.resources, game_of(sheet.identity));
    const auto &marks = carried.last_fight ? carried.last_fight->effects : carried.conditions;
    // Restoration is given outside fights, where a last fight's levels below
    // the sheet's are a training since, not a drain.
    const bool restoring_now = std::ranges::find(marks, restoring, &Effect::code) != marks.end();
    if (const auto &last = carried.last_fight; last && !restoring_now)
    {
        Drain drain;
        bool drained = false;
        for (std::size_t at = 0; at < class_slot_count; ++at)
        {
            drain.levels[at] =
                std::max(0, sheet.original->class_levels[at] - last->class_levels[at]);
            drained = drained || drain.levels[at] > 0;
        }
        if (drained)
        {
            drain.hit_points = std::max(0, sheet.hit_points - last->max_hit_points);
            AdvancementChoice change;
            change.rules_data = encode_drain(drain);
            changes.push_back(std::move(change));
        }
    }
    const auto &effects = carried.last_fight ? carried.last_fight->effects : carried.conditions;
    if (std::ranges::find(effects, restoring, &Effect::code) != effects.end())
    {
        if (const auto given_back = restoration_of(sheet))
        {
            AdvancementChoice change;
            change.rules_data = encode_restoration(*given_back);
            changes.push_back(std::move(change));
        }
    }
    const auto age = std::ranges::find(effects, age_in_years, &Effect::code);
    if (age != effects.end() && age->strength != sheet.original->age)
    {
        AdvancementChoice change;
        change.rules_data = encode_age(age->strength - sheet.original->age);
        changes.push_back(std::move(change));
    }
    return changes;
}

unsigned OriginalModule::experience_after_advance(const CharacterSheet &before,
                                                  unsigned experience) const
{
    const auto trained = ready_classes(before, experience);
    const auto capped = std::min<unsigned>(experience, std::numeric_limits<int>::max());
    return static_cast<unsigned>(experience_after_training(original_numbers(before).class_levels,
                                                           trained, static_cast<int>(capped),
                                                           game_of(before.identity)));
}

unsigned OriginalModule::experience_share(const CharacterSheet &sheet, unsigned award) const
{
    const auto capped = std::min<unsigned>(award, std::numeric_limits<int>::max());
    return static_cast<unsigned>(original::experience_share(
        static_cast<Profession>(original_numbers(sheet).profession), sheet.scores,
        static_cast<int>(capped), game_of(sheet.identity)));
}

// A day's rest heals each member not dead 1 Hit Point (gametime.pas,
// CheckRestingHeal: one every 288 five-minute ticks), but one a disease
// wastes, which takes the point back within ten minutes (EffDiseaseHitPoints).
void OriginalModule::recover(VitalState &state, const CharacterSheet &sheet) const
{
    // A statue heals nothing until Stone to Flesh (CheckRestingHeal heals
    // only those okay).
    if (state.description == stoned_description)
    {
        return;
    }
    recover_days(state, sheet, 1);
    end_rest(state, sheet);
}

// Each day of rest heals 1 Hit Point (gametime.pas, CheckRestingHeal, every
// 288 five-minute steps), but not a statue, the dead or one wasting away.
void OriginalModule::recover_days(VitalState &state, const CharacterSheet &sheet,
                                  unsigned days) const
{
    constexpr int wasting_hit_points = 44;
    if (days == 0 || state.description == stoned_description || state.dead ||
        condition(conditions_of(sheet, state), wasting_hit_points))
    {
        return;
    }
    state.hit_points = static_cast<int>(
        std::min<long long>(static_cast<long long>(state.hit_points) + days, sheet.hit_points));
}

// The rest memorized the sheet's spells: the next fight starts from them.
// The lasting conditions stay, the rest's time having passed for them
// (pass_time).
void OriginalModule::end_rest(VitalState &state, const CharacterSheet &sheet) const
{
    if (state.description == stoned_description)
    {
        return;
    }
    state.resources = rested_state(conditions_of(sheet, state));
}

// The spells left to cast: its last fight's, or after a rest its sheet's
// (spells_left), each by its id.
std::vector<std::string> OriginalModule::memorized_spells(const CharacterSheet &sheet,
                                                          const VitalState &state) const
{
    std::vector<std::string> ids;
    for (const int number : spells_left(sheet, state))
    {
        ids.push_back(spell_id(number));
    }
    return ids;
}

// The spells a choice memorizes into empty slots (camp.pas,
// CampMemorizeMenu's pending spells): each the choice holds beyond the copies
// of it still memorized and not cast, by its level (CampSpellPrepTime). The
// rest finishes them by spell number, the order its memorized spells are
// sorted in (SortMemorizedSpells; gametime.pas, CheckMemorizeDone).
std::vector<MemorizedAnew> OriginalModule::memorizing_anew(const CharacterSheet &sheet,
                                                           const VitalState &state,
                                                           const SpellChoices &choices) const
{
    if (!sheet.original || !choices.prepared)
    {
        return {};
    }
    const auto game = game_of(sheet.identity);
    auto unspent = spells_left(sheet, state);
    std::vector<std::pair<int, MemorizedAnew>> anew;
    for (std::size_t entry = 0; entry < choices.prepared->size(); ++entry)
    {
        const auto &id = (*choices.prepared)[entry];
        const auto number = spell_number(id.substr(0, id.find('#')), game);
        if (!number)
        {
            continue;
        }
        if (const auto kept = std::ranges::find(unspent, *number); kept != unspent.end())
        {
            unspent.erase(kept);
            continue;
        }
        anew.emplace_back(*number,
                          MemorizedAnew{entry, static_cast<unsigned>(spell(*number)->level)});
    }
    std::ranges::stable_sort(anew, {}, &std::pair<int, MemorizedAnew>::first);
    std::vector<MemorizedAnew> spells;
    for (const auto &[number, spell] : anew)
    {
        spells.push_back(spell);
    }
    return spells;
}

namespace
{
// The years the campaign's clock passes from minute `from` to `to`, by the
// original's calendar: 12 months of 30 days (gdconsts.pas, unitScale).
int years_passed(std::uint64_t from, std::uint64_t to) noexcept
{
    constexpr std::uint64_t year_minutes = 12ULL * 30 * 24 * 60;
    return to <= from ? 0 : static_cast<int>(to / year_minutes - from / year_minutes);
}

bool ages_with_the_clock(const CharacterSheet &sheet) noexcept
{
    return sheet.original && sheet.original->aging_changes_scores;
}

// Unlimited Adventures' rot as months pass (effects.c, fx_rotting): each
// month that runs out takes 2 Charisma, never below 3, for the sheet to lose
// (charisma_rotted, clock_changes); the rot lasts the months its strength
// counts by 32s, and the last one leaves the member dead: it rots away.
void rot_away(const CharacterSheet &sheet, VitalState &state, std::vector<Effect> &conditions,
              int passed)
{
    constexpr int month = 43200, charisma_a_month = 2, lowest_charisma = 3, month_strength = 32;
    const auto rotting = std::ranges::find(conditions, kit_rotting, &Effect::code);
    if (rotting == conditions.end())
    {
        return;
    }
    rotting->minutes -= passed;
    int months = 0;
    bool rotted_away = false;
    while (rotting->minutes <= 0 && !rotted_away)
    {
        ++months;
        if (rotting->strength / month_strength > 1)
        {
            rotting->strength -= month_strength;
            rotting->minutes += month;
        }
        else
        {
            rotted_away = true;
        }
    }
    if (rotted_away)
    {
        conditions.erase(rotting);
        state.dead = true;
        state.hit_points = 0;
    }
    if (months == 0)
    {
        return;
    }
    auto taken = std::ranges::find(conditions, charisma_rotted, &Effect::code);
    if (taken == conditions.end())
    {
        conditions.push_back({charisma_rotted, 0, 0, false});
        taken = std::prev(conditions.end());
    }
    const int own = sheet.scores[Ability::charisma];
    taken->strength =
        std::min(taken->strength + charisma_a_month * months, std::max(own - lowest_charisma, 0));
    if (taken->strength == 0)
    {
        conditions.erase(taken);
    }
}
} // namespace

// Time passing for a member outside a fight, as the original's clock counts
// it (gametime.pas): its lasting conditions run down, camp spells, Slow
// Poison and the paladin's day and week among them; a disease whose time
// comes takes hold, as Strength and Hit Points wasting away (effhndlr.pas,
// EffDiseased), which then waste a point of Strength each hour and a Hit
// Point each ten minutes the clock passes; Slow Poison running out lets its
// poison kill (EffSlowPoison).
void OriginalModule::pass_time(VitalState &state, const CharacterSheet &sheet, std::uint64_t from,
                               std::uint64_t to) const
{
    if (state.dead || to <= from || state.resources.empty() || !sheet.original)
    {
        return;
    }
    constexpr int diseased = 34, wasting_strength = 43, wasting_hit_points = 44;
    const auto passed = static_cast<int>(std::min<std::uint64_t>(to - from, 1U << 30));
    const auto boundaries = [from, to](std::uint64_t every)
    {
        return static_cast<int>(std::min<std::uint64_t>(to / every - from / every, 1U << 30));
    };
    const auto game = game_of(sheet.identity);
    bool slow_poison_ended = false;
    // The age a member carries grows with the clock as its sheet does
    // (clock_changes), so a later change of age starts from it.
    const int years = ages_with_the_clock(sheet) ? years_passed(from, to) : 0;
    change_conditions(
        sheet, state,
        [&](std::vector<Effect> &effects)
        {
            if (const auto age = std::ranges::find(effects, age_in_years, &Effect::code);
                age != effects.end())
            {
                age->strength += years;
            }
            // What wasted through this time: only a disease that
            // had taken hold already.
            waste_away(sheet, state, effects, boundaries(60), boundaries(10));
            rot_away(sheet, state, effects, passed);
            std::vector<Effect> taking_hold;
            std::erase_if(effects,
                          [&](Effect &e)
                          {
                              if (e.minutes == 0 || e.code == kit_rotting ||
                                  !lasting_condition(e, game))
                              {
                                  return false;
                              }
                              e.minutes -= passed;
                              if (e.minutes > 0)
                              {
                                  return false;
                              }
                              if (e.code == diseased)
                              {
                                  taking_hold.push_back({wasting_strength, 0, e.strength, false});
                                  taking_hold.push_back({wasting_hit_points, 0, e.strength, false});
                              }
                              slow_poison_ended = slow_poison_ended || e.code == slow_poison_end;
                              return true;
                          });
            for (const auto &e : taking_hold)
            {
                if (!condition(effects, e.code))
                {
                    effects.push_back(e);
                }
            }
            // Slow Poison only held the poison back.
            if (slow_poison_ended && condition(effects, poisoned))
            {
                state.dead = true;
                state.hit_points = 0;
            }
        });
}

// Under AD&D aging a character grows a year older for each year the clock
// passes (the Aging setting, docs/ORIGINAL-RULES.md); the original's clock
// ages no one until its year count overflows (gametime.pas, ValidateClock).
// The Charisma the kit's rot took as the clock passed (pass_time) comes off
// the sheet for good.
std::vector<AdvancementChoice> OriginalModule::clock_changes(const CharacterSheet &sheet,
                                                             const VitalState &state,
                                                             std::uint64_t from,
                                                             std::uint64_t to) const
{
    std::vector<AdvancementChoice> changes;
    if (const int years = years_passed(from, to); ages_with_the_clock(sheet) && years != 0)
    {
        AdvancementChoice change;
        change.rules_data = encode_age(years);
        changes.push_back(std::move(change));
    }
    if (!sheet.original || state.resources.empty())
    {
        return changes;
    }
    const auto conditions = conditions_of(sheet, state);
    if (const auto *rotted = condition(conditions, charisma_rotted); rotted && rotted->strength > 0)
    {
        AdvancementChoice change;
        change.rules_data = encode_charisma_loss(rotted->strength);
        changes.push_back(std::move(change));
    }
    return changes;
}

// Spiritual Hammer's hammer stays while its spell lasts, which outlasts the
// fight by its minutes, and goes as the spell ends (EffSpiritualHammer, as
// its effect is removed); every other item stays.
bool OriginalModule::item_lasts(const VitalState &state, const CharacterSheet &sheet,
                                std::string_view definition) const
{
    constexpr int spiritual_hammer = 23;
    const auto item = por::item_of_original_key(definition);
    if (!item || !sheet.original || !is_spiritual_hammer(*item, game_of(sheet.identity)))
    {
        return true;
    }
    return condition(conditions_of(sheet, state), spiritual_hammer) != nullptr;
}

// The original has no Hit Dice or resources a rest restores; every living
// member may rest.
RecoveryInfo OriginalModule::recovery_info(const CharacterSheet &, const VitalState &state) const
{
    RecoveryInfo info;
    info.can_rest = !state.dead;
    return info;
}

void OriginalModule::grant_temporary_hit_points(VitalState &, const CharacterSheet &,
                                                const TemporaryHitPoints &, TemporaryHpChoice) const
{
    throw unavailable("grant_temporary_hit_points");
}

// The original has no short rest: an hour's rest heals nothing.
void OriginalModule::recover_short_rest(VitalState &, const CharacterSheet &) const
{
}

Message OriginalModule::recover_rest_choice(VitalState &, const CharacterSheet &,
                                            std::string_view) const
{
    throw unavailable("recover_rest_choice");
}

HitDieResult OriginalModule::spend_hit_die(VitalState &, const CharacterSheet &,
                                           RandomState &) const
{
    throw unavailable("spend_hit_die");
}

// The camp's long rest is a day, the time the original heals 1 Hit Point in,
// with no wait after it.
RestPolicy OriginalModule::long_rest_policy() const
{
    constexpr unsigned day_minutes = 24 * 60;
    return {day_minutes, 0};
}

RestPolicy OriginalModule::short_rest_policy() const
{
    constexpr unsigned hour_minutes = 60;
    return {hour_minutes, 0};
}

// The original's temple (temple.pas): Cure Light, Serious and Critical
// Wounds heal 1d8, 2d8+1 and 3d8+3, and Raise Dead brings the dead back with 1
// Hit Point, its poison gone. Cure Blindness, Cure Disease, Neutralize Poison
// and Remove Curse take their conditions away; in Secret and Gateway
// Neutralize Poison stands one poison killed up again, and Secret's and
// Treasures' Heal cure more (heal_cures). No temple raises by Resurrection or
// cures feeblemind alone; Treasures' Fix does (offers_temple_service). Stone
// to Flesh brings a statue back with 1 Hit Point.
bool OriginalModule::temple_service_helps(const CharacterSheet &sheet, const VitalState &state,
                                          TempleService service) const
{
    const auto game = game_of(sheet.identity);
    const bool reviving_poison = game == GoldBoxGame::secret_of_the_silver_blades ||
                                 game == GoldBoxGame::gateway_to_the_savage_frontier;
    switch (service)
    {
    case TempleService::cure_light_wounds:
    case TempleService::cure_serious_wounds:
    case TempleService::cure_critical_wounds:
        // A statue "can only be healed by a stonemason" (temple.pas).
        return !state.dead && state.description != stoned_description &&
               state.hit_points < sheet.hit_points;
    case TempleService::heal:
    {
        if (state.description == stoned_description)
        {
            return false;
        }
        const bool cures = has_any(sheet, state, heal_cures(game));
        return !state.dead && (state.hit_points < sheet.hit_points || cures);
    }
    case TempleService::cure_feeblemind:
        return !state.dead && has_any(sheet, state, std::array{feebleminded});
    case TempleService::resurrection:
    {
        // Only the Fix casts it (offers_temple_service): the dead but elves.
        constexpr int elf = static_cast<int>(Race::elf);
        return state.dead && original_numbers(sheet).race != elf &&
               state.description != stoned_description;
    }
    case TempleService::raise_dead:
        return state.dead;
    case TempleService::cure_blindness:
        return !state.dead && has_any(sheet, state, std::array{blind});
    case TempleService::cure_disease:
        return !state.dead && has_any(sheet, state, disease_codes(game));
    case TempleService::neutralize_poison:
        return (!state.dead || reviving_poison) && has_any(sheet, state, poison_codes);
    case TempleService::remove_curse:
        return !state.dead && has_any(sheet, state, std::array{bestow_curse});
    case TempleService::stone_to_flesh:
        return !state.dead && state.description == stoned_description;
    case TempleService::restoration:
        return !state.dead && restoration_of(sheet).has_value();
    default:
        return false;
    }
}

void OriginalModule::temple_service(VitalState &state, const CharacterSheet &sheet,
                                    TempleService service, RandomState &random_state) const
{
    if (!temple_service_helps(sheet, state, service))
    {
        throw std::runtime_error("The temple cannot help with that");
    }
    const auto game = game_of(sheet.identity);
    const auto heal = [&](int dice, int bonus)
    {
        int healing = bonus;
        for (int die = 0; die < dice; ++die)
        {
            healing += roll_die(random_state, 8);
        }
        state.hit_points = std::min(state.hit_points + healing, sheet.hit_points);
    };
    switch (service)
    {
    case TempleService::cure_light_wounds:
        heal(1, 0);
        break;
    case TempleService::cure_serious_wounds:
        heal(2, 1);
        break;
    case TempleService::cure_critical_wounds:
        heal(3, 3);
        break;
    case TempleService::heal:
        // Curse's Heal: all but 1d4 Hit Points (ovr005, temple_heal); Secret's
        // and Treasures' cure too (heal_cures).
        (void)cure(sheet, state, heal_cures(game));
        state.hit_points = std::max(state.hit_points, sheet.hit_points - roll_die(random_state, 4));
        break;
    case TempleService::cure_feeblemind:
        (void)cure(sheet, state, std::array{feebleminded, blind});
        break;
    case TempleService::resurrection:
        // Raised with every Hit Point and its poison gone, losing nothing
        // (magic.pas, SpellResurrection).
        (void)cure(sheet, state, poison_codes);
        stand_unpoisoned(sheet, state);
        state.hit_points = sheet.hit_points;
        break;
    case TempleService::raise_dead:
    {
        // Its poison and its last fight's spells are gone; Gateway raises one
        // poison killed at no cost (its RaiseDead).
        const bool poisoned_dead = has_any(sheet, state, poison_codes);
        auto conditions = conditions_of(sheet, state);
        std::erase_if(conditions,
                      [](const Effect &e)
                      {
                          return std::ranges::find(poison_codes, e.code) != poison_codes.end();
                      });
        state.resources = rested_state(conditions);
        state.dead = false;
        state.hit_points = 1;
        if (poisoned_dead && game == GoldBoxGame::gateway_to_the_savage_frontier)
        {
            stand_unpoisoned(sheet, state);
        }
        break;
    }
    case TempleService::cure_blindness:
        (void)cure(sheet, state, std::array{blind});
        break;
    case TempleService::cure_disease:
        (void)cure(sheet, state, disease_codes(game));
        break;
    case TempleService::neutralize_poison:
        (void)cure(sheet, state, poison_codes);
        if (state.dead)
        {
            stand_unpoisoned(sheet, state);
        }
        break;
    case TempleService::remove_curse:
        (void)cure(sheet, state, std::array{bestow_curse});
        break;
    case TempleService::restoration:
        // Marked for the level the game gives back, as the spell does
        // (temple.pas, Restoration; lasting_changes).
        change_conditions(sheet, state,
                          [](std::vector<Effect> &effects)
                          {
                              if (std::ranges::find(effects, restoring, &Effect::code) ==
                                  effects.end())
                              {
                                  effects.push_back({restoring, 0, 0, false});
                              }
                          });
        return;
    case TempleService::stone_to_flesh:
        // A statue comes back into play okay at 1 Hit Point (temple.pas,
        // StoneToFlesh).
        state.description.clear();
        state.hit_points = 1;
        break;
    default:
        break;
    }
}

// A script's harm as the original deals it (eclvm.pas, EclDamage): an attack
// hits when its d20 is above 1 and the roll with the script's figure tops the
// target's stored Armor Class (fxengine.pas, AttackRollNoAttacker); a saving
// throw of the script's category and modifier avoids the damage
// (RollSavingThrow); the damage takes a member down at 0 and kills it at -10.
HazardAttackResult OriginalModule::hazard_attack(VitalState &state, const CharacterSheet &sheet,
                                                 std::span<const std::string> gear,
                                                 const HazardAttack &attack,
                                                 RandomState &random_state) const
{
    const auto c = character_combatant(sheet, readied(gear));
    HazardAttackResult result;
    result.armor_class = c.armor_class;
    result.hit = true;
    if (!attack.automatic && attack.original_attack >= 0)
    {
        result.natural = roll_die(random_state, 20);
        result.total = result.natural + attack.original_attack;
        result.hit =
            result.natural == 20 || (result.natural > 1 && result.total > 60 - c.armor_class);
    }
    if (!result.hit)
    {
        return result;
    }
    if (attack.original_save >= 0 && attack.original_save < 5)
    {
        const int roll = roll_die(random_state, 20);
        result.save_total = roll + c.save_bonus + attack.original_save_modifier;
        result.saved =
            roll == 20 ||
            (roll > 1 &&
             result.save_total >= c.saving_throws[static_cast<std::size_t>(attack.original_save)]);
        if (result.saved && !attack.half_on_save)
        {
            return result;
        }
    }
    int damage = attack.damage_bonus;
    for (unsigned die = 0; die < attack.dice; ++die)
    {
        const int rolled = roll_die(random_state, static_cast<int>(attack.sides));
        result.damage_dice.push_back(rolled);
        damage += rolled;
    }
    result.damage_rolled = damage;
    result.damage = std::max(result.saved ? damage / 2 : damage, 0);
    const int left = state.hit_points - result.damage;
    state.dead = state.dead || left <= -10;
    state.hit_points = std::max(left, 0);
    return result;
}

// Casting in camp (camp.pas, NoncombatCastMenu): Cure Light Wounds heals a
// member 1d8, and Curse's Cure Serious and Cure Critical Wounds 2d8+1 and
// 3d8+3; Curse's Raise Dead brings back a dead member who is no elf and has
// Constitution left with 1 Hit Point (its cost is revival_changes'); Secret's
// Heal (and Barkskin) heals every Hit Point but 1d4, and its druid's Cure
// Light Wounds 1d8. Cure Blindness, Cure Disease and Remove Curse take their
// conditions away, Heal blindness, disease and feeblemind besides, and
// Curse's Neutralize Poison stands a member poison killed up again. A
// paladin lays on hands and cures disease.
std::vector<CampAction> OriginalModule::camp_actions(const CharacterSheet &sheet,
                                                     const VitalState &state) const
{
    if (state.dead || state.description == stoned_description || !sheet.original)
    {
        return {};
    }
    const auto left = spells_left(sheet, state);
    std::vector<CampAction> actions;
    if (may_lay_on_hands(sheet, state))
    {
        actions.push_back({std::string(lay_on_hands), {"Lay on hands", {}}, false});
    }
    if (cures_left(sheet, state) > 0)
    {
        actions.push_back({std::string(cure_disease_action), {"Cure disease", {}}, false});
    }
    for (const int number : camp_spells)
    {
        if (std::ranges::find(left, number) != left.end())
        {
            actions.push_back(
                {std::string(cast_prefix) + spell_id(number),
                 {"Cast {spell}", {{"spell", std::string(spell(number)->name), true}}},
                 false});
        }
    }
    // The spells whose effects carry into the next fight, those the game's
    // table lets be cast out of one, on the party for a party's spell.
    const auto game = game_of(sheet.identity);
    for (const int number : camp_effect_spells)
    {
        const auto *known = spell(number, game);
        if (std::ranges::find(left, number) != left.end() && known->when != 1)
        {
            actions.push_back({std::string(cast_prefix) + spell_id(number),
                               {"Cast {spell}", {{"spell", std::string(known->name), true}}},
                               known->camp_target == party_target});
        }
    }
    std::vector<int> sights;
    for (const int number : left)
    {
        if (sight_spell(number) && std::ranges::find(sights, number) == sights.end())
        {
            sights.push_back(number);
            actions.push_back(
                {std::string(cast_prefix) + spell_id(number),
                 {"Cast {spell}", {{"spell", std::string(spell(number)->name), true}}},
                 false});
        }
    }
    return actions;
}

void OriginalModule::use_camp_action(const CharacterSheet &user, VitalState &user_state,
                                     const CharacterSheet &target, VitalState &target_state,
                                     std::string_view action, RandomState &random_state) const
{
    const auto game = game_of(user.identity);
    if (action == lay_on_hands || action == cure_disease_action)
    {
        const bool laying = action == lay_on_hands;
        if (user_state.dead || target_state.dead ||
            (laying ? !may_lay_on_hands(user, user_state) : cures_left(user, user_state) <= 0))
        {
            throw std::runtime_error("That cannot be done now");
        }
        change_conditions(user, user_state,
                          [laying](std::vector<Effect> &effects)
                          {
                              if (laying)
                              {
                                  effects.push_back({laid_on_hands, 1440, 0, false});
                                  return;
                              }
                              const auto spent =
                                  std::ranges::find(effects, cured_this_week, &Effect::code);
                              if (spent == effects.end())
                              {
                                  effects.push_back({cured_this_week, week_minutes, 1, false});
                              }
                              else
                              {
                                  ++spent->strength;
                              }
                          });
        if (laying)
        {
            // Two Hit Points a paladin level (PaladinHeal in Curse's reconstruction).
            target_state.hit_points =
                std::min(target_state.hit_points +
                             2 * level_in(original_numbers(user).class_levels, ClassSlot::paladin),
                         target.hit_points);
        }
        else
        {
            (void)cure(target, target_state, disease_codes(game));
        }
        return;
    }
    const auto number =
        action.starts_with(cast_prefix)
            ? spell_number(action.substr(cast_prefix.size()), game_of(user.identity))
            : std::nullopt;
    const auto left = spells_left(user, user_state);
    if (number && std::ranges::find(camp_effect_spells, *number) != camp_effect_spells.end())
    {
        const auto *known = spell(*number, game);
        if (std::ranges::find(left, *number) == left.end() || user_state.dead ||
            target_state.dead || known->when == 1 || known->camp_target == party_target)
        {
            throw std::runtime_error("That spell cannot be cast now");
        }
        spend_spell(user, user_state, *number);
        // A spell on the caster goes on the caster, whomever it was aimed at.
        const bool on_caster = known->camp_target == 1;
        lay_camp_effect(user, on_caster ? user : target, on_caster ? user_state : target_state,
                        *number, random_state);
        return;
    }
    if (number && sight_spell(*number))
    {
        if (std::ranges::find(left, *number) == left.end() || user_state.dead)
        {
            throw std::runtime_error("That spell cannot be cast now");
        }
        spend_spell(user, user_state, *number);
        return;
    }
    if (!number || std::ranges::find(camp_spells, *number) == camp_spells.end() ||
        std::ranges::find(left, *number) == left.end() || user_state.dead)
    {
        throw std::runtime_error("That spell cannot be cast now");
    }
    const bool raising = *number == raise_dead || *number == resurrection;
    const bool unpoisoning = *number == neutralize_poison || *number == druid_neutralize_poison;
    constexpr int elf = static_cast<int>(Race::elf);
    if (raising && (!target_state.dead || original_numbers(target).race == elf ||
                    target.scores[Ability::constitution] <= 0))
    {
        throw std::runtime_error("Raise Dead cannot bring this member back");
    }
    const bool slowing = *number == slow_poison;
    if (!raising && target_state.dead &&
        !((unpoisoning || slowing) && has_any(target, target_state, poison_codes)))
    {
        throw std::runtime_error("The dead cannot be healed");
    }
    spend_spell(user, user_state, *number);
    const auto roll_8s = [&](int dice)
    {
        int total = 0;
        for (int die = 0; die < dice; ++die)
        {
            total += roll_die(random_state, 8);
        }
        return total;
    };
    switch (*number)
    {
    case raise_dead:
        // Raised with poison and the animation of the dead taken away
        // (Curse's SpellRaiseDead).
        target_state.dead = false;
        target_state.hit_points = 1;
        (void)cure(target, target_state, poison_codes);
        return;
    case resurrection:
        // Raised with every Hit Point and its poison gone, losing nothing
        // (magic.pas, SpellResurrection).
        (void)cure(target, target_state, poison_codes);
        stand_unpoisoned(target, target_state);
        target_state.hit_points = target.hit_points;
        return;
    case neutralize_poison:
    case druid_neutralize_poison:
        if (cure(target, target_state, poison_codes) && target_state.dead)
        {
            stand_unpoisoned(target, target_state);
        }
        return;
    case cure_blindness:
        (void)cure(target, target_state, std::array{blind});
        return;
    case restoration:
        // Marked for the level the game gives back (lasting_changes); on one
        // with no level drained it does nothing, as the original's.
        if (restoration_of(target))
        {
            change_conditions(target, target_state,
                              [](std::vector<Effect> &effects)
                              {
                                  if (std::ranges::find(effects, restoring, &Effect::code) ==
                                      effects.end())
                                  {
                                      effects.push_back({restoring, 0, 0, false});
                                  }
                              });
        }
        return;
    case slow_poison:
        hold_poison_back(user, target, target_state);
        return;
    case stone_to_flesh:
        // A statue comes back okay at 1 Hit Point (magic.pas,
        // SpellStoneToFlesh); on a member of flesh the spell is spent.
        if (target_state.description == stoned_description)
        {
            target_state.description.clear();
            target_state.hit_points = 1;
        }
        return;
    case enlarge:
    case reduce:
    case strength_spell:
        strengthen_in_camp(user, target, target_state, *number, random_state);
        return;
    case cure_disease:
    case druid_cure_disease:
        (void)cure(target, target_state, disease_codes(game));
        return;
    case remove_curse:
    case magic_user_remove_curse:
        (void)cure(target, target_state, std::array{bestow_curse});
        return;
    case heal:
    case barkskin:
    {
        auto cured = disease_codes(game);
        cured.insert(cured.end(), {blind, feebleminded});
        (void)cure(target, target_state, cured);
        const int short_of = roll_die(random_state, 4);
        if (target_state.hit_points + short_of <= target.hit_points)
        {
            target_state.hit_points = target.hit_points - short_of;
        }
        return;
    }
    default:
        break;
    }
    const bool one_die = *number == cure_light_wounds || *number == druid_cure_light_wounds;
    const int healed = one_die                          ? roll_8s(1)
                       : *number == cure_serious_wounds ? roll_8s(2) + 1
                                                        : roll_8s(3) + 3;
    target_state.hit_points = std::min(target_state.hit_points + healed, target.hit_points);
}

namespace
{
constexpr int keoghtums_ointment = 58, potion_of_healing = 62, youth = 263;

// Youth used outside a fight (Secret's SpellYouth): its user becomes 1d4+1
// years younger, but not below 18, which the game keeps (lasting_changes).
void make_younger(const CharacterSheet &sheet, VitalState &state, RandomState &random_state)
{
    constexpr int youngest = 18;
    const int years = roll_die(random_state, 4) + 1;
    change_conditions(sheet, state,
                      [&](std::vector<Effect> &effects)
                      {
                          auto age = std::ranges::find(effects, age_in_years, &Effect::code);
                          if (age == effects.end())
                          {
                              effects.push_back(
                                  {age_in_years, 0, original_numbers(sheet).age, false});
                              age = std::prev(effects.end());
                          }
                          age->strength = std::max(youngest, age->strength - years);
                      });
}

// The item power an item of the original's holds, by the spell table's
// numbers, or nothing: in Pool of Radiance a power above 56 is 23 less
// (viewchar.pas, UseItem); Curse names the spell (canonical_spell).
std::optional<int> item_power(std::string_view key, GoldBoxGame game)
{
    const auto record = por::item_of_original_key(key);
    if (!record || record->effect_codes[1] == 0 || record->effect_codes[2] >= 0x80)
    {
        return std::nullopt;
    }
    const int code = record->effect_codes[1];
    if (game != GoldBoxGame::pool_of_radiance)
    {
        return canonical_spell(code & 0x7F, game);
    }
    return code > 56 ? code - 23 : code;
}
} // namespace

// An item of the original's is cursed by its record's flag (ITEMS).
bool OriginalModule::cursed_item(std::string_view item) const
{
    const auto record = por::item_of_original_key(item);
    return record && record->cursed_raw != 0;
}

// Out of a fight the original's Potion of Healing heals 2d4+2, and Keoghtum's
// Ointment takes poison away, else disease, else heals 1d4+8, as in one
// (magic.pas); the other items' powers are a fight's.
bool OriginalModule::usable_outside_combat(std::string_view item) const
{
    const auto power = item_power(item, content_.game);
    return power == potion_of_healing || power == keoghtums_ointment || power == youth;
}

void OriginalModule::drink_potion(const CharacterSheet &sheet, VitalState &state,
                                  std::string_view item, RandomState &random_state) const
{
    if (!usable_outside_combat(item) || state.dead)
    {
        throw std::runtime_error("That cannot be used now");
    }
    if (item_power(item, content_.game) == youth)
    {
        make_younger(sheet, state, random_state);
        return;
    }
    const bool ointment = item_power(item, content_.game) == keoghtums_ointment;
    if (ointment && (cure(sheet, state, poison_codes) ||
                     cure(sheet, state, disease_codes(game_of(sheet.identity)))))
    {
        return;
    }
    const int healing = ointment ? roll_die(random_state, 4) + 8
                                 : roll_die(random_state, 4) + roll_die(random_state, 4) + 2;
    state.hit_points = std::min(state.hit_points + healing, sheet.hit_points);
}

// A party's spell cast in camp (camp.pas, NoncombatCastMenu): Bless,
// Invisibility 10' Radius and the like on every living member, Haste on as
// many as the caster's level.
void OriginalModule::use_party_camp_action(const CharacterSheet &user, VitalState &user_state,
                                           std::span<CampTarget> party, std::string_view action,
                                           RandomState &random_state) const
{
    const auto number =
        action.starts_with(cast_prefix)
            ? spell_number(action.substr(cast_prefix.size()), game_of(user.identity))
            : std::nullopt;
    const auto game = game_of(user.identity);
    const auto left = spells_left(user, user_state);
    if (!number || std::ranges::find(camp_effect_spells, *number) == camp_effect_spells.end() ||
        std::ranges::find(left, *number) == left.end() || user_state.dead ||
        spell(*number, game)->camp_target != party_target || spell(*number, game)->when == 1)
    {
        throw std::runtime_error("That spell cannot be cast now");
    }
    spend_spell(user, user_state, *number);
    constexpr int haste = 48;
    int left_to_hasten = camp_caster_level(user, *spell(*number, game));
    for (auto &member : party)
    {
        if (member.state.get().dead || (*number == haste && left_to_hasten-- <= 0))
        {
            continue;
        }
        lay_camp_effect(user, member.sheet.get(), member.state.get(), *number, random_state);
    }
}

std::vector<CampAction> OriginalModule::scroll_actions(const CharacterSheet &, const VitalState &,
                                                       std::string_view) const
{
    return {};
}

bool OriginalModule::use_scroll(const CharacterSheet &, VitalState &, const CharacterSheet &,
                                VitalState &, std::string_view, std::string_view,
                                RandomState &) const
{
    throw unavailable("use_scroll");
}

bool OriginalModule::use_party_scroll(const CharacterSheet &, VitalState &, std::span<CampTarget>,
                                      std::string_view, std::string_view, RandomState &) const
{
    throw unavailable("use_party_scroll");
}

// A spell's level when it may go into the member's book: one not in it, of a
// class and level the member has slots for (CampScribeMenu).
std::optional<unsigned> OriginalModule::copyable_spell_level(const CharacterSheet &sheet,
                                                             std::string_view id) const
{
    const auto number = spell_number(id);
    const auto *known = number ? spell(*number, game_of(sheet.identity)) : nullptr;
    if (!known || !sheet.original || slots_for(*sheet.original, *known) == 0 ||
        std::ranges::find(sheet.original->known_spells, *number) !=
            sheet.original->known_spells.end())
    {
        return std::nullopt;
    }
    return static_cast<unsigned>(known->level);
}

// The flag on a scroll's spell byte that marks it to be scribed at the next
// rest (camp.pas, C_Spell.PENDING_FLAG).
constexpr std::uint8_t pending_scribe = 0x80;

// The spells a member may scribe from a scroll it carries (camp.pas,
// CampScribeMenu; magic.pas, AddScrollSpells): one awake and alive, a scroll
// it can read (known, or by Read Magic, or a cleric's scroll for a cleric),
// and each spell on it not in its book whose class and level it has slots
// for.
std::vector<ScrollCopy> OriginalModule::scroll_copies(const CharacterSheet &sheet,
                                                      const VitalState &state,
                                                      std::string_view scroll) const
{
    const auto record = por::item_of_original_key(scroll);
    if (!sheet.original || !record || record->type >= content_.item_types.size() || state.dead ||
        state.hit_points <= 0)
    {
        return {};
    }
    constexpr int read_magic = 16, cleric_scroll = 12;
    const int slot = content_.item_types[record->type].worn_location;
    const auto &numbers = *sheet.original;
    const bool cleric = level_in(numbers.class_levels, ClassSlot::cleric) > 0;
    const bool readable = record->hidden_name_words == 0 ||
                          condition(conditions_of(sheet, state), read_magic) ||
                          (cleric && slot == cleric_scroll);
    if (slot < 11 || slot > 13 || !readable)
    {
        return {};
    }
    const auto game = game_of(sheet.identity);
    std::vector<ScrollCopy> copies;
    for (std::size_t at = 0; at < record->effect_codes.size(); ++at)
    {
        // A spell marked already is scribed at the next rest.
        if ((record->effect_codes[at] & pending_scribe) != 0)
        {
            continue;
        }
        const int number = canonical_spell(record->effect_codes[at] & 0x7F, game);
        const auto *known = spell(number, game);
        const auto id = spell_id(number);
        const bool in_book =
            std::ranges::find(numbers.known_spells, number) != numbers.known_spells.end();
        const bool listed = std::ranges::find(copies, id, &ScrollCopy::spell) != copies.end();
        if (!known || id.empty() || in_book || listed || slots_for(numbers, *known) == 0)
        {
            continue;
        }
        copies.push_back({static_cast<unsigned>(at), id, static_cast<unsigned>(known->level),
                          std::string(known->name)});
    }
    return copies;
}

// The spells marked on a scroll to be scribed at the next rest (camp.pas,
// CampScribeMenu's pending flag), as gametime.pas, CheckScribeDone, finds
// them on a readable scroll.
std::vector<ScrollCopy> OriginalModule::pending_scribing(const CharacterSheet &sheet,
                                                         std::string_view scroll) const
{
    const auto record = por::item_of_original_key(scroll);
    if (!sheet.original || !record || record->type >= content_.item_types.size())
    {
        return {};
    }
    const int slot = content_.item_types[record->type].worn_location;
    if (slot < 11 || slot > 13)
    {
        return {};
    }
    const auto game = game_of(sheet.identity);
    std::vector<ScrollCopy> pending;
    for (std::size_t at = 0; at < record->effect_codes.size(); ++at)
    {
        const auto byte = record->effect_codes[at];
        const int number = canonical_spell(byte & 0x7F, game);
        const auto *known = spell(number, game);
        if ((byte & pending_scribe) == 0 || !known || spell_id(number).empty())
        {
            continue;
        }
        pending.push_back({static_cast<unsigned>(at), spell_id(number),
                           static_cast<unsigned>(known->level), std::string(known->name)});
    }
    return pending;
}

// A rest's preparation for its scribing and memorizing (camp.pas,
// CampSpellPrepTime; gametime.pas, AdvanceSpellPrepBase, AdvanceSpellPrep):
// four hours, six with a spell above the second level, shared by all, then
// each spell a quarter hour a level after the one before.
std::vector<std::chrono::minutes>
OriginalModule::preparation_finishes(std::span<const unsigned> levels) const
{
    const bool high = std::ranges::any_of(levels,
                                          [](unsigned level)
                                          {
                                              return level > 2;
                                          });
    std::chrono::minutes done = std::chrono::hours{high ? 6 : 4};
    std::vector<std::chrono::minutes> finishes;
    for (const auto level : levels)
    {
        done += std::chrono::minutes{15 * level};
        finishes.push_back(done);
    }
    return finishes;
}

std::vector<ScrollWriting> OriginalModule::writable_spells(const CharacterSheet &) const
{
    return {};
}

namespace
{
// An exploration spell by its id ("cleric:dispel_magic"), or Knock by the
// name every rules module answers to.
std::optional<int> exploration_spell(std::string_view spell)
{
    constexpr int knock = 31;
    return spell == "knock" ? std::optional<int>{knock} : spell_number(spell);
}
} // namespace

// A spell cast out of a fight, as a script asks for one (SPELL): one the
// member has memorized and not cast.
bool OriginalModule::can_cast_exploration_spell(const CharacterSheet &sheet,
                                                const VitalState &state,
                                                std::string_view spell) const
{
    const auto number = exploration_spell(spell);
    const auto left = spells_left(sheet, state);
    return number && !state.dead && std::ranges::find(left, *number) != left.end();
}

void OriginalModule::cast_exploration_spell(const CharacterSheet &caster, VitalState &state,
                                            std::string_view spell) const
{
    if (!can_cast_exploration_spell(caster, state, spell))
    {
        throw std::runtime_error("That spell cannot be cast now");
    }
    auto left = spells_left(caster, state);
    left.erase(std::ranges::find(left, *exploration_spell(spell)));
    auto numbers = Combatant::decode(state.resources).value_or(character_combatant(caster));
    numbers.spells = std::move(left);
    state.resources = numbers.encode();
}

std::unique_ptr<RulesModule> rules_module(Content content)
{
    return std::make_unique<OriginalModule>(std::move(content));
}

rules::AdvancementChoice romance_refused()
{
    rules::AdvancementChoice change;
    change.rules_data = std::string(romance_refused_entry);
    return change;
}

} // namespace opengold::original
