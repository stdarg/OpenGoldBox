// Included in the campaign rest suite so the same rules, clocks and save codecs
// exercise both the existing atomic callers and interrupted activity callers.
namespace rest_activity_checks
{
constexpr std::uint64_t minute = 60000;

RestTicket ticket(const CampaignParty &party)
{
    check(party.state().rest_activity.has_value(), "Rest activity exists");
    return party.state().rest_activity->ticket;
}

CampaignParty wounded()
{
    CampaignParty party(module());
    party.add_pc(hero());
    auto state = party.checkpoint();
    state.roster[0].vitals = {1, false, "SRD1 0 0 0 0 0"};
    party.restore(state);
    return party;
}

void advance(CampaignParty &party, std::uint64_t elapsed, RestWork work = RestWork::sleep)
{
    (void)party.advance_rest(ticket(party), elapsed, work);
}

void finish_spending(CampaignParty &party)
{
    check(party.state().short_rest.has_value(), "Earned spending exists");
    party.finish_short_rest(party.state().short_rest->ticket);
}

void prior_writer()
{
    const auto read = [](const char *file)
    {
        std::ifstream in(std::filesystem::path(OPENGOLD_SOURCE_DIR) / "tests/fixtures" / file,
                         std::ios::binary);
        check(bool(in), "Prior writer fixture exists");
        return std::string(std::istreambuf_iterator<char>(in), {});
    };
    const auto before = read("campaign-v11-rest-activity-before.ogs");
    auto party = loaded(before);
    check(!party.state().rest_activity && saved(party) == saved(loaded(before)),
          "Actual previous writer retains pending spending without inventing activity");
    (void)party.spend_hit_die(party.state().short_rest->ticket, 1);
    check(saved(party) == saved(loaded(read("campaign-v11-rest-activity-spent.ogs"))),
          "Prior writer's next die/RNG/resource continuation remains exact");
}

void segments()
{
    auto party = wounded();
    const auto start = party.begin_rest(RestKind::long_rest);
    check(start.has_value(), "Start Long Rest");
    check(party.state().time_minutes == 0 && winds(party.member(1)) == 0,
          "Starting rest grants no time or recovery");
    advance(party, 70 * minute);
    const auto before = saved(party);
    rejects(
        [&]
    {
        advance(party, 0);
    });
    rejects(
        [&]
    {
        (void)party.advance_rest(*start, minute, RestWork::sleep);
    });
    rejects(
        [&]
    {
        party.advance_time(1);
    });
    rejects(
        [&]
    {
        party.remove(1);
    });
    rejects(
        [&]
    {
        party.begin_combat();
    });
    check(saved(party) == before,
          "Invalid, stale and out-of-band requests leave active rest unchanged");
    check(before.starts_with("OPENGOLD-CAMPAIGN 12\n"), "Active rest uses version twelve");
    party = loaded(before);
    check(saved(party) == before && party.remaining_rest_milliseconds() == 410 * minute,
          "Active progress round-trips exactly");
    const auto interrupted_ticket = ticket(party);
    party.interrupt_rest(interrupted_ticket, RestInterruption::damage);
    check(winds(party.member(1)) == 1 && party.state().short_rest &&
          party.remaining_rest_milliseconds() == 470 * minute,
          "70-minute interruption grants one Short Rest and adds one hour");
    const auto paused = saved(party);
    rejects(
        [&]
    {
        party.interrupt_rest(interrupted_ticket, RestInterruption::damage);
    });
    rejects(
        [&]
    {
        party.resume_rest(ticket(party));
    });
    rejects(
        [&]
    {
        party.begin_combat();
    });
    rejects(
        [&]
    {
        party.advance_time(1);
    });
    check(saved(party) == paused,
          "Duplicate interruption, premature resume and combat cannot lose pending choices");
    party = loaded(paused);
    const auto die = party.spend_hit_die(party.state().short_rest->ticket, 1);
    check(die.healing > 0, "Sequential die spend works after interruption");
    finish_spending(party);
    party.resume_rest(ticket(party));
    advance(party, 10 * minute);
    party.interrupt_rest(ticket(party), RestInterruption::spell);
    check(!party.state().short_rest && winds(party.member(1)) == 1 &&
          party.state().rest_activity->extension_milliseconds == 120 * minute,
          "Ten fresh minutes cannot credit the earlier 70-minute segment");
    party.resume_rest(ticket(party));
    advance(party, 60 * minute);
    party.interrupt_rest(ticket(party), RestInterruption::initiative);
    check(party.state().short_rest && winds(party.member(1)) == 2,
          "A fresh full hour earns another single recharge");
    finish_spending(party);
    party.resume_rest(ticket(party));
    const auto result =
        party.advance_rest(ticket(party), party.remaining_rest_milliseconds(), RestWork::sleep);
    check(result && result->duration_minutes == 660 && !party.state().rest_activity &&
          !party.state().short_rest && party.state().time_minutes == 660,
          "Three interruptions require eleven resting hours without repeating earlier time");
    check(party.member(1).vitals.hit_points == party.member(1).character.sheet().hit_points &&
          party.member(1).last_rest_minutes == 660 && winds(party.member(1)) == 3,
          "Long Rest completion restores resources and records its actual completion time once");
    check(saved(party).starts_with("OPENGOLD-CAMPAIGN 11\n"),
          "No activity retains compact version eleven");
}

void boundaries()
{
    for (const auto kind :
            {
                RestKind::short_rest, RestKind::long_rest
            })
        for (const auto cause :
                {
                    RestInterruption::initiative, RestInterruption::spell, RestInterruption::damage
                })
        {
            auto party = wounded();
            (void)party.begin_rest(kind);
            advance(party, 60 * minute - 1, RestWork::light_activity);
            party.interrupt_rest(ticket(party), cause);
            check(!party.state().short_rest && winds(party.member(1)) == 0 &&
                  party.state().time_minutes == 59 &&
                  party.state().subminute_milliseconds == 59999,
                  "Interrupting before one hour grants no benefits for every cause");
            check(party.state().rest_activity.has_value() == (kind == RestKind::long_rest),
                  "Only Long Rest is resumable");
        }
    auto party = wounded();
    (void)party.begin_rest(RestKind::long_rest);
    advance(party, 120 * minute, RestWork::light_activity);
    const auto before = saved(party);
    rejects(
        [&]
    {
        advance(party, 1, RestWork::light_activity);
    });
    check(saved(party) == before,
          "Light activity cannot exceed two hours or consume time on rejection");
    advance(party, 360 * minute - 1);
    check(party.state().rest_activity && winds(party.member(1)) == 0,
          "One millisecond before six hours sleep has no Long Rest recovery");
    advance(party, 1);
    check(!party.state().rest_activity && party.state().time_minutes == 480,
          "Six hours sleep plus two light hours completes exactly");
    party = wounded();
    (void)party.begin_rest(RestKind::long_rest);
    advance(party, 10 * minute);
    advance(party, 60 * minute - 1, RestWork::exertion);
    check(!party.state().rest_activity->interrupted &&
          party.state().rest_activity->elapsed_milliseconds == 10 * minute,
          "Exertion consumes campaign time but does not count as resting");
    advance(party, 1, RestWork::exertion);
    check(party.state().rest_activity->interrupted && !party.state().short_rest &&
          party.state().time_minutes == 70,
          "One hour exertion interrupts without inventing an hour of rest");
    party.abandon_rest(ticket(party));
    check(party.state().time_minutes == 70 && !party.state().rest_activity,
          "Abandon preserves elapsed time");
    party = wounded();
    (void)party.begin_rest(RestKind::short_rest);
    advance(party, 1, RestWork::exertion);
    check(!party.state().rest_activity && !party.state().short_rest &&
          party.state().subminute_milliseconds == 1,
          "Strenuous activity ends a Short Rest without benefits");
}

void combat_and_validation()
{
    auto party = wounded();
    (void)party.begin_rest(RestKind::long_rest);
    advance(party, minute);
    party.interrupt_rest(ticket(party), RestInterruption::initiative);
    auto before = saved(party);
    party.begin_combat();
    Snapshot invalid;
    rejects(
        [&]
    {
        party.apply_combat(invalid);
    });
    party.end_combat();
    check(saved(party) == before,
          "Failed combat initialization preserves suspended progress and RNG");
    auto actors = party.participants();
    actors.push_back({999, "bandit", "Enemy", 1, {6, 6}});
    const auto combat = module()->create(
    { {8, 8, std::vector<std::uint8_t>(64)}, actors
    }, 42);
    party.begin_combat();
    party.apply_combat(combat->snapshot());
    party.end_combat();
    check(party.state().rest_activity && party.state().rest_activity->interrupted &&
          party.state().rest_activity->elapsed_milliseconds == minute,
          "Successful combat handoff retains rest progress");
    party = loaded(saved(party));
    party.resume_rest(ticket(party));
    before = saved(party);
    for (unsigned corrupt = 0; corrupt < 10; ++corrupt)
    {
        auto state = party.checkpoint();
        auto &activity = *state.rest_activity;
        switch (corrupt)
        {
        case 0:
            activity.members.push_back(1);
            break;
        case 1:
            activity.ticket.session = state.next_rest_session;
            break;
        case 2:
            activity.segment_milliseconds = activity.elapsed_milliseconds + 1;
            break;
        case 3:
            activity.extension_milliseconds = 1;
            break;
        case 4:
            activity.light_milliseconds = 121 * minute;
            activity.elapsed_milliseconds =
                activity.light_milliseconds + activity.sleep_milliseconds;
            break;
        case 5:
            activity.kind = static_cast<RestKind>(77);
            break;
        case 6:
            activity.work = static_cast<RestWork>(77);
            break;
        case 7:
            activity.started_minutes = state.time_minutes + 1;
            break;
        case 8:
            activity.exertion_milliseconds = 59 * minute;
            break;
        case 9:
            state.roster[0].last_rest_minutes = 0;
            break;
        }
        rejects(
            [&]
        {
            party.restore(state);
        });
        check(saved(party) == before, "Malformed activity cannot replace campaign state");
    }
    auto state = party.checkpoint();
    state.rest_activity->ticket.revision = std::numeric_limits<std::uint64_t>::max();
    party.restore(state);
    before = saved(party);
    rejects(
        [&]
    {
        advance(party, minute);
    });
    check(saved(party) == before, "Revision overflow cannot advance time or effects");
}

void discard_and_bad_saves()
{
    auto party = wounded();
    (void)party.begin_rest(RestKind::long_rest);
    const auto original = saved(party);
    const auto body = original.substr(original.find('\n', original.find('\n') + 1) + 1);
    const std::string suffix = "2 0 1 1 1 1 0 0 0 0 0 0 0 0 0 0 0 1 1 ";
    check(body.ends_with(suffix),
          "Independent version-twelve activity footer matches documented fields");
    for (const auto field :
            {
                5u, 7u, 13u, 15u
            })
    {
        std::array<std::uint64_t, 19> values{2, 0, 1, 1, 1, 1, 0, 0, 0, 0,
                                             0, 0, 0, 0, 0, 0, 0, 1, 1};
        values[field] = field == 7 ? 60000 : field == 13 ? 1 : 99;
        std::ostringstream footer;
        for (auto value : values)
            footer << value << ' ';
        auto bytes = payload(body.substr(0, body.size() - suffix.size()) + footer.str());
        bytes.replace(0, bytes.find('\n') + 1, "OPENGOLD-CAMPAIGN 12\n");
        rejects(
            [&]
        {
            (void)decode_campaign(bytes, *srd5::character_rules(), *module(), "campaign-rest",
            nullptr);
        });
        check(saved(party) == original,
              "Correct-checksum malformed activity decoding cannot alter live state");
    }
    advance(party, 70 * minute);
    party.interrupt_rest(ticket(party), RestInterruption::damage);
    (void)party.spend_hit_die(party.state().short_rest->ticket, 1);
    auto vitals = party.member(1).vitals;
    module()->set_rest_work(vitals, party.member(1).character.sheet(), RestWork::light_activity);
    (void)module()->recover_at_safety(vitals, party.member(1).character.sheet(), {});
    const auto rng = party.state().random_state;
    const auto elapsed = party.state().time_minutes;
    party.abandon_rest(ticket(party));
    check(!party.state().rest_activity && !party.state().short_rest &&
          party.member(1).vitals == vitals && party.state().random_state == rng &&
          party.state().time_minutes == elapsed,
          "Ending unfinished rest retains earned healing/recharge, spent dice, time and RNG");
}

void host_interruptions()
{
    for (auto kind :
            {
                RestKind::short_rest, RestKind::long_rest
            })
        for (auto elapsed :
                {
                    minute - 1, 70 * minute
                })
        {
            if (kind == RestKind::short_rest && elapsed >= 60 * minute)
                continue;
            auto party = wounded();
            (void)party.begin_rest(kind);
            advance(party, elapsed);
            const auto rng = party.state().random_state;
            const auto clock = party.state().time_minutes;
            const bool ready = party.prepare_combat();
            check(ready == (kind == RestKind::short_rest || elapsed < 60 * minute),
                  "Initiative waits only for earned recovery choices");
            check(party.state().random_state == rng && party.state().time_minutes == clock,
                  "Preparing initiative does not reroll or advance time");
            const auto prepared = saved(party);
            check(party.prepare_combat() == ready && saved(party) == prepared,
                  "Repeated encounter polling does not extend rest or repeat recharge");
            if (!ready)
            {
                rejects(
                    [&]
                {
                    party.begin_combat();
                });
                check(saved(party) == prepared, "Combat cannot skip pending Hit Dice");
                party = loaded(prepared);
                (void)party.spend_hit_die(party.state().short_rest->ticket, 1);
                finish_spending(party);
            }
            check(party.prepare_combat(), "Resolved interruption is ready for combat");
            party.begin_combat();
            party.end_combat();
        }
    // Actual ECL HP readback, including a zero-HP victim and rejected wealth writes.
    for (auto kind :
            {
                RestKind::short_rest, RestKind::long_rest
            })
        for (int hp :
                {
                    0, 1
                })
        {
            CampaignParty party(module());
            party.add_pc(hero());
            auto state = party.checkpoint();
            state.roster[0].vitals.hit_points = 5;
            party.restore(state);
            (void)party.begin_rest(kind);
            advance(party, kind == RestKind::short_rest ? minute : 70 * minute);
            por::EclMachine vm(program({0}));
            for (const auto &write : party.character_reply(0).writes)
                vm.bind_variable(write.address, write.value);
            vm.bind_variable(0x6C19, hp);
            const auto rng = party.state().random_state;
            party.read_character(0, vm);
            check(party.member(1).vitals.hit_points == hp && party.state().random_state == rng,
                  "Script damage commits exact HP without an extra RNG draw");
            check(bool(party.state().rest_activity) == (kind == RestKind::long_rest),
                  "Script damage cancels Short Rest and retains interrupted Long Rest");
            check(bool(party.state().short_rest) == (kind == RestKind::long_rest && hp > 0),
                  "Damage victim must remain eligible to earn recovery");
            const auto damaged = saved(party);
            party.read_character(0, vm);
            check(saved(party) == damaged, "Repeated HP readback cannot duplicate interruption");
            party = loaded(damaged);
            check(saved(party) == damaged, "Automatic damage interruption reloads exactly");
        }
    auto party = wounded();
    (void)party.begin_rest(RestKind::long_rest);
    advance(party, 70 * minute);
    auto state = party.checkpoint();
    state.rest_activity->ticket.revision = std::numeric_limits<std::uint64_t>::max();
    party.restore(state);
    const auto before = saved(party);
    rejects(
        [&]
    {
        (void)party.prepare_combat();
    });
    check(saved(party) == before, "Failed initiative preparation rolls back recovery and tickets");
}

void run()
{
    host_interruptions();
    prior_writer();
    segments();
    boundaries();
    combat_and_validation();
    discard_and_bad_saves();
}
} // namespace rest_activity_checks
