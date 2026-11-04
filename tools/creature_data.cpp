#include "creature_data.h"
#include "../src/OpenGold.Core/src/json_document.h"
#include "creature_edits.h"
#include "opengold/campaign_encounter.h"
#include "opengold/companions.h"
#include "opengold/creature_art.h"
#include "opengold/difficulty.h"
#include "opengold/ecl_address.h"
#include "opengold/encounter_budget.h"
#include "opengold/encounter_data.h"
#include "opengold/monster_bundle.h"
#include "opengold/rolf_tour.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <fstream>
#include <map>
#include <set>

namespace opengold::creatures
{
namespace
{
using detail::Json;
namespace fs = std::filesystem;

const fs::path rules_path = "data/rules/srd-5.2.1/combat.rules";

template <typename Number> Json number(Number value)
{
    return Json{static_cast<double>(value)};
}

Json text(std::string value)
{
    return Json{std::move(value)};
}

Json texts(const std::vector<std::string> &values)
{
    Json::Array array;
    for (const auto &value : values)
    {
        array.push_back(text(value));
    }
    return Json{std::move(array)};
}

Json numbers(const auto &values)
{
    Json::Array array;
    for (const auto value : values)
    {
        array.push_back(number(value));
    }
    return Json{std::move(array)};
}

Json read_file_json(const fs::path &path, std::string_view what, std::string_view empty)
{
    return fs::exists(path) ? detail::read_json_file(path, what) : detail::parse_json(empty);
}

const Json::Array &list(const Json &document, std::string_view key)
{
    return detail::json_field<Json::Array>(document, key, key);
}

// The entry of a data file's list for which `matches` holds, or null.
template <typename Matches> Json entry_or_null(const Json::Array &entries, Matches matches)
{
    const auto found = std::find_if(entries.begin(), entries.end(), matches);
    return found == entries.end() ? Json{} : *found;
}

std::string creature_key(unsigned bank, unsigned record)
{
    return "MON" + std::to_string(bank) + "CHA.DAX:" + std::to_string(record);
}

Json attack_json(const rules::AttackStats &attack)
{
    return Json{Json::Object{{"to_hit", number(attack.to_hit)},
                             {"dice", number(attack.dice)},
                             {"sides", number(attack.sides)},
                             {"damage_bonus", number(attack.damage_bonus)},
                             {"damage_type", text(attack.damage_type)},
                             {"range", number(attack.range_feet)},
                             {"long_range", number(attack.long_range_feet)}}};
}

Json stats_json(const rules::CreatureStats &stats)
{
    return Json{Json::Object{{"armor_class", number(stats.armor_class)},
                             {"hit_points", number(stats.hit_points)},
                             {"initiative", number(stats.initiative)},
                             {"speed", number(stats.speed_feet)},
                             {"attacks", number(stats.attacks)},
                             {"melee", attack_json(stats.melee)},
                             {"ranged", stats.ranged ? attack_json(*stats.ranged) : Json{}},
                             {"saves", numbers(stats.saves)},
                             {"spell_slots", number(stats.spell_slots)},
                             {"casting_bonus", number(stats.casting_bonus)},
                             {"spell_level", number(stats.spell_level)},
                             {"spells", texts(stats.spells)}}};
}

// The name of an original item, as the game decodes it.
std::string item_name(const por::Equipment &item)
{
    return item.decoded_name.empty() ? item.label() : item.decoded_name;
}
} // namespace

struct CreatureData::Loaded
{
    std::unique_ptr<rules::RulesModule> rules;
    std::unique_ptr<rules::CharacterRules> creation;
    std::string rules_text;
    por::EncounterData encounters;
    std::shared_ptr<const por::MonsterBundle> monsters;
    std::vector<CompanionDefinition> companions;
    std::shared_ptr<const por::PhlanResources> town;
    Json conversions_file, overrides_file, chances_file, companions_file;

    // The map of the area `script` belongs to, which holds its creatures.
    [[nodiscard]] const por::PhlanResources &district(unsigned script) const
    {
        for (const auto &[id, district] : town->districts)
        {
            if (district->loaded_by(script) && !district->encounter_creatures.empty())
            {
                return *district;
            }
        }
        throw std::runtime_error("No area loads script " + std::to_string(script));
    }

    [[nodiscard]] const por::Creature &original(unsigned script, unsigned record) const
    {
        const auto &creatures = district(script).encounter_creatures;
        const auto found = creatures.find(record);
        if (found == creatures.end())
        {
            throw std::runtime_error("Script " + std::to_string(script) + " converts no record " +
                                     std::to_string(record));
        }
        return found->second;
    }

    [[nodiscard]] const por::ExportedMonster *exported(unsigned bank, unsigned record) const
    {
        return monsters->creature(bank, record);
    }

    [[nodiscard]] std::string name(unsigned script, unsigned record) const
    {
        const auto *found = exported(district(script).bank, record);
        return found ? found->name : original(script, record).stored.name;
    }

    // The script as the player's install holds it, without chances.json's numbers.
    [[nodiscard]] por::EclProgram original_program(unsigned script) const
    {
        const auto &program = *town->programs.at(script);
        return por::EclProgram::decode(program.raw(), program.source());
    }

    // The area's LOAD MONSTERs, in address order.
    [[nodiscard]] std::vector<por::EclInstruction> loads(unsigned script) const
    {
        auto instructions = por::walk_reachable(original_program(script)).instructions;
        std::erase_if(instructions,
                      [](const por::EclInstruction &instruction)
                      {
                          constexpr std::uint8_t load_monster = 11;
                          return instruction.opcode != load_monster;
                      });
        std::ranges::sort(instructions, {}, &por::EclInstruction::address);
        return instructions;
    }

    // The creature as a fight at `difficulty` fields it, met by a party at
    // full strength (average level 2 and up).
    [[nodiscard]] rules::CreatureStats stats(unsigned script, unsigned record,
                                             Difficulty difficulty) const
    {
        const auto &definition = district(script).conversions.at(record).definition;
        const auto bank = district(script).bank;
        CampaignEncounter encounter;
        encounter.enemies.push_back(
            por::staged_fighter(1000, definition, name(script, record), rules::Side::opposition,
                                original(script, record), exported(bank, record), difficulty));
        apply_difficulty(encounter, difficulty, full_strength_party_level);
        return rules->creature_stats(encounter.enemies.front());
    }
};

CreatureData::CreatureData(const fs::path &repository, const fs::path &game_directory,
                           por::PngDecoder decode)
{
    auto loaded = std::make_unique<Loaded>();
    {
        std::ifstream in(repository / rules_path, std::ios::binary);
        loaded->rules_text.assign(std::istreambuf_iterator<char>(in), {});
    }
    loaded->rules = srd5::parse_content(loaded->rules_text);
    loaded->creation = srd5::character_rules();
    loaded->encounters = por::load_encounter_data(repository / "data/encounters");
    loaded->monsters = std::make_shared<const por::MonsterBundle>(
        por::load_monster_bundle(repository / "data/monsters", std::move(decode)));
    loaded->companions = load_companions(repository / "data/companions");
    loaded->town =
        por::RolfTourSession::load(game_directory, loaded->encounters, {}, loaded->monsters)
            .resources();
    loaded->conversions_file = detail::read_json_file(
        repository / "data/encounters/conversions.json", "data/encounters/conversions.json");
    loaded->overrides_file =
        read_file_json(repository / "data/encounters/overrides.json",
                       "data/encounters/overrides.json", R"({"overrides": []})");
    loaded->chances_file = read_file_json(repository / "data/encounters/chances.json",
                                          "data/encounters/chances.json", R"({"chances": []})");
    loaded->companions_file = detail::read_json_file(repository / "data/companions/companions.json",
                                                     "data/companions/companions.json");
    loaded_ = std::move(loaded);
}

CreatureData::~CreatureData() = default;
CreatureData::CreatureData(CreatureData &&) noexcept = default;
CreatureData &CreatureData::operator=(CreatureData &&) noexcept = default;

std::string CreatureData::creatures() const
{
    const auto &data = *loaded_;
    std::map<std::string, Json::Array> sharing; // records by stat line
    Json::Array creatures;
    const auto &areas = list(data.conversions_file, "areas");
    for (std::size_t area_index = 0; area_index < data.encounters.areas.size(); ++area_index)
    {
        const auto &area = data.encounters.areas[area_index];
        const auto script = area.scripts.front();
        const auto &entries = list(areas.at(area_index), "creatures");
        for (std::size_t n = 0; n < area.creatures.size(); ++n)
        {
            const auto &[record, conversion] = area.creatures[n];
            const auto &stored = data.original(script, record).stored;
            sharing[conversion.definition].push_back(
                Json{Json::Object{{"area", text(area.name)}, {"record", number(record)}}});
            creatures.push_back(
                Json{Json::Object{{"area", text(area.name)},
                                  {"scripts", numbers(area.scripts)},
                                  {"bank", number(area.bank)},
                                  {"record", number(record)},
                                  {"key", text(creature_key(area.bank, record))},
                                  {"name", text(data.name(script, record))},
                                  {"definition", text(conversion.definition)},
                                  {"fit_xp", number(conversion.fit_xp)},
                                  {"award_xp", number(conversion.award_xp)},
                                  {"original_hit_points", number(stored.max_hit_points)},
                                  {"original_armor_class", number(stored.base_armor_class)},
                                  {"entry", entries.at(n)}}});
        }
    }
    for (auto &creature : creatures)
    {
        auto &members = std::get<Json::Object>(creature.value);
        const auto definition = std::get<std::string>(creature.find("definition")->value);
        members.emplace_back("sharing", Json{sharing.at(definition)});
    }
    return detail::json_file_text(Json{Json::Object{{"creatures", Json{std::move(creatures)}}}});
}

std::string CreatureData::creature(unsigned script, unsigned record) const
{
    const auto &data = *loaded_;
    const auto &district = data.district(script);
    const auto &original = data.original(script, record);
    const auto *exported = data.exported(district.bank, record);
    Json::Array by_difficulty;
    for (std::size_t n = 0; n < difficulty_names.size(); ++n)
    {
        const auto difficulty = static_cast<Difficulty>(n);
        auto stats = stats_json(data.stats(script, record, difficulty));
        std::get<Json::Object>(stats.value)
            .insert(std::get<Json::Object>(stats.value).begin(),
                    {"difficulty", text(std::string(difficulty_names[n]))});
        by_difficulty.push_back(std::move(stats));
    }
    // What a fight uses each item for: a human-shaped creature fights with the
    // weapons it holds; on Hard and above a monster uses its potions, wands and
    // scrolls; everything else is loot.
    std::set<int> fighting_places;
    for (const auto &item : por::fighting_gear(original, exported))
    {
        fighting_places.insert(item.equipment_index);
    }
    const auto use = [&](bool wielded, int place)
    {
        return wielded ? "weapon" : fighting_places.contains(place) ? "hard_and_above" : "loot";
    };
    Json::Array gear;
    if (exported)
    {
        const bool holds_weapons = por::human_shaped(*exported);
        const auto wielded = por::wielded_items(*exported);
        for (const auto &item : exported->equipment)
        {
            const bool held = holds_weapons && (&item == wielded.melee || &item == wielded.ranged);
            gear.push_back(Json{Json::Object{{"name", text(item.name)},
                                             {"kind", text(item.kind)},
                                             {"readied", Json{item.readied}},
                                             {"use", text(use(held, item.source_index))}}});
        }
    }
    else
    {
        for (std::size_t n = 0; n < original.equipment.size(); ++n)
        {
            gear.push_back(
                Json{Json::Object{{"name", text(item_name(original.equipment[n]))},
                                  {"kind", text("gear")},
                                  {"readied", Json{original.equipment[n].stored.readied()}},
                                  {"use", text(use(false, static_cast<int>(n)))}}});
        }
    }
    return detail::json_file_text(
        Json{Json::Object{{"script", number(script)},
                          {"bank", number(district.bank)},
                          {"record", number(record)},
                          {"name", text(data.name(script, record))},
                          {"human_shaped", Json{exported && por::human_shaped(*exported)}},
                          {"stats", Json{std::move(by_difficulty)}},
                          {"gear", Json{std::move(gear)}}}});
}

std::optional<Image> CreatureData::creature_art(unsigned script, unsigned record) const
{
    const auto &data = *loaded_;
    const auto &district = data.district(script);
    const auto *area = data.encounters.area(script);
    const auto scripts = area ? area->scripts : std::vector<unsigned>{script};
    for (const auto area_script : scripts)
    {
        for (const auto &instruction : data.loads(area_script))
        {
            const auto load = por::written_load_monster(instruction);
            if (load && load->record == record && load->icon)
            {
                const auto *exported = data.exported(district.bank, record);
                auto art = por::creature_art(district.combat_archive, *load->icon, exported, false);
                if (art)
                {
                    return std::move(art->image);
                }
            }
        }
    }
    return std::nullopt;
}

std::string CreatureData::stat_lines() const
{
    const auto &data = *loaded_;
    Json::Array lines;
    for (const auto &key : edits::stat_line_keys(data.rules_text))
    {
        rules::Participant creature;
        creature.definition = key;
        const auto stats = data.rules->creature_stats(creature);
        lines.push_back(Json{Json::Object{{"definition", text(key)},
                                          {"hit_points", number(stats.hit_points)},
                                          {"spell_slots", number(stats.spell_slots)},
                                          {"casting_bonus", number(stats.casting_bonus)},
                                          {"level", number(stats.spell_level)}}});
    }
    return detail::json_file_text(Json{Json::Object{{"stat_lines", Json{std::move(lines)}}}});
}

std::string CreatureData::encounters() const
{
    const auto &data = *loaded_;
    const auto &overrides = list(data.overrides_file, "overrides");
    Json::Array sites;
    for (const auto &[script, bank] : data.town->script_banks)
    {
        const auto *area = data.encounters.area(script);
        const auto loads = data.loads(script);
        // A fight is the LOAD MONSTERs that follow one another.
        unsigned fight = 0;
        for (std::size_t n = 0; n < loads.size(); ++n)
        {
            const auto &instruction = loads[n];
            if (n == 0 || loads[n - 1].next != instruction.address)
            {
                ++fight;
            }
            const auto name = por::site_name({bank, script, instruction.address});
            const auto written = por::written_load_monster(instruction);
            const auto &operands = instruction.operands;
            sites.push_back(Json{Json::Object{
                {"site", text(name)},
                {"area", text(area ? area->name : "Script " + std::to_string(script))},
                {"script", number(script)},
                {"bank", number(bank)},
                {"fight", text("ECL" + std::to_string(bank) + ".DAX:" + std::to_string(script) +
                               ":" + std::to_string(fight))},
                {"record", written ? number(written->record) : Json{}},
                {"count", written ? number(written->count) : Json{}},
                {"icon", written && written->icon ? number(*written->icon) : Json{}},
                {"record_variable",
                 por::is_written(operands.at(0)) ? Json{} : number(operands[0].value)},
                {"count_variable",
                 por::is_written(operands.at(1)) ? Json{} : number(operands[1].value)},
                {"editable", Json{written.has_value()}},
                {"override", entry_or_null(overrides,
                                           [&](const Json &entry)
                                           {
                                               const auto *site = entry.find("site");
                                               return site && detail::json_equal(*site, Json{name});
                                           })}}});
        }
    }
    return detail::json_file_text(Json{Json::Object{{"encounters", Json{std::move(sites)}}}});
}

std::string CreatureData::fight(const std::string &site, std::span<const unsigned> levels,
                                unsigned challenge) const
{
    const auto &data = *loaded_;
    const auto encounters = detail::parse_json(this->encounters());
    const auto &sites = list(encounters, "encounters");
    const auto chosen = std::find_if(sites.begin(), sites.end(),
                                     [&](const Json &entry)
                                     {
                                         return detail::json_equal(*entry.find("site"), Json{site});
                                     });
    if (chosen == sites.end())
    {
        throw std::runtime_error("No LOAD MONSTER at " + site);
    }
    const auto fight_name = *chosen->find("fight");
    const auto script = static_cast<unsigned>(std::get<double>(chosen->find("script")->value));
    const auto &conversions = data.district(script).conversions;
    // The records staged as the game stages them: each LOAD MONSTER of the
    // fight, with its override, up to the fight's limit.
    std::vector<unsigned> records;
    Json::Array groups;
    for (const auto &entry : sites)
    {
        if (!detail::json_equal(*entry.find("fight"), fight_name))
        {
            continue;
        }
        if (!std::get<bool>(entry.find("editable")->value))
        {
            throw std::runtime_error(site + " is part of a fight the script rolls from its tables");
        }
        const auto &change = *entry.find("override");
        const auto value = [&](std::string_view key)
        {
            const auto *changed = change.find(key);
            return static_cast<unsigned>(
                std::get<double>((changed ? *changed : *entry.find(key)).value));
        };
        const auto record = value("record");
        const auto loaded = static_cast<unsigned>(std::min<std::size_t>(
            value("count"),
            por::fight_monster_limit - std::min(por::fight_monster_limit, records.size())));
        records.insert(records.end(), loaded, record);
        groups.push_back(Json{Json::Object{{"site", *entry.find("site")},
                                           {"record", number(record)},
                                           {"name", text(data.name(script, record))},
                                           {"fit_xp", number(conversions.at(record).fit_xp)},
                                           {"loaded", number(loaded)}}});
    }
    const auto staged = por::staged_groups(records, conversions);
    Json::Array by_difficulty;
    for (std::size_t n = 0; n < difficulty_names.size(); ++n)
    {
        const auto difficulty = static_cast<Difficulty>(n);
        const auto strength = monster_strength(difficulty, average_party_level(levels));
        const auto counts = fit_encounter_to_difficulty(staged, levels, challenge, difficulty);
        by_difficulty.push_back(Json{Json::Object{
            {"difficulty", text(std::string(difficulty_names[n]))},
            {"budget",
             number(encounter_xp_budget(levels, challenge * strength.challenge_percent / 100))},
            {"met", numbers(counts)}}});
    }
    return detail::json_file_text(Json{Json::Object{{"fight", fight_name},
                                                    {"levels", numbers(levels)},
                                                    {"challenge", number(challenge)},
                                                    {"groups", Json{std::move(groups)}},
                                                    {"sized", Json{std::move(by_difficulty)}}}});
}

std::string CreatureData::companions() const
{
    const auto &data = *loaded_;
    const auto &entries = list(data.companions_file, "companions");
    Json::Array companions;
    for (std::size_t n = 0; n < data.companions.size(); ++n)
    {
        const auto character = build_companion(data.companions[n], *data.creation, *data.rules);
        const auto &sheet = character.sheet();
        std::vector<int> scores, saves;
        for (const auto ability : rules::all_abilities)
        {
            scores.push_back(sheet.scores[ability]);
            saves.push_back(sheet.saving_throws[ability]);
        }
        std::vector<std::string> features, items;
        for (const auto &grant : sheet.grants)
        {
            features.push_back(grant.id);
        }
        for (const auto &item : character.inventory().items())
        {
            items.push_back(item.name);
        }
        companions.push_back(Json{Json::Object{{"name", text(sheet.name)},
                                               {"class", text(sheet.character_class)},
                                               {"level", number(sheet.level)},
                                               {"hit_points", number(sheet.hit_points)},
                                               {"scores", numbers(scores)},
                                               {"saves", numbers(saves)},
                                               {"features", texts(features)},
                                               {"spells", texts(sheet.prepared_spells)},
                                               {"items", texts(items)},
                                               {"entry", entries.at(n)}}});
    }
    return detail::json_file_text(Json{Json::Object{{"companions", Json{std::move(companions)}}}});
}

std::string CreatureData::chances() const
{
    const auto &data = *loaded_;
    constexpr std::uint8_t compare = 3, random = 8;
    const auto &entries = list(data.chances_file, "chances");
    Json::Array chances;
    for (const auto &[script, bank] : data.town->script_banks)
    {
        const auto *area = data.encounters.area(script);
        auto instructions = por::walk_reachable(data.original_program(script)).instructions;
        std::ranges::sort(instructions, {}, &por::EclInstruction::address);
        // A COMPARE is a chance only when it tests a variable a RANDOM rolls.
        std::set<std::uint16_t> rolled;
        for (const auto &instruction : instructions)
        {
            if (instruction.opcode == random && instruction.operands.size() == 2)
            {
                rolled.insert(instruction.operands[1].value);
            }
        }
        for (const auto &instruction : instructions)
        {
            const auto &operands = instruction.operands;
            for (unsigned operand = 0; operand < operands.size(); ++operand)
            {
                if (!por::is_chance_operand(instruction, operand) ||
                    !por::is_written(operands[operand]))
                {
                    continue;
                }
                if (instruction.opcode == compare &&
                    (operands.size() != 2 || por::is_written(operands[1 - operand]) ||
                     !rolled.contains(operands[1 - operand].value)))
                {
                    continue;
                }
                const auto kind = instruction.opcode == random    ? "random"
                                  : instruction.opcode == compare ? "compare"
                                  : operands[1].value == por::ecl_rest_check_interval.location
                                      ? "rest_interval"
                                      : "rest_chance";
                const auto name = por::site_name({bank, script, instruction.address});
                chances.push_back(Json{Json::Object{
                    {"site", text(name)},
                    {"operand", number(operand)},
                    {"area", text(area ? area->name : "Script " + std::to_string(script))},
                    {"kind", text(kind)},
                    {"variable",
                     number(operands[instruction.opcode == random ? 1 : 1 - operand].value)},
                    {"original", number(operands[operand].value)},
                    {"entry", entry_or_null(entries,
                                            [&](const Json &entry)
                                            {
                                                return detail::json_equal(*entry.find("site"),
                                                                          Json{name}) &&
                                                       detail::json_equal(*entry.find("operand"),
                                                                          number(operand));
                                            })}}});
            }
        }
    }
    return detail::json_file_text(Json{Json::Object{{"chances", Json{std::move(chances)}}}});
}
} // namespace opengold::creatures
