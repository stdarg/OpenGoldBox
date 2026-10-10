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

// Blessed Warrior, the level-two alternative to a Fighting Style feat.
void blessed_warrior_checks()
{
    auto rules = module();
    auto creation_rules = srd5::character_rules();
    CampaignParty party(module());
    const auto id = party.add_pc(Character(*creation_rules, paladin_draft({"cure_wounds"}), {}));
    party.award_experience(300, "blessed-warrior-xp");
    const auto &sheet = party.member(id).character.sheet();
    const auto styles = rules->advancement_options(sheet).fighting_styles;
    check(std::any_of(styles.begin(), styles.end(), [](const auto & s)
    {
        return s.id == "blessed_warrior" && s.available;
    }),
    "A level-one Paladin may choose Blessed Warrior");

    auto choice = party.default_advancement(id);
    choice.fighting_style = "blessed_warrior";
    const auto next = rules->spell_choice_sheet(sheet, choice);
    const auto options = rules->spell_choice_options(next, SpellChoiceContext::advancement);
    check(options.learning.size() == 1 && options.learning.front().id == "cantrips:2" &&
          options.learning.front().count == 2 &&
          option_ids(options.learning.front().options) ==
          std::vector<std::string> {"resistance", "sacred_flame", "spare_the_dying"},
          "Blessed Warrior learns two Cleric cantrips at level two");
    auto defense = choice;
    defense.fighting_style = "defense";
    check(rules->spell_choice_options(rules->spell_choice_sheet(sheet, defense),
                                      SpellChoiceContext::advancement).learning.empty(),
          "A Fighting Style feat brings no cantrips");

    auto wizard_cantrip = choice;
    (*wizard_cantrip.spell_learning)["cantrips:2"] = {"fire_bolt"};
    const auto before = saved(party);
    rejects(
        [&]
    {
        party.advance(id, wizard_cantrip);
    });
    check(saved(party) == before, "Blessed Warrior learns only Cleric cantrips");

    (*choice.spell_learning)["cantrips:2"] = {"sacred_flame", "spare_the_dying"};
    party.advance(id, choice);
    const auto &warrior = party.member(id).character.sheet();
    const auto access = rules->spell_access(warrior);
    check(access.cantrip_choices == 2 && access.cantrips.size() == 2 &&
          access.cantrips.front().id == "sacred_flame",
          "The Paladin knows Sacred Flame and Spare the Dying");
    const auto profile = rules->character_profile(warrior, {}).data;
    auto fight = rules->create({{8, 4, std::vector<Terrain>(32)},
        {   {1, "campaign-character", "Paladin", Side::party, {1, 1}, profile},
            {99, "bandit", "Enemy", Side::opposition, {4, 1}}
        }},
    5);
    bool offered = false;
    for (unsigned turns = 0; turns < 4 && !offered; ++turns)
    {
        const auto commands = fight->legal_commands();
        offered = std::any_of(commands.begin(), commands.end(), [](const auto & c)
        {
            return c.actor == 1 && c.verb == "sacred_flame" && c.target == 99;
        });
        if (!offered)
            fight->submit(commands.front());
    }
    check(offered, "The Blessed Warrior casts Sacred Flame in combat");
    const auto bytes = saved(party);
    CampaignParty restored(module());
    restored.restore(decode_campaign(bytes, *creation_rules, *rules, "spell-access", nullptr).party);
    check(saved(restored) == bytes, "A Blessed Warrior round-trips");
}

const ResourcePool *pool(const RecoveryInfo &info, std::string_view id)
{
    for (const auto &resource : info.resources)
        if (resource.id == id)
            return &resource;
    return nullptr;
}

// Level three: Channel Divinity and the Oath of Devotion's always prepared spells.
void devotion_checks()
{
    auto rules = module();
    auto creation_rules = srd5::character_rules();
    CampaignParty party(module());
    const auto id = party.add_pc(
                        Character(*creation_rules, paladin_draft({"cure_wounds", "shield_of_faith"}), {}));
    party.award_experience(900, "devotion-xp");
    party.advance(id, party.default_advancement(id));
    check(!pool(rules->recovery_info(party.member(id).character.sheet(), party.member(id).vitals),
                "channel_divinity"),
          "Channel Divinity waits for level three");
    auto third = party.default_advancement(id);
    check(std::find(third.spells.begin(), third.spells.end(), "shield_of_faith") == third.spells.end(),
          "Shield of Faith, prepared earlier, frees its place at level three");
    auto doubled = third;
    doubled.spells.back() = "shield_of_faith";
    const auto before = saved(party);
    rejects(
        [&]
    {
        party.advance(id, doubled);
    });
    check(saved(party) == before, "An always prepared spell cannot also be prepared");
    party.advance(id, third);
    const auto &sheet = party.member(id).character.sheet();
    const auto access = rules->spell_access(sheet);
    check(access.prepared.size() == 4 &&
          std::find(access.prepared.begin(), access.prepared.end(), "shield_of_faith") ==
          access.prepared.end() &&
          std::any_of(sheet.grants.begin(), sheet.grants.end(), [](const auto & g)
    {
        return g.id == "subclass:devotion";
    }),
    "Level three takes the Oath of Devotion and prepares four other spells");
    const auto recovery = rules->recovery_info(sheet, party.member(id).vitals);
    const auto *channel = pool(recovery, "channel_divinity");
    check(channel && channel->capacity == 2 && channel->remaining == 2 &&
          channel->short_rest_recovery == 1,
          "Channel Divinity has two uses and regains one on a Short Rest");
}

void paladin_choices_checks()
{
    blessed_warrior_checks();
    devotion_checks();
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
          std::vector<std::string> {"bless", "command", "cure_wounds", "divine_favor", "divine_smite", "heroism",
                                    "protection_from_evil_and_good", "searing_smite", "shield_of_faith"
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
        (void)creation_rules->evaluate(paladin_draft(bad), NameRequirement::required);
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
        creator.assign_roll(n, static_cast<rules::Ability>(n));
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
        // Paladin's Smite: Divine Smite is always prepared and not counted; the
        // Oath of Devotion adds its spells at level three.
        const auto always = level >= 3 ? std::vector<std::string> {"divine_smite",
                            "protection_from_evil_and_good", "shield_of_faith"
                                                                  }
                            : std::vector<std::string> {"divine_smite"};
        check(access.always_prepared == always &&
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

// Writes the campaign tests/blessed_warrior_view_tests.gd loads: a level-one
// Paladin with enough experience for level two. Requires the original assets
// for their identity.
void write_blessed_warrior_ui_fixture()
{
    const auto *directory = std::getenv("OPENGOLD_GAME_DIR");
    if (!directory || !*directory)
        return;
    CampaignParty party(module());
    (void)party.add_pc(Character(*srd5::character_rules(), paladin_draft({"cure_wounds"}), {}));
    party.award_experience(300, "blessed-warrior-ui");
    write_campaign_file(std::filesystem::path(OPENGOLD_BINARY_DIR) / "blessed-warrior-ui.ogs",
                        encode_campaign(party, nullptr, campaign_asset_identity(directory)));
}
