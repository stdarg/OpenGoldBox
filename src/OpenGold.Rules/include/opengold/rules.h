#ifndef OPENGOLD_RULES_H
#define OPENGOLD_RULES_H
#include "opengold/message.h"
#include <compare>
#include <array>
#include <cstdint>
#include <memory>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace opengold::rules
{
struct CharacterSheet;
struct AbilityCheckModifier;
struct FeatureGrant;

struct CreationChoice
{
    std::string id, label, description;
};

using TrainingChoices = std::map<std::string, std::vector<std::string>>;
enum class TrainingChoiceControl
{
    checkboxes,
    single_selection
};

struct TrainingChoiceGroup
{
    std::string id, label;
    unsigned count{};
    std::vector<CreationChoice> options;
    TrainingChoiceControl control{TrainingChoiceControl::checkboxes};
    std::string continuity_id; // Same choice purpose across changing source entitlements.
    unsigned acquired_level{1};
};
enum class SpellChoiceContext
{
    pending,
    advancement,
    long_rest
};

struct SpellChoices
{
    TrainingChoices learning;
    std::optional<std::vector<std::string>> prepared;
    std::string replace_cantrip, replacement;
    bool operator==(const SpellChoices &) const = default;
};

struct SpellChoiceOptions
{
    std::vector<TrainingChoiceGroup> learning;
    std::vector<CreationChoice> preparation, replaceable, replacements;
    std::vector<std::string> locked_prepared;
    unsigned prepared_count{};
    bool may_prepare{}, may_replace{};
};

struct AdvancementChoice
{
    std::string feat;
    std::array<unsigned, 6> abilities{};
    std::vector<std::string> spells;
    TrainingChoices training;
    std::optional<TrainingChoices> spell_learning; // Absent only for historical advancement replay.
    bool operator==(const AdvancementChoice &) const = default;
};

struct AdvancementOption
{
    std::string id, label, description;
    bool available{true};
};

struct AdvancementOptions
{
    unsigned level{};
    std::vector<AdvancementOption> feats, spells;
    std::vector<TrainingChoiceGroup> training;
    std::string description;
};

struct LearnedSpell
{
    std::string id, label, source_id;
    unsigned acquired_level{};
    bool operator==(const LearnedSpell &) const = default;
};

// Derived from sourced grants and the selected preparation. Unfilled choices
// remain explicit; an incomplete catalog never erases an entitlement.
struct SpellAccess
{
    std::vector<LearnedSpell> cantrips, spellbook;
    std::vector<std::string> prepared;
    unsigned cantrip_choices{}, spellbook_choices{}, prepared_choices{};
};

// Zero selects the equipped weapon's minimum required hands.
struct EquipmentState
{
    unsigned weapon_hands{};
    bool operator==(const EquipmentState &) const = default;
};

struct GripOption
{
    unsigned hands{};
    Message label;
    bool available{true};
};

struct CharacterProfile
{
    std::string data;
    int hit_points{}, armor_class{};
    std::string description;
    int movement_feet{}, melee_attack_bonus{};
    std::string item_modifiers, spell_modifiers;
    bool strength_dexterity_disadvantage{};
    std::vector<Message> item_messages, spell_messages;
    EquipmentState equipment;
    std::vector<GripOption> grips;
};
enum class EquipmentSlot
{
    unsupported,
    weapon,
    armor,
    shield,
    carried
};

struct EquipmentInfo
{
    EquipmentSlot slot{EquipmentSlot::unsupported};
    unsigned hands{};
};

// Module-owned continuation, separate from encounter turn budgets.
struct VitalState
{
    int hit_points{};
    bool dead{};
    std::string resources;
    std::string description;
    bool operator==(const VitalState &) const = default;
};

struct TemporaryHitPoints
{
    int amount{};
    std::string source_id;
    bool operator==(const TemporaryHitPoints &) const = default;
};
enum class TemporaryHpChoice
{
    keep_current,
    use_new
};
// Engine-independent rest events and continuation. No campaign IDs or UI state.
enum class RestKind
{
    short_rest,
    long_rest
};
enum class RestWork
{
    sleep,
    light_activity,
    exertion
};
enum class RestInterruption
{
    initiative,
    spell,
    damage,
    exertion
};

struct RestProgress
{
    RestKind kind{};
    std::uint64_t elapsed_milliseconds{}, segment_milliseconds{}, sleep_milliseconds{},
        light_milliseconds{};
    std::uint64_t exertion_milliseconds{}, extension_milliseconds{};
    bool interrupted{};
    RestWork work{RestWork::sleep};
    RestInterruption interruption{RestInterruption::initiative};
};
enum class RestBenefit
{
    none,
    short_rest,
    long_rest
};

struct RestTransition
{
    std::optional<RestProgress> progress;
    RestBenefit benefit{RestBenefit::none};
    std::uint64_t completed_duration_milliseconds{};
};

struct RestPolicy
{
    unsigned duration_minutes{}, wait_after_rest_minutes{};
    unsigned minimum_sleep_minutes{}, maximum_light_minutes{}, interruption_extension_minutes{},
        exertion_limit_minutes{};
};

struct ResourcePool
{
    std::string id;
    Message label;
    unsigned remaining{}, capacity{}, short_rest_recovery{};
};

struct RestRecoveryChoice
{
    std::string id;
    Message label;
};

struct RecoveryInfo
{
    unsigned hit_die{}, hit_dice{}, hit_dice_max{};
    bool can_rest{};
    std::vector<ResourcePool> resources;
    TemporaryHitPoints temporary_hp;
    std::vector<RestRecoveryChoice> choices;
};

struct HitDieResult
{
    unsigned die{};
    int roll{}, modifier{}, healing{};
    unsigned remaining{};
};

using EntityId = std::uint32_t;

struct Cell
{
    int x{}, y{};
    auto operator<=>(const Cell &) const = default;
};

struct Battlefield
{
    int width{}, height{};
    // Geometry facts: 0=open, 1=opaque obstacle, 2=difficult terrain.
    std::vector<std::uint8_t> terrain;
    [[nodiscard]] bool contains(Cell p) const noexcept;
    [[nodiscard]] unsigned at(Cell p) const noexcept;
};

// Stable encounter item identity references the original participant and equipment
// ordinal. Campaign adapters map these to inventory identities, never rule code.
struct HeldItemView
{
    unsigned id{};
    EntityId origin{}, holder{}; // Holder zero means on the ground.
    unsigned equipment_index{};
    std::string definition;
    Message label;
    Cell cell;
    std::uint64_t inventory_id{}; // Original source identity; zero for legacy/profile equipment.
    unsigned quantity{1};         // A held stack means one held unit and the remainder carried.
    bool stowed{};
};

struct CarriedEquipment
{
    std::uint64_t inventory_id{};
    std::string definition;
    unsigned quantity{};
    int equipment_index{-1};
};

struct Participant
{
    EntityId id{};
    std::string definition, name;
    unsigned side{}; // 0=party, 1=opposition in this first encounter adapter.
    Cell cell;
    std::string character_profile;
    std::optional<VitalState> state;
    bool surprised{}; // The rules module determines the mechanical effect.
    bool facing_left{};
    // Initial equipment ordinals already on the ground at this participant's
    // encounter position. Combat checkpoints persist their resulting item state.
    std::vector<unsigned> ground_equipment;
    std::vector<CarriedEquipment> inventory;
};

struct Encounter
{
    Battlefield battlefield;
    std::vector<Participant> participants;
    std::uint64_t scope{1};
};

struct Identity
{
    std::string module, version, content;
    auto operator<=>(const Identity &) const = default;
};
enum class Outcome
{
    ongoing,
    victory,
    defeat
};

struct ThrownWeaponOption
{
    unsigned item{};
    Message label;
    bool available{};
};

struct CombatantView
{
    EntityId id{};
    std::string name, definition;
    unsigned side{};
    Cell cell;
    int hit_points{}, max_hit_points{}, armor_class{}, initiative{}, movement_feet{};
    bool action{}, bonus_action{}, reaction{}, conscious{}, dead{}, facing_left{};
    std::string status;
    VitalState persistent;
    std::vector<Message> status_messages;
    std::vector<Message> conditions; // Derived display state; mechanics stay in the module.
    std::string type_name, melee_weapon, ranged_weapon; // Rules-owned combat display data.
    bool ranged_attack_available{};
    EquipmentState equipment;
    std::vector<GripOption> grips;
    TemporaryHitPoints temporary_hp;
    std::vector<ResourcePool> resources;
    std::vector<Message> hp_messages;
    std::vector<std::string>
        bonus_actions; // Entitlements remain visible after spending the Bonus Action.
    std::vector<std::string> known_cantrips; // Knowledge persists while casting is unavailable.
    bool naturally_sleeping{}, prone{};
    std::vector<ThrownWeaponOption> thrown_weapons;
};

struct TemporaryHpOffer
{
    EntityId recipient{};
    TemporaryHitPoints current, offered;
};

struct SneakAttackChoice
{
    EntityId attacker{}, target{};
    int dice_count{}, dice_sides{};
    bool critical{};
};

struct SavageAttackChoice
{
    EntityId attacker{}, target{};
    std::string weapon;
    int dice_count{}, dice_sides{}, modifier{}, first_damage{};
    std::optional<int> second_damage;
    bool critical{};
    int extra_damage{};
};

struct AbilityCheckChoice
{
    EntityId actor{}, target{};
    int natural{}, modifier{}, total{}, difficulty{}, resource_uses{};
};

struct FreeMovement
{
    EntityId actor{};
    int remaining_feet{};
};

struct Snapshot
{
    Identity identity;
    std::uint64_t revision{};
    unsigned round{};
    EntityId actor{};
    Outcome outcome{};
    Battlefield battlefield;
    std::vector<CombatantView> combatants; // Initiative order.
    std::vector<std::string> log;
    std::vector<Message> log_messages;
    bool reaction_pending{};
    std::uint64_t elapsed_milliseconds{};
    std::optional<TemporaryHpOffer> temporary_hp_offer;
    std::optional<SavageAttackChoice> savage_attack_choice;
    std::optional<SneakAttackChoice> sneak_attack_choice;
    std::optional<AbilityCheckChoice> ability_check_choice;
    std::optional<FreeMovement> free_movement;
    std::vector<HeldItemView> held_items;
    bool physical_inventory{};
};

// Verbs are owned by a module, not an enumeration of edition-specific rules.
// Presentation submits only currently offered commands. The module revalidates.
struct Command
{
    std::uint64_t revision{};
    EntityId actor{}, target{};
    std::string verb, label;
    Cell destination;
    unsigned item{};
};

// A terminal encounter query; application applies recovery once when leaving combat.
struct SafeRecovery
{
    std::vector<EntityId> members;
    std::vector<unsigned> items;
};

class CombatSession
{
  public:
    virtual ~CombatSession() = default;
    [[nodiscard]] virtual Snapshot snapshot() const = 0;
    [[nodiscard]] virtual std::vector<Command> legal_commands() const = 0;
    // Preview the remaining movement range of a combatant, including one
    // selected outside its turn. Only legal_commands() can authorize a move.
    [[nodiscard]] virtual std::vector<Cell> movement_reach(EntityId actor) const = 0;

    [[nodiscard]] virtual SafeRecovery safe_recovery() const
    {
        return {};
    }

    virtual bool submit(const Command &command) = 0;
    [[nodiscard]] virtual std::string save() const = 0;
};

class RulesModule
{
  public:
    virtual ~RulesModule() = default;
    [[nodiscard]] virtual Identity identity() const = 0;

    [[nodiscard]] virtual bool accepts_campaign_identity(const Identity &saved) const
    {
        return saved == identity();
    }

    [[nodiscard]] virtual std::vector<std::string> supported_features() const = 0;
    [[nodiscard]] virtual std::unique_ptr<CombatSession> create(Encounter encounter,
                                                                std::uint64_t seed) const = 0;
    [[nodiscard]] virtual std::unique_ptr<CombatSession>
    restore(std::string_view checkpoint) const = 0;
    [[nodiscard]] virtual CharacterProfile character_profile(const CharacterSheet &,
                                                             std::span<const std::string>,
                                                             EquipmentState equipment = {}) const;

    [[nodiscard]] virtual EquipmentState migrate_equipment(std::span<const std::string>) const
    {
        return {};
    }

    [[nodiscard]] virtual EquipmentInfo equipment_info(std::string_view) const
    {
        return {};
    }

    [[nodiscard]] virtual SpellAccess spell_access(const CharacterSheet &) const
    {
        return {};
    }

    [[nodiscard]] virtual SpellChoiceOptions spell_choice_options(const CharacterSheet &,
                                                                  SpellChoiceContext) const
    {
        return {};
    }

    virtual void apply_spell_choices(CharacterSheet &, const SpellChoices &, SpellChoiceContext,
                                     bool require_complete = true) const;
    [[nodiscard]] virtual AbilityCheckModifier
    ability_check(const CharacterSheet &, std::span<const std::string> gear, unsigned ability,
                  std::string_view skill = {}, std::string_view tool = {},
                  EquipmentState equipment = {}) const;
    [[nodiscard]] virtual unsigned experience_for_level(unsigned level) const;
    // False means this module's supported advancement ceiling was reached.
    virtual bool advance_character(CharacterSheet &sheet, VitalState &state) const;

    [[nodiscard]] virtual std::vector<TrainingChoiceGroup>
    training_options(const CharacterSheet &) const
    {
        return {};
    }

    [[nodiscard]] virtual AdvancementOptions advancement_options(const CharacterSheet &) const
    {
        return {};
    }

    [[nodiscard]] virtual AdvancementChoice default_advancement(const CharacterSheet &) const
    {
        return {};
    }

    virtual bool advance_character(CharacterSheet &sheet, VitalState &state,
                                   const AdvancementChoice &) const;
    virtual void recover(VitalState &state, const CharacterSheet &sheet) const;
    [[nodiscard]] virtual RecoveryInfo recovery_info(const CharacterSheet &,
                                                     const VitalState &) const;
    // A granting feature must establish entitlement and spend its costs before
    // calling this operation. The choice is explicit; pools never stack.
    virtual void grant_temporary_hit_points(VitalState &, const CharacterSheet &,
                                            const TemporaryHitPoints &, TemporaryHpChoice) const;
    // The campaign must establish completed-rest eligibility before invoking
    // these resource operations. Each spend commits one die and its RNG draw.
    virtual void recover_short_rest(VitalState &, const CharacterSheet &) const;
    [[nodiscard]] virtual Message recover_rest_choice(VitalState &, const CharacterSheet &,
                                                      std::string_view) const;
    virtual HitDieResult spend_hit_die(VitalState &, const CharacterSheet &, std::uint64_t &) const;

    // Advances module-owned lasting effects for a group in deterministic order.
    virtual void elapse(std::span<Participant>, std::uint64_t, std::uint64_t &) const
    {
    }

    virtual void validate_character_state(const CharacterSheet &, const VitalState &) const;
    virtual void validate_saved_grants(const Identity &, const CharacterSheet &,
                                       std::span<const FeatureGrant>) const;

    // Called after replaying saved creation/advancement under the current rules.
    // Edition-specific migration preserves wounds and opaque resource state.
    virtual void migrate_character_state(const Identity &, const CharacterSheet &sheet,
                                         VitalState &state) const
    {
        validate_character_state(sheet, state);
    }

    [[nodiscard]] virtual RestProgress begin_rest(RestKind) const;
    [[nodiscard]] virtual RestTransition advance_rest(const RestProgress &, std::uint64_t,
                                                      RestWork) const;
    [[nodiscard]] virtual RestTransition interrupt_rest(const RestProgress &,
                                                        RestInterruption) const;
    [[nodiscard]] virtual RestProgress resume_rest(const RestProgress &) const;
    [[nodiscard]] virtual std::uint64_t remaining_rest(const RestProgress &) const;
    virtual void validate_rest(const RestProgress &) const;
    [[nodiscard]] virtual RestPolicy long_rest_policy() const;
    [[nodiscard]] virtual RestPolicy short_rest_policy() const;

    // Rest hosts report an activity; the module owns sleep/condition effects.
    // Stand when able without restoring HP/resources; report ability to collect.
    virtual bool recover_at_safety(VitalState &, const CharacterSheet &,
                                   std::span<const std::string>) const
    {
        return false;
    }

    virtual void set_rest_work(VitalState &, const CharacterSheet &, RestWork) const
    {
    }

    // The module identifies equipment that the current condition releases.
    [[nodiscard]] virtual std::vector<unsigned>
    released_equipment(const CharacterSheet &, const VitalState &,
                       std::span<const std::string>) const
    {
        return {};
    }

    virtual void set_hit_points(VitalState &, const CharacterSheet &, int) const;
    virtual void temple_heal(VitalState &state, const CharacterSheet &sheet,
                             std::uint64_t &random_state) const;
};
} // namespace opengold::rules
#endif
