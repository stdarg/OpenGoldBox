// Actual 0.6.55 writer evidence, captured before mastery implementation.
namespace mastery_baseline
{
using light_attack_checks::act;
using light_attack_checks::settle;

void write(std::string_view name, const std::string &bytes)
{
    std::ofstream out(std::filesystem::path(OPENGOLD_SOURCE_DIR) / "tests/fixtures" / name,
                      std::ios::binary);
    out << bytes;
    check(bool(out), "Write genuine pre-mastery fixture");
}

auto party()
{
    CampaignParty p(light_attack_checks::rules());
    for (const auto *klass : {"fighter", "rogue", "paladin", "ranger", "barbarian"})
    {
        auto h = light_attack_checks::hero_for(false, false, klass);
        h.inventory().add("dagger", "Dagger", 3);
        h.inventory().add("longsword", "Longsword");
        const bool recruited = p.state().roster.size() == 2 || p.state().roster.size() == 3;
        const auto id = recruited
                            ? p.recruit(std::string("mastery-baseline:") + klass, std::move(h))
                            : p.add_pc(std::move(h));
        p.equip(id, 1);
    }
    p.award_experience(2700, "mastery-baseline");
    for (const auto id : {1u, 2u, 3u, 4u})
        for (unsigned level = 2; level <= 4; ++level)
            p.advance(id, p.default_advancement(id));
    auto state = p.checkpoint();
    for (auto &m : state.roster)
        m.vitals.hit_points -= 2;
    p.restore(std::move(state));
    return p;
}

void freeze()
{
    auto module = light_attack_checks::rules();
    check(module->identity().version == "0.6.55",
          "Mastery baseline must use the genuine0.6.55 writer");
    auto p = party();
    write("campaign-v17-mastery-before.ogs", encode_campaign(p, nullptr, "mastery-before"));
    bool captured = false;
    for (unsigned seed = 1; seed <= 128 && !captured; ++seed)
    {
        auto light = light_attack_checks::party(true);
        auto c = light_attack_checks::battle(light, seed);
        act(*c, "melee");
        settle(*c);
        const auto qualified = c->save();
        act(*c, "light_melee", 2);
        if (!c->snapshot().savage_attack_choice)
            continue;
        write("combat-v22-mastery-light-qualified.save", qualified);
        write("combat-v22-mastery-light-first.save", c->save());
        act(*c, "savage_use");
        write("combat-v22-mastery-light-second.save", c->save());
        act(*c, "savage_second");
        settle(*c);
        write("combat-v22-mastery-light-settled.save", c->save());
        captured = true;
    }
    check(captured,
          "Capture Light qualification, physical second weapon and both Savage decisions");
}

std::string normalized(std::string bytes, const RulesModule &module)
{
    replace(bytes, "0.6.55", module.identity().version);
    return bytes;
}

void verify()
{
    auto module = light_attack_checks::rules();
    const auto old = fixture("campaign-v17-mastery-before.ogs");
    CampaignParty p(light_attack_checks::rules());
    p.restore(
        decode_campaign(old, *srd5::character_rules(), *module, "mastery-before", nullptr).party);
    const auto body = [](const std::string &s)
    {
        return s.substr(s.find('\n', s.find('\n') + 1) + 1);
    };
    check(
        body(encode_campaign(p, nullptr, "mastery-before")) == normalized(body(old), *module),
        "Pre-mastery five-class campaign retains exact choices, provenance, equipment and wounds");
    check(p.state().roster.size() == 5,
          "Mastery baseline includes all five current source classes");
    const auto restored = [&](const char *name)
    {
        auto c = module->restore(fixture(name));
        check(c->save() == normalized(fixture(name), *module),
              "Pre-mastery checkpoint restores exactly");
        return c;
    };
    auto c = restored("combat-v22-mastery-light-qualified.save");
    act(*c, "light_melee", 2);
    check(c->save() == normalized(fixture("combat-v22-mastery-light-first.save"), *module),
          "Pre-mastery extra attack preserves exact RNG, budgets and pending damage");
    c = restored("combat-v22-mastery-light-first.save");
    act(*c, "savage_use");
    check(c->save() == normalized(fixture("combat-v22-mastery-light-second.save"), *module),
          "Pre-mastery Savage second roll preserves exact continuation");
    c = restored("combat-v22-mastery-light-second.save");
    act(*c, "savage_second");
    settle(*c);
    check(c->save() == normalized(fixture("combat-v22-mastery-light-settled.save"), *module),
          "Pre-mastery settled Light attack preserves spent resources and damage");
    (void)restored("combat-v22-mastery-light-settled.save");
}
} // namespace mastery_baseline
