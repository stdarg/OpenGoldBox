#ifndef OPENGOLD_CAMPAIGN_PARTY_H
#define OPENGOLD_CAMPAIGN_PARTY_H
#include "opengold/character.h"
#include "opengold/character_creator.h"
#include "opengold/coin_purse.h"
#include "opengold/equipment.h"
#include "opengold/shop_sale.h"
#include "opengold/training_hall.h"
#include <chrono>

namespace opengold::por
{
// Only read_character() names the script machine, by reference, so files that
// include this header do not compile the machine (Effective C++ Item 31).
class EclMachine;
} // namespace opengold::por

namespace opengold
{
using MemberId = rules::EntityId;

// A place in the party's marching order: 0-5 hold PCs and 6-7 hold NPCs. A slot
// is not a MemberId, because the member in a slot changes as the party changes
// (Effective C++ Item 18).
// Scripts scan party slots 0 to 8 (Skullcrusher's search, ECL4:2 0xAA95);
// no member fills slot 8, so it always reads as empty.
inline constexpr unsigned ecl_scanned_slots = 8;

struct PartySlot
{
    unsigned index{};

    friend bool operator==(PartySlot, PartySlot) = default;
};

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
// conscious; Pick is Dexterity (Sleight of Hand) by a conscious Rogue, whose
// Thieves' Tools double the chance (LOCK-1) and Magic Lock Picks give
// Advantage (SECRET-1); Knock is the spell, cast by a member who has it
// prepared, and always opens it.
enum class DoorMethod
{
    bash,
    pick,
    knock
};

// One member's check against a locked door. The difficulty is the lock's, or
// lower for a Rogue picking it with Thieves' Tools.
struct DoorAttempt
{
    MemberId member{};
    rules::AbilityCheckRoll roll;
    int difficulty{};
    bool thieves_tools{};
    bool magic_lock_picks{}; // used up for Advantage on the check (SECRET-1)
};

// One member's d6 roll to find a secret door: found when the die is at most
// `needed` (SECRET-1).
struct SecretDoorRoll
{
    MemberId member{};
    int die{}, needed{};
};

// Which way auto-equip (EQUIP-1) settles a choice between a shield and a
// second hand on the weapon: each character's own setting, on its sheet.
// A typical early monster's Armor Class, which auto-equip rates weapons
// against (EQUIP-1) and a shop's hover tells the roll to hit.
inline constexpr int rated_armor_class = 13;

// What a shop's hover tells of an item for one member, rated as the party
// knows the item (an unidentified one by its plain stats) and as the member
// would hold it in place of what it holds now (EQUIP-1).
struct ItemPreview
{
    std::optional<rules::WeaponAttackView> attack; // a weapon's
    // Armor or a shield's Armor Class on the member, and the member's now.
    std::optional<int> armor_class;
    int armor_class_now{};
    // Armor Class falls as it gets better (the original rules).
    bool descending_armor_class{};
    unsigned weight_hundredths{};
};

enum class GearPreference : std::uint8_t
{
    defense, // the higher Armor Class first, then the higher damage
    damage   // the higher damage first, then the higher Armor Class
};

// Whether auto-equip puts on a magic item the party has not identified.
enum class UnidentifiedMagic : std::uint8_t
{
    equip,
    skip
};

// What auto-equip did with an item a member gained (EQUIP-1), by the names
// the party knows the items by.
struct GearChange
{
    MemberId member{};
    std::string name;                    // The member's.
    std::vector<std::string> equipped;   // The gained item first.
    std::vector<std::string> unequipped; // Items that went to the pack.
    // An alignment-bound sword (#267) that burned its taker for this much and
    // stayed in the pack; nothing was equipped.
    std::string burned_by;
    int burn_damage{};
};

struct PartyMember
{
    MemberId id{};
    Character character;
    std::string npc_source; // Empty for PCs; explicit campaign conversion identity for NPCs.
    rules::VitalState vitals;
    std::vector<std::uint64_t> equipped;
    unsigned morale{100};
    unsigned experience{};
    std::optional<std::uint64_t> last_rest_minutes;
    unsigned last_rest_subminute_milliseconds{};
    std::map<std::uint64_t, por::Equipment> item_sources;
    std::string creation_source; // Stable pool candidate identity, empty for authored PCs.
    // The level at which the member last bought a hall's experience; 0 for never.
    unsigned hall_experience_level{};
    bool quick{}; // The computer plays it in fights (Quick) until the player takes control.
    GearPreference gear_preference{GearPreference::defense};
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
    PartySlot selected_slot{};
    MemberId leader{};  // Speaks for the party and buys in shops; 0 means the first member.
    Purse wealth{};     // All the party's money; no member carries coins of their own.
    bool quick_magic{}; // Members on Quick may cast spells.
    std::uint64_t time_minutes{};
    // The minute Detect Magic ends; until then item lists mark magic items.
    std::uint64_t detect_magic_until{};
    // Friends (MAGIC-6): the member whose Charisma it raises, by how much, and
    // the minute it ends.
    MemberId friends_member{};
    int friends_bonus{};
    std::uint64_t friends_until{};
    // The minute Find Traps ends (MAGIC-6). The original's trap scripts in
    // Kovel Mansion and the Kobold Caves ask for it.
    std::uint64_t find_traps_until{};
    RandomState random_state{42};
    std::vector<std::string> claimed_rewards;
    unsigned subminute_milliseconds{};
    std::uint64_t next_combat_scope{1};
    std::uint64_t next_rest_session{1};
    std::optional<ShortRestSession> short_rest;
    std::optional<ShortRestSession> spell_rest; // Completed-rest choices, consumed once per member.
    std::optional<ShortRestSession>
        training_rest; // Rule-owned training replacements after spell choices.
    // The training_rest follows a Short Rest, which changes fewer choices.
    bool training_after_short_rest{};
    // The spell_rest follows a Short Rest: Memorize Spell changes one spell.
    bool spells_after_short_rest{};
};

// Who may stand in the party's two NPC positions: NPCs only, as in the game,
// or player characters too, for a developer tool's party of eight.
enum class NpcPositions
{
    npcs_only,
    open_to_player_characters
};

// One shared campaign value store. Sessions share this owner, never separate PCs.
// While combat owns mutable vitals, roster/equipment/script mutations are barred.
class CampaignParty
{
  public:
    explicit CampaignParty(std::unique_ptr<rules::RulesModule> rules);
    // A game's party: its rules module and the character rules it makes and
    // keeps its characters by, both under the rules the game chose.
    CampaignParty(std::unique_ptr<rules::RulesModule> rules,
                  std::shared_ptr<const rules::CharacterRules> creation);

    [[nodiscard]] const PartyState &state() const noexcept
    {
        return state_;
    }

    [[nodiscard]] const PartyMember &member(MemberId id) const;

    [[nodiscard]] MemberId selected() const
    {
        return state_.slots.at(state_.selected_slot.index);
    }

    void select(PartySlot slot);
    // The designated leader, or the first member when none is designated.
    [[nodiscard]] MemberId leader() const;
    void make_leader(MemberId id);
    // Quick combat, as in the original: the computer plays a member on Quick
    // until the player takes control of the party or, unless Quick is kept,
    // the fight ends (CombatDemo::keep_quick). Settings, so they change during
    // a fight too.
    void set_quick(MemberId id);
    void set_gear_preference(MemberId id, GearPreference preference);
    // A new look, such as another portrait, which a player may choose at any time.
    void set_appearance(MemberId id, por::CharacterAppearance appearance);
    void take_control();
    void set_quick_magic(bool on);
    // A member left on the field when the party flees is lost for good: dead
    // and out of the party (the original's rule).
    void lose(MemberId id);
    // A champion who lost a duel is left with 1 Hit Point, not dying (the
    // original engine's rule); a dead one stays dead.
    void recover_duelist(MemberId id);
    // Who speaks for the party now: the leader, or the first conscious member
    // when the leader is down. 0 when nobody can speak.
    [[nodiscard]] MemberId spokesman() const;
    MemberId add_pc(Character character);

    // Developer tools only (dev_tools/party_tester): player characters may
    // also take the two NPC positions, so up to eight of them fight. A game
    // party never allows it, and no save holds such a party.
    void open_npc_positions_to_player_characters() noexcept
    {
        npc_positions_ = NpcPositions::open_to_player_characters;
    }
    MemberId recruit(std::string source, Character converted, unsigned morale = 100);
    // Simulacrum from camp (LEVELS-3): `action`, one of the caster's camp
    // actions for the spell, makes a copy of `original` named `name` that
    // joins the party in an NPC position. The caster's earlier simulacrum is
    // destroyed; twelve hours pass. Atomic.
    MemberId create_simulacrum(MemberId caster, MemberId original, std::string_view action,
                               std::string name);
    // Whether the member is a simulacrum, which gains no levels.
    [[nodiscard]] bool is_simulacrum(MemberId id) const;
    // Reincarnate from camp (LEVELS-5): the species of the body the next
    // casting forms. The dice are read, not spent, until reincarnate().
    [[nodiscard]] std::string reincarnation_species() const;
    // `action`, one of the caster's camp actions for Reincarnate, gives the
    // dead `target` a body of reincarnation_species() (reincarnation_draft()),
    // with that species' `choices` and `appearance`; Raise Dead's offering is
    // paid. Atomic.
    void reincarnate(const rules::CharacterRules &creation, MemberId caster, MemberId target,
                     std::string_view action, ReincarnationChoices choices,
                     por::CharacterAppearance appearance);
    void rejoin(MemberId id);
    void remove(MemberId id);
    // Moves a player character to `place` (from 0) among the party's player
    // characters, the others keeping their order. A fight stands the party
    // in this order, the first half in the front rank (PLACE-1). The
    // characters swap positions; no position is emptied or filled.
    void move_member(MemberId id, std::size_t place);
    [[nodiscard]] std::vector<rules::EquipmentChoice> equipment_choices(MemberId id,
                                                                        std::uint64_t item) const;
    // An alignment-bound sword (#267) taken up by a member of another
    // alignment stays in the pack and burns the member for its damage, which
    // is returned; otherwise nothing is returned.
    std::optional<rules::HazardAttackResult>
    equip(MemberId id, std::uint64_t item,
          rules::EquipmentOperation operation = rules::EquipmentOperation::equip);
    void unequip(MemberId id, std::uint64_t item);
    [[nodiscard]] rules::EquipmentInfo equipment_info(MemberId id, std::uint64_t item) const;
    // The party's gold pays; the item goes to the member.
    void purchase(MemberId id, const por::Equipment &item);
    // The member picks up `item` for nothing, from a treasure. Throws when
    // it would take the member past its carrying capacity.
    void take_item(MemberId id, const por::Equipment &item);
    // What the member carries weighs, in hundredths of a pound: each item at
    // the rules' weight, or at its original record's (ten coins to the pound)
    // when the rules give none. Coins weigh nothing.
    [[nodiscard]] unsigned carried_weight(MemberId id) const;
    // The most the member can carry, in pounds.
    [[nodiscard]] unsigned carrying_capacity(MemberId id) const;
    // Whether the member could use `item` fully (the rules' can_use_item):
    // a shop or a treasure shows the rest in dark red.
    [[nodiscard]] bool can_use(MemberId id, const por::Equipment &item) const;
    // Whether `item` is armor or a shield that would not raise the member's
    // Armor Class in place of the armor or shield it wears, judged as
    // auto-equip judges it (EQUIP-1): Scale Mail on a Barbarian whose
    // Unarmored Defense is as good. A shop shows such an item in gray.
    [[nodiscard]] bool adds_no_armor_class(MemberId id, const por::Equipment &item) const;
    [[nodiscard]] ItemPreview preview(MemberId id, const por::Equipment &item) const;
    // Where the rules put an original item: a weapon, armor, a shield, worn
    // or carried; a shop's filter sorts its stock by it.
    [[nodiscard]] rules::EquipmentSlot equipment_slot(const por::Equipment &item) const;
    // Auto-equip (EQUIP-1): a melee weapon, armor, shield or worn item (a
    // ring, bracers, a cloak, gauntlets, a necklace or boots) the member just
    // gained goes on when the gear it makes rates better than what the
    // member wears, by the member's gear preference, and a worn item in a free
    // place unless it rates worse; whatever it replaces goes to the pack. Untrained armor, armor
    // the member lacks the Strength for, a known cursed item and, when asked, an unidentified magic
    // item stay in the pack. Nothing is returned when nothing changed.
    std::optional<GearChange> auto_equip(MemberId id, std::uint64_t item,
                                         UnidentifiedMagic unidentified);
    // Reads a Manual of Bodily Health the member carries (MAGIC-3):
    // Constitution and its maximum rise by 2, Hit Points with the new maximum,
    // and the book's magic is spent.
    void read_manual(MemberId id, std::uint64_t item);
    // Drinks one of a potion stack the member carries, outside combat (MAGIC-3).
    void drink_potion(MemberId id, std::uint64_t item);
    // Spends a charge of a Wand of Secrets the member carries, outside combat
    // (SECRET-1). Throws when the wand has none left or the member cannot act.
    void use_wand_of_secrets(MemberId id, std::uint64_t item);
    // The shop's ID: for 200 gp from the party purse, any item's hidden name
    // words are revealed, as in the original. False when there were none.
    bool identify_at_shop(MemberId id, std::uint64_t item);
    // What the shop in `market` offers for an item the member carries, in
    // gold pieces, for the share of a stack still carried (shop_offer, SELL-1).
    // The higher Charisma modifier of the member and the leader bargains. An
    // item with no original record, such as a scroll a member wrote, is worth
    // nothing.
    [[nodiscard]] unsigned sale_offer(MemberId id, std::uint64_t item,
                                      const SaleMarket &market) const;
    // The shop's Sell: the whole entry goes, taken off first when equipped,
    // and sale_offer() goes to the party purse in gold, which is returned.
    // Throws, changing nothing, for a cursed item, worn or packed (SELL-1).
    unsigned sell(MemberId id, std::uint64_t item, const SaleMarket &market);
    // Drop: the whole entry is gone for good, taken off first when equipped.
    // Throws, changing nothing, for a cursed item, worn or packed (SELL-1).
    void drop(MemberId id, std::uint64_t item);
    // The original's DESTROY ITEMS (Curse's opcode 0x40): every item of this
    // original type the party's members carry is gone, cursed or not.
    void destroy_items(std::uint8_t type);
    void set_wealth(Purse wealth);
    // Money a joining character brings; throws if the party purse cannot hold it.
    void add_coins(Coin coin, std::uint32_t count);
    // Each living member gains `amount`, or only `only` when it is given (a
    // duel's champion, as the original's other members stand aside).
    void award_experience(unsigned amount, std::string reward_id, MemberId only = {});
    // A training hall's experience session: half the experience between the
    // member's level and the next, once per level, paid in the party's gold.
    [[nodiscard]] ExperienceOffer experience_offer(MemberId id, TrainingHall hall) const;

    // Training under rules that train at halls (the original's): the member's
    // classes this hall would raise a level now, or why it turns the member
    // away.
    struct HallTraining
    {
        TrainingDenial denial{TrainingDenial::none};
        std::vector<std::string> classes;
    };

    [[nodiscard]] HallTraining hall_training(MemberId id, TrainingHall hall) const;
    // Pays the fee and trains the member: a level in the classes
    // hall_training() names, a magic-user's learning `spells` (one). Atomic:
    // throws, changing nothing, when the hall cannot.
    void train_at_hall_level(MemberId id, TrainingHall hall, std::vector<std::string> spells);
    // Whether the member has the experience and the state to gain a level,
    // wherever the rules let it.
    [[nodiscard]] bool ready_to_advance(MemberId id) const;
    // Atomic: throws, changing nothing, unless experience_offer() allows it.
    void buy_experience(MemberId id, TrainingHall hall);
    // Why the hall cannot teach the member a skill now; none when it can. The
    // fee of each lesson is checked when it is bought.
    [[nodiscard]] TrainingDenial skill_training_denial(MemberId id, TrainingHall hall) const;
    // The skills and Expertise the hall teaches the member, as whole grants.
    [[nodiscard]] rules::TrainingChoiceGroup skill_lessons(MemberId id, TrainingHall hall) const;
    // One lesson per level, paid in the party's gold. Atomic.
    void train_skill(MemberId id, TrainingHall hall, const std::string &grant);
    [[nodiscard]] bool can_advance(MemberId id) const;
    // The classes the member's next level may be taken in (MULTI-1); one it
    // does not qualify for is unavailable.
    [[nodiscard]] std::vector<rules::AdvancementOption> advancement_classes(MemberId id) const;
    [[nodiscard]] rules::AdvancementOptions
    advancement_options(MemberId id, const rules::AdvancementChoice &choice = {}) const;
    [[nodiscard]] rules::AdvancementChoice default_advancement(MemberId id) const;
    [[nodiscard]] PartyMember preview_advancement(MemberId id,
                                                  const rules::AdvancementChoice &choice) const;
    void advance(MemberId id, const rules::AdvancementChoice &choice);
    // Long Rest spell choices for a member offered them by the completed rest.
    [[nodiscard]] rules::SpellChoiceOptions spell_choice_options(MemberId id) const;
    // The rest the pending spell choices follow: a Long Rest, or a Short Rest
    // for Memorize Spell.
    [[nodiscard]] rules::SpellChoiceContext rest_spell_context() const;
    [[nodiscard]] PartyMember preview_spell_choices(MemberId id, const rules::SpellChoices &) const;
    void choose_spells(MemberId id, const rules::SpellChoices &);
    void keep_rest_spells(MemberId id);
    [[nodiscard]] PartyMember preview_rest_training(RestTicket, MemberId,
                                                    std::span<const std::string>) const;
    void replace_rest_training(RestTicket, MemberId, std::span<const std::string>);
    void keep_rest_training(RestTicket, MemberId);
    // Atomic original loot delivery. A full party purse leaves it unclaimed.
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
    // Whether a temple's service would help an active member (TEMPLE-1);
    // Remove Curse also frees one from its cursed items.
    [[nodiscard]] bool temple_service_helps(MemberId target, rules::TempleService service) const;
    // A temple casts the service on an active member it would help, paid
    // from the party's gold.
    void temple_service(MemberId target, rules::TempleService service, std::uint32_t price_gp);
    // An attack from outside combat, such as an arrow trap, on a random active
    // conscious member; nothing when none is conscious.
    std::optional<HazardHit> hazard_attack(const rules::HazardAttack &attack);
    // The same on an active member a script chose, such as a fall down a well;
    // nothing when that member is already down.
    std::optional<rules::HazardAttackResult> hazard_attack(MemberId target,
                                                           const rules::HazardAttack &attack);
    // The chance in percent that the member climbs a sheer wall (CADORNA-1).
    [[nodiscard]] unsigned climbing_chance(MemberId id) const;
    // The chance in percent that the member picks a lock or disarms a trap
    // with a DC 15 Dexterity (Sleight of Hand) check: a conscious Rogue only,
    // doubled with Thieves' Tools (LOCK-1, KOVEL-1).
    [[nodiscard]] unsigned thieves_tools_chance(MemberId id) const;
    // The member's current Hit Point maximum, raised by Aid.
    [[nodiscard]] int hit_point_maximum(MemberId id) const;
    // The member's status effects, such as Aid or a used Relentless Endurance.
    [[nodiscard]] std::vector<rules::StatusEffectView> status_effects(MemberId id) const;
    // Spells and features an active member can use outside combat (CLASS-3).
    [[nodiscard]] std::vector<rules::CampAction> camp_actions(MemberId id) const;
    // Atomic: on failure nothing changes.
    void use_camp_action(MemberId user, MemberId target, std::string_view action);
    // The first active member who can cast Identify now, if any. Identify is
    // cast on an item, so camp_actions() leaves it out.
    [[nodiscard]] std::optional<MemberId> identifier() const;
    // Identify, cast by `caster` on an item `holder` carries. False when the
    // item hid nothing; the spell slot is spent either way.
    bool cast_identify(MemberId caster, MemberId holder, std::uint64_t item);
    // Whether Detect Magic is in effect: item lists mark magic items.
    [[nodiscard]] bool detecting_magic() const noexcept
    {
        return state_.time_minutes < state_.detect_magic_until;
    }
    // Whether Detect Magic would sense the member's item: an enchantment, a
    // curse, charges or a spell.
    [[nodiscard]] bool magical(MemberId id, std::uint64_t item) const;
    // The spells the member can read outside combat from a scroll it carries
    // (MAGIC-6). Each action's id names the spell's place on the scroll.
    [[nodiscard]] std::vector<rules::CampAction> scroll_actions(MemberId id,
                                                                std::uint64_t item) const;
    // Reads one of scroll_actions() on `target`, which a spell for the whole
    // party ignores; the spell leaves the scroll, and a scroll with none left
    // is gone. False when the reading failed (Use Magic Device's Arcana check)
    // and the spell was lost to no effect. Atomic: on a throw nothing changes.
    bool read_scroll(MemberId reader, std::uint64_t item, MemberId target,
                     std::string_view action);
    // A spell on a scroll the member can copy into its spellbook now: its place
    // on the scroll and its level.
    struct ScrollCopy
    {
        unsigned entry{};
        std::string spell;
        unsigned level{};
    };
    [[nodiscard]] std::vector<ScrollCopy> copyable_spells(MemberId id, std::uint64_t item) const;
    // Copies a spell from a scroll the member carries into its spellbook
    // (MAGIC-6): it takes a bottle of Magic Ink of the spell's level or higher
    // the member carries and two hours for each spell level, and the spell
    // leaves the scroll.
    void copy_scroll_spell(MemberId id, std::uint64_t item, unsigned entry);
    // The spells the member can write on a scroll now (MAGIC-6), with a
    // bottle of Magic Ink it carries.
    [[nodiscard]] std::vector<rules::ScrollWriting> writable_spells(MemberId id) const;
    // Writes a scroll of one spell (MAGIC-6): it takes a bottle of Magic Ink
    // of the spell's level or higher the member carries and two hours for each
    // spell level, and no spell slot. The Camp dialog offers it.
    void write_scroll(MemberId id, std::string_view spell);
    // The member's Charisma as the party's spokesman, with Friends (MAGIC-6).
    [[nodiscard]] int reaction_charisma(MemberId id) const;
    // Whether Find Traps is in effect (MAGIC-6).
    [[nodiscard]] bool finding_traps() const noexcept
    {
        return state_.time_minutes < state_.find_traps_until;
    }
    // Eligible active members, in party order, try once each until one meets
    // the difficulty. Returns every attempt; the door opens if the last succeeded.
    [[nodiscard]] std::vector<DoorAttempt> try_door(DoorMethod method, int difficulty);
    [[nodiscard]] bool can_try_door(DoorMethod method) const;
    // The original's search for a secret door (SECRET-1): each conscious
    // active member in party order rolls a d6 and finds it on 1 while the
    // party searches, an elf on 1 or 2, or not at all otherwise, an elf on 1.
    // Rolls stop at the first find; the door is found if the last succeeded.
    [[nodiscard]] std::vector<SecretDoorRoll> search_for_secret_door(bool searching);
    // Minutes, seconds and milliseconds all convert; a bare number does not
    // (Effective C++ Item 18).
    void advance_time(std::chrono::milliseconds elapsed);

    [[nodiscard]] std::uint64_t time_hours() const noexcept
    {
        return state_.time_minutes / 60;
    }

    [[nodiscard]] rules::CharacterProfile profile(MemberId id) const;
    [[nodiscard]] rules::AbilityCheckModifier ability_check(MemberId id, rules::Ability ability,
                                                            std::string_view skill = {}) const;
    [[nodiscard]] rules::RecoveryInfo recovery_info(MemberId id) const;
    // Applies the script's HP and coin changes. The script sees the party purse
    // as the slot's money, and coins are seen and settled as script_coins()
    // describes; the result reports any change the purse made.
    // The cells are Pool of Radiance's, moved to where `game` keeps them.
    [[nodiscard]] std::optional<CoinExchange>
    read_character(PartySlot slot, const por::EclMachine &vm,
                   GoldBoxGame game = GoldBoxGame::pool_of_radiance);

    [[nodiscard]] PartyState checkpoint() const
    {
        return state_;
    }

    void restore(PartyState state);
    static void validate(const PartyState &state,
                         NpcPositions npc_positions = NpcPositions::npcs_only);
    static void validate_rest_choices(const PartyState &state, const rules::RulesModule &rules,
                                      NpcPositions npc_positions = NpcPositions::npcs_only);
    [[nodiscard]] std::vector<rules::Participant> participants() const;
    void begin_combat();
    // Combat changes vitals only: characters keep their gear throughout.
    void apply_combat(const rules::Snapshot &snapshot);

    // A simulacrum that melted away in the fight leaves the party.
    void end_combat() noexcept;

    [[nodiscard]] bool in_combat() const noexcept
    {
        return combat_;
    }

    [[nodiscard]] const rules::RulesModule &rule_module() const noexcept
    {
        return *rules_;
    }

    [[nodiscard]] rules::Identity identity() const
    {
        return rules_->identity();
    }

    // The game's character rules; only a game's party has them.
    [[nodiscard]] const std::shared_ptr<const rules::CharacterRules> &character_rules() const;

    // The setting the game chose for its rules (CharacterRules::game_setting),
    // empty for a party without character rules.
    [[nodiscard]] std::string game_setting() const
    {
        return creation_ ? creation_->game_setting() : std::string{};
    }

  private:
    // The member's next level, or 0 at its rules cap or the Training Hall's
    // limit (LEVELS-2).
    [[nodiscard]] unsigned next_level(const PartyMember &m) const;
    [[nodiscard]] std::vector<DoorAttempt> cast_knock(int difficulty);
    void change_equipment(MemberId id, std::uint64_t item, rules::EquipmentOperation operation);
    // One way auto-equip could put an item on: equipment changes in order.
    using EquipmentPlan = std::vector<std::pair<std::uint64_t, rules::EquipmentOperation>>;
    [[nodiscard]] std::vector<EquipmentPlan>
    equipment_plans(const PartyMember &m, std::uint64_t item, UnidentifiedMagic unidentified) const;
    // The member's gear rated as the party knows it: an unidentified item by
    // its plain stats.
    [[nodiscard]] rules::GearRating known_gear_rating(const PartyMember &m) const;

    // The rules key the party knows `item` by, and the member's known gear
    // with it in place of the gear it would replace: a weapon replaces the
    // weapons held, and the shield too when it takes both hands; armor the
    // armor worn; a shield the shield and the weapons. No gear for an item
    // of another slot.
    struct GearInPlace
    {
        std::string key;
        rules::EquipmentSlot slot{};
        std::vector<std::string> gear;
    };

    [[nodiscard]] GearInPlace gear_in_place(const PartyMember &m, const por::Equipment &item) const;
    std::optional<rules::HazardAttackResult> refuse_bound_weapon(MemberId id, std::uint64_t item);
    std::unique_ptr<rules::RulesModule> rules_;
    std::shared_ptr<const rules::CharacterRules> creation_;
    PartyState state_;
    NpcPositions npc_positions_{NpcPositions::npcs_only};
    bool combat_{};
    bool combat_registered_{};
    std::uint64_t combat_elapsed_{};
    unsigned combat_offerings_{}; // gold the current fight's offerings already took
    // The fight's members whose identity apply_combat() has checked, with the
    // Hit Point maximum it was checked at: a member is checked again only
    // when that changes.
    std::vector<std::pair<MemberId, int>> combat_checked_;
    void elapse(PartyState &state, std::chrono::milliseconds elapsed,
                std::span<const MemberId> in_combat = {}) const;

    // Time passing, worked out without touching the party (everything that
    // can throw), then applied with moves only, so a caller need not copy the
    // whole party to stay atomic.
    struct Elapsed
    {
        std::vector<std::pair<MemberId, rules::VitalState>> vitals;
        RandomState random;
        std::uint64_t time_minutes{};
        unsigned subminute_milliseconds{};
    };

    [[nodiscard]] Elapsed elapsed(const PartyState &state, std::chrono::milliseconds elapsed,
                                  std::span<const MemberId> in_combat = {}) const;
    static void apply(PartyState &state, Elapsed &&elapsed) noexcept;
    // A member of `state`, which may be a working copy of state_. Throws for a
    // missing id rather than dereferencing the end of the roster.
    [[nodiscard]] static const PartyMember &member_in(const PartyState &state, MemberId id);
    [[nodiscard]] static PartyMember &member_in(PartyState &state, MemberId id);
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
// The item's definition under these rules: its conversion to the SRD's
// catalog, or its original record for rules that keep the original's items.
[[nodiscard]] std::string item_definition(const rules::RulesModule &rules,
                                          const por::Equipment &item);
// How many an original item counts for in an inventory: a stack's items, or a
// charged item's charges or doses (MAGIC-3), otherwise one.
[[nodiscard]] unsigned carried_quantity(const por::Equipment &item);
// The same under these rules: under rules that play the original's records,
// every charged item of the original's counts its charges.
[[nodiscard]] unsigned carried_quantity(const rules::RulesModule &rules,
                                        const por::Equipment &item);
// The original item as a fight left it: `left` of its stacked items, charges
// or doses, or for a scroll the spells whose bits are set in `left` (MAGIC-6).
// Nothing when none is left.
[[nodiscard]] std::optional<por::Equipment> item_after_uses(por::Equipment item, unsigned left);
// The item's key as the rules see it: ":unidentified" is added while its magic
// is not identified (#262).
[[nodiscard]] std::string rules_definition(const PartyMember &member, const InventoryItem &item);
// The charges a member's Wand of Secrets has left (SECRET-1). A spent wand
// stays in the pack, and each Long Rest gives it back 1d3, up to 3.
[[nodiscard]] unsigned wand_charges(const PartyMember &member, std::uint64_t item);
void set_wand_charges(PartyMember &member, std::uint64_t item, unsigned charges);
// Reveals the item's hidden name words, so it takes its full name. False when
// nothing was hidden.
bool identify_item(PartyMember &member, std::uint64_t item);
} // namespace opengold
#endif
