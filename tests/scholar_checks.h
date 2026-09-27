// Included by training_tests.cpp; shares its actual campaign/training helpers.
namespace scholar_checks
{
constexpr auto source = "class:wizard:scholar";

Character wizard(std::string_view proficient)
{
    auto d = draft("wizard", "soldier");
    d.training = {{"class:wizard", {std::string(proficient), "insight"}},
                  {"origin:languages", {"elvish", "dwarvish"}},
                  {"background:soldier:gaming_set", {"dice"}}};
    return hero(d);
}

void choices_and_sources()
{
    auto rules = module();
    for (const auto *name :
         {"arcana", "history", "investigation", "medicine", "nature", "religion"})
    {
        CampaignParty party(module());
        const auto id = party.add_pc(wizard(name));
        party.award_experience(2700, "scholar");
        const auto &initial = party.member(id).character.sheet();
        const auto groups = rules->advancement_options(initial).training;
        check(groups.size() == 1 && groups[0].acquired_level == 2 &&
                  groups[0].options.size() == 1 && groups[0].options[0].id == name,
              "Scholar filters all six choices by actual proficiency");
        const auto before = encode_campaign(party, nullptr, "scholar");
        auto choice = party.default_advancement(id);
        choice.training.clear();
        rejects(
            [&]
            {
                party.advance(id, choice);
            });
        choice.training[source] = {"insight"};
        rejects(
            [&]
            {
                party.advance(id, choice);
            });
        choice.training[source] = {name, name};
        rejects(
            [&]
            {
                party.advance(id, choice);
            });
        choice.training = {{"forged", {name}}};
        rejects(
            [&]
            {
                party.advance(id, choice);
            });
        check(encode_campaign(party, nullptr, "scholar") == before,
              "Invalid Scholar choices are atomic");
        choice.training = {{source, {name}}};
        party.advance(id, choice);
        for (unsigned level = 2; level <= 4; ++level)
        {
            if (level > 2)
                party.advance(id, party.default_advancement(id));
            const auto &sheet = party.member(id).character.sheet();
            const auto &trained = skill(sheet, name);
            const auto check_result = rules->ability_check(sheet, {}, trained.ability, name);
            check(trained.expertise && check_result.expertise && check_result.proficiency == 4 &&
                      trained.bonus == sheet.modifiers[trained.ability] + 4,
                  "Expertise doubles proficiency exactly once at every supported level");
            check(std::any_of(trained.sources.begin(), trained.sources.end(),
                              [](const auto &grant)
                              {
                                  return grant.source_id == source && grant.level == 2;
                              }),
                  "Scholar source remains level two");
            const auto bytes = encode_campaign(party, nullptr, "scholar");
            check(bytes.starts_with("OPENGOLD-CAMPAIGN 16\n"),
                  "Independent Wizard spell history selects campaign format 16");
            CampaignParty restored(module());
            restored.restore(
                decode_campaign(bytes, *srd5::character_rules(), *rules, "scholar", nullptr).party);
            check(encode_campaign(restored, nullptr, "scholar") == bytes,
                  "Current Scholar campaign round trip is exact");
            auto forged = sheet;
            for (auto &g : forged.grants)
                if (g.source_id == source)
                    g.level = 1;
            rejects(
                [&]
                {
                    (void)rules->character_profile(forged, {});
                });
            forged = sheet;
            forged.grants.push_back({"expertise:" + std::string(name), source, 2, {}});
            rejects(
                [&]
                {
                    (void)rules->character_profile(forged, {});
                });
            forged = sheet;
            std::erase_if(forged.grants,
                          [&](const auto &g)
                          {
                              return g.id == "skill:" + std::string(name);
                          });
            rejects(
                [&]
                {
                    (void)rules->character_profile(forged, {});
                });
        }
    }
    for (const auto *klass : {"fighter", "cleric", "rogue"})
    {
        const auto character = hero(draft(klass));
        check(rules->training_options(character.sheet()).empty() &&
                  rules->advancement_options(character.sheet()).training.empty(),
              "Other classes have no Scholar entitlement");
    }
}

auto prior_module()
{
    std::ifstream in(std::filesystem::path(OPENGOLD_SOURCE_DIR) /
                     "data/rules/srd-5.2.1/combat.rules");
    return srd5::parse_content(
        std::string(std::istreambuf_iterator<char>(in), {}) +
        "\ncreature recovery_target 1 1000 0 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n");
}

void previous_writer()
{
    auto rules = prior_module();
    for (unsigned level = 2; level <= 4; ++level)
    {
        const auto file = "campaign-scholar-level" + std::to_string(level) + ".ogs";
        CampaignParty party(prior_module());
        party.restore(decode_campaign(fixture(file.c_str()), *srd5::character_rules(), *rules,
                                      "scholar-baseline", nullptr)
                          .party);
        party.finish_short_rest(party.state().short_rest->ticket);
        auto wounded = party.checkpoint();
        wounded.roster[0].vitals.hit_points -= 3;
        party.restore(std::move(wounded));
        const auto before = party.checkpoint();
        const auto &member = party.member(1);
        check(member.character.sheet().level == int(level) &&
                  !member.character.sheet().training.complete &&
                  !skill(member.character.sheet(), "medicine").expertise,
              "Actual old saves retain attained level and pending Scholar");
        check(member.character.advancements()[0].training.empty(),
              "Old choices are never invented");
        auto editor =
            CharacterCreator(srd5::character_rules(), member.character, party.rule_module());
        editor.training_choice("origin:languages", "elvish", true);
        editor.training_choice("origin:languages", "dwarvish", true);
        editor.training_choice(source, "medicine", true);
        check(editor.training_complete(), "Review Training includes missing advancement choices");
        party.complete_training(1, editor.rules(), editor.draft().training);
        const auto &after = party.member(1);
        check(after.vitals == before.roster[0].vitals &&
                  after.equipped == before.roster[0].equipped &&
                  after.equipment == before.roster[0].equipment,
              "Review preserves wounds, equipment and spent Arcane Recovery/slots");
        check(after.character.sheet().training.complete &&
                  skill(after.character.sheet(), "medicine").expertise,
              "Review grants selected Scholar Expertise");
        check(after.character.creation_data().training.count(source) == 0 &&
                  after.character.advancements()[0].training.at(source) ==
                      std::vector<std::string>{"medicine"},
              "Level-two choice is stored in advancement history, never creation");
        auto changed = after.character.training_choices();
        changed[source] = {"nature"};
        rejects(
            [&]
            {
                (void)after.character.preview_training(editor.rules(), *rules, changed, false);
            });
        CharacterCreator locked(srd5::character_rules(), after.character, party.rule_module());
        rejects(
            [&]
            {
                locked.training_choice(source, "medicine", false);
            });
        const auto bytes = encode_campaign(party, nullptr, "scholar-baseline");
        CampaignParty restored(prior_module());
        restored.restore(
            decode_campaign(bytes, editor.rules(), *rules, "scholar-baseline", nullptr).party);
        check(bytes == encode_campaign(restored, nullptr, "scholar-baseline"),
              "Reviewed legacy character reloads exactly");
    }
    auto expected = [&](std::string bytes)
    {
        replace(bytes, "0.6.49", rules->identity().version);
        return bytes;
    };
    auto combat = rules->restore(fixture("combat-scholar-before.save"));
    check(combat->save() == expected(fixture("combat-scholar-before.save")),
          "Actual old combat round trip is unchanged");
    auto act = [&](std::string_view verb)
    {
        for (const auto &command : combat->legal_commands())
            if (command.verb == verb)
            {
                check(combat->submit(command), "Accept continued command");
                return;
            }
        throw std::runtime_error("Missing continued command");
    };
    while (combat->snapshot().actor != 1)
        act("end");
    act("magic_missile");
    check(combat->save() == expected(fixture("combat-scholar-continued.save")),
          "Actual old combat continues with identical resources and RNG");
}

void medicine_combat()
{
    auto rules = prior_module();
    auto character = wizard("medicine");
    VitalState scratch;
    check(character.advance(*rules, scratch), "Scholar medic advances normally");
    auto profile = rules->character_profile(character.sheet(), {}).data;
    check(profile.starts_with("PC33 "), "Scholar combat uses explicit profile version");
    auto forged = profile;
    forged.replace(0, 4, "PC32");
    auto encounter = [&](const std::string &data)
    {
        return Encounter{{8, 8, std::vector<std::uint8_t>(64)},
                         {{1, "campaign-character", "Scholar", 0, {1, 1}, data},
                          {2,
                           "recovery_target",
                           "Patient",
                           0,
                           {2, 1},
                           {},
                           VitalState{0, false, "SRD5 0 0 0 1 1 0 0 6000 0 FX1 1 0"}},
                          {99, "vanguard", "Enemy", 1, {6, 6}}}};
    };
    rejects(
        [&]
        {
            (void)rules->create(encounter(forged), 1);
        });
    bool success = false, failure = false;
    for (unsigned seed = 0; seed < 80 && !(success && failure); ++seed)
    {
        auto combat = rules->create(encounter(profile), seed);
        while (combat->snapshot().actor != 1)
        {
            bool ended = false;
            for (const auto &c : combat->legal_commands())
                if (c.verb == "end")
                {
                    check(combat->submit(c), "End prior turn");
                    ended = true;
                    break;
                }
            check(ended, "Reach medic turn");
        }
        auto commands = combat->legal_commands();
        const auto found = std::find_if(commands.begin(), commands.end(),
                                        [](const auto &c)
                                        {
                                            return c.verb == "stabilize" && c.target == 2;
                                        });
        if (found == commands.end())
            continue;
        auto copy = rules->restore(combat->save());
        check(combat->submit(*found) && copy->submit(*found),
              "Scholar uses actual Stabilize action");
        check(combat->save() == copy->save(),
              "Scholar combat continuation preserves roll and recovery RNG");
        bool observed = false;
        for (const auto &message : combat->snapshot().log_messages)
        {
            int roll = -1, total = -1;
            for (const auto &arg : message.arguments)
            {
                if (arg.name == "roll")
                    roll = std::stoi(arg.value);
                if (arg.name == "total")
                    total = std::stoi(arg.value);
            }
            if (roll >= 0 && total >= 0)
            {
                check(total - roll == character.sheet().modifiers[4] + 4,
                      "Actual Medicine roll uses Scholar Expertise");
                observed = true;
                success |= total >= 10;
                failure |= total < 10;
            }
        }
        check(observed, "Actual check log reports roll and total");
    }
    check(success && failure, "Scholar Medicine success and failure exercised");
}

void write_ui_fixture()
{
    const auto *directory = std::getenv("OPENGOLD_GAME_DIR");
    if (!directory || !*directory)
        return;
    CampaignParty party(module());
    party.add_pc(wizard("medicine"));
    auto rules = prior_module();
    for (unsigned level : {2u, 4u})
    {
        const auto name = "campaign-scholar-level" + std::to_string(level) + ".ogs";
        auto old = decode_campaign(fixture(name.c_str()), *srd5::character_rules(), *rules,
                                   "scholar-baseline", nullptr)
                       .party.roster[0];
        auto choices = old.character.training_choices();
        choices["origin:languages"] = {"elvish", "dwarvish"};
        old.character =
            old.character.preview_training(*srd5::character_rules(), *rules, choices, false);
        const auto id = party.add_pc(old.character);
        auto state = party.checkpoint();
        state.roster.back().vitals = old.vitals;
        state.roster.back().vitals.hit_points -= 3;
        party.restore(std::move(state));
    }
    party.award_experience(2700, "scholar-ui");
    write_campaign_file(std::filesystem::path(OPENGOLD_BINARY_DIR) / "scholar-ui.ogs",
                        encode_campaign(party, nullptr, campaign_asset_identity(directory)));
}

void verify_ui(const char *path)
{
    const auto *directory = std::getenv("OPENGOLD_GAME_DIR");
    check(directory && *directory, "UI verification needs game directory");
    const auto assets = campaign_asset_identity(directory);
    auto rules = module();
    auto creation = srd5::character_rules();
    CampaignParty expected(module());
    expected.restore(decode_campaign(read_campaign_file(std::filesystem::path(OPENGOLD_BINARY_DIR) /
                                                        "scholar-ui.ogs"),
                                     *creation, *rules, assets, nullptr)
                         .party);
    expected.advance(1, expected.default_advancement(1));
    for (MemberId id : {2u, 3u})
    {
        auto selected = expected.member(id).character.training_choices();
        selected[source] = {"medicine"};
        expected.complete_training(id, *creation, selected);
    }
    auto saved = decode_campaign(read_campaign_file(path), *creation, *rules, assets, nullptr);
    auto state = expected.checkpoint();
    state.selected = saved.party.selected;
    expected.restore(std::move(state));
    CampaignParty actual(module());
    actual.restore(std::move(saved.party));
    check(
        encode_campaign(actual, nullptr, assets) == encode_campaign(expected, nullptr, assets),
        "Actual UI result changes only Scholar choices and the requested level-up; preserves every other campaign field");
    std::cout << "Scholar UI persistence verified\n";
}

void unchanged_starting_review()
{
    auto rules = module();
    auto character = hero(draft());
    CharacterCreator editor(srd5::character_rules(), character, *rules);
    editor.training_choice("class:rogue", "acrobatics", true);
    editor.training_choice("class:rogue:expertise", "acrobatics", true);
    editor.training_choice("class:rogue", "acrobatics", false);
    check(!editor.draft().training.contains("class:rogue:expertise"),
          "Starting-only Review Training retains dependent-choice pruning");
    check(character.creation_data().training.empty(),
          "Review editor never mutates the original character");
}

void run()
{
    unchanged_starting_review();
    choices_and_sources();
    previous_writer();
    medicine_combat();
    write_ui_fixture();
}
} // namespace scholar_checks
