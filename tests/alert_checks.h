// Alert acceptance and actual pre-Alert writer fixtures.
namespace alert_checks
{
CampaignParty party()
{
    CampaignParty p(module());
    for (const auto *klass :
            {"fighter", "rogue"
            })
    {
        auto d = draft(klass, "criminal");
        if (d.character_class == "rogue")
            d.training = choices();
        else
            for (const auto &g : srd5::character_rules()->training_options(d))
                for (unsigned i = 0; i < g.count; ++i)
                    d.training[g.id].push_back(g.options.at(i).id);
        auto h = hero(d);
        h.inventory().add("dagger", "Dagger", 2);
        const auto id = p.add_pc(std::move(h));
        p.equip(id, 1);
    }
    p.award_experience(2700, "alert-baseline");
    for (unsigned id :
            {
                1, 2
            })
        for (unsigned level = 2; level <= 4; ++level)
            p.advance(id, p.default_advancement(id));
    auto state = p.checkpoint();
    for (auto &m : state.roster)
        m.vitals.hit_points -= 2;
    p.restore(std::move(state));
    return p;
}

auto battle(CampaignParty &p, unsigned seed = 37)
{
    auto actors = p.participants();
    actors[0].cell = {1, 1};
    actors[1].cell = {2, 1};
    actors.push_back({99, "vanguard", "Enemy", 1, {8, 6}});
    return p.rule_module().create({{12, 8, std::vector<std::uint8_t>(96)}, actors}, seed);
}

void write(const char *name, const std::string &bytes)
{
    std::ofstream out(std::filesystem::path(OPENGOLD_SOURCE_DIR) / "tests/fixtures" / name,
                      std::ios::binary);
    out << bytes;
    check(bool(out), "Write Alert prior writer fixture");
}

void freeze()
{
    auto p = party();
    check(p.rule_module().identity().version == "0.6.60",
          "Alert capture requires actual0.6.60 writer");
    write("campaign-alert-0.6.60.ogs", encode_campaign(p, nullptr, "alert-before"));
    auto c = battle(p);
    write("combat-alert-0.6.60.save", c->save());
    light_attack_checks::act(*c, "end");
    write("combat-alert-0.6.60-continued.save", c->save());
}

Command command(const CombatSession &c, std::string_view verb, unsigned owner, unsigned ally = 0)
{
    for (const auto &cmd : c.legal_commands())
        if (cmd.verb == verb && cmd.actor == owner && cmd.target == ally)
            return cmd;
    throw std::runtime_error("Missing Alert command: " + std::string(verb));
}

void decide(CombatSession &c, unsigned owner, unsigned ally = 0)
{
    check(c.submit(command(c, ally ? "initiative_swap" : "initiative_keep", owner, ally)),
          "Execute Alert decision");
}

void decline(CombatSession &c)
{
    while (!c.snapshot().initiative_choices.empty())
        decide(c, c.snapshot().initiative_choices.front());
}

CombatantView unit(const CombatSession &c, unsigned id)
{
    for (const auto &a : c.snapshot().combatants)
        if (a.id == id)
            return a;
    throw std::runtime_error("Missing Alert actor");
}

void exact(const CombatSession &c)
{
    check(module()->restore(c.save())->save() == c.save(), "Alert exact pending continuation");
}

void baseline()
{
    auto rules = module();
    const auto normalize = [&](std::string s)
    {
        replace(s, "0.6.60", rules->identity().version);
        return s;
    };
    auto c = rules->restore(fixture("combat-alert-0.6.60.save"));
    check(c->snapshot().initiative_choices.empty() &&
          c->save() == normalize(fixture("combat-alert-0.6.60.save")),
          "Old combat retains exact rolls and no reopened initiative choices");
    light_attack_checks::act(*c, "end");
    check(c->save() == normalize(fixture("combat-alert-0.6.60-continued.save")),
          "Old combat retains exact deterministic continuation");
    CampaignParty restored(module());
    restored.restore(decode_campaign(fixture("campaign-alert-0.6.60.ogs"), *srd5::character_rules(),
                                     *rules, "alert-before", nullptr)
                     .party);
    auto expected = party();
    check(
        encode_campaign(restored, nullptr, "alert-before") ==
        encode_campaign(expected, nullptr, "alert-before"),
        "Campaign migration adds only fixed Alert, preserving whole character/equipment/resource state");
    const auto saved = encode_campaign(restored, nullptr, "alert-before");
    restored.restore(
        decode_campaign(saved, *srd5::character_rules(), *rules, "alert-before", nullptr).party);
    check(encode_campaign(restored, nullptr, "alert-before") == saved,
          "New campaign reload is canonical");
}

void grants()
{
    auto rules = module();
    auto creation = srd5::character_rules();
    for (const auto &klass : creation->choices(CreationField::character_class))
    {
        const auto h = hero(draft(klass.id, "criminal"));
        const auto &g = h.sheet().grants;
        check(std::count(g.begin(), g.end(),
                         FeatureGrant{"feat:alert", "background:criminal", 1, {}}) == 1,
              "Every class Criminal receives one sourced Alert feat");
        (void)rules->character_profile(h.sheet(), {});
    }
    for (const auto klass :
            {"fighter", "rogue", "cleric", "wizard", "paladin", "ranger"
            })
        for (bool criminal :
                {
                    false, true
                })
        {
            auto d = draft(klass, criminal ? "criminal" : "sage");
            if (d.character_class == "rogue")
            {
                d.training = choices();
                d.training["class:rogue:expertise"] = {"investigation", "perception"};
            }
            for (const auto &group : creation->training_options(d))
                if (!d.training.contains(group.id))
                    for (unsigned i = 0; i < group.count; ++i)
                        d.training[group.id].push_back(group.options.at(i).id);
            CampaignParty p(module());
            const auto id = p.add_pc(hero(d));
            p.award_experience(2700, "alert-feat");
            for (unsigned level = 2; level <= 3; ++level)
                p.advance(id, p.default_advancement(id));
            const auto options = rules->advancement_options(p.member(id).character.sheet());
            const auto found = std::find_if(options.feats.begin(), options.feats.end(),
                                            [](const auto & f)
            {
                return f.id == "alert";
            });
            check(found != options.feats.end() && found->available != criminal,
                  "All current level-four routes enforce nonrepeatable Alert");
            auto choice = p.default_advancement(id);
            choice.feat = "alert";
            choice.abilities = {};
            if (criminal)
            {
                const auto old = encode_campaign(p, nullptr, "alert");
                rejects(
                    [&]
                {
                    p.advance(id, choice);
                });
                check(encode_campaign(p, nullptr, "alert") == old,
                      "Duplicate Alert rejects atomically");
            }
            else
            {
                p.advance(id, choice);
                const auto &g = p.member(id).character.sheet().grants;
                check(std::find(
                          g.begin(), g.end(),
                          FeatureGrant{"feat:alert",
                                       std::string("class:") + klass + ":ability_score_improvement",
                                       4,
                                       {}}) != g.end(),
                      "Alert retains actual feat entitlement");
            }
        }
    auto invalid = hero(draft("fighter", "criminal")).sheet();
    invalid.grants.push_back({"feat:alert", "background:criminal", 1, {}});
    rejects(
        [&]
    {
        (void)rules->character_profile(invalid, {});
    });
}

void run()
{
    baseline();
    grants();
    auto p = party();
    auto c = battle(p);
    const auto before = c->snapshot();
    auto old = module()->restore(fixture("combat-alert-0.6.60.save"));
    for (unsigned id :
            {
                1, 2
            })
        check(unit(*c, id).initiative == unit(*old, id).initiative + 2,
              "Actual proficiency adds once, including Champion Advantage");
    check(before.initiative_choices.size() == 2 && before.elapsed_milliseconds == 0,
          "All Alert holders choose before time passes");
    exact(*c);
    for (const auto *tail :
            {"2 1 1\n", "1 99\n", "0\n"
            })
    {
        auto bad = c->save();
        bad.replace(bad.rfind('\n', bad.size() - 2) + 1, std::string::npos, tail);
        rejects(
            [&]
        {
            (void)module()->restore(bad);
        });
    }
    const auto stale = command(*c, "initiative_keep", 2);
    auto illegal = command(*c, "initiative_swap", 1, 2);
    illegal.target = 99;
    auto bytes = c->save();
    check(!c->submit(illegal) && c->save() == bytes, "Enemy target rejects atomically");
    illegal.target = 1;
    check(!c->submit(illegal) && c->save() == bytes, "Self target rejects atomically");
    auto normal = command(*c, "initiative_keep", 1);
    normal.verb = "end";
    check(!c->submit(normal) && c->save() == bytes, "Combat actions wait for initiative");
    decide(*c, 1, 2);
    check(unit(*c, 1).initiative == unit(*old, 2).initiative + 2 &&
          unit(*c, 2).initiative == unit(*old, 1).initiative + 2,
          "Swap exchanges complete current totals without rerolling");
    check(c->snapshot().initiative_choices == std::vector<EntityId> {2},
          "Swap does not consume ally's own decision");
    exact(*c);
    bytes = c->save();
    check(!c->submit(stale) && c->save() == bytes, "Stale decision rejects atomically");
    decide(*c, 2, 1);
    check(c->snapshot().initiative_choices.empty() &&
          unit(*c, 1).initiative == unit(*old, 1).initiative + 2,
          "Later holder can exchange updated totals");
    exact(*c);
    for (unsigned id :
            {
                1, 2
            })
        check(unit(*c, id).action && unit(*c, id).bonus_action && unit(*c, id).reaction &&
              unit(*c, id).hit_points == unit(*old, id).hit_points,
              "Choice spends no action, reaction or HP");
    // Decisions can be resolved in either order, including choosing Keep first.
    c = battle(p);
    decide(*c, 2);
    exact(*c);
    decide(*c, 1);
    exact(*c);
    auto actors = p.participants();
    actors[0].cell = {1, 1};
    actors[1].cell = {2, 1};
    actors.push_back({99, "vanguard", "Enemy", 1, {8, 6}});
    actors[1].state->hit_points = 0;
    auto helpless = module()->create(
    { {12, 8, std::vector<std::uint8_t>(96)}, actors
    }, 37);
    check(helpless->snapshot().initiative_choices == std::vector<EntityId> {1},
          "Incapacitated holder cannot swap");
    check(helpless->legal_commands().size() == 1 &&
          helpless->legal_commands()[0].verb == "initiative_keep",
          "Incapacitated ally excluded");
    check(unit(*helpless, 2).initiative == unit(*c, 2).initiative,
          "Incapacitation preserves Initiative proficiency");
    exact(*helpless);
    decline(*helpless);
    exact(*helpless);
    // Both enemy holders automatically keep; only the party decision remains.
    actors = p.participants();
    actors[0].cell = {1, 1};
    actors[1].cell = {8, 6};
    actors[1].side = 1;
    auto enemy = module()->create(
    { {12, 8, std::vector<std::uint8_t>(96)}, actors
    }, 37);
    check(enemy->snapshot().initiative_choices == std::vector<EntityId> {1},
          "Enemy holder keeps its roll automatically");
    decline(*enemy);
    // Conditional disadvantage combines with the Champion's existing Advantage.
    actors = p.participants();
    actors[0].cell = {1, 1};
    actors[0].surprised = true;
    actors[1].cell = {2, 1};
    actors.push_back({99, "vanguard", "Enemy", 1, {8, 6}});
    auto surprised = module()->create(
    { {12, 8, std::vector<std::uint8_t>(96)}, actors
    }, 37);
    exact(*surprised);
    std::uint64_t dice_state = 37;
    const int roll = style_route_checks::die(dice_state, 20);
    check(unit(*surprised, 1).initiative == roll + p.member(1).character.sheet().modifiers[1] + 2,
          "Advantage and Disadvantage cancel before adding Alert once");
    decline(*surprised);
    auto sleeping_actors = actors;
    module()->set_rest_work(*sleeping_actors[1].state, p.member(2).character.sheet(),
                            RestWork::sleep);
    auto sleeping = module()->create(
    { {12, 8, std::vector<std::uint8_t>(96)}, sleeping_actors
    }, 37);
    check(sleeping->snapshot().initiative_choices == std::vector<EntityId> {1} &&
          sleeping->legal_commands().size() == 1,
          "Naturally sleeping holders and allies cannot swap");
    exact(*sleeping);
    decline(*sleeping);
    bool down_first = false;
    actors = p.participants();
    actors[0].cell = {1, 1};
    actors[1].cell = {2, 1};
    actors[1].state->hit_points = 0;
    actors.push_back({99, "vanguard", "Enemy", 1, {8, 6}});
    for (unsigned seed = 1; seed <= 32; ++seed)
    {
        auto pending = module()->create(
        { {12, 8, std::vector<std::uint8_t>(96)}, actors
        }, seed);
        exact(*pending);
        const auto state = pending->snapshot();
        if (state.combatants.front().id == 2)
        {
            down_first = true;
            check(unit(*pending, 2).hit_points == 0, "No recovery before opening choice");
            for (const auto &line : state.log)
                check(line.find("death save") == line.npos,
                      "First-turn processing waits for choice");
        }
        decline(*pending);
        exact(*pending);
    }
    check(down_first, "Tests cover an incapacitated first Initiative slot");
    const auto dir = std::filesystem::path(OPENGOLD_BINARY_DIR) / "alert-fixtures";
    std::filesystem::create_directories(dir);
    const auto save = [&](const char *name, const CombatSession & battle)
    {
        std::ofstream out(dir / (std::string(name) + ".save"), std::ios::binary);
        out << battle.save();
        check(bool(out), "Write Alert UI oracle");
    };
    c = battle(p, 13);
    check(unit(*c, 1).initiative != unit(*c, 2).initiative, "UI swaps different totals");
    save("pending", *c);
    decide(*c, 1, 2);
    save("swapped", *c);
    decide(*c, 2);
    save("done", *c);
    c = battle(p, 13);
    decide(*c, 2);
    save("second-kept", *c);
    decide(*c, 1);
    save("kept", *c);
}
} // namespace alert_checks
