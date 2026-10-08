#include "opengold/campaign_save.h"
#include "opengold/save_file.h"
#include "opengold/srd5.h"
#include "opengold/combat_demo.h"
#include <iostream>
#include <chrono>
#include <fstream>
using namespace opengold;

namespace
{
struct TestDirectory
{
    std::filesystem::path path =
        std::filesystem::temp_directory_path() /
        ("opengold-save-test-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    TestDirectory() = default;
    TestDirectory(const TestDirectory &) = delete;
    TestDirectory &operator=(const TestDirectory &) = delete;

    ~TestDirectory()
    {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
};

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
    check(caught, "Invalid save must reject");
}

// The message f throws, or an empty string when f does not throw.
template <class F> std::string rejection_message(F f)
{
    try
    {
        f();
    }
    catch (const std::exception &e)
    {
        return e.what();
    }

    return {};
}

// The only campaign format this build reads and writes.
constexpr unsigned current_campaign_format = 24;

// The checksum line covers only the body, so rewriting the header number
// changes nothing but the claimed format.
std::string with_campaign_format(const std::string &saved, unsigned format)
{
    return "OPENGOLD-CAMPAIGN " + std::to_string(format) + saved.substr(saved.find('\n'));
}

std::string changed_identity(const std::string &saved, const std::string &identity,
                             const std::string &replacement = "incompatible")
{
    auto start = saved.find('\n', saved.find('\n') + 1) + 1;
    auto body = saved.substr(start);
    auto position = body.find('"' + identity + '"');
    check(position != body.npos, "Identity must be present");
    body.replace(position, identity.size() + 2, '"' + replacement + '"');
    std::uint64_t hash = 14695981039346656037ULL;
    for (unsigned char c : body)
    {
        hash ^= c;
        hash *= 1099511628211ULL;
    }
    return saved.substr(0, saved.find('\n') + 1) + std::to_string(hash) + '\n' + body;
}

auto module()
{
    return srd5::load(std::filesystem::path(OPENGOLD_SOURCE_DIR) /
                      "data/rules/srd-5.2.1/combat.rules");
}

Character character(std::string klass)
{
    rules::CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = klass;
    d.alignment = "neutral_good";
    d.background = "soldier";
    if (klass == "wizard")
        d.training["class:wizard"] = {"medicine", "nature"};
    d.name = "Save test " + klass;
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    por::CharacterAppearance a;
    a.portrait = "human-male-fighter-01.png";
    return Character(*srd5::character_rules(), d, a);
}

auto prototype()
{
    std::vector<std::uint8_t> bytes{0, 0};
    for (int n = 0; n < 5; ++n)
        bytes.insert(bytes.end(), {1, 1, 0x15, 0x99});
    bytes.push_back(0);
    // Each LOOK increments a persistent cell, then completes.
    bytes.insert(bytes.end(), {4, 1, 0x10, 0x98, 0, 1, 1, 0x10, 0x98, 0});
    auto p =
        std::make_shared<const por::EclProgram>(por::EclProgram::decode(bytes, "save fixture"));
    auto resources = std::make_shared<por::PhlanResources>();
    resources->programs[0] = p;
    return por::RolfTourSession({}, p, {}, 0x9914, {}, resources);
}

void settle(por::RolfTourSession &town)
{
    for (int i = 0; i < 100 && town.snapshot().phase == por::TourPhase::running; ++i)
        town.advance(1);
    check(town.can_leave(), "Fixture must finish");
}

std::string campaign_payload(unsigned version, const std::string &body)
{
    std::uint64_t hash = 14695981039346656037ULL;
    for (unsigned char c : body)
    {
        hash ^= c;
        hash *= 1099511628211ULL;
    }
    return "OPENGOLD-CAMPAIGN " + std::to_string(version) + '\n' + std::to_string(hash) + '\n' +
           body;
}

void fog_saves(const std::filesystem::path &directory)
{
    // Two authored resource banks. CAMP is a test travel trigger; ordinary
    // movement/search entries exit. Entry 4 selects the destination map.
    const auto travel_program = [](unsigned area, unsigned destination)
    {
        std::vector<std::uint8_t> bytes{0, 0};
        for (unsigned slot = 0; slot < 5; ++slot)
        {
            const unsigned address = slot == 2 ? 0x9915 : slot == 4 ? 0x9919 : 0x9914;
            bytes.insert(bytes.end(), {1, 1, static_cast<std::uint8_t>(address & 255),
                                       static_cast<std::uint8_t>(address >> 8)
                                      });
        }
        bytes.insert(bytes.end(),
        {
            0, 32, 0, static_cast<std::uint8_t>(destination), 0, 33, 0,
            static_cast<std::uint8_t>(area), 0, static_cast<std::uint8_t>(area ? 2 : 0),
            0, static_cast<std::uint8_t>(area ? 255 : 0), 0
        });
        return std::make_shared<const por::EclProgram>(
                   por::EclProgram::decode(bytes, "fog travel fixture"));
    };
    auto resources = std::make_shared<por::PhlanResources>();
    resources->map = por::GeoMap{};
    resources->programs[0] = travel_program(0, 20);
    resources->programs[20] = travel_program(20, 0);
    auto district = std::make_shared<por::PhlanResources>();
    district->map = por::GeoMap{};
    district->script = 20; // the Slums script loads it
    district->bank = 2;
    resources->districts[20] = district;
    auto party = std::make_shared<CampaignParty>(module());
    party->add_pc(character("fighter"));
    por::RolfTourSession town({}, resources->programs.at(0), {}, 0x9914, {}, resources);
    town.campaign_party(party);
    settle(town);
    town.explore(por::ExplorationCommand::turn_right);
    town.explore(por::ExplorationCommand::forward);
    settle(town);
    (void)town.observe_view();
    const auto city = town.snapshot();
    check(city.visited.count() == 2 && city.seen.test(3),
          "City tracks both walked squares and forward sight");
    town.explore(por::ExplorationCommand::camp);
    settle(town);
    check(town.snapshot().area_id == 20 && town.snapshot().seen.count() == 1 &&
          !town.snapshot().seen.test(3),
          "A new district does not inherit knowledge from matching coordinates in the city");
    town.explore(por::ExplorationCommand::turn_around);
    (void)town.observe_view();
    const auto slums = town.snapshot();
    check(slums.seen != city.seen && slums.visited.count() == 1,
          "Districts retain independent seen and visited histories");

    const auto saved = encode_campaign(*party, &town, "fog-fixture");
    const auto path = directory / "fog.ogs";
    write_campaign_file(path, saved);
    por::RolfTourSession base({}, resources->programs.at(0), {}, 0x9914, {}, resources);
    auto rules = module();
    auto loaded = decode_campaign(read_campaign_file(path), *srd5::character_rules(), *rules,
                                  "fog-fixture", &base);
    auto replacement = std::make_shared<CampaignParty>(module());
    replacement->restore(std::move(loaded.party));
    auto &restored = *loaded.town;
    restored.attach_restored_party(replacement);
    check(restored.snapshot().seen == slums.seen && restored.snapshot().visited == slums.visited,
          "Disk reload restores the active district's exact knowledge");
    check(encode_campaign(*replacement, &restored, "fog-fixture") == saved,
          "All district knowledge round trips canonically");
    restored.explore(por::ExplorationCommand::camp);
    settle(restored);
    check(restored.snapshot().area_id == 0 && restored.snapshot().seen == city.seen &&
          restored.snapshot().visited == city.visited,
          "Returning after reload recovers the inactive district's exact history");
    restored.explore(por::ExplorationCommand::camp);
    settle(restored);
    check(restored.snapshot().area_id == 20 && restored.snapshot().seen == slums.seen,
          "Revisiting retains the other district's history too");

    auto single = prototype();
    single.campaign_party(party);
    settle(single);
    single.explore(por::ExplorationCommand::turn_right);
    (void)single.observe_view();
    const auto current = encode_campaign(*party, &single, "fog-fixture");
    auto body = current.substr(current.find('\n', current.find('\n') + 1) + 1);
    // Split the body around the active area's knowledge record so malformed
    // records can be spliced in while every other field stays current.
    const auto knowledge = "1 0 \"" + single.snapshot().seen.to_string() + "\" ";
    const auto knowledge_at = body.rfind(knowledge);
    check(knowledge_at != body.npos, "The town fixture records its sight knowledge");
    const auto after_knowledge = body.substr(knowledge_at + knowledge.size());
    body.resize(knowledge_at);
    auto town_base = prototype();
    const auto decode_with_knowledge = [&](const std::string & fields)
    {
        return decode_campaign(
                   campaign_payload(current_campaign_format, body + fields + after_knowledge),
                   *srd5::character_rules(), *rules, "fog-fixture", &town_base);
    };
    check(decode_with_knowledge(knowledge).town->snapshot().seen == single.snapshot().seen,
          "The reassembled fixture loads its exact knowledge");
    for (const auto &invalid :
            {
                std::string("0 "), "1 0 \"" + std::string(256, '0') + "\" ",
                "1 99 \"" + std::string(256, '1') + "\" ", "1 0 \"" + std::string(255, '1') + "x\" "
            })
        rejects(
            [&]
    {
        (void)decode_with_knowledge(invalid);
    });
    check(encode_campaign(*party, &single, "fog-fixture") == current,
          "Malformed fog saves leave the live campaign untouched");
}

void file_safety(const std::filesystem::path &directory)
{
    const auto path = directory / "storage.save";
    auto temporary = path;
    temporary += ".tmp";
    auto backup = path;
    backup += ".bak";
    const auto has_temporary = [&]
    {
for (const auto &entry : std::filesystem::directory_iterator(directory))
        if (entry.path().filename().string().starts_with("storage.save.tmp"))
            return true;
            return false;
        };
const std::string first("first\0checkpoint", 16);
    write_save_file(path, first, 64);
    check(read_save_file(path, 64) == first, "Storage preserves binary checkpoint bytes");
    check(!has_temporary(), "Successful save consumes its temporary file");
    write_save_file(path, "second", 64);
    check(read_save_file(backup, 64) == first, "Storage retains the preceding checkpoint");
    rejects(
        [&]
    {
        write_save_file(path, "oversized", 4);
    });
    rejects(
        [&]
    {
        (void)read_save_file(path, 4);
    });
    check(read_save_file(path, 64) == "second" && !has_temporary(),
          "Size rejection leaves the current save intact");

    // Force failure after the temporary file is written and verified.
    std::filesystem::remove(backup);
    std::filesystem::create_directory(backup);
    rejects(
        [&]
    {
        write_save_file(path, "third", 64);
    });
    check(read_save_file(path, 64) == "second",
          "Failed backup/replacement preserves the current save");
    check(!has_temporary(), "Failed replacement removes its owned temporary file");
    std::filesystem::remove(backup);
    write_save_file(path, "third", 64);
    check(read_save_file(path, 64) == "third" && read_save_file(backup, 64) == "second",
          "Retry succeeds after replacement failure");

    // Files left by interrupted or concurrent writes are not ours to remove.
    {
        std::ofstream held(temporary, std::ios::binary);
        held << "another writer";
    }
    write_save_file(path, "fourth", 64);
    check(read_save_file(temporary, 64) == "another writer" && read_save_file(path, 64) == "fourth",
          "Unowned temporary file survives and does not block a save");
    std::filesystem::remove(temporary);
    write_save_file(path, {}, 64);
    check(read_save_file(path, 64).empty(), "Empty checkpoints round trip");
    rejects(
        [&]
    {
        (void)read_save_file(directory / "missing.save", 64);
    });
}

// Before 1.0 there is no save migration: only the current campaign format with
// the current rules identity loads, and older saves are refused with one message.
void check_campaign_format_cutoff(const std::string &saved, const rules::RulesModule &rules,
                                  const por::RolfTourSession &town_template)
{
    const auto decode_error = [&](const std::string & bytes)
    {
        return rejection_message(
                   [&]
        {
            (void)decode_campaign(bytes, *srd5::character_rules(), rules, "fixture-v1",
                                  &town_template);
        });
    };
    check(saved.starts_with("OPENGOLD-CAMPAIGN " + std::to_string(current_campaign_format) + "\n"),
          "The writer emits the current campaign format");
    check(decode_error(saved).empty(), "The current writer's campaign loads");
    check(decode_error(with_campaign_format(saved, current_campaign_format - 1)) ==
          rules::older_save_message,
          "An older campaign format is refused as an older pre-release save");
    check(decode_error(changed_identity(saved, rules.identity().version, "0.6.61")) ==
          rules::older_save_message,
          "A campaign from another rules version is refused as an older pre-release save");
    check(decode_error(with_campaign_format(saved, current_campaign_format + 1)) ==
          "Unsupported campaign save version",
          "An unknown newer campaign format is unsupported");
}

void roundtrip(const std::filesystem::path &directory)
{
    std::filesystem::create_directories(directory);
    for (const auto *filename :
            {"../portrait.png", "a/b.png", "a\\b.png", "portrait.jpg"
            })
        rejects(
            [&]
    {
        por::CharacterAppearance a;
        a.portrait = filename;
        por::validate_character_appearance(a);
    });
    auto party = std::make_shared<CampaignParty>(module());
    auto fighter = party->add_pc(character("fighter"));
    auto mage = party->add_pc(character("wizard"));
    auto reserve = party->add_pc(character("bard"));
    party->remove(reserve);
    party->set_wealth(fighter, {0, 0, 0, 500, 0, 0, 2});
    por::Equipment sword;
    sword.stored.type = 36;
    sword.stored.stack_size = 1;
    sword.stored.value = 10;
    party->purchase(fighter, sword);
    party->equip(fighter, 1);
    party->award_experience(300, "save:encounter");
    party->advance(fighter, party->default_advancement(fighter));
    party->advance(mage, party->default_advancement(mage));
    check(party->rest(), "Initial rest");
    party->keep_rest_spells(mage);
    auto state = party->checkpoint();
    state.roster[0].vitals.hit_points = 1;
    // A single level-one slot remains; every other pool is full.
    const std::string spent_slot = "SRD11 0 1 0 0 0 0 2 0 0 0 \"\" 0 0 1 0 0 0 FX8 1 0 0";
    state.roster[1].vitals.resources = spent_slot;
    // The module rewrites the description whenever it touches the vitals, so
    // keep this hand-built state consistent with what it would write.
    state.roster[1].vitals.description =
        "Level-one spell slots: 1 / 3\nArcane Recovery uses: 1 / 1";
    party->restore(state);
    party->temple_heal(fighter);
    auto town = prototype();
    town.campaign_party(party);
    settle(town);
    town.explore(por::ExplorationCommand::look);
    settle(town);
    auto path = directory / std::filesystem::u8path("named save ü.ogs");
    const auto saved = encode_campaign(*party, &town, "fixture-v1");
    write_campaign_file(path, saved);
    auto base = prototype();
    auto rules = module();
    auto loaded = decode_campaign(read_campaign_file(path), *srd5::character_rules(), *rules,
                                  "fixture-v1", &base);
    auto replacement = std::make_shared<CampaignParty>(module());
    replacement->restore(std::move(loaded.party));
    loaded.town->attach_restored_party(replacement);
    check(encode_campaign(*replacement, &*loaded.town, "fixture-v1") == saved,
          "Complete serialized state round trips");
    check_campaign_format_cutoff(saved, *rules, base);
    const auto encounter = [](const CampaignParty & p)
    {
        rules::Encounter e{{8, 8, std::vector<std::uint8_t>(64)}, p.participants()};
        e.participants.push_back({99, "bandit", "Bandit", 1, {6, 6}});
        return e;
    };
    auto combat_a = rules->create(encounter(*party), 42),
         combat_b = rules->create(encounter(*replacement), 42);
    for (int i = 0; i < 30 && combat_a->snapshot().outcome == rules::Outcome::ongoing; ++i)
    {
        auto command = choose_demo_command(*combat_a);
        check(combat_a->submit(command) && combat_b->submit(command) &&
              combat_a->save() == combat_b->save(),
              "Next combat continues deterministically after disk reload");
    }
    check(replacement->member(fighter).wealth[3] == 390, "Temple charge survives load");
    check(replacement->member(mage).vitals.resources == spent_slot,
          "Spent caster slots survive load");
    replacement->award_experience(300, "save:encounter");
    check(replacement->member(fighter).experience == 300, "No duplicate XP after reload");
    check(!replacement->rest(), "Reload must not reset rest timer");
    const auto fighter_rest = replacement->member(fighter).last_rest_minutes;
    const auto mage_vitals = replacement->member(mage).vitals;
    replacement->rejoin(reserve);
    check(replacement->rest(), "A newly eligible reserve can rest after rejoining");
    check(replacement->member(fighter).last_rest_minutes == fighter_rest &&
          replacement->member(mage).vitals == mage_vitals,
          "The rejoined member does not bypass other members' cooldowns or refill their resources");
    replacement->remove(reserve);
    auto wounded = replacement->checkpoint();
    wounded.roster[0].vitals.hit_points = 1;
    replacement->restore(wounded);
    party->restore(wounded);
    replacement->temple_heal(fighter);
    party->temple_heal(fighter);
    check(replacement->member(fighter).vitals == party->member(fighter).vitals &&
          replacement->state().random_state == party->state().random_state,
          "Service RNG continuation matches");
    loaded.town->explore(por::ExplorationCommand::look);
    town.explore(por::ExplorationCommand::look);
    settle(*loaded.town);
    settle(town);
    check(loaded.town->script_variable(0x9810) == town.script_variable(0x9810),
          "Script continuation matches");
    auto next = encode_campaign(*replacement, &*loaded.town, "fixture-v1");
    write_campaign_file(path, next);
    auto backup = path;
    backup += ".bak";
    check(read_campaign_file(backup) == saved, "Overwrite retains previous save");
    write_campaign_file(path, saved);
    check(read_campaign_file(backup) == next, "Second overwrite rotates previous save");
    auto truncated = saved.substr(0, saved.size() / 2);
    write_campaign_file(directory / "truncated.ogs", truncated);
    rejects(
        [&]
    {
        (void)decode_campaign(read_campaign_file(directory / "truncated.ogs"),
        *srd5::character_rules(), *rules, "fixture-v1", &base);
    });
    auto corrupt = saved;
    corrupt.back() ^= 1;
    rejects(
        [&]
    {
        (void)decode_campaign(corrupt, *srd5::character_rules(), *rules, "fixture-v1", &base);
    });
    rejects(
        [&]
    {
        (void)decode_campaign(saved, *srd5::character_rules(), *rules, "different-assets",
        &base);
    });
    for (const auto &identity :
            {
                rules->identity().module, rules->identity().version, rules->identity().content
            })
        rejects(
            [&]
    {
        (void)decode_campaign(changed_identity(saved, identity), *srd5::character_rules(),
        *rules, "fixture-v1", &base);
    });
#ifdef _WIN32
    {
        std::ifstream held(path, std::ios::binary);
        rejects(
            [&]
        {
            write_campaign_file(path, next);
        });
        check(read_campaign_file(path) == saved, "Failed file replacement preserves existing save");
    }
#endif
    auto version = saved;
    version.replace(18, 1, "99");
    rejects(
        [&]
    {
        (void)decode_campaign(version, *srd5::character_rules(), *rules, "fixture-v1", &base);
    });
    auto invalid = party->checkpoint();
    invalid.roster[0].vitals.resources = "SRD11 999 0 0 0 0 0 2 0 0 0 \"\" 0 1 0 0 0 0 FX8 1 0 0";
    party->restore(invalid);
    auto malformed = encode_campaign(*party, nullptr, "fixture-v1");
    rejects(
        [&]
    {
        (void)decode_campaign(malformed, *srd5::character_rules(), *rules, "fixture-v1",
        nullptr);
    });
    party->begin_combat();
    rejects(
        [&]
    {
        (void)encode_campaign(*party, nullptr, "fixture-v1");
    });
    party->end_combat();
    auto busy = prototype();
    rejects(
        [&]
    {
        (void)encode_campaign(*party, &busy, "fixture-v1");
    });
}
} // namespace

int main()
{
    try
    {
        TestDirectory directory;
        roundtrip(directory.path);
        fog_saves(directory.path);
        file_safety(directory.path);
        std::cout << "Campaign file save tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
