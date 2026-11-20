#ifndef OPENGOLD_BUCK_ROGERS_UPKEEP_H
#define OPENGOLD_BUCK_ROGERS_UPKEEP_H
#include "opengold/buck_rogers_records.h"
#include "opengold/buck_rogers_rules.h"
#include "opengold/gold_box_game.h"
#include "opengold/random_state.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string_view>
#include <vector>

// The team's upkeep in the two Buck Rogers games, Countdown to Doomsday and
// Matrix Cubed, under their original rules: the wounded after a fight, the
// medics' treatment, poison and antidotes, healing with time and rest, the
// credit pool, shops, ammunition and bundles (docs/games/countdown.md,
// "Upkeep and money"). The engine routines are the same in both games, so
// these rules are too. The bank, the hospital, the port and the ship's
// Medical Center are the games' own scripts, which the script host runs;
// script_operand_tag corrects the one mistake found in them.
namespace opengold::original
{
// A character's status, the record's status byte (types.pas TStatus). A
// dying character is bleeding: it loses a point each round of a fight and
// slips into a coma when it has lost ten.
enum class BuckRogersStatus : std::uint8_t
{
    no_status,
    okay,
    gone,
    dead,
    dying,
    unconscious,
    fleeing,
    poisoned,
    comatose,
    not_here,
    dodging,
    sprinting,
    failed_zero_g,
    used_jetpack,
    using_jetpack
};

[[nodiscard]] BuckRogersStatus status_of(const buck_rogers::Creature &character) noexcept;
// Sets the status and whether the character can act: only an okay character
// can.
void set_status(buck_rogers::Creature &character, BuckRogersStatus status) noexcept;

// A member of the team as these rules see it: its record, its items in the
// gear list's order, and the two effects of its effect list (.sfx) that
// upkeep reads.
struct TeamMember
{
    buck_rogers::Creature character;
    std::vector<buck_rogers::Item> items;
    // Holds the Dizzy or the Stunned effect (c_effect.pas DIZZY, STUNNED).
    bool stunned{};
    // Holds the Unkillable effect (c_effect.pas UNKILLABLE).
    bool unkillable{};
};

// The item type of an antidote (postcbt.pas ANTIDOTE_TYPE).
inline constexpr std::uint8_t antidote_type = 31;
// The flag bit of an item that is never joined or halved (c_item.pas).
inline constexpr std::uint8_t not_joinable = 0x20;
// The most items a character carries (c_item.pas MAX_CARRIED_ITEMS), and
// the most a joined bundle holds (viewchar.pas MAX_STACK).
inline constexpr std::size_t max_carried_items = 16;
inline constexpr int max_bundle = 100;

// After a fight the team did not flee or lose (postcbt.pas
// DetermineCombatResults): a member who dodged, sprinted or failed in zero
// gravity is okay again, and one who fled comes back. One who is bleeding
// falls unconscious, and an unconscious one with Hit Points comes round.
// When the script asked for it (cell 7EE6h), the bleeding and the
// unconscious are instead put back on their feet with at least 1 Hit Point.
// What a flight or a defeat does to the team, and the experience and booty,
// are the combat session's.
void recover_after_fight(std::span<TeamMember> team, bool revives_team);

// One line of the treatment report.
struct TreatmentLine
{
    enum class Kind
    {
        // A Medic's Treat Poisoning cured the patient.
        poison_cured,
        // No Medic cured it; an antidote that `helper` carried did.
        antidote_used,
        // Neither: the patient slips into a coma.
        coma,
        // `helper` healed `hit_points` of the patient's wounds with `skill`.
        wounds_healed
    };

    Kind kind{};
    std::size_t helper{}, patient{};
    int hit_points{};
    Skill skill{};
};

// The report's last line.
enum class TreatmentSummary
{
    no_injuries,
    no_medics,
    nothing_treated,
    healing_completed
};

struct TreatmentReport
{
    std::vector<TreatmentLine> lines;
    TreatmentSummary summary{};
};

// The question the original asks when no Medic cures a poisoned member and
// someone carries an antidote: whether to use the carrier's on the patient.
// It asks about the first carrier in the team's order only.
using AntidoteChoice = std::function<bool(std::size_t patient, std::size_t carrier)>;

// The medics' work after a fight (postcbt.pas TreatInjuries), in two parts.
// Poison first: each poisoned member is tried by every okay Medic in turn
// with Treat Poisoning until one succeeds; failing that, an antidote is
// offered; failing that, the patient slips into a coma. A cured patient is
// unconscious with 0 Hit Points. Then wounds: each member of the team who is
// okay, bleeding or unconscious and short of Hit Points is given First Aid
// by every okay member in turn (1d6, never above 6 Hit Points), then the
// first success of Treat Critical Wounds (twice 3d8 + 3), Treat Serious
// Wounds (twice 2d8 + 1) or Treat Light Wounds (twice 1d8). Treating one's
// own critical wounds is at difficulty 4, every other check at 2. A healed
// patient is okay. Last, an Unkillable member below 45 Hit Points stands up
// with 45.
//
// PostCombat runs it after every ground fight that began and every ship
// fight that ended without boarding, unless the team was destroyed or a
// script set cell 4B80h.
[[nodiscard]] TreatmentReport
treat_injuries(std::span<TeamMember> team, const AntidoteChoice &use_antidote, RandomState &random);

// The members a medic can see to with AID (viewchar.pas FindPatients): the
// bleeding, when the medic has First Aid or Treat Light Wounds, and the
// dizzy or stunned who can act, when it has Treat Stun. The original offers
// AID in a ground fight, where the combat session keeps only the patients
// next to the medic, and to the crew in a ship fight, where any will do.
[[nodiscard]] std::vector<std::size_t> find_patients(std::size_t medic,
                                                     std::span<const TeamMember> team);

struct PatientTreatment
{
    enum class Kind
    {
        // The bleeding patient is bandaged: unconscious, no longer losing
        // Hit Points.
        bandaged,
        // A Treat Stun check ended the patient's dizziness and stun.
        stun_ended
    };

    std::size_t patient{};
    Kind kind{};
};

// One die of `sides` faces, 1 to `sides`, from wherever the caller's dice
// come: the ship fight's AID rolls with its own dice.
using DieRoll = std::function<int(int sides)>;

// AID (viewchar.pas TreatPatients): the medic bandages each bleeding patient
// without a roll and tries Treat Stun on each dizzy or stunned one. Returns
// what it did, in the patients' order. The ground fight and the ship fight
// both treat by it.
std::vector<PatientTreatment> treat_patients(std::size_t medic, std::span<TeamMember> team,
                                             std::span<const std::size_t> patients,
                                             RandomState &random);
std::vector<PatientTreatment> treat_patients(std::size_t medic, std::span<TeamMember> team,
                                             std::span<const std::size_t> patients,
                                             const DieRoll &roll);

// How many days pass for a rest of the rest menu (PROGRAM 9; explore.pas
// RestMenu) and for a step of the rocket on the solar map (explore.pas
// MoveRocket).
inline constexpr int rest_days = 1;
inline constexpr int solar_map_step_days = 3;

// Days of time (gametime.pas RestAndHeal): each unconscious member comes
// round, and each okay one gains a Hit Point a day, up to its maximum. The
// bleeding, poisoned and comatose gain nothing. The caller moves the clock
// on by the days.
void rest_and_heal(std::span<TeamMember> team, int days);

// The team's pool of credits (gdvars.pas poolCredits), shared by the booty
// menu after a fight and by the shops.
struct CreditPool
{
    std::uint32_t credits{};
};

// The npc byte of a computer-run character the team treats as its own
// (c_npc.pas PARTY_MEMBER_NPC).
inline constexpr std::uint8_t party_member_npc = 0xB3;

// Whether POOL takes the member's credits (money.pas PoolMoney): a player
// character (npc byte 0) or a party-member NPC.
[[nodiscard]] bool pools_credits(const buck_rogers::Creature &character) noexcept;
// POOL: every such member's credits go into the pool.
void pool_credits(std::span<TeamMember> team, CreditPool &pool);
// DIVVY (money.pas ShareMoney): the pool is shared out evenly among the
// members POOL takes from, the remainder a credit each to the first of them,
// and emptied. The original divided by those members but paid the player
// characters, so a party-member NPC's share was lost (Bugs).
void share_credits(std::span<TeamMember> team, CreditPool &pool);
// TAKE (money.pas TakeMoney): the character takes the amount from the pool,
// or all of it when the pool holds less.
void take_credits(buck_rogers::Creature &character, CreditPool &pool,
                  std::uint32_t amount) noexcept;
// Credits handed from one character to another on the character sheet
// (viewchar.pas TradeTreasureMenu), at most what the giver has. The
// original moved only the amount's last 16 bits (Bugs).
void give_credits(buck_rogers::Creature &giver, buck_rogers::Creature &receiver,
                  std::uint32_t amount) noexcept;

// The number of shop lists SHOP (4Bh) may name, from 1 (gdconsts.pas
// itemListStarts).
inline constexpr unsigned shop_list_count = 16;

// The items SHOP stocks from a list (eclvm.pas EclShop), in the order the
// shop shows them: the list's last item first. A list entry whose template
// ITEM0.DAX does not hold is left out. Throws std::out_of_range for a list
// number outside 1 to 16.
[[nodiscard]] std::vector<buck_rogers::Item>
shop_stock(unsigned list, std::span<const buck_rogers::Item> templates);

// What a shop asks for an item (shop.pas ItemPrice, ShopBuyItemSelection):
// its value, 1 when it has none, times the multiplier a script saved in cell
// 7F6Dh over 16; the value itself when the multiplier is 0.
[[nodiscard]] std::uint32_t shop_price(const buck_rogers::Item &item,
                                       std::uint8_t multiplier) noexcept;

enum class PurchaseResult
{
    bought,
    not_enough_money,
    overloaded
};

// BUY (shop.pas ShopBuyMenu, GiveItemCopyToCreature): the buyer's own
// credits pay when they suffice, else all of them and the rest from the
// pool. The item goes to the end of the buyer's gear and joins a like bundle;
// a buyer left with more than 16 items keeps none of it and pays nothing.
// The original then took back the new item even when the join had moved a
// full bundle's rounds into it (Bugs).
[[nodiscard]] PurchaseResult buy_item(TeamMember &buyer, const buck_rogers::Item &item,
                                      std::uint8_t multiplier, CreditPool &pool);

// What a shop pays for an item (viewchar.pas SellItem): half its value, for
// every item of a bundle; 0 for an item worth nothing, which the shop takes
// away for nothing.
[[nodiscard]] std::uint32_t sale_price(const buck_rogers::Item &item) noexcept;
// SELL: the item leaves the seller's gear for its sale price.
void sell_item(TeamMember &seller, std::size_t item);

// Whether AMMO lists an item (shop.pas ShopAmmoMenu): a weapon, readied or
// not, that is neither a melee weapon nor a grenade, and is never joined.
// Its quantity is the rounds it holds.
[[nodiscard]] bool takes_ammunition(const buck_rogers::Item &weapon,
                                    std::span<const buck_rogers::ItemType> types);

struct AmmunitionOffer
{
    int rounds{};
    std::uint32_t cost{};
};

// A clip or a fill for a weapon (shop.pas AmmoValues): a clip is 10 rounds
// and a fill up to 250, each 10 rounds or part of them costing the weapon
// type's price (gdconsts.pas ammoPrices); a rocket launcher or a plasma
// thrower holds 10 and is sold a round at a time. The shop's price
// multiplier does not apply. Throws std::invalid_argument for a weapon AMMO
// does not list.
[[nodiscard]] AmmunitionOffer ammunition_offer(const buck_rogers::Item &weapon,
                                               std::span<const buck_rogers::ItemType> types,
                                               bool fill);

enum class AmmunitionResult
{
    bought,
    already_full,
    not_enough_money
};

// Buying the clip or the fill, paid as BUY pays.
[[nodiscard]] AmmunitionResult buy_ammunition(TeamMember &buyer, std::size_t weapon,
                                              std::span<const buck_rogers::ItemType> types,
                                              bool fill, CreditPool &pool);

// HALVE (viewchar.pas HalveItem): a bundle of grenades or antidotes splits
// in two, the larger half staying where it was and the smaller, not readied,
// after it. False, and nothing changes, for an item never halved, a bundle
// of one, or a character already carrying 16 items (the menu offers HALVE
// only below that).
bool halve_item(std::vector<buck_rogers::Item> &items, std::size_t item);
// JOIN (viewchar.pas JoinItem): every like item in the gear (the same name,
// type, bonuses, weight and specials) joins the chosen one, in the list's
// order, until a bundle holds 100; the rest makes the next bundle. An item
// with no quantity counts as one.
void join_item(std::vector<buck_rogers::Item> &items, std::size_t item);

// The width a script command reads an operand's cell with, by its tag (1 a
// byte, 3 a word, 5 four bytes). The hospital's check that the bank or the
// patient can pay the fee reads one byte of the two-byte fee it then
// charges: at the shops' multipliers of 16 and 32 the fee of 2,560 or 5,120
// credits reads as 0, so the check never refuses and the bank, then the
// patient, goes into debt. Where the script and
// address are the ones the game ships, the check reads the word (Bugs). Every
// other operand keeps its tag.
[[nodiscard]] std::uint8_t script_operand_tag(GoldBoxGame game, std::string_view archive,
                                              unsigned script, std::uint16_t address,
                                              std::size_t operand, std::uint8_t tag) noexcept;
} // namespace opengold::original
#endif
