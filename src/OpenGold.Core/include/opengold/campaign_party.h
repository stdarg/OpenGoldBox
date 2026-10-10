#ifndef OPENGOLD_CAMPAIGN_PARTY_H
#define OPENGOLD_CAMPAIGN_PARTY_H
#include "opengold/character.h"
#include "opengold/coin_purse.h"
#include "opengold/creature_catalog.h"
#include "opengold/ecl_machine.h"

namespace opengold
{
using MemberId = rules::EntityId;
using RestKind = rules::RestKind;
enum class RestDenial
{
    none,
    vitality,
    cooldown,
    combat,
    spending
};

struct MemberRestInfo
{
    MemberId id{};
    rules::RecoveryInfo recovery;
    RestDenial denial{RestDenial::none};
    std::uint64_t wait_milliseconds{};
};

struct RestTicket
{
    std::uint64_t session{}, revision{};
    auto operator<=>(const RestTicket &) const = default;
};

// A completed hour grants a bounded spending window, not an undoable preview.
struct ShortRestSession
{
    RestTicket ticket;
    std::uint64_t completed_minutes{};
    unsigned completed_subminute_milliseconds{};
    std::vector<MemberId> members;
};

struct RestResult
{
    RestKind kind{};
    std::uint64_t duration_minutes{};
    std::vector<MemberId> members;
    std::optional<RestTicket> spending;
};

// How the party tries a locked door: Bash is Strength (Athletics) by anyone
// conscious; Pick is Dexterity (Sleight of Hand) by a conscious Rogue; Knock is
// the spell, cast by a member who has it prepared, and always opens it.
enum class DoorMethod
{
    bash,
    pick,
    knock
};

// One member's check against a locked door.
struct DoorAttempt
{
    MemberId member{};
    rules::AbilityCheckRoll roll;
};

struct PartyMember
{
    MemberId id{};
    Character character;
    std::string npc_source; // Empty for PCs; explicit campaign conversion identity for NPCs.
    rules::VitalState vitals;
    std::array<std::uint16_t, 7> wealth{};
    std::vector<std::uint64_t> equipped;
    unsigned morale{100};
    unsigned experience{};
    std::optional<std::uint64_t> last_rest_minutes;
    unsigned last_rest_subminute_milliseconds{};
    std::map<std::uint64_t, por::Equipment> item_sources;
    std::string creation_source; // Stable pool candidate identity, empty for authored PCs.
    bool quick{}; // The computer plays it in fights (Quick) until the player takes control.
};

struct HazardHit
{
    MemberId target{};
    rules::HazardAttackResult result;
};

struct PartyState
{
    std::vector<PartyMember> roster;
    std::array<MemberId, 8> slots{};
    MemberId next_id{1};
    unsigned selected{};
    MemberId leader{}; // Speaks for the party and buys in shops; 0 means the first member.
    bool quick_magic{}; // Members on Quick may cast spells.
    std::uint64_t time_minutes{};
    RandomState random_state{42};
    std::vector<std::string> claimed_rewards;
    unsigned subminute_milliseconds{};
    std::uint64_t next_combat_scope{1};
    std::uint64_t next_rest_session{1};
    std::optional<ShortRestSession> short_rest;
    std::optional<ShortRestSession> spell_rest; // Completed-rest choices, consumed once per member.
    std::optional<ShortRestSession>
    training_rest; // Rule-owned training replacements after spell choices.
};

// One shared campaign value store. Sessions share this owner, never separate PCs.
// While combat owns mutable vitals, roster/equipment/script mutations are barred.
class CampaignParty
{
  public:
    explicit CampaignParty(std::unique_ptr<rules::RulesModule> rules);

    [[nodiscard]] const PartyState &state() const
    {
        return state_;
    }

    [[nodiscard]] const PartyMember &member(MemberId id) const;

    [[nodiscard]] MemberId selected() const
    {
        return state_.slots.at(state_.selected);
    }

    void select(unsigned slot);
    // The designated leader, or the first member when none is designated.
    [[nodiscard]] MemberId leader() const;
    void make_leader(MemberId id);
    // Quick combat, as in the original: the computer plays a member on Quick
    // in this fight and later ones until the player takes control of the
    // party. Settings, so they change during a fight too.
    void set_quick(MemberId id);
    void take_control();
    void set_quick_magic(bool on);
    // A member left on the field when the party flees is lost for good: dead
    // and out of the party (the original's rule).
    void lose(MemberId id);
    // Who speaks for the party now: the leader, or the first conscious member
    // when the leader is down. 0 when nobody can speak.
    [[nodiscard]] MemberId spokesman() const;
    MemberId add_pc(Character character);
    MemberId recruit(std::string source, Character converted, unsigned morale = 100);
    void rejoin(MemberId id);
    void remove(MemberId id);
    [[nodiscard]] std::vector<rules::EquipmentChoice> equipment_choices(MemberId id,
            std::uint64_t item) const;
    void equip(MemberId id, std::uint64_t item,
               rules::EquipmentOperation operation = rules::EquipmentOperation::equip);
    void unequip(MemberId id, std::uint64_t item);
    [[nodiscard]] rules::EquipmentInfo equipment_info(MemberId id, std::uint64_t item) const;
    void purchase(MemberId id, const por::Equipment &item);
    void set_wealth(MemberId id, std::array<std::uint16_t, 7> wealth);
    void award_experience(unsigned amount, std::string reward_id);
    [[nodiscard]] bool can_advance(MemberId id) const;
    [[nodiscard]] rules::AdvancementOptions
    advancement_options(MemberId id, const rules::AdvancementChoice &choice = {}) const;
    [[nodiscard]] rules::AdvancementChoice default_advancement(MemberId id) const;
    [[nodiscard]] PartyMember preview_advancement(MemberId id,
            const rules::AdvancementChoice &choice) const;
    void advance(MemberId id, const rules::AdvancementChoice &choice);
    // Long Rest spell choices for a member offered them by the completed rest.
    [[nodiscard]] rules::SpellChoiceOptions spell_choice_options(MemberId id) const;
    [[nodiscard]] PartyMember preview_spell_choices(MemberId id, const rules::SpellChoices &) const;
    void choose_spells(MemberId id, const rules::SpellChoices &);
    void keep_rest_spells(MemberId id);
    [[nodiscard]] PartyMember preview_rest_training(RestTicket, MemberId,
            std::span<const std::string>) const;
    void replace_rest_training(RestTicket, MemberId, std::span<const std::string>);
    void keep_rest_training(RestTicket, MemberId);
    // Atomic original loot delivery. A full set of purses leaves it unclaimed.
    bool award_loot(const std::array<unsigned, 7> &wealth, const std::vector<por::Equipment> &items,
                    std::string reward_id);
    [[nodiscard]] bool rest();
    // The campaign service must first approve the location and interruption profile.
    [[nodiscard]] std::vector<MemberRestInfo> rest_info(RestKind kind) const;
    // A rest completes in one step; an interrupted rest is never started here.
    [[nodiscard]] std::optional<RestResult> rest(RestKind kind);
    // Spends Hit Dice one at a time until the member is at full HP or out of dice.
    [[nodiscard]] std::vector<rules::HitDieResult> heal_with_hit_dice(RestTicket ticket,
            MemberId id);
    [[nodiscard]] rules::Message recover_rest_choice(RestTicket ticket, MemberId id,
            std::string_view choice);
    void finish_short_rest(RestTicket ticket);
    void temple_heal(MemberId target);
    // An attack from outside combat, such as an arrow trap, on a random active
    // conscious member; nothing when none is conscious.
    std::optional<HazardHit> hazard_attack(const rules::HazardAttack &attack);
    // The member's current Hit Point maximum, raised by Aid.
    [[nodiscard]] int hit_point_maximum(MemberId id) const;
    // Spells and features an active member can use outside combat (CLASS-3).
    [[nodiscard]] std::vector<rules::CampAction> camp_actions(MemberId id) const;
    // Atomic: on failure nothing changes.
    void use_camp_action(MemberId user, MemberId target, std::string_view action);
    // Eligible active members, in party order, try once each until one meets
    // the difficulty. Returns every attempt; the door opens if the last succeeded.
    [[nodiscard]] std::vector<DoorAttempt> try_door(DoorMethod method, int difficulty);
    [[nodiscard]] bool can_try_door(DoorMethod method) const;
    void advance_time(unsigned minutes);
    void advance_time_milliseconds(std::uint64_t milliseconds);

    [[nodiscard]] std::uint64_t time_hours() const noexcept
    {
        return state_.time_minutes / 60;
    }

    [[nodiscard]] rules::CharacterProfile profile(MemberId id) const;
    [[nodiscard]] rules::AbilityCheckModifier ability_check(MemberId id, unsigned ability,
            std::string_view skill = {}) const;
    [[nodiscard]] rules::RecoveryInfo recovery_info(MemberId id) const;
    // Applies the script's HP and coin changes. Coins are seen and settled as
    // script_coins() describes; the result reports any change the purse made.
    [[nodiscard]] std::optional<CoinExchange> read_character(unsigned slot,
            const por::EclMachine &vm);

    [[nodiscard]] PartyState checkpoint() const
    {
        return state_;
    }

    void restore(PartyState state);
    static void validate(const PartyState &state);
    static void validate_rest_choices(const PartyState &state, const rules::RulesModule &rules);
    [[nodiscard]] std::vector<rules::Participant> participants() const;
    void begin_combat();
    // Combat changes vitals only: characters keep their gear throughout.
    void apply_combat(const rules::Snapshot &snapshot);

    void end_combat() noexcept
    {
        combat_ = false;
    }

    [[nodiscard]] bool in_combat() const
    {
        return combat_;
    }

    [[nodiscard]] const rules::RulesModule &rule_module() const
    {
        return *rules_;
    }

    [[nodiscard]] const rules::Identity identity() const
    {
        return rules_->identity();
    }

  private:
    [[nodiscard]] std::vector<DoorAttempt> cast_knock(int difficulty);
    void change_equipment(MemberId id, std::uint64_t item, rules::EquipmentOperation operation);
    std::unique_ptr<rules::RulesModule> rules_;
    PartyState state_;
    bool combat_{};
    bool combat_registered_{};
    std::uint64_t combat_elapsed_{};
    void elapse(PartyState &state, std::uint64_t milliseconds,
                std::span<const MemberId> in_combat = {}) const;
    void editable() const;
    void outside_combat() const;
    void require_rest_ticket(RestTicket ticket) const;
    void short_rest_benefits(PartyState &state, const std::vector<MemberId> &members) const;
    void long_rest_benefits(PartyState &state, const std::vector<MemberId> &members,
                            RestTicket ticket, RestResult &result) const;
    PartyMember &edit(MemberId id);
    void join(MemberId id, bool npc);
};

[[nodiscard]] std::string equipment_conversion(const por::Equipment &item);
} // namespace opengold
#endif
