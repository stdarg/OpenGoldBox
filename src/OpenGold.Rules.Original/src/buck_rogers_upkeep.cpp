#include "opengold/buck_rogers_upkeep.h"
#include "opengold/dice.h"

#include <algorithm>
#include <array>
#include <optional>
#include <stdexcept>

namespace opengold::original
{
namespace
{
using buck_rogers::Creature;
using buck_rogers::Item;
using buck_rogers::ItemType;
using Status = BuckRogersStatus;

// The record's sides (c_side.pas).
constexpr int team_side = 0;
constexpr int hostile_side = 1;
// The medics' checks are at difficulty 2, a medic's Treat Critical Wounds on
// itself at 4 (postcbt.pas TreatInjuries).
constexpr int standard_difficulty = 2;
constexpr int self_treatment_difficulty = 4;
// First Aid never brings a patient above this (postcbt.pas TreatInjuries).
constexpr int first_aid_ceiling = 6;
// The Hit Points the Unkillable effect keeps a member at.
constexpr int unkillable_hit_points = 45;
// Item type rows the shop and its ammunition counter name (c_equip.pas,
// c_wclass.pas).
constexpr std::uint8_t weapon_slot = 0;
constexpr std::uint8_t melee_class = 0, rocket_launcher_class = 7, grenade_class = 8,
                       plasma_class = 9;
constexpr int clip_rounds = 10, weapon_capacity = 250, area_weapon_capacity = 10;

// What one clip costs, by weapon type 5 to 18 (gdconsts.pas ammoPrices).
constexpr std::uint8_t first_priced_type = 5;
constexpr std::array<std::uint32_t, 14> clip_prices{10, 5,  10,  0,  35, 50, 100,
                                                    50, 35, 100, 80, 0,  80, 100};

// The shop lists (gdconsts.pas itemListStarts, itemListCounts,
// itemListEntries): each list's entries name ITEM0.DAX templates from 1.
constexpr std::array<std::uint8_t, shop_list_count> list_starts{1,  17, 22, 26, 33, 37, 42, 46,
                                                                51, 61, 69, 73, 79, 83, 88, 92};
constexpr std::array<std::uint8_t, shop_list_count> list_counts{16, 5, 4, 7, 4, 5, 4, 5,
                                                                10, 8, 4, 6, 4, 5, 4, 38};
constexpr std::array<std::uint8_t, 129> list_entries{
    1,  3,  4,  6,  7,  9,  10, 12, 14, 15, 20, 21, 28, 29, 30, 31, 2,  8,  11, 13, 16, 22,
    27, 29, 30, 2,  3,  4,  6,  8,  9,  13, 20, 21, 27, 31, 2,  8,  11, 13, 16, 20, 26, 28,
    29, 39, 42, 45, 47, 51, 26, 27, 30, 31, 35, 36, 37, 38, 92, 93, 1,  2,  3,  4,  8,  19,
    40, 41, 9,  13, 45, 48, 26, 34, 35, 36, 37, 38, 1,  3,  14, 16, 20, 21, 22, 27, 30, 10,
    11, 46, 50, 1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
    20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38};

// The hospital's fee checks, which read one byte of the two-byte fee at
// 7F79h (script 99 of Countdown's ECL6, script 20 of Matrix Cubed's ECL1).
struct ScriptTagFix
{
    GoldBoxGame game;
    std::string_view archive;
    unsigned script;
    std::uint16_t address;
    std::size_t operand;
    std::uint8_t shipped, meant;
};

constexpr std::uint8_t byte_cell_tag = 1, word_cell_tag = 3;
constexpr std::array<ScriptTagFix, 4> script_tag_fixes{{
    {GoldBoxGame::countdown_to_doomsday, "ECL6.DAX", 99, 0x84F1, 0, byte_cell_tag, word_cell_tag},
    {GoldBoxGame::countdown_to_doomsday, "ECL6.DAX", 99, 0x850B, 0, byte_cell_tag, word_cell_tag},
    {GoldBoxGame::matrix_cubed, "ECL1.DAX", 20, 0x853B, 0, byte_cell_tag, word_cell_tag},
    {GoldBoxGame::matrix_cubed, "ECL1.DAX", 20, 0x8555, 0, byte_cell_tag, word_cell_tag},
}};

[[nodiscard]] bool is(const Creature &character, Status status) noexcept
{
    return status_of(character) == status;
}

[[nodiscard]] int roll_dice(int count, int sides, RandomState &random)
{
    int total = 0;
    for (int die = 0; die < count; ++die)
    {
        total += roll_die(random, sides);
    }
    return total;
}

// A single character's check succeeds (SelectSkillCharacter with that
// character).
[[nodiscard]] bool succeeds(const Creature &character, Skill skill, int difficulty,
                            RandomState &random)
{
    const std::array<int, 1> value{skill_value(character, skill)};
    const auto grade = check_skill(value, difficulty, random).grade;
    return grade == SkillCheckGrade::success || grade == SkillCheckGrade::great_success;
}

[[nodiscard]] int hit_points_missing(const Creature &character) noexcept
{
    return std::max(0, character.hit_point_maximum - character.hit_points);
}

void heal(Creature &character, int hit_points)
{
    character.hit_points = static_cast<std::uint16_t>(character.hit_points + hit_points);
}

// Cured of poison: unconscious with nothing left, for the wound pass.
void cure_poison(Creature &patient) noexcept
{
    set_status(patient, Status::unconscious);
    patient.hit_points = 0;
}

[[nodiscard]] bool joinable(const Item &item) noexcept
{
    return (item.flags & not_joinable) == 0;
}

[[nodiscard]] bool alike(const Item &a, const Item &b) noexcept
{
    return a.name_parts == b.name_parts && a.type == b.type && a.plus == b.plus &&
           a.save_bonus == b.save_bonus && a.weight == b.weight && a.special == b.special;
}

void consume_one(std::vector<Item> &items, std::size_t item)
{
    if (items[item].quantity <= 1)
    {
        items.erase(items.begin() + static_cast<std::ptrdiff_t>(item));
        return;
    }
    --items[item].quantity;
}

[[nodiscard]] std::optional<std::size_t> antidote_carrier(std::span<const TeamMember> team,
                                                          std::size_t &antidote)
{
    for (std::size_t member = 0; member < team.size(); ++member)
    {
        const auto &items = team[member].items;
        const auto found = std::find_if(items.begin(), items.end(),
                                        [](const Item &item)
                                        {
                                            return item.type == antidote_type;
                                        });
        if (found != items.end())
        {
            antidote = static_cast<std::size_t>(found - items.begin());
            return member;
        }
    }
    return std::nullopt;
}

// The poison half of TreatInjuries. Returns whether a Medic was there to
// try, and sets cured_any when someone was cured.
bool treat_poison(std::span<TeamMember> team, const AntidoteChoice &use_antidote,
                  RandomState &random, TreatmentReport &report, bool &cured_any)
{
    bool medic_present = false;
    for (std::size_t patient = 0; patient < team.size(); ++patient)
    {
        auto &poisoned = team[patient].character;
        if (!is(poisoned, Status::poisoned))
        {
            continue;
        }

        bool cured = false;
        for (std::size_t medic = 0; medic < team.size() && !cured; ++medic)
        {
            const auto &healer = team[medic].character;
            if (career_from_record(healer.career) != Profession::medic || !is(healer, Status::okay))
            {
                continue;
            }

            medic_present = true;
            if (succeeds(healer, Skill::treat_poisoning, standard_difficulty, random))
            {
                cure_poison(poisoned);
                report.lines.push_back({TreatmentLine::Kind::poison_cured, medic, patient, 0});
                cured = true;
            }
        }

        if (cured)
        {
            cured_any = true;
            continue;
        }

        std::size_t antidote = 0;
        const auto carrier = antidote_carrier(team, antidote);
        if (carrier && use_antidote && use_antidote(patient, *carrier))
        {
            consume_one(team[*carrier].items, antidote);
            cure_poison(poisoned);
            report.lines.push_back({TreatmentLine::Kind::antidote_used, *carrier, patient, 0});
            cured_any = true;
            continue;
        }

        set_status(poisoned, Status::comatose);
        report.lines.push_back({TreatmentLine::Kind::coma, patient, patient, 0});
    }
    return medic_present;
}

[[nodiscard]] bool wounded(const Creature &character) noexcept
{
    const auto status = status_of(character);
    const bool treatable =
        status == Status::okay || status == Status::dying || status == Status::unconscious;
    return treatable && character.side == team_side &&
           character.hit_points < character.hit_point_maximum;
}

[[nodiscard]] bool can_treat(const Creature &medic) noexcept
{
    return is(medic, Status::okay) && medic.side != hostile_side && medic.hit_points > 0;
}

// One medic's try at one pass of the wound treatment. Pass 0 is First Aid,
// 1 to 3 the Treat Critical, Serious and Light Wounds skills.
struct WoundPass
{
    Skill skill;
    int dice, sides, bonus;
};

constexpr std::array<WoundPass, 4> wound_passes{{
    {Skill::first_aid, 1, 6, 0},
    {Skill::treat_critical_wounds, 3, 8, 3},
    {Skill::treat_serious_wounds, 2, 8, 1},
    {Skill::treat_light_wounds, 1, 8, 0},
}};

// The wound half of TreatInjuries for one patient. Returns whether any
// medic tried had the pass's skill.
bool treat_wounds(std::span<TeamMember> team, std::size_t patient, RandomState &random,
                  TreatmentReport &report, bool &healed_any)
{
    bool skilled_medic = false;
    auto &injured = team[patient].character;
    bool done = !wounded(injured);
    for (std::size_t pass = 0; pass < wound_passes.size(); ++pass)
    {
        const auto &step = wound_passes[pass];
        for (std::size_t medic = 0; medic < team.size() && !done; ++medic)
        {
            const auto &healer = team[medic].character;
            if (!can_treat(healer))
            {
                continue;
            }

            int amount = roll_dice(step.dice, step.sides, random) + step.bonus;
            if (pass == 0)
            {
                amount = std::min(amount, first_aid_ceiling - injured.hit_points);
            }
            else
            {
                amount *= 2;
            }
            amount = std::min(amount, hit_points_missing(injured));
            if (level_of(healer.skills, step.skill) > 0)
            {
                skilled_medic = true;
            }

            const bool on_self = medic == patient && step.skill == Skill::treat_critical_wounds;
            const int difficulty = on_self ? self_treatment_difficulty : standard_difficulty;
            if (amount <= 0 || !succeeds(healer, step.skill, difficulty, random))
            {
                continue;
            }

            heal(injured, amount);
            set_status(injured, Status::okay);
            report.lines.push_back(
                {TreatmentLine::Kind::wounds_healed, medic, patient, amount, step.skill});
            healed_any = true;
            done = pass != 0;
        }
    }
    return skilled_medic;
}

void keep_unkillable_standing(TeamMember &member)
{
    auto &character = member.character;
    if (!member.unkillable || character.side == hostile_side ||
        character.hit_points >= unkillable_hit_points)
    {
        return;
    }

    set_status(character, Status::okay);
    character.hit_points = unkillable_hit_points;
}

// Pays a price from the character's credits, or all of them and the rest
// from the pool (ShopBuyMenu, ShopAmmoMenu). False when both together fall
// short.
[[nodiscard]] bool can_pay(const Creature &buyer, const CreditPool &pool,
                           std::uint32_t price) noexcept
{
    return price <= static_cast<std::uint64_t>(buyer.credits) + pool.credits;
}

void pay(Creature &buyer, CreditPool &pool, std::uint32_t price) noexcept
{
    if (price <= buyer.credits)
    {
        buyer.credits -= price;
        return;
    }
    pool.credits -= price - buyer.credits;
    buyer.credits = 0;
}

[[nodiscard]] const ItemType &type_of(const Item &item, std::span<const ItemType> types)
{
    if (item.type >= types.size())
    {
        throw std::invalid_argument("The item's type is not in ITEMS");
    }
    return types[item.type];
}

[[nodiscard]] bool sold_by_the_round(const ItemType &type) noexcept
{
    return type.weapon_class == rocket_launcher_class || type.weapon_class == plasma_class;
}

[[nodiscard]] std::uint32_t clip_price(std::uint8_t weapon_type)
{
    const std::size_t row = static_cast<std::size_t>(weapon_type) - first_priced_type;
    if (weapon_type < first_priced_type || row >= clip_prices.size())
    {
        throw std::invalid_argument("The weapon type has no ammunition price");
    }
    return clip_prices[row];
}

[[nodiscard]] int capacity_of(const ItemType &type) noexcept
{
    return sold_by_the_round(type) ? area_weapon_capacity : weapon_capacity;
}
} // namespace

BuckRogersStatus status_of(const Creature &character) noexcept
{
    return static_cast<BuckRogersStatus>(character.status);
}

void set_status(Creature &character, BuckRogersStatus status) noexcept
{
    character.status = static_cast<std::uint8_t>(status);
    character.enabled = status == Status::okay ? 1 : 0;
}

void recover_after_fight(std::span<TeamMember> team, bool revives_team)
{
    for (auto &member : team)
    {
        auto &character = member.character;
        switch (status_of(character))
        {
        case Status::okay:
        case Status::dodging:
        case Status::sprinting:
        case Status::failed_zero_g:
        case Status::fleeing:
            set_status(character, Status::okay);
            break;
        case Status::dying:
            if (revives_team)
            {
                set_status(character, Status::okay);
                character.hit_points = 1;
                break;
            }
            character.status = static_cast<std::uint8_t>(Status::unconscious);
            break;
        case Status::unconscious:
            if (character.hit_points > 0)
            {
                set_status(character, Status::okay);
            }
            else if (revives_team)
            {
                set_status(character, Status::okay);
                character.hit_points = 1;
            }
            break;
        default:
            break;
        }
    }
}

TreatmentReport treat_injuries(std::span<TeamMember> team, const AntidoteChoice &use_antidote,
                               RandomState &random)
{
    TreatmentReport report;
    bool treated_any = false;
    const bool any_poisoned = std::any_of(team.begin(), team.end(),
                                          [](const TeamMember &member)
                                          {
                                              return is(member.character, Status::poisoned);
                                          });
    bool medic_available = treat_poison(team, use_antidote, random, report, treated_any);

    bool any_wounded = false;
    for (std::size_t patient = 0; patient < team.size(); ++patient)
    {
        any_wounded = any_wounded || wounded(team[patient].character);
        if (treat_wounds(team, patient, random, report, treated_any))
        {
            medic_available = true;
        }
        keep_unkillable_standing(team[patient]);
    }

    if (!any_poisoned && !any_wounded)
    {
        report.summary = TreatmentSummary::no_injuries;
    }
    else if (!medic_available)
    {
        report.summary = TreatmentSummary::no_medics;
    }
    else if (!treated_any)
    {
        report.summary = TreatmentSummary::nothing_treated;
    }
    else
    {
        report.summary = TreatmentSummary::healing_completed;
    }
    return report;
}

std::vector<std::size_t> find_patients(std::size_t medic, std::span<const TeamMember> team)
{
    const auto &skills = team[medic].character.skills;
    const bool bandages =
        level_of(skills, Skill::first_aid) > 0 || level_of(skills, Skill::treat_light_wounds) > 0;
    const bool treats_stun = level_of(skills, Skill::treat_stun) > 0;
    std::vector<std::size_t> patients;
    for (std::size_t candidate = 0; candidate < team.size(); ++candidate)
    {
        const auto &member = team[candidate];
        if (member.character.side != team_side)
        {
            continue;
        }

        const bool bleeding = is(member.character, Status::dying) && bandages;
        const bool dazed = member.character.enabled != 0 && member.stunned && treats_stun;
        if (bleeding || dazed)
        {
            patients.push_back(candidate);
        }
    }
    return patients;
}

std::vector<PatientTreatment> treat_patients(std::size_t medic, std::span<TeamMember> team,
                                             std::span<const std::size_t> patients,
                                             RandomState &random)
{
    const auto &healer = team[medic].character;
    const bool bandages = level_of(healer.skills, Skill::first_aid) > 0 ||
                          level_of(healer.skills, Skill::treat_light_wounds) > 0;
    std::vector<PatientTreatment> done;
    for (const auto patient : patients)
    {
        auto &member = team[patient];
        if (is(member.character, Status::dying) && bandages)
        {
            set_status(member.character, Status::unconscious);
            done.push_back({patient, PatientTreatment::Kind::bandaged});
            continue;
        }

        if (member.stunned && succeeds(healer, Skill::treat_stun, standard_difficulty, random))
        {
            member.stunned = false;
            done.push_back({patient, PatientTreatment::Kind::stun_ended});
        }
    }
    return done;
}

void rest_and_heal(std::span<TeamMember> team, int days)
{
    for (auto &member : team)
    {
        auto &character = member.character;
        if (is(character, Status::unconscious))
        {
            set_status(character, Status::okay);
        }
        if (!is(character, Status::okay))
        {
            continue;
        }

        set_status(character, Status::okay);
        const int healed = character.hit_points + days;
        character.hit_points =
            static_cast<std::uint16_t>(std::min<int>(healed, character.hit_point_maximum));
    }
}

bool pools_credits(const Creature &character) noexcept
{
    return character.npc == 0 || character.npc == party_member_npc;
}

void pool_credits(std::span<TeamMember> team, CreditPool &pool)
{
    for (auto &member : team)
    {
        if (pools_credits(member.character))
        {
            pool.credits += member.character.credits;
            member.character.credits = 0;
        }
    }
}

void share_credits(std::span<TeamMember> team, CreditPool &pool)
{
    const auto sharers =
        static_cast<std::uint32_t>(std::count_if(team.begin(), team.end(),
                                                 [](const TeamMember &member)
                                                 {
                                                     return pools_credits(member.character);
                                                 }));
    if (pool.credits == 0 || sharers == 0)
    {
        return;
    }

    const std::uint32_t share = pool.credits / sharers;
    std::uint32_t remainder = pool.credits % sharers;
    for (auto &member : team)
    {
        if (!pools_credits(member.character))
        {
            continue;
        }

        member.character.credits += share;
        if (remainder > 0)
        {
            ++member.character.credits;
            --remainder;
        }
    }
    pool.credits = 0;
}

void take_credits(Creature &character, CreditPool &pool, std::uint32_t amount) noexcept
{
    const auto taken = std::min(amount, pool.credits);
    pool.credits -= taken;
    character.credits += taken;
}

void give_credits(Creature &giver, Creature &receiver, std::uint32_t amount) noexcept
{
    const auto given = std::min(amount, giver.credits);
    giver.credits -= given;
    receiver.credits += given;
}

std::vector<Item> shop_stock(unsigned list, std::span<const Item> templates)
{
    if (list < 1 || list > shop_list_count)
    {
        throw std::out_of_range("A shop list is numbered 1 to 16");
    }

    const std::size_t first = list_starts[list - 1] - 1u;
    std::vector<Item> stock;
    for (std::size_t entry = first; entry < first + list_counts[list - 1]; ++entry)
    {
        const std::size_t number = list_entries[entry];
        if (number > templates.size() || templates[number - 1].type == Item::no_type)
        {
            continue;
        }
        // EclShop puts each item at the front of the shop's list.
        stock.insert(stock.begin(), templates[number - 1]);
    }
    return stock;
}

std::uint32_t shop_price(const Item &item, std::uint8_t multiplier) noexcept
{
    const std::uint32_t value = std::max<std::uint32_t>(item.value, 1);
    if (multiplier == 0)
    {
        return value;
    }
    return value * multiplier / 16;
}

PurchaseResult buy_item(TeamMember &buyer, const Item &item, std::uint8_t multiplier,
                        CreditPool &pool)
{
    const auto price = shop_price(item, multiplier);
    if (!can_pay(buyer.character, pool, price))
    {
        return PurchaseResult::not_enough_money;
    }

    auto gear = buyer.items;
    gear.push_back(item);
    if (joinable(item))
    {
        join_item(gear, gear.size() - 1);
    }
    if (gear.size() > max_carried_items)
    {
        return PurchaseResult::overloaded;
    }

    buyer.items = std::move(gear);
    pay(buyer.character, pool, price);
    return PurchaseResult::bought;
}

std::uint32_t sale_price(const Item &item) noexcept
{
    std::uint32_t price = item.value / 2u;
    if (item.quantity > 1 && joinable(item))
    {
        price *= item.quantity;
    }
    return price;
}

void sell_item(TeamMember &seller, std::size_t item)
{
    const auto price = sale_price(seller.items.at(item));
    seller.items.erase(seller.items.begin() + static_cast<std::ptrdiff_t>(item));
    seller.character.credits += price;
}

bool takes_ammunition(const Item &weapon, std::span<const ItemType> types)
{
    const auto &type = type_of(weapon, types);
    return type.slot == weapon_slot && type.weapon_class != melee_class &&
           type.weapon_class != grenade_class && !joinable(weapon);
}

AmmunitionOffer ammunition_offer(const Item &weapon, std::span<const ItemType> types, bool fill)
{
    if (!takes_ammunition(weapon, types))
    {
        throw std::invalid_argument("AMMO does not list the item");
    }

    const auto &type = type_of(weapon, types);
    const int capacity = capacity_of(type);
    const int asked = fill ? capacity : (sold_by_the_round(type) ? 1 : clip_rounds);
    const int rounds = std::max(0, std::min(asked, capacity - weapon.quantity));
    const int clips = sold_by_the_round(type) ? rounds : (rounds + clip_rounds - 1) / clip_rounds;
    return {rounds, clip_price(weapon.type) * static_cast<std::uint32_t>(clips)};
}

AmmunitionResult buy_ammunition(TeamMember &buyer, std::size_t weapon,
                                std::span<const ItemType> types, bool fill, CreditPool &pool)
{
    auto &item = buyer.items.at(weapon);
    if (item.quantity >= capacity_of(type_of(item, types)))
    {
        return AmmunitionResult::already_full;
    }

    const auto offer = ammunition_offer(item, types, fill);
    if (!can_pay(buyer.character, pool, offer.cost))
    {
        return AmmunitionResult::not_enough_money;
    }

    pay(buyer.character, pool, offer.cost);
    item.quantity = static_cast<std::uint8_t>(item.quantity + offer.rounds);
    return AmmunitionResult::bought;
}

bool halve_item(std::vector<Item> &items, std::size_t item)
{
    auto &bundle = items.at(item);
    const int half = bundle.quantity / 2;
    if (!joinable(bundle) || half == 0 || items.size() >= max_carried_items)
    {
        return false;
    }

    auto smaller = bundle;
    smaller.quantity = static_cast<std::uint8_t>(half);
    smaller.flags = static_cast<std::uint8_t>(smaller.flags & ~Item::readied);
    bundle.quantity = static_cast<std::uint8_t>(bundle.quantity - half);
    items.insert(items.begin() + static_cast<std::ptrdiff_t>(item) + 1, smaller);
    return true;
}

void join_item(std::vector<Item> &items, std::size_t item)
{
    if (!joinable(items.at(item)))
    {
        return;
    }

    // Marked items leave the list once the walk is done, so the indices hold.
    std::vector<bool> joined(items.size(), false);
    std::size_t destination = item;
    int quantity = std::max<int>(items[destination].quantity, 1);
    for (std::size_t current = 0; current < items.size(); ++current)
    {
        auto &other = items[current];
        if (current == destination || joined[current] || !alike(other, items[destination]))
        {
            continue;
        }

        const int other_quantity = std::max<int>(other.quantity, 1);
        if (quantity + other_quantity <= max_bundle)
        {
            quantity += other_quantity;
            joined[current] = true;
            continue;
        }

        items[destination].quantity = static_cast<std::uint8_t>(max_bundle);
        quantity = other_quantity - (max_bundle - quantity);
        destination = current;
    }
    items[destination].quantity = static_cast<std::uint8_t>(quantity);

    std::size_t kept = 0;
    for (std::size_t index = 0; index < items.size(); ++index)
    {
        if (!joined[index])
        {
            items[kept++] = items[index];
        }
    }
    items.resize(kept);
}

std::uint8_t script_operand_tag(GoldBoxGame game, std::string_view archive, unsigned script,
                                std::uint16_t address, std::size_t operand,
                                std::uint8_t tag) noexcept
{
    for (const auto &fix : script_tag_fixes)
    {
        if (fix.game == game && fix.archive == archive && fix.script == script &&
            fix.address == address && fix.operand == operand && fix.shipped == tag)
        {
            return fix.meant;
        }
    }
    return tag;
}
} // namespace opengold::original
