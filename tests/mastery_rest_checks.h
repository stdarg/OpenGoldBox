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

// The original inn script reads the character back after its Long Rest; a
// pending mastery replacement must not fault that read.
void script_reads_after_rest(CampaignParty &party, MemberId id)
{
    std::vector<std::uint8_t> record{0, 0};
    for (int entry = 0; entry < 5; ++entry)
        record.insert(record.end(), {1, 1, 0x14, 0x99});
    record.push_back(0); // An EXIT body; the machine only supplies variables here.
    por::EclMachine vm(std::make_shared<const por::EclProgram>(
                           por::EclProgram::decode(record, "inn-read")));
    vm.bind_variable(0x6C19, static_cast<std::uint16_t>(party.member(id).vitals.hit_points));
    for (std::uint16_t coin = 0x6BBB; coin <= 0x6BC7; coin += 2)
        vm.bind_variable(coin, coin == 0x6BC1 ? 7 : 0); // Seven gold coins.
    (void)party.read_character(0, vm);
    check(party.member(id).wealth[3] == 7 && party.state().training_rest,
          "Script read after a Long Rest keeps the pending mastery choice");
}

void run()
{
    ui_fixture();
    auto rules = module();
    auto creation = srd5::character_rules();
    auto roundtrip = [&](const CampaignParty & party)
    {
        CampaignParty next(module());
        const auto bytes = saved(party);
        next.restore(decode_campaign(bytes, *creation, *rules, "grant-fixture", nullptr).party);
        check(saved(next) == bytes,
              "Pending/rest-edited training retains canonical campaign continuation");
        return next;
    };
    for (const auto klass :
            {"fighter", "barbarian", "rogue", "paladin", "ranger"
            })
        for (bool npc :
                {
                    false, true
                })
        {
            CampaignParty party(module());
            auto h = mastery_grant_checks::chosen(klass);
            h.add_item({.definition_id = "dagger", .name = "Retained dagger"});
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
            check(bool(party.rest(RestKind::long_rest)), "Real Long Rest completes");
            // A Paladin's prepared-spell choice comes first; keeping it leaves the mastery choice.
            if (party.state().spell_rest)
                party.keep_rest_spells(id);
            check(party.state().training_rest &&
                  party.state().training_rest->members == std::vector<MemberId> {id},
                  "Only qualified completed rest grants one per-member replacement");
            if (!npc)
                script_reads_after_rest(party, id);
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
                party.advance_time(std::chrono::minutes(1));
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
            check(saved(party).starts_with("OPENGOLD-CAMPAIGN 25\n"),
                  "Actual training history uses the current campaign format");
            party.advance_time(std::chrono::minutes(24 * 60));
            check(bool(party.rest(RestKind::long_rest)),
                  "Next qualified rest can offer a new choice");
            if (party.state().spell_rest)
                party.keep_rest_spells(id);
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
                            std::vector<std::string> {"greatsword", "longsword", "shortbow"});
    p.award_experience(2700, "rest-mastery-levels");
    for (unsigned level = 2; level <= 4; ++level)
        p.advance(id, p.default_advancement(id));
    p = roundtrip(p);
    check(p.member(id).character.sheet().training.masteries.size() == 4,
          "Rest edit replays before later Fighter entitlement");
    p.advance_time(std::chrono::minutes(24 * 60));
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
    // Missing mastery, reserve members and dead actors get no window.
    CampaignParty excluded(module());
    excluded.add_pc(hero("fighter"));
    auto reserve = excluded.add_pc(mastery_grant_checks::chosen("rogue"));
    excluded.remove(reserve);
    const auto dead = excluded.add_pc(mastery_grant_checks::chosen("fighter"));
    auto state = excluded.checkpoint();
    for (auto &m : state.roster)
        if (m.id == dead)
            m.vitals = {0, true, "SRD11 0 0 0 0 3 0 1 0 0 0 \"\" 0 0 0 0 0 0 FX8 1 0 0", "Dead"};
    excluded.restore(state);
    check(bool(excluded.rest(RestKind::long_rest)) && !excluded.state().training_rest,
          "Unqualified/dead/reserve/pending-training members do not gain replacement windows");
}
} // namespace mastery_rest_checks
