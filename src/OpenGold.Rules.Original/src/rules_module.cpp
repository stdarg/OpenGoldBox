#include "combat_session.h"
#include "opengold/original.h"
#include "opengold/original_combat.h"
#include "opengold/original_spells.h"
#include "opengold/dice.h"

#include <algorithm>
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
// shield, armor, gauntlets to boots in their own places, rings two at most;
// anything else (arrows, quarrels, wands, scrolls) only takes its hands.
constexpr std::uint8_t weapon_place = 0, shield_place = 1, armor_place = 2, boots_place = 8,
                       ring_place = 9;
constexpr std::uint8_t arrows_type = 73, quarrels_type = 28;

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

// The classes the character would train in with this much experience.
std::vector<ClassSlot> ready_classes(const CharacterSheet &sheet, unsigned experience)
{
    const auto &numbers = original_numbers(sheet);
    const auto capped = std::min<unsigned>(experience, std::numeric_limits<int>::max());
    return classes_ready_to_train(static_cast<Race>(numbers.race), numbers.class_levels,
                                  sheet.scores, static_cast<int>(capped), game_of(sheet.identity));
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

// The slots a character has for a spell's class and level.
int slots_for(const CharacterSheet::Original &numbers, const Spell &known)
{
    const auto &slots =
        known.caster_class == SpellClass::cleric ? numbers.cleric_slots : numbers.magic_user_slots;
    return known.level >= 1 && known.level <= 7 ? slots[static_cast<std::size_t>(known.level - 1)]
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
    numbers.cleric_slots =
        spell_slots(ClassSlot::cleric, level_in(levels, ClassSlot::cleric),
                    sheet.scores[Ability::wisdom], sheet.scores[Ability::intelligence], game);
    numbers.magic_user_slots =
        spell_slots(ClassSlot::magic_user, level_in(levels, ClassSlot::magic_user),
                    sheet.scores[Ability::wisdom], sheet.scores[Ability::intelligence], game);
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
    if (game == GoldBoxGame::secret_of_the_silver_blades)
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
    for (const auto &id : sheet.prepared_spells)
    {
        if (const auto number = spell_number(id))
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
    constexpr int dwarf = 1, elf = 2, gnome = 3, half_elf = 4, halfling = 5;
    constexpr int dwarf_attack = 26, gnome_attack = 18, giant_defense = 47, bugbear_defense = 48;
    const auto innate = [&c](int code)
    {
        c.effects.push_back({code, 0, 255, false});
    };
    if (c.race == dwarf || c.race == halfling)
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
    if (c.race == dwarf || c.race == gnome)
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
    // (gdconsts.pas, coinValues).
    [[nodiscard]] std::array<std::uint64_t, 5> coin_worth() const override
    {
        return {1, 10, 100, 200, 1000};
    }
    void drain_character(CharacterSheet &sheet, VitalState &state, const Drain &drain) const;
    void raise_character(CharacterSheet &sheet, VitalState &state, Revival by) const;
    [[nodiscard]] unsigned experience_share(const CharacterSheet &sheet,
                                            unsigned award) const override;
    void recover(VitalState &state, const CharacterSheet &sheet) const override;
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

    [[nodiscard]] bool offers_temple_service(TempleService service) const override
    {
        return service != TempleService::resurrection;
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
};

} // namespace

Identity OriginalModule::identity() const
{
    return original::identity(content_.game);
}

std::unique_ptr<CombatSession> OriginalModule::create(Encounter encounter, std::uint64_t seed) const
{
    return detail::create_session(identity(), std::move(encounter), seed, content_);
}

std::unique_ptr<CombatSession> OriginalModule::restore(std::string_view checkpoint) const
{
    return detail::restore_session(identity(), checkpoint);
}

CharacterProfile OriginalModule::character_profile(const CharacterSheet &sheet,
                                                   std::span<const std::string> gear) const
{
    const auto carried = readied(gear);
    const auto combatant = character_combatant(sheet, carried);
    CharacterProfile profile;
    profile.data = combatant.encode();
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
// The usability bit of each class slot (gdconsts.pas, usabilityBit).
constexpr std::array<std::uint8_t, class_slot_count> usability_bits{1 << 1, 1 << 4, 1 << 3, 1 << 6,
                                                                    1 << 7, 1 << 0, 1 << 2, 1 << 5};

// Secret's items mark rangers with the paladin's bit (its classMaskTbl).
std::uint8_t usability(const CharacterSheet &sheet)
{
    const bool secret = game_of(sheet.identity) == GoldBoxGame::secret_of_the_silver_blades;
    std::uint8_t bits = 0;
    const auto &levels = original_numbers(sheet).class_levels;
    for (std::size_t at = 0; at < class_slot_count; ++at)
    {
        if (levels[at] > 0)
        {
            const bool ranger = static_cast<ClassSlot>(at) == ClassSlot::ranger;
            bits |= secret && ranger ? usability_bits[static_cast<std::size_t>(ClassSlot::paladin)]
                                     : usability_bits[at];
        }
    }
    return bits;
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
    if ((chosen.type.class_restrictions & usability(sheet)) == 0)
    {
        throw std::runtime_error("Wrong class");
    }
    unsigned hands = chosen.type.hands, rings = 0;
    const auto place = chosen.type.worn_location;
    for (unsigned n = 0; n < candidates.size(); ++n)
    {
        if (n == selected)
        {
            continue;
        }
        const auto other = item_for(candidates[n]);
        hands += other.type.hands;
        const auto other_place = other.type.worn_location;
        rings += other_place == ring_place ? 1 : 0;
        const bool same_place = place <= boots_place && other_place == place;
        const bool same_stack =
            (chosen.item.type == arrows_type || chosen.item.type == quarrels_type) &&
            other.item.type == chosen.item.type;
        if (same_place || same_stack)
        {
            throw std::runtime_error("Already using another item there");
        }
        change.indices.push_back(n);
    }
    if (place == ring_place && rings >= 2)
    {
        throw std::runtime_error("Already using another item there");
    }
    if (hands > 2)
    {
        throw std::runtime_error("Your hands are full!");
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
    for (const auto &slots : {numbers.cleric_slots, numbers.magic_user_slots})
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
        const auto *chosen = spell(*spell_number(id));
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

void OriginalModule::train_at_hall(CharacterSheet &, std::string_view) const
{
    throw unavailable("train_at_hall");
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
    if (magic_user == 0)
    {
        return options;
    }
    const auto slots =
        spell_slots(ClassSlot::magic_user, magic_user + 1, sheet.scores[Ability::wisdom],
                    sheet.scores[Ability::intelligence], game_of(sheet.identity));
    for (const int number : castable_spells(game_of(sheet.identity)))
    {
        const auto *offered = spell(number);
        if (offered->caster_class == SpellClass::magic_user && offered->level <= 5 &&
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
    const auto training = decode_training(choice.rules_data);
    // The spell a magic-user learns, when the level is one of its own.
    const bool magic_user_trained =
        (training.classes & (1U << static_cast<unsigned>(ClassSlot::magic_user))) != 0;
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
        next.original->known_spells.push_back(*spell_number(id));
    }
    auto &levels = next.original->class_levels;
    for (std::size_t at = 0; at < class_slot_count; ++at)
    {
        if ((training.classes & (1U << at)) == 0)
        {
            continue;
        }
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
    const int gain =
        training_hit_points(static_cast<Profession>(numbers.profession), training.rolled,
                            next.scores[Ability::constitution], game_of(next.identity));
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

// A raised character loses a point of Constitution while it has one
// (temple.pas, RaiseDead; Curse's cast_raise). A temple's raising also takes
// Hit Points off its maximum (raised_hit_point_loss); the spell's does not.
void OriginalModule::raise_character(CharacterSheet &sheet, VitalState &state, Revival by) const
{
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

std::vector<AdvancementChoice> OriginalModule::lasting_changes(const CharacterSheet &sheet,
                                                               const VitalState &after) const
{
    const auto last = Combatant::decode(after.resources);
    if (after.resources.empty() || !last || !sheet.original)
    {
        return {};
    }
    Drain drain;
    bool drained = false;
    for (std::size_t at = 0; at < class_slot_count; ++at)
    {
        drain.levels[at] = std::max(0, sheet.original->class_levels[at] - last->class_levels[at]);
        drained = drained || drain.levels[at] > 0;
    }
    if (!drained)
    {
        return {};
    }
    drain.hit_points = std::max(0, sheet.hit_points - last->max_hit_points);
    AdvancementChoice change;
    change.rules_data = encode_drain(drain);
    return {change};
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
// CheckRestingHeal: one every 288 five-minute ticks).
void OriginalModule::recover(VitalState &state, const CharacterSheet &sheet) const
{
    if (!state.dead)
    {
        state.hit_points = std::min(state.hit_points + 1, sheet.hit_points);
    }
    // The day's rest memorizes again the spells cast: the next fight starts
    // from the sheet's.
    state.resources.clear();
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
// Hit Point. Its cures of blindness, disease, poison, curses and stone come
// with the conditions that last outside fights; it raises no one by
// Resurrection.
bool OriginalModule::temple_service_helps(const CharacterSheet &sheet, const VitalState &state,
                                          TempleService service) const
{
    switch (service)
    {
    case TempleService::cure_light_wounds:
    case TempleService::cure_serious_wounds:
    case TempleService::cure_critical_wounds:
    case TempleService::heal:
        return !state.dead && state.hit_points < sheet.hit_points;
    case TempleService::raise_dead:
        return state.dead;
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
        // Curse's Heal: all but 1d4 Hit Points (ovr005, temple_heal).
        state.hit_points = std::max(state.hit_points, sheet.hit_points - roll_die(random_state, 4));
        break;
    case TempleService::raise_dead:
        state.dead = false;
        state.hit_points = 1;
        state.resources.clear();
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
        if (result.saved)
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
    result.damage = std::max(damage, 0);
    const int left = state.hit_points - result.damage;
    state.dead = state.dead || left <= -10;
    state.hit_points = std::max(left, 0);
    return result;
}

namespace
{
constexpr int cure_light_wounds = 3, cure_serious_wounds = 158, cure_critical_wounds = 171,
              raise_dead = 175;
// The spells a member may cast in camp: the cures, and Curse's Raise Dead.
constexpr std::array<int, 4> camp_spells{cure_light_wounds, cure_serious_wounds,
                                         cure_critical_wounds, raise_dead};
constexpr std::string_view cast_prefix = "cast:";
// A paladin's laying on of hands, once between a day's rests; the member's
// state keeps that it was used (as the paladinDailyHealCast effect).
constexpr std::string_view lay_on_hands = "paladin:lay_on_hands";
constexpr int laid_on_hands = 340;

// The spells a member has memorized and not cast: its last fight's, or after
// a day's rest its sheet's.
std::vector<int> spells_left(const CharacterSheet &sheet, const VitalState &state)
{
    if (!state.resources.empty())
    {
        if (const auto last = Combatant::decode(state.resources))
        {
            return last->spells;
        }
    }
    std::vector<int> memorized;
    for (const auto &id : sheet.prepared_spells)
    {
        if (const auto number = spell_number(id))
        {
            memorized.push_back(*number);
        }
    }
    return memorized;
}
} // namespace

// Casting in camp (camp.pas, NoncombatCastMenu): Cure Light Wounds heals a
// member 1d8, and Curse's Cure Serious and Cure Critical Wounds 2d8+1 and
// 3d8+3; Curse's Raise Dead brings back a dead member who is no elf and has
// Constitution left with 1 Hit Point (its cost is revival_changes'). The spells whose effects last
// beyond a fight come with the conditions (plan step 6).
std::vector<CampAction> OriginalModule::camp_actions(const CharacterSheet &sheet,
                                                     const VitalState &state) const
{
    if (state.dead || !sheet.original)
    {
        return {};
    }
    const auto left = spells_left(sheet, state);
    std::vector<CampAction> actions;
    const auto last = Combatant::decode(state.resources);
    const bool laid = last && std::ranges::any_of(last->effects,
                                                  [](const Effect &e)
                                                  {
                                                      return e.code == laid_on_hands;
                                                  });
    if (level_in(sheet.original->class_levels, ClassSlot::paladin) > 0 && !laid)
    {
        actions.push_back({std::string(lay_on_hands), {"Lay on hands", {}}, false});
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
    return actions;
}

void OriginalModule::use_camp_action(const CharacterSheet &user, VitalState &user_state,
                                     const CharacterSheet &target, VitalState &target_state,
                                     std::string_view action, RandomState &random_state) const
{
    if (action == lay_on_hands)
    {
        // Two Hit Points a paladin level (PaladinHeal in Curse's reconstruction).
        const auto offered = camp_actions(user, user_state);
        if (user_state.dead || target_state.dead ||
            std::ranges::none_of(offered,
                                 [](const CampAction &a)
                                 {
                                     return a.id == lay_on_hands;
                                 }))
        {
            throw std::runtime_error("That cannot be done now");
        }
        auto numbers = Combatant::decode(user_state.resources).value_or(character_combatant(user));
        numbers.spells = spells_left(user, user_state);
        numbers.effects.push_back({laid_on_hands, 1440, 0, false});
        user_state.resources = numbers.encode();
        target_state.hit_points =
            std::min(target_state.hit_points +
                         2 * level_in(original_numbers(user).class_levels, ClassSlot::paladin),
                     target.hit_points);
        return;
    }
    const auto number = action.starts_with(cast_prefix)
                            ? spell_number(action.substr(cast_prefix.size()))
                            : std::nullopt;
    auto left = spells_left(user, user_state);
    const auto copy = number ? std::ranges::find(left, *number) : left.end();
    if (!number || std::ranges::find(camp_spells, *number) == camp_spells.end() ||
        copy == left.end() || user_state.dead)
    {
        throw std::runtime_error("That spell cannot be cast now");
    }
    const bool raising = *number == raise_dead;
    constexpr int elf = static_cast<int>(Race::elf);
    if (raising && (!target_state.dead || original_numbers(target).race == elf ||
                    target.scores[Ability::constitution] <= 0))
    {
        throw std::runtime_error("Raise Dead cannot bring this member back");
    }
    if (!raising && target_state.dead)
    {
        throw std::runtime_error("The dead cannot be healed");
    }
    left.erase(copy);
    auto numbers = Combatant::decode(user_state.resources).value_or(character_combatant(user));
    numbers.spells = std::move(left);
    user_state.resources = numbers.encode();
    if (raising)
    {
        target_state.dead = false;
        target_state.hit_points = 1;
        return;
    }
    const auto roll_8s = [&](int dice)
    {
        int total = 0;
        for (int die = 0; die < dice; ++die)
        {
            total += roll_die(random_state, 8);
        }
        return total;
    };
    const int healed = *number == cure_light_wounds     ? roll_8s(1)
                       : *number == cure_serious_wounds ? roll_8s(2) + 1
                                                        : roll_8s(3) + 3;
    target_state.hit_points = std::min(target_state.hit_points + healed, target.hit_points);
}

namespace
{
constexpr int keoghtums_ointment = 58, potion_of_healing = 62;

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

// Out of a fight the original's Potion of Healing heals 2d4+2 and Keoghtum's
// Ointment 1d4+8, as in one (magic.pas); the other items' powers are a
// fight's. The ointment's cure of poison and disease comes with the
// conditions (plan step 6).
bool OriginalModule::usable_outside_combat(std::string_view item) const
{
    const auto power = item_power(item, content_.game);
    return power == potion_of_healing || power == keoghtums_ointment;
}

void OriginalModule::drink_potion(const CharacterSheet &sheet, VitalState &state,
                                  std::string_view item, RandomState &random_state) const
{
    if (!usable_outside_combat(item) || state.dead)
    {
        throw std::runtime_error("That cannot be used now");
    }
    const int healing = item_power(item, content_.game) == potion_of_healing
                            ? roll_die(random_state, 4) + roll_die(random_state, 4) + 2
                            : roll_die(random_state, 4) + 8;
    state.hit_points = std::min(state.hit_points + healing, sheet.hit_points);
}

void OriginalModule::use_party_camp_action(const CharacterSheet &, VitalState &,
                                           std::span<CampTarget>, std::string_view,
                                           RandomState &) const
{
    throw unavailable("use_party_camp_action");
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

std::optional<unsigned> OriginalModule::copyable_spell_level(const CharacterSheet &,
                                                             std::string_view) const
{
    return std::nullopt;
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

} // namespace opengold::original
