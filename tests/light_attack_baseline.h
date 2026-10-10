// Hand-state party and the Godot hand UI fixture and its comparison.
namespace light_attack_baseline
{
CampaignParty party()
{
    CampaignParty p(rogue_attack_checks::rules_module());
    for (const auto *klass :
            {"fighter", "paladin", "ranger"
            })
    {
        auto h = style_route_checks::starter(klass);
        h.add_item({.definition_id = "greatsword", .name = "Greatsword"});
        h.add_item({.definition_id = "dagger", .name = "Dagger", .quantity = 3});
        h.add_item({.definition_id = "hand_crossbow", .name = "Hand Crossbow"});
        h.add_item({.definition_id = "shield", .name = "Shield"});
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
    expected.selected_slot = actual.state().selected_slot;
    p.restore(std::move(expected));
    check(encode_campaign(actual, nullptr, assets) == encode_campaign(p, nullptr, assets),
          "UI hand choices exactly match native equipment, inventory, wounds and resources");
}
} // namespace light_attack_baseline
