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
#include "spell_access.h"
#include "spell_components.h"
#include "spell_table.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
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
        // A mask is a frozen legacy wire encoding. New spells carry none, so
        // only the rows that have one must be distinct single bits.
        check(!spell.mask || (spell.mask & (spell.mask - 1)) == 0, "A mask is a single bit");
        check(!spell.mask || (seen_masks & spell.mask) == 0, "Every mask bit is distinct");
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

// ---- generic per-row behaviour (Phase 2 of the acceleration plan) --------
//
// These assertions are written against the table, not against named spells, so
// a spell added as a row plus a class grant gets baseline coverage with no new
// test file. The per-spell targets remain the place for a spell's exact dice,
// save arithmetic and RNG consumption.
namespace generic {
using namespace opengold::srd5::detail;

struct Probe { const char* klass; unsigned level; };
constexpr Probe probes[]{{"wizard", 1}, {"wizard", 4}, {"cleric", 1},
                         {"cleric", 4}, {"warlock", 1}, {"sorcerer", 1}};

// The widest legitimate caster for a class and level: every cantrip its own
// entitlement offers, plus every levelled row the profile writer accepts.
// Discovered from the access API so new rows need no edit here.
CharacterSheet widest(const RulesModule& rules, const Probe& probe) {
    const auto group = starting_cantrip_options(probe.klass);
    std::vector<std::string> cantrips;
    for (const auto& option : group.options) {
        if (cantrips.size() >= group.count) break;
        cantrips.push_back(option.id);
    }
    auto sheet = hero(probe.klass, probe.level, cantrips).sheet();
    for (const auto& row : spell_table) {
        if (!row.level) continue;
        auto trial = sheet;
        trial.prepared_spells.push_back(std::string(row.id));
        try { (void)rules.character_profile(trial, {}); sheet = std::move(trial); }
        catch (const std::exception&) { /* not available to this class or level */ }
    }
    return sheet;
}

CombatantView unit(const CombatSession& c, EntityId id) {
    for (const auto& a : c.snapshot().combatants) if (a.id == id) return a;
    throw std::runtime_error("Missing actor");
}
std::optional<Command> find(const CombatSession& c, std::string_view verb, EntityId target) {
    for (const auto& a : c.legal_commands()) if (a.verb == verb && a.target == target) return a;
    return std::nullopt;
}

// Caster at (1,1), wounded ally id 2 and enemy id 3 both exactly `feet` away.
std::unique_ptr<CombatSession> battle(const RulesModule& rules, const CharacterSheet& sheet,
                                     int feet, std::vector<std::string> gear = {}) {
    const auto profile = rules.character_profile(sheet, gear);
    const int column = 1 + feet / 5;
    auto c = rules.create({{34, 4, std::vector<std::uint8_t>(34 * 4)},
        {{1, "campaign-character", "Caster", 0, {1, 1}, profile.data},
         {2, "target", "Ally", 0, {column, 1}, {}, VitalState{3, false, {}}},
         {3, "target", "Enemy", 1, {column, 2}}}}, 13);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 4; ++turns) {
        const auto end = find(*c, "end", 0);
        check(end && c->submit(*end), "Reach the caster's turn");
    }
    check(c->snapshot().actor == 1, "Caster acts");
    return c;
}

void row_behaviour(const RulesModule& rules, const SpellDef& row, const CharacterSheet& sheet,
                   bool& covered) {
    const EntityId target = row.target == SpellTarget::wounded_ally ? 2 : 3;
    auto at_range = battle(rules, sheet, row.range);
    const auto offer = find(*at_range, row.id, target);
    if (!offer) return; // this caster cannot reach this row; another probe may
    covered = true;

    // Range is inclusive at `range` and excludes the next square. An
    // out-of-range submission must change nothing at all.
    auto beyond = battle(rules, sheet, row.range + 5);
    check(!find(*beyond, row.id, target), "A spell is not offered past its range");
    const auto untouched = beyond->save();
    check(!beyond->submit({beyond->snapshot().revision, 1, target, std::string(row.id)}) &&
          beyond->save() == untouched, "Out-of-range casting is rejected atomically");

    // Somatic spells need a free hand; a weapon plus shield occupies both.
    if (row.somatic) {
        auto encumbered = battle(rules, sheet, row.range, {"quarterstaff", "shield"});
        check(!find(*encumbered, row.id, target), "A Somatic spell needs a free hand");
    }

    // Resolution is deterministic: the same ticket on a restored copy must
    // produce an identical checkpoint, which also pins RNG consumption.
    auto copy = rules.restore(at_range->save());
    const auto before_caster = unit(*at_range, 1);
    const auto before_target = unit(*at_range, target);
    check(at_range->submit(*offer) && copy->submit(*offer), "The offered ticket resolves");
    check(at_range->save() == copy->save(), "Resolution is deterministic across a restore");

    const auto after_caster = unit(*at_range, 1);
    const auto after_target = unit(*at_range, target);
    if (row.bonus_action)
        check(!after_caster.bonus_action && after_caster.action == before_caster.action,
              "A Bonus Action spell spends the Bonus Action and keeps the Action");
    else
        check(!after_caster.action, "An Action spell spends the Action");

    if (row.pattern == SpellPattern::heal)
        check(after_target.hit_points >= before_target.hit_points, "Healing never reduces hit points");
    else
        check(after_target.hit_points <= before_target.hit_points, "An attack never restores hit points");

    // A rider is observable whenever the spell actually landed.
    if (row.rider != Rider::none && after_target.hit_points < before_target.hit_points)
        check(!after_target.conditions.empty() || row.rider == Rider::chill_touch,
              "A landed rider is visible on the target");

    // A stale ticket must not resolve twice.
    const auto settled = at_range->save();
    check(!at_range->submit(*offer) && at_range->save() == settled, "A spent ticket is inert");
}

void behaviour() {
    auto rules = custom();
    std::vector<CharacterSheet> sheets;
    for (const auto& probe : probes) sheets.push_back(widest(*rules, probe));
    for (const auto& row : spell_table) {
        bool covered = false;
        for (const auto& sheet : sheets) row_behaviour(*rules, row, sheet, covered);
        // The payoff: a row no class can actually cast fails here instead of
        // shipping as dead data.
        check(covered, ("No probed class can cast " + std::string(row.id)).c_str());
    }
}
}

// ---- profile capability tags ---------------------------------------------
//
// The PC<n> tag parser replaced a chain of literal comparisons, so it is now
// the single place that decides which capabilities a stored profile carries.
// Profiles are untrusted input (PRD section 20): an unrecognised tag must be
// rejected, never treated as "has everything".
void profile_tags() {
    auto rules = custom();
    const auto loads = [&](const std::string& profile) {
        try {
            (void)rules->create({{8, 8, std::vector<std::uint8_t>(64)},
                {{1, "campaign-character", "Forged", 0, {1, 1}, profile},
                 {2, "target", "Target", 1, {3, 1}}}}, 13);
            return true;
        } catch (const std::exception&) { return false; }
    };

    const auto rogue = rules->character_profile(hero("rogue", 1, {}).sheet(), {}).data;
    check(rogue.starts_with("PC35 "), "A Rogue profile writes the Rogue capability tag");

    // A Wizard profile still writes the packed bitmask, because every
    // implemented spell has a bit. Split it so the tag and the spell field can
    // be replaced independently.
    const auto wizard = rules->character_profile(hero("wizard", 1, {"fire_bolt"}).sheet(), {}).data;
    std::istringstream split(wizard);
    std::string tag, level, features, packed, rest;
    split >> tag >> level >> features >> packed;
    std::getline(split, rest);
    check(!packed.empty() && packed != "0", "A Wizard profile stores spells as a bitmask");
    const auto rebuild = [&](const std::string& new_tag, const std::string& spell_field) {
        return new_tag + " " + level + " " + features + " " + spell_field + rest;
    };
    check(loads(rebuild(tag, packed)), "The split profile is reassembled faithfully");

    // Unrecognised tags: below the floor, above the ceiling, non-numeric, and
    // not a PC tag at all. An unknown tag must never read as "has everything".
    for (const auto& bad : {"PC00", "PC37", "PC99", "PCxx", "XX35", "PC-1"})
        check(!loads(rebuild(bad, packed)), ("Profile tag " + std::string(bad) + " must be rejected").c_str());

    // The explicit spell list is the path that lifts the 31-spell ceiling. No
    // writer emits it yet, because every implemented spell still has a bit, so
    // exercise it with a hand-built profile.
    check(loads(rebuild("PC36", "2 fire_bolt magic_missile")),
          "An explicit spell list loads and satisfies the Wizard grant cross-check");
    check(!loads(rebuild("PC36", "1 not_a_spell")), "An unknown spell id in the list is rejected");
    check(!loads(rebuild("PC36", "99 fire_bolt")), "A spell count beyond the catalog is rejected");
    check(!loads(rebuild("PC36", "2 fire_bolt fire_bolt")), "A duplicate spell id is rejected");
    check(!loads(rebuild("PC36", "2 fire_bolt sacred_flame")),
          "A spell the class cannot access is rejected");
    // A bitmask bit that maps to no spell must reject rather than be dropped.
    check(!loads(rebuild(tag, "4096")), "An unmapped bitmask bit is rejected");
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
        profile_tags();
        generic::behaviour();
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
