// Treasures of the Savage Frontier's campaign (docs/games/treasure.md): its
// opening plays on the real files when Treasures' folder is found
// (installed_games.h).
#include "opengold/campaign_party.h"
#include "opengold/campaign_save.h"
#include "opengold/campaign_session.h"
#include "opengold/character_creator.h"
#include "opengold/creature_catalog.h"
#include "opengold/ecl_machine.h"
#include "opengold/game_census.h"
#include "opengold/original.h"
#include "opengold/original_combat.h"
#include "opengold/original_content.h"
#include "opengold/pool_radiance.h"
#include "opengold/por_sound.h"
#include "opengold/treasures_import.h"
#include "original_creation_support.h"
#include "installed_games.h"
#include "test_support.h"
#include "tour_access.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <iostream>
#include <memory>
#include <set>
#include <string>

namespace
{
using opengold::GoldBoxGame;
using opengold::por::ExplorationCommand;
using opengold::por::TourPhase;
using opengold::test::check;

constexpr auto treasures_game = GoldBoxGame::treasures_of_the_savage_frontier;

std::filesystem::path treasures_folder()
{
    const auto folder = opengold::test::installed_game_folder(treasures_game);
    return !folder.empty() ? folder : std::filesystem::path();
}

// A party made under Treasures' original rules: a fighter, a paladin, a
// ranger, a cleric, a magic-user and a thief, each human.
std::shared_ptr<opengold::CampaignParty> treasures_party(const std::filesystem::path &folder)
{
    std::shared_ptr<const opengold::rules::CharacterRules> creation =
        opengold::original::character_rules(opengold::original::default_aging, treasures_game);
    auto party = std::make_shared<opengold::CampaignParty>(
        opengold::original::rules_module(opengold::load_original_content(folder, treasures_game)),
        creation);
    unsigned seed = 70;
    for (const std::string klass :
         {"fighter", "paladin", "ranger", "cleric", "magic_user", "thief"})
    {
        opengold::CharacterCreator creator(creation, ++seed);
        creator.select(opengold::rules::CreationField::race, "human");
        creator.select(opengold::rules::CreationField::character_class, klass);
        creator.name(klass);
        opengold::test::walk_original_creation(creator);
        static_cast<void>(party->add_pc(creator.create_character()));
    }
    return party;
}

std::size_t colours(const opengold::Image &image)
{
    std::set<std::uint32_t> seen;
    for (std::size_t p = 0; p + 3 < image.rgba.size(); p += 4)
    {
        seen.insert(image.rgba[p] << 16 | image.rgba[p + 1] << 8 | image.rgba[p + 2]);
    }
    return seen.size();
}

// A new game runs script 16: over pictures 2, 3 and 4 the party is taken from
// a hilltop above Yartar to the dwarves' fortress in Llorkh, each screen
// waiting for a key (WAIT KEY), and stands in map 16 at (10,7) facing north
// with the walls of the sets 9, 10 and 11 (LOAD WALL SETS 16, 9, 10, 11).
void the_opening_reaches_the_fortress()
{
    const auto folder = treasures_folder();
    if (folder.empty())
    {
        std::cout << "Treasures' files are not here; its opening is not played.\n";
        return;
    }
    auto session = opengold::load_campaign(folder, treasures_game);
    const auto party = treasures_party(folder);
    session.campaign_party(party);
    const auto &fighter = *party->member(party->state().slots[0]).character.sheet().original;
    check(fighter.experience == 22600 && fighter.class_levels[2] == 5,
          "The party is made under Treasures' rules: a fighter of level 5");
    const auto settle = [&session]
    {
        for (int step = 0; step < 50 && session.snapshot().phase == TourPhase::running; ++step)
        {
            session.advance(1.0);
        }
    };
    const auto diagnostics = [&session]
    {
        return session.snapshot().diagnostic +
               (session.script_diagnostics().empty() ? "" : session.script_diagnostics().front());
    };
    settle();
    check(session.snapshot().dialogue.find("LATE SUMMER IN THE VALLEY OF DESSARIN") !=
              std::string::npos,
          "The opening begins: " + session.snapshot().dialogue + diagnostics());
    check(session.picture().has_value() && colours(*session.picture()) > 16,
          "Picture 2 shows the valley in 256 colors: " + diagnostics());
    std::string told;
    for (int screen = 0; screen < 12 && session.snapshot().phase == TourPhase::awaiting_continue;
         ++screen)
    {
        told += session.snapshot().dialogue;
        check(session.choose(session.snapshot().continue_ticket, 0), "A key goes on");
        settle();
    }
    check(told.find("AMANITAS") != std::string::npos && told.find("MILZORR") != std::string::npos,
          "Amanitas and Milzorr speak: " + diagnostics());
    const auto &snapshot = session.snapshot();
    check(snapshot.phase == TourPhase::completed, "The opening ends: " + diagnostics());
    check(snapshot.area_id == 16, "Map 16 is loaded: " + diagnostics());
    check(snapshot.pose.x == 10 && snapshot.pose.y == 7 &&
              snapshot.pose.facing == opengold::por::MapDirection::north,
          "The party stands at (10,7) facing north");
    check(session.script_diagnostics().empty(), "The opening needs nothing unsupported");
    const auto view = session.observe_view();
    check(colours(view) > 8, "The 3D view shows the fortress's walls");
    check(session.explore(ExplorationCommand::turn_left), "The party turns");
    check(session.observe_view().rgba != view.rgba, "Turning shows another view");

    // Treasures has no ending of its own: the event in which Freezefire dies
    // (script 38 sets cell 25Ch to 1) wins the game as it ends. Here the cell
    // is set and a look around the fortress taken as that event.
    using opengold::test::TourAccess;
    TourAccess::set_variable(session, opengold::por::EclAddress{0x25C}, 1);
    check(session.explore(ExplorationCommand::look), "The party looks around");
    TourAccess::soul_freed_in_event(session);
    settle();
    for (int screen = 0; screen < 6 && session.snapshot().phase == TourPhase::awaiting_continue &&
                         !session.snapshot().game_won;
         ++screen)
    {
        check(session.choose(session.snapshot().continue_ticket, 0), "The words go on");
        settle();
    }
    check(session.snapshot().game_won && session.snapshot().dialogue == "You have won.",
          "Freezefire's fall wins the game: " + session.snapshot().dialogue);
    check(session.continue_dialogue(session.snapshot().continue_ticket) &&
              !session.snapshot().game_won,
          "The game goes on after its win");
}

// The overland: script 51 sets the overland cell (40h) and loads the map its
// cell 2EEh names, with that map's picture (OVER 51 to 57) for its walls; the
// view shows 11 by 11 squares of the picture around the party with its arrow.
void the_overland_is_drawn_from_above()
{
    using opengold::por::EclAddress;
    using opengold::por::MapDirection;
    using opengold::test::TourAccess;
    const auto folder = treasures_folder();
    if (folder.empty())
    {
        std::cout << "Treasures' files are not here; its overland is not drawn.\n";
        return;
    }
    auto session = opengold::load_campaign(folder, treasures_game);
    session.campaign_party(treasures_party(folder));
    const auto play = [&session]
    {
        for (int step = 0; step < 200; ++step)
        {
            const auto &snapshot = session.snapshot();
            if (snapshot.phase == TourPhase::running)
            {
                session.advance(1.0);
            }
            else if (snapshot.phase == TourPhase::awaiting_continue)
            {
                check(session.choose(snapshot.continue_ticket, 0), "The screen goes on");
            }
            else
            {
                return;
            }
        }
    };
    constexpr std::uint16_t overland_shown = 0x40, map_cell = 0x2EE, x_cell = 0x2EC, y_cell = 0x2ED;
    constexpr unsigned first_overland = 51;
    play();
    TourAccess::set_variable(session, EclAddress{map_cell}, first_overland);
    TourAccess::set_variable(session, EclAddress{x_cell}, 3);
    TourAccess::set_variable(session, EclAddress{y_cell}, 10);
    TourAccess::enter(session, first_overland, {3, 10, MapDirection::north});
    play();
    check(session.snapshot().area_id == first_overland && session.script_diagnostics().empty(),
          "The overland's script loads its first map: map " +
              std::to_string(session.snapshot().area_id) + " " + session.snapshot().diagnostic +
              (session.script_diagnostics().empty() ? "" : session.script_diagnostics().front()));
    check(TourAccess::machine(session).variable(EclAddress{overland_shown}) == 1,
          "The overland is shown");
    const auto view = session.observe_view();
    check(view.width == 88 && view.height == 88 && colours(view) > 16,
          "The overland is drawn from above in 256 colors");
    check(session.explore(ExplorationCommand::turn_left) &&
              session.observe_view().rgba != view.rgba,
          "The party's arrow turns with it");
    // Its map's colors turn as time passes (the picture's color cycle).
    const auto still = session.observe_view();
    const auto revision = session.snapshot().picture_revision;
    check(!session.turn_colors(0.0) && session.turn_colors(5.0) &&
              session.snapshot().picture_revision != revision &&
              session.observe_view().rgba != still.rgba,
          "The overland's colors turn as time passes");
}

// The real Waterdeep Dock escape grants 5 hundreds of experience to
// every member (ECL1:21, 8786h; eclvm.pas, EclAwardExperience).
void script_awards_count_in_hundreds()
{
    using namespace opengold::por;
    using opengold::test::TourAccess;
    const auto folder = treasures_folder();
    if (folder.empty())
    {
        return;
    }
    auto session = opengold::load_campaign(folder, treasures_game);
    auto party = treasures_party(folder);
    session.campaign_party(party);
    const auto play = [&]
    {
        for (unsigned step = 0; step < 200; ++step)
        {
            const auto snapshot = session.snapshot();
            if (snapshot.phase == TourPhase::running)
            {
                session.advance(1.0);
            }
            else if (snapshot.phase == TourPhase::awaiting_continue)
            {
                check(session.choose(snapshot.continue_ticket, 0), "Read the escape dialogue");
            }
            else
            {
                return;
            }
        }
        throw std::runtime_error("The escape dialogue did not finish");
    };
    play();
    const auto first = party->state().slots.front();
    const auto before = party->member(first).experience;
    TourAccess::set_variable(session, EclAddress{0x128}, 1);
    // NEW ECL records the previous script itself; enter from the caverns.
    TourAccess::enter(session, 22, {0, 15, MapDirection::north});
    play();
    TourAccess::enter(session, 21, {15, 6, MapDirection::south});
    play();
    check(party->member(first).experience - before == 500,
          "The actual cavern escape award of 5 gives each member 500 experience; got " +
              std::to_string(party->member(first).experience - before));

    // GetEclVar also preserves a word amount, before the host scales it.
    std::vector<std::uint8_t> bytes{0, 0};
    for (unsigned entry = 0; entry < 5; ++entry)
    {
        bytes.insert(bytes.end(), {1, 1, 0x14, 0x80});
    }
    bytes.insert(bytes.end(), {0x41, 0, 1, 2, 0xF4, 1, 0});
    EclMachine machine(std::make_shared<const EclProgram>(
        EclProgram::decode(bytes, "award-word", treasures_game)));
    machine.enable_host(65);
    check(machine.start(0), "Start the word award");
    static_cast<void>(machine.run());
    check(machine.pending_request() && machine.pending_request()->arguments.at(1).number() == 500,
          "A scripted award keeps its whole word amount");
}

void monster_loot_uses_treasures_item_names()
{
    const auto folder = treasures_folder();
    if (folder.empty())
    {
        return;
    }
    std::ifstream program(folder / std::string(opengold::item_words_program(treasures_game)),
                          std::ios::binary);
    check(static_cast<bool>(program), "Treasure's item-name program exists");
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(program), {}};
    const auto words = opengold::por::decode_item_words(
        opengold::por::unpack_sound_executable(bytes), treasures_game);
    check(words.has_value(), "Treasure's original item words exist");
    const auto catalog = opengold::por::CreatureCatalog::load(folder, treasures_game);
    unsigned compared = 0;
    for (const auto &[id, creature] : catalog.all())
    {
        for (const auto &item : creature.equipment)
        {
            if (!item.stored.stored_name.empty())
            {
                continue;
            }
            check(item.decoded_name == opengold::por::item_name(item.stored, *words),
                  "Treasure monster loot uses its own item words: " + id.key() + ": " +
                      item.label());
            ++compared;
        }
    }
    check(compared > 0, "The installed monsters supplied actual equipment names");
}

// The Leilon stairs put the party in a farmhouse without a NEW ECL.
// Continuing straight ahead must read the farmhouse, not the old stair event.
void a_scripted_stair_move_refreshes_the_next_step()
{
    using namespace opengold::por;
    using opengold::test::TourAccess;
    const auto folder = treasures_folder();
    if (folder.empty())
    {
        std::cout << "Treasures' files are not here; its stairs are not played.\n";
        return;
    }
    auto session = opengold::load_campaign(folder, treasures_game);
    session.campaign_party(treasures_party(folder));
    const auto play = [&]
    {
        for (unsigned step = 0; step < 100; ++step)
        {
            const auto snapshot = session.snapshot();
            if (snapshot.phase == TourPhase::running)
            {
                session.advance(1.0);
            }
            else if (snapshot.phase == TourPhase::awaiting_continue)
            {
                check(session.choose(snapshot.continue_ticket, 0), "Continue the stair dialogue");
            }
            else
            {
                check(snapshot.phase == TourPhase::completed,
                      "The stairs finish without another service: " + snapshot.diagnostic);
                return;
            }
        }
        throw std::runtime_error("The stair dialogue did not finish");
    };
    play();
    TourAccess::set_variable(session, EclAddress{0x2EB}, 1);
    TourAccess::set_variable(session, EclAddress{0x2EC}, 15);
    TourAccess::set_variable(session, EclAddress{0x2ED}, 6);
    TourAccess::enter(session, 20, {15, 6, MapDirection::east});
    play();
    if (session.snapshot().pose.x == 15 && session.snapshot().pose.y == 6)
    {
        check(session.explore(ExplorationCommand::forward), "Climb the actual Leilon stairs");
        play();
    }
    check(session.snapshot().pose.x == 4 && session.snapshot().pose.y == 3 &&
              session.snapshot().pose.facing == MapDirection::east,
          "The stairs deliver the party to the farmhouse facing east");
    check(session.explore(ExplorationCommand::forward), "Continue east without turning");
    play();
    check(session.snapshot().pose.x == 5 && session.snapshot().pose.y == 3,
          "The next forward step leaves the stairs instead of repeating them");
}

// Daggerford's released temple leaders explicitly waive their donation.
void daggerfords_freed_temple_heals_without_gold()
{
    using namespace opengold::por;
    using opengold::test::TourAccess;
    const auto folder = treasures_folder();
    if (folder.empty())
    {
        std::cout << "Treasures' files are not here; Daggerford's temple is not visited.\n";
        return;
    }
    auto session = opengold::load_campaign(folder, treasures_game);
    auto party = treasures_party(folder);
    auto wounded = party->checkpoint();
    wounded.wealth.fill(0);
    const auto fighter = wounded.slots.front();
    for (auto &member : wounded.roster)
    {
        if (member.id == fighter)
        {
            member.vitals.hit_points = 1;
        }
    }
    party->restore(std::move(wounded));
    session.campaign_party(party);
    const auto settle = [&]
    {
        for (unsigned step = 0; step < 100; ++step)
        {
            const auto snapshot = session.snapshot();
            if (snapshot.phase == TourPhase::running)
            {
                session.advance(1.0);
            }
            else if (snapshot.phase == TourPhase::awaiting_continue && snapshot.choices.size() == 1)
            {
                check(session.choose(snapshot.continue_ticket, 0), "Read the temple's dialogue");
            }
            else
            {
                return;
            }
        }
        throw std::runtime_error("The temple's dialogue did not finish");
    };
    const auto choose = [&](const std::string &label)
    {
        const auto snapshot = session.snapshot();
        const auto found = std::ranges::find(snapshot.choices, label);
        check(found != snapshot.choices.end(),
              "The temple offers " + label + ": " + snapshot.dialogue);
        check(session.choose(snapshot.continue_ticket,
                             static_cast<std::size_t>(found - snapshot.choices.begin())),
              "Choose the temple's " + label);
        settle();
    };
    settle();
    TourAccess::set_variable(session, EclAddress{0x2EB}, 1);
    TourAccess::set_variable(session, EclAddress{0x2EC}, 14);
    TourAccess::set_variable(session, EclAddress{0x2ED}, 10);
    TourAccess::enter(session, 39, {14, 10, MapDirection::east});
    settle();
    check(session.explore(ExplorationCommand::forward), "Enter the Temple of Eldath");
    settle();
    choose("HEAL");
    choose("Cure Light Wounds (100 gp)");
    choose("fighter");
    check(party->member(fighter).vitals.hit_points == 1 && party->purse_gold() == 0,
          "Before the rescue the penniless party cannot buy a cure");
    choose("Back");
    choose("Leave");
    // Isolated service fixture: the real headquarters sets this rescue flag.
    TourAccess::set_variable(session, EclAddress{0x265}, 1);
    check(session.explore(ExplorationCommand::forward), "Visit the freed temple leaders");
    settle();
    choose("HEAL");
    check(std::ranges::find(session.snapshot().choices, "Raise Dead (0 gp)") !=
              session.snapshot().choices.end(),
          "The freed temple waives even the raising donation");
    choose("Cure Light Wounds (0 gp)");
    choose("fighter");
    choose("Pay");
    check(party->member(fighter).vitals.hit_points > 1 && party->purse_gold() == 0,
          "The grateful temple actually heals the penniless party");
}

// A Gateway character comes to Treasures as its LoadChar takes one in: its
// levels, Hit Points and experience under Treasures' rules, and its items
// renumbered (a battle axe stays item type 1, named again by its words), but
// one of a type Treasures lacks (Gateway's 3) left behind, none readied.
void a_gateway_character_comes_across()
{
    const auto treasures = treasures_folder();
    constexpr auto gateway_game = GoldBoxGame::gateway_to_the_savage_frontier;
    const auto gateway = opengold::test::installed_game_folder(gateway_game);
    if (treasures.empty() || gateway.empty())
    {
        std::cout << "Gateway's or Treasures' files are not here; no one comes across.\n";
        return;
    }
    const auto gateway_content = opengold::load_original_content(gateway, gateway_game);
    std::shared_ptr<const opengold::rules::CharacterRules> gateway_creation =
        opengold::original::character_rules(opengold::original::default_aging, gateway_game);
    opengold::CampaignParty first(opengold::original::rules_module(gateway_content),
                                  gateway_creation);
    opengold::CharacterCreator creator(gateway_creation, 5);
    creator.select(opengold::rules::CreationField::race, "human");
    creator.select(opengold::rules::CreationField::character_class, "fighter");
    creator.name("Veteran");
    opengold::test::walk_original_creation(creator);
    const auto id = first.add_pc(creator.create_character());
    const auto item = [&](std::uint8_t type)
    {
        opengold::por::Equipment equipment;
        equipment.stored.type = type;
        equipment.stored.name_components = {type, 0, 0};
        // Gateway's record bytes: its type and its words (byte 46 on).
        equipment.stored.raw[46] = type;
        equipment.stored.raw[47] = type;
        equipment.base = gateway_content.item_types.at(type);
        return equipment;
    };
    constexpr std::uint8_t battle_axe = 1, lost_type = 3;
    first.take_item(id, item(battle_axe));
    first.take_item(id, item(lost_type));
    const auto &veteran = first.member(id);
    // OPENGOLD_GATEWAY_SAVE_FIXTURE writes the veteran's party, with money, as
    // a Gateway save for the party screen's check (treasure_from_gateway.cmake).
    if (const char *fixture = std::getenv("OPENGOLD_GATEWAY_SAVE_FIXTURE"))
    {
        auto wealth = first.state().wealth;
        wealth[static_cast<std::size_t>(opengold::Coin::platinum)] = 1234;
        wealth[static_cast<std::size_t>(opengold::Coin::gems)] = 6;
        wealth[static_cast<std::size_t>(opengold::Coin::jewelry)] = 2;
        first.set_wealth(wealth);
        opengold::write_campaign_file(
            fixture,
            opengold::encode_campaign(first, nullptr, opengold::campaign_asset_identity(gateway)));
    }

    std::shared_ptr<const opengold::rules::CharacterRules> creation =
        opengold::original::character_rules(opengold::original::default_aging, treasures_game);
    opengold::CampaignParty second(opengold::original::rules_module(
                                       opengold::load_original_content(treasures, treasures_game)),
                                   creation);
    const auto arrived =
        second.bring_across(veteran, opengold::gateway_items_for_treasures(treasures));
    const auto &member = second.member(arrived);
    const auto &sheet = member.character.sheet();
    check(sheet.original->class_levels == veteran.character.sheet().original->class_levels &&
              sheet.hit_points == veteran.character.sheet().hit_points &&
              member.experience == veteran.experience &&
              opengold::original::game_of(sheet.identity) == treasures_game,
          "Its levels, Hit Points and experience come along, under Treasures' rules");
    const auto &items = member.character.inventory().items();
    check(items.size() == 1 && member.equipped.empty(),
          "The battle axe comes along unreadied, the item Treasures lacks stays behind");
    const auto &axe = member.item_sources.at(items.front().id);
    check(axe.stored.type == battle_axe && axe.stored.name_components[0] == battle_axe,
          "The axe keeps its type and its word, as Treasures numbers them");
    // Its record's bytes, which the inventory keeps it by, say the same: an
    // item's type and words Treasures renumbers are rewritten there too.
    const auto kept = opengold::por::item_of_original_key(items.front().definition_id);
    check(kept && kept->type == axe.stored.type &&
              kept->name_components == axe.stored.name_components,
          "The axe's record bytes hold its Treasures type and words");
    // An item whose type Treasures numbers otherwise is known by its new
    // number, its record's bytes rewritten with it.
    const auto convert = opengold::gateway_items_for_treasures(treasures);
    bool renumbered = false;
    for (unsigned type = 7; type <= 127 && !renumbered; ++type)
    {
        auto gateway_item =
            item(static_cast<std::uint8_t>(type < gateway_content.item_types.size() ? type : 1));
        gateway_item.stored.name_components = {battle_axe, 0, 0};
        gateway_item.stored.raw[47] = battle_axe;
        const auto converted = convert(gateway_item);
        if (!converted || converted->stored.type == gateway_item.stored.type)
        {
            continue;
        }
        renumbered = true;
        const auto known = opengold::por::item_of_original_key(
            opengold::por::original_item_key(converted->stored));
        check(known && known->type == converted->stored.type,
              "A renumbered item's record bytes hold its Treasures type");
    }
    check(renumbered, "Some Gateway item type is numbered otherwise in Treasures");
    check(second.state().wealth[static_cast<std::size_t>(opengold::Coin::platinum)] == 200,
          "With the option off it starts with Treasures' 200 platinum");

    // With the Gateway's-money option on, it keeps its own platinum, gems and
    // jewelry instead.
    opengold::CampaignParty third(opengold::original::rules_module(
                                      opengold::load_original_content(treasures, treasures_game)),
                                  creation);
    (void)third.bring_across(veteran, opengold::gateway_items_for_treasures(treasures),
                             opengold::CampaignParty::BroughtMoney{1234, 5, 2});
    const auto &wealth = third.state().wealth;
    check(wealth[static_cast<std::size_t>(opengold::Coin::platinum)] == 1234 &&
              wealth[static_cast<std::size_t>(opengold::Coin::gems)] == 5 &&
              wealth[static_cast<std::size_t>(opengold::Coin::jewelry)] == 2,
          "With the option on it keeps its own platinum, gems and jewelry");
}

// The camp's Fix (camp.pas, CampFixMenu): the party rests while its cleric's
// spell slots heal the wounded, a quarter hour for each first-level slot, and
// spends no memorized spell. The camp's Crown tells the luck once the party
// has Amanitas's crown (cell 3Fh).
void the_camp_fixes_the_party()
{
    const auto folder = treasures_folder();
    if (folder.empty())
    {
        return;
    }
    using opengold::test::TourAccess;
    auto session = opengold::load_campaign(folder, treasures_game);
    const auto party = treasures_party(folder);
    session.campaign_party(party);
    const auto settle = [&session]
    {
        for (int step = 0; step < 50 && session.snapshot().phase == TourPhase::running; ++step)
        {
            session.advance(1.0);
        }
        for (int screen = 0;
             screen < 12 && session.snapshot().phase == TourPhase::awaiting_continue; ++screen)
        {
            check(session.choose(session.snapshot().continue_ticket, 0), "A key goes on");
            for (int step = 0; step < 50 && session.snapshot().phase == TourPhase::running; ++step)
            {
                session.advance(1.0);
            }
        }
    };
    settle();
    check(session.can_leave(), "The opening ends in the fortress");
    const auto fighter = party->state().slots[0];
    auto wounded = party->checkpoint();
    for (auto &member : wounded.roster)
    {
        if (member.id == fighter)
        {
            member.vitals.hit_points = 1;
        }
    }
    party->restore(std::move(wounded));
    // No interruption: the rest is not rolled for.
    TourAccess::set_variable(session, opengold::por::EclAddress{0x2B}, 0);
    const auto cleric = party->state().slots[3];
    const auto spells_before = party->camp_actions(cleric).size();
    const auto minutes_before = party->state().time_minutes;
    check(session.camp_offers_fix() && session.camp_fix(), "The camp's Fix begins");
    settle();
    check(party->member(fighter).vitals.hit_points > 1,
          "The cleric's slots heal the fighter: " + session.snapshot().dialogue);
    check(party->state().time_minutes > minutes_before &&
              party->state().time_minutes <= minutes_before + 24 * 60,
          "The Fix takes the time of its casting, not a night");
    check(party->camp_actions(cleric).size() == spells_before, "The Fix spends no memorized spell");

    check(!session.camp_offers_crown(), "No crown, no reading");
    TourAccess::set_variable(session, opengold::por::EclAddress{0x3F}, 1);
    check(session.camp_offers_crown() && session.camp_crown() &&
              !session.snapshot().dialogue.empty(),
          "The crown tells the luck: " + session.snapshot().dialogue);
    // The camp names the crystals the party holds (cell 3Ch: 1 red, 2
    // green, 4 blue).
    check(session.camp_crystals() == 0, "No crystal yet");
    TourAccess::set_variable(session, opengold::por::EclAddress{0x3C}, 5);
    check(session.camp_crystals() == 5, "The red and blue crystals held");

    // OPENGOLD_TREASURE_CAMP_FIXTURE saves the camp, the fighter wounded
    // again, the crown and the red and blue crystals held, and the fighter
    // and the paladin in love, their scene due at the next camp, for the camp
    // dialog's check (treasure_camp.cmake).
    if (const char *fixture = std::getenv("OPENGOLD_TREASURE_CAMP_FIXTURE"))
    {
        TourAccess::set_variable(session, opengold::por::EclAddress{0x105}, 21);
        TourAccess::set_string(session, opengold::por::EclAddress{0xE5}, "FIGHTER");
        TourAccess::set_string(session, opengold::por::EclAddress{0xF5}, "PALADIN");
        TourAccess::set_variable(session, opengold::por::EclAddress{0x106}, 30);
        TourAccess::set_variable(session, opengold::por::EclAddress{0x107}, 60);
        auto hurt = party->checkpoint();
        for (auto &member : hurt.roster)
        {
            if (member.id == fighter)
            {
                member.vitals.hit_points = 1;
            }
        }
        party->restore(std::move(hurt));
        opengold::write_campaign_file(
            fixture,
            opengold::encode_campaign(*party, &session, opengold::campaign_asset_identity(folder),
                                      treasures_game));
    }
}

// The Fix's second rest (camp.pas, CampFixMenu): a cleric's seventh-level
// slots each give a drained level back by Restoration, and its sixth-level
// ones cure feeblemind, with blindness, as Heal does, healing nothing.
// Treasures' clerics stop at level 10, short of those slots, so the party is
// made under Pools of Darkness's rules, Treasures' engine's, which let a
// cleric reach them.
void the_fix_restores_and_cures_feeblemind()
{
    const auto folder = treasures_folder();
    if (folder.empty())
    {
        return;
    }
    using opengold::MemberId;
    using opengold::RandomState;
    using opengold::test::TourAccess;
    auto session = opengold::load_campaign(folder, treasures_game);
    constexpr auto darkness = GoldBoxGame::pools_of_darkness;
    opengold::original::Content content;
    content.game = darkness;
    std::shared_ptr<const opengold::rules::CharacterRules> creation =
        opengold::original::character_rules(opengold::original::default_aging, darkness);
    const auto party = std::make_shared<opengold::CampaignParty>(
        opengold::original::rules_module(content), creation);
    const auto &module = party->rule_module();
    // A cleric of Wisdom 18, which seventh-level spells need, trained until
    // it has them.
    MemberId cleric{};
    for (std::uint64_t seed = 1; seed < 400 && !cleric; ++seed)
    {
        opengold::rules::CharacterDraft draft;
        draft.race = "human";
        draft.gender = "female";
        draft.alignment = "neutral_good";
        draft.character_class = "cleric";
        draft.name = "Ilsa";
        RandomState random{seed};
        creation->roll_in_place(draft, random);
        const opengold::Character candidate(*creation, draft, {});
        if (candidate.sheet().scores[opengold::rules::Ability::wisdom] >= 18)
        {
            cleric = party->add_pc(candidate);
        }
    }
    check(cleric != MemberId{}, "A cleric of Wisdom 18 is rolled");
    const auto made = [&](const std::string &klass, unsigned seed)
    {
        opengold::CharacterCreator creator(creation, seed);
        creator.select(opengold::rules::CreationField::race, "human");
        creator.select(opengold::rules::CreationField::character_class, klass);
        creator.name(klass);
        opengold::test::walk_original_creation(creator);
        return party->add_pc(creator.create_character());
    };
    const auto fighter = made("fighter", 71);
    const auto mage = made("magic_user", 75);
    const auto before = party->member(fighter).character.sheet();
    // The fighter loses two levels to a drain; the magic-user is
    // feebleminded and blind; the cleric is wounded, so the Fix runs.
    auto drained =
        *opengold::original::Combatant::decode(module.character_profile(before, {}).data);
    drained.class_levels[2] -= 2;
    drained.max_hit_points -= before.hit_points / 3;
    const opengold::rules::VitalState after{drained.max_hit_points, false, drained.encode(), {}};
    const auto drain = module.lasting_changes(before, after);
    check(drain.size() == 1, "The drain is a lasting change");
    constexpr int blind = 33, feebleminded = 268;
    auto state = party->checkpoint();
    RandomState random{41};
    for (auto &member : state.roster)
    {
        if (member.id == fighter)
        {
            check(member.character.advance(module, member.vitals, drain.front()),
                  "The fighter takes the drain");
        }
        if (member.id == mage)
        {
            const std::vector<opengold::original::Effect> conditions{{feebleminded, 0, 0, false},
                                                                     {blind, 0, 0, false}};
            member.vitals.resources = opengold::original::rested_state(conditions);
        }
        while (member.id == cleric && member.character.sheet().original->cleric_slots[6] == 0 &&
               member.character.sheet().original->class_levels[0] < 20)
        {
            const auto &sheet = member.character.sheet();
            auto training = module.default_advancement(sheet);
            module.roll_advancement(sheet, 10'000'000, training, random);
            check(member.character.advance(module, member.vitals, training), "The cleric trains");
        }
        if (member.id == cleric)
        {
            member.vitals.hit_points = 1;
        }
    }
    party->restore(std::move(state));
    check(party->member(cleric).character.sheet().original->cleric_slots[6] > 0,
          "The cleric casts seventh-level spells");
    check(party->temple_service_helps(mage, opengold::rules::TempleService::cure_feeblemind),
          "The magic-user is feebleminded");
    session.campaign_party(party);
    for (int step = 0; step < 50 && session.snapshot().phase == TourPhase::running; ++step)
    {
        session.advance(1.0);
    }
    for (int screen = 0; screen < 12 && session.snapshot().phase == TourPhase::awaiting_continue;
         ++screen)
    {
        check(session.choose(session.snapshot().continue_ticket, 0), "A key goes on");
        for (int step = 0; step < 50 && session.snapshot().phase == TourPhase::running; ++step)
        {
            session.advance(1.0);
        }
    }
    TourAccess::set_variable(session, opengold::por::EclAddress{0x2B}, 0);
    check(session.camp_offers_fix() && session.camp_fix(), "The camp's Fix begins");
    for (int step = 0; step < 50 && session.snapshot().phase == TourPhase::running; ++step)
    {
        session.advance(1.0);
    }
    // One seventh-level slot, one level back.
    const auto slots_of_seventh =
        static_cast<int>(party->member(cleric).character.sheet().original->cleric_slots[6]);
    const auto &restored = *party->member(fighter).character.sheet().original;
    check(restored.class_levels[2] ==
              before.original->class_levels[2] - 2 + std::min(slots_of_seventh, 2),
          "Restoration gives a level back for each seventh-level slot: " +
              std::to_string(restored.class_levels[2]) + " of " +
              std::to_string(before.original->class_levels[2]) + ", " +
              std::to_string(slots_of_seventh) + " slots");
    check(!party->temple_service_helps(mage, opengold::rules::TempleService::cure_feeblemind) &&
              !party->temple_service_helps(mage, opengold::rules::TempleService::cure_blindness),
          "Heal's slot cures the magic-user's feeblemind and blindness");
    check(session.snapshot().dialogue.find("magic_user is healed") != std::string::npos,
          "The Fix tells of it: " + session.snapshot().dialogue);
}

// Treasures' romance at a camp (romance.pas, UpdateRelationship): once a
// script has named the NPC (cell 105h) and the pair (cells E5h and F5h), a
// camp tells the LUCK file's scene for the two loves' bands of 25 (cells 106h
// and 107h) when one has changed, and only then; at the lowest band of one
// and the highest of the other the NPC leaves; at the highest of both it
// proposes, and refused it leaves, the member losing 6 Charisma for good.
// Here the fighter stands for the party's lover and the paladin for the NPC.
void the_camp_tells_the_romance()
{
    const auto folder = treasures_folder();
    if (folder.empty())
    {
        return;
    }
    using opengold::por::EclAddress;
    using opengold::test::TourAccess;
    auto session = opengold::load_campaign(folder, treasures_game);
    const auto party = treasures_party(folder);
    session.campaign_party(party);
    const auto run = [&session, &party]
    {
        for (int step = 0; step < 50 && session.snapshot().phase == TourPhase::running; ++step)
        {
            // The casters keep the spells they had as each rest ends.
            if (party->state().spell_rest)
            {
                for (const auto id : std::vector(party->state().spell_rest->members))
                {
                    party->keep_rest_spells(id);
                }
            }
            session.advance(1.0);
        }
    };
    // Through every screen, answering the proposal with `answer`; what was told.
    const auto settle = [&](std::size_t answer = 0)
    {
        std::string told;
        run();
        for (int screen = 0;
             screen < 12 && session.snapshot().phase == TourPhase::awaiting_continue; ++screen)
        {
            told += session.snapshot().dialogue + "\n";
            const auto choice = session.snapshot().choices.size() > 1 ? answer : 0;
            check(session.choose(session.snapshot().continue_ticket, choice), "A key goes on");
            run();
        }
        return told;
    };
    settle();
    TourAccess::set_variable(session, EclAddress{0x2B}, 0);
    const auto fighter = party->state().slots[0], paladin = party->state().slots[1];
    TourAccess::set_variable(session, EclAddress{0x105}, 21);
    TourAccess::set_string(session, EclAddress{0xE5}, "FIGHTER");
    TourAccess::set_string(session, EclAddress{0xF5}, "PALADIN");
    const auto camp_with =
        [&](std::uint16_t member_love, std::uint16_t npc_love, std::size_t answer = 0)
    {
        TourAccess::set_variable(session, EclAddress{0x106}, member_love);
        TourAccess::set_variable(session, EclAddress{0x107}, npc_love);
        check(session.camp(opengold::RestKind::long_rest), "The party camps");
        return settle(answer);
    };
    const auto in_party = [&](opengold::MemberId id)
    {
        const auto &slots = party->state().slots;
        return std::ranges::find(slots, id) != slots.end();
    };
    // Bands 1 and 2: their scene names the lovers, the stronger first.
    auto told = camp_with(30, 60);
    check(told.find("PALADIN") != std::string::npos && told.find("FIGHTER") != std::string::npos,
          "The camp tells the scene of the pair's bands: " + told);
    check(session.script_variable(EclAddress{0x4C}) == 2 &&
              session.script_variable(EclAddress{0x4D}) == 1,
          "The bands are kept (cells 4Ch and 4Dh)");
    check(camp_with(40, 70).find("FIGHTER") == std::string::npos,
          "Loves moved within their bands tell nothing");
    // The member loves completely and is not loved: the NPC leaves.
    told = camp_with(100, 10);
    check(!told.empty() && !in_party(paladin), "Unloved, the NPC leaves the party: " + told);

    // The proposal at the top bands of both, refused.
    TourAccess::set_variable(session, EclAddress{0x4C}, 0);
    TourAccess::set_variable(session, EclAddress{0x4D}, 0);
    TourAccess::set_string(session, EclAddress{0xF5}, "RANGER");
    const auto ranger = party->state().slots[2];
    const int charisma =
        party->member(fighter).character.sheet().scores[opengold::rules::Ability::charisma];
    TourAccess::set_variable(session, EclAddress{0x106}, 100);
    TourAccess::set_variable(session, EclAddress{0x107}, 100);
    check(session.camp(opengold::RestKind::long_rest), "The party camps");
    run();
    check(session.snapshot().choices.size() == 2 && session.picture().has_value(),
          "The pair proposes over its picture, the party to accept or refuse it");
    told = settle(1);
    check(!in_party(ranger) &&
              party->member(fighter).character.sheet().scores[opengold::rules::Ability::charisma] ==
                  std::max(charisma - 6, 0),
          "Refused, the NPC leaves and the member loses 6 Charisma: " + told);
}

// Treasures' camp menu (eclvm.pas, EnterCamp; camp.pas, CampMainMenu): as
// it opens, the map's camp entry runs and the romance's scene is told, before
// the player chooses anything, so the Crown alone hears it too. A rest from
// the open menu runs neither again; the next opening tells the next scene.
void the_camp_menu_tells_the_romance_as_it_opens()
{
    const auto folder = treasures_folder();
    if (folder.empty())
    {
        return;
    }
    using opengold::por::EclAddress;
    using opengold::test::TourAccess;
    auto session = opengold::load_campaign(folder, treasures_game);
    const auto party = treasures_party(folder);
    session.campaign_party(party);
    const auto run = [&session, &party]
    {
        for (int step = 0; step < 50 && session.snapshot().phase == TourPhase::running; ++step)
        {
            if (party->state().spell_rest)
            {
                for (const auto id : std::vector(party->state().spell_rest->members))
                {
                    party->keep_rest_spells(id);
                }
            }
            session.advance(1.0);
        }
    };
    const auto settle = [&]
    {
        std::string told;
        run();
        for (int screen = 0;
             screen < 12 && session.snapshot().phase == TourPhase::awaiting_continue; ++screen)
        {
            told += session.snapshot().dialogue + "\n";
            check(session.choose(session.snapshot().continue_ticket, 0), "A key goes on");
            run();
        }
        return told;
    };
    settle();
    TourAccess::set_variable(session, EclAddress{0x2B}, 0);
    TourAccess::set_variable(session, EclAddress{0x105}, 21);
    TourAccess::set_string(session, EclAddress{0xE5}, "FIGHTER");
    TourAccess::set_string(session, EclAddress{0xF5}, "PALADIN");
    TourAccess::set_variable(session, EclAddress{0x106}, 30);
    TourAccess::set_variable(session, EclAddress{0x107}, 60);
    TourAccess::set_variable(session, EclAddress{0x3F}, 1);

    check(session.open_camp(), "The camp menu opens");
    run();
    check(session.snapshot().phase == TourPhase::awaiting_continue &&
              session.snapshot().dialogue.find("PALADIN") != std::string::npos &&
              !session.snapshot().camp_menu_open,
          "The romance's scene comes before the menu: " + session.snapshot().dialogue);
    settle();
    check(session.snapshot().phase == TourPhase::completed && session.snapshot().camp_menu_open,
          "The menu is open once the scene is told");
    const auto minutes = party->state().time_minutes;
    check(session.camp_crown() && !session.snapshot().dialogue.empty(),
          "The Crown alone tells the luck after the scene");
    check(party->state().time_minutes == minutes, "Opening the menu takes no time");

    // Loves moved to other bands while the menu was open: the rest tells
    // nothing, the next opening does.
    check(session.open_camp(), "The camp menu opens again");
    check(settle().find("PALADIN") == std::string::npos, "Bands unchanged, no scene");
    TourAccess::set_variable(session, EclAddress{0x106}, 60);
    TourAccess::set_variable(session, EclAddress{0x107}, 60);
    check(session.camp(opengold::RestKind::long_rest), "The party rests from the menu");
    const auto rested = settle();
    check(rested.find("PALADIN") == std::string::npos && party->state().time_minutes > minutes,
          "The rest tells no scene: " + rested);
    check(session.open_camp(), "The camp menu opens a third time");
    check(settle().find("PALADIN") != std::string::npos, "The next opening tells the new bands");
}

// Treasures' pictures (its portrait.pas, LoadAnimation): the PIC record cell
// 4Eh names is drawn in the colors of the record cell 4Fh names, one of the
// records that only set colors (each town's script sets its own, script 16
// picture 10, the town's street).
void a_picture_takes_a_color_records_colors()
{
    const auto folder = treasures_folder();
    if (folder.empty())
    {
        return;
    }
    using opengold::por::EclAddress;
    using opengold::test::TourAccess;
    auto session = opengold::load_campaign(folder, treasures_game);
    session.campaign_party(treasures_party(folder));
    for (int step = 0; step < 200 && session.snapshot().phase != TourPhase::completed; ++step)
    {
        if (session.snapshot().phase == TourPhase::awaiting_continue)
        {
            check(session.choose(session.snapshot().continue_ticket, 0), "A key goes on");
        }
        session.advance(1.0);
    }
    const auto image = [&session](std::uint16_t colors)
    {
        TourAccess::set_variable(session, EclAddress{0x4E}, 10);
        TourAccess::set_variable(session, EclAddress{0x4F}, colors);
        const auto *frames = session.pic_frames(10);
        check(frames && !frames->empty(), "Picture 10 is there");
        return frames->front().image.rgba;
    };
    const auto own = image(0), yartar = image(0x30), leilon = image(0x39);
    check(own != yartar && yartar != leilon,
          "Picture 10 takes the colors of the record cell 4Fh names");
    TourAccess::set_variable(session, EclAddress{0x4E}, 11);
    check(session.pic_frames(10)->front().image.rgba == own,
          "Another picture than cell 4Eh's keeps its own colors");
}

} // namespace

int main()
{
    try
    {
        the_opening_reaches_the_fortress();
        the_camp_fixes_the_party();
        the_camp_tells_the_romance();
        the_camp_menu_tells_the_romance_as_it_opens();
        a_picture_takes_a_color_records_colors();
        the_fix_restores_and_cures_feeblemind();
        the_overland_is_drawn_from_above();
        script_awards_count_in_hundreds();
        monster_loot_uses_treasures_item_names();
        a_scripted_stair_move_refreshes_the_next_step();
        daggerfords_freed_temple_heals_without_gold();
        a_gateway_character_comes_across();
        std::cout << "Treasures passed.\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
