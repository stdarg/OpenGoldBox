namespace graze_checks
{
using namespace mastery_combat_checks;

auto rules(std::string_view defense)
{
    std::ifstream in(std::filesystem::path(OPENGOLD_SOURCE_DIR) /
                     "data/rules/srd-5.2.1/combat.rules");
    std::string text{std::istreambuf_iterator<char>(in), {}};
    text += "\ncreature graze_target 25 1000 0 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n";
    if (defense == "resistance_vulnerability")
        text +=
            "affinity graze_target ward resistance all\naffinity graze_target weakness vulnerability all\n";
    else if (!defense.empty())
        text += "affinity graze_target ward " + std::string(defense) + " all\n";
    return srd5::parse_content(text);
}

void ui_fixtures()
{
    const auto output = std::getenv("OPENGOLD_GRAZE_FIXTURES");
    if (!output)
        return;
    const auto directory = std::filesystem::path(output);
    std::filesystem::create_directories(directory);
    auto r = module();
    auto e = encounter(*r, hero("greatsword", "fighter", "soldier"), "greatsword");
    e.participants.back().definition = "vanguard";
    for (unsigned seed = 1; seed < 128; ++seed)
    {
        auto c = r->create(e, seed);
        turn(*c, 1);
        const auto before = c->save();
        act(*c, "melee", 99);
        if (!c->snapshot().optional_effect_choice)
            continue;
        const auto write = [&](const char *name, const std::string & bytes)
        {
            std::ofstream out(directory / (std::string(name) + ".save"), std::ios::binary);
            out << bytes;
            check(bool(out), "Write actual Graze UI checkpoint");
        };
        write("before", before);
        write("pending", c->save());
        auto copy = r->restore(c->save());
        act(*c, "effect_use");
        write("used", c->save());
        act(*copy, "effect_skip");
        write("skipped", copy->save());
        return;
    }
    throw std::runtime_error("UI fixture needs an actual miss");
}

void reactions_and_limits()
{
    for (const auto key :
            {"glaive", "greatsword"
            })
    {
        auto r = rules("");
        auto e = encounter(*r, hero(key), key);
        e.participants.back().definition = "graze_target";
        const int edge = std::string_view(key) == "glaive" ? 3 : 2;
        e.participants.back().cell = {edge, 1};
        bool tested = false;
        for (unsigned seed = 1; seed < 128 && !tested; ++seed)
        {
            auto c = r->create(e, seed);
            turn(*c, 1);
            act(*c, "end");
            Command move;
            for (const auto &offered : c->legal_commands())
                if (offered.verb == "move" && offered.destination == Cell{edge + 1, 1})
                    move = offered;
            check(!move.verb.empty() && c->submit(move) && c->snapshot().reaction_pending,
                  "Enemy leaving reach provokes Graze-capable reaction");
            const auto position = unit(*c, 99).cell;
            act(*c, "opportunity", 99);
            if (!c->snapshot().optional_effect_choice)
                continue;
            check(unit(*c, 99).cell == position && !unit(*c, 1).reaction,
                  "Pending miss retains interrupted movement and spent Reaction");
            const auto pending = c->save();
            auto copy = r->restore(pending);
            act(*c, "effect_use");
            act(*copy, "effect_use");
            check(c->save() == copy->save() && unit(*c, 99).cell == Cell{edge + 1, 1},
                  "Graze resolves before exact interrupted movement continuation");
            auto skipped = r->restore(pending);
            act(*skipped, "effect_skip");
            check(unit(*skipped, 99).cell == Cell{edge + 1, 1} && !unit(*skipped, 1).reaction,
                  "Skipping also resumes movement without refund");
            tested = true;
        }
        check(tested, "Actual missed opportunity tested");
    }
    for (int score :
            {
                3, 8, 18
            })
    {
        auto draft = hero("greatsword", "fighter", "soldier").creation_data();
        const int face = score / 3;
        for (auto &roll : draft.rolls)
            roll = {{face, face, face + score % 3, 1}, 3};
        draft.training["class:fighter:fighting_style"] = {"great_weapon_fighting"};
        Character h(*srd5::character_rules(), draft, {});
        auto r = rules("");
        auto e = encounter(*r, h, "greatsword");
        e.participants.back().definition = "graze_target";
        bool tested = false;
        for (unsigned seed = 1; seed < 64 && !tested; ++seed)
        {
            auto c = r->create(e, seed);
            turn(*c, 1);
            act(*c, "melee", 99);
            if (h.sheet().modifiers[0] <= 0)
            {
                if (!result(*c).source.ends_with("misses."))
                    continue;
                check(!c->snapshot().optional_effect_choice && unit(*c, 99).hit_points == 1000,
                      "Nonpositive modifiers do not produce a pointless Graze prompt");
                tested = true;
                continue;
            }
            if (!c->snapshot().optional_effect_choice)
                continue;
            const int hp = unit(*c, 99).hit_points;
            act(*c, "effect_use");
            check(unit(*c, 99).hit_points == hp - h.sheet().modifiers[0], "Graze omits style dice");
            tested = true;
        }
        check(tested, "Each ability modifier actually missed");
    }
    auto draft = hero("greatsword").creation_data();
    draft.training.erase("class:fighter:weapon_mastery");
    auto r = rules("");
    auto e = encounter(*r, Character(*srd5::character_rules(), draft, {}), "greatsword");
    e.participants.back().definition = "graze_target";
    for (unsigned seed = 1; seed < 16; ++seed)
    {
        auto c = r->create(e, seed);
        turn(*c, 1);
        act(*c, "melee", 99);
        check(!c->snapshot().optional_effect_choice,
              "Weapon metadata alone does not grant mastery");
    }
}

void advancement_and_rejection()
{
    for (const auto klass :
            {"fighter", "paladin", "ranger"
            })
        for (unsigned level = 1; level <= 4; ++level)
        {
            auto r = rules("");
            CampaignParty party(module());
            auto h = hero("greatsword", klass, "soldier");
            h.inventory().add("greatsword", "Physical blade");
            const auto id = party.add_pc(h);
            party.equip(id, 1);
            party.award_experience(2700, "graze-levels");
            while (party.member(id).character.sheet().level < int(level))
                party.advance(id, party.default_advancement(id));
            auto actors = party.participants();
            actors.front().cell = {1, 1};
            actors.push_back({99, "graze_target", "Target", 1, {2, 1}});
            bool tested = false;
            for (unsigned seed = 1; seed < 64 && !tested; ++seed)
            {
                auto c = r->create({{12, 8, std::vector<std::uint8_t>(96)}, actors, 777}, seed);
                turn(*c, 1);
                if (offers(*c, "action_surge"))
                    act(*c, "action_surge");
                act(*c, "melee", 99);
                if (!c->snapshot().optional_effect_choice)
                    continue;
                const auto pending = c->save();
                check(r->restore(pending)->save() == pending,
                      "Physical and advanced sources retain pending choice");
                auto stale = command(*c, "effect_use");
                const int hp = unit(*c, 99).hit_points;
                act(*c, "effect_use");
                check(hp - unit(*c, 99).hit_points ==
                      std::max(0, party.member(id).character.sheet().modifiers[0]),
                      "Advancement changes actual attack ability damage");
                const auto after = c->save();
                check(!c->submit(stale) && c->save() == after,
                      "A stale Use cannot deal damage again");
                if (std::string_view(klass) == "fighter" && level >= 2)
                    check(offers(*c, "melee"),
                          "Graze preserves the separate Action Surge allowance");
                auto bad = pending;
                bad.replace(bad.find(r->identity().version), 6, "0.6.58");
                rejects(
                    [&]
                {
                    (void)r->restore(bad);
                });
                const auto tail = pending.rfind('\n', pending.size() - 2) + 1;
                for (const char *invalid :
                        {"99999 99 1 1 1\n", "1 1 1 1 1\n", "1 99 20 1 1\n"
                        })
                {
                    bad = pending;
                    bad.replace(tail, std::string::npos, invalid);
                    rejects(
                        [&]
                    {
                        (void)r->restore(bad);
                    });
                }
                tested = true;
            }
            check(tested, "Every currently supported advancement route actually missed");
        }
}

void run()
{
    unsigned cases = 0;
    for (const auto key :
            {"glaive", "greatsword"
            })
        for (const auto klass :
                {"fighter", "barbarian", "paladin", "ranger"
                })
            for (bool npc :
                    {
                        false, true
                    })
                for (const auto defense :
                        {"", "resistance", "vulnerability", "immunity", "resistance_vulnerability"
                        })
                {
                    auto r = rules(defense);
                    auto h = hero(key, klass, "soldier");
                    auto e = encounter(*r, h, key);
                    e.participants.back().definition = "graze_target";
                    if (npc)
                    {
                        e.participants.front().side = 1;
                        e.participants.back().side = 0;
                    }
                    bool tested = false;
                    for (unsigned seed = 1; seed < 64 && !tested; ++seed)
                    {
                        auto c = r->create(e, seed);
                        turn(*c, 1);
                        act(*c, "melee", 99);
                        if (!c->snapshot().optional_effect_choice)
                        {
                            if (std::string_view(defense) == "immunity" &&
                                    result(*c).source.ends_with("misses."))
                            {
                                check(
                                    unit(*c, 99).hit_points == 1000 && !offers(*c, "melee"),
                                    "Immunity suppresses a pointless Graze choice without refunding the attack");
                                tested = true;
                                ++cases;
                            }
                            continue;
                        }
                        const auto pending = c->save();
                        check(pending.starts_with("OGCOMBAT 34 "),
                              "Graze uses the current checkpoint format");
                        check(r->restore(pending)->save() == pending,
                              "Pending Graze round trips exactly");
                        check(c->legal_commands().size() == 2 && c->movement_reach(1).empty(),
                              "Only Use/Skip legal while Graze waits");
                        auto invalid = command(*c, "effect_use");
                        invalid.target = 1;
                        check(!c->submit(invalid) && c->save() == pending,
                              "Rejected choice preserves all state");
                        const int hp = unit(*c, 99).hit_points;
                        auto skipped = r->restore(pending);
                        act(*skipped, "effect_skip");
                        check(unit(*skipped, 99).hit_points == hp, "Skip leaves damage unchanged");
                        check(!offers(*skipped, "melee"), "Skip does not refund the Action");
                        const int modifier = h.sheet().modifiers[0];
                        const int expected = std::string_view(defense) == "immunity" ? 0
                                             : std::string_view(defense).starts_with("resistance")
                                             ? std::max(0, modifier) / 2
                                             : std::max(0, modifier);
                        act(*c, "effect_use");
                        check(unit(*c, 99).hit_points == hp - expected,
                              "Graze typed defense and vulnerability cap");
                        check(!logged(*c, "(Savage Attacker)") &&
                              !logged(*c, "adds Sneak Attack") &&
                              !c->snapshot().free_movement,
                              "Miss damage never triggers hit/critical dice features");
                        check(!offers(*c, "melee") && r->restore(c->save())->save() == c->save(),
                              "Resolved Graze retains spent Action and continuation");
                        tested = true;
                        ++cases;
                    }
                    check(tested, "Each Graze source/weapon/defense actually misses");
                }
    check(cases == 80, "Full Graze source matrix ran");
    reactions_and_limits();
    advancement_and_rejection();
    ui_fixtures();
}
} // namespace graze_checks
