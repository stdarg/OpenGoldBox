#include "opengold/campaign_party.h"
#include "opengold/authored_items.h"
#include "opengold/dice.h"
#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>
#include <tuple>

namespace opengold
{
namespace
{
constexpr std::array<std::uint16_t, 7> money{0x6BBB, 0x6BBD, 0x6BBF, 0x6BC1,
    0x6BC3, 0x6BC5, 0x6BC7};

// The original class groups whose THAC0 tables PARTY STRENGTH reads.
enum class AdndGroup
{
    fighter,
    cleric,
    magic_user,
    thief
};

AdndGroup adnd_group(std::string_view character_class)
{
    if (character_class == "Cleric" || character_class == "Druid" || character_class == "Monk")
        return AdndGroup::cleric;
    if (character_class == "Wizard" || character_class == "Sorcerer" ||
            character_class == "Warlock")
        return AdndGroup::magic_user;
    if (character_class == "Rogue" || character_class == "Bard")
        return AdndGroup::thief;
    return AdndGroup::fighter;
}

// Gold Box THAC0 by level 1..12 (coab engine/ovr018.cs thac0_table, stored
// there as 60 - THAC0). Higher levels keep the level-12 value: the original
// table ends there and nothing is extrapolated.
int adnd_thac0(AdndGroup group, unsigned level)
{
    static constexpr std::array<std::array<int, 12>, 4> table{{
            {20, 20, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9},
            {20, 20, 20, 18, 18, 18, 16, 16, 16, 14, 14, 14},
            {21, 21, 21, 21, 21, 19, 19, 19, 19, 19, 17, 17},
            {20, 20, 20, 20, 19, 19, 19, 19, 16, 16, 16, 16}
        }
    };
    const auto row = static_cast<std::size_t>(group);
    return table[row][std::clamp(level, 1u, 12u) - 1];
}

// AD&D descending AC of worn armor (nearest original suit for SRD-only suits),
// or nullopt for items that are not armor. A shield improves AC by one.
std::optional<int> adnd_armor(std::string_view definition)
{
    static const std::map<std::string_view, int> suits{
        {"padded", 8},      {"leather", 8},     {"studded_leather", 7},
        {"hide", 6},        {"chain_shirt", 6}, {"scale_mail", 6},
        {"breastplate", 5}, {"half_plate", 4},  {"ring_mail", 7},
        {"chain_mail", 5},  {"splint", 4},      {"plate", 3}};
    const auto found = suits.find(definition);
    if (found == suits.end())
        return std::nullopt;
    return found->second;
}

// PARTY STRENGTH reads an equivalent AD&D character: THAC0 by class and level,
// AC from armor and shield worn, with original magic bonuses on equipped items.
struct AdndCombatValues
{
    int thac0{20}, armor_class{10};
};

AdndCombatValues adnd_combat_values(const PartyMember &m)
{
    const auto &sheet = m.character.sheet();
    AdndCombatValues values;
    values.thac0 = adnd_thac0(adnd_group(sheet.character_class), sheet.level);
    int weapon_bonus = 0;
    for (const auto equipped : m.equipped)
    {
        const auto item = m.character.inventory().find(equipped);
        if (!item)
            throw std::runtime_error("Equipped item is missing");
        const auto source = m.item_sources.find(equipped);
        const int magic = source == m.item_sources.end() ? 0 : source->second.stored.magic_bonus;
        const auto &definition = item->get().definition_id;
        if (definition == "shield")
            values.armor_class -= 1 + magic;
        else if (const auto suit = adnd_armor(definition))
            values.armor_class -= 10 - *suit + magic;
        else
            weapon_bonus = std::max(weapon_bonus, magic);
    }
    values.thac0 -= weapon_bonus;
    return values;
}
}

namespace
{
// Removes the thrown gear (Acid, Alchemist's Fire, Oil) a fight used up, so the
// member carries `left` of `definition`, taking from the newest stacks first.
void keep_thrown_gear(PartyMember &member, const std::string &definition, unsigned left)
{
    std::vector<std::pair<std::uint64_t, std::uint32_t>> stacks;
    unsigned carried = 0;
    for (const auto &item : member.character.inventory().items())
        if (item.definition_id == definition)
        {
            stacks.emplace_back(item.id, item.quantity);
            carried += item.quantity;
        }
    if (left > carried)
        throw std::runtime_error("Combat reported more thrown gear than was carried");
    auto inventory = member.character.inventory();
    for (auto stack = stacks.rbegin(); stack != stacks.rend() && carried > left; ++stack)
    {
        const auto used = std::min(stack->second, carried - left);
        inventory.remove(stack->first, used);
        carried -= used;
        if (used == stack->second)
            member.item_sources.erase(stack->first);
    }
    member.character.inventory() = std::move(inventory);
}
} // namespace

std::string equipment_conversion(const por::Equipment &item)
{
    const auto &raw = item.stored;
    if (raw.magic_bonus || raw.cursed_raw ||
            std::any_of(raw.effect_codes.begin(), raw.effect_codes.end(),
                        [](auto n)
{
    return n != 0;
}))
    return "por:unsupported:" + std::to_string(raw.type);
    switch (raw.type)
    {
    case 1:
        return "battleaxe";
    case 2:
        return "handaxe";
    case 3:
    case 5:
    case 10:
    case 11:
    case 14:
    case 15:
    case 16:
    case 17:
    case 40:
        return "glaive";
    case 4:
    case 18:
    case 19:
        return "halberd";
    case 6:
    case 22:
    case 33:
        return "quarterstaff";
    case 7:
        return "club";
    case 8:
        return "dagger";
    case 9:
        return "dart";
    case 12:
        return "flail";
    case 13:
    case 25:
    case 27:
    case 29:
    case 32:
        return "pike";
    case 20:
        return "warhammer";
    case 21:
        return "javelin";
    case 23:
        return "mace";
    case 24:
        return "morningstar";
    case 26:
        return "war_pick";
    case 30:
        return "scimitar";
    case 31:
        return "spear";
    case 34:
    case 35:
    case 36:
        return "longsword";
    case 37:
        return "shortsword";
    case 38:
        return "greatsword";
    case 39:
        return "trident";
    case 41:
    case 43:
    case 45:
        return "longbow";
    case 42:
    case 44:
        return "shortbow";
    case 46:
        return "light_crossbow";
    case 47:
        return "sling";
    case 79:
        return "wand";
    case 28:
        return "bolt";
    case 73:
        return "arrow";
    case 50:
        return "leather";
    case 51:
        return "padded";
    case 52:
        return "studded_leather";
    case 53:
        return "ring_mail";
    case 54:
        return "scale_mail";
    case 55:
        return "chain_mail";
    // SRD 5.2.1 has no Banded Mail; Splint is its heavy armor with the same AD&D AC 4.
    case 56:
    case 57:
        return "splint";
    case 58:
        return "plate";
    case 59:
        return "shield";
    case original_item::flask_of_oil:
        return "oil";
    case authored_item::torch:
        return "torch";
    case authored_item::acid:
        return "acid";
    case authored_item::alchemists_fire:
        return "alchemists_fire";
    default:
        return "por:unsupported:" + std::to_string(raw.type);
    }
}

CampaignParty::CampaignParty(std::unique_ptr<rules::RulesModule> rules) : rules_(std::move(rules))
{
    if (!rules_)
        throw std::runtime_error("Party requires a rules module");
}

void CampaignParty::outside_combat() const
{
    if (combat_)
        throw std::runtime_error("Finish combat before changing the party");
}

void CampaignParty::editable() const
{
    outside_combat();
    if (state_.short_rest)
        throw std::runtime_error("Finish Short Rest spending before changing the party");
    if (state_.spell_rest)
        throw std::runtime_error("Finish Long Rest spell choices before changing the party");
    if (state_.training_rest)
        throw std::runtime_error("Finish Long Rest training choices before changing the party");
}

const PartyMember &CampaignParty::member(MemberId id) const
{
    const auto it = std::find_if(state_.roster.begin(), state_.roster.end(),
                                 [&](const auto & m)
    {
        return m.id == id;
    });
    if (it == state_.roster.end())
        throw std::runtime_error("Unknown party member");
    return *it;
}

PartyMember &CampaignParty::edit(MemberId id)
{
    return const_cast<PartyMember &>(std::as_const(*this).member(id));
}

void CampaignParty::select(unsigned slot)
{
    outside_combat();
    if (slot >= 8 || !state_.slots[slot])
        throw std::runtime_error("Empty party position");
    state_.selected = slot;
}

MemberId CampaignParty::leader() const
{
    if (state_.leader &&
            std::find(state_.slots.begin(), state_.slots.end(), state_.leader) != state_.slots.end())
        return state_.leader;
    for (const auto id : state_.slots)
        if (id)
            return id;
    return 0;
}

void CampaignParty::make_leader(MemberId id)
{
    if (std::find(state_.slots.begin(), state_.slots.end(), id) == state_.slots.end() || !id)
        throw std::runtime_error("Member is not in party");
    state_.leader = id;
}

MemberId CampaignParty::spokesman() const
{
    const auto conscious = [&](MemberId id)
    {
        const auto &vitals = member(id).vitals;
        return id && !vitals.dead && vitals.hit_points > 0;
    };
    if (const auto first = leader(); first && conscious(first))
        return first;
    for (const auto id : state_.slots)
        if (id && conscious(id))
            return id;
    return 0;
}

void CampaignParty::join(MemberId id, bool npc)
{
    if (std::find(state_.slots.begin(), state_.slots.end(), id) != state_.slots.end())
        throw std::runtime_error("Already in party");
    const auto start = npc ? 6 : 0, end = npc ? 8 : 6;
    for (int i = start; i < end; ++i)
        if (!state_.slots[i])
        {
            state_.slots[i] = id;
            if (!selected())
                state_.selected = i;
            return;
        }
    throw std::runtime_error(npc ? "Both NPC positions are occupied"
                             : "All six PC positions are occupied");
}

MemberId CampaignParty::add_pc(Character character)
{
    editable();
    if (state_.roster.size() >= 128)
        throw std::runtime_error("Roster is full");
    if (std::none_of(state_.slots.begin(), state_.slots.begin() + 6,
                     [](auto id)
{
    return !id;
}))
    throw std::runtime_error("All six PC positions are occupied");
    const auto id = state_.next_id;
    const auto hp = character.sheet().hit_points;
    state_.roster.push_back({id, std::move(character), {}, {hp, false, {}}});
    ++state_.next_id;
    join(id, false);
    return id;
}

MemberId CampaignParty::recruit(std::string source, Character converted, unsigned morale)
{
    editable();
    if (source.empty() || morale > 255)
        throw std::runtime_error("NPC conversion requires a source identity and byte morale");
    const auto found = std::find_if(state_.roster.begin(), state_.roster.end(),
                                    [&](const auto & m)
    {
        return m.npc_source == source;
    });
    if (found != state_.roster.end())
    {
        const auto id = found->id;
        rejoin(id);
        return id;
    }
    if (state_.roster.size() >= 128)
        throw std::runtime_error("Roster is full");
    if (state_.slots[6] && state_.slots[7])
        throw std::runtime_error("Both NPC positions are occupied");
    (void)rules_->character_profile(converted.sheet(),
                                    {}); // Reject unknown conversion before recruitment.
    const auto id = state_.next_id;
    const auto hp = converted.sheet().hit_points;
    state_.roster.push_back(
    {id, std::move(converted), std::move(source), {hp, false, {}}, {}, {}, morale});
    ++state_.next_id;
    join(id, true);
    return id;
}

void CampaignParty::rejoin(MemberId id)
{
    editable();
    join(id, !member(id).npc_source.empty());
}

void CampaignParty::remove(MemberId id)
{
    editable();
    (void)member(id);
    auto it = std::find(state_.slots.begin(), state_.slots.end(), id);
    if (it == state_.slots.end())
        throw std::runtime_error("Member is not in party");
    *it = 0;
    if (state_.leader == id)
        state_.leader = 0;
    if (!selected())
        for (unsigned i = 0; i < 8; ++i)
            if (state_.slots[i])
            {
                state_.selected = i;
                break;
            }
}

rules::RecoveryInfo CampaignParty::recovery_info(MemberId id) const
{
    const auto &who = member(id);
    return rules_->recovery_info(who.character.sheet(), who.vitals);
}

rules::CharacterProfile CampaignParty::profile(MemberId id) const
{
    const auto &m = member(id);
    std::vector<std::string> keys;
    for (auto equipped : m.equipped)
    {
        auto item = m.character.inventory().find(equipped);
        if (!item)
            throw std::runtime_error("Equipped item is missing");
        keys.push_back(item->get().definition_id);
    }
    return rules_->character_profile(m.character.sheet(), keys);
}

rules::AbilityCheckModifier CampaignParty::ability_check(MemberId id, unsigned ability,
        std::string_view skill) const
{
    const auto &m = member(id);
    std::vector<std::string> keys;
    for (auto equipped : m.equipped)
    {
        const auto item = m.character.inventory().find(equipped);
        if (!item)
            throw std::runtime_error("Equipped item is missing");
        keys.push_back(item->get().definition_id);
    }
    return rules_->ability_check(m.character.sheet(), keys, ability, skill);
}

std::vector<rules::EquipmentChoice> CampaignParty::equipment_choices(MemberId id,
        std::uint64_t item) const
{
    const auto &m = member(id);
    const auto selected = m.character.inventory().find(item);
    if (!selected)
        throw std::runtime_error("Unknown item");
    if (selected->get().quantity == 1 &&
            std::find(m.equipped.begin(), m.equipped.end(), item) != m.equipped.end())
        return {};
    std::vector<std::string> keys;
    for (auto key : m.equipped)
        keys.push_back(m.character.inventory().find(key)->get().definition_id);
    keys.push_back(selected->get().definition_id);
    return rules_->equipment_choices(m.character.sheet(), keys, keys.size() - 1);
}

void CampaignParty::equip(MemberId id, std::uint64_t item, rules::EquipmentOperation operation)
{
    if (operation == rules::EquipmentOperation::unequip)
        throw std::runtime_error("Invalid equip operation");
    change_equipment(id, item, operation);
}

void CampaignParty::unequip(MemberId id, std::uint64_t item)
{
    change_equipment(id, item, rules::EquipmentOperation::unequip);
}

void CampaignParty::change_equipment(MemberId id, std::uint64_t item,
                                     rules::EquipmentOperation operation)
{
    editable();
    const auto &before = member(id);
    auto candidates = before.equipped;
    const auto found = std::find(candidates.begin(), candidates.end(), item);
    if (operation == rules::EquipmentOperation::equip && found != candidates.end())
        return;
    if (operation == rules::EquipmentOperation::unequip && found == candidates.end())
        return;
    const unsigned selected = operation != rules::EquipmentOperation::unequip
                              ? candidates.size()
                              : found - candidates.begin();
    if (operation != rules::EquipmentOperation::unequip)
        candidates.push_back(item);
    std::vector<std::string> keys;
    for (auto id : candidates)
    {
        const auto entry = before.character.inventory().find(id);
        if (!entry)
            throw std::runtime_error("Unknown item");
        keys.push_back(entry->get().definition_id);
    }
    const auto plan = rules_->equipment_change(before.character.sheet(), keys, selected, operation);
    auto inventory = before.character.inventory();
    auto sources = before.item_sources;
    bool inventory_changed = false;
    if (plan.separate_selected_unit)
    {
        const auto unit = inventory.find(item)->get();
        if (unit.quantity > 1)
        {
            inventory_changed = true;
            inventory.remove(item, 1);
            candidates[selected] =
                inventory.add(unit.definition_id, unit.name, 1, unit.original_type);
            if (const auto source = sources.find(item); source != sources.end())
                sources.emplace(candidates[selected], source->second);
        }
    }
    std::vector<std::uint64_t> next;
    next.reserve(plan.indices.size());
    for (auto index : plan.indices)
    {
        if (index >= candidates.size() ||
                std::find(next.begin(), next.end(), candidates[index]) != next.end())
            throw std::runtime_error("Invalid equipment result from rules module");
        next.push_back(candidates[index]);
    }
    auto &target = edit(id);
    if (inventory_changed)
    {
        target.character.inventory() = std::move(inventory);
        target.item_sources = std::move(sources);
    }
    target.equipped = std::move(next);
}

rules::EquipmentInfo CampaignParty::equipment_info(MemberId id, std::uint64_t item) const
{
    const auto found = member(id).character.inventory().find(item);
    if (!found)
        throw std::runtime_error("Unknown item");
    return rules_->equipment_info(found->get().definition_id);
}

void CampaignParty::purchase(MemberId id, const por::Equipment &item)
{
    editable();
    auto &m = edit(id);
    if (m.character.inventory().items().size() >= 16)
        throw std::runtime_error("Inventory is full (16 items)");
    if (m.wealth[3] < item.stored.value)
        throw std::runtime_error("Not enough gold");
    auto inventory = m.character.inventory();
    auto sources = m.item_sources;
    const auto key =
        inventory.add(equipment_conversion(item), item.label(),
                      std::max(1u, unsigned(item.stored.stack_size)), item.stored.type);
    sources.emplace(key, item);
    m.character.inventory() = std::move(inventory);
    m.item_sources = std::move(sources);
    m.wealth[3] -= item.stored.value;
}

void CampaignParty::set_wealth(MemberId id, std::array<std::uint16_t, 7> wealth)
{
    editable();
    edit(id).wealth = wealth;
}

bool CampaignParty::award_loot(const std::array<unsigned, 7> &wealth,
                               const std::vector<por::Equipment> &items, std::string reward_id)
{
    editable();
    if (reward_id.empty() || reward_id.size() > 160)
        throw std::runtime_error("Loot requires a bounded stable identity");
    if (std::find(state_.claimed_rewards.begin(), state_.claimed_rewards.end(), reward_id) !=
            state_.claimed_rewards.end())
        return true;
    if (state_.claimed_rewards.size() >= 1024 || items.size() > 256)
        throw std::runtime_error("Loot collection exceeds supported limits");
    auto next = state_;
    std::vector<std::size_t> recipients;
    for (auto id : next.slots)
        if (id)
        {
            const auto found = std::find_if(next.roster.begin(), next.roster.end(),
                                            [&](const auto & m)
            {
                return m.id == id;
            });
            if (!found->vitals.dead)
                recipients.push_back(found - next.roster.begin());
        }
    if (recipients.empty())
        return false;
    for (unsigned coin = 0; coin < 7; ++coin)
    {
        auto remaining = wealth[coin];
        for (auto index : recipients)
        {
            auto &purse = next.roster[index].wealth[coin];
            const auto amount = std::min(remaining, unsigned(65535 - purse));
            purse += amount;
            remaining -= amount;
        }
        if (remaining)
            return false;
    }
    for (const auto &item : items)
    {
        const auto recipient =
            *std::min_element(recipients.begin(), recipients.end(),
                              [&](auto a, auto b)
        {
            return next.roster[a].character.inventory().items().size() <
                   next.roster[b].character.inventory().items().size();
        });
        auto &m = next.roster[recipient];
        // Encounter rewards are retained even beyond the shop's purchase cap.
        const auto id = m.character.inventory().add(equipment_conversion(item), item.label(),
            std::max(1u, unsigned(item.stored.stack_size)),
            item.stored.type);
        m.item_sources.emplace(id, item);
    }
    next.claimed_rewards.push_back(std::move(reward_id));
    state_ = std::move(next);
    return true;
}

void CampaignParty::award_experience(unsigned amount, std::string reward_id)
{
    editable();
    if (reward_id.empty() || reward_id.size() > 160)
        throw std::runtime_error("Reward requires a bounded stable identity");
    if (std::find(state_.claimed_rewards.begin(), state_.claimed_rewards.end(), reward_id) !=
            state_.claimed_rewards.end())
        return;
    if (state_.claimed_rewards.size() >= 1024)
        throw std::runtime_error("Reward history is full");
    auto next = state_;
    bool awarded = false;
    for (auto id : next.slots)
        if (id)
        {
            auto &member = *std::find_if(next.roster.begin(), next.roster.end(),
                                         [&](const auto & m)
            {
                return m.id == id;
            });
            if (member.vitals.dead)
                continue;
            if (amount > std::numeric_limits<unsigned>::max() - member.experience)
                throw std::runtime_error("Experience overflow");
            awarded = true;
            member.experience += amount;
        }
    if (!awarded)
        throw std::runtime_error("Reward requires a living active member");
    next.claimed_rewards.push_back(std::move(reward_id));
    state_ = std::move(next);
}

bool CampaignParty::can_advance(MemberId id) const
{
    if (combat_ || state_.short_rest || state_.spell_rest || state_.training_rest)
        return false;
    const auto &m = member(id);
    const auto options = rules_->advancement_options(m.character.sheet());
    return !m.vitals.dead && options.level &&
           m.experience >= rules_->experience_for_level(options.level);
}

rules::AdvancementOptions
CampaignParty::advancement_options(MemberId id, const rules::AdvancementChoice &choice) const
{
    return rules_->advancement_options(member(id).character.sheet(), choice);
}

rules::AdvancementChoice CampaignParty::default_advancement(MemberId id) const
{
    return rules_->default_advancement(member(id).character.sheet());
}

PartyMember CampaignParty::preview_advancement(MemberId id,
        const rules::AdvancementChoice &choice) const
{
    if (!can_advance(id))
        throw std::runtime_error("This character is not ready to level up");
    auto next = member(id);
    const auto level = next.character.sheet().level;
    if (rules_->default_advancement(next.character.sheet()).spell_learning &&
            !choice.spell_learning)
        throw std::runtime_error("Independent spell learning choices are required");
    for (const auto &group : rules_->advancement_options(next.character.sheet()).training)
    {
        const auto found = choice.training.find(group.id);
        if (found == choice.training.end() || found->second.size() != group.count)
            throw std::runtime_error("Complete required advancement training");
    }
    if (!next.character.advance(*rules_, next.vitals, choice) ||
            next.character.sheet().level != level + 1)
        throw std::runtime_error("Unsupported advancement");
    return next;
}

void CampaignParty::advance(MemberId id, const rules::AdvancementChoice &choice)
{
    editable();
    auto member = preview_advancement(id, choice);
    auto next = state_;
    *std::find_if(next.roster.begin(), next.roster.end(),
                  [&](const auto & m)
    {
        return m.id == id;
    }) = std::move(member);
    state_ = std::move(next);
}

rules::SpellChoiceOptions CampaignParty::spell_choice_options(MemberId id) const
{
    return rules_->spell_choice_options(member(id).character.sheet(),
                                        rules::SpellChoiceContext::long_rest);
}

PartyMember CampaignParty::preview_spell_choices(MemberId id,
        const rules::SpellChoices &choices) const
{
    outside_combat();
    if (state_.short_rest)
        throw std::runtime_error("Finish resting before spell choices");
    if (!state_.spell_rest || state_.spell_rest->completed_minutes != state_.time_minutes ||
            state_.spell_rest->completed_subminute_milliseconds != state_.subminute_milliseconds ||
            std::find(state_.spell_rest->members.begin(), state_.spell_rest->members.end(), id) ==
            state_.spell_rest->members.end())
        throw std::runtime_error("No completed Long Rest spell choices");
    auto candidate = member(id);
    candidate.character.choose_spells(*rules_, choices, state_.spell_rest->ticket.session);
    rules_->validate_character_state(candidate.character.sheet(), candidate.vitals);
    return candidate;
}

void CampaignParty::choose_spells(MemberId id, const rules::SpellChoices &choices)
{
    auto candidate = preview_spell_choices(id, choices);
    auto next = state_;
    *std::find_if(next.roster.begin(), next.roster.end(),
                  [&](const auto & m)
    {
        return m.id == id;
    }) = std::move(candidate);
    std::erase(next.spell_rest->members, id);
    if (next.spell_rest->members.empty())
        next.spell_rest.reset();
    state_ = std::move(next);
}

void CampaignParty::keep_rest_spells(MemberId id)
{
    outside_combat();
    if (!state_.spell_rest ||
            std::find(state_.spell_rest->members.begin(), state_.spell_rest->members.end(), id) ==
            state_.spell_rest->members.end())
        throw std::runtime_error("No Long Rest spell choice to decline");
    auto next = state_;
    std::erase(next.spell_rest->members, id);
    if (next.spell_rest->members.empty())
        next.spell_rest.reset();
    state_ = std::move(next);
}

PartyMember CampaignParty::preview_rest_training(RestTicket ticket, MemberId id,
        std::span<const std::string> selections) const
{
    outside_combat();
    const auto &rest = state_.training_rest;
    if (state_.spell_rest || state_.short_rest || !rest ||
            rest->ticket != ticket || rest->completed_minutes != state_.time_minutes ||
            rest->completed_subminute_milliseconds != state_.subminute_milliseconds ||
            std::find(rest->members.begin(), rest->members.end(), id) == rest->members.end())
        throw std::runtime_error("No completed Long Rest training choice");
    auto candidate = member(id);
    candidate.character.replace_rest_training(*rules_, selections, ticket.session);
    rules_->validate_character_state(candidate.character.sheet(), candidate.vitals);
    return candidate;
}

void CampaignParty::replace_rest_training(RestTicket ticket, MemberId id,
        std::span<const std::string> selections)
{
    auto candidate = preview_rest_training(ticket, id, selections);
    auto next = state_;
    *std::find_if(next.roster.begin(), next.roster.end(),
                  [&](const auto & m)
    {
        return m.id == id;
    }) = std::move(candidate);
    std::erase(next.training_rest->members, id);
    if (next.training_rest->members.empty())
        next.training_rest.reset();
    state_ = std::move(next);
}

void CampaignParty::keep_rest_training(RestTicket ticket, MemberId id)
{
    // Keeping the current legal set consumes exactly the same entitlement.
    const auto options = rules_->rest_training_options(member(id).character.sheet());
    if (!options)
        throw std::runtime_error("No Long Rest training choice to decline");
    replace_rest_training(ticket, id, options->selected);
}

void CampaignParty::advance_time(unsigned minutes)
{
    advance_time_milliseconds(std::uint64_t(minutes) * 60000);
}

void CampaignParty::advance_time_milliseconds(std::uint64_t milliseconds)
{
    outside_combat();
    if ((state_.spell_rest || state_.training_rest) && milliseconds)
        throw std::runtime_error("Finish Long Rest spell choices before advancing time");
    auto next = state_;
    elapse(next, milliseconds);
    if (milliseconds)
        next.short_rest.reset();
    state_ = std::move(next);
}

void CampaignParty::elapse(PartyState &state, std::uint64_t milliseconds,
                           std::span<const MemberId> in_combat) const
{
    const auto remainder = state.subminute_milliseconds + milliseconds % 60000;
    const auto minutes = milliseconds / 60000 + remainder / 60000;
    if (minutes > std::numeric_limits<std::uint64_t>::max() - state.time_minutes)
        throw std::runtime_error("Campaign clock overflow");
    std::vector<rules::Participant> participants;
    for (const auto &member : state.roster)
    {
        if (std::find(in_combat.begin(), in_combat.end(), member.id) != in_combat.end())
            continue;
        std::vector<std::string> gear;
        for (auto id : member.equipped)
            gear.push_back(member.character.inventory().find(id)->get().definition_id);
        const auto profile =
            rules_->character_profile(member.character.sheet(), gear);
        participants.push_back({member.id,
                                "campaign-character",
                                member.character.sheet().name,
                                0,
                                {},
                                profile.data,
                                member.vitals});
    }
    rules_->elapse(participants, milliseconds, state.random_state);
    for (const auto &participant : participants)
        std::find_if(state.roster.begin(), state.roster.end(),
                     [&](const auto & m)
    {
        return m.id == participant.id;
    })
    ->vitals = *participant.state;
    state.time_minutes += minutes;
    state.subminute_milliseconds = static_cast<unsigned>(remainder % 60000);
}

namespace
{
bool tries_door(const PartyMember &m, DoorMethod method)
{
    const bool conscious = !m.vitals.dead && m.vitals.hit_points > 0;
    return conscious &&
           (method == DoorMethod::bash || m.character.sheet().character_class == "Rogue");
}
} // namespace

bool CampaignParty::can_try_door(DoorMethod method) const
{
    return std::any_of(state_.slots.begin(), state_.slots.end(), [&](MemberId id)
    {
        if (!id)
            return false;
        const auto &m = member(id);
        return method == DoorMethod::knock
               ? rules_->can_cast_exploration_spell(m.character.sheet(), m.vitals, "knock")
               : tries_door(m, method);
    });
}

std::vector<DoorAttempt> CampaignParty::try_door(DoorMethod method, int difficulty)
{
    editable();
    if (method == DoorMethod::knock)
        return cast_knock(difficulty);
    std::vector<DoorAttempt> attempts;
    auto random_state = state_.random_state;
    for (auto id : state_.slots)
    {
        if (!id)
            continue;
        const auto &m = member(id);
        if (!tries_door(m, method))
            continue;
        std::vector<std::string> gear;
        for (auto equipped : m.equipped)
        {
            const auto item = m.character.inventory().find(equipped);
            if (!item)
                throw std::runtime_error("Equipped item is missing");
            gear.push_back(item->get().definition_id);
        }

        const auto roll =
            method == DoorMethod::bash
            ? rules_->roll_ability_check(m.character.sheet(), gear, 0, "athletics", random_state)
            : rules_->roll_ability_check(m.character.sheet(), gear, 1, "sleight_of_hand",
                                         random_state);
        attempts.push_back({id, roll});
        if (roll.total >= difficulty)
            break;
    }
    state_.random_state = random_state;
    return attempts;
}

std::vector<DoorAttempt> CampaignParty::cast_knock(int difficulty)
{
    // The first active member able to cast it does; the lock always opens.
    auto next = state_;
    for (const auto id : next.slots)
    {
        if (!id)
            continue;
        auto &caster = *std::find_if(next.roster.begin(), next.roster.end(), [&](const auto & m)
        {
            return m.id == id;
        });
        if (!rules_->can_cast_exploration_spell(caster.character.sheet(), caster.vitals, "knock"))
            continue;
        rules_->cast_exploration_spell(caster.character.sheet(), caster.vitals, "knock");
        state_ = std::move(next);
        return {{id, {0, difficulty}}};
    }
    throw std::runtime_error("No member can cast Knock");
}

void CampaignParty::temple_heal(MemberId target)
{
    editable();
    const auto &current = member(target);
    if (std::find(state_.slots.begin(), state_.slots.end(), target) == state_.slots.end())
        throw std::runtime_error("Temple target must be active");
    if (current.vitals.dead || current.vitals.hit_points >= hit_point_maximum(target))
        throw std::runtime_error("Cure Wounds requires a wounded living member");
    auto next = state_;
    auto &healed = *std::find_if(next.roster.begin(), next.roster.end(),
                                 [&](const auto & m)
    {
        return m.id == target;
    });
    unsigned remaining = 100;
    for (auto id : next.slots)
        if (id)
        {
            auto &m = *std::find_if(next.roster.begin(), next.roster.end(),
                                    [&](const auto & x)
            {
                return x.id == id;
            });
            const auto paid = std::min<unsigned>(m.wealth[3], remaining);
            m.wealth[3] -= static_cast<std::uint16_t>(paid);
            remaining -= paid;
        }
    if (remaining)
        throw std::runtime_error("Party cannot afford temple service");
    rules_->temple_heal(healed.vitals, healed.character.sheet(), next.random_state);
    state_ = std::move(next);
}

std::optional<HazardHit> CampaignParty::hazard_attack(const rules::HazardAttack &attack)
{
    editable();
    std::vector<MemberId> conscious;
    for (const auto id : state_.slots)
        if (id && member(id).vitals.hit_points > 0)
            conscious.push_back(id);
    if (conscious.empty())
        return std::nullopt;
    auto next = state_;
    const auto target = conscious[roll_die(next.random_state, int(conscious.size())) - 1];
    auto &struck = *std::find_if(next.roster.begin(), next.roster.end(),
                                 [&](const auto & m)
    {
        return m.id == target;
    });
    HazardHit hit{target, rules_->hazard_attack(struck.vitals, struck.character.sheet(), attack,
                  next.random_state)};
    state_ = std::move(next);
    return hit;
}

int CampaignParty::hit_point_maximum(MemberId id) const
{
    const auto &m = member(id);
    return rules_->hit_point_maximum(m.character.sheet(), m.vitals);
}

std::vector<rules::CampAction> CampaignParty::camp_actions(MemberId id) const
{
    const auto &m = member(id);
    if (std::find(state_.slots.begin(), state_.slots.end(), id) == state_.slots.end())
        return {};
    return rules_->camp_actions(m.character.sheet(), m.vitals);
}

void CampaignParty::use_camp_action(MemberId user, MemberId target, std::string_view action)
{
    editable();
    for (const auto id : {user, target})
        if (std::find(state_.slots.begin(), state_.slots.end(), id) == state_.slots.end())
            throw std::runtime_error("Camp actions are for active members");
    auto next = state_;
    const auto find = [&](MemberId id) -> PartyMember &
    {
        return *std::find_if(next.roster.begin(), next.roster.end(), [&](const auto & m)
        {
            return m.id == id;
        });
    };
    auto &caster = find(user);
    const auto actions = camp_actions(user);
    const auto chosen = std::find_if(actions.begin(), actions.end(), [&](const auto & a)
    {
        return a.id == action;
    });
    // A whole-party action such as Prayer of Healing picks its own members
    // among the active ones; `target` is not used.
    if (chosen != actions.end() && chosen->whole_party)
    {
        std::vector<rules::CampTarget> party;
        for (const auto id : next.slots)
            if (id)
            {
                auto &m = find(id);
                party.push_back({&m.character.sheet(), &m.vitals});
            }
        rules_->use_party_camp_action(caster.character.sheet(), caster.vitals, party, action,
                                      next.random_state);
    }
    else
    {
        auto &patient = find(target);
        rules_->use_camp_action(caster.character.sheet(), caster.vitals, patient.character.sheet(),
                                patient.vitals, action, next.random_state);
    }
    state_ = std::move(next);
}

bool CampaignParty::has_item(unsigned type) const
{
    for (auto id : state_.slots)
        if (id)
            for (const auto &item : member(id).character.inventory().items())
                if (item.original_type == type)
                    return true;
    return false;
}

unsigned CampaignParty::strength() const
{
    unsigned result = 0;
    for (auto id : state_.slots)
        if (id)
        {
            const auto &m = member(id);
            if (m.vitals.dead)
                continue;
            // The original per-member term (coab CMD_PartyStrength): Cleric level x 4,
            // magic-user level x 8, current HP, 5 per point of AC below 0 and
            // 5 per point of THAC0 below 21.
            const auto adnd = adnd_combat_values(m);
            const auto &sheet = m.character.sheet();
            const auto group = adnd_group(sheet.character_class);
            const int cleric_levels = sheet.character_class == "Cleric" ? int(sheet.level) : 0;
            const int magic_user_levels = group == AdndGroup::magic_user ? int(sheet.level) : 0;
            result += unsigned(cleric_levels * 4 + m.vitals.hit_points +
                               5 * std::max(0, -adnd.armor_class) +
                               5 * std::max(0, 21 - adnd.thac0) + magic_user_levels * 8) /
                      10;
        }
    return result & 255;
}

std::array<unsigned, 4> CampaignParty::query(unsigned address, unsigned effect) const
{
    if (address != 0x6C1B || effect)
        throw std::runtime_error("Unsupported CHECK PARTY attribute/effect conversion");
    unsigned count = 0, low = 255, high = 0, total = 0;
    for (auto id : state_.slots)
        if (id)
        {
            const auto &m = member(id);
            const unsigned move =
                m.vitals.dead || m.vitals.hit_points == 0 ? 0 : profile(id).movement_feet * 2 / 5;
            low = std::min(low, move);
            high = std::max(high, move);
            total += move;
            ++count;
        }
    // The fourth output is not recoverable from the scripts; the Slums' Run after
    // a parley compares it with the monsters' speed, as the encounter menu's
    // Flee compares the slowest member, so it is the slowest member too.
    return count ? std::array<unsigned, 4> {low, high, total / count, low} :
           std::array<unsigned, 4> {};
}

por::EclHostReply CampaignParty::character_reply(unsigned slot) const
{
    if (slot >= 8)
        throw std::runtime_error("Invalid ECL party position");
    std::array<std::uint16_t, 285> fields{};
    por::EclHostReply reply;
    if (const auto id = state_.slots[slot])
    {
        const auto &m = member(id);
        const auto &sheet = m.character.sheet();
        const auto &name = sheet.name;
        for (std::size_t n = 0; n < name.size() && n < 15; ++n)
            fields[n] = static_cast<unsigned char>(name[n]);
        fields[0x18] = sheet.scores[2];
        fields[0x100] = m.vitals.dead ? 0 : 1;
        fields[0x119] = m.vitals.hit_points;
        const auto coins = script_coins(m.wealth);
        for (unsigned n = 0; n < 7; ++n)
            fields[money[n] - 0x6B00] = coins[n];
    }
    for (unsigned n = 0; n < fields.size(); ++n)
        reply.writes.push_back({static_cast<std::uint16_t>(0x6B00 + n), fields[n]});
    reply.writes.push_back({0x6DB1, static_cast<std::uint16_t>(slot)});
    reply.writes.push_back({0x6DB4, static_cast<std::uint16_t>(slot)});
    return reply;
}

std::optional<CoinExchange> CampaignParty::read_character(unsigned slot,
        const por::EclMachine &vm)
{
    outside_combat();
    if (slot >= 8)
        throw std::runtime_error("Invalid ECL party position");
    if (!state_.slots[slot])
        return {};
    const auto &current = member(state_.slots[slot]);
    const auto hp = vm.variable(0x6C19);
    if (hp > hit_point_maximum(state_.slots[slot]) || (current.vitals.dead && hp))
        throw std::runtime_error("Unsupported script HP change");
    Purse after_script;
    for (unsigned n = 0; n < 7; ++n)
        after_script[n] = vm.variable(money[n]);
    // A script read changes only HP and coins, which pending Long Rest spell or
    // training choices do not depend on; the inn's script continues after its rest.
    if (state_.short_rest)
        throw std::runtime_error("Finish Short Rest spending before changing the party");
    auto settled = settle_script_coins(current.wealth, after_script);
    auto &m = edit(state_.slots[slot]);
    auto vitals = m.vitals;
    rules_->set_hit_points(vitals, m.character.sheet(), hp);
    m.wealth = settled.purse;
    m.vitals = std::move(vitals);
    return settled.exchange;
}

void CampaignParty::validate(const PartyState &state)
{
    if (state.roster.size() > 128 || state.selected >= 8 || !state.next_id ||
            state.claimed_rewards.size() > 1024 || state.subminute_milliseconds >= 60000 ||
            !state.next_combat_scope || !state.next_rest_session ||
            (state.leader &&
             std::find(state.slots.begin(), state.slots.end(), state.leader) == state.slots.end()))
        throw std::runtime_error("Invalid party checkpoint");
    std::set<MemberId> ids, active;
    std::set<std::string> sources, creation_sources;
    for (const auto &m : state.roster)
    {
        if (!m.creation_source.empty() &&
                (m.creation_source.size() > 160 || !creation_sources.insert(m.creation_source).second))
            throw std::runtime_error("Invalid creation source checkpoint");
        if (!m.id || m.id >= state.next_id || !ids.insert(m.id).second || m.vitals.hit_points < 0 ||
                (m.last_rest_minutes &&
                 (*m.last_rest_minutes > state.time_minutes ||
                  (*m.last_rest_minutes == state.time_minutes &&
                   m.last_rest_subminute_milliseconds > state.subminute_milliseconds))) ||
                m.last_rest_subminute_milliseconds >= 60000 ||
                (!m.last_rest_minutes && m.last_rest_subminute_milliseconds) ||
                (m.vitals.dead && m.vitals.hit_points) || m.morale > 255 ||
                (!m.npc_source.empty() && !sources.insert(m.npc_source).second))
            throw std::runtime_error("Invalid roster checkpoint");
        std::set<std::uint64_t> equipment;
        for (auto item : m.equipped)
            if (!m.character.inventory().find(item) || !equipment.insert(item).second)
                throw std::runtime_error("Invalid equipment checkpoint");
    }
    if (std::any_of(state.claimed_rewards.begin(), state.claimed_rewards.end(),
                    [](const auto & id)
{
    return id.empty() || id.size() > 160;
    }) ||
    std::set<std::string>(state.claimed_rewards.begin(), state.claimed_rewards.end()).size() !=
    state.claimed_rewards.size())
    throw std::runtime_error("Invalid claimed rewards");
    for (unsigned slot = 0; slot < 8; ++slot)
        if (auto id = state.slots[slot])
        {
            if (!ids.contains(id) || !active.insert(id).second)
                throw std::runtime_error("Invalid active party checkpoint");
            const auto it = std::find_if(state.roster.begin(), state.roster.end(),
                                         [&](const auto & m)
            {
                return m.id == id;
            });
            if ((slot < 6) != it->npc_source.empty())
                throw std::runtime_error("Invalid PC/NPC checkpoint position");
        }
    if (!active.empty() && !state.slots[state.selected])
        throw std::runtime_error("Invalid selected member checkpoint");
    if (state.spell_rest)
    {
        const auto &rest = *state.spell_rest;
        std::set<MemberId> members;
        if (state.short_rest || !rest.ticket.session ||
                rest.ticket.session >= state.next_rest_session || !rest.ticket.revision ||
                rest.completed_minutes != state.time_minutes ||
                rest.completed_subminute_milliseconds != state.subminute_milliseconds ||
                rest.members.empty() || rest.members.size() > 8)
            throw std::runtime_error("Invalid spell-choice rest checkpoint");
        for (auto id : rest.members)
        {
            if (!active.contains(id) || !members.insert(id).second)
                throw std::runtime_error("Invalid spell-choice rest member");
            const auto &m = *std::find_if(state.roster.begin(), state.roster.end(),
                                          [&](const auto & m)
            {
                return m.id == id;
            });
            if (!m.last_rest_minutes || *m.last_rest_minutes != rest.completed_minutes ||
                    m.last_rest_subminute_milliseconds != rest.completed_subminute_milliseconds ||
                    std::any_of(m.character.spell_edits().begin(), m.character.spell_edits().end(),
                                [&](const auto & e)
        {
            return e.rest_session >= rest.ticket.session;
        }))
            throw std::runtime_error("Invalid completed-rest entitlement");
        }
    }
    if (state.training_rest)
    {
        const auto &rest = *state.training_rest;
        std::set<MemberId> members;
        if (state.short_rest || !rest.ticket.session ||
                rest.ticket.session >= state.next_rest_session || !rest.ticket.revision ||
                rest.completed_minutes != state.time_minutes ||
                rest.completed_subminute_milliseconds != state.subminute_milliseconds ||
                rest.members.empty() || rest.members.size() > 8)
            throw std::runtime_error("Invalid training-choice rest checkpoint");
        if (state.spell_rest && state.spell_rest->ticket != rest.ticket)
            throw std::runtime_error("Rest choice tickets disagree");
        for (auto id : rest.members)
        {
            if (!active.contains(id) || !members.insert(id).second)
                throw std::runtime_error("Invalid training-choice rest member");
            const auto &m = *std::find_if(state.roster.begin(), state.roster.end(),
                                          [&](const auto & m)
            {
                return m.id == id;
            });
            if (!m.last_rest_minutes || *m.last_rest_minutes != rest.completed_minutes ||
                    m.last_rest_subminute_milliseconds != rest.completed_subminute_milliseconds ||
                    std::any_of(m.character.training_edits().begin(),
                                m.character.training_edits().end(),
                                [&](const auto & e)
        {
            return e.rest_session >= rest.ticket.session;
        }))
            throw std::runtime_error("Invalid completed-rest training entitlement");
        }
    }
    if (state.short_rest)
    {
        const auto &rest = *state.short_rest;
        std::set<MemberId> members;
        if (!rest.ticket.session || rest.ticket.session >= state.next_rest_session ||
                !rest.ticket.revision || rest.completed_minutes != state.time_minutes ||
                rest.completed_subminute_milliseconds != state.subminute_milliseconds ||
                rest.members.empty() || rest.members.size() > 8)
            throw std::runtime_error("Invalid Short Rest checkpoint");
        for (auto id : rest.members)
        {
            if (!active.contains(id) || !members.insert(id).second)
                throw std::runtime_error("Invalid Short Rest member");
        }
    }
}

void CampaignParty::validate_rest_choices(const PartyState &state, const rules::RulesModule &rules)
{
    validate(state);
    // The maximum is the rules': Aid raises it above the sheet's.
    for (const auto &m : state.roster)
        if (m.vitals.hit_points > rules.hit_point_maximum(m.character.sheet(), m.vitals))
            throw std::runtime_error("Invalid party checkpoint");
    for (const auto &m : state.roster)
        for (const auto &e : m.character.spell_edits())
            if (e.rest_session >= state.next_rest_session)
                throw std::runtime_error("Spell history exceeds rest sequence");
    for (const auto &m : state.roster)
        for (const auto &e : m.character.training_edits())
            if (e.rest_session >= state.next_rest_session)
                throw std::runtime_error("Training history exceeds rest sequence");
    if (state.training_rest)
        for (auto id : state.training_rest->members)
        {
            const auto &m = *std::find_if(state.roster.begin(), state.roster.end(),
                                          [&](const auto & m)
            {
                return m.id == id;
            });
            if (!rules.recovery_info(m.character.sheet(), m.vitals).can_rest ||
                    !rules.rest_training_options(m.character.sheet()))
                throw std::runtime_error("Invalid Long Rest training eligibility");
        }
    if (state.spell_rest)
        for (auto id : state.spell_rest->members)
        {
            const auto &m = *std::find_if(state.roster.begin(), state.roster.end(),
                                          [&](const auto & m)
            {
                return m.id == id;
            });
            const auto options = rules.spell_choice_options(m.character.sheet(),
                rules::SpellChoiceContext::long_rest);
            if (!rules.recovery_info(m.character.sheet(), m.vitals).can_rest ||
                    (!options.may_prepare && !options.may_replace))
                throw std::runtime_error("Invalid Long Rest spell eligibility");
        }
    if (state.short_rest)
        for (auto id : state.short_rest->members)
        {
            const auto &m = *std::find_if(state.roster.begin(), state.roster.end(),
                                          [&](const auto & member)
            {
                return member.id == id;
            });
            if (!rules.recovery_info(m.character.sheet(), m.vitals).can_rest)
                throw std::runtime_error("Invalid Short Rest vitality");
        }
}

void CampaignParty::restore(PartyState state)
{
    outside_combat();
    validate_rest_choices(state, *rules_);
    for (const auto &m : state.roster)
    {
        std::vector<std::string> gear;
        for (auto id : m.equipped)
            gear.push_back(m.character.inventory().find(id)->get().definition_id);
        (void)rules_->character_profile(m.character.sheet(), gear);
    }
    state_ = std::move(state);
}

std::vector<rules::Participant> CampaignParty::participants() const
{
    std::vector<rules::Participant> result;
    for (unsigned slot = 0; slot < 8; ++slot)
        if (auto id = state_.slots[slot])
        {
            const auto &m = member(id);
            if (m.vitals.dead)
                continue;
            std::vector<std::string> gear;
            for (const auto equipped : m.equipped)
                gear.push_back(m.character.inventory().find(equipped)->get().definition_id);
            const auto p = rules_->character_profile(m.character.sheet(), gear);
            result.push_back({id,
                              "campaign-character",
                              m.character.sheet().name,
                              0,
            {1 + int(slot / 4), 1 + int(slot % 4) * 2},
            p.data,
            m.vitals});
            for (const auto &item : m.character.inventory().items())
            {
                const auto equipped = std::find(m.equipped.begin(), m.equipped.end(), item.id);
                result.back().inventory.push_back(
                {
                    item.id, item.definition_id, item.quantity,
                    equipped == m.equipped.end()
                                          ? -1
                                          : static_cast<int>(equipped - m.equipped.begin())});
            }
        }
    if (result.empty())
        throw std::runtime_error("Add a living combat-ready character first");
    return result;
}

void CampaignParty::begin_combat()
{
    outside_combat();
    if (state_.spell_rest || state_.training_rest)
        throw std::runtime_error("Finish Long Rest choices before combat");
    if (state_.next_combat_scope == std::numeric_limits<std::uint64_t>::max())
        throw std::runtime_error("Combat identity exhausted");
    combat_ = true;
    combat_registered_ = false;
    combat_elapsed_ = 0;
}

void CampaignParty::apply_combat(const rules::Snapshot &snapshot)
{
    if (!combat_ || snapshot.identity != rules_->identity())
        throw std::runtime_error("Combat rules identity mismatch");
    if (snapshot.elapsed_milliseconds < combat_elapsed_)
        throw std::runtime_error("Combat clock moved backward");
    auto next = state_;
    std::vector<MemberId> active;
    for (const auto &actor : snapshot.combatants)
        if (actor.side == 0)
            active.push_back(actor.id);
    // Combat has already advanced active actors' effects and mortality. Only reserves
    // need campaign-side updates, preventing duplicate recovery rolls.
    elapse(next, snapshot.elapsed_milliseconds - combat_elapsed_, active);
    for (const auto &actor : snapshot.combatants)
        if (actor.side == 0)
        {
            const auto it = std::find_if(next.roster.begin(), next.roster.end(),
                                         [&](const auto & m)
            {
                return m.id == actor.id;
            });
            if (it == next.roster.end() ||
                    actor.max_hit_points != rules_->hit_point_maximum(it->character.sheet(),
                            actor.persistent))
                throw std::runtime_error("Combat party identity mismatch");
            std::vector<std::string> gear;
            for (auto id : it->equipped)
                gear.push_back(it->character.inventory().find(id)->get().definition_id);
            (void)rules_->character_profile(it->character.sheet(), gear);
            it->vitals = actor.persistent;
            for (const auto &[definition, left] : actor.thrown_gear_left)
                keep_thrown_gear(*it, definition, left);
        }
    next.short_rest.reset();
    if (!combat_registered_)
        ++next.next_combat_scope;
    state_ = std::move(next);
    combat_elapsed_ = snapshot.elapsed_milliseconds;
    combat_registered_ = true;
}
} // namespace opengold
