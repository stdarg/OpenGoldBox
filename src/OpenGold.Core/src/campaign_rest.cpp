#include "opengold/campaign_party.h"
#include <algorithm>
#include <limits>

namespace opengold
{
namespace
{
rules::RestPolicy policy(const rules::RulesModule &rules, RestKind kind)
{
    switch (kind)
    {
    case RestKind::short_rest:
        return rules.short_rest_policy();
    case RestKind::long_rest:
        return rules.long_rest_policy();
    }
    throw std::runtime_error("Invalid rest kind");
}
} // namespace

std::vector<MemberRestInfo> CampaignParty::rest_info(RestKind kind) const
{
    const auto timing = policy(*rules_, kind);
    std::vector<MemberRestInfo> result;
    for (auto id : state_.slots)
        if (id)
        {
            const auto &m = member(id);
            MemberRestInfo info{id, rules_->recovery_info(m.character.sheet(), m.vitals)};
            if (kind == RestKind::long_rest && m.last_rest_minutes)
            {
                const auto elapsed = state_.time_minutes - *m.last_rest_minutes;
                if (elapsed <= timing.wait_after_rest_minutes)
                {
                    const auto needed =
                        std::uint64_t(timing.wait_after_rest_minutes - elapsed) * 60000 +
                        m.last_rest_subminute_milliseconds;
                    if (needed > state_.subminute_milliseconds)
                        info.wait_milliseconds = needed - state_.subminute_milliseconds;
                }
            }
            if (combat_)
                info.denial = RestDenial::combat;
            else if (state_.short_rest || state_.spell_rest || state_.training_rest)
                info.denial = RestDenial::spending;
            else if (!info.recovery.can_rest)
                info.denial = RestDenial::vitality;
            else if (info.wait_milliseconds)
                info.denial = RestDenial::cooldown;
            result.push_back(std::move(info));
        }
    return result;
}

bool CampaignParty::rest()
{
    return rest(RestKind::long_rest).has_value();
}

namespace
{
RestTicket new_ticket(PartyState &state)
{
    if (state.next_rest_session == std::numeric_limits<std::uint64_t>::max())
        throw std::runtime_error("Rest identity exhausted");
    return {state.next_rest_session++, 1};
}
} // namespace

std::optional<RestResult> CampaignParty::rest(RestKind kind)
{
    editable();
    std::vector<MemberId> members;
    for (const auto &info : rest_info(kind))
        if (info.denial == RestDenial::none)
            members.push_back(info.id);
    if (members.empty())
        return std::nullopt;
    const auto minutes = policy(*rules_, kind).duration_minutes;
    auto next = state_;
    // Eligibility was captured before the rest's time passed, so a member who
    // recovers from zero HP meanwhile gains no rest benefits.
    elapse(next, std::chrono::minutes{minutes});
    RestResult result{kind, minutes, {}};
    if (kind == RestKind::short_rest)
    {
        short_rest_benefits(next, members);
        if (next.short_rest)
        {
            result.spending = next.short_rest->ticket;
            result.members = next.short_rest->members;
        }
    }
    else
        long_rest_benefits(next, members, new_ticket(next), result);
    state_ = std::move(next);
    return result;
}

void CampaignParty::short_rest_benefits(PartyState &state,
                                        const std::vector<MemberId> &members) const
{
    if (state.short_rest)
        throw std::runtime_error("Finish Short Rest spending first");
    std::vector<MemberId> eligible;
    for (auto id : members)
    {
        auto &member = member_in(state, id);
        if (!rules_->recovery_info(member.character.sheet(), member.vitals).can_rest)
            continue;
        rules_->recover_short_rest(member.vitals, member.character.sheet());
        eligible.push_back(id);
    }
    if (!eligible.empty())
        state.short_rest = ShortRestSession{new_ticket(state), state.time_minutes,
                                            state.subminute_milliseconds, std::move(eligible)};
}

void CampaignParty::long_rest_benefits(PartyState &state, const std::vector<MemberId> &members,
                                       RestTicket ticket, RestResult &result) const
{
    for (auto id : members)
    {
        auto &member = member_in(state, id);
        if (!rules_->recovery_info(member.character.sheet(), member.vitals).can_rest)
            continue;
        rules_->recover(member.vitals, member.character.sheet());
        const auto choices = rules_->spell_choice_options(member.character.sheet(),
                             rules::SpellChoiceContext::long_rest);
        if (choices.may_prepare || choices.may_replace)
        {
            if (!state.spell_rest)
                state.spell_rest =
                    ShortRestSession{ticket, state.time_minutes, state.subminute_milliseconds, {}};
            state.spell_rest->members.push_back(id);
        }
        if (rules_->rest_training_options(member.character.sheet()))
        {
            if (!state.training_rest)
                state.training_rest =
                    ShortRestSession{ticket, state.time_minutes, state.subminute_milliseconds, {}};
            state.training_rest->members.push_back(id);
        }
        result.members.push_back(id);
        member.last_rest_minutes = state.time_minutes;
        member.last_rest_subminute_milliseconds = state.subminute_milliseconds;
    }
}

void CampaignParty::require_rest_ticket(RestTicket ticket) const
{
    outside_combat();
    if (!state_.short_rest || state_.short_rest->ticket != ticket ||
            state_.short_rest->completed_minutes != state_.time_minutes ||
            state_.short_rest->completed_subminute_milliseconds != state_.subminute_milliseconds)
        throw std::runtime_error("Expired Short Rest spending request");
}

std::vector<rules::HitDieResult> CampaignParty::heal_with_hit_dice(RestTicket ticket, MemberId id)
{
    require_rest_ticket(ticket);
    const auto &session = *state_.short_rest;
    if (std::find(session.members.begin(), session.members.end(), id) == session.members.end())
        throw std::runtime_error("Member did not complete this Short Rest");
    if (ticket.revision == std::numeric_limits<std::uint64_t>::max())
        throw std::runtime_error("Rest revision exhausted");
    auto next = state_;
    auto &m = member_in(next, id);
    // Spending until full or out of dice is the only sensible choice, so one
    // request spends them all; a die is never spent at full HP.
    std::vector<rules::HitDieResult> result;
    while (m.vitals.hit_points < rules_->hit_point_maximum(m.character.sheet(), m.vitals) &&
            rules_->recovery_info(m.character.sheet(), m.vitals).hit_dice > 0)
    {
        result.push_back(rules_->spend_hit_die(m.vitals, m.character.sheet(), next.random_state));
        if (result.back().healing == 0)
            break; // Healing is blocked; further dice would be wasted.
    }
    if (result.empty())
        throw std::runtime_error("No Hit Dice can heal this character");
    ++next.short_rest->ticket.revision;
    state_ = std::move(next);
    return result;
}

rules::Message CampaignParty::recover_rest_choice(RestTicket ticket, MemberId id,
        std::string_view choice)
{
    require_rest_ticket(ticket);
    const auto &session = *state_.short_rest;
    if (std::find(session.members.begin(), session.members.end(), id) == session.members.end())
        throw std::runtime_error("Member did not complete this Short Rest");
    if (ticket.revision == std::numeric_limits<std::uint64_t>::max())
        throw std::runtime_error("Rest revision exhausted");
    auto next = state_;
    auto &member = member_in(next, id);
    auto result = rules_->recover_rest_choice(member.vitals, member.character.sheet(), choice);
    ++next.short_rest->ticket.revision;
    state_ = std::move(next);
    return result;
}

void CampaignParty::finish_short_rest(RestTicket ticket)
{
    require_rest_ticket(ticket);
    state_.short_rest.reset();
}
} // namespace opengold
