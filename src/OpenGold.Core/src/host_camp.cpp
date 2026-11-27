#include "opengold/rolf_tour.h"
#include "opengold/camp_service.h"

namespace opengold::por
{
void RolfTourSession::decline_camp()
{
    // The wilderness's menu comes back as its search entry runs again, which
    // takes no time: the player only left the camp, and did not Look.
    if ((in_curse_wilderness() || in_champions_story_rest()) &&
        snapshot_.phase == TourPhase::completed && snapshot_.camp_offered)
    {
        begin_event(EventTrigger::look);
        advance(0);
    }
}

// The original engine rests in five-minute steps. Each step counts toward the
// area's check interval (0x6DD2); on reaching it, the count restarts and d100 at
// or below the chance (0x6DD3) interrupts. The count carries over between rests.
// Returns the minutes rested before an interruption, or nothing when none occurs.
std::optional<RestResult> CampService::camp_rest(TownServiceHost &host)
{
    auto &campaign = *host.campaign();
    return camp_length_ ? campaign.rest(*camp_length_) : campaign.rest(camp_kind_);
}

unsigned CampService::camp_minutes(const CampaignParty &campaign) const
{
    if (camp_length_)
    {
        return static_cast<unsigned>(camp_length_->count());
    }
    const auto &rules = campaign.rule_module();
    return (camp_kind_ == RestKind::short_rest ? rules.short_rest_policy()
                                               : rules.long_rest_policy())
        .duration_minutes;
}

std::optional<unsigned> CampService::rest_interruption(TownServiceHost &host, unsigned interval,
                                                       unsigned chance)
{
    auto &machine = host.script_machine();
    return rest_interruption(interval, chance, camp_minutes(*host.campaign()),
                             [&machine](unsigned count)
                             {
                                 return machine.engine_random(count);
                             });
}

void CampService::foresee_rest_interruption(TownServiceHost &host, unsigned interval,
                                            unsigned chance)
{
    auto &campaign = *host.campaign();
    auto &machine = host.script_machine();
    const auto rested = campaign.state().spell_rest->rested_minutes;
    const auto longest = static_cast<std::uint64_t>(campaign.longest_rest_to_memorize().count());
    if (longest <= rested)
    {
        return;
    }
    const auto dice = machine.dice_state();
    const auto checks = rest_checks_;
    const auto foreseen =
        rest_interruption(interval, chance, static_cast<unsigned>(longest - rested),
                          [&machine](unsigned count)
                          {
                              return machine.engine_random(count);
                          });
    machine.restore_dice(dice);
    rest_checks_ = checks;
    if (foreseen)
    {
        campaign.interrupt_rest_at(std::chrono::minutes{rested + *foreseen});
    }
}

void RolfTourSession::interrupt_camp()
{
    event_stage_ = EventStage::camp_interrupted;
    if (!machine_.start(3))
    {
        throw EclError("Cannot enter camp interruption script");
    }
}

std::optional<unsigned> CampService::rest_interruption(unsigned interval, unsigned chance,
                                                       unsigned duration, const Dice &dice)
{
    if (!interval)
    {
        return std::nullopt;
    }
    constexpr unsigned step_minutes = 5;
    for (unsigned rested = step_minutes; rested <= duration; rested += step_minutes)
    {
        if (++rest_checks_ < interval)
        {
            continue;
        }
        rest_checks_ = 0;
        if (dice(100) + 1 <= chance)
        {
            return rested;
        }
    }
    return std::nullopt;
}

bool RolfTourSession::camp_warded() const
{
    if (!campaign_)
    {
        return false;
    }
    const auto &state = campaign_->state();
    std::vector<rules::CharacterSheet> sheets;
    std::vector<rules::VitalState> vitals;
    for (const auto id : state.slots)
    {
        if (id)
        {
            const auto &member = campaign_->member(id);
            sheets.push_back(member.character.sheet());
            vitals.push_back(member.vitals);
        }
    }
    std::vector<rules::CampTarget> party;
    for (std::size_t n = 0; n < sheets.size(); ++n)
    {
        party.push_back({sheets[n], vitals[n]});
    }
    std::vector<std::string> definitions;
    for (const auto &enemy : encounters_.staged_enemies())
    {
        definitions.push_back(enemy.definition);
    }
    return campaign_->rule_module().keeps_away(party, definitions);
}

// Monsters that interrupt a camp are announced ahead of the script's own text.
void RolfTourSession::announce_camp_attack()
{
    constexpr std::string_view attack = "Your camp is attacked!";
    if (event_stage_ == EventStage::camp_interrupted && !snapshot_.dialogue.starts_with(attack))
    {
        snapshot_.dialogue = std::string(attack) + "\n" + snapshot_.dialogue;
    }
}

} // namespace opengold::por
