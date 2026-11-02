#include "opengold/ai_scenario.h"
#include "opengold/encounter_budget.h"
#include "opengold/campaign_party.h"
#include "opengold/campaign_save.h"
#include "opengold/combat_demo.h"
#include <algorithm>
#include <charconv>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>

namespace opengold
{
namespace
{
constexpr std::string_view format_line = "opengold-ai-encounter 2";

// A fight the policy cannot finish: both sides stand off with no player to break
// it. A real fight takes far fewer commands than this.
constexpr unsigned stalled_after_commands = 4000;

std::vector<std::string_view> split(std::string_view text, char separator)
{
    std::vector<std::string_view> parts;
    while (true)
    {
        const auto end = text.find(separator);
        parts.push_back(text.substr(0, end));
        if (end == std::string_view::npos)
        {
            return parts;
        }
        text.remove_prefix(end + 1);
    }
}

long long number(std::string_view text)
{
    long long value{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size())
    {
        throw std::runtime_error("Bad number in an AI scenario: " + std::string(text));
    }
    return value;
}

int as_int(std::string_view text)
{
    return static_cast<int>(number(text));
}

unsigned as_unsigned(std::string_view text)
{
    return static_cast<unsigned>(number(text));
}

void write_participant(std::ostream &out, std::string_view tag, const rules::Participant &who)
{
    if (!who.character_profile.empty() || who.state)
    {
        throw std::runtime_error("An AI scenario keeps only rules-built monsters: " + who.name);
    }
    out << tag << '\t' << who.id << '\t' << who.definition << '\t' << who.name << '\t'
        << static_cast<int>(who.side) << '\t' << who.cell.x << '\t' << who.cell.y << '\t'
        << who.surprised << '\t' << who.facing_left << '\t' << who.resting << '\t' << who.morale
        << '\t' << who.intelligence << '\t' << who.melee_weapon << '\t' << who.ranged_weapon << '\t'
        << who.footprint.width << '\t' << who.footprint.height << '\t' << who.holds_shield << '\t'
        << who.inventory.size() << '\n';
    for (const auto &item : who.inventory)
    {
        out << "item\t" << item.inventory_id << '\t' << item.definition << '\t' << item.quantity
            << '\t' << item.equipment_index << '\n';
    }
}

rules::Participant read_participant(const std::vector<std::string_view> &fields)
{
    if (fields.size() != 18)
    {
        throw std::runtime_error("Bad creature in an AI scenario");
    }
    rules::Participant who;
    who.id = static_cast<rules::EntityId>(number(fields[1]));
    who.definition = fields[2];
    who.name = fields[3];
    who.side = static_cast<rules::Side>(as_int(fields[4]));
    who.cell = {as_int(fields[5]), as_int(fields[6])};
    who.surprised = as_int(fields[7]) != 0;
    who.facing_left = as_int(fields[8]) != 0;
    who.resting = as_int(fields[9]) != 0;
    who.morale = as_unsigned(fields[10]);
    who.intelligence = as_unsigned(fields[11]);
    who.melee_weapon = fields[12];
    who.ranged_weapon = fields[13];
    who.footprint = {as_int(fields[14]), as_int(fields[15])};
    who.holds_shield = as_int(fields[16]) != 0;
    return who;
}

std::string read_file(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        throw std::runtime_error("Cannot read " + path.string());
    }
    return {std::istreambuf_iterator<char>(in), {}};
}
} // namespace

std::string encode_ai_encounter(const CampaignEncounter &encounter)
{
    const auto &board = encounter.field.geometry;
    std::ostringstream out;
    out << format_line << '\n';
    out << "field\t" << board.width << '\t' << board.height << '\n';
    out << "terrain\t";
    for (const auto terrain : board.terrain)
    {
        out << static_cast<int>(terrain);
    }
    out << "\nplants\t";
    for (const bool plant : board.plants)
    {
        out << plant;
    }
    out << "\nwalled\t";
    for (const auto &row : encounter.field.walled)
    {
        for (const auto &square : row)
        {
            for (const bool side : square)
            {
                out << side;
            }
        }
    }
    out << '\n';
    for (const auto &member : encounter.band)
    {
        write_participant(out, "band", member.creature);
        out << "bandfit\t" << member.record << '\t' << member.xp << '\n';
    }
    out << "\noptions\t" << static_cast<int>(encounter.facing) << '\t'
        << static_cast<int>(encounter.surprise) << '\t' << encounter.distance << '\t'
        << encounter.party_resting << '\t' << encounter.morale << '\t' << encounter.champion << '\t'
        << encounter.daylight << '\n';
    out << "positions";
    for (const auto cell : encounter.positions)
    {
        out << '\t' << cell.x << '\t' << cell.y;
    }
    out << '\n';
    for (const auto &enemy : encounter.enemies)
    {
        write_participant(out, "enemy", enemy);
    }
    for (const auto &ally : encounter.allies)
    {
        write_participant(out, "ally", ally);
    }
    return out.str();
}

CampaignEncounter decode_ai_encounter(std::string_view text)
{
    CampaignEncounter encounter;
    auto &board = encounter.field.geometry;
    std::vector<rules::CarriedEquipment> *last_gear = nullptr;
    bool first_line = true;
    for (const auto line : split(text, '\n'))
    {
        if (first_line)
        {
            if (line != format_line)
            {
                throw std::runtime_error("Not an AI scenario of this version");
            }
            first_line = false;
            continue;
        }
        if (line.empty())
        {
            continue;
        }
        const auto fields = split(line, '\t');
        const auto tag = fields.front();
        if (tag == "field")
        {
            board.width = as_int(fields.at(1));
            board.height = as_int(fields.at(2));
        }
        else if (tag == "terrain")
        {
            for (const char digit : fields.at(1))
            {
                board.terrain.push_back(static_cast<rules::Terrain>(digit - '0'));
            }
        }
        else if (tag == "plants")
        {
            for (const char digit : fields.at(1))
            {
                board.plants.push_back(digit == '1');
            }
        }
        else if (tag == "walled")
        {
            std::size_t next = 0;
            const auto bits = fields.at(1);
            for (auto &row : encounter.field.walled)
            {
                for (auto &square : row)
                {
                    for (auto &&side : square)
                    {
                        side = bits.at(next++) == '1';
                    }
                }
            }
        }
        else if (tag == "options")
        {
            encounter.facing = static_cast<por::MapDirection>(as_int(fields.at(1)));
            encounter.surprise = static_cast<Surprise>(as_int(fields.at(2)));
            encounter.distance = as_unsigned(fields.at(3));
            encounter.party_resting = as_int(fields.at(4)) != 0;
            encounter.morale = as_unsigned(fields.at(5));
            encounter.champion = static_cast<rules::EntityId>(number(fields.at(6)));
            encounter.daylight = as_int(fields.at(7)) != 0;
        }
        else if (tag == "positions")
        {
            for (std::size_t n = 1; n + 1 < fields.size(); n += 2)
            {
                encounter.positions.push_back({as_int(fields[n]), as_int(fields[n + 1])});
            }
        }
        else if (tag == "enemy" || tag == "ally")
        {
            auto &side = tag == "enemy" ? encounter.enemies : encounter.allies;
            side.push_back(read_participant(fields));
            last_gear = &side.back().inventory;
        }
        else if (tag == "band")
        {
            encounter.band.push_back({read_participant(fields)});
            last_gear = &encounter.band.back().creature.inventory;
        }
        else if (tag == "bandfit")
        {
            encounter.band.back().record = as_unsigned(fields.at(1));
            encounter.band.back().xp = as_unsigned(fields.at(2));
        }
        else if (tag == "item" && last_gear)
        {
            last_gear->push_back({static_cast<std::uint64_t>(number(fields.at(1))),
                                  std::string(fields.at(2)), as_unsigned(fields.at(3)),
                                  as_int(fields.at(4))});
        }
        else
        {
            throw std::runtime_error("Unknown line in an AI scenario: " + std::string(tag));
        }
    }
    return encounter;
}

void write_ai_scenario(const std::filesystem::path &folder, std::string_view name,
                       const CampaignParty &party, const CampaignEncounter &encounter)
{
    std::filesystem::create_directories(folder);
    const auto encoded_encounter = encode_ai_encounter(encounter);
    const auto stem = folder / std::string(name);
    write_campaign_file(std::filesystem::path(stem).concat(".ogs"),
                        encode_campaign(party, nullptr, ai_scenario_assets));
    std::ofstream(std::filesystem::path(stem).concat(".fight"), std::ios::binary)
        << encoded_encounter;
}

std::vector<AiScenario> load_ai_scenarios(const std::filesystem::path &folder)
{
    std::vector<AiScenario> scenarios;
    for (const auto &file : std::filesystem::directory_iterator(folder))
    {
        if (file.path().extension() != ".fight")
        {
            continue;
        }
        auto party_path = file.path();
        party_path.replace_extension(".ogs");
        scenarios.push_back({file.path().stem().string(), read_campaign_file(party_path),
                             decode_ai_encounter(read_file(file.path()))});
    }
    std::sort(scenarios.begin(), scenarios.end(),
              [](const AiScenario &a, const AiScenario &b)
              {
                  return a.name < b.name;
              });
    return scenarios;
}

CampaignEncounter encounter_at_difficulty(CampaignEncounter encounter,
                                          std::span<const unsigned> member_levels,
                                          Difficulty difficulty)
{
    if (!encounter.band.empty())
    {
        std::vector<EncounterGroup> groups;
        for (std::size_t n = 0; n < encounter.band.size(); ++n)
        {
            if (n == 0 || encounter.band[n].record != encounter.band[n - 1].record)
            {
                groups.push_back({encounter.band[n].xp, 0});
            }
            ++groups.back().count;
        }
        const auto counts = fit_encounter_to_difficulty(groups, member_levels,
                                                        default_encounter_challenge, difficulty);
        encounter.enemies.clear();
        encounter.positions.clear();
        for (std::size_t n = 0, group = 0, kept = 0; n < encounter.band.size(); ++n)
        {
            if (n && encounter.band[n].record != encounter.band[n - 1].record)
            {
                ++group;
                kept = 0;
            }
            if (kept++ >= counts[group])
            {
                continue;
            }
            auto enemy = encounter.band[n].creature;
            enemy.id = static_cast<rules::EntityId>(1000 + encounter.enemies.size());
            enemy.name += " " + std::to_string(encounter.enemies.size() + 1);
            if (!monsters_use_items(difficulty))
            {
                enemy.inventory.clear();
            }
            encounter.enemies.push_back(std::move(enemy));
        }
    }
    apply_difficulty(encounter, difficulty, average_party_level(member_levels));
    return encounter;
}

AiFightResult play_ai_fight(const RulesModuleFactory &make_module, const PartyState &party,
                            const CampaignEncounter &encounter, Difficulty party_level,
                            Difficulty monster_level, std::uint64_t seed)
{
    auto fighters = std::make_shared<CampaignParty>(make_module());
    fighters->restore(party);
    std::vector<unsigned> member_levels;
    for (const auto id : fighters->state().slots)
    {
        if (id && !fighters->member(id).vitals.dead)
        {
            member_levels.push_back(fighters->member(id).character.sheet().level);
        }
    }
    CombatDemo combat(make_module());
    combat.campaign_party(fighters);
    combat.encounter(encounter_at_difficulty(encounter, member_levels, monster_level), seed);
    AiFightResult result;
    for (; result.commands < stalled_after_commands; ++result.commands)
    {
        const auto state = combat.combat().snapshot();
        if (state.outcome != rules::Outcome::ongoing)
        {
            break;
        }
        const bool member_acts =
            state.actor && !is_ally(state.actor) &&
            std::any_of(state.combatants.begin(), state.combatants.end(),
                        [&](const auto &unit)
                        {
                            return unit.id == state.actor && unit.side == rules::Side::party;
                        });
        const auto level = member_acts ? party_level : monster_level;
        if (!combat.submit(choose_demo_command(state, combat.combat().legal_commands(),
                                               Searching::party, level)))
        {
            throw std::runtime_error("Combat refused the policy's command");
        }
    }
    const auto final_state = combat.combat().snapshot();
    result.stalled = result.commands >= stalled_after_commands;
    result.won = final_state.outcome == rules::Outcome::victory;
    for (const auto &unit : final_state.combatants)
    {
        if (unit.side == rules::Side::party && !is_ally(unit.id))
        {
            result.hit_points_left += std::max(0, unit.hit_points);
            result.members_down += unit.hit_points <= 0 ? 1 : 0;
            result.members_dead += unit.dead ? 1 : 0;
        }
    }
    return result;
}
} // namespace opengold
