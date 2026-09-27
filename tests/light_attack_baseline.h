// Actual 0.6.53 writer evidence for the Light/hand-state transition.
namespace light_attack_baseline
{
using cunning_checks::act;
const auto directory = std::filesystem::path(OPENGOLD_SOURCE_DIR) / "tests/fixtures";

void write(std::string_view name, const std::string &bytes)
{
    std::ofstream out(directory / name, std::ios::binary);
    out << bytes;
    check(bool(out), "Write actual Light baseline fixture");
}

std::string read(const char *name)
{
    return fixture(name);
}

std::string normalized(std::string bytes, const RulesModule &rules)
{
    replace(bytes, "0.6.53", rules.identity().version);
    return bytes;
}

CampaignParty party()
{
    CampaignParty p(rogue_attack_checks::rules_module());
    for (const auto *klass :
            {"fighter", "paladin", "ranger"
            })
    {
        auto h = style_route_checks::starter(klass);
        h.inventory().add("greatsword", "Greatsword");
        h.inventory().add("dagger", "Dagger", 3);
        h.inventory().add("hand_crossbow", "Hand Crossbow");
        h.inventory().add("shield", "Shield");
        const auto id = std::string_view(klass) == "fighter"
                        ? p.add_pc(std::move(h))
                        : p.recruit(std::string("baseline:") + klass, std::move(h));
        p.equip(id, 1);
    }
    p.award_experience(2700, "light-baseline");
    for (const auto id :
            {
                1u, 2u, 3u
            })
        for (unsigned level = 2; level <= 4; ++level)
        {
            auto choice = p.default_advancement(id);
            if (level == 2)
                choice.fighting_style = "great_weapon_fighting";
            if (level == 4)
            {
                choice.feat = "archery";
                choice.abilities = {};
            }
            p.advance(id, choice);
        }
    auto state = p.checkpoint();
    for (auto &member : state.roster)
        member.vitals.hit_points -= 2;
    p.restore(std::move(state));
    return p;
}

void finish_attack(CombatSession &c)
{
    if (c.snapshot().savage_attack_choice)
        act(c, "savage_skip");
    if (c.snapshot().free_movement)
        act(c, "end");
}

void two_actions(CombatSession &c)
{
    act(c, "ranged");
    finish_attack(c);
    act(c, "action_surge");
    act(c, "ranged");
    finish_attack(c);
}

void freeze()
{
    auto rules = rogue_attack_checks::rules_module();
    check(rules->identity().version == "0.6.53", "Light baseline requires the actual prior writer");
    auto p = party();
    write("campaign-v17-light-before.ogs", encode_campaign(p, nullptr, "light-before"));
    auto actors = p.participants();
    actors[0].cell = {1, 1};
    actors[1].cell = {9, 6};
    actors[2].cell = {10, 6};
    actors.push_back({99, "target", "Target", 1, {2, 1}});
    bool captured = false;
    for (unsigned seed = 1; seed <= 256 && !captured; ++seed)
    {
        auto c = rules->create({{12, 8, std::vector<std::uint8_t>(96)}, actors}, seed);
        while (c->snapshot().actor != 1)
            act(*c, "end");
        const auto before = c->save();
        act(*c, "melee");
        if (!c->snapshot().savage_attack_choice || !c->snapshot().savage_attack_choice->critical)
            continue;
        write("combat-v21-light-before-attack.save", before);
        write("combat-v21-light-before-first.save", c->save());
        act(*c, "savage_use");
        write("combat-v21-light-before-second.save", c->save());
        act(*c, "savage_second");
        if (c->snapshot().free_movement)
            act(*c, "end");
        write("combat-v21-light-before-settled.save", c->save());
        captured = true;
    }
    check(captured, "Capture genuine critical GWF/Savage prior continuation");
    p.equip(1, 3); // Real carried Hand Crossbow replaces the Greatsword.
    actors = p.participants();
    actors[0].cell = {1, 1};
    actors[1].cell = {9, 6};
    actors[2].cell = {10, 6};
    actors.push_back({99, "target", "Target", 1, {5, 1}});
    auto loading = rules->create({{12, 8, std::vector<std::uint8_t>(96)}, actors}, 13);
    while (loading->snapshot().actor != 1)
        act(*loading, "end");
    write("combat-v21-loading-before.save", loading->save());
    two_actions(*loading);
    write("combat-v21-loading-two-actions.save", loading->save());
}

void verify()
{
    auto rules = rogue_attack_checks::rules_module();
    const auto bytes = read("campaign-v17-light-before.ogs");
    CampaignParty p(rogue_attack_checks::rules_module());
    p.restore(
        decode_campaign(bytes, *srd5::character_rules(), *rules, "light-before", nullptr).party);
    const auto body = [](const auto & text)
    {
        return text.substr(text.find('\n', text.find('\n') + 1) + 1);
    };
    check(
        body(encode_campaign(p, nullptr, "light-before")) == normalized(body(bytes), *rules),
        "Prior three-class campaign preserves inventory identities, grip, wounds, training and styles");
    check(p.state().roster.size() == 3 &&
          p.member(2).character.sheet().character_class == "Paladin" &&
          p.member(3).character.sheet().character_class == "Ranger",
          "Baseline retains recruited source classes");
    for (const auto &member : p.state().roster)
    {
        check(member.character.inventory().find(2)->get().quantity == 3 &&
              member.equipped.size() == 1 && member.equipped.front() == 1 &&
              member.vitals.hit_points == member.character.sheet().hit_points - 2,
              "Historical carried dagger stack, single held weapon and wounds are unchanged");
    }
    auto c = rules->restore(read("combat-v21-light-before-attack.save"));
    act(*c, "melee");
    check(c->save() == normalized(read("combat-v21-light-before-first.save"), *rules),
          "Prior first attack and RNG are exact");
    auto restored = rules->restore(read("combat-v21-light-before-first.save"));
    act(*c, "savage_use");
    act(*restored, "savage_use");
    check(c->save() == restored->save() &&
          c->save() == normalized(read("combat-v21-light-before-second.save"), *rules),
          "Prior second damage roll and pending weapon identity remain exact");
    act(*c, "savage_second");
    if (c->snapshot().free_movement)
        act(*c, "end");
    check(c->save() == normalized(read("combat-v21-light-before-settled.save"), *rules),
          "Prior resolved damage, spent action, items and Champion continuation remain exact");
    auto loading = rules->restore(read("combat-v21-loading-before.save"));
    two_actions(*loading);
    auto expected = rules->restore(read("combat-v21-loading-two-actions.save"));
    check(expected->save() == normalized(read("combat-v21-loading-two-actions.save"), *rules),
          "Historical spent Loading actions restore without inventing new turn history");
    const auto actual_actor = rogue_attack_checks::unit(*loading);
    const auto expected_actor = rogue_attack_checks::unit(*expected);
    check(!actual_actor.action && actual_actor.bonus_action &&
          actual_actor.persistent == expected_actor.persistent &&
          rogue_attack_checks::unit(*loading, 99).hit_points ==
          rogue_attack_checks::unit(*expected, 99).hit_points &&
          style_route_checks::random_state(*loading) ==
          style_route_checks::random_state(*expected),
          "Separate Loading actions retain actual rolls, HP, resources and unused Bonus Action");
}

void write_hands_ui_fixture()
{
    if (const auto *dir = std::getenv("OPENGOLD_GAME_DIR"); dir && *dir)
    {
        auto baseline = party();
        CampaignParty p(module());
        p.restore(baseline.checkpoint());
        write_campaign_file(std::filesystem::path(OPENGOLD_BINARY_DIR) / "hands-ui.ogs",
                            encode_campaign(p, nullptr, campaign_asset_identity(dir)));
    }
}

void verify_hands_ui(const std::filesystem::path &path)
{
    const auto *dir = std::getenv("OPENGOLD_GAME_DIR");
    check(dir && *dir, "Hand UI comparison requires game assets");
    auto baseline = party();
    CampaignParty p(module());
    p.restore(baseline.checkpoint());
    for (auto id :
            {
                1u, 2u
            })
    {
        p.equip(id, 2, EquipmentOperation::equip_main);
        p.equip(id, 2, EquipmentOperation::equip_other);
    }
    const auto assets = campaign_asset_identity(dir);
    auto rules = module();
    CampaignParty actual(module());
    actual.restore(
        decode_campaign(read_campaign_file(path), *srd5::character_rules(), *rules, assets, nullptr)
        .party);
    auto expected = p.checkpoint();
    expected.selected = actual.state().selected;
    p.restore(std::move(expected));
    check(encode_campaign(actual, nullptr, assets) == encode_campaign(p, nullptr, assets),
          "UI hand choices exactly match native equipment, inventory, wounds and resources");
}

void freeze_hands()
{
    auto rules = rogue_attack_checks::rules_module();
    check(rules->identity().version == "0.6.54", "Hand baseline requires actual0.6.54 writer");
    auto p = party();
    for (auto id :
            {
                1u, 2u
            })
    {
        p.equip(id, 2, EquipmentOperation::equip_main);
        p.equip(id, 3, EquipmentOperation::equip_other);
    }
    write("campaign-v17-hands-before.ogs", encode_campaign(p, nullptr, "hands-before"));
    auto actors = p.participants();
    actors[0].cell = {1, 1};
    actors[1].cell = {9, 6};
    actors[2].cell = {10, 6};
    actors.push_back({99, "target", "Target", 1, {2, 1}});
    bool captured = false;
    for (unsigned seed = 1; seed <= 256 && !captured; ++seed)
    {
        auto c = rules->create({{12, 8, std::vector<std::uint8_t>(96)}, actors}, seed);
        while (c->snapshot().actor != 1)
            act(*c, "end");
        const auto before = c->save();
        act(*c, "melee");
        if (!c->snapshot().savage_attack_choice)
            continue;
        write("combat-v21-hands-before-attack.save", before);
        write("combat-v21-hands-before-first.save", c->save());
        act(*c, "savage_use");
        write("combat-v21-hands-before-second.save", c->save());
        act(*c, "savage_second");
        if (c->snapshot().free_movement)
            act(*c, "end");
        write("combat-v21-hands-before-settled.save", c->save());
        captured = true;
    }
    check(captured, "Capture actual two-weapon pending hit");
}

void verify_hands_baseline()
{
    auto rules = rogue_attack_checks::rules_module();
    const auto normalize = [&](std::string s)
    {
        replace(s, "0.6.54", rules->identity().version);
        return s;
    };
    const auto bytes = read("campaign-v17-hands-before.ogs");
    CampaignParty p(rogue_attack_checks::rules_module());
    p.restore(
        decode_campaign(bytes, *srd5::character_rules(), *rules, "hands-before", nullptr).party);
    const auto body = [](const std::string & s)
    {
        return s.substr(s.find('\n', s.find('\n') + 1) + 1);
    };
    check(body(encode_campaign(p, nullptr, "hands-before")) == normalize(body(bytes)),
          "Actual dual-hand campaign preserves both weapons, carried counts, styles and wounds");
    for (const auto *phase :
            {"attack", "first", "second", "settled"
            })
    {
        const auto saved =
            read((std::string("combat-v21-hands-before-") + phase + ".save").c_str());
        check(rules->restore(saved)->save() == normalize(saved),
              "Actual dual-hand checkpoint retains its exact old state");
    }
    auto c = rules->restore(read("combat-v21-hands-before-first.save"));
    act(*c, "savage_use");
    check(c->save() == normalize(read("combat-v21-hands-before-second.save")),
          "Two-weapon pending Savage reroll/RNG exact");
    act(*c, "savage_second");
    if (c->snapshot().free_movement)
        act(*c, "end");
    check(c->save() == normalize(read("combat-v21-hands-before-settled.save")),
          "Two-weapon pending resolution exact");
}
} // namespace light_attack_baseline
