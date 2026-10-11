// Included by paladin_spell_tests.cpp; Command through the public combat session.

// The commands a session offers, as verbs.
std::vector<std::string> offered_verbs(const CombatSession &c)
{
    std::vector<std::string> verbs;
    for (const auto &command : c.legal_commands())
        verbs.push_back(command.verb);
    return verbs;
}

// Chebyshev distance in cells between the Paladin (1) and the enemy (99).
int gap_cells(const CombatSession &c)
{
    const auto paladin_cell = unit(c, 1).cell, enemy_cell = unit(c, 99).cell;
    return std::max(std::abs(paladin_cell.x - enemy_cell.x), std::abs(paladin_cell.y - enemy_cell.y));
}

// The Paladin (1) and an enemy (99) `gap` cells away, once the Paladin's
// Command has landed: the first seed whose Wisdom save fails.
std::unique_ptr<CombatSession> commanded(const RulesModule &module, const std::string &verb, int gap)
{
    const auto profile = module.character_profile(paladin({"command"}).sheet(),
                         std::vector<std::string> {"longsword"}).data;
    for (std::uint64_t seed = 1; seed < 64; ++seed)
    {
        auto c = module.create({{12, 4, std::vector<Terrain>(48)},
            {   {1, "campaign-character", "Paladin", Side::party, {1, 1}, profile},
                {99, "target", "Enemy", Side::opposition, {1 + gap, 1}}
            }},
        seed);
        for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 4; ++turns)
            check(submit(*c, "end"), "Reach the Paladin's turn");
        check(submit(*c, verb, 99), "The Paladin casts Command");
        if (logged(*c, "must obey Paladin's Command"))
            return c;
    }
    throw std::runtime_error("No seed fails the Wisdom save");
}

// Ends the Paladin's turn so the commanded enemy acts.
void reach_enemy(CombatSession &c)
{
    check(submit(c, "end") && c.snapshot().actor == 99, "The commanded enemy's turn begins");
}

void command_offer_checks()
{
    auto module = rules();
    auto c = battle(*module, paladin({"command"}));
    std::vector<std::string> labels;
    for (const auto &command : c->legal_commands())
        if (command.verb.starts_with("command") && command.target == 99)
            labels.push_back(command.label);
    check(labels == std::vector<std::string> {"Command: Approach", "Command: Flee",
                                              "Command: Grovel", "Command: Halt"
                                             },
          "Command offers four options and no Drop");
}

void command_grovel_halt_checks()
{
    auto module = rules();
    auto grovel = commanded(*module, "command_grovel", 1);
    check(!unit(*grovel, 1).action, "Command takes the Action");
    const auto saved = grovel->save();
    check(module->restore(saved)->save() == saved, "A Command survives a checkpoint");
    check(submit(*grovel, "end") && grovel->snapshot().actor == 1 && unit(*grovel, 99).prone &&
          logged(*grovel, "Enemy grovels."),
          "Grovel: the target falls Prone and its turn ends");
    check(!logged(*grovel, "Paladin takes"), "A groveling enemy does not attack");

    auto halt = commanded(*module, "command_halt", 1);
    check(submit(*halt, "end") && halt->snapshot().actor == 1 && !unit(*halt, 99).prone &&
          logged(*halt, "Enemy halts."),
          "Halt: the target neither moves nor acts");
    check(submit(*halt, "end") && halt->snapshot().actor == 99 &&
          offered_verbs(*halt).size() > 1,
          "The Command lasts one turn");
}

void command_approach_checks()
{
    auto module = rules();
    auto c = commanded(*module, "command_approach", 5);
    reach_enemy(*c);
    check(offered_verbs(*c) == std::vector<std::string> {"move"},
          "Approach: the target may only move toward the caster");
    check(submit(*c, "move") && gap_cells(*c) == 1,
          "Approach takes the shortest route to the caster");
    check(offered_verbs(*c) == std::vector<std::string> {"end"},
          "Approach ends the turn within 5 feet of the caster");
}

void command_flee_checks()
{
    auto module = rules();
    auto c = commanded(*module, "command_flee", 2);
    reach_enemy(*c);
    std::vector<std::string> steps;
    for (unsigned n = 0; n < 6 && offered_verbs(*c) != std::vector<std::string> {"end"}; ++n)
    {
        const auto verbs = offered_verbs(*c);
        check(verbs.size() == 1, "Flee leaves one way to obey");
        steps.push_back(verbs.front());
        check(submit(*c, verbs.front()), "The enemy flees");
    }
    check(steps == std::vector<std::string> {"move", "dash", "move"} && gap_cells(*c) == 10,
          "Flee moves away, Dashes and moves on to the far edge, then ends the turn");
}

// A level-three Cleric with Command prepared; level three brings level-two slots.
std::string cleric_profile(const RulesModule &module)
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "male";
    d.character_class = "cleric";
    d.background = "acolyte";
    d.alignment = "lawful_good";
    d.name = "Cleric";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    d.cantrips = std::vector<std::string> {"sacred_flame"};
    d.spells = SpellChoices{{}, std::vector<std::string>{"command"}, {}, {}};
    d.training = {{"class:cleric", {"medicine", "persuasion"}},
        {"class:cleric:divine_order", {"protector"}}
    };
    CampaignParty party(srd5::load(root / "data/rules/srd-5.2.1/combat.rules"));
    const auto id = party.add_pc(Character(*srd5::character_rules(), d, {}));
    party.award_experience(900, "command-upcast-xp");
    party.advance(id, party.default_advancement(id));
    party.advance(id, party.default_advancement(id));
    return module.character_profile(party.member(id).character.sheet(),
                                    std::vector<std::string> {"mace"}).data;
}

void command_upcast_checks()
{
    auto module = rules();
    auto c = module->create({{8, 4, std::vector<Terrain>(32)},
        {   {1, "campaign-character", "Cleric", Side::party, {1, 1}, cleric_profile(*module)},
            {98, "target", "First", Side::opposition, {3, 1}},
            {99, "target", "Second", Side::opposition, {3, 2}}
        }},
    5);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 6; ++turns)
        check(submit(*c, "end"), "Reach the Cleric's turn");
    check(submit(*c, "command_halt_2", 98) && c->snapshot().spell_targeting &&
          unit(*c, 1).action,
          "A level-two Command starts choosing its creatures");
    const auto choosing = c->save();
    check(module->restore(choosing)->save() == choosing, "The open choice survives a checkpoint");
    check(submit(*c, "command_halt_2", 99) && !c->snapshot().spell_targeting &&
          !unit(*c, 1).action,
          "The second creature casts Command");
    unsigned saves = 0;
    for (const auto &line : c->snapshot().log())
        saves += line.find("Wisdom save") != std::string::npos;
    check(saves == 2, "Each chosen creature makes its own Wisdom save");
}

// A Paladin with Command prepared and a vanguard 25 feet away, at a seed where
// the vanguard fails its save, for tests/command_view_tests.gd. The game loads
// it with the standard rules content.
void write_command_fixture()
{
    auto module = srd5::load(root / "data/rules/srd-5.2.1/combat.rules");
    const auto profile = module->character_profile(paladin({"command", "cure_wounds"}).sheet(),
                         std::vector<std::string> {"longsword"}).data;
    for (std::uint64_t seed = 1; seed < 64; ++seed)
    {
        auto c = module->create({{12, 9, std::vector<Terrain>(108)},
            {   {1, "campaign-character", "Paladin", Side::party, {1, 1}, profile},
                {99, "vanguard", "Enemy", Side::opposition, {6, 1}}
            }},
        seed);
        for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 4; ++turns)
            check(submit(*c, "end"), "Reach the Paladin's turn");
        const auto ready = c->save();
        if (!submit(*c, "command_grovel", 99) || !logged(*c, "must obey"))
            continue;
        const auto path = std::filesystem::path(OPENGOLD_BINARY_DIR) / "bless-fixtures";
        std::filesystem::create_directories(path);
        std::ofstream out(path / "command.save", std::ios::binary);
        out << ready;
        check(bool(out), "Write the Command UI fixture");
        return;
    }
    throw std::runtime_error("No seed fails the vanguard's save");
}

void command_checks()
{
    command_offer_checks();
    command_grovel_halt_checks();
    command_approach_checks();
    command_flee_checks();
    command_upcast_checks();
    write_command_fixture();
}
