#include "opengold/campaign_save.h"
#include "opengold/srd5.h"
#include "status_effects.h"
#include "combat_grid.h"
#include "recovery_timeline.h"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
using namespace opengold;
using namespace opengold::rules;
namespace fx = opengold::srd5::detail;

namespace
{
void check(bool b, const char *message)
{
    if (!b)
        throw std::runtime_error(message);
}

template <class F> void rejects(F f)
{
    bool failed = false;
    try
    {
        f();
    }
    catch (const std::exception &)
    {
        failed = true;
    }
    check(failed, "Invalid sleep state accepted");
}

auto module()
{
    return srd5::load(std::filesystem::path(OPENGOLD_SOURCE_DIR) /
                      "data/rules/srd-5.2.1/combat.rules");
}

Character hero(std::string klass = "fighter")
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = klass;
    if (klass == "cleric")
        d.cantrips = std::vector<std::string> {"sacred_flame"};
    d.background = "soldier";
    d.alignment = "neutral_good";
    d.name = "Sleeper";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    return Character(*srd5::character_rules(), d, {});
}

CombatantView unit(const CombatSession &s, EntityId id)
{
    for (const auto &a : s.snapshot().combatants)
        if (a.id == id)
            return a;
    throw std::runtime_error("Missing actor");
}

Command command(const CombatSession &s, std::string_view verb, EntityId target = 0)
{
    for (const auto &c : s.legal_commands())
        if (c.verb == verb && (!target || c.target == target))
            return c;
    throw std::runtime_error("Missing command: " + std::string(verb));
}

void turn(CombatSession &s, EntityId id)
{
    for (unsigned n = 0; n < 20 && s.snapshot().actor != id; ++n)
        check(s.submit(command(s, "end")), "Advance fixture turn");
    check(s.snapshot().actor == id, "Requested conscious turn reached");
}

void codec()
{
    fx::EffectState state;
    state.sleeping = state.prone = true;
    std::ostringstream out;
    fx::write_effects(out, state);
    check(out.str() == "FX4 1 0 1 1", "Canonical sleep codec");
    std::istringstream in(out.str());
    check(fx::read_effects(in) == state, "Sleep round trips");
    for (const auto bad :
            {"FX4 1 0 1 0", "FX4 1 0 2 1", "FX4 1 0 0 0", "FX4 1 0 -1 1"
            })
        rejects(
            [&]
    {
        std::istringstream input(bad);
        (void)fx::read_effects(input);
    });
    std::uint64_t rng = 17;
    std::array<fx::EffectSubject, 1> subjects{{{1, state, {}}}};
    fx::elapse_effects(subjects, 86400000, rng);
    check(state.sleeping && state.prone && rng == 17,
          "Sleep has no guessed expiry, saving throw or RNG cost");
    for (const auto old :
            {"FX1 1 0", "FX2 2 1 1 2 1 1 \"Caster\" 0 6000 0", "FX3 2 1 1 3 1 1 \"Caster\" 0 6000 0"
            })
    {
        std::istringstream input(old);
        const auto decoded = fx::read_effects(input);
        std::ostringstream output;
        fx::write_effects(output, decoded);
        check(output.str() == old, "Prior effect codecs retain exact continuation");
    }
}

void combat()
{
    const auto rules = module();
    CampaignParty party(module());
    auto id = party.add_pc(hero());
    party.add_pc(hero());
    const auto began = party.begin_rest(RestKind::long_rest);
    check(began.has_value(), "Start natural sleep");
    const auto encoded = encode_campaign(party, nullptr, "sleep");
    auto decoded = decode_campaign(encoded, *srd5::character_rules(), *rules, "sleep", nullptr);
    party.restore(decoded.party);
    check(encode_campaign(party, nullptr, "sleep") == encoded,
          "Sleeping campaign round trips exactly");
    const std::array<MemberId, 1> awake{2};
    party.loud_noise(awake);
    auto participants = party.participants();
    participants[0].cell = {2, 2};
    participants[1].cell = {2, 3};
    participants.push_back({3, "bandit", "Enemy", 1, {6, 2}});
    auto s = rules->create({{9, 7, std::vector<std::uint8_t>(63)}, participants}, 37);
    const auto sleeper = unit(*s, id);
    check(sleeper.naturally_sleeping && sleeper.prone && !sleeper.conscious &&
          sleeper.movement_feet == 0 && !sleeper.action && !sleeper.reaction &&
          !sleeper.bonus_action,
          "Sleep grants Unconscious without changing HP");
    check(s->movement_reach(id).empty(), "Sleeping actor has no movement preview");
    const auto output = std::filesystem::path(OPENGOLD_BINARY_DIR) / "sleep-fixtures";
    std::filesystem::create_directories(output);
    const auto write = [&](const char *name, const CombatSession & c)
    {
        std::ofstream out(output / (std::string(name) + ".save"), std::ios::binary);
        out << c.save();
        check(bool(out), "Sleep UI fixture written");
    };
    turn(*s, 3);
    write("offturn", *s);
    turn(*s, 2);
    write("wake", *s);
    const auto ticket = command(*s, "wake_ally", 1);
    check(s->submit(ticket), "Adjacent ally can spend an Action to wake");
    check(!unit(*s, 1).naturally_sleeping && unit(*s, 1).prone && !unit(*s, 2).action,
          "Wake spends Action and retains Prone");
    const auto after = s->save();
    check(!s->submit(ticket) && s->save() == after, "Stale wake cannot spend resources or RNG");
    s = rules->restore(after);
    check(s->save() == after, "Woken Prone combat reloads exactly");
    turn(*s, 1);
    write("stand", *s);
    const auto movement = unit(*s, 1).movement_feet;
    check(s->submit(command(*s, "stand_up")), "Standing is available after waking");
    check(!unit(*s, 1).prone && unit(*s, 1).action && unit(*s, 1).movement_feet == movement - 15,
          "Stand costs half Speed without an Action");
    const auto stood = s->save();
    check(rules->restore(stood)->save() == stood, "Standing continuation round trips");
    // Walls block the wake interaction; being nearby alone is insufficient.
    auto wall = std::vector<std::uint8_t>(63);
    wall[2 * 9 + 3] = 1;
    participants[1].cell = {3, 3};
    auto blocked = rules->create({{9, 7, wall}, participants}, 37);
    turn(*blocked, 2);
    write("unreachable", *blocked);
    const auto offers = blocked->legal_commands();
    check(std::none_of(offers.begin(), offers.end(),
                       [](const auto & c)
    {
        return c.verb == "wake_ally";
    }),
    "Wake cannot reach through a blocked diagonal");
}

void damage_and_saves()
{
    const auto rules = module();
    const auto target = hero();
    VitalState state{target.sheet().hit_points};
    rules->set_rest_work(state, target.sheet(), RestWork::sleep);
    rules->grant_temporary_hit_points(state, target.sheet(), {30, "test:buffer"},
                                      TemporaryHpChoice::use_new);
    bool witnessed = false;
    for (unsigned seed = 1; seed < 100 && !witnessed; ++seed)
    {
        auto battle = rules->create({{8, 8, std::vector<std::uint8_t>(64)},
            {   {1, "bandit", "Attacker", 0, {2, 2}},
                {
                    2,
                    "campaign-character",
                    "Sleeper",
                    1,
                    {3, 2},
                    rules->character_profile(target.sheet(), {}).data,
                    state
                }
            }},
        seed);
        const auto initial = battle->save();
        check(!battle->submit({battle->snapshot().revision, 2, 1, "melee"}) &&
              battle->save() == initial,
              "Sleeping actor cannot submit attacks");
        check(battle->submit(command(*battle, "melee", 2)), "Attack sleeper");
        const auto victim = unit(*battle, 2);
        if (victim.temporary_hp.amount == 30)
            continue;
        check(!victim.naturally_sleeping && victim.prone && victim.hit_points == state.hit_points,
              "Damage absorbed by Temporary HP still wakes; Prone remains");
        for (const auto &line : battle->snapshot().log)
            if (line.find("CRITICAL") != line.npos && line.find("d20 20 ") == line.npos)
                witnessed = true;
        check(rules->restore(battle->save())->save() == battle->save(),
              "Damage wake continuation persists");
    }
    check(witnessed, "A non-natural-20 hit within five feet is critical against sleep");
    const auto cleric = hero("cleric");
    auto spell = rules->create({{8, 8, std::vector<std::uint8_t>(64)},
        {   {
                1,
                "campaign-character",
                "Cleric",
                0,
                {2, 2},
                rules->character_profile(cleric.sheet(), {}).data
            },
            {
                2,
                "campaign-character",
                "Sleeper",
                1,
                {4, 2},
                rules->character_profile(target.sheet(), {}).data,
                state
            }
        }},
    9);
    check(spell->submit(command(*spell, "sacred_flame", 2)),
          "Dexterity-save spell targets sleeper");
    bool failed = false;
    for (const auto &line : spell->snapshot().log)
        failed |= line.find("automatically fails the Dexterity") != line.npos;
    check(failed && !unit(*spell, 2).naturally_sleeping,
          "Unconscious automatically fails Dexterity; resulting damage wakes");
}

void prior_writer()
{
    const auto rules = module();
    const auto read = [](const char *name)
    {
        std::ifstream in(std::filesystem::path(OPENGOLD_SOURCE_DIR) / "tests/fixtures" / name,
                         std::ios::binary);
        check(bool(in), "Actual prior writer fixture exists");
        return std::string(std::istreambuf_iterator<char>(in), {});
    };
    const auto normalized = [&](std::string bytes)
    {
        const auto pos = bytes.find("0.6.40");
        check(pos != bytes.npos, "Frozen writer identity is unchanged");
        bytes.replace(pos, 6, rules->identity().version);
        return bytes;
    };
    const auto before = read("combat-v15-sleep-before.save");
    auto battle = rules->restore(before);
    check(battle->save() == normalized(before),
          "Actual 0.6.40 combat retains exact state apart from identity");
    check(battle->submit(command(*battle, "cunning_dash")) &&
          battle->submit(command(*battle, "dash")),
          "Continue actual prior writer's actions");
    check(battle->save() == normalized(read("combat-v15-sleep-continued.save")),
          "Prior writer's next commands/resources/RNG remain byte-exact");
}

void held_items()
{
    const auto rules = module();
    const auto person = hero();
    VitalState sleep{person.sheet().hit_points};
    rules->set_rest_work(sleep, person.sheet(), RestWork::sleep);
    const std::array<std::string, 2> gear{"longsword", "shield"};
    auto battle = rules->create({{9, 7, std::vector<std::uint8_t>(63)},
        {   {
                1,
                "campaign-character",
                "Sleeper",
                0,
                {2, 2},
                rules->character_profile(person.sheet(), gear).data,
                sleep
            },
            {
                2,
                "campaign-character",
                "Ally",
                0,
                {2, 3},
                rules->character_profile(person.sheet(), {}).data
            },
            {3, "bandit", "Enemy", 1, {6, 2}}
        }},
    37);
    const auto state = battle->snapshot();
    check(state.held_items.size() == 2 && state.held_items[0].holder == 0 &&
          state.held_items[1].holder == 0,
          "Sleep drops both held weapon and shield");
    check(unit(*battle, 1).armor_class == rules->character_profile(person.sheet(), {}).armor_class,
          "Dropped shield no longer supplies AC");
    check(battle->save().starts_with("OGCOMBAT 16 ") &&
          rules->restore(battle->save())->save() == battle->save(),
          "Dropped equipment and budgets round trip");
    turn(*battle, 2);
    const auto fixture_path = std::filesystem::path(OPENGOLD_BINARY_DIR) / "sleep-fixtures";
    {
        std::ofstream out(fixture_path / "ground.save", std::ios::binary);
        out << battle->save();
    }
    check(battle->submit(command(*battle, "wake_ally", 1)), "Spend Action waking item owner");
    const auto pickup = command(*battle, "pick_up", 1);
    check(battle->submit(pickup),
          "Another character can pick up the owner's weapon with free interaction");
    check(!unit(*battle, 2).action && battle->snapshot().held_items[0].holder == 2,
          "Pickup does not refund spent Action and records actual holder");
    const auto saved = battle->save();
    check(!battle->submit(pickup) && battle->save() == saved,
          "Stale pickup cannot duplicate equipment or spend again");
    battle = rules->restore(saved);
    check(battle->save() == saved, "Cross-character pickup persists exactly");
    const auto commands = battle->legal_commands();
    check(std::none_of(commands.begin(), commands.end(),
                       [](const auto & c)
    {
        return c.verb == "pick_up";
    }),
    "Shield pickup needs available Utilize action");
    turn(*battle, 1);
    check(battle->submit(command(*battle, "pick_up", 2)),
          "Owner can recover shield with an Action");
    check(!unit(*battle, 1).action &&
          unit(*battle, 1).armor_class ==
          rules->character_profile(person.sheet(), gear).armor_class,
          "Shield recovery restores only AC and spends Action");
    check(rules->restore(battle->save())->save() == battle->save(),
          "Shield recovery reloads exactly");
    // The serialized equipment ledger cannot create an unknown holder or duplicate token.
    auto invalid = saved;
    auto suffix = invalid.rfind("\n2\n1 2 0 0\n");
    check(suffix != invalid.npos, "Held-item checkpoint has canonical count and first token");
    invalid.replace(suffix + 5, 1, "999");
    rejects(
        [&]
    {
        (void)rules->restore(invalid);
    });
    bool witnessed = false;
    for (unsigned seed = 1; seed < 100 && !witnessed; ++seed)
    {
        auto hit = rules->create({{8, 8, std::vector<std::uint8_t>(64)},
            {   {1, "bandit", "Attacker", 0, {2, 2}},
                {
                    2,
                    "campaign-character",
                    "Wounded",
                    1,
                    {3, 2},
                    rules->character_profile(person.sheet(), gear).data,
                    VitalState{1}
                }
            }},
        seed);
        const auto available = hit->legal_commands();
        const auto melee = std::find_if(available.begin(), available.end(),
                                        [](const auto & c)
        {
            return c.verb == "melee";
        });
        if (melee == available.end())
            continue;
        check(hit->submit(*melee), "Damage fixture attack");
        if (unit(*hit, 2).hit_points)
            continue;
        witnessed = true;
        const auto fallen = hit->snapshot();
        check(fallen.held_items.size() == 2 &&
              std::all_of(fallen.held_items.begin(), fallen.held_items.end(),
                          [](const auto & i)
        {
            return !i.holder;
        }),
        "Falling to zero HP drops held gear");
        check(rules->restore(hit->save())->save() == hit->save(),
              "Lethal hit and dropped gear preserve continuation");
    }
    check(witnessed, "An actual attack dropped equipment at zero HP");
    std::ifstream old(std::filesystem::path(OPENGOLD_SOURCE_DIR) /
                      "tests/fixtures/combat-v8-grants-continued.save");
    check(bool(old), "Actual prior-writer equipment fixture exists");
    auto legacy = rules->restore(std::string(std::istreambuf_iterator<char>(old), {}));
    for (unsigned n = 0; n < 100 && legacy->snapshot().held_items.empty(); ++n)
    {
        const auto available = legacy->legal_commands();
        const auto attack =
            std::find_if(available.begin(), available.end(),
                         [](const auto & c)
        {
            return c.actor == 99 && c.verb == "melee" && c.target == 2;
        });
        check(legacy->submit(attack != available.end() ? *attack : command(*legacy, "end")),
              "Continue legacy combat until another holder falls");
    }
    const auto migrated = legacy->snapshot();
    check(!migrated.held_items.empty() && !unit(*legacy, 1).conscious,
          "Legacy encounter activates equipment ledger with an already-fallen holder");
    check(std::all_of(migrated.held_items.begin(), migrated.held_items.end(),
                      [](const auto & i)
    {
        return i.origin != 1 || !i.holder;
    }),
    "Ledger activation reconciles previously unconscious holders");
    check(rules->restore(legacy->save())->save() == legacy->save(),
          "Legacy fall and new drop produce a valid continuation");
}

void campaign_item_handoff()
{
    const auto rules = module();
    auto first = hero();
    const auto sword = first.inventory().add("longsword", "Owned sword");
    const auto shield = first.inventory().add("shield", "Owned shield");
    CampaignParty party(module());
    const auto owner = party.add_pc(std::move(first));
    const auto ally = party.add_pc(hero());
    party.equip(owner, sword);
    party.equip(owner, shield);
    auto sleeping = party.checkpoint();
    rules->set_rest_work(sleeping.roster[0].vitals, sleeping.roster[0].character.sheet(),
                         RestWork::sleep);
    party.restore(sleeping);
    auto people = party.participants();
    people[0].cell = {2, 2};
    people[1].cell = {2, 3};
    people.push_back({1000, "bandit", "Enemy", 1, {6, 2}});
    auto battle = rules->create({{9, 7, std::vector<std::uint8_t>(63)}, people}, 37);
    party.begin_combat();
    party.apply_combat(battle->snapshot());
    check(party.member(owner).equipped.empty() &&
          party.member(owner).character.inventory().empty() &&
          party.state().detached_items.size() == 2,
          "Dropping moves actual inventory to ground without copies");
    party.apply_combat(battle->snapshot());
    check(party.state().detached_items.size() == 2,
          "Repeated snapshot does not duplicate dropped items");
    turn(*battle, ally);
    check(battle->submit(command(*battle, "pick_up", 1)), "Ally picks up original inventory item");
    party.apply_combat(battle->snapshot());
    const auto &acquired = party.member(ally).character.inventory().items();
    check(acquired.size() == 1 && acquired[0].name == "Owned sword" &&
          party.member(ally).equipped.size() == 1 && party.state().detached_items.size() == 1,
          "Cross-character pickup transfers physical item and name exactly once");
    auto invalid = battle->snapshot();
    invalid.held_items[0].definition = "dagger";
    const auto before = party.checkpoint();
    rejects(
        [&]
    {
        party.apply_combat(invalid);
    });
    check(party.member(ally).character.inventory().items().size() == 1 &&
          party.state().random_state == before.random_state &&
          party.state().detached_items.size() == 1,
          "Rejected manifest preserves inventory and RNG");
    party.end_combat();
    const auto bytes = encode_campaign(party, nullptr, "detached-items");
    check(bytes.starts_with("OPENGOLD-CAMPAIGN 13"),
          "Detached items use versioned campaign persistence");
    {
        std::ofstream out(std::filesystem::path(OPENGOLD_BINARY_DIR) /
                          "sleep-fixtures/campaign-detached.ogs", std::ios::binary);
        out << bytes;
    }
    auto decoded =
        decode_campaign(bytes, *srd5::character_rules(), *rules, "detached-items", nullptr);
    CampaignParty copy(module());
    copy.restore(decoded.party);
    check(encode_campaign(copy, nullptr, "detached-items") == bytes &&
          copy.state().detached_items.size() == 1,
          "Uncollected equipment persists without assumed automatic cleanup");
}

void recovery_posture()
{
    // A legacy zero-HP record gains explicit posture only when it actually recovers.
    for (const bool stable :
            {
                false, true
            })
    {
        fx::LifeState life
        {
0, 0, 0, stable, false, stable ? fx::RecoveryClock{0, 1} :
            fx::RecoveryClock{1, 0}};
        fx::EffectState effects;
        std::uint64_t rng = 17;
        std::array<fx::RecoverySubject, 1> subjects{{{{1, effects, {}}, life}}};
        fx::elapse_recovery(subjects, 1, rng);
        check(life.hp == 1 && effects.prone && !effects.sleeping,
              "Stable recovery and natural-20 death saves leave legacy actors Prone");
        check(rng == (stable ? 17 : 11400714819323198502ULL),
              "Adding Prone does not alter recovery RNG");
    }
}

void rest_ground_equipment()
{
    const auto rules = module();
    auto character = hero();
    const auto sword = character.inventory().add("longsword", "Camp sword", 2, 34);
    const auto shield = character.inventory().add("shield", "Camp shield");
    const auto armor = character.inventory().add("chain_mail", "Worn armor");
    CampaignParty party(module());
    const auto owner = party.add_pc(std::move(character));
    const auto ally = party.add_pc(hero());
    party.equip(owner, sword);
    party.equip(owner, shield);
    party.equip(owner, armor);
    auto original = party.checkpoint();
    por::Equipment source;
    source.stored.type = 34;
    source.stored.value = 71;
    source.stored.stack_size = 2;
    original.roster[0].item_sources.emplace(sword, source);
    party.restore(original);
    const auto ticket = *party.begin_rest(RestKind::long_rest);
    auto legacy_sleep = party.checkpoint();
    legacy_sleep.detached_items.clear();
    legacy_sleep.roster[0].character = original.roster[0].character;
    legacy_sleep.roster[0].equipped = original.roster[0].equipped;
    legacy_sleep.roster[0].item_sources = original.roster[0].item_sources;
    CampaignParty waking_old_sleep(module());
    waking_old_sleep.restore(legacy_sleep);
    const std::array<MemberId, 1> wake_owner{owner};
    waking_old_sleep.loud_noise(wake_owner);
    check(waking_old_sleep.state().detached_items.size() == 2 &&
          waking_old_sleep.member(owner).equipped == std::vector<std::uint64_t> {armor},
          "Waking an older resting record reconciles held items before removing sleep");
    CampaignParty damaged_old_sleep(module());
    damaged_old_sleep.restore(legacy_sleep);
    std::vector<std::uint8_t> script{0, 0};
    for (unsigned n = 0; n < 5; ++n)
        script.insert(script.end(), {1, 1, 0x15, 0x99});
    script.insert(script.end(), {0, 0});
    por::EclMachine vm(std::make_shared<const por::EclProgram>(
                           por::EclProgram::decode(script, "rest item damage")));
    for (const auto &write : damaged_old_sleep.character_reply(0).writes)
        vm.bind_variable(write.address, write.value);
    vm.bind_variable(0x6C19, damaged_old_sleep.member(owner).vitals.hit_points - 1);
    damaged_old_sleep.read_character(0, vm);
    check(damaged_old_sleep.state().detached_items.size() == 2 &&
          damaged_old_sleep.state().rest_activity->interrupted &&
          damaged_old_sleep.participants()[0].ground_equipment.size() == 2,
          "Script damage retains camp drops when it wakes an older sleeping record");
    check(party.member(owner).equipped == std::vector<std::uint64_t> {armor} &&
          party.state().detached_items.size() == 2,
          "Sleep releases held items but keeps worn armor");
    check(party.member(owner).character.inventory().find(sword)->get().quantity == 1 &&
          party.member(owner).item_sources.contains(sword),
          "Dropping a held stack member retains the remaining inventory and provenance");
    check(party.state().detached_items[0].rest_session == ticket.session &&
          party.state().detached_items[0].original->stored.value == 71,
          "Camp ground item retains session and original-item provenance");
    const std::array<MemberId, 2> awake{owner, ally};
    party.loud_noise(awake);
    const auto bytes = encode_campaign(party, nullptr, "rest-ground");
    check(bytes.starts_with("OPENGOLD-CAMPAIGN 14"),
          "Unplaced camp equipment uses a versioned save extension");
    auto restored =
        decode_campaign(bytes, *srd5::character_rules(), *rules, "rest-ground", nullptr);
    party.restore(restored.party);
    check(encode_campaign(party, nullptr, "rest-ground") == bytes,
          "Awake characters and ground camp equipment reload exactly");
    auto malformed = restored.party;
    malformed.detached_items[0].rest_session = malformed.next_rest_session;
    rejects(
        [&]
    {
        party.restore(malformed);
    });
    check(encode_campaign(party, nullptr, "rest-ground") == bytes,
          "Invalid camp item identity preserves the live party");
    malformed = restored.party;
    malformed.detached_items[1].item.definition_id = "invalid:camp-item";
    rejects(
        [&]
    {
        party.restore(malformed);
    });
    check(encode_campaign(party, nullptr, "rest-ground") == bytes,
          "Unknown camp equipment rejects without mutating the live party");
    CampaignParty abandoned(module());
    abandoned.restore(restored.party);
    abandoned.abandon_rest(abandoned.state().rest_activity->ticket);
    const auto unrelated = abandoned.participants();
    check(std::all_of(unrelated.begin(), unrelated.end(),
                      [](const auto & p)
    {
        return p.ground_equipment.empty();
    }),
    "Abandoned camp gear does not teleport into another encounter");
    check(abandoned.state().detached_items.empty() &&
          abandoned.member(owner).character.inventory().items().size() == 4,
          "Safe cancellation collects the physical gear into inventory");
    check(party.prepare_combat(), "Immediate interruption has no unearned Hit Dice choice");
    auto people = party.participants();
    people[0].cell = {2, 2};
    people[1].cell = {2, 3};
    people.push_back({1000, "bandit", "Enemy", 1, {6, 2}});
    check(people[0].ground_equipment == std::vector<unsigned> {1, 2},
          "Interruption formation carries explicit ground ordinals after worn armor");
    auto bad_people = people;
    bad_people[0].ground_equipment.push_back(99);
    rejects(
        [&]
    {
        (void)rules->create({{9, 7, std::vector<std::uint8_t>(63)}, bad_people}, 37);
    });
    auto battle = rules->create(
    {{9, 7, std::vector<std::uint8_t>(63)}, people, party.state().next_combat_scope}, 37);
    const auto ground = battle->snapshot();
    check(!unit(*battle, owner).naturally_sleeping && ground.held_items.size() == 2 &&
          std::all_of(ground.held_items.begin(), ground.held_items.end(),
                      [](const auto & i)
    {
        return !i.holder;
    }),
    "Waking before initiative does not re-equip camp ground items");
    check(unit(*battle, owner).armor_class == party.profile(owner).armor_class,
          "Initial ground shield supplies no combat AC");
    check(rules->restore(battle->save())->save() == battle->save(),
          "Rest-imported ground items use the existing combat continuation format");
    party.begin_combat();
    party.apply_combat(battle->snapshot());
    party.apply_combat(battle->snapshot());
    check(party.state().detached_items.size() == 2 &&
          std::all_of(party.state().detached_items.begin(), party.state().detached_items.end(),
                      [](const auto & i)
    {
        return i.scope && !i.rest_session;
    }),
    "Rest inventory converts to positioned encounter inventory exactly once");
    turn(*battle, ally);
    check(battle->submit(command(*battle, "pick_up", 1)),
          "Ally can pick up the camp weapon after interruption");
    party.apply_combat(battle->snapshot());
    const auto acquired = party.member(ally).equipped.front();
    check(party.member(ally).character.inventory().find(acquired)->get().name == "Camp sword" &&
          party.member(ally).item_sources.at(acquired).stored.value == 71,
          "Camp-to-combat pickup retains physical item provenance");
    party.end_combat();
}

void safe_recovery()
{
    const auto rules = module();
    for (const bool complete :
            {
                false, true
            })
    {
        CampaignParty party(module());
        auto person = hero();
        const auto sword = person.inventory().add("longsword", "Recovered sword", 1, 34);
        const auto id = party.add_pc(std::move(person));
        party.equip(id, sword);
        auto initial = party.checkpoint();
        initial.roster[0].vitals.hit_points = 1;
        party.restore(initial);
        const auto ticket = *party.begin_rest(RestKind::long_rest);
        if (complete)
            (void)party.advance_rest(ticket, party.remaining_rest_milliseconds(), RestWork::sleep);
        else
            party.abandon_rest(ticket);
        check(party.state().detached_items.empty() && !party.state().rest_activity,
              "Both safe camp endings collect ground equipment");
        auto vitals = party.member(id).vitals;
        check(rules->recover_at_safety(vitals, party.member(id).character.sheet(), {}) &&
              vitals == party.member(id).vitals,
              "Safe camp ending already leaves able members standing");
        check(party.member(id).vitals.hit_points ==
              (complete ? party.member(id).character.sheet().hit_points : 1) &&
              party.state().random_state == 42,
              "Cancellation grants no healing or recovery RNG");
        const auto bytes = encode_campaign(party, nullptr, "safe-camp");
        party.restore(
            decode_campaign(bytes, *srd5::character_rules(), *rules, "safe-camp", nullptr).party);
        check(encode_campaign(party, nullptr, "safe-camp") == bytes,
              "Collected camp inventory saves exactly");
    }
    for (const bool wall :
            {
                false, true
            })
        for (const bool dead :
                {
                    false, true
                })
        {
            CampaignParty party(module());
            auto person = hero();
            const auto sword = person.inventory().add("longsword", "Owner sword", 1, 34);
            const auto owner = party.add_pc(std::move(person));
            const auto ally = party.add_pc(hero());
            party.equip(owner, sword);
            (void)party.begin_rest(RestKind::long_rest);
            const std::array<MemberId, 1> awake{ally};
            party.loud_noise(awake);
            if (dead)
            {
                auto state = party.checkpoint();
                state.roster[0].vitals = {0, true};
                party.restore(state);
            }
            check(party.prepare_combat(), "Safe recovery fixture interrupts rest");
            auto people = party.participants();
            people[0].cell = {1, 2};
            people[1].cell = {5, 2};
            people.push_back(
            {1000, "bandit", "Defeated enemy", 1, {6, 4}, {}, VitalState{0, true}});
            Battlefield board{8, 6, std::vector<std::uint8_t>(48)};
            if (wall)
                for (int y = 0; y < 6; ++y)
                    board.terrain[y * 8 + 3] = 1;
            auto battle = rules->create({board, people, party.state().next_combat_scope}, 37);
            check(battle->snapshot().outcome == Outcome::victory, "Fixture has a terminal victory");
            const auto before = battle->save();
            const auto plan = battle->safe_recovery();
            check(plan.members == std::vector<EntityId> {ally} &&
                  plan.items.size() == (wall ? 0 : 1),
                  "Only conscious allies collect equipment reachable around terrain");
            check(battle->save() == before, "Recovery query leaves combat costs and RNG untouched");
            party.begin_combat();
            party.apply_combat(battle->snapshot(), plan);
            const auto once = party.checkpoint();
            party.apply_combat(battle->snapshot(), plan);
            for (unsigned n = 0; n < 2; ++n)
                check(std::ranges::equal(party.state().roster[n].character.inventory().items(),
                                         once.roster[n].character.inventory().items()),
                      "Repeated terminal handoff cannot duplicate recovered gear");
            party.end_combat();
            check(party.state().detached_items.size() == (wall ? 1 : 0),
                  "Unreachable equipment remains at the encounter");
            if (!wall)
            {
                const auto &items = party.member(dead ? ally : owner).character.inventory().items();
                check(items.size() == 1 && items.front().name == "Owner sword",
                      "Items return to living owner or survivor when owner died");
            }
            check(party.member(owner).vitals.hit_points ==
                  (dead ? 0 : party.member(owner).character.sheet().hit_points),
                  "Safe collection cannot heal or revive an owner");
        }
}

void prior_equipment_formats()
{
    const auto rules = module();
    const auto read = [](const char *name)
    {
        std::ifstream in(std::filesystem::path(OPENGOLD_SOURCE_DIR) / "tests/fixtures" / name,
                         std::ios::binary);
        check(bool(in), "Previous equipment writer fixture exists");
        return std::string(std::istreambuf_iterator<char>(in), {});
    };
    const auto campaign = read("campaign-v13-detached.ogs");
    CampaignParty party(module());
    party.restore(
        decode_campaign(campaign, *srd5::character_rules(), *rules, "detached-items", nullptr)
        .party);
    const auto normalized = [&](std::string bytes)
    {
        const auto at = bytes.find("0.6.42");
        check(at != bytes.npos, "Prior equipment fixture keeps original identity");
        bytes.replace(at, 6, rules->identity().version);
        return bytes;
    };
    const auto body = [](const std::string & bytes)
    {
        return bytes.substr(bytes.find('\n', bytes.find('\n') + 1) + 1);
    };
    check(body(encode_campaign(party, nullptr, "detached-items")) == body(normalized(campaign)),
          "Actual campaign 13 state unchanged apart from identity/checksum");
    const auto combat = read("combat-v16-ground.save");
    check(rules->restore(combat)->save() == normalized(combat),
          "Actual combat 16 writer remains byte-exact");
}

void movement()
{
    Battlefield board{5, 5, std::vector<std::uint8_t>(25)};
    board.terrain[2 * 5 + 3] = 2;
    fx::MovementGrid grid(board, {2, 2}, {}, true);
    check(grid.step_cost({2, 2}, {2, 3}) == 10 && grid.step_cost({2, 2}, {3, 2}) == 15,
          "Crawling and difficult terrain add independent movement costs");
    check(grid.reachable(14).cost_to({3, 2}) == std::nullopt &&
          grid.reachable(15).cost_to({3, 2}) == 15,
          "Crawling reach uses exact weighted path cost");
}
} // namespace

int main()
{
    try
    {
        codec();
        combat();
        damage_and_saves();
        prior_writer();
        held_items();
        campaign_item_handoff();
        recovery_posture();
        rest_ground_equipment();
        prior_equipment_formats();
        safe_recovery();
        movement();
        std::cout << "Natural sleep tests passed\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
