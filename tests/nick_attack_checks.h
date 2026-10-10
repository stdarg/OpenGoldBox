namespace nick_attack_checks
{
using namespace mastery_combat_checks;

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
    h.add_item({.definition_id = weapon, .name = "First weapon"});
    h.add_item({.definition_id = weapon, .name = "Second weapon"});
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
    actors.push_back({99, "mastery_target", "Target", Side::opposition, {2, 1}});
    auto c = p.rule_module().create({{12, 8, std::vector<Terrain>(96)}, actors, 777}, seed);
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
                        c->save().starts_with("OGCOMBAT 47 ") && unit(*c, 1).nick_mastery &&
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
                    const auto nick = result(*c);
                    if (!arg(nick, "hit").empty())
                    {
                        // The modifier is not reported, so bound the damage by the
                        // weapon's dice with the expected shared Light modifier.
                        const int modifier = negative ? -2 : style ? 3 : 0;
                        const int sides = std::string_view(weapon) == "scimitar" ? 6 : 4;
                        const int count = arg(nick, "hit") == "CRITICAL" ? 2 : 1;
                        const int damage = std::stoi(arg(nick, "damage"));
                        check(damage >= std::max(0, count + modifier) &&
                              damage <= std::max(0, count * sides + modifier),
                              "Nick shares Light damage modifier and Two-Weapon Fighting");
                        ++hits;
                        if (count == 2)
                            ++criticals;
                        check(p.rule_module().restore(c->save())->save() == c->save(),
                              "Nick hit restores with unspent Bonus Action");
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
          "Matrix actually exercises ordinary and critical Nick damage");
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
    rogue.add_item({.definition_id = "shortsword", .name = "Vex blade"});
    rogue.add_item({.definition_id = "scimitar", .name = "Nick blade"});
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
        check(logged(*c, "adds Sneak Attack"),
              "The first eligible hit applies Sneak Attack automatically");
        act(*c, "nick_melee", 99);
        const auto log = c->snapshot().log();
        check(std::count_if(log.begin(), log.end(), [](const auto & line)
        {
            return line.find("adds Sneak Attack") != std::string::npos;
        }) == 1, "Sneak Attack applies only once per turn");
        auto copy = p.rule_module().restore(c->save());
        check(copy->save() == c->save() && !fx::vexed_by(state(*c, 99), 777, 1) &&
              !unit(*c, 1).bonus_action && !offers(*c, "nick_melee") &&
              !offers(*c, "light_melee"),
              "Nick consumes Vex and keeps the shared spent allowance and Steady Aim");
        checked = true;
    }
    check(checked, "Actual Vex hit exercises automatic Sneak Attack before Nick");
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
        if (arg(result(*c), "hit") != "CRITICAL")
            continue;
        check(c->snapshot().free_movement.has_value(), "Nick critical grants Champion movement");
        auto copy = champion.rule_module().restore(c->save());
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
              "Thrown Nick keeps the shared budget continuation");
        const auto items = c->snapshot().held_items;
        check(std::any_of(items.begin(), items.end(),
                          [&](const auto & item)
        {
            return item.id == extra.item && item.holder == 1;
        }),
        "Thrown Nick weapon stays with the thrower");
    }
    auto p = party();
    auto h = p.member(1).character;
    h.add_item({.definition_id = "club", .name = "Other kind"});
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
    plain.add_item({.definition_id = "dagger", .name = "One"});
    plain.add_item({.definition_id = "dagger", .name = "Two"});
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

void forged_budgets()
{
    auto p = party();
    auto c = battle(p);
    bool hit = false;
    for (unsigned seed = 1; seed <= 96 && !hit; ++seed)
    {
        c = battle(p, seed);
        act(*c, "melee", 99);
        settle(*c);
        act(*c, "nick_melee", 99);
        hit = !arg(result(*c), "hit").empty();
    }
    check(hit, "Forged-budget test has an actual Nick hit");
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
    // The Light budget, the Nick origin and the Cleave flag precede the actor
    // row's fourteen later fields (Colossus Slayer through Concentration).
    auto end = bytes.find('\n', row);
    for (unsigned n = 0; n < 14; ++n)
        end = bytes.rfind(' ', end - 1);
    const auto cleave = bytes.rfind(' ', end - 1), origin = bytes.rfind(' ', cleave - 1),
               budget = bytes.rfind(' ', origin - 1);
    bad = bytes;
    bad.replace(budget + 1, origin - budget - 1, "3");
    rejects(
        [&]
    {
        (void)p.rule_module().restore(bad);
    });
    bad = bytes;
    bad.replace(origin + 1, cleave - origin - 1, "9");
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
        std::ofstream out(path / (std::string(name) + ".save"), std::ios::binary);
        out << c.save();
        check(bool(out), "Write actual Nick UI fixture");
    };
    CampaignParty p(module());
    auto h = hero("handaxe", "fighter", "soldier");
    h.add_item({.definition_id = "dagger", .name = "First dagger"});
    h.add_item({.definition_id = "dagger", .name = "Second dagger"});
    const auto id = p.add_pc(h);
    p.equip(id, 1);
    p.equip(id, 2, EquipmentOperation::equip_other);
    p.award_experience(300, "nick-ui");
    p.advance(id, p.default_advancement(id));
    // Wounded, so the spent-bonus fixture can use Second Wind.
    auto saved = p.checkpoint();
    saved.roster[0].vitals.hit_points -= 3;
    p.restore(saved);
    auto actors = p.participants();
    actors.front().cell = {1, 1};
    actors.push_back({99, "vanguard", "Target", Side::opposition, {2, 1}});
    auto c = r->create({{12, 8, std::vector<Terrain>(96)}, actors, 777}, 1);
    turn(*c, id);
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
    test("Nick forged budgets", forged_budgets);
    ui_fixtures();
}

} // namespace nick_attack_checks
