#ifndef OPENGOLD_RULES_H
#define OPENGOLD_RULES_H
#include "opengold/message.h"
#include "opengold/random_state.h"
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

// Whether spell choices must settle every open choice, or may leave some for
// later (a level-up's new spells, chosen before its preparation). Named
// rather than a bool so a call site says which (Effective C++ Item 18).
enum class ChoiceCompleteness
{
    partial,
    complete
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
    EquipmentOperation operation{EquipmentOperation::equip};
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

// An attack on a party member outside combat, such as an original arrow trap:
// an attack roll against the member's AC, then the damage dice on a hit.
struct HazardAttack
{
    int attack_bonus{};
    unsigned dice{}, sides{};
    int damage_bonus{};
    std::string damage_type;
};

struct HazardAttackResult
{
    int natural{}, total{}, armor_class{}, damage{};
    bool hit{}, critical{};
    // With no turns outside combat, a member dropped to 0 HP rolls these at once.
    std::vector<int> death_saves;
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
    bool whole_party{}; // affects several members at once; no target is chosen
};

// A party member a whole-party camp action may affect.
struct CampTarget
{
    const CharacterSheet *sheet{};
    VitalState *state{};
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

// Endpoints are cell centers. Touching either wall at a diagonal corner blocks
// sight. Callers supply a validated battlefield; creatures do not obstruct sight.
[[nodiscard]] bool has_line_of_sight(const Battlefield &board, Cell from, Cell to);

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
    // Morale (the original's rule, for creatures the rules control): its own,
    // 0-100, where 0 leaves it to the encounter's; and the Intelligence that
    // decides whether a broken creature that cannot run surrenders.
    unsigned morale{};
    unsigned intelligence{10};
};

struct Encounter
{
    Battlefield battlefield;
    std::vector<Participant> participants;
    std::uint64_t scope{1};
    // The encounter's morale, 0-100, set by the original script; 100 never breaks.
    unsigned morale{100};
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
    defeat,
    fled // No party member is left on the field and at least one got away.
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
    std::string form; // Wild Shape's Beast form, such as "wolf"; empty otherwise.
    bool prone{};
    std::vector<ThrownWeaponOption> thrown_weapons;
    std::vector<ThrownWeaponOption> weapons;
    unsigned selected_weapon{};
    std::vector<ItemAttackOption> light_attacks;
    bool nick_mastery{};
    std::vector<ItemAttackOption> nick_attacks;
    // Thrown gear the combatant still carries, by item definition; the campaign
    // trims its inventory to these counts.
    std::vector<std::pair<std::string, unsigned>> thrown_gear_left;
    // Regenerates Hit Points each turn unless Acid or Fire stops it (stopped: it
    // already took Acid or Fire since its last turn); burning; covered in oil
    // (its next Fire damage deals 5 more); carries or wields a Torch.
    bool regenerates{}, regeneration_stopped{}, burning{}, oiled{}, has_torch{};
    bool fled{}; // Ran off the field; it rejoins the party when the fight ends.
    // A party member fast enough to run off the field (no conscious enemy is
    // faster) that has not failed a flight this fight.
    bool can_flee{};
    // Its morale broke and it runs for the edge (it may rally); or it gave up
    // and left the fight, counted as defeated.
    bool panicked{}, surrendered{};
    // Every spell it knows, cantrips included: a command whose verb is one of
    // these, or one of these and a suffix (an upcast form), casts a spell.
    std::vector<std::string> spells;
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

// One line of the combat log: the English text and the message the game
// translates, kept together so a module cannot let them drift apart
// (Effective C++ Item 18).
struct LogEntry
{
    std::string english;
    Message message;
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
    std::vector<LogEntry> log_entries; // Oldest first.
    bool reaction_pending{};
    std::uint64_t elapsed_milliseconds{};
    std::optional<TemporaryHpOffer> temporary_hp_offer;
    std::optional<AbilityCheckChoice> ability_check_choice;
    std::optional<FreeMovement> free_movement;
    std::optional<OptionalEffectChoice> optional_effect_choice;
    std::optional<EffectTargeting> effect_targeting;
    std::optional<SpellTargeting> spell_targeting;
    std::optional<AreaTargeting> area_targeting;
    // Squares a spell makes Heavily Obscured, such as Fog Cloud's.
    std::vector<Cell> obscured;
    // Squares inside Silence.
    std::vector<Cell> silenced;
    // Where each Spiritual Weapon floats.
    std::vector<Cell> spiritual_weapons;
    // Where each Flaming Sphere burns.
    std::vector<Cell> flaming_spheres;
    // Squares inside a Moonbeam.
    std::vector<Cell> moonbeams;
    // Pre-turn decisions; legal commands carry eligible actors and allies.
    std::vector<EntityId> initiative_choices;
    std::vector<HeldItemView> held_items;
    bool physical_inventory{};

    // The log's English lines, oldest first.
    [[nodiscard]] std::vector<std::string> log() const
    {
        std::vector<std::string> lines;
        lines.reserve(log_entries.size());
        for (const auto &entry : log_entries)
            lines.push_back(entry.english);
        return lines;
    }

    // The log's messages, oldest first.
    [[nodiscard]] std::vector<Message> log_messages() const
    {
        std::vector<Message> messages;
        messages.reserve(log_entries.size());
        for (const auto &entry : log_entries)
            messages.push_back(entry.message);
        return messages;
    }
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

  protected:
    // Implementations may copy themselves (a session's rollback does), but an
    // interface reference must not copy or assign only its empty base part
    // (Effective C++ Items 5 and 6).
    CombatSession() = default;
    CombatSession(const CombatSession &) = default;
    CombatSession(CombatSession &&) = default;
    CombatSession &operator=(const CombatSession &) = default;
    CombatSession &operator=(CombatSession &&) = default;
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

    // No default for the completeness: an override cannot redefine one
    // (Effective C++ Item 37), and every caller says which it means.
    virtual void apply_spell_choices(CharacterSheet &, const SpellChoices &, SpellChoiceContext,
                                     ChoiceCompleteness) const;
    [[nodiscard]] virtual AbilityCheckModifier
    ability_check(const CharacterSheet &, std::span<const std::string> gear, unsigned ability,
                  std::string_view skill = {}) const;
    // Rolls that check outside combat, advancing the campaign service random state.
    [[nodiscard]] virtual AbilityCheckRoll
    roll_ability_check(const CharacterSheet &, std::span<const std::string> gear, unsigned ability,
                       std::string_view skill, RandomState &random_state) const;
    [[nodiscard]] virtual unsigned experience_for_level(unsigned level) const;

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

    // The only form a module overrides; default_advancement() supplies the
    // choice when the player makes none. False means this module's supported
    // advancement ceiling was reached.
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
    virtual HitDieResult spend_hit_die(VitalState &, const CharacterSheet &, RandomState &) const;

    // Advances module-owned lasting effects for a group in deterministic order.
    virtual void elapse(std::span<Participant>, std::uint64_t, RandomState &) const
    {
    }

    virtual void validate_character_state(const CharacterSheet &, const VitalState &) const;

    [[nodiscard]] virtual RestPolicy long_rest_policy() const;
    [[nodiscard]] virtual RestPolicy short_rest_policy() const;

    virtual void set_hit_points(VitalState &, const CharacterSheet &, int) const;
    virtual void temple_heal(VitalState &state, const CharacterSheet &sheet,
                             RandomState &random_state) const;
    [[nodiscard]] virtual HazardAttackResult hazard_attack(VitalState &state,
            const CharacterSheet &sheet, const HazardAttack &attack,
            RandomState &random_state) const;
    // The current Hit Point maximum, which a lasting effect such as Aid raises
    // above the sheet's.
    [[nodiscard]] virtual int hit_point_maximum(const CharacterSheet &, const VitalState &) const;
    [[nodiscard]] virtual std::vector<CampAction> camp_actions(const CharacterSheet &,
            const VitalState &) const
    {
        return {};
    }
    // Uses one of camp_actions() on the target. A character acting on itself
    // passes the same VitalState as both user and target.
    virtual void use_camp_action(const CharacterSheet &user, VitalState &user_state,
                                 const CharacterSheet &target, VitalState &target_state,
                                 std::string_view action, RandomState &random_state) const;
    // Uses a whole_party camp action on the members it chooses among `party`,
    // which may include the user's own state.
    virtual void use_party_camp_action(const CharacterSheet &user, VitalState &user_state,
                                       std::span<const CampTarget> party, std::string_view action,
                                       RandomState &random_state) const;
    // A spell cast while exploring, such as Knock on a locked door.
    [[nodiscard]] virtual bool can_cast_exploration_spell(const CharacterSheet &,
            const VitalState &, std::string_view) const
    {
        return false;
    }
    virtual void cast_exploration_spell(const CharacterSheet &caster, VitalState &state,
                                        std::string_view spell) const;

  protected:
    // Implementations may copy themselves (a session's rollback does), but an
    // interface reference must not copy or assign only its empty base part
    // (Effective C++ Items 5 and 6).
    RulesModule() = default;
    RulesModule(const RulesModule &) = default;
    RulesModule(RulesModule &&) = default;
    RulesModule &operator=(const RulesModule &) = default;
    RulesModule &operator=(RulesModule &&) = default;
};
} // namespace opengold::rules
#endif
