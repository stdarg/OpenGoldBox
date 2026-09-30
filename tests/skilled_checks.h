// Skilled acceptance.
namespace skilled_checks
{
// A single member ready for the level-four choice, so each acceptance check
// drives the real advancement transaction rather than a synthesised sheet.
CampaignParty ready(const char *klass, const char *background)
{
    CampaignParty p(module());
    auto d = draft(klass, background);
    d.name = std::string("Skilled ") + klass;
    if (std::string_view(klass) == "wizard")
        d.cantrips = std::vector<std::string> {"fire_bolt", "ray_of_frost"};
    for (const auto &g : srd5::character_rules()->training_options(d))
        for (unsigned i = 0; i < g.count; ++i)
            d.training[g.id].push_back(g.options.at(i).id);
    const auto id = p.add_pc(hero(d));
    p.award_experience(2700, "skilled-acceptance");
    for (unsigned level = 2; level <= 3; ++level)
        p.advance(id, p.default_advancement(id));
    check(p.member(id).character.sheet().level == 3, "Ready at level three");
    return p;
}

AdvancementChoice pick(const CampaignParty &p, std::vector<std::string> ids)
{
    auto choice = p.default_advancement(1);
    choice.feat = "skilled";
    choice.abilities = {};
    choice.training["feat:skilled"] = std::move(ids);
    return choice;
}

const FeatureGrant *grant(const CharacterSheet &sheet, std::string_view id)
{
    const auto found = std::find_if(sheet.grants.begin(), sheet.grants.end(),
                                    [&](const auto & g)
    {
        return g.id == id;
    });
    return found == sheet.grants.end() ? nullptr : &*found;
}

AbilityCheckModifier probe(const CharacterSheet &sheet, unsigned ability, std::string_view skill,
                           std::string_view tool)
{
    return srd5::character_rules()->ability_check(sheet, ability, skill, tool);
}

void options_and_grants()
{
    auto p = ready("fighter", "criminal");
    const auto sheet = p.member(1).character.sheet();
    const auto offered = p.advancement_options(1);
    const auto feat = std::find_if(offered.feats.begin(), offered.feats.end(),
                                   [](const auto & f)
    {
        return f.id == "skilled";
    });
    check(feat != offered.feats.end() && feat->available, "Skilled is offered at level four");
    check(std::none_of(offered.training.begin(), offered.training.end(),
                       [](const auto & g)
    {
        return g.id == "feat:skilled";
    }),
    "No Skilled page before Skilled is selected");
    const auto with = p.advancement_options(1, pick(p, {}));
    const auto group = std::find_if(with.training.begin(), with.training.end(),
                                    [](const auto & g)
    {
        return g.id == "feat:skilled";
    });
    check(group != with.training.end() && group->count == 3,
          "Selecting Skilled opens a page of three");
    check(group->acquired_level == 4, "Skilled proficiencies are acquired at level four");
    unsigned already = 0;
    for (const auto &g : sheet.grants)
        if (g.id.starts_with("skill:") || g.id.starts_with("tool:"))
            ++already;
    check(already && group->options.size() == 18 + 37 - already,
          "Every catalog entry not already held is offered exactly once");
    for (const auto &option : group->options)
        check(!grant(sheet, option.id), "An already-known proficiency is never offered");
    p.advance(1, pick(p, {"skill:arcana", "tool:dice", "skill:medicine"}));
    const auto next = p.member(1).character.sheet();
    check(next.level == 4, "Skilled advancement completes");
    const auto *feat_grant = grant(next, "feat:skilled");
    check(feat_grant && feat_grant->level == 4 &&
          feat_grant->source_id == "class:fighter:ability_score_improvement" &&
          feat_grant->choices.empty(),
          "Skilled records its entitlement as provenance and stores no sub-choices");
    for (const auto *id :
            {"skill:arcana", "tool:dice", "skill:medicine"
            })
    {
        const auto *g = grant(next, id);
        check(g && g->source_id == "feat:skilled" && g->level == 4,
              "Each chosen proficiency is granted with Skilled provenance");
    }
    check(next.scores == sheet.scores, "Skilled adds no ability points");
    check(next.training.complete, "A completed Skilled acquisition is complete training");
}

void check_effects()
{
    auto p = ready("fighter", "criminal");
    const auto before = p.member(1).character.sheet();
    const auto cold = probe(before, 3, "nature", {});
    p.advance(1, pick(p, {"skill:nature", "tool:herbalism_kit", "tool:navigators_tools"}));
    const auto after = p.member(1).character.sheet();
    const auto warm = probe(after, 3, "nature", {});
    check(warm.proficiency > cold.proficiency && warm.total > cold.total,
          "A Skilled skill pick changes the real check result");
    check(probe(after, 3, {}, "herbalism_kit").proficiency > 0,
          "A Skilled tool pick supplies the proficiency bonus");
    const auto combined = probe(after, 3, "nature", "herbalism_kit");
    check(combined.tool_advantage && combined.advantage,
          "Proficiency in both the skill and the tool grants advantage");
    const auto stealth_before = probe(before, 1, "stealth", {});
    const auto stealth_after = probe(after, 1, "stealth", {});
    check(stealth_after.total == stealth_before.total &&
          stealth_after.expertise == stealth_before.expertise,
          "Existing proficiencies and Expertise are unchanged");
    check(probe(after, 3, "nature", {}).sources.size() == 1 &&
          probe(after, 3, "nature", {}).sources.front().source_id == "feat:skilled",
          "The new proficiency reports Skilled as its only source");
}

void rejections()
{
    auto p = ready("fighter", "criminal");
    // Duplicate, short, long, unknown, unprefixed, and already-known picks.
    rejects([&]
    {
        p.advance(1, pick(p, {"skill:arcana", "skill:arcana", "skill:nature"}));
    });
    rejects([&]
    {
        p.advance(1, pick(p, {"skill:arcana", "skill:nature"}));
    });
    rejects([&]
    {
        p.advance(1, pick(p, {"skill:arcana", "skill:nature", "skill:religion", "skill:medicine"}));
    });
    rejects([&]
    {
        p.advance(1, pick(p, {"skill:arcana", "skill:nature", "skill:not_a_skill"}));
    });
    rejects([&]
    {
        p.advance(1, pick(p, {"arcana", "nature", "medicine"}));
    });
    rejects([&]
    {
        p.advance(1, pick(p, {"skill:stealth", "skill:nature", "skill:arcana"}));
    });
    rejects([&]
    {
        p.advance(1, pick(p, {"tool:thieves_tools", "skill:nature", "skill:arcana"}));
    });
    auto bare = p.default_advancement(1);
    bare.feat = "skilled";
    bare.abilities = {};
    rejects([&]
    {
        p.advance(1, bare);
    });
    auto paid = pick(p, {"skill:arcana", "skill:nature", "skill:medicine"});
    paid.abilities[0] = 2;
    rejects([&]
    {
        p.advance(1, paid);
    });
    check(p.member(1).character.sheet().level == 3,
          "Every rejected Skilled choice leaves the character untouched");
    p.advance(1, pick(p, {"skill:arcana", "skill:nature", "skill:medicine"}));
    check(p.member(1).character.sheet().level == 4,
          "A valid choice still applies after rejections");
}

void persistence()
{
    auto p = ready("wizard", "sage");
    p.advance(1, pick(p, {"skill:stealth", "tool:poisoners_kit", "tool:disguise_kit"}));
    const auto sheet = p.member(1).character.sheet();
    check(p.rule_module().character_profile(sheet, {}).data.starts_with("PC42 "),
          "A Skilled profile is written in the current profile format");
    const auto saved = encode_campaign(p, nullptr, "skilled-persist");
    CampaignParty reloaded(module());
    reloaded.restore(decode_campaign(saved, *srd5::character_rules(), p.rule_module(),
                                     "skilled-persist", nullptr)
                     .party);
    check(encode_campaign(reloaded, nullptr, "skilled-persist") == saved,
          "A Skilled campaign reload is canonical");
    const auto back = reloaded.member(1).character.sheet();
    check(back.grants == sheet.grants, "Reload preserves every Skilled grant and its provenance");
    check(probe(back, 1, "stealth", {}).proficiency == probe(sheet, 1, "stealth", {}).proficiency &&
          probe(back, 4, {}, "poisoners_kit").proficiency > 0,
          "Skilled check effects survive a reload");
}

void run()
{
    for (const auto &[name, test] : std::initializer_list<std::pair<const char *, void (*)()>>
{
    {"options_and_grants", options_and_grants},
    {"check_effects", check_effects},
    {"rejections", rejections},
    {"persistence", persistence}
})
    try
    {
        test();
    }
    catch (const std::exception &e)
    {
        throw std::runtime_error(std::string("Skilled/") + name + ": " + e.what());
    }
}
} // namespace skilled_checks
