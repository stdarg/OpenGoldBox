#include "opengold/champions_records.h"
#include "opengold/ecl.h"
#include "opengold/ecl_machine.h"
#include "opengold/formats.h"
#include "opengold/game_census.h"
#include "opengold/original_rules.h"
#include "opengold/music_render.h"
#include "opengold/por_sound.h"
#include "opengold/start_executable.h"
#include "opengold/original.h"
#include "opengold/character_creator.h"
#include "opengold/coin_purse.h"
#include "opengold/combat_demo.h"
#include "opengold/training_hall.h"
#include "opengold/campaign_session.h"
#include "opengold/campaign_save.h"
#include "opengold/original_combat.h"
#include "opengold/original_spells.h"
#include "opengold/original_content.h"
#include "original_creation_support.h"
#include "support/synthetic_dax.h"
#include "installed_games.h"
#include "test_support.h"
#include "tour_access.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <tuple>

namespace
{
using opengold::test::check;
constexpr auto game = opengold::GoldBoxGame::champions_of_krynn;

std::vector<std::uint8_t> read(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    check(static_cast<bool>(in), "Open " + path.string());
    return {std::istreambuf_iterator<char>(in), {}};
}

void a_knights_independent_fields_survive_decoding()
{
    check(static_cast<unsigned>(opengold::GoldBoxGame::unlimited_adventures) == 6 &&
              static_cast<unsigned>(game) == 7,
          "Existing saved game IDs are never renumbered");
    // types.pas TCharRecord, checked against the installed 409-byte records.
    std::vector<std::uint8_t> raw(409);
    const std::string name = "SOLAMNIC KNIGHT";
    raw[0] = static_cast<std::uint8_t>(name.size());
    std::copy(name.begin(), name.end(), raw.begin() + 1);
    raw[16] = 18;
    raw[17] = 17;
    raw[28] = 64;
    raw[29] = 80;
    raw[30] = 42;
    raw[89] = 47;
    raw[90] = 6;
    raw[91] = 7;
    raw[92] = 3;
    raw[93] = 4;
    raw[94] = 1;
    opengold::test::word(raw, 96, 32);
    raw[98] = 45;
    raw[99 + 22] = 1;
    raw[213] = 12;
    opengold::test::word(raw, 243, 20);
    raw[256] = 7;
    raw[275] = 62;
    raw[278] = 0x40;
    raw[279] = 0xE2;
    raw[280] = 1;
    raw[283] = 30;
    raw[284] = 5;
    raw[294] = 3;
    raw[299] = 2;
    raw[380] = 255;
    raw[391] = 50;
    raw[407] = 38;
    const auto c = opengold::por::decode_character(raw, game);
    check(c && c->game == game && c->raw == raw, "Keep Champions' identity and raw record");
    check(c->name == name && c->race == 6 && c->character_class == 7,
          "Human and knight use Champions' original codes");
    check(c->knight_order == 3 && c->deity == 4 && c->robe_order == 1,
          "Order, deity and robe are independent fields");
    check(c->levels.monk == 7 && c->age == 32 && c->experience == 123456,
          "Slot seven's knight levels, age and experience");
    check(c->base_armor_class == -2 && c->base_thac0 == 13 && c->max_hit_points == 45 &&
              c->current.hit_points_raw == 38 && c->rolled_hit_points == 30,
          "Permanent and current combat numbers");
    check(c->abilities.exceptional_strength == 80 &&
              c->current_abilities.exceptional_strength == 64,
          "Permanent and current exceptional Strength");
    check(c->spell_slots[0][0] == 5 && c->spell_slots[2][0] == 2 && c->deity_spell_slots[0] == 3 &&
              c->spell_book[22] == 1 && c->memorized_spells.front() == 42,
          "Deity gifts do not overwrite normal cleric or mage preparation");
    check(c->wealth[4] == 20 && c->save_bonus == -1 && c->magic_resistance == 50,
          "Raw steel count, signed save adjustment and magic resistance");
    raw.pop_back();
    check(!opengold::champions::decode_creature(raw), "A short creature record is rejected");
}

void repeated_archive_ids_keep_their_payloads_and_first_lookup()
{
    const std::vector<std::uint8_t> first{1, 2}, second{3, 4, 5};
    auto bytes = opengold::test::literal_dax({{14, first}, {14, second}});
    const auto archive = opengold::decode_dax_archive(bytes);
    check(archive.status == opengold::FormatResult::ok && archive.records.size() == 2,
          "Original archives can repeat an ID");
    check(archive.records[0].bytes == first && archive.records[1].bytes == second,
          "Each actual header's payload is decoded independently");
    check(opengold::decode_dax_record(bytes, 14) == first,
          "Indexed lookup takes the first match, like DaxIo.LoadDax");
    // A duplicate does not hide damage to its own header or payload.
    opengold::test::word(bytes, 12, 65535);
    check(opengold::decode_dax_archive(bytes).status == opengold::FormatResult::invalid_data,
          "Validate the later duplicate's bounds too");
}

void actual_champions_records_decode()
{
    const auto folder = opengold::test::installed_game_folder(game);
    if (!std::filesystem::exists(folder / "KRYNN.CFG"))
    {
        std::cout << "Champions' files are not here.\n";
        return;
    }
    check(opengold::identify_game(folder) == game,
          "KRYNN.CFG takes precedence over the shared TILES/SOUND names");
    unsigned count = 0;
    for (unsigned disk = 1; disk <= 3; ++disk)
    {
        const auto archive =
            opengold::decode_dax_archive(read(folder / ("MON" + std::to_string(disk) + "CHA.DAX")));
        check(archive.status == opengold::FormatResult::ok, "Read the real creature archive");
        for (const auto &record : archive.records)
        {
            const auto c = opengold::por::decode_character(record.bytes, game);
            check(c && !c->name.empty() && c->raw == record.bytes,
                  "Decode every real creature without rewriting it");
            if (disk == 1 && record.id == 1)
            {
                check(c->name == "BAAZ" && c->max_hit_points == 9 && c->base_thac0 == 16 &&
                          c->base_armor_class == 4,
                      "The first actual Baaz has its original combat values");
            }
            if (disk == 3 && (record.id == 1 || record.id == 12 || record.id == 24))
            {
                const auto combatant = opengold::original::combatant_from_record(*c);
                using namespace opengold::original;
                if (record.id == 1)
                {
                    check((combatant.traits & (trait::person | trait::unliving | trait::mammal)) ==
                              0,
                          "A Baaz is a reptilian draconian, not a charmable person or undead");
                }
                if (record.id == 12)
                {
                    check(
                        (combatant.traits & trait::person) != 0 && combatant.kin != 0,
                        "The native hobgoblin keeps humanoid targeting and its dwarf/ranger foe flags");
                }
                if (record.id == 24)
                {
                    check((combatant.traits & trait::unliving) != 0 &&
                              (combatant.traits & trait::mammal) == 0,
                          "The death knight's undead bit is distinct from Secret's mammal bit");
                }
            }
            ++count;
        }
    }
    check(count == 88, "All 88 installed creature records decode");
    const auto sprites = read(folder / "SPRIT2.DAX");
    const auto archive = opengold::decode_dax_archive(sprites);
    check(archive.status == opengold::FormatResult::ok, "Read Champions' repeated sprite IDs");
    const auto first = std::ranges::find(archive.records, 14, &opengold::DaxRecord::id);
    check(first != archive.records.end(), "The original repeated ID exists");
    const auto second = std::find_if(std::next(first), archive.records.end(),
                                     [](const opengold::DaxRecord &r)
                                     {
                                         return r.id == 14;
                                     });
    check(second != archive.records.end() && first->bytes != second->bytes &&
              opengold::decode_dax_record(sprites, 14) == first->bytes,
          "Keep both original sprite payloads and choose the first on lookup");

    const auto effects = opengold::por::load_sound_effects(folder, game);
    check(effects.size() == 21, "All 21 original speaker table entries decode");
    const auto image = opengold::por::unpack_sound_executable(
        read(opengold::format::find_start_executable(folder).value()));
    const auto words = opengold::por::decode_item_words(image, game);
    check(words && words->size() == 124 && (*words)[1] == "Battle Axe",
          "Champions has 123 item-name parts, plus the empty index zero");
    const auto music = opengold::load_music_bank(folder, game, 1);
    check(music.songs.size() == 1 && music.instruments.size() == 20,
          "Read the actual score and the driver's program-to-instrument map");
    const auto &song = music.songs.front();
    check(song.tracks.size() == 9 && song.adlib_voice_count == 9 && song.first_adlib_track == 0 &&
              !song.rhythm_enabled,
          "Retain all nine melodic voices rather than dropping the later tracks");
    check(std::ranges::all_of(song.tracks,
                              [](const auto &track)
                              {
                                  return track.length_ticks == 7860;
                              }),
          "The actual score lasts 7860 ticks at the original 54.6 Hz timer rate");
    auto excerpt = music;
    for (auto &track : excerpt.songs.front().tracks)
    {
        std::erase_if(track.events,
                      [](const auto &event)
                      {
                          return event.start_tick >= 100;
                      });
    }
    const auto pcm = opengold::render_music(excerpt, 0, 8000);
    check(!pcm.empty() && std::ranges::any_of(pcm,
                                              [](auto sample)
                                              {
                                                  return sample != 0;
                                              }),
          "The original instruments and note events produce audible native audio");
    const auto drivers = opengold::decode_dax_archive(read(folder / "MIDI.DAX"));
    check(drivers && drivers.records.size() == 5, "All five hardware drivers are present");
    for (const auto &record : drivers.records)
    {
        check(opengold::is_music_driver(record.bytes, game), "Validate each driver's entry table");
        auto broken = record.bytes;
        broken[1] = broken[2] = 0x7F;
        check(!opengold::is_music_driver(broken, game), "Reject a driver with an invalid jump");
    }

    std::shared_ptr<const opengold::rules::CharacterRules> creation =
        opengold::original::character_rules(opengold::original::default_aging, game);
    auto module = opengold::original::rules_module(opengold::load_original_content(folder, game));
    auto party = std::make_shared<opengold::CampaignParty>(std::move(module), creation);
    const auto birth_items = opengold::load_item_definitions(folder, game);
    unsigned seed = 74;
    for (const auto *klass : {"fighter", "fighter", "knight", "cleric", "cleric", "magic_user"})
    {
        opengold::CharacterCreator creator(creation, ++seed);
        creator.select(opengold::rules::CreationField::race, "krynn_human");
        creator.select(opengold::rules::CreationField::character_class, klass);
        creator.name(klass);
        opengold::test::walk_original_creation(creator);
        const auto id = party->add_pc(creator.create_character());
        opengold::outfit_original_character(*party, id, birth_items);
        if (std::string_view{klass} == "knight")
        {
            check(party->member(id).character.inventory().items().size() == 3,
                  "A fresh knight receives all three original creation items");
            const auto carried = party->member(id).character.inventory().items();
            for (const auto &item : carried)
            {
                (void)party->equip(id, item.id);
            }
            check(party->member(id).equipped.size() == 3,
                  "The knight can actually ready all three native birth items");
            for (const auto &item : carried)
            {
                const auto source = party->member(id).item_sources.at(item.id);
                check(
                    opengold::por::item_of_original_key(
                        opengold::por::original_item_key(source.stored))
                            ->type == source.stored.type,
                    "Starting equipment keeps its original bytes for inventory, combat and saves");
            }
        }
        party->add_coins(opengold::Coin::platinum, creator.starting_platinum());
    }
    auto session = opengold::load_campaign(folder, game);
    session.campaign_party(party);
    for (unsigned screen = 0; screen < 100; ++screen)
    {
        session.advance(1);
        const auto &snapshot = session.snapshot();
        check(snapshot.phase != opengold::por::TourPhase::faulted,
              "Champions' opening runs: " + snapshot.diagnostic);
        if (snapshot.phase == opengold::por::TourPhase::completed)
        {
            break;
        }
        if (snapshot.phase == opengold::por::TourPhase::awaiting_continue)
        {
            check(session.choose(snapshot.continue_ticket, 0), "Continue the actual introduction");
        }
    }
    check(
        session.snapshot().phase == opengold::por::TourPhase::completed &&
            session.snapshot().script_id == 17 && session.snapshot().overland &&
            session.snapshot().overland_x == 1 && session.snapshot().overland_y == 3,
        "ECL1:16 803E/8044 moves the new party to its first outpost (script 17), at (1,3): phase " +
            std::to_string(static_cast<int>(session.snapshot().phase)) + " script " +
            std::to_string(session.snapshot().script_id) + " map " +
            std::to_string(session.snapshot().area_id) + " overland " +
            std::to_string(session.snapshot().overland) + " at " +
            std::to_string(session.snapshot().overland_x) + "," +
            std::to_string(session.snapshot().overland_y) + " " + session.snapshot().dialogue +
            " " + session.snapshot().diagnostic);
    const auto view = session.observe_view();
    check(view.width >= 304 && view.height >= 120,
          "Show Champions' original wilderness map and cursor");
    const auto original_x = session.script_variable(opengold::por::EclAddress{0x4BC3});
    opengold::test::TourAccess::set_variable(session, opengold::por::EclAddress{0x4BC3},
                                             original_x + 1);
    const auto moved_marker = session.observe_view();
    check(view.rgba != moved_marker.rgba,
          "The wilderness marker follows the native coordinate cell");
    opengold::test::TourAccess::set_variable(session, opengold::por::EclAddress{0x4BC3},
                                             original_x);
    check(session.script_diagnostics().empty(),
          "The real opening needs no unsupported script operation");
    const auto assets = opengold::campaign_asset_identity(folder);
    const auto saved = opengold::encode_campaign(*party, &session, assets, game);
    check(opengold::saved_campaign_rules(saved).game == game,
          "The normal save identifies Champions with its appended stable game ID");
    const auto restored =
        opengold::decode_campaign(saved, *creation, party->rule_module(), assets, &session);
    check(restored.game == game && restored.party.roster.size() == party->state().roster.size(),
          "The actual campaign and every created character load through the normal save reader");

    // Isolated stronghold fixture: ECL98 keeps GEO97 while its laboratory
    // runs. This grants nothing to the earned campaign.
    opengold::test::TourAccess::enter_script(session, 98, 97,
                                             {11, 2, opengold::por::MapDirection::north});
    for (unsigned screen = 0; screen < 100; ++screen)
    {
        session.advance(0);
        const auto snapshot = session.snapshot();
        if (snapshot.phase == opengold::por::TourPhase::completed)
        {
            break;
        }
        check(snapshot.phase != opengold::por::TourPhase::combat &&
                  session.script_diagnostics().empty(),
              "The isolated stronghold laboratory needs no combat or unsupported event");
        if (snapshot.phase == opengold::por::TourPhase::awaiting_continue)
        {
            check(session.choose(snapshot.continue_ticket, 0),
                  "Read the native laboratory's notice");
        }
    }
    const auto stronghold_save = opengold::encode_campaign(*party, &session, assets, game);
    const auto stronghold = opengold::decode_campaign(stronghold_save, *creation,
                                                      party->rule_module(), assets, &session);
    auto stronghold_party = std::make_shared<opengold::CampaignParty>(
        opengold::original::rules_module(opengold::load_original_content(folder, game)), creation);
    stronghold_party->restore(stronghold.party);
    check(stronghold.town && stronghold.town->snapshot().area_id == 97 &&
              stronghold.town->snapshot().script_id == 98 &&
              opengold::encode_campaign(*stronghold_party, &*stronghold.town, assets, game) ==
                  stronghold_save,
          "The stronghold's ordinary save reloads its retained map and roundtrips byte for byte");

    // Isolated transition regression: a return from Gargath keeps its map
    // while the outpost script offers the inn. This fixture is independent
    // of the earned campaign and grants it no progress or resources.
    opengold::test::TourAccess::enter_script(session, 17, 49,
                                             {11, 15, opengold::por::MapDirection::north});
    for (unsigned screen = 0; screen < 100; ++screen)
    {
        session.advance(0);
        const auto snapshot = session.snapshot();
        check(session.script_diagnostics().empty(), "Return to the actual outpost without errors");
        if (snapshot.phase == opengold::por::TourPhase::completed)
        {
            break;
        }
        if (snapshot.phase == opengold::por::TourPhase::awaiting_continue)
        {
            const auto inn = std::ranges::find(snapshot.choices, "INN");
            const auto choice = inn == snapshot.choices.end()
                                    ? 0
                                    : static_cast<unsigned>(inn - snapshot.choices.begin());
            check(session.choose(snapshot.continue_ticket, choice), "Accept the outpost's inn");
        }
    }
    check(session.snapshot().camp_offered && session.snapshot().script_id == 17 &&
              session.snapshot().area_id == 49,
          "An inn offer waits for rest instead of consuming the next outpost decision");
    check(session.camp(opengold::RestKind::long_rest), "Rest at the actual outpost inn");
    bool returned_to_menu = false;
    for (unsigned screen = 0; screen < 100; ++screen)
    {
        if (party->state().spell_rest)
        {
            const auto choosing = party->state().spell_rest->members;
            for (const auto id : choosing)
            {
                party->keep_rest_spells(id);
            }
        }
        session.advance(0);
        const auto &snapshot = session.snapshot();
        check(session.script_diagnostics().empty() &&
                  snapshot.phase != opengold::por::TourPhase::combat,
              "The inn's safe rest does not invoke a wilderness encounter");
        if (snapshot.phase == opengold::por::TourPhase::awaiting_continue &&
            std::ranges::find(snapshot.choices, "INN") != snapshot.choices.end())
        {
            returned_to_menu = true;
            break;
        }
        if (snapshot.phase == opengold::por::TourPhase::awaiting_continue)
        {
            check(session.choose(snapshot.continue_ticket, 0), "Finish the inn's rest");
        }
    }
    check(returned_to_menu && session.snapshot().script_id == 17,
          "Finishing the inn's rest returns to the outpost menu with its retained district");
    const auto choices = session.snapshot().choices;
    const auto leave = std::ranges::find(choices, "LEAVE");
    check(leave != choices.end() && session.choose(session.snapshot().continue_ticket,
                                                   static_cast<unsigned>(leave - choices.begin())),
          "Leave the actual outpost for the wilderness");
    for (unsigned screen = 0; screen < 100; ++screen)
    {
        session.advance(0);
        const auto snapshot = session.snapshot();
        if (snapshot.phase == opengold::por::TourPhase::completed)
        {
            break;
        }
        if (snapshot.phase == opengold::por::TourPhase::awaiting_continue)
        {
            const auto no = std::ranges::find(snapshot.choices, "NO");
            check(session.choose(snapshot.continue_ticket,
                                 no == snapshot.choices.end()
                                     ? 0
                                     : static_cast<unsigned>(no - snapshot.choices.begin())),
                  "Decline to enter the outpost again");
        }
    }
    check(session.snapshot().script_id == 16 && session.snapshot().area_id == 49,
          "The wilderness retains Gargath's district after leaving the outpost");
    const auto campsite = session.detached_copy();
    const auto rested_party = party->checkpoint();
    bool encountered = false;
    for (unsigned dice_seed = 1; dice_seed <= 8 && !encountered; ++dice_seed)
    {
        session = campsite.detached_copy();
        party->restore(rested_party);
        session.attach_restored_party(party);
        session.reseed_dice(dice_seed);
        check(session.camp(opengold::RestKind::long_rest), "Camp in the actual wilderness");
        for (unsigned screen = 0; screen < 100; ++screen)
        {
            if (party->state().spell_rest)
            {
                const auto choosing = party->state().spell_rest->members;
                for (const auto id : choosing)
                {
                    party->keep_rest_spells(id);
                }
            }
            session.advance(0);
            const auto snapshot = session.snapshot();
            check(session.script_diagnostics().empty(),
                  "Wilderness creatures load from disk 1 instead of the retained dungeon");
            if (snapshot.phase == opengold::por::TourPhase::combat)
            {
                encountered = true;
                break;
            }
            if (snapshot.phase == opengold::por::TourPhase::completed)
            {
                break;
            }
            if (snapshot.phase == opengold::por::TourPhase::awaiting_continue)
            {
                check(session.choose(snapshot.continue_ticket, 0), "Meet the camp's encounter");
            }
        }
    }
    check(encountered, "The native wilderness camp stages an actual combat on the retained map");

    // Isolated post-combat fixture, separate from the earned campaign proof:
    // provide a victory result to exercise actual wilderness loot dispatch.
    auto result = [&]
    {
        opengold::CombatDemo combat(
            opengold::original::rules_module(opengold::load_original_content(folder, game)));
        combat.campaign_party(party);
        combat.encounter(*session.pending_encounter(), 7);
        return combat.combat().snapshot();
    }();
    result.outcome = opengold::rules::Outcome::victory;
    for (auto &unit : result.combatants)
    {
        if (unit.side == opengold::rules::Side::opposition)
        {
            unit.hit_points = 0;
            unit.dead = true;
            unit.conscious = false;
        }
    }
    check(session.resolve_combat(result) && session.script_diagnostics().empty() &&
              session.snapshot().phase != opengold::por::TourPhase::faulted,
          "Wilderness victory reads disk-1 monster loot despite the retained disk-2 map");
}

void the_gold_dragons_camp_waits_for_the_players_departure()
{
    using namespace opengold;
    const auto folder = opengold::test::installed_game_folder(game);
    if (!std::filesystem::exists(folder / "KRYNN.CFG"))
    {
        return;
    }
    const std::shared_ptr<const rules::CharacterRules> creation =
        original::character_rules(original::Aging::original, game);
    auto party = std::make_shared<CampaignParty>(
        original::rules_module(load_original_content(folder, game)), creation);
    CharacterCreator creator(creation, 47);
    creator.select(rules::CreationField::race, "krynn_human");
    creator.select(rules::CreationField::character_class, "knight");
    creator.name("Flight knight");
    test::walk_original_creation(creator);
    (void)party->add_pc(creator.create_character());
    auto session = load_campaign(folder, game);
    session.campaign_party(party);
    const auto settle = [&]
    {
        for (unsigned screen = 0; screen < 200; ++screen)
        {
            if (party->state().spell_rest)
            {
                const auto choosing = party->state().spell_rest->members;
                for (const auto id : choosing)
                {
                    party->keep_rest_spells(id);
                }
            }
            session.advance(0);
            const auto snapshot = session.snapshot();
            check(
                session.script_diagnostics().empty() && snapshot.phase != por::TourPhase::combat &&
                    snapshot.phase != por::TourPhase::faulted,
                "The isolated actual flight and its safe camp need no combat or unsupported event");
            if (snapshot.phase == por::TourPhase::completed)
            {
                return;
            }
            if (snapshot.phase == por::TourPhase::awaiting_continue)
            {
                const auto yes = std::ranges::find(snapshot.choices, "YES");
                check(session.choose(snapshot.continue_ticket,
                                     yes == snapshot.choices.end()
                                         ? 0
                                         : static_cast<unsigned>(yes - snapshot.choices.begin())),
                      "Accept the actual dragon's invitation to rest");
            }
        }
        check(false, "The flight's dialogue settles");
    };
    settle();
    // Isolated native script fixture; no earned route uses this placement.
    test::TourAccess::enter_script(session, 96, 96, {0, 8, por::MapDirection::north});
    settle();
    const auto offered = session.detached_copy();
    const auto before_rest = party->checkpoint();
    const auto still_flying = [&]
    {
        check(session.snapshot().camp_offered &&
                  session.script_variable(por::EclAddress{0x4C0A}) == 1 &&
                  session.snapshot().pose != por::PartyPose{6, 14, por::MapDirection::north},
              "The gold dragon's safe camp stays before the citadel landing");
    };
    still_flying();
    for (unsigned night = 0; night < 2; ++night)
    {
        check(session.camp(RestKind::long_rest), "Rest another night on the gold dragon");
        settle();
        still_flying();
    }
    const auto bytes = encode_campaign(*party, &session, "dragon-rest-fixture", game);
    auto restored =
        decode_campaign(bytes, *creation, party->rule_module(), "dragon-rest-fixture", &session);
    party->restore(restored.party);
    session = std::move(*restored.town);
    session.attach_restored_party(party);
    check(encode_campaign(*party, &session, "dragon-rest-fixture", game) == bytes,
          "An ordinary save retains the offered flying camp byte for byte");
    still_flying();
    session.decline_camp();
    settle();
    check(session.snapshot().pose == por::PartyPose{6, 14, por::MapDirection::north} &&
              session.script_variable(por::EclAddress{0x4C0A}) == 0,
          "Leaving the camp after rest and reload tells the landing once");
    session = offered.detached_copy();
    party->restore(before_rest);
    session.attach_restored_party(party);
    session.decline_camp();
    settle();
    check(session.snapshot().pose == por::PartyPose{6, 14, por::MapDirection::north},
          "Leaving without resting also continues the actual flight");

    // Isolated offered-chamber fixture: the earned route reaches these flags
    // and this pose through the native stairs and windcaptain battle.
    session = offered.detached_copy();
    party->restore(before_rest);
    session.attach_restored_party(party);
    test::TourAccess::place(session, {11, 11, por::MapDirection::east});
    test::TourAccess::set_variable(session, por::EclAddress{0x4C0A}, 0);
    test::TourAccess::set_variable(session, por::EclAddress{0x4C07}, 1);
    check(session.camp(RestKind::long_rest), "Take the windcaptain's offered safe rest");
    settle();
    check(session.snapshot().camp_offered &&
              session.script_variable(por::EclAddress{0x4C07}) == 1 &&
              session.snapshot().pose == por::PartyPose{11, 11, por::MapDirection::east},
          "Resting in the chamber waits for the player before the counterattack");
    session.decline_camp();
    session.advance(0);
    check(session.snapshot().phase == por::TourPhase::awaiting_continue &&
              session.script_variable(por::EclAddress{0x4C07}) == 0 &&
              session.snapshot().dialogue.find("SOME TIME PASSES AS TAS MANEUVERS") !=
                  std::string::npos,
          "Leaving the windcaptain's camp resumes the native counterattack");
}

void spell_queries_preserve_preparations_until_the_script_spends_one()
{
    using namespace opengold;
    const std::shared_ptr<const rules::CharacterRules> creation =
        original::character_rules(original::Aging::original, game);
    original::Content content;
    content.game = game;
    auto party =
        std::make_shared<CampaignParty>(original::rules_module(std::move(content)), creation);
    for (unsigned n = 0; n < 2; ++n)
    {
        CharacterCreator creator(creation, 72 + n);
        creator.select(rules::CreationField::race, "krynn_human");
        creator.select(rules::CreationField::character_class, "cleric");
        creator.name("Priest " + std::to_string(n));
        test::walk_original_creation(creator);
        (void)party->add_pc(creator.create_character());
    }
    // A synthetic prepared-spell fixture isolates the actual ECL protocol.
    // It is independent of the earned campaign and awards it no resources.
    auto state = party->checkpoint();
    for (std::size_t n = 0; n < state.roster.size(); ++n)
    {
        auto profile = *original::Combatant::decode(
            party->rule_module().character_profile(state.roster[n].character.sheet(), {}).data);
        profile.spells = n == 0 ? std::vector<int>{1} : std::vector<int>{1, 22, 22};
        state.roster[n].vitals.resources = profile.encode();
    }
    party->restore(std::move(state));
    const auto query = [](std::uint8_t number)
    {
        return std::vector<std::uint8_t>{59, 0, number, 1, 0x79, 0x7F, 1, 0x7B, 0x7F};
    };
    const auto run = [&](std::vector<std::uint8_t> body)
    {
        std::vector<std::uint8_t> bytes{0, 0};
        for (unsigned entry = 0; entry < 5; ++entry)
        {
            bytes.insert(bytes.end(), {1, 1, 0x14, 0x80});
        }
        bytes.insert(bytes.end(), body.begin(), body.end());
        bytes.push_back(0);
        auto program = std::make_shared<const por::EclProgram>(
            por::EclProgram::decode(bytes, "Champions spell query", game));
        auto resources = std::make_shared<por::PhlanResources>();
        resources->opening_script = 1;
        por::RolfTourSession session({}, std::move(program), {}, 0x8014, {}, resources);
        session.campaign_party(party);
        for (unsigned n = 0; n < 100 && session.snapshot().phase == por::TourPhase::running; ++n)
        {
            session.advance(1);
        }
        check(session.snapshot().phase == por::TourPhase::completed,
              "The spell query and selected-character write finish: " +
                  session.snapshot().diagnostic);
        const auto machine = test::TourAccess::machine(session);
        return std::pair{machine.variable(por::EclAddress{0x7F79}),
                         machine.variable(por::EclAddress{0x7F7B})};
    };
    const auto before = party->checkpoint();
    check(run(query(22)) == std::pair{std::uint16_t{1}, std::uint16_t{1}} &&
              party->member(before.slots[1]).vitals.resources == before.roster[1].vitals.resources,
          "Find Traps locates the exact prepared slot and caster without consuming a copy");
    check(run(query(22)).first == 1 &&
              party->member(before.slots[1]).vitals.resources == before.roster[1].vitals.resources,
          "Declining to cast permits another lookup of the same prepared copy");
    std::vector<std::uint8_t> spend{10, 0, 1,                 // LOAD CHARACTER 1
                                    9,  0, 0,  1, 0x21, 0x7C, // Clear prepared slot 1.
                                    10, 0, 129};              // Store selected character 1.
    const auto again = query(22);
    spend.insert(spend.end(), again.begin(), again.end());
    check(run(spend) == std::pair{std::uint16_t{1}, std::uint16_t{1}} &&
              party->memorized_spells(before.slots[1]).size() == 2 &&
              party->member(before.slots[0]).vitals.resources == before.roster[0].vitals.resources,
          "Choosing to cast spends exactly one copy from the selected priest");
    check(run(spend).first == 255 && party->memorized_spells(before.slots[1]).size() == 1,
          "No copy remains after the second actual cast; the no-match sentinel is 255");
    check(run(query(31)).first == 255, "An entirely unprepared spell is a normal no-match result");
}

void native_strength_can_force_the_required_wizard_locked_door()
{
    using namespace opengold;
    const std::shared_ptr<const rules::CharacterRules> creation =
        original::character_rules(original::Aging::original, game);
    original::Content content;
    content.game = game;
    CampaignParty party(original::rules_module(content), creation);
    CharacterCreator creator(creation, 37);
    creator.select(rules::CreationField::race, "krynn_human");
    creator.select(rules::CreationField::character_class, "knight");
    creator.name("Door knight");
    test::walk_original_creation(creator);
    const auto id = party.add_pc(creator.create_character());
    test::modify_to_the_highest(party, id);
    auto sheet = party.member(id).character.sheet();
    auto module = original::rules_module(content);
    // Isolated Strength-table fixtures, including the exceptional boundary
    // and scores a strength item or spell can give. No campaign is advanced.
    for (const auto &[strength, exceptional, faces, sides] :
         {std::tuple{17, 0, 0U, 6U}, std::tuple{18, 90, 0U, 6U}, std::tuple{18, 91, 1U, 6U},
          std::tuple{18, 100, 2U, 6U}, std::tuple{19, 0, 3U, 6U}, std::tuple{21, 0, 4U, 6U},
          std::tuple{23, 0, 5U, 6U}, std::tuple{24, 0, 7U, 8U}, std::tuple{25, 0, 1U, 1U}})
    {
        sheet.scores[rules::Ability::strength] = strength;
        sheet.original->exceptional_strength = exceptional;
        check(module->check_chance(sheet, {}, {}, rules::Ability::strength, "athletics", 35) ==
                  faces * 100 / sides,
              "Krynn's magical-door chance follows its exceptional Strength table");
        RandomState random{45};
        for (unsigned attempt = 0; attempt < 64; ++attempt)
        {
            const auto roll = module->roll_ability_check_against(
                sheet, {}, {}, rules::Ability::strength, "athletics", 35, random);
            check(
                (roll.total >= 35) == (faces && roll.die <= static_cast<int>(faces)) &&
                    (faces ? roll.die >= 1 && roll.die <= static_cast<int>(sides) : roll.die == 0),
                "The actual door roll uses the native die and success threshold");
        }
    }
    bool forced = false;
    for (unsigned attempt = 0; attempt < 64 && !forced; ++attempt)
    {
        const auto results = party.try_door(DoorMethod::bash, 35);
        forced = std::ranges::any_of(results,
                                     [](const auto &result)
                                     {
                                         return result.roll.total >= result.difficulty;
                                     });
    }
    check(forced, "A legal 18/00 knight can open a wizard-locked door through CampaignParty");
}

void a_top_level_return_ends_the_event_before_embedded_data()
{
    using namespace opengold;
    std::vector<std::uint8_t> bytes{0, 0};
    for (unsigned entry = 0; entry < 5; ++entry)
    {
        bytes.insert(bytes.end(), {1, 1, 0x14, 0x80});
    }
    // Script 81 ends a directly entered room event with RETURN. Its tables
    // follow; interpreting those bytes as another instruction is a fault.
    bytes.insert(bytes.end(), {19, 255});
    auto program = std::make_shared<const por::EclProgram>(
        por::EclProgram::decode(bytes, "Champions return", game));
    por::EclMachine machine(std::move(program));
    check(machine.start(0) && machine.run(10).state == por::EclState::completed,
          "Champions' empty return stack ends the event before embedded data");
}

void malformed_scores_are_rejected()
{
    std::vector<std::uint8_t> bytes(125);
    bytes[0] = 125;
    bytes[34] = 1;
    const std::vector<std::uint8_t> events{0xC1, 1,  0, 0x91, 60, 127, 5, 64,   100, 5,
                                           0x81, 60, 0, 0,    64, 0,   2, 0xFC, 0x81};
    bytes.insert(bytes.end(), events.begin(), events.end());
    const auto decoded = opengold::decode_music_bank(bytes, game);
    check(decoded && decoded->songs.front().tracks.front().length_ticks == 12,
          "Running status, voice replacement, note-off and final delay are decoded");
    bytes.pop_back();
    check(!opengold::decode_music_bank(bytes, game), "Reject a missing end mode");
    bytes.pop_back();
    check(!opengold::decode_music_bank(bytes, game), "Reject a missing end marker");
    bytes.resize(126);
    bytes.back() = 60;
    check(!opengold::decode_music_bank(bytes, game), "Reject data before a running status");
}

void native_creation_keeps_krynns_choices_and_progression()
{
    using namespace opengold;
    using namespace opengold::original;
    const auto creation = character_rules(Aging::original, game);
    const auto offered = creation->choices(rules::CreationField::race);
    check(offered.size() == 7 && offered.front().id == "silvanesti_elf",
          "Champions offers its seven races and preserves their order");
    check(character_rules()->choices(rules::CreationField::race).size() == 6,
          "Pool's existing race menu retains its six choices");
    RandomState random{17};
    for (const auto &race : offered)
    {
        rules::CharacterDraft draft;
        draft.name = "Krynn Hero";
        draft.race = race.id;
        draft.gender = "male";
        for (const auto &profession : creation->choices(rules::CreationField::character_class))
        {
            if (!creation->allows(draft, rules::CreationField::character_class, profession.id))
            {
                continue;
            }
            draft.character_class = profession.id;
            draft.background =
                (race.id == "mountain_dwarf" || race.id == "hill_dwarf") ? "reorx" : "mishakal";
            draft.alignment = "lawful_good";
            if (!creation->allows(draft, rules::CreationField::alignment, draft.alignment))
            {
                draft.alignment = "neutral";
            }
            if (!creation->allows(draft, rules::CreationField::alignment, draft.alignment))
            {
                draft.alignment = "neutral_good";
            }
            // A deity choice constrains a cleric; it is absent on other classes.
            if (!creation->allows(draft, rules::CreationField::background, draft.background))
            {
                draft.background.clear();
            }
            if (!creation->allows(draft, rules::CreationField::alignment, draft.alignment))
            {
                draft.alignment = "lawful_good";
            }
            creation->roll_in_place(draft, random);
            const auto sheet = creation->evaluate(draft, rules::NameRequirement::required);
            check(sheet.original && sheet.hit_points > 0 && game_of(sheet.identity) == game,
                  "Every eligible race/class creates a live Champions character");
            const auto &n = *sheet.original;
            const auto raw_race = record_race(static_cast<Race>(n.race), game);
            check(raw_race < 7 && race_from_record(raw_race, game) == static_cast<Race>(n.race),
                  "Race codes round-trip without changing old games' values");
            if (profession.id == "knight")
            {
                check(n.knight_order == 1 && n.class_levels[7] == 1 && n.experience == 2500 &&
                          sheet.level_classes.front() == "knight",
                      "A new knight starts in the Crown at level one, never as a monk");
            }
            if (profession.id == "magic_user")
            {
                check(n.class_levels[5] == 2 && n.robe_order != 0 &&
                          std::ranges::find(n.known_spells, 15) != n.known_spells.end(),
                      "A mage starts at two with its robe and actual spell book");
            }
            const auto imported = creation->imported_draft(sheet, n.experience);
            check(imported.has_value(), "A native Champions character can be imported");
            const auto restored = creation->evaluate(*imported, rules::NameRequirement::required);
            check(restored.original->deity == n.deity &&
                      restored.original->robe_order == n.robe_order &&
                      restored.original->knight_order == n.knight_order &&
                      restored.hit_points == sheet.hit_points,
                  "Imported draft versions retain deity, robe, order and hit points");
        }
    }
    check(champions_experience_for_level(ClassSlot::cleric, 2, {4, 0, 0}) == 4000 &&
              champions_experience_for_level(ClassSlot::cleric, 2, {6, 0, 0}) == 3000 &&
              champions_experience_for_level(ClassSlot::monk, 4, {0, 0, 1}) == 18500 &&
              champions_experience_for_level(ClassSlot::monk, 4, {0, 0, 3}) == 27000,
          "Deities and knight orders select their actual training progressions");
    check(original_training_fee(game) == 0, "Champions trains at its halls without a fee");
    rules::AbilityArray<int> prime;
    prime[rules::Ability::wisdom] = 15;
    prime[rules::Ability::strength] = 18;
    check(
        experience_share(Profession::cleric, prime, 100, game) == 110 &&
            experience_share(Profession::knight, prime, 100, game) == 100,
        "Champions clerics receive their combat bonus at Wisdom fifteen; knights have no prime-score bonus");
}

void steel_prices_and_change_conserve_value()
{
    using namespace opengold;
    const CoinRates steel{{1, 5, 10, 25, 50}, 4};
    Purse purse{0, 0, 5, 0, 3, 0, 0};
    check(gold_value(purse, steel) == 4, "Five bronze and three steel are worth four steel");
    const auto changed = change_coins_up(purse, steel);
    check(gold_value(changed, steel) == 4 && changed[4] == 4,
          "Bronze-to-platinum change uses exact multiples and creates no money");
    const auto payment = pay_gold(purse, 3, steel);
    check(gold_value(payment.purse, steel) == 1, "A three-steel price takes exactly three steel");
    check(original::record_race(original::Race::human, GoldBoxGame::pool_of_radiance) == 7,
          "The old human record code remains seven");
}

void hit_points_and_deity_changes_follow_the_actual_rules()
{
    using namespace opengold;
    using namespace opengold::original;
    ClassLevels knight{};
    knight[7] = 1;
    const ModifiedCharacter crown{
        Race::krynn_human, Gender::male, Profession::knight, knight, {0, 0, 1}};
    check(modify_hit_point_maximum(crown, 18, game) == 28 &&
              modify_hit_point_minimum(crown, 18, game) == 6 &&
              modified_rolled_hit_points(crown, 18, 28, 10, game) == 20,
          "A first-level knight has two d10 and the warrior Constitution bonus, not monk dice");
    ClassLevels cleric{};
    cleric[0] = 1;
    const ModifiedCharacter good{
        Race::krynn_human, Gender::male, Profession::cleric, cleric, {4, 0, 0}};
    const ModifiedCharacter neutral{
        Race::krynn_human, Gender::male, Profession::cleric, cleric, {6, 0, 0}};
    check(modify_hit_point_maximum(good, 16, game) == 20 &&
              modify_hit_point_maximum(neutral, 16, game) == 10,
          "Good-deity clerics start with two d8; neutral-deity clerics have one");
    std::shared_ptr<const rules::CharacterRules> creation = character_rules(default_aging, game);
    CharacterCreator creator(creation, 61);
    creator.select(rules::CreationField::race, "krynn_human");
    creator.name("Devotee");
    creator.next();
    creator.next();
    creator.next();
    creator.roll();
    check(creator.draft().rolled, "The good cleric has rolled");
    creator.select(rules::CreationField::background, "reorx");
    check(!creator.draft().rolled && creator.draft().alignment == "lawful_neutral",
          "Changing deity updates the allowed alignment and invalidates its different hit dice");
    creator.roll();
    const auto sheet = creator.sheet();
    check(sheet.original->deity == 6 && sheet.original->experience == 1500 &&
              creator.starting_gold() == 0 && creator.starting_platinum() > 0,
          "The rerolled Reorx cleric has its own experience and starting steel");
    Content content;
    content.game = game;
    const auto module = rules_module(content);
    const auto ready = module->classes_to_train(sheet, 3000);
    check(ready == std::vector<std::string>{"cleric"},
          "Neutral-deity clerics can train at 3000 experience");
}

void knight_petitions_apply_the_next_orders_progression()
{
    using namespace opengold;
    using namespace opengold::original;
    Content content;
    content.game = game;
    const auto module = rules_module(std::move(content));
    rules::CharacterSheet knight;
    knight.identity = identity(game);
    knight.name = "Knight";
    knight.level = 4;
    knight.hit_points = 50;
    knight.level_classes = {"knight", "knight", "knight", "knight"};
    for (const auto ability : rules::all_abilities)
    {
        knight.base[ability] = knight.scores[ability] = 18;
    }
    knight.original = opengold::rules::CharacterSheet::Original{};
    knight.original->race = static_cast<int>(Race::krynn_human);
    knight.original->profession = static_cast<int>(Profession::knight);
    knight.original->class_levels[7] = 4;
    knight.original->knight_order = 1;
    knight.original->rolled_hit_points = 30;
    knight.original->experience = 11999;
    const std::array<std::string, 1> classes{"knight"};
    check(module->hall_training_options(knight, classes).options.empty(),
          "A Crown knight cannot petition before earning the Sword's minimum experience");
    knight.original->experience = 18500;
    const auto sword = module->hall_training_options(knight, classes).options;
    check(sword.size() == 1, "A qualified Crown knight can petition for the Sword");
    module->train_at_hall(knight, sword.front().id);
    check(
        knight.original->knight_order == 2 && knight.original->class_levels[7] == 3 &&
            knight.original->rolled_hit_points == 24 && knight.hit_points == 40 &&
            knight.level_classes.size() == 3 && knight.level == 3,
        "Changing order loses the unaffordable fourth level, its rolled HP and warrior Constitution bonus");
    check(module->hall_training_options(knight, classes).options.empty(),
          "The Sword knight must earn level four before petitioning for the Rose");
    knight.original->class_levels[7] = 4;
    knight.level = 4;
    knight.level_classes.push_back("knight");
    knight.original->experience = 27000;
    knight.base[rules::Ability::constitution] = 14;
    check(module->hall_training_options(knight, classes).options.empty(),
          "Rose petition checks its higher minimum base Constitution");
    knight.base[rules::Ability::constitution] = 18;
    const auto rose = module->hall_training_options(knight, classes).options;
    check(rose.size() == 1, "A qualified Sword knight can petition for the Rose");
    module->train_at_hall(knight, rose.front().id);
    check(
        knight.original->knight_order == 3 && knight.original->class_levels[7] == 4 &&
            knight.original->cleric_slots[0] == 0 &&
            champions_spell_slots(ClassSlot::monk, 6, 18, {0, 0, 3})[0] == 1 &&
            module->hall_training_options(knight, classes).options.empty(),
        "A Rose knight keeps an affordable level, gains spells at level six and cannot promote further");
}

void knight_order_changes_replay_after_a_lost_and_regained_level()
{
    using namespace opengold;
    using namespace opengold::original;
    std::shared_ptr<const rules::CharacterRules> creation = character_rules(Aging::original, game);
    Content content;
    content.game = game;
    CampaignParty party(rules_module(content), creation);
    CharacterCreator creator(creation, 121);
    creator.select(rules::CreationField::race, "krynn_human");
    creator.select(rules::CreationField::character_class, "knight");
    creator.name("Knight");
    test::walk_original_creation(creator);
    const auto id = party.add_pc(creator.create_character());
    test::modify_to_the_highest(party, id);
    for (const auto amount : {2500U, 5000U, 8500U})
    {
        party.award_experience(amount, "training-" + std::to_string(amount));
        party.train_at_hall_level(id, 0x10, {});
    }
    check(party.member(id).character.sheet().original->class_levels[7] == 4,
          "The native knight earns and trains its Crown levels through ordinary party commands");
    const auto sword = party.hall_lessons(id, 0x10).options;
    check(sword.size() == 1, "The earned knight qualifies for its Sword petition");
    const auto money = party.state().wealth;
    party.train_hall_lesson(id, 0x10, sword.front().id);
    check(party.member(id).character.sheet().original->class_levels[7] == 3 &&
              party.state().wealth == money,
          "The hall applies the original free promotion and its level loss");
    party.award_experience(10000, "regained level");
    party.train_at_hall_level(id, 0x10, {});
    const auto rose = party.hall_lessons(id, 0x10).options;
    check(rose.size() == 1, "The knight's regained fourth level qualifies for its Rose petition");
    party.train_hall_lesson(id, 0x10, rose.front().id);
    const auto &before = party.member(id);
    check(before.character.sheet().original->knight_order == 3 &&
              before.character.sheet().original->class_levels[7] == 4,
          "The knight finishes as a fourth-level Rose knight");
    const auto saved = encode_campaign(party, nullptr, "knight history", game);
    const auto loaded =
        decode_campaign(saved, *creation, party.rule_module(), "knight history", nullptr);
    CampaignParty restored(rules_module(content), creation);
    restored.restore(loaded.party);
    check(encode_campaign(restored, nullptr, "knight history", game) == saved &&
              restored.member(id).character.sheet().original->knight_order == 3 &&
              restored.member(id).character.sheet().hit_points ==
                  before.character.sheet().hit_points &&
              restored.member(id).vitals.hit_points == before.vitals.hit_points,
          "Normal saves replay petitions, the lost level and regained training byte-identically");
}

void deity_gifts_have_their_own_shared_preparations()
{
    using namespace opengold;
    using namespace opengold::original;
    const auto creation = character_rules(Aging::original, game);
    Content content;
    content.game = game;
    const auto module = rules_module(std::move(content));
    RandomState random{271};
    static constexpr std::array<std::string_view, 7> gods{
        "paladine", "majere", "kiri_jolith", "mishakal", "sirrion", "reorx", "shinare"};
    for (std::size_t god = 0; god < gods.size(); ++god)
    {
        rules::CharacterDraft draft;
        draft.name = "Priest";
        draft.race = "krynn_human";
        draft.gender = "male";
        draft.character_class = "cleric";
        draft.background = gods[god];
        draft.alignment = god < 4 ? "lawful_good" : "neutral";
        creation->roll_in_place(draft, random);
        auto sheet = creation->evaluate(draft, rules::NameRequirement::required);
        const int quota = god == 3 ? 3 : god == 5 ? 0 : 1;
        check(sheet.original->deity_spell_slots[0] == quota,
              "The chosen god supplies the original separate gift quota");
        const auto options =
            module->spell_choice_options(sheet, rules::SpellChoiceContext::long_rest);
        std::vector<std::string> gifts;
        for (const auto &choice : options.preparation)
        {
            if (choice.id.starts_with("deity:"))
            {
                gifts.push_back(choice.id);
            }
        }
        check(gifts.size() == static_cast<std::size_t>(god == 3 ? 9 : quota),
              "Mishakal's three gifts share three slots; the other gods offer only their own gift");
        if (gifts.empty())
        {
            continue;
        }
        rules::SpellChoices choices;
        choices.prepared = std::vector<std::string>{gifts.front(), "cleric:bless"};
        module->apply_spell_choices(sheet, choices, rules::SpellChoiceContext::long_rest,
                                    rules::ChoiceCompleteness::complete);
        const auto profile = Combatant::decode(module->character_profile(sheet, {}).data);
        const auto gift = spell_number(gifts.front(), game);
        check(profile && gift && std::ranges::find(profile->spells, *gift) != profile->spells.end(),
              "Native record export retains a prepared deity gift's original spell number");
        if (god == 3)
        {
            choices.prepared = std::vector<std::string>{"deity:bless", "deity:bless#2",
                                                        "deity:bless#3", "cleric:bless"};
            module->apply_spell_choices(sheet, choices, rules::SpellChoiceContext::long_rest,
                                        rules::ChoiceCompleteness::complete);
            rules::VitalState own{sheet.hit_points, false, {}, {}};
            std::vector<rules::CampTarget> party{{sheet, own}};
            module->use_party_camp_action(sheet, own, party, "cast:deity:bless", random);
            const auto used = Combatant::decode(own.resources);
            check(
                used && std::ranges::count(used->spells, 505) == 2 &&
                    std::ranges::count(used->spells, 1) == 1,
                "Casting a deity gift consumes one gift and preserves ordinary cleric preparations");
            choices.prepared->push_back("deity:charm_person");
            bool refused = false;
            try
            {
                module->apply_spell_choices(sheet, choices, rules::SpellChoiceContext::long_rest,
                                            rules::ChoiceCompleteness::complete);
            }
            catch (const std::runtime_error &)
            {
                refused = true;
            }
            check(refused, "Different Mishakal gifts share one quota of three");
        }
    }
    const auto finishes = module->preparation_finishes(std::array<unsigned, 2>{0, 1});
    check(finishes == std::vector<std::chrono::minutes>{std::chrono::minutes{255},
                                                        std::chrono::minutes{270}},
          "Level-zero gifts take fifteen minutes after normal rest, like first-level spells");
    check(canonical_spell(101, game) == 501 && spell_id(501).starts_with("deity:") &&
              canonical_spell(101, GoldBoxGame::pools_of_darkness) == 301,
          "Champions gifts keep stable distinct IDs without changing other games' spells");
}

void native_monster_conditions_use_champions_durations_and_immunities()
{
    using namespace opengold;
    using namespace opengold::original;
    const auto suffer = [](int power, Race race, unsigned seed, bool yelling = false)
    {
        por::CharacterRecord raw;
        raw.game = game;
        raw.race = 7;
        raw.name = "Monster";
        raw.max_hit_points = 200;
        raw.current.hit_points_raw = 200;
        raw.base_thac0 = 0;
        raw.base_armor_class = 0;
        raw.base_movement = 12;
        raw.saves.spells = 21;
        raw.base_attacks[0] = {2, {1, 2, 0}};
        raw.current_abilities = raw.abilities = {18, 10, 10, 10, 10, 10, 0};
        por::EffectRecord effect;
        effect.code = static_cast<std::uint8_t>(power);
        effect.data = 255;
        Content content;
        content.game = game;
        content.bestiary.emplace("monster", combatant_from_record(raw, {}, std::array{effect}));
        const auto module = rules_module(std::move(content));
        rules::CharacterSheet hero;
        hero.identity = identity(game);
        hero.name = "Hero";
        hero.hit_points = 200;
        hero.original = opengold::rules::CharacterSheet::Original{};
        hero.original->class_levels[2] = 8;
        hero.original->race = static_cast<int>(race);
        hero.original->profession = static_cast<int>(Profession::fighter);
        hero.original->saving_throws.fill(21);
        for (const auto ability : rules::all_abilities)
        {
            hero.scores[ability] = 10;
        }
        rules::Encounter encounter;
        encounter.battlefield = {6, 6, std::vector<rules::Terrain>(36, rules::Terrain::open)};
        rules::Participant pc;
        pc.id = 1;
        pc.name = "Hero";
        pc.side = rules::Side::party;
        pc.cell = {1, 1};
        pc.character_profile = module->character_profile(hero, {}).data;
        rules::Participant monster;
        monster.id = 2;
        monster.name = "Monster";
        monster.definition = "monster";
        monster.side = rules::Side::opposition;
        monster.cell = {2, 1};
        encounter.participants = {pc, monster};
        const auto fight = module->create(std::move(encounter), seed);
        for (unsigned turn = 0; turn < 30 && fight->snapshot().outcome == rules::Outcome::ongoing;
             ++turn)
        {
            const auto commands = fight->legal_commands();
            const auto has_yell = std::ranges::any_of(commands,
                                                      [](const auto &command)
                                                      {
                                                          return command.verb == "yell";
                                                      });
            const auto verb = fight->snapshot().actor == 2 ? std::string{"quick"}
                              : yelling && has_yell        ? std::string{"yell"}
                                                           : std::string{"end"};
            const auto end = std::ranges::find(commands, verb, &rules::Command::verb);
            check(end != commands.end() && fight->submit(*end),
                  "The member waits through legal commands");
            const auto after = fight->snapshot();
            const auto victim =
                std::ranges::find(after.combatants, rules::EntityId{1}, &rules::CombatantView::id);
            const auto changed = Combatant::decode(victim->persistent.resources);
            if (yelling && changed &&
                std::ranges::any_of(changed->effects,
                                    [](const Effect &effect)
                                    {
                                        return effect.code == 1653;
                                    }))
            {
                const auto enemy = std::ranges::find(after.combatants, rules::EntityId{2},
                                                     &rules::CombatantView::id);
                const auto taunted = Combatant::decode(enemy->persistent.resources);
                check(taunted && std::ranges::any_of(taunted->effects,
                                                     [](const Effect &effect)
                                                     {
                                                         return effect.code == 1491 &&
                                                                effect.strength == 1;
                                                     }),
                      "A failed save against a kender's Yell remembers the living yeller");
                const auto next_commands = fight->legal_commands();
                check(std::ranges::none_of(next_commands,
                                           [](const auto &command)
                                           {
                                               return command.verb == "yell";
                                           }),
                      "Yell is used once per fight, including after the active turn changes");
                break;
            }
            const int expected = power == 79 ? 1518 : power == 82 ? 1519 : 52;
            if (changed && std::ranges::any_of(changed->effects,
                                               [expected](const Effect &effect)
                                               {
                                                   return effect.code == expected;
                                               }))
            {
                break; // Inspect the landed condition before a helpless victim can be slain.
            }
        }
        const auto snapshot = fight->snapshot();
        const auto member =
            std::ranges::find(snapshot.combatants, rules::EntityId{1}, &rules::CombatantView::id);
        check(member != snapshot.combatants.end(), "The combat retains its participating member");
        const auto numbers = Combatant::decode(member->persistent.resources);
        check(numbers.has_value(), "Combat publishes its actual Original Rules conditions");
        const auto saved = fight->save();
        check(module->restore(saved)->save() == saved,
              "Native conditions and durations restore identically");
        return *numbers;
    };
    const auto kapak = suffer(69, Race::krynn_human, 17);
    const auto paralyzed = std::ranges::find(kapak.effects, 52, &Effect::code);
    check(paralyzed != kapak.effects.end() && paralyzed->minutes >= 99 && paralyzed->minutes <= 100,
          "A kapak's touch paralyzes for one hundred rounds, not an earlier game's short duration");
    const auto ghoul = suffer(88, Race::krynn_human, 17);
    check(std::ranges::any_of(ghoul.effects,
                              [](const Effect &effect)
                              {
                                  return effect.code == 52 && effect.minutes >= 99;
                              }),
          "Champions' ghoul also paralyzes a non-elf for one hundred rounds");
    const auto elf = suffer(88, Race::silvanesti_elf, 17);
    check(std::ranges::none_of(elf.effects,
                               [](const Effect &effect)
                               {
                                   return effect.code == 52;
                               }),
          "An elf resists a ghoul's paralyzing touch");
    const auto stench = suffer(79, Race::krynn_human, 17);
    check(std::ranges::any_of(stench.effects,
                              [](const Effect &effect)
                              {
                                  return effect.code == 1518;
                              }),
          "A ghast's stench affects its adjacent victim and persists during the fight");
    const auto dragon = suffer(82, Race::krynn_human, 18);
    check(std::ranges::any_of(dragon.effects,
                              [](const Effect &effect)
                              {
                                  return effect.code == 1519;
                              }),
          "A high-level member who fails against dragon fear takes Champions' lesser fear");
    const auto kender = suffer(82, Race::kender, 18);
    check(std::ranges::none_of(kender.effects,
                               [](const Effect &effect)
                               {
                                   return effect.code == 1519 || effect.code == 342;
                               }) &&
              std::ranges::any_of(kender.effects,
                                  [](const Effect &effect)
                                  {
                                      return effect.code == 492;
                                  }),
          "A kender's native immunity prevents both forms of dragon fear");
    const auto yelled = suffer(0, Race::kender, 18, true);
    check(std::ranges::any_of(yelled.effects,
                              [](const Effect &effect)
                              {
                                  return effect.code == 1653;
                              }),
          "A kender can use the original Yell command through normal combat controls");
    check(canonical_effect(71, game) == 1471 && canonical_effect(72, game) == 1472 &&
              canonical_effect(93, game) == 94,
          "Death-knight reflection and fear never borrow Blink or Secret's different fear");
}

void quick_combat_cannot_recover_a_baaz_banked_weapon()
{
    using namespace opengold;
    using namespace opengold::original;
    por::CharacterRecord hero;
    hero.game = game;
    hero.race = 6;
    hero.character_class = 2;
    hero.levels.fighter = 8;
    hero.highest_level_raw = 8;
    hero.name = "Fighter";
    hero.max_hit_points = hero.current.hit_points_raw = 200;
    hero.base_thac0 = -10;
    hero.base_armor_class = -10;
    hero.base_movement = 12;
    hero.item_limit = 255;
    hero.base_attacks[0] = {2, {1, 1, 0}};
    hero.current_abilities = hero.abilities = {18, 10, 10, 3, 18, 10, 0};
    std::vector<std::uint8_t> raw_item(63);
    raw_item[46] = 18;
    raw_item[52] = raw_item[57] = 1;
    const auto sword = por::decode_items(raw_item)->front();
    por::ItemTemplate sword_type;
    sword_type.worn_location = 0;
    sword_type.hands = 1;
    sword_type.class_restrictions = 255;
    sword_type.small_medium_damage = sword_type.large_damage = {1, 20, 0};
    const std::array<CarriedItem, 1> equipment{{{sword, sword_type}}};
    auto baaz = hero;
    baaz.race = 7;
    baaz.base_armor_class = 10;
    baaz.max_hit_points = baaz.current.hit_points_raw = 1;
    por::EffectRecord disarm;
    disarm.code = 67;
    disarm.data = 255;
    Content content;
    content.game = game;
    content.item_types.resize(19);
    content.item_types[18] = sword_type;
    content.bestiary.emplace("baaz", combatant_from_record(baaz, {}, std::array{disarm}));
    auto bystander = baaz;
    bystander.max_hit_points = bystander.current.hit_points_raw = 200;
    content.bestiary.emplace("bystander", combatant_from_record(bystander));
    const auto module = rules_module(std::move(content));
    rules::Encounter encounter;
    encounter.battlefield = {6, 6, std::vector<rules::Terrain>(36, rules::Terrain::open)};
    rules::Participant fighter;
    fighter.id = 1;
    fighter.name = hero.name;
    fighter.side = rules::Side::party;
    fighter.cell = {1, 1};
    fighter.character_profile = combatant_from_record(hero, equipment).encode();
    fighter.character_record = encode_fight_record(hero);
    fighter.inventory.push_back({7, por::original_item_key(sword), 1, 0, "Sword"});
    encounter.participants.push_back(fighter);
    for (const auto &[id, name, cell] : {std::tuple{2U, "baaz", rules::Cell{2, 1}},
                                         std::tuple{3U, "bystander", rules::Cell{4, 1}}})
    {
        rules::Participant monster;
        monster.id = id;
        monster.definition = name;
        monster.name = name;
        monster.side = rules::Side::opposition;
        monster.cell = cell;
        encounter.participants.push_back(monster);
    }
    auto fight = module->create(std::move(encounter), 19);
    bool banked = false, checked_quick = false;
    for (unsigned turn = 0; turn < 60 && fight->snapshot().outcome == rules::Outcome::ongoing;
         ++turn)
    {
        const auto commands = fight->legal_commands();
        const bool checking = banked && fight->snapshot().actor == 1;
        if (checking)
        {
            check(std::ranges::none_of(commands,
                                       [](const auto &command)
                                       {
                                           return command.verb == "ready";
                                       }),
                  "The sword stuck in the Baaz cannot be readied manually");
            const auto saved = fight->save();
            fight = module->restore(saved);
            check(fight->save() == saved, "A combat save retains the sword's temporary bank");
        }
        const auto verb = fight->snapshot().actor == 1 ? "quick" : "end";
        const auto command = std::ranges::find(commands, std::string{verb}, &rules::Command::verb);
        check(command != commands.end() && fight->submit(*command), "Use normal combat commands");
        const auto state = fight->snapshot();
        const auto &view =
            *std::ranges::find(state.combatants, rules::EntityId{1}, &rules::CombatantView::id);
        const auto numbers = Combatant::decode(view.persistent.resources);
        banked = numbers && std::ranges::any_of(numbers->effects,
                                                [](const Effect &power)
                                                {
                                                    return power.code == 1654;
                                                });
        if (checking)
        {
            check(banked && numbers->attacks[0].sides == 1 && view.readied_items &&
                      view.readied_items->empty(),
                  "Quick fights unarmed rather than recovering the stone-bound sword");
            checked_quick = true;
            break;
        }
    }
    check(banked && checked_quick, "A Baaz death actually disarms its melee attacker");
}

void innate_aurak_bolts_and_the_death_knights_fireball_need_no_spellbook()
{
    using namespace opengold;
    using namespace opengold::original;
    const auto exercise = [](unsigned power)
    {
        por::CharacterRecord raw;
        raw.game = game;
        raw.race = 7;
        raw.max_hit_points = raw.current.hit_points_raw = 200;
        raw.base_armor_class = 10;
        raw.base_thac0 = 20;
        raw.base_movement = 12;
        raw.current_abilities = raw.abilities = {10, 10, 10, 10, 10, 10, 0};
        por::EffectRecord innate;
        innate.code = static_cast<std::uint8_t>(power);
        innate.data = 255;
        Content content;
        content.game = game;
        content.bestiary.emplace("power", combatant_from_record(raw, {}, std::array{innate}));
        const auto module = rules_module(std::move(content));
        rules::CharacterSheet hero;
        hero.identity = identity(game);
        hero.name = "Hero";
        hero.hit_points = 200;
        hero.original = opengold::rules::CharacterSheet::Original{};
        hero.original->class_levels[2] = 8;
        hero.original->race = static_cast<int>(Race::krynn_human);
        hero.original->profession = static_cast<int>(Profession::fighter);
        hero.original->saving_throws.fill(21);
        for (const auto ability : rules::all_abilities)
        {
            hero.scores[ability] = 10;
        }
        rules::Encounter encounter;
        encounter.battlefield = {10, 10, std::vector<rules::Terrain>(100, rules::Terrain::open)};
        for (unsigned n = 0; n < 2; ++n)
        {
            rules::Participant pc;
            pc.id = n + 1;
            pc.name = "Hero " + std::to_string(n);
            pc.side = rules::Side::party;
            pc.cell = {1, static_cast<int>(n + 1)};
            auto profile = *Combatant::decode(module->character_profile(hero, {}).data);
            // A synthetic durable target isolates the power from an early kill.
            profile.hit_points = profile.max_hit_points = 2000;
            if (power == 74)
            {
                profile.effects.push_back({39, 1000, 1, false});
            }
            pc.character_profile = profile.encode();
            encounter.participants.push_back(pc);
        }
        rules::Participant foe;
        foe.id = 3;
        foe.name = "Innate caster";
        foe.definition = "power";
        foe.side = rules::Side::opposition;
        foe.cell = power == 4    ? rules::Cell{2, 1}
                   : power == 74 ? rules::Cell{5, 4}
                                 : rules::Cell{7, 7};
        encounter.participants.push_back(foe);
        auto fight = module->create(std::move(encounter), 19);
        for (unsigned turn = 0; turn < 100 && fight->snapshot().outcome == rules::Outcome::ongoing;
             ++turn)
        {
            const auto commands = fight->legal_commands();
            const auto verb = power == 6 && fight->snapshot().actor == 3 ? "quick" : "end";
            const auto end = std::ranges::find(commands, std::string{verb}, &rules::Command::verb);
            check(end != commands.end() && fight->submit(*end), "Wait through normal combat turns");
        }
        const auto saved = fight->save();
        check(module->restore(saved)->save() == saved,
              "The innate power's use and remaining breaths survive a combat save");
        const auto snapshot = fight->snapshot();
        const auto count = [&](std::string_view text)
        {
            return std::ranges::count_if(snapshot.log(),
                                         [&](const auto &entry)
                                         {
                                             return entry.find(text) != std::string::npos;
                                         });
        };
        const auto &caster =
            *std::ranges::find(snapshot.combatants, rules::EntityId{3}, &rules::CombatantView::id);
        const auto remaining = Combatant::decode(caster.persistent.resources);
        check(remaining && remaining->spells.empty(),
              "An innate attack never invents prepared spells");
        if (power == 4)
        {
            const auto breaths = count("breathes a blinding cloud");
            check(count("fires an energy bolt") > 0 && breaths == 3 &&
                      std::ranges::any_of(remaining->effects,
                                          [](const auto &effect)
                                          {
                                              return effect.code == 1404 && effect.strength == 0;
                                          }),
                  "An aurak fires energy bolts and expends exactly its three blinding breaths");
        }
        else if (power == 6)
        {
            check(count("breathes.") == 1 && std::ranges::any_of(remaining->effects,
                                                                 [](const auto &effect)
                                                                 {
                                                                     return effect.code == 1406 &&
                                                                            effect.strength == 0;
                                                                 }),
                  "Maya uses her native eight-square cold cone once per fight");
        }
        else if (power == 74)
        {
            const bool dispelled =
                std::ranges::any_of(snapshot.combatants,
                                    [](const auto &unit)
                                    {
                                        const auto state =
                                            Combatant::decode(unit.persistent.resources);
                                        return unit.side == rules::Side::party && state &&
                                               std::ranges::none_of(state->effects,
                                                                    [](const auto &effect)
                                                                    {
                                                                        return effect.code == 39;
                                                                    });
                                    });
            check(count("invokes Dispel Magic") > 0 && dispelled,
                  "The death knight's random innate dispel removes a real opposing ward");
        }
        else
        {
            int most_damage = 0;
            for (const auto &target : snapshot.combatants)
            {
                if (target.side == rules::Side::party)
                {
                    most_damage = std::max(most_damage, 2000 - target.hit_points);
                }
            }
            check(count("throws a fireball") == 1 && most_damage > 36 && most_damage <= 90 &&
                      std::ranges::none_of(remaining->effects,
                                           [](const auto &effect)
                                           {
                                               return effect.code == 1478;
                                           }),
                  "The death knight throws its single 15d6 fireball, not a fallback-level spell");
        }
    };
    exercise(4);
    exercise(6);
    exercise(74);
    exercise(78);
}

void a_death_knight_reflects_a_targeted_spell_to_its_real_caster()
{
    using namespace opengold;
    using namespace opengold::original;
    por::CharacterRecord raw;
    raw.game = game;
    raw.race = 7;
    raw.max_hit_points = raw.current.hit_points_raw = 200;
    raw.base_armor_class = 10;
    raw.base_thac0 = 20;
    raw.base_movement = 12;
    raw.levels.mage = 8;
    raw.current_abilities = raw.abilities = {10, 10, 10, 10, 10, 10, 0};
    auto mage = combatant_from_record(raw);
    mage.spells = {15};
    por::EffectRecord reflects;
    reflects.code = 71;
    reflects.data = 255;
    Content content;
    content.game = game;
    content.bestiary.emplace("knight", combatant_from_record(raw, {}, std::array{reflects}));
    const auto module = rules_module(std::move(content));
    rules::Encounter encounter;
    encounter.battlefield = {8, 8, std::vector<rules::Terrain>(64, rules::Terrain::open)};
    rules::Participant caster;
    caster.id = 1;
    caster.name = "Mage";
    caster.cell = {1, 1};
    caster.character_profile = mage.encode();
    encounter.participants.push_back(caster);
    rules::Participant knight;
    knight.id = 2;
    knight.name = "Death knight";
    knight.definition = "knight";
    knight.side = rules::Side::opposition;
    knight.cell = {5, 1};
    encounter.participants.push_back(knight);
    bool reflected = false, unreflected = false;
    for (unsigned seed = 1; seed <= 64 && !(reflected && unreflected); ++seed)
    {
        auto fight = module->create(encounter, seed);
        bool cast = false, aimed = false;
        for (unsigned turn = 0; turn < 30 && !aimed; ++turn)
        {
            const auto commands = fight->legal_commands();
            const bool ours = fight->snapshot().actor == 1;
            const auto chosen =
                std::ranges::find_if(commands,
                                     [&](const auto &c)
                                     {
                                         if (!ours)
                                         {
                                             return c.verb == "end";
                                         }
                                         if (!cast)
                                         {
                                             return c.verb == "cast" && c.item == 15;
                                         }
                                         return c.verb == "aim" && c.target == 2;
                                     });
            check(chosen != commands.end() && fight->submit(*chosen),
                  "Cast and aim an actual prepared spell");
            if (ours && !cast)
            {
                cast = true;
            }
            else if (ours)
            {
                aimed = true;
            }
        }
        const auto snapshot = fight->snapshot();
        const auto &mage_view =
            *std::ranges::find(snapshot.combatants, rules::EntityId{1}, &rules::CombatantView::id);
        const auto &knight_view =
            *std::ranges::find(snapshot.combatants, rules::EntityId{2}, &rules::CombatantView::id);
        const auto remaining = Combatant::decode(mage_view.persistent.resources);
        const bool bounced =
            std::ranges::any_of(snapshot.log(),
                                [](const auto &line)
                                {
                                    return line.find("reflects the spell") != std::string::npos;
                                });
        check(aimed && remaining && remaining->spells.empty(),
              "Aimed Magic Missile spends its preparation");
        if (bounced)
        {
            check(mage_view.hit_points < 200 && knight_view.hit_points == 200,
                  "A reflected Magic Missile harms its caster and spares the death knight");
            reflected = true;
        }
        else
        {
            check(mage_view.hit_points == 200 && knight_view.hit_points < 200,
                  "An unreflected Magic Missile harms its intended target");
            unreflected = true;
        }
        const auto saved = fight->save();
        check(module->restore(saved)->save() == saved,
              "A reflected spell's effects survive a combat save");
    }
    check(reflected && unreflected,
          "A death knight reflects a creature-aimed spell one time in ten, not every time");
}

void a_kapaks_acid_pool_burns_then_drains_after_a_saved_fight()
{
    using namespace opengold;
    using namespace opengold::original;
    por::CharacterRecord raw;
    raw.game = game;
    raw.race = 7;
    raw.max_hit_points = raw.current.hit_points_raw = 200;
    raw.base_armor_class = 10;
    raw.base_thac0 = -20;
    raw.base_movement = 12;
    raw.base_attacks[0] = {4, {1, 1, 0}};
    raw.current_abilities = raw.abilities = {10, 10, 10, 10, 10, 10, 0};
    auto kapak = raw;
    kapak.max_hit_points = kapak.current.hit_points_raw = 1;
    por::EffectRecord acid;
    acid.code = 70;
    acid.data = 255;
    Content content;
    content.game = game;
    content.bestiary.emplace("kapak", combatant_from_record(kapak, {}, std::array{acid}));
    content.bestiary.emplace("bystander", combatant_from_record(raw));
    const auto module = rules_module(std::move(content));
    rules::Encounter encounter;
    encounter.battlefield = {6, 6, std::vector<rules::Terrain>(36, rules::Terrain::open)};
    rules::Participant hero;
    hero.id = 1;
    hero.name = "Hero";
    hero.cell = {1, 1};
    hero.character_profile = combatant_from_record(raw).encode();
    encounter.participants.push_back(hero);
    for (const auto &[id, name, cell] : {std::tuple{2U, "kapak", rules::Cell{2, 1}},
                                         std::tuple{3U, "bystander", rules::Cell{4, 4}}})
    {
        rules::Participant foe;
        foe.id = id;
        foe.name = name;
        foe.definition = name;
        foe.cell = cell;
        foe.side = rules::Side::opposition;
        encounter.participants.push_back(foe);
    }
    auto fight = module->create(std::move(encounter), 19);
    bool killed = false, entered = false, saved_pool = false;
    for (unsigned turn = 0; turn < 100 && fight->snapshot().outcome == rules::Outcome::ongoing;
         ++turn)
    {
        const auto snapshot = fight->snapshot();
        const auto commands = fight->legal_commands();
        const bool our_turn = snapshot.actor == 1;
        const auto chosen = std::ranges::find_if(commands,
                                                 [&](const auto &c)
                                                 {
                                                     if (our_turn && !killed)
                                                     {
                                                         return c.verb == "melee" && c.target == 2;
                                                     }
                                                     if (our_turn && !entered)
                                                     {
                                                         return c.verb == "move" &&
                                                                c.destination == rules::Cell{2, 1};
                                                     }
                                                     return c.verb == "end";
                                                 });
        check(chosen != commands.end() && fight->submit(*chosen),
              "Follow ordinary acid-pool commands");
        if (our_turn && !killed)
        {
            const auto now = fight->snapshot();
            killed = std::ranges::any_of(now.log(),
                                         [](const auto &line)
                                         {
                                             return line.find("dissolves into acid") !=
                                                    std::string::npos;
                                         });
            if (killed)
            {
                const auto saved = fight->save();
                fight = module->restore(saved);
                check(fight->save() == saved, "A saved fight retains the acid pool and its timer");
                saved_pool = true;
            }
        }
        else if (our_turn && !entered)
        {
            entered = true;
        }
    }
    const auto result = fight->snapshot();
    check(killed && saved_pool && entered &&
              std::ranges::any_of(result.log(),
                                  [](const auto &line)
                                  {
                                      return line.find("burned by acid") != std::string::npos;
                                  }) &&
              std::ranges::any_of(result.log(),
                                  [](const auto &line)
                                  {
                                      return line.find("acid drains away") != std::string::npos;
                                  }),
          "Entering a slain kapak's square burns the entrant until its d6-round pool drains");
    const auto saved = fight->save();
    check(module->restore(saved)->save() == saved, "The drained pool also survives a combat save");
}

void native_weapon_powers_use_their_actual_quarry_and_damage()
{
    using namespace opengold;
    using namespace opengold::original;
    const auto strike =
        [](int power, int targets, int traits, int undead, unsigned seed = 19, int thac0 = -20)
    {
        por::CharacterRecord raw;
        raw.game = game;
        raw.race = 7;
        raw.max_hit_points = 200;
        raw.current.hit_points_raw = 37;
        raw.base_armor_class = 10;
        raw.base_thac0 = static_cast<std::int8_t>(thac0);
        raw.base_movement = 12;
        raw.base_attacks[0] = {2, {1, 1, 0}};
        raw.current_abilities = raw.abilities = {10, 10, 10, 10, 10, 10, 0};
        por::ItemRecord item;
        item.type = 18;
        item.readied_raw = 1;
        item.effect_codes = {0, static_cast<std::uint8_t>(power), 0x80};
        por::ItemTemplate type;
        type.worn_location = 0;
        type.hands = 1;
        type.class_restrictions = 255;
        type.small_medium_damage = type.large_damage = {1, 1, 0};
        const std::array<CarriedItem, 1> gear{{{item, type}}};
        const auto wielder = combatant_from_record(raw, gear);
        check(power == 0 || std::ranges::any_of(wielder.effects,
                                                [&](const auto &e)
                                                {
                                                    return e.code == canonical_effect(power, game);
                                                }),
              "A readied native item grants its weapon power");
        auto victim = raw;
        victim.current.hit_points_raw = 200;
        victim.target_flags = static_cast<std::uint8_t>(targets);
        victim.creature_type_flags = static_cast<std::uint8_t>(traits);
        victim.undead_level = static_cast<std::uint8_t>(undead);
        auto target_profile = combatant_from_record(victim);
        target_profile.hit_points = target_profile.max_hit_points = 2000;
        Content content;
        content.game = game;
        const auto module = rules_module(std::move(content));
        rules::Encounter encounter;
        encounter.battlefield = {4, 4, std::vector<rules::Terrain>(16, rules::Terrain::open)};
        rules::Participant attacker;
        attacker.id = 1;
        attacker.name = "Wielder";
        attacker.side = rules::Side::party;
        attacker.cell = {1, 1};
        attacker.character_profile = wielder.encode();
        encounter.participants.push_back(attacker);
        rules::Participant target;
        target.id = 2;
        target.name = "Quarry";
        target.side = rules::Side::opposition;
        target.cell = {2, 1};
        target.character_profile = target_profile.encode();
        encounter.participants.push_back(target);
        auto fight = module->create(std::move(encounter), seed);
        bool struck = false;
        for (unsigned turn = 0; turn < 10 && !struck; ++turn)
        {
            const auto commands = fight->legal_commands();
            const bool ours = fight->snapshot().actor == 1;
            const auto chosen = std::ranges::find_if(
                commands,
                [&](const auto &c)
                {
                    return ours ? c.verb == "melee" && c.target == 2 : c.verb == "end";
                });
            check(chosen != commands.end() && fight->submit(*chosen),
                  "Make one normal melee attack");
            struck = ours;
        }
        const auto snapshot = fight->snapshot();
        const auto &quarry =
            *std::ranges::find(snapshot.combatants, rules::EntityId{2}, &rules::CombatantView::id);
        const auto saved = fight->save();
        check(module->restore(saved)->save() == saved, "Weapon outcomes survive combat saves");
        return quarry;
    };
    const auto slayer = strike(75, 1, 0, 0);
    const int slayer_damage = 2000 - slayer.hit_points;
    check(slayer_damage >= 7 && slayer_damage <= 40 && (slayer_damage - 4) % 3 == 0,
          "Dragon Slayer triples one d12 before its four-point bonus");
    check(strike(75, 0, 0, 0).hit_points == 1999 && strike(114, 0, 0, 0).hit_points == 1999,
          "Dragon weapons keep normal damage against other creatures");
    check(strike(114, 1, 0, 0).hit_points == 1963,
          "A dragonlance deals its wounded wielder's current 37 HP, not maximum HP");
    check(strike(115, 0, 32, 0).hit_points == 1996 && strike(115, 0, 0, 0).hit_points == 1999,
          "The type bonus adds three only against native reptile flags");
    check(strike(117, 0, 2, 0).hit_points == 1998 && strike(117, 0, 0, 0).hit_points == 1999,
          "Disruption doubles damage to unliving creatures without a turning level");
    const auto disrupted = strike(117, 0, 2, 1);
    check(disrupted.hit_points == 0 && !disrupted.conscious,
          "Disruption removes an undead creature with a positive turning level");
    bool bonus_hit = false;
    for (unsigned seed = 1; seed <= 64 && !bonus_hit; ++seed)
    {
        const auto ordinary = strike(0, 1, 0, 0, seed, 20);
        const auto special = strike(75, 1, 0, 0, seed, 20);
        bonus_hit = ordinary.hit_points == 2000 && special.hit_points < 2000;
    }
    check(bonus_hit, "Dragon Slayer also improves its attack roll against dragons");
}

void removed_ghasts_wait_for_the_death_knights_gate()
{
    using namespace opengold;
    using namespace opengold::original;
    por::CharacterRecord raw;
    raw.game = game;
    raw.race = 7;
    raw.max_hit_points = raw.current.hit_points_raw = 63;
    raw.base_armor_class = 10;
    raw.base_thac0 = 20;
    raw.base_movement = 12;
    raw.current_abilities = raw.abilities = {10, 10, 10, 10, 10, 10, 0};
    por::EffectRecord gate;
    gate.code = 76;
    gate.data = 255;
    Content content;
    content.game = game;
    content.bestiary.emplace("knight", combatant_from_record(raw, {}, std::array{gate}));
    auto removed = raw;
    removed.max_hit_points = removed.current.hit_points_raw = 18;
    removed.status = 9;
    content.bestiary.emplace("reserve", combatant_from_record(removed));
    content.gear.emplace("reserve", MonsterGear{removed, {}});
    const auto module = rules_module(std::move(content));
    rules::CharacterSheet hero;
    hero.identity = identity(game);
    hero.name = "Fighter";
    hero.hit_points = 200;
    hero.original = opengold::rules::CharacterSheet::Original{};
    hero.original->class_levels[2] = 8;
    hero.original->race = static_cast<int>(Race::krynn_human);
    hero.original->profession = static_cast<int>(Profession::fighter);
    hero.original->thac0 = -10;
    hero.original->exceptional_strength = 100;
    for (const auto ability : rules::all_abilities)
    {
        hero.scores[ability] = 18;
    }
    rules::Encounter encounter;
    encounter.battlefield = {6, 6, std::vector<rules::Terrain>(36, rules::Terrain::open)};
    rules::Participant pc;
    pc.id = 1;
    pc.name = hero.name;
    pc.side = rules::Side::party;
    pc.cell = {1, 1};
    pc.character_profile = module->character_profile(hero, {}).data;
    encounter.participants.push_back(pc);
    rules::Participant knight;
    knight.id = 2;
    knight.name = "Death knight";
    knight.definition = "knight";
    knight.side = rules::Side::opposition;
    knight.cell = {2, 1};
    encounter.participants.push_back(knight);
    for (unsigned n = 0; n < 4; ++n)
    {
        rules::Participant reserve;
        reserve.id = n + 3;
        reserve.name = "Ghast " + std::to_string(n);
        reserve.definition = "reserve";
        reserve.side = rules::Side::opposition;
        reserve.cell = {4 + static_cast<int>(n % 2), 4 + static_cast<int>(n / 2)};
        encounter.participants.push_back(reserve);
    }
    auto fight = module->create(std::move(encounter), 19);
    const auto initial = fight->snapshot();
    check(std::ranges::count_if(initial.combatants,
                                [](const auto &unit)
                                {
                                    return unit.id >= 3 && !unit.conscious && unit.fled &&
                                           unit.status == "waiting";
                                }) == 4,
          "All four native removed ghasts start off the field, alive and unable to act");
    const auto before = fight->save();
    fight = module->restore(before);
    check(fight->save() == before, "A save retains waiting reserves without timed arrivals");
    bool gated = false;
    for (unsigned turn = 0; turn < 100 && fight->snapshot().outcome == rules::Outcome::ongoing;
         ++turn)
    {
        const auto commands = fight->legal_commands();
        const bool attack = !gated && fight->snapshot().actor == 1;
        const auto command = std::ranges::find_if(commands,
                                                  [&](const auto &candidate)
                                                  {
                                                      return attack ? candidate.verb == "melee" &&
                                                                          candidate.target == 2
                                                                    : candidate.verb == "end";
                                                  });
        check(command != commands.end() && fight->submit(*command),
              "Fight through normal commands");
        const auto current = fight->snapshot();
        const auto arrived =
            std::ranges::count_if(current.combatants,
                                  [](const auto &unit)
                                  {
                                      return unit.id >= 3 && unit.conscious && !unit.fled;
                                  });
        if (arrived == 4)
        {
            gated = true;
            for (const auto &unit : current.combatants)
            {
                if (unit.id >= 3)
                {
                    check(unit.hit_points == 18 &&
                              std::max(std::abs(unit.cell.x - 1), std::abs(unit.cell.y - 1)) == 1,
                          "Each ghast gates in at full HP beside an enemy");
                }
            }
            const auto after = fight->save();
            check(module->restore(after)->save() == after,
                  "A save retains the completed one-use gate");
            const auto &boss = *std::ranges::find(current.combatants, rules::EntityId{2},
                                                  &rules::CombatantView::id);
            const auto state = Combatant::decode(boss.persistent.resources);
            check(state && std::ranges::none_of(state->effects,
                                                [](const auto &effect)
                                                {
                                                    return effect.code == 1476;
                                                }),
                  "The gate cannot be used a second time");
            break;
        }
    }
    check(gated,
          "Wounding the death knight below half its HP actually gates in the waiting ghasts");
}

void an_aurak_must_finish_its_real_transformations()
{
    using namespace opengold;
    using namespace opengold::original;
    check(
        canonical_effect(13, game) != canonical_effect(13, GoldBoxGame::pool_of_radiance) &&
            canonical_effect(68, game) != canonical_effect(68, GoldBoxGame::pool_of_radiance) &&
            canonical_effect(100, game) == 115 && canonical_effect(97, game) == 109,
        "Champions' deaths, weapon resistance and paralysis immunity never borrow unrelated powers");
    por::CharacterRecord raw;
    raw.game = game;
    raw.race = 7;
    raw.name = "AURAK";
    raw.max_hit_points = 30;
    raw.current.hit_points_raw = 1;
    raw.base_thac0 = 20;
    raw.base_armor_class = 10;
    raw.base_movement = 12;
    raw.base_attacks[0] = {2, {1, 2, 0}};
    raw.current_abilities = {10, 10, 10, 10, 10, 10, 0};
    raw.abilities = raw.current_abilities;
    por::EffectRecord frenzy;
    frenzy.code = 13;
    frenzy.data = 255;
    Content content;
    content.game = game;
    content.bestiary.emplace("aurak", combatant_from_record(raw, {}, std::array{frenzy}));
    auto explosion = frenzy;
    explosion.code = 68;
    content.bestiary.emplace("bozak", combatant_from_record(raw, {}, std::array{explosion}));
    const auto module = rules_module(std::move(content));
    rules::CharacterSheet hero;
    hero.identity = identity(game);
    hero.name = "Fighter";
    hero.hit_points = 200;
    for (const auto ability : rules::all_abilities)
    {
        hero.scores[ability] = 18;
    }
    rules::CharacterSheet::Original numbers;
    numbers.class_levels[2] = 8;
    numbers.race = static_cast<int>(Race::krynn_human);
    numbers.profession = static_cast<int>(Profession::fighter);
    numbers.exceptional_strength = 100;
    numbers.thac0 = -5;
    numbers.saving_throws.fill(2);
    hero.original = numbers;
    rules::Encounter encounter;
    encounter.battlefield = {6, 6, std::vector<rules::Terrain>(36, rules::Terrain::open)};
    rules::Participant fighter;
    fighter.id = 1;
    fighter.name = hero.name;
    fighter.definition = "member";
    fighter.side = rules::Side::party;
    fighter.cell = {1, 1};
    fighter.character_profile = module->character_profile(hero, {}).data;
    encounter.participants.push_back(fighter);
    rules::Participant aurak;
    aurak.id = 2;
    aurak.name = "Aurak";
    aurak.definition = "aurak";
    aurak.side = rules::Side::opposition;
    aurak.cell = {2, 1};
    encounter.participants.push_back(aurak);
    auto bozak_encounter = encounter;
    bozak_encounter.participants.back().definition = "bozak";
    bozak_encounter.participants.back().name = "Bozak";
    auto fight = module->create(std::move(encounter), 17);
    bool restored_last_form = false;
    for (unsigned turn = 0; turn < 300 && fight->snapshot().outcome == rules::Outcome::ongoing;
         ++turn)
    {
        const auto commands = fight->legal_commands();
        auto chosen = std::ranges::find(commands, std::string{"quick"}, &rules::Command::verb);
        if (chosen == commands.end())
        {
            chosen = std::ranges::find(commands, std::string{"end"}, &rules::Command::verb);
        }
        check(chosen != commands.end() && fight->submit(*chosen),
              "Play the native synthetic combat through legal commands");
        const auto messages = fight->snapshot().log();
        if (!restored_last_form &&
            std::ranges::any_of(messages,
                                [](const auto &message)
                                {
                                    return message.find("becomes a blazing shape") !=
                                           std::string::npos;
                                }))
        {
            const auto checkpoint = fight->save();
            auto restored = module->restore(checkpoint);
            check(restored->save() == checkpoint,
                  "The Champions combat checkpoint preserves the aurak's final form and timer");
            fight = std::move(restored);
            restored_last_form = true;
        }
    }
    const auto snapshot = fight->snapshot();
    const auto messages = snapshot.log();
    const auto says = [&](std::string_view text)
    {
        return std::ranges::any_of(messages,
                                   [&](const std::string &message)
                                   {
                                       return message.find(text) != std::string::npos;
                                   });
    };
    check(
        snapshot.outcome == rules::Outcome::victory && snapshot.round >= 4 && restored_last_form &&
            says("rises in a frenzy") && says("becomes a blazing shape") && says("explodes"),
        "An aurak survives its first fall, transforms, remains dangerous for three rounds and finally explodes");
    const auto bozak = module->create(std::move(bozak_encounter), 19);
    for (unsigned turn = 0; turn < 50 && bozak->snapshot().outcome == rules::Outcome::ongoing;
         ++turn)
    {
        const auto commands = bozak->legal_commands();
        const auto quick = std::ranges::find(commands, std::string{"quick"}, &rules::Command::verb);
        check(quick != commands.end() && bozak->submit(*quick),
              "Fight the Bozak through normal combat commands");
    }
    const auto result = bozak->snapshot();
    const auto &hero_after =
        *std::ranges::find(result.combatants, rules::EntityId{1}, &rules::CombatantView::id);
    check(result.outcome == rules::Outcome::victory && hero_after.hit_points < 200 &&
              std::ranges::any_of(
                  result.log(),
                  [](const auto &message)
                  {
                      return message.find("explodes") != std::string::npos;
                  }),
          "A dying Bozak explodes and wounds its adjacent opponent");
}
} // namespace

int main()
{
    try
    {
        a_knights_independent_fields_survive_decoding();
        check(opengold::original::modify_allowed(2001, game) &&
                  opengold::original::modify_allowed(5000, game) &&
                  !opengold::original::modify_allowed(2002, game),
              "Champions Modify uses its own exact experience values");
        repeated_archive_ids_keep_their_payloads_and_first_lookup();
        malformed_scores_are_rejected();
        a_top_level_return_ends_the_event_before_embedded_data();
        spell_queries_preserve_preparations_until_the_script_spends_one();
        native_strength_can_force_the_required_wizard_locked_door();
        native_creation_keeps_krynns_choices_and_progression();
        steel_prices_and_change_conserve_value();
        hit_points_and_deity_changes_follow_the_actual_rules();
        knight_petitions_apply_the_next_orders_progression();
        knight_order_changes_replay_after_a_lost_and_regained_level();
        deity_gifts_have_their_own_shared_preparations();
        native_monster_conditions_use_champions_durations_and_immunities();
        quick_combat_cannot_recover_a_baaz_banked_weapon();
        innate_aurak_bolts_and_the_death_knights_fireball_need_no_spellbook();
        a_death_knight_reflects_a_targeted_spell_to_its_real_caster();
        a_kapaks_acid_pool_burns_then_drains_after_a_saved_fight();
        native_weapon_powers_use_their_actual_quarry_and_damage();
        removed_ghasts_wait_for_the_death_knights_gate();
        an_aurak_must_finish_its_real_transformations();
        actual_champions_records_decode();
        the_gold_dragons_camp_waits_for_the_players_departure();
        std::cout << "Champions checks passed.\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
