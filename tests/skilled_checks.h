// Skilled acceptance and actual pre-Skilled writer fixtures.
namespace skilled_checks
{
// A Wizard exercises the three-page advancement flow, the Rogue carries dense
// existing proficiencies so already-known entries must stay unavailable, and the
// Fighter holds a Fighting Style and weapon mastery.
CampaignParty party()
{
    CampaignParty p(module());
    for (const auto *klass :
            {"wizard", "rogue", "fighter"
            })
    {
        auto d = draft(klass, std::string_view(klass) == "wizard" ? "sage" : "criminal");
        d.name = std::string("Skilled ") + klass;
        if (std::string_view(klass) == "wizard")
            d.cantrips = std::vector<std::string> {"fire_bolt", "ray_of_frost"};
        if (std::string_view(klass) == "rogue")
            d.training = choices();
        else
            for (const auto &g : srd5::character_rules()->training_options(d))
                for (unsigned i = 0; i < g.count; ++i)
                    d.training[g.id].push_back(g.options.at(i).id);
        auto h = hero(d);
        h.inventory().add("dagger", "Dagger", 2);
        const auto id = p.add_pc(std::move(h));
        p.equip(id, 1);
    }
    p.award_experience(2700, "skilled-baseline");
    for (unsigned id :
            {
                1, 2, 3
            })
        for (unsigned level = 2; level <= 4; ++level)
            p.advance(id, p.default_advancement(id));
    auto state = p.checkpoint();
    for (auto &m : state.roster)
        m.vitals.hit_points -= 2;
    p.restore(std::move(state));
    return p;
}

auto battle(CampaignParty &p, unsigned seed = 29)
{
    auto actors = p.participants();
    actors[0].cell = {1, 1};
    actors[1].cell = {2, 1};
    actors[2].cell = {3, 1};
    actors.push_back({99, "vanguard", "Enemy", 1, {8, 6}});
    return p.rule_module().create({{12, 8, std::vector<std::uint8_t>(96)}, actors}, seed);
}

void write(const char *name, const std::string &bytes)
{
    std::ofstream out(std::filesystem::path(OPENGOLD_SOURCE_DIR) / "tests/fixtures" / name);
    out << bytes;
    check(bool(out), "Write Skilled prior writer fixture");
}

void freeze()
{
    auto p = party();
    check(p.rule_module().identity().version == "0.6.61",
          "Skilled capture requires actual 0.6.61 writer");
    write("campaign-skilled-0.6.61.ogs", encode_campaign(p, nullptr, "skilled-before"));
    auto c = battle(p);
    write("combat-skilled-0.6.61.save", c->save());
    // The criminal background grants Alert, so members 2 and 3 open with a
    // pending initiative queue. Freeze it, then resolve it for the continuation.
    alert_checks::decline(*c);
    light_attack_checks::act(*c, "end");
    write("combat-skilled-0.6.61-continued.save", c->save());
}
} // namespace skilled_checks
