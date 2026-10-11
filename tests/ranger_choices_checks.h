// Included by spell_access_tests.cpp; Ranger spellcasting and Favored Enemy
// through the public rules and Core paths.
CharacterDraft ranger_draft(std::vector<std::string> prepared)
{
    auto draft = hero("ranger").creation_data();
    draft.name = "Ranger tester";
    draft.spells = SpellChoices{{}, std::move(prepared), {}, {}};
    draft.training = {{"class:ranger", {"athletics", "nature", "perception"}}};
    return draft;
}

void druidic_warrior_checks();

void ranger_choices_checks()
{
    druidic_warrior_checks();
    auto rules = module();
    auto creation_rules = srd5::character_rules();
    const auto draft = ranger_draft({"cure_wounds", "longstrider"});
    const Character ranger(*creation_rules, draft, {});
    const auto access = rules->spell_access(ranger.sheet());
    check(access.cantrip_choices == 0 && access.prepared_choices == 2 &&
          access.always_prepared == std::vector<std::string> {"hunters_mark"},
          "A level-one Ranger prepares two spells and always has Hunter's Mark");
    const auto options = creation_rules->spell_choice_options(draft);
    check(options.may_prepare && options.prepared_count == 2 &&
          option_ids(options.preparation) ==
          std::vector<std::string> {"cure_wounds", "ensnaring_strike", "entangle", "fog_cloud", "goodberry",
                                    "longstrider"
                                   },
          "Creation prepares from the implemented Ranger list, without Hunter's Mark");
    rejects(
        [&]
    {
        (void)creation_rules->evaluate(ranger_draft({"hunters_mark"}), NameRequirement::required);
    });
    rejects(
        [&]
    {
        (void)creation_rules->evaluate(ranger_draft({"bless"}), NameRequirement::required);
    });
    const auto messages = rules->character_profile(ranger.sheet(), {}).spell_messages;
    check(std::any_of(messages.begin(), messages.end(), [](const auto & m)
    {
        return m.source.starts_with("Ranger spellcasting: Wisdom score");
    }),
    "Ranger spellcasting uses Wisdom");
    CharacterCreator creator(srd5::character_rules(), 1);
    creator.roll();
    for (unsigned n = 0; n < 6; ++n)
        creator.assign_roll(n, static_cast<rules::Ability>(n));
    creator.select(CreationField::character_class, "ranger");
    check(creator.has_spell_choices(), "The Spell Choices step appears for a Ranger");

    // Slots 2/2/3/3 and prepared spells 2/3/4/4; Favored Enemy's two free casts.
    CampaignParty party(module());
    const auto id = party.add_pc(Character(*creation_rules, ranger_draft({"cure_wounds"}), {}));
    party.award_experience(2700, "ranger-xp");
    const std::array<unsigned, 4> slots{2, 2, 3, 3}, prepared{2, 3, 4, 4};
    for (unsigned level = 2; level <= 4; ++level)
    {
        party.advance(id, party.default_advancement(id));
        const auto &sheet = party.member(id).character.sheet();
        check(slot_capacity(*rules, party.member(id)) == slots[level - 1] &&
              rules->spell_access(sheet).prepared_choices == prepared[level - 1],
              "Ranger slots and prepared spells follow the class table");
    }
    const auto recovery = party.recovery_info(id);
    check(std::any_of(recovery.resources.begin(), recovery.resources.end(), [](const auto & r)
    {
        return r.id == "favored_enemy" && r.capacity == 2 && !r.short_rest_recovery;
    }),
    "Favored Enemy has two free casts, restored by a Long Rest");
    const auto bytes = saved(party);
    CampaignParty restored(module());
    restored.restore(decode_campaign(bytes, *creation_rules, *rules, "spell-access", nullptr).party);
    check(saved(restored) == bytes, "A level-four Ranger round-trips");

    // After a Long Rest a Ranger replaces one prepared spell, not more.
    CampaignParty resting(module());
    const auto rester =
        resting.add_pc(Character(*creation_rules, ranger_draft({"cure_wounds", "longstrider"}), {}));
    (void)resting.rest(RestKind::long_rest);
    check(resting.state().spell_rest.has_value(), "The Ranger's Long Rest opens the spell window");
    SpellChoices same;
    same.prepared = std::vector<std::string> {"longstrider", "cure_wounds"};
    resting.choose_spells(rester, same);
    check(!resting.state().spell_rest, "Keeping the preparation closes the window");
}

// Druidic Warrior, the level-two alternative to a Fighting Style feat.
void druidic_warrior_checks()
{
    auto rules = module();
    auto creation_rules = srd5::character_rules();
    CampaignParty party(module());
    const auto id = party.add_pc(Character(*creation_rules, ranger_draft({"cure_wounds"}), {}));
    party.award_experience(300, "druidic-warrior-xp");
    const auto &sheet = party.member(id).character.sheet();
    auto choice = party.default_advancement(id);
    choice.fighting_style = "druidic_warrior";
    const auto options = rules->spell_choice_options(rules->spell_choice_sheet(sheet, choice),
                         SpellChoiceContext::advancement);
    check(options.learning.size() == 1 && options.learning.front().count == 2 &&
          option_ids(options.learning.front().options) ==
          std::vector<std::string> {"poison_spray", "produce_flame", "resistance", "shillelagh",
                                    "spare_the_dying", "starry_wisp"
                                   },
          "Druidic Warrior learns two Druid cantrips at level two");
    auto cleric_cantrip = choice;
    (*cleric_cantrip.spell_learning)["cantrips:2"] = {"sacred_flame"};
    const auto before = saved(party);
    rejects(
        [&]
    {
        party.advance(id, cleric_cantrip);
    });
    check(saved(party) == before, "Druidic Warrior learns only Druid cantrips");
    (*choice.spell_learning)["cantrips:2"] = {"poison_spray", "spare_the_dying"};
    party.advance(id, choice);
    const auto &warrior = party.member(id).character.sheet();
    const auto access = rules->spell_access(warrior);
    check(access.cantrip_choices == 2 && access.cantrips.size() == 2 &&
          access.cantrips.front().id == "poison_spray" &&
          rules->character_profile(warrior, {}).data.find(" poison_spray ") != std::string::npos,
          "The Ranger knows Poison Spray and Spare the Dying");
    const auto bytes = saved(party);
    CampaignParty restored(module());
    restored.restore(decode_campaign(bytes, *creation_rules, *rules, "spell-access", nullptr).party);
    check(saved(restored) == bytes, "A Druidic Warrior round-trips");
}
