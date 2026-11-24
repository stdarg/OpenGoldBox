#include "dark_queen_route.h"
#include <string_view>

namespace opengold::test
{
bool dark_queen_scripts_share_map(unsigned area, unsigned from, unsigned to)
{
    return (area == 38 && from >= 38 && from <= 40 && to >= 38 && to <= 40) ||
           (area == 42 && from >= 42 && from <= 43 && to >= 42 && to <= 43) ||
           (area == 26 && from >= 26 && from <= 28 && to >= 26 && to <= 28) ||
           (area == 30 && from >= 30 && from <= 32 && to >= 30 && to <= 32) ||
           (area == 34 && from >= 34 && from <= 36 && to >= 34 && to <= 36) ||
           (area == 54 && from >= 54 && from <= 55 && to >= 54 && to <= 55) ||
           (area == 50 && from >= 50 && from <= 52 && to >= 50 && to <= 52) ||
           (area == 58 && from >= 58 && from <= 61 && to >= 58 && to <= 61) ||
           ((area == 62 || area == 63) && from >= 62 && from <= 65 && to >= 62 && to <= 65) ||
           ((area == 46 || area == 47 || area == 48) && from >= 46 && from <= 49 && to >= 46 &&
            to <= 49);
}

std::vector<RouteBeat> dark_queen_earned_route(DarkQueenProgress progress)
{
    using D = opengold::por::MapDirection;
    if (progress == DarkQueenProgress::opening)
    {
        return {};
    }
    RouteBeat recover{4, 3, 2, D::south, {}, true};
    recover.arrive_only = true;
    recover.rest_for_spells = true;
    recover.cure_first = true;
    recover.cure_to_full = true;
    recover.memorize_first_of = {"read_magic", "cure_light_wounds", "heal",
                                 "bless",      "fireball",          "magic_missile"};
    RouteBeat attack{4, 3, 8, D::south, {}};
    attack.arrive_only = true;
    attack.searching = false;
    attack.reloads_until_won = 120;
    std::vector<RouteBeat> beats{recover, attack};
    if (progress == DarkQueenProgress::caergoth)
    {
        return beats;
    }
    recover.x = 3;
    recover.y = 8;
    beats.push_back(recover);
    RouteBeat captain{4, 8, 14, D::south, {"YES"}};
    captain.arrive_only = true;
    captain.searching = false;
    beats.push_back(captain);
    if (progress == DarkQueenProgress::caves)
    {
        return beats;
    }
    recover.map = 10;
    recover.x = 11;
    recover.y = 27;
    recover.memorize_first_of.insert(recover.memorize_first_of.begin(), "dispel_magic");
    beats.push_back(recover);
    const auto cave_fight = [&](unsigned x, unsigned y, std::vector<std::string> answers)
    {
        RouteBeat fight{10, x, y, D::north, std::move(answers)};
        fight.arrive_only = true;
        fight.searching = false;
        fight.cure_first = true;
        fight.cure_to_full = true;
        fight.reloads_until_won = 120;
        beats.push_back(std::move(fight));
        // The cluebook permits uninterrupted rest at the entrance. Walk
        // back through cleared chambers, recover there, then continue.
        RouteBeat return_to_entrance{10, 11, 27, D::south, {}};
        return_to_entrance.arrive_only = true;
        return_to_entrance.searching = false;
        return_to_entrance.cure_first = true;
        return_to_entrance.cure_to_full = true;
        return_to_entrance.reloads_until_won = 120;
        if (x == 5 && y == 15)
        {
            // The cleared caves' store contains the native White Mage
            // scroll (253), including Globe of Invulnerability. The next
            // beat returns to the entrance and rests to scribe it.
            return_to_entrance.x = 5;
            return_to_entrance.y = 20;
            return_to_entrance.take_items = {{"White", "magic_user 6"}};
            return_to_entrance.scribe_after = {"globe_of_invulnerability"};
        }
        beats.push_back(return_to_entrance);
        beats.push_back(recover);
    };
    cave_fight(10, 22, {});
    cave_fight(8, 17, {});
    // Crysia's fireball is dispelled by a cleric with Dispel Magic prepared
    // (the recovery memorizes it first); the White mage has none.
    cave_fight(5, 15, {"CAST DISPEL MAGIC", "cleric 4"});
    RouteBeat cave_exit{10, 24, 0, D::north, {"CAST DISPEL MAGIC", "cleric 4", "YES"}};
    cave_exit.arrive_only = true;
    cave_exit.searching = false;
    cave_exit.cure_first = true;
    cave_exit.cure_to_full = true;
    cave_exit.reloads_until_won = 120;
    beats.push_back(cave_exit);
    if (progress == DarkQueenProgress::sea_elves)
    {
        return beats;
    }
    RouteBeat acclimate{14, 11, 17, D::east, {"YES"}};
    acclimate.repeat_in_place = true;
    acclimate.repeat_until_bits = std::pair<std::uint16_t, std::uint16_t>{0x9E, 2};
    acclimate.take_items = {{"Ring of Free Action", "knight 1"}};
    beats.push_back(acclimate);
    if (progress == DarkQueenProgress::naulidis)
    {
        return beats;
    }
    RouteBeat hall{14, 5, 17, D::west, {"YES", "cleric 4"}};
    hall.arrive_only = true;
    hall.searching = false;
    hall.cure_first = true;
    hall.cure_to_full = true;
    hall.reloads_until_won = 120;
    hall.take_items = {{"Ring of Free Action", "knight 2"}};
    beats.push_back(hall);
    if (progress == DarkQueenProgress::safe_hall)
    {
        return beats;
    }
    RouteBeat hinge_job{14, 5, 17, D::east, {"YES"}};
    hinge_job.take_items = {{"Ring of Free Action", "knight 2"}};
    beats.push_back(hinge_job);
    RouteBeat workshop{14, 17, 25, D::south, {"YES"}};
    workshop.cure_first = true;
    workshop.cure_to_full = true;
    workshop.reloads_until_won = 120;
    beats.push_back(workshop);
    hall.arrive_only = false;
    beats.push_back(hall);
    if (progress == DarkQueenProgress::hinge)
    {
        return beats;
    }
    RouteBeat assistance = hall;
    assistance.rest_first = true;
    assistance.rest_for_spells = true;
    assistance.answers = {"YES", "cleric 4", "magic_user 6"};
    assistance.dialogue_answers = {{"NEED ANY YOURSELF", "NO"}, {"NEED ANY MORE", "NO"}};
    assistance.memorize_first_of = {"cure_light_wounds", "heal", "dispel_magic",
                                    "cure_serious_wounds"};
    for (unsigned visit = 0; visit < 8; ++visit)
    {
        // Escort people found between the two halls into safety. These
        // ordinary journeys also encounter the city's other aid scenes.
        assistance.x = visit % 2 == 0 ? 3 : 5;
        assistance.y = visit % 2 == 0 ? 9 : 17;
        auto aid = assistance;
        aid.rest_first = visit % 4 != 2;
        if (visit % 4 == 1)
        {
            aid.x = 17;
            aid.y = 19;
            aid.answers = {"LEAVE"};
        }
        beats.push_back(aid);
    }
    // The boy's sister is in one of the guardrooms. Search the rooms and
    // escort the children back before expecting another aid assignment.
    for (const auto [x, y] : {std::pair{7U, 20U}, std::pair{6U, 22U}, std::pair{12U, 15U}})
    {
        RouteBeat guardroom{14, x, y, D::east, {"YES"}};
        guardroom.cure_first = true;
        guardroom.cure_to_full = true;
        guardroom.reloads_until_won = 120;
        beats.push_back(guardroom);
    }
    beats.push_back(hall);
    for (unsigned visit = 0; visit < 8; ++visit)
    {
        if (visit == 4)
        {
            // The hinge errand is offered when leaving a safe hall facing
            // east. Looking west inside the hall cannot request it.
            beats.push_back(hinge_job);
            continue;
        }
        if (visit == 5)
        {
            beats.push_back(workshop);
            continue;
        }
        if (visit == 6)
        {
            beats.push_back(hall);
            continue;
        }
        assistance.x = visit % 2 == 0 ? 3 : 5;
        assistance.y = visit % 2 == 0 ? 9 : 17;
        auto aid = assistance;
        aid.rest_first = visit % 4 != 2;
        if (visit == 7)
        {
            // Returning the hinges completes the fifth distinct aid task.
            // Walk between halls so the city's movement event can arrange
            // the audience; another Look on the same square cannot do so.
            aid.x = 3;
            aid.y = 9;
            aid.repeat_until_at_least = std::pair<std::uint16_t, std::uint16_t>{0x123, 1};
            aid.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x123, 1};
        }
        else if (visit % 4 == 1)
        {
            // Carry the actual spear bundles to the eastern defenders.
            // Repeatedly returning them to another safe hall does not
            // fulfill the mission or unlock Lyzian's audience.
            aid.x = 17;
            aid.y = 19;
            aid.answers = {"LEAVE"};
        }
        beats.push_back(aid);
    }
    if (progress == DarkQueenProgress::audience)
    {
        return beats;
    }
    RouteBeat rescue{14, 5, 5, D::east, {"CARRY HIM", "YES"}};
    rescue.searching = true;
    rescue.cure_first = true;
    rescue.cure_to_full = true;
    rescue.reloads_until_won = 120;
    rescue.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x123, 2};
    beats.push_back(rescue);
    hall.answers = {"YES"};
    hall.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x123, 3};
    beats.push_back(hall);
    if (progress == DarkQueenProgress::captain)
    {
        return beats;
    }
    auto prepare_celanost = hall;
    prepare_celanost.expect_cell.reset();
    prepare_celanost.rest_first = true;
    prepare_celanost.rest_for_spells = true;
    prepare_celanost.memorize_first_of = {"resist_fire", "heal",         "cure_light_wounds",
                                          "ice_storm",   "dispel_magic", "magic_missile"};
    beats.push_back(prepare_celanost);
    RouteBeat approach_well{14, 8, 28, D::north, {"YES"}};
    approach_well.dialogue_answers = {{"CLIMB THE WALL", "NO"}, {"COMMAND THE CRYLOKON", "NO"}};
    approach_well.searching = false;
    approach_well.cure_first = true;
    approach_well.cure_to_full = true;
    approach_well.reloads_until_won = 120;
    approach_well.arrive_only = true;
    // The anemones paralyse whoever strikes them: the party fights the
    // way's ambushes on Quick rather than closing on them.
    approach_well.fight_tactic = 0;
    beats.push_back(approach_well);
    // Climb after reaching the well. Remain in the air chamber, and use
    // the Crylokon there before attempting to breathe its air.
    RouteBeat well{14, 8, 28, D::north, {"YES"}};
    well.dialogue_answers = {
        {"CLIMB THE WALL", "YES"}, {"DIVE IN?", "NO"}, {"COMMAND THE CRYLOKON", "YES"}};
    beats.push_back(well);
    if (progress == DarkQueenProgress::air_chamber)
    {
        return beats;
    }
    RouteBeat air_rest{14, 14, 37, D::north, {"YES"}, true};
    air_rest.arrive_only = true;
    air_rest.rest_for_spells = true;
    air_rest.cure_first = true;
    air_rest.cure_to_full = true;
    air_rest.reloads_until_won = 120;
    air_rest.dialogue_answers = {{"DIVE IN?", "NO"}, {"COMMAND THE CRYLOKON", "NO"}};
    air_rest.memorize_first_of = {"resist_fire", "heal", "ice_storm", "magic_missile",
                                  "cure_light_wounds"};
    air_rest.ward_each_with = "resist_fire";
    beats.push_back(air_rest);
    RouteBeat guardians{14, 5, 37, D::north, {"YES"}};
    guardians.cure_first = true;
    guardians.cure_to_full = true;
    guardians.reloads_until_won = 120;
    guardians.dialogue_answers = air_rest.dialogue_answers;
    beats.push_back(guardians);
    if (progress == DarkQueenProgress::air_guardians)
    {
        return beats;
    }
    air_rest.x = 5;
    beats.push_back(air_rest);
    auto mage = guardians;
    mage.x = 0;
    mage.y = 37;
    beats.push_back(mage);
    auto treasure = guardians;
    treasure.x = 0;
    treasure.y = 35;
    treasure.searching = true;
    beats.push_back(treasure);
    // Recover from the mage before crossing the last guarded hall and
    // attempting the icy dives. This cleared room permits actual rest.
    air_rest.x = 0;
    air_rest.y = 35;
    beats.push_back(air_rest);
    auto eggs = guardians;
    eggs.x = 3;
    eggs.y = 29;
    eggs.searching = false;
    eggs.reload_event = true;
    eggs.answers.clear();
    for (unsigned dive = 0; dive < 12; ++dive)
    {
        eggs.answers.push_back("YES");
        eggs.answers.push_back(
            std::array{"knight 1", "knight 2", "knight 3", "cleric 4", "cleric 5", "magic_user 6"}
                .at(dive % 6));
    }
    eggs.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0xA5, 0};
    beats.push_back(eggs);
    if (progress == DarkQueenProgress::eggs)
    {
        return beats;
    }
    auto return_well = guardians;
    return_well.x = 14;
    return_well.y = 37;
    return_well.arrive_only = true;
    beats.push_back(return_well);
    auto dive = return_well;
    dive.arrive_only = false;
    dive.dialogue_answers = {
        {"DIVE IN?", "YES"}, {"COMMAND THE CRYLOKON", "YES"}, {"CLIMB THE WALL", "NO"}};
    beats.push_back(dive);
    RouteBeat breathe{14, 8, 29, D::north, {"YES"}};
    breathe.dialogue_answers = {{"CLIMB THE WALL", "NO"}, {"COMMAND THE CRYLOKON", "YES"}};
    breathe.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x6F, 0};
    beats.push_back(breathe);
    RouteBeat dragons{14, 18, 36, D::north, {"GIVE", "BACK OFF", "YES"}};
    dragons.dialogue_answers = {{"CLIMB THE WALL", "NO"}, {"COMMAND THE CRYLOKON", "NO"}};
    dragons.arrive_only = true;
    dragons.cure_first = true;
    dragons.cure_to_full = true;
    dragons.reloads_until_won = 120;
    dragons.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x126, 1};
    beats.push_back(dragons);
    RouteBeat report{14, 9, 26, D::north, {"YES"}};
    report.cure_first = true;
    report.cure_to_full = true;
    report.reloads_until_won = 120;
    beats.push_back(report);
    if (progress == DarkQueenProgress::eggs_returned)
    {
        return beats;
    }
    RouteBeat talhook{14, 5, 5, D::east, {"YES"}};
    talhook.cure_first = true;
    talhook.cure_to_full = true;
    talhook.reload_event = true;
    talhook.reloads_until_won = 120;
    talhook.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x123, 255};
    beats.push_back(talhook);
    auto royal_rest = air_rest;
    royal_rest.x = 5;
    royal_rest.y = 5;
    royal_rest.ward_each_with.clear();
    royal_rest.dialogue_answers.clear();
    beats.push_back(royal_rest);
    RouteBeat ceremony{14, 2, 5, D::east, {"YES"}};
    ceremony.cure_first = true;
    ceremony.cure_to_full = true;
    ceremony.reloads_until_won = 120;
    beats.push_back(ceremony);
    if (progress == DarkQueenProgress::taladas)
    {
        return beats;
    }
    RouteBeat ezra{18, 8, 6, D::east, {"LYZIAN", "YES"}};
    ezra.step = true;
    ezra.searching = false;
    ezra.cure_first = true;
    ezra.cure_to_full = true;
    ezra.reloads_until_won = 120;
    ezra.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x12D, 1};
    beats.push_back(ezra);
    if (progress == DarkQueenProgress::ezra)
    {
        return beats;
    }
    RouteBeat militia{
        18,
        6,
        10,
        D::north,
        {"knight 1", "knight 2", "knight 3", "cleric 4", "cleric 5", "magic_user 6", "Leave"}};
    militia.step = true;
    militia.cure_first = true;
    militia.cure_to_full = true;
    militia.reload_event = true;
    militia.reloads_until_won = 120;
    beats.push_back(militia);
    RouteBeat leave_hizden{18, 7, 15, D::south, {"YES"}};
    leave_hizden.step = true;
    leave_hizden.searching = false;
    leave_hizden.cure_first = true;
    leave_hizden.cure_to_full = true;
    leave_hizden.reload_event = true;
    leave_hizden.reloads_until_won = 120;
    beats.push_back(leave_hizden);
    if (progress == DarkQueenProgress::hizden)
    {
        return beats;
    }
    using C = opengold::por::ExplorationCommand;
    // The original entry consumes its departure lock before the player
    // moves. Walk north, then approach the lighthouse from the southwest.
    RouteBeat outside_hizden{2, 3, 4, D::north, {"NO"}};
    outside_hizden.area_steps = {C::north};
    outside_hizden.searching = false;
    beats.push_back(outside_hizden);
    RouteBeat lighthouse{2, 4, 1, D::north, {"YES"}};
    lighthouse.area_steps = {C::north, C::northeast, C::look};
    lighthouse.searching = false;
    // The lighthouse's pit drops the party into a swarm: it arrives with
    // its spells prayed for.
    lighthouse.rest_first = true;
    lighthouse.rest_for_spells = true;
    lighthouse.reloads_until_won = 120;
    beats.push_back(lighthouse);
    if (progress == DarkQueenProgress::luminari)
    {
        return beats;
    }
    RouteBeat pit{22, 5, 11, D::south, {}};
    pit.step = true;
    pit.searching = false;
    pit.reload_event = true;
    pit.reloads_until_won = 120;
    beats.push_back(pit);
    RouteBeat mound{22, 9, 7, D::north, {"CLIMB TO THE TOP.", "CLIMB OUT"}};
    mound.cure_first = true;
    mound.cure_to_full = true;
    mound.searching = false;
    mound.reloads_until_won = 120;
    beats.push_back(mound);
    if (progress == DarkQueenProgress::luminari_deep)
    {
        return beats;
    }
    // The native lighthouse eventually shakes and its aging teleporters
    // stop working. Wait with ordinary Looks in a cleared passage, instead
    // of walking into the same active teleporter repeatedly.
    RouteBeat lighthouse_wait{22, 6, 4, D::west, {}};
    lighthouse_wait.repeat_in_place = true;
    lighthouse_wait.searching = false;
    lighthouse_wait.repeat_until_at_least = std::pair<std::uint16_t, std::uint16_t>{0x13E, 5};
    lighthouse_wait.reloads_until_won = 120;
    // Hurt from the pit, the party meets the hydras and the swarms on Quick.
    lighthouse_wait.fight_tactic = 0;
    beats.push_back(lighthouse_wait);
    RouteBeat lighthouse_rest{22, 6, 4, D::west, {}, true};
    lighthouse_rest.rest_for_spells = true;
    lighthouse_rest.cure_first = true;
    lighthouse_rest.cure_to_full = true;
    lighthouse_rest.arrive_only = true;
    lighthouse_rest.searching = false;
    lighthouse_rest.reloads_until_won = 120;
    beats.push_back(lighthouse_rest);
    RouteBeat porch_arch{22, 2, 6, D::west, {"STEP THROUGH THE ARCH", "LEAVE"}};
    porch_arch.step = true;
    porch_arch.searching = false;
    porch_arch.reload_event = true;
    porch_arch.reloads_until_won = 120;
    beats.push_back(porch_arch);
    RouteBeat wizard{
        22, 21, 24, D::east, {"CLEAR YOUR THROAT.", "SCREAM AND SHOUT.", "EXPLAIN YOUR MISSION."}};
    wizard.cure_first = true;
    wizard.cure_to_full = true;
    wizard.searching = false;
    wizard.reload_event = true;
    wizard.reloads_until_won = 120;
    wizard.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x70, 1};
    beats.push_back(wizard);
    if (progress == DarkQueenProgress::fastillion)
    {
        return beats;
    }
    RouteBeat leave_wizard{22, 21, 21, D::east, {"STEP THROUGH THE ARCH", "LEAVE"}};
    leave_wizard.step = true;
    leave_wizard.searching = false;
    leave_wizard.reloads_until_won = 120;
    beats.push_back(leave_wizard);
    RouteBeat down_arch{22, 4, 1, D::north, {"STEP THROUGH THE ARCH"}};
    down_arch.step = true;
    down_arch.searching = false;
    down_arch.reloads_until_won = 120;
    beats.push_back(down_arch);
    RouteBeat leave_luminari{22, 1, 16, D::west, {}};
    leave_luminari.step = true;
    leave_luminari.searching = false;
    leave_luminari.reloads_until_won = 120;
    beats.push_back(leave_luminari);
    RouteBeat outside_luminari{2, 4, 1, D::south, {"NO"}};
    outside_luminari.area_steps = {C::south};
    outside_luminari.searching = false;
    beats.push_back(outside_luminari);
    RouteBeat strait{2, 5, 4, D::south, {"YES"}};
    strait.area_steps = {C::south, C::southeast, C::look};
    strait.searching = false;
    strait.reloads_until_won = 120;
    beats.push_back(strait);
    if (progress == DarkQueenProgress::kristophan)
    {
        return beats;
    }
    RouteBeat city_gate{38, 37, 16, D::west, {"CITY"}};
    city_gate.step = true;
    city_gate.searching = false;
    beats.push_back(city_gate);
    RouteBeat guild{38,
                    12,
                    4,
                    D::east,
                    {"YES", "knight 1", "knight 2", "knight 3", "cleric 4", "cleric 5",
                     "magic_user 6", "Leave"}};
    guild.step = true;
    guild.dialogue_answers = {{"Choose a spell for magic_user 6", "Delayed Blast Fireball"}};
    guild.searching = false;
    guild.cure_first = true;
    guild.cure_to_full = true;
    guild.reload_event = true;
    guild.reloads_until_won = 120;
    beats.push_back(guild);
    RouteBeat selias{38, 29, 16, D::south, {"TALK", "YES", "TWENTY"}};
    selias.step = true;
    selias.searching = false;
    selias.cure_first = true;
    selias.cure_to_full = true;
    selias.reload_event = true;
    selias.reloads_until_won = 120;
    beats.push_back(selias);
    RouteBeat old_temple{38, 25, 2, D::north, {"YES"}};
    old_temple.step = true;
    old_temple.searching = false;
    old_temple.cure_first = true;
    old_temple.cure_to_full = true;
    old_temple.reload_event = true;
    old_temple.reloads_until_won = 120;
    beats.push_back(old_temple);
    if (progress == DarkQueenProgress::tombs)
    {
        return beats;
    }
    RouteBeat quiet_portal{42, 10, 13, D::north, {"NO"}};
    quiet_portal.searching = false;
    quiet_portal.cure_first = true;
    quiet_portal.cure_to_full = true;
    quiet_portal.reload_event = true;
    quiet_portal.reloads_until_won = 120;
    beats.push_back(quiet_portal);
    RouteBeat tomb_rest{42, 10, 13, D::north, {"NO"}, true};
    tomb_rest.rest_for_spells = true;
    tomb_rest.cure_first = true;
    tomb_rest.cure_to_full = true;
    tomb_rest.arrive_only = true;
    tomb_rest.searching = false;
    tomb_rest.keep_preparation = true;
    tomb_rest.memorize_first_of = {
        "heal",         "cure_light_wounds", "enlarge",      "enlarge",
        "enlarge",      "dispel_magic",      "dispel_magic", "protection_from_evil_10_radius",
        "prayer",       "fireball",          "fireball",     "fireball",
        "magic_missile"};
    tomb_rest.reloads_until_won = 120;
    beats.push_back(tomb_rest);
    RouteBeat upper_tombs{42, 6, 15, D::south, {"GOLD KEY", "knight 1", "YES", "NO"}};
    upper_tombs.step = true;
    upper_tombs.searching = false;
    upper_tombs.cure_first = true;
    upper_tombs.cure_to_full = true;
    upper_tombs.reload_event = true;
    upper_tombs.reloads_until_won = 120;
    beats.push_back(upper_tombs);
    RouteBeat coffin_approach{42, 14, 11, D::north, {"NO"}};
    coffin_approach.arrive_only = true;
    coffin_approach.searching = false;
    coffin_approach.dialogue_answers = {{"DO YOU OPEN THE COFFIN?", "NO"}};
    beats.push_back(coffin_approach);
    for (const char *spell : {"protection_from_evil_10_radius", "enlarge"})
    {
        RouteBeat ward{42, 14, 11, D::north, {}};
        ward.waiting = true;
        ward.keep_preparation = true;
        if (std::string_view(spell) == "enlarge")
        {
            ward.ward_each_with = spell;
            ward.ward_members = {"knight 1", "knight 2", "knight 3"};
        }
        else
        {
            ward.cast_after = {spell};
        }
        beats.push_back(ward);
    }
    RouteBeat king_tomb{42, 13, 3, D::south, {"NO"}};
    king_tomb.arrive_only = true;
    king_tomb.searching = false;
    king_tomb.cure_first = true;
    king_tomb.cure_to_full = true;
    king_tomb.reload_event = true;
    king_tomb.reloads_until_won = 120;
    king_tomb.dialogue_answers = {{"RANSACK THE COFFINS", "NO"},
                                  {"DO YOU OPEN THE COFFIN?", "YES"}};
    beats.push_back(king_tomb);
    RouteBeat king_search = king_tomb;
    king_search.arrive_only = false;
    king_search.searching = true;
    king_search.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x105, 1};
    beats.push_back(king_search);
    RouteBeat cleared_coffin{42, 14, 10, D::north, {"NO"}};
    cleared_coffin.searching = false;
    cleared_coffin.dialogue_answers = king_tomb.dialogue_answers;
    beats.push_back(cleared_coffin);
    RouteBeat upper_rest = tomb_rest;
    upper_rest.x = 14;
    upper_rest.y = 10;
    upper_rest.keep_preparation = true;
    upper_rest.memorize_first_of = tomb_rest.memorize_first_of;
    beats.push_back(upper_rest);
    RouteBeat vampire_approach{42, 18, 8, D::south, {"NO"}};
    vampire_approach.arrive_only = true;
    vampire_approach.searching = false;
    vampire_approach.cure_first = true;
    vampire_approach.cure_to_full = true;
    vampire_approach.reloads_until_won = 120;
    beats.push_back(vampire_approach);
    for (const char *spell : {"protection_from_evil_10_radius", "prayer", "enlarge"})
    {
        RouteBeat ward{42, 18, 8, D::south, {}};
        ward.waiting = true;
        ward.keep_preparation = true;
        if (std::string_view(spell) == "enlarge")
        {
            ward.ward_each_with = spell;
            ward.ward_members = {"knight 1", "knight 2", "knight 3"};
        }
        else
        {
            ward.cast_after = {spell};
        }
        beats.push_back(ward);
    }
    RouteBeat vampires{42, 18, 9, D::south, {"NO"}};
    vampires.searching = false;
    vampires.cure_first = true;
    vampires.cure_to_full = true;
    vampires.reloads_until_won = 2400;
    vampires.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x105, 2};
    // The vampires' court ambush is fought on Quick.
    vampires.fight_tactic = 0;
    beats.push_back(vampires);
    RouteBeat leave_upper{42, 15, 10, D::south, {"YES", "NO"}};
    leave_upper.arrive_only = true;
    leave_upper.searching = false;
    leave_upper.cure_first = true;
    leave_upper.cure_to_full = true;
    leave_upper.reload_event = true;
    leave_upper.reloads_until_won = 120;
    leave_upper.dialogue_answers = king_tomb.dialogue_answers;
    beats.push_back(leave_upper);
    RouteBeat leave_tombs{42, 27, 27, D::south, {"YES"}};
    leave_tombs.step = true;
    leave_tombs.searching = false;
    beats.push_back(leave_tombs);
    RouteBeat recovery_temple{
        38, 27, 13, D::west, {"YES", "raise:dead", "restore:drained", "cure:all", "Leave", "NO"}};
    recovery_temple.searching = false;
    recovery_temple.step = true;
    beats.push_back(recovery_temple);
    RouteBeat old_temple_hall{38, 25, 1, D::west, {"NO"}};
    old_temple_hall.arrive_only = true;
    old_temple_hall.searching = false;
    beats.push_back(old_temple_hall);
    RouteBeat reenter_tombs{38, 25, 0, D::east, {"YES"}};
    reenter_tombs.step = true;
    reenter_tombs.searching = false;
    beats.push_back(reenter_tombs);
    beats.push_back(quiet_portal);
    beats.push_back(tomb_rest);
    RouteBeat circus_portal{42, 13, 17, D::south, {"YES", "NO"}};
    circus_portal.arrive_only = true;
    circus_portal.searching = false;
    circus_portal.cure_first = true;
    circus_portal.cure_to_full = true;
    circus_portal.reload_event = true;
    circus_portal.reloads_until_won = 120;
    beats.push_back(circus_portal);
    RouteBeat web{42, 23, 7, D::north, {"PAY", "YES"}};
    web.searching = true;
    web.cure_first = true;
    web.cure_to_full = true;
    web.reload_event = true;
    web.reloads_until_won = 120;
    web.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x105, 3};
    beats.push_back(web);
    RouteBeat circlet{42, 24, 1, D::north, {"YES", "knight 1"}};
    circlet.searching = true;
    circlet.cure_first = true;
    circlet.cure_to_full = true;
    circlet.reload_event = true;
    circlet.reloads_until_won = 120;
    circlet.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x7A, 1};
    beats.push_back(circlet);
    RouteBeat circus_exit{42, 26, 1, D::west, {"YES", "NO"}};
    circus_exit.step = true;
    circus_exit.searching = false;
    circus_exit.cure_first = true;
    circus_exit.cure_to_full = true;
    circus_exit.reload_event = true;
    circus_exit.reloads_until_won = 120;
    beats.push_back(circus_exit);
    RouteBeat trapdoor{42, 8, 13, D::south, {"NO"}};
    trapdoor.searching = true;
    trapdoor.repeat_until_bits = std::pair<std::uint16_t, std::uint16_t>{0x16F, 1};
    trapdoor.repeat_in_place = true;
    trapdoor.cure_first = true;
    trapdoor.cure_to_full = true;
    trapdoor.reloads_until_won = 120;
    beats.push_back(trapdoor);
    RouteBeat ladder{42, 8, 13, D::south, {"YES", "NO"}};
    ladder.searching = true;
    beats.push_back(ladder);
    RouteBeat ghost{42, 7, 26, D::north, {"YES", "YES"}};
    ghost.searching = false;
    ghost.cure_first = true;
    ghost.cure_to_full = true;
    ghost.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x105, 4};
    beats.push_back(ghost);
    RouteBeat climb{42, 9, 23, D::north, {"YES", "NO"}};
    climb.arrive_only = true;
    climb.searching = false;
    beats.push_back(climb);
    RouteBeat heart_portal{42, 15, 12, D::north, {"YES", "NO"}};
    heart_portal.arrive_only = true;
    heart_portal.searching = false;
    heart_portal.cure_first = true;
    heart_portal.cure_to_full = true;
    heart_portal.reloads_until_won = 120;
    beats.push_back(heart_portal);
    RouteBeat heart_key{42, 6, 22, D::north, {"YES", "knight 1", "YES"}};
    heart_key.searching = false;
    heart_key.cure_first = true;
    heart_key.cure_to_full = true;
    heart_key.reloads_until_won = 120;
    heart_key.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x7B, 1};
    beats.push_back(heart_key);
    RouteBeat heart_return{42, 9, 17, D::north, {"YES", "NO"}};
    heart_return.arrive_only = true;
    heart_return.searching = false;
    heart_return.reloads_until_won = 120;
    beats.push_back(heart_return);
    RouteBeat guardian_portal{42, 10, 13, D::north, {"YES", "NO"}};
    guardian_portal.arrive_only = true;
    guardian_portal.searching = false;
    beats.push_back(guardian_portal);
    RouteBeat guardian{42, 3, 17, D::north, {"TALK", "BEFRIEND HIM.", "YES", "YES"}};
    guardian.searching = false;
    guardian.cure_first = true;
    guardian.cure_to_full = true;
    guardian.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x105, 5};
    beats.push_back(guardian);
    if (progress == DarkQueenProgress::oracle)
    {
        return beats;
    }
    RouteBeat guardian_return{42, 1, 13, D::north, {"YES", "NO"}};
    guardian_return.arrive_only = true;
    guardian_return.searching = false;
    beats.push_back(guardian_return);
    RouteBeat oracle_departure = leave_tombs;
    oracle_departure.answers = {"YES", "NO"};
    oracle_departure.cure_first = true;
    oracle_departure.cure_to_full = true;
    oracle_departure.reloads_until_won = 120;
    beats.push_back(oracle_departure);
    beats.push_back(leave_tombs);
    RouteBeat trained_again = guild;
    trained_again.dialogue_answers = {{"Choose a spell for magic_user 6", "Mind Blank"}};
    beats.push_back(trained_again);
    RouteBeat city_exit{38, 20, 19, D::south, {"YES"}};
    city_exit.step = true;
    city_exit.searching = false;
    city_exit.cure_first = true;
    city_exit.cure_to_full = true;
    beats.push_back(city_exit);
    RouteBeat outside_city{38, 37, 16, D::east, {"LEAVE"}};
    outside_city.step = true;
    outside_city.searching = false;
    beats.push_back(outside_city);
    RouteBeat hulder_cleft{2, 17, 7, D::east, {"NO", "YES"}};
    hulder_cleft.area_steps = {C::northeast, C::east,      C::east,      C::east,
                               C::east,      C::east,      C::southeast, C::east,
                               C::east,      C::southeast, C::southeast, C::southeast};
    hulder_cleft.searching = false;
    hulder_cleft.cure_first = true;
    hulder_cleft.cure_to_full = true;
    hulder_cleft.reloads_until_won = 120;
    beats.push_back(hulder_cleft);
    // The forest's hunger must first overcome the party. Waiting with
    // ordinary Look commands lets its own countdown and encounter run.
    RouteBeat forest_hunger{26, 7, 5, D::south, {}};
    forest_hunger.repeat_in_place = true;
    forest_hunger.repeat_until_at_least = std::pair<std::uint16_t, std::uint16_t>{0x9C, 0x10};
    forest_hunger.area_steps = {C::turn_right, C::look};
    forest_hunger.dialogue_answers = {{"TRY TO BRING IT DOWN?", "NO"}};
    forest_hunger.searching = false;
    beats.push_back(forest_hunger);
    RouteBeat hulder_words{26, 7, 5, D::south, {}};
    hulder_words.arrive_only = true;
    hulder_words.rest_first = true;
    hulder_words.rest_for_spells = true;
    hulder_words.dialogue_answers = {{"HEARS A TINY RUSTLE AMONG THE LEAVES", "GUARD"},
                                     {"DO YOU LISTEN?", "YES"},
                                     {"NOW, TELL ME A WORD YOU HAVE LEARNED", "LIMENE"}};
    beats.push_back(hulder_words);
    for (unsigned lesson = 1; lesson < 4; ++lesson)
    {
        auto hunger_again = forest_hunger;
        hunger_again.repeat_until_at_least =
            std::pair<std::uint16_t, std::uint16_t>{0xB7, static_cast<std::uint16_t>(lesson + 1)};
        beats.push_back(hunger_again);
        beats.push_back(hulder_words);
    }
    RouteBeat banquet{26, 7, 11, D::south, {"type:GEBENE", "MOVE"}};
    banquet.searching = false;
    banquet.cure_first = true;
    banquet.cure_to_full = true;
    beats.push_back(banquet);
    RouteBeat royal_audience{26, 7, 14, D::south, {"YES", "NO", "YES"}};
    royal_audience.searching = false;
    royal_audience.dialogue_answers = {{"STAND AND SAY PEACE", "GEBENE"},
                                       {"DOUR AND INHOSPITABLE", "SHUWARA"},
                                       {"WILL YOU RAISE ARMS", "LIMENE"},
                                       {"IS THAT THE FATE YOU WISH", "BILBARA"},
                                       {"SWEAR AN OATH", "LIMENE"},
                                       {"WILL YOU TELL THE NEWS", "LIMENE"},
                                       {"NEW AURIM. YOU HAVE BEEN THERE?", "NO"},
                                       {"BLACKWATER GLADE. YOU HAVE BEEN THERE?", "NO"},
                                       {"THE ELVES OF ARMACH. YOU HAVE VISITED THEM?", "NO"},
                                       {"WILL YOU ACCEPT OUR HOSPITALITY?", "LIMENE"}};
    beats.push_back(royal_audience);
    RouteBeat mother_of_trees{26, 13, 12, D::north, {"YES"}};
    mother_of_trees.searching = false;
    beats.push_back(mother_of_trees);
    // Remember and report the number the actual counting scene displays.
    RouteBeat first_report{26, 7, 12, D::north, {"type:GEBENE", "YES", "type:counted-leaves"}};
    first_report.arrive_only = true;
    first_report.searching = false;
    beats.push_back(first_report);
    RouteBeat pluck{26, 13, 12, D::north, {"NO", "YES"}};
    pluck.arrive_only = true;
    pluck.searching = false;
    beats.push_back(pluck);
    RouteBeat last_report{
        26, 7, 12, D::north, {"type:GEBENE", "YES", "type:1", "knight 1", "knight 1"}};
    last_report.arrive_only = true;
    last_report.searching = false;
    last_report.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x73, 1};
    beats.push_back(last_report);
    RouteBeat farewell_feast{26, 7, 14, D::south, {"YES"}};
    farewell_feast.arrive_only = true;
    farewell_feast.searching = false;
    beats.push_back(farewell_feast);
    RouteBeat hulder_rest{26, 5, 18, D::north, {}};
    hulder_rest.rest_first = true;
    hulder_rest.rest_for_spells = true;
    hulder_rest.arrive_only = true;
    hulder_rest.searching = false;
    beats.push_back(hulder_rest);
    RouteBeat hulder_exit{26, 6, 2, D::north, {"YES"}};
    hulder_exit.arrive_only = true;
    hulder_exit.searching = false;
    beats.push_back(hulder_exit);
    RouteBeat new_aurim{2, 24, 22, D::south, {"YES"}};
    new_aurim.area_steps = {C::southwest, C::southeast, C::southeast, C::southeast, C::southeast,
                            C::southeast, C::south,     C::southeast, C::southeast, C::southeast,
                            C::southeast, C::southeast, C::south,     C::southwest, C::southwest};
    new_aurim.searching = false;
    new_aurim.cure_first = true;
    new_aurim.cure_to_full = true;
    new_aurim.reloads_until_won = 240;
    beats.push_back(new_aurim);
    RouteBeat thenol_guards{30, 20, 23, D::north, {"TALK", "YES"}};
    thenol_guards.searching = false;
    thenol_guards.reloads_until_won = 240;
    beats.push_back(thenol_guards);
    RouteBeat quartermaster{30, 24, 23, D::north, {"YES", "YES", "Leave"}};
    quartermaster.searching = false;
    quartermaster.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x14B, 1};
    beats.push_back(quartermaster);
    RouteBeat quiet_kender{30, 4, 16, D::north, {"TALK", "LEAVE"}};
    quiet_kender.searching = false;
    quiet_kender.cure_first = true;
    quiet_kender.cure_to_full = true;
    quiet_kender.reloads_until_won = 240;
    beats.push_back(quiet_kender);
    // This imported party has no thief, and its mage's robe does not
    // teach Knock. Take the original army-orders path to a palace pass.
    RouteBeat hith_orders{30, 21, 17, D::north, {"ORDERS"}};
    hith_orders.step = true;
    hith_orders.searching = false;
    hith_orders.cure_first = true;
    hith_orders.cure_to_full = true;
    hith_orders.reloads_until_won = 240;
    beats.push_back(hith_orders);
    RouteBeat lieutenant{30, 24, 5, D::north, {"TALK", "HITH TEMPLE"}};
    lieutenant.step = true;
    lieutenant.searching = false;
    lieutenant.cure_first = true;
    lieutenant.cure_to_full = true;
    lieutenant.reloads_until_won = 240;
    beats.push_back(lieutenant);
    RouteBeat commander{30, 5, 21, D::north, {"TALK"}};
    commander.step = true;
    commander.searching = false;
    commander.cure_first = true;
    commander.cure_to_full = true;
    commander.reloads_until_won = 240;
    commander.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x7C, 1};
    beats.push_back(commander);
    RouteBeat palace{30, 20, 19, D::west, {"KNOCK", "ENTER", "YES"}};
    palace.step = true;
    palace.searching = false;
    palace.cure_first = true;
    palace.cure_to_full = true;
    palace.reloads_until_won = 240;
    beats.push_back(palace);
    RouteBeat dungeon_stair{34, 5, 8, D::west, {"YES"}};
    dungeon_stair.step = true;
    dungeon_stair.searching = false;
    dungeon_stair.cure_first = true;
    dungeon_stair.cure_to_full = true;
    dungeon_stair.reloads_until_won = 480;
    dungeon_stair.dialogue_answers = {
        {"SHOW SOMETHING TO IT?", "YES"}, {"WHAT?", "CLAY TABLET"}, {"WHERE YOU FROM?", "PALACE"}};
    beats.push_back(dungeon_stair);
    RouteBeat aurak_assignment{34, 15, 25, D::west, {"YES"}};
    aurak_assignment.arrive_only = true;
    aurak_assignment.searching = false;
    aurak_assignment.cure_first = true;
    aurak_assignment.cure_to_full = true;
    aurak_assignment.reloads_until_won = 480;
    aurak_assignment.dialogue_answers = dungeon_stair.dialogue_answers;
    aurak_assignment.dialogue_answers.emplace_back("", "WAIT");
    beats.push_back(aurak_assignment);
    RouteBeat return_stair = dungeon_stair;
    return_stair.x = 27;
    return_stair.y = 25;
    return_stair.facing = D::east;
    beats.push_back(return_stair);
    RouteBeat second_ring = dungeon_stair;
    second_ring.x = 0;
    second_ring.y = 1;
    second_ring.facing = D::north;
    beats.push_back(second_ring);
    RouteBeat inner_palace = dungeon_stair;
    inner_palace.x = 20;
    inner_palace.y = 6;
    inner_palace.facing = D::south;
    inner_palace.dialogue_answers.insert(inner_palace.dialogue_answers.end(),
                                         {{"WHO COMES FORWARD?", "knight 1"},
                                          {"DO YOU HATE DRACONIANS", "YES"},
                                          {"YOU SEEN HIM YET?", "NO"},
                                          {"YOU TALKED TO HER YET?", "NO"},
                                          {"CAN I JOIN YOU?", "YES"}});
    beats.push_back(inner_palace);
    RouteBeat queen{34, 26, 22, D::north, {"YES"}};
    queen.arrive_only = true;
    queen.searching = false;
    queen.cure_first = true;
    queen.cure_to_full = true;
    queen.reloads_until_won = 480;
    queen.dialogue_answers = inner_palace.dialogue_answers;
    queen.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x87, 1};
    beats.push_back(queen);
    RouteBeat palace_preparation{34, 26, 22, D::north, {}};
    palace_preparation.arrive_only = true;
    palace_preparation.searching = false;
    palace_preparation.rest_first = true;
    palace_preparation.rest_for_spells = true;
    palace_preparation.cure_first = true;
    palace_preparation.cure_to_full = true;
    palace_preparation.keep_preparation = true;
    palace_preparation.ward_memorized = "protection_from_evil_10_radius";
    palace_preparation.memorize_first_of = {"enlarge",
                                            "enlarge",
                                            "enlarge",
                                            "enlarge",
                                            "delayed_blast_fireball",
                                            "delayed_blast_fireball",
                                            "mind_blank",
                                            "globe_of_invulnerability",
                                            "globe_of_invulnerability",
                                            "globe_of_invulnerability",
                                            "fire_shield",
                                            "fire_shield",
                                            "fire_shield",
                                            "dispel_magic",
                                            "dispel_magic",
                                            "fireball",
                                            "fireball",
                                            "fireball",
                                            "prayer",
                                            "heal",
                                            "heal",
                                            "heal",
                                            "heal",
                                            "raise_dead",
                                            "raise_dead",
                                            "cure_light_wounds",
                                            "cure_light_wounds",
                                            "cure_light_wounds",
                                            "cure_light_wounds",
                                            "magic_missile"};
    beats.push_back(palace_preparation);
    RouteBeat palace_ward{34, 26, 22, D::north, {}};
    palace_ward.waiting = true;
    palace_ward.ward_each_with = "protection_from_evil_10_radius";
    beats.push_back(palace_ward);
    RouteBeat blank_mind = palace_ward;
    blank_mind.ward_each_with = "mind_blank";
    blank_mind.ward_members = {"magic_user 6"};
    beats.push_back(blank_mind);
    RouteBeat leave_inner = dungeon_stair;
    leave_inner.x = 27;
    leave_inner.y = 23;
    leave_inner.facing = D::south;
    beats.push_back(leave_inner);
    RouteBeat first_ring = dungeon_stair;
    first_ring.x = 1;
    first_ring.y = 24;
    first_ring.facing = D::north;
    beats.push_back(first_ring);
    RouteBeat passage_stair = dungeon_stair;
    passage_stair.x = 0;
    passage_stair.y = 22;
    passage_stair.facing = D::east;
    beats.push_back(passage_stair);
    passage_stair.y = 18;
    beats.push_back(passage_stair);
    RouteBeat ceremony_door{34, 6, 21, D::east, {}};
    ceremony_door.arrive_only = true;
    ceremony_door.searching = false;
    ceremony_door.cure_first = true;
    ceremony_door.cure_to_full = true;
    ceremony_door.reloads_until_won = 480;
    ceremony_door.dialogue_answers = dungeon_stair.dialogue_answers;
    beats.push_back(ceremony_door);
    RouteBeat final_enlarge{34, 6, 21, D::east, {}};
    final_enlarge.waiting = true;
    final_enlarge.ward_each_with = "enlarge";
    final_enlarge.ward_members = {"knight 1", "knight 2", "knight 3", "Grunschka"};
    final_enlarge.cast_after = {"prayer"};
    beats.push_back(final_enlarge);
    RouteBeat corruption{34, 7, 21, D::east, {"ATTACK"}};
    corruption.arrive_only = true;
    corruption.searching = false;
    corruption.cure_first = true;
    corruption.cure_to_full = true;
    corruption.reload_event = true;
    corruption.reloads_until_won = 480;
    corruption.dialogue_answers = {{"WHAT DO YOU DO?", "ATTACK"}};
    beats.push_back(corruption);
    RouteBeat stop_ritual{34, 9, 21, D::east, {}};
    // Its guard's Dark Wizard and Thenol priests are struck first.
    stop_ritual.fight_tactic = 3;
    stop_ritual.arrive_only = true;
    stop_ritual.searching = false;
    stop_ritual.cure_first = true;
    stop_ritual.cure_to_full = true;
    stop_ritual.reload_event = true;
    stop_ritual.reloads_until_won = 480;
    stop_ritual.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x155, 3};
    beats.push_back(stop_ritual);
    RouteBeat back_to_passage = ceremony_door;
    back_to_passage.facing = D::west;
    beats.push_back(back_to_passage);
    passage_stair.x = 1;
    passage_stair.y = 18;
    passage_stair.facing = D::west;
    beats.push_back(passage_stair);
    passage_stair.y = 22;
    beats.push_back(passage_stair);
    passage_stair.y = 23;
    passage_stair.facing = D::south;
    beats.push_back(passage_stair);
    RouteBeat out_of_fireplace = dungeon_stair;
    out_of_fireplace.x = 27;
    out_of_fireplace.y = 24;
    out_of_fireplace.facing = D::north;
    beats.push_back(out_of_fireplace);
    RouteBeat recover_after_ritual = palace_preparation;
    recover_after_ritual.x = 27;
    recover_after_ritual.y = 23;
    beats.push_back(recover_after_ritual);
    leave_inner.x = 22;
    leave_inner.y = 14;
    leave_inner.facing = D::north;
    beats.push_back(leave_inner);
    first_ring.x = 14;
    first_ring.y = 0;
    first_ring.facing = D::south;
    beats.push_back(first_ring);
    RouteBeat leave_palace = dungeon_stair;
    leave_palace.x = 13;
    leave_palace.y = 13;
    leave_palace.facing = D::east;
    beats.push_back(leave_palace);
    RouteBeat leave_aurim{30, 21, 24, D::south, {"YES", "NO"}};
    leave_aurim.step = true;
    leave_aurim.searching = false;
    leave_aurim.cure_first = true;
    leave_aurim.cure_to_full = true;
    beats.push_back(leave_aurim);
    RouteBeat hawkbluff{2, 3, 9, D::south, {"YES"}};
    hawkbluff.area_steps = {C::southwest, C::west, C::west};
    hawkbluff.searching = false;
    hawkbluff.cure_first = true;
    hawkbluff.cure_to_full = true;
    hawkbluff.reloads_until_won = 120;
    beats.push_back(hawkbluff);
    RouteBeat keyhole_pass{47, 4, 15, D::south, {"YES", "YES", "type:KEYHOLE", "YES"}};
    keyhole_pass.step = true;
    keyhole_pass.searching = false;
    keyhole_pass.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x7D, 1};
    beats.push_back(keyhole_pass);
    RouteBeat discreet_walk{47, 4, 15, D::east, {}};
    discreet_walk.searching = false;
    discreet_walk.area_steps = {C::turn_left};
    for (unsigned circuit = 0; circuit < 15; ++circuit)
    {
        discreet_walk.area_steps.insert(discreet_walk.area_steps.end(),
                                        {C::forward, C::turn_around, C::forward, C::turn_around});
    }
    beats.push_back(discreet_walk);
    RouteBeat lantern_pass = keyhole_pass;
    lantern_pass.answers = {"YES", "type:LANTERN", "YES", "YES", "type:DAVIK"};
    lantern_pass.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x85, 1};
    beats.push_back(lantern_pass);
    beats.push_back(discreet_walk);
    RouteBeat baton_pass = keyhole_pass;
    baton_pass.answers = {"YES", "type:BATON", "YES"};
    baton_pass.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x82, 1};
    beats.push_back(baton_pass);
    RouteBeat fortress_entry{47, 6, 14, D::north, {"YES"}};
    fortress_entry.step = true;
    fortress_entry.searching = false;
    beats.push_back(fortress_entry);
    RouteBeat dungeon_descent{46, 1, 11, D::south, {"YES"}};
    dungeon_descent.step = true;
    dungeon_descent.searching = false;
    dungeon_descent.cure_first = true;
    dungeon_descent.cure_to_full = true;
    beats.push_back(dungeon_descent);
    RouteBeat release_davik{46, 26, 14, D::east, {"X"}};
    release_davik.searching = false;
    release_davik.cure_first = true;
    release_davik.cure_to_full = true;
    release_davik.repeat_until_bits = std::pair<std::uint16_t, std::uint16_t>{0xB5, 0x20};
    release_davik.repeat_in_place = true;
    beats.push_back(release_davik);
    RouteBeat first_level_return = dungeon_descent;
    first_level_return.x = 24;
    first_level_return.y = 13;
    first_level_return.facing = D::north;
    beats.push_back(first_level_return);
    RouteBeat second_level = dungeon_descent;
    second_level.x = 0;
    second_level.y = 11;
    second_level.facing = D::north;
    beats.push_back(second_level);
    RouteBeat third_level = dungeon_descent;
    third_level.x = 1;
    third_level.y = 23;
    third_level.facing = D::south;
    beats.push_back(third_level);
    RouteBeat fourth_level = dungeon_descent;
    fourth_level.x = 14;
    fourth_level.y = 6;
    fourth_level.facing = D::west;
    fourth_level.ward_each_with = "enlarge";
    fourth_level.ward_members = {"knight 1", "knight 2", "knight 3", "Grunschka"};
    fourth_level.cast_after = {"prayer"};
    beats.push_back(fourth_level);
    RouteBeat invasion_plans{46, 14, 10, D::east, {"NO"}};
    invasion_plans.arrive_only = true;
    invasion_plans.searching = false;
    invasion_plans.cure_first = true;
    invasion_plans.cure_to_full = true;
    invasion_plans.reload_event = true;
    invasion_plans.reloads_until_won = 240;
    invasion_plans.dialogue_answers = {{"DO YOU FOLLOW", "NO"}};
    invasion_plans.repeat_until_bits = std::pair<std::uint16_t, std::uint16_t>{0xB7, 4};
    invasion_plans.repeat_in_place = true;
    beats.push_back(invasion_plans);
    RouteBeat return_third = dungeon_descent;
    return_third.x = 13;
    return_third.y = 13;
    return_third.facing = D::east;
    return_third.reloads_until_won = 240;
    beats.push_back(return_third);
    RouteBeat return_second = return_third;
    return_second.x = 14;
    return_second.y = 6;
    return_second.facing = D::north;
    beats.push_back(return_second);
    RouteBeat return_first = return_third;
    return_first.x = 0;
    return_first.y = 23;
    return_first.facing = D::south;
    beats.push_back(return_first);
    RouteBeat book{46, 9, 7, D::north, {"YES", "X"}};
    book.arrive_only = true;
    book.searching = false;
    book.cure_first = true;
    book.cure_to_full = true;
    book.reload_event = true;
    book.reloads_until_won = 240;
    book.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x74, 1};
    book.dialogue_answers = {{"WHERE DO YOU WISH DAVIK", "X"}};
    beats.push_back(book);
    RouteBeat daviks_exit{46, 11, 8, D::east, {"YES", "X"}};
    daviks_exit.step = true;
    daviks_exit.searching = false;
    daviks_exit.cure_first = true;
    daviks_exit.cure_to_full = true;
    daviks_exit.reloads_until_won = 240;
    daviks_exit.dialogue_answers = book.dialogue_answers;
    beats.push_back(daviks_exit);
    RouteBeat revisit_hawkbluff = hawkbluff;
    revisit_hawkbluff.area_steps = {C::look};
    beats.push_back(revisit_hawkbluff);
    RouteBeat abandoned_hall{47, 3, 15, D::north, {"YES"}};
    abandoned_hall.step = true;
    abandoned_hall.searching = false;
    beats.push_back(abandoned_hall);
    RouteBeat hawk_recovery = palace_preparation;
    hawk_recovery.map = 47;
    hawk_recovery.x = 3;
    hawk_recovery.y = 15;
    beats.push_back(hawk_recovery);
    RouteBeat leave_hawkbluff{47, 6, 16, D::south, {"YES"}};
    leave_hawkbluff.step = true;
    leave_hawkbluff.searching = false;
    beats.push_back(leave_hawkbluff);
    RouteBeat baior{2, 3, 9, D::south, {"YES"}};
    baior.area_steps = {C::east,      C::northeast, C::northeast, C::northeast, C::northeast,
                        C::southeast, C::southeast, C::southeast, C::southeast, C::southeast,
                        C::east,      C::southeast, C::southeast, C::southeast};
    baior.searching = false;
    baior.cure_first = true;
    baior.cure_to_full = true;
    baior.reloads_until_won = 240;
    beats.push_back(baior);
    RouteBeat anthela{54,
                      7,
                      10,
                      D::east,
                      {"FORCE THE DOOR", "knight 1", "'WE COME FROM ANSALON TO FIGHT EVIL.'", "YES",
                       "'FEAR NOT, WE WILL SLAY THE DRAGONS!'"}};
    anthela.arrive_only = true;
    anthela.searching = false;
    anthela.cure_first = true;
    anthela.cure_to_full = true;
    anthela.reloads_until_won = 240;
    anthela.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x101, 1};
    anthela.cast_after = {"prayer"};
    beats.push_back(anthela);
    RouteBeat sharkmen{54, 10, 11, D::north, {}};
    sharkmen.arrive_only = true;
    sharkmen.searching = false;
    sharkmen.cure_first = true;
    sharkmen.cure_to_full = true;
    sharkmen.reload_event = true;
    sharkmen.reloads_until_won = 240;
    sharkmen.ward_each_with = "enlarge";
    sharkmen.ward_members = {"knight 1", "knight 2", "knight 3", "Grunschka"};
    sharkmen.repeat_until_bits = std::pair<std::uint16_t, std::uint16_t>{0x187, 1};
    sharkmen.repeat_in_place = true;
    sharkmen.take_items = {{"Ring", "magic_user 6"}, {"Bracers", "magic_user 6"}};
    // Bai'or's dragon attack is fought on Quick.
    sharkmen.fight_tactic = 0;
    beats.push_back(sharkmen);
    RouteBeat baior_inn{54, 17, 14, D::east, {}};
    baior_inn.arrive_only = true;
    baior_inn.searching = false;
    baior_inn.cure_first = true;
    baior_inn.cure_to_full = true;
    beats.push_back(baior_inn);
    RouteBeat dragon_preparation = palace_preparation;
    dragon_preparation.map = 54;
    dragon_preparation.x = 17;
    dragon_preparation.y = 14;
    beats.push_back(dragon_preparation);
    RouteBeat dragon_wards{54, 17, 14, D::east, {}};
    dragon_wards.waiting = true;
    dragon_wards.ward_each_with = "protection_from_evil_10_radius";
    beats.push_back(dragon_wards);
    RouteBeat dragon_mind = dragon_wards;
    dragon_mind.ward_each_with = "mind_blank";
    dragon_mind.ward_members = {"magic_user 6"};
    beats.push_back(dragon_mind);
    RouteBeat dragon_enlarge = dragon_wards;
    dragon_enlarge.ward_each_with = "enlarge";
    dragon_enlarge.ward_members = {"knight 1", "knight 2", "knight 3", "Grunschka"};
    dragon_enlarge.cast_after = {"prayer"};
    beats.push_back(dragon_enlarge);
    RouteBeat first_dragon{54, 9, 2, D::north, {"'HAVE AT THEE.'"}};
    first_dragon.arrive_only = true;
    first_dragon.searching = false;
    first_dragon.cure_first = true;
    first_dragon.cure_to_full = true;
    first_dragon.reload_event = true;
    first_dragon.reloads_until_won = 240;
    first_dragon.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x188, 1};
    beats.push_back(first_dragon);
    baior_inn.answers = {"LET IT GO"};
    beats.push_back(baior_inn);
    beats.push_back(dragon_preparation);
    beats.push_back(dragon_wards);
    beats.push_back(dragon_mind);
    beats.push_back(dragon_enlarge);
    RouteBeat last_dragons = first_dragon;
    last_dragons.x = 6;
    last_dragons.y = 2;
    last_dragons.answers = {"TALK", "BE FRIENDLY", "GIVE IT FOOD AND WATER.",
                            "'WE WILL DEFEAT ERESTEM!'"};
    last_dragons.reloads_until_won = 480;
    last_dragons.dialogue_answers = {{"", "LET IT GO"}};
    last_dragons.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0xEF, 1};
    beats.push_back(last_dragons);
    beats.push_back(baior_inn);
    beats.push_back(dragon_preparation);
    RouteBeat speed_supplies{54, 14, 12, D::south, {"YES", "sell:FLAIL@knight 1"}};
    for (unsigned vial = 0; vial < 2; ++vial)
    {
        speed_supplies.answers.push_back(
            "buy:POTION OF SPEED@knight 1,knight 2,knight 3,Grunschka");
        speed_supplies.answers.push_back("buy:POTION OF INVISIBILITY@magic_user 6");
    }
    speed_supplies.arrive_only = true;
    speed_supplies.searching = false;
    speed_supplies.cure_first = true;
    speed_supplies.cure_to_full = true;
    speed_supplies.pack_purchases = true;
    beats.push_back(speed_supplies);
    RouteBeat leave_baior{54, 20, 12, D::east, {"YES"}};
    leave_baior.arrive_only = true;
    leave_baior.searching = false;
    beats.push_back(leave_baior);
    RouteBeat blackwater{2, 3, 9, D::north, {"YES"}};
    blackwater.area_steps = {C::northwest, C::northwest, C::northwest, C::northwest};
    blackwater.searching = false;
    blackwater.cure_first = true;
    blackwater.cure_to_full = true;
    blackwater.reloads_until_won = 240;
    beats.push_back(blackwater);
    RouteBeat tremor_gate{50, 17, 6, D::north, {"type:HABO!LJX"}};
    tremor_gate.step = true;
    tremor_gate.searching = false;
    tremor_gate.cure_first = true;
    tremor_gate.cure_to_full = true;
    tremor_gate.reload_event = true;
    tremor_gate.reloads_until_won = 240;
    tremor_gate.dialogue_answers = {{"ATTACK HIM", "NO"},
                                    {"DO YOU ATTACK THEM", "NO"},
                                    {"WHO WILL HELP", "knight 2"},
                                    {"WHO WILL TRY TO FIND IT", "magic_user 6"},
                                    {"WHO WILL CARRY THE NECKLACE", "knight 1"},
                                    {"FOLLOW THEM", "NO"},
                                    {"INTERFERE", "NO"},
                                    {"MAY I JOIN", "YES"},
                                    {"ALLOW BALDRIC", "YES"},
                                    {"GIVE UP AN ITEM", "YES"},
                                    {"WHICH ITEM", "NECKLACE"},
                                    {"GIVE TREMOR THE DRAGON SCALE", "DRAGON SCALE"},
                                    {"ALLOW TREMOR", "YES"},
                                    {"DO YOU GIVE TREMOR", "YES"},
                                    {"APPROACH TREMOR", "YES"},
                                    {"WILL ANYONE APPROACH", "YES"},
                                    {"PLACE IT GENTLY", "GENTLY"}};
    tremor_gate.repeat_until_bits = std::pair<std::uint16_t, std::uint16_t>{0x17F, 8};
    tremor_gate.repeat_in_place = true;
    beats.push_back(tremor_gate);
    RouteBeat tremor{50, 22, 2, D::north, {}};
    tremor.arrive_only = true;
    tremor.searching = false;
    tremor.cure_first = true;
    tremor.cure_to_full = true;
    tremor.reloads_until_won = 240;
    tremor.dialogue_answers = tremor_gate.dialogue_answers;
    tremor.take_items = {{"Sword", "knight 2"}};
    tremor.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x73, 2};
    // The tremor's fights are fought on Quick.
    tremor.fight_tactic = 0;
    beats.push_back(tremor);
    RouteBeat clematra{50, 12, 16, D::south, {}};
    clematra.arrive_only = true;
    clematra.searching = false;
    clematra.cure_first = true;
    clematra.cure_to_full = true;
    clematra.reloads_until_won = 240;
    clematra.take_items = {{"Shield", "knight 2"}};
    clematra.take_wand = true;
    clematra.dialogue_answers = {{"DO YOU TELL HIM", "YES"}, {"AGREE TO TAKE THE FLIGHT", "NO"}};
    clematra.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x102, 1};
    beats.push_back(clematra);
    RouteBeat clematras_refuge = palace_preparation;
    clematras_refuge.map = 50;
    clematras_refuge.x = 12;
    clematras_refuge.y = 16;
    beats.push_back(clematras_refuge);
    clematra.arrive_only = false;
    // Her welcome is an entry event; looking while already inside does not
    // repeat the invitation to fly. Leave through the southern door and return.
    clematra.area_steps = {C::turn_around, C::forward, C::turn_around, C::forward};
    clematra.dialogue_answers = {{"DO YOU TELL HIM", "YES"}, {"AGREE TO TAKE THE FLIGHT", "YES"}};
    beats.push_back(clematra);
    RouteBeat citadel{2, 3, 9, D::north, {"YES"}};
    citadel.area_steps = {C::east};
    citadel.searching = false;
    citadel.dialogue_answers = {{"WHAT DO YOU DO", "  IDENTIFY YOURSELVES"},
                                {"", "  'WE ARE THE DIGNITARIES'"},
                                {"MIGHT JOIN YOU", "YES"}};
    beats.push_back(citadel);
    RouteBeat audience_preparation = dragon_enlarge;
    audience_preparation.map = 58;
    audience_preparation.x = 18;
    audience_preparation.y = 7;
    beats.push_back(audience_preparation);
    RouteBeat false_king{58, 18, 8, D::south, {}};
    false_king.arrive_only = true;
    false_king.searching = false;
    false_king.cure_first = true;
    false_king.cure_to_full = true;
    false_king.reloads_until_won = 240;
    false_king.dialogue_answers = {{"", "  TELL HIM ABOUT THE DRACONIANS."},
                                   {"", "  KISS THE KING'S RING."}};
    beats.push_back(false_king);
    RouteBeat palace_lift{58, 18, 3, D::north, {"GO DOWN"}};
    palace_lift.step = true;
    palace_lift.searching = false;
    palace_lift.cure_first = true;
    palace_lift.cure_to_full = true;
    beats.push_back(palace_lift);
    RouteBeat quiet_storeroom{58, 5, 21, D::west, {}};
    quiet_storeroom.searching = false;
    quiet_storeroom.cure_first = true;
    quiet_storeroom.cure_to_full = true;
    quiet_storeroom.reloads_until_won = 240;
    beats.push_back(quiet_storeroom);
    RouteBeat storeroom_rest = palace_preparation;
    storeroom_rest.map = 58;
    storeroom_rest.x = 5;
    storeroom_rest.y = 21;
    storeroom_rest.facing = D::west;
    beats.push_back(storeroom_rest);
    RouteBeat workshop_lift{58, 7, 17, D::north, {"WORKSHOP"}};
    workshop_lift.step = true;
    workshop_lift.searching = false;
    workshop_lift.cure_first = true;
    workshop_lift.cure_to_full = true;
    beats.push_back(workshop_lift);
    RouteBeat enhancement = guild;
    enhancement.map = 58;
    enhancement.x = 14;
    enhancement.y = 22;
    enhancement.facing = D::south;
    enhancement.dialogue_answers = {{"Choose a spell for magic_user 6", "Fire Shield"}};
    beats.push_back(enhancement);
    RouteBeat quiet_workshop{58, 25, 18, D::east, {}};
    quiet_workshop.searching = false;
    quiet_workshop.cure_first = true;
    quiet_workshop.cure_to_full = true;
    quiet_workshop.reloads_until_won = 240;
    beats.push_back(quiet_workshop);
    RouteBeat workshop_rest = storeroom_rest;
    workshop_rest.x = 25;
    workshop_rest.y = 18;
    workshop_rest.facing = D::east;
    workshop_rest.memorize_first_of.insert(workshop_rest.memorize_first_of.begin(),
                                           {"death_spell", "death_spell", "death_spell"});
    beats.push_back(workshop_rest);
    RouteBeat descend_workshop{58, 20, 17, D::north, {"GO DOWN"}};
    descend_workshop.step = true;
    descend_workshop.searching = false;
    descend_workshop.cure_first = true;
    descend_workshop.cure_to_full = true;
    beats.push_back(descend_workshop);
    RouteBeat guest_lift = workshop_lift;
    guest_lift.answers = {"GUEST ROOMS"};
    beats.push_back(guest_lift);
    RouteBeat guest_inn{58, 6, 6, D::north, {}};
    guest_inn.searching = false;
    guest_inn.cure_first = true;
    guest_inn.cure_to_full = true;
    guest_inn.reloads_until_won = 240;
    beats.push_back(guest_inn);
    guest_inn.facing = D::west;
    guest_inn.step = true;
    beats.push_back(guest_inn);
    RouteBeat quiet_bedroom{58, 5, 9, D::south, {}};
    quiet_bedroom.arrive_only = true;
    quiet_bedroom.searching = false;
    quiet_bedroom.cure_first = true;
    quiet_bedroom.cure_to_full = true;
    beats.push_back(quiet_bedroom);
    RouteBeat guest_rest = workshop_rest;
    guest_rest.x = 5;
    guest_rest.y = 9;
    guest_rest.facing = D::south;
    guest_rest.ward_memorized = "resist_fire";
    guest_rest.memorize_first_of.insert(
        guest_rest.memorize_first_of.begin(),
        {"meteor_swarm", "resist_cold", "resist_cold", "resist_cold", "resist_cold",
         "protection_from_evil_10_radius", "protection_from_evil_10_radius",
         "protection_from_evil_10_radius", "protection_from_evil_10_radius"});
    beats.push_back(guest_rest);
    audience_preparation.x = 5;
    audience_preparation.y = 9;
    beats.push_back(audience_preparation);
    RouteBeat needle_door{58, 3, 10, D::west, {"BASH", "knight 1"}};
    needle_door.step = true;
    needle_door.searching = false;
    needle_door.cure_first = true;
    needle_door.cure_to_full = true;
    needle_door.reloads_until_won = 240;
    beats.push_back(needle_door);
    RouteBeat magic_door = needle_door;
    magic_door.x = 2;
    magic_door.answers = {"CAST", "cleric 4", "BASH", "knight 1"};
    beats.push_back(magic_door);
    RouteBeat disguised_court{58, 1, 9, D::north, {}};
    disguised_court.arrive_only = true;
    disguised_court.searching = false;
    disguised_court.cure_first = true;
    disguised_court.cure_to_full = true;
    disguised_court.reloads_until_won = 240;
    beats.push_back(disguised_court);
    beats.push_back(quiet_bedroom);
    // The court's earned XP qualifies five members for another level.
    // Sell the remaining obsolete sword at the guest's real shop to pay the fees,
    // then use the lifts to revisit the workshop training hall.
    RouteBeat training_funds{58,
                             10,
                             9,
                             D::east,
                             {"TALK", "sell:TWO-HANDED SWORD@knight 3",
                              "buy:SLING@knight 1,knight 2,knight 3,Grunschka"}};
    training_funds.step = true;
    training_funds.searching = false;
    training_funds.cure_first = true;
    training_funds.cure_to_full = true;
    training_funds.pack_purchases = true;
    training_funds.reload_event = true;
    training_funds.reloads_until_won = 240;
    beats.push_back(training_funds);
    RouteBeat guest_descent{58, 7, 3, D::north, {"GO DOWN"}};
    guest_descent.step = true;
    guest_descent.searching = false;
    guest_descent.cure_first = true;
    guest_descent.cure_to_full = true;
    beats.push_back(guest_descent);
    beats.push_back(workshop_lift);
    RouteBeat advanced_training = enhancement;
    advanced_training.dialogue_answers = {{"Choose a spell for magic_user 6", "Meteor Swarm"}};
    beats.push_back(advanced_training);
    beats.push_back(descend_workshop);
    beats.push_back(guest_lift);
    beats.push_back(quiet_bedroom);
    beats.push_back(guest_rest);
    // Reach the cleared corridor outside the mages' room before casting
    // short wards such as Prayer.
    RouteBeat royal_approach{58, 1, 7, D::north, {}};
    royal_approach.arrive_only = true;
    royal_approach.searching = false;
    royal_approach.cure_first = true;
    royal_approach.cure_to_full = true;
    beats.push_back(royal_approach);
    RouteBeat royal_fire = royal_approach;
    royal_fire.ready_names.clear();
    royal_fire.waiting = true;
    royal_fire.ward_each_with = "resist_fire";
    beats.push_back(royal_fire);
    RouteBeat royal_cold = royal_fire;
    royal_cold.ward_each_with = "resist_cold";
    beats.push_back(royal_cold);
    beats.push_back(audience_preparation);
    RouteBeat rescue_mind = audience_preparation;
    rescue_mind.ward_each_with = "mind_blank";
    rescue_mind.ward_members = {"magic_user 6"};
    rescue_mind.cast_after = {"globe_of_invulnerability"};
    rescue_mind.cast_after_by = "magic_user 6";
    beats.push_back(rescue_mind);
    RouteBeat rescue_wards = audience_preparation;
    rescue_wards.ward_each_with = "protection_from_evil_10_radius";
    rescue_wards.ward_members.clear();
    rescue_wards.cast_after.clear();
    beats.push_back(rescue_wards);
    RouteBeat royal_mages = disguised_court;
    royal_mages.y = 6;
    royal_mages.reload_event = true;
    royal_mages.reloads_until_won = 480;
    royal_mages.drink_names = {{"Potion of Speed", "knight 1"},
                               {"Potion of Speed", "knight 2"},
                               {"Potion of Speed", "knight 3"},
                               {"Potion of Speed", "Grunschka"},
                               {"Potion of Invisibility", "magic_user 6"}};
    royal_mages.dialogue_answers = {{"", "  TELL THE KING TO HALT."}, {"", "DIVE OUT OF THE WAY"}};
    beats.push_back(royal_mages);
    beats.push_back(quiet_bedroom);
    beats.push_back(guest_rest);
    RouteBeat portal_approach = royal_approach;
    portal_approach.x = 2;
    portal_approach.y = 4;
    portal_approach.ready_names.clear();
    beats.push_back(portal_approach);
    royal_fire.x = royal_cold.x = 2;
    royal_fire.y = royal_cold.y = 4;
    beats.push_back(royal_fire);
    beats.push_back(royal_cold);
    beats.push_back(audience_preparation);
    beats.push_back(rescue_mind);
    beats.push_back(rescue_wards);
    RouteBeat king_portal{58, 2, 3, D::north, {}};
    king_portal.searching = false;
    king_portal.cure_first = true;
    king_portal.cure_to_full = true;
    king_portal.reloads_until_won = 240;
    king_portal.dialogue_answers = {
        {"", "ENTER THE TELEPORTER"}, {"", "LOOK BEHIND YOU"}, {"", "  ASK THE KINGS QUESTIONS."}};
    beats.push_back(king_portal);
    RouteBeat return_portal{58, 1, 13, D::north, {"ENTER THE TELEPORTER"}};
    return_portal.searching = false;
    return_portal.cure_first = true;
    return_portal.cure_to_full = true;
    beats.push_back(return_portal);
    RouteBeat war_council{58, 2, 3, D::north, {}};
    war_council.arrive_only = true;
    war_council.searching = false;
    war_council.take_wand = true;
    war_council.take_items = {{"Ring", "knight 3"}};
    beats.push_back(war_council);
    beats.push_back(palace_lift);
    beats.push_back(quiet_storeroom);
    RouteBeat council_rest = workshop_rest;
    council_rest.x = 5;
    council_rest.y = 21;
    council_rest.facing = D::west;
    beats.push_back(council_rest);
    beats.push_back(workshop_lift);
    enhancement.dialogue_answers = {{"Choose a spell for magic_user 6", "Meteor Swarm"}};
    enhancement.answers.insert(enhancement.answers.end() - 1,
                               {"knight 2: Petition for the Order of the Sword",
                                "knight 3: Petition for the Order of the Sword"});
    beats.push_back(enhancement);
    beats.push_back(quiet_workshop);
    workshop_rest.memorize_first_of.insert(workshop_rest.memorize_first_of.begin(), "meteor_swarm");
    beats.push_back(workshop_rest);
    beats.push_back(descend_workshop);
    beats.push_back(quiet_storeroom);
    RouteBeat tower_preparation = workshop_rest;
    tower_preparation.x = 5;
    tower_preparation.y = 21;
    tower_preparation.facing = D::west;
    tower_preparation.ward_memorized = "resist_fire";
    beats.push_back(tower_preparation);
    RouteBeat tower_fire = rescue_wards;
    tower_fire.ward_each_with = "resist_fire";
    beats.push_back(tower_fire);
    beats.push_back(rescue_mind);
    RouteBeat windship{58, 11, 16, D::north, {"YES"}};
    windship.arrive_only = true;
    windship.searching = false;
    windship.cure_first = true;
    windship.cure_to_full = true;
    windship.reloads_until_won = 240;
    beats.push_back(windship);
    RouteBeat falling_bridge{62, 0, 24, D::east, {"YES", "AHEAD"}};
    falling_bridge.arrive_only = true;
    falling_bridge.searching = false;
    falling_bridge.cure_first = true;
    falling_bridge.cure_to_full = true;
    falling_bridge.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x19B, 0};
    beats.push_back(falling_bridge);
    RouteBeat tower_recovery = tower_preparation;
    tower_recovery.map = 62;
    tower_recovery.x = 0;
    tower_recovery.y = 23;
    tower_recovery.facing = D::north;
    beats.push_back(tower_recovery);
    tower_fire.map = rescue_mind.map = 62;
    beats.push_back(tower_fire);
    beats.push_back(rescue_mind);
    RouteBeat engine{62, 1, 19, D::east, {"MOVE"}};
    engine.arrive_only = true;
    engine.searching = false;
    engine.cure_first = true;
    engine.cure_to_full = true;
    engine.reloads_until_won = 240;
    beats.push_back(engine);
    RouteBeat engine_lever = engine;
    engine_lever.x = 2;
    beats.push_back(engine_lever);
    engine_lever.arrive_only = false;
    engine_lever.answers = {"YES"};
    beats.push_back(engine_lever);
    engine.answers = {"YES"};
    engine.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0xA8, 1};
    beats.push_back(engine);
    RouteBeat first_stair{62, 5, 19, D::north, {"YES"}};
    first_stair.step = true;
    first_stair.searching = false;
    first_stair.cure_first = true;
    first_stair.cure_to_full = true;
    first_stair.reload_event = true;
    first_stair.reloads_until_won = 240;
    first_stair.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x19B, 1};
    beats.push_back(first_stair);
    RouteBeat beholder_recovery = tower_recovery;
    beholder_recovery.x = 19;
    beholder_recovery.y = 15;
    beholder_recovery.memorize_first_of.insert(
        beholder_recovery.memorize_first_of.begin(),
        {"protection_from_evil_10_radius", "protection_from_evil_10_radius",
         "protection_from_evil_10_radius", "protection_from_evil_10_radius"});
    beats.push_back(beholder_recovery);
    beats.push_back(tower_fire);
    beats.push_back(rescue_mind);
    RouteBeat beholder_approach{62, 24, 14, D::east, {}};
    beholder_approach.arrive_only = true;
    beholder_approach.searching = false;
    beholder_approach.cure_first = true;
    beholder_approach.cure_to_full = true;
    beholder_approach.reloads_until_won = 240;
    beats.push_back(beholder_approach);
    audience_preparation.map = 62;
    beats.push_back(audience_preparation);
    rescue_wards.map = 62;
    beats.push_back(rescue_wards);
    RouteBeat beholders{62, 25, 14, D::east, {}};
    beholders.arrive_only = true;
    beholders.searching = false;
    beholders.cure_first = true;
    beholders.cure_to_full = true;
    beholders.reload_event = true;
    beholders.reloads_until_won = 480;
    beholders.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0xA8, 3};
    beats.push_back(beholders);
    RouteBeat siege_port{62, 27, 27, D::south, {"YES"}};
    siege_port.arrive_only = true;
    siege_port.searching = false;
    siege_port.cure_first = true;
    siege_port.cure_to_full = true;
    siege_port.reload_event = true;
    siege_port.reloads_until_won = 480;
    siege_port.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0xA8, 7};
    beats.push_back(siege_port);
    RouteBeat upper_refuge{62, 27, 17, D::north, {}};
    upper_refuge.arrive_only = true;
    upper_refuge.searching = false;
    upper_refuge.cure_first = true;
    upper_refuge.cure_to_full = true;
    upper_refuge.reloads_until_won = 240;
    beats.push_back(upper_refuge);
    RouteBeat tower_upper_rest = beholder_recovery;
    tower_upper_rest.x = 27;
    tower_upper_rest.y = 17;
    beats.push_back(tower_upper_rest);
    beats.push_back(tower_fire);
    beats.push_back(rescue_mind);
    beats.push_back(rescue_wards);
    // The central core is entered through the gnomes' brief opening in
    // its flame wall. Ordinary walking cannot cross that closed boundary.
    RouteBeat bucket_approach{62, 18, 25, D::south, {}};
    bucket_approach.arrive_only = true;
    bucket_approach.searching = false;
    bucket_approach.cure_first = true;
    bucket_approach.cure_to_full = true;
    bucket_approach.reloads_until_won = 240;
    beats.push_back(bucket_approach);
    beats.push_back(audience_preparation);
    RouteBeat core_defenders{62, 18, 26, D::south, {}};
    core_defenders.arrive_only = true;
    core_defenders.searching = false;
    core_defenders.cure_first = true;
    core_defenders.cure_to_full = true;
    core_defenders.reload_event = true;
    core_defenders.reloads_until_won = 480;
    beats.push_back(core_defenders);
    RouteBeat bucket_signal{62, 25, 21, D::north, {}};
    bucket_signal.arrive_only = true;
    bucket_signal.searching = false;
    bucket_signal.cure_first = true;
    bucket_signal.cure_to_full = true;
    bucket_signal.reloads_until_won = 240;
    beats.push_back(bucket_signal);
    RouteBeat bucket_gap{62, 19, 25, D::north, {"YES"}};
    bucket_gap.arrive_only = true;
    bucket_gap.searching = false;
    bucket_gap.cure_first = true;
    bucket_gap.cure_to_full = true;
    beats.push_back(bucket_gap);
    RouteBeat gnome_ladder{62, 22, 23, D::north, {"YES"}};
    gnome_ladder.arrive_only = true;
    gnome_ladder.searching = false;
    gnome_ladder.cure_first = true;
    gnome_ladder.cure_to_full = true;
    gnome_ladder.reload_event = true;
    gnome_ladder.reloads_until_won = 240;
    gnome_ladder.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x19B, 2};
    beats.push_back(gnome_ladder);
    RouteBeat coven_recovery = tower_upper_rest;
    coven_recovery.x = 10;
    coven_recovery.y = 12;
    beats.push_back(coven_recovery);
    beats.push_back(tower_fire);
    beats.push_back(rescue_mind);
    RouteBeat coven_approach{62, 4, 9, D::north, {}};
    coven_approach.arrive_only = true;
    coven_approach.searching = false;
    coven_approach.cure_first = true;
    coven_approach.cure_to_full = true;
    coven_approach.reloads_until_won = 240;
    beats.push_back(coven_approach);
    beats.push_back(audience_preparation);
    beats.push_back(rescue_wards);
    RouteBeat coven{62, 4, 8, D::north, {"YES"}};
    coven.arrive_only = true;
    coven.searching = false;
    coven.cure_first = true;
    coven.cure_to_full = true;
    coven.reload_event = true;
    coven.reloads_until_won = 480;
    coven.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0xA8, 15};
    beats.push_back(coven);
    RouteBeat coven_rest = tower_upper_rest;
    coven_rest.x = 4;
    coven_rest.y = 8;
    beats.push_back(coven_rest);
    beats.push_back(tower_fire);
    beats.push_back(rescue_mind);
    RouteBeat dragon_approach{62, 11, 3, D::north, {"ATTACK"}};
    dragon_approach.arrive_only = true;
    dragon_approach.searching = false;
    dragon_approach.cure_first = true;
    dragon_approach.cure_to_full = true;
    dragon_approach.reloads_until_won = 240;
    beats.push_back(dragon_approach);
    beats.push_back(audience_preparation);
    beats.push_back(rescue_wards);
    RouteBeat dragon_loft = dragon_approach;
    dragon_loft.arrive_only = false;
    dragon_loft.step = true;
    dragon_loft.reload_event = true;
    dragon_loft.reloads_until_won = 480;
    beats.push_back(dragon_loft);
    RouteBeat loft_rest = tower_upper_rest;
    loft_rest.x = 11;
    loft_rest.y = 2;
    loft_rest.memorize_first_of.insert(
        loft_rest.memorize_first_of.begin(),
        {"read_magic", "dispel_magic", "cure_critical_wounds", "cure_serious_wounds",
         "silence_15_radius", "silence_15_radius", "silence_15_radius", "silence_15_radius",
         "resist_cold", "resist_cold", "resist_cold"});
    beats.push_back(loft_rest);
    beats.push_back(tower_fire);
    beats.push_back(rescue_mind);
    RouteBeat lava_lift{62, 9, 8, D::north, {"YES"}};
    lava_lift.arrive_only = true;
    lava_lift.searching = false;
    lava_lift.cure_first = true;
    lava_lift.cure_to_full = true;
    lava_lift.reload_event = true;
    lava_lift.reloads_until_won = 240;
    lava_lift.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x19B, 3};
    beats.push_back(lava_lift);
    RouteBeat temple_approach{62, 21, 7, D::south, {"YES", "ATTACK"}};
    temple_approach.arrive_only = true;
    temple_approach.searching = false;
    temple_approach.cure_first = true;
    temple_approach.cure_to_full = true;
    temple_approach.reload_event = true;
    temple_approach.reloads_until_won = 480;
    beats.push_back(temple_approach);
    beats.push_back(audience_preparation);
    RouteBeat font_protection = rescue_wards;
    font_protection.cast_after = {"globe_of_invulnerability"};
    font_protection.cast_after_by = "magic_user 6";
    beats.push_back(font_protection);
    RouteBeat font_cold = tower_fire;
    font_cold.ward_each_with = "resist_cold";
    font_cold.cast_after = {"fire_shield:cold"};
    font_cold.cast_after_by = "magic_user 6";
    beats.push_back(font_cold);
    // The script supplies its own east/west ambush (mask 10), regardless
    // of the doorway used to enter the Font.
    RouteBeat temple_font{62, 21, 9, D::south, {"YES", "ATTACK"}};
    temple_font.step = true;
    temple_font.searching = false;
    temple_font.cure_first = true;
    temple_font.cure_to_full = true;
    temple_font.reload_event = true;
    temple_font.reloads_until_won = 240;
    temple_font.drink_names = royal_mages.drink_names;
    temple_font.ready_types = {
        {69, "knight 1"}, {28, "knight 2"}, {69, "knight 3"}, {1, "Grunschka"}, {29, "cleric 4"}};
    temple_font.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0xA8, 31};
    beats.push_back(temple_font);
    // The remaining way follows the authored rescue, Abyss and return scenes.
    // These appended beats preserve the Font's existing checkpoint number.
    const auto ending_walk =
        [](unsigned map, unsigned x, unsigned y, D facing, std::vector<std::string> answers = {})
    {
        RouteBeat beat{map, x, y, facing, std::move(answers)};
        beat.arrive_only = true;
        beat.searching = false;
        beat.cure_first = true;
        beat.cure_to_full = true;
        beat.reload_event = true;
        beat.reloads_until_won = 480;
        return beat;
    };
    // Fire creatures break every camp on the Font's lava floor, so the party
    // does not rest there. The gnome windship rises from the shaft north of
    // 17,10: stepping into it leaps aboard, and the scene's own spell choice
    // frees Crysia before the party lands on an upper floor.
    auto upper_windship = ending_walk(
        62, 17, 10, D::north, {"LEAP ABOARD", "USE SPELL", "DISPEL", "CRYSIA", "YES", "ATTACK"});
    upper_windship.arrive_only = false;
    upper_windship.step = true;
    upper_windship.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x19B, 4};
    beats.push_back(upper_windship);
    // The captain leaves with his rescued sister through the ordinary script.
    // The windship sets the party down on a ledge whose way east crosses
    // two shaft edges: the party jumps them, as the shaft's prompt offers.
    RouteBeat ledge{62, 0, 24, D::east, {"YES"}};
    ledge.step = true;
    ledge.searching = false;
    beats.push_back(ledge);
    ledge.x = 1;
    beats.push_back(ledge);
    // Rest again before the Sanctum's two consecutive waves and the Abyss.
    RouteBeat upper_court_recovery = tower_upper_rest;
    upper_court_recovery.x = 4;
    upper_court_recovery.y = 22;
    upper_court_recovery.memorize_first_of.insert(upper_court_recovery.memorize_first_of.begin(),
                                                  {"dispel_magic", "dispel_magic", "dispel_magic"});
    beats.push_back(upper_court_recovery);
    beats.push_back(tower_fire);
    beats.push_back(rescue_mind);
    beats.push_back(rescue_wards);
    // Cast before entering the Abyss, whose field inhibits magic. Raistlin's
    // torturers must be faced with the party's ordinary readied weapons.
    beats.push_back(audience_preparation);
    // Arriving at the Sanctum plays its two waves and the fall into the Abyss.
    beats.push_back(ending_walk(62, 5, 25, D::south, {"YES", "ATTACK"}));
    // Raistlin's cell is entered through its west wall from 13,2. The
    // Sanctum's waves leave the party hurt: it rests, cures and casts its
    // lasting wards before stepping into the room that inhibits magic. After freeing him, no beat
    // camps or cures: head straight for the portal while his warning still
    // leaves time.
    // No spell can be cast in Raistlin's cell, so what the party prays for
    // here also carries it through the portal to the five-headed dragon,
    // which hatches before the party can rest again.
    auto abyss_rest = ending_walk(66, 13, 2, D::east);
    abyss_rest.rest_first = true;
    abyss_rest.rest_for_spells = true;
    abyss_rest.keep_preparation = true;
    abyss_rest.memorize_first_of = palace_preparation.memorize_first_of;
    abyss_rest.memorize_first_of.insert(
        abyss_rest.memorize_first_of.begin(),
        {"protection_from_evil_10_radius", "protection_from_evil_10_radius",
         "protection_from_evil_10_radius", "protection_from_evil_10_radius"});
    beats.push_back(abyss_rest);
    // Spells already cast still hold in the room that bars new ones.
    RouteBeat abyss_wards{66, 13, 2, D::east, {}};
    abyss_wards.waiting = true;
    abyss_wards.ward_each_with = "protection_from_evil_10_radius";
    beats.push_back(abyss_wards);
    RouteBeat abyss_enlarge = abyss_wards;
    abyss_enlarge.ward_each_with = "enlarge";
    abyss_enlarge.ward_members = {"knight 1", "knight 2", "knight 3", "Grunschka"};
    abyss_enlarge.cast_after = {"prayer"};
    beats.push_back(abyss_enlarge);
    auto raistlin_cell = ending_walk(66, 13, 2, D::east, {"FIGHT MONSTERS", "CUT CHAINS", "YES"});
    raistlin_cell.arrive_only = false;
    raistlin_cell.step = true;
    raistlin_cell.cure_first = false;
    raistlin_cell.cure_to_full = false;
    beats.push_back(raistlin_cell);
    auto free_raistlin = ending_walk(66, 15, 2, D::north, {"FIGHT MONSTERS", "CUT CHAINS", "YES"});
    free_raistlin.cure_first = false;
    free_raistlin.cure_to_full = false;
    free_raistlin.ready_types = {{69, "knight 1"},    {19, "knight 2"}, {69, "knight 3"},
                                 {1, "Grunschka"},    {15, "cleric 4"}, {15, "cleric 5"},
                                 {15, "magic_user 6"}};
    free_raistlin.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x9F, 1};
    // Raistlin's torturers include enchanted Bozaks whose Fire Shields return
    // every blow twice over and enchanted Sivaks whose dying meteors strike
    // all within 2 squares; no spell or item works in the room.
    free_raistlin.fight_tactic = 5;
    beats.push_back(free_raistlin);
    // Leave his cell the way the party came, through its west wall.
    auto leave_cell = ending_walk(66, 14, 2, D::west);
    leave_cell.arrive_only = false;
    leave_cell.step = true;
    leave_cell.cure_first = false;
    leave_cell.cure_to_full = false;
    beats.push_back(leave_cell);
    // All six imported PCs have Dexterity 18. The knight therefore ties for
    // their best throw. The first portal menu is still foggy: wait there,
    // then jump only after the bell clears it. Jumping early kills a member.
    auto abyss_portal = ending_walk(66, 27, 4, D::east,
                                    {"CREEP", "LEFT", "CREEP", "WAIT", "THROW STONE", "knight 1",
                                     "RUN TO THE PORTAL", "NO", "WAIT", "JUMP THROUGH THE PORTAL"});
    abyss_portal.cure_first = false;
    abyss_portal.cure_to_full = false;
    abyss_portal.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x104, 1};
    // The portal sets the party down hurt beside the Greystone's tripod,
    // where a camp hatches the five-headed dragon. Leave the tripod alone,
    // walk back to the upper court, and rest and ward there first.
    abyss_portal.dialogue_answers = {{"INVESTIGATE THE TRIPOD", "NO"}};
    beats.push_back(abyss_portal);
    auto leave_tripod = ending_walk(62, 4, 22, D::north);
    leave_tripod.dialogue_answers = abyss_portal.dialogue_answers;
    // The egg hatches as the party moves off: the five-headed dragon is
    // fought here, its heads slain while its body can neither be harmed nor
    // act.
    beats.push_back(leave_tripod);
    auto egg_rest = palace_preparation;
    egg_rest.map = 62;
    egg_rest.x = 4;
    egg_rest.y = 22;
    egg_rest.facing = D::north;
    egg_rest.arrive_only = false;
    // This dungeon uses the normal camp, rather than a wilderness menu.
    egg_rest.waiting = false;
    egg_rest.dialogue_answers = abyss_portal.dialogue_answers;
    egg_rest.memorize_first_of.insert(
        egg_rest.memorize_first_of.begin(),
        {"protection_from_evil_10_radius", "protection_from_evil_10_radius",
         "protection_from_evil_10_radius", "protection_from_evil_10_radius"});
    beats.push_back(egg_rest);
    beats.push_back(rescue_wards);
    beats.push_back(rescue_mind);
    beats.push_back(audience_preparation);
    auto egg_approach = ending_walk(62, 5, 25, D::south);
    egg_approach.dialogue_answers = {{"INVESTIGATE THE TRIPOD", "YES"},
                                     {"CLIMB UP ONE OF ITS LEGS", "YES"},
                                     {"WHO WILL DO THE EASY CLIMB", "knight 1"},
                                     {"TOUCH THE GREAT EGG", "YES"}};
    beats.push_back(egg_approach);
    auto dragon_egg = ending_walk(62, 5, 25, D::south, {"YES", "YES", "knight 1", "YES"});
    dragon_egg.arrive_only = false;
    dragon_egg.step = true;
    dragon_egg.dialogue_answers = egg_approach.dialogue_answers;
    dragon_egg.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0xBB, 0};
    beats.push_back(dragon_egg);
    RouteBeat egg_recovery = upper_court_recovery;
    beats.push_back(egg_recovery);
    beats.push_back(tower_fire);
    beats.push_back(rescue_mind);
    beats.push_back(rescue_wards);
    auto descending_stair = ending_walk(62, 5, 19, D::north, {"YES"});
    descending_stair.step = true;
    descending_stair.arrive_only = false;
    descending_stair.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x19B, 3};
    beats.push_back(descending_stair);
    // Enter the fire platform from the west: its script needs the type-2
    // wall cached before the step onto (23,6), not a later look.
    auto lower_stair = ending_walk(62, 22, 6, D::east, {"YES"});
    lower_stair.step = true;
    lower_stair.arrive_only = false;
    lower_stair.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x19B, 2};
    beats.push_back(lower_stair);
    auto queen_mirror = ending_walk(62, 8, 6, D::south, {"DEFEND", "LEAVE"});
    queen_mirror.ward_each_with = "globe_of_invulnerability";
    queen_mirror.ward_members = {"magic_user 6"};
    beats.push_back(queen_mirror);
    RouteBeat flower_recovery = upper_court_recovery;
    // The path to (4,8) crosses the Iron Flower's guard event at (4,4).
    // Rest and prepare at the mirror's exit before entering that passage.
    flower_recovery.x = 9;
    flower_recovery.y = 6;
    beats.push_back(flower_recovery);
    beats.push_back(tower_fire);
    beats.push_back(rescue_mind);
    beats.push_back(rescue_wards);
    // Pursue the Greystone through the actual Iron Flower guards and both
    // final waves. The last throw cuts Hrumbishnog's grapple before boarding.
    auto iron_flower = ending_walk(62, 4, 4, D::north, {"WAIT", "WAIT", "knight 1", "YES"});
    iron_flower.fight_tactic = 6;
    iron_flower.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x104, 1};
    beats.push_back(iron_flower);
    auto celebration_lift = workshop_lift;
    celebration_lift.answers = {"PALACE"};
    beats.push_back(celebration_lift);
    auto celebration = ending_walk(58, 18, 11, D::south);
    celebration.arrive_only = false;
    celebration.step = true;
    celebration.dialogue_answers = {{"", "STAY"}};
    // The test driver records PROGRAM 1 before Continue, then follows the
    // dragon epilogue through its authored THE END and exploration unlock.
    celebration.expect_cell = std::pair<std::uint16_t, std::uint16_t>{0x106, 1};
    beats.push_back(celebration);
    return beats;
}
} // namespace opengold::test
