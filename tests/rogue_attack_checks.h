// Live rules evidence; included after normal creation and command helpers.
namespace rogue_attack_checks
{
using cunning_checks::act;
using cunning_checks::command;
using cunning_checks::has;

auto rules_module(std::string extra = {})
{
    std::ifstream in(std::filesystem::path(OPENGOLD_SOURCE_DIR) /
                     "data/rules/srd-5.2.1/combat.rules");
    std::string text{std::istreambuf_iterator<char>(in), {}};
    return srd5::parse_content(
               text + "\ncreature target 1 1000 0 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n" + extra);
}

CombatantView unit(const CombatSession &c, EntityId id = 1)
{
    for (const auto &a : c.snapshot().combatants)
        if (a.id == id)
            return a;
    throw std::runtime_error("Rogue test actor missing");
}

auto battle(const RulesModule &rules, const Character &h, std::string weapon = "dagger",
            bool ally = true, unsigned seed = 13, bool ranged = false, std::string enemy = "target")
{
    std::vector<std::string> gear;
    if (!weapon.empty())
        gear.push_back(weapon);
    const Cell target = ranged ? Cell{5, 1} :
                        Cell{2, 1};
    Encounter e{{12, 8, std::vector<std::uint8_t>(96)},
        {   {
                1,
                "campaign-character",
                "Rogue",
                0,
                {1, 1},
                rules.character_profile(h.sheet(), gear).data
            },
            {99, enemy, "Target", 1, target}
        }};
    if (ally)
        e.participants.push_back({2, "vanguard", "Ally", 0, {target.x, 2}});
    auto c = rules.create(std::move(e), seed);
    for (unsigned i = 0; c->snapshot().actor != 1 && i < 5; ++i)
        act(*c, "end");
    check(c->snapshot().actor == 1, "Rogue receives ordinary turn");
    return c;
}

void roundtrip(const RulesModule &r, const CombatSession &c)
{
    check(r.restore(c.save())->save() == c.save(), "Rogue choices/resources/RNG restore exactly");
}

void move(CombatSession &c, Cell to)
{
    for (const auto &v : c.legal_commands())
        if (v.verb == "move" && v.destination == to)
        {
            check(c.submit(v), "Rogue movement accepted");
            return;
        }
    throw std::runtime_error("Rogue move unavailable");
}

void reject_hit_field(const RulesModule &rules, const CombatSession &combat, unsigned field,
                      int value)
{
    auto bytes = combat.save();
    const auto start = bytes.find("\n1 99 ");
    check(start != std::string::npos, "Locate serialized pending hit");
    const auto end = bytes.find('\n', start + 1);
    std::istringstream in(bytes.substr(start + 1, end - start - 1));
    std::vector<int> fields;
    int n;
    while (in >> n)
        fields.push_back(n);
    check(fields.size() == ((bytes.starts_with("OGCOMBAT 22 ") || bytes.starts_with("OGCOMBAT 23 "))
                            ? 14u
                            : 12u) &&
          field < fields.size(),
          "Versioned pending hit field shape");
    fields[field] = value;
    std::ostringstream out;
    for (unsigned i = 0; i < fields.size(); ++i)
    {
        if (i)
            out << ' ';
        out << fields[i];
    }
    bytes.replace(start + 1, end - start - 1, out.str());
    rejects(
        [&]
    {
        (void)rules.restore(bytes);
    });
}

void run()
{
    const auto output = std::filesystem::path(OPENGOLD_BINARY_DIR) / "rogue-fixtures";
    std::filesystem::create_directories(output);
    auto write = [&](const std::string & name, const CombatSession & c)
    {
        std::ofstream out(output / (name + ".save"), std::ios::binary);
        out << c.save();
        check(bool(out), "Write current Rogue UI fixture");
    };
    auto rules = rules_module();
    auto d = draft("rogue", "soldier");
    d.training = choices();
    d.training["class:rogue:expertise"] = {"investigation", "perception"};
    d.training["background:soldier:gaming_set"] = {"dice"};
    auto h = hero(d);
    VitalState wounds{h.sheet().hit_points - 2};
    for (unsigned level = 1; level <= 4; ++level)
    {
        if (level > 1)
            check(h.advance(*rules, wounds), "Rogue advances normally through level four");
        check(h.sheet().level == level && wounds.hit_points == h.sheet().hit_points - 2,
              "Rogue advancement preserves wounds");
        auto c = battle(*rules, h);
        act(*c, "melee");
        const auto offer = c->snapshot().sneak_attack_choice;
        check(bool(offer), "Ally near target enables live Sneak hit");
        check(offer->dice_sides == 6 &&
              offer->dice_count == int(level < 3 ? 1 : 2) * (offer->critical ? 2 : 1),
              "Independent attained-level/critical extra dice");
        check(!unit(*c).action && unit(*c, 99).hit_points == 1000 &&
              !c->snapshot().savage_attack_choice,
              "Sneak decision precedes damage/Savage and preserves action cost");
        roundtrip(*rules, *c);
        reject_hit_field(*rules, *c, 11, 1);
        reject_hit_field(*rules, *c, 10, 1);
        reject_hit_field(*rules, *c, 8, 1);
        const auto before = c->save();
        check(!c->submit({c->snapshot().revision, 1, 0, "dash"}) && c->save() == before,
              "Other actions reject atomically while hit waits");
        auto skipped = rules->restore(before);
        act(*skipped, "sneak_skip");
        check(bool(skipped->snapshot().savage_attack_choice),
              "Declining Sneak still offers Savage");
        act(*skipped, "savage_skip");
        act(*c, "sneak_use");
        roundtrip(*rules, *c);
        auto hit = *c->snapshot().savage_attack_choice;
        reject_hit_field(*rules, *c, 11, 999);
        check(hit.extra_damage >= offer->dice_count && hit.extra_damage <= offer->dice_count * 6,
              "Extra dice have independent legal bounds");
        act(*c, "savage_use");
        roundtrip(*rules, *c);
        check(c->snapshot().savage_attack_choice->extra_damage == hit.extra_damage,
              "Savage rerolls weapon only");
        act(*c, "savage_first");
        check(unit(*skipped, 99).hit_points - unit(*c, 99).hit_points == hit.extra_damage,
              "Same first weapon roll gains exactly Sneak component");
        roundtrip(*rules, *c);
        auto bad = h.sheet();
        std::erase_if(bad.grants,
                      [](const auto & g)
        {
            return g.id == "feature:sneak_attack";
        });
        rejects(
            [&]
        {
            (void)rules->character_profile(bad, {});
        });
        if (level < 3)
        {
            c = battle(*rules, h);
            check(!has(*c, "steady_aim"), "Steady Aim absent below level three");
            continue;
        }
        c = battle(*rules, h, "shortbow", false, 13, true);
        check(has(*c, "steady_aim"), "Steady Aim offered at attained Rogue level three/four");
        act(*c, "steady_aim");
        check(!unit(*c).bonus_action && unit(*c).action && unit(*c).movement_feet == 0 &&
              c->movement_reach(1).empty(),
              "Aim spends only Bonus Action and makes Speed zero");
        roundtrip(*rules, *c);
        act(*c, "dash");
        check(unit(*c).movement_feet == 0, "Dash cannot restore movement under Steady Aim");
        act(*c, "end");
        while (c->snapshot().actor != 1)
            act(*c, "end");
        check(unit(*c).movement_feet == 30 && has(*c, "steady_aim"),
              "Next turn restores speed and Aim eligibility");
        move(*c, {2, 1});
        check(!has(*c, "steady_aim"), "Actual movement disables Aim");
        move(*c, {1, 1});
        check(!has(*c, "steady_aim"), "Returning to starting square does not restore Aim");
        roundtrip(*rules, *c);
        c = battle(*rules, h, "shortbow", false, 13, true);
        act(*c, "steady_aim");
        act(*c, "ranged");
        check(bool(c->snapshot().sneak_attack_choice),
              "Aim alone enables a ranged Sneak hit without ally");
        roundtrip(*rules, *c);
        act(*c, "sneak_use");
        act(*c, "savage_skip");
        roundtrip(*rules, *c);
    }
    for (unsigned level = 1; level <= 4; ++level)
    {
        CampaignParty recruited(rules_module());
        auto pc = hero(d);
        VitalState v;
        for (unsigned n = 2; n <= level; ++n)
            check(pc.advance(*rules, v), "Recruited Rogue ordinary advancement");
        pc.inventory().add("dagger", "Dagger");
        const auto id = recruited.recruit("fixture:rogue-attacks", std::move(pc));
        recruited.equip(id, 1);
        recruited.add_pc(hero(d));
        auto state = recruited.checkpoint();
        state.roster.front().vitals.hit_points -= 2;
        recruited.restore(state);
        auto actors = recruited.participants();
        actors.front().cell = {1, 1};
        actors[1].cell = {2, 2};
        actors.push_back({99, "target", "Target", 1, {2, 1}});
        auto c = rules->create({{8, 8, std::vector<std::uint8_t>(64)}, actors}, 13);
        while (c->snapshot().actor != id)
            act(*c, "end");
        const auto vitals = unit(*c, id).persistent;
        act(*c, "melee");
        check(c->snapshot().sneak_attack_choice.has_value(),
              "Recruited Rogue receives Sneak decision");
        act(*c, "sneak_use");
        act(*c, "savage_skip");
        recruited.begin_combat();
        recruited.apply_combat(c->snapshot());
        recruited.end_combat();
        check(recruited.member(id).vitals == vitals,
              "Recruited combat handoff preserves wounds/resources");
        auto saved = encode_campaign(recruited, nullptr, "recruited-rogue");
        CampaignParty restored(rules_module());
        restored.restore(
            decode_campaign(saved, *srd5::character_rules(), *rules, "recruited-rogue", nullptr)
            .party);
        check(encode_campaign(restored, nullptr, "recruited-rogue") == saved,
              "Recruited ownership/history and grants survive campaign reload");
    }
    // Aim's unused Advantage expires; an ordinary later hit without an ally cannot Sneak.
    {
        auto c = battle(*rules, h, "shortbow", false, 13, true);
        act(*c, "steady_aim");
        act(*c, "end");
        while (c->snapshot().actor != 1)
            act(*c, "end");
        act(*c, "ranged");
        check(c->snapshot().savage_attack_choice.has_value() && !c->snapshot().sneak_attack_choice,
              "Real later hit proves unused Aim expires at turn end");
        roundtrip(*rules, *c);
    }
    // An aimed miss consumes the attack-roll benefit but not its turn-long speed restriction.
    {
        auto hard =
            rules_module("creature hard_target 30 1000 0 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n");
        bool checked = false;
        for (unsigned seed = 1; seed <= 64 && !checked; ++seed)
        {
            auto c = battle(*hard, h, "shortbow", false, seed, true, "hard_target");
            act(*c, "steady_aim");
            act(*c, "ranged");
            if (unit(*c, 99).hit_points != 1000 || c->snapshot().sneak_attack_choice ||
                    c->snapshot().savage_attack_choice)
                continue;
            const auto saved = c->save();
            const auto start = saved.find("\n1 \"campaign-character\"");
            check(start != std::string::npos, "Locate saved aimed actor");
            const auto end = saved.find('\n', start + 1);
            std::istringstream actor_fields(saved.substr(start + 1, end - start - 1));
            std::string field;
            for (unsigned n = 0; n < 39; ++n)
                actor_fields >> std::quoted(field);
            bool sneak_used, aim_used, aim_ready, moved;
            actor_fields >> sneak_used >> aim_used >> aim_ready >> moved;
            check(actor_fields && !sneak_used && aim_used && !aim_ready && !moved,
                  "Miss clears saved Aim readiness while keeping Speed restriction");
            check(!unit(*c).action && !unit(*c).bonus_action && unit(*c).movement_feet == 0 &&
                  !has(*c, "steady_aim"),
                  "Miss preserves spent Action/Bonus Action and zero Speed");
            roundtrip(*hard, *c);
            checked = true;
        }
        check(checked, "Actual aimed miss exercised");
    }
    auto old_identity = rules->identity();
    old_identity.version = "0.6.51";
    rejects(
        [&]
    {
        auto state = wounds;
        rules->migrate_character_state(old_identity, h.sheet(), state);
    });
    check(!rules->advancement_options(h.sheet()).level, "This batch does not enable level five");
    for (const auto &weapon :
            {"dagger", "shortbow", "blowgun"
            })
    {
        const bool ranged = std::string_view(weapon) != "dagger";
        auto c = battle(*rules, h, weapon, true, 13, ranged);
        act(*c, ranged ? "ranged" : "melee");
        check(bool(c->snapshot().sneak_attack_choice),
              "Finesse and Ranged categories including fixed Blowgun qualify");
        roundtrip(*rules, *c);
        act(*c, "sneak_use");
        if (c->snapshot().savage_attack_choice)
            act(*c, "savage_skip");
        roundtrip(*rules, *c);
    }
    for (const auto &weapon :
            {"handaxe", ""
            })
    {
        auto c = battle(*rules, h, weapon);
        act(*c, "melee");
        check(!c->snapshot().sneak_attack_choice, "Non-Finesse and unarmed attacks do not qualify");
    }
    // Live cancellation cases: close ranged attacks have Disadvantage; Aim cancels it.
    for (bool ally :
            {
                false, true
            })
        for (bool aim :
                {
                    false, true
                })
        {
            bool checked = false;
            for (unsigned seed = 1; seed <= 64 && !checked; ++seed)
            {
                auto c = battle(*rules, h, "shortbow", ally, seed, false);
                if (aim)
                    act(*c, "steady_aim");
                act(*c, "ranged");
                if (!c->snapshot().sneak_attack_choice && !c->snapshot().savage_attack_choice)
                    continue;
                check(
                    c->snapshot().sneak_attack_choice.has_value() == (ally && aim),
                    "Actual canceled Advantage permits ally clause; uncanceled Disadvantage never does");
                roundtrip(*rules, *c);
                checked = true;
            }
            check(checked, "Live opposed-roll hit exercised");
        }
    bool critical_verified = false;
    for (unsigned seed = 1; seed <= 128 && !critical_verified; ++seed)
    {
        auto c = battle(*rules, h, "dagger", true, seed);
        act(*c, "melee");
        const auto offered = c->snapshot().sneak_attack_choice;
        if (!offered || !offered->critical)
            continue;
        check(offered->dice_count == 4, "Level-four critical rolls four extra d6");
        roundtrip(*rules, *c);
        act(*c, "sneak_use");
        act(*c, "savage_use");
        roundtrip(*rules, *c);
        critical_verified = true;
    }
    check(critical_verified, "Actual critical Sneak hit exercised");
    // Naturally sleeping adjacent allies are living but Incapacitated.
    {
        VitalState sleep{h.sheet().hit_points};
        rules->set_rest_work(sleep, h.sheet(), RestWork::sleep);
        bool checked = false;
        for (unsigned seed = 1; seed <= 64 && !checked; ++seed)
        {
            Encounter e
            {
                {8, 8, std::vector<std::uint8_t>(64)},
                {   {
                        1,
                        "campaign-character",
                        "Rogue",
                        0,
                        {1, 1},
                        rules->character_profile(h.sheet(), std::array<std::string, 1>{"dagger"}).data
                    },
                    {
                        2,
                        "campaign-character",
                        "Sleeping ally",
                        0,
                        {2, 2},
                        rules->character_profile(h.sheet(), {}).data,
                        sleep
                    },
                    {99, "target", "Target", 1, {2, 1}}
                }};
            auto c = rules->create(std::move(e), seed);
            while (c->snapshot().actor != 1)
                act(*c, "end");
            act(*c, "melee");
            check(!c->snapshot().sneak_attack_choice,
                  "Sleeping adjacent ally cannot enable Sneak Attack");
            checked = c->snapshot().savage_attack_choice.has_value();
        }
        check(checked, "Hit with incapacitated ally exercised");
    }
    // Real inventory throws retain the selected weapon while the held item lands.
    for (const auto *weapon :
            {"dagger", "dart", "handaxe"
            })
    {
        bool checked = false;
        for (unsigned seed = 1; seed <= 64 && !checked; ++seed)
        {
            CampaignParty party(rules_module());
            auto pc = h;
            const auto token = pc.inventory().add(weapon, weapon);
            const auto id = party.add_pc(std::move(pc));
            party.equip(id, token);
            auto actors = party.participants();
            actors.front().cell = {1, 1};
            actors.push_back({99, "target", "Target", 1, {5, 1}});
            actors.push_back({2, "vanguard", "Ally", 0, {5, 2}});
            auto c = rules->create({{12, 8, std::vector<std::uint8_t>(96)}, actors}, seed);
            while (c->snapshot().actor != id)
                act(*c, "end");
            act(*c, "throw");
            if (!c->snapshot().sneak_attack_choice && !c->snapshot().savage_attack_choice)
                continue;
            check(c->snapshot().sneak_attack_choice.has_value() ==
                  (std::string_view(weapon) != "handaxe"),
                  "Thrown Finesse/Ranged weapon qualifies; thrown Handaxe does not");
            roundtrip(*rules, *c);
            if (c->snapshot().sneak_attack_choice)
                act(*c, "sneak_use");
            act(*c, "savage_skip");
            roundtrip(*rules, *c);
            checked = true;
        }
        check(checked, "Real thrown weapon hit exercised");
    }
    // A real opportunity hit gets a fresh once-per-turn use during the enemy turn.
    bool reaction_verified = false;
    for (unsigned seed = 1; seed <= 64 && !reaction_verified; ++seed)
    {
        auto c = battle(*rules, h, "dagger", true, seed);
        act(*c, "melee");
        if (!c->snapshot().sneak_attack_choice)
            continue;
        act(*c, "sneak_use");
        act(*c, "savage_skip");
        act(*c, "end");
        while (c->snapshot().actor != 99)
            act(*c, "end");
        move(*c, {4, 1});
        while (c->snapshot().reaction_pending && c->snapshot().actor != 1)
            act(*c, "decline");
        check(c->snapshot().actor == 1 && c->snapshot().reaction_pending,
              "Enemy movement offers actual Rogue opportunity attack");
        act(*c, "opportunity");
        if (!c->snapshot().sneak_attack_choice)
            continue;
        check(!unit(*c).reaction && unit(*c, 99).cell == Cell{2, 1},
              "Reaction is spent and enemy movement suspended for Sneak decision");
        roundtrip(*rules, *c);
        auto restored = rules->restore(c->save());
        for (auto *session :
                {
                    c.get(), restored.get()
                })
        {
            act(*session, "sneak_use");
            act(*session, "savage_skip");
            while (session->snapshot().reaction_pending)
                act(*session, "decline");
        }
        check(c->save() == restored->save() && unit(*c, 99).cell == Cell{4, 1},
              "Reaction Sneak damage and interrupted movement continue identically");
        reaction_verified = true;
    }
    check(reaction_verified, "Real second-turn opportunity Sneak case exercised");
    // Signed weapon damage and extra dice combine before the single resistance floor.
    auto resisted = rules_module("affinity target ward resistance piercing\n");
    auto weak = d;
    for (auto &r : weak.rolls)
        r = {{1, 1, 1, 1}, 3};
    auto weak_hero = hero(weak);
    bool negative_verified = false;
    for (unsigned seed = 1; seed <= 64 && !negative_verified; ++seed)
    {
        auto c = battle(*resisted, weak_hero, "dagger", true, seed);
        act(*c, "melee");
        if (!c->snapshot().sneak_attack_choice)
            continue;
        act(*c, "sneak_use");
        const auto hit = *c->snapshot().savage_attack_choice;
        if (hit.first_damage >= 0)
            continue;
        roundtrip(*resisted, *c);
        act(*c, "savage_skip");
        check(unit(*c, 99).hit_points ==
              1000 - std::max(0, hit.first_damage + hit.extra_damage) / 2,
              "Signed weapon plus Sneak damage is resisted once");
        negative_verified = true;
    }
    check(negative_verified, "Negative weapon component exercised in live combat");
    auto live = module();
    auto aimed = battle(*live, h, "shortbow", false, 13, true, "vanguard");
    write("aim-available", *aimed);
    for (const auto *verb :
            {"cunning_dash", "cunning_disengage"
            })
    {
        auto bonus = live->restore(aimed->save());
        act(*bonus, verb);
        write(verb, *bonus);
    }
    act(*aimed, "steady_aim");
    write("aim-spent", *aimed);
    auto ui_hero = hero(d);
    VitalState ui_vitals;
    for (unsigned level = 1; level <= 4; ++level)
    {
        if (level > 1)
            check(ui_hero.advance(*live, ui_vitals), "UI Rogue advances normally");
        bool written = false;
        for (unsigned seed = 1; seed <= 64 && !written; ++seed)
        {
            auto c = battle(*live, ui_hero, "dagger", true, seed, false, "vanguard");
            act(*c, "melee");
            if (!c->snapshot().sneak_attack_choice)
                continue;
            write("sneak-level" + std::to_string(level), *c);
            act(*c, "sneak_use");
            write("savage-extra-level" + std::to_string(level), *c);
            written = true;
        }
        check(written, "Current shipped-content hit fixture captured");
    }
    CampaignParty party(module());
    auto id = party.add_pc(hero(d));
    party.award_experience(2700, "rogue-batch");
    for (unsigned n = 2; n <= 4; ++n)
        party.advance(id, party.default_advancement(id));
    const auto bytes = encode_campaign(party, nullptr, "rogue-attacks");
    CampaignParty copy(module());
    copy.restore(
        decode_campaign(bytes, *srd5::character_rules(), *module(), "rogue-attacks", nullptr)
        .party);
    check(encode_campaign(copy, nullptr, "rogue-attacks") == bytes,
          "XP-driven Rogue level-four campaign replay is exact");
    check(bool(copy.rest(RestKind::short_rest)), "Rogue Short Rest completes");
    copy.finish_short_rest(copy.state().short_rest->ticket);
    check(bool(copy.rest(RestKind::long_rest)), "Rogue Long Rest completes");
    rejects(
        [&]
    {
        (void)decode_campaign(corrupt(bytes, module()->identity().version, "0.6.51"),
        *srd5::character_rules(), *module(), "rogue-attacks", nullptr);
    });
    if (const auto *directory = std::getenv("OPENGOLD_GAME_DIR"); directory && *directory)
    {
        CampaignParty ui(module());
        const auto ui_id = ui.add_pc(hero(d));
        ui.award_experience(2700, "rogue-ui");
        auto state = ui.checkpoint();
        state.roster.front().vitals.hit_points -= 2;
        ui.restore(std::move(state));
        write_campaign_file(std::filesystem::path(OPENGOLD_BINARY_DIR) / "rogue-advancement-ui.ogs",
                            encode_campaign(ui, nullptr, campaign_asset_identity(directory)));
    }
}

void verify_ui(const char *file)
{
    const auto *directory = std::getenv("OPENGOLD_GAME_DIR");
    check(directory && *directory, "Rogue UI comparison requires game assets");
    auto rules = module();
    auto creation = srd5::character_rules();
    const auto assets = campaign_asset_identity(directory);
    CampaignParty expected(module());
    expected.restore(decode_campaign(read_campaign_file(std::filesystem::path(OPENGOLD_BINARY_DIR) /
            "rogue-advancement-ui.ogs"),
                                     *creation, *rules, assets, nullptr)
                     .party);
    for (unsigned level = 2; level <= 4; ++level)
        expected.advance(1, expected.default_advancement(1));
    CampaignParty actual(module());
    actual.restore(
        decode_campaign(read_campaign_file(file), *creation, *rules, assets, nullptr).party);
    auto state = expected.checkpoint();
    state.selected = actual.state().selected;
    expected.restore(std::move(state));
    check(
        encode_campaign(actual, nullptr, assets) == encode_campaign(expected, nullptr, assets),
        "Rogue UI advancement matches native HP, grants, feat, wounds, equipment and history exactly");
    std::cout << "Rogue UI persistence verified\n";
}

} // namespace rogue_attack_checks
