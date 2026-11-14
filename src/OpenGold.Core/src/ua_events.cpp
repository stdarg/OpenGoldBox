// The event loop of an Unlimited Adventures design and its kinds of event
// (docs/games/ua.md, Events). The kit runs them in one call that waits for
// the player inside; here a kind that waits for the player leaves a Pending
// answer, and the loop goes on from where it stopped once it comes.
#include "opengold/ua_session.h"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace opengold::ua
{
namespace
{

using por::TourPhase;

[[nodiscard]] unsigned word_at(const EventRecord &event, std::size_t at)
{
    return event[at] | event[at + 1] << 8;
}

[[nodiscard]] std::uint32_t long_at(const EventRecord &event, std::size_t at)
{
    return static_cast<std::uint32_t>(word_at(event, at)) | word_at(event, at + 2) << 16;
}

// A facing as the kit keeps it, 0 north and on clockwise by twos to 6 west.
[[nodiscard]] por::MapDirection kit_facing(unsigned facing)
{
    return por::MapDirection{(facing & 7) / 2 % 4};
}

// The options of a menu text: each starts at a ~ or ^ mark, the letter after
// it being its key.
[[nodiscard]] std::vector<std::string> menu_options(const std::string &menu)
{
    std::vector<std::string> options;
    for (std::size_t at = 0; at < menu.size(); ++at)
    {
        if (menu[at] != '~' && menu[at] != '^')
        {
            if (!options.empty())
            {
                options.back() += menu[at];
            }
            continue;
        }
        options.emplace_back();
    }
    for (auto &option : options)
    {
        const auto first = option.find_first_not_of(' ');
        const auto last = option.find_last_not_of(' ');
        option =
            first == std::string::npos ? std::string{} : option.substr(first, last - first + 1);
    }
    return options;
}

} // namespace

// The loop's place between steps.
enum class LoopStep : std::uint8_t
{
    outer,
    next_event,
    after_kind,
};

void UaSession::run_events(unsigned n)
{
    if (running_ || n == 0)
    {
        return;
    }
    running_ = true;
    current_ = n;
    again_ = false;
    chain_due_ = false;
    step_back_due_ = false;
    rerun_square_ = false;
    loop_step_ = static_cast<std::uint8_t>(LoopStep::outer);
    snapshot_.dialogue.clear();
    snapshot_.choices.clear();
    snapshot_.host_dialogue.clear();
    snapshot_.host_choices.clear();
    ++snapshot_.event_runs;
    pump();
}

void UaSession::pump()
{
    while (running_ && pending_.wait == Wait::nothing)
    {
        switch (static_cast<LoopStep>(loop_step_))
        {
        case LoopStep::outer:
            chain_due_ = current_ != 0;
            party_moved_ = false;
            loop_step_ = static_cast<std::uint8_t>(LoopStep::next_event);
            break;
        case LoopStep::next_event:
            if (chain_due_ && (!party_moved_ || rerun_square_ || again_))
            {
                again_ = false;
                chain_due_ = false;
                const auto event = record(current_);
                chained_event_ = event[3];
                loop_step_ = static_cast<std::uint8_t>(LoopStep::after_kind);
                if (event_passes(current_, event))
                {
                    set_done(current_, true);
                    run_kind(current_, event);
                }
                break;
            }
            // The chain has ended: an event a menu or question pushed comes
            // back while the party stays where it is.
            if (!moved_last_pass_)
            {
                current_ = pop_event();
                again_ = current_ != 0;
            }
            else
            {
                current_ = 0;
                event_stack_.clear();
            }
            if (current_ != 0 && !party_lost_)
            {
                loop_step_ = static_cast<std::uint8_t>(LoopStep::outer);
            }
            else
            {
                end_events();
            }
            break;
        case LoopStep::after_kind:
            after_kind();
            break;
        }
    }
    refresh_snapshot();
}

void UaSession::finish_kind(unsigned next)
{
    if (next != 0)
    {
        current_ = next;
        chain_due_ = true;
    }
}

void UaSession::after_kind()
{
    if (!party_lost_)
    {
        if (!chain_due_)
        {
            current_ = chain_allowed_ && !party_moved_ ? chained_event_ : 0;
            chain_due_ = current_ != 0;
        }
        if (!chain_due_)
        {
            if (step_back_due_)
            {
                step_back_due_ = false;
                step_back();
                party_moved_ = true;
            }
            if (party_moved_ && rerun_square_)
            {
                current_ = square_event(snapshot_.pose.x, snapshot_.pose.y);
                chain_due_ = current_ != 0;
            }
        }
        rerun_square_ = false;
    }
    moved_last_pass_ = party_moved_;
    party_moved_ = false;
    loop_step_ = static_cast<std::uint8_t>(LoopStep::next_event);
    if (party_lost_)
    {
        chain_due_ = false;
    }
}

void UaSession::end_events()
{
    running_ = false;
    event_stack_.clear();
    if (party_lost_)
    {
        snapshot_.phase = TourPhase::defeated;
        return;
    }
    snapshot_.phase = TourPhase::completed;
}

bool UaSession::event_passes(unsigned n, const EventRecord &event)
{
    const auto kind = static_cast<EventKind>(event[0]);
    bool ok = false;
    const auto flag = [this](unsigned number)
    {
        return quest_flags_[number & 0xFF];
    };
    const unsigned value = event[2];
    // A quest stage runs only at its stage, and a flag is set only to change it.
    const bool blocked =
        (kind == EventKind::quest_stage && flag(event[8]) != event[9]) ||
        (kind == EventKind::special_item && ((flag(event[8]) == 0) == ((event[7] & 4) != 0)));
    if (!blocked && !((event[1] & 1) && done(n)))
    {
        switch (event[1] >> 3)
        {
        case 0:
            ok = true;
            break;
        case 1:
            ok = flag(value) != 0;
            break;
        case 2:
            ok = flag(value) == 0;
            break;
        case 3:
            ok = hour() >= 6 && hour() < 20;
            break;
        case 4:
            ok = hour() < 6 || hour() >= 20;
            break;
        case 5:
            ok = roll_percent() <= value;
            break;
        case 6:
            ok = snapshot_.searching || looking_;
            break;
        case 7:
            ok = !snapshot_.searching && !looking_;
            break;
        case 8:
        {
            // Facing north, east, south or west: bits 1, 2, 4 and 8.
            const auto facing = static_cast<unsigned>(por::index(snapshot_.pose.facing));
            ok = area_.overland() || (value & (1U << facing)) != 0;
            break;
        }
        case 9:
            ok = flag(value) == 0xFF;
            break;
        case 10:
            ok = flag(value) == 0x80;
            break;
        case 11:
            ok = flag(value) != 0 && flag(value) != 0x80 && flag(value) != 0xFF;
            break;
        case 16:
            ok = party_has_class_group(value);
            break;
        case 17:
            ok = party_has_race(value);
            break;
        default:
            // Kinds 12 to 15 ask about two magical effects on the party
            // (effects 13h and 18h), which the party never has yet.
            ok = (event[1] >> 3) == 13 || (event[1] >> 3) == 15;
            break;
        }
    }
    chain_allowed_ = ok ? (event[1] & 4) == 0 : (event[1] & 2) == 0;
    return ok;
}

void UaSession::run_kind(unsigned n, const EventRecord &event)
{
    const auto kind = static_cast<EventKind>(event[0]);
    switch (kind)
    {
    case EventKind::combat:
        combat_event(event, false);
        return;
    case EventKind::pick_one_combat:
        combat_event(event, true);
        return;
    case EventKind::text:
        text_event(event);
        return;
    case EventKind::npc_says:
        // Words, as text's, only with the NPC (byte 5; 0 any) in the party.
        if (party_ && npc_member(event[5]))
        {
            text_event(event);
        }
        return;
    case EventKind::training_hall:
        training_event(event, false, service_end(event));
        return;
    case EventKind::tavern:
        tavern_event(event, false, service_end(event));
        return;
    case EventKind::shop:
        shop_event(event, false, service_end(event));
        return;
    case EventKind::temple:
        temple_event(event, false, service_end(event));
        return;
    case EventKind::vault:
        vault_event(event, false, service_end(event));
        return;
    case EventKind::camp:
        inn_event(event, false, service_end(event));
        return;
    case EventKind::small_town:
        town_event(event);
        return;
    case EventKind::guided_tour:
        tour_event(event);
        return;
    case EventKind::add_npc:
        add_npc_event(event);
        return;
    case EventKind::remove_npc:
        remove_npc_event(event);
        return;
    case EventKind::encounter:
        encounter_event(event);
        return;
    case EventKind::who_tries:
        who_tries_event(event);
        return;
    case EventKind::who_pays:
        who_pays_event(event);
        return;
    case EventKind::password:
        password_event(event);
        return;
    case EventKind::give_treasure:
        treasure_event(event, true);
        return;
    case EventKind::combat_treasure:
        treasure_event(event, false);
        return;
    case EventKind::damage:
        damage_event(event);
        return;
    case EventKind::stairs:
    case EventKind::transfer_module:
    case EventKind::teleporter:
        transfer_event(event, kind);
        return;
    case EventKind::question_buttons:
    case EventKind::question_list:
        menu_event(n, event);
        return;
    case EventKind::utilities:
        utilities_event(event);
        return;
    case EventKind::sounds:
        // The view has no sound of the design's yet; the event plays nothing.
        return;
    case EventKind::chain:
        finish_kind(event[8]);
        return;
    case EventKind::gain_experience:
        experience_event(event);
        return;
    case EventKind::pass_time:
        advance_minutes(event[8] * 24 * 60 + event[9] * 60 + event[10]);
        return;
    case EventKind::quest_stage:
        quest_event(event);
        return;
    case EventKind::yes_no:
        yes_no_event(n, event);
        return;
    case EventKind::tavern_tales:
        tavern_tales_.clear();
        for (std::size_t slot = 8; slot < 20; slot += 2)
        {
            tavern_tales_.push_back(text(word_at(event, slot)));
        }
        return;
    case EventKind::special_item:
        flag_event(event);
        return;
    default:
        unsupported(kind);
        return;
    }
}

void UaSession::unsupported(EventKind kind)
{
    diagnostics_.push_back("Event kind " + std::to_string(static_cast<unsigned>(kind)) +
                           " is not played yet");
}

void UaSession::say(std::string words)
{
    if (words.empty())
    {
        return;
    }
    // With the kit's words shown, the designer's join them as they are.
    if (!snapshot_.host_dialogue.empty())
    {
        snapshot_.host_dialogue.push_back({"{text}", {{"text", words}}});
    }
    if (!snapshot_.dialogue.empty())
    {
        snapshot_.dialogue += '\n';
    }
    snapshot_.dialogue += std::move(words);
}

void UaSession::wait_for_return(std::function<void()> then)
{
    pending_.wait = Wait::words;
    pending_.resume = [then = std::move(then)](std::size_t, std::string_view)
    {
        then();
    };
    snapshot_.phase = TourPhase::awaiting_continue;
    snapshot_.choices.clear();
    snapshot_.continue_ticket = next_ticket_++;
    ++snapshot_.prompts;
}

void UaSession::ask(std::vector<std::string> choices, std::function<void(std::size_t)> then)
{
    pending_.wait = Wait::choice;
    pending_.resume = [then = std::move(then)](std::size_t choice, std::string_view)
    {
        then(choice);
    };
    snapshot_.phase = TourPhase::awaiting_continue;
    snapshot_.choices = std::move(choices);
    snapshot_.host_choices.clear();
    snapshot_.continue_ticket = next_ticket_++;
    ++snapshot_.prompts;
}

// The kit's own Yes and No, which the view translates.
void UaSession::ask_yes_no(std::function<void(bool)> then)
{
    ask({"Yes", "No"},
        [then = std::move(then)](std::size_t choice)
        {
            then(choice == 0);
        });
    snapshot_.host_choices = {{"Yes", {}}, {"No", {}}};
}

bool UaSession::answer(std::uint64_t ticket, Wait wait, std::size_t choice, std::string_view typed)
{
    if (pending_.wait != wait || ticket != snapshot_.continue_ticket)
    {
        return false;
    }
    if (wait == Wait::choice && choice >= snapshot_.choices.size())
    {
        return false;
    }
    auto resume = std::move(pending_.resume);
    pending_ = {};
    snapshot_.phase = TourPhase::running;
    snapshot_.dialogue.clear();
    snapshot_.choices.clear();
    snapshot_.host_dialogue.clear();
    snapshot_.host_choices.clear();
    snapshot_.number_input = false;
    resume(choice, typed);
    pump();
    return true;
}

bool UaSession::continue_dialogue(std::uint64_t ticket)
{
    return answer(ticket, Wait::words, 0, {});
}

bool UaSession::choose(std::uint64_t ticket, std::size_t choice)
{
    if (pending_.wait == Wait::words)
    {
        return continue_dialogue(ticket);
    }
    return answer(ticket, Wait::choice, choice, {});
}

bool UaSession::input(std::uint64_t ticket, std::string_view value)
{
    return answer(ticket, Wait::typing, 0, value);
}

// Kind 2: up to five texts, each waiting for Return when its bit says so.
void UaSession::text_event(const EventRecord &event)
{
    show_texts(event, 0);
}

void UaSession::show_texts(const EventRecord &event, unsigned from)
{
    for (unsigned at = from; at < 5; ++at)
    {
        const auto slot = word_at(event, 8 + 2 * at);
        if (slot == 0)
        {
            continue;
        }
        say(text(slot));
        if (event[4] & (1U << at))
        {
            wait_for_return(
                [this, event, at]
                {
                    show_texts(event, at + 1);
                });
            return;
        }
    }
    if (event[4] & 0x20)
    {
        step_back_due_ = true;
    }
}

// An event's own words (bytes 4 and 5), waiting for Return as the kit does.
void UaSession::show_event_text(const EventRecord &event, std::function<void()> then)
{
    const auto slot = word_at(event, 4);
    if (slot == 0)
    {
        then();
        return;
    }
    say(text(slot));
    wait_for_return(std::move(then));
}

// Kinds 3 and 25: platinum, gems, jewelry and up to eight items. Kind 3 hands
// them over at once; kind 25 keeps them for the next fight's spoils.
void UaSession::treasure_event(const EventRecord &event, bool now)
{
    treasure_wealth_ = {};
    constexpr std::size_t platinum = 4, gems = 5, jewelry = 6;
    treasure_wealth_[platinum] = long_at(event, 4) & 0x7FFFFFFF;
    treasure_wealth_[gems] = word_at(event, 8);
    treasure_wealth_[jewelry] = word_at(event, 10);
    treasure_items_.clear();
    // Bit 7 of byte 7: the items come identified.
    treasure_identified_ = (event[7] & 0x80) != 0;
    for (std::size_t at = 12; at < 20; ++at)
    {
        if (event[at] != 0)
        {
            treasure_items_.push_back(event[at]);
        }
    }
    if (now)
    {
        hand_over_treasure();
    }
}

// Kind 4: dice of damage, once or more, on the party or some of it.
void UaSession::damage_event(const EventRecord &event)
{
    show_event_text(event,
                    [this, event]
                    {
                        for (unsigned round = 0; round < event[8] && !party_lost_; ++round)
                        {
                            damage_round(event);
                        }
                        if (party_lost_)
                        {
                            say_kit({"The entire party is killed!", {}});
                        }
                    });
}

void UaSession::damage_round(const EventRecord &event)
{
    const auto members = party_members();
    switch ((event[7] & 0xC) >> 2)
    {
    case 0:
        for (const auto id : members)
        {
            damage_member(event, id);
        }
        break;
    case 1:
        if (party_ && party_->selected())
        {
            damage_member(event, party_->selected());
        }
        break;
    case 2:
        if (!members.empty())
        {
            damage_member(event, members[roll(static_cast<unsigned>(members.size())) - 1]);
        }
        break;
    default:
        for (const auto id : members)
        {
            if (roll_percent() <= event[16])
            {
                damage_member(event, id);
            }
        }
        break;
    }
    party_lost_ = party_ && active_members().empty();
}

// Kinds 5, 11 and 34: to another square, entry point or area.
void UaSession::transfer_event(const EventRecord &event, EventKind kind)
{
    rerun_square_ = (event[12] & 4) != 0;
    const bool asks = (event[7] & 0x20) != 0;
    const auto go_on = [this, event, kind](bool go)
    {
        const bool stay = !(event[7] & 0x10) && !area_.overland() && kind != EventKind::teleporter;
        if (!go)
        {
            if (stay)
            {
                step_back();
                party_moved_ = true;
            }
            return;
        }
        transfer(event, kind);
    };
    if (asks)
    {
        say(text(word_at(event, 4)));
        const bool on_no = (event[7] & 0x40) != 0;
        ask_yes_no(
            [go_on, on_no](bool yes)
            {
                go_on(yes != on_no);
            });
        return;
    }
    show_event_text(event,
                    [go_on]
                    {
                        go_on(true);
                    });
}

void UaSession::transfer(const EventRecord &event, EventKind kind)
{
    const auto spot_in = [this](unsigned slot) -> por::PartyPose
    {
        const auto &header = area_.area.header;
        const std::size_t at = 0xE + 4 * ((slot == 0 ? 1 : slot) - 1);
        return {std::min<unsigned>(header[at + 1], area_.area.columns - 1U),
                std::min<unsigned>(header[at], area_.area.rows - 1U), kit_facing(header[at + 2])};
    };
    por::PartyPose destination;
    if (kind == EventKind::transfer_module)
    {
        if (event[14] != 0 && event[14] != area_.number)
        {
            chain_allowed_ = false;
        }
        if (event[14] != 0)
        {
            load_area(event[14]);
        }
    }
    if (event[12] & 1)
    {
        destination = spot_in(event[13]);
    }
    else
    {
        destination = {std::min<unsigned>(event[9], area_.area.columns - 1U),
                       std::min<unsigned>(event[8], area_.area.rows - 1U),
                       kit_facing((event[7] & 0xC) >> 1)};
    }
    move_party(destination.x, destination.y, destination.facing);
    party_moved_ = true;
    if (const auto arrival = word_at(event, 10); arrival != 0)
    {
        say(text(arrival));
        wait_for_return(
            []
            {
            });
    }
}

// Kinds 10 and 21: a menu of up to five choices, each to its own event.
void UaSession::menu_event(unsigned n, const EventRecord &event)
{
    say(text(word_at(event, 4)));
    const auto slot = word_at(event, 8);
    const auto options = slot == 0 ? std::vector<std::string>{} : menu_options(text(slot, false));
    if (options.empty())
    {
        choose_branch(n, event, 0);
        return;
    }
    ask(options,
        [this, n, event](std::size_t choice)
        {
            choose_branch(n, event, static_cast<unsigned>(choice));
        });
}

void UaSession::choose_branch(unsigned n, const EventRecord &event, unsigned way)
{
    if (way > 4)
    {
        return;
    }
    if (event[7] & (4U << way))
    {
        push_event(n);
    }
    if (event[15] & (1U << way))
    {
        step_back_due_ = true;
    }
    finish_kind(event[10 + way]);
}

// Kind 16: quest flags set, added to, taken from or combined.
void UaSession::utilities_event(const EventRecord &event)
{
    auto &target = quest_flags_[event[5]];
    switch (event[4] & 3)
    {
    case 1:
        target = event[6];
        break;
    case 2:
        target = static_cast<std::uint8_t>(std::min(255, target + event[6]));
        break;
    case 3:
        target = static_cast<std::uint8_t>(std::max(0, target - event[6]));
        break;
    default:
        break;
    }
    if (event[4] & 0xC)
    {
        const bool all = (event[4] & 4) != 0;
        bool result = all;
        for (std::size_t at = 7; at <= 12; ++at)
        {
            const bool set = quest_flags_[event[at]] != 0;
            result = all ? result && set : result || set;
        }
        quest_flags_[event[13]] = result ? 1 : 0;
    }
    // The adventure ends here, as when no member can act.
    if (event[4] & 0x10)
    {
        party_lost_ = true;
    }
}

// Kind 26: experience for every active member, or the selected one.
void UaSession::experience_event(const EventRecord &event)
{
    show_event_text(event,
                    [this, event]
                    {
                        if (!party_)
                        {
                            return;
                        }
                        const bool everyone = !(event[7] & 4);
                        party_->grant_script_experience(long_at(event, 8),
                                                        everyone ? MemberId{} : party_->selected());
                    });
}

// Kind 35: a quest's stage, asked about or not, moved on or failed.
void UaSession::quest_event(const EventRecord &event)
{
    say(text(word_at(event, 4)));
    const auto settle = [this, event](bool yes)
    {
        auto &stage = quest_flags_[event[8]];
        if (yes)
        {
            stage = (event[7] & 0x40) ? 0xFF : static_cast<std::uint8_t>(stage + 1);
            finish_kind(event[10]);
            return;
        }
        if (event[7] & 4)
        {
            stage = 0x80;
        }
        finish_kind(event[11]);
    };
    const unsigned mode = (event[7] >> 3) & 7;
    if (mode <= 3)
    {
        ask_yes_no(
            [settle, mode](bool yes)
            {
                settle(mode == 1 ? yes : mode == 2 ? !yes : mode == 3);
            });
        return;
    }
    const bool yes = mode == 5;
    if (word_at(event, 4) != 0)
    {
        wait_for_return(
            [settle, yes]
            {
                settle(yes);
            });
        return;
    }
    settle(yes);
}

// Kind 36: a question answered Yes or No, each with its words and event.
void UaSession::yes_no_event(unsigned n, const EventRecord &event)
{
    say(text(word_at(event, 4)));
    ask_yes_no(
        [this, n, event](bool yes)
        {
            const auto then = [this, n, event, yes]
            {
                if (event[7] & (yes ? 4 : 8))
                {
                    push_event(n);
                }
                else if (event[7] & (yes ? 0x20 : 0x10))
                {
                    step_back_due_ = true;
                }
                finish_kind(event[yes ? 8 : 9]);
            };
            const auto words = word_at(event, yes ? 10 : 12);
            if (words == 0)
            {
                then();
                return;
            }
            say(text(words));
            wait_for_return(then);
        });
}

// Kind 38: a quest flag set or cleared.
void UaSession::flag_event(const EventRecord &event)
{
    show_event_text(event,
                    [this, event]
                    {
                        quest_flags_[event[8]] = (event[7] & 4) ? 0 : 1;
                    });
}

bool UaSession::done(unsigned n) const
{
    return done_bits_[area_.number * 100 + n - 1];
}

void UaSession::set_done(unsigned n, bool value)
{
    done_bits_[area_.number * 100 + n - 1] = value;
}

void UaSession::push_event(unsigned n)
{
    constexpr std::size_t stack_size = 8;
    if (n != 0 && event_stack_.size() < stack_size)
    {
        event_stack_.push_back(n);
        set_done(n, false);
    }
}

unsigned UaSession::pop_event()
{
    if (event_stack_.empty())
    {
        return 0;
    }
    const auto n = event_stack_.back();
    event_stack_.pop_back();
    return n;
}

} // namespace opengold::ua
