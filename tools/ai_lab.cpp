// Replays campaign fights to measure the computer's play: which difficulty levels
// are harder, and what a change to opengold/ai_tuning.h does. See docs/AI-LAB.md.
//
//   opengold_ai_lab --pack DIR [options]
#include "opengold/ai_scenario.h"
#include "opengold/campaign_party.h"
#include "opengold/campaign_save.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <thread>

using namespace opengold;

namespace
{
constexpr std::string_view usage =
    R"(Usage: opengold_ai_lab --pack DIR [options]

Which fights:
  --list              list the pack's fights and stop
  --only NAME         fights with an enemy whose definition contains NAME
  --fights A,B,...    these fights, by number
  --min-foes N        fights with at least N enemies
  --every N           every Nth of the fights chosen so far

What is staged in them:
  --party-of FIGHT    use the party from this fight, such as a weaker earlier one
  --enemies LIST      replace the enemies, such as bandit-captain*6,ettin*2; each
                      takes its gear from the first fight in the pack that has it

How it is played:
  --party LEVELS      party levels, comma-separated (default infernal, as on Quick)
  --monsters LEVELS   monster levels (default all five); or "all"
  --base P:M          compare against this pairing (default the first party level
                      against normal, or the first monster level)
  --seeds N           dice per fight and pairing (default 8)
  --seed-base N       first seed (default 1000)
  --jobs N            threads (default every core)

Results:
  --by-fight          also one line per fight, for the first and last pairings
)";

struct Options
{
    std::filesystem::path pack;
    bool list{}, by_fight{};
    std::string only, party_of, enemies;
    std::set<std::string, std::less<>> fights;
    unsigned min_foes{}, every{1}, seeds{8}, seed_base{1000};
    unsigned jobs{std::max(1U, std::thread::hardware_concurrency())};
    std::vector<Difficulty> party_levels{Difficulty::infernal};
    std::vector<Difficulty> monster_levels{Difficulty::easy, Difficulty::normal, Difficulty::hard,
                                           Difficulty::nightmare, Difficulty::infernal};
    std::optional<std::pair<Difficulty, Difficulty>> base;
};

std::vector<std::string> comma_list(const std::string &text)
{
    std::vector<std::string> parts;
    std::stringstream stream(text);
    for (std::string part; std::getline(stream, part, ',');)
    {
        parts.push_back(part);
    }
    return parts;
}

Difficulty level_named(const std::string &name)
{
    const auto level = parse_difficulty(name);
    if (!level)
    {
        throw std::runtime_error("Unknown difficulty: " + name);
    }
    return *level;
}

std::vector<Difficulty> levels_named(const std::string &text)
{
    if (text == "all")
    {
        return {Difficulty::easy, Difficulty::normal, Difficulty::hard, Difficulty::nightmare,
                Difficulty::infernal};
    }
    std::vector<Difficulty> levels;
    for (const auto &name : comma_list(text))
    {
        levels.push_back(level_named(name));
    }
    return levels;
}

std::string_view name_of(Difficulty level)
{
    return difficulty_names[static_cast<std::size_t>(level)];
}

Options parse_options(int argc, char **argv)
{
    Options options;
    for (int n = 1; n < argc; ++n)
    {
        const std::string_view flag = argv[n];
        const auto value = [&]() -> std::string
        {
            if (n + 1 >= argc)
            {
                throw std::runtime_error("Missing a value after " + std::string(flag));
            }
            return argv[++n];
        };
        if (flag == "--pack")
        {
            options.pack = value();
        }
        else if (flag == "--list")
        {
            options.list = true;
        }
        else if (flag == "--by-fight")
        {
            options.by_fight = true;
        }
        else if (flag == "--only")
        {
            options.only = value();
        }
        else if (flag == "--fights")
        {
            for (auto &fight : comma_list(value()))
            {
                options.fights.insert(std::move(fight));
            }
        }
        else if (flag == "--min-foes")
        {
            options.min_foes = static_cast<unsigned>(std::stoul(value()));
        }
        else if (flag == "--every")
        {
            options.every = std::max(1U, static_cast<unsigned>(std::stoul(value())));
        }
        else if (flag == "--party-of")
        {
            options.party_of = value();
        }
        else if (flag == "--enemies")
        {
            options.enemies = value();
        }
        else if (flag == "--party")
        {
            options.party_levels = levels_named(value());
        }
        else if (flag == "--monsters")
        {
            options.monster_levels = levels_named(value());
        }
        else if (flag == "--base")
        {
            const auto pair = value();
            const auto colon = pair.find(':');
            if (colon == std::string::npos)
            {
                throw std::runtime_error("--base takes PARTY:MONSTERS");
            }
            options.base = {level_named(pair.substr(0, colon)),
                            level_named(pair.substr(colon + 1))};
        }
        else if (flag == "--seeds")
        {
            options.seeds = static_cast<unsigned>(std::stoul(value()));
        }
        else if (flag == "--seed-base")
        {
            options.seed_base = static_cast<unsigned>(std::stoul(value()));
        }
        else if (flag == "--jobs")
        {
            options.jobs = std::max(1U, static_cast<unsigned>(std::stoul(value())));
        }
        else
        {
            throw std::runtime_error("Unknown option: " + std::string(flag));
        }
    }
    if (options.pack.empty())
    {
        throw std::runtime_error("--pack is required");
    }
    return options;
}

std::unique_ptr<rules::RulesModule> make_module()
{
    // Read once; each fight owns a module parsed from the text. A local static
    // starts safely across the worker threads.
    static const std::string content = []
    {
        std::ifstream in(std::filesystem::path(OPENGOLD_SOURCE_DIR) /
                             "data/rules/srd-5.2.1/combat.rules",
                         std::ios::binary);
        return std::string{std::istreambuf_iterator<char>(in), {}};
    }();
    return srd5::parse_content(content);
}

std::string enemies_of(const CampaignEncounter &encounter)
{
    std::map<std::string, unsigned> counts;
    for (const auto &enemy : encounter.enemies)
    {
        ++counts[enemy.definition];
    }
    std::string text;
    for (const auto &[definition, count] : counts)
    {
        text += (text.empty() ? "" : ", ") + definition + "*" + std::to_string(count);
    }
    return text;
}

// Hand-written enemies, each a copy of the first of its kind in the pack, so it
// carries the gear the game gives it.
std::vector<rules::Participant> staged_enemies(const std::string &list,
                                               const std::vector<AiScenario> &pack)
{
    std::vector<rules::Participant> enemies;
    rules::EntityId next_id = 1000;
    for (const auto &entry : comma_list(list))
    {
        const auto star = entry.find('*');
        const auto definition = entry.substr(0, star);
        const unsigned count = star == std::string::npos
                                   ? 1
                                   : static_cast<unsigned>(std::stoul(entry.substr(star + 1)));
        const rules::Participant *model = nullptr;
        for (const auto &scenario : pack)
        {
            for (const auto &enemy : scenario.encounter.enemies)
            {
                if (!model && enemy.definition == definition)
                {
                    model = &enemy;
                }
            }
        }
        if (!model)
        {
            throw std::runtime_error("No fight in the pack has " + definition);
        }
        for (unsigned n = 0; n < count; ++n, ++next_id)
        {
            auto enemy = *model;
            enemy.id = next_id;
            enemy.name = definition + " " + std::to_string(next_id);
            enemies.push_back(std::move(enemy));
        }
    }
    return enemies;
}

std::vector<const AiScenario *> chosen_fights(const Options &options,
                                              const std::vector<AiScenario> &pack)
{
    std::vector<const AiScenario *> chosen;
    for (const auto &scenario : pack)
    {
        const auto &enemies = scenario.encounter.enemies;
        const bool has_named =
            options.only.empty() ||
            std::any_of(enemies.begin(), enemies.end(),
                        [&](const auto &enemy)
                        {
                            return enemy.definition.find(options.only) != std::string::npos;
                        });
        if (has_named && enemies.size() >= options.min_foes &&
            (options.fights.empty() || options.fights.contains(scenario.name)))
        {
            chosen.push_back(&scenario);
        }
    }
    std::vector<const AiScenario *> thinned;
    for (std::size_t n = 0; n < chosen.size(); n += options.every)
    {
        thinned.push_back(chosen[n]);
    }
    return thinned;
}

struct Staged
{
    std::string name, description;
    PartyState party;
    CampaignEncounter encounter;
};

std::vector<Staged> staged_fights(const Options &options, const std::vector<AiScenario> &pack)
{
    const auto character_rules = srd5::character_rules();
    const auto rules = make_module();
    const auto party_from = [&](const AiScenario &scenario)
    {
        return decode_campaign(scenario.party_save, *character_rules, *rules, ai_scenario_assets,
                               nullptr)
            .party;
    };
    std::optional<PartyState> common_party;
    if (!options.party_of.empty())
    {
        const auto found = std::find_if(pack.begin(), pack.end(),
                                        [&](const AiScenario &scenario)
                                        {
                                            return scenario.name == options.party_of;
                                        });
        if (found == pack.end())
        {
            throw std::runtime_error("No fight " + options.party_of + " in the pack");
        }
        common_party = party_from(*found);
    }
    std::vector<Staged> staged;
    for (const auto *scenario : chosen_fights(options, pack))
    {
        auto encounter = scenario->encounter;
        if (!options.enemies.empty())
        {
            encounter.enemies = staged_enemies(options.enemies, pack);
            encounter.positions.clear();
            encounter.band.clear();
        }
        auto description = enemies_of(encounter);
        staged.push_back({scenario->name, std::move(description),
                          common_party ? *common_party : party_from(*scenario),
                          std::move(encounter)});
    }
    return staged;
}

struct Pairing
{
    Difficulty party, monsters;
    auto operator<=>(const Pairing &) const = default;
};

struct Job
{
    std::size_t fight{};
    Pairing pairing{};
    unsigned seed{};
    AiFightResult result;
    std::string error;
};

// One thread per job slot; each job writes only its own result.
void run_jobs(std::vector<Job> &jobs, const std::vector<Staged> &fights, unsigned threads)
{
    std::atomic<std::size_t> next{0};
    std::vector<std::thread> workers;
    for (unsigned n = 0; n < threads; ++n)
    {
        workers.emplace_back(
            [&]
            {
                for (auto index = next++; index < jobs.size(); index = next++)
                {
                    auto &job = jobs[index];
                    const auto &fight = fights[job.fight];
                    try
                    {
                        job.result =
                            play_ai_fight(make_module, fight.party, fight.encounter,
                                          job.pairing.party, job.pairing.monsters, job.seed);
                    }
                    catch (const std::exception &e)
                    {
                        job.error = e.what();
                    }
                }
            });
    }
    for (auto &worker : workers)
    {
        worker.join();
    }
}

struct Totals
{
    unsigned fights{}, wins{}, stalls{};
    double hit_points{}, down{}, dead{}, commands{};

    void add(const AiFightResult &result)
    {
        ++fights;
        wins += result.won;
        stalls += result.stalled;
        hit_points += result.hit_points_left;
        down += result.members_down;
        dead += result.members_dead;
        commands += result.commands;
    }
};

void print_summary(const std::vector<Job> &jobs, const std::vector<Pairing> &pairings)
{
    std::printf("%-10s %-10s %6s %7s %8s %6s %6s %6s\n", "party", "monsters", "fights", "win%",
                "hp left", "down", "dead", "stalls");
    for (const auto &pairing : pairings)
    {
        Totals totals;
        for (const auto &job : jobs)
        {
            if (job.pairing == pairing && job.error.empty())
            {
                totals.add(job.result);
            }
        }
        const double count = std::max(1U, totals.fights);
        std::printf("%-10s %-10s %6u %7.1f %8.1f %6.2f %6.2f %6u\n",
                    std::string(name_of(pairing.party)).c_str(),
                    std::string(name_of(pairing.monsters)).c_str(), totals.fights,
                    100.0 * totals.wins / count, totals.hit_points / count, totals.down / count,
                    totals.dead / count, totals.stalls);
    }
}

// The same fight and dice under two pairings, so luck largely cancels; "±" is one
// standard error of the mean difference.
void print_differences(const std::vector<Job> &jobs, const std::vector<Pairing> &pairings,
                       Pairing base)
{
    std::map<std::pair<std::size_t, unsigned>, const Job *> base_jobs;
    for (const auto &job : jobs)
    {
        if (job.pairing == base && job.error.empty())
        {
            base_jobs[{job.fight, job.seed}] = &job;
        }
    }
    std::printf("\nAgainst party %s, monsters %s, same fights and dice:\n",
                std::string(name_of(base.party)).c_str(),
                std::string(name_of(base.monsters)).c_str());
    std::printf("%-10s %-10s %9s %16s %8s\n", "party", "monsters", "win", "hp left", "dead");
    for (const auto &pairing : pairings)
    {
        if (pairing == base)
        {
            continue;
        }
        std::vector<double> hit_point_gaps;
        double win_gap = 0, dead_gap = 0;
        for (const auto &job : jobs)
        {
            const auto other = base_jobs.find({job.fight, job.seed});
            if (job.pairing != pairing || !job.error.empty() || other == base_jobs.end())
            {
                continue;
            }
            const auto &baseline = other->second->result;
            hit_point_gaps.push_back(job.result.hit_points_left - baseline.hit_points_left);
            win_gap += static_cast<int>(job.result.won) - static_cast<int>(baseline.won);
            dead_gap += job.result.members_dead - baseline.members_dead;
        }
        const double count = std::max<std::size_t>(1, hit_point_gaps.size());
        double mean = 0;
        for (const double gap : hit_point_gaps)
        {
            mean += gap / count;
        }
        double spread = 0;
        for (const double gap : hit_point_gaps)
        {
            spread += (gap - mean) * (gap - mean);
        }
        const double standard_error = count > 1 ? std::sqrt(spread / (count - 1) / count) : 0.0;
        std::printf("%-10s %-10s %+8.2f%% %+8.2f ±%5.2f %+8.2f\n",
                    std::string(name_of(pairing.party)).c_str(),
                    std::string(name_of(pairing.monsters)).c_str(), 100.0 * win_gap / count, mean,
                    standard_error, dead_gap / count);
    }
}

void print_by_fight(const std::vector<Job> &jobs, const std::vector<Staged> &fights, Pairing first,
                    Pairing last)
{
    std::printf(
        "\nBy fight: wins and hp left, party %s monsters %s | party %s monsters %s\n",
        std::string(name_of(first.party)).c_str(), std::string(name_of(first.monsters)).c_str(),
        std::string(name_of(last.party)).c_str(), std::string(name_of(last.monsters)).c_str());
    for (std::size_t fight = 0; fight < fights.size(); ++fight)
    {
        Totals one, other;
        for (const auto &job : jobs)
        {
            if (job.fight == fight && job.error.empty())
            {
                if (job.pairing == first)
                {
                    one.add(job.result);
                }
                if (job.pairing == last)
                {
                    other.add(job.result);
                }
            }
        }
        std::printf("%-8s %3u/%-3u %6.0f | %3u/%-3u %6.0f  %s\n", fights[fight].name.c_str(),
                    one.wins, one.fights, one.hit_points / std::max(1U, one.fights), other.wins,
                    other.fights, other.hit_points / std::max(1U, other.fights),
                    fights[fight].description.c_str());
    }
}

int run(const Options &options)
{
    const auto pack = load_ai_scenarios(options.pack);
    if (options.list)
    {
        for (const auto &scenario : pack)
        {
            std::cout << scenario.name << "  " << enemies_of(scenario.encounter) << '\n';
        }
        return 0;
    }
    const auto fights = staged_fights(options, pack);
    if (fights.empty())
    {
        throw std::runtime_error("No fight in the pack matches");
    }
    std::vector<Pairing> pairings;
    for (const auto party : options.party_levels)
    {
        for (const auto monsters : options.monster_levels)
        {
            pairings.push_back({party, monsters});
        }
    }
    const auto listed_normal =
        std::find(options.monster_levels.begin(), options.monster_levels.end(), Difficulty::normal);
    const Pairing base = options.base ? Pairing{options.base->first, options.base->second}
                                      : Pairing{options.party_levels.front(),
                                                listed_normal != options.monster_levels.end()
                                                    ? Difficulty::normal
                                                    : options.monster_levels.front()};
    if (std::find(pairings.begin(), pairings.end(), base) == pairings.end())
    {
        pairings.push_back(base);
    }
    std::vector<Job> jobs;
    for (std::size_t fight = 0; fight < fights.size(); ++fight)
    {
        for (unsigned seed = 0; seed < options.seeds; ++seed)
        {
            for (const auto &pairing : pairings)
            {
                jobs.push_back({fight, pairing, options.seed_base + seed, {}, {}});
            }
        }
    }
    const auto start = std::chrono::steady_clock::now();
    run_jobs(jobs, fights, options.jobs);
    const std::chrono::duration<double> taken = std::chrono::steady_clock::now() - start;
    std::printf("%zu fights, %u seeds, %zu games in %.1f s on %u threads\n\n", fights.size(),
                options.seeds, jobs.size(), taken.count(), options.jobs);
    print_summary(jobs, pairings);
    if (pairings.size() > 1)
    {
        print_differences(jobs, pairings, base);
    }
    if (options.by_fight)
    {
        print_by_fight(jobs, fights, pairings.front(), pairings.back());
    }
    int failures = 0;
    for (const auto &job : jobs)
    {
        if (!job.error.empty())
        {
            if (++failures <= 5)
            {
                std::cerr << "Fight " << fights[job.fight].name << " seed " << job.seed << ": "
                          << job.error << '\n';
            }
        }
    }
    if (failures)
    {
        std::cerr << failures << " games failed\n";
    }
    return failures ? 1 : 0;
}
} // namespace

int main(int argc, char **argv)
{
    try
    {
        return run(parse_options(argc, argv));
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << "\n\n" << usage;
        return 2;
    }
}
