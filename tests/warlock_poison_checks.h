// Included by the shared Warlock cantrip test harness.
namespace warlock_poison
{
void freeze()
{
    auto rules = module();
    check(rules->identity().version == "0.6.38", "Freeze requires actual prior writer");
    CampaignParty party(module());
    party.add_pc(hero());
    auto state = party.checkpoint();
    state.roster[0].vitals.hit_points -= 2;
    state.roster[0].wealth[3] = 37;
    state.random_state = 789;
    party.restore(state);
    const auto base = root / "tests/fixtures";
    write(base / "campaign-v11-warlock-poison-before.ogs",
          encode_campaign(party, nullptr, "warlock-poison"));
    auto actors = party.participants();
    actors[0].cell = {1, 1};
    actors.push_back({99, "vanguard", "Enemy", 1, {5, 1}});
    auto c = rules->create({{8, 8, std::vector<std::uint8_t>(64)}, actors}, 13);
    check(c->snapshot().actor == 1, "Prior Warlock starts");
    check(c->submit(command(*c, "dash")) && c->submit(command(*c, "adrenaline_rush")),
          "Prior action budgets spent");
    write(base / "combat-v13-warlock-poison-before.save", c->save());
    check(c->submit(command(*c, "end")) && c->submit(command(*c, "end")) &&
              c->submit(command(*c, "eldritch_blast", 99)),
          "Prior Eldritch Blast continuation");
    write(base / "combat-v13-warlock-poison-continued.save", c->save());
}
Character selected(bool both = false)
{
    auto d = draft();
    d.cantrips = both ? std::vector<std::string>{"eldritch_blast", "poison_spray"}
                      : std::vector<std::string>{"poison_spray"};
    return Character(*srd5::character_rules(), d, {});
}
void access()
{
    auto rules = module();
    auto h = selected();
    auto available = rules->spell_access(h.sheet());
    check(available.cantrip_choices == 2 && available.cantrips.size() == 1 &&
              available.cantrips[0].id == "poison_spray" &&
              available.cantrips[0].source_id == "class:warlock:pact_magic" &&
              available.cantrips[0].acquired_level == 1,
          "Explicit Poison Spray uses real Warlock source");
    check(rules->spell_access(selected(true).sheet()).cantrips.size() == 2,
          "Two supported choices can complete starting cantrips");
    auto d = draft();
    for (auto bad : std::vector<std::vector<std::string>>{
             {"poison_spray", "poison_spray"}, {"eldritch_blast", "poison_spray", "fire_bolt"}})
    {
        d.cantrips = bad;
        rejects(
            [&]
            {
                (void)srd5::character_rules()->evaluate(d, true);
            });
    }
    auto bad = h.sheet();
    for (auto &g : bad.grants)
        if (g.id == "spell:poison_spray")
            g.source_id = "class:wizard:spellcasting";
    rejects(
        [&]
        {
            (void)rules->character_profile(bad, {});
        });
    auto old_identity = rules->identity();
    old_identity.version = "0.6.38";
    rejects(
        [&]
        {
            rules->validate_saved_grants(old_identity, h.sheet(), h.sheet().grants);
        });
    auto profile = rules->character_profile(h.sheet(), {}).data;
    check(profile.starts_with("PC28 1 2 64 "), "Versioned Warlock mask");
    profile.replace(0, 4, "PC26");
    rejects(
        [&]
        {
            (void)rules->create({{8, 8, std::vector<std::uint8_t>(64)},
                                 {{1, "campaign-character", "Forged", 0, {1, 1}, profile},
                                  {99, "vanguard", "Enemy", 1, {5, 1}}}},
                                13);
        });
    auto unselected = battle(*custom(), hero());
    check(!has(*unselected, "poison_spray"),
          "Having a class entitlement never silently grants an unselected cantrip");
}
void rolls()
{
    for (unsigned seed : {0u, 3u, 13u, 40u})
        for (const std::string defense : {"", "resistance", "vulnerability", "immunity"})
        {
            auto rules =
                custom(defense.empty() ? "" : "affinity target test " + defense + " poison\n");
            auto c = battle(*rules, selected(), seed);
            auto copy = rules->restore(c->save());
            const auto before = unit(*c);
            const auto random = rng(*c);
            const auto ticket = command(*c, "poison_spray", 2);
            check(c->submit(ticket) && copy->submit(ticket) && c->save() == copy->save(),
                  "Real Warlock cast continues identically after checkpoint");
            // Independent SplitMix64: seed 3 natural 10 and d12 12 (d10 would be 8).
            const int raw = seed == 0    ? 13
                            : seed == 3  ? 12
                            : seed == 13 ? 8
                                         : 0,
                      expected = defense == "immunity"        ? 0
                                 : defense == "resistance"    ? raw / 2
                                 : defense == "vulnerability" ? raw * 2
                                                              : raw;
            check(unit(*c, 2).hit_points == 1000 - expected,
                  "d12 Poison hit/miss/critical and typed defenses");
            check(arg(attack(*c), "bonus") == "6",
                  "Charisma 18 plus proficiency, not Intelligence 15");
            check(rng(*c) == random + 0x9e3779b97f4a7c15ULL * (seed == 0    ? 3u
                                                               : seed == 40 ? 1u
                                                                            : 2u),
                  "No extra save, damage modifier or random draw");
            const auto after = unit(*c);
            check(!after.action && after.bonus_action && after.reaction &&
                      after.movement_feet == before.movement_feet &&
                      after.persistent == before.persistent,
                  "Cantrip spends only its Magic action");
            const auto saved = c->save();
            check(!c->submit(ticket) && c->save() == saved, "Spent Action rejects atomically");
        }
}
void eligibility()
{
    auto rules = custom();
    for (int feet : {30, 35})
    {
        auto c = battle(*rules, selected(), 13, {1 + feet / 5, 1});
        check(has(*c, "poison_spray", 2) == (feet == 30), "Real Warlock range boundary");
        if (feet == 35)
        {
            const auto before = c->save();
            check(!c->submit({c->snapshot().revision, 1, 2, "poison_spray"}) && c->save() == before,
                  "Out-of-range rejection leaves all state intact");
        }
    }
    for (auto gear : std::vector<std::vector<std::string>>{
             {"quarterstaff", "shield"}, {"wand", "shield"}, {"plate"}})
    {
        auto c = battle(*rules, selected(), 13, {3, 1}, gear);
        const auto before = c->save();
        check(!has(*c, "poison_spray") &&
                  !c->submit({c->snapshot().revision, 1, 2, "poison_spray"}) && c->save() == before,
              "Somatic/untrained armor checks apply to Warlock grant");
    }
    auto c = battle(*rules, selected(), 13, {2, 1});
    check(c->submit(command(*c, "poison_spray", 2)) && unit(*c, 2).hit_points == 996 &&
              arg(attack(*c), "disadvantage") == " (disadvantage)",
          "Adjacent hostile gives ranged spell Disadvantage");
}
void campaign()
{
    auto rules = module();
    for (bool npc : {false, true})
    {
        CampaignParty party(module());
        auto h = selected(true);
        h.inventory().add("quarterstaff", "Quarterstaff");
        const auto id = npc ? party.recruit("fixture:warlock-poison", std::move(h))
                            : party.add_pc(std::move(h));
        party.equip(id, 1);
        party.set_grip(id, 2);
        auto state = party.checkpoint();
        state.roster[0].vitals.hit_points -= 2;
        state.roster[0].wealth[3] = 37;
        party.restore(state);
        auto actors = party.participants();
        actors[0].cell = {1, 1};
        actors.push_back({99, "vanguard", "Enemy", 1, {5, 1}});
        auto c = rules->create({{8, 8, std::vector<std::uint8_t>(64)}, actors}, 13);
        const auto old = unit(*c, id).persistent;
        check(c->submit(command(*c, "poison_spray", 99)),
              "Ordinary party profile provides Warlock cast");
        party.begin_combat();
        party.apply_combat(c->snapshot());
        party.end_combat();
        check(party.member(id).vitals == old, "Handoff preserves wounds and resources");
        auto saved = encode_campaign(party, nullptr, "warlock-poison");
        CampaignParty restored(module());
        restored.restore(
            decode_campaign(saved, *srd5::character_rules(), *rules, "warlock-poison", nullptr)
                .party);
        check(encode_campaign(restored, nullptr, "warlock-poison") == saved &&
                  restored.member(id).character.creation_data().cantrips ==
                      party.member(id).character.creation_data().cantrips,
              "Exact campaign reconstruction retains explicit choices");
        for (auto kind : {RestKind::short_rest, RestKind::long_rest})
        {
            check(bool(restored.rest(kind)), "Camp/inn rest");
            if (restored.state().short_rest)
                restored.finish_short_rest(restored.state().short_rest->ticket);
            saved = encode_campaign(restored, nullptr, "warlock-poison");
            CampaignParty again(module());
            again.restore(
                decode_campaign(saved, *srd5::character_rules(), *rules, "warlock-poison", nullptr)
                    .party);
            check(encode_campaign(again, nullptr, "warlock-poison") == saved &&
                      rules->spell_access(again.member(id).character.sheet()).cantrips.size() == 2,
                  "Rest save/reload preserves both learned cantrips");
        }
    }
}
void legacy()
{
    auto rules = module();
    auto base = root / "tests/fixtures";
    const auto prior = read(base / "campaign-v11-warlock-poison-before.ogs");
    CampaignParty party(module());
    party.restore(
        decode_campaign(prior, *srd5::character_rules(), *rules, "warlock-poison", nullptr).party);
    auto body = [](const std::string &s)
    {
        return s.substr(s.find('\n', s.find('\n') + 1) + 1);
    };
    auto expected = body(prior);
    expected.replace(expected.find("0.6.38"), 6, rules->identity().version);
    check(body(encode_campaign(party, nullptr, "warlock-poison")) == expected,
          "Actual old campaign preserves all fields except module identity/checksum");
    check(rules->spell_access(party.member(1).character.sheet()).cantrips.size() == 1,
          "Missing second choice stays pending on old Warlocks");
    auto upgraded = [&](const char *name)
    {
        auto s = read(base / name);
        s.replace(s.find("0.6.38"), 6, rules->identity().version);
        return s;
    };
    auto c = rules->restore(read(base / "combat-v13-warlock-poison-before.save"));
    check(c->save() == upgraded("combat-v13-warlock-poison-before.save") &&
              !has(*c, "poison_spray"),
          "Actual prior recipe and budgets retained");
    check(c->submit(command(*c, "end")) && c->submit(command(*c, "end")) &&
              c->submit(command(*c, "eldritch_blast", 99)),
          "Prior selected spell continues");
    check(c->save() == upgraded("combat-v13-warlock-poison-continued.save"),
          "Prior attack/RNG continuation exact");
}
void fixtures()
{
    auto rules = module();
    const auto path = std::filesystem::path(OPENGOLD_BINARY_DIR) / "eldritch-fixtures";
    auto c = rules->create({{12, 9, std::vector<std::uint8_t>(108)},
                            {{1,
                              "campaign-character",
                              "Warlock",
                              0,
                              {1, 1},
                              rules->character_profile(selected(true).sheet(), {}).data},
                             {2, "vanguard", "Ally", 0, {3, 1}},
                             {99, "vanguard", "Enemy", 1, {5, 1}}}},
                           2);
    write(path / "both.save", c->save());
}
void run()
{
    access();
    rolls();
    eligibility();
    campaign();
    legacy();
    fixtures();
}
} // namespace warlock_poison
