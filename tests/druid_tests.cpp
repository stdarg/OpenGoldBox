#include "opengold/campaign_party.h"
#include "opengold/character.h"
#include "opengold/combat_demo.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
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

// A sturdy AC 1 target that hits back.
std::unique_ptr<RulesModule> rules()
{
    return srd5::parse_content(read(root / "data/rules/srd-5.2.1/combat.rules") +
                               "\ncreature target 1 1000 0 30 20 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n");
}

// A Druid with Wisdom 18 and the given Primal Order, advanced to the given level.
Character druid(unsigned level = 1, std::string order = "warden")
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = "druid";
    d.background = "sage";
    d.alignment = "neutral_good";
    d.name = "Druid";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    d.rolls[4] = {{6, 6, 6, 1}, 3};
    d.training = {{"class:druid", {"nature", "medicine"}}, {"class:druid:primal_order", {order}}};
    d.cantrips = {"produce_flame", "shillelagh"};
    d.spells = SpellChoices{{},
        std::vector<std::string> {"cure_wounds", "entangle", "faerie_fire", "thunderwave"},
        {}, {}};
    CampaignParty party(srd5::load(root / "data/rules/srd-5.2.1/combat.rules"));
    const auto id = party.add_pc(Character(*srd5::character_rules(), d, {}));
    party.award_experience(2700, "druid-xp");
    for (unsigned n = 1; n < level; ++n)
        party.advance(id, party.default_advancement(id));
    return party.member(id).character;
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
    for (const auto &command : c.legal_commands())
        if (command.verb == verb && (!target || command.target == target))
            return c.submit(command);
    return false;
}

bool aim(CombatSession &c, Cell cell)
{
    for (const auto &command : c.legal_commands())
        if (command.verb == "area_move" && command.destination == cell)
            return c.submit(command);
    return false;
}

bool logged(const CombatSession &c, std::string_view text)
{
    const auto log = c.snapshot().log();
    return std::any_of(log.begin(), log.end(), [&](const auto & line)
    {
        return line.find(text) != std::string::npos;
    });
}

bool has_condition(const CombatSession &c, EntityId id, std::string_view label)
{
    const auto conditions = unit(c, id).conditions;
    return std::any_of(conditions.begin(), conditions.end(), [&](const auto & condition)
    {
        return condition.source == label;
    });
}

bool has_grant(const Character &hero, std::string_view id)
{
    const auto &grants = hero.sheet().grants;
    return std::any_of(grants.begin(), grants.end(), [&](const auto & g)
    {
        return g.id == id;
    });
}

// The Druid (1), an ally (2) and an enemy (98) beside the Druid or 15 feet away.
std::unique_ptr<CombatSession> battle(const RulesModule &module, const CharacterSheet &sheet,
                                      std::vector<std::string> gear = {}, bool adjacent = false,
                                      Cell ally = {1, 2}, std::uint64_t seed = 5)
{
    const auto profile = module.character_profile(sheet, gear).data;
    auto c = module.create({{12, 6, std::vector<Terrain>(72)},
        {   {1, "campaign-character", "Druid", 0, {1, 1}, profile},
            {2, "target", "Ally", 0, ally},
            {98, "target", "Enemy", 1, {adjacent ? 2 : 4, 1}}
        }},
    seed);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 4; ++turns)
        check(submit(*c, "end"), "Reach the Druid's turn");
    check(c->snapshot().actor == 1, "The Druid acts");
    return c;
}

// The bonus in the latest "d20 N + B vs AC" line.
int attack_bonus(const CombatSession &c)
{
    const auto log = c.snapshot().log();
    for (auto line = log.rbegin(); line != log.rend(); ++line)
        if (const auto at = line->find(" vs AC "); at != std::string::npos)
        {
            const auto plus = line->rfind(" + ", at);
            return std::stoi(line->substr(plus + 3, at - plus - 3));
        }
    throw std::runtime_error("No attack roll logged");
}

void cantrip_checks()
{
    auto module = rules();
    const auto sheet = druid().sheet();
    {
        auto c = battle(*module, sheet);
        check(submit(*c, "produce_flame") && has_condition(*c, 1, "Produce Flame") &&
              unit(*c, 1).action && !unit(*c, 1).bonus_action,
              "Produce Flame is a Bonus Action");
        check(submit(*c, "hurl_flame", 98) && logged(*c, "Druid hurls Produce Flame.") &&
              !unit(*c, 1).action && has_condition(*c, 1, "Produce Flame"),
              "Hurling the flame is an Action and the flame stays");
    }
    {
        auto c = battle(*module, sheet);
        const auto commands = c->legal_commands();
        check(std::none_of(commands.begin(), commands.end(),
                           [](const auto & command)
        {
            return command.verb == "shillelagh" || command.verb == "hurl_flame";
        }),
        "Shillelagh needs a Club or Quarterstaff; no flame, no hurl");
    }
    auto plain = battle(*module, sheet, {"quarterstaff"}, true);
    check(submit(*plain, "melee", 98), "A plain staff strike");
    const int strength_bonus = attack_bonus(*plain);
    auto c = battle(*module, sheet, {"quarterstaff"}, true);
    check(submit(*c, "shillelagh") && has_condition(*c, 1, "Shillelagh") && submit(*c, "melee", 98),
          "Shillelagh, then a staff strike");
    check(attack_bonus(*c) == strength_bonus + 2,
          "Shillelagh attacks with Wisdom (+4) instead of Strength (+2)");
}

void order_checks()
{
    auto module = rules();
    const auto warden = druid(1, "warden").sheet(), magician = druid(1, "magician").sheet();
    auto draft = CharacterDraft{};
    draft.character_class = "druid";
    draft.training["class:druid:primal_order"] = {"magician"};
    check(srd5::character_rules()->cantrip_options(draft).count == 3 &&
          module->spell_access(magician).cantrip_choices == 3 &&
          module->spell_access(warden).cantrip_choices == 2,
          "Magician learns one extra cantrip");
    const auto casts = [&](const CharacterSheet & sheet)
    {
        auto c = battle(*module, sheet, {"chain_shirt"});
        const auto commands = c->legal_commands();
        return std::any_of(commands.begin(), commands.end(), [](const auto & command)
        {
            return command.verb == "produce_flame";
        });
    };
    check(casts(warden) && !casts(magician), "Warden trains Medium armor; others cannot cast in it");
}

void circle_checks()
{
    auto module = rules();
    const auto third = druid(3), fourth = druid(4);
    check(has_grant(third, "subclass:land") && has_grant(third, "feature:circle_spells") &&
          has_grant(third, "land:arid"), "Level three brings the Circle of the Land");
    const auto always = module->spell_access(third.sheet()).always_prepared;
    check(always == std::vector<std::string> {"blur", "burning_hands", "fire_bolt"},
          "Arid Land's Circle Spells are always prepared");
    auto c = battle(*module, third.sheet());
    check(submit(*c, "fire_bolt", 98), "A Circle cantrip is cast");
    check(fourth.sheet().level == 4 && module->spell_access(fourth.sheet()).cantrip_choices == 3,
          "A level-four Druid knows a third cantrip");
}

void barkskin_checks()
{
    auto module = rules();
    auto sheet = druid(3).sheet();
    sheet.prepared_spells.back() = "barkskin";
    auto c = battle(*module, sheet);
    const int before = unit(*c, 2).armor_class;
    check(before < 17 && submit(*c, "barkskin", 2) && unit(*c, 2).armor_class == 17 &&
          unit(*c, 1).action, "Barkskin is a Bonus Action that raises AC to 17");
}

// A level-three Druid with `spell` prepared in place of its last prepared spell.
CharacterSheet preparing(std::string spell)
{
    auto sheet = druid(3).sheet();
    sheet.prepared_spells.back() = std::move(spell);
    return sheet;
}

std::size_t count_logged(const CombatSession &c, std::string_view text)
{
    const auto log = c.snapshot().log();
    return std::count_if(log.begin(), log.end(), [&](const auto & line)
    {
        return line.find(text) != std::string::npos;
    });
}

// Ends turns until the given creature acts.
void reach(CombatSession &c, EntityId id)
{
    check(submit(c, "end"), "End the turn");
    for (unsigned turns = 0; c.snapshot().actor != id && turns < 4; ++turns)
        check(submit(c, "end"), "End a turn");
}

bool move_to(CombatSession &c, Cell cell)
{
    for (const auto &command : c.legal_commands())
        if (command.verb == "move" && command.destination == cell)
            return c.submit(command);
    return false;
}

void flame_blade_checks()
{
    auto module = rules();
    auto c = battle(*module, preparing("flame_blade"), {}, true);
    check(!submit(*c, "flame_blade_strike", 98) && submit(*c, "flame_blade") &&
          has_condition(*c, 1, "Flame Blade") && unit(*c, 1).action,
          "Flame Blade is a Bonus Action that evokes the blade");
    check(submit(*c, "flame_blade_strike", 98) && !unit(*c, 1).action && logged(*c, "Druid -> Enemy"),
          "A melee spell attack with the blade is an Action");
}

void moonbeam_checks()
{
    auto module = rules();
    // The ally stands far enough from the Druid that a beam on it spares the Druid.
    auto c = battle(*module, preparing("moonbeam"), {}, false, Cell{1, 5});
    check(submit(*c, "moonbeam") && aim(*c, Cell{4, 1}) && submit(*c, "area_cast") &&
          c->snapshot().moonbeams.front() == Cell{4, 1},
          "Moonbeam shines on the enemy");
    const auto burned = [&]
    {
        return count_logged(*c, "Radiant damage from the Moonbeam.");
    };
    check(burned() == 1 && logged(*c, "Enemy Constitution save"), "The enemy saves when it appears");
    const auto saved = c->save();
    check(module->restore(saved)->save() == saved, "The beam survives a checkpoint");
    reach(*c, 98);
    check(move_to(*c, Cell{5, 1}) && burned() == 1, "Moving within the beam is not entering it");
    check(submit(*c, "end") && burned() == 2, "Ending its turn in the beam burns again");
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 4; ++turns)
        check(submit(*c, "end"), "Reach the Druid's turn");
    check(submit(*c, "move_moonbeam", 2) && !unit(*c, 1).action && burned() == 3 &&
          logged(*c, "Ally Constitution save"), "A Magic action moves the beam onto the ally");
}

void spike_growth_checks()
{
    auto module = rules();
    auto c = battle(*module, preparing("spike_growth"));
    check(submit(*c, "spike_growth") && aim(*c, Cell{4, 1}) && submit(*c, "area_cast") &&
          c->snapshot().battlefield.at(Cell{4, 1}) == Terrain::difficult,
          "Spike Growth makes Difficult Terrain");
    reach(*c, 98);
    check(move_to(*c, Cell{5, 1}) && count_logged(*c, "Piercing damage from the spikes.") == 1,
          "Each square moved within the spikes pierces");
}

void heat_metal_checks()
{
    auto module = rules();
    const auto caster = module->character_profile(preparing("heat_metal"),
                        std::vector<std::string> {}).data;
    // An enemy Druid in Chain Mail wears metal; the "target" monster has no
    // equipment row, so it wears none.
    const auto knight = module->character_profile(druid().sheet(),
                        std::vector<std::string> {"chain_mail"}).data;
    auto c = module->create({{12, 6, std::vector<Terrain>(72)},
        {   {1, "campaign-character", "Druid", 0, {1, 1}, caster},
            {2, "target", "Ally", 0, {1, 2}},
            {97, "campaign-character", "Knight", 1, {3, 1}, knight},
            {98, "target", "Enemy", 1, {4, 1}}
        }},
    5);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 6; ++turns)
        check(submit(*c, "end"), "Reach the Druid's turn");
    check(!submit(*c, "heat_metal", 98), "A monster without an equipment row wears no metal");
    check(submit(*c, "heat_metal", 97) && logged(*c, "Fire damage from the hot metal.") &&
          has_condition(*c, 97, "Heated (Heat Metal)"), "Heat Metal burns the Knight in Chain Mail");
    check(!submit(*c, "heat_metal_again", 97), "Not again on the casting turn");
    reach(*c, 1);
    check(submit(*c, "heat_metal_again", 97) && !unit(*c, 1).bonus_action &&
          count_logged(*c, "Fire damage from the hot metal.") == 2,
          "A Bonus Action on a later turn heats it again");
}

unsigned wild_shapes_left(const CombatSession &c)
{
    for (const auto &pool : unit(c, 1).resources)
        if (pool.id == "wild_shape")
            return pool.remaining;
    throw std::runtime_error("No Wild Shape pool");
}

// combat.rules' equipment rows (#231): the orc leader readies chain mail, the
// orc no armor.
void creature_equipment_checks()
{
    auto module = rules();
    const auto caster = module->character_profile(preparing("heat_metal"),
                        std::vector<std::string> {}).data;
    auto c = module->create({{12, 6, std::vector<Terrain>(72)},
        {   {1, "campaign-character", "Druid", 0, {1, 1}, caster},
            {96, "slums-orc-leader", "Orc Leader", 1, {5, 1}},
            {95, "slums-orc", "Orc", 1, {5, 3}}
        }},
    5);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 6; ++turns)
        check(submit(*c, "end"), "Reach the Druid's turn");
    const auto commands = c->legal_commands();
    const auto heats = [&](EntityId target)
    {
        return std::any_of(commands.begin(), commands.end(), [&](const auto & command)
        {
            return command.verb == "heat_metal" && command.target == target;
        });
    };
    check(heats(96) && !heats(95), "Heat Metal targets the orc leader in chain mail, not the orc");
    const auto base = read(root / "data/rules/srd-5.2.1/combat.rules");
    for (const auto *bad :
            {"equipment slums-orc metal_armor", "equipment nobody metal_armor MON2CHA.DAX:4 chain_mail",
             "equipment slums-orc shield MON2CHA.DAX:4 shield",
             "equipment slums-orc-leader metal_armor MON2CHA.DAX:5 chain_mail",
             "equipment slums-orc metal_armor MON2CHA.DAX:4 chain_mail extra"
            })
    {
        bool rejected = false;
        try
        {
            (void)srd5::parse_content(base + "\n" + bad + "\n");
        }
        catch (const std::exception &)
        {
            rejected = true;
        }
        check(rejected, ("Invalid equipment row rejected: " + std::string(bad)).c_str());
    }
}

void wild_shape_checks()
{
    auto module = rules();
    check(has_grant(druid(2), "feature:wild_shape") && !has_grant(druid(1), "feature:wild_shape"),
          "Level two brings Wild Shape");
    auto c = battle(*module, druid(2).sheet(), {}, true);
    const int druid_ac = unit(*c, 1).armor_class;
    check(wild_shapes_left(*c) == 2 && submit(*c, "wild_shape_wolf") && wild_shapes_left(*c) == 1 &&
          unit(*c, 1).form == "wolf" && has_condition(*c, 1, "Wild Shape ({form})") &&
          unit(*c, 1).armor_class == 12 && unit(*c, 1).temporary_hp.amount == 2 &&
          !unit(*c, 1).bonus_action && unit(*c, 1).action,
          "A Bonus Action takes the Wolf's form with Temporary HP equal to the Druid level");
    const auto commands = c->legal_commands();
    check(std::none_of(commands.begin(), commands.end(), [](const auto & command)
    {
        return command.verb == "produce_flame" || command.verb == "cure_wounds";
    }), "A Beast form casts no spells");
    const auto saved = c->save();
    check(module->restore(saved)->snapshot().combatants.front().form == "wolf",
          "The form survives a checkpoint");
    check(submit(*c, "melee", 98) && logged(*c, "Druid -> Enemy: d20") && logged(*c, " + 4 vs AC"),
          "The Wolf bites at +4");
    // The enemy's AC is 1, so the bite hits.
    check(unit(*c, 98).prone && logged(*c, "Enemy is knocked Prone."),
          "The Wolf's bite knocks a Medium creature Prone");
    reach(*c, 1);
    check(submit(*c, "leave_wild_shape") && unit(*c, 1).form.empty() &&
          unit(*c, 1).armor_class == druid_ac && logged(*c, "Druid leaves Wild Shape."),
          "A Bonus Action leaves the form");
}

// A creature that the Wolf's opportunity bite knocks Prone must crawl; when it
// cannot crawl the rest of its route, it stops where it is and the fight goes on.
void prone_mover_checks()
{
    auto module = rules();
    bool covered = false;
    for (std::uint64_t seed = 1; seed < 40 && !covered; ++seed)
    {
        auto c = battle(*module, druid(2).sheet(), {}, true, Cell{1, 5}, seed);
        check(submit(*c, "wild_shape_wolf"), "Take the Wolf's form");
        reach(*c, 98);
        check(move_to(*c, Cell{8, 1}) && c->snapshot().reaction_pending,
              "Moving away from the Wolf provokes its bite");
        check(submit(*c, "opportunity"), "The Wolf bites");
        if (!unit(*c, 98).prone)
            continue;
        covered = true;
        const auto mover = unit(*c, 98);
        check(mover.cell.x < 8 && c->snapshot().outcome == Outcome::ongoing &&
              c->snapshot().actor == 98,
              "Knocked Prone mid-route, the mover stops short and keeps its turn");
        check(submit(*c, "end"), "The fight goes on");
    }
    check(covered, "A bite knocked the mover Prone");
}

void lands_aid_checks()
{
    auto module = rules();
    const auto profile = module->character_profile(druid(3).sheet(), std::vector<std::string> {}).data;
    auto c = module->create({{12, 6, std::vector<Terrain>(72)},
        {   {1, "campaign-character", "Druid", 0, {1, 1}, profile},
            {2, "target", "Ally", 0, {4, 2}, {}, VitalState{3, false, {}}},
            {98, "target", "Enemy", 1, {4, 1}}
        }},
    5);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 4; ++turns)
        check(submit(*c, "end"), "Reach the Druid's turn");
    check(submit(*c, "lands_aid") && aim(*c, Cell{4, 1}) && submit(*c, "area_cast") &&
          logged(*c, "Druid uses Land's Aid.") && logged(*c, "Enemy takes") &&
          logged(*c, "Necrotic damage from the thorns.") && unit(*c, 2).hit_points > 3 &&
          !unit(*c, 1).action && wild_shapes_left(*c) == 1,
          "Land's Aid spends a Wild Shape use: thorns for the enemy, flowers for the ally");
}

// The automated policy aims Moonbeam at an enemy clear of allies, then fights
// as a Wolf while it holds the beam.
void policy_checks()
{
    auto module = rules();
    auto c = battle(*module, preparing("moonbeam"), {}, false, Cell{1, 5});
    check(choose_demo_command(*c).verb == "produce_flame" && c->submit(choose_demo_command(*c)),
          "The policy lights Produce Flame with its Bonus Action");
    check(choose_demo_command(*c).verb == "moonbeam", "Then it casts Moonbeam");
    for (unsigned n = 0; n < 4 && c->snapshot().moonbeams.empty(); ++n)
        check(c->submit(choose_demo_command(*c)), "Aim and cast Moonbeam");
    check(c->snapshot().moonbeams.front() == Cell{4, 1} && has_condition(*c, 1, "Concentrating"),
          "The beam shines on the enemy while the Druid concentrates");
    // Produce Flame spent this turn's Bonus Action.
    reach(*c, 1);
    check(choose_demo_command(*c).verb == "wild_shape_wolf",
          "On its next turn it takes the Wolf's form");
}

} // namespace

int main()
{
    try
    {
        cantrip_checks();
        order_checks();
        circle_checks();
        barkskin_checks();
        flame_blade_checks();
        moonbeam_checks();
        spike_growth_checks();
        heat_metal_checks();
        creature_equipment_checks();
        wild_shape_checks();
        prone_mover_checks();
        lands_aid_checks();
        policy_checks();
        std::cout << "Druid tests passed\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
