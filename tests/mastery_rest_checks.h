namespace mastery_rest_checks
{
void ui_fixture()
{
    const auto output = std::getenv("OPENGOLD_MASTERY_REST_FIXTURE");
    if (!output)
        return;
    const auto assets = std::getenv("OPENGOLD_GAME_DIR");
    check(assets, "UI fixture needs original assets");
    auto party = std::make_shared<CampaignParty>(module());
    party->add_pc(mastery_grant_checks::chosen("fighter"));
    auto town = por::RolfTourSession::load(assets);
    town.campaign_party(party);
    for (unsigned n = 0; n < 1000 && !town.can_leave(); ++n)
    {
        const auto state = town.snapshot();
        if (state.phase == por::TourPhase::awaiting_continue)
            town.continue_dialogue(state.continue_ticket);
        else if (state.phase == por::TourPhase::awaiting_input)
            town.choose(state.continue_ticket, 0);
        else
            town.advance(1);
    }
    check(town.can_leave(), "Original town reaches a saveable boundary");
    check(bool(party->rest(RestKind::long_rest)) && party->state().training_rest,
          "Completed rest has pending mastery");
    const auto bytes = encode_campaign(*party, &town, campaign_asset_identity(assets));
    auto prototype = por::RolfTourSession::load(assets);
    const auto loaded = decode_campaign(bytes, *srd5::character_rules(), party->rule_module(),
                                        campaign_asset_identity(assets), &prototype);
    check(loaded.party.training_rest.has_value() && loaded.town->can_leave(),
          "UI fixture decodes through the real campaign loader");
    write_campaign_file(output, bytes);
}

void run()
{
    ui_fixture();
    auto rules = module();
    auto creation = srd5::character_rules();
    auto roundtrip = [&](const CampaignParty &party)
    {
        CampaignParty next(module());
        const auto bytes = saved(party);
        next.restore(decode_campaign(bytes, *creation, *rules, "grant-fixture", nullptr).party);
        check(saved(next) == bytes,
              "Pending/rest-edited training retains canonical campaign continuation");
        return next;
    };
    for (const auto klass : {"fighter", "barbarian", "rogue", "paladin", "ranger"})
        for (bool npc : {false, true})
        {
            CampaignParty party(module());
            auto h = mastery_grant_checks::chosen(klass);
            h.inventory().add("dagger", "Retained dagger");
            const auto id = npc ? party.recruit("mastery-rest:npc", h) : party.add_pc(h);
            check(!party.state().training_rest, "Creation does not invent a rest entitlement");
            rejects(
                [&]
                {
                    party.keep_rest_training({1, 1}, id);
                });
            const auto short_rest = party.rest(RestKind::short_rest);
            check(bool(short_rest) && !party.state().training_rest,
                  "Short Rest never permits replacement");
            party.finish_short_rest(*short_rest->spending);
            const auto start = party.begin_rest(RestKind::long_rest);
            check(bool(start), "Real Long Rest starts");
            party.interrupt_rest(*start, RestInterruption::damage);
            check(!party.state().training_rest, "Interrupted Long Rest does not grant replacement");
            party.resume_rest(party.state().rest_activity->ticket);
            (void)party.advance_rest(party.state().rest_activity->ticket,
                                     party.remaining_rest_milliseconds(), RestWork::sleep);
            check(party.state().training_rest &&
                      party.state().training_rest->members == std::vector<MemberId>{id},
                  "Only qualified completed rest grants one per-member replacement");
            party = roundtrip(party);
            const auto ticket = party.state().training_rest->ticket;
            const auto options = *rules->rest_training_options(party.member(id).character.sheet());
            auto selected = options.selected;
            for (const auto &o : options.group.options)
                if (std::find(selected.begin(), selected.end(), o.id) == selected.end())
                {
                    selected[0] = o.id;
                    break;
                }
            const auto before = saved(party);
            const auto member = party.member(id);
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
            rejects(
                [&]
                {
                    party.remove(id);
                });
            rejects(
                [&]
                {
                    party.replace_rest_training({ticket.session, ticket.revision + 1}, id,
                                                selected);
                });
            auto bad = selected;
            bad[1] = bad[0];
            rejects(
                [&]
                {
                    party.replace_rest_training(ticket, id, bad);
                });
            bad = selected;
            bad[0] = "wand";
            rejects(
                [&]
                {
                    party.replace_rest_training(ticket, id, bad);
                });
            check(saved(party) == before,
                  "Blocked exploration and rejected replacements are atomic");
            const auto preview = party.preview_rest_training(ticket, id, selected);
            check(saved(party) == before, "Replacement preview has no side effects");
            party.replace_rest_training(ticket, id, selected);
            const auto &after = party.member(id);
            check(!party.state().training_rest &&
                      after.character.sheet().grants == preview.character.sheet().grants,
                  "Apply consumes this member's entitlement and matches preview");
            check(after.vitals == member.vitals && after.equipped == member.equipped &&
                      after.character.inventory().items().size() ==
                          member.character.inventory().items().size() &&
                      after.character.advancements() == member.character.advancements(),
                  "Replacing training preserves health, resources, equipment and advancement");
            check(after.character.training_edits().size() == 1 &&
                      after.character.training_edits().front().rest_session == ticket.session,
                  "Replacement retains exact rest provenance");
            rejects(
                [&]
                {
                    party.replace_rest_training(ticket, id, selected);
                });
            party = roundtrip(party);
            check(saved(party).starts_with("OPENGOLD-CAMPAIGN 18\n"),
                  "Actual training history uses campaign18");
            party.advance_time(24 * 60);
            check(bool(party.rest(RestKind::long_rest)),
                  "Next qualified rest can offer a new choice");
            const auto keep_before = party.member(id).character.sheet().grants;
            party.keep_rest_training(party.state().training_rest->ticket, id);
            check(party.member(id).character.sheet().grants == keep_before &&
                      !party.state().training_rest,
                  "Keep retains exact grants and consumes the window");
            party = roundtrip(party);
            check(party.member(id).character.training_edits().size() == 2,
                  "Kept window cannot later be replayed as unused");
        }
    // Other training may remain pending when mastery itself is complete. Replay
    // replacement at its original level before applying the fourth-weapon choice.
    auto draft = hero("fighter").creation_data();
    draft.training["class:fighter:weapon_mastery"] = {"dagger", "longsword", "shortbow"};
    CampaignParty p(module());
    const auto id = p.add_pc(Character(*creation, draft, {}));
    check(bool(p.rest(RestKind::long_rest)) && p.state().training_rest,
          "Mastery replacement does not require unrelated training to be complete");
    p.replace_rest_training(p.state().training_rest->ticket, id,
                            std::vector<std::string>{"greatsword", "longsword", "shortbow"});
    auto choices = p.member(id).character.training_choices();
    CharacterDraft effective = draft;
    effective.training = choices;
    for (const auto &g : creation->training_options(effective))
    {
        auto &selected = choices[g.id];
        for (const auto &o : g.options)
            if (selected.size() < g.count &&
                std::find(selected.begin(), selected.end(), o.id) == selected.end())
                selected.push_back(o.id);
    }
    const auto history = p.member(id).character.training_edits();
    p.complete_training(id, *creation, choices);
    check(p.member(id).character.training_edits() == history &&
              p.member(id).character.training_choices().at("class:fighter:weapon_mastery") ==
                  std::vector<std::string>({"greatsword", "longsword", "shortbow"}),
          "Review Training retains effective masteries and their original replacement history");
    p.award_experience(2700, "rest-mastery-levels");
    for (unsigned level = 2; level <= 4; ++level)
        p.advance(id, p.default_advancement(id));
    p = roundtrip(p);
    check(p.member(id).character.sheet().training.masteries.size() == 4,
          "Rest edit replays before later Fighter entitlement");
    p.advance_time(24 * 60);
    check(bool(p.rest(RestKind::long_rest)), "Level4 qualified rest starts");
    const auto options = *rules->rest_training_options(p.member(id).character.sheet());
    check(options.group.count == 4 && options.replacement_limit == 1,
          "Fourth-kind capacity retains Fighter one-replacement limit");
    auto selected = options.selected;
    for (const auto &o : options.group.options)
        if (std::find(selected.begin(), selected.end(), o.id) == selected.end())
        {
            selected[0] = o.id;
            break;
        }
    p.replace_rest_training(p.state().training_rest->ticket, id, selected);
    p = roundtrip(p);
    check(p.member(id).character.training_edits().back().level == 4,
          "Replacement history records actual attained level");
    // Actual acquisition writer, captured before any replacement implementation.
    const auto old = fixture("campaign-v15-mastery-acquired-before.ogs");
    std::istringstream fields(old.substr(old.find('\n', old.find('\n') + 1) + 1));
    std::string module_id, version, content_id, assets;
    fields >> std::quoted(module_id) >> std::quoted(version) >> std::quoted(content_id) >>
        std::quoted(assets);
    check(version == "0.6.56", "Frozen acquisition fixture retains its real writer identity");
    CampaignParty old_party(module());
    old_party.restore(decode_campaign(old, *creation, *rules, assets, nullptr).party);
    auto expected = old.substr(old.find('\n', old.find('\n') + 1) + 1);
    expected.replace(expected.find("0.6.56"), 6, rules->identity().version);
    const auto rewritten = encode_campaign(old_party, nullptr, assets);
    check(
        rewritten.substr(0, rewritten.find('\n')) == old.substr(0, old.find('\n')) &&
            rewritten.substr(rewritten.find('\n', rewritten.find('\n') + 1) + 1) ==
                test::with_alert_grants(expected),
        "Actual acquired-masteries save adds fixed Alert and retains other grants, wounds, resources, inventories and history apart from module identity and checksum");
    check(!old_party.state().training_rest &&
              std::all_of(old_party.state().roster.begin(), old_party.state().roster.end(),
                          [](const auto &m)
                          {
                              return m.character.training_edits().empty();
                          }),
          "Loading an acquired-masteries save does not invent rest windows or history");
    // Missing old-save mastery, reserve members and dead actors get no window.
    CampaignParty excluded(module());
    excluded.add_pc(hero("fighter"));
    auto reserve = excluded.add_pc(mastery_grant_checks::chosen("rogue"));
    excluded.remove(reserve);
    const auto dead = excluded.add_pc(mastery_grant_checks::chosen("fighter"));
    auto state = excluded.checkpoint();
    for (auto &m : state.roster)
        if (m.id == dead)
            m.vitals = {0, true, "SRD1 0 0 0 3 0", "Dead"};
    excluded.restore(state);
    check(bool(excluded.rest(RestKind::long_rest)) && !excluded.state().training_rest,
          "Unqualified/dead/reserve/pending-training members do not gain replacement windows");
}
} // namespace mastery_rest_checks
