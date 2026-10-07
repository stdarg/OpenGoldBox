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
                 std::vector<Participant> others, std::vector<std::string> gear = {})
{
    const auto profile = module.character_profile(hero, gear).data;
    Encounter e{{14, 8, std::vector<std::uint8_t>(112)},
        {{1, "campaign-character", "Hero", 0, {1, 1}, profile}}};
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
        const Participant ally{2, "healer", "Ally", 0, {1, 4}};
        const Participant near{98, "vanguard", "Enemy", 1, {6, 1}};
        const Participant far{99, "vanguard", "Far enemy", 1, {9, 3}};
        // Scenarios that play a second round have no monster ally: the game
        // has none, and the demo waits for a party monster's turn.
        write(*module, directory, "moonbeam", battle(*module, druid(3, "moonbeam"), {near, far}),
              "moonbeam");
        write(*module, directory, "spike-growth",
              battle(*module, druid(3, "spike_growth"), {near, far}), "spike_growth");
        write(*module, directory, "lands-aid",
              battle(*module, druid(3), {{2, "healer", "Ally", 0, {6, 2}, {}, VitalState{3, false, {}}},
                                          near
                                         }), "lands_aid");
        write(*module, directory, "produce-flame", battle(*module, druid(1), {ally, near}),
              "produce_flame");
        // An enemy in Chain Mail wears metal; monsters' armor is unknown (#231).
        const auto knight = module->character_profile(druid(4), std::vector<std::string> {"chain_mail"}).data;
        write(*module, directory, "heat-metal",
              battle(*module, druid(3, "heat_metal"),
        {{97, "campaign-character", "Knight", 1, {4, 1}, knight}}), "heat_metal");
        write(*module, directory, "flame-blade",
              battle(*module, druid(3, "flame_blade"), {ally, {98, "vanguard", "Enemy", 1, {2, 1}}}),
              "flame_blade");
        write(*module, directory, "bardic-inspiration",
              battle(*module, caster("bard", 5, {"vicious_mockery", "starry_wisp"},
        {"dissonant_whispers", "faerie_fire", "healing_word", "charm_person"}, 1),
        {ally, near}), "bardic_inspiration");
        std::cout << "Play-test fixtures written\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
