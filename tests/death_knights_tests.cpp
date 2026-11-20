#include "opengold/death_knights_records.h"
#include "opengold/ecl.h"
#include "opengold/ecl_machine.h"
#include "opengold/formats.h"
#include "opengold/game_census.h"
#include "opengold/por_sound.h"
#include "opengold/original_spells.h"
#include "opengold/original.h"
#include "opengold/campaign_session.h"
#include "opengold/campaign_party.h"
#include "opengold/campaign_save.h"
#include "opengold/death_knights_import.h"
#include "opengold/original_content.h"
#include "opengold/music_render.h"
#include "opengold/training_hall.h"
#include "opengold/character_creator.h"
#include "original_creation_support.h"
#include "opengold/creature_catalog.h"
#include "opengold/original_combat.h"
#include "opengold/wall_art.h"
#include "installed_games.h"
#include "test_support.h"
#include "tour_access.h"
#include "death_knights_rules.h"
#include "death_knights_effects.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>

namespace
{
using opengold::test::check;
using Bytes = std::vector<std::uint8_t>;
constexpr auto game = opengold::GoldBoxGame::death_knights_of_krynn;

Bytes read(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    check(static_cast<bool>(input), "Open " + path.string());
    return {std::istreambuf_iterator<char>(input), {}};
}

void independent_fields_and_separate_spells_survive()
{
    check(static_cast<unsigned>(opengold::GoldBoxGame::champions_of_krynn) == 7 &&
              static_cast<unsigned>(game) == 8,
          "Append Death Knights without renumbering released games");
    Bytes raw(216);
    raw[0] = 6;
    const std::string name = "KNIGHT";
    std::copy(name.begin(), name.end(), raw.begin() + 1);
    raw[16] = 18;
    raw[17] = 17;
    raw[34] = 3;
    raw[35] = 4;
    raw[36] = 1;
    raw[40] = 77;
    raw[76] = 6;
    raw[98] = 0x53;
    raw[99] = 0x34;
    raw[100] = 3;
    auto record = opengold::death_knights::decode_creature(raw);
    check(record && record->raw == raw && record->name == name &&
              record->abilities.strength == 18 && record->current_abilities.strength == 17 &&
              record->max_hit_points == 77 && record->experience == 210003 &&
              record->knight_order == 3 && record->deity == 4 && record->robe_order == 1,
          "Read permanent/current statistics, XP and all three independent Krynn fields");
    Bytes spells(220);
    spells[0] = 114;
    spells[187] = 1;
    spells[219] = 4; // Mage's eighth-level slots, beyond the earlier record's five levels.
    check(opengold::death_knights::attach_spells(*record, spells), "Attach the native WIZ record");
    const auto decoded = opengold::death_knights::decode_spells(record->raw_spell_block);
    check(decoded && decoded->slots[3][7] == 4 && decoded->known[113] == 1 &&
              decoded->memorized[0] == 114 && record->raw_spell_block == spells,
          "High-level slots and all known/memorized entries remain intact");
    spells.pop_back();
    check(!opengold::death_knights::attach_spells(*record, spells) &&
              record->raw_spell_block.size() == 220,
          "Malformed spell blocks fail without replacing the earlier data");
    raw.pop_back();
    check(!opengold::death_knights::decode_creature(raw), "Reject truncated character records");
}

void numbered_templates_and_complete_tile_sheets()
{
    Bytes list(34);
    list[0] = 5;
    list[4] = 255;
    list[17] = 28;
    const auto items = opengold::death_knights::decode_item_list(list);
    check(items && items->size() == 2 && items->front().type == 5 &&
              items->front().magic_bonus == -1 && items->back().type == 28,
          "Numbered 17-byte templates become ordinary carried items in source order");
    list.pop_back();
    check(!opengold::death_knights::decode_item_list(list), "Reject a partial numbered template");
    Bytes sheet(17 + 256 * 32);
    sheet[0] = 8;
    sheet[2] = 1;
    sheet[8] = 255;
    sheet[17 + 255 * 32] = 0xA5;
    const auto tiles = opengold::por::decode_wall_tiles(sheet);
    check(tiles && tiles->size() == 256 && tiles->back()[0] == 10 && tiles->back()[1] == 5,
          "FFh represents all 256 physical tiles, including the final tile's pixels");
    auto earlier = sheet;
    earlier.resize(17 + 255 * 32);
    const auto legacy = opengold::por::decode_wall_tiles(earlier);
    check(legacy && legacy->size() == 255, "Earlier literal 255-frame sheets remain supported");
    sheet.pop_back();
    check(!opengold::por::decode_wall_tiles(sheet), "Reject an incomplete physical tile frame");
}

std::shared_ptr<const opengold::por::EclProgram> script(const Bytes &body)
{
    Bytes record{0, 0};
    for (unsigned entry = 0; entry < 5; ++entry)
    {
        record.insert(record.end(), {1, 1, 0x14, 0x80});
    }
    record.insert(record.end(), body.begin(), body.end());
    return std::make_shared<const opengold::por::EclProgram>(
        opengold::por::EclProgram::decode(record, "Death Knights protocol fixture", game));
}

void later_commands_keep_the_earlier_memory_and_host_protocol()
{
    using namespace opengold::por;
    check(!opengold::vga_engine(game) && opengold::later_ecl_commands(game),
          "The command dialect does not select the VGA renderer or memory map");
    for (const std::uint8_t tag : {std::uint8_t{1}, std::uint8_t{3}})
    {
        EclMachine arithmetic(script({0x05, 0, 1, tag, 0x60, 0x4C, tag, 0x60, 0x4C, 0x13}));
        arithmetic.bind_variable(EclAddress{0x4C60}, 0);
        check(arithmetic.start(0) && arithmetic.run().state == EclState::completed &&
                  arithmetic.variable(EclAddress{0x4C60}) == (tag == 1 ? 255 : 65535),
              "Native byte and word writes preserve their respective arithmetic widths");
    }
    for (const std::size_t answer : {0U, 1U})
    {
        // YES NO; IF EQUAL; SAVE 7 to a game cell; top-level RETURN.
        EclMachine vm(script({0x22, 0x16, 0x09, 0x00, 7, 0x01, 0x60, 0x4C, 0x13}));
        vm.bind_variable(EclAddress{0x4C60}, 0);
        check(vm.start(0), "Start the native command fixture");
        const auto asked = vm.run();
        check(asked.request && asked.request->choices == std::vector<std::string>{"YES", "NO"},
              "YES NO uses its native choices without reading operands");
        check(vm.resume(asked.request->id, answer) && vm.run().state == EclState::completed &&
                  vm.variable(EclAddress{0x4C60}) == (answer == 0 ? 7 : 0),
              "Only YES sets equal; an empty return stack ends the event");
    }
    EclMachine prompts(script({0x1D, 0x1F, 0x00, 42, 0x23, 0xFF}));
    check(prompts.start(0), "Start the prompt fixture");
    const auto waiting = prompts.run();
    check(waiting.request && prompts.resume(waiting.request->id, 0), "WAIT KEY waits for Continue");
    const auto journal = prompts.run();
    check(journal.request && journal.request->text.find("42") != std::string::npos &&
              prompts.resume(journal.request->id, 0),
          "JOURNAL ENTRY displays its actual number");
    const auto cleared = prompts.run();
    check(cleared.request && cleared.request->clear && prompts.resume(cleared.request->id) &&
              prompts.run().state == EclState::completed,
          "CLEAR AND EXIT does not execute the following data byte");

    EclMachine host(
        script({0x27, 2, 0x34, 0x12, 2,    0x45, 0x23, 2,    0x56, 0x34, 0, 1, 0, 96, 0x41, 0,
                1,    2, 0x45, 0x23, 0x3C, 0,    2,    0x37, 0,    1,    0, 2, 0, 3,  0x13}));
    for (const auto opcode : {39, 65, 60, 55})
    {
        host.enable_host(static_cast<std::uint8_t>(opcode));
    }
    check(host.start(0), "Start the host-argument fixture");
    const auto treasure = host.run();
    check(treasure.request && treasure.request->arguments.size() == 5 &&
              treasure.request->arguments[0].number() == 0x1234 &&
              treasure.request->arguments[1].number() == 0x2345 &&
              treasure.request->arguments[2].number() == 0x3456 &&
              treasure.request->arguments[4].number() == 96 &&
              host.resume_host(treasure.request->id, {}),
          "Steel/gems/jewelry keep word values and templates remain numbered");
    const auto experience = host.run();
    check(experience.request && experience.request->arguments[1].number() == 0x2345 &&
              host.resume_host(experience.request->id, {}),
          "The host receives a full XP word without the VGA games' scaling");
    const auto store = host.run();
    check(store.request && store.request->arguments[0].number() == 2 &&
              host.resume_host(store.request->id, {}),
          "STORE receives a stock number rather than an old PROTECTION address");
    const auto files = host.run();
    check(files.request && files.request->arguments.size() == 3 &&
              files.request->arguments[0].number() == 1 &&
              files.request->arguments[2].number() == 3 &&
              host.resume_host(files.request->id, {}) && host.run().state == EclState::completed,
          "Opcode 37 remains a three-number LOAD FILES rather than VGA FIND NPC");
}

void installed_records()
{
    const auto folder = opengold::test::installed_game_folder(game);
    if (!std::filesystem::exists(folder / "DKK.CFG"))
    {
        std::cout << "Death Knights installed-data checks skipped: " << folder << '\n';
        return;
    }
    check(opengold::identify_game(folder) == game, "DKK.CFG takes precedence over SOUND.DAX");
    const auto stores = opengold::death_knights::decode_stores(
        opengold::por::unpack_sound_executable(read(folder / "START.EXE")));
    check(
        stores && stores->size() == 8 && stores->back().size() == 30 && stores->back().back() == 75,
        "All native shop entries come from the installed program, including its complete final list");
    const auto creatures = opengold::por::CreatureCatalog::load(folder, game);
    unsigned books = 0;
    for (const auto &[id, creature] : creatures.all())
    {
        (void)id;
        books += !creature.stored.raw_spell_block.empty();
    }
    check(creatures.all().size() == 64 && books == 14,
          "Every separate native spell block is attached to its encounter creature");
    const auto campaign = opengold::load_campaign(folder, game);
    check(campaign.resources()->item_definitions.size() == 91 &&
              campaign.resources()->stores.size() == 8,
          "The real campaign loads its complete equipment and shop definitions");
    check(campaign.resources()->gateway_wilderness &&
              campaign.resources()->wilderness_art.size() == 38 &&
              campaign.resources()->area_map_marker &&
              campaign.resources()->area_map_marker->width == 8,
          "The wilderness uses Death Knights' own 38 tiles and native eight-pixel cursor");
    check(campaign.resources()->shared_sprites.size() == 4 &&
              campaign.resources()->animations.contains(83) &&
              !campaign.resources()->animations.at(83).empty(),
          "The native shared sprites and seventh picture bank load across all districts");
    check(opengold::title_music_piece(game) == 1,
          "The native introduction starts MUSIC.DAX piece one");
    auto music = opengold::load_music_bank(folder, game, 1);
    check(!music.songs.empty() && !music.instruments.empty(),
          "Read the actual introduction music and its instrument bank");
    for (auto &track : music.songs.front().tracks)
    {
        std::erase_if(track.events,
                      [](const auto &event)
                      {
                          return event.start_tick > 150;
                      });
        track.length_ticks = 150;
    }
    const auto samples = opengold::render_music(music, 0, 8000);
    check(std::ranges::any_of(samples,
                              [](auto sample)
                              {
                                  return sample != 0;
                              }),
          "The installed native introduction score produces audible samples");

    const auto monsters = opengold::decode_dax_archive(read(folder / "MON1CHA.DAX"));
    check(monsters && monsters.records.size() == 64, "Read all 64 actual creature records");
    for (const auto &raw : monsters.records)
    {
        const auto creature = opengold::death_knights::decode_creature(raw.bytes);
        check(creature && !creature->name.empty() && creature->raw == raw.bytes,
              "Actual creature fields retain their original record");
    }
    const auto spells = opengold::decode_dax_archive(read(folder / "MON1WIZ.DAX"));
    check(spells && spells.records.size() == 14, "Read all 14 actual spell records");
    for (const auto &raw : spells.records)
    {
        check(opengold::death_knights::decode_spells(raw.bytes).has_value(),
              "Every physical WIZ payload decodes");
    }
    const auto image = opengold::por::unpack_sound_executable(read(folder / "START.EXE"));
    const auto words = opengold::por::decode_item_words(image, game);
    const auto names = opengold::por::decode_spell_names(image, game);
    check(words && words->size() == 126 && names && names->size() == 115 &&
              (*names)[2] == "Barkskin" && (*names)[36].empty() && !(*names)[37].empty(),
          "Read the changed spell numbering through empty slots and all 125 item words");
    check(opengold::por::ecl_opcode(0x22, game).operands == 0 &&
              opengold::por::ecl_opcode(0x21, game).operands == 3 &&
              opengold::por::ecl_opcode(0x0C, game).operands == 4,
          "Native YES NO, LOAD FILES and SETUP MONSTER differ independently");
    check(opengold::take_census(folder, game).complete(),
          "The entire installed census is complete");
}

void native_creation_and_progression_tables()
{
    using namespace opengold::original;
    using A = opengold::rules::Ability;
    check(starting_experience(Profession::knight, game) == 210003 &&
              starting_experience(Profession::cleric_magic_user, game) == 105002 &&
              starting_experience(Profession::cleric_fighter_magic_user, game) == 70001,
          "Native creation uses its exact rounded XP shares");
    const auto elves = professions_for(Race::silvanesti_elf, game);
    const auto dwarves = professions_for(Race::mountain_dwarf, game);
    check(std::ranges::find(elves, Profession::paladin) != elves.end() &&
              std::ranges::find(dwarves, Profession::paladin) != dwarves.end() &&
              death_knights_alignment_allowed(Profession::thief, Alignment::chaotic_neutral) &&
              !death_knights_alignment_allowed(Profession::knight, Alignment::chaotic_neutral),
          "Death Knights adds its own paladin and alignment choices");
    check(ability_limits(Race::qualinesti_elf, Gender::male, game).minimum[A::wisdom] == 6 &&
              ability_limits(Race::qualinesti_elf, Gender::male,
                             opengold::GoldBoxGame::champions_of_krynn)
                      .minimum[A::wisdom] == 3 &&
              class_minimums(Profession::magic_user, game)[A::charisma] == 6,
          "New class and race floors do not change Champions");
    opengold::rules::AbilityArray<int> scores;
    for (const auto ability : opengold::rules::all_abilities)
    {
        scores[ability] = 18;
    }
    check(racial_level_limit(Race::krynn_human, ClassSlot::fighter, scores, game) == 14 &&
              racial_level_limit(Race::krynn_human, ClassSlot::thief, scores, game) == 18 &&
              racial_level_limit(Race::kender, ClassSlot::ranger, scores, game) == 5 &&
              death_knights_experience_for_level(ClassSlot::monk, 8, {0, 0, 1}) == 220000,
          "Racial caps and knight-order XP use Death Knights' tables");
    ClassLevels levels{};
    levels[static_cast<std::size_t>(ClassSlot::cleric)] = 5;
    const auto cleric = death_knights_base_slots(levels, 18, {1, 0, 0});
    check(cleric[0][0] == 5 && cleric[0][1] == 5 && cleric[0][2] == 2,
          "A good cleric gains the native class slots and Wisdom bonuses once");
    levels = {};
    levels[static_cast<std::size_t>(ClassSlot::magic_user)] = 18;
    check(death_knights_base_slots(levels, 18, {0, 1, 0})[3][7] > 0,
          "The mage progression retains eighth-level spells");
    levels = {};
    levels[static_cast<std::size_t>(ClassSlot::ranger)] = 14;
    const auto ranger = death_knights_base_slots(levels, 18, {});
    check(ranger[1][0] > 0 && ranger[3][0] > 0,
          "Ranger progression separates nature spells from its arcane spells");
}

void native_spell_numbers_reuse_existing_mechanics()
{
    using namespace opengold::original;
    const int barkskin = canonical_spell(2, game);
    const int resurrection = canonical_spell(7, game);
    check(spell(barkskin) && spell(barkskin)->caster_class == SpellClass::druid &&
              spell_id(resurrection) == "cleric:resurrection" &&
              native_spell(barkskin, game) == 2 && native_spell(resurrection, game) == 7,
          "Changed native slots select the correct existing mechanics in both directions");
    check(canonical_spell(36, game) == 0 && !spell(canonical_spell(36, game), game),
          "Empty native slots do not become an unrelated earlier spell");
    const auto offered = castable_spells(game);
    for (int raw = 1; raw <= 114; ++raw)
    {
        const int canonical = canonical_spell(raw, game);
        if (canonical > 0)
        {
            check(spell(canonical, game) &&
                      canonical_spell(native_spell(canonical, game), game) == canonical &&
                      std::ranges::find(offered, canonical) != offered.end(),
                  "Every nonempty native spell maps to a supported reversible canonical spell");
        }
    }
    check(canonical_spell(101, game) == 501 && spell(501, game)->level == 0 &&
              native_spell(501, game) == 101,
          "Deity gifts retain their independent allowance and native slot numbers");
    const auto *bolt = spell(canonical_spell(51, game), game);
    check(bolt && bolt->range_per_level == 1 &&
              spell(51, opengold::GoldBoxGame::champions_of_krynn)->range_per_level == 1 &&
              canonical_spell(2, opengold::GoldBoxGame::champions_of_krynn) == 2,
          "Native spell metadata and aliases leave Champions unchanged");
}

void native_creation_profiles_and_training_use_the_right_game()
{
    using namespace opengold;
    using namespace original;
    constexpr auto game = GoldBoxGame::death_knights_of_krynn;
    const auto creation = character_rules(Aging::original, game);
    Content content;
    content.game = game;
    const auto module = rules_module(std::move(content));
    check(game_of(creation->identity()) == game,
          "Death Knights has its own persisted rules identity");
    const auto races = creation->choices(rules::CreationField::race);
    check(races.size() == 7, "Death Knights offers exactly its seven Krynn races");
    RandomState random{271};
    unsigned created = 0;
    for (const auto &race : races)
    {
        for (const auto &profession : creation->choices(rules::CreationField::character_class))
        {
            rules::CharacterDraft draft;
            draft.name = "Native hero";
            draft.race = race.id;
            draft.gender = "male";
            if (!creation->allows(draft, rules::CreationField::character_class, profession.id))
            {
                continue;
            }
            draft.character_class = profession.id;
            draft.background =
                (race.id == "mountain_dwarf" || race.id == "hill_dwarf") ? "reorx" : "mishakal";
            if (!creation->allows(draft, rules::CreationField::background, draft.background))
            {
                draft.background.clear();
            }
            draft.alignment = draft.background == "reorx" ? "neutral" : "lawful_good";
            if (!creation->allows(draft, rules::CreationField::alignment, draft.alignment))
            {
                draft.alignment = "neutral_good";
            }
            creation->roll_in_place(draft, random);
            const auto sheet = creation->evaluate(draft, rules::NameRequirement::required);
            const auto &n = *sheet.original;
            check(game_of(sheet.identity) == game && sheet.hit_points > 0 && n.platinum == 500,
                  "Every eligible native class creates a live character with 500 steel");
            check(record_race(static_cast<Race>(n.race), game) < 7 &&
                      race_from_record(record_race(static_cast<Race>(n.race), game), game) ==
                          static_cast<Race>(n.race),
                  "Native race numbers round-trip through the new rules identity");
            check(n.experience == starting_experience(static_cast<Profession>(n.profession), game),
                  "Creation retains exact rounded native XP shares");
            if (profession.id == "knight")
            {
                check(n.knight_order == 1 && n.class_levels[7] == 7 &&
                          sheet.level_classes.front() == "knight",
                      "A new Crown Knight reaches seven and never becomes a monk");
            }
            if (profession.id == "magic_user")
            {
                check(n.class_levels[5] == 8 && n.robe_order == 1 && n.magic_user_slots[3] > 0 &&
                          std::ranges::find(n.known_spells, canonical_spell(10, game)) !=
                              n.known_spells.end() &&
                          std::ranges::find(n.known_spells, canonical_spell(87, game)) !=
                              n.known_spells.end(),
                      "A white mage starts at eight with native initial and trained spells");
            }
            const auto profile = Combatant::decode(module->character_profile(sheet, {}).data);
            check(profile && profile->max_hit_points == sheet.hit_points && !profile->large,
                  "Every native player character fights as a normal one-square creature");
            ++created;
        }
    }
    check(created > 40, "The creation integration checks every supported race/class combination");
    ClassLevels knight{};
    knight[7] = 14;
    check(thac0(knight, game) == 7 && saving_throws(knight, game)[0] == 5,
          "High-level knights use their own attack and save tables");
}

void moon_allowances_follow_the_native_calendar()
{
    using namespace opengold;
    using namespace original;
    const auto creation = character_rules(Aging::original, game);
    Content content;
    content.game = game;
    const auto module = rules_module(std::move(content));
    rules::CharacterDraft draft;
    draft.name = "Moon mage";
    draft.race = "krynn_human";
    draft.gender = "male";
    draft.character_class = "magic_user";
    draft.alignment = "lawful_good";
    RandomState random{991};
    creation->roll_in_place(draft, random);
    auto sheet = creation->evaluate(draft, rules::NameRequirement::required);
    auto &n = *sheet.original;
    check(n.krynn_moon_phases == std::array<int, 3>{2, 2, 2} &&
              death_knights_moon_allowance(n) == 2,
          "A new game starts at full moons with two shared bonus preparations");
    const auto options = module->spell_choice_options(sheet, rules::SpellChoiceContext::long_rest);
    unsigned base = 0;
    for (const auto count : n.magic_user_slots)
    {
        base += static_cast<unsigned>(count);
    }
    check(options.prepared_count == base + 2,
          "The moon grants two total slots, never two per spell level");
    rules::SpellChoices selected;
    selected.prepared = std::vector<std::string>{};
    for (int copy = 1; copy <= n.magic_user_slots[0] + 2; ++copy)
    {
        selected.prepared->push_back("magic_user:magic_missile" +
                                     (copy > 1 ? "#" + std::to_string(copy) : ""));
    }
    module->apply_spell_choices(sheet, selected, rules::SpellChoiceContext::long_rest,
                                rules::ChoiceCompleteness::complete);
    auto excessive = selected;
    // Spending both extras at level one leaves none at any other level.
    for (int copy = 1; copy <= n.magic_user_slots[2] + 1; ++copy)
    {
        excessive.prepared->push_back("magic_user:fireball" +
                                      (copy > 1 ? "#" + std::to_string(copy) : ""));
    }
    bool refused = false;
    try
    {
        module->apply_spell_choices(sheet, excessive, rules::SpellChoiceContext::long_rest,
                                    rules::ChoiceCompleteness::complete);
    }
    catch (const std::runtime_error &)
    {
        refused = true;
    }
    check(refused && sheet.prepared_spells == *selected.prepared,
          "Cross-level over-allocation is rejected atomically");
    rules::VitalState state;
    state.hit_points = sheet.hit_points;
    check(module->clock_changes(sheet, state, 0, 719).empty(),
          "Noon-based time has not crossed midnight at minute 719");
    const auto midnight = module->clock_changes(sheet, state, 719, 720);
    check(midnight.size() == 1 && module->advance_character(sheet, state, midnight.front()) &&
              n.krynn_calendar_days == 1,
          "The calendar change is an ordinary replayable rules outcome at midnight");
    const auto after = module->clock_changes(sheet, state, 720, 9 * 1440);
    check(after.size() == 1 && module->advance_character(sheet, state, after.front()) &&
              n.krynn_moon_phases == std::array<int, 3>{3, 2, 3} &&
              death_knights_moon_allowance(n) == 0 &&
              sheet.prepared_spells.size() == static_cast<std::size_t>(n.magic_user_slots[0]),
          "The 9/2/7-day moon periods advance independently and unavailable extras are forgotten");
    auto replay = creation->evaluate(draft, rules::NameRequirement::required);
    replay.prepared_spells = *selected.prepared;
    check(module->advance_character(replay, state, after.front()) &&
              replay.original->krynn_moon_phases == n.krynn_moon_phases &&
              replay.prepared_spells == sheet.prepared_spells,
          "A combined calendar outcome replays to the same phase and prepared spells");
    death_knights_calendar(n, 27);
    check(death_knights_moon_allowance(n) == 1, "The waxing white moon allows one shared extra");
    n.class_levels[5] = 0;
    check(death_knights_moon_allowance(n) == 0, "A ranger or non-mage gets no robe-moon allowance");
}

void native_monster_effects_and_casting_survive_combat()
{
    using namespace opengold;
    using namespace original;
    check(
        canonical_effect(67, game) == 85 && canonical_effect(68, game) == 86 &&
            canonical_effect(65, game) == 335 && canonical_effect(93, game) == 115,
        "Native drains, electrical immunity and skeletal weapon resistance select their actual mechanics");
    check(canonical_effect(67, GoldBoxGame::champions_of_krynn) != canonical_effect(67, game),
          "A Death Knights drain never becomes Champions' Baaz weapon seizure");
    ClassLevels mage{};
    mage[5] = 8;
    check(death_knights_caster_level(mage, SpellClass::magic_user, 1, 0) == 7 &&
              death_knights_caster_level(mage, SpellClass::magic_user, 1, 1) == 8 &&
              death_knights_caster_level(mage, SpellClass::magic_user, 1, 2) == 9,
          "Low, waxing and full moons change casting level independently from preparation slots");
    mage[5] = 5;
    check(death_knights_caster_level(mage, SpellClass::magic_user, 1, 2) == 5,
          "A mage below six receives no full-moon casting-level increase");
    const auto moon_duration = [](std::uint64_t days)
    {
        const auto creation = character_rules(Aging::original, game);
        rules::CharacterDraft draft;
        draft.name = "Caster";
        draft.race = "krynn_human";
        draft.gender = "male";
        draft.character_class = "magic_user";
        draft.alignment = "lawful_good";
        RandomState random{721};
        creation->roll_in_place(draft, random);
        auto sheet = creation->evaluate(draft, rules::NameRequirement::required);
        death_knights_calendar(*sheet.original, days);
        sheet.prepared_spells = {"magic_user:enlarge"};
        Content content;
        content.game = game;
        const auto module = rules_module(std::move(content));
        rules::Encounter encounter;
        encounter.battlefield = {8, 8, std::vector<rules::Terrain>(64, rules::Terrain::open)};
        rules::Participant caster;
        caster.id = 1;
        caster.name = sheet.name;
        caster.side = rules::Side::party;
        caster.cell = {1, 1};
        caster.character_profile = module->character_profile(sheet, {}).data;
        Combatant dummy;
        dummy.hit_points = dummy.max_hit_points = 200;
        dummy.movement = 12;
        rules::Participant target;
        target.id = 2;
        target.name = "Observer";
        target.side = rules::Side::opposition;
        target.cell = {6, 6};
        target.character_profile = dummy.encode();
        encounter.participants = {caster, target};
        encounter.time_minutes = days * 1440;
        auto fight = module->create(std::move(encounter), 59);
        bool casting = false;
        unsigned cast_round = 0;
        for (unsigned step = 0; step < 30; ++step)
        {
            const auto snapshot = fight->snapshot();
            const auto actor = std::ranges::find(snapshot.combatants, rules::EntityId{1},
                                                 &rules::CombatantView::id);
            const auto profile = Combatant::decode(actor->persistent.resources);
            if (profile)
            {
                if (const auto effect = std::ranges::find(profile->effects, 12, &Effect::code);
                    effect != profile->effects.end())
                {
                    auto restored = module->restore(fight->save());
                    check(
                        restored->save() == fight->save(),
                        "A live fight preserves the new native moon context without dropping old profile data");
                    return std::pair{effect->minutes, snapshot.round - cast_round};
                }
            }
            const auto commands = fight->legal_commands();
            const auto command =
                std::ranges::find_if(commands,
                                     [&](const auto &c)
                                     {
                                         return snapshot.actor != 1 ? c.verb == "end"
                                                : !casting ? c.verb == "cast" && c.item == 12
                                                           : c.verb == "aim" && c.target == 1;
                                     });
            check(command != commands.end(),
                  "A native prepared Enlarge is available through legal combat controls");
            const auto verb = command->verb;
            if (verb == "aim")
            {
                cast_round = snapshot.round;
            }
            check(fight->submit(*command), "Execute the actual native spell command");
            casting = casting || verb == "cast";
        }
        throw std::runtime_error("Native Enlarge did not resolve");
    };
    const auto full = moon_duration(0), low = moon_duration(18);
    const auto *enlarge = spell(12, game);
    check(
        full.first + static_cast<int>(full.second) ==
                enlarge->minutes_fixed + 9 * enlarge->minutes_per_level &&
            low.first + static_cast<int>(low.second) ==
                enlarge->minutes_fixed + 7 * enlarge->minutes_per_level,
        "Actual combat spell duration uses the moon-adjusted level, while class level stays eight: full=" +
            std::to_string(full.first) + ", low=" + std::to_string(low.first) +
            ", per-level=" + std::to_string(enlarge->minutes_per_level));

    Content content;
    content.game = game;
    const auto module = rules_module(std::move(content));
    Combatant hero;
    hero.thac0 = -20;
    hero.hit_points = hero.max_hit_points = 200;
    hero.movement = 12;
    hero.saving_throws.fill(21);
    hero.attacks[0] = {2, 1, 1, 5};
    Combatant nightmare = hero;
    nightmare.attacks[0] = {2, 1, 1, 0};
    nightmare.effects = {{canonical_effect(106, game), 0, 255, false}};
    rules::Encounter encounter;
    encounter.battlefield = {6, 6, std::vector<rules::Terrain>(36, rules::Terrain::open)};
    rules::Participant pc;
    pc.id = 1;
    pc.side = rules::Side::party;
    pc.cell = {1, 1};
    pc.name = "Hero";
    pc.character_profile = hero.encode();
    rules::Participant foe;
    foe.id = 2;
    foe.side = rules::Side::opposition;
    foe.cell = {2, 1};
    foe.name = "Nightmare";
    foe.character_profile = nightmare.encode();
    encounter.participants = {pc, foe};
    auto fight = module->create(std::move(encounter), 417);
    bool smoked = false, hit = false;
    for (unsigned step = 0; step < 80 && !hit; ++step)
    {
        const auto before = fight->snapshot();
        const auto victim =
            std::ranges::find(before.combatants, rules::EntityId{1}, &rules::CombatantView::id);
        const auto profile = Combatant::decode(victim->persistent.resources);
        smoked =
            smoked || (profile && std::ranges::find(profile->effects, death_knights_effect::smoked,
                                                    &Effect::code) != profile->effects.end());
        const auto hp =
            std::ranges::find(before.combatants, rules::EntityId{2}, &rules::CombatantView::id)
                ->hit_points;
        const auto commands = fight->legal_commands();
        const auto chosen = std::ranges::find_if(commands,
                                                 [&](const auto &c)
                                                 {
                                                     return before.actor == 2 ? c.verb == "quick"
                                                            : smoked
                                                                ? c.verb == "melee" && c.target == 2
                                                                : c.verb == "end";
                                                 });
        check(chosen != commands.end(),
              "Nightmare smoke and the hero's attack use legal combat commands");
        const bool attacking = chosen->verb == "melee";
        check(fight->submit(*chosen), "Execute the smoke encounter");
        const auto after = fight->snapshot();
        const int damage =
            hp - std::ranges::find(after.combatants, rules::EntityId{2}, &rules::CombatantView::id)
                     ->hit_points;
        if (attacking && damage > 0)
        {
            check(
                damage == 4,
                "Nightmare smoke takes two from actual weapon damage, independently of ordinary Blindness");
            hit = true;
        }
    }
    check(smoked && hit,
          "The real nightmare AI applies smoke and its victim fights with the native penalty");
    const auto restored = module->restore(fight->save());
    check(restored->save() == fight->save(),
          "A smoked fight resumes with identical native effects and RNG continuation");
}

void a_saved_fight_advances_the_native_campaign_calendar()
{
    using namespace opengold;
    using namespace original;
    std::shared_ptr<const rules::CharacterRules> creation = character_rules(Aging::original, game);
    Content content;
    content.game = game;
    auto party = std::make_shared<CampaignParty>(rules_module(std::move(content)), creation);
    CharacterCreator creator(creation, 92);
    creator.select(rules::CreationField::race, "krynn_human");
    creator.select(rules::CreationField::character_class, "magic_user");
    creator.name("Calendar mage");
    test::walk_original_creation(creator);
    const auto member = party->add_pc(creator.create_character());
    constexpr std::uint64_t before_midnight = 9 * 1440 - 720 - 1;
    party->advance_time(std::chrono::minutes{before_midnight});
    check(party->member(member).character.sheet().original->krynn_moon_phases[0] == 2,
          "The campaign is one minute before the white moon wanes");
    rules::Encounter encounter;
    encounter.battlefield = {8, 8, std::vector<rules::Terrain>(64, rules::Terrain::open)};
    encounter.participants = party->participants();
    encounter.participants.front().cell = {1, 1};
    encounter.time_minutes = party->state().time_minutes;
    Combatant dummy;
    dummy.hit_points = dummy.max_hit_points = 200;
    dummy.movement = 12;
    rules::Participant observer;
    observer.id = 1000;
    observer.name = "Observer";
    observer.side = rules::Side::opposition;
    observer.cell = {6, 6};
    observer.character_profile = dummy.encode();
    encounter.participants.push_back(observer);
    const auto &module = party->rule_module();
    auto fight = module.create(std::move(encounter), 599);
    party->begin_combat();
    for (unsigned step = 0; step < 30 && fight->snapshot().elapsed_milliseconds == 0; ++step)
    {
        const auto commands = fight->legal_commands();
        const auto end = std::ranges::find(commands, std::string{"end"}, &rules::Command::verb);
        check(end != commands.end() && fight->submit(*end),
              "Both actors wait through a real completed combat round");
    }
    const auto snapshot = fight->snapshot();
    check(snapshot.elapsed_milliseconds >= 60000,
          "Death Knights advances one native minute for a completed round");
    const auto actor = std::ranges::find(snapshot.combatants, member, &rules::CombatantView::id);
    const auto numbers = Combatant::decode(actor->persistent.resources);
    check(numbers &&
              std::ranges::find(numbers->effects, death_knights_effect::casting_moon, &Effect::code)
                      ->strength == 7,
          "A live mage's casting context follows the moon through midnight");
    party->apply_combat(snapshot);
    check(party->state().time_minutes == before_midnight + snapshot.elapsed_milliseconds / 60000 &&
              party->member(member).character.sheet().original->krynn_calendar_days == 9 &&
              party->member(member).character.sheet().original->krynn_moon_phases[0] == 3,
          "Active party members receive the rules calendar outcome exactly once");
    const auto once = party->state().time_minutes;
    party->apply_combat(snapshot);
    check(party->state().time_minutes == once,
          "Reapplying a combat snapshot does not advance time again");
    const auto save = fight->save();
    check(save.find("OGORIGINALCOMBAT 28") != std::string::npos,
          "The native calendar checkpoint records its context explicitly");
    auto restored = module.restore(save);
    check(restored->save() == save, "Native calendar and new effects round-trip byte-for-byte");
    for (unsigned step = 0; step < 8; ++step)
    {
        const auto commands = fight->legal_commands();
        const auto end = std::ranges::find(commands, std::string{"end"}, &rules::Command::verb);
        check(end != commands.end() && fight->submit(*end) && restored->submit(*end) &&
                  restored->save() == fight->save(),
              "Resumed combat preserves subsequent clock, moon and RNG progression");
    }
    party->end_combat();
}

void knight_petitions_use_death_knights_high_level_progression()
{
    using namespace opengold;
    using namespace opengold::original;
    Content content;
    content.game = game;
    const auto module = rules_module(content);
    check(original_training_fee(game) == 0 &&
              original_mask_classes(0x10, game) == std::vector<std::string_view>{"knight"} &&
              original_mask_classes(0x40, game) == std::vector<std::string_view>{"paladin"} &&
              original_mask_classes(0x80, game) == std::vector<std::string_view>{"ranger"},
          "Native training masks distinguish knights, paladins and rangers");
    rules::CharacterSheet knight;
    knight.identity = identity(game);
    knight.name = "Senior knight";
    knight.level = 11;
    knight.hit_points = 144;
    knight.level_classes.assign(11, "knight");
    for (const auto ability : rules::all_abilities)
    {
        knight.base[ability] = knight.scores[ability] = 18;
    }
    knight.original = rules::CharacterSheet::Original{};
    auto &numbers = *knight.original;
    numbers.race = static_cast<int>(Race::krynn_human);
    numbers.profession = static_cast<int>(Profession::knight);
    numbers.class_levels[7] = 11;
    numbers.knight_order = 1;
    numbers.rolled_hit_points = 100;
    const auto threshold = death_knights_experience_for_level(ClassSlot::monk, 11, {0, 0, 2});
    check(threshold.has_value(), "Death Knights retains progression above Champions' level cap");
    numbers.experience = *threshold - 1;
    const auto choices =
        module->hall_training_options(knight, std::array<std::string, 1>{"knight"});
    check(choices.options.size() == 1, "An experienced Crown knight can petition for the Sword");
    module->train_at_hall(knight, choices.options.front().id);
    check(
        knight.original->knight_order == 2 && knight.original->class_levels[7] == 10 &&
            knight.original->rolled_hit_points == 98 && knight.hit_points == 142 &&
            knight.level == 10,
        "Above level nine an unaffordable knight promotion loses exactly two HP, without a Constitution loss");
    rules::AbilityArray<int> prime;
    prime[rules::Ability::wisdom] = 15;
    check(experience_share(Profession::cleric, prime, 100, game) == 110 &&
              experience_share(Profession::knight, prime, 100, game) == 100,
          "Death Knights clerics get their Wisdom-fifteen bonus and knights no prime bonus");
}

void actual_champions_winners_keep_their_earned_state()
{
    using namespace opengold;
    using namespace opengold::original;
    const auto *source = std::getenv("OPENGOLD_CHAMPIONS_WINNERS");
    if (!source || !*source)
    {
        std::cout << "Actual Champions-winner import skipped: set OPENGOLD_CHAMPIONS_WINNERS.\n";
        return;
    }
    constexpr auto champions = GoldBoxGame::champions_of_krynn;
    const auto champions_folder = opengold::test::installed_game_folder(champions);
    const auto death_knights_folder = opengold::test::installed_game_folder(game);
    const auto source_creation = character_rules(default_aging, champions);
    const auto source_rules = rules_module(load_original_content(champions_folder, champions));
    const auto source_world = load_campaign(champions_folder, champions);
    const auto saved = decode_campaign(read_campaign_file(source), *source_creation, *source_rules,
                                       campaign_asset_identity(champions_folder), &source_world);
    check(saved.game == champions,
          "Read the real Champions winning campaign under its original rules");
    std::shared_ptr<const rules::CharacterRules> creation = character_rules(default_aging, game);
    const auto content = load_original_content(death_knights_folder, game);
    auto party = std::make_shared<CampaignParty>(rules_module(content), creation);
    const auto convert = champions_items_for_death_knights(death_knights_folder);
    std::vector<const PartyMember *> winners;
    for (const auto slot : saved.party.slots)
    {
        const auto member = std::ranges::find(saved.party.roster, slot, &PartyMember::id);
        if (slot && member != saved.party.roster.end() && member->npc_source.empty())
        {
            winners.push_back(&*member);
        }
    }
    check(winners.size() == 6, "All six actual earned Champions PCs are available for import");
    unsigned wounded = 0;
    for (std::size_t index = 0; index < winners.size(); ++index)
    {
        const auto &from = *winners[index];
        const auto id = party->bring_across(
            from, convert,
            champions_money_for_death_knights(saved.party.wealth, index, winners.size()),
            CampaignParty::ConvertedItems::readied_as_they_were);
        const auto &to = party->member(id);
        const auto &before = from.character.sheet();
        const auto &after = to.character.sheet();
        check(
            to.experience == from.experience &&
                after.original->class_levels == before.original->class_levels &&
                after.original->rolled_hit_points == before.original->rolled_hit_points &&
                after.prepared_spells == before.prepared_spells &&
                after.hit_points == before.hit_points &&
                to.vitals.hit_points == from.vitals.hit_points &&
                to.vitals.dead == from.vitals.dead && after.original->platinum == 0,
            "Imported winners keep earned levels, XP, rolled/max/current HP without fresh-character money");
        wounded += to.vitals.hit_points < after.hit_points;
        std::vector<por::ItemRecord> expected;
        for (const auto &item : from.character.inventory().items())
        {
            if (const auto converted = convert(from.item_sources.at(item.id)))
            {
                expected.push_back(converted->stored);
            }
        }
        const auto &items = to.character.inventory().items();
        check(items.size() == expected.size(),
              "The import retains exactly the surviving original items");
        for (std::size_t item = 0; item < items.size(); ++item)
        {
            check(to.item_sources.at(items[item].id).stored.raw == expected[item].raw,
                  "Each retained item's native record survives byte for byte");
        }
        auto aged = before;
        aged.original->age = 3000;
        const auto draft = creation->imported_draft(aged, from.experience);
        check(draft &&
                  creation->evaluate(*draft, rules::NameRequirement::required).original->age == 50,
              "Native human imports cap age at fifty without changing earned state");
    }
    check(wounded > 0,
          "The real winning party includes a wound, so this check detects an import heal");
    check(party->state().wealth == saved.party.wealth,
          "All seven money denominations survive exactly once, including remainder coins");
    const auto assets = campaign_asset_identity(death_knights_folder);
    // The ordinary party-screen save before the opening battle. Campaign
    // exploration saves additionally require finishing that real battle.
    const auto bytes = encode_campaign(*party, nullptr, assets, game);
    auto reloaded = decode_campaign(bytes, *creation, *rules_module(content), assets, nullptr);
    auto restored = std::make_shared<CampaignParty>(rules_module(content), creation);
    restored->restore(std::move(reloaded.party));
    check(!reloaded.town && encode_campaign(*restored, nullptr, assets, game) == bytes,
          "The actual imported winning party roundtrips in an ordinary party-screen save");
    std::cout << "Imported all six actual Champions winners with exact wounds, items and purse.\n";
    const auto before_opening = party->checkpoint();
    for (const bool rest : {false, true})
    {
        party->restore(before_opening);
        auto opening = load_campaign(death_knights_folder, game);
        opening.campaign_party(party);
        bool battle = false;
        for (unsigned screen = 0; screen < 200; ++screen)
        {
            if (party->state().spell_rest)
            {
                const auto choosing = party->state().spell_rest->members;
                for (const auto id : choosing)
                {
                    const auto from =
                        std::ranges::find(before_opening.roster, id, &PartyMember::id);
                    check(from != before_opening.roster.end(),
                          "The resting caster came from the actual import");
                    party->choose_spells(id, {{}, from->character.sheet().prepared_spells});
                }
            }
            opening.advance(1);
            const auto &snapshot = opening.snapshot();
            check(snapshot.phase != por::TourPhase::faulted,
                  "The actual imported party runs the opening: " + snapshot.diagnostic);
            if (snapshot.phase == por::TourPhase::combat)
            {
                battle = true;
                break;
            }
            if (snapshot.phase == por::TourPhase::awaiting_continue)
            {
                const auto answer = std::ranges::find(snapshot.choices, rest ? "YES" : "NO");
                check(
                    opening.choose(snapshot.continue_ticket,
                                   answer == snapshot.choices.end()
                                       ? 0U
                                       : static_cast<unsigned>(answer - snapshot.choices.begin())),
                    "Continue the native introduction and answer its rest question");
            }
        }
        check(battle, "Both rest answers reach the actual opening battle");
        check(party->state().wealth == before_opening.wealth,
              "Native robe and knight-order fields never change the party's coins");
        check(!rest || party->state().time_minutes >= before_opening.time_minutes + 480,
              "Accepting the initial camp passes the normal night's rest and resumes the script");
        if (rest)
        {
            for (const auto id : party->state().slots)
            {
                if (!id)
                {
                    continue;
                }
                const auto &sheet = party->member(id).character.sheet();
                if (!sheet.prepared_spells.empty())
                {
                    check(!party->memorized_spells(id).empty(),
                          "The native opening rest retains the imported casters' preparation");
                }
            }
        }
    }
    // Isolate the native ending branches here; the earned route separately
    // proves the battles. The pre-battle flag alone must never award a win.
    for (const unsigned previous_script : {17u, 98u})
    {
        party->restore(before_opening);
        const auto native = load_campaign(death_knights_folder, game);
        auto resources = std::make_shared<por::PhlanResources>(*native.resources());
        resources->opening_script = previous_script;
        // An empty fixture entry supplies an idle predecessor. The council
        // itself runs the installed ECL48, without fabricated battle results.
        por::RolfTourSession council(por::GeoMap{}, script({0x13}), {}, 0, {}, resources);
        council.campaign_party(party);
        council.advance(1);
        check(council.can_leave(), "The council fixture begins between events");
        test::TourAccess::set_variable(council, por::EclAddress{0x4C68}, 255);
        test::TourAccess::set_variable(council, por::EclAddress{0x4CD0}, 1);
        test::TourAccess::enter(council, 48, {0, 1, por::MapDirection::east});
        bool won = false, closing_words = false;
        for (unsigned screen = 0; screen < 200; ++screen)
        {
            council.advance(1);
            const auto snapshot = council.snapshot();
            check(snapshot.phase != por::TourPhase::faulted,
                  "Run the native council ending: " + snapshot.diagnostic);
            closing_words = closing_words ||
                            snapshot.dialogue.find("THE MEETING BREAKS UP") != std::string::npos;
            if (snapshot.game_won)
            {
                check(closing_words && previous_script == 98,
                      "Victory waits for the actual council's final closing words");
                won = true;
            }
            if (council.can_leave())
            {
                break;
            }
            if (snapshot.phase == por::TourPhase::awaiting_continue)
            {
                check(council.choose(snapshot.continue_ticket, 0), "Continue the council");
            }
        }
        check(won == (previous_script == 98),
              "Only returning from Dargaard through the closing council announces victory");
        check(council.can_leave() && !council.snapshot().game_won,
              "Continue returns the winners to ordinary exploration");
    }
}
} // namespace

int main()
{
    try
    {
        independent_fields_and_separate_spells_survive();
        numbered_templates_and_complete_tile_sheets();
        later_commands_keep_the_earlier_memory_and_host_protocol();
        native_creation_and_progression_tables();
        native_spell_numbers_reuse_existing_mechanics();
        native_creation_profiles_and_training_use_the_right_game();
        moon_allowances_follow_the_native_calendar();
        native_monster_effects_and_casting_survive_combat();
        a_saved_fight_advances_the_native_campaign_calendar();
        knight_petitions_use_death_knights_high_level_progression();
        actual_champions_winners_keep_their_earned_state();
        installed_records();
        std::cout << "Death Knights record checks passed.\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
