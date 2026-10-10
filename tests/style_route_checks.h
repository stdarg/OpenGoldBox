// Independent public creation/advancement/combat/save acceptance for #78/#79/#80.
namespace style_route_checks
{
using cunning_checks::act;
using rogue_attack_checks::unit;

Character starter(std::string klass, std::string style = "defense")
{
    auto d = draft(klass, "soldier");
    const auto creation = srd5::character_rules();
    for (const auto &group : creation->training_options(d))
        for (unsigned i = 0; i < group.count; ++i)
            d.training[group.id].push_back(group.options.at(i).id);
    if (klass == "fighter")
        d.training["class:fighter:fighting_style"] = {style};
    return hero(d);
}

Character leveled(const RulesModule &rules, std::string klass, std::string style, unsigned level)
{
    auto h = starter(klass, style);
    VitalState v{h.sheet().hit_points - 2};
    for (unsigned n = 2; n <= level; ++n)
    {
        auto choice = rules.default_advancement(h.sheet());
        if (klass != "fighter" && n == 2)
            choice.fighting_style = style;
        check(h.advance(rules, v, choice), "Ordinary style-source advancement");
        check(v.hit_points == h.sheet().hit_points - 2, "Style advancement preserves wounds");
    }
    return h;
}

auto combat(const RulesModule &r, const Character &h, std::vector<std::string> gear,
            unsigned seed = 13, bool ranged = false)
{
    auto profile = r.character_profile(h.sheet(), gear);
    auto c = r.create({{12, 8, std::vector<Terrain>(96)},
        {   {1, "campaign-character", "Style tester", 0, {1, 1}, profile.data},
            {99, "target", "Target", 1, ranged ? Cell{5, 1} : Cell{2, 1}}
        }},
    seed);
    while (c->snapshot().actor != 1)
        act(*c, "end");
    return c;
}

int die(std::uint64_t &rng, unsigned sides)
{
    rng += 0x9e3779b97f4a7c15ULL;
    auto x = rng;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return int((x ^ (x >> 31)) % sides) + 1;
}

std::uint64_t random_state(const CombatSession &c)
{
    std::istringstream in(c.save());
    std::string line;
    for (int i = 0; i < 3; ++i)
        std::getline(in, line);
    std::uint64_t rng{};
    in >> rng;
    return rng;
}

void run()
{
    auto rules = rogue_attack_checks::rules_module();
    const auto creation = srd5::character_rules();
    for (const auto *klass :
            {"fighter", "paladin", "ranger"
            })
        for (const auto *style :
                {"archery", "defense", "great_weapon_fighting"
                })
        {
            for (unsigned level = 1; level <= 4; ++level)
            {
                auto h = leveled(*rules, klass, style, level);
                const bool entitled = std::string_view(klass) == "fighter" || level >= 2;
                const auto source = "class:" + std::string(klass) + ":fighting_style";
                check(std::any_of(h.sheet().grants.begin(), h.sheet().grants.end(),
                                  [&](const auto & g)
                {
                    return g.source_id == source &&
                           g.id == "feat:" + std::string(style);
                }) == entitled,
                "Class and attained-level entitlement is real");
                const auto armor =
                    rules->character_profile(h.sheet(), std::array<std::string, 1> {"breastplate"});
                auto no_style = h.sheet();
                if (entitled)
                    for (auto &g : no_style.grants)
                        if (g.source_id == source)
                            g.id = "feat:great_weapon_fighting";
                check(armor.armor_class ==
                      rules->character_profile(no_style,
                                               std::array<std::string, 1> {"breastplate"})
                      .armor_class +
                      (entitled && std::string_view(style) == "defense" ? 1 : 0),
                      "Defense contributes exactly one armored AC");
                check(rules->character_profile(h.sheet(), std::array<std::string, 1> {"shield"})
                      .armor_class ==
                      rules->character_profile(no_style, std::array<std::string, 1> {"shield"})
                      .armor_class,
                      "Shield alone never activates Defense");
                auto c = combat(*rules, h, {"shortbow"}, 13, true);
                act(*c, "ranged");
                bool attack = false;
                for (const auto &m : c->snapshot().log_messages())
                    if (m.source.starts_with("{actor} -> {target}: d20"))
                        for (const auto &a : m.arguments)
                            if (a.name == "bonus")
                            {
                                check(std::stoi(a.value) ==
                                      h.sheet().modifiers[1] + 2 +
                                      (entitled && std::string_view(style) == "archery"
                                       ? 2
                                       : 0),
                                      "Archery is sourced at every attained level");
                                attack = true;
                            }
                check(attack, "Actual ranged attack checked");
                check(rules->restore(c->save())->save() == c->save(),
                      "Style combat continuation exact");
                if (level < 4)
                {
                    auto invalid = rules->default_advancement(h.sheet());
                    invalid.fighting_style = "unknown";
                    auto before = h.sheet();
                    VitalState v;
                    rejects(
                        [&]
                    {
                        h.advance(*rules, v, invalid);
                    });
                    check(h.sheet().grants == before.grants, "Invalid style choice is atomic");
                }
            }
        }

    // Exact die oracle covers all eligible two-hand shapes, grip changes, criticals and Savage.
    struct Weapon
    {
        const char *id;
        int count, sides;
        bool shield; // A shield in the other hand keeps a Versatile weapon one-handed.
        bool eligible;
        bool ranged{};
    };

    const std::array weapons
    {
        Weapon{"greatsword", 2, 6, false, true},  Weapon{"greataxe", 1, 12, false, true},
        Weapon{"maul", 2, 6, false, true},        Weapon{"longsword", 1, 10, false, true},
        Weapon{"longsword", 1, 8, true, false},   Weapon{"spear", 1, 8, false, true},
        Weapon{"dagger", 1, 4, false, false},     Weapon{"shortbow", 1, 6, false, false, true},
        Weapon{"dart", 1, 4, false, false, true}};
    for (const auto *klass :
            {"fighter", "paladin", "ranger"
            })
        for (unsigned level = std::string_view(klass) == "fighter" ? 1 : 2; level <= 4; ++level)
        {
            auto h = leveled(*rules, klass, "great_weapon_fighting", level);
            for (const auto &w : weapons)
            {
                bool critical_seen = false;
                for (unsigned seed = 1; seed <= 128; ++seed)
                {
                    std::vector<std::string> gear{w.id};
                    if (w.shield)
                        gear.emplace_back("shield");
                    auto c = combat(*rules, h, gear, seed, w.ranged);
                    auto rng = random_state(*c);
                    const int natural = die(rng, 20);
                    const bool critical = natural == 20 || (std::string_view(klass) == "fighter" &&
                                                            level >= 3 && natural == 19);
                    act(*c, w.ranged ? "ranged" : "melee");
                    if (natural == 1)
                    {
                        check(!test::logged(*c, "(Savage Attacker)"),
                              "Natural-one miss never rolls damage");
                        continue;
                    }
                    const int modifier =
                        (std::string_view(w.id) == "dagger" || std::string_view(w.id) == "dart")
                        ? std::max(h.sheet().modifiers[0], h.sheet().modifiers[1])
                        : h.sheet().modifiers[w.ranged ? 1 : 0];
                    const auto roll = [&]
                    {
                        int total = modifier;
                        for (int i = 0; i < w.count * (critical ? 2 : 1); ++i)
                        {
                            const int face = die(rng, w.sides);
                            total += w.eligible ? std::max(3, face) : face;
                        }
                        return total;
                    };
                    const int first = roll(), second = roll();
                    const auto savage = "(Savage Attacker): " + std::to_string(first) + " and " +
                                        std::to_string(second) + ", keeps " +
                                        std::to_string(std::max(first, second)) + ".";
                    if (!test::logged(*c, savage) ||
                            (rogue_attack_checks::last_hit(*c) == "CRITICAL") != critical)
                        std::cerr << klass << " level=" << level << " weapon=" << w.id
                                  << " seed=" << seed << " natural=" << natural
                                  << " expected=" << savage << " log=" << c->snapshot().log().back()
                                  << "\n";
                    check(test::logged(*c, savage) &&
                          (rogue_attack_checks::last_hit(*c) == "CRITICAL") == critical,
                          "Both GWF rolls match independent dice without extra RNG");
                    check(rules->restore(c->save())->save() == c->save(),
                          "GWF hit continuation preserves independent RNG");
                    check(unit(*c, 99).hit_points == 1000 - std::max(first, second),
                          "Higher GWF damage applied once");
                    if (critical)
                    {
                        critical_seen = true;
                        break;
                    }
                }
                check(critical_seen, "Actual critical GWF roll exercised");
            }
        }
    for (const auto *klass :
            {"fighter", "paladin", "ranger"
            })
    {
        auto h = leveled(*rules, klass, "great_weapon_fighting",
                         std::string_view(klass) == "fighter" ? 1 : 2);
        bool reacted = false, threw = false;
        for (unsigned seed = 1; seed <= 64 && (!reacted || !threw); ++seed)
        {
            if (!reacted)
            {
                auto c = combat(*rules, h, {"greatsword"}, seed);
                while (c->snapshot().actor != 99)
                    act(*c, "end");
                Command move;
                for (const auto &cmd : c->legal_commands())
                    if (cmd.verb == "move" && cmd.destination == Cell{3, 1})
                        move = cmd;
                check(move.actor == 99 && c->submit(move) && c->snapshot().reaction_pending,
                      "Movement provokes actual style reaction");
                auto rng = random_state(*c);
                const auto natural = die(rng, 20);
                act(*c, "opportunity");
                if (natural != 1)
                {
                    int expected = h.sheet().modifiers[0];
                    for (int i = 0; i < (natural == 20 ? 4 : 2); ++i)
                        expected += std::max(3, die(rng, 6));
                    check(test::logged(*c, "(Savage Attacker): " + std::to_string(expected) + " and "),
                          "GWF applies once to actual opportunity damage");
                    check(rules->restore(c->save())->save() == c->save() && !unit(*c).reaction &&
                          unit(*c).action && unit(*c, 99).cell == Cell{3, 1},
                          "Reaction restores exactly and spends only Reaction");
                    reacted = true;
                }
            }
            if (!threw)
            {
                CampaignParty p(rogue_attack_checks::rules_module());
                auto thrower = h;
                thrower.add_item({.definition_id = "spear", .name = "Spear"});
                p.add_pc(std::move(thrower));
                p.equip(1, 1);
                auto actors = p.participants();
                actors.front().cell = {1, 1};
                actors.push_back({99, "target", "Target", 1, {4, 1}});
                auto c = rules->create({{8, 8, std::vector<Terrain>(64)}, actors}, seed);
                while (c->snapshot().actor != 1)
                    act(*c, "end");
                auto rng = random_state(*c);
                const auto natural = die(rng, 20);
                act(*c, "throw");
                if (natural != 1)
                {
                    int expected = h.sheet().modifiers[0];
                    for (int i = 0; i < (natural == 20 ? 2 : 1); ++i)
                        expected += die(rng, 6);
                    check(test::logged(*c, "(Savage Attacker): " + std::to_string(expected) + " and "),
                          "Throwing two-handed spear uses normal d6 faces, no GWF or Versatile bonus");
                    check(rules->restore(c->save())->save() == c->save(),
                          "Thrown GWF-owner checkpoint is valid");
                    threw = true;
                }
            }
        }
        check(reacted && threw, "Every source class exercises reaction and thrown exclusion");
    }
    for (const auto *klass :
            {"fighter", "paladin", "ranger"
            })
        for (const auto *feat :
                {"defense", "great_weapon_fighting"
                })
        {
            auto h = leveled(*rules, klass, "archery", 3);
            auto choice = rules->default_advancement(h.sheet());
            choice.feat = feat;
            choice.abilities = {};
            VitalState v{h.sheet().hit_points - 2};
            auto invalid = choice;
            invalid.feat = "archery";
            const auto before = h.sheet().grants;
            rejects(
                [&]
            {
                h.advance(*rules, v, invalid);
            });
            check(h.sheet().grants == before, "Independent feat cannot duplicate owned style");
            check(h.advance(*rules, v, choice),
                  "Each style source also supports independent level-four feats");
            check(std::any_of(h.sheet().grants.begin(), h.sheet().grants.end(),
                              [&](const auto & g)
            {
                return g.id == "feat:" + std::string(feat) &&
                       g.source_id == "class:" + std::string(klass) +
                       ":ability_score_improvement" &&
                       g.level == 4;
            }),
            "Style feat has its actual independent acquisition source");
        }
    // Replacement frees only the class entitlement, even alongside the level-four feat.
    {
        auto h = leveled(*rules, "fighter", "defense", 3);
        auto choice = rules->default_advancement(h.sheet());
        choice.fighting_style = "archery";
        choice.feat = "defense";
        choice.abilities = {};
        VitalState v{h.sheet().hit_points - 2};
        check(
            h.advance(*rules, v, choice),
            "Fighter can replace starting Defense and independently acquire Defense at level four");
        check(std::count_if(h.sheet().grants.begin(), h.sheet().grants.end(),
                            [](const auto & g)
        {
            return g.id == "feat:defense";
        }) == 1,
        "No duplicate Defense effect");
    }
    for (const auto *klass :
            {"fighter", "paladin", "ranger"
            })
        for (bool npc :
                {
                    false, true
                })
            for (const auto *style :
                    {"archery", "defense", "great_weapon_fighting"
                    })
            {
                CampaignParty p(rogue_attack_checks::rules_module());
                auto h = starter(klass);
                h.add_item({.definition_id = "greatsword", .name = "Greatsword"});
                const auto id =
                    npc ? p.recruit("fixture:styles", std::move(h)) : p.add_pc(std::move(h));
                p.equip(id, 1);
                p.award_experience(2700, "style-routes");
                for (unsigned level = 2; level <= 4; ++level)
                {
                    auto choice = p.default_advancement(id);
                    if (level == 2)
                        choice.fighting_style = std::string_view(klass) == "fighter" &&
                                                std::string_view(style) == "defense"
                                                ? "archery"
                                                : style;
                    p.advance(id, choice);
                    auto actors = p.participants();
                    actors.front().cell = {1, 1};
                    actors.push_back({99, "target", "Target", 1, {2, 1}});
                    auto fight = rules->create({{8, 8, std::vector<Terrain>(64)}, actors}, 13);
                    while (fight->snapshot().actor != id)
                        act(*fight, "end");
                    act(*fight, "melee");
                    if (fight->snapshot().free_movement)
                        act(*fight, "end");
                    p.begin_combat();
                    p.apply_combat(fight->snapshot());
                    p.end_combat();
                }
                const auto saved = encode_campaign(p, nullptr, "style-routes");
                check(saved.starts_with("OPENGOLD-CAMPAIGN 25\n"),
                      "Style history is written in the current campaign format");
                CampaignParty restored(rogue_attack_checks::rules_module());
                restored.restore(
                    decode_campaign(saved, *creation, *rules, "style-routes", nullptr).party);
                check(encode_campaign(restored, nullptr, "style-routes") == saved,
                      "All class/ownership histories replay exactly");
                check(bool(restored.rest(RestKind::short_rest)), "Style character Short Rest");
                restored.finish_short_rest(restored.state().short_rest->ticket);
                check(bool(restored.rest(RestKind::long_rest)), "Style character Long Rest");
            }

    for (const auto *klass :
            {"paladin", "ranger"
            })
    {
        auto h = leveled(*rules, klass, "archery", 2);
        auto bad = h.sheet();
        std::erase_if(bad.grants,
                      [&](const auto & g)
        {
            return g.source_id == "class:" + std::string(klass) + ":fighting_style";
        });
        rejects(
            [&]
        {
            (void)rules->character_profile(bad, {});
        });
    }
    if (const auto *directory = std::getenv("OPENGOLD_GAME_DIR"); directory && *directory)
        for (const auto *klass :
                {"fighter", "paladin", "ranger"
                })
        {
            CampaignParty ui(module());
            ui.add_pc(starter(klass));
            ui.award_experience(2700, "styles-ui");
            auto state = ui.checkpoint();
            state.roster.front().vitals.hit_points -= 2;
            ui.restore(std::move(state));
            write_campaign_file(std::filesystem::path(OPENGOLD_BINARY_DIR) /
                                (std::string("style-") + klass + "-ui.ogs"),
                                encode_campaign(ui, nullptr, campaign_asset_identity(directory)));
        }
}

void verify_ui(const char *klass, const char *file, bool light = false)
{
    const auto *directory = std::getenv("OPENGOLD_GAME_DIR");
    check(directory && *directory, "Style UI comparison requires game assets");
    auto rules = module();
    auto creation = srd5::character_rules();
    const auto assets = campaign_asset_identity(directory);
    CampaignParty expected(module());
    expected.restore(
        decode_campaign(read_campaign_file(std::filesystem::path(OPENGOLD_BINARY_DIR) /
                                           (std::string("style-") + klass + "-ui.ogs")),
                        *creation, *rules, assets, nullptr)
        .party);
    for (unsigned level = 2; level <= 4; ++level)
    {
        auto choice = expected.default_advancement(1);
        if (level == 2)
            choice.fighting_style = light ? "two_weapon_fighting" : "great_weapon_fighting";
        if (std::string_view(klass) == "fighter" && level == 3)
            choice.fighting_style = "archery";
        if (std::string_view(klass) == "fighter" && level == 4)
        {
            choice.fighting_style = light ? "two_weapon_fighting" : "defense";
            choice.feat = "archery";
            choice.abilities = {};
        }
        expected.advance(1, choice);
    }
    CampaignParty actual(module());
    actual.restore(
        decode_campaign(read_campaign_file(file), *creation, *rules, assets, nullptr).party);
    auto state = expected.checkpoint();
    state.selected = actual.state().selected;
    expected.restore(std::move(state));
    check(encode_campaign(actual, nullptr, assets) == encode_campaign(expected, nullptr, assets),
          "Style UI advancement exactly matches native grants, HP, wounds, resources and history");
    std::cout << "Style UI persistence verified\n";
}
} // namespace style_route_checks
