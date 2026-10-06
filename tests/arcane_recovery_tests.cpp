#include "opengold/campaign_save.h"
#include "opengold/srd5.h"
#include "../src/OpenGold.Rules.Srd5/src/status_effects.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace opengold;
using namespace opengold::rules;

namespace
{
const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR);

void check(bool value, const char *message)
{
    if (!value)
        throw std::runtime_error(message);
}

std::string read(const std::filesystem::path &path)
{
    std::ifstream input(path);
    check(bool(input), "Read fixture/content");
    return {std::istreambuf_iterator<char>(input), {}};
}

auto module()
{
    return srd5::parse_content(
               read(root / "data/rules/srd-5.2.1/combat.rules") +
               "\ncreature recovery_target 1 1000 0 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n");
}

Character wizard()
{
    CharacterDraft draft;
    draft.race = "human";
    draft.gender = "female";
    draft.character_class = "wizard";
    draft.background = "sage";
    draft.alignment = "neutral_good";
    draft.name = "Arcane Recovery baseline";
    draft.rolled = true;
    for (auto &roll : draft.rolls)
        roll = {{6, 5, 4, 1}, 3};
    return Character(*srd5::character_rules(), draft, {});
}

void act(CombatSession &combat, std::string_view verb)
{
    for (const auto &command : combat.legal_commands())
        if (command.verb == verb)
        {
            check(combat.submit(command), "Accept baseline command");
            return;
        }
    throw std::runtime_error("Missing command: " + std::string(verb));
}

void next_player(CombatSession &combat)
{
    act(combat, "end");
    while (combat.snapshot().actor != 1)
        act(combat, "end");
}

void spend_slots(CampaignParty &party, CombatSession &combat)
{
    while (combat.snapshot().actor != 1)
        act(combat, "end");
    act(combat, "magic_missile");
    party.apply_combat(combat.snapshot());
    next_player(combat);
    act(combat, "magic_missile");
    party.apply_combat(combat.snapshot());
}

// A Wizard of the given level who spent slots in combat and then completed a
// Short Rest, so the party holds an open Short Rest spending ticket.
PartyState baseline(unsigned level)
{
    auto rules = module();
    CampaignParty party(module());
    const auto id = party.add_pc(wizard());
    party.award_experience(2700, "arcane-baseline");
    for (unsigned n = 2; n <= level; ++n)
        party.advance(id, party.default_advancement(id));

    auto participants = party.participants();
    participants.front().cell = {1, 1};
    participants.push_back({99, "recovery_target", "Target", 1, {5, 1}});
    auto combat = rules->create({{8, 8, std::vector<std::uint8_t>(64)}, participants}, 13);
    party.begin_combat();
    party.apply_combat(combat->snapshot());
    spend_slots(party, *combat);
    if (level >= 3)
    {
        next_player(*combat);
        act(*combat, "magic_missile_2");
        party.apply_combat(combat->snapshot());
    }

    if (level == 4)
    {
        next_player(*combat);
        act(*combat, "magic_missile");
        party.apply_combat(combat->snapshot());
    }

    party.end_combat();
    (void)party.rest(RestKind::short_rest);
    check(party.state().short_rest.has_value(), "Completed Short Rest retains spending ticket");
    return party.checkpoint();
}

void baseline_spending()
{
    for (unsigned level = 1; level <= 4; ++level)
    {
        CampaignParty party(module());
        party.restore(baseline(level));
        check(party.member(1).character.sheet().level == level && party.state().short_rest &&
              party.state().short_rest->members == std::vector<MemberId> {1},
              "Attained Wizard level and pending Short Rest eligibility survive");
        const auto resources = party.recovery_info(1).resources;
        for (const auto &pool : resources)
            if (pool.id == "spell_slot:1")
                check(pool.remaining == (level == 1   ? 0u
                                         : level == 2 ? 1u
                                         : level == 3 ? 2u
                                         : 1u),
                      "Spent first-level slots remain spent");
        for (const auto &pool : resources)
            if (pool.id == "spell_slot:2")
                check(pool.remaining == (level == 3 ? 1u : 2u),
                      "Spent second-level slot remains spent");
    }
}

unsigned remaining(const RecoveryInfo &info, std::string_view id)
{
    for (const auto &pool : info.resources)
        if (pool.id == id)
            return pool.remaining;
    throw std::runtime_error("Missing recovery pool: " + std::string(id));
}

template <class Action> void rejected(CampaignParty &party, Action action)
{
    const auto before = encode_campaign(party, nullptr, "arcane-test");
    bool caught = false;
    try
    {
        action();
    }
    catch (const std::exception &)
    {
        caught = true;
    }
    check(caught && encode_campaign(party, nullptr, "arcane-test") == before,
          "Rejected recovery is completely atomic");
}

void recovery_transactions()
{
    auto rules = module();

    struct Expected
    {
        const char *id;
        unsigned first, second;
    };

    const std::array expected{Expected{"arcane_recovery:1:0", 1, 0},
                              Expected{"arcane_recovery:2:0", 2, 0},
                              Expected{"arcane_recovery:0:1", 0, 1}};
    for (unsigned level = 1; level <= 4; ++level)
        for (const auto &choice : expected)
        {
            CampaignParty party(module());
            party.restore(baseline(level));
            const auto before = party.checkpoint();
            const auto ticket = before.short_rest->ticket;
            const auto info = party.recovery_info(1);
            check(
                remaining(info, "arcane_recovery") == 1 &&
                info.choices.size() == (level < 3 ? 1u : 3u),
                "Every attained Wizard level gains one use and the exact legal recovery combinations");
            rejected(party,
                     [&]
            {
                (void)party.recover_rest_choice({ticket.session, ticket.revision + 1}, 1,
                choice.id);
            });
            rejected(party,
                     [&]
            {
                (void)party.recover_rest_choice(ticket, 99, choice.id);
            });
            rejected(party,
                     [&]
            {
                (void)party.recover_rest_choice(ticket, 1, "");
            });
            rejected(party,
                     [&]
            {
                (void)party.recover_rest_choice(ticket, 1, "arcane_recovery:9:0");
            });
            if (level < 3 && (choice.first > 1 || choice.second))
            {
                rejected(party,
                         [&]
                {
                    (void)party.recover_rest_choice(ticket, 1, choice.id);
                });
                continue;
            }
            (void)party.recover_rest_choice(ticket, 1, choice.id);
            const auto after = party.recovery_info(1);
            check(
                remaining(after, "arcane_recovery") == 0 && after.choices.empty() &&
                remaining(after, "spell_slot:1") ==
                remaining(info, "spell_slot:1") + choice.first,
                "A legal nonempty choice restores exactly its first-level slots and spends the entire use");
            if (level >= 3)
                check(
                    remaining(after, "spell_slot:2") ==
                    remaining(info, "spell_slot:2") + choice.second,
                    "Second-level allocation is exact and mutually exclusive with first-level recovery");
            check(party.state().random_state == before.random_state &&
                  party.state().time_minutes == before.time_minutes &&
                  party.state().subminute_milliseconds == before.subminute_milliseconds &&
                  party.member(1).vitals.hit_points ==
                  before.roster.front().vitals.hit_points &&
                  party.member(1).equipped == before.roster.front().equipped &&
                  after.hit_dice == info.hit_dice,
                  "Recovery changes neither time/RNG nor wounds, gear or Hit Dice");
            rejected(party,
                     [&]
            {
                (void)party.recover_rest_choice(ticket, 1, choice.id);
            });
            rejected(party,
                     [&]
            {
                (void)party.recover_rest_choice(party.state().short_rest->ticket, 1,
                choice.id);
            });
            const auto saved = encode_campaign(party, nullptr, "arcane-test");
            CampaignParty copy(module());
            copy.restore(
                decode_campaign(saved, *srd5::character_rules(), *rules, "arcane-test", nullptr)
                .party);
            check(encode_campaign(copy, nullptr, "arcane-test") == saved &&
                  remaining(copy.recovery_info(1), "arcane_recovery") == 0,
                  "Pending rest save/reload preserves committed slots and spent Arcane Recovery");
            party.finish_short_rest(party.state().short_rest->ticket);
            (void)party.rest(RestKind::short_rest);
            check(remaining(party.recovery_info(1), "arcane_recovery") == 0,
                  "Another Short Rest never refreshes Arcane Recovery");
            party.finish_short_rest(party.state().short_rest->ticket);
            (void)party.rest(RestKind::long_rest);
            check(remaining(party.recovery_info(1), "arcane_recovery") == 1 &&
                  party.recovery_info(1).choices.empty(),
                  "Long Rest restores the use and full slots leave no recovery choice");
            rejected(party,
                     [&]
            {
                (void)party.recover_rest_choice(ticket, 1, choice.id);
            });
            party.keep_rest_spells(1);
            (void)party.rest(RestKind::short_rest);
            rejected(party,
                     [&]
            {
                (void)party.recover_rest_choice(party.state().short_rest->ticket, 1,
                choice.id);
            });
            check(remaining(party.recovery_info(1), "arcane_recovery") == 1,
                  "Full pools reject under a fresh valid rest ticket without spending the use");
        }
}

void eligibility_and_effects()
{
    auto rules = module();
    const auto features = rules->supported_features();
    check(std::find(features.begin(), features.end(), "arcane_recovery") != features.end(),
          "Module advertises the implemented capability");
    CampaignParty party(module());
    party.restore(baseline(3));
    const auto &sheet = party.member(1).character.sheet();
    auto state = party.member(1).vitals;
    const auto conscious_state = state;
    rules->set_hit_points(state, sheet, 0);
    check(rules->recovery_info(sheet, state).choices.empty(), "An unconscious Wizard cannot study");
    const auto unconscious = state.resources;
    bool caught = false;
    try
    {
        (void)rules->recover_rest_choice(state, sheet, "arcane_recovery:1:0");
    }
    catch (const std::exception &)
    {
        caught = true;
    }
    check(caught && state.resources == unconscious, "Unconscious recovery rejects atomically");
    state = conscious_state;
    (void)rules->recover_rest_choice(state, sheet, "arcane_recovery:1:0");
    check(state.resources.starts_with("SRD11 "), "Spent use has a versioned vital record");
    // A lasting effect must still expire when its host resource record is SRD11.
    srd5::detail::EffectState effects;
    srd5::detail::apply_ray_of_frost(effects, 1, 99, "Recovery test", 6000);
    std::ostringstream encoded;
    srd5::detail::write_effects(encoded, effects);
    const auto at = state.resources.find("FX");
    check(at != state.resources.npos, "Vital record contains effects");
    state.resources.replace(at, state.resources.size() - at, encoded.str());
    rules->validate_character_state(sheet, state);
    auto participants = party.participants();
    participants.front().state = state;
    std::uint64_t rng = 123;
    rules->elapse(participants, 6000, rng);
    const auto &elapsed = *participants.front().state;
    std::istringstream decoded(elapsed.resources.substr(elapsed.resources.find("FX")));
    check(srd5::detail::read_effects(decoded).active.empty() && rng == 123 &&
          remaining(rules->recovery_info(sheet, elapsed), "arcane_recovery") == 0,
          "Campaign time expires effects without refreshing the spent feature or consuming RNG");
    auto invalid = sheet;
    invalid.grants.push_back({"feature:arcane_recovery", "class:wizard", 1, {}});
    caught = false;
    try
    {
        (void)rules->character_profile(invalid, {});
    }
    catch (const std::exception &)
    {
        caught = true;
    }
    check(caught, "Duplicate feature grants are rejected");
}

void combat_and_advancement()
{
    auto rules = module();
    for (bool physical :
            {
                false, true
            })
    {
        CampaignParty party(module());
        party.restore(baseline(3));
        (void)party.recover_rest_choice(party.state().short_rest->ticket, 1, "arcane_recovery:1:0");
        party.finish_short_rest(party.state().short_rest->ticket);
        party.advance(1, party.default_advancement(1));
        check(party.member(1).character.sheet().level == 4 &&
              remaining(party.recovery_info(1), "arcane_recovery") == 0,
              "Ordinary advancement retains the spent use");
        if (physical)
        {
            por::Equipment dagger;
            dagger.stored.type = 8;
            dagger.stored.stack_size = 2;
            party.purchase(1, dagger);
        }
        auto participants = party.participants();
        participants.front().cell = {1, 1};
        participants.push_back({99, "recovery_target", "Target", 1, {5, 1}});
        auto combat = rules->create({{8, 8, std::vector<std::uint8_t>(64)}, participants}, 13);
        party.begin_combat();
        party.apply_combat(combat->snapshot());
        const auto before = combat->save();
        check(before.starts_with("OGCOMBAT 40 "),
              "Spent recovery uses a versioned combat checkpoint");
        auto copy = rules->restore(before);
        check(copy->save() == before && copy->snapshot().physical_inventory == physical,
              "New checkpoint preserves either ordinary or physical inventory mode exactly");
        for (auto *session :
                {
                    combat.get(), copy.get()
                })
        {
            while (session->snapshot().actor != 1)
                act(*session, "end");
            act(*session, "magic_missile");
        }
        check(combat->save() == copy->save(),
              "Reloaded combat preserves exact spell expenditure and RNG");
        party.apply_combat(combat->snapshot());
        party.end_combat();
        check(remaining(party.recovery_info(1), "arcane_recovery") == 0,
              "Combat handoff cannot refresh Arcane Recovery");
    }
    CampaignParty declined(module());
    declined.restore(baseline(1));
    declined.finish_short_rest(declined.state().short_rest->ticket);
    check(remaining(declined.recovery_info(1), "arcane_recovery") == 1,
          "Finishing without use preserves availability");
}
} // namespace

int main()
{
    try
    {
        baseline_spending();
        recovery_transactions();
        eligibility_and_effects();
        combat_and_advancement();
        std::cout << "Arcane Recovery checks passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
