// Included by paladin_spell_tests.cpp; the level-three Paladin's Oath of
// Devotion and Sacred Weapon through the public combat session.

// A level-three Paladin, advanced through the campaign so the oath is granted.
Character level_three_paladin()
{
    CampaignParty party(srd5::load(root / "data/rules/srd-5.2.1/combat.rules"));
    const auto id = party.add_pc(paladin({"cure_wounds", "bless"}));
    party.award_experience(900, "devotion-xp");
    party.advance(id, party.default_advancement(id));
    party.advance(id, party.default_advancement(id));
    return party.member(id).character;
}

int charisma_bonus(const Character &hero)
{
    return std::max(1, hero.sheet().modifiers[Ability::charisma]);
}

void sacred_weapon_checks()
{
    auto module = rules();
    const auto hero = level_three_paladin();
    auto plain = battle(*module, hero);
    check(submit(*plain, "melee", 99), "An attack without Sacred Weapon");
    const int base = logged_bonus(*plain);

    auto c = battle(*module, hero);
    check(submit(*c, "sacred_weapon", 1) && logged(*c, "Paladin uses Sacred Weapon.") &&
          unit(*c, 1).action,
          "Sacred Weapon spends a Channel Divinity use, not the Action");
    check(!submit(*c, "sacred_weapon", 1), "Sacred Weapon is not offered while it lasts");
    const auto saved = c->save();
    check(module->restore(saved)->save() == saved, "Sacred Weapon survives a checkpoint");
    check(submit(*c, "melee", 99) && logged_bonus(*c) == base + charisma_bonus(hero),
          "Sacred Weapon adds the Charisma modifier to melee attack rolls");

    auto low = battle(*module, paladin({"cure_wounds"}));
    check(!submit(*low, "sacred_weapon", 1), "A level-one Paladin has no Sacred Weapon");
}

void sacred_radiant_checks()
{
    auto module = rules();
    const auto hero = level_three_paladin();
    auto plain = battle(*module, hero, "fiend");
    check(submit(*plain, "melee", 99) && logged(*plain, "Slashing damage"),
          "The Fiend resists the longsword's Slashing damage");
    auto sacred = battle(*module, hero, "fiend");
    check(submit(*sacred, "sacred_weapon", 1) && submit(*sacred, "melee", 99) &&
          !logged(*sacred, "Slashing damage") && unit(*sacred, 99).hit_points < 1000,
          "Sacred Weapon deals Radiant damage to a target that resists the weapon");
}

// A level-three Paladin beside a vanguard on its turn, for
// tests/sacred_weapon_view_tests.gd. The game loads it with the standard rules content.
void write_sacred_weapon_fixture()
{
    auto module = srd5::load(root / "data/rules/srd-5.2.1/combat.rules");
    const auto profile = module->character_profile(level_three_paladin().sheet(),
                         std::vector<std::string> {"longsword"}).data;
    auto c = module->create({{12, 9, std::vector<Terrain>(108)},
        {   {1, "campaign-character", "Paladin", Side::party, {1, 1}, profile},
            {99, "vanguard", "Enemy", Side::opposition, {6, 1}}
        }},
    2);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 4; ++turns)
        check(submit(*c, "end"), "Reach the Paladin's turn");
    const auto path = std::filesystem::path(OPENGOLD_BINARY_DIR) / "bless-fixtures";
    std::filesystem::create_directories(path);
    std::ofstream out(path / "sacred.save", std::ios::binary);
    out << c->save();
    check(bool(out), "Write the Sacred Weapon UI fixture");
}

void devotion_checks()
{
    sacred_weapon_checks();
    sacred_radiant_checks();
    write_sacred_weapon_fixture();
}
