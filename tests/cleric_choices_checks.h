// Included by spell_access_tests.cpp; Cleric preparation, cantrips and Divine
// Order through the public rules and Core paths.
CharacterDraft cleric_draft(std::string order, std::vector<std::string> prepared)
{
    auto draft = hero("cleric").creation_data();
    draft.name = "Cleric tester";
    draft.cantrips = std::vector<std::string> {"sacred_flame"};
    draft.spells = SpellChoices{{}, std::move(prepared), {}, {}};
    draft.training = {{"class:cleric", {"medicine", "persuasion"}},
        {"class:cleric:divine_order", {std::move(order)}}
    };
    return draft;
}

const std::vector<std::string> level_one_cleric_spells{"cure_wounds", "healing_word",
    "inflict_wounds", "shield_of_faith"};

std::vector<std::string> option_ids(const std::vector<CreationChoice> &options)
{
    std::vector<std::string> result;
    for (const auto &o : options)
        result.push_back(o.id);
    std::sort(result.begin(), result.end());
    return result;
}

void cleric_creation_checks()
{
    auto rules = module();
    auto creation_rules = srd5::character_rules();
    const auto draft = cleric_draft("protector", level_one_cleric_spells);
    const Character cleric(*creation_rules, draft, {});
    const auto &sheet = cleric.sheet();
    check(sheet.training.complete, "Divine Order completes Cleric training");
    const auto access = rules->spell_access(sheet);
    check(access.cantrip_choices == 3 && access.prepared_choices == 4 &&
          access.prepared == level_one_cleric_spells,
          "A level-one Cleric knows three cantrips and prepares four spells");
    const auto options = creation_rules->spell_choice_options(draft);
    check(options.may_prepare && options.prepared_count == 4 &&
          option_ids(options.preparation) == std::vector<std::string>
    {"bane", "bless", "command", "cure_wounds", "guiding_bolt", "healing_word", "inflict_wounds",
        "protection_from_evil_and_good", "sanctuary", "shield_of_faith"},
    "Creation prepares from the Cleric list up to level-one slots");
    const auto profile = rules->character_profile(sheet, {}).data;
    check(profile.starts_with(
              "PC42 1 0 5 sacred_flame cure_wounds healing_word inflict_wounds shield_of_faith "),
          "The profile records the cantrip and the prepared spells");
    for (const auto &bad : std::vector<std::vector<std::string>>
{
    {"blindness"}, {"magic_missile"}, {"cure_wounds", "cure_wounds"},
        {"cure_wounds", "healing_word", "inflict_wounds", "blindness", "magic_missile"}
    })
    rejects(
        [&]
    {
        (void)creation_rules->evaluate(cleric_draft("protector", bad), true);
    });

    // Without preparation a Cleric no longer silently knows Cure Wounds.
    auto unprepared = draft;
    unprepared.spells.reset();
    const Character empty(*creation_rules, unprepared, {});
    check(rules->spell_access(empty.sheet()).prepared.empty() &&
          rules->character_profile(empty.sheet(), {}).data.starts_with("PC42 1 0 1 sacred_flame "),
          "An unprepared Cleric casts only its cantrip");

    auto thaumaturge = cleric_draft("thaumaturge", level_one_cleric_spells);
    check(creation_rules->cantrip_options(thaumaturge).count == 4 &&
          rules->spell_access(Character(*creation_rules, thaumaturge, {}).sheet())
          .cantrip_choices == 4,
          "Thaumaturge adds a fourth Cleric cantrip");
    auto unordered = draft;
    unordered.training.erase("class:cleric:divine_order");
    check(!Character(*creation_rules, unordered, {}).sheet().training.complete,
          "Divine Order is a required choice");
}

void divine_order_checks()
{
    auto rules = module();
    auto creation_rules = srd5::character_rules();
    const Character protector(*creation_rules, cleric_draft("protector", {}), {});
    const Character thaumaturge(*creation_rules, cleric_draft("thaumaturge", {}), {});
    const std::string trained = "Class training: no untrained-use penalty.";
    check(srd5::equipment_note(protector.sheet(), "chain_mail") == trained &&
          srd5::equipment_note(protector.sheet(), "longsword") == trained,
          "Protector trains Heavy armor and Martial weapons");
    check(srd5::equipment_note(thaumaturge.sheet(), "chain_mail") != trained &&
          srd5::equipment_note(thaumaturge.sheet(), "longsword") != trained &&
          srd5::equipment_note(thaumaturge.sheet(), "scale_mail") == trained,
          "Other Clerics keep their Medium armor and Simple weapon training");
    const auto wisdom = std::max(1, thaumaturge.sheet().modifiers[4]);
    for (const auto *skill :
            {"arcana", "religion"
            })
        check(rules->ability_check(thaumaturge.sheet(), {}, 3, skill).total ==
              rules->ability_check(protector.sheet(), {}, 3, skill).total + wisdom,
              "Thaumaturge adds Wisdom to Arcana and Religion checks");
    check(rules->ability_check(thaumaturge.sheet(), {}, 3, "history").total ==
          rules->ability_check(protector.sheet(), {}, 3, "history").total,
          "Thaumaturge adds nothing to other Intelligence checks");
}

void cleric_advancement_checks()
{
    auto rules = module();
    auto creation_rules = srd5::character_rules();
    CampaignParty party(module());
    const auto id = party.add_pc(
                        Character(*creation_rules, cleric_draft("protector", {"cure_wounds"}), {}));
    party.award_experience(2700, "cleric-choice-xp");

    auto second = party.default_advancement(id);
    auto defaults = second.spells;
    std::sort(defaults.begin(), defaults.end());
    check(second.spell_learning.has_value() &&
          defaults == std::vector<std::string> {"bless", "cure_wounds", "healing_word",
                                                "inflict_wounds", "shield_of_faith"
                                               },
          "A Cleric's default fills the new preparation from the Cleric list");
    auto short_list = second;
    short_list.spells = {"cure_wounds", "healing_word"};
    const auto before = saved(party);
    rejects(
        [&]
    {
        party.advance(id, short_list);
    });
    check(saved(party) == before, "An incomplete preparation rejects atomically");
    auto dropped = second;
    dropped.spells = {"healing_word", "inflict_wounds"};
    rejects(
        [&]
    {
        party.advance(id, dropped);
    });
    check(saved(party) == before, "Level-up keeps earlier preparations");
    party.advance(id, second);
    auto third = party.default_advancement(id);
    check(std::find(third.spells.begin(), third.spells.end(), "blindness") != third.spells.end(),
          "Level three prepares a level-two spell");
    party.advance(id, third);
    party.advance(id, party.default_advancement(id));
    const auto sheet = party.member(id).character.sheet();
    const auto access = rules->spell_access(sheet);
    // Aid, Bless, Cure Wounds and Lesser Restoration are Life Domain spells from
    // level three; seven others fill the places. Of four cantrips only Sacred
    // Flame and Spare the Dying exist, so two stay pending.
    check(sheet.level == 4 && access.prepared_choices == 7 && access.prepared.size() == 7 &&
          access.cantrip_choices == 4 && access.cantrips.size() == 2,
          "Level four keeps unfilled Cleric choices pending");
    const auto pending = rules->spell_choice_options(sheet, SpellChoiceContext::advancement);
    check(!pending.may_replace && pending.replaceable.empty(),
          "Cantrip replacement waits for a second implemented Cleric cantrip");

    // A Long Rest reopens the whole preparation; cantrips are not replaced then.
    (void)party.rest(RestKind::long_rest);
    check(party.state().spell_rest &&
          party.state().spell_rest->members == std::vector<MemberId> {id},
          "A completed Long Rest lets the Cleric change prepared spells");
    const auto options = party.spell_choice_options(id);
    check(options.may_prepare && !options.may_replace && options.locked_prepared.empty(),
          "Rest preparation is unlocked and offers no cantrip replacement");
    const auto after_rest = saved(party);
    SpellChoices replace;
    replace.prepared = sheet.prepared_spells;
    replace.replace_cantrip = "sacred_flame";
    replace.replacement = "fire_bolt";
    SpellChoices too_few;
    too_few.prepared = std::vector<std::string> {"cure_wounds"};
    for (const auto &bad :
            {
                replace, too_few
            })
        rejects(
            [&]
        {
            party.choose_spells(id, bad);
        });
    check(saved(party) == after_rest, "Rejected rest choices change nothing");
    auto round = decode_campaign(after_rest, *creation_rules, *rules, "spell-access", nullptr);
    CampaignParty restored(module());
    restored.restore(round.party);
    check(saved(restored) == after_rest, "The Cleric's rest window roundtrips");
    SpellChoices reordered;
    const auto &kept = party.member(id).character.sheet().prepared_spells;
    reordered.prepared = std::vector<std::string>(kept.rbegin(), kept.rend());
    party.choose_spells(id, reordered);
    restored.choose_spells(id, reordered);
    check(saved(restored) == saved(party) && !party.state().spell_rest,
          "The rest choice commits identically after reload and closes the window");
}

void cleric_choices_checks()
{
    cleric_creation_checks();
    divine_order_checks();
    cleric_advancement_checks();
}

// Writes the campaign tests/cleric_preparation_view_tests.gd loads: one fresh
// level-one Cleric with enough experience to reach level three through the
// real level-up controls. Requires the original assets for their identity.
void write_cleric_ui_fixture()
{
    const auto *directory = std::getenv("OPENGOLD_GAME_DIR");
    if (!directory || !*directory)
        return;
    CampaignParty party(module());
    (void)party.add_pc(Character(*srd5::character_rules(),
                                 cleric_draft("protector", level_one_cleric_spells), {}));
    party.award_experience(2700, "cleric-ui");
    write_campaign_file(std::filesystem::path(OPENGOLD_BINARY_DIR) / "cleric-choices-ui.ogs",
                        encode_campaign(party, nullptr, campaign_asset_identity(directory)));
}
