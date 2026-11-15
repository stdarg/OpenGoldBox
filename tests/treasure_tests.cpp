// Treasures of the Savage Frontier's campaign (docs/games/treasure.md): its
// opening plays on the real files when OPENGOLD_GOLDBOX_DIR names the folder
// holding TREASURE.
#include "opengold/campaign_party.h"
#include "opengold/campaign_save.h"
#include "opengold/campaign_session.h"
#include "opengold/character_creator.h"
#include "opengold/ecl_machine.h"
#include "opengold/game_census.h"
#include "opengold/original.h"
#include "opengold/original_combat.h"
#include "opengold/original_content.h"
#include "opengold/pool_radiance.h"
#include "opengold/treasures_import.h"
#include "original_creation_support.h"
#include "test_support.h"
#include "tour_access.h"

#include <cstdlib>
#include <filesystem>
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
    const char *games = std::getenv("OPENGOLD_GOLDBOX_DIR");
    const auto folder = games ? std::filesystem::path(games) / "TREASURE" : std::filesystem::path();
    return games && opengold::identify_game(folder) == treasures_game ? folder
                                                                      : std::filesystem::path();
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

// A Gateway character comes to Treasures as its LoadChar takes one in: its
// levels, Hit Points and experience under Treasures' rules, and its items
// renumbered (a battle axe stays item type 1, named again by its words), but
// one of a type Treasures lacks (Gateway's 3) left behind, none readied.
void a_gateway_character_comes_across()
{
    const auto treasures = treasures_folder();
    const char *games = std::getenv("OPENGOLD_GOLDBOX_DIR");
    const auto gateway = games ? std::filesystem::path(games) / "GATEWAY" : std::filesystem::path();
    constexpr auto gateway_game = GoldBoxGame::gateway_to_the_savage_frontier;
    if (treasures.empty() || opengold::identify_game(gateway) != gateway_game)
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
    // again, the crown and the red and blue crystals held, for the camp
    // dialog's check (treasure_camp.cmake).
    if (const char *fixture = std::getenv("OPENGOLD_TREASURE_CAMP_FIXTURE"))
    {
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

} // namespace

int main()
{
    try
    {
        the_opening_reaches_the_fortress();
        the_camp_fixes_the_party();
        the_fix_restores_and_cures_feeblemind();
        the_overland_is_drawn_from_above();
        a_gateway_character_comes_across();
        std::cout << "Treasures passed.\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
