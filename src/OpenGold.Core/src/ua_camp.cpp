// The camp menu of an Unlimited Adventures design (docs/games/ua.md, Camping;
// the reconstruction's camp.c, camp_menu, alter_menu and fix_menu, and
// rest.c, rest_menu and camp_rest): Rest for the time the player sets, Alter
// the party's order or drop a member, and Fix the party with its healing
// spells. View, Magic, Save and Load are the game's own screens.
#include "opengold/original_spells.h"
#include "opengold/ua_session.h"

#include <algorithm>
#include <array>
#include <string>
#include <utility>

namespace opengold::ua
{
namespace
{

using rules::Message;

constexpr unsigned day_minutes = 24 * 60;
// The rest menu's longest rest: 99 days, 23 hours and 55 minutes.
constexpr unsigned longest_rest = 100 * day_minutes - 5;

// What a spell cast in camp does for Fix: the temple's service of the same
// name, which says whom it would help.
struct FixSpell
{
    std::string_view name;
    rules::TempleService service;
};

constexpr std::array<FixSpell, 11> fix_spells{{
    {"Cure Light Wounds", rules::TempleService::cure_light_wounds},
    {"Cure Serious Wounds", rules::TempleService::cure_serious_wounds},
    {"Cure Critical Wounds", rules::TempleService::cure_critical_wounds},
    {"Heal", rules::TempleService::heal},
    {"Raise Dead", rules::TempleService::raise_dead},
    {"Resurrection", rules::TempleService::resurrection},
    {"Neutralize Poison", rules::TempleService::neutralize_poison},
    {"Cure Blindness", rules::TempleService::cure_blindness},
    {"Cure Disease", rules::TempleService::cure_disease},
    {"Remove Curse", rules::TempleService::remove_curse},
    {"Stone to Flesh", rules::TempleService::stone_to_flesh},
}};

// The spell a camp action casts ("cast:cleric:cure_light_wounds"), if any.
[[nodiscard]] const original::Spell *cast_spell(const rules::CampAction &action)
{
    constexpr std::string_view prefix = "cast:";
    if (!action.id.starts_with(prefix))
    {
        return nullptr;
    }
    const auto number = original::spell_number(std::string_view(action.id).substr(prefix.size()));
    return number ? original::spell(*number) : nullptr;
}

[[nodiscard]] std::optional<rules::TempleService> fix_service(const original::Spell &spell)
{
    const auto found = std::ranges::find(fix_spells, spell.name, &FixSpell::name);
    return found == fix_spells.end() ? std::nullopt : std::optional{found->service};
}

[[nodiscard]] bool wound_service(rules::TempleService service) noexcept
{
    return service == rules::TempleService::cure_light_wounds ||
           service == rules::TempleService::cure_serious_wounds ||
           service == rules::TempleService::cure_critical_wounds ||
           service == rules::TempleService::heal;
}

} // namespace

bool UaSession::camp_menu()
{
    if (!party_ || !can_leave() || running_ || party_lost_ || encounter_ || party_->in_combat() ||
        party_->state().short_rest || party_->state().spell_rest || party_->state().training_rest)
    {
        return false;
    }
    snapshot_.dialogue.clear();
    snapshot_.host_dialogue.clear();
    rest_unit_ = RestUnit::hours;
    kit_camp_menu();
    refresh_snapshot();
    return true;
}

void UaSession::kit_camp_menu()
{
    camp_open_ = true;
    say_kit({"The party makes camp.", {}});
    ask_kit({{"Rest", {}}, {"Alter", {}}, {"Fix", {}}, {"Exit", {}}},
            [this](std::size_t choice)
            {
                switch (choice)
                {
                case 0:
                {
                    // The rest offered first is the time memorizing takes
                    // (camp.c, study_rest): the party's longest
                    // preparation, in five-minute steps.
                    const auto proposed = static_cast<unsigned>(party_->proposed_rest().count());
                    rest_minutes_ = std::min((proposed + 4) / 5 * 5, longest_rest);
                    rest_time_menu();
                    return;
                }
                case 1:
                    alter_menu();
                    return;
                case 2:
                    fix_party();
                    return;
                default:
                    camp_open_ = false;
                    if (camp_end_)
                    {
                        auto end = std::exchange(camp_end_, {});
                        end();
                    }
                    return;
                }
            });
}

// The rest menu (rest.c, rest_menu): the time to rest, its days, hours or
// minutes chosen and added to or taken from, 5 minutes at a time.
void UaSession::rest_time_menu()
{
    const auto days = rest_minutes_ / day_minutes, hours = rest_minutes_ % day_minutes / 60,
               minutes = rest_minutes_ % 60;
    say_kit({"Rest time: {days} days, {hours} hours, {minutes} minutes.",
             {{"days", std::to_string(days)},
              {"hours", std::to_string(hours)},
              {"minutes", std::to_string(minutes)}}});
    static constexpr std::array<std::string_view, 3> unit_words{
        "Add and Subtract change the days.", "Add and Subtract change the hours.",
        "Add and Subtract change the minutes."};
    say_kit({std::string(unit_words[static_cast<std::size_t>(rest_unit_)]), {}});
    ask_kit({{"Rest", {}},
             {"Days", {}},
             {"Hours", {}},
             {"Minutes", {}},
             {"Add", {}},
             {"Subtract", {}},
             {"Exit", {}}},
            [this](std::size_t choice)
            {
                static constexpr std::array<unsigned, 3> steps{day_minutes, 60, 5};
                const auto step = steps[static_cast<std::size_t>(rest_unit_)];
                switch (choice)
                {
                case 0:
                    if (camp_end_)
                    {
                        rest_in_service();
                        return;
                    }
                    camp_open_ = false;
                    (void)rest_for(rest_minutes_);
                    return;
                case 1:
                case 2:
                case 3:
                    rest_unit_ = static_cast<RestUnit>(choice - 1);
                    break;
                case 4:
                    rest_minutes_ = std::min(rest_minutes_ + step, longest_rest);
                    break;
                case 5:
                    rest_minutes_ = rest_minutes_ > step ? rest_minutes_ - step : 0;
                    break;
                default:
                    kit_camp_menu();
                    return;
                }
                rest_time_menu();
            });
}

// A rest of `minutes` (rest.c, camp_rest): five-minute steps, each a chance
// for the rest encounter of the party's zone to break it. The rest, broken
// or not, is the rules' rest of the time it lasted: its whole days heal, and
// it goes on while its casters memorize the spells they choose, which the
// encounter may break as well. Its encounter comes once they have chosen.
bool UaSession::rest_for(unsigned minutes)
{
    rest_ticks_ = 0;
    const auto start = party_->state().time_minutes;
    std::optional<unsigned> event;
    unsigned rested = minutes;
    for (unsigned step = 5; step <= minutes && !event; step += 5)
    {
        rest_clock_ = start + step;
        event = rest_encounter();
        rested = event ? step : minutes;
    }
    rest_clock_.reset();
    const auto granted = rested >= 5 ? party_->rest(std::chrono::minutes{rested}) : std::nullopt;
    if (!granted)
    {
        advance_minutes(rested);
    }
    else if (event && party_->state().spell_rest)
    {
        party_->interrupt_rest_at(std::chrono::minutes{rested});
    }
    else if (party_->state().spell_rest)
    {
        foresee_rest_encounter();
    }
    if (party_->state().spell_rest)
    {
        camp_memorizing_ = CampMemorizing{party_->state().time_minutes, rested, event};
        snapshot_.phase = por::TourPhase::running;
        ++snapshot_.revision;
        return !event;
    }
    end_rest(event);
    return !event;
}

void UaSession::foresee_rest_encounter()
{
    const auto rested = party_->state().spell_rest->rested_minutes;
    const auto longest = static_cast<std::uint64_t>(party_->longest_rest_to_memorize().count());
    const auto dice = dice_;
    const auto ticks = rest_ticks_;
    const bool chain = chain_allowed_;
    std::optional<std::uint64_t> foreseen;
    for (std::uint64_t step = 5; rested + step <= longest && !foreseen; step += 5)
    {
        rest_clock_ = party_->state().time_minutes + step;
        if (rest_encounter())
        {
            foreseen = step;
        }
    }
    rest_clock_.reset();
    dice_ = dice;
    rest_ticks_ = ticks;
    chain_allowed_ = chain;
    if (foreseen)
    {
        party_->interrupt_rest_at(std::chrono::minutes{rested + *foreseen});
    }
}

void UaSession::advance(double)
{
    if (!camp_memorizing_ || !party_ || party_->state().spell_rest)
    {
        return;
    }
    // The rest went on for the spells chosen: an encounter within that time
    // comes now, with the same dice foreseen.
    const auto waited = *std::exchange(camp_memorizing_, std::nullopt);
    auto event = waited.event;
    const auto went_on = party_->last_rest_minutes() - waited.rested_minutes;
    for (std::uint64_t step = 5; step <= went_on && !event; step += 5)
    {
        rest_clock_ = waited.rest_end_minutes + step;
        event = rest_encounter();
    }
    rest_clock_.reset();
    end_rest(event);
}

void UaSession::end_rest(std::optional<unsigned> event)
{
    if (!event)
    {
        say_kit({"The party rests.", {}});
        refresh_snapshot();
        return;
    }
    say_kit({"The party's repose is interrupted!", {}});
    wait_for_return(
        [this, n = *event]
        {
            run_events(n);
            refresh_snapshot();
        });
    refresh_snapshot();
}

// At an inn the rest is safe, and the camp menu comes back after it (camp.c,
// camp_menu while a service event runs); the casters keep their spells.
void UaSession::rest_in_service()
{
    if (rest_minutes_ < 5 || !party_->rest(std::chrono::minutes{rest_minutes_}))
    {
        advance_minutes(rest_minutes_);
    }
    // Each caster memorizes again the spells it cast, the rest going on for
    // them (rests_any_length).
    while (party_->state().spell_rest)
    {
        const auto id = party_->state().spell_rest->members.front();
        try
        {
            party_->choose_spells(id, {{}, party_->member(id).character.sheet().prepared_spells});
        }
        catch (const std::exception &)
        {
            // Spells a drained level no longer allows: it memorizes none.
            party_->keep_rest_spells(id);
        }
    }
    say_kit({"The party rests.", {}});
    wait_for_return(
        [this]
        {
            kit_camp_menu();
        });
}

// The Alter menu (camp.c, alter_menu): the party's order and dropping a
// member, for the selected member. The kit's game speed, combat icon and
// difficulty are the game's own settings.
void UaSession::alter_menu()
{
    ask_kit({{"Order", {}}, {"Drop", {}}, {"Exit", {}}},
            [this](std::size_t choice)
            {
                if (choice == 0)
                {
                    order_menu();
                }
                else if (choice == 1)
                {
                    drop_member();
                }
                else
                {
                    kit_camp_menu();
                }
            });
}

// The selected member moves up or down the party's order (camp.c, order_up
// and order_down: past the end it goes round to the other end).
void UaSession::order_menu()
{
    say_kit({"{name} is selected.", {{"name", member_name(party_->selected())}}});
    ask_kit({{"Up", {}}, {"Down", {}}, {"Exit", {}}},
            [this](std::size_t choice)
            {
                if (choice >= 2)
                {
                    alter_menu();
                    return;
                }
                const auto members = party_members();
                const auto id = party_->selected();
                const auto place =
                    static_cast<std::size_t>(std::ranges::find(members, id) - members.begin());
                const auto last = members.size() - 1;
                const auto next =
                    choice == 0 ? (place == 0 ? last : place - 1) : (place == last ? 0 : place + 1);
                party_->move_member(id, next);
                order_menu();
            });
}

// The selected member leaves the party for good, asked first; the last
// member never does.
void UaSession::drop_member()
{
    const auto id = party_->selected();
    if (party_members().size() < 2)
    {
        alter_menu();
        return;
    }
    say_kit({"Drop {name} from the party?", {{"name", member_name(id)}}});
    ask_yes_no(
        [this, id](bool yes)
        {
            if (yes)
            {
                party_->remove(id);
                party_->select(PartySlot{0});
            }
            alter_menu();
        });
}

// Fix (camp.c, fix_menu): the party rests while its members cast every
// healing and curing spell they have memorized that would help someone,
// each the most wounded member or one it would cure. The rest lasts the
// longest caster's casting, a quarter hour a spell level, and its rest
// encounter may break it before any spell is cast.
void UaSession::fix_party()
{
    camp_open_ = false;
    const auto helped = [this](rules::TempleService service)
    {
        std::optional<MemberId> best;
        int best_missing = 0;
        for (const auto id : party_members())
        {
            if (!party_->temple_service_helps(id, service))
            {
                continue;
            }
            if (!wound_service(service))
            {
                return std::optional{id};
            }
            const int missing =
                party_->hit_point_maximum(id) - party_->member(id).vitals.hit_points;
            if (!best || missing > best_missing)
            {
                best = id;
                best_missing = missing;
            }
        }
        return best;
    };
    // The casting time, worked out before the rest as the kit's does.
    unsigned longest = 0;
    for (const auto caster : active_members())
    {
        unsigned casting = 0;
        for (const auto &action : party_->camp_actions(caster))
        {
            const auto *spell = cast_spell(action);
            const auto service = spell ? fix_service(*spell) : std::nullopt;
            if (service && helped(*service))
            {
                casting += static_cast<unsigned>(spell->level) * 15;
            }
        }
        longest = std::max(longest, casting);
    }
    if (longest == 0)
    {
        say_kit({"No one can fix the party now.", {}});
        wait_for_return(
            [this]
            {
                kit_camp_menu();
            });
        return;
    }
    rest_ticks_ = 0;
    for (unsigned rested = 5; rested <= longest; rested += 5)
    {
        if (const auto event = rest_encounter())
        {
            advance_minutes(rested);
            say_kit({"The party's repose is interrupted!", {}});
            wait_for_return(
                [this, n = *event]
                {
                    run_events(n);
                    refresh_snapshot();
                });
            return;
        }
    }
    advance_minutes(longest);
    // Each caster casts while its spells help anyone.
    for (bool cast = true; cast;)
    {
        cast = false;
        for (const auto caster : active_members())
        {
            for (const auto &action : party_->camp_actions(caster))
            {
                const auto *spell = cast_spell(action);
                const auto service = spell ? fix_service(*spell) : std::nullopt;
                const auto target = service ? helped(*service) : std::nullopt;
                if (target)
                {
                    party_->use_camp_action(caster, *target, action.id);
                    cast = true;
                    break;
                }
            }
        }
    }
    say_kit({"The party is fixed.", {}});
    wait_for_return(
        [this]
        {
            kit_camp_menu();
        });
}

} // namespace opengold::ua
