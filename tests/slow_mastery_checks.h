// Slow's shared lifecycle and consumers, before optional hit-choice integration.
namespace slow_mastery_checks
{
using namespace mastery_combat_checks;

fx::EffectState slow(bool frost = false)
{
    fx::EffectState e;
    fx::apply_attack_mastery(e, fx::EffectKind::slow, 777, 99, "Master", 6000);
    if (frost)
        fx::apply_ray_of_frost(e, 777, 98, "Caster", 4000);
    return e;
}

void lifecycle()
{
    auto e = slow(true);
    fx::apply_attack_mastery(e, fx::EffectKind::slow, 777, 98, "Other master", 5000);
    fx::apply_ray_of_frost(e, 777, 99, "Other caster", 3000);
    const auto next = e.next_id;
    fx::apply_attack_mastery(e, fx::EffectKind::slow, 777, 99, "Master refreshed", 6000);
    check(e.next_id == next && e.active.size() == 4 && fx::speed_penalty(e) == 20,
          "Slow refreshes its source; repeated Slow/Frost do not stack, distinct effects combine");
    std::ostringstream out;
    fx::write_effects(out, e);
    check(out.str().starts_with("FX8 "), "Slow uses the current FX8 tag");
    std::istringstream in(out.str());
    check(fx::read_effects(in) == e, "All Slow sources and clocks round trip");
    auto forged = out.str();
    forged.replace(0, 3, "FX6");
    rejects(
        [&]
    {
        std::istringstream bytes(forged);
        (void)fx::read_effects(bytes);
    });
    auto whole = e;
    std::uint64_t rng = 41, whole_rng = rng;
    std::array<fx::EffectSubject, 1> subjects{{{1, e, {}}}}, whole_subjects{{{1, whole, {}}}};
    fx::elapse_effects(subjects, 3999, rng);
    check(fx::speed_penalty(e) == 20, "One frost source survives until its exact boundary");
    fx::elapse_effects(subjects, 1, rng);
    check(fx::speed_penalty(e) == 10 && !fx::frosted(e), "Frost expiry retains independent Slow");
    fx::elapse_effects(subjects, 1000, rng);
    check(fx::speed_penalty(e) == 10 && e.active.size() == 1,
          "First Slow expiry retains the later source without stacking");
    fx::elapse_effects(subjects, 999, rng);
    check(fx::slowed(e), "Slow survives one millisecond before expiry");
    fx::elapse_effects(subjects, 1, rng);
    fx::elapse_effects(whole_subjects, 6000, whole_rng);
    check(e == whole && rng == 41 && rng == whole_rng && !fx::speed_penalty(e),
          "Chunked Slow expiry is exact and consumes no RNG");
    auto full = slow();
    while (full.active.size() < fx::effect_limit)
        fx::apply_attack_mastery(full, fx::EffectKind::slow, 777, static_cast<rules::EntityId>(full.active.size() + 100),
                                 "Source", 6000);
    check(!fx::can_apply_attack_mastery(full, fx::EffectKind::slow, 777, 999) &&
          fx::can_apply_attack_mastery(full, fx::EffectKind::slow, 777, 99),
          "Full store permits same-source Slow refresh only");
    fx::apply_attack_mastery(full, fx::EffectKind::slow, 777, 99, "Master", 3000);
    check(full.active.size() == fx::effect_limit, "Refresh does not consume storage");
    for (unsigned duration :
            {
                0u, 6001u
            })
    {
        auto before = e;
        rejects(
            [&]
        {
            fx::apply_attack_mastery(e, fx::EffectKind::slow, 777, 99, "Master", duration);
        });
        check(e == before, "Malformed Slow duration rejects atomically");
    }
    for (const auto bytes :
            {"FX8 2 1 1 7 777 99 \"Master\" 1 6000 0 0",
             "FX8 2 1 1 7 777 99 \"Master\" 0 6001 0 0",
             "FX8 2 1 1 7 777 99 \"Master\" 0 6000 1 0"
            })
        rejects(
            [&]
    {
        std::istringstream input(bytes);
        (void)fx::read_effects(input);
    });
}

void consumers()
{
    auto p = nick_attack_checks::party("dagger", false, false, false, "rogue");
    p.award_experience(900, "slow-consumers");
    while (p.member(1).character.sheet().level < 3)
        p.advance(1, p.default_advancement(1));
    auto actors = p.participants();
    actors.front().cell = {1, 1};
    actors.push_back({99, "mastery_target", "Target", Side::opposition, {5, 1}});
    const auto make = [&](fx::EffectState e)
    {
        auto roster = actors;
        auto base =
        p.rule_module().create({{12, 8, std::vector<Terrain>(96)}, roster, 777}, 1);
        inject(roster[0], unit(*base, 1).persistent, e);
        auto c = p.rule_module().create({{12, 8, std::vector<Terrain>(96)}, roster, 777}, 1);
        turn(*c, 1);
        return c;
    };
    for (bool frost :
            {
                false, true
            })
    {
        auto c = make(slow(frost));
        const int speed = frost ? 10 : 20;
        check(unit(*c, 1).movement_feet == speed, "Slow participates in live combat Speed");
        const auto statuses = unit(*c, 1).status_messages;
        check(std::any_of(statuses.begin(), statuses.end(),
                          [](const auto & m)
        {
            return m.source.starts_with("Slow (");
        }),
        "Slow status identifies its source");
        check(std::any_of(statuses.begin(), statuses.end(),
                          [](const auto & m)
        {
            return m.source.starts_with("Ray of Frost:");
        }) == frost,
        "Slow is never mislabeled Ray of Frost");
        act(*c, "dash");
        check(unit(*c, 1).movement_feet == speed * 2, "Action Dash gains reduced Speed");
        act(*c, "cunning_dash");
        check(unit(*c, 1).movement_feet == speed * 3,
              "Bonus Dash gains reduced Speed independently");
        check(p.rule_module().restore(c->save())->save() == c->save(),
              "Slow and two Dash budgets restore exactly");
        const auto current = c->save();
        auto old = current;
        old.replace(old.find(p.identity().version), p.identity().version.size(), "0.6.57");
        rejects(
            [&]
        {
            (void)p.rule_module().restore(old);
        });
        auto e = slow(frost);
        e.prone = true;
        c = make(e);
        act(*c, "stand_up");
        check(unit(*c, 1).movement_feet == speed / 2,
              "Standing costs half the combined reduced Speed");
        c = make(slow(frost));
        act(*c, "steady_aim");
        check(unit(*c, 1).movement_feet == 0 && !offers(*c, "move"),
              "Steady Aim keeps Speed zero under Slow");
    }
    auto c = make(slow(true));
    auto state = p.checkpoint();
    state.roster[0].vitals = unit(*c, 1).persistent;
    p.restore(state);
    const auto encoded = encode_campaign(p, nullptr, "slow-consumers");
    auto decoded = decode_campaign(encoded, *srd5::character_rules(), p.rule_module(),
                                   "slow-consumers", nullptr);
    CampaignParty copy(rules());
    copy.restore(decoded.party);
    check(encode_campaign(copy, nullptr, "slow-consumers") == encoded,
          "Campaign Slow continuation retains exact resources and clocks");
    p.advance_time(std::chrono::milliseconds(6000));
    for (unsigned n = 0; n < 6; ++n)
        copy.advance_time(std::chrono::milliseconds(1000));
    check(encode_campaign(copy, nullptr, "slow-consumers") ==
          encode_campaign(p, nullptr, "slow-consumers"),
          "Campaign chunking preserves Slow/Frost expiry and random state");
}

void run()
{
    lifecycle();
    consumers();
}
} // namespace slow_mastery_checks
