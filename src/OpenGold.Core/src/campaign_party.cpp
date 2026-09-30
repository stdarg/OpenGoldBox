#include "opengold/campaign_party.h"
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
}

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
    case 55:
        return "chain_mail";
    case 59:
        return "shield";
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
    if (state_.rest_activity)
        throw std::runtime_error("Finish or abandon the rest before changing the party");
    if (state_.spell_rest)
        throw std::runtime_error("Finish Long Rest spell choices before changing the party");
    if (state_.training_rest)
        throw std::runtime_error("Finish Long Rest training choices before changing the party");
}

void CampaignParty::rewardable() const
{
    // An encounter may finish during a paused rest. Its earned rewards do not
    // permit unrelated edits, new resting progress or unspent Hit Dice choices.
    if (state_.rest_activity && state_.rest_activity->interrupted && !state_.short_rest)
        outside_combat();
    else
        editable();
}

void CampaignParty::commit_reward(PartyState next)
{
    if (next.rest_activity)
    {
        if (next.rest_activity->ticket.revision == std::numeric_limits<std::uint64_t>::max())
            throw std::runtime_error("Rest revision exhausted");
        ++next.rest_activity->ticket.revision;
    }
    state_ = std::move(next);
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
    return rules_->character_profile(m.character.sheet(), keys, m.equipment);
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
    return rules_->ability_check(m.character.sheet(), keys, ability, skill, m.equipment);
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
    return rules_->equipment_choices(m.character.sheet(), keys, m.equipment, keys.size() - 1);
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
    const auto plan = rules_->equipment_change(before.character.sheet(), keys, before.equipment,
        selected, operation);
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
    target.equipment = plan.equipment;
}

void CampaignParty::set_grip(MemberId id, unsigned hands)
{
    editable();
    auto &m = edit(id);
    const auto current = profile(id);
    if (std::none_of(current.grips.begin(), current.grips.end(),
                     [&](const auto & choice)
{
    return choice.hands == hands && choice.available;
}))
    throw std::runtime_error("This grip is incompatible with the equipped weapon or shield.");
    m.equipment = {hands};
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
    rewardable();
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
    commit_reward(std::move(next));
    return true;
}

void CampaignParty::award_experience(unsigned amount, std::string reward_id)
{
    rewardable();
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
    commit_reward(std::move(next));
}

bool CampaignParty::can_advance(MemberId id) const
{
    if (combat_ || state_.short_rest || state_.rest_activity || state_.spell_rest ||
            state_.training_rest)
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
    if (state_.rest_activity || state_.short_rest)
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
    if (state_.spell_rest || state_.rest_activity || state_.short_rest || !rest ||
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
    if (state_.rest_activity && state_.short_rest && milliseconds)
        throw std::runtime_error("Finish interrupted-rest Hit Dice choices before advancing time");
    if (state_.rest_activity && !state_.rest_activity->interrupted && milliseconds)
        throw std::runtime_error("Advance active rest through its activity request");
    auto next = state_;
    elapse(next, milliseconds);
    if (milliseconds)
    {
        next.short_rest.reset();
        if (next.rest_activity)
        {
            if (next.rest_activity->ticket.revision == std::numeric_limits<std::uint64_t>::max())
                throw std::runtime_error("Rest revision exhausted");
            ++next.rest_activity->ticket.revision;
        }
    }
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
            rules_->character_profile(member.character.sheet(), gear, member.equipment);
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

void CampaignParty::temple_heal(MemberId target)
{
    editable();
    const auto &current = member(target);
    if (std::find(state_.slots.begin(), state_.slots.end(), target) == state_.slots.end())
        throw std::runtime_error("Temple target must be active");
    if (current.vitals.dead || current.vitals.hit_points >= current.character.sheet().hit_points)
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
            const auto p = profile(id);
            const auto &klass = m.character.sheet().character_class;
            // Explicit SRD-to-PoR adapter: descending AC=20-AC, THAC0=20-attack bonus.
            result += (m.vitals.hit_points + 5 * std::max(0, p.armor_class - 20) +
                       5 * std::max(0, p.melee_attack_bonus + 1) +
                       (klass == "Cleric"   ? 4
                        : klass == "Wizard" ? 8
                        : 0)) /
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
    return count ? std::array<unsigned, 4> {low, high, total / count, 0} :
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
        for (unsigned n = 0; n < 7; ++n)
            fields[money[n] - 0x6B00] = m.wealth[n];
    }
    for (unsigned n = 0; n < fields.size(); ++n)
        reply.writes.push_back({static_cast<std::uint16_t>(0x6B00 + n), fields[n]});
    reply.writes.push_back({0x6DB1, static_cast<std::uint16_t>(slot)});
    reply.writes.push_back({0x6DB4, static_cast<std::uint16_t>(slot)});
    return reply;
}

void CampaignParty::read_character(unsigned slot, const por::EclMachine &vm)
{
    outside_combat();
    if (slot >= 8)
        throw std::runtime_error("Invalid ECL party position");
    if (!state_.slots[slot])
        return;
    const auto &current = member(state_.slots[slot]);
    const auto hp = vm.variable(0x6C19);
    if (hp > current.character.sheet().hit_points || (current.vitals.dead && hp))
        throw std::runtime_error("Unsupported script HP change");
    std::array<std::uint16_t, 7> wealth;
    for (unsigned n = 0; n < 7; ++n)
        wealth[n] = vm.variable(money[n]);
    // The post-combat script reads back the values just published by the host.
    // Permit that exact no-op without opening a path for script mutations.
    if (state_.rest_activity && hp == current.vitals.hit_points && wealth == current.wealth)
        return;
    if (state_.rest_activity && hp < current.vitals.hit_points && wealth == current.wealth)
    {
        if (state_.short_rest)
            throw std::runtime_error("Resolve earned Hit Dice before another script event");
        auto next = state_;
        auto &m = *std::find_if(next.roster.begin(), next.roster.end(),
                                [&](const auto & m)
        {
            return m.id == current.id;
        });
        release_rest_equipment(next, m);
        rules_->set_hit_points(m.vitals, m.character.sheet(), hp);
        if (!next.rest_activity->interrupted)
            interrupt_rest_state(next, RestInterruption::damage);
        else
        {
            if (next.rest_activity->ticket.revision == std::numeric_limits<std::uint64_t>::max())
                throw std::runtime_error("Rest revision exhausted");
            ++next.rest_activity->ticket.revision;
        }
        state_ = std::move(next);
        return;
    }
    editable();
    auto &m = edit(state_.slots[slot]);
    auto vitals = m.vitals;
    rules_->set_hit_points(vitals, m.character.sheet(), hp);
    m.wealth = wealth;
    m.vitals = std::move(vitals);
}

void CampaignParty::validate(const PartyState &state)
{
    if (state.roster.size() > 128 || state.selected >= 8 || !state.next_id ||
            state.claimed_rewards.size() > 1024 || state.subminute_milliseconds >= 60000 ||
            !state.next_combat_scope || !state.next_rest_session)
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
                m.vitals.hit_points > m.character.sheet().hit_points ||
                (m.vitals.dead && m.vitals.hit_points) || m.morale > 255 ||
                (!m.npc_source.empty() && !sources.insert(m.npc_source).second))
            throw std::runtime_error("Invalid roster checkpoint");
        std::set<std::uint64_t> equipment;
        for (auto item : m.equipped)
            if (!m.character.inventory().find(item) || !equipment.insert(item).second)
                throw std::runtime_error("Invalid equipment checkpoint");
    }
    std::set<std::tuple<std::uint64_t, std::uint64_t, unsigned>> detached;
    if (state.detached_items.size() > 4096)
        throw std::runtime_error("Too many detached inventory items");
    for (const auto &item : state.detached_items)
    {
        const bool location =
            item.rest_session
            ? item.scope == 0 && item.rest_session < state.next_rest_session &&
            item.original_owner && item.holder == 0 && item.cell == rules::Cell{}
            :
            item.scope > 0 && item.scope < state.next_combat_scope;
        if (!location || !item.token ||
                !detached.emplace(item.scope, item.rest_session, item.token).second ||
                (item.original_owner && !ids.contains(item.original_owner)) ||
                ids.contains(item.holder) || item.cell.x < 0 || item.cell.y < 0 || item.item.id ||
                item.item.quantity != 1 || item.item.definition_id.empty() || item.item.name.empty())
            throw std::runtime_error("Invalid detached inventory item");
        if (item.original && (item.item.definition_id != equipment_conversion(*item.original) ||
                              item.item.original_type != item.original->stored.type))
            throw std::runtime_error("Detached inventory provenance mismatch");
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
    if (state.rest_activity)
    {
        const auto &rest = *state.rest_activity;
        std::set<MemberId> members;
        if (!rest.ticket.session || rest.ticket.session >= state.next_rest_session ||
                !rest.ticket.revision ||
                (rest.kind != RestKind::short_rest && rest.kind != RestKind::long_rest) ||
                (rest.work != RestWork::sleep && rest.work != RestWork::light_activity &&
                 rest.work != RestWork::exertion) ||
                (rest.interruption != RestInterruption::initiative &&
                 rest.interruption != RestInterruption::spell &&
                 rest.interruption != RestInterruption::damage &&
                 rest.interruption != RestInterruption::exertion) ||
                rest.started_subminute_milliseconds >= 60000 ||
                rest.started_minutes > state.time_minutes ||
                (rest.started_minutes == state.time_minutes &&
                 rest.started_subminute_milliseconds > state.subminute_milliseconds) ||
                rest.members.empty() || rest.members.size() > 8 ||
                rest.segment_milliseconds > rest.elapsed_milliseconds ||
                rest.sleep_milliseconds > rest.elapsed_milliseconds ||
                rest.light_milliseconds != rest.elapsed_milliseconds - rest.sleep_milliseconds ||
                (state.short_rest &&
                 (!rest.interrupted || state.short_rest->ticket.session <= rest.ticket.session)))
            throw std::runtime_error("Invalid rest activity checkpoint");
        if (rest.exertion_milliseconds >
                std::numeric_limits<std::uint64_t>::max() - rest.elapsed_milliseconds)
            throw std::runtime_error("Rest clock overflow");
        const auto physical = rest.elapsed_milliseconds + rest.exertion_milliseconds;
        const auto elapsed_minutes = physical / 60000;
        const auto carry = (rest.started_subminute_milliseconds + physical % 60000) / 60000;
        if (elapsed_minutes > state.time_minutes - rest.started_minutes ||
                carry > state.time_minutes - rest.started_minutes - elapsed_minutes ||
                (elapsed_minutes + carry == state.time_minutes - rest.started_minutes &&
                 (rest.started_subminute_milliseconds + physical % 60000) % 60000 >
                 state.subminute_milliseconds))
            throw std::runtime_error("Rest progress exceeds elapsed campaign time");
        for (auto id : rest.members)
            if (!active.contains(id) || !members.insert(id).second)
                throw std::runtime_error("Invalid rest activity member");
        if (state.short_rest)
            for (auto id : state.short_rest->members)
                if (!members.contains(id))
                    throw std::runtime_error("Short Rest member did not start this activity");
    }
    if (state.spell_rest)
    {
        const auto &rest = *state.spell_rest;
        std::set<MemberId> members;
        if (state.short_rest || state.rest_activity || !rest.ticket.session ||
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
        if (state.short_rest || state.rest_activity || !rest.ticket.session ||
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

void CampaignParty::validate_rest_activity(const PartyState &state, const rules::RulesModule &rules)
{
    validate(state);
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
    for (const auto &item : state.detached_items)
        if (item.rest_session)
        {
            const auto owner = std::find_if(state.roster.begin(), state.roster.end(),
                                            [&](const auto & m)
            {
                return m.id == item.original_owner;
            });
            const std::array<std::string, 1> gear{item.item.definition_id};
            (void)rules.character_profile(owner->character.sheet(), gear);
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
    if (state.rest_activity)
    {
        const auto &activity = *state.rest_activity;
        const auto timing = activity.kind == RestKind::long_rest ? rules.long_rest_policy()
                            : rules.short_rest_policy();
        rules.validate_rest(activity);
        if (activity.kind == RestKind::long_rest)
            for (auto id : activity.members)
            {
                const auto &m = *std::find_if(state.roster.begin(), state.roster.end(),
                                              [&](const auto & member)
                {
                    return member.id == id;
                });
                if (!m.last_rest_minutes)
                    continue;
                if (*m.last_rest_minutes > activity.started_minutes)
                    throw std::runtime_error("Rest started before member eligibility");
                const auto elapsed = activity.started_minutes - *m.last_rest_minutes;
                if (elapsed <= timing.wait_after_rest_minutes &&
                        std::uint64_t(timing.wait_after_rest_minutes - elapsed) * 60000 +
                        m.last_rest_subminute_milliseconds >
                        activity.started_subminute_milliseconds)
                    throw std::runtime_error("Rest started before member eligibility");
            }
    }
}

void CampaignParty::restore(PartyState state)
{
    outside_combat();
    validate_rest_activity(state, *rules_);
    for (const auto &m : state.roster)
    {
        std::vector<std::string> gear;
        for (auto id : m.equipped)
            gear.push_back(m.character.inventory().find(id)->get().definition_id);
        (void)rules_->character_profile(m.character.sheet(), gear, m.equipment);
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
            std::vector<std::string> gear;
            std::vector<unsigned> ground;
            for (const auto equipped : m.equipped)
                gear.push_back(m.character.inventory().find(equipped)->get().definition_id);
            if (state_.rest_activity)
                for (const auto &item : state_.detached_items)
                    if (item.rest_session == state_.rest_activity->ticket.session &&
                            item.original_owner == id)
                    {
                        ground.push_back(static_cast<unsigned>(gear.size()));
                        gear.push_back(item.item.definition_id);
                    }
            if (m.vitals.dead && ground.empty())
                continue;
            const auto p = rules_->character_profile(m.character.sheet(), gear, m.equipment);
            result.push_back({id,
                              "campaign-character",
                              m.character.sheet().name,
                              0,
            {1 + int(slot / 4), 1 + int(slot % 4) * 2},
            p.data,
            m.vitals,
            false,
            false,
            std::move(ground)});
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
    if (state_.rest_activity && (!state_.rest_activity->interrupted || state_.short_rest))
        throw std::runtime_error("Resolve rest interruption and Hit Dice choices before combat");
    if (state_.next_combat_scope == std::numeric_limits<std::uint64_t>::max())
        throw std::runtime_error("Combat identity exhausted");
    combat_ = true;
    combat_registered_ = false;
    combat_elapsed_ = 0;
    combat_scope_ = state_.next_combat_scope;
    combat_items_.clear();
}

void CampaignParty::apply_physical_items(PartyState &next,
        std::vector<CombatInventoryItem> &manifest,
        const rules::Snapshot &snapshot) const
{
    const auto member = [&](MemberId id)
    {
        return std::find_if(next.roster.begin(), next.roster.end(),
                            [&](const auto & m)
        {
            return m.id == id;
        });
    };
    const auto source = [](MemberId origin, std::uint64_t inventory, unsigned ordinal)
    {
        return std::tuple{origin, inventory, inventory ? 0u : ordinal};
    };
    const bool initial = manifest.empty();
    std::set<unsigned> seen;
    for (const auto &item : snapshot.held_items)
    {
        if (!item.id || !item.origin || !item.quantity || !seen.insert(item.id).second)
            throw std::runtime_error("Invalid physical item identity");
        if (item.holder)
        {
            if (std::none_of(snapshot.combatants.begin(), snapshot.combatants.end(),
                             [&](const auto & a)
        {
            return a.id == item.holder &&
                   (item.stowed || (!a.dead && a.conscious));
            }) ||
            item.cell != rules::Cell{})
            throw std::runtime_error("Invalid physical item holder");
        }
        else if (item.quantity != 1 || item.stowed || !snapshot.battlefield.contains(item.cell) ||
                 snapshot.battlefield.at(item.cell) == 1)
            throw std::runtime_error("Invalid physical item location");
        auto found = std::find_if(manifest.begin(), manifest.end(),
                                  [&](const auto & e)
        {
            return e.token == item.id;
        });
        if (found == manifest.end())
        {
            const auto parent = std::find_if(
                                    manifest.begin(), manifest.end(),
                                    [&](const auto & e)
            {
                return source(e.origin, e.source_inventory, e.equipment_index) ==
                       source(item.origin, item.inventory_id, item.equipment_index);
            });
            CombatInventoryItem entry;
            if (parent != manifest.end())
            {
                entry = *parent;
                entry.holder = 0;
                entry.inventory_id = 0;
                entry.item.quantity = 0;
                entry.rest_token = 0;
                entry.stowed = false;
            }
            else
            {
                if (!initial)
                    throw std::runtime_error("Encounter introduced an unknown item source");
                entry.origin = item.origin;
                entry.source_inventory = item.inventory_id;
                entry.equipment_index = item.equipment_index;
                entry.holder = item.origin;
                const auto owner = member(item.origin);
                const auto actor =
                    std::find_if(snapshot.combatants.begin(), snapshot.combatants.end(),
                                 [&](const auto & a)
                {
                    return a.id == item.origin;
                });
                if (actor == snapshot.combatants.end())
                    throw std::runtime_error("Unknown physical item source");
                if (actor->side == 0)
                {
                    if (owner == next.roster.end())
                        throw std::runtime_error("Unknown party item owner");
                    entry.original_owner = owner->id;
                    if (item.inventory_id || item.equipment_index < owner->equipped.size())
                    {
                        entry.inventory_id = item.inventory_id
                                             ? item.inventory_id
                                             : owner->equipped[item.equipment_index];
                        const auto actual = owner->character.inventory().find(entry.inventory_id);
                        if (!actual)
                            throw std::runtime_error("Physical item is absent from inventory");
                        entry.item = actual->get();
                        entry.stowed = std::find(owner->equipped.begin(), owner->equipped.end(),
                                                 entry.inventory_id) == owner->equipped.end();
                        if (const auto provenance = owner->item_sources.find(entry.inventory_id);
                                provenance != owner->item_sources.end())
                            entry.original = provenance->second;
                    }
                    else
                    {
                        unsigned index = unsigned(owner->equipped.size());
                        const DetachedPartyItem *loose = nullptr;
                        if (next.rest_activity)
                            for (const auto &candidate : next.detached_items)
                                if (candidate.rest_session == next.rest_activity->ticket.session &&
                                        candidate.original_owner == owner->id)
                                    if (index++ == item.equipment_index)
                                    {
                                        loose = &candidate;
                                        break;
                                    }
                        if (!loose)
                            throw std::runtime_error("Physical item is absent from camp equipment");
                        entry.rest_token = loose->token;
                        entry.item = loose->item;
                        entry.original = loose->original;
                    }
                }
                else
                {
                    std::uint64_t quantity = 0;
                    for (const auto &row : snapshot.held_items)
                        if (source(row.origin, row.inventory_id, row.equipment_index) ==
                                source(item.origin, item.inventory_id, item.equipment_index))
                            quantity += row.quantity;
                    if (quantity > std::numeric_limits<unsigned>::max())
                        throw std::runtime_error("Physical stack exceeds limit");
                    entry.item = {0, item.definition, item.label.source, unsigned(quantity), -1};
                }
            }
            entry.token = item.id;
            entry.item.id = 0;
            manifest.push_back(std::move(entry));
            found = std::prev(manifest.end());
        }
        if (found->origin != item.origin || found->source_inventory != item.inventory_id ||
                found->equipment_index != item.equipment_index ||
                found->item.definition_id != item.definition)
            throw std::runtime_error("Physical item source changed");
    }
    if (manifest.size() != snapshot.held_items.size())
        throw std::runtime_error("Physical item disappeared");
    std::map<std::tuple<MemberId, std::uint64_t, unsigned>, std::pair<std::uint64_t, std::uint64_t>>
    totals;
    for (const auto &e : manifest)
        totals[source(e.origin, e.source_inventory, e.equipment_index)].first += e.item.quantity;
    for (const auto &i : snapshot.held_items)
        totals[source(i.origin, i.inventory_id, i.equipment_index)].second += i.quantity;
    for (const auto &[key, total] : totals)
        if (total.first != total.second)
            throw std::runtime_error("Physical inventory quantity changed");
    for (const auto &item : snapshot.held_items)
    {
        auto &entry = *std::find_if(manifest.begin(), manifest.end(),
                                    [&](const auto & e)
        {
            return e.token == item.id;
        });
        if (!entry.rest_token && entry.holder == item.holder &&
                entry.item.quantity == item.quantity && entry.stowed == item.stowed)
            continue;
        const auto old_owner = member(entry.holder);
        const bool retained = entry.holder && entry.holder == item.holder &&
                              entry.item.quantity >= item.quantity && !entry.rest_token;
        if (entry.rest_token)
        {
            if (!next.rest_activity)
                throw std::runtime_error("Camp item lost its rest session");
            const auto session = next.rest_activity->ticket.session;
            std::erase_if(next.detached_items,
                          [&](const auto & loose)
            {
                return loose.rest_session == session &&
                       loose.token == entry.rest_token;
            });
            entry.rest_token = 0;
        }
        else if (old_owner != next.roster.end() && entry.item.quantity)
        {
            auto &inventory = old_owner->character.inventory();
            const auto actual = inventory.find(entry.inventory_id);
            if (!actual || actual->get().definition_id != entry.item.definition_id)
                throw std::runtime_error("Physical transfer lost its source");
            const auto removed =
                retained ? entry.item.quantity - item.quantity : entry.item.quantity;
            if (removed)
                inventory.remove(entry.inventory_id, removed);
            if (!retained || item.stowed)
                std::erase(old_owner->equipped, entry.inventory_id);
            if (!inventory.find(entry.inventory_id))
                old_owner->item_sources.erase(entry.inventory_id);
        }
        std::erase_if(next.detached_items,
                      [&](const auto & loose)
        {
            return loose.scope == combat_scope_ && loose.token == item.id;
        });
        entry.item.quantity = item.quantity;
        entry.holder = item.holder;
        entry.stowed = item.stowed;
        if (const auto holder = member(item.holder); holder != next.roster.end())
        {
            if (!retained)
                entry.inventory_id =
                    holder->character.inventory().add(entry.item.definition_id, entry.item.name,
                                                      item.quantity, entry.item.original_type);
            if (!item.stowed && std::find(holder->equipped.begin(), holder->equipped.end(),
                                          entry.inventory_id) == holder->equipped.end())
                holder->equipped.push_back(entry.inventory_id);
            if (entry.original)
                holder->item_sources.emplace(entry.inventory_id, *entry.original);
        }
        else
        {
            entry.inventory_id = 0;
            next.detached_items.push_back({combat_scope_, item.id, entry.original_owner,
                                           item.holder, item.cell, entry.item, entry.original});
        }
    }
}

void CampaignParty::apply_combat_items(PartyState &next, std::vector<CombatInventoryItem> &manifest,
                                       const rules::Snapshot &snapshot) const
{
    if (snapshot.physical_inventory)
    {
        apply_physical_items(next, manifest, snapshot);
        return;
    }
    const auto find_member = [&](MemberId id)
    {
        return std::find_if(next.roster.begin(), next.roster.end(),
                            [&](const auto & m)
        {
            return m.id == id;
        });
    };
    std::set<std::pair<MemberId, unsigned>> sources;
    if (manifest.empty())
        for (const auto &item : snapshot.held_items)
        {
            if (!item.id || !item.origin || item.definition.empty() ||
                    !sources.emplace(item.origin, item.equipment_index).second)
                throw std::runtime_error("Invalid encounter item identity");
            CombatInventoryItem entry;
            entry.token = item.id;
            entry.origin = item.origin;
            entry.equipment_index = item.equipment_index;
            entry.holder = item.origin;
            const auto source_actor =
                std::find_if(snapshot.combatants.begin(), snapshot.combatants.end(),
                             [&](const auto & a)
            {
                return a.id == item.origin;
            });
            if (source_actor == snapshot.combatants.end())
                throw std::runtime_error("Unknown encounter item source");
            const auto owner = find_member(item.origin);
            if (owner != next.roster.end() && source_actor->side == 0)
            {
                entry.original_owner = owner->id;
                if (item.equipment_index < owner->equipped.size())
                {
                    entry.inventory_id = owner->equipped[item.equipment_index];
                    entry.item = owner->character.inventory().find(entry.inventory_id)->get();
                    if (const auto source = owner->item_sources.find(entry.inventory_id);
                            source != owner->item_sources.end())
                        entry.original = source->second;
                }
                else
                {
                    unsigned index = static_cast<unsigned>(owner->equipped.size());
                    const DetachedPartyItem *loose = nullptr;
                    if (next.rest_activity)
                        for (const auto &candidate : next.detached_items)
                            if (candidate.rest_session == next.rest_activity->ticket.session &&
                                    candidate.original_owner == owner->id)
                            {
                                if (index++ == item.equipment_index)
                                {
                                    loose = &candidate;
                                    break;
                                }
                            }
                    if (!loose)
                        throw std::runtime_error("Encounter item is missing from owner equipment");
                    entry.rest_token = loose->token;
                    entry.item = loose->item;
                    entry.original = loose->original;
                }
                if (entry.item.definition_id != item.definition)
                    throw std::runtime_error("Encounter item disagrees with inventory provenance");
            }
            else
            {
                if (std::none_of(snapshot.combatants.begin(), snapshot.combatants.end(),
                                 [&](const auto & a)
            {
                return a.id == item.origin && a.side != 0;
            }))
                throw std::runtime_error("Unknown encounter item source");
                entry.item = {0, item.definition, item.label.source, 1, -1};
            }
            entry.item.id = 0;
            entry.item.quantity = 1;
            manifest.push_back(std::move(entry));
        }
    if (manifest.size() != snapshot.held_items.size())
        throw std::runtime_error("Encounter item manifest changed size");
    std::set<unsigned> seen;
    for (const auto &item : snapshot.held_items)
    {
        if (!seen.insert(item.id).second)
            throw std::runtime_error("Duplicate encounter item");
        const auto entry = std::find_if(manifest.begin(), manifest.end(),
                                        [&](const auto & e)
        {
            return e.token == item.id;
        });
        if (entry == manifest.end() || entry->origin != item.origin ||
                entry->equipment_index != item.equipment_index ||
                entry->item.definition_id != item.definition)
            throw std::runtime_error("Encounter item manifest changed identity");
        if (item.holder && std::none_of(snapshot.combatants.begin(), snapshot.combatants.end(),
                                        [&](const auto & a)
    {
        return a.id == item.holder && !a.dead && a.conscious;
    }))
        throw std::runtime_error("Invalid encounter item holder");
        if (!item.holder &&
                (!snapshot.battlefield.contains(item.cell) || snapshot.battlefield.at(item.cell) == 1))
            throw std::runtime_error("Invalid encounter item location");
        if (!entry->rest_token && entry->holder == item.holder)
            continue;
        if (entry->rest_token)
        {
            const auto session = next.rest_activity->ticket.session;
            std::erase_if(next.detached_items,
                          [&](const auto & loose)
            {
                return loose.rest_session == session &&
                       loose.token == entry->rest_token;
            });
            entry->rest_token = 0;
        }
        else if (const auto owner = find_member(entry->holder); owner != next.roster.end())
        {
            auto &inventory = owner->character.inventory();
            const auto held = inventory.find(entry->inventory_id);
            if (!held || held->get().definition_id != entry->item.definition_id)
                throw std::runtime_error("Encounter transfer lost its inventory item");
            inventory.remove(entry->inventory_id);
            std::erase(owner->equipped, entry->inventory_id);
            if (!inventory.find(entry->inventory_id))
                owner->item_sources.erase(entry->inventory_id);
        }
        std::erase_if(next.detached_items,
                      [&](const auto & loose)
        {
            return loose.scope == combat_scope_ && loose.token == item.id;
        });
        entry->holder = item.holder;
        entry->inventory_id = 0;
        if (const auto holder = find_member(item.holder); holder != next.roster.end())
        {
            entry->inventory_id = holder->character.inventory().add(
                                      entry->item.definition_id, entry->item.name, 1, entry->item.original_type);
            holder->equipped.push_back(entry->inventory_id);
            if (entry->original)
                holder->item_sources.emplace(entry->inventory_id, *entry->original);
        }
        else
            next.detached_items.push_back({combat_scope_, item.id, entry->original_owner,
                                           item.holder, item.cell, entry->item, entry->original});
    }
}

void CampaignParty::apply_combat(const rules::Snapshot &snapshot,
                                 const rules::SafeRecovery &recovery)
{
    if (!combat_ || snapshot.identity != rules_->identity())
        throw std::runtime_error("Combat rules identity mismatch");
    if (snapshot.elapsed_milliseconds < combat_elapsed_)
        throw std::runtime_error("Combat clock moved backward");
    auto next = state_;
    auto items = combat_items_;
    apply_combat_items(next, items, snapshot);
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
            if (it == next.roster.end() || actor.max_hit_points != it->character.sheet().hit_points)
                throw std::runtime_error("Combat party identity mismatch");
            std::vector<std::string> gear;
            for (auto id : it->equipped)
                gear.push_back(it->character.inventory().find(id)->get().definition_id);
            (void)rules_->character_profile(it->character.sheet(), gear, actor.equipment);
            it->vitals = actor.persistent;
            it->equipment = actor.equipment;
        }
    if (!recovery.members.empty() || !recovery.items.empty())
    {
        if (snapshot.outcome != rules::Outcome::victory)
            throw std::runtime_error("Recovery requires victory");
        std::set<MemberId> seen;
        for (auto id : recovery.members)
        {
            if (!seen.insert(id).second ||
                    std::find(active.begin(), active.end(), id) == active.end())
                throw std::runtime_error("Invalid recovery member");
            auto &m = *std::find_if(next.roster.begin(), next.roster.end(),
                                    [&](const auto & member)
            {
                return member.id == id;
            });
            std::vector<std::string> gear;
            for (auto item : m.equipped)
                gear.push_back(m.character.inventory().find(item)->get().definition_id);
            if (!rules_->recover_at_safety(m.vitals, m.character.sheet(), gear))
                throw std::runtime_error("Unable recovery member");
        }
        std::set<unsigned> tokens;
        for (auto token : recovery.items)
            if (!tokens.insert(token).second ||
                    std::none_of(snapshot.held_items.begin(), snapshot.held_items.end(),
                                 [&](const auto & item)
        {
            return item.id == token && !item.holder &&
                   std::find(active.begin(), active.end(), item.origin) !=
                       active.end();
            }))
        throw std::runtime_error("Invalid recovery item");
        collect_equipment(next, recovery.members, combat_scope_, 0, recovery.items);
    }
    next.short_rest.reset();
    if (next.rest_activity)
    {
        if (next.rest_activity->ticket.revision == std::numeric_limits<std::uint64_t>::max())
            throw std::runtime_error("Rest revision exhausted");
        ++next.rest_activity->ticket.revision;
    }
    if (!combat_registered_)
        ++next.next_combat_scope;
    state_ = std::move(next);
    combat_items_ = std::move(items);
    combat_elapsed_ = snapshot.elapsed_milliseconds;
    combat_registered_ = true;
}
} // namespace opengold
