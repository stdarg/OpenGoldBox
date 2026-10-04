// Included by spell_access_tests.cpp; Paladin spellcasting through the public
// rules and Core paths.
CharacterDraft paladin_draft(std::vector<std::string> prepared)
{
    auto draft = hero("paladin").creation_data();
    draft.name = "Paladin tester";
    draft.spells = SpellChoices{{}, std::move(prepared), {}, {}};
    draft.training = {{"class:paladin", {"athletics", "insight"}}};
    return draft;
}

unsigned slot_capacity(const RulesModule &rules, const PartyMember &member)
{
    for (const auto &pool :
            rules.recovery_info(member.character.sheet(), member.vitals).resources)
        if (pool.id == "spell_slot:1")
            return pool.capacity;
    return 0;
}

void paladin_choices_checks()
{
    auto rules = module();
    auto creation_rules = srd5::character_rules();
    const auto draft = paladin_draft({"cure_wounds"});
    const Character paladin(*creation_rules, draft, {});
    const auto access = rules->spell_access(paladin.sheet());
    check(access.cantrip_choices == 0 && access.prepared_choices == 2 &&
          access.prepared == std::vector<std::string> {"cure_wounds"},
          "A level-one Paladin prepares two spells and knows no cantrips");
    const auto options = creation_rules->spell_choice_options(draft);
    check(options.may_prepare && options.prepared_count == 2 &&
          option_ids(options.preparation) ==
          std::vector<std::string> {"bless", "cure_wounds", "divine_favor", "divine_smite", "heroism",
                                    "searing_smite", "shield_of_faith"
                                   },
    "Creation prepares from the implemented Paladin list");
    check(rules->character_profile(paladin.sheet(), {}).data.starts_with("PC42 1 0 1 cure_wounds "),
          "The profile records the prepared spell");
    for (const auto &bad : std::vector<std::vector<std::string>>
{
    {"healing_word"}, {"magic_missile"}, {"cure_wounds", "cure_wounds"},
        {"cure_wounds", "divine_smite", "searing_smite"}, {"healing_word", "cure_wounds"}
    })
    rejects(
        [&]
    {
        (void)creation_rules->evaluate(paladin_draft(bad), true);
    });
    const auto messages = rules->character_profile(paladin.sheet(), {}).spell_messages;
    check(std::any_of(messages.begin(), messages.end(), [&](const auto & m)
    {
        return m.source.starts_with("Paladin spellcasting: Charisma score");
    }),
    "Paladin spellcasting uses Charisma");

    CharacterCreator creator(srd5::character_rules(), 1);
    creator.roll();
    for (unsigned n = 0; n < 6; ++n)
        creator.assign_roll(n, n);
    creator.select(CreationField::character_class, "paladin");
    check(creator.has_spell_choices(), "The Spell Choices step appears for a Paladin");

    // Slots 2/2/3/3 and prepared spells 2/3/4/5; earlier choices stay prepared.
    CampaignParty party(module());
    const auto id = party.add_pc(Character(*creation_rules, draft, {}));
    party.award_experience(2700, "paladin-xp");
    check(slot_capacity(*rules, party.member(id)) == 2, "Level one has two slots");
    const std::array<unsigned, 4> slots{2, 2, 3, 3};
    for (unsigned level = 2; level <= 4; ++level)
    {
        auto choice = party.default_advancement(id);
        check(choice.spell_learning.has_value() && choice.spells.front() == "cure_wounds",
              "Paladin level-up keeps its earlier preparation");
        party.advance(id, choice);
        const auto &sheet = party.member(id).character.sheet();
        const auto access = rules->spell_access(sheet);
        check(slot_capacity(*rules, party.member(id)) == slots[level - 1] &&
              access.prepared_choices == level + 1,
              "Paladin slots and prepared spells follow the class table");
        // Paladin's Smite: Divine Smite is always prepared and not counted.
        check(access.always_prepared == std::vector<std::string> {"divine_smite"} &&
              std::find(access.prepared.begin(), access.prepared.end(), "divine_smite") ==
              access.prepared.end() &&
              rules->character_profile(sheet, {}).data.find(" divine_smite ") != std::string::npos,
              "Divine Smite is always prepared from level two");
    }
    const auto bytes = saved(party);
    CampaignParty restored(module());
    restored.restore(decode_campaign(bytes, *creation_rules, *rules, "spell-access", nullptr).party);
    check(saved(restored) == bytes, "A level-four Paladin round-trips");

    // After a Long Rest a Paladin replaces one prepared spell, not more.
    CampaignParty resting(module());
    const auto rester =
        resting.add_pc(Character(*creation_rules, paladin_draft({"cure_wounds", "searing_smite"}), {}));
    (void)resting.rest(RestKind::long_rest);
    check(resting.state().spell_rest.has_value(), "The Paladin's Long Rest opens the spell window");
    const auto before_rest = saved(resting);
    SpellChoices two;
    two.prepared = std::vector<std::string> {"heroism", "divine_favor"};
    rejects(
        [&]
    {
        resting.choose_spells(rester, two);
    });
    check(saved(resting) == before_rest, "Replacing two prepared spells is refused");
    SpellChoices one;
    one.prepared = std::vector<std::string> {"cure_wounds", "heroism"};
    resting.choose_spells(rester, one);
    check(resting.member(rester).character.sheet().prepared_spells == *one.prepared,
          "Replacing one prepared spell is allowed");
}
