namespace mastery_grant_checks
{
namespace mastery = opengold::srd5::detail;

Character chosen(std::string klass)
{
    auto h = hero(klass);
    auto d = h.creation_data();
    auto creation = srd5::character_rules();
    for (const auto &original : creation->training_options(d))
    {
        const auto groups = creation->training_options(d);
        const auto group = *std::find_if(groups.begin(), groups.end(),
                                         [&](const auto & g)
        {
            return g.id == original.id;
        });
        auto &selected = d.training[group.id];
        for (const auto &option : group.options)
            if (selected.size() < group.count &&
                    std::find(selected.begin(), selected.end(), option.id) == selected.end())
                selected.push_back(option.id);
    }
    return Character(*creation, d, {});
}

void run()
{
    const std::map<std::string, unsigned> expected
    {
        {"barbarian", 2}, {"fighter", 3}, {"paladin", 2}, {"ranger", 2}, {"rogue", 2}};
    for (const auto &klass : srd5::character_rules()->choices(CreationField::character_class))
    {
        const auto options = mastery::mastery_options(klass.id, 1);
        const auto found = expected.find(klass.id);
        check(options.count == (found == expected.end() ? 0 : found->second),
              "Independent starting mastery counts across all twelve classes");
        unsigned kinds = 0;
        for (const auto &item : mastery::weapons)
        {
            const bool allowed =
                item.key != "wand" && item.key != "torch" && found != expected.end() &&
                (klass.id != "barbarian" || !item.ranged) &&
                (klass.id != "rogue" || !item.martial || item.finesse || item.light);
            const bool offered = std::any_of(options.options.begin(), options.options.end(),
                                             [&](const auto & o)
            {
                return o.id == item.key;
            });
            check(allowed == offered, "Every SRD weapon obeys the actual class restrictions");
            if (allowed)
                ++kinds;
        }
        if (found == expected.end())
        {
            check(kinds == 0, "No implicit mastery for other classes");
            continue;
        }
        if (klass.id == "fighter" || klass.id == "paladin" || klass.id == "ranger")
            check(kinds == 38, "Full ordinary SRD weapon catalog offered");
        auto h = chosen(klass.id);
        auto grants = h.sheet().grants;
        check(h.sheet().training.masteries.size() == options.count,
              "Real character creation produces chosen mastery grants");
        auto rules = module();
        check(rules->character_profile(h.sheet(), {}).data.starts_with("PC42 "),
              "Chosen masteries use validated combat recipes");
        for (unsigned n = 0; n < options.count; ++n)
        {
            auto bad = h.sheet();
            auto g = *std::find_if(grants.begin(), grants.end(), mastery::is_mastery_grant);
            if (n == 0)
                g.source_id = "class:wizard:weapon_mastery";
            else if (n == 1)
                g.level = 4;
            else
                g.id = "mastery:wand";
            bad.grants.push_back(g);
            rejects(
                [&]
            {
                (void)rules->character_profile(bad, {});
            });
        }
        auto duplicate = grants;
        duplicate.push_back(*std::find_if(grants.begin(), grants.end(), mastery::is_mastery_grant));
        rejects(
            [&]
        {
            (void)mastery::mastery_choices(duplicate, klass.id, 1);
        });
        std::vector<std::string> selected;
        for (const auto &g : grants)
            if (mastery::is_mastery_grant(g))
                selected.push_back(g.id.substr(8));
        auto next = selected;
        next[0] = options.options.at(options.count).id;
        const auto replaced = mastery::replace_masteries(grants, klass.id, 1, next);
        check(grants == h.sheet().grants && replaced != grants,
              "Rest replacement produces a candidate without mutating original grants");
        check(mastery::replace_masteries(grants, klass.id, 1, selected) == grants,
              "Keeping selections is an exact no-op");
        next[1] = options.options.at(options.count + 1).id;
        if (klass.id == "fighter" || klass.id == "barbarian")
            rejects(
                [&]
        {
            (void)mastery::replace_masteries(grants, klass.id, 1, next);
        });
        else
            check(mastery::replace_masteries(grants, klass.id, 1, next) != grants,
                  "Paladin/Ranger/Rogue may replace both kinds");
        next = selected;
        next[0] = next[1];
        rejects(
            [&]
        {
            (void)mastery::replace_masteries(grants, klass.id, 1, next);
        });
        next = selected;
        next[0] = "wand";
        rejects(
            [&]
        {
            (void)mastery::replace_masteries(grants, klass.id, 1, next);
        });
        const auto original = grants;
        std::erase_if(grants, mastery::is_mastery_grant);
        rejects(
            [&]
        {
            (void)mastery::replace_masteries(grants, klass.id, 1, selected);
        });
        if (klass.id == "barbarian")
            continue;
        CampaignParty p(module());
        const auto id = p.add_pc(h);
        p.award_experience(2700, "mastery-levels");
        for (unsigned level = 2; level <= 4; ++level)
        {
            const auto old = p.member(id).character.sheet().grants;
            const auto choice = p.default_advancement(id);
            p.advance(id, choice);
            const auto &sheet = p.member(id).character.sheet();
            check(sheet.training.masteries.size() ==
                  options.count + (klass.id == "fighter" && level == 4 ? 1 : 0),
                  "Mastery count follows actual attained levels");
            for (const auto &g : old)
                if (mastery::is_mastery_grant(g))
                    check(has(sheet, g), "Advancement retains previous mastery choices");
        }
        const auto saved_bytes = saved(p);
        CampaignParty copy(module());
        copy.restore(
            decode_campaign(saved_bytes, *srd5::character_rules(), *rules, "grant-fixture", nullptr)
            .party);
        check(saved(copy) == saved_bytes,
              "Mastery choices and advancement round-trip exactly through campaign saves");
    }
    {
        CampaignParty p(module());
        const auto id = p.add_pc(hero("fighter"));
        p.award_experience(2700, "pending-mastery");
        for (unsigned n = 2; n <= 4; ++n)
            p.advance(id, p.default_advancement(id));
        check(p.member(id).character.sheet().training.masteries.size() == 1,
              "New fourth mastery can coexist with pending historical starting choices");
    }
    // A fourth Fighter choice cannot repeat a first-level kind or be acquired early.
    auto h = chosen("fighter");
    auto grants = h.sheet().grants;
    const auto extra = mastery::mastery_options("fighter", 4, grants);
    check(extra.count == 1 && extra.options.size() == 35,
          "Fourth Fighter choice excludes all three existing kinds");
    grants.push_back({"mastery:" + extra.options.front().id, extra.id, 4, {}});
    rejects(
        [&]
    {
        (void)mastery::mastery_choices(grants, "fighter", 3);
    });
    check(mastery::mastery_choices(grants, "fighter", 4).at(extra.id).size() == 1,
          "Fourth kind validates at4");
}
} // namespace mastery_grant_checks
