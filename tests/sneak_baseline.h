// Actual pre-Sneak writer fixtures; included after the normal creation helpers.
namespace sneak_baseline
{
void freeze()
{
    auto rules = module();
    check(rules->identity().version == "0.6.35", "Freeze requires actual pre-Sneak writer");
    auto d = draft("rogue", "soldier");
    d.training = choices();
    d.training["class:rogue:expertise"] = {"investigation", "perception"};
    d.training["background:soldier:gaming_set"] = {"dice"};
    CampaignParty party(module());
    party.add_pc(hero(d));
    party.add_pc(hero(d));
    party.award_experience(600, "sneak-baseline");
    party.advance(2, party.default_advancement(2));
    auto state = party.checkpoint();
    for (auto &m : state.roster)
    {
        m.vitals.hit_points -= 2;
        m.wealth[3] = 37;
    }
    party.restore(state);
    const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR) / "tests/fixtures";
    auto write = [&](const char *name, const std::string &bytes)
    {
        std::ofstream out(root / name);
        out << bytes;
        check(bool(out), "Write actual prior Sneak fixture");
    };
    write("campaign-v11-sneak-before.ogs", encode_campaign(party, nullptr, "sneak-before"));
    auto combat = cunning_checks::battle(party.member(2).character);
    cunning_checks::act(*combat, "cunning_dash");
    cunning_checks::act(*combat, "melee");
    check(bool(combat->snapshot().savage_attack_choice),
          "Real Rogue weapon hit awaits existing Savage decision");
    write("combat-v15-sneak-before.save", combat->save());
    cunning_checks::act(*combat, "savage_use");
    write("combat-v15-sneak-second.save", combat->save());
    cunning_checks::act(*combat, "savage_second");
    write("combat-v15-sneak-resolved.save", combat->save());
}
void verify()
{
    auto rules = module();
    auto creation = srd5::character_rules();
    const auto prior = fixture("campaign-v11-sneak-before.ogs");
    CampaignParty party(module());
    party.restore(decode_campaign(prior, *creation, *rules, "sneak-before", nullptr).party);
    auto body = [](const auto &text)
    {
        return text.substr(text.find('\n', text.find('\n') + 1) + 1);
    };
    auto expected = body(prior);
    replace(expected, "0.6.35", rules->identity().version);
    check(body(encode_campaign(party, nullptr, "sneak-before")) ==
              test::with_sneak_attack_grants(expected),
          "Actual pre-Sneak campaign changes only fixed Sneak grant and module identity/checksum");
    for (const auto &m : party.state().roster)
        check(m.character.sheet().level == m.id && m.character.sheet().training.complete &&
                  m.vitals.hit_points == m.character.sheet().hit_points - 2 && m.wealth[3] == 37,
              "Both ordinary Rogue levels preserve training, wounds and wealth");
    auto upgraded = [&](const char *name)
    {
        auto text = fixture(name);
        replace(text, "0.6.35", rules->identity().version);
        return text;
    };
    auto combat = rules->restore(fixture("combat-v15-sneak-before.save"));
    check(combat->save() == upgraded("combat-v15-sneak-before.save"),
          "Pre-Sneak pending hit retains exact continuation");
    const auto actor = cunning_checks::unit(*combat);
    check(!actor.action && !actor.bonus_action && actor.movement_feet == 60,
          "Existing attack and Bonus Dash expenditures remain spent");
    cunning_checks::act(*combat, "savage_use");
    check(combat->save() == upgraded("combat-v15-sneak-second.save"),
          "Actual prior second weapon roll and RNG remain exact");
    cunning_checks::act(*combat, "savage_second");
    check(combat->save() == upgraded("combat-v15-sneak-resolved.save"),
          "Actual prior resolved damage and budgets remain exact");
}
} // namespace sneak_baseline
