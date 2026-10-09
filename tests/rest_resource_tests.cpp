#include "opengold/campaign_save.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace opengold;
using namespace opengold::rules;

namespace
{
void check(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}

template <class F> void rejects(F f)
{
    bool caught = false;
    try
    {
        f();
    }
    catch (const std::exception &)
    {
        caught = true;
    }
    check(caught, "Invalid recovery must reject");
}

auto module()
{
    return srd5::load(std::filesystem::path(OPENGOLD_SOURCE_DIR) /
                      "data/rules/srd-5.2.1/combat.rules");
}

Character hero(std::string klass = "fighter", unsigned level = 1, int constitution = 15)
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = klass;
    d.background = "soldier";
    d.alignment = "neutral_good";
    d.name = "Rest tester";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    if (constitution == 3)
        d.rolls[2] = {{1, 1, 1, 1}, 3};
    Character result(*srd5::character_rules(), d, {});
    VitalState scratch;
    for (unsigned n = 2; n <= level; ++n)
        check(result.advance(*module(), scratch), "Fixture level is supported");
    return result;
}

const ResourcePool &pool(const RecoveryInfo &info, std::string_view id)
{
    const auto it = std::find_if(info.resources.begin(), info.resources.end(),
                                 [&](const auto & p)
    {
        return p.id == id;
    });
    check(it != info.resources.end(), "Named pool exists");
    return *it;
}

Command command(const CombatSession &combat, std::string_view verb)
{
    for (const auto &c : combat.legal_commands())
        if (c.verb == verb)
            return c;
    throw std::runtime_error("Missing combat command");
}

void class_dice_and_recharge()
{
    auto rules = module();
    check(rules->short_rest_policy().duration_minutes == 60 &&
          rules->short_rest_policy().wait_after_rest_minutes == 0,
          "Short Rest lasts an hour without a once-per-day limit");
    check(rules->long_rest_policy().duration_minutes == 480 &&
          rules->long_rest_policy().wait_after_rest_minutes == 960,
          "Long Rest uses eight hours and the sixteen-hour starting cooldown");
    const std::map<std::string, unsigned> dice{{"barbarian", 12}, {"bard", 8},     {"cleric", 8},
        {"druid", 8},      {"fighter", 10}, {"monk", 8},
        {"paladin", 10},   {"ranger", 10},  {"rogue", 8},
        {"sorcerer", 6},   {"warlock", 8},  {"wizard", 6}};
    const std::map<unsigned, int> first_roll{{6, 2}, {8, 8}, {10, 6}, {12, 8}};
    const std::map<unsigned, int> first_healing{{6, 4}, {8, 9}, {10, 8}, {12, 10}};
    for (const auto &[klass, die] : dice)
    {
        const auto character = hero(klass);
        const auto &sheet = character.sheet();
        VitalState state{1, false, {}};
        const auto initial = state;
        auto info = rules->recovery_info(sheet, state);
        check(state == initial && info.can_rest && info.hit_die == die && info.hit_dice == 1 &&
              info.hit_dice_max == 1,
              "Each starting class has its correct unspent Hit Die and a pure resource query");
        std::uint64_t rng = 0;
        const auto spent = rules->spend_hit_die(state, sheet, rng);
        check(spent.die == die && spent.roll == first_roll.at(die) && spent.modifier == 2 &&
              spent.healing == first_healing.at(die) && spent.remaining == 0,
              "Golden first rolls heal by the class die plus Constitution, capped at maximum HP");
        check(state.hit_points == 1 + spent.healing && rng == 11400714819323198485ULL,
              "One spend consumes exactly one known RNG draw");
        check(state.resources.starts_with("SRD11 ") &&
              rules->recovery_info(sheet, state).hit_dice == 0,
              "Expenditure persists in the versioned continuation");
        const auto depleted = state;
        const auto used_rng = rng;
        rejects(
            [&]
        {
            rules->spend_hit_die(state, sheet, rng);
        });
        check(state == depleted && rng == used_rng, "Exhausted dice reject atomically");
        rules->recover_short_rest(state, sheet);
        check(rules->recovery_info(sheet, state).hit_dice == 0 &&
              state.hit_points == depleted.hit_points,
              "Short Rest does not restore Hit Dice or heal automatically");
        rules->recover(state, sheet);
        info = rules->recovery_info(sheet, state);
        check(info.hit_dice == 1 && state.hit_points == sheet.hit_points,
              "Long Rest restores all Hit Dice and HP for every class");
    }
    const auto fighter = hero("fighter", 4);
    const auto &sheet = fighter.sheet();
    VitalState state{1, false, "SRD11 0 0 0 0 0 0 4 0 0 0 \"\" 0 1 0 0 0 0 FX8 1 0 0"};
    auto info = rules->recovery_info(sheet, state);
    check(info.hit_dice == 4 && pool(info, "second_wind").capacity == 3 &&
          pool(info, "second_wind").short_rest_recovery == 1,
          "Level-four capacity is four dice and three Second Wind uses");
    rules->recover_short_rest(state, sheet);
    info = rules->recovery_info(sheet, state);
    check(state.hit_points == 1 && pool(info, "second_wind").remaining == 1,
          "A Short Rest returns exactly one spent Second Wind");
    for (unsigned n = 0; n < 4; ++n)
        rules->recover_short_rest(state, sheet);
    check(pool(rules->recovery_info(sheet, state), "second_wind").remaining == 3,
          "Repeated completed rests never exceed capacity");
    state = {1, false, "SRD11 0 0 0 0 0 0 4 0 0 0 \"\" 0 1 0 0 0 0 FX8 1 0 0"};
    std::uint64_t rng = 2;
    rules->spend_hit_die(state, sheet, rng);
    rules->spend_hit_die(state, sheet, rng);
    rules->recover(state, sheet);
    info = rules->recovery_info(sheet, state);
    check(info.hit_dice == 4 && pool(info, "second_wind").remaining == 3,
          "Long Rest restores every spent die and Second Wind use");
    for (const auto klass :
            {"cleric", "wizard"
            })
    {
        const auto caster = hero(klass, 4);
        VitalState slots{1, false, "SRD11 0 1 1 0 0 0 4 0 0 0 \"\" 0 0 0 0 0 0 FX8 1 0 0"};
        rules->recover_short_rest(slots, caster.sheet());
        info = rules->recovery_info(caster.sheet(), slots);
        check(pool(info, "spell_slot:1").remaining == 1 &&
              pool(info, "spell_slot:2").remaining == 1,
              "Ordinary caster slots do not recharge on a Short Rest");
        rules->recover(slots, caster.sheet());
        info = rules->recovery_info(caster.sheet(), slots);
        check(pool(info, "spell_slot:1").remaining == 4 &&
              pool(info, "spell_slot:2").remaining == 3,
              "Long Rest restores both supported slot pools");
    }
}

void minimum_caps_and_rejection()
{
    auto rules = module();
    auto character = hero("fighter", 1, 3);
    VitalState state{1, false, {}};
    std::uint64_t rng = 2;
    const auto result = rules->spend_hit_die(state, character.sheet(), rng);
    check(result.roll == 1 && result.modifier == -4 && result.healing == 1 && state.hit_points == 2,
          "A low roll with negative Constitution still restores one HP");
    auto dwarf = hero().creation_data();
    dwarf.race = "dwarf";
    const Character dwarven(*srd5::character_rules(), dwarf, {});
    state = {1, false, {}};
    rng = 42;
    check(rules->spend_hit_die(state, dwarven.sheet(), rng).healing == 6,
          "Dwarven Toughness affects maximum HP without adding to a Hit Die healing roll");
    character = hero();
    state = {character.sheet().hit_points - 1, false, {}};
    rng = 0;
    check(rules->spend_hit_die(state, character.sheet(), rng).healing == 1 &&
          state.hit_points == character.sheet().hit_points,
          "Hit Die healing stops at maximum HP");
    state = {character.sheet().hit_points, false, {}};
    rng = 0;
    check(rules->spend_hit_die(state, character.sheet(), rng).healing == 0 &&
          rules->recovery_info(character.sheet(), state).hit_dice == 0,
          "A voluntarily spent die is consumed even if no HP is missing");
    for (const auto dead :
            {
                false, true
            })
    {
        state = {0, dead,
                 dead ? "SRD11 0 0 0 0 3 0 1 0 0 0 \"\" 0 0 0 0 0 0 FX8 1 0 0"
                 : "SRD11 0 0 0 1 2 0 1 0 0 0 \"\" 0 0 0 0 0 0 FX8 1 0 0"
                };
        const auto before = state;
        rng = 42;
        check(!rules->recovery_info(character.sheet(), state).can_rest,
              "Unconscious and dead characters cannot start a rest");
        rejects(
            [&]
        {
            rules->spend_hit_die(state, character.sheet(), rng);
        });
        rejects(
            [&]
        {
            rules->recover_short_rest(state, character.sheet());
        });
        rejects(
            [&]
        {
            rules->recover(state, character.sheet());
        });
        check(state == before && rng == 42,
              "Rejected recovery never wakes, stabilizes or revives a character");
    }
    for (const auto malformed :
            {"SRD11 0 0 0 0 0 0 -1 0 0 0 \"\" 0 0 0 0 0 0 FX8 1 0 0",
             "SRD11 0 0 0 0 0 0 2 0 0 0 \"\" 0 0 0 0 0 0 FX8 1 0 0",
             "SRD11 0 0 0 0 0 0 0 0 0 0 \"\" 0 0 0 0 0 0",
             "SRD11 0 0 0 0 0 0 0 0 0 0 \"\" 0 0 0 0 0 0 FX8 1 0 0 junk",
             "SRD11 999 0 0 0 0 0 0 0 0 0 \"\" 0 0 0 0 0 0 FX8 1 0 0"
            })
    {
        state = {1, false, malformed};
        const auto before = state;
        rng = 42;
        rejects(
            [&]
        {
            (void)rules->recovery_info(character.sheet(), state);
        });
        rejects(
            [&]
        {
            rules->spend_hit_die(state, character.sheet(), rng);
        });
        rejects(
            [&]
        {
            rules->recover_short_rest(state, character.sheet());
        });
        rejects(
            [&]
        {
            rules->recover(state, character.sheet());
        });
        check(state == before && rng == 42,
              "Malformed counts, effects and resource pools reject without mutation");
    }
}

void persistence_and_advancement()
{
    auto rules = module();
    CampaignParty party(module());
    const auto id = party.add_pc(hero());
    auto state = party.checkpoint();
    state.roster[0].vitals = {1, false,
                              "SRD11 1 0 0 0 0 0 1 0 0 0 \"\" 0 0 0 0 0 0 "
                              "FX8 2 1 1 1 77 99 \"Source caster\" 13 43000 2000 0"
                             };
    const auto effect =
        state.roster[0].vitals.resources.substr(state.roster[0].vitals.resources.find("FX8"));
    rules->spend_hit_die(state.roster[0].vitals, state.roster[0].character.sheet(),
                         state.random_state);
    party.restore(state);
    check(party.member(id).vitals.resources.ends_with(effect),
          "Spending a die preserves active effects and recovery timers");
    const auto saved = encode_campaign(party, nullptr, "rest");
    auto loaded = decode_campaign(saved, *srd5::character_rules(), *rules, "rest", nullptr);
    CampaignParty restored(module());
    restored.restore(std::move(loaded.party));
    check(encode_campaign(restored, nullptr, "rest") == saved,
          "Campaign round trip retains the spent die and next RNG state exactly");
    auto bad_body = saved.substr(saved.find('\n', saved.find('\n') + 1) + 1);
    const std::string from = "SRD11 1 0 0 0 0 0 0 ";
    const auto where = bad_body.find(from);
    check(where != bad_body.npos, "Campaign fixture contains the spent die");
    bad_body.replace(where, from.size(), "SRD11 1 0 0 0 0 0 2 ");
    std::uint64_t checksum = 14695981039346656037ULL;
    for (unsigned char c : bad_body)
    {
        checksum ^= c;
        checksum *= 1099511628211ULL;
    }
    const auto header = saved.substr(0, saved.find('\n') + 1);
    rejects(
        [&]
    {
        (void)decode_campaign(header + std::to_string(checksum) + '\n' + bad_body,
        *srd5::character_rules(), *rules, "rest", nullptr);
    });
    check(
        encode_campaign(restored, nullptr, "rest") == saved,
        "An excessive die count rejects even with a correct checksum and cannot replace the campaign");
    party.award_experience(300, "rest-xp");
    party.advance(id, party.default_advancement(id));
    auto info = rules->recovery_info(party.member(id).character.sheet(), party.member(id).vitals);
    check(info.hit_dice_max == 2 && info.hit_dice == 1 && pool(info, "second_wind").remaining == 1,
          "Advancement adds only the newly acquired die and preserves earlier expenditure");
    const auto before_next_roll = encode_campaign(party, nullptr, "rest");
    auto next_load =
        decode_campaign(before_next_roll, *srd5::character_rules(), *rules, "rest", nullptr);
    auto first = party.member(id).vitals, second = next_load.party.roster[0].vitals;
    auto first_rng = party.state().random_state, second_rng = next_load.party.random_state;
    const auto next_die =
        rules->spend_hit_die(first, party.member(id).character.sheet(), first_rng);
    rules->spend_hit_die(second, next_load.party.roster[0].character.sheet(), second_rng);
    check(
        next_die.roll == 2 && first == second && first_rng == second_rng &&
        first_rng == 4354685564936845396ULL,
        "The next chosen die after save/reload consumes the known second RNG draw and gives the same healing");
    party.advance_time_milliseconds(1000);
    check(rules->recovery_info(party.member(id).character.sheet(), party.member(id).vitals)
          .hit_dice == 1 &&
          party.member(id).vitals.resources.find("42000 1000") != std::string::npos,
          "Effect elapsed time retains Hit Dice");
    auto healed = party.member(id).vitals;
    auto healing_rng = party.state().random_state;
    rules->temple_heal(healed, party.member(id).character.sheet(), healing_rng);
    check(rules->recovery_info(party.member(id).character.sheet(), healed).hit_dice == 1,
          "Spell healing preserves Hit Dice");
    auto members = party.participants();
    members[0].cell = {1, 1};
    members.push_back({99, "vanguard", "Enemy", 1, {5, 1}});
    auto combat = rules->create({{8, 8, std::vector<std::uint8_t>(64)}, members}, 42);
    const auto checkpoint = combat->save();
    check(checkpoint.starts_with("OGCOMBAT 47 ") &&
          rules->restore(checkpoint)->save() == checkpoint,
          "Combat checkpoint stores remaining dice exactly");
    auto copy = rules->restore(checkpoint);
    for (unsigned turn = 0; turn < 6; ++turn)
    {
        const auto end = command(*combat, "end");
        check(combat->submit(end) && copy->submit(end) && combat->save() == copy->save(),
              "Spent dice and effects continue deterministically through combat turns");
    }
    party.begin_combat();
    party.apply_combat(combat->snapshot());
    party.end_combat();
    check(rules->recovery_info(party.member(id).character.sheet(), party.member(id).vitals)
          .hit_dice == 1,
          "Campaign combat handoff preserves expenditure");
    // Corrupt only the appended count for the character; all other checkpoint data remains valid.
    std::istringstream in(checkpoint);
    std::vector<std::string> rows;
    for (std::string row; std::getline(in, row);)
        rows.push_back(row);
    for (unsigned i = 4; i < 6; ++i)
        if (rows[i].starts_with("1 "))
        {
            // Published actor field 26 is Hit Dice, before clocks and feature pools.
            std::istringstream fields(rows[i]);
            std::string field;
            for (unsigned n = 0; n < 26; ++n)
                fields >> std::quoted(field);
            fields >> std::ws;
            const auto begin = static_cast<std::size_t>(fields.tellg());
            fields >> field;
            check(field == "1", "Mutation targets the actual spent Hit Dice field");
            rows[i].replace(begin, static_cast<std::size_t>(fields.tellg()) - begin, "3");
        }
    std::string bad;
    for (const auto &row : rows)
        bad += row + '\n';
    rejects(
        [&]
    {
        (void)rules->restore(bad);
    });
    check(copy->save() == combat->save(), "Malformed Hit Dice cannot mutate an existing combat");
}
} // namespace

int main()
{
    try
    {
        class_dice_and_recharge();
        minimum_caps_and_rejection();
        persistence_and_advancement();
        std::cout << "Rest resource tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
