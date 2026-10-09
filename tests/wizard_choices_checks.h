// Included by spell_access_tests.cpp; exercises the actual public rules/Core paths.
void wizard_choices_checks()
{
    auto rules = module();
    auto creation_rules = srd5::character_rules();
    auto draft = hero().creation_data();
    draft.cantrips = std::vector<std::string> {"fire_bolt", "ray_of_frost", "chill_touch"};
    draft.spells = SpellChoices
    {
        {{"spellbook:1", {"magic_missile"}}}, std::vector<std::string>{"magic_missile"}, {}, {}};
    draft.training = {{"class:wizard", {"medicine", "nature"}}};
    CampaignParty party(module());
    const auto id = party.add_pc(Character(*creation_rules, draft, {}));
    party.award_experience(2700, "wizard-choice-xp");
    const auto original = saved(party);
    check(original.starts_with("OPENGOLD-CAMPAIGN 25\n"),
          "Explicit independent creation uses the current campaign format");
    SpellChoices bad;
    bad.prepared = std::vector<std::string> {};
    rejects(
        [&]
    {
        party.choose_spells(id, bad);
    });
    check(saved(party) == original,
          "Spell choices without a completed Long Rest reject atomically");
    auto second = party.default_advancement(id);
    check(second.spell_learning.has_value(), "New Wizard defaults use independent knowledge");
    party.advance(id, second);
    (void)party.rest(RestKind::long_rest);
    check(party.state().spell_rest &&
          party.state().spell_rest->members == std::vector<MemberId> {id},
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
    // Every available book spell stays prepared; only the cantrip changes.
    rest.prepared = party.member(id).character.sheet().prepared_spells;
    rest.replace_cantrip = "fire_bolt";
    rest.replacement = "poison_spray";
    const auto preview = party.preview_spell_choices(id, rest);
    check(saved(party) == after_rest && preview.vitals == vitals,
          "Rest spell preview preserves wounds, resources, RNG and live choices");
    auto round = decode_campaign(after_rest, *creation_rules, *rules, "spell-access", nullptr);
    CampaignParty restored(module());
    restored.restore(round.party);
    check(saved(restored) == after_rest, "Unresolved completed-rest entitlement roundtrips");
    restored.choose_spells(id, rest);
    party.choose_spells(id, rest);
    check(saved(restored) == saved(party), "Choice commits identically after reload");
    check(!party.state().spell_rest && party.member(id).vitals == vitals,
          "Spell changes spend entitlement, never recover resources");
    const auto used = saved(party);
    rejects(
        [&]
    {
        party.choose_spells(id, rest);
    });
    check(saved(party) == used, "Repeated rest choice is rejected atomically");
    const auto replaced = rules->spell_access(party.member(id).character.sheet());
    check(std::none_of(replaced.cantrips.begin(), replaced.cantrips.end(),
                       [](const auto & s)
    {
        return s.id == "fire_bolt";
    }) &&
    std::any_of(replaced.cantrips.begin(), replaced.cantrips.end(),
                [](const auto & s)
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
    check(saved(party) == used, "Wizard advancement requires independent spell learning choices");
    party.advance(id, third);
    party.advance(id, party.default_advancement(id));
    const auto fourth = rules->spell_access(party.member(id).character.sheet());
    // Two book spells per level: Magic Missile plus six more, all prepared.
    check(fourth.cantrips.size() == 4 && fourth.spellbook.size() == 7 &&
          fourth.prepared.size() == 7,
          "Level four adds one cantrip and retains known/prepared book spells");
    check(fourth.cantrip_choices == 4 && fourth.spellbook_choices == 12 &&
          fourth.prepared_choices == 7,
          "Incomplete catalog never reduces SRD entitlements");
    const auto bytes = saved(party);
    restored.restore(
        decode_campaign(bytes, *creation_rules, *rules, "spell-access", nullptr).party);
    check(saved(restored) == bytes, "Rest edit before later advancement replays chronologically");
    (void)party.rest(RestKind::short_rest);
    check(!party.state().spell_rest, "Short Rest never grants spell replacement");
    party.finish_short_rest(party.state().short_rest->ticket);
    CampaignParty resting(module());
    const auto resting_id = resting.add_pc(hero());
    check(!resting.state().spell_rest,
          "A Wizard without a completed Long Rest has no spell-choice entitlement");
    const auto canceled = saved(resting);
    rejects(
        [&]
    {
        resting.choose_spells(resting_id, rest);
    });
    check(saved(resting) == canceled, "Replacement without a completed rest rejects atomically");
    auto forged = party.checkpoint();
    forged.next_rest_session = 1;
    rejects(
        [&]
    {
        restored.restore(forged);
    });
}

// Writes the campaign tests/wizard_choices_view_tests.gd loads: one fresh
// level-one Wizard with enough experience to reach level three through the
// real level-up controls. Requires the original assets for their identity.
void write_wizard_ui_fixture()
{
    const auto *directory = std::getenv("OPENGOLD_GAME_DIR");
    if (!directory || !*directory)
        return;
    auto draft = hero().creation_data();
    draft.cantrips = std::vector<std::string> {"fire_bolt", "ray_of_frost", "chill_touch"};
    draft.spells = SpellChoices
    {
        {{"spellbook:1", {"magic_missile"}}}, std::vector<std::string>{"magic_missile"}, {}, {}};
    draft.training = {{"class:wizard", {"medicine", "nature"}}};
    CampaignParty party(module());
    (void)party.add_pc(Character(*srd5::character_rules(), draft, {}));
    party.award_experience(2700, "wizard-ui");
    write_campaign_file(std::filesystem::path(OPENGOLD_BINARY_DIR) / "wizard-choices-ui.ogs",
                        encode_campaign(party, nullptr, campaign_asset_identity(directory)));
}
