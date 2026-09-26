// Included by spell_access_tests.cpp; exercises the actual public rules/Core paths.
void wizard_choices_checks()
{
    auto rules = module();
    auto creation_rules = srd5::character_rules();
    for (unsigned level = 1; level <= 4; ++level)
    {
        auto old = decode_campaign(read(root / ("tests/fixtures/campaign-wizard-choices-level" +
                                                std::to_string(level) + ".ogs")),
                                   *creation_rules, *rules, "wizard-choices-baseline", nullptr);
        CampaignParty party(module());
        party.restore(old.party);
        const auto &m = party.member(1);
        const auto access = rules->spell_access(m.character.sheet());
        check(access.cantrips.size() == 3 && access.spellbook.size() == (level >= 3 ? 3 : 1),
              "Real old-writer knowledge stays unchanged");
        check(m.vitals.hit_points == m.character.sheet().hit_points - 2,
              "Real old-writer wounds survive");
        const auto recovery = rules->recovery_info(m.character.sheet(), m.vitals);
        check(std::any_of(recovery.resources.begin(), recovery.resources.end(),
                          [](const auto &p)
                          {
                              return p.id == "arcane_recovery" && p.remaining == 0;
                          }),
              "Old Arcane Recovery expenditure survives");
        const auto bytes = saved(party);
        CampaignParty again(module());
        again.restore(
            decode_campaign(bytes, *creation_rules, *rules, "spell-access", nullptr).party);
        check(saved(again) == bytes, "Old Wizard canonical campaign roundtrip");
        if (level == 3)
            check(access.prepared == std::vector<std::string>({"scorching_ray", "blindness"}),
                  "Old advancement may retain historical replaced preparation");
        if (level == 4)
            check(access.prepared == std::vector<std::string>({"magic_missile"}),
                  "Old unprepared knowledge remains in the book");
    }
    auto old_combat =
        rules->restore(read(root / "tests/fixtures/combat-wizard-choices-before.save"));
    check(old_combat->submit(command(*old_combat, "end")), "Old Wizard combat ends turn");
    check(old_combat->save() ==
              rules->restore(read(root / "tests/fixtures/combat-wizard-choices-continued.save"))
                  ->save(),
          "Actual 0.6.50 combat RNG, effects, expenditure continue exactly");
    auto draft = hero().creation_data();
    draft.cantrips = std::vector<std::string>{"fire_bolt", "ray_of_frost", "chill_touch"};
    draft.spells = SpellChoices{
        {{"spellbook:1", {"magic_missile"}}}, std::vector<std::string>{"magic_missile"}, {}, {}};
    draft.training = {{"origin:languages", {"elvish", "dwarvish"}},
                      {"class:wizard", {"medicine", "nature"}}};
    CampaignParty party(module());
    const auto id = party.add_pc(Character(*creation_rules, draft, {}));
    party.award_experience(2700, "wizard-choice-xp");
    const auto original = saved(party);
    check(original.starts_with("OPENGOLD-CAMPAIGN 16\n"),
          "Explicit independent creation uses campaign16");
    SpellChoices bad;
    bad.prepared = std::vector<std::string>{};
    rejects(
        [&]
        {
            party.choose_spells(id, bad);
        });
    check(saved(party) == original, "Pending learning cannot change preparation");
    bad = {};
    bad.learning["spellbook:1"] = {"scorching_ray"};
    rejects(
        [&]
        {
            party.choose_spells(id, bad);
        });
    check(saved(party) == original,
          "Acquisition-level eligibility rejects late spell in level-one entitlement atomically");
    auto second = party.default_advancement(id);
    check(second.spell_learning.has_value(), "New Wizard defaults use independent knowledge");
    party.advance(id, second);
    (void)party.rest(RestKind::long_rest);
    check(party.state().spell_rest &&
              party.state().spell_rest->members == std::vector<MemberId>{id},
          "Completed Long Rest grants a real choice window");
    const auto after_rest = saved(party);
    const auto vitals = party.member(id).vitals;
    rejects(
        [&]
        {
            party.advance_time(1);
        });
    rejects(
        [&]
        {
            party.begin_combat();
        });
    check(saved(party) == after_rest, "Rest choices block time/combat without spending them");
    SpellChoices rest;
    rest.prepared = std::vector<std::string>{"magic_missile"};
    rest.replace_cantrip = "fire_bolt";
    rest.replacement = "poison_spray";
    const auto preview = party.preview_spell_choices(id, rest, true);
    check(saved(party) == after_rest && preview.vitals == vitals,
          "Rest spell preview preserves wounds, resources, RNG and live choices");
    auto round = decode_campaign(after_rest, *creation_rules, *rules, "spell-access", nullptr);
    CampaignParty restored(module());
    restored.restore(round.party);
    check(saved(restored) == after_rest, "Unresolved completed-rest entitlement roundtrips");
    restored.choose_spells(id, rest, true);
    party.choose_spells(id, rest, true);
    check(saved(restored) == saved(party), "Choice commits identically after reload");
    check(!party.state().spell_rest && party.member(id).vitals == vitals,
          "Spell changes spend entitlement, never recover resources");
    const auto used = saved(party);
    rejects(
        [&]
        {
            party.choose_spells(id, rest, true);
        });
    check(saved(party) == used, "Repeated rest choice is rejected atomically");
    const auto replaced = rules->spell_access(party.member(id).character.sheet());
    check(std::none_of(replaced.cantrips.begin(), replaced.cantrips.end(),
                       [](const auto &s)
                       {
                           return s.id == "fire_bolt";
                       }) &&
              std::any_of(replaced.cantrips.begin(), replaced.cantrips.end(),
                          [](const auto &s)
                          {
                              return s.id == "poison_spray" && s.acquired_level == 2;
                          }),
          "Replacement preserves entitlement and records actual learning level");
    auto c = battle(*rules, party.member(id).character.sheet(), party.member(id).vitals);
    check(has(*c, "poison_spray") && !has(*c, "fire_bolt"), "Replacement reaches actual casting");
    check(rules->restore(c->save())->save() == c->save(), "PC34 replacement combat roundtrip");
    auto third = party.default_advancement(id);
    auto invalid = third;
    invalid.spells = {"scorching_ray", "blindness"};
    rejects(
        [&]
        {
            party.advance(id, invalid);
        });
    check(saved(party) == used, "New level-up cannot replace existing preparation");
    invalid = third;
    invalid.spell_learning = TrainingChoices{};
    rejects(
        [&]
        {
            party.advance(id, invalid);
        });
    check(saved(party) == used, "Preparation cannot learn missing book entries");
    invalid = third;
    invalid.spell_learning.reset();
    rejects(
        [&]
        {
            party.advance(id, invalid);
        });
    check(saved(party) == used,
          "Current player advancement cannot use the historical learning bypass");
    party.advance(id, third);
    party.advance(id, party.default_advancement(id));
    const auto fourth = rules->spell_access(party.member(id).character.sheet());
    check(fourth.cantrips.size() == 4 && fourth.spellbook.size() == 3 &&
              fourth.prepared.size() == 3,
          "Level four adds one cantrip and retains known/prepared book spells");
    check(fourth.cantrip_choices == 4 && fourth.spellbook_choices == 12 &&
              fourth.prepared_choices == 7,
          "Incomplete catalog never reduces SRD entitlements");
    const auto bytes = saved(party);
    restored.restore(
        decode_campaign(bytes, *creation_rules, *rules, "spell-access", nullptr).party);
    check(saved(restored) == bytes, "Rest edit before later advancement replays chronologically");
    const auto trained = party.member(id).character.preview_training(
        *creation_rules, *rules, party.member(id).character.training_choices(), false);
    check(trained.sheet().grants == party.member(id).character.sheet().grants &&
              trained.sheet().prepared_spells == party.member(id).character.sheet().prepared_spells,
          "Training replay preserves spell edits interleaved with advancement");
    (void)party.rest(RestKind::short_rest);
    check(!party.state().spell_rest, "Short Rest never grants spell replacement");
    party.finish_short_rest(party.state().short_rest->ticket);
    CampaignParty pending(module());
    const auto pending_id = pending.add_pc(hero());
    const auto unfilled = saved(pending);
    SpellChoices knowledge;
    knowledge.learning["cantrips:1"] = {"ray_of_frost", "chill_touch"};
    const auto candidate = pending.preview_spell_choices(pending_id, knowledge);
    check(saved(pending) == unfilled && candidate.vitals == pending.member(pending_id).vitals,
          "Old missing knowledge preview preserves live state");
    pending.choose_spells(pending_id, knowledge);
    check(rules->spell_access(pending.member(pending_id).character.sheet()).cantrips.size() == 3,
          "Old missing knowledge completes without replacing old choices");
    bad = knowledge;
    bad.learning["cantrips:1"] = {"fire_bolt"};
    rejects(
        [&]
        {
            pending.choose_spells(pending_id, bad);
        });
    auto pending_bytes = saved(pending);
    restored.restore(
        decode_campaign(pending_bytes, *creation_rules, *rules, "spell-access", nullptr).party);
    check(saved(restored) == pending_bytes, "Pending-knowledge event roundtrip");
    const auto interrupted = pending.begin_rest(RestKind::long_rest);
    check(interrupted.has_value(), "Wizard starts interruption fixture");
    (void)pending.advance_rest(*interrupted, 70 * 60000, RestWork::sleep);
    pending.interrupt_rest(pending.state().rest_activity->ticket, RestInterruption::initiative);
    check(pending.state().short_rest && !pending.state().spell_rest,
          "An interrupted Wizard Long Rest grants only earned Short Rest benefits");
    pending.finish_short_rest(pending.state().short_rest->ticket);
    pending.abandon_rest(pending.state().rest_activity->ticket);
    check(!pending.state().spell_rest,
          "Canceled Wizard Long Rest grants no spell-choice entitlement");
    const auto canceled = saved(pending);
    rejects(
        [&]
        {
            pending.choose_spells(pending_id, rest, true);
        });
    check(saved(pending) == canceled, "Canceled-rest replacement rejects atomically");
    auto forged = party.checkpoint();
    forged.next_rest_session = 1;
    rejects(
        [&]
        {
            restored.restore(forged);
        });
}

void write_wizard_ui_fixture()
{
    const auto *directory = std::getenv("OPENGOLD_GAME_DIR");
    if (!directory || !*directory)
        return;
    auto rules = module();
    CampaignParty party(module());
    for (unsigned level : {1u, 4u})
    {
        auto old =
            decode_campaign(read(root / ("tests/fixtures/campaign-wizard-choices-level" +
                                         std::to_string(level) + ".ogs")),
                            *srd5::character_rules(), *rules, "wizard-choices-baseline", nullptr)
                .party.roster.front();
        const auto id = party.add_pc(old.character);
        auto state = party.checkpoint();
        state.roster.back().vitals = old.vitals;
        party.restore(std::move(state));
    }
    party.award_experience(2700, "wizard-ui");
    write_campaign_file(std::filesystem::path(OPENGOLD_BINARY_DIR) / "wizard-choices-ui.ogs",
                        encode_campaign(party, nullptr, campaign_asset_identity(directory)));
}

void verify_wizard_ui(const char *file)
{
    const auto *directory = std::getenv("OPENGOLD_GAME_DIR");
    check(directory && *directory, "UI comparison requires game assets");
    auto rules = module();
    auto creation = srd5::character_rules();
    const auto assets = campaign_asset_identity(directory);
    CampaignParty expected(module());
    expected.restore(decode_campaign(read_campaign_file(std::filesystem::path(OPENGOLD_BINARY_DIR) /
                                                        "wizard-choices-ui.ogs"),
                                     *creation, *rules, assets, nullptr)
                         .party);
    expected.advance(1, expected.default_advancement(1));
    expected.advance(1, expected.default_advancement(1));
    SpellChoices missing;
    missing.learning["cantrips:4"] = {"poison_spray"};
    expected.choose_spells(2, missing);
    CampaignParty actual(module());
    actual.restore(
        decode_campaign(read_campaign_file(file), *creation, *rules, assets, nullptr).party);
    auto selection = expected.checkpoint();
    selection.selected = actual.state().selected;
    expected.restore(std::move(selection));
    check(
        encode_campaign(actual, nullptr, assets) == encode_campaign(expected, nullptr, assets),
        "UI output changes exactly the approved spell/advancement choices; wounds, resources, equipment, RNG and other state match");
    std::cout << "Wizard UI persistence verified\n";
}
