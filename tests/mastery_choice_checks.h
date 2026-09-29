// Actual writer baseline for optional mastery and simultaneous critical effects.
namespace mastery_choice_checks
{
using namespace mastery_combat_checks;

void capture()
{
    const auto output = std::getenv("OPENGOLD_MASTERY_CHOICE_BASELINE");
    if (!output)
        return;
    auto r = module();
    check(r->identity().version == "0.6.57", "Capture only the actual pre-choice writer");
    const auto directory = std::filesystem::path(output);
    std::filesystem::create_directories(directory);
    for (const auto weapon :
            {"longbow", "maul"
            })
    {
        CampaignParty p(module());
        auto h = hero(weapon, "fighter", "soldier");
        h.inventory().add(weapon, "Mastery weapon");
        p.add_pc(h);
        p.equip(1, 1);
        p.award_experience(900, "choice-baseline");
        while (p.member(1).character.sheet().level < 3)
            p.advance(1, p.default_advancement(1));
        auto actors = p.participants();
        actors.front().cell = {1, 1};
        actors.push_back(
        {99, "vanguard", "Target", 1, {std::string_view(weapon) == "longbow" ? 3 : 2, 1}});
        bool written = false;
        for (unsigned seed = 1; seed <= 128 && !written; ++seed)
        {
            auto c = r->create({{12, 8, std::vector<std::uint8_t>(96)}, actors, 777}, seed);
            turn(*c, 1);
            const auto before = c->save();
            act(*c, std::string_view(weapon) == "longbow" ? "ranged" : "melee", 99);
            if (!c->snapshot().savage_attack_choice ||
                    !c->snapshot().savage_attack_choice->critical)
                continue;
            const auto write = [&](const char *suffix, const std::string & bytes)
            {
                std::ofstream out(directory /
                                  (std::string("combat-v23-") + weapon + suffix + ".save"),
                                  std::ios::binary);
                out << bytes;
                check(bool(out), "Write actual pre-choice writer fixture");
            };
            write("-before", before);
            write("-damage", c->save());
            act(*c, "savage_skip");
            check(bool(c->snapshot().free_movement), "Actual Champion critical grants movement");
            write("-move", c->save());
            act(*c, "end");
            write("-settled", c->save());
            written = true;
            std::cout << "Captured " << weapon << " critical at seed " << seed << '\n';
        }
        check(written, "Captured an actual critical for each new optional property");
    }
}

std::string fixture(const char *weapon, const char *suffix)
{
    std::ifstream in(std::filesystem::path(OPENGOLD_SOURCE_DIR) / "tests/fixtures" /
                     (std::string("combat-v23-") + weapon + suffix + ".save"));
    check(bool(in), "Open actual pre-choice writer fixture");
    return {std::istreambuf_iterator<char>(in), {}};
}

void historical()
{
    auto r = module();
    for (const auto weapon :
            {"longbow", "maul"
            })
    {
        const auto current = [&](const char *suffix)
        {
            auto bytes = fixture(weapon, suffix);
            const auto at = bytes.find("0.6.57");
            check(at != bytes.npos,
                  "Frozen optional-choice baseline has its actual writer identity");
            bytes.replace(at, 6, r->identity().version);
            return bytes;
        };
        for (const auto suffix :
                {"-before", "-damage", "-move", "-settled"
                })
        {
            const auto bytes = fixture(weapon, suffix);
            check(r->restore(bytes)->save() == current(suffix),
                  "Actual writer round trips every critical phase apart from module identity");
        }
        auto c = r->restore(fixture(weapon, "-before"));
        act(*c, std::string_view(weapon) == "longbow" ? "ranged" : "melee", 99);
        const auto fresh = c->snapshot().savage_attack_choice;
        const auto old = r->restore(fixture(weapon, "-damage"))->snapshot().savage_attack_choice;
        check(fresh && old && fresh->first_damage == old->first_damage &&
              fresh->dice_count == old->dice_count && fresh->modifier == old->modifier,
              "New attack preserves historical damage before offering new mastery choices");
        c = r->restore(fixture(weapon, "-damage"));
        c = r->restore(c->save());
        act(*c, "savage_skip");
        check(c->save() == current("-move"),
              "Actual pending hit reproduces frozen Champion movement");
        c = r->restore(c->save());
        act(*c, "end");
        check(c->save() == current("-settled"),
              "Actual critical continuation reproduces frozen settled writer");
    }
}
} // namespace mastery_choice_checks
