#include "opengold/campaign_save.h"
#include "opengold/save_file.h"
#include "opengold/srd5.h"
#include "opengold/combat_demo.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <fstream>
#include <sstream>
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
// These synthetic legacy cases have one member and no grip choice. Locate its
// final field using the same party serialized without a town, then omit it.
void remove_v7_grip(std::string &body, const std::string &party_save)
{
    auto prefix = party_save.substr(party_save.find('\n', party_save.find('\n') + 1) + 1);
    check(prefix.ends_with("1 0 "), "Current fixture has no rest session");
    prefix.resize(prefix.size() - 4);
    // Omit the v8 grant extension before constructing older format fixtures.
    const auto first_grant = prefix.find(" \"feat:savage_attacker\"");
    check(first_grant != prefix.npos, "Soldier fixture includes its creation grant");
    const auto grants_begin = prefix.rfind(' ', first_grant - 1);
    const auto grants_size = prefix.size() - 3 - grants_begin;
    body.erase(grants_begin, grants_size);
    prefix.erase(grants_begin, grants_size);
    check(prefix.ends_with("\" 0 0 ") && body.starts_with(prefix.substr(0, prefix.size() - 2)),
          "Single-member fixture ends in creation source, grip, town flag");
    body.erase(prefix.size() - 4, 2);
    // The version-nine draft adds a training-choice map. This legacy fixture
    // predates those choices and therefore has an empty map.
    std::istringstream in(body);
    std::string text;
    std::uint64_t value{}, count{};
    for (unsigned i = 0; i < 4; ++i)
        in >> std::quoted(text); // identity and assets
    for (unsigned i = 0; i < 12; ++i)
        in >> value; // party slots and scalar state
    in >> count;
    for (unsigned i = 0; i < count; ++i)
        in >> std::quoted(text); // rewards
    in >> value >> value >> count;
    for (unsigned i = 0; i < count; ++i)
        in >> value >> value; // clock/rest
    in >> value;              // roster size
    for (unsigned i = 0; i < 6; ++i)
        in >> std::quoted(text); // draft identities/name
    in >> count;
    for (unsigned i = 0; i < count; ++i)
        in >> std::quoted(text); // class goals
    for (unsigned i = 0; i < 38; ++i)
        in >> value; // six rolls, assignments, adjustment, rolled
    const auto begin = in.tellg();
    in >> count;
    unsigned cantrips{};
    in >> cantrips;
    const auto end = in.tellg();
    check(bool(in) && count == 0 && cantrips == 0,
          "Legacy draft has no chosen training or explicit cantrips");
    body.erase(static_cast<std::size_t>(begin), static_cast<std::size_t>(end - begin));
}
// Versions 1-5 had no sub-minute clock or encounter-scope fields.
void remove_v6_clock(std::string &body)
{
    std::istringstream in(body);
    std::string text;
    for (unsigned i = 0; i < 4; ++i)
        in >> std::quoted(text); // rules identity and assets
    std::uint64_t n{};
    for (unsigned i = 0; i < 12; ++i)
        in >> n; // slots, next ID, selected, minutes, RNG
    in >> n;
    for (std::uint64_t i = 0; i < n; ++i)
        in >> std::quoted(text);
    const auto begin = in.tellg();
    in >> n >> n;
    std::uint64_t count{};
    in >> count;
    for (std::uint64_t i = 0; i < count; ++i)
        in >> n >> n;
    const auto end = in.tellg();
    check(bool(in), "Fixture clock fields exist");
    body.erase(static_cast<std::size_t>(begin), static_cast<std::size_t>(end - begin));
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
                                       static_cast<std::uint8_t>(address >> 8)});
        }
        bytes.insert(bytes.end(),
                     {0, 32, 0, static_cast<std::uint8_t>(destination), 0, 33, 0,
                      static_cast<std::uint8_t>(area), 0, static_cast<std::uint8_t>(area ? 2 : 0),
                      0, static_cast<std::uint8_t>(area ? 255 : 0), 0});
        return std::make_shared<const por::EclProgram>(
            por::EclProgram::decode(bytes, "fog travel fixture"));
    };
    auto resources = std::make_shared<por::PhlanResources>();
    resources->map = por::GeoMap{};
    resources->programs[0] = travel_program(0, 20);
    resources->programs[20] = travel_program(20, 0);
    auto district = std::make_shared<por::PhlanResources>();
    district->map = por::GeoMap{};
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
    check(body.ends_with("1 0 "), "No pending rest in legacy town fixture");
    body.resize(body.size() - 4);
    const auto suffix = "1 0 \"" + single.snapshot().seen.to_string() + "\" ";
    check(body.ends_with(suffix), "Knowledge is the version-five extension");
    body.resize(body.size() - suffix.size());
    remove_v7_grip(body, encode_campaign(*party, nullptr, "fog-fixture"));
    remove_v6_clock(body);
    auto old_base = prototype();
    auto legacy = decode_campaign(campaign_payload(4, body), *srd5::character_rules(), *rules,
                                  "fog-fixture", &old_base);
    check(legacy.town->snapshot().seen == single.snapshot().visited,
          "Version-four migration preserves visits without inventing prior sightlines");
    (void)legacy.town->observe_view();
    check(legacy.town->snapshot().seen == single.snapshot().seen,
          "Displaying the restored view discovers its current sightline");
    for (const auto &invalid :
         {std::string("0 "), "1 0 \"" + std::string(256, '0') + "\" ",
          "1 99 \"" + std::string(256, '1') + "\" ", "1 0 \"" + std::string(255, '1') + "x\" "})
        rejects(
            [&]
            {
                (void)decode_campaign(campaign_payload(5, body + invalid), *srd5::character_rules(),
                                      *rules, "fog-fixture", &old_base);
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
void roundtrip(const std::filesystem::path &directory)
{
    std::filesystem::create_directories(directory);
    // A save from the preceding pack remains valid after additive encounters.
    const auto old_pack = directory / "previous.rules";
    {
        std::ifstream input(std::filesystem::path(OPENGOLD_SOURCE_DIR) /
                            "data/rules/srd-5.2.1/combat.rules");
        std::ofstream output(old_pack, std::ios::binary);
        std::string line;
        while (std::getline(input, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (line.starts_with("damage_types ") || line.starts_with("affinity ") ||
                line.starts_with("saves ") || line.starts_with("spellcasting ") ||
                line.starts_with("creature blindness-adept "))
                continue;
            if (line.starts_with("creature slums-") && !line.starts_with("creature slums-orc "))
                continue;
            output << line << '\n';
        }
    }
    CampaignParty previous(srd5::load(old_pack));
    previous.add_pc(character("fighter"));
    auto old_save = encode_campaign(previous, nullptr, "fixture-v1");
    // Version 3 encoded the same appearance fields without the new filename.
    auto legacy_body = old_save.substr(old_save.find('\n', old_save.find('\n') + 1) + 1);
    check(legacy_body.ends_with("1 0 "), "No pending rest in legacy party fixture");
    legacy_body.resize(legacy_body.size() - 4);
    const std::string portrait_field = "\"human-male-fighter-01.png\" ";
    remove_v7_grip(legacy_body, old_save);
    const auto portrait_position = legacy_body.find(portrait_field);
    check(portrait_position != legacy_body.npos, "Portrait filename is serialized");
    legacy_body.erase(portrait_position, portrait_field.size());
    remove_v6_clock(legacy_body);
    std::uint64_t legacy_hash = 14695981039346656037ULL;
    for (unsigned char c : legacy_body)
    {
        legacy_hash ^= c;
        legacy_hash *= 1099511628211ULL;
    }
    const auto legacy_v3 =
        decode_campaign("OPENGOLD-CAMPAIGN 3\n" + std::to_string(legacy_hash) + "\n" + legacy_body,
                        *srd5::character_rules(), *module(), "fixture-v1", nullptr);
    check(legacy_v3.party.roster[0].character.appearance().portrait.empty(),
          "Version 3 loads without inventing a saved portrait");
    for (const auto *filename : {"../portrait.png", "a/b.png", "a\\b.png", "portrait.jpg"})
        rejects(
            [&]
            {
                por::CharacterAppearance a;
                a.portrait = filename;
                por::validate_character_appearance(a);
            });
    const auto imported =
        decode_campaign(old_save, *srd5::character_rules(), *module(), "fixture-v1", nullptr);
    check(imported.party.roster.size() == 1 &&
              imported.party.roster[0].character.sheet().level == 1,
          "Preceding content pack remains compatible");
    for (unsigned version : {1, 2})
    {
        const auto fixture =
            read_campaign_file(std::filesystem::path(OPENGOLD_SOURCE_DIR) /
                               ("tests/fixtures/campaign-v" + std::to_string(version) + ".ogs"));
        const auto legacy =
            decode_campaign(fixture, *srd5::character_rules(), *module(), "fixture-v1", nullptr);
        check(legacy.party.roster.size() == 1 &&
                  legacy.party.roster[0].character.sheet().level == version,
              "Frozen saves from the old binary migrate without losing levels");
    }
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
    state.roster[1].vitals.resources = "SRD1 0 1 0 0 0";
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
    for (const std::string prior_version :
         {"0.6.0", "0.6.1", "0.6.2", "0.6.3", "0.6.4", "0.6.5", "0.6.6"})
    {
        // Current spell grants cannot masquerade as an older writer's records.
        // Genuine frozen previous-writer fixtures above cover migration instead.
        rejects(
            [&]
            {
                (void)decode_campaign(
                    changed_identity(saved, rules->identity().version, prior_version),
                    *srd5::character_rules(), *rules, "fixture-v1", &base);
            });
    }
    const auto encounter = [](const CampaignParty &p)
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
    check(replacement->member(mage).vitals.resources == "SRD1 0 1 0 0 0",
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
         {rules->identity().module, rules->identity().version, rules->identity().content})
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
    invalid.roster[0].vitals.resources = "SRD1 999 0 0 0 0";
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
void hp_migration()
{
    const auto fixture = read_campaign_file(std::filesystem::path(OPENGOLD_SOURCE_DIR) /
                                            "tests/fixtures/campaign-v6-low-con.ogs");
    auto rules = module();
    const auto creation = srd5::character_rules();
    for (const std::string version : {"0.4.0", "0.5.0", "0.6.0", "0.6.1", "0.6.2"})
    {
        const auto bytes =
            version == "0.6.2" ? fixture : changed_identity(fixture, "0.6.2", version);
        auto loaded = decode_campaign(bytes, *creation, *rules, "hp-history-fixture", nullptr);
        CampaignParty party(module());
        party.restore(std::move(loaded.party));
        const std::array<int, 6> maximum{9, 9, 9, 9, 13, 30}, current{9, 7, 0, 0, 11, 28};
        check(party.state().roster.size() == maximum.size(), "All frozen fixture members migrate");
        for (unsigned i = 0; i < maximum.size(); ++i)
        {
            const auto &member = party.member(i + 1);
            const auto &sources = member.character.sheet().ability_adjustments;
            check(sources.size() == 2 && sources[0].source_id == "background:sage" &&
                      sources[0].level == 1 && sources[0].bonuses[2] == 0 &&
                      sources[1].source_id == "feat:ability_score_improvement" &&
                      sources[1].level == 4 && sources[1].bonuses[2] == 2,
                  "Frozen prior-module saves reconstruct separate background and feat sources");
            check(
                member.character.sheet().hit_points == maximum[i] &&
                    party.profile(i + 1).hit_points == maximum[i],
                "Legacy advancement reconstructs corrected HP, including Dwarf and normal Constitution");
            check(member.vitals.hit_points == current[i] && member.vitals.dead == (i == 3),
                  "Migration preserves health deficits, unconsciousness and death");
            const std::string expected = i == 2   ? "SRD2 0 1 1 1 2 0"
                                         : i == 3 ? "SRD2 0 1 1 1 3 0"
                                                  : "SRD2 0 1 1 0 0 0";
            check(member.vitals.resources == expected,
                  "Migration leaves spell expenditure and death-save counters intact");
        }
        const auto saved = encode_campaign(party, nullptr, "hp-history-fixture");
        auto reloaded = decode_campaign(saved, *creation, *rules, "hp-history-fixture", nullptr);
        CampaignParty restored(module());
        restored.restore(std::move(reloaded.party));
        check(encode_campaign(restored, nullptr, "hp-history-fixture") == saved,
              "New-version reload does not apply the HP migration twice");
        const auto original = party.member(1).vitals;
        auto invalid = original; // 9 HP exceeds the legacy maximum of 6.
        auto old_identity = rules->identity();
        old_identity.version = version;
        rejects(
            [&]
            {
                rules->migrate_character_state(old_identity, party.member(1).character.sheet(),
                                               invalid);
            });
        check(invalid == original, "Rejected legacy state migration is atomic");
    }
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
        hp_migration();
        std::cout << "Campaign file save tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
