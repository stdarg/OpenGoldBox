// Weapon Mastery combat acceptance, using public commands and retained effects.
namespace mastery_combat_checks
{
fx::EffectState state(const CombatSession &c, EntityId id)
{
    const auto resources = unit(c, id).persistent.resources;
    const auto at = resources.find("FX");
    if (at == resources.npos)
        return {};
    std::istringstream in(resources.substr(at));
    return fx::read_effects(in);
}

void inject(Participant &p, const VitalState &vitals, const fx::EffectState &effects)
{
    p.state = with_effects(vitals, effects);
}

Message result(const CombatSession &c)
{
    const auto messages = c.snapshot().log_messages;
    for (auto i = messages.rbegin(); i != messages.rend(); ++i)
        if (i->source.starts_with("{actor} -> {target}: d20"))
            return *i;
    throw std::runtime_error("Missing mastery attack result");
}

std::string arg(const Message &m, std::string_view name)
{
    for (const auto &a : m.arguments)
        if (a.name == name)
            return a.value;
    return {};
}

void act(CombatSession &c, std::string_view verb, EntityId target = 0)
{
    check(c.submit(command(c, verb, target)), "Mastery command accepted");
}

void turn(CombatSession &c, EntityId id)
{
    for (unsigned n = 0; c.snapshot().actor != id && n < 8; ++n)
        act(c, "end");
    check(c.snapshot().actor == id, "Mastery fixture reaches turn");
}

void settle(CombatSession &c)
{
    if (c.snapshot().sneak_attack_choice)
        act(c, "sneak_skip");
    if (c.snapshot().savage_attack_choice)
        act(c, "savage_skip");
    if (c.snapshot().free_movement)
        act(c, "end");
}

Character hero(std::string key, std::string klass = "fighter", std::string background = "sage")
{
    auto draft = character(klass, "Master").creation_data();
    draft.background = background;
    auto &selected = draft.training["class:" + klass + ":weapon_mastery"];
    selected = {key, "club"};
    if (klass == "fighter")
        selected.push_back("dagger");
    return Character(*srd5::character_rules(), draft, {});
}

auto rules(bool immune = false)
{
    std::ifstream in(std::filesystem::path(OPENGOLD_SOURCE_DIR) /
                     "data/rules/srd-5.2.1/combat.rules");
    std::string text{std::istreambuf_iterator<char>(in), {}};
    text += "\ncreature mastery_target 1 1000 0 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n";
    if (immune)
        text += "affinity mastery_target ward immunity all\n";
    return srd5::parse_content(text);
}

Encounter encounter(const RulesModule &rules, const Character &hero, std::string key,
                    bool ranged = false)
{
    const auto profile = rules.character_profile(hero.sheet(), std::array<std::string, 1> {key});
    return {{12, 8, std::vector<std::uint8_t>(96)},
        {   {1, "campaign-character", "Master", 0, {1, 1}, profile.data},
            {99, "mastery_target", "Target", 1, {ranged ? 3 : 2, 1}}
        },
        777};
}

void codec_and_lifecycle()
{
    fx::EffectState attacker, target;
    fx::apply_attack_mastery(attacker, fx::EffectKind::sap, 7, 2, "One", 6000);
    fx::apply_attack_mastery(attacker, fx::EffectKind::sap, 8, 2, "Two", 4000);
    fx::apply_attack_mastery(target, fx::EffectKind::vex, 7, 1, "Master", 9000);
    fx::apply_attack_mastery(target, fx::EffectKind::vex, 8, 1, "Other encounter", 9000);
    fx::apply_attack_mastery(target, fx::EffectKind::vex, 7, 3, "Other actor", 9000);
    check(fx::sapped(attacker) && fx::vexed_by(target, 7, 1) && !fx::vexed_by(target, 9, 1),
          "Source and scope distinguish Vex");
    const auto next = target.next_id;
    fx::apply_attack_mastery(target, fx::EffectKind::vex, 7, 1, "Master", 8000);
    check(target.active.size() == 3 && target.next_id == next,
          "Reapplication refreshes instead of stacking");
    target.prone = true;
    std::ostringstream out;
    fx::write_effects(out, target);
    check(out.str().starts_with("FX7 "), "Effects use the only current tag FX7");
    std::istringstream in(out.str());
    check(fx::read_effects(in) == target, "FX7 preserves posture and multiple sources");
    auto old = out.str();
    old.replace(0, 3, "FX6");
    rejects(
        [&]
    {
        std::istringstream bytes(old);
        (void)fx::read_effects(bytes);
    });
    for (auto bad :
            {"FX7 2 1 1 5 7 2 \"Source\" 0 6001 0 0 0",
             "FX7 2 1 1 6 7 2 \"Source\" 0 12001 0 0 0", "FX7 2 1 1 6 0 2 \"Source\" 0 9000 0 0 0"
            })
        rejects(
            [&]
    {
        std::istringstream bytes(bad);
        (void)fx::read_effects(bytes);
    });
    fx::consume_attack_masteries(attacker, target, 7, 1);
    check(!fx::sapped(attacker) && !fx::vexed_by(target, 7, 1) && target.active.size() == 2,
          "One roll consumes all Sap and only matching Vex");
    std::uint64_t rng = 19;
    const auto before = rng;
    std::array<fx::EffectSubject, 1> subjects{{{99, target, {}}}};
    fx::elapse_effects(subjects, 8999, rng);
    check(target.active.size() == 2, "Vex persists one millisecond before expiry");
    fx::elapse_effects(subjects, 1, rng);
    check(target.active.empty() && rng == before, "Exact expiry needs no saving throw or RNG");
    fx::EffectState full;
    for (unsigned i = 0; i < fx::effect_limit; ++i)
        fx::apply_attack_mastery(full, fx::EffectKind::sap, 7, i + 1, "Source", 6000);
    check(!fx::can_apply_attack_mastery(full, fx::EffectKind::sap, 7, 999) &&
          fx::can_apply_attack_mastery(full, fx::EffectKind::sap, 7, 1),
          "Full store permits same-source refresh only");
    fx::apply_attack_mastery(full, fx::EffectKind::sap, 7, 1, "Source", 5000);
    check(full.active.size() == fx::effect_limit, "Refresh does not overflow");
}

void weapon_matrix()
{
    for (bool immune :
            {
                false, true
            })
    {
        auto r = rules(immune);
        for (const auto &weapon : fx::weapons)
        {
            if (weapon.mastery != fx::Mastery::sap && weapon.mastery != fx::Mastery::vex)
                continue;
            for (bool ranged :
                    {
                        false, true
                    })
            {
                if ((!ranged && weapon.ranged) || (ranged && !weapon.ranged && !weapon.thrown))
                    continue;
                for (unsigned seed :
                        {
                            11u, 72u, 89u
                        })
                {
                    auto e = encounter(*r, hero(std::string(weapon.key)), std::string(weapon.key),
                                       ranged);
                    auto c = r->create(e, seed);
                    turn(*c, 1);
                    const auto ticket = command(*c, ranged ? "ranged" : "melee", 99);
                    auto invalid = ticket;
                    invalid.target = 1234;
                    const auto before = c->save();
                    check(!c->submit(invalid) && c->save() == before,
                          "Invalid mastery attack preserves state and RNG");
                    check(c->submit(ticket), "Every Sap/Vex weapon attack submits");
                    settle(*c);
                    const auto hit = result(*c).source.find("{damage}") != std::string::npos;
                    const auto expected = hit && (weapon.mastery == fx::Mastery::sap || !immune);
                    const auto effects = state(*c, 99);
                    check(fx::has_attack_mastery(effects) == expected,
                          "Mastery requires hit; Vex additionally requires resolved damage");
                    if (expected)
                    {
                        check(effects.active.size() == 1 && effects.active[0].source_scope == 777 &&
                              effects.active[0].source_actor == 1,
                              "Weapon attack retains source");
                        check(effects.active[0].remaining_ms ==
                              (weapon.mastery == fx::Mastery::sap ? 6000u : 9000u),
                              "Source's next start/end determines lifetime");
                    }
                    check(r->restore(c->save())->save() == c->save(),
                          "Every weapon result round trips exactly");
                }
            }
        }
    }
    auto r = rules();
    auto h = character("fighter", "Untrained");
    auto c = r->create(encounter(*r, h, "mace"), 89);
    turn(*c, 1);
    act(*c, "melee");
    check(!fx::has_attack_mastery(state(*c, 99)), "Catalog property alone grants no mastery");
    c = r->create(encounter(*r, hero("shortbow"), "shortbow"), 89);
    turn(*c, 1);
    act(*c, "melee");
    check(!fx::has_attack_mastery(state(*c, 99)), "Unarmed fallback never applies bow mastery");
}

void timing_and_rolls()
{
    auto r = rules();
    for (const auto key :
            {"mace", "rapier"
            })
    {
        auto e = encounter(*r, hero(key), key);
        auto c = r->create(e, 89);
        turn(*c, 1);
        act(*c, "melee");
        check(fx::has_attack_mastery(state(*c, 99)), "Hit installs effect");
        act(*c, "end");
        check(fx::has_attack_mastery(state(*c, 99)), "Effect survives source's current turn");
        act(*c, "end");
        check(fx::has_attack_mastery(state(*c, 99)) == (std::string_view(key) == "rapier"),
              "Sap expires at source's next start, Vex survives");
        auto copy = r->restore(c->save());
        act(*c, "end");
        act(*copy, "end");
        check(!fx::has_attack_mastery(state(*c, 99)) && c->save() == copy->save(),
              "Vex expires exactly at source's next end across reload");
    }
    // Sap on a creature affects its next attack, whether it hits or misses.
    auto e = encounter(*r, hero("mace"), "mace");
    auto c = r->create(e, 89);
    turn(*c, 1);
    act(*c, "melee");
    act(*c, "end");
    act(*c, "melee", 1);
    check(arg(result(*c), "disadvantage") == " (disadvantage)" && !fx::sapped(state(*c, 99)),
          "Target's first attack consumes Sap");
    // Opposing sources cancel even on a pending damage decision.
    for (bool sap :
            {
                false, true
            })
        for (const auto background :
                {"sage", "soldier"
                })
        {
            e = encounter(*r, hero("rapier", "fighter", background), "rapier");
            auto base = r->create(e, 89);
            fx::EffectState target;
            fx::apply_attack_mastery(target, fx::EffectKind::vex, 777, 1, "Master", 9000);
            inject(e.participants[1], unit(*base, 99).persistent, target);
            if (sap)
            {
                fx::EffectState own;
                fx::apply_attack_mastery(own, fx::EffectKind::sap, 777, 99, "Target", 6000);
                inject(e.participants[0], unit(*base, 1).persistent, own);
            }
            c = r->create(e, 89);
            turn(*c, 1);
            act(*c, "melee");
            auto copy = r->restore(c->save());
            check(c->save() == copy->save(), "Pending roll mode remains valid on reload");
            if (c->snapshot().savage_attack_choice)
            {
                act(*c, "savage_use");
                act(*copy, "savage_use");
                check(c->save() == copy->save(), "Second damage roll replay");
                copy = r->restore(c->save());
                act(*c, "savage_first");
                act(*copy, "savage_first");
            }
            check(c->save() == copy->save() && !fx::sapped(state(*c, 1)),
                  "Damage resolution consumes Sap on the live actor");
            check(arg(result(*c), "disadvantage") == (sap ? "" : " (advantage)"),
                  "Sap and Vex use shared cancellation");
            check(state(*c, 99).active.size() == 1 && state(*c, 99).next_id == 3,
                  "Hit consumes previous Vex before applying a fresh one");
        }
    // Attack spells consume existing Sap/Vex but never apply the held weapon's property.
    auto draft = character("wizard", "Caster").creation_data();
    draft.cantrips = std::vector<std::string> {"fire_bolt"};
    Character wizard(*srd5::character_rules(), draft, {});
    e = encounter(*r, wizard, "dagger", true);
    auto base = r->create(e, 89);
    fx::EffectState own, target;
    fx::apply_attack_mastery(own, fx::EffectKind::sap, 777, 99, "Target", 6000);
    fx::apply_attack_mastery(target, fx::EffectKind::vex, 777, 1, "Caster", 9000);
    inject(e.participants[0], unit(*base, 1).persistent, own);
    inject(e.participants[1], unit(*base, 99).persistent, target);
    c = r->create(e, 89);
    turn(*c, 1);
    act(*c, "fire_bolt");
    check(arg(result(*c), "disadvantage").empty() && !fx::has_attack_mastery(state(*c, 1)) &&
          !fx::has_attack_mastery(state(*c, 99)),
          "Spell attack consumes both effects and adds none");
}

void physical_attacks_and_reactions()
{
    for (bool npc :
            {
                false, true
            })
        for (bool thrown :
                {
                    false, true
                })
        {
            CampaignParty party(rules());
            auto h = hero("handaxe");
            h.inventory().add("handaxe", "First");
            h.inventory().add("handaxe", "Second");
            const auto id = npc ? party.recruit("mastery:npc", h) : party.add_pc(h);
            party.equip(id, 1);
            party.equip(id, 2, EquipmentOperation::equip_other);
            party.award_experience(300, "surge");
            party.advance(id, party.default_advancement(id));
            auto actors = party.participants();
            actors.front().cell = {1, 1};
            actors.push_back({99, "mastery_target", "Target", 1, {2, 1}});
            auto c = party.rule_module().create(
            {{12, 8, std::vector<std::uint8_t>(96)}, actors, 777}, 89);
            turn(*c, id);
            act(*c, "melee", 99);
            check(fx::vexed_by(state(*c, 99), 777, id),
                  "Physical first handaxe grants Vex for PC/NPC");
            const auto extra = command(*c, thrown ? "light_throw" : "light_melee", 99);
            check(extra.item == 2, "Second physical handaxe qualifies");
            auto copy = party.rule_module().restore(c->save());
            check(c->submit(extra) && copy->submit(extra) && c->save() == copy->save(),
                  "Light physical attack resumes with Vex");
            check(arg(result(*c), "disadvantage") == (thrown ? "" : " (advantage)"),
                  "Light Vex combines with adjacent ranged disadvantage");
            check(!unit(*c, id).bonus_action, "Light still spends its Bonus Action");
            act(*c, "action_surge");
            act(*c, "melee", 99);
            check(arg(result(*c), "disadvantage") == " (advantage)",
                  "Action Surge uses the freshly applied Vex");
        }
    auto r = rules();
    for (const auto key :
            {"mace", "rapier"
            })
    {
        auto e = encounter(*r, hero(key, "fighter", "soldier"), key);
        auto c = r->create(e, 89);
        turn(*c, 1);
        act(*c, "end");
        Command move;
        for (const auto &option : c->legal_commands())
            if (option.verb == "move" && option.destination == Cell{3, 1})
                move = option;
        check(!move.verb.empty() && c->submit(move) && c->snapshot().reaction_pending,
              "Moving enemy offers mastery reaction");
        act(*c, "opportunity", 99);
        auto copy = r->restore(c->save());
        settle(*c);
        settle(*copy);
        check(c->save() == copy->save(),
              "Reaction damage choice and interrupted movement resume exactly");
        check(fx::has_attack_mastery(state(*c, 99)), "Reaction applies selected mastery");
        check(state(*c, 99).active.front().remaining_ms ==
              (std::string_view(key) == "mace" ? 3000u : 6000u),
              "Reaction expires relative to source turn rather than enemy turn");
        check(!unit(*c, 1).reaction, "Mastery does not refund Reaction");
    }
}

void capacity_and_identity()
{
    auto r = rules();
    for (bool matching :
            {
                false, true
            })
        for (const auto key :
                {"mace", "rapier"
                })
        {
            auto e = encounter(*r, hero(key), key);
            auto base = r->create(e, 89);
            turn(*base, 1);
            const auto ticket = command(*base, "melee", 99);
            fx::EffectState full;
            for (unsigned i = 0; i < fx::effect_limit; ++i)
                fx::apply_attack_mastery(full,
                                         i == 0 && std::string_view(key) == "rapier"
                                         ? fx::EffectKind::vex
                                         : fx::EffectKind::sap,
                                         777, i == 0 && matching ? 1 : 500 + i, "Source", 6000);
            inject(e.participants[1], unit(*base, 99).persistent, full);
            auto c = r->create(e, 89);
            turn(*c, 1);
            const auto before = c->save();
            if (matching)
            {
                check(offers(*c, "melee"), "Existing matching effect keeps bounded attack legal");
                act(*c, "melee", 99);
                check(state(*c, 99).active.size() == fx::effect_limit,
                      "Consume or refresh leaves bounded collection");
            }
            else
                check(!offers(*c, "melee") && !c->submit(ticket) && c->save() == before,
                      "Capacity rejection is atomic before RNG/action consumption");
        }
    auto e = encounter(*r, character("fighter", "No grant"), "mace");
    auto base = r->create(e, 89);
    fx::EffectState effect;
    fx::apply_attack_mastery(effect, fx::EffectKind::sap, 777, 1, "Master", 6000);
    inject(e.participants[1], unit(*base, 99).persistent, effect);
    auto c = r->create(e, 89);
    auto old = c->save();
    const auto position = old.find(r->identity().version);
    check(position != old.npos, "Find module identity");
    old.replace(position, r->identity().version.size(), "0.6.55");
    rejects(
        [&]
    {
        (void)r->restore(old);
    });
    // Campaign vitals may carry mastery effects between encounters.
    const auto campaign_state = with_effects(unit(*base, 1).persistent, effect);
    r->validate_character_state(character("fighter", "No grant").sheet(), campaign_state);
}

void ui_fixtures()
{
    const auto output = std::getenv("OPENGOLD_MASTERY_FIXTURES");
    if (!output)
        return;
    auto r = module();
    const auto path = std::filesystem::path(output);
    std::filesystem::create_directories(path);
    for (const auto key :
            {"mace", "rapier"
            })
    {
        auto e = encounter(*r, hero(key), key);
        e.participants[1].definition = "vanguard";
        bool written = false;
        for (unsigned seed = 1; seed < 128 && !written; ++seed)
        {
            auto c = r->create(e, seed);
            turn(*c, 1);
            const auto before = c->save();
            act(*c, "melee", 99);
            if (!fx::has_attack_mastery(state(*c, 99)))
                continue;
            for (const auto suffix :
                    {"-before", "-after"
                    })
            {
                std::ofstream out(path / (std::string(key) + suffix + ".save"), std::ios::binary);
                out << (std::string_view(suffix) == "-before" ? before : c->save());
                check(bool(out), "Write actual mastery UI checkpoint");
            }
            written = true;
        }
        check(written, "UI fixture actually hits");
    }
}

void pending_interactions()
{
    auto r = rules();
    for (const auto klass :
            {"fighter", "rogue"
            })
    {
        CampaignParty p(rules());
        const auto id = p.add_pc(hero("rapier", klass, "soldier"));
        p.award_experience(900, "mastery-interaction");
        while (p.member(id).character.sheet().level < 3)
            p.advance(id, p.default_advancement(id));
        auto e = encounter(*r, p.member(id).character, "rapier");
        auto base = r->create(e, 72);
        fx::EffectState target;
        fx::apply_attack_mastery(target, fx::EffectKind::vex, 777, 1, "Master", 9000);
        inject(e.participants[1], unit(*base, 99).persistent, target);
        auto c = r->create(e, 72);
        turn(*c, 1);
        act(*c, "melee");
        auto copy = r->restore(c->save());
        if (std::string_view(klass) == "rogue")
        {
            check(bool(c->snapshot().sneak_attack_choice), "Vex qualifies Sneak Attack");
            act(*c, "sneak_use");
            act(*copy, "sneak_use");
            copy = r->restore(c->save());
        }
        check(bool(c->snapshot().savage_attack_choice), "Savage follows Sneak when both apply");
        act(*c, "savage_skip");
        act(*copy, "savage_skip");
        check(c->save() == copy->save() && state(*c, 99).active.size() == 1,
              "Pending damage choices consume then renew Vex once");
        if (std::string_view(klass) == "fighter")
        {
            check(bool(c->snapshot().free_movement), "Champion critical retains free movement");
            copy = r->restore(c->save());
            act(*c, "end");
            act(*copy, "end");
            check(c->save() == copy->save(), "Champion movement continuation retains Vex");
        }
    }
}

void run()
{
    codec_and_lifecycle();
    weapon_matrix();
    timing_and_rolls();
    physical_attacks_and_reactions();
    capacity_and_identity();
    pending_interactions();
    ui_fixtures();
}
} // namespace mastery_combat_checks
