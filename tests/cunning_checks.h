// Included by training_tests.cpp to reuse normal creation and prior-writer helpers.
namespace cunning_checks
{
Command command(const CombatSession &c, std::string_view verb)
{
    for (const auto &v : c.legal_commands())
        if (v.verb == verb)
            return v;
    throw std::runtime_error("Missing command: " + std::string(verb));
}

bool has(const CombatSession &c, std::string_view verb)
{
    for (const auto &v : c.legal_commands())
        if (v.verb == verb)
            return true;
    return false;
}

void act(CombatSession &c, std::string_view verb)
{
    check(c.submit(command(c, verb)), "Cunning command accepted");
}

CombatantView unit(const CombatSession &c)
{
    for (const auto &a : c.snapshot().combatants)
        if (a.id == 1)
            return a;
    throw std::runtime_error("Missing Rogue");
}

auto battle(const Character &h)
{
    auto rules = module();
    auto c = rules->create(
    {
        {10, 8, std::vector<Terrain>(80)},
        {   {
                1,
                "campaign-character",
                "Rogue",
                Side::party,
                {1, 1},
                rules->character_profile(h.sheet(), std::array<std::string, 1>{"dagger"}).data
            },
            {99, "vanguard", "Enemy", Side::opposition, {2, 1}}
        }},
    2);
    test::keep_initiative(*c);
    while (c->snapshot().actor != 1)
        act(*c, "end");
    return c;
}

void run()
{
    auto rules = module();
    auto creation = srd5::character_rules();
    const auto output = std::filesystem::path(OPENGOLD_BINARY_DIR) / "cunning-fixtures";
    std::filesystem::create_directories(output);
    auto write = [&](std::string_view name, const CombatSession & c)
    {
        std::ofstream out(output / (std::string(name) + ".save"), std::ios::binary);
        out << c.save();
        check(bool(out), "Cunning UI fixture written");
    };
    for (const auto *race :
            {"human", "orc", "dwarf", "goliath"
            })
    {
        auto d = draft();
        d.race = race;
        d.training = choices();
        auto h = hero(d);
        auto c = battle(h);
        check(!has(*c, "cunning_dash") && unit(*c).bonus_actions.empty(),
              "Level one has no Cunning Action");
        write("level1", *c);
        const auto hp = h.sheet().hit_points;
        const auto grants = h.sheet().grants;
        VitalState state{hp - 2};
        check(h.advance(*rules, state), "Normal Rogue advancement reaches level two");
        check(h.sheet().level == 2 && h.sheet().hit_die == 8 &&
              h.sheet().hit_points == hp + 5 + h.sheet().modifiers[2] + (d.race == "dwarf") &&
              state.hit_points == h.sheet().hit_points - 2,
              "Independent d8 fixed-average growth preserves wounds");
        check(h.sheet().training.complete && h.sheet().hit_point_modifiers.size() == 2 &&
              std::equal(grants.begin(), grants.end(), h.sheet().grants.begin()),
              "Advancement preserves training and grants with Con history");
        check(std::find(h.sheet().grants.begin(), h.sheet().grants.end(),
                        FeatureGrant{"feature:cunning_action", "class:rogue", 2, {}}) !=
              h.sheet().grants.end(),
              "Sourced level-two grant");
        check(rules->advancement_options(h.sheet()).level == 3,
              "Rogue level three is now available through normal advancement");
        auto bad = h.sheet();
        std::erase_if(bad.grants,
                      [](const auto & g)
        {
            return g.id == "feature:cunning_action";
        });
        rejects(
            [&]
        {
            (void)rules->character_profile(bad, {});
        });
        for (bool bonus_first :
                {
                    false, true
                })
        {
            c = battle(h);
            const int speed = unit(*c).movement_feet;
            write("available", *c);
            act(*c, bonus_first ? "cunning_dash" : "dash");
            check(unit(*c).action == bonus_first && unit(*c).bonus_action != bonus_first &&
                  unit(*c).movement_feet == speed * 2,
                  "Dash spends exactly its chosen budget");
            auto copy = rules->restore(c->save());
            act(*c, bonus_first ? "dash" : "cunning_dash");
            act(*copy, bonus_first ? "dash" : "cunning_dash");
            check(c->save() == copy->save() && unit(*c).movement_feet == speed * 3,
                  "Both Dash orders combine and restore deterministically");
            check(!unit(*c).action && !unit(*c).bonus_action && !has(*c, "cunning_dash") &&
                  !has(*c, "adrenaline_rush"),
                  "Shared Bonus Action cannot be reused");
            write("spent", *c);
            const auto saved = c->save();
            check(!c->submit({c->snapshot().revision, 1, 0, "cunning_dash"}) && c->save() == saved,
                  "Repeated bonus rejects atomically");
            act(*c, "end");
            write("offturn", *c);
            check(!has(*c, "cunning_dash"), "Off-turn Cunning Action unavailable");
            while (c->snapshot().actor != 1)
                act(*c, "end");
            check(has(*c, "cunning_dash") && unit(*c).movement_feet == speed,
                  "Cunning renews each turn without resting");
        }
        c = battle(h);
        act(*c, "cunning_disengage");
        check(unit(*c).action && !unit(*c).bonus_action, "Bonus Disengage retains ordinary action");
        auto move = [&](CombatSession & session, Cell cell)
        {
            for (const auto &v : session.legal_commands())
                if (v.verb == "move" && v.destination == cell)
                {
                    check(session.submit(v), "Move accepted");
                    return;
                }
            throw std::runtime_error("Move absent");
        };
        move(*c, {0, 1});
        check(!c->snapshot().reaction_pending, "Bonus Disengage prevents leaving-reach OA");
        check(rules->restore(c->save())->save() == c->save(),
              "Disengage and movement continuation round-trip");
        move(*c, {1, 1});
        act(*c, "end");
        while (c->snapshot().actor != 1)
            act(*c, "end");
        move(*c, {0, 1});
        check(c->snapshot().reaction_pending && !has(*c, "cunning_dash"),
              "Disengage expires on next turn; reaction window blocks Cunning");
        write("reaction", *c);
        auto copy = rules->restore(c->save());
        act(*c, "decline");
        act(*copy, "decline");
        check(c->save() == copy->save(), "Movement reaction resumes identically");
        if (d.race == "orc")
        {
            c = battle(h);
            act(*c, "adrenaline_rush");
            check(!has(*c, "cunning_dash") && unit(*c).action,
                  "Adrenaline Rush competes for same Bonus Action");
            act(*c, "end");
            while (c->snapshot().actor != 1)
                act(*c, "end");
            act(*c, "adrenaline_rush");
            check(c->snapshot().temporary_hp_offer && !has(*c, "cunning_dash"),
                  "Temporary HP decision blocks other actions");
            write("decision", *c);
        }
    }
    auto soldier_draft = draft("rogue", "soldier");
    soldier_draft.training = choices();
    soldier_draft.training["class:rogue:expertise"] = {"investigation", "perception"};
    auto soldier = hero(soldier_draft);
    VitalState soldier_state;
    check(soldier.advance(*rules, soldier_state), "Soldier Rogue advances normally");
    auto hit = battle(soldier);
    act(*hit, "melee");
    check(has(*hit, "cunning_dash"),
          "Automatic Savage damage leaves the unspent Bonus Action for Cunning Action");
    for (const auto &resource : rules->recovery_info(soldier.sheet(), soldier_state).resources)
        check(resource.id != "cunning_action", "Cunning Action is not a rest-use pool");
    // A real Ray of Frost hit reduces every Dash allowance, including the new one.
    auto d = draft();
    d.training = choices();
    auto rogue = hero(d);
    VitalState vitals;
    check(rogue.advance(*rules, vitals), "Slow fixture advances normally");
    auto wizard_draft = draft("wizard", "sage");
    wizard_draft.cantrips = std::vector<std::string> {"ray_of_frost"};
    auto wizard = hero(wizard_draft);
    auto slow = rules->create({{10, 8, std::vector<Terrain>(80)},
        {   {
                1,
                "campaign-character",
                "Rogue",
                Side::party,
                {1, 1},
                rules->character_profile(rogue.sheet(), {}).data
            },
            {
                99,
                "campaign-character",
                "Wizard",
                Side::opposition,
                {5, 1},
                rules->character_profile(wizard.sheet(), {}).data
            }
        }},
    2);
    test::keep_initiative(*slow);
    while (slow->snapshot().actor != 99)
        act(*slow, "end");
    act(*slow, "ray_of_frost");
    act(*slow, "end");
    check(unit(*slow).movement_feet == 20, "Actual Frost hit reduces Rogue Speed to twenty");
    act(*slow, "cunning_dash");
    check(unit(*slow).movement_feet == 40, "Bonus Dash grants reduced Speed");
    act(*slow, "dash");
    check(unit(*slow).movement_feet == 60 && rules->restore(slow->save())->save() == slow->save(),
          "Both slowed Dash allowances persist");
    for (bool dead :
            {
                false, true
            })
    {
        auto down = rules->create({{8, 8, std::vector<Terrain>(64)},
            {   {
                    1,
                    "campaign-character",
                    "Down Rogue",
                    Side::party,
                    {1, 1},
                    rules->character_profile(rogue.sheet(), {}).data,
                    VitalState{0, dead, {}}
                },
                {99, "vanguard", "Enemy", Side::opposition, {5, 5}}
            }},
        2);
        check(!has(*down, "cunning_dash"), "Unconscious/dead actors cannot use Cunning Action");
        const auto saved = down->save();
        check(!down->submit({down->snapshot().revision, 1, 0, "cunning_dash"}) &&
              saved == down->save(),
              "Incapacitated attempt rejects atomically");
    }
    CampaignParty party(module());
    for (const auto *background :
            {"sage", "criminal", "acolyte", "soldier"
            })
    {
        auto rogue_draft = draft("rogue", background);
        rogue_draft.race = "orc";
        rogue_draft.training = choices();
        rogue_draft.training["class:rogue:expertise"] = {"investigation", "perception"};
        party.add_pc(hero(rogue_draft));
    }
    auto wounded = party.checkpoint();
    for (auto &member : wounded.roster)
        member.vitals.hit_points -= 2;
    party.restore(std::move(wounded));
    check(rules->experience_for_level(2) == 300, "Independent level-two XP threshold");
    rejects(
        [&]
    {
        party.advance(1, party.default_advancement(1));
    });
    party.award_experience(1200, "cunning-xp");
    for (MemberId id = 1; id <= 4; ++id)
    {
        party.advance(id, party.default_advancement(id));
        check(party.member(id).character.sheet().level == 2 &&
              party.member(id).vitals.hit_points ==
              party.member(id).character.sheet().hit_points - 2,
              "Ordinary campaign advancement preserves wounds");
    }
    const auto bytes = encode_campaign(party, nullptr, "cunning");
    CampaignParty copy(module());
    copy.restore(decode_campaign(bytes, *creation, *rules, "cunning", nullptr).party);
    check(encode_campaign(copy, nullptr, "cunning") == bytes, "Advanced campaign replay exact");
    check(bool(copy.rest(RestKind::short_rest)), "Rogue short rest valid");
    auto rest = copy.state().short_rest;
    check(bool(rest), "Rest ticket exists");
    copy.finish_short_rest(rest->ticket);
    check(bool(copy.rest(RestKind::long_rest)), "Rogue long rest valid");
}
} // namespace cunning_checks
