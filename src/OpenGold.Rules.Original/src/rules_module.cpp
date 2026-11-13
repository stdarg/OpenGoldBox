#include "combat_session.h"
#include "opengold/original.h"
#include "opengold/original_combat.h"

#include <algorithm>
#include <limits>
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
                                  sheet.scores[Ability::strength], static_cast<int>(capped));
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

// The sheet's numbers that follow from its class levels, worked out again.
void recount_levels(CharacterSheet &sheet)
{
    auto &numbers = *sheet.original;
    const auto &levels = numbers.class_levels;
    numbers.thac0 = thac0(levels);
    numbers.saving_throws = saving_throws(levels);
    numbers.thief_skills =
        thief_skills(static_cast<Race>(numbers.race), level_in(levels, ClassSlot::thief),
                     sheet.scores[Ability::dexterity]);
    numbers.cleric_slots = spell_slots(ClassSlot::cleric, level_in(levels, ClassSlot::cleric),
                                       sheet.scores[Ability::wisdom]);
    numbers.magic_user_slots =
        spell_slots(ClassSlot::magic_user, level_in(levels, ClassSlot::magic_user),
                    sheet.scores[Ability::wisdom]);
}

// A character's numbers from its sheet. Unarmed, as the original's characters
// start: one attack a round (three in two rounds for a fighter above level 6)
// of 1d2, with Strength's bonuses (charhlp.pas, RecalcStats).
Combatant character_combatant(const CharacterSheet &sheet)
{
    if (!sheet.original)
    {
        throw std::runtime_error("This character was not made under the original rules");
    }
    const auto &original = *sheet.original;
    const auto strength = sheet.scores[Ability::strength];
    const auto dexterity = sheet.scores[Ability::dexterity];
    Combatant c;
    c.thac0 = original.thac0 - strength_to_hit_bonus(strength, original.exceptional_strength);
    c.armor_class = 10 - dexterity_armor_class_bonus(dexterity);
    // Without Dexterity, and 2 worse, as the original keeps it (cbtacts.pas).
    c.rear_armor_class = 12;
    c.max_hit_points = c.hit_points = std::max(sheet.hit_points, 1);
    c.movement = 12;
    const auto fighter = original.class_levels[static_cast<std::size_t>(ClassSlot::fighter)];
    c.attacks[0] = {fighter > 6 ? 3 : 2, 1, 2,
                    strength_damage_bonus(strength, original.exceptional_strength)};
    c.saving_throws = original.saving_throws;
    c.level = *std::max_element(original.class_levels.begin(), original.class_levels.end());
    c.class_levels = original.class_levels;
    c.attack_level = fighter > 0 ? fighter : 1;
    c.initiative_bonus = dexterity_missile_bonus(dexterity);
    c.intelligence = sheet.scores[Ability::intelligence];
    c.strength = strength;
    // The original's race numbers, 1 (dwarf) to 7 (human).
    constexpr std::array<std::string_view, 7> races{"dwarf",    "elf",      "gnome", "half_elf",
                                                    "halfling", "half_orc", "human"};
    const auto race = std::find(races.begin(), races.end(), sheet.race);
    c.race = race == races.end() ? 7 : static_cast<int>(race - races.begin()) + 1;
    c.constitution = sheet.scores[Ability::constitution];
    // The innate effects each race gets at creation (chargen.pas): the
    // Constitution saves against poison (90) and against spells and wands
    // (97), and the elf's and half-elf's resistance to sleep and charm (107, 124).
    constexpr int dwarf = 1, elf = 2, gnome = 3, half_elf = 4, halfling = 5;
    // A dwarf's and a gnome's bonus against their foes (26, 18), their
    // defense against giants (47), and a gnome's against bugbears (48).
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
    c.thief_level = original.class_levels[static_cast<std::size_t>(ClassSlot::thief)];
    // Unarmed and unarmored, a thief can backstab.
    c.backstabs = c.thief_level > 0;
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
    void roll_advancement(const CharacterSheet &sheet, unsigned experience,
                          AdvancementChoice &choice, RandomState &random_state) const override;
    [[nodiscard]] unsigned experience_after_advance(const CharacterSheet &before,
                                                    unsigned experience) const override;
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
    Content content_;
};

} // namespace

Identity OriginalModule::identity() const
{
    return original::identity();
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
    if (!gear.empty())
    {
        throw unavailable("Gear");
    }
    const auto combatant = character_combatant(sheet);
    CharacterProfile profile;
    profile.data = combatant.encode();
    profile.hit_points = combatant.max_hit_points;
    profile.armor_class = combatant.armor_class;
    profile.movement_feet = combatant.movement * 5;
    return profile;
}

unsigned OriginalModule::experience_for_level(unsigned level) const
{
    // The fighter's table, as the party screen shows a single figure.
    const auto needed = original::experience_for_level(ClassSlot::fighter, static_cast<int>(level));
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

EquipmentInfo OriginalModule::equipment_info(std::string_view) const
{
    throw unavailable("equipment_info");
}

unsigned OriginalModule::carrying_capacity_pounds(const CharacterSheet &) const
{
    throw unavailable("carrying_capacity_pounds");
}

bool OriginalModule::can_use_item(const CharacterSheet &, std::string_view) const
{
    throw unavailable("can_use_item");
}

std::vector<EquipmentChoice> OriginalModule::equipment_choices(const CharacterSheet &,
                                                               std::span<const std::string>,
                                                               unsigned) const
{
    throw unavailable("equipment_choices");
}

EquipmentChange OriginalModule::equipment_change(const CharacterSheet &,
                                                 std::span<const std::string>, unsigned,
                                                 EquipmentOperation) const
{
    throw unavailable("equipment_change");
}

GearRating OriginalModule::gear_rating(const CharacterSheet &, std::span<const std::string>,
                                       int) const
{
    throw unavailable("gear_rating");
}

std::optional<WeaponAttackView> OriginalModule::weapon_attack(const CharacterSheet &,
                                                              std::span<const std::string>) const
{
    throw unavailable("weapon_attack");
}

bool OriginalModule::suits(const CharacterSheet &, std::string_view) const
{
    throw unavailable("suits");
}

SpellAccess OriginalModule::spell_access(const CharacterSheet &) const
{
    throw unavailable("spell_access");
}

SpellChoiceOptions OriginalModule::spell_choice_options(const CharacterSheet &,
                                                        SpellChoiceContext) const
{
    throw unavailable("spell_choice_options");
}

void OriginalModule::apply_spell_choices(CharacterSheet &, const SpellChoices &, SpellChoiceContext,
                                         ChoiceCompleteness) const
{
    throw unavailable("apply_spell_choices");
}

AbilityCheckModifier OriginalModule::ability_check(const CharacterSheet &,
                                                   std::span<const std::string>, Ability,
                                                   std::string_view) const
{
    throw unavailable("ability_check");
}

unsigned OriginalModule::climbing_chance(const CharacterSheet &, const VitalState &,
                                         std::span<const std::string>) const
{
    throw unavailable("climbing_chance");
}

unsigned OriginalModule::check_chance(const CharacterSheet &, const VitalState &,
                                      std::span<const std::string>, Ability, std::string_view,
                                      int) const
{
    throw unavailable("check_chance");
}

AbilityCheckRoll OriginalModule::roll_ability_check(const CharacterSheet &, const VitalState &,
                                                    std::span<const std::string>, Ability,
                                                    std::string_view, RandomState &) const
{
    throw unavailable("roll_ability_check");
}

std::vector<TrainingReplacementOptions>
OriginalModule::rest_training_options(const CharacterSheet &, RestKind) const
{
    throw unavailable("rest_training_options");
}

TrainingChoices OriginalModule::replace_rest_training(CharacterSheet &,
                                                      std::span<const std::string>) const
{
    throw unavailable("replace_rest_training");
}

std::vector<TrainingChoiceGroup> OriginalModule::training_options(const CharacterSheet &) const
{
    throw unavailable("training_options");
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
    if (!ready_classes(sheet, std::numeric_limits<unsigned>::max()).empty())
    {
        options.level = static_cast<unsigned>(sheet.level) + 1;
        options.class_level = options.level;
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

void OriginalModule::roll_advancement(const CharacterSheet &sheet, unsigned experience,
                                      AdvancementChoice &choice, RandomState &random_state) const
{
    const auto trained = ready_classes(sheet, experience);
    if (trained.empty())
    {
        throw std::runtime_error("This character is not ready to train");
    }
    Training training;
    for (const auto slot : trained)
    {
        training.classes |= 1U << static_cast<unsigned>(slot);
    }
    training.rolled = roll_training_hit_dice(trained, random_state);
    choice.rules_data = encode_training(training);
}

bool OriginalModule::advance_character(CharacterSheet &sheet, VitalState &state,
                                       const AdvancementChoice &choice) const
{
    const auto &numbers = original_numbers(sheet);
    const auto training = decode_training(choice.rules_data);
    auto next = sheet;
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
    }
    recount_levels(next);
    const int gain = training_hit_points(static_cast<Profession>(numbers.profession),
                                         training.rolled, next.scores[Ability::constitution]);
    next.hit_points += gain;
    ++next.level;
    sheet = std::move(next);
    state.hit_points += gain;
    return true;
}

unsigned OriginalModule::experience_after_advance(const CharacterSheet &before,
                                                  unsigned experience) const
{
    const auto trained = ready_classes(before, experience);
    const auto capped = std::min<unsigned>(experience, std::numeric_limits<int>::max());
    return static_cast<unsigned>(experience_after_training(original_numbers(before).class_levels,
                                                           trained, static_cast<int>(capped)));
}

unsigned OriginalModule::experience_share(const CharacterSheet &sheet, unsigned award) const
{
    const auto capped = std::min<unsigned>(award, std::numeric_limits<int>::max());
    return static_cast<unsigned>(
        original::experience_share(static_cast<Profession>(original_numbers(sheet).profession),
                                   sheet.scores, static_cast<int>(capped)));
}

void OriginalModule::recover(VitalState &, const CharacterSheet &) const
{
    throw unavailable("recover");
}

RecoveryInfo OriginalModule::recovery_info(const CharacterSheet &, const VitalState &) const
{
    throw unavailable("recovery_info");
}

void OriginalModule::grant_temporary_hit_points(VitalState &, const CharacterSheet &,
                                                const TemporaryHitPoints &, TemporaryHpChoice) const
{
    throw unavailable("grant_temporary_hit_points");
}

void OriginalModule::recover_short_rest(VitalState &, const CharacterSheet &) const
{
    throw unavailable("recover_short_rest");
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

RestPolicy OriginalModule::long_rest_policy() const
{
    throw unavailable("long_rest_policy");
}

RestPolicy OriginalModule::short_rest_policy() const
{
    throw unavailable("short_rest_policy");
}

bool OriginalModule::temple_service_helps(const CharacterSheet &, const VitalState &,
                                          TempleService) const
{
    throw unavailable("temple_service_helps");
}

void OriginalModule::temple_service(VitalState &, const CharacterSheet &, TempleService,
                                    RandomState &) const
{
    throw unavailable("temple_service");
}

HazardAttackResult OriginalModule::hazard_attack(VitalState &, const CharacterSheet &,
                                                 std::span<const std::string>, const HazardAttack &,
                                                 RandomState &) const
{
    throw unavailable("hazard_attack");
}

std::vector<CampAction> OriginalModule::camp_actions(const CharacterSheet &,
                                                     const VitalState &) const
{
    throw unavailable("camp_actions");
}

void OriginalModule::use_camp_action(const CharacterSheet &, VitalState &, const CharacterSheet &,
                                     VitalState &, std::string_view, RandomState &) const
{
    throw unavailable("use_camp_action");
}

void OriginalModule::drink_potion(const CharacterSheet &, VitalState &, std::string_view,
                                  RandomState &) const
{
    throw unavailable("drink_potion");
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
    throw unavailable("scroll_actions");
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
    throw unavailable("copyable_spell_level");
}

std::vector<ScrollWriting> OriginalModule::writable_spells(const CharacterSheet &) const
{
    throw unavailable("writable_spells");
}

bool OriginalModule::can_cast_exploration_spell(const CharacterSheet &, const VitalState &,
                                                std::string_view) const
{
    throw unavailable("can_cast_exploration_spell");
}

void OriginalModule::cast_exploration_spell(const CharacterSheet &, VitalState &,
                                            std::string_view) const
{
    throw unavailable("cast_exploration_spell");
}

std::unique_ptr<RulesModule> rules_module(Content content)
{
    return std::make_unique<OriginalModule>(std::move(content));
}

} // namespace opengold::original
