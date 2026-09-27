namespace nick_attack_checks
{
using namespace mastery_combat_checks;

void capture()
{
    const auto output = std::getenv("OPENGOLD_NICK_BASELINE");
    if (!output)
        return;
    auto r = module();
    check(r->identity().version == "0.6.56", "Freeze only actual pre-Nick production writer");
    CampaignParty p(module());
    auto h = hero("handaxe", "fighter", "soldier");
    h.inventory().add("dagger", "First dagger");
    h.inventory().add("dagger", "Second dagger");
    const auto id = p.add_pc(h);
    p.equip(id, 1);
    p.equip(id, 2, EquipmentOperation::equip_other);
    p.award_experience(300, "nick-before");
    p.advance(id, p.default_advancement(id));
    auto saved = p.checkpoint();
    saved.roster[0].vitals.hit_points -= 3;
    p.restore(saved);
    auto actors = p.participants();
    actors.front().cell = {1, 1};
    actors.push_back({99, "vanguard", "Target", 1, {2, 1}});
    const auto path = std::filesystem::path(output);
    std::filesystem::create_directories(path);
    const auto write = [&](const char *name, const CombatSession & c)
    {
        std::ofstream out(path / name);
        out << c.save();
        check(bool(out), "Write actual pre-Nick checkpoint");
    };
    for (unsigned seed = 1; seed < 400; ++seed)
    {
        auto c = r->create({{12, 8, std::vector<std::uint8_t>(96)}, actors, 777}, seed);
        turn(*c, id);
        const auto before = c->save();
        act(*c, "melee", 99);
        settle(*c);
        const auto qualified = c->save();
        act(*c, "light_melee", 99);
        if (!c->snapshot().savage_attack_choice)
            continue;
        write("combat-v22-nick-light-pending.save", *c);
        act(*c, "savage_skip");
        write("combat-v22-nick-light-spent.save", *c);
        c = r->restore(before);
        write("combat-v22-nick-before.save", *c);
        act(*c, "second_wind");
        act(*c, "melee", 99);
        settle(*c);
        write("combat-v22-nick-other-bonus-spent.save", *c);
        std::cout << "Captured actual pre-Nick writer at seed " << seed << '\n';
        return;
    }
    throw std::runtime_error("No pre-Nick pending Light fixture");
}

CampaignParty party(std::string weapon = "dagger", bool style = false, bool negative = false,
                    bool npc = false, std::string klass = "fighter")
{
    auto draft = hero("handaxe", klass, "soldier").creation_data();
    draft.training["class:" + klass + ":weapon_mastery"] = {weapon, "handaxe"};
    if (klass == "fighter")
    {
        draft.training["class:" + klass + ":weapon_mastery"].push_back("club");
        draft.training["class:fighter:fighting_style"] = {style ? "two_weapon_fighting"
                                                          : "defense"
                                                         };
    }
    if (negative)
        for (auto &roll : draft.rolls)
            roll = {{2, 2, 1, 1}, 3};
    Character h(*srd5::character_rules(), draft, {});
    h.inventory().add(weapon, "First weapon");
    h.inventory().add(weapon, "Second weapon");
    CampaignParty p(rules());
    const auto id = npc ? p.recruit("nick:npc", h) : p.add_pc(h);
    p.equip(id, 1);
    p.equip(id, 2, EquipmentOperation::equip_other);
    if (klass == "fighter")
    {
        p.award_experience(300, "nick-check");
        p.advance(id, p.default_advancement(id));
    }
    return p;
}

auto battle(CampaignParty &p, unsigned seed = 1)
{
    auto actors = p.participants();
    actors.front().cell = {1, 1};
    actors.push_back({99, "mastery_target", "Target", 1, {2, 1}});
    auto c = p.rule_module().create({{12, 8, std::vector<std::uint8_t>(96)}, actors, 777}, seed);
    turn(*c, 1);
    return c;
}

void round_trip(const RulesModule &r, CombatSession &c)
{
    auto copy = r.restore(c.save());
    check(copy->save() == c.save(), "Nick checkpoint canonical");
}

void grants_and_budgets()
{
    unsigned hits = 0, criticals = 0;
    for (const auto weapon :
            {"dagger", "light_hammer", "sickle", "scimitar"
            })
        for (bool style :
                {
                    false, true
                })
            for (bool negative :
                    {
                        false, true
                    })
                for (unsigned seed = 1; seed <= 48; ++seed)
                {
                    auto p = party(weapon, style, negative, seed % 2);
                    auto c = battle(p, seed);
                    check(
                        c->save().starts_with("OGCOMBAT 23 ") && unit(*c, 1).nick_mastery &&
                        !offers(*c, "nick_melee"),
                        "Chosen Nick creates explicit shared budget, not an attack before qualification");
                    act(*c, "melee", 99);
                    settle(*c);
                    const auto extra = command(*c, "nick_melee", 99);
                    check(extra.item == 2,
                          "Nick uses a different physical weapon of the same kind");
                    check(unit(*c, 1).bonus_action && !unit(*c, 1).action,
                          "Qualifying Attack leaves Bonus Action");
                    auto copy = p.rule_module().restore(c->save());
                    check(c->submit(extra) && copy->submit(extra), "Nick submits once");
                    if (c->snapshot().savage_attack_choice)
                    {
                        const auto hit = *c->snapshot().savage_attack_choice;
                        check(hit.modifier == (negative ? -2
                                               : style  ? 3
                                               : 0),
                              "Nick shares Light damage modifier and Two-Weapon Fighting");
                        ++hits;
                        if (hit.critical)
                            ++criticals;
                        check(p.rule_module().restore(c->save())->save() == c->save(),
                              "Nick pending damage restores with unspent Bonus Action");
                    }
                    settle(*c);
                    settle(*copy);
                    check(c->save() == copy->save(), "Nick hit continuation exact");
                    check(unit(*c, 1).bonus_action && !unit(*c, 1).action &&
                          !offers(*c, "light_melee") && !offers(*c, "nick_melee"),
                          "Nick leaves Bonus Action and uses the shared extra attack");
                    const auto before = c->save();
                    check(!c->submit(extra) && c->save() == before, "Stale Nick ticket is atomic");
                    act(*c, "action_surge");
                    act(*c, "melee", 99);
                    settle(*c);
                    check(!offers(*c, "light_melee") && !offers(*c, "nick_melee"),
                          "Surge grants no second Light/Nick extra attack");
                    act(*c, "end");
                    act(*c, "end");
                    act(*c, "melee", 99);
                    settle(*c);
                    check(offers(*c, "nick_melee"), "Fresh turn resets shared extra attack budget");
                }
    check(hits > 0 && criticals > 0,
          "Matrix actually exercises pending ordinary and critical damage");
    for (const auto klass :
            {"fighter", "rogue", "paladin", "ranger", "barbarian"
            })
        for (bool npc :
                {
                    false, true
                })
        {
            auto p = party("dagger", false, false, npc, klass);
            auto c = battle(p);
            act(*c, "melee", 99);
            settle(*c);
            act(*c, "nick_melee", 99);
            settle(*c);
            check(unit(*c, 1).bonus_action && !offers(*c, "light_melee"),
                  "All five class PC/NPC routes receive the same Nick rule");
        }
    auto p = party();
    auto c = battle(p);
    act(*c, "melee", 99);
    settle(*c);
    act(*c, "light_melee", 99);
    settle(*c);
    check(!unit(*c, 1).bonus_action && !offers(*c, "nick_melee"),
          "Choosing ordinary Light instead consumes the shared allowance");
    act(*c, "action_surge");
    act(*c, "melee", 99);
    settle(*c);
    check(!offers(*c, "nick_melee"), "Surge after ordinary Light cannot bypass its allowance");
    // An unrelated Bonus Action does not consume Nick.
    auto state = p.checkpoint();
    state.roster[0].vitals.hit_points -= 3;
    p.restore(state);
    c = battle(p);
    act(*c, "second_wind");
    act(*c, "melee", 99);
    settle(*c);
    check(!unit(*c, 1).bonus_action && offers(*c, "nick_melee"),
          "Nick works after Second Wind spent the Bonus Action");
    act(*c, "nick_melee", 99);
    round_trip(p.rule_module(), *c);
    settle(*c);
    check(!unit(*c, 1).bonus_action, "Nick never restores a previously spent Bonus Action");
    c = battle(p);
    act(*c, "melee", 99);
    settle(*c);
    act(*c, "action_surge");
    act(*c, "dash");
    check(!offers(*c, "nick_melee") && offers(*c, "light_melee"),
          "A later non-Attack Action closes Nick but retains the ordinary later-turn Light attack");
    round_trip(p.rule_module(), *c);
    c = battle(p);
    act(*c, "melee", 99);
    settle(*c);
    act(*c, "action_surge");
    for (const auto &command : c->legal_commands())
        if (command.verb == "weapon_select" && command.item == 2)
        {
            check(c->submit(command), "Select other weapon");
            break;
        }
    act(*c, "melee", 99);
    settle(*c);
    check(
        command(*c, "nick_melee", 99).item == 1,
        "Nick uses a different weapon from the current Attack action, not an earlier Surge action");
}

void pending_interactions()
{
    auto draft = hero("shortsword", "rogue", "soldier").creation_data();
    draft.training["class:rogue:weapon_mastery"] = {"shortsword", "scimitar"};
    Character rogue(*srd5::character_rules(), draft, {});
    rogue.inventory().add("shortsword", "Vex blade");
    rogue.inventory().add("scimitar", "Nick blade");
    CampaignParty p(rules());
    p.add_pc(rogue);
    p.equip(1, 1);
    p.equip(1, 2, EquipmentOperation::equip_other);
    p.award_experience(900, "nick-rogue");
    while (p.member(1).character.sheet().level < 3)
        p.advance(1, p.default_advancement(1));
    bool checked = false;
    for (unsigned seed = 1; seed <= 128 && !checked; ++seed)
    {
        auto c = battle(p, seed);
        act(*c, "steady_aim");
        act(*c, "melee", 99);
        settle(*c);
        if (!fx::vexed_by(state(*c, 99), 777, 1))
            continue;
        act(*c, "nick_melee", 99);
        if (!c->snapshot().sneak_attack_choice)
            continue;
        check(fx::vexed_by(state(*c, 99), 777, 1) && !unit(*c, 1).bonus_action,
              "Nick retains Vex for pending-roll validation and leaves spent Steady Aim unchanged");
        auto copy = p.rule_module().restore(c->save());
        check(copy->save() == c->save(), "Nick Sneak choice restores exactly");
        act(*c, "sneak_use");
        act(*copy, "sneak_use");
        check(c->save() == copy->save(), "Nick Sneak damage continuation is deterministic");
        const auto hit = *c->snapshot().savage_attack_choice;
        check(hit.modifier == 0 && hit.extra_damage > 0,
              "Nick omits positive weapon modifier while retaining Sneak dice");
        copy = p.rule_module().restore(c->save());
        act(*c, "savage_use");
        act(*copy, "savage_use");
        check(c->save() == copy->save() &&
              c->snapshot().savage_attack_choice->extra_damage == hit.extra_damage,
              "Savage rerolls only Nick weapon dice");
        copy = p.rule_module().restore(c->save());
        act(*c, "savage_second");
        act(*copy, "savage_second");
        check(
            c->save() == copy->save() && !fx::vexed_by(state(*c, 99), 777, 1) &&
            !offers(*c, "nick_melee") && !offers(*c, "light_melee"),
            "Resolved Nick consumes Vex and all damage choices preserve the shared spent allowance");
        checked = true;
    }
    check(checked, "Actual Nick hit exercises Sneak and both Savage stages");
    auto champion = party();
    champion.award_experience(600, "nick-champion");
    champion.advance(1, champion.default_advancement(1));
    checked = false;
    for (unsigned seed = 1; seed <= 128 && !checked; ++seed)
    {
        auto c = battle(champion, seed);
        act(*c, "melee", 99);
        settle(*c);
        act(*c, "nick_melee", 99);
        if (!c->snapshot().savage_attack_choice || !c->snapshot().savage_attack_choice->critical)
            continue;
        auto copy = champion.rule_module().restore(c->save());
        act(*c, "savage_skip");
        act(*copy, "savage_skip");
        check(c->save() == copy->save() && c->snapshot().free_movement,
              "Nick critical grants Champion movement after pending damage");
        copy = champion.rule_module().restore(c->save());
        act(*c, "end");
        act(*copy, "end");
        check(c->save() == copy->save() && unit(*c, 1).bonus_action && !offers(*c, "nick_melee") &&
              !offers(*c, "light_melee"),
              "Champion continuation keeps Bonus Action and spent Nick allowance");
        checked = true;
    }
    check(checked, "Actual Nick critical exercises Champion movement");
}

void throwing_and_provenance()
{
    for (const auto weapon :
            {"dagger", "light_hammer"
            })
    {
        auto p = party(weapon);
        auto c = battle(p);
        act(*c, "melee", 99);
        settle(*c);
        const auto extra = command(*c, "nick_throw", 99);
        auto copy = p.rule_module().restore(c->save());
        check(c->submit(extra) && copy->submit(extra),
              "Nick can throw a different physical weapon");
        round_trip(p.rule_module(), *c);
        settle(*c);
        settle(*copy);
        check(c->save() == copy->save() && unit(*c, 1).bonus_action && !offers(*c, "nick_throw"),
              "Thrown Nick keeps ground-item and shared budget continuation");
        const auto items = c->snapshot().held_items;
        check(std::any_of(items.begin(), items.end(),
                          [&](const auto & item)
        {
            return item.id == extra.item && !item.holder &&
                   item.cell == Cell{2, 1};
        }),
        "Thrown Nick lands at target");
    }
    auto p = party();
    auto h = p.member(1).character;
    h.inventory().add("club", "Other kind");
    CampaignParty mixed(rules());
    mixed.add_pc(h);
    mixed.equip(1, 1);
    mixed.equip(1, 3, EquipmentOperation::equip_other);
    auto c = battle(mixed);
    act(*c, "melee", 99);
    settle(*c);
    check(
        !offers(*c, "nick_melee") && offers(*c, "light_melee"),
        "Mastered Nick on the first weapon does not grant Nick to a different non-Nick second weapon");
    auto plain = character("fighter", "Pending mastery");
    plain.inventory().add("dagger", "One");
    plain.inventory().add("dagger", "Two");
    CampaignParty missing(rules());
    missing.add_pc(plain);
    missing.equip(1, 1);
    missing.equip(1, 2, EquipmentOperation::equip_other);
    c = battle(missing);
    act(*c, "melee", 99);
    settle(*c);
    check(!unit(*c, 1).nick_mastery && !offers(*c, "nick_melee") && offers(*c, "light_melee"),
          "Nick metadata alone grants no permission");
}

std::string fixture(const char *name)
{
    std::ifstream in(std::filesystem::path(OPENGOLD_SOURCE_DIR) / "tests/fixtures" / name);
    check(bool(in), "Open genuine pre-Nick writer");
    return {std::istreambuf_iterator<char>(in), {}};
}

void historical()
{
    auto r = module();
    for (const auto name :
            {"combat-v22-nick-before.save", "combat-v22-nick-light-pending.save",
             "combat-v22-nick-light-spent.save", "combat-v22-nick-other-bonus-spent.save"
            })
    {
        auto bytes = fixture(name);
        const auto at = bytes.find("0.6.56");
        check(at != bytes.npos, "Actual prior identity");
        auto c = r->restore(bytes);
        bytes.replace(at, 6, r->identity().version);
        check(
            c->save() == bytes,
            "Historical current-turn budgets, pending choices and RNG unchanged apart from identity");
        check(!offers(*c, "nick_melee"),
              "Ambiguous old current-turn Bonus Action is not reinterpreted");
        if (c->snapshot().savage_attack_choice)
        {
            act(*c, "savage_skip");
            auto settled = fixture("combat-v22-nick-light-spent.save");
            settled.replace(settled.find("0.6.56"), 6, r->identity().version);
            check(c->save() == settled,
                  "Actual old pending Light resolves to actual old settled writer");
        }
        act(*c, "end");
        act(*c, "end");
        act(*c, "melee", 99);
        settle(*c);
        check(c->save().starts_with("OGCOMBAT 23 ") && offers(*c, "nick_melee"),
              "Retained old encounter gains Nick at its first unambiguous fresh turn");
        round_trip(*r, *c);
    }
    auto p = party();
    auto c = battle(p);
    bool pending = false;
    for (unsigned seed = 1; seed <= 96 && !pending; ++seed)
    {
        c = battle(p, seed);
        act(*c, "melee", 99);
        settle(*c);
        act(*c, "nick_melee", 99);
        pending = bool(c->snapshot().savage_attack_choice);
    }
    check(pending, "Forged-budget test has actual pending Nick damage");
    auto bytes = c->save();
    auto bad = bytes;
    bad.replace(bad.find(p.rule_module().identity().version),
                p.rule_module().identity().version.size(), "0.6.56");
    rejects(
        [&]
    {
        (void)p.rule_module().restore(bad);
    });
    std::size_t row = 0;
    for (unsigned i = 0; i < 4; ++i)
        row = bytes.find('\n', row) + 1;
    while (!bytes.substr(row).starts_with("1 "))
        row = bytes.find('\n', row) + 1;
    const auto end = bytes.find('\n', row), origin = bytes.rfind(' ', end),
               budget = bytes.rfind(' ', origin - 1);
    for (const auto value :
            {"0", "3"
            })
    {
        bad = bytes;
        bad.replace(budget + 1, origin - budget - 1, value);
        rejects(
            [&]
        {
            (void)p.rule_module().restore(bad);
        });
    }
    bad = bytes;
    bad.replace(origin + 1, end - origin - 1, "0");
    rejects(
        [&]
    {
        (void)p.rule_module().restore(bad);
    });
}

void ui_fixtures()
{
    const auto output = std::getenv("OPENGOLD_NICK_FIXTURES");
    if (!output)
        return;
    auto r = module();
    const auto path = std::filesystem::path(output);
    std::filesystem::create_directories(path);
    const auto write = [&](const char *name, const CombatSession & c)
    {
        std::ofstream out(path / (std::string(name) + ".save"));
        out << c.save();
        check(bool(out), "Write actual Nick UI fixture");
    };
    auto c = r->restore(fixture("combat-v22-nick-before.save"));
    act(*c, "end");
    act(*c, "end");
    write("before", *c);
    const auto before = c->save();
    act(*c, "melee", 99);
    settle(*c);
    write("qualified", *c);
    const auto qualified = c->save();
    act(*c, "nick_melee", 99);
    settle(*c);
    write("after", *c);
    c = r->restore(qualified);
    act(*c, "nick_throw", 99);
    settle(*c);
    write("thrown", *c);
    c = r->restore(before);
    act(*c, "second_wind");
    act(*c, "melee", 99);
    settle(*c);
    write("spent-bonus", *c);
    act(*c, "nick_melee", 99);
    settle(*c);
    write("spent-after", *c);
}

void run()
{
    const auto test = [](const char *label, auto fn)
    {
        try
        {
            fn();
        }
        catch (const std::exception &e)
        {
            throw std::runtime_error(std::string(label) + ": " + e.what());
        }
    };
    test("Nick grants/budgets", grants_and_budgets);
    test("Nick interactions", pending_interactions);
    test("Nick throwing/provenance", throwing_and_provenance);
    test("Nick historical", historical);
    ui_fixtures();
}

} // namespace nick_attack_checks
