// Characterization snapshot for the spell offer set, plus spell-table
// consistency checks.
//
// The eight per-spell test targets already pin resolution precisely (exact
// dice, saves, RNG consumption and typed defenses). What no test covered was
// the *set* of offers available in a given position, which is what a
// table-driven rewrite of legal_commands() can silently change. This file
// records that set across casters, distances and gear so the rewrite has to
// reproduce it byte for byte.
#include "campaign_fixture.h"
#include "opengold/campaign_save.h"
#include "opengold/character_pool.h"
#include "opengold/srd5.h"
#include "spell_components.h"
#include "spell_table.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace opengold;
using namespace opengold::rules;

namespace {
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR);
std::string read(const std::filesystem::path& p) {
    std::ifstream in(p); check(bool(in), "Read file"); return {std::istreambuf_iterator<char>(in), {}};
}
auto module() { return srd5::load(root / "data/rules/srd-5.2.1/combat.rules"); }
// Same high-HP inert target the per-spell tests use, so offers are never
// suppressed by a target dying mid-snapshot.
auto custom() {
    return srd5::parse_content(read(root / "data/rules/srd-5.2.1/combat.rules") +
        "\ncreature target 1 1000 0 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\nsaves target 0 0 0 0 0 0\n");
}

Character hero(std::string klass, unsigned level, std::vector<std::string> cantrips) {
    CharacterDraft d;
    d.race = "orc"; d.gender = "female"; d.character_class = klass; d.background = "sage";
    d.alignment = "neutral_good"; d.name = "Table caster"; d.rolled = true;
    for (auto& r : d.rolls) r = {{6, 5, 4, 1}, 3};
    if (!cantrips.empty()) d.cantrips = cantrips;
    Character h(*srd5::character_rules(), d, {});
    VitalState scratch;
    for (unsigned n = 1; n < level; ++n) check(h.advance(*module(), scratch), "Ordinary advancement");
    return h;
}

struct Caster { std::string label; CharacterSheet sheet; std::vector<std::string> gear; };

std::vector<Caster> casters() {
    std::vector<Caster> result;
    auto add = [&](std::string label, CharacterSheet sheet, std::vector<std::string> gear = {}) {
        result.push_back({std::move(label), std::move(sheet), std::move(gear)});
    };
    add("wizard-1", hero("wizard", 1, {"fire_bolt", "poison_spray", "ray_of_frost"}).sheet());
    add("wizard-4", hero("wizard", 4, {"shocking_grasp", "chill_touch", "fire_bolt"}).sheet());
    {   // Cleric prepared lists are set directly on the sheet, as in chill_touch_tests.
        auto s = hero("cleric", 1, {"sacred_flame"}).sheet();
        s.prepared_spells = {"cure_wounds"};
        add("cleric-1", s);
    }
    {
        auto s = hero("cleric", 4, {"sacred_flame"}).sheet();
        s.prepared_spells = {"cure_wounds", "healing_word", "blindness"};
        add("cleric-4", s);
    }
    add("warlock-1", hero("warlock", 1, {"eldritch_blast", "poison_spray"}).sheet());
    add("sorcerer-1", hero("sorcerer", 1, {"fire_bolt", "poison_spray", "ray_of_frost", "shocking_grasp"}).sheet());
    // Gear variants exercise the somatic-hand and untrained-armor gates that
    // suppress every spell offer.
    add("wizard-4+shield", hero("wizard", 4, {"shocking_grasp", "chill_touch", "fire_bolt"}).sheet(), {"quarterstaff", "shield"});
    add("wizard-4+plate", hero("wizard", 4, {"shocking_grasp", "chill_touch", "fire_bolt"}).sheet(), {"plate"});
    return result;
}

// Every offer except movement, sorted, so the snapshot is order-independent at
// the file level while P04 still has to reproduce the exact membership.
std::string offers(const CombatSession& c) {
    std::set<std::string> lines;
    for (const auto& command : c.legal_commands()) {
        if (command.verb == "move") continue;
        lines.insert(command.verb + "|" + std::to_string(command.target));
    }
    std::string out;
    for (const auto& line : lines) out += "    " + line + "\n";
    if (out.empty()) out = "    (none)\n";
    return out;
}

std::string snapshot() {
    auto rules = custom();
    std::string out = "# Spell offer characterization. Caster at (1,1); wounded ally and\n"
                      "# enemy both at exactly the listed distance (Chebyshev x 5).\n";
    for (const auto& caster : casters()) {
        const auto profile = rules->character_profile(caster.sheet, caster.gear);
        for (int feet : {5, 30, 60, 120, 125}) {
            const int column = 1 + feet / 5;
            auto c = rules->create({{28, 4, std::vector<std::uint8_t>(28 * 4)},
                {{1, "campaign-character", "Caster", 0, {1, 1}, profile.data},
                 {2, "target", "Ally", 0, {column, 1}, {}, VitalState{3, false, {}}},
                 {3, "target", "Enemy", 1, {column, 2}}}}, 13);
            // Seed 13 does not always start the caster; advance to their turn.
            for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 4; ++turns) {
                bool ended = false;
                for (const auto& command : c->legal_commands())
                    if (command.verb == "end") { ended = c->submit(command); break; }
                check(ended, "Reach the caster's turn");
            }
            check(c->snapshot().actor == 1, "Caster acts");
            out += caster.label + " @ " + std::to_string(feet) + "ft\n" + offers(*c);
        }
    }
    return out;
}

// ---- table consistency (Phase 1) ----------------------------------------
void table() {
    using namespace opengold::srd5::detail;
    std::set<std::string_view> ids;
    unsigned seen_masks = 0;
    for (const auto& spell : spell_table) {
        check(ids.insert(spell.id).second, "Spell ids are unique");
        check(!spell.id.empty() && !spell.label.empty(), "Every row has an id and a label");
        check(spell.mask && (spell.mask & (spell.mask - 1)) == 0, "Every mask is a power of two");
        check((seen_masks & spell.mask) == 0, "Every mask bit is distinct");
        seen_masks |= spell.mask;
        check(spell.range > 0, "Every row has a range");
        // Components must agree with the catalog the components table served.
        const auto* components = spell_components(spell.id);
        check(components && components->verbal == spell.verbal && components->somatic == spell.somatic,
              "Row components match the component catalog");
        const bool saves = spell.pattern == SpellPattern::save_damage || spell.pattern == SpellPattern::save_condition;
        check(saves || spell.save == Ability::strength, "Only save patterns carry a save ability");
        check(spell.instances == 1 || spell.pattern == SpellPattern::auto_damage ||
              spell.pattern == SpellPattern::repeat_attack, "Only multi-instance patterns repeat");
        const bool riders = spell.pattern == SpellPattern::spell_attack || spell.pattern == SpellPattern::save_condition;
        check(spell.rider == Rider::none || riders, "Riders belong to attack and save-condition patterns");
        check(spell.pattern != SpellPattern::save_condition || spell.rider != Rider::none,
              "A save-condition spell applies a rider");
        check(spell.pattern != SpellPattern::heal || spell.add_casting_modifier,
              "Healing adds the caster's spellcasting modifier");
    }
    // Deliberately not an exact row count: that would make adding a spell an
    // edit here too. The explicit id list below is the drift guard instead.
    check(spell_table.size() >= 12, "The implemented catalog is present");
    check(find_spell("magic_missile") == find_spell("magic_missile_2"), "Upcast verbs resolve to one row");
    check(find_spell("melee") == nullptr && find_spell("dash") == nullptr, "Non-spell verbs are not rows");
    // Every id the access catalog knows must exist in the table, so the two
    // cannot drift apart.
    for (const auto* id : {"chill_touch", "shocking_grasp", "eldritch_blast", "ray_of_frost",
                           "sacred_flame", "fire_bolt", "poison_spray", "cure_wounds",
                           "healing_word", "magic_missile", "scorching_ray", "blindness"})
        check(find_spell(id) != nullptr, "Access catalog ids are all table rows");
}

void offers_match_baseline() {
    const auto path = root / "tests/fixtures/spell-offer-baseline.txt";
    const auto actual = snapshot();
    if (!std::filesystem::exists(path)) {
        std::ofstream(path) << actual;
        throw std::runtime_error("Baseline recorded at tests/fixtures/spell-offer-baseline.txt; re-run to verify");
    }
    if (read(path) != actual) {
        const auto dump = std::filesystem::path(OPENGOLD_BINARY_DIR) / "spell-offer-actual.txt";
        std::ofstream(dump) << actual;
        throw std::runtime_error("Spell offers changed; compare tests/fixtures/spell-offer-baseline.txt with " + dump.string());
    }
}
}

int main() {
    try {
        table();
        offers_match_baseline();
        // Run twice: the snapshot must not depend on process state or RNG carry-over.
        offers_match_baseline();
    } catch (const std::exception& error) {
        std::cerr << "spell_table_tests: " << error.what() << '\n';
        return 1;
    }
    std::cout << "spell_table_tests: ok\n";
    return 0;
}
