// Public API acceptance for actual Light weapon identities and action budgets.
namespace light_attack_checks
{
using rogue_attack_checks::unit;

auto rules()
{
    return rogue_attack_checks::rules_module();
}

Character hero_for(bool feat = false, bool negative = false, std::string klass = "fighter")
{
    auto d = draft(klass, "soldier");
    if (negative)
        for (auto &roll : d.rolls)
            roll = {{2, 2, 1, 1}, 3};
    if (klass == "wizard")
        d.cantrips = std::vector<std::string> {"fire_bolt"};
    auto creation = srd5::character_rules();
    for (const auto &group : creation->training_options(d))
        for (unsigned i = 0; i < group.count; ++i)
            d.training[group.id].push_back(group.options.at(i).id);
    if (klass == "rogue")
    {
        d.training = choices();
        d.training["class:rogue:expertise"] = {"investigation", "perception"};
    }
    if (klass == "fighter")
        d.training["class:fighter:fighting_style"] = {feat ? "two_weapon_fighting" : "defense"};
    return hero(d);
}

CampaignParty party(bool feat = false, bool negative = false, bool npc = false)
{
    CampaignParty p(rules());
    auto h = hero_for(feat, negative);
    h.add_item({.definition_id = "dagger", .name = "Dagger"});
    h.add_item({.definition_id = "dagger", .name = "Dagger"});
    h.add_item({.definition_id = "dagger", .name = "Dagger", .quantity = 3});
    h.add_item({.definition_id = "hand_crossbow", .name = "Hand Crossbow"});
    h.add_item({.definition_id = "hand_crossbow", .name = "Hand Crossbow"});
    h.add_item({.definition_id = "shield", .name = "Shield"});
    const auto id = npc ? p.recruit("light:npc", h) : p.add_pc(h);
    p.equip(id, 1);
    p.equip(id, 2, EquipmentOperation::equip_other);
    return p;
}

auto battle(CampaignParty &p, unsigned seed = 13, Cell target = {2, 1}, int target_hp = 1000)
{
    auto actors = p.participants();
    actors.front().cell = {1, 1};
    actors.push_back({99, "target", "Target", 1, target, {}, VitalState{target_hp}});
    auto c = p.rule_module().create({{12, 8, std::vector<std::uint8_t>(96)}, actors}, seed);
    for (unsigned n = 0; c->snapshot().actor != 1 && n < 5; ++n)
        cunning_checks::act(*c, "end");
    check(c->snapshot().actor == 1, "Ordinary actor turn");
    return c;
}

void act(CombatSession &c, std::string_view verb, unsigned item = 0)
{
    for (const auto &cmd : c.legal_commands())
        if (cmd.verb == verb && (!item || cmd.item == item))
        {
            check(c.submit(cmd), "Legal Light command executes");
            return;
        }
    throw std::runtime_error("Missing Light command: " + std::string(verb));
}

bool offered(const CombatSession &c, std::string_view verb, unsigned item = 0)
{
    for (const auto &cmd : c.legal_commands())
        if (cmd.verb == verb && (!item || cmd.item == item))
            return true;
    return false;
}

void settle(CombatSession &c)
{
    for (unsigned n = 0; n < 8; ++n)
    {
        const auto s = c.snapshot();
        if (s.free_movement)
            act(c, "end");
        else
            return;
    }
    throw std::runtime_error("Unfinished Light damage choice");
}

// The damage roll is not reported, so bound the latest hit by a dagger's d4s.
bool dagger_damage_within(const CombatSession &c, int modifier)
{
    const auto hit = rogue_attack_checks::last_hit(c);
    const int count = hit == "CRITICAL" ? 2 : 1;
    const auto messages = c.snapshot().log_messages();
    for (auto m = messages.rbegin(); m != messages.rend(); ++m)
        if (m->source.starts_with("{actor} -> {target}: d20"))
            for (const auto &a : m->arguments)
                if (a.name == "damage")
                {
                    const int damage = std::stoi(a.value);
                    return damage >= std::max(0, count + modifier) &&
                           damage <= std::max(0, count * 4 + modifier);
                }
    return false;
}

void exact_restore(const CombatSession &c)
{
    auto restored = rules()->restore(c.save());
    check(restored->save() == c.save(), "Light state restores exactly");
}

void ui_fixtures()
{
    auto rules = srd5::load(std::filesystem::path(OPENGOLD_SOURCE_DIR) /
                            "data/rules/srd-5.2.1/combat.rules");
    auto p = party(true);
    auto actors = p.participants();
    actors.front().cell = {1, 1};
    actors.push_back({99, "vanguard", "Target", 1, {2, 1}});
    const auto path = std::filesystem::path(OPENGOLD_BINARY_DIR) / "light-fixtures";
    std::filesystem::create_directories(path);
    const auto write = [&](const char *name, const CombatSession & c)
    {
        std::ofstream out(path / name, std::ios::binary);
        out << c.save();
        check(bool(out), "Write actual Light UI fixture");
    };
    auto c = rules->create({{12, 8, std::vector<std::uint8_t>(96)}, actors}, 13);
    while (c->snapshot().actor != 1)
        act(*c, "end");
    write("before.save", *c);
    act(*c, "weapon_select", 2);
    write("selected.save", *c);
    act(*c, "melee");
    settle(*c);
    write("qualified.save", *c);
    act(*c, "light_melee", 1);
    settle(*c);
    write("after.save", *c);
}

void run()
{
    ui_fixtures();
    unsigned hits = 0, criticals = 0;
    for (bool feat :
            {
                false, true
            })
        for (bool negative :
                {
                    false, true
                })
            for (unsigned seed = 1; seed <= 32; ++seed)
            {
                auto p = party(feat, negative, seed % 2);
                auto c = battle(p, seed);
                check(!offered(*c, "light_melee"), "Light cannot precede an Attack action");
                act(*c, "melee");
                if (!rogue_attack_checks::last_hit(*c).empty())
                    check(dagger_damage_within(*c, negative ? -2 : 3),
                          "Style does not double normal attack modifiers");
                settle(*c);
                check(!unit(*c).action && unit(*c).bonus_action && offered(*c, "light_melee", 2) &&
                      !offered(*c, "light_melee", 1),
                      "Different identical weapon qualifies even after an initial miss");
                exact_restore(*c);
                const auto hp = unit(*c, 99).hit_points;
                act(*c, "light_melee", 2);
                check(!unit(*c).bonus_action && !unit(*c).action,
                      "Extra attack spends Bonus Action and preserves spent Action");
                if (const auto hit = rogue_attack_checks::last_hit(*c); !hit.empty())
                {
                    ++hits;
                    criticals += hit == "CRITICAL";
                    check(dagger_damage_within(*c, negative ? -2
                                               : feat   ? 3
                                               : 0),
                          "Light removes only positive modifier; TWF retains normal modifier exactly once");
                    check(unit(*c, 99).hit_points < hp || negative,
                          "Light damage applies once when the hit resolves");
                    exact_restore(*c);
                }
                settle(*c);
                check(!offered(*c, "light_melee"),
                      "One Bonus Action forbids a second Light attack");
                exact_restore(*c);
                act(*c, "end");
                if (c->snapshot().actor != 1)
                    act(*c, "end");
                check(!offered(*c, "light_melee"), "Qualification expires at next turn");
            }
    check(hits > 40 && criticals > 0,
          "Exercise normal hits, critical hits and misses with both modifier signs");
    {
        auto p = party();
        auto c = battle(p);
        const auto before = unit(*c);
        act(*c, "weapon_select", 2);
        check(unit(*c).selected_weapon == 2 && unit(*c).action == before.action &&
              unit(*c).bonus_action == before.bonus_action,
              "Weapon selection is free");
        exact_restore(*c);
        act(*c, "melee");
        settle(*c);
        check(offered(*c, "light_melee", 1) && !offered(*c, "light_melee", 2),
              "Selected physical weapon is the qualifying weapon");
    }
    {
        auto p = party();
        auto c = battle(p);
        act(*c, "throw", 1);
        settle(*c);
        check(offered(*c, "light_throw", 3) && !offered(*c, "light_throw", 1),
              "A fresh carried stack unit may follow a thrown Light weapon");
        act(*c, "light_throw", 3);
        settle(*c);
        unsigned total = 0, ground = 0;
        for (const auto &item : c->snapshot().held_items)
            if (item.definition == "dagger")
            {
                total += item.quantity;
                if (!item.holder)
                    ++ground;
            }
        check(total == 5 && ground == 0,
              "Throwing two distinct Light weapons keeps both, like ammunition");
        exact_restore(*c);
    }
    {
        auto p = party();
        p.equip(1, 4);
        p.equip(1, 5, EquipmentOperation::equip_other);
        p.award_experience(300, "loading");
        p.advance(1, p.default_advancement(1));
        auto c = battle(p, 13, {5, 1});
        act(*c, "ranged");
        settle(*c);
        act(*c, "action_surge");
        act(*c, "ranged");
        settle(*c);
        act(*c, "light_ranged", 2);
        settle(*c);
        check(!unit(*c).action && !unit(*c).bonus_action,
              "Loading allows distinct Action, Surge Action and Light Bonus Action");
        exact_restore(*c);
    }
    // A reaction made while another Light weapon is held cannot earn Light. With
    // adjacent-only melee (MELEE-1), leaving the Whip's square provokes directly.
    {
        auto h = hero_for();
        h.add_item({.definition_id = "whip", .name = "Whip"});
        h.add_item({.definition_id = "dagger", .name = "Dagger"});
        CampaignParty reactions(rules());
        reactions.add_pc(std::move(h));
        reactions.equip(1, 1);
        reactions.equip(1, 2, EquipmentOperation::equip_other);
        auto c = battle(reactions);
        act(*c, "end");
        check(c->snapshot().actor == 99, "Enemy turn permits reaction test");
        bool moved = false;
        for (const auto &command : c->legal_commands())
            if (command.verb == "move" && command.destination == Cell{3, 1})
            {
                check(c->submit(command), "Move out of melee reach");
                moved = true;
                break;
            }
        check(moved && c->snapshot().reaction_pending && offered(*c, "opportunity"),
              "Leaving the adjacent square provokes with the selected Whip");
        exact_restore(*c);
        act(*c, "opportunity");
        settle(*c);
        exact_restore(*c);
        check(!unit(*c).reaction && unit(*c).light_attacks.empty(),
              "Reaction spends only Reaction and never qualifies for Light");
    }
    // A Magic action with a Light weapon held never earns an extra weapon attack.
    {
        auto h = hero_for(false, false, "wizard");
        h.add_item({.definition_id = "dagger", .name = "Dagger"});
        h.add_item({.definition_id = "dagger", .name = "Dagger"});
        CampaignParty p(rules());
        p.add_pc(std::move(h));
        p.equip(1, 1);
        auto c = battle(p);
        check(offered(*c, "fire_bolt"), "Known Fire Bolt is offered");
        act(*c, "fire_bolt");
        check(unit(*c).light_attacks.empty(), "Fire Bolt is Magic, not an Attack action");
    }
    {
        auto h = hero_for(false, false, "rogue");
        h.add_item({.definition_id = "dagger", .name = "Dagger"});
        h.add_item({.definition_id = "dagger", .name = "Dagger"});
        CampaignParty p(rules());
        p.add_pc(std::move(h));
        p.equip(1, 1);
        p.equip(1, 2, EquipmentOperation::equip_other);
        auto actors = p.participants();
        actors.front().cell = {1, 1};
        actors.push_back({2, "vanguard", "Ally", 0, {2, 2}});
        actors.push_back({99, "target", "Target", 1, {2, 1}});
        bool checked = false;
        for (unsigned seed = 1; seed <= 32 && !checked; ++seed)
        {
            auto c = rules()->create({{12, 8, std::vector<std::uint8_t>(96)}, actors}, seed);
            while (c->snapshot().actor != 1)
                act(*c, "end");
            act(*c, "melee");
            settle(*c);
            if (rogue_attack_checks::sneaked(*c))
                continue;
            act(*c, "light_melee", 2);
            if (!rogue_attack_checks::sneaked(*c))
                continue;
            check(rogue_attack_checks::sneak_damage(*c) > 0,
                  "A Light attack after a miss applies the unused Sneak Attack automatically");
            exact_restore(*c);
            checked = true;
        }
        check(checked, "Actual Rogue Light attack applies Sneak Attack left unused by a miss");
    }
    for (bool npc :
            {
                false, true
            })
    {
        auto p = party(true, false, npc);
        auto c = battle(p, 13, {2, 1}, 20);
        act(*c, "melee");
        settle(*c);
        act(*c, "light_throw", 2);
        settle(*c);
        for (unsigned n = 0; n < 20 && c->snapshot().outcome == Outcome::ongoing; ++n)
        {
            if (c->snapshot().actor == 1 && offered(*c, "melee"))
            {
                act(*c, "melee");
                settle(*c);
            }
            else
                act(*c, "end");
        }
        check(c->snapshot().outcome == Outcome::victory, "Actual victory ends the Light battle");
        p.begin_combat();
        p.apply_combat(c->snapshot());
        p.end_combat();
        const auto saved = encode_campaign(p, nullptr, "light-campaign");
        CampaignParty restored(rules());
        restored.restore(
            decode_campaign(saved, *srd5::character_rules(), *rules(), "light-campaign", nullptr)
            .party);
        check(encode_campaign(restored, nullptr, "light-campaign") == saved,
              "PC/recruited physical inventory and TWF survive campaign reload");
        unsigned count = 0;
        for (const auto &item : restored.member(1).character.inventory().items())
            if (item.definition_id == "dagger")
                count += item.quantity;
        check(count == 5, "Throwing never removes a weapon from inventory");
        check(bool(restored.rest(RestKind::short_rest)), "Light user can Short Rest");
        restored.finish_short_rest(restored.state().short_rest->ticket);
        check(bool(restored.rest(RestKind::long_rest)), "Light user can Long Rest");
    }
    for (const auto *klass :
            {"fighter", "paladin", "ranger"
            })
    {
        auto h = style_route_checks::leveled(*rules(), klass, "defense", 3);
        auto choice = rules()->default_advancement(h.sheet());
        choice.feat = "two_weapon_fighting";
        choice.abilities = {};
        VitalState vitals;
        check(h.advance(*rules(), vitals, choice),
              "Independent level-four TWF feat available to every style class");
        auto bad = h.sheet();
        bad.grants.push_back(
        {"feat:two_weapon_fighting", "class:" + std::string(klass) + ":fighting_style", 2, {}});
        rejects(
            [&]
        {
            (void)rules()->character_profile(bad, {});
        });
    }
    {
        auto h = style_route_checks::leveled(*rules(), "fighter", "two_weapon_fighting", 3);
        auto choice = rules()->default_advancement(h.sheet());
        choice.fighting_style = "archery";
        choice.feat = "two_weapon_fighting";
        choice.abilities = {};
        VitalState vitals;
        check(h.advance(*rules(), vitals, choice),
              "Fighter replaces class TWF and independently retains it as level-four feat");
    }
    for (const auto *klass :
            {"fighter", "paladin", "ranger"
            })
        for (unsigned level = 2; level <= 4; ++level)
        {
            auto h = style_route_checks::leveled(*rules(), klass, "two_weapon_fighting", level);
            check(std::any_of(h.sheet().grants.begin(), h.sheet().grants.end(),
                              [](const auto & g)
            {
                return g.id == "feat:two_weapon_fighting";
            }),
            "Actual supported class advancement grants TWF");
            check(rules()
                  ->character_profile(h.sheet(), std::vector<std::string> {"dagger"})
                  .data.starts_with("PC42 "),
                  "TWF and mastery have versioned sourced profiles");
        }
}
} // namespace light_attack_checks
