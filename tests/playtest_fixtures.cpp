// Writes combat saves for the play-test of targeted and aimed actions
// (tests/playtest_actions.gd), and checks each scenario offers the action it
// exercises. See docs/COMBAT-DEMO.md.
#include "opengold/campaign_party.h"
#include "opengold/character.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace opengold;
using namespace opengold::rules;

namespace
{
const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR);

void check(bool ok, const std::string &message)
{
    if (!ok)
        throw std::runtime_error(message);
}

std::unique_ptr<RulesModule> module_rules()
{
    return srd5::load(root / "data/rules/srd-5.2.1/combat.rules");
}

// A caster with its casting ability at 18, advanced to `level`, with
// `spell` prepared in place of its last prepared spell.
CharacterSheet caster(std::string klass, unsigned ability, std::vector<std::string> cantrips,
                      std::vector<std::string> prepared, unsigned level, std::string spell = {})
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = klass;
    d.background = "sage";
    d.alignment = "neutral_good";
    d.name = klass == "druid" ? "Druid" : klass == "bard" ? "Bard" : "Wizard";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    d.rolls[ability] = {{6, 6, 6, 1}, 3};
    if (klass == "druid")
        d.training = {{"class:druid", {"nature", "medicine"}}, {"class:druid:primal_order", {"warden"}}};
    if (klass == "bard")
        d.training = {{"class:bard", {"performance", "persuasion", "history"}}};
    if (klass == "wizard")
        d.training = {{"class:wizard", {"arcana", "history"}}};
    d.cantrips = std::move(cantrips);
    d.spells = SpellChoices{{}, std::move(prepared), {}, {}};
    CampaignParty party(module_rules());
    const auto id = party.add_pc(Character(*srd5::character_rules(), d, {}));
    party.award_experience(2700, "playtest-xp");
    for (unsigned n = 1; n < level; ++n)
        party.advance(id, party.default_advancement(id));
    auto sheet = party.member(id).character.sheet();
    if (!spell.empty())
        sheet.prepared_spells.back() = std::move(spell);
    return sheet;
}

// A level-four Fighter for the gear scenarios.
CharacterSheet fighter()
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "male";
    d.character_class = "fighter";
    d.background = "soldier";
    d.alignment = "neutral_good";
    d.name = "Fighter";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    d.rolls[0] = {{6, 6, 6, 1}, 3};
    CampaignParty party(module_rules());
    const auto id = party.add_pc(Character(*srd5::character_rules(), d, {}));
    party.award_experience(2700, "playtest-xp");
    for (unsigned n = 1; n < 4; ++n)
        party.advance(id, party.default_advancement(id));
    return party.member(id).character.sheet();
}

// A level-two Monk: Unarmored Movement makes it faster than any Slums monster.
CharacterSheet monk()
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = "monk";
    d.background = "sage";
    d.alignment = "neutral_good";
    d.name = "Monk";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    d.rolls[1] = {{6, 6, 6, 1}, 3};
    d.training = {{"class:monk", {"acrobatics", "stealth"}}};
    CampaignParty party(module_rules());
    const auto id = party.add_pc(Character(*srd5::character_rules(), d, {}));
    party.award_experience(2700, "playtest-xp");
    party.advance(id, party.default_advancement(id));
    return party.member(id).character.sheet();
}

CharacterSheet druid(unsigned level, std::string spell = {})
{
    return caster("druid", 4, {"produce_flame", "shillelagh"},
    {"cure_wounds", "entangle", "faerie_fire", "thunderwave"}, level, std::move(spell));
}

// Walks the battle to the party's first character and writes it.
void write(const RulesModule &module, const std::filesystem::path &directory,
           const std::string &name, Encounter encounter, std::string_view verb,
           std::uint64_t seed = 6)
{
    auto c = module.create(std::move(encounter), seed);
    for (unsigned n = 0; c->snapshot().actor != 1 && n < 8; ++n)
    {
        const auto commands = c->legal_commands();
        const auto keep = std::find_if(commands.begin(), commands.end(), [](const auto & command)
        {
            return command.verb == "initiative_keep" || command.verb == "end";
        });
        check(keep != commands.end() && c->submit(*keep), name + ": reach the first character");
    }
    const auto commands = c->legal_commands();
    check(c->snapshot().actor == 1 &&
          std::any_of(commands.begin(), commands.end(), [&](const auto & command)
    {
        return command.verb == verb;
    }), name + ": offers " + std::string(verb));
    std::ofstream(directory / (name + ".save"), std::ios::binary) << c->save();
}

Encounter battle(const RulesModule &module, const CharacterSheet &hero,
                 std::vector<Participant> others, std::vector<std::string> gear = {},
                 std::vector<CarriedEquipment> carried = {})
{
    const auto profile = module.character_profile(hero, gear).data;
    Encounter e{{14, 8, std::vector<Terrain>(112)},
        {{1, "campaign-character", "Hero", Side::party, {1, 1}, profile}}};
    e.participants.front().inventory = std::move(carried);
    e.participants.insert(e.participants.end(), others.begin(), others.end());
    return e;
}
} // namespace

int main()
{
    try
    {
        const auto directory = std::filesystem::path(OPENGOLD_BINARY_DIR) / "playtest-fixtures";
        std::filesystem::create_directories(directory);
        auto module = module_rules();
        const Participant ally{2, "healer", "Ally", Side::party, {1, 4}};
        const Participant near{98, "vanguard", "Enemy", Side::opposition, {6, 1}};
        const Participant far{99, "vanguard", "Far enemy", Side::opposition, {9, 3}};
        // Scenarios that play a second round have no monster ally: the game
        // has none, and the demo waits for a party monster's turn.
        write(*module, directory, "moonbeam", battle(*module, druid(3, "moonbeam"), {near, far}),
              "moonbeam");
        write(*module, directory, "spike-growth",
              battle(*module, druid(3, "spike_growth"), {near, far}), "spike_growth");
        write(*module, directory, "lands-aid",
              battle(*module, druid(3), {{2, "healer", "Ally", Side::party, {6, 2}, {},
                                              VitalState{3, false, {}}},
                                          near
                                         }), "lands_aid");
        write(*module, directory, "produce-flame", battle(*module, druid(1), {ally, near}),
              "produce_flame");
        // An enemy in Chain Mail wears metal; monsters' armor is unknown (#231).
        const auto knight = module->character_profile(druid(4), std::vector<std::string> {"chain_mail"}).data;
        write(*module, directory, "heat-metal",
              battle(*module, druid(3, "heat_metal"),
        {{97, "campaign-character", "Knight", Side::opposition, {4, 1}, knight}}), "heat_metal");
        write(*module, directory, "flame-blade",
              battle(*module, druid(3, "flame_blade"),
                     {ally, {98, "vanguard", "Enemy", Side::opposition, {2, 1}}}),
              "flame_blade");
        write(*module, directory, "bardic-inspiration",
              battle(*module, caster("bard", 5, {"vicious_mockery", "starry_wisp"},
        {"dissonant_whispers", "faerie_fire", "healing_word", "charm_person"}, 1),
        {ally, near}), "bardic_inspiration");
        // Gear against a troll: a sword-and-shield Fighter carrying a Torch, a
        // longbow and one flask of each kind.
        const std::vector<std::string> sword_and_shield{"longsword", "shield", "chain_mail"};
        const std::vector<CarriedEquipment> pack{{11, "torch", 1, -1}, {12, "longbow", 1, -1},
            {13, "arrow", 20, -1}, {14, "oil", 1, -1}, {15, "alchemists_fire", 1, -1},
            {16, "acid", 1, -1}
        };
        const Participant troll_beside{98, "troll", "Troll", Side::opposition, {2, 1}};
        const Participant troll_near{98, "troll", "Troll", Side::opposition, {4, 1}};
        const Participant troll_far{98, "troll", "Troll", Side::opposition, {11, 5}};
        // The hero on the field's west edge, a same-speed enemy far away: a
        // step west tries to flee.
        {
            auto edge = battle(*module, fighter(),
                               {{98, "vanguard", "Enemy", Side::opposition, {12, 6}}});
            edge.participants.front().cell = {0, 1};
            write(*module, directory, "flee", std::move(edge), "flee");
        }
        // Kobolds whose encounter morale breaks at the first wound to their
        // side: beside a Fighter as fast as they are, they flee in panic;
        // beside a faster Monk, they surrender.
        const std::vector<Participant> kobolds{
            {98, "slums-kobold", "Kobold", Side::opposition, {2, 1}},
            {97, "slums-kobold", "Kobold 2", Side::opposition, {6, 3}},
            {96, "slums-kobold", "Kobold 3", Side::opposition, {7, 5}}
        };
        {
            auto fight = battle(*module, fighter(), kobolds, sword_and_shield);
            fight.morale = 1;
            write(*module, directory, "morale-panic", std::move(fight), "melee");
        }
        {
            auto fight = battle(*module, monk(), kobolds);
            fight.morale = 1;
            write(*module, directory, "morale-surrender", std::move(fight), "melee");
        }
        // A dying Fighter ally when the last enemy, a wounded Kobold, falls.
        {
            const auto ally = module->character_profile(fighter(), sword_and_shield).data;
            write(*module, directory, "bandage",
                  battle(*module, fighter(),
            {   {2, "campaign-character", "Ally", Side::party, {1, 3}, ally,
                    VitalState{0, false, "SRD11 0 0 0 0 0 0 1 6000 0 0 \"\" 0 0 0 0 0 0 FX8 1 0 1"}},
                {98, "slums-kobold", "Kobold", Side::opposition, {2, 1}, {}, VitalState{1}}
            }, sword_and_shield), "melee");
        }
        write(*module, directory, "gear-torch",
              battle(*module, fighter(), {troll_beside}, sword_and_shield, pack), "torch");
        // The same with a party ally, to click on before acting.
        write(*module, directory, "gear-ally",
              battle(*module, fighter(), {{2, "healer", "Ally", Side::party, {1, 3}}, troll_beside},
                     sword_and_shield, pack), "torch");
        write(*module, directory, "gear-flasks",
              battle(*module, fighter(), {troll_near}, sword_and_shield, pack), "throw_oil");
        write(*module, directory, "gear-bow",
              battle(*module, fighter(), {troll_far}, sword_and_shield, pack), "doff_shield");
        std::cout << "Play-test fixtures written\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
