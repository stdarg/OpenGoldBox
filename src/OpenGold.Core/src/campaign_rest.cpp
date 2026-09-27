#include "opengold/campaign_party.h"
#include <algorithm>
#include <limits>
#include <set>

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
            else if (state_.rest_activity)
                info.denial = RestDenial::activity;
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
void advance_ticket(RestTicket &ticket)
{
    if (ticket.revision == std::numeric_limits<std::uint64_t>::max())
        throw std::runtime_error("Rest revision exhausted");
    ++ticket.revision;
}

RestTicket new_ticket(PartyState &state)
{
    if (state.next_rest_session == std::numeric_limits<std::uint64_t>::max())
        throw std::runtime_error("Rest identity exhausted");
    return {state.next_rest_session++, 1};
}
} // namespace

std::optional<RestResult> CampaignParty::rest(RestKind kind)
{
    // Existing safe-camp/inn callers retain an atomic operation. The same
    // activity engine also supports hosts which stop at an interruption.
    auto before = state_;
    try
    {
        const auto ticket = begin_rest(kind);
        if (!ticket)
            return std::nullopt;
        return advance_rest(*ticket, remaining_rest_milliseconds(), state_.rest_activity->work);
    }
    catch (...)
    {
        state_ = std::move(before);
        throw;
    }
}

std::optional<RestTicket> CampaignParty::begin_rest(RestKind kind)
{
    editable();
    RestActivity activity;
    static_cast<rules::RestProgress &>(activity) = rules_->begin_rest(kind);
    activity.started_minutes = state_.time_minutes;
    activity.started_subminute_milliseconds = state_.subminute_milliseconds;
    for (const auto &info : rest_info(kind))
        if (info.denial == RestDenial::none)
            activity.members.push_back(info.id);
    if (activity.members.empty())
        return std::nullopt;
    auto next = state_;
    activity.ticket = new_ticket(next);
    next.rest_activity = activity;
    apply_rest_work(next, activity.members, activity.work);
    state_ = std::move(next);
    return activity.ticket;
}

void CampaignParty::require_activity_ticket(RestTicket ticket) const
{
    outside_combat();
    if (!state_.rest_activity || state_.rest_activity->ticket != ticket)
        throw std::runtime_error("Expired rest activity request");
}

std::uint64_t CampaignParty::remaining_rest_milliseconds() const
{
    if (!state_.rest_activity)
        return 0;
    return rules_->remaining_rest(*state_.rest_activity);
}

void CampaignParty::short_rest_benefits(PartyState &state,
                                        const std::vector<MemberId> &members) const
{
    if (state.short_rest)
        throw std::runtime_error("Finish Short Rest spending first");
    std::vector<MemberId> eligible;
    for (auto id : members)
    {
        auto &member = *std::find_if(state.roster.begin(), state.roster.end(),
                                     [&](const auto &m)
                                     {
                                         return m.id == id;
                                     });
        if (!rules_->recovery_info(member.character.sheet(), member.vitals).can_rest)
            continue;
        rules_->recover_short_rest(member.vitals, member.character.sheet());
        eligible.push_back(id);
    }
    if (!eligible.empty())
        state.short_rest = ShortRestSession{new_ticket(state), state.time_minutes,
                                            state.subminute_milliseconds, std::move(eligible)};
}

void CampaignParty::apply_rest_work(PartyState &state, std::span<const MemberId> members,
                                    RestWork work) const
{
    for (auto id : members)
    {
        auto found = std::find_if(state.roster.begin(), state.roster.end(),
                                  [&](const auto &m)
                                  {
                                      return m.id == id;
                                  });
        if (found == state.roster.end())
            throw std::runtime_error("Unknown rest member");
        release_rest_equipment(state, *found);
        if (work != RestWork::sleep ||
            rules_->recovery_info(found->character.sheet(), found->vitals).can_rest)
            rules_->set_rest_work(found->vitals, found->character.sheet(), work);
        release_rest_equipment(state, *found);
    }
}

void CampaignParty::release_rest_equipment(PartyState &state, PartyMember &member) const
{
    if (!state.rest_activity || member.equipped.empty())
        return;
    std::vector<std::string> keys;
    for (const auto id : member.equipped)
        keys.push_back(member.character.inventory().find(id)->get().definition_id);
    const auto released = rules_->released_equipment(member.character.sheet(), member.vitals, keys);
    if (released.empty())
        return;
    const auto session = state.rest_activity->ticket.session;
    unsigned token{};
    for (const auto &item : state.detached_items)
        if (item.rest_session == session)
            token = std::max(token, item.token);
    if (state.detached_items.size() + released.size() > 4096 ||
        released.size() > std::numeric_limits<unsigned>::max() - token)
        throw std::runtime_error("Too many detached inventory items");
    std::set<unsigned> indices;
    for (const auto index : released)
        if (index >= keys.size() || !indices.insert(index).second)
            throw std::runtime_error("Invalid released equipment ordinal");
    const auto equipped = member.equipped;
    for (const auto index : released)
    {
        const auto id = equipped[index];
        auto item = member.character.inventory().find(id)->get();
        item.id = 0;
        item.quantity = 1;
        std::optional<por::Equipment> original;
        if (const auto source = member.item_sources.find(id); source != member.item_sources.end())
            original = source->second;
        state.detached_items.push_back(
            {0, ++token, member.id, 0, {}, std::move(item), std::move(original), session});
        member.character.inventory().remove(id);
        std::erase(member.equipped, id);
        if (!member.character.inventory().find(id))
            member.item_sources.erase(id);
    }
    member.equipment = {};
}

void CampaignParty::collect_equipment(PartyState &state, std::span<const MemberId> collectors,
                                      std::uint64_t scope, std::uint64_t rest_session,
                                      std::span<const unsigned> tokens) const
{
    if (collectors.empty())
        return;
    for (auto it = state.detached_items.begin(); it != state.detached_items.end();)
    {
        if (it->holder || it->scope != scope || it->rest_session != rest_session ||
            std::find(tokens.begin(), tokens.end(), it->token) == tokens.end())
        {
            ++it;
            continue;
        }
        auto owner = std::find_if(state.roster.begin(), state.roster.end(),
                                  [&](const auto &m)
                                  {
                                      return m.id == it->original_owner;
                                  });
        if (owner == state.roster.end())
            throw std::runtime_error("Recovery item has no original owner");
        if (owner->vitals.dead)
            owner = std::find_if(state.roster.begin(), state.roster.end(),
                                 [&](const auto &m)
                                 {
                                     return m.id == collectors.front();
                                 });
        if (owner == state.roster.end())
            throw std::runtime_error("Recovery collector is missing");
        const auto id = owner->character.inventory().add(it->item.definition_id, it->item.name, 1,
                                                         it->item.original_type);
        if (it->original)
            owner->item_sources.emplace(id, *it->original);
        it = state.detached_items.erase(it);
    }
}

void CampaignParty::recover_camp(PartyState &state) const
{
    std::vector<MemberId> collectors;
    for (auto id : state.slots)
        if (id)
        {
            auto &m = *std::find_if(state.roster.begin(), state.roster.end(),
                                    [&](const auto &member)
                                    {
                                        return member.id == id;
                                    });
            std::vector<std::string> gear;
            for (auto item : m.equipped)
                gear.push_back(m.character.inventory().find(item)->get().definition_id);
            if (rules_->recover_at_safety(m.vitals, m.character.sheet(), gear))
                collectors.push_back(id);
        }
    const auto session = state.rest_activity->ticket.session;
    std::vector<unsigned> tokens;
    for (const auto &item : state.detached_items)
        if (item.rest_session == session)
            tokens.push_back(item.token);
    collect_equipment(state, collectors, 0, session, tokens);
}

void CampaignParty::loud_noise(std::span<const MemberId> affected)
{
    outside_combat();
    auto next = state_;
    apply_rest_work(next, affected, RestWork::light_activity);
    if (next.rest_activity)
        advance_ticket(next.rest_activity->ticket);
    state_ = std::move(next);
}

void CampaignParty::interrupt_rest_state(PartyState &state, RestInterruption cause) const
{
    const auto outcome = rules_->interrupt_rest(*state.rest_activity, cause);
    if (outcome.benefit == rules::RestBenefit::short_rest)
        short_rest_benefits(state, state.rest_activity->members);
    if (!outcome.progress)
    {
        state.rest_activity.reset();
        return;
    }
    static_cast<rules::RestProgress &>(*state.rest_activity) = *outcome.progress;
    advance_ticket(state.rest_activity->ticket);
}

void CampaignParty::interrupt_rest(RestTicket ticket, RestInterruption cause)
{
    require_activity_ticket(ticket);
    auto next = state_;
    interrupt_rest_state(next, cause);
    state_ = std::move(next);
}

std::optional<RestResult> CampaignParty::advance_rest(RestTicket ticket, std::uint64_t milliseconds,
                                                      RestWork work)
{
    require_activity_ticket(ticket);
    if (state_.short_rest)
        throw std::runtime_error("Resolve Hit Dice choices before advancing rest");
    const auto outcome = rules_->advance_rest(*state_.rest_activity, milliseconds, work);
    auto next = state_;
    auto &activity = *next.rest_activity;
    apply_rest_work(next, activity.members, work);
    elapse(next, milliseconds);
    advance_ticket(activity.ticket);
    RestResult result{activity.kind, outcome.completed_duration_milliseconds / 60000, {}};
    if (outcome.benefit == rules::RestBenefit::short_rest)
    {
        short_rest_benefits(next, activity.members);
        if (next.short_rest)
        {
            result.spending = next.short_rest->ticket;
            result.members = next.short_rest->members;
        }
    }
    else if (outcome.benefit == rules::RestBenefit::long_rest)
        for (auto id : activity.members)
        {
            auto &member = *std::find_if(next.roster.begin(), next.roster.end(),
                                         [&](const auto &m)
                                         {
                                             return m.id == id;
                                         });
            if (!rules_->recovery_info(member.character.sheet(), member.vitals).can_rest)
                continue;
            rules_->recover(member.vitals, member.character.sheet());
            const auto choices = rules_->spell_choice_options(member.character.sheet(),
                                                              rules::SpellChoiceContext::long_rest);
            if (choices.may_prepare || choices.may_replace)
            {
                if (!next.spell_rest)
                    next.spell_rest = ShortRestSession{
                        activity.ticket, next.time_minutes, next.subminute_milliseconds, {}};
                next.spell_rest->members.push_back(id);
            }
            if (rules_->rest_training_options(member.character.sheet()))
            {
                if (!next.training_rest)
                    next.training_rest = ShortRestSession{
                        activity.ticket, next.time_minutes, next.subminute_milliseconds, {}};
                next.training_rest->members.push_back(id);
            }
            result.members.push_back(id);
            member.last_rest_minutes = next.time_minutes;
            member.last_rest_subminute_milliseconds = next.subminute_milliseconds;
        }
    if (outcome.progress)
        static_cast<rules::RestProgress &>(activity) = *outcome.progress;
    else
    {
        apply_rest_work(next, activity.members, RestWork::light_activity);
        recover_camp(next);
        next.rest_activity.reset();
    }
    state_ = std::move(next);
    if (outcome.completed_duration_milliseconds)
        return result;
    return std::nullopt;
}

void CampaignParty::resume_rest(RestTicket ticket)
{
    require_activity_ticket(ticket);
    if (!state_.rest_activity->interrupted || state_.short_rest)
        throw std::runtime_error("Resolve interruption and Hit Dice choices before resuming");
    auto next = state_;
    auto &activity = *next.rest_activity;
    std::erase_if(activity.members,
                  [&](auto id)
                  {
                      const auto &m = member(id);
                      return !rules_->recovery_info(m.character.sheet(), m.vitals).can_rest;
                  });
    if (activity.members.empty())
        throw std::runtime_error("No eligible member can resume resting");
    static_cast<rules::RestProgress &>(activity) = rules_->resume_rest(activity);
    apply_rest_work(next, activity.members, activity.work);
    advance_ticket(activity.ticket);
    state_ = std::move(next);
}

void CampaignParty::abandon_rest(RestTicket ticket)
{
    require_activity_ticket(ticket);
    auto next = state_;
    apply_rest_work(next, next.rest_activity->members, RestWork::light_activity);
    recover_camp(next);
    next.rest_activity.reset();
    next.short_rest.reset();
    state_ = std::move(next);
}

bool CampaignParty::prepare_combat()
{
    outside_combat();
    if (state_.rest_activity && !state_.rest_activity->interrupted)
    {
        auto next = state_;
        interrupt_rest_state(next, RestInterruption::initiative);
        state_ = std::move(next);
    }
    return !state_.short_rest && !state_.spell_rest && !state_.training_rest;
}

void CampaignParty::require_rest_ticket(RestTicket ticket) const
{
    outside_combat();
    if (!state_.short_rest || state_.short_rest->ticket != ticket ||
        state_.short_rest->completed_minutes != state_.time_minutes ||
        state_.short_rest->completed_subminute_milliseconds != state_.subminute_milliseconds)
        throw std::runtime_error("Expired Short Rest spending request");
}

rules::HitDieResult CampaignParty::spend_hit_die(RestTicket ticket, MemberId id)
{
    require_rest_ticket(ticket);
    const auto &session = *state_.short_rest;
    if (std::find(session.members.begin(), session.members.end(), id) == session.members.end())
        throw std::runtime_error("Member did not complete this Short Rest");
    if (ticket.revision == std::numeric_limits<std::uint64_t>::max())
        throw std::runtime_error("Rest revision exhausted");
    auto next = state_;
    auto &m = *std::find_if(next.roster.begin(), next.roster.end(),
                            [&](const auto &value)
                            {
                                return value.id == id;
                            });
    const auto result = rules_->spend_hit_die(m.vitals, m.character.sheet(), next.random_state);
    ++next.short_rest->ticket.revision;
    if (next.rest_activity)
        advance_ticket(next.rest_activity->ticket);
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
    auto &member = *std::find_if(next.roster.begin(), next.roster.end(),
                                 [&](const auto &value)
                                 {
                                     return value.id == id;
                                 });
    auto result = rules_->recover_rest_choice(member.vitals, member.character.sheet(), choice);
    ++next.short_rest->ticket.revision;
    if (next.rest_activity)
        advance_ticket(next.rest_activity->ticket);
    state_ = std::move(next);
    return result;
}

void CampaignParty::finish_short_rest(RestTicket ticket)
{
    require_rest_ticket(ticket);
    auto next = state_;
    next.short_rest.reset();
    if (next.rest_activity)
        advance_ticket(next.rest_activity->ticket);
    state_ = std::move(next);
}
} // namespace opengold
