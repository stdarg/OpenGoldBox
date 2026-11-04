// The Creature Stats Editor's view of the game (tools/creature_data.h), checked
// against the player's install: what it shows is what the game computes.
#include "../src/OpenGold.Core/src/json_document.h"
#include "../tools/creature_data.h"
#include "png_size_only.h"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
using opengold::creatures::CreatureData;
using opengold::detail::Json;

void check(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

Json parsed(const std::string &text)
{
    return opengold::detail::parse_json(text);
}

const Json::Array &list(const Json &document, std::string_view key)
{
    return std::get<Json::Array>(document.find(key)->value);
}

double number(const Json &object, std::string_view key)
{
    return std::get<double>(object.find(key)->value);
}

std::string text(const Json &object, std::string_view key)
{
    return std::get<std::string>(object.find(key)->value);
}

// The first entry whose members have these values.
const Json &entry(const Json::Array &entries,
                  std::initializer_list<std::pair<std::string_view, Json>> members)
{
    for (const auto &candidate : entries)
    {
        bool matches = true;
        for (const auto &[key, value] : members)
        {
            const auto *found = candidate.find(key);
            matches = matches && found && opengold::detail::json_equal(*found, value);
        }
        if (matches)
        {
            return candidate;
        }
    }
    throw std::runtime_error("No such entry");
}

void castle_guards(const CreatureData &data)
{
    const auto creatures = parsed(data.creatures());
    const auto &guard =
        entry(list(creatures, "creatures"),
              {{"area", Json{std::string("Valjevo Castle")}}, {"record", Json{85.0}}});
    check(text(guard, "definition") == "castle-knight" && number(guard, "fit_xp") == 700 &&
              number(guard, "award_xp") == 700 && number(guard, "original_hit_points") == 87 &&
              text(guard, "key") == "MON5CHA.DAX:85",
          "The castle's guards fight as castle-knight, 700 and 700, from the original's 87");
    check(list(guard, "sharing").size() == 2,
          "The guards and Tempus's 8th level fighters in New Phlan fight as castle-knight");
    const auto &goblin = entry(list(creatures, "creatures"),
                               {{"area", Json{std::string("The Slums")}}, {"record", Json{2.0}}});
    check(text(goblin, "definition") == "slums-goblin" && list(goblin, "sharing").size() == 3,
          "The Slums' goblins share their stat line with Podol Plaza's and the Temple of Bane's");

    const auto details = parsed(data.creature(7, 85));
    const auto &stats = list(details, "stats");
    check(stats.size() == 5, "A creature shows each difficulty");
    const auto &normal = entry(stats, {{"difficulty", Json{std::string("normal")}}});
    const auto &easy = entry(stats, {{"difficulty", Json{std::string("easy")}}});
    const auto &infernal = entry(stats, {{"difficulty", Json{std::string("infernal")}}});
    check(number(normal, "hit_points") == 52 && number(normal, "armor_class") == 18,
          "At Normal the guards have the stat line's 52 Hit Points and AC 18");
    check(number(easy, "hit_points") == 52 * 60 / 100 &&
              number(infernal, "hit_points") == 52 * 320 / 100,
          "Each difficulty scales the Hit Points as a fight does");
    check(number(*infernal.find("melee"), "to_hit") == number(*normal.find("melee"), "to_hit") + 4,
          "Infernal adds its attack bonus");
    check(!list(details, "gear").empty(), "The guards' gear is listed");
    check(data.creature_art(3, 85).has_value(),
          "The guards have their combat art, though only one of the castle's scripts loads them");
}

void stat_lines(const CreatureData &data)
{
    const auto lines = parsed(data.stat_lines());
    const auto &knight =
        entry(list(lines, "stat_lines"), {{"definition", Json{std::string("castle-knight")}}});
    check(number(knight, "hit_points") == 52, "Every stat line is listed with its Hit Points");
}

void guards_fight(const CreatureData &data)
{
    const auto encounters = parsed(data.encounters());
    const auto &site =
        entry(list(encounters, "encounters"), {{"site", Json{std::string("ECL5.DAX:7:0xA60D")}}});
    check(number(site, "record") == 85 && number(site, "count") == 12 &&
              std::get<bool>(site.find("editable")->value),
          "The guards' LOAD MONSTER loads twelve of record 85 and can be overridden");
    const std::vector<unsigned> levels{10, 10, 10, 10, 10, 10, 10, 6};
    const auto fight = parsed(data.fight("ECL5.DAX:7:0xA60D", levels, 33));
    const auto &normal = entry(list(fight, "sized"), {{"difficulty", Json{std::string("normal")}}});
    check(list(fight, "groups").size() == 1 &&
              std::get<double>(list(normal, "met").at(0).value) == 8,
          "Eight of the twelve guards meet a party of seven 10th and one 6th level at Normal");
}

void chances(const CreatureData &data)
{
    const auto found = parsed(data.chances());
    const auto &all = list(found, "chances");
    const auto &roll = entry(all, {{"site", Json{std::string("ECL2.DAX:20:0x9B3A")}}});
    const auto &compare = entry(all, {{"site", Json{std::string("ECL2.DAX:20:0x9B60")}}});
    const auto &rest = entry(all, {{"site", Json{std::string("ECL2.DAX:20:0x9A3C")}}});
    check(text(roll, "kind") == "random" && number(roll, "original") == 13 &&
              text(compare, "kind") == "compare" && number(compare, "operand") == 1 &&
              number(compare, "original") == 12 && text(rest, "kind") == "rest_interval" &&
              number(rest, "original") == 24,
          "The Slums' step roll, its threshold and its rest interval are chances");
}

void companions(const CreatureData &data)
{
    const auto found = parsed(data.companions());
    const auto &genheeris =
        entry(list(found, "companions"), {{"name", Json{std::string("Genheeris")}}});
    check(number(genheeris, "level") == 6 && number(genheeris, "hit_points") > 0 &&
              genheeris.find("entry")->find("record") != nullptr,
          "Genheeris is built at level 6, with his entry");
}
} // namespace

int main()
{
    const char *game = std::getenv("OPENGOLD_GAME_DIR");
    if (!game)
    {
        std::cout << "Skipped: set OPENGOLD_GAME_DIR to the Pool of Radiance files\n";
        return 0;
    }
    try
    {
        const CreatureData data(OPENGOLD_SOURCE_DIR, game, opengold::test::png_size_only);
        castle_guards(data);
        stat_lines(data);
        guards_fight(data);
        chances(data);
        companions(data);
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Creature data tests passed\n";
}
