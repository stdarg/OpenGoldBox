namespace optional_mastery_checks
{
using namespace mastery_combat_checks;

void choose(CombatSession &c, std::string_view verb, unsigned item)
{
    for (const auto &command : c.legal_commands())
        if (command.verb == verb && command.item == item)
        {
            check(c.submit(command), "Effect choice accepted");
            return;
        }
    throw std::runtime_error("Missing effect choice " + std::string(verb) + " " +
                             std::to_string(item));
}

void roundtrip(const RulesModule &r, CombatSession &c)
{
    check(r.restore(c.save())->save() == c.save(), "Optional mastery checkpoint round trip");
}

void matrix()
{
    auto r = rules();
    unsigned cases = 0;
    for (const auto &weapon : fx::weapons)
    {
        const auto kind = weapon.mastery;
        if (kind != fx::Mastery::slow && kind != fx::Mastery::topple &&
                kind != fx::Mastery::cleave && kind != fx::Mastery::push)
            continue;
        for (bool ranged :
                {
                    false, true
                })
        {
            if ((!ranged && weapon.ranged) || (ranged && !weapon.ranged && !weapon.thrown))
                continue;
            // Club is already selected by the common fixture helper.
            auto draft = hero("quarterstaff").creation_data();
            draft.training["class:fighter:weapon_mastery"] = {std::string(weapon.key), "dagger",
                                                              "mace"
                                                             };
            auto e = encounter(*r, Character(*srd5::character_rules(), draft, {}),
                               std::string(weapon.key), ranged);
            e.participants.push_back({98, "mastery_target", "Second", 1, {2, 2}});
            bool tested = false;
            for (unsigned seed = 1; seed < 32 && !tested; ++seed)
            {
                auto c = r->create(e, seed);
                turn(*c, 1);
                act(*c, ranged ? "ranged" : "melee", 99);
                if (!c->snapshot().optional_effect_choice)
                    continue;
                check(c->snapshot().optional_effect_choice->title.source ==
                      (kind == fx::Mastery::slow     ? "Slow"
                       : kind == fx::Mastery::topple ? "Topple"
                       : kind == fx::Mastery::cleave ? "Cleave"
                       : "Push"),
                      "Actual weapon selects its mastery");
                roundtrip(*r, *c);
                const auto pending = c->save();
                auto skipped = r->restore(pending);
                choose(*skipped, "effect_skip", 1);
                check(!skipped->snapshot().optional_effect_choice &&
                      !offers(*skipped, ranged ? "ranged" : "melee"),
                      "Skip consumes no further action and does not refund attack");
                auto invalid = command(*c, "effect_use");
                invalid.item = 99;
                check(!c->submit(invalid) && c->save() == pending,
                      "Unknown option rejected atomically");
                const auto movement = unit(*c, 1).movement_feet;
                choose(*c, "effect_use", 1);
                if (kind == fx::Mastery::slow)
                    check(fx::slowed(state(*c, 99)), "Slow applied on actual damaging hit");
                if (kind == fx::Mastery::topple)
                    check(!c->snapshot().optional_effect_choice, "Topple save resolves choice");
                if (kind == fx::Mastery::cleave)
                {
                    check(c->snapshot().effect_targeting && !offers(*c, "melee"),
                          "Cleave targets with no normal action");
                    roundtrip(*r, *c);
                    act(*c, "effect_attack", 98);
                    check(!c->snapshot().optional_effect_choice && !offers(*c, "effect_attack"),
                          "Cleave cannot chain itself");
                }
                if (kind == fx::Mastery::push)
                {
                    check(bool(c->snapshot().effect_targeting), "Push enters landing selection");
                    roundtrip(*r, *c);
                    const auto prior = unit(*c, 99).cell;
                    act(*c, "effect_push", 99);
                    check(unit(*c, 99).cell != prior, "Push changes target cell");
                }
                check(unit(*c, 1).movement_feet == movement && !c->snapshot().reaction_pending,
                      "Mastery consumes no normal movement and Push causes no opportunity attack");
                roundtrip(*r, *c);
                tested = true;
                ++cases;
            }
            check(tested, "Every optional weapon/range produced an actual choice");
        }
    }
    check(cases >= 18, "Optional property catalog covered");
}

void simultaneous()
{
    auto r = module();
    for (const auto weapon :
            {"longbow", "maul"
            })
        for (bool movement_first :
                {
                    false, true
                })
        {
            auto c = r->restore(mastery_choice_checks::critical_before(weapon));
            act(*c, std::string_view(weapon) == "longbow" ? "ranged" : "melee", 99);
            act(*c, "savage_skip");
            check(c->snapshot().optional_effect_choice &&
                  c->snapshot().optional_effect_choice->options.size() == 2,
                  "Critical retains mastery and Champion as separate options");
            roundtrip(*r, *c);
            if (movement_first)
            {
                choose(*c, "effect_use", 2);
                check(bool(c->snapshot().free_movement), "Selected Champion movement starts");
                roundtrip(*r, *c);
                act(*c, "end");
                check(bool(c->snapshot().optional_effect_choice),
                      "Finishing movement retains mastery");
                choose(*c, "effect_use", 1);
            }
            else
            {
                choose(*c, "effect_use", 1);
                check(c->snapshot().optional_effect_choice &&
                      c->snapshot().optional_effect_choice->options.size() == 1,
                      "Using mastery retains movement");
                roundtrip(*r, *c);
                choose(*c, "effect_use", 2);
                act(*c, "end");
            }
            check(!c->snapshot().optional_effect_choice && !c->snapshot().free_movement,
                  "Both chosen effects finish independently");
            roundtrip(*r, *c);
        }
}

void reactions()
{
    auto r = rules();
    for (const auto key :
            {"javelin", "maul", "halberd", "warhammer"
            })
        for (bool move_first :
                {
                    false, true
                })
        {
            CampaignParty p(module());
            auto h = hero(key, "fighter", "soldier");
            h.inventory().add(key, "Mastery weapon");
            p.add_pc(h);
            p.equip(1, 1);
            p.award_experience(900, "reaction-mastery");
            while (p.member(1).character.sheet().level < 3)
                p.advance(1, p.default_advancement(1));
            auto actors = p.participants();
            actors.front().cell = {1, 1};
            const int edge = std::string_view(key) == "halberd" ? 3 : 2;
            actors.push_back({99, "mastery_target", "Mover", 1, {edge, 1}});
            actors.push_back({98, "mastery_target", "Second", 1, {edge, 2}});
            bool tested = false;
            for (unsigned seed = 1; seed < 128 && !tested; ++seed)
            {
                auto c = r->create({{12, 8, std::vector<std::uint8_t>(96)}, actors, 777}, seed);
                turn(*c, 99);
                Command move;
                for (const auto &cmd : c->legal_commands())
                    if (cmd.verb == "move" && cmd.destination == Cell{edge + 1, 1})
                        move = cmd;
                check(!move.verb.empty() && c->submit(move) && c->snapshot().reaction_pending,
                      "Mastery source gets a real opportunity");
                act(*c, "opportunity", 99);
                if (!c->snapshot().savage_attack_choice ||
                        !c->snapshot().savage_attack_choice->critical)
                    continue;
                act(*c, "savage_skip");
                if (!c->snapshot().optional_effect_choice)
                    continue;
                // On the enemy turn, mastery is offered before movement regardless of player
                // preference.
                check(c->snapshot().optional_effect_choice->options.size() == 1,
                      "Enemy-turn mastery resolves before Champion");
                roundtrip(*r, *c);
                choose(*c, "effect_use", 1);
                if (c->snapshot().effect_targeting)
                {
                    roundtrip(*r, *c);
                    act(*c, std::string_view(key) == "halberd" ? "effect_attack" : "effect_push",
                        std::string_view(key) == "halberd" ? 98 : 99);
                    if (c->snapshot().savage_attack_choice)
                        act(*c, "savage_skip");
                }
                roundtrip(*r, *c);
                while (c->snapshot().optional_effect_choice)
                {
                    choose(*c, "effect_use", 2);
                    roundtrip(*r, *c);
                    if (move_first)
                    {
                        for (const auto &cmd : c->legal_commands())
                            if (cmd.verb == "move")
                            {
                                check(c->submit(cmd), "Reaction Champion movement");
                                break;
                            }
                    }
                    if (c->snapshot().free_movement)
                        act(*c, "end");
                }
                roundtrip(*r, *c);
                check(!c->snapshot().reaction_pending && !unit(*c, 1).reaction,
                      "Effects finish without refunding Reaction");
                tested = true;
            }
            check(tested, "Critical opportunity with optional mastery covered");
        }
}

void cleave_criticals()
{
    auto r = rules();
    CampaignParty p(module());
    auto h = hero("greataxe", "fighter", "soldier");
    h.inventory().add("greataxe", "Axe");
    p.add_pc(h);
    p.equip(1, 1);
    p.award_experience(900, "cleave-champion");
    while (p.member(1).character.sheet().level < 3)
        p.advance(1, p.default_advancement(1));
    auto actors = p.participants();
    actors.front().cell = {1, 1};
    actors.push_back({99, "mastery_target", "First", 1, {2, 1}});
    actors.push_back({98, "mastery_target", "Second", 1, {2, 2}});
    bool tested = false;
    for (unsigned seed = 1; seed < 1000 && !tested; ++seed)
    {
        auto c = r->create({{12, 8, std::vector<std::uint8_t>(96)}, actors, 777}, seed);
        turn(*c, 1);
        act(*c, "melee", 99);
        if (!c->snapshot().savage_attack_choice || !c->snapshot().savage_attack_choice->critical)
            continue;
        act(*c, "savage_skip");
        choose(*c, "effect_use", 1);
        act(*c, "effect_attack", 98);
        if (!c->snapshot().savage_attack_choice || !c->snapshot().savage_attack_choice->critical)
            continue;
        check(c->snapshot().savage_attack_choice->modifier == 0,
              "Cleave omits positive ability modifier even on a critical");
        roundtrip(*r, *c);
        act(*c, "savage_skip");
        check(c->snapshot().optional_effect_choice &&
              c->snapshot().optional_effect_choice->options.size() == 2,
              "Two critical hits retain two separate movement entitlements");
        roundtrip(*r, *c);
        choose(*c, "effect_use", 2);
        roundtrip(*r, *c);
        act(*c, "end");
        check(c->snapshot().optional_effect_choice &&
              c->snapshot().optional_effect_choice->options.size() == 1,
              "Finishing first movement retains second");
        choose(*c, "effect_use", 2);
        act(*c, "end");
        roundtrip(*r, *c);
        act(*c, "action_surge");
        act(*c, "melee", 99);
        if (c->snapshot().savage_attack_choice)
            act(*c, "savage_skip");
        check(!c->snapshot().effect_targeting &&
              (!c->snapshot().optional_effect_choice ||
               c->snapshot().optional_effect_choice->title.source != "Cleave"),
              "Action Surge does not reset Cleave once-per-turn budget");
        tested = true;
    }
    check(tested, "Actual consecutive Champion criticals covered");
}

void ui_fixtures()
{
    const auto output = std::getenv("OPENGOLD_OPTIONAL_MASTERY_FIXTURES");
    if (!output)
        return;
    auto r = module();
    const auto directory = std::filesystem::path(output);
    std::filesystem::create_directories(directory);
    const auto write = [&](const std::string & name, const std::string & data)
    {
        std::ofstream out(directory / (name + ".save"), std::ios::binary);
        out << data;
        check(bool(out), "Write optional mastery UI checkpoint");
    };
    for (const auto key :
            {"javelin", "maul", "greataxe", "warhammer"
            })
    {
        const std::string name = key;
        auto e = encounter(*r, hero(key), key);
        e.participants.back().definition = "vanguard";
        e.participants.push_back(
        {98, "vanguard", "Second", std::string_view(key) == "greataxe" ? 0u : 1u, {2, 2}});
        bool written = false;
        for (unsigned seed = 1; seed < 64 && !written; ++seed)
        {
            auto c = r->create(e, seed);
            turn(*c, 1);
            const auto before = c->save();
            act(*c, "melee", 99);
            if (!c->snapshot().optional_effect_choice)
                continue;
            write(name + "-before", before);
            write(name + "-pending", c->save());
            auto skip = r->restore(c->save());
            choose(*skip, "effect_skip", 1);
            write(name + "-skipped", skip->save());
            choose(*c, "effect_use", 1);
            if (c->snapshot().effect_targeting)
            {
                write(name + "-targeting", c->save());
                auto cancel = r->restore(c->save());
                choose(*cancel, "effect_skip", 1);
                write(name + "-cancelled", cancel->save());
                act(*c, std::string_view(key) == "greataxe" ? "effect_attack" : "effect_push",
                    std::string_view(key) == "greataxe" ? 98 : 99);
            }
            write(name + "-used", c->save());
            written = true;
        }
        check(written, "Actual hit optional mastery UI fixture");
    }
    auto c = r->restore(mastery_choice_checks::critical_before("maul"));
    act(*c, "melee", 99);
    act(*c, "savage_skip");
    write("ordered-pending", c->save());
    choose(*c, "effect_use", 2);
    write("ordered-moving", c->save());
    act(*c, "end");
    write("ordered-mastery", c->save());
    choose(*c, "effect_use", 1);
    write("ordered-used", c->save());
}

void boundaries()
{
    const auto content = [](std::string suffix)
    {
        std::ifstream in(std::filesystem::path(OPENGOLD_SOURCE_DIR) /
                         "data/rules/srd-5.2.1/combat.rules");
        return std::string{std::istreambuf_iterator<char>(in), {}} +
               "\ncreature mastery_target 1 1000 0 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n" + suffix;
    };
    for (const auto size :
            {"tiny", "small", "medium", "large", "huge", "gargantuan"
            })
    {
        auto r = srd5::parse_content(content(std::string("size mastery_target ") + size + "\n"));
        auto e = encounter(*r, hero("warhammer"), "warhammer");
        for (unsigned seed = 1; seed <= 8; ++seed)
        {
            auto c = r->create(e, seed);
            turn(*c, 1);
            act(*c, "melee", 99);
            const bool hit = !result(*c).source.ends_with("misses.");
            check(bool(c->snapshot().optional_effect_choice) ==
                  (hit && std::string_view(size) != "huge" &&
                   std::string_view(size) != "gargantuan"),
                  "Push size limit is Large or smaller");
        }
    }
    rejects(
        [&]
    {
        (void)srd5::parse_content(
        content("size mastery_target large\nsize mastery_target small\n"));
    });
    for (const auto key :
            {"javelin", "maul"
            })
    {
        auto r = rules(true);
        auto e = encounter(*r, hero(key), key);
        bool tested = false;
        for (unsigned seed = 1; seed <= 16 && !tested; ++seed)
        {
            auto c = r->create(e, seed);
            turn(*c, 1);
            act(*c, "melee", 99);
            if (result(*c).source.ends_with("misses."))
                continue;
            check(bool(c->snapshot().optional_effect_choice) == (std::string_view(key) == "maul"),
                  "Slow needs damage; Topple only needs a hit");
            tested = true;
        }
        check(tested, "Immune target hit");
    }
    auto r = rules();
    for (bool blocked :
            {
                false, true
            })
    {
        auto e = encounter(*r, hero("warhammer"), "warhammer");
        if (blocked)
            e.battlefield.terrain[1 * 12 + 3] = 1;
        auto c = r->create(e, 11);
        turn(*c, 1);
        act(*c, "melee", 99);
        check(bool(c->snapshot().optional_effect_choice) != blocked,
              "Wall blocks all directly-away Push landings and suppresses futile prompt");
        if (!blocked)
        {
            choose(*c, "effect_use", 1);
            unsigned count = 0;
            for (const auto &command : c->legal_commands())
                if (command.verb == "effect_push")
                {
                    check(command.destination.y == 1 &&
                          (command.destination.x == 3 || command.destination.x == 4),
                          "Push allows only five or ten feet straight away");
                    ++count;
                }
            check(count == 2, "Both unobstructed landing distances offered");
        }
    }
    // The target's save uses Constitution; the DC uses the attack ability, not a spellcasting
    // score.
    for (unsigned seed = 1; seed <= 24; ++seed)
    {
        auto c = r->create(encounter(*r, hero("maul"), "maul"), seed);
        turn(*c, 1);
        act(*c, "melee", 99);
        if (!c->snapshot().optional_effect_choice)
            continue;
        const auto description = c->snapshot().optional_effect_choice->description;
        check(arg(description, "dc") == std::to_string(10 + hero("maul").sheet().modifiers[0]),
              "Topple DC uses actual Strength and proficiency");
        const auto before = c->save();
        choose(*c, "effect_use", 1);
        auto copy = r->restore(before);
        choose(*copy, "effect_use", 1);
        check(copy->save() == c->save(), "Topple save RNG continuation exact");
    }
}

void movement_enables_mastery()
{
    auto r = rules();
    for (const auto key :
            {"halberd", "warhammer"
            })
    {
        CampaignParty p(module());
        auto h = hero(key, "fighter", "soldier");
        h.inventory().add(key, "Weapon");
        p.add_pc(h);
        p.equip(1, 1);
        p.award_experience(900, "positioning");
        while (p.member(1).character.sheet().level < 3)
            p.advance(1, p.default_advancement(1));
        auto roster = p.participants();
        roster.front().cell = {1, 1};
        const bool cleave = std::string_view(key) == "halberd";
        roster.push_back({99, "mastery_target", "First", 1, {cleave ? 3 : 2, 1}});
        if (cleave)
            roster.push_back({98, "mastery_target", "Second", 1, {4, 1}});
        Battlefield board{12, 8, std::vector<std::uint8_t>(96)};
        if (!cleave)
            board.terrain[15] = 1;
        bool tested = false;
        for (unsigned seed = 1; seed < 128 && !tested; ++seed)
        {
            auto c = r->create({board, roster, 777}, seed);
            turn(*c, 1);
            act(*c, "melee", 99);
            if (!c->snapshot().savage_attack_choice ||
                    !c->snapshot().savage_attack_choice->critical)
                continue;
            act(*c, "savage_skip");
            check(c->snapshot().optional_effect_choice &&
                  c->snapshot().optional_effect_choice->options.size() == 2 &&
                  !c->snapshot().optional_effect_choice->options[0].available,
                  "Position-dependent effect retained while Champion can enable it");
            roundtrip(*r, *c);
            choose(*c, "effect_use", 2);
            const Cell destination = cleave ? Cell{2, 1} :
                                     Cell{2, 2};
            bool moved = false;
            for (const auto &cmd : c->legal_commands())
                if (cmd.verb == "move" && cmd.destination == destination)
                {
                    check(c->submit(cmd), "Move to enable mastery");
                    moved = true;
                    break;
                }
            check(moved, "Legal enabling position");
            act(*c, "end");
            roundtrip(*r, *c);
            check(c->snapshot().optional_effect_choice &&
                  c->snapshot().optional_effect_choice->options[0].available,
                  "Moved source re-evaluates mastery geometry");
            choose(*c, "effect_use", 1);
            act(*c, cleave ? "effect_attack" : "effect_push", cleave ? 98 : 99);
            if (c->snapshot().savage_attack_choice)
                act(*c, "savage_skip");
            roundtrip(*r, *c);
            tested = true;
        }
        check(tested, "Both position-dependent masteries enabled by actual critical movement");
    }
}

void slain_reaction_mover()
{
    auto r = rules();
    CampaignParty p(module());
    auto h = hero("greataxe", "fighter", "soldier");
    h.inventory().add("greataxe", "Axe");
    p.add_pc(h);
    p.equip(1, 1);
    auto roster = p.participants();
    roster.front().cell = {1, 1};
    roster.push_back({99, "mastery_target", "Mover", 1, {2, 1}});
    roster.push_back({98, "mastery_target", "Second", 1, {2, 2}});
    auto baseline = r->create({{12, 8, std::vector<std::uint8_t>(96)}, roster, 777}, 1);
    roster[1].state = unit(*baseline, 99).persistent;
    roster[1].state->hit_points = 1;
    bool tested = false;
    for (unsigned seed = 1; seed < 64 && !tested; ++seed)
    {
        auto c = r->create({{12, 8, std::vector<std::uint8_t>(96)}, roster, 777}, seed);
        turn(*c, 99);
        for (const auto &cmd : c->legal_commands())
            if (cmd.verb == "move" && cmd.destination == Cell{3, 1})
            {
                check(c->submit(cmd), "Doomed mover leaves reach");
                break;
            }
        act(*c, "opportunity", 99);
        if (!c->snapshot().savage_attack_choice)
            continue;
        act(*c, "savage_skip");
        check(unit(*c, 99).hit_points == 0 && c->snapshot().optional_effect_choice,
              "Slain mover leaves Cleave available on another creature");
        roundtrip(*r, *c);
        choose(*c, "effect_use", 1);
        act(*c, "effect_attack", 98);
        if (c->snapshot().savage_attack_choice)
        {
            roundtrip(*r, *c);
            act(*c, "savage_skip");
        }
        roundtrip(*r, *c);
        check(c->snapshot().actor != 99 && !c->snapshot().reaction_pending,
              "Dead mover cannot resume its route");
        tested = true;
    }
    check(tested, "Slain mover reaction continuation covered");
}

void physical_and_damage()
{
    auto r = rules();
    for (const auto key :
            {"javelin", "trident"
            })
        for (const auto background :
                {"sage", "soldier"
                })
        {
            CampaignParty p(module());
            auto h = hero(key, "fighter", background);
            h.inventory().add(key, "Thrown weapon");
            p.add_pc(h);
            p.equip(1, 1);
            auto roster = p.participants();
            roster.front().cell = {1, 1};
            roster.push_back({99, "mastery_target", "Target", 1, {3, 1}});
            bool tested = false;
            for (unsigned seed = 1; seed < 32 && !tested; ++seed)
            {
                auto c = r->create({{12, 8, std::vector<std::uint8_t>(96)}, roster, 777}, seed);
                turn(*c, 1);
                act(*c, "throw", 99);
                if (c->snapshot().savage_attack_choice)
                {
                    roundtrip(*r, *c);
                    act(*c, "savage_skip");
                }
                if (!c->snapshot().optional_effect_choice)
                    continue;
                roundtrip(*r, *c);
                choose(*c, "effect_use", 1);
                roundtrip(*r, *c);
                check(c->snapshot().held_items.front().holder == 0,
                      "Mastery does not recover the thrown weapon");
                tested = true;
            }
            check(tested, "Immediate and deferred thrown mastery provenance covered");
        }
    for (int score :
            {
                3, 18
            })
    {
        auto draft = hero("greataxe", "fighter", "soldier").creation_data();
        const int face = score / 3;
        for (auto &roll : draft.rolls)
            roll = {{face, face, face, 1}, 3};
        Character h(*srd5::character_rules(), draft, {});
        auto e = encounter(*r, h, "greataxe");
        e.participants.push_back({98, "mastery_target", "Ally", 0, {2, 2}});
        bool tested = false;
        for (unsigned seed = 1; seed < 64 && !tested; ++seed)
        {
            auto c = r->create(e, seed);
            turn(*c, 1);
            act(*c, "melee", 99);
            if (!c->snapshot().savage_attack_choice)
                continue;
            act(*c, "savage_skip");
            choose(*c, "effect_use", 1);
            check(choose_demo_command(*c).verb == "effect_skip",
                  "Automatic combat declines Cleave when only allies are available");
            act(*c, "effect_attack", 98);
            if (!c->snapshot().savage_attack_choice)
                continue;
            check(c->snapshot().savage_attack_choice->modifier ==
                  std::min(0, h.sheet().modifiers[0]),
                  "Cleave retains negative ability modifiers and omits positive ones");
            roundtrip(*r, *c);
            act(*c, "savage_use");
            roundtrip(*r, *c);
            act(*c, "savage_second");
            roundtrip(*r, *c);
            tested = true;
        }
        check(tested, "Cleave ally and Savage reroll with positive/negative modifier covered");
    }
}

void malformed()
{
    auto r = module();
    auto c = r->restore(mastery_choice_checks::critical_before("maul"));
    act(*c, "melee", 99);
    act(*c, "savage_skip");
    const auto bytes = c->save();
    const auto marker = "\n1 99 " + std::to_string(unsigned(fx::Mastery::topple)) + " ";
    const auto start = bytes.find(marker);
    check(start != bytes.npos, "Find pending mastery record");
    const auto end = bytes.find('\n', start + 1);
    const auto row = bytes.substr(start + 1, end - start - 1);
    for (const auto replacement :
            {
                std::string("99999 99 ") + row.substr(5), std::string("1 1 ") + row.substr(5),
                std::string("1 99 99 20 0 0 \"maul\" 1 1 0")
            })
    {
        auto bad = bytes;
        bad.replace(start + 1, end - start - 1, replacement);
        rejects(
            [&]
        {
            (void)r->restore(bad);
        });
    }
    auto bad = bytes;
    bad.replace(bad.find(r->identity().version), r->identity().version.size(), "0.6.59");
    rejects(
        [&]
    {
        (void)r->restore(bad);
    });
    // The tail is one Champion offer followed by the absent reaction-origin marker.
    const auto offer_start = end + 3;
    const auto offer_end = bytes.find('\n', offer_start);
    check(bytes.substr(end + 1, 2) == "1\n", "Exactly one pending Champion entitlement");
    bad = bytes;
    bad.replace(end + 1, offer_end - end,
                std::string("2\n") + bytes.substr(offer_start, offer_end - offer_start) + "\n" +
                bytes.substr(offer_start, offer_end - offer_start) + "\n");
    rejects(
        [&]
    {
        (void)r->restore(bad);
    });
    roundtrip(*r, *c);
}

void unconscious_cleave()
{
    auto r = rules();
    auto e = encounter(*r, hero("greataxe", "fighter", "soldier"), "greataxe");
    e.participants.push_back({98, "mastery_target", "Unconscious ally", 0, {2, 2}});
    e.participants.back().state =
        VitalState{0, false, "SRD9 0 0 0 0 0 1 0 0 0 0 \"\" 0 0 0 FX7 1 0 0 0"};
    bool tested = false;
    for (unsigned seed = 1; seed < 32 && !tested; ++seed)
    {
        auto c = r->create(e, seed);
        turn(*c, 1);
        act(*c, "melee", 99);
        if (!c->snapshot().savage_attack_choice)
            continue;
        act(*c, "savage_skip");
        choose(*c, "effect_use", 1);
        act(*c, "effect_attack", 98);
        if (!c->snapshot().savage_attack_choice)
            continue;
        check(c->snapshot().savage_attack_choice->critical,
              "Cleave against an adjacent unconscious creature is a critical hit");
        roundtrip(*r, *c);
        act(*c, "savage_skip");
        roundtrip(*r, *c);
        tested = true;
    }
    check(tested, "Unconscious living Cleave target continuation covered");
}

void run()
{
    const auto run = [](auto test, const char *name)
    {
        try
        {
            test();
        }
        catch (const std::exception &e)
        {
            throw std::runtime_error(std::string(name) + ": " + e.what());
        }
    };
    run(ui_fixtures, "UI fixtures");
    run(unconscious_cleave, "Unconscious Cleave target");
    run(malformed, "Malformed mastery checkpoint");
    run(physical_and_damage, "Physical mastery and damage");
    run(movement_enables_mastery, "Movement enabling mastery");
    run(slain_reaction_mover, "Slain reaction mover");
    run(boundaries, "Rule boundaries");
    run(matrix, "Weapon matrix");
    run(simultaneous, "Simultaneous order");
    run(reactions, "Opportunity reactions");
    run(cleave_criticals, "Cleave criticals");
}
} // namespace optional_mastery_checks
