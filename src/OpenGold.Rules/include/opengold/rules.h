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
    advancement,
    long_rest
};

struct TrainingReplacementOptions
{
    TrainingChoiceGroup group;
    std::vector<std::string> selected;
    unsigned replacement_limit{};
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
    std::optional<std::string>
    fighting_style; // Class-granted choice/replacement, separate from a level-four feat.
    std::optional<TrainingChoices> spell_learning; // Required for Wizards; absent otherwise.
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
    std::vector<AdvancementOption> feats, spells, fighting_styles;
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
    // Granted by a class or subclass feature; never counted against prepared_choices.
    std::vector<std::string> always_prepared;
    unsigned cantrip_choices{}, spellbook_choices{}, prepared_choices{};
};

enum class EquipmentOperation
{
    equip,
    unequip,
    equip_main,
    equip_other
};

struct EquipmentChoice
{
    EquipmentOperation operation;
    Message label, explanation;
    bool available{};
};

// Indices reference the supplied candidates, not inventory IDs. The module
// determines the resulting loadout; Core maps indices back to owned items.
struct EquipmentChange
{
    std::vector<unsigned> indices;
    bool separate_selected_unit{};
};

struct CharacterProfile
{
    std::string data;
    int hit_points{}, armor_class{};
    std::string description;
    int movement_feet{}, melee_attack_bonus{};
    std::string item_modifiers, spell_modifiers;
    std::vector<Message> equipment_positions;
    bool strength_dexterity_disadvantage{};
    std::vector<Message> item_messages, spell_messages;
    // Hands the equipped weapon is wielded with when attacking. A Versatile
    // weapon uses two exactly when no shield or second weapon is held.
    unsigned weapon_hands{};
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
enum class RestKind
{
    short_rest,
    long_rest
};

// A rest either completes or is interrupted; an interrupted rest grants nothing.
struct RestPolicy
{
    unsigned duration_minutes{}, wait_after_rest_minutes{};
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

// A spell or feature a character can use outside combat, from the Camp dialog.
// `id` is the rules module's key, such as a spell's upcast form.
struct CampAction
{
    std::string id;
    Message label;
    bool operator==(const CampAction &) const = default;
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

// One rolled d20 ability check: the kept die and the check total.
struct AbilityCheckRoll
{
    int die{}, total{};
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
// ordinal. Items never leave their holder: downed characters keep their gear and
// a thrown weapon stays in inventory, like ammunition.
struct HeldItemView
{
    unsigned id{};
    EntityId origin{}, holder{};
    unsigned equipment_index{};
    std::string definition;
    Message label;
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
    // The encounter interrupted this participant's rest; the rules module
    // decides how it starts (SRD: awake and Prone).
    bool resting{};
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

// Pre-1.0 saves are not migrated: every save kind accepts only its current
// format and the current rules identity. Hosts localize this message.
inline constexpr char older_save_message[] =
    "This save was made by an older pre-release version of OpenGoldBox and can't be loaded.";
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

struct ItemAttackOption
{
    unsigned item{};
    std::string verb;
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
    TemporaryHitPoints temporary_hp;
    std::vector<ResourcePool> resources;
    std::vector<Message> hp_messages;
    std::vector<std::string>
    bonus_actions; // Entitlements remain visible after spending the Bonus Action.
    std::vector<std::string> known_cantrips; // Knowledge persists while casting is unavailable.
    bool prone{};
    std::vector<ThrownWeaponOption> thrown_weapons;
    std::vector<ThrownWeaponOption> weapons;
    unsigned selected_weapon{};
    std::vector<ItemAttackOption> light_attacks;
    bool nick_mastery{};
    std::vector<ItemAttackOption> nick_attacks;
};

struct TemporaryHpOffer
{
    EntityId recipient{};
    TemporaryHitPoints current, offered;
};

struct AbilityCheckChoice
{
    EntityId actor{}, target{};
    int natural{}, modifier{}, total{}, difficulty{}, resource_uses{};
};

struct EffectOption
{
    unsigned id{};
    Message title, description;
    bool available{true};
};

struct OptionalEffectChoice
{
    EntityId actor{}, target{};
    Message title, description;
    std::vector<EffectOption> options;
};

struct EffectTargeting
{
    EntityId actor{};
    std::string verb;
    Message prompt;
    bool destination{};
};

// Choosing the creatures of a spell that affects several, such as Bless.
// Choosing `verb` on a creature adds or removes it; spell_cast casts on the
// chosen creatures and spell_cancel spends nothing.
struct SpellTargeting
{
    EntityId actor{};
    std::string verb;
    std::vector<EntityId> chosen;
    unsigned maximum{};
};

// An area spell being aimed (CLASS-5): `center` is the previewed point and
// `cells` the squares the spell would affect there.
struct AreaTargeting
{
    EntityId actor{};
    std::string verb;
    Cell center;
    std::vector<Cell> cells;
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
    std::optional<AbilityCheckChoice> ability_check_choice;
    std::optional<FreeMovement> free_movement;
    std::optional<OptionalEffectChoice> optional_effect_choice;
    std::optional<EffectTargeting> effect_targeting;
    std::optional<SpellTargeting> spell_targeting;
    std::optional<AreaTargeting> area_targeting;
    // Pre-turn decisions; legal commands carry eligible actors and allies.
    std::vector<EntityId> initiative_choices;
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
    bool aims_area{}; // Starts aiming an area spell (CLASS-5).
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

    virtual bool submit(const Command &command) = 0;
    [[nodiscard]] virtual std::string save() const = 0;
};

class RulesModule
{
  public:
    virtual ~RulesModule() = default;
    [[nodiscard]] virtual Identity identity() const = 0;
    [[nodiscard]] virtual std::vector<std::string> supported_features() const = 0;
    [[nodiscard]] virtual std::unique_ptr<CombatSession> create(Encounter encounter,
            std::uint64_t seed) const = 0;
    [[nodiscard]] virtual std::unique_ptr<CombatSession>
    restore(std::string_view checkpoint) const = 0;
    [[nodiscard]] virtual CharacterProfile character_profile(const CharacterSheet &,
            std::span<const std::string>) const;

    [[nodiscard]] virtual EquipmentInfo equipment_info(std::string_view) const
    {
        return {};
    }

    [[nodiscard]] virtual std::vector<EquipmentChoice>
    equipment_choices(const CharacterSheet &, std::span<const std::string>, unsigned) const
    {
        return {};
    }

    [[nodiscard]] virtual EquipmentChange equipment_change(const CharacterSheet &,
            std::span<const std::string> candidates, unsigned selected,
            EquipmentOperation) const;

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
                  std::string_view skill = {}) const;
    // Rolls that check outside combat, advancing the campaign service random state.
    [[nodiscard]] virtual AbilityCheckRoll
    roll_ability_check(const CharacterSheet &, std::span<const std::string> gear, unsigned ability,
                       std::string_view skill, std::uint64_t &random_state) const;
    [[nodiscard]] virtual unsigned experience_for_level(unsigned level) const;
    // False means this module's supported advancement ceiling was reached.
    virtual bool advance_character(CharacterSheet &sheet, VitalState &state) const;

    [[nodiscard]] virtual std::optional<TrainingReplacementOptions>
    rest_training_options(const CharacterSheet &) const
    {
        return {};
    }

    // Returns the effective source-group selections after applying a legal edit.
    virtual TrainingChoices replace_rest_training(CharacterSheet &,
            std::span<const std::string>) const;

    [[nodiscard]] virtual std::vector<TrainingChoiceGroup>
    training_options(const CharacterSheet &) const
    {
        return {};
    }

    [[nodiscard]] virtual AdvancementOptions advancement_options(const CharacterSheet &) const
    {
        return {};
    }

    [[nodiscard]] virtual AdvancementOptions advancement_options(const CharacterSheet &sheet,
            const AdvancementChoice &) const
    {
        return advancement_options(sheet);
    }

    [[nodiscard]] virtual AdvancementChoice default_advancement(const CharacterSheet &) const
    {
        return {};
    }

    // The sheet a level-up's spell choices are offered for: one level higher,
    // with the choice's features that change them (such as a Fighting Style).
    [[nodiscard]] virtual CharacterSheet spell_choice_sheet(const CharacterSheet &,
            const AdvancementChoice &) const;

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

    [[nodiscard]] virtual RestPolicy long_rest_policy() const;
    [[nodiscard]] virtual RestPolicy short_rest_policy() const;

    virtual void set_hit_points(VitalState &, const CharacterSheet &, int) const;
    virtual void temple_heal(VitalState &state, const CharacterSheet &sheet,
                             std::uint64_t &random_state) const;
    [[nodiscard]] virtual std::vector<CampAction> camp_actions(const CharacterSheet &,
            const VitalState &) const
    {
        return {};
    }
    // Uses one of camp_actions() on the target. A character acting on itself
    // passes the same VitalState as both user and target.
    virtual void use_camp_action(const CharacterSheet &user, VitalState &user_state,
                                 const CharacterSheet &target, VitalState &target_state,
                                 std::string_view action, std::uint64_t &random_state) const;
};
} // namespace opengold::rules
#endif
