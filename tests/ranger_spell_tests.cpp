#include "opengold/campaign_party.h"
#include "opengold/character.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace opengold;
using namespace opengold::rules;

namespace
{
const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR);

void check(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}

std::string read(const std::filesystem::path &p)
{
    std::ifstream in(p);
    check(bool(in), "Read rules");
    return {std::istreambuf_iterator<char>(in), {}};
}

// A sturdy AC 1 target that hits back and a one-Hit-Point weakling.
std::unique_ptr<RulesModule> rules()
{
    return srd5::parse_content(read(root / "data/rules/srd-5.2.1/combat.rules") +
                               "\ncreature target 1 1000 0 30 20 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n" +
                               "creature weakling 1 1 0 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n" +
                               "creature brute 1 1000 0 30 20 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n" +
                               "affinity brute hide resistance slashing\n");
}

// A level-one Ranger who prepared the named spells; Hunter's Mark is always prepared.
Character ranger(std::vector<std::string> prepared)
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "male";
    d.character_class = "ranger";
    d.background = "soldier";
    d.alignment = "neutral_good";
    d.name = "Ranger";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    d.spells = SpellChoices{{}, std::move(prepared), {}, {}};
    return Character(*srd5::character_rules(), d, {});
}

CombatantView unit(const CombatSession &c, EntityId id)
{
    for (const auto &a : c.snapshot().combatants)
        if (a.id == id)
            return a;
    throw std::runtime_error("Missing combatant");
}

bool submit(CombatSession &c, std::string_view verb, EntityId target = 0)
{
    const auto offered = c.legal_commands();
    for (const auto &command : offered)
        if (command.verb == verb && (!target || command.target == target))
            return c.submit(command);
    return false;
}

bool logged(const CombatSession &c, std::string_view text)
{
    const auto log = c.snapshot().log;
    return std::any_of(log.begin(), log.end(), [&](const auto & line)
    {
        return line.find(text) != std::string::npos;
    });
}

// Level-one slots, read from the vital record "SRD11 winds slots ...".
int slots(const CombatSession &c)
{
    std::istringstream in(unit(c, 1).persistent.resources);
    std::string magic;
    int winds{}, level_one{};
    in >> magic >> winds >> level_one;
    return level_one;
}

unsigned remaining(const CombatSession &c, std::string_view pool)
{
    for (const auto &resource : unit(c, 1).resources)
        if (resource.id == pool)
            return resource.remaining;
    return 0;
}

// The Ranger (1) with a weakling (98) and a sturdy target (99) beside it.
std::unique_ptr<CombatSession> battle(const RulesModule &module, const Character &hero,
                                      std::string sturdy = "target", std::uint64_t seed = 5,
                                      std::string weapon = "longsword")
{
    const auto profile =
        module.character_profile(hero.sheet(), std::vector<std::string> {std::move(weapon)}).data;
    auto c = module.create({{8, 4, std::vector<std::uint8_t>(32)},
        {   {1, "campaign-character", "Ranger", 0, {1, 1}, profile},
            {98, "weakling", "Weakling", 1, {2, 1}},
            {99, std::move(sturdy), "Target", 1, {2, 2}}
        }},
    seed);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 6; ++turns)
        check(submit(*c, "end"), "Reach the Ranger's turn");
    check(c->snapshot().actor == 1, "The Ranger acts");
    return c;
}

// Ends the Ranger's turn and returns to it.
void next_turn(CombatSession &c)
{
    check(submit(c, "end"), "End the Ranger's turn");
    for (unsigned turns = 0; c.snapshot().actor != 1 && turns < 6; ++turns)
        check(submit(c, "end"), "Back to the Ranger");
}

void favored_enemy_checks()
{
    auto module = rules();
    auto c = battle(*module, ranger({"cure_wounds"}));
    check(remaining(*c, "favored_enemy") == 2 && slots(*c) == 2,
          "Favored Enemy: two free casts beside two slots");
    check(submit(*c, "hunters_mark_free", 99) && !unit(*c, 1).bonus_action &&
          unit(*c, 1).action && remaining(*c, "favored_enemy") == 1 &&
          slots(*c) == 2,
          "A free Hunter's Mark is a Bonus Action that spends no slot");
    const auto saved = c->save();
    check(module->restore(saved)->save() == saved, "The mark survives a checkpoint");
    next_turn(*c);
    check(!submit(*c, "hunters_mark_free", 98), "No free cast while the quarry still stands");
    check(submit(*c, "melee", 99) && logged(*c, "Force damage from Hunter's Mark"),
          "A hit on the quarry adds Hunter's Mark's 1d6 Force damage");
}

void move_mark_checks()
{
    auto module = rules();
    auto moved = battle(*module, ranger({"cure_wounds"}));
    check(submit(*moved, "hunters_mark_free", 98) && submit(*moved, "melee", 98), "Drop the quarry");
    next_turn(*moved);
    check(submit(*moved, "hunters_mark_move", 99) && logged(*moved, "Ranger moves Hunter's Mark to Target.") &&
          remaining(*moved, "favored_enemy") == 1,
          "A dropped quarry's mark moves for a Bonus Action without a new cast");
    check(submit(*moved, "melee", 99) && logged(*moved, "Target takes"),
          "The new quarry is marked");
}

void slot_cast_checks()
{
    auto module = rules();
    auto c = battle(*module, ranger({"cure_wounds"}));
    check(submit(*c, "hunters_mark", 99) && slots(*c) == 1 &&
          remaining(*c, "favored_enemy") == 2,
          "Hunter's Mark from a slot keeps the free casts");
}

void longstrider_checks()
{
    auto module = rules();
    auto c = battle(*module, ranger({"cure_wounds", "longstrider"}));
    const int before = unit(*c, 1).movement_feet;
    check(submit(*c, "longstrider", 1) && !unit(*c, 1).action &&
          unit(*c, 1).movement_feet == before + 10,
          "Longstrider adds 10 feet of Speed at once");
    next_turn(*c);
    check(unit(*c, 1).movement_feet == before + 10, "Longstrider lasts into later turns");
}
// A level-three Hunter with the chosen Hunter's Prey option.
Character hunter(std::string prey)
{
    CampaignParty party(srd5::load(root / "data/rules/srd-5.2.1/combat.rules"));
    const auto id = party.add_pc(ranger({"cure_wounds"}));
    party.award_experience(900, "hunter-xp");
    party.advance(id, party.default_advancement(id));
    auto third = party.default_advancement(id);
    third.training["subclass:ranger:hunter"] = {std::move(prey)};
    party.advance(id, third);
    return party.member(id).character;
}

// A Ranger with a vanguard 25 feet away on its turn, for
// tests/hunters_mark_view_tests.gd. The game loads it with the standard rules content.
void write_ui_fixture()
{
    auto module = srd5::load(root / "data/rules/srd-5.2.1/combat.rules");
    const auto profile = module->character_profile(ranger({"cure_wounds", "longstrider"}).sheet(),
                         std::vector<std::string> {"longsword"}).data;
    auto c = module->create({{12, 9, std::vector<std::uint8_t>(108)},
        {   {1, "campaign-character", "Ranger", 0, {1, 1}, profile},
            {99, "vanguard", "Enemy", 1, {6, 1}}
        }},
    2);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 4; ++turns)
        check(submit(*c, "end"), "Reach the Ranger's turn");
    const auto path = std::filesystem::path(OPENGOLD_BINARY_DIR) / "ranger-fixtures";
    std::filesystem::create_directories(path);
    std::ofstream out(path / "ranger.save", std::ios::binary);
    out << c->save();
    check(bool(out), "Write the UI fixture");

    // A Horde Breaker beside two adjacent vanguards.
    const auto hunter_profile = module->character_profile(hunter("horde_breaker").sheet(),
                                std::vector<std::string> {"longsword"}).data;
    auto h = module->create({{12, 9, std::vector<std::uint8_t>(108)},
        {   {1, "campaign-character", "Ranger", 0, {1, 1}, hunter_profile},
            {98, "vanguard", "First", 1, {2, 1}},
            {99, "vanguard", "Second", 1, {2, 2}}
        }},
    2);
    for (unsigned turns = 0; h->snapshot().actor != 1 && turns < 4; ++turns)
        check(submit(*h, "end"), "Reach the Hunter's turn");
    std::ofstream hunter_out(path / "hunter.save", std::ios::binary);
    hunter_out << h->save();
    check(bool(hunter_out), "Write the Hunter UI fixture");

    // A Ranger with Ensnaring Strike prepared, just after a hit on a vanguard.
    const auto ensnaring_profile =
        module->character_profile(ranger({"cure_wounds", "ensnaring_strike"}).sheet(),
                                  std::vector<std::string> {"longsword"}).data;
    for (std::uint64_t seed = 1; seed < 64; ++seed)
    {
        auto e = module->create({{12, 9, std::vector<std::uint8_t>(108)},
            {   {1, "campaign-character", "Ranger", 0, {1, 1}, ensnaring_profile},
                {99, "vanguard", "Target", 1, {2, 1}}
            }},
        seed);
        for (unsigned turns = 0; e->snapshot().actor != 1 && turns < 4; ++turns)
            check(submit(*e, "end"), "Reach the Ranger's turn");
        const auto offered = [&]
        {
            const auto commands = e->legal_commands();
            return std::any_of(commands.begin(), commands.end(), [](const auto & c)
            {
                return c.verb == "ensnaring_strike";
            });
        };
        if (!submit(*e, "melee", 99) || !offered())
            continue;
        std::ofstream ensnare_out(path / "ensnare.save", std::ios::binary);
        ensnare_out << e->save();
        check(bool(ensnare_out), "Write the Ensnaring Strike UI fixture");
        return;
    }
    throw std::runtime_error("No seed hits the vanguard");
}

// A Ranger with Entangle prepared and two vanguards 25 feet away, for
// tests/entangle_view_tests.gd.
void write_entangle_fixture()
{
    auto module = srd5::load(root / "data/rules/srd-5.2.1/combat.rules");
    const auto profile = module->character_profile(ranger({"cure_wounds", "entangle"}).sheet(),
                         std::vector<std::string> {"longsword"}).data;
    auto c = module->create({{12, 9, std::vector<std::uint8_t>(108)},
        {   {1, "campaign-character", "Ranger", 0, {1, 1}, profile},
            {98, "vanguard", "First", 1, {6, 1}},
            {99, "vanguard", "Second", 1, {6, 2}}
        }},
    2);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 4; ++turns)
        check(submit(*c, "end"), "Reach the Ranger's turn");
    const auto path = std::filesystem::path(OPENGOLD_BINARY_DIR) / "ranger-fixtures";
    std::ofstream out(path / "entangle.save", std::ios::binary);
    out << c->save();
    check(bool(out), "Write the Entangle UI fixture");
}
void hunters_prey_option_checks()
{
    auto module = rules();
    CampaignParty party(srd5::load(root / "data/rules/srd-5.2.1/combat.rules"));
    const auto id = party.add_pc(ranger({"cure_wounds"}));
    party.award_experience(900, "hunter-xp");
    party.advance(id, party.default_advancement(id));
    const auto options = module->advancement_options(party.member(id).character.sheet());
    check(options.training.size() == 1 && options.training.front().id == "subclass:ranger:hunter" &&
          options.training.front().options.size() == 2,
          "Level three chooses Hunter's Prey");
    auto bad = party.default_advancement(id);
    bad.training["subclass:ranger:hunter"] = {"giant_killer"};
    bool refused = false;
    try
    {
        party.advance(id, bad);
    }
    catch (const std::exception &)
    {
        refused = true;
    }
    check(refused && party.member(id).character.sheet().level == 2, "An unknown option is refused");
}

void colossus_slayer_checks()
{
    auto module = rules();
    auto c = battle(*module, hunter("colossus_slayer"));
    check(submit(*c, "melee", 99) && !logged(*c, "Colossus Slayer"),
          "Colossus Slayer waits for a wounded creature");
    next_turn(*c);
    check(submit(*c, "melee", 99) && logged(*c, "Colossus Slayer adds"),
          "Colossus Slayer adds 1d8 against a wounded creature");
    check(!submit(*c, "horde_breaker", 98), "Colossus Slayer brings no Horde Breaker");
}

void horde_breaker_checks()
{
    auto module = rules();
    auto c = battle(*module, hunter("horde_breaker"));
    check(!submit(*c, "horde_breaker", 98), "Horde Breaker follows a weapon attack");
    check(submit(*c, "melee", 99), "Attack the sturdy target");
    const auto saved = c->save();
    check(module->restore(saved)->save() == saved, "Horde Breaker's turn state survives a checkpoint");
    check(!submit(*c, "horde_breaker", 99), "Horde Breaker needs a different creature");
    check(submit(*c, "horde_breaker", 98) && logged(*c, "Ranger uses Horde Breaker."),
          "Horde Breaker attacks the creature beside the first target");
    check(!submit(*c, "horde_breaker", 98), "Horde Breaker is once per turn");
}

void hunters_lore_checks()
{
    auto module = rules();
    auto c = battle(*module, hunter("colossus_slayer"), "brute");
    check(submit(*c, "hunters_mark_free", 99) &&
          logged(*c, "Hunter's Lore: Target has Resistance to Slashing."),
          "Hunter's Lore reveals the quarry's Resistance");
    auto plain = battle(*module, ranger({"cure_wounds"}), "brute");
    check(submit(*plain, "hunters_mark_free", 99) && !logged(*plain, "Hunter's Lore"),
          "Hunter's Lore needs the Hunter");
}
// A battle in which the Ranger's Ensnaring Strike has Restrained the target
// (99): the first seed whose Strength save fails.
std::unique_ptr<CombatSession> ensnared(const RulesModule &module)
{
    for (std::uint64_t seed = 1; seed < 64; ++seed)
    {
        auto c = battle(module, ranger({"cure_wounds", "ensnaring_strike"}), "target", seed);
        if (submit(*c, "melee", 99) && submit(*c, "ensnaring_strike", 99) &&
                logged(*c, "Target is Restrained."))
            return c;
    }
    throw std::runtime_error("No seed fails the Strength save");
}

bool restrained(const CombatSession &c, EntityId id)
{
    const auto conditions = unit(c, id).conditions;
    return std::any_of(conditions.begin(), conditions.end(), [](const auto & m)
    {
        return m.source == "Restrained";
    });
}

void ensnaring_strike_checks()
{
    auto module = rules();
    auto plain = battle(*module, ranger({"cure_wounds", "ensnaring_strike"}));
    check(!submit(*plain, "ensnaring_strike", 99), "Ensnaring Strike follows a weapon hit");
    auto c = ensnared(*module);
    check(restrained(*c, 99) && slots(*c) == 1 && !unit(*c, 1).bonus_action,
          "Ensnaring Strike spends a slot and the Bonus Action and Restrains on a failed save");
    const auto saved = c->save();
    check(module->restore(saved)->save() == saved, "Restrained survives a checkpoint");
    check(submit(*c, "end"), "End the Ranger's turn");
    while (c->snapshot().actor != 99)
        check(submit(*c, "end"), "Reach the Restrained creature");
    check(logged(*c, "Target takes") && logged(*c, "Piercing damage from the vines") &&
          unit(*c, 99).movement_feet == 0,
          "The vines deal Piercing damage at the start of its turn and hold it in place");
    check(submit(*c, "melee", 1) && logged(*c, "(disadvantage)"),
          "A Restrained creature attacks with Disadvantage");
    next_turn(*c);
    if (restrained(*c, 99))
        check(submit(*c, "melee", 99) && logged(*c, "(advantage)"),
              "Attacks against a Restrained creature have Advantage");

    auto breaking = ensnared(*module);
    check(submit(*breaking, "end"), "End the Ranger's turn");
    while (breaking->snapshot().actor != 99)
        check(submit(*breaking, "end"), "Reach the Restrained creature");
    for (unsigned round = 0; round < 20 && restrained(*breaking, 99); ++round)
    {
        check(submit(*breaking, "escape", 99) && logged(*breaking, "Athletics check"),
              "The creature spends its Action on an Athletics check");
        if (!restrained(*breaking, 99))
            break;
        check(submit(*breaking, "end"), "End the creature's turn");
        while (breaking->snapshot().actor != 99)
            check(submit(*breaking, "end"), "Back to the creature");
    }
    check(!restrained(*breaking, 99) && logged(*breaking, "(escapes)"),
          "A successful check ends Ensnaring Strike");
}

void ranged_smite_window_checks()
{
    auto module = rules();
    // A longbow hit opens Ensnaring Strike; the target stands 10 feet away.
    for (std::uint64_t seed = 1; seed < 64; ++seed)
    {
        auto c = battle(*module, ranger({"cure_wounds", "ensnaring_strike"}), "target", seed,
                        "longbow");
        check(submit(*c, "ranged", 99), "The Ranger shoots");
        if (!logged(*c, "hits") && !logged(*c, "CRITICAL"))
            continue;
        check(submit(*c, "ensnaring_strike", 99), "Ensnaring Strike follows a Ranged weapon hit");
        return;
    }
    throw std::runtime_error("No seed hits with the longbow");
}
bool submit_cell(CombatSession &c, std::string_view verb, Cell cell)
{
    for (const auto &command : c.legal_commands())
        if (command.verb == verb && command.destination == cell)
            return c.submit(command);
    return false;
}

void entangle_aiming_checks()
{
    auto module = rules();
    auto c = battle(*module, ranger({"cure_wounds", "entangle"}));
    const auto offers = c->legal_commands();
    const auto start = std::find_if(offers.begin(), offers.end(), [](const auto & command)
    {
        return command.verb == "entangle";
    });
    check(start != offers.end() && start->aims_area && !start->target,
          "Entangle is offered once, to be aimed");
    check(c->submit(*start), "Choosing Entangle starts aiming");
    auto aim = c->snapshot().area_targeting;
    check(aim && aim->verb == "entangle" && aim->center == Cell{2, 1} && aim->cells.size() == 16 &&
          unit(*c, 1).action && slots(*c) == 2,
          "The preview starts on the nearest enemy, a 20-foot square, nothing spent");
    const auto aiming = c->save();
    check(module->restore(aiming)->save() == aiming, "Aiming survives a checkpoint");
    check(submit_cell(*c, "area_move", Cell{5, 2}) && c->snapshot().area_targeting->center == Cell{5, 2},
          "Moving the preview recentres the square");
    check(submit(*c, "spell_cancel") && !c->snapshot().area_targeting && unit(*c, 1).action &&
          slots(*c) == 2,
          "Cancelling spends nothing");
}

// A battle in which Entangle, aimed at the two enemies, Restrained at least
// one of them: the first seed where a Strength save fails.
std::unique_ptr<CombatSession> entangled(const RulesModule &module, std::size_t &reach_before)
{
    for (std::uint64_t seed = 1; seed < 64; ++seed)
    {
        auto c = battle(module, ranger({"cure_wounds", "entangle"}), "target", seed);
        reach_before = c->movement_reach(1).size();
        check(submit(*c, "entangle") && submit_cell(*c, "area_move", Cell{2, 1}) &&
              submit(*c, "area_cast"),
              "Aim and cast Entangle");
        if (restrained(*c, 98) || restrained(*c, 99))
            return c;
    }
    throw std::runtime_error("No seed fails a Strength save");
}

void entangle_checks()
{
    auto module = rules();
    std::size_t reach_before{};
    auto c = entangled(*module, reach_before);
    check(logged(*c, "Ranger casts Entangle.") && !unit(*c, 1).action && slots(*c) == 1 &&
          !restrained(*c, 1),
          "Entangle spends the Action and a slot and spares its caster");
    check(c->movement_reach(1).size() < reach_before &&
          c->snapshot().battlefield.at(Cell{2, 1}) == 2,
          "The square is Difficult Terrain, shown on the battlefield");
    const auto saved = c->save();
    check(module->restore(saved)->save() == saved, "The plants survive a checkpoint");
    // A new Concentration spell ends Entangle: the plants and the Restraint go.
    check(submit(*c, "hunters_mark_free", 99) && !restrained(*c, 98) && !restrained(*c, 99) &&
          c->movement_reach(1).size() == reach_before,
          "Losing Concentration removes the plants");
}

void fog_cloud_checks()
{
    auto module = rules();
    auto c = battle(*module, ranger({"cure_wounds", "fog_cloud"}));
    const auto clear = c->save();
    check(submit(*c, "hunters_mark_free", 99), "In clear air the Ranger can mark the target");
    c = module->restore(clear);
    check(submit(*c, "fog_cloud") && submit_cell(*c, "area_move", Cell{6, 2}) &&
          submit(*c, "area_cast") && logged(*c, "Ranger casts Fog Cloud.") && slots(*c) == 1,
          "Fog Cloud is aimed and cast");
    const auto obscured = c->snapshot().obscured;
    const auto inside = [&](Cell cell)
    {
        return std::find(obscured.begin(), obscured.end(), cell) != obscured.end();
    };
    check(inside(Cell{2, 2}) && inside(Cell{6, 2}) && !inside(Cell{1, 1}),
          "A 20-foot-radius sphere of squares is Heavily Obscured");
    check(!submit(*c, "hunters_mark_free", 99), "Spells that need sight cannot reach into the fog");
    const auto saved = c->save();
    check(module->restore(saved)->save() == saved, "The fog survives a checkpoint");
    next_turn(*c);
    check(submit(*c, "melee", 99) && !logged(*c, "(advantage)") && !logged(*c, "(disadvantage)"),
          "Neither side sees the other: Advantage and Disadvantage cancel");
}
} // namespace

int main()
{
    try
    {
        favored_enemy_checks();
        move_mark_checks();
        slot_cast_checks();
        longstrider_checks();
        hunters_prey_option_checks();
        colossus_slayer_checks();
        horde_breaker_checks();
        hunters_lore_checks();
        ensnaring_strike_checks();
        ranged_smite_window_checks();
        entangle_aiming_checks();
        entangle_checks();
        fog_cloud_checks();
        write_ui_fixture();
        write_entangle_fixture();
        std::cout << "Ranger spell tests passed\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
