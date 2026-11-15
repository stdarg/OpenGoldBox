#include "opengold/campaign_party.h"
#include "opengold/diagnostic_log.h"
#include "opengold/dice.h"
#include <algorithm>
#include <limits>

namespace opengold
{
namespace
{
constexpr std::uint64_t day_minutes = 24 * 60;

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
    {
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
                        static_cast<std::uint64_t>(timing.wait_after_rest_minutes - elapsed) *
                            60000 +
                        m.last_rest_subminute_milliseconds;
                    if (needed > state_.subminute_milliseconds)
                    {
                        info.wait_milliseconds = needed - state_.subminute_milliseconds;
                    }
                }
            }
            if (combat_)
            {
                info.denial = RestDenial::combat;
            }
            else if (state_.short_rest || state_.spell_rest || state_.training_rest)
            {
                info.denial = RestDenial::spending;
            }
            else if (!info.recovery.can_rest)
            {
                info.denial = RestDenial::vitality;
            }
            else if (info.wait_milliseconds)
            {
                info.denial = RestDenial::cooldown;
            }
            result.push_back(std::move(info));
        }
    }
    return result;
}

bool CampaignParty::rest()
{
    return rest(RestKind::long_rest).has_value();
}

namespace
{
// A Wand of Secrets regains 1d3 charges, up to 3 (SECRET-1).
void recharge_wands(PartyMember &member, RandomState &random)
{
    std::vector<std::uint64_t> wands;
    for (const auto &item : member.character.inventory().items())
    {
        if (item.definition_id == "wand_of_secrets")
        {
            wands.push_back(item.id);
        }
    }
    for (const auto wand : wands)
    {
        const auto charges = wand_charges(member, wand);
        if (charges < 3)
        {
            const RollPurpose purpose("a wand's charges after a long rest");
            set_wand_charges(member, wand,
                             std::min(3U, charges + static_cast<unsigned>(roll_die(random, 3))));
        }
    }
}

RestTicket new_ticket(PartyState &state)
{
    if (state.next_rest_session == std::numeric_limits<std::uint64_t>::max())
    {
        throw std::runtime_error("Rest identity exhausted");
    }
    return {state.next_rest_session++, 1};
}
} // namespace

std::optional<RestResult> CampaignParty::rest(RestKind kind)
{
    return rest_for(kind, std::nullopt);
}

std::optional<RestResult> CampaignParty::rest(std::chrono::minutes length)
{
    if (!rules_->rests_any_length())
    {
        throw std::runtime_error("These rules rest only a short or a long rest");
    }
    // The original's rest menu keeps 99 days at most (gametime.pas,
    // ValidateRestTime) and counts in five-minute steps.
    constexpr std::chrono::minutes step{5}, longest{99 * 24 * 60 + 23 * 60 + 55};
    if (length < step || length > longest)
    {
        throw std::runtime_error("A rest lasts from 5 minutes to 99 days, 23 hours and 55 minutes");
    }
    return rest_for(RestKind::long_rest, length);
}

std::chrono::minutes CampaignParty::preparation(const PartyMember &member,
                                                const rules::SpellChoices &choices) const
{
    std::vector<unsigned> levels;
    for (const auto &[item, marked] : pending_scribing(member))
    {
        levels.push_back(marked.level);
    }
    const auto memorizing =
        rules_->memorizing_levels(member.character.sheet(), member.vitals, choices);
    levels.insert(levels.end(), memorizing.begin(), memorizing.end());
    return rules_->preparation_time(levels);
}

std::chrono::minutes CampaignParty::proposed_rest() const
{
    std::chrono::minutes longest{};
    for (const auto id : state_.slots)
    {
        if (id && !member(id).vitals.dead)
        {
            const auto &sheet = member(id).character.sheet();
            longest = std::max(longest, preparation(member(id), {{}, sheet.prepared_spells}));
        }
    }
    return longest.count() > 0 ? longest : std::chrono::minutes{day_minutes};
}

std::optional<RestResult> CampaignParty::rest_for(RestKind kind,
                                                  std::optional<std::chrono::minutes> length)
{
    editable();
    std::vector<MemberId> members;
    for (const auto &info : rest_info(kind))
    {
        if (info.denial == RestDenial::none)
        {
            members.push_back(info.id);
        }
    }
    if (members.empty())
    {
        return std::nullopt;
    }
    const auto minutes =
        length ? static_cast<unsigned>(length->count()) : policy(*rules_, kind).duration_minutes;
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
    {
        long_rest_benefits(next, members, new_ticket(next), result);
    }
    state_ = std::move(next);
    return result;
}

void CampaignParty::interrupt_rest_at(std::chrono::minutes since_start)
{
    if (!rules_->rests_any_length() || !state_.spell_rest || since_start.count() <= 0 ||
        static_cast<std::uint64_t>(since_start.count()) < state_.spell_rest->rested_minutes)
    {
        throw std::runtime_error("No rest waits for its spells to be interrupted then");
    }
    auto &rest = *state_.spell_rest;
    rest.interrupted_minutes = static_cast<std::uint64_t>(since_start.count());
    // Interrupted as its own time ends, the rest goes on no more: one whose
    // first spell, scribed or memorized, was not done by then gets none.
    // Interrupted later, the rest reaches it only if a member's choice keeps
    // it going.
    if (rest.interrupted_minutes != rest.rested_minutes)
    {
        return;
    }
    std::erase_if(rest.members,
                  [&](MemberId id)
                  {
                      std::vector<unsigned> levels;
                      for (const auto &[item, marked] : pending_scribing(member(id)))
                      {
                          levels.push_back(marked.level);
                      }
                      levels.push_back(1);
                      return rules_->preparation_finishes(levels).front() > since_start;
                  });
    if (rest.members.empty())
    {
        last_rest_minutes_ = state_.spell_rest->rested_minutes;
        state_.spell_rest.reset();
    }
}

std::chrono::minutes CampaignParty::longest_rest_to_memorize() const
{
    if (!state_.spell_rest)
    {
        return {};
    }
    std::chrono::minutes longest{state_.spell_rest->rested_minutes};
    for (const auto id : state_.spell_rest->members)
    {
        const auto &resting = member(id);
        std::vector<unsigned> levels;
        for (const auto &[item, marked] : pending_scribing(resting))
        {
            levels.push_back(marked.level);
        }
        if (const auto &numbers = resting.character.sheet().original)
        {
            for (const auto &slots :
                 {numbers->cleric_slots, numbers->druid_slots, numbers->magic_user_slots})
            {
                for (std::size_t level = 0; level < slots.size(); ++level)
                {
                    levels.insert(levels.end(), static_cast<std::size_t>(std::max(slots[level], 0)),
                                  static_cast<unsigned>(level + 1));
                }
            }
        }
        longest = std::max(longest, rules_->preparation_time(levels));
    }
    return longest;
}

// The member's preparation runs from the rest's start (gametime.pas,
// CampRest, AdvanceSpellPrepBase, AdvanceSpellPrep): its base hours, then its
// scribing (CheckScribeDone) and its memorizing (CheckMemorizeDone) one spell
// at a time. The rest goes on until the last is done, unless interrupted
// first; what is not done then is not had.
rules::SpellChoices CampaignParty::rest_to_memorize(MemberId id, const rules::SpellChoices &choices)
{
    if (!rules_->rests_any_length() || !state_.spell_rest->rested_minutes)
    {
        return choices;
    }
    const auto &resting = member(id);
    const auto scribing = pending_scribing(resting);
    const auto anew = rules_->memorizing_anew(resting.character.sheet(), resting.vitals, choices);
    std::vector<unsigned> levels;
    for (const auto &[item, marked] : scribing)
    {
        levels.push_back(marked.level);
    }
    for (const auto &spell : anew)
    {
        levels.push_back(spell.level);
    }
    const auto finishes = rules_->preparation_finishes(levels);
    const auto &rest = *state_.spell_rest;
    const auto limit = rest.interrupted_minutes ? rest.interrupted_minutes
                                                : std::numeric_limits<std::uint64_t>::max();
    const auto done = [&](std::size_t spell)
    {
        return static_cast<std::uint64_t>(finishes[spell].count()) <= limit;
    };
    auto next = state_;
    const auto rested = rest.rested_minutes;
    const auto needed =
        finishes.empty() ? 0 : std::min(static_cast<std::uint64_t>(finishes.back().count()), limit);
    if (needed > rested)
    {
        const auto more = needed - rested;
        elapse(next, std::chrono::minutes{more});
        const auto days =
            static_cast<unsigned>((rested + more) / day_minutes - rested / day_minutes);
        for (const auto member_id : next.slots)
        {
            if (member_id)
            {
                auto &healing = member_in(next, member_id);
                rules_->recover_days(healing.vitals, healing.character.sheet(), days);
            }
        }
        next.spell_rest->rested_minutes += more;
        next.spell_rest->completed_minutes = next.time_minutes;
        next.spell_rest->completed_subminute_milliseconds = next.subminute_milliseconds;
    }
    std::size_t scribed = 0;
    while (scribed < scribing.size() && done(scribed))
    {
        ++scribed;
    }
    scribe_pending(member_in(next, id), scribed);
    auto memorized = choices;
    if (memorized.prepared)
    {
        std::vector<bool> dropped(memorized.prepared->size());
        for (std::size_t spell = 0; spell < anew.size(); ++spell)
        {
            dropped[anew[spell].entry] = !done(scribing.size() + spell);
        }
        std::vector<std::string> kept;
        for (std::size_t entry = 0; entry < dropped.size(); ++entry)
        {
            if (!dropped[entry])
            {
                kept.push_back((*memorized.prepared)[entry]);
            }
        }
        memorized.prepared = std::move(kept);
    }
    state_ = std::move(next);
    return memorized;
}

void CampaignParty::short_rest_benefits(PartyState &state,
                                        const std::vector<MemberId> &members) const
{
    if (state.short_rest)
    {
        throw std::runtime_error("Finish Short Rest spending first");
    }
    std::vector<MemberId> eligible;
    for (auto id : members)
    {
        auto &member = member_in(state, id);
        if (!rules_->recovery_info(member.character.sheet(), member.vitals).can_rest)
        {
            continue;
        }
        rules_->recover_short_rest(member.vitals, member.character.sheet());
        eligible.push_back(id);
    }
    if (!eligible.empty())
    {
        state.short_rest = ShortRestSession{new_ticket(state), state.time_minutes,
                                            state.subminute_milliseconds, std::move(eligible)};
    }
}

void CampaignParty::long_rest_benefits(PartyState &state, const std::vector<MemberId> &members,
                                       RestTicket ticket, RestResult &result) const
{
    for (auto id : members)
    {
        auto &member = member_in(state, id);
        if (!rules_->recovery_info(member.character.sheet(), member.vitals).can_rest)
        {
            continue;
        }
        // Rules that rest any length heal by the rest's whole days, and a
        // caster's spells come back only as it memorizes them (choose_spells),
        // its scribing done with them, their preparation shared.
        const bool any_length = rules_->rests_any_length();
        if (any_length)
        {
            rules_->recover_days(member.vitals, member.character.sheet(),
                                 static_cast<unsigned>(result.duration_minutes / day_minutes));
        }
        else
        {
            rules_->recover(member.vitals, member.character.sheet());
        }
        const auto choices = rules_->spell_choice_options(member.character.sheet(),
                                                          rules::SpellChoiceContext::long_rest);
        if (!any_length || (!choices.may_prepare && !choices.may_replace))
        {
            scribe_pending(member);
        }
        recharge_wands(member, state.random_state);
        if (any_length && !choices.may_prepare && !choices.may_replace)
        {
            rules_->end_rest(member.vitals, member.character.sheet());
        }
        if (choices.may_prepare || choices.may_replace)
        {
            if (!state.spell_rest)
            {
                state.spell_rest = ShortRestSession{ticket,
                                                    state.time_minutes,
                                                    state.subminute_milliseconds,
                                                    {},
                                                    result.duration_minutes};
            }
            state.spells_after_short_rest = false;
            state.spell_rest->members.push_back(id);
        }
        if (!rules_->rest_training_options(member.character.sheet(), RestKind::long_rest).empty())
        {
            if (!state.training_rest)
            {
                state.training_rest =
                    ShortRestSession{ticket, state.time_minutes, state.subminute_milliseconds, {}};
            }
            state.training_after_short_rest = false;
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
    {
        throw std::runtime_error("Expired Short Rest spending request");
    }
}

std::vector<rules::HitDieResult> CampaignParty::heal_with_hit_dice(RestTicket ticket, MemberId id)
{
    require_rest_ticket(ticket);
    const auto &session = *state_.short_rest;
    if (std::find(session.members.begin(), session.members.end(), id) == session.members.end())
    {
        throw std::runtime_error("Member did not complete this Short Rest");
    }
    if (ticket.revision == std::numeric_limits<std::uint64_t>::max())
    {
        throw std::runtime_error("Rest revision exhausted");
    }
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
        {
            break; // Healing is blocked; further dice would be wasted.
        }
    }
    if (result.empty())
    {
        throw std::runtime_error("No Hit Dice can heal this character");
    }
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
    {
        throw std::runtime_error("Member did not complete this Short Rest");
    }
    if (ticket.revision == std::numeric_limits<std::uint64_t>::max())
    {
        throw std::runtime_error("Rest revision exhausted");
    }
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
    // Choices a Short Rest can change (Memorize Spell's one prepared spell,
    // then the Hunter's Prey) are offered next.
    auto session = std::move(*state_.short_rest);
    state_.short_rest.reset();
    auto spells = session;
    std::erase_if(spells.members,
                  [&](MemberId id)
                  {
                      return !rules_
                                  ->spell_choice_options(member(id).character.sheet(),
                                                         rules::SpellChoiceContext::short_rest)
                                  .may_prepare;
                  });
    if (!spells.members.empty())
    {
        state_.spell_rest = std::move(spells);
        state_.spells_after_short_rest = true;
    }
    std::erase_if(
        session.members,
        [&](MemberId id)
        {
            return rules_->rest_training_options(member(id).character.sheet(), RestKind::short_rest)
                .empty();
        });
    if (session.members.empty())
    {
        return;
    }
    state_.training_rest = std::move(session);
    state_.training_after_short_rest = true;
}
} // namespace opengold
